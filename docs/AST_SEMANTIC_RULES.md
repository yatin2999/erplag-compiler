# AST Construction Rules

Semantic rules that build the abstract syntax tree (AST) from the parse tree, one for each of the 134 productions of [`grammar.txt`](../grammar.txt), numbered in the order of that file. Each rule gives its evaluation order and its actions in the form

```text
n    production
     { visit X, Y : actions ; visit Z : actions }
```

where `visit X, Y` means that the subtrees of X and Y are processed, from left to right, before the actions that follow are applied. Only address attributes are used: `addr_syn` is synthesized and `addr_inh` is inherited. An address is an AST node, the first node of a linked list of AST nodes, or a token.

The AST is built in one pass over the parse tree. In [`src/ast.c`](../src/ast.c), the function `build` applies the rule of a parse-tree node in a `switch` on its production number. A list (rules 2, 5, 11–16, 32, 59–64 and 123–125) and an expression tail (rules 71–81 and 95–99) are right-recursive in the grammar; `build_list` and `fold_left` follow them with a loop. After the rule of a node has been applied, the node is freed together with the children the AST does not keep: keywords, punctuation, operators, `ε`-nodes and the nonterminal nodes of chains. The tokens of identifiers and literals are moved from the parse-tree leaves into the AST nodes, so no AST node refers to freed memory.

## Notation

| Notation | Meaning |
|---|---|
| `X.addr_syn` | the address synthesized for X: an AST node, the first node of a linked list, or a token |
| `X.addr_inh` | the address passed down to X: the left operand of an expression tail (rules 70–81, 94–99), the assigned variable (rules 51–55), or the module node whose body is filled in (rules 7, 8, 31) |
| `X.low_syn`, `X.high_syn` | the nodes built for the two bounds of a range (rules 20, 21, 131, 133) |
| `T.token` | the token of the terminal T, moved into the AST node |
| `X₁`, `X₂` | X on the right side of a production, numbered to tell it apart from the X on the left side or from another X |
| NULL | no node, an empty list or no token |
| `A ; B` | A and then B |
| `makeNode(K, ...)` | a new AST node of kind K whose children or fields are the arguments |
| `insertAtHead(l, n)` | sets n.next = l and gives the list that starts with n |
| `concat(...)` | the lists and nodes joined in the given order |
| `leaf(T)` | a Number, Real or Boolean node that holds the token of T |
| `var(ID)` | a Variable node, a use of a name, that holds the token of ID |
| `element(v, i)` | v if i is NULL, otherwise `makeNode(Element, v, i)` |
| `signed(s, n)` | n if s is NULL, otherwise `makeNode(Unary, s, n)` |
| `line(T)` | the line number of the token T, stored in the node |

## Program and modules

```text
1    <program> → <moduleDeclarations> <otherModules>₁ <driverModule> <otherModules>₂
     { visit <moduleDeclarations>, <otherModules>₁, <driverModule>, <otherModules>₂ :
         <program>.addr_syn = makeNode(Program, <moduleDeclarations>.addr_syn,
                                concat(<otherModules>₁.addr_syn, <driverModule>.addr_syn, <otherModules>₂.addr_syn)) }
2    <moduleDeclarations> → <moduleDeclaration> <moduleDeclarations>₁
     { visit <moduleDeclaration>, <moduleDeclarations>₁ :
         <moduleDeclarations>.addr_syn = insertAtHead(<moduleDeclarations>₁.addr_syn, <moduleDeclaration>.addr_syn) }
3    <moduleDeclarations> → ε
     { <moduleDeclarations>.addr_syn = NULL }
4    <moduleDeclaration> → DECLARE MODULE ID SEMICOL
     { visit ID : <moduleDeclaration>.addr_syn = makeNode(ModuleDeclaration, ID.token) }
5    <otherModules> → <module> <otherModules>₁
     { visit <module>, <otherModules>₁ : <otherModules>.addr_syn = insertAtHead(<otherModules>₁.addr_syn, <module>.addr_syn) }
6    <otherModules> → ε
     { <otherModules>.addr_syn = NULL }
7    <driverModule> → DRIVERDEF DRIVER PROGRAM DRIVERENDDEF <moduleDef>
     { <moduleDef>.addr_inh = makeNode(Driver) ;
       visit <moduleDef> : <driverModule>.addr_syn = <moduleDef>.addr_inh }
8    <module> → DEF MODULE ID ENDDEF TAKES INPUT SQBO <inputPlist> SQBC SEMICOL <ret> <moduleDef>
     { visit ID, <inputPlist>, <ret> :
         <moduleDef>.addr_inh = makeNode(Module, ID.token, inputs: <inputPlist>.addr_syn, outputs: <ret>.addr_syn) ;
       visit <moduleDef> : <module>.addr_syn = <moduleDef>.addr_inh }
9    <ret> → RETURNS SQBO <outputPlist> SQBC SEMICOL
     { visit <outputPlist> : <ret>.addr_syn = <outputPlist>.addr_syn }
10   <ret> → ε
     { <ret>.addr_syn = NULL }
```

