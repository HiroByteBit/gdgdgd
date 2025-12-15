#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "assembly.h"
#include "symbol_table.h"
#include "ast.h"

// string table for storing string literals
typedef struct {
    char *label;
    char *value;
} StringEntry;

static StringEntry string_table[100];
static int string_count = 0;
static int string_label_counter = 0;

// Track string variables separately
typedef struct {
    char *name;
    char *value;  // For initialized strings
    int is_initialized;
} StringVariable;

static StringVariable string_vars[100];
static int string_var_count = 0;

// track w/c vars have been initialized
static char *initialized_vars[100];
static int init_var_count = 0;

// r4 for syscall arguments
// r10-r19 for temporary calculations
static int temp_start = 10;
static int temp_next = 10;
static int temp_max = 19;

// get or create label for a string literal
static char* GetStringLabel(const char *str, int is_variable_decl) {
    // process escape sequences
    char *processed_str = malloc(strlen(str) * 2 + 1);
    char *dst = processed_str;
    
    for(const char *src = str; *src; src++) {
        if(*src == '\\' && *(src+1)) {
            src++;
            switch(*src) {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                default:
                    *dst++ = '\\';
                    *dst++ = *src;
                    break;
            }
        } else {
            *dst++ = *src;
        }
    }
    *dst = '\0';
    
    // For string variable declarations (ch x = "value")
    if(is_variable_decl) {
        free(processed_str);
        return NULL;
    }
    
    // For string literals in print statements
    for(int i = 0; i < string_count; i++) {
        if(strcmp(string_table[i].value, processed_str) == 0) {
            free(processed_str);
            return string_table[i].label;
        }
    }
    
    // create new string entry
    if(string_count >= 100) {
        free(processed_str);
        return NULL;
    }
    
    string_table[string_count].value = processed_str;
    char *label = malloc(20);
    sprintf(label, "str%d", string_label_counter++);
    string_table[string_count].label = label;
    
    string_count++;
    return label;
}

// Add or update string variable
static void AddStringVariable(const char *name, const char *value, int is_initialized) {
    for(int i = 0; i < string_var_count; i++) {
        if(strcmp(string_vars[i].name, name) == 0) {
            // Update existing
            free(string_vars[i].value);
            string_vars[i].value = strdup(value);
            string_vars[i].is_initialized = is_initialized;
            return;
        }
    }
    
    if(string_var_count >= 100) return;
    
    string_vars[string_var_count].name = strdup(name);
    string_vars[string_var_count].value = strdup(value);
    string_vars[string_var_count].is_initialized = is_initialized;
    string_var_count++;
}

// Get string variable value
static char* GetStringVariableValue(const char *name) {
    for(int i = 0; i < string_var_count; i++) {
        if(strcmp(string_vars[i].name, name) == 0) {
            return string_vars[i].value;
        }
    }
    return NULL;
}

// mark variable as initialized
static void mark_initialized(const char *name) {
    for(int i = 0; i < init_var_count; i++) {
        if(strcmp(initialized_vars[i], name) == 0)
            return;
    }
    if(init_var_count < 100) {
        initialized_vars[init_var_count++] = strdup(name);
    }
}

// initialize assembly generator
void AssemblyInit() {
    temp_next = temp_start;
    init_var_count = 0;
    string_var_count = 0;
}

// allocate a temporary reg (r10-r19)
static int NewTempRegister() {
    int r = temp_next++;
    if(temp_next > temp_max)
        temp_next = temp_start;
    return r;
}

// reset temporary reg allocation
static void ResetTempRegister() {
    temp_next = temp_start;
}

// load immediate value into register
static void GenerateLoadImmediate(FILE *out, int reg, long long imm) {
    fprintf(out, "daddiu r%d, r0, #%lld\n", reg, imm);
}

