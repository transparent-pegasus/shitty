# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.
{
  description = "A small, fast Wayland/X11/Vulkan and macOS/Metal terminal emulator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    {
      self,
      nixpkgs,
    }:
    let
      inherit (nixpkgs) lib;

      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      forAllSystems = lib.genAttrs systems;

      nixpkgsFor = system: nixpkgs.legacyPackages.${system};

      # YYYY.MM.DD from the flake's lastModifiedDate, matching build.py's scheme.
      versionFromFlake =
        let
          d = self.lastModifiedDate or "19700101";
        in
        "${builtins.substring 0 4 d}.${builtins.substring 4 2 d}.${builtins.substring 6 2 d}";

      sanitizerConfigs = {
        asan = {
          flag = "-fsanitize=address";
          environment = "export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1";
        };
        ubsan = {
          flag = "-fsanitize=undefined";
          environment = "export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1";
        };
      };

      configureBuildEnvironment =
        {
          sanitizer ? null,
          coverage ? false,
          isLinux,
          warningsAsErrors ? false,
        }:
        assert !(coverage && sanitizer != null);
        let
          config = if sanitizer == null then null else sanitizerConfigs.${sanitizer};
          cxxFlags =
            if config == null then
              ""
            else
              lib.concatStringsSep " " [
                config.flag
                "-fno-sanitize-recover=all"
                "-fno-omit-frame-pointer"
                "-g"
              ];
          linkInstrumentation =
            if config != null then
              config.flag
            else if coverage then
              "-fprofile-instr-generate -Wl,--build-id=sha1"
            else
              "";
        in
        ''
          unset CPPFLAGS CFLAGS CXXFLAGS LDFLAGS ASAN_OPTIONS UBSAN_OPTIONS LLVM_PROFILE_FILE
          # build intentionally passes its canonical target triple. Nix's
          # wrapper spells the equivalent native vendor field differently.
          export NIX_CC_WRAPPER_SUPPRESS_TARGET_WARNING=1
          ${lib.optionalString warningsAsErrors ''
            export CFLAGS=-Werror
          ''}
          ${lib.optionalString (config != null) ''
            export CXXFLAGS=${lib.escapeShellArg cxxFlags}
            ${config.environment}
          ''}
          ${lib.optionalString coverage ''
            export CXXFLAGS="-fprofile-instr-generate -fcoverage-mapping -fcoverage-compilation-dir=. -fcoverage-prefix-map=$PWD=."
          ''}
          export LDFLAGS="${
            lib.optionalString (linkInstrumentation != "") "${linkInstrumentation} "
          }${lib.optionalString isLinux "$(pkg-config --libs wayland-client xkbcommon xcb) -lrt"}"
        '';

      mkShitty =
        pkgs:
        {
          sanitizer ? null,
          stdenv ? pkgs.llvmPackages.stdenv,
          warningsAsErrors ? false,
        }:
        let
          sanitizerSuffix = lib.optionalString (sanitizer != null) "-${sanitizer}";
          buildDirectory = ".build${sanitizerSuffix}";
        in
        stdenv.mkDerivation rec {
          pname = "st-pt${sanitizerSuffix}";
          version = versionFromFlake;

          src = self;

          # build.py stamps SHITTY_VERSION from date.today(), which is impure
          # under the Nix sandbox. Pin it to the flake revision date instead.
          postPatch = ''
            substituteInPlace build.py \
              --replace-fail 'date.today().strftime("%Y.%m.%d")' '"${version}"'
          '';

          nativeBuildInputs =
            with pkgs;
            [
              glslang
              librsvg
              makeWrapper
              pkg-config
              python3
              ragel
            ]
            ++ lib.optionals stdenv.hostPlatform.isLinux [
              addDriverRunpath
              wayland-protocols
              wayland-scanner
            ]
            ++ lib.optionals stdenv.hostPlatform.isDarwin [ spirv-cross ];

          buildInputs =
            with pkgs;
            [
              brotli
              fontconfig
              freetype
              harfbuzz
              simdutf
            ]
            ++ lib.optionals stdenv.hostPlatform.isLinux [
              libxau
              libxdmcp
              libxkbcommon
              vulkan-headers
              vulkan-loader
              wayland
              xorg.libxcb
            ];

          # The project's runner honours the usual toolchain env vars. Keep the
          # build out of $src (read-only store path) via -B.
          buildPhase = ''
            runHook preBuild
            ${configureBuildEnvironment {
              inherit sanitizer warningsAsErrors;
              isLinux = stdenv.hostPlatform.isLinux;
            }}
            python3 ./build -B ${buildDirectory} -j "$NIX_BUILD_CORES"
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            install -Dm755 ${buildDirectory}/st "$out/bin/st"
            install -Dm755 ${buildDirectory}/pt "$out/bin/pt"
            install -Dm644 bin/st/shitty.desktop \
              "$out/share/applications/shitty.desktop"
            install -Dm644 bin/pt/pretty.desktop \
              "$out/share/applications/pretty.desktop"
            install -Dm644 bin/st/shitty.svg \
              "$out/share/icons/hicolor/scalable/apps/shitty.svg"
            install -Dm644 bin/pt/pretty.svg \
              "$out/share/icons/hicolor/scalable/apps/pretty.svg"
            runHook postInstall
          '';

          # Vulkan ICDs live under /run/opengl-driver on NixOS; addDriverRunpath
          # puts that directory on the binary's RUNPATH.
          postFixup = lib.optionalString stdenv.hostPlatform.isLinux ''
            addDriverRunpath "$out/bin/st"
            addDriverRunpath "$out/bin/pt"
          '';

          meta = {
            description = "Small, fast terminal emulator with Wayland/X11/Vulkan and macOS/Metal frontends";
            homepage = "https://github.com/pg83/shitty";
            license = with lib.licenses; [
              gpl3Plus
              mit
            ];
            mainProgram = "st";
            platforms = lib.platforms.linux ++ lib.platforms.darwin;
          };
        };

      mkTestCheck =
        pkgs:
        {
          sanitizer ? null,
          coverage ? false,
          straceAudit ? false,
          testGroup ? null,
          testGroupCount ? null,
          stdenv ? pkgs.llvmPackages.stdenv,
        }:
        assert !(coverage && sanitizer != null);
        assert !(straceAudit && (coverage || sanitizer != null));
        assert (testGroup == null) == (testGroupCount == null);
        assert
          testGroup == null
          || (
            builtins.isInt testGroup
            && builtins.isInt testGroupCount
            && testGroupCount > 0
            && testGroup >= 0
            && testGroup < testGroupCount
          );
        let
          base = mkShitty pkgs { inherit sanitizer stdenv; };
          sanitizerSuffix = lib.optionalString (sanitizer != null) "-${sanitizer}";
          straceSuffix = lib.optionalString straceAudit "-sandboxed";
          partitioned = testGroup != null;
          partitionSuffix = lib.optionalString partitioned "-group-${toString testGroup}-of-${toString testGroupCount}";
          testPartitionArgs = lib.escapeShellArgs (
            lib.optionals partitioned [
              "-Dgroup=${toString testGroup}"
              "-Dgroup_count=${toString testGroupCount}"
            ]
          );
          testTarget = if sanitizer == null then "test" else "instrumented-test";
          checkSuffix = "${
            if coverage then "-coverage" else sanitizerSuffix
          }${straceSuffix}${partitionSuffix}";
          buildDirectory = ".build-tests${checkSuffix}";
        in
        base.overrideAttrs (old: {
          pname = "terminal-tests${checkSuffix}";

          nativeBuildInputs =
            old.nativeBuildInputs
            ++ [
              pkgs.ncurses
              pkgs.perl
              pkgs.vttest
            ]
            ++ lib.optionals pkgs.stdenv.hostPlatform.isLinux [ pkgs.strace ]
            ++ lib.optionals (sanitizer != null || coverage) [ pkgs.llvmPackages.llvm ];

          postPatch = old.postPatch + ''
            # Some vendored xterm scripts invoke other scripts by their
            # /usr/bin/env shebang. Rewrite those interpreters for the Nix
            # sandbox without modifying the upstream fixtures in git.
            patchShebangs \
              tst/xterm_vttests/upstream \
              tst/xterm_vttests/bin
            for script in $(grep -l "CMD='/bin/echo'" tst/xterm_vttests/upstream/*.sh); do
              # The prefix adapter preserves and rewrites these two unbounded
              # generators in memory; leave their upstream source intact.
              case "$script" in
                */8colors.sh|*/16colors.sh) continue ;;
              esac
              substituteInPlace "$script" \
                --replace-fail "CMD='/bin/echo'" "CMD='echo'"
            done
          '';

          FONTCONFIG_FILE = pkgs.makeFontsConf {
            fontDirectories = with pkgs; [
              dejavu_fonts
              ibm-plex
            ];
          };

          buildPhase = ''
            runHook preBuild
            ${configureBuildEnvironment {
              inherit sanitizer coverage;
              isLinux = pkgs.stdenv.hostPlatform.isLinux;
            }}
            ${lib.optionalString pkgs.stdenv.hostPlatform.isLinux ''
              # Software Vulkan for the headless-surface shadow renderer:
              # the smoke test must fail, not skip, when lavapipe breaks.
              export VK_DRIVER_FILES="$(echo ${pkgs.mesa}/share/vulkan/icd.d/lvp_icd.*.json)"
              export VK_ICD_FILENAMES="$VK_DRIVER_FILES"
              export SHITTY_TEST_VULKAN_REQUIRED=1
            ''}
            ${lib.optionalString (sanitizer == "asan") ''
              export ASAN_SYMBOLIZER_PATH=${lib.getExe' pkgs.llvmPackages.llvm "llvm-symbolizer"}
            ''}
            ${lib.optionalString coverage ''
              profileDirectory="$TMPDIR/shitty-coverage-profiles"
              mkdir -p "$profileDirectory"
              export LLVM_PROFILE_FILE="$profileDirectory/%b-%16m.profraw"
            ''}
            python3 ./build \
              -B ${buildDirectory} \
              -j "$NIX_BUILD_CORES" \
              -k ${lib.optionalString straceAudit "--strace"} ${testPartitionArgs} ${testTarget}
            ${lib.optionalString coverage ''
              # Groups deliberately do not publish their outputs. Ask the
              # runner for every instrumented executable used by the suite so
              # stable source-root symlinks can be passed to llvm-cov. A
              # profile without its owning object is silently absent from the
              # report: in particular that used to discard the TOML corpus
              # executed by toml_dump and the production-parser tier.
              python3 ./build \
                -B ${buildDirectory} \
                -j "$NIX_BUILD_CORES" \
                st pt \
                st_test pt_test \
                st_test_prod_parser pt_test_prod_parser \
                unit_tests toml_dump plt_unit_tests \
                plt_wayland_integration_tests
              coverageDirectory="$PWD/.coverage"
              coverageIgnore='(^|/)(tst|ext/libstd|\.build[^/]*)/|(^|/)[^/]*_ut\.cpp$|(^|/)(test_mode|test_input)\.(cpp|h)$|^/nix/store/'
              mkdir -p "$coverageDirectory"
              coverageBinaries=(
                ./st
                ./pt
                ./st_test
                ./pt_test
                ./st_test_prod_parser
                ./pt_test_prod_parser
                ./unit_tests
                ./toml_dump
                ./plt_unit_tests
              )
              coverageProfiles=()
              for binary in "''${coverageBinaries[@]}"; do
                buildId="$(llvm-readelf -n "$binary" |
                  sed -n 's/.*Build ID: //p' |
                  head -1)"
                if [[ -z "$buildId" ]]; then
                  echo "coverage binary has no build ID: $binary" >&2
                  exit 1
                fi
                binaryProfiles=("$profileDirectory/$buildId"-*.profraw)
                if [[ ! -e "''${binaryProfiles[0]}" ]]; then
                  echo "coverage binary produced no profiles: $binary" >&2
                  exit 1
                fi
                coverageProfiles+=("''${binaryProfiles[@]}")
              done
              primaryCoverageBinary="''${coverageBinaries[0]}"
              coverageObjects=()
              for binary in "''${coverageBinaries[@]:1}"; do
                coverageObjects+=("-object=$binary")
              done
              waylandBinary=./plt_wayland_integration_tests
              waylandBuildId="$(llvm-readelf -n "$waylandBinary" |
                sed -n 's/.*Build ID: //p' |
                head -1)"
              if [[ -z "$waylandBuildId" ]]; then
                echo "coverage binary has no build ID: $waylandBinary" >&2
                exit 1
              fi
              waylandProfiles=("$profileDirectory/$waylandBuildId"-*.profraw)
              if [[ ! -e "''${waylandProfiles[0]}" ]]; then
                echo "coverage binary produced no profiles: $waylandBinary" >&2
                exit 1
              fi
              llvm-profdata merge \
                -sparse \
                "''${coverageProfiles[@]}" \
                -o "$coverageDirectory/coverage.profdata"
              llvm-profdata merge \
                -sparse \
                "''${waylandProfiles[@]}" \
                -o "$coverageDirectory/wayland.profdata"
              llvm-cov export \
                "$primaryCoverageBinary" \
                "''${coverageObjects[@]}" \
                -instr-profile="$coverageDirectory/coverage.profdata" \
                -format=lcov \
                -ignore-filename-regex="$coverageIgnore" \
                > "$coverageDirectory/coverage.info"
              llvm-cov export \
                "$waylandBinary" \
                -instr-profile="$coverageDirectory/wayland.profdata" \
                -format=lcov \
                -ignore-filename-regex="$coverageIgnore" \
                > "$coverageDirectory/wayland-coverage.info"
              {
                echo "Core coverage"
                llvm-cov report \
                  "$primaryCoverageBinary" \
                  "''${coverageObjects[@]}" \
                  -instr-profile="$coverageDirectory/coverage.profdata" \
                  -ignore-filename-regex="$coverageIgnore"
                echo
                echo "Wayland integration coverage"
                llvm-cov report \
                  "$waylandBinary" \
                  -instr-profile="$coverageDirectory/wayland.profdata" \
                  -ignore-filename-regex="$coverageIgnore"
              } > "$coverageDirectory/summary.txt"
              llvm-cov show \
                "$primaryCoverageBinary" \
                "''${coverageObjects[@]}" \
                -instr-profile="$coverageDirectory/coverage.profdata" \
                -format=html \
                -output-dir="$coverageDirectory/html" \
                -show-branches=percent \
                -coverage-watermark=80,50 \
                -ignore-filename-regex="$coverageIgnore"
              llvm-cov show \
                "$waylandBinary" \
                -instr-profile="$coverageDirectory/wayland.profdata" \
                -format=html \
                -output-dir="$coverageDirectory/html/wayland" \
                -show-branches=percent \
                -coverage-watermark=80,50 \
                -ignore-filename-regex="$coverageIgnore"
              for coverageInfo in \
                "$coverageDirectory/coverage.info" \
                "$coverageDirectory/wayland-coverage.info"; do
                substituteInPlace "$coverageInfo" \
                  --replace-quiet "SF:$PWD/" "SF:"
                if grep -q '^SF:/' "$coverageInfo"; then
                  echo "coverage report contains absolute source paths: $coverageInfo" >&2
                  grep '^SF:/' "$coverageInfo" | head -10 >&2
                  exit 1
                fi
                if ! grep -q '^SF:' "$coverageInfo"; then
                  echo "coverage report does not contain source files: $coverageInfo" >&2
                  exit 1
                fi
              done
              cat "$coverageDirectory/summary.txt"
            ''}
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            mkdir -p "$out"
            ${
              if coverage then
                ''
                  install -Dm644 .coverage/coverage.info "$out/coverage.info"
                  install -Dm644 .coverage/wayland-coverage.info "$out/wayland-coverage.info"
                  install -Dm644 .coverage/summary.txt "$out/summary.txt"
                  cp -R .coverage/html "$out/html"
                ''
              else
                ''
                  touch "$out/passed"
                ''
            }
            runHook postInstall
          '';

          postFixup = "";
        });

      mkDevShell =
        pkgs:
        let
          stdenv = pkgs.llvmPackages.stdenv;
          shitty = mkShitty pkgs { };
        in
        pkgs.mkShell.override { inherit stdenv; } {
          inputsFrom = [ shitty ];
          NIX_CC_WRAPPER_SUPPRESS_TARGET_WARNING = "1";

          packages =
            with pkgs;
            [
              clang-tools
              gdb
              ncurses
              perl
              vttest
              # Fonts for manual runs inside the shell.
              dejavu_fonts
              ibm-plex
            ]
            ++ lib.optionals stdenv.hostPlatform.isLinux [ strace ];

          FONTCONFIG_FILE = pkgs.makeFontsConf {
            fontDirectories = with pkgs; [
              dejavu_fonts
              ibm-plex
            ];
          };

          shellHook = ''
            # Keep ambient toolchain flags from contaminating this shell.
            unset CPPFLAGS CFLAGS CXXFLAGS LDFLAGS
            NIX_LDFLAGS="''${NIX_LDFLAGS#"-rpath $out/lib "}"
            export NIX_LDFLAGS
            export LDFLAGS="$(pkg-config --libs wayland-client xkbcommon xcb) -lrt"
            echo "shitty dev shell — run: ./build && ./st"
          '';
        };
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgsFor system;
          shitty = mkShitty pkgs { };
        in
        {
          default = shitty;
          pretty = shitty;
          shitty = shitty;
        }
      );

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/st";
        };
        pretty = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/pt";
        };
        shitty = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/st";
        };
      });

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgsFor system;
        in
        {
          default = mkDevShell pkgs;
        }
      );

      checks = forAllSystems (
        system:
        let
          pkgs = nixpkgsFor system;
          darwinTestGroupCount = 5;
          sandboxedGroupCount = 5;
        in
        {
          build = mkShitty pkgs { warningsAsErrors = true; };
          tests = mkTestCheck pkgs { };
          tests-gcc = mkTestCheck pkgs { stdenv = pkgs.gcc16Stdenv; };
          coverage = mkTestCheck pkgs { coverage = true; };
          build-asan = mkShitty pkgs { sanitizer = "asan"; };
          tests-asan = mkTestCheck pkgs { sanitizer = "asan"; };
          build-ubsan = mkShitty pkgs { sanitizer = "ubsan"; };
          tests-ubsan = mkTestCheck pkgs { sanitizer = "ubsan"; };
        }
        // lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux (
          lib.listToAttrs (
            map (testGroup: {
              name = "sandboxed-${toString testGroup}-of-${toString sandboxedGroupCount}";
              value = mkTestCheck pkgs {
                straceAudit = true;
                inherit testGroup;
                testGroupCount = sandboxedGroupCount;
              };
            }) (lib.range 0 (sandboxedGroupCount - 1))
          )
        )
        // lib.optionalAttrs pkgs.stdenv.hostPlatform.isDarwin (
          lib.listToAttrs (
            map (testGroup: {
              name = "tests-${toString testGroup}-of-${toString darwinTestGroupCount}";
              value = mkTestCheck pkgs {
                inherit testGroup;
                testGroupCount = darwinTestGroupCount;
              };
            }) (lib.range 0 (darwinTestGroupCount - 1))
          )
        )
      );

      formatter = forAllSystems (system: (nixpkgsFor system).nixfmt);

      # Overlay so consumers can `pkgs.shitty` after importing the flake.
      overlays.default = final: prev: {
        pretty = mkShitty final { };
        shitty = mkShitty final { };
      };
    };
}
