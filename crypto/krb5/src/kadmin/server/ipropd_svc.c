/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* #pragma ident	"@(#)ipropd_svc.c	1.2	04/02/20 SMI" */


#include "k5-platform.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h> /* rlimit */
#include <syslog.h>

#include <kadm5/admin.h>
#include <kadm5/kadm_rpc.h>
#include <kadm5/server_internal.h>
#include <adm_proto.h>
#include <string.h>
#include <gssapi_krb5.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <kdb_log.h>
#include "auth.h"
#include "misc.h"
#include "osconf.h"

extern gss_name_t rqst2name(struct svc_req *rqstp);

extern void *global_server_handle;
extern int nofork;
extern short l_port;
extern char *kdb5_util;
extern char *kprop;
extern char *dump_file;
extern char *kprop_port;

static char *reply_ok_str	= "UPDATE_OK";
static char *reply_err_str	= "UPDATE_ERROR";
static char *reply_fr_str	= "UPDATE_FULL_RESYNC_NEEDED";
static char *reply_busy_str	= "UPDATE_BUSY";
static char *reply_nil_str	= "UPDATE_NIL";
static char *reply_perm_str	= "UPDATE_PERM_DENIED";
static char *reply_unknown_str	= "<UNKNOWN_CODE>";

#define	LOG_UNAUTH  _("Unauthorized request: %s, client=%s, service=%s, addr=%s")
#define	LOG_DONE    _("Request: %s, %s, %s, client=%s, service=%s, addr=%s")

#ifdef	DPRINT
#undef	DPRINT
#endif
#ifdef DEBUG
#define	DPRINT(...)				\
    do {					\
	if (nofork) {				\
	    fprintf(stderr, __VA_ARGS__);	\
	    fflush(stderr);			\
	}					\
    } while (0)
#else
#define	DPRINT(...)
#endif

static void
debprret(char *w, update_status_t ret, kdb_sno_t sno)
{
    switch (ret) {
    case UPDATE_OK:
	printf("%s: end (OK, sno=%u)\n",
	       w, sno);
	break;
    case UPDATE_ERROR:
	printf("%s: end (ERROR)\n", w);
	break;
    case UPDATE_FULL_RESYNC_NEEDED:
	printf("%s: end (FR NEEDED)\n", w);
	break;
    case UPDATE_BUSY:
	printf("%s: end (BUSY)\n", w);
	break;
    case UPDATE_NIL:
	printf("%s: end (NIL)\n", w);
	break;
    case UPDATE_PERM_DENIED:
	printf("%s: end (PERM)\n", w);
	break;
    default:
	printf("%s: end (UNKNOWN return code (%d))\n", w, ret);
    }
}

static char *
replystr(update_status_t ret)
{
    switch (ret) {
    case UPDATE_OK:
	return (reply_ok_str);
    case UPDATE_ERROR:
	return (reply_err_str);
    case UPDATE_FULL_RESYNC_NEEDED:
	return (reply_fr_str);
    case UPDATE_BUSY:
	return (reply_busy_str);
    case UPDATE_NIL:
	return (reply_nil_str);
    case UPDATE_PERM_DENIED:
	return (reply_perm_str);
    default:
	return (reply_unknown_str);
    }
}

/* Returns null on allocation failure.
   Regardless of success or failure, frees the input buffer.  */
static char *
buf_to_string(gss_buffer_desc *b)
{
    OM_uint32 min_stat;
    char *s = malloc(b->length+1);

    if (s) {
	memcpy(s, b->value, b->length);
	s[b->length] = 0;
    }
    (void) gss_release_buffer(&min_stat, b);
    return s;
}

static krb5_boolean
iprop_acl_check(krb5_context context, const char *client_name)
{
    krb5_principal client_princ;
    krb5_boolean result;

    if (krb5_parse_name(context, client_name, &client_princ) != 0)
	return FALSE;
    result = auth(context, OP_IPROP, client_princ,
		  NULL, NULL, NULL, NULL, NULL, 0);
    krb5_free_principal(context, client_princ);
    return result;
}