// collect symbols and strings from AST
static void CollectSymbolsFromAST(Node *node) {
    if(!node)
        return;
    
    Node *current = node;
    while(current) {
        switch(current->node_type) {
            case 1: // NODE_STR - string literal
                GetStringLabel(current->str_val, 0);
                break;
                
            case 4: { // NODE_DECL - declaration
                Node *item = current->decl_assign.items;
                while(item) {
                    if(item->node_type == 2) {
                        // simple declaration: int x or ch x
                        // We'll determine type during code generation
                        // For now, allocate as integer (will be updated if string)
                        AllocateRegisterForTheSymbol(item->str_val);
                    } else if(item->node_type == 3 && item->binop.op == '=') {
                        // initialized declaration: int x = expr
                        if(item->binop.left && item->binop.left->node_type == 2) {
                            AllocateRegisterForTheSymbol(item->binop.left->str_val);
                        }
                        CollectSymbolsFromAST(item->binop.right);
                    } else if(item->node_type == 8) {  // NODE_STR_ASSIGN - ch x = "string"
                        Node *id_node = item->str_assign.id;
                        Node *str_node = item->str_assign.str;
                        
                        if(id_node && id_node->node_type == 2 && 
                           str_node && str_node->node_type == 1) {
                            // This is a string variable declaration: ch name = "value"
                            // Allocate as string variable
                            AllocateStringVariable(id_node->str_val);
                            mark_initialized(id_node->str_val);
                            
                            // Store the string value
                            AddStringVariable(id_node->str_val, str_node->str_val, 1);
                        }
                    }
                    item = item->next;
                }
                break;
            }
                
            case 5: { // NODE_ASSIGN - assignment
                Node *assign = current->decl_assign.items;
                while(assign) {
                    if(assign->node_type == 3 && assign->binop.op == '=') {
                        // integer assignment: x = expr
                        if(assign->binop.left && assign->binop.left->node_type == 2) {
                            // Make sure variable exists
                            if(GetRegisterOfTheSymbol(assign->binop.left->str_val) == -1) {
                                AllocateRegisterForTheSymbol(assign->binop.left->str_val);
                            }
                        }
                        CollectSymbolsFromAST(assign->binop.right);
                    } else if(assign->node_type == 8) {  // string assignment
                        Node *id_node = assign->str_assign.id;
                        Node *str_node = assign->str_assign.str;
                        
                        if(id_node && id_node->node_type == 2 && 
                           str_node && str_node->node_type == 1) {
                            // string assignment: name = "new value"
                            // Update string variable
                            AddStringVariable(id_node->str_val, str_node->str_val, 1);
                        }
                    }
                    assign = assign->next;
                }
                break;
            }
                
            case 6: { // NODE_PRINT - print statement
                Node *part = current->print_stmt.parts;
                while(part) {
                    if(part->node_type == 7) {  // NODE_PRINT_PART
                        Node *content = part->print_part.items;
                        if(content && content->node_type == 1) {
                            GetStringLabel(content->str_val, 0);
                        } else {
                            CollectSymbolsFromAST(content);
                        }
                    } else if(part->node_type == 1) {
                        GetStringLabel(part->str_val, 0);
                    } else {
                        CollectSymbolsFromAST(part);
                    }
                    part = part->print_part.part_next;
                }
                break;
            }
                
            case 3: // NODE_BINOP - expression
                CollectSymbolsFromAST(current->binop.left);
                CollectSymbolsFromAST(current->binop.right);
                break;
                
            case 2: // NODE_ID - variable reference
                // Ensure variable exists
                if(GetRegisterOfTheSymbol(current->str_val) == -1) {
                    // Check if it's a string variable by looking at context
                    // For now, allocate as integer
                    AllocateRegisterForTheSymbol(current->str_val);
                }
                break;
                
            case 7: // NODE_PRINT_PART
                CollectSymbolsFromAST(current->print_part.items);
                if(current->print_part.part_next) {
                    CollectSymbolsFromAST(current->print_part.part_next);
                }
                break;
                
            case 8: // NODE_STR_ASSIGN
                if(current->str_assign.id && current->str_assign.id->node_type == 2) {
                    // Make sure string variable exists
                    if(GetRegisterOfTheSymbol(current->str_assign.id->str_val) == -1) {
                        AllocateStringVariable(current->str_assign.id->str_val);
                    }
                }
                if(current->str_assign.str && current->str_assign.str->node_type == 1) {
                    GetStringLabel(current->str_assign.str->str_val, 0);
                }
                break;
        }
        
        current = current->next;
    }
}

