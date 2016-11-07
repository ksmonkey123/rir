#ifndef RIR_ANALYSIS_DATAFLOW_H
#define RIR_ANALYSIS_DATAFLOW_H

#include "ir/CodeEditor.h"
#include "R/Funtab.h"
#include "code/analysis.h"
#include "code/dispatchers.h"
#include "interpreter/interp_context.h"

namespace rir {

/*
 *                                  Any
 *
 *                Value
 *
 *               Constant                    Argument
 */

class FValue {
  public:
    enum class Type { Bottom, Constant, Argument, Value, Any };

    Type t = Type::Bottom;

    FValue() : FValue(Type::Bottom) {}
    FValue(Type t) : t(t) {
        assert(t == Type::Any || t == Type::Bottom);
        value.argument = -1;
    }
    FValue(const FValue& p) : t(p.t), value(p.value) {}

    static FValue Argument(int i) {
        FValue p;
        p.t = Type::Argument;
        p.value.argument = i;
        return p;
    }

    static FValue Constant(CodeEditor::Iterator def) {
        FValue p;
        p.t = Type::Constant;
        p.value.u = UseDef();
        p.value.u.def = def;
        return p;
    }

    static FValue Any() { return FValue(Type::Any); }

    static FValue Value(CodeEditor::Iterator ins) {
        FValue val;
        val.t = Type::Value;
        val.value.u = UseDef();
        val.value.u.def = ins;
        return val;
    }

    bool singleDef() {
        if (t == Type::Constant || t == Type::Value) {
            assert(value.u.def != UseDef::unused());
            return value.u.def != UseDef::multiuse();
        }
        return false;
    }

    CodeEditor::Iterator def() {
        assert(singleDef());
        return value.u.def;
    }

    void used(CodeEditor::Iterator ins) {
        if (value.u.use == UseDef::unused())
            value.u.use = ins;
        else if (value.u.use != ins)
            value.u.use = UseDef::multiuse();
    }

    bool used() {
        assert(t == Type::Constant || t == Type::Value);
        return value.u.use != UseDef::unused();
    }

    struct UseDef {
        CodeEditor::Iterator def = unused();
        CodeEditor::Iterator use = unused();

        static CodeEditor::Iterator multiuse() {
            static CodeEditor::Iterator val(-2);
            return val;
        }

        static CodeEditor::Iterator unused() {
            static CodeEditor::Iterator val(-3);
            return val;
        }

        bool operator==(const UseDef& other) const {
            return def == other.def && use == other.use;
        }

        bool mergeWith(const UseDef& other) {
            bool changed = false;
            if (def != multiuse() && other.def != unused() &&
                def != other.def) {
                def = multiuse();
                changed = true;
            }
            if (use != multiuse() && other.use != unused() &&
                use != other.use) {
                use = multiuse();
                changed = true;
            }
            return changed;
        }
    };

    union AValue {
        int argument;
        UseDef u;

        AValue() {}
        AValue(const AValue& v) { *this = v; }
    };
    AValue value;

    bool isValue() { return t == Type::Value || t == Type::Constant; }

    SEXP constant() {
        assert(t == Type::Constant);
        assert(value.u.def != UseDef::unused());
        BC bc = *value.u.def;
        if (bc.bc == BC_t::push_)
            return bc.immediateConst();
        if (bc.bc == BC_t::guard_fun_)
            return Pool::get(bc.immediate.guard_fun_args.expected);
        assert(false);
        return R_NilValue;
    }

    bool pure() const {
        // Type::Value is already evaluated!
        return t != Type::Any && t != Type::Argument;
    }

    bool operator==(FValue const& other) const {
        if (t != other.t)
            return false;

        switch (t) {
        case Type::Constant:
        case Type::Value:
            return value.u == other.value.u;
        case Type::Argument:
            return value.argument == other.value.argument;
        default:
            break;
        }
        return true;
    }

    bool operator!=(FValue const& other) const { return !(*this == other); }

    bool mergeWith(FValue const& other) {
        if (other.t == Type::Bottom)
            return false;

        bool changed = false;

        switch (t) {
        case Type::Bottom:
            t = other.t;
            value = other.value;
            return t != Type::Bottom;
        case Type::Argument:
            if (other.t != Type::Argument ||
                value.argument != other.value.argument) {
                t = Type::Any;
                return true;
            }
            return false;
        case Type::Constant:
            if (other.t != Type::Constant) {
                t = other.pure() ? Type::Value : Type::Any;
                changed = true;
            }
            if (value.u.def != other.value.u.def) {
                t = Type::Value;
                changed = true;
            }
            break;
        case Type::Value:
            if (!other.pure()) {
                t = Type::Any;
                changed = true;
            }
            break;
        case Type::Any:
            break;
        }
        return value.u.mergeWith(other.value.u) || changed;
    }

