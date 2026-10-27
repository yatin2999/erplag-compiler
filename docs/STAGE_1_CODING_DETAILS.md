# Stage 1: Coding Details

The coding details of the lexer and parser, answered in the order of the Stage 1 proforma of the course. Team names, IDs, the group number, the date and the declaration of the original proforma are not filled in, because this repository is not a 2023 group submission.

## Submitted files

| Stage 1 files | |
|---|---|
| Grammar | `grammar.txt` |
| Lexer | `src/lexerDef.h`, `src/lexer.h`, `src/lexer.c` |
| Parser | `src/parserDef.h`, `src/parser.h`, `src/parser.c`, `src/grammar.h`, `src/grammar.c` |
| Driver | `src/stage1driver.c`, built as `stage1exe` |
| Build | `makefile` |
| Test cases | `tests/stage1/t01.txt` to `t07.txt` |

## Lexer details

| Question | Answer |
|---|---|
| Technique used for pattern matching | A hand-coded DFA. The first character chooses the recognizer (identifier or keyword, number, operator, comment), which reads characters one at a time and retracts the characters that are not part of the token. Diagrams: [DFA_DESIGN.md](DFA_DESIGN.md). |
| DFA implementation | A `switch` on the current character for operators, and loops over character classes for identifiers, numbers and comments. |
| Keyword handling technique | Each word of identifier form is looked up in a hash table of the 30 keywords: a keyword gets its own token, any other word is ID. |
| Hash function | `h = 31 × h + c` over the characters of the word, modulo the 61 slots of the table; collisions are resolved by linear probing. |
| Twin buffer used | Yes. Two halves of `size_of_buffer` characters (a command line argument); `get_stream` fills a half with `fread` when the forward pointer enters it. The characters of the current lexeme are copied into a growing scan buffer as they are read (the text of a comment is skipped, not copied), so lexemes and comments can cross the boundary between the halves, and a retraction of up to two characters never needs a reloaded half. |
| Lexical errors handled | A malformed number (`26.`, `12.5E`), an integer number above 9223372036854775807 or a real number above the largest double, an identifier longer than 20 characters, `=`, `!` or `.` alone, a character that cannot begin a token, and a comment that is never closed. Each is reported with its line number and text, and scanning continues with the next character. |
| tokenInfo | Token kind, line number, the complete lexeme, an error message for an error token, and the value of a NUM (`long long`, checked with `strtoll` and `errno`) or RNUM (`double`, from `strtod`). The lexeme of the current token points into the scan buffer; the parse tree keeps its own copy of the lexeme of each matched token, and the AST takes that copy over. |
| Interface with parser | The parser calls `get_next_token` whenever it needs the next token, and looks no further ahead than that token. It prints a lexical error token with its line number and reads the next token. |

## Parser details

| Structure | Description |
|---|---|
| Grammar | An array of productions, each with its left side and the array of its right-side symbols, numbered from 1 in the order of `grammar.txt`. Terminals are token kinds, nonterminals are numbered from 57 ([GRAMMAR_ENCODING.md](GRAMMAR_ENCODING.md)). |
| Parse table | A two-dimensional array of production numbers indexed by nonterminal and terminal; 0 means a syntax error. |
| Parse tree | Each node holds its grammar symbol, the number of the production applied to it, the array of its children and, for a leaf, its token. The printer passes each node's parent symbol down as it walks the tree. |
| Parsing stack node | A grammar symbol, the parse-tree node to fill in, and the node below it: the stack is a linked list. |

## Parse tree

| Question | Answer |
|---|---|
| Constructed | Yes, while parsing. |
| Printing as per the given format | Yes: seven columns of fixed minimum width, each followed by a blank (lexeme, line number, token name, value of a NUM or RNUM, parent symbol or `ROOT`, `yes`/`no` for a leaf, node symbol), with `----` where a field does not apply. A longer value moves the rest of its line. |
| Printing order | Inorder: the subtree of the leftmost child, the node, then the subtrees of the other children. |

## Grammar and FIRST and FOLLOW sets

| Question | Answer |
|---|---|
| Data structure for the original grammar rules | The production array above, read from `grammar.txt` at run time; no rule is written in the C source. |
| FIRST and FOLLOW computation automated | Yes. The sets are listed in [FIRST_FOLLOW.md](FIRST_FOLLOW.md). |
| Data structure for the sets | One 64-bit mask per nonterminal, in which bit t stands for terminal t (57 terminals with `$`); a union is a bitwise OR. |
| Time complexity of computing FIRST sets | Passes over all productions are repeated until no set changes. Each pass is linear in the total length of the right sides; this grammar needs 7 passes. |
| Functions | `compute_first_and_follow_sets` (with `compute_nullable`, `compute_first`, `compute_follow`) and `create_parse_table`, called by `load_grammar`. |
| Manual computation | No. |

## Error handling

| Question | Answer |
|---|---|
| Attempted | Yes. |
| Printing errors | All lexical and syntax errors. The parser prints each one when it detects it, so they appear in line order. |
| Types of errors handled | A missing or wrong terminal (`unexpected 'integer', expected ':'`), a token that cannot start the nonterminal on top of the stack (`unexpected '+' while parsing <varPrint>`), and tokens after the end of the program. |
| Synchronizing tokens | Panic mode. A terminal on top of the stack that does not match is popped, as if it had been present. For a nonterminal, tokens are skipped until one is in its FIRST or FOLLOW set, or, inside a statement list, until SEMICOL or END; the nonterminal is then expanded, popped, or popped with the stack down to the nearest `<statements>`. After an error, no other error is reported until a terminal has been matched. |
| Errors detected in `t06` | All 12 annotated error lines. |

## Compilation and execution

| Question | Answer |
|---|---|
| Makefile works | Yes: `make` builds `stage1exe` and `compiler`. |
| Code compiles | Yes, without warnings, with GCC 15.2.0 on Ubuntu 26.04. GCC 9.4.0 on Ubuntu 20.04.4 was not available for testing; the code is standard C11, the default dialect of GCC 9, and uses only the C standard library. |
| Driver | `./stage1exe testcase.txt parsetreeOutFile.txt size_of_buffer`, with options 0 exit, 1 comment removal, 2 token list, 3 parsing (errors on the console, parse tree in the file), 4 CPU time of the lexer and parser. The status of the implementation is printed first. |
| Execution status | All Stage 1 test files give the expected output. |
| Segmentation fault with any test case | No. |
| Language features not handled | None. |

CPU time of option 4 (lexing and parsing, with the printing of errors), measured with `clock()` (1 tick = 1 µs; best of 20 runs):

| Test | `t01` | `t02` | `t03` | `t04` | `t05` | `t06` | `t07` |
|---|---:|---:|---:|---:|---:|---:|---:|
| Ticks | 187 | 148 | 195 | 222 | 237 | 241 | 245 |
| Seconds | 0.000187 | 0.000148 | 0.000195 | 0.000222 | 0.000237 | 0.000241 | 0.000245 |
