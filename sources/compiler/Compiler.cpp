
#include "Compiler.h"
#include "BitcodeDisassembler.h"

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

OwningPtr<CompilerInstance> CreateCompilerInvocation(const char **ArgBegin, const char **ArgEnd,
                                                     const char *Argv0, void *MainAddr)
{
    OwningPtr<CompilerInstance> Clang { new CompilerInstance() };

    IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
    IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
    TextDiagnosticBuffer *DiagsBuffer = new TextDiagnosticBuffer;
    DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);
    bool Success;
    Success = CompilerInvocation::CreateFromArgs(Clang->getInvocation(),
                                                 ArgBegin, ArgEnd, Diags);

    // Infer the builtin include path if unspecified.
    if (Clang->getHeaderSearchOpts().UseBuiltinIncludes &&
        Clang->getHeaderSearchOpts().ResourceDir.empty())
      Clang->getHeaderSearchOpts().ResourceDir =
        CompilerInvocation::GetResourcesPath(Argv0, MainAddr);

    // Create the actual diagnostics engine.
    Clang->createDiagnostics();
    if (!Clang->hasDiagnostics())
        return OwningPtr<CompilerInstance>();

    // Set an error handler, so that any LLVM backend diagnostics go through our
    // error handler.
    llvm::install_fatal_error_handler(LLVMErrorHandler,
                                    static_cast<void*>(&Clang->getDiagnostics()));

    DiagsBuffer->FlushDiagnostics(Clang->getDiagnostics());
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

std::string GetSourceFileName(SmallVector<const char*, 256>& Args)
{
    SmallVector<const char*, 256>::const_iterator it =
            find_if(begin(Args), end(Args), [](const char* s) { return string(s) == string("-c"); } );
    if (it!=end(Args)) return *++it;
    else return Args[Args.size()-1];
}

}  // namespace

namespace compiler {

std::string BuildClCode(
        const char **ArgBegin, const char **ArgEnd,
        const char *Argv0, void *MainAddr,
        SmallVector<const char*, 256>& Args)
{
    InitializeTargets();

    OwningPtr<CompilerInstance> Clang { CreateCompilerInvocation(ArgBegin, ArgEnd, Argv0, MainAddr) };
    CreateTarget(Clang);
    CreateFileManager(Clang);
    CreatePreprocessor(Clang);
    CreateAST(Clang);

    std::string SourceFileName = GetSourceFileName(Args);
    Clang->getSourceManager().createMainFileID(Clang->getFileManager().getFile(SourceFileName.c_str()));
    Clang->getDiagnosticClient().BeginSourceFile(Clang->getLangOpts(), &Clang->getPreprocessor());

    bool Success = ExecuteCompilerInvocation(Clang.get());

    // If any timers were active but haven't been destroyed yet, print their
    // results now.  This happens in -disable-free mode.
    llvm::TimerGroup::printAll(llvm::errs());

    // Our error handler depends on the Diagnostics object, which we're
    // potentially about to delete. Uninstall the handler now so that any
    // later errors use the default handling behavior instead.
    llvm::remove_fatal_error_handler();

    if (!Success) return "";

    OwningPtr<clang::CodeGenAction> Act(new clang::EmitLLVMOnlyAction());
    if (!Clang->ExecuteAction(*Act)) {
        Act.reset();
        llvm::errs() << "Could not generate source\n";
        return "";
    }

    BitcodeDisassembler cm{Act->takeModule()};
    std::string OpenCLSource = cm.DisassembleModule();
#ifdef WRITE_TO_FILE
    std::ofstream a_file{"/tmp/opencl_temp.cl"};
    a_file << OpenCLSource;
    a_file.close();
#endif
    llvm::llvm_shutdown();

    return OpenCLSource;
}

} // namespace compiler

