/*
 * Copyright (c) 1992/3 Theo de Raadt <deraadt@fsa.ca>
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$FreeBSD$";
#endif

#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <stdlib.h>
#include <string.h>

extern int (*ypresp_allfn)();
extern void *ypresp_data;

/*
 * I'm leaving the xdr_datum() function in purely for backwards
 * compatibility. yplib.c doesn't actually use it, but it's listed
 * in yp_prot.h as being available, so it's probably a good idea to
 * leave it in in case somebody goes looking for it.
 */
typedef struct {
	char *dptr;
	int  dsize;
} datum;

bool_t
xdr_datum(xdrs, objp)
XDR *xdrs;
datum *objp;
{
	if (!xdr_bytes(xdrs, (char **)&objp->dptr, (u_int *)&objp->dsize, YPMAXRECORD)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_all_seq(xdrs, objp)
XDR *xdrs;
u_long *objp;
{
	struct ypresp_all out;
	u_long status;
	char *key, *val;
	int r;

	bzero(&out, sizeof out);
	while(1) {
		if( !xdr_ypresp_all(xdrs, &out)) {
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = YP_YPERR;
			return FALSE;
		}
		if(out.more == 0) {
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = YP_NOMORE;
			return TRUE;
		}
		status = out.ypresp_all_u.val.stat;
		switch(status) {
		case YP_TRUE:
			key = (char *)malloc(out.ypresp_all_u.val.key.keydat_len + 1);
			bcopy(out.ypresp_all_u.val.key.keydat_val, key,
				out.ypresp_all_u.val.key.keydat_len);
			key[out.ypresp_all_u.val.key.keydat_len] = '\0';
			val = (char *)malloc(out.ypresp_all_u.val.val.valdat_len + 1);
			bcopy(out.ypresp_all_u.val.val.valdat_val, val,
				out.ypresp_all_u.val.val.valdat_len);
			val[out.ypresp_all_u.val.val.valdat_len] = '\0';
			xdr_free(xdr_ypresp_all, (char *)&out);

			r = (*ypresp_allfn)(status,
				key, out.ypresp_all_u.val.key.keydat_len,
				val, out.ypresp_all_u.val.val.valdat_len,
				ypresp_data);
			*objp = status;
			free(key);
			free(val);
			if(r)
				return TRUE;
			break;
		case YP_NOMORE:
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = YP_NOMORE;
			return TRUE;
		default:
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = status;
			return TRUE;
		}
	}
}
