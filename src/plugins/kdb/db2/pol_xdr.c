#include <sys/types.h>
#include <krb5.h>
#include <gssrpc/rpc.h>
#include <kdb.h>
#include <kadm5/admin_xdr.h>
#include "policy_db.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>

static int
osa_policy_min_vers(osa_policy_ent_t objp)
{
    if (objp->attributes ||
        objp->max_life ||
        objp->max_renewable_life ||
        objp->allowed_keysalts ||
        objp->n_tl_data)
        return OSA_ADB_POLICY_VERSION_3;

    if (objp->pw_max_fail ||
        objp->pw_failcnt_interval ||
        objp->pw_lockout_duration)
        return OSA_ADB_POLICY_VERSION_2;

    return OSA_ADB_POLICY_VERSION_1;
}

bool_t
xdr_osa_policy_ent_rec(XDR *xdrs, osa_policy_ent_t objp)
{
    switch (xdrs->x_op) {
    case XDR_ENCODE:
	 objp->version = osa_policy_min_vers(objp);
	 /* fall through */
    case XDR_FREE:
	 if (!xdr_int(xdrs, &objp->version))
	      return FALSE;
	 break;
    case XDR_DECODE:
	 if (!xdr_int(xdrs, &objp->version))
	      return FALSE;
	 if (objp->version != OSA_ADB_POLICY_VERSION_1 &&
             objp->version != OSA_ADB_POLICY_VERSION_2 &&
             objp->version != OSA_ADB_POLICY_VERSION_3)
	      return FALSE;
	 break;
    }

    if(!xdr_nullstring(xdrs, &objp->name))
	return (FALSE);
    if (!xdr_u_int32(xdrs, &objp->pw_min_life))
	return (FALSE);
    if (!xdr_u_int32(xdrs, &objp->pw_max_life))
	return (FALSE);
    if (!xdr_u_int32(xdrs, &objp->pw_min_length))
	return (FALSE);
    if (!xdr_u_int32(xdrs, &objp->pw_min_classes))
	return (FALSE);
    if (!xdr_u_int32(xdrs, &objp->pw_history_num))
	return (FALSE);
    if (!xdr_u_int32(xdrs, &objp->policy_refcnt))
	return (FALSE);
    if (objp->version > OSA_ADB_POLICY_VERSION_1) {
        if (!xdr_u_int32(xdrs, &objp->pw_max_fail))
	    return (FALSE);
        if (!xdr_u_int32(xdrs, &objp->pw_failcnt_interval))
	    return (FALSE);
        if (!xdr_u_int32(xdrs, &objp->pw_lockout_duration))
	    return (FALSE);
    }
    if (objp->version > OSA_ADB_POLICY_VERSION_2) {
        if (!xdr_u_int32(xdrs, &objp->attributes))
	    return (FALSE);
        if (!xdr_u_int32(xdrs, &objp->max_life))
	    return (FALSE);
        if (!xdr_u_int32(xdrs, &objp->max_renewable_life))
	    return (FALSE);
        if (!xdr_nullstring(xdrs, &objp->allowed_keysalts))
	    return (FALSE);
        if (!xdr_short(xdrs, &objp->n_tl_data))
            return (FALSE);
        if (!xdr_nulltype(xdrs, (void **) &objp->tl_data,
                          xdr_krb5_tl_data))
            return FALSE;
    }
    return (TRUE);
}
