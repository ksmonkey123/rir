#ifndef LAZY_ENVIRONMENT_H
#define LAZY_ENVIRONMENT_H

#include "../ir/BC_inc.h"
#include "../runtime/RirRuntimeObject.h"
#include "instance.h"
#include "interp_incl.h"

#include <cassert>
#include <cstdint>
#include <functional>

namespace rir {

#define LAZY_ENVIRONMENT_MAGIC 0xe4210e47

/**
 * EnvironmentStub holds the information needed to create an
 * environment lazily.
 */
struct LazyEnvironment
    : public RirRuntimeObject<LazyEnvironment, LAZY_ENVIRONMENT_MAGIC> {
    LazyEnvironment() = delete;
    LazyEnvironment(const LazyEnvironment&) = delete;
    LazyEnvironment& operator=(const LazyEnvironment&) = delete;
    constexpr static int ArgOffset = 2;

    LazyEnvironment(SEXP parent, size_t nargs, Immediate* names)
        : RirRuntimeObject(sizeof(LazyEnvironment) + sizeof(char) * nargs,
                           nargs + ArgOffset),
          nargs(nargs), names(names) {
        memset(notMissing, 0, sizeof(char) * nargs);
    }

    SEXP materialized() { return getEntry(0); }
    void materialized(SEXP m) { setEntry(0, m); }

    size_t nargs;
    Immediate* names;

    SEXP getArg(size_t i) { return getEntry(i + ArgOffset); }
    void setArg(size_t i, SEXP val, bool overrideMissing) {
        setEntry(i + ArgOffset, val);
        if (overrideMissing)
            notMissing[i] = true;
    }

    SEXP getParent() { return getEntry(1); }

    static LazyEnvironment* BasicNew(SEXP parent, size_t nargs,
                                     Immediate* names) {
        SEXP wrapper = Rf_allocVector(
            EXTERNALSXP, sizeof(LazyEnvironment) + sizeof(char) * nargs +
                             sizeof(SEXP) * (nargs + ArgOffset));
        auto le = new (DATAPTR(wrapper))
            LazyEnvironment(parent, nargs, (Immediate*)names);
        le->setEntry(1, parent);
        assert(LazyEnvironment::check(wrapper));
        return le;
    }

    static LazyEnvironment* New(SEXP parent, size_t nargs, Immediate* names) {
        auto le = BasicNew(parent, nargs, names);
        for (long i = nargs - 1; i >= 0; --i) {
            auto v = ostack_pop(ctx);
            INCREMENT_NAMED(v);
            le->setArg(i, v, false);
        }
        return le;
    }

    // This byteset remembers which slots have been overwritten, such that they
    // should not be considered missing anymore.
    char notMissing[];
};

} // namespace rir

#endif
