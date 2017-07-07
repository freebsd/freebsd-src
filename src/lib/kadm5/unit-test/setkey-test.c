/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <k5-int.h>
#include <kadm5/admin.h>

#if     HAVE_SRAND48
#define RAND()          lrand48()
#define SRAND(a)        srand48(a)
#define RAND_TYPE       long
#elif   HAVE_SRAND
#define RAND()          rand()
#define SRAND(a)        srand(a)
#define RAND_TYPE       int
#elif   HAVE_SRANDOM
#define RAND()          random()
#define SRAND(a)        srandom(a)
#define RAND_TYPE       long
#else   /* no random */
need a random number generator
#endif  /* no random */

krb5_keyblock test1[] = {
    {0, ENCTYPE_DES_CBC_CRC, 0, 0},
    {-1},
};
krb5_keyblock test2[] = {
    {0, ENCTYPE_DES_CBC_CRC, 0, 0},
    {-1},
};
krb5_keyblock test3[] = {
    {0, ENCTYPE_DES_CBC_CRC, 0, 0},
    {-1},
};

krb5_keyblock *tests[] = {
    test1, test2, test3, NULL
};

#if 0
int keyblocks_equal(krb5_keyblock *kb1, krb5_keyblock *kb2)
{
    return (kb1->enctype == kb2->enctype &&
            kb1->length == kb2->length &&
            memcmp(kb1->contents, kb2->contents, kb1->length) == 0);
}
#endif

krb5_data tgtname = {
    0,
    KRB5_TGS_NAME_SIZE,
    KRB5_TGS_NAME
};

krb5_enctype ktypes[] = { 0, 0 };

