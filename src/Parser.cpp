#include "Parser.h"

#include <MacroParser.h>
#include <ast/AddressNode.h>
#include <ast/ArrayInitialisationNode.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <llvm/IR/InstrTypes.h>

#include "ast/ArrayAccessNode.h"
#include "ast/ArrayAssignmentNode.h"
#include "ast/BinaryOperationNode.h"
#include "ast/BooleanNode.h"
#include "ast/BreakNode.h"
#include "ast/CaseNode.h"
#include "ast/CharConstantNode.h"
#include "ast/ComparissionNode.h"
#include "ast/DoubleNode.h"
#include "ast/EnumAccessNode.h"
#include "ast/FieldAccessNode.h"
#include "ast/FieldAssignmentNode.h"
#include "ast/ForEachNode.h"
#include "ast/ForNode.h"
#include "ast/FunctionCallNode.h"
#include "ast/IfConditionNode.h"
#include "ast/LogicalExpressionNode.h"
#include "ast/MinusNode.h"
#include "ast/NilPointerNode.h"
#include "ast/NumberNode.h"
#include "ast/RepeatUntilNode.h"
#include "ast/StringConstantNode.h"
#include "ast/SystemFunctionCallNode.h"
#include "ast/TypeNode.h"
#include "ast/VariableAccessNode.h"
#include "ast/VariableAssignmentNode.h"
#include "ast/WhileNode.h"
#include "ast/types/EnumType.h"
#include "ast/types/FileType.h"
#include "ast/types/RecordType.h"
#include "ast/types/StringType.h"
#include "ast/types/ValueRangeType.h"
#include "compare.h"
#include "magic_enum/magic_enum.hpp"


static std::unordered_map<std::string, std::unique_ptr<UnitNode>> unitCache;

Parser::Parser(const std::vector<std::filesystem::path> &rtlDirectories, std::filesystem::path path,
               const std::unordered_map<std::string, bool> &definitions, const std::vector<Token> &tokens) :
    m_rtlDirectories(rtlDirectories), m_file_path(std::move(path)), m_tokens(tokens), m_definitions(definitions)
{
    m_typeDefinitions["shortint"] = VariableType::getInteger(8);
    m_typeDefinitions["byte"] = VariableType::getInteger(8);
    m_typeDefinitions["char"] = VariableType::getCharacter();
    m_typeDefinitions["smallint"] = VariableType::getInteger(16);
    m_typeDefinitions["word"] = VariableType::getInteger(16);
    m_typeDefinitions["longint"] = VariableType::getInteger();
    m_typeDefinitions["integer"] = VariableType::getInteger();
    m_typeDefinitions["int64"] = VariableType::getInteger(64);
    m_typeDefinitions["string"] = StringType::getString();
    m_typeDefinitions["boolean"] = VariableType::getBoolean();
    m_typeDefinitions["pointer"] = PointerType::getUnqual();
    m_typeDefinitions["pinteger"] = PointerType::getPointerTo(VariableType::getInteger());
    m_typeDefinitions["double"] = VariableType::getDouble();
    m_typeDefinitions["real"] = VariableType::getDouble();
    m_typeDefinitions["single"] = VariableType::getSingle();
}
bool Parser::hasError() const
{
    return std::ranges::any_of(m_errors.begin(), m_errors.end(),
                               [](const ParserError &msg) { return msg.outputType == OutputType::ERROR; });
}
bool Parser::hasMessages() const { return !m_errors.empty(); }
void Parser::printErrors(std::ostream &outputStream, const bool printColor) const
{
    for (auto &error: m_errors)
    {
        error.msg(outputStream, printColor);
    }
}


Token Parser::next()
{
    ++m_current;
    return current();
}
Token Parser::current() { return m_tokens[m_current]; }
bool Parser::hasNext() const { return m_current < m_tokens.size(); }
bool Parser::consume(const TokenType tokenType)
{
    if (canConsume(tokenType))
    {
        ++m_current;
        return true;
    }

    m_errors.push_back(ParserError{

            .token = m_tokens[m_current + 1],
            .message = "expected token '" + std::string(magic_enum::enum_name(tokenType)) + "' but found " +
                       std::string(magic_enum::enum_name(m_tokens[m_current + 1].tokenType)) + "!"});
    throw ParserException(m_errors);

    return false;
}
bool Parser::tryConsume(const TokenType tokenType)
{
    if (canConsume(tokenType))
    {
        ++m_current;
        return true;
    }
    return false;
}
bool Parser::canConsume(const TokenType tokenType) const { return canConsume(tokenType, 1); }
bool Parser::canConsume(const TokenType tokenType, const size_t next) const
{
    return hasNext() && m_tokens[m_current + next].tokenType == tokenType;
}

bool Parser::consumeKeyWord(const std::string &keyword)
{
    if (tryConsumeKeyWord(keyword))
    {
        return true;
    }
    m_errors.push_back(ParserError{.token = m_tokens[m_current + 1],
                                   .message = "expected keyword '" + keyword + "' but found " +
                                              std::string(m_tokens[m_current + 1].lexical()) + "!"});
    throw ParserException(m_errors);
}

bool Parser::canConsumeKeyWord(const std::string &keyword) const
{
    return canConsume(TokenType::KEYWORD) && iequals(m_tokens[m_current + 1].lexical(), keyword);
}

bool Parser::tryConsumeKeyWord(const std::string &keyword)
{
    if (canConsumeKeyWord(keyword))
    {
        next();
        return true;
    }
    return false;
}

std::optional<std::shared_ptr<VariableType>> Parser::determinVariableTypeByName(const std::string &name) const
{
    for (const auto &[defName, definition]: m_typeDefinitions)
    {
        if (iequals(defName, name))
            return definition;
    }

    return std::nullopt;
}

std::shared_ptr<ASTNode> Parser::parseEscapedString(const Token &token)
{
    auto result = std::string{};
    size_t x = 1;
    while (x < token.sourceLocation.num_bytes)
    {
        auto next = token.lexical().find('#', x);
        if (next == std::string::npos)
            next = token.sourceLocation.num_bytes;
        auto tmp = token.lexical().substr(x, next - x);
        result += static_cast<char>(std::atoi(tmp.data()));
        x = next + 1;
    }
    if (result.size() == 1)
    {
        return std::make_shared<CharConstantNode>(token, result);
    }
    return std::make_shared<StringConstantNode>(token, result);
}

std::shared_ptr<ASTNode> Parser::parseNumber()
{
    consume(TokenType::NUMBER);
    auto token = current();
    if (token.lexical().find('.') != std::string::npos)
    {
        auto value = std::atof(token.lexical().data());
        return std::make_shared<DoubleNode>(token, value);
    }

    auto value = std::atoll(token.lexical().data());
    auto base = 1 + static_cast<int>(std::log2(value));
    base = (base > 32) ? 64 : 32;
    return std::make_shared<NumberNode>(token, value, base);
}
bool Parser::isVariableDefined(const std::string_view &name, const size_t scope)
{
    return std::ranges::any_of(m_known_variable_definitions, [name, scope](const VariableDefinition &def)
                               { return iequals(def.variableName, name) && def.scopeId <= scope; });
}

bool Parser::isConstantDefined(const std::string_view &name, const size_t scope)
{
    return std::ranges::any_of(m_known_variable_definitions, [name, scope](const VariableDefinition &def)
                               { return iequals(def.variableName, name) && def.scopeId <= scope && def.constant; });
}


