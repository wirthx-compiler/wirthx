#include "BreakNode.h"
#include "compiler/Context.h"
#include "llvm/IR/IRBuilder.h"

BreakNode::BreakNode(const Token &token) : ASTNode(token) {}


void BreakNode::print() {}

llvm::Value *BreakNode::codegen(std::unique_ptr<Context> &context)
{
    assert(context->breakBlock().Block != nullptr && "no break block defined");
    context->builder()->CreateBr(context->breakBlock().Block);
    context->breakBlock().BlockUsed = true;
    return nullptr;
}
