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
 * gss_release_oid has been moved to g_initialize, because it requires access
 * to the mechanism list.  All functions requiring direct access to the
 * mechanism list are now in g_initialize.c
 */

OM_uint32 KRB5_CALLCONV
gss_create_empty_oid_set(OM_uint32 *minor_status, gss_OID_set *oid_set)
{
    OM_uint32 status;

    if (minor_status != NULL)
	*minor_status = 0;
    if (oid_set != NULL)
	*oid_set = GSS_C_NO_OID_SET;
    if (minor_status == NULL || oid_set == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;
    status = generic_gss_create_empty_oid_set(minor_status, oid_set);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
}

OM_uint32 KRB5_CALLCONV
gss_add_oid_set_member(OM_uint32 *minor_status, gss_OID member_oid,
		       gss_OID_set *oid_set)
{
    OM_uint32 status;

    if (minor_status != NULL)
	*minor_status = 0;
    if (minor_status == NULL || oid_set == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;
    if (member_oid == GSS_C_NO_OID || member_oid->length == 0 ||
	member_oid->elements == NULL)
	return GSS_S_CALL_INACCESSIBLE_READ;
    status = generic_gss_add_oid_set_member(minor_status, member_oid, oid_set);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
}

OM_uint32 KRB5_CALLCONV
gss_test_oid_set_member(OM_uint32 *minor_status, gss_OID member,
			gss_OID_set set, int *present)
{
    if (minor_status != NULL)
	*minor_status = 0;
    if (present != NULL)
	*present = 0;
    if (minor_status == NULL || present == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;
    if (member == GSS_C_NO_OID || set == GSS_C_NO_OID_SET)
	return GSS_S_CALL_INACCESSIBLE_READ;
    return generic_gss_test_oid_set_member(minor_status, member, set, present);
}

OM_uint32 KRB5_CALLCONV
gss_oid_to_str(OM_uint32 *minor_status, gss_OID oid, gss_buffer_t oid_str)
{
    OM_uint32 status;

    if (minor_status != NULL)
	*minor_status = 0;
    if (oid_str != GSS_C_NO_BUFFER) {
	oid_str->length = 0;
	oid_str->value = NULL;
    }
    if (minor_status == NULL || oid_str == GSS_C_NO_BUFFER)
	return GSS_S_CALL_INACCESSIBLE_WRITE;
    if (oid == GSS_C_NO_OID || oid->length == 0 || oid->elements == NULL)
	return GSS_S_CALL_INACCESSIBLE_READ;
    status = generic_gss_oid_to_str(minor_status, oid, oid_str);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
}

OM_uint32 KRB5_CALLCONV
gss_str_to_oid(OM_uint32 *minor_status, gss_buffer_t oid_str, gss_OID *oid)
{
    OM_uint32 status;

    if (minor_status != NULL)
	*minor_status = 0;
    if (oid != NULL)
	*oid = GSS_C_NO_OID;
    if (minor_status == NULL || oid == NULL)
	return GSS_S_CALL_INACCESSIBLE_WRITE;
    if (GSS_EMPTY_BUFFER(oid_str))
	return GSS_S_CALL_INACCESSIBLE_READ;
    status = generic_gss_str_to_oid(minor_status, oid_str, oid);
    if (status != GSS_S_COMPLETE)
	map_errcode(minor_status);
    return status;
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
