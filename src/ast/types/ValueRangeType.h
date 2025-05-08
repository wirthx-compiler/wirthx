#pragma once
#include "RangeType.h"
#include "VariableType.h"

class ValueRangeType final : public VariableType, public RangeType
{
private:
    int64_t m_startValue = 0;
    int64_t m_endValue = 0;

public:
    ValueRangeType(const std::string &name, int64_t startValue, int64_t endValue);


    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;
    [[nodiscard]] size_t length() const;
    [[nodiscard]] llvm::Value *generateLowerBounds(const Token &token, std::unique_ptr<Context> &context) override;
    [[nodiscard]] llvm::Value *generateUpperBounds(const Token &token, std::unique_ptr<Context> &context) override;
    llvm::Value *generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context) override;
};
