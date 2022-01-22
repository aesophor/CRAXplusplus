// Copyright 2021-2022 Software Quality Laboratory, NYCU.
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

#include <s2e/Plugins/CRAX/CRAX.h>

#include "DynamicRop.h"

using namespace klee;

namespace s2e::plugins::crax {

DynamicRop::DynamicRop()
    : Module(),
      m_constraints() {
    g_crax->beforeExploitGeneration.connect(
            sigc::mem_fun(*this, &DynamicRop::beforeExploitGeneration));
}


DynamicRop &DynamicRop::addConstraint(const DynamicRop::Constraint &c) {
    m_constraints.push_back(c);
    return *this;
}

void DynamicRop::scheduleConstraints() {
    auto modState = g_crax->getPluginModuleState(g_crax->getCurrentState(), this);
    modState->constraintsQueue.push(std::move(m_constraints));
}


void DynamicRop::applyNextConstraint() {
    S2EExecutionState *state = g_crax->getCurrentState();
    auto modState = g_crax->getPluginModuleState(state, this);

    if (modState->constraintsQueue.empty()) {
        log<WARN>() << "modState->constraintsQueue empty...\n";
        return;
    }

    bool ok;
    bool hasControlFlowChanged = false;
    const auto &rop = g_crax->getExploitGenerator().getRopChainBuilder();

    log<WARN>() << "adding constraints from modState->constraintsQueue...\n";
    for (const auto &c : modState->constraintsQueue.front()) {
        if (const auto rc = std::get_if<RegisterConstraint>(&c)) {
            ok = rop.addRegisterConstraint(rc->reg, rc->e, /*rewriteSymbolic=*/true);
            hasControlFlowChanged |= rc->reg == Register::X64::RIP;
        } else if (const auto mc = std::get_if<MemoryConstraint>(&c)) {
            ok = rop.addMemoryConstraint(mc->addr, mc->e, /*rewriteSymbolic=*/true);
        }

        if (!ok) {
            g_s2e->getExecutor()->terminateState(*state, "Dynamic ROP failed");
        }
    }

    modState->constraintsQueue.pop();

    // Invalidate current translation block.
    if (hasControlFlowChanged) {
        throw CpuExitException();
    }
}

void DynamicRop::beforeExploitGeneration(S2EExecutionState *state) {
    applyNextConstraint();
}

}  // namespace s2e::plugins::crax