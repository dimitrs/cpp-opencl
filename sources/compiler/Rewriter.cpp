#include "Rewriter.h"

#include <clang/AST/ASTContext.h>
#include <clang/Sema/Sema.h>
#include <clang/Lex/Lexer.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Parse/ParseAST.h>

using namespace clang;

namespace compiler {


void ParseAST(OwningPtr<CompilerInstance>& TheCompInst, RewriterASTConsumer& TheConsumer)
{
    clang::ParseAST(TheCompInst->getPreprocessor(), &TheConsumer, TheCompInst->getASTContext());
}

RewriterASTConsumer::RewriterASTConsumer(const OwningPtr<CompilerInstance>& CI) :
    RewritenCpuSource{}, RewritenGpuSource{}, ParallelForEachCallCount{0}
{
    TheCpuRewriter.setSourceMgr(CI->getSourceManager(), CI->getLangOpts());
    TheGpuRewriter.setSourceMgr(CI->getSourceManager(), CI->getLangOpts());
}

RewriterASTConsumer::~RewriterASTConsumer()
{}

std::string RewriterASTConsumer::GetRewritenCpuSource() const
{
    return RewritenCpuSource;
}

std::string RewriterASTConsumer::GetRewritenGpuSource() const
{
    return RewritenGpuSource;
}

void RewriterASTConsumer::InclusionDirective(clang::SourceLocation HashLoc,
                                             const clang::Token& IncludeTok,
                                             clang::StringRef FileName,
                                             bool IsAngled,
                                             clang::CharSourceRange FilenameRange,
                                             const clang::FileEntry* File,
                                             clang::StringRef SearchPath,
                                             clang::StringRef RelativePath,
                                             const clang::Module* Imported)
{

    if ("ParallelForEach.h" == FileName) {
        SourceManager& SM = TheGpuRewriter.getSourceMgr();
        SourceRange Range;
        Range.setBegin(HashLoc);
        Range.setEnd(SM.getSpellingLoc(FilenameRange.getEnd()));
        TheGpuRewriter.RemoveText(Range);
    }
}

void RewriterASTConsumer::HandleTranslationUnit(ASTContext& Context)
{
    TranslationUnitDecl* D = Context.getTranslationUnitDecl();
    TraverseDecl(D);

    const RewriteBuffer& RewriteBufG =
            TheGpuRewriter.getEditBuffer(TheGpuRewriter.getSourceMgr().getMainFileID());
    RewritenGpuSource = std::string(RewriteBufG.begin(), RewriteBufG.end());

    const RewriteBuffer& RewriteBufC =
            TheCpuRewriter.getEditBuffer(TheCpuRewriter.getSourceMgr().getMainFileID());
    RewritenCpuSource = std::string(RewriteBufC.begin(), RewriteBufC.end());

#ifdef Out
    llvm::errs() << "______________________________ CPU _____________________ \n";
    llvm::errs() << std::string(RewriteBufC.begin(), RewriteBufC.end());
    llvm::errs() << "\n\n______________________________ GPU _____________________ \n";
    llvm::errs() << std::string(RewriteBufG.begin(), RewriteBufG.end());
#endif
}

void RemoveFunction(clang::Rewriter& TheRewriter, const FunctionDecl* F)
{
    ExpandSourceRange SourceRange{TheRewriter};
    TheRewriter.RemoveText(SourceRange(F->getSourceRange()));
    if (FunctionTemplateDecl* D = F->getDescribedFunctionTemplate()) {
        TheRewriter.RemoveText(SourceRange(D->getSourceRange()));
    }
}

bool RewriterASTConsumer::VisitFunctionDecl(clang::FunctionDecl const * const F)
{
    Func = F;

    if (F->isMain()) {
        TheGpuRewriter.RemoveText(F->getSourceRange());
        return true;
    }

    HasRestrictAttribute Result{F};
    if (!Result.IsRestrict()) return true;
    if (!Result.IsValid()) {
        SourceManager& TheSourceMgr = F->getASTContext().getSourceManager();
        llvm::errs() << "Not Valid amp attribute: " << Result.getLocation().printToString(TheSourceMgr) << "\n";
        return true;
    }

    if (Result.HasCPU() && !Result.HasGPU()) {
        RemoveFunction(TheGpuRewriter, F);
    }

    if (!Result.HasCPU() && Result.HasGPU()) {
        RemoveFunction(TheCpuRewriter, F);
    }

    return true;
}

void RemoveStatement(clang::Rewriter& TheRewriter, clang::CallExpr const * const Statement)
{
    ExpandSourceRange SourceRange{TheRewriter};
    TheRewriter.RemoveText(SourceRange(Statement->getSourceRange()));
}

bool RewriterASTConsumer::VisitCallExpr(clang::CallExpr const * const Statement)
{
    if (clang::FunctionDecl const * const F = Statement->getDirectCallee()) {
        if ("compute::parallel_for_each" != F->getQualifiedNameAsString())
            return true;
        //RemoveStatement(TheGpuRewriter, Statement);
        RemoveFunction(TheGpuRewriter, Func);
        OnParallelForEachCall(Statement);
    }
    return true;
}

void RewriterASTConsumer::OnParallelForEachCall(clang::CallExpr const * const Statement)
{
    WriteGpuDeclarators(Statement);

    LambdaRewiter Lambda(TheCpuRewriter, TheGpuRewriter);
    Lambda.Rewrite(Statement);
}

void RewriterASTConsumer::WriteGpuDeclarators(clang::CallExpr const * const Statement)
{
    if (++ParallelForEachCallCount == 1) {
        SourceManager& SM = TheGpuRewriter.getSourceMgr();
        std::pair<FileID, unsigned> locInfo = SM.getDecomposedLoc(Statement->getLocStart());
        SourceLocation Eof = SM.getLocForEndOfFile(locInfo.first);
        std::string Decls1 { "extern \"C\" long long get_global_id(int);" };
        std::string Decls2 { "extern \"C\" int get_global_size(int);" };
        TheGpuRewriter.InsertTextAfter(Eof, Decls1 + "\n" + Decls2 + "\n\n\n");
    }
}


} // namespace compiler


