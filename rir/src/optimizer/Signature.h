#pragma once

#include "code/analysis.h"

namespace rir {

enum class Bool3 {
    no,
    yes,
    maybe,
};

inline std::ostream & operator << (std::ostream & stream, Bool3 val) {
    switch (val) {
        case Bool3::no:
            stream << "no";
            break;
        case Bool3::yes:
            stream << "yes";
            break;
        case Bool3::maybe:
            stream << "maybe";
            break;
    }
    return stream;
}

/** Argument evaluation record.


 */
class ArgumentEvaluation {
public:

    /** Determines whether the promise associated with the argument has been evaluated.
     */
    Bool3 forced;

    /** Determines whether currently, the variable still contains the original value.
     */
    Bool3 contains;

    /** Merges two argument evaluation records, returns true if the source changed.

      forced n m y
         n   n m m
         m   m m m
         y   m m y

      contains n m y
         n     n m m
         m     m m m
         y     m m y

     */
    bool mergeWith(ArgumentEvaluation const & other) {
        bool result = false;
        if (forced != Bool3::maybe and forced != other.forced) {
            result = true;
            forced = Bool3::maybe;
        }
        if (contains != Bool3::maybe and contains != other.contains) {
            result = true;
            contains = Bool3::maybe;
        }
        return result;
    }

    ArgumentEvaluation():
        forced(Bool3::no),
        contains(Bool3::yes) {
    }

private:
};

class Signature : public State {
public:
    bool isLeaf() const {
        return leaf_;
    }

    Bool3 isArgumentEvaluated(SEXP name) {
        auto i = argumentEvaluation_.find(name);
        assert(i != argumentEvaluation_.end() and "Not an argument");
        return i->second.forced;
    }

    Signature * clone() const {
        return new Signature(*this);
    }

    bool mergeWith(State const * other) override {
        Signature const & sig = * dynamic_cast<Signature const *>(other);
        bool result = false;
        if (leaf_ and not sig.leaf_) {
            leaf_ = false;
            result = true;
        }
        for (auto & i : argumentEvaluation_) {
            auto const & e = * (sig.argumentEvaluation_.find(i.first));
            result = i.second.mergeWith(e.second) or result;
        }
        return result;
    }

    Signature():
        leaf_(true) {
    }

    Signature(CodeEditor const & code):
        leaf_(true) {
        for (SEXP i : code.arguments())
            argumentEvaluation_[i] = ArgumentEvaluation();
    }

    Signature(Signature const &) = default;

    void print() const {
        Rprintf("Leaf function:       ");
        if (isLeaf())
            Rprintf("yes\n");
        else
            Rprintf("no\n");
        Rprintf("Argument evaluation: \n");
        for (auto i : argumentEvaluation_) {
            Rprintf("    ");
            Rprintf(CHAR(PRINTNAME(i.first)));
            switch (i.second.forced) {
                case Bool3::no:
                    Rprintf(" no\n");
                    break;
                case Bool3::maybe:
                    Rprintf(" maybe\n");
                    break;
                case Bool3::yes:
                    Rprintf(" yes\n");
                    break;
            }
        }
    }

protected:
    friend class SignatureAnalysis;

    void setAsNotLeaf() {
        leaf_ = false;
    }

    void forceArgument(SEXP name) {
        auto i = argumentEvaluation_.find(name);
        if (i == argumentEvaluation_.end())
            return;
        if (i->second.contains == Bool3::no)
            return;
        if (i->second.contains == Bool3::maybe and i->second.forced == Bool3::no)
            i->second.forced = Bool3::maybe;
        else if (i->second.contains == Bool3::yes)
            i->second.forced = Bool3::yes;

    }

    void storeArgument(SEXP name) {
        auto i = argumentEvaluation_.find(name);
        if (i == argumentEvaluation_.end())
            return;
        i->second.contains = Bool3::no;
    }


private:

    bool leaf_;

    std::map<SEXP, ArgumentEvaluation> argumentEvaluation_;


};




class SignatureAnalysis : public ForwardAnalysisFinal<Signature>, InstructionVisitor::Receiver {
public:
    void print() override {
        finalState().print();
    }

    SignatureAnalysis():
        dispatcher_(InstructionVisitor(*this)) {
    }

protected:

    void ldfun_(CodeEditor::Cursor ins) override {
        current().forceArgument(ins.bc().immediateConst());
    }

    void ldddvar_(CodeEditor::Cursor ins) override {

    }

    void ldvar_(CodeEditor::Cursor ins) override {
        current().forceArgument(ins.bc().immediateConst());
    }

    void call_(CodeEditor::Cursor ins) override {
        current().setAsNotLeaf();
    }

    void dispatch_(CodeEditor::Cursor ins) override {

    }

    void dispatch_stack_(CodeEditor::Cursor ins) override {

    }

    void call_stack_(CodeEditor::Cursor ins) override {

    }

    void stvar_(CodeEditor::Cursor ins) override {
        current().storeArgument(ins.bc().immediateConst());
    }


    Dispatcher & dispatcher() override {
        return dispatcher_;
    }

protected:
    Signature * initialState() override {
        return new Signature(*code_);

    }

private:
    InstructionVisitor dispatcher_;



};

}
