/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <k5-platform.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#ifdef _AIX
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <gssrpc/rpc.h>
#include <gssapi/gssapi.h>
#include "gssapiP_krb5.h" /* for kg_get_context */
#include <gssrpc/auth_gssapi.h>
#include <kadm5/admin.h>
#include <kadm5/kadm_rpc.h>
#include <kadm5/server_acl.h>
#include <adm_proto.h>
#include "kdb_kt.h"  /* for krb5_ktkdb_set_context */
#include <string.h>
#include "kadm5/server_internal.h" /* XXX for kadm5_server_handle_t */
#include <kdb_log.h>

#include "misc.h"

#if defined(NEED_DAEMON_PROTO)
int daemon(int, int);
#endif

#define TIMEOUT 15

gss_name_t gss_changepw_name = NULL, gss_oldchangepw_name = NULL;
void *global_server_handle;
int nofork = 0;
char *kdb5_util = KPROPD_DEFAULT_KDB5_UTIL;
char *kprop = KPROPD_DEFAULT_KPROP;
char *dump_file = KPROP_DEFAULT_FILE;
char *kprop_port = NULL;

static krb5_context context;
static char *progname;

#ifdef USE_PASSWORD_SERVER
void kadm5_set_use_password_server(void);
#endif

static void
usage()
{
    fprintf(stderr, _("Usage: kadmind [-x db_args]* [-r realm] [-m] [-nofork] "
                      "[-port port-number]\n"
                      "\t\t[-proponly] [-p path-to-kdb5_util] [-F dump-file]\n"
                      "\t\t[-K path-to-kprop] [-k kprop-port] [-P pid_file]\n"
                      "\nwhere,\n\t[-x db_args]* - any number of database "
                      "specific arguments.\n"
                      "\t\t\tLook at each database documentation for "
                      "supported arguments\n"));
    exit(1);
}

/*
 * Output a message to stderr and the admin server log, and exit with status 1.
 * msg should not be punctuated.  If code is given, msg should indicate what
 * operation was taking place in the present progressive.  Otherwise msg should
 * be capitalized and should indicate what went wrong.
 */
static void
fail_to_start(krb5_error_code code, const char *msg)
{
    const char *errmsg;

    fprintf(stderr, "%s: ", progname);
    if (code) {
        errmsg = krb5_get_error_message(context, code);
        fprintf(stderr, _("%s: %s while %s, aborting\n"), progname, errmsg,
                msg);
        krb5_klog_syslog(LOG_ERR, _("%s while %s, aborting\n"), errmsg, msg);
    } else {
        fprintf(stderr, _("%s: %s, aborting\n"), progname, msg);
        krb5_klog_syslog(LOG_ERR, _("%s, aborting"), msg);
    }
    exit(1);
}

static int
write_pid_file(const char *pid_file)
{
    FILE *file;
    unsigned long pid;
    int st1, st2;

    file = fopen(pid_file, "w");
    if (file == NULL)
        return errno;
    pid = (unsigned long)getpid();
    st1 = (fprintf(file, "%ld\n", pid) < 0) ? errno : 0;
    st2 = (fclose(file) == EOF) ? errno : 0;
    return st1 ? st1 : st2;
}

/* Set up the main loop.  If proponly is set, don't set up ports for kpasswd or
 * kadmin.  May set *ctx_out even on error. */
static krb5_error_code
setup_loop(int proponly, verto_ctx **ctx_out)
{
    krb5_error_code ret;
    verto_ctx *ctx;
    kadm5_server_handle_t handle = global_server_handle;

    *ctx_out = ctx = loop_init(VERTO_EV_TYPE_SIGNAL);
    if (ctx == NULL)
        return ENOMEM;
    ret = loop_setup_signals(ctx, global_server_handle, NULL);
    if (ret)
        return ret;
    if (!proponly) {
        ret = loop_add_udp_address(handle->params.kpasswd_port,
                                   handle->params.kpasswd_listen);
        if (ret)
            return ret;
        ret = loop_add_tcp_address(handle->params.kpasswd_port,
                                   handle->params.kpasswd_listen);
        if (ret)
            return ret;
        ret = loop_add_rpc_service(handle->params.kadmind_port,
                                   handle->params.kadmind_listen,
                                   KADM, KADMVERS, kadm_1);
        if (ret)
            return ret;
    }
#ifndef DISABLE_IPROP
    if (handle->params.iprop_enabled) {
        ret = loop_add_rpc_service(handle->params.iprop_port,
                                   handle->params.iprop_listen,
                                   KRB5_IPROP_PROG, KRB5_IPROP_VERS,
                                   krb5_iprop_prog_1);
        if (ret)
            return ret;
    }
#endif
    return loop_setup_network(ctx, global_server_handle, progname,
                              DEFAULT_TCP_LISTEN_BACKLOG);
}

