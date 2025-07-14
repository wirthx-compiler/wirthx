#include "ArrayInitialisationNode.h"

#include <cassert>
#include <compiler/Context.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
ArrayInitialisationNode::ArrayInitialisationNode(const Token &token,
                                                 const std::vector<std::shared_ptr<ASTNode>> &arguments) :
    ASTNode(token), m_arguments(arguments)
{
}
void ArrayInitialisationNode::print() {}
llvm::Value *ArrayInitialisationNode::codegen(std::unique_ptr<Context> &context)
{

    const auto valueType =
            m_arguments[0]->resolveType(context->programUnit(), resolveParent(context))->generateLlvmType(context);
    llvm::ArrayType *ArrayTy = llvm::ArrayType::get(valueType, m_arguments.size());

    // Define the array content
    std::vector<llvm::Constant *> Elements;
    for (const auto &element: m_arguments)
    {
        Elements.push_back(static_cast<std::vector<llvm::Constant *>::value_type>(element->codegen(context)));
    }

    return llvm::ConstantArray::get(ArrayTy, Elements);
}
