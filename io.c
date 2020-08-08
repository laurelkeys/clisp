#include "io.h"

#ifdef _WIN32

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

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

#endif
