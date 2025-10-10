/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Extensively modified from /usr/src/usr.sbin/gssd.c r344402 for
 * the server side of kernel RPC-over-TLS by Rick Macklem.
 */

#include <sys/types.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <libutil.h>
#include <netdb.h>
#include <pthread.h>
#include <pwd.h>
#include <stdbool.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/rpcsec_tls.h>

#include <openssl/opensslconf.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "rpctlssd.h"
#include "rpc.tlscommon.h"

#ifndef	_PATH_CERTANDKEY
#define	_PATH_CERTANDKEY	"/etc/rpc.tlsservd/"
#endif
#ifndef	_PATH_RPCTLSSDPID
#define	_PATH_RPCTLSSDPID	"/var/run/rpc.tlsservd.pid"
#endif
#ifndef	_PREFERRED_CIPHERS
#define	_PREFERRED_CIPHERS	"AES128-GCM-SHA256"
#endif

/* Global variables also used by rpc.tlscommon.c. */
int			rpctls_debug_level;
bool			rpctls_verbose;
SSL_CTX			*rpctls_ctx = NULL;
const char		*rpctls_verify_cafile = NULL;
const char		*rpctls_verify_capath = NULL;
char			*rpctls_crlfile = NULL;
bool			rpctls_gothup = false;

static SVCXPRT		*xprt;
static pthread_key_t	xidkey;
struct ssl_list		rpctls_ssllist;
static pthread_rwlock_t	rpctls_rwlock;
static u_int		rpctls_nthreads = 0;
static pthread_mutex_t	rpctls_mtx;
static pthread_cond_t	rpctls_cv;

static struct pidfh	*rpctls_pfh = NULL;
static bool		rpctls_do_mutual = false;
static const char	*rpctls_certdir = _PATH_CERTANDKEY;
static bool		rpctls_comparehost = false;
static unsigned int	rpctls_wildcard = X509_CHECK_FLAG_NO_WILDCARDS;
static bool		rpctls_cnuser = false;
static char		*rpctls_dnsname;
static const char	*rpctls_cnuseroid = "1.3.6.1.4.1.2238.1.1.1";
static const char	*rpctls_ciphers = NULL;
static int		rpctls_mintls = TLS1_3_VERSION;
static u_int		rpctls_maxthreads;

static void		rpctls_cleanup_term(int sig);
static SSL_CTX		*rpctls_setup_ssl(const char *certdir);
static SSL		*rpctls_server(SSL_CTX *ctx, int s,
			    uint32_t *flags, uint32_t *uidp,
			    int *ngrps, uint32_t *gidp, X509 **certp);
static int		rpctls_cnname(X509 *cert, uint32_t *uidp,
			    int *ngrps, uint32_t *gidp);
static char		*rpctls_getdnsname(char *dnsname);
static void		rpctls_huphandler(int sig __unused);

extern void		rpctlssd_2(struct svc_req *rqstp, SVCXPRT *transp);

static void *dummy_thread(void *v __unused) { return (NULL); }

static struct option longopts[] = {
	{ "allowtls1_2",	no_argument,		NULL,	'2' },
	{ "ciphers",		required_argument,	NULL,	'C' },
	{ "certdir",		required_argument,	NULL,	'D' },
	{ "debuglevel",		no_argument,		NULL,	'd' },
	{ "checkhost",		no_argument,		NULL,	'h' },
	{ "verifylocs",		required_argument,	NULL,	'l' },
	{ "mutualverf",		no_argument,		NULL,	'm' },
	{ "maxthreads",		required_argument,	NULL,	'N' },
	{ "domain",		required_argument,	NULL,	'n' },
	{ "verifydir",		required_argument,	NULL,	'p' },
	{ "crl",		required_argument,	NULL,	'r' },
	{ "certuser",		no_argument,		NULL,	'u' },
	{ "verbose",		no_argument,		NULL,	'v' },
	{ "multiwild",		no_argument,		NULL,	'W' },
	{ "singlewild",		no_argument,		NULL,	'w' },
	{ NULL,			0,			NULL,	0  }
};

