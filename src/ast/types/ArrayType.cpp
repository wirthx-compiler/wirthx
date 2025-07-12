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
    type->typeName = "array_" + baseType->typeName + "_" + std::to_string(low) + "_" + std::to_string(heigh);
    type->baseType = VariableBaseType::Array;
    type->low = low;
    type->high = heigh;
    type->arrayBase = baseType;
    type->isDynArray = false;
    type->llvmType = nullptr;
    return type;
}

std::shared_ptr<ArrayType> ArrayType::getDynArray(const std::shared_ptr<VariableType> &baseType)
{
    auto type = std::make_shared<ArrayType>();
    type->baseType = VariableBaseType::Array;
    type->typeName = "dynarray_" + baseType->typeName;
    type->low = 0;
    type->high = 0;
    type->arrayBase = baseType;
    type->isDynArray = true;
    type->llvmType = nullptr;
    return type;
}


llvm::Type *IntegerType::generateLlvmType(std::unique_ptr<Context> &context)
{
    return llvm::IntegerType::get(*context->context(), this->length);
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


            llvmType = llvm::StructType::create(*context->context(), Elements, arrayBase->typeName);
        }
        else
        {

            const auto arraySize = high - low + 1;

            return llvm::ArrayType::get(arrayBaseType, arraySize);
        }
    }
    return llvmType;
}

llvm::Value *ArrayType::generateFieldAccess(Token &token, llvm::Value *indexValue, std::unique_ptr<Context> &context)
{
    const auto arrayName = std::string(token.lexical());
    llvm::Value *arrayAllocation = context->namedAllocation(arrayName);

    if (!arrayAllocation)
    {
        for (auto &arg: context->currentFunction()->args())
        {
            if (arg.getName() == arrayName)
            {
                arrayAllocation = context->currentFunction()->getArg(arg.getArgNo());
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
                context->builder()->CreateStructGEP(llvmRecordType, arrayAllocation, 1, "array.ptr.offset");
        // const llvm::DataLayout &DL = context->module()->getDataLayout();
        // auto alignment = DL.getPrefTypeAlign(ptrType);
        const auto loadResult =
                context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), arrayPointerOffset);


        const auto bounds = context->builder()->CreateGEP(arrayBaseType, loadResult,
                                                          llvm::ArrayRef<llvm::Value *>{indexValue}, "", true);

        return context->builder()->CreateLoad(arrayBaseType, bounds);
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
        index = context->builder()->CreateSub(
                index, context->builder()->getIntN(index->getType()->getIntegerBitWidth(), this->low),
                "array.index.sub");

    const auto arrayType = this->generateLlvmType(context);
    const auto arrayValue = context->builder()->CreateGEP(
            arrayType, arrayAllocation, {context->builder()->getInt64(0), index}, "arrayindex", false);
    return context->builder()->CreateLoad(arrayType->getArrayElementType(), arrayValue);
}
llvm::Value *ArrayType::generateLengthValue(const Token &token, std::unique_ptr<Context> &context)
{
    const auto arrayName = std::string(token.lexical());
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
        return LogErrorV("Unknown variable for array access: " + arrayName);


    if (isDynArray)
    {
        const auto llvmRecordType = generateLlvmType(context);

        const auto arraySizeOffset = context->builder()->CreateStructGEP(llvmRecordType, value, 0, "array.size.offset");
        const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);

        return context->builder()->CreateLoad(indexType, arraySizeOffset);
    }
    return context->builder()->getInt64(high - low);
}
llvm::Value *ArrayType::getLowValue(std::unique_ptr<Context> &context)
{
    if (isDynArray)
    {
        return context->builder()->getInt64(0);
    }
    return context->builder()->getInt64(low);
}
llvm::Value *ArrayType::generateHighValue(const Token &token, std::unique_ptr<Context> &context)
{
    if (isDynArray)
    {
        return context->builder()->CreateSub(generateLengthValue(token, context), context->builder()->getInt64(1));
    }
    return context->builder()->getInt64(high);
}
llvm::Value *ArrayType::generateLowerBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return getLowValue(context);
}
llvm::Value *ArrayType::generateUpperBounds(const Token &token, std::unique_ptr<Context> &context)
{
    return generateHighValue(token, context);
}