    static FValue const& top() {
        static FValue value(Type::Any);
        return value;
    }

    static FValue const& bottom() {
        static FValue value(Type::Bottom);
        return value;
    }

    void print() const {
        switch (t) {
        case Type::Constant:
            std::cout << "const ";
            break;
        case Type::Argument:
            std::cout << "arg " << value.argument;
            break;
        case Type::Bottom:
            std::cout << "??";
            break;
        case Type::Value:
            std::cout << "Value";
            break;
        case Type::Any:
            std::cout << "Any";
            break;
        }
    }
};

enum class Type { Conservative, NoReflection };

template <Type type>
class DataflowAnalysis : public ForwardAnalysisIns<AbstractState<FValue>>,
                         public InstructionDispatcher::Receiver {

  public:
    DataflowAnalysis() : dispatcher_(*this) {}

  protected:
    virtual Dispatcher& dispatcher() override { return dispatcher_; }

    AbstractState<FValue>* initialState() override {
        auto* result = new AbstractState<FValue>();
        int i = 0;
        for (SEXP x : code_->arguments()) {
            (*result)[x] = FValue::Argument(i++);
        }
        return result;
    }

    void swap_(CodeEditor::Iterator ins) override {
        auto a = current().pop();
        auto b = current().pop();
        a.used(ins);
        b.used(ins);
        current().push(a);
        current().push(b);
    }

    void pull_(CodeEditor::Iterator ins) override {
        int n = (*ins).immediate.i;
        auto& v = current().stack()[n];
        v.used(ins);
        current().push(v);
    }

    void put_(CodeEditor::Iterator ins) override {
        int n = (*ins).immediate.i;
        auto& v = current().top();
        v.used(ins);
        for (int i = 0; i < n - 1; i++)
            current()[i] = current()[i + i];
        current()[n] = v;
    }

    void ldfun_(CodeEditor::Iterator ins) override {
        SEXP sym = Pool::get((*ins).immediate.pool);
        auto v = current()[sym];

        switch (v.t) {
        case FValue::Type::Constant:
            // If ldfun loads a known function there are no sideeffects,
            // otherwise we cannot be sure, since in the next scope there might
            // be an unevaluated promise with the same name.
            if (TYPEOF(v.constant()) == CLOSXP ||
                TYPEOF(v.constant()) == BUILTINSXP ||
                TYPEOF(v.constant()) == SPECIALSXP) {
                current().push(v);
            } else {
                doCall();
                current().push(FValue::Value(ins));
            }
            break;
        case FValue::Type::Argument:
        case FValue::Type::Any:
            doCall();
            current().push(FValue::Value(ins));
            break;
        case FValue::Type::Value:
            current().push(v);
            break;
        case FValue::Type::Bottom:
            doCall();
            current().push(FValue::Value(ins));
            break;
        }
    }

    void ldvar_(CodeEditor::Iterator ins) override {
        SEXP sym = Pool::get((*ins).immediate.pool);
        auto v = current()[sym];
        if (!v.pure())
            doCall();
        if (v == FValue::bottom())
            current().push(FValue::Value(ins));
        else
            current().push(v.pure() ? v : FValue::Value(ins));
    }

    void ldddvar_(CodeEditor::Iterator ins) override {
        SEXP sym = Pool::get((*ins).immediate.pool);
        auto v = current()[sym];
        if (!v.pure())
            doCall();
        if (v == FValue::bottom())
            current().push(FValue::Value(ins));
        else
            current().push(v.pure() ? v : FValue::Value(ins));
    }

    void ldarg_(CodeEditor::Iterator ins) override {
        SEXP sym = Pool::get((*ins).immediate.pool);
        auto v = current()[sym];
        if (!v.pure())
            doCall();
        if (v == FValue::bottom())
            current().push(FValue::Value(ins));
        else
            current().push(v.pure() ? v : FValue::Value(ins));
    }

    void force_(CodeEditor::Iterator ins) override {
        auto v = current().pop();
        if (v.pure()) {
            current().push(v);
        } else {
            doCall();
            current().push(FValue::Value(ins));
        }
    }

    void stvar_(CodeEditor::Iterator ins) override {
        SEXP sym = Pool::get((*ins).immediate.pool);
        auto v = current().pop();
        current()[sym] = v;
    }

    void call_(CodeEditor::Iterator ins) override {
        current().pop();
        // TODO we could be fancy and if its known whether fun is eager or lazy
        // analyze the promises accordingly
        doCall();
        current().push(FValue::Any());
    }

    void call_stack_(CodeEditor::Iterator ins) override {
        bool noProms = true;
        for (size_t i = 0; i < (*ins).immediate.call_args.nargs; ++i) {
            if (!current().pop().isValue())
                noProms = false;
        }
        if (!noProms)
            doCall();

        auto fun = current().pop();

        bool safeTarget = false;
        if (fun.t == FValue::Type::Constant) {
            if (TYPEOF(fun.constant()) == BUILTINSXP ||
                TYPEOF(fun.constant()) == SPECIALSXP) {
                int prim = fun.constant()->u.primsxp.offset;
                safeTarget = isSafeBuiltin(prim);
            }
        }
        if (!safeTarget) {
            doCall();
            current().push(FValue::Any());
        } else {
            current().push(FValue::Value(ins));
        }
    }

    void static_call_stack_(CodeEditor::Iterator ins) override {
        CallSite cs = ins.callSite();
        SEXP fun = cs.target();

        bool noProms = true;
        for (size_t i = 0; i < (*ins).immediate.call_args.nargs; ++i) {
            if (!current().pop().isValue())
                noProms = false;
        }
        if (!noProms)
            doCall();

        bool safeTarget = false;
        if (TYPEOF(fun) == BUILTINSXP || TYPEOF(fun) == SPECIALSXP) {
            int prim = fun->u.primsxp.offset;
            safeTarget = isSafeBuiltin(prim);
        }
        if (!safeTarget) {
            doCall();
            current().push(FValue::Any());
        } else {
            current().push(FValue::Value(ins));
        }
    }

    void dispatch_(CodeEditor::Iterator ins) override {
        // function
        current().pop();
        doCall();
        current().push(FValue::Any());
    }

    void dispatch_stack_(CodeEditor::Iterator ins) override {
        // function
        current().pop((*ins).immediate.call_args.nargs);
        doCall();
        current().push(FValue::Any());
    }

    void guard_fun_(CodeEditor::Iterator ins) override {
        doCall();
        BC bc = *ins;
        SEXP sym = Pool::get(bc.immediate.guard_fun_args.name);
        // TODO this is not quite right, since its most likely coming from a
        // parent environment
        current()[sym] = FValue::Constant(ins);
    }

    void pick_(CodeEditor::Iterator ins) override {
        int n = (*ins).immediate.i;
        auto& v = current()[n];
        v.used(ins);
        for (int i = 0; i < n - 1; i++)
            current()[i + 1] = current()[i];
        current().top() = v;
    }

    void label(CodeEditor::Iterator ins) override {}

    void doCall() {
        if (type == Type::Conservative)
            current().mergeAllEnv(FValue::Type::Any);
    }

    void uniq_(CodeEditor::Iterator ins) override { current().top().used(ins); }

    void brobj_(CodeEditor::Iterator ins) override {
        current().top().used(ins);
    }

    void any(CodeEditor::Iterator ins) override {
        BC bc = *ins;
        // pop as many as we need, push as many tops as we need
        current().pop(bc.popCount());
        if (!bc.isPure())
            doCall();

        switch (bc.bc) {
        // We know those BC do not return promises
        case BC_t::inc_:
        case BC_t::is_:
        case BC_t::isfun_:
        case BC_t::extract1_:
        case BC_t::subset1_:
        case BC_t::extract2_:
        case BC_t::subset2_:
        case BC_t::close_:
        case BC_t::lgl_or_:
        case BC_t::lgl_and_:
        case BC_t::test_bounds_:
        case BC_t::seq_:
        case BC_t::names_:
        case BC_t::length_:
        case BC_t::alloc_:
            for (size_t i = 0, e = bc.pushCount(); i != e; ++i)
                current().push(FValue::Value(ins));
            break;

        default:
            for (size_t i = 0, e = bc.pushCount(); i != e; ++i)
                current().push(FValue::Any());
            break;
        }
    }

    void dup_(CodeEditor::Iterator ins) override {
        auto& v = current().top();
        v.used(ins);
        current().push(v);
    }

    void dup2_(CodeEditor::Iterator ins) override {
        auto& a = current().stack()[0];
        auto& b = current().stack()[1];
        a.used(ins);
        b.used(ins);
        current().push(a);
        current().push(b);
    }

    void return_(CodeEditor::Iterator ins) override {
        // Return is also a leave instruction
        current().pop(current().stack().depth());
    }

    void alloc_(CodeEditor::Iterator ins) override {
        current().push(FValue::Value(ins));
    }

    void push_(CodeEditor::Iterator ins) override {
        current().push(FValue::Constant(ins));
    }

    InstructionDispatcher dispatcher_;
};
}
#endif