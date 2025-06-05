/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/dbutil/dump.c - Dump a KDC database */
/*
 * Copyright 1990,1991,2001,2006,2008,2009,2013 by the Massachusetts Institute
 * of Technology.  All Rights Reserved.
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
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <k5-int.h>
#include <kadm5/admin.h>
#include <kadm5/server_internal.h>
#include <kdb.h>
#include <com_err.h>
#include "kdb5_util.h"
#if defined(HAVE_REGEX_H) && defined(HAVE_REGCOMP)
#include <regex.h>
#endif  /* HAVE_REGEX_H */

/* Needed for master key conversion. */
static krb5_boolean mkey_convert;
krb5_keyblock new_master_keyblock;
krb5_kvno new_mkvno;

#define K5Q1(x) #x
#define K5Q(x) K5Q1(x)
#define K5CONST_WIDTH_SCANF_STR(x) "%" K5Q(x) "s"

/* Use compile(3) if no regcomp present. */
#if !defined(HAVE_REGCOMP) && defined(HAVE_REGEXP_H)
#define INIT char *sp = instring;
#define GETC() (*sp++)
#define PEEKC() (*sp)
#define UNGETC(c) (--sp)
#define RETURN(c) return(c)
#define ERROR(c)
#define RE_BUF_SIZE 1024
#include <regexp.h>
#endif /* !HAVE_REGCOMP && HAVE_REGEXP_H */

typedef krb5_error_code (*dump_func)(krb5_context context,
                                     krb5_db_entry *entry, const char *name,
                                     FILE *fp, krb5_boolean verbose,
                                     krb5_boolean omit_nra);
typedef int (*load_func)(krb5_context context, const char *dumpfile, FILE *fp,
                         krb5_boolean verbose, int *linenop);

typedef struct _dump_version {
    char *name;
    char *header;
    int updateonly;
    int iprop;
    int ipropx;
    dump_func dump_princ;
    osa_adb_iter_policy_func dump_policy;
    load_func load_record;
} dump_version;

struct dump_args {
    FILE *ofile;
    krb5_context context;
    char **names;
    int nnames;
    krb5_boolean verbose;
    krb5_boolean omit_nra;      /* omit non-replicated attributes */
    dump_version *dump;
};

/* External data */
extern krb5_db_entry *master_entry;

/*
 * Re-encrypt the key_data with the new master key...
 */
krb5_error_code
master_key_convert(krb5_context context, krb5_db_entry *db_entry)
{
    krb5_error_code retval;
    krb5_keyblock v5plainkey, *key_ptr, *tmp_mkey;
    krb5_keysalt keysalt;
    krb5_key_data new_key_data, *key_data;
    krb5_boolean is_mkey;
    krb5_kvno kvno;
    int i, j;

    is_mkey = krb5_principal_compare(context, master_princ, db_entry->princ);

    if (is_mkey) {
        return add_new_mkey(context, db_entry, &new_master_keyblock,
                            new_mkvno);
    }

    for (i = 0; i < db_entry->n_key_data; i++) {
        key_data = &db_entry->key_data[i];
        retval = krb5_dbe_find_mkey(context, db_entry, &tmp_mkey);
        if (retval)
            return retval;
        retval = krb5_dbe_decrypt_key_data(context, tmp_mkey, key_data,
                                           &v5plainkey, &keysalt);
        if (retval)
            return retval;

        memset(&new_key_data, 0, sizeof(new_key_data));

        key_ptr = &v5plainkey;
        kvno = key_data->key_data_kvno;

        retval = krb5_dbe_encrypt_key_data(context, &new_master_keyblock,
                                           key_ptr, &keysalt, kvno,
                                           &new_key_data);
        if (retval)
            return retval;
        krb5_free_keyblock_contents(context, &v5plainkey);
        for (j = 0; j < key_data->key_data_ver; j++) {
            if (key_data->key_data_length[j])
                free(key_data->key_data_contents[j]);
        }
        *key_data = new_key_data;
    }
    assert(new_mkvno > 0);
    return krb5_dbe_update_mkvno(context, db_entry, new_mkvno);
}

/* Create temp file for new dump to be named ofile. */
static FILE *
create_ofile(char *ofile, char **tmpname)
{
    int fd = -1;
    FILE *f;

    *tmpname = NULL;
    if (asprintf(tmpname, "%s-XXXXXX", ofile) < 0)
        goto error;

    fd = mkstemp(*tmpname);
    if (fd == -1)
        goto error;

    f = fdopen(fd, "w+");
    if (f != NULL)
        return f;

error:
    com_err(progname, errno, _("while allocating temporary filename dump"));
    if (fd >= 0)
        unlink(*tmpname);
    exit(1);
}

/* Rename new dump file into place. */
static void
finish_ofile(char *ofile, char **tmpname)
{
    if (rename(*tmpname, ofile) == -1) {
        com_err(progname, errno, _("while renaming dump file into place"));
        exit(1);
    }
    free(*tmpname);
    *tmpname = NULL;
}

/* Create the .dump_ok file. */
static krb5_boolean
prep_ok_file(krb5_context context, char *file_name, int *fd_out)
{
    static char ok[] = ".dump_ok";
    krb5_error_code retval;
    char *file_ok = NULL;
    int fd = -1;
    krb5_boolean success = FALSE;

    *fd_out = -1;

    if (asprintf(&file_ok, "%s%s", file_name, ok) < 0) {
        com_err(progname, ENOMEM, _("while allocating dump_ok filename"));
        goto cleanup;
    }

    fd = open(file_ok, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) {
        com_err(progname, errno, _("while creating 'ok' file, '%s'"), file_ok);
        goto cleanup;
    }
    retval = krb5_lock_file(context, fd, KRB5_LOCKMODE_EXCLUSIVE);
    if (retval) {
        com_err(progname, retval, _("while locking 'ok' file, '%s'"), file_ok);
        goto cleanup;
    }

    *fd_out = fd;
    fd = -1;
    success = TRUE;

cleanup:
    free(file_ok);
    if (fd != -1)
        close(fd);
    if (!success)
        exit_status++;
    return success;
}

/*
 * Update the "ok" file.
 */
static void
update_ok_file(krb5_context context, int fd)
{
    write(fd, "", 1);
    krb5_lock_file(context, fd, KRB5_LOCKMODE_UNLOCK);
    close(fd);
}

