#ifndef BitcodeDisassembler_H
#define BitcodeDisassembler_H

#include <memory>
#include <string>

namespace llvm
{
    class Module;
}

namespace compiler
{


class BitcodeDisassemblerImpl;

// Convert LLVM byte code to platform-specific 'CL' code
class BitcodeDisassembler
{
public:
    BitcodeDisassembler(const BitcodeDisassembler& that) = delete;
    BitcodeDisassembler& operator=(BitcodeDisassembler&) = delete;

    explicit BitcodeDisassembler(llvm::Module* M);
    virtual ~BitcodeDisassembler();

    /// Return the disassembled bitcode as source-code e.g. CL source
    std::string DisassembleModule();

protected:
    std::shared_ptr<BitcodeDisassemblerImpl> TheDisassembler;
};


} // namespace compiler

#endif
