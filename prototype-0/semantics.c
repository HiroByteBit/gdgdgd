#include "semantics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sem_init(Semantics *sem) {
    sem->symbol_table = NULL;
    sem->current_line = 0;
    sem->error_count = 0;
    sem->in_decl_line = false;
}

void sem_set_line(Semantics *sem, int line) {
    sem->current_line = line;
}

void sem_set_decl_line(Semantics *sem, bool is_decl_line) {
    sem->in_decl_line = is_decl_line;
}

bool sem_check_declared(Semantics *sem, const char *name) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            return true;
        }
        s = s->next;
    }
    
    fprintf(stderr, "Line %d: Variable '%s' used before declaration\n", 
            sem->current_line, name);
    sem->error_count++;
    return false;
}

bool sem_add_symbol(Semantics *sem, const char *name, bool is_string) {
    // check for duplicate declaration
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            if(sem->in_decl_line) {
                fprintf(stderr, "Line %d: Variable '%s' already declared\n", 
                        sem->current_line, name);
                sem->error_count++;
                return false;
            }
            return true;
        }
        s = s->next;
    }
    
    // add new symbol
    Symbol *sym = malloc(sizeof(Symbol));
    if(!sym) {
        fprintf(stderr, "Memory allocation error\n");
        return false;
    }
    
    sym->name = strdup(name);
    sym->declared_line = sem->current_line;
    sym->initialized = false;
    sym->is_error = false;
    sym->is_string = is_string;  // store type
    sym->next = sem->symbol_table;
    sem->symbol_table = sym;
    
    return true;
}

bool sem_is_duplicate(Semantics *sem, const char *name) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            return true;
        }
        s = s->next;
    }
    return false;
}

int sem_get_error_count(Semantics *sem) {
    return sem->error_count;
}

void sem_print_symbols(Semantics *sem) {
    printf("\nSymbol Table\n");
    Symbol *s = sem->symbol_table;
    while(s) {
        printf("  %s (declared at line %d, initialized: %s, type: %s)\n",
               s->name, s->declared_line, s->initialized ? "yes" : "no",
               s->is_string ? "string" : "integer");
        s = s->next;
    }
}

void sem_cleanup(Semantics *sem) {
    Symbol *current = sem->symbol_table;
    while(current) {
        Symbol *next = current->next;
        free(current->name);
        free(current);
        current = next;
    }
    sem->symbol_table = NULL;
}

bool sem_check_type(Semantics *sem, const char *type_name) {
    // This function seems incomplete in original code
    // Keeping it as is for compatibility
    return true;
}

// check for division by zero in constant expressions
bool sem_check_division_by_zero(Node *expr_node) {
    if(!expr_node) 
        return true;
    
    switch(expr_node->node_type) {
        case 3: { // NODE_BINOP
            if(expr_node->binop.op == '/') {
                // check right side
                Node *right = expr_node->binop.right;
                if(right->node_type == 0) { // NODE_NUM
                    if(right->int_val == 0) {
                        return false; // division by zero
                    }
                }
            }
            // recursively check both sides
            return sem_check_division_by_zero(expr_node->binop.left) &&
                   sem_check_division_by_zero(expr_node->binop.right);
        }
        default:
            return true;
    }
}

// check variable type
bool sem_is_string_type(Semantics *sem, const char *name) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            return s->is_string;
        }
        s = s->next;
    }
    return false;  // not found
}

// check for type mismatch in assignment
bool sem_check_type_compatibility(Semantics *sem, const char *name, bool is_string_assign) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            if(s->is_string != is_string_assign) {
                fprintf(stderr, "Line %d: Type mismatch for variable '%s'\n",
                        sem->current_line, name);
                sem->error_count++;
                return false;
            }
            return true;
        }
        s = s->next;
    }
    return false;  // var not declared
}

// FIX 8: do not add to symbol table if vars are declared/assigned a value incorrectly
bool is_string_expression(Node *expr) {
    if(!expr)
        return false;
    return expr->node_type == 1; // NODE_STR
}

bool is_constant_expression(Node *expr) {
    if(!expr)
        return false;
    
    switch(expr->node_type) {
        case 0:  // NUM - always constant
            return true;
        case 1:  // STR - always constant
            return true;
        case 3:  // BINOP - check if both children are constant
            return is_constant_expression(expr->binop.left) && 
                   is_constant_expression(expr->binop.right);
        default:
            return false;
    }
}

// evaluate constant numeric expression
int eval_constant_expression(Node *expr) {
    if(!expr)
        return 0;
    
    switch(expr->node_type) {
        case 0:  // NUM
            return expr->int_val;
        case 3:{  // BINOP
            int left = eval_constant_expression(expr->binop.left);
            int right = eval_constant_expression(expr->binop.right);
            switch(expr->binop.op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right != 0 ? left / right : 0;
                default: return 0;
            }
        }
        default:
            return 0;
    }
}

// NEW FUNCTION: Check if expression in print statement is valid
bool sem_check_print_expression(Semantics *sem, Node *expr) {
    if(!expr) return true;
    
    switch(expr->node_type) {
        case 2: { // NODE_ID (variable)
            // For print statements, string variables are allowed when printed directly
            // But we need to check if they're used in arithmetic expressions
            // This check is handled recursively for binary operations
            if(!sem_check_declared(sem, expr->str_val)) {
                return false;
            }
            return true;
        }
        case 3: { // NODE_BINOP (binary operation)
            // Check both sides of binary operation
            if(!sem_check_print_expression(sem, expr->binop.left)) {
                return false;
            }
            if(!sem_check_print_expression(sem, expr->binop.right)) {
                return false;
            }
            
            // Now check if either side is a string variable
            bool left_is_string = false;
            bool right_is_string = false;
            
            // Check left side
            if(expr->binop.left->node_type == 2) { // ID node
                left_is_string = sem_is_string_type(sem, expr->binop.left->str_val);
            } else if(expr->binop.left->node_type == 1) { // STR node
                left_is_string = true;
            } else if(expr->binop.left->node_type == 3) { // Another BINOP
                // Check recursively for string variables in sub-expressions
                left_is_string = sem_has_string_in_expression(sem, expr->binop.left);
            }
            
            // Check right side
            if(expr->binop.right->node_type == 2) { // ID node
                right_is_string = sem_is_string_type(sem, expr->binop.right->str_val);
            } else if(expr->binop.right->node_type == 1) { // STR node
                right_is_string = true;
            } else if(expr->binop.right->node_type == 3) { // Another BINOP
                right_is_string = sem_has_string_in_expression(sem, expr->binop.right);
            }
            
            // If either side is a string, it's an error for arithmetic operations
            if(left_is_string || right_is_string) {
                fprintf(stderr, "Line %d: Cannot use string variables in arithmetic expression in print statement\n",
                        sem->current_line);
                sem->error_count++;
                return false;
            }
            return true;
        }
        case 0: // NODE_NUM (number literal) - always OK
        case 1: // NODE_STR (string literal) - always OK in print
            return true;
        default:
            return true;
    }
}

// NEW: Helper function to check if an expression contains any string variables
bool sem_has_string_in_expression(Semantics *sem, Node *expr) {
    if(!expr) return false;
    
    switch(expr->node_type) {
        case 2: { // NODE_ID (variable)
            return sem_is_string_type(sem, expr->str_val);
        }
        case 1: // NODE_STR (string literal)
            return true;
        case 3: // NODE_BINOP (binary operation)
            return sem_has_string_in_expression(sem, expr->binop.left) ||
                   sem_has_string_in_expression(sem, expr->binop.right);
        default:
            return false;
    }
}