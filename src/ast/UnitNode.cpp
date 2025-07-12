#include "ast/UnitNode.h"
#include <iostream>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/TargetParser/Triple.h>

#include "compare.h"
#include "compiler/Context.h"
#include "compiler/intrinsics.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "types/FileType.h"


UnitNode::UnitNode(const Token &token, const UnitType unitType, const std::string &unitName,
                   const std::vector<std::shared_ptr<FunctionDefinitionNode>> &functionDefinitions,
                   const TypeRegistry &typeDefinitions, const std::shared_ptr<BlockNode> &blockNode) :
    ASTNode(token), m_unitType(unitType), m_unitName(unitName), m_functionDefinitions(functionDefinitions),
    m_typeDefinitions(std::move(typeDefinitions)), m_blockNode(blockNode)
{
}
UnitNode::UnitNode(const Token &token, const UnitType unitType, const std::string &unitName,
                   const std::vector<std::string> &argumentNames,
                   const std::vector<std::shared_ptr<FunctionDefinitionNode>> &functionDefinitions,
                   const TypeRegistry &typeDefinitions, const std::shared_ptr<BlockNode> &blockNode) :
    ASTNode(token), m_unitType(unitType), m_unitName(unitName), m_functionDefinitions(functionDefinitions),
    m_typeDefinitions(std::move(typeDefinitions)), m_blockNode(blockNode), m_argumentNames(argumentNames)
{
}

void UnitNode::print()
{
    if (m_unitType == UnitType::PROGRAM)
    {
        std::cout << "program ";
    }
    else
    {
        std::cout << "unit ";
    }
    std::cout << m_unitName << "\n";
    for (const auto &def: m_functionDefinitions)
    {
        def->print();
    }

    m_blockNode->print();
}

std::vector<std::shared_ptr<FunctionDefinitionNode>> UnitNode::getFunctionDefinitions()
{
    return m_functionDefinitions;
}

std::optional<std::shared_ptr<FunctionDefinitionNode>> UnitNode::getFunctionDefinition(const std::string &functionName)
{
    for (auto &def: m_functionDefinitions)
    {
        if (iequals(def->functionSignature(), functionName) ||
            (iequals(def->name(), functionName) && def->externalName() != def->name()))
        {
            return def;
        }
    }
    return std::nullopt;
}
std::optional<std::shared_ptr<FunctionDefinitionNode>>
UnitNode::getFunctionDefinitionByName(const std::string &functionName)
{
    for (auto &def: m_functionDefinitions)
    {
        if (iequals(def->name(), functionName))
        {
            return def;
        }
    }
    return std::nullopt;
}

void UnitNode::addFunctionDefinition(const std::shared_ptr<FunctionDefinitionNode> &functionDefinition)
{
    m_functionDefinitions.push_back(functionDefinition);
}

std::string UnitNode::getUnitName() { return m_unitName; }

