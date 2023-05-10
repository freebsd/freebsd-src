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
 * the client side of kernel RPC-over-TLS by Rick Macklem.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <err.h>
#include <getopt.h>
#include <libutil.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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

#include "rpctlscd.h"
#include "rpc.tlscommon.h"

#ifndef _PATH_RPCTLSCDSOCK
#define _PATH_RPCTLSCDSOCK	"/var/run/rpc.tlsclntd.sock"
#endif
#ifndef	_PATH_CERTANDKEY
#define	_PATH_CERTANDKEY	"/etc/rpc.tlsclntd/"
#endif
#ifndef	_PATH_RPCTLSCDPID
#define	_PATH_RPCTLSCDPID	"/var/run/rpc.tlsclntd.pid"
#endif

/* Global variables also used by rpc.tlscommon.c. */
int			rpctls_debug_level;
bool			rpctls_verbose;
SSL_CTX			*rpctls_ctx = NULL;
const char		*rpctls_verify_cafile = NULL;
const char		*rpctls_verify_capath = NULL;
char			*rpctls_crlfile = NULL;
bool			rpctls_cert = false;
bool			rpctls_gothup = false;
struct ssl_list		rpctls_ssllist;

static struct pidfh	*rpctls_pfh = NULL;
static const char	*rpctls_certdir = _PATH_CERTANDKEY;
static const char	*rpctls_ciphers = NULL;
static uint64_t		rpctls_ssl_refno = 0;
static uint64_t		rpctls_ssl_sec = 0;
static uint64_t		rpctls_ssl_usec = 0;
static int		rpctls_tlsvers = TLS1_3_VERSION;

static void		rpctlscd_terminate(int);
static SSL_CTX		*rpctls_setupcl_ssl(void);
static SSL		*rpctls_connect(SSL_CTX *ctx, int s, char *certname,
			    u_int certlen, X509 **certp);
static void		rpctls_huphandler(int sig __unused);

extern void rpctlscd_1(struct svc_req *rqstp, SVCXPRT *transp);

static struct option longopts[] = {
	{ "usetls1_2",		no_argument,		NULL,	'2' },
	{ "certdir",		required_argument,	NULL,	'D' },
	{ "ciphers",		required_argument,	NULL,	'C' },
	{ "debuglevel",		no_argument,		NULL,	'd' },
	{ "verifylocs",		required_argument,	NULL,	'l' },
	{ "mutualverf",		no_argument,		NULL,	'm' },
	{ "verifydir",		required_argument,	NULL,	'p' },
	{ "crl",		required_argument,	NULL,	'r' },
	{ "verbose",		no_argument,		NULL,	'v' },
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
	int ch, fd, oldmask;
	SVCXPRT *xprt;
	bool tls_enable;
	struct timeval tm;
	struct timezone tz;
	pid_t otherpid;
	size_t tls_enable_len;

