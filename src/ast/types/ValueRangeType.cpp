//
// Created by stefan on 23.03.25.
//

#include "ValueRangeType.h"

#include <cmath>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
ValueRangeType::ValueRangeType(const std::string &name, int64_t startValue, int64_t endValue) :
    VariableType(VariableBaseType::Integer, name), m_startValue(startValue), m_endValue(endValue)
{
}

llvm::Type *ValueRangeType::generateLlvmType(std::unique_ptr<Context> &context)
{

    return context->builder()->getIntNTy(length());
}
size_t ValueRangeType::length() const
{
    auto base = 1 + static_cast<size_t>(std::log2(m_endValue));
    base = (base > 32) ? 64 : 32;
    return base;
}
llvm::Value *ValueRangeType::generateLowerBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return llvm::ConstantInt::get(generateLlvmType(context), m_startValue);
}
llvm::Value *ValueRangeType::generateUpperBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return llvm::ConstantInt::get(generateLlvmType(context), m_endValue);
}
llvm::Value *ValueRangeType::generateFieldAccess(Token &token, llvm::Value *indexValue,
                                                 std::unique_ptr<Context> &context)
{
    return indexValue;
}
