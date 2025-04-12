#pragma once

#include <memory>
#include <string>
#include "Context.h"
#include "ast/FunctionDefinitionNode.h"
#include "ast/types/VariableType.h"


void createSystemCall(std::unique_ptr<Context> &context, const std::string &functionName,
                      const std::vector<FunctionArgument> &functionParams,
                      const std::shared_ptr<VariableType> &returnType = nullptr);

void createPrintfCall(const std::unique_ptr<Context> &context);
void createFPrintfCall(const std::unique_ptr<Context> &context);

void createAssignCall(std::unique_ptr<Context> &context);
void createResetCall(std::unique_ptr<Context> &context);
void createRewriteCall(std::unique_ptr<Context> &context);
void createReadLnCall(std::unique_ptr<Context> &context);
void createReadLnStdinCall(std::unique_ptr<Context> &context);
void createCloseFileCall(std::unique_ptr<Context> &context);
