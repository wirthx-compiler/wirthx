#pragma once
#include <memory>
#include <optional>
#include "types/VariableType.h"

namespace llvm
{
    class Value;

};
struct Context;


class UnitNode;

class ASTNode
{
    Token m_token;

public:
    explicit ASTNode(const Token &token);
    virtual ~ASTNode() = default;

    virtual void print() = 0;
    virtual llvm::Value *codegen(std::unique_ptr<Context> &context) = 0;
    virtual llvm::Value *codegenForTargetType(std::unique_ptr<Context> &context,
                                              const std::shared_ptr<VariableType> &targetType)
    {
        return codegen(context);
    }

    virtual std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode);
    virtual std::optional<std::shared_ptr<ASTNode>> block() { return std::nullopt; }
    virtual void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) {};

    virtual Token expressionToken() { return m_token; }
    static ASTNode *resolveParent(const std::unique_ptr<Context> &context);
};