## Parameters, types and ranges

```text
11   <inputPlist> → ID COLON <dataType> <inputPlistTail>
     { visit ID, <dataType>, <inputPlistTail> :
         <inputPlist>.addr_syn = insertAtHead(<inputPlistTail>.addr_syn, makeNode(Parameter, ID.token, <dataType>.addr_syn)) }
12   <inputPlistTail> → COMMA ID COLON <dataType> <inputPlistTail>₁
     { visit ID, <dataType>, <inputPlistTail>₁ :
         <inputPlistTail>.addr_syn = insertAtHead(<inputPlistTail>₁.addr_syn, makeNode(Parameter, ID.token, <dataType>.addr_syn)) }
13   <inputPlistTail> → ε
     { <inputPlistTail>.addr_syn = NULL }
14   <outputPlist> → ID COLON <type> <outputPlistTail>
     { visit ID, <type>, <outputPlistTail> :
         <outputPlist>.addr_syn = insertAtHead(<outputPlistTail>.addr_syn, makeNode(Parameter, ID.token, <type>.addr_syn)) }
15   <outputPlistTail> → COMMA ID COLON <type> <outputPlistTail>₁
     { visit ID, <type>, <outputPlistTail>₁ :
         <outputPlistTail>.addr_syn = insertAtHead(<outputPlistTail>₁.addr_syn, makeNode(Parameter, ID.token, <type>.addr_syn)) }
16   <outputPlistTail> → ε
     { <outputPlistTail>.addr_syn = NULL }
17   <dataType> → INTEGER
     { visit INTEGER : <dataType>.addr_syn = makeNode(Type, INTEGER.token) }
18   <dataType> → REAL
     { visit REAL : <dataType>.addr_syn = makeNode(Type, REAL.token) }
19   <dataType> → BOOLEAN
     { visit BOOLEAN : <dataType>.addr_syn = makeNode(Type, BOOLEAN.token) }
20   <dataType> → ARRAY SQBO <rangeArrays> SQBC OF <type>
     { visit <rangeArrays>, <type> :
         <dataType>.addr_syn = makeNode(ArrayType, low: <rangeArrays>.low_syn, high: <rangeArrays>.high_syn, element: <type>.addr_syn) }
21   <rangeArrays> → <indexArr>₁ RANGEOP <indexArr>₂
     { visit <indexArr>₁, <indexArr>₂ : <rangeArrays>.low_syn = <indexArr>₁.addr_syn ; <rangeArrays>.high_syn = <indexArr>₂.addr_syn }
22   <type> → INTEGER
     { visit INTEGER : <type>.addr_syn = makeNode(Type, INTEGER.token) }
23   <type> → REAL
     { visit REAL : <type>.addr_syn = makeNode(Type, REAL.token) }
24   <type> → BOOLEAN
     { visit BOOLEAN : <type>.addr_syn = makeNode(Type, BOOLEAN.token) }
25   <indexArr> → <sign> <newIndex>
     { visit <sign>, <newIndex> : <indexArr>.addr_syn = signed(<sign>.addr_syn, <newIndex>.addr_syn) }
26   <newIndex> → NUM
     { visit NUM : <newIndex>.addr_syn = leaf(NUM) }
27   <newIndex> → ID
     { visit ID : <newIndex>.addr_syn = var(ID) }
28   <sign> → PLUS
     { visit PLUS : <sign>.addr_syn = PLUS.token }
29   <sign> → MINUS
     { visit MINUS : <sign>.addr_syn = MINUS.token }
30   <sign> → ε
     { <sign>.addr_syn = NULL }
```

