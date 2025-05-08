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
    std::vector<std::shared_ptr<ASTNode>> m_elseExpressions;
    llvm::Value *codegen_constants(std::unique_ptr<Context> &context);
    llvm::Value *codegen_strings(std::unique_ptr<Context> &context);
    static llvm::Value *compareSelectorAndValue(llvm::Value *value, const std::shared_ptr<ASTNode> &selector,
                                                std::unique_ptr<Context> &context);
    llvm::Value *codegen_ranges(std::unique_ptr<Context> &context);

    [[nodiscard]] bool oneSelectorHasARangeType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode);

public:
    explicit CaseNode(const Token &token, std::shared_ptr<ASTNode> selector, std::vector<Selector> selectors,
                      std::vector<std::shared_ptr<ASTNode>> elseExpressions);
    ~CaseNode() override = default;
    void print() override;
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::optional<std::shared_ptr<ASTNode>> block() override;
    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;
};


#endif // CASENODE_H
