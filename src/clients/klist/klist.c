/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* clients/klist/klist.c - List contents of credential cache or keytab */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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
#include <krb5.h>
#include <com_err.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
/* Need definition of INET6 before network headers, for IRIX.  */
#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif

#ifndef _WIN32
#define GET_PROGNAME(x) (strrchr((x), '/') ? strrchr((x), '/')+1 : (x))
#else
#define GET_PROGNAME(x) max(max(strrchr((x), '/'), strrchr((x), '\\')) + 1,(x))
#endif

#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#endif

extern int optind;

int show_flags = 0, show_time = 0, status_only = 0, show_keys = 0;
int show_etype = 0, show_addresses = 0, no_resolve = 0, print_version = 0;
int show_adtype = 0, show_all = 0, list_all = 0, use_client_keytab = 0;
int show_config = 0;
char *defname;
char *progname;
krb5_int32 now;
unsigned int timestamp_width;

krb5_context kcontext;

krb5_boolean is_local_tgt (krb5_principal princ, krb5_data *realm);
char * etype_string (krb5_enctype );
void show_credential (krb5_creds *);

void list_all_ccaches (void);
int list_ccache (krb5_ccache);
void show_all_ccaches (void);
void do_ccache_name (char *);
int show_ccache (krb5_ccache);
int check_ccache (krb5_ccache);
void do_keytab (char *);
void printtime (time_t);
void one_addr (krb5_address *);
void fillit (FILE *, unsigned int, int);

#define DEFAULT 0
#define CCACHE 1
#define KEYTAB 2

static void usage()
{
#define KRB_AVAIL_STRING(x) ((x)?"available":"not available")

    fprintf(stderr, _("Usage: %s [-e] [-V] [[-c] [-l] [-A] [-d] [-f] [-s] "
                      "[-a [-n]]] [-k [-t] [-K]] [name]\n"), progname);
    fprintf(stderr, _("\t-c specifies credentials cache\n"));
    fprintf(stderr, _("\t-k specifies keytab\n"));
    fprintf(stderr, _("\t   (Default is credentials cache)\n"));
    fprintf(stderr, _("\t-i uses default client keytab if no name given\n"));
    fprintf(stderr, _("\t-l lists credential caches in collection\n"));
    fprintf(stderr, _("\t-A shows content of all credential caches\n"));
    fprintf(stderr, _("\t-e shows the encryption type\n"));
    fprintf(stderr, _("\t-V shows the Kerberos version and exits\n"));
    fprintf(stderr, _("\toptions for credential caches:\n"));
    fprintf(stderr, _("\t\t-d shows the submitted authorization data "
                      "types\n"));
    fprintf(stderr, _("\t\t-f shows credentials flags\n"));
    fprintf(stderr, _("\t\t-s sets exit status based on valid tgt "
                      "existence\n"));
    fprintf(stderr, _("\t\t-a displays the address list\n"));
    fprintf(stderr, _("\t\t\t-n do not reverse-resolve\n"));
    fprintf(stderr, _("\toptions for keytabs:\n"));
    fprintf(stderr, _("\t\t-t shows keytab entry timestamps\n"));
    fprintf(stderr, _("\t\t-K shows keytab entry keys\n"));
    exit(1);
}

