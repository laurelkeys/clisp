#include "lval.h"

#include <assert.h>
#include <stdarg.h>

//
// Constructors.
//

lval *lval_num(const long num) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = num;
    return v;
}

lval *lval_err(const char *fmt, ...) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);
    v->err = malloc(MAX_ERR_LEN + 1);
    vsnprintf(v->err, MAX_ERR_LEN, fmt, va);

    // Reallocate the message to the number of bytes actually used.
    v->err = realloc(v->err, strlen(v->err) + 1);

    va_end(va);
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
    v->builtin = fun;
    return v;
}

lval *lval_lambda(lval *formals, lval *body) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = NULL;
    v->env = lenv_new();
    v->formals = formals;
    v->body = body;
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
    e->parent_ref = NULL;
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

        case LVAL_FUN:
            if (!v->builtin) {
                lenv_free(v->env);
                lval_free(v->formals);
                lval_free(v->body);
            }
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            while (v->cell_count)
                lval_free(v->cell[--(v->cell_count)]);

            free(v->cell);
            break;

        default: assert(false);
    }

    free(v);
}

void lenv_free(lenv *e) {
    for (int i = 0; i < e->count; ++i) {
        free(e->syms[i]);
        lval_free(e->vals[i]);
    }

    // Free allocated memory for lists.
    free(e->syms);
    free(e->vals);

    free(e);
}

//
// Helper functions.
//

char *lval_type_name(LVAL_TYPE t) {
    switch (t) {
        case LVAL_NUM:   return "Number";
        case LVAL_ERR:   return "Error";
        case LVAL_SYM:   return "Symbol";
        case LVAL_FUN:   return "Function";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default:         return "Unknown";
    }
}

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
        /*dst*/&v->cell[i],
        /*src*/&v->cell[i + 1],
        /*count*/(v->cell_count - (i + 1)) * sizeof(lval *)
    );

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
#if false
    for (int i = 0; i < y->cell_count; ++i)
        x = lval_add(x, y->cell[i]);

    lval_free(y->cell);
    lval_free(y);
#else
    // For each cell in `y` add it to `x`.
    while (y->cell_count) x = lval_add(x, lval_pop(y, 0));

    // Delete the empty `y` and return `x`.
    lval_free(y);
#endif

    return x;
}

#define VARIABLE_ARGUMENTS true
// Parse the symbol '&' as a way to create user-defined functions that can take in
// a variable number of arguments, e.g.: a (lambda) function with formal arguments
// `{x & xs}` will take in a single argument `x`, followed by zero or more other
// arguments, joined together into a list called `xs`.

lval *lval_call(lenv *e, lval *f, lval *a) {
    // If `f` is a built-in, simply call it.
    if (f->builtin) return f->builtin(e, a);

    // Otherwise, assign each argument in order. Note that,
    // if given < total, the function is partially applied.
    const int given = a->cell_count;
    const int total = f->formals->cell_count;

    while (a->cell_count) {
        if (f->formals->cell_count == 0) {
            lval_free(a);
            return lval_err(
                "function passed too many arguments. "
                "Got %i, expected %i.", given, total
            );
        }

        lval *sym = lval_pop(f->formals, 0);
#if VARIABLE_ARGUMENTS
        // Special case to handle '&'.
        if (!strcmp(sym->sym, "&")) {
            // Ensure '&' is followed by another symbol.
            if (f->formals->cell_count != 1) {
                lval_free(a);
                return lval_err(
                    "function format invalid. "
                    "Symbol '&' not followed by single symbol."
                );
            }

            // Next formal should be bound to the remaining arguments.
            lval *nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, lval_builtin_list(e, a));

            lval_free(sym);
            lval_free(nsym);
            break;
        }
#endif
        lval *val = lval_pop(a, 0);

        lenv_put(f->env, sym, val);

        lval_free(sym);
        lval_free(val);
    }

    // Argument list is now bound, so it can be cleaned up.
    lval_free(a);