kdb_incr_result_t *
iprop_get_updates_1_svc(kdb_last_t *arg, struct svc_req *rqstp)
{
    static kdb_incr_result_t ret;
    char *whoami = "iprop_get_updates_1";
    int kret;
    kadm5_server_handle_t handle = global_server_handle;
    char *client_name = 0, *service_name = 0;
    char obuf[256] = {0};

    /* default return code */
    ret.ret = UPDATE_ERROR;

    DPRINT("%s: start, last_sno=%lu\n", whoami,
	    (unsigned long)arg->last_sno);

    if (!handle) {
	krb5_klog_syslog(LOG_ERR,
			 _("%s: server handle is NULL"),
			 whoami);
	goto out;
    }

    {
	gss_buffer_desc client_desc, service_desc;

	if (setup_gss_names(rqstp, &client_desc, &service_desc) < 0) {
	    krb5_klog_syslog(LOG_ERR,
			     _("%s: setup_gss_names failed"),
			     whoami);
	    goto out;
	}
	client_name = buf_to_string(&client_desc);
	service_name = buf_to_string(&service_desc);
	if (client_name == NULL || service_name == NULL) {
	    krb5_klog_syslog(LOG_ERR,
			     _("%s: out of memory recording principal names"),
			     whoami);
	    goto out;
	}
    }

    DPRINT("%s: clprinc=`%s'\n\tsvcprinc=`%s'\n", whoami, client_name,
	   service_name);

    if (!iprop_acl_check(handle->context, client_name)) {
	ret.ret = UPDATE_PERM_DENIED;

	DPRINT("%s: PERMISSION DENIED: clprinc=`%s'\n\tsvcprinc=`%s'\n",
		whoami, client_name, service_name);

	krb5_klog_syslog(LOG_NOTICE, LOG_UNAUTH, whoami,
			 client_name, service_name,
			 client_addr(rqstp->rq_xprt));
	goto out;
    }

    kret = ulog_get_entries(handle->context, arg, &ret);

    if (ret.ret == UPDATE_OK) {
	(void) snprintf(obuf, sizeof (obuf),
			_("%s; Incoming SerialNo=%lu; Outgoing SerialNo=%lu"),
			replystr(ret.ret),
			(unsigned long)arg->last_sno,
			(unsigned long)ret.lastentry.last_sno);
    } else {
	(void) snprintf(obuf, sizeof (obuf),
			_("%s; Incoming SerialNo=%lu; Outgoing SerialNo=N/A"),
			replystr(ret.ret),
			(unsigned long)arg->last_sno);
    }

    DPRINT("%s: request %s %s\n\tclprinc=`%s'\n\tsvcprinc=`%s'\n",
	   whoami, obuf,
	   ((kret == 0) ? "success" : error_message(kret)),
	   client_name, service_name);

    krb5_klog_syslog(LOG_NOTICE,
		     _("Request: %s, %s, %s, client=%s, service=%s, addr=%s"),
		     whoami,
		     obuf,
		     ((kret == 0) ? "success" : error_message(kret)),
		     client_name, service_name,
		     client_addr(rqstp->rq_xprt));

out:
    if (nofork)
	debprret(whoami, ret.ret, ret.lastentry.last_sno);
    free(client_name);
    free(service_name);
    return (&ret);
}


/*
 * Given a client princ (foo/fqdn@R), copy (in arg cl) the fqdn substring.
 * Return arg cl str ptr on success, else NULL.
 */
static char *
getclhoststr(const char *clprinc, char *cl, size_t len)
{
    const char *s, *e;

    if ((s = strchr(clprinc, '/')) == NULL || (e = strchr(++s, '@')) == NULL ||
	(size_t)(e - s) >= len)
	return NULL;
    memcpy(cl, s, e - s);
    cl[e - s] = '\0';
    return (cl);
}

