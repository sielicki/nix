#!/usr/bin/env bash

source common.sh

# Use a real configuration file; command-line options must override it even
# when the configured backend is invalid or unavailable in this build.
export NIX_USER_CONF_FILES="$TEST_ROOT/parser.conf"
unset NIX_CONFIG
writeConfig() {
    printf 'include %s\nparser = %s\n' "$test_nix_conf" "$1" > "$NIX_USER_CONF_FILES"
}
evaluate() {
    nix-instantiate --eval --strict --expr 'let f = x: x + 1; in f 41' "$@"
}

writeConfig bison
[[ $(evaluate) == 42 ]]
# The default backend remains Bison, which gates pipe operators.
expectStderr 1 nix-instantiate --experimental-features '' --eval --expr '1 |> (x: x)' \
    | grepQuiet pipe-operators

writeConfig invalid
expectStderr 1 evaluate | grepQuiet "unknown parser 'invalid'"
[[ $(evaluate --option parser bison) == 42 ]]

writeConfig lexy
if ! evaluate > "$TEST_ROOT/parser.out" 2> "$TEST_ROOT/parser.err"; then
    grepQuiet 'built without lexy parser support' < "$TEST_ROOT/parser.err"
    [[ ${NIX_TEST_REQUIRE_LEXY:-0} != 1 ]] || fail 'CI requires lexy support in this build'
    [[ $(evaluate --option parser bison) == 42 ]]
    exit 0
fi
[[ $(cat "$TEST_ROOT/parser.out") == 42 ]]
[[ $(evaluate --option parser bison) == 42 ]]
writeConfig bison
[[ $(evaluate --option parser lexy) == 42 ]]
writeConfig lexy

for expression in '6 |> (x: x * 7)' '(x: x * 7) <| 6'; do
    expectStderr 1 nix-instantiate --experimental-features '' --eval --expr "$expression" \
        | grepQuiet 'syntax error'
    [[ $(nix-instantiate --extra-experimental-features pipe-operators --eval --expr "$expression") == 42 ]]
done
[[ $(nix-instantiate --eval --expr 'with { or = 42; }; (x: x) or') == 42 ]]
[[ $(nix-instantiate --eval --expr '{ or = 42; }.or') == 42 ]]
[[ $(nix-instantiate --eval --expr '{}.missing or 42') == 42 ]]

# Warnings occur when a list element, argument or fallback is reduced. Compare
# their order as well as their spans, including whitespace after the `or` token.
for expression in \
    'with { f = x: x; or = 1; }; [ f or [ f or ] ]' \
    'with { f = x: x; or = 1; }; f f or [ f or ]' \
    'with { f = x: x; or = 1; }; [ {}.missing or f or   [ f or ] ]' \
    'with { f = x: x; or = 1; }; [ (f or) (f (f or)) ]'; do
    for backend in bison lexy; do
        nix-instantiate --option parser "$backend" --parse --expr "$expression" \
            > "$TEST_ROOT/$backend.out" 2> "$TEST_ROOT/$backend.err"
    done
    diff -u "$TEST_ROOT/bison.out" "$TEST_ROOT/lexy.out"
    diff -u "$TEST_ROOT/bison.err" "$TEST_ROOT/lexy.err"
done

# Exercise REPL bindings and continuation through the selected backend.
output=$(nix repl --extra-experimental-features nix-command 2>&1 <<'REPL'
x = 40
let y = 2; in
x + y
:q
REPL
)
echo "$output" | grepQuiet 42
echo "$output" | grepQuietInverse 'error:'

# Shared language fixtures: compare successful evaluation results. Preserve the
# fixture flags and environment from lang.sh; its golden tests cover diagnostics.
export TEST_VAR=foo
export NIX_REMOTE=dummy://
export NIX_STORE_DIR=/nix/store
for fixture in lang/eval-okay-*.nix; do
    [[ ! -e ${fixture%.nix}.exp-disabled ]] || continue
    flags=()
    if [[ -e ${fixture%.nix}.flags ]]; then
        read -r -a flags < "${fixture%.nix}.flags"
    fi
    if [[ -e ${fixture%.nix}.exp.xml ]]; then
        flags+=(--xml --no-location)
    fi
    env NIX_PATH=lang/dir3:lang/dir4 HOME=/fake-home \
        nix-instantiate "${flags[@]}" --option parser bison --extra-experimental-features pipe-operators \
        --eval --strict "$fixture" > "$TEST_ROOT/bison.out" 2> "$TEST_ROOT/bison.err"
    env NIX_PATH=lang/dir3:lang/dir4 HOME=/fake-home \
        nix-instantiate "${flags[@]}" --option parser lexy --extra-experimental-features pipe-operators \
        --eval --strict "$fixture" > "$TEST_ROOT/lexy.out" 2> "$TEST_ROOT/lexy.err"
    diff -u "$TEST_ROOT/bison.out" "$TEST_ROOT/lexy.out"
    if [[ $fixture == lang/eval-okay-deprecate-cursed-or.nix ]]; then
        # Includes warning locations, exact expression spans, and parenthesized
        # cases that must not warn. The warning machinery is shared by both.
        diff -u "$TEST_ROOT/bison.err" "$TEST_ROOT/lexy.err"
    fi
done

# Run the existing REPL regression script under lexy in its own test directory.
NIX_CONFIG='parser = lexy' TEST_NAME=parser-repl bash ./repl.sh
