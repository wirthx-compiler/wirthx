//
// Created by stefan on 02.03.25.
//

#include "LanguageServer.h"

#include <future>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Endian.h>

#include <iostream>
#include <llvm/Support/JSON.h>
#include <utility>
#include "Lexer.h"
#include "MacroParser.h"
#include "Parser.h"
#include "ast/VariableAccessNode.h"
#include "ast/VariableAssignmentNode.h"


LanguageServer::LanguageServer(CompilerOptions options) : m_options(std::move(options)) {}
void sendNotification(const std::string &method)
{
    std::string resultString;

    llvm::raw_string_ostream sstream(resultString);
    llvm::json::Object response;
    response["jsonrpc"] = "2.0";
    response["method"] = method;
    llvm::json::Value resultValue(std::move(response));
    sstream << resultValue;
    std::cerr << "TEST: " << sstream.str() << "\n";
    std::cout << "Content-Length: " << sstream.str().length() << "\r\n\r\n";
    std::cout << sstream.str() << "\n";
}

llvm::json::Object buildPosition(const size_t line, const size_t character)
{
    llvm::json::Object response;
    response["line"] = line - 1;
    response["character"] = character - 1;
    return response;
}

llvm::json::Object buildColor(TokenType token)
{
    llvm::json::Object response;
    response["red"] = 1.0;
    response["green"] = 0.0;
    response["blue"] = 1.0;
    response["alpha"] = 1.0;
    return response;
}
void sendLogMessage(const std::vector<std::string> &messages)
{
    if (messages.empty())
        return;
    llvm::json::Array logMessages;
    for (auto &message: messages)
    {
        llvm::json::Object logMessage;
        logMessage["message"] = message;
        logMessage["type"] = 2;
        logMessages.push_back(std::move(logMessage));
    }
    llvm::json::Object response;
    response["jsonrpc"] = "2.0";
    response["method"] = "window/logMessage";
    response["params"] = std::move(logMessages);
    llvm::json::Value resultValue(std::move(response));
    std::string resultString;

    llvm::raw_string_ostream sstream(resultString);
    sstream << resultValue;
    // std::cerr << "TEST: " << sstream.str() << "\n";
    std::cout << "Content-Length: " << sstream.str().length() << "\r\n\r\n";
    std::cout << sstream.str();
}

constexpr int mapOutputTypeToSeverity(const OutputType output)
{
    switch (output)
    {
        case OutputType::ERROR:
            return 1;
        case OutputType::HINT:
            return 4;
        case OutputType::WARN:
            return 2;
    }
    return 0;
}
void sentDiagnostics(std::map<std::string, std::vector<ParserError>> errorsMap)
{
    if (errorsMap.empty())
        return;

    // group messages by file


    for (auto &[fileName, messsages]: errorsMap)
    {
        llvm::json::Array logMessages;
        for (const auto &[outputType, token, message]: messsages)
        {
            llvm::json::Object logMessage;
            llvm::json::Object range;
            range["start"] = buildPosition(token.row, token.col);
            range["end"] = buildPosition(token.row, token.col + token.sourceLocation.num_bytes);
            logMessage["range"] = std::move(range);
            logMessage["severity"] = mapOutputTypeToSeverity(outputType);
            logMessage["message"] = message;
            llvm::json::Array relatedInformations;
            llvm::json::Object source;
            llvm::json::Object location;
            location["uri"] = token.sourceLocation.filename;
            llvm::json::Object range2;
            range2["start"] = buildPosition(token.row, token.col);
            range2["end"] = buildPosition(token.row, token.col + token.sourceLocation.num_bytes);
            location["range"] = std::move(range2);
            source["location"] = std::move(location);
            source["message"] = message;
            source["source"] = "wirthx";
            relatedInformations.push_back(std::move(source));

            logMessage["relatedInformation"] = std::move(relatedInformations);
            logMessages.push_back(std::move(logMessage));
        }
        llvm::json::Object response;
        response["jsonrpc"] = "2.0";
        response["method"] = "textDocument/publishDiagnostics";
        llvm::json::Array diagnosticsParams;
        {
            llvm::json::Object PublishDiagnosticsParams;
            PublishDiagnosticsParams["uri"] = fileName;
            PublishDiagnosticsParams["diagnostics"] = std::move(logMessages);
            diagnosticsParams.push_back(std::move(PublishDiagnosticsParams));
        }
        response["params"] = std::move(diagnosticsParams);
        llvm::json::Value resultValue(std::move(response));
        std::string resultString;

        llvm::raw_string_ostream sstream(resultString);
        sstream << resultValue;
        std::cerr << "TEST: " << sstream.str() << "\n";
        std::cout << "Content-Length: " << sstream.str().length() << "\r\n\r\n";
        std::cout << sstream.str();
    }
}

