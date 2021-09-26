// Copyright (C) 2021-2022, Marco Wang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Requiem.h"

#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>

#include <capstone/capstone.h>
#include <pybind11/embed.h>

namespace py = pybind11;

namespace s2e::plugins::requiem {

namespace {

// This class can optionally be used to store per-state plugin data.
//
// Use it as follows:
// void ExploitGenerator::onEvent(S2EExecutionState *state, ...) {
//     DECLARE_PLUGINSTATE(RequiemState, state);
//     plgState->...
// }

class RequiemState: public PluginState {
    // Declare any methods and fields you need here

public:
    static PluginState *factory(Plugin *p, S2EExecutionState *s) {
        return new RequiemState();
    }

    virtual ~RequiemState() {
        // Destroy any object if needed
    }

    virtual RequiemState *clone() const {
        return new RequiemState(*this);
    }
};

}  // namespace


S2E_DEFINE_PLUGIN(Requiem, "Automatic Exploit Generation Engine", "", );

void Requiem::initialize() {
    m_monitor = static_cast<OSMonitor *>(s2e()->getPlugin("OSMonitor"));
    m_monitor->onProcessLoad.connect(sigc::mem_fun(*this, &Requiem::hookInstructions));

    s2e()->getCorePlugin()->onSymbolicAddress.connect(sigc::mem_fun(*this, &Requiem::onRipCorrupt));

    py::scoped_interpreter guard{}; // start the interpreter and keep it alive
    py::print("Hello, World!"); // use the Python API
}


void Requiem::onRipCorrupt(S2EExecutionState *state,
                           klee::ref<klee::Expr> virtualAddress,
                           uint64_t concreteAddress,
                           bool &concretize,
                           CorePlugin::symbolicAddressReason reason) {
    s2e()->getWarningsStream(state)
        << "Detected symbolic RIP: " << klee::hexval(concreteAddress)
        << ", original value is: " << klee::hexval(state->regs()->getPc()) << "\n";

    s2e()->getExecutor()->terminateState(*state, "End of exploit generation");
}

void Requiem::hookInstructions(S2EExecutionState *state,
                               uint64_t cr3,
                               uint64_t pid,
                               const std::string &imageFileName) {
    if (imageFileName.find("readme") != imageFileName.npos) {
        s2e()->getInfoStream() << "hooking instructions\n";

        s2e()->getCorePlugin()->onTranslateInstructionEnd.connect(
                sigc::mem_fun(*this, &Requiem::onTranslateInstructionEnd));
    }
}

void Requiem::onTranslateInstructionEnd(ExecutionSignal *onInstructionExecute,
                                        S2EExecutionState *state,
                                        TranslationBlock *tb,
                                        uint64_t pc) {
    if (pc == 0x401126) {
        s2e()->getWarningsStream(state) << "reached main()\n";
    }

    if (pc >= m_monitor->getKernelStart()) {
        return;
    }

    onInstructionExecute->connect(sigc::mem_fun(*this, & Requiem::instructionHook));
}

void Requiem::instructionHook(S2EExecutionState *state,
                              uint64_t pc) {
    uint8_t code[16] = {};
    csh handle;
    cs_insn *insn;
    size_t count;

    if (!state->mem()->read(pc, code, sizeof(code) - 1)) {
        s2e()->getWarningsStream() << "cannot read from memory at: " << klee::hexval(pc) << "\n";
        return;
    }

    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) {
        return;
    }

    count = cs_disasm(handle, code, sizeof(code) - 1, pc, 0, &insn);

    if (count) {
        //s2e()->getInfoStream() << "" << klee::hexval(insn[0].address) << ": "
        //    << insn[0].mnemonic << " " << insn[0].op_str << "\n";

        if (!strcmp(insn[0].mnemonic, "syscall")) {
            syscallHook(state, pc);
        }

        cs_free(insn, count);
    }

    cs_close(&handle);
}

void Requiem::syscallHook(S2EExecutionState *state,
                          uint64_t pc) {
    uint64_t rax = state->regs()->read<uint64_t>(CPU_OFFSET(regs[R_EAX]));
    uint64_t rdi = state->regs()->read<uint64_t>(CPU_OFFSET(regs[R_EDI]));
    uint64_t rsi = state->regs()->read<uint64_t>(CPU_OFFSET(regs[R_ESI]));
    uint64_t rdx = state->regs()->read<uint64_t>(CPU_OFFSET(regs[R_EDX]));
    uint64_t r10 = state->regs()->read<uint64_t>(CPU_OFFSET(regs[10]));
    uint64_t r8 = state->regs()->read<uint64_t>(CPU_OFFSET(regs[8]));
    uint64_t r9 = state->regs()->read<uint64_t>(CPU_OFFSET(regs[9]));

    s2e()->getInfoStream(state)
        << "syscall: " << klee::hexval(rax) << " ("
        << klee::hexval(rdi) << ", " << klee::hexval(rsi) << ", "
        << klee::hexval(rdx) << ", " << klee::hexval(r10) << ", "
        << klee::hexval(r8) << ", " << klee::hexval(r9) << ")\n";
}


void Requiem::handleOpcodeInvocation(S2EExecutionState *state,
                                     uint64_t guestDataPtr,
                                     uint64_t guestDataSize) {
    S2E_REQUIEM_COMMAND command;

    if (guestDataSize != sizeof(command)) {
        getWarningsStream(state) << "mismatched S2E_REQUIEM_COMMAND size\n";
        return;
    }

    if (!state->mem()->read(guestDataPtr, &command, guestDataSize)) {
        getWarningsStream(state) << "could not read transmitted data\n";
        return;
    }

    switch (command.Command) {
        // TODO: add custom commands here
        case COMMAND_1:
            break;
        default:
            getWarningsStream(state) << "Unknown command " << command.Command << "\n";
            break;
    }
}

}  // namespace s2e::plugins::requiem
