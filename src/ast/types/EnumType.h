#pragma once

#include <map>
#include "RangeType.h"
#include "VariableType.h"


class EnumType : public RangeType, public VariableType
{
private:
    std::map<std::string, int64_t> enumNamesWithValues;

public:
    explicit EnumType(const std::string &typeName = "");
    ~EnumType() override = default;

    void addEnumValue(const std::string &name, const int64_t &value);

    [[nodiscard]] int64_t lowerBounds() override;
    [[nodiscard]] int64_t upperBounds() override;
    [[nodiscard]] int64_t getValue(const std::string &string) const;

    [[nodiscard]] static std::shared_ptr<EnumType> getEnum(const std::string &name = "");
    [[nodiscard]] bool hasEnumKey(const std::string &name) const;

    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;
};
