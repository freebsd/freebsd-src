/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/t_ser.c - Test serialization */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

/*
 * Dump an external representation.
 */
static void
print_erep(krb5_octet *erep, size_t elen)
{
    unsigned int i, j;

    for (i=0; i<elen; ) {
        printf("%08d: ", i);
        for (j=0; j<15; j++) {
            if ((i+j) < elen)
                printf("%02x ", erep[i+j]);
            else
                printf("-- ");
        }
        printf("\t");
        for (j=0; j<15; j++) {
            if ((i+j) < elen) {
                if (isprint(erep[i+j]) && (erep[i+j] != '\n'))
                    printf("%c", erep[i+j]);
                else
                    printf(".");
            }
            else
                printf("-");
        }
        printf("\n");
        i += 15;
    }
}

/*
 * Do a serialization test.
 */
static krb5_error_code
ser_data(int verbose, char *msg, krb5_pointer ctx, krb5_magic dtype)
{
    krb5_error_code     kret;
    krb5_context        ser_ctx;
    krb5_pointer        nctx;
    krb5_octet          *outrep, *ibuf, *outrep2;
    size_t              outlen, ilen, outlen2;

    /* Initialize context and initialize all Kerberos serializers */
    if ((kret = krb5_init_context(&ser_ctx))) {
        printf("Couldn't initialize krb5 library: %s\n",
               error_message(kret));
        exit(1);
    }
    krb5_ser_context_init(ser_ctx);
    krb5_ser_auth_context_init(ser_ctx);
    krb5_ser_ccache_init(ser_ctx);
    krb5_ser_rcache_init(ser_ctx);
    krb5_ser_keytab_init(ser_ctx);

    /* Externalize the data */
    kret = krb5_externalize_data(ser_ctx, ctx, &outrep, &outlen);
    if (!kret) {
        if (verbose) {
            printf("%s: externalized in %d bytes\n", msg, (int)outlen);
            print_erep(outrep, outlen);
        }

        /* Now attempt to re-constitute it */
        ibuf = outrep;
        ilen = outlen;
        kret = krb5_internalize_opaque(ser_ctx,
                                       dtype,
                                       (krb5_pointer *) &nctx,
                                       &ibuf,
                                       &ilen);
        if (!kret) {
            if (ilen)
                printf("%s: %d bytes left over after internalize\n",
                       msg, (int)ilen);
            /* Now attempt to re-externalize it */
            kret = krb5_externalize_data(ser_ctx, nctx, &outrep2, &outlen2);
            if (!kret) {
                /* Compare the results. */
                if ((outlen2 != outlen) ||
                    memcmp(outrep, outrep2, outlen)) {
                    printf("%s: comparison failed\n", msg);
                    print_erep(outrep2, outlen2);
                }
                else {
                    if (verbose)
                        printf("%s: compare succeeded\n", msg);
                }
                free(outrep2);
            }
            else
                printf("%s: second externalize returned %d\n", msg, kret);

            /* Free the data */
            switch (dtype) {
            case KV5M_CONTEXT:
                krb5_free_context((krb5_context) nctx);
                break;
            case KV5M_AUTH_CONTEXT:
                krb5_auth_con_free(ser_ctx, (krb5_auth_context) nctx);
                break;
            case KV5M_CCACHE:
                krb5_cc_close(ser_ctx, (krb5_ccache) nctx);
                break;
            case KV5M_RCACHE:
                krb5_rc_close(ser_ctx, (krb5_rcache) nctx);
                break;
            case KV5M_KEYTAB:
                krb5_kt_close(ser_ctx, (krb5_keytab) nctx);
                break;
            case KV5M_ENCRYPT_BLOCK:
                if (nctx) {
                    krb5_encrypt_block *eblock;

                    eblock = (krb5_encrypt_block *) nctx;
#if 0
                    if (eblock->priv && eblock->priv_size)
                        free(eblock->priv);
#endif
                    if (eblock->key)
                        krb5_free_keyblock(ser_ctx, eblock->key);
                    free(eblock);
                }
                break;
            case KV5M_PRINCIPAL:
                krb5_free_principal(ser_ctx, (krb5_principal) nctx);
                break;
            case KV5M_CHECKSUM:
                krb5_free_checksum(ser_ctx, (krb5_checksum *) nctx);
                break;
            default:
                printf("don't know how to free %d\n", dtype);
                break;
            }
        }
        else
            printf("%s: internalize returned %d\n", msg, kret);
        free(outrep);
    }
    else
        printf("%s: externalize_data returned %d\n", msg, kret);
    krb5_free_context(ser_ctx);
    return(kret);
}

