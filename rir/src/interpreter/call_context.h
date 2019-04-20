#ifndef RIR_INTERPRETER_DATA_C_H
#define RIR_INTERPRETER_DATA_C_H

#include "../compiler/parameter.h"
#include "../config.h"
#include <assert.h>
#include <stdint.h>

#include "runtime/Code.h"
#include "runtime/DispatchTable.h"
#include "runtime/Function.h"

#include "R/r.h"

#include "instance.h"
#include "interp_incl.h"
#include "safe_force.h"

namespace rir {

struct CallContext {
    CallContext(Code* c, SEXP callee, size_t nargs, SEXP ast,
                R_bcstack_t* stackArgs, Immediate* implicitArgs,
                Immediate* names, SEXP callerEnv,
                const Assumptions& givenAssumptions, size_t* sfCounter,
                InterpreterInstance* ctx)
        : caller(c), suppliedArgs(nargs), passedArgs(nargs),
          stackArgs(stackArgs), implicitArgs(implicitArgs), names(names),
          callerEnv(callerEnv), ast(ast), callee(callee),
          givenAssumptions(givenAssumptions), sfCounter(sfCounter) {
        assert(callee &&
               (TYPEOF(callee) == CLOSXP || TYPEOF(callee) == SPECIALSXP ||
                TYPEOF(callee) == BUILTINSXP));
    }

    CallContext(Code* c, SEXP callee, size_t nargs, Immediate ast,
                Immediate* implicitArgs, Immediate* names, SEXP callerEnv,
                const Assumptions& givenAssumptions, size_t* sfCounter,
                InterpreterInstance* ctx)
        : CallContext(c, callee, nargs, cp_pool_at(ctx, ast), nullptr,
                      implicitArgs, names, callerEnv, givenAssumptions,
                      sfCounter, ctx) {}

    CallContext(Code* c, SEXP callee, size_t nargs, Immediate ast,
                R_bcstack_t* stackArgs, Immediate* names, SEXP callerEnv,
                const Assumptions& givenAssumptions, InterpreterInstance* ctx)
        : CallContext(c, callee, nargs, cp_pool_at(ctx, ast), stackArgs,
                      nullptr, names, callerEnv, givenAssumptions, nullptr,
                      ctx) {}

    CallContext(Code* c, SEXP callee, size_t nargs, Immediate ast,
                Immediate* implicitArgs, SEXP callerEnv,
                const Assumptions& givenAssumptions, size_t* sfCounter,
                InterpreterInstance* ctx)
        : CallContext(c, callee, nargs, cp_pool_at(ctx, ast), nullptr,
                      implicitArgs, nullptr, callerEnv, givenAssumptions,
                      sfCounter, ctx) {}

    CallContext(Code* c, SEXP callee, size_t nargs, Immediate ast,
                R_bcstack_t* stackArgs, SEXP callerEnv,
                const Assumptions& givenAssumptions, InterpreterInstance* ctx)
        : CallContext(c, callee, nargs, cp_pool_at(ctx, ast), stackArgs,
                      nullptr, nullptr, callerEnv, givenAssumptions, nullptr,
                      ctx) {}

    const Code* caller;
    const size_t suppliedArgs;
    size_t passedArgs;
    const R_bcstack_t* stackArgs;
    const Immediate* implicitArgs;
    const Immediate* names;
    const SEXP callerEnv;
    const SEXP ast;
    const SEXP callee;
    Assumptions givenAssumptions;
    size_t* sfCounter;
    SEXP arglist = nullptr;

    bool hasStackArgs() const { return stackArgs != nullptr; }
    bool hasEagerCallee() const { return TYPEOF(callee) == BUILTINSXP; }
    bool hasNames() const { return names; }

    Immediate implicitArgIdx(unsigned i) const {
        assert(implicitArgs);
        if (i < passedArgs)
            return implicitArgs[i];
        else
            return MISSING_ARG_IDX;
    }

    bool missingArg(unsigned i) const {
        return implicitArgIdx(i) == MISSING_ARG_IDX;
    }

    Code* implicitArg(unsigned i) const {
        assert(caller);
        return caller->getPromise(implicitArgIdx(i));
    }

    SEXP implicitArgEager(unsigned i) const {
        assert(caller);
        unsigned idx = implicitArgIdx(i);
        if (idx == MISSING_ARG_IDX || idx == DOTS_ARG_IDX)
            // Should not return R_MissingArg - missing args are explicitly
            // handled elsewhere
            return R_UnboundValue;
        else
            return caller->getEagerPromise(idx);
    }

    SEXP stackArg(unsigned i) const {
        assert(stackArgs && i < passedArgs);
        return ostack_at_cell(stackArgs + i);
    }

    SEXP argSexp(unsigned i) const {
        if (hasStackArgs())
            return stackArg(i);
        else
            return implicitArgEager(i);
    }

    SEXP name(unsigned i, InterpreterInstance* ctx) const {
        assert(hasNames() && i < suppliedArgs);
        return cp_pool_at(ctx, names[i]);
    }

    bool shouldSafeForce();

    void incSafeForceCounter();
};

} // namespace rir

#endif // RIR_INTERPRETER_C_H
