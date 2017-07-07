/* #pragma ident	"@(#)g_oid_ops.c	1.11	98/01/22 SMI" */
/* lib/gssapi/mechglue/g_oid_ops.c - GSSAPI V2 interfaces to manipulate OIDs */
/*
 * Copyright 1995, 2007 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "mglueP.h"

/*
 * gss_release_oid has been moved to g_initialize, becasue it requires access
 * to the mechanism list.  All functions requiring direct access to the
 * mechanism list are now in g_initialize.c
 */

OM_uint32 KRB5_CALLCONV
gss_create_empty_oid_set(minor_status, oid_set)
    OM_uint32	*minor_status;
    gss_OID_set	*oid_set;
{
    OM_uint32 status;
    status = generic_gss_create_empty_oid_set(minor_status, oid_set);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
}

OM_uint32 KRB5_CALLCONV
gss_add_oid_set_member(minor_status, member_oid, oid_set)
    OM_uint32	*minor_status;
    gss_OID	member_oid;
    gss_OID_set	*oid_set;
{
    OM_uint32 status;
    status = generic_gss_add_oid_set_member(minor_status, member_oid, oid_set);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
}

OM_uint32 KRB5_CALLCONV
gss_test_oid_set_member(minor_status, member, set, present)
    OM_uint32	*minor_status;
    gss_OID	member;
    gss_OID_set	set;
    int		*present;
{
    return generic_gss_test_oid_set_member(minor_status, member, set, present);
}

OM_uint32 KRB5_CALLCONV
gss_oid_to_str(minor_status, oid, oid_str)
    OM_uint32		*minor_status;
    gss_OID		oid;
    gss_buffer_t	oid_str;
{
    OM_uint32 status = generic_gss_oid_to_str(minor_status, oid, oid_str);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
}

OM_uint32 KRB5_CALLCONV
gss_str_to_oid(minor_status, oid_str, oid)
    OM_uint32		*minor_status;
    gss_buffer_t	oid_str;
    gss_OID		*oid;
{
    OM_uint32 status = generic_gss_str_to_oid(minor_status, oid_str, oid);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
}

OM_uint32
gssint_copy_oid_set(
    OM_uint32 *minor_status,
    const gss_OID_set_desc * const oidset,
    gss_OID_set *new_oidset)
{
    return generic_gss_copy_oid_set(minor_status, oidset, new_oidset);
}

int KRB5_CALLCONV
gss_oid_equal(
    gss_const_OID first_oid,
    gss_const_OID second_oid)
{
    /* GSS_C_NO_OID doesn't match itself, per draft-josefsson-gss-capsulate. */
    if (first_oid == GSS_C_NO_OID || second_oid == GSS_C_NO_OID)
	return 0;
    return g_OID_equal(first_oid, second_oid);
}
