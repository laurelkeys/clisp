#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mpc.h"

#include "lval.h"

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