%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "semantics.h"
#include "ast.h"
#include "assembly.h"
#include "machine_code.h"
#include "interpreter.h"

#define NODE_PRINT_PART 7
#define NODE_STR_ASSIGN 8 

// delimiters r necessaryy
int found_prog_end = 0;
int found_prog_start = 0;

// AST root
Node *ast_root = NULL;

// global semantic analyzer
Semantics sem_analyzer;

extern int yylex();
extern int yyparse();
extern FILE *yyin;
void yyerror(const char *s);
int yylex_destroy(void);

Node *create_num_node(int val);
Node *create_str_node(char *str);
Node *create_id_node(char *name);
Node *create_binop_node(int op, Node *left, Node *right);
Node *create_decl_node(Node *items);
Node *create_assign_node(Node *items);
Node *create_print_node(Node *parts);
Node *append_to_list(Node *list, Node *item);
Node *create_print_part_node(Node *content);
Node *create_str_assign_node(Node *id_node, Node *str_node);
void free_node(Node *node);

// AST output functions
void print_ast_to_console(Node *node, int depth);
void save_ast_to_file(Node *node, const char *filename);
void print_ast_to_file(Node *node, FILE *file, int depth);
void save_ast_tree(Node *node, const char *filename);
void print_tree(Node *node, FILE *file, int depth, int is_last, const char *prefix);
%}

%union {
    int int_val;
    char *str_val;
    void *node_ptr;
}

%token PROG_START PROG_END
%token KW_INT KW_PRINT KW_CH 
%token NEWLINE_TOKEN ILLEGAL
%token <int_val> NUM
%token <str_val> ID STR
%token SEMICOLON // ; as terminator

%type <node_ptr> program lines line stmt decl print_stmt assign
%type <node_ptr> print_list print_item expr term factor

%nonassoc PRINT_EXPR

%%

// added optional \ns for balance
// Simplified to avoid reduce/reduce conflicts
program: PROG_START lines PROG_END
    {
        ast_root = $2;
        found_prog_start = 1;
        found_prog_end = 1;
    }
    | PROG_START lines  // missing <<<
    {
        ast_root = $2;
        found_prog_start = 1; 
        found_prog_end = 0; // another >>> issue
    }
    | lines PROG_END  // no >>>
    {
        ast_root = $1;
        found_prog_end = 1;
        found_prog_start = 0; // wasn't found
    }
    | lines  // no delimiters at all
    {
        ast_root = $1;
        found_prog_start = 0;
        found_prog_end = 0;
    }
    ;

// Remove leading_newlines and optional_newlines rules completely
// They cause reduce/reduce conflicts with the lines rule

lines: line lines
    {
        $$ = append_to_list((Node*)$1, (Node*)$2);
    }
    | /* empty */
    {
        $$ = NULL;
    }
    ;

line: stmt NEWLINE_TOKEN
    {
        $$ = $1;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
    }
    | error NEWLINE_TOKEN
    {
        fprintf(stderr, "Line %d: Syntax error caused by any or one of the ff:\n\t"
        "(a) missing or extra ( or )\n\t"
        "(b) unknown operator: PMDAS only\n\t"
        "(c) keyword in the wrong place: e.g.: int 5 or ch \"Dazai Osamu\"\n\t"
        "(d) missing ':' after p in printing\n\t"
        "(e) invalid escape sequence: only \\n, \\t, \", &, \\\\\n\t"
        "(f) invalid variable name: must be in letter(letter + digit + _)* format\n\t"
        "(g) unsupported statement (declaration, assignment, & print only)\n\t"
        "(h) duplicated/incorrect delimiter (>>> for start; <<< for end)\n\t"
        "\t*** code must start w/ >>>\n\t\t*** code must end with >>>\n", 
        sem_analyzer.current_line); // missing ( or ) & other syntax errors
        sem_analyzer.error_count++; ///////
        $$ = NULL;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
        yyerrok;
    }
    | NEWLINE_TOKEN
    {
        $$ = NULL;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
    }
    ;

