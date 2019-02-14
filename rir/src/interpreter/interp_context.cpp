#include "interp_context.h"
#include "api.h"
#include "runtime.h"

void initializeResizeableList(ResizeableList* l, size_t capacity, SEXP parent,
                              size_t index) {
    l->capacity = capacity;
    l->list = Rf_allocVector(VECSXP, capacity);
    SET_VECTOR_ELT(parent, index, l->list);
    rl_setLength(l, 0);
}

SEXP R_Subset2Sym;
SEXP R_SubsetSym;
SEXP R_SubassignSym;
SEXP R_Subassign2Sym;
SEXP R_valueSym;
SEXP setterPlaceholderSym;
SEXP getterPlaceholderSym;
SEXP quoteSym;

R_bcstack_t int_stack_obj(int x) {
    R_bcstack_t res;
#ifdef USE_TYPED_STACK
    res.tag = STACK_OBJ_INT;
    res.u.ival = x;
#else
    res.tag = STACK_OBJ_SEXP;
    res.u.sxpval = Rf_allocVector(INTSXP, 1);
    *INTEGER(res.u.sxpval) = x;
#endif
    return res;
}

R_bcstack_t real_stack_obj(double x) {
    R_bcstack_t res;
#ifdef USE_TYPED_STACK
    res.tag = STACK_OBJ_REAL;
    res.u.dval = x;
#else
    res.tag = STACK_OBJ_SEXP;
    res.u.sxpval = Rf_allocVector(REALSXP, 1);
    *REAL(res.u.sxpval) = x;
#endif
    return res;
}

R_bcstack_t logical_stack_obj(int x) {
    R_bcstack_t res;
#ifdef USE_TYPED_STACK
    res.tag = STACK_OBJ_LOGICAL;
    res.u.ival = x;
#else
    res.tag = STACK_OBJ_SEXP;
    res.u.sxpval = Rf_allocVector(LGLSXP, 1);
    *LOGICAL(res.u.sxpval) = x;
#endif
    return res;
}

R_bcstack_t sexp_to_stack_obj(SEXP x, bool unprotect) {
    assert(x != NULL);
#ifdef USE_TYPED_STACK
    if (x == loopTrampolineMarker || ATTRIB(x) != R_NilValue) {
        R_bcstack_t res;
        res.tag = STACK_OBJ_SEXP;
        res.u.sxpval = x;
        return res;
    } else if (IS_SIMPLE_SCALAR(x, INTSXP)) {
        return int_stack_obj(*INTEGER(x));
    } else if (IS_SIMPLE_SCALAR(x, REALSXP)) {
        return real_stack_obj(*REAL(x));
    } else if (IS_SIMPLE_SCALAR(x, LGLSXP)) {
        return logical_stack_obj(*INTEGER(x));
    } else {
        R_bcstack_t res;
        res.tag = STACK_OBJ_SEXP;
        res.u.sxpval = x;
        return res;
    }
#else
    R_bcstack_t res;
    res.tag = STACK_OBJ_SEXP;
    res.u.sxpval = x;
    return res;
#endif
}

SEXP stack_obj_to_sexp(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_INT:
#ifdef USE_TYPED_STACK
        SEXP res;
        res = Rf_allocVector(INTSXP, 1);
        for (int i = 0; i < 100; i++) {
            R_PreserveObject(res);
        }
        *INTEGER(res) = x.u.ival;
        return res;
    case STACK_OBJ_REAL:
        res = Rf_allocVector(REALSXP, 1);
        for (int i = 0; i < 100; i++) {
            R_PreserveObject(res);
        }
        *REAL(res) = x.u.dval;
        return res;
    case STACK_OBJ_LOGICAL:
        res = Rf_allocVector(LGLSXP, 1);
        for (int i = 0; i < 100; i++) {
            R_PreserveObject(res);
        }
        *LOGICAL(res) = x.u.ival;
        return res;
#endif
    case STACK_OBJ_SEXP:
        for (int i = 0; i < 100; i++) {
            R_PreserveObject(x.u.sxpval);
        }
        return x.u.sxpval;
    default:
        assert(false);
    }
}

// Doesn't consider reals integers
bool stack_obj_is_integer(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_INT:
        return true;
    case STACK_OBJ_REAL:
    case STACK_OBJ_LOGICAL:
        return false;
    case STACK_OBJ_SEXP:
        return IS_SIMPLE_SCALAR(x.u.sxpval, INTSXP);
    default:
        assert(false);
    }
}

// Returns NA_INTEGER if not an integer, doesn't consider reals integers
int try_stack_obj_to_integer(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_INT:
        return x.u.ival;
    case STACK_OBJ_REAL:
    case STACK_OBJ_LOGICAL:
        return NA_INTEGER;
    case STACK_OBJ_SEXP:
        if (IS_SIMPLE_SCALAR(x.u.sxpval, INTSXP)) {
            return *INTEGER(x.u.sxpval);
        } else {
            return NA_INTEGER;
        }
    default:
        assert(false);
    }
}

// Doesn't consider integers reals
bool stack_obj_is_real(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_REAL:
        return true;
    case STACK_OBJ_INT:
    case STACK_OBJ_LOGICAL:
        return false;
    case STACK_OBJ_SEXP:
        return IS_SIMPLE_SCALAR(x.u.sxpval, REALSXP);
    default:
        assert(false);
    }
}

