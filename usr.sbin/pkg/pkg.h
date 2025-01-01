/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012-2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2013 Bryan Drewery <bdrewery@FreeBSD.org>
 * All rights reserved.
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

#ifndef _PKG_H
#define	_PKG_H

#include <stdbool.h>

struct pkgsign_ctx {
	const struct pkgsign_impl	*impl;
};

/* Tentatively won't be needing to free any state, all allocated in the ctx. */
typedef int pkgsign_new_cb(const char *, struct pkgsign_ctx *);
typedef bool pkgsign_verify_cert_cb(const struct pkgsign_ctx *, int,
    const char *, const unsigned char *, int, unsigned char *, int);
typedef bool pkgsign_verify_data_cb(const struct pkgsign_ctx *,
    const char *, size_t, const char *, const unsigned char *, int,
    unsigned char *, int);

struct pkgsign_ops {
	size_t			 pkgsign_ctx_size;
	pkgsign_new_cb		*pkgsign_new;
	pkgsign_verify_cert_cb	*pkgsign_verify_cert;
	pkgsign_verify_data_cb	*pkgsign_verify_data;
};

extern const struct pkgsign_ops pkgsign_ecc;
extern const struct pkgsign_ops pkgsign_rsa;

struct sig_cert {
	char *name;
	char *type;
	unsigned char *sig;
	int siglen;
	unsigned char *cert;
	int certlen;
	bool trusted;
};

struct pubkey {
	char *sigtype;
	unsigned char *sig;
	int siglen;
};

char *pkg_read_fd(int fd, size_t *osz);

#endif /* _PKG_H */