stmt: decl
    {
        $$ = $1;
        // removed for fix 4 as it should be done in decl rule itslef
    }
    | print_stmt
    {
        $$ = $1;
    }
    | assign
    {
        $$ = $1;
    }
    ;

decl: KW_INT ID
    {
        sem_set_decl_line(&sem_analyzer, true); // to flag redeclaration
        if(sem_add_symbol(&sem_analyzer, $2, false)) {
            Node *id_node = create_id_node($2);
            $$ = create_decl_node(id_node);
        } else {
            $$ = NULL;
        }
    }
    |
    KW_INT ID SEMICOLON  // ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_INT ID '=' expr
    {
        sem_set_decl_line(&sem_analyzer, true);
        if(!sem_check_division_by_zero((Node*)$4)) {
            fprintf(stderr, "Line %d: Division by zero in initialization\n", 
                    sem_analyzer.current_line);
            $$ = NULL;
        } else {
            // only add to symbol table if validation passes
            if(sem_add_symbol(&sem_analyzer, $2, false)) {
                Node *id_node = create_id_node($2);
                Node *assign_node = create_binop_node('=', id_node, (Node*)$4);
                $$ = create_decl_node(assign_node);
            } else {
                $$ = NULL;
            }
        }
    }
    | KW_INT ID '=' expr SEMICOLON  // ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_INT ID '=' expr ',' ID  // multiple vars in 1 declarayion
    {
        fprintf(stderr, "Line %d: Only one declaration per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        $$ = NULL;
    }
    | KW_INT ID '=' STR  // catch: int y = "string"
    {
        fprintf(stderr, "Line %d: Cannot assign string to integer variable '%s'\n",
                sem_analyzer.current_line, $2);
        $$ = NULL;
    }
    | KW_INT ID ',' ID  // multiple vars in 1 declaration
    {
        fprintf(stderr, "Line %d: Only one declaration per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        $$ = NULL;
    }
    | KW_CH ID
    {
        sem_set_decl_line(&sem_analyzer, true); // to flag redeclaration
        if(sem_add_symbol(&sem_analyzer, $2, true)) {
            Node *id_node = create_id_node($2);
            $$ = create_decl_node(id_node);
        } else {
            $$ = NULL;
        }
    }
    | KW_CH ID SEMICOLON  // ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_CH ID '=' STR
    {
        sem_set_decl_line(&sem_analyzer, true); // to flag redeclaration
        if(sem_add_symbol(&sem_analyzer, $2, true)) {
            Node *id_node = create_id_node($2);
            Node *str_node = create_str_node($4);
            Node *str_assign = create_str_assign_node(id_node, str_node);
            $$ = create_decl_node(str_assign);
        } else {
            $$ = NULL;
        }
    }
    | KW_CH ID '=' STR SEMICOLON // ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    // to flag ch x = expr as error
    | KW_CH ID '=' expr
    {
        sem_set_decl_line(&sem_analyzer, true);
        
        // check if the expression is a string FIRST
        if(((Node*)$4)->node_type != 1) { // not a STR node
            fprintf(stderr, "Line %d: Cannot assign numeric expression to string variable '%s'\n",
                    sem_analyzer.current_line, $2);
            $$ = NULL;  // don't add to symbol table
        } else if(!sem_check_division_by_zero((Node*)$4)) {
            fprintf(stderr, "Line %d: Division by zero in initialization\n", 
                    sem_analyzer.current_line);
            $$ = NULL;  // dont add to symbol table
        } else {
            // only add to symbol table if all validations pass
            if(sem_add_symbol(&sem_analyzer, $2, true)) {
                Node *id_node = create_id_node($2);
                Node *str_assign = create_str_assign_node(id_node, (Node*)$4);
                $$ = create_decl_node(str_assign);
            } else {
                $$ = NULL;
            }
        }
    }
    | KW_CH ID ',' ID  // multiple vars in 1 declaration
    {
        fprintf(stderr, "Line %d: Only one declaration per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        $$ = NULL;
    }
    ;  

assign: ID '=' expr
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            if(sem_is_string_type(&sem_analyzer, $1)) {
                fprintf(stderr, "Line %d: Cannot assign integer to string variable '%s'\n",
                        sem_analyzer.current_line, $1);
                $$ = NULL;
            } else if(!sem_check_division_by_zero((Node*)$3)) {
                fprintf(stderr, "Line %d: Division by zero in assignment\n", 
                        sem_analyzer.current_line);
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($1);
                Node *assign_node = create_binop_node('=', id_node, (Node*)$3);
                $$ = create_assign_node(assign_node);
            }
        } else {
            $$ = NULL;
        }
    }
    | ID '=' STR
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            if(!sem_is_string_type(&sem_analyzer, $1)) {
                fprintf(stderr, "Line %d: Cannot assign string to integer variable '%s'\n",
                        sem_analyzer.current_line, $1);
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($1);
                Node *str_node = create_str_node($3);
                Node *str_assign = create_str_assign_node(id_node, str_node);
                $$ = create_assign_node(str_assign);
            }
        } else {
            $$ = NULL;
        }
    }
    | ID '=' expr ',' ID '=' expr  // multiple assignmenmts in one line
    {
        fprintf(stderr, "Line %d: Only one assignment per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        $$ = NULL;
    }
    ;

print_stmt: KW_PRINT ':' print_list
    {
        $$ = create_print_node((Node*)$3);
    }
    ;

print_list: print_item
    {
        Node *wrapped = create_print_part_node($1);
        $$ = wrapped;
    }
    | print_item ',' print_list
    {
        Node *first_wrapped = create_print_part_node($1);
        // Chain print parts using print_part.part_next
        first_wrapped->print_part.part_next = $3;
        $$ = first_wrapped;
    }
    ;

print_item: STR
    {
        $$ = create_str_node($1);
    }
    | expr %prec PRINT_EXPR
    {
        // NEW: Check if expression is valid for print statement
        // (no string variables in arithmetic expressions)
        if(!sem_check_print_expression(&sem_analyzer, (Node*)$1)) {
            fprintf(stderr, "Line %d: Invalid expression in print statement\n",
                    sem_analyzer.current_line);
            $$ = NULL;
        } else {
            $$ = $1;
        }
    }
    ;

expr: expr '+' term
    {
        $$ = create_binop_node('+', (Node*)$1, (Node*)$3);
    }
    | expr '-' term
    {
        $$ = create_binop_node('-', (Node*)$1, (Node*)$3);
    }
    | term
    {
        $$ = $1;
    }
    ;

term: term '*' factor
    {
        $$ = create_binop_node('*', (Node*)$1, (Node*)$3);
    }
    | term '/' factor
    {
        $$ = create_binop_node('/', (Node*)$1, (Node*)$3);
    }
    | factor
    {
        $$ = $1;
    }
    ;

factor: NUM
    {
        $$ = create_num_node($1);
    }
    | ID
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            $$ = create_id_node($1);
        } else {
            $$ = NULL;
        }
    }
    | '(' expr ')'
    {
        $$ = $2;
    } 
    | '-' factor
    {
        Node *neg_one = create_num_node(-1);
        $$ = create_binop_node('*', neg_one, (Node*)$2);
    }
    ;