/* Return true if a principal name matches a regular expression or string. */
static int
name_matches(char *name, struct dump_args *args)
{
#if HAVE_REGCOMP
    regex_t reg;
    regmatch_t rmatch;
    int st;
    char errmsg[BUFSIZ];
#elif   HAVE_REGEXP_H
    char regexp_buffer[RE_BUF_SIZE];
#elif   HAVE_RE_COMP
    extern char *re_comp();
    char *re_result;
#endif  /* HAVE_RE_COMP */
    int i, match;

    /* Check each regular expression in args. */
    match = args->nnames ? 0 : 1;
    for (i = 0; i < args->nnames && !match; i++) {
#if HAVE_REGCOMP
        /* Compile the regular expression. */
        st = regcomp(&reg, args->names[i], REG_EXTENDED);
        if (st) {
            regerror(st, &reg, errmsg, sizeof(errmsg));
            fprintf(stderr, _("%s: regular expression error: %s\n"), progname,
                    errmsg);
            break;
        }
        /* See if we have a match. */
        st = regexec(&reg, name, 1, &rmatch, 0);
        if (st == 0) {
            /* See if it matches the whole name. */
            if (rmatch.rm_so == 0 && (size_t)rmatch.rm_eo == strlen(name))
                match = 1;
        } else if (st != REG_NOMATCH) {
            regerror(st, &reg, errmsg, sizeof(errmsg));
            fprintf(stderr, _("%s: regular expression match error: %s\n"),
                    progname, errmsg);
            break;
        }
        regfree(&reg);
#elif HAVE_REGEXP_H
        /* Compile the regular expression. */
        compile(args->names[i], regexp_buffer, &regexp_buffer[RE_BUF_SIZE],
                '\0');
        if (step(name, regexp_buffer)) {
            if (loc1 == name && loc2 == &name[strlen(name)])
                match = 1;
        }
#elif HAVE_RE_COMP
        /* Compile the regular expression. */
        re_result = re_comp(args->names[i]);
        if (re_result) {
            fprintf(stderr, _("%s: regular expression error: %s\n"), progname,
                    re_result);
            break;
        }
        if (re_exec(name))
            match = 1;
#else /* HAVE_RE_COMP */
        /* If no regular expression support, then just compare the strings. */
        if (!strcmp(args->names[i], name))
            match = 1;
#endif /* HAVE_REGCOMP */
    }
    return match;
}

/* Output "-1" if len is 0; otherwise output len bytes of data in hex. */
static void
dump_octets_or_minus1(FILE *fp, unsigned char *data, size_t len)
{
    if (len > 0) {
        for (; len > 0; len--)
            fprintf(fp, "%02x", *data++);
    } else {
        fprintf(fp, "-1");
    }
}

/*
 * Dump TL data; common to principals and policies.
 *
 * If filter_kadm then the KRB5_TL_KADM_DATA (where a principal's policy
 * name is stored) is filtered out.  This is for dump formats that don't
 * support policies.
 */
static void
dump_tl_data(FILE *ofile, krb5_tl_data *tlp, krb5_boolean filter_kadm)
{
    for (; tlp != NULL; tlp = tlp->tl_data_next) {
        if (tlp->tl_data_type == KRB5_TL_KADM_DATA && filter_kadm)
            continue;
        fprintf(ofile, "\t%d\t%d\t", (int)tlp->tl_data_type,
                (int)tlp->tl_data_length);
        dump_octets_or_minus1(ofile, tlp->tl_data_contents,
                              tlp->tl_data_length);
    }
}

/* Dump a principal entry in krb5 beta 7 format.  Omit kadmin tl-data if kadm
 * is false. */
static krb5_error_code
k5beta7_common(krb5_context context, krb5_db_entry *entry,
               const char *name, FILE *fp, krb5_boolean verbose,
               krb5_boolean omit_nra, krb5_boolean kadm)
{
    krb5_tl_data *tlp;
    krb5_key_data *kdata;
    int counter, skip, i;

    /*
     * The dump format is as follows:
     *      len strlen(name) n_tl_data n_key_data e_length
     *      name
     *      attributes max_life max_renewable_life expiration
     *      pw_expiration last_success last_failed fail_auth_count
     *      n_tl_data*[type length <contents>]
     *      n_key_data*[ver kvno ver*(type length <contents>)]
     *      <e_data>
     * Fields which are not encapsulated by angle-brackets are to appear
     * verbatim.  A bracketed field's absence is indicated by a -1 in its
     * place.
     */

    /* Make sure that the tagged list is reasonably correct. */
    counter = skip = 0;
    for (tlp = entry->tl_data; tlp; tlp = tlp->tl_data_next) {
        /* Don't dump tl data types we know aren't understood by earlier
         * versions. */
        if (tlp->tl_data_type == KRB5_TL_KADM_DATA && !kadm)
            skip++;
        else
            counter++;
    }

    if (counter + skip != entry->n_tl_data) {
        fprintf(stderr, _("%s: tagged data list inconsistency for %s "
                          "(counted %d, stored %d)\n"), progname, name,
                counter + skip, (int)entry->n_tl_data);
        return EINVAL;
    }

    /* Write out header. */
    fprintf(fp, "princ\t%d\t%lu\t%d\t%d\t%d\t%s\t", (int)entry->len,
            (unsigned long)strlen(name), counter, (int)entry->n_key_data,
            (int)entry->e_length, name);
    fprintf(fp, "%d\t%d\t%d\t%u\t%u\t%u\t%u\t%d", entry->attributes,
            entry->max_life, entry->max_renewable_life,
            (unsigned int)entry->expiration,
            (unsigned int)entry->pw_expiration,
            (unsigned int)(omit_nra ? 0 : entry->last_success),
            (unsigned int)(omit_nra ? 0 : entry->last_failed),
            omit_nra ? 0 : entry->fail_auth_count);

    /* Write out tagged data. */
    dump_tl_data(fp, entry->tl_data, !kadm);
    fprintf(fp, "\t");

    /* Write out key data. */
    for (counter = 0; counter < entry->n_key_data; counter++) {
        kdata = &entry->key_data[counter];
        fprintf(fp, "%d\t%d\t", (int)kdata->key_data_ver,
                (int)kdata->key_data_kvno);
        for (i = 0; i < kdata->key_data_ver; i++) {
            fprintf(fp, "%d\t%d\t", kdata->key_data_type[i],
                    kdata->key_data_length[i]);
            dump_octets_or_minus1(fp, kdata->key_data_contents[i],
                                  kdata->key_data_length[i]);
            fprintf(fp, "\t");
        }
    }

    /* Write out extra data. */
    dump_octets_or_minus1(fp, entry->e_data, entry->e_length);

    /* Write trailer. */
    fprintf(fp, ";\n");

    if (verbose)
        fprintf(stderr, "%s\n", name);

    return 0;
}

