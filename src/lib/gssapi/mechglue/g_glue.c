/* #pragma ident	"@(#)g_glue.c	1.25	04/02/23 SMI" */

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

#include "mglueP.h"
#include "k5-der.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

#define	MSO_BIT (8*(sizeof (int) - 1))  /* Most significant octet bit */

extern gss_mechanism *gssint_mechs_array;

/*
 * This file contains the support routines for the glue layer.
 */

/* Retrieve the mechanism OID from an RFC 2743 InitialContextToken.  Place
 * the result into *oid_out, aliasing memory from token. */
OM_uint32 gssint_get_mech_type_oid(gss_OID oid_out, gss_buffer_t token)
{
    struct k5input in;

    if (oid_out == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);
    if (token == NULL || token->value == NULL)
	return (GSS_S_DEFECTIVE_TOKEN);

    k5_input_init(&in, token->value, token->length);
    if (!k5_der_get_value(&in, 0x60, &in))
	return (GSS_S_DEFECTIVE_TOKEN);
    if (!k5_der_get_value(&in, 0x06, &in))
	return (GSS_S_DEFECTIVE_TOKEN);
    oid_out->length = in.len;
    oid_out->elements = (uint8_t *)in.ptr;
    return (GSS_S_COMPLETE);
}

/*
 * The following mechanisms do not always identify themselves
 * per the GSS-API specification, when interoperating with MS
 * peers. We include the OIDs here so we do not have to ilnk
 * with the mechanism.
 */
static gss_OID_desc gss_ntlm_mechanism_oid_desc =
	{10, (void *)"\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a"};
static gss_OID_desc gss_spnego_mechanism_oid_desc =
	{6, (void *)"\x2b\x06\x01\x05\x05\x02"};
