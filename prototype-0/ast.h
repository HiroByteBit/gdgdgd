#ifndef AST_H
#define AST_H

#define NODE_PRINT_PART 7  
#define NODE_STR_ASSIGN 8

// AST node structure - FIXED VERSION
typedef struct Node {
    int node_type;
    struct Node *next;  // COMMON field for ALL nodes to chain statements
    
    union {
        int int_val;
        char *str_val;
        struct {
            struct Node *left;
            struct Node *right;
            int op;
        } binop;
        struct {
            struct Node *items;
        } decl_assign;  // For DECL and ASSIGN nodes
        struct {
            struct Node *parts;
        } print_stmt;
        struct {
            struct Node *id;
            struct Node *str;
        } str_assign;
        struct {
            struct Node *items;
            struct Node *part_next;  // For chaining PRINT_PART nodes
        } print_part;
    };
} Node;

void print_ast(Node *node, int depth);

#endif