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
 * argv_parse.h --- header file for the argv parser.
 *
 * This file defines the interface for the functions argv_parse() and
 * argv_free().
 *
 ***********************************************************************
 * int argv_parse(char *in_buf, int *ret_argc, char ***ret_argv)
 *
 * This function takes as its first argument a string which it will
 * parse into an argv argument vector, with each white-space separated
 * word placed into its own slot in the argv.  This function handles
 * double quotes and backslashes so that the parsed words can contain
 * special characters.   The count of the number words found in the
 * parsed string, as well as the argument vector, are returned into
 * ret_argc and ret_argv, respectively.
 ***********************************************************************
 * extern void argv_free(char **argv);
 *
 * This function frees the argument vector created by argv_parse().
 ***********************************************************************
 */

extern int argv_parse(char *in_buf, int *ret_argc, char ***ret_argv);
extern void argv_free(char **argv);
