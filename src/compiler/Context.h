#pragma once

#include <map>
#include <memory>
#include <string>

#include "CompilerOptions.h"

#include <unordered_map>

namespace llvm
{
    class LLVMContext;
    class Module;


    class AllocaInst;
    class Value;
    class Function;

    class PassInstrumentationCallbacks;
    class StandardInstrumentations;
    class BasicBlock;
    class ConstantFolder;
    class IRBuilderDefaultInserter;
    class Triple;
    // template<typename FolderTy = ConstantFolder, typename InserterTy = IRBuilderDefaultInserter>
    template<class FolderTy, class InserterTy>
    class IRBuilder;

    template<typename IRUnitT, typename... ExtraArgTs>
    class AnalysisManager;

    template<typename IRUnitT, typename AnalysisManagerT, typename... ExtraArgTs>
    class PassManager;

    using FunctionPassManager = PassManager<Function, AnalysisManager<Function>>;
    using FunctionAnalysisManager = AnalysisManager<Function>;
    using ModuleAnalysisManager = AnalysisManager<Module>;

} // namespace llvm


// #include "llvm/IR/PassManager.h"

class UnitNode;

struct BreakBasicBlock
{
    llvm::BasicBlock *Block = nullptr;
    bool BlockUsed = false;
};

struct Context
{
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>> Builder;
    std::unordered_map<std::string, llvm::AllocaInst *> NamedAllocations;
    std::unordered_map<std::string, llvm::Value *> NamedValues;
    llvm::Function *TopLevelFunction;
    std::unordered_map<std::string, llvm::Function *> FunctionDefinitions;
    BreakBasicBlock BreakBlock;

    std::unique_ptr<llvm::FunctionPassManager> TheFPM;
    std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
    std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
    std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
    std::unique_ptr<llvm::StandardInstrumentations> TheSI;
    std::unique_ptr<llvm::Triple> TargetTriple;

    std::unique_ptr<UnitNode> ProgramUnit;
    CompilerOptions compilerOptions;
    bool loadValue = true;
    bool expliciteReturn = false;
};

void LogError(const char *Str);
llvm::Value *LogErrorV(const char *Str);
llvm::Value *LogErrorV(const std::string &Str);
