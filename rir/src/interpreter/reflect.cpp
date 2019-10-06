#include "reflect.h"
#include "LazyEnvironment.h"
#include "compiler/parameter.h"
#include "runtime/DispatchTable.h"
#include "runtime/Function.h"
#include "utils/Pool.h"
#include "utils/Terminal.h"
#include "utils/capture_out.h"
#include <R/Symbols.h>
#include <iostream>

extern "C" SEXP R_ReplaceFunsTable;
extern "C" RPRSTACK* R_PendingPromises;

namespace rir {

bool freezeEnabled = false;

inline static SEXP currentEnv() {
    if (R_ExternalEnvStack == NULL)
        return R_BaseEnv;
    else
        return R_ExternalEnvStack->env;
}

inline static ReflectGuard curReflectGuard() {
    return (ReflectGuard)R_GlobalContext->curReflectGuard;
}

inline static bool canEnvAccess(SEXP x, SEXP target) {
    SLOWASSERT(TYPEOF(target) == ENVSXP);
    if (x == R_BaseEnv || x == R_BaseNamespace || target == R_ReplaceFunsTable)
        return true;
    for (SEXP cur = x; cur != R_EmptyEnv; /* increment in body */) {
        if (cur == symbol::delayedEnv) // TODO: Use most lexical non-elieded env
                                       // (probably cloenv)
            cur = R_GlobalEnv;
        if (cur == target)
            return true;
        if (auto lcur = LazyEnvironment::check(cur)) {
            if (lcur->materialized()) {
                cur = lcur->materialized();
            } else {
                cur = lcur->getParent();
            }
        } else {
            SLOWASSERT(TYPEOF(cur) == ENVSXP);
            cur = ENCLOS(cur);
        }
    }
    return false;
}

extern "C" Rboolean R_Visible;

// Mark the topmost RIR function non-reflective, and remove all RIR functions in
// the call stack from their dispatch table. Also jumps out of these functions'
// execution
static void taintCallStack(ReflectGuard guard) {
    if (ConsoleColor::isTTY(std::cerr)) {
        ConsoleColor::yellow(std::cerr);
    }
    std::cerr
        << "Unmarked closure tried to perform introspection. RIR stack:\n";
    if (ConsoleColor::isTTY(std::cerr)) {
        ConsoleColor::clear(std::cerr);
    }

    bool taintedTopmost = false;
    RCNTXT* rctx;
    for (rctx = R_GlobalContext;
         !rctx->isFreezeFunCtx && rctx->callflag != CTXT_TOPLEVEL;
         rctx = rctx->nextcontext) {
        if (Function* fun = (Function*)rctx->rirCallFun) {
            DispatchTable* table = DispatchTable::unpack(BODY(rctx->callfun));
            // TODO: this version is still reachable from static call inline
            // caches (it will not be called though). We could delete the
            // function body to save some space.
            Pool::insert(fun->container());
            table->remove(fun->body());
            if (!taintedTopmost) {
                if (table->reflectGuard >= guard) {
                    std::cerr << "*";
                    table->reflectGuard = ReflectGuard::None;
                    for (size_t i = 1; i < table->size(); i++) {
                        Function* ofun = table->get(i);
                        if (ofun != fun && ofun->reflectGuard >= guard) {
                            Pool::insert(ofun->container());
                            table->remove(ofun->body());
                            i--; // since table's size decreased
                        }
                    }
                }
                if (fun->reflectGuard >= guard) {
                    std::cerr << "+";
                    taintedTopmost = true;
                }
            }
            std::cerr << "- ";
            Rf_PrintValue(rctx->call);
        }
    }
    std::cerr << "=====\n";
    assert(taintedTopmost);
    // Don't signal pending promise warnings
    while (R_PendingPromises != rctx->prstack) {
        SET_PRSEEN(R_PendingPromises->promise, 0);
        R_PendingPromises = R_PendingPromises->next;
    }
    R_jumpctxt(rctx, 0, R_RestartToken);
}

void willPerformReflection(SEXP env, EnvAccessType typ) {
    switch (curReflectGuard()) {
    case ReflectGuard::None:
        break;
    case ReflectGuard::Introspect:
        if (typ == EnvAccessType::Get) // Only blocks introspection
            break;
        taintCallStack(ReflectGuard::Introspect);
        break;
    default:
        assert(false);
        break;
    }
}

void willAccessEnv(SEXP env, EnvAccessType typ) {
    // Fastcase no-guard
    if (curReflectGuard() != ReflectGuard::None &&
        !canEnvAccess(currentEnv(), env))
        willPerformReflection(env, typ);
}

} // namespace rir
