#ifndef __CLISP_IO_H__
#define __CLISP_IO_H__

#ifdef _WIN32

    // @FIXME handle input better (using a fixed size buffer,
    // even if large, is error-prone.. although common in C)
    #define BUFFER_SIZE 2048

    char *readline(char *prompt) ;

    void add_history(char *unused);

#else // *nix or macOS

    #include <editline/readline.h>
    #include <editline/history.h>

#endif

#endif // __CLISP_IO_H__
