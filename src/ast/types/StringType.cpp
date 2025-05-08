#include "StringType.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
#include "exceptions/CompilerException.h"


llvm::Type *StringType::generateLlvmType(std::unique_ptr<Context> &context)
{
    if (llvmType != nullptr && llvmType->getContext().pImpl != context->TheContext->pImpl)
    {
        llvmType = nullptr;
    }

    if (llvmType == nullptr)
    {
        const auto baseType = IntegerType::getInteger(8);
        const auto charType = baseType->generateLlvmType(context);
        std::vector<llvm::Type *> types;
        types.emplace_back(VariableType::getInteger(64)->generateLlvmType(context));
        types.emplace_back(VariableType::getInteger(64)->generateLlvmType(context));
        types.emplace_back(llvm::PointerType::getUnqual(charType));


        llvm::ArrayRef<llvm::Type *> Elements(types);


        llvmType = llvm::StructType::create(Elements, "string");
    }
    return llvmType;
}

llvm::Value *StringType::generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context)
{
    const auto arrayName = token.lexical();
    llvm::Value *V = context->NamedAllocations[arrayName];

    if (!V)
    {
        for (auto &arg: context->TopLevelFunction->args())
        {
            if (arg.getName() == arrayName)
            {
                V = context->TopLevelFunction->getArg(arg.getArgNo());
                break;
            }
        }
    }

    if (!V)
        return LogErrorV("Unknown variable for string access: " + arrayName);


    const auto llvmRecordType = this->generateLlvmType(context);
    const auto arrayBaseType = IntegerType::getInteger(8)->generateLlvmType(context);

    const auto arrayPointerOffset = context->Builder->CreateStructGEP(llvmRecordType, V, 2, "string.ptr.offset");

    const auto loadResult =
            context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), arrayPointerOffset);


    const auto bounds =
            context->Builder->CreateGEP(arrayBaseType, loadResult, llvm::ArrayRef<llvm::Value *>{indexValue}, "", true);

    return context->Builder->CreateLoad(arrayBaseType, bounds);
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
    llvm::Value *value = context->NamedAllocations[arrayName];

    if (!value)
    {
        for (auto &arg: context->TopLevelFunction->args())
        {
            if (arg.getName() == arrayName)
            {
                value = context->TopLevelFunction->getArg(arg.getArgNo());
                break;
            }
        }
    }

    if (!value)
        return LogErrorV("Unknown variable for string access: " + arrayName);


    const auto llvmRecordType = generateLlvmType(context);

    const auto arraySizeOffset = context->Builder->CreateStructGEP(llvmRecordType, value, 1, "length");
    const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);

    return context->Builder->CreateSub(context->Builder->CreateLoad(indexType, arraySizeOffset, "loaded.length"),
                                       context->Builder->getInt64(1));
}
llvm::Value *StringType::generateHighValue(const Token &token, std::unique_ptr<Context> &context)
{
    return context->Builder->CreateSub(generateLengthValue(token, context), context->Builder->getInt64(1));
}
llvm::Value *StringType::generateLowerBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return context->Builder->getInt64(0);
}
llvm::Value *StringType::generateUpperBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return generateHighValue(token, context);
}
