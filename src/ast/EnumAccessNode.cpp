//
// Created by stefan on 23.03.25.
//

#include "EnumAccessNode.h"

#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
EnumAccessNode::EnumAccessNode(const Token &token, std::shared_ptr<EnumType> enumType) :
    ASTNode(token), m_enumType(std::move(enumType))
{
}
void EnumAccessNode::print() {}
llvm::Value *EnumAccessNode::codegen(std::unique_ptr<Context> &context)
{
    const auto enumName = expressionToken().lexical();
    const auto valueType = context->Builder->getInt32Ty();

    llvm::GlobalVariable *globalVar = context->TheModule->getGlobalVariable(enumName);
    if (!globalVar)
    {
        llvm::Constant *constValue = llvm::ConstantInt::get(valueType, m_enumType->getValue(enumName));


        globalVar = new llvm::GlobalVariable(*context->TheModule, valueType, true, llvm::GlobalValue::ExternalLinkage,
                                             constValue, enumName);
    }

    return context->Builder->CreateLoad(valueType, globalVar, enumName);
}
std::shared_ptr<VariableType> EnumAccessNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return m_enumType;
}
