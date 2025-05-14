%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pLLVM.h"
extern int yylex();
void yyerror(const char *s) {
    fprintf(stderr,"Parse error: %s\n",s);
    exit(1);
}

extern LLVMBuilderRef builder;
extern LLVMContextRef context;
extern LLVMModuleRef module;
extern StmtList global_program;
%}

%union {
    int       ival;
    float     fval;
    char     *sval;
    Expr*     expr;
    Stmt*     stmt;
    StmtList  slist;
    ParamList plist;
    ExprList  elist;
}

%token <ival>   INT
%token <fval>   FLOAT
%token <sval>   ID
%token          CHAIN LET ASSIGN IF THEN ELSE DONE FOR IN OUTPUT
%token          EQ NE GE LE GT LT
%token          PLUS MINUS MUL DIV
%token          AND OR NOT
%token          LPAREN RPAREN COMMA DOTS
%token          FUNCTION END TRY CATCH UNKNOWN RETURN WHILE DO

%left OR
%left AND
%left EQ NE
%left GT GE LT LE
%left PLUS MINUS
%left MUL DIV
%right NOT

%type  <slist>  program statement_sequence statement_list
%type  <stmt>   statement
%type  <expr>   expression
%type  <plist>  param_list
%type  <elist>  arg_list

%start program
%%

program:
    statement_sequence
    {
        global_program = $1;
        generate_program($1);
        finalize_codegen();
    }
;

statement_sequence:
    statement
    {
        $$ = make_stmt_list(1, (Stmt*[]){ $1 });
    }
  | statement_sequence CHAIN statement
    {
        add_stmt(&$$, $3);
    }
;

statement_list:
    statement
    {
        $$ = make_stmt_list(1, (Stmt*[]){ $1 });
    }
  | statement_list CHAIN statement
    {
        add_stmt(&$$, $3);
    }
;

statement:
    LET ID ASSIGN expression
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_LET;
        s->let.name = $2;
        s->let.expr = $4;
        $$ = s;
    }
  | ID ASSIGN expression
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_ASSIGN;
        s->assign.name = $1;
        s->assign.expr = $3;
        $$ = s;
    }
  | OUTPUT expression
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_OUTPUT;
        s->output.expr = $2;
        $$ = s;
    }
  | IF expression THEN statement_list ELSE statement_list DONE
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_IF;
        s->if_stmt.cond = $2;
        s->if_stmt.then_stmt = $4;
        s->if_stmt.else_stmt = $6;
        $$ = s;
    }
  | FOR ID IN INT DOTS INT statement_list DONE
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_FOR;
        s->for_stmt.var = $2;
        s->for_stmt.start = $4;
        s->for_stmt.end = $6;
        s->for_stmt.body = $7;
        $$ = s;
    }
  | FUNCTION ID LPAREN param_list RPAREN statement_list END
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_FUNC_DECL;
        s->func_decl.name = $2;
        s->func_decl.params = $4;
        s->func_decl.body = $6;
        $$ = s;
    }
  | RETURN expression
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_RETURN;
        s->return_stmt.expr = $2;
        $$ = s;
    }
  | TRY statement_list CATCH statement_list END
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_TRY_CATCH;
        s->try_catch.try_stmt = $2;
        s->try_catch.catch_stmt = $4;
        $$ = s;
    }
  | WHILE expression DO statement_list DONE
    {
        Stmt* s = malloc(sizeof(Stmt));
        s->type = STMT_WHILE;
        s->while_stmt.cond = $2;
        s->while_stmt.body = $4;
        $$ = s;
    }
;

expression:
    INT
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_INT;
        e->ival = $1;
        $$ = e;
    }
  | FLOAT
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_FLOAT;
        e->fval = $1;
        $$ = e;
    }
  | ID
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_VAR;
        e->var_name = $1;
        $$ = e;
    }
  | expression PLUS expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "+";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression MINUS expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "-";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression MUL expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "*";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression DIV expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "/";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression EQ expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "==";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression NE expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "!=";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression LT expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "<";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression GT expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = ">";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression LE expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "<=";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression GE expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = ">=";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression AND expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "&&";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | expression OR expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_BINOP;
        e->binop.op = "||";
        e->binop.left = $1;
        e->binop.right = $3;
        $$ = e;
    }
  | NOT expression
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_UNARYOP;
        e->unaryop.op = "!";
        e->unaryop.operand = $2;
        $$ = e;
    }
  | LPAREN expression RPAREN
    {
        $$ = $2;
    }
  | ID LPAREN arg_list RPAREN
    {
        Expr* e = malloc(sizeof(Expr));
        e->type = EXPR_FUNC_CALL;
        e->func_call.func_name = $1;
        e->func_call.args = $3;
        $$ = e;
    }
;

param_list:
    ID
    {
        ParamList list;
        list.args = malloc(sizeof(char*));
        list.args[0] = $1;
        list.count = 1;
        $$ = list;
    }
  | param_list COMMA ID
    {
        $1.args = realloc($1.args, ($1.count + 1) * sizeof(char*));
        $1.args[$1.count++] = $3;
        $$ = $1;
    }
;

arg_list:
    expression
    {
        ExprList list;
        list.exprs = malloc(sizeof(Expr*));
        list.exprs[0] = $1;
        list.count = 1;
        $$ = list;
    }
  | arg_list COMMA expression
    {
        $1.exprs = realloc($1.exprs, ($1.count + 1) * sizeof(Expr*));
        $1.exprs[$1.count++] = $3;
        $$ = $1;
    }
;

%%

int main() {
    init_codegen();
    return yyparse();
}
