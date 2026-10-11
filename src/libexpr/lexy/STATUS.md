# Lexy parser validation and performance

Lexy is an optional backend; Bison remains the default. The implementation
passes the builder and scratch allocator through lexy's parse state, uses
allocator-aware collection callbacks and public recognition rules, and bounds
pending recursive operators.

An explicit trivia production collects documentation using `fold_inplace`
and `callback_with_state`. Committed callbacks attach comments to their next
token without thread-local parsing hooks or speculative state changes. The
[implementation guide](README.md) describes these state and trivia conventions.

These measurements were collected on October 11, 2026. Their original source
and binary provenance is retained: the first two tables compare the archived
development baseline `f52a953f0f7b752781350fbb008ec56ce06b5b4d` with the public-DSL
refactor before the complete-exponent correction. Later experiments measure
the retained correction and alternative implementations. These archived
snapshots are not intermediate commits in the rewritten submission series.
Before/after columns compare lexy implementations; interleaved Bison runs expose
changes in machine conditions.

## Paired parsing measurements

The table shows CPU milliseconds per parse. Each old/new pair was measured
consecutively, alternating their order between workloads. The unchanged Bison
backend was interleaved with lexy within each executable; its change is shown to
expose machine-condition differences. Negative changes mean faster parsing.

| Workload | Before ms | After ms | Lexy change | Bison control change |
| --- | ---: | ---: | ---: | ---: |
| `strings` | 2.6754 | 2.3474 | -12.3% | -0.1% |
| `names` | 0.7332 | 0.6775 | -7.6% | -4.4% |
| `inherit` | 0.5517 | 0.5264 | -4.6% | -4.2% |
| `operators` | 0.9183 | 0.9709 | +5.7% | +11.8% |
| `plain` | 0.1618 | 0.1487 | -8.1% | +1.5% |
| `all-packages` | 4.5087 | 4.2152 | -6.5% | +0.4% |
| `doc-chain` | 0.0203 | 0.0080 | -60.6% | -0.1% |
| `deep-parentheses` | 1.0398 | 0.9778 | -6.0% | +0.6% |

Interpolated strings, `all-packages`, and documentation chains improved with
nearly unchanged Bison controls. The operator pair became slower in both
backends, with a larger slowdown in Bison; that raw timing difference cannot be
attributed entirely to this refactor. Likewise, part of the apparent gain in
names and inheritance coincides with faster Bison runs. Host activity was not
controlled, so small differences remain inconclusive and exact ratios should be
remeasured on the target machine. No end-to-end evaluation speedup is claimed.

Peak RSS for 8,192 nested parentheses rose from 76.65 to 79.18 MiB in the paired
run (2.53 MiB), despite a 6% decrease in parsing CPU time. The full-suite run
below also observed this memory increase. Other process-memory changes were
small. These samples do not establish a general reduction in memory usage.

All timings are medians of seven repetitions of 100 iterations, pinned to CPU 2
on Linux. Both builds used GCC 15.2.0, optimization level 3, debug disabled,
no LTO, and mimalloc 3.5.1. Each executable was saved with its matching Nix shared
libraries. The Nixpkgs revision was
`c3eea5b2156db11c7eeeada3dc737711255b253e`; `all-packages.nix` is 353,469 bytes.

`Parse` includes native AST construction and documentation handling. Input is
already in memory; buffer preparation, binding, evaluation, and evaluator
setup/teardown are outside the timed region. RSS samples use three fresh
processes of 20 iterations each and include setup, binding, and teardown.
They do not isolate scratch allocation or C++ stack usage. Comparisons measure
the combined change, rather than each grammar idiom separately.

## Full-suite results

The complete 35-workload suite was also measured in separate stages using the
same settings. Stage-to-stage host variation is a larger confounder than in the
paired runs above. Peak RSS is in MiB.

