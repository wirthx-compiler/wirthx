#pragma once
#include <string>
#include "ASTNode.h"

class VariableAccessNode : public ASTNode
{
private:
    std::string m_variableName;
    bool m_dereference;

public:
    explicit VariableAccessNode(const Token &token, bool dereference);
    ~VariableAccessNode() override = default;
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;

    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parent) override;
    std::string variableName() const { return m_variableName; }
};