/* Point GSSAPI at the KDB keytab so we don't need an actual file keytab. */
static krb5_error_code
setup_kdb_keytab()
{
    krb5_error_code ret;

    ret = krb5_ktkdb_set_context(context);
    if (ret)
        return ret;
    ret = krb5_db_register_keytab(context);
    if (ret)
        return ret;
    return krb5_gss_register_acceptor_identity("KDB:");
}


/* Return "name@realm". */
static char *
build_princ_name(char *name, char *realm)
{
    char *fullname;

    if (asprintf(&fullname, "%s@%s", name, realm) < 0)
        return NULL;
    return fullname;
}

/* Callback from GSSRPC for garbled/forged/replayed/etc messages. */
static void
log_badverf(gss_name_t client_name, gss_name_t server_name,
            struct svc_req *rqst, struct rpc_msg *msg, char *data)
{
    static const struct {
        rpcproc_t proc;
        const char *proc_name;
    } proc_names[] = {
        {1, "CREATE_PRINCIPAL"},
        {2, "DELETE_PRINCIPAL"},
        {3, "MODIFY_PRINCIPAL"},
        {4, "RENAME_PRINCIPAL"},
        {5, "GET_PRINCIPAL"},
        {6, "CHPASS_PRINCIPAL"},
        {7, "CHRAND_PRINCIPAL"},
        {8, "CREATE_POLICY"},
        {9, "DELETE_POLICY"},
        {10, "MODIFY_POLICY"},
        {11, "GET_POLICY"},
        {12, "GET_PRIVS"},
        {13, "INIT"},
        {14, "GET_PRINCS"},
        {15, "GET_POLS"},
        {16, "SETKEY_PRINCIPAL"},
        {17, "SETV4KEY_PRINCIPAL"},
        {18, "CREATE_PRINCIPAL3"},
        {19, "CHPASS_PRINCIPAL3"},
        {20, "CHRAND_PRINCIPAL3"},
        {21, "SETKEY_PRINCIPAL3"},
        {22, "PURGEKEYS"},
        {23, "GET_STRINGS"},
        {24, "SET_STRING"}
    };
    OM_uint32 minor;
    gss_buffer_desc client, server;
    gss_OID gss_type;
    const char *a;
    rpcproc_t proc;
    unsigned int i;
    const char *procname;
    size_t clen, slen;
    char *cdots, *sdots;

    client.length = 0;
    client.value = NULL;
    server.length = 0;
    server.value = NULL;

    (void)gss_display_name(&minor, client_name, &client, &gss_type);
    (void)gss_display_name(&minor, server_name, &server, &gss_type);
    if (client.value == NULL) {
        client.value = "(null)";
        clen = sizeof("(null)") - 1;
    } else {
        clen = client.length;
    }
    trunc_name(&clen, &cdots);
    if (server.value == NULL) {
        server.value = "(null)";
        slen = sizeof("(null)") - 1;
    } else {
        slen = server.length;
    }
    trunc_name(&slen, &sdots);
    a = client_addr(rqst->rq_xprt);

    proc = msg->rm_call.cb_proc;
    procname = NULL;
    for (i = 0; i < sizeof(proc_names) / sizeof(*proc_names); i++) {
        if (proc_names[i].proc == proc) {
            procname = proc_names[i].proc_name;
            break;
        }
    }
    if (procname != NULL) {
        krb5_klog_syslog(LOG_NOTICE,
                         _("WARNING! Forged/garbled request: %s, claimed "
                           "client = %.*s%s, server = %.*s%s, addr = %s"),
                         procname, (int)clen, (char *)client.value, cdots,
                         (int)slen, (char *)server.value, sdots, a);
    } else {
        krb5_klog_syslog(LOG_NOTICE,
                         _("WARNING! Forged/garbled request: %d, claimed "
                           "client = %.*s%s, server = %.*s%s, addr = %s"),
                         proc, (int)clen, (char *)client.value, cdots,
                         (int)slen, (char *)server.value, sdots, a);
    }

    (void)gss_release_buffer(&minor, &client);
    (void)gss_release_buffer(&minor, &server);
}

/* Callback from GSSRPC for miscellaneous errors */
static void
log_miscerr(struct svc_req *rqst, struct rpc_msg *msg, char *error, char *data)
{
    krb5_klog_syslog(LOG_NOTICE, _("Miscellaneous RPC error: %s, %s"),
                     client_addr(rqst->rq_xprt), error);
}