int
main(int argc, char **argv)
{
	int ch;
	char hostname[MAXHOSTNAMELEN + 2];
	pid_t otherpid;
	pthread_t tid;
	bool tls_enable;
	size_t tls_enable_len;
	u_int ncpu;

	/* Check that another rpctlssd isn't already running. */
	rpctls_pfh = pidfile_open(_PATH_RPCTLSSDPID, 0600, &otherpid);
	if (rpctls_pfh == NULL) {
		if (errno == EEXIST)
			errx(1, "rpctlssd already running, pid: %d.", otherpid);
		warn("cannot open or create pidfile");
	}

	/* Check to see that the ktls is enabled. */
	tls_enable_len = sizeof(tls_enable);
	if (sysctlbyname("kern.ipc.tls.enable", &tls_enable, &tls_enable_len,
	    NULL, 0) != 0 || !tls_enable)
		errx(1, "Kernel TLS not enabled");

	/* Set the dns name for the server. */
	rpctls_dnsname = rpctls_getdnsname(hostname);
	if (rpctls_dnsname == NULL) {
		strcpy(hostname, "@default.domain");
		rpctls_dnsname = hostname;
	}

	rpctls_verbose = false;
	ncpu = (u_int)sysconf(_SC_NPROCESSORS_ONLN);
#ifdef notnow
	rpctls_maxthreads = ncpu > 1 ? ncpu / 2 : 1;
#else
	/* XXX For now, until fixed properly!! */
	rpctls_maxthreads = 1;
#endif

	while ((ch = getopt_long(argc, argv, "2C:D:dhl:N:n:mp:r:uvWw", longopts,
	    NULL)) != -1) {
		switch (ch) {
		case '2':
			rpctls_mintls = TLS1_2_VERSION;
			break;
		case 'C':
			rpctls_ciphers = optarg;
			break;
		case 'D':
			rpctls_certdir = optarg;
			break;
		case 'd':
			rpctls_debug_level++;
			break;
		case 'h':
			rpctls_comparehost = true;
			break;
		case 'l':
			rpctls_verify_cafile = optarg;
			break;
		case 'm':
			rpctls_do_mutual = true;
			break;
		case 'N':
			rpctls_maxthreads = atoi(optarg);
			if (rpctls_maxthreads < 1 || rpctls_maxthreads > ncpu)
				errx(1, "maximum threads must be between 1 and "
				    "number of CPUs (%d)", ncpu);
			/* XXX For now, until fixed properly!! */
			rpctls_maxthreads = 1;
			break;
		case 'n':
			hostname[0] = '@';
			strlcpy(&hostname[1], optarg, MAXHOSTNAMELEN + 1);
			rpctls_dnsname = hostname;
			break;
		case 'p':
			rpctls_verify_capath = optarg;
			break;
		case 'r':
			rpctls_crlfile = optarg;
			break;
		case 'u':
			rpctls_cnuser = true;
			break;
		case 'v':
			rpctls_verbose = true;
			break;
		case 'W':
			if (rpctls_wildcard != X509_CHECK_FLAG_NO_WILDCARDS)
				errx(1, "options -w and -W are mutually "
				    "exclusive");
			rpctls_wildcard = X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS;
			break;
		case 'w':
			if (rpctls_wildcard != X509_CHECK_FLAG_NO_WILDCARDS)
				errx(1, "options -w and -W are mutually "
				    "exclusive");
			rpctls_wildcard = 0;
			break;
		default:
			fprintf(stderr, "usage: %s "
			    "[-2/--allowtls1_2] "
			    "[-C/--ciphers available_ciphers] "
			    "[-D/--certdir certdir] [-d/--debuglevel] "
			    "[-h/--checkhost] "
			    "[-l/--verifylocs CAfile] [-m/--mutualverf] "
			    "[-n/--domain domain_name] "
			    "[-p/--verifydir CApath] [-r/--crl CRLfile] "
			    "[-u/--certuser] [-v/--verbose] [-W/--multiwild] "
			    "[-w/--singlewild]\n", argv[0]);
			exit(1);
		}
	}
	if (rpctls_do_mutual && rpctls_verify_cafile == NULL &&
	    rpctls_verify_capath == NULL)
		errx(1, "-m requires the -l <CAfile> and/or "
		    "-p <CApath> options");
	if (rpctls_comparehost && (!rpctls_do_mutual ||
	    (rpctls_verify_cafile == NULL && rpctls_verify_capath == NULL)))
		errx(1, "-h requires the -m plus the "
		    "-l <CAfile> and/or -p <CApath> options");
	if (!rpctls_comparehost && rpctls_wildcard !=
	    X509_CHECK_FLAG_NO_WILDCARDS)
		errx(1, "The -w or -W options require the -h option");
	if (rpctls_cnuser && (!rpctls_do_mutual ||
	    (rpctls_verify_cafile == NULL && rpctls_verify_capath == NULL)))
		errx(1, "-u requires the -m plus the "
		    "-l <CAfile> and/or -p <CApath> options");

	if (modfind("krpc") < 0) {
		/* Not present in kernel, try loading it */
		if (kldload("krpc") < 0 || modfind("krpc") < 0)
			errx(1, "Kernel RPC is not available");
	}
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, rpctls_huphandler);
	signal(SIGTERM, rpctls_cleanup_term);

	if (rpctls_debug_level == 0 && daemon(0, 0) != 0)
		err(1, "Can't daemonize");
	pidfile_write(rpctls_pfh);

	/*
	 * XXX: Push libc internal state into threaded mode before creating
	 * the threaded svc_nl xprt.
	 */
	(void)pthread_create(&tid, NULL, dummy_thread, NULL);
	(void)pthread_join(tid, NULL);
	if ((xprt = svc_nl_create("tlsserv")) == NULL) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't create transport for local rpctlssd socket");
			exit(1);
		}
		err(1, "Can't create transport for local rpctlssd socket");
	}
	if (!SVC_CONTROL(xprt, SVCNL_GET_XIDKEY, &xidkey))
		err(1, "Failed to obtain pthread key for xid from svc_nl");
	if (!svc_reg(xprt, RPCTLSSD, RPCTLSSDVERS, rpctlssd_2, NULL)) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't register service for local rpctlssd socket");
			exit(1);
		}
		err(1, "Can't register service for local rpctlssd socket");
	}

	rpctls_ctx = rpctls_setup_ssl(rpctls_certdir);
	if (rpctls_ctx == NULL) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR, "Can't create SSL context");
			exit(1);
		}
		err(1, "Can't create SSL context");
	}
	rpctls_gothup = false;
	pthread_rwlock_init(&rpctls_rwlock, NULL);
	pthread_mutex_init(&rpctls_mtx, NULL);
	pthread_cond_init(&rpctls_cv, NULL);
	LIST_INIT(&rpctls_ssllist);

	rpctls_svc_run();

	SSL_CTX_free(rpctls_ctx);
	return (0);
}

