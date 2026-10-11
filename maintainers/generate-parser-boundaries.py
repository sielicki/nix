#!/usr/bin/env python3
"""Generate reproducible string, path, and operator boundary cases for check-parser-corpus.py.

Both valid and deliberately truncated inputs matter: the checker compares raw
acceptance, normalized native ASTs, selected positions, and documentation. This
is a bounded differential corpus, not a proof of equivalence or a timing suite.
"""

import argparse
from itertools import product
from pathlib import Path
import random


def sources(count, seed):
    quoted = [
        "a", r"\n", r"\t", r"\r", r'\"', r"\\", r"\$", "$", "$$",
        '${"x"}', "\r", "\r\n", "\xff", "\\\xff", "\\\r", "\\\r\n",
        "{", "}", r"\q", "\\\n", " ", "\t",
    ]
    indented = [
        "a", "'''", "''$", "''\\n", "''\\t", "''\\r", "''\\ ",
        "$", "$$", '${"x"}', "\r\n", "\xff", "''\\\xff", "  ",
        "\n", "'", "{", "}", "\t", "''\\\n", "''\\\r",
    ]
    rng = random.Random(seed)
    for i in range(count):
        delimiter, pieces = ('"', quoted) if i % 2 == 0 else ("''", indented)
        source = delimiter + "".join(rng.choices(pieces, k=rng.randrange(1, 25))) + delimiter
        if i % 11 == 0:
            source = source[:-rng.randrange(1, min(7, len(source)) + 1)]
        yield f"string-{i:05}.nix", source

    prefixes = ['./a', '../a', '/a', 'a/b', './${"a"}', '/a${"b"}']
    bodies = ['/', '//', '///', '/b/', '/b//', '${"b"}/', '/${"b"}//']
    suffixes = ['', 'c', '${"c"}', '${"c"}/d', ' c']
    for i, (prefix, body, suffix) in enumerate(product(prefixes, bodies, suffixes)):
        yield f"path-{i:05}.nix", prefix + body + suffix

    # Cross precedence boundaries in both directions. Some syntactically valid
    # cases fail at evaluation; the checker deliberately compares raw trees.
    operators = ["+", "-", "*", "/", "++", "//", "<", "<=", ">", ">=",
                 "==", "!=", "&&", "||", "->", "|>", "<|"]
    cases = set()
    for left, right in product(operators, repeat=2):
        cases.update([f"1 {left} 2 {right} 3", f"(1 {left} 2) {right} 3",
                      f"1 {left} (2 {right} 3)"])
    for operator, prefix in product(operators, ["!", "-", "!!", "--", "!-", "-!"]):
        cases.update([f"{prefix} 1 {operator} 2", f"1 {operator} {prefix} 2"])
    cases.update(["{} ? a ? b", "!{} ? a", "-{} ? a", "{} ? a + 1", "{} ? a == 1",
                  "1 + {} ? a", "-!1", "!-1", "!!1", "--1"])
    for i, source in enumerate(sorted(cases)):
        yield f"operator-{i:05}.nix", source

    # Adjacent non-indenting tokens must remain separate until final-line trim.
    yield "indented-trailing-space.nix", "''''\\n''\\ ''"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="empty directory for generated .nix files")
    parser.add_argument("--count", type=int, default=3000, help="number of randomized string cases")
    parser.add_argument("--seed", type=int, default=20261010)
    args = parser.parse_args()
    if args.count < 1:
        parser.error("count must be positive")
    if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
        parser.error("output directory must be empty")
    args.output.mkdir(parents=True, exist_ok=True)
    for count, (name, source) in enumerate(sources(args.count, args.seed), 1):
        (args.output / name).write_bytes(source.encode("latin-1"))
    print(f"Wrote {count} cases to {args.output}; seed={args.seed}")


if __name__ == "__main__":
    main()
