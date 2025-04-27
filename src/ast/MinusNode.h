#pragma once
#include "NumberNode.h"
#include "ast/ASTNode.h"


class MinusNode final : public NumberNode
{
private:
    std::shared_ptr<ASTNode> m_node;

public:
    MinusNode(const Token &token, const std::shared_ptr<ASTNode> &node);
    ~MinusNode() override = default;
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;

    int64_t getValue() const override;
};
