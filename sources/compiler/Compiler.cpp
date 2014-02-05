
#include "Compiler.h"
#include "BitcodeDisassembler.h"
#include "Rewriter.h"

#include <memory>
#include <vector>
#include <set>
#include <ctime>
#include <cstdio>
#include <fstream>

#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Driver/DriverDiagnostic.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/FrontendDiagnostic.h>
#include <clang/Frontend/TextDiagnosticBuffer.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/FrontendTool/Utils.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Timer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Host.h>
#include <llvm/Target/TargetOptions.h>


using namespace llvm;
using namespace std;
using namespace clang;
using namespace llvm::opt;


namespace {



static void LLVMErrorHandler(void *UserData, const std::string &Message,
                             bool GenCrashDiag)
{
    DiagnosticsEngine& Diags = *static_cast<DiagnosticsEngine*>(UserData);

    Diags.Report(diag::err_fe_error_backend) << Message;

    // Run the interrupt handlers to make sure any special cleanups get done, in
    // particular that we remove files registered with RemoveFileOnSignal.
    llvm::sys::RunInterruptHandlers();

    // We cannot recover from llvm errors.  When reporting a fatal error, exit
    // with status 70 to generate crash diagnostics.  For BSD systems this is
    // defined as an internal software error.  Otherwise, exit with status 1.
    exit(GenCrashDiag ? 70 : 1);
}

void InitializeTargets()
{
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
}

std::string GetExecutablePath(const char *Argv0, bool CanonicalPrefixes) {
  if (!CanonicalPrefixes)
    return Argv0;

  // This just needs to be some symbol in the binary; C++ doesn't
  // allow taking the address of ::main however.
  void *P = (void*) (intptr_t) GetExecutablePath;
  return llvm::sys::fs::getMainExecutable(Argv0, P);
}


OwningPtr<CompilerInstance> CreateCompilerInvocation(SmallVector<const char*, 256>& Args, DiagnosticConsumer* DiagsBuffer)
{
    const char** ArgBegin = Args.data()+2;
    const char** ArgEnd = Args.data()+Args.size();
    const char* ExecutablePath = Args[0];

    OwningPtr<CompilerInstance> Clang { new CompilerInstance() };

    IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
    IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
    TextDiagnosticBuffer *DiagsB = new TextDiagnosticBuffer;
    //IgnoringDiagConsumer *DiagsB = new IgnoringDiagConsumer;
    DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsB);
    bool Success;
    Success = CompilerInvocation::CreateFromArgs(Clang->getInvocation(),
                                                 ArgBegin, ArgEnd, Diags);

    void *MainAddr = (void*) (intptr_t) GetExecutablePath;

    // Infer the builtin include path if unspecified.
    if (Clang->getHeaderSearchOpts().UseBuiltinIncludes &&
        Clang->getHeaderSearchOpts().ResourceDir.empty())
      Clang->getHeaderSearchOpts().ResourceDir =
        CompilerInvocation::GetResourcesPath(ExecutablePath, MainAddr);

    // Create the actual diagnostics engine.
    if (DiagsBuffer)
        Clang->createDiagnostics();
    else
        Clang->createDiagnostics(new IgnoringDiagConsumer);
    if (!Clang->hasDiagnostics())
        return OwningPtr<CompilerInstance>();

    DiagsB->FlushDiagnostics(Clang->getDiagnostics());
    if (!Success) {
        return OwningPtr<CompilerInstance>();
    }
    return Clang;
}

void CreateTarget(OwningPtr<CompilerInstance>& Clang)
{
    clang::TargetOptions* TO = new clang::TargetOptions;
    TO->Triple = llvm::sys::getDefaultTargetTriple();
    Clang->setTarget(clang::TargetInfo::CreateTargetInfo(Clang->getDiagnostics(), TO));
}

void CreateFileManager(OwningPtr<CompilerInstance>& Clang)
{
    Clang->createFileManager();
    FileManager& FileMgr = Clang->getFileManager();
    Clang->createSourceManager(FileMgr);
}

void CreatePreprocessor(OwningPtr<CompilerInstance>& Clang)
{
    Clang->createPreprocessor();
    Preprocessor &PP = Clang->getPreprocessor();
    PP.getBuiltinInfo().InitializeBuiltins(PP.getIdentifierTable(), PP.getLangOpts());
}

void CreateAST(OwningPtr<CompilerInstance>& Clang)
{
    Clang->createASTContext();
}

void CreateMainFile(OwningPtr<CompilerInstance>& Clang, const char* SourceFileName)
{
    Clang->getSourceManager().createMainFileID(Clang->getFileManager().getFile(SourceFileName));
    Clang->getDiagnosticClient().BeginSourceFile(Clang->getLangOpts(), &Clang->getPreprocessor());
}

