//
// Created by stefan on 12.04.25.
//

#include "TypeNode.h"

#include <cassert>
void TypeNode::print() {}
llvm::Value *TypeNode::codegen(std::unique_ptr<Context> &context)
{
    assert(false && "TypeNode::codegen should not be called");
    return nullptr;
}
std::shared_ptr<VariableType> TypeNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return m_variableType;
}
