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
    v->err = malloc(strlen(err) + 1);
    strcpy(v->err, err);
    return v;
}

lval *lval_sym(const char *sym) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(sym) + 1);
    strcpy(v->sym, sym);
    return v;
}

lval *lval_fun(lbuiltin fun) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = fun;
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

lenv *lenv_new(void) {
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

//
// Destructor.
//

void lval_free(lval *v) {
    switch (v->type) {
        case LVAL_NUM: break;

        case LVAL_ERR: free(v->err); break;

        case LVAL_SYM: free(v->sym); break;

        case LVAL_FUN: break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            while (v->cell_count) lval_free(v->cell[--(v->cell_count)]);
            free(v->cell);
            break;

        default: assert(false);
    }

    free(v);
}

void lenv_del(lenv *e) {
    for (int i = 0; i < e->count; ++i) {
        free(e->syms[i]);
        lval_free(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
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

lval *lval_join(lval *x, lval *y) {
    // For each cell in `y` add it to `x`.
    while (y->cell_count) x = lval_add(x, lval_pop(y, 0));

    // Delete the empty `y` and return `x`.
    lval_free(y);
    return x;
}

lval *lval_copy(lval *v) {
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        // Copy functions and numbers directly.
        case LVAL_FUN: x->fun = v->fun; break;
        case LVAL_NUM: x->num = v->num; break;

        // Copy strings using malloc and strcpy.
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        // Copy lists by copying each sub-expression.
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->cell_count = v->cell_count;
            x->cell = malloc(x->cell_count * sizeof(lval *));
            for (int i = 0; i < x->cell_count; ++i) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

lval *lenv_get(lenv *e, lval *k) {
    // Iterate over all items in the environment,
    // checking if the stored string matches the symbol string.
    for (int i = 0; i < e->count; ++i) {
        if (!strcmp(e->syms[i], k->sym)) {
            // If it does, return a copy of the value.
            return lval_copy(e->vals[i]);
        }
    }

    return lval_err("unbound symbol");
}

void lenv_put(lenv *e, lval *k, lval *v) {
    // Iterate over all items in the environment
    // to see if the variable already exists.
    for (int i = 0; i < e->count; ++i) {
        if (!strcmp(e->syms[i], k->sym)) {
            // If it is found, delete the item at that position
            // and replace it with a copy of the given variable.
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    // If no existing entry is found, allocate space for a new one.
    e->count++;

    e->syms = realloc(e->syms, e->count * sizeof(char *));
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);

    e->vals = realloc(e->vals, e->count * sizeof(lval *));
    e->vals[e->count - 1] = lval_copy(v);
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
    return lval_err("unknown function");
}

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_free(args); return lval_err(err); }

lval *lval_builtin_op(lval *a, const char *op) {
    // Ensure all arguments are numbers.
    for (int i = 0; i < a->cell_count; ++i) {
        LASSERT(a, a->cell[i]->type == LVAL_NUM, "cannot operate on non-number");
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
                x = lval_err("division by zero");
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
    LASSERT(a, a->cell_count == 1, "function 'head' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "function 'head' passed incorrect type");
    LASSERT(a, a->cell[0]->cell_count != 0, "function 'head' passed {}");

    // Take the first argument.
    lval *v = lval_take(a, 0);

    // Delete all elements that are not head and return.
    while (v->cell_count > 1) lval_free(lval_pop(v, 1));
    return v;
}

lval *lval_builtin_tail(lval *a) {
    LASSERT(a, a->cell_count == 1, "function 'tail' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "function 'tail' passed incorrect type");
    LASSERT(a, a->cell[0]->cell_count != 0, "function 'tail' passed {}");

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
    LASSERT(a, a->cell_count == 1, "function 'eval' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "function 'eval' passed incorrect type");

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval *lval_builtin_join(lval *a) {
    for (int i = 0; i < a->cell_count; ++i) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR, "function 'join' passed incorrect type");
    }

    lval *x = lval_pop(a, 0);
    while (a->cell_count) x = lval_join(x, lval_pop(a, 0));

    lval_free(a);
    return x;
}

//
// Eval.
//

lval *lval_eval_sexpr(lenv *e, lval *v) {
    // Evaluate children.
    for (int i = 0; i < v->cell_count; ++i)
        v->cell[i] = lval_eval(e, v->cell[i]);

    // Error checking.
    for (int i = 0; i < v->cell_count; ++i)
        if (v->cell[i]->type == LVAL_ERR) return lval_take(v, i);

    // Empty expression.
    if (v->cell_count == 0) return v;

    // Single expression.
    if (v->cell_count == 1) return lval_take(v, 0);

    // Ensure the first element is a function after evaluation.
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_free(f);
        lval_free(v);
        return lval_err("first element is not a function");
    }

    // Call built-in function.
    lval *result = f->fun(e, v);
    lval_free(f);
    return result;
}

lval *lval_eval(lenv *e, lval *v) {
    // Evaluate symbols.
    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(e, v);
        lval_free(v);
        return x;
    }

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

void lval_print_expr(const lval *v, const char open, const char close) {
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
        case LVAL_FUN:   printf("<function>");         break;
        case LVAL_SEXPR: lval_print_expr(v, '(', ')'); break;
        case LVAL_QEXPR: lval_print_expr(v, '{', '}'); break;
        default: assert(false);
    }
}

void lval_println(const lval *v) {
    lval_print(v); putchar('\n');
}