bool_t
rpctlssd_null_2_svc(__unused void *argp, __unused void *result,
    __unused struct svc_req *rqstp)
{

	rpctls_verbose_out("rpctlssd_null_svc: done\n");
	return (TRUE);
}

/*
 * To parallelize SSL handshakes we will launch a thread per handshake.  Thread
 * creation/destruction shall be order(s) of magnitude cheaper than a crypto
 * handshake, so we are not keeping a pool of workers here.
 *
 * Marrying rpc(3) and pthread(3):
 *
 * Normally the rpcgen(1) generated rpctlssd_V() calls rpctlssd_connect_V_svc(),
 * and the latter processes the RPC all the way to the end and returns a TRUE
 * value and populates the result.  The generated code immediately calls
 * svc_sendreply() transmitting the result back.
 *
 * We will make a private copy of arguments and return FALSE.  Then it is our
 * obligation to call svc_sendreply() once we do the work in the thread.
 */

static void * rpctlssd_connect_thread(void *);
struct rpctlssd_connect_thread_ctx {
	struct rpctlssd_connect_arg arg;
	uint32_t xid;
};

bool_t
rpctlssd_connect_2_svc(struct rpctlssd_connect_arg *argp,
    struct rpctlssd_connect_res *result __unused, struct svc_req *rqstp)
{
	struct rpctlssd_connect_thread_ctx *ctx;
	pthread_t tid;

	assert(rqstp->rq_xprt == xprt);

	ctx = malloc(sizeof(*ctx));
	memcpy(&ctx->arg, argp, sizeof(ctx->arg));
	ctx->xid = *(uint32_t *)pthread_getspecific(xidkey);