%%

// no content should be after <<<
int check_content_after_end_delimiter(const char *filename) {
    FILE *f = fopen(filename, "r");
    if(!f)
        return 0;
    
    char line[256];
    int reading_after_end = 0;
    int has_error = 0;
    
    while(fgets(line, sizeof(line), f)) {
        // keep og line (w/ newline) for exact checking
        char original_line[256];
        strcpy(original_line, line);
        
        // find where line ends (b4 newline)
        int line_len = strcspn(original_line, "\r\n");
        original_line[line_len] = '\0';  // terrminate at newline
        
        char *p = original_line;
        // skip leading whitespace for content checking
        while(*p && isspace((unsigned char)*p))
            p++;
        
        // skip empty lines & comments
        if(*p == '\0' || (p[0] == '/' && p[1] == '/'))
            continue;
        
        // check if this line contains <<<
        char *end_pos = strstr(original_line, "<<<");
        if(end_pos != NULL) {
            reading_after_end = 1;  // now we're after <<<
            
            // check for content on same line after <<<
            char *after_end = end_pos + 3; // skip "<<<"
            // check rest of the line
            while(*after_end) {
                if(!isspace((unsigned char)*after_end)) {
                    if(!has_error) {
                        fprintf(stderr, "Extra error: Anything after '<<<' delimiter is not allowed\n");
                        has_error = 1;
                    }
                    break;
                }
                after_end++;
            }
        } 
        // if we're reading after <<< & find non empty, non comment line
        else if(reading_after_end) {
            if(!has_error) {
                fprintf(stderr, "Extra error: Anything after '<<<' delimiter is not allowed\n");
                has_error = 1;
            }
            break;  // found content after <<<, no need to continue
        }
    }
    
    fclose(f);
    return has_error ? 1 : 0;
}

