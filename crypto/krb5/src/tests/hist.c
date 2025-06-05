/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/hist.c - Perform unusual operations on history keys */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This program is invoked from t_pwhist.py to simulate some conditions
 * normally only seen in databases created before krb5 1.3.  With the "make"
 * argument, the history key is rolled over to a kvno containing two keys
 * (since krb5 1.3 we ordinarily ensure that there's only one).  With the
 * "swap" argument, the two history keys are swapped in order; we use this
 * operation to simulate the case where krb5 1.7 or earlier chose something
 * other than the first history key to create password history entries.
 */

#include <k5-int.h>
#include <kadm5/admin.h>

static void
check(krb5_error_code ret)
{
    if (ret) {
	fprintf(stderr, "Unexpected failure, aborting\n");
	abort();
    }
}

int
main(int argc, char **argv)
{
    krb5_context ctx;
    krb5_db_entry *ent;
    krb5_principal hprinc;
    kadm5_principal_ent_rec kent;
    krb5_key_salt_tuple ks[2];
    krb5_key_data kd;
    kadm5_config_params params = { 0 };
    void *handle;
    char *realm;
    long mask = KADM5_PRINCIPAL | KADM5_MAX_LIFE | KADM5_ATTRIBUTES;

    check(kadm5_init_krb5_context(&ctx));
    check(krb5_parse_name(ctx, "kadmin/history", &hprinc));
    check(krb5_get_default_realm(ctx, &realm));
    params.mask |= KADM5_CONFIG_REALM;
    params.realm = realm;
    check(kadm5_init(ctx, "user", "", "", &params, KADM5_STRUCT_VERSION,
                     KADM5_API_VERSION_4, NULL, &handle));
    if (strcmp(argv[1], "make") == 0) {
        memset(&kent, 0, sizeof(kent));
        kent.principal = hprinc;
        kent.max_life = KRB5_KDB_DISALLOW_ALL_TIX;
        kent.attributes = 0;
	ks[0].ks_enctype = ENCTYPE_AES256_CTS_HMAC_SHA1_96;
	ks[0].ks_salttype = KRB5_KDB_SALTTYPE_NORMAL;
	ks[1].ks_enctype = ENCTYPE_AES128_CTS_HMAC_SHA1_96;
	ks[1].ks_salttype = KRB5_KDB_SALTTYPE_NORMAL;
        check(kadm5_create_principal_3(handle, &kent, mask, 2, ks, NULL));
    } else if (strcmp(argv[1], "swap") == 0) {
        check(krb5_db_get_principal(ctx, hprinc, 0, &ent));
	kd = ent->key_data[0];
	ent->key_data[0] = ent->key_data[1];
	ent->key_data[1] = kd;
        check(krb5_db_put_principal(ctx, ent));
        krb5_db_free_principal(ctx, ent);
    }
    krb5_free_default_realm(ctx, realm);
    kadm5_destroy(handle);
    krb5_free_principal(ctx, hprinc);
    krb5_free_context(ctx);
    return 0;
}