/*
 * Serialize krb5_context.
 */
static krb5_error_code
ser_kcontext_test(krb5_context kcontext, int verbose)
{
    krb5_error_code     kret;
    profile_t           sprofile;
    char                dbname[128];

    snprintf(dbname, sizeof(dbname), "temp_%d", (int) getpid());
    sprofile = kcontext->profile;
    kcontext->profile = (profile_t) NULL;
    if (!(kret = ser_data(verbose, "> Context with no profile",
                          (krb5_pointer) kcontext,
                          KV5M_CONTEXT))) {
        kcontext->profile = sprofile;
        if (!(kret = ser_data(verbose, "> Context with no realm",
                              (krb5_pointer) kcontext,
                              KV5M_CONTEXT)) &&
            !(kret = krb5_set_default_realm(kcontext, "this.is.a.test"))) {
            if (!(kret = ser_data(verbose, "> Context with default realm",
                                  (krb5_pointer) kcontext,
                                  KV5M_CONTEXT))) {
                if (verbose)
                    printf("* krb5_context test succeeded\n");
            }
        }
    }
    if (kret)
        printf("* krb5_context test failed\n");
    return(kret);
}

/*
 * Serialize krb5_auth_context.
 */
static krb5_error_code
ser_acontext_test(krb5_context kcontext, int verbose)
{
    krb5_error_code     kret;
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

    actx = (krb5_auth_context) NULL;
    if (!(kret = krb5_auth_con_init(kcontext, &actx)) &&
        !(kret = ser_data(verbose, "> Vanilla auth context",
                          (krb5_pointer) actx,
                          KV5M_AUTH_CONTEXT))) {
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
        if (!(kret = krb5_auth_con_setaddrs(kcontext, actx,
                                            &local_address,
                                            &remote_address)) &&
            !(kret = krb5_auth_con_setports(kcontext, actx,
                                            &local_address,
                                            &remote_address)) &&
            !(kret = ser_data(verbose, "> Auth context with addrs/ports",
                              (krb5_pointer) actx,
                              KV5M_AUTH_CONTEXT))) {
            memset(&ukeyblock, 0, sizeof(ukeyblock));
            memset(keydata, 0, sizeof(keydata));
            ukeyblock.enctype = ENCTYPE_DES_CBC_MD5;
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
            if (!(kret = krb5_auth_con_setuseruserkey(kcontext, actx,
                                                      &ukeyblock)) &&
                !(kret = ser_data(verbose, "> Auth context with user key",
                                  (krb5_pointer) actx,
                                  KV5M_AUTH_CONTEXT)) &&
                !(kret = krb5_auth_con_initivector(kcontext, actx)) &&
                !(kret = ser_data(verbose, "> Auth context with new vector",
                                  (krb5_pointer) actx,
                                  KV5M_AUTH_CONTEXT)) &&
                !(kret = ser_data(verbose, "> Auth context with set vector",
                                  (krb5_pointer) actx,
                                  KV5M_AUTH_CONTEXT))) {
                /*
                 * Finally, add an authenticator.
                 */
                memset(&aent, 0, sizeof(aent));
                aent.magic = KV5M_AUTHENTICATOR;
                snprintf(clname, sizeof(clname),
                         "help/me/%d@this.is.a.test", (int) getpid());
                actx->authentp = &aent;
                if (!(kret = krb5_parse_name(kcontext, clname,
                                             &aent.client)) &&
                    !(kret = ser_data(verbose,
                                      "> Auth context with authenticator",
                                      (krb5_pointer) actx,
                                      KV5M_AUTH_CONTEXT))) {
                    adataent.magic = KV5M_AUTHDATA;
                    adataent.ad_type = 123;
                    adataent.length = 128;
                    adataent.contents = (krb5_octet *) stuff;
                    adatalist[0] = &adataent;
                    adatalist[1] = &adataent;
                    adatalist[2] = (krb5_authdata *) NULL;
                    aent.authorization_data = adatalist;
                    if (!(kret = ser_data(verbose,
                                          "> Auth context with full auth",
                                          (krb5_pointer) actx,
                                          KV5M_AUTH_CONTEXT))) {
                        if (verbose)
                            printf("* krb5_auth_context test succeeded\n");
                    }
                    krb5_free_principal(kcontext, aent.client);
                }
                actx->authentp = (krb5_authenticator *) NULL;
            }
        }
    }
    if (actx)
        krb5_auth_con_free(kcontext, actx);
    if (kret)
        printf("* krb5_auth_context test failed\n");
    return(kret);
}