// ============================================================================
// AST TREE OUTPUT FUNCTIONS (ASCII TREE)
// ============================================================================

// Save AST as ASCII tree
void save_ast_tree(Node *node, const char *filename) {
    FILE *file = fopen(filename, "w");
    if(!file) {
        fprintf(stderr, "Error: Cannot create AST file %s\n", filename);
        return;
    }
    
    
    // Print the tree starting from root
    print_tree(node, file, 0, 1, "");
    
    fprintf(file, "\n┌─────────────────────────────────────────────────┐\n");
    fprintf(file, "│                     LEGEND                      │\n");
    fprintf(file, "├─────────────────────────────────────────────────┤\n");
    fprintf(file, "│  • NUM: Number literal                         │\n");
    fprintf(file, "│  • STR: String literal                         │\n");
    fprintf(file, "│  • ID:  Variable identifier                    │\n");
    fprintf(file, "│  • BINOP: Binary operation                     │\n");
    fprintf(file, "│  • DECL: Variable declaration                  │\n");
    fprintf(file, "│  • ASSIGN: Assignment statement                │\n");
    fprintf(file, "│  • PRINT: Print statement                      │\n");
    fprintf(file, "└─────────────────────────────────────────────────┘\n");
    
    fclose(file);
}

// Print tree recursively with ASCII connectors
void print_tree(Node *node, FILE *file, int depth, int is_last, const char *prefix) {
    if(!node) return;
    
    // Print current node with proper prefix
    fprintf(file, "%s", prefix);
    
    // Print tree connectors based on depth
    if(depth > 0) {
        fprintf(file, is_last ? "└── " : "├── ");
    }
    
    // Print node content
    switch(node->node_type) {
        case 0: // NODE_NUM
            fprintf(file, "● NUM: %d\n", node->int_val);
            break;
            
        case 1: // NODE_STR
            fprintf(file, "● STR: \"%s\"\n", node->str_val);
            break;
            
        case 2: // NODE_ID
            fprintf(file, "● ID: %s\n", node->str_val);
            break;
            
        case 3: // NODE_BINOP
            fprintf(file, "● BINOP: '%c'\n", node->binop.op);
            // Create new prefix for children
            char new_prefix[256];
            strcpy(new_prefix, prefix);
            strcat(new_prefix, depth > 0 ? (is_last ? "    " : "│   ") : "");
            
            // Process children (left then right)
            if(node->binop.left) {
                int left_is_last = (node->binop.right == NULL) ? 1 : 0;
                print_tree(node->binop.left, file, depth + 1, left_is_last, new_prefix);
            }
            if(node->binop.right) {
                print_tree(node->binop.right, file, depth + 1, 1, new_prefix);
            }
            break;
            
        case 4: // NODE_DECL
            fprintf(file, "● DECLARATION\n");
            {
                char new_prefix[256];
                strcpy(new_prefix, prefix);
                strcat(new_prefix, depth > 0 ? (is_last ? "    " : "│   ") : "");
                
                Node *current = node->decl_assign.items;
                while(current) {
                    int next_is_last = (current->next == NULL) ? 1 : 0;
                    print_tree(current, file, depth + 1, next_is_last, new_prefix);
                    current = current->next;
                }
            }
            break;
            
        case 5: // NODE_ASSIGN
            fprintf(file, "● ASSIGNMENT\n");
            {
                char new_prefix[256];
                strcpy(new_prefix, prefix);
                strcat(new_prefix, depth > 0 ? (is_last ? "    " : "│   ") : "");
                
                Node *current = node->decl_assign.items;
                while(current) {
                    int next_is_last = (current->next == NULL) ? 1 : 0;
                    print_tree(current, file, depth + 1, next_is_last, new_prefix);
                    current = current->next;
                }
            }
            break;
            
        case 6: // NODE_PRINT
            fprintf(file, "● PRINT STATEMENT\n");
            {
                char new_prefix[256];
                strcpy(new_prefix, prefix);
                strcat(new_prefix, depth > 0 ? (is_last ? "    " : "│   ") : "");
                
                Node *part = node->print_stmt.parts;
                while(part) {
                    int next_is_last = (part->print_part.part_next == NULL) ? 1 : 0;
                    if(part->node_type == NODE_PRINT_PART) {
                        print_tree(part->print_part.items, file, depth + 1, next_is_last, new_prefix);
                    } else {
                        print_tree(part, file, depth + 1, next_is_last, new_prefix);
                    }
                    part = part->print_part.part_next;
                }
            }
            break;
            
        case 7: // NODE_PRINT_PART
            fprintf(file, "● PRINT_PART\n");
            {
                char new_prefix[256];
                strcpy(new_prefix, prefix);
                strcat(new_prefix, depth > 0 ? (is_last ? "    " : "│   ") : "");
                print_tree(node->print_part.items, file, depth + 1, 1, new_prefix);
            }
            break;
            
        case 8: // NODE_STR_ASSIGN
            fprintf(file, "● STRING_ASSIGNMENT\n");
            {
                char new_prefix[256];
                strcpy(new_prefix, prefix);
                strcat(new_prefix, depth > 0 ? (is_last ? "    " : "│   ") : "");
                print_tree(node->str_assign.id, file, depth + 1, node->str_assign.str ? 0 : 1, new_prefix);
                if(node->str_assign.str) {
                    print_tree(node->str_assign.str, file, depth + 1, 1, new_prefix);
                }
            }
            break;
            
        default:
            fprintf(file, "● UNKNOWN NODE TYPE: %d\n", node->node_type);
    }
    
    // Process next statement in program
    if(node->next) {
        print_tree(node->next, file, depth, is_last, prefix);
    }
}

