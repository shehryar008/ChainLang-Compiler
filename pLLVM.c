#include "pLLVM.h"
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/Scalar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Symbol         symtab[128];
int            symcount = 0;
LLVMBuilderRef builder;
LLVMContextRef context;
LLVMModuleRef  module;
LLVMValueRef   currentFunction;
LLVMValueRef   printfFn;
StmtList       global_program;

void init_codegen() {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    context = LLVMContextCreate();
    module = LLVMModuleCreateWithNameInContext("chainlang", context);
    builder = LLVMCreateBuilderInContext(context);
    LLVMTypeRef i8Ptr = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
    LLVMTypeRef printfTy = LLVMFunctionType(LLVMInt32TypeInContext(context), &i8Ptr, 1, 1);
    printfFn = LLVMAddFunction(module, "printf", printfTy);
    LLVMSetLinkage(printfFn, LLVMExternalLinkage);
    LLVMTypeRef mainTy = LLVMFunctionType(LLVMInt32TypeInContext(context), NULL, 0, 0);
    LLVMValueRef mainFn = LLVMAddFunction(module, "main", mainTy);
    currentFunction = mainFn;
    LLVMBasicBlockRef entryBB = LLVMAppendBasicBlockInContext(context, mainFn, "entry");
    LLVMPositionBuilderAtEnd(builder, entryBB);
}

void finalize_codegen() {
    LLVMBasicBlockRef BB = LLVMGetInsertBlock(builder);
    if (!LLVMGetBasicBlockTerminator(BB))
        LLVMBuildRet(builder, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0));
    char *err = NULL;
    if (LLVMVerifyModule(module, LLVMReturnStatusAction, &err)) {
        fprintf(stderr, "Verification failed:\n%s\n", err);
        LLVMDisposeMessage(err);
        exit(1);
    }
    if (LLVMPrintModuleToFile(module, "output.ll", &err) != 0) {
        fprintf(stderr, "Error writing IR:\n%s\n", err);
        LLVMDisposeMessage(err);
        exit(1);
    }
    LLVMDisposeBuilder(builder);
    LLVMContextDispose(context);
}

LLVMValueRef create_int(int n) {
    return LLVMConstInt(LLVMInt32TypeInContext(context), n, 0);
}

LLVMValueRef create_float(float f) {
    return LLVMConstReal(LLVMFloatTypeInContext(context), f);
}

LLVMValueRef get_variable(const char* name) {
    for (int i = 0; i < symcount; i++) {
        if (!strcmp(symtab[i].name, name)) {
            return LLVMBuildLoad2(builder, LLVMGetElementType(LLVMTypeOf(symtab[i].ptr)), symtab[i].ptr, name);
        }
    }
    fprintf(stderr, "Undefined variable: %s\n", name);
    exit(1);
}

void declare_variable(const char* name, LLVMValueRef val) {
    LLVMTypeRef ty = LLVMTypeOf(val);
    LLVMValueRef ptr = LLVMBuildAlloca(builder, ty, name);
    LLVMBuildStore(builder, val, ptr);
    symtab[symcount++] = (Symbol){ strdup(name), ptr };
}

