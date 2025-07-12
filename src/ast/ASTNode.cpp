#include "ASTNode.h"

#include <compiler/Context.h>
#include <llvm/IR/Function.h>

#include "UnitNode.h"

ASTNode::ASTNode(const Token &token) : m_token(token) {}

std::shared_ptr<VariableType> ASTNode::resolveType([[maybe_unused]] const std::unique_ptr<UnitNode> &unit,
                                                   ASTNode *parentNode)
{
    return std::make_shared<VariableType>();
}
ASTNode *ASTNode::resolveParent(const std::unique_ptr<Context> &context)
{
    ASTNode *parent = context->programUnit().get();
    if (context->currentFunction())
    {
        if (const auto def =
                    context->programUnit()->getFunctionDefinition(std::string(context->currentFunction()->getName())))
        {
            parent = def.value().get();
        }
    }
    return parent;
}