	pthread_mutex_lock(&rpctls_mtx);
	while (rpctls_nthreads >= rpctls_maxthreads)
		pthread_cond_wait(&rpctls_cv, &rpctls_mtx);
	rpctls_nthreads++;
	pthread_mutex_unlock(&rpctls_mtx);

	rpctls_verbose_out("rpctlsd_connect_svc: xid %u thread %u\n",
	    ctx->xid, rpctls_nthreads);

	if (pthread_create(&tid, NULL, rpctlssd_connect_thread, ctx) != 0)
		warn("failed to start handshake thread");

	/* Intentionally, so that RPC generated code doesn't try to reply. */
	return (FALSE);
}

static void *
rpctlssd_connect_thread(void *v)
{
	struct rpctlssd_connect_thread_ctx *ctx = v;
	struct rpctlssd_connect_res result;
	uint64_t socookie;
	int ngrps, s;
	SSL *ssl;
	uint32_t flags;
	struct ssl_entry *newslp;
	uint32_t xid, uid;
	uint32_t *gidp;
	X509 *cert;

	socookie = ctx->arg.socookie;
	xid = ctx->xid;
	free(ctx);
	ctx = NULL;
	pthread_detach(pthread_self());

	if (pthread_setspecific(xidkey, &xid) != 0) {
		rpctls_verbose_out("rpctlssd_connect_svc: pthread_setspecific "
		    "failed\n");
		goto out;
	}

	/* Get the socket fd from the kernel. */
	s = rpctls_syscall(socookie);
	if (s < 0) {
		rpctls_verbose_out("rpctlssd_connect_svc: rpctls_syscall "
		    "accept failed\n");
		goto out;
	}

	/* Do the server side of a TLS handshake. */
	gidp = calloc(NGROUPS, sizeof(*gidp));
	ssl = rpctls_server(rpctls_ctx, s, &flags, &uid, &ngrps, gidp, &cert);
	if (ssl == NULL) {
		free(gidp);
		rpctls_verbose_out("rpctlssd_connect_svc: ssl "
		    "accept failed\n");
		/*
		 * For RPC-over-TLS, this upcall is expected
		 * to close off the socket upon handshake failure.
		 */
		close(s);
		goto out;
	} else {
		rpctls_verbose_out("rpctlssd_connect_svc: "
		    "succeeded flags=0x%x\n", flags);
		if ((flags & RPCTLS_FLAGS_CERTUSER) != 0)
			result = (struct rpctlssd_connect_res){
				.flags = flags,
				.uid = uid,
				.gid.gid_len = ngrps,
				.gid.gid_val = gidp,
			};
		else
			result = (struct rpctlssd_connect_res){
				.flags = flags,
				.uid = 0,
				.gid.gid_len = 0,
				.gid.gid_val = gidp,
			};
	}

	/* Maintain list of all current SSL *'s */
	newslp = malloc(sizeof(*newslp));
	newslp->ssl = ssl;
	newslp->s = s;
	newslp->shutoff = false;
	newslp->cookie = socookie;
	newslp->cert = cert;
	pthread_rwlock_wrlock(&rpctls_rwlock);
	LIST_INSERT_HEAD(&rpctls_ssllist, newslp, next);
	pthread_rwlock_unlock(&rpctls_rwlock);

	if (!svc_sendreply(xprt, (xdrproc_t)xdr_rpctlssd_connect_res, &result))
		svcerr_systemerr(xprt);

	free(result.gid.gid_val);
	rpctls_verbose_out("rpctlsd_connect_svc: xid %u: thread finished\n",
	    xid);

out:
	pthread_mutex_lock(&rpctls_mtx);
	if (rpctls_nthreads-- >= rpctls_maxthreads) {
		pthread_mutex_unlock(&rpctls_mtx);
		pthread_cond_signal(&rpctls_cv);
	} else
		pthread_mutex_unlock(&rpctls_mtx);
	return (NULL);
}

bool_t
rpctlssd_handlerecord_2_svc(struct rpctlssd_handlerecord_arg *argp,
    struct rpctlssd_handlerecord_res *result, __unused struct svc_req *rqstp)
{
	struct ssl_entry *slp;
	int ret;
	char junk;

