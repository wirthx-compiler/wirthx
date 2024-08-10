#pragma once
#include <string_view>
#include <vector>
#include "Token.h"

inline auto possible_tokens = {"program", "unit",  "uses", "begin", "end",   "procedure", "function",
                               "var",     "if",    "then", "else",  "while", "do",        "for",
                               "to",      "break", "type", "array", "of",    "const"};

class Lexer
{
private:
    bool find_fixed_token(std::string_view content, size_t start, size_t *endPosition);
    bool find_token(std::string_view content, size_t start, size_t *endPosition);

    bool find_string(std::string_view content, size_t start, size_t *endPosition);
    bool find_number(std::string_view content, size_t start, size_t *endPosition);
    bool find_comment(std::string_view content, size_t start, size_t *endPosition);

public:
    Lexer();
    ~Lexer();

    std::vector<Token> tokenize(std::string_view content);
};
