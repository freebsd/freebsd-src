/*
    YPS-0.2, NIS-Server for Linux
    Copyright (C) 1994  Tobias Reber

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Modified for use with FreeBSD 2.x by Bill Paul (wpaul@ctr.columbia.edu)

	$Id$
*/
/*
 *	$Author: root $
 *	$Log: yppush_s.c,v $
 * Revision 1.3  1994/01/02  21:59:08  root
 * Strict prototypes
 *
 * Revision 1.2  1994/01/02  20:10:08  root
 * Added GPL notice
 *
 * Revision 1.1  1994/01/02  18:04:08  root
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>

enum yppush_status {
	YPPUSH_SUCC = 1,
	YPPUSH_AGE = 2,
	YPPUSH_NOMAP = -1,
	YPPUSH_NODOM = -2,
	YPPUSH_RSRC = -3,
	YPPUSH_RPC = -4,
	YPPUSH_MADDR = -5,
	YPPUSH_YPERR = -6,
	YPPUSH_BADARGS = -7,
	YPPUSH_DBM = -8,
	YPPUSH_FILE = -9,
	YPPUSH_SKEW = -10,
	YPPUSH_CLEAR = -11,
	YPPUSH_FORCE = -12,
	YPPUSH_XFRERR = -13,
	YPPUSH_REFUSED = -14,
};
typedef enum yppush_status yppush_status;
bool_t __xdr_yppush_status(XDR *, void *);

static inline char *
yppush_err_string(enum yppush_status y) {
	switch(y) {
	case YPPUSH_SUCC:	return "Success";
	case YPPUSH_AGE:	return "Master's version not newer";
	case YPPUSH_NOMAP:	return "Can't find server for map";
	case YPPUSH_NODOM:	return "Domain not supported";
	case YPPUSH_RSRC:	return "Local resource alloc failure";
	case YPPUSH_RPC:	return "RPC failure talking to server";
	case YPPUSH_MADDR:	return "Can't get master address";
	case YPPUSH_YPERR:	return "YP server/map db error";
	case YPPUSH_BADARGS:	return "Request arguments bad";
	case YPPUSH_DBM:	return "Local dbm operation failed";
	case YPPUSH_FILE:	return "Local file I/O operation failed";
	case YPPUSH_SKEW:	return "Map version skew during transfer";
	case YPPUSH_CLEAR:	return "Can't send \"Clear\" req to local ypserv";
	case YPPUSH_FORCE:	return "No local order number in map  use -f flag.";
	case YPPUSH_XFRERR:	return "ypxfr error";
	case YPPUSH_REFUSED:	return "Transfer request refused by ypserv";
	}
}

struct yppushresp_xfr {
	u_int transid;
	yppush_status status;
};
typedef struct yppushresp_xfr yppushresp_xfr;
bool_t __xdr_yppushresp_xfr(XDR *, void *);


#define YPPUSH_XFRRESPPROG ((u_long)0x40000000)
#define YPPUSH_XFRRESPVERS ((u_long)1)

#define YPPUSHPROC_NULL ((u_long)0)
static inline void *
yppushproc_null_1(void * req, struct svc_req * rqstp) {
	static int resp;
	return &resp;
}

#define YPPUSHPROC_XFRRESP ((u_long)1)
static inline void *
yppushproc_xfrresp_1(yppushresp_xfr *req, struct svc_req * rqstp) {
	static int resp;
	
	if (req->status!=YPPUSH_SUCC)
		fprintf(stderr, "YPPUSH: %s\n", yppush_err_string(req->status));
	return &resp;
}

void
yppush_xfrrespprog_1( struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		int fill;
	} argument;
	char *result;
	bool_t (*xdr_argument)(XDR *, void *), (*xdr_result)(XDR *, void *);
	char *(*local)( void *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case YPPUSHPROC_NULL:
		xdr_argument = (bool_t (*)(XDR *, void *))xdr_void;
		xdr_result = (bool_t (*)(XDR *, void *))xdr_void;
		local = (char *(*)( void *, struct svc_req *)) yppushproc_null_1;
		break;

	case YPPUSHPROC_XFRRESP:
		xdr_argument = __xdr_yppushresp_xfr;
		xdr_result = (bool_t (*)(XDR *, void *))xdr_void;
		local = (char *(*)( void *, struct svc_req *)) yppushproc_xfrresp_1;
		break;

	default:
		svcerr_noproc(transp);
		exit(1);
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		exit(1);
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	if (rqstp->rq_proc!=YPPUSHPROC_NULL)
		exit(0);
	}
	exit(0);
}