/*
 * Serialize krb5_ccache
 */
static krb5_error_code
ser_ccache_test(krb5_context kcontext, int verbose)
{
    krb5_error_code     kret;
    char                ccname[128];
    char                princname[256];
    krb5_ccache         ccache;
    krb5_principal      principal;

    snprintf(ccname, sizeof(ccname), "temp_cc_%d", (int) getpid());
    snprintf(princname, sizeof(princname),
             "zowie%d/instance%d@this.is.a.test",
             (int) getpid(), (int) getpid());
    if (!(kret = krb5_cc_resolve(kcontext, ccname, &ccache)) &&
        !(kret = ser_data(verbose, "> Resolved default ccache",
                          (krb5_pointer) ccache, KV5M_CCACHE)) &&
        !(kret = krb5_parse_name(kcontext, princname, &principal)) &&
        !(kret = krb5_cc_initialize(kcontext, ccache, principal)) &&
        !(kret = ser_data(verbose, "> Initialized default ccache",
                          (krb5_pointer) ccache, KV5M_CCACHE)) &&
        !(kret = krb5_cc_destroy(kcontext, ccache))) {
        krb5_free_principal(kcontext, principal);
        snprintf(ccname, sizeof(ccname), "FILE:temp_cc_%d", (int) getpid());
        snprintf(princname, sizeof(princname), "xxx%d/i%d@this.is.a.test",
                 (int) getpid(), (int) getpid());
        if (!(kret = krb5_cc_resolve(kcontext, ccname, &ccache)) &&
            !(kret = ser_data(verbose, "> Resolved FILE ccache",
                              (krb5_pointer) ccache, KV5M_CCACHE)) &&
            !(kret = krb5_parse_name(kcontext, princname, &principal)) &&
            !(kret = krb5_cc_initialize(kcontext, ccache, principal)) &&
            !(kret = ser_data(verbose, "> Initialized FILE ccache",
                              (krb5_pointer) ccache, KV5M_CCACHE)) &&
            !(kret = krb5_cc_destroy(kcontext, ccache))) {
            krb5_free_principal(kcontext, principal);

            if (verbose)
                printf("* ccache test succeeded\n");
        }
    }
    if (kret)
        printf("* krb5_ccache test failed\n");
    return(kret);
}

/*
 * Serialize krb5_keytab.
 */
static krb5_error_code
ser_keytab_test(krb5_context kcontext, int verbose)
{
    krb5_error_code     kret;
    char                ccname[128];
    krb5_keytab         keytab;

    snprintf(ccname, sizeof(ccname), "temp_kt_%d", (int) getpid());
    if (!(kret = krb5_kt_resolve(kcontext, ccname, &keytab)) &&
        !(kret = ser_data(verbose, "> Resolved default keytab",
                          (krb5_pointer) keytab, KV5M_KEYTAB)) &&
        !(kret = krb5_kt_close(kcontext, keytab))) {
        snprintf(ccname, sizeof(ccname), "FILE:temp_kt_%d", (int) getpid());
        if (!(kret = krb5_kt_resolve(kcontext, ccname, &keytab)) &&
            !(kret = ser_data(verbose, "> Resolved FILE keytab",
                              (krb5_pointer) keytab, KV5M_KEYTAB)) &&
            !(kret = krb5_kt_close(kcontext, keytab))) {
            snprintf(ccname, sizeof(ccname),
                     "WRFILE:temp_kt_%d", (int) getpid());
            if (!(kret = krb5_kt_resolve(kcontext, ccname, &keytab)) &&
                !(kret = ser_data(verbose, "> Resolved WRFILE keytab",
                                  (krb5_pointer) keytab, KV5M_KEYTAB)) &&
                !(kret = krb5_kt_close(kcontext, keytab))) {
                if (verbose)
                    printf("* keytab test succeeded\n");
            }
        }
    }
    if (kret)
        printf("* krb5_keytab test failed\n");
    return(kret);
}

