cmake_minimum_required(VERSION 3.20)
project(llvm-c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Find Clang libraries for preprocessor
find_package(Clang REQUIRED CONFIG)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -fsanitize=address")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/syntax
    ${CMAKE_SOURCE_DIR}/src/lexer
    ${CMAKE_SOURCE_DIR}/src/parser
    ${CMAKE_SOURCE_DIR}/src/ast
    ${CMAKE_SOURCE_DIR}/src/codegen
    ${CMAKE_SOURCE_DIR}/src/preprocessor
)

# Source files
set(SOURCES
    src/main.c
    src/common/error.c
    src/common/memory.c
    src/common/debug.c
    src/syntax/c_syntax.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/parser/c_parser.c
    src/ast/ast.c
    src/codegen/codegen.c
    src/codegen/backend.c
    src/codegen/llvm_backend_impl.c
    src/preprocessor/preprocessor.c
)

# Executable
add_executable(llvm-c ${SOURCES})

# Link LLVM libraries
# Use llvm-config to get the correct library flags
execute_process(
    COMMAND llvm-config --libs
    OUTPUT_VARIABLE LLVM_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND llvm-config --ldflags
    OUTPUT_VARIABLE LLVM_LDFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Link libclang for preprocessor
target_link_libraries(llvm-c ${LLVM_LIBS} ${LLVM_LDFLAGS} -lclang)

# Install
install(TARGETS llvm-c DESTINATION bin)

# Tests
add_executable(test_lexer 
    tests/test_lexer.c
    src/common/error.c
    src/common/memory.c
    src/common/debug.c
    src/syntax/c_syntax.c
    src/lexer/lexer.c
)
target_link_libraries(test_lexer ${LLVM_LIBS} ${LLVM_LDFLAGS})

add_executable(test_parser
    tests/test_parser.c
    src/common/error.c
    src/common/memory.c
    src/common/debug.c
    src/syntax/c_syntax.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/parser/c_parser.c
    src/ast/ast.c
)
target_link_libraries(test_parser ${LLVM_LIBS} ${LLVM_LDFLAGS})

add_executable(test_preprocessor
    tests/test_preprocessor.c
    src/common/error.c
    src/common/memory.c
    src/common/debug.c
    src/syntax/c_syntax.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/parser/c_parser.c
    src/ast/ast.c
    src/preprocessor/preprocessor.c
)
target_link_libraries(test_preprocessor ${LLVM_LIBS} ${LLVM_LDFLAGS} -lclang)

add_executable(test_parser_stress
    tests/test_parser_stress.c
    src/common/error.c
    src/common/memory.c
    src/common/debug.c
    src/syntax/c_syntax.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/parser/c_parser.c
    src/ast/ast.c
)
target_link_libraries(test_parser_stress ${LLVM_LIBS} ${LLVM_LDFLAGS})

add_executable(test_lua
    tests/test_lua.c
    src/common/error.c
    src/common/memory.c
    src/common/debug.c
    src/syntax/c_syntax.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/parser/c_parser.c
    src/ast/ast.c
    src/preprocessor/preprocessor.c
)
target_link_libraries(test_lua ${LLVM_LIBS} ${LLVM_LDFLAGS} -lclang)

add_executable(test_codegen
    tests/test_codegen.c
    src/common/error.c
    src/common/memory.c
    src/common/debug.c
    src/syntax/c_syntax.c
    src/lexer/lexer.c
    src/parser/parser.c
    src/parser/c_parser.c
    src/ast/ast.c
    src/codegen/codegen.c
    src/codegen/backend.c
    src/codegen/llvm_backend_impl.c
)
target_link_libraries(test_codegen ${LLVM_LIBS} ${LLVM_LDFLAGS})
