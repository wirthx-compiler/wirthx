#include "VariableAccessNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include "FunctionCallNode.h"
#include "UnitNode.h"
#include "compare.h"
#include "compiler/Context.h"


VariableAccessNode::VariableAccessNode(const Token &token, bool dereference) :
    ASTNode(token), m_variableName(token.lexical()), m_dereference(dereference)
{
}

void VariableAccessNode::print() { std::cout << m_variableName; }

llvm::Value *VariableAccessNode::codegen(std::unique_ptr<Context> &context)
{
    const auto variableName = to_lower(m_variableName);
    llvm::Value *V = context->NamedValues[m_variableName];
    if (V)
    {
        return V;
    }

    llvm::AllocaInst *A = context->NamedAllocations[m_variableName];

    if (!A)
    {
        for (auto &arg: context->TopLevelFunction->args())
        {
            if (iequals(arg.getName(), variableName))
            {
                auto functionDefinition =
                        context->ProgramUnit->getFunctionDefinition(context->TopLevelFunction->getName().str());

                const auto argType = functionDefinition.value()->getParam(arg.getArgNo());
                const auto llvmArgType = argType->type->generateLlvmType(context);
                auto argValue = context->TopLevelFunction->getArg(arg.getArgNo());
                if (argType->type->baseType == VariableBaseType::Struct)
                {
                    llvm::AllocaInst *alloca =
                            context->Builder->CreateAlloca(llvmArgType, nullptr, argType->argumentName + "_struct");
                    return context->Builder->CreateLoad(A->getAllocatedType(), alloca, m_variableName.c_str());
                }
                if (argType->isReference && (argType->type->isSimpleType()))
                {

                    return context->Builder->CreateLoad(llvmArgType, argValue);
                }

                return argValue;
            }
        }


        return LogErrorV("Unknown variable name: " + m_variableName);
    }

    // auto type = resolveType(context->ProgramUnit, resolveParent(context));
    // if (!m_dereference && type->baseType == VariableBaseType::Pointer)
    // {
    //     return A;
    // }

    // Load the value.
    if (A->getAllocatedType()->isStructTy() || !context->loadValue)
        return A;


    return context->Builder->CreateLoad(A->getAllocatedType(), A, m_variableName.c_str());
}

std::shared_ptr<VariableType> VariableAccessNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parent)
{
    std::shared_ptr<VariableType> type;
    if (auto *functionDefinition = dynamic_cast<FunctionDefinitionNode *>(parent))
    {
        if (auto param = functionDefinition->getParam(m_variableName))
        {
            type = param.value().type;
        }
        if (auto var = functionDefinition->body()->getVariableDefinition(m_variableName))
        {
            type = var.value().variableType;
        }
    }
    else if (auto *functionCall = dynamic_cast<FunctionCallNode *>(parent))
    {
        if (auto unitFunctionDefinition = unit->getFunctionDefinition(functionCall->name()))
        {
            if (auto param = unitFunctionDefinition.value()->getParam(m_variableName))
            {
                type = param.value().type;
            }
            if (auto var = unitFunctionDefinition.value()->body()->getVariableDefinition(m_variableName))
            {
                type = var.value().variableType;
            }
        }
    }
    if (type == nullptr)
    {
        if (auto definition = unit->getVariableDefinition(m_variableName))
        {
            type = definition.value().variableType;
        }
    }

    if (m_dereference)
    {
        if (auto ptrType = std::dynamic_pointer_cast<PointerType>(type))
        {
            return ptrType->pointerBase;
        }
    }
    else if (type)
    {
        return type;
    }


    return std::make_shared<VariableType>();
}
