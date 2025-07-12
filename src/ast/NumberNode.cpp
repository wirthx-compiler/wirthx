#include "NumberNode.h"
#include <iostream>
#include <llvm/IR/Constants.h>

#include "compiler/Context.h"


NumberNode::NumberNode(const Token &token, int64_t value, size_t numBits) :
    ASTNode(token), m_value(value), m_numBits(numBits)
{
}

void NumberNode::print() { std::cout << m_value; }

llvm::Value *NumberNode::codegen(std::unique_ptr<Context> &context)
{
    return llvm::ConstantInt::get(*context->context(), llvm::APInt(static_cast<unsigned>(m_numBits), m_value));
}

std::shared_ptr<VariableType> NumberNode::resolveType([[maybe_unused]] const std::unique_ptr<UnitNode> &unit,
                                                      ASTNode *parentNode)
{
    return VariableType::getInteger(m_numBits);
}

int64_t NumberNode::getValue() const { return m_value; }