| Workload | Before ms | After ms | Lexy change | Bison control change | Before MiB | After MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `strings` | 2.6386 | 2.3647 | -10.4% | +1.4% | 61.67 | 61.10 |
| `attrs` | 1.0760 | 1.0165 | -5.5% | +2.0% | 60.73 | 60.40 |
| `plain` | 0.1553 | 0.1597 | +2.8% | +3.0% | 58.71 | 58.55 |
| `indented` | 0.3517 | 0.3426 | -2.6% | -3.4% | 56.52 | 56.27 |
| `names` | 0.7333 | 0.6688 | -8.8% | +1.7% | 62.73 | 62.49 |
| `paths` | 0.4883 | 0.3899 | -20.2% | +0.8% | 62.60 | 62.32 |
| `all-packages` | 4.6061 | 4.1793 | -9.3% | -1.1% | 64.99 | 64.78 |
| `small-int` | 0.0018 | 0.0017 | -6.8% | -2.5% | 54.40 | 53.97 |
| `small-string` | 0.0018 | 0.0018 | +3.0% | -7.7% | 54.30 | 54.27 |
| `small-set` | 0.0024 | 0.0021 | -15.4% | +0.7% | 54.55 | 54.20 |
| `small-function` | 0.0027 | 0.0027 | -0.7% | -8.8% | 54.59 | 54.18 |
| `integers` | 0.2267 | 0.1851 | -18.4% | -1.0% | 56.53 | 56.43 |
| `floats` | 0.2394 | 0.2168 | -9.5% | +1.1% | 58.84 | 58.14 |
| `operators` | 0.8448 | 0.8248 | -2.4% | -1.4% | 58.35 | 58.25 |
| `unary` | 0.3207 | 0.3118 | -2.8% | -2.1% | 54.35 | 54.57 |
| `has-attr` | 0.5671 | 0.5331 | -6.0% | -1.5% | 56.74 | 56.18 |
| `pipe-into` | 0.2408 | 0.2374 | -1.4% | -1.0% | 54.72 | 54.43 |
| `pipe-from` | 0.2573 | 0.2538 | -1.4% | -9.4% | 55.64 | 56.20 |
| `applications` | 0.3110 | 0.2854 | -8.2% | -1.3% | 54.50 | 54.17 |
| `selections` | 0.7840 | 0.7416 | -5.4% | +0.3% | 56.33 | 56.20 |
| `formals` | 0.9273 | 0.8454 | -8.8% | -1.7% | 58.62 | 58.11 |
| `inherit` | 0.5013 | 0.4878 | -2.7% | -4.8% | 56.81 | 56.11 |
| `dynamic-attrs` | 0.7206 | 0.5838 | -19.0% | -1.2% | 54.49 | 54.37 |
| `quoted-attrs` | 2.4262 | 2.1747 | -10.4% | -2.7% | 68.72 | 68.33 |
| `escapes` | 0.2659 | 0.2404 | -9.6% | -3.9% | 60.31 | 60.08 |
| `indented-escapes` | 3.6052 | 3.4610 | -4.0% | -4.2% | 58.30 | 58.04 |
| `line-comments` | 0.0821 | 0.0766 | -6.7% | -2.0% | 58.48 | 58.29 |
| `block-comments` | 0.0771 | 0.0792 | +2.7% | -1.0% | 58.49 | 58.06 |
| `doc-comments` | 0.3339 | 0.3053 | -8.6% | -6.0% | 60.69 | 60.30 |
| `doc-chain` | 0.0208 | 0.0090 | -56.6% | -1.2% | 56.64 | 56.25 |
| `whitespace` | 0.0968 | 0.0807 | -16.7% | +2.3% | 58.56 | 57.93 |
| `nested-parentheses` | 0.0142 | 0.0137 | -3.2% | -1.3% | 54.85 | 54.64 |
| `deep-parentheses` | 1.0487 | 0.9733 | -7.2% | -3.6% | 76.98 | 79.23 |
| `nested-sets` | 0.0228 | 0.0204 | -10.8% | -0.3% | 54.93 | 54.31 |
| `nested-lists` | 0.0054 | 0.0050 | -7.9% | -1.3% | 54.70 | 54.13 |