## Statements

```text
31   <moduleDef> → START <statements> END
     { visit <statements> : <moduleDef>.addr_inh.body = <statements>.addr_syn ; <moduleDef>.addr_inh.end = line(END) }
32   <statements> → <statement> <statements>₁
     { visit <statement>, <statements>₁ : <statements>.addr_syn = insertAtHead(<statements>₁.addr_syn, <statement>.addr_syn) }
33   <statements> → ε
     { <statements>.addr_syn = NULL }
34   <statement> → <ioStmt>
     { visit <ioStmt> : <statement>.addr_syn = <ioStmt>.addr_syn }
35   <statement> → <simpleStmt>
     { visit <simpleStmt> : <statement>.addr_syn = <simpleStmt>.addr_syn }
36   <statement> → <declareStmt>
     { visit <declareStmt> : <statement>.addr_syn = <declareStmt>.addr_syn }
37   <statement> → <conditionalStmt>
     { visit <conditionalStmt> : <statement>.addr_syn = <conditionalStmt>.addr_syn }
38   <statement> → <iterativeStmt>
     { visit <iterativeStmt> : <statement>.addr_syn = <iterativeStmt>.addr_syn }
```

## Input and output

```text
39   <ioStmt> → GET_VALUE BO ID BC SEMICOL
     { visit ID : <ioStmt>.addr_syn = makeNode(GetValue, var(ID)) }
40   <ioStmt> → PRINT BO <varPrint> BC SEMICOL
     { visit <varPrint> : <ioStmt>.addr_syn = makeNode(Print, <varPrint>.addr_syn) }
41   <varPrint> → ID <printIndex>
     { visit ID, <printIndex> : <varPrint>.addr_syn = element(var(ID), <printIndex>.addr_syn) }
42   <varPrint> → NUM
     { visit NUM : <varPrint>.addr_syn = leaf(NUM) }
43   <varPrint> → RNUM
     { visit RNUM : <varPrint>.addr_syn = leaf(RNUM) }
44   <varPrint> → <boolConst>
     { visit <boolConst> : <varPrint>.addr_syn = <boolConst>.addr_syn }
45   <printIndex> → SQBO <indexArr> SQBC
     { visit <indexArr> : <printIndex>.addr_syn = <indexArr>.addr_syn }
46   <printIndex> → ε
     { <printIndex>.addr_syn = NULL }
47   <boolConst> → TRUE
     { visit TRUE : <boolConst>.addr_syn = leaf(TRUE) }
48   <boolConst> → FALSE
     { visit FALSE : <boolConst>.addr_syn = leaf(FALSE) }
```

## Assignments

