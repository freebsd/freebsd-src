/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/main.c - Main procedure body for the KDC server process */
/*
 * Copyright 1990,2001,2008,2009,2016 by the Massachusetts Institute of
 * Technology.
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
#include <kadm5/admin.h>
#include "adm_proto.h"
#include "kdc_util.h"
#include "kdc_audit.h"
#include "extern.h"
#include "policy.h"
#include "kdc5_err.h"
#include "kdb_kt.h"
#include "net-server.h"
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <locale.h>
#include <syslog.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>

#if defined(NEED_DAEMON_PROTO)
extern int daemon(int, int);
#endif

static void usage (char *);

static void initialize_realms(krb5_context kcontext, int argc, char **argv,
                              int *tcp_listen_backlog_out);

static void finish_realms (void);

static int nofork = 0;
static int workers = 0;
static int time_offset = 0;
static const char *pid_file = NULL;
static volatile int signal_received = 0;
static volatile int sighup_received = 0;

#define KRB5_KDC_MAX_REALMS     32

static const char *kdc_progname;

/*
 * Static server_handle for this file.  Other code will get access to
 * it through the application handle that net-server.c uses.
 */
static struct server_handle shandle;

/*
 * We use krb5_klog_init to set up a com_err callback to log error
 * messages.  The callback also pulls the error message out of the
 * context we pass to krb5_klog_init; however, we use realm-specific
 * contexts for most of our krb5 library calls, so the error message
 * isn't present in the global context.  This wrapper ensures that the
 * error message state from the call context is copied into the
 * context known by krb5_klog.  call_context can be NULL if the error
 * code did not come from a krb5 library function.
 */
void
kdc_err(krb5_context call_context, errcode_t code, const char *fmt, ...)
{
    va_list ap;

    if (call_context)
        krb5_copy_error_message(shandle.kdc_err_context, call_context);
    va_start(ap, fmt);
    com_err_va(kdc_progname, code, fmt, ap);
    va_end(ap);
}

/*
 * Find the realm entry for a given realm.
 */
kdc_realm_t *
find_realm_data(struct server_handle *handle, char *rname, krb5_ui_4 rsize)
{
    int i;
    kdc_realm_t **kdc_realmlist = handle->kdc_realmlist;
    int kdc_numrealms = handle->kdc_numrealms;

    for (i=0; i<kdc_numrealms; i++) {
        if ((rsize == strlen(kdc_realmlist[i]->realm_name)) &&
            !strncmp(rname, kdc_realmlist[i]->realm_name, rsize))
            return(kdc_realmlist[i]);
    }
    return((kdc_realm_t *) NULL);
}

kdc_realm_t *
setup_server_realm(struct server_handle *handle, krb5_principal sprinc)
{
    kdc_realm_t         *newrealm;
    kdc_realm_t **kdc_realmlist = handle->kdc_realmlist;
    int kdc_numrealms = handle->kdc_numrealms;

    if (sprinc == NULL)
        return NULL;

    if (kdc_numrealms > 1) {
        newrealm = find_realm_data(handle, sprinc->realm.data,
                                   sprinc->realm.length);
    } else {
        newrealm = kdc_realmlist[0];
    }
    if (newrealm != NULL) {
        krb5_klog_set_context(newrealm->realm_context);
        shandle.kdc_err_context = newrealm->realm_context;
    }
    return newrealm;
}

static void
finish_realm(kdc_realm_t *rdp)
{
    if (rdp->realm_name)
        free(rdp->realm_name);
    if (rdp->realm_mpname)
        free(rdp->realm_mpname);
    if (rdp->realm_stash)
        free(rdp->realm_stash);
    if (rdp->realm_listen)
        free(rdp->realm_listen);
    if (rdp->realm_tcp_listen)
        free(rdp->realm_tcp_listen);
    if (rdp->realm_keytab)
        krb5_kt_close(rdp->realm_context, rdp->realm_keytab);
    if (rdp->realm_hostbased)
        free(rdp->realm_hostbased);
    if (rdp->realm_no_referral)
        free(rdp->realm_no_referral);
    if (rdp->realm_context) {
        if (rdp->realm_mprinc)
            krb5_free_principal(rdp->realm_context, rdp->realm_mprinc);
        zapfree(rdp->realm_mkey.contents, rdp->realm_mkey.length);
        krb5_db_fini(rdp->realm_context);
        if (rdp->realm_tgsprinc)
            krb5_free_principal(rdp->realm_context, rdp->realm_tgsprinc);
        krb5_free_context(rdp->realm_context);
    }
    zapfree(rdp, sizeof(*rdp));
}