/*
 * Serialize krb5_rcache.
 */
static krb5_error_code
ser_rcache_test(krb5_context kcontext, int verbose)
{
    krb5_error_code     kret;
    char                rcname[128];
    krb5_rcache         rcache;

    snprintf(rcname, sizeof(rcname), "dfl:temp_rc_%d", (int) getpid());
    if (!(kret = krb5_rc_resolve_full(kcontext, &rcache, rcname)) &&
        !(kret = ser_data(verbose, "> Resolved FILE rcache",
                          (krb5_pointer) rcache, KV5M_RCACHE)) &&
        !(kret = krb5_rc_initialize(kcontext, rcache, 3600*24)) &&
        !(kret = ser_data(verbose, "> Initialized FILE rcache",
                          (krb5_pointer) rcache, KV5M_RCACHE)) &&
        !(kret = krb5_rc_destroy(kcontext, rcache))) {
        if (verbose)
            printf("* rcache test succeeded\n");
    }
    if (kret)
        printf("* krb5_rcache test failed\n");
    return(kret);
}

#if 0
/*
 * Serialize krb5_encrypt_block.
 */
static krb5_error_code
ser_eblock_test(kcontext, verbose)
    krb5_context        kcontext;
    int                 verbose;
{
    krb5_error_code     kret;
    krb5_encrypt_block  eblock;
    krb5_keyblock       ukeyblock;
    krb5_octet          keydata[8];

    memset(&eblock, 0, sizeof(krb5_encrypt_block));
    eblock.magic = KV5M_ENCRYPT_BLOCK;
    krb5_use_enctype(kcontext, &eblock, DEFAULT_KDC_ENCTYPE);
    if (!(kret = ser_data(verbose, "> NULL eblock",
                          (krb5_pointer) &eblock, KV5M_ENCRYPT_BLOCK))) {
#if 0
        eblock.priv = (krb5_pointer) stuff;
        eblock.priv_size = 8;
#endif
        if (!(kret = ser_data(verbose, "> eblock with private data",
                              (krb5_pointer) &eblock,
                              KV5M_ENCRYPT_BLOCK))) {
            memset(&ukeyblock, 0, sizeof(ukeyblock));
            memset(keydata, 0, sizeof(keydata));
            ukeyblock.enctype = ENCTYPE_DES_CBC_MD5;
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
            eblock.key = &ukeyblock;
            if (!(kret = ser_data(verbose, "> eblock with private key",
                                  (krb5_pointer) &eblock,
                                  KV5M_ENCRYPT_BLOCK))) {
                if (verbose)
                    printf("* eblock test succeeded\n");
            }
        }
    }
    if (kret)
        printf("* eblock test failed\n");
    return(kret);
}
#endif

/*
 * Serialize krb5_principal
 */
static krb5_error_code
ser_princ_test(krb5_context kcontext, int verbose)
{
    krb5_error_code     kret;
    krb5_principal      princ;
    char                pname[1024];

    snprintf(pname, sizeof(pname),
             "the/quick/brown/fox/jumped/over/the/lazy/dog/%d@this.is.a.test",
             (int) getpid());
    if (!(kret = krb5_parse_name(kcontext, pname, &princ))) {
        if (!(kret = ser_data(verbose, "> Principal",
                              (krb5_pointer) princ, KV5M_PRINCIPAL))) {
            if (verbose)
                printf("* principal test succeeded\n");
        }
        krb5_free_principal(kcontext, princ);
    }
    if (kret)
        printf("* principal test failed\n");
    return(kret);
}