LLVMValueRef generate_expression(Expr* e, LLVMBasicBlockRef catchBB) {
    switch (e->type) {
        case EXPR_INT:
            return create_int(e->ival);
        case EXPR_FLOAT:
            return create_float(e->fval);
        case EXPR_VAR:
            return get_variable(e->var_name);
        case EXPR_BINOP: {
            if (!strcmp(e->binop.op, "&&")) {
                LLVMBasicBlockRef thenBB = LLVMAppendBasicBlock(currentFunction, "and.then");
                LLVMBasicBlockRef elseBB = LLVMAppendBasicBlock(currentFunction, "and.else");
                LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlock(currentFunction, "and.merge");

                // Evaluate left operand and ensure it’s i1
                LLVMValueRef left = generate_expression(e->binop.left, catchBB);
                if (LLVMTypeOf(left) != LLVMInt1TypeInContext(context)) {
                    left = LLVMBuildICmp(builder, LLVMIntNE, left, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0), "tobool");
                }
                LLVMBuildCondBr(builder, left, thenBB, elseBB);

                // Then branch: evaluate right operand and ensure it’s i1
                LLVMPositionBuilderAtEnd(builder, thenBB);
                LLVMValueRef right = generate_expression(e->binop.right, catchBB);
                if (LLVMTypeOf(right) != LLVMInt1TypeInContext(context)) {
                    right = LLVMBuildICmp(builder, LLVMIntNE, right, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0), "tobool");
                }
                LLVMBuildBr(builder, mergeBB);

                // Else branch: constant false (i1)
                LLVMPositionBuilderAtEnd(builder, elseBB);
                LLVMValueRef falseVal = LLVMConstInt(LLVMInt1TypeInContext(context), 0, 0);
                LLVMBuildBr(builder, mergeBB);

                // Merge branch: create PHI node with i1 type
                LLVMPositionBuilderAtEnd(builder, mergeBB);
                LLVMValueRef phi = LLVMBuildPhi(builder, LLVMInt1TypeInContext(context), "and.result");
                LLVMAddIncoming(phi, &right, &thenBB, 1);
                LLVMAddIncoming(phi, &falseVal, &elseBB, 1);
                return phi;
            } else if (!strcmp(e->binop.op, "||")) {
                LLVMBasicBlockRef thenBB = LLVMAppendBasicBlock(currentFunction, "or.then");
                LLVMBasicBlockRef elseBB = LLVMAppendBasicBlock(currentFunction, "or.else");
                LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlock(currentFunction, "or.merge");

                // Evaluate left operand and ensure it’s i1
                LLVMValueRef left = generate_expression(e->binop.left, catchBB);
                if (LLVMTypeOf(left) != LLVMInt1TypeInContext(context)) {
                    left = LLVMBuildICmp(builder, LLVMIntNE, left, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0), "tobool");
                }
                LLVMBuildCondBr(builder, left, thenBB, elseBB);

                // Then branch: constant true (i1)
                LLVMPositionBuilderAtEnd(builder, thenBB);
                LLVMValueRef trueVal = LLVMConstInt(LLVMInt1TypeInContext(context), 1, 0);
                LLVMBuildBr(builder, mergeBB);

                // Else branch: evaluate right operand and ensure it’s i1
                LLVMPositionBuilderAtEnd(builder, elseBB);
                LLVMValueRef right = generate_expression(e->binop.right, catchBB);
                if (LLVMTypeOf(right) != LLVMInt1TypeInContext(context)) {
                    right = LLVMBuildICmp(builder, LLVMIntNE, right, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0), "tobool");
                }
                LLVMBuildBr(builder, mergeBB);

                // Merge branch: create PHI node with i1 type
                LLVMPositionBuilderAtEnd(builder, mergeBB);
                LLVMValueRef phi = LLVMBuildPhi(builder, LLVMInt1TypeInContext(context), "or.result");
                LLVMAddIncoming(phi, &trueVal, &thenBB, 1);
                LLVMAddIncoming(phi, &right, &elseBB, 1);
                return phi;
            } else {
                LLVMValueRef left = generate_expression(e->binop.left, catchBB);
                LLVMValueRef right = generate_expression(e->binop.right, catchBB);
                LLVMTypeRef leftType = LLVMTypeOf(left);

                if (!strcmp(e->binop.op, "+")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildAdd(builder, left, right, "addtmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFAdd(builder, left, right, "faddtmp");
                } else if (!strcmp(e->binop.op, "-")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildSub(builder, left, right, "subtmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFSub(builder, left, right, "fsubtmp");
                } else if (!strcmp(e->binop.op, "*")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildMul(builder, left, right, "multmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFMul(builder, left, right, "fmultmp");
                } else if (!strcmp(e->binop.op, "/")) {
                    if (leftType == LLVMInt32TypeInContext(context)) {
                        if (catchBB != NULL) {
                            LLVMBasicBlockRef currentBB = LLVMGetInsertBlock(builder);
                            LLVMBasicBlockRef divBB = LLVMAppendBasicBlock(currentFunction, "div");
                            LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0);
                            LLVMValueRef isZero = LLVMBuildICmp(builder, LLVMIntEQ, right, zero, "isZero");
                            LLVMBuildCondBr(builder, isZero, catchBB, divBB);
                            LLVMPositionBuilderAtEnd(builder, divBB);
                            return LLVMBuildSDiv(builder, left, right, "divtmp");
                        } else {
                            return LLVMBuildSDiv(builder, left, right, "divtmp");
                        }
                    } else if (leftType == LLVMFloatTypeInContext(context)) {
                        return LLVMBuildFDiv(builder, left, right, "fdivtmp");
                    }
                } else if (!strcmp(e->binop.op, "==")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildICmp(builder, LLVMIntEQ, left, right, "eqtmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFCmp(builder, LLVMRealOEQ, left, right, "feqtmp");
                } else if (!strcmp(e->binop.op, "!=")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildICmp(builder, LLVMIntNE, left, right, "netmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFCmp(builder, LLVMRealONE, left, right, "fnetmp");
                } else if (!strcmp(e->binop.op, "<")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildICmp(builder, LLVMIntSLT, left, right, "lttmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFCmp(builder, LLVMRealOLT, left, right, "flttmp");
                } else if (!strcmp(e->binop.op, ">")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildICmp(builder, LLVMIntSGT, left, right, "gttmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFCmp(builder, LLVMRealOGT, left, right, "fgttmp");
                } else if (!strcmp(e->binop.op, "<=")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildICmp(builder, LLVMIntSLE, left, right, "letmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFCmp(builder, LLVMRealOLE, left, right, "fletmp");
                } else if (!strcmp(e->binop.op, ">=")) {
                    if (leftType == LLVMInt32TypeInContext(context))
                        return LLVMBuildICmp(builder, LLVMIntSGE, left, right, "getmp");
                    else if (leftType == LLVMFloatTypeInContext(context))
                        return LLVMBuildFCmp(builder, LLVMRealOGE, left, right, "fgetmp");
                }
                fprintf(stderr, "Unsupported binary operator or type\n");
                exit(1);
            }
        }
        case EXPR_UNARYOP: {
            if (!strcmp(e->unaryop.op, "!")) {
                LLVMValueRef operand = generate_expression(e->unaryop.operand, catchBB);
                // Ensure operand is i1 before applying NOT
                if (LLVMTypeOf(operand) != LLVMInt1TypeInContext(context)) {
                    operand = LLVMBuildICmp(builder, LLVMIntNE, operand, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0), "tobool");
                }
                return LLVMBuildNot(builder, operand, "not");
            } else {
                fprintf(stderr, "Unsupported unary operator: %s\n", e->unaryop.op);
                exit(1);
            }
        }
        case EXPR_FUNC_CALL: {
            LLVMValueRef func = LLVMGetNamedFunction(module, e->func_call.func_name);
            if (!func) {
                fprintf(stderr, "Undefined function: %s\n", e->func_call.func_name);
                exit(1);
            }
            ExprList args = e->func_call.args;
            LLVMValueRef* arg_vals = malloc(args.count * sizeof(LLVMValueRef));
            for (int i = 0; i < args.count; i++)
                arg_vals[i] = generate_expression(args.exprs[i], catchBB);
            LLVMValueRef call = LLVMBuildCall2(builder,
                                               LLVMGetElementType(LLVMTypeOf(func)),
                                               func,
                                               arg_vals,
                                               args.count,
                                               "calltmp");
            free(arg_vals);
            return call;
        }
    }
    return NULL;
}

void generate_statement(Stmt* s, LLVMBasicBlockRef currentBB, LLVMBasicBlockRef catchBB) {
    LLVMPositionBuilderAtEnd(builder, currentBB);
    switch (s->type) {
        case STMT_LET: {
            LLVMValueRef val = generate_expression(s->let.expr, catchBB);
            declare_variable(s->let.name, val);
            break;
        }
        case STMT_ASSIGN: {
            LLVMValueRef val = generate_expression(s->assign.expr, catchBB);
            LLVMValueRef ptr = NULL;
            for (int i = 0; i < symcount; i++) {
                if (!strcmp(symtab[i].name, s->assign.name)) {
                    ptr = symtab[i].ptr;
                    break;
                }
            }
            if (!ptr) {
                fprintf(stderr, "Undefined variable: %s\n", s->assign.name);
                exit(1);
            }
            LLVMBuildStore(builder, val, ptr);
            break;
        }
        case STMT_OUTPUT: {
            LLVMValueRef val = generate_expression(s->output.expr, catchBB);
            LLVMTypeRef i8ptr = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
            LLVMTypeRef printfTy = LLVMFunctionType(LLVMInt32TypeInContext(context), &i8ptr, 1, 1);
            LLVMValueRef fmt;
            if (LLVMTypeOf(val) == LLVMFloatTypeInContext(context))
                fmt = LLVMBuildGlobalStringPtr(builder, "%f\n", "fmt");
            else
                fmt = LLVMBuildGlobalStringPtr(builder, "%d\n", "fmt");
            LLVMBuildCall2(builder, printfTy, printfFn, (LLVMValueRef[]){fmt, val}, 2, "");
            break;
        }
        case STMT_IF: {
            LLVMValueRef cond = generate_expression(s->if_stmt.cond, catchBB);
            // Ensure cond is i1
            if (LLVMTypeOf(cond) != LLVMInt1TypeInContext(context)) {
                cond = LLVMBuildICmp(builder, LLVMIntNE, cond, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0), "tobool");
            }
            LLVMBasicBlockRef thenBB = LLVMAppendBasicBlock(currentFunction, "if.then");
            LLVMBasicBlockRef elseBB = LLVMAppendBasicBlock(currentFunction, "if.else");
            LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlock(currentFunction, "if.merge");
            LLVMBuildCondBr(builder, cond, thenBB, elseBB);

            LLVMPositionBuilderAtEnd(builder, thenBB);
            for (int i = 0; i < s->if_stmt.then_stmt.count; i++) {
                generate_statement(s->if_stmt.then_stmt.stmts[i], thenBB, catchBB);
                thenBB = LLVMGetInsertBlock(builder);
            }
            LLVMBuildBr(builder, mergeBB);

            LLVMPositionBuilderAtEnd(builder, elseBB);
            for (int i = 0; i < s->if_stmt.else_stmt.count; i++) {
                generate_statement(s->if_stmt.else_stmt.stmts[i], elseBB, catchBB);
                elseBB = LLVMGetInsertBlock(builder);
            }
            LLVMBuildBr(builder, mergeBB);

            LLVMPositionBuilderAtEnd(builder, mergeBB);
            break;
        }
        case STMT_FOR: {
            const char* var = s->for_stmt.var;
            int start = s->for_stmt.start;
            int end = s->for_stmt.end;
            StmtList body = s->for_stmt.body;
            LLVMTypeRef i32 = LLVMInt32TypeInContext(context);

            LLVMValueRef startV = LLVMConstInt(i32, start, 0);
            LLVMValueRef endV = LLVMConstInt(i32, end, 0);
            LLVMValueRef counter = LLVMBuildAlloca(builder, i32, var);
            LLVMBuildStore(builder, startV, counter);
            symtab[symcount++] = (Symbol){ strdup(var), counter };

            LLVMBasicBlockRef condBB = LLVMAppendBasicBlock(currentFunction, "for.cond");
            LLVMBasicBlockRef bodyBB = LLVMAppendBasicBlock(currentFunction, "for.body");
            LLVMBasicBlockRef incBB = LLVMAppendBasicBlock(currentFunction, "for.inc");
            LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(currentFunction, "for.end");

            LLVMBuildBr(builder, condBB);
            LLVMPositionBuilderAtEnd(builder, condBB);
            LLVMValueRef cur = LLVMBuildLoad2(builder, i32, counter, "current");
            LLVMValueRef cmp = LLVMBuildICmp(builder, LLVMIntSLE, cur, endV, "for.cond");
            LLVMBuildCondBr(builder, cmp, bodyBB, endBB);

            LLVMPositionBuilderAtEnd(builder, bodyBB);
            for (int i = 0; i < body.count; i++) {
                generate_statement(body.stmts[i], bodyBB, catchBB);
                bodyBB = LLVMGetInsertBlock(builder);
            }
            LLVMBuildBr(builder, incBB);

            LLVMPositionBuilderAtEnd(builder, incBB);
            LLVMValueRef loadedCounter = LLVMBuildLoad2(builder, i32, counter, "current.inc");
            LLVMValueRef next = LLVMBuildAdd(builder, loadedCounter, LLVMConstInt(i32, 1, 0), "for.inc");
            LLVMBuildStore(builder, next, counter);
            LLVMBuildBr(builder, condBB);

            LLVMPositionBuilderAtEnd(builder, endBB);
            symcount--;
            break;
        }
        case STMT_FUNC_DECL: {
            int param_count = s->func_decl.params.count;
            LLVMTypeRef* param_types = malloc(param_count * sizeof(LLVMTypeRef));
            for (int i = 0; i < param_count; i++)
                param_types[i] = LLVMInt32TypeInContext(context);

            LLVMTypeRef ret_type = LLVMInt32TypeInContext(context);
            LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types, param_count, 0);
            LLVMValueRef func = LLVMAddFunction(module, s->func_decl.name, func_type);
            LLVMSetLinkage(func, LLVMExternalLinkage);

            LLVMBasicBlockRef entryBB = LLVMAppendBasicBlockInContext(context, func, "entry");
            LLVMBasicBlockRef oldBB = LLVMGetInsertBlock(builder);
            LLVMPositionBuilderAtEnd(builder, entryBB);

            int old_symcount = symcount;
            for (int i = 0; i < param_count; i++) {
                char* param_name = s->func_decl.params.args[i];
                LLVMValueRef param_val = LLVMGetParam(func, i);
                LLVMValueRef alloc = LLVMBuildAlloca(builder, LLVMInt32TypeInContext(context), param_name);
                LLVMBuildStore(builder, param_val, alloc);
                symtab[symcount++] = (Symbol){ strdup(param_name), alloc };
            }

            StmtList body = s->func_decl.body;
            for (int i = 0; i < body.count; i++) {
                generate_statement(body.stmts[i], entryBB, NULL);
                entryBB = LLVMGetInsertBlock(builder);
            }

            if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(builder)))
                LLVMBuildRet(builder, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0));

            symcount = old_symcount;
            LLVMPositionBuilderAtEnd(builder, oldBB);
            free(param_types);
            break;
        }
        case STMT_RETURN: {
            LLVMValueRef val = generate_expression(s->return_stmt.expr, catchBB);
            LLVMBuildRet(builder, val);
            break;
        }
        case STMT_TRY_CATCH: {
            LLVMBasicBlockRef tryBB = LLVMAppendBasicBlock(currentFunction, "try");
            LLVMBasicBlockRef catchBBLocal = LLVMAppendBasicBlock(currentFunction, "catch");
            LLVMBasicBlockRef afterBB = LLVMAppendBasicBlock(currentFunction, "after");

            LLVMBuildBr(builder, tryBB);
            LLVMPositionBuilderAtEnd(builder, tryBB);
            for (int i = 0; i < s->try_catch.try_stmt.count; i++) {
                generate_statement(s->try_catch.try_stmt.stmts[i], tryBB, catchBBLocal);
                tryBB = LLVMGetInsertBlock(builder);
                if (LLVMGetBasicBlockTerminator(tryBB)) break;
            }
            if (!LLVMGetBasicBlockTerminator(tryBB))
                LLVMBuildBr(builder, afterBB);

            LLVMPositionBuilderAtEnd(builder, catchBBLocal);
            for (int i = 0; i < s->try_catch.catch_stmt.count; i++) {
                generate_statement(s->try_catch.catch_stmt.stmts[i], catchBBLocal, NULL);
                catchBBLocal = LLVMGetInsertBlock(builder);
                if (LLVMGetBasicBlockTerminator(catchBBLocal)) break;
            }
            if (!LLVMGetBasicBlockTerminator(catchBBLocal))
                LLVMBuildBr(builder, afterBB);

            LLVMPositionBuilderAtEnd(builder, afterBB);
            break;
        }
        case STMT_WHILE: {
            LLVMBasicBlockRef condBB = LLVMAppendBasicBlock(currentFunction, "while.cond");
            LLVMBasicBlockRef bodyBB = LLVMAppendBasicBlock(currentFunction, "while.body");
            LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(currentFunction, "while.end");

            LLVMBuildBr(builder, condBB);
            LLVMPositionBuilderAtEnd(builder, condBB);
            LLVMValueRef cond = generate_expression(s->while_stmt.cond, catchBB);
            // Ensure cond is i1
            if (LLVMTypeOf(cond) != LLVMInt1TypeInContext(context)) {
                cond = LLVMBuildICmp(builder, LLVMIntNE, cond, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0), "tobool");
            }
            LLVMBuildCondBr(builder, cond, bodyBB, endBB);

            LLVMPositionBuilderAtEnd(builder, bodyBB);
            StmtList body = s->while_stmt.body;
            for (int i = 0; i < body.count; i++) {
                generate_statement(body.stmts[i], bodyBB, catchBB);
                bodyBB = LLVMGetInsertBlock(builder);
            }
            LLVMBuildBr(builder, condBB);

            LLVMPositionBuilderAtEnd(builder, endBB);
            break;
        }
    }
}

void generate_program(StmtList program) {
    LLVMBasicBlockRef currentBB = LLVMGetInsertBlock(builder);
    for (int i = 0; i < program.count; i++) {
        generate_statement(program.stmts[i], currentBB, NULL);
        currentBB = LLVMGetInsertBlock(builder);
    }
}

StmtList make_stmt_list(int cnt, Stmt** arr) {
    StmtList list;
    list.stmts = malloc(cnt * sizeof(Stmt*));
    memcpy(list.stmts, arr, cnt * sizeof(Stmt*));
    list.count = cnt;
    return list;
}

void add_stmt(StmtList* L, Stmt* s) {
    LLVMBasicBlockRef currentBB = LLVMGetInsertBlock(builder);
    L->stmts = realloc(L->stmts, (L->count + 1) * sizeof(Stmt*));
    L->stmts[L->count++] = s;
}
