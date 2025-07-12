#include "StringType.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
#include "exceptions/CompilerException.h"


llvm::Type *StringType::generateLlvmType(std::unique_ptr<Context> &context)
{

    const auto llvmType = llvm::StructType::getTypeByName(*context->context(), "string");


    if (llvmType == nullptr)
    {
        const auto baseType = IntegerType::getInteger(8);
        const auto charType = baseType->generateLlvmType(context);
        std::vector<llvm::Type *> types;
        types.emplace_back(VariableType::getInteger(64)->generateLlvmType(context));
        types.emplace_back(VariableType::getInteger(64)->generateLlvmType(context));
        types.emplace_back(llvm::PointerType::getUnqual(charType));


        llvm::ArrayRef<llvm::Type *> Elements(types);


        return llvm::StructType::create(*context->context(), Elements, "string");
    }
    return llvmType;
}

llvm::Value *StringType::generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context)
{
    const auto arrayName = token.lexical();
    llvm::Value *V = context->namedAllocation(arrayName);

    if (!V)
    {
        for (auto &arg: context->currentFunction()->args())
        {
            if (arg.getName() == arrayName)
            {
                V = context->currentFunction()->getArg(arg.getArgNo());
                break;
            }
        }
    }

    if (!V)
        return LogErrorV("Unknown variable for string access: " + arrayName);


    const auto llvmRecordType = this->generateLlvmType(context);
    const auto arrayBaseType = IntegerType::getInteger(8)->generateLlvmType(context);

    const auto arrayPointerOffset = context->builder()->CreateStructGEP(llvmRecordType, V, 2, "string.ptr.offset");

    const auto loadResult =
            context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), arrayPointerOffset);


    const auto bounds = context->builder()->CreateGEP(arrayBaseType, loadResult,
                                                      llvm::ArrayRef<llvm::Value *>{indexValue}, "", true);

    return context->builder()->CreateLoad(arrayBaseType, bounds);
}

std::shared_ptr<StringType> StringType::getString()
{
    static auto stringType = std::make_shared<StringType>();
    stringType->baseType = VariableBaseType::String;
    stringType->typeName = "string";
    return stringType;
}
llvm::Value *StringType::generateLengthValue(const Token &token, std::unique_ptr<Context> &context)
{
    const auto arrayName = token.lexical();
    llvm::Value *value = context->namedAllocation(arrayName);

    if (!value)
    {
        for (auto &arg: context->currentFunction()->args())
        {
            if (arg.getName() == arrayName)
            {
                value = context->currentFunction()->getArg(arg.getArgNo());
                break;
            }
        }
    }

    if (!value)
        return LogErrorV("Unknown variable for string access: " + arrayName);


    const auto llvmRecordType = generateLlvmType(context);

    const auto arraySizeOffset = context->builder()->CreateStructGEP(llvmRecordType, value, 1, "length");
    const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);

    return context->builder()->CreateSub(context->builder()->CreateLoad(indexType, arraySizeOffset, "loaded.length"),
                                         context->builder()->getInt64(1));
}
llvm::Value *StringType::generateHighValue(const Token &token, std::unique_ptr<Context> &context)
{
    return context->builder()->CreateSub(generateLengthValue(token, context), context->builder()->getInt64(1));
}
llvm::Value *StringType::generateLowerBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return context->builder()->getInt64(0);
}
llvm::Value *StringType::generateUpperBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return generateHighValue(token, context);
}
