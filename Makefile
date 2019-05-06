make: shell.l argshell.c 
	flex shell.l
	cc -o argshell argshell.c lex.yy.c -lfl 
clean: 
	rm argshell lex.yy.c 
