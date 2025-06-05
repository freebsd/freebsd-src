/*
 * Replacement for krb5_kuserok for testing.
 *
 * This is a reimplementation of krb5_kuserok that uses the replacement
 * getpwnam function and the special passwd struct internal to the fake PAM
 * module to locate .k5login.  The default Kerberos krb5_kuserok always calls
 * the system getpwnam, which we may not be able to intercept, and will
 * therefore fail because it can't locate the .k5login file for the test user
 * (or succeed oddly because it finds some random file on the testing system).
 *
 * This implementation is drastically simplified from the Kerberos library
 * version, and much less secure (which shouldn't matter since it's only
 * acting on test data).
 *
 * This is an optional part of the fake PAM library and can be omitted when
 * testing modules that don't use Kerberos.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2011
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
#include <portable/krb5.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <pwd.h>

#include <tests/fakepam/pam.h>
#include <tests/tap/string.h>


/*
 * Given a Kerberos principal representing the authenticated identity and the
 * username of the local account, return true if that principal is authorized
 * to log on to that account.  The principal is authorized if the .k5login
 * file does not exist and the user matches the localname form of the
 * principal, or if the file does exist and the principal is listed in it.
 *
 * This version retrieves the home directory from the internal fake PAM
 * library path.
 */
krb5_boolean
krb5_kuserok(krb5_context ctx, krb5_principal princ, const char *user)
{
    char *principal, *path;
    struct passwd *pwd;
    FILE *file;
    krb5_error_code code;
    char buffer[BUFSIZ];
    bool found = false;
#ifdef HAVE_PAM_MODUTIL_GETPWNAM
    struct pam_handle pamh;
#endif

    /*
     * Find .k5login and confirm if it exists.  If it doesn't, fall back on
     * krb5_aname_to_localname.
     */
#ifdef HAVE_PAM_MODUTIL_GETPWNAM
    memset(&pamh, 0, sizeof(pamh));
    pwd = pam_modutil_getpwnam(&pamh, user);
#else
    pwd = getpwnam(user);
#endif
    if (pwd == NULL)
        return false;
    basprintf(&path, "%s/.k5login", pwd->pw_dir);
    if (access(path, R_OK) < 0) {
        free(path);
        code = krb5_aname_to_localname(ctx, princ, sizeof(buffer), buffer);
        return (code == 0 && strcmp(buffer, user) == 0);
    }
    file = fopen(path, "r");
    if (file == NULL) {
        free(path);
        return false;
    }
    free(path);

    /* .k5login exists.  Scan it for the principal. */
    if (krb5_unparse_name(ctx, princ, &principal) != 0) {
        fclose(file);
        return false;
    }
    while (!found && (fgets(buffer, sizeof(buffer), file) != NULL)) {
        if (buffer[strlen(buffer) - 1] == '\n')
            buffer[strlen(buffer) - 1] = '\0';
        if (strcmp(buffer, principal) == 0)
            found = true;
    }
    fclose(file);
    krb5_free_unparsed_name(ctx, principal);
    return found;
}
