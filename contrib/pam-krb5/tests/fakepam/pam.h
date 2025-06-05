/*
 * Testing interfaces to the fake PAM library.
 *
 * This header defines the interfaces to the fake PAM library that are used by
 * test code to initialize the library and recover test data from it.  We
 * don't define any interface that we're going to duplicate from the main PAM
 * API.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2012
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

#ifndef FAKEPAM_PAM_H
#define FAKEPAM_PAM_H 1

#include <config.h>
#include <portable/macros.h>
#include <portable/pam.h>

/* Used inside the fake PAM library to hold data items. */
struct fakepam_data {
    char *name;
    void *data;
    void (*cleanup)(pam_handle_t *, void *, int);
    struct fakepam_data *next;
};

/* This is an opaque data structure, so we can put whatever we want in it. */
struct pam_handle {
    const char *service;
    const char *user;
    char *authtok;
    char *oldauthtok;
    char *rhost;
    char *ruser;
    char *tty;
    const struct pam_conv *conversation;
    char **environ;
    struct fakepam_data *data;
    struct passwd *pwd;
};

/*
 * Used to accumulate output from the PAM module.  Each call to a logging
 * function will result in an additional line added to the array, and count
 * will hold the total.
 */
struct output {
    size_t count;
    size_t allocated;
    struct {
        int priority;
        char *line;
    } * lines;
};

BEGIN_DECLS

/*
 * Sets the struct passwd returned by getpwnam calls.  The last struct passed
 * to this function will be returned provided the pw_name matches.
 */
void pam_set_pwd(struct passwd *pwd);

/*
 * Returns the accumulated messages logged with pam_syslog or pam_vsyslog
 * since the last call to pam_output and then clears the output.  Returns
 * newly allocated memory that the caller is responsible for freeing with
 * pam_output_free, or NULL if no output has been logged since the last call
 * or since startup.
 */
struct output *pam_output(void);
void pam_output_free(struct output *);

END_DECLS

#endif /* !FAKEPAM_API_H */
