# ERPLAG Language Specification

ERPLAG is a strongly typed language with three primitive data types: `integer`, `real` (floating-point numbers) and `boolean`. It also supports arrays. The language has arithmetic and boolean expressions, and its statements are declarations, assignments, input/output statements, module calls, a conditional statement and loops. Programs are built from modules (functions); a module can return several values, or none. The scope of a variable is static: a variable is visible only in the block where it is declared and in the blocks nested inside it.

The syntax of ERPLAG is defined by the grammar in [`grammar.txt`](grammar.txt) (section 5).

## 1. Lexical structure

### 1.1 Keywords and identifiers

The keywords are listed in Table 1 (section 4). These keywords and the words `true`, `false`, `AND` and `OR` (Table 2) are reserved: they cannot be used as identifiers.

An identifier is a sequence of letters, digits and underscores that starts with a letter or an underscore: `[a-zA-Z_][a-zA-Z0-9_]*`. It is at most 20 characters long and is tokenized as ID. `computeSum`, `C`, `A_1`, `a2cD2___`, `_Abc`, `abc_123` and `abc123` are identifiers; `1abc` is not.

The language is case sensitive: `switch` is a keyword, while `SWITCH` and `Switch` are identifiers, and `Value` and `value` are two different variables.

### 1.2 White spaces, separators and comments

Blanks, tabs and newlines are white space. White space separates tokens and is otherwise ignored, so `value:integer` and `value : integer` are both the tokens ID, COLON and INTEGER. Two keywords or identifiers must be separated by white space or another token: `valueabc` is one identifier, and `val ue` is two.

A semicolon ends every statement except `switch`, `for` and `while`, which end with `end`. A semicolon also ends each module declaration, the `takes input` and `returns` lines of a module, and each `break`.

A comment starts with `**` and ends with the next `**`. It may span several lines and is ignored.

```erplag
x := y;    ** a comment **
```

### 1.3 Numbers

An integer number is a sequence of digits, such as `234`, `1` or `90123`. It is tokenized as NUM and has type `integer`.

A real number is a sequence of digits, a decimal point and a fraction part of one or more digits, such as `23.89`, `908.567`, `25.0` or `0.25`. It may have an exponent: `E` or `e`, an optional `+` or `-`, and digits, as in `123.2E+6`, `124.2E-1`, `124.2e-1` or `1.24E1`. An exponent without a sign is positive. A real number is tokenized as RNUM and has type `real`. A real number needs digits on both sides of the decimal point, and an exponent needs digits: `26.`, `.25`, `.124E+2` and `12.5E` are not numbers.

Numbers are unsigned. A `+` or `-` in front of a number is an operator (section 2.2). An integer number is at most 9223372036854775807, the largest `integer` value, and a real number is at most the largest double-precision value, about 1.8E+308; a real number too small for double precision becomes 0. Leading zeros are allowed: `007` is the number 7.

### 1.4 Operators

The arithmetic operators are `+`, `-`, `*` and `/`. The relational operators are `<`, `<=`, `>`, `>=`, `==` and `!=`. The logical operators are `AND` and `OR`, written in upper case only. The assignment operator is `:=`.

The range operator `..` separates the two bounds of a range, as in `declare C: array[1..10] of integer;`. It is different from the decimal point of a real number: `1..10` is the number `1`, the range operator and the number `10`.

In a module definition, `<<` and `>>` enclose `module` and the module name, and `<<<` and `>>>` enclose `driver program`. Table 2 (section 4) gives the token of each operator and delimiter.

### 1.5 Lexical errors

A lexical error is reported, with its line number, for an identifier longer than 20 characters, a malformed number or a number that is too large (section 1.3), a comment that is never closed, and a character that cannot begin a token, such as `$` or a single `=`, `!` or `.`.

## 2. Language features

### 2.1 Data types

ERPLAG has four data types: `integer`, `real`, `boolean` and arrays. The first three are the primitive types.

| Type | Values | Constants |
|---|---|---|
| `integer` | whole numbers from -9223372036854775808 to 9223372036854775807 | integer numbers, such as `5` |
| `real` | double-precision floating-point numbers | real numbers, such as `56.34` |
| `boolean` | `true` and `false` | `true`, `false` |

