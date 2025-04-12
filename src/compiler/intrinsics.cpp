#include "intrinsics.h"

#include <llvm/IR/IRBuilder.h>

#include "ast/types/FileType.h"
#include "ast/types/StringType.h"
#include "codegen.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Verifier.h"


#include <bitset>
void createSystemCall(std::unique_ptr<Context> &context, const std::string &functionName,
                      const std::vector<FunctionArgument> &functionParams,
                      const std::shared_ptr<VariableType> &returnType)
{
    if (llvm::Function *F = context->FunctionDefinitions[functionName]; F == nullptr)
    {
        std::vector<llvm::Type *> params;
        for (const auto &param: functionParams)
        {
            params.push_back(param.type->generateLlvmType(context));
        }
        llvm::Type *resultType = llvm::Type::getVoidTy(*context->TheContext);
        if (returnType)
        {
            resultType = returnType->generateLlvmType(context);
        }

        llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);

        F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, functionName, context->TheModule.get());

        // Set names for all arguments.
        unsigned idx = 0;
        for (auto &arg: F->args())
            arg.setName(functionParams[idx++].argumentName);
    }
}

void createPrintfCall(const std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getInt32Ty(*context->TheContext);
    llvm::FunctionType *functionType = llvm::FunctionType::get(resultType, params, true);
    llvm::Function *function =
            llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, "printf", context->TheModule.get());
    for (auto &arg: function->args())
        arg.setName("__fmt");
}


void createFPrintfCall(const std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getInt32Ty(*context->TheContext);
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, true);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "fprintf", context->TheModule.get());
    F->getArg(0)->setName("file");
    F->getArg(1)->setName("format");
}

void createAssignCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = FileType::getFileType();
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->TheContext);
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "assignfile(file,string)",
                                               context->TheModule.get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->TheContext, "_block", F);
    context->Builder->SetInsertPoint(BB);

    //
    const auto stringStructPtr = F->getArg(1);
    const auto type = StringType::getString()->generateLlvmType(context);
    const auto valueType = IntegerType::getInteger(8)->generateLlvmType(context);
    const auto arrayPointerOffset = context->Builder->CreateStructGEP(type, stringStructPtr, 2, "string.ptr.offset");


    const auto fileName = context->Builder->CreateStructGEP(llvmFileType, F->getArg(0), 0, "file.name");
    const auto fileNameSize = context->Builder->CreateStructGEP(type, stringStructPtr, 1, "file.name.size");
    const auto loadedSize = context->Builder->CreateLoad(context->Builder->getInt64Ty(), fileNameSize, "size");


    const auto indexType = VariableType::getInteger(64)->generateLlvmType(context);
    const auto allocSize =
            context->Builder->CreateMul(loadedSize, context->Builder->getInt64(valueType->getPrimitiveSizeInBits()));
    llvm::Value *allocatedNewFilename =
            context->Builder->CreateMalloc(indexType, //
                                           valueType, // Type of elements
                                           allocSize, // Number of elements
                                           nullptr // Optional array size multiplier (nullptr for scalar allocation)
            );
    const auto boundsLhs = context->Builder->CreateGEP(
            valueType, allocatedNewFilename, llvm::ArrayRef<llvm::Value *>{context->Builder->getInt64(0)}, "", false);

    const auto memcpyCall = llvm::Intrinsic::getDeclaration(
            context->TheModule.get(), llvm::Intrinsic::memcpy,
            {context->Builder->getPtrTy(), context->Builder->getPtrTy(), context->Builder->getInt64Ty()});

    const auto loadedStringPtr =
            context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), arrayPointerOffset);
    std::vector<llvm::Value *> memcopyArgs;

    memcopyArgs.push_back(context->Builder->CreateBitCast(boundsLhs, context->Builder->getPtrTy()));
    memcopyArgs.push_back(loadedStringPtr);
    memcopyArgs.push_back(loadedSize);
    memcopyArgs.push_back(context->Builder->getFalse());
    context->Builder->CreateCall(memcpyCall, memcopyArgs);

    const auto bounds = context->Builder->CreateGEP(valueType, allocatedNewFilename,
                                                    llvm::ArrayRef<llvm::Value *>{loadedSize}, "", false);

    context->Builder->CreateStore(context->Builder->getInt8(0), bounds);
    context->Builder->CreateStore(allocatedNewFilename, fileName);

    context->Builder->CreateRetVoid();
}
void createResetCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = FileType::getFileType();
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->TheContext);
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "reset(file)", context->TheModule.get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->TheContext, "_block", F);
    context->Builder->SetInsertPoint(BB);

    llvm::Function *CalleeF = context->TheModule->getFunction("fopen");
    std::vector<llvm::Value *> argsV;
    //
    const auto fileNameOffset = context->Builder->CreateStructGEP(llvmFileType, F->getArg(0), 0, "file.name");
    const auto fileName =
            context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), fileNameOffset);

    //
    argsV.push_back(fileName);
    argsV.push_back(context->Builder->CreateGlobalStringPtr("r+"));
    const auto callResult = context->Builder->CreateCall(CalleeF, argsV);
    const auto resultPointer = context->Builder->CreatePointerCast(callResult, context->Builder->getInt64Ty());


    llvm::Value *condition =
            context->Builder->CreateCmp(llvm::CmpInst::ICMP_EQ, resultPointer, context->Builder->getInt64(0));
    codegen::codegen_ifexpr(context, condition,
                            [fileName](const std::unique_ptr<Context> &ctx)
                            {
                                {
                                    llvm::Function *printfCall = ctx->TheModule->getFunction("printf");

                                    std::vector<llvm::Value *> argsV = {};
                                    argsV.push_back(ctx->Builder->CreateGlobalString("file with the name %s not found!",
                                                                                     "format_string"));
                                    argsV.push_back(fileName);
                                    ctx->Builder->CreateCall(printfCall, argsV);
                                }

                                llvm::Function *callExit = ctx->TheModule->getFunction("exit");
                                const std::vector<llvm::Value *> argsV = {ctx->Builder->getInt32(1)};

                                ctx->Builder->CreateCall(callExit, argsV);
                            });
    auto filePtr = context->Builder->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    context->Builder->CreateStore(callResult, filePtr);

    context->Builder->CreateRetVoid();
}
void createRewriteCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = FileType::getFileType();
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->TheContext);
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "reset(file)", context->TheModule.get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->TheContext, "_block", F);
    context->Builder->SetInsertPoint(BB);

    llvm::Function *CalleeF = context->TheModule->getFunction("fopen");
    std::vector<llvm::Value *> argsV;
    //
    const auto fileNameOffset = context->Builder->CreateStructGEP(llvmFileType, F->getArg(0), 0, "file.name");
    const auto fileName =
            context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), fileNameOffset);

    //
    argsV.push_back(fileName);
    argsV.push_back(context->Builder->CreateGlobalStringPtr("w+"));
    const auto callResult = context->Builder->CreateCall(CalleeF, argsV);
    const auto resultPointer = context->Builder->CreatePointerCast(callResult, context->Builder->getInt64Ty());


    llvm::Value *condition =
            context->Builder->CreateCmp(llvm::CmpInst::ICMP_EQ, resultPointer, context->Builder->getInt64(0));
    codegen::codegen_ifexpr(context, condition,
                            [fileName](const std::unique_ptr<Context> &ctx)
                            {
                                {
                                    llvm::Function *calleF = ctx->TheModule->getFunction("printf");

                                    std::vector<llvm::Value *> argsV = {};
                                    argsV.push_back(ctx->Builder->CreateGlobalString("file with the name %s not found!",
                                                                                     "format_string"));
                                    argsV.push_back(fileName);
                                    ctx->Builder->CreateCall(calleF, argsV);
                                }

                                llvm::Function *callExit = ctx->TheModule->getFunction("exit");
                                const std::vector<llvm::Value *> argsV = {ctx->Builder->getInt32(1)};

                                ctx->Builder->CreateCall(callExit, argsV);
                            });
    const auto filePtr = context->Builder->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    context->Builder->CreateStore(callResult, filePtr);

    context->Builder->CreateRetVoid();
}
void createReadLnCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = FileType::getFileType();
    const auto llvmFileType = fileType->generateLlvmType(context);
    const auto llvmStringType = StringType::getString()->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->TheContext);
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "readln(file,string)", context->TheModule.get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->TheContext, "_block", F);
    context->Builder->SetInsertPoint(BB);
    F->getArg(0)->setName("file");
    F->getArg(1)->setName("value");
    //


    const auto filePtrOffset = context->Builder->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    const auto filePtr =
            context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), filePtrOffset);

    const auto stringPtr = context->Builder->CreateStructGEP(llvmStringType, F->getArg(1), 2, "string.ptr");
    const auto stringSizeOffset = context->Builder->CreateStructGEP(llvmStringType, F->getArg(1), 1, "string.size");


    const auto resultPointer = context->Builder->CreatePointerCast(filePtr, context->Builder->getInt64Ty());

    llvm::Value *condition =
            context->Builder->CreateCmp(llvm::CmpInst::ICMP_NE, resultPointer, context->Builder->getInt64(0));
    codegen::codegen_ifexpr(
            context, condition,
            [filePtr, stringPtr, stringSizeOffset, F](std::unique_ptr<Context> &ctx)
            {
                const auto valueType = VariableType::getInteger(8)->generateLlvmType(ctx);
                const auto size = ctx->Builder->CreateAlloca(ctx->Builder->getInt64Ty(), nullptr, "size");
                ctx->Builder->CreateStore(ctx->Builder->getInt64(0), size);
                const auto currentChar = ctx->Builder->CreateAlloca(valueType, nullptr, "currentChar");
                ctx->Builder->CreateStore(ctx->Builder->getInt8(0), currentChar);

                constexpr auto bufferSize = 256;
                const auto arrayBaseType = llvm::ArrayType::get(valueType, bufferSize);
                const auto buffer = ctx->Builder->CreateAlloca(arrayBaseType, nullptr, "buffer");
                // auto loadedBuffer = ctx->Builder->CreateLoad(llvm::ArrayType::get(valueType, bufferSize), buffer);
                {
                    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*ctx->TheContext, "loop", F);
                    llvm::BasicBlock *LoopCondBB = llvm::BasicBlock::Create(*ctx->TheContext, "loop.cond", F);

                    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*ctx->TheContext, "afterloop", F);

                    // Insert an explicit fall through from the current block to the LoopBB.
                    ctx->Builder->CreateBr(LoopCondBB);
                    ctx->Builder->SetInsertPoint(LoopCondBB);
                    // Create the "after loop" block and insert it.
                    // Compute the end condition.
                    const auto currCharValue = ctx->Builder->CreateLoad(valueType, currentChar);
                    const auto lineEndCondition =
                            ctx->Builder->CreateCmp(llvm::CmpInst::ICMP_NE, currCharValue, ctx->Builder->getInt8(10));
                    llvm::Value *EndCond = lineEndCondition;


                    // Insert the conditional branch into the end of LoopEndBB.
                    ctx->Builder->CreateCondBr(EndCond, LoopBB, AfterBB);


                    // Start insertion in LoopBB.
                    ctx->Builder->SetInsertPoint(LoopBB);

                    // Emit the body of the loop.  This, like any other expr, can change the
                    // current BB.  Note that we ignore the value computed by the body, but don't
                    // allow an error.
                    const auto lastBreakBlock = ctx->BreakBlock.Block;
                    ctx->BreakBlock.Block = AfterBB;
                    ctx->BreakBlock.BlockUsed = false;

                    ctx->Builder->SetInsertPoint(LoopBB);
                    llvm::Function *fgetc = ctx->TheModule->getFunction("fgetc");
                    std::vector<llvm::Value *> fgetcArgs;
                    fgetcArgs.push_back(filePtr);
                    const auto value = ctx->Builder->CreateCall(fgetc, fgetcArgs);
                    ctx->Builder->CreateStore(value, currentChar);

                    const auto loadedSize = ctx->Builder->CreateLoad(ctx->Builder->getInt64Ty(), size, "size");
                    const auto bounds = ctx->Builder->CreateGEP(
                            arrayBaseType, buffer,
                            llvm::ArrayRef<llvm::Value *>{ctx->Builder->getInt64(0),
                                                          dynamic_cast<llvm::Value *>(loadedSize)},
                            "", true);

                    ctx->Builder->CreateStore(value, bounds);

                    const auto tmp = ctx->Builder->CreateAdd(loadedSize, ctx->Builder->getInt64(1));

                    ctx->Builder->CreateStore(tmp, size);
                    ctx->Builder->CreateBr(LoopCondBB);

                    ctx->BreakBlock.Block = lastBreakBlock;

                    // Any new code will be inserted in AfterBB.
                    ctx->Builder->SetInsertPoint(AfterBB);
                }


                const auto loadedSize = ctx->Builder->CreateLoad(ctx->Builder->getInt64Ty(), size, "size");

                const auto sizeWithoutNewline = ctx->Builder->CreateAdd(loadedSize, ctx->Builder->getInt64(-1));

                ctx->Builder->CreateStore(loadedSize, stringSizeOffset);
                const auto indexType = VariableType::getInteger(64)->generateLlvmType(ctx);
                const auto allocSize = ctx->Builder->CreateMul(
                        loadedSize, ctx->Builder->getInt64(valueType->getPrimitiveSizeInBits()));
                llvm::Value *allocCall = ctx->Builder->CreateMalloc(
                        indexType, //
                        valueType, // Type of elements
                        allocSize, // Number of elements
                        nullptr // Optional array size multiplier (nullptr for scalar allocation)
                );

                const auto boundsRHS = ctx->Builder->CreateGEP(
                        llvm::ArrayType::get(valueType, bufferSize), buffer,
                        llvm::ArrayRef<llvm::Value *>{ctx->Builder->getInt64(0),
                                                      dynamic_cast<llvm::Value *>(ctx->Builder->getInt64(0))},
                        "", false);
                const auto memcpyCall = llvm::Intrinsic::getDeclaration(
                        ctx->TheModule.get(), llvm::Intrinsic::memcpy,
                        {ctx->Builder->getPtrTy(), ctx->Builder->getPtrTy(), ctx->Builder->getInt64Ty()});
                std::vector<llvm::Value *> memcopyArgs;

                memcopyArgs.push_back(ctx->Builder->CreateBitCast(allocCall, ctx->Builder->getPtrTy()));
                memcopyArgs.push_back(boundsRHS);
                memcopyArgs.push_back(sizeWithoutNewline);
                memcopyArgs.push_back(ctx->Builder->getFalse());
                ctx->Builder->CreateCall(memcpyCall, memcopyArgs);
                ctx->Builder->CreateStore(allocCall, stringPtr);

                const auto bounds =
                        ctx->Builder->CreateGEP(valueType, allocCall, llvm::ArrayRef{sizeWithoutNewline}, "", false);

                ctx->Builder->CreateStore(ctx->Builder->getInt8(0), bounds);
            });


    context->Builder->CreateRetVoid();
}
void createReadLnStdinCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;
    const auto fileType = FileType::getFileType();
    const auto llvmFileType = fileType->generateLlvmType(context);
    const auto llvmStringType = StringType::getString()->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->TheContext);
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "readln(string)", context->TheModule.get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->TheContext, "_block", F);
    context->Builder->SetInsertPoint(BB);
    F->getArg(0)->setName("value");
    //


    const auto filePtrOffset =
            context->Builder->CreateStructGEP(llvmFileType, context->NamedValues["stdin"], 1, "stdin");
    const auto filePtr =
            context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), filePtrOffset);

    const auto stringPtr = context->Builder->CreateStructGEP(llvmStringType, F->getArg(0), 2, "string.ptr");
    const auto stringSizeOffset = context->Builder->CreateStructGEP(llvmStringType, F->getArg(0), 1, "string.size");


    const auto resultPointer = context->Builder->CreatePointerCast(filePtr, context->Builder->getInt64Ty());

    llvm::Value *condition =
            context->Builder->CreateCmp(llvm::CmpInst::ICMP_NE, resultPointer, context->Builder->getInt64(0));
    codegen::codegen_ifexpr(
            context, condition,
            [filePtr, stringPtr, stringSizeOffset, F](std::unique_ptr<Context> &ctx)
            {
                const auto valueType = VariableType::getInteger(8)->generateLlvmType(ctx);
                const auto size = ctx->Builder->CreateAlloca(ctx->Builder->getInt64Ty(), nullptr, "size");
                ctx->Builder->CreateStore(ctx->Builder->getInt64(0), size);
                const auto currentChar = ctx->Builder->CreateAlloca(valueType, nullptr, "currentChar");
                ctx->Builder->CreateStore(ctx->Builder->getInt8(0), currentChar);

                constexpr auto bufferSize = 256;
                const auto arrayBaseType = llvm::ArrayType::get(valueType, bufferSize);
                const auto buffer = ctx->Builder->CreateAlloca(arrayBaseType, nullptr, "buffer");
                // auto loadedBuffer = ctx->Builder->CreateLoad(llvm::ArrayType::get(valueType, bufferSize), buffer);
                {
                    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*ctx->TheContext, "loop", F);
                    llvm::BasicBlock *LoopCondBB = llvm::BasicBlock::Create(*ctx->TheContext, "loop.cond", F);

                    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*ctx->TheContext, "afterloop", F);

                    // Insert an explicit fall through from the current block to the LoopBB.
                    ctx->Builder->CreateBr(LoopCondBB);
                    ctx->Builder->SetInsertPoint(LoopCondBB);
                    // Create the "after loop" block and insert it.
                    // Compute the end condition.
                    const auto currCharValue = ctx->Builder->CreateLoad(valueType, currentChar);
                    const auto lineEndCondition =
                            ctx->Builder->CreateCmp(llvm::CmpInst::ICMP_NE, currCharValue, ctx->Builder->getInt8(10));
                    llvm::Value *EndCond = lineEndCondition;


                    // Insert the conditional branch into the end of LoopEndBB.
                    ctx->Builder->CreateCondBr(EndCond, LoopBB, AfterBB);


                    // Start insertion in LoopBB.
                    ctx->Builder->SetInsertPoint(LoopBB);

                    // Emit the body of the loop.  This, like any other expr, can change the
                    // current BB.  Note that we ignore the value computed by the body, but don't
                    // allow an error.
                    const auto lastBreakBlock = ctx->BreakBlock.Block;
                    ctx->BreakBlock.Block = AfterBB;
                    ctx->BreakBlock.BlockUsed = false;

                    ctx->Builder->SetInsertPoint(LoopBB);
                    llvm::Function *fgetc = ctx->TheModule->getFunction("fgetc");
                    std::vector<llvm::Value *> fgetcArgs;
                    fgetcArgs.push_back(filePtr);
                    const auto value = ctx->Builder->CreateCall(fgetc, fgetcArgs);
                    ctx->Builder->CreateStore(value, currentChar);

                    const auto loadedSize = ctx->Builder->CreateLoad(ctx->Builder->getInt64Ty(), size, "size");
                    const auto bounds = ctx->Builder->CreateGEP(
                            arrayBaseType, buffer,
                            llvm::ArrayRef<llvm::Value *>{ctx->Builder->getInt64(0),
                                                          dynamic_cast<llvm::Value *>(loadedSize)},
                            "", true);

                    ctx->Builder->CreateStore(value, bounds);

                    const auto tmp = ctx->Builder->CreateAdd(loadedSize, ctx->Builder->getInt64(1));

                    ctx->Builder->CreateStore(tmp, size);
                    ctx->Builder->CreateBr(LoopCondBB);

                    ctx->BreakBlock.Block = lastBreakBlock;

                    // Any new code will be inserted in AfterBB.
                    ctx->Builder->SetInsertPoint(AfterBB);
                }


                const auto loadedSize = ctx->Builder->CreateLoad(ctx->Builder->getInt64Ty(), size, "size");

                const auto sizeWithoutNewline = ctx->Builder->CreateAdd(loadedSize, ctx->Builder->getInt64(-1));

                ctx->Builder->CreateStore(loadedSize, stringSizeOffset);
                const auto indexType = VariableType::getInteger(64)->generateLlvmType(ctx);
                const auto allocSize = ctx->Builder->CreateMul(
                        loadedSize, ctx->Builder->getInt64(valueType->getPrimitiveSizeInBits()));
                llvm::Value *allocCall = ctx->Builder->CreateMalloc(
                        indexType, //
                        valueType, // Type of elements
                        allocSize, // Number of elements
                        nullptr // Optional array size multiplier (nullptr for scalar allocation)
                );

                const auto boundsRHS = ctx->Builder->CreateGEP(
                        llvm::ArrayType::get(valueType, bufferSize), buffer,
                        llvm::ArrayRef<llvm::Value *>{ctx->Builder->getInt64(0),
                                                      dynamic_cast<llvm::Value *>(ctx->Builder->getInt64(0))},
                        "", false);
                const auto memcpyCall = llvm::Intrinsic::getDeclaration(
                        ctx->TheModule.get(), llvm::Intrinsic::memcpy,
                        {ctx->Builder->getPtrTy(), ctx->Builder->getPtrTy(), ctx->Builder->getInt64Ty()});
                std::vector<llvm::Value *> memcopyArgs;

                memcopyArgs.push_back(ctx->Builder->CreateBitCast(allocCall, ctx->Builder->getPtrTy()));
                memcopyArgs.push_back(boundsRHS);
                memcopyArgs.push_back(sizeWithoutNewline);
                memcopyArgs.push_back(ctx->Builder->getFalse());
                ctx->Builder->CreateCall(memcpyCall, memcopyArgs);
                ctx->Builder->CreateStore(allocCall, stringPtr);

                const auto bounds =
                        ctx->Builder->CreateGEP(valueType, allocCall, llvm::ArrayRef{sizeWithoutNewline}, "", false);

                ctx->Builder->CreateStore(ctx->Builder->getInt8(0), bounds);
            });


    // context->Builder->CreateStore(, filePtr);

    context->Builder->CreateRetVoid();
}
void createCloseFileCall(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;

    const auto fileType = FileType::getFileType();
    const auto llvmFileType = fileType->generateLlvmType(context);
    params.push_back(llvm::PointerType::getUnqual(*context->TheContext));

    llvm::Type *resultType = llvm::Type::getVoidTy(*context->TheContext);
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);
    llvm::Function *F =
            llvm::Function::Create(FT, llvm::Function::PrivateLinkage, "closefile(file)", context->TheModule.get());
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->TheContext, "_block", F);
    context->Builder->SetInsertPoint(BB);
    //


    const auto filePtrOffset = context->Builder->CreateStructGEP(llvmFileType, F->getArg(0), 1, "file.ptr");
    const auto filePtr =
            context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext), filePtrOffset);
    const auto resultPointer = context->Builder->CreatePointerCast(filePtr, context->Builder->getInt64Ty());

    llvm::Value *condition =
            context->Builder->CreateCmp(llvm::CmpInst::ICMP_NE, resultPointer, context->Builder->getInt64(0));
    codegen::codegen_ifexpr(context, condition,
                            [filePtr](const std::unique_ptr<Context> &ctx)
                            {
                                llvm::Function *CalleeF = ctx->TheModule->getFunction("fclose");
                                const std::vector<llvm::Value *> argsV = {filePtr};
                                ctx->Builder->CreateCall(CalleeF, argsV);
                            });


    // context->Builder->CreateStore(, filePtr);

    context->Builder->CreateRetVoid();
}
