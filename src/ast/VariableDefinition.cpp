#include "VariableDefinition.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <magic_enum/magic_enum.hpp>
#include "ASTNode.h"
#include "compiler/Context.h"
#include "types/ArrayType.h"
#include "types/FileType.h"
#include "types/RecordType.h"
#include "types/StringType.h"
using namespace std::literals;

llvm::AllocaInst *VariableDefinition::generateCode(std::unique_ptr<Context> &context) const
{
    if (const auto array = std::dynamic_pointer_cast<ArrayType>(this->variableType))
    {
        const auto arrayType = array->generateLlvmType(context);

        auto arrayAllocation = context->builder()->CreateAlloca(arrayType, nullptr, this->variableName);

        if (array->isDynArray)
        {
            const auto arraySizeOffset =
                    context->builder()->CreateStructGEP(arrayType, arrayAllocation, 0, "array.size.offset");
            const auto arrayPointerOffset =
                    context->builder()->CreateStructGEP(arrayType, arrayAllocation, 1, "array.ptr.offset");
            context->builder()->CreateStore(context->builder()->getInt64(0), arraySizeOffset);
            context->builder()->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(
                                                    array->arrayBase->generateLlvmType(context))),
                                            arrayPointerOffset);


            return arrayAllocation;
        }

        auto *gvar_array_a = new llvm::GlobalVariable(*context->module(), arrayType, true,
                                                      llvm::GlobalValue::ExternalLinkage, nullptr, this->variableName);

        // Constant Definitions
        llvm::ConstantAggregateZero *const_array_2 = llvm::ConstantAggregateZero::get(arrayType);

        // Global Variable Definitions
        gvar_array_a->setInitializer(const_array_2);
        {
            auto memcpyCall = llvm::Intrinsic::getDeclaration(
                    context->module().get(), llvm::Intrinsic::memcpy,
                    {context->builder()->getPtrTy(), context->builder()->getPtrTy(), context->builder()->getInt64Ty()});
            std::vector<llvm::Value *> memcopyArgs;

            const llvm::DataLayout &DL = context->module()->getDataLayout();
            uint64_t structSize = DL.getTypeAllocSize(arrayType);


            memcopyArgs.push_back(context->builder()->CreateBitCast(arrayAllocation, context->builder()->getPtrTy()));
            memcopyArgs.push_back(context->builder()->CreateBitCast(gvar_array_a, context->builder()->getPtrTy()));
            memcopyArgs.push_back(context->builder()->getInt64(structSize));
            memcopyArgs.push_back(context->builder()->getFalse());

            context->builder()->CreateCall(memcpyCall, memcopyArgs);
        }
        // context->builder()->CreateStore(gvar_array_a, arrayAllocation);
        return arrayAllocation;
    }


    switch (this->variableType->baseType)
    {
        case VariableBaseType::Integer:
        {
            auto type = this->variableType->generateLlvmType(context);
            auto allocation = context->builder()->CreateAlloca(type, nullptr, this->variableName);
            // if (!this->value)
            context->builder()->CreateStore(
                    context->builder()->getIntN(allocation->getAllocatedType()->getIntegerBitWidth(), 0), allocation);
            return allocation;
        }
        case VariableBaseType::Character:
        {
            auto type = this->variableType->generateLlvmType(context);
            auto allocation = context->builder()->CreateAlloca(type, nullptr, this->variableName);
            context->builder()->CreateStore(
                    context->builder()->getIntN(allocation->getAllocatedType()->getIntegerBitWidth(), 0), allocation);
            return allocation;
        }
        case VariableBaseType::Boolean:
        {
            auto allocation =
                    context->builder()->CreateAlloca(context->builder()->getInt1Ty(), nullptr, this->variableName);
            context->builder()->CreateStore(context->builder()->getFalse(), allocation);
            return allocation;
        }
        case VariableBaseType::Float:
            return context->builder()->CreateAlloca(llvm::Type::getFloatTy(*context->context()), nullptr,
                                                    this->variableName);
        case VariableBaseType::Double:
            return context->builder()->CreateAlloca(llvm::Type::getDoubleTy(*context->context()), nullptr,
                                                    this->variableName);
        case VariableBaseType::Struct:
        {
            const auto structType = std::dynamic_pointer_cast<RecordType>(this->variableType);
            if (structType != nullptr)
            {
                return context->builder()->CreateAlloca(structType->generateLlvmType(context), nullptr,
                                                        this->variableName);
            }
        }
        case VariableBaseType::String:
        {
            const auto stringType = std::dynamic_pointer_cast<StringType>(this->variableType);
            if (stringType != nullptr)
            {
                const auto llvmType = stringType->generateLlvmType(context);
                const auto allocated = context->builder()->CreateAlloca(llvmType, nullptr, this->variableName);

                const auto arraySizeOffset =
                        context->builder()->CreateStructGEP(llvmType, allocated, 1, "string.size.offset");
                const auto arrayPointerOffset =
                        context->builder()->CreateStructGEP(llvmType, allocated, 2, "string.ptr.offset");
                context->builder()->CreateStore(context->builder()->getInt64(0), arraySizeOffset);
                context->builder()->CreateStore(
                        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context->context())),
                        arrayPointerOffset);
                return allocated;
            }
        }
        case VariableBaseType::Pointer:
        {
            const auto type = std::dynamic_pointer_cast<PointerType>(this->variableType);

            return context->builder()->CreateAlloca(type->generateLlvmType(context), nullptr, this->variableName);
        }
        case VariableBaseType::File:
        {
            const auto fileType = std::dynamic_pointer_cast<FileType>(this->variableType);
            if (fileType != nullptr)
            {
                auto llvmFileType = fileType->generateLlvmType(context);
                auto allocatedFile = context->builder()->CreateAlloca(llvmFileType, nullptr, this->variableName);
                if (llvmValue)
                {
                    auto filePtr = context->builder()->CreateStructGEP(llvmFileType, allocatedFile, 1, "file.ptr");

                    context->builder()->CreateStore(llvmValue, filePtr);
                }
                return allocatedFile;
            }
        }
        case VariableBaseType::Enum:
        {
            auto type = this->variableType->generateLlvmType(context);
            auto allocation = context->builder()->CreateAlloca(type, nullptr, this->variableName);
            // if (!this->value)
            context->builder()->CreateStore(
                    context->builder()->getIntN(allocation->getAllocatedType()->getIntegerBitWidth(), 0), allocation);
            return allocation;
        }
        default:
            assert(false && "unsupported variable base type to generate variable definition");
            return nullptr;
    }
}


llvm::Value *VariableDefinition::generateCodeForConstant(std::unique_ptr<Context> &context) const
{
    if (llvm::Function *currentFunction = context->currentFunction())
        llvm::IRBuilder<> TmpB(&currentFunction->getEntryBlock(), currentFunction->getEntryBlock().begin());

    return this->value->codegenForTargetType(context, this->variableType);
}
