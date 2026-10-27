# Stage 2: Coding Details

The coding details of the integrated compiler, answered in the order of the Stage 2 proforma of the course. Team names, IDs, the group number, the date, the declaration and the strengths self-assessment of the original proforma are not filled in, because this repository is not a 2023 group submission.

## Submitted files

| Files | |
|---|---|
| Definitions (4) | `src/lexerDef.h`, `src/parserDef.h`, `src/astDef.h`, `src/symbolTableDef.h` |
| Interfaces (8) | `src/lexer.h`, `src/grammar.h`, `src/parser.h`, `src/ast.h`, `src/symbolTable.h`, `src/typechecker.h`, `src/semanticAnalyzer.h`, `src/codegen.h` |
| Implementation (10) | `src/lexer.c`, `src/grammar.c`, `src/parser.c`, `src/ast.c`, `src/symbolTable.c`, `src/typechecker.c`, `src/semanticAnalyzer.c`, `src/codegen.c`, `src/driver.c`, `src/stage1driver.c` |
| Other | `grammar.txt`, `makefile`, the 37 test cases in `tests/`, and the documents in `docs/` |

## Status of code development

| Module | Developed |
|---|---|
| Lexer, parser | Yes |
| Abstract syntax tree | Yes |
| Symbol table | Yes |
| Type checking module | Yes |
| Semantic analysis module | Yes: all 37 compile-time errors of the language specification (LEVEL 4) |
| Code generator | Yes |

## Execution status

| Question | Answer |
|---|---|
| Code generator produces `code.asm` | Yes, with option 9, for a program without errors. |
| `code.asm` produces correct output using NASM for `c01`–`c11` | Yes, for all eleven and for the evaluation programs `v03`–`v09`. |
| Semantic analyzer produces semantic errors appropriately | Yes: for `t01`–`t10`, `v01` and `v02` the reported lines are the annotated lines. |
| Static type checker reports type mismatch errors appropriately | Yes. |
| Dynamic type checking works for arrays and reports errors when `code.asm` runs | Yes: index out of bound, array size mismatch and invalid array range, and also division by zero. |
| Symbol table constructed and printed; AST constructed and printed | Yes; yes. |
| Test cases with a segmentation fault | None. |

## Data structures

| Structure | Description |
|---|---|
| AST node | Construct kind, line number, `next` link for lists, and the fields of the kind: child nodes, the operator, the token of an identifier or literal (moved from the parse tree), the link to the symbol-table entry, the computed type and, for a call, the called module. |
| Symbol table | One chained hash table per scope (module parameters, module body, loop body, `case`, `default`) with its parent scope, nesting level and line range. Module names are kept in a separate hash table, each with its first declaration and first definition. |
| Array type expression | Element type, static or dynamic, and two bounds, each a signed number or the entry of the bound variable. |
| Input and output parameters | Arrays of the module's symbol-table entries, in the order of the parameters. |
| Three-address code | Not created: NASM code is generated directly from the AST. |

## AST and symbol table

| Item | Answer |
|---|---|
| AST construction | One pass over the parse tree with a `switch` on the production number of each node, following the rules of [AST_SEMANTIC_RULES.md](AST_SEMANTIC_RULES.md). Each parse-tree node is freed as soon as its part of the AST has been built. Option 3 prints the AST in preorder. |
| Parse tree and AST sizes (option 4) | Node counts and `sizeof` of the nodes (a parse-tree node is 56 bytes, plus 8 bytes for each child pointer; an AST node is 80 bytes; both hold tokens of 32 bytes, whose lexemes are allocated separately). For `c11`: parse tree 658 nodes, 42104 bytes; AST 153 nodes, 12240 bytes; compression 70.93 %. |
| Widths | integer 2, real 4, boolean 1; static array 1 + n·w; dynamic array 1; array parameter 1 + 2·2 = 5. |
| Offsets and activation record size | Offsets count from 0 in each module: input parameters, output parameters, then local and loop variables in textual order. The activation record size (option 6) is the sum of the widths. |

## Semantic checks

`semanticAnalyzer.c` walks the modules in the order of the source, and each module body statement by statement. It opens the scopes and enters the declarations through `symbolTable.c`, which checks declarations and parameters, calls the type checker `typechecker.c`, which computes the type of every expression and checks the type rules, and checks the other rules itself. The parameters of all modules are entered before the walk, so that a call can be checked against a module defined later in the source.

| Check | Scheme |
|---|---|
| Variable not declared | No entry in the current or an enclosing scope. |
| Multiple declarations | Name already in the current scope. |
| Number and type of input and output parameters | Counts compared with the parameter arrays of the called module; types compared entry by entry. |
| Assignment of a value to an output parameter | No assignment recorded for the parameter when the module ends. |
| Function call semantics | The definition or a declaration must precede the call. |
| Static type checking | Expression types computed bottom-up over the AST. |
| Return semantics | A result list exactly when the module has output parameters. |
| Recursion | At each call from A to B, a depth-first search over the calls of the modules (with a visited mark per module) checks whether B leads back to A; if so, the call is reported. |
| Module overloading | A second definition of a module name. |
| `switch` semantics | Selector and case types, duplicate case values, `default` rules. After a real, array or undeclared selector, the cases are not analysed, so their declarations get no entries. |
| `for` and `while` semantics | The loop variable is not assigned; some variable of the `while` condition is assigned in the body. |
| Offsets for nested scopes | One running offset per module. |
| Offsets for formal parameters | Inputs, then outputs, from offset 0. |
| Local variable over an input parameter | Allowed: the body is a nested scope. |
| Array semantics and type checking of arrays | Structural equivalence; constant indices of static arrays checked at compile time. |
| Scope and visibility | Lookup from the innermost scope outwards. |
| Nesting depth | Parameters 0, module body 1, then one more per scope. |

