#pragma once

#include <set>
#include <unordered_map>
#include "ASTNode.h"
#include "ast/BlockNode.h"
#include "ast/FunctionDefinitionNode.h"
#include "types/TypeRegistry.h"

enum class UnitType
{
    UNIT,
    PROGRAM
};

class UnitNode : public ASTNode
{
private:
    UnitType m_unitType;
    std::string m_unitName;
    std::vector<std::shared_ptr<FunctionDefinitionNode>> m_functionDefinitions;
    TypeRegistry m_typeDefinitions;
    std::shared_ptr<BlockNode> m_blockNode;
    std::vector<std::string> m_argumentNames;

public:
    UnitNode(const Token &token, UnitType unitType, const std::string &unitName,
             const std::vector<std::shared_ptr<FunctionDefinitionNode>> &functionDefinitions,
             const TypeRegistry &typeDefinitions, const std::shared_ptr<BlockNode> &blockNode);
    UnitNode(const Token &token, UnitType unitType, const std::string &unitName,
             const std::vector<std::string> &argumentNames,
             const std::vector<std::shared_ptr<FunctionDefinitionNode>> &functionDefinitions,
             const TypeRegistry &typeDefinitions, const std::shared_ptr<BlockNode> &blockNode);
    ~UnitNode() override = default;

    void print() override;

    std::vector<std::shared_ptr<FunctionDefinitionNode>> getFunctionDefinitions();
    std::optional<std::shared_ptr<FunctionDefinitionNode>> getFunctionDefinition(const std::string &functionName);
    std::optional<std::shared_ptr<FunctionDefinitionNode>> getFunctionDefinitionByName(const std::string &functionName);
    void addFunctionDefinition(const std::shared_ptr<FunctionDefinitionNode> &functionDefinition);
    std::string getUnitName();
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::optional<VariableDefinition> getVariableDefinition(const std::string &name) const;
    std::set<std::string> collectLibsToLink();
    TypeRegistry getTypeDefinitions();

    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
    std::optional<std::pair<const ASTNode *, std::shared_ptr<ASTNode>>> getNodeByToken(const Token &token) const;
};