// generate code for an expression
static int GenerateExpression(Node *node, FILE *out, int target_reg) {
    if(!node)
        return 0;
    
    // handle NODE_PRINT_PART wrapper
    if(node->node_type == 7) {
        return GenerateExpression(node->print_part.items, out, target_reg);
    }

    switch(node->node_type) {
        case 0: { // NODE_NUM - number literal
            int reg = target_reg ? target_reg : NewTempRegister();
            GenerateLoadImmediate(out, reg, node->int_val);
            return reg;
        }
            
        case 2: { // NODE_ID - variable reference
            if(target_reg) {
                // load directly into target register
                fprintf(out, "ld r%d, %s(r0)\n", target_reg, node->str_val);
                return target_reg;
            } else {
                // load into temporary register
                int reg = NewTempRegister();
                fprintf(out, "ld r%d, %s(r0)\n", reg, node->str_val);
                return reg;
            }
        }
            
        case 3: { // NODE_BINOP - binary operation
            if(target_reg) {
                int left_reg = GenerateExpression(node->binop.left, out, 0);
                int right_reg = GenerateExpression(node->binop.right, out, 0);
                
                switch(node->binop.op) {
                    case '+':
                        fprintf(out, "daddu r%d, r%d, r%d\n", target_reg, left_reg, right_reg);
                        break;
                    case '-':
                        fprintf(out, "dsubu r%d, r%d, r%d\n", target_reg, left_reg, right_reg);
                        break;
                    case '*':
                        fprintf(out, "dmult r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", target_reg);
                        break;
                    case '/':
                        fprintf(out, "ddiv r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", target_reg);
                        break;
                }
                
                return target_reg;
            } else {
                int left_reg = GenerateExpression(node->binop.left, out, 0);
                int right_reg = GenerateExpression(node->binop.right, out, 0);
                int result_reg = NewTempRegister();
                
                switch(node->binop.op) {
                    case '+':
                        fprintf(out, "daddu r%d, r%d, r%d\n", result_reg, left_reg, right_reg);
                        break;
                    case '-':
                        fprintf(out, "dsubu r%d, r%d, r%d\n", result_reg, left_reg, right_reg);
                        break;
                    case '*':
                        fprintf(out, "dmult r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", result_reg);
                        break;
                    case '/':
                        fprintf(out, "ddiv r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", result_reg);
                        break;
                }
                
                return result_reg;
            }
        }
    }
    
    return 0;
}

static void GenerateDeclaration(Node *node, FILE *out) {
    if(!node || node->node_type != 4)
        return;
    
    Node *current = node->decl_assign.items;
    while(current) {
        if(current->node_type == 3 && current->binop.op == '=') {
            // integer declaration with initialization: int x = expr
            Node *left = current->binop.left;
            Node *right = current->binop.right;
            
            // allocate symbol (if not already)
            if(GetRegisterOfTheSymbol(left->str_val) == -1) {
                AllocateRegisterForTheSymbol(left->str_val);
            }
            mark_initialized(left->str_val);
            
            // evaluate expression into r4
            GenerateExpression(right, out, 4);
            
            // store from r4 to memory
            fprintf(out, "sd r4, %s(r0)\n", left->str_val);
            
        } else if(current->node_type == 8) {
            // string declaration: ch x = "string"
            Node *id_node = current->str_assign.id;
            Node *str_node = current->str_assign.str;
            
            if(id_node && id_node->node_type == 2 && 
               str_node && str_node->node_type == 1) {
                // Mark as initialized
                mark_initialized(id_node->str_val);
            }
            
        } else if(current->node_type == 2) {
            // simple declaration without initialization
            // Just allocate space, value remains uninitialized
            if(GetRegisterOfTheSymbol(current->str_val) == -1) {
                AllocateRegisterForTheSymbol(current->str_val);
            }
        }
        current = current->next;
    }
}

static void GenerateAssignment(Node *node, FILE *out) {
    if(!node || node->node_type != 5)
        return;
    
    Node *current = node->decl_assign.items;
    while(current) {
        if(current->node_type == 3 && current->binop.op == '=') {
            // integer assignment: x = expr
            Node *left = current->binop.left;
            Node *right = current->binop.right;
            
            // evaluate expression into r4
            GenerateExpression(right, out, 4);
            
            // store from r4 to memory
            fprintf(out, "sd r4, %s(r0)\n", left->str_val);
            mark_initialized(left->str_val);
            
        } else if(current->node_type == 8) {
            // string assignment: x = "new string"
            Node *id_node = current->str_assign.id;
            Node *str_node = current->str_assign.str;
            
            if(id_node && id_node->node_type == 2 && 
               str_node && str_node->node_type == 1) {
                // For string assignment, we update the string value
                mark_initialized(id_node->str_val);
            }
        }
        current = current->next;
    }
}

