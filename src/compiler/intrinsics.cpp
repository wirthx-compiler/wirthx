#include "intrinsics.h"

#include <llvm/IR/IRBuilder.h>

#include "ast/types/FileType.h"
#include "ast/types/StringType.h"
#include "codegen.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"


#include <bitset>

#include "ast/UnitNode.h"
void createSystemCall(std::unique_ptr<Context> &context, const std::string &functionName,
                      const std::vector<FunctionArgument> &functionParams,
                      const std::shared_ptr<VariableType> &returnType)
{
    if (llvm::Function *F = context->functionDefinition(functionName); F == nullptr)
    {
        std::vector<llvm::Type *> params;
        for (const auto &param: functionParams)
        {
            params.push_back(param.type->generateLlvmType(context));
        }
        llvm::Type *resultType = llvm::Type::getVoidTy(*context->context());
        if (returnType)
        {
            resultType = returnType->generateLlvmType(context);
        }

        llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);

        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, functionName, context->module().get());

        // Set names for all arguments.
        unsigned idx = 0;
        for (auto &arg: F->args())
            arg.setName(functionParams[idx++].argumentName);
    }
}

void createReAllocCall(const std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    params.push_back(llvm::PointerType::getUnqual(*context->context()));
    params.push_back(llvm::Type::getInt64Ty(*context->context()));

    llvm::Type *resultType = llvm::PointerType::getUnqual(*context->context());
    llvm::FunctionType *functionType = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, "realloc", context->module().get());
    F->setMemoryEffects(llvm::MemoryEffects::argMemOnly());
    F->addFnAttr(llvm::Attribute::WillReturn);
    F->addFnAttr(llvm::Attribute::NoFree);

    F->getArg(0)->setName("ptr");
    F->getArg(1)->setName("new_size");
    // F->getArg(0)->addAttr(llvm::Attribute::NullPointerIsValid);
    F->getArg(0)->addAttr(llvm::Attribute::NoUndef);
    F->getArg(1)->addAttr(llvm::Attribute::NoUndef);
    // F->getArg(1)->addAttr(llvm::Attribute::AllocSize);
}


void createPrintfCall(const std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getInt32Ty(*context->context());
    llvm::FunctionType *functionType = llvm::FunctionType::get(resultType, params, true);
    llvm::Function *function =
            llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, "printf", context->module().get());
    for (auto &arg: function->args())
        arg.setName("__fmt");
}


void createFPrintfCall(const std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    params.push_back(llvm::PointerType::getUnqual(*context->context()));
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getInt32Ty(*context->context());
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, true);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "fprintf", context->module().get());
    F->getArg(0)->setName("file");
    F->getArg(1)->setName("format");
}

void createAssignCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = context->programUnit()->getTypeDefinitions().getType("file");
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->context()));
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->context());
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "assignfile(file,string)",
                                               context->module().get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->context(), "_block", F);
    context->builder()->SetInsertPoint(BB);

    //
    const auto stringStructPtr = F->getArg(1);
    const auto type = StringType::getString()->generateLlvmType(context);
    const auto valueType = IntegerType::getInteger(8)->generateLlvmType(context);
    const auto arrayPointerOffset = context->builder()->CreateStructGEP(type, stringStructPtr, 2, "string.ptr.offset");


    const auto fileName = context->builder()->CreateStructGEP(llvmFileType, F->getArg(0), 0, "file.name");
    const auto fileNameSize = context->builder()->CreateStructGEP(type, stringStructPtr, 1, "file.name.size");
    const auto loadedSize = context->builder()->CreateLoad(context->builder()->getInt64Ty(), fileNameSize, "size");


    const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);
    const auto allocSize = context->builder()->CreateMul(
            loadedSize, context->builder()->getInt64(valueType->getPrimitiveSizeInBits()));
    llvm::Value *allocatedNewFilename =
            context->builder()->CreateMalloc(indexType, //
                                             valueType, // Type of elements
                                             allocSize, // Number of elements
                                             nullptr // Optional array size multiplier (nullptr for scalar allocation)
            );
    const auto boundsLhs = context->builder()->CreateGEP(
            valueType, allocatedNewFilename, llvm::ArrayRef<llvm::Value *>{context->builder()->getInt64(0)}, "", false);

    const auto memcpyCall = llvm::Intrinsic::getDeclaration(
            context->module().get(), llvm::Intrinsic::memcpy,
            {context->builder()->getPtrTy(), context->builder()->getPtrTy(), context->builder()->getInt64Ty()});

    const auto loadedStringPtr =
            context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), arrayPointerOffset);
    std::vector<llvm::Value *> memcopyArgs;

    memcopyArgs.push_back(context->builder()->CreateBitCast(boundsLhs, context->builder()->getPtrTy()));
    memcopyArgs.push_back(loadedStringPtr);
    memcopyArgs.push_back(loadedSize);
    memcopyArgs.push_back(context->builder()->getFalse());
    context->builder()->CreateCall(memcpyCall, memcopyArgs);

    const auto bounds = context->builder()->CreateGEP(valueType, allocatedNewFilename,
                                                      llvm::ArrayRef<llvm::Value *>{loadedSize}, "", false);

    context->builder()->CreateStore(context->builder()->getInt8(0), bounds);
    context->builder()->CreateStore(allocatedNewFilename, fileName);

    context->builder()->CreateRetVoid();
}
void createResetCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = context->programUnit()->getTypeDefinitions().getType("file");
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->context());
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "reset(file)", context->module().get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->context(), "_block", F);
    context->builder()->SetInsertPoint(BB);

    llvm::Function *CalleeF = context->module()->getFunction("fopen");
    std::vector<llvm::Value *> argsV;
    //
    const auto fileNameOffset = context->builder()->CreateStructGEP(llvmFileType, F->getArg(0), 0, "file.name");
    const auto fileName =
            context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), fileNameOffset);

    //
    argsV.push_back(fileName);
    argsV.push_back(context->builder()->CreateGlobalStringPtr("r+"));
    const auto callResult = context->builder()->CreateCall(CalleeF, argsV);
    const auto resultPointer = context->builder()->CreatePointerCast(callResult, context->builder()->getInt64Ty());


    llvm::Value *condition =
            context->builder()->CreateCmp(llvm::CmpInst::ICMP_EQ, resultPointer, context->builder()->getInt64(0));
    codegen::codegen_ifexpr(context, condition,
                            [fileName](const std::unique_ptr<Context> &ctx)
                            {
                                {
                                    llvm::Function *printfCall = ctx->module()->getFunction("printf");

                                    std::vector<llvm::Value *> argsV = {};
                                    argsV.push_back(ctx->getOrCreateGlobalString("file with the name %s not found!"));
                                    argsV.push_back(fileName);
                                    ctx->builder()->CreateCall(printfCall, argsV);
                                }

                                llvm::Function *callExit = ctx->module()->getFunction("exit");
                                const std::vector<llvm::Value *> argsV = {ctx->builder()->getInt32(1)};

                                ctx->builder()->CreateCall(callExit, argsV);
                            });
    auto filePtr = context->builder()->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    context->builder()->CreateStore(callResult, filePtr);

    context->builder()->CreateRetVoid();
}
void createRewriteCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = context->programUnit()->getTypeDefinitions().getType("file");
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->context());
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "reset(file)", context->module().get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->context(), "_block", F);
    context->builder()->SetInsertPoint(BB);

    llvm::Function *CalleeF = context->module()->getFunction("fopen");
    std::vector<llvm::Value *> argsV;
    //
    const auto fileNameOffset = context->builder()->CreateStructGEP(llvmFileType, F->getArg(0), 0, "file.name");
    const auto fileName =
            context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), fileNameOffset);

    //
    argsV.push_back(fileName);
    argsV.push_back(context->builder()->CreateGlobalStringPtr("w+"));
    const auto callResult = context->builder()->CreateCall(CalleeF, argsV);
    const auto resultPointer = context->builder()->CreatePointerCast(callResult, context->builder()->getInt64Ty());


    llvm::Value *condition =
            context->builder()->CreateCmp(llvm::CmpInst::ICMP_EQ, resultPointer, context->builder()->getInt64(0));
    codegen::codegen_ifexpr(context, condition,
                            [fileName](const std::unique_ptr<Context> &ctx)
                            {
                                {
                                    llvm::Function *calleF = ctx->module()->getFunction("printf");

                                    std::vector<llvm::Value *> argsV = {};
                                    argsV.push_back(ctx->getOrCreateGlobalString("file with the name %s not found!"));
                                    argsV.push_back(fileName);
                                    ctx->builder()->CreateCall(calleF, argsV);
                                }

                                llvm::Function *callExit = ctx->module()->getFunction("exit");
                                const std::vector<llvm::Value *> argsV = {ctx->builder()->getInt32(1)};

                                ctx->builder()->CreateCall(callExit, argsV);
                            });
    const auto filePtr = context->builder()->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    context->builder()->CreateStore(callResult, filePtr);

    context->builder()->CreateRetVoid();
}
void createReadLnCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = context->programUnit()->getTypeDefinitions().getType("file");
    const auto llvmFileType = fileType->generateLlvmType(context);
    const auto llvmStringType = StringType::getString()->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->context()));
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->context());
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "readln(file,string)", context->module().get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->context(), "_block", F);
    context->builder()->SetInsertPoint(BB);
    F->getArg(0)->setName("file");
    F->getArg(1)->setName("value");
    //


    const auto filePtrOffset = context->builder()->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    const auto filePtr =
            context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), filePtrOffset);

    const auto stringPtr = context->builder()->CreateStructGEP(llvmStringType, F->getArg(1), 2, "string.ptr");
    const auto stringSizeOffset = context->builder()->CreateStructGEP(llvmStringType, F->getArg(1), 1, "string.size");


    const auto resultPointer = context->builder()->CreatePointerCast(filePtr, context->builder()->getInt64Ty());

    llvm::Value *condition =
            context->builder()->CreateCmp(llvm::CmpInst::ICMP_NE, resultPointer, context->builder()->getInt64(0));
    codegen::codegen_ifexpr(
            context, condition,
            [filePtr, stringPtr, stringSizeOffset, F](std::unique_ptr<Context> &ctx)
            {
                const auto valueType = VariableType::getInteger(8)->generateLlvmType(ctx);
                const auto size = ctx->builder()->CreateAlloca(ctx->builder()->getInt64Ty(), nullptr, "size");
                ctx->builder()->CreateStore(ctx->builder()->getInt64(0), size);
                const auto currentChar = ctx->builder()->CreateAlloca(valueType, nullptr, "currentChar");
                ctx->builder()->CreateStore(ctx->builder()->getInt8(0), currentChar);

                constexpr auto bufferSize = 256;
                const auto arrayBaseType = llvm::ArrayType::get(valueType, bufferSize);
                const auto buffer = ctx->builder()->CreateAlloca(arrayBaseType, nullptr, "buffer");
                // auto loadedBuffer = ctx->builder()->CreateLoad(llvm::ArrayType::get(valueType, bufferSize), buffer);
                {
                    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*ctx->context(), "loop", F);
                    llvm::BasicBlock *LoopCondBB = llvm::BasicBlock::Create(*ctx->context(), "loop.cond", F);

                    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*ctx->context(), "afterloop", F);

                    // Insert an explicit fall through from the current block to the LoopBB.
                    ctx->builder()->CreateBr(LoopCondBB);
                    ctx->builder()->SetInsertPoint(LoopCondBB);
                    // Create the "after loop" block and insert it.
                    // Compute the end condition.
                    const auto currCharValue = ctx->builder()->CreateLoad(valueType, currentChar);
                    const auto lineEndCondition = ctx->builder()->CreateCmp(llvm::CmpInst::ICMP_NE, currCharValue,
                                                                            ctx->builder()->getInt8(10));
                    llvm::Value *EndCond = lineEndCondition;


                    // Insert the conditional branch into the end of LoopEndBB.
                    ctx->builder()->CreateCondBr(EndCond, LoopBB, AfterBB);


                    // Start insertion in LoopBB.
                    ctx->builder()->SetInsertPoint(LoopBB);

                    // Emit the body of the loop.  This, like any other expr, can change the
                    // current BB.  Note that we ignore the value computed by the body, but don't
                    // allow an error.
                    const auto lastBreakBlock = ctx->breakBlock().Block;
                    ctx->breakBlock().Block = AfterBB;
                    ctx->breakBlock().BlockUsed = false;

                    ctx->builder()->SetInsertPoint(LoopBB);
                    llvm::Function *fgetc = ctx->module()->getFunction("fgetc");
                    std::vector<llvm::Value *> fgetcArgs;
                    fgetcArgs.push_back(filePtr);
                    const auto value = ctx->builder()->CreateCall(fgetc, fgetcArgs);
                    ctx->builder()->CreateStore(value, currentChar);

                    const auto loadedSize = ctx->builder()->CreateLoad(ctx->builder()->getInt64Ty(), size, "size");
                    const auto bounds = ctx->builder()->CreateGEP(
                            arrayBaseType, buffer,
                            llvm::ArrayRef<llvm::Value *>{ctx->builder()->getInt64(0),
                                                          dynamic_cast<llvm::Value *>(loadedSize)},
                            "", true);

                    ctx->builder()->CreateStore(value, bounds);

                    const auto tmp = ctx->builder()->CreateAdd(loadedSize, ctx->builder()->getInt64(1));

                    ctx->builder()->CreateStore(tmp, size);
                    ctx->builder()->CreateBr(LoopCondBB);

                    ctx->breakBlock().Block = lastBreakBlock;

                    // Any new code will be inserted in AfterBB.
                    ctx->builder()->SetInsertPoint(AfterBB);
                }


                const auto loadedSize = ctx->builder()->CreateLoad(ctx->builder()->getInt64Ty(), size, "size");

                const auto sizeWithoutNewline = ctx->builder()->CreateAdd(loadedSize, ctx->builder()->getInt64(-1));

                ctx->builder()->CreateStore(loadedSize, stringSizeOffset);
                const auto indexType = VariableType::getInteger(64)->generateLlvmType(ctx);
                const auto allocSize = ctx->builder()->CreateMul(
                        loadedSize, ctx->builder()->getInt64(valueType->getPrimitiveSizeInBits()));
                llvm::Value *allocCall = ctx->builder()->CreateMalloc(
                        indexType, //
                        valueType, // Type of elements
                        allocSize, // Number of elements
                        nullptr // Optional array size multiplier (nullptr for scalar allocation)
                );

                const auto boundsRHS = ctx->builder()->CreateGEP(
                        llvm::ArrayType::get(valueType, bufferSize), buffer,
                        llvm::ArrayRef<llvm::Value *>{ctx->builder()->getInt64(0),
                                                      dynamic_cast<llvm::Value *>(ctx->builder()->getInt64(0))},
                        "", false);
                const auto memcpyCall = llvm::Intrinsic::getDeclaration(
                        ctx->module().get(), llvm::Intrinsic::memcpy,
                        {ctx->builder()->getPtrTy(), ctx->builder()->getPtrTy(), ctx->builder()->getInt64Ty()});
                std::vector<llvm::Value *> memcopyArgs;

                memcopyArgs.push_back(ctx->builder()->CreateBitCast(allocCall, ctx->builder()->getPtrTy()));
                memcopyArgs.push_back(boundsRHS);
                memcopyArgs.push_back(sizeWithoutNewline);
                memcopyArgs.push_back(ctx->builder()->getFalse());
                ctx->builder()->CreateCall(memcpyCall, memcopyArgs);
                ctx->builder()->CreateStore(allocCall, stringPtr);

                const auto bounds =
                        ctx->builder()->CreateGEP(valueType, allocCall, llvm::ArrayRef{sizeWithoutNewline}, "", false);

                ctx->builder()->CreateStore(ctx->builder()->getInt8(0), bounds);
            });


    context->builder()->CreateRetVoid();
}
void createReadLnStdinCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = context->programUnit()->getTypeDefinitions().getType("file");
    const auto llvmFileType = fileType->generateLlvmType(context);
    const auto llvmStringType = StringType::getString()->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->context());
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "readln(string)", context->module().get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->context(), "_block", F);
    context->builder()->SetInsertPoint(BB);
    F->getArg(0)->setName("value");
    //


    const auto filePtrOffset =
            context->builder()->CreateStructGEP(llvmFileType, context->namedValue("stdin"), 1, "stdin");
    const auto filePtr =
            context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), filePtrOffset);

    const auto stringPtr = context->builder()->CreateStructGEP(llvmStringType, F->getArg(0), 2, "string.ptr");
    const auto stringSizeOffset = context->builder()->CreateStructGEP(llvmStringType, F->getArg(0), 1, "string.size");


    const auto resultPointer = context->builder()->CreatePointerCast(filePtr, context->builder()->getInt64Ty());

    llvm::Value *condition =
            context->builder()->CreateCmp(llvm::CmpInst::ICMP_NE, resultPointer, context->builder()->getInt64(0));
    codegen::codegen_ifexpr(
            context, condition,
            [filePtr, stringPtr, stringSizeOffset, F](std::unique_ptr<Context> &ctx)
            {
                const auto valueType = VariableType::getInteger(8)->generateLlvmType(ctx);
                const auto size = ctx->builder()->CreateAlloca(ctx->builder()->getInt64Ty(), nullptr, "size");
                ctx->builder()->CreateStore(ctx->builder()->getInt64(0), size);
                const auto currentChar = ctx->builder()->CreateAlloca(valueType, nullptr, "currentChar");
                ctx->builder()->CreateStore(ctx->builder()->getInt8(0), currentChar);

                constexpr auto bufferSize = 256;
                const auto arrayBaseType = llvm::ArrayType::get(valueType, bufferSize);
                const auto buffer = ctx->builder()->CreateAlloca(arrayBaseType, nullptr, "buffer");
                // auto loadedBuffer = ctx->builder()->CreateLoad(llvm::ArrayType::get(valueType, bufferSize), buffer);
                {
                    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*ctx->context(), "loop", F);
                    llvm::BasicBlock *LoopCondBB = llvm::BasicBlock::Create(*ctx->context(), "loop.cond", F);

                    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*ctx->context(), "afterloop", F);

                    // Insert an explicit fall through from the current block to the LoopBB.
                    ctx->builder()->CreateBr(LoopCondBB);
                    ctx->builder()->SetInsertPoint(LoopCondBB);
                    // Create the "after loop" block and insert it.
                    // Compute the end condition.
                    const auto currCharValue = ctx->builder()->CreateLoad(valueType, currentChar);
                    const auto lineEndCondition = ctx->builder()->CreateCmp(llvm::CmpInst::ICMP_NE, currCharValue,
                                                                            ctx->builder()->getInt8(10));
                    llvm::Value *EndCond = lineEndCondition;


                    // Insert the conditional branch into the end of LoopEndBB.
                    ctx->builder()->CreateCondBr(EndCond, LoopBB, AfterBB);


                    // Start insertion in LoopBB.
                    ctx->builder()->SetInsertPoint(LoopBB);

                    // Emit the body of the loop.  This, like any other expr, can change the
                    // current BB.  Note that we ignore the value computed by the body, but don't
                    // allow an error.
                    const auto lastBreakBlock = ctx->breakBlock().Block;
                    ctx->breakBlock().Block = AfterBB;
                    ctx->breakBlock().BlockUsed = false;

                    ctx->builder()->SetInsertPoint(LoopBB);
                    llvm::Function *fgetc = ctx->module()->getFunction("fgetc");
                    std::vector<llvm::Value *> fgetcArgs;
                    fgetcArgs.push_back(filePtr);
                    const auto value = ctx->builder()->CreateCall(fgetc, fgetcArgs);
                    ctx->builder()->CreateStore(value, currentChar);

                    const auto loadedSize = ctx->builder()->CreateLoad(ctx->builder()->getInt64Ty(), size, "size");
                    const auto bounds = ctx->builder()->CreateGEP(
                            arrayBaseType, buffer,
                            llvm::ArrayRef<llvm::Value *>{ctx->builder()->getInt64(0),
                                                          dynamic_cast<llvm::Value *>(loadedSize)},
                            "", true);

                    ctx->builder()->CreateStore(value, bounds);

                    const auto tmp = ctx->builder()->CreateAdd(loadedSize, ctx->builder()->getInt64(1));

                    ctx->builder()->CreateStore(tmp, size);
                    ctx->builder()->CreateBr(LoopCondBB);

                    ctx->breakBlock().Block = lastBreakBlock;

                    // Any new code will be inserted in AfterBB.
                    ctx->builder()->SetInsertPoint(AfterBB);
                }


                const auto loadedSize = ctx->builder()->CreateLoad(ctx->builder()->getInt64Ty(), size, "size");

                const auto sizeWithoutNewline = ctx->builder()->CreateAdd(loadedSize, ctx->builder()->getInt64(-1));

                ctx->builder()->CreateStore(loadedSize, stringSizeOffset);
                const auto indexType = VariableType::getInteger(64)->generateLlvmType(ctx);
                const auto allocSize = ctx->builder()->CreateMul(
                        loadedSize, ctx->builder()->getInt64(valueType->getPrimitiveSizeInBits()));
                llvm::Value *allocCall = ctx->builder()->CreateMalloc(
                        indexType, //
                        valueType, // Type of elements
                        allocSize, // Number of elements
                        nullptr // Optional array size multiplier (nullptr for scalar allocation)
                );

                const auto boundsRHS = ctx->builder()->CreateGEP(
                        llvm::ArrayType::get(valueType, bufferSize), buffer,
                        llvm::ArrayRef<llvm::Value *>{ctx->builder()->getInt64(0),
                                                      dynamic_cast<llvm::Value *>(ctx->builder()->getInt64(0))},
                        "", false);
                const auto memcpyCall = llvm::Intrinsic::getDeclaration(
                        ctx->module().get(), llvm::Intrinsic::memcpy,
                        {ctx->builder()->getPtrTy(), ctx->builder()->getPtrTy(), ctx->builder()->getInt64Ty()});
                std::vector<llvm::Value *> memcopyArgs;

                memcopyArgs.push_back(ctx->builder()->CreateBitCast(allocCall, ctx->builder()->getPtrTy()));
                memcopyArgs.push_back(boundsRHS);
                memcopyArgs.push_back(sizeWithoutNewline);
                memcopyArgs.push_back(ctx->builder()->getFalse());
                ctx->builder()->CreateCall(memcpyCall, memcopyArgs);
                ctx->builder()->CreateStore(allocCall, stringPtr);

                const auto bounds =
                        ctx->builder()->CreateGEP(valueType, allocCall, llvm::ArrayRef{sizeWithoutNewline}, "", false);

                ctx->builder()->CreateStore(ctx->builder()->getInt8(0), bounds);
            });


    // context->builder()->CreateStore(, filePtr);

    context->builder()->CreateRetVoid();
}
void createCloseFileCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;

    const auto fileType = context->programUnit()->getTypeDefinitions().getType("file");
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->context()));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->context());
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "closefile(file)", context->module().get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->context(), "_block", F);
    context->builder()->SetInsertPoint(BB);
    //


    const auto filePtrOffset = context->builder()->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    const auto filePtr =
            context->builder()->CreateLoad(llvm::PointerType::getUnqual(*context->context()), filePtrOffset);
    const auto resultPointer = context->builder()->CreatePointerCast(filePtr, context->builder()->getInt64Ty());

    llvm::Value *condition =
            context->builder()->CreateCmp(llvm::CmpInst::ICMP_NE, resultPointer, context->builder()->getInt64(0));
    codegen::codegen_ifexpr(context, condition,
                            [filePtr](const std::unique_ptr<Context> &ctx)
                            {
                                llvm::Function *CalleeF = ctx->module()->getFunction("fclose");
                                const std::vector<llvm::Value *> argsV = {filePtr};
                                ctx->builder()->CreateCall(CalleeF, argsV);
                            });


    // context->builder()->CreateStore(, filePtr);

    context->builder()->CreateRetVoid();
}
