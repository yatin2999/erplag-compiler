# ERPLAG Compiler

A compiler, written in C, for ERPLAG: a strongly typed language with `integer`, `real` and `boolean` types, static and dynamic arrays, and modules that can return several values. The compiler translates an ERPLAG program into 64-bit NASM assembly for Linux.

The pipeline:

1. **Lexer**: a hand-coded DFA with a twin buffer.
2. **Parser**: a table-driven LL(1) parser with error recovery. The grammar is read from `grammar.txt` at run time, and the FIRST and FOLLOW sets are computed from it.
3. **AST**: built from the parse tree.
4. **Semantic analysis**: symbol tables, type checking and the compile-time error checks.
5. **Code generation**: NASM assembly generated directly from the AST. It includes run-time checks for array bounds, array sizes, dynamic array ranges and division by zero.

[Watch the demo](https://github.com/user-attachments/assets/3f9902c5-3e17-46a5-ab21-eef69a9e027e)

## Requirements

- GCC and `make`, to build the compiler
- NASM and GCC, to assemble and link the generated code (x86-64 Linux)

## Building

```sh
make          # builds ./compiler and ./stage1exe
make clean    # removes them
```

| Target | Executable | Contents |
|---|---|---|
| `compiler` | `./compiler` | The full compiler (`src/driver.c`) |
| `stage1exe` | `./stage1exe` | Only the lexer and parser (`src/stage1driver.c`) |

Both executables open `grammar.txt` by a relative path, so run them from the repository root.

## Usage

### Full compiler

```sh
./compiler <testcase file> <assembly file>
```

This opens an interactive menu. Enter an option number; `0` exits.

| Option | Action |
|---|---|
| 1 | Print the token list |
| 2 | Check the syntax and print the parse tree |
| 3 | Print the abstract syntax tree |
| 4 | Show parse tree and AST node counts, memory sizes and the compression percentage |
| 5 | Print the symbol table |
| 6 | Print activation record sizes |
| 7 | Print static and dynamic arrays |
| 8 | Report errors and the total compile time |
| 9 | Generate assembly into `<assembly file>` (only if the program has no errors) |

Errors are printed as `Line <n>: <lexical|syntax|type|semantic> error: <message>`.

### Compiling and running a program

```sh
make
printf '9\n0\n' | ./compiler tests/codegen/c01.txt code.asm
nasm -f elf64 code.asm -o code.o
gcc -no-pie code.o -o code
./code
```

The generated assembly uses `printf`, `scanf`, `calloc` and `exit` from the C library, so link it with `gcc`. If a run-time check fails, the program prints `RUN TIME ERROR: …` and exits with status 1.

### Lexer and parser only

```sh
./stage1exe <testcase file> <parse tree output file> <size of buffer>
```

`<size of buffer>` is the size of each half of the lexer's twin buffer and must be at least 2. The menu options are:

| Option | Action |
|---|---|
| 1 | Print the source with comments removed |
| 2 | Print the token list |
| 3 | Parse the source and write the parse tree to `<parse tree output file>` |
| 4 | Print the total CPU time of the lexer and parser |

## Example ERPLAG program

From `tests/codegen/c01.txt`:

```
<<<driver program>>>
start
	declare x, y, z:integer;
	declare a, b, c:integer;
	a:= 5;
	b:= 9;
	get_value(x);
	get_value(y);
	z:= x + y*b +(a-b)*y+ a*2 - b*x;
	print(z);
end
```

`LANGUAGE_SPECIFICATION.md` has the full language definition and more sample programs.

## Project structure

```
.
├── grammar.txt                 LL(1) grammar of ERPLAG, read by the compiler at run time
├── makefile
├── LANGUAGE_SPECIFICATION.md   The ERPLAG language definition
├── docs/                       Design documents
├── src/
│   ├── lexer.c, lexerDef.h, lexer.h                     Lexer
│   ├── grammar.c, grammar.h                             Grammar loading, FIRST/FOLLOW sets, parse table
│   ├── parser.c, parserDef.h, parser.h                  LL(1) parser and parse tree
│   ├── ast.c, astDef.h, ast.h                           AST construction
│   ├── symbolTable.c, symbolTableDef.h, symbolTable.h   Symbol tables
│   ├── typechecker.c, typechecker.h                     Type checking
│   ├── semanticAnalyzer.c, semanticAnalyzer.h           Semantic checks
│   ├── codegen.c, codegen.h                             NASM code generation
│   ├── driver.c                                         Driver for ./compiler
│   └── stage1driver.c                                   Driver for ./stage1exe
└── tests/
    ├── stage1/       t01–t07: lexer and parser tests
    ├── semantic/     t01–t10: semantic and type error tests
    ├── codegen/      c01–c11: code generation tests
    └── evaluation/   v01–v09: evaluation programs
```

## Documentation

| Document | Contents |
|---|---|
| [`LANGUAGE_SPECIFICATION.md`](LANGUAGE_SPECIFICATION.md) | Lexical structure, types, statements, scope, run-time behaviour, compile-time errors, token list and grammar |
| [`docs/DFA_DESIGN.md`](docs/DFA_DESIGN.md) | DFA of the lexer |
| [`docs/GRAMMAR_ENCODING.md`](docs/GRAMMAR_ENCODING.md) | Grammar transformations, the `grammar.txt` format and parse table construction |
| [`docs/FIRST_FOLLOW.md`](docs/FIRST_FOLLOW.md) | FIRST and FOLLOW sets |
| [`docs/AST_SEMANTIC_RULES.md`](docs/AST_SEMANTIC_RULES.md) | Rules for building the AST |
| [`docs/STAGE_1_CODING_DETAILS.md`](docs/STAGE_1_CODING_DETAILS.md) | Implementation details of the lexer and parser |
| [`docs/STAGE_2_CODING_DETAILS.md`](docs/STAGE_2_CODING_DETAILS.md) | Implementation details of the AST, semantic analysis and code generation |

## License

MIT. See [`LICENSE`](LICENSE).
