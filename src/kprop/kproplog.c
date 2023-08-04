/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This module will parse the update logs on the primary or replica servers.
 */

#include "k5-int.h"
#include "k5-hex.h"
#include <locale.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <limits.h>
#include <locale.h>
#include <syslog.h>
#include <kdb_log.h>
#include <kadm5/admin.h>
#include <adm_proto.h>

static char *progname;

static void
usage()
{
    fprintf(stderr, _("\nUsage: %s [-h] [-v] [-v] [-e num]\n\t%s -R\n\n"),
            progname, progname);
    exit(1);
}

/*
 * Print the attribute flags of principal in human readable form.
 */
static void
print_flags(unsigned int flags)
{
    char **attrstrs, **sp;

    if (krb5_flags_to_strings(flags, &attrstrs) != 0) {
        printf("\t\t\t(error)\n");
        return;
    }
    for (sp = attrstrs; sp != NULL && *sp != NULL; sp++) {
        printf("\t\t\t%s\n", *sp);
        free(*sp);
    }
    free(attrstrs);
}

/* ctime() for uint32_t* */
static const char *
ctime_uint32(uint32_t *time32)
{
    time_t tmp;
    const char *r;

    tmp = *time32;
    r = ctime(&tmp);
    return (r == NULL) ? "(error)" : r;
}

/* Display time information. */
static void
print_time(uint32_t *timep)
{
    if (*timep == 0L)
        printf("\t\t\tNone\n");
    else
        printf("\t\t\t%s", ctime_uint32(timep));
}

static void
print_deltat(uint32_t *deltat)
{
    krb5_error_code ret;
    static char buf[30];

    ret = krb5_deltat_to_string(*deltat, buf, sizeof(buf));
    if (ret)
        printf("\t\t\t(error)\n");
    else
        printf("\t\t\t%s\n", buf);
}

/* Display string in hex primitive. */
static void
print_hex(const char *tag, utf8str_t *str)
{
    unsigned int len;
    char *hex;

    len = str->utf8str_t_len;

    if (k5_hex_encode(str->utf8str_t_val, len, FALSE, &hex) != 0)
        abort();
    printf("\t\t\t%s(%d): 0x%s\n", tag, len, hex);
    free(hex);
}

/* Display string primitive. */
static void
print_str(const char *tag, utf8str_t *str)
{
    krb5_error_code ret;
    char *s;

    s = k5memdup0(str->utf8str_t_val, str->utf8str_t_len, &ret);
    if (s == NULL) {
        fprintf(stderr, _("\nCouldn't allocate memory"));
        exit(1);
    }
    printf("\t\t\t%s(%d): %s\n", tag, str->utf8str_t_len, s);
    free(s);
}

/* Display data components. */
static void
print_data(const char *tag, kdbe_data_t *data)
{
    printf("\t\t\tmagic: 0x%x\n", data->k_magic);
    print_str(tag, &data->k_data);
}

/* Display the principal components. */
static void
print_princ(kdbe_princ_t *princ)
{
    int i, len;
    kdbe_data_t *data;

    print_str("realm", &princ->k_realm);

    len = princ->k_components.k_components_len;
    data = princ->k_components.k_components_val;
    for (i = 0; i < len; i++, data++)
        print_data("princ", data);
}

/* Display individual key. */
static void
print_key(kdbe_key_t *k)
{
    unsigned int i;
    utf8str_t *str;

    printf("\t\t\tver: %d\n", k->k_ver);
    printf("\t\t\tkvno: %d\n", k->k_kvno);

    for (i = 0; i < k->k_enctype.k_enctype_len; i++)
        printf("\t\t\tenc type: 0x%x\n", k->k_enctype.k_enctype_val[i]);

    str = k->k_contents.k_contents_val;
    for (i = 0; i < k->k_contents.k_contents_len; i++, str++)
        print_hex("key", str);
}

/* Display all key data. */
static void
print_keydata(kdbe_key_t *keys, unsigned int len)
{
    unsigned int i;

    for (i = 0; i < len; i++, keys++)
        print_key(keys);
}

