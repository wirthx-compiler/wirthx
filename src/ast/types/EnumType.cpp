//
// Created by stefan on 23.03.25.
//

#include "EnumType.h"

#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
EnumType::EnumType(const std::string &typeName) : VariableType(VariableBaseType::Enum, typeName) {}
void EnumType::addEnumValue(const std::string &name, const int64_t &value) { enumNamesWithValues.emplace(name, value); }
int64_t EnumType::lowerBounds()
{
    int64_t lowerBound = 0;
    for (const auto &[name, value]: enumNamesWithValues)
    {
        lowerBound = std::min(lowerBound, value);
    }
    return lowerBound;
}
int64_t EnumType::upperBounds()
{
    int64_t upperBound = 0;
    for (const auto &[name, value]: enumNamesWithValues)
    {
        upperBound = std::max(upperBound, value);
    }
    return upperBound;
}
int64_t EnumType::getValue(const std::string &string) const { return enumNamesWithValues.at(string); }
std::shared_ptr<EnumType> EnumType::getEnum(const std::string &name) { return std::make_shared<EnumType>(name); }
bool EnumType::hasEnumKey(const std::string &name) const { return enumNamesWithValues.contains(name); }
llvm::Type *EnumType::generateLlvmType(std::unique_ptr<Context> &context) { return context->Builder->getInt32Ty(); }
