#!/usr/bin/env python3
"""Compare both libexpr parsers over every .nix file in a source tree."""

import argparse
import concurrent.futures
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import time


def check_batch(checker, batch, timeout):
    """Isolate failed batches so one crashed or hung parse cannot hide other files."""
    try:
        result = subprocess.run([str(checker), *map(str, batch)], capture_output=True,
                                text=True, errors="replace", timeout=timeout)
        reason = f"checker exited with status {result.returncode}"
        if result.returncode in (0, 1):
            try:
                rows = [json.loads(line) for line in result.stdout.splitlines()]
                valid = (len(rows) == len(batch)
                         and all(isinstance(row, dict) and row.get("file") == str(path)
                                 and isinstance(row.get("match"), bool)
                                 and isinstance(row.get("accepted"), bool)
                                 for row, path in zip(rows, batch)))
                if valid and result.returncode == int(any(not row["match"] for row in rows)):
                    return rows
            except json.JSONDecodeError:
                pass
            reason = "invalid or incomplete checker output"
        failure = {"fatal": reason, "exit_code": result.returncode,
                   "stderr": result.stderr[-4096:]}
    except subprocess.TimeoutExpired:
        failure = {"fatal": f"checker exceeded {timeout:g} seconds", "timeout_seconds": timeout}
    except OSError as error:
        # Record launch errors for the whole batch without retrying each input.
        return [{"file": str(path), "match": False, "fatal": str(error)} for path in batch]
    if len(batch) > 1:
        return [row for path in batch for row in check_batch(checker, [path], timeout)]
    return [{"file": str(batch[0]), "match": False, **failure}]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--checker", type=Path, default=Path("build/src/libexpr-tests/nix-parser-check"))
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--timeout", type=float, default=60,
                        help="seconds per checker invocation, including single-file retries")
    parser.add_argument("--output", type=Path, default=Path("parser-corpus.json"))
    args = parser.parse_args()
    if args.jobs < 1 or args.batch_size < 1:
        parser.error("jobs and batch-size must be positive")
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("timeout must be a finite positive number")
    root, checker = args.root.resolve(), args.checker.resolve()
    if not checker.is_file() or not os.access(checker, os.X_OK):
        parser.error(f"checker is not an executable file: {checker}")
    output = args.output.resolve()
    journal_path = output.with_suffix(".jsonl")
    if output == journal_path:
        parser.error("the .jsonl suffix is reserved for the per-file journal")
    paths = sorted(path for path in root.rglob("*.nix") if path.is_file())
    if not paths:
        parser.error("the source tree contains no .nix files")
    if output in paths or output == checker:
        parser.error("output must not overwrite an input file or the checker")
    output.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    for path in paths:
        digest.update(str(path.relative_to(root)).encode() + b"\0")
        digest.update(hashlib.sha256(path.read_bytes()).digest())

    started = time.monotonic()
    rows = []
    differences = 0
    batches = [paths[i:i + args.batch_size] for i in range(0, len(paths), args.batch_size)]
    with journal_path.open("w") as journal, concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        pending = [pool.submit(check_batch, checker, batch, args.timeout) for batch in batches]
        for future in concurrent.futures.as_completed(pending):
            batch = future.result()
            rows.extend(batch)
            differences += sum(not row["match"] for row in batch)
            for row in batch:
                journal.write(json.dumps(row) + "\n")
            journal.flush()
            print(f"{len(rows)}/{len(paths)} files, {differences} differences", flush=True)
    rows.sort(key=lambda row: row["file"])
    failures = [row for row in rows if not row["match"]]
    report = {
        "root": str(root), "checker": str(checker), "corpus_sha256": digest.hexdigest(),
        "checker_sha256": hashlib.sha256(checker.read_bytes()).hexdigest(),
        "timeout_seconds": args.timeout, "journal": str(journal_path),
        "files": len(rows), "accepted": sum(row.get("accepted", False) for row in rows),
        "expected_differences": [],
        "rejected": [row for row in rows if row.get("accepted") is False],
        "differences": failures, "seconds": time.monotonic() - started,
        "comparison": "raw parse acceptance and native AST structure, folding adjacent literal string fragments; includes attribute/formal/lambda positions and attribute/lambda documentation; excludes error text and operator diagnostic positions",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Report: {args.output}; {len(failures)} differences")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
