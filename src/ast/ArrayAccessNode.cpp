#include "ArrayAccessNode.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>

#include "ComparissionNode.h"
#include "SystemFunctionCallNode.h"
#include "UnitNode.h"
#include "VariableAccessNode.h"
#include "compiler/Context.h"
#include "types/ArrayType.h"
#include "types/StringType.h"


ArrayAccessNode::ArrayAccessNode(Token arrayName, const std::shared_ptr<ASTNode> &indexNode) :
    ASTNode(arrayName), m_arrayNameToken(std::move(arrayName)), m_indexNode(indexNode)
{
}

void ArrayAccessNode::print() {}


std::shared_ptr<VariableType> ArrayAccessNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    std::shared_ptr<VariableType> varType = nullptr;

    if (parentNode)
    {
        if (auto func = dynamic_cast<FunctionDefinitionNode *>(parentNode))
        {
            if (const auto param = func->getParam(m_arrayNameToken.lexical()))
            {
                varType = param.value().type;
            }
            else if (const auto variableDef = func->body()->getVariableDefinition(m_arrayNameToken.lexical()))
            {
                varType = variableDef->variableType;
            }
        }
    }

    if (!varType)
    {
        if (const auto definition = unit->getVariableDefinition(m_arrayNameToken.lexical()); !definition)
        {
            if (const auto functionDefinition = dynamic_cast<FunctionDefinitionNode *>(parentNode))
            {
                if (const auto param = functionDefinition->getParam(m_arrayNameToken.lexical()))
                {
                    varType = param.value().type;
                }
                else if (const auto variableDef =
                                 functionDefinition->body()->getVariableDefinition(m_arrayNameToken.lexical()))
                {
                    varType = variableDef->variableType;
                }
            }
        }
        else
        {
            varType = definition->variableType;
        }
    }

    if (varType)
    {
        if (const auto array = std::dynamic_pointer_cast<ArrayType>(varType))
            return array->arrayBase;
        if (auto array = std::dynamic_pointer_cast<StringType>(varType))
            return IntegerType::getInteger(8);
    }
    return std::make_shared<VariableType>();
}
Token ArrayAccessNode::expressionToken()
{
    const auto start = m_arrayNameToken.sourceLocation.byte_offset;
    const auto end = m_indexNode->expressionToken().sourceLocation.byte_offset;
    Token token = m_arrayNameToken;
    token.sourceLocation.num_bytes = end - start + m_indexNode->expressionToken().sourceLocation.num_bytes + 1;
    token.sourceLocation.byte_offset = start;
    return token;
}

llvm::Value *ArrayAccessNode::codegen(std::unique_ptr<Context> &context)
{
    const llvm::Value *V = context->NamedAllocations[m_arrayNameToken.lexical()];


    if (!V)
    {
        for (auto &arg: context->TopLevelFunction->args())
        {
            if (arg.getName() == m_arrayNameToken.lexical())
            {
                V = context->TopLevelFunction->getArg(arg.getArgNo());
                break;
            }
        }
    }

    if (!V)
        return LogErrorV("Unknown variable for array access: " + m_arrayNameToken.lexical());

    const auto parent = resolveParent(context);

    const auto arrayDef = context->ProgramUnit->getVariableDefinition(m_arrayNameToken.lexical());
    std::shared_ptr<VariableType> arrayDefType = nullptr;
    if (arrayDef)
    {
        arrayDefType = arrayDef->variableType;
    }

    if (const auto functionDefinition = dynamic_cast<FunctionDefinitionNode *>(parent))
    {
        if (const auto param = functionDefinition->getParam(m_arrayNameToken.lexical()))
        {
            arrayDefType = param.value().type;
        }
        else if (const auto variableDef = functionDefinition->body()->getVariableDefinition(m_arrayNameToken.lexical()))
        {
            arrayDefType = variableDef->variableType;
        }
    }

    if (!arrayDefType)
    {
        return LogErrorV("Unknown variable for array access: " + m_arrayNameToken.lexical());
    }
    if (const auto fieldAccessType = std::dynamic_pointer_cast<FieldAccessableType>(arrayDefType))
    {
        Token token = expressionToken();
        std::vector<std::shared_ptr<ASTNode>> args = {std::make_shared<VariableAccessNode>(m_arrayNameToken, false)};

        auto index = m_indexNode->codegen(context);
        constexpr unsigned maxBitWith = 64;
        const auto targetType = llvm::IntegerType::get(*context->TheContext, maxBitWith);
        if (maxBitWith != index->getType()->getIntegerBitWidth())
        {
            index = context->Builder->CreateIntCast(index, targetType, true, "lhs_cast");
        }
        const auto lowValue = fieldAccessType->getLowValue(context);
        const auto highValue = fieldAccessType->generateHighValue(m_arrayNameToken, context);
        const auto compareSmaller = context->Builder->CreateICmpSLE(index, highValue);
        const auto compareGreater = context->Builder->CreateICmpSGE(index, lowValue);
        const auto andNode = context->Builder->CreateAnd(compareGreater, compareSmaller);
        const std::string message = "index out of range for expression: " + token.lexical();
        SystemFunctionCallNode::codegen_assert(context, resolveParent(context), this, andNode, message);


        return fieldAccessType->generateFieldAccess(m_arrayNameToken, index, context);
    }
    return LogErrorV("variable can not access elements by [] operator: " + m_arrayNameToken.lexical());
}
