#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include "Token.h"

enum class VariableBaseType
{
    Integer,
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

struct Context;

class IntegerType;


class VariableType
{
public:
    explicit VariableType(VariableBaseType baseType = VariableBaseType::Unknown, const std::string &typeName = "");
    virtual ~VariableType() = default;
    [[nodiscard]] bool isSimpleType() const;
    VariableBaseType baseType = VariableBaseType::Unknown;
    std::string typeName = "";

    virtual llvm::Type *generateLlvmType(std::unique_ptr<Context> &context);
    static std::shared_ptr<IntegerType> getInteger(size_t length = 32);
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

class ArrayType final : public VariableType, public FieldAccessableType
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
