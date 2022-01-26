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

#ifndef S2E_PLUGINS_CRAX_EXPR_H
#define S2E_PLUGINS_CRAX_EXPR_H

#include <s2e/Plugins/CRAX/Exploit.h>
#include <s2e/Plugins/CRAX/Pwnlib/ELF.h>
#include <s2e/Plugins/CRAX/Utils/StringUtil.h>

#include <klee/Expr.h>

#include <cassert>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>

using s2e::plugins::crax::format;


namespace klee {

// This is CRAX's extension to klee.


// In a generated exploit script, each line contains a statement such as:
//
// 1. payload = p64(0x401060)
// 2. payload = p64(elf_base + elf.sym['read'] + 0x30 * 2)
// 3. payload = p64(__libc_csu_init_gadget1)
//
// Analysis:
//
// 1. The simple 0x401060 can be represented by a klee::ConstantExpr.
// 2. `elf_base + elf.sym['read']` will be represented by our BaseOffsetExpr
//     which essentially is a klee::AddExpr.
// 3. `__libc_csu_init_gadget1` is a symbol used by the script itself
//     and can be rewritten as `elf_base + __libc_csu_init_gadget1`,
//     so essentially it is also a klee::AddExpr.
class BaseOffsetExpr : public AddExpr {
    using Exploit = s2e::plugins::crax::Exploit;
    using ELF = s2e::plugins::crax::ELF;

public:
    virtual ~BaseOffsetExpr() = default;

    virtual unsigned getNumKids() const override {
        return 0;
    }

    virtual ref<Expr> getKid(unsigned i) const override {
        return nullptr;
    }

    static ref<Expr> alloc(const ref<ConstantExpr> &lce,
                           const ref<ConstantExpr> &rce,
                           const std::string &strBase,
                           const std::string &strOffset) {
        return ref<Expr>(new BaseOffsetExpr(lce, rce, strBase, strOffset));
    }

    static ref<Expr> create(uint64_t base,
                            uint64_t offset,
                            std::string strBase = "",
                            std::string strOffset = "") {
        auto lce = ConstantExpr::create(base, Expr::Int64);
        auto rce = ConstantExpr::create(offset, Expr::Int64);

        return alloc(lce, rce, strBase, strOffset);
    }

    // Create a BaseOffsetExpr that represents an offset from elf_base,
    // E.g. "elf_base + 0x666"
    static ref<Expr> create(const ELF &elf, uint64_t offset) {
        return create(elf.getBase(), offset, "elf_base", std::to_string(offset));
    }

    // Create a BaseOffsetExpr that represents an offset from elf_base,
    // where the offset is a variable declared in the exploit script's symbol table.
    // E.g., "elf_base + __libc_csu_init_gadget1"
    // XXX: The base is hardcoded to elf_base for now...
    static ref<Expr> create(const Exploit &exploit, std::string var = "") {
        auto it = exploit.getSymtab().find(var);
        assert(it != exploit.getSymtab().end() && "Var doesn't exist in script's symtab");

        return create(exploit.getElf().getBase(), it->second, "elf_base", var);
    }

    // Create a BaseOffsetExpr that represents one of the following:
    // 1. "elf_base + elf.sym['read']"  <- `BaseOffsetExpr::create(elf, "sym", "read")`
    // 2. "elf_base + elf.got['read']"  <- `BaseOffsetExpr::create(elf, "got", "read")`
    // 3. "elf_base + elf.bss()"        <- `BaseOffsetExpr::create(elf, "bss")`
    // XXX: Add support for libc base/offset
    static ref<Expr> create(const ELF &elf,
                            const std::string &base,
                            const std::string &symbol = "") {
        uint64_t offset = 0;
        std::string strOffset;

        if (base == "sym") {
            const auto &symbolMap = elf.symbols();
            auto it = symbolMap.find(symbol);
            assert(it != symbolMap.end() && "Symbol doesn't exist in elf.sym");
            offset = it->second;
            strOffset = format("elf.sym['%s']", symbol.c_str());

        } else if (base == "got") {
            const auto &gotMap = elf.got();
            auto it = gotMap.find(symbol);
            assert(it != gotMap.end() && "Symbol doesn't exist in elf.got");
            offset = it->second;
            strOffset = format("elf.got['%s']", symbol.c_str());

        } else if (base == "bss") {
            offset = elf.bss();
            strOffset = "elf.bss()";

        } else {
            pabort("Unsupported type of `base`");
        }

        return create(elf.getBase(), offset, "elf_base", std::move(strOffset));
    }

    // For the symbol (identifier) of the exploit script.
    static ref<Expr> create(const std::string &symbol, uint64_t value) {
        return create(0, value, "", symbol);
    }

