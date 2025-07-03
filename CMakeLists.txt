cmake_minimum_required(VERSION 3.10)

project(wirthx VERSION 0.1 DESCRIPTION "wirthx is a pascal interpreter and in future compiler")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

SET(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
SET(CMAKE_FIND_PACKAGE_SORT_DIRECTION ASC)


find_package(LLVM 18.1.0 REQUIRED CONFIG)

message(STATUS " Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")

message(STATUS "Clang VERSION: ${CLANG_VERSION_STRING}")
if (UNIX)
    # G++
    file(GLOB LINKER_SRC src/linker/unix/pascal_linker.cpp src/os/unix/command.cpp)
elseif (WIN32)
    file(GLOB LINKER_SRC src/linker/windows/pascal_linker.cpp src/os/windows/command.cpp)
endif ()

set(WIRTHX_VERSION_MAJOR 0)
set(WIRTHX_VERSION_MINOR 1)
set(WIRTHX_VERSION_PATCH 0)
add_compile_definitions(WIRTHX_PATH="${WIRTHX_PATH}")

file(WRITE "${PROJECT_BINARY_DIR}/VERSION.TXT" "${WIRTHX_VERSION_MAJOR}.${WIRTHX_VERSION_MINOR}.${WIRTHX_VERSION_PATCH}")

# configure a header file to pass some of the CMake settings
# to the source code
configure_file(
        "${PROJECT_SOURCE_DIR}/config.h.in"
        "${PROJECT_BINARY_DIR}/config.h"
)


file(GLOB SRC
        ${LINKER_SRC}
        ${PROJECT_BINARY_DIR}/config.h
        src/compare.cpp
        src/ast/BooleanNode.cpp
        src/ast/VariableDefinition.cpp
        src/ast/ASTNode.cpp
        src/ast/PrintNode.cpp
        src/ast/DoubleNode.cpp
        src/ast/NumberNode.cpp
        src/ast/CharConstantNode.cpp
        src/ast/StringConstantNode.cpp
        src/ast/BinaryOperationNode.cpp
        src/ast/MinusNode.cpp
        src/ast/InputNode.cpp
        src/ast/TypeNode.cpp
        src/ast/ComparrisionNode.cpp
        src/ast/VariableAccessNode.cpp
        src/ast/ArrayAccessNode.cpp
        src/ast/FieldAccessNode.cpp
        src/ast/EnumAccessNode.cpp
        src/ast/LogicalExpressionNode.cpp
        src/ast/IfConditionNode.cpp
        src/ast/WhileNode.cpp
        src/ast/VariableAssignmentNode.cpp
        src/ast/ArrayAssignmentNode.cpp
        src/ast/ArrayInitialisationNode.cpp
        src/ast/FieldAssignmentNode.cpp
        src/ast/FunctionDefinitionNode.cpp
        src/ast/ReturnNode.cpp
        src/ast/FunctionCallNode.cpp
        src/ast/BlockNode.cpp
        src/ast/UnitNode.cpp
        src/ast/SystemFunctionCallNode.cpp
        src/ast/AddressNode.cpp
        src/ast/types/VariableType.cpp
        src/ast/types/RecordType.cpp
        src/ast/types/StringType.cpp
        src/ast/types/FileType.cpp
        src/ast/types/RangeType.cpp
        src/ast/types/EnumType.cpp
        src/ast/types/ValueRangeType.cpp
        src/ast/types/ArrayType.cpp
        src/ast/ForNode.cpp
        src/ast/ForEachNode.cpp
        src/ast/RepeatUntilNode.cpp
        src/ast/BreakNode.cpp
        src/ast/CaseNode.cpp
        src/compiler/intrinsics.cpp
        src/compiler/CompilerOptions.cpp
        src/compiler/Context.cpp
        src/compiler/Compiler.cpp
        src/compiler/codegen.cpp
        src/exceptions/CompilerException.cpp
        src/lsp/LanguageServer.cpp
        src/Lexer.cpp
        src/MacroParser.cpp
        src/Parser.cpp)
INCLUDE_DIRECTORIES("src")

# include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

#separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

#add_definitions(${LLVM_DEFINITIONS_LIST})

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
# Find and link LLD libraries

llvm_map_components_to_libnames(llvm_libs support core irreader native nativecodegen passes)


include(FetchContent)

option(BUILD_TESTS "Build test programs" ON)
if (BUILD_TESTS)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.0.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests)

endif ()

# Let's set compiler-specific flags
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    # Clang
    add_compile_options(-Wall -Werror)
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    # G++
    add_compile_options(-Wall -Werror)
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    # MSVC
    target_compile_options(${PROJECT_NAME} PRIVATE /MT /EHsc /W2 /c)

    # Set the DLLEXPORT variable to export symbols
    #target_compile_definitions(${PROJECT_NAME} PRIVATE WIN_EXPORT)
endif ()


add_executable(${PROJECT_NAME} "main.cpp" ${SRC})
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
    set_property(TARGET ${PROJECT_NAME} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()
target_link_libraries(${PROJECT_NAME} ${llvm_libs})

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "wirthx")
set(CPACK_PACKAGE_VENDOR "hisoka999")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "wirthx is a pascal compiler and language server")
set(CPACK_PACKAGE_VERSION_MAJOR ${WIRTHX_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${WIRTHX_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${WIRTHX_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "hisoka999@gmail.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")


# Windows ZIP-Paket
set(CPACK_GENERATOR "ZIP")
if (WIN32)
    set(CPACK_GENERATOR "ZIP")
endif ()

# Linux DEB-Paket
if (UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Stefan Lüdtke <hisoka999@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.27) , binutils, gcc")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://wirthx-compiler.github.io/")

endif ()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY rtl/ DESTINATION include/wirthx/rtl/)


include(CPack)