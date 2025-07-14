#include "MinusNode.h"

#include <cassert>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include "ast/DoubleNode.h"
#include "ast/NumberNode.h"
#include "compiler/Context.h"

MinusNode::MinusNode(const Token &token, const std::shared_ptr<ASTNode> &node) : NumberNode(token, 0, 64), m_node(node)
{
}

void MinusNode::print() {}

llvm::Value *MinusNode::codegen(std::unique_ptr<Context> &context)
{
    const auto type = m_node->resolveType(context->programUnit(), resolveParent(context));

    switch (type->baseType)
    {

        case VariableBaseType::Integer:
        {
            if (const auto intNode = std::dynamic_pointer_cast<NumberNode>(m_node))
            {
                return llvm::ConstantInt::get(*context->context(), llvm::APInt(64, -intNode->getValue()));
            }
        }
        case VariableBaseType::Double:
        case VariableBaseType::Float:
            if (const auto doubleNode = std::dynamic_pointer_cast<DoubleNode>(m_node))
            {
                return llvm::ConstantFP::get(context->builder()->getDoubleTy(), -doubleNode->getValue());
            }
            break;
        default:
            break;
    }


    assert(false);
    return nullptr;
}
std::shared_ptr<VariableType> MinusNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return m_node->resolveType(unit, parentNode);
}
void MinusNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    ASTNode::typeCheck(unit, parentNode);
}
int64_t MinusNode::getValue() const
{
    if (const auto intNode = std::dynamic_pointer_cast<NumberNode>(m_node))
    {
        return -intNode->getValue();
    }
    if (const auto doubleNode = std::dynamic_pointer_cast<DoubleNode>(m_node))
    {
        return -doubleNode->getValue();
    }
    assert(false && "no compile time negation of the value possible");
    return 0;
}
