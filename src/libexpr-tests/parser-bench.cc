#include <benchmark/benchmark.h>

#include "nix/expr/eval.hh"
#include "nix/expr/eval-settings.hh"
#include "nix/expr/parser.hh"
#include "nix/fetchers/fetch-settings.hh"
#include "nix/store/store-open.hh"
#include "nix/util/configuration.hh"
#include "nix/util/environment-variables.hh"
#include "nix/util/file-system.hh"

namespace nix {

template<bool BindTimed>
static void parseSource(benchmark::State & state, const std::string & source, const std::string & backend)
{
    auto store = openStore("dummy://");
    fetchers::Settings fetchSettings{};
    bool readOnlyMode = true;
    EvalSettings evalSettings{readOnlyMode};
    evalSettings.nixPath = {};
    evalSettings.parser = backend;

    // Validate the input and backend before starting the timed loop.
    try {
        EvalState evaluator(LookupPath{}, store, fetchSettings, evalSettings, nullptr);
        evaluator.parseExprFromString(source, evaluator.rootPath(CanonPath::root));
    } catch (UsageError & e) {
        if (backend == "lexy"
            && std::string_view(e.what()).find("without lexy parser support") != std::string_view::npos)
            state.SkipWithMessage("lexy support disabled");
        else
            state.SkipWithError(e.what());
        return;
    } catch (Error & e) {
        state.SkipWithError(e.what());
        return;
    }

    for (auto _ : state) {
        state.PauseTiming();
        {
            // A fresh evaluator prevents retained expressions, symbols and source
            // positions from accumulating across benchmark iterations.
            EvalState evaluator(LookupPath{}, store, fetchSettings, evalSettings, nullptr);
            auto basePath = evaluator.rootPath(CanonPath::root);
            if constexpr (BindTimed) {
                state.ResumeTiming();
                auto expression = evaluator.parseExprFromString(source, basePath);
                benchmark::DoNotOptimize(expression);
                state.PauseTiming();
            } else {
                auto original = make_ref<std::string>(source);
                auto buffer = source;
                buffer.append("\0\0", 2);
                DocCommentMap comments;
                state.ResumeTiming();
                auto expression = parseExprFromBuf(
                    buffer.data(),
                    buffer.size(),
                    Pos::String{.source = original},
                    basePath,
                    evaluator.mem.exprs,
                    evaluator.symbols,
                    evalSettings,
                    evaluator.positions,
                    comments,
                    evaluator.rootFS);
                benchmark::DoNotOptimize(expression);
                state.PauseTiming();
                // Binding migrates temporary native containers into the AST
                // arena. Keep that lifecycle step, but exclude it from Parse.
                expression->bindVars(evaluator, evaluator.staticBaseEnv);
            }
        }
        state.ResumeTiming();
    }

    state.SetBytesProcessed(state.iterations() * source.size());
    state.SetItemsProcessed(state.iterations());
}

void registerParserBenchmarks()
{
    experimentalFeatureSettings.set("experimental-features", "pipe-operators");
    auto add = [](const std::string & name, const std::string & source) {
        for (std::string backend : {"bison", "lexy"}) {
            benchmark::RegisterBenchmark(
                ("Parse/" + name + "/" + backend).c_str(),
                [source, backend](benchmark::State & state) { parseSource<false>(state, source, backend); });
            benchmark::RegisterBenchmark(
                ("ParseAndBind/" + name + "/" + backend).c_str(),
                [source, backend](benchmark::State & state) { parseSource<true>(state, source, backend); });
        }
    };

    for (auto count : {100, 1000, 10000}) {
        std::string attrs = "{\n";
        std::string strings = "let name = \"world\"; in [\n";
        for (int i = 0; i < count; ++i) {
            attrs += "attr" + std::to_string(i) + " = { x = 1 + 2 * 3; y = [ true null 42 ]; };\n";
            strings += "\"hello ${name}: " + std::to_string(i) + "\\n\"\n";
        }
        add("attrs/" + std::to_string(count), attrs + "}");
        add("strings/" + std::to_string(count), strings + "]");
    }

    // Read once so the file workload measures the same warm-buffer path as the
    // generated workloads. Imports are parsed as expressions, never evaluated.
    if (auto path = getEnv("NIX_PARSER_BENCH_FILE"))
        add("file", readFile(*path));
}

} // namespace nix
