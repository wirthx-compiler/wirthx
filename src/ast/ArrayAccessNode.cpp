#include "ArrayAccessNode.h"

#include <llvm/IR/Instructions.h>

#include "UnitNode.h"
#include "compiler/Context.h"


ArrayAccessNode::ArrayAccessNode(Token arrayName, const std::shared_ptr<ASTNode> &indexNode) :
    ASTNode(arrayName), m_arrayNameToken(std::move(arrayName)), m_indexNode(indexNode)
{
}

void ArrayAccessNode::print() {}


std::shared_ptr<VariableType> ArrayAccessNode::resolveType(const std::unique_ptr<UnitNode> &unit, ASTNode *parentNode)
{
    auto definition = unit->getVariableDefinition(m_arrayNameToken.lexical());
    std::shared_ptr<VariableType> varType = nullptr;
    if (!definition)
    {
        if (auto functionDefinition = dynamic_cast<FunctionDefinitionNode *>(parentNode))
        {
            if (const auto param = functionDefinition->getParam(m_arrayNameToken.lexical()))
            {
                varType = param.value().type;
            }
        }
    }
    else
    {
        varType = definition->variableType;
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

    ASTNode *parent = context->ProgramUnit.get();
    if (context->TopLevelFunction)
    {
        if (const auto def = context->ProgramUnit->getFunctionDefinition(context->TopLevelFunction->getName().str()))
        {
            parent = def.value().get();
        }
    }

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
    }

    if (!arrayDefType)
    {
        return LogErrorV("Unknown variable for array access: " + m_arrayNameToken.lexical());
    }
    if (const auto fieldAccessType = std::dynamic_pointer_cast<FieldAccessableType>(arrayDefType))
    {

        const auto index = m_indexNode->codegen(context);
        return fieldAccessType->generateFieldAccess(m_arrayNameToken, index, context);
    }
    return LogErrorV("variable can not access elements by [] operator: " + m_arrayNameToken.lexical());
}
