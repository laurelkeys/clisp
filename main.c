#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>

// @FIXME handle input better (using a fixed size buffer,
// even if large, is error-prone.. although common in C)
#define BUFFER_SIZE 2048

static char buffer[BUFFER_SIZE];

char *readline(char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, BUFFER_SIZE, stdin);

    // @FIXME use strncpy and pass a max size to strlen
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';

    return cpy;
}

void add_history(char *unused) {}

#else // *nix or macOS
#include <editline/readline.h>
#include <editline/history.h>
#endif

typedef enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR } LVAL_TYPE;

// A Lispy value, which is either "some thing" or an error.
typedef struct lval {
    LVAL_TYPE   type;
    long        num;
    char        *err;
    char        *sym;
    int         cell_count;
    struct lval **cell;
} lval;

lval * lval_num(const long num) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = num;
    return v;
}

lval * lval_err(const char *err) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    // @FIXME use strncpy and pass a max size to strlen
    v->err = malloc(strlen(err) + 1);
    strcpy(v->err, err);
    return v;
}

lval * lval_sym(const char *sym) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    // @FIXME use strncpy and pass a max size to strlen
    v->sym = malloc(strlen(sym) + 1);
    strcpy(v->sym, sym);
    return v;
}

lval * lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->cell_count = 0;
    v->cell = NULL;
    return v;
}

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

lval * lval_read_num(const mpc_ast_t *t) {
    errno = 0;
    const long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE
        ? lval_num(x)
        : lval_err("invalid number");
}

lval * lval_add(lval *v, lval *x) {
    v->cell_count++;
    v->cell = realloc(v->cell, v->cell_count * sizeof(lval *));
    v->cell[v->cell_count - 1] = x;
    return v;
}

lval * lval_read(const mpc_ast_t *t) {
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

// Forward declaration.
void lval_print(const lval *v);

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

#if 0
lval eval_op(const lval x, const char *op, const lval y) {
    if (x.type == LVAL_ERR) return x;
    if (y.type == LVAL_ERR) return y;

    assert(x.type == LVAL_NUM && y.type == LVAL_NUM);

    if (!strcmp(op, "+")) return lval_num(x.num + y.num);
    if (!strcmp(op, "-")) return lval_num(x.num - y.num);
    if (!strcmp(op, "*")) return lval_num(x.num * y.num);
    if (!strcmp(op, "/")) {
        return y.num == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num(x.num / y.num);
    }

    return lval_err(LERR_BAD_OP);
}

lval eval(const mpc_ast_t *t) {
    // If tagged as number, return it directly.
    if (strstr(t->tag, "number")) {
        errno = 0;
        const long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE
            ? lval_num(x)
            : lval_err(LERR_BAD_NUM);
    }

    // The operator is always the second child.
    const char *op = t->children[1]->contents;

    // Store the third child in `x`, iterate through
    // the remaining children, and combine the result.
    lval x = eval(t->children[2]);

    for (int i = 3; strstr(t->children[i]->tag, "expr"); ++i)
        x = eval_op(x, op, eval(t->children[i]));

    return x;
}
#endif

int main(int argc, const char *argv[]) {
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr  = mpc_new("sexpr"); // S(ymbol)-Expression
    mpc_parser_t *Expr   = mpc_new("expr");
    mpc_parser_t *Lispy  = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                            \
            number : /-?[0-9]+/ ;                    \
            symbol : '+' | '-' | '*' | '/' ;         \
            sexpr  : '(' <expr>* ')' ;               \
            expr   : <number> | <symbol> | <sexpr> ; \
            lispy  : /^/ <expr>* /$/ ;               \
        ",
        Number, Symbol, Sexpr, Expr, Lispy
    );

    puts("Lispy Version 0.6.6.6");
    puts("Press Ctrl+C to exit\n");

    while (true) {
        char *input = readline("lispy> ");
        add_history(input);

        // Parse the user input.
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval *x = lval_read(r.output);
            lval_println(x);
            lval_free(x);

            // lval_println(eval(r.output));
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // Undefine and delete parsers.
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

    return 0;
}