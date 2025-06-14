#pragma once
#include <filesystem>
#include <string_view>
#include <utility>
#include "SourceLocation.h"
enum class TokenType : uint16_t
{
    NUMBER,
    STRING,
    ESCAPED_STRING,
    CHAR,
    KEYWORD,
    MACROKEYWORD,
    NAMEDTOKEN,
    PLUS,
    MINUS,
    DIV,
    MUL,
    LEFT_CURLY,
    RIGHT_CURLY,
    LEFT_SQUAR,
    RIGHT_SQUAR,
    EQUAL,
    GREATER,
    LESS,
    ENDLINE,
    T_EOF,
    COMMA,
    SEMICOLON,
    DOT,
    COLON,
    CARET,
    BANG,
    AT,
    MACRO_START,
    MACRO_END,
};


struct Token
{
    SourceLocation sourceLocation;
    size_t row{};
    size_t col{};
    TokenType tokenType;

    Token() : sourceLocation(), tokenType(TokenType::T_EOF) {}

    Token(const SourceLocation &sourceLocation, const size_t row, const size_t col, const TokenType tokenType) :
        sourceLocation(sourceLocation), row(row), col(col), tokenType(tokenType)
    {
    }

    Token(const Token &other) = default;
    Token(Token &&other) = default;

    Token &operator=(const Token &other) = default;

    [[nodiscard]] std::string lexical() const { return sourceLocation.text(); }
    bool operator==(const Token &other) const
    {
        return sourceLocation == other.sourceLocation && row == other.row && col == other.col &&
               tokenType == other.tokenType;
    }
};