static void
log_badauth_display_status_1(char *m, OM_uint32 code, int type)
{
    OM_uint32 gssstat, minor_stat;
    gss_buffer_desc msg;
    OM_uint32 msg_ctx;

    msg_ctx = 0;
    while (1) {
        gssstat = gss_display_status(&minor_stat, code, type, GSS_C_NULL_OID,
                                     &msg_ctx, &msg);
        if (gssstat != GSS_S_COMPLETE) {
            krb5_klog_syslog(LOG_ERR, _("%s Cannot decode status %d"), m,
                             (int)code);
            return;
        }

        krb5_klog_syslog(LOG_NOTICE, "%s %.*s", m, (int)msg.length,
                         (char *)msg.value);
        (void)gss_release_buffer(&minor_stat, &msg);

        if (!msg_ctx)
            break;
    }
}

/* Callback from GSSRPC for authentication failures */
void
log_badauth(OM_uint32 major, OM_uint32 minor, SVCXPRT *xprt, char *data)
{
    krb5_klog_syslog(LOG_NOTICE, _("Authentication attempt failed: %s, "
                                   "GSS-API error strings are:"),
                     client_addr(xprt));
    log_badauth_display_status_1("   ", major, GSS_C_GSS_CODE);
    log_badauth_display_status_1("   ", minor, GSS_C_MECH_CODE);
    krb5_klog_syslog(LOG_NOTICE, _("   GSS-API error strings complete."));
}