A boolean is not a number: `true` and `false` cannot be used as 1 and 0, and no operation converts between `boolean` and the numeric types. An `integer` value is not converted to `real` either, except by division, which always gives a `real` result (section 2.2).

**Arrays.** An array is a one-dimensional collection of elements of one primitive type, declared over a range of indices. `declare C: array[5..10] of real;` declares an array of six real numbers, `C[5]`, `C[6]`, …, `C[10]`. Each bound is an integer number or an `integer` variable, optionally preceded by `+` or `-`:

- A **static array** has two numbers as bounds, as in `array[-5..10] of real` (16 elements). Its lower bound must not exceed its upper bound. Its elements are part of the activation record of its module, and a declaration that would make the variables of a module need more than 2147483632 bytes is a compile-time error.
- A **dynamic array** has at least one variable as a bound, as in `array[a..b] of integer` or `array[5..-b] of real`. The bounds are evaluated when the declaration is executed, and a lower bound greater than the upper bound is then a run-time error.

An element is written `A[index]`. The index is an `integer` expression made of integer numbers, variables, arithmetic operators and parentheses, as in `M[-2]`, `C[k]` or `P[a+b-7]`. It cannot contain an array element, so `A[A[i]]` is invalid, and `A[i/2]` is a type error because `/` gives a `real` value. Every index is checked against the bounds of its array: by the compiler when the array is static and the index is an integer number, possibly signed or in parentheses (`A[5]`, `A[-1]`, `A[(7)]`), and otherwise when the program runs.

A whole array cannot be an operand. For arrays P and Q, `P+Q` is not allowed, while `P[7]+Q[3]` is.

**Type equivalence.** Two array types are structurally equivalent when they have the same element type and the same number of elements; their bounds may differ. `array[-3..12] of real` and `array[-20..-5] of real` are equivalent, while `array[-3..12] of real` and `array[-20..-4] of real` are not. For a dynamic array, the number of elements is compared when the program runs.

### 2.2 Expressions

An expression is arithmetic or boolean. Its operands are numbers, `true`, `false`, variables of primitive types, array elements and expressions in parentheses.

The operators have the following precedence, from the highest to the lowest:

1. parentheses, and unary `+` and `-`;
2. `*` and `/`;
3. binary `+` and `-`;
4. the relational operators `<`, `<=`, `>`, `>=`, `==` and `!=`;
5. `AND` and `OR`.

Operators of the same precedence are left associative. For example, `15+29-12*2` is 20, `15+(29-12)*2` is 49, `a/b*c` means `(a/b)*c`, and `p OR q AND r` means `(p OR q) AND r`. A relational operator compares two arithmetic expressions, and comparisons cannot be chained: `a < b < c` is not an expression. The boolean expression `((x>=10 AND y<0) OR x<10)` is true when x is 6 and y is -10.

The type of an expression follows from the types of its operands:

| Operator | Operand types | Result type |
|---|---|---|
| `+`, `-`, `*` | both `integer`, or both `real` | the type of the operands |
| `/` | `integer` or `real` | `real` |
| `<`, `<=`, `>`, `>=`, `==`, `!=` | both `integer`, or both `real` | `boolean` |
| `AND`, `OR` | both `boolean` | `boolean` |
| unary `+`, `-` | `integer` or `real` | the type of the operand |

Any other combination is a type error. For example, `2 * 1.5`, a comparison of an `integer` with a `real`, `true == false` and `true AND 1` are errors. Division by zero is a run-time error.

### 2.3 Statements

ERPLAG has five kinds of statements: declarative, simple (assignments and module calls), input/output, conditional and iterative statements.

**Declarations.** `declare a, b, c: integer;` declares the variables `a`, `b` and `c` of type `integer`. A declaration may appear anywhere among the statements, and a variable must be declared before it is used. A name cannot be declared twice in the same scope (section 2.5).

**Assignments.** `x := E;` assigns the value of the expression `E` to the variable `x`, and `A[i] := E;` assigns it to an element of the array `A`. The type of `E` must be the type of `x`, or the element type of `A`.

