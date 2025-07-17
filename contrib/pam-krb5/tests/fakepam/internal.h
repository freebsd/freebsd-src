/*
 * Internal data types and prototypes for the fake PAM test framework.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2021 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef FAKEPAM_INTERNAL_H
#define FAKEPAM_INTERNAL_H 1

#include <portable/pam.h>
#include <sys/types.h>

/* Forward declarations to avoid unnecessary includes. */
struct output;
struct script_config;

/* The type of a PAM module call. */
typedef int (*pam_call)(pam_handle_t *, int, int, const char **);

/* The possible PAM groups as element numbers in an array of options. */
enum group_type
{
    GROUP_ACCOUNT = 0,
    GROUP_AUTH = 1,
    GROUP_PASSWORD = 2,
    GROUP_SESSION = 3,
};

/* Holds a PAM argc and argv. */
struct options {
    char **argv;
    int argc;
};

/*
 * Holds a linked list of actions: a PAM call that should return some
 * status.
 */
struct action {
    char *name;
    pam_call call;
    int flags;
    enum group_type group;
    int status;
    struct action *next;
};

/* Holds an expected PAM prompt style, the prompt, and the response. */
struct prompt {
    int style;
    char *prompt;
    char *response;
};

/* Holds an array of PAM prompts and the current index into that array. */
struct prompts {
    struct prompt *prompts;
    size_t size;
    size_t allocated;
    size_t current;
};

/*
 * Holds the complete set of things that we should do, configuration for them,
 * and expected output and return values.
 */
struct work {
    struct options options[4];
    struct action *actions;
    struct prompts *prompts;
    struct output *output;
    int end_flags;
};

BEGIN_DECLS


/* Create a new output struct. */
struct output *output_new(void);

/* Add a new output line (with numeric priority) to an output struct. */
void output_add(struct output *, int, const char *);


/*
 * Parse a PAM interaction script.  Returns the total work to do as a work
 * struct.
 */
struct work *parse_script(FILE *, const struct script_config *);

END_DECLS

#endif /* !FAKEPAM_API_H */
