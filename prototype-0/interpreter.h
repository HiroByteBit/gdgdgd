#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include "output.h"

typedef struct InterpreterState InterpreterState;

char* interpret_program(Node *program);

#endif