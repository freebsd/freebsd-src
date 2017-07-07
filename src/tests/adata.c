/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/adata.c - Test harness for KDC authorization data */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
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
 * Usage: ./adata [-c ccname] [-p clientprinc] serviceprinc
 *            [ad-type ad-contents ...]
 *
 * This program acquires credentials for the specified service principal, using
 * the specified or default ccache, possibly including requested authdata.  The
 * resulting ticket is decrypted using the default keytab, and the authdata in
 * the ticket are displayed to stdout.
 *
 * In the requested authdata types, the type may be prefixed with '?' for an
 * AD-IF-RELEVANT container, '!' for an AD-MANDATORY-FOR-KDC container, or '^'
 * for an AD-KDC-ISSUED container checksummed with a random AES256 key.
 * Multiple prefixes may be specified for nested container.
 *
 * In the output, authdata containers will be flattened and displayed with the
 * above prefixes or '+' for an AD-CAMMAC container.  AD-KDC-ISSUED and
 * AD-CAMMAC containers will be verified with the appropriate key.  Nested
 * containers only display the prefix for the innermost container.
 */

#include <k5-int.h>
#include <ctype.h>

static krb5_context ctx;

static void display_authdata_list(krb5_authdata **list, krb5_keyblock *skey,
                                  krb5_keyblock *tktkey, char prefix_byte);

static void
check(krb5_error_code code)
{
    const char *errmsg;

    if (code) {
        errmsg = krb5_get_error_message(ctx, code);
        fprintf(stderr, "%s\n", errmsg);
        krb5_free_error_message(ctx, errmsg);
        exit(1);
    }
}

static krb5_authdatatype
get_type_for_prefix(int prefix_byte)
{
    if (prefix_byte == '?')
        return KRB5_AUTHDATA_IF_RELEVANT;
    if (prefix_byte == '!')
        return KRB5_AUTHDATA_MANDATORY_FOR_KDC;
    if (prefix_byte == '^')
        return KRB5_AUTHDATA_KDC_ISSUED;
    if (prefix_byte == '+')
        return KRB5_AUTHDATA_CAMMAC;
    abort();
}

static int
get_prefix_byte(krb5_authdata *ad)
{
    if (ad->ad_type == KRB5_AUTHDATA_IF_RELEVANT)
        return '?';
    if (ad->ad_type == KRB5_AUTHDATA_MANDATORY_FOR_KDC)
        return '!';
    if (ad->ad_type == KRB5_AUTHDATA_KDC_ISSUED)
        return '^';
    if (ad->ad_type == KRB5_AUTHDATA_CAMMAC)
        return '+';
    abort();
}

/* Construct a container of type ad_type for the single authdata element
 * content.  For KDC-ISSUED containers, use a random checksum key. */
static krb5_authdata *
make_container(krb5_authdatatype ad_type, krb5_authdata *content)
{
    krb5_authdata *list[2], **enclist, *ad;
    krb5_keyblock kb;

    list[0] = content;
    list[1] = NULL;

    if (ad_type == KRB5_AUTHDATA_KDC_ISSUED) {
        check(krb5_c_make_random_key(ctx, ENCTYPE_AES256_CTS_HMAC_SHA1_96,
                                     &kb));
        check(krb5_make_authdata_kdc_issued(ctx, &kb, NULL, list, &enclist));
        krb5_free_keyblock_contents(ctx, &kb);
    } else {
        check(krb5_encode_authdata_container(ctx, ad_type, list, &enclist));
    }

    /* Grab the first element from the encoded list and free the array. */
    ad = enclist[0];
    free(enclist);
    return ad;
}

/* Parse typestr and contents into an authdata element. */
static krb5_authdata *
make_authdata(const char *typestr, const char *contents)
{
    krb5_authdata *inner_ad, *ad;

    if (*typestr == '?' || *typestr == '!' || *typestr == '^') {
        inner_ad = make_authdata(typestr + 1, contents);
        ad = make_container(get_type_for_prefix(*typestr), inner_ad);
        free(inner_ad->contents);
        free(inner_ad);
        return ad;
    }

    ad = malloc(sizeof(*ad));
    assert(ad != NULL);
    ad->magic = KV5M_AUTHDATA;
    ad->ad_type = atoi(typestr);
    ad->length = strlen(contents);
    ad->contents = (unsigned char *)strdup(contents);
    assert(ad->contents != NULL);
    return ad;
}

static krb5_authdata **
get_container_contents(krb5_authdata *ad, krb5_keyblock *skey,
                       krb5_keyblock *tktkey)
{
    krb5_authdata **inner_ad;

    if (ad->ad_type == KRB5_AUTHDATA_KDC_ISSUED)
        check(krb5_verify_authdata_kdc_issued(ctx, skey, ad, NULL, &inner_ad));
    else if (ad->ad_type == KRB5_AUTHDATA_CAMMAC)
        check(k5_unwrap_cammac_svc(ctx, ad, tktkey, &inner_ad));
    else
        check(krb5_decode_authdata_container(ctx, ad->ad_type, ad, &inner_ad));
    return inner_ad;
}