// Print AST to console (human-readable format) - KEEPING FOR REFERENCE
void print_ast_to_console(Node *node, int depth) {
    if(!node) {
        for(int i = 0; i < depth; i++) printf("  ");
        printf("NULL\n");
        return;
    }
    
    for(int i = 0; i < depth; i++) printf("  ");
    
    switch(node->node_type) {
        case 0: // NODE_NUM
            printf("NUM: %d\n", node->int_val);
            break;
            
        case 1: // NODE_STR
            printf("STR: \"%s\"\n", node->str_val);
            break;
            
        case 2: // NODE_ID
            printf("ID: %s\n", node->str_val);
            break;
            
        case 3: // NODE_BINOP
            printf("BINOP: '%c'\n", node->binop.op);
            print_ast_to_console(node->binop.left, depth + 1);
            print_ast_to_console(node->binop.right, depth + 1);
            break;
            
        case 4: // NODE_DECL
            printf("DECLARATION\n");
            {
                Node *current = node->decl_assign.items;
                while(current) {
                    print_ast_to_console(current, depth + 1);
                    current = current->next;
                }
            }
            break;
            
        case 5: // NODE_ASSIGN
            printf("ASSIGNMENT\n");
            {
                Node *current = node->decl_assign.items;
                while(current) {
                    print_ast_to_console(current, depth + 1);
                    current = current->next;
                }
            }
            break;
            
        case 6: // NODE_PRINT
            printf("PRINT STATEMENT\n");
            {
                Node *part = node->print_stmt.parts;
                while(part) {
                    if(part->node_type == NODE_PRINT_PART) {
                        print_ast_to_console(part->print_part.items, depth + 1);
                    } else {
                        print_ast_to_console(part, depth + 1);
                    }
                    part = part->print_part.part_next;
                }
            }
            break;
            
        case 7: // NODE_PRINT_PART
            printf("PRINT_PART\n");
            print_ast_to_console(node->print_part.items, depth + 1);
            break;
            
        case 8: // NODE_STR_ASSIGN
            printf("STRING_ASSIGNMENT\n");
            print_ast_to_console(node->str_assign.id, depth + 1);
            print_ast_to_console(node->str_assign.str, depth + 1);
            break;
            
        default:
            printf("UNKNOWN NODE TYPE: %d\n", node->node_type);
    }
    
    // Process next statement in program
    if(node->next) {
        print_ast_to_console(node->next, depth);
    }
}

