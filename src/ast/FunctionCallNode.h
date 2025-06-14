#pragma once

#include <string>
#include <vector>
#include "ASTNode.h"

class FunctionCallNode : public ASTNode
{
protected:
    std::string m_name;
    std::vector<std::shared_ptr<ASTNode>> m_args;
    std::string callSignature(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) const;

public:
    FunctionCallNode(const Token &token, std::string name, const std::vector<std::shared_ptr<ASTNode>> &args);
    ~FunctionCallNode() override = default;
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unitNode, ASTNode *parentNode) override;

    std::string name();

    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
    [[nodiscard]] bool tokenIsPartOfNode(const Token &token) const override;
};
