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
    llvm::Value *allocatedValue = context->namedAllocation(m_variableName);

    llvm::Type *type = nullptr;

    if (!allocatedValue)
    {
        for (auto &arg: context->currentFunction()->args())
        {
            if (arg.getName() == m_variableName)
            {
                auto functionDefinition =
                        context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str());
                if (functionDefinition.has_value())
                {
                    const auto argType = functionDefinition.value()->getParam(arg.getArgNo());
                    type = argType->type->generateLlvmType(context);
                    const auto argValue = context->currentFunction()->getArg(arg.getArgNo());
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
        type = context->namedAllocation(m_variableName)->getAllocatedType();
    }


    if (!allocatedValue)
        return LogErrorV("Unknown variable name for assignment: " + m_variableName);


    auto expressionResult = m_expression->codegen(context);

    if (type->isIntegerTy() && expressionResult->getType()->isIntegerTy())
    {
        const auto targetType = llvm::IntegerType::get(*context->context(), type->getIntegerBitWidth());
        if (type->getIntegerBitWidth() != expressionResult->getType()->getIntegerBitWidth())
        {
            expressionResult = context->builder()->CreateIntCast(expressionResult, targetType, true, "lhs_cast");
        }

        context->builder()->CreateStore(expressionResult, allocatedValue);
        // context->NamedValues[m_variableName] = expressionResult;
        return allocatedValue;
    }
    if (type->isIEEELikeFPTy() && expressionResult->getType()->isIEEELikeFPTy())
    {
        allocatedValue = context->builder()->CreateFPCast(allocatedValue, expressionResult->getType());
        context->builder()->CreateStore(expressionResult, allocatedValue);
        // context->NamedValues[m_variableName] = expressionResult;
        return allocatedValue;
    }

    if (type->isStructTy() && expressionResult->getType()->isPointerTy())
    {
        // we might have to free the value
        auto variable_definition = context->programUnit()->getVariableDefinition(m_variableName);
        std::shared_ptr<VariableType> varType =
                (variable_definition) ? variable_definition.value().variableType : nullptr;
        if (context->currentFunction() && !variable_definition)
        {
            if (const auto def =
                        context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str()))
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
        //             context->builder()->CreateStructGEP(type, stringStructPtr, 2, m_variableName + ".ptr.offset");
        //     // auto strValuePtr = context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()),
        //     //                                                 arrayPointerOffset, m_variableName + ".loaded");
        //
        //     context->builder()->CreateCall(context->module()->getFunction("freemem(integer8_ptr)"),
        //                                  llvm::ArrayRef<llvm::Value *>{arrayPointerOffset});
        // }

        auto llvmArgType = type;

        auto memcpyCall = llvm::Intrinsic::getDeclaration(
                context->module().get(), llvm::Intrinsic::memcpy,
                {context->builder()->getPtrTy(), context->builder()->getPtrTy(), context->builder()->getInt64Ty()});
        std::vector<llvm::Value *> memcopyArgs;

        const llvm::DataLayout &DL = context->module()->getDataLayout();
        uint64_t structSize = DL.getTypeAllocSize(llvmArgType);


        memcopyArgs.push_back(context->builder()->CreateBitCast(allocatedValue, context->builder()->getPtrTy()));
        memcopyArgs.push_back(context->builder()->CreateBitCast(expressionResult, context->builder()->getPtrTy()));
        memcopyArgs.push_back(context->builder()->getInt64(structSize));
        memcopyArgs.push_back(context->builder()->getFalse());

        context->builder()->CreateCall(memcpyCall, memcopyArgs);

        return expressionResult;
    }
    if (expressionResult->getType()->isPointerTy())
    {
        if (llvm::isa<llvm::AllocaInst>(expressionResult))
        {
            context->setNamedAllocation(m_variableName, llvm::cast<llvm::AllocaInst>(expressionResult));
            return expressionResult;
        }

        if (m_dereference)
        {
            const auto expressionType = m_expression->resolveType(context->programUnit(), resolveParent(context));
            const auto dereferenced = context->builder()->CreateLoad(context->builder()->getPtrTy(), allocatedValue,
                                                                     "deref." + m_variableName);
            if (expressionType->baseType == VariableBaseType::String)
            {

                const auto arrayPointerOffset = context->builder()->CreateStructGEP(
                        expressionType->generateLlvmType(context), expressionResult, 2, "string.ptr.offset");
                context->builder()->CreateStore(
                        context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()),
                                                       arrayPointerOffset),
                        dereferenced);
            }
            else
            {
                context->builder()->CreateStore(expressionResult, dereferenced);
            }
            return expressionResult;
        }

        // auto resultType = m_expression->resolveType(context->programUnit(), resolveParent(context));

        // expressionResult = context->builder()->CreateLoad(resultType->generateLlvmType(context), expressionResult);
    }

    context->builder()->CreateStore(expressionResult, allocatedValue);
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

    m_expression->typeCheck(unit, parentNode);
}
bool VariableAssignmentNode::tokenIsPartOfNode(const Token &token) const
{
    if (ASTNode::tokenIsPartOfNode(token))
        return true;
    return m_expression->tokenIsPartOfNode(token);
}