#if VARIABLE_ARGUMENTS
    // If '&' remains in the formal list, bind to empty list.
    if (f->formals->cell_count > 0
        && !strcmp(f->formals->cell[0]->sym, "&")) {
        // Ensure that '&' is not passed invalidly.
        if (f->formals->cell_count != 2)
            return lval_err(
                "function format invalid. "
                "Symbol '&' not followed by single symbol."
            );

        // Pop and delete the '&' symbol.
        lval_free(lval_pop(f->formals, 0));

        // Pop next symbol and create empty list.
        lval *sym = lval_pop(f->formals, 0);
        lval *val = lval_qexpr();

        lenv_put(f->env, sym, val);

        lval_free(sym);
        lval_free(val);
    }
#endif

    // If all formals have been bound, evaluate.
    if (f->formals->cell_count == 0) {
        // Set the parent reference to the evaluation environment.
        f->env->parent_ref = e;

        // Evaluate the body and return.
        return lval_builtin_eval(
            f->env,
            lval_add(lval_sexpr(), lval_copy(f->body))
        );
    }

    // Otherwise, return the partially evaluated function.
    return lval_copy(f);
}

lval *lval_copy(lval *v) {
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_NUM: x->num = v->num; break;

        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;

        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        case LVAL_FUN:
            if (v->builtin) {
                x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->cell_count = v->cell_count;
            x->cell = malloc(x->cell_count * sizeof(lval *));
            for (int i = 0; i < x->cell_count; ++i)
                x->cell[i] = lval_copy(v->cell[i]);

            break;

        default: assert(false);
    }

    return x;
}

lenv *lenv_copy(lenv *e) {
    lenv *n = malloc(sizeof(lenv));
    n->parent_ref = e->parent_ref;
    n->count = e->count;

    n->syms = malloc(n->count * sizeof(char *));
    n->vals = malloc(n->count * sizeof(lval *));

    for (int i = 0; i < e->count; ++i) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);

        n->vals[i] = lval_copy(e->vals[i]);
    }

    return n;
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

    // If no symbol is found, check for it in the parent environment.
    if (e->parent_ref) return lenv_get(e->parent_ref, k);

    return lval_err("unbound symbol `%s`", k->sym);
}

