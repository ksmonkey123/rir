#ifndef BB_TRANSFORM_H
#define BB_TRANSFORM_H

#include "../pir/bb.h"
#include "../pir/pir.h"
#include "../util/cfg.h"

namespace rir {
namespace pir {

class Assume;
class FrameState;
class Checkpoint;
class BBTransform {
  public:
    static BB* clone(BB* src, Code* target, ClosureVersion* targetClosure);
    static BB* splitEdge(size_t next_id, BB* from, BB* to, Code* target);
    static BB* split(size_t next_id, BB* src, BB::Instrs::iterator,
                     Code* target);
    static void splitCriticalEdges(Code* fun);
    static std::pair<Value*, BB*> forInline(BB* inlinee, BB* cont);
    static BB* lowerExpect(Code* closure, BB* src,
                           BB::Instrs::iterator position, Value* condition,
                           bool expected, BB* deoptBlock,
                           const std::string& debugMesage);
    static Assume* insertAssume(Instruction* condition, Checkpoint* cp, BB* bb,
                                BB::Instrs::iterator& position,
                                bool assumePositive);
    static Assume* insertAssume(Instruction* condition, Checkpoint* cp,
                                bool assumePositive);
    static Assume* insertIdenticalAssume(Value* given, Value* expected,
                                         Checkpoint* cp, BB* bb,
                                         BB::Instrs::iterator& position);

    static void mergeRedundantBBs(Code* closure);

    // Renumber in dominance order. This ensures that controlflow always goes
    // from smaller id to bigger id, except for back-edges.
    static void renumber(Code* fun);

    // Remove dead instructions (instead of waiting until the cleanup pass).
    // Note that a phi is dead if it is not used by any instruction, or it is
    // used only by dead phis.
    static void removeDeadInstrs(Code* fun);
};

} // namespace pir
} // namespace rir

#endif
