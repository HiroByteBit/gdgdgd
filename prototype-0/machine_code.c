#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "machine_code.h"
#include "symbol_table.h"

// I-type opcodes
#define OP_DADDIU 0x19 // daddiu rt, rs, immediate
#define OP_LD 0x37 // 64-bit load doubleword
#define OP_SD 0x3F // 64-bit store doubleword

// R-type function codes (funct field)
#define FUNCT_DADDU 0x2D
#define FUNCT_DSUBU 0x2F // FIX 13: from 23
#define FUNCT_DMULT 0x18
#define FUNCT_DDIV 0x1A
#define FUNCT_MFHI 0x10
#define FUNCT_MFLO 0x12
#define FUNCT_SYSCALL 0x0C

// map reister name "r0".."r31" to number
// convert reg name string into number
static int RegisterNumber(const char *r) {
    return atoi(r + 1); // skip first character (w/c is 'r', and directly go to the first digit)
}

// R-type instruction: opcode rs rt rd shamt funct
static uint32_t Encode_R_Type(uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt, uint8_t funct) {
    return (0 << 26) | (rs << 21) | (rt << 16) | (rd << 11) | (shamt << 6) | funct;
}

// I-type instruction: opcode rs rt immediate
static uint32_t Encode_I_Type(uint8_t opcode, uint8_t rs, uint8_t rt, int16_t imm) {
    return (opcode << 26) | (rs << 21) | (rt << 16) | ((uint16_t)imm & 0xFFFF);
}

// print 32-bit instruction in binary
static void PrintBinary(uint32_t code, FILE *out) {
    for(int i = 31; i >= 0; i--) {
        fprintf(out, "%c", (code & (1U << i)) ? '1' : '0');
        if(i % 4 == 0)
            fprintf(out, " "); // spacing every 4 bits
    }
}