void lenv_put(lenv *e, lval *k, lval *v) {
    // Iterate over all items in the environment
    // to see if the variable already exists.
    for (int i = 0; i < e->count; ++i) {
        if (!strcmp(e->syms[i], k->sym)) {
            // If it is found, delete the item at that position
            // and replace it with a copy of the given variable.
            lval_free(e->vals[i]);
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

void lenv_def(lenv *e, lval *k, lval *v) {
    while (e->parent_ref)
        e = e->parent_ref;

    lenv_put(e, k, v);
}

//
// Built-in functions.
//

lval *lval_builtin(lenv *e, lval *a, const char *fun) {
    if (!strcmp("list", fun)) return lval_builtin_list(e, a);
    if (!strcmp("head", fun)) return lval_builtin_head(e, a);
    if (!strcmp("tail", fun)) return lval_builtin_tail(e, a);
    if (!strcmp("join", fun)) return lval_builtin_join(e, a);
    if (!strcmp("eval", fun)) return lval_builtin_eval(e, a);
    if (strstr("+-/*",  fun)) return lval_builtin_op(e, a, fun);
    if (strstr("<=>=",  fun)) return lval_builtin_ord(e, a, fun); // < > <= >=
    if (strstr("!==",   fun)) return lval_builtin_cmp(e, a, fun); // == !=
    if (!strcmp("if",   fun)) return lval_builtin_if(e, a);

    lval_free(a);
    return lval_err("unknown function `%s`", fun);
}

#define LASSERT(args, cond, err_fmt, ...)             \
    if (!(cond)) {                                    \
        lval *err = lval_err(err_fmt, ##__VA_ARGS__); \
        lval_free(args);                              \
        return err;                                   \
    }

#define LASSERT_ARG_TYPE(fun, args, index, expected)                                       \
    LASSERT(                                                                               \
        args, (args)->cell[index]->type == expected,                                       \
        "function '%s' passed incorrect type for argument `%i`. Got `%s`, expected `%s`.", \
        fun, index, lval_type_name((args)->cell[index]->type), lval_type_name(expected))

#define LASSERT_ARG_COUNT(fun, args, count)                                             \
    LASSERT(                                                                            \
        args, (args)->cell_count == count,                                              \
        "function '%s' passed incorrect number of arguments. Got `%i`, expected `%i`.", \
        fun, (args)->cell_count, count)

#define LASSERT_ARG_NOT_EMPTY(fun, args, index)         \
    LASSERT(                                            \
        args, (args)->cell[index]->cell_count != 0,     \
        "function '%s' passed `{}` for argument `%i`.", \
        fun, index)

void lenv_add_builtins(lenv *e) {
    lenv_add_builtin(e, "\\", lval_builtin_lambda); // user-defined function
    lenv_add_builtin(e, "def", lval_builtin_def); // user-(globally-)defined variable
    lenv_add_builtin(e, "=", lval_builtin_put); // user-(locally-)defined variable

    lenv_add_builtin(e, "list", lval_builtin_list);
    lenv_add_builtin(e, "head", lval_builtin_head);
    lenv_add_builtin(e, "tail", lval_builtin_tail);
    lenv_add_builtin(e, "eval", lval_builtin_eval);
    lenv_add_builtin(e, "join", lval_builtin_join);

    lenv_add_builtin(e, "+", lval_builtin_add);
    lenv_add_builtin(e, "-", lval_builtin_sub);
    lenv_add_builtin(e, "*", lval_builtin_mul);
    lenv_add_builtin(e, "/", lval_builtin_div);

    lenv_add_builtin(e, "<=", lval_builtin_le);
    lenv_add_builtin(e, ">=", lval_builtin_ge);
    lenv_add_builtin(e, "<", lval_builtin_lt);
    lenv_add_builtin(e, ">", lval_builtin_gt);

    lenv_add_builtin(e, "==", lval_builtin_eq);
    lenv_add_builtin(e, "!=", lval_builtin_ne);

    lenv_add_builtin(e, "if", lval_builtin_if);
}

void lenv_add_builtin(lenv *e, const char *name, lbuiltin fun) {
    lval *k = lval_sym(name);
    lval *v = lval_fun(fun);

    lenv_put(e, k, v);

    lval_free(k);
    lval_free(v);
}

lval *lval_builtin_op(lenv *e, lval *a, const char *op) {
    // Ensure all arguments are numbers.
    for (int i = 0; i < a->cell_count; ++i)
        LASSERT_ARG_TYPE(op, a, /*index*/i, /*expected*/LVAL_NUM);

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

lval *lval_builtin_add(lenv *e, lval *a) { return lval_builtin_op(e, a, "+"); }
lval *lval_builtin_sub(lenv *e, lval *a) { return lval_builtin_op(e, a, "-"); }
lval *lval_builtin_mul(lenv *e, lval *a) { return lval_builtin_op(e, a, "*"); }
lval *lval_builtin_div(lenv *e, lval *a) { return lval_builtin_op(e, a, "/"); }

lval *lval_builtin_ord(lenv *e, lval *a, const char *op) {
    LASSERT_ARG_COUNT(op, a, /*count*/2);
    LASSERT_ARG_TYPE(op, a, /*index*/0, /*expected*/LVAL_NUM);
    LASSERT_ARG_TYPE(op, a, /*index*/1, /*expected*/LVAL_NUM);

    int result;
    if (!strcmp(op, "<")) result = a->cell[0]->num < a->cell[1]->num;
    if (!strcmp(op, ">")) result = a->cell[0]->num > a->cell[1]->num;
    if (!strcmp(op, "<=")) result = a->cell[0]->num <= a->cell[1]->num;
    if (!strcmp(op, ">=")) result = a->cell[0]->num >= a->cell[1]->num;

    lval_free(a);
    return lval_num(result);
}

lval *lval_builtin_lt(lenv *e, lval *a) { return lval_builtin_ord(e, a, "<"); }
lval *lval_builtin_gt(lenv *e, lval *a) { return lval_builtin_ord(e, a, ">"); }
lval *lval_builtin_le(lenv *e, lval *a) { return lval_builtin_ord(e, a, "<="); }
lval *lval_builtin_ge(lenv *e, lval *a) { return lval_builtin_ord(e, a, ">="); }

bool lval_equals(lval *x, lval *y) {
    if (x->type != y->type) return false;

    switch (x->type) {
        case LVAL_NUM: return x->num == y->num;

        case LVAL_ERR: return !strcmp(x->err, y->err);
        case LVAL_SYM: return !strcmp(x->sym, y->sym);

        case LVAL_FUN:
            if (x->builtin || y->builtin)
                return x->builtin == y->builtin;
            else
                return lval_equals(x->formals, y->formals)
                    && lval_equals(x->body, y->body);

        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->cell_count != y->cell_count) return false;

            for (int i = 0; i < x->cell_count; ++i)
                if (!lval_equals(x->cell[i], y->cell[i]))
                    return false;

            return true;
    }

    assert(false);
    return false;
}

lval *lval_builtin_cmp(lenv *e, lval *a, const char *op) {
    LASSERT_ARG_COUNT(op, a, /*count*/2);

    int result;
    if (!strcmp(op, "==")) result =  lval_equals(a->cell[0], a->cell[1]);
    if (!strcmp(op, "!=")) result = !lval_equals(a->cell[0], a->cell[1]);

    lval_free(a);
    return lval_num(result);
}

lval *lval_builtin_eq(lenv *e, lval *a) { return lval_builtin_cmp(e, a, "=="); }
lval *lval_builtin_ne(lenv *e, lval *a) { return lval_builtin_cmp(e, a, "!="); }

lval *lval_builtin_if(lenv *e, lval *a) {
    LASSERT_ARG_COUNT("if", a, /*count*/3);
    LASSERT_ARG_TYPE("if", a, /*index*/0, /*expected*/LVAL_NUM);
    LASSERT_ARG_TYPE("if", a, /*index*/1, /*expected*/LVAL_QEXPR);
    LASSERT_ARG_TYPE("if", a, /*index*/2, /*expected*/LVAL_QEXPR);

    // Mark both expressions evaluable.
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;

    lval *x = a->cell[0]->num           // condition
        ? lval_eval(e, lval_pop(a, 1))  // true
        : lval_eval(e, lval_pop(a, 2)); // false

    lval_free(a);
    return x;
}

lval *lval_builtin_list(lenv *e, lval *a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval *lval_builtin_head(lenv *e, lval *a) {
    LASSERT_ARG_COUNT("head", a, /*count*/1);
    LASSERT_ARG_TYPE("head", a, /*index*/0, /*expected*/LVAL_QEXPR);
    LASSERT_ARG_NOT_EMPTY("head", a, /*index*/0);

    // Take the first argument.
    lval *v = lval_take(a, 0);

    // Delete all elements that are not head and return.
    while (v->cell_count > 1) lval_free(lval_pop(v, 1));
    return v;
}

lval *lval_builtin_tail(lenv *e, lval *a) {
    LASSERT_ARG_COUNT("tail", a, /*count*/1);
    LASSERT_ARG_TYPE("tail", a, /*index*/0, /*expected*/LVAL_QEXPR);
    LASSERT_ARG_NOT_EMPTY("tail", a, /*index*/0);

    // Take the first argument.
    lval* v = lval_take(a, 0);

    // Delete the first element and return.
    lval_free(lval_pop(v, 0));
    return v;
}

lval *lval_builtin_eval(lenv *e, lval *a) {
    LASSERT_ARG_COUNT("eval", a, /*count*/1);
    LASSERT_ARG_TYPE("eval", a, /*index*/0, /*expected*/LVAL_QEXPR);

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval *lval_builtin_join(lenv *e, lval *a) {
    for (int i = 0; i < a->cell_count; ++i)
        LASSERT_ARG_TYPE("join", a, /*index*/i, /*expected*/LVAL_QEXPR);

    lval *x = lval_pop(a, 0);
    while (a->cell_count) x = lval_join(x, lval_pop(a, 0));

    lval_free(a);
    return x;
}

lval *lval_builtin_var(lenv *e, lval *a, const char *fun) {
    LASSERT_ARG_TYPE(fun, a, /*index*/0, /*expected*/LVAL_QEXPR);

    // First argument is (expected to be) a symbol list.
    lval *syms = a->cell[0];
    for (int i = 0; i < syms->cell_count; ++i) {
        LASSERT(
            a, syms->cell[i]->type == LVAL_SYM,
            "function '%s' cannot define non-symbol. Got `%s`, expected `%s`.",
            fun, lval_type_name(syms->cell[i]->type), lval_type_name(LVAL_SYM)
        );
    }

    LASSERT(
        a, syms->cell_count == a->cell_count - 1,
        "function '%s' cannot define an unmatched number of values to symbols. "
        "Got %s, expected %s.", fun, syms->cell_count, a->cell_count - 1
    );

    // Assign (copies of) values to symbols.
    for (int i = 0; i < syms->cell_count; ++i) {
        // Note that `syms` is `a->cell[0]`, hence why the `i`-th
        // value corresponds to the `i + 1`-th symbol in `a->cell`.

        if (!strcmp(fun, "def"))
            lenv_def(e, syms->cell[i], a->cell[i + 1]); // define in global scope
        else if (!strcmp(fun, "="))
            lenv_put(e, syms->cell[i], a->cell[i + 1]); // define in local scope
        else
            assert(false);
    }

    lval_free(a);
    return lval_sexpr();
}

lval *lval_builtin_def(lenv *e, lval *a) { return lval_builtin_var(e, a, "def"); }
lval *lval_builtin_put(lenv *e, lval *a) { return lval_builtin_var(e, a, "="); }

lval *lval_builtin_lambda(lenv *e, lval *a) {
    LASSERT_ARG_COUNT("\\", a, /*count*/2);
    LASSERT_ARG_TYPE("\\", a, /*index*/0, /*expected*/LVAL_QEXPR);
    LASSERT_ARG_TYPE("\\", a, /*index*/1, /*expected*/LVAL_QEXPR);

    // Check if the first Q-Expression only contains symbols.
    for (int i = 0; i < a->cell[0]->cell_count; ++i) {
        LASSERT(
            a, a->cell[0]->cell[i]->type == LVAL_SYM,
            "cannot define non-symbol. Got `%s`, expected `%s`.",
            lval_type_name(a->cell[0]->cell[i]->type), lval_type_name(LVAL_SYM)
        );
    }

    lval *formals = lval_pop(a, 0);
    lval *body = lval_pop(a, 0);
    lval_free(a);

    return lval_lambda(formals, body);
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
        lval *err = lval_err(
            "S-Expression starting with incorrect type. "
            "Got `%s`, expected `%s`.", lval_type_name(f->type), lval_type_name(LVAL_FUN)
        );
        lval_free(f);
        lval_free(v);
        return err;
    }

    // Call the (built-in or user-defined) function.
    lval *result = lval_call(e, f, v);

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
    if (v->type == LVAL_SEXPR) return lval_eval_sexpr(e, v);

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

void lval_print_lambda(const lval *v) {
    // (\ `formals` `body`)
    printf("(\\ ");
    lval_print(v->formals);
    putchar(' ');
    lval_print(v->body);
    putchar(')');
}

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
        case LVAL_NUM:      printf("%li", v->num);        break;
        case LVAL_ERR:      printf("Error: %s", v->err);  break;
        case LVAL_SYM:      printf("%s", v->sym);         break;
        case LVAL_FUN:
            if (v->builtin) printf("<builtin>");
            else            lval_print_lambda(v);
            break;
        case LVAL_SEXPR:    lval_print_expr(v, '(', ')'); break;
        case LVAL_QEXPR:    lval_print_expr(v, '{', '}'); break;
        default:            assert(false);
    }
}

void lval_println(const lval *v) { lval_print(v); putchar('\n'); }