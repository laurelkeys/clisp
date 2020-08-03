#include "lval.h"

#include <assert.h>
#include <stdbool.h>

//
// Constructors.
//

lval *lval_num(const long num) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = num;
    return v;
}

lval *lval_err(const char *err) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    // @FIXME use strncpy and pass a max size to strlen
    v->err = malloc(strlen(err) + 1);
    strcpy(v->err, err);
    return v;
}

lval *lval_sym(const char *sym) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    // @FIXME use strncpy and pass a max size to strlen
    v->sym = malloc(strlen(sym) + 1);
    strcpy(v->sym, sym);
    return v;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->cell_count = 0;
    v->cell = NULL;
    return v;
}

lval *lval_qexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->cell_count = 0;
    v->cell = NULL;
    return v;
}

//
// Destructor.
//

void lval_free(lval *v) {
    switch (v->type) {
        case LVAL_NUM: break;

        case LVAL_ERR: free(v->err); break;

        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            while (v->cell_count) lval_free(v->cell[--(v->cell_count)]);
            free(v->cell);
            break;

        default: assert(false);
    }

    free(v);
}

lval *lval_join(lval *x, lval *y) {
    // For each cell in `y` add it to `x`.
    while (y->cell_count) x = lval_add(x, lval_pop(y, 0));

    // Delete the empty `y` and return `x`.
    lval_free(y);
    return x;
}

//
// Helper functions.
//

lval *lval_add(lval *v, lval *x) {
    v->cell_count++;
    v->cell = realloc(v->cell, v->cell_count * sizeof(lval *));
    v->cell[v->cell_count - 1] = x;
    return v;
}

lval *lval_pop(lval *v, const int i) {
    lval *x = v->cell[i];

    // Shift memory after the `i`-th cell item (i.e. `x`),
    // update the cell count, and reallocate the memory used.
    memmove(
        /*dst*/ &v->cell[i],
        /*src*/ &v->cell[i + 1],
        /*count*/ (v->cell_count - (i + 1)) * sizeof(lval *));
    v->cell_count--;
    v->cell = realloc(v->cell, v->cell_count * sizeof(lval *));

    return x;
}

lval *lval_take(lval *v, const int i) {
    lval *x = lval_pop(v, i);
    lval_free(v);
    return x;
}

//
// Built-in functions.
//

lval *lval_builtin(lval *a, const char *func) {
    if (!strcmp("list", func)) return lval_builtin_list(a);
    if (!strcmp("head", func)) return lval_builtin_head(a);
    if (!strcmp("tail", func)) return lval_builtin_tail(a);
    if (!strcmp("join", func)) return lval_builtin_join(a);
    if (!strcmp("eval", func)) return lval_builtin_eval(a);
    if (strstr("+-/*",  func)) return lval_builtin_op(a, func);

    lval_free(a);
    return lval_err("unknown function!");
}

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_free(args); return lval_err(err); }

lval *lval_builtin_op(lval *a, const char *op) {
    // Ensure all arguments are numbers.
    for (int i = 0; i < a->cell_count; ++i) {
        LASSERT(a, a->cell[i]->type == LVAL_NUM, "cannot operate on non-number!");
    }

    // Pop the first element.
    lval *x = lval_pop(a, 0);

    // If `op` == "-" and there are no arguments, perform unary negation.
    if (!strcmp(op, "-") && a->cell_count == 0) x->num = -x->num;

    while (a->cell_count > 0) {
        // Pop the next element.
        lval *y = lval_pop(a, 0);

        if (!strcmp(op, "+")) x->num += y->num;
        if (!strcmp(op, "-")) x->num -= y->num;
        if (!strcmp(op, "*")) x->num *= y->num;
        if (!strcmp(op, "/")) {
            if (y->num == 0) {
                lval_free(x);
                lval_free(y);
                x = lval_err("division by zero!");
                break;
            }
            x->num /= y->num;
        }

        lval_free(y);
    }

    lval_free(a);
    return x;
}

