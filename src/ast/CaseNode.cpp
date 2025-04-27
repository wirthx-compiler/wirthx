#include "CaseNode.h"

#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <utility>

#include "compiler/Context.h"
#include "compiler/codegen.h"
#include "exceptions/CompilerException.h"
#include "types/ValueRangeType.h"


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
    for (const auto &[selectorNode, expression]: m_selectors)
    {

        const auto selectorBlock = llvm::BasicBlock::Create(*context->TheContext, "case", function);
        context->Builder->SetInsertPoint(selectorBlock);
        expression->codegen(context);
        context->Builder->CreateBr(endBlock);
        const auto selectorValue = selectorNode->codegen(context);
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
llvm::Value *CaseNode::codegen_strings(std::unique_ptr<Context> &context)
{
    const auto function = context->Builder->GetInsertBlock()->getParent();

    const auto value = m_selector->codegen(context);
    const auto compareFunction = context->TheModule->getFunction("comparestr(string,string)");
    const auto defaultBlock = llvm::BasicBlock::Create(*context->TheContext, "caseDefault", function);
    llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(*context->TheContext, "caseEnd", function);


    llvm::BasicBlock *nextCaseBlock = llvm::BasicBlock::Create(*context->TheContext, "caseFalse", function);
    size_t caseIndex = 0;
    for (auto &[selectorNode, expression]: m_selectors)
    {
        const auto selectorValue = selectorNode->codegen(context);

        std::vector arguments = {value, selectorValue};

        const auto lhs = context->Builder->CreateCall(compareFunction, arguments);
        const auto rhs = context->Builder->getInt32(0);
        const auto condition = context->Builder->CreateICmpEQ(lhs, rhs);
        const auto selectorTrueBlock = llvm::BasicBlock::Create(*context->TheContext, "caseTrue", function);
        if (caseIndex == m_selectors.size() - 1)
        {
            nextCaseBlock = defaultBlock;
        }
        context->Builder->CreateCondBr(condition, selectorTrueBlock, nextCaseBlock);
        context->Builder->SetInsertPoint(selectorTrueBlock);
        expression->codegen(context);
        context->Builder->CreateBr(endBlock);
        if (caseIndex < m_selectors.size() - 1)
        {
            context->Builder->SetInsertPoint(nextCaseBlock);
            nextCaseBlock = llvm::BasicBlock::Create(*context->TheContext, "caseFalse", function);
        }
        caseIndex++;
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
llvm::Value *CaseNode::compareSelectorAndValue(llvm::Value *value, const std::shared_ptr<ASTNode> &selector,
                                               std::unique_ptr<Context> &context)
{

    const auto selectorType = selector->resolveType(context->ProgramUnit, resolveParent(context));

    if (const auto range = std::dynamic_pointer_cast<ValueRangeType>(selectorType))
    {

        return context->Builder->CreateAnd(
                context->Builder->CreateICmpSGE(value, context->Builder->getInt32(range->lowerBounds())),
                context->Builder->CreateICmpSLE(value, context->Builder->getInt32(range->upperBounds()))

        );
    }

    const auto selectorValue = selector->codegen(context);
    return context->Builder->CreateICmpEQ(value, selectorValue);
}
llvm::Value *CaseNode::codegen_ranges(std::unique_ptr<Context> &context)
{
    const auto function = context->Builder->GetInsertBlock()->getParent();

    const auto value = m_selector->codegen(context);
    const auto defaultBlock = llvm::BasicBlock::Create(*context->TheContext, "caseDefault", function);
    llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(*context->TheContext, "caseEnd", function);


    llvm::BasicBlock *nextCaseBlock = llvm::BasicBlock::Create(*context->TheContext, "caseFalse", function);
    size_t caseIndex = 0;
    for (auto &[selectorNode, expression]: m_selectors)
    {


        const auto lhs = compareSelectorAndValue(value, selectorNode, context);
        const auto rhs = context->Builder->getTrue();
        const auto condition = context->Builder->CreateICmpEQ(lhs, rhs);
        const auto selectorTrueBlock = llvm::BasicBlock::Create(*context->TheContext, "caseTrue", function);
        if (caseIndex == m_selectors.size() - 1)
        {
            nextCaseBlock = defaultBlock;
        }
        context->Builder->CreateCondBr(condition, selectorTrueBlock, nextCaseBlock);
        context->Builder->SetInsertPoint(selectorTrueBlock);
        expression->codegen(context);
        context->Builder->CreateBr(endBlock);
        if (caseIndex < m_selectors.size() - 1)
        {
            context->Builder->SetInsertPoint(nextCaseBlock);
            nextCaseBlock = llvm::BasicBlock::Create(*context->TheContext, "caseFalse", function);
        }
        caseIndex++;
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
bool CaseNode::oneSelectorHasARangeType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return std::ranges::any_of(m_selectors,
                               [&unit, &parentNode](const Selector &selector)
                               {
                                   return std::dynamic_pointer_cast<ValueRangeType>(
                                                  selector.selector->resolveType(unit, parentNode)) != nullptr;
                               });
}
llvm::Value *CaseNode::codegen(std::unique_ptr<Context> &context)
{
    const auto selectorType = m_selector->resolveType(context->ProgramUnit, resolveParent(context));
    if (oneSelectorHasARangeType(context->ProgramUnit, resolveParent(context)))
    {
        return codegen_ranges(context);
    }
    if (selectorType->baseType == VariableBaseType::Enum || selectorType->baseType == VariableBaseType::Integer)
    {
        return codegen_constants(context);
    }
    if (selectorType->baseType == VariableBaseType::String)
    {
        return codegen_strings(context);
    }
    assert(false && "variable type not yet supported for the case statement");
    return nullptr;
}
std::optional<std::shared_ptr<ASTNode>> CaseNode::block() { return ASTNode::block(); }
void CaseNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    const auto selectorType = m_selector->resolveType(unit, parentNode);

    for (const auto &[selector, expression]: m_selectors)
    {
        if (const auto selectorType2 = selector->resolveType(unit, parentNode); *selectorType != *selectorType2)
        {
            throw CompilerException(
                    ParserError{.token = selector->expressionToken(),
                                .message = "The type of the expression in the case statement is not the same."});
        }
        expression->typeCheck(unit, parentNode);
    }
    if (m_elseExpression)
    {
        m_elseExpression->typeCheck(unit, parentNode);
    }
}
