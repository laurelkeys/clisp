#include "lval.h"

//
// lval constructors.
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

//
// lval destructor.
//

void lval_free(lval *v) {
    switch (v->type) {
        case LVAL_NUM: break;

        case LVAL_ERR: free(v->err); break;

        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXPR:
            while (v->cell_count) lval_free(v->cell[--(v->cell_count)]);
            free(v->cell);
            break;
    }

    free(v); v = NULL;
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

//
// Read functions.
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
    lval* x = NULL;
    if (!strcmp(t->tag, ">"))    x = lval_sexpr();
    if (strstr(t->tag, "sexpr")) x = lval_sexpr();

    // Fill this list with any valid expression contained within.
    for (int i = 0; i < t->children_num; ++i) {
        if (!strcmp(t->children[i]->contents, "(")) continue;
        if (!strcmp(t->children[i]->contents, ")")) continue;
        if (!strcmp(t->children[i]->tag, "regex"))  continue;
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

//
// Print functions.
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
    }
}

void lval_println(const lval *v) {
    lval_print(v); putchar('\n');
}