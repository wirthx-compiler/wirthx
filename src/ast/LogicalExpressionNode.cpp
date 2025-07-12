#include "LogicalExpressionNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"


LogicalExpressionNode::LogicalExpressionNode(const Token &token, LogicalOperator op,
                                             const std::shared_ptr<ASTNode> &lhs, const std::shared_ptr<ASTNode> &rhs) :
    ASTNode(token), m_lhs(lhs), m_rhs(rhs), m_operator(op)
{
}

LogicalExpressionNode::LogicalExpressionNode(const Token &token, LogicalOperator op,
                                             const std::shared_ptr<ASTNode> &rhs) :
    ASTNode(token), m_lhs(nullptr), m_rhs(rhs), m_operator(op)
{
}

void LogicalExpressionNode::print()
{
    switch (m_operator)
    {
        case LogicalOperator::AND:
        {
            m_lhs->print();
            std::cout << " and ";
            m_rhs->print();
        }
        break;
        case LogicalOperator::OR:
        {
            m_lhs->print();
            std::cout << " or ";
            m_rhs->print();
        }
        break;
        case LogicalOperator::NOT:
        {

            std::cout << " not ";
            m_rhs->print();
        }
        break;
        default:
            break;
    }
}

llvm::Value *LogicalExpressionNode::codegen(std::unique_ptr<Context> &context)
{

    switch (m_operator)
    {
        case LogicalOperator::AND:
            return context->builder()->CreateAnd(m_lhs->codegen(context), m_rhs->codegen(context));
        case LogicalOperator::OR:
            return context->builder()->CreateOr(m_lhs->codegen(context), m_rhs->codegen(context));
        case LogicalOperator::NOT:
            return context->builder()->CreateNot(m_rhs->codegen(context));
        default:
            assert(false && "unknown logical operator");
    }
    return nullptr;
}
std::shared_ptr<VariableType> LogicalExpressionNode::resolveType(const std::unique_ptr<UnitNode> &unit,
                                                                 ASTNode *parentNode)
{
    return VariableType::getBoolean();
}
Token LogicalExpressionNode::expressionToken()
{
    auto start = m_lhs->expressionToken().sourceLocation.byte_offset;
    auto end = m_rhs->expressionToken().sourceLocation.byte_offset;
    if (start == end)
        return m_lhs->expressionToken();
    Token token = ASTNode::expressionToken();
    token.sourceLocation.num_bytes = end - start;
    token.sourceLocation.byte_offset = start;
    return token;
}
