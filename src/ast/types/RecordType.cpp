#include "RecordType.h"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/DerivedTypes.h>

#include "compiler/Context.h"

RecordType::RecordType(std::vector<VariableDefinition> fields, const std::string &typeName) :
    m_fields(std::move(fields))
{
    this->typeName = typeName;
    this->baseType = VariableBaseType::Struct;
}

void RecordType::addField(const VariableDefinition &field) { m_fields.push_back(field); }

auto RecordType::getField(const size_t index) -> VariableDefinition { return m_fields.at(index); }

std::optional<VariableDefinition> RecordType::getFieldByName(const std::string &fieldName)
{
    for (auto &field: m_fields)
    {
        if (field.variableName == fieldName)
        {
            return field;
        }
    }
    return std::nullopt;
}

int RecordType::getFieldIndexByName(const std::string &name) const
{
    for (size_t i = 0; i < m_fields.size(); ++i)
    {
        if (m_fields[i].variableName == name)
        {
            return i;
        }
    }
    return 0;
}

llvm::Type *RecordType::generateLlvmType(std::unique_ptr<Context> &context)
{
    auto cached_type = llvm::StructType::getTypeByName(*context->context(), typeName);
    if (cached_type == nullptr)
    {
        std::vector<llvm::Type *> types;
        for (size_t i = 0; i < size(); ++i)
        {
            types.emplace_back(getField(i).variableType->generateLlvmType(context));
        }

        llvm::ArrayRef<llvm::Type *> Elements(types);


        return llvm::StructType::create(*context->context(), Elements, typeName);
    }
    return cached_type;
}

size_t RecordType::size() const { return m_fields.size(); }
