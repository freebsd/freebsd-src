/*-
 * Copyright (c) 2003 Tim J. Robbins.
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
 *
 * $FreeBSD: src/sys/netncp/ncpio.h,v 1.1 2003/02/28 04:31:29 tjr Exp $
 */

#ifndef _NETNCP_NCPIO_H_
#define _NETNCP_NCPIO_H_

#ifndef _KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>

#define	NCP_NAME	"ncp"

struct ncp_conn_args;
struct ncp_buf;

struct ncpioc_connect {
	struct ncp_conn_args	*ioc_li;
	int			*ioc_connhandle;
};

struct ncpioc_request {
	int			ioc_connhandle;
	int			ioc_fn;
	struct ncp_buf		*ioc_ncpbuf;
};

struct ncpioc_connscan {
	struct ncp_conn_args	*ioc_li;
	int			*ioc_connhandle;
};

#define	NCPIOC_CONNECT		_IOW('N',  100, struct ncpioc_connect)
#define	NCPIOC_REQUEST		_IOW('N',  101, struct ncpioc_request)
#define	NCPIOC_CONNSCAN		_IOW('N',  102, struct ncpioc_connscan)

#endif /* _NETNCP_NCPIO_H_ */
