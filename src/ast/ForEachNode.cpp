#include "ForEachNode.h"

#include <llvm/IR/IRBuilder.h>

#include "BlockNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"
#include "types/RangeType.h"
ForEachNode::ForEachNode(const Token &token, const Token &loopVariable, const std::shared_ptr<ASTNode> &loopExpression,
                         const std::vector<std::shared_ptr<ASTNode>> &body) :
    ASTNode(token), m_loopVariable(loopVariable), m_loopExpression(loopExpression), m_body(body)
{
}
void ForEachNode::print() {}
llvm::Value *ForEachNode::codegen(std::unique_ptr<Context> &context)
{
    const auto loopExpressionType = m_loopExpression->resolveType(context->programUnit(), resolveParent(context));
    const auto rangeType = std::dynamic_pointer_cast<RangeType>(loopExpressionType);
    if (!rangeType)
    {
        throw CompilerException(ParserError{.token = m_loopExpression->expressionToken(),
                                            .message = "The loop expression is not a range type."});
    }


    llvm::Value *startValue = rangeType->generateLowerBounds(m_loopExpression->expressionToken(), context);
    if (!startValue)
        return nullptr;

    const auto &builder = context->builder();
    const auto &llvmContext = context->context();


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

    const auto indexName = "index";

    llvm::PHINode *Variable = builder->CreatePHI(targetType, 2, indexName);


    if (startValue->getType()->getIntegerBitWidth() != bitLength)
    {
        startValue = context->builder()->CreateIntCast(startValue, targetType, true, "startValue_cast");
    }
    Variable->addIncoming(startValue, preheaderBB);

    // context->NamedValues[m_loopVariable] = Variable;
    Token expressionToken = m_loopExpression->expressionToken();
    const auto loopVariableAllocation = context->namedAllocation(m_loopVariable.lexical());
    llvm::Value *value = rangeType->generateFieldAccess(expressionToken, Variable, context);
    context->builder()->CreateStore(value, loopVariableAllocation);
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
    llvm::Value *stepValue = builder->getIntN(bitLength, 1);

    llvm::Value *nextVar = builder->CreateAdd(Variable, stepValue, "nextvar");

    //  Compute the end condition.
    llvm::Value *EndCond = rangeType->generateUpperBounds(m_loopExpression->expressionToken(), context);
    if (!EndCond)
        return nullptr;
    if (EndCond->getType()->getIntegerBitWidth() != bitLength)
    {
        EndCond = context->builder()->CreateIntCast(EndCond, targetType, true, "lhs_cast");
    }

    // Convert condition to a bool by comparing non-equal to 0.0.
    EndCond = context->builder()->CreateCmp(llvm::CmpInst::ICMP_SLE, nextVar, EndCond, "for.loopcond");


    // Create the "after loop" block and insert it.
    llvm::BasicBlock *loopEndBB = builder->GetInsertBlock();

    // Insert the conditional branch into the end of loopEndBB.
    builder->CreateCondBr(EndCond, loopBB, afterBB);

    // Any new code will be inserted in AfterBB.
    builder->SetInsertPoint(afterBB);

    // Add a new entry to the PHI node for the backedge.
    Variable->addIncoming(nextVar, loopEndBB);

    // Restore the unshadowed variable.

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*llvmContext));
}
std::optional<std::shared_ptr<ASTNode>> ForEachNode::block()
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
void ForEachNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    ASTNode::typeCheck(unit, parentNode);
}
