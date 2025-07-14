#include "BooleanNode.h"
#include <iostream>
#include "compiler/Context.h"
#include "llvm/IR/IRBuilder.h"


BooleanNode::BooleanNode(const Token &token, const bool value) : ASTNode(token), m_value(value) {}

void BooleanNode::print()
{
    if (m_value)
        std::cout << "true";
    else
        std::cout << "false";
}

llvm::Value *BooleanNode::codegen(std::unique_ptr<Context> &context)
{
    if (m_value)
        return context->builder()->getTrue();

    return context->builder()->getFalse();
}

std::shared_ptr<VariableType> BooleanNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return VariableType::getBoolean();
}