// generate code for print statement
static void GeneratePrint(Node *node, FILE *out) {
    if(!node || node->node_type != 6)
        return;
    
    Node *current = node->print_stmt.parts;
    
    while(current) {
        Node *content = current;
        if(current->node_type == 7) {
            content = current->print_part.items;
        }
        
        if(content && content->node_type == 1) {  // string literal
            char *label = GetStringLabel(content->str_val, 0);
            if(label) {
                fprintf(out, "daddiu r4, r0, %s\n", label);
                fprintf(out, "syscall 5\n");
            }
        } else if(content && content->node_type == 2) {  // variable
            // Check if it's a string variable
            if(IsStringVariable(content->str_val)) {
                // String variable - load its address directly
                fprintf(out, "daddiu r4, r0, %s\n", content->str_val);
                fprintf(out, "syscall 5\n");
            } else {
                // Integer variable
                fprintf(out, "ld r4, %s(r0)\n", content->str_val);
                fprintf(out, "syscall 5\n");
            }
        } else if(content) {  // expression
            GenerateExpression(content, out, 4);
            fprintf(out, "syscall 5\n");
        }
        current = current->print_part.part_next;
    }
}

// generate code for a single AST node
void GenerateAssemblyNode(Node *node, FILE *out) {
    if(!node || !out)
        return;
    
    ResetTempRegister();
    
    switch(node->node_type) {
        case 4: // NODE_DECL
            GenerateDeclaration(node, out);
            break;
        case 5: // NODE_ASSIGN
            GenerateAssignment(node, out);
            break;
        case 6: // NODE_PRINT
            GeneratePrint(node, out);
            break;
        default:
            // For other nodes, just continue to next statement
            break;
    }
}

// Print string literals section
static void PrintStringLiteralsSection(FILE *out) {
    for(int i = 0; i < string_count; i++) {
        fprintf(out, "%s: .asciiz \"", string_table[i].label);
        for(char *p = string_table[i].value; *p; p++) {
            if(*p == '\n') fprintf(out, "\\n");
            else if(*p == '"') fprintf(out, "\\\"");
            else if(*p == '\\') fprintf(out, "\\\\");
            else fputc(*p, out);
        }
        fprintf(out, "\"\n");
    }
}

// Print string variables section (without _str suffix)
static void PrintStringVariablesSection(FILE *out) {
    for(int i = 0; i < string_var_count; i++) {
        if(string_vars[i].is_initialized) {
            fprintf(out, "%s: .asciiz \"", string_vars[i].name);
            for(char *p = string_vars[i].value; *p; p++) {
                if(*p == '\n') fprintf(out, "\\n");
                else if(*p == '"') fprintf(out, "\\\"");
                else if(*p == '\\') fprintf(out, "\\\\");
                else fputc(*p, out);
            }
            fprintf(out, "\"\n");
        }
    }
}

// generate complete assembly program
void GenerateAssemblyProgram(Node *program, FILE *out) {
    if(!program || !out)
        return;
    
    // initialize
    SymbolInit();
    AssemblyInit();
    string_count = 0;
    string_label_counter = 0;
    string_var_count = 0;
    
    // collect all symbols and strings
    CollectSymbolsFromAST(program);
    
    // register string labels (str0, str1, ...) in the symbol table
    for(int i = 0; i < string_count; i++) {
        AddLabel(string_table[i].label, strlen(string_table[i].value) + 1);
    }
    
    // debug: print symbol table
    // PrintAllSymbols(out);
    
    // generate .data section
    fprintf(out, ".data\n");
    
    // Generate integer variables (from PrintDataSection)
    PrintDataSection(out);
    
    // Generate string literals (str0, str1, ...)
    PrintStringLiteralsSection(out);
    
    // Generate string variables WITHOUT _str suffix
    PrintStringVariablesSection(out);
    
    fprintf(out, "\n.code\n");
    
    // generate code
    Node *current = program;
    while(current) {
        GenerateAssemblyNode(current, out);
        current = current->next;
    }
    
    // exit program
    // fprintf(out, "syscall 10\n");
    
    // cleanup
    for(int i = 0; i < string_count; i++) {
        free(string_table[i].value);
        free(string_table[i].label);
    }
    
    for(int i = 0; i < string_var_count; i++) {
        free(string_vars[i].name);
        free(string_vars[i].value);
    }
    
    for(int i = 0; i < init_var_count; i++) {
        free(initialized_vars[i]);
    }
}