The [developer guide](../../../doc/manual/source/development/lexy.md) explains
how to reproduce comparisons with
[`benchmark-lexy.py`](../../../maintainers/benchmark-lexy.py). These runs use
`--suite extended --min-time=100x --repetitions 7 --cpu 2` and mimalloc preloaded;
paired runs select one workload with `--workload`. Synthetic inputs are defined
in that driver and [`parser_workloads.py`](../../../maintainers/parser_workloads.py).

Measurement provenance (the source diff was saved before final reporting and
comment-only edits; the original build's source diff is empty):

| SHA-256 | Before | After |
| --- | --- | --- |
| `git diff HEAD -- src` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `8504fd6dc89556d5b6e7ffecd9ee9d4ad8d740988f78a1679a9ad99aeac48a04` |
| Benchmark executable | `fc549d6adf2d07fb2efa3c93d0c44942aa6ad79c2bf1d0bf6e8270aa56075791` | `26cab8e2f2d8c5e13bbf799cad52ffd76575a1add99e9c622da5c56740204c65` |
| libexpr | `f1b78ee466bdbb79cf489d747faac61cbae06db2cbf1199704c2de7233dd28f0` | `dd0d7a14047effaf6fa590723f156894200a9c0ba0eaeafd1d5455c864d742bb` |

## Selected configuration after profiling

Keep the idiom refactor above, the existing optimized-build inlining settings,
and direct access to the synchronized symbol table. The selection prioritizes
throughput across ordinary Nix inputs. None of the six candidates below
established a broad improvement that justified adopting it. This selects the
lexy implementation configuration; Bison remains the default parser backend.

The following changes were tested independently against the refactored parser,
not against the clean commit used in the earlier tables. Negative percentages
mean faster parsing. Results are median paired changes from seven randomized
rounds of 150 iterations per backend, with Bison interleaved as a control.

| Experiment | `all-packages` change | Other measured tradeoff |
| --- | ---: | --- |
| Reuse native attribute-path storage within bindings | +0.0% | No clear overall runtime or peak-memory benefit |
| Combine named-lambda lookahead | -2.3% | Documentation-heavy input +7.8% |
| Bounded Boost flat-map symbol cache | +7.6% | Formals -9.5%; unique quoted attributes +12.2% |
| Single-entry symbol cache | +0.8% | Interpolated strings -6.8%; formals +9.6% |
| GCC default inlining for `lexy/parser.cc` | +5.6% | Interpolated strings +19.3%; deep parentheses -10.2% |

A final variable-only cache borrowed names from the live input and avoided
caching attributes and formal parameters. Its four-workload pilot measured
strings -4.2%, formals +2.6%, and quoted attributes +6.1%, with respective Bison
controls of +0.3%, -0.4%, and +2.0%. Its real-file pairs varied substantially
alongside host-load changes and did not establish a gain. Keep this experiment
out of the implementation as well; these results do not establish that adding
a cache improves ordinary parsing.

Default inlining reduced parser-object text from 829 to 340 KiB. Perf call
stacks and radare2 disassembly showed the observed parenthesis recursion cycle
shrinking from 3,216 to 2,672 bytes, including saved registers and return
addresses. Peak RSS at 8,192 levels fell from 79.19 to 74.80 MiB. Those savings
do not justify the ordinary-workload slowdown for the selected configuration.
The stack figures are specific to this compiler, build, and syntax; targeted
outlining of recursive productions remains a separate experiment.

Each of the six candidates passed 64 focused unit tests and 7,634 differential
cases covering language fixtures and generated syntax/trivia boundaries.
Peak RSS used three fresh processes of 20 iterations each, including evaluator
setup, binding, and teardown. Host activity was not controlled, and small
runtime or sub-MiB RSS differences should not be treated as established wins.

## Upstream performance feature experiments

This round retained complete-token matching for optional floating-point
exponents. Buffering, inline recognition helpers, a numeric `scan_production`,
and `delimited` strings remain experiments. The dependency remains pinned to
upstream `c1358c41`; additional performance patches in the local lexy fork were
not included in these measurements.

The exponent change uses `token` around the entire exponent before the optional
branch commits. The expanded boundary corpus found 192 existing acceptance or
AST differences: for example, Flex tokenizes `[0.0e]` as a float and an identifier,
while the old lexy rule rejected it. The new rule fixes those differences.
The full float lookahead remains: removing it or replacing number parsing with
`scan_production` did not establish a better overall result.

Two seven-round comparisons measured the retained change at -3.9% and -4.3%
CPU time on the floating-point workload. Other workloads varied across runs;
there is no established broad speedup. Parser-object text falls from 849,142 to
848,899 bytes (243 bytes). The change adds no input copy or scratch allocation,
and RSS measurements do not establish a memory saving.

The table below is the last comparison with the selected idiom-refactor baseline
from the previous round. Negative CPU changes mean faster parsing. Each row
uses seven randomized rounds of 150 iterations per backend, pinned to CPU 2,
with Bison interleaved as a control. Percentages are medians of per-round ratios,
not ratios of independently calculated medians. Evaluator setup and binding
are excluded from CPU time but included in peak RSS, which uses three fresh
processes of 20 iterations each. All candidates share the original build flags,
GCC 15.2, optimization level 3, no LTO, and mimalloc 3.5.1.

| Workload | Retained change: Lexy CPU | Bison control | Peak RSS change (MiB) |
| --- | ---: | ---: | ---: |
| `integers` | +0.5% | -4.5% | +0.21 |
| `floats` | -4.3% | +5.4% | +0.12 |
| `all-packages` | +0.3% | +3.6% | -0.04 |
| `real-modules` | +1.8% | -1.4% | +0.25 |
| `real-strings` | -1.5% | -0.4% | -0.02 |
| `real-types` | +0.4% | +7.0% | -0.08 |
| `strings` | -4.7% | -2.1% | -0.27 |
| `escapes` | +3.9% | +5.1% | -0.24 |
| `nonascii` | +9.3% | +1.5% | -0.05 |
| `small-int` | +2.2% | +8.0% | -0.38 |
| `small-set` | +7.8% | -2.2% | +0.21 |
| `small-function` | +3.1% | -9.8% | -0.16 |

These are local measurements on a shared host. Bison controls and individual
pairs show host variability; small changes are inconclusive. Tiny expressions
run in a few microseconds and have particularly noisy ratios. The exponent
change is retained primarily to correct token boundaries, with a repeatable
improvement on the float workload. No end-to-end evaluation speedup is claimed.

### Buffered input experiment

The strongest buffering candidate copied inputs of at least 1 KiB into a
`lexy::buffer<utf8_char_encoding>` using the caller's scratch resource. A
separate matching production validated UTF-8 before running the Nix grammar.
It used `identifier(ascii::character).pattern()` to scan ASCII runs with SWAR
and `code_point` for intervening non-ASCII sequences. A generic `while_` over a
character class does not itself use its SWAR matcher. Byte `0xFF` selected the
original byte reader before validation, since it is lexy's UTF-8 EOF sentinel;
invalid UTF-8 also fell back to that reader. Native positions were calculated
relative to the active input, and borrowed spans were copied or interned before
the owned buffer was released. Buffer allocation, copying, and validation were
included in the measured parse time.

Unconditional buffering slowed small integer, set, and function workloads by
about 10–15% in an earlier seven-round comparison. A five-round size sweep at
64, 256, 1024, and 4096 bytes motivated the 1 KiB cutoff. However, the final
production build still had unfavorable small-expression timings despite
retaining the byte path for those inputs:

| Workload | Buffered candidate: Lexy CPU | Bison control | Peak RSS change (MiB) |
| --- | ---: | ---: | ---: |
| `all-packages` | -7.2% | +0.6% | +0.27 |
| `real-modules` | -5.5% | +0.2% | +0.35 |
| `real-strings` | -4.8% | -1.1% | +0.38 |
| `real-types` | -6.8% | +1.3% | +0.42 |
| `nonascii` | +6.1% | -0.1% | +0.33 |
| `small-int` | +9.0% | -11.4% | +0.49 |
| `small-set` | +25.6% | +4.3% | +0.89 |
| `small-function` | +38.8% | -1.6% | +0.37 |

The real-file improvements do not justify this default for the current parser.
Parser-object text grows to 1,563,064 bytes (+84.1%, approximately 697 KiB) because
both readers instantiate the grammar. A buffered parse also needs one extra
copy of the input plus sentinel padding; invalid UTF-8 without `0xFF` incurs
that copy before falling back. RSS was higher in every final sampled workload.
Dense non-ASCII text was less favorable to prevalidation. The timings do not
isolate why tiny inputs regressed, and do not prove that code size caused it.
The byte reader therefore remains the sole production input path.

An additional source-level constraint explains why buffering is not a general
vectorization switch: our composed identifier-tail, path, and trivia character
classes return lexy's unsupported-SWAR marker. The standard `ascii::word` class
has a matcher, but its union with the extra Nix identifier characters does not.
Simply switching to `buffer<byte_encoding>` does not enable SWAR either.

### Other candidates

| Experiment | Relevant result |
| --- | --- |
| Inline recognition helpers | Pilot: strings +5.4%, formals +5.0%; negligible code-size reduction |
| Remove outer float lookahead, with exponent fix | Seven rounds: floats -3.4%, real strings +3.3%; no consistent overall advantage |
| Numeric `scan_production` | Seven rounds: integers +4.7%, real strings +5.7% |
| ASCII-only buffer | Most representative real files contain non-ASCII comments and use the fallback; pilot long literals +9.1% |
| Coalescing `delimited` strings with byte input | Pilot escapes +6.5%; no established broad benefit |
| ASCII buffer plus coalescing `delimited` | Pilot long literals -17.4%, escapes +54.4% |
| UTF-8 buffer with scalar validation | Pilot long literals +28.0%, dense non-ASCII +24.4%; replaced by token-pattern validation in later experiments |

Pilots used three rounds of 50 iterations with the same interleaved controls.
The string experiments preserved one decoded span between interpolations and
were adjusted to retain REPL incomplete-input behavior. Tested grammar variants
passed the existing parser/trivia/language cases. Numeric fixes also cleared
the newly discovered exponent differences.

The retained implementation passes 461 enabled libexpr unit tests (one existing
disabled test), the functional parser test, and 54,303 differential cases:
43,345 Nixpkgs files, 4,292 existing boundary cases, 2,840 trivia cases, 502 language
fixtures, 2,084 new numeric/string boundaries, and 1,240 UTF-8/byte boundaries.
There are no acceptance or native-AST differences. Differential checking includes
selected source positions and documentation but excludes error text and operator
and call diagnostic positions; unit tests separately cover REPL errors and input
lifetime. The buffered candidate also passed these correctness checks.

Local sources, patches, build commands, logs, and raw measurements are retained
under `.git/experiments/lexy-doc-review` (also linked from
`/tmp/nix-lexy-doc-review`). The consolidated report is `results.json`.
The last comparison's raw label `selected` denotes the rejected buffered build,
now archived as `buffer-final`; `final-selected` contains the retained build.
The exponent-only benchmark and retained build have identical parser machine
code. The retained libexpr SHA-256 is:
`97d0ceb1904db7cb7535812c0658ad4673f3d8bcc923c87469b6d9f515cb326f`.

## Construction and allocation choices

- Grammar callbacks construct native libexpr expressions without an intermediate
  expression tree or a separate conversion pass.
- Names and unchanged literal spans borrow input until interned or copied into
  native storage. Plain quoted attribute names avoid a discarded string node.
- Native PMR cursors are reused across files on each parsing thread. Scratch
  buffers and string assembly vectors are reused within a parse. The injected
  construction allocator supplies temporary storage; returned AST handles
  borrow roots owned by `Exprs`.
- Lexy's expression productions avoid descending through every precedence level
  for a simple operand. Application and pipe folds construct native calls as
  operands complete.
- Trivia retains only the last documentation span in a run, without allocating
  a list of comments. Its committed callback associates that span with the next
  token before bindings and lambdas are constructed.

## Grammar compilation cost

An earlier compilation sample from commit
`3c36f7a0f3319648dc40ab382538648903fbec4b` measured `lexy/parser.cc` at 80.34 s
CPU time, 80.39 s wall time, and 3.01 GiB peak compiler RSS with the optimized
build's compiler and flags. This predates the idiom refactor; compilation cost
was not remeasured for this change. Native builder files do not instantiate
the grammar. The translation unit instantiates pipes off/on, both with legacy
`f or` enabled; enabling all four language-option combinations would require
another measurement.

## Differential and interface validation

The updated `nix-parser-check` executable was run through
`maintainers/check-parser-corpus.py`, with pipe operators enabled:

| Corpus | Files | Accepted by both | Differences |
| --- | ---: | ---: | ---: |
| Nixpkgs at the revision above | 43,345 | 43,345 | 0 |
| `tests/functional/lang` | 502 | 455 | 0 |
| Generated boundaries, seed 20261010, 3,000 random strings | 4,292 | 3,784 | 0 |
| Additional trivia boundaries, including truncated comments | 2,840 | 2,445 | 0 |

The additional corpus inserts documentation, ordinary block comments, line
comments, and whitespace at individual or all boundaries of fixed token
sequences covering collections, functions, bindings, operators, and attributes.
It also includes unterminated comments. Its dot/comment cases reproduced the
original branch's rejection before the shared dot recognizer was corrected;
the typed unit tests now retain that regression.

Rejected inputs are included. The checker compares acceptance and normalized
native trees, including selected positions and documentation; it excludes
error text, operator and call diagnostic positions, and legacy `or` markers.
Passing these bounded corpora does not prove equivalence for every input.

The Nixpkgs corpus SHA-256 was
`9b72630c560b0212095fe153905cdb014a309ddd8f2992d8df9f53b9ed2efd1e`;
the checker executable SHA-256 was
`6c09746968dfcd0644eba5e8f98c508fe540e2b323fa42ae4386c25d3c09dfcc`.

The Bison-only test commit was built and tested before the lexy dependency or
grammar was introduced. All 424 enabled libexpr tests passed at that stage,
including the 31 shared parser cases; the preceding API commit has identical
production sources. The testing branch changes only the typed test source and
its Meson registration, establishing the existing backend's contract separately.

All 461 enabled libexpr unit tests passed; one pre-existing test is disabled.
The parser and allocator subset contains 68 tests, including 31 generic cases
for each backend, two lexy nesting tests, and four allocator tests. These cover
values and diagnostics, documentation attachment across lookahead, false comment
markers inside strings and other comments, trivia at token boundaries,
borrowed-root ownership, temporary-allocator cleanup, and AST lifetime after
allocator destruction. Long flat chains include mixed precedence and completed
unary operands, so recursion protection does not impose a total operator limit.

The previously crashing input of 500,000 `! ` prefixes now returns a complete
operator-nesting error from lexy. Tests cover six prefix/right-associative
operator forms, completed and truncated inputs, both parser entry points, and
successful parsing after errors. Grammar nesting and pending operators are
bounded separately; the C++ call stack still imposes platform, build, and
thread-dependent limits. This does not establish unrestricted nesting parity
with Bison's heap stack.

The functional parser script passed, including shared evaluation fixtures,
REPL tests, feature gating, and exact legacy-`or` warning spans and ordering.
