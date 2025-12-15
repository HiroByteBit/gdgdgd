# compiler and flags
CC = gcc
CFLAGS = -g -Wall -Wno-unused-function
LDFLAGS = -lfl

# source files
SRCS = ast.c semantics.c assembly.c symbol_table.c machine_code.c output.c interpreter.c
OBJS = $(SRCS:.c=.o)

# default target
all: compiler

# generate parser
parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

# generate lexer
lex.yy.c: lexer.l parser.tab.h
	flex lexer.l

# compile parser and lexer
parser.tab.o: parser.tab.c
	$(CC) $(CFLAGS) -c parser.tab.c -o parser.tab.o

lex.yy.o: lex.yy.c
	$(CC) $(CFLAGS) -c lex.yy.c -o lex.yy.o

# compile other source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# link everything
compiler: parser.tab.o lex.yy.o $(OBJS)
	$(CC) $(CFLAGS) -o compiler parser.tab.o lex.yy.o $(OBJS) $(LDFLAGS)

# clean
clean:
	rm -f compiler parser.tab.c parser.tab.h lex.yy.c *.o MIPS64.s MACHINE_CODE.mc
	clear

# run
this: compiler
	./compiler source_code.p0
