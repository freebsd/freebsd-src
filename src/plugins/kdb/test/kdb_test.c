/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/test/kdb_test.c - Test KDB module */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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
 * This is a read-only KDB module intended to help test KDC behavior which
 * cannot be exercised with the DB2 module.  Responses are read from the
 * dbmodules subsection according to this example:
 *
 *     [dbmodules]
 *         test = {
 *             alias = {
 *                 aliasname = canonname
 *                 # For cross-realm aliases, only the realm part will
 *                 # matter to the client.
 *                 aliasname = @FOREIGN_REALM
 *                 enterprise@PRINC = @FOREIGN_REALM
 *             }
 *             princs = {
 *                 krbtgt/KRBTEST.COM = {
 *                     flags = +preauth +ok-to-auth-as-delegate
 *                     maxlife = 1d
 *                     maxrenewlife = 7d
 *                     expiration = 14d # relative to current time
 *                     pwexpiration = 1h
 *                     # Initial number is kvno; defaults to 1.
 *                     keys = 3 aes256-cts aes128-cts:normal
 *                     keys = 2 rc4-hmac
 *                     strings = key1:value1
 *                     strings = key2:value2
 *                 }
 *             }
 *             delegation = {
 *                 # Traditional constrained delegation; target_service
 *                 # must be in the same realm.
 *                 intermediate_service = target_service
 *             }
 *             rbcd = {
 *                 # Resource-based constrained delegation;
 *                 # intermediate_service may be in a different realm.
 *                 target_service = intermediate_service
 *             }
 *         }
 *
 * Key values are generated using a hash of the kvno, enctype, salt type,
 * principal name, and lookup realm.  This module does not use master key
 * encryption, so it serves as a partial test of the DAL's ability to avoid
 * that.
 *
 * Inbound cross-realm TGT entries are currently implicit; they will use the
 * same configuration and key enctypes as the local krbtgt principal, although
 * they will use different keys (because the lookup realm is hashed in).
 * Outgoing cross-realm TGT entries must be added explicitly
 * (krbtgt/OTHER_REALM).
 */

#include "k5-int.h"
#include "kdb5.h"
#include "adm_proto.h"
#include <ctype.h>

#define TEST_AD_TYPE -456

#define IS_TGS_PRINC(p) ((p)->length == 2 &&                            \
                         data_eq_string((p)->data[0], KRB5_TGS_NAME))

typedef struct {
    void *profile;
    char *section;
    const char *names[6];
} *testhandle;

static void *
ealloc(size_t sz)
{
    void *p = calloc(sz, 1);

    if (p == NULL)
        abort();
    return p;
}

static char *
estrdup(const char *s)
{
    char *copy = strdup(s);

    if (copy == NULL)
        abort();
    return copy;
}

static void
check(krb5_error_code code)
{
    if (code != 0)
        abort();
}

/* Set up for a profile query using h->names.  Look up s1 -> s2 -> s3 (some of
 * which may be NULL) within this database's dbmodules section. */
static void
set_names(testhandle h, const char *s1, const char *s2, const char *s3)
{
    h->names[0] = KDB_MODULE_SECTION;
    h->names[1] = h->section;
    h->names[2] = s1;
    h->names[3] = s2;
    h->names[4] = s3;
    h->names[5] = NULL;
}

/* Look up a string within this database's dbmodules section. */
static char *
get_string(testhandle h, const char *s1, const char *s2, const char *s3)
{
    krb5_error_code ret;
    char **values, *val;

    set_names(h, s1, s2, s3);
    ret = profile_get_values(h->profile, h->names, &values);
    if (ret == PROF_NO_RELATION)
        return NULL;
    if (ret)
        abort();
    val = estrdup(values[0]);
    profile_free_list(values);
    return val;
}

