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

#ifndef S2E_PLUGINS_CRAX_DEFAULT_STRATEGY_H
#define S2E_PLUGINS_CRAX_DEFAULT_STRATEGY_H

#include <s2e/Plugins/CRAX/Modules/Strategies/Strategy.h>

namespace s2e::plugins::crax {

// Forward declaration
class CRAX;

// Default exploitation strategy:
//
// 1. migrate the stack to .bss
// 2. use ret2csu to partially overwrite read@GOT to point to `syscall` gadget.
// 3. use ret2csu to call sys_execve("/bin/sh", 0, 0)
class DefaultStrategy : public Strategy {
public:
    explicit DefaultStrategy(CRAX &ctx);
    virtual ~DefaultStrategy() = default;
};

}  // namespace s2e::plugins::crax

#endif  // S2E_PLUGINS_CRAX_STRATEGY_H