void InstallFatalErrorHandler(OwningPtr<CompilerInstance>& Clang)
{
    llvm::remove_fatal_error_handler();
    // Set an error handler, so that any LLVM backend diagnostics go through our
    // error handler.
    llvm::install_fatal_error_handler(LLVMErrorHandler,
                                    static_cast<void*>(&Clang->getDiagnostics()));
}

std::string GetSourceFileName(SmallVector<const char*, 256>& Args)
{
    SmallVector<const char*, 256>::const_iterator it =
            find_if(begin(Args), end(Args), [](const char* s) { return string(s) == string("-c"); } );
    if (it!=end(Args)) return *++it;
    else return Args[Args.size()-1];
}

OwningPtr<CompilerInstance> CreateCompilerInstance(SmallVector<const char*, 256>& Args, DiagnosticConsumer* DiagsBuffer)
{
    OwningPtr<CompilerInstance> Clang { CreateCompilerInvocation(Args, DiagsBuffer) };
    InstallFatalErrorHandler(Clang);
    CreateTarget(Clang);
    CreateFileManager(Clang);
    CreatePreprocessor(Clang);
    CreateAST(Clang);
    CreateMainFile(Clang, GetSourceFileName(Args).c_str());
    return Clang;
}

void CompileCpuSourceFile(SmallVector<const char*, 256>& Args, std::string SourceCode)
{
    std::string CpuFileName { GetSourceFileName(Args) + "_cpu.cpp" };
    std::ofstream cpu_file{ CpuFileName };
    cpu_file << SourceCode;
    cpu_file.close();

    SmallVector<const char*, 256> ArgsCpu {Args};
    SmallVector<const char*, 256>::iterator it =
            find_if(begin(ArgsCpu), end(ArgsCpu), [](const char* s) { return string(s) == string("-c"); } );
    if (it!=end(ArgsCpu)) *++it = CpuFileName.c_str();
    else ArgsCpu[ArgsCpu.size()-1] = CpuFileName.c_str();

    OwningPtr<CompilerInstance> Clang { CreateCompilerInstance(ArgsCpu, new TextDiagnosticBuffer) };

    ExecuteCompilerInvocation(Clang.get());

    // If any timers were active but haven't been destroyed yet, print their
    // results now.  This happens in -disable-free mode.
    llvm::TimerGroup::printAll(llvm::errs());
}

std::string CompileGpuSourceFile(SmallVector<const char*, 256>& Args, std::string SourceCode)
{
    std::string GpuFileName { GetSourceFileName(Args) + "_gpu.cpp" };
    std::ofstream gpu_file{ GpuFileName };
    gpu_file << SourceCode;
    gpu_file.close();

    SmallVector<const char*, 256> ArgsGpu {Args};
    SmallVector<const char*, 256>::iterator it2 =
            find_if(begin(ArgsGpu), end(ArgsGpu), [](const char* s) { return string(s) == string("-c"); } );
    if (it2!=end(ArgsGpu)) *++it2 = GpuFileName.c_str();
    else ArgsGpu[ArgsGpu.size()-1] = GpuFileName.c_str();

    OwningPtr<CompilerInstance> Clang { CreateCompilerInstance(ArgsGpu, new TextDiagnosticBuffer) };

    OwningPtr<clang::CodeGenAction> Act(new clang::EmitLLVMOnlyAction());
    if (!Clang->ExecuteAction(*Act)) {
        Act.reset();
        llvm::errs() << "Could not generate source\n";
        return "";
    }

    compiler::BitcodeDisassembler cm{Act->takeModule()};
    std::string OpenCLSource = cm.DisassembleModule();

    std::string OpenClFileName { GetSourceFileName(Args) + ".cl" };
    std::ofstream a_file{ OpenClFileName };
    a_file << OpenCLSource;
    a_file.close();

#ifdef Put
    llvm::errs() << "_________________ OpenCL _____________________________\n";
    llvm::errs() << OpenCLSource;
    llvm::errs() << "\n\n";
#endif
    return OpenCLSource;
}


}  // namespace

namespace compiler {


std::vector<std::string> RewriteSourceFile(SmallVector<const char*, 256>& Args)
{
    OwningPtr<CompilerInstance> Clang { CreateCompilerInstance(Args, nullptr) };

    compiler::RewriterASTConsumer* TheConsumer = new compiler::RewriterASTConsumer {Clang};
    Preprocessor &PP = Clang->getPreprocessor();
    PP.addPPCallbacks(TheConsumer); // Takes ownership of TheConsumer
    ParseAST(Clang, *TheConsumer);

    return {TheConsumer->GetRewritenCpuSource(), TheConsumer->GetRewritenGpuSource()};
}

std::vector<std::string> BuildClCode(SmallVector<const char*, 256>& Args)
{
    InitializeTargets();

    auto Sources = RewriteSourceFile(Args);
    assert(Sources.size() == 2);
    CompileCpuSourceFile(Args, Sources[0]);
    CompileGpuSourceFile(Args, Sources[1]);

    llvm::llvm_shutdown();

    return {};
}

} // namespace compiler

