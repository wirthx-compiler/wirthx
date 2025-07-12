#include "CaseNode.h"

#include <cassert>
#include <llvm/IR/IRBuilder.h>
#include <utility>

#include "compiler/Context.h"
#include "exceptions/CompilerException.h"
#include "types/ValueRangeType.h"


CaseNode::CaseNode(const Token &token, std::shared_ptr<ASTNode> selector, std::vector<Selector> selectors,
                   std::vector<std::shared_ptr<ASTNode>> elseExpressions) :
    ASTNode(token), m_selector(std::move(selector)), m_selectors(std::move(selectors)),
    m_elseExpressions(std::move(elseExpressions))
{
}
void CaseNode::print() {}
llvm::Value *CaseNode::codegen_constants(std::unique_ptr<Context> &context)
{
    const auto value = m_selector->codegen(context);
    auto function = context->builder()->GetInsertBlock()->getParent();
    llvm::BasicBlock *defaultBlock = llvm::BasicBlock::Create(*context->context(), "default", function);
    llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(*context->context(), "caseEnd", function);
    const auto switchInstruction = context->builder()->CreateSwitch(value, defaultBlock, m_selectors.size() + 1);
    for (const auto &[selectorNode, expression]: m_selectors)
    {

        const auto selectorBlock = llvm::BasicBlock::Create(*context->context(), "case", function);
        context->builder()->SetInsertPoint(selectorBlock);
        expression->codegen(context);
        context->builder()->CreateBr(endBlock);
        const auto selectorValue = selectorNode->codegen(context);
        switchInstruction->addCase(llvm::cast<llvm::ConstantInt>(selectorValue), selectorBlock);
    }
    context->builder()->SetInsertPoint(defaultBlock);
    if (!m_elseExpressions.empty())
    {
        for (const auto &elseExpression: m_elseExpressions)
        {
            elseExpression->codegen(context);
        }
    }
    context->builder()->CreateBr(endBlock);

    context->builder()->SetInsertPoint(endBlock);

    return nullptr;
}
llvm::Value *CaseNode::codegen_strings(std::unique_ptr<Context> &context)
{
    const auto function = context->builder()->GetInsertBlock()->getParent();

    const auto value = m_selector->codegen(context);
    const auto compareFunction = context->module()->getFunction("comparestr(string,string)");
    const auto defaultBlock = llvm::BasicBlock::Create(*context->context(), "caseDefault", function);
    llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(*context->context(), "caseEnd", function);


    llvm::BasicBlock *nextCaseBlock = nullptr;
    size_t caseIndex = 0;
    for (auto &[selectorNode, expression]: m_selectors)
    {
        const auto selectorValue = selectorNode->codegen(context);

        std::vector arguments = {value, selectorValue};

        const auto lhs = context->builder()->CreateCall(compareFunction, arguments);
        const auto rhs = context->builder()->getInt32(0);
        const auto condition = context->builder()->CreateICmpEQ(lhs, rhs);
        const auto selectorTrueBlock = llvm::BasicBlock::Create(*context->context(), "caseTrue", function);
        if (caseIndex == m_selectors.size() - 1)
        {
            nextCaseBlock = defaultBlock;
        }
        else
        {
            nextCaseBlock = llvm::BasicBlock::Create(*context->context(), "caseFalse", function);
        }
        context->builder()->CreateCondBr(condition, selectorTrueBlock, nextCaseBlock);
        context->builder()->SetInsertPoint(selectorTrueBlock);
        expression->codegen(context);
        context->builder()->CreateBr(endBlock);
        if (caseIndex < m_selectors.size() - 1)
        {
            context->builder()->SetInsertPoint(nextCaseBlock);
        }
        caseIndex++;
    }

    context->builder()->SetInsertPoint(defaultBlock);
    if (!m_elseExpressions.empty())
    {
        for (const auto &elseExpression: m_elseExpressions)
        {
            elseExpression->codegen(context);
        }
    }
    context->builder()->CreateBr(endBlock);

    context->builder()->SetInsertPoint(endBlock);

    return nullptr;
}
llvm::Value *CaseNode::compareSelectorAndValue(llvm::Value *value, const std::shared_ptr<ASTNode> &selector,
                                               std::unique_ptr<Context> &context)
{

    const auto selectorType = selector->resolveType(context->programUnit(), resolveParent(context));

    if (const auto range = std::dynamic_pointer_cast<ValueRangeType>(selectorType))
    {

        return context->builder()->CreateAnd(
                context->builder()->CreateICmpSGE(value,
                                                range->generateLowerBounds(selector->expressionToken(), context)),
                context->builder()->CreateICmpSLE(value,
                                                range->generateUpperBounds(selector->expressionToken(), context)));
    }

    const auto selectorValue = selector->codegen(context);
    return context->builder()->CreateICmpEQ(value, selectorValue);
}
llvm::Value *CaseNode::codegen_ranges(std::unique_ptr<Context> &context)
{
    const auto function = context->builder()->GetInsertBlock()->getParent();

    const auto value = m_selector->codegen(context);
    const auto defaultBlock = llvm::BasicBlock::Create(*context->context(), "caseDefault", function);
    llvm::BasicBlock *endBlock = llvm::BasicBlock::Create(*context->context(), "caseEnd", function);


    llvm::BasicBlock *nextCaseBlock = nullptr;
    size_t caseIndex = 0;
    for (auto &[selectorNode, expression]: m_selectors)
    {

        const auto lhs = compareSelectorAndValue(value, selectorNode, context);
        const auto rhs = context->builder()->getTrue();
        const auto condition = context->builder()->CreateICmpEQ(lhs, rhs);
        const auto selectorTrueBlock = llvm::BasicBlock::Create(*context->context(), "caseTrue", function);
        if (caseIndex == m_selectors.size() - 1)
        {
            nextCaseBlock = defaultBlock;
        }
        else
        {
            nextCaseBlock = llvm::BasicBlock::Create(*context->context(), "caseFalse", function);
        }
        context->builder()->CreateCondBr(condition, selectorTrueBlock, nextCaseBlock);
        context->builder()->SetInsertPoint(selectorTrueBlock);
        expression->codegen(context);
        context->builder()->CreateBr(endBlock);
        if (caseIndex < m_selectors.size() - 1)
        {

            context->builder()->SetInsertPoint(nextCaseBlock);
        }
        caseIndex++;
    }

    context->builder()->SetInsertPoint(defaultBlock);
    if (!m_elseExpressions.empty())
    {
        for (const auto &elseExpression: m_elseExpressions)
        {
            elseExpression->codegen(context);
        }
    }
    context->builder()->CreateBr(endBlock);

    context->builder()->SetInsertPoint(endBlock);

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
    const auto selectorType = m_selector->resolveType(context->programUnit(), resolveParent(context));
    if (oneSelectorHasARangeType(context->programUnit(), resolveParent(context)))
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
    if (!m_elseExpressions.empty())
    {
        for (const auto &elseExpression: m_elseExpressions)
        {
            elseExpression->typeCheck(unit, parentNode);
        }
    }
}
