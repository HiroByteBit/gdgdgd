#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "interpreter.h"

#define NODE_PRINT_PART 7

typedef struct Variable {
    char *name;
    union {
        int int_val;
        char *str_val;
    } value;
    bool is_string;
    bool initialized;
} Variable;

struct InterpreterState {
    Variable *vars;
    int var_count;
    int var_capacity;
    OutputCapture *output;
};

static Variable* find_variable(InterpreterState *state, const char *name) {
    for(int i = 0; i < state->var_count; i++) {
        if(strcmp(state->vars[i].name, name) == 0) {
            return &state->vars[i];
        }
    }
    return NULL;
}

static Variable* add_variable(InterpreterState *state, const char *name) {
    if(state->var_count >= state->var_capacity) {
        state->var_capacity = state->var_capacity ? state->var_capacity * 2 : 10;
        state->vars = realloc(state->vars, sizeof(Variable) * state->var_capacity);
    }
    
    Variable *var = &state->vars[state->var_count++];
    var->name = strdup(name);
    var->value.int_val = 0;
    var->initialized = false;
    var->is_string = false;
    return var;
}

static InterpreterState* create_state() {
    InterpreterState *state = malloc(sizeof(InterpreterState));
    state->var_capacity = 10;
    state->var_count = 0;
    state->vars = malloc(sizeof(Variable) * state->var_capacity);
    state->output = malloc(sizeof(OutputCapture));
    capture_init(state->output);
    return state;
}

static void free_state(InterpreterState *state) {
    for(int i = 0; i < state->var_count; i++) {
        free(state->vars[i].name);
        if(state->vars[i].is_string && state->vars[i].initialized) {
            free(state->vars[i].value.str_val);
        }
    }
    free(state->vars);
    if(state->output) {
        capture_free(state->output);
        free(state->output);
    }
    free(state);
}

static int get_int_value(Variable *var) {
    if(!var || !var->initialized)
        return 0;
    if(var->is_string)
        return 0;
    return var->value.int_val;
}

static const char* get_str_value(Variable *var) {
    if(!var || !var->initialized)
        return "";
    if(!var->is_string)
        return "";
    return var->value.str_val ? var->value.str_val : "";
}

static int evaluate_expression(Node *node, InterpreterState *state) {
    if(!node) {
        return 0;
    }
    
    switch(node->node_type) {
        case 0: // NODE_NUM
            return node->int_val;
            
        case 2: // NODE_ID
        {
            Variable *var = find_variable(state, node->str_val);
            if(!var) {
                var = add_variable(state, node->str_val);
            }
            return get_int_value(var);
        }
            
        case 3: // NODE_BINOP
        {
            int left = evaluate_expression(node->binop.left, state);
            int right = evaluate_expression(node->binop.right, state);
            
            switch(node->binop.op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right != 0 ? left / right : 0;
                case '=': return left;
                default: return 0;
            }
        }
            
        default:
            return 0;
    }
}

static void execute_statement(Node *node, InterpreterState *state) {
    if(!node) {
        return;
    }
    
    switch(node->node_type) {
        case 4: // NODE_DECL
        {
            Node *current = node->decl_assign.items;
            
            while(current) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    // declaration w/ initialization
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var) {
                        var = add_variable(state, left->str_val);
                    }
                    
                    int value = evaluate_expression(right, state);
                    var->value.int_val = value;
                    var->initialized = true;
                    var->is_string = false;
                    
                } else if(current->node_type == NODE_STR_ASSIGN) {  // string assignment in declaration
                    // ch var = "string"
                    Node *id_node = current->str_assign.id;
                    Node *str_node = current->str_assign.str;
                    
                    Variable *var = find_variable(state, id_node->str_val);
                    if(!var) {
                        var = add_variable(state, id_node->str_val);
                    }
                    
                    var->value.str_val = strdup(str_node->str_val);
                    var->initialized = true;
                    var->is_string = true;
                    
                } else if(current->node_type == 2) {
                    // declaration without initialization
                    Variable *var = find_variable(state, current->str_val);
                    if(!var) {
                        var = add_variable(state, current->str_val);
                    }
                    var->initialized = false;
                    var->value.int_val = 0;
                }
                current = current->next;
            }
            break;
        }
            
        case 5: // NODE_ASSIGN
        {
            Node *current = node->decl_assign.items;
            
            while(current) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    // integer assignment: x = expr
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var) {
                        var = add_variable(state, left->str_val);
                    }
                    
                    int value = evaluate_expression(right, state);
                    var->value.int_val = value;
                    var->initialized = true;
                    var->is_string = false;
                    
                } else if(current->node_type == NODE_STR_ASSIGN) {  // string assignment
                    // var = "string"
                    Node *id_node = current->str_assign.id;
                    Node *str_node = current->str_assign.str;
                    
                    Variable *var = find_variable(state, id_node->str_val);
                    if(!var) {
                        var = add_variable(state, id_node->str_val);
                    }
                
                    // free old string if it exists
                    if(var->is_string && var->initialized && var->value.str_val) {
                        free(var->value.str_val);
                    }
                    
                    var->value.str_val = strdup(str_node->str_val);
                    var->initialized = true;
                    var->is_string = true;
                }
                current = current->next;
            }
            break;
        }

        case 6: // NODE_PRINT
        {
            Node *current = node->print_stmt.parts;
            
            if(!current) {
                break;
            }

            Node *last_part = NULL;
            Node *temp = current;
            // find the last part in the print line
            while(temp) {
                last_part = temp;
                temp = temp->print_part.part_next;
            }

            while(current) {
                if(current->node_type == NODE_PRINT_PART) {
                    Node *content = current->print_part.items;
                    
                    if(content->node_type == 1) {  // STR literal
                        capture_printf(state->output, "%s", content->str_val);
                    } else if(content->node_type == 2) {  // ID (variable)
                        Variable *var = find_variable(state, content->str_val);
                        if(var && var->initialized) {
                            if(var->is_string) {
                                capture_printf(state->output, "%s", var->value.str_val);
                            } else {
                                capture_printf(state->output, "%d", var->value.int_val);
                            }
                        } else {
                            capture_printf(state->output, "0");
                        }
                    } else {  // expression or NUM
                        int value = evaluate_expression(content, state);
                        capture_printf(state->output, "%d", value);
                    }
                }
                current = current->print_part.part_next;
            }
            
            // newline after print statement
            if(last_part && last_part->node_type == NODE_PRINT_PART) {
                Node *last_content = last_part->print_part.items;
                
                // check if last content is not a string literal & not a string var
                if(last_content->node_type != 1) {  // not a STR literal
                    if(last_content->node_type == 2) {  // ID - check if it's a string var
                        Variable *var = find_variable(state, last_content->str_val);
                        if(!var || !var->is_string) {
                            // not a string variable (or doesn't exist): add newline
                            capture_printf(state->output, "\n");
                        }
                    } else {
                        // expr or other non-string: add \n
                        capture_printf(state->output, "\n");
                    }
                }
            }
            break;
        }
        
        default:
            // Unknown statement type - ignore
            break;
    }
}

char* interpret_program(Node *program) {
    if(!program) {
        return strdup("");
    }
    
    InterpreterState *state = create_state();

    // execute all statements
    Node *current = program;
    while(current) {
        execute_statement(current, state);
        current = current->next;
    }
    
    char *output = capture_get(state->output);
    char *result = strdup(output ? output : "");
    
    free_state(state);
    
    return result;
}