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
 *	$Log: yp_clnt.c,v $
 * Revision 0.16  1994/01/02  22:48:22  root
 * Added strict prototypes
 *
 * Revision 0.15  1994/01/02  20:09:39  root
 * Added GPL notice
 *
 * Revision 0.14  1993/12/19  12:42:32  root
 * *** empty log message ***
 *
 * Revision 0.13  1993/06/12  09:39:30  root
 * Align with include-4.4
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/time.h>
#include "yp.h"

#ifdef DEBUG
#define PRINTF(x) printf x
#else
#define PRINTF(x)
#endif

static struct timeval TIMEOUT = { 25, 0 };
 
void *
ypproc_null_2( int *argp, CLIENT *clnt)
{
        static char res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_NULL, xdr_void, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return ((void *)&res);
}
 
 
bool_t *
ypproc_domain_2( domainname *argp, CLIENT *clnt)
{
        static bool_t res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_DOMAIN, xdr_domainname, argp, xdr_bool, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
 
bool_t *
ypproc_domain_nonack_2( domainname *argp, CLIENT *clnt)
{
        static bool_t res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_DOMAIN_NONACK, xdr_domainname, argp, xdr_bool, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
 
ypresp_val *
ypproc_match_2( ypreq_key *argp, CLIENT *clnt)
{
        static ypresp_val res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_MATCH, __xdr_ypreq_key, argp, __xdr_ypresp_val, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
 
ypresp_key_val *
ypproc_first_2( ypreq_key *argp, CLIENT *clnt)
{
        static ypresp_key_val res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_FIRST, __xdr_ypreq_key, argp, __xdr_ypresp_key_val, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
 
ypresp_key_val *
ypproc_next_2( ypreq_key *argp, CLIENT *clnt)
{
        static ypresp_key_val res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_NEXT, __xdr_ypreq_key, argp, __xdr_ypresp_key_val, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
 
ypresp_xfr *
ypproc_xfr_2( ypreq_xfr *argp, CLIENT *clnt)
{
        static ypresp_xfr res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_XFR, __xdr_ypreq_xfr, argp, __xdr_ypresp_xfr, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
 
void *
ypproc_clear_2( int *argp, CLIENT *clnt)
{
        static char res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_CLEAR, xdr_void, argp, xdr_void, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return ((void *)&res);
}
 
 
ypresp_all *
ypproc_all_2( ypreq_nokey *argp, CLIENT *clnt)
{
        static ypresp_all res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_ALL, __xdr_ypreq_nokey, argp, __xdr_ypresp_all, &res, TIMEOUT) != RPC_SUCCESS) {
		PRINTF(("ypproc_all_2 retuning NULL\n"));
                return (NULL);
        }
	PRINTF(("ypproc_all_2 retuning non-NULL\n"));
        return (&res);
}
 
 
ypresp_master *
ypproc_master_2( ypreq_nokey *argp, CLIENT *clnt)
{
        static ypresp_master res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_MASTER, __xdr_ypreq_nokey, argp, __xdr_ypresp_master, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
 
ypresp_order *
ypproc_order_2( ypreq_nokey *argp, CLIENT *clnt)
{
        static ypresp_order res;
	PRINTF (("ypproc_order_2()\n"));
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_ORDER, __xdr_ypreq_nokey, argp, __xdr_ypresp_order, &res, TIMEOUT) != RPC_SUCCESS) {
	PRINTF (("ypproc_order_2()\n"));
                return (NULL);
        }
	PRINTF (("ypproc_order_2()\n"));
        return (&res);
}
 
 
ypresp_maplist *
ypproc_maplist_2( domainname *argp, CLIENT *clnt)
{
        static ypresp_maplist res;
 
        bzero(&res, sizeof(res));
        if (clnt_call(clnt, YPPROC_MAPLIST, xdr_domainname, argp, __xdr_ypresp_maplist, &res, TIMEOUT) != RPC_SUCCESS) {
                return (NULL);
        }
        return (&res);
}
 