```text
49   <simpleStmt> → <assignmentStmt>
     { visit <assignmentStmt> : <simpleStmt>.addr_syn = <assignmentStmt>.addr_syn }
50   <simpleStmt> → <moduleReuseStmt>
     { visit <moduleReuseStmt> : <simpleStmt>.addr_syn = <moduleReuseStmt>.addr_syn }
51   <assignmentStmt> → ID <whichStmt>
     { visit ID : <whichStmt>.addr_inh = var(ID) ;
       visit <whichStmt> : <assignmentStmt>.addr_syn = <whichStmt>.addr_syn }
52   <whichStmt> → <lvalueIDStmt>
     { <lvalueIDStmt>.addr_inh = <whichStmt>.addr_inh ;
       visit <lvalueIDStmt> : <whichStmt>.addr_syn = <lvalueIDStmt>.addr_syn }
53   <whichStmt> → <lvalueARRStmt>
     { <lvalueARRStmt>.addr_inh = <whichStmt>.addr_inh ;
       visit <lvalueARRStmt> : <whichStmt>.addr_syn = <lvalueARRStmt>.addr_syn }
54   <lvalueIDStmt> → ASSIGNOP <expression> SEMICOL
     { visit <expression> : <lvalueIDStmt>.addr_syn = makeNode(Assign, <lvalueIDStmt>.addr_inh, <expression>.addr_syn) }
55   <lvalueARRStmt> → SQBO <indexExpr> SQBC ASSIGNOP <expression> SEMICOL
     { visit <indexExpr>, <expression> :
         <lvalueARRStmt>.addr_syn = makeNode(AssignElement, element(<lvalueARRStmt>.addr_inh, <indexExpr>.addr_syn), <expression>.addr_syn) }
```

## Module reuse

```text
56   <moduleReuseStmt> → <optional> USE MODULE ID WITH PARAMETERS <actualParaList> SEMICOL
     { visit <optional>, ID, <actualParaList> :
         <moduleReuseStmt>.addr_syn = makeNode(ModuleCall, results: <optional>.addr_syn, module: ID.token,
                                               arguments: <actualParaList>.addr_syn) }
57   <optional> → SQBO <idList> SQBC ASSIGNOP
     { visit <idList> : <optional>.addr_syn = <idList>.addr_syn }
58   <optional> → ε
     { <optional>.addr_syn = NULL }
59   <idList> → ID <idListTail>
     { visit ID, <idListTail> : <idList>.addr_syn = insertAtHead(<idListTail>.addr_syn, var(ID)) }
60   <idListTail> → COMMA ID <idListTail>₁
     { visit ID, <idListTail>₁ : <idListTail>.addr_syn = insertAtHead(<idListTail>₁.addr_syn, var(ID)) }
61   <idListTail> → ε
     { <idListTail>.addr_syn = NULL }
62   <actualParaList> → <actualPara> <actualParaTail>
     { visit <actualPara>, <actualParaTail> : <actualParaList>.addr_syn = insertAtHead(<actualParaTail>.addr_syn, <actualPara>.addr_syn) }
63   <actualParaTail> → COMMA <actualPara> <actualParaTail>₁
     { visit <actualPara>, <actualParaTail>₁ : <actualParaTail>.addr_syn = insertAtHead(<actualParaTail>₁.addr_syn, <actualPara>.addr_syn) }
64   <actualParaTail> → ε
     { <actualParaTail>.addr_syn = NULL }
65   <actualPara> → <boolConst>
     { visit <boolConst> : <actualPara>.addr_syn = <boolConst>.addr_syn }
66   <actualPara> → <sign> <paraValue>
     { visit <sign>, <paraValue> : <actualPara>.addr_syn = signed(<sign>.addr_syn, <paraValue>.addr_syn) }
67   <paraValue> → NUM
     { visit NUM : <paraValue>.addr_syn = leaf(NUM) }
68   <paraValue> → RNUM
     { visit RNUM : <paraValue>.addr_syn = leaf(RNUM) }
69   <paraValue> → ID <arrayIndex>
     { visit ID, <arrayIndex> : <paraValue>.addr_syn = element(var(ID), <arrayIndex>.addr_syn) }
```

## Expressions

