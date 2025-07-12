#include "BinaryOperationNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>

#include "UnitNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"
#include "magic_enum/magic_enum.hpp"
#include "types/StringType.h"

BinaryOperationNode::BinaryOperationNode(const Token &operatorToken, const Operator op,
                                         const std::shared_ptr<ASTNode> &lhs, const std::shared_ptr<ASTNode> &rhs) :
    ASTNode(operatorToken), m_operatorToken(operatorToken), m_lhs(lhs), m_rhs(rhs), m_operator(op)
{
}

void BinaryOperationNode::print()
{
    m_lhs->print();
    std::cout << static_cast<char>(m_operator);
    m_rhs->print();
}


llvm::Value *BinaryOperationNode::generateForInteger(llvm::Value *lhs, llvm::Value *rhs,
                                                     std::unique_ptr<Context> &context)
{
    const unsigned maxBitWith = std::max(lhs->getType()->getIntegerBitWidth(), rhs->getType()->getIntegerBitWidth());
    const auto targetType = llvm::IntegerType::get(*context->context(), maxBitWith);
    if (maxBitWith != lhs->getType()->getIntegerBitWidth())
    {
        lhs = context->builder()->CreateIntCast(lhs, targetType, true, "lhs_cast");
    }
    if (maxBitWith != rhs->getType()->getIntegerBitWidth())
    {
        rhs = context->builder()->CreateIntCast(rhs, targetType, true, "rhs_cast");
    }


    switch (m_operator)
    {
        case Operator::PLUS:
            return context->builder()->CreateAdd(lhs, rhs, "addtmp");
        case Operator::MINUS:
            return context->builder()->CreateSub(lhs, rhs, "subtmp");
        case Operator::MUL:
            return context->builder()->CreateMul(lhs, rhs, "multmp");
        case Operator::MOD:
            return context->builder()->CreateSRem(lhs, rhs, "srem");
        case Operator::IDIV:
            return context->builder()->CreateSDiv(lhs, rhs, "sdiv");
        default:
            throw CompilerException(ParserError{.token = ASTNode::expressionToken(),
                                                .message = "the operation " +
                                                           std::string(magic_enum::enum_name(m_operator)) +
                                                           " cannot be applied for an integer"});
    }
    return nullptr;
}
llvm::Value *BinaryOperationNode::generateForFloat(llvm::Value *lhs, llvm::Value *rhs,
                                                   std::unique_ptr<Context> &context)
{
    if (lhs->getType()->isFloatTy() && rhs->getType()->isDoubleTy())
    {
        lhs = context->builder()->CreateFPCast(lhs, rhs->getType());
    }
    else if (lhs->getType()->isDoubleTy() && rhs->getType()->isFloatTy())
    {
        lhs = context->builder()->CreateFPCast(rhs, lhs->getType());
    }


    switch (m_operator)
    {
        case Operator::PLUS:
            return context->builder()->CreateFAdd(lhs, rhs, "addtmp");
        case Operator::MINUS:
            return context->builder()->CreateFSub(lhs, rhs, "subtmp");
        case Operator::MUL:
            return context->builder()->CreateFMul(lhs, rhs, "multmp");
        case Operator::MOD:
            return context->builder()->CreateFRem(lhs, rhs, "srem");
        case Operator::DIV:
            return context->builder()->CreateFDiv(lhs, rhs, "sdiv");
        default:
            throw CompilerException(ParserError{.token = ASTNode::expressionToken(),
                                                .message = "the operation " +
                                                           std::string(magic_enum::enum_name(m_operator)) +
                                                           " cannot be applied for an float/double"});
    }
    return nullptr;
}

