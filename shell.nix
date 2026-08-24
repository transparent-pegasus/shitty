# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.
#
# Legacy entry point for `nix-shell`. Prefer `nix develop` from the flake.
# Keep this package set aligned with flake.nix's mkDevShell / mkShitty.

{
  pkgs ? import <nixpkgs> { },
}:

(pkgs.mkShell.override { stdenv = pkgs.llvmPackages.stdenv; }) {
  FONTCONFIG_FILE = pkgs.makeFontsConf {
    fontDirectories = [
      pkgs.dejavu_fonts
      pkgs.ibm-plex
    ];
  };

  packages = with pkgs; [
    brotli
    clang-tools
    fontconfig
    freetype
    glslang
    harfbuzz
    libxau
    libxdmcp
    libxkbcommon
    ncurses
    perl
    pkg-config
    python3
    ragel
    simdutf
    vttest
    vulkan-headers
    vulkan-loader
    wayland
    wayland-protocols
    wayland-scanner
    xorg.libxcb
  ];

  shellHook = ''
    # Keep ambient toolchain flags from contaminating this shell.
    unset CPPFLAGS CFLAGS CXXFLAGS LDFLAGS
    export LDFLAGS="$(pkg-config --libs wayland-client xkbcommon xcb) -lrt"
  '';
}
