/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Rick Macklem
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/time.h>

#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include <rpc/rpc.h>

#include <openssl/opensslconf.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "rpc.tlscommon.h"

/*
 * How long to delay a reload of the CRL when there are RPC request(s)
 * to process, in usec.  Must be less than 1second.
 */
#define	RELOADDELAY	250000

void
rpctls_svc_run(void)
{
	int ret;
	struct timeval tv;
	fd_set readfds;
	uint64_t curtime, nexttime;
	struct timespec tp;
	sigset_t sighup_mask;

	/* Expand svc_run() here so that we can call rpctls_loadcrlfile(). */
	curtime = nexttime = 0;
	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	for (;;) {
		clock_gettime(CLOCK_MONOTONIC, &tp);
		curtime = tp.tv_sec;
		curtime = curtime * 1000000 + tp.tv_nsec / 1000;
		sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
		if (rpctls_gothup && curtime >= nexttime) {
			rpctls_gothup = false;
			sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
			ret = rpctls_loadcrlfile(rpctls_ctx);
			if (ret != 0)
				rpctls_checkcrl();
			else
				rpctls_verbose_out("rpc.tlsservd: Can't "
				    "reload CRLfile\n");
			clock_gettime(CLOCK_MONOTONIC, &tp);
			nexttime = tp.tv_sec;
			nexttime = nexttime * 1000000 + tp.tv_nsec / 1000 +
			    RELOADDELAY;
		} else
			sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);

		/*
		 * If a reload is pending, poll for received request(s),
		 * otherwise set a RELOADDELAY timeout, since a SIGHUP
		 * could be processed between the got_sighup test and
		 * the select() system call.
		 */
		tv.tv_sec = 0;
		if (rpctls_gothup)
			tv.tv_usec = 0;
		else
			tv.tv_usec = RELOADDELAY;
		readfds = svc_fdset;
		switch (select(svc_maxfd + 1, &readfds, NULL, NULL, &tv)) {
		case -1:
			if (errno == EINTR) {
				/* Allow a reload now. */
				nexttime = 0;
				continue;
			}
			syslog(LOG_ERR, "rpc.tls daemon died: select: %m");
			exit(1);
		case 0:
			/* Allow a reload now. */
			nexttime = 0;
			continue;
		default:
			svc_getreqset(&readfds);
		}
	}
}

/*
 * (re)load the CRLfile into the certificate verification store.
 */
int
rpctls_loadcrlfile(SSL_CTX *ctx)
{
	X509_STORE *certstore;
	X509_LOOKUP *certlookup;
	int ret;

	if ((rpctls_verify_cafile != NULL ||
	    rpctls_verify_capath != NULL) &&
	    rpctls_crlfile != NULL) {
		certstore = SSL_CTX_get_cert_store(ctx);
		certlookup = X509_STORE_add_lookup(
		    certstore, X509_LOOKUP_file());
		ret = 0;
		if (certlookup != NULL)
			ret = X509_load_crl_file(certlookup,
			    rpctls_crlfile, X509_FILETYPE_PEM);
		if (ret != 0)
			ret = X509_STORE_set_flags(certstore,
			    X509_V_FLAG_CRL_CHECK |
			    X509_V_FLAG_CRL_CHECK_ALL);
		if (ret == 0) {
			rpctls_verbose_out(
			    "rpctls_loadcrlfile: Can't"
			    " load CRLfile=%s\n",
			    rpctls_crlfile);
			return (ret);
		}
	}
	return (1);
}

/*
 * Read the CRL file and check for any extant connections
 * that might now be revoked.
 */
void
rpctls_checkcrl(void)
{
	struct ssl_entry *slp;
	BIO *infile;
	X509_CRL *crl;
	X509_REVOKED *revoked;
	char *cp, *cp2, nullstr[1];
	int ret;

	if (rpctls_crlfile == NULL || (rpctls_verify_cafile == NULL &&
	    rpctls_verify_capath == NULL))
		return;
	infile = BIO_new(BIO_s_file());
	if (infile == NULL) {
		rpctls_verbose_out("rpctls_checkcrl: Cannot BIO_new\n");
		return;
	}
	ret = BIO_read_filename(infile, rpctls_crlfile);
	if (ret != 1) {
		rpctls_verbose_out("rpctls_checkcrl: Cannot read CRL file\n");
		BIO_free(infile);
		return;
	}

	nullstr[0] = '\0';
	for (crl = PEM_read_bio_X509_CRL(infile, NULL, NULL, nullstr);
	    crl != NULL; crl = PEM_read_bio_X509_CRL(infile, NULL, NULL,
	    nullstr)) {
		LIST_FOREACH(slp, &rpctls_ssllist, next) {
			if (slp->cert != NULL) {
				ret = X509_CRL_get0_by_cert(crl, &revoked,
				    slp->cert);
				/*
				 * Do a shutdown on the socket, so that it
				 * can no longer be used.  The kernel RPC
				 * code will notice the socket is disabled
				 * and will do a disconnect upcall, which will
				 * close the socket.
				 */
				if (ret == 1) {
					cp2 = X509_NAME_oneline(
					    X509_get_subject_name(slp->cert),
					    NULL, 0);
					cp = X509_NAME_oneline(
					    X509_get_issuer_name(slp->cert),
					    NULL, 0);
					if (rpctls_debug_level == 0)
						syslog(LOG_INFO | LOG_DAEMON,
						    "rpctls_daemon: Certificate"
						    " Revoked "
						    "issuerName=%s "
						    "subjectName=%s: "
						    "TCP connection closed",
						    cp, cp2);
					else
						fprintf(stderr,
						    "rpctls_daemon: Certificate"
						    " Revoked "
						    "issuerName=%s "
						    "subjectName=%s: "
						    "TCP connection closed",
						    cp, cp2);
					shutdown(slp->s, SHUT_WR);
					slp->shutoff = true;
				}
			}
		}
		X509_CRL_free(crl);
	}
	BIO_free(infile);
}

void
rpctls_verbose_out(const char *fmt, ...)
{
	va_list ap;

	if (rpctls_verbose) {
		va_start(ap, fmt);
		if (rpctls_debug_level == 0)
			vsyslog(LOG_INFO | LOG_DAEMON, fmt, ap);
		else
			vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

/*
 * Check a IP address against any host address in the
 * certificate.  Basically getnameinfo(3) and
 * X509_check_host().
 */
int
rpctls_checkhost(struct sockaddr *sad, X509 *cert, unsigned int wildcard)
{
	char hostnam[NI_MAXHOST];
	int ret;

	if (getnameinfo((const struct sockaddr *)sad,
	    sad->sa_len, hostnam, sizeof(hostnam),
	    NULL, 0, NI_NAMEREQD) != 0)
		return (0);
	rpctls_verbose_out("rpctls_checkhost: DNS %s\n",
	    hostnam);
	ret = X509_check_host(cert, hostnam, strlen(hostnam),
	    wildcard, NULL);
	return (ret);
}

/*
 * Get the peer's IP address.
 */
int
rpctls_gethost(int s, struct sockaddr *sad, char *hostip, size_t hostlen)
{
	socklen_t slen;
	int ret;

	slen = sizeof(struct sockaddr_storage);
	if (getpeername(s, sad, &slen) < 0)
		return (0);
	ret = 0;
	if (getnameinfo((const struct sockaddr *)sad,
	    sad->sa_len, hostip, hostlen,
	    NULL, 0, NI_NUMERICHOST) == 0) {
		rpctls_verbose_out("rpctls_gethost: %s\n",
		    hostip);
		ret = 1;
	}
	return (ret);
}
