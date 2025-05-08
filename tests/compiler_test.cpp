#include "compiler/Compiler.h"
#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <utility>

#include "os/command.h"

using namespace std::literals;

class CompilerTest : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite() { init_compiler(); }
};

class ProjectEulerTest : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite() { init_compiler(); }
};

class CompilerTestError : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite() { init_compiler(); }
};

class WriteToStdErrTest : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite() { init_compiler(); }
};

TEST_P(CompilerTest, TestNoError)
{
    // Inside a test, access the test parameter with the GetParam() method
    // of the TestWithParam<T> class:
    std::filesystem::path base_path = "testfiles";
    auto name = GetParam();
    std::filesystem::path input_path = base_path / (name + ".pas");
    std::filesystem::path output_path = base_path / (name + ".txt");
    std::cerr << "current path" << std::filesystem::current_path();

    if (!std::filesystem::exists(input_path))
        std::cerr << "absolute input path: " << std::filesystem::absolute(input_path);
    if (!std::filesystem::exists(output_path))
        std::cerr << "absolute input path: " << std::filesystem::absolute(output_path);
    ASSERT_TRUE(std::filesystem::exists(input_path));
    ASSERT_TRUE(std::filesystem::exists(output_path));
    std::stringstream ostream;
    std::stringstream erstream;
    CompilerOptions options;
    options.rtlDirectories.emplace_back("rtl");

    options.runProgram = true;
    options.buildMode = BuildMode::Debug;
    options.outputDirectory = std::filesystem::current_path();
    compile_file(options, input_path, erstream, ostream);

    std::ifstream file;
    std::istringstream is;
    std::string s;
    std::string group;

    file.open(output_path, std::ios::in);

    if (!file.is_open())
    {
        std::cerr << input_path.string() << "\n";
        std::cerr << std::filesystem::absolute(input_path);
        FAIL();
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto expected = buffer.str();
    std::string result = ostream.str();

    result.erase(std::ranges::remove(result, '\r').begin(), result.end());
    if (result != expected)
    {
        std::cout << "expected: " << expected;
        std::cout << result << "\n";
    }


    ASSERT_EQ(erstream.str(), "");
    ASSERT_EQ(result, expected);
}

TEST_P(ProjectEulerTest, TestNoError)
{
    // Inside a test, access the test parameter with the GetParam() method
    // of the TestWithParam<T> class:
    std::filesystem::path base_path = "projecteuler";
    auto name = GetParam();
    std::filesystem::path input_path = base_path / (name + ".pas");
    std::filesystem::path output_path = base_path / (name + ".txt");
    ASSERT_TRUE(std::filesystem::exists(input_path));
    ASSERT_TRUE(std::filesystem::exists(output_path));
    std::stringstream ostream;
    std::stringstream erstream;
    CompilerOptions options;
    options.rtlDirectories.emplace_back("rtl");

    options.buildMode = BuildMode::Release;

    options.runProgram = true;
    options.outputDirectory = std::filesystem::current_path();
    compile_file(options, input_path, erstream, ostream);

    std::ifstream file;
    std::istringstream is;
    std::string s;
    std::string group;


    file.open(output_path, std::ios::in);

    if (!file.is_open())
    {
        std::cerr << input_path.string() << "\n";
        std::cerr << std::filesystem::absolute(input_path);
        FAIL();
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto expected = buffer.str();
    std::string result = ostream.str();
    result.erase(std::ranges::remove(result, '\r').begin(), result.end());
    std::cout << "current path" << std::filesystem::current_path();
    std::cout << "expected: " << expected;
    std::cout << ostream.str() << "\n";
    ASSERT_EQ(erstream.str(), "");
    ASSERT_EQ(result, expected);
    ASSERT_GT(ostream.str().size(), 0);
}

TEST_P(CompilerTestError, CompilerTestWithError)
{
    // Inside a test, access the test parameter with the GetParam() method
    // of the TestWithParam<T> class:
    std::filesystem::path base_path = "errortests";
    auto name = GetParam();
    std::filesystem::path input_path = base_path / (name + ".pas");
    std::filesystem::path output_path = base_path / (name + ".txt");
    ASSERT_TRUE(std::filesystem::exists(input_path));
    ASSERT_TRUE(std::filesystem::exists(output_path));
    std::stringstream ostream;
    std::stringstream erstream;
    CompilerOptions options;
    options.rtlDirectories.emplace_back("rtl");
    options.colorOutput = false;
    compile_file(options, input_path, erstream, ostream);

    std::ifstream file;
    std::istringstream is;
    std::string s;
    std::string group;

    file.open(output_path, std::ios::in);

    if (!file.is_open())
    {
        std::cerr << input_path.string() << "\n";
        std::cerr << std::filesystem::absolute(input_path);
        FAIL();
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto expected = buffer.str();
    std::string placeholder = "FILENAME";
    while (expected.find(placeholder) != std::string::npos)
        expected = expected.replace(expected.find(placeholder), placeholder.size(), input_path.string());
    std::cout << "expected: " << expected;
    std::cout << ostream.str() << "\n";
    ASSERT_EQ(erstream.str(), expected);
    ASSERT_EQ(ostream.str(), "");
}

TEST_P(WriteToStdErrTest, WriteToStdErrTest)
{
    // Inside a test, access the test parameter with the GetParam() method
    // of the TestWithParam<T> class:
    std::filesystem::path base_path = "testfiles";
    base_path /= "stderr"s;
    auto name = GetParam();
    std::filesystem::path input_path = base_path / (name + ".pas");
    std::filesystem::path output_path = base_path / (name + ".txt");
    std::cerr << "current path" << std::filesystem::current_path() << "\n";
    ;

    if (!std::filesystem::exists(input_path))
        std::cerr << "absolute input path: " << std::filesystem::absolute(input_path) << "\n";
    ;
    if (!std::filesystem::exists(output_path))
        std::cerr << "absolute input path: " << std::filesystem::absolute(output_path) << "\n";
    ;
    ASSERT_TRUE(std::filesystem::exists(input_path));
    ASSERT_TRUE(std::filesystem::exists(output_path));
    std::stringstream ostream;
    std::stringstream erstream;
    CompilerOptions options;
    options.rtlDirectories.emplace_back("rtl");

    options.runProgram = true;
    options.buildMode = BuildMode::Release;
    options.outputDirectory = std::filesystem::current_path();
    compile_file(options, input_path, erstream, ostream);

    std::ifstream file;
    std::istringstream is;
    std::string s;
    std::string group;

    file.open(output_path, std::ios::in);

    if (!file.is_open())
    {
        std::cerr << input_path.string() << "\n";
        std::cerr << std::filesystem::absolute(input_path) << "\n";
        FAIL();
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto expected = buffer.str();
    std::string result = erstream.str();

    result.erase(std::ranges::remove(result, '\r').begin(), result.end());
    if (result != expected)
    {
        std::cout << "expected: " << expected;
        std::cout << result << "\n";
    }


    ASSERT_EQ(ostream.str(), "");
    ASSERT_EQ(result, expected);
}


INSTANTIATE_TEST_SUITE_P(CompilerTestNoError, CompilerTest,
                         testing::Values("helloworld", "functions", "math", "includetest", "whileloop", "conditions",
                                         "forloop", "arraytest", "constantstest", "customint", "logicalcondition",
                                         "basicvec2", "dynarray", "externalfunction", "stringtest", "readfile",
                                         "repeatuntil", "stringcompare", "pointer_test", "rule110", "positive_assert",
                                         "stringconv", "singletest", "doubletest", "exittest", "stringreturn",
                                         "enumtest", "rangetypetest", "casetest", "forintest"));

INSTANTIATE_TEST_SUITE_P(CompilerTestWithError, CompilerTestError,
                         testing::Values("arrayaccess", "missing_return_type", "wrong_return_type", "parsing_errors"));

INSTANTIATE_TEST_SUITE_P(ProjectEuler, ProjectEulerTest,
                         testing::Values("problem1", "problem2", "problem3", "problem4", "problem5", "problem6",
                                         "problem7", "problem8", "problem9", "problem10"));

INSTANTIATE_TEST_SUITE_P(WriteToStdErrTest, WriteToStdErrTest, testing::Values("writetoerror"));
