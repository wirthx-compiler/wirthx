#include "BlockNode.h"
#include <iostream>

#include "compare.h"
#include "compiler/Context.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"


BlockNode::BlockNode(const Token &token, const std::vector<VariableDefinition> &variableDefinitions,
                     const std::vector<std::shared_ptr<ASTNode>> &expressions) :
    ASTNode(token), m_expressions(expressions), m_variableDefinitions(variableDefinitions)
{
}

void BlockNode::print()
{
    if (!m_variableDefinitions.empty())
        std::cout << "var\n";
    for (auto &def: m_variableDefinitions)
    {
        std::cout << def.variableName << " : " << def.variableType->typeName << ";\n";
    }
    std::cout << "begin\n";
    for (auto exp: m_expressions)
    {
        exp->print();
    }

    std::cout << "end;\n";
}


void BlockNode::setBlockName(const std::string &name) { m_blockname = name; }
void BlockNode::codegenConstantDefinitions(std::unique_ptr<Context> &context)
{
    for (auto &def: m_variableDefinitions)
    {
        if (def.constant)
        {
            context->NamedValues[def.variableName] = def.generateCodeForConstant(context);
        }
    }
}

llvm::Value *BlockNode::codegen(std::unique_ptr<Context> &context)
{
    if (!m_blockname.empty())
    {

        // Create a new basic block to start insertion into.
        llvm::BasicBlock *BB = llvm::BasicBlock::Create(*context->TheContext, m_blockname, context->TopLevelFunction);

        context->Builder->SetInsertPoint(BB);
    }
    codegenConstantDefinitions(context);

    for (auto &def: m_variableDefinitions)
    {
        if (!def.constant)

        {
            context->NamedAllocations[def.variableName] = def.generateCode(context);
            if (!def.alias.empty())
            {
                context->NamedAllocations[def.alias] = context->NamedAllocations[def.variableName];
            }
            if (def.value)
            {

                auto result = def.value->codegen(context);
                const auto type = def.variableType->generateLlvmType(context);
                if (result->getType()->isIntegerTy())
                {
                    result = context->Builder->CreateIntCast(result, type, true);
                }
                else if (type->isIEEELikeFPTy())
                {
                    result = context->Builder->CreateFPCast(result, type);
                }
                if (type->isStructTy() && result->getType()->isPointerTy())
                {
                    auto llvmArgType = type;

                    auto memcpyCall =
                            llvm::Intrinsic::getDeclaration(context->TheModule.get(), llvm::Intrinsic::memcpy,
                                                            {context->Builder->getPtrTy(), context->Builder->getPtrTy(),
                                                             context->Builder->getInt64Ty()});
                    std::vector<llvm::Value *> memcopyArgs;

                    const llvm::DataLayout &DL = context->TheModule->getDataLayout();
                    uint64_t structSize = DL.getTypeAllocSize(llvmArgType);


                    memcopyArgs.push_back(context->Builder->CreateBitCast(context->NamedAllocations[def.variableName],
                                                                          context->Builder->getPtrTy()));
                    memcopyArgs.push_back(context->Builder->CreateBitCast(result, context->Builder->getPtrTy()));
                    memcopyArgs.push_back(context->Builder->getInt64(structSize));
                    memcopyArgs.push_back(context->Builder->getFalse());

                    context->Builder->CreateCall(memcpyCall, memcopyArgs);
                }
                else
                {
                    context->Builder->CreateStore(result, context->NamedAllocations[def.variableName]);
                }
            }
        }
    }
    std::vector<llvm::Value *> values;

    for (const auto &exp: m_expressions)
    {
        values.push_back(exp->codegen(context));
    }

    auto topLevelFunctionName = (context->TopLevelFunction) ? context->TopLevelFunction->getName().str() : "";
    if (context->TopLevelFunction)
        topLevelFunctionName = topLevelFunctionName.substr(0, topLevelFunctionName.find('('));
    for (auto &def: m_variableDefinitions)
    {

        if (def.variableType->baseType == VariableBaseType::String)
        {
            // auto stringStructPtr = context->NamedAllocations[def.variableName];
            // auto type = def.variableType;
            // const auto arrayPointerOffset = context->Builder->CreateStructGEP(type->generateLlvmType(context),
            //                                                                   stringStructPtr, 2,
            //                                                                   "string.ptr.offset");
            // auto strValuePtr = context->Builder->CreateLoad(llvm::PointerType::getUnqual(*context->TheContext),
            //                                                 arrayPointerOffset);

            // context->Builder->CreateCall(context->TheModule->getFunction("free"), strValuePtr);
        }

        if (!context->TopLevelFunction || !iequals(def.variableName, topLevelFunctionName))
        {
            context->NamedValues.erase(def.variableName);
            context->NamedAllocations.erase(def.variableName);
        }
    }
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*context->TheContext));
}

std::optional<VariableDefinition> BlockNode::getVariableDefinition(const std::string &name) const
{
    for (const auto &def: m_variableDefinitions)
    {
        if (iequals(def.variableName, name) or iequals(def.alias, name))
        {
            return def;
        }
    }
    for (auto &node: m_expressions)
    {
        const auto block = node->block();
        if (!block)
        {
            continue;
        }
        if (const auto &b = std::dynamic_pointer_cast<BlockNode>(block.value()))
        {
            if (auto result = b->getVariableDefinition(name))
            {
                return result;
            }
        }
    }
    return std::nullopt;
}

void BlockNode::addVariableDefinition(VariableDefinition definition) { m_variableDefinitions.emplace_back(definition); }


void BlockNode::appendExpression(const std::shared_ptr<ASTNode> &node) { m_expressions.push_back(node); }
void BlockNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    for (const auto &exp: m_expressions)
    {
        exp->typeCheck(unit, parentNode);
    }
}
std::vector<VariableDefinition> BlockNode::getVariableDefinitions() { return m_variableDefinitions; }
std::optional<std::shared_ptr<ASTNode>> BlockNode::getNodeByToken(const Token &token) const
{
    for (const auto &node: m_expressions)
    {
        if (node->tokenIsPartOfNode(token))
        {
            return node;
        }
        if (const auto block = std::dynamic_pointer_cast<BlockNode>(node))
        {

            if (auto result = block->getNodeByToken(token))
            {
                return result;
            }
        }
    }
    return std::nullopt;
}

void BlockNode::preappendExpression(std::shared_ptr<ASTNode> node)
{
    m_expressions.emplace(m_expressions.begin(), node);
}
