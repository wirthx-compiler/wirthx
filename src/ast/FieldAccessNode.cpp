#include "FieldAccessNode.h"
#include "FunctionCallNode.h"
#include "FunctionDefinitionNode.h"
#include "UnitNode.h"
#include "compiler/Context.h"
#include "exceptions/CompilerException.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "types/RecordType.h"


FieldAccessNode::FieldAccessNode(const Token &element, const Token &field) :
    ASTNode(element), m_element(element), m_elementName(element.lexical()), m_field(field), m_fieldName(field.lexical())
{
}

void FieldAccessNode::print() {}

llvm::Value *FieldAccessNode::codegen(std::unique_ptr<Context> &context)
{
    llvm::AllocaInst *V = context->namedAllocation(m_elementName);

    const auto fieldName = m_elementName + "." + m_fieldName;
    if (!V)
    {
        for (size_t i = 0; i < context->currentFunction()->arg_size(); ++i)
        {
            auto arg = context->currentFunction()->getArg(static_cast<unsigned>(i));
            if (arg->getName() == m_elementName)
            {

                auto functionDefinition =
                        context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str());
                auto structDef = functionDefinition.value()->getParam(m_elementName);

                if (!structDef)
                {
                    return LogErrorV("Unknown struct with name: " + m_elementName);
                }

                auto recordType = std::dynamic_pointer_cast<RecordType>(structDef->type);
                auto llvmRecordType = llvm::cast<llvm::StructType>(recordType->generateLlvmType(context));

                auto index = recordType->getFieldIndexByName(m_fieldName);
                auto field = recordType->getField(index);
                llvm::Value *value = arg;
                if (arg->getType()->isStructTy())
                {
                    llvm::AllocaInst *alloca =
                            context->builder()->CreateAlloca(llvmRecordType, nullptr, m_elementName + "_ptr");
                    context->builder()->CreateStore(arg, alloca);

                    value = alloca;
                }
                auto fieldType = field.variableType->generateLlvmType(context);
                const llvm::DataLayout &DL = context->module()->getDataLayout();
                auto alignment = DL.getPrefTypeAlign(fieldType);


                auto arrayValue = context->builder()->CreateStructGEP(llvmRecordType, value, index, fieldName);
                // if (fieldType->isPointerTy())
                // {
                //     return arrayValue;
                // }
                return context->builder()->CreateAlignedLoad(fieldType, arrayValue, alignment, fieldName);
            }
        }
    }
    std::optional<VariableDefinition> structDef;
    if (context->currentFunction())
    {
        auto functionDefinition =
                context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str());
        if (functionDefinition)
            structDef = functionDefinition.value()->body()->getVariableDefinition(m_elementName);
    }

    if (!structDef)
    {
        structDef = context->programUnit()->getVariableDefinition(m_elementName);
    }
    if (!structDef)
    {
        return LogErrorV("Unknown record variable name");
    }
    else
    {
        auto recordType = std::dynamic_pointer_cast<RecordType>(structDef->variableType);

        auto index = recordType->getFieldIndexByName(m_fieldName);
        auto field = recordType->getField(index);


        auto arrayValue = context->builder()->CreateStructGEP(V->getAllocatedType(), V, index, fieldName);
        return context->builder()->CreateLoad(field.variableType->generateLlvmType(context), arrayValue);
    }
}

std::shared_ptr<VariableType> FieldAccessNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    if (auto functionDefinition = dynamic_cast<FunctionDefinitionNode *>(parentNode))
    {
        if (auto param = functionDefinition->getParam(m_elementName))
        {
            if (param->type->baseType == VariableBaseType::Struct)
            {
                auto type = std::dynamic_pointer_cast<RecordType>(param->type);

                auto field = type->getFieldByName(m_fieldName);

                if (field)
                {
                    return field.value().variableType;
                }
            }
        }
        if (auto variable = functionDefinition->body()->getVariableDefinition(m_elementName))
        {
            if (variable->variableType->baseType == VariableBaseType::Struct)
            {
                auto type = std::dynamic_pointer_cast<RecordType>(variable->variableType);

                if (auto field = type->getFieldByName(m_fieldName))
                {
                    return field.value().variableType;
                }
            }
        }
    }
    auto definition = unit->getVariableDefinition(m_elementName);

    if (definition->variableType->baseType == VariableBaseType::Struct)
    {
        auto type = std::dynamic_pointer_cast<RecordType>(definition->variableType);

        if (auto field = type->getFieldByName(m_fieldName))
        {
            return field.value().variableType;
        }
    }

    return std::make_shared<VariableType>();
}
