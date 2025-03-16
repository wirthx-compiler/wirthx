#pragma once


#include "FunctionCallNode.h"

bool isKnownSystemCall(const std::string &name);

class SystemFunctionCallNode final : public FunctionCallNode
{
private:
    llvm::Value *codegen_setlength(std::unique_ptr<Context> &context, ASTNode *parent) const;
    llvm::Value *codegen_length(std::unique_ptr<Context> &context, ASTNode *parent) const;
    llvm::Value *find_target_fileout(std::unique_ptr<Context> &context, ASTNode *parent) const;
    llvm::Value *codegen_write(std::unique_ptr<Context> &context, ASTNode *parent) const;
    llvm::Value *codegen_writeln(std::unique_ptr<Context> &context, ASTNode *parent) const;
    llvm::Value *codegen_new(std::unique_ptr<Context> &context, ASTNode *parent) const;

public:
    SystemFunctionCallNode(const Token &token, std::string name, const std::vector<std::shared_ptr<ASTNode>> &args);

    static llvm::Value *codegen_assert(std::unique_ptr<Context> &context, ASTNode *parent, ASTNode *argument,
                                       llvm::Value *expression, const std::string &assertation);

    ~SystemFunctionCallNode() override = default;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unitNode, ASTNode *parentNode) override;
};