```text
70   <expression> → <anyTerm> <logicalTail>
     { visit <anyTerm> : <logicalTail>.addr_inh = <anyTerm>.addr_syn ;
       visit <logicalTail> : <expression>.addr_syn = <logicalTail>.addr_syn }
71   <logicalTail> → <logicalOp> <anyTerm> <logicalTail>₁
     { visit <logicalOp>, <anyTerm> : <logicalTail>₁.addr_inh = makeNode(Binary, <logicalOp>.addr_syn, <logicalTail>.addr_inh, <anyTerm>.addr_syn) ;
       visit <logicalTail>₁ : <logicalTail>.addr_syn = <logicalTail>₁.addr_syn }
72   <logicalTail> → ε
     { <logicalTail>.addr_syn = <logicalTail>.addr_inh }
73   <anyTerm> → <arithmeticExpr> <relationalTail>
     { visit <arithmeticExpr> : <relationalTail>.addr_inh = <arithmeticExpr>.addr_syn ;
       visit <relationalTail> : <anyTerm>.addr_syn = <relationalTail>.addr_syn }
74   <relationalTail> → <relationalOp> <arithmeticExpr>
     { visit <relationalOp>, <arithmeticExpr> :
         <relationalTail>.addr_syn = makeNode(Binary, <relationalOp>.addr_syn, <relationalTail>.addr_inh, <arithmeticExpr>.addr_syn) }
75   <relationalTail> → ε
     { <relationalTail>.addr_syn = <relationalTail>.addr_inh }
76   <arithmeticExpr> → <term> <addTail>
     { visit <term> : <addTail>.addr_inh = <term>.addr_syn ;
       visit <addTail> : <arithmeticExpr>.addr_syn = <addTail>.addr_syn }
77   <addTail> → <addOp> <term> <addTail>₁
     { visit <addOp>, <term> : <addTail>₁.addr_inh = makeNode(Binary, <addOp>.addr_syn, <addTail>.addr_inh, <term>.addr_syn) ;
       visit <addTail>₁ : <addTail>.addr_syn = <addTail>₁.addr_syn }
78   <addTail> → ε
     { <addTail>.addr_syn = <addTail>.addr_inh }
79   <term> → <factor> <mulTail>
     { visit <factor> : <mulTail>.addr_inh = <factor>.addr_syn ;
       visit <mulTail> : <term>.addr_syn = <mulTail>.addr_syn }
80   <mulTail> → <mulOp> <factor> <mulTail>₁
     { visit <mulOp>, <factor> : <mulTail>₁.addr_inh = makeNode(Binary, <mulOp>.addr_syn, <mulTail>.addr_inh, <factor>.addr_syn) ;
       visit <mulTail>₁ : <mulTail>.addr_syn = <mulTail>₁.addr_syn }
81   <mulTail> → ε
     { <mulTail>.addr_syn = <mulTail>.addr_inh }
82   <factor> → BO <expression> BC
     { visit <expression> : <factor>.addr_syn = <expression>.addr_syn }
83   <factor> → NUM
     { visit NUM : <factor>.addr_syn = leaf(NUM) }
84   <factor> → RNUM
     { visit RNUM : <factor>.addr_syn = leaf(RNUM) }
85   <factor> → <boolConst>
     { visit <boolConst> : <factor>.addr_syn = <boolConst>.addr_syn }
86   <factor> → ID <arrayIndex>
     { visit ID, <arrayIndex> : <factor>.addr_syn = element(var(ID), <arrayIndex>.addr_syn) }
87   <factor> → <unaryOp> <unaryOperand>
     { visit <unaryOp>, <unaryOperand> : <factor>.addr_syn = signed(<unaryOp>.addr_syn, <unaryOperand>.addr_syn) }
88   <unaryOperand> → BO <expression> BC
     { visit <expression> : <unaryOperand>.addr_syn = <expression>.addr_syn }
89   <unaryOperand> → NUM
     { visit NUM : <unaryOperand>.addr_syn = leaf(NUM) }
90   <unaryOperand> → RNUM
     { visit RNUM : <unaryOperand>.addr_syn = leaf(RNUM) }
91   <unaryOperand> → ID <arrayIndex>
     { visit ID, <arrayIndex> : <unaryOperand>.addr_syn = element(var(ID), <arrayIndex>.addr_syn) }
```

