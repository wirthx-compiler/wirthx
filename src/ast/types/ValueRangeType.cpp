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
int64_t ValueRangeType::lowerBounds() { return m_startValue; }
int64_t ValueRangeType::upperBounds() { return m_endValue; }
llvm::Type *ValueRangeType::generateLlvmType(std::unique_ptr<Context> &context)
{

    return context->Builder->getIntNTy(length());
}
size_t ValueRangeType::length() const
{
    auto base = 1 + static_cast<size_t>(std::log2(m_endValue));
    base = (base > 32) ? 64 : 32;
    return base;
}
