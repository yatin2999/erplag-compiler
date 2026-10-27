# FIRST and FOLLOW Sets

FIRST and FOLLOW sets of the 68 nonterminals of [`grammar.txt`](../grammar.txt), in the order of that file. ε in a FIRST set means that the nonterminal can derive the empty string; `$` in a FOLLOW set is the end of the input.

| Nonterminal | FIRST | FOLLOW |
|---|---|---|
| `<program>` | DECLARE, DEF, DRIVERDEF | `$` |
| `<moduleDeclarations>` | DECLARE, ε | DEF, DRIVERDEF |
| `<moduleDeclaration>` | DECLARE | DECLARE, DEF, DRIVERDEF |
| `<otherModules>` | DEF, ε | DRIVERDEF, `$` |
| `<driverModule>` | DRIVERDEF | DEF, `$` |
| `<module>` | DEF | DEF, DRIVERDEF, `$` |
| `<ret>` | RETURNS, ε | START |
| `<inputPlist>` | ID | SQBC |
| `<inputPlistTail>` | COMMA, ε | SQBC |
| `<outputPlist>` | ID | SQBC |
| `<outputPlistTail>` | COMMA, ε | SQBC |
| `<dataType>` | INTEGER, REAL, BOOLEAN, ARRAY | SEMICOL, COMMA, SQBC |
| `<rangeArrays>` | ID, NUM, PLUS, MINUS | SQBC |
| `<type>` | INTEGER, REAL, BOOLEAN | SEMICOL, COMMA, SQBC |
| `<indexArr>` | ID, NUM, PLUS, MINUS | RANGEOP, SQBC |
| `<newIndex>` | ID, NUM | RANGEOP, SQBC |
| `<sign>` | PLUS, MINUS, ε | ID, NUM, RNUM |
| `<moduleDef>` | START | DEF, DRIVERDEF, `$` |
| `<statements>` | DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, WHILE, ID, SQBO, ε | END, BREAK |
| `<statement>` | DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, WHILE, ID, SQBO | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<ioStmt>` | GET_VALUE, PRINT | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<varPrint>` | TRUE, FALSE, ID, NUM, RNUM | BC |
| `<printIndex>` | SQBO, ε | BC |
| `<boolConst>` | TRUE, FALSE | AND, OR, PLUS, MINUS, MUL, DIV, LT, LE, GT, GE, EQ, NE, SEMICOL, COMMA, BC |
| `<simpleStmt>` | USE, ID, SQBO | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<assignmentStmt>` | ID | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<whichStmt>` | ASSIGNOP, SQBO | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<lvalueIDStmt>` | ASSIGNOP | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<lvalueARRStmt>` | SQBO | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<moduleReuseStmt>` | USE, SQBO | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<optional>` | SQBO, ε | USE |
| `<idList>` | ID | COLON, SQBC |
| `<idListTail>` | COMMA, ε | COLON, SQBC |
| `<actualParaList>` | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS | SEMICOL |
| `<actualParaTail>` | COMMA, ε | SEMICOL |
| `<actualPara>` | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS | SEMICOL, COMMA |
| `<paraValue>` | ID, NUM, RNUM | SEMICOL, COMMA |
| `<expression>` | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO | SEMICOL, BC |
| `<logicalTail>` | AND, OR, ε | SEMICOL, BC |
| `<anyTerm>` | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO | AND, OR, SEMICOL, BC |
| `<relationalTail>` | LT, LE, GT, GE, EQ, NE, ε | AND, OR, SEMICOL, BC |
| `<arithmeticExpr>` | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO | AND, OR, LT, LE, GT, GE, EQ, NE, SEMICOL, BC |
| `<addTail>` | PLUS, MINUS, ε | AND, OR, LT, LE, GT, GE, EQ, NE, SEMICOL, BC |
| `<term>` | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO | AND, OR, PLUS, MINUS, LT, LE, GT, GE, EQ, NE, SEMICOL, BC |
| `<mulTail>` | MUL, DIV, ε | AND, OR, PLUS, MINUS, LT, LE, GT, GE, EQ, NE, SEMICOL, BC |
| `<factor>` | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO | AND, OR, PLUS, MINUS, MUL, DIV, LT, LE, GT, GE, EQ, NE, SEMICOL, BC |
| `<unaryOperand>` | ID, NUM, RNUM, BO | AND, OR, PLUS, MINUS, MUL, DIV, LT, LE, GT, GE, EQ, NE, SEMICOL, BC |
| `<arrayIndex>` | SQBO, ε | AND, OR, PLUS, MINUS, MUL, DIV, LT, LE, GT, GE, EQ, NE, SEMICOL, COMMA, BC |
| `<indexExpr>` | ID, NUM, PLUS, MINUS, BO | SQBC, BC |
| `<indexAddTail>` | PLUS, MINUS, ε | SQBC, BC |
| `<indexTerm>` | ID, NUM, PLUS, MINUS, BO | PLUS, MINUS, SQBC, BC |
| `<indexMulTail>` | MUL, DIV, ε | PLUS, MINUS, SQBC, BC |
| `<indexFactor>` | ID, NUM, PLUS, MINUS, BO | PLUS, MINUS, MUL, DIV, SQBC, BC |
| `<indexUnaryOperand>` | ID, NUM, BO | PLUS, MINUS, MUL, DIV, SQBC, BC |
| `<unaryOp>` | PLUS, MINUS | ID, NUM, RNUM, BO |
| `<addOp>` | PLUS, MINUS | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO |
| `<mulOp>` | MUL, DIV | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO |
| `<logicalOp>` | AND, OR | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO |
| `<relationalOp>` | LT, LE, GT, GE, EQ, NE | TRUE, FALSE, ID, NUM, RNUM, PLUS, MINUS, BO |
| `<declareStmt>` | DECLARE | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<conditionalStmt>` | SWITCH | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<caseStmts>` | CASE | END, DEFAULT |
| `<moreCases>` | CASE, ε | END, DEFAULT |
| `<value>` | TRUE, FALSE, NUM | COLON |
| `<default>` | DEFAULT, ε | END |
| `<iterativeStmt>` | FOR, WHILE | END, DECLARE, GET_VALUE, PRINT, USE, FOR, SWITCH, BREAK, WHILE, ID, SQBO |
| `<rangeForLoop>` | NUM, PLUS, MINUS | BC |
| `<indexForLoop>` | NUM, PLUS, MINUS | RANGEOP, BC |
