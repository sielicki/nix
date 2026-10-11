# Lexy parser implementation

This is the optional [foonathan/lexy](https://github.com/foonathan/lexy) backend
for libexpr. Build and comparison commands are documented in the
[developer guide](../../../doc/manual/source/development/lexy.md).
See [STATUS.md](STATUS.md) for measurements and their limits.

Start with `../include/nix/expr/parser.hh`, which defines `ParserBackendTraits`,
`ParserBackend` and the borrowed `ParsedAst` handle. The traits name expression,
REPL-binding, error and construction-allocator types. The backend operations
return `std::expected`; the handles identify non-null roots and their `Exprs`
owner without extending its lifetime. `../parser.cc` selects a backend and
translates returned parse errors into exceptions for existing callers.

Backends use `std::make_obj_using_allocator` construction. Bison is allocator-
unaware; lexy accepts the allocator for temporary parsing storage. Native AST
allocation remains owned by `Exprs`, independently of that temporary resource.

`parser.cc` in this directory is the only translation unit that
instantiates lexy. It calls the ordinary C++ builders in `native/` as each
production completes.

Backend values and native builders share `nix::lex`; grammar productions live
in `nix::lex::grammar`. The upstream library keeps its separate `lexy` namespace.

Read the implementation in this order:

1. `parser.cc` and `native/context.hh` for parse lifetime and native ownership.
2. `grammar.hh`, `grammar/roots.hh`, and `grammar/functions.hh` for entry points.
3. `grammar/common.hh` and `values/` for callbacks and unfinished values.
4. The relevant production family and its matching native builder.

## File map

| Files | Responsibility |
| --- | --- |
| `parser.cc` | Backend methods, parse lifetime, syntax errors, and grammar instantiation. |
| `values/` | Native expression handles, unfinished bindings/formals/fragments, and scratch storage. |
| `grammar/` | Lexical rules and expression productions, with separate string, binding, formal, lambda, application, and selection headers. |
| `grammar/paths/` | Path characters, recursion-free lookahead, interpolation, and search paths. |
| `grammar/operators/` | One header per precedence level, including its associativity. |
| `native/` | Native builders grouped by literals, collections, lambdas, control flow, attributes, strings, paths, operators, calls, and positions/documentation. |
| `values.hh`, `grammar.hh` | Umbrella headers; `grammar.hh` completes the recursive production graph. |
| `meson.build` | Optional parser sources, included only when lexy support is enabled. |

## Ownership and allocation

`parser.cc` passes a native `Builder` and its scratch resource through lexy's
parse-state argument. Callbacks use `lexy::bind` and `lexy::parse_state` to receive
that builder explicitly. `lexy::as_list<std::pmr::vector<T>>.allocator(...)` supplies
scratch storage to both list sinks and optional empty lists.
Grammar callbacks construct native `Expr` objects as productions
complete. Each completed expression is passed by value as an `Expr *` plus its
first source byte. This retains positions for native literal nodes that do not
store their own `PosIdx`, without allocating a second expression node. There is
no intermediate expression tree and no recursive conversion pass.

Native builders allocate through `std::pmr::monotonic_buffer_resource` cursors
owned by `ParserState::exprs`, implemented in the private [`../parser-allocators.hh`](../parser-allocators.hh).
`Exprs` initializes its cursor map on the first `parserAllocator()` call.
Each parsing thread reuses one cursor across successive files, so unused space
in its current chunk remains available to later parses. Bison parsing uses
`Exprs::alloc` directly and does not request a cursor.
Both the resource objects and their chunks outlive individual parses, including
failed parses. They are destroyed with `Exprs`; a thread's exit does not release
storage still referenced by expressions.

A mutex protects cursor lookup/creation at parse entry. Node allocations use the
thread's unsynchronized cursor; only new chunk requests reach the shared,
thread-safe expression arena. A 256-byte initial size limits the minimum request
for tiny expressions. Standard PMR growth may leave unused space in the last
chunk of each cursor. This preserves independent allocation cursors for concurrent
parsing threads without an atomic operation per node. Nested parses on one thread
can use the same cursor because allocation calls do not overlap.

Use the borrowed allocator only on the requesting thread. Do not pass it to a
mutable container that retains it and might later allocate on another thread.
The constructors used here copy bytes or arrays and store no allocator reference;
mutable native containers retain their existing allocator and binding lifecycle.
Symbol interning and shared `ParserState` helpers also keep their existing allocators.

The builder reuses concatenation and indented-string assembly vectors within a
parse. Parent construction runs after all its children have completed, and native
constructors copy these vectors before returning. Do not recurse into the parser
while using an assembly buffer; unfinished grammar lists keep their own storage.
Each vector retains capacity between builder calls and grows when a later
construction needs more space.

Unfinished lists, bindings, formal parameters, and string/path parts use scratch
vectors. Their expression children are already native. Builders copy literal
bytes into `ParserState::exprs`, intern names in its symbol table, and convert
borrowed positions to offsets in its source origin. No source view or scratch
container may escape into the native tree. Existing source ownership and evaluator
interfaces are unchanged. All mutable parsing state is explicit; the grammar
has no thread-local builder, allocator, or documentation hook.
Semantic errors can be raised as soon as their
production completes, before a later syntax error in the same input.

Plain quoted attribute names keep borrowed or decoded byte views until name
interning, including escaped and empty names. They use the ordinary quoted-string
recognizer with a separate completion callback, avoiding a discarded native
`ExprString` and a second copy of the literal. Interpolated names still construct
native expressions; `${"literal"}` retains the same handling as Bison.

Documentation is ordinary parse data. `ws_star` recognizes trivia with a list
and uses `fold_inplace` to retain only its last documentation comment. A
`callback_with_state` receives that span and the next token's position after
the trivia run completes. Recognition during lookahead invokes no value
callbacks, so it cannot register comments. There is no deduplication cache or
second scan of the comment text. Lambda construction attaches explicit comments
before a containing binding can propagate its comment; native `setDocComment`
preserves the innermost comment on curried functions.

Names, verbatim literals, and ordinary string/path runs borrow the caller's buffer
until their native parent is built. Each string or path has at most one literal
part between interpolations. Quoted strings decode escapes and CR/CRLF once into
a scratch buffer when needed; unchanged runs remain borrowed. The byte recognizer
preserves dollar pairing and reports a trailing escape at EOF as incomplete input.
Indented strings retain raw spans until `native/indented-strings.cc` reconstructs
native `StringToken`s and calls `stripIndentation`. Preserve Flex's ordinary-token
boundaries and indentation flags: merging escaped newline and trailing-space
tokens changes final-line trimming. Paths retain complete literal spans including
separators; `native/paths.cc` preserves native prefix/suffix boundaries around
repeated separators and resolves prefixes through the source accessor.

`values/storage.hh` starts with 16 KiB of local storage and uses uniform 64 KiB
overflow blocks for ordinary allocations. Oversized vectors get dedicated blocks.
Scratch containers have ordinary C++ lifetimes. Released buffers up to 4 KiB
are recycled in power-of-two size classes; completed native children allow those
buffers to serve subsequent productions. Larger or over-aligned allocations are
retained until bulk release at the end of parsing. Decoded byte runs have no
individual owner and remain live until then. Arena alignment, live-value stability,
small-buffer reuse, and bulk release are tested independently of whether the lexy
backend is enabled.

## Grammar conventions

Production headers include definitions used through `dsl::p`, while
`grammar/forward.hh` declares recursion boundaries used by `dsl::recurse`.
Include `grammar.hh` before instantiating a parse. Only `parser.cc` instantiates
lexy templates; native builders are ordinary translation units, so their edits
can be compiled without rebuilding the grammar. `positioned_value` supplies a
production's start byte before native construction; forwarding productions keep
the child's original position. Operator definitions specify precedence and
associativity for lexy's expression parser, which calls native builders as each
operation completes. A plain operand does not traverse a stack of precedence
productions. Unary operators use `prefix_op`; attribute tests use `postfix_op`.
Nix permits `!` wherever an operand is expected, even below the current binding
power. `expr_logical_not` reenters through `subexpression_production` for this
case, preserving the precedence of its operand without consuming later lower
precedence operators. Its lookahead excludes `!=` so recursion always advances.
Pipes form an outer expression with separate operator groups for each direction.
This rejects mixed directions without parentheses and folds completed calls
directly, avoiding an intermediate operand list.
Application uses `fold_inplace` to append each completed argument to a native
call. Its first selection is required; a recursion-free lookahead acts as a
zero-width separator for later arguments. The fold separately retains the start
of the first selection so parenthesized functions keep their call positions,
while an expression without arguments keeps its original position.

Choices commit once their branch condition matches. Keep lookahead recursion-free:
`dsl::peek` parses through a token matcher with expression recursion disabled.
Use the path-prefix and expression-start recognizers rather than peeking a full
recursive expression. Trivia is explicit: `spaced(rule)` appends `dsl::p<ws_star>`
to a complete token while preserving its branch behavior. Roots consume leading
trivia, and complete tokens consume trailing trivia. Do not introduce automatic
whitespace skipping: it would consume documentation before the stateful callback
can see it. Lexical prefixes and string/path interiors remain unspaced.
Interpolation consumes trivia after `${` and inside its expression, but leaves
the bytes after `}` to its caller. Dynamic attribute names add trailing trivia;
string and path parts retain following bytes as literal text. Selection and
attribute-path dots share the same boundary rule, including a comment immediately
after a dot (`a./* comment */b`).

`variable_start`, `uri_prefix`, and `number_start` inspect only enough input to
choose a branch; full productions capture and validate the token. Reserved-word
checks share a literal trie of `LEXY_KEYWORD` rules. Identifier recognition uses
`dsl::identifier` and `.reserve(...)`; ASCII sets use character-class unions.
Selection's
`or` recognizer also excludes longer path and URI tokens: `f or/path` and
`f or:uri` are applications, not selection defaults or a reserved word.

Keep semantic checks in native builders where possible. Attribute merging,
duplicate detection, formal validation, indentation stripping, path accessors,
and source ownership should remain consistent with the Bison backend.
Documentation is collected once from committed trivia through normal parse-state
callbacks. Unattached documentation remains legal trivia, and apparent comment
markers inside strings or other comments cannot produce documentation values.

## Language options and compatibility

`LanguageOptions` in `grammar/options.hh` is a structural template argument
throughout the recursive grammar. Expression-bearing productions carry the same
options into interpolations, formal defaults, bindings, and nested collections;
lexical productions that do not depend on language options remain shared.
The `recurse` wrapper in `common.hh` delays specialization across grammar cycles
until parser instantiation, avoiding eager instantiation during C++ operator
lookup. It delegates to lexy's `dsl::recurse` without changing depth accounting.

The driver instantiates two variants, selected using the same `pipe-operators`
experimental-feature setting as Bison. Disabled pipes are absent from the
expression grammar. The `cursedOr` option controls the legacy `f or` production
and stays enabled in both runtime variants. Its native call retains the end
position for deprecation snippets; application, selection-default, and list
contexts mirror Bison's warning/reset behavior. Adding other runtime combinations
requires measuring the grammar translation unit's compile time and memory.

Repeated attribute tests (`x ? a ? b`) remain left-associated in both backends.
Bison's `%nonassoc '?'` does not prohibit them: the completed attribute path
reduces without a shift/reduce conflict on the following question mark.

Syntax error wording may differ. The driver marks errors at EOF as incomplete
input for the REPL; semantic failures and the recursion bound are complete errors.

## Nesting

`grammar/roots.hh` bounds actual grammar cycles. Precedence transitions do not
consume this budget. Both entry points are tested with 8,192 nested parentheses
and incomplete input at that depth. This is not unrestricted nesting parity:
Bison uses a growing heap stack, while lexy still uses the C++ call stack. Its
capacity depends on the grammar, compiler, build mode, and calling thread. This
is an implementation limitation, not an intentional language difference. Long
flat operator chains must not be rejected merely for containing many operators.

Prefix and right-associative operators have a separate limit of 10,000 pending
operations. Their `dsl::op` rules produce a move-only guard through a positioned,
state-bound callback. The guard counts the operator until its operand completes,
and releases the count on exceptions. This includes operators inside nested
expressions. Exceeding the bound produces a complete parse error even at EOF.
Lexy's built-in operator limit counts completed operations too, including
mixed-precedence operands of `infix_op_list`, so it cannot enforce this policy
without rejecting long flat chains. The parser instead bounds live guards.

## Changing the parser

Add language and lifetime regressions to `src/libexpr-tests/parser.cc` (relative
to the repository root). The same cases run against both backends; backend
selection must preserve the accepted language and evaluation results.

Use `nix-parser-check` for differential acceptance and native-tree checks; no
intentional acceptance or tree differences are expected. Use generated
boundary cases and real source trees, and compare performance with an optimized
build before changing allocation or token scanning. The developer guide describes
the measurement boundaries and limits of these comparisons.