A whole array can be assigned another structurally equivalent array. After `A := B;`, `A` refers to the elements of `B` instead of its own: the two names share the same elements, and `A` keeps its own index range, so `A[lowA + k]` is the element `B[lowB + k]`. The elements that `A` referred to before are no longer accessible through `A`. If either array is dynamic, the numbers of elements are compared when the assignment is executed.

**Module calls.** A module reuse statement calls a module (section 2.4) and, if the module has output parameters, assigns its results to the variables in square brackets:

```erplag
[r, m] := use module mod1 with parameters v, w;
use module report with parameters A, 5, -x, true;
```

**Input and output.** `get_value(v);` reads a value from the keyboard into the variable `v`; if `v` is an array, it reads a value for each element, in increasing index order. Only a variable can be read: `get_value(A[i])` and `get_value(5)` are syntax errors. `print(x);` writes the value of `x` on the screen, where `x` is a variable, an array element, an unsigned number, `true` or `false`; an array is printed with all its elements, in increasing index order, on one line. The index of an element in `print` is an integer number or a variable, optionally signed: `print(A[4])`, `print(A[-4])` and `print(A[k])` are valid, while `print(A[k+1])` is not. Section 2.7 gives the input and output formats.

**Conditional statement.** The only conditional statement is `switch`:

```erplag
switch (n)
start
	case 1: print(1);
		break;
	case 2: print(2);
		break;
	default: print(0);
		 break;
end
```

The selector in parentheses is a variable of type `integer` or `boolean`; a `real` or array selector is an error. Each case value is an unsigned integer number, `true` or `false`, has the type of the selector and appears only once. The statements of each case end with `break`. Only the statements of the matching case are executed, or those of `default` when no case matches. A `switch` with an `integer` selector must have a `default` part. A `switch` with a `boolean` selector has exactly the cases `true` and `false`, and no `default`.

ERPLAG has no `if` statement; a `switch` with a boolean selector serves the same purpose:

```erplag
flag := a > b;
switch (flag)
start
	case true: print(a);
		break;
	case false: print(b);
		break;
end
```

**Iterative statements.** The `for` loop iterates over a range of integers:

```erplag
for (k in -2..3)
start
	x := x + 2*k;
end
```

The bounds of the range are integer numbers, optionally signed. The loop variable is an `integer` variable that exists only inside the loop: it needs no declaration, and it hides any variable or parameter with the same name. It takes every value from the first bound to the second, increasing by 1 if the first bound is smaller and decreasing by 1 if it is greater; both bounds are included. The loop variable must not be changed in the loop body, by an assignment, `get_value` or a result list. With `x` initially 12, the loop above gives `x` the values 8, 6, 6, 8, 12 and 18.

The `while` loop executes its body as long as its condition, a `boolean` expression, is true:

```erplag
while (i <= k)
start
	get_value(tempvar);
	arr1[i] := tempvar;
	i := i + 1;
end
```

The condition is evaluated before each iteration. At least one variable of the condition must be assigned in the loop body, by an assignment, `get_value` or a result list; assigning an element of an array counts as assigning the array.

### 2.4 Functions

Functions are written as modules. A module definition has a name, input parameters, optional output parameters and a body:

```erplag
<<module sum>>
takes input [a: integer, b: integer];
returns [x: integer, abc: real];
start
	x := a + b;
	abc := a / b;
end
```

A module has at least one input parameter. An input parameter has type `integer`, `real`, `boolean` or an array type whose bounds are integer numbers, such as `array[1..10] of real` or `array[-10..4] of boolean`. Output parameters follow `returns` and have type `integer`, `real` or `boolean`; a module without output parameters has no `returns` line. All parameters of a module have different names, and every output parameter must be assigned a value in the module body.

A module is called with a module reuse statement (section 2.3), and the call must agree with the definition:

- The actual parameters are numbers, `true`, `false`, variables, whole arrays and array elements, but not expressions. A `+` or `-` sign may be applied only to a parameter of type `integer` or `real`.
- The number of actual parameters equals the number of input parameters, and each one has the type of its input parameter. An array must be structurally equivalent to the array type of its parameter.
- A module with output parameters is called with a result list holding one variable of the matching type for each output parameter. A module without output parameters is called without a result list.

