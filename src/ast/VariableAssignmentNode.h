#pragma once
#include <string>
#include "ASTNode.h"

class VariableAssignmentNode : public ASTNode
{
private:
    Token m_variable;
    std::string m_variableName;
    std::shared_ptr<ASTNode> m_expression;
    bool m_dereference;

public:
    VariableAssignmentNode(const Token &variableName, const std::shared_ptr<ASTNode> &expression, bool dereference);
    ~VariableAssignmentNode() override = default;
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
    [[nodiscard]] std::string variableName() const { return m_variableName; }
    bool tokenIsPartOfNode(const Token &token) const override;
};
