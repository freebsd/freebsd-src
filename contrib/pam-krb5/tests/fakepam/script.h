/*
 * PAM interaction script API.
 *
 * Provides an interface that loads a PAM interaction script from a file and
 * runs through that script, calling the internal PAM module functions and
 * checking their results.  This allows automation of PAM testing through
 * external data files instead of coding everything in C.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Russ Allbery <eagle@eyrie.org>
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

#ifndef TESTS_MODULE_SCRIPT_H
#define TESTS_MODULE_SCRIPT_H 1

#include <portable/pam.h>

#include <tests/tap/basic.h>

/* A test callback called after PAM functions are run but before pam_end. */
struct script_config;
typedef void (*script_callback)(pam_handle_t *, const struct script_config *,
                                void *);

/* Configuration for the PAM interaction script API. */
struct script_config {
    const char *user;         /* Username to pass into pam_start (%u). */
    const char *password;     /* Substituted for %p in prompts. */
    const char *newpass;      /* Substituted for %n in prompts. */
    const char *extra[10];    /* Substituted for %0-%9 in logging. */
    const char *authtok;      /* Stored as AUTHTOK before PAM. */
    const char *oldauthtok;   /* Stored as OLDAUTHTOK before PAM. */
    script_callback callback; /* Called after PAM, before pam_end. */
    void *data;               /* Passed to the callback function. */
};

BEGIN_DECLS

/*
 * Given the file name of an interaction script (which may be a full path or
 * relative to C_TAP_SOURCE or C_TAP_BUILD) and configuration containing other
 * parameters such as the user, run that script, reporting the results via the
 * TAP format.
 */
void run_script(const char *file, const struct script_config *)
    __attribute__((__nonnull__));

/*
 * The same as run_script, but run every script found in the given directory,
 * skipping file names that contain characters other than alphanumerics and -.
 */
void run_script_dir(const char *dir, const struct script_config *)
    __attribute__((__nonnull__));

END_DECLS

#endif /* !TESTS_MODULE_SCRIPT_H */
