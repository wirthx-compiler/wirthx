#pragma once

#include <optional>
#include "VariableType.h"

class FileType : public VariableType
{
private:
    std::optional<std::shared_ptr<VariableType>> m_childType;

public:
    explicit FileType(const std::string &typeName,
                      std::optional<std::shared_ptr<VariableType>> childType = std::nullopt);
    llvm::Type *generateLlvmType(std::unique_ptr<Context> &context) override;

    static std::shared_ptr<VariableType>
    getFileType(std::optional<std::shared_ptr<VariableType>> childType = std::nullopt);
};
