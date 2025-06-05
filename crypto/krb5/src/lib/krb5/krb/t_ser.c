/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_ser.c - Test serialization */
/*
 * Copyright 1995, 2019 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-int.h"
#include "com_err.h"
#include "auth_con.h"

#include <ctype.h>

static const char stuff[]="You can't take a pointer to a function and convert \
it to a pointer to char; ANSI doesn't say it'll work, and in fact on the HPPA \
you can lose some bits of the function pointer, and get a pointer that you \
can't safely dereference.  This test file used to make this mistake, often.";

static void
check(krb5_error_code code)
{
    if (code != 0) {
        com_err("t_ser", code, NULL);
        abort();
    }
}

static void *
ealloc(size_t size)
{
    void *ptr = calloc(1, size);

    if (ptr == NULL)
        abort();
    return ptr;
}

static void
ser_context(krb5_context ctx)
{
    uint8_t *erep, *erep2, *bp;
    size_t elen = 0, elen2 = 0, blen;
    krb5_context ctx2;

    check(k5_size_context(ctx, &elen));
    erep = ealloc(elen);

    bp = erep;
    blen = elen;
    check(k5_externalize_context(ctx, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    bp = erep;
    blen = elen;
    check(k5_internalize_context(&ctx2, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    check(k5_size_context(ctx2, &elen2));
    assert(elen2 == elen);
    erep2 = ealloc(elen2);

    bp = erep2;
    blen = elen2;
    check(k5_externalize_context(ctx2, &bp, &blen));
    assert(bp == erep2 + elen2 && blen == 0);
    assert(memcmp(erep, erep2, elen) == 0);

    free(erep);
    free(erep2);
    krb5_free_context(ctx2);
}

static void
ser_auth_context(krb5_auth_context actx)
{
    uint8_t *erep, *erep2, *bp;
    size_t elen = 0, elen2 = 0, blen;
    krb5_auth_context actx2;

    check(k5_size_auth_context(actx, &elen));
    erep = ealloc(elen);

    bp = erep;
    blen = elen;
    check(k5_externalize_auth_context(actx, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    bp = erep;
    blen = elen;
    check(k5_internalize_auth_context(&actx2, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    check(k5_size_auth_context(actx2, &elen2));
    assert(elen2 == elen);
    erep2 = ealloc(elen2);

    bp = erep2;
    blen = elen2;
    check(k5_externalize_auth_context(actx2, &bp, &blen));
    assert(bp == erep2 + elen2 && blen == 0);
    assert(memcmp(erep, erep2, elen) == 0);

    free(erep);
    free(erep2);
    krb5_auth_con_free(NULL, actx2);
}

static void
ser_principal(krb5_principal princ)
{
    uint8_t *erep, *erep2, *bp;
    size_t elen = 0, elen2 = 0, blen;
    krb5_principal princ2;

    check(k5_size_principal(princ, &elen));
    erep = ealloc(elen);

    bp = erep;
    blen = elen;
    check(k5_externalize_principal(princ, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    bp = erep;
    blen = elen;
    check(k5_internalize_principal(&princ2, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    check(k5_size_principal(princ2, &elen2));
    assert(elen2 == elen);
    erep2 = ealloc(elen2);

    bp = erep2;
    blen = elen2;
    check(k5_externalize_principal(princ2, &bp, &blen));
    assert(bp == erep2 + elen2 && blen == 0);
    assert(memcmp(erep, erep2, elen) == 0);

    free(erep);
    free(erep2);
    krb5_free_principal(NULL, princ2);
}

static void
ser_checksum(krb5_checksum *cksum)
{
    uint8_t *erep, *erep2, *bp;
    size_t elen = 0, elen2 = 0, blen;
    krb5_checksum *cksum2;

    check(k5_size_checksum(cksum, &elen));
    erep = ealloc(elen);

    bp = erep;
    blen = elen;
    check(k5_externalize_checksum(cksum, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    bp = erep;
    blen = elen;
    check(k5_internalize_checksum(&cksum2, &bp, &blen));
    assert(bp == erep + elen && blen == 0);

    check(k5_size_checksum(cksum2, &elen2));
    assert(elen2 == elen);
    erep2 = ealloc(elen2);

    bp = erep2;
    blen = elen2;
    check(k5_externalize_checksum(cksum2, &bp, &blen));
    assert(bp == erep2 + elen2 && blen == 0);
    assert(memcmp(erep, erep2, elen) == 0);

    free(erep);
    free(erep2);
    krb5_free_checksum(NULL, cksum2);
}

static void
ser_context_test()
{
    krb5_context context;
    profile_t sprofile;

    check(krb5_init_context(&context));

    sprofile = context->profile;
    context->profile = NULL;
    ser_context(context);

    context->profile = sprofile;
    ser_context(context);

    check(krb5_set_default_realm(context, "this.is.a.test"));
    ser_context(context);

    krb5_free_context(context);
}

static void
ser_acontext_test()
{
    krb5_auth_context   actx;
    krb5_address        local_address;
    krb5_address        remote_address;
    krb5_octet          laddr_bytes[16];
    krb5_octet          raddr_bytes[16];
    krb5_keyblock       ukeyblock;
    krb5_octet          keydata[8];
    krb5_authenticator  aent;
    char                clname[128];
    krb5_authdata       *adatalist[3];
    krb5_authdata       adataent;

    check(krb5_auth_con_init(NULL, &actx));
    ser_auth_context(actx);

    memset(&local_address, 0, sizeof(local_address));
    memset(&remote_address, 0, sizeof(remote_address));
    memset(laddr_bytes, 0, sizeof(laddr_bytes));
    memset(raddr_bytes, 0, sizeof(raddr_bytes));
    local_address.addrtype = ADDRTYPE_INET;
    local_address.length = sizeof(laddr_bytes);
    local_address.contents = laddr_bytes;
    laddr_bytes[0] = 6;
    laddr_bytes[1] = 2;
    laddr_bytes[2] = 69;
    laddr_bytes[3] = 16;
    laddr_bytes[4] = 1;
    laddr_bytes[5] = 0;
    laddr_bytes[6] = 0;
    laddr_bytes[7] = 127;
    remote_address.addrtype = ADDRTYPE_INET;
    remote_address.length = sizeof(raddr_bytes);
    remote_address.contents = raddr_bytes;
    raddr_bytes[0] = 6;
    raddr_bytes[1] = 2;
    raddr_bytes[2] = 70;
    raddr_bytes[3] = 16;
    raddr_bytes[4] = 1;
    raddr_bytes[5] = 0;
    raddr_bytes[6] = 0;
    raddr_bytes[7] = 127;
    check(krb5_auth_con_setaddrs(NULL, actx, &local_address, &remote_address));
    check(krb5_auth_con_setports(NULL, actx, &local_address, &remote_address));
    ser_auth_context(actx);

    memset(&ukeyblock, 0, sizeof(ukeyblock));
    memset(keydata, 0, sizeof(keydata));
    ukeyblock.enctype = ENCTYPE_AES128_CTS_HMAC_SHA256_128;
    ukeyblock.length = sizeof(keydata);
    ukeyblock.contents = keydata;
    keydata[0] = 0xde;
    keydata[1] = 0xad;
    keydata[2] = 0xbe;
    keydata[3] = 0xef;
    keydata[4] = 0xfe;
    keydata[5] = 0xed;
    keydata[6] = 0xf0;
    keydata[7] = 0xd;
    check(krb5_auth_con_setuseruserkey(NULL, actx, &ukeyblock));
    ser_auth_context(actx);

    check(krb5_auth_con_initivector(NULL, actx));
    ser_auth_context(actx);

    memset(&aent, 0, sizeof(aent));
    aent.magic = KV5M_AUTHENTICATOR;
    snprintf(clname, sizeof(clname),
             "help/me/%d@this.is.a.test", (int)getpid());
    actx->authentp = &aent;
    check(krb5_parse_name(NULL, clname, &aent.client));
    ser_auth_context(actx);

    adataent.magic = KV5M_AUTHDATA;
    adataent.ad_type = 123;
    adataent.length = 128;
    adataent.contents = (uint8_t *)stuff;
    adatalist[0] = &adataent;
    adatalist[1] = &adataent;
    adatalist[2] = NULL;
    aent.authorization_data = adatalist;
    ser_auth_context(actx);

    krb5_free_principal(NULL, aent.client);
    actx->authentp = NULL;
    krb5_auth_con_free(NULL, actx);
}

static void
ser_princ_test()
{
    krb5_principal      princ;
    char                pname[1024];

    snprintf(pname, sizeof(pname),
             "the/quick/brown/fox/jumped/over/the/lazy/dog/%d@this.is.a.test",
             (int) getpid());
    check(krb5_parse_name(NULL, pname, &princ));
    ser_principal(princ);
    krb5_free_principal(NULL, princ);
}

static void
ser_cksum_test()
{
    krb5_checksum       checksum;
    krb5_octet          ckdata[24];

    memset(&checksum, 0, sizeof(krb5_checksum));
    checksum.magic = KV5M_CHECKSUM;
    ser_checksum(&checksum);

    checksum.checksum_type = 123;
    checksum.length = sizeof(ckdata);
    checksum.contents = ckdata;
    memcpy(ckdata, &stuff, sizeof(ckdata));
    ser_checksum(&checksum);
}

int
main(int argc, char **argv)
{
    ser_context_test();
    ser_acontext_test();
    ser_princ_test();
    ser_cksum_test();
    return 0;
}
