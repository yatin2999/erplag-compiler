# Grammar Encoding and Parse Table

The LL(1) grammar of ERPLAG, how it is encoded in [`grammar.txt`](../grammar.txt) and in the compiler, and how the predictive parse table is built from it. The FIRST and FOLLOW sets are listed in [FIRST_FOLLOW.md](FIRST_FOLLOW.md).

## Grammar transformations

The rules for the constructs of the language specification were changed as follows to make the grammar LL(1). Null and unit productions are kept.

| Transformation | Productions | Example |
|---|---|---|
| Left recursion removed from each operator level: `E → E op T \| T` becomes `E → T Tail` and `Tail → op T Tail \| ε` | 70–81, 94–99 | `<arithmeticExpr> → <term> <addTail>`, `<addTail> → <addOp> <term> <addTail> \| ε` |
| Precedence by levels: `AND` and `OR` (one level), relational operators, `+` and `-`, `*` and `/`, then operands | 70–91 | `<expression>` → `<anyTerm>` → `<arithmeticExpr>` → `<term>` → `<factor>` |
| At most one relational operator in an operand of `AND` and `OR` | 74, 75 | `<relationalTail> → <relationalOp> <arithmeticExpr> \| ε` |
| Lists written with right recursion and an empty alternative | 2, 3, 5, 6, 11–16, 32, 33, 59–64, 123–125 | `<statements> → <statement> <statements> \| ε` |
| Left factoring of statements that begin with the same token | 51–55, 56–58 | `<assignmentStmt> → ID <whichStmt>` chooses between `:=` and `[` after the identifier |
| Left factoring of an identifier with an optional index | 41, 45, 46, 69, 86, 91–93 | `<factor> → ID <arrayIndex>`, `<arrayIndex> → SQBO <indexExpr> SQBC \| ε` |
| Separate nonterminals for the restricted forms of the language | 25–30, 45, 46, 100–106, 126–128, 133, 134 | an index cannot contain an array element (`<indexExpr>`); a `print` index is a signed number or identifier (`<indexArr>`); a `for` range has signed numbers only (`<indexForLoop>`); a case value is an unsigned number, `true` or `false` (`<value>`) |
| Arrays only where the language allows them | 17–24 | `<dataType>` for declarations and input parameters, `<type>` for array elements and output parameters |

The result has 68 nonterminals, 56 terminals and 134 productions, 20 of them empty.

## Encoding in `grammar.txt`

Each line of `grammar.txt` defines one nonterminal: the nonterminal, `->`, and its alternatives separated by `|`. A nonterminal is written in angle brackets, a terminal as its token name (such as `SEMICOL`), and `EPSILON` denotes an empty alternative. Symbols are separated by blanks. The first line defines the start symbol `<program>`.

| Item | Encoding in the compiler (`parserDef.h`) |
|---|---|
| Terminal | its token kind, 0 to 55, from the `TokenKind` enumeration of `lexerDef.h` |
| End of input `$` | `T_EOF`, 56 |
| Nonterminal | 57 onwards (`NT_BASE`), numbered in the order of first appearance in `grammar.txt`; `<program>` is 57 |
| Production | the left side, the array of right-side symbols and its length; productions are numbered from 1 in the order of the file, alternatives from left to right, and `EPSILON` has length 0 |
| Grammar | arrays of nonterminal names, productions, nullable flags, FIRST and FOLLOW sets (one 64-bit mask per nonterminal, in which bit t stands for terminal t; there are 57 terminals with `$`) and the parse table |

`load_grammar` in `src/grammar.c` reads the file with `fgets` and splits each line with `strtok`. A nonterminal is looked up by name and numbered when it first appears; a terminal name is matched with the token names of the lexer. A line that is not of the form `<A> -> …`, an unknown symbol, or a grammar larger than the arrays that hold it (100 nonterminals with names of up to 39 characters, 200 productions, 16 symbols on a right side) stops the compiler with a message naming the line.

## FIRST and FOLLOW sets

`compute_first_and_follow_sets` computes three properties by repeating passes over all productions until no set changes:

1. A nonterminal is nullable if one of its productions has only nullable nonterminals on its right side (or none).
2. For `A → X1 X2 … Xn`, FIRST(A) receives FIRST(X1), and FIRST(Xi+1) while X1 … Xi are nullable; the FIRST set of a terminal is the terminal itself. This grammar needs 7 passes.
3. FOLLOW(`<program>`) contains `$`. For `A → α B β`, FOLLOW(B) receives FIRST(β), and also FOLLOW(A) if β is nullable.

## Parse table

`create_parse_table` fills the table `table[A][t]` with production numbers. For each production `A → α`, every terminal t in FIRST(α) gets the entry `table[A][t] = production`; if α is nullable, so does every terminal t in FOLLOW(A), including `$`. An entry that would receive a second production is an LL(1) conflict: the compiler prints both productions and stops. An entry of 0 means a syntax error.

For this grammar the table has 270 filled entries and no conflict. The predictive parser in `src/parser.c` uses the table to expand the nonterminal on top of its stack, and the FIRST and FOLLOW sets to recover after a syntax error ([STAGE_1_CODING_DETAILS.md](STAGE_1_CODING_DETAILS.md)).
