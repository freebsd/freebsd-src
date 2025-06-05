/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1999 by Theodore Ts'o.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.  THE SOFTWARE IS PROVIDED "AS IS" AND THEODORE TS'O (THE
 * AUTHOR) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  (Isn't
 * it sick that the U.S. culture of lawsuit-happy lawyers requires
 * this kind of disclaimer?)
 */

/*
 * Version 1.1, modified 2/27/1999
 *
 * argv_parse.c --- utility function for parsing a string into a
 *      argc, argv array.
 *
 * This file defines a function argv_parse() which parsing a
 * passed-in string, handling double quotes and backslashes, and
 * creates an allocated argv vector which can be freed using the
 * argv_free() function.
 *
 * See argv_parse.h for the formal definition of the functions.
 */

#include "prof_int.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <ctype.h>
#include <string.h>
#include "argv_parse.h"

#define STATE_WHITESPACE        1
#define STATE_TOKEN             2
#define STATE_QUOTED            3

/*
 * Returns 0 on success, -1 on failure.
 */
int argv_parse(char *in_buf, int *ret_argc, char ***ret_argv)
{
    int     argc = 0, max_argc = 0;
    char    **argv, **new_argv, *buf, ch;
    char    *cp = 0, *outcp = 0;
    int     state = STATE_WHITESPACE;

    buf = malloc(strlen(in_buf)+1);
    if (!buf)
        return -1;

    max_argc = 0; argc = 0; argv = 0;
    outcp = buf;
    for (cp = in_buf; (ch = *cp); cp++) {
        if (state == STATE_WHITESPACE) {
            if (isspace((int) ch))
                continue;
            /* Not whitespace, so start a new token */
            state = STATE_TOKEN;
            if (argc >= max_argc) {
                max_argc += 3;
                new_argv = realloc(argv,
                                   (max_argc+1)*sizeof(char *));
                if (!new_argv) {
                    if (argv) free(argv);
                    free(buf);
                    return -1;
                }
                argv = new_argv;
            }
            argv[argc++] = outcp;
        }
        if (state == STATE_QUOTED) {
            if (ch == '"')
                state = STATE_TOKEN;
            else
                *outcp++ = ch;
            continue;
        }
        /* Must be processing characters in a word */
        if (isspace((int) ch)) {
            /*
             * Terminate the current word and start
             * looking for the beginning of the next word.
             */
            *outcp++ = 0;
            state = STATE_WHITESPACE;
            continue;
        }
        if (ch == '"') {
            state = STATE_QUOTED;
            continue;
        }
        if (ch == '\\') {
            ch = *++cp;
            switch (ch) {
            case '\0':
                ch = '\\'; cp--; break;
            case 'n':
                ch = '\n'; break;
            case 't':
                ch = '\t'; break;
            case 'b':
                ch = '\b'; break;
            }
        }
        *outcp++ = ch;
    }
    if (state != STATE_WHITESPACE)
        *outcp++ = '\0';
    if (argv == 0) {
        argv = malloc(sizeof(char *));
        free(buf);
    }
    argv[argc] = 0;
    if (ret_argc)
        *ret_argc = argc;
    if (ret_argv)
        *ret_argv = argv;
    return 0;
}

void argv_free(char **argv)
{
    if (*argv)
        free(*argv);
    free(argv);
}

#ifdef DEBUG
/*
 * For debugging
 */

#include <stdio.h>

int main(int argc, char **argv)
{
    int     ac, ret;
    char    **av, **cpp;
    char    buf[256];

    while (!feof(stdin)) {
        if (fgets(buf, sizeof(buf), stdin) == NULL)
            break;
        ret = argv_parse(buf, &ac, &av);
        if (ret != 0) {
            printf("Argv_parse returned %d!\n", ret);
            continue;
        }
        printf("Argv_parse returned %d arguments...\n", ac);
        for (cpp = av; *cpp; cpp++) {
            if (cpp != av)
                printf(", ");
            printf("'%s'", *cpp);
        }
        printf("\n");
        argv_free(av);
    }
    exit(0);
}
#endif /* DEBUG */
