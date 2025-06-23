#include "CompilerOptions.h"
#include <filesystem>

std::string shiftarg(std::vector<std::string> &args)
{
    auto result = args.front();
    args.erase(args.begin());
    return result;
}


CompilerOptions parseCompilerOptions(std::vector<std::string> &argList)
{
    using namespace std::literals;
    CompilerOptions options;
    options.outputDirectory = std::filesystem::current_path().string();
    options.compilerPath = shiftarg(argList);

    while (!argList.empty())
    {
        auto arg = shiftarg(argList);
        if (arg == "--ast"sv)
        {
            options.printAST = true;
        }
        else if (arg == "--llvm-ir"sv)
        {
            options.printLLVMIR = true;
        }
        else if (arg == "--run"sv or arg == "-r"sv)
        {
            options.runProgram = true;
        }
        else if (arg == "--output"sv)
        {
            options.outputDirectory = shiftarg(argList);
        }
        else if (arg == "--rtl"sv)
        {
            options.rtlDirectories.emplace(options.rtlDirectories.begin(), shiftarg(argList));
        }
        else if (arg == "-c")
        {
            options.option = CompileOption::COMPILE;
        }
        else if (arg == "--release")
        {
            options.buildMode = BuildMode::Release;
        }
        else if (arg == "--debug")
        {
            options.buildMode = BuildMode::Debug;
        }
        else if (arg == "--lsp")
        {
            options.lsp = true;
        }
        else
        {
            argList.push_back(arg);
            break;
        }
    }

    if (const char *env_p = std::getenv("WIRTHX_PATH"))
    {
        options.rtlDirectories.emplace_back(env_p + "/rtl"s);
    }
    else if (options.rtlDirectories.empty())
    {
        options.rtlDirectories.push_back(std::filesystem::current_path() / "rtl");
    }

    return options;
}
