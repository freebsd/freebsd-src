/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>
#include <getopt.h>
#include <libutil.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
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

#ifndef _PATH_RPCTLSSDSOCK
#define _PATH_RPCTLSSDSOCK	"/var/run/rpc.tlsservd.sock"
#endif
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
struct ssl_list		rpctls_ssllist;

static struct pidfh	*rpctls_pfh = NULL;
static bool		rpctls_do_mutual = false;
static const char	*rpctls_certdir = _PATH_CERTANDKEY;
static bool		rpctls_comparehost = false;
static unsigned int	rpctls_wildcard = X509_CHECK_FLAG_NO_WILDCARDS;
static uint64_t		rpctls_ssl_refno = 0;
static uint64_t		rpctls_ssl_sec = 0;
static uint64_t		rpctls_ssl_usec = 0;
static bool		rpctls_cnuser = false;
static char		*rpctls_dnsname;
static const char	*rpctls_cnuseroid = "1.3.6.1.4.1.2238.1.1.1";
static const char	*rpctls_ciphers = NULL;
static int		rpctls_mintls = TLS1_3_VERSION;
static int		rpctls_procs = 1;
static char		*rpctls_sockname[RPCTLS_SRV_MAXNPROCS];
static pid_t		rpctls_workers[RPCTLS_SRV_MAXNPROCS - 1];
static bool		rpctls_im_a_worker = false;

static void		rpctls_cleanup_term(int sig);
static SSL_CTX		*rpctls_setup_ssl(const char *certdir);
static SSL		*rpctls_server(SSL_CTX *ctx, int s,
			    uint32_t *flags, uint32_t *uidp,
			    int *ngrps, uint32_t *gidp, X509 **certp);
static int		rpctls_cnname(X509 *cert, uint32_t *uidp,
			    int *ngrps, uint32_t *gidp);
static char		*rpctls_getdnsname(char *dnsname);
static void		rpctls_huphandler(int sig __unused);

extern void		rpctlssd_1(struct svc_req *rqstp, SVCXPRT *transp);

static struct option longopts[] = {
	{ "allowtls1_2",	no_argument,		NULL,	'2' },
	{ "ciphers",		required_argument,	NULL,	'C' },
	{ "certdir",		required_argument,	NULL,	'D' },
	{ "debuglevel",		no_argument,		NULL,	'd' },
	{ "checkhost",		no_argument,		NULL,	'h' },
	{ "verifylocs",		required_argument,	NULL,	'l' },
	{ "mutualverf",		no_argument,		NULL,	'm' },
	{ "numdaemons",		required_argument,	NULL,	'N' },
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
	/*
	 * We provide an RPC service on a local-domain socket. The
	 * kernel rpctls code will upcall to this daemon to do the initial
	 * TLS handshake.
	 */
	struct sockaddr_un sun;
	int ch, fd, i, mypos, oldmask;
	SVCXPRT *xprt;
	struct timeval tm;
	struct timezone tz;
	char hostname[MAXHOSTNAMELEN + 2];
	pid_t otherpid;
	bool tls_enable;
	size_t tls_enable_len;
	sigset_t signew;

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

	/* Get the time when this daemon is started. */
	gettimeofday(&tm, &tz);
	rpctls_ssl_sec = tm.tv_sec;
	rpctls_ssl_usec = tm.tv_usec;

	/* Set the dns name for the server. */
	rpctls_dnsname = rpctls_getdnsname(hostname);
	if (rpctls_dnsname == NULL) {
		strcpy(hostname, "@default.domain");
		rpctls_dnsname = hostname;
	}

	/* Initialize socket names. */
	for (i = 0; i < RPCTLS_SRV_MAXNPROCS; i++) {
		asprintf(&rpctls_sockname[i], "%s.%d", _PATH_RPCTLSSDSOCK, i);
		if (rpctls_sockname[i] == NULL)
			errx(1, "Cannot malloc socknames");
	}

	rpctls_verbose = false;
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
			rpctls_procs = atoi(optarg);
			if (rpctls_procs < 1 ||
			    rpctls_procs > RPCTLS_SRV_MAXNPROCS)
				errx(1, "numdaemons/-N must be between 1 and "
				    "%d", RPCTLS_SRV_MAXNPROCS);
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
			    "[-N/--numdaemons num] "
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

