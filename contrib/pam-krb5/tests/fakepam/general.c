/*
 * Interface for fake PAM library, used for testing.
 *
 * This contains the basic public interfaces for the fake PAM library, used
 * for testing, and some general utility functions.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2011, 2014
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
#include <portable/pam.h>
#include <portable/system.h>

#include <errno.h>
#include <pwd.h>

#include <tests/fakepam/pam.h>

/* Stores the static struct passwd returned by getpwnam if the name matches. */
static struct passwd *pwd_info = NULL;

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))


/*
 * Initializes the pam_handle_t data structure.  This function is only called
 * from test programs, not from any of the module code.  We can put anything
 * we want in this structure, since it's opaque to the regular code.
 */
int
pam_start(const char *service_name, const char *user,
          const struct pam_conv *pam_conversation, pam_handle_t **pamh)
{
    struct pam_handle *handle;

    handle = calloc(1, sizeof(struct pam_handle));
    if (handle == NULL)
        return PAM_BUF_ERR;
    handle->service = service_name;
    handle->user = user;
    handle->conversation = pam_conversation;
    *pamh = handle;
    return PAM_SUCCESS;
}


/*
 * Free the pam_handle_t data structure and related resources.  This is
 * important to test the data cleanups.  Freeing the memory is not strictly
 * required since it's only used for testing, but it helps keep our memory
 * usage clean so that we can run the test suite under valgrind.
 */
int
pam_end(pam_handle_t *pamh, int status)
{
    struct fakepam_data *item, *next;
    size_t i;

    if (pamh->environ != NULL) {
        for (i = 0; pamh->environ[i] != NULL; i++)
            free(pamh->environ[i]);
        free(pamh->environ);
    }
    free(pamh->authtok);
    free(pamh->oldauthtok);
    free(pamh->rhost);
    free(pamh->ruser);
    free(pamh->tty);
    for (item = pamh->data; item != NULL;) {
        if (item->cleanup != NULL)
            item->cleanup(pamh, item->data, status);
        free(item->name);
        next = item->next;
        free(item);
        item = next;
    }
    free(pamh);
    return PAM_SUCCESS;
}


/*
 * Interface specific to this fake PAM library to set the struct passwd that's
 * returned by getpwnam queries if the name matches.
 */
void
pam_set_pwd(struct passwd *pwd)
{
    pwd_info = pwd;
}


/*
 * For testing purposes, we want to be able to intercept getpwnam.  This is
 * fairly easy on platforms that have pam_modutil_getpwnam, since then our
 * code will always call that function and we can provide an implementation
 * that does whatever we want.  For platforms that don't have that function,
 * we'll try to intercept the C library getpwnam function.
 *
 * We store only one struct passwd data structure statically.  If the user
 * we're looking up matches that, we return it; otherwise, we return NULL.
 */
#ifdef HAVE_PAM_MODUTIL_GETPWNAM
struct passwd *
pam_modutil_getpwnam(pam_handle_t *pamh UNUSED, const char *name)
{
    if (pwd_info != NULL && strcmp(pwd_info->pw_name, name) == 0)
        return pwd_info;
    else {
        errno = 0;
        return NULL;
    }
}
#else
struct passwd *
getpwnam(const char *name)
{
    if (pwd_info != NULL && strcmp(pwd_info->pw_name, name) == 0)
        return pwd_info;
    else {
        errno = 0;
        return NULL;
    }
}
#endif