/* Output a dump record in krb5b7 format. */
static krb5_error_code
dump_k5beta7_princ(krb5_context context, krb5_db_entry *entry,
                   const char *name, FILE *fp, krb5_boolean verbose,
                   krb5_boolean omit_nra)
{
    return k5beta7_common(context, entry, name, fp, verbose, omit_nra, FALSE);
}

static krb5_error_code
dump_k5beta7_princ_withpolicy(krb5_context context, krb5_db_entry *entry,
                              const char *name, FILE *fp, krb5_boolean verbose,
                              krb5_boolean omit_nra)
{
    return k5beta7_common(context, entry, name, fp, verbose, omit_nra, TRUE);
}

static void
dump_k5beta7_policy(void *data, osa_policy_ent_t entry)
{
    struct dump_args *arg = data;

    fprintf(arg->ofile, "policy\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n", entry->name,
            entry->pw_min_life, entry->pw_max_life, entry->pw_min_length,
            entry->pw_min_classes, entry->pw_history_num, 0);
}

static void
dump_r1_8_policy(void *data, osa_policy_ent_t entry)
{
    struct dump_args *arg = data;

    fprintf(arg->ofile, "policy\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
            entry->name, entry->pw_min_life, entry->pw_max_life,
            entry->pw_min_length, entry->pw_min_classes, entry->pw_history_num,
            0, entry->pw_max_fail, entry->pw_failcnt_interval,
            entry->pw_lockout_duration);
}

static void
dump_r1_11_policy(void *data, osa_policy_ent_t entry)
{
    struct dump_args *arg = data;

    fprintf(arg->ofile, "policy\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
            "%d\t%d\t%d\t%s\t%d", entry->name, entry->pw_min_life,
            entry->pw_max_life, entry->pw_min_length, entry->pw_min_classes,
            entry->pw_history_num, 0, entry->pw_max_fail,
            entry->pw_failcnt_interval, entry->pw_lockout_duration,
            entry->attributes, entry->max_life, entry->max_renewable_life,
            entry->allowed_keysalts ? entry->allowed_keysalts : "-",
            entry->n_tl_data);

    dump_tl_data(arg->ofile, entry->tl_data, FALSE);
    fprintf(arg->ofile, "\n");
}

static krb5_error_code
dump_iterator(void *ptr, krb5_db_entry *entry)
{
    krb5_error_code ret;
    struct dump_args *args = ptr;
    char *name;

    ret = krb5_unparse_name(args->context, entry->princ, &name);
    if (ret) {
        com_err(progname, ret, _("while unparsing principal name"));
        return ret;
    }

    /* Re-encode the keys in the new master key, if necessary. */
    if (mkey_convert) {
        ret = master_key_convert(args->context, entry);
        if (ret) {
            com_err(progname, ret, _("while converting %s to new master key"),
                    name);
            goto cleanup;
        }
    }

    /* Don't dump this entry if we have match strings and it doesn't match. */
    if (args->nnames > 0 && !name_matches(name, args))
        goto cleanup;

    ret = args->dump->dump_princ(args->context, entry, name, args->ofile,
                                 args->verbose, args->omit_nra);

cleanup:
    free(name);
    return ret;
}

static inline void
load_err(const char *fname, int lineno, const char *msg)
{
    fprintf(stderr, _("%s(%d): %s\n"), fname, lineno, msg);
}

/* Read a string of bytes.  Increment *lp for each newline.  Return 0 on
 * success, 1 on failure. */
static int
read_string(FILE *f, char *buf, int len, int *lp)
{
    int c, i;

    for (i = 0; i < len; i++) {
        c = fgetc(f);
        if (c < 0)
            return 1;
        if (c == '\n')
            (*lp)++;
        buf[i] = c;
    }
    buf[len] = '\0';
    return 0;
}

/* Read a string of two-character representations of bytes. */
static int
read_octet_string(FILE *f, unsigned char *buf, int len)
{
    int c, i;

    for (i = 0; i < len; i++) {
        if (fscanf(f, "%02x", &c) != 1)
            return 1;
        buf[i] = c;
    }
    return 0;
}

/* Read the end of a dumpfile record. */
static void
read_record_end(FILE *f, const char *fn, int lineno)
{
    int ch;

    if ((ch = fgetc(f)) != ';' || (ch = fgetc(f)) != '\n') {
        fprintf(stderr, _("%s(%d): ignoring trash at end of line: "), fn,
                lineno);
        while (ch != '\n') {
            putc(ch, stderr);
            ch = fgetc(f);
        }
        putc(ch, stderr);
    }
}

/* Allocate and form a TL data list of a desired size. */
static int
alloc_tl_data(krb5_int16 n_tl_data, krb5_tl_data **tldp)
{
    krb5_tl_data **tlp = tldp;
    int i;

    for (i = 0; i < n_tl_data; i++) {
        *tlp = calloc(1, sizeof(krb5_tl_data));
        if (*tlp == NULL)
            return ENOMEM; /* caller cleans up */
        tlp = &((*tlp)->tl_data_next);
    }

    return 0;
}

/* If len is zero, read the string "-1" from fp.  Otherwise allocate space and
 * read len octets.  Return 0 on success, 1 on failure. */
static int
read_octets_or_minus1(FILE *fp, size_t len, unsigned char **out)
{
    int ival;
    unsigned char *buf;

    *out = NULL;
    if (len == 0)
        return fscanf(fp, "%d", &ival) != 1 || ival != -1;
    buf = malloc(len);
    if (buf == NULL)
        return 1;
    if (read_octet_string(fp, buf, len)) {
        free(buf);
        return 1;
    }
    *out = buf;
    return 0;
}

/* Read TL data for a principal or policy.  Print an error and return -1 on
 * failure. */
static int
process_tl_data(const char *fname, FILE *filep, int lineno,
                krb5_tl_data *tl_data)
{
    krb5_tl_data *tl;
    int nread, i1;
    unsigned int u1;

    for (tl = tl_data; tl; tl = tl->tl_data_next) {
        nread = fscanf(filep, "%d\t%u\t", &i1, &u1);
        if (nread != 2) {
            load_err(fname, lineno,
                     _("cannot read tagged data type and length"));
            return EINVAL;
        }
        if (i1 < INT16_MIN || i1 > INT16_MAX || u1 > UINT16_MAX) {
            load_err(fname, lineno, _("data type or length overflowed"));
            return EINVAL;
        }
        tl->tl_data_type = i1;
        tl->tl_data_length = u1;
        if (read_octets_or_minus1(filep, tl->tl_data_length,
                                  &tl->tl_data_contents)) {
            load_err(fname, lineno, _("cannot read tagged data contents"));
            return EINVAL;
        }
    }

    return 0;
}

/* Read a beta 7 entry and add it to the database.  Return -1 for end of file,
 * 0 for success and 1 for failure. */