/*
 * Serialize krb5_checksum.
 */
static krb5_error_code
ser_cksum_test(krb5_context kcontext, int verbose)
{
    krb5_error_code     kret;
    krb5_checksum       checksum;
    krb5_octet          ckdata[24];

    memset(&checksum, 0, sizeof(krb5_checksum));
    checksum.magic = KV5M_CHECKSUM;
    if (!(kret = ser_data(verbose, "> NULL checksum",
                          (krb5_pointer) &checksum, KV5M_CHECKSUM))) {
        checksum.checksum_type = 123;
        checksum.length = sizeof(ckdata);
        checksum.contents = ckdata;
        memcpy(ckdata, &stuff, sizeof(ckdata));
        if (!(kret = ser_data(verbose, "> checksum with data",
                              (krb5_pointer) &checksum, KV5M_CHECKSUM))) {
            if (verbose)
                printf("* checksum test succeeded\n");
        }
    }
    if (kret)
        printf("* checksum test failed\n");
    return(kret);
}

/*
 * Main procedure.
 */
int
main(int argc, char **argv)
{
    krb5_error_code     kret;
    krb5_context        kcontext;
    int                 do_atest, do_ctest, do_ktest, do_rtest, do_xtest;
    int                 do_etest, do_ptest, do_stest;
    int                 verbose;
    int                 option;
    extern char         *optarg;
    char                ch_err;

    kret = 0;
    verbose = 0;
    do_atest = 1;
    do_xtest = 1;
    do_ctest = 1;
    do_etest = 1;
    do_ktest = 1;
    do_ptest = 1;
    do_rtest = 1;
    do_stest = 1;
    while ((option = getopt(argc, argv, "acekprsxvACEKPRSX")) != -1) {
        switch (option) {
        case 'a':
            do_atest = 0;
            break;
        case 'c':
            do_ctest = 0;
            break;
        case 'e':
            do_etest = 0;
            break;
        case 'k':
            do_ktest = 0;
            break;
        case 'p':
            do_ptest = 0;
            break;
        case 'r':
            do_rtest = 0;
            break;
        case 's':
            do_stest = 0;
            break;
        case 'x':
            do_xtest = 0;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'A':
            do_atest = 1;
            break;
        case 'C':
            do_ctest = 1;
            break;
#if 0
        case 'E':
            do_etest = 1;
            break;
#endif
        case 'K':
            do_ktest = 1;
            break;
        case 'P':
            do_ptest = 1;
            break;
        case 'R':
            do_rtest = 1;
            break;
        case 'S':
            do_stest = 1;
            break;
        case 'X':
            do_xtest = 1;
            break;
        default:
            fprintf(stderr,
                    "%s: usage is %s [-acekprsxvACEKPRSX]\n",
                    argv[0], argv[0]);
            exit(1);
            break;
        }
    }
    if ((kret = krb5_init_context(&kcontext))) {
        com_err(argv[0], kret, "while initializing krb5");
        exit(1);
    }

    if (do_xtest) {
        ch_err = 'x';
        kret = ser_kcontext_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
    if (do_atest) {
        ch_err = 'a';
        kret = ser_acontext_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
    if (do_ctest) {
        ch_err = 'c';
        kret = ser_ccache_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
    if (do_ktest) {
        ch_err = 'k';
        kret = ser_keytab_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
    if (do_rtest) {
        ch_err = 'r';
        kret = ser_rcache_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
#if 0 /* code to be tested is currently disabled */
    if (do_etest) {
        ch_err = 'e';
        kret = ser_eblock_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
#endif
    if (do_ptest) {
        ch_err = 'p';
        kret = ser_princ_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
    if (do_stest) {
        ch_err = 's';
        kret = ser_cksum_test(kcontext, verbose);
        if (kret)
            goto fail;
    }
    krb5_free_context(kcontext);

    exit(0);
fail:
    com_err(argv[0], kret, "--- test %cfailed", ch_err);
    krb5_free_context(kcontext);
    exit(1);
}
