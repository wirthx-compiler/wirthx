#include "codegen.h"

#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"

llvm::Value *codegen::codegen_ifexpr(std::unique_ptr<Context> &context, llvm::Value *condition,
                                     std::function<void(std::unique_ptr<Context> &)> body)
{
    llvm::Value *CondV = condition;
    if (!CondV)
        return nullptr;
    CondV = context->Builder->CreateICmpEQ(CondV, context->Builder->getTrue(), "ifcond");

    llvm::Function *TheFunction = context->Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*context->TheContext, "then", TheFunction);
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*context->TheContext, "ifcont");

    context->Builder->CreateCondBr(CondV, ThenBB, MergeBB);

    context->Builder->SetInsertPoint(ThenBB);

    body(context);

    if (!context->BreakBlock.BlockUsed)
        context->Builder->CreateBr(MergeBB);
    context->BreakBlock.BlockUsed = false;
    TheFunction->insert(TheFunction->end(), MergeBB);
    context->Builder->SetInsertPoint(MergeBB);

    return CondV;
}

llvm::Value *codegen::codegen_ifelseexpr(std::unique_ptr<Context> &context, llvm::Value *condition,
                                         std::function<void(std::unique_ptr<Context> &)> ifExpressions,
                                         std::function<void(std::unique_ptr<Context> &)> elseExpressions)
{
    if (!condition)
        return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    condition = context->Builder->CreateICmpEQ(condition, context->Builder->getTrue(), "ifcond");
    llvm::Function *TheFunction = context->Builder->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*context->TheContext, "then", TheFunction);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*context->TheContext, "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*context->TheContext, "ifcont");
    context->Builder->CreateCondBr(condition, ThenBB, ElseBB);

    // Emit then value.
    context->Builder->SetInsertPoint(ThenBB);

    ifExpressions(context);
    if (!context->BreakBlock.BlockUsed)
        context->Builder->CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = context->Builder->GetInsertBlock();

    // Emit else block.
    TheFunction->insert(TheFunction->end(), ElseBB);
    context->Builder->SetInsertPoint(ElseBB);

    elseExpressions(context);
    if (!context->BreakBlock.BlockUsed)
        context->Builder->CreateBr(MergeBB);
    // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = context->Builder->GetInsertBlock();

    // Emit merge block.
    TheFunction->insert(TheFunction->end(), MergeBB);
    context->Builder->SetInsertPoint(MergeBB);
    // llvm::PHINode *PN =
    //     context->Builder->CreatePHI(llvm::Type::getInt64Ty(*context->TheContext), 2, "iftmp");

    // PN->addIncoming(ThenV, ThenBB);
    // PN->addIncoming(ElseV, ElseBB);
    // return PN;
    return condition;
}

llvm::Value *codegen::codegen_while(std::unique_ptr<Context> &context, llvm::Value *condition,
                                    std::function<void(std::unique_ptr<Context> &)> body)
{
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*context->TheContext, "loop", context->TopLevelFunction);
    llvm::BasicBlock *LoopCondBB =
            llvm::BasicBlock::Create(*context->TheContext, "loop.cond", context->TopLevelFunction);

    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*context->TheContext, "afterloop", context->TopLevelFunction);

    // Insert an explicit fall through from the current block to the LoopBB.
    context->Builder->CreateBr(LoopCondBB);
    context->Builder->SetInsertPoint(LoopCondBB);
    // // Convert condition to a bool by comparing non-equal to 0.0.
    // EndCond = context->Builder->CreateICmpEQ(
    //     EndCond, llvm::ConstantInt::get(*context->TheContext, llvm::APInt(64, 0)), "loopcond");
    // Create the "after loop" block and insert it.
    // Compute the end condition.
    llvm::Value *EndCond = condition;
    if (!EndCond)
        return nullptr;


    // Insert the conditional branch into the end of LoopEndBB.
    context->Builder->CreateCondBr(EndCond, LoopBB, AfterBB);


    // Start insertion in LoopBB.
    context->Builder->SetInsertPoint(LoopBB);

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    auto lastBreakBlock = context->BreakBlock.Block;
    context->BreakBlock.Block = AfterBB;
    context->BreakBlock.BlockUsed = false;

    context->Builder->SetInsertPoint(LoopBB);
    body(context);
    context->Builder->CreateBr(LoopCondBB);

    context->BreakBlock.Block = lastBreakBlock;

    // Any new code will be inserted in AfterBB.
    context->Builder->SetInsertPoint(AfterBB);

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getInt64Ty(*context->TheContext));
}
