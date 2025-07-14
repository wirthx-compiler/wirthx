#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "CompilerOptions.h"

#include <unordered_map>

namespace llvm
{
    class GlobalValue;
    class GlobalVariable;
    class LLVMContext;
    class Module;


    class AllocaInst;
    class Value;
    class Function;

    class BasicBlock;
    class ConstantFolder;
    class IRBuilderDefaultInserter;
    class Triple;
    // template<typename FolderTy = ConstantFolder, typename InserterTy = IRBuilderDefaultInserter>
    template<class FolderTy, class InserterTy>
    class IRBuilder;


} // namespace llvm


// #include "llvm/IR/PassManager.h"

class UnitNode;

struct BreakBasicBlock
{
    llvm::BasicBlock *Block = nullptr;
    bool BlockUsed = false;
};

struct ContextImpl;

class Context
{
    CompilerOptions compilerOptions;
    std::shared_ptr<ContextImpl> m_impl;
    std::unique_ptr<UnitNode> ProgramUnit;

public:
    bool loadValue = true;
    bool explicitReturn = false;
    std::unique_ptr<llvm::Triple> TargetTriple;


    explicit Context(std::unique_ptr<UnitNode> &unit, CompilerOptions options, const std::string &TargetTriple);

    std::unique_ptr<llvm::Module> &module() const;
    llvm::Function *currentFunction() const;
    void setCurrentFunction(llvm::Function *function) const;
    llvm::AllocaInst *namedAllocation(const std::string &name) const;
    llvm::Value *namedValue(const std::string &name) const;
    BreakBasicBlock &breakBlock() const;
    std::unique_ptr<llvm::LLVMContext> &context() const;
    std::unique_ptr<UnitNode> &programUnit();
    const CompilerOptions &options() const { return compilerOptions; }

    std::unique_ptr<llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>> &builder() const;
    void verifyModule(llvm::Function *function) const;
    void verifyFunction(llvm::Function *function) const;
    void setNamedAllocation(const std::string &name, llvm::AllocaInst *allocation) const;
    void removeName(const std::string &name) const;
    void setNamedValue(const std::string &name, llvm::Value *value) const;
    void addFunctionDefinition(const std::string &function_signature, llvm::Function *function) const;
    llvm::Function *functionDefinition(const std::string &string) const;

    std::optional<llvm::Value *> findValue(const std::string &name) const;
    llvm::GlobalVariable *getOrCreateGlobalString(const std::string &value, const std::string &name = "") const;
};

void LogError(const char *Str);
llvm::Value *LogErrorV(const char *Str);
llvm::Value *LogErrorV(const std::string &Str);