// Print AST to file (helper function) - KEEPING FOR REFERENCE
void print_ast_to_file(Node *node, FILE *file, int depth) {
    if(!node || !file) {
        for(int i = 0; i < depth; i++) fprintf(file, "  ");
        fprintf(file, "NULL\n");
        return;
    }
    
    for(int i = 0; i < depth; i++) fprintf(file, "  ");
    
    switch(node->node_type) {
        case 0: // NODE_NUM
            fprintf(file, "NUM: %d\n", node->int_val);
            break;
            
        case 1: // NODE_STR
            fprintf(file, "STR: \"%s\"\n", node->str_val);
            break;
            
        case 2: // NODE_ID
            fprintf(file, "ID: %s\n", node->str_val);
            break;
            
        case 3: // NODE_BINOP
            fprintf(file, "BINOP: '%c'\n", node->binop.op);
            print_ast_to_file(node->binop.left, file, depth + 1);
            print_ast_to_file(node->binop.right, file, depth + 1);
            break;
            
        case 4: // NODE_DECL
            fprintf(file, "DECLARATION\n");
            {
                Node *current = node->decl_assign.items;
                while(current) {
                    print_ast_to_file(current, file, depth + 1);
                    current = current->next;
                }
            }
            break;
            
        case 5: // NODE_ASSIGN
            fprintf(file, "ASSIGNMENT\n");
            {
                Node *current = node->decl_assign.items;
                while(current) {
                    print_ast_to_file(current, file, depth + 1);
                    current = current->next;
                }
            }
            break;
            
        case 6: // NODE_PRINT
            fprintf(file, "PRINT STATEMENT\n");
            {
                Node *part = node->print_stmt.parts;
                while(part) {
                    if(part->node_type == NODE_PRINT_PART) {
                        print_ast_to_file(part->print_part.items, file, depth + 1);
                    } else {
                        print_ast_to_file(part, file, depth + 1);
                    }
                    part = part->print_part.part_next;
                }
            }
            break;
            
        case 7: // NODE_PRINT_PART
            fprintf(file, "PRINT_PART\n");
            print_ast_to_file(node->print_part.items, file, depth + 1);
            break;
            
        case 8: // NODE_STR_ASSIGN
            fprintf(file, "STRING_ASSIGNMENT\n");
            print_ast_to_file(node->str_assign.id, file, depth + 1);
            print_ast_to_file(node->str_assign.str, file, depth + 1);
            break;
            
        default:
            fprintf(file, "UNKNOWN NODE TYPE: %d\n", node->node_type);
    }
    
    // Process next statement in program
    if(node->next) {
        print_ast_to_file(node->next, file, depth);
    }
}