/* Set *val_out to an allocated string containing val1 and/or val2, separated
 * by a space if both are set, or NULL if neither is set. */
static krb5_error_code
combine(const char *val1, const char *val2, char **val_out)
{
    if (val1 == NULL && val2 == NULL) {
        *val_out = NULL;
    } else if (val1 != NULL && val2 != NULL) {
        if (asprintf(val_out, "%s %s", val1, val2) < 0) {
            *val_out = NULL;
            return ENOMEM;
        }
    } else {
        *val_out = strdup((val1 != NULL) ? val1 : val2);
        if (*val_out == NULL)
            return ENOMEM;
    }
    return 0;
}

/*
 * Initialize a realm control structure from the alternate profile or from
 * the specified defaults.
 *
 * After we're complete here, the essence of the realm is embodied in the
 * realm data and we should be all set to begin operation for that realm.
 */
static  krb5_error_code
init_realm(kdc_realm_t * rdp, krb5_pointer aprof, char *realm,
           char *def_mpname, krb5_enctype def_enctype, char *def_udp_listen,
           char *def_tcp_listen, krb5_boolean def_manual,
           krb5_boolean def_restrict_anon, char **db_args, char *no_referral,
           char *hostbased)
{
    krb5_error_code     kret;
    krb5_boolean        manual;
    int                 kdb_open_flags;
    char                *svalue = NULL;
    const char          *hierarchy[4];
    krb5_kvno       mkvno = IGNORE_VNO;
    char ename[32];

    memset(rdp, 0, sizeof(kdc_realm_t));
    if (!realm) {
        kret = EINVAL;
        goto whoops;
    }

    if (def_enctype != ENCTYPE_UNKNOWN &&
        krb5int_c_deprecated_enctype(def_enctype)) {
        if (krb5_enctype_to_name(def_enctype, FALSE, ename, sizeof(ename)))
            ename[0] = '\0';
        fprintf(stderr,
                _("Requested master password enctype %s in %s is "
                  "DEPRECATED!\n"),
                ename, realm);
    }

    hierarchy[0] = KRB5_CONF_REALMS;
    hierarchy[1] = realm;
    hierarchy[3] = NULL;

    rdp->realm_name = strdup(realm);
    if (rdp->realm_name == NULL) {
        kret = ENOMEM;
        goto whoops;
    }
    kret = krb5int_init_context_kdc(&rdp->realm_context);
    if (kret) {
        kdc_err(NULL, kret, _("while getting context for realm %s"), realm);
        goto whoops;
    }
    if (time_offset != 0)
        (void)krb5_set_time_offsets(rdp->realm_context, time_offset, 0);

    /* Handle master key name */
    hierarchy[2] = KRB5_CONF_MASTER_KEY_NAME;
    if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &rdp->realm_mpname)) {
        rdp->realm_mpname = (def_mpname) ? strdup(def_mpname) :
            strdup(KRB5_KDB_M_NAME);
    }
    if (!rdp->realm_mpname) {
        kret = ENOMEM;
        goto whoops;
    }

    /* Handle KDC addresses/ports */
    hierarchy[2] = KRB5_CONF_KDC_LISTEN;
    if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &rdp->realm_listen)) {
        /* Try the old kdc_ports configuration option. */
        hierarchy[2] = KRB5_CONF_KDC_PORTS;
        if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &rdp->realm_listen))
            rdp->realm_listen = strdup(def_udp_listen);
    }
    if (!rdp->realm_listen) {
        kret = ENOMEM;
        goto whoops;
    }
    hierarchy[2] = KRB5_CONF_KDC_TCP_LISTEN;
    if (krb5_aprof_get_string(aprof, hierarchy, TRUE,
                              &rdp->realm_tcp_listen)) {
        /* Try the old kdc_tcp_ports configuration option. */
        hierarchy[2] = KRB5_CONF_KDC_TCP_PORTS;
        if (krb5_aprof_get_string(aprof, hierarchy, TRUE,
                                  &rdp->realm_tcp_listen))
            rdp->realm_tcp_listen = strdup(def_tcp_listen);
    }
    if (!rdp->realm_tcp_listen) {
        kret = ENOMEM;
        goto whoops;
    }
    /* Handle stash file */
    hierarchy[2] = KRB5_CONF_KEY_STASH_FILE;
    if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &rdp->realm_stash))
        manual = def_manual;
    else
        manual = FALSE;

    hierarchy[2] = KRB5_CONF_RESTRICT_ANONYMOUS_TO_TGT;
    if (krb5_aprof_get_boolean(aprof, hierarchy, TRUE,
                               &rdp->realm_restrict_anon))
        rdp->realm_restrict_anon = def_restrict_anon;

    /* Handle master key type */
    hierarchy[2] = KRB5_CONF_MASTER_KEY_TYPE;
    if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &svalue) ||
        krb5_string_to_enctype(svalue, &rdp->realm_mkey.enctype))
        rdp->realm_mkey.enctype = manual ? def_enctype : ENCTYPE_UNKNOWN;
    free(svalue);
    svalue = NULL;

    /* Handle reject-bad-transit flag */
    hierarchy[2] = KRB5_CONF_REJECT_BAD_TRANSIT;
    if (krb5_aprof_get_boolean(aprof, hierarchy, TRUE,
                                &rdp->realm_reject_bad_transit))
        rdp->realm_reject_bad_transit = TRUE;

    /* Handle ticket maximum life */
    hierarchy[2] = KRB5_CONF_MAX_LIFE;
    if (krb5_aprof_get_deltat(aprof, hierarchy, TRUE, &rdp->realm_maxlife))
        rdp->realm_maxlife = KRB5_KDB_MAX_LIFE;

    /* Handle ticket renewable maximum life */
    hierarchy[2] = KRB5_CONF_MAX_RENEWABLE_LIFE;
    if (krb5_aprof_get_deltat(aprof, hierarchy, TRUE, &rdp->realm_maxrlife))
        rdp->realm_maxrlife = KRB5_KDB_MAX_RLIFE;

    /* Handle KDC referrals */
    hierarchy[2] = KRB5_CONF_NO_HOST_REFERRAL;
    (void)krb5_aprof_get_string_all(aprof, hierarchy, &svalue);
    kret = combine(no_referral, svalue, &rdp->realm_no_referral);
    if (kret)
        goto whoops;
    free(svalue);
    svalue = NULL;

    hierarchy[2] = KRB5_CONF_HOST_BASED_SERVICES;
    (void)krb5_aprof_get_string_all(aprof, hierarchy, &svalue);
    kret = combine(hostbased, svalue, &rdp->realm_hostbased);
    if (kret)
        goto whoops;
    free(svalue);
    svalue = NULL;

    hierarchy[2] = KRB5_CONF_DISABLE_PAC;
    if (krb5_aprof_get_boolean(aprof, hierarchy, TRUE,
                               &rdp->realm_disable_pac))
        rdp->realm_disable_pac = FALSE;

    /*
     * We've got our parameters, now go and setup our realm context.
     */

    /* Set the default realm of this context */
    if ((kret = krb5_set_default_realm(rdp->realm_context, realm))) {
        kdc_err(rdp->realm_context, kret,
                _("while setting default realm to %s"), realm);
        goto whoops;
    }

    /* first open the database  before doing anything */
    kdb_open_flags = KRB5_KDB_OPEN_RW | KRB5_KDB_SRV_TYPE_KDC;
    if ((kret = krb5_db_open(rdp->realm_context, db_args, kdb_open_flags))) {
        kdc_err(rdp->realm_context, kret,
                _("while initializing database for realm %s"), realm);
        goto whoops;
    }

    /* Assemble and parse the master key name */
    if ((kret = krb5_db_setup_mkey_name(rdp->realm_context, rdp->realm_mpname,
                                        rdp->realm_name, (char **) NULL,
                                        &rdp->realm_mprinc))) {
        kdc_err(rdp->realm_context, kret,
                _("while setting up master key name %s for realm %s"),
                rdp->realm_mpname, realm);
        goto whoops;
    }

    /*
     * Get the master key (note, may not be the most current mkey).
     */
    if ((kret = krb5_db_fetch_mkey(rdp->realm_context, rdp->realm_mprinc,
                                   rdp->realm_mkey.enctype, manual,
                                   FALSE, rdp->realm_stash,
                                   &mkvno, NULL, &rdp->realm_mkey))) {
        kdc_err(rdp->realm_context, kret,
                _("while fetching master key %s for realm %s"),
                rdp->realm_mpname, realm);
        goto whoops;
    }

    if (krb5int_c_deprecated_enctype(rdp->realm_mkey.enctype)) {
        if (krb5_enctype_to_name(rdp->realm_mkey.enctype, FALSE, ename,
                                 sizeof(ename)))
            ename[0] = '\0';
        fprintf(stderr, _("Stash file %s uses DEPRECATED enctype %s!\n"),
                rdp->realm_stash, ename);
    }

    if ((kret = krb5_db_fetch_mkey_list(rdp->realm_context, rdp->realm_mprinc,
                                        &rdp->realm_mkey))) {
        kdc_err(rdp->realm_context, kret,
                _("while fetching master keys list for realm %s"), realm);
        goto whoops;
    }


    /* Set up the keytab */
    if ((kret = krb5_ktkdb_resolve(rdp->realm_context, NULL,
                                   &rdp->realm_keytab))) {
        kdc_err(rdp->realm_context, kret,
                _("while resolving kdb keytab for realm %s"), realm);
        goto whoops;
    }

    /* Preformat the TGS name */
    if ((kret = krb5_build_principal(rdp->realm_context, &rdp->realm_tgsprinc,
                                     strlen(realm), realm, KRB5_TGS_NAME,
                                     realm, (char *) NULL))) {
        kdc_err(rdp->realm_context, kret,
                _("while building TGS name for realm %s"), realm);
        goto whoops;
    }