static kdb_fullresync_result_t *
ipropx_resync(uint32_t vers, struct svc_req *rqstp)
{
    static kdb_fullresync_result_t ret;
    char *ubuf = 0;
    char clhost[NI_MAXHOST] = {0};
    int pret, fret;
    FILE *p;
    kadm5_server_handle_t handle = global_server_handle;
    char *client_name = NULL, *service_name = NULL;
    char *whoami = "iprop_full_resync_1";

    /*
     * vers contains the highest version number the client is
     * willing to accept. A client can always accept a lower
     * version: the version number is indicated in the dump
     * header.
     */

    /* default return code */
    ret.ret = UPDATE_ERROR;

    if (!handle) {
	krb5_klog_syslog(LOG_ERR,
			 _("%s: server handle is NULL"),
			 whoami);
	goto out;
    }

    DPRINT("%s: start\n", whoami);

    {
	gss_buffer_desc client_desc, service_desc;

	if (setup_gss_names(rqstp, &client_desc, &service_desc) < 0) {
	    DPRINT("%s: setup_gss_names failed\n", whoami);
	    krb5_klog_syslog(LOG_ERR,
			     _("%s: setup_gss_names failed"),
			     whoami);
	    goto out;
	}
	client_name = buf_to_string(&client_desc);
	service_name = buf_to_string(&service_desc);
	if (client_name == NULL || service_name == NULL) {
	    DPRINT("%s: out of memory\n", whoami);
	    krb5_klog_syslog(LOG_ERR,
			     _("%s: out of memory recording principal names"),
			     whoami);
	    goto out;
	}
    }

    DPRINT("%s: clprinc=`%s'\n\tsvcprinc=`%s'\n",
	    whoami, client_name, service_name);

    if (!iprop_acl_check(handle->context, client_name)) {
	ret.ret = UPDATE_PERM_DENIED;

	DPRINT("%s: Permission denied\n", whoami);
	krb5_klog_syslog(LOG_NOTICE, LOG_UNAUTH, whoami,
			 client_name, service_name,
			 client_addr(rqstp->rq_xprt));
	goto out;
    }

    if (!getclhoststr(client_name, clhost, sizeof (clhost))) {
	krb5_klog_syslog(LOG_ERR,
			 _("%s: getclhoststr failed"),
			 whoami);
	goto out;
    }

    /*
     * Note the -i; modified version of kdb5_util dump format
     * to include sno (serial number). This argument is now
     * versioned (-i0 for legacy dump format, -i1 for ipropx
     * version 1 format, etc).
     *
     * The -c option ("conditional") causes the dump to dump only if no
     * dump already exists or that dump is not in ipropx format, or the
     * sno and timestamp in the header of that dump are outside the
     * ulog.  This allows us to share a single global dump with all
     * replicas, since it's OK to share an older dump, as long as its
     * sno and timestamp are in the ulog (then the replicas can get the
     * subsequent updates very iprop).
     */
    if (asprintf(&ubuf, "%s -r %s dump -i%d -c %s", kdb5_util,
		 handle->params.realm, vers, dump_file) < 0) {
	krb5_klog_syslog(LOG_ERR,
			 _("%s: cannot construct kdb5 util dump string too long; out of memory"),
			 whoami);
	goto out;
    }

    /*
     * Fork to dump the db and xfer it to the replica.
     * (the fork allows parent to return quickly and the child
     * acts like a callback to the replica).
     */
    fret = fork();
    DPRINT("%s: fork=%d (%d)\n", whoami, fret, getpid());

    switch (fret) {
    case -1: /* error */
	if (nofork) {
	    perror(whoami);
	}
	DPRINT("%s: fork failed\n", whoami);
	krb5_klog_syslog(LOG_ERR,
			 _("%s: fork failed: %s"),
			 whoami,
			 error_message(errno));
	goto out;

    case 0: /* child */
	DPRINT("%s: run `%s' ...\n", whoami, ubuf);
	(void) signal(SIGCHLD, SIG_DFL);
	/* run kdb5_util(1M) dump for IProp */
	p = popen(ubuf, "w");
	if (p == NULL) {
	    krb5_klog_syslog(LOG_ERR,
			     _("%s: popen failed: %s"),
			     whoami, error_message(errno));
	    _exit(1);
	}
	pret = pclose(p);
	DPRINT("%s: pclose=%d\n", whoami, pret);
	if (pret != 0) {
	    /* XXX popen/pclose may not set errno
	       properly, and the error could be from the
	       subprocess anyways.  */
	    if (nofork) {
		perror(whoami);
	    }
	    krb5_klog_syslog(LOG_ERR,
			     _("%s: pclose(popen) failed: %s"),
			     whoami,
			     error_message(errno));
	    _exit(1);
	}

	if (kprop_port != NULL) {
	    DPRINT("%s: exec `kprop -r %s -f %s -P %s %s' ...\n",
		   whoami, handle->params.realm, dump_file, kprop_port,
		   clhost);
	    pret = execl(kprop, "kprop", "-r", handle->params.realm, "-f",
			 dump_file, "-P", kprop_port, clhost, NULL);
	} else {
	    DPRINT("%s: exec `kprop -r %s -f %s %s' ...\n",
		   whoami, handle->params.realm, dump_file, clhost);
	    pret = execl(kprop, "kprop", "-r", handle->params.realm, "-f",
			 dump_file, clhost, NULL);
	}
	perror(whoami);
	krb5_klog_syslog(LOG_ERR,
			 _("%s: exec failed: %s"),
			 whoami,
			 error_message(errno));
	_exit(1);

    default: /* parent */
	ret.ret = UPDATE_OK;
	/* not used by replica (sno is retrieved from kdb5_util dump) */
	ret.lastentry.last_sno = 0;
	ret.lastentry.last_time.seconds = 0;
	ret.lastentry.last_time.useconds = 0;

	DPRINT("%s: spawned resync process %d, client=%s, "
		"service=%s, addr=%s\n", whoami, fret, client_name,
		service_name, client_addr(rqstp->rq_xprt));
	krb5_klog_syslog(LOG_NOTICE,
			 _("Request: %s, spawned resync process %d, client=%s, service=%s, addr=%s"),
			 whoami, fret,
			 client_name, service_name,
			 client_addr(rqstp->rq_xprt));

	goto out;
    }

out:
    if (nofork)
	debprret(whoami, ret.ret, 0);
    free(client_name);
    free(service_name);
    free(ubuf);
    return (&ret);
}