// MAIN TRANSLATION SECTION
// convert assembly to machine code, one line per assembly
// each instrcution line is converted into a bits of integer code
// and teh resulting binary and hex are written to out_file
int MachineFromAssembly(const char *asm_file, const char *out_file) {
    FILE *in = fopen(asm_file, "r");
    if(!in)
        return 0;

    FILE *out = fopen(out_file, "w");
    if(!out) { 
        fclose(in); 
        return 0; 
    }

    char line[MAX_SYMBOLS];
    while(fgets(line, sizeof(line), in)) {
        line[strcspn(line, "\r\n")] = '\0'; // remove newline
        char *p = line;
        while(*p && isspace(*p)) 
            p++;
        if(*p == '#' || *p == '\0') 
            continue; // skip comments/empty

        ////
        // skip assembler directives and labels
        if(strncmp(p, ".data", 5) == 0 || strncmp(p, ".code",5) == 0)
            continue;
        if(strchr(p, ':'))   // labels like a: .space 8
            continue;
        if(strstr(line, ".asciiz"))
            continue;
        if(strchr(p, ';'))
            continue; // to skip comments b4 the actual assembly (w/c is teh symbol table content 4 deugging)
        ////

        // parsed fields
        char regA[8], regB[MAX_NAME_LEN], regC[8]; // tempoeary string buffers to use when parsing assembly instructions
        // 3 regs since most MIPS64 instruction formats have at most 3 registers
        // regB is MAX_NAME_LEN (64) bc it may hold memory operands like "result(r0)" or variable names, w/c can be long
        // regA and regC are size 8 since the longest reg name is of length 3 (r10 - r31) + \0, and extra padding for safety
        char imm_str[MAX_NAME_LEN];
        int imm;
        uint32_t code = 0;
        int matched = 0; // flag for valid instruction

        // daddiu w/ numeric immediate: daddiu rt, rs, #numeric
        // %7[^,] means read up to 7 characters and stop at the comma
        // #%i reads an int following a #
        // sscanf(...) == 3 means all 3 fields were parsed successfully
        if(sscanf(line, "daddiu %7[^,], %7[^,], #%i", regA, regB, &imm) == 3) {
            int rt = RegisterNumber(regA);
            int rs = RegisterNumber(regB); // convert rt and rs strings to reg numbers
            if(rt >= 0 && rs >= 0) { 
                code = Encode_I_Type(OP_DADDIU, rs, rt, imm); 
                matched = 1; 
            }
        }
//>>>>>>>> ! problematic (machine code)
        // daddiu w/ label: daddiu rt, rs, symbol  (string labels like str0, str1)
        else if(sscanf(p, "daddiu %15[^,], %15[^,], %63s", regA, regB, imm_str) == 3) {
            int rt = RegisterNumber(regA);
            int rs = RegisterNumber(regB);
            if(rt >= 0 && rs >= 0) {
                int offset = GetOffsetOfTheSymbol(imm_str);
                if(offset != -1) {
                    code = Encode_I_Type(OP_DADDIU, rs, rt, (int16_t)offset);
                    matched = 1;
                } else {
                    fprintf(stderr, "Error: %s is not a known symbol\n", imm_str);
                }
            }
        }
        // daddu rd, rs, rt
        else if(sscanf(line, "daddu %7[^,], %7[^,], %7s", regA, regB, regC) == 3) {
            int rd = RegisterNumber(regA);
            int rs = RegisterNumber(regB);
            int rt = RegisterNumber(regC);
            if(rd >= 0 && rs >= 0 && rt >= 0) { 
                code = Encode_R_Type(rs, rt, rd, 0, FUNCT_DADDU); 
                matched = 1; 
            }
        }
        // dsubu
        else if(sscanf(line, "dsubu %7[^,], %7[^,], %7s", regA, regB, regC) == 3) {
            int rd = RegisterNumber(regA);
            int rs = RegisterNumber(regB);
            int rt = RegisterNumber(regC);
            if(rd >= 0 && rs >= 0 && rt >= 0) { 
                code = Encode_R_Type(rs, rt, rd, 0, FUNCT_DSUBU);
                matched = 1;
            }
        }
        // dmult
        else if(sscanf(line, "dmult %7[^,], %7s", regA, regB) == 2) {
            int rs = RegisterNumber(regA);
            int rt = RegisterNumber(regB);
            if(rs >= 0 && rt >= 0) {
                code = Encode_R_Type(rs, rt, 0, 0, FUNCT_DMULT + 4); // + 4 bc 0x1C - 0x18 = 0x04 (this outputs ...18 while in the simlator it is ...1C); same for ddiv
                matched = 1; 
            }
        }
        // ddiv
        else if(sscanf(line, "ddiv %7[^,], %7s", regA, regB) == 2) {
            int rs = RegisterNumber(regA);
            int rt = RegisterNumber(regB);
            if(rs >= 0 && rt >= 0) { 
                code = Encode_R_Type(rs, rt, 0, 0, FUNCT_DDIV + 4);  
                matched = 1; 
            }
        }
        // mflo
        else if(sscanf(line, "mflo %7s", regA) == 1) {
            int rd = RegisterNumber(regA);
            if(rd >= 0) {
                code = Encode_R_Type(0, 0, rd, 0, FUNCT_MFLO);
                matched = 1;
            }
        }
        // mfhi
        else if(sscanf(line, "mfhi %7s", regA) == 1) {
            int rd = RegisterNumber(regA);
            if(rd >= 0 ){ 
                code = Encode_R_Type(0, 0, rd, 0, FUNCT_MFHI); 
                matched = 1; 
            }
        }
        // ld (load doubleword)
        else if(sscanf(line, "ld %7[^,], %7[^)]", regA, regB) == 2) {
            int rt = RegisterNumber(regA);
            int rs = 0;
            int16_t imm = 0;
            char var_name[MAX_NAME_LEN] = {0};
            sscanf(regB, "%63[^ (]", var_name);
            imm = (int16_t)GetOffsetOfTheSymbol(var_name);
            if(rt >= 0) {
                code = Encode_I_Type(OP_LD, rs, rt, imm);
                matched = 1;
            }
        }
        // sd (store doubleword)
        else if(sscanf(line, "sd %7[^,], %7[^)]", regA, regB) == 2) {
            int rt = RegisterNumber(regA);
            int rs = 0;
            int16_t imm = 0;
            char var_name[MAX_NAME_LEN] = {0};
            sscanf(regB, "%63[^ (]", var_name);
            imm = (int16_t)GetOffsetOfTheSymbol(var_name);
            if(rt >= 0) {
                code = Encode_I_Type(OP_SD, rs, rt, imm);
                matched = 1;
            }
        } 

        // syscall with number - like syscall 4
        else if(sscanf(line, "syscall %d", &imm) == 1) {
            code = Encode_R_Type(0, 0, 0, imm, FUNCT_SYSCALL);
            matched = 1;
        }

        // syscall without number - like syscall
        else if(strncmp(p, "syscall", 7) == 0) {
            code = Encode_R_Type(0, 0, 0, 0, FUNCT_SYSCALL);
            matched = 1;
        }
        //////////////

        if(matched) {
            PrintBinary(code, out);
            fprintf(out," : %08X\n", code); // hex representation
        } else {
            fprintf(stderr,"Warning: could not parse line: %s\n", line);
        }
    }

    fclose(in);
    fclose(out);
    return 1;
}
