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

#ifndef LINT
static char *rcsid = "$Id: xdryp.c,v 1.3 1995/04/02 19:58:29 wpaul Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

extern int (*ypresp_allfn)();
extern void *ypresp_data;

struct ypresp_all {
	bool_t more;
	union {
		struct ypresp_key_val val;
	} ypresp_all_u;
};

enum ypxfrstat {
	YPXFR_SUCC = 1,
	YPXFR_AGE = 2,
	YPXFR_NOMAP = -1,
	YPXFR_NODOM = -2,
	YPXFR_RSRC = -3,
	YPXFR_RPC = -4,
	YPXFR_MADDR = -5,
	YPXFR_YPERR = -6,
	YPXFR_BADARGS = -7,
	YPXFR_DBM = -8,
	YPXFR_FILE = -9,
	YPXFR_SKEW = -10,
	YPXFR_CLEAR = -11,
	YPXFR_FORCE = -12,
	YPXFR_XFRERR = -13,
	YPXFR_REFUSED = -14,
};

struct ypresp_xfr {
	u_int transid;
	enum ypxfrstat xfrstat;
};

bool_t
xdr_domainname(xdrs, objp)
XDR *xdrs;
char *objp;
{
	if (!xdr_string(xdrs, &objp, YPMAXDOMAIN)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_peername(xdrs, objp)
XDR *xdrs;
char *objp;
{
	if (!xdr_string(xdrs, &objp, YPMAXPEER)) {
		return (FALSE);
	}
	return (TRUE);
}

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
xdr_mapname(xdrs, objp)
XDR *xdrs;
char *objp;
{
	if (!xdr_string(xdrs, &objp, YPMAXMAP)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypreq_key(xdrs, objp)
XDR *xdrs;
struct ypreq_key *objp;
{
	if (!xdr_domainname(xdrs, objp->domain)) {
		return (FALSE);
	}
	if (!xdr_mapname(xdrs, objp->map)) {
		return (FALSE);
	}
	if (!xdr_datum(xdrs, &objp->keydat)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypreq_nokey(xdrs, objp)
XDR *xdrs;
struct ypreq_nokey *objp;
{
	if (!xdr_domainname(xdrs, objp->domain)) {
		return (FALSE);
	}
	if (!xdr_mapname(xdrs, objp->map)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_yp_inaddr(xdrs, objp)
XDR *xdrs;
struct in_addr *objp;
{
	if (!xdr_opaque(xdrs, (caddr_t)&objp->s_addr, sizeof objp->s_addr)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypbind_binding(xdrs, objp)
XDR *xdrs;
struct ypbind_binding *objp;
{
	if (!xdr_yp_inaddr(xdrs, &objp->ypbind_binding_addr)) {
		return (FALSE);
	}
	if (!xdr_opaque(xdrs, (void *)&objp->ypbind_binding_port,
	    sizeof objp->ypbind_binding_port)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypbind_resptype(xdrs, objp)
XDR *xdrs;
enum ypbind_resptype *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypstat(xdrs, objp)
XDR *xdrs;
enum ypbind_resptype *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypbind_resp(xdrs, objp)
XDR *xdrs;
struct ypbind_resp *objp;
{
	if (!xdr_ypbind_resptype(xdrs, &objp->ypbind_status)) {
		return (FALSE);
	}
	switch (objp->ypbind_status) {
	case YPBIND_FAIL_VAL:
		if (!xdr_u_int(xdrs, (u_int *)&objp->ypbind_respbody.ypbind_error)) {
			return (FALSE);
		}
		break;
	case YPBIND_SUCC_VAL:
		if (!xdr_ypbind_binding(xdrs, &objp->ypbind_respbody.ypbind_bindinfo)) {
			return (FALSE);
		}
		break;
	default:
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_val(xdrs, objp)
XDR *xdrs;
struct ypresp_val *objp;
{
	if (!xdr_ypstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	if (!xdr_datum(xdrs, &objp->valdat)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypbind_setdom(xdrs, objp)
XDR *xdrs;
struct ypbind_setdom *objp;
{
	if (!xdr_domainname(xdrs, objp->ypsetdom_domain)) {
		return (FALSE);
	}
	if (!xdr_ypbind_binding(xdrs, &objp->ypsetdom_binding)) {
		return (FALSE);
	}
	if (!xdr_u_short(xdrs, &objp->ypsetdom_vers)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_key_val(xdrs, objp)
XDR *xdrs;
struct ypresp_key_val *objp;
{
	if (!xdr_ypstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	if (!xdr_datum(xdrs, &objp->valdat)) {
		return (FALSE);
	}
	if (!xdr_datum(xdrs, &objp->keydat)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_all(xdrs, objp)
XDR *xdrs;
struct ypresp_all *objp;
{
	if (!xdr_bool(xdrs, &objp->more)) {
		return (FALSE);
	}
	switch (objp->more) {
	case TRUE:
		if (!xdr_ypresp_key_val(xdrs, &objp->ypresp_all_u.val)) {
			return (FALSE);
		}
		break;
	case FALSE:
		break;
	default:
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
			return FALSE;
		}
		status = out.ypresp_all_u.val.status;
		switch(status) {
		case YP_TRUE:
			key = (char *)malloc(out.ypresp_all_u.val.keydat.dsize + 1);
			bcopy(out.ypresp_all_u.val.keydat.dptr, key,
				out.ypresp_all_u.val.keydat.dsize);
			key[out.ypresp_all_u.val.keydat.dsize] = '\0';
			val = (char *)malloc(out.ypresp_all_u.val.valdat.dsize + 1);
			bcopy(out.ypresp_all_u.val.valdat.dptr, val,
				out.ypresp_all_u.val.valdat.dsize);
			val[out.ypresp_all_u.val.valdat.dsize] = '\0';
			xdr_free(xdr_ypresp_all, (char *)&out);

			r = (*ypresp_allfn)(status,
				key, out.ypresp_all_u.val.keydat.dsize,
				val, out.ypresp_all_u.val.valdat.dsize,
				ypresp_data);
			*objp = status;
			free(key);
			free(val);
			if(r)
				return TRUE;
			break;
		case YP_NOMORE:
			xdr_free(xdr_ypresp_all, (char *)&out);
			return TRUE;
		default:
			xdr_free(xdr_ypresp_all, (char *)&out);
			*objp = status;
			return TRUE;
		}
	}
}

bool_t
xdr_ypresp_master(xdrs, objp)
XDR *xdrs;
struct ypresp_master *objp;
{
	if (!xdr_ypstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->master, YPMAXPEER)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypmaplist_str(xdrs, objp)
XDR *xdrs;
char *objp;
{
	if (!xdr_string(xdrs, &objp, YPMAXMAP+1)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypmaplist(xdrs, objp)
XDR *xdrs;
struct ypmaplist *objp;
{
	if (!xdr_ypmaplist_str(xdrs, objp->ypml_name)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (caddr_t *)&objp->ypml_next,
	    sizeof(struct ypmaplist), xdr_ypmaplist)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_maplist(xdrs, objp)
XDR *xdrs;
struct ypresp_maplist *objp;
{
	if (!xdr_ypstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	if (!xdr_pointer(xdrs, (caddr_t *)&objp->list,
	    sizeof(struct ypmaplist), xdr_ypmaplist)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_order(xdrs, objp)
XDR *xdrs;
struct ypresp_order *objp;
{
	if (!xdr_ypstat(xdrs, &objp->status)) {
		return (FALSE);
	}
	if (!xdr_u_long(xdrs, &objp->ordernum)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypxfrstat(xdrs, objp)
XDR *xdrs;
enum ypxfrstat *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypresp_xfr(xdrs, objp)
XDR *xdrs;
struct ypresp_xfr *objp;
{
	if (!xdr_u_int(xdrs, &objp->transid)) {
		return (FALSE);
	}
	if (!xdr_ypxfrstat(xdrs, &objp->xfrstat)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_ypmap_parms(xdrs, objp)
XDR *xdrs;
struct ypmap_parms *objp;
{
	if (!xdr_domainname(xdrs, objp->domain)) {
		return (FALSE);
	}
	if (!xdr_mapname(xdrs, objp->map)) {
		return (FALSE);
	}
	if (!xdr_u_long(xdrs, &objp->ordernum)) {
		return (FALSE);
	}
	if (!xdr_peername(xdrs, objp->owner)) {
		return (FALSE);
	}
}

bool_t
xdr_ypreq_xfr(xdrs, objp)
XDR *xdrs;
struct ypreq_xfr *objp;
{
	if (!xdr_ypmap_parms(xdrs, &objp->map_parms)) {
		return (FALSE);
	}
	if (!xdr_u_long(xdrs, &objp->transid)) {
		return (FALSE);
	}
	if (!xdr_u_long(xdrs, &objp->proto)) {
		return (FALSE);
	}
	if (!xdr_u_short(xdrs, &objp->port)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_yppush_status(xdrs, objp)
XDR *xdrs;
enum yppush_status *objp;
{
	if (!xdr_enum(xdrs, (enum_t *)objp)) {
		return (FALSE);
	}
	return (TRUE);
}

bool_t
xdr_yppushresp_xfr(xdrs, objp)
XDR *xdrs;
struct yppushresp_xfr *objp;
{
	if (!xdr_u_long(xdrs, &objp->transid)) {
		return (FALSE);
	}
	if (!xdr_yppush_status(xdrs, &objp->status)) {
		return (FALSE);
	}
	return (TRUE);
}