std::optional<std::shared_ptr<VariableType>> Parser::parseVariableType(size_t scope, bool includeErrors,
                                                                       const std::string &typeName)
{

    const auto isPointerType = tryConsume(TokenType::CARET);
    // parse type
    if (tryConsumeKeyWord("array"))
    {
        return parseArray(scope);
    }
    else if (tryConsumeKeyWord("record"))
    {
        std::vector<VariableDefinition> fieldDefinitions;

        while (!canConsumeKeyWord("end"))
        {
            for (const auto &definition: parseVariableDefinitions(scope))
                fieldDefinitions.emplace_back(definition);
        }

        consumeKeyWord("end");
        return std::make_shared<RecordType>(fieldDefinitions, typeName);
    }
    else if (canConsume(TokenType::NAMEDTOKEN) || canConsume(TokenType::MINUS))
    {

        if (canConsume(TokenType::DOT, 2) || canConsume(TokenType::DOT, 3))
        {
            const auto startConstant = parseRangeElement(scope);
            consume(TokenType::DOT);
            consume(TokenType::DOT);
            const auto endConstant = parseRangeElement(scope);
            if (const auto startNumber = std::dynamic_pointer_cast<NumberNode>(startConstant))
            {
                if (const auto endNumber = std::dynamic_pointer_cast<NumberNode>(endConstant))
                {
                    return std::make_shared<ValueRangeType>(typeName, startNumber->getValue(), endNumber->getValue());
                }
                else if (const auto accessNode = std::dynamic_pointer_cast<VariableAccessNode>(endConstant))
                {
                    const auto varName = accessNode->expressionToken().lexical();
                    for (auto &var: m_known_variable_definitions)
                    {
                        if (iequals(var.variableName, varName))
                        {
                            if (var.constant)
                            {
                                if (const auto valueNode = std::dynamic_pointer_cast<NumberNode>(var.value))
                                {
                                    return std::make_shared<ValueRangeType>(typeName, startNumber->getValue(),
                                                                            valueNode->getValue());
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            consume(TokenType::NAMEDTOKEN);
        }

        const auto internalTypeName = current().lexical();
        const auto internalType = determinVariableTypeByName(internalTypeName);
        if (!internalType.has_value())
        {
            if (includeErrors)
            {
                m_errors.push_back(ParserError{
                        .token = current(), .message = "The type " + internalTypeName + " could not be determined!"});
            }
            m_current--;
            return std::nullopt;
        }

        if (isPointerType)
        {
            return PointerType::getPointerTo(internalType.value());
        }
        else
        {
            return internalType.value();
        }
    }
    else if (tryConsume(TokenType::LEFT_CURLY))
    {

        if (canConsume(TokenType::NAMEDTOKEN))
        {
            auto enumType = EnumType::getEnum(typeName);

            int64_t startValue = 0;
            while (canConsume(TokenType::NAMEDTOKEN))
            {
                next();
                enumType->addEnumValue(current().lexical(), startValue);
                if (tryConsume(TokenType::EQUAL))
                {
                    const auto number = std::dynamic_pointer_cast<NumberNode>(parseNumber());
                    startValue = number->getValue();
                }
                tryConsume(TokenType::COMMA);

                startValue++;
            }
            consume(TokenType::RIGHT_CURLY);
            return enumType;
        }
        else if (canConsume(TokenType::NUMBER))
        {
            const auto startNumber = std::dynamic_pointer_cast<NumberNode>(parseNumber());
            consume(TokenType::DOT);
            consume(TokenType::DOT);
            const auto endNumber = std::dynamic_pointer_cast<NumberNode>(parseNumber());
            consume(TokenType::RIGHT_CURLY);
            return std::make_shared<ValueRangeType>(typeName, startNumber->getValue(), endNumber->getValue());
        }
    }
    return std::nullopt;
}


void Parser::parseTypeDefinitions(const size_t scope)
{
    // parse type definitions
    while (tryConsume(TokenType::NAMEDTOKEN))
    {
        const auto typeName = current().lexical();
        consume(TokenType::EQUAL);
        if (const auto type = parseVariableType(scope, true, typeName); type.has_value())
        {
            m_typeDefinitions[typeName] = type.value();
            consume(TokenType::SEMICOLON);
        }
    }
}
std::shared_ptr<ArrayType> Parser::parseArray(size_t scope)
{
    const auto isFixedArray = tryConsume(TokenType::LEFT_SQUAR);
    size_t arrayStart = 0;
    size_t arrayEnd = 0;
    if (isFixedArray)
    {

        const auto arrayStartNode = parseToken(scope);
        if (const auto node = std::dynamic_pointer_cast<NumberNode>(arrayStartNode))
        {
            arrayStart = node->getValue();
        }
        consume(TokenType::DOT);
        consume(TokenType::DOT);

        const auto arrayEndNode = parseToken(scope);
        if (const auto node = std::dynamic_pointer_cast<NumberNode>(arrayEndNode))
        {
            arrayEnd = node->getValue();
        }
        else if (const auto accessNode = std::dynamic_pointer_cast<VariableAccessNode>(arrayEndNode))
        {
            const auto varName = accessNode->expressionToken().lexical();
            for (auto &var: m_known_variable_definitions)
            {
                if (iequals(var.variableName, varName))
                {
                    if (var.constant)
                    {
                        if (const auto valueNode = std::dynamic_pointer_cast<NumberNode>(var.value))
                        {
                            arrayEnd = valueNode->getValue();
                        }
                    }
                }
            }
        }
        consume(TokenType::RIGHT_SQUAR);
    }
    consumeKeyWord("of");
    consume(TokenType::NAMEDTOKEN);
    const auto internalTypeName = std::string(current().lexical());
    const auto internalType = determinVariableTypeByName(internalTypeName);


    if (isFixedArray)
    {
        return ArrayType::getFixedArray(arrayStart, arrayEnd, internalType.value());
    }
    return ArrayType::getDynArray(internalType.value());
}
std::optional<VariableDefinition> Parser::parseConstantDefinition(size_t scope)
{

    // consume var declarations
    consume(TokenType::NAMEDTOKEN);
    const Token varNameToken = current();

    const auto varName = std::string(current().lexical());


    std::optional<std::shared_ptr<VariableType>> type;
    std::string varType;

    if (tryConsume(TokenType::COLON))
    {
        consume(TokenType::NAMEDTOKEN);
        varType = std::string(current().lexical());
        type = determinVariableTypeByName(varType);
    }
    std::shared_ptr<ASTNode> value;
    if (consume(TokenType::EQUAL))
    {
        value = parseToken(scope);

        // determin the type from the parsed token
        if (!type)
        {
            type = value->resolveType(nullptr, nullptr);
            if (type.has_value())
                varType = type.value()->typeName;
        }
    }

    consume(TokenType::SEMICOLON);

    if (isVariableDefined(varName, scope))
    {
        m_errors.push_back(
                ParserError{.token = varNameToken,
                            .message = "A variable or constant with the name " + varName + " was allready defined!"});
        return std::nullopt;
    }

    if (!type.has_value())
    {
        m_errors.push_back(ParserError{.token = varNameToken,
                                       .message = "A type " + varType + " of the variable " + varName +
                                                  " could not be determined!"});
        return std::nullopt;
    }

    return VariableDefinition{.variableType = type.value(),
                              .variableName = varName,
                              .token = varNameToken,
                              .scopeId = scope,
                              .value = value,
                              .constant = true};
}
std::shared_ptr<ASTNode> Parser::parseArrayConstructor(size_t size)
{
    std::vector<std::shared_ptr<ASTNode>> arguments;
    consume(TokenType::LEFT_SQUAR);
    Token startToken = current();

    while (!canConsume(TokenType::RIGHT_SQUAR))
    {
        arguments.push_back(parseToken(size));
        tryConsume(TokenType::COMMA);
    }
    consume(TokenType::RIGHT_SQUAR);
    return std::make_shared<ArrayInitialisationNode>(startToken, arguments);
}
std::vector<VariableDefinition> Parser::parseVariableDefinitions(const size_t scope)
{
    std::vector<VariableDefinition> result;
    Token _currentToken = current();

    // consume var declarations


    std::vector<Token> varNames;
    do
    {
        consume(TokenType::NAMEDTOKEN);
        _currentToken = current();
        varNames.push_back(_currentToken);
        if (!tryConsume(TokenType::COMMA))
        {
            break;
        }
    }
    while (!canConsume(TokenType::COLON));

    std::optional<std::shared_ptr<VariableType>> type;
    std::string varType;

    if (tryConsume(TokenType::COLON))
    {
        auto isPointerType = tryConsume(TokenType::CARET);

        if (tryConsume(TokenType::NAMEDTOKEN))
        {
            _currentToken = current();
            varType = std::string(_currentToken.lexical());
            type = determinVariableTypeByName(varType);
        }
        else if (canConsumeKeyWord("file"))
        {
            consumeKeyWord("file");
            varType = std::string(_currentToken.lexical());
            type = FileType::getFileType();
        }
        else if (tryConsumeKeyWord("array"))
        {
            varType = "array";
            type = parseArray(scope);
        }
        if (isPointerType && type)
        {
            type = PointerType::getPointerTo(type.value());
        }
    }
    std::shared_ptr<ASTNode> value;
    if (type && type.value()->baseType == VariableBaseType::Array)
    {
        if (tryConsume(TokenType::EQUAL))
        {
            value = parseArrayConstructor(scope);
        }
    }
    else if (tryConsume(TokenType::EQUAL))
    {
        value = parseToken(scope);
        assert(value && "value for an variable initialization with assignment can not be null");
        // determin the type from the parsed token
        if (!type)
        {
            type = value->resolveType(nullptr, nullptr);
            if (type.has_value())
                varType = type.value()->typeName;
        }
    }

    consume(TokenType::SEMICOLON);
    for (const auto &varName: varNames)
    {
        if (isVariableDefined(varName.lexical(), scope))
        {
            m_errors.push_back(ParserError{.token = _currentToken,
                                           .message = "A variable or constant with the name " + varName.lexical() +
                                                      " was all ready defined!"});
            return {};
        }
        if (!type.has_value())
        {
            m_errors.push_back(ParserError{.token = _currentToken,
                                           .message = "A type " + varType + " of the variable " + varName.lexical() +
                                                      " could not be determined!"});
            return {};
        }

        result.push_back(VariableDefinition{.variableType = type.value(),
                                            .variableName = varName.lexical(),
                                            .token = varName,
                                            .scopeId = scope,
                                            .value = value,
                                            .constant = false});
    }
    return result;
}

std::shared_ptr<ASTNode> Parser::parseLogicalExpression(const size_t scope, std::shared_ptr<ASTNode> lhs)
{
    if (canConsumeKeyWord("not"))
    {
        consumeKeyWord("not");
        auto token = current();
        auto rhs = parseExpression(scope);
        return parseExpression(scope, std::make_shared<LogicalExpressionNode>(token, LogicalOperator::NOT, rhs));
    }

    if (!lhs)
        return nullptr;

    if (canConsumeKeyWord("or"))
    {
        consumeKeyWord("or");
        auto token = current();
        auto rhs = parseExpression(scope);
        return parseExpression(scope, std::make_shared<LogicalExpressionNode>(token, LogicalOperator::OR, lhs, rhs));
    }
    if (canConsumeKeyWord("and"))
    {
        consumeKeyWord("and");
        auto token = current();
        auto rhs = parseExpression(scope);
        return parseExpression(scope, std::make_shared<LogicalExpressionNode>(token, LogicalOperator::AND, lhs, rhs));
    }
    return lhs;
}

void Parser::checkLhsExists(const std::shared_ptr<ASTNode> &lhs, const Token &token)
{
    if (!lhs)
    {
        m_errors.push_back(ParserError{

                .token = token,
                .message = "unexpected token " + std::string(magic_enum::enum_name(token.tokenType)) + "!"});
    }
}
/*
 a1 * a2 + b1 * b2 ==> (a * a) + (b * b)
 1. lhs = a1
 2. parse *
 3. parse a2;
*/
static std::map<Operator, int> operatorPrecedence{
        {Operator::MUL, 20}, {Operator::DIV, 10}, {Operator::PLUS, 5}, {Operator::MINUS, 2}};
std::shared_ptr<ASTNode> Parser::parseBaseExpression(const size_t scope, const std::shared_ptr<ASTNode> &origLhs,
                                                     bool includeCompare)
{
    auto lhs = (origLhs) ? origLhs : parseToken(scope);


    if (tryConsume(TokenType::PLUS))
    {
        Token operatorToken = current();
        checkLhsExists(lhs, operatorToken);
        auto rhs = parseToken(scope);

        if (canConsume(TokenType::MUL) or canConsume(TokenType::DIV) or canConsume(TokenType::LEFT_CURLY))
        {
            rhs = parseBaseExpression(scope, rhs, false);
        }

        return parseExpression(scope, std::make_shared<BinaryOperationNode>(operatorToken, Operator::PLUS, lhs, rhs));
    }
    if (tryConsume(TokenType::MINUS))
    {
        Token operatorToken = current();
        checkLhsExists(lhs, operatorToken);
        auto rhs = parseToken(scope);
        if (canConsume(TokenType::MUL) or canConsume(TokenType::DIV) or canConsume(TokenType::LEFT_CURLY))
        {
            rhs = parseBaseExpression(scope, rhs, false);
        }

        return parseExpression(scope, std::make_shared<BinaryOperationNode>(operatorToken, Operator::MINUS, lhs, rhs));
    }
    if (tryConsume(TokenType::MUL))
    {
        Token operatorToken = current();
        checkLhsExists(lhs, operatorToken);
        auto rhs = parseToken(scope);
        return parseExpression(scope, std::make_shared<BinaryOperationNode>(operatorToken, Operator::MUL, lhs, rhs));
    }
    if (tryConsume(TokenType::DIV))
    {
        Token operatorToken = current();
        checkLhsExists(lhs, operatorToken);
        auto rhs = parseToken(scope);
        return parseExpression(scope, std::make_shared<BinaryOperationNode>(operatorToken, Operator::DIV, lhs, rhs));
    }
    if (canConsumeKeyWord("mod"))
    {
        Token operatorToken = current();
        checkLhsExists(lhs, operatorToken);
        consumeKeyWord("mod");
        auto rhs = parseToken(scope);
        return parseExpression(scope, std::make_shared<BinaryOperationNode>(operatorToken, Operator::MOD, lhs, rhs));
    }
    if (canConsumeKeyWord("div"))
    {
        Token operatorToken = current();
        checkLhsExists(lhs, operatorToken);
        consumeKeyWord("div");
        auto rhs = parseToken(scope);
        return parseExpression(scope, std::make_shared<BinaryOperationNode>(operatorToken, Operator::IDIV, lhs, rhs));
    }
    if (canConsume(TokenType::LEFT_CURLY))
    {
        consume(TokenType::LEFT_CURLY);
        auto result = parseExpression(scope, nullptr);
        consume(TokenType::RIGHT_CURLY);
        if (auto binOp = std::dynamic_pointer_cast<BinaryOperationNode>(lhs))
        {
            result = std::make_shared<BinaryOperationNode>(binOp->expressionToken(), binOp->binoperator(), binOp->lhs(),
                                                           result);
        }
        return parseExpression(scope, result);
    }

    if (includeCompare)
    {
        if (canConsume(TokenType::GREATER))
        {
            consume(TokenType::GREATER);
            auto operatorToken = current();
            checkLhsExists(lhs, operatorToken);
            if (canConsume(TokenType::EQUAL))
            {
                consume(TokenType::EQUAL);
                auto rhs = parseBaseExpression(scope);
                return std::make_shared<ComparrisionNode>(operatorToken, CMPOperator::GREATER_EQUAL, lhs, rhs);
            }
            auto rhs = parseBaseExpression(scope);
            return parseExpression(scope,
                                   std::make_shared<ComparrisionNode>(operatorToken, CMPOperator::GREATER, lhs, rhs));
        }

        if (canConsume(TokenType::LESS))
        {
            consume(TokenType::LESS);
            auto operatorToken = current();
            checkLhsExists(lhs, operatorToken);
            if (canConsume(TokenType::EQUAL))
            {
                consume(TokenType::EQUAL);
                auto rhs = parseBaseExpression(scope);
                return std::make_shared<ComparrisionNode>(operatorToken, CMPOperator::LESS_EQUAL, lhs, rhs);
            }
            else if (canConsume(TokenType::GREATER))
            {
                consume(TokenType::GREATER);
                auto rhs = parseBaseExpression(scope);
                return parseExpression(
                        scope, std::make_shared<ComparrisionNode>(operatorToken, CMPOperator::NOT_EQUALS, lhs, rhs));
            }
            auto rhs = parseBaseExpression(scope);
            return parseExpression(scope,
                                   std::make_shared<ComparrisionNode>(operatorToken, CMPOperator::LESS, lhs, rhs));
        }

        if (canConsume(TokenType::BANG) && canConsume(TokenType::EQUAL, 2))
        {
            consume(TokenType::BANG);
            consume(TokenType::EQUAL);
            auto operatorToken = current();
            checkLhsExists(lhs, operatorToken);
            auto rhs = parseBaseExpression(scope);
            return parseExpression(
                    scope, std::make_shared<ComparrisionNode>(operatorToken, CMPOperator::NOT_EQUALS, lhs, rhs));
        }

        if (canConsume(TokenType::EQUAL))
        {
            consume(TokenType::EQUAL);
            auto operatorToken = current();
            checkLhsExists(lhs, operatorToken);
            auto rhs = parseBaseExpression(scope);
            return parseExpression(scope,
                                   std::make_shared<ComparrisionNode>(operatorToken, CMPOperator::EQUALS, lhs, rhs));
        }
    }

    return lhs;
}

std::shared_ptr<ASTNode> Parser::parseExpression(const size_t scope, const std::shared_ptr<ASTNode> &origLhs)
{
    auto lhs = parseLogicalExpression(scope, origLhs);
    if (!lhs)
        lhs = origLhs;

    if (!lhs)
        lhs = parseBaseExpression(scope);

    if (auto rhs = parseLogicalExpression(scope, parseBaseExpression(scope, lhs)))
        return rhs;

    return lhs;
}

std::shared_ptr<ASTNode> Parser::parseVariableAssignment(size_t scope)
{
    consume(TokenType::NAMEDTOKEN);
    auto currentToken = current();
    auto variableNameToken = currentToken;
    auto variableName = std::string(currentToken.lexical());
    auto dereference = tryConsume(TokenType::CARET);

    if (canConsume(TokenType::COLON))
    {
        if (!consume(TokenType::COLON))
        {
        }

        if (!consume(TokenType::EQUAL))
        {
            m_errors.push_back(ParserError{.token = currentToken, .message = "missing assignment for varaible!"});
            return nullptr;
        }

        if (!isVariableDefined(variableName, scope))
        {
            m_errors.push_back(
                    ParserError{.token = currentToken,
                                .message = "The variable " + std::string(variableName) + " is not yet declared!"});
        }

        // parse expression
        auto expression = parseExpression(scope);
        return std::make_shared<VariableAssignmentNode>(variableNameToken, expression, dereference);
    }
    else if (canConsume(TokenType::DOT))
    {
        consume(TokenType::DOT);
        if (!consume(TokenType::NAMEDTOKEN))
        {
            m_errors.push_back(
                    ParserError{.token = current(), .message = "missing field on variable " + variableName + "!"});
            return nullptr;
        }
        auto fieldName = current();
        consume(TokenType::COLON);
        if (!consume(TokenType::EQUAL))
        {
            m_errors.push_back(ParserError{.token = currentToken, .message = "missing assignment for varaible!"});
            return nullptr;
        }

        if (!isVariableDefined(variableName, scope))
        {
            m_errors.push_back(
                    ParserError{.token = currentToken,
                                .message = "The variable " + std::string(variableName) + " is not yet declared!"});
            return nullptr;
        }

        // parse expression
        auto expression = parseExpression(scope);

        auto variable = currentToken;
        auto field = fieldName;
        return std::make_shared<FieldAssignmentNode>(variable, field, expression);
    }
    else
    {
        consume(TokenType::LEFT_SQUAR);
        auto index = parseExpression(scope);
        consume(TokenType::RIGHT_SQUAR);
        consume(TokenType::COLON);
        if (!consume(TokenType::EQUAL))
        {
            m_errors.push_back(ParserError{.token = currentToken, .message = "missing assignment for varaible!"});
            return nullptr;
        }

        if (!isVariableDefined(variableName, scope))
        {
            m_errors.push_back(
                    ParserError{.token = currentToken,
                                .message = "The variable " + std::string(variableName) + " is not yet declared!"});
            return nullptr;
        }
        auto expression = parseExpression(scope);
        return std::make_shared<ArrayAssignmentNode>(variableNameToken, index, expression);
    }
}
std::optional<std::shared_ptr<EnumType>> Parser::tryGetEnumTypeByValue(const std::string &enumKey) const
{
    for (const auto &[typeName, type]: m_typeDefinitions)
    {
        if (auto enumType = std::dynamic_pointer_cast<EnumType>(type))
        {
            if (enumType->hasEnumKey(enumKey))
                return enumType;
        }
    }
    return std::nullopt;
}

std::shared_ptr<ASTNode> Parser::parseRangeElement(const size_t scope)
{
    if (canConsume(TokenType::NUMBER))
    {
        return parseNumber();
    }
    if (canConsume(TokenType::STRING))
    {
        consume(TokenType::STRING);

        return std::make_shared<StringConstantNode>(current(), std::string(current().lexical()));
    }
    if (canConsume(TokenType::CHAR))
    {
        consume(TokenType::CHAR);
        return std::make_shared<CharConstantNode>(current(), std::string(current().lexical()));
    }
    if (canConsume(TokenType::ESCAPED_STRING))
    {
        consume(TokenType::ESCAPED_STRING);
        return parseEscapedString(current());
    }

    if (canConsume(TokenType::NAMEDTOKEN))
    {
        return parseConstantAccess(scope);
    }
    if (tryConsumeKeyWord("true"))
    {
        return std::make_shared<BooleanNode>(current(), true);
    }
    if (tryConsumeKeyWord("false"))
    {
        return std::make_shared<BooleanNode>(current(), false);
    }
    if (canConsume(TokenType::MINUS) && canConsume(TokenType::NAMEDTOKEN, 2))
    {
        consume(TokenType::MINUS);
        const auto constAccessNode = parseConstantAccess(scope);
        const auto varName = constAccessNode->expressionToken().lexical();
        for (auto &var: m_known_variable_definitions)
        {
            if (iequals(var.variableName, varName))
            {
                if (var.constant)
                {
                    if (const auto valueNode = std::dynamic_pointer_cast<NumberNode>(var.value))
                    {
                        return std::make_shared<MinusNode>(current(), var.value);
                    }
                }
            }
        }
    }
    return nullptr;
}

std::shared_ptr<ASTNode> Parser::parseConstantAccess(const size_t scope)
{
    consume(TokenType::NAMEDTOKEN);
    auto token = current();
    auto constantName = std::string(current().lexical());

    if (auto enumType = tryGetEnumTypeByValue(token.lexical()))
    {
        return std::make_shared<EnumAccessNode>(token, enumType.value());
    }
    if (!isConstantDefined(token.lexical(), scope))
    {
        m_errors.push_back(ParserError{
                .token = token, .message = "A constant with the name '" + token.lexical() + "' is not yet defined!"});
        return nullptr;
    }
    auto dereference = tryConsume(TokenType::CARET);

    return std::make_shared<VariableAccessNode>(token, dereference);
}

std::shared_ptr<ASTNode> Parser::parseVariableAccess(const size_t scope)
{
    consume(TokenType::NAMEDTOKEN);
    auto token = current();
    auto variableName = std::string(current().lexical());
    if (canConsume(TokenType::LEFT_SQUAR))
    {
        const Token arrayName = token;
        if (!isVariableDefined(token.lexical(), scope))
        {
            m_errors.push_back(
                    ParserError{.token = token,
                                .message = "A variable with the name '" + token.lexical() + "' is not yet defined!"});
            return nullptr;
        }
        consume(TokenType::LEFT_SQUAR);
        auto indexNode = parseExpression(scope);
        consume(TokenType::RIGHT_SQUAR);
        return std::make_shared<ArrayAccessNode>(arrayName, indexNode);
    }
    if (canConsume(TokenType::DOT))
    {
        consume(TokenType::DOT);
        consume(TokenType::NAMEDTOKEN);
        Token field = current();
        if (!isVariableDefined(token.lexical(), scope))
        {
            m_errors.push_back(
                    ParserError{.token = token,
                                .message = "A variable with the name '" + token.lexical() + "' is not yet defined!"});
            return nullptr;
        }
        return std::make_shared<FieldAccessNode>(token, field);
    }

    if (auto enumType = tryGetEnumTypeByValue(token.lexical()))
    {
        return std::make_shared<EnumAccessNode>(token, enumType.value());
    }
    if (!isVariableDefined(token.lexical(), scope))
    {
        m_errors.push_back(ParserError{
                .token = token, .message = "A variable with the name '" + token.lexical() + "' is not yet defined!"});
        return nullptr;
    }
    auto dereference = tryConsume(TokenType::CARET);

    return std::make_shared<VariableAccessNode>(token, dereference);
}
std::shared_ptr<ASTNode> Parser::parseToken(const size_t scope)
{
    if (canConsume(TokenType::NUMBER))
    {
        return parseNumber();
    }
    if (canConsume(TokenType::STRING))
    {
        consume(TokenType::STRING);

        return std::make_shared<StringConstantNode>(current(), std::string(current().lexical()));
    }
    if (canConsume(TokenType::CHAR))
    {
        consume(TokenType::CHAR);
        return std::make_shared<CharConstantNode>(current(), std::string(current().lexical()));
    }
    if (canConsume(TokenType::ESCAPED_STRING))
    {
        consume(TokenType::ESCAPED_STRING);
        return parseEscapedString(current());
    }
    if (canConsume(TokenType::AT))
    {
        consume(TokenType::AT);
        consume(TokenType::NAMEDTOKEN);
        Token field = current();
        return std::make_shared<AddressNode>(field);
    }
    if (canConsume(TokenType::NAMEDTOKEN))
    {
        if (canConsume(TokenType::LEFT_CURLY, 2))
        {
            return parseFunctionCall(scope);
        }


        return parseVariableAccess(scope);
    }
    if (tryConsumeKeyWord("true"))
    {
        return std::make_shared<BooleanNode>(current(), true);
    }
    if (tryConsumeKeyWord("false"))
    {
        return std::make_shared<BooleanNode>(current(), false);
    }
    if (tryConsumeKeyWord("nil"))
    {
        return std::make_shared<NilPointerNode>(current());
    }
    if (canConsume(TokenType::MINUS) && canConsume(TokenType::NAMEDTOKEN, 2))
    {
        consume(TokenType::MINUS);
        auto valueNode = parseToken(scope);
        return std::make_shared<MinusNode>(current(), valueNode);
    }
    return nullptr;
}
std::shared_ptr<ASTNode> Parser::parseRangeElementOrType(const size_t scope)
{

    if (auto type = parseVariableType(scope, false); type.has_value())
    {
        return std::make_shared<TypeNode>(current(), type.value());
    }

    if (auto token = parseRangeElement(scope))
    {
        return token;
    }
    return nullptr;
}
std::shared_ptr<FunctionDefinitionNode> Parser::parseFunctionDeclaration(size_t scope, bool isFunction)
{
    consume(TokenType::NAMEDTOKEN);
    auto functionNameToken = current();
    auto functionName = current().lexical();
    m_known_function_names.push_back(functionName);
    std::string libName;
    std::string externalName = functionName;

    std::shared_ptr<FunctionDefinitionNode> functionDefinition = nullptr;

    consume(TokenType::LEFT_CURLY);
    auto token = next();
    std::vector<FunctionArgument> functionParams;
    std::vector<FunctionAttribute> functionAttributes;
    while (token.tokenType != TokenType::RIGHT_CURLY)
    {

        bool isReference = false;
        if (token.tokenType == TokenType::KEYWORD && iequals(token.lexical(), "var"))
        {
            next();
            isReference = true;
        }
        token = current();
        const std::string funcParamName = token.lexical();
        std::vector<Token> paramNames;
        paramNames.push_back(token);
        while (canConsume(TokenType::COMMA))
        {
            consume(TokenType::COMMA);
            consume(TokenType::NAMEDTOKEN);
            paramNames.emplace_back(current());
        }

        consume(TokenType::COLON);
        if (canConsume(TokenType::NAMEDTOKEN))
        {
            token = next();
            auto type = determinVariableTypeByName(token.lexical());

            for (const auto &param: paramNames)
            {

                if (isVariableDefined(param.lexical(), scope))
                {
                    m_errors.push_back(ParserError{.token = token,
                                                   .message = "A variable with the name " + param.lexical() +
                                                              " was allready defined!"});
                }
                else if (!type.has_value())
                {
                    m_errors.push_back(ParserError{.token = token,
                                                   .message = "A type " + token.lexical() + " of the variable " +
                                                              param.lexical() + " could not be determined!"});
                }
                else
                {


                    functionParams.push_back(FunctionArgument{.type = type.value(),
                                                              .argumentName = param.lexical(),
                                                              .token = param,
                                                              .isReference = isReference});
                }
            }
            tryConsume(TokenType::SEMICOLON);
        }
        else if (canConsumeKeyWord("file"))
        {
            consumeKeyWord("file");
            std::shared_ptr<VariableType> variableType = FileType::getFileType();
            for (const auto &param: paramNames)
            {
                functionParams.push_back(FunctionArgument{.type = variableType,
                                                          .argumentName = param.lexical(),
                                                          .token = param,
                                                          .isReference = isReference});
            }
            tryConsume(TokenType::SEMICOLON);
        }
        else
        {
            // TODO: type def missing
            m_errors.push_back(ParserError{.token = token,
                                           .message = "For the parameter definition " + funcParamName +
                                                      " there is a type missing"});
        }

        token = next();
    }
    if (isFunction)
        if (!tryConsume(TokenType::COLON))
        {
            m_errors.push_back(
                    ParserError{.token = current(),
                                .message = "the return type for the function \"" + functionName + "\" is missing."});
            throw ParserException(m_errors);
        }
    std::shared_ptr<VariableType> returnType;

    if (isFunction && consume(TokenType::NAMEDTOKEN))
    {
        const auto typeName = current().lexical();
        const auto type = determinVariableTypeByName(typeName);
        if (!type)
        {
            m_errors.push_back(
                    ParserError{.token = current(),
                                .message = "A return type " + typeName + " of function could not be determined!"});
        }
        else
        {
            returnType = type.value();
        }
    }
    consume(TokenType::SEMICOLON);

    if (tryConsumeKeyWord("external"))
    {
        if (tryConsume(TokenType::STRING) || tryConsume(TokenType::CHAR))
            libName = std::string(current().lexical());

        if (tryConsumeKeyWord("name"))
        {
            consume(TokenType::STRING);
            externalName = std::string(current().lexical());
        }
        tryConsume(TokenType::SEMICOLON);
    }
    else if (tryConsumeKeyWord("inline"))
    {
        functionAttributes.emplace_back(FunctionAttribute::Inline);
        consume(TokenType::SEMICOLON);
    }


    return std::make_shared<FunctionDefinitionNode>(functionNameToken, functionName, externalName, libName,
                                                    functionParams, !isFunction, returnType);
}
std::shared_ptr<FunctionDefinitionNode> Parser::parseFunctionDefinition(size_t scope, bool isFunction)
{
    consume(TokenType::NAMEDTOKEN);
    auto functionNameToken = current();
    auto functionName = current().lexical();
    m_known_function_names.push_back(functionName);
    bool isExternalFunction = false;
    std::string libName;
    std::string externalName = functionName;

    std::shared_ptr<FunctionDefinitionNode> functionDefinition = nullptr;

    consume(TokenType::LEFT_CURLY);
    auto token = next();
    std::vector<FunctionArgument> functionParams;
    std::vector<FunctionAttribute> functionAttributes;
    while (token.tokenType != TokenType::RIGHT_CURLY)
    {

        bool isReference = false;
        if (token.tokenType == TokenType::KEYWORD && iequals(token.lexical(), "var"))
        {
            next();
            isReference = true;
        }
        token = current();
        const auto funcParamName = token;
        std::vector<Token> paramNames;
        paramNames.push_back(funcParamName);
        while (canConsume(TokenType::COMMA))
        {
            consume(TokenType::COMMA);
            consume(TokenType::NAMEDTOKEN);
            paramNames.emplace_back(current());
        }

        consume(TokenType::COLON);
        if (canConsume(TokenType::NAMEDTOKEN))
        {
            token = next();
            auto type = determinVariableTypeByName(token.lexical());

            for (const auto &param: paramNames)
            {

                if (isVariableDefined(param.lexical(), scope))
                {
                    m_errors.push_back(ParserError{.token = token,
                                                   .message = "A variable with the name " + param.lexical() +
                                                              " was allready defined!"});
                }
                else if (!type.has_value())
                {
                    m_errors.push_back(ParserError{.token = token,
                                                   .message = "A type " + token.lexical() + " of the variable " +
                                                              param.lexical() + " could not be determined!"});
                }
                else
                {

                    m_known_variable_definitions.push_back(VariableDefinition{.variableType = type.value(),
                                                                              .variableName = param.lexical(),
                                                                              .token = param,
                                                                              .scopeId = scope});

                    functionParams.push_back(FunctionArgument{.type = type.value(),
                                                              .argumentName = param.lexical(),
                                                              .token = param,
                                                              .isReference = isReference});
                }
            }
            tryConsume(TokenType::SEMICOLON);
        }
        else
        {
            // TODO: type def missing
            m_errors.push_back(ParserError{.token = token,
                                           .message = "For the parameter definition " + funcParamName.lexical() +
                                                      " there is a type missing"});
        }

        token = next();
    }
    if (isFunction)
        if (!tryConsume(TokenType::COLON))
        {
            m_errors.push_back(
                    ParserError{.token = m_tokens[m_current + 1],
                                .message = "the return type for the function \"" + functionName + "\" is missing."});
            throw ParserException(m_errors);
        }
    std::shared_ptr<VariableType> returnType;

    if (isFunction && consume(TokenType::NAMEDTOKEN))
    {
        const auto typeName = current().lexical();
        const auto type = determinVariableTypeByName(typeName);
        if (!type)
        {
            m_errors.push_back(
                    ParserError{.token = current(),
                                .message = "A return type " + typeName + " of function could not be determined!"});
        }
        else
        {
            returnType = type.value();
        }

        m_known_variable_definitions.push_back(VariableDefinition{.variableType = returnType,
                                                                  .variableName = functionName,
                                                                  .token = functionNameToken,
                                                                  .scopeId = scope});
        m_known_variable_definitions.push_back(VariableDefinition{
                .variableType = returnType, .variableName = "result", .token = functionNameToken, .scopeId = scope});
    }
    consume(TokenType::SEMICOLON);

    if (tryConsumeKeyWord("external"))
    {
        isExternalFunction = true;
        if (tryConsume(TokenType::STRING) || tryConsume(TokenType::CHAR))
            libName = std::string(current().lexical());

        if (tryConsumeKeyWord("name"))
        {
            consume(TokenType::STRING);
            externalName = std::string(current().lexical());
        }
        tryConsume(TokenType::SEMICOLON);
    }
    else if (tryConsumeKeyWord("inline"))
    {
        functionAttributes.emplace_back(FunctionAttribute::Inline);
        consume(TokenType::SEMICOLON);
    }


    // parse function body
    if (isExternalFunction)
    {
        functionDefinition = std::make_shared<FunctionDefinitionNode>(functionNameToken, functionName, externalName,
                                                                      libName, functionParams, !isFunction, returnType);
    }
    else
    {
        auto functionBody = parseBlock(scope + 1);
        consume(TokenType::SEMICOLON);
        if (isFunction)
        {
            functionBody->addVariableDefinition(VariableDefinition{.variableType = returnType,
                                                                   .variableName = functionName,
                                                                   .token = functionNameToken,
                                                                   .alias = "result",
                                                                   .scopeId = 0,
                                                                   .value = nullptr,
                                                                   .constant = false});
        }
        functionDefinition = std::make_shared<FunctionDefinitionNode>(functionNameToken, functionName, functionParams,
                                                                      functionBody, !isFunction, returnType);
        for (auto attribute: functionAttributes)
            functionDefinition->addAttribute(attribute);

        for (auto &def: functionBody->getVariableDefinitions())
        {
            m_known_variable_definitions.erase(
                    std::ranges::remove_if(m_known_variable_definitions, [def](const VariableDefinition &value)
                                           { return def.variableName == value.variableName; })
                            .begin());
            if (!def.alias.empty())
            {
                m_known_variable_definitions.erase(std::ranges::remove_if(m_known_variable_definitions,
                                                                          [def](const VariableDefinition &value)
                                                                          { return def.alias == value.variableName; })
                                                           .begin());
            }
        }
    }


    for (auto &param: functionParams)
    {
        m_known_variable_definitions.erase(std::ranges::remove_if(m_known_variable_definitions,
                                                                  [param](const VariableDefinition &value)
                                                                  { return param.argumentName == value.variableName; })
                                                   .begin());
    }
    return functionDefinition;
}

std::shared_ptr<ASTNode> Parser::parseStatement(size_t scope, bool withSemicolon)
{
    std::shared_ptr<ASTNode> result = nullptr;
    if (canConsume(TokenType::NAMEDTOKEN))
    {
        if (canConsume(TokenType::LEFT_CURLY, 2))
        {
            result = parseFunctionCall(scope);
        }
        else
        {
            result = parseVariableAssignment(scope);
        }
        if (withSemicolon)
            consume(TokenType::SEMICOLON);
    }
    else if (canConsume(TokenType::KEYWORD))
    {
        result = parseKeyword(scope, withSemicolon);
    }
    if (!result)
    {
        m_errors.push_back(
                ParserError{.token = m_tokens[m_current + 1],
                            .message = "unexpected token found " +
                                       std::string(magic_enum::enum_name(m_tokens[m_current + 1].tokenType)) + "!"});
    }

    return result;
}


void Parser::parseConstantDefinitions(size_t scope, std::vector<VariableDefinition> &variable_definitions)
{
    if (tryConsumeKeyWord("const"))
    {
        while (!canConsume(TokenType::KEYWORD))
        {
            const auto definition = parseConstantDefinition(scope);
            if (definition.has_value())
            {
                variable_definitions.push_back(definition.value());
                m_known_variable_definitions.push_back(definition.value());
            }
        }
    }
}
std::shared_ptr<BlockNode> Parser::parseBlock(const size_t scope)
{
    std::vector<VariableDefinition> variable_definitions;

    parseConstantDefinitions(scope, variable_definitions);

    if (tryConsumeKeyWord("var"))
    {
        while (!canConsumeKeyWord("begin"))
        {
            auto def = parseVariableDefinitions(scope);
            for (auto &definition: def)
            {
                variable_definitions.push_back(definition);
                m_known_variable_definitions.push_back(definition);
            }
        }
    }
    consumeKeyWord("begin");
    auto beginToken = current();
    std::vector<std::shared_ptr<ASTNode>> expressions;
    while (!tryConsumeKeyWord("end"))
    {
        if (auto statement = parseStatement(scope))
        {
            expressions.push_back(statement);
        }
        else if (hasError())
        {
            throw ParserException(m_errors);
        }
        else
        {
            assert(false && "could not parse statement");
        }
    }

    return std::make_shared<BlockNode>(beginToken, variable_definitions, expressions);
}
std::shared_ptr<ASTNode> Parser::parseKeyword(size_t scope, bool withSemicolon)
{
    if (tryConsumeKeyWord("if"))
    {
        auto ifToken = current();

        auto condition = parseExpression(scope);
        std::vector<std::shared_ptr<ASTNode>> ifStatements;
        std::vector<std::shared_ptr<ASTNode>> elseStatements;
        consumeKeyWord("then");
        auto blockIf = canConsumeKeyWord("begin");
        if (blockIf)
        {
            ifStatements.emplace_back(parseBlock(scope + 1));
            tryConsume(TokenType::SEMICOLON);
        }
        else
        {
            ifStatements.emplace_back(parseStatement(scope, false));
        }
        if (tryConsumeKeyWord("else"))
        {
            if (canConsumeKeyWord("begin"))
            {
                elseStatements.emplace_back(parseBlock(scope + 1));
                tryConsume(TokenType::SEMICOLON);
            }
            else
            {
                elseStatements.emplace_back(parseStatement(scope));
            }
        }
        else if (!blockIf && withSemicolon)
        {
            consume(TokenType::SEMICOLON);
        }

        return std::make_shared<IfConditionNode>(ifToken, condition, ifStatements, elseStatements);
    }

    if (tryConsumeKeyWord("for"))
    {
        auto forToken = current();
        consume(TokenType::NAMEDTOKEN);
        auto loopVariable = std::string(current().lexical());
        auto loopVariableToken = current();
        if (canConsumeKeyWord("in"))
        {
            consumeKeyWord("in");
            auto loopExpression = parseBaseExpression(scope + 1);
            std::vector<std::shared_ptr<ASTNode>> forNodes;

            consumeKeyWord("do");

            if (canConsumeKeyWord("begin"))
            {
                forNodes.emplace_back(parseBlock(scope + 1));
                if (withSemicolon)
                    tryConsume(TokenType::SEMICOLON);
            }
            else
            {
                forNodes.emplace_back(parseStatement(scope));
            }
            return std::make_shared<ForEachNode>(forToken, loopVariableToken, loopExpression, forNodes);
        }

        consume(TokenType::COLON);
        consume(TokenType::EQUAL);
        auto loopStart = parseBaseExpression(scope + 1);
        int increment;
        if (tryConsumeKeyWord("to"))
        {
            increment = 1;
        }
        else if (tryConsumeKeyWord("downto"))
        {
            increment = -1;
        }
        else
        {
            m_errors.push_back(ParserError{.token = m_tokens[m_current + 1],
                                           .message = "expected keyword  'to' or 'downto' but found " +
                                                      std::string(m_tokens[m_current + 1].lexical()) + "!"});
            throw ParserException(m_errors);
        }
        auto loopEnd = parseBaseExpression(scope + 1);

        std::vector<std::shared_ptr<ASTNode>> forNodes;

        consumeKeyWord("do");

        if (canConsumeKeyWord("begin"))
        {
            forNodes.emplace_back(parseBlock(scope + 1));
            if (withSemicolon)
                tryConsume(TokenType::SEMICOLON);
        }
        else
        {
            forNodes.emplace_back(parseStatement(scope));
        }


        return std::make_shared<ForNode>(forToken, loopVariable, loopStart, loopEnd, forNodes, increment);
    }
    if (tryConsumeKeyWord("while"))
    {
        auto whileToken = current();

        auto expression = parseExpression(scope + 1);
        std::vector<std::shared_ptr<ASTNode>> whileNodes;

        consumeKeyWord("do");
        if (!canConsumeKeyWord("begin"))
        {
            whileNodes.push_back(parseStatement(scope));
            tryConsume(TokenType::SEMICOLON);
        }
        else
        {
            whileNodes.push_back(parseBlock(scope + 1));
            if (withSemicolon)
                consume(TokenType::SEMICOLON);
        }

        return std::make_shared<WhileNode>(whileToken, expression, whileNodes);
    }

    if (tryConsumeKeyWord("repeat"))
    {
        auto repeatToken = current();

        std::vector<std::shared_ptr<ASTNode>> whileNodes;
        if (!canConsumeKeyWord("begin"))
        {
            whileNodes.push_back(parseStatement(scope));
        }
        else
        {
            whileNodes.push_back(parseBlock(scope + 1));
        }
        tryConsume(TokenType::SEMICOLON);

        consumeKeyWord("until");
        auto expression = parseExpression(scope + 1);
        if (withSemicolon)
            tryConsume(TokenType::SEMICOLON);

        return std::make_shared<RepeatUntilNode>(repeatToken, expression, whileNodes);
    }

    if (tryConsumeKeyWord("break"))
    {
        if (withSemicolon)
            tryConsume(TokenType::SEMICOLON);
        return std::make_shared<BreakNode>(current());
    }

    if (tryConsumeKeyWord("case"))
    {
        auto token = current();
        auto identifier = parseToken(scope);
        if (!identifier)
        {
            m_errors.push_back(ParserError{.token = token,
                                           .message = "expected a variable name but found " + token.lexical() + "!"});
        }
        consumeKeyWord("of");
        std::vector<Selector> selectors;
        while (auto selector = parseRangeElementOrType(scope))
        {
            consume(TokenType::COLON);

            auto expression = parseStatement(scope);
            selectors.emplace_back(selector, expression);
        }
        std::vector<std::shared_ptr<ASTNode>> elseExpressions;
        if (tryConsumeKeyWord("else"))
        {
            if (!canConsumeKeyWord("begin"))
            {
                elseExpressions.push_back(parseStatement(scope));
                consumeKeyWord("end");
            }
            else
            {
                elseExpressions.push_back(parseBlock(scope + 1));
            }
        }
        else
        {
            consumeKeyWord("end");
        }
        if (withSemicolon)
            tryConsume(TokenType::SEMICOLON);

        return std::make_shared<CaseNode>(token, identifier, selectors, elseExpressions);
    }

    m_errors.push_back(
            ParserError{.token = m_tokens[m_current + 1],
                        .message = "unexpected keyword found " + std::string(m_tokens[m_current + 1].lexical()) + "!"});

    return nullptr;
}
std::shared_ptr<ASTNode> Parser::parseFunctionCall(const size_t scope)
{
    consume(TokenType::NAMEDTOKEN);
    auto nameToken = current();
    auto functionName = current().lexical();
    const bool isSysCall = isKnownSystemCall(functionName);
    if (!isSysCall && !isFunctionDeclared(functionName))
    {
        m_errors.push_back(ParserError{
                .token = current(), .message = "a function with the name '" + functionName + "' is not yet defined!"});
    }

    std::vector<std::shared_ptr<ASTNode>> callArgs;
    consume(TokenType::LEFT_CURLY);
    while (true)
    {
        if (auto arg = parseExpression(scope))
        {
            callArgs.push_back(arg);
        }
        else if (!tryConsume(TokenType::COMMA))
        {
            break;
        }
    }


    consume(TokenType::RIGHT_CURLY);
    if (isSysCall)
    {
        return std::make_shared<SystemFunctionCallNode>(nameToken, functionName, callArgs);
    }
    return std::make_shared<FunctionCallNode>(nameToken, functionName, callArgs);
}

bool Parser::importUnit(const Token &token, const std::string &filename, bool includeSystem)
{
    auto path = this->m_file_path.parent_path() / filename;
    auto it = m_rtlDirectories.begin();
    while (!std::filesystem::exists(path))
    {
        if (m_rtlDirectories.end() == it)
            break;
        path = *it / filename;
        ++it;
    }
    if (!unitCache.contains(path.string()))
    {
        std::ifstream file;
        std::istringstream is;
        file.open(path, std::ios::in);
        if (!file.is_open())
        {
            m_errors.push_back(ParserError{.token = token, .message = path.string() + " is not a valid unit"});
            return true;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        Lexer lexer;
        auto tokens = lexer.tokenize(path.string(), buffer.str());
        MacroParser macroParser(m_definitions);
        Parser parser(m_rtlDirectories, path, m_definitions, macroParser.parseFile(tokens));
        if (auto unit = parser.parseUnit(includeSystem))
            unitCache[path.string()] = std::move(unit);
        for (auto &error: parser.m_errors)
        {
            m_errors.push_back(error);
        }
        if (!m_errors.empty())
        {
            return true;
        }
    }
    for (auto &[typeName, newType]: unitCache[path.string()]->getTypeDefinitions())
    {
        if (!m_typeDefinitions.contains(typeName))
        {
            m_typeDefinitions[typeName] = newType;
        }
    }

    for (auto &definition: unitCache[path.string()]->getFunctionDefinitions())
    {
        bool functionExists = false;
        for (auto &function: m_functionDefinitions)
        {

            if (function->functionSignature() == definition->functionSignature())
            {
                functionExists = true;
                break;
            }
        }
        if (!functionExists)
        {
            m_functionDefinitions.push_back(definition);
            m_known_function_names.push_back(definition->name());
        }
    }


    return false;
}
bool Parser::isFunctionDeclared(const std::string &name) const
{
    for (const auto &function: m_functionDefinitions)
    {
        if (iequals(function->name(), name))
        {
            return true;
        }
    }
    for (const auto &function_name: m_known_function_names)
    {
        if (iequals(function_name, name))
        {
            return true;
        }
    }
    return false;
}

void Parser::parseInterfaceSection()
{
    if (tryConsumeKeyWord("uses"))
    {
        while (consume(TokenType::NAMEDTOKEN))
        {
            auto filename = std::string(current().lexical()) + ".pas";
            importUnit(current(), filename);


            if (!tryConsume(TokenType::COMMA))
                break;
        }
        consume(TokenType::SEMICOLON);
    }

    while (!canConsumeKeyWord("implementation"))
    {
        if (tryConsumeKeyWord("type"))
        {
            parseTypeDefinitions(0);
        }
        else if (tryConsumeKeyWord("procedure"))
        {
            m_functionDeclarations.emplace_back(parseFunctionDeclaration(0, false));
        }
        else if (tryConsumeKeyWord("function"))
        {
            m_functionDeclarations.emplace_back(parseFunctionDeclaration(0, true));
        }
        else if (!canConsumeKeyWord("implementation"))
        {
            m_errors.push_back(ParserError{

                    .token = m_tokens[m_current + 1],
                    .message = "unexpected token found " +
                               std::string(magic_enum::enum_name(m_tokens[m_current + 1].tokenType)) + "!"});
            break;
        }
    }
}

void Parser::parseImplementationSection(bool includeSystem)
{
    if (tryConsumeKeyWord("uses"))
    {
        while (consume(TokenType::NAMEDTOKEN))
        {
            auto filename = std::string(current().lexical()) + ".pas";
            importUnit(current(), filename, includeSystem);


            if (!tryConsume(TokenType::COMMA))
                break;
        }
        consume(TokenType::SEMICOLON);
    }

    while (!canConsumeKeyWord("end") && !canConsumeKeyWord("initialization"))
    {
        if (tryConsumeKeyWord("type"))
        {
            parseTypeDefinitions(0);
        }
        else if (tryConsumeKeyWord("procedure"))
        {
            m_functionDefinitions.emplace_back(parseFunctionDefinition(0, false));
        }
        else if (tryConsumeKeyWord("function"))
        {
            m_functionDefinitions.emplace_back(parseFunctionDefinition(0, true));
        }
        else if (!canConsumeKeyWord("end") && !canConsumeKeyWord("initialization"))
        {
            m_errors.push_back(ParserError{

                    .token = m_tokens[m_current + 1],
                    .message = "unexpected token found " +
                               std::string(magic_enum::enum_name(m_tokens[m_current + 1].tokenType)) + "!"});
            break;
        }
    }
}


std::unique_ptr<UnitNode> Parser::parseUnit(bool includeSystem)
{
    try
    {

        auto unitType = UnitType::UNIT;


        consume(TokenType::NAMEDTOKEN);
        auto unitName = std::string(current().lexical());
        auto unitNameToken = current();

        consume(TokenType::SEMICOLON);


        std::shared_ptr<BlockNode> blockNode = nullptr;
        while (hasNext())
        {
            if (tryConsumeKeyWord("interface"))
            {
                parseInterfaceSection();
            }
            else if (tryConsumeKeyWord("implementation"))
            {
                parseImplementationSection(includeSystem);
            }
            else if (tryConsumeKeyWord("end"))
            {
                consume(TokenType::DOT);
                consume(TokenType::T_EOF);
                break;
            }
            else if (tryConsume(TokenType::T_EOF))
            {
                break;
            }
            else
            {
                m_errors.push_back(ParserError{

                        .token = m_tokens[m_current + 1],
                        .message = "unexpected token found " +
                                   std::string(magic_enum::enum_name(m_tokens[m_current + 1].tokenType)) + "!"});
                break;
            }
        }

        if (hasError())
        {
            throw ParserException(m_errors);
        }

        for (auto declaration: m_functionDeclarations)
        {
            bool found = false;
            for (auto def: m_functionDefinitions)
            {
                if (def->functionSignature() == declaration->functionSignature())
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                m_functionDefinitions.emplace_back(declaration);
            }
        }


        return std::make_unique<UnitNode>(unitNameToken, unitType, unitName, m_functionDefinitions, m_typeDefinitions,
                                          blockNode);
    }
    catch (ParserException &e)
    {
    }
    return nullptr;
}


std::unique_ptr<UnitNode> Parser::parseProgram()
{
    try
    {
        UnitType unitType = UnitType::PROGRAM;


        consume(TokenType::NAMEDTOKEN);
        auto unitName = std::string(current().lexical());
        auto unitNameToken = current();
        std::vector<std::string> paramNames;

        if (tryConsume(TokenType::LEFT_CURLY))
        {
            while (canConsume(TokenType::NAMEDTOKEN))
            {
                consume(TokenType::NAMEDTOKEN);
                auto paramName = current().lexical();
                paramNames.emplace_back(paramName);
                m_known_variable_definitions.push_back(VariableDefinition{.variableType = FileType::getFileType(),
                                                                          .variableName = paramName,
                                                                          .token = current(),
                                                                          .scopeId = 0});
                tryConsume(TokenType::COMMA);
            }
            consume(TokenType::RIGHT_CURLY);
        }

        consume(TokenType::SEMICOLON);

        if (unitName != "system")
        {
            importUnit(m_tokens.front(), "system.pas", false);
        }

        std::shared_ptr<BlockNode> blockNode = nullptr;
        std::vector<VariableDefinition> variable_definitions;
        while (hasNext())
        {
            constexpr int scope = 0;
            if (tryConsumeKeyWord("type"))
            {
                parseTypeDefinitions(scope);
            }
            else if (canConsumeKeyWord("const"))
            {
                parseConstantDefinitions(scope, variable_definitions);
            }
            else if (tryConsumeKeyWord("var"))
            {
                while (!canConsume(TokenType::KEYWORD))
                {
                    auto def = parseVariableDefinitions(scope);
                    if (def.empty())
                        break;
                    for (auto &definition: def)
                    {
                        variable_definitions.push_back(definition);
                        m_known_variable_definitions.push_back(definition);
                    }
                }
            }
            else if (tryConsumeKeyWord("uses"))
            {
                while (consume(TokenType::NAMEDTOKEN))
                {
                    auto filename = std::string(current().lexical()) + ".pas";
                    importUnit(current(), filename);


                    if (!tryConsume(TokenType::COMMA))
                        break;
                }
                consume(TokenType::SEMICOLON);
            }
            else if (tryConsumeKeyWord("procedure"))
            {
                m_functionDefinitions.emplace_back(parseFunctionDefinition(scope, false));
            }
            else if (tryConsumeKeyWord("function"))
            {
                m_functionDefinitions.emplace_back(parseFunctionDefinition(scope, true));
            }
            else if (canConsumeKeyWord("const") || canConsumeKeyWord("var") || canConsumeKeyWord("begin"))
            {
                blockNode = parseBlock(scope);
                consume(TokenType::DOT);
            }
            else if (tryConsume(TokenType::T_EOF))
            {
                break;
            }
            else
            {
                m_errors.push_back(ParserError{

                        .token = m_tokens[m_current + 1],
                        .message = "unexpected token found " +
                                   std::string(magic_enum::enum_name(m_tokens[m_current + 1].tokenType)) + "!"});
                break;
            }
        }

        if (hasError())
        {
            throw ParserException(m_errors);
        }
        for (const auto &var: variable_definitions)
        {
            blockNode->addVariableDefinition(var);
        }

        return std::make_unique<UnitNode>(unitNameToken, unitType, unitName, paramNames, m_functionDefinitions,
                                          m_typeDefinitions, blockNode);
    }
    catch (ParserException &e)
    {
    }
    return nullptr;
}


std::unique_ptr<UnitNode> Parser::parseFile()
{
    const bool isProgram = current().tokenType == TokenType::KEYWORD && iequals(current().lexical(), "program");
    const bool isUnit = current().tokenType == TokenType::KEYWORD && iequals(current().lexical(), "unit");

    if (isProgram)
        return parseProgram();
    if (isUnit)
        return parseUnit(true);

    m_errors.push_back(ParserError{.token = m_tokens[m_current + 1],
                                   .message = "unexpected expected token found " +
                                              std::string(magic_enum::enum_name(m_tokens[m_current + 1].tokenType)) +
                                              "!"});
    throw ParserException(m_errors);
}
