#pragma once
#include <vector>
#include "ASTNode.h"

class IfConditionNode : public ASTNode
{
private:
    std::shared_ptr<ASTNode> m_conditionNode;
    std::vector<std::shared_ptr<ASTNode>> m_ifExpressions;
    std::vector<std::shared_ptr<ASTNode>> m_elseExpressions;

    llvm::Value *codegenIf(std::unique_ptr<Context> &context);
    llvm::Value *codegenIfElse(std::unique_ptr<Context> &context);

public:
    IfConditionNode(const Token &token, const std::shared_ptr<ASTNode> &conditionNode,
                    const std::vector<std::shared_ptr<ASTNode>> &ifExpressions,
                    const std::vector<std::shared_ptr<ASTNode>> &elseExpressions);
    ~IfConditionNode() override = default;
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;

    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
    [[nodiscard]] bool tokenIsPartOfNode(const Token &token) const override;
};
