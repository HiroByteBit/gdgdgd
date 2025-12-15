#include <stdio.h>         
#include <string.h>
#include <stdint.h>
#include "symbol_table.h"

// symbol table entry
typedef struct {
    char name[MAX_NAME_LEN];
    int reg; // reg assigned (-1 for labels like str0, str1 that have no register)
    uint64_t offset; // memory offset
    int is_string_var; // NEW: 1 if this is a string variable (ch type), 0 otherwise
    char *string_value; // Store string value for string variables
} SymbolEntry;

static SymbolEntry table[MAX_SYMBOLS];
static int symbol_count = 0;
static int next_reg = REG_MIN;
static uint64_t next_offset = 0x0;

// print .data section with .space directives for integers, .asciiz for strings
void PrintDataSection(FILE *out) {
    for(int i = 0; i < symbol_count; i++) {
        if(table[i].reg != -1) {
            // Only generate .space for INTEGER variables, NOT for string variables
            if(!table[i].is_string_var) {
                // Integer variables (int type) get .space
                fprintf(out, "%s: .space 8\n", table[i].name);
            }
            // String variables will be generated separately with .asciiz
        }
    }
}

// initialize/reset symbol table
void SymbolInit() {
    symbol_count = 0;
    next_reg = REG_MIN;
    next_offset = 0x0;
    for(int i = 0; i < MAX_SYMBOLS; i++) {
        table[i].name[0] = '\0';
        table[i].is_string_var = 0;
        table[i].string_value = NULL;
    }
}

// get register assigned to symbol
// returns -1 if symbol is a label (like str0) or not found
int GetRegisterOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return table[i].reg;
        }
    }
    return -1;
}

// check if symbol exists (variable or label)
int SymbolExists(const char *name) {
    return GetRegisterOfTheSymbol(name) != -1 || GetOffsetOfTheSymbol(name) != (uint64_t)-1;
}

// NEW: Check if symbol is a string variable
int IsStringVariable(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return table[i].is_string_var;
        }
    }
    return 0;
}

// NEW: Get string value of a string variable
char *GetStringValueOfSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0 && table[i].is_string_var) {
            return table[i].string_value;
        }
    }
    return NULL;
}

// NEW: Set string value for a string variable
void SetStringValueOfSymbol(const char *name, const char *value) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0 && table[i].is_string_var) {
            // Free old string if exists
            if(table[i].string_value) {
                free(table[i].string_value);
            }
            // Allocate and copy new string
            table[i].string_value = strdup(value);
            break;
        }
    }
}

// allocate a reg for a new symbol (for integer variables)
int AllocateRegisterForTheSymbol(const char *name) {
    // check if alr allocated
    int existing = GetRegisterOfTheSymbol(name);
    if(existing != -1) {
        return existing;
    }
    
    // check limits
    if(symbol_count >= MAX_SYMBOLS) {
        return -1;
    }
    
    // skip r1-r4 (r4 is for syscall args)
    if(next_reg >= 1 && next_reg <= 4)
        next_reg = 5;
    
    if(next_reg > REG_MAX)
        return -1;
    
    // add symbol to table
    strncpy(table[symbol_count].name, name, MAX_NAME_LEN - 1);
    table[symbol_count].name[MAX_NAME_LEN - 1] = '\0';
    table[symbol_count].reg = next_reg;
    table[symbol_count].offset = next_offset;
    table[symbol_count].is_string_var = 0; // Integer variable
    table[symbol_count].string_value = NULL;
    
    symbol_count++;
    next_offset += 8;  // 8 bytes for integer
    
    return next_reg++;
}

// Allocate register for a string variable
int AllocateStringVariable(const char *name) {
    // check if alr allocated
    int existing = GetRegisterOfTheSymbol(name);
    if(existing != -1) {
        return existing;
    }
    
    // check limits
    if(symbol_count >= MAX_SYMBOLS) {
        return -1;
    }
    
    // skip r1-r4 (r4 is for syscall args)
    if(next_reg >= 1 && next_reg <= 4)
        next_reg = 5;
    
    if(next_reg > REG_MAX)
        return -1;
    
    // add symbol to table as string variable
    strncpy(table[symbol_count].name, name, MAX_NAME_LEN - 1);
    table[symbol_count].name[MAX_NAME_LEN - 1] = '\0';
    table[symbol_count].reg = next_reg;
    table[symbol_count].offset = next_offset;
    table[symbol_count].is_string_var = 1; // Mark as string variable
    table[symbol_count].string_value = NULL; // Initialize with no value
    
    symbol_count++;
    next_offset += 64;  // Allocate space for string
    
    return next_reg++;
}

// add label (for standalone strings) w/o register
void AddLabel(const char *name, uint64_t size) {
    // check if alr exists (avoid duplicates)
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return;
        }
    }
    
    if(symbol_count >= MAX_SYMBOLS) {
        return;
    }
    
    strncpy(table[symbol_count].name, name, MAX_NAME_LEN - 1);
    table[symbol_count].name[MAX_NAME_LEN - 1] = '\0';
    table[symbol_count].reg = -1;           // marks this as a label, not a variable
    table[symbol_count].offset = next_offset;
    table[symbol_count].is_string_var = 0;
    table[symbol_count].string_value = NULL;
    
    symbol_count++;
    next_offset += size;  // advance offset by string size (including '\0')
}

// get memory offset for symbol
uint64_t GetOffsetOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return table[i].offset;
        }
    }
    return (uint64_t)-1; // not found
}

// print symbol table for debugging
void PrintAllSymbols(FILE *out) {
    fprintf(out, "; Symbol Table\n");
    fprintf(out, "; Name\t\tReg\tOffset\tType\t\tValue\n");
    fprintf(out, "; ──────────────────────────────────────────────────────────────\n");
    for(int i = 0; i < symbol_count; i++) {
        if(table[i].reg != -1) {
            fprintf(out, "; %s\t\tr%d\t0x%lX\t%s\t",
                    table[i].name,
                    table[i].reg,
                    (unsigned long)table[i].offset,
                    table[i].is_string_var ? "STRING" : "INT");
            if(table[i].is_string_var && table[i].string_value) {
                fprintf(out, "\"%s\"\n", table[i].string_value);
            } else {
                fprintf(out, "\n");
            }
        } else {
            fprintf(out, "; %s\t\t---\t0x%lX\tLABEL\t",
                    table[i].name,
                    (unsigned long)table[i].offset);
            if(table[i].string_value) {
                fprintf(out, "\"%s\"\n", table[i].string_value);
            } else {
                fprintf(out, "\n");
            }
        }
    }
    fprintf(out, "\n");
}

// Clean up allocated memory
void SymbolCleanup() {
    for(int i = 0; i < symbol_count; i++) {
        if(table[i].string_value) {
            free(table[i].string_value);
        }
    }
    SymbolInit();
}