	pthread_rwlock_rdlock(&rpctls_rwlock);
	LIST_FOREACH(slp, &rpctls_ssllist, next)
		if (slp->cookie == argp->socookie)
			break;
	pthread_rwlock_unlock(&rpctls_rwlock);

	if (slp != NULL) {
		rpctls_verbose_out("rpctlssd_handlerecord fd=%d\n",
		    slp->s);
		/*
		 * An SSL_read() of 0 bytes should fail, but it should
		 * handle the non-application data record before doing so.
		 */
		ret = SSL_read(slp->ssl, &junk, 0);
		if (ret <= 0) {
			/* Check to see if this was a close alert. */
			ret = SSL_get_shutdown(slp->ssl);
			if ((ret & (SSL_SENT_SHUTDOWN |
			    SSL_RECEIVED_SHUTDOWN)) == SSL_RECEIVED_SHUTDOWN)
				SSL_shutdown(slp->ssl);
		} else {
			if (rpctls_debug_level == 0)
				syslog(LOG_ERR, "SSL_read returned %d", ret);
			else
				fprintf(stderr, "SSL_read returned %d\n", ret);
		}
		result->reterr = RPCTLSERR_OK;
	} else
		result->reterr = RPCTLSERR_NOSSL;
	return (TRUE);
}

bool_t
rpctlssd_disconnect_2_svc(struct rpctlssd_disconnect_arg *argp,
    struct rpctlssd_disconnect_res *result, __unused struct svc_req *rqstp)
{
	struct ssl_entry *slp;
	int ret;

	pthread_rwlock_wrlock(&rpctls_rwlock);
	LIST_FOREACH(slp, &rpctls_ssllist, next)
		if (slp->cookie == argp->socookie) {
			LIST_REMOVE(slp, next);
			break;
		}
	pthread_rwlock_unlock(&rpctls_rwlock);

	if (slp != NULL) {
		rpctls_verbose_out("rpctlssd_disconnect fd=%d closed\n",
		    slp->s);
		if (!slp->shutoff) {
			ret = SSL_get_shutdown(slp->ssl);
			/*
			 * Do an SSL_shutdown() unless a close alert has
			 * already been sent.
			 */
			if ((ret & SSL_SENT_SHUTDOWN) == 0)
				SSL_shutdown(slp->ssl);
		}
		SSL_free(slp->ssl);
		if (slp->cert != NULL)
			X509_free(slp->cert);
		/*
		 * For RPC-over-TLS, this upcall is expected
		 * to close off the socket.
		 */
		if (!slp->shutoff)
			shutdown(slp->s, SHUT_WR);
		close(slp->s);
		free(slp);
		result->reterr = RPCTLSERR_OK;
	} else
		result->reterr = RPCTLSERR_NOCLOSE;
	return (TRUE);
}

int
rpctlssd_2_freeresult(__unused SVCXPRT *transp, xdrproc_t xdr_result __unused,
    caddr_t result __unused)
{
	return (TRUE);
}

/*
 * cleanup_term() called via SIGTERM (or SIGCHLD if a child dies).
 */
static void
rpctls_cleanup_term(int sig __unused)
{
	struct ssl_entry *slp;

	LIST_FOREACH(slp, &rpctls_ssllist, next)
		shutdown(slp->s, SHUT_RD);
	SSL_CTX_free(rpctls_ctx);
	EVP_cleanup();
	pidfile_remove(rpctls_pfh);

	exit(0);
}

/* Allow the handshake to proceed. */
static int
rpctls_verify_callback(__unused int preverify_ok,
    __unused X509_STORE_CTX *x509_ctx)
{

	return (1);
}

