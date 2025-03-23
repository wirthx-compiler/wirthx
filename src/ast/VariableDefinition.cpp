#include "VariableDefinition.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <magic_enum/magic_enum.hpp>
#include "ASTNode.h"
#include "compiler/Context.h"
#include "types/FileType.h"
#include "types/RecordType.h"
#include "types/StringType.h"
using namespace std::literals;

llvm::AllocaInst *VariableDefinition::generateCode(std::unique_ptr<Context> &context) const
{
    if (const auto array = std::dynamic_pointer_cast<ArrayType>(this->variableType))
    {
        const auto arrayType = array->generateLlvmType(context);

        auto arrayAllocation = context->Builder->CreateAlloca(arrayType, nullptr, this->variableName);


        if (array->isDynArray)
        {
            return arrayAllocation;
        }

        auto *gvar_array_a = new llvm::GlobalVariable(*context->TheModule, arrayType, true,
                                                      llvm::GlobalValue::ExternalLinkage, nullptr, this->variableName);

        // Constant Definitions
        llvm::ConstantAggregateZero *const_array_2 = llvm::ConstantAggregateZero::get(arrayType);

        // Global Variable Definitions
        gvar_array_a->setInitializer(const_array_2);
        {
            auto memcpyCall = llvm::Intrinsic::getDeclaration(
                    context->TheModule.get(), llvm::Intrinsic::memcpy,
                    {context->Builder->getPtrTy(), context->Builder->getPtrTy(), context->Builder->getInt64Ty()});
            std::vector<llvm::Value *> memcopyArgs;

            const llvm::DataLayout &DL = context->TheModule->getDataLayout();
            uint64_t structSize = DL.getTypeAllocSize(arrayType);


            memcopyArgs.push_back(context->Builder->CreateBitCast(arrayAllocation, context->Builder->getPtrTy()));
            memcopyArgs.push_back(context->Builder->CreateBitCast(gvar_array_a, context->Builder->getPtrTy()));
            memcopyArgs.push_back(context->Builder->getInt64(structSize));
            memcopyArgs.push_back(context->Builder->getFalse());

            context->Builder->CreateCall(memcpyCall, memcopyArgs);
        }
        // context->Builder->CreateStore(gvar_array_a, arrayAllocation);
        return arrayAllocation;
    }


    switch (this->variableType->baseType)
    {
        case VariableBaseType::Integer:
        {
            auto type = this->variableType->generateLlvmType(context);
            auto allocation = context->Builder->CreateAlloca(type, nullptr, this->variableName);
            // if (!this->value)
            context->Builder->CreateStore(
                    context->Builder->getIntN(allocation->getAllocatedType()->getIntegerBitWidth(), 0), allocation);
            return allocation;
        }
        case VariableBaseType::Boolean:
        {
            auto allocation =
                    context->Builder->CreateAlloca(context->Builder->getInt1Ty(), nullptr, this->variableName);
            context->Builder->CreateStore(context->Builder->getFalse(), allocation);
            return allocation;
        }
        case VariableBaseType::Float:
            return context->Builder->CreateAlloca(llvm::Type::getFloatTy(*context->TheContext), nullptr,
                                                  this->variableName);
        case VariableBaseType::Double:
            return context->Builder->CreateAlloca(llvm::Type::getDoubleTy(*context->TheContext), nullptr,
                                                  this->variableName);
        case VariableBaseType::Struct:
        {
            const auto structType = std::dynamic_pointer_cast<RecordType>(this->variableType);
            if (structType != nullptr)
            {
                return context->Builder->CreateAlloca(structType->generateLlvmType(context), nullptr,
                                                      this->variableName);
            }
        }
        case VariableBaseType::String:
        {
            const auto stringType = std::dynamic_pointer_cast<StringType>(this->variableType);
            if (stringType != nullptr)
            {
                return context->Builder->CreateAlloca(stringType->generateLlvmType(context), nullptr,
                                                      this->variableName);
            }
        }
        case VariableBaseType::Pointer:
        {
            const auto type = std::dynamic_pointer_cast<PointerType>(this->variableType);
            if (type->pointerBase)
                return context->Builder->CreateAlloca(type->pointerBase->generateLlvmType(context), nullptr,
                                                      this->variableName);
            return context->Builder->CreateAlloca(type->generateLlvmType(context), nullptr, this->variableName);
        }
        case VariableBaseType::File:
        {
            const auto fileType = std::dynamic_pointer_cast<FileType>(this->variableType);
            if (fileType != nullptr)
            {
                auto llvmFileType = fileType->generateLlvmType(context);
                auto allocatedFile = context->Builder->CreateAlloca(llvmFileType, nullptr, this->variableName);
                if (llvmValue)
                {
                    auto filePtr = context->Builder->CreateStructGEP(llvmFileType, allocatedFile, 1, "file.ptr");

                    context->Builder->CreateStore(llvmValue, filePtr);
                }
                return allocatedFile;
            }
        }
        case VariableBaseType::Enum:
        {
            auto type = this->variableType->generateLlvmType(context);
            auto allocation = context->Builder->CreateAlloca(type, nullptr, this->variableName);
            // if (!this->value)
            context->Builder->CreateStore(
                    context->Builder->getIntN(allocation->getAllocatedType()->getIntegerBitWidth(), 0), allocation);
            return allocation;
        }
        default:
            assert(false && "unsupported variable base type to generate variable definition");
            return nullptr;
    }
}


llvm::Value *VariableDefinition::generateCodeForConstant(std::unique_ptr<Context> &context) const
{
    llvm::Function *TheFunction = context->TopLevelFunction;
    if (TheFunction)
        llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());

    // auto array = std::dynamic_pointer_cast<ArrayType>(this->variableType);

    return this->value->codegen(context);
}
