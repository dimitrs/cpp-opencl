#include "BitcodeDisassembler.h"

#include "../sources/CBackend/CTargetMachine.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/PluginLoader.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Target/TargetMachine.h>

#include <memory>
#include <stdexcept>

#include <llvm/IRReader/IRReader.h>

#include <llvm/CodeGen/CommandFlags.h>
#include <llvm/CodeGen/LinkAllAsmWriterComponents.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>


using namespace llvm;

Target llvm::TheCBackendTarget;
RegisterTarget<> X(TheCBackendTarget, "c", "C backend");
RegisterTargetMachine<CTargetMachine> XX(TheCBackendTarget);


namespace compiler
{


class BitcodeDisassemblerImpl
{
public:
    explicit BitcodeDisassemblerImpl(llvm::Module* M);
    virtual ~BitcodeDisassemblerImpl() {}

    /// Return the disassembled bitcode as source-code e.g. CL source
    std::string DisassembleModule();

protected:
    virtual void InitializeTargets();
    virtual void InitializePasses();
    virtual void CreateTargetOptions();
    virtual void CreateTriple();
    virtual void CreateTargetMachine();
    virtual void CreatePassManager();
    virtual std::string RunPass();


    llvm::Module* TheModule;
    llvm::Triple TheTriple;
    TargetMachine* TheTargetMachine;
    llvm::TargetOptions TheTargetOptions;
    llvm::PassManager ThePassMgr;
};

BitcodeDisassemblerImpl::BitcodeDisassemblerImpl(llvm::Module* M) :
    TheModule{M},
    TheTriple{Triple{TheModule->getTargetTriple()}},
    TheTargetMachine{nullptr}
{
}

void BitcodeDisassemblerImpl::InitializeTargets()
{
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmPrinters();
    InitializeAllAsmParsers();
}

void BitcodeDisassemblerImpl::InitializePasses()
{
    PassRegistry* Registry = PassRegistry::getPassRegistry();
    initializeCore(*Registry);
    initializeCodeGen(*Registry);
    initializeLoopStrengthReducePass(*Registry);
    initializeLowerIntrinsicsPass(*Registry);
    initializeUnreachableBlockElimPass(*Registry);
}

void BitcodeDisassemblerImpl::CreateTargetOptions()
{
    TheTargetOptions.LessPreciseFPMADOption = EnableFPMAD;
    TheTargetOptions.NoFramePointerElim = DisableFPElim;
    TheTargetOptions.NoFramePointerElimNonLeaf = DisableFPElimNonLeaf;
    TheTargetOptions.AllowFPOpFusion = FuseFPOps;
    TheTargetOptions.UnsafeFPMath = EnableUnsafeFPMath;
    TheTargetOptions.NoInfsFPMath = EnableNoInfsFPMath;
    TheTargetOptions.NoNaNsFPMath = EnableNoNaNsFPMath;
    TheTargetOptions.HonorSignDependentRoundingFPMathOption = EnableHonorSignDependentRoundingFPMath;
    TheTargetOptions.UseSoftFloat = GenerateSoftFloatCalls;
    if (FloatABIForCalls != FloatABI::Default)
        TheTargetOptions.FloatABIType = FloatABIForCalls;
    TheTargetOptions.NoZerosInBSS = DontPlaceZerosInBSS;
    TheTargetOptions.GuaranteedTailCallOpt = EnableGuaranteedTailCallOpt;
    TheTargetOptions.DisableTailCalls = DisableTailCalls;
    TheTargetOptions.StackAlignmentOverride = OverrideStackAlignment;
    TheTargetOptions.RealignStack = EnableRealignStack;
    TheTargetOptions.TrapFuncName = TrapFuncName;
    TheTargetOptions.PositionIndependentExecutable = EnablePIE;
    TheTargetOptions.EnableSegmentedStacks = SegmentedStacks;
    TheTargetOptions.UseInitArray = UseInitArray;
    TheTargetOptions.SSPBufferSize = SSPBufferSize;
}

void BitcodeDisassemblerImpl::CreateTriple()
{
    if (TheTriple.getTriple().empty())
        TheTriple.setTriple(sys::getDefaultTargetTriple());
}

void BitcodeDisassemblerImpl::CreateTargetMachine()
{
    std::string Error{};
    const llvm::Target* TheTarget = TargetRegistry::lookupTarget("c", TheTriple, Error);
    if (!TheTarget) {
        throw std::runtime_error(Error);
    }

    // Package up features to be passed to target/subtarget
    std::string FeaturesStr;
    if (MAttrs.size()) {
        SubtargetFeatures Features;
        for (unsigned i = 0; i != MAttrs.size(); ++i)
            Features.AddFeature(MAttrs[i]);
        FeaturesStr = Features.getString();
    }

    CodeGenOpt::Level OLvl = CodeGenOpt::Default; // Determine optimization level

    TheTargetMachine = TheTarget->createTargetMachine(
                TheTriple.getTriple(), MCPU, FeaturesStr,
                TheTargetOptions, RelocModel, CMModel, OLvl);
    assert(TheTargetMachine && "Could not allocate target machine!");

    if (DisableDotLoc)
        TheTargetMachine->setMCUseLoc(false);

    if (DisableCFI)
        TheTargetMachine->setMCUseCFI(false);

    if (EnableDwarfDirectory)
        TheTargetMachine->setMCUseDwarfDirectory(true);

    if (GenerateSoftFloatCalls)
        FloatABIForCalls = FloatABI::Soft;

    // Disable .loc support for older OS X versions.
    if (TheTriple.isMacOSX() &&
      TheTriple.isMacOSXVersionLT(10, 6))
    TheTargetMachine->setMCUseLoc(false);

    // Override default to generate verbose assembly.
    TheTargetMachine->setAsmVerbosityDefault(true);

    if (RelaxAll) {
        if (FileType != TargetMachine::CGFT_ObjectFile)
            errs() << "warning: ignoring -mc-relax-all because filetype != obj";
        else
            TheTargetMachine->setMCRelaxAll(true);
    }
}

void BitcodeDisassemblerImpl::CreatePassManager()
{
    // Add an appropriate TargetLibraryInfo pass for the module's triple.
    TargetLibraryInfo *TLI = new TargetLibraryInfo(TheTriple);
    ThePassMgr.add(TLI);

    // Add intenal analysis passes from the target machine.
    TheTargetMachine->addAnalysisPasses(ThePassMgr);

    // Add the target data from the target machine, if it exists, or the module.
    if (const DataLayout *TD = TheTargetMachine->getDataLayout())
        ThePassMgr.add(new DataLayout(*TD));
    else
        ThePassMgr.add(new DataLayout(TheModule));
}

std::string BitcodeDisassemblerImpl::RunPass()
{
    std::string output;

    raw_string_ostream B{output};
    formatted_raw_ostream FOS{B};
    AnalysisID StartAfterID = 0;
    AnalysisID StopAfterID = 0;
    cl::opt<bool> NoVerify("disable-verify", cl::Hidden, cl::desc("Do not verify input module"));
    if (TheTargetMachine->addPassesToEmitFile(ThePassMgr, FOS, FileType, NoVerify, StartAfterID, StopAfterID)) {
        errs() <<  ": target does not support generation of this"
               << " file type!\n";
        return "";
    }

    //cl::PrintOptionValues();
    ThePassMgr.run(*TheModule);
    FOS.flush();

    return output;
}

std::string BitcodeDisassemblerImpl::DisassembleModule()
{
    assert(TheModule!=nullptr);
static bool count = true;
if (count) {
    InitializeTargets();
    InitializePasses();
    CreateTriple();
    CreateTargetOptions();
    CreateTargetMachine();
    count = false;
}
    return RunPass();
}

BitcodeDisassembler::BitcodeDisassembler(llvm::Module* M) :
    TheDisassembler{new BitcodeDisassemblerImpl(M)}
{
}

BitcodeDisassembler::~BitcodeDisassembler()
{
}

std::string BitcodeDisassembler::DisassembleModule()
{
    std::string Output = TheDisassembler->DisassembleModule();
    return Output;
}

}



