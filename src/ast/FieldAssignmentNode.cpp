#include "FieldAssignmentNode.h"
#include "FunctionCallNode.h"
#include "UnitNode.h"
#include "compiler/Context.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "types/RecordType.h"

FieldAssignmentNode::FieldAssignmentNode(const Token &variable, const Token &field,
                                         const std::shared_ptr<ASTNode> &expression) :
    ASTNode(variable), m_variable(variable), m_variableName(std::string(m_variable.lexical())), m_field(field),
    m_fieldName(std::string(m_field.lexical())), m_expression(expression)
{
}

void FieldAssignmentNode::print() {}

llvm::Value *FieldAssignmentNode::codegen(std::unique_ptr<Context> &context)
{
    using namespace std::string_literals;
    llvm::AllocaInst *V = context->namedAllocation(m_variableName);

    const auto fieldName = m_variableName + "." + m_fieldName;

    if (!V)
    {
        for (size_t i = 0; i < context->currentFunction()->arg_size(); ++i)
        {
            auto arg = context->currentFunction()->getArg(static_cast<unsigned>(i));
            if (arg->getName() == m_variableName)
            {
                auto functionDefinition =
                        context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str());
                if (!functionDefinition.has_value())
                {
                    return LogErrorV("Cannot find function definition for " +
                                     context->currentFunction()->getName().str());
                }
                auto structDef = functionDefinition.value()->getParam(m_variableName);

                if (!structDef)
                {
                    return LogErrorV("Unknown variable name");
                }

                auto recordType = std::dynamic_pointer_cast<RecordType>(structDef->type);
                auto llvmRecordType = llvm::cast<llvm::StructType>(recordType->generateLlvmType(context));

                auto index = recordType->getFieldIndexByName(m_fieldName);
                auto field = recordType->getField(index);
                auto fieldType = field.variableType->generateLlvmType(context);
                auto result = m_expression->codegen(context);

                if (fieldType->isIntegerTy())
                {
                    auto bitLength = fieldType->getIntegerBitWidth();
                    if (result->getType()->isIntegerTy() && result->getType()->getIntegerBitWidth() != bitLength)
                    {
                        result = context->builder()->CreateIntCast(result, fieldType, true, "result_cast");
                    }
                }
                if (arg->getType()->isStructTy())
                {
                    llvm::AllocaInst *alloca =
                            context->builder()->CreateAlloca(llvmRecordType, nullptr, m_variableName + "_ptr");
                    context->builder()->CreateStore(arg, alloca);

                    auto arrayValue = context->builder()->CreateStructGEP(llvmRecordType, alloca, index, fieldName);
                    context->builder()->CreateStore(result, arrayValue);
                }
                else
                {
                    auto arrayValue = context->builder()->CreateStructGEP(llvmRecordType, arg, index, fieldName);


                    context->builder()->CreateStore(result, arrayValue);
                }
                return result;
            }
        }
    }
    if (!V)
        return LogErrorV("Unknown record variable name "s + m_variableName);


    std::optional<VariableDefinition> structDef;
    if (context->currentFunction())
    {
        auto functionDefinition =
                context->programUnit()->getFunctionDefinition(context->currentFunction()->getName().str());
        if (functionDefinition)
            structDef = functionDefinition.value()->body()->getVariableDefinition(m_variableName);
    }

    if (!structDef)
    {
        structDef = context->programUnit()->getVariableDefinition(m_variableName);
    }
    auto recordType = std::dynamic_pointer_cast<RecordType>(structDef->variableType);

    auto index = recordType->getFieldIndexByName(m_fieldName);
    auto field = recordType->getField(index);


    auto elementPointer =
            context->builder()->CreateStructGEP(recordType->generateLlvmType(context), V, index, fieldName);

    auto fieldType = field.variableType->generateLlvmType(context);
    auto bitLength = fieldType->getIntegerBitWidth();
    auto result = m_expression->codegen(context);
    if (result->getType()->isIntegerTy() && result->getType()->getIntegerBitWidth() != bitLength)
    {
        result = context->builder()->CreateIntCast(result, fieldType, true, "result_cast");
    }

    const llvm::DataLayout &DL = context->module()->getDataLayout();
    auto alignment = DL.getPrefTypeAlign(field.variableType->generateLlvmType(context));

    context->builder()->CreateAlignedStore(result, elementPointer, alignment);
    return result;
}