/* Display TL item. */
static void
print_tl(kdbe_tl_t *tl)
{
    int i, len;

    printf("\t\t\ttype: 0x%x\n", tl->tl_type);

    len = tl->tl_data.tl_data_len;

    printf("\t\t\tvalue(%d): 0x", len);
    for (i = 0; i < len; i++)
        printf("%02x", (krb5_octet)tl->tl_data.tl_data_val[i]);
    printf("\n");
}

/* Display TL data items. */
static void
print_tldata(kdbe_tl_t *tldata, int len)
{
    int i;

    printf("\t\t\titems: %d\n", len);
    for (i = 0; i < len; i++, tldata++)
        print_tl(tldata);
}

/*
 * Print the individual types if verbose mode was specified.
 * If verbose-verbose then print types along with respective values.
 */
static void
print_attr(kdbe_val_t *val, int vverbose)
{
    switch (val->av_type) {
    case AT_ATTRFLAGS:
        printf(_("\t\tAttribute flags\n"));
        if (vverbose)
            print_flags(val->kdbe_val_t_u.av_attrflags);
        break;
    case AT_MAX_LIFE:
        printf(_("\t\tMaximum ticket life\n"));
        if (vverbose)
            print_deltat(&val->kdbe_val_t_u.av_max_life);
        break;
    case AT_MAX_RENEW_LIFE:
        printf(_("\t\tMaximum renewable life\n"));
        if (vverbose)
            print_deltat(&val->kdbe_val_t_u.av_max_renew_life);
        break;
    case AT_EXP:
        printf(_("\t\tPrincipal expiration\n"));
        if (vverbose)
            print_time(&val->kdbe_val_t_u.av_exp);
        break;
    case AT_PW_EXP:
        printf(_("\t\tPassword expiration\n"));
        if (vverbose)
            print_time(&val->kdbe_val_t_u.av_pw_exp);
        break;
    case AT_LAST_SUCCESS:
        printf(_("\t\tLast successful auth\n"));
        if (vverbose)
            print_time(&val->kdbe_val_t_u.av_last_success);
        break;
    case AT_LAST_FAILED:
        printf(_("\t\tLast failed auth\n"));
        if (vverbose)
            print_time(&val->kdbe_val_t_u.av_last_failed);
        break;
    case AT_FAIL_AUTH_COUNT:
        printf(_("\t\tFailed passwd attempt\n"));
        if (vverbose)
            printf("\t\t\t%d\n", val->kdbe_val_t_u.av_fail_auth_count);
        break;
    case AT_PRINC:
        printf(_("\t\tPrincipal\n"));
        if (vverbose)
            print_princ(&val->kdbe_val_t_u.av_princ);
        break;
    case AT_KEYDATA:
        printf(_("\t\tKey data\n"));
        if (vverbose) {
            print_keydata(val->kdbe_val_t_u.av_keydata.av_keydata_val,
                          val->kdbe_val_t_u.av_keydata.av_keydata_len);
        }
        break;
    case AT_TL_DATA:
        printf(_("\t\tTL data\n"));
        if (vverbose) {
            print_tldata(val->kdbe_val_t_u.av_tldata.av_tldata_val,
                         val->kdbe_val_t_u.av_tldata.av_tldata_len);
        }
        break;
    case AT_LEN:
        printf(_("\t\tLength\n"));
        if (vverbose)
            printf("\t\t\t%d\n", val->kdbe_val_t_u.av_len);
        break;
    case AT_PW_LAST_CHANGE:
        printf(_("\t\tPassword last changed\n"));
        if (vverbose)
            print_time(&val->kdbe_val_t_u.av_pw_last_change);
        break;
    case AT_MOD_PRINC:
        printf(_("\t\tModifying principal\n"));
        if (vverbose)
            print_princ(&val->kdbe_val_t_u.av_mod_princ);
        break;
    case AT_MOD_TIME:
        printf(_("\t\tModification time\n"));
        if (vverbose)
            print_time(&val->kdbe_val_t_u.av_mod_time);
        break;
    case AT_MOD_WHERE:
        printf(_("\t\tModified where\n"));
        if (vverbose)
            print_str("where", &val->kdbe_val_t_u.av_mod_where);
        break;
    case AT_PW_POLICY:
        printf(_("\t\tPassword policy\n"));
        if (vverbose)
            print_str("policy", &val->kdbe_val_t_u.av_pw_policy);
        break;
    case AT_PW_POLICY_SWITCH:
        printf(_("\t\tPassword policy switch\n"));
        if (vverbose)
            printf("\t\t\t%d\n", val->kdbe_val_t_u.av_pw_policy_switch);
        break;
    case AT_PW_HIST_KVNO:
        printf(_("\t\tPassword history KVNO\n"));
        if (vverbose)
            printf("\t\t\t%d\n", val->kdbe_val_t_u.av_pw_hist_kvno);
        break;
    case AT_PW_HIST:
        printf(_("\t\tPassword history\n"));
        if (vverbose)
            printf("\t\t\tPW history elided\n");
        break;
    } /* switch */

}
/*
 * Print the update entry information
 */
