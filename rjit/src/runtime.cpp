


#include "runtime.h"
#include "ICCompiler.h"
#include "stack_map.h"

namespace rjit {


void patchIC(void * ic, uint64_t stackmapId, void * caller) {
    auto r = StackMap::getPatchpoint(stackmapId);
    assert(r.getNumLocations() == 1);

    uint8_t * patchAddr = (uint8_t*) ((uintptr_t) caller + r.getInstructionOffset());

    int reg = r.getLocation(0).getDwarfRegNum();

    uint8_t prefix = reg > 7 ? 0x49 : 0x48;
    uint8_t movinst = 0xb8 + (reg % 8);

    static_assert(patchpointSize == 10, "require 10 bytes to patch call");

    *patchAddr++ = prefix;
    *patchAddr++ = movinst;
    *(void**)(patchAddr) = ic;
}

void * compileIC(uint64_t numargs, SEXP call, SEXP fun, SEXP rho, uint64_t stackmapId) {
    JITModule m("ic");

    ICCompiler compiler(stackmapId, numargs, m, nextStackmapId++);

    return compiler.compile(call, fun, rho);
}

}