	for (i = 0; i < rpctls_procs - 1; i++)
		rpctls_workers[i] = -1;
	mypos = 0;

	if (rpctls_debug_level == 0) {
		/*
		 * Temporarily block SIGTERM and SIGCHLD, so workers[] can't
		 * end up bogus.
		 */
		sigemptyset(&signew);
		sigaddset(&signew, SIGTERM);
		sigaddset(&signew, SIGCHLD);
		sigprocmask(SIG_BLOCK, &signew, NULL);

		if (daemon(0, 0) != 0)
			err(1, "Can't daemonize");
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	}
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, rpctls_huphandler);
	signal(SIGTERM, rpctls_cleanup_term);
	signal(SIGCHLD, rpctls_cleanup_term);

	pidfile_write(rpctls_pfh);

	rpctls_syscall(RPCTLS_SYSC_SRVSTARTUP, "");

	if (rpctls_debug_level == 0) {
		/* Fork off the worker daemons. */
		for (i = 0; i < rpctls_procs - 1; i++) {
			rpctls_workers[i] = fork();
			if (rpctls_workers[i] == 0) {
				rpctls_im_a_worker = true;
				mypos = i + 1;
				setproctitle("server");
				break;
			} else if (rpctls_workers[i] < 0) {
				syslog(LOG_ERR, "fork: %m");
			}
		}

		if (!rpctls_im_a_worker && rpctls_procs > 1)
			setproctitle("master");
	}
	sigemptyset(&signew);
	sigaddset(&signew, SIGTERM);
	sigaddset(&signew, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &signew, NULL);

	memset(&sun, 0, sizeof sun);
	sun.sun_family = AF_LOCAL;
	unlink(rpctls_sockname[mypos]);
	strcpy(sun.sun_path, rpctls_sockname[mypos]);
	sun.sun_len = SUN_LEN(&sun);
	fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR, "Can't create local rpctlssd socket");
			exit(1);
		}
		err(1, "Can't create local rpctlssd socket");
	}
	oldmask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if (bind(fd, (struct sockaddr *)&sun, sun.sun_len) < 0) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR, "Can't bind local rpctlssd socket");
			exit(1);
		}
		err(1, "Can't bind local rpctlssd socket");
	}
	umask(oldmask);
	if (listen(fd, SOMAXCONN) < 0) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't listen on local rpctlssd socket");
			exit(1);
		}
		err(1, "Can't listen on local rpctlssd socket");
	}
	xprt = svc_vc_create(fd, RPC_MAXDATASIZE, RPC_MAXDATASIZE);
	if (!xprt) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't create transport for local rpctlssd socket");
			exit(1);
		}
		err(1, "Can't create transport for local rpctlssd socket");
	}
	if (!svc_reg(xprt, RPCTLSSD, RPCTLSSDVERS, rpctlssd_1, NULL)) {
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
	LIST_INIT(&rpctls_ssllist);

	if (rpctls_syscall(RPCTLS_SYSC_SRVSETPATH, rpctls_sockname[mypos]) < 0){
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't set upcall socket path=%s errno=%d",
			    rpctls_sockname[mypos], errno);
			exit(1);
		}
		err(1, "Can't set upcall socket path=%s",
		    rpctls_sockname[mypos]);
	}

	rpctls_svc_run();

	SSL_CTX_free(rpctls_ctx);
	return (0);
}

bool_t
rpctlssd_null_1_svc(__unused void *argp, __unused void *result,
    __unused struct svc_req *rqstp)
{

	rpctls_verbose_out("rpctlssd_null_svc: done\n");
	return (TRUE);
}

bool_t
rpctlssd_connect_1_svc(__unused void *argp,
    struct rpctlssd_connect_res *result, __unused struct svc_req *rqstp)
{
	int ngrps, s;
	SSL *ssl;
	uint32_t flags;
	struct ssl_entry *newslp;
	uint32_t uid;
	uint32_t *gidp;
	X509 *cert;

