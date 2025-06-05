/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/tabdump.c - reporting-friendly tabular KDB dumps */
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

#include <k5-int.h>
#include "k5-platform.h"        /* for asprintf */
#include "k5-hex.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <kadm5/admin.h>
#include <kadm5/server_internal.h>

#include "adm_proto.h"
#include "kdb5_util.h"
#include "tdumputil.h"

struct tdopts {
    int csv;                    /* 1 for CSV, 0 for tab-separated */
    int emptyhex_empty;         /* print empty hex strings as "" not "-1" */
    int numeric;                /* numeric instead of symbolic output */
    int omitheader;             /* omit field headers */
    int writerectype;           /* write record type prefix */
    char *fname;                /* output file name */
};

struct rec_args;

typedef int (tdump_princ_fn)(struct rec_args *, const char *, krb5_db_entry *);
typedef int (tdump_policy_fn)(struct rec_args *, const char *,
                              kadm5_policy_ent_t);

/* Descriptor for a tabdump record type */
struct tdtype {
    const char *rectype;
    char * const *fieldnames;
    tdump_princ_fn *princ_fn;
    tdump_policy_fn *policy_fn;
};

static tdump_princ_fn keydata;
static tdump_princ_fn keyinfo;
static tdump_princ_fn princ_flags;
static tdump_princ_fn princ_lockout;
static tdump_princ_fn princ_meta;
static tdump_princ_fn princ_stringattrs;
static tdump_princ_fn princ_tktpolicy;

static char * const keydata_fields[] = {
    "name", "keyindex", "kvno", "enctype", "key", "salttype", "salt", NULL
};
static char * const keyinfo_fields[] = {
    "name", "keyindex", "kvno", "enctype", "salttype", "salt", NULL
};
static char * const princ_flags_fields[] = {
    "name", "flag", "value", NULL
};
static char * const princ_lockout_fields[] = {
    "name", "last_success", "last_failed", "fail_count", NULL
};
static char * const princ_meta_fields[] = {
    "name", "modby", "modtime", "lastpwd", "policy", "mkvno", "hist_kvno", NULL
};
static char * const princ_stringattrs_fields[] = {
    "name", "key", "value", NULL
};
static char * const princ_tktpolicy_fields[] = {
    "name", "expiration", "pw_expiration", "max_life", "max_renew_life", NULL
};

/* Lookup table for tabdump record types */
static struct tdtype tdtypes[] = {
    {"keydata", keydata_fields, keydata, NULL},
    {"keyinfo", keyinfo_fields, keyinfo, NULL},
    {"princ_flags", princ_flags_fields, princ_flags, NULL},
    {"princ_lockout", princ_lockout_fields, princ_lockout, NULL},
    {"princ_meta", princ_meta_fields, princ_meta, NULL},
    {"princ_stringattrs", princ_stringattrs_fields, princ_stringattrs, NULL},
    {"princ_tktpolicy", princ_tktpolicy_fields, princ_tktpolicy, NULL},
};
#define NTDTYPES (sizeof(tdtypes)/sizeof(tdtypes[0]))

/* State to pass to KDB iterator */
struct rec_args {
    FILE *f;
    struct tdtype *tdtype;
    struct rechandle *rh;
    struct tdopts *opts;
};

/* Decode the KADM_DATA from a DB entry.*/
static int
get_adb(krb5_db_entry *dbe, osa_princ_ent_rec *adb)
{
    XDR xdrs;
    int success;
    krb5_tl_data tl_data;
    krb5_error_code ret;

    memset(adb, 0, sizeof(*adb));
    tl_data.tl_data_type = KRB5_TL_KADM_DATA;
    ret = krb5_dbe_lookup_tl_data(util_context, dbe, &tl_data);
    if (ret != 0 || tl_data.tl_data_length == 0)
        return 0;
    xdrmem_create(&xdrs, (caddr_t)tl_data.tl_data_contents,
                  tl_data.tl_data_length, XDR_DECODE);
    success = xdr_osa_princ_ent_rec(&xdrs, adb);
    xdr_destroy(&xdrs);
    return success;
}

/* Write a date field as an ISO 8601 UTC date/time representation. */
static int
write_date_iso(struct rec_args *args, krb5_timestamp when)
{
    char buf[64];
    time_t t;
    struct tm *tm = NULL;
    struct rechandle *h = args->rh;

    t = ts2tt(when);
    tm = gmtime(&t);
    if (tm == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm) == 0) {
        errno = EINVAL;
        return -1;
    }
    if (writefield(h, "%s", buf) < 0)
        return -1;
    return 0;
}

