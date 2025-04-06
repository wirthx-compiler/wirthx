//
// Created by stefan on 30.03.25.
//

#ifndef CASENODE_H
#define CASENODE_H
#include <vector>


#include "ASTNode.h"

struct Selector
{
    std::shared_ptr<ASTNode> selector;
    std::shared_ptr<ASTNode> expression;
};

class CaseNode : public ASTNode
{
private:
    std::shared_ptr<ASTNode> m_selector;
    std::vector<Selector> m_selectors;
    std::shared_ptr<ASTNode> m_elseExpression;
    llvm::Value *codegen_constants(std::unique_ptr<Context> &context);

public:
    explicit CaseNode(const Token &token, std::shared_ptr<ASTNode> selector, std::vector<Selector> selectors,
                      std::shared_ptr<ASTNode> elseExpression);
    ~CaseNode() override = default;
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::optional<std::shared_ptr<ASTNode>> block() override;
};


#endif // CASENODE_H