static SSL_CTX *
rpctls_setup_ssl(const char *certdir)
{
	SSL_CTX *ctx;
	char path[PATH_MAX];
	size_t len, rlen;
	int ret;

	ctx = SSL_CTX_new(TLS_server_method());
	if (ctx == NULL) {
		rpctls_verbose_out("rpctls_setup_ssl: SSL_CTX_new failed\n");
		return (NULL);
	}

	if (rpctls_ciphers != NULL) {
		/*
		 * Set available ciphers, since KERN_TLS only supports a
		 * few of them.  Normally, not doing this should be ok,
		 * since the library defaults will work.
		 */
		ret = SSL_CTX_set_ciphersuites(ctx, rpctls_ciphers);
		if (ret == 0) {
			rpctls_verbose_out("rpctls_setup_ssl: "
			    "SSL_CTX_set_ciphersuites failed: %s\n",
			    rpctls_ciphers);
			SSL_CTX_free(ctx);
			return (NULL);
		}
	}

	ret = SSL_CTX_set_min_proto_version(ctx, rpctls_mintls);
	if (ret == 0) {
		rpctls_verbose_out("rpctls_setup_ssl: "
		    "SSL_CTX_set_min_proto_version failed\n");
		SSL_CTX_free(ctx);
		return (NULL);
	}
	ret = SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
	if (ret == 0) {
		rpctls_verbose_out("rpctls_setup_ssl: "
		    "SSL_CTX_set_max_proto_version failed\n");
		SSL_CTX_free(ctx);
		return (NULL);
	}

	/* Get the cert.pem and certkey.pem files from the directory certdir. */
	len = strlcpy(path, certdir, sizeof(path));
	rlen = sizeof(path) - len;
	if (strlcpy(&path[len], "cert.pem", rlen) != 8) {
		SSL_CTX_free(ctx);
		return (NULL);
	}
	ret = SSL_CTX_use_certificate_file(ctx, path, SSL_FILETYPE_PEM);
	if (ret != 1) {
		rpctls_verbose_out("rpctls_setup_ssl: can't use certificate "
		    "file path=%s ret=%d\n", path, ret);
		SSL_CTX_free(ctx);
		return (NULL);
	}
	if (strlcpy(&path[len], "certkey.pem", rlen) != 11) {
		SSL_CTX_free(ctx);
		return (NULL);
	}
	ret = SSL_CTX_use_PrivateKey_file(ctx, path, SSL_FILETYPE_PEM);
	if (ret != 1) {
		rpctls_verbose_out("rpctls_setup_ssl: Can't use private "
		    "key path=%s ret=%d\n", path, ret);
		SSL_CTX_free(ctx);
		return (NULL);
	}

	/* Set Mutual authentication, as required. */
	if (rpctls_do_mutual) {
		if (rpctls_verify_cafile != NULL ||
		    rpctls_verify_capath != NULL) {
			if (rpctls_crlfile != NULL) {
				ret = rpctls_loadcrlfile(ctx);
				if (ret == 0) {
					rpctls_verbose_out("rpctls_setup_ssl:"
					    " Load CRLfile failed\n");
					SSL_CTX_free(ctx);
					return (NULL);
				}
			}
#if OPENSSL_VERSION_NUMBER >= 0x30000000
			ret = 1;
			if (rpctls_verify_cafile != NULL)
				ret = SSL_CTX_load_verify_file(ctx,
				    rpctls_verify_cafile);
			if (ret != 0 && rpctls_verify_capath != NULL)
				ret = SSL_CTX_load_verify_dir(ctx,
				    rpctls_verify_capath);
#else
			ret = SSL_CTX_load_verify_locations(ctx,
			    rpctls_verify_cafile, rpctls_verify_capath);
#endif
			if (ret == 0) {
				rpctls_verbose_out("rpctls_setup_ssl: "
				    "Can't load verify locations\n");
				SSL_CTX_free(ctx);
				return (NULL);
			}
			if (rpctls_verify_cafile != NULL)
				SSL_CTX_set_client_CA_list(ctx,
				    SSL_load_client_CA_file(
			    rpctls_verify_cafile));
		}
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER,
		    rpctls_verify_callback);
	}
#ifdef SSL_OP_ENABLE_KTLS
	SSL_CTX_set_options(ctx, SSL_OP_ENABLE_KTLS);
#endif
#ifdef SSL_MODE_NO_KTLS_TX
	SSL_CTX_clear_mode(ctx, SSL_MODE_NO_KTLS_TX | SSL_MODE_NO_KTLS_RX);
#endif
	return (ctx);
}

