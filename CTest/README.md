# C99 Test Suite for LLVM-C Compiler

This directory contains test programs to verify the LLVM-C compiler functionality.

## Test Files

### 1. `simple.c`
- **Purpose**: Simplest possible C program
- **Features**: Basic main function with return value
- **Expected**: Returns exit code 42

### 2. `no_stdlib.c`
- **Purpose**: Pure computation without standard library
- **Features**: Multiple functions, recursion, arithmetic
- **Expected**: Returns 1084 (30 + 30 + 1024)

### 3. `hello.c`
- **Purpose**: Classic hello world
- **Features**: stdio.h include, printf function call
- **Expected**: Prints "Hello from LLVM-C compiler!"

### 4. `math.c`
- **Purpose**: Mathematical operations
- **Features**: Multiple functions, recursion (factorial, fibonacci)
- **Expected**: Prints various math results

### 5. `control_flow.c`
- **Purpose**: Control flow statements
- **Features**: if/else statements, comparisons
- **Expected**: Prints max, min, abs, sign results

### 6. `operators.c`
- **Purpose**: All operator types
- **Features**: Arithmetic, comparison, bitwise operators
- **Expected**: Prints operator test results

## How to Test

### Compile Only (No Linking)
```bash
# Generate LLVM IR
./llvm-c --emit-llvm simple.c -o simple.ll

# Generate assembly
./llvm-c -S simple.c -o simple.s

# Generate object file
./llvm-c -c simple.c -o simple.o
```

### Compile and Link
```bash
# No optimization
./llvm-c simple.c -o simple

# With optimization
./llvm-c -O2 simple.c -o simple_opt

# Maximum optimization
./llvm-c -O3 simple.c -o simple_fast
```

### Run Tests
```bash
# Test simple program
./llvm-c simple.c -o simple && ./simple
echo $?  # Should print 42

# Test no_stdlib
./llvm-c no_stdlib.c -o no_stdlib && ./no_stdlib
echo $?  # Should print 1084 (mod 256 = 60)

# Test with printf (requires libc)
./llvm-c hello.c -o hello && ./hello
```

## Current Compiler Status

### ✅ Working
- Function declarations
- Function calls (including recursion)
- If/else statements
- Arithmetic operators (+, -, *, /, %)
- Comparison operators (==, !=, <, <=, >, >=)
- Bitwise operators (&, |, ^, <<, >>)
- Integer literals
- Return statements
- Parameter passing
- LLVM optimization passes (O0-O3)

### 🚧 Not Yet Implemented
- While/for/do-while loops
- Switch statements
- Variable declarations (local/global)
- Arrays
- Pointers and dereferencing
- Structs and unions
- Unary operators (++, --, !, ~, &, *)
- Assignment operators (=, +=, -=, etc.)
- Standard library integration
- Preprocessor directives

## Expected Results

Programs that should compile successfully:
- ✅ `simple.c` - Basic program structure
- ✅ `no_stdlib.c` - Pure computation
- ⚠️ `hello.c` - Needs external printf (linking may fail)
- ⚠️ `math.c` - Needs external printf (linking may fail)
- ⚠️ `control_flow.c` - Needs external printf (linking may fail)
- ⚠️ `operators.c` - Needs external printf (linking may fail)

Note: Programs using `printf` will generate correct LLVM IR but may fail at link time if libc is not properly linked.