/* Look up a duration within this database's dbmodules section. */
static krb5_deltat
get_duration(testhandle h, const char *s1, const char *s2, const char *s3)
{
    char *strval = get_string(h, s1, s2, s3);
    krb5_deltat val;

    if (strval == NULL)
        return 0;
    check(krb5_string_to_deltat(strval, &val));
    free(strval);
    return val;
}

/* Look up an absolute time within this database's dbmodules section.  The time
 * is expressed in the profile as an interval relative to the current time. */
static krb5_timestamp
get_time(testhandle h, const char *s1, const char *s2, const char *s3)
{
    char *strval = get_string(h, s1, s2, s3);
    krb5_deltat val;

    if (strval == NULL)
        return 0;
    check(krb5_string_to_deltat(strval, &val));
    free(strval);
    return val + time(NULL);
}

/* Initialize kb_out with a key of type etype, using a hash of kvno, etype,
 * salttype, and princstr for the key bytes. */
static void
make_keyblock(krb5_kvno kvno, krb5_enctype etype, int32_t salttype,
              const char *princstr, const krb5_data *realm,
              krb5_keyblock *kb_out)
{
    size_t keybytes, keylength, pos, n;
    char *hashstr;
    krb5_data d, rndin;
    krb5_checksum cksum;

    check(krb5_c_keylengths(NULL, etype, &keybytes, &keylength));
    alloc_data(&rndin, keybytes);

    /* Hash the kvno, enctype, salt type, and principal name together. */
    if (asprintf(&hashstr, "%d %d %d %s %.*s", (int)kvno, (int)etype,
                 (int)salttype, princstr, (int)realm->length, realm->data) < 0)
        abort();
    d = string2data(hashstr);
    check(krb5_c_make_checksum(NULL, CKSUMTYPE_SHA1, NULL, 0, &d, &cksum));

    /* Make the appropriate number of input bytes from the hash result. */
    for (pos = 0; pos < keybytes; pos += n) {
        n = (cksum.length < keybytes - pos) ? cksum.length : keybytes - pos;
        memcpy(rndin.data + pos, cksum.contents, n);
    }

    kb_out->enctype = etype;
    kb_out->length = keylength;
    kb_out->contents = ealloc(keylength);
    check(krb5_c_random_to_key(NULL, etype, &rndin, kb_out));
    free(cksum.contents);
    free(rndin.data);
    free(hashstr);
}

/* Return key data for the given key/salt tuple strings, using hashes of the
 * enctypes, salts, and princstr for the key contents. */
static void
make_keys(char **strings, const char *princstr, const krb5_data *realm,
          krb5_db_entry *ent)
{
    krb5_key_data *key_data, *kd;
    krb5_keyblock kb;
    int32_t *ks_list_sizes, nstrings, nkeys, i, j;
    krb5_key_salt_tuple **ks_lists, *ks;
    krb5_kvno *kvnos;
    char *s;

    for (nstrings = 0; strings[nstrings] != NULL; nstrings++);
    ks_lists = ealloc(nstrings * sizeof(*ks_lists));
    ks_list_sizes = ealloc(nstrings * sizeof(*ks_list_sizes));
    kvnos = ealloc(nstrings * sizeof(*kvnos));

    /* Convert each string into a key/salt tuple list and count the total
     * number of key data structures needed. */
    nkeys = 0;
    for (i = 0; i < nstrings; i++) {
        s = strings[i];
        /* Read a leading kvno if present; otherwise assume kvno 1. */
        if (isdigit(*s)) {
            kvnos[i] = strtol(s, &s, 10);
            while (isspace(*s))
                s++;
        } else {
            kvnos[i] = 1;
        }
        check(krb5_string_to_keysalts(s, NULL, NULL, FALSE, &ks_lists[i],
                                      &ks_list_sizes[i]));
        nkeys += ks_list_sizes[i];
    }

    /* Turn each key/salt tuple into a key data entry. */
    kd = key_data = ealloc(nkeys * sizeof(*kd));
    for (i = 0; i < nstrings; i++) {
        ks = ks_lists[i];
        for (j = 0; j < ks_list_sizes[i]; j++) {
            make_keyblock(kvnos[i], ks[j].ks_enctype, ks[j].ks_salttype,
                          princstr, realm, &kb);
            kd->key_data_ver = 2;
            kd->key_data_kvno = kvnos[i];
            kd->key_data_type[0] = ks[j].ks_enctype;
            kd->key_data_length[0] = kb.length;
            kd->key_data_contents[0] = kb.contents;
            kd->key_data_type[1] = ks[j].ks_salttype;
            kd++;
        }
    }

    for (i = 0; i < nstrings; i++)
        free(ks_lists[i]);
    free(ks_lists);
    free(ks_list_sizes);
    free(kvnos);
    ent->key_data = key_data;
    ent->n_key_data = nkeys;
}

