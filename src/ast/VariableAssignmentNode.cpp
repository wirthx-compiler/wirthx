#include "VariableAssignmentNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include "FunctionCallNode.h"
#include "UnitNode.h"
#include "VariableAccessNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"

VariableAssignmentNode::VariableAssignmentNode(const Token &variableName, const std::shared_ptr<ASTNode> &expression,
                                               bool dereference) :
    ASTNode(variableName), m_variable(variableName), m_variableName(std::string(m_variable.lexical())),
    m_expression(expression), m_dereference(dereference)
{
}

void VariableAssignmentNode::print()
{
    std::cout << m_variableName << ":=";
    m_expression->print();
    std::cout << ";\n";
}

llvm::Value *VariableAssignmentNode::codegen(std::unique_ptr<Context> &context)
{
    // Look this variable up in the function.
    llvm::Value *allocatedValue = context->NamedAllocations[m_variableName];

    llvm::Type *type = nullptr;

    if (!allocatedValue)
    {
        for (auto &arg: context->TopLevelFunction->args())
        {
            if (arg.getName() == m_variableName)
            {
                auto functionDefinition =
                        context->ProgramUnit->getFunctionDefinition(context->TopLevelFunction->getName().str());
                if (functionDefinition.has_value())
                {
                    const auto argType = functionDefinition.value()->getParam(arg.getArgNo());
                    type = argType->type->generateLlvmType(context);
                    const auto argValue = context->TopLevelFunction->getArg(arg.getArgNo());
                    if (argType->isReference)
                    {

                        allocatedValue = argValue;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        type = context->NamedAllocations[m_variableName]->getAllocatedType();
    }


    if (!allocatedValue)
        return LogErrorV("Unknown variable name for assignment: " + m_variableName);


    auto expressionResult = m_expression->codegen(context);

    if (type->isIntegerTy() && expressionResult->getType()->isIntegerTy())
    {
        const auto targetType = llvm::IntegerType::get(*context->TheContext, type->getIntegerBitWidth());
        if (type->getIntegerBitWidth() != expressionResult->getType()->getIntegerBitWidth())
        {
            expressionResult = context->Builder->CreateIntCast(expressionResult, targetType, true, "lhs_cast");
        }

        context->Builder->CreateStore(expressionResult, allocatedValue);
        // context->NamedValues[m_variableName] = expressionResult;
        return allocatedValue;
    }
    if (type->isIEEELikeFPTy() && expressionResult->getType()->isIEEELikeFPTy())
    {
        allocatedValue = context->Builder->CreateFPCast(allocatedValue, expressionResult->getType());
        context->Builder->CreateStore(expressionResult, allocatedValue);
        // context->NamedValues[m_variableName] = expressionResult;
        return allocatedValue;
    }

    if (type->isStructTy() && expressionResult->getType()->isPointerTy())
    {
        // we might have to free the value
        auto variable_definition = context->ProgramUnit->getVariableDefinition(m_variableName);
        std::shared_ptr<VariableType> varType =
                (variable_definition) ? variable_definition.value().variableType : nullptr;
        if (context->TopLevelFunction && !variable_definition)
        {
            if (const auto def =
                        context->ProgramUnit->getFunctionDefinition(context->TopLevelFunction->getName().str()))
            {
                variable_definition = def.value()->body()->getVariableDefinition(m_variableName);

                if (!variable_definition)
                {
                    varType = def.value()->getParam(m_variableName)->type;
                }
                else
                {
                    varType = variable_definition->variableType;
                }
            }
        }
        // TODO not everything is allocated by malloc this needs proper ref counting
        // if (varType->baseType == VariableBaseType::String)
        // {
        //     auto stringStructPtr = allocatedValue;
        //     const auto arrayPointerOffset =
        //             context->Builder->CreateStructGEP(type, stringStructPtr, 2, m_variableName + ".ptr.offset");
        //     // auto strValuePtr = context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext),
        //     //                                                 arrayPointerOffset, m_variableName + ".loaded");
        //
        //     context->Builder->CreateCall(context->TheModule->getFunction("freemem(integer8_ptr)"),
        //                                  llvm::ArrayRef<llvm::Value *>{arrayPointerOffset});
        // }

        auto llvmArgType = type;

        auto memcpyCall = llvm::Intrinsic::getDeclaration(
                context->TheModule.get(), llvm::Intrinsic::memcpy,
                {context->Builder->getPtrTy(), context->Builder->getPtrTy(), context->Builder->getInt64Ty()});
        std::vector<llvm::Value *> memcopyArgs;

        const llvm::DataLayout &DL = context->TheModule->getDataLayout();
        uint64_t structSize = DL.getTypeAllocSize(llvmArgType);


        memcopyArgs.push_back(context->Builder->CreateBitCast(allocatedValue, context->Builder->getPtrTy()));
        memcopyArgs.push_back(context->Builder->CreateBitCast(expressionResult, context->Builder->getPtrTy()));
        memcopyArgs.push_back(context->Builder->getInt64(structSize));
        memcopyArgs.push_back(context->Builder->getFalse());

        context->Builder->CreateCall(memcpyCall, memcopyArgs);

        return expressionResult;
    }
    if (expressionResult->getType()->isPointerTy())
    {
        if (llvm::isa<llvm::AllocaInst>(expressionResult))
        {
            context->NamedAllocations[m_variableName] = llvm::cast<llvm::AllocaInst>(expressionResult);
            return expressionResult;
        }

        if (m_dereference)
        {
            const auto expressionType = m_expression->resolveType(context->ProgramUnit, resolveParent(context));
            const auto dereferenced = context->Builder->CreateLoad(context->Builder->getPtrTy(), allocatedValue,
                                                                   "deref." + m_variableName);
            if (expressionType->baseType == VariableBaseType::String)
            {

                const auto arrayPointerOffset = context->Builder->CreateStructGEP(
                        expressionType->generateLlvmType(context), expressionResult, 2, "string.ptr.offset");
                context->Builder->CreateStore(
                        context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext),
                                                     arrayPointerOffset),
                        dereferenced);
            }
            else
            {
                context->Builder->CreateStore(expressionResult, dereferenced);
            }
            return expressionResult;
        }

        // auto resultType = m_expression->resolveType(context->ProgramUnit, resolveParent(context));

        // expressionResult = context->Builder->CreateLoad(resultType->generateLlvmType(context), expressionResult);
    }

    context->Builder->CreateStore(expressionResult, allocatedValue);
    return expressionResult;
}
void VariableAssignmentNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    if (parentNode != unit.get())
    {
        if (const auto functionDef = dynamic_cast<FunctionDefinitionNode *>(parentNode))
        {
            if (const auto varType = functionDef->body()->getVariableDefinition(m_variableName))
            {
                const auto expressionType = m_expression->resolveType(unit, parentNode);
                if (*expressionType != *varType.value().variableType)
                {

                    throw CompilerException(ParserError{.token = m_variable,
                                                        .message = "the type for the variable \"" + m_variableName +
                                                                   "\" is \"" + varType.value().variableType->typeName +
                                                                   "\" but a \"" + expressionType->typeName +
                                                                   "\" was assigned."});
                }
            }
        }
    }
    else if (const auto varType = unit->getVariableDefinition(m_variableName))
    {
        const auto expressionType = m_expression->resolveType(unit, parentNode);
        if (*expressionType != *varType.value().variableType)
        {

            throw CompilerException(ParserError{.token = m_variable,
                                                .message = "the type for the variable \"" + m_variableName +
                                                           "\" is \"" + varType.value().variableType->typeName +
                                                           "\" but a \"" + expressionType->typeName +
                                                           "\" was assigned."});
        }
    }
}
bool VariableAssignmentNode::tokenIsPartOfNode(const Token &token) const
{
    if (ASTNode::tokenIsPartOfNode(token))
        return true;
    return m_expression->tokenIsPartOfNode(token);
}
