#include "FunctionCallNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <utility>
#include "FunctionDefinitionNode.h"
#include "UnitNode.h"
#include "compare.h"
#include "compiler/Context.h"
#include "stdlib.h"


FunctionCallNode::FunctionCallNode(const Token &token, std::string name,
                                   const std::vector<std::shared_ptr<ASTNode>> &args) :
    ASTNode(token), m_name(std::move(name)), m_args(args)
{
}

void FunctionCallNode::print()
{
    std::cout << m_name << "(";
    for (auto &arg: m_args)
    {
        arg->print();
        std::cout << ",";
    }
    std::cout << ");\n";
}

std::string FunctionCallNode::callSignature(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) const
{
    ASTNode *parent = unit.get();
    if (parentNode != nullptr)
    {
        parent = parentNode;
    }
    std::string result = to_lower(m_name) + "(";
    for (size_t i = 0; i < m_args.size(); ++i)
    {
        const auto arg = m_args.at(i)->resolveType(unit, parent);

        result += arg->typeName + ((i < m_args.size() - 1) ? "," : "");
    }
    result += ")";
    return result;
}

llvm::Value *FunctionCallNode::codegen(std::unique_ptr<Context> &context)
{
    // Look up the name in the global module table.
    ASTNode *parent = context->programUnit().get();
    if (context->currentFunction())
    {
        if (auto def = context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str()))
        {
            parent = def.value().get();
        }
    }

    std::string functionName = callSignature(context->programUnit(), parent);

    llvm::Function *CalleeF = context->module()->getFunction(functionName);
    auto functionDefinition = context->programUnit()->getFunctionDefinition(functionName);
    if (!CalleeF)
    {
        functionDefinition = context->programUnit()->getFunctionDefinition(m_name);
        if (functionDefinition)
            CalleeF = context->module()->getFunction(functionDefinition.value()->functionSignature());
        if (CalleeF)
        {
            functionName = m_name;
        }
    }


    if (!CalleeF)
        return LogErrorV("Unknown function referenced: " + functionName);

    // If argument mismatch error.
    if (CalleeF->arg_size() != m_args.size() && !CalleeF->isVarArg())
    {
        std::cerr << "incorrect argument size for call " << functionName << " != " << CalleeF->arg_size() << "\n";
        return LogErrorV("Incorrect # arguments passed");
    }

    std::vector<llvm::Value *> ArgsV;
    for (unsigned argumentIndex = 0; argumentIndex < m_args.size(); ++argumentIndex)
    {

        std::optional<FunctionArgument> argType = std::nullopt;
        if (functionDefinition.has_value())
        {
            argType = functionDefinition.value()->getParam(argumentIndex);
        }
        if (argType.has_value())
            context->loadValue = !argType.value().isReference;

        auto argValue = m_args[argumentIndex]->codegen(context);
        context->loadValue = true;

        if (argType.has_value() && argType.value().isReference)
        {
            ArgsV.push_back(argValue);
        }
        else if (argType.has_value() && !argType.value().type->isSimpleType())
        {
            auto fieldName = functionDefinition.value()->name() + "_" + argType->argumentName;
            const auto llvmArgType = argType->type->generateLlvmType(context);

            auto memcpyCall = llvm::Intrinsic::getDeclaration(
                    context->module().get(), llvm::Intrinsic::memcpy,
                    {context->builder()->getPtrTy(), context->builder()->getPtrTy(), context->builder()->getInt64Ty()});
            std::vector<llvm::Value *> memcpyArgs;
            llvm::AllocaInst *alloca = context->builder()->CreateAlloca(llvmArgType, nullptr, fieldName + "_ptr");

            const llvm::DataLayout &DL = context->module()->getDataLayout();
            uint64_t structSize = DL.getTypeAllocSize(argType->type->generateLlvmType(context));


            memcpyArgs.push_back(context->builder()->CreateBitCast(alloca, context->builder()->getPtrTy()));
            memcpyArgs.push_back(context->builder()->CreateBitCast(argValue, context->builder()->getPtrTy()));
            memcpyArgs.push_back(context->builder()->getInt64(structSize));
            memcpyArgs.push_back(context->builder()->getFalse());

            context->builder()->CreateCall(memcpyCall, memcpyArgs);

            ArgsV.push_back(alloca);
        }
        else
        {
            ArgsV.push_back(argValue);
        }


        if (!ArgsV.back())
            return nullptr;
    }

    llvm::AllocaInst *allocInst = nullptr;
    auto returnType = functionDefinition.value()->returnType();
    if (returnType && returnType->baseType == VariableBaseType::String)
    {
        allocInst = context->builder()->CreateAlloca(returnType->generateLlvmType(context));
    }

    auto callInst = context->builder()->CreateCall(CalleeF, ArgsV);
    for (size_t i = 0, e = m_args.size(); i != e; ++i)
    {
        std::optional<FunctionArgument> argType = std::nullopt;
        if (functionDefinition.has_value())
        {
            argType = functionDefinition.value()->getParam(static_cast<unsigned>(i));
        }
        if (argType.has_value() && argType.value().type->baseType == VariableBaseType::Struct &&
            !argType.value().isReference)
        {
            auto llvmArgType = argType->type->generateLlvmType(context);

            callInst->addParamAttr(static_cast<unsigned>(i), llvm::Attribute::NoUndef);
            callInst->addParamAttr(static_cast<unsigned>(i),
                                   llvm::Attribute::getWithByValType(*context->context(), llvmArgType));
        }
    };

    if (allocInst)
    {
        context->builder()->CreateStore(callInst, allocInst);
        return allocInst;
    }
    return callInst;
}

std::shared_ptr<VariableType> FunctionCallNode::resolveType(const std::unique_ptr<UnitNode> &unitNode,
                                                            ASTNode *parentNode)
{
    auto functionDefinition = unitNode->getFunctionDefinition(callSignature(unitNode, parentNode));
    if (!functionDefinition)
    {
        functionDefinition = unitNode->getFunctionDefinition(m_name);
    }

    if (!functionDefinition)
    {
        return std::make_shared<VariableType>();
    }

    return functionDefinition.value()->returnType();
}


std::string FunctionCallNode::name() { return m_name; }
void FunctionCallNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) {}
bool FunctionCallNode::tokenIsPartOfNode(const Token &token) const
{
    if (ASTNode::tokenIsPartOfNode(token))
        return true;

    for (auto &arg: m_args)
    {
        if (arg->tokenIsPartOfNode(token))
        {
            return true;
        }
    }
    return false;
}