**Parameter passing.** Parameters of primitive types are passed by value: the module receives a copy and cannot change the caller's variable. An array is not copied: the parameter refers to the caller's array, as if passed by reference, so the module reads and changes the caller's elements. It uses the parameter's own index range: element `low + k` of the parameter is the element at position `k` of the caller's array. Assigning a whole array to an array parameter changes only what the parameter refers to inside the module, not the caller's array. If the actual array is dynamic, its size is compared with the size of the parameter when the call is executed. When the module ends, the values of its output parameters are assigned, in order, to the variables of the result list.

**Declaration and definition order.** A module can be called in any module that is defined after it, including the driver module. To call it from a module that comes earlier in the program, the module must be declared at the beginning of the program with `declare module name;`. A declaration that no such call needs is redundant and is an error. A module cannot be declared twice, and a declared module must be defined. ERPLAG supports neither recursion nor overloading: a module cannot call itself, directly or through other modules, and cannot be defined twice. Module names are separate from variable names.

### 2.5 Scope of variables

Scopes are static and nest as the program text does. Each of the following is a scope:

- the parameters of a module;
- the body of a module, from `start` to `end`, nested in the parameter scope;
- the body of a `for` or `while` loop; the scope of a `for` loop also contains its loop variable;
- each `case` and the `default` part of a `switch`, up to its `break`.

A variable is visible from its declaration to the end of its scope, including the scopes nested in it; outside its scope, the name is undeclared. A name cannot be declared twice in the same scope. A declaration in a nested scope may hide a variable of an enclosing scope, and because the parameters have their own scope, a local variable may also hide an input parameter. An output parameter cannot be redeclared anywhere in its module.

### 2.6 Program structure

A program is a single source file. It consists of, in this order:

1. any module declarations (`declare module name;`);
2. any module definitions;
3. the driver module, `<<<driver program>>>`, which has no parameters;
4. any further module definitions.

Execution starts with the first statement of the driver module and ends after its last statement. Sample 1 in section 3 has a module declaration, the driver module and a module defined after it.

### 2.7 Run-time behaviour

Each input prompt and each output value is written on its own line. The values for an array are typed separated by white space.

| Statement | Console text |
|---|---|
| `get_value(v)`, `v` of type `integer` | `Input: Enter an integer value` |
| `get_value(v)`, `v` of type `real` | `Input: Enter a real value` |
| `get_value(v)`, `v` of type `boolean` | `Input: Enter a boolean value` (the user types `1` for true or `0` for false) |
| `get_value(A)`, `A` an array | `Input: Enter <n> array elements of <type> type for range <low> to <high>`, such as `Input: Enter 5 array elements of integer type for range 6 to 10` |
| `print(x)`, `x` of type `integer` | `Output: 10` |
| `print(x)`, `x` of type `real` | `Output: 3.5000` (four decimal places) |
| `print(x)`, `x` of type `boolean` | `Output: true` or `Output: false` |
| `print(A)`, `A` an array | `Output: 2 -6 4 10 -8` (each element in the format of its type) |

A program that reaches the end of the driver module ends with exit status 0. When a run-time check fails, the program prints one of the following messages after any earlier output and stops with exit status 1:

| Situation | Message |
|---|---|
| an array index outside the bounds of the array | `RUN TIME ERROR: Array index out of bound` |
| an array assignment or an array parameter with arrays of different sizes | `RUN TIME ERROR: Array size mismatch` |
| a dynamic array whose lower bound is greater than its upper bound | `RUN TIME ERROR: Invalid array range` |
| a division by zero | `RUN TIME ERROR: Division by zero` |

### 2.8 Compile-time errors

The compiler reports lexical errors (section 1.5) and syntax errors, which are token sequences that the grammar does not allow, with their line numbers. A program with lexical or syntax errors is not analysed further. In a program without them, every semantic error below is reported at the line of the item in the last column; "statement" means the line where the statement begins.

