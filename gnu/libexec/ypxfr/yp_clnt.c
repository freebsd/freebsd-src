#include <rpc/rpc.h>
#include "yp.h"
#ifndef lint
/*static char sccsid[] = "from: @(#)yp.x	2.1 88/08/01 4.0 RPCSRC";*/
static char rcsid[] = "yp.x,v 1.1 1994/08/04 19:01:55 wollman Exp";
#endif /* not lint */

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

void *
ypproc_null_2(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_NULL, xdr_void, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return ((void *)&res);
}


bool_t *
ypproc_domain_2(argp, clnt)
	domainname *argp;
	CLIENT *clnt;
{
	static bool_t res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_DOMAIN, xdr_domainname, argp, xdr_bool, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


bool_t *
ypproc_domain_nonack_2(argp, clnt)
	domainname *argp;
	CLIENT *clnt;
{
	static bool_t res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_DOMAIN_NONACK, xdr_domainname, argp, xdr_bool, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


ypresp_val *
ypproc_match_2(argp, clnt)
	ypreq_key *argp;
	CLIENT *clnt;
{
	static ypresp_val res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_MATCH, xdr_ypreq_key, argp, xdr_ypresp_val, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


ypresp_key_val *
ypproc_first_2(argp, clnt)
	ypreq_key *argp;
	CLIENT *clnt;
{
	static ypresp_key_val res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_FIRST, xdr_ypreq_key, argp, xdr_ypresp_key_val, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


ypresp_key_val *
ypproc_next_2(argp, clnt)
	ypreq_key *argp;
	CLIENT *clnt;
{
	static ypresp_key_val res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_NEXT, xdr_ypreq_key, argp, xdr_ypresp_key_val, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


ypresp_xfr *
ypproc_xfr_2(argp, clnt)
	ypreq_xfr *argp;
	CLIENT *clnt;
{
	static ypresp_xfr res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_XFR, xdr_ypreq_xfr, argp, xdr_ypresp_xfr, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


void *
ypproc_clear_2(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_CLEAR, xdr_void, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return ((void *)&res);
}


ypresp_all *
ypproc_all_2(argp, clnt)
	ypreq_nokey *argp;
	CLIENT *clnt;
{
	static ypresp_all res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_ALL, xdr_ypreq_nokey, argp, __xdr_ypresp_all, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


ypresp_master *
ypproc_master_2(argp, clnt)
	ypreq_nokey *argp;
	CLIENT *clnt;
{
	static ypresp_master res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_MASTER, xdr_ypreq_nokey, argp, xdr_ypresp_master, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


ypresp_order *
ypproc_order_2(argp, clnt)
	ypreq_nokey *argp;
	CLIENT *clnt;
{
	static ypresp_order res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_ORDER, xdr_ypreq_nokey, argp, xdr_ypresp_order, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


ypresp_maplist *
ypproc_maplist_2(argp, clnt)
	domainname *argp;
	CLIENT *clnt;
{
	static ypresp_maplist res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPROC_MAPLIST, xdr_domainname, argp, xdr_ypresp_maplist, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


void *
yppushproc_null_1(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPUSHPROC_NULL, xdr_void, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return ((void *)&res);
}


yppushresp_xfr *
yppushproc_xfrresp_1(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	static yppushresp_xfr res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPPUSHPROC_XFRRESP, xdr_void, argp, xdr_yppushresp_xfr, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


void *
ypbindproc_null_2(argp, clnt)
	void *argp;
	CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPBINDPROC_NULL, xdr_void, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return ((void *)&res);
}


ypbind_resp *
ypbindproc_domain_2(argp, clnt)
	domainname *argp;
	CLIENT *clnt;
{
	static ypbind_resp res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPBINDPROC_DOMAIN, xdr_domainname, argp, xdr_ypbind_resp, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&res);
}


void *
ypbindproc_setdom_2(argp, clnt)
	ypbind_setdom *argp;
	CLIENT *clnt;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	if (clnt_call(clnt, YPBINDPROC_SETDOM, xdr_ypbind_setdom, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return ((void *)&res);
}

