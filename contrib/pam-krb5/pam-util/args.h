/*
 * Standard structure for PAM data.
 *
 * The PAM utility functions often need an initial argument that encapsulates
 * the PAM handle, some configuration information, and possibly a Kerberos
 * context.  This header provides a standard structure definition.
 *
 * The individual PAM modules should provide a definition of the pam_config
 * struct appropriate to that module.  None of the PAM utility functions need
 * to know what that configuration struct looks like.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2010, 2013
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

#ifndef PAM_UTIL_ARGS_H
#define PAM_UTIL_ARGS_H 1

#include <config.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/pam.h>
#include <portable/stdbool.h>

/* Opaque struct from the PAM utility perspective. */
struct pam_config;

struct pam_args {
    pam_handle_t *pamh;        /* Pointer back to the PAM handle. */
    struct pam_config *config; /* Per-module PAM configuration. */
    bool debug;                /* Log debugging information. */
    bool silent;               /* Do not pass text to the application. */
    const char *user;          /* User being authenticated. */

#ifdef HAVE_KRB5
    krb5_context ctx; /* Context for Kerberos operations. */
    char *realm;      /* Kerberos realm for configuration. */
#endif
};

BEGIN_DECLS

/* Default to a hidden visibility for all internal functions. */
#pragma GCC visibility push(hidden)

/*
 * Allocate and free the pam_args struct.  We assume that user is a pointer to
 * a string maintained elsewhere and don't free it here.  config must be freed
 * separately by the caller.
 */
struct pam_args *putil_args_new(pam_handle_t *, int flags);
void putil_args_free(struct pam_args *);

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !PAM_UTIL_ARGS_H */
