# DFA of the ERPLAG Lexer

One DFA recognizes every token. Its four parts below share the start state 0.

- `((n))` is state *n*; `(((T)))` is an accepting state that returns the token *T*.
- `*` on an accepting or error state: the last character read is not part of the token and is read again; `**`: the last two characters.
- *other*: any character without an arrow of its own, including the end of the input.
- *error*: the text read is reported as a lexical error with its line number, and scanning continues in state 0.

## Identifiers, keywords and numbers

```mermaid
flowchart LR
    S0((0)) -->|"letter or _"| S1((1))
    S1 -->|"letter, digit or _"| S1
    S1 -->|other| ID((("ID or keyword *")))
    S0 -->|digit| S2((2))
    S2 -->|digit| S2
    S2 -->|other| NUM1((("NUM *")))
    S2 -->|"."| S3((3))
    S3 -->|"."| NUM2((("NUM **")))
    S3 -->|digit| S4((4))
    S3 -->|other| ERR1["error *: no digit after the point"]
    S4 -->|digit| S4
    S4 -->|other| RNUM1((("RNUM *")))
    S4 -->|"e or E"| S5((5))
    S5 -->|"+ or -"| S6((6))
    S5 -->|digit| S7((7))
    S5 -->|other| ERR2["error *: no digit in the exponent"]
    S6 -->|digit| S7
    S6 -->|other| ERR2
    S7 -->|digit| S7
    S7 -->|other| RNUM2((("RNUM *")))
```

Action at *ID or keyword*: a word longer than 20 characters is a lexical error; any other word is looked up in the keyword table below. Action at *NUM* and *RNUM*: an integer above 9223372036854775807 or a real number above the largest double is a lexical error. `NUM **` is an integer followed by the range operator, as in `1..10`.

## Operators and punctuation

```mermaid
flowchart LR
    S0((0)) -->|"+"| PLUS((("PLUS")))
    S0 -->|"-"| MINUS((("MINUS")))
    S0 -->|"/"| DIV((("DIV")))
    S0 -->|";"| SEMICOL((("SEMICOL")))
    S0 -->|","| COMMA((("COMMA")))
    S0 -->|"["| SQBO((("SQBO")))
    S0 -->|"]"| SQBC((("SQBC")))
    S0 -->|"("| BO((("BO")))
    S0 -->|")"| BC((("BC")))
    S0 -->|"*"| S8((8))
    S8 -->|other| MUL((("MUL *")))
    S8 -->|"*"| S9((9))
    S0 -->|"character that cannot begin a token"| ERR["error"]
```

State 9, the inside of a comment, is in the last diagram.

## Relational, assignment, range and delimiter operators

```mermaid
flowchart LR
    S0((0)) -->|"#60;"| S11((11))
    S11 -->|"="| LE((("LE")))
    S11 -->|other| LT((("LT *")))
    S11 -->|"#60;"| S12((12))
    S12 -->|"#60;"| DRIVERDEF((("DRIVERDEF")))
    S12 -->|other| DEF((("DEF *")))
    S0 -->|"#62;"| S13((13))
    S13 -->|"="| GE((("GE")))
    S13 -->|other| GT((("GT *")))
    S13 -->|"#62;"| S14((14))
    S14 -->|"#62;"| DRIVERENDDEF((("DRIVERENDDEF")))
    S14 -->|other| ENDDEF((("ENDDEF *")))
    S0 -->|":"| S15((15))
    S15 -->|"="| ASSIGNOP((("ASSIGNOP")))
    S15 -->|other| COLON((("COLON *")))
    S0 -->|"="| S16((16))
    S16 -->|"="| EQ((("EQ")))
    S16 -->|other| ERR3["error *"]
    S0 -->|"!"| S17((17))
    S17 -->|"="| NE((("NE")))
    S17 -->|other| ERR3
    S0 -->|"."| S18((18))
    S18 -->|"."| RANGEOP((("RANGEOP")))
    S18 -->|other| ERR3
```

## Comments and white space

```mermaid
flowchart LR
    S0((0)) -->|"blank, tab or CR"| S0
    S0 -->|"newline: line + 1"| S0
    S0 -->|"*"| S8((8))
    S8 -->|"*"| S9((9))
    S9 -->|"other; newline: line + 1"| S9
    S9 -->|"*"| S10((10))
    S10 -->|"other; newline: line + 1"| S9
    S10 -->|"*"| CMT((("comment ends, no token")))
    S9 -->|"end of input"| ERRC["error at the line of the opening **"]
    S10 -->|"end of input"| ERRC
    S0 -->|"end of input"| EOI((("end of the token list")))
```

## Keyword lookup table

The 30 keywords are kept in a hash table of 61 slots, filled when the lexer is opened. The hash of a word is `h = 31 × h + c` over its characters, modulo 61, and a collision moves on to the next free slot. The lookup compares whole words, so it is case sensitive: `Switch` is an identifier.

The DFA does not depend on the twin buffer through which the lexer reads its input.

| Keyword | Token | Keyword | Token | Keyword | Token |
|---|---|---|---|---|---|
| `integer` | INTEGER | `get_value` | GET_VALUE | `switch` | SWITCH |
| `real` | REAL | `print` | PRINT | `case` | CASE |
| `boolean` | BOOLEAN | `use` | USE | `break` | BREAK |
| `of` | OF | `with` | WITH | `default` | DEFAULT |
| `array` | ARRAY | `parameters` | PARAMETERS | `while` | WHILE |
| `start` | START | `takes` | TAKES | `true` | TRUE |
| `end` | END | `input` | INPUT | `false` | FALSE |
| `declare` | DECLARE | `returns` | RETURNS | `AND` | AND |
| `module` | MODULE | `for` | FOR | `OR` | OR |
| `driver` | DRIVER | `in` | IN | | |
| `program` | PROGRAM | | | | |
