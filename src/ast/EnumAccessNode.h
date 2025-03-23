//
// Created by stefan on 23.03.25.
//

#ifndef ENUMACCESSNODE_H
#define ENUMACCESSNODE_H
#include "ASTNode.h"
#include "types/EnumType.h"


class EnumAccessNode : public ASTNode
{
private:
    std::shared_ptr<EnumType> m_enumType;

public:
    explicit EnumAccessNode(const Token &token, std::shared_ptr<EnumType> enumType);
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
};


#endif // ENUMACCESSNODE_H