static void
extended_com_err_fn(const char *prog, errcode_t code, const char *fmt,
                    va_list args)
{
    const char *msg;

    msg = krb5_get_error_message(kcontext, code);
    fprintf(stderr, "%s: %s%s", prog, msg, (*fmt == '\0') ? "" : " ");
    krb5_free_error_message(kcontext, msg);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

int
main(argc, argv)
    int argc;
    char **argv;
{
    int c;
    char *name;
    int mode;

    setlocale(LC_ALL, "");
    progname = GET_PROGNAME(argv[0]);
    set_com_err_hook(extended_com_err_fn);

    name = NULL;
    mode = DEFAULT;
    /* V=version so v can be used for verbose later if desired.  */
    while ((c = getopt(argc, argv, "dfetKsnacki45lAVC")) != -1) {
        switch (c) {
        case 'd':
            show_adtype = 1;
            break;
        case 'f':
            show_flags = 1;
            break;
        case 'e':
            show_etype = 1;
            break;
        case 't':
            show_time = 1;
            break;
        case 'K':
            show_keys = 1;
            break;
        case 's':
            status_only = 1;
            break;
        case 'n':
            no_resolve = 1;
            break;
        case 'a':
            show_addresses = 1;
            break;
        case 'c':
            if (mode != DEFAULT) usage();
            mode = CCACHE;
            break;
        case 'k':
            if (mode != DEFAULT) usage();
            mode = KEYTAB;
            break;
        case 'i':
            use_client_keytab = 1;
            break;
        case '4':
            fprintf(stderr, _("Kerberos 4 is no longer supported\n"));
            exit(3);
            break;
        case '5':
            break;
        case 'l':
            list_all = 1;
            break;
        case 'A':
            show_all = 1;
            break;
        case 'C':
            show_config = 1;
            break;
        case 'V':
            print_version = 1;
            break;
        default:
            usage();
            break;
        }
    }

    if (no_resolve && !show_addresses) {
        usage();
    }

    if (mode == DEFAULT || mode == CCACHE) {
        if (show_time || show_keys)
            usage();
        if ((show_all && list_all) || (status_only && list_all))
            usage();
    } else {
        if (show_flags || status_only || show_addresses ||
            show_all || list_all)
            usage();
    }

    if (argc - optind > 1) {
        fprintf(stderr, _("Extra arguments (starting with \"%s\").\n"),
                argv[optind+1]);
        usage();
    }

    if (print_version) {
#ifdef _WIN32                   /* No access to autoconf vars; fix somehow. */
        printf("Kerberos for Windows\n");
#else
        printf(_("%s version %s\n"), PACKAGE_NAME, PACKAGE_VERSION);
#endif
        exit(0);
    }

    name = (optind == argc-1) ? argv[optind] : 0;

    now = time(0);
    {
        char tmp[BUFSIZ];

        if (!krb5_timestamp_to_sfstring(now, tmp, 20, (char *) NULL) ||
            !krb5_timestamp_to_sfstring(now, tmp, sizeof(tmp),
                                        (char *) NULL))
            timestamp_width = (int) strlen(tmp);
        else
            timestamp_width = 15;
    }

    {
        krb5_error_code retval;
        retval = krb5_init_context(&kcontext);
        if (retval) {
            com_err(progname, retval, _("while initializing krb5"));
            exit(1);
        }

        if (list_all)
            list_all_ccaches();
        else if (show_all)
            show_all_ccaches();
        else if (mode == DEFAULT || mode == CCACHE)
            do_ccache_name(name);
        else
            do_keytab(name);
    }

    return 0;
}

void do_keytab(name)
    char *name;
{
    krb5_keytab kt;
    krb5_keytab_entry entry;
    krb5_kt_cursor cursor;
    char buf[BUFSIZ]; /* hopefully large enough for any type */
    char *pname;
    int code;

    if (name == NULL && use_client_keytab) {
        if ((code = krb5_kt_client_default(kcontext, &kt))) {
            com_err(progname, code, _("while getting default client keytab"));
            exit(1);
        }
    } else if (name == NULL) {
        if ((code = krb5_kt_default(kcontext, &kt))) {
            com_err(progname, code, _("while getting default keytab"));
            exit(1);
        }
    } else {
        if ((code = krb5_kt_resolve(kcontext, name, &kt))) {
            com_err(progname, code, _("while resolving keytab %s"), name);
            exit(1);
        }
    }

    if ((code = krb5_kt_get_name(kcontext, kt, buf, BUFSIZ))) {
        com_err(progname, code, _("while getting keytab name"));
        exit(1);
    }

    printf("Keytab name: %s\n", buf);

    if ((code = krb5_kt_start_seq_get(kcontext, kt, &cursor))) {
        com_err(progname, code, _("while starting keytab scan"));
        exit(1);
    }

    /* XXX Translating would disturb table alignment; skip for now. */
    if (show_time) {
        printf("KVNO Timestamp");
        fillit(stdout, timestamp_width - sizeof("Timestamp") + 2, (int) ' ');
        printf("Principal\n");
        printf("---- ");
        fillit(stdout, timestamp_width, (int) '-');
        printf(" ");
        fillit(stdout, 78 - timestamp_width - sizeof("KVNO"), (int) '-');
        printf("\n");
    } else {
        printf("KVNO Principal\n");
        printf("---- --------------------------------------------------------------------------\n");
    }

    while ((code = krb5_kt_next_entry(kcontext, kt, &entry, &cursor)) == 0) {
        if ((code = krb5_unparse_name(kcontext, entry.principal, &pname))) {
            com_err(progname, code, _("while unparsing principal name"));
            exit(1);
        }
        printf("%4d ", entry.vno);
        if (show_time) {
            printtime(entry.timestamp);
            printf(" ");
        }
        printf("%s", pname);
        if (show_etype)
            printf(" (%s) " , etype_string(entry.key.enctype));
        if (show_keys) {
            printf(" (0x");
            {
                unsigned int i;
                for (i = 0; i < entry.key.length; i++)
                    printf("%02x", entry.key.contents[i]);
            }
            printf(")");
        }
        printf("\n");
        krb5_free_unparsed_name(kcontext, pname);
        krb5_free_keytab_entry_contents(kcontext, &entry);
    }
    if (code && code != KRB5_KT_END) {
        com_err(progname, code, _("while scanning keytab"));
        exit(1);
    }
    if ((code = krb5_kt_end_seq_get(kcontext, kt, &cursor))) {
        com_err(progname, code, _("while ending keytab scan"));
        exit(1);
    }
    exit(0);
}

void
list_all_ccaches(void)
{
    krb5_error_code code;
    krb5_ccache cache;
    krb5_cccol_cursor cursor;
    int exit_status;

    code = krb5_cccol_cursor_new(kcontext, &cursor);
    if (code) {
        if (!status_only)
            com_err(progname, code, _("while listing ccache collection"));
        exit(1);
    }

    /* XXX Translating would disturb table alignment; skip for now. */
    printf("%-30s %s\n", "Principal name", "Cache name");
    printf("%-30s %s\n", "--------------", "----------");
    exit_status = 1;
    while (!(code = krb5_cccol_cursor_next(kcontext, cursor, &cache)) &&
           cache != NULL) {
        exit_status = list_ccache(cache) && exit_status;
        krb5_cc_close(kcontext, cache);
    }
    krb5_cccol_cursor_free(kcontext, &cursor);
    exit(exit_status);
}

int
list_ccache(krb5_ccache cache)
{
    krb5_error_code code;
    krb5_principal princ = NULL;
    char *princname = NULL, *ccname = NULL;
    int expired, status = 1;

    code = krb5_cc_get_principal(kcontext, cache, &princ);
    if (code)                   /* Uninitialized cache file, probably. */
        goto cleanup;
    code = krb5_unparse_name(kcontext, princ, &princname);
    if (code)
        goto cleanup;
    code = krb5_cc_get_full_name(kcontext, cache, &ccname);
    if (code)
        goto cleanup;

    expired = check_ccache(cache);

    printf("%-30.30s %s", princname, ccname);
    if (expired)
        printf(" %s", _("(Expired)"));
    printf("\n");

    status = 0;
cleanup:
    krb5_free_principal(kcontext, princ);
    krb5_free_unparsed_name(kcontext, princname);
    krb5_free_string(kcontext, ccname);
    return status;
}

void
show_all_ccaches(void)
{
    krb5_error_code code;
    krb5_ccache cache;
    krb5_cccol_cursor cursor;
    krb5_boolean first;
    int exit_status, st;

    code = krb5_cccol_cursor_new(kcontext, &cursor);
    if (code) {
        if (!status_only)
            com_err(progname, code, _("while listing ccache collection"));
        exit(1);
    }
    exit_status = 1;
    first = TRUE;
    while (!(code = krb5_cccol_cursor_next(kcontext, cursor, &cache)) &&
           cache != NULL) {
        if (!status_only && !first)
            printf("\n");
        first = FALSE;
        st = status_only ? check_ccache(cache) : show_ccache(cache);
        exit_status = st && exit_status;
        krb5_cc_close(kcontext, cache);
    }
    krb5_cccol_cursor_free(kcontext, &cursor);
    exit(exit_status);
}

void
do_ccache_name(char *name)
{
    krb5_error_code code;
    krb5_ccache cache;

    if (name == NULL) {
        if ((code = krb5_cc_default(kcontext, &cache))) {
            if (!status_only)
                com_err(progname, code, _("while getting default ccache"));
            exit(1);
        }
    } else {
        if ((code = krb5_cc_resolve(kcontext, name, &cache))) {
            if (!status_only)
                com_err(progname, code, _("while resolving ccache %s"),
                        name);
            exit(1);
        }
    }
    exit(status_only ? check_ccache(cache) : show_ccache(cache));
}

/* Display the contents of cache. */
int
show_ccache(krb5_ccache cache)
{
    krb5_cc_cursor cur;
    krb5_creds creds;
    krb5_principal princ;
    krb5_error_code code;

    if ((code = krb5_cc_get_principal(kcontext, cache, &princ))) {
        com_err(progname, code, "");
        return 1;
    }
    if ((code = krb5_unparse_name(kcontext, princ, &defname))) {
        com_err(progname, code, _("while unparsing principal name"));
        return 1;
    }

    printf(_("Ticket cache: %s:%s\nDefault principal: %s\n\n"),
           krb5_cc_get_type(kcontext, cache),
           krb5_cc_get_name(kcontext, cache), defname);
    /* XXX Translating would disturb table alignment; skip for now. */
    fputs("Valid starting", stdout);
    fillit(stdout, timestamp_width - sizeof("Valid starting") + 3, (int) ' ');
    fputs("Expires", stdout);
    fillit(stdout, timestamp_width - sizeof("Expires") + 3, (int) ' ');
    fputs("Service principal\n", stdout);

    if ((code = krb5_cc_start_seq_get(kcontext, cache, &cur))) {
        com_err(progname, code, _("while starting to retrieve tickets"));
        return 1;
    }
    while (!(code = krb5_cc_next_cred(kcontext, cache, &cur, &creds))) {
        if (show_config || !krb5_is_config_principal(kcontext, creds.server))
            show_credential(&creds);
        krb5_free_cred_contents(kcontext, &creds);
    }
    krb5_free_principal(kcontext, princ);
    krb5_free_unparsed_name(kcontext, defname);
    defname = NULL;
    if (code == KRB5_CC_END) {
        if ((code = krb5_cc_end_seq_get(kcontext, cache, &cur))) {
            com_err(progname, code, _("while finishing ticket retrieval"));
            return 1;
        }
        return 0;
    } else {
        com_err(progname, code, _("while retrieving a ticket"));
        return 1;
    }
}

/* Return 0 if cache is accessible, present, and unexpired; return 1 if not. */
int
check_ccache(krb5_ccache cache)
{
    krb5_error_code ret;
    krb5_cc_cursor cur;
    krb5_creds creds;
    krb5_principal princ;
    krb5_boolean found_tgt, found_current_tgt, found_current_cred;

    if (krb5_cc_get_principal(kcontext, cache, &princ) != 0)
        return 1;
    if (krb5_cc_start_seq_get(kcontext, cache, &cur) != 0)
        return 1;
    found_tgt = found_current_tgt = found_current_cred = FALSE;
    while (!(ret = krb5_cc_next_cred(kcontext, cache, &cur, &creds))) {
        if (is_local_tgt(creds.server, &princ->realm)) {
            found_tgt = TRUE;
            if (creds.times.endtime > now)
                found_current_tgt = TRUE;
        } else if (!krb5_is_config_principal(kcontext, creds.server) &&
                   creds.times.endtime > now) {
            found_current_cred = TRUE;
        }
        krb5_free_cred_contents(kcontext, &creds);
    }
    krb5_free_principal(kcontext, princ);
    if (ret != KRB5_CC_END)
        return 1;
    if (krb5_cc_end_seq_get(kcontext, cache, &cur) != 0)
        return 1;

    /* If the cache contains at least one local TGT, require that it be
     * current.  Otherwise accept any current cred. */
    if (found_tgt)
        return found_current_tgt ? 0 : 1;
    return found_current_cred ? 0 : 1;
}

/* Return true if princ is the local krbtgt principal for local_realm. */
krb5_boolean
is_local_tgt(krb5_principal princ, krb5_data *realm)
{
    return princ->length == 2 && data_eq(princ->realm, *realm) &&
        data_eq_string(princ->data[0], KRB5_TGS_NAME) &&
        data_eq(princ->data[1], *realm);
}

char *
etype_string(enctype)
    krb5_enctype enctype;
{
    static char buf[100];
    krb5_error_code retval;

    if ((retval = krb5_enctype_to_name(enctype, FALSE, buf, sizeof(buf)))) {
        /* XXX if there's an error != EINVAL, I should probably report it */
        snprintf(buf, sizeof(buf), "etype %d", enctype);
    }

    return buf;
}

static char *
flags_string(cred)
    register krb5_creds *cred;
{
    static char buf[32];
    int i = 0;

    if (cred->ticket_flags & TKT_FLG_FORWARDABLE)
        buf[i++] = 'F';
    if (cred->ticket_flags & TKT_FLG_FORWARDED)
        buf[i++] = 'f';
    if (cred->ticket_flags & TKT_FLG_PROXIABLE)
        buf[i++] = 'P';
    if (cred->ticket_flags & TKT_FLG_PROXY)
        buf[i++] = 'p';
    if (cred->ticket_flags & TKT_FLG_MAY_POSTDATE)
        buf[i++] = 'D';
    if (cred->ticket_flags & TKT_FLG_POSTDATED)
        buf[i++] = 'd';
    if (cred->ticket_flags & TKT_FLG_INVALID)
        buf[i++] = 'i';
    if (cred->ticket_flags & TKT_FLG_RENEWABLE)
        buf[i++] = 'R';
    if (cred->ticket_flags & TKT_FLG_INITIAL)
        buf[i++] = 'I';
    if (cred->ticket_flags & TKT_FLG_HW_AUTH)
        buf[i++] = 'H';
    if (cred->ticket_flags & TKT_FLG_PRE_AUTH)
        buf[i++] = 'A';
    if (cred->ticket_flags & TKT_FLG_TRANSIT_POLICY_CHECKED)
        buf[i++] = 'T';
    if (cred->ticket_flags & TKT_FLG_OK_AS_DELEGATE)
        buf[i++] = 'O';         /* D/d are taken.  Use short strings?  */
    if (cred->ticket_flags & TKT_FLG_ANONYMOUS)
        buf[i++] = 'a';
    buf[i] = '\0';
    return(buf);
}

void
printtime(tv)
    time_t tv;
{
    char timestring[BUFSIZ];
    char fill;

    fill = ' ';
    if (!krb5_timestamp_to_sfstring((krb5_timestamp) tv,
                                    timestring,
                                    timestamp_width+1,
                                    &fill)) {
        printf("%s", timestring);
    }
}

static void
print_config_data(int col, krb5_data *data)
{
    unsigned int i;

    for (i = 0; i < data->length; i++) {
        while (col < 8) {
            putchar(' ');
            col++;
        }
        if (data->data[i] > 0x20 && data->data[i] < 0x7f) {
            putchar(data->data[i]);
            col++;
        } else {
            col += printf("\\%03o", (unsigned char)data->data[i]);
        }
        if (col > 72) {
            putchar('\n');
            col = 0;
        }
    }
    if (col > 0)
        putchar('\n');
}

void
show_credential(cred)
    register krb5_creds * cred;
{
    krb5_error_code retval;
    krb5_ticket *tkt;
    char *name, *sname, *flags;
    int extra_field = 0, ccol = 0, i;

    retval = krb5_unparse_name(kcontext, cred->client, &name);
    if (retval) {
        com_err(progname, retval, _("while unparsing client name"));
        return;
    }
    retval = krb5_unparse_name(kcontext, cred->server, &sname);
    if (retval) {
        com_err(progname, retval, _("while unparsing server name"));
        krb5_free_unparsed_name(kcontext, name);
        return;
    }
    if (!cred->times.starttime)
        cred->times.starttime = cred->times.authtime;

    if (!krb5_is_config_principal(kcontext, cred->server)) {
        printtime(cred->times.starttime);
        putchar(' '); putchar(' ');
        printtime(cred->times.endtime);
        putchar(' '); putchar(' ');

        printf("%s\n", sname);
    } else {
        fputs("config: ", stdout);
        ccol = 8;
        for (i = 1; i < cred->server->length; i++) {
            ccol += printf("%s%.*s%s",
                           i > 1 ? "(" : "",
                           (int)cred->server->data[i].length,
                           cred->server->data[i].data,
                           i > 1 ? ")" : "");
        }
        fputs(" = ", stdout);
        ccol += 3;
    }

    if (strcmp(name, defname)) {
        printf(_("\tfor client %s"), name);
        extra_field++;
    }

    if (krb5_is_config_principal(kcontext, cred->server))
        print_config_data(ccol, &cred->ticket);

    if (cred->times.renew_till) {
        if (!extra_field)
            fputs("\t",stdout);
        else
            fputs(", ",stdout);
        fputs(_("renew until "), stdout);
        printtime(cred->times.renew_till);
        extra_field += 2;
    }

    if (extra_field > 3) {
        fputs("\n", stdout);
        extra_field = 0;
    }

    if (show_flags) {
        flags = flags_string(cred);
        if (flags && *flags) {
            if (!extra_field)
                fputs("\t",stdout);
            else
                fputs(", ",stdout);
            printf(_("Flags: %s"), flags);
            extra_field++;
        }
    }

    if (extra_field > 2) {
        fputs("\n", stdout);
        extra_field = 0;
    }

    if (show_etype) {
        retval = krb5_decode_ticket(&cred->ticket, &tkt);
        if (retval)
            goto err_tkt;

        if (!extra_field)
            fputs("\t",stdout);
        else
            fputs(", ",stdout);
        printf(_("Etype (skey, tkt): %s, "),
               etype_string(cred->keyblock.enctype));
        printf("%s ",
               etype_string(tkt->enc_part.enctype));
        extra_field++;

    err_tkt:
        if (tkt != NULL)
            krb5_free_ticket(kcontext, tkt);
    }

    if (show_adtype) {
        if (cred->authdata != NULL) {
            if (!extra_field)
                fputs("\t",stdout);
            else
                fputs(", ",stdout);
            printf(_("AD types: "));
            for (i = 0; cred->authdata[i] != NULL; i++) {
                if (i)
                    printf(", ");
                printf("%d", cred->authdata[i]->ad_type);
            }
            extra_field++;
        }
    }

    /* if any additional info was printed, extra_field is non-zero */
    if (extra_field)
        putchar('\n');


    if (show_addresses) {
        if (!cred->addresses || !cred->addresses[0]) {
            printf(_("\tAddresses: (none)\n"));
        } else {
            printf(_("\tAddresses: "));
            one_addr(cred->addresses[0]);

            for (i=1; cred->addresses[i]; i++) {
                printf(", ");
                one_addr(cred->addresses[i]);
            }

            printf("\n");
        }
    }

    krb5_free_unparsed_name(kcontext, name);
    krb5_free_unparsed_name(kcontext, sname);
}

#include "port-sockets.h"
#include "socket-utils.h" /* for ss2sin etc */
#include "fake-addrinfo.h"

void one_addr(a)
    krb5_address *a;
{
    struct sockaddr_storage ss;
    int err;
    char namebuf[NI_MAXHOST];

    memset (&ss, 0, sizeof (ss));

    switch (a->addrtype) {
    case ADDRTYPE_INET:
        if (a->length != 4) {
        broken:
            printf(_("broken address (type %d length %d)"),
                   a->addrtype, a->length);
            return;
        }
        {
            struct sockaddr_in *sinp = ss2sin (&ss);
            sinp->sin_family = AF_INET;
            memcpy (&sinp->sin_addr, a->contents, 4);
        }
        break;
    case ADDRTYPE_INET6:
        if (a->length != 16)
            goto broken;
        {
            struct sockaddr_in6 *sin6p = ss2sin6 (&ss);
            sin6p->sin6_family = AF_INET6;
            memcpy (&sin6p->sin6_addr, a->contents, 16);
        }
        break;
    default:
        printf(_("unknown addrtype %d"), a->addrtype);
        return;
    }

    namebuf[0] = 0;
    err = getnameinfo (ss2sa (&ss), sa_socklen (ss2sa (&ss)),
                       namebuf, sizeof (namebuf), 0, 0,
                       no_resolve ? NI_NUMERICHOST : 0U);
    if (err) {
        printf(_("unprintable address (type %d, error %d %s)"), a->addrtype,
               err, gai_strerror (err));
        return;
    }
    printf ("%s", namebuf);
}

void
fillit(f, num, c)
    FILE                *f;
    unsigned int        num;
    int                 c;
{
    unsigned int i;

    for (i=0; i<num; i++)
        fputc(c, f);
}
