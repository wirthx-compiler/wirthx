#include "RepeatUntilNode.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
#include "exceptions/CompilerException.h"

RepeatUntilNode::RepeatUntilNode(const Token &token, std::shared_ptr<ASTNode> loopCondition,
                                 std::vector<std::shared_ptr<ASTNode>> nodes) :
    ASTNode(token), m_loopCondition(std::move(loopCondition)), m_nodes(std::move(nodes))
{
}
void RepeatUntilNode::print() {}
llvm::Value *RepeatUntilNode::codegen(std::unique_ptr<Context> &context)
{

    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*context->context(), "repeat", context->currentFunction());
    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*context->context(), "afteruntil", context->currentFunction());

    // Insert an explicit fall through from the current block to the LoopBB.
    context->builder()->CreateBr(LoopBB);
    context->builder()->SetInsertPoint(LoopBB);


    // Start insertion in LoopBB.
    context->builder()->SetInsertPoint(LoopBB);
    auto savedBreakBlock = context->breakBlock().Block;
    context->breakBlock().Block = AfterBB;
    context->breakBlock().BlockUsed = false;
    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    for (auto &node: m_nodes)
    {
        context->builder()->SetInsertPoint(LoopBB);
        node->codegen(context);
    }

    context->breakBlock().Block = savedBreakBlock;

    // // Convert condition to a bool by comparing non-equal to 0.0.

    // Create the "after loop" block and insert it.
    // Compute the end condition.
    llvm::Value *EndCond = m_loopCondition->codegen(context);
    if (!EndCond)
        return nullptr;

    // Insert the conditional branch into the end of LoopEndBB.
    context->builder()->CreateCondBr(EndCond, AfterBB, LoopBB);


    // Any new code will be inserted in AfterBB.
    context->builder()->SetInsertPoint(AfterBB);

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*context->context()));
}
void RepeatUntilNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    if (const auto &conditionType = m_loopCondition->resolveType(unit, parentNode))
    {
        if (conditionType->baseType != VariableBaseType::Boolean)
        {
            throw CompilerException(ParserError{.token = expressionToken(),
                                                .message = "the loop expression does not return a boolean"});
        }
    }
    for (const auto &exp: m_nodes)
    {
        exp->typeCheck(unit, parentNode);
    }
}