	rpctls_verbose_out("rpctlsd_connect_svc: started\n");
	memset(result, 0, sizeof(*result));
	/* Get the socket fd from the kernel. */
	s = rpctls_syscall(RPCTLS_SYSC_SRVSOCKET, "");
	if (s < 0)
		return (FALSE);

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
		return (FALSE);
	} else {
		rpctls_verbose_out("rpctlssd_connect_svc: "
		    "succeeded flags=0x%x\n", flags);
		result->flags = flags;
		result->sec = rpctls_ssl_sec;
		result->usec = rpctls_ssl_usec;
		result->ssl = ++rpctls_ssl_refno;
		/* Hard to believe this could ever wrap around.. */
		if (rpctls_ssl_refno == 0)
			result->ssl = ++rpctls_ssl_refno;
		if ((flags & RPCTLS_FLAGS_CERTUSER) != 0) {
			result->uid = uid;
			result->gid.gid_len = ngrps;
			result->gid.gid_val = gidp;
		} else {
			result->uid = 0;
			result->gid.gid_len = 0;
			result->gid.gid_val = gidp;
		}
	}

	/* Maintain list of all current SSL *'s */
	newslp = malloc(sizeof(*newslp));
	newslp->ssl = ssl;
	newslp->s = s;
	newslp->shutoff = false;
	newslp->refno = rpctls_ssl_refno;
	newslp->cert = cert;
	LIST_INSERT_HEAD(&rpctls_ssllist, newslp, next);
	return (TRUE);
}

bool_t
rpctlssd_handlerecord_1_svc(struct rpctlssd_handlerecord_arg *argp,
    struct rpctlssd_handlerecord_res *result, __unused struct svc_req *rqstp)
{
	struct ssl_entry *slp;
	int ret;
	char junk;

	slp = NULL;
	if (argp->sec == rpctls_ssl_sec && argp->usec ==
	    rpctls_ssl_usec) {
		LIST_FOREACH(slp, &rpctls_ssllist, next) {
			if (slp->refno == argp->ssl)
				break;
		}
	}

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
rpctlssd_disconnect_1_svc(struct rpctlssd_disconnect_arg *argp,
    struct rpctlssd_disconnect_res *result, __unused struct svc_req *rqstp)
{
	struct ssl_entry *slp;
	int ret;

	slp = NULL;
	if (argp->sec == rpctls_ssl_sec && argp->usec ==
	    rpctls_ssl_usec) {
		LIST_FOREACH(slp, &rpctls_ssllist, next) {
			if (slp->refno == argp->ssl)
				break;
		}
	}

	if (slp != NULL) {
		rpctls_verbose_out("rpctlssd_disconnect fd=%d closed\n",
		    slp->s);
		LIST_REMOVE(slp, next);
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
rpctlssd_1_freeresult(__unused SVCXPRT *transp, xdrproc_t xdr_result,
    caddr_t result)
{
	rpctlssd_connect_res *res;

	if (xdr_result == (xdrproc_t)xdr_rpctlssd_connect_res) {
		res = (rpctlssd_connect_res *)(void *)result;
		free(res->gid.gid_val);
	}
	return (TRUE);
}

/*
 * cleanup_term() called via SIGTERM (or SIGCHLD if a child dies).
 */
static void
rpctls_cleanup_term(int sig)
{
	struct ssl_entry *slp;
	int i, cnt;

	if (rpctls_im_a_worker && sig == SIGCHLD)
		return;
	LIST_FOREACH(slp, &rpctls_ssllist, next)
		shutdown(slp->s, SHUT_RD);
	SSL_CTX_free(rpctls_ctx);
	EVP_cleanup();

	if (rpctls_im_a_worker)
		exit(0);

	/* I'm the server, so terminate the workers. */
	cnt = 0;
	for (i = 0; i < rpctls_procs - 1; i++) {
		if (rpctls_workers[i] != -1) {
			cnt++;
			kill(rpctls_workers[i], SIGTERM);
		}
	}

	/*
	 * Wait for them to die.
	 */
	for (i = 0; i < cnt; i++)
		wait3(NULL, 0, NULL);

	rpctls_syscall(RPCTLS_SYSC_SRVSHUTDOWN, "");
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