static gss_OID_desc gss_krb5_mechanism_oid_desc =
	{9, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"};

#define NTLMSSP_SIGNATURE "NTLMSSP"

OM_uint32 gssint_get_mech_type(OID, token)
    gss_OID		OID;
    gss_buffer_t	token;
{
    /* Check for interoperability exceptions */
    if (token->length >= sizeof(NTLMSSP_SIGNATURE) &&
	memcmp(token->value, NTLMSSP_SIGNATURE,
	       sizeof(NTLMSSP_SIGNATURE)) == 0) {
	*OID = gss_ntlm_mechanism_oid_desc;
    } else if (token->length != 0 &&
	       ((char *)token->value)[0] == 0x6E) {
 	/* Could be a raw AP-REQ (check for APPLICATION tag) */
	*OID = gss_krb5_mechanism_oid_desc;
    } else if (token->length == 0) {
	*OID = gss_spnego_mechanism_oid_desc;
    } else {
	return gssint_get_mech_type_oid(OID, token);
    }

    return (GSS_S_COMPLETE);
}

static OM_uint32
import_internal_attributes(OM_uint32 *minor,
			   gss_mechanism dmech,
			   gss_union_name_t sname,
			   gss_name_t dname)
{
    OM_uint32 major, tmpMinor;
    gss_mechanism smech;
    gss_buffer_set_t attrs = GSS_C_NO_BUFFER_SET;
    size_t i;

    if (sname->mech_name == GSS_C_NO_NAME)
	return (GSS_S_UNAVAILABLE);

    smech = gssint_get_mechanism (sname->mech_type);
    if (smech == NULL)
	return (GSS_S_BAD_MECH);

    if (smech->gss_inquire_name == NULL ||
	smech->gss_get_name_attribute == NULL)
	return (GSS_S_UNAVAILABLE);

    if (dmech->gss_set_name_attribute == NULL)
	return (GSS_S_UNAVAILABLE);

    major = smech->gss_inquire_name(minor, sname->mech_name,
				    NULL, NULL, &attrs);
    if (GSS_ERROR(major) || attrs == GSS_C_NO_BUFFER_SET) {
	gss_release_buffer_set(&tmpMinor, &attrs);
	return (major);
    }

    for (i = 0; i < attrs->count; i++) {
	int more = -1;

	while (more != 0) {
	    gss_buffer_desc value, display_value;
	    int authenticated, complete;

	    major = smech->gss_get_name_attribute(minor, sname->mech_name,
						  &attrs->elements[i],
						  &authenticated, &complete,
						  &value, &display_value,
						  &more);
	    if (GSS_ERROR(major))
		continue;

	    if (authenticated) {
		dmech->gss_set_name_attribute(minor, dname, complete,
					      &attrs->elements[i], &value);
	    }

	    gss_release_buffer(&tmpMinor, &value);
	    gss_release_buffer(&tmpMinor, &display_value);
	}
    }

    gss_release_buffer_set(&tmpMinor, &attrs);

    return (GSS_S_COMPLETE);
}

/*
 *  Internal routines to get and release an internal mechanism name
 */

OM_uint32 gssint_import_internal_name (minor_status, mech_type, union_name,
				internal_name)
OM_uint32	*minor_status;
gss_OID		mech_type;
gss_union_name_t	union_name;
gss_name_t	*internal_name;
{
    OM_uint32		status, tmpMinor;
    gss_mechanism	mech;
    gss_OID		public_mech;

    mech = gssint_get_mechanism (mech_type);
    if (mech == NULL)
	return (GSS_S_BAD_MECH);

    /*
     * If we are importing a name for the same mechanism, and the
     * mechanism implements gss_duplicate_name, then use that.
     */
    if (union_name->mech_type != GSS_C_NO_OID &&
	union_name->mech_name != GSS_C_NO_NAME &&
	g_OID_equal(union_name->mech_type, mech_type) &&
	mech->gss_duplicate_name != NULL) {
	status = mech->gss_duplicate_name(minor_status,
					  union_name->mech_name,
					  internal_name);
	if (status != GSS_S_UNAVAILABLE) {
	    if (status != GSS_S_COMPLETE)
		map_error(minor_status, mech);
	    return (status);
	}
    }

    if (mech->gssspi_import_name_by_mech) {
	public_mech = gssint_get_public_oid(mech_type);
	status = mech->gssspi_import_name_by_mech(minor_status, public_mech,
						  union_name->external_name,
						  union_name->name_type,
						  internal_name);
    } else if (mech->gss_import_name) {
	status = mech->gss_import_name(minor_status, union_name->external_name,
				       union_name->name_type, internal_name);
    } else {
	return (GSS_S_UNAVAILABLE);
    }

    if (status == GSS_S_COMPLETE) {
        /* Attempt to round-trip attributes */
	(void) import_internal_attributes(&tmpMinor, mech,
				          union_name, *internal_name);
    } else {
	map_error(minor_status, mech);
    }

    return (status);
}

OM_uint32 gssint_export_internal_name(minor_status, mech_type,
				     internal_name, name_buf)
    OM_uint32		*minor_status;
    const gss_OID		mech_type;
    const gss_name_t	internal_name;
    gss_buffer_t		name_buf;
{
    OM_uint32 status;
    gss_mechanism mech;
    gss_buffer_desc dispName;
    gss_OID nameOid;
    int mech_der_len = 0;
    struct k5buf buf;

    mech = gssint_get_mechanism(mech_type);
    if (!mech)
	return (GSS_S_BAD_MECH);

    if (mech->gss_export_name) {
	status = mech->gss_export_name(minor_status,
				       internal_name,
				       name_buf);
	if (status != GSS_S_COMPLETE)
	    map_error(minor_status, mech);
	return status;
    }

    /*
     * if we are here it is because the mechanism does not provide
     * a gss_export_name so we will use our implementation.  We
     * do required that the mechanism define a gss_display_name.
     */
    if (!mech->gss_display_name)
	return (GSS_S_UNAVAILABLE);

    /*
     * NOTE: RFC2743 (section 3.2) governs the format of the outer
     *	 wrapper of exported names; the mechanisms' specs govern
     *	 the format of the inner portion of the exported name
     *	 and, for some (e.g., RFC1964, the Kerberos V mech), a
     *	 generic default as implemented here will do.
     *
     * The outer wrapper of an exported MN is: 2-octet tok Id
     * (0x0401) + 2-octet network-byte order mech OID length + mech
     * oid (in DER format, including DER tag and DER length) +
     * 4-octet network-byte order length of inner portion + inner
     * portion.
     *
     * For the Kerberos V mechanism the inner portion of an exported
     * MN is the display name string and ignores the name type OID
     * altogether.  And we hope this will be so for any future
     * mechanisms also, so that factoring name export/import out of
     * the mech and into libgss pays off.
     */
    if ((status = mech->gss_display_name(minor_status,
					 internal_name,
					 &dispName,
					 &nameOid))
	!= GSS_S_COMPLETE) {
	map_error(minor_status, mech);
	return (status);
    }

    /* Allocate space and prepare a buffer. */
    mech_der_len = k5_der_value_len(mech_type->length);
    name_buf->length = 2 + 2 + mech_der_len + 4 + dispName.length;
    name_buf->value = gssalloc_malloc(name_buf->length);
    if (name_buf->value == NULL) {
	name_buf->length = 0;
	(void) gss_release_buffer(&status, &dispName);
	return (GSS_S_FAILURE);
    }
    k5_buf_init_fixed(&buf, name_buf->value, name_buf->length);

    /* Assemble the name. */
    k5_buf_add_len(&buf, "\x04\x01", 2);
    k5_buf_add_uint16_be(&buf, mech_der_len);
    k5_der_add_value(&buf, 0x06, mech_type->elements, mech_type->length);
    k5_buf_add_uint32_be(&buf, dispName.length);
    k5_buf_add_len(&buf, dispName.value, dispName.length);
    assert(buf.len == name_buf->length);

    /* release the buffer obtained from gss_display_name */
    (void) gss_release_buffer(minor_status, &dispName);
    return (GSS_S_COMPLETE);
} /*  gssint_export_internal_name */

OM_uint32 gssint_display_internal_name (minor_status, mech_type, internal_name,
				 external_name, name_type)
OM_uint32	*minor_status;
gss_OID		mech_type;
gss_name_t	internal_name;
gss_buffer_t	external_name;
gss_OID		*name_type;
{
    OM_uint32		status;
    gss_mechanism	mech;

    mech = gssint_get_mechanism (mech_type);
    if (mech) {
	if (mech->gss_display_name) {
	    status = mech->gss_display_name (
					     minor_status,
					     internal_name,
					     external_name,
					     name_type);
	    if (status != GSS_S_COMPLETE)
		map_error(minor_status, mech);
	} else
	    status = GSS_S_UNAVAILABLE;

	return (status);
    }

    return (GSS_S_BAD_MECH);
}

OM_uint32 gssint_release_internal_name (minor_status, mech_type, internal_name)
OM_uint32	*minor_status;
gss_OID		mech_type;
gss_name_t	*internal_name;
{
    OM_uint32		status;
    gss_mechanism	mech;

    mech = gssint_get_mechanism (mech_type);
    if (mech) {
	if (mech->gss_release_name) {
	    status = mech->gss_release_name (
					     minor_status,
					     internal_name);
	    if (status != GSS_S_COMPLETE)
		map_error(minor_status, mech);
	} else
	    status = GSS_S_UNAVAILABLE;

	return (status);
    }

    return (GSS_S_BAD_MECH);
}

OM_uint32 gssint_delete_internal_sec_context (minor_status,
					      mech_type,
					      internal_ctx,
					      output_token)
OM_uint32	*minor_status;
gss_OID		mech_type;
gss_ctx_id_t	*internal_ctx;
gss_buffer_t	output_token;
{
    OM_uint32		status;
    gss_mechanism	mech;

    mech = gssint_get_mechanism (mech_type);
    if (mech) {
	if (mech->gss_delete_sec_context)
	    status = mech->gss_delete_sec_context (minor_status,
						   internal_ctx,
						   output_token);
	else
	    status = GSS_S_UNAVAILABLE;

	return (status);
    }

    return (GSS_S_BAD_MECH);
}

/*
 * This function converts an internal gssapi name to a union gssapi
 * name.  Note that internal_name should be considered "consumed" by
 * this call, whether or not we return an error.
 */
OM_uint32 gssint_convert_name_to_union_name(minor_status, mech,
					   internal_name, external_name)
    OM_uint32 *minor_status;
    gss_mechanism	mech;
    gss_name_t	internal_name;
    gss_name_t	*external_name;
{
    OM_uint32 major_status,tmp;
    gss_union_name_t union_name;

    union_name = (gss_union_name_t) malloc (sizeof(gss_union_name_desc));
    if (!union_name) {
	major_status = GSS_S_FAILURE;
	*minor_status = ENOMEM;
	map_errcode(minor_status);
	goto allocation_failure;
    }
    union_name->mech_type = 0;
    union_name->mech_name = internal_name;
    union_name->name_type = 0;
    union_name->external_name = 0;

    major_status = generic_gss_copy_oid(minor_status, &mech->mech_type,
					&union_name->mech_type);
    if (major_status != GSS_S_COMPLETE) {
	map_errcode(minor_status);
	goto allocation_failure;
    }

    union_name->external_name =
	(gss_buffer_t) malloc(sizeof(gss_buffer_desc));
    if (!union_name->external_name) {
	    major_status = GSS_S_FAILURE;
	    goto allocation_failure;
    }
    union_name->external_name->length = 0;
    union_name->external_name->value = NULL;

    major_status = mech->gss_display_name(minor_status,
					  internal_name,
					  union_name->external_name,
					  &union_name->name_type);
    if (major_status != GSS_S_COMPLETE) {
	map_error(minor_status, mech);
	goto allocation_failure;
    }

    union_name->loopback = union_name;
    *external_name = /*(gss_name_t) CHECK */union_name;
    return (GSS_S_COMPLETE);

allocation_failure:
    if (union_name) {
	if (union_name->external_name) {
	    if (union_name->external_name->value)
		free(union_name->external_name->value);
	    free(union_name->external_name);
	}
	if (union_name->name_type)
	    gss_release_oid(&tmp, &union_name->name_type);
	if (union_name->mech_type)
	    gss_release_oid(&tmp, &union_name->mech_type);
	free(union_name);
    }
    /*
     * do as the top comment says - since we are now owners of
     * internal_name, we must clean it up
     */
    if (internal_name)
	(void) gssint_release_internal_name(&tmp, &mech->mech_type,
					   &internal_name);
    return (major_status);
}

/*
 * Glue routine for returning the mechanism-specific credential from a
 * external union credential.
 */
gss_cred_id_t
gssint_get_mechanism_cred(union_cred, mech_type)
    gss_union_cred_t	union_cred;
    gss_OID		mech_type;
{
    int		i;

    if (union_cred == GSS_C_NO_CREDENTIAL)
	return GSS_C_NO_CREDENTIAL;

    for (i=0; i < union_cred->count; i++) {
	if (g_OID_equal(mech_type, &union_cred->mechs_array[i]))
	    return union_cred->cred_array[i];
    }
    return GSS_C_NO_CREDENTIAL;
}

/*
 * Routine to create and copy the gss_buffer_desc structure.
 * Both space for the structure and the data is allocated.
 */
OM_uint32
gssint_create_copy_buffer(srcBuf, destBuf, addNullChar)
    const gss_buffer_t	srcBuf;
    gss_buffer_t 		*destBuf;
    int			addNullChar;
{
    gss_buffer_t aBuf;
    unsigned int len;

    if (destBuf == NULL)
	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    *destBuf = 0;

    aBuf = (gss_buffer_t)malloc(sizeof (gss_buffer_desc));
    if (!aBuf)
	return (GSS_S_FAILURE);

    if (addNullChar)
	len = srcBuf->length + 1;
    else
	len = srcBuf->length;

    if (!(aBuf->value = (void*)gssalloc_malloc(len))) {
	free(aBuf);
	return (GSS_S_FAILURE);
    }


    (void) memcpy(aBuf->value, srcBuf->value, srcBuf->length);
    aBuf->length = srcBuf->length;
    *destBuf = aBuf;

    /* optionally add a NULL character */
    if (addNullChar)
	((char *)aBuf->value)[aBuf->length] = '\0';

    return (GSS_S_COMPLETE);
} /* ****** gssint_create_copy_buffer  ****** */

OM_uint32
gssint_create_union_context(OM_uint32 *minor, gss_const_OID mech_oid,
			    gss_union_ctx_id_t *ctx_out)
{
    OM_uint32 status;
    gss_union_ctx_id_t ctx;

    *ctx_out = NULL;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
	*minor = ENOMEM;
	return GSS_S_FAILURE;
    }

    status = generic_gss_copy_oid(minor, mech_oid, &ctx->mech_type);
    if (status != GSS_S_COMPLETE) {
	free(ctx);
	return status;
    }

    ctx->loopback = ctx;
    ctx->internal_ctx_id = GSS_C_NO_CONTEXT;

    *ctx_out = ctx;
    return GSS_S_COMPLETE;
}