    // Method for support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const Expr *E) {
        // XXX: The normal way of implementing BaseOffsetExpr::classof() is
        // adding our typeinfo to klee::Expr::Kind enum. However, since I don't
        // want to touch klee's source code, I'll simply forward the job
        // to C++'s vtable. Perhaps this can be optimized later.
        return dynamic_cast<const BaseOffsetExpr *>(E) != nullptr;
    }

    // Method for support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const BaseOffsetExpr *) {
        return true;
    }

    ref<ConstantExpr> toConstantExpr() const {
        auto lce = dyn_cast<ConstantExpr>(AddExpr::getKid(0));
        auto rce = dyn_cast<ConstantExpr>(AddExpr::getKid(1));

        assert(lce && rce);
        return lce->Add(rce);
    }

    std::string toString() const {
        if (m_strBase.size() && m_strOffset.size()) {
            return m_strBase + " + " + m_strOffset;
        }
        return (m_strBase.size()) ? m_strBase : m_strOffset;
    }
 
    uint64_t getZExtValue() const {
        return toConstantExpr()->getZExtValue();
    }
   
private:
    BaseOffsetExpr(const ref<ConstantExpr> &lce,
                   const ref<ConstantExpr> &rce,
                   const std::string &strBase,
                   const std::string &strOffset)
        : AddExpr(lce, rce),
          m_strBase(strBase),
          m_strOffset(strOffset) {
        assert(strBase.size() || strOffset.size());
    }

    std::string m_strBase;
    std::string m_strOffset;
};


// In ret2csu, we need to have placeholder expr tree node.
class PlaceholderExpr : public Expr {
public:
    virtual ~PlaceholderExpr() = default;

    virtual Kind getKind() const override {
        // Under normal circumstances, this expr shouldn't exist.
        // It is supposed to be replaced sometime before expr evaluation.
        return Expr::InvalidKind;
    }

    virtual Width getWidth() const override {
        return Expr::InvalidWidth;
    }

    virtual unsigned getNumKids() const override {
        return 0;
    }

    virtual ref<Expr> getKid(unsigned i) const override {
        return nullptr;
    }

    virtual ref<Expr> rebuild(ref<Expr> kids[]) const override {
        std::abort();
    }

    static ref<Expr> alloc(const std::string &tag) {
        return ref<Expr>(new PlaceholderExpr(tag));
    }

    static ref<Expr> create(const std::string &tag) {
        return alloc(tag);
    }

    // Method for support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const Expr *E) {
        return dynamic_cast<const PlaceholderExpr*>(E) != nullptr;
    }

    // Method for support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const PlaceholderExpr *) {
        return true;
    }

    const std::string &getTag() const {
        return m_tag;
    }

    bool hasTag(const std::string &tag) const {
        return m_tag.find(tag) != m_tag.npos;
    }

    void setTag(const std::string &tag) {
        m_tag = tag;
    }

private:
    PlaceholderExpr(const std::string &tag) : Expr(), m_tag(tag) {}

    std::string m_tag;
};


// Sometimes we want to send a sequence of bytes whose size is not simply a QWORD.
class ByteVectorExpr : public Expr {
public:
    virtual ~ByteVectorExpr() = default;

    virtual Kind getKind() const override {
        return Expr::Constant;
    }

    virtual Width getWidth() const override {
        // XXX: the width of byte vector is not fixed,
        // but maybe we can return something like Expr::Int32 or Expr::Int64
        // according to m_bytes.size()?
        return Expr::InvalidWidth;
    }

    virtual unsigned getNumKids() const override {
        return 0;
    }

    virtual ref<Expr> getKid(unsigned i) const override {
        return nullptr;
    }

    virtual ref<Expr> rebuild(ref<Expr> kids[]) const override {
        std::abort();
    }

    static ref<Expr> alloc(const std::vector<uint8_t> &bytes) {
        return ref<Expr>(new ByteVectorExpr(bytes));
    }

    static ref<Expr> create(const std::vector<uint8_t> &bytes) {
        return alloc(bytes);
    }

    static ref<Expr> create(const std::string &s) {
        std::vector<uint8_t> bytes(s.begin(), s.end());
        return alloc(bytes);
    }

    // Method for support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const Expr *E) {
        return dynamic_cast<const ByteVectorExpr*>(E) != nullptr;
    }

    // Method for support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const ByteVectorExpr *) {
        return true;
    }

    const std::vector<uint8_t> &getBytes() const {
        return m_bytes;
    }

    std::string toString() const {
        std::string ret = "b'";
        for (auto __byte : m_bytes) {
            ret += format("\\x%02x", __byte);
        }
        ret += '\'';
        return ret;
    }

private:
    ByteVectorExpr(const std::vector<uint8_t> &bytes) : Expr(), m_bytes(bytes) {}

    std::vector<uint8_t> m_bytes;
};

}  // namespace klee

#endif  // S2E_PLUGINS_CRAX_EXPR_H