llvm::Value *UnitNode::codegen(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;

    m_blockNode->codegenConstantDefinitions(context);
    {
        // #define stdin  (__acrt_iob_func(0))
        // #define stdout (__acrt_iob_func(1))
        // #define stderr (__acrt_iob_func(2))
        if (context->TargetTriple->getOS() == llvm::Triple::Win32)
        {
            auto cFile = llvm::PointerType::getUnqual(*context->context());
            auto ext_stdout =
                    new llvm::GlobalVariable(*context->module(), cFile, false, llvm::GlobalValue::InternalLinkage,
                                             llvm::ConstantPointerNull::get(cFile), "stdout");

            // ext_stdout->setExternallyInitialized(true);
            context->setNamedValue("stdout", ext_stdout);
            // ext_stdout->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);

            auto ext_stderr =
                    new llvm::GlobalVariable(*context->module(), cFile, false, llvm::GlobalValue::InternalLinkage,
                                             llvm::ConstantPointerNull::get(cFile), "stderr");
            context->setNamedValue("stderr", ext_stderr);

            auto ext_stdin =
                    new llvm::GlobalVariable(*context->module(), cFile, false, llvm::GlobalValue::InternalLinkage,
                                             llvm::ConstantPointerNull::get(cFile), "stdin");
            // ext_stdin->setExternallyInitialized(true);
            context->setNamedValue("stdin", ext_stdin);
        }
        else
        {
            auto cFile = llvm::PointerType::getUnqual(*context->context());
            auto fileType = context->programUnit()->getTypeDefinitions().getType("file");
            auto ext_stderr = new llvm::GlobalVariable(*context->module(), cFile, false,
                                                       llvm::GlobalValue::ExternalLinkage, nullptr, "stderr");
            // ext_stderr->setExternallyInitialized(true);
            ext_stderr->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);
            if (m_argumentNames.size() >= 3)
            {
                m_blockNode->addVariableDefinition(VariableDefinition{.variableType = fileType,
                                                                      .variableName = m_argumentNames[2],
                                                                      .scopeId = 0,
                                                                      .llvmValue = ext_stderr,
                                                                      .constant = false});
            }

            context->setNamedValue("stderr", ext_stderr);
            auto ext_stdout = new llvm::GlobalVariable(*context->module(), cFile, false,
                                                       llvm::GlobalValue::ExternalLinkage, nullptr, "stdout");
            // ext_stdout->setExternallyInitialized(true);
            context->setNamedValue("stdout", ext_stdout);
            if (m_argumentNames.size() >= 2)
            {
                m_blockNode->addVariableDefinition(VariableDefinition{.variableType = fileType,
                                                                      .variableName = m_argumentNames[1],
                                                                      .scopeId = 0,
                                                                      .llvmValue = ext_stdout,
                                                                      .constant = false});
            }
            auto ext_stdin = new llvm::GlobalVariable(*context->module(), cFile, false,
                                                      llvm::GlobalValue::ExternalLinkage, nullptr, "stdin");
            // ext_stdin->setExternallyInitialized(true);
            context->setNamedValue("stdin", ext_stdin);
            if (!m_argumentNames.empty())
            {
                m_blockNode->addVariableDefinition(VariableDefinition{.variableType = fileType,
                                                                      .variableName = m_argumentNames[1],
                                                                      .scopeId = 0,
                                                                      .llvmValue = ext_stdin,
                                                                      .constant = false});
            }
        }

        createReadLnStdinCall(context);
    }


    for (auto &fdef: m_functionDefinitions)
    {
        fdef->codegen(context);
    }
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context->context()), params, false);

    std::string functionName = m_unitName;
    if (m_unitType == UnitType::PROGRAM)
    {
        functionName = "main";
    }

    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, functionName, context->module().get());
    context->setCurrentFunction(F);
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->context(), "entry", context->currentFunction());
    context->builder()->SetInsertPoint(BB);
    if (context->TargetTriple->getOS() == llvm::Triple::Win32)
    {
        llvm::Function *CalleeF = context->module()->getFunction("__acrt_iob_func");
        auto stdOutArgs = llvm::ArrayRef<llvm::Value *>{context->builder()->getInt32(1)};
        auto stdOutPtr = context->builder()->CreateCall(CalleeF, stdOutArgs);
        context->builder()->CreateStore(stdOutPtr, context->namedValue("stdout"));

        auto stdInPtr =
                context->builder()->CreateCall(CalleeF, llvm::ArrayRef<llvm::Value *>{context->builder()->getInt32(0)});
        context->builder()->CreateStore(stdInPtr, context->namedValue("stdin"));
        auto stdErrPtr =
                context->builder()->CreateCall(CalleeF, llvm::ArrayRef<llvm::Value *>{context->builder()->getInt32(2)});

        context->builder()->CreateStore(stdErrPtr, context->namedValue("stderr"));

        if (m_argumentNames.size() >= 2)
        {
            auto fileType = context->programUnit()->getTypeDefinitions().getType("file");


            m_blockNode->addVariableDefinition(VariableDefinition{.variableType = fileType,
                                                                  .variableName = m_argumentNames[1],
                                                                  .scopeId = 0,
                                                                  .llvmValue = context->namedValue("stdout"),
                                                                  .constant = false});
        }
        auto fileType = context->programUnit()->getTypeDefinitions().getType("file");

        if (m_argumentNames.size() >= 1)
        {
            m_blockNode->addVariableDefinition(VariableDefinition{.variableType = fileType,
                                                                  .variableName = m_argumentNames[0],
                                                                  .scopeId = 0,
                                                                  .llvmValue = context->namedValue("stdin"),
                                                                  .constant = false});
        }


        if (m_argumentNames.size() >= 2)
        {
            m_blockNode->addVariableDefinition(VariableDefinition{.variableType = fileType,
                                                                  .variableName = m_argumentNames[2],
                                                                  .scopeId = 0,
                                                                  .llvmValue = context->namedValue("stderr"),
                                                                  .constant = false});
        }
    }

    // m_blockNode->setBlockName("entry");
    //  Create a new basic block to start insertion into.
    m_blockNode->codegen(context);

    llvm::Function *exitCall = context->module()->getFunction("exit");
    std::vector<llvm::Value *> exitArgs;
    exitArgs.push_back(llvm::ConstantInt::get(*context->context(), llvm::APInt(32, 0)));

    context->builder()->CreateCall(exitCall, exitArgs);

    context->builder()->CreateRet(llvm::ConstantInt::get(*context->context(), llvm::APInt(32, 0)));


    context->verifyModule(F);
    return nullptr;
}

std::optional<VariableDefinition> UnitNode::getVariableDefinition(const std::string &name) const
{
    if (m_blockNode)
        return m_blockNode->getVariableDefinition(name);
    return std::nullopt;
}

std::set<std::string> UnitNode::collectLibsToLink()
{
    std::set<std::string> result;
    result.insert("c");
    for (auto &function: m_functionDefinitions)
    {
        auto libName = function->libName();
        if (!libName.empty())
            result.insert(libName);
    }
    return result;
}
TypeRegistry UnitNode::getTypeDefinitions() { return m_typeDefinitions; }
void UnitNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    for (const auto &def: m_functionDefinitions)
    {
        def->typeCheck(unit, parentNode);
    }

    m_blockNode->typeCheck(unit, parentNode);
}
std::optional<std::pair<const ASTNode *, std::shared_ptr<ASTNode>>> UnitNode::getNodeByToken(const Token &token) const
{
    if (m_blockNode)
    {
        if (auto result = m_blockNode->getNodeByToken(token))
            return std::pair{dynamic_cast<const ASTNode *>(this), result.value()};
    }
    for (const auto &function: m_functionDefinitions)
    {
        if (function->body())
        {
            if (auto result = function->body()->getNodeByToken(token))
                return std::pair{dynamic_cast<const ASTNode *>(function.get()), result.value()};
        }
    }

    return std::nullopt;
}
