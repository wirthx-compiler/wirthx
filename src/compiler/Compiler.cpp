#include "compiler/Compiler.h"

#include <MacroParser.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "Lexer.h"
#include "Parser.h"
#include "ast/FunctionDefinitionNode.h"
#include "ast/UnitNode.h"

#include "compiler/Context.h"
#include "compiler/intrinsics.h"
#include "linker/pascal_linker.h"
#include "llvm/IR/PassManager.h"

#include "llvm/IR/LegacyPassManager.h"

#include "llvm/IR/Verifier.h"
#include "llvm/LTO/LTO.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "os/command.h"

static auto TargetTriple = llvm::sys::getDefaultTargetTriple();


void init_compiler()
{
    using namespace llvm;
    using namespace llvm::sys;
    // Initialize the target registry etc.InitializeAllTargetInfos();
    // InitializeAllTargets();
    // InitializeAllTargetMCs();
    // InitializeAllAsmParsers();
    // InitializeAllAsmPrinters();

    InitializeNativeTarget();
    InitializeNativeTargetAsmParser();
    InitializeNativeTargetAsmPrinter();
}
void compile_file(const CompilerOptions &options, const std::filesystem::path &inputPath, std::ostream &errorStream,
                  std::ostream &outputStream)
{
    std::ifstream file;
    std::istringstream is;

    file.open(inputPath, std::ios::in);
    if (!file.is_open())
    {
        return;
    }
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    std::string buffer(size, ' ');
    file.seekg(0);
    file.read(&buffer[0], size);


    using namespace llvm;
    using namespace llvm::sys;


    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    // Print an error and exit if we couldn't find the requested target.
    // This generally occurs if we've forgotten to initialise the
    // TargetRegistry or we have a bogus target triple.
    if (!Target)
    {
        errs() << Error;
        return;
    }

    auto CPU = "generic";
    auto Features = "";

    TargetOptions opt;
    auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, Reloc::PIC_);
    Triple target(TargetTriple);
    Lexer lexer;
    MacroMap defines;
    switch (target.getOS())
    {
        case Triple::Darwin:
        case Triple::Linux:
        case Triple::OpenBSD:
        case Triple::FreeBSD:
            defines["UNIX"] = true;
            break;
        case Triple::Win32:
            defines["WINDOWS"] = true;
            break;
        default:
            break;
    }
    defines.insert(std::make_pair(target.getArchName(), true));


    auto tokens = lexer.tokenize(inputPath.string(), buffer);
    MacroParser macroParser(defines);
    Parser parser(options.rtlDirectories, inputPath, macroParser.macroDefinitions(), macroParser.parseFile(tokens));
    auto unit = parser.parseFile();
    if (parser.hasError())
    {
        parser.printErrors(errorStream, options.colorOutput);
        return;
    }
    if (parser.hasMessages())
    {
        parser.printErrors(errorStream, options.colorOutput);
    }
    auto context = std::make_unique<Context>(unit, options, TargetTriple);
    auto intType = VariableType::getInteger();
    auto int64Type = VariableType::getInteger(64);
    auto int8Type = VariableType::getInteger(8);
    auto pCharType = ::PointerType::getPointerTo(VariableType::getInteger(8));
    context->module()->setDataLayout(TheTargetMachine->createDataLayout());

    createSystemCall(context, "exit", {FunctionArgument{.type = intType, .argumentName = "X", .isReference = false}});
    createSystemCall(context, "fflush",
                     {FunctionArgument{.type = VariableType::getPointer(), .argumentName = "X", .isReference = false}});
    createSystemCall(context, "fopen",
                     {FunctionArgument{.type = pCharType, .argumentName = "filename"},
                      {FunctionArgument{.type = pCharType, .argumentName = "modes"}}},
                     ::PointerType::getUnqual());

    createReAllocCall(context);
    createSystemCall(context, "fclose", {FunctionArgument{.type = ::PointerType::getUnqual(), .argumentName = "file"}},
                     intType);
    // ssize_t getline(char **lineptr, size_t *n, FILE *stream);
    createSystemCall(context, "fgetc", {FunctionArgument{.type = ::PointerType::getUnqual(), .argumentName = "file"}},
                     int8Type);

    createSystemCall(context, "fwrite",
                     {FunctionArgument{.type = ::PointerType::getUnqual(), .argumentName = "buffer"},
                      FunctionArgument{.type = int64Type, .argumentName = "size"},
                      FunctionArgument{.type = int64Type, .argumentName = "count"},
                      FunctionArgument{.type = ::PointerType::getUnqual(), .argumentName = "file"}},
                     int64Type);

    if (target.getOS() == Triple::Linux)
    {
        createSystemCall(context, "__assert_fail",
                         {FunctionArgument{.type = pCharType, .argumentName = "assertion", .isReference = false},
                          FunctionArgument{.type = pCharType, .argumentName = "filename", .isReference = false},
                          FunctionArgument{.type = intType, .argumentName = "line", .isReference = false},
                          FunctionArgument{.type = pCharType, .argumentName = "function", .isReference = false}});
    }
    else if (target.getOS() == Triple::Win32)
    {
        createSystemCall(context, "_assert",
                         {FunctionArgument{.type = pCharType, .argumentName = "assertion", .isReference = false},
                          FunctionArgument{.type = pCharType, .argumentName = "filename", .isReference = false},
                          FunctionArgument{.type = intType, .argumentName = "line", .isReference = false},
                          FunctionArgument{.type = pCharType, .argumentName = "function", .isReference = false}});
        createSystemCall(context, "__acrt_iob_func",
                         {FunctionArgument{.type = intType, .argumentName = "line", .isReference = false}},
                         ::PointerType::getUnqual());
    }


    createPrintfCall(context);
    createFPrintfCall(context);
    createAssignCall(context);
    createResetCall(context);
    createRewriteCall(context);
    createCloseFileCall(context);
    createReadLnCall(context);

    try
    {
        context->programUnit()->typeCheck(context->programUnit(), nullptr);
        context->programUnit()->codegen(context);
    }
    catch (CompilerException &e)
    {
        errorStream << e.what();
        return;
    }


    auto basePath = context->options().outputDirectory;


    auto objectFileName = basePath / (context->programUnit()->getUnitName() + ".o");
    std::vector<std::string> objectFiles;
    objectFiles.emplace_back(objectFileName.string());
    std::error_code EC;
    raw_fd_ostream dest(objectFileName.string(), EC, sys::fs::OF_None);

    if (EC)
    {
        errs() << "Could not open file: " << EC.message();
        return;
    }

    legacy::PassManager pass;
    if (context->options().buildMode == BuildMode::Release)
    {
        pass.add(llvm::createAlwaysInlinerLegacyPass());
        pass.add(llvm::createInstructionCombiningPass());
        TheTargetMachine->setOptLevel(CodeGenOptLevel::Aggressive);
    }
    else
    {
        TheTargetMachine->setOptLevel(CodeGenOptLevel::None);
    }


    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, CodeGenFileType::ObjectFile))
    {
        errs() << "TheTargetMachine can't emit a file of this type";
        return;
    }

    pass.run(*context->module());
    dest.flush();
    dest.close();


    llvm::verifyModule(*context->module(), &llvm::errs());
    if (context->options().printLLVMIR)
    {
        context->module()->print(llvm::errs(), nullptr, false, false);
    }

    outs() << "Wrote " << objectFileName.string() << "\n";

    std::vector<std::string> flags;
    for (const auto &lib: context->programUnit()->collectLibsToLink())
    {
        flags.push_back("-l" + lib);
    }


    if (context->options().buildMode == BuildMode::Debug && target.getOS() != Triple::Win32)
    {
        flags.emplace_back("-fsanitize=address");
        flags.emplace_back("-fno-omit-frame-pointer");
    }

    std::string executableName = context->programUnit()->getUnitName();

    if (target.getOS() == Triple::Win32)
    {
        executableName += ".exe";
        flags.erase(std::ranges::find(flags, "-lc"));
    }

    if (!pascal_link_modules(errorStream, basePath, executableName, flags, objectFiles))
    {
        return;
    }

    if (context->options().runProgram)
    {

        if (!execute_command(outputStream, errorStream, (basePath / executableName).string()))
        {
            errorStream << "program could not be executed!\n";
        }
    }
}