## Array indices

```text
92   <arrayIndex> → SQBO <indexExpr> SQBC
     { visit <indexExpr> : <arrayIndex>.addr_syn = <indexExpr>.addr_syn }
93   <arrayIndex> → ε
     { <arrayIndex>.addr_syn = NULL }
94   <indexExpr> → <indexTerm> <indexAddTail>
     { visit <indexTerm> : <indexAddTail>.addr_inh = <indexTerm>.addr_syn ;
       visit <indexAddTail> : <indexExpr>.addr_syn = <indexAddTail>.addr_syn }
95   <indexAddTail> → <addOp> <indexTerm> <indexAddTail>₁
     { visit <addOp>, <indexTerm> : <indexAddTail>₁.addr_inh = makeNode(Binary, <addOp>.addr_syn, <indexAddTail>.addr_inh, <indexTerm>.addr_syn) ;
       visit <indexAddTail>₁ : <indexAddTail>.addr_syn = <indexAddTail>₁.addr_syn }
96   <indexAddTail> → ε
     { <indexAddTail>.addr_syn = <indexAddTail>.addr_inh }
97   <indexTerm> → <indexFactor> <indexMulTail>
     { visit <indexFactor> : <indexMulTail>.addr_inh = <indexFactor>.addr_syn ;
       visit <indexMulTail> : <indexTerm>.addr_syn = <indexMulTail>.addr_syn }
98   <indexMulTail> → <mulOp> <indexFactor> <indexMulTail>₁
     { visit <mulOp>, <indexFactor> : <indexMulTail>₁.addr_inh = makeNode(Binary, <mulOp>.addr_syn, <indexMulTail>.addr_inh, <indexFactor>.addr_syn) ;
       visit <indexMulTail>₁ : <indexMulTail>.addr_syn = <indexMulTail>₁.addr_syn }
99   <indexMulTail> → ε
     { <indexMulTail>.addr_syn = <indexMulTail>.addr_inh }
100  <indexFactor> → ID
     { visit ID : <indexFactor>.addr_syn = var(ID) }
101  <indexFactor> → NUM
     { visit NUM : <indexFactor>.addr_syn = leaf(NUM) }
102  <indexFactor> → BO <indexExpr> BC
     { visit <indexExpr> : <indexFactor>.addr_syn = <indexExpr>.addr_syn }
103  <indexFactor> → <unaryOp> <indexUnaryOperand>
     { visit <unaryOp>, <indexUnaryOperand> : <indexFactor>.addr_syn = signed(<unaryOp>.addr_syn, <indexUnaryOperand>.addr_syn) }
104  <indexUnaryOperand> → ID
     { visit ID : <indexUnaryOperand>.addr_syn = var(ID) }
105  <indexUnaryOperand> → NUM
     { visit NUM : <indexUnaryOperand>.addr_syn = leaf(NUM) }
106  <indexUnaryOperand> → BO <indexExpr> BC
     { visit <indexExpr> : <indexUnaryOperand>.addr_syn = <indexExpr>.addr_syn }
```

## Operators