static void
make_strings(char **stringattrs, krb5_db_entry *ent)
{
    struct k5buf buf;
    char **p;
    const char *str, *sep;
    krb5_tl_data *tl;

    k5_buf_init_dynamic(&buf);
    for (p = stringattrs; *p != NULL; p++) {
        str = *p;
        sep = strchr(str, ':');
        assert(sep != NULL);
        k5_buf_add_len(&buf, str, sep - str);
        k5_buf_add_len(&buf, "\0", 1);
        k5_buf_add_len(&buf, sep + 1, strlen(sep + 1) + 1);
    }
    assert(buf.data != NULL);

    tl = ealloc(sizeof(*ent->tl_data));
    tl->tl_data_next = NULL;
    tl->tl_data_type = KRB5_TL_STRING_ATTRS;
    tl->tl_data_length = buf.len;
    tl->tl_data_contents = buf.data;
    ent->tl_data = tl;
}

static krb5_error_code
test_init()
{
    return 0;
}

static krb5_error_code
test_cleanup()
{
    return 0;
}

static krb5_error_code
test_open(krb5_context context, char *conf_section, char **db_args, int mode)
{
    testhandle h;

    h = ealloc(sizeof(*h));
    h->profile = context->profile;
    h->section = estrdup(conf_section);
    context->dal_handle->db_context = h;
    return 0;
}

static krb5_error_code
test_close(krb5_context context)
{
    testhandle h = context->dal_handle->db_context;

    free(h->section);
    free(h);
    return 0;
}

/* Return the principal name krbtgt/tgs_realm@our_realm. */
static krb5_principal
tgtname(krb5_context context, const krb5_data *tgs_realm,
        const krb5_data *our_realm)
{
    krb5_principal princ;

    check(krb5_build_principal_ext(context, &princ,
                                   our_realm->length, our_realm->data,
                                   KRB5_TGS_NAME_SIZE, KRB5_TGS_NAME,
                                   tgs_realm->length, tgs_realm->data, 0));
    princ->type = KRB5_NT_SRV_INST;
    return princ;
}

/* Return true if search_for is within context's default realm or is an
 * incoming cross-realm TGS name. */
static krb5_boolean
request_for_us(krb5_context context, krb5_const_principal search_for)
{
    char *defrealm;
    krb5_data realm;
    krb5_boolean for_us;
    krb5_principal local_tgs;

    check(krb5_get_default_realm(context, &defrealm));
    realm = string2data(defrealm);
    local_tgs = tgtname(context, &realm, &realm);
    krb5_free_default_realm(context, defrealm);

    for_us = krb5_realm_compare(context, local_tgs, search_for) ||
        krb5_principal_compare_any_realm(context, local_tgs, search_for);
    krb5_free_principal(context, local_tgs);
    return for_us;
}