static int
process_k5beta7_princ(krb5_context context, const char *fname, FILE *filep,
                      krb5_boolean verbose, int *linenop)
{
    int retval, nread, i, j;
    krb5_db_entry *dbentry;
    int t1, t2, t3, t4;
    unsigned int u1, u2, u3, u4, u5;
    char *name = NULL;
    krb5_key_data *kp = NULL, *kd;
    krb5_tl_data *tl;
    krb5_error_code ret;

    dbentry = calloc(1, sizeof(*dbentry));
    if (dbentry == NULL)
        return 1;
    (*linenop)++;
    nread = fscanf(filep, "%u\t%u\t%u\t%u\t%u\t", &u1, &u2, &u3, &u4, &u5);
    if (nread == EOF) {
        retval = -1;
        goto cleanup;
    }
    if (nread != 5) {
        load_err(fname, *linenop, _("cannot match size tokens"));
        goto fail;
    }

    /* Get memory for flattened principal name */
    if (u2 > UINT_MAX / 2) {
        load_err(fname, *linenop, _("cannot allocate principal (too large)"));
        goto fail;
    }
    name = malloc(u2 + 1);
    if (name == NULL)
        goto fail;

    /* Get memory for and form tagged data linked list */
    if (u3 > UINT16_MAX) {
        load_err(fname, *linenop, _("cannot allocate tl_data (too large)"));
        goto fail;
    }
    if (alloc_tl_data(u3, &dbentry->tl_data))
        goto fail;
    dbentry->n_tl_data = u3;

    /* Get memory for key list */
    if (u4 > INT16_MAX) {
        load_err(fname, *linenop, _("invalid key_data size"));
        goto fail;
    }
    if (u4 && (kp = calloc(u4, sizeof(krb5_key_data))) == NULL)
        goto fail;

    dbentry->len = u1;
    dbentry->n_key_data = u4;
    dbentry->e_length = u5;

    if (kp != NULL) {
        dbentry->key_data = kp;
        kp = NULL;
    }

    /* Read in and parse the principal name */
    if (read_string(filep, name, u2, linenop)) {
        load_err(fname, *linenop, _("cannot read name string"));
        goto fail;
    }
    ret = krb5_parse_name(context, name, &dbentry->princ);
    if (ret) {
        com_err(progname, ret, _("while parsing name %s"), name);
        goto fail;
    }

    /* Get the fixed principal attributes */
    nread = fscanf(filep, "%d\t%d\t%d\t%u\t%u\t%d\t%d\t%d\t",
                   &t1, &t2, &t3, &u1, &u2, &u3, &u4, &u5);
    if (nread != 8) {
        load_err(fname, *linenop, _("cannot read principal attributes"));
        goto fail;
    }
    dbentry->attributes = t1;
    dbentry->max_life = t2;
    dbentry->max_renewable_life = t3;
    dbentry->expiration = u1;
    dbentry->pw_expiration = u2;
    dbentry->last_success = u3;
    dbentry->last_failed = u4;
    dbentry->fail_auth_count = u5;
    dbentry->mask = KADM5_LOAD | KADM5_PRINCIPAL | KADM5_ATTRIBUTES |
        KADM5_MAX_LIFE | KADM5_MAX_RLIFE |
        KADM5_PRINC_EXPIRE_TIME | KADM5_PW_EXPIRATION | KADM5_LAST_SUCCESS |
        KADM5_LAST_FAILED | KADM5_FAIL_AUTH_COUNT;

    /* Read tagged data. */
    if (dbentry->n_tl_data) {
        if (process_tl_data(fname, filep, *linenop, dbentry->tl_data))
            goto fail;
        for (tl = dbentry->tl_data; tl; tl = tl->tl_data_next) {
            /* test to set mask fields */
            if (tl->tl_data_type == KRB5_TL_KADM_DATA) {
                XDR xdrs;
                osa_princ_ent_rec osa_princ_ent;

                /*
                 * Assuming aux_attributes will always be
                 * there
                 */
                dbentry->mask |= KADM5_AUX_ATTRIBUTES;

                /* test for an actual policy reference */
                memset(&osa_princ_ent, 0, sizeof(osa_princ_ent));
                xdrmem_create(&xdrs, (char *)tl->tl_data_contents,
                              tl->tl_data_length, XDR_DECODE);
                if (xdr_osa_princ_ent_rec(&xdrs, &osa_princ_ent)) {
                    if ((osa_princ_ent.aux_attributes & KADM5_POLICY) &&
                        osa_princ_ent.policy != NULL)
                        dbentry->mask |= KADM5_POLICY;
                    kdb_free_entry(NULL, NULL, &osa_princ_ent);
                }
                xdr_destroy(&xdrs);
            }
        }
        dbentry->mask |= KADM5_TL_DATA;
    }

    /* Get the key data. */
    for (i = 0; i < dbentry->n_key_data; i++) {
        kd = &dbentry->key_data[i];
        nread = fscanf(filep, "%d\t%d\t", &t1, &t2);
        if (nread != 2) {
            load_err(fname, *linenop, _("cannot read key size and version"));
            goto fail;
        }
        if (t1 > KRB5_KDB_V1_KEY_DATA_ARRAY) {
            load_err(fname, *linenop, _("unsupported key_data_ver version"));
            goto fail;
        }
        if (t2 < 0 || t2 > UINT16_MAX) {
            load_err(fname, *linenop, _("invalid kvno"));
            goto fail;
        }

        kd->key_data_ver = t1;
        kd->key_data_kvno = t2;

        for (j = 0; j < t1; j++) {
            nread = fscanf(filep, "%d\t%d\t", &t3, &t4);
            if (nread != 2 || t4 < 0 || t4 > UINT16_MAX) {
                load_err(fname, *linenop,
                         _("cannot read key type and length"));
                goto fail;
            }
            kd->key_data_type[j] = t3;
            kd->key_data_length[j] = t4;
            if (read_octets_or_minus1(filep, t4, &kd->key_data_contents[j])) {
                load_err(fname, *linenop, _("cannot read key data"));
                goto fail;
            }
        }
    }
    if (dbentry->n_key_data)
        dbentry->mask |= KADM5_KEY_DATA;

    /* Get the extra data */
    if (read_octets_or_minus1(filep, dbentry->e_length, &dbentry->e_data)) {
        load_err(fname, *linenop, _("cannot read extra data"));
        goto fail;
    }

    /* Finally, find the end of the record. */
    read_record_end(filep, fname, *linenop);

    ret = krb5_db_put_principal(context, dbentry);
    if (ret) {
        com_err(progname, ret, _("while storing %s"), name);
        goto fail;
    }

    if (verbose)
        fprintf(stderr, "%s\n", name);
    retval = 0;

cleanup:
    free(kp);
    free(name);
    krb5_db_free_principal(context, dbentry);
    return retval;

fail:
    retval = 1;
    goto cleanup;
}

