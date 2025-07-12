#include "EnumType.h"

#include <llvm/IR/IRBuilder.h>
#include <ranges>

#include "compiler/Context.h"
#include "exceptions/CompilerException.h"
EnumType::EnumType(const std::string &typeName) : VariableType(VariableBaseType::Enum, typeName) {}
void EnumType::addEnumValue(const std::string &name, const int64_t &value) { enumNamesWithValues.emplace(name, value); }
llvm::Value *EnumType::generateLowerBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return llvm::ConstantInt::get(generateLlvmType(context), lowerBounds());
}
llvm::Value *EnumType::generateUpperBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return llvm::ConstantInt::get(generateLlvmType(context), upperBounds());
}
int64_t EnumType::lowerBounds()
{
    int64_t lowerBound = 0;
    for (const auto &value: enumNamesWithValues | std::views::values)
    {
        lowerBound = std::min(lowerBound, value);
    }
    return lowerBound;
}
int64_t EnumType::upperBounds()
{
    int64_t upperBound = 0;
    for (const auto &value: enumNamesWithValues | std::views::values)
    {
        upperBound = std::max(upperBound, value);
    }
    return upperBound;
}
int64_t EnumType::getValue(const std::string &string) const { return enumNamesWithValues.at(string); }
std::shared_ptr<EnumType> EnumType::getEnum(const std::string &name) { return std::make_shared<EnumType>(name); }
bool EnumType::hasEnumKey(const std::string &name) const { return enumNamesWithValues.contains(name); }
llvm::Type *EnumType::generateLlvmType(std::unique_ptr<Context> &context) { return context->builder()->getInt32Ty(); }
llvm::Value *EnumType::generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context)
{
    throw CompilerException(ParserError{.token = token, .message = "EnumType does not support field access."});
}
