#ifndef REWRITER_H
#define REWRITER_H

#include <memory>
#include <string>

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Lex/PPCallbacks.h>


namespace clang
{
    class CompilerInstance;
    class ASTContext;
    class AMPrestrictAttr;
}

namespace compiler {


/// Rewrite the source code: The result of rewritting the source code
/// should be two source code files (or strings holding the source);
/// one that will be compiled on the CPU and the other on the GPU
class RewriterASTConsumer :
        public clang::ASTConsumer,
        public clang::RecursiveASTVisitor<RewriterASTConsumer>,
        public clang::PPCallbacks
{
public:
    RewriterASTConsumer(const RewriterASTConsumer& that) = delete;
    RewriterASTConsumer& operator=(RewriterASTConsumer&) = delete;

    explicit RewriterASTConsumer(const llvm::OwningPtr<clang::CompilerInstance>& CI);

    ~RewriterASTConsumer();

    std::string GetRewritenCpuSource() const;
    std::string GetRewritenGpuSource() const;

    virtual void HandleTranslationUnit(clang::ASTContext& Context);
    virtual bool VisitFunctionDecl(clang::FunctionDecl const * const F);
    virtual bool VisitCallExpr(clang::CallExpr const * const Stmt);
    virtual void InclusionDirective(clang::SourceLocation HashLoc,
                                    const clang::Token &IncludeTok,
                                    clang::StringRef FileName,
                                    bool IsAngled,
                                    clang::CharSourceRange FilenameRange,
                                    const clang::FileEntry *File,
                                    clang::StringRef SearchPath,
                                    clang::StringRef RelativePath,
                                    const clang::Module *Imported);

protected:
    virtual void OnParallelForEachCall(clang::CallExpr const * const Stmt);
private:
    void WriteGpuDeclarators(clang::CallExpr const * const Statement);

private:
    std::string RewritenCpuSource;
    std::string RewritenGpuSource;

    clang::Rewriter TheCpuRewriter;
    clang::Rewriter TheGpuRewriter;

    int ParallelForEachCallCount;

    clang::FunctionDecl const* Func;
};

void ParseAST(llvm::OwningPtr<clang::CompilerInstance>& TheCompInst, RewriterASTConsumer& TheConsumer);

} // namespace compiler


namespace {

class HasRestrictAttribute {
public:
    HasRestrictAttribute(const HasRestrictAttribute& that) = delete;
    HasRestrictAttribute& operator=(HasRestrictAttribute&) = delete;

    explicit HasRestrictAttribute(clang::FunctionDecl const * const F);

    bool IsRestrict() const;
    bool IsValid() const;
    bool HasCPU() const;
    bool HasGPU() const;

    clang::SourceLocation getLocation() const;

private:
    bool IsRestrictKeywordBeforeFunctionName() const;
    bool IsDeclarationADefinitionRestrictKeywordDifferent();

private:
    bool Restrict;
    bool Valid;
    bool CPU;
    bool GPU;

    clang::AMPrestrictAttr* Attr;
    clang::FunctionDecl const* Func;
};


class ExpandSourceRange {
public:
    ExpandSourceRange(const ExpandSourceRange& that) = delete;
    ExpandSourceRange& operator=(ExpandSourceRange&) = delete;

    ExpandSourceRange(clang::Rewriter& Rewrite);

    clang::SourceRange operator() (clang::SourceRange loc);
private:
    clang::SourceLocation FindSemiColonAfterLocation(clang::SourceLocation loc);
private:
    clang::Rewriter& TheRewriter;
};


/// What arguments (names and types) are captured by the lambda ? Some are captured by reference
/// and others be value.
class LambdaRewiter :
        public clang::RecursiveASTVisitor<LambdaRewiter>
{
public:
    LambdaRewiter(clang::Rewriter& CpuRewriter, clang::Rewriter& GpuRewriter);

    void Rewrite(clang::CallExpr const * const Statement);

public:
    bool VisitLambdaExpr(clang::LambdaExpr *LE);
    bool VisitDeclStmt(clang::DeclStmt *S);
    bool VisitVarDecl(clang::VarDecl *VD);

private:
    void ExtractLambdaFunctionInfo(clang::CallExpr const * const Statement);
    void GenerateKernelNamePostfix();
    void RewriteCpuCode();
    void RewriteGpuCode();

private:
    struct DeclarationInfo {
        std::string ValueType;
        std::string Type;
        std::string VariableName;

        // e.g. std::vector<int>& A;
        // ValueType would be 'int'
        // Type would be 'std::vector<int>'
        // VariableName would be 'A'
    };

    using DeclarationInfoList = std::vector<DeclarationInfo>;

    clang::Rewriter& TheCpuRewriter;
    clang::Rewriter& TheGpuRewriter;

    DeclarationInfoList TheCapturesByRef;
    DeclarationInfoList TheCapturesByValue;
    DeclarationInfoList TheParams;
    clang::SourceRange CaptureListRange;
    clang::SourceRange BodyRange;
    clang::SourceRange ParamRange;

    std::string PostfixName;
};

}

#endif
