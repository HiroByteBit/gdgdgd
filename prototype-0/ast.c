#include <stdio.h>
#include "ast.h"

void print_ast(Node *node, int depth) {
    for(int i = 0; i < depth; i++)
        printf("  ");
    if(!node) { 
        printf("NULL\n"); 
        return; 
    }
    
    printf("Node type: %d", node->node_type);
    switch(node->node_type) {
        case 0: printf(" (NUM) value: %d\n", node->int_val); break;
        case 1: printf(" (STR) value: %s\n", node->str_val); break;
        case 2: printf(" (ID) name: %s\n", node->str_val); break;
        case 3: printf(" (BINOP) op: %c\n", node->binop.op); 
                print_ast(node->binop.left, depth + 1);
                print_ast(node->binop.right, depth + 1);
                break;
        case 4: printf(" (DECL)\n"); 
                print_ast(node->decl_assign.items, depth + 1);
                print_ast(node->next, depth);
                break;
        case 5: printf(" (ASSIGN)\n");
                print_ast(node->decl_assign.items, depth + 1);
                print_ast(node->next, depth);
                break;
        case 6: printf(" (PRINT)\n");
                {
                    Node *part = node->print_stmt.parts;
                    while(part) {
                        print_ast(part, depth + 1);
                        part = part->print_part.part_next;
                    }
                }
                print_ast(node->next, depth);
                break;
        case 7: printf(" (PRINT_PART)\n");
                print_ast(node->print_part.items, depth + 1);
                print_ast(node->print_part.part_next, depth);
                print_ast(node->next, depth);
                break;
        case 8: printf(" (STR_ASSIGN)\n");
                print_ast(node->str_assign.id, depth + 1);
                print_ast(node->str_assign.str, depth + 1);
                print_ast(node->next, depth);
                break;
        default: printf(" (UNKNOWN)\n");
    }
}