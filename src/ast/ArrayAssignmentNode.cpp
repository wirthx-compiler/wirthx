#include "ArrayAssignmentNode.h"
#include "ArrayAccessNode.h"
#include "SystemFunctionCallNode.h"
#include "UnitNode.h"
#include "VariableAccessNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "types/ArrayType.h"
#include "types/StringType.h"

ArrayAssignmentNode::ArrayAssignmentNode(const Token &arrayToken, const std::shared_ptr<ASTNode> &indexNode,
                                         const std::shared_ptr<ASTNode> &expression) :
    ASTNode(arrayToken), m_arrayToken(arrayToken), m_variableName(std::string(arrayToken.lexical())),
    m_indexNode(indexNode), m_expression(expression)
{
}

void ArrayAssignmentNode::print() {}


void ArrayAssignmentNode::range_check(const std::shared_ptr<FieldAccessableType> &fieldAccesableType,
                                      std::unique_ptr<Context> &context)
{
    const Token token = expressionToken();

    const auto lowValue = context->builder()->getInt64(0);
    auto index = m_indexNode->codegen(context);
    constexpr unsigned maxBitWith = 64;
    const auto targetType = llvm::IntegerType::get(*context->context(), maxBitWith);
    if (maxBitWith != index->getType()->getIntegerBitWidth())
    {
        index = context->builder()->CreateIntCast(index, targetType, true, "lhs_cast");
    }

    const auto highValue = fieldAccesableType->generateHighValue(m_arrayToken, context);
    const auto compareSmaller = context->builder()->CreateICmpSLE(index, highValue);
    const auto compareGreater = context->builder()->CreateICmpSGE(index, lowValue);
    const auto andNode = context->builder()->CreateAnd(compareGreater, compareSmaller);
    const std::string message = "index out of range for expression: " + token.lexical();

    SystemFunctionCallNode::codegen_assert(context, resolveParent(context), this, andNode, message);
}

llvm::Value *ArrayAssignmentNode::codegen(std::unique_ptr<Context> &context)
{
    // Look this variable up in the function.
    auto V = context->findValue(m_variableName);

    if (!V)
        return LogErrorV("Unknown variable name for array assignment: " + m_variableName);

    const auto result = m_expression->codegen(context);

    auto index = m_indexNode->codegen(context);
    auto arrayDef = context->programUnit()->getVariableDefinition(m_variableName);
    std::shared_ptr<VariableType> variableType = nullptr;

    if (context->currentFunction())
    {
        if (const auto functionDef =
                    context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str()))
        {
            arrayDef = functionDef.value()->body()->getVariableDefinition(m_variableName);
            if (!arrayDef)
            {
                if (const auto param = functionDef.value()->getParam(m_variableName))
                    variableType = param.value().type;
            }
        }
    }
    if (arrayDef)
    {
        variableType = arrayDef.value().variableType;
    }
    if (!variableType)
    {
        return LogErrorV("Unknown variable name for array assignment: " + m_variableName);
    }
    if (const auto def = std::dynamic_pointer_cast<ArrayType>(variableType))
    {
        bool runtimeRangeCheck = true;
        if (llvm::isa<llvm::ConstantInt>(index) && !def->isDynArray)
        {
            const auto value = reinterpret_cast<llvm::ConstantInt *>(index);

            if (value->getSExtValue() < static_cast<int64_t>(def->low) ||
                value->getSExtValue() > static_cast<int64_t>(def->high))
            {
                throw CompilerException(
                        ParserError{.token = m_arrayToken, .message = "the array index is not in the defined range."});
            }
            runtimeRangeCheck = false;
        }

        if (def->low > 0)
            index = context->builder()->CreateSub(
                    index, context->builder()->getIntN(index->getType()->getIntegerBitWidth(), def->low), "subtmp");

        const auto llvmRecordType = def->generateLlvmType(context);

        if (runtimeRangeCheck)
        {
            auto type = std::dynamic_pointer_cast<FieldAccessableType>(def);
            range_check(type, context);
        }

        if (def->isDynArray)
        {

            const auto arrayBaseType = def->arrayBase->generateLlvmType(context);

            const auto arrayPointerOffset =
                    context->builder()->CreateStructGEP(llvmRecordType, V.value(), 1, "array.ptr.offset");

            const auto loadResult = context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()),
                                                                   arrayPointerOffset);


            const auto bounds = context->builder()->CreateGEP(arrayBaseType, loadResult,
                                                              llvm::ArrayRef<llvm::Value *>{index}, "", true);

            context->builder()->CreateStore(result, bounds);
            return result;
        }

        const auto bounds = context->builder()->CreateGEP(
                llvmRecordType, V.value(), {context->builder()->getInt64(0), index}, "arrayindex", false);

        context->builder()->CreateStore(result, bounds);
        return result;
    }
    if (const auto def = std::dynamic_pointer_cast<StringType>(variableType))
    {
        auto type = std::dynamic_pointer_cast<FieldAccessableType>(def);
        range_check(type, context);
        const auto llvmRecordType = def->generateLlvmType(context);
        const auto arrayBaseType = IntegerType::getInteger(8)->generateLlvmType(context);

        const auto arrayPointerOffset =
                context->builder()->CreateStructGEP(llvmRecordType, V.value(), 2, "array.ptr.offset");

        const auto loadResult =
                context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), arrayPointerOffset);


        const auto bounds = context->builder()->CreateGEP(arrayBaseType, loadResult,
                                                          llvm::ArrayRef<llvm::Value *>{index}, "", true);

        context->builder()->CreateStore(result, bounds);
    }

    if (const auto ptrType = std::dynamic_pointer_cast<PointerType>(variableType))
    {
        const auto arrayBaseType = ptrType->pointerBase->generateLlvmType(context);


        const auto loadResult =
                context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), V.value());


        const auto bounds = context->builder()->CreateGEP(arrayBaseType, loadResult,
                                                          llvm::ArrayRef<llvm::Value *>{index}, "", true);

        context->builder()->CreateStore(result, bounds);
    }
    return nullptr;
}
Token ArrayAssignmentNode::expressionToken()
{
    auto start = m_arrayToken.sourceLocation.byte_offset;
    auto end = m_expression->expressionToken().sourceLocation.byte_offset +
               m_expression->expressionToken().sourceLocation.num_bytes;
    Token token = m_arrayToken;
    token.sourceLocation.num_bytes = end - start;
    token.sourceLocation.byte_offset = start;
    return token;
}
