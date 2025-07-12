#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include "Token.h"

enum class VariableBaseType
{
    Integer,
    Character,
    Float,
    Double,
    String,
    Struct,
    Array,
    Class,
    Boolean,
    Pointer,
    Unknown,
    File,
    Enum
};

namespace llvm
{
    class Type;
    class Value;
    class AllocaInst;
} // namespace llvm

class Context;

class IntegerType;


class VariableType
{
public:
    explicit VariableType(VariableBaseType baseType = VariableBaseType::Unknown, const std::string &typeName = "");
    virtual ~VariableType() = default;
    [[nodiscard]] bool isSimpleType() const;
    [[nodiscard]] bool isNumberType() const;
    VariableBaseType baseType = VariableBaseType::Unknown;
    std::string typeName = "";

    virtual llvm::Type *generateLlvmType(std::unique_ptr<Context> &context);
    static std::shared_ptr<IntegerType> getInteger(size_t length = 32);
    static std::shared_ptr<VariableType> getCharacter();
    static std::shared_ptr<VariableType> getSingle();
    static std::shared_ptr<VariableType> getDouble();
    static std::shared_ptr<VariableType> getBoolean();
    static std::shared_ptr<VariableType> getPointer();

    bool operator==(const VariableType &other) const;
};

class FieldAccessableType
{
public:
    virtual ~FieldAccessableType() = default;
    virtual llvm::Value *generateFieldAccess(Token &token, llvm::Value *indexValue,
                                             std::unique_ptr<Context> &context) = 0;
    virtual llvm::Value *generateLengthValue(const Token &token, std::unique_ptr<Context> &context) = 0;
    virtual llvm::Value *generateHighValue(const Token &token, std::unique_ptr<Context> &context) = 0;
    virtual llvm::Value *getLowValue(std::unique_ptr<Context> &context);
};


class IntegerType final : public VariableType
{
public:
    size_t length;
    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;
};


class PointerType : public VariableType
{
private:
    llvm::Type *llvmType = nullptr;

public:
    std::shared_ptr<VariableType> pointerBase;

    static std::shared_ptr<PointerType> getPointerTo(const std::shared_ptr<VariableType> &baseType);
    static std::shared_ptr<PointerType> getUnqual();


    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;


    bool operator==(const PointerType &other) const { return this->baseType == other.baseType; }
};
