# Experimental lexy parser

The `lexy` development shell includes a pinned checkout of
[foonathan/lexy](https://github.com/foonathan/lexy), the C++ parsing library.
This is a different project from the package named `lexy` in Nixpkgs.

```console
$ nix develop .#lexy
[nix-shell]$ configurePhase
[nix-shell]$ buildPhase
[nix-shell]$ build/src/nix/nix --option parser lexy eval --expr 'let f = x: x + 1; in f 41'
42
```

For an existing Meson build, make the `foonathan-lexy` pkg-config dependency
available and enable `-Dlibexpr:lexy=enabled`. Standalone libexpr builds use
`-Dlexy=enabled`. The `nix-lexy` flake package also builds the CLI with this support.
Both parsers are built when this option is enabled; Bison remains the default.
The default build disables lexy and needs no lexy dependency.

Select the backend in `nix.conf`:

```ini
parser = lexy
```

`--option parser bison` or `--option parser lexy` overrides that configuration
for one command. Unknown backend names and selecting lexy in a build without
it produce an error when parsing an expression. There is no automatic fallback.

Both backends use the same language settings:

- Pipe operators (`|>` and `<|`) require the `pipe-operators` experimental feature.
- The legacy use of `or` as an argument in `f or` is accepted, including Bison's
  deprecation warnings in ambiguous contexts. Attribute defaults
  (`attrs.name or fallback`) and attributes named `or` remain supported.

Syntax error messages can differ. Lexy syntax errors reported at EOF are marked
as incomplete so the REPL can request more input, except at a nesting bound.
Semantic errors are reported immediately.

The backend constructs libexpr's native expressions in grammar callbacks and
uses the evaluator's existing binding, attribute, and path machinery. It supports
REPL bindings and documentation comments as well as expressions.

Deeply nested expressions still depend on the C++ call stack, a grammar
recursion bound, and a separate bound on pending prefix and right-associative
operators. Long flat operator chains do not consume the pending-operator bound.
Their nesting capacity varies with the platform, build, and
calling thread; it is not equivalent to Bison's growing heap stack. This is an
implementation limitation. The implementation guide describes the nesting regression tests.

The `ParserTest/*` typed unit tests run the same result, error, lifetime and
language checks against every configured backend through `ParserBackend`.
Bison supplies the initial implementation; enabling lexy adds its type to the
same suite. The allocator checks cover temporary-storage release and native
AST survival. The lexy-specific grammar and operator nesting tests are separate.

The unit-test build includes `nix-parser-check` without requiring benchmarks.
It and the corpus driver compare every
`.nix` file in a checkout without binding variables or evaluating it:

```console
[nix-shell]$ ninja -C build src/libexpr-tests/nix-parser-check
[nix-shell]$ python maintainers/check-parser-corpus.py /path/to/nixpkgs --output parser-corpus.json
```

The checker compares acceptance and native AST structure, including exact
floating-point values, interpolation coercion, attribute/formal/function
positions, and attribute/function documentation. It folds adjacent constant string
fragments because lexers can split these differently. Error text, operator and
call diagnostic positions, and legacy `or` markers are excluded. The report includes a digest of all source
paths and contents; a companion JSONL file records the result for every file.
The checker is strict: no acceptance or AST differences are expected, including
legacy `f or` expressions. Differences cause a nonzero exit status. Crashes, timeouts, and malformed
checker output also count as failures; failed batches are retried one file at a
time. `--timeout` bounds each invocation (60 seconds by default). The per-file
journal is flushed as batches finish, so interrupted runs retain those results.
The report filename must not end in `.jsonl`, which is reserved for that journal.
The checker enables pipe operators for both backends before comparing trees.
The backend tests separately exercise both feature settings, including nested
expressions and REPL bindings.

## Comparing parser performance

Use an optimized build with both backends enabled. After configuring in
`nix develop .#lexy`, enable the existing libexpr benchmark target:

```console
[nix-shell]$ meson configure build -Doptimization=3 -Ddebug=false -Dlibexpr-tests:benchmarks=true
[nix-shell]$ ninja -C build src/libexpr-tests/nix-expr-benchmarks src/nix/nix
[nix-shell]$ build/src/libexpr-tests/nix-expr-benchmarks --benchmark_filter='^Parse(AndBind)?/' --benchmark_min_time=100x --benchmark_repetitions=10 --benchmark_enable_random_interleaving=true --benchmark_display_aggregates_only=true --benchmark_out=parser-results.json --benchmark_out_format=json
```

`Parse` measures parsing, native expression construction, and documentation
handling. Buffer preparation and variable binding
are outside its timed region.

`ParseAndBind` measures `EvalState::parseExprFromString`: copying the input,
parsing, constructing native expressions in grammar callbacks for lexy, and
binding variables. It excludes file I/O, evaluator setup and teardown,
evaluation, and printing. Each iteration uses a fresh evaluator to keep retained
ASTs and symbol tables from growing. Generated attribute sets and interpolated
strings cover 100, 1,000, and 10,000 entries. Compare the median CPU time and
bytes/second of matching `/bison` and `/lexy` cases.

To use a real file, load it into memory once before measurement:

```console
[nix-shell]$ NIX_PARSER_BENCH_FILE=/path/to/nixpkgs/pkgs/top-level/all-packages.nix build/src/libexpr-tests/nix-expr-benchmarks --benchmark_filter='^ParseAndBind/file/' --benchmark_min_time=100x --benchmark_repetitions=10 --benchmark_enable_random_interleaving=true --benchmark_report_aggregates_only=true
```

The file must parse and bind as a standalone expression; imports are not
evaluated. Pipe operators are enabled, as in the libexpr unit tests.

Also measure a representative evaluation using the same executable, expression,
and store for each backend, with the evaluation cache disabled. The Unix driver
verifies equal results, warms both commands once, alternates their order, and
records wall time, CPU time, and each process's peak resident memory:

```console
[nix-shell]$ python maintainers/benchmark-parsers.py /path/to/nixpkgs --nix build/src/nix/nix --runs 10 --output parser-evaluation.json
```

Report elapsed time and maximum resident memory alongside the parser benchmark;
these process measurements include evaluator setup, imports, evaluation, and
store work. Record the source revisions,
compiler and build flags, CPU, and cache conditions. Do not use an unoptimized
validation build to draw performance conclusions.

## Working on the lexy implementation

The [implementation guide](https://github.com/NixOS/nix/blob/master/src/libexpr/lexy/README.md)
describes the file layout, grammar conventions, and allocation lifetimes. Grammar
callbacks construct native expressions directly; scratch storage is released at
the end of each parse, while native PMR cursors are reused across files on each
parsing thread. Source ownership and evaluator interfaces remain unchanged.

For a new language case, update the relevant production or native builder and
add a focused case in `src/libexpr-tests/parser.cc`. Rebuild the parser tests and
corpus checker, then use the comparisons above to verify the language fixtures
and pinned Nixpkgs snapshot. Structural refactors should preserve grammar rules,
accepted/rejected inputs, native ASTs, source positions, and attached
documentation. Store generated benchmark/profile reports outside the source
tree; measurements should record their source revision and allocator.

Generate reproducible string, path, and operator cases, including truncated input,
to supplement the real-file corpus:

```console
$ python3 maintainers/generate-parser-boundaries.py /tmp/parser-boundaries
$ python3 maintainers/check-parser-corpus.py /tmp/parser-boundaries --checker build/src/libexpr-tests/nix-parser-check --output boundaries.json
```

The generator requires an empty directory and defaults to seed `20261010` and
3,000 randomized string cases, plus path, escape, and operator regression cases.
The operator cases compare precedence, prefix operands, pipe directions, and
chained attribute tests without requiring the expressions to type-check. Change
`--seed` or `--count` to explore further boundaries. A passing finite corpus does
not establish equivalence for every input.

## Profiling parser CPU and memory use

For successive optimization commits, `maintainers/benchmark-lexy.py` measures
the same synthetic strings, attributes, long names, paths, and Nixpkgs
`all-packages.nix` with both backends. For example:

```console
$ python3 maintainers/benchmark-lexy.py /path/to/nixpkgs --stage baseline --output baseline.json --allocator /path/to/libmimalloc.so --cpu 2
```

The report retains individual CPU samples, their median and variation, and peak
RSS from separate fixed-iteration processes. RSS includes setup, binding, and
teardown; it is not a measurement of the temporary arena alone. Bison runs are
interleaved with lexy to expose machine-load changes. Compare each stage against
its predecessor and the initial baseline; small differences within the sample
variation are inconclusive. Record the same allocator and build options throughout.

Use `--suite extended` to include small expressions, numeric lists, operators
(including unary, attribute-test, and long pipe chains),
applications, selections, formal defaults, inheritance, dynamic and quoted attributes,
escape-heavy strings, comments, whitespace, and nested collections.
`--mode ParseAndBind` includes buffer copying and variable binding.
The default is 100 fixed iterations per sample (`--min-time=100x`);
inputs smaller than 4 KiB always use a fixed count (`--small-iterations`).
Automatic time-based calibration can construct too many fresh evaluators for a
microsecond parse and retain substantial memory. The driver stops
a timing process that exceeds 1 GiB RSS or five minutes.
Treat small timing differences cautiously:
timer suspension and resumption have proportionally more influence here.

The evaluation driver also accepts `--suite extended`: it compares process
startup, importing the Nixpkgs library, evaluating a module, enumerating package names, `hello.drvPath`,
and the derivation paths of four packages. `--cpu 2` pins the child processes,
and `--expression` selects a custom expression that returns a string.
Use `--suite nixpkgs` for larger dependency graphs, defined in
`maintainers/evaluation_workloads.py`:

| Workload | Forced evaluation |
| --- | --- |
| `package-set` | Derivation paths for 30 native packages, including compilers, browsers, graphics tools, databases, and virtualization tools. |
| `python-environment` | A Python environment with NumPy, pandas, SciPy, matplotlib, JupyterLab, scikit-learn, and supporting packages. |
| `cross-packages` | Six packages and their dependencies cross-compiled for `aarch64-multiplatform`. |
| `nixos-server` | A complete NixOS system with SSH, nginx, PostgreSQL, Redis, and a Prometheus node exporter. |
| `nixos-desktop` | A complete NixOS GNOME system with GDM, NetworkManager, PipeWire, Firefox, LibreOffice, and GIMP. |

These evaluate derivation paths, including `system.build.toplevel`, without
building their outputs. Import-from-derivation is disabled for every evaluation
benchmark. The NixOS configurations use fixed device names as configuration data
and do not inspect or modify host devices. The larger suite defaults to
`--system x86_64-linux`; its module options and package names must be compatible
with the chosen Nixpkgs revision. Use repeatable `--workload NAME` options to
select cases from either named suite. Larger evaluations can take seconds and
use substantially more memory per process; measure them separately from builds
or other benchmarks.

Each evaluation workload checks equal results before recording alternating
fresh-process samples. The parse benchmark validates that its input parses and
binds; use the differential checker for AST comparisons. Reports retain the
executable hash, source revision and uncommitted source changes' hash, plus a
companion libexpr hash when found. The library lookup does not inspect the dynamic
loader; ensure that the recorded library matches the runtime configuration.
Both drivers save completed workloads
incrementally so an interrupted run retains its measurements; `complete: true`
marks a finished report.
File hashes are streamed to bound the driver's own memory use before it launches
benchmark processes. Use `--build-dir` to select a Meson build
directory (default: `build`), or
`--binary` / `--nix` to select an executable. CPU affinity is optional; `--cpu`
must identify a CPU available to the current Linux process.

```console
$ python3 maintainers/benchmark-lexy.py /path/to/nixpkgs --suite extended --stage baseline --output parse-baseline.json --allocator /path/to/libmimalloc.so --cpu 2
$ python3 maintainers/benchmark-parsers.py /path/to/nixpkgs --suite extended --runs 10 --cpu 2 --output evaluation-baseline.json
$ python3 maintainers/benchmark-parsers.py /path/to/nixpkgs --suite nixpkgs --runs 7 --cpu 2 --output evaluation-nixpkgs.json
```

Compare parsing CPU time, evaluation CPU/wall time, and peak RSS separately.
Passing a finite suite does not establish superiority for every Nix input, and
none of these timing measurements evaluates the quality of error diagnostics.

On Linux, collect a CPU profile for one backend and workload at a time:

```console
$ perf record -e cycles:u -F 499 --call-graph dwarf,16384 -o parser.data -- taskset -c 2 build/src/libexpr-tests/nix-expr-benchmarks --benchmark_filter='^Parse/strings/10000/lexy$' --benchmark_min_time=100x
$ perf report -i parser.data --stdio --no-inline --no-children -g none
```

Choose an available CPU for `taskset`. Repeat with `/bison`, or set
`NIX_PARSER_BENCH_FILE` and select `^Parse/file/lexy$`. The process-wide profile
also samples setup, binding and teardown, even when the benchmark timer is
paused. Use the call stacks to distinguish these from parsing. Keep unprofiled
timing results separate from the instrumented runs. The fixed iteration count
also bounds evaluator setup allocations; repeat fresh processes to collect more
samples instead of relying on time-based calibration.

Use a fixed iteration count when comparing hardware counters and page faults:

```console
$ perf stat -r 3 -e cycles:u,instructions:u,cache-references:u,cache-misses:u,minor-faults -- taskset -c 2 build/src/libexpr-tests/nix-expr-benchmarks --benchmark_filter='^Parse/strings/10000/lexy$' --benchmark_min_time=100x
```

Record the allocator as well as the compiler flags. The CLI can link mimalloc
while the benchmark executable uses the system allocator. Check their dynamic
dependencies with `ldd`; to test the benchmark with the CLI's allocator, prefix
its command with `env LD_PRELOAD=/path/to/libmimalloc.so`. This can change both
page faults and retained memory substantially. Report peak RSS and user/system
CPU time alongside the timing result.

An allocation profiler such as heaptrack complements perf: CPU or memory-access
samples do not measure live heap size. Heaptrack can attribute the temporary
PMR arena's upstream blocks, but each block contains many suballocations, and
its growth stack does not identify every object's size. Native AST arenas use
anonymous mappings that ordinary heap interception does not capture. Also
verify that the allocator's C++ allocation entry points are intercepted before
using heaptrack results; mimalloc can bypass its default malloc interception.
