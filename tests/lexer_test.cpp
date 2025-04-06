#include "Lexer.h"
#include <gtest/gtest.h>
#include <magic_enum/magic_enum.hpp>
#include <string>
using namespace std::literals;


void PrintTo(const TokenType e, std::ostream *os) { *os << magic_enum::enum_name(e); }

TEST(LexerTest, LexHelloWorld)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(program Test;
    begin
        WriteLn('HelloWorld');
    end.)");

    EXPECT_EQ(result.size(), 12);
    ASSERT_EQ(result[0].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[0].lexical(), "program"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].lexical(), "Test"sv);
}

TEST(LexerTest, LexNumbers)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(x := (1 + 2) * 5;)");

    EXPECT_EQ(result.size(), 12);
    ASSERT_EQ(result[0].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[0].lexical(), "x"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::COLON);
    ASSERT_EQ(result[1].lexical(), ":"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[2].lexical(), "="sv);
    ASSERT_EQ(result[3].tokenType, TokenType::LEFT_CURLY);
    ASSERT_EQ(result[3].lexical(), "("sv);
    ASSERT_EQ(result[4].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[4].lexical(), "1"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::PLUS);
    ASSERT_EQ(result[5].lexical(), "+"sv);
    ASSERT_EQ(result[6].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[6].lexical(), "2"sv);
    ASSERT_EQ(result[7].tokenType, TokenType::RIGHT_CURLY);
    ASSERT_EQ(result[7].lexical(), ")"sv);
    ASSERT_EQ(result[8].tokenType, TokenType::MUL);
    ASSERT_EQ(result[8].lexical(), "*"sv);
    ASSERT_EQ(result[9].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[9].lexical(), "5"sv);
    ASSERT_EQ(result[10].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[10].lexical(), ";"sv);
    ASSERT_EQ(result[11].tokenType, TokenType::T_EOF);
}

TEST(LexerTest, LexNegNumbers)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(x := (-1 + 2) * -5;)");

    EXPECT_EQ(result.size(), 12);
    ASSERT_EQ(result[0].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[0].lexical(), "x"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::COLON);
    ASSERT_EQ(result[1].lexical(), ":"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[2].lexical(), "="sv);
    ASSERT_EQ(result[3].tokenType, TokenType::LEFT_CURLY);
    ASSERT_EQ(result[3].lexical(), "("sv);
    ASSERT_EQ(result[4].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[4].lexical(), "-1"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::PLUS);
    ASSERT_EQ(result[5].lexical(), "+"sv);
    ASSERT_EQ(result[6].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[6].lexical(), "2"sv);
    ASSERT_EQ(result[7].tokenType, TokenType::RIGHT_CURLY);
    ASSERT_EQ(result[7].lexical(), ")"sv);
    ASSERT_EQ(result[8].tokenType, TokenType::MUL);
    ASSERT_EQ(result[8].lexical(), "*"sv);
    ASSERT_EQ(result[9].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[9].lexical(), "-5"sv);
    ASSERT_EQ(result[10].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[10].lexical(), ";"sv);
    ASSERT_EQ(result[11].tokenType, TokenType::T_EOF);
}

TEST(LexerTest, LexVarDeclaration)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(var x :integer;)");

    EXPECT_EQ(result.size(), 6);
    ASSERT_EQ(result[0].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[0].lexical(), "var"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].lexical(), "x"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::COLON);
    ASSERT_EQ(result[2].lexical(), ":"sv);
    ASSERT_EQ(result[3].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[3].lexical(), "integer"sv);
    ASSERT_EQ(result[4].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[4].lexical(), ";"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::T_EOF);
}

TEST(LexerTest, LexProcedureDeclaration)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(procedure MyProc;
    Begin
        {My Comment}
    END;)");

    EXPECT_EQ(result.size(), 7);
    ASSERT_EQ(result[0].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[0].lexical(), "procedure"sv);
    ASSERT_EQ(result[0].row, 1);
    ASSERT_EQ(result[1].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].lexical(), "MyProc"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[2].lexical(), ";"sv);
    ASSERT_EQ(result[3].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[3].lexical(), "Begin"sv);
    ASSERT_EQ(result[3].row, 2);
    ASSERT_EQ(result[4].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[4].lexical(), "END"sv);
    ASSERT_EQ(result[4].row, 4);
    ASSERT_EQ(result[5].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[5].lexical(), ";"sv);

    ASSERT_EQ(result[6].tokenType, TokenType::T_EOF);
}