// Save AST to a file - KEEPING FOR REFERENCE
void save_ast_to_file(Node *node, const char *filename) {
    FILE *file = fopen(filename, "w");
    if(!file) {
        fprintf(stderr, "Error: Cannot create AST file %s\n", filename);
        return;
    }
    
    print_ast_to_file(node, file, 0);
    fclose(file);
}

int main(int argc, char **argv) {
    int error_count = 0;

    char *asm_filename = "MIPS64.s";
    char *machine_filename = "MACHINE_CODE.mc";
    
    if(argc >= 3) {
        asm_filename = argv[2];
        // create machine code filename from assembly filename
        char *dot = strrchr(asm_filename, '.');
        if(dot && strcmp(dot, ".s") == 0) {
            // replace .s with .mc
            strcpy(dot, ".mc");
            machine_filename = asm_filename;
            strcpy(dot, ".s"); // restore .s
        } else {
            // append .mc
            machine_filename = malloc(strlen(asm_filename) + 4);
            sprintf(machine_filename, "%s.mc", asm_filename);
        }
    }
    
    // initialize semantic analyzer
    sem_init(&sem_analyzer);
    sem_set_line(&sem_analyzer, 1);
    
    yyin = fopen(argv[1], "r");
    if(!yyin) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        sem_cleanup(&sem_analyzer);
        return 1;
    }
    
    int parse_result = yyparse();
    
    // delimiters r necessaryyy
    if(!found_prog_start) {
        fprintf(stderr, "Delimiter error: Missing program start delimiter '>>>'\n");
        error_count++;
    }
    
    error_count += sem_get_error_count(&sem_analyzer); // fix total error; missing <<< error is overwrittem, that's why

    // delimiters r necessaryyy
    if(!found_prog_end) {
        fprintf(stderr, "Delimiter error: Missing program end delimiter '<<<'\n");
        error_count++;
    }
    
    // Check for content AFTER <<< (LAST)
    int after_error = check_content_after_end_delimiter(argv[1]);
    
    // TOTAL errors
    int total_errors = error_count + after_error;

    if(parse_result == 0 && error_count == 0) {
        // Generate ASCII tree AST (NEW - this is what you want)
        save_ast_tree(ast_root, "AST.txt");
        
        // Also keep the old format if needed
        save_ast_to_file(ast_root, "AST_DUMP.txt");
        
        // open output file for assembly
        FILE *asm_file = fopen(asm_filename, "w");
        if(!asm_file) {
            fprintf(stderr, "Error: Cannot open assembly file %s\n", asm_filename);
            fclose(yyin);
            sem_cleanup(&sem_analyzer);
            free_node(ast_root);
            return 1;
        }
        
        // generate MIPS64 assembly
        GenerateAssemblyProgram(ast_root, asm_file);
        fclose(asm_file);
        
        // now convert assembly to machine code
        if(MachineFromAssembly(asm_filename, machine_filename)) {
            // Machine code generation successful
        }

        // now interpret the program and display output
        if(ast_root == NULL) {
            printf("ast_root is NULL! Cannot interpret.\n");
        } else {
            char *output = interpret_program(ast_root);
            if(output && strlen(output) > 0) {
                printf("%s", output);
            } else {
                printf("(No output produced)\n");
            }
            free(output);
        }
        
    } else {
        printf("\nCompilation failed with %d error(s)\n", total_errors);
    }
    
    fclose(yyin);
    yylex_destroy();
    sem_cleanup(&sem_analyzer);
    free_node(ast_root);
    
    return (parse_result != 0 || error_count > 0) ? 1 : 0;
}
void yyerror(const char *s) {
    //fprintf(stderr, "Syntax error at line %d: %s\n", sem_analyzer.current_line, s);
    //sem_analyzer.error_count++;
}

