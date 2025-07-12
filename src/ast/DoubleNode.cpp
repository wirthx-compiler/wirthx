#include "DoubleNode.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
DoubleNode::DoubleNode(const Token &token, const double value) : ASTNode(token), m_value(value) {}
void DoubleNode::print() {}
llvm::Value *DoubleNode::codegen(std::unique_ptr<Context> &context)
{
    return llvm::ConstantFP::get(context->builder()->getDoubleTy(), m_value);
}
std::shared_ptr<VariableType> DoubleNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return VariableType::getDouble();
}
double DoubleNode::getValue() const { return m_value; }
