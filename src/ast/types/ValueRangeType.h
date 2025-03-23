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
    int64_t lowerBounds() override;
    int64_t upperBounds() override;

    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;
    size_t length() const;
};
