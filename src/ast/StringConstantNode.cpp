#include "StringConstantNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>

#include "compiler/Context.h"
#include "types/StringType.h"


StringConstantNode::StringConstantNode(const Token &token, const std::string &literal) :
    ASTNode(token), m_literal(literal)
{
}

void StringConstantNode::print() { std::cout << "\'" << m_literal << "\'"; }

llvm::GlobalVariable *StringConstantNode::generateConstant(std::unique_ptr<Context> &context, std::string &result) const
{

    bool isEscape = false;
    for (size_t i = 0; i < m_literal.size(); ++i)
    {
        if (m_literal[i] == '\\')
        {
            isEscape = true;
        }
        else if (isEscape)
        {
            switch (m_literal[i])
            {
                case 'n':
                    result += 10;
                    break;
                case 'r':
                    result += 13;
                    break;
            }
            isEscape = false;
        }
        else if (m_literal[i] == '\'')
        {
            result += m_literal[i];
            if (m_literal.size() - 1 > i + 1 && m_literal[i + 1] == '\'')
            {
                i++;
            }
        }
        else
        {
            result += m_literal[i];
        }
    }


    auto resultVar = context->builder()->CreateGlobalString(result, ".str", 0, context->module().get());
    resultVar->setLinkage(llvm::GlobalValue::PrivateLinkage);
    return resultVar;
}
llvm::Value *StringConstantNode::codegen(std::unique_ptr<Context> &context)
{
    std::string result;
    llvm::GlobalVariable *const constant = generateConstant(context, result);
    if (context->currentFunction())
    {
        const auto varType = StringType::getString();
        const auto llvmRecordType = varType->generateLlvmType(context);
        const auto stringAlloc = context->builder()->CreateAlloca(llvmRecordType, nullptr, "string_constant");


        const auto arrayRefCountOffset =
                context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 0, "string.refCount.offset");
        const auto arraySizeOffset =
                context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 1, "string.size.offset");


        const auto arrayPointerOffset =
                context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 2, "string.ptr.offset");
        // auto arrayPointer =
        //         context->builder()->CreateAlignedLoad(arrayBaseType, arrayPointerOffset, alignment, "array.ptr");
        const auto newSize = context->builder()->getInt64(result.size() + 1);
        // change array size
        context->builder()->CreateStore(context->builder()->getInt64(1), arrayRefCountOffset);
        context->builder()->CreateStore(newSize, arraySizeOffset);
        context->builder()->CreateStore(constant, arrayPointerOffset);

        return stringAlloc;
    }
    return constant;
}
llvm::Value *StringConstantNode::codegenForTargetType(std::unique_ptr<Context> &context,
                                                      const std::shared_ptr<VariableType> &targetType)
{
    if (targetType->baseType == VariableBaseType::String)
    {
        return codegen(context);
    }
    if (targetType->baseType == VariableBaseType::Pointer)
    {
        std::string result;
        return generateConstant(context, result);
    }
    return LogErrorV("cannot convert string constant to target type");
}

std::shared_ptr<VariableType> StringConstantNode::resolveType([[maybe_unused]] const std::unique_ptr<UnitNode> &unit,
                                                              ASTNode *parentNode)
{
    return StringType::getString();
}
