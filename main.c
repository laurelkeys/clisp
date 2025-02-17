#include <stdio.h>
#include <stdbool.h>

#include "ext/mpc.h"

#include "io.h"
#include "lval.h"

mpc_parser_t *Lispy;

int main(int argc, char *argv[]) {

    Lispy                 = mpc_new("lispy");
    mpc_parser_t *Number  = mpc_new("number");
    mpc_parser_t *Symbol  = mpc_new("symbol"); // a-z A-Z 0-9 _+-*/\=<>!&
    mpc_parser_t *String  = mpc_new("string");
    mpc_parser_t *Comment = mpc_new("comment");
    mpc_parser_t *Sexpr   = mpc_new("sexpr"); // S(ymbol)-Expression
    mpc_parser_t *Qexpr   = mpc_new("qexpr"); // Q(uoted)-Expression
    mpc_parser_t *Expr    = mpc_new("expr");

    #define PARSERS_COUNT 8
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

    // Load the standard library functions.
    lval *std = lval_builtin_load(e, lval_add(lval_sexpr(), lval_str("prelude.cl")));
    if (std->type == LVAL_ERR) lval_println(std);
    lval_free(std);

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