/* Decode and display authentication indicator authdata. */
static void
display_auth_indicator(krb5_authdata *ad)
{
    krb5_data **strs = NULL, **p;

    check(k5_authind_decode(ad, &strs));
    assert(strs != NULL);

    printf("[");
    for (p = strs; *p != NULL; p++) {
        printf("%.*s", (int)(*p)->length, (*p)->data);
        if (*(p + 1) != NULL)
            printf(", ");
    }
    printf("]");
    k5_free_data_ptr_list(strs);
}

/* Display ad as either a hex dump or ASCII text. */
static void
display_binary_or_ascii(krb5_authdata *ad)
{
    krb5_boolean binary = FALSE;
    unsigned char *p;

    for (p = ad->contents; p < ad->contents + ad->length; p++) {
        if (!isascii(*p) || !isprint(*p))
            binary = TRUE;
    }
    if (binary) {
        for (p = ad->contents; p < ad->contents + ad->length; p++)
            printf("%02X", *p);
    } else {
        printf("%.*s", (int)ad->length, ad->contents);
    }
}

/* Display the contents of an authdata element, prefixed by prefix_byte.  skey
 * must be the ticket session key. */
static void
display_authdata(krb5_authdata *ad, krb5_keyblock *skey, krb5_keyblock *tktkey,
                 int prefix_byte)
{
    krb5_authdata **inner_ad;

    if (ad->ad_type == KRB5_AUTHDATA_IF_RELEVANT ||
        ad->ad_type == KRB5_AUTHDATA_MANDATORY_FOR_KDC ||
        ad->ad_type == KRB5_AUTHDATA_KDC_ISSUED ||
        ad->ad_type == KRB5_AUTHDATA_CAMMAC) {
        /* Decode and display the contents. */
        inner_ad = get_container_contents(ad, skey, tktkey);
        display_authdata_list(inner_ad, skey, tktkey, get_prefix_byte(ad));
        krb5_free_authdata(ctx, inner_ad);
        return;
    }

    printf("%c", prefix_byte);
    printf("%d: ", (int)ad->ad_type);

    if (ad->ad_type == KRB5_AUTHDATA_AUTH_INDICATOR)
        display_auth_indicator(ad);
    else
        display_binary_or_ascii(ad);
    printf("\n");
}

static void
display_authdata_list(krb5_authdata **list, krb5_keyblock *skey,
                      krb5_keyblock *tktkey, char prefix_byte)
{
    if (list == NULL)
        return;
    for (; *list != NULL; list++)
        display_authdata(*list, skey, tktkey, prefix_byte);
}

int
main(int argc, char **argv)
{
    const char *ccname = NULL, *clientname = NULL;
    krb5_principal client, server;
    krb5_ccache ccache;
    krb5_keytab keytab;
    krb5_creds in_creds, *creds;
    krb5_ticket *ticket;
    krb5_authdata **req_authdata = NULL, *ad;
    krb5_keytab_entry ktent;
    size_t count;
    int c;

    check(krb5_init_context(&ctx));

    while ((c = getopt(argc, argv, "+c:p:")) != -1) {
        switch (c) {
        case 'c':
            ccname = optarg;
            break;
        case 'p':
            clientname = optarg;
            break;
        default:
            abort();
        }
    }
    argv += optind;
    /* Parse arguments. */
    assert(*argv != NULL);
    check(krb5_parse_name(ctx, *argv++, &server));

    count = 0;
    for (; argv[0] != NULL && argv[1] != NULL; argv += 2) {
        ad = make_authdata(argv[0], argv[1]);
        req_authdata = realloc(req_authdata,
                               (count + 2) * sizeof(*req_authdata));
        assert(req_authdata != NULL);
        req_authdata[count++] = ad;
        req_authdata[count] = NULL;
    }
    assert(*argv == NULL);

    if (ccname != NULL)
        check(krb5_cc_resolve(ctx, ccname, &ccache));
    else
        check(krb5_cc_default(ctx, &ccache));

    if (clientname != NULL)
        check(krb5_parse_name(ctx, clientname, &client));
    else
        check(krb5_cc_get_principal(ctx, ccache, &client));

    memset(&in_creds, 0, sizeof(in_creds));
    in_creds.client = client;
    in_creds.server = server;
    in_creds.authdata = req_authdata;

    check(krb5_get_credentials(ctx, KRB5_GC_NO_STORE, ccache, &in_creds,
                               &creds));

    check(krb5_decode_ticket(&creds->ticket, &ticket));
    check(krb5_kt_default(ctx, &keytab));
    check(krb5_kt_get_entry(ctx, keytab, server, ticket->enc_part.kvno,
                            ticket->enc_part.enctype, &ktent));
    check(krb5_decrypt_tkt_part(ctx, &ktent.key, ticket));

    display_authdata_list(ticket->enc_part2->authorization_data,
                          ticket->enc_part2->session, &ktent.key, ' ');

    while (count > 0) {
        free(req_authdata[--count]->contents);
        free(req_authdata[count]);
    }
    free(req_authdata);
    krb5_free_keytab_entry_contents(ctx, &ktent);
    krb5_free_creds(ctx, creds);
    krb5_free_ticket(ctx, ticket);
    krb5_free_principal(ctx, client);
    krb5_free_principal(ctx, server);
    krb5_cc_close(ctx, ccache);
    krb5_kt_close(ctx, keytab);
    krb5_free_context(ctx);
    return 0;
}