| # | Semantic error | Line |
|---|---|---|
| 1 | a variable is not declared, or its declaration is not visible (for an array bound, see rule 12) | the variable |
| 2 | a variable is declared twice in the same scope | the second declaration |
| 3 | an output parameter is redeclared | the redeclaration |
| 4 | two parameters of a module have the same name | the second parameter |
| 5 | an operator has operands of the wrong types (section 2.2) | the operator |
| 6 | a whole array is used as an operand | the array |
| 7 | the assigned value has the wrong type | statement |
| 8 | a variable that is not an array is indexed | the variable |
| 9 | an index is not of type `integer` | the indexed variable |
| 10 | a constant index is outside the bounds of a static array (section 2.1) | the indexed variable |
| 11 | the arrays of an array assignment are not structurally equivalent | statement |
| 12 | an array bound is undeclared or not an `integer` variable, or a static array's lower bound exceeds its upper bound | the declaration |
| 13 | a bound of an array parameter is not an integer number | the parameter |
| 14 | a module is declared more than once | the repeated declaration |
| 15 | a module is declared but never defined | the declaration |
| 16 | a module declaration is redundant | the module definition |
| 17 | a module is defined more than once | the repeated definition |
| 18 | a called module is neither defined nor declared | statement |
| 19 | a module is called before its definition without a declaration | statement |
| 20 | a module calls itself, directly or through other modules | statement of each such call |
| 21 | the number of actual parameters is wrong | statement |
| 22 | an actual parameter has the wrong type | statement |
| 23 | a sign is applied to a parameter that is not `integer` or `real` | the parameter |
| 24 | a module without output parameters is called with a result list | statement |
| 25 | a module with output parameters is called without a result list | statement |
| 26 | the number of result variables is wrong | statement |
| 27 | a result variable has the wrong type | statement |
| 28 | an output parameter is never assigned | the module's `end` |
| 29 | a `switch` selector is not `integer` or `boolean` | the `switch` |
| 30 | a case value does not match the type of the selector | the `case` |
| 31 | two cases have the same value | the later `case` |
| 32 | an `integer` switch has no `default` | the switch's `end` |
| 33 | a `boolean` switch has a `default` | the `default` |
| 34 | a `boolean` switch lacks the `true` or the `false` case | the switch's `end` |
| 35 | the loop variable of a `for` loop is assigned in the loop | statement |
| 36 | a `while` condition is not `boolean` | the `while` |
| 37 | no variable of a `while` condition is assigned in the loop body | the loop's `end` |

The messages for rules 28, 32, 34 and 37 also give the first and last lines of the module, `switch` or `while` loop concerned. Checks that depend on an earlier error are skipped to avoid further errors caused by the same mistake. A declaration rejected under rules 2 to 4 has no effect.

## 3. Sample ERPLAG programs

**Sample 1.** A module defined after the driver module, its declaration, and a call that returns two results. For the input 19, the program prints 14 and 11.4000.

```erplag
declare module mod1;
<<<driver program>>>
start
	declare v, w, r: integer;
	get_value(v);
	w := 5;
	declare m: real;
	[r, m] := use module mod1 with parameters v, w;
	print(r);
	print(m);
end

<<module mod1>>
takes input [a: integer, b: integer];
returns [x: integer, abc: real];
start
	declare c: real;
	c := 10.4;
	x := a + b - 10;
	abc := b / 5 + c;
end
```

**Sample 2.** A `boolean` variable and a `switch` statement. `b - a` is 2, so `c` is false and the program prints -100.

```erplag
<<<driver program>>>
start
	declare a, b: integer;
	declare c: boolean;
	a := 21;
	b := 23;
	c := (b - a > 3);
	switch (c)
	start
		case true: b := 100;
			break;
		case false: b := -100;
			break;
	end
	print(b);
end
```

**Sample 3.** Identifiers with underscores, a module that returns no value, the operator `AND` and declarations among the statements. The program prints 54.

```erplag
<<module mod1>>
takes input [index: integer, val_: integer];
** this module does not return any value **
start
	declare i_1: integer;
	i_1 := val_ + index - 4;
	print(i_1);
end

<<<driver program>>>
start
	declare a, b, dummy: integer;
	a := 48;
	b := 10;
	dummy := 100;
	declare flag: boolean;
	flag := (a >= 30) AND (b < 30);
	switch (flag)
	start
		case false: print(100);
			break;
		case true: use module mod1 with parameters a, b;
			break;
	end
end
```