/* Write a date field, optionally as a decimal POSIX timestamp. */
static int
write_date(struct rec_args *args, krb5_timestamp when)
{
    struct tdopts *opts = args->opts;
    struct rechandle *h = args->rh;

    if (opts->numeric)
        return writefield(h, "%d", when);

    return write_date_iso(args, when);
}

/* Write an enctype field, optionally as decimal. */
static krb5_error_code
write_enctype(struct rec_args *args, krb5_int16 etype)
{
    char buf[256];
    krb5_error_code ret;
    struct rechandle *h = args->rh;
    struct tdopts *opts = args->opts;

    if (!opts->numeric) {
        ret = krb5_enctype_to_name(etype, 0, buf, sizeof(buf));
        if (ret == 0) {
            if (writefield(h, "%s", buf) < 0)
                return errno;
            return ret;
        }
    }
    /* decimal if requested, or if conversion failed */
    if (writefield(h, "%d", etype) < 0)
        return errno;
    return 0;
}

/* Write a salttype field, optionally as decimal. */
static krb5_error_code
write_salttype(struct rec_args *args, krb5_int16 salttype)
{
    char buf[256];
    krb5_error_code ret;
    struct rechandle *h = args->rh;
    struct tdopts *opts = args->opts;

    if (!opts->numeric) {
        ret = krb5_salttype_to_string(salttype, buf, sizeof(buf));
        if (ret == 0) {
            if (writefield(h, "%s", buf) < 0)
                return errno;
            return ret;
        }
    }
    /* decimal if requested, or if conversion failed */
    if (writefield(h, "%d", salttype) < 0)
        return errno;
    return 0;
}

/*
 * Write a field of bytes from krb5_data as a hexadecimal string.  Write empty
 * strings as "-1" unless requested.
 */
static int
write_data(struct rec_args *args, krb5_data *data)
{
    int ret;
    char *hex;
    struct rechandle *h = args->rh;
    struct tdopts *opts = args->opts;

    if (data->length == 0 && !opts->emptyhex_empty) {
        if (writefield(h, "-1") < 0)
            return -1;
        return 0;
    }

    ret = k5_hex_encode(data->data, data->length, FALSE, &hex);
    if (ret) {
        errno = ret;
        return -1;
    }

    ret = writefield(h, "%s", hex);
    free(hex);
    return ret;
}

/* Write a single record of a keydata/keyinfo key set. */
static krb5_error_code
keyinfo_rec(struct rec_args *args, const char *name, int i, krb5_key_data *kd,
            int dumpkeys)
{
    int ret;
    krb5_data data;
    struct rechandle *h = args->rh;

    if (startrec(h) < 0)
        return errno;
    if (writefield(h, "%s", name) < 0)
        return errno;
    if (writefield(h, "%d", i) < 0)
        return errno;
    if (writefield(h, "%d", kd->key_data_kvno) < 0)
        return errno;
    if (write_enctype(args, kd->key_data_type[0]) < 0)
        return errno;
    if (dumpkeys) {
        data.length = kd->key_data_length[0];
        data.data = (void *)kd->key_data_contents[0];
        if (write_data(args, &data) < 0)
            return errno;
    }
    ret = write_salttype(args, kd->key_data_type[1]);
    if (ret)
        return ret;
    data.length = kd->key_data_length[1];
    data.data = (void *)kd->key_data_contents[1];
    if (write_data(args, &data) < 0)
        return errno;
    if (endrec(h) < 0)
        return errno;
    return 0;
}

/* Write out a principal's key set, optionally including actual key data. */
static krb5_error_code
keyinfo_common(struct rec_args *args, const char *name, krb5_db_entry *entry,
               int dumpkeys)
{
    krb5_error_code ret;
    krb5_key_data kd;
    int i;

    for (i = 0; i < entry->n_key_data; i++) {
        kd = entry->key_data[i];
        /* missing salt data -> normal salt */
        if (kd.key_data_ver == 1) {
            kd.key_data_ver = 2;
            kd.key_data_type[1] = KRB5_KDB_SALTTYPE_NORMAL;
            kd.key_data_length[1] = 0;
            kd.key_data_contents[1] = NULL;
        }
        ret = keyinfo_rec(args, name, i, &kd, dumpkeys);
        if (ret)
            return ret;
    }
    return 0;
}

/* Write a principal's key data. */
static krb5_error_code
keydata(struct rec_args *args, const char *name, krb5_db_entry *dbe)
{
    return keyinfo_common(args, name, dbe, 1);
}

/* Write a principal's key info (suppressing actual key data). */
static krb5_error_code
keyinfo(struct rec_args *args, const char *name, krb5_db_entry *dbe)
{
    return keyinfo_common(args, name, dbe, 0);
}