static int
process_k5beta7_policy(krb5_context context, const char *fname, FILE *filep,
                       krb5_boolean verbose, int *linenop)
{
    osa_policy_ent_rec rec;
    char namebuf[1024];
    unsigned int refcnt;
    int nread, ret;

    memset(&rec, 0, sizeof(rec));

    (*linenop)++;
    rec.name = namebuf;

    nread = fscanf(filep, "%1023s\t%u\t%u\t%u\t%u\t%u\t%u", rec.name,
                   &rec.pw_min_life, &rec.pw_max_life, &rec.pw_min_length,
                   &rec.pw_min_classes, &rec.pw_history_num, &refcnt);
    if (nread == EOF)
        return -1;
    if (nread != 7) {
        fprintf(stderr, _("cannot parse policy (%d read)\n"), nread);
        return 1;
    }

    ret = krb5_db_create_policy(context, &rec);
    if (ret)
        ret = krb5_db_put_policy(context, &rec);
    if (ret) {
        com_err(progname, ret, _("while creating policy"));
        return 1;
    }
    if (verbose)
        fprintf(stderr, _("created policy %s\n"), rec.name);

    return 0;
}

static int
process_r1_8_policy(krb5_context context, const char *fname, FILE *filep,
                    krb5_boolean verbose, int *linenop)
{
    osa_policy_ent_rec rec;
    char namebuf[1024];
    unsigned int refcnt;
    int nread, ret;

    memset(&rec, 0, sizeof(rec));

    (*linenop)++;
    rec.name = namebuf;

    nread = fscanf(filep, "%1023s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u",
                   rec.name, &rec.pw_min_life, &rec.pw_max_life,
                   &rec.pw_min_length, &rec.pw_min_classes,
                   &rec.pw_history_num, &refcnt, &rec.pw_max_fail,
                   &rec.pw_failcnt_interval, &rec.pw_lockout_duration);
    if (nread == EOF)
        return -1;
    if (nread != 10) {
        fprintf(stderr, _("cannot parse policy (%d read)\n"), nread);
        return 1;
    }

    ret = krb5_db_create_policy(context, &rec);
    if (ret)
        ret = krb5_db_put_policy(context, &rec);
    if (ret) {
        com_err(progname, ret, _("while creating policy"));
        return 1;
    }
    if (verbose)
        fprintf(stderr, "created policy %s\n", rec.name);

    return 0;
}

static int
process_r1_11_policy(krb5_context context, const char *fname, FILE *filep,
                     krb5_boolean verbose, int *linenop)
{
    osa_policy_ent_rec rec;
    krb5_tl_data *tl, *tl_next;
    char namebuf[1024];
    char keysaltbuf[KRB5_KDB_MAX_ALLOWED_KS_LEN + 1];
    unsigned int refcnt;
    int nread, c, ret = 0;

    memset(&rec, 0, sizeof(rec));

    (*linenop)++;
    rec.name = namebuf;

    /*
     * Due to a historical error, iprop dumps use the same version before and
     * after the 1.11 policy extensions.  So we need to accept both 1.8-format
     * and 1.11-format policy entries.  Begin by reading the 1.8 fields.
     */
    nread = fscanf(filep, "%1023s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u",
                   rec.name, &rec.pw_min_life, &rec.pw_max_life,
                   &rec.pw_min_length, &rec.pw_min_classes,
                   &rec.pw_history_num, &refcnt, &rec.pw_max_fail,
                   &rec.pw_failcnt_interval, &rec.pw_lockout_duration);
    if (nread == EOF)
        return -1;
    if (nread != 10) {
        fprintf(stderr, _("cannot parse policy (%d read)\n"), nread);
        return 1;
    }

    /* The next character should be a newline (1.8) or a tab (1.11). */
    c = getc(filep);
    if (c == EOF)
        return -1;
    if (c != '\n') {
        /* Read the additional 1.11-format fields. */
        rec.allowed_keysalts = keysaltbuf;
        nread = fscanf(filep, "%u\t%u\t%u\t"
                       K5CONST_WIDTH_SCANF_STR(KRB5_KDB_MAX_ALLOWED_KS_LEN)
                       "\t%hd", &rec.attributes, &rec.max_life,
                       &rec.max_renewable_life, rec.allowed_keysalts,
                       &rec.n_tl_data);
        if (nread == EOF)
            return -1;
        if (nread != 5) {
            fprintf(stderr, _("cannot parse policy (%d read)\n"), nread);
            return 1;
        }

        if (rec.allowed_keysalts && !strcmp(rec.allowed_keysalts, "-"))
            rec.allowed_keysalts = NULL;

        /* Get TL data */
        ret = alloc_tl_data(rec.n_tl_data, &rec.tl_data);
        if (ret)
            goto cleanup;

        ret = process_tl_data(fname, filep, *linenop, rec.tl_data);
        if (ret)
            goto cleanup;
    }

    ret = krb5_db_create_policy(context, &rec);
    if (ret)
        ret = krb5_db_put_policy(context, &rec);
    if (ret) {
        com_err(progname, ret, _("while creating policy"));
        goto cleanup;
    }
    if (verbose)
        fprintf(stderr, "created policy %s\n", rec.name);

cleanup:
    for (tl = rec.tl_data; tl; tl = tl_next) {
        tl_next = tl->tl_data_next;
        free(tl->tl_data_contents);
        free(tl);
    }
    return ret ? 1 : 0;
}

/* Read a record which is tagged with "princ" or "policy", calling princfn
 * or policyfn as appropriate. */
static int
process_tagged(krb5_context context, const char *fname, FILE *filep,
               krb5_boolean verbose, int *linenop, load_func princfn,
               load_func policyfn)
{
    int nread;
    char rectype[100];

    nread = fscanf(filep, "%99s\t", rectype);
    if (nread == EOF)
        return -1;
    if (nread != 1)
        return 1;
    if (strcmp(rectype, "princ") == 0)
        return (*princfn)(context, fname, filep, verbose, linenop);
    if (strcmp(rectype, "policy") == 0)
        return (*policyfn)(context, fname, filep, verbose, linenop);
    if (strcmp(rectype, "End") == 0)  /* Only expected for OV format */
        return -1;

    fprintf(stderr, _("unknown record type \"%s\"\n"), rectype);
    return 1;
}

static int
process_k5beta7_record(krb5_context context, const char *fname, FILE *filep,
                       krb5_boolean verbose, int *linenop)
{
    return process_tagged(context, fname, filep, verbose, linenop,
                          process_k5beta7_princ, process_k5beta7_policy);
}

