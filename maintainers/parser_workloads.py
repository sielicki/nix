"""Deterministic, independently bindable inputs for parser comparisons."""


def extended_sources():
    def listed(items):
        return "[\n" + "\n".join(items) + "\n]"

    return {
        "small-int": "42",
        "small-string": '"hello"',
        "small-set": '{ name = "hello"; version = "1.0"; }',
        "small-function": "{ x ? 1, y ? 2, ... }: x + y",
        "integers": listed(str(i * 123457) for i in range(4096)),
        "floats": listed(f"{i + 1}.125e-3" for i in range(4096)),
        "operators": listed("(1 + 2 * 3 - 4 / 2 < 9 && true || false)" for _ in range(1000)),
        "unary": listed("(!false && !true || false)" for _ in range(1000)),
        "has-attr": listed("({ a.b = 42; } ? a.b)" for _ in range(1000)),
        "pipe-into": "let f = x: x; in 42" + " |> f" * 2000,
        "pipe-from": "let f = x: x; in " + "f <| " * 2000 + "42",
        "applications": "let f = a: b: c: a + b + c; in " + listed("(f 1 2 3)" for _ in range(1000)),
        "selections": listed("({ a.b.c = 42; }.a.b.c or 0)" for _ in range(1000)),
        "formals": listed("({ a ? 1, b ? a, c ? b, ... }@args: args.a or c)" for _ in range(1000)),
        "inherit": "let x = { a = 1; b = 2; c = 3; }; in " + listed("{ inherit (x) a b c; }" for _ in range(1000)),
        "dynamic-attrs": "let name = \"field\"; in " + listed('{ ${name} = 42; "quoted name" = 1; }' for _ in range(1000)),
        "quoted-attrs": "{\n" + "".join(f'"quoted attribute name {i}" = {i};\n' for i in range(10000)) + "}",
        "escapes": listed('"' + r"a\nb\tc\r\"d\\e\${literal}" * 8 + '"' for _ in range(1000)),
        "indented-escapes": listed("''" + r"a''\nb''\tc''\rd'''e''${literal}" * 8 + "''" for _ in range(1000)),
        "line-comments": "[\n" + ("# " + "ordinary comment " * 8 + "\n42\n") * 1000 + "]",
        "block-comments": "[\n" + ("/* " + "ordinary comment " * 8 + "*/ 42\n") * 1000 + "]",
        "doc-comments": "{\n" + "".join(f"/** Documentation for attribute {i}. */ a{i} = x: x;\n" for i in range(1000)) + "}",
        "doc-chain": "/** Earlier documentation. */\n" * 1000 + "/** Attached documentation. */ x: x",
        "whitespace": listed(" " * 128 + "42\t\r\n" for _ in range(1000)),
        "nested-parentheses": "(" * 128 + "42" + ")" * 128,
        "deep-parentheses": "(" * 8192 + "42" + ")" * 8192,
        "nested-sets": "{ a = " * 64 + "42" + "; }" * 64,
        "nested-lists": "[ " * 64 + "42" + " ]" * 64,
    }
