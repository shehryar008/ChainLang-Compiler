%{
#include <llvm-c/Core.h>
#include "pLLVM.h"
#include "bison.tab.h"
#include <stdlib.h>
#include <string.h>
%}

DIGIT       [0-9]
ID          [a-zA-Z_][a-zA-Z0-9_]*
FLOAT       {DIGIT}+"."{DIGIT}+|"\."{DIGIT}+|{DIGIT}+"\."
INT         {DIGIT}+

%%

"->"                    { return CHAIN; }
"="                     { return ASSIGN; }
"=="                    { return EQ; }
"!="                    { return NE; }
">="                    { return GE; }
"<="                    { return LE; }
">"                     { return GT; }
"<"                     { return LT; }
"+"                     { return PLUS; }
"-"                     { return MINUS; }
"*"                     { return MUL; }
"/"                     { return DIV; }
"&&"                    { return AND; }
"||"                    { return OR; }
"!"                     { return NOT; }
"if"                    { return IF; }
"then"                  { return THEN; }
"else"                  { return ELSE; }
"done"                  { return DONE; }
"for"                   { return FOR; }
"in"                    { return IN; }
"let"                   { return LET; }
"output"                { return OUTPUT; }
"function"              { return FUNCTION; }
"end"                   { return END; }
"try"                   { return TRY; }
"catch"                 { return CATCH; }
"return"                { return RETURN; }
"while"                 { return WHILE; }
"do"                    { return DO; }
"("                     { return LPAREN; }
")"                     { return RPAREN; }
","                     { return COMMA; }
".."                    { return DOTS; }
{FLOAT}                 { yylval.fval = atof(yytext); return FLOAT; }
{INT}                   { yylval.ival = atoi(yytext); return INT; }
{ID}                    { yylval.sval = strdup(yytext); return ID; }
[ \t\r\n]+              { /* skip whitespace */ }
"//".*                  { /* skip comment */ }
.                       {
                          fprintf(stderr, "Unknown token: %s\n", yytext);
                          return UNKNOWN;
                        }

%%

int yywrap(void) { return 1; }