/* Write a record corresponding to a single principal flag setting. */
static krb5_error_code
princflag_rec(struct rechandle *h, const char *name, const char *flagname,
              int set)
{
    if (startrec(h) < 0)
        return errno;
    if (writefield(h, "%s", name) < 0)
        return errno;
    if (writefield(h, "%s", flagname) < 0)
        return errno;
    if (writefield(h, "%d", set) < 0)
        return errno;
    if (endrec(h) < 0)
        return errno;
    return 0;
}

/* Write a principal's flag settings. */
static krb5_error_code
princ_flags(struct rec_args *args, const char *name, krb5_db_entry *dbe)
{
    int i;
    char *s = NULL;
    krb5_flags flags = dbe->attributes;
    krb5_error_code ret;
    struct tdopts *opts = args->opts;
    struct rechandle *h = args->rh;

    for (i = 0; i < 32; i++) {
        if (opts->numeric) {
            if (asprintf(&s, "0x%08lx", 1UL << i) == -1)
                return ENOMEM;
        } else {
            ret = krb5_flagnum_to_string(i, &s);
            if (ret)
                return ret;
            /* Don't print unknown flags if they're not set and numeric output
             * isn't requested. */
            if (!(flags & (1UL << i)) && strncmp(s, "0x", 2) == 0) {
                free(s);
                continue;
            }
        }
        ret = princflag_rec(h, name, s, ((flags & (1UL << i)) != 0));
        free(s);
        if (ret)
            return ret;
    }
    return 0;
}

/* Write a principal's lockout data. */
static krb5_error_code
princ_lockout(struct rec_args *args, const char *name, krb5_db_entry *dbe)
{
    struct rechandle *h = args->rh;

    if (startrec(h) < 0)
        return errno;
    if (writefield(h, "%s", name) < 0)
        return errno;
    if (write_date(args, dbe->last_success) < 0)
        return errno;
    if (write_date(args, dbe->last_failed) < 0)
        return errno;
    if (writefield(h, "%d", dbe->fail_auth_count) < 0)
        return errno;
    if (endrec(h) < 0)
        return errno;
    return 0;
}

/* Write a principal's metadata. */
static krb5_error_code
princ_meta(struct rec_args *args, const char *name, krb5_db_entry *dbe)
{
    int got_adb = 0;
    char *modby;
    krb5_kvno mkvno;
    const char *policy;
    krb5_principal mod_princ = NULL;
    krb5_timestamp mod_time, last_pwd;
    krb5_error_code ret;
    osa_princ_ent_rec adb;
    struct rechandle *h = args->rh;

    memset(&adb, 0, sizeof(adb));
    if (startrec(h) < 0)
        return errno;
    if (writefield(h, "%s", name) < 0)
        return errno;

    ret = krb5_dbe_lookup_last_pwd_change(util_context, dbe, &last_pwd);
    if (ret)
        return ret;
    ret = krb5_dbe_get_mkvno(util_context, dbe, &mkvno);
    if (ret)
        return ret;

    ret = krb5_dbe_lookup_mod_princ_data(util_context, dbe, &mod_time,
                                         &mod_princ);
    if (ret)
        return ret;
    ret = krb5_unparse_name(util_context, mod_princ, &modby);
    krb5_free_principal(util_context, mod_princ);
    if (ret)
        return ret;
    ret = writefield(h, "%s", modby);
    krb5_free_unparsed_name(util_context, modby);
    if (ret < 0)
        return errno;

    if (write_date(args, mod_time) < 0)
        return errno;
    if (write_date(args, last_pwd) < 0)
        return errno;

    got_adb = get_adb(dbe, &adb);
    if (got_adb && adb.policy != NULL)
        policy = adb.policy;
    else
        policy = "";
    ret = writefield(h, "%s", policy);
    if (ret < 0) {
        ret = errno;
        goto cleanup;
    }
    if (writefield(h, "%d", mkvno) < 0) {
        ret = errno;
        goto cleanup;
    }
    if (writefield(h, "%d", adb.admin_history_kvno) < 0) {
        ret = errno;
        goto cleanup;
    }
    if (endrec(h) < 0)
        ret = errno;
    else
        ret = 0;

cleanup:
    kdb_free_entry(NULL, NULL, &adb);
    return ret;
}

