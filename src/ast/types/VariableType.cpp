#include "VariableType.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
#include "exceptions/CompilerException.h"

VariableType::VariableType(const VariableBaseType baseType, const std::string &typeName) :
    baseType(baseType), typeName(typeName)
{
}
bool VariableType::isSimpleType() const
{
    switch (this->baseType)
    {
        case VariableBaseType::Array:
        case VariableBaseType::Struct:
        case VariableBaseType::String:
            return false;
        case VariableBaseType::Pointer:
        case VariableBaseType::Integer:
        case VariableBaseType::Character:
        case VariableBaseType::Float:
        case VariableBaseType::Double:
        case VariableBaseType::Boolean:
        case VariableBaseType::Enum:
            return true;
        default:
            assert(false && "unknown base type to generate llvm type for");
            return false;
    }
}
bool VariableType::isNumberType() const
{
    switch (this->baseType)
    {
        case VariableBaseType::Integer:
        case VariableBaseType::Float:
        case VariableBaseType::Double:
            return true;
        default:
            return false;
    }
}

llvm::Type *VariableType::generateLlvmType(std::unique_ptr<Context> &context)
{
    switch (this->baseType)
    {
        case VariableBaseType::Pointer:
            return llvm::PointerType::getUnqual(*context->context());
        case VariableBaseType::Float:
            return llvm::Type::getFloatTy(*context->context());
        case VariableBaseType::Double:
            return llvm::Type::getDoubleTy(*context->context());
        case VariableBaseType::Boolean:
            return llvm::Type::getInt1Ty(*context->context());
        case VariableBaseType::Character:
            return llvm::Type::getInt8Ty(*context->context());
        default:
            assert(false && "unknown base type to generate llvm type for");
            return nullptr;
    }
}

std::shared_ptr<IntegerType> VariableType::getInteger(const size_t length)
{
    auto integer = std::make_shared<IntegerType>();
    integer->baseType = VariableBaseType::Integer;
    integer->length = length;
    integer->typeName = "integer" + std::to_string(length);
    return integer;
}
std::shared_ptr<VariableType> VariableType::getCharacter()
{
    auto charType = std::make_shared<VariableType>();
    charType->baseType = VariableBaseType::Character;
    charType->typeName = "char";
    return charType;
}
std::shared_ptr<VariableType> VariableType::getSingle()
{
    auto floatType = std::make_shared<VariableType>();
    floatType->baseType = VariableBaseType::Float;
    floatType->typeName = "single";
    return floatType;
}
std::shared_ptr<VariableType> VariableType::getDouble()
{
    auto doubleType = std::make_shared<VariableType>();
    doubleType->baseType = VariableBaseType::Double;
    doubleType->typeName = "double";
    return doubleType;
}

std::shared_ptr<VariableType> VariableType::getBoolean()
{
    auto boolean = std::make_shared<VariableType>();
    boolean->baseType = VariableBaseType::Boolean;
    boolean->typeName = "boolean";
    return boolean;
}

std::shared_ptr<VariableType> VariableType::getPointer()
{
    auto pointer = std::make_shared<VariableType>();
    pointer->baseType = VariableBaseType::Pointer;
    pointer->typeName = "pointer";
    return pointer;
}
bool VariableType::operator==(const VariableType &other) const { return this->baseType == other.baseType; }
llvm::Value *FieldAccessableType::getLowValue(std::unique_ptr<Context> &context)
{
    return context->builder()->getInt64(0);
}


std::shared_ptr<PointerType> PointerType::getPointerTo(const std::shared_ptr<VariableType> &baseType)
{
    auto ptrType = std::make_shared<PointerType>();

    ptrType->pointerBase = baseType;
    ptrType->baseType = VariableBaseType::Pointer;
    ptrType->typeName = baseType->typeName + "_ptr";
    return ptrType;
}
std::shared_ptr<PointerType> PointerType::getUnqual()
{
    auto ptrType = std::make_shared<PointerType>();
    ptrType->pointerBase = nullptr;
    ptrType->baseType = VariableBaseType::Pointer;
    ptrType->typeName = "pointer";
    return ptrType;
}
llvm::Type *PointerType::generateLlvmType(std::unique_ptr<Context> &context)
{
    if (pointerBase)
    {
        const auto llvmBaseType = pointerBase->generateLlvmType(context);
        return llvm::PointerType::getUnqual(llvmBaseType);
    }
    return llvm::PointerType::getUnqual(*context->context());
}