TEST(LexerTest, LexProcedureDeclarationWithArgs)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(procedure MyProc(arg1: integer; arg2 : real);
    Begin
        {My Comment}
    END;)");

    EXPECT_EQ(result.size(), 16);
    ASSERT_EQ(result[0].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[0].lexical(), "procedure"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].lexical(), "MyProc"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::LEFT_CURLY);
    ASSERT_EQ(result[2].lexical(), "("sv);
    ASSERT_EQ(result[3].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[3].lexical(), "arg1"sv);
    ASSERT_EQ(result[4].tokenType, TokenType::COLON);
    ASSERT_EQ(result[4].lexical(), ":"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[5].lexical(), "integer"sv);
    ASSERT_EQ(result[6].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[6].lexical(), ";"sv);

    ASSERT_EQ(result[7].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[7].lexical(), "arg2"sv);
    ASSERT_EQ(result[8].tokenType, TokenType::COLON);
    ASSERT_EQ(result[8].lexical(), ":"sv);
    ASSERT_EQ(result[9].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[9].lexical(), "real"sv);
    ASSERT_EQ(result[10].tokenType, TokenType::RIGHT_CURLY);
    ASSERT_EQ(result[10].lexical(), ")"sv);

    ASSERT_EQ(result[11].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[11].lexical(), ";"sv);
    ASSERT_EQ(result[12].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[12].lexical(), "Begin"sv);

    ASSERT_EQ(result[13].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[13].lexical(), "END"sv);
    ASSERT_EQ(result[14].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[14].lexical(), ";"sv);

    ASSERT_EQ(result[15].tokenType, TokenType::T_EOF);
}

TEST(LexerTest, LexFunctionDeclaration)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(function MyFunc : integer;
    Begin
        MyFunc := 100;
        {comment
        multiline}
    END;)");

    EXPECT_EQ(result.size(), 14);
    ASSERT_EQ(result[0].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[0].lexical(), "function"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].lexical(), "MyFunc"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::COLON);
    ASSERT_EQ(result[2].lexical(), ":"sv);
    ASSERT_EQ(result[3].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[3].lexical(), "integer"sv);

    ASSERT_EQ(result[4].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[4].lexical(), ";"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[5].lexical(), "Begin"sv);
    ASSERT_EQ(result[6].tokenType, TokenType::NAMEDTOKEN);

    ASSERT_EQ(result[6].lexical(), "MyFunc"sv);
    ASSERT_EQ(result[7].tokenType, TokenType::COLON);
    ASSERT_EQ(result[7].lexical(), ":"sv);
    ASSERT_EQ(result[8].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[8].lexical(), "="sv);
    ASSERT_EQ(result[9].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[9].lexical(), "100"sv);
    ASSERT_EQ(result[10].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[10].lexical(), ";"sv);
    ASSERT_EQ(result[11].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[11].lexical(), "END"sv);
    ASSERT_EQ(result[11].row, 6);
    ASSERT_EQ(result[12].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[12].lexical(), ";"sv);
    ASSERT_EQ(result[12].row, 6);

    ASSERT_EQ(result[13].tokenType, TokenType::T_EOF);
}
TEST(LexerTest, LexQuotedString)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(str4 := 'this is a ''quoted'' string'; )");

    EXPECT_EQ(result.size(), 6);
    ASSERT_EQ(result[0].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].tokenType, TokenType::COLON);
    ASSERT_EQ(result[2].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[3].tokenType, TokenType::STRING);
    ASSERT_EQ(result[3].lexical(), "this is a ''quoted'' string"s);
    ASSERT_EQ(result[4].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[5].tokenType, TokenType::T_EOF);
}


TEST(LexerTest, LexEscapedString)
{

    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(str4 := #13#10; )");

    EXPECT_EQ(result.size(), 6);
    ASSERT_EQ(result[0].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].tokenType, TokenType::COLON);
    ASSERT_EQ(result[2].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[3].tokenType, TokenType::ESCAPED_STRING);
    ASSERT_EQ(result[3].lexical(), "#13#10"sv);
    ASSERT_EQ(result[4].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[5].tokenType, TokenType::T_EOF);
}