Each error is printed when the walk finds it, as `Line <n>: <lexical|syntax|type|semantic> error: <message>`; as the walk follows the source, the errors appear in the order of the source. Options 5 to 7 print the errors and then their tables. An error caused by a missing construct (a `default` or `case`, an assignment to an output parameter, a change of a `while` condition) is reported at the closing line with the line range of its `switch`, module or loop.

## Code generation

| Question | Answer |
|---|---|
| NASM version; 32 or 64 bit | 64-bit (`nasm -f elf64`). Tested with NASM 3.01; NASM 2.14.02 was not available for testing. |
| One memory word | 8 bytes. |
| Registers | Base of the activation record: `rbp`; stack pointer: `rsp`; others: `rax`, `xmm0` and `xmm1` for values, `rcx`, `rdx`, `rsi`, `rdi` and `r8` for addresses and routine arguments, and `rbx`, `r12`–`r14` inside the array routines. |
| Physical sizes | integer, real and boolean: 1 word (8 bytes) each. |
| Function call model | The caller reserves a parameter block on the stack, stores the input parameters in it from left to right and calls `module_<name>`. The callee saves `rbp`, reserves its frame for the local variables and runs its body. After the return, the caller copies the output parameters from the block to the result variables and frees the block. |
| Caller's responsibilities | Fill the parameter block, check the size of a dynamic array argument, copy the outputs back and free the block. |
| Callee's responsibilities | Save `rbp`, reserve its frame, run the body, restore `rbp` and return. |
| Return addresses | Pushed by `call` and popped by `ret`; the saved `rbp` links the frames. |
| Parameter passing; offsets used by the callee | Scalars by value; an array as the address of its first element with the low and high bounds (3 words). Parameter i is at `rbp + 16 + offset_i`, fixed at compile time from the parameter list. |
| Dynamic array parameter ranges | The caller writes the formal parameter's bounds into the parameter block and checks that a dynamic actual array has the same size. |
| Activation record size computation | Local variables only, laid out by the symbol table as they are declared: 8 bytes per scalar, and 24 per array (address of the first element and bounds) followed by 8 per element of a static array; the parameters are in the caller's parameter block. A declaration that would make the frame larger than 2147483632 bytes, the largest that 32-bit displacements address, is a semantic error. |
| Static and dynamic arrays | Each time a declaration runs, the array's address and bounds are set. A static array's elements are in the activation record. For a dynamic array, the bounds are computed and checked, the number of elements high - low + 1 is computed without wrapping, and `calloc` allocates the elements; they are never freed, so that an array sharing them through `A := B` keeps them after the declaring scope or module has ended (the language specification accepts that unreachable elements leak). `A := B` copies B's first-element address into A, so the arrays share their elements. |
| Register allocation | Fixed templates: values are computed in `rax` or `xmm0`, and the left operand is pushed while the right one is evaluated. |
| Primitive data types handled | integer, real and boolean. |
| Temporaries | Pushed on the stack, below the local variables of the frame. |
| Run-time checks | Array index bounds, array sizes in assignments and calls, dynamic array ranges and division by zero; each prints `RUN TIME ERROR: …` and ends the program with exit status 1. If the number of elements of a dynamic array cannot be represented or `calloc` cannot allocate them, the program prints `RUN TIME ERROR: Out of memory` in the same way. |
| Run-time routines | Input and output of each type, array input and output, and the error exits are written by `codegen.c` once, at the end of `code.asm`, which needs no other file. |
| Command for running `code.asm` | `nasm -f elf64 code.asm -o code.o`, then `gcc -no-pie code.o -o code`, then `./code`. |

## Compilation and execution time

| Question | Answer |
|---|---|
| Makefile works / code compiles | Yes, without warnings (GCC 15.2.0). GCC 9.4.0 was not available for testing; the code is standard C11, the default dialect of GCC 9. |
| Driver | `./compiler testcase.txt code.asm`, then a menu of the ten options 0–9, after the line `LEVEL 4: …`. |
| Language features not handled | None. |

CPU time of option 8 (lexical, syntax and semantic analysis, including the printing of errors; 1 tick = 1 µs; best of 20 runs):

| Test | `t01` | `t02` | `t03` | `t04` | `t05` | `t06` | `t07` | `t08` | `t09` | `t10` |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Ticks | 329 | 306 | 543 | 500 | 585 | 893 | 856 | 954 | 1337 | 334 |
| Seconds | 0.000329 | 0.000306 | 0.000543 | 0.000500 | 0.000585 | 0.000893 | 0.000856 | 0.000954 | 0.001337 | 0.000334 |
