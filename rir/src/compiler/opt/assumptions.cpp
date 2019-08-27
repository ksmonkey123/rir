#include "../analysis/available_checkpoints.h"
#include "../pir/pir_impl.h"
#include "../translations/pir_translator.h"
#include "../util/cfg.h"
#include "../util/visitor.h"

#include "R/r.h"
#include "pass_definitions.h"

#include <unordered_map>
#include <unordered_set>

namespace rir {
namespace pir {

struct AAssumption {
    // cppcheck-suppress noExplicitConstructor
    AAssumption(Assume* a) : yesNo(a->assumeTrue), assumption(a->condition()) {}
    bool yesNo;
    Value* assumption;
    bool operator==(const AAssumption& other) const {
        return yesNo == other.yesNo && assumption == other.assumption;
    }
    void print(std::ostream& out, bool) {
        if (!yesNo)
            out << "!";
        assumption->printRef(out);
    }
};

struct AvailableAssumptions
    : public StaticAnalysis<IntersectionSet<AAssumption>> {
    AvailableAssumptions(ClosureVersion* cls, LogStream& log)
        : StaticAnalysis("AvailableAssumptions", cls, cls, log) {}
    AbstractResult apply(IntersectionSet<AAssumption>& state,
                         Instruction* i) const {
        AbstractResult res;
        if (auto a = Assume::Cast(i)) {
            if (!state.available.includes(a)) {
                state.available.insert(a);
                res.update();
            }
        }
        return res;
    }
    const SmallSet<AAssumption> at(Instruction* i) const {
        auto res = StaticAnalysis::at<
            StaticAnalysis::PositioningStyle::BeforeInstruction>(i);
        return res.available;
    }
};

void OptimizeAssumptions::apply(RirCompiler&, ClosureVersion* function,
                                LogStream& log) const {
    {
        CFG cfg(function);
        Visitor::run(function->entry, [&](BB* bb) {
            if (bb->isBranch()) {
                if (auto cp = Checkpoint::Cast(bb->last())) {
                    if (cfg.isMergeBlock(cp->nextBB())) {
                        // Ensure that bb at the beginning of a loop has a bb
                        // to hoist assumes out of the loop.
                        auto preheader = new BB(function, function->nextBBId++);
                        preheader->setNext(cp->nextBB());
                        assert(cp->nextBB() == bb->next0);
                        bb->next0 = preheader;
                    }
                }
            }
        });
    }

    AvailableCheckpoints checkpoint(function, log);
    AvailableAssumptions assumptions(function, log);
    std::unordered_map<Checkpoint*, Checkpoint*> replaced;

    std::unordered_map<Instruction*, std::pair<Instruction*, Checkpoint*>>
        hoistAssume;
    Visitor::runPostChange(function->entry, [&](BB* bb) {
        auto ip = bb->begin();
        while (ip != bb->end()) {
            auto next = ip + 1;
            auto instr = *ip;

            // Remove Unneccessary checkpoints. If we arrive at a checkpoint and
            // the previous checkpoint is still available, and there is also a
            // next checkpoint available we might as well remove this one.
            if (auto cp = Checkpoint::Cast(instr)) {
                if (auto cp0 = checkpoint.at(instr)) {
                    while (replaced.count(cp0))
                        cp0 = replaced.at(cp0);
                    replaced[cp] = cp0;

                    assert(bb->last() == instr);
                    cp->replaceUsesWith(cp0);
                    bb->remove(ip);
                    delete bb->next1;
                    bb->next1 = nullptr;
                    return;
                }
            }

            if (auto assume = Assume::Cast(instr)) {
                if (assumptions.at(instr).includes(assume)) {
                    next = bb->remove(ip);
                } else {
                    // We are trying to group multiple assumes into the same
                    // checkpoint by finding for each assume the topmost
                    // compatible
                    // checkpoint.
                    // TODO: we could also try to move up the assume itself,
                    // since
                    // if we move both at the same time, we could even jump over
                    // effectful instructions.
                    if (auto cp0 = checkpoint.at(instr)) {
                        while (replaced.count(cp0))
                            cp0 = replaced.at(cp0);
                        if (assume->checkpoint() != cp0)
                            assume->checkpoint(cp0);
                    }
                    auto guard = Instruction::Cast(assume->condition());
                    auto cp = assume->checkpoint();
                    if (guard && guard->bb() != cp->bb()) {
                        if (auto cp0 = checkpoint.at(guard)) {
                            while (replaced.count(cp0))
                                cp0 = replaced.at(cp0);
                            if (assume->checkpoint() != cp0) {
                                hoistAssume[guard] = {guard, cp0};
                                next = bb->remove(ip);
                            }
                        }
                    }
                }
            }
            ip = next;
        }
    });

    Visitor::run(function->entry, [&](BB* bb) {
        auto ip = bb->begin();
        while (ip != bb->end()) {
            auto h = hoistAssume.find(*ip);
            if (h != hoistAssume.end()) {
                auto g = h->second;
                ip++;
                ip = bb->insert(ip, new Assume(g.first, g.second));
            }
            ip++;
        }
    });
}

} // namespace pir
} // namespace rir