static krb5_error_code
test_get_principal(krb5_context context, krb5_const_principal search_for,
                   unsigned int flags, krb5_db_entry **entry)
{
    krb5_error_code ret;
    krb5_principal princ = NULL, tgtprinc;
    krb5_principal_data empty_princ = { KV5M_PRINCIPAL };
    testhandle h = context->dal_handle->db_context;
    char *search_name = NULL, *canon = NULL, *flagstr;
    char **names, **key_strings, **stringattrs;
    const char *ename;
    krb5_db_entry *ent;

    *entry = NULL;

    if (!request_for_us(context, search_for))
        return KRB5_KDB_NOENTRY;

    check(krb5_unparse_name_flags(context, search_for,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                  &search_name));
    canon = get_string(h, "alias", search_name, NULL);
    if (canon != NULL) {
        check(krb5_parse_name(context, canon, &princ));
        if (!krb5_realm_compare(context, search_for, princ)) {
            /* Out of realm */
            if ((flags & KRB5_KDB_FLAG_CLIENT) &&
                (flags & KRB5_KDB_FLAG_REFERRAL_OK)) {
                /* Return a client referral by creating an entry with only the
                 * principal set. */
                *entry = ealloc(sizeof(**entry));
                (*entry)->princ = princ;
                princ = NULL;
                ret = 0;
                goto cleanup;
            } else if (flags & KRB5_KDB_FLAG_REFERRAL_OK) {
                /* Generate a server referral by looking up the TGT for the
                 * canonical name's realm. */
                tgtprinc = tgtname(context, &princ->realm, &search_for->realm);
                krb5_free_principal(context, princ);
                princ = tgtprinc;

                krb5_free_unparsed_name(context, search_name);
                check(krb5_unparse_name_flags(context, princ,
                                              KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                              &search_name));
                ename = search_name;
            } else {
                ret = KRB5_KDB_NOENTRY;
                goto cleanup;
            }
        } else {
            ename = canon;
        }
    } else {
        check(krb5_copy_principal(context, search_for, &princ));
        ename = search_name;
    }

    /* Check that the entry exists. */
    set_names(h, "princs", ename, NULL);
    ret = profile_get_relation_names(h->profile, h->names, &names);
    if (ret == PROF_NO_RELATION) {
        ret = KRB5_KDB_NOENTRY;
        goto cleanup;
    }
    profile_free_list(names);

    /* No error exits after this point. */

    ent = ealloc(sizeof(*ent));
    ent->princ = princ;
    princ = NULL;

    flagstr = get_string(h, "princs", ename, "flags");
    if (flagstr != NULL) {
        check(krb5_flagspec_to_mask(flagstr, &ent->attributes,
                                    &ent->attributes));
    }
    free(flagstr);

    ent->max_life = get_duration(h, "princs", ename, "maxlife");
    ent->max_renewable_life = get_duration(h, "princs", ename, "maxrenewlife");
    ent->expiration = get_time(h, "princs", ename, "expiration");
    ent->pw_expiration = get_time(h, "princs", ename, "pwexpiration");

    /* Leave last_success, last_failed, fail_auth_count zeroed. */
    /* Leave e_data empty. */

    set_names(h, "princs", ename, "keys");
    ret = profile_get_values(h->profile, h->names, &key_strings);
    if (ret != PROF_NO_RELATION) {
        make_keys(key_strings, ename, &search_for->realm, ent);
        profile_free_list(key_strings);
    }

    set_names(h, "princs", ename, "strings");
    ret = profile_get_values(h->profile, h->names, &stringattrs);
    if (ret != PROF_NO_RELATION) {
        make_strings(stringattrs, ent);
        profile_free_list(stringattrs);
    }

    /* We must include mod-princ data or kadm5_get_principal() won't work and
     * we can't extract keys with kadmin.local. */
    check(krb5_dbe_update_mod_princ_data(context, ent, 0, &empty_princ));

    *entry = ent;
    ret = 0;

cleanup:
    krb5_free_unparsed_name(context, search_name);
    krb5_free_principal(context, princ);
    free(canon);
    return ret;
}