/* Write a principal's string attributes. */
static krb5_error_code
princ_stringattrs(struct rec_args *args, const char *name, krb5_db_entry *dbe)
{
    int i, nattrs;
    krb5_error_code ret;
    krb5_string_attr *attrs;
    struct rechandle *h = args->rh;

    ret = krb5_dbe_get_strings(util_context, dbe, &attrs, &nattrs);
    if (ret)
        return ret;
    for (i = 0; i < nattrs; i++) {
        if (startrec(h) < 0) {
            ret = errno;
            goto cleanup;
        }
        if (writefield(h, "%s", name) < 0) {
            ret = errno;
            goto cleanup;
        }
        if (writefield(h, "%s", attrs[i].key) < 0) {
            ret = errno;
            goto cleanup;
        }
        if (writefield(h, "%s", attrs[i].value) < 0) {
            ret = errno;
            goto cleanup;
        }
        if (endrec(h) < 0) {
            ret = errno;
            goto cleanup;
        }
    }
cleanup:
    krb5_dbe_free_strings(util_context, attrs, nattrs);
    return ret;
}

/* Write a principal's ticket policy. */
static krb5_error_code
princ_tktpolicy(struct rec_args *args, const char *name, krb5_db_entry *dbe)
{
    struct rechandle *h = args->rh;

    if (startrec(h) < 0)
        return errno;
    if (writefield(h, "%s", name) < 0)
        return errno;
    if (write_date(args, dbe->expiration) < 0)
        return errno;
    if (write_date(args, dbe->pw_expiration) < 0)
        return errno;
    if (writefield(h, "%d", dbe->max_life) < 0)
        return errno;
    if (writefield(h, "%d", dbe->max_renewable_life) < 0)
        return errno;
    if (endrec(h) < 0)
        return errno;
    return 0;
}

/* Iterator function for krb5_db_iterate() */
static krb5_error_code
tditer(void *ptr, krb5_db_entry *entry)
{
    krb5_error_code ret;
    struct rec_args *args = ptr;
    char *name;

    ret = krb5_unparse_name(util_context, entry->princ, &name);
    if (ret) {
        com_err(progname, ret, _("while unparsing principal name"));
        return ret;
    }
    ret = args->tdtype->princ_fn(args, name, entry);
    krb5_free_unparsed_name(util_context, name);
    if (ret)
        return ret;
    return 0;
}

/* Set up state structure for the iterator. */
static krb5_error_code
setup_args(struct rec_args *args, struct tdtype *tdtype,
             struct tdopts *opts)
{
    FILE *f = NULL;
    const char *rectype = NULL;
    struct rechandle *rh;

    args->tdtype = tdtype;
    args->opts = opts;
    if (opts->fname != NULL && strcmp(opts->fname, "-") != 0) {
        f = fopen(opts->fname, "w");
        if (f == NULL) {
            com_err(progname, errno, _("opening %s for writing"),
                    opts->fname);
            return errno;
        }
        args->f = f;
    } else {
        f = stdout;
        args->f = NULL;
    }
    if (opts->writerectype)
        rectype = tdtype->rectype;
    if (opts->csv)
        rh = rechandle_csv(f, rectype);
    else
        rh = rechandle_tabsep(f, rectype);
    if (rh == NULL)
        return ENOMEM;
    args->rh = rh;
    if (!opts->omitheader && writeheader(rh, tdtype->fieldnames) < 0)
        return errno;
    return 0;
}

/* Clean up the state structure. */
static void
cleanup_args(struct rec_args *args)
{
    rechandle_free(args->rh);
    if (args->f != NULL)
        fclose(args->f);
}

void
tabdump(int argc, char **argv)
{
    int ch;
    size_t i;
    const char *rectype;
    struct rec_args args;
    struct tdopts opts;
    krb5_error_code ret;

    memset(&opts, 0, sizeof(opts));
    memset(&args, 0, sizeof(args));
    optind = 1;
    while ((ch = getopt(argc, argv, "Hceno:")) != -1) {
        switch (ch) {
        case 'H':
            opts.omitheader = 1;
            break;
        case 'c':
            opts.csv = 1;
            break;
        case 'e':
            opts.emptyhex_empty = 1;
            break;
        case 'n':
            opts.numeric = 1;
            break;
        case 'o':
            opts.fname = optarg;
            break;
        case '?':
        default:
            usage();
            break;
        }
    }
    if (argc - optind < 1)
        usage();
    rectype = argv[optind];
    for (i = 0; i < NTDTYPES; i++) {
        if (strcmp(rectype, tdtypes[i].rectype) == 0) {
            setup_args(&args, &tdtypes[i], &opts);
            break;
        }
    }
    if (i >= NTDTYPES)
        usage();
    ret = krb5_db_iterate(util_context, NULL, tditer, &args, 0);
    cleanup_args(&args);
    if (ret) {
        com_err(progname, ret, _("performing tabular dump"));
        exit_status++;
    }
}
