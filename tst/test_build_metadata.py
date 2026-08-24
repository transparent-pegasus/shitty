# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

import concurrent.futures
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from importlib.machinery import SourceFileLoader
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]


class BuildMetadataTests(unittest.TestCase):
    def run_build(self, build_file, *arguments):
        return subprocess.run(
            [
                sys.executable,
                ROOT / "build",
                "--build-file",
                build_file,
                *arguments,
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def run_build_with_descr(self, descr):
        with tempfile.TemporaryDirectory() as directory:
            build_file = Path(directory) / "build.py"
            build_file.write_text(
                "target = command(\n"
                "    outputs=['$(B)/result'],\n"
                "    cmd=['true'],\n"
                f"    descr={descr!r},\n"
                ")\n"
                "install(target)\n"
            )
            return self.run_build(build_file, "--list")

    def test_build_accepts_exactly_two_ascii_letters_in_descr(self):
        result = self.run_build_with_descr("OK")

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_build_rejects_every_other_descr_shape(self):
        for descr in ("A", "ABC", "A1", "A ", "ÄB"):
            with self.subTest(descr=descr):
                result = self.run_build_with_descr(descr)

                self.assertNotEqual(result.returncode, 0)
                self.assertIn(
                    "descr must be exactly two ASCII letters",
                    result.stderr,
                )

    def test_groups_are_additive_cli_aliases(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build_file = root / "build.py"
            build_file.write_text(
                "one = command(\n"
                "    outputs=['$(B)/one'],\n"
                "    cmd=['python3', '-c', "
                "\"from pathlib import Path; Path(r'$(B)/one').touch()\"],\n"
                ")\n"
                "two = command(\n"
                "    outputs=['$(B)/two'],\n"
                "    cmd=['python3', '-c', "
                "\"from pathlib import Path; Path(r'$(B)/two').touch()\"],\n"
                ")\n"
                "group('batch', one)\n"
                "group('batch', two)\n"
                "group('install', one)\n"
            )

            listed = self.run_build(build_file, "--list")
            self.assertEqual(listed.returncode, 0, listed.stderr)
            self.assertEqual(
                listed.stdout.splitlines(),
                ["batch", "install", "one", "two"],
            )

            result = self.run_build(build_file, "-B", ".out", "batch")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((root / ".out" / "one").exists())
            self.assertTrue((root / ".out" / "two").exists())
            self.assertFalse((root / "one").exists())
            self.assertFalse((root / "two").exists())

    def test_install_group_is_the_default(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build_file = root / "build.py"
            build_file.write_text(
                "target = command(\n"
                "    outputs=['$(B)/result'],\n"
                "    cmd=['python3', '-c', "
                "\"from pathlib import Path; Path(r'$(B)/result').touch()\"],\n"
                ")\n"
                "group('install', target)\n"
            )

            result = self.run_build(build_file, "-B", ".out")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((root / ".out" / "result").exists())

    def test_group_name_cannot_conflict_with_target_name(self):
        with tempfile.TemporaryDirectory() as directory:
            build_file = Path(directory) / "build.py"
            build_file.write_text(
                "same = command(\n"
                "    outputs=['$(B)/result'],\n"
                "    cmd=['/usr/bin/touch', '$(B)/result'],\n"
                ")\n"
                "group('same', same)\n"
            )

            result = self.run_build(build_file, "--list")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "group name conflicts with target name: same",
                result.stderr,
            )

    def test_libstd_is_bundled_as_source(self):
        self.assertFalse((ROOT / ".gitmodules").exists())
        self.assertTrue((ROOT / "ext/libstd/build.py").is_file())
        self.assertTrue((ROOT / "ext/libstd/std/lib/buffer.cpp").is_file())

    def test_readme_builds_without_submodule_setup(self):
        readme = (ROOT / "README.md").read_text()
        self.assertNotIn("git submodule", readme)
        self.assertIn("ext/libstd", readme)

    def test_header_probe_uses_target_compiler_and_current_flags(self):
        loader = SourceFileLoader("shitty_build_header_probe", str(ROOT / "build"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.assertIsNotNone(spec)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[loader.name] = runner
        loader.exec_module(runner)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            context = runner.BuildContext(
                root,
                root / ".out",
                target="aarch64-unknown-linux-gnu",
            )
            context.cflags = ["-target-c"]
            context.cxxflags = ["-target-cxx"]
            context.cppflags = ["-target-cpp"]
            compiler = ["target-c++", "--target=aarch64-unknown-linux-gnu"]
            completed = subprocess.CompletedProcess(compiler, 0)
            with mock.patch.object(
                context,
                "_compiler_command",
                return_value=compiler,
            ), mock.patch.object(
                runner.subprocess,
                "run",
                return_value=completed,
            ) as run:
                self.assertTrue(context.have_header("optional/header.h"))

            command = run.call_args.args[0]
            self.assertEqual(command[:2], compiler)
            self.assertIn("-target-c", command)
            self.assertIn("-target-cxx", command)
            self.assertIn("-target-cpp", command)
            self.assertEqual(
                run.call_args.kwargs["input"],
                "#include <optional/header.h>\n",
            )

    def test_static_pkg_config_includes_private_link_dependencies(self):
        loader = SourceFileLoader("shitty_build_pkg_config", str(ROOT / "build"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.assertIsNotNone(spec)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[loader.name] = runner
        loader.exec_module(runner)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            context = runner.BuildContext(root, root / ".out")
            with mock.patch.object(
                runner.subprocess,
                "check_output",
                side_effect=["-I/xcb/include\n", "-lxcb -lXau\n"],
            ) as check_output:
                dependency = context.pkg_config("xcb", static=True)

            self.assertEqual(dependency.public_cflags, ["-I/xcb/include"])
            self.assertEqual(dependency.ldflags, ["-lxcb", "-lXau"])
            self.assertEqual(
                check_output.call_args_list[1].args[0],
                ["pkg-config", "xcb", "--static", "--libs"],
            )

    def test_tool_resolution_preserves_the_path_selected_argv_zero(self):
        loader = SourceFileLoader("shitty_build_tool_path", str(ROOT / "build"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.assertIsNotNone(spec)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[loader.name] = runner
        loader.exec_module(runner)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            implementation = root / "multicall"
            implementation.touch()
            alias = root / "sh"
            alias.symlink_to(implementation.name)
            context = runner.BuildContext(root, root / ".out")
            with mock.patch.object(
                runner.shutil,
                "which",
                return_value=str(alias),
            ):
                resolved = context.resolve_tool("sh")

            self.assertEqual(resolved, str(alias))
            self.assertNotEqual(resolved, str(implementation))

    def test_imported_program_gets_injected_dependency_link_flags(self):
        loader = SourceFileLoader("shitty_build_import_ldflags", str(ROOT / "build"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.assertIsNotNone(spec)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[loader.name] = runner
        loader.exec_module(runner)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            dependency = root / "dependency"
            application = root / "application"
            dependency.mkdir()
            application.mkdir()
            (dependency / "support.c").write_text("int support(void) { return 0; }\n")
            (dependency / "build.py").write_text(
                "support = library(srcs=['$(S)/support.c'])\n"
            )
            (application / "main.c").write_text("int main(void) { return 0; }\n")
            (application / "build.py").write_text(
                "app = program(srcs=['$(S)/main.c'])\n"
            )
            build_file = root / "build.py"
            build_file.write_text(
                "support = import_build('dependency/build.py', 'libsupport.a')\n"
                "support.ldflags += ['-lsupport-runtime']\n"
                "app = import_build(\n"
                "    'application/build.py', 'app', deps=[support],\n"
                ")\n"
            )

            context = runner.BuildContext(root, root / ".out")
            context.load(build_file)
            context.build_graph()

            support = context.target_names["support"]
            app = context.target_names["app"]
            command = app.root.commands[-1]
            archive = command.index(support.output)
            self.assertEqual(command[archive + 1], "-lsupport-runtime")

    def test_strace_parser_counts_stat_and_file_backed_mmap(self):
        loader = SourceFileLoader("shitty_build_strace", str(ROOT / "build"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.assertIsNotNone(spec)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[loader.name] = runner
        loader.exec_module(runner)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stat_input = root / "from-stat"
            mmap_input = root / "from-mmap"
            declared = root / "declared"
            generated = root / "generated"
            build_root = root / ".out"
            build_output = build_root / "generated"
            for path in (stat_input, mmap_input, declared, generated, build_output):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            payload = {
                "source_root": str(root),
                "build_root": str(build_root),
                "cwd": str(root),
                "inputs": [str(declared)],
            }
            trace = (
                f'stat("{stat_input}", {{st_mode=S_IFREG|0644}}) = 0\n'
                f'mmap(NULL, 1, PROT_READ, MAP_PRIVATE, '
                f'3<{mmap_input}>, 0) = 0x1234\n'
                f'open("{declared}", O_RDONLY) = 4<{declared}>\n'
                f'42 open("{generated}", O_RDWR|O_CREAT|O_EXCL, 0600 '
                f'<unfinished ...>\n'
                f'42 <... open resumed>) = 5<{generated}>\n'
                f'stat("{generated}", {{st_mode=S_IFREG|0600}}) = 0\n'
                f'mmap(NULL, 1, PROT_READ, MAP_PRIVATE, '
                f'5<{generated}>, 0) = 0x5678\n'
                f'newfstatat(7<{root}/vanished (deleted)>, "from-dirfd", '
                f'{{st_mode=S_IFREG|0644}}, 0) = 0\n'
                f'newfstatat(AT_FDCWD<{build_root}>, "generated", '
                f'{{st_mode=S_IFREG|0644}}, 0) = 0\n'
            )

            self.assertEqual(
                runner._strace_missing_inputs(payload, trace),
                [
                    "$(S)/from-mmap",
                    "$(S)/from-stat",
                    "$(S)/vanished/from-dirfd",
                ],
            )

    @unittest.skipUnless(
        sys.platform.startswith("linux") and shutil.which("strace"),
        "strace is only available on Linux",
    )
    @unittest.skipIf(
        os.environ.get("BUILD_STRACE_ACTIVE"),
        "ptrace cannot be nested inside a strace audit",
    )
    def test_strace_rejects_undeclared_read_on_a_cached_node(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            hidden = root / "hidden.txt"
            hidden.write_text("hidden")
            build_file = root / "build.py"
            build_file.write_text(
                "target = command(\n"
                "    outputs=['$(B)/result'],\n"
                "    cmd=['python3', '-c', \"import os; from pathlib import Path; "
                "os.stat(r'$(S)/hidden.txt'); Path(r'$(B)/result').touch()\"],\n"
                ")\n"
                "install(target)\n"
            )

            cached = self.run_build(build_file, "-B", ".out", "install")
            self.assertEqual(cached.returncode, 0, cached.stderr)

            traced = self.run_build(
                build_file,
                "-B", ".out",
                "--strace",
                "install",
            )
            self.assertNotEqual(traced.returncode, 0)
            self.assertIn("undeclared source input(s)", traced.stderr)
            self.assertIn("$(S)/hidden.txt", traced.stderr)

    @unittest.skipUnless(
        sys.platform.startswith("linux") and shutil.which("strace"),
        "strace is only available on Linux",
    )
    @unittest.skipIf(
        os.environ.get("BUILD_STRACE_ACTIVE"),
        "ptrace cannot be nested inside a strace audit",
    )
    def test_strace_accepts_inputs_from_the_dependency_closure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "dependency.txt").write_text("dependency")
            (root / "direct.txt").write_text("direct")
            build_file = root / "build.py"
            build_file.write_text(
                "dependency = command(\n"
                "    inputs=['$(S)/dependency.txt'],\n"
                "    outputs=['$(B)/dependency'],\n"
                "    cmd=['python3', '-c', \"from pathlib import Path; "
                "Path(r'$(S)/dependency.txt').read_text(); "
                "Path(r'$(B)/dependency').touch()\"],\n"
                ")\n"
                "target = command(\n"
                "    inputs=['$(S)/direct.txt'],\n"
                "    deps=[dependency],\n"
                "    outputs=['$(B)/result'],\n"
                "    cmd=['python3', '-c', \"from pathlib import Path; "
                "Path(r'$(S)/direct.txt').read_text(); "
                "Path(r'$(S)/dependency.txt').read_text(); "
                "Path(r'$(B)/result').touch()\"],\n"
                ")\n"
                "install(target)\n"
            )

            result = self.run_build(
                build_file,
                "-B", ".out",
                "--strace",
                "install",
            )
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_parallel_execution_with_the_same_uid_keeps_a_stable_lock(self):
        loader = SourceFileLoader("shitty_build_parallel_uid", str(ROOT / "build"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.assertIsNotNone(spec)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[loader.name] = runner
        loader.exec_module(runner)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            context = runner.BuildContext(root, root / ".out")
            executor = runner.Executor(
                context,
                jobs=2,
                verbose=False,
                keep_going=False,
                strace=True,
            )
            node = runner.Node(inputs=[], outputs=[], commands=[])
            node.uid = "same-uid"
            discarded = threading.Event()
            acquisition_lock = threading.Lock()
            acquisitions = 0
            real_flock = runner.fcntl.flock
            real_discard = executor._discard_contents

            def flock(fd, operation):
                nonlocal acquisitions
                real_flock(fd, operation)
                with acquisition_lock:
                    acquisitions += 1
                    acquisition = acquisitions
                if acquisition == 2:
                    self.assertTrue(discarded.wait(timeout=5))

            def discard(path):
                real_discard(path)
                discarded.set()

            executor._discard_contents = discard
            with mock.patch.object(runner.fcntl, "flock", side_effect=flock):
                with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                    futures = [
                        pool.submit(executor._execute, node, True, False)
                        for _ in range(2)
                    ]
                    for future in futures:
                        future.result(timeout=10)

            self.assertEqual(acquisitions, 2)

    def test_unit_test_suites_have_twenty_shard_nodes(self):
        result = self.run_build(ROOT / "build.py", "--list")
        self.assertEqual(result.returncode, 0, result.stderr)

        targets = result.stdout.splitlines()
        self.assertIn("test_suite", targets)
        self.assertIn("test_suite_prod_parser", targets)
        for prefix in (
            "unit_tests_group_",
            "test_suite_group_",
            "test_suite_prod_parser_group_",
        ):
            with self.subTest(prefix=prefix):
                self.assertEqual(
                    [target for target in targets if target.startswith(prefix)],
                    [f"{prefix}{group:02}" for group in range(20)],
                )

    def test_test_partitions_are_deterministic_complete_and_disjoint(self):
        loader = SourceFileLoader("shitty_build_runner", str(ROOT / "build"))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.assertIsNotNone(spec)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[loader.name] = runner
        loader.exec_module(runner)

        with tempfile.TemporaryDirectory() as directory:
            def test_ids(values, suffix):
                context = runner.BuildContext(
                    ROOT,
                    Path(directory) / suffix,
                    runner.Flags(values),
                )
                context.load(ROOT / "build.py")
                return {
                    target.name or target.output or "\0".join(target.outputs)
                    for target in context.groups["test"]
                }

            full = test_ids({}, "full")
            partitions = [
                test_ids(
                    {"group": str(group), "group_count": "5"},
                    f"group-{group}",
                )
                for group in range(5)
            ]

            self.assertEqual(set().union(*partitions), full)
            self.assertEqual(sum(map(len, partitions)), len(full))
            self.assertEqual(
                test_ids({"group": "2", "group_count": "5"}, "repeat"),
                partitions[2],
            )


if __name__ == "__main__":
    unittest.main()