// AST Creation Functions - UPDATED FOR NEW STRUCTURE
Node *create_num_node(int val) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 0;
    node->next = NULL;
    node->int_val = val;
    return node;
}

Node *create_str_node(char *str) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 1;
    node->next = NULL;
    node->str_val = strdup(str);
    return node;
}

Node *create_id_node(char *name) {
    Node *node = malloc(sizeof(Node));
    if(!node) {
        return NULL;
    }
    node->node_type = 2;
    node->next = NULL;
    node->str_val = strdup(name);
    if(!node->str_val) {
        free(node);
        return NULL;
    }
    return node;
}

Node *create_binop_node(int op, Node *left, Node *right) {
    Node *node = calloc(1, sizeof(Node));
    if(!node) {
        return NULL;
    }
    node->node_type = 3;
    node->next = NULL;
    node->binop.op = op;
    node->binop.left = left;
    node->binop.right = right;
    return node;
}

Node *create_decl_node(Node *items) {
    Node *node = calloc(1, sizeof(Node));
    if(!node) {
        return NULL;
    }
    node->node_type = 4;
    node->next = NULL;
    node->decl_assign.items = items;  // decl_assign.items instead of list.items
    return node;
}

Node *create_assign_node(Node *items) {
    Node *node = calloc(1, sizeof(Node));
    if(!node) {
        return NULL;
    }
    node->node_type = 5;
    node->next = NULL;
    node->decl_assign.items = items;  // decl_assign.items instead of list.items
    return node;
}

Node *create_print_node(Node *parts) {
    Node *node = calloc(1, sizeof(Node));
    if(!node) {
        return NULL;
    }
    node->node_type = 6;
    node->next = NULL;
    node->print_stmt.parts = parts;
    return node;
}

Node *create_print_part_node(Node *content) {
    Node *node = calloc(1, sizeof(Node));
    if(!node) {
        return NULL;
    }
    node->node_type = NODE_PRINT_PART;
    node->next = NULL;
    node->print_part.items = content;  // print_part.items instead of list.items
    node->print_part.part_next = NULL;  // For chaining print parts
    return node;
}

Node *create_str_assign_node(Node *id_node, Node *str_node) {
    Node *node = calloc(1, sizeof(Node));
    if(!node) {
        return NULL;
    }
    node->node_type = NODE_STR_ASSIGN;
    node->next = NULL;
    node->str_assign.id = id_node;
    node->str_assign.str = str_node;
    return node;
}

Node *append_to_list(Node *first, Node *rest) {
    if(!first) {
        return rest;
    }
    if(!rest) {
        return first;
    }
    
    // Chain statements using the common 'next' field
    Node *current = first;
    while(current->next) {  // next instead of list.next
        current = current->next;
    }
    current->next = rest;
    return first;
}

void free_node(Node *node) {
    if(!node)
        return;
    
    switch(node->node_type) {
        case 1: // STR
        case 2: // ID
            free(node->str_val);
            break;
        case 3: // BINOP
            free_node(node->binop.left);
            free_node(node->binop.right);
            break;
        case 4: // DECL
        case 5: // ASSIGN
            free_node(node->decl_assign.items);  // decl_assign.items instead of list.items
            break;
        case 6: // PRINT
            free_node(node->print_stmt.parts);
            break;
        case 7: // PRINT_PART
            free_node(node->print_part.items);  // print_part.items instead of list.items
            free_node(node->print_part.part_next);  // For chaining print parts
            break;
        case 8: // STR_ASSIGN
            free_node(node->str_assign.id);
            free_node(node->str_assign.str);
            break;
    }
    
    // Free the next statement in program list
    free_node(node->next);  // next instead of list.next
    
    free(node);
}