llvm::Value *BinaryOperationNode::generateForStringPlusChar(llvm::Value *lhs, llvm::Value *rhs,
                                                            std::unique_ptr<Context> &context)
{
    const auto varType = StringType::getString();
    const auto valueType = VariableType::getInteger(8)->generateLlvmType(context);
    const auto llvmRecordType = varType->generateLlvmType(context);
    const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);
    const auto stringAlloc = context->builder()->CreateAlloca(llvmRecordType, nullptr, "combined_string");


    const auto arrayRefCountOffset =
            context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 0, "combined_string.refCount.offset");
    const auto arraySizeOffset =
            context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 1, "combined_string.size.offset");


    const auto arrayPointerOffset =
            context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 2, "combined_string.ptr.offset");

    const auto lhsIndexPtr = context->builder()->CreateStructGEP(llvmRecordType, lhs, 1, "lhs.size.offset");


    // lhs size
    const auto lhsIndex = context->builder()->CreateLoad(indexType, lhsIndexPtr, "lhs.size");

    llvm::Value *rhsSize = nullptr;
    if (rhs->getType()->getIntegerBitWidth() == 8)
    {
        rhsSize = context->builder()->getInt64(1);
    }

    const auto newSize = context->builder()->CreateAdd(lhsIndex, rhsSize, "new_size");


    // change array size
    context->builder()->CreateStore(context->builder()->getInt64(1), arrayRefCountOffset);
    context->builder()->CreateStore(newSize, arraySizeOffset);

    const auto allocCall = context->builder()->CreateMalloc(
            indexType, context->builder()->getPtrTy(),
            context->builder()->CreateAdd(newSize, context->builder()->getInt64(1)), newSize);

    {
        const auto memcpyCall = llvm::Intrinsic::getDeclaration(
                context->module().get(), llvm::Intrinsic::memcpy,
                {context->builder()->getPtrTy(), context->builder()->getPtrTy(), context->builder()->getInt64Ty()});
        const auto boundsLhs = context->builder()->CreateGEP(
                valueType, allocCall, llvm::ArrayRef<llvm::Value *>{context->builder()->getInt64(0)}, "", false);
        const auto lhsPtrOffset = context->builder()->CreateStructGEP(llvmRecordType, lhs, 2, "lhs.ptr.offset");
        const auto loadResult =
                context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), lhsPtrOffset);
        std::vector<llvm::Value *> memcopyArgs;
        memcopyArgs.push_back(context->builder()->CreateBitCast(boundsLhs, context->builder()->getPtrTy()));
        memcopyArgs.push_back(context->builder()->CreateBitCast(loadResult, context->builder()->getPtrTy()));
        memcopyArgs.push_back(lhsIndex);
        memcopyArgs.push_back(context->builder()->getFalse());

        context->builder()->CreateCall(memcpyCall, memcopyArgs);
    }
    context->builder()->CreateStore(allocCall, arrayPointerOffset);

    {

        const auto bounds =
                context->builder()->CreateGEP(valueType, allocCall, llvm::ArrayRef<llvm::Value *>{lhsIndex}, "", false);

        context->builder()->CreateStore(rhs, bounds);
    }
    {

        const auto bounds = context->builder()->CreateGEP(
                valueType, allocCall,
                llvm::ArrayRef<llvm::Value *>{context->builder()->CreateAdd(lhsIndex, context->builder()->getInt64(1))},
                "", false);

        context->builder()->CreateStore(context->builder()->getInt8(0), bounds);
    }
    return stringAlloc;
}

llvm::Value *BinaryOperationNode::generateForString(llvm::Value *lhs, llvm::Value *rhs,
                                                    std::unique_ptr<Context> &context)
{

    const auto varType = StringType::getString();
    const auto valueType = VariableType::getInteger(8)->generateLlvmType(context);
    const auto llvmRecordType = varType->generateLlvmType(context);
    const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);
    const auto stringAlloc = context->builder()->CreateAlloca(llvmRecordType, nullptr, "combined_string");


    const auto arrayRefCountOffset =
            context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 0, "combined_string.refCount.offset");
    const auto arraySizeOffset =
            context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 1, "combined_string.size.offset");


    const auto arrayPointerOffset =
            context->builder()->CreateStructGEP(llvmRecordType, stringAlloc, 2, "combined_string.ptr.offset");

    const auto lhsIndexPtr = context->builder()->CreateStructGEP(llvmRecordType, lhs, 1, "lhs.size.offset");

    const auto rhsIndexPtr = context->builder()->CreateStructGEP(llvmRecordType, rhs, 1, "rhs.size.offset");
    // lhs size
    const auto lhsIndex = context->builder()->CreateLoad(indexType, lhsIndexPtr, "lhs.size");
    const auto rhsIndex = context->builder()->CreateLoad(indexType, rhsIndexPtr, "rhs.size");

    const auto newSize = context->builder()->CreateAdd(
            lhsIndex, context->builder()->CreateAdd(rhsIndex, context->builder()->getInt64(-1)), "new_size");
    ;


    // change array size
    context->builder()->CreateStore(context->builder()->getInt64(1), arrayRefCountOffset);
    context->builder()->CreateStore(newSize, arraySizeOffset);

    const auto allocCall =
            context->builder()->CreateMalloc(indexType, context->builder()->getPtrTy(), newSize, nullptr);


    const auto memcpyCall = llvm::Intrinsic::getDeclaration(
            context->module().get(), llvm::Intrinsic::memcpy,
            {context->builder()->getPtrTy(), context->builder()->getPtrTy(), context->builder()->getInt64Ty()});
    {
        const auto boundsLhs = context->builder()->CreateGEP(
                valueType, allocCall, llvm::ArrayRef<llvm::Value *>{context->builder()->getInt64(0)}, "", false);
        const auto lhsPtrOffset = context->builder()->CreateStructGEP(llvmRecordType, lhs, 2, "lhs.ptr.offset");
        const auto loadResult =
                context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), lhsPtrOffset);
        std::vector<llvm::Value *> memcopyArgs;
        memcopyArgs.push_back(context->builder()->CreateBitCast(boundsLhs, context->builder()->getPtrTy()));
        memcopyArgs.push_back(context->builder()->CreateBitCast(loadResult, context->builder()->getPtrTy()));
        memcopyArgs.push_back(lhsIndex);
        memcopyArgs.push_back(context->builder()->getFalse());

        context->builder()->CreateCall(memcpyCall, memcopyArgs);
    }
    context->builder()->CreateStore(allocCall, arrayPointerOffset);

    {
        const auto rhsPtrOffset = context->builder()->CreateStructGEP(llvmRecordType, rhs, 2, "rhs.ptr.offset");
        const auto loadResult =
                context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), rhsPtrOffset);
        const auto bounds = context->builder()->CreateGEP(valueType, allocCall,
                                                          llvm::ArrayRef<llvm::Value *>{context->builder()->CreateAdd(
                                                                  lhsIndex, context->builder()->getInt64(-1))},
                                                          "", false);

        std::vector<llvm::Value *> memcopyArgs;
        memcopyArgs.push_back(context->builder()->CreateBitCast(bounds, context->builder()->getPtrTy()));
        memcopyArgs.push_back(context->builder()->CreateBitCast(loadResult, context->builder()->getPtrTy()));
        memcopyArgs.push_back(rhsIndex);
        memcopyArgs.push_back(context->builder()->getFalse());

        context->builder()->CreateCall(memcpyCall, memcopyArgs);
    }

    return stringAlloc;
}

