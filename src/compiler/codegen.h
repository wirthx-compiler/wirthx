//
// Created by stefan on 01.02.25.
//

#ifndef CODEGEN_H
#define CODEGEN_H

#include <functional>
#include <memory>
namespace llvm
{
    class Value;

};
class Context;

namespace codegen
{
    llvm::Value *codegen_ifexpr(std::unique_ptr<Context> &context, llvm::Value *condition,
                                std::function<void(std::unique_ptr<Context> &)> body);

    llvm::Value *codegen_ifelseexpr(std::unique_ptr<Context> &context, llvm::Value *condition,
                                    std::function<void(std::unique_ptr<Context> &)> ifExpressions,
                                    std::function<void(std::unique_ptr<Context> &)> elseExpressions);

    llvm::Value *codegen_while(std::unique_ptr<Context> &context, llvm::Value *condition,
                               std::function<void(std::unique_ptr<Context> &)> body);
} // namespace codegen

#endif // CODEGEN_H
