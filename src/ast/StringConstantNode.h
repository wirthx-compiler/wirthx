#pragma once

#include <llvm-18/llvm/IR/GlobalVariable.h>
#include <string>

#include "ASTNode.h"

class StringConstantNode : public ASTNode
{
private:
    std::string m_literal;
    llvm::GlobalVariable *generateConstant(std::unique_ptr<Context> &context, std::string &result) const;

public:
    StringConstantNode(const Token &token, const std::string &literal);
    ~StringConstantNode() override = default;
    void print() override;

    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    llvm::Value *codegenForTargetType(std::unique_ptr<Context> &context,
                                      const std::shared_ptr<VariableType> &targetType) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
};