TEST(LexerTest, LexPointer)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(ptr := @myvar; )");

    EXPECT_EQ(result.size(), 7);
    ASSERT_EQ(result[0].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[0].lexical(), "ptr"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::COLON);
    ASSERT_EQ(result[2].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[3].tokenType, TokenType::AT);
    ASSERT_EQ(result[3].col, 8);
    ASSERT_EQ(result[3].lexical(), "@"sv);
    ASSERT_EQ(result[4].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[4].col, 9);
    ASSERT_EQ(result[4].lexical(), "myvar"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[6].tokenType, TokenType::T_EOF);
}

TEST(LexerTest, LexUnit)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"( unit unitname;      // Name of the unit

 interface      // Everything declared here may be used by this and other units (public)

 uses myimport;


 implementation // The implementation of the requirements for this unit only (private)

 uses import2;


 initialization // Optional section: variables, data etc initialised here

 finalization   // Optional section: code executed when the program ends

 end.)");

    EXPECT_EQ(result.size(), 16);
    ASSERT_EQ(result[0].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[0].lexical(), "unit"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].lexical(), "unitname"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[2].lexical(), ";"sv);
    ASSERT_EQ(result[3].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[3].lexical(), "interface"sv);
    ASSERT_EQ(result[4].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[4].lexical(), "uses"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[5].lexical(), "myimport"sv);
    ASSERT_EQ(result[6].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[6].lexical(), ";"sv);

    ASSERT_EQ(result[7].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[7].lexical(), "implementation"sv);
    ASSERT_EQ(result[8].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[8].lexical(), "uses"sv);
    ASSERT_EQ(result[9].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[9].lexical(), "import2"sv);
    ASSERT_EQ(result[10].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[10].lexical(), ";"sv);
    ASSERT_EQ(result[11].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[11].lexical(), "initialization"sv);
    ASSERT_EQ(result[12].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[12].lexical(), "finalization"sv);

    ASSERT_EQ(result[13].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[13].lexical(), "end"sv);
    ASSERT_EQ(result[14].tokenType, TokenType::DOT);
    ASSERT_EQ(result[14].lexical(), "."sv);

    ASSERT_EQ(result[15].tokenType, TokenType::T_EOF);
}


TEST(LexerTest, LexMacroSimple)
{
    Lexer lexer;

    auto result = lexer.tokenize("filename.pas", R"(
    {$ifdef UNIX}

    {$endif}

)");

    EXPECT_EQ(result.size(), 8);
    ASSERT_EQ(result[0].tokenType, TokenType::MACRO_START);
    ASSERT_EQ(result[0].lexical(), "{$"sv);
    ASSERT_EQ(result[1].tokenType, TokenType::MACROKEYWORD);
    ASSERT_EQ(result[1].lexical(), "ifdef"sv);
    ASSERT_EQ(result[2].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[2].lexical(), "UNIX"sv);
    ASSERT_EQ(result[3].tokenType, TokenType::MACRO_END);
    ASSERT_EQ(result[4].tokenType, TokenType::MACRO_START);
    ASSERT_EQ(result[4].lexical(), "{$"sv);
    ASSERT_EQ(result[5].tokenType, TokenType::MACROKEYWORD);
    ASSERT_EQ(result[5].lexical(), "endif"sv);
    ASSERT_EQ(result[6].tokenType, TokenType::MACRO_END);


    ASSERT_EQ(result[7].tokenType, TokenType::T_EOF);
}


