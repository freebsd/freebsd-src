/* #pragma ident	"@(#)g_imp_name.c	1.26	04/02/23 SMI" */

/*
 * Copyright 1996 by Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Sun Microsystems not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. Sun Microsystems makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *  glue routine gss_import_name
 *
 */

#include "mglueP.h"
#include "k5-der.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

/* local function to import GSS_C_EXPORT_NAME names */
static OM_uint32 importExportName(OM_uint32 *, gss_union_name_t, gss_OID);

static OM_uint32
val_imp_name_args(
    OM_uint32 *minor_status,
    gss_buffer_t input_name_buffer,
    gss_OID input_name_type,
    gss_name_t *output_name)
{

    /* Initialize outputs. */

    if (minor_status != NULL)
	*minor_status = 0;

    if (output_name != NULL)
	*output_name = GSS_C_NO_NAME;

    /* Validate arguments. */

    if (minor_status == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (output_name == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    if (input_name_buffer == GSS_C_NO_BUFFER)
	return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

    if (input_name_type == GSS_C_NO_OID ||
	!g_OID_equal(input_name_type, GSS_C_NT_ANONYMOUS)) {
	if (input_name_buffer->length == 0)
	    return (GSS_S_BAD_NAME);

	if (input_name_buffer->value == NULL)
	    return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);
    }

    return (GSS_S_COMPLETE);
}

static gss_buffer_desc emptyNameBuffer;

OM_uint32 KRB5_CALLCONV
gss_import_name(minor_status,
                input_name_buffer,
                input_name_type,
                output_name)

OM_uint32 *		minor_status;
gss_buffer_t		input_name_buffer;
gss_OID			input_name_type;
gss_name_t *		output_name;

{
    gss_union_name_t	union_name;
    OM_uint32		tmp, major_status = GSS_S_FAILURE;

    if (input_name_buffer == GSS_C_NO_BUFFER)
	input_name_buffer = &emptyNameBuffer;

    major_status = val_imp_name_args(minor_status,
				     input_name_buffer, input_name_type,
				     output_name);
    if (major_status != GSS_S_COMPLETE)
	return (major_status);

    /*
     * First create the union name struct that will hold the external
     * name and the name type.
     */
    union_name = (gss_union_name_t) malloc (sizeof(gss_union_name_desc));
    if (!union_name)
	return (GSS_S_FAILURE);

    union_name->loopback = 0;
    union_name->mech_type = 0;
    union_name->mech_name = 0;
    union_name->name_type = 0;
    union_name->external_name = 0;

    /*
     * All we do here is record the external name and name_type.
     * When the name is actually used, the underlying gss_import_name()
     * is called for the appropriate mechanism.  The exception to this
     * rule is when the name of GSS_C_NT_EXPORT_NAME type.  If that is
     * the case, then we make it MN in this call.
     */
    major_status = gssint_create_copy_buffer(input_name_buffer,
					    &union_name->external_name, 0);
    if (major_status != GSS_S_COMPLETE) {
	free(union_name);
	return (major_status);
    }

    if (input_name_type != GSS_C_NULL_OID) {
	major_status = generic_gss_copy_oid(minor_status,
					    input_name_type,
					    &union_name->name_type);
	if (major_status != GSS_S_COMPLETE) {
	    map_errcode(minor_status);
	    goto allocation_failure;
	}
    }

    /*
     * In MIT Distribution the mechanism is determined from the nametype;
     * This is not a good idea - first mechanism that supports a given
     * name type is picked up; later on the caller can request a
     * different mechanism. So we don't determine the mechanism here. Now
     * the user level and kernel level import_name routine looks similar
     * except the kernel routine makes a copy of the nametype structure. We
     * do however make this an MN for names of GSS_C_NT_EXPORT_NAME type.
     */
    if (input_name_type != GSS_C_NULL_OID &&
	(g_OID_equal(input_name_type, GSS_C_NT_EXPORT_NAME) ||
	 g_OID_equal(input_name_type, GSS_C_NT_COMPOSITE_EXPORT))) {
	major_status = importExportName(minor_status, union_name, input_name_type);
	if (major_status != GSS_S_COMPLETE)
	    goto allocation_failure;
    }

    union_name->loopback = union_name;
    *output_name = (gss_name_t)union_name;
    return (GSS_S_COMPLETE);

allocation_failure:
    if (union_name) {
	if (union_name->external_name) {
	    if (union_name->external_name->value)
		free(union_name->external_name->value);
	    free(union_name->external_name);
	}
	if (union_name->name_type)
	    generic_gss_release_oid(&tmp, &union_name->name_type);
	if (union_name->mech_name)
	    gssint_release_internal_name(minor_status, union_name->mech_type,
					&union_name->mech_name);
	if (union_name->mech_type)
	    generic_gss_release_oid(&tmp, &union_name->mech_type);
	free(union_name);
    }
    return (major_status);
}

static OM_uint32
importExportName(minor, unionName, inputNameType)
    OM_uint32 *minor;
    gss_union_name_t unionName;
    gss_OID inputNameType;
{
    gss_OID_desc mechOid;
    gss_buffer_desc expName;
    gss_mechanism mech;
    OM_uint32 major, mechOidLen, nameLen;
    uint8_t b2;
    const uint8_t *name;
    struct k5input in, oid, old_format;

    expName.value = unionName->external_name->value;
    expName.length = unionName->external_name->length;
    k5_input_init(&in, expName.value, expName.length);

    if (k5_input_get_byte(&in) != 0x04)
	return (GSS_S_DEFECTIVE_TOKEN);
    b2 = k5_input_get_byte(&in);
    if (b2 != 0x01 && b2 != 0x02) /* allow composite names */
	return (GSS_S_DEFECTIVE_TOKEN);

    mechOidLen = k5_input_get_uint16_be(&in);

    if (!k5_der_get_value(&in, 0x06, &oid))
	return (GSS_S_DEFECTIVE_TOKEN);
    /* Verify that mechOidLen is consistent with the DER OID length. */
    if (mechOidLen != k5_der_value_len(oid.len))
	return (GSS_S_DEFECTIVE_TOKEN);
    mechOid.length = oid.len;
    mechOid.elements = (uint8_t *)oid.ptr;
    if ((mech = gssint_get_mechanism(&mechOid)) == NULL)
	return (GSS_S_BAD_MECH);

    if (mech->gssspi_import_name_by_mech == NULL &&
	mech->gss_import_name == NULL)
	return (GSS_S_UNAVAILABLE);

    /*
     * we must now determine if we should unwrap the name ourselves
     * or make the mechanism do it - we should only unwrap it
     * if we create it; so if mech->gss_export_name == NULL, we must
     * have created it.
     */
    if (mech->gss_export_name) {
	if (mech->gssspi_import_name_by_mech) {
	    major = mech->gssspi_import_name_by_mech(minor, &mechOid, &expName,
						     inputNameType,
						     &unionName->mech_name);
	} else {
	    major = mech->gss_import_name(minor, &expName, inputNameType,
					  &unionName->mech_name);
	}
	if (major != GSS_S_COMPLETE)
	    map_error(minor, mech);
	else {
	    major = generic_gss_copy_oid(minor, &mechOid,
					 &unionName->mech_type);
	    if (major != GSS_S_COMPLETE)
		map_errcode(minor);
	}
	return (major);
    }
    /*
     * we must have exported the name - so we now need to reconstruct it
     * and call the mechanism to create it
     *
     * WARNING:	Older versions of gssint_export_internal_name() did
     *		not export names correctly, but now it does.  In
     *		order to stay compatible with existing exported
     *		names we must support names exported the broken
     *		way.
     *
     * Specifically, gssint_export_internal_name() used to include
     * the name type OID in the encoding of the exported MN.
     * Additionally, the Kerberos V mech used to make display names
     * that included a null terminator which was counted in the
     * display name gss_buffer_desc.
     */

    /* next 4 bytes in the name are the name length */
    nameLen = k5_input_get_uint32_be(&in);
    name = k5_input_get_bytes(&in, nameLen);
    if (name == NULL)
	return (GSS_S_DEFECTIVE_TOKEN);

    /*
     * We detect broken exported names here: they always start with
     * a two-octet network-byte order OID length, which is always
     * less than 256 bytes, so the first octet of the length is
     * always '\0', which is not allowed in GSS-API display names
     * (or never occurs in them anyways).  Of course, the OID
     * shouldn't be there, but it is.  After the OID (sans DER tag
     * and length) there's the name itself, though null-terminated;
     * this null terminator should also not be there, but it is.
     */
    if (nameLen > 0 && *name == '\0') {
	OM_uint32 nameTypeLen;

	/* Skip the name type. */
	k5_input_init(&old_format, name, nameLen);
	nameTypeLen = k5_input_get_uint16_be(&old_format);
	if (k5_input_get_bytes(&old_format, nameTypeLen) == NULL)
	    return (GSS_S_DEFECTIVE_TOKEN);
	/* Remove a null terminator if one is present. */
	if (old_format.len > 0 && old_format.ptr[old_format.len - 1] == 0)
	    old_format.len--;
	name = old_format.ptr;
	nameLen = old_format.len;
    }

    /*
     * Can a name be null?  Let the mech decide.
     *
     * NOTE: We use GSS_C_NULL_OID as the name type when importing
     *	 the unwrapped name.  Presumably the exported name had,
     *	 prior to being exported been obtained in such a way
     *	 that it has been properly perpared ("canonicalized," in
     *	 GSS-API terms) according to some name type; we cannot
     *	 tell what that name type was now, but the name should
     *	 need no further preparation other than the lowest
     *	 common denominator afforded by the mech to names
     *	 imported with GSS_C_NULL_OID.  For the Kerberos V mech
     *	 this means doing less busywork too (particularly once
     *	 IDN is thrown in with Kerberos V extensions).
     */
    expName.length = nameLen;
    expName.value = nameLen ? (uint8_t *)name : NULL;
    if (mech->gssspi_import_name_by_mech) {
	major = mech->gssspi_import_name_by_mech(minor, &mechOid, &expName,
						 GSS_C_NULL_OID,
						 &unionName->mech_name);
    } else {
	major = mech->gss_import_name(minor, &expName,
				      GSS_C_NULL_OID, &unionName->mech_name);
    }
    if (major != GSS_S_COMPLETE) {
	map_error(minor, mech);
	return (major);
    }

    major = generic_gss_copy_oid(minor, &mechOid, &unionName->mech_type);
    if (major != GSS_S_COMPLETE) {
	map_errcode(minor);
    }
    return major;
} /* importExportName */
