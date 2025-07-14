#include "AddressNode.h"

#include <compare.h>
#include <compiler/Context.h>
#include "UnitNode.h"
#include "VariableAccessNode.h"

AddressNode::AddressNode(const Token &token) : ASTNode(token), m_variableName(token.lexical()) {}

void AddressNode::print() {}

llvm::Value *AddressNode::codegen(std::unique_ptr<Context> &context)
{
    const auto allocatedValue = context->findValue(to_lower(m_variableName));

    if (!allocatedValue)
    {
        return LogErrorV("Unknown variable name: " + m_variableName);
    }
    // Load the value.
    return allocatedValue.value();
}
std::shared_ptr<VariableType> AddressNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    VariableAccessNode node(this->expressionToken(), false);
    const auto nodeType = node.resolveType(unit, parentNode);
    return PointerType::getPointerTo(nodeType);
}
void AddressNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    ASTNode::typeCheck(unit, parentNode);
}