extern krb5_kt_ops krb5_ktf_writable_ops;

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_keytab kt;
    krb5_keytab_entry ktent;
    krb5_encrypt_block eblock;
    krb5_creds my_creds;
    krb5_get_init_creds_opt *opt;
    kadm5_principal_ent_rec princ_ent;
    krb5_principal princ, server;
    char pw[16];
    char *whoami, *principal, *authprinc, *authpwd;
    krb5_data pwdata;
    void *handle;
    int ret, i, test, encnum;

    whoami = argv[0];

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s principal [authuser] [authpwd]\n", whoami);
        exit(1);
    }
    principal = argv[1];
    authprinc = (argc > 2) ? argv[2] : argv[0];
    authpwd = (argc > 3) ? argv[3] : NULL;

    /*
     * Setup.  Initialize data structures, open keytab, open connection
     * to kadm5 server.
     */

    memset(&context, 0, sizeof(context));
    kadm5_init_krb5_context(&context);

    ret = krb5_parse_name(context, principal, &princ);
    if (ret) {
        com_err(whoami, ret, "while parsing principal name %s", principal);
        exit(1);
    }

    if((ret = krb5_build_principal_ext(context, &server,
                                       krb5_princ_realm(kcontext, princ)->length,
                                       krb5_princ_realm(kcontext, princ)->data,
                                       tgtname.length, tgtname.data,
                                       krb5_princ_realm(kcontext, princ)->length,
                                       krb5_princ_realm(kcontext, princ)->data,
                                       0))) {
        com_err(whoami, ret, "while building server name");
        exit(1);
    }

    ret = krb5_kt_default(context, &kt);
    if (ret) {
        com_err(whoami, ret, "while opening keytab");
        exit(1);
    }

    ret = kadm5_init(context, authprinc, authpwd, KADM5_ADMIN_SERVICE, NULL,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, NULL,
                     &handle);
    if (ret) {
        com_err(whoami, ret, "while initializing connection");
        exit(1);
    }

    /* these pw's don't need to be secure, just different every time */
    SRAND((RAND_TYPE)time((void *) NULL));
    pwdata.data = pw;
    pwdata.length = sizeof(pw);

    /*
     * For each test:
     *
     * For each enctype in the test, construct a random password/key.
     * Assign all keys to principal with kadm5_setkey_principal.  Add
     * each key to the keytab, and acquire an initial ticket with the
     * keytab (XXX can I specify the kvno explicitly?).  If
     * krb5_get_init_creds_keytab succeeds, then the keys were set
     * successfully.
     */
    for (test = 0; tests[test] != NULL; test++) {
        krb5_keyblock *testp = tests[test];
        kadm5_key_data *extracted;
        int n_extracted, match;
        printf("+ Test %d:\n", test);

        for (encnum = 0; testp[encnum].magic != -1; encnum++) {
            for (i = 0; i < sizeof(pw); i++)
                pw[i] = (RAND() % 26) + '0'; /* XXX */

            krb5_use_enctype(context, &eblock, testp[encnum].enctype);
            ret = krb5_string_to_key(context, &eblock, &testp[encnum],
                                     &pwdata, NULL);
            if (ret) {
                com_err(whoami, ret, "while converting string to key");
                exit(1);
            }
        }

        /* now, encnum == # of keyblocks in testp */
        ret = kadm5_setkey_principal(handle, princ, testp, encnum);
        if (ret) {
            com_err(whoami, ret, "while setting keys");
            exit(1);
        }

        ret = kadm5_get_principal(handle, princ, &princ_ent, KADM5_KVNO);
        if (ret) {
            com_err(whoami, ret, "while retrieving principal");
            exit(1);
        }

        ret = kadm5_get_principal_keys(handle, princ, 0, &extracted,
                                       &n_extracted);
        if (ret) {
            com_err(whoami, ret, "while extracting keys");
            exit(1);
        }

        for (encnum = 0; testp[encnum].magic != -1; encnum++) {
            printf("+   enctype %d\n", testp[encnum].enctype);

            for (match = 0; match < n_extracted; match++) {
                if (extracted[match].key.enctype == testp[encnum].enctype)
                    break;
            }
            if (match >= n_extracted) {
                com_err(whoami, KRB5_WRONG_ETYPE, "while matching enctypes");
                exit(1);
            }
            if (extracted[match].key.length != testp[encnum].length ||
                memcmp(extracted[match].key.contents, testp[encnum].contents,
                       testp[encnum].length) != 0) {
                com_err(whoami, KRB5_KDB_NO_MATCHING_KEY, "verifying keys");
                exit(1);
            }

            memset(&ktent, 0, sizeof(ktent));
            ktent.principal = princ;
            ktent.key = testp[encnum];
            ktent.vno = princ_ent.kvno;

            ret = krb5_kt_add_entry(context, kt, &ktent);
            if (ret) {
                com_err(whoami, ret, "while adding keytab entry");
                exit(1);
            }

            memset(&my_creds, 0, sizeof(my_creds));
            my_creds.client = princ;
            my_creds.server = server;

            ktypes[0] = testp[encnum].enctype;
            ret = krb5_get_init_creds_opt_alloc(context, &opt);
            if (ret) {
                com_err(whoami, ret, "while allocating gic opts");
                exit(1);
            }
            krb5_get_init_creds_opt_set_etype_list(opt, ktypes, 1);
            ret = krb5_get_init_creds_keytab(context, &my_creds, princ,
                                             kt, 0, NULL /* in_tkt_service */,
                                             opt);
            krb5_get_init_creds_opt_free(context, opt);
            if (ret) {
                com_err(whoami, ret, "while acquiring initial ticket");
                exit(1);
            }
            krb5_free_cred_contents(context, &my_creds);

            /* since I can't specify enctype explicitly ... */
            ret = krb5_kt_remove_entry(context, kt, &ktent);
            if (ret) {
                com_err(whoami, ret, "while removing keytab entry");
                exit(1);
            }
        }

        (void)kadm5_free_kadm5_key_data(context, n_extracted, extracted);
    }

    ret = krb5_kt_close(context, kt);
    if (ret) {
        com_err(whoami, ret, "while closing keytab");
        exit(1);
    }

    ret = kadm5_destroy(handle);
    if (ret) {
        com_err(whoami, ret, "while closing kadmin connection");
        exit(1);
    }

    krb5_free_principal(context, princ);
    krb5_free_principal(context, server);
    krb5_free_context(context);
    return 0;
}