	/* Check that another rpctlscd isn't already running. */
	rpctls_pfh = pidfile_open(_PATH_RPCTLSCDPID, 0600, &otherpid);
	if (rpctls_pfh == NULL) {
		if (errno == EEXIST)
			errx(1, "rpctlscd already running, pid: %d.", otherpid);
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

	rpctls_verbose = false;
	while ((ch = getopt_long(argc, argv, "2C:D:dl:mp:r:v", longopts,
	    NULL)) != -1) {
		switch (ch) {
		case '2':
			rpctls_tlsvers = TLS1_2_VERSION;
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
		case 'l':
			rpctls_verify_cafile = optarg;
			break;
		case 'm':
			rpctls_cert = true;
			break;
		case 'p':
			rpctls_verify_capath = optarg;
			break;
		case 'r':
			rpctls_crlfile = optarg;
			break;
		case 'v':
			rpctls_verbose = true;
			break;
		default:
			fprintf(stderr, "usage: %s "
			    "[-2/--usetls1_2] "
			    "[-C/--ciphers available_ciphers] "
			    "[-D/--certdir certdir] [-d/--debuglevel] "
			    "[-l/--verifylocs CAfile] [-m/--mutualverf] "
			    "[-p/--verifydir CApath] [-r/--crl CRLfile] "
			    "[-v/--verbose]\n", argv[0]);
			exit(1);
			break;
		}
	}
	if (rpctls_crlfile != NULL && rpctls_verify_cafile == NULL &&
	    rpctls_verify_capath == NULL)
		errx(1, "-r requires the -l <CAfile> and/or "
		    "-p <CApath> options");

	if (modfind("krpc") < 0) {
		/* Not present in kernel, try loading it */
		if (kldload("krpc") < 0 || modfind("krpc") < 0)
			errx(1, "Kernel RPC is not available");
	}

	/*
	 * Set up the SSL_CTX *.
	 * Do it now, before daemonizing, in case the private key
	 * is encrypted and requires a passphrase to be entered.
	 */
	rpctls_ctx = rpctls_setupcl_ssl();
	if (rpctls_ctx == NULL) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR, "Can't set up TLS context");
			exit(1);
		}
		err(1, "Can't set up TLS context");
	}
	LIST_INIT(&rpctls_ssllist);

	if (!rpctls_debug_level) {
		if (daemon(0, 0) != 0)
			err(1, "Can't daemonize");
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
	}
	signal(SIGTERM, rpctlscd_terminate);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, rpctls_huphandler);

	pidfile_write(rpctls_pfh);

	memset(&sun, 0, sizeof sun);
	sun.sun_family = AF_LOCAL;
	unlink(_PATH_RPCTLSCDSOCK);
	strcpy(sun.sun_path, _PATH_RPCTLSCDSOCK);
	sun.sun_len = SUN_LEN(&sun);
	fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR, "Can't create local rpctlscd socket");
			exit(1);
		}
		err(1, "Can't create local rpctlscd socket");
	}
	oldmask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if (bind(fd, (struct sockaddr *)&sun, sun.sun_len) < 0) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR, "Can't bind local rpctlscd socket");
			exit(1);
		}
		err(1, "Can't bind local rpctlscd socket");
	}
	umask(oldmask);
	if (listen(fd, SOMAXCONN) < 0) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't listen on local rpctlscd socket");
			exit(1);
		}
		err(1, "Can't listen on local rpctlscd socket");
	}
	xprt = svc_vc_create(fd, RPC_MAXDATASIZE, RPC_MAXDATASIZE);
	if (!xprt) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't create transport for local rpctlscd socket");
			exit(1);
		}
		err(1, "Can't create transport for local rpctlscd socket");
	}
	if (!svc_reg(xprt, RPCTLSCD, RPCTLSCDVERS, rpctlscd_1, NULL)) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't register service for local rpctlscd socket");
			exit(1);
		}
		err(1, "Can't register service for local rpctlscd socket");
	}

	if (rpctls_syscall(RPCTLS_SYSC_CLSETPATH, _PATH_RPCTLSCDSOCK) < 0) {
		if (rpctls_debug_level == 0) {
			syslog(LOG_ERR,
			    "Can't set upcall socket path errno=%d", errno);
			exit(1);
		}
		err(1, "Can't set upcall socket path");
	}

	rpctls_svc_run();

	rpctls_syscall(RPCTLS_SYSC_CLSHUTDOWN, "");

	SSL_CTX_free(rpctls_ctx);
	EVP_cleanup();
	return (0);
}

bool_t
rpctlscd_null_1_svc(__unused void *argp, __unused void *result,
    __unused struct svc_req *rqstp)
{

	rpctls_verbose_out("rpctlscd_null: done\n");
	return (TRUE);
}

bool_t
rpctlscd_connect_1_svc(struct rpctlscd_connect_arg *argp,
    struct rpctlscd_connect_res *result, __unused struct svc_req *rqstp)
{
	int s;
	SSL *ssl;
	struct ssl_entry *newslp;
	X509 *cert;

	rpctls_verbose_out("rpctlsd_connect: started\n");
	/* Get the socket fd from the kernel. */
	s = rpctls_syscall(RPCTLS_SYSC_CLSOCKET, "");
	if (s < 0) {
		result->reterr = RPCTLSERR_NOSOCKET;
		return (TRUE);
	}

