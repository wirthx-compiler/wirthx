#pragma once
#include "../VariableDefinition.h"
#include "VariableType.h"

#include <cstddef>
#include <optional>
#include <vector>

class RecordType : public VariableType
{
private:
    std::vector<VariableDefinition> m_fields;

public:
    VariableDefinition getField(size_t index);
    [[nodiscard]] size_t size() const;
    void addField(const VariableDefinition &field);
    std::optional<VariableDefinition> getFieldByName(const std::string &fieldName);
    [[nodiscard]] int getFieldIndexByName(const std::string &name) const;

    RecordType(std::vector<VariableDefinition> fields, const std::string &typeName);
    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;
};