kdb_fullresync_result_t *
iprop_full_resync_1_svc(/* LINTED */ void *argp, struct svc_req *rqstp)
{
    return ipropx_resync(IPROPX_VERSION_0, rqstp);
}

kdb_fullresync_result_t *
iprop_full_resync_ext_1_svc(uint32_t *argp, struct svc_req *rqstp)
{
    return ipropx_resync(*argp, rqstp);
}

static int
check_iprop_rpcsec_auth(struct svc_req *rqstp)
{
    /* XXX Since the client can authenticate against any principal in
       the database, we need to do a sanity check.  Only checking for
       "kiprop" now, but that means theoretically the client could be
       authenticating to kiprop on some other machine.  */
    /* Code taken from kadm_rpc_svc.c, tweaked.  */

     gss_ctx_id_t ctx;
     krb5_context kctx;
     OM_uint32 maj_stat, min_stat;
     gss_name_t name;
     krb5_principal princ;
     int ret, success;
     krb5_data *c1, *realm;
     gss_buffer_desc gss_str;
     kadm5_server_handle_t handle;
     size_t slen;
     char *sdots;

     success = 0;
     handle = (kadm5_server_handle_t)global_server_handle;

     if (rqstp->rq_cred.oa_flavor != RPCSEC_GSS)
	  return 0;

     ctx = rqstp->rq_svccred;

     maj_stat = gss_inquire_context(&min_stat, ctx, NULL, &name,
				    NULL, NULL, NULL, NULL, NULL);
     if (maj_stat != GSS_S_COMPLETE) {
	  krb5_klog_syslog(LOG_ERR,
			   _("check_rpcsec_auth: failed inquire_context, "
			     "stat=%u"), maj_stat);
	  log_badauth(maj_stat, min_stat, rqstp->rq_xprt, NULL);
	  goto fail_name;
     }

     kctx = handle->context;
     ret = gss_to_krb5_name_1(rqstp, kctx, name, &princ, &gss_str);
     if (ret == 0)
	  goto fail_name;

     slen = gss_str.length;
     trunc_name(&slen, &sdots);
     /*
      * Since we accept with GSS_C_NO_NAME, the client can authenticate
      * against the entire kdb.  Therefore, ensure that the service
      * name is something reasonable.
      */
     if (krb5_princ_size(kctx, princ) != 2)
	  goto fail_princ;

     c1 = krb5_princ_component(kctx, princ, 0);
     realm = krb5_princ_realm(kctx, princ);
     if (strncmp(handle->params.realm, realm->data, realm->length) == 0
	 && strncmp("kiprop", c1->data, c1->length) == 0) {
	 success = 1;
     }

fail_princ:
     if (!success) {
	  krb5_klog_syslog(LOG_ERR, _("bad service principal %.*s%s"),
			   (int) slen, (char *) gss_str.value, sdots);
     }
     gss_release_buffer(&min_stat, &gss_str);
     krb5_free_principal(kctx, princ);
fail_name:
     gss_release_name(&min_stat, &name);
     return success;
}