static int
process_r1_8_record(krb5_context context, const char *fname, FILE *filep,
                    krb5_boolean verbose, int *linenop)
{
    return process_tagged(context, fname, filep, verbose, linenop,
                          process_k5beta7_princ, process_r1_8_policy);
}

static int
process_r1_11_record(krb5_context context, const char *fname, FILE *filep,
                     krb5_boolean verbose, int *linenop)
{
    return process_tagged(context, fname, filep, verbose, linenop,
                          process_k5beta7_princ, process_r1_11_policy);
}

dump_version beta7_version = {
    "Kerberos version 5",
    "kdb5_util load_dump version 4\n",
    0,
    0,
    0,
    dump_k5beta7_princ,
    dump_k5beta7_policy,
    process_k5beta7_record,
};
dump_version r1_3_version = {
    "Kerberos version 5 release 1.3",
    "kdb5_util load_dump version 5\n",
    0,
    0,
    0,
    dump_k5beta7_princ_withpolicy,
    dump_k5beta7_policy,
    process_k5beta7_record,
};
dump_version r1_8_version = {
    "Kerberos version 5 release 1.8",
    "kdb5_util load_dump version 6\n",
    0,
    0,
    0,
    dump_k5beta7_princ_withpolicy,
    dump_r1_8_policy,
    process_r1_8_record,
};
dump_version r1_11_version = {
    "Kerberos version 5 release 1.11",
    "kdb5_util load_dump version 7\n",
    0,
    0,
    0,
    dump_k5beta7_princ_withpolicy,
    dump_r1_11_policy,
    process_r1_11_record,
};
dump_version iprop_version = {
    "Kerberos iprop version",
    "iprop",
    0,
    1,
    0,
    dump_k5beta7_princ_withpolicy,
    dump_k5beta7_policy,
    process_k5beta7_record,
};
dump_version ipropx_1_version = {
    "Kerberos iprop extensible version",
    "ipropx",
    0,
    1,
    1,
    dump_k5beta7_princ_withpolicy,
    dump_r1_11_policy,
    process_r1_11_record,
};

/* Read the dump header.  Return 1 on success, 0 if the file is not a
 * recognized iprop dump format. */
static int
parse_iprop_header(char *buf, dump_version **dv, kdb_last_t *last)
{
    char head[128];
    int nread;
    uint32_t u[4];
    uint32_t *up = &u[0];

    nread = sscanf(buf, "%127s %u %u %u %u", head, &u[0], &u[1], &u[2], &u[3]);
    if (nread < 1)
        return 0;

    if (!strcmp(head, ipropx_1_version.header)) {
        if (nread != 5)
            return 0;
        if (u[0] == IPROPX_VERSION_0) {
            *dv = &iprop_version;
        } else if (u[0] == IPROPX_VERSION_1) {
            *dv = &ipropx_1_version;
        } else {
            fprintf(stderr, _("%s: Unknown iprop dump version %d\n"), progname,
                    u[0]);
            return 0;
        }
        up = &u[1];
    } else if (!strcmp(head, iprop_version.header)) {
        if (nread != 4)
            return 0;
        *dv = &iprop_version;
    } else {
        fprintf(stderr, "Invalid iprop header\n");
        return 0;
    }

    last->last_sno = *up++;
    last->last_time.seconds = *up++;
    last->last_time.useconds = *up++;
    return 1;
}

/* Return true if the serial number and timestamp in an existing dump file is
 * in the ulog. */
static krb5_boolean
current_dump_sno_in_ulog(krb5_context context, const char *ifile)
{
    update_status_t status;
    dump_version *junk;
    kdb_last_t last;
    char buf[BUFSIZ], *r;
    FILE *f;

    f = fopen(ifile, "r");
    if (f == NULL)
        return 0;              /* aliasing other errors to ENOENT here is OK */

    r = fgets(buf, sizeof(buf), f);
    fclose(f);
    if (r == NULL)
        return errno ? -1 : 0;

    if (!parse_iprop_header(buf, &junk, &last))
        return 0;

    status = ulog_get_sno_status(context, &last);
    return status == UPDATE_OK || status == UPDATE_NIL;
}

