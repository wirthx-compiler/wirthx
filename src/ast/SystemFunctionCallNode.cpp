#include "SystemFunctionCallNode.h"

#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include <llvm/TargetParser/Triple.h>
#include <utility>
#include <vector>

#include <compiler/codegen.h>

#include "../compare.h"
#include "UnitNode.h"
#include "compiler/Context.h"
#include "types/ArrayType.h"
#include "types/FileType.h"
#include "types/RangeType.h"
#include "types/StringType.h"
#include "types/ValueRangeType.h"


static std::vector<std::string> knownSystemCalls = {"writeln", "write",     "printf",     "exit",   "low",
                                                    "high",    "setlength", "length",     "pchar",  "new",
                                                    "halt",    "assert",    "assignfile", "readln", "closefile",
                                                    "reset",   "rewrite",   "ord",        "chr",    "strdispose"};

bool isKnownSystemCall(const std::string &name)
{
    for (const auto &call: knownSystemCalls)
    {
        if (iequals(call, name))
            return true;
    }
    return false;
}

SystemFunctionCallNode::SystemFunctionCallNode(const Token &token, std::string name,
                                               const std::vector<std::shared_ptr<ASTNode>> &args) :
    FunctionCallNode(token, std::move(name), args)
{
}

llvm::Value *SystemFunctionCallNode::codegen_setlength(std::unique_ptr<Context> &context, ASTNode *parent) const
{
    assert(m_args.size() == 2 && "setlength needs 2 arguments");
    const auto array = m_args[0];
    auto newSize = m_args[1]->codegen(context);
    if (!newSize->getType()->isIntegerTy())
    {
        return nullptr;
    }

    const auto arrayType = array->resolveType(context->ProgramUnit, parent);
    if (arrayType->baseType == VariableBaseType::Array)
    {
        const auto realType = std::dynamic_pointer_cast<ArrayType>(arrayType);
        const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);
        const auto value = array->codegen(context);
        const auto arrayBaseType = realType->arrayBase->generateLlvmType(context);
        const auto llvmRecordType = realType->generateLlvmType(context);


        auto ptrType = llvm::PointerType::getUnqual(arrayBaseType);

        const auto arraySizeOffset = context->Builder->CreateStructGEP(llvmRecordType, value, 0, "array.size.offset");


        const auto arrayPointerOffset = context->Builder->CreateStructGEP(llvmRecordType, value, 1, "array.ptr.offset");
        const llvm::DataLayout &DL = context->TheModule->getDataLayout();
        const auto alignment = DL.getPrefTypeAlign(arrayBaseType);
        auto arrayPointer = context->Builder->CreateAlignedLoad(ptrType, arrayPointerOffset, alignment, "array.ptr");
        if (64 != newSize->getType()->getIntegerBitWidth())
        {
            newSize = context->Builder->CreateIntCast(newSize, indexType, true, "lhs_cast");
        }

        // change array size
        context->Builder->CreateStore(newSize, arraySizeOffset);

        // allocate memory for pointer

        const auto allocSize = context->Builder->CreateMul(
                newSize, context->Builder->getInt64(arrayBaseType->getPrimitiveSizeInBits()));


        const auto condition = context->Builder->CreateCmp(
                llvm::CmpInst::ICMP_EQ, arrayPointer,
                llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context->TheContext)));

        codegen::codegen_ifelseexpr(
                context, condition,
                [indexType, arrayBaseType, allocSize, alignment,
                 arrayPointerOffset](const std::unique_ptr<Context> &ctx)
                {
                    llvm::Value *allocCall = ctx->Builder->CreateMalloc(
                            indexType, //
                            arrayBaseType, // Type of elements
                            allocSize, // Number of elements
                            nullptr // Optional array size multiplier (nullptr for scalar allocation)
                    );

                    ctx->Builder->CreateMemSet(allocCall, //
                                               ctx->Builder->getInt8(0), // Number of elements
                                               allocSize, // Optional array size multiplier (nullptr for scalar
                                               alignment //
                    );
                    ctx->Builder->CreateStore(allocCall, arrayPointerOffset);
                    ctx->Builder->CreateLifetimeStart(allocCall);
                },
                [arrayPointerOffset, arrayPointer, newSize, arrayBaseType](const std::unique_ptr<Context> &ctx)
                {
                    const auto reallocFunc = ctx->TheModule->getFunction("realloc");
                    if (!reallocFunc)
                    {
                        LogErrorV("the function realloc was not found");
                        return;
                    }
                    std::vector<llvm::Value *> ArgsV;

                    ArgsV.push_back(arrayPointer);
                    ArgsV.push_back(
                            ctx->Builder->CreateMul(newSize,
                                                    ctx->Builder->getInt64(arrayBaseType->getPrimitiveSizeInBits() /
                                                                           8))); // Number of elements);
                    const auto reallocCall = ctx->Builder->CreateCall(reallocFunc, ArgsV);
                    ctx->Builder->CreateStore(reallocCall, arrayPointerOffset);
                    ctx->Builder->CreateLifetimeStart(reallocCall);
                }


        );

        return nullptr;
    }
    if (arrayType->baseType == VariableBaseType::String)
    {
        const auto realType = std::dynamic_pointer_cast<StringType>(arrayType);
        const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);
        const auto value = array->codegen(context);
        const llvm::DataLayout &DL = context->TheModule->getDataLayout();
        auto alignment = DL.getPrefTypeAlign(indexType);
        const auto arrayBaseType = IntegerType::getInteger(8)->generateLlvmType(context);
        const auto llvmRecordType = realType->generateLlvmType(context);


        const auto arraySizeOffset = context->Builder->CreateStructGEP(llvmRecordType, value, 1, "array.size.offset");


        const auto arrayPointerOffset = context->Builder->CreateStructGEP(llvmRecordType, value, 2, "array.ptr.offset");
        auto ptrType = llvm::PointerType::getUnqual(arrayBaseType);

        auto arrayPointer = context->Builder->CreateAlignedLoad(ptrType, arrayPointerOffset, alignment, "array.ptr");
        if (64 != newSize->getType()->getIntegerBitWidth())
        {
            newSize = context->Builder->CreateIntCast(newSize, indexType, true, "lhs_cast");
        }

        // change array size
        context->Builder->CreateStore(newSize, arraySizeOffset);

        // allocate memory for pointer

        const auto allocSize = context->Builder->CreateMul(
                newSize, context->Builder->getInt64(arrayBaseType->getPrimitiveSizeInBits()));
        // auto allocCall = context->Builder->CreateCall(context->TheModule->getFunction("malloc"), allocSize);

        const auto condition = context->Builder->CreateCmp(
                llvm::CmpInst::ICMP_EQ, arrayPointer,
                llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context->TheContext)));
        codegen::codegen_ifelseexpr(
                context, condition,
                [indexType, arrayBaseType, allocSize, alignment,
                 arrayPointerOffset](const std::unique_ptr<Context> &ctx)
                {
                    llvm::Value *allocCall = ctx->Builder->CreateMalloc(
                            indexType, //
                            arrayBaseType, // Type of elements
                            allocSize, // Number of elements
                            nullptr // Optional array size multiplier (nullptr for scalar allocation)
                    );

                    ctx->Builder->CreateMemSet(allocCall, //
                                               ctx->Builder->getInt8(0), // Number of elements
                                               allocSize, // Optional array size multiplier (nullptr for scalar
                                               alignment //
                    );
                    ctx->Builder->CreateStore(allocCall, arrayPointerOffset);
                    ctx->Builder->CreateLifetimeStart(allocCall);
                },
                [arrayPointerOffset, arrayPointer, newSize, arrayBaseType](const std::unique_ptr<Context> &ctx)
                {
                    const auto reallocFunc = ctx->TheModule->getFunction("realloc");
                    if (!reallocFunc)
                    {
                        LogErrorV("the function realloc was not found");
                        return;
                    }
                    std::vector<llvm::Value *> ArgsV;

                    ArgsV.push_back(arrayPointer);
                    ArgsV.push_back(
                            ctx->Builder->CreateMul(newSize,
                                                    ctx->Builder->getInt64(arrayBaseType->getPrimitiveSizeInBits() /
                                                                           8))); // Number of elements);
                    const auto reallocCall = ctx->Builder->CreateCall(reallocFunc, ArgsV);
                    ctx->Builder->CreateStore(reallocCall, arrayPointerOffset);
                    ctx->Builder->CreateLifetimeStart(reallocCall);
                }


        );
    }

    return nullptr;
}
llvm::Value *SystemFunctionCallNode::codegen_length(std::unique_ptr<Context> &context, ASTNode *parent) const
{
    const auto paramType = m_args[0]->resolveType(context->ProgramUnit, parent);
    if (const auto type = std::dynamic_pointer_cast<FieldAccessableType>(paramType))
    {
        return type->generateLengthValue(m_args[0]->expressionToken(), context);
    }
    return nullptr;
}
llvm::Value *SystemFunctionCallNode::find_target_fileout(std::unique_ptr<Context> &context, ASTNode *parent) const
{
    llvm::Value *loadedStdOut = context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext),
                                                             context->NamedValues["stdout"]);

    for (const auto &arg: m_args)
    {
        auto type = arg->resolveType(context->ProgramUnit, parent);
        if (const auto fileType = std::dynamic_pointer_cast<FileType>(type))
        {
            const auto llvmFileType = fileType->generateLlvmType(context);
            const auto argValue = arg->codegen(context);

            const auto filePtr = context->Builder->CreateStructGEP(llvmFileType, argValue, 1, "file.ptr");

            loadedStdOut = context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), filePtr);
            loadedStdOut =
                    context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), loadedStdOut);
        }
    }
    return loadedStdOut;
}
llvm::Value *SystemFunctionCallNode::codegen_write(std::unique_ptr<Context> &context, ASTNode *parent) const
{
    llvm::Function *fprintf = context->TheModule->getFunction("fprintf");

    llvm::Value *loadedStdOut = find_target_fileout(context, parent);
    if (!fprintf)
        LogErrorV("the function fprintf was not found");
    for (const auto &arg: m_args)
    {
        auto type = arg->resolveType(context->ProgramUnit, parent);
        auto argValue = arg->codegen(context);

        std::vector<llvm::Value *> ArgsV;
        ArgsV.push_back(loadedStdOut);
        if (const auto integerType = std::dynamic_pointer_cast<IntegerType>(type))
        {
            if (context->TargetTriple->getOS() == llvm::Triple::Win32)
            {
                if (integerType->length > 32)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%lli", "format_int64"));
                else if (integerType->length == 8)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%c", "format_char"));
                else
                    ArgsV.push_back(context->Builder->CreateGlobalString("%i", "format_int"));
            }
            else
            {
                if (integerType->length > 32)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%ld", "format_int64"));
                else if (integerType->length == 8)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%c", "format_char"));
                else
                    ArgsV.push_back(context->Builder->CreateGlobalString("%d", "format_int"));
            }

            ArgsV.push_back(argValue);
        }
        else if (auto stringType = std::dynamic_pointer_cast<StringType>(type))
        {

            ArgsV.push_back(context->Builder->CreateGlobalString("%s", "format_string"));

            const auto stringStructPtr = argValue;
            const auto stringLlvmType = type->generateLlvmType(context);
            const auto arrayPointerOffset =
                    context->Builder->CreateStructGEP(stringLlvmType, stringStructPtr, 2, "write.string.offset");


            const auto value = context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext),
                                                            arrayPointerOffset);

            ArgsV.push_back(value);
        }
        else if (type->baseType == VariableBaseType::Double || type->baseType == VariableBaseType::Float)
        {
            ArgsV.push_back(context->Builder->CreateGlobalString("%f", "format_double"));


            ArgsV.push_back(context->Builder->CreateFPCast(argValue, context->Builder->getDoubleTy()));
        }
        else if (auto fileType = std::dynamic_pointer_cast<FileType>(type))
        {
            continue;
        }
        else if (const auto rangeType = std::dynamic_pointer_cast<ValueRangeType>(type))
        {
            if (context->TargetTriple->getOS() == llvm::Triple::Win32)
            {
                if (rangeType->length() > 32)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%lli", "format_int64"));
                else if (rangeType->length() == 8)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%c", "format_char"));
                else
                    ArgsV.push_back(context->Builder->CreateGlobalString("%i", "format_int"));
            }
            else
            {
                if (rangeType->length() > 32)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%ld", "format_int64"));
                else if (rangeType->length() == 8)
                    ArgsV.push_back(context->Builder->CreateGlobalString("%c", "format_char"));
                else
                    ArgsV.push_back(context->Builder->CreateGlobalString("%d", "format_int"));
            }

            ArgsV.push_back(argValue);
        }
        else if (const auto ptrType = std::dynamic_pointer_cast<PointerType>(type))
        {
            if (*(ptrType->pointerBase) == *(VariableType::getCharacter()))
            {
                ArgsV.push_back(context->Builder->CreateGlobalString("%s", "format_string"));
                ArgsV.push_back(argValue);
            }
            else
            {
                assert(false && "type can not be printed");
            }
        }
        else if (type->baseType == VariableBaseType::Character)
        {
            ArgsV.push_back(context->Builder->CreateGlobalString("%c", "format_char"));
            ArgsV.push_back(argValue);
        }
        else
        {
            assert(false && "type can not be printed");
        }
        context->Builder->CreateCall(fprintf, ArgsV);

        // size_t fwrite( const void* buffer, size_t size, size_t count,
        // FILE* stream );
    }
    return nullptr;
}
llvm::Value *SystemFunctionCallNode::codegen_writeln(std::unique_ptr<Context> &context, ASTNode *parent) const
{
    codegen_write(context, parent);

    llvm::Function *fprintf = context->TheModule->getFunction("fprintf");

    llvm::Value *loadedStdOut = find_target_fileout(context, parent);
    if (!fprintf)
        LogErrorV("the function fprintf was not found");
    std::vector<llvm::Value *> ArgsV;
    ArgsV.push_back(loadedStdOut);
    if (context->TargetTriple->getOS() == llvm::Triple::Win32)
    {
        ArgsV.push_back(context->Builder->CreateGlobalString("\r\n"));
    }
    else
    {
        ArgsV.push_back(context->Builder->CreateGlobalString("\n"));
    }
    context->Builder->CreateCall(fprintf, ArgsV);

    return nullptr;
}
llvm::Value *SystemFunctionCallNode::codegen_assert(std::unique_ptr<Context> &context, ASTNode *parent,
                                                    ASTNode *argument, llvm::Value *expression,
                                                    const std::string &assertation)
{
    const auto callingFunctionName = parent->expressionToken().lexical();
    const auto condition = context->Builder->CreateNot(expression);
    std::string assertFunction;
    if (context->TargetTriple->getOS() == llvm::Triple::Linux)
    {
        assertFunction = "__assert_fail";
    }
    else if (context->TargetTriple->getOS() == llvm::Triple::Win32)
    {
        assertFunction = "_assert";
    }
    else
    {
        assert(false && "assert is not supported");
    }

    codegen::codegen_ifexpr(
            context, condition,
            [argument, callingFunctionName, assertation, assertFunction](const std::unique_ptr<Context> &ctx)
            {
                const auto assertCall = ctx->TheModule->getFunction(assertFunction);
                std::vector<llvm::Value *> ArgsV;
                const auto token = argument->expressionToken();
                ArgsV.push_back(ctx->Builder->CreateGlobalString(assertation, "assertion"));
                ArgsV.push_back(
                        ctx->Builder->CreateGlobalString(token.sourceLocation.filename, "assertion_source_file"));
                ArgsV.push_back(ctx->Builder->getInt32(token.row));
                ArgsV.push_back(ctx->Builder->CreateGlobalString(callingFunctionName, "assertion_function"));
                ctx->Builder->CreateCall(assertCall, ArgsV);
            });
    return nullptr;
}
llvm::Value *SystemFunctionCallNode::codegen_new(std::unique_ptr<Context> &context, ASTNode *parent) const
{
    m_args[0]->codegen(context);
    const auto type = m_args[0]->resolveType(context->ProgramUnit, parent);
    if (const auto ptrType = std::dynamic_pointer_cast<PointerType>(type))
        return context->Builder->CreateAlloca(ptrType->pointerBase->generateLlvmType(context));

    return LogErrorV("argument is not a pointer type");
}
llvm::Value *SystemFunctionCallNode::codegen(std::unique_ptr<Context> &context)
{
    ASTNode *parent = context->ProgramUnit.get();
    if (context->TopLevelFunction)
    {
        if (const auto def =
                    context->ProgramUnit->getFunctionDefinition(std::string(context->TopLevelFunction->getName())))
        {
            parent = def.value().get();
        }
    }

    if (iequals(m_name, "low"))
    {

        const auto paramType = m_args[0]->resolveType(context->ProgramUnit, parent);
        if (const auto arrayType = std::dynamic_pointer_cast<ArrayType>(paramType))
        {
            if (arrayType->isDynArray)
            {
                return context->Builder->getInt64(0);
            }
            return context->Builder->getInt64(arrayType->low);
        }
        if (const auto stringType = std::dynamic_pointer_cast<StringType>(paramType))

        {
            return context->Builder->getInt64(0);
        }
        if (const auto range = std::dynamic_pointer_cast<RangeType>(paramType))
        {
            return range->generateLowerBounds(expressionToken(), context);
        }
    }
    else if (iequals(m_name, "high"))
    {
        const auto paramType = m_args[0]->resolveType(context->ProgramUnit, parent);
        if (const auto arrayType = std::dynamic_pointer_cast<ArrayType>(paramType))
        {
            if (arrayType->isDynArray)
            {
                return context->Builder->CreateSub(codegen_length(context, parent), context->Builder->getInt64(1));
            }
            return context->Builder->getInt64(arrayType->high);
        }
        if (const auto stringType = std::dynamic_pointer_cast<StringType>(paramType))
        {
            return stringType->generateHighValue(m_args[0]->expressionToken(), context);
        }
        if (const auto range = std::dynamic_pointer_cast<RangeType>(paramType))
        {
            return range->generateUpperBounds(expressionToken(), context);
        }
    }
    else if (iequals(m_name, "length"))
    {
        return codegen_length(context, parent);
    }
    else if (iequals(m_name, "setlength"))
    {
        return codegen_setlength(context, parent);
    }
    else if (iequals(m_name, "write"))
    {
        return codegen_write(context, parent);
    }
    else if (iequals(m_name, "writeln"))
    {

        return codegen_writeln(context, parent);
    }
    else if (iequals(m_name, "pchar"))
    {
        const auto stringStructPtr = m_args[0]->codegen(context);
        const auto type = m_args[0]->resolveType(context->ProgramUnit, parent);
        const auto arrayPointerOffset = context->Builder->CreateStructGEP(type->generateLlvmType(context),
                                                                          stringStructPtr, 2, "string.ptr.offset");
        return context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), arrayPointerOffset);
    }
    else if (iequals(m_name, "new"))
    {
        return codegen_new(context, parent);
    }
    else if (iequals(m_name, "halt"))
    {
        const auto argValue = m_args[0]->codegen(context);
        llvm::Function *CalleeF = context->TheModule->getFunction("exit");

        return context->Builder->CreateCall(CalleeF, argValue);
    }
    else if (iequals(m_name, "exit"))
    {
        context->expliciteReturn = true;
        context->BreakBlock.BlockUsed = true;
        if (m_args.empty())
        {
            return context->Builder->CreateRetVoid();
        }
        const auto argValue = m_args[0]->codegen(context);

        return context->Builder->CreateRet(argValue);
    }
    else if (iequals(m_name, "assert"))
    {
        return codegen_assert(context, parent, m_args[0].get(), m_args[0]->codegen(context),
                              m_args[0]->expressionToken().lexical());
    }
    else if (iequals(m_name, "ord"))
    {
        const auto argValue = m_args[0]->codegen(context);
        return argValue;
    }
    else if (iequals(m_name, "chr"))
    {
        const auto argValue = m_args[0]->codegen(context);
        return argValue;
    }
    else if (iequals(m_name, "strdispose"))
    {
        const auto argValue = m_args[0]->codegen(context);
        llvm::Function *CalleeF = context->TheModule->getFunction("free");

        return context->Builder->CreateCall(CalleeF, argValue);
    }

    return FunctionCallNode::codegen(context);
}


std::shared_ptr<VariableType> SystemFunctionCallNode::resolveType(const std::unique_ptr<UnitNode> &unitNode,
                                                                  ASTNode *parentNode)
{
    if (iequals(m_name, "low"))
    {
        return IntegerType::getInteger(64);
    }
    if (iequals(m_name, "high"))
    {
        return IntegerType::getInteger(64);
    }
    if (iequals(m_name, "length"))
    {
        return IntegerType::getInteger(64);
    }
    if (iequals(m_name, "setlength"))
    {
        return nullptr;
    }
    if (iequals(m_name, "pchar"))
    {
        return PointerType::getPointerTo(IntegerType::getCharacter());
    }
    if (iequals(m_name, "ord"))
    {
        return IntegerType::getInteger(32);
    }
    if (iequals(m_name, "chr"))
    {
        return IntegerType::getCharacter();
    }

    return nullptr;
}
