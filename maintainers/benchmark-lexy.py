#!/usr/bin/env python3
"""Compare parsing CPU time and process memory for both backends (Linux)."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import tempfile
import time
import sys

from parser_benchmark_support import affinity_prefix, file_sha256, record_library

from parser_workloads import extended_sources


def run_timing(command, env):
    # Evaluator setup allocates outside the timed region. Bound the whole process
    # while calibrating, especially for inputs that parse in microseconds.
    with tempfile.TemporaryFile() as log:
        with subprocess.Popen(command, env=env, stdout=log, stderr=log) as child:
            started = time.monotonic()
            while child.poll() is None:
                try:
                    status = Path(f"/proc/{child.pid}/status").read_text()
                    rss = next(int(line.split()[1]) for line in status.splitlines() if line.startswith("VmRSS:"))
                except (FileNotFoundError, ProcessLookupError, StopIteration):
                    rss = 0
                if rss > 1024 * 1024 or time.monotonic() - started > 300:
                    child.kill()
                    child.wait()
                    raise RuntimeError("benchmark exceeded 1 GiB RSS or 300 seconds; use fewer fixed iterations")
                time.sleep(0.05)
            if child.returncode:
                log.seek(0)
                raise RuntimeError(log.read().decode(errors="replace"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nixpkgs", type=Path)
    parser.add_argument("--stage", default="comparison", help="label for this measurement")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--binary", type=Path, help="override the executable under --build-dir")
    parser.add_argument("--allocator", type=Path, help="shared library to preload, matching the CLI allocator")
    parser.add_argument("--library-dir", type=Path, help="load matching Nix libraries from this directory (must match the executable ABI)")
    parser.add_argument("--cpu", type=int, help="pin each child to this Linux CPU")
    parser.add_argument("--repetitions", type=int, default=7)
    parser.add_argument("--min-time", default="100x")
    parser.add_argument("--workload", action="append", help="limit to named workloads")
    parser.add_argument("--suite", choices=("original", "extended"), default="original")
    parser.add_argument("--mode", choices=("Parse", "ParseAndBind"), default="Parse")
    parser.add_argument("--small-iterations", type=int, default=100,
                        help="fixed iterations per sample for inputs smaller than 4096 bytes")
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("this driver uses Linux /proc for its memory limit")
    if args.repetitions < 2:
        parser.error("use at least two repetitions")
    binary = (args.binary or args.build_dir / "src/libexpr-tests/nix-expr-benchmarks").resolve()
    prefix = affinity_prefix(parser, args.cpu)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, LC_ALL="C", NO_COLOR="1")
    if args.library_dir:
        env["LD_LIBRARY_PATH"] = os.pathsep.join(filter(None, (str(args.library_dir.resolve()), env.get("LD_LIBRARY_PATH"))))
    if args.allocator:
        env["LD_PRELOAD"] = str(args.allocator.resolve())
    else:
        env.pop("LD_PRELOAD", None)

    # Complement interpolated strings with plain literals, names and paths.
    sources = {
        "plain": "[\n" + ('"' + "ordinary literal text " * 6 + '"\n') * 1000 + "]",
        "indented": "[\n" + ("''\n  first line\n    " + "literal text " * 8 + "\n  ''\n") * 1000 + "]",
        "names": "{\n" + "".join(
            f"long_attribute_name_for_allocation_{i} = let long_local_variable_name_{i} = 1; "
            f"in long_local_variable_name_{i};\n" for i in range(1000)) + "}",
        "paths": "[\n" + "".join(f"./first/second/third/fourth/package-{i}/default.nix\n" for i in range(2000)) + "]",
        "all-packages": (args.nixpkgs / "pkgs/top-level/all-packages.nix").read_text(),
    }
    if args.suite == "extended":
        sources.update(extended_sources())
    workloads = {"strings": "strings/10000", "attrs": "attrs/1000", **dict.fromkeys(sources, "file")}
    if args.workload and (unknown := set(args.workload) - workloads.keys()):
        parser.error(f"unknown workloads: {', '.join(sorted(unknown))}")
    if args.small_iterations < 1:
        parser.error("--small-iterations must be positive")
    report = {
        "stage": args.stage,
        "parent_revision": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        "source_diff_sha256": hashlib.sha256(subprocess.check_output(["git", "diff", "HEAD", "--", "src"])).hexdigest(),
        "binary_sha256": file_sha256(binary),
        "allocator": str(args.allocator.resolve()) if args.allocator else "system default",
        "cpu": args.cpu, "repetitions": args.repetitions, "min_time": args.min_time,
        "suite": args.suite, "mode": args.mode,
        "complete": False,
        "hashing": "streamed to bound driver memory before launching benchmarks",
        "protocol": f"warm-buffer {args.mode}; random interleaving of both backends; CPU medians; RSS from three fresh processes with 20 iterations each (includes setup/binding/teardown)",
        "workloads": {},
    }
    record_library(parser, report, binary, args.library_dir)
    with tempfile.TemporaryDirectory(prefix="lexy-benchmark-") as directory:
        temporary = Path(directory)
        for name, benchmark in workloads.items():
            if args.workload and name not in args.workload:
                continue
            if name in sources:
                source = temporary / (name + ".nix")
                source.write_text(sources[name])
                env["NIX_PARSER_BENCH_FILE"] = str(source)
            else:
                env.pop("NIX_PARSER_BENCH_FILE", None)
            output = temporary / "timings.json"
            min_time = (f"{args.small_iterations}x" if name in sources and len(sources[name]) < 4096
                        else args.min_time)
            base_command = [*prefix, str(binary)]
            command = base_command + [
                       f"--benchmark_filter=^{args.mode}/{benchmark}/(bison|lexy)$",
                       f"--benchmark_min_time={min_time}",
                       f"--benchmark_repetitions={args.repetitions}",
                       "--benchmark_enable_random_interleaving=true",
                       f"--benchmark_out={output}", "--benchmark_out_format=json"]
            run_timing(command, env)
            measurements = json.loads(output.read_text())
            entry = {"min_time": min_time, "backends": {}}
            if name in sources:
                entry["source_sha256"] = hashlib.sha256(sources[name].encode()).hexdigest()
                entry["source_bytes"] = len(sources[name].encode())
            for backend in ("bison", "lexy"):
                rows = [row for row in measurements["benchmarks"]
                        if row.get("run_type") == "iteration" and row["name"] == f"{args.mode}/{benchmark}/{backend}"]
                if len(rows) != args.repetitions or any(row.get("error_occurred") for row in rows):
                    raise RuntimeError(f"missing or failed benchmarks: {name}/{backend}")
                scales = {"ns": 1e-6, "us": 1e-3, "ms": 1, "s": 1e3}
                times = [row["cpu_time"] * scales[row["time_unit"]] for row in rows]
                resources = []
                for _ in range(3):
                    fixed = base_command + [f"--benchmark_filter=^{args.mode}/{benchmark}/{backend}$",
                                            "--benchmark_min_time=20x", "--benchmark_repetitions=1"]
                    with tempfile.TemporaryFile() as log:
                        child = subprocess.Popen(fixed, env=env, stdout=log, stderr=log)
                        _, status, usage = os.wait4(child.pid, 0)
                        child.returncode = os.waitstatus_to_exitcode(status)
                        if child.returncode:
                            log.seek(0)
                            raise RuntimeError(log.read().decode())
                    resources.append({"rss_bytes": usage.ru_maxrss * 1024,
                                      "minor_faults": usage.ru_minflt,
                                      "cpu_seconds": usage.ru_utime + usage.ru_stime})
                entry["backends"][backend] = {
                    "cpu_ms_samples": times, "median_cpu_ms": statistics.median(times),
                    "wall_ms_samples": [row["real_time"] * scales[row["time_unit"]] for row in rows],
                    "cpu_cv": statistics.stdev(times) / statistics.mean(times),
                    "median_rss_bytes": statistics.median(r["rss_bytes"] for r in resources),
                    "resource_samples": resources,
                }
            report["workloads"][name] = entry
            args.output.write_text(json.dumps(report, indent=2) + "\n")
            print(name, {b: round(r["median_cpu_ms"], 4) for b, r in entry["backends"].items()}, flush=True)
    report["complete"] = True
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