void
dump_db(int argc, char **argv)
{
    FILE *f;
    struct dump_args args;
    char *ofile = NULL, *tmpofile = NULL, *new_mkey_file = NULL;
    krb5_error_code ret, retval;
    dump_version *dump;
    int aindex, ok_fd = -1;
    bool_t dump_sno = FALSE;
    kdb_log_context *log_ctx;
    unsigned int ipropx_version = IPROPX_VERSION_0;
    krb5_kvno kt_kvno;
    krb5_boolean conditional = FALSE;
    kdb_last_t last;
    krb5_flags iterflags = 0;

    /* Parse the arguments. */
    dump = &r1_11_version;
    args.verbose = FALSE;
    args.omit_nra = FALSE;
    mkey_convert = FALSE;
    log_ctx = util_context->kdblog_context;

    /*
     * Parse the qualifiers.
     */
    for (aindex = 1; aindex < argc; aindex++) {
        if (!strcmp(argv[aindex], "-b7")) {
            dump = &beta7_version;
        } else if (!strcmp(argv[aindex], "-ov")) {
            fprintf(stderr, _("OV dump format not supported\n"));
            goto error;
        } else if (!strcmp(argv[aindex], "-r13")) {
            dump = &r1_3_version;
        } else if (!strcmp(argv[aindex], "-r18")) {
            dump = &r1_8_version;
        } else if (!strncmp(argv[aindex], "-i", 2)) {
            /* Intentionally undocumented - only used by kadmin. */
            if (log_ctx && log_ctx->iproprole) {
                /* ipropx_version is the maximum version acceptable. */
                ipropx_version = atoi(argv[aindex] + 2);
                dump = ipropx_version ? &ipropx_1_version : &iprop_version;
                /*
                 * dump_sno is used to indicate if the serial number should be
                 * populated in the output file to be used later by iprop for
                 * updating the replica's update log when loading.
                 */
                dump_sno = TRUE;
                /* FLAG_OMIT_NRA is set to indicate that non-replicated
                 * attributes should be omitted. */
                args.omit_nra = TRUE;
            } else {
                fprintf(stderr, _("Iprop not enabled\n"));
                goto error;
            }
        } else if (!strcmp(argv[aindex], "-c")) {
            conditional = 1;
        } else if (!strcmp(argv[aindex], "-verbose")) {
            args.verbose = TRUE;
        } else if (!strcmp(argv[aindex], "-mkey_convert")) {
            mkey_convert = 1;
        } else if (!strcmp(argv[aindex], "-new_mkey_file")) {
            new_mkey_file = argv[++aindex];
            mkey_convert = 1;
        } else if (!strcmp(argv[aindex], "-rev")) {
            iterflags |= KRB5_DB_ITER_REV;
        } else if (!strcmp(argv[aindex], "-recurse")) {
            iterflags |= KRB5_DB_ITER_RECURSE;
        } else {
            break;
        }
    }

    args.names = NULL;
    args.nnames = 0;
    if (aindex < argc) {
        ofile = argv[aindex];
        aindex++;
        if (aindex < argc) {
            args.names = &argv[aindex];
            args.nnames = argc - aindex;
        }
    }

    /* If a conditional ipropx dump we check if the existing dump is
     * good enough. */
    if (ofile != NULL && conditional) {
        if (!dump->iprop) {
            com_err(progname, 0,
                    _("Conditional dump is an undocumented option for "
                      "use only for iprop dumps"));
            goto error;
        }
        if (current_dump_sno_in_ulog(util_context, ofile))
            return;
    }

    /*
     * Make sure the database is open.  The policy database only has
     * to be opened if we try a dump that uses it.
     */
    if (!dbactive) {
        com_err(progname, 0, _("Database not currently opened!"));
        goto error;
    }

    /*
     * If we're doing a master key conversion, set up for it.
     */
    if (mkey_convert) {
        if (!valid_master_key) {
            /* TRUE here means read the keyboard, but only once */
            retval = krb5_db_fetch_mkey(util_context, master_princ,
                                        master_keyblock.enctype, TRUE, FALSE,
                                        NULL, NULL, NULL, &master_keyblock);
            if (retval) {
                com_err(progname, retval, _("while reading master key"));
                exit(1);
            }
            retval = krb5_db_fetch_mkey_list(util_context, master_princ,
                                             &master_keyblock);
            if (retval) {
                com_err(progname, retval, _("while verifying master key"));
                exit(1);
            }
        }
        new_master_keyblock.enctype = global_params.enctype;
        if (new_master_keyblock.enctype == ENCTYPE_UNKNOWN)
            new_master_keyblock.enctype = DEFAULT_KDC_ENCTYPE;

        if (new_mkey_file) {
            if (global_params.mask & KADM5_CONFIG_KVNO)
                kt_kvno = global_params.kvno;
            else
                kt_kvno = IGNORE_VNO;

            retval = krb5_db_fetch_mkey(util_context, master_princ,
                                        new_master_keyblock.enctype, FALSE,
                                        FALSE, new_mkey_file, &kt_kvno, NULL,
                                        &new_master_keyblock);
            if (retval) {
                com_err(progname, retval, _("while reading new master key"));
                exit(1);
            }
        } else {
            printf(_("Please enter new master key....\n"));
            retval = krb5_db_fetch_mkey(util_context, master_princ,
                                        new_master_keyblock.enctype, TRUE,
                                        TRUE, NULL, NULL, NULL,
                                        &new_master_keyblock);
            if (retval) {
                com_err(progname, retval, _("while reading new master key"));
                exit(1);
            }
        }
        /* Get new master key vno that will be used to protect princs. */
        new_mkvno = get_next_kvno(util_context, master_entry);
    }

    ret = 0;

    if (ofile != NULL && strcmp(ofile, "-")) {
        /* Discourage accidental dumping to filenames beginning with '-'. */
        if (ofile[0] == '-')
            usage();
        if (!prep_ok_file(util_context, ofile, &ok_fd))
            return;             /* prep_ok_file() bumps exit_status */
        f = create_ofile(ofile, &tmpofile);
        if (f == NULL) {
            com_err(progname, errno, _("while opening %s for writing"), ofile);
            goto error;
        }
    } else {
        f = stdout;
    }

    args.ofile = f;
    args.context = util_context;
    args.dump = dump;
    fprintf(args.ofile, "%s", dump->header);

    if (dump_sno) {
        ret = ulog_get_last(util_context, &last);
        if (ret) {
            com_err(progname, ret, _("while reading update log header"));
            goto error;
        }
        if (ipropx_version)
            fprintf(f, " %u", IPROPX_VERSION);
        fprintf(f, " %u", last.last_sno);
        fprintf(f, " %u", last.last_time.seconds);
        fprintf(f, " %u", last.last_time.useconds);
    }

    if (dump->header[strlen(dump->header)-1] != '\n')
        fputc('\n', args.ofile);

    ret = krb5_db_iterate(util_context, NULL, dump_iterator, &args, iterflags);
    if (ret) {
        com_err(progname, ret, _("performing %s dump"), dump->name);
        goto error;
    }

    /* Don't dump policies if specific principal entries were requested. */
    if (dump->dump_policy != NULL && args.nnames == 0) {
        ret = krb5_db_iter_policy(util_context, "*", dump->dump_policy, &args);
        if (ret) {
            com_err(progname, ret, _("performing %s dump"), dump->name);
            goto error;
        }
    }

    if (f != stdout) {
        fclose(f);
        finish_ofile(ofile, &tmpofile);
        update_ok_file(util_context, ok_fd);
    }
    return;

error:
    if (tmpofile != NULL)
        unlink(tmpofile);
    free(tmpofile);
    exit_status++;
}

/* Restore the database from any version dump file. */
static int
restore_dump(krb5_context context, char *dumpfile, FILE *f,
             krb5_boolean verbose, dump_version *dump)
{
    int err = 0;
    int lineno = 1;

    /* Process the records. */
    while (!(err = dump->load_record(context, dumpfile, f, verbose, &lineno)));
    if (err != -1) {
        fprintf(stderr, _("%s: error processing line %d of %s\n"), progname,
                lineno, dumpfile);
        return err;
    }
    return 0;
}

