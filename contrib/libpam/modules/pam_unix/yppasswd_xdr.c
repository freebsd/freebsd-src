/* 
 * yppasswdd
 * Copyright 1994, 1995, 1996 Olaf Kirch, <okir@monad.swb.de>
 *
 * This program is covered by the GNU General Public License, version 2.
 * It is provided in the hope that it is useful. However, the author
 * disclaims ALL WARRANTIES, expressed or implied. See the GPL for details.
 * 
 * This file was generated automatically by rpcgen from yppasswd.x, and
 * editied manually.
 */

#include <security/_pam_aconf.h>

#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include "yppasswd.h"

bool_t
xdr_xpasswd(XDR * xdrs, xpasswd * objp)
{
	return xdr_string(xdrs, &objp->pw_name, ~0)
	    && xdr_string(xdrs, &objp->pw_passwd, ~0)
	    && xdr_int(xdrs, &objp->pw_uid)
	    && xdr_int(xdrs, &objp->pw_gid)
	    && xdr_string(xdrs, &objp->pw_gecos, ~0)
	    && xdr_string(xdrs, &objp->pw_dir, ~0)
	    && xdr_string(xdrs, &objp->pw_shell, ~0);
}


bool_t
xdr_yppasswd(XDR * xdrs, yppasswd * objp)
{
	return xdr_string(xdrs, &objp->oldpass, ~0)
	    && xdr_xpasswd(xdrs, &objp->newpw);
}