// Returns NA_REAL if not a real, doesn't consider integers reals
double try_stack_obj_to_real(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_REAL:
        return x.u.dval;
    case STACK_OBJ_INT:
    case STACK_OBJ_LOGICAL:
        return NA_REAL;
    case STACK_OBJ_SEXP:
        if (IS_SIMPLE_SCALAR(x.u.sxpval, REALSXP)) {
            return *REAL(x.u.sxpval);
        } else {
            return NA_REAL;
        }
    default:
        assert(false);
    }
}

bool stack_obj_is_logical(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_LOGICAL:
        return true;
    case STACK_OBJ_INT:
    case STACK_OBJ_REAL:
        return false;
    case STACK_OBJ_SEXP:
        return IS_SIMPLE_SCALAR(x.u.sxpval, LGLSXP);
    default:
        assert(false);
    }
}

// Returns NA_LOGICAL if not a logical
int try_stack_obj_to_logical(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_LOGICAL:
        return x.u.ival;
    case STACK_OBJ_INT:
    case STACK_OBJ_REAL:
        return NA_LOGICAL;
    case STACK_OBJ_SEXP:
        if (IS_SIMPLE_SCALAR(x.u.sxpval, LGLSXP)) {
            return *LOGICAL(x.u.sxpval);
        } else {
            return NA_LOGICAL;
        }
    default:
        assert(false);
    }
}

// Fails if not a logical or NA
int try_stack_obj_to_logical_na(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_LOGICAL:
        return x.u.ival;
    case STACK_OBJ_SEXP:
        if (TYPEOF(x.u.sxpval) == LGLSXP) {
            return XLENGTH(x.u.sxpval) == 0 ? NA_LOGICAL : *LOGICAL(x.u.sxpval);
        } else {
            assert(false);
        }
    default:
        assert(false);
    }
}

// Returns regular if int, truncated if real, -1 otherwise
int try_stack_obj_to_idx(R_bcstack_t x) {
    if (stack_obj_is_integer(x)) {
        return try_stack_obj_to_integer(x) - 1;
    } else if (stack_obj_is_real(x)) {
        return (int)try_stack_obj_to_real(x) - 1;
    } else {
        return -1;
    }
}

SEXPTYPE stack_obj_sexp_type(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_INT:
        return INTSXP;
    case STACK_OBJ_REAL:
        return REALSXP;
    case STACK_OBJ_LOGICAL:
        return LGLSXP;
    case STACK_OBJ_SEXP:
        return TYPEOF(x.u.sxpval);
    default:
        assert(false);
    }
}

bool stack_obj_is_vector(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_INT:
    case STACK_OBJ_REAL:
    case STACK_OBJ_LOGICAL:
        return true;
    case STACK_OBJ_SEXP:
        return Rf_isVector(x.u.sxpval);
    default:
        assert(false);
    }
}

R_xlen_t stack_obj_length(R_bcstack_t x) {
    switch (x.tag) {
    case STACK_OBJ_INT:
    case STACK_OBJ_REAL:
    case STACK_OBJ_LOGICAL:
        return 1;
    case STACK_OBJ_SEXP:
        return XLENGTH(x.u.sxpval);
    default:
        assert(false);
    }
}

bool stack_objs_equal(R_bcstack_t x, R_bcstack_t y) {
    if (x.tag != y.tag) {
        return false;
    }

    switch (x.tag) { // == y.tag
    case STACK_OBJ_INT:
    case STACK_OBJ_LOGICAL:
        return x.u.ival == y.u.ival;
    case STACK_OBJ_REAL:
        return x.u.dval == y.u.dval;
    case STACK_OBJ_SEXP:
        return x.u.sxpval == y.u.sxpval;
    default:
        assert(false);
    }
}

Context* context_create() {
    Context* c = new Context;
    c->list = Rf_allocVector(VECSXP, 2);
    R_PreserveObject(c->list);
    initializeResizeableList(&c->cp, POOL_CAPACITY, c->list, CONTEXT_INDEX_CP);
    initializeResizeableList(&c->src, POOL_CAPACITY, c->list,
                             CONTEXT_INDEX_SRC);
    // first item in source and constant pools is R_NilValue so that we can use
    // the index 0 for other purposes
    src_pool_add(c, R_NilValue);
    cp_pool_add(c, R_NilValue);
    R_Subset2Sym = Rf_install("[[");
    R_SubsetSym = Rf_install("[");
    R_SubassignSym = Rf_install("[<-");
    R_Subassign2Sym = Rf_install("[[<-");
    R_valueSym = Rf_install("value");
    setterPlaceholderSym = Rf_install("*.placeholder.setter.*");
    getterPlaceholderSym = Rf_install("*.placeholder.getter.*");
    quoteSym = Rf_install("quote");

    auto pir = getenv("PIR_ENABLE");

    c->exprCompiler = rir_compile;
    c->closureCompiler = [](SEXP closure, SEXP name) {
        return rir_compile(closure, R_NilValue);
    };
    c->closureOptimizer = [](SEXP f, const rir::Assumptions&, SEXP n) {
        return f;
    };

    if (pir && std::string(pir).compare("off") == 0) {
        // do nothing; use defaults
    } else if (pir && std::string(pir).compare("force") == 0) {
        c->closureCompiler = [](SEXP f, SEXP n) {
            SEXP rir = rir_compile(f, R_NilValue);
            return rirOptDefaultOpts(rir, rir::Assumptions(), n);
        };
    } else if (pir && std::string(pir).compare("force_dryrun") == 0) {
        c->closureCompiler = [](SEXP f, SEXP n) {
            SEXP rir = rir_compile(f, R_NilValue);
            return rirOptDefaultOptsDryrun(rir, rir::Assumptions(), n);
        };
    } else {
        c->closureOptimizer = rirOptDefaultOpts;
    }

    return c;
}

extern Context* globalContext_;