void
load_db(int argc, char **argv)
{
    krb5_error_code ret;
    FILE *f = NULL;
    char *dumpfile = NULL, *dbname, buf[BUFSIZ];
    dump_version *load = NULL;
    int aindex;
    kdb_log_context *log_ctx;
    kdb_last_t last;
    krb5_boolean db_locked = FALSE, temp_db_created = FALSE;
    krb5_boolean verbose = FALSE, update = FALSE, iprop_load = FALSE;

    /* Parse the arguments. */
    dbname = global_params.dbname;
    exit_status = 0;
    log_ctx = util_context->kdblog_context;

    for (aindex = 1; aindex < argc; aindex++) {
        if (!strcmp(argv[aindex], "-b7")){
            load = &beta7_version;
        } else if (!strcmp(argv[aindex], "-ov")) {
            fprintf(stderr, _("OV dump format not supported\n"));
            goto error;
        } else if (!strcmp(argv[aindex], "-r13")) {
            load = &r1_3_version;
        } else if (!strcmp(argv[aindex], "-r18")){
            load = &r1_8_version;
        } else if (!strcmp(argv[aindex], "-i")) {
            /* Intentionally undocumented - only used by kadmin. */
            if (log_ctx && log_ctx->iproprole) {
                load = &iprop_version;
                iprop_load = TRUE;
            } else {
                fprintf(stderr, _("Iprop not enabled\n"));
                goto error;
            }
        } else if (!strcmp(argv[aindex], "-verbose")) {
            verbose = TRUE;
        } else if (!strcmp(argv[aindex], "-update")){
            update = TRUE;
        } else if (!strcmp(argv[aindex], "-hash")) {
            if (!add_db_arg("hash=true")) {
                com_err(progname, ENOMEM, _("while parsing options"));
                goto error;
            }
        } else {
            break;
        }
    }
    if (argc - aindex != 1)
        usage();
    dumpfile = argv[aindex];

    /* Open the dumpfile. */
    if (dumpfile != NULL) {
        f = fopen(dumpfile, "r");
        if (f == NULL) {
            com_err(progname, errno, _("while opening %s"), dumpfile);
            goto error;
        }
    } else {
        f = stdin;
        dumpfile = _("standard input");
    }

    /* Auto-detect dump version if we weren't told, or verify if we were. */
    if (fgets(buf, sizeof(buf), f) == NULL) {
        fprintf(stderr, _("%s: can't read dump header in %s\n"), progname,
                dumpfile);
        goto error;
    }
    if (load) {
        /* Only check what we know; some headers only contain a prefix.
         * NB: this should work for ipropx even though load is iprop */
        if (strncmp(buf, load->header, strlen(load->header)) != 0) {
            fprintf(stderr, _("%s: dump header bad in %s\n"), progname,
                    dumpfile);
            goto error;
        }
    } else {
        if (strcmp(buf, beta7_version.header) == 0) {
            load = &beta7_version;
        } else if (strcmp(buf, r1_3_version.header) == 0) {
            load = &r1_3_version;
        } else if (strcmp(buf, r1_8_version.header) == 0) {
            load = &r1_8_version;
        } else if (strcmp(buf, r1_11_version.header) == 0) {
            load = &r1_11_version;
        } else {
            fprintf(stderr, _("%s: dump header bad in %s\n"), progname,
                    dumpfile);
            goto error;
        }
    }

    if (global_params.iprop_enabled &&
        ulog_map(util_context, global_params.iprop_logfile,
                 global_params.iprop_ulogsize)) {
        fprintf(stderr, _("Could not open iprop ulog\n"));
        goto error;
    }

    if (load->updateonly && !update) {
        fprintf(stderr, _("%s: dump version %s can only be loaded with the "
                          "-update flag\n"), progname, load->name);
        goto error;
    }

    /* If we are not in update mode, we create an alternate database and then
     * promote it to be the live db. */
    if (!update) {
        if (!add_db_arg("temporary")) {
            com_err(progname, ENOMEM, _("computing parameters for database"));
            goto error;
        }

        if (iprop_load && !add_db_arg("merge_nra")) {
            com_err(progname, ENOMEM, _("computing parameters for database"));
            goto error;
        }

        ret = krb5_db_create(util_context, db5util_db_args);
        if (ret) {
            com_err(progname, ret, _("while creating database"));
            goto error;
        }
        temp_db_created = TRUE;
    } else {
        /* Initialize the database. */
        ret = krb5_db_open(util_context, db5util_db_args,
                           KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_ADMIN);
        if (ret) {
            com_err(progname, ret, _("while opening database"));
            goto error;
        }

        /* Make sure the db is left unusable if the update fails, if the db
         * supports locking. */
        ret = krb5_db_lock(util_context, KRB5_DB_LOCKMODE_PERMANENT);
        if (ret == 0) {
            db_locked = TRUE;
        } else if (ret != KRB5_PLUGIN_OP_NOTSUPP) {
            com_err(progname, ret, _("while permanently locking database"));
            goto error;
        }
    }

    if (log_ctx != NULL && log_ctx->iproprole && !update) {
        /* Don't record updates we are making to the temporary DB.  We will
         * reinitialize or update the ulog header after promoting it. */
        log_ctx->iproprole = IPROP_REPLICA;
        if (iprop_load) {
            /* Parse the iprop header information. */
            if (!parse_iprop_header(buf, &load, &last))
                goto error;
        }
    }

    if (restore_dump(util_context, dumpfile ? dumpfile : _("standard input"),
                     f, verbose, load)) {
        fprintf(stderr, _("%s: %s restore failed\n"), progname, load->name);
        goto error;
    }

    if (db_locked && (ret = krb5_db_unlock(util_context))) {
        com_err(progname, ret, _("while unlocking database"));
        goto error;
    }

    if (!update) {
        /* Initialize the ulog header before promoting so we can't leave behind
         * the pre-load ulog state if we are killed just after promoting. */
        if (log_ctx != NULL && log_ctx->iproprole) {
            ret = ulog_init_header(util_context);
            if (ret) {
                com_err(progname, ret, _("while reinitializing update log"));
                goto error;
            }
        }

        ret = krb5_db_promote(util_context, db5util_db_args);
        /* Ignore a not supported error since there is nothing to do about it
         * anyway. */
        if (ret != 0 && ret != KRB5_PLUGIN_OP_NOTSUPP) {
            com_err(progname, ret,
                    _("while making newly loaded database live"));
            goto error;
        }

        if (log_ctx != NULL && log_ctx->iproprole) {
            /* Reinitialize the ulog header since we replaced the DB, and
             * record the iprop state if we received it. */
            ret = ulog_init_header(util_context);
            if (ret) {
                com_err(progname, ret, _("while reinitializing update log"));
                goto error;
            }
            if (iprop_load) {
                ret = ulog_set_last(util_context, &last);
                if (ret) {
                    com_err(progname, ret,
                            _("while writing update log header"));
                    goto error;
                }
            }
        }
    }

cleanup:
    /* If we created a temporary DB but didn't succeed, destroy it. */
    if (exit_status && temp_db_created) {
        ret = krb5_db_destroy(util_context, db5util_db_args);
        /* Ignore a not supported error since there is nothing to do about
         * it anyway. */
        if (ret != 0 && ret != KRB5_PLUGIN_OP_NOTSUPP) {
            com_err(progname, ret, _("while deleting bad database %s"),
                    dbname);
        }
    }

    if (f != NULL && f != stdin)
        fclose(f);

    return;

error:
    exit_status++;
    goto cleanup;
}