whoops:
    /*
     * If we choked, then clean up any dirt we may have dropped on the floor.
     */
    if (kret) {

        finish_realm(rdp);
    }
    return(kret);
}

static void
on_monitor_signal(int signo)
{
    signal_received = signo;
}

static void
on_monitor_sighup(int signo)
{
    sighup_received = 1;
}

/*
 * Kill the worker subprocesses given by pids[0..bound-1], skipping any which
 * are set to -1, and wait for them to exit (so that we know the ports are no
 * longer in use).
 */
static void
terminate_workers(pid_t *pids, int bound)
{
    int i, status, num_active = 0;
    pid_t pid;

    /* Kill the active worker pids. */
    for (i = 0; i < bound; i++) {
        if (pids[i] == -1)
            continue;
        kill(pids[i], SIGTERM);
        num_active++;
    }

    /* Wait for them to exit. */
    while (num_active > 0) {
        pid = wait(&status);
        if (pid >= 0)
            num_active--;
    }
}

/*
 * Create num worker processes and return successfully in each child.  The
 * parent process will act as a supervisor and will only return from this
 * function in error cases.
 */
static krb5_error_code
create_workers(verto_ctx *ctx, int num)
{
    krb5_error_code retval;
    int i, status;
    pid_t pid, *pids;
#ifdef POSIX_SIGNALS
    struct sigaction s_action;
#endif /* POSIX_SIGNALS */

    /*
     * Setup our signal handlers which will forward to the children.
     * These handlers will be overridden in the child processes.
     */
#ifdef POSIX_SIGNALS
    (void) sigemptyset(&s_action.sa_mask);
    s_action.sa_flags = 0;
    s_action.sa_handler = on_monitor_signal;
    (void) sigaction(SIGINT, &s_action, (struct sigaction *) NULL);
    (void) sigaction(SIGTERM, &s_action, (struct sigaction *) NULL);
    (void) sigaction(SIGQUIT, &s_action, (struct sigaction *) NULL);
    s_action.sa_handler = on_monitor_sighup;
    (void) sigaction(SIGHUP, &s_action, (struct sigaction *) NULL);
#else  /* POSIX_SIGNALS */
    signal(SIGINT, on_monitor_signal);
    signal(SIGTERM, on_monitor_signal);
    signal(SIGQUIT, on_monitor_signal);
    signal(SIGHUP, on_monitor_sighup);
#endif /* POSIX_SIGNALS */

    /* Create child worker processes; return in each child. */
    krb5_klog_syslog(LOG_INFO, _("creating %d worker processes"), num);
    pids = calloc(num, sizeof(pid_t));
    if (pids == NULL)
        return ENOMEM;
    for (i = 0; i < num; i++) {
        pid = fork();
        if (pid == 0) {
            free(pids);
            if (!verto_reinitialize(ctx)) {
                krb5_klog_syslog(LOG_ERR,
                                 _("Unable to reinitialize main loop"));
                return ENOMEM;
            }
            retval = loop_setup_signals(ctx, &shandle, reset_for_hangup);
            if (retval) {
                krb5_klog_syslog(LOG_ERR, _("Unable to initialize signal "
                                            "handlers in pid %d"), pid);
                return retval;
            }

            /* Avoid race condition */
            if (signal_received)
                exit(0);

            /* Return control to main() in the new worker process. */
            return 0;
        }
        if (pid == -1) {
            /* Couldn't fork enough times. */
            status = errno;
            terminate_workers(pids, i);
            free(pids);
            return status;
        }
        pids[i] = pid;
    }

    /* We're going to use our own main loop here. */
    loop_free(ctx);

    /* Supervise the worker processes. */
    while (!signal_received) {
        /* Wait until a worker process exits or we get a signal. */
        pid = wait(&status);
        if (pid >= 0) {
            krb5_klog_syslog(LOG_ERR, _("worker %ld exited with status %d"),
                             (long) pid, status);

            /* Remove the pid from the table. */
            for (i = 0; i < num; i++) {
                if (pids[i] == pid)
                    pids[i] = -1;
            }

            /* When one worker process exits, terminate them all, so that KDC
             * crashes behave similarly with or without worker processes. */
            break;
        }

        /* Propagate HUP signal to worker processes if we received one. */
        if (sighup_received) {
            sighup_received = 0;
            for (i = 0; i < num; i++) {
                if (pids[i] != -1)
                    kill(pids[i], SIGHUP);
            }
        }
    }
    if (signal_received)
        krb5_klog_syslog(LOG_INFO, _("signal %d received in supervisor"),
                         signal_received);

    terminate_workers(pids, num);
    free(pids);
    exit(0);
}

