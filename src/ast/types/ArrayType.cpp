//
// Created by stefan on 02.05.25.
//

#include "ArrayType.h"

#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
#include "exceptions/CompilerException.h"

std::shared_ptr<ArrayType> ArrayType::getFixedArray(size_t low, size_t heigh,
                                                    const std::shared_ptr<VariableType> &baseType)
{
    auto type = std::make_shared<ArrayType>();
    type->baseType = VariableBaseType::Array;
    type->low = low;
    type->high = heigh;
    type->arrayBase = baseType;
    type->isDynArray = false;
    return type;
}

std::shared_ptr<ArrayType> ArrayType::getDynArray(const std::shared_ptr<VariableType> &baseType)
{
    auto type = std::make_shared<ArrayType>();
    type->baseType = VariableBaseType::Array;
    type->low = 0;
    type->high = 0;
    type->arrayBase = baseType;
    type->isDynArray = true;
    return type;
}


llvm::Type *IntegerType::generateLlvmType(std::unique_ptr<Context> &context)
{
    return llvm::IntegerType::get(*context->TheContext, this->length);
}


llvm::Type *ArrayType::generateLlvmType(std::unique_ptr<Context> &context)
{
    if (llvmType == nullptr)
    {
        auto arrayBaseType = arrayBase->generateLlvmType(context);
        if (isDynArray)
        {

            std::vector<llvm::Type *> types;
            types.emplace_back(VariableType::getInteger(64)->generateLlvmType(context));

            types.emplace_back(llvm::PointerType::getUnqual(arrayBaseType));


            llvm::ArrayRef<llvm::Type *> Elements(types);


            llvmType = llvm::StructType::create(Elements);
        }
        else
        {

            const auto arraySize = high - low + 1;

            llvmType = llvm::ArrayType::get(arrayBaseType, arraySize);
        }
    }
    return llvmType;
}

llvm::Value *ArrayType::generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context)
{
    const auto arrayName = std::string(token.lexical());
    llvm::Value *arrayAllocation = context->NamedAllocations[arrayName];

    if (!arrayAllocation)
    {
        for (auto &arg: context->TopLevelFunction->args())
        {
            if (arg.getName() == arrayName)
            {
                arrayAllocation = context->TopLevelFunction->getArg(arg.getArgNo());
                break;
            }
        }
    }


    if (!arrayAllocation)
        return LogErrorV("Unknown variable for array access: " + arrayName);

    if (this->isDynArray)
    {
        const auto llvmRecordType = this->generateLlvmType(context);
        const auto arrayBaseType = this->arrayBase->generateLlvmType(context);

        const auto arrayPointerOffset =
                context->Builder->CreateStructGEP(llvmRecordType, arrayAllocation, 1, "array.ptr.offset");
        // const llvm::DataLayout &DL = context->TheModule->getDataLayout();
        // auto alignment = DL.getPrefTypeAlign(ptrType);
        const auto loadResult =
                context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), arrayPointerOffset);


        const auto bounds = context->Builder->CreateGEP(arrayBaseType, loadResult,
                                                        llvm::ArrayRef<llvm::Value *>{indexValue}, "", true);

        return context->Builder->CreateLoad(arrayBaseType, bounds);
    }

    if (llvm::isa<llvm::ConstantInt>(indexValue))
    {

        if (const auto *value = reinterpret_cast<llvm::ConstantInt *>(indexValue);
            value->getSExtValue() < static_cast<int64_t>(this->low) ||
            value->getSExtValue() > static_cast<int64_t>(this->high))
        {
            throw CompilerException(
                    ParserError{.token = token, .message = "the array index is not in the defined range."});
        }
    }
    llvm::Value *index = indexValue;
    if (this->low > 0)
        index = context->Builder->CreateSub(
                index, context->Builder->getIntN(index->getType()->getIntegerBitWidth(), this->low), "array.index.sub");

    const auto arrayType = this->generateLlvmType(context);
    const auto arrayValue = context->Builder->CreateGEP(arrayType, arrayAllocation,
                                                        {context->Builder->getInt64(0), index}, "arrayindex", false);
    return context->Builder->CreateLoad(arrayType->getArrayElementType(), arrayValue);
}
llvm::Value *ArrayType::generateLengthValue(const Token &token, std::unique_ptr<Context> &context)
{
    const auto arrayName = std::string(token.lexical());
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
        return LogErrorV("Unknown variable for array access: " + arrayName);


    if (isDynArray)
    {
        const auto llvmRecordType = generateLlvmType(context);

        const auto arraySizeOffset = context->Builder->CreateStructGEP(llvmRecordType, value, 0, "array.size.offset");
        const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);

        return context->Builder->CreateLoad(indexType, arraySizeOffset);
    }
    return context->Builder->getInt64(high - low);
}
llvm::Value *ArrayType::getLowValue(std::unique_ptr<Context> &context)
{
    if (isDynArray)
    {
        return context->Builder->getInt64(0);
    }
    return context->Builder->getInt64(low);
}
llvm::Value *ArrayType::generateHighValue(const Token &token, std::unique_ptr<Context> &context)
{
    if (isDynArray)
    {
        return context->Builder->CreateSub(generateLengthValue(token, context), context->Builder->getInt64(1));
    }
    return context->Builder->getInt64(high);
}
llvm::Value *ArrayType::generateLowerBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return getLowValue(context);
}
llvm::Value *ArrayType::generateUpperBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return generateHighValue(token, context);
}
