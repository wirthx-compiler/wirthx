#include "CharConstantNode.h"
#include "compiler/Context.h"
#include "llvm/IR/Constants.h"

CharConstantNode::CharConstantNode(const Token &token, std::string_view literal) :
    ASTNode(token), m_literal(literal.at(0))
{
}

void CharConstantNode::print() {}
llvm::Value *CharConstantNode::codegen(std::unique_ptr<Context> &context)
{
    return llvm::ConstantInt::get(*context->context(), llvm::APInt(8, m_literal));
}

std::shared_ptr<VariableType> CharConstantNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return VariableType::getCharacter();
}