void
krb5_iprop_prog_1(struct svc_req *rqstp,
		  SVCXPRT *transp)
{
    union {
	kdb_last_t iprop_get_updates_1_arg;
    } argument;
    void *result;
    bool_t (*_xdr_argument)(), (*_xdr_result)();
    void *(*local)(/* union XXX *, struct svc_req * */);
    char *whoami = "krb5_iprop_prog_1";

    if (!check_iprop_rpcsec_auth(rqstp)) {
	krb5_klog_syslog(LOG_ERR, _("authentication attempt failed: %s, RPC "
				    "authentication flavor %d"),
			 client_addr(rqstp->rq_xprt),
			 rqstp->rq_cred.oa_flavor);
	svcerr_weakauth(transp);
	return;
    }

    switch (rqstp->rq_proc) {
    case NULLPROC:
	(void) svc_sendreply(transp, xdr_void,
			     (char *)NULL);
	return;

    case IPROP_GET_UPDATES:
	_xdr_argument = xdr_kdb_last_t;
	_xdr_result = xdr_kdb_incr_result_t;
	local = (void *(*)()) iprop_get_updates_1_svc;
	break;

    case IPROP_FULL_RESYNC:
	_xdr_argument = xdr_void;
	_xdr_result = xdr_kdb_fullresync_result_t;
	local = (void *(*)()) iprop_full_resync_1_svc;
	break;

    case IPROP_FULL_RESYNC_EXT:
	_xdr_argument = xdr_u_int32;
	_xdr_result = xdr_kdb_fullresync_result_t;
	local = (void *(*)()) iprop_full_resync_ext_1_svc;
	break;

    default:
	krb5_klog_syslog(LOG_ERR,
			 _("RPC unknown request: %d (%s)"),
			 rqstp->rq_proc, whoami);
	svcerr_noproc(transp);
	return;
    }
    (void) memset(&argument, 0, sizeof (argument));
    if (!svc_getargs(transp, _xdr_argument, (caddr_t)&argument)) {
	krb5_klog_syslog(LOG_ERR,
			 _("RPC svc_getargs failed (%s)"),
			 whoami);
	svcerr_decode(transp);
	return;
    }
    result = (*local)(&argument, rqstp);

    if (_xdr_result && result != NULL &&
	!svc_sendreply(transp, _xdr_result, result)) {
	krb5_klog_syslog(LOG_ERR,
			 _("RPC svc_sendreply failed (%s)"),
			 whoami);
	svcerr_systemerr(transp);
    }
    if (!svc_freeargs(transp, _xdr_argument, (caddr_t)&argument)) {
	krb5_klog_syslog(LOG_ERR,
			 _("RPC svc_freeargs failed (%s)"),
			 whoami);

	exit(1);
    }

    if (rqstp->rq_proc == IPROP_GET_UPDATES) {
	/* LINTED */
	kdb_incr_result_t *r = (kdb_incr_result_t *)result;

	if (r->ret == UPDATE_OK) {
	    ulog_free_entries(r->updates.kdb_ulog_t_val,
			      r->updates.kdb_ulog_t_len);
	    r->updates.kdb_ulog_t_val = NULL;
	    r->updates.kdb_ulog_t_len = 0;
	}
    }

}