static void
print_update(kdb_hlog_t *ulog, uint32_t entry, uint32_t ulogentries,
             unsigned int verbose)
{
    XDR xdrs;
    uint32_t start_sno, i, j, indx;
    char *dbprinc;
    kdb_ent_header_t *indx_log;
    kdb_incr_update_t upd;

    if (entry && (entry < ulog->kdb_num))
        start_sno = ulog->kdb_last_sno - entry;
    else
        start_sno = ulog->kdb_first_sno - 1;

    for (i = start_sno; i < ulog->kdb_last_sno; i++) {
        indx = i % ulogentries;

        indx_log = INDEX(ulog, indx);

        /*
         * Check for corrupt update entry
         */
        if (indx_log->kdb_umagic != KDB_ULOG_MAGIC) {
            fprintf(stderr, _("Corrupt update entry\n\n"));
            exit(1);
        }

        printf("---\n");
        printf(_("Update Entry\n"));

        printf(_("\tUpdate serial # : %u\n"), indx_log->kdb_entry_sno);

        /* The initial entry after a reset is a dummy entry; skip it. */
        if (indx_log->kdb_entry_size == 0) {
            printf(_("\tDummy entry\n"));
            continue;
        }

        memset(&upd, 0, sizeof(kdb_incr_update_t));
        xdrmem_create(&xdrs, (char *)indx_log->entry_data,
                      indx_log->kdb_entry_size, XDR_DECODE);
        if (!xdr_kdb_incr_update_t(&xdrs, &upd)) {
            printf(_("Entry data decode failure\n\n"));
            exit(1);
        }

        printf(_("\tUpdate operation : "));
        if (upd.kdb_deleted)
            printf(_("Delete\n"));
        else
            printf(_("Add\n"));

        dbprinc = malloc(upd.kdb_princ_name.utf8str_t_len + 1);
        if (dbprinc == NULL) {
            printf(_("Could not allocate principal name\n\n"));
            exit(1);
        }
        strncpy(dbprinc, upd.kdb_princ_name.utf8str_t_val,
                upd.kdb_princ_name.utf8str_t_len);
        dbprinc[upd.kdb_princ_name.utf8str_t_len] = 0;
        printf(_("\tUpdate principal : %s\n"), dbprinc);

        printf(_("\tUpdate size : %u\n"), indx_log->kdb_entry_size);
        printf(_("\tUpdate committed : %s\n"),
               indx_log->kdb_commit ? "True" : "False");

        if (indx_log->kdb_time.seconds == 0L) {
            printf(_("\tUpdate time stamp : None\n"));
        } else{
            printf(_("\tUpdate time stamp : %s"),
                   ctime_uint32(&indx_log->kdb_time.seconds));
        }

        printf(_("\tAttributes changed : %d\n"), upd.kdb_update.kdbe_t_len);

        if (verbose) {
            for (j = 0; j < upd.kdb_update.kdbe_t_len; j++)
                print_attr(&upd.kdb_update.kdbe_t_val[j], verbose > 1 ? 1 : 0);
        }

        xdr_free(xdr_kdb_incr_update_t, (char *)&upd);
        free(dbprinc);
    }
}

/* Return a read-only mmap of the ulog, or NULL on failure. */
static kdb_hlog_t *
map_ulog(const char *filename, int *fd_out)
{
    int fd;
    struct stat st;
    kdb_hlog_t *ulog = MAP_FAILED;

    *fd_out = -1;

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        return NULL;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }
    ulog = mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ulog == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    *fd_out = fd;
    return ulog;
}

