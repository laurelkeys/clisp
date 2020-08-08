#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mpc.h"

#include "lval.h"
mpc_parser_t *Lispy;

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

int main(int argc, char *argv[]) {
    #define PARSERS_COUNT 8

    mpc_parser_t *Number  = mpc_new("number");
    mpc_parser_t *Symbol  = mpc_new("symbol"); // a-z A-Z 0-9 _+-*/\=<>!&
    mpc_parser_t *String  = mpc_new("string");
    mpc_parser_t *Comment = mpc_new("comment");
    mpc_parser_t *Sexpr   = mpc_new("sexpr"); // S(ymbol)-Expression
    mpc_parser_t *Qexpr   = mpc_new("qexpr"); // Q(uoted)-Expression
    mpc_parser_t *Expr    = mpc_new("expr");
    Lispy                 = mpc_new("lispy");

    #define PARSERS_COMMA_SEPARATED \
        Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
            number  : /-?[0-9]+/ ;                              \
            symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        \
            string  : /\"(\\\\.|[^\"])*\"/ ;                    \
            comment : /;[^\\r\\n]*/ ;                           \
            sexpr   : '(' <expr>* ')' ;                         \
            qexpr   : '{' <expr>* '}' ;                         \
            expr    : <number> | <symbol>                       \
                    | <string> | <comment>                      \
                    | <sexpr>  | <qexpr> ;                      \
            lispy   : /^/ <expr>* /$/ ;                         \
        ",
        PARSERS_COMMA_SEPARATED
    );

    // Create an environment with built-in functions.
    lenv *e = lenv_new();
    lenv_add_builtins(e);

    if (argc >= 2) {
        for (int i = 1; i < argc; ++i) {
            // Create an argument list with a single argument
            // (the filename), then load and evaluate its contents.
            lval *args = lval_add(lval_sexpr(), lval_str(argv[i]));
            lval *x = lval_builtin_load(e, args);

            if (x->type == LVAL_ERR) lval_println(x);
            lval_free(x);
        }
    } else {
        // Execute the REPL.
        puts("Lispy Version 0.0.0.0");
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
    }

    lenv_free(e);

    // Undefine and delete parsers.
    mpc_cleanup(PARSERS_COUNT, PARSERS_COMMA_SEPARATED);

    return 0;
}
