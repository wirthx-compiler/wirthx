#include <cassert>
#include <iostream>
#include <llvm/IR/IRBuilder.h>

#include "ComparissionNode.h"
#include "UnitNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"

ComparrisionNode::ComparrisionNode(const Token &operatorToken, const CMPOperator op,
                                   const std::shared_ptr<ASTNode> &lhs, const std::shared_ptr<ASTNode> &rhs) :
    ASTNode(operatorToken), m_operatorToken(operatorToken), m_lhs(lhs), m_rhs(rhs), m_operator(op)
{
}

void ComparrisionNode::print()
{
    m_lhs->print();
    switch (m_operator)
    {
        case CMPOperator::EQUALS:
            std::cout << "=";
            break;
        case CMPOperator::GREATER:
            std::cout << ">";
            break;
        case CMPOperator::GREATER_EQUAL:
            std::cout << ">=";
            break;
        case CMPOperator::LESS:
            std::cout << "<";
            break;
        case CMPOperator::LESS_EQUAL:
            std::cout << "<=";
            break;
        default:
            break;
    }
    m_rhs->print();
}


llvm::Value *ComparrisionNode::codegen(std::unique_ptr<Context> &context)
{
    auto lhs = m_lhs->codegen(context);
    assert(lhs && "lhs of the comparison is null");
    auto rhs = m_rhs->codegen(context);
    assert(rhs && "rhs of the comparison is null");

    llvm::CmpInst::Predicate pred = llvm::CmpInst::ICMP_EQ;
    if (lhs->getType()->isDoubleTy() || lhs->getType()->isFloatTy())
    {
        pred = llvm::CmpInst::FCMP_OEQ;
        switch (m_operator)
        {
            case CMPOperator::NOT_EQUALS:
                pred = llvm::CmpInst::FCMP_ONE;
                break;
            case CMPOperator::EQUALS:

                break;
            case CMPOperator::GREATER:
                pred = llvm::CmpInst::FCMP_OGT;
                break;
            case CMPOperator::GREATER_EQUAL:
                pred = llvm::CmpInst::FCMP_OGE;
                break;
            case CMPOperator::LESS:
                pred = llvm::CmpInst::FCMP_OLT;
                break;
            case CMPOperator::LESS_EQUAL:
                pred = llvm::CmpInst::FCMP_OLE;
                break;
            default:
                break;
        }
    }
    else
    {
        switch (m_operator)
        {
            case CMPOperator::NOT_EQUALS:
                pred = llvm::CmpInst::ICMP_NE;
                break;
            case CMPOperator::EQUALS:

                break;
            case CMPOperator::GREATER:
                pred = llvm::CmpInst::ICMP_SGT;
                break;
            case CMPOperator::GREATER_EQUAL:
                pred = llvm::CmpInst::ICMP_SGE;
                break;
            case CMPOperator::LESS:
                pred = llvm::CmpInst::ICMP_SLT;
                break;
            case CMPOperator::LESS_EQUAL:
                pred = llvm::CmpInst::ICMP_SLE;
                break;
            default:
                break;
        }
    }

    ASTNode *parent = context->ProgramUnit.get();
    if (context->TopLevelFunction)
    {
        if (auto def = context->ProgramUnit->getFunctionDefinition(context->TopLevelFunction->getName().str()))
        {
            parent = def.value().get();
        }
    }

    auto lhsType = m_lhs->resolveType(context->ProgramUnit, parent);
    auto rhsType = m_rhs->resolveType(context->ProgramUnit, parent);

    if (*lhsType == *rhsType && lhsType->baseType == VariableBaseType::Integer)
    {
        if (lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy())
        {
            const unsigned maxBitWith =
                    std::max(lhs->getType()->getIntegerBitWidth(), rhs->getType()->getIntegerBitWidth());
            const auto targetType = llvm::IntegerType::get(*context->TheContext, maxBitWith);
            if (maxBitWith != lhs->getType()->getIntegerBitWidth())
            {
                lhs = context->Builder->CreateIntCast(lhs, targetType, true, "lhs_cast");
            }
            if (maxBitWith != rhs->getType()->getIntegerBitWidth())
            {
                rhs = context->Builder->CreateIntCast(rhs, targetType, true, "rhs_cast");
            }
        }
    }
    else if (lhsType->baseType == VariableBaseType::Pointer || rhsType->baseType == VariableBaseType::Pointer)
    {
        auto targetType = llvm::IntegerType::getInt64Ty(*context->TheContext);
        const unsigned maxBitWith = targetType->getIntegerBitWidth();
        if (lhsType->baseType == VariableBaseType::Pointer)
        {
            lhs = context->Builder->CreateBitOrPointerCast(lhs, targetType);
        }
        else if (lhsType->baseType == VariableBaseType::Integer)
        {
            lhs = context->Builder->CreateIntCast(lhs, targetType, true, "lhs_cast");
        }
        if (rhsType->baseType == VariableBaseType::Pointer)
        {
            rhs = context->Builder->CreateBitOrPointerCast(rhs, targetType);
        }
        else if (lhsType->baseType == VariableBaseType::Integer)
        {
            rhs = context->Builder->CreateIntCast(rhs, targetType, true, "rhs_cast");
        }

        if (maxBitWith != lhs->getType()->getIntegerBitWidth())
        {
            lhs = context->Builder->CreateIntCast(lhs, targetType, true, "lhs_cast");
        }
        if (maxBitWith != rhs->getType()->getIntegerBitWidth())
        {
            rhs = context->Builder->CreateIntCast(rhs, targetType, true, "rhs_cast");
        }
    }
    else if (lhsType && (lhsType->baseType == VariableBaseType::Float or lhsType->baseType == VariableBaseType::Double))
    {
        if (lhs->getType()->isFloatTy() && rhs->getType()->isDoubleTy())
        {
            lhs = context->Builder->CreateFPCast(lhs, rhs->getType());
        }
        else if (lhs->getType()->isDoubleTy() && rhs->getType()->isFloatTy())
        {
            lhs = context->Builder->CreateFPCast(rhs, lhs->getType());
        }
    }
    else
    {
        if (lhsType && lhsType->baseType == VariableBaseType::String)
        {

            if (llvm::Function *CalleeF = context->TheModule->getFunction("comparestr(string,string)"))
            {
                std::vector<llvm::Value *> ArgsV = {lhs, rhs};

                lhs = context->Builder->CreateCall(CalleeF, ArgsV);
                rhs = context->Builder->getInt32(0);
            }
        }
    }

    return context->Builder->CreateCmp(pred, lhs, rhs);
}
void ComparrisionNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    const auto lhsType = m_lhs->resolveType(unit, parentNode);
    const auto rhsType = m_rhs->resolveType(unit, parentNode);
    if (*lhsType != *rhsType)
    {
        throw CompilerException(ParserError{.token = m_operatorToken,
                                            .message = "the comparison of \"" + lhsType->typeName + "\" and \"" +
                                                       rhsType->typeName +
                                                       "\" is not possible because the types are not the same"});
    }
}
std::shared_ptr<VariableType> ComparrisionNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return VariableType::getBoolean();
}
Token ComparrisionNode::expressionToken()
{
    const auto start = m_lhs->expressionToken().sourceLocation.byte_offset;
    const auto end = m_rhs->expressionToken().sourceLocation.byte_offset;
    if (start == end)
        return m_operatorToken;
    Token token = ASTNode::expressionToken();
    token.sourceLocation.num_bytes = end - start + m_rhs->expressionToken().sourceLocation.num_bytes;
    token.sourceLocation.byte_offset = start;
    return token;
}
