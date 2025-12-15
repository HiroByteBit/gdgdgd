#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdio.h>
#include <stdint.h>

#define MAX_SYMBOLS 100
#define MAX_NAME_LEN 50
#define REG_MIN 1
#define REG_MAX 31

// Initialize symbol table
void SymbolInit();

// Print data section (only integer variables)
void PrintDataSection(FILE *out);

// Allocate register for symbol (integer variables)
int AllocateRegisterForTheSymbol(const char *name);

// Allocate register for string variable
int AllocateStringVariable(const char *name);

// Check if symbol exists
int SymbolExists(const char *name);

// Get register of symbol
int GetRegisterOfTheSymbol(const char *name);

// Check if symbol is string variable
int IsStringVariable(const char *name);

// Get string value of symbol
char *GetStringValueOfSymbol(const char *name);

// Set string value for symbol
void SetStringValueOfSymbol(const char *name, const char *value);

// Add label (for standalone strings)
void AddLabel(const char *name, uint64_t size);

// Get offset of symbol
uint64_t GetOffsetOfTheSymbol(const char *name);

// Print all symbols (for debugging)
void PrintAllSymbols(FILE *out);

// Clean up memory
void SymbolCleanup();

#endif