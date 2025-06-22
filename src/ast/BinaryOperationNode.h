#include "ASTNode.h"
#include "NumberNode.h"

enum class Operator : char
{
    PLUS = '+',
    MINUS = '-',
    MUL = '*',
    MOD = '%',
    DIV = '/',
    IDIV = 'D'
};

class BinaryOperationNode : public ASTNode
{
    Token m_operatorToken;
    std::shared_ptr<ASTNode> m_lhs;
    std::shared_ptr<ASTNode> m_rhs;
    Operator m_operator;

    llvm::Value *generateForInteger(llvm::Value *lhs, llvm::Value *rhs, std::unique_ptr<Context> &context);
    llvm::Value *generateForFloat(llvm::Value *lhs, llvm::Value *rhs, std::unique_ptr<Context> &context);
    llvm::Value *generateForString(llvm::Value *lhs, llvm::Value *rhs, std::unique_ptr<Context> &context);

public:
    BinaryOperationNode(const Token &operatorToken, Operator op, const std::shared_ptr<ASTNode> &lhs,
                        const std::shared_ptr<ASTNode> &rhs);
    ~BinaryOperationNode() override = default;

    void print() override;
    llvm::Value *generateForStringPlusChar(llvm::Value *lhs, llvm::Value *rhs, std::unique_ptr<Context> &context);
    llvm::Value *codegen(std::unique_ptr<Context> &context) override;
    std::shared_ptr<VariableType> resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;

    void typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode) override;

    [[nodiscard]] std::shared_ptr<ASTNode> lhs() const { return m_lhs; }
    [[nodiscard]] std::shared_ptr<ASTNode> rhs() const { return m_rhs; }
    [[nodiscard]] Operator binoperator() const { return m_operator; }

    Token expressionToken() override;
    ;
};
