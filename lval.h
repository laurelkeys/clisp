#ifndef __CLISP_LVAL_H__
#define __CLISP_LVAL_H__

#include <stdbool.h>

#include "mpc.h"

extern mpc_parser_t *Lispy;

// Forward declarations.
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// Max size for an error message.
#define MAX_ERR_LEN 511

// Valid types for a lval.
typedef enum {
    LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_STR,
    LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR
} LVAL_TYPE;

// Pointer to a built-in lval function.
typedef lval *(*lbuiltin)(lenv *, lval *);

// A "Lisp value" (lval), which is either "some thing" or an error.
struct lval {
    LVAL_TYPE   type;

    // Basic.
    long        num;
    char        *err;
    char        *sym;
    char        *str;

    // Function.
    lbuiltin    builtin; // NULL for user-defined functions
    lenv        *env;
    lval        *formals;
    lval        *body;

    // {S,Q}-Expression.
    int         cell_count;
    lval        **cell;
};

// A "Lisp environment", which encodes relationships between names and values.
struct lenv {
  lenv  *parent_ref; // reference to a parent environment
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
lval *lval_str(const char *str);
lval *lval_fun(lbuiltin fun); // built-in function
lval *lval_lambda(lval *formals, lval *body); // user-defined function
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

// Returns a string representation of the type.
char *lval_type_name(LVAL_TYPE t);

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

// Calls a (built-in or user-defined) function `f` with arguments `a`.
// The function is (possibly partially) evaluated on the environment `e`.
lval *lval_call(lenv *e, lval *f, lval *a);

// Indicates whether `x` is "equal to" `y`.
bool lval_equals(lval *x, lval *y);

// Creates a copy of an lval.
lval *lval_copy(lval *v);

// Creates a copy of an lenv.
lenv *lenv_copy(lenv *e);

// Gets the lval mapped by `k`.
lval *lenv_get(lenv *e, lval *k);

// Puts `v`, mapped by `k`, in the local (innermost) environment of `e`.
void lenv_put(lenv *e, lval *k, lval *v);

// Puts `v`, mapped by `k`, in the global (outermost) environment of `e`.
void lenv_def(lenv *e, lval *k, lval *v);

//
// Built-in functions.
//

// Adds the built-in functions to environment `e`.
void lenv_add_builtins(lenv *e);
void lenv_add_builtin(lenv *e, const char *name, lbuiltin fun);

// + - * /
lval *lval_builtin_op(lenv *e, lval *a, const char *op); // (numbers only)
lval *lval_builtin_add(lenv *e, lval *a);
lval *lval_builtin_sub(lenv *e, lval *a);
lval *lval_builtin_mul(lenv *e, lval *a);
lval *lval_builtin_div(lenv *e, lval *a);

// < > <= >=
lval *lval_builtin_ord(lenv *e, lval *a, const char *op); // (numbers only)
lval *lval_builtin_lt(lenv *e, lval *a);
lval *lval_builtin_gt(lenv *e, lval *a);
lval *lval_builtin_le(lenv *e, lval *a);
lval *lval_builtin_ge(lenv *e, lval *a);

// == !=
lval *lval_builtin_cmp(lenv *e, lval *a, const char *op);
lval *lval_builtin_eq(lenv *e, lval *a);
lval *lval_builtin_ne(lenv *e, lval *a);

// Expects three arguments: a condition and two Q-Expressions; and evaluates
// one of the expressions depending on whether or not the condition is true.
lval *lval_builtin_if(lenv *e, lval *a);

// Takes one or more arguments and returns a new Q-Expression containing the arguments.
lval *lval_builtin_list(lenv *e, lval *a);

lval *lval_builtin_head(lenv *e, lval *a);
lval *lval_builtin_tail(lenv *e, lval *a);

// Takes a Q-Expression and evaluates it as if it were a S-Expression.
lval *lval_builtin_eval(lenv *e, lval *a);

// Takes one or more Q-Expressions and returns a Q-Expression of them conjoined together.
lval *lval_builtin_join(lenv *e, lval *a);

// Adds a user-defined variable to the environment `e`.
lval *lval_builtin_var(lenv *e, lval *a, const char *fun);
lval *lval_builtin_def(lenv *e, lval *a); // global assignment
lval *lval_builtin_put(lenv *e, lval *a); // local assignment

// Adds a user-defined function to the environment `e`.
lval *lval_builtin_lambda(lenv *e, lval *a);

// Loads and evaluates a file, given its name in `a->cell[0]->str`.
lval *lval_builtin_load(lenv *e, lval *a);

// Prints the arguments given in `a->cell`, separated by whitespace,
// with a trailing newline. Returns an empty S-Expression.
lval *lval_builtin_print(lenv *e, lval *a);

// Returns an LVAL_ERR with the given error message `a->cell[0]->str`.
lval *lval_builtin_error(lenv *e, lval *a);

//
// Eval.
//

lval *lval_eval_sexpr(lenv *e, lval *v);
lval *lval_eval(lenv *e, lval *v);

//
// Read.
//

lval *lval_read_num(const mpc_ast_t *t);
lval *lval_read_str(const mpc_ast_t *t);
lval *lval_read(const mpc_ast_t *t);

//
// Print.
//

void lval_print_lambda(const lval *v);
void lval_print_expr(const lval *v, const char open, const char close);
void lval_print_str(const lval *v);
void lval_print(const lval *v);
void lval_println(const lval *v);

#endif // __CLISP_LVAL_H__