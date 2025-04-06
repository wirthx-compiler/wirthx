//
// Created by stefan on 30.03.25.
//

#include "CaseNode.h"

#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <utility>

#include "compiler/Context.h"


CaseNode::CaseNode(const Token &token, std::shared_ptr<ASTNode> selector, std::vector<Selector> selectors,
                   std::shared_ptr<ASTNode> elseExpression) :
    ASTNode(token), m_selector(std::move(selector)), m_selectors(std::move(selectors)),
    m_elseExpression(std::move(elseExpression))
{
}
void CaseNode::print() {}
llvm::Value *CaseNode::codegen_constants(std::unique_ptr<Context> &context)
{
    const auto value = m_selector->codegen(context);
    auto function = context->Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *defaultBlock = llvm::BasicBlock::Create(*context->TheContext, "default", function);
    llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(*context->TheContext, "caseEnd", function);
    const auto switchInstruction = context->Builder->CreateSwitch(value, defaultBlock, m_selectors.size() + 1);
    for (const auto &selector: m_selectors)
    {

        const auto selectorBlock = llvm::BasicBlock::Create(*context->TheContext, "case", function);
        context->Builder->SetInsertPoint(selectorBlock);
        selector.expression->codegen(context);
        context->Builder->CreateBr(endBlock);
        const auto selectorValue = selector.selector->codegen(context);
        switchInstruction->addCase(llvm::cast<llvm::ConstantInt>(selectorValue), selectorBlock);
    }
    context->Builder->SetInsertPoint(defaultBlock);
    if (m_elseExpression)
    {
        m_elseExpression->codegen(context);
    }
    context->Builder->CreateBr(endBlock);

    context->Builder->SetInsertPoint(endBlock);

    return nullptr;
}
llvm::Value *CaseNode::codegen(std::unique_ptr<Context> &context)
{
    auto selectorType = m_selector->resolveType(context->ProgramUnit, resolveParent(context));
    if (selectorType->baseType == VariableBaseType::Enum || selectorType->baseType == VariableBaseType::Integer)
    {
        return codegen_constants(context);
    }
    assert(false && "variable type not yet supported for the case statement");
    return nullptr;
}
std::optional<std::shared_ptr<ASTNode>> CaseNode::block() { return ASTNode::block(); }
