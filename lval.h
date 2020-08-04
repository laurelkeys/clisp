#ifndef __CLISP_LVAL_H__
#define __CLISP_LVAL_H__

#include "mpc.h"

// Forward declarations.
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// Max size for an error message.
#define MAX_ERR_LEN 511

// Valid types for a lval.
typedef enum {
    LVAL_NUM, LVAL_ERR,   LVAL_SYM,
    LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR
} LVAL_TYPE;

// Pointer to a built-in lval function.
typedef lval *(*lbuiltin)(lenv *, lval *);

// A "Lisp value" (lval), which is either "some thing" or an error.
struct lval {
    LVAL_TYPE   type;

    long        num;
    char        *err;
    char        *sym;
    lbuiltin    fun;

    int         cell_count;
    lval        **cell;
};

// A "Lisp environment", which encodes relationships between names and values.
struct lenv {
  int   count;
  char  **syms;
  lval  **vals;
};

//
// Constructors (one per LVAL_TYPE).
//

lval *lval_num(const long num);
lval *lval_err(const char *fmt, ...);
lval *lval_sym(const char *sym);
lval *lval_fun(lbuiltin fun);
lval *lval_sexpr(void);
lval *lval_qexpr(void);

lenv *lenv_new(void);

//
// Destructor.
//

void lval_free(lval *v);

void lenv_free(lenv *e);

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

// Creates a copy of an lval.
lval *lval_copy(lval *v);

// Returns a string representation of the type.
char *lval_type_name(LVAL_TYPE t);

lval *lenv_get(lenv *e, lval *k);
void lenv_put(lenv *e, lval *k, lval *v);

//
// Built-in functions.
//

// Calls the correct built-in function.
lval *lval_builtin(lenv *e, lval *a, const char *fun);

// Adds the built-in functions to the environment `e`.
void lenv_add_builtins(lenv *e);
void lenv_add_builtin(lenv *e, const char *name, lbuiltin fun);

lval *lval_builtin_op(lenv *e, lval *a, const char *op); // + - * /
lval *lval_builtin_add(lenv *e, lval *a);
lval *lval_builtin_sub(lenv *e, lval *a);
lval *lval_builtin_mul(lenv *e, lval *a);
lval *lval_builtin_div(lenv *e, lval *a);

// Takes one or more arguments and returns a new Q-Expression containing the arguments.
lval *lval_builtin_list(lenv *e, lval *a);

lval *lval_builtin_head(lenv *e, lval *a);
lval *lval_builtin_tail(lenv *e, lval *a);

// Takes a Q-Expression and evaluates it as if it were a S-Expression.
lval *lval_builtin_eval(lenv *e, lval *a);

// Takes one or more Q-Expressions and returns a Q-Expression of them conjoined together.
lval *lval_builtin_join(lenv *e, lval *a);

// Adds a user-defined function to the environment `e`.
lval *lval_builtin_def(lenv *e, lval *a);

//
// Eval.
//

lval *lval_eval_sexpr(lenv *e, lval *v);
lval *lval_eval(lenv *e, lval *v);

//
// Read.
//

lval *lval_read_num(const mpc_ast_t *t);
lval *lval_read(const mpc_ast_t *t);

//
// Print.
//

void lval_print_expr(const lval *v, const char open, const char close);
void lval_print(const lval *v);
void lval_println(const lval *v);

#endif // __CLISP_LVAL_H__