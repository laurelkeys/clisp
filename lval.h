#ifndef __CLISP_LVAL_H__
#define __CLISP_LVAL_H__

#include "mpc.h"

// Valid lval types.
typedef enum {
    LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR
} LVAL_TYPE;

// A "Lisp value" (lval), which is either "some thing" or an error.
typedef struct lval {
    LVAL_TYPE   type;
    long        num;
    char        *err;
    char        *sym;
    int         cell_count;
    struct lval **cell;
} lval;

// Constructors (one per LVAL_TYPE).
lval *lval_num(const long num);
lval *lval_err(const char *err);
lval *lval_sym(const char *sym);
lval *lval_sexpr(void);
lval *lval_qexpr(void);

// Destructor.
void lval_free(lval *v);

// Helper functions.
lval *lval_add(lval *v, lval *x);
lval *lval_pop(lval *v, const int i);
lval *lval_take(lval *v, const int i);

// Eval.
lval *lval_eval_builtin_op(lval *v, const char *op);
lval *lval_eval_sexpr(lval *v);
lval *lval_eval(lval *v);

// Read.
lval *lval_read_num(const mpc_ast_t *t);
lval *lval_read(const mpc_ast_t *t);

// Print.
void lval_expr_print(const lval *v, const char open, const char close);
void lval_print(const lval *v);
void lval_println(const lval *v);

#endif // __CLISP_LVAL_H__