void parseAndSendDiagnostics(std::vector<std::filesystem::path> rtlDirectories, llvm::StringRef uri,
                             llvm::StringRef text)
{
    std::map<std::string, std::vector<ParserError>> errorsMap;
    Lexer lexer;
    auto tokens = lexer.tokenize(uri.str(), text.str());
    std::filesystem::path filePath = uri.str();
    std::unordered_map<std::string, bool> definitions;
    MacroParser macro_parser(definitions);
    Parser parser(rtlDirectories, filePath, definitions, macro_parser.parseFile(tokens));
    auto ast = parser.parseFile();
    if (!parser.hasMessages())
    {
        errorsMap[filePath.string()] = {};
    }
    else
    {
        for (auto &error: parser.getErrors())
        {
            errorsMap[error.token.sourceLocation.filename].push_back(error);
        }
    }
    sentDiagnostics(errorsMap);
}
llvm::json::Object buildLocationFromToken(const Token &expressionToken)
{
    llvm::json::Object location;
    auto filePath = expressionToken.sourceLocation.filename;
    if (filePath.starts_with("file://"))
    {
        location["uri"] = filePath;
    }
    else
    {
        location["uri"] = "file://" + filePath;
    }
    llvm::json::Object range;
    range["start"] = buildPosition(expressionToken.row, expressionToken.col);
    range["end"] = buildPosition(expressionToken.row, expressionToken.col + expressionToken.sourceLocation.num_bytes);
    location["range"] = std::move(range);
    return location;
}
llvm::json::Object buildError(const char *message, const int errorCode)
{
    llvm::json::Object error;
    error["code"] = errorCode;
    error["message"] = message;
    error["data"] = "An internal error occurred while processing the request.";
    return error;
}
bool tokenInRange(const Token &token, size_t line, size_t character)
{
    return token.row == line + 1 && character + 1 >= token.col &&
           character + 1 <= token.col + token.sourceLocation.num_bytes;
}
void LanguageServer::handleRequest()
{
    std::vector<std::string> logMessages;
    std::map<std::string, std::vector<ParserError>> errorsMap;

    while (true)
    {
        logMessages.clear();


        std::string commandString;
        getline(std::cin, commandString);
        auto length = std::atoi(commandString.substr(16).c_str());
        getline(std::cin, commandString); // empty line
        std::vector<char> buffer;
        buffer.resize(length);
        std::cin.read(&buffer[0], length);
        commandString = std::string(buffer.begin(), buffer.end());

        auto request = llvm::json::parse(commandString);
        if (!request)
        {
            logMessages.emplace_back("Failed to parse request");
            logMessages.emplace_back(" command: " + commandString);
            continue;
        }
        else
        {
            // std::cerr << "command: " << commandString << std::endl;
        }

        auto requestObject = request.get().getAsObject();
        if (auto method = requestObject->getString("method"))
        {
            try
            {
                std::string resultString;
                std::cerr << "command: " << commandString << std::endl;
                logMessages.push_back("method: " + method.value().str());
                llvm::raw_string_ostream sstream(resultString);
                llvm::json::Object response;
                response["jsonrpc"] = "2.0";
                bool hasId = false;
                if (requestObject->getInteger("id").has_value())
                {
                    response["id"] = requestObject->getInteger("id").value();
                    hasId = true;
                }
                else if (requestObject->getString("id").has_value())
                {
                    response["id"] = requestObject->getString("id").value();
                    hasId = true;
                }
                llvm::json::Object result;
                if (method.value() == "shutdown")
                {
                    // sendNotification("exit");
                    return;
                }
                if (method.value() == "initialize")
                {
                    llvm::json::Object capabilities;
                    capabilities["documentHighlightProvider"] = false;
                    capabilities["documentSymbolProvider"] = true;
                    capabilities["colorProvider"] = false;
                    llvm::json::Object diagnosticProvider;
                    diagnosticProvider["interFileDependencies"] = true;
                    diagnosticProvider["workspaceDiagnostics"] = false;
                    capabilities["diagnosticProvider"] = std::move(diagnosticProvider);
                    llvm::json::Object textDocumentSync;
                    textDocumentSync["openClose"] = true;
                    textDocumentSync["change"] = 1;
                    capabilities["textDocumentSync"] = std::move(textDocumentSync);
                    capabilities["declarationProvider"] = true;
                    capabilities["definitionProvider"] = true;

                    result["capabilities"] = std::move(capabilities);

                    llvm::json::Object completionProvider;
                    std::vector<std::string> triggerCharacters = {"."};
                    completionProvider["triggerCharacters"] = triggerCharacters;
                    result["completionProvider"] = std::move(completionProvider);

                    llvm::json::Object serverInfo;
                    serverInfo["name"] = "wirthx";
                    serverInfo["version"] = "0.1";
                    result["serverInfo"] = std::move(serverInfo);
                    response["result"] = std::move(result);
                }
                else if (method.value() == "workspace/didChangeConfiguration")
                {
                    std::cerr << "command: " << commandString << std::endl;
                }
                else if (method.value() == "initialized")
                {
                    response["result"] = std::move(result);
                    continue;
                }
                else if (method.value() == "textDocument/didOpen")
                {
                    auto params = requestObject->getObject("params");
                    auto uri = params->getObject("textDocument")->getString("uri");

                    auto text = params->getObject("textDocument")->getString("text");
                    m_openDocuments[uri.value().str()] =
                            LspDocument{.uri = uri.value().str(), .text = text.value().str()};
                    response["result"] = std::move(result);
                    std::ignore = std::async(std::launch::async, [rtlDirectories = this->m_options.rtlDirectories,
                                                                  uri = uri.value(), text = text.value()]()
                                             { parseAndSendDiagnostics(rtlDirectories, uri, text); });
                }
                else if (method.value() == "textDocument/didClose")
                {
                    auto params = requestObject->getObject("params");
                    auto uri = params->getObject("textDocument")->getString("uri");
                    m_openDocuments.erase(uri.value().str());
                }
                else if (method.value() == "textDocument/didChange")
                {
                    auto params = requestObject->getObject("params");
                    auto uri = params->getObject("textDocument")->getString("uri");

                    auto text = params->getArray("contentChanges")->front().getAsObject()->getString("text");
                    m_openDocuments[uri.value().str()] =
                            LspDocument{.uri = uri.value().str(), .text = text.value().str()};
                    response["result"] = std::move(result);
                    std::ignore = std::async(std::launch::async, [rtlDirectories = this->m_options.rtlDirectories,
                                                                  uri = uri.value(), text = text.value()]()
                                             { parseAndSendDiagnostics(rtlDirectories, uri, text); });
                }
                else if (method.value() == "textDocument/documentHighlight")
                {
                    std::cerr << " command: " << commandString << "\n";

                    auto params = requestObject->getObject("params");
                    auto uri = params->getObject("textDocument")->getString("uri");
                    auto &document = m_openDocuments.at(uri.value().str());
                    Lexer lexer;
                    auto tokens = lexer.tokenize(uri.value().str(), document.text);
                    llvm::json::Array array;


                    response["result"] = std::move(array);
                }
                else if (method.value() == "textDocument/documentColor")
                {
                    // auto params = requestObject->getObject("params");
                    //  auto uri = params->getObject("textDocument")->getString("uri");


                    llvm::json::Array array;

                    response["result"] = std::move(array);
                }
                else if (method.value() == "textDocument/definition")
                {
                    auto params = requestObject->getObject("params");
                    auto uri = params->getObject("textDocument")->getString("uri");
                    auto position = params->getObject("position");
                    size_t line = position->getInteger("line").value();
                    size_t character = position->getInteger("character").value();
                    auto &document = m_openDocuments.at(uri.value().str());
                    std::filesystem::path filePath = uri.value().str();

                    Lexer lexer;
                    auto tokens = lexer.tokenize(filePath.string(), document.text);
                    std::unordered_map<std::string, bool> definitions;

                    MacroParser macro_parser(definitions);
                    Parser parser(this->m_options.rtlDirectories, filePath, definitions,
                                  macro_parser.parseFile(tokens));
                    auto ast = parser.parseFile();
                    bool found = false;
                    for (auto &token: tokens)
                    {
                        if (tokenInRange(token, line, character + 1))
                        {


                            if (token.tokenType == TokenType::NAMEDTOKEN)
                            {
                                auto resultPair = ast->getNodeByToken(token);
                                if (resultPair.has_value())
                                {
                                    std::cerr << " node found for token: " << token.lexical() << "\n";

                                    auto [parent, node] = resultPair.value();
                                    if (const auto function = dynamic_cast<const FunctionDefinitionNode *>(parent))
                                    {
                                        if (auto varDefinition =
                                                    function->body()->getVariableDefinition(token.lexical()))
                                        {
                                            std::cerr << "Found variable definition: " << varDefinition->token.lexical()
                                                      << "\n";

                                            llvm::json::Object location = buildLocationFromToken(varDefinition->token);
                                            response["result"] = std::move(location);
                                            found = true;
                                            break;
                                        }
                                        if (auto param = function->getParam(token.lexical()); param.has_value())
                                        {

                                            llvm::json::Object location = buildLocationFromToken(param->token);
                                            response["result"] = std::move(location);
                                            found = true;
                                            break;
                                        }
                                    }
                                    else if (auto unit = dynamic_cast<const UnitNode *>(parent))
                                    {
                                        if (auto varDefinition = unit->getVariableDefinition(token.lexical()))
                                        {
                                            std::cerr << "Found variable definition: " << varDefinition->token.lexical()
                                                      << "\n";

                                            llvm::json::Object location = buildLocationFromToken(varDefinition->token);
                                            response["result"] = std::move(location);
                                            found = true;
                                            break;
                                        }
                                    }
                                }

                                if (auto functionDefinition = ast->getFunctionDefinitionByName(token.lexical()))
                                {

                                    auto expressionToken = functionDefinition.value()->expressionToken();
                                    llvm::json::Object location = buildLocationFromToken(expressionToken);
                                    response["result"] = std::move(location);
                                    found = true;
                                    break;
                                }
                            }
                            else
                            {
                                std::cerr << "unsupported token type \n";
                            }
                        }
                        if (found)
                            break;
                    }
                    if (!found)
                    {
                        response["error"] = buildError("No definition found for token at position", -32603);
                    }
                }
                else
                {
                    std::cerr << "unsupported method: " << method.value().str() << "\n";
                    logMessages.push_back("unsupported method: " + method.value().str());
                    response["error"] = buildError("Method not found", -32601);
                }

                if (hasId)
                {
                    llvm::json::Value resultValue(std::move(response));
                    sstream << resultValue;
                    std::cout << "Content-Length: " << sstream.str().length() << "\r\n\r\n";
                    std::cout << sstream.str();
                    std::cerr << "response: " << sstream.str() + "\n";
                }
            }
            catch (...)
            {
                std::cerr << " command: " << commandString << "\n";
                std::cerr << "unexpected error happend\n";
            }
        }
        else
        {
            logMessages.emplace_back("method not found");
        }

        sendLogMessage(logMessages);
    }
}
