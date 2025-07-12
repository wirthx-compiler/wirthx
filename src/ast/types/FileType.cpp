#include "FileType.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <vector>

#include "compiler/Context.h"

FileType::FileType(const std::string &typeName, std::optional<std::shared_ptr<VariableType>> childType) :
    VariableType(VariableBaseType::File, typeName), m_childType(std::move(childType))
{
}
llvm::Type *FileType::generateLlvmType(std::unique_ptr<Context> &context)
{

    auto cache = llvm::StructType::getTypeByName(*context->context(), typeName);
    if (cache == nullptr)
    {
        std::vector<llvm::Type *> types;
        types.emplace_back(::PointerType::getPointerTo(VariableType::getInteger(8))->generateLlvmType(context));
        types.emplace_back(VariableType::getPointer()->generateLlvmType(context));
        types.emplace_back(VariableType::getBoolean()->generateLlvmType(context));

        const llvm::ArrayRef<llvm::Type *> elements(types);


        return llvm::StructType::create(*context->context(), elements, typeName);
    }
    return cache;
}
std::shared_ptr<VariableType> FileType::getFileType(std::optional<std::shared_ptr<VariableType>> childType)
{
    return std::make_shared<FileType>("file", childType);
}
