#include "codegen.h"

#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"

llvm::Value *codegen::codegen_ifexpr(std::unique_ptr<Context> &context, llvm::Value *condition,
                                     std::function<void(std::unique_ptr<Context> &)> body)
{
    llvm::Value *CondV = condition;
    if (!CondV)
        return nullptr;
    CondV = context->builder()->CreateICmpEQ(CondV, context->builder()->getTrue(), "ifcond");

    llvm::Function *TheFunction = context->builder()->GetInsertBlock()->getParent();
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*context->context(), "then", TheFunction);
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*context->context(), "ifcont");

    context->builder()->CreateCondBr(CondV, ThenBB, MergeBB);

    context->builder()->SetInsertPoint(ThenBB);

    body(context);

    if (!context->breakBlock().BlockUsed)
        context->builder()->CreateBr(MergeBB);
    context->breakBlock().BlockUsed = false;
    TheFunction->insert(TheFunction->end(), MergeBB);
    context->builder()->SetInsertPoint(MergeBB);

    return CondV;
}

llvm::Value *codegen::codegen_ifelseexpr(std::unique_ptr<Context> &context, llvm::Value *condition,
                                         std::function<void(std::unique_ptr<Context> &)> ifExpressions,
                                         std::function<void(std::unique_ptr<Context> &)> elseExpressions)
{
    if (!condition)
        return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    condition = context->builder()->CreateICmpEQ(condition, context->builder()->getTrue(), "ifcond");
    llvm::Function *TheFunction = context->builder()->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*context->context(), "then", TheFunction);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*context->context(), "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*context->context(), "ifcont");
    context->builder()->CreateCondBr(condition, ThenBB, ElseBB);

    // Emit then value.
    context->builder()->SetInsertPoint(ThenBB);

    ifExpressions(context);
    if (!context->breakBlock().BlockUsed)
        context->builder()->CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = context->builder()->GetInsertBlock();

    // Emit else block.
    TheFunction->insert(TheFunction->end(), ElseBB);
    context->builder()->SetInsertPoint(ElseBB);

    elseExpressions(context);
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
    return condition;
}

llvm::Value *codegen::codegen_while(std::unique_ptr<Context> &context, llvm::Value *condition,
                                    std::function<void(std::unique_ptr<Context> &)> body)
{
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*context->context(), "loop", context->currentFunction());
    llvm::BasicBlock *LoopCondBB =
            llvm::BasicBlock::Create(*context->context(), "loop.cond", context->currentFunction());

    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*context->context(), "afterloop", context->currentFunction());

    // Insert an explicit fall through from the current block to the LoopBB.
    context->builder()->CreateBr(LoopCondBB);
    context->builder()->SetInsertPoint(LoopCondBB);
    // // Convert condition to a bool by comparing non-equal to 0.0.
    // EndCond = context->builder()->CreateICmpEQ(
    //     EndCond, llvm::ConstantInt::get(*context->context(), llvm::APInt(64, 0)), "loopcond");
    // Create the "after loop" block and insert it.
    // Compute the end condition.
    llvm::Value *EndCond = condition;
    if (!EndCond)
        return nullptr;


    // Insert the conditional branch into the end of LoopEndBB.
    context->builder()->CreateCondBr(EndCond, LoopBB, AfterBB);


    // Start insertion in LoopBB.
    context->builder()->SetInsertPoint(LoopBB);

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    auto lastBreakBlock = context->breakBlock().Block;
    context->breakBlock().Block = AfterBB;
    context->breakBlock().BlockUsed = false;

    context->builder()->SetInsertPoint(LoopBB);
    body(context);
    context->builder()->CreateBr(LoopCondBB);

    context->breakBlock().Block = lastBreakBlock;

    // Any new code will be inserted in AfterBB.
    context->builder()->SetInsertPoint(AfterBB);

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*context->context()));
}