int
main(int argc, char **argv)
{
    int c, ulog_fd = -1;
    unsigned int verbose = 0;
    bool_t headeronly = FALSE, reset = FALSE;
    uint32_t entry = 0;
    krb5_context context;
    kadm5_config_params params;
    kdb_hlog_t *ulog = NULL;

    setlocale(LC_ALL, "");

    progname = argv[0];

    while ((c = getopt(argc, argv, "Rvhe:")) != -1) {
        switch (c) {
        case 'h':
            headeronly = TRUE;
            break;
        case 'e':
            entry = atoi(optarg);
            break;
        case 'R':
            reset = TRUE;
            break;
        case 'v':
            verbose++;
            break;
        default:
            usage();
        }
    }

    if (kadm5_init_krb5_context(&context)) {
        fprintf(stderr, _("Unable to initialize Kerberos\n\n"));
        exit(1);
    }

    memset(&params, 0, sizeof(params));

    if (kadm5_get_config_params(context, 1, &params, &params)) {
        fprintf(stderr, _("Couldn't read database_name\n\n"));
        exit(1);
    }

    printf(_("\nKerberos update log (%s)\n"), params.iprop_logfile);

    if (reset) {
        if (ulog_map(context, params.iprop_logfile, params.iprop_ulogsize)) {
            fprintf(stderr, _("Unable to map log file %s\n\n"),
                    params.iprop_logfile);
            exit(1);
        }
        if (ulog_init_header(context) != 0) {
            fprintf(stderr, _("Couldn't reinitialize ulog file %s\n\n"),
                    params.iprop_logfile);
            exit(1);
        }
        printf(_("Reinitialized the ulog.\n"));
        ulog_fini(context);
        goto done;
    }

    ulog = map_ulog(params.iprop_logfile, &ulog_fd);
    if (ulog == NULL) {
        fprintf(stderr, _("Unable to map log file %s\n\n"),
                params.iprop_logfile);
        exit(1);
    }

    if (ulog->kdb_hmagic != KDB_ULOG_HDR_MAGIC) {
        fprintf(stderr, _("Corrupt header log, exiting\n\n"));
        exit(1);
    }

    printf(_("Update log dump :\n"));
    printf(_("\tLog version # : %u\n"), ulog->db_version_num);
    printf(_("\tLog state : "));
    switch (ulog->kdb_state) {
    case KDB_STABLE:
        printf(_("Stable\n"));
        break;
    case KDB_UNSTABLE:
        printf(_("Unstable\n"));
        break;
    case KDB_CORRUPT:
        printf(_("Corrupt\n"));
        break;
    default:
        printf(_("Unknown state: %d\n"), ulog->kdb_state);
        break;
    }
    printf(_("\tEntry block size : %u\n"), ulog->kdb_block);
    printf(_("\tNumber of entries : %u\n"), ulog->kdb_num);

    if (ulog->kdb_last_sno == 0) {
        printf(_("\tLast serial # : None\n"));
    } else {
        if (ulog->kdb_first_sno == 0) {
            printf(_("\tFirst serial # : None\n"));
        } else {
            printf(_("\tFirst serial # : "));
            printf("%u\n", ulog->kdb_first_sno);
        }

        printf(_("\tLast serial # : "));
        printf("%u\n", ulog->kdb_last_sno);
    }

    if (ulog->kdb_last_time.seconds == 0L) {
        printf(_("\tLast time stamp : None\n"));
    } else {
        if (ulog->kdb_first_time.seconds == 0L) {
            printf(_("\tFirst time stamp : None\n"));
        } else {
            printf(_("\tFirst time stamp : %s"),
                   ctime_uint32(&ulog->kdb_first_time.seconds));
        }

        printf(_("\tLast time stamp : %s\n"),
               ctime_uint32(&ulog->kdb_last_time.seconds));
    }

    if (!headeronly && ulog->kdb_num)
        print_update(ulog, entry, params.iprop_ulogsize, verbose);

    printf("\n");

done:
    close(ulog_fd);
    kadm5_free_config_params(context, &params);
    krb5_free_context(context);
    return 0;
}
