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
 *	@(#)misc_rpc.c	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 *
 */

/*
 * Additions to Sun RPC.
 */

#include "am.h"

void rpc_msg_init P((struct rpc_msg *mp, u_long prog, u_long vers, u_long proc));
void rpc_msg_init(mp, prog, vers, proc)
struct rpc_msg *mp;
unsigned long prog, vers, proc;
{
	/*
	 * Initialise the message
	 */
	bzero((voidp) mp, sizeof(*mp));
	mp->rm_xid = 0;
	mp->rm_direction = CALL;
	mp->rm_call.cb_rpcvers = RPC_MSG_VERSION;
	mp->rm_call.cb_prog = prog;
	mp->rm_call.cb_vers = vers;
	mp->rm_call.cb_proc = proc;
}

/*
 * Field reply to call to mountd
 */
int pickup_rpc_reply P((voidp pkt, int len, voidp where, xdrproc_t where_xdr));
int pickup_rpc_reply(pkt, len, where, where_xdr)
voidp pkt;
int len;
voidp where;
xdrproc_t where_xdr;
{
	XDR reply_xdr;
	int ok;
	struct rpc_err err;
	struct rpc_msg reply_msg;
	int error = 0;

	/*bzero((voidp) &err, sizeof(err));*/
	bzero((voidp) &reply_msg, sizeof(reply_msg));

	reply_msg.acpted_rply.ar_results.where = (caddr_t) where;
	reply_msg.acpted_rply.ar_results.proc = where_xdr;

	xdrmem_create(&reply_xdr, pkt, len, XDR_DECODE);

	ok = xdr_replymsg(&reply_xdr, &reply_msg);
	if (!ok) {
		error = EIO;
		goto drop;
	}
	_seterr_reply(&reply_msg, &err);
	if (err.re_status != RPC_SUCCESS) {
		error = EIO;
		goto drop;
	}

drop:
	if (reply_msg.rm_reply.rp_stat == MSG_ACCEPTED &&
			reply_msg.acpted_rply.ar_verf.oa_base) {
		reply_xdr.x_op = XDR_FREE;
		(void)xdr_opaque_auth(&reply_xdr,
			&reply_msg.acpted_rply.ar_verf);
	}
	xdr_destroy(&reply_xdr);

	return error;
}

int make_rpc_packet P((char *buf, int buflen, unsigned long proc,
			struct rpc_msg *mp, voidp arg, xdrproc_t arg_xdr, AUTH *auth));
int make_rpc_packet(buf, buflen, proc, mp, arg, arg_xdr, auth)
char *buf;
int buflen;
unsigned long proc;
struct rpc_msg *mp;
voidp arg;
xdrproc_t arg_xdr;
AUTH *auth;
{
	XDR msg_xdr;
	int len;

	xdrmem_create(&msg_xdr, buf, buflen, XDR_ENCODE);
	/*
	 * Basic protocol header
	 */
	if (!xdr_callhdr(&msg_xdr, mp))
		return -EIO;
	/*
	 * Called procedure number
	 */
	if (!xdr_enum(&msg_xdr, (enum_t *)&proc))
		return -EIO;
	/*
	 * Authorization
	 */
	if (!AUTH_MARSHALL(auth, &msg_xdr))
		return -EIO;
	/*
	 * Arguments
	 */
	if (!(*arg_xdr)(&msg_xdr, arg))
		return -EIO;
	/*
	 * Determine length
	 */
	len = xdr_getpos(&msg_xdr);
	/*
	 * Throw away xdr
	 */
	xdr_destroy(&msg_xdr);
	return len;
}


/*
 * Early RPC seems to be missing these..
 * Extracted from the RPC 3.9 sources as indicated
 */

#ifdef NEED_XDR_POINTER
/* @(#)xdr_reference.c	1.1 87/11/04 3.9 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */


/*
 * xdr_pointer():
 *
 * XDR a pointer to a possibly recursive data structure. This
 * differs with xdr_reference in that it can serialize/deserialiaze
 * trees correctly.
 *
 *  What's sent is actually a union:
 *
 *  union object_pointer switch (boolean b) {
 *  case TRUE: object_data data;
 *  case FALSE: void nothing;
 *  }
 *
 * > objpp: Pointer to the pointer to the object.
 * > obj_size: size of the object.
 * > xdr_obj: routine to XDR an object.
 *
 */
bool_t
xdr_pointer(xdrs,objpp,obj_size,xdr_obj)
	register XDR *xdrs;
	char **objpp;
	u_int obj_size;
	xdrproc_t xdr_obj;
{

	bool_t more_data;

	more_data = (*objpp != NULL);
	if (! xdr_bool(xdrs,&more_data)) {
		return (FALSE);
	}
	if (! more_data) {
		*objpp = NULL;
		return (TRUE);
	}
	return (xdr_reference(xdrs,objpp,obj_size,xdr_obj));
}
#endif /* NEED_XDR_POINTER */

#ifdef NEED_CLNT_SPERRNO
/* @(#)clnt_perror.c	1.1 87/11/04 3.9 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

struct rpc_errtab {
	enum clnt_stat status;
	char *message;
};

static struct rpc_errtab  rpc_errlist[] = {
	{ RPC_SUCCESS,
		"RPC: Success" },
	{ RPC_CANTENCODEARGS,
		"RPC: Can't encode arguments" },
	{ RPC_CANTDECODERES,
		"RPC: Can't decode result" },
	{ RPC_CANTSEND,
		"RPC: Unable to send" },
	{ RPC_CANTRECV,
		"RPC: Unable to receive" },
	{ RPC_TIMEDOUT,
		"RPC: Timed out" },
	{ RPC_VERSMISMATCH,
		"RPC: Incompatible versions of RPC" },
	{ RPC_AUTHERROR,
		"RPC: Authentication error" },
	{ RPC_PROGUNAVAIL,
		"RPC: Program unavailable" },
	{ RPC_PROGVERSMISMATCH,
		"RPC: Program/version mismatch" },
	{ RPC_PROCUNAVAIL,
		"RPC: Procedure unavailable" },
	{ RPC_CANTDECODEARGS,
		"RPC: Server can't decode arguments" },
	{ RPC_SYSTEMERROR,
		"RPC: Remote system error" },
	{ RPC_UNKNOWNHOST,
		"RPC: Unknown host" },
/*	{ RPC_UNKNOWNPROTO,
		"RPC: Unknown protocol" },*/
	{ RPC_PMAPFAILURE,
		"RPC: Port mapper failure" },
	{ RPC_PROGNOTREGISTERED,
		"RPC: Program not registered"},
	{ RPC_FAILED,
		"RPC: Failed (unspecified error)"}
};


/*
 * This interface for use by clntrpc
 */
char *
clnt_sperrno(stat)
	enum clnt_stat stat;
{
	int i;

	for (i = 0; i < sizeof(rpc_errlist)/sizeof(struct rpc_errtab); i++) {
		if (rpc_errlist[i].status == stat) {
			return (rpc_errlist[i].message);
		}
	}
	return ("RPC: (unknown error code)");
}

#endif /* NEED_CLNT_SPERRNO */