static SSL *
rpctls_server(SSL_CTX *ctx, int s, uint32_t *flags, uint32_t *uidp,
    int *ngrps, uint32_t *gidp, X509 **certp)
{
	SSL *ssl;
	X509 *cert;
	struct sockaddr *sad;
	struct sockaddr_storage ad;
	char hostnam[NI_MAXHOST];
	int gethostret, ret;
	char *cp, *cp2;
	long verfret;

	*flags = 0;
	*certp = NULL;
	sad = (struct sockaddr *)&ad;
	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		rpctls_verbose_out("rpctls_server: SSL_new failed\n");
		return (NULL);
	}
	if (SSL_set_fd(ssl, s) != 1) {
		rpctls_verbose_out("rpctls_server: SSL_set_fd failed\n");
		SSL_free(ssl);
		return (NULL);
	}
	ret = SSL_accept(ssl);
	if (ret != 1) {
		rpctls_verbose_out("rpctls_server: SSL_accept "
		    "failed ret=%d\n", ret);
		SSL_free(ssl);
		return (NULL);
	}
	*flags |= RPCTLS_FLAGS_HANDSHAKE;
	if (rpctls_verbose) {
		gethostret = rpctls_gethost(s, sad, hostnam, sizeof(hostnam));
		if (gethostret == 0)
			hostnam[0] = '\0';
		rpctls_verbose_out("rpctls_server: SSL handshake ok for host %s"
		    " <%s %s>\n", hostnam, SSL_get_version(ssl),
		    SSL_get_cipher(ssl));
	}
	if (rpctls_do_mutual) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000
		cert = SSL_get1_peer_certificate(ssl);
#else
		cert = SSL_get_peer_certificate(ssl);
#endif
		if (cert != NULL) {
			if (!rpctls_verbose) {
				gethostret = rpctls_gethost(s, sad, hostnam,
				    sizeof(hostnam));
				if (gethostret == 0)
					hostnam[0] = '\0';
			}
			cp2 = X509_NAME_oneline(
			    X509_get_subject_name(cert), NULL, 0);
			*flags |= RPCTLS_FLAGS_GOTCERT;
			verfret = SSL_get_verify_result(ssl);
			if (verfret != X509_V_OK) {
				cp = X509_NAME_oneline(
				    X509_get_issuer_name(cert), NULL, 0);
				if (rpctls_debug_level == 0)
					syslog(LOG_INFO | LOG_DAEMON,
					    "rpctls_server: client IP %s "
					    "issuerName=%s subjectName=%s"
					    " verify failed %s\n", hostnam,
					    cp, cp2,
					    X509_verify_cert_error_string(
					    verfret));
				else
					fprintf(stderr,
					    "rpctls_server: client IP %s "
					    "issuerName=%s subjectName=%s"
					    " verify failed %s\n", hostnam,
					    cp, cp2,
					    X509_verify_cert_error_string(
					    verfret));
			}
			if (verfret ==
			    X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
			    verfret == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)
				*flags |= RPCTLS_FLAGS_SELFSIGNED;
			else if (verfret == X509_V_OK) {
				if (rpctls_comparehost) {
					ret = 0;
					if (gethostret != 0)
						ret = rpctls_checkhost(sad,
						    cert, rpctls_wildcard);
					if (ret != 1) {
						*flags |=
						    RPCTLS_FLAGS_DISABLED;
						rpctls_verbose_out(
						    "rpctls_server: "
						    "checkhost "
						    "failed\n");
					}
				}
				if (rpctls_cnuser) {
					ret = rpctls_cnname(cert, uidp,
					    ngrps, gidp);
					if (ret != 0)
						*flags |= RPCTLS_FLAGS_CERTUSER;
				}
				*flags |= RPCTLS_FLAGS_VERIFIED;
				*certp = cert;
				cert = NULL;
			}
			if (cert != NULL)
				X509_free(cert);
		} else
			rpctls_verbose_out("rpctls_server: "
			    "No peer certificate\n");
	}

	/* Check to see that ktls is working for the connection. */
	ret = BIO_get_ktls_send(SSL_get_wbio(ssl));
	rpctls_verbose_out("rpctls_server: BIO_get_ktls_send=%d\n", ret);
	if (ret != 0) {
		ret = BIO_get_ktls_recv(SSL_get_rbio(ssl));
		rpctls_verbose_out("rpctls_server: BIO_get_ktls_recv=%d\n",
		    ret);
	}
	if (ret == 0) {
		if (rpctls_debug_level == 0)
			syslog(LOG_ERR, "ktls not working");
		else
			fprintf(stderr, "ktls not working\n");
		/*
		 * The handshake has completed, so all that can be
		 * done is disable the connection.
		 */
		*flags |= RPCTLS_FLAGS_DISABLED;
	}

	return (ssl);
}