	/* Do a TLS connect handshake. */
	ssl = rpctls_connect(rpctls_ctx, s, argp->certname.certname_val,
	    argp->certname.certname_len, &cert);
	if (ssl == NULL) {
		rpctls_verbose_out("rpctlsd_connect: can't do TLS "
		    "handshake\n");
		result->reterr = RPCTLSERR_NOSSL;
	} else {
		result->reterr = RPCTLSERR_OK;
		result->sec = rpctls_ssl_sec;
		result->usec = rpctls_ssl_usec;
		result->ssl = ++rpctls_ssl_refno;
		/* Hard to believe this will ever wrap around.. */
		if (rpctls_ssl_refno == 0)
			result->ssl = ++rpctls_ssl_refno;
	}

	if (ssl == NULL) {
		/*
		 * For RPC-over-TLS, this upcall is expected
		 * to close off the socket.
		 */
		close(s);
		return (TRUE);
	}

	/* Maintain list of all current SSL *'s */
	newslp = malloc(sizeof(*newslp));
	newslp->refno = rpctls_ssl_refno;
	newslp->s = s;
	newslp->shutoff = false;
	newslp->ssl = ssl;
	newslp->cert = cert;
	LIST_INSERT_HEAD(&rpctls_ssllist, newslp, next);
	return (TRUE);
}

