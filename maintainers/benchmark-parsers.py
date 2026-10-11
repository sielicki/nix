#!/usr/bin/env python3
"""Measure warm-cache Nixpkgs evaluation with each libexpr parser (Unix)."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import tempfile
import time

from parser_benchmark_support import affinity_prefix, file_sha256, record_library

from evaluation_workloads import nixpkgs_workloads


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nixpkgs", type=Path)
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--nix", type=Path, help="override the executable under --build-dir")
    parser.add_argument("--runs", type=int, default=10)
    parser.add_argument("--output", type=Path, default=Path("parser-evaluation.json"))
    parser.add_argument("--expression", help="override the hello.drvPath workload; must evaluate to a string")
    parser.add_argument("--cpu", type=int, help="pin each child to this Linux CPU")
    parser.add_argument("--suite", choices=("original", "extended", "nixpkgs"), default="original")
    parser.add_argument("--system", default="x86_64-linux", help="target system for the nixpkgs suite")
    parser.add_argument("--workload", action="append", help="limit a named suite to these workloads (repeatable)")
    parser.add_argument("--library-dir", type=Path, help="load matching Nix libraries from this directory (must match the executable ABI)")
    args = parser.parse_args()
    if args.runs < 2:
        parser.error("use at least two measured runs per backend")
    nix = (args.nix or args.build_dir / "src/nix/nix").resolve()
    prefix = affinity_prefix(parser, args.cpu)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    root = args.nixpkgs.resolve()
    quoted_root = json.dumps(str(root), ensure_ascii=False).replace("${", "\\${")
    expression = args.expression or f"(import (builtins.toPath {quoted_root}) {{}}).hello.drvPath"
    if args.expression and args.suite != "original":
        parser.error("--expression cannot be combined with a named suite")
    if args.workload and args.suite == "original":
        parser.error("--workload requires --suite extended or --suite nixpkgs")
    env = dict(os.environ, LC_ALL="C", NO_COLOR="1")
    if args.library_dir:
        env["LD_LIBRARY_PATH"] = os.pathsep.join(filter(None, (str(args.library_dir.resolve()), env.get("LD_LIBRARY_PATH"))))

    def run(backend, expression):
        command = [*prefix, str(nix), "--extra-experimental-features", "nix-command",
                   "--option", "parser", backend, "--option", "eval-cache", "false",
                   "--option", "allow-import-from-derivation", "false",
                   "eval", "--impure", "--raw", "--expr", expression]
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            started = time.perf_counter()
            process = subprocess.Popen(command, stdout=stdout, stderr=stderr, env=env)
            # wait4 provides this child's RSS, rather than the cumulative peak
            # returned by getrusage(RUSAGE_CHILDREN) across earlier runs.
            _, status, usage = os.wait4(process.pid, 0)
            elapsed = time.perf_counter() - started
            process.returncode = os.waitstatus_to_exitcode(status)
            stdout.seek(0)
            stderr.seek(0)
            output, errors = stdout.read(), stderr.read()
        if process.returncode:
            raise RuntimeError(f"{backend} exited {process.returncode}: {errors.decode(errors='replace')}")
        return {
            "backend": backend, "elapsed_seconds": elapsed,
            "cpu_seconds": usage.ru_utime + usage.ru_stime,
            # Linux reports KiB; macOS reports bytes.
            "max_rss_bytes": usage.ru_maxrss * (1 if os.uname().sysname == "Darwin" else 1024),
            "minor_faults": usage.ru_minflt, "major_faults": usage.ru_majflt,
            "involuntary_context_switches": usage.ru_nivcsw,
            "output": output.decode(), "stderr": errors.decode(errors="replace"),
        }

    def measure(name, expression):
        # Verify outputs and warm both paths before starting measurement.
        warmups = [run(backend, expression) for backend in ("bison", "lexy")]
        expected = warmups[0]["output"]
        if warmups[1]["output"] != expected:
            raise RuntimeError("the parsers produced different evaluation results")
        samples = []
        for repetition in range(args.runs):
            order = ("bison", "lexy") if repetition % 2 == 0 else ("lexy", "bison")
            for backend in order:
                sample = run(backend, expression)
                if sample["output"] != expected:
                    raise RuntimeError("evaluation output changed during measurement")
                sample["repetition"] = repetition
                samples.append(sample)
                print(f"{name} {repetition + 1}/{args.runs} {backend}: {sample['elapsed_seconds']:.3f}s, "
                      f"{sample['max_rss_bytes'] / 1024**2:.1f} MiB", flush=True)
        summary = {}
        for backend in ("bison", "lexy"):
            summary[backend] = {
                metric: statistics.median(s[metric] for s in samples if s["backend"] == backend)
                for metric in ("elapsed_seconds", "cpu_seconds", "max_rss_bytes")
            }
        return {"expression": expression, "warmups": warmups, "samples": samples, "medians": summary}

    report = {
        "nix": str(nix), "nix_sha256": file_sha256(nix),
        "hashing": "streamed to bound driver memory before launching benchmarks",
        "parent_revision": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        "source_diff_sha256": hashlib.sha256(subprocess.check_output(["git", "diff", "HEAD", "--", "src"])).hexdigest(),
        "cpu": args.cpu,
        "suite": args.suite,
        "system": args.system if args.suite == "nixpkgs" else "builtins.currentSystem",
        "complete": False,
        "nixpkgs": str(root), "runs_per_backend": args.runs,
        "protocol": "fresh processes; file/store caches warmed by one run per backend; eval-cache and import-from-derivation disabled; alternating order",
    }
    record_library(parser, report, nix, args.library_dir)
    if args.suite != "original":
        workloads = nixpkgs_workloads(root, args.system) if args.suite == "nixpkgs" else {
            "startup": '"hello"',
            "library": f"builtins.toString (builtins.length (builtins.attrNames (import ((builtins.toPath {quoted_root}) + \"/lib\"))))",
            "modules": f'let lib = import ((builtins.toPath {quoted_root}) + "/lib"); in '
                       'builtins.toString (lib.evalModules { modules = [ { options.answer = lib.mkOption '
                       '{ type = lib.types.int; default = 42; }; } ]; }).config.answer',
            "package-names": f"builtins.toString (builtins.length (builtins.attrNames (import (builtins.toPath {quoted_root}) {{}})))",
            "hello": expression,
            "packages": f'let pkgs = import (builtins.toPath {quoted_root}) {{}}; in '
                        'builtins.concatStringsSep "\\n" (map (name: pkgs.${name}.drvPath) [ "hello" "bash" "coreutils" "busybox" ])',
        }
        if args.workload:
            if unknown := set(args.workload) - workloads.keys():
                parser.error(f"unknown workloads: {', '.join(sorted(unknown))}")
            workloads = {name: expr for name, expr in workloads.items() if name in args.workload}
        report["workloads"] = {}
        for name, expression in workloads.items():
            report["workloads"][name] = measure(name, expression)
            args.output.write_text(json.dumps(report, indent=2) + "\n")
    else:
        report.update(measure("expression", expression))
    report["complete"] = True
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