**Sample 4.** A `for` loop. The program prints 49, 36, 25, 16, 9, 4 and 1.

```erplag
<<<driver program>>>
start
	declare num, a, k: integer;
	num := 9;
	for (k in 2..8)
	start
		a := (num - k) * (num - k);
		print(a);
	end
end
```

**Sample 5.** Array elements in a `for` loop. `A` has the range 1..10, but `k` starts at -7, so the first assignment stops the program with `RUN TIME ERROR: Array index out of bound`.

```erplag
<<<driver program>>>
start
	declare num, k: integer;
	declare A: array[1..10] of integer;
	num := 5;
	for (k in -7..10)
	start
		A[k] := (num - k) * (num - k);
		print(A[k]);
	end
end
```

**Sample 6.** An array parameter. The program prints 64, 49, 36, 25, 16, 9 and 4, and then their sum, 203.

```erplag
<<module arraysum>>
takes input [list: array[4..10] of integer];
returns [sum: integer];
start
	declare s: integer;
	s := 0;
	declare index: integer;
	for (index in 4..10)
	start
		s := s + list[index];
	end
	sum := s;
end

<<<driver program>>>
start
	declare num, k: integer;
	declare A: array[4..10] of integer;
	num := 12;
	for (k in 4..10)
	start
		A[k] := (num - k) * (num - k);
		print(A[k]);
	end
	[num] := use module arraysum with parameters A;
	print(num);
end
```

## 4. Token list

The lexemes below are tokenized as the token names shown. `AND` and `OR` are written in upper case and all other keywords in lower case; token names are written in upper case.

**Table 1: Keywords**

| Pattern | Token | Pattern | Token | Pattern | Token |
|---|---|---|---|---|---|
| `integer` | INTEGER | `driver` | DRIVER | `returns` | RETURNS |
| `real` | REAL | `program` | PROGRAM | `for` | FOR |
| `boolean` | BOOLEAN | `get_value` | GET_VALUE | `in` | IN |
| `of` | OF | `print` | PRINT | `switch` | SWITCH |
| `array` | ARRAY | `use` | USE | `case` | CASE |
| `start` | START | `with` | WITH | `break` | BREAK |
| `end` | END | `parameters` | PARAMETERS | `default` | DEFAULT |
| `declare` | DECLARE | `takes` | TAKES | `while` | WHILE |
| `module` | MODULE | `input` | INPUT | | |

**Table 2: Regular expressions and tokens**

| Pattern | Token | Pattern | Token | Pattern | Token |
|---|---|---|---|---|---|
| `[a-zA-Z_][a-zA-Z0-9_]*` (at most 20 characters) | ID | `/` | DIV | `:` | COLON |
| `[0-9]+` | NUM | `<` | LT | `..` | RANGEOP |
| `[0-9]+[.][0-9]+` | RNUM | `<=` | LE | `;` | SEMICOL |
| `[0-9]+[.][0-9]+[Ee][+-]?[0-9]+` | RNUM | `>=` | GE | `,` | COMMA |
| `AND` | AND | `>` | GT | `:=` | ASSIGNOP |
| `OR` | OR | `==` | EQ | `[` | SQBO |
| `true` | TRUE | `!=` | NE | `]` | SQBC |
| `false` | FALSE | `<<` | DEF | `(` | BO |
| `+` | PLUS | `>>` | ENDDEF | `)` | BC |
| `-` | MINUS | `<<<` | DRIVERDEF | | |
| `*` | MUL | `>>>` | DRIVERENDDEF | | |

The comment marker `**` (section 1.2) is not a token.

## 5. Grammar

The file [`grammar.txt`](grammar.txt) contains the LL(1) grammar of ERPLAG that the compiler uses; its start symbol is `<program>`. Each line defines one nonterminal in the form `<A> -> α1 | α2 | …`, where each α is a sequence of grammar symbols. A name in angle brackets is a nonterminal, any other name is a token from section 4, and `EPSILON` stands for an empty alternative.