static void
lookup_princ_by_cert(krb5_context context, const krb5_data *client_cert,
                     krb5_principal *princ)
{
    krb5_error_code ret;
    char *cert_princ_name;

    /* The test client sends a principal string instead of a cert. */
    cert_princ_name = k5memdup0(client_cert->data, client_cert->length, &ret);
    check(ret);

    check(krb5_parse_name_flags(context, cert_princ_name,
                                KRB5_PRINCIPAL_PARSE_ENTERPRISE, princ));
    free(cert_princ_name);
}

static krb5_error_code
test_get_s4u_x509_principal(krb5_context context, const krb5_data *client_cert,
                            krb5_const_principal princ, unsigned int flags,
                            krb5_db_entry **entry)
{
    krb5_error_code ret;
    krb5_principal cert_princ, canon_princ;
    testhandle h = context->dal_handle->db_context;
    krb5_boolean match;
    char *canon, *princ_name;

    lookup_princ_by_cert(context, client_cert, &cert_princ);

    ret = test_get_principal(context, cert_princ, flags, entry);
    krb5_free_principal(context, cert_princ);
    if (ret || (flags & KRB5_KDB_FLAG_REFERRAL_OK))
        return ret;

    if (!krb5_realm_compare(context, princ, (*entry)->princ))
        abort();

    if (princ->length == 0 ||
        krb5_principal_compare(context, princ, (*entry)->princ))
        return 0;

    match = FALSE;
    check(krb5_unparse_name_flags(context, princ,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                  &princ_name));
    canon = get_string(h, "alias", princ_name, NULL);
    krb5_free_unparsed_name(context, princ_name);
    if (canon != NULL) {
        check(krb5_parse_name(context, canon, &canon_princ));
        match = krb5_principal_compare(context, canon_princ, (*entry)->princ);
        krb5_free_principal(context, canon_princ);
    }

    free(canon);
    return match ? 0 : KRB5KDC_ERR_CLIENT_NAME_MISMATCH;
}

static krb5_error_code
test_fetch_master_key(krb5_context context, krb5_principal mname,
                      krb5_keyblock *key_out, krb5_kvno *kvno_out,
                      char *db_args)
{
    memset(key_out, 0, sizeof(*key_out));
    *kvno_out = 0;
    return 0;
}

static krb5_error_code
test_fetch_master_key_list(krb5_context context, krb5_principal mname,
                           const krb5_keyblock *key,
                           krb5_keylist_node **mkeys_out)
{
    /* krb5_dbe_get_mkvno() returns an error if we produce NULL, so return an
     * empty node to make kadm5_get_principal() work. */
    *mkeys_out = ealloc(sizeof(**mkeys_out));
    return 0;
}

static krb5_error_code
test_decrypt_key_data(krb5_context context, const krb5_keyblock *mkey,
                      const krb5_key_data *kd, krb5_keyblock *key_out,
                      krb5_keysalt *salt_out)
{
    key_out->magic = KV5M_KEYBLOCK;
    key_out->enctype = kd->key_data_type[0];
    key_out->length = kd->key_data_length[0];
    key_out->contents = ealloc(key_out->length);
    memcpy(key_out->contents, kd->key_data_contents[0], key_out->length);
    if (salt_out != NULL) {
        salt_out->type = (kd->key_data_ver > 1) ? kd->key_data_type[1] :
            KRB5_KDB_SALTTYPE_NORMAL;
        salt_out->data = empty_data();
    }
    return 0;
}

static krb5_error_code
test_encrypt_key_data(krb5_context context, const krb5_keyblock *mkey,
                      const krb5_keyblock *key, const krb5_keysalt *salt,
                      int kvno, krb5_key_data *kd_out)
{
    memset(kd_out, 0, sizeof(*kd_out));
    kd_out->key_data_ver = 2;
    kd_out->key_data_kvno = kvno;
    kd_out->key_data_type[0] = key->enctype;
    kd_out->key_data_length[0] = key->length;
    kd_out->key_data_contents[0] = ealloc(key->length);
    memcpy(kd_out->key_data_contents[0], key->contents, key->length);
    kd_out->key_data_type[1] = (salt != NULL) ? salt->type :
        KRB5_KDB_SALTTYPE_NORMAL;
    return 0;
}

