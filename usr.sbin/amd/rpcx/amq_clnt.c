/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)amq_clnt.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

#include "am.h"
#include "amq.h"

static struct timeval TIMEOUT = { ALLOWED_MOUNT_TIME, 0 };

voidp
amqproc_null_1(argp, clnt)
	voidp argp;
	CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_NULL, xdr_void, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return ((voidp)&res);
}


amq_mount_tree_p *
amqproc_mnttree_1(argp, clnt)
	amq_string *argp;
	CLIENT *clnt;
{
	static amq_mount_tree_p res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_MNTTREE, xdr_amq_string, argp, xdr_amq_mount_tree_p, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


voidp
amqproc_umnt_1(argp, clnt)
	amq_string *argp;
	CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_UMNT, xdr_amq_string, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return ((voidp)&res);
}


amq_mount_stats *
amqproc_stats_1(argp, clnt)
	voidp argp;
	CLIENT *clnt;
{
	static amq_mount_stats res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_STATS, xdr_void, argp, xdr_amq_mount_stats, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


amq_mount_tree_list *
amqproc_export_1(argp, clnt)
	voidp argp;
	CLIENT *clnt;
{
	static amq_mount_tree_list res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_EXPORT, xdr_void, argp, xdr_amq_mount_tree_list, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}

int *
amqproc_setopt_1(argp, clnt)
	amq_setopt *argp;
	CLIENT *clnt;
{
	static int res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_SETOPT, xdr_amq_setopt, argp, xdr_int, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


amq_mount_info_list *
amqproc_getmntfs_1(argp, clnt)
	voidp argp;
	CLIENT *clnt;
{
	static amq_mount_info_list res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_GETMNTFS, xdr_void, argp, xdr_amq_mount_info_list, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


int *
amqproc_mount_1(argp, clnt)
	voidp argp;
	CLIENT *clnt;
{
	static int res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_MOUNT, xdr_amq_string, argp, xdr_int, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


amq_string *
amqproc_getvers_1(argp, clnt)
	voidp argp;
	CLIENT *clnt;
{
	static amq_string res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, AMQPROC_GETVERS, xdr_void, argp, xdr_amq_string, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}

