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

//
// Constructors (one per LVAL_TYPE).
//

lval *lval_num(const long num);
lval *lval_err(const char *err);
lval *lval_sym(const char *sym);
lval *lval_sexpr(void);
lval *lval_qexpr(void);

//
// Destructor.
//

void lval_free(lval *v);

//
// Helper functions.
//

// Adds an element (`x`) to a {S,Q}-Expression (`v`).
lval *lval_add(lval *v, lval *x);

// Extracts a single element, at index `i`, from a {S,Q}-Expression,
// then shifts the rest of the list backward and returns the element.
lval *lval_pop(lval *v, const int i);

// Behaves like `lval_pop`, but the {S,Q}-Expression `v` is deleted.
lval *lval_take(lval *v, const int i);

// Repeatedly pops items from `y` and adds them to `x`, until `y` is empty.
// Then deletes `y` and returns `x`.
lval *lval_join(lval *x, lval *y);

//
// Built-in functions.
//

// Calls the correct built-in function.
lval *lval_builtin(lval *a, const char *func);

lval *lval_builtin_op(lval *a, const char *op); // + - * /

lval *lval_builtin_head(lval *a);
lval *lval_builtin_tail(lval *a);

// Takes one or more arguments and returns a new Q-Expression containing the arguments.
lval *lval_builtin_list(lval *a);

// Takes a Q-Expression and evaluates it as if it were a S-Expression.
lval *lval_builtin_eval(lval *a);

// Takes one or more Q-Expressions and returns a Q-Expression of them conjoined together.
lval *lval_builtin_join(lval *a);

//
// Eval.
//

lval *lval_eval_sexpr(lval *v);
lval *lval_eval(lval *v);

//
// Read.
//

lval *lval_read_num(const mpc_ast_t *t);
lval *lval_read(const mpc_ast_t *t);

//
// Print.
//

void lval_expr_print(const lval *v, const char open, const char close);
void lval_print(const lval *v);
void lval_println(const lval *v);

#endif // __CLISP_LVAL_H__