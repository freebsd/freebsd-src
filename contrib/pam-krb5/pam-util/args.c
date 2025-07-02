/*
 * Constructor and destructor for PAM data.
 *
 * The PAM utility functions often need an initial argument that encapsulates
 * the PAM handle, some configuration information, and possibly a Kerberos
 * context.  This implements a constructor and destructor for that data
 * structure.
 *
 * The individual PAM modules should provide a definition of the pam_config
 * struct appropriate to that module.  None of the PAM utility functions need
 * to know what that configuration struct looks like, and it must be freed
 * before calling putil_args_free().
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2010, 2012-2014
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

#include <config.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/pam.h>
#include <portable/system.h>

#include <errno.h>

#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Allocate a new pam_args struct and return it, or NULL on memory allocation
 * or Kerberos initialization failure.  If HAVE_KRB5 is defined, we also
 * allocate a Kerberos context.
 */
struct pam_args *
putil_args_new(pam_handle_t *pamh, int flags)
{
    struct pam_args *args;
#ifdef HAVE_KRB5
    krb5_error_code status;
#endif

    args = calloc(1, sizeof(struct pam_args));
    if (args == NULL) {
        putil_crit(NULL, "cannot allocate memory: %s", strerror(errno));
        return NULL;
    }
    args->pamh = pamh;
    args->silent = ((flags & PAM_SILENT) == PAM_SILENT);

#ifdef HAVE_KRB5
    if (issetugid())
        status = krb5_init_secure_context(&args->ctx);
    else
        status = krb5_init_context(&args->ctx);
    if (status != 0) {
        putil_err_krb5(args, status, "cannot create Kerberos context");
        free(args);
        return NULL;
    }
#endif /* HAVE_KRB5 */
    return args;
}


/*
 * Free a pam_args struct.  The config member must be freed separately.
 */
void
putil_args_free(struct pam_args *args)
{
    if (args == NULL)
        return;
#ifdef HAVE_KRB5
    free(args->realm);
    if (args->ctx != NULL)
        krb5_free_context(args->ctx);
#endif
    free(args);
}
