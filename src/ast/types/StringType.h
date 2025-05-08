#pragma once
#include "RangeType.h"
#include "VariableType.h"


class StringType : public VariableType, public FieldAccessableType, public RangeType
{
private:
    llvm::Type *llvmType = nullptr;

public:
    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;
    llvm::Value *generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context) override;
    static std::shared_ptr<StringType> getString();

    bool operator==(const StringType &) const { return true; }

    llvm::Value *generateLengthValue(const Token &token, std::unique_ptr<Context> &context) override;
    llvm::Value *generateHighValue(const Token &token, std::unique_ptr<Context> &context) override;
    [[nodiscard]] llvm::Value *generateLowerBounds(const Token &token, std::unique_ptr<Context> &context) override;
    [[nodiscard]] llvm::Value *generateUpperBounds(const Token &token, std::unique_ptr<Context> &context) override;
};
