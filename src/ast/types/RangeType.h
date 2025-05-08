//
// Created by stefan on 23.03.25.
//

#ifndef RANGETYPE_H
#define RANGETYPE_H

#include <llvm/IR/Value.h>

#include "ast/ASTNode.h"
#include "compiler/Context.h"


class RangeType
{
public:
    virtual ~RangeType() = default;
    [[nodiscard]] virtual llvm::Value *generateLowerBounds(const Token &token, std::unique_ptr<Context> &context) = 0;
    [[nodiscard]] virtual llvm::Value *generateUpperBounds(const Token &token, std::unique_ptr<Context> &context) = 0;
    virtual llvm::Value *generateFieldAccess(Token &token, llvm::Value *indexValue,
                                             std::unique_ptr<Context> &context) = 0;
};


#endif // RANGETYPE_H
