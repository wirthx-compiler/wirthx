#include "NilPointerNode.h"
#include <llvm/IR/Constants.h>
#include "compiler/Context.h"

NilPointerNode::NilPointerNode(const Token &token) : ASTNode(token) {}
void NilPointerNode::print() {}
llvm::Value *NilPointerNode::codegen(std::unique_ptr<Context> &context)
{
    return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context->TheContext));
}
std::shared_ptr<VariableType> NilPointerNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    return VariableType::getPointer();
}
