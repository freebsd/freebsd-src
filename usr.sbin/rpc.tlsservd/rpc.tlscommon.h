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
 * $FreeBSD$
 */

/*
 * Functions in rpc.tlscommon.c used by both rpc.tlsservd.c and rpc.tlsclntd.c.
 */
int		rpctls_gethost(int s, struct sockaddr *sad,
		    char *hostip, size_t hostlen);
int		rpctls_checkhost(struct sockaddr *sad, X509 *cert,
		    unsigned int wildcard);
int		rpctls_loadcrlfile(SSL_CTX *ctx);
void		rpctls_checkcrl(void);
void		rpctls_verbose_out(const char *fmt, ...);
void		rpctls_svc_run(void);

/*
 * A linked list of all current "SSL *"s and socket "fd"s
 * for kernel RPC TLS connections is maintained.
 * The "refno" field is a unique 64bit value used to
 * identify which entry a kernel RPC upcall refers to.
 */
LIST_HEAD(ssl_list, ssl_entry);
struct ssl_entry {
	LIST_ENTRY(ssl_entry)	next;
	uint64_t		refno;
	int			s;
	bool			shutoff;
	SSL			*ssl;
	X509			*cert;
};

/* Global variables shared between rpc.tlscommon.c and the daemons. */
extern int			rpctls_debug_level;
extern bool			rpctls_verbose;
extern SSL_CTX			*rpctls_ctx;
extern const char		*rpctls_verify_cafile;
extern const char		*rpctls_verify_capath;
extern char			*rpctls_crlfile;
extern bool			rpctls_cert;
extern bool			rpctls_gothup;
extern struct ssl_list		rpctls_ssllist;

