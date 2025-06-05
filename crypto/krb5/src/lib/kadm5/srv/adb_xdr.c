/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

#include <sys/types.h>
#include <krb5.h>
#include <gssrpc/rpc.h>
#include	"server_internal.h"
#include "admin_xdr.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

bool_t
xdr_krb5_key_data(XDR *xdrs, krb5_key_data *objp)
{
    unsigned int tmp;

    if (!xdr_krb5_int16(xdrs, &objp->key_data_ver))
	return(FALSE);
    if (!xdr_krb5_ui_2(xdrs, &objp->key_data_kvno))
	return(FALSE);
    if (!xdr_krb5_int16(xdrs, &objp->key_data_type[0]))
	return(FALSE);
    if (!xdr_krb5_int16(xdrs, &objp->key_data_type[1]))
	return(FALSE);
    if (!xdr_krb5_ui_2(xdrs, &objp->key_data_length[0]))
	return(FALSE);
    if (!xdr_krb5_ui_2(xdrs, &objp->key_data_length[1]))
	return(FALSE);

    tmp = (unsigned int) objp->key_data_length[0];
    if (!xdr_bytes(xdrs, (char **) &objp->key_data_contents[0],
		   &tmp, ~0))
	return FALSE;

    tmp = (unsigned int) objp->key_data_length[1];
    if (!xdr_bytes(xdrs, (char **) &objp->key_data_contents[1],
		   &tmp, ~0))
	return FALSE;

    /* don't need to copy tmp out, since key_data_length will be set
       by the above encoding. */

    return(TRUE);
}

bool_t
xdr_osa_pw_hist_ent(XDR *xdrs, osa_pw_hist_ent *objp)
{
    if (!xdr_array(xdrs, (caddr_t *) &objp->key_data,
		   (u_int *) &objp->n_key_data, ~0,
		   sizeof(krb5_key_data),
		   xdr_krb5_key_data))
	return (FALSE);
    return (TRUE);
}

bool_t
xdr_osa_princ_ent_rec(XDR *xdrs, osa_princ_ent_t objp)
{
    switch (xdrs->x_op) {
    case XDR_ENCODE:
	 objp->version = OSA_ADB_PRINC_VERSION_1;
	 /* fall through */
    case XDR_FREE:
	 if (!xdr_int(xdrs, &objp->version))
	      return FALSE;
	 break;
    case XDR_DECODE:
	 if (!xdr_int(xdrs, &objp->version))
	      return FALSE;
	 if (objp->version != OSA_ADB_PRINC_VERSION_1)
	      return FALSE;
	 break;
    }

    if (!xdr_nullstring(xdrs, &objp->policy))
	return (FALSE);
    if (!xdr_long(xdrs, &objp->aux_attributes))
	return (FALSE);
    if (!xdr_u_int(xdrs, &objp->old_key_next))
	return (FALSE);
    if (!xdr_krb5_kvno(xdrs, &objp->admin_history_kvno))
	return (FALSE);
    if (!xdr_array(xdrs, (caddr_t *) &objp->old_keys,
		   (unsigned int *) &objp->old_key_len, ~0,
		   sizeof(osa_pw_hist_ent),
		   xdr_osa_pw_hist_ent))
	return (FALSE);
    return (TRUE);
}

void
osa_free_princ_ent(osa_princ_ent_t val)
{
    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);

    xdr_osa_princ_ent_rec(&xdrs, val);
    free(val);
}
