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

int main(void) {
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol"); // a-z A-Z 0-9 _+-*/\=<>!&
    mpc_parser_t *Sexpr  = mpc_new("sexpr"); // S(ymbol)-Expression
    mpc_parser_t *Qexpr  = mpc_new("qexpr"); // Q(uoted)-Expression
    mpc_parser_t *Expr   = mpc_new("expr");
    mpc_parser_t *Lispy  = mpc_new("lispy");

    // Note:
    // a-z A-Z 0-9 _ + - * / \ = < > ! &

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                      \
            number : /-?[0-9]+/ ;                              \
            symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        \
            sexpr  : '(' <expr>* ')' ;                         \
            qexpr  : '{' <expr>* '}' ;                         \
            expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
            lispy  : /^/ <expr>* /$/ ;                         \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy
    );

    // Create an environment with built-in functions.
    lenv *e = lenv_new();
    lenv_add_builtins(e);

    puts("Lispy Version 0.6.6.6");
    puts("Press Ctrl+C to exit\n");

    while (true) {
        char *input = readline("lispy> ");
        add_history(input);

        // Parse the user input.
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval *x = lval_eval(e, lval_read(r.output));
            lval_println(x);

            lval_free(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    lenv_free(e);

    // Undefine and delete parsers.
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}
