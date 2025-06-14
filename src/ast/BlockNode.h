#pragma once
#include <optional>
#include <vector>

#include "ASTNode.h"
#include "ast/VariableDefinition.h"

class BlockNode : public ASTNode
{
private:
    std::vector<std::shared_ptr<ASTNode>> m_expressions;
    std::vector<VariableDefinition> m_variableDefinitions;
    std::string m_blockname;

public:
    BlockNode(const Token &token, const std::vector<VariableDefinition> &variableDefinitions,
              const std::vector<std::shared_ptr<ASTNode>> &expressions);
    ~BlockNode() override = default;

    void print() override;
    void setBlockName(const std::string &name);
    void codegenConstantDefinitions(std::unique_ptr<Context> &context);
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::optional<VariableDefinition> getVariableDefinition(const std::string &name) const;
    void addVariableDefinition(VariableDefinition definition);
    void preappendExpression(std::shared_ptr<ASTNode> node);
    void appendExpression(const std::shared_ptr<ASTNode> &node);
    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
    std::vector<VariableDefinition> getVariableDefinitions();
    std::optional<std::shared_ptr<ASTNode>> getNodeByToken(const Token &token) const;
};
