/* 
 * yppasswdd
 * Copyright 1994 Olaf Kirch, <okir@monad.swb.de>
 *
 * This program is covered by the GNU General Public License, version 2.
 * It is provided in the hope that it is useful. However, the author
 * disclaims ALL WARRANTIES, expressed or implied. See the GPL for details.
 * 
 * This file was generated automatically by rpcgen from yppasswd.x, and
 * editied manually.
 */

#include <rpc/rpc.h>
#include "yppasswd.h"


bool_t
xdr_xpasswd(XDR *xdrs, xpasswd *objp)
{
	if (!xdr_string(xdrs, &objp->pw_name, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_passwd, ~0)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->pw_uid)) {
		return (FALSE);
	}
	if (!xdr_int(xdrs, &objp->pw_gid)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_gecos, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_dir, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->pw_shell, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}


bool_t
xdr_yppasswd(XDR *xdrs, yppasswd *objp)
{
	if (!xdr_string(xdrs, &objp->oldpass, ~0)) {
		return (FALSE);
	}
	if (!xdr_xpasswd(xdrs, &objp->newpw)) {
		return (FALSE);
	}
	return (TRUE);
}