static void
change_auth_indicators(krb5_context context, krb5_data ***auth_indicators)
{
    krb5_data **inds, d;
    int i, val;

    /* If we see an auth indicator "dbincrX", replace the whole indicator list
     * with "dbincr{X+1}". */
    inds = *auth_indicators;
    for (i = 0; inds != NULL && inds[i] != NULL; i++) {
        if (inds[i]->length == 7 && memcmp(inds[i]->data, "dbincr", 6) == 0) {
            val = inds[i]->data[6];
            k5_free_data_ptr_list(inds);
            inds = ealloc(2 * sizeof(*inds));
            d = string2data("dbincr0");
            check(krb5_copy_data(context, &d, &inds[0]));
            inds[0]->data[6] = val + 1;
            inds[1] = NULL;
            *auth_indicators = inds;
            break;
        }
    }
}

static krb5_error_code
test_issue_pac(krb5_context context, unsigned int flags, krb5_db_entry *client,
               krb5_keyblock *replaced_reply_key, krb5_db_entry *server,
               krb5_db_entry *krb5tgt, krb5_timestamp authtime,
               krb5_pac old_pac, krb5_pac new_pac,
               krb5_data ***auth_indicators)
{
    krb5_data data = empty_data();
    krb5_boolean found_logon_info = FALSE;
    krb5_ui_4 *types = NULL;
    size_t num_buffers = 0, i;

    change_auth_indicators(context, auth_indicators);

    if (old_pac == NULL ||
        (client != NULL && (flags & KRB5_KDB_FLAG_PROTOCOL_TRANSITION))) {
        /* Generating an initial PAC. */
        assert(client != NULL);
        data = string2data("fake");
        check(krb5_pac_add_buffer(context, new_pac, KRB5_PAC_LOGON_INFO,
                                  &data));

        if (replaced_reply_key != NULL) {
            /* Add a fake PAC_CREDENTIALS_INFO buffer so we can test whether
             * this parameter was set. */
            data = string2data("fake credinfo");
            check(krb5_pac_add_buffer(context, new_pac,
                                      KRB5_PAC_CREDENTIALS_INFO, &data));
        }
        return 0;
    } else {
        /* Field copying - my favorite! */
        if (old_pac != NULL)
            check(krb5_pac_get_types(context, old_pac, &num_buffers, &types));

        for (i = 0; i < num_buffers; i++) {
            /* Skip buffer types handled by KDC. */
            if (types[i] == KRB5_PAC_SERVER_CHECKSUM ||
                types[i] == KRB5_PAC_PRIVSVR_CHECKSUM ||
                types[i] == KRB5_PAC_TICKET_CHECKSUM ||
                types[i] == KRB5_PAC_CLIENT_INFO ||
                types[i] == KRB5_PAC_DELEGATION_INFO)
                continue;

            check(krb5_pac_get_buffer(context, old_pac, types[i], &data));

            if (types[i] == KRB5_PAC_LOGON_INFO) {
                found_logon_info = TRUE;
                assert(data_eq_string(data, "fake"));
            }

            check(krb5_pac_add_buffer(context, new_pac, types[i], &data));
            krb5_free_data_contents(context, &data);
        }

        if (old_pac != NULL)
            assert(found_logon_info);

        free(types);
    }

    return 0;
}

