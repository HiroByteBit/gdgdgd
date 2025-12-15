#ifndef SEMANTICS_H
#define SEMANTICS_H

#include <stdbool.h>
#include "ast.h"

// symbol table entry
typedef struct Symbol {
    char *name;
    int declared_line;
    bool initialized;
    bool is_error; // added to stop counting all undeclared variable errors
                   // & stop at the first encounter of such error
    bool is_string;  // true for string (ch), false for integer (int)
    struct Symbol *next;
} Symbol;

// semantic analyzer state
typedef struct Semantics {
    Symbol *symbol_table;
    int current_line;
    int error_count;
    bool in_decl_line;  // r we parsing a declaration line?
} Semantics;

// initialize semantic analyzer
void sem_init(Semantics *sem);

// set current line number
void sem_set_line(Semantics *sem, int line);

// set declaration line flag
void sem_set_decl_line(Semantics *sem, bool is_decl_line);

// check if variable is declared before use
bool sem_check_declared(Semantics *sem, const char *name);

// add a new symbol (declaration) - UPDATED with type parameter
bool sem_add_symbol(Semantics *sem, const char *name, bool is_string);

// check for duplicate declaration
bool sem_is_duplicate(Semantics *sem, const char *name);

// get error count
int sem_get_error_count(Semantics *sem);

// print symbol table (for debugging)
void sem_print_symbols(Semantics *sem);

// clean up
void sem_cleanup(Semantics *sem);

// check for division by zero in constant expressions
bool sem_check_division_by_zero(Node *expr_node);

// check if variable is string type
bool sem_is_string_type(Semantics *sem, const char *name);

// check for type mismatch in assignment
bool sem_check_type_compatibility(Semantics *sem, const char *name, bool is_string_assign);

// check if expression is string expression
bool is_string_expression(Node *expr);

// check if expression is constant expression
bool is_constant_expression(Node *expr);

// evaluate constant numeric expression
int eval_constant_expression(Node *expr);

// NEW: Check if expression in print statement is valid (no string vars in arithmetic)
bool sem_check_print_expression(Semantics *sem, Node *expr);

// NEW: Helper function to check if expression contains any string variables
bool sem_has_string_in_expression(Semantics *sem, Node *expr);

#endif