static void
usage(char *name)
{
    fprintf(stderr,
            _("usage: %s [-x db_args]* [-d dbpathname] [-r dbrealmname]\n"
              "\t\t[-T time_offset] [-m] [-k masterenctype]\n"
              "\t\t[-M masterkeyname] [-p port] [-P pid_file]\n"
              "\t\t[-n] [-w numworkers] [/]\n\n"
              "where,\n"
              "\t[-x db_args]* - Any number of database specific arguments.\n"
              "\t\t\tLook at each database module documentation for "
              "\t\t\tsupported arguments\n"),
            name);
    exit(1);
}


static void
initialize_realms(krb5_context kcontext, int argc, char **argv,
                  int *tcp_listen_backlog_out)
{
    int                 c;
    char                *db_name = (char *) NULL;
    char                *lrealm = (char *) NULL;
    char                *mkey_name = (char *) NULL;
    krb5_error_code     retval;
    krb5_enctype        menctype = ENCTYPE_UNKNOWN;
    kdc_realm_t         *rdatap = NULL;
    krb5_boolean        manual = FALSE;
    krb5_boolean        def_restrict_anon;
    char                *def_udp_listen = NULL;
    char                *def_tcp_listen = NULL;
    krb5_pointer        aprof = kcontext->profile;
    const char          *hierarchy[3];
    char                *no_referral = NULL;
    char                *hostbased = NULL;
    int                  db_args_size = 0;
    char                **db_args = NULL;

    extern char *optarg;

    hierarchy[0] = KRB5_CONF_KDCDEFAULTS;
    hierarchy[1] = KRB5_CONF_KDC_LISTEN;
    hierarchy[2] = NULL;
    if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &def_udp_listen)) {
        hierarchy[1] = KRB5_CONF_KDC_PORTS;
        if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &def_udp_listen))
            def_udp_listen = NULL;
    }
    hierarchy[1] = KRB5_CONF_KDC_TCP_LISTEN;
    if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &def_tcp_listen)) {
        hierarchy[1] = KRB5_CONF_KDC_TCP_PORTS;
        if (krb5_aprof_get_string(aprof, hierarchy, TRUE, &def_tcp_listen))
            def_tcp_listen = NULL;
    }
    hierarchy[1] = KRB5_CONF_KDC_MAX_DGRAM_REPLY_SIZE;
    if (krb5_aprof_get_int32(aprof, hierarchy, TRUE, &max_dgram_reply_size))
        max_dgram_reply_size = MAX_DGRAM_SIZE;
    if (tcp_listen_backlog_out != NULL) {
        hierarchy[1] = KRB5_CONF_KDC_TCP_LISTEN_BACKLOG;
        if (krb5_aprof_get_int32(aprof, hierarchy, TRUE,
                                 tcp_listen_backlog_out))
            *tcp_listen_backlog_out = DEFAULT_TCP_LISTEN_BACKLOG;
    }
    hierarchy[1] = KRB5_CONF_RESTRICT_ANONYMOUS_TO_TGT;
    if (krb5_aprof_get_boolean(aprof, hierarchy, TRUE, &def_restrict_anon))
        def_restrict_anon = FALSE;
    hierarchy[1] = KRB5_CONF_NO_HOST_REFERRAL;
    if (krb5_aprof_get_string_all(aprof, hierarchy, &no_referral))
        no_referral = 0;
    hierarchy[1] = KRB5_CONF_HOST_BASED_SERVICES;
    if (krb5_aprof_get_string_all(aprof, hierarchy, &hostbased))
        hostbased = 0;

    if (def_udp_listen == NULL) {
        def_udp_listen = strdup(DEFAULT_KDC_UDP_PORTLIST);
        if (def_udp_listen == NULL) {
            fprintf(stderr, _(" KDC cannot initialize. Not enough memory\n"));
            exit(1);
        }
    }
    if (def_tcp_listen == NULL) {
        def_tcp_listen = strdup(DEFAULT_KDC_TCP_PORTLIST);
        if (def_tcp_listen == NULL) {
            fprintf(stderr, _(" KDC cannot initialize. Not enough memory\n"));
            exit(1);
        }
    }

    /*
     * Loop through the option list.  Each time we encounter a realm name, use
     * the previously scanned options to fill in for defaults.  We do this
     * twice if worker processes are used, so we must initialize optind.
     */
    optind = 1;
    while ((c = getopt(argc, argv, "x:r:d:mM:k:R:P:p:nw:4:T:X3")) != -1) {
        switch(c) {
        case 'x':
            db_args_size++;
            {
                char **temp = realloc( db_args, sizeof(char*) * (db_args_size+1)); /* one for NULL */
                if( temp == NULL )
                {
                    fprintf(stderr, _("%s: KDC cannot initialize. Not enough "
                                      "memory\n"), argv[0]);
                    exit(1);
                }

                db_args = temp;
            }
            db_args[db_args_size-1] = optarg;
            db_args[db_args_size]   = NULL;
            break;

        case 'r':                       /* realm name for db */
            if (!find_realm_data(&shandle, optarg, (krb5_ui_4) strlen(optarg))) {
                if ((rdatap = (kdc_realm_t *) malloc(sizeof(kdc_realm_t)))) {
                    retval = init_realm(rdatap, aprof, optarg, mkey_name,
                                        menctype, def_udp_listen,
                                        def_tcp_listen, manual,
                                        def_restrict_anon, db_args,
                                        no_referral, hostbased);
                    if (retval) {
                        fprintf(stderr, _("%s: cannot initialize realm %s - "
                                          "see log file for details\n"),
                                argv[0], optarg);
                        exit(1);
                    }
                    shandle.kdc_realmlist[shandle.kdc_numrealms] = rdatap;
                    shandle.kdc_numrealms++;
                    free(db_args), db_args=NULL, db_args_size = 0;
                }
                else
                {
                    fprintf(stderr, _("%s: cannot initialize realm %s. Not "
                                      "enough memory\n"), argv[0], optarg);
                    exit(1);
                }
            }
            break;
        case 'd':                       /* pathname for db */
            /* now db_name is not a separate argument.
             * It has to be passed as part of the db_args
             */
            if( db_name == NULL ) {
                if (asprintf(&db_name, "dbname=%s", optarg) < 0) {
                    fprintf(stderr, _("%s: KDC cannot initialize. Not enough "
                                      "memory\n"), argv[0]);
                    exit(1);
                }
            }

            db_args_size++;
            {
                char **temp = realloc( db_args, sizeof(char*) * (db_args_size+1)); /* one for NULL */
                if( temp == NULL )
                {
                    fprintf(stderr, _("%s: KDC cannot initialize. Not enough "
                                      "memory\n"), argv[0]);
                    exit(1);
                }

                db_args = temp;
            }
            db_args[db_args_size-1] = db_name;
            db_args[db_args_size]   = NULL;
            break;
        case 'm':                       /* manual type-in of master key */
            manual = TRUE;
            if (menctype == ENCTYPE_UNKNOWN)
                menctype = DEFAULT_KDC_ENCTYPE;
            break;
        case 'M':                       /* master key name in DB */
            mkey_name = optarg;
            break;
        case 'n':
            nofork++;                   /* don't detach from terminal */
            break;
        case 'w':                       /* create multiple worker processes */
            workers = atoi(optarg);
            if (workers <= 0)
                usage(argv[0]);
            break;
        case 'k':                       /* enctype for master key */
            if (krb5_string_to_enctype(optarg, &menctype))
                com_err(argv[0], 0, _("invalid enctype %s"), optarg);
            break;
        case 'R':
            /* Replay cache name; defunct since we don't use a replay cache. */
            break;
        case 'P':
            pid_file = optarg;
            break;
        case 'p':
            free(def_udp_listen);
            free(def_tcp_listen);
            def_udp_listen = strdup(optarg);
            def_tcp_listen = strdup(optarg);
            if (def_udp_listen == NULL || def_tcp_listen == NULL) {
                fprintf(stderr, _(" KDC cannot initialize. Not enough "
                                  "memory\n"));
                exit(1);
            }
            break;
        case 'T':
            time_offset = atoi(optarg);
            break;
        case '4':
            break;
        case 'X':
            break;
        case '?':
        default:
            usage(argv[0]);
        }
    }

    /*
     * Check to see if we processed any realms.
     */
    if (shandle.kdc_numrealms == 0) {
        /* no realm specified, use default realm */
        if ((retval = krb5_get_default_realm(kcontext, &lrealm))) {
            com_err(argv[0], retval,
                    _("while attempting to retrieve default realm"));
            fprintf (stderr,
                     _("%s: %s, attempting to retrieve default realm\n"),
                     argv[0], krb5_get_error_message(kcontext, retval));
            exit(1);
        }
        if ((rdatap = (kdc_realm_t *) malloc(sizeof(kdc_realm_t)))) {
            retval = init_realm(rdatap, aprof, lrealm, mkey_name, menctype,
                                def_udp_listen, def_tcp_listen, manual,
                                def_restrict_anon, db_args, no_referral,
                                hostbased);
            if (retval) {
                fprintf(stderr, _("%s: cannot initialize realm %s - see log "
                                  "file for details\n"), argv[0], lrealm);
                exit(1);
            }
            shandle.kdc_realmlist[0] = rdatap;
            shandle.kdc_numrealms++;
        }
        krb5_free_default_realm(kcontext, lrealm);
    }

    if (def_udp_listen)
        free(def_udp_listen);
    if (def_tcp_listen)
        free(def_tcp_listen);
    if (db_args)
        free(db_args);
    if (db_name)
        free(db_name);
    if (hostbased)
        free(hostbased);
    if (no_referral)
        free(no_referral);

    return;
}

static krb5_error_code
write_pid_file(const char *path)
{
    FILE *file;
    unsigned long pid;

    file = fopen(path, "w");
    if (file == NULL)
        return errno;
    pid = (unsigned long) getpid();
    if (fprintf(file, "%ld\n", pid) < 0 || fclose(file) == EOF)
        return errno;
    return 0;
}

static void
finish_realms()
{
    int i;

    for (i = 0; i < shandle.kdc_numrealms; i++) {
        finish_realm(shandle.kdc_realmlist[i]);
        shandle.kdc_realmlist[i] = 0;
    }
    shandle.kdc_numrealms = 0;
}

/*
  outline:

  process args & setup

  initialize database access (fetch master key, open DB)

  initialize network

  loop:
  listen for packet

  determine packet type, dispatch to handling routine
  (AS or TGS (or V4?))

  reflect response

  exit on signal

  clean up secrets, close db

  shut down network

  exit
*/

int main(int argc, char **argv)
{
    krb5_error_code     retval;
    krb5_context        kcontext;
    kdc_realm_t *realm;
    verto_ctx *ctx;
    int tcp_listen_backlog;
    int errout = 0;
    int i;

    setlocale(LC_ALL, "");
    if (strrchr(argv[0], '/'))
        argv[0] = strrchr(argv[0], '/')+1;

    shandle.kdc_realmlist = malloc(sizeof(kdc_realm_t *) *
                                   KRB5_KDC_MAX_REALMS);
    if (shandle.kdc_realmlist == NULL) {
        fprintf(stderr, _("%s: cannot get memory for realm list\n"), argv[0]);
        exit(1);
    }
    memset(shandle.kdc_realmlist, 0,
           (size_t) (sizeof(kdc_realm_t *) * KRB5_KDC_MAX_REALMS));

    /*
     * A note about Kerberos contexts: This context, "kcontext", is used
     * for the KDC operations, i.e. setup, network connection and error
     * reporting.  The per-realm operations use the "realm_context"
     * associated with each realm.
     */
    retval = krb5int_init_context_kdc(&kcontext);
    if (retval) {
        com_err(argv[0], retval, _("while initializing krb5"));
        exit(1);
    }
    krb5_klog_init(kcontext, "kdc", argv[0], 1);
    shandle.kdc_err_context = kcontext;
    kdc_progname = argv[0];
    /* N.B.: After this point, com_err sends output to the KDC log
       file, and not to stderr.  We use the kdc_err wrapper around
       com_err to ensure that the error state exists in the context
       known to the krb5_klog callback. */

    initialize_kdc5_error_table();

    /*
     * Scan through the argument list
     */
    initialize_realms(kcontext, argc, argv, &tcp_listen_backlog);

#ifndef NOCACHE
    retval = kdc_init_lookaside(kcontext);
    if (retval) {
        kdc_err(kcontext, retval, _("while initializing lookaside cache"));
        finish_realms();
        return 1;
    }
#endif

    ctx = loop_init(VERTO_EV_TYPE_NONE);
    if (!ctx) {
        kdc_err(kcontext, ENOMEM, _("while creating main loop"));
        finish_realms();
        return 1;
    }

    load_preauth_plugins(&shandle, kcontext, ctx);
    load_authdata_plugins(kcontext);
    retval = load_kdcpolicy_plugins(kcontext);
    if (retval) {
        kdc_err(kcontext, retval, _("while loading KDC policy plugin"));
        finish_realms();
        return 1;
    }

    /* Add each realm's listener addresses to the loop. */
    for (i = 0; i < shandle.kdc_numrealms; i++) {
        realm = shandle.kdc_realmlist[i];
        if (*realm->realm_listen != '\0') {
            retval = loop_add_udp_address(KRB5_DEFAULT_PORT,
                                          realm->realm_listen);
            if (retval)
                goto net_init_error;
        }
        if (*realm->realm_tcp_listen != '\0') {
            retval = loop_add_tcp_address(KRB5_DEFAULT_PORT,
                                          realm->realm_tcp_listen);
            if (retval)
                goto net_init_error;
        }
    }

    if (workers == 0) {
        retval = loop_setup_signals(ctx, &shandle, reset_for_hangup);
        if (retval) {
            kdc_err(kcontext, retval, _("while initializing signal handlers"));
            finish_realms();
            return 1;
        }
    }
    if ((retval = loop_setup_network(ctx, &shandle, kdc_progname,
                                     tcp_listen_backlog))) {
    net_init_error:
        kdc_err(kcontext, retval, _("while initializing network"));
        finish_realms();
        return 1;
    }

    /* Clean up realms for now and reinitialize them after daemonizing, since
     * some KDB modules are not fork-safe. */
    finish_realms();

    if (!nofork && daemon(0, 0)) {
        kdc_err(kcontext, errno, _("while detaching from tty"));
        return 1;
    }
    if (pid_file != NULL) {
        retval = write_pid_file(pid_file);
        if (retval) {
            kdc_err(kcontext, retval, _("while creating PID file"));
            finish_realms();
            return 1;
        }
    }
    if (workers > 0) {
        retval = create_workers(ctx, workers);
        if (retval) {
            kdc_err(kcontext, errno, _("creating worker processes"));
            return 1;
        }
    }

    initialize_realms(kcontext, argc, argv, NULL);

    /* Initialize audit system and audit KDC startup. */
    retval = load_audit_modules(kcontext);
    if (retval) {
        kdc_err(kcontext, retval, _("while loading audit plugin module(s)"));
        finish_realms();
        return 1;
    }
    krb5_klog_syslog(LOG_INFO, _("commencing operation"));
    if (nofork)
        fprintf(stderr, _("%s: starting...\n"), kdc_progname);
    kau_kdc_start(kcontext, TRUE);

    verto_run(ctx);
    kau_kdc_stop(kcontext, TRUE);
    krb5_klog_syslog(LOG_INFO, _("shutting down"));
    unload_preauth_plugins(kcontext);
    unload_authdata_plugins(kcontext);
    unload_kdcpolicy_plugins(kcontext);
    unload_audit_modules(kcontext);
    krb5_klog_close(kcontext);
    finish_realms();
    if (shandle.kdc_realmlist)
        free(shandle.kdc_realmlist);
#ifndef NOCACHE
    kdc_free_lookaside(kcontext);
#endif
    loop_free(ctx);
    krb5_free_context(kcontext);
    return errout;
}