llvm::Value *BinaryOperationNode::codegen(std::unique_ptr<Context> &context)
{

    llvm::Value *lhs = m_lhs->codegen(context);
    llvm::Value *rhs = m_rhs->codegen(context);
    if (!lhs || !rhs)
        return nullptr;

    const auto parent = resolveParent(context);

    if (lhs->getType()->isIntegerTy())
    {
        return generateForInteger(lhs, rhs, context);
    }

    const auto lhs_type = m_lhs->resolveType(context->programUnit(), parent);
    const auto rhs_type = m_rhs->resolveType(context->programUnit(), parent);


    switch (lhs_type->baseType)
    {
        case VariableBaseType::Integer:
            return generateForInteger(lhs, rhs, context);
        case VariableBaseType::String:
            switch (rhs_type->baseType)
            {
                case VariableBaseType::String:
                    return generateForString(lhs, rhs, context);
                case VariableBaseType::Character:
                    return generateForStringPlusChar(lhs, rhs, context);
                default:
                    assert(false && "unknown variable type for binary operation");
                    break;
            }
        case VariableBaseType::Double:
        case VariableBaseType::Float:
            return generateForFloat(lhs, rhs, context);
        default:
            assert(false && "unknown variable type for binary operation");
            break;
    }

    return nullptr;
}

std::shared_ptr<VariableType> BinaryOperationNode::resolveType(const std::unique_ptr<UnitNode> &unit,
                                                               ASTNode *parentNode)
{
    if (auto type = m_lhs->resolveType(unit, parentNode))
    {
        return type;
    }
    return std::make_shared<VariableType>();
}
void BinaryOperationNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    if (const auto lhsType = m_lhs->resolveType(unit, parentNode);
        const auto rhsType = m_rhs->resolveType(unit, parentNode))
    {
        if (*lhsType != *rhsType)
        {
            if (not(lhsType->baseType == VariableBaseType::String && rhsType->baseType == VariableBaseType::Character))
            {
                throw CompilerException(ParserError{
                        .token = m_operatorToken,
                        .message = "the binary operation of \"" + lhsType->typeName + "\" and \"" + rhsType->typeName +
                                   "\" is not possible because the types are not the same"});
            }
        }
        else if ((!lhsType->isNumberType() or !rhsType->isNumberType()) and
                 (lhsType->baseType != VariableBaseType::String and rhsType->baseType != VariableBaseType::String))
        {
            throw CompilerException(ParserError{
                    .token = m_operatorToken,
                    .message = "the binary operation of \"" + lhsType->typeName + "\" and \"" + rhsType->typeName +
                               "\" is not possible because the types can not be used in a binary operation"});
        }
    }
}
Token BinaryOperationNode::expressionToken()
{
    const auto start = m_lhs->expressionToken().sourceLocation.byte_offset;
    const auto end =
            m_rhs->expressionToken().sourceLocation.byte_offset + m_rhs->expressionToken().sourceLocation.num_bytes;
    Token token = ASTNode::expressionToken();
    token.sourceLocation.num_bytes = end - start;
    token.sourceLocation.byte_offset = start;
    return token;
}
