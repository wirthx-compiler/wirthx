#include "IfConditionNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>

#include "BlockNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"


IfConditionNode::IfConditionNode(const Token &token, const std::shared_ptr<ASTNode> &conditionNode,
                                 const std::vector<std::shared_ptr<ASTNode>> &ifExpressions,
                                 const std::vector<std::shared_ptr<ASTNode>> &elseExpressions) :
    ASTNode(token), m_conditionNode(conditionNode), m_ifExpressions(ifExpressions), m_elseExpressions(elseExpressions)
{
}

void IfConditionNode::print()
{
    std::cout << "if ";
    m_conditionNode->print();
    std::cout << " then\n";

    for (auto &exp: m_ifExpressions)
    {
        exp->print();
    }
    if (m_elseExpressions.size() > 0)
    {
        std::cout << "else\n";
        for (auto &exp: m_elseExpressions)
        {
            exp->print();
        }
        // std::cout << "end;\n";
    }
}

llvm::Value *IfConditionNode::codegenIf(std::unique_ptr<Context> &context)
{
    llvm::Value *CondV = m_conditionNode->codegen(context);
    if (!CondV)
        return nullptr;
    CondV = context->builder()->CreateICmpEQ(CondV, context->builder()->getTrue(), "ifcond");

    llvm::Function *TheFunction = context->builder()->GetInsertBlock()->getParent();
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*context->context(), "then", TheFunction);
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*context->context(), "ifcont");

    context->builder()->CreateCondBr(CondV, ThenBB, MergeBB);

    context->builder()->SetInsertPoint(ThenBB);

    for (auto &exp: m_ifExpressions)
    {
        exp->codegen(context);
    }
    if (!context->breakBlock().BlockUsed)
        context->builder()->CreateBr(MergeBB);
    context->breakBlock().BlockUsed = false;
    TheFunction->insert(TheFunction->end(), MergeBB);
    context->builder()->SetInsertPoint(MergeBB);

    return CondV;
}

llvm::Value *IfConditionNode::codegenIfElse(std::unique_ptr<Context> &context)
{
    llvm::Value *CondV = m_conditionNode->codegen(context);
    if (!CondV)
        return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    CondV = context->builder()->CreateICmpEQ(CondV, context->builder()->getInt1(1), "ifcond");
    llvm::Function *TheFunction = context->builder()->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*context->context(), "then", TheFunction);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*context->context(), "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*context->context(), "ifcont");
    context->builder()->CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then value.
    context->builder()->SetInsertPoint(ThenBB);

    for (auto &exp: m_ifExpressions)
    {
        exp->codegen(context);
    }
    if (!context->breakBlock().BlockUsed)
        context->builder()->CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = context->builder()->GetInsertBlock();

    // Emit else block.
    TheFunction->insert(TheFunction->end(), ElseBB);
    context->builder()->SetInsertPoint(ElseBB);

    for (auto &exp: m_elseExpressions)
    {
        exp->codegen(context);
    }
    if (!context->breakBlock().BlockUsed)
        context->builder()->CreateBr(MergeBB);
    // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = context->builder()->GetInsertBlock();

    // Emit merge block.
    TheFunction->insert(TheFunction->end(), MergeBB);
    context->builder()->SetInsertPoint(MergeBB);
    // llvm::PHINode *PN =
    //     context->builder()->CreatePHI(llvm::Type::getInt64Ty(*context->context()), 2, "iftmp");

    // PN->addIncoming(ThenV, ThenBB);
    // PN->addIncoming(ElseV, ElseBB);
    // return PN;
    return CondV;
}
llvm::Value *IfConditionNode::codegen(std::unique_ptr<Context> &context)
{
    if (!m_elseExpressions.empty())
        return codegenIfElse(context);
    else
        return codegenIf(context);
}
void IfConditionNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    m_conditionNode->typeCheck(unit, parentNode);
    const auto conditionType = m_conditionNode->resolveType(unit, parentNode);
    if (conditionType->baseType != VariableBaseType::Boolean)
    {
        throw CompilerException(
                ParserError{.token = m_conditionNode->expressionToken(),
                            .message = "The type of the expression in the if condition is not a boolean."});
    }

    for (const auto &exp: m_ifExpressions)
    {
        exp->typeCheck(unit, parentNode);
    }

    for (const auto &exp: m_elseExpressions)
    {
        exp->typeCheck(unit, parentNode);
    }
}
bool IfConditionNode::tokenIsPartOfNode(const Token &token) const
{
    if (m_conditionNode->tokenIsPartOfNode(token))
        return true;

    for (const auto &node: m_ifExpressions)
    {
        if (node->tokenIsPartOfNode(token))
        {
            return true;
        }
        if (const auto block = std::dynamic_pointer_cast<BlockNode>(node))
        {

            if (auto result = block->getNodeByToken(token))
            {
                return true;
            }
        }
    }

    for (const auto &node: m_elseExpressions)
    {
        if (node->tokenIsPartOfNode(token))
        {
            return true;
        }
        if (const auto block = std::dynamic_pointer_cast<BlockNode>(node))
        {

            if (auto result = block->getNodeByToken(token))
            {
                return true;
            }
        }
    }
    return false;
}
