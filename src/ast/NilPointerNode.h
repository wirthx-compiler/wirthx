#pragma once
#include "ASTNode.h"


class NilPointerNode final : public ASTNode
{
public:
    explicit NilPointerNode(const Token &token);
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
};
