#ifndef REVNGC_FUNCTIONDECLCREATIONACTION_H
#define REVNGC_FUNCTIONDECLCREATIONACTION_H

#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/FrontendAction.h>

namespace llvm {
class Module;
class Function;
} // namespace llvm

namespace clang {

class CompilerInstance;

namespace tooling {

class FuncDeclCreationAction : public ASTFrontendAction {

public:
  using FunctionsMap = std::map<llvm::Function *, clang::FunctionDecl *>;

public:
  FuncDeclCreationAction(llvm::Module &M,
                         FunctionsMap &Decls,
                         FunctionsMap &Defs) :
    M(M),
    FunctionDecls(Decls),
    FunctionDefs(Defs) {}

public:
  std::unique_ptr<ASTConsumer> newASTConsumer();

  virtual std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &, llvm::StringRef) override {
    return newASTConsumer();
  }

private:
  llvm::Module &M;
  FunctionsMap &FunctionDecls;
  FunctionsMap &FunctionDefs;
};

} // end namespace tooling

inline std::unique_ptr<ASTConsumer>
CreateFuncDeclCreator(llvm::Module &M,
                      tooling::FuncDeclCreationAction::FunctionsMap &FunDecls,
                      tooling::FuncDeclCreationAction::FunctionsMap &FunDefs) {
  return tooling::FuncDeclCreationAction(M, FunDecls, FunDefs).newASTConsumer();
}

} // end namespace clang

#endif // REVNGC_FUNCTIONDECLCREATIONACTION_H