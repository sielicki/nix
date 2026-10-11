#pragma once

/**
 * @file
 * Native expression construction, independent of lexy templates.
 * Builder methods take completed native children and construct native parents.
 * Callers must complete each child before constructing its parent.
 * Literal bytes and scratch containers must not escape: strings use state.exprs,
 * names use state.symbols, and source positions use state.positions.
 * Semantic checks reuse ParserState, including attribute merging and formals.
 * Implementation files are grouped by language feature and do not include
 * grammar headers, so builder implementation changes can compile independently.
 */

#include "../values.hh"
#include "nix/expr/parser-state.hh"
#include <string_view>
#include <utility>

namespace nix::lex {

struct Builder
{
    /** Native allocation, symbols, source origin, and shared semantic checks. */
    ParserState & state;
    /** Borrowed input; all temporary positions point into this byte range. */
    std::string_view text;
    /** Scratch storage is passed through lexy's parse state, never retained by native nodes. */
    std::pmr::memory_resource * scratch;

    template<typename T>
    std::pmr::polymorphic_allocator<T> scratchAllocator() const
    {
        return std::pmr::polymorphic_allocator<T>{scratch};
    }

    /** Count pending recursive operators, not completed operations in a flat chain. */
    static constexpr unsigned maxOperatorDepth = 10000;
    unsigned operatorDepth = 0;

    struct OperatorDepth
    {
        Builder * builder;

        OperatorDepth(Builder & builder, const char * position)
            : builder(&builder)
        {
            if (builder.operatorDepth == maxOperatorDepth)
                builder.error(position, "maximum operator nesting depth exceeded");
            ++builder.operatorDepth;
        }

        OperatorDepth(OperatorDepth && other) noexcept
            : builder(std::exchange(other.builder, nullptr))
        {
        }

        OperatorDepth & operator=(OperatorDepth && other) noexcept
        {
            if (this != &other) {
                if (builder)
                    --builder->operatorDepth;
                builder = std::exchange(other.builder, nullptr);
            }
            return *this;
        }

        ~OperatorDepth()
        {
            if (builder)
                --builder->operatorDepth;
        }

        OperatorDepth(const OperatorDepth &) = delete;
        OperatorDepth & operator=(const OperatorDepth &) = delete;
    };

    ParserLocation location(const char * p) const
    {
        auto offset = p ? static_cast<int>(p - text.data()) : 0;
        return {offset, offset};
    }

    PosIdx pos(const char * p) const
    {
        return state.at(location(p));
    }

    /** Reuse this thread's PMR cursor, owned by state.exprs across file parses. */
    std::pmr::polymorphic_allocator<char> alloc = state.exprs.parserAllocator();

    /**
     * Reused only while completing a parent: its children are already native,
     * and native constructors copy these spans before the next callback runs.
     * No parser recursion may occur while an assembly buffer is in use.
     */
    std::vector<std::pair<PosIdx, Expr *>> concatParts;
    std::vector<std::pair<PosIdx, std::variant<Expr *, StringToken>>> indentedParts;

    /** Associate the last documentation comment in committed trivia with its next token. */
    void recordDocComment(const char * begin, const char * end, const char * nextToken);

    /** Preserve documentation on lambdas, including binding-propagated comments. */
    ExprLambda * lambdaDoc(ExprLambda * lambda, PosIdx p);

    [[noreturn]] void error(const char * p, const std::string & message) const;

    template<typename T, typename... Args>
    T * make(Args &&... args)
    {
        // Mutable containers must not retain a thread-specific parser cursor.
        static_assert(!std::uses_allocator_v<T, std::pmr::polymorphic_allocator<char>>);
        return alloc.new_object<T>(std::forward<Args>(args)...);
    }

    /** Extend an existing native application when possible, preserving argument order. */
    Expr * call(PosIdx p, Expr * fn, Expr * arg);

    Expr * builtin(PosIdx p, Symbol name, Expr * a, Expr * b);

    AttrName attr(const nix::lex::attr_name & a);

    AttrSelectionPath attrPath(const std::pmr::vector<nix::lex::attr_name> & path);

    /** Share attribute merging, duplicate checks, and binding docs with ParserState. */
    ExprAttrs * bindings(const std::pmr::vector<nix::lex::binding> & bindings, PosIdx p = noPos);

    nix::lex::expression string(const char *, bool, const std::pmr::vector<nix::lex::part> &);

    /** Reconstruct native escape tokens before stripping indentation. */
    Expr * indentedString(const char *, const std::pmr::vector<nix::lex::part> &);

    nix::lex::expression path(const char *, nix::lex::path_parts);

    nix::lex::expression binary(nix::lex::binary_operator, nix::lex::expression, nix::lex::expression);

    using Expression = nix::lex::expression;
    Expression variable(const char *, std::string_view);
    Expression integer(const char *, std::string_view);
    Expression floating(const char *, std::string_view);
    Expression uri(const char *, std::string_view);
    Expression searchPath(const char *, std::string_view);
    Expression list(const char *, const std::pmr::vector<Expression> &);
    Expression listElement(Expression);
    Expression attrset(const char *, bool, const std::pmr::vector<nix::lex::binding> &);
    Expression simpleLambda(const char *, std::string_view, Expression);
    Expression lambda(const char *, const std::pmr::vector<nix::lex::formal> &, bool, std::string_view, Expression);
    Expression apply(const char *, Expression, Expression);
    Expression cursedOr(const char *, Expression, const char *);
    Expression select(const char *, Expression, const std::pmr::vector<nix::lex::attr_name> &, Expression);
    Expression hasAttr(const char *, Expression, const std::pmr::vector<nix::lex::attr_name> &);
    Expression conditional(const char *, Expression, Expression, Expression);
    Expression assertion(const char *, Expression, Expression);
    Expression with(const char *, Expression, Expression);
    Expression let(const char *, const std::pmr::vector<nix::lex::binding> &, Expression);
    Expression unary(const char *, nix::lex::unop_kind, Expression);
};

} // namespace nix::lex