/*
 * Acquire the dnsname for this server.
 */
static char *
rpctls_getdnsname(char *hostname)
{
	char *cp, *dnsname;
	struct addrinfo *aip, hints;
	int error;

	dnsname = NULL;
	if (gethostname(hostname, MAXHOSTNAMELEN) == 0) {
		if ((cp = strchr(hostname, '.')) != NULL &&
		    *(cp + 1) != '\0') {
			*cp = '@';
			dnsname = cp;
		} else {
			memset((void *)&hints, 0, sizeof (hints));
			hints.ai_flags = AI_CANONNAME;
			error = getaddrinfo(hostname, NULL, &hints, &aip);
			if (error == 0) {
				if (aip->ai_canonname != NULL &&
				    (cp = strchr(aip->ai_canonname, '.')) !=
				    NULL && *(cp + 1) != '\0') {
					hostname[0] = '@';
					strlcpy(&hostname[1], cp + 1,
					    MAXHOSTNAMELEN + 1);
					dnsname = hostname;
				}
				freeaddrinfo(aip);
			}
		}
	}
	return (dnsname);
}

/*
 * Check for an otherName component of subjectAltName where the OID
 * matches and the "domain" matches that of this server.
 * If found, map "user" to a <uid, gidlist> for it.
 */
static int
rpctls_cnname(X509 *cert, uint32_t *uidp, int *ngrps, uint32_t *gidp)
{
	char *cp, usern[1024 + 1];
	struct passwd *pwd;
	gid_t gids[NGROUPS];
	int i, j;
	GENERAL_NAMES *genlist;
	GENERAL_NAME *genname;
	OTHERNAME *val;
	size_t slen;

	/* First, find the otherName in the subjectAltName. */
	genlist = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (genlist == NULL)
		return (0);
	cp = NULL;
	for (i = 0; i < sk_GENERAL_NAME_num(genlist); i++) {
		genname = sk_GENERAL_NAME_value(genlist, i);
		if (genname->type != GEN_OTHERNAME)
			continue;
		val = genname->d.otherName;

		/* Check to see that it is the correct OID. */
		slen = i2t_ASN1_OBJECT(usern, sizeof(usern), val->type_id);
		if (slen != strlen(rpctls_cnuseroid) || memcmp(usern,
		    rpctls_cnuseroid, slen) != 0)
			continue;

		/* Sanity check the otherName. */
		if (val->value->type != V_ASN1_UTF8STRING ||
		    val->value->value.utf8string->length < 3 ||
		    (size_t)val->value->value.utf8string->length > sizeof(usern)
		    - 1) {
			rpctls_verbose_out("rpctls_cnname: invalid cnuser "
			    "type=%d\n", val->value->type);
			continue;
		}

		/* Look for a "user" in the otherName */
		memcpy(usern, val->value->value.utf8string->data,
		    val->value->value.utf8string->length);
		usern[val->value->value.utf8string->length] = '\0';

		/* Now, look for the @dnsname suffix in the commonName. */
		cp = strcasestr(usern, rpctls_dnsname);
		if (cp == NULL)
			continue;
		if (*(cp + strlen(rpctls_dnsname)) != '\0') {
			cp = NULL;
			continue;
		}
		*cp = '\0';
		break;
	}
	if (cp == NULL)
		return (0);

	/* See if the "user" is in the passwd database. */
	pwd = getpwnam(usern);
	if (pwd == NULL)
		return (0);
	*uidp = pwd->pw_uid;
	*ngrps = NGROUPS;
	if (getgrouplist(pwd->pw_name, pwd->pw_gid, gids, ngrps) < 0)
		return (0);
	rpctls_verbose_out("mapped user=%s ngrps=%d uid=%d\n", pwd->pw_name,
	    *ngrps, pwd->pw_uid);
	for (j = 0; j < *ngrps; j++)
		gidp[j] = gids[j];
	return (1);
}

static void
rpctls_huphandler(int sig __unused)
{

	rpctls_gothup = true;
}
