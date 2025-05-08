#pragma once

#include <vector>
#include "ASTNode.h"

class ForEachNode : public ASTNode
{
private:
    Token m_loopVariable;
    std::shared_ptr<ASTNode> m_loopExpression;
    std::vector<std::shared_ptr<ASTNode>> m_body;

public:
    ForEachNode(const Token &token, const Token &loopVariable, const std::shared_ptr<ASTNode> &loopExpression,
                const std::vector<std::shared_ptr<ASTNode>> &body);
    ~ForEachNode() override = default;

    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::optional<std::shared_ptr<ASTNode>> block() override;

    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
};
