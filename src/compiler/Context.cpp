#include "Context.h"

#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO/GlobalDCE.h>
#include <llvm/Transforms/IPO/PartialInlining.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/DCE.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/MemCpyOptimizer.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SCCP.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/Utils/LoopSimplify.h>

#include <utility>

#include "ast/UnitNode.h"
#include "compare.h"


void LogError(const char *Str) { fprintf(stderr, "Error: %s\n", Str); }

llvm::Value *LogErrorV(const char *Str)
{
    LogError(Str);
    return nullptr;
}

llvm::Value *LogErrorV(const std::string &Str)
{
    LogError(Str.c_str());
    return nullptr;
}

struct ContextImpl
{
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<llvm::ConstantFolder, llvm::IRBuilderDefaultInserter>> Builder;
    std::unordered_map<std::string, llvm::AllocaInst *> NamedAllocations;
    std::unordered_map<std::string, llvm::Value *> NamedValues;
    llvm::Function *TopLevelFunction{};
    std::unordered_map<std::string, llvm::Function *> FunctionDefinitions;
    BreakBasicBlock BreakBlock;

    std::unique_ptr<llvm::FunctionPassManager> TheFPM;
    std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
    std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
    std::unique_ptr<llvm::ModulePassManager> TheMPM;
    std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
    std::unique_ptr<llvm::StandardInstrumentations> TheSI;
};


Context::Context(std::unique_ptr<UnitNode> &unit, CompilerOptions options, const std::string &TargetTriple) :
    compilerOptions(std::move(options))
{
    m_impl = std::make_shared<ContextImpl>();

    // Open a new context and module.
    m_impl->TheContext = std::make_unique<llvm::LLVMContext>();
    m_impl->TheModule = std::make_unique<llvm::Module>(unit->getUnitName(), *m_impl->TheContext);

    // Create a new builder for the module.
    m_impl->Builder = std::make_unique<llvm::IRBuilder<>>(*m_impl->TheContext);
    // Create new pass and analysis managers.
    m_impl->TheFPM = std::make_unique<llvm::FunctionPassManager>();
    m_impl->TheMPM = std::make_unique<llvm::ModulePassManager>();
    m_impl->TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
    m_impl->TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();

    m_impl->ThePIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
    m_impl->TheSI = std::make_unique<llvm::StandardInstrumentations>(*m_impl->TheContext,
                                                                     /*DebugLogging*/ true);

    m_impl->TheSI->registerCallbacks(*m_impl->ThePIC, m_impl->TheMAM.get());
    this->TargetTriple = std::make_unique<llvm::Triple>(TargetTriple);

    // Add transform passes.
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    if (compilerOptions.buildMode == BuildMode::Release)
    {
        m_impl->TheFPM->addPass(llvm::InstCombinePass());
        // Reassociate expressions.
        m_impl->TheFPM->addPass(llvm::ReassociatePass());
        // Eliminate Common SubExpressions.
        m_impl->TheFPM->addPass(llvm::GVNPass());
        // Simplify the control flow graph (deleting unreachable blocks, etc).
        m_impl->TheFPM->addPass(llvm::SimplifyCFGPass());

        m_impl->TheFPM->addPass(llvm::SCCPPass());

        m_impl->TheFPM->addPass(llvm::LoopSimplifyPass());

        m_impl->TheFPM->addPass(llvm::MemCpyOptPass());

        m_impl->TheFPM->addPass(llvm::DCEPass());
        m_impl->TheMPM->addPass(llvm::AlwaysInlinerPass());


        m_impl->TheMPM->addPass(llvm::PartialInlinerPass());
        m_impl->TheMPM->addPass(llvm::ModuleInlinerPass());
        m_impl->TheMPM->addPass(llvm::GlobalDCEPass());

        // how do i remove unused functions?


        m_impl->TheMPM->addPass(llvm::createModuleToFunctionPassAdaptor(
                llvm::DCEPass())); // Remove dead functions and global variables.
    }

    // Register analysis passes used in these transform passes.
    llvm::PassBuilder PB;
    const auto TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
    const auto TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();

    PB.registerModuleAnalyses(*m_impl->TheMAM);
    PB.registerFunctionAnalyses(*m_impl->TheFAM);
    PB.crossRegisterProxies(*TheLAM, *m_impl->TheFAM, *TheCGAM, *m_impl->TheMAM);


    ProgramUnit = std::move(unit);
}
std::unique_ptr<llvm::Module> &Context::module() const { return m_impl->TheModule; }
llvm::Function *Context::currentFunction() const { return m_impl->TopLevelFunction; }
void Context::setCurrentFunction(llvm::Function *function) const { m_impl->TopLevelFunction = function; }
llvm::AllocaInst *Context::namedAllocation(const std::string &name) const { return m_impl->NamedAllocations[name]; }
llvm::Value *Context::namedValue(const std::string &name) const { return m_impl->NamedValues[name]; }
BreakBasicBlock &Context::breakBlock() const { return m_impl->BreakBlock; }
std::unique_ptr<llvm::LLVMContext> &Context::context() const { return m_impl->TheContext; }
std::unique_ptr<UnitNode> &Context::programUnit() { return ProgramUnit; }
std::unique_ptr<llvm::IRBuilder<>> &Context::builder() const { return m_impl->Builder; }
void Context::verifyModule(llvm::Function *function) const
{
    llvm::verifyFunction(*function, &llvm::errs());
    if (!llvm::verifyModule(*m_impl->TheModule, &llvm::errs()))
    {
        if (compilerOptions.buildMode == BuildMode::Release)
        {
            m_impl->TheMPM->run(*m_impl->TheModule, *m_impl->TheMAM);
            m_impl->TheFPM->run(*function, *m_impl->TheFAM);
        }
    }
}
void Context::verifyFunction(llvm::Function *functionDefinition) const
{
    if (llvm::verifyFunction(*functionDefinition, &llvm::errs()))
    {
        if (compilerOptions.buildMode == BuildMode::Release)
        {
            m_impl->TheFPM->run(*functionDefinition, *m_impl->TheFAM);
        }
    }
}
void Context::setNamedAllocation(const std::string &name, llvm::AllocaInst *allocation) const
{
    m_impl->NamedAllocations[name] = allocation;
}
void Context::removeName(const std::string &name) const
{
    m_impl->NamedValues.erase(name);
    m_impl->NamedAllocations.erase(name);
}
void Context::setNamedValue(const std::string &name, llvm::Value *value) const { m_impl->NamedValues[name] = value; }
void Context::addFunctionDefinition(const std::string &function_signature, llvm::Function *function) const
{
    m_impl->FunctionDefinitions[function_signature] = function;
}
llvm::Function *Context::functionDefinition(const std::string &string) const
{
    return m_impl->FunctionDefinitions[string];
}
std::optional<llvm::Value *> Context::findValue(const std::string &name) const
{
    llvm::Value *V = namedAllocation(name);


    if (!V)
    {
        for (auto &arg: currentFunction()->args())
        {
            if (iequals(arg.getName(), name))
            {
                V = currentFunction()->getArg(arg.getArgNo());
                break;
            }
        }
    }
    return V ? std::make_optional(V) : std::nullopt;
}
