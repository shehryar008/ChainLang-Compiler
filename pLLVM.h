// pLLVM.h (updated)

#ifndef PLLVM_H
#define PLLVM_H
#include <llvm-c/Core.h>

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct {
    Stmt** stmts;
    int count;
} StmtList;

typedef struct {
    char     *name;
    LLVMValueRef ptr;
} Symbol;

typedef struct {
    char** args;
    int count;
} ParamList;

typedef struct {
    Expr** exprs;
    int count;
} ExprList;

extern Symbol         symtab[128];
extern int            symcount;
extern LLVMBuilderRef builder;
extern LLVMContextRef context;
extern LLVMModuleRef  module;
extern LLVMValueRef   currentFunction;
extern LLVMValueRef   printfFn;
extern StmtList       global_program;

typedef enum {
    STMT_LET,
    STMT_ASSIGN,
    STMT_OUTPUT,
    STMT_IF,
    STMT_FOR,
    STMT_FUNC_DECL,
    STMT_RETURN,
    STMT_TRY_CATCH,
    STMT_WHILE
} StmtType;

typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_VAR,
    EXPR_BINOP,
    EXPR_UNARYOP,  // Added for unary operations
    EXPR_FUNC_CALL
} ExprType;

struct Expr {
    ExprType type;
    union {
        int ival;
        float fval;
        char* var_name;
        struct {
            char* op;
            Expr* left;
            Expr* right;
        } binop;
        struct {
            char* op;
            Expr* operand;
        } unaryop;  // Added for unary operations
        struct {
            char* func_name;
            ExprList args;
        } func_call;
    };
};

struct Stmt {
    StmtType type;
    union {
        struct { char* name; Expr* expr; } let;
        struct { char* name; Expr* expr; } assign;
        struct { Expr* expr; } output;
        struct { Expr* cond; StmtList then_stmt; StmtList else_stmt; } if_stmt;
        struct {
            char* var;
            int start;
            int end;
            StmtList body;
        } for_stmt;
        struct {
            char* name;
            ParamList params;
            StmtList body;
        } func_decl;
        struct {
            Expr* expr;
        } return_stmt;
        struct { StmtList try_stmt; StmtList catch_stmt; } try_catch;
        struct { Expr* cond; StmtList body; } while_stmt;
    };
};

StmtList make_stmt_list(int cnt, Stmt** arr);
void add_stmt(StmtList* L, Stmt* s);
void init_codegen();
void finalize_codegen();
void generate_program(StmtList program);
void generate_statement(Stmt* s, LLVMBasicBlockRef currentBB, LLVMBasicBlockRef catchBB);
LLVMValueRef generate_expression(Expr* e, LLVMBasicBlockRef catchBB);
LLVMValueRef create_int(int n);
LLVMValueRef create_float(float f);
LLVMValueRef get_variable(const char* name);
void declare_variable(const char* name, LLVMValueRef val);

#endif
