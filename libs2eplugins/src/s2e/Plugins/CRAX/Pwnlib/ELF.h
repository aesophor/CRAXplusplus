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

#ifndef S2E_PLUGINS_CRAX_PWNLIB_ELF_H
#define S2E_PLUGINS_CRAX_PWNLIB_ELF_H

#include <s2e/Plugins/CRAX/Pwnlib/Function.h>
#include <pybind11/embed.h>

#include <map>
#include <string>

namespace s2e::plugins::crax {

class ELF {
public:
    using SymbolMap = std::map<std::string, uint64_t>;
    using FunctionMap = std::map<std::string, Function>;

    ELF(pybind11::module pwnlib,
        const std::string &filename); 

    SymbolMap symbols() const;
    SymbolMap got() const;
    FunctionMap functions() const;
    uint64_t bss() const;

    uint64_t getBase() const { return m_base; }
    void setBase(uint64_t base) { m_base= base; }

private:
    pybind11::module m_pwnlib;
    pybind11::object m_elf;

    uint64_t m_base;
};

}  // namespace s2e::plugins::crax

#endif  // S2E_PLUGINS_CRAX_PWNLIB_ELF_H