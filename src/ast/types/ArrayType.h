#pragma once

#include "RangeType.h"
#include "VariableType.h"


class ArrayType final : public VariableType, public FieldAccessableType, public RangeType
{
private:
    llvm::Type *llvmType = nullptr;

public:
    size_t low;
    size_t high;
    bool isDynArray;


    std::shared_ptr<VariableType> arrayBase;

    static std::shared_ptr<ArrayType> getFixedArray(size_t low, size_t heigh,
                                                    const std::shared_ptr<VariableType> &baseType);
    static std::shared_ptr<ArrayType> getDynArray(const std::shared_ptr<VariableType> &baseType);

    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;
    llvm::Value *generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context) override;
    llvm::Value *generateLengthValue(const Token &token, std::unique_ptr<Context> &context) override;
    llvm::Value *getLowValue(std::unique_ptr<Context> &context) override;
    llvm::Value *generateHighValue(const Token &token, std::unique_ptr<Context> &context) override;


    bool operator==(const ArrayType &other) const
    {
        return this->low == other.low && this->high == other.high && this->arrayBase == other.arrayBase &&
               this->isDynArray == other.isDynArray;
    }
    [[nodiscard]] llvm::Value *generateLowerBounds(const Token &token, std::unique_ptr<Context> &context) override;
    [[nodiscard]] llvm::Value *generateUpperBounds(const Token &token, std::unique_ptr<Context> &context) override;
};