TEST(LexerTest, FunctionCalls)
{
    Lexer lexer;
    auto result = lexer.tokenize("filename.pas", R"(program test;
begin
    for i := 0 to high(arr) do
    begin
        stmt := i;
    end;
end.)");

    EXPECT_EQ(result.size(), 26);
    ASSERT_EQ(result[0].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[0].lexical(), "program"sv);

    ASSERT_EQ(result[1].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[1].lexical(), "test"sv);

    ASSERT_EQ(result[2].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[2].lexical(), ";"sv);

    ASSERT_EQ(result[3].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[3].lexical(), "begin"sv);

    ASSERT_EQ(result[4].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[4].lexical(), "for"sv);


    ASSERT_EQ(result[5].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[5].lexical(), "i"sv);
    ASSERT_EQ(result[5].row, 3);
    ASSERT_EQ(result[5].col, 9);

    ASSERT_EQ(result[6].tokenType, TokenType::COLON);
    ASSERT_EQ(result[6].lexical(), ":"sv);
    ASSERT_EQ(result[6].row, 3);
    ASSERT_EQ(result[6].col, 11);

    ASSERT_EQ(result[7].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[7].lexical(), "="sv);
    ASSERT_EQ(result[7].row, 3);
    ASSERT_EQ(result[7].col, 12);

    ASSERT_EQ(result[8].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[8].lexical(), "0"sv);
    ASSERT_EQ(result[8].row, 3);
    ASSERT_EQ(result[8].col, 14);

    ASSERT_EQ(result[9].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[9].lexical(), "to"sv);
    ASSERT_EQ(result[9].row, 3);
    ASSERT_EQ(result[9].col, 16);

    ASSERT_EQ(result[10].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[10].lexical(), "high"sv);
    ASSERT_EQ(result[10].row, 3);
    ASSERT_EQ(result[10].col, 19);

    ASSERT_EQ(result[11].tokenType, TokenType::LEFT_CURLY);
    ASSERT_EQ(result[11].lexical(), "("sv);
    ASSERT_EQ(result[11].row, 3);
    ASSERT_EQ(result[11].col, 23);

    ASSERT_EQ(result[12].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[12].lexical(), "arr"sv);
    ASSERT_EQ(result[12].row, 3);
    ASSERT_EQ(result[12].col, 24);
    ASSERT_EQ(result[12].sourceLocation.num_bytes, 3);

    // ASSERT_EQ(result[7].tokenType, TokenType::T_EOF);
}


TEST(LexerTest, LexFloatNumber)
{
    Lexer lexer;
    auto result = lexer.tokenize("filename.pas", R"(
    myVar := 2.55;
    myVar := -3.14;
)");
    EXPECT_EQ(result.size(), 11);
    size_t i = 0;
    ASSERT_EQ(result[i].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[i].lexical(), "myVar"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::COLON);
    ASSERT_EQ(result[i].lexical(), ":"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[i].lexical(), "="sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[i].lexical(), "2.55"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[i].lexical(), ";"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[i].lexical(), "myVar"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::COLON);
    ASSERT_EQ(result[i].lexical(), ":"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::EQUAL);
    ASSERT_EQ(result[i].lexical(), "="sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[i].lexical(), "-3.14"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::SEMICOLON);
    ASSERT_EQ(result[i].lexical(), ";"sv);
}

TEST(LexerTest, ParseArray)
{
    //        buffer: array [0..100] of char;
    Lexer lexer;
    auto result = lexer.tokenize("filename.pas", R"(
    buffer: array [0..100] of char;
)");
    EXPECT_EQ(result.size(), 13);
    size_t i = 0;
    ASSERT_EQ(result[i].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[i].lexical(), "buffer"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::COLON);
    ASSERT_EQ(result[i].lexical(), ":"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[i].lexical(), "array"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::LEFT_SQUAR);
    ASSERT_EQ(result[i].lexical(), "["sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[i].lexical(), "0"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::DOT);
    ASSERT_EQ(result[i].lexical(), "."sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::DOT);
    ASSERT_EQ(result[i].lexical(), "."sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::NUMBER);
    ASSERT_EQ(result[i].lexical(), "100"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::RIGHT_SQUAR);
    ASSERT_EQ(result[i].lexical(), "]"sv);
}

TEST(LexerTest, CaseTest)
{
    Lexer lexer;
    auto result = lexer.tokenize("filename.pas", R"(
        case color of
            RED: writeln('red');
        end;
            )");

    EXPECT_EQ(result.size(), 13);
    size_t i = 0;
    ASSERT_EQ(result[i].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[i].lexical(), "case"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::NAMEDTOKEN);
    ASSERT_EQ(result[i].lexical(), "color"sv);
    i++;
    ASSERT_EQ(result[i].tokenType, TokenType::KEYWORD);
    ASSERT_EQ(result[i].lexical(), "of"sv);
    i++;
}
