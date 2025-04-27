#include "FunctionDefinitionNode.h"
#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include <utility>

#include "FieldAccessNode.h"
#include "compare.h"
#include "compiler/Context.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Verifier.h"
#include "types/RecordType.h"


FunctionDefinitionNode::FunctionDefinitionNode(const Token &token, std::string name,
                                               std::vector<FunctionArgument> params, std::shared_ptr<BlockNode> body,
                                               const bool isProcedure, std::shared_ptr<VariableType> returnType) :
    ASTNode(token), m_name(std::move(name)), m_externalName(m_name), m_params(std::move(params)),
    m_body(std::move(body)), m_isProcedure(isProcedure), m_returnType(std::move(returnType))
{
}

FunctionDefinitionNode::FunctionDefinitionNode(const Token &token, std::string name, std::string externalName,
                                               std::string libName, std::vector<FunctionArgument> params,
                                               const bool isProcedure, std::shared_ptr<VariableType> returnType) :
    ASTNode(token), m_name(std::move(name)), m_externalName(std::move(externalName)), m_libName(std::move(libName)),
    m_params(std::move(params)), m_body(nullptr), m_isProcedure(isProcedure), m_returnType(std::move(returnType))
{
}

void FunctionDefinitionNode::print()
{
    if (m_isProcedure)
        std::cout << "procedure " + m_name + "(";
    else
        std::cout << "function " + m_name + "(";
    for (size_t i = 0; i < m_params.size(); ++i)
    {
        auto &param = m_params[i];
        if (param.isReference)
        {
            std::cout << "var ";
        }
        std::cout << param.argumentName + " :" + param.type->typeName;

        if (i != m_params.size() - 1)
        {
            std::cout << ",";
        }
    }
    std::cout << ")";
    if (m_isProcedure)
    {
        std::cout << "\n";
    }
    else
    {
        std::cout << ": " << m_returnType->typeName << ";\n";
    }
    m_body->print();

    // std::cout << "end;\n";
}

std::string &FunctionDefinitionNode::name() { return m_name; }

std::shared_ptr<VariableType> FunctionDefinitionNode::returnType() { return m_returnType; }

llvm::Value *FunctionDefinitionNode::codegen(std::unique_ptr<Context> &context)
{
    std::vector<llvm::Type *> params;

    for (auto &param: m_params)
    {

        if (param.isReference || param.type->baseType == VariableBaseType::Struct ||
            param.type->baseType == VariableBaseType::String)
        {

            auto ptr = llvm::PointerType::getUnqual(param.type->generateLlvmType(context));
            params.push_back(ptr);
        }
        else
        {
            params.push_back(param.type->generateLlvmType(context));
        }
    }
    llvm::Type *resultType;
    if (m_isProcedure)
    {
        resultType = llvm::Type::getVoidTy(*context->TheContext);
    }
    else
    {
        resultType = m_returnType->generateLlvmType(context);
    }
    llvm::FunctionType *FT = llvm::FunctionType::get(resultType, params, false);

    llvm::Function *functionDefinition =
            llvm::Function::Create(FT, llvm::Function::ExternalLinkage, functionSignature(), context->TheModule.get());
    if (m_libName.empty())
    {
        functionDefinition->setDSOLocal(true);
        functionDefinition->addFnAttr(llvm::Attribute::MustProgress);
        if (!m_isProcedure && m_returnType->baseType == VariableBaseType::String)
            functionDefinition->addFnAttr(llvm::Attribute::NoFree);
        llvm::AttrBuilder b(*context->TheContext);
        b.addAttribute("frame-pointer", "all");
        functionDefinition->addFnAttrs(b);
    }
    for (const auto attribute: m_attributes)
    {
        switch (attribute)
        {
            case FunctionAttribute::Inline:
                functionDefinition->addFnAttr(llvm::Attribute::AlwaysInline);
                break;
        }
    }

    // Set names for all arguments.
    unsigned idx = 0;
    for (auto &arg: functionDefinition->args())
    {
        const auto param = m_params[idx];
        if (!param.isReference && param.type->baseType == VariableBaseType::Struct)
        {
            arg.addAttr(llvm::Attribute::getWithByValType(*context->TheContext, param.type->generateLlvmType(context)));
            arg.addAttr(llvm::Attribute::NoUndef);
        }


        arg.setName(param.argumentName);
        idx++;
    }
    context->FunctionDefinitions[functionSignature()] = functionDefinition;
    // Create a new basic block to start insertion into.

    context->TopLevelFunction = functionDefinition;
    if (m_body)
    {
        context->expliciteReturn = false;
        m_body->setBlockName(m_name + "_block");
        m_body->codegen(context);
        if (m_isProcedure)
        {
            context->Builder->CreateRetVoid();

            if (verifyFunction(*functionDefinition, &llvm::errs()))
            {
                if (context->compilerOptions.buildMode == BuildMode::Release)
                {
                    context->TheFPM->run(*functionDefinition, *context->TheFAM);
                }
            }

            return functionDefinition;
        }
        if (!context->expliciteReturn)
        {
            context->Builder->CreateRet(context->Builder->CreateLoad(resultType, context->NamedAllocations[m_name]));
        }

        // Finish off the function.

        // Validate the generated code, checking for consistency.
        if (!llvm::verifyFunction(*functionDefinition, &llvm::errs()))
        {
            if (context->compilerOptions.buildMode == BuildMode::Release)
            {
                context->TheFPM->run(*functionDefinition, *context->TheFAM);
            }
        }
    }


    return functionDefinition;
}
void FunctionDefinitionNode::typeCheck(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    if (m_body)
        m_body->typeCheck(unit, this);
}
void FunctionDefinitionNode::addAttribute(FunctionAttribute attribute) { m_attributes.emplace_back(attribute); }

std::optional<FunctionArgument> FunctionDefinitionNode::getParam(const std::string &paramName)
{
    for (auto &param: m_params)
    {
        if (iequals(param.argumentName, paramName))
        {
            return param;
        }
    }
    return std::nullopt;
}

std::optional<FunctionArgument> FunctionDefinitionNode::getParam(const size_t index)
{
    if (m_params.size() > index)
    {
        return m_params[index];
    }
    return std::nullopt;
}

std::shared_ptr<BlockNode> FunctionDefinitionNode::body() { return m_body; }


std::string FunctionDefinitionNode::functionSignature()
{
    if (!m_libName.empty())
        return m_externalName;

    if (m_functionSignature.empty())
    {
        std::stringstream stream;
        stream << to_lower(m_name) << "(";
        for (size_t i = 0; i < m_params.size(); ++i)
        {
            stream << m_params[i].type->typeName << ((i < m_params.size() - 1) ? "," : "");
        }
        stream << ")";
        m_functionSignature = stream.str();
    }
    return m_functionSignature;
}

std::string &FunctionDefinitionNode::externalName() { return m_externalName; }

std::string &FunctionDefinitionNode::libName() { return m_libName; }
