#include "ForNode.h"

#include <llvm/IR/IRBuilder.h>

#include <utility>

#include "BlockNode.h"
#include "UnitNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"

ForNode::ForNode(const Token &token, std::string loopVariable, const std::shared_ptr<ASTNode> &startExpression,
                 const std::shared_ptr<ASTNode> &endExpression, const std::vector<std::shared_ptr<ASTNode>> &body,
                 int increment) :
    ASTNode(token), m_loopVariable(std::move(loopVariable)), m_startExpression(startExpression),
    m_endExpression(endExpression), m_body(body), m_increment(increment)
{
}


void ForNode::print() {}

llvm::Value *ForNode::codegen(std::unique_ptr<Context> &context)
{

    llvm::Value *startValue = m_startExpression->codegen(context);
    if (!startValue)
        return nullptr;

    auto &builder = context->builder();
    auto &llvmContext = context->context();
    // Make the new basic block for the loop header, inserting after current
    // block.
    // Within the loop, the variable is defined equal to the PHI node.  If it
    // shadows an existing variable, we have to restore it, so save it now.
    // auto *oldValue = context->namedAllocation(context->NamedAllocations[m_loopVariable]);
    // if (!oldValue)
    //     return nullptr;
    // builder->CreateStore(startValue, context->namedAllocation(context->NamedAllocations[m_loopVariable]));

    llvm::Function *TheFunction = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *preheaderBB = builder->GetInsertBlock();
    llvm::BasicBlock *loopBB = llvm::BasicBlock::Create(*llvmContext, "for.body", TheFunction);
    llvm::BasicBlock *afterBB = llvm::BasicBlock::Create(*llvmContext, "for.cleanup", TheFunction);
    // Insert an explicit fall through from the current block to the LoopBB.
    builder->CreateBr(loopBB);

    // Start insertion in LoopBB.
    builder->SetInsertPoint(loopBB);

    // Start the PHI node with an entry for Start.
    constexpr unsigned bitLength = 64;
    const auto targetType = llvm::Type::getIntNTy(*llvmContext, bitLength);
    // if (auto integerType = std::dynamic_pointer_cast<IntegerType>(endExpressionType))
    // {
    //     bitLength = integerType->length;
    // }
    llvm::PHINode *Variable = builder->CreatePHI(targetType, 2, m_loopVariable);

    if (startValue->getType()->getIntegerBitWidth() != bitLength)
    {
        startValue = context->builder()->CreateIntCast(startValue, targetType, true, "startValue_cast");
    }
    Variable->addIncoming(startValue, preheaderBB);

    context->setNamedValue(m_loopVariable, Variable);

    context->breakBlock().Block = afterBB;
    context->breakBlock().BlockUsed = false;

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    for (const auto &exp: m_body)
    {
        builder->SetInsertPoint(loopBB);
        exp->codegen(context);
    }
    context->breakBlock().Block = nullptr;
    // Emit the step value.
    llvm::Value *stepValue = builder->getIntN(bitLength, m_increment);

    llvm::Value *nextVar = builder->CreateAdd(Variable, stepValue, "nextvar");
    // builder->CreateStore(nextVar, context->namedAllocation(context->NamedAllocations[m_loopVariable]));
    //  Compute the end condition.
    llvm::Value *EndCond = m_endExpression->codegen(context);
    if (!EndCond)
        return nullptr;
    if (EndCond->getType()->getIntegerBitWidth() != bitLength)
    {
        EndCond = context->builder()->CreateIntCast(EndCond, targetType, true, "lhs_cast");
    }

    // Convert condition to a bool by comparing non-equal to 0.0.
    if (m_increment > 0)
        EndCond = context->builder()->CreateCmp(llvm::CmpInst::ICMP_SLE, nextVar, EndCond, "for.loopcond");
    else
        EndCond = context->builder()->CreateCmp(llvm::CmpInst::ICMP_SGE, nextVar, EndCond, "for.loopcond");

    // Create the "after loop" block and insert it.
    llvm::BasicBlock *loopEndBB = builder->GetInsertBlock();

    // Insert the conditional branch into the end of loopEndBB.
    builder->CreateCondBr(EndCond, loopBB, afterBB);

    // Any new code will be inserted in AfterBB.
    builder->SetInsertPoint(afterBB);

    // Add a new entry to the PHI node for the backedge.
    Variable->addIncoming(nextVar, loopEndBB);

    // Restore the unshadowed variable.
    // context->namedAllocation(context->NamedAllocations[m_loopVariable]) = result;

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*llvmContext));
}


std::optional<std::shared_ptr<ASTNode>> ForNode::block()
{
    for (auto &exp: m_body)
    {
        if (auto block = std::dynamic_pointer_cast<BlockNode>(exp))
        {
            return block;
        }
    }
    return std::nullopt;
}
void ForNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    const auto startType = m_startExpression->resolveType(unit, parentNode);
    const auto endType = m_endExpression->resolveType(unit, parentNode);

    if (*startType != *endType)
    {
        throw CompilerException(ParserError{
                .token = m_startExpression->expressionToken(),
                .message = "The type of the start expression does not match the type of the end expression."});
    }

    for (const auto &exp: m_body)
    {
        exp->typeCheck(unit, parentNode);
    }
}