static krb5_boolean
match_in_table(krb5_context context, const char *table, const char *sprinc,
               const char *tprinc)
{
    testhandle h = context->dal_handle->db_context;
    krb5_error_code ret;
    char **values, **v;
    krb5_boolean found = FALSE;

    set_names(h, table, sprinc, NULL);
    ret = profile_get_values(h->profile, h->names, &values);
    assert(ret == 0 || ret == PROF_NO_RELATION);
    if (ret)
        return FALSE;
    for (v = values; *v != NULL; v++) {
        if (tprinc == NULL || strcmp(*v, tprinc) == 0) {
            found = TRUE;
            break;
        }
    }
    profile_free_list(values);
    return found;
}

static krb5_error_code
test_check_allowed_to_delegate(krb5_context context,
                               krb5_const_principal client,
                               const krb5_db_entry *server,
                               krb5_const_principal proxy)
{
    char *sprinc, *tprinc = NULL;
    krb5_boolean found = FALSE;

    check(krb5_unparse_name_flags(context, server->princ,
                                  KRB5_PRINCIPAL_UNPARSE_NO_REALM, &sprinc));
    if (proxy != NULL) {
        check(krb5_unparse_name_flags(context, proxy,
                                      KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                      &tprinc));
    }
    found = match_in_table(context, "delegation", sprinc, tprinc);
    krb5_free_unparsed_name(context, sprinc);
    krb5_free_unparsed_name(context, tprinc);
    return found ? 0 : KRB5KDC_ERR_BADOPTION;
}

static krb5_error_code
test_allowed_to_delegate_from(krb5_context context,
                              krb5_const_principal client,
                              krb5_const_principal server,
                              krb5_pac server_pac,
                              const krb5_db_entry *proxy)
{
    char *proxy_princ, *server_princ, *pac_client_princ, *client_princ;
    krb5_boolean found = FALSE;

    assert(server_pac != NULL);

    check(krb5_unparse_name(context, proxy->princ, &proxy_princ));
    check(krb5_unparse_name(context, server, &server_princ));
    check(krb5_unparse_name(context, client, &client_princ));

    check(krb5_pac_get_client_info(context, server_pac, NULL,
                                   &pac_client_princ));

    /* Skip realm portion if not present in PAC. */
    assert(strncmp(pac_client_princ, server_princ,
                   strlen(pac_client_princ)) == 0);

    free(pac_client_princ);

    found = match_in_table(context, "rbcd", proxy_princ, server_princ);
    krb5_free_unparsed_name(context, proxy_princ);
    krb5_free_unparsed_name(context, server_princ);
    krb5_free_unparsed_name(context, client_princ);
    return found ? 0 : KRB5KDC_ERR_BADOPTION;
}

kdb_vftabl PLUGIN_SYMBOL_NAME(krb5_test, kdb_function_table) = {
    KRB5_KDB_DAL_MAJOR_VERSION,             /* major version number */
    0,                                      /* minor version number */
    test_init,
    test_cleanup,
    test_open,
    test_close,
    NULL, /* create */
    NULL, /* destroy */
    NULL, /* get_age */
    NULL, /* lock */
    NULL, /* unlock */
    test_get_principal,
    NULL, /* put_principal */
    NULL, /* delete_principal */
    NULL, /* rename_principal */
    NULL, /* iterate */
    NULL, /* create_policy */
    NULL, /* get_policy */
    NULL, /* put_policy */
    NULL, /* iter_policy */
    NULL, /* delete_policy */
    test_fetch_master_key,
    test_fetch_master_key_list,
    NULL, /* store_master_key_list */
    NULL, /* dbe_search_enctype */
    NULL, /* change_pwd */
    NULL, /* promote_db */
    test_decrypt_key_data,
    test_encrypt_key_data,
    NULL, /* check_transited_realms */
    NULL, /* check_policy_as */
    NULL, /* check_policy_tgs */
    NULL, /* audit_as_req */
    NULL, /* refresh_config */
    test_check_allowed_to_delegate,
    NULL, /* free_principal_e_data */
    test_get_s4u_x509_principal,
    test_allowed_to_delegate_from,
    test_issue_pac,
};