lval *lval_builtin_head(lval *a) {
    LASSERT(a, a->cell_count == 1, "function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "function 'head' passed incorrect type!");
    LASSERT(a, a->cell[0]->cell_count != 0, "function 'head' passed {}!");

    // Take the first argument.
    lval *v = lval_take(a, 0);

    // Delete all elements that are not head and return.
    while (v->cell_count > 1) lval_free(lval_pop(v, 1));
    return v;
}

lval *lval_builtin_tail(lval *a) {
    LASSERT(a, a->cell_count == 1, "function 'tail' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "function 'tail' passed incorrect type!");
    LASSERT(a, a->cell[0]->cell_count != 0, "function 'tail' passed {}!");

    // Take the first argument.
    lval* v = lval_take(a, 0);

    // Delete the first element and return.
    lval_free(lval_pop(v, 0));
    return v;
}

lval *lval_builtin_list(lval *a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval *lval_builtin_eval(lval *a) {
    LASSERT(a, a->cell_count == 1, "function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "function 'eval' passed incorrect type!");

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval *lval_builtin_join(lval *a) {
    for (int i = 0; i < a->cell_count; ++i) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "function 'join' passed incorrect type!");
    }

    lval *x = lval_pop(a, 0);
    while (a->cell_count) x = lval_join(x, lval_pop(a, 0));

    lval_free(a);
    return x;
}

//
// Eval.
//

lval *lval_eval_sexpr(lval *v) {
    // Evaluate children.
    for (int i = 0; i < v->cell_count; ++i)
        v->cell[i] = lval_eval(v->cell[i]);

    // Error checking.
    for (int i = 0; i < v->cell_count; ++i)
        if (v->cell[i]->type == LVAL_ERR) return lval_take(v, i);

    // Empty expression.
    if (v->cell_count == 0) return v;

    // Single expression.
    if (v->cell_count == 1) return lval_take(v, 0);

    // Ensure first element is symbol.
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_free(f);
        lval_free(v);
        return lval_err("S-expression does not start with symbol!");
    }

    // Call built-in with operator.
    lval *result = lval_builtin(v, f->sym);
    lval_free(f);
    return result;
}

lval *lval_eval(lval *v) {
    // Evaluate S-expressions.
    if (v->type == LVAL_SEXPR) return lval_eval_sexpr(v);

    // All other lval types remain the same.
    return v;
}

//
// Read.
//

lval *lval_read_num(const mpc_ast_t *t) {
    errno = 0;
    const long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
        ? lval_num(x)
        : lval_err("invalid number");
}

lval *lval_read(const mpc_ast_t *t) {
    // If Symbol or Number, convert to it and return.
    if (strstr(t->tag, "number")) return lval_read_num(t);
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

    // If root (>) or sexpr, then create an empty list.
    lval *x = NULL;
    if (!strcmp(t->tag, ">"))    x = lval_sexpr();
    if (strstr(t->tag, "sexpr")) x = lval_sexpr();
    if (strstr(t->tag, "qexpr")) x = lval_qexpr();

    // Fill this list with any valid expression contained within.
    for (int i = 0; i < t->children_num; ++i) {
        if (!strcmp(t->children[i]->contents, "(")) continue;
        if (!strcmp(t->children[i]->contents, ")")) continue;
        if (!strcmp(t->children[i]->contents, "{")) continue;
        if (!strcmp(t->children[i]->contents, "}")) continue;
        if (!strcmp(t->children[i]->tag,  "regex")) continue;
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

//
// Print.
//

void lval_expr_print(const lval *v, const char open, const char close) {
    putchar(open);
    const int last_i = v->cell_count - 1;
    for (int i = 0; i <= last_i; ++i) {
        lval_print(v->cell[i]);
        if (i != last_i) putchar(' ');
    }
    putchar(close);
}

void lval_print(const lval *v) {
    switch (v->type) {
        case LVAL_NUM:   printf("%li", v->num);        break;
        case LVAL_ERR:   printf("Error: %s", v->err);  break;
        case LVAL_SYM:   printf("%s", v->sym);         break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
        default: assert(false);
    }
}

void lval_println(const lval *v) {
    lval_print(v); putchar('\n');
}