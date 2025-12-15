%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

int line_num = 1;
int column_num = 1;

// FIX 17
extern int found_prog_end;
extern int found_prog_start;

void update_column(int length);

void yyerror(const char *s);
%}

%option noyywrap

DIGIT       [0-9]
LETTER      [a-zA-Z]
ID          {LETTER}({LETTER}|{DIGIT}|_)*
WHITESPACE  [ \t\r\f]+
NEWLINE     \n
COMMENT     "//"[^\n]*

%%

{COMMENT}   { update_column(yyleng); /* ignore comments */ }

">>>"       { 
                update_column(3); 
                found_prog_start = 1; // ended up not being used, so safe to comment out | update: now used
                return PROG_START; 
            }
"<<<"       {   
                update_column(3);
                found_prog_end = 1;
                return PROG_END;
            }

"int"       { update_column(3); return KW_INT; }
"ch"        { update_column(2); return KW_CH; }
"double"    |
"float"     |
"char"      |
"bool"      |
"string"    |
"void"      |
"long"      |
"short"     { 
              update_column(yyleng); 
              //fprintf(stderr, "Line %d, column %d: Type '%s' not supported; only \"int\" & \"ch\"\n", 
               //       line_num, column_num - yyleng, yytext);
              return ILLEGAL; 
            }
"p"         { update_column(1); return KW_PRINT; }

"="         { update_column(1); return '='; }
"+"         { update_column(1); return '+'; }
"-"         { update_column(1); return '-'; }
"*"         { update_column(1); return '*'; }
"/"         { update_column(1); return '/'; }
"("         { update_column(1); return '('; }
")"         { update_column(1); return ')'; }
","         { update_column(1); return ','; }
":"         { update_column(1); return ':'; }

";"       {  // FIX 9: ; as terminator
              update_column(1);
              return SEMICOLON; 
            }//////


{ID}     { 
              yylval.str_val = strdup(yytext);
              update_column(yyleng);
              return ID;
            }

[_a-zA-Z][_a-zA-Z0-9]*[^a-zA-Z0-9_ \t\r\n\f=+*/()-,:][^ \t\r\n\f]* { // FIX 2: Catch invalid IDs
    fprintf(stderr, "Line %d, column %d: '%s' is an invalid variable name (must consist of _, letters, & numbers, but must start w/ a letter)\n",
            line_num, column_num, yytext);
    update_column(yyleng);
    return ILLEGAL;
}

_[a-zA-Z0-9_]+ { // FIX 2: Catch invalid IDs ; starts with _
    //fprintf(stderr, "Line %d, column %d: Identifiers cannot start with underscore: '%s'\n",
    //        line_num, column_num, yytext);
    update_column(yyleng);
    return ILLEGAL;
}

{DIGIT}+{LETTER}+    {
              //fprintf(stderr, "Line %d: Invalid number '%s' (cannot mix digits and letters)\n", 
                //      line_num, yytext);
              update_column(yyleng);
              return ILLEGAL;
            }

{DIGIT}+    {
              yylval.int_val = atoi(yytext);
              update_column(yyleng);
              return NUM;
            }

\"([^"\\\n]|\\.)*\" {
              // string literal with escape sequences
              char *text = yytext;
              int len = yyleng;
              
              // check for invalid escape sequences first
              for(int i = 1; i < len-1; i++) {
                  if(text[i] == '\\') {
                      i++;
                      if(i >= len-1) {
                          //fprintf(stderr, "Line %d, column %d: Incomplete escape sequence at end of string\n", 
                          //        line_num, column_num + i - 1);
                          update_column(yyleng);
                          return ILLEGAL;
                      }
                      switch(text[i]) {
                          case 'n': case 't': case '"': case '\\':
                              // Valid escapes
                              break;
                          default:
                              //fprintf(stderr, "Line %d, column %d: Invalid escape sequence \\%c\n", 
                              //        line_num, column_num + i - 1, text[i]);
                              update_column(yyleng);
                              return ILLEGAL;
                      }
                  }
              }
              
              // remove quotes for processing
              text[len-1] = '\0';
              text++;
              
              // process escape sequences
              char *result = malloc(len);
              char *dest = result;
              char *src = text;
              
              while(*src) {
                  if(*src == '\\') {
                      src++;
                      switch(*src) {
                          case 'n': *dest++ = '\n'; break;
                          case 't': *dest++ = '\t'; break;
                          case '"': *dest++ = '"'; break;
                          case '\\': *dest++ = '\\'; break;
                          // alr validated above, so no default needed
                      }
                  } else {
                      *dest++ = *src;
                  }
                  src++;
              }
              *dest = '\0';
              
              yylval.str_val = result;
              update_column(yyleng);
              return STR;
            }

{WHITESPACE} { update_column(yyleng); }

{NEWLINE}   { line_num++; column_num = 1; return NEWLINE_TOKEN; }

.           { 
              update_column(1);
              return ILLEGAL;
            }
%%

void update_column(int length) {
    column_num += length;
}