```text
107  <unaryOp> → PLUS
     { visit PLUS : <unaryOp>.addr_syn = PLUS.token }
108  <unaryOp> → MINUS
     { visit MINUS : <unaryOp>.addr_syn = MINUS.token }
109  <addOp> → PLUS
     { visit PLUS : <addOp>.addr_syn = PLUS.token }
110  <addOp> → MINUS
     { visit MINUS : <addOp>.addr_syn = MINUS.token }
111  <mulOp> → MUL
     { visit MUL : <mulOp>.addr_syn = MUL.token }
112  <mulOp> → DIV
     { visit DIV : <mulOp>.addr_syn = DIV.token }
113  <logicalOp> → AND
     { visit AND : <logicalOp>.addr_syn = AND.token }
114  <logicalOp> → OR
     { visit OR : <logicalOp>.addr_syn = OR.token }
115  <relationalOp> → LT
     { visit LT : <relationalOp>.addr_syn = LT.token }
116  <relationalOp> → LE
     { visit LE : <relationalOp>.addr_syn = LE.token }
117  <relationalOp> → GT
     { visit GT : <relationalOp>.addr_syn = GT.token }
118  <relationalOp> → GE
     { visit GE : <relationalOp>.addr_syn = GE.token }
119  <relationalOp> → EQ
     { visit EQ : <relationalOp>.addr_syn = EQ.token }
120  <relationalOp> → NE
     { visit NE : <relationalOp>.addr_syn = NE.token }
```

## Declarations, switch and loops

```text
121  <declareStmt> → DECLARE <idList> COLON <dataType> SEMICOL
     { visit <idList>, <dataType> : <declareStmt>.addr_syn = makeNode(Declare, <idList>.addr_syn, <dataType>.addr_syn) }
122  <conditionalStmt> → SWITCH BO ID BC START <caseStmts> <default> END
     { visit ID, <caseStmts>, <default> :
         <conditionalStmt>.addr_syn = makeNode(Switch, var(ID), <caseStmts>.addr_syn, <default>.addr_syn, end: line(END)) }
123  <caseStmts> → CASE <value> COLON <statements> BREAK SEMICOL <moreCases>
     { visit <value>, <statements>, <moreCases> :
         <caseStmts>.addr_syn = insertAtHead(<moreCases>.addr_syn,
                                             makeNode(Case, <value>.addr_syn, <statements>.addr_syn, break: line(BREAK))) }
124  <moreCases> → CASE <value> COLON <statements> BREAK SEMICOL <moreCases>₁
     { visit <value>, <statements>, <moreCases>₁ :
         <moreCases>.addr_syn = insertAtHead(<moreCases>₁.addr_syn,
                                             makeNode(Case, <value>.addr_syn, <statements>.addr_syn, break: line(BREAK))) }
125  <moreCases> → ε
     { <moreCases>.addr_syn = NULL }
126  <value> → NUM
     { visit NUM : <value>.addr_syn = leaf(NUM) }
127  <value> → TRUE
     { visit TRUE : <value>.addr_syn = leaf(TRUE) }
128  <value> → FALSE
     { visit FALSE : <value>.addr_syn = leaf(FALSE) }
129  <default> → DEFAULT COLON <statements> BREAK SEMICOL
     { visit <statements> : <default>.addr_syn = makeNode(Default, <statements>.addr_syn, break: line(BREAK)) }
130  <default> → ε
     { <default>.addr_syn = NULL }
131  <iterativeStmt> → FOR BO ID IN <rangeForLoop> BC START <statements> END
     { visit ID, <rangeForLoop>, <statements> :
         <iterativeStmt>.addr_syn = makeNode(For, var(ID), <rangeForLoop>.low_syn, <rangeForLoop>.high_syn,
                                             <statements>.addr_syn, end: line(END)) }
132  <iterativeStmt> → WHILE BO <expression> BC START <statements> END
     { visit <expression>, <statements> :
         <iterativeStmt>.addr_syn = makeNode(While, <expression>.addr_syn, <statements>.addr_syn, end: line(END)) }
133  <rangeForLoop> → <indexForLoop>₁ RANGEOP <indexForLoop>₂
     { visit <indexForLoop>₁, <indexForLoop>₂ :
         <rangeForLoop>.low_syn = <indexForLoop>₁.addr_syn ; <rangeForLoop>.high_syn = <indexForLoop>₂.addr_syn }
134  <indexForLoop> → <sign> NUM
     { visit <sign>, NUM : <indexForLoop>.addr_syn = signed(<sign>.addr_syn, leaf(NUM)) }
```
