STAGE1 = src/lexer.c src/grammar.c src/parser.c
STAGE2 = src/ast.c src/symbolTable.c src/typechecker.c src/semanticAnalyzer.c src/codegen.c

all: compiler stage1exe

compiler: src/driver.c $(STAGE1) $(STAGE2) src/*.h
	gcc -o compiler src/driver.c $(STAGE1) $(STAGE2)

stage1exe: src/stage1driver.c $(STAGE1) src/*.h
	gcc -o stage1exe src/stage1driver.c $(STAGE1)

clean:
	rm -f compiler stage1exe