int
main(int argc, char *argv[])
{
    OM_uint32 minor_status;
    gss_buffer_desc in_buf;
    gss_OID nt_krb5_name_oid = (gss_OID)GSS_KRB5_NT_PRINCIPAL_NAME;
    auth_gssapi_name names[4];
    kadm5_config_params params;
    verto_ctx *vctx;
    const char *pid_file = NULL;
    char **db_args = NULL, **tmpargs;
    int ret, i, db_args_size = 0, strong_random = 1, proponly = 0;

    setlocale(LC_ALL, "");
    setvbuf(stderr, NULL, _IONBF, 0);

    names[0].name = names[1].name = names[2].name = names[3].name = NULL;
    names[0].type = names[1].type = names[2].type = names[3].type =
        nt_krb5_name_oid;

    progname = (strrchr(argv[0], '/') != NULL) ? strrchr(argv[0], '/') + 1 :
        argv[0];

    memset(&params, 0, sizeof(params));

    argc--, argv++;
    while (argc) {
        if (strcmp(*argv, "-x") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            db_args_size++;
            tmpargs = realloc(db_args, sizeof(char *) * (db_args_size + 1));
            if (tmpargs == NULL) {
                fprintf(stderr, _("%s: cannot initialize. Not enough "
                                  "memory\n"), progname);
                exit(1);
            }
            db_args = tmpargs;
            db_args[db_args_size - 1] = *argv;
            db_args[db_args_size] = NULL;
        } else if (strcmp(*argv, "-r") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            params.realm = *argv;
            params.mask |= KADM5_CONFIG_REALM;
            argc--, argv++;
            continue;
        } else if (strcmp(*argv, "-m") == 0) {
            params.mkey_from_kbd = 1;
            params.mask |= KADM5_CONFIG_MKEY_FROM_KBD;
        } else if (strcmp(*argv, "-nofork") == 0) {
            nofork = 1;
#ifdef USE_PASSWORD_SERVER
        } else if (strcmp(*argv, "-passwordserver") == 0) {
            kadm5_set_use_password_server();
#endif
#ifndef DISABLE_IPROP
        } else if (strcmp(*argv, "-proponly") == 0) {
            proponly = 1;
#endif
        } else if (strcmp(*argv, "-port") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            params.kadmind_port = atoi(*argv);
            params.mask |= KADM5_CONFIG_KADMIND_PORT;
        } else if (strcmp(*argv, "-P") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            pid_file = *argv;
        } else if (strcmp(*argv, "-W") == 0) {
            strong_random = 0;
        } else if (strcmp(*argv, "-p") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            kdb5_util = *argv;
        } else if (strcmp(*argv, "-F") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            dump_file = *argv;
        } else if (strcmp(*argv, "-K") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            kprop = *argv;
        } else if (strcmp(*argv, "-k") == 0) {
            argc--, argv++;
            if (!argc)
                usage();
            kprop_port = *argv;
        } else {
            break;
        }
        argc--, argv++;
    }

    if (argc != 0)
        usage();

    ret = kadm5_init_krb5_context(&context);
    if (ret) {
        fprintf(stderr, _("%s: %s while initializing context, aborting\n"),
                progname, error_message(ret));
        exit(1);
    }

    krb5_klog_init(context, "admin_server", progname, 1);

    ret = kadm5_init(context, "kadmind", NULL, NULL, &params,
                     KADM5_STRUCT_VERSION, KADM5_API_VERSION_4, db_args,
                     &global_server_handle);
    if (ret)
        fail_to_start(ret, _("initializing"));

    ret = kadm5_get_config_params(context, 1, &params, &params);
    if (ret)
        fail_to_start(ret, _("getting config parameters"));
    if (!(params.mask & KADM5_CONFIG_REALM))
        fail_to_start(0, _("Missing required realm configuration"));
    if (!(params.mask & KADM5_CONFIG_ACL_FILE))
        fail_to_start(0, _("Missing required ACL file configuration"));

    ret = setup_loop(proponly, &vctx);
    if (ret)
        fail_to_start(ret, _("initializing network"));

    names[0].name = build_princ_name(KADM5_ADMIN_SERVICE, params.realm);
    names[1].name = build_princ_name(KADM5_CHANGEPW_SERVICE, params.realm);
    if (names[0].name == NULL || names[1].name == NULL)
        fail_to_start(0, _("Cannot build GSSAPI auth names"));

    ret = setup_kdb_keytab();
    if (ret)
        fail_to_start(0, _("Cannot set up KDB keytab"));

    if (svcauth_gssapi_set_names(names, 2) == FALSE)
        fail_to_start(0, _("Cannot set GSSAPI authentication names"));

    /* if set_names succeeded, this will too */
    in_buf.value = names[1].name;
    in_buf.length = strlen(names[1].name) + 1;
    (void)gss_import_name(&minor_status, &in_buf, nt_krb5_name_oid,
                          &gss_changepw_name);

    svcauth_gssapi_set_log_badauth2_func(log_badauth, NULL);
    svcauth_gssapi_set_log_badverf_func(log_badverf, NULL);
    svcauth_gssapi_set_log_miscerr_func(log_miscerr, NULL);

    svcauth_gss_set_log_badauth2_func(log_badauth, NULL);
    svcauth_gss_set_log_badverf_func(log_badverf, NULL);
    svcauth_gss_set_log_miscerr_func(log_miscerr, NULL);

    if (svcauth_gss_set_svc_name(GSS_C_NO_NAME) != TRUE)
        fail_to_start(0, _("Cannot initialize GSSAPI service name"));

    ret = kadm5int_acl_init(context, 0, params.acl_file);
    if (ret)
        fail_to_start(ret, _("initializing ACL file"));

    if (!nofork && daemon(0, 0) != 0)
        fail_to_start(errno, _("spawning daemon process"));
    if (pid_file != NULL) {
        ret = write_pid_file(pid_file);
        if (ret)
            fail_to_start(ret, _("creating PID file"));
    }

    krb5_klog_syslog(LOG_INFO, _("Seeding random number generator"));
    ret = krb5_c_random_os_entropy(context, strong_random, NULL);
    if (ret)
        fail_to_start(ret, _("getting random seed"));

    if (params.iprop_enabled == TRUE) {
        ulog_set_role(context, IPROP_MASTER);

        ret = ulog_map(context, params.iprop_logfile, params.iprop_ulogsize);
        if (ret)
            fail_to_start(ret, _("mapping update log"));

        if (nofork) {
            fprintf(stderr,
                    _("%s: create IPROP svc (PROG=%d, VERS=%d)\n"),
                    progname, KRB5_IPROP_PROG, KRB5_IPROP_VERS);
        }
    }

    if (kprop_port == NULL)
        kprop_port = getenv("KPROP_PORT");

    krb5_klog_syslog(LOG_INFO, _("starting"));
    if (nofork)
        fprintf(stderr, _("%s: starting...\n"), progname);

    verto_run(vctx);
    krb5_klog_syslog(LOG_INFO, _("finished, exiting"));

    /* Clean up memory, etc */
    svcauth_gssapi_unset_names();
    kadm5_destroy(global_server_handle);
    loop_free(vctx);
    kadm5int_acl_finish(context, 0);
    (void)gss_release_name(&minor_status, &gss_changepw_name);
    (void)gss_release_name(&minor_status, &gss_oldchangepw_name);
    for (i = 0; i < 4; i++)
        free(names[i].name);

    krb5_klog_close(context);
    krb5_free_context(context);
    exit(2);
}
