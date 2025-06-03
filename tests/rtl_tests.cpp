#include <algorithm>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <utility>
#include "compiler/Compiler.h"

#include "os/command.h"

using namespace std::literals;

class RtlTest : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite() { init_compiler(); }
};

class CompilerTestError : public testing::TestWithParam<std::string>
{
public:
    static void SetUpTestSuite() { init_compiler(); }
};


TEST_P(RtlTest, StringTests)
{
    // Inside a test, access the test parameter with the GetParam() method
    // of the TestWithParam<T> class:
    std::filesystem::path base_path = "rtl";
    base_path /= "strings";
    const auto& name = GetParam();
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


INSTANTIATE_TEST_SUITE_P(StringTests, RtlTest, testing::Values("strcat"));
