#pragma once
#include "ASTNode.h"


class TypeNode final : public ASTNode
{
public:
    TypeNode(const Token &token, const std::shared_ptr<VariableType> &m_variable_type) :
        ASTNode(token), m_variableType(m_variable_type)
    {
    }
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;

private:
    std::shared_ptr<VariableType> m_variableType;
};
