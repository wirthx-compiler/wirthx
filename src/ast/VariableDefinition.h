#pragma once
#include <string>
#include "types/VariableType.h"

class ASTNode;
namespace llvm
{
    class Value;
}
struct VariableDefinition
{
    std::shared_ptr<VariableType> variableType;
    std::string variableName;
    Token token;
    std::string alias;
    size_t scopeId;
    std::shared_ptr<ASTNode> value = nullptr;
    llvm::Value *llvmValue = nullptr;
    bool constant = false;

    llvm::AllocaInst *generateCode(std::unique_ptr<Context> &context) const;
    llvm::Value *generateCodeForConstant(std::unique_ptr<Context> &context) const;
};
