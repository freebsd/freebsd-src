/*
 * Kerberos test setup requiring the kadmin API.
 *
 * This file collects Kerberos test setup functions that use the kadmin API to
 * put principals into particular configurations for testing.  Currently, the
 * only implemented functionality is to mark a password as expired.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017 Russ Allbery <eagle@eyrie.org>
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
#ifdef HAVE_KADM5CLNT
#    include <portable/kadmin.h>
#    include <portable/krb5.h>
#endif
#include <portable/system.h>

#include <time.h>

#include <tests/tap/basic.h>
#include <tests/tap/kadmin.h>
#include <tests/tap/kerberos.h>

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))


/*
 * Given the principal to set an expiration on, set that principal to have an
 * expired password.  This requires that the realm admin server be configured
 * either in DNS (with SRV records) or in krb5.conf (possibly the one
 * KRB5_CONFIG is pointing to).  Authentication is done using the keytab
 * stored in config/admin-keytab.
 *
 * Returns true on success.  Returns false if necessary configuration is
 * missing so that the caller can choose whether to call bail or skip_all.  If
 * the configuration is present but the operation fails, bails.
 */
#ifdef HAVE_KADM5CLNT
bool
kerberos_expire_password(const char *principal, time_t expires)
{
    char *path, *user;
    const char *realm;
    krb5_context ctx;
    krb5_principal admin = NULL;
    krb5_principal princ = NULL;
    kadm5_ret_t code;
    kadm5_config_params params;
    kadm5_principal_ent_rec ent;
    void *handle;
    bool okay = false;

    /* Set up for making our call. */
    path = test_file_path("config/admin-keytab");
    if (path == NULL)
        return false;
    code = krb5_init_context(&ctx);
    if (code != 0)
        bail_krb5(ctx, code, "error initializing Kerberos");
    admin = kerberos_keytab_principal(ctx, path);
    realm = krb5_principal_get_realm(ctx, admin);
    code = krb5_set_default_realm(ctx, realm);
    if (code != 0)
        bail_krb5(ctx, code, "cannot set default realm");
    code = krb5_unparse_name(ctx, admin, &user);
    if (code != 0)
        bail_krb5(ctx, code, "cannot unparse admin principal");
    code = krb5_parse_name(ctx, principal, &princ);
    if (code != 0)
        bail_krb5(ctx, code, "cannot parse principal %s", principal);

    /*
     * If the actual kadmin calls fail, we may be built with MIT Kerberos
     * against a Heimdal server or vice versa.  Return false to skip the
     * tests.
     */
    memset(&params, 0, sizeof(params));
    params.realm = (char *) realm;
    params.mask = KADM5_CONFIG_REALM;
    code = kadm5_init_with_skey_ctx(ctx, user, path, KADM5_ADMIN_SERVICE,
                                    &params, KADM5_STRUCT_VERSION,
                                    KADM5_API_VERSION, &handle);
    if (code != 0) {
        diag_krb5(ctx, code, "error initializing kadmin");
        goto done;
    }
    memset(&ent, 0, sizeof(ent));
    ent.principal = princ;
    ent.pw_expiration = (krb5_timestamp) expires;
    code = kadm5_modify_principal(handle, &ent, KADM5_PW_EXPIRATION);
    if (code == 0)
        okay = true;
    else
        diag_krb5(ctx, code, "error setting password expiration");

done:
    kadm5_destroy(handle);
    krb5_free_unparsed_name(ctx, user);
    krb5_free_principal(ctx, admin);
    krb5_free_principal(ctx, princ);
    krb5_free_context(ctx);
    test_file_path_free(path);
    return okay;
}
#else  /* !HAVE_KADM5CLNT */
bool
kerberos_expire_password(const char *principal UNUSED, time_t expires UNUSED)
{
    return false;
}
#endif /* !HAVE_KADM5CLNT */
