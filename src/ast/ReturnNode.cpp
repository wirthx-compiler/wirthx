#include "ReturnNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"

ReturnNode::ReturnNode(const Token &token, std::shared_ptr<ASTNode> expression) :
    ASTNode(token), m_expression(expression)
{
}

void ReturnNode::print()
{
    std::cout << "return ";
    m_expression->print();
    std::cout << "\n";
}


llvm::Value *ReturnNode::codegen(std::unique_ptr<Context> &context)
{
    auto RetVal = m_expression->codegen(context);
    context->builder()->CreateRet(RetVal);
    return nullptr;
}
