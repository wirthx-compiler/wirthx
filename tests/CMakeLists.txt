set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GoogleTest)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)


mark_as_advanced(
        BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
        gtest_build_tests
        gtest_force_shared_crt gtest_disable_pthreads
)

set_target_properties(gtest PROPERTIES FOLDER extern)
set_target_properties(gtest_main PROPERTIES FOLDER extern)
set_target_properties(gmock PROPERTIES FOLDER extern)
set_target_properties(gmock_main PROPERTIES FOLDER extern)

set(TEST_SRC "")
file(GLOB_RECURSE TEST_SRC ${SRC})
message(${SRC})
macro(package_add_test TESTNAME)
    #INCLUDE_DIRECTORIES()

    include_directories(${PROJECT_SOURCE_DIR}/src/ ${LLVM_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})
    # create an exectuable in which the tests will be stored
    add_executable(${TESTNAME} ${TEST_SRC} ${ARGN})
    # link the Google test infrastructure, mocking library, and a default main fuction to
    # the test executable.  Remove g_test_main if writing your own main function.
    target_link_libraries(${TESTNAME} gtest gmock gtest_main)
    target_link_libraries(${TESTNAME} ${llvm_libs})
    set(TEST_WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/tests")

    # gtest_discover_tests replaces gtest_add_tests,
    # see https://cmake.org/cmake/help/v3.10/module/GoogleTest.html for more options to pass to it
    gtest_discover_tests(${TESTNAME}
            # set a working directory so your project root so that you can find test data via paths relative to the project root
            WORKING_DIRECTORY $<TARGET_FILE_DIR:wirthx_test>
            PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:wirthx_test>"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

macro(package_add_test_with_libraries TESTNAME FILES LIBRARIES TEST_WORKING_DIRECTORY)

    add_executable(} ${FILES})
    llvm_map_components_to_libnames(llvm_libs support core irreader)
    target_link_libraries(${TESTNAME} ${llvm_libs})

    target_link_libraries(${TESTNAME} gtest gmock gtest_main ${LIBRARIES})
    gtest_discover_tests(${TESTNAME}
            WORKING_DIRECTORY ${TEST_WORKING_DIRECTORY}
            PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()


package_add_test(wirthx_test lexer_test.cpp compiler_test.cpp rtl_tests.cpp)
add_custom_command(TARGET wirthx_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/testfiles/ $<TARGET_FILE_DIR:wirthx_test>/testfiles/)

add_custom_command(TARGET wirthx_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/errortests/ $<TARGET_FILE_DIR:wirthx_test>/errortests/)

add_custom_command(TARGET wirthx_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/projecteuler/ $<TARGET_FILE_DIR:wirthx_test>/projecteuler/)

add_custom_command(TARGET wirthx_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/rtl/ $<TARGET_FILE_DIR:wirthx_test>/rtl/)


add_custom_command(TARGET wirthx_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/rtl/ $<TARGET_FILE_DIR:wirthx_test>/rtl/)