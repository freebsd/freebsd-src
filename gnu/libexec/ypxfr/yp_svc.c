#include <stdio.h>
#include <rpc/rpc.h>
#include "yp.h"
#ifndef lint
/*static char sccsid[] = "from: @(#)yp.x	2.1 88/08/01 4.0 RPCSRC";*/
static char rcsid[] = "yp.x,v 1.1 1994/08/04 19:01:55 wollman Exp";
#endif /* not lint */

static void ypprog_2();
static void yppush_xfrrespprog_1();
static void ypbindprog_2();

main()
{
	SVCXPRT *transp;

	(void)pmap_unset(YPPROG, YPVERS);
	(void)pmap_unset(YPPUSH_XFRRESPPROG, YPPUSH_XFRRESPVERS);
	(void)pmap_unset(YPBINDPROG, YPBINDVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		(void)fprintf(stderr, "cannot create udp service.\n");
		exit(1);
	}
	if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, IPPROTO_UDP)) {
		(void)fprintf(stderr, "unable to register (YPPROG, YPVERS, udp).\n");
		exit(1);
	}
	if (!svc_register(transp, YPPUSH_XFRRESPPROG, YPPUSH_XFRRESPVERS, yppush_xfrrespprog_1, IPPROTO_UDP)) {
		(void)fprintf(stderr, "unable to register (YPPUSH_XFRRESPPROG, YPPUSH_XFRRESPVERS, udp).\n");
		exit(1);
	}
	if (!svc_register(transp, YPBINDPROG, YPBINDVERS, ypbindprog_2, IPPROTO_UDP)) {
		(void)fprintf(stderr, "unable to register (YPBINDPROG, YPBINDVERS, udp).\n");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		(void)fprintf(stderr, "cannot create tcp service.\n");
		exit(1);
	}
	if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, IPPROTO_TCP)) {
		(void)fprintf(stderr, "unable to register (YPPROG, YPVERS, tcp).\n");
		exit(1);
	}
	if (!svc_register(transp, YPPUSH_XFRRESPPROG, YPPUSH_XFRRESPVERS, yppush_xfrrespprog_1, IPPROTO_TCP)) {
		(void)fprintf(stderr, "unable to register (YPPUSH_XFRRESPPROG, YPPUSH_XFRRESPVERS, tcp).\n");
		exit(1);
	}
	if (!svc_register(transp, YPBINDPROG, YPBINDVERS, ypbindprog_2, IPPROTO_TCP)) {
		(void)fprintf(stderr, "unable to register (YPBINDPROG, YPBINDVERS, tcp).\n");
		exit(1);
	}
	svc_run();
	(void)fprintf(stderr, "svc_run returned\n");
	exit(1);
}

static void
ypprog_2(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		domainname ypproc_domain_2_arg;
		domainname ypproc_domain_nonack_2_arg;
		ypreq_key ypproc_match_2_arg;
		ypreq_key ypproc_first_2_arg;
		ypreq_key ypproc_next_2_arg;
		ypreq_xfr ypproc_xfr_2_arg;
		ypreq_nokey ypproc_all_2_arg;
		ypreq_nokey ypproc_master_2_arg;
		ypreq_nokey ypproc_order_2_arg;
		domainname ypproc_maplist_2_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case YPPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) ypproc_null_2;
		break;

	case YPPROC_DOMAIN:
		xdr_argument = xdr_domainname;
		xdr_result = xdr_bool;
		local = (char *(*)()) ypproc_domain_2;
		break;

	case YPPROC_DOMAIN_NONACK:
		xdr_argument = xdr_domainname;
		xdr_result = xdr_bool;
		local = (char *(*)()) ypproc_domain_nonack_2;
		break;

	case YPPROC_MATCH:
		xdr_argument = xdr_ypreq_key;
		xdr_result = xdr_ypresp_val;
		local = (char *(*)()) ypproc_match_2;
		break;

	case YPPROC_FIRST:
		xdr_argument = xdr_ypreq_key;
		xdr_result = xdr_ypresp_key_val;
		local = (char *(*)()) ypproc_first_2;
		break;

	case YPPROC_NEXT:
		xdr_argument = xdr_ypreq_key;
		xdr_result = xdr_ypresp_key_val;
		local = (char *(*)()) ypproc_next_2;
		break;

	case YPPROC_XFR:
		xdr_argument = xdr_ypreq_xfr;
		xdr_result = xdr_ypresp_xfr;
		local = (char *(*)()) ypproc_xfr_2;
		break;

	case YPPROC_CLEAR:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) ypproc_clear_2;
		break;

	case YPPROC_ALL:
		xdr_argument = xdr_ypreq_nokey;
		xdr_result = __xdr_ypresp_all;
		local = (char *(*)()) ypproc_all_2;
		break;

	case YPPROC_MASTER:
		xdr_argument = xdr_ypreq_nokey;
		xdr_result = xdr_ypresp_master;
		local = (char *(*)()) ypproc_master_2;
		break;

	case YPPROC_ORDER:
		xdr_argument = xdr_ypreq_nokey;
		xdr_result = xdr_ypresp_order;
		local = (char *(*)()) ypproc_order_2;
		break;

	case YPPROC_MAPLIST:
		xdr_argument = xdr_domainname;
		xdr_result = xdr_ypresp_maplist;
		local = (char *(*)()) ypproc_maplist_2;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
}


static void
yppush_xfrrespprog_1(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		int fill;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case YPPUSHPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) yppushproc_null_1;
		break;

	case YPPUSHPROC_XFRRESP:
		xdr_argument = xdr_void;
		xdr_result = xdr_yppushresp_xfr;
		local = (char *(*)()) yppushproc_xfrresp_1;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
}


static void
ypbindprog_2(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		domainname ypbindproc_domain_2_arg;
		ypbind_setdom ypbindproc_setdom_2_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case YPBINDPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) ypbindproc_null_2;
		break;

	case YPBINDPROC_DOMAIN:
		xdr_argument = xdr_domainname;
		xdr_result = xdr_ypbind_resp;
		local = (char *(*)()) ypbindproc_domain_2;
		break;

	case YPBINDPROC_SETDOM:
		xdr_argument = xdr_ypbind_setdom;
		xdr_result = xdr_void;
		local = (char *(*)()) ypbindproc_setdom_2;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
}