bool_t
rpctlscd_handlerecord_1_svc(struct rpctlscd_handlerecord_arg *argp,
    struct rpctlscd_handlerecord_res *result, __unused struct svc_req *rqstp)
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
		rpctls_verbose_out("rpctlscd_handlerecord fd=%d\n",
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
rpctlscd_disconnect_1_svc(struct rpctlscd_disconnect_arg *argp,
    struct rpctlscd_disconnect_res *result, __unused struct svc_req *rqstp)
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
		rpctls_verbose_out("rpctlscd_disconnect: fd=%d closed\n",
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
rpctlscd_1_freeresult(__unused SVCXPRT *transp, __unused xdrproc_t xdr_result,
    __unused caddr_t result)
{

	return (TRUE);
}

static void
rpctlscd_terminate(int sig __unused)
{

	rpctls_syscall(RPCTLS_SYSC_CLSHUTDOWN, "");
	pidfile_remove(rpctls_pfh);
	exit(0);
}

static SSL_CTX *
rpctls_setupcl_ssl(void)
{
	SSL_CTX *ctx;
	char path[PATH_MAX];
	size_t len, rlen;
	int ret;

	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();

	ctx = SSL_CTX_new(TLS_client_method());
	if (ctx == NULL) {
		rpctls_verbose_out("rpctls_setupcl_ssl: SSL_CTX_new "
		    "failed\n");
		return (NULL);
	}
	SSL_CTX_set_ecdh_auto(ctx, 1);

	if (rpctls_ciphers != NULL) {
		/*
		 * Set available ciphers, since KERN_TLS only supports a
		 * few of them.
		 */
		ret = SSL_CTX_set_ciphersuites(ctx, rpctls_ciphers);
		if (ret == 0) {
			rpctls_verbose_out("rpctls_setupcl_ssl: "
			    "SSL_CTX_set_ciphersuites failed: %s\n",
			    rpctls_ciphers);
			SSL_CTX_free(ctx);
			return (NULL);
		}
	}

	/*
	 * If rpctls_cert is true, a certificate and key exists in
	 * rpctls_certdir, so that it can do mutual authentication.
	 */
	if (rpctls_cert) {
		/* Get the cert.pem and certkey.pem files. */
		len = strlcpy(path, rpctls_certdir, sizeof(path));
		rlen = sizeof(path) - len;
		if (strlcpy(&path[len], "cert.pem", rlen) != 8) {
			SSL_CTX_free(ctx);
			return (NULL);
		}
		ret = SSL_CTX_use_certificate_file(ctx, path,
		    SSL_FILETYPE_PEM);
		if (ret != 1) {
			rpctls_verbose_out("rpctls_setupcl_ssl: can't use "
			    "certificate file path=%s ret=%d\n", path, ret);
			SSL_CTX_free(ctx);
			return (NULL);
		}
		if (strlcpy(&path[len], "certkey.pem", rlen) != 11) {
			SSL_CTX_free(ctx);
			return (NULL);
		}
		ret = SSL_CTX_use_PrivateKey_file(ctx, path,
		    SSL_FILETYPE_PEM);
		if (ret != 1) {
			rpctls_verbose_out("rpctls_setupcl_ssl: Can't use "
			    "private key path=%s ret=%d\n", path, ret);
			SSL_CTX_free(ctx);
			return (NULL);
		}
	}

	if (rpctls_verify_cafile != NULL || rpctls_verify_capath != NULL) {
		if (rpctls_crlfile != NULL) {
			ret = rpctls_loadcrlfile(ctx);
			if (ret == 0) {
				rpctls_verbose_out("rpctls_setupcl_ssl: "
				    "Load CRLfile failed\n");
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
			rpctls_verbose_out("rpctls_setupcl_ssl: "
			    "Can't load verify locations\n");
			SSL_CTX_free(ctx);
			return (NULL);
		}
		/*
		 * The man page says that the
		 * SSL_CTX_set0_CA_list() call is not normally
		 * needed, but I believe it is harmless.
		 */
		if (rpctls_verify_cafile != NULL)
			SSL_CTX_set0_CA_list(ctx,
			    SSL_load_client_CA_file(rpctls_verify_cafile));
	}

	/*
	 * The RFC specifies that RPC-over-TLS must use TLS1.3.
	 * However, early FreeBSD versions (13.0, 13.1) did not
	 * support RX for KTLS1.3, so TLS1.2 needs to be used for
	 * these servers.
	 */
	ret = SSL_CTX_set_min_proto_version(ctx, rpctls_tlsvers);
	if (ret == 0) {
		rpctls_verbose_out("rpctls_setupcl_ssl: "
		    "SSL_CTX_set_min_proto_version failed\n");
		SSL_CTX_free(ctx);
		return (NULL);
	}
	ret = SSL_CTX_set_max_proto_version(ctx, rpctls_tlsvers);
	if (ret == 0) {
		rpctls_verbose_out("rpctls_setupcl_ssl: "
		    "SSL_CTX_set_max_proto_version failed\n");
		SSL_CTX_free(ctx);
		return (NULL);
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
rpctls_connect(SSL_CTX *ctx, int s, char *certname, u_int certlen, X509 **certp)
{
	SSL *ssl;
	X509 *cert;
	struct sockaddr_storage ad;
	struct sockaddr *sad;
	char hostnam[NI_MAXHOST], path[PATH_MAX];
	int gethostret, ret;
	char *cp, *cp2;
	size_t len, rlen;
	long verfret;

	*certp = NULL;
	sad = (struct sockaddr *)&ad;
	ssl = SSL_new(ctx);
	if (ssl == NULL) {
		rpctls_verbose_out("rpctls_connect: "
		    "SSL_new failed\n");
		return (NULL);
	}
	if (SSL_set_fd(ssl, s) != 1) {
		rpctls_verbose_out("rpctls_connect: "
		    "SSL_set_fd failed\n");
		SSL_free(ssl);
		return (NULL);
	}

	/*
	 * If rpctls_cert is true and certname is set, a alternate certificate
	 * and key exists in files named <certname>.pem and <certname>key.pem
	 * in rpctls_certdir that is to be used for mutual authentication.
	 */
	if (rpctls_cert && certlen > 0) {
		len = strlcpy(path, rpctls_certdir, sizeof(path));
		rlen = sizeof(path) - len;
		if (rlen <= certlen) {
			SSL_free(ssl);
			return (NULL);
		}
		memcpy(&path[len], certname, certlen);
		rlen -= certlen;
		len += certlen;
		path[len] = '\0';
		if (strlcpy(&path[len], ".pem", rlen) != 4) {
			SSL_free(ssl);
			return (NULL);
		}
		ret = SSL_use_certificate_file(ssl, path, SSL_FILETYPE_PEM);
		if (ret != 1) {
			rpctls_verbose_out("rpctls_connect: can't use "
			    "certificate file path=%s ret=%d\n", path, ret);
			SSL_free(ssl);
			return (NULL);
		}
		if (strlcpy(&path[len], "key.pem", rlen) != 7) {
			SSL_free(ssl);
			return (NULL);
		}
		ret = SSL_use_PrivateKey_file(ssl, path, SSL_FILETYPE_PEM);
		if (ret != 1) {
			rpctls_verbose_out("rpctls_connect: Can't use "
			    "private key path=%s ret=%d\n", path, ret);
			SSL_free(ssl);
			return (NULL);
		}
	}

	ret = SSL_connect(ssl);
	if (ret != 1) {
		rpctls_verbose_out("rpctls_connect: "
		    "SSL_connect failed %d: %s\n",
		    ret, ERR_error_string(ERR_get_error(), NULL));
		SSL_free(ssl);
		return (NULL);
	}

	cert = SSL_get_peer_certificate(ssl);
	if (cert == NULL) {
		rpctls_verbose_out("rpctls_connect: get peer"
		    " certificate failed\n");
		SSL_free(ssl);
		return (NULL);
	}
	gethostret = rpctls_gethost(s, sad, hostnam, sizeof(hostnam));
	if (gethostret == 0)
		hostnam[0] = '\0';
	verfret = SSL_get_verify_result(ssl);
	if (verfret == X509_V_OK && (rpctls_verify_cafile != NULL ||
	    rpctls_verify_capath != NULL) && (gethostret == 0 ||
	    rpctls_checkhost(sad, cert, X509_CHECK_FLAG_NO_WILDCARDS) != 1))
		verfret = X509_V_ERR_HOSTNAME_MISMATCH;
	if (verfret != X509_V_OK && (rpctls_verify_cafile != NULL ||
	    rpctls_verify_capath != NULL)) {
		if (verfret != X509_V_OK) {
			cp = X509_NAME_oneline(X509_get_issuer_name(cert),
			    NULL, 0);
			cp2 = X509_NAME_oneline(X509_get_subject_name(cert),
			    NULL, 0);
			if (rpctls_debug_level == 0)
				syslog(LOG_INFO | LOG_DAEMON,
				    "rpctls_connect: client IP %s "
				    "issuerName=%s subjectName=%s verify "
				    "failed %s\n", hostnam, cp, cp2,
				    X509_verify_cert_error_string(verfret));
			else
				fprintf(stderr,
				    "rpctls_connect: client IP %s "
				    "issuerName=%s subjectName=%s verify "
				    "failed %s\n", hostnam, cp, cp2,
				    X509_verify_cert_error_string(verfret));
		}
		X509_free(cert);
		SSL_free(ssl);
		return (NULL);
	}

	/* Check to see if ktls is enabled on the connection. */
	ret = BIO_get_ktls_send(SSL_get_wbio(ssl));
	rpctls_verbose_out("rpctls_connect: BIO_get_ktls_send=%d\n", ret);
	if (ret != 0) {
		ret = BIO_get_ktls_recv(SSL_get_rbio(ssl));
		rpctls_verbose_out("rpctls_connect: BIO_get_ktls_recv=%d\n",
		    ret);
	}
	if (ret == 0) {
		if (rpctls_debug_level == 0)
			syslog(LOG_ERR, "ktls not working\n");
		else
			fprintf(stderr, "ktls not working\n");
		X509_free(cert);
		SSL_free(ssl);
		return (NULL);
	}
	if (ret == X509_V_OK && (rpctls_verify_cafile != NULL ||
	    rpctls_verify_capath != NULL) && rpctls_crlfile != NULL)
		*certp = cert;
	else
		X509_free(cert);

	return (ssl);
}

static void
rpctls_huphandler(int sig __unused)
{

	rpctls_gothup = true;
}