namespace {

LambdaRewiter::LambdaRewiter(Rewriter& CpuRewriter, Rewriter& GpuRewriter)
    : RecursiveASTVisitor<LambdaRewiter>(),
      TheCpuRewriter(CpuRewriter), TheGpuRewriter(GpuRewriter)
{
}

void LambdaRewiter::Rewrite(CallExpr const * const Statement)
{
    ExtractLambdaFunctionInfo(Statement);
    GenerateKernelNamePostfix();
    RewriteGpuCode();
    RewriteCpuCode();
}

void LambdaRewiter::ExtractLambdaFunctionInfo(CallExpr const * const Statement)
{
    FunctionDecl const * const F = Statement->getDirectCallee();
    static const unsigned int NR_ARGUMENTS = 4;
    assert(NR_ARGUMENTS == std::min(Statement->getNumArgs(), F->getNumParams()));
    Stmt const * const S = Statement->getArg(NR_ARGUMENTS-1);
    this->TraverseStmt(const_cast<Stmt*>(S));
}

bool LambdaRewiter::VisitLambdaExpr(LambdaExpr *LE)
{
    LambdaExpr::capture_iterator I = LE->capture_begin();
    LambdaExpr::capture_iterator E = LE->capture_end();
    for (; I != E; ++I) {
        if (VarDecl* D = I->getCapturedVar()) {
            std::string Type {QualType::getAsString(D->getType().split())};
            std::string Variable {D->getName().str()};

            if (I->getCaptureKind() == LCK_ByRef) {
                TheCapturesByRef.push_back({"",Type,Variable});
            }
            else if (I->getCaptureKind() == LCK_ByCopy) {
                TheCapturesByValue.push_back({"",Type,Variable});
            }
            else {
                return false;
            }
        }
    }

    CaptureListRange.setBegin(LE->getIntroducerRange().getBegin());
    CaptureListRange.setEnd(LE->getIntroducerRange().getEnd());
    BodyRange.setBegin(LE->getBody()->getLocStart());
    BodyRange.setEnd(LE->getBody()->getLocEnd());

    TraverseLambdaBody(LE);
    return true;
}

bool LambdaRewiter::VisitDeclStmt(DeclStmt *S)
{
    for (DeclStmt::decl_iterator I = S->decl_begin(),
         E = S->decl_end(); I!=E; ++I) {
        if (VarDecl *VD = dyn_cast<VarDecl>(*I)) {
            VisitVarDecl(VD);
        }
    }
    ParamRange.setBegin(S->getLocStart());
    ParamRange.setEnd(S->getLocEnd());

    return true;
}

bool LambdaRewiter::VisitVarDecl(VarDecl *VD)
{
    if (! VD->isLocalVarDecl()) {
        std::string VarTypeName {QualType::getAsString(VD->getType().split())};
        std::string VarName {VD->getName().str()};
        TheParams.push_back({"",VarTypeName,VarName});
    }
    return true;
}

void LambdaRewiter::GenerateKernelNamePostfix()
{
    PostfixName = std::string("_") + std::to_string(std::rand());
}

void LambdaRewiter::RewriteCpuCode()
{
    SourceManager& SM = TheCpuRewriter.getSourceMgr();
    std::string FileName { " \"" + std::string {SM.getFilename(BodyRange.getBegin())} +  ".cl\" " };
    std::string KernelName {" \"_Kernel" + PostfixName + "\" "};
    std::string NewLambdaBody { " { return std::pair<std::string,std::string> ( " + FileName + "," + KernelName + "); }" };
    ExpandSourceRange Range{TheCpuRewriter};
    TheCpuRewriter.ReplaceText(Range(BodyRange), NewLambdaBody.c_str());
    //TheCpuRewriter.ReplaceText(ParamRange, "");
}

void LambdaRewiter::RewriteGpuCode()
{
    assert(1 == TheParams.size());

    std::string SignatureLambda { TheParams[0].Type + " _Lambda" + PostfixName +
                "(" + TheParams[0].Type + " " + TheParams[0].VariableName + ") " };
    std::string BodyLambda { TheCpuRewriter.getRewrittenText(BodyRange) };

    std::string SignatureKernel { std::string {"extern \"C\" void _Kernel"} + PostfixName +
                "(" + TheParams[0].Type + "* in, " + TheParams[0].Type + "* out) " } ;
    std::string BodyKernel { "{ unsigned idx = get_global_id(0); out[idx] = _Lambda" + PostfixName + "(in[idx]); }" };

    SourceManager& SM = TheGpuRewriter.getSourceMgr();
    std::pair<FileID, unsigned> locInfo = SM.getDecomposedLoc(BodyRange.getEnd());
    SourceLocation Eof = SM.getLocForEndOfFile(locInfo.first);
    TheGpuRewriter.InsertTextAfter(Eof, SignatureLambda + BodyLambda + "\n\n" + SignatureKernel + BodyKernel);
}

HasRestrictAttribute::HasRestrictAttribute(FunctionDecl const * const F) :
    Restrict{false}, Valid{false}, CPU{false}, GPU{false},
    Attr{nullptr}, Func{F}
{
    Attr = F->getAttr<AMPrestrictAttr>();
    if (Attr) {
        Restrict = true;
        Valid = true;
        for (AMPrestrictAttr::args_iterator it = Attr->args_begin();
             it != Attr->args_end();
             ++it) {
            StringLiteral* string = cast<StringLiteral>(*it);
            if (string->getString() == "cpu") CPU = true;
            else if (string->getString() == "gpu") GPU = true;
            else Valid = false;
        }
        if (!CPU && !GPU) Valid = false;
        if (Valid) {
            if (IsRestrictKeywordBeforeFunctionName() /*||
                    IsDeclarationADefinitionRestrictKeywordDifferent()*/) {
                Valid = false;
            }
        }
    }
}

bool HasRestrictAttribute::IsRestrict() const { return Restrict; }
bool HasRestrictAttribute::IsValid() const { return Valid; }
bool HasRestrictAttribute::HasCPU() const { return CPU; }
bool HasRestrictAttribute::HasGPU() const { return GPU; }

SourceLocation HasRestrictAttribute::getLocation() const
{
    SourceManager& TheSourceMgr = Func->getASTContext().getSourceManager();
    return TheSourceMgr.getExpansionLoc(Attr->getLocation());
}

bool HasRestrictAttribute::IsRestrictKeywordBeforeFunctionName() const {
    if (getLocation() < Func->getNameInfo().getBeginLoc()) {
        return true;
    }
    return false;
}

bool HasRestrictAttribute::IsDeclarationADefinitionRestrictKeywordDifferent()
{
    if (Func->isThisDeclarationADefinition()) {
        if (const FunctionDecl* FD = Func->getCanonicalDecl()) {
            if (FD == Func) return false;

            HasRestrictAttribute Result{FD};
            if (!Result.IsRestrict()) return true;
            if (!Result.IsValid()) return true;
            if (CPU != Result.HasCPU()) return true;
            if (GPU != Result.HasGPU()) return true;
        }
    }
    return false;
}

ExpandSourceRange::ExpandSourceRange(Rewriter& Rewrite) :
    TheRewriter(Rewrite)
{}

SourceRange ExpandSourceRange::operator() (SourceRange loc)
{
    // If the range is a full statement, and is followed by a
    // semi-colon then expand the range to include the semicolon.
    //return loc;

    SourceLocation b = loc.getBegin();
    SourceLocation e = FindSemiColonAfterLocation(loc.getEnd());
    if (e.isInvalid()) e = loc.getEnd();
    return SourceRange(b,e);
}

SourceLocation ExpandSourceRange::FindSemiColonAfterLocation(SourceLocation loc)
{
    SourceManager &SM = TheRewriter.getSourceMgr();
    if (loc.isMacroID()) {
        if (!Lexer::isAtEndOfMacroExpansion(loc, SM,
                                            TheRewriter.getLangOpts(), &loc))
            return SourceLocation();
    }
    loc = Lexer::getLocForEndOfToken(loc, /*Offset=*/0, SM,
                                     TheRewriter.getLangOpts());

    // Break down the source location.
    std::pair<FileID, unsigned> locInfo = SM.getDecomposedLoc(loc);

    // Try to load the file buffer.
    bool invalidTemp = false;
    StringRef file = SM.getBufferData(locInfo.first, &invalidTemp);
    if (invalidTemp)
        return SourceLocation();

    const char *tokenBegin = file.data() + locInfo.second;

    // Lex from the start of the given location.
    Lexer lexer(SM.getLocForStartOfFile(locInfo.first),
                TheRewriter.getLangOpts(),
                file.begin(), tokenBegin, file.end());
    Token tok;
    lexer.LexFromRawLexer(tok);
    if (tok.isNot(tok::semi))
        return SourceLocation();

    return tok.getLocation();
}


} // namespace


