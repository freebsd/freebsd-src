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

/*
 * get_der_length: Givin a pointer to a buffer that contains a DER encoded
 * length, decode the length updating the buffer to point to the character
 * after the DER encoding. The parameter bytes will point to the number of
 * bytes that made up the DER encoding of the length originally pointed to
 * by the buffer. Note we return -1 on error.
 */
int
gssint_get_der_length(unsigned char **buf, unsigned int buf_len, unsigned int *bytes)
{
    /* p points to the beginning of the buffer */
    unsigned char *p = *buf;
    int length, new_length;
    unsigned int octets;

    if (buf_len < 1)
	return (-1);

    /* We should have at least one byte */
    *bytes = 1;

    /*
     * If the High order bit is not set then the length is just the value
     * of *p.
     */
    if (*p < 128) {
	*buf = p+1;	/* Advance the buffer */
	return (*p);		/* return the length */
    }

    /*
     * if the High order bit is set, then the low order bits represent
     * the number of bytes that contain the DER encoding of the length.
     */

    octets = *p++ & 0x7f;
    *bytes += octets;

    /* See if the supplied buffer contains enough bytes for the length. */
    if (octets > buf_len - 1)
	return (-1);

    /*
     * Calculate a multibyte length. The length is encoded as an
     * unsigned integer base 256.
     */
    for (length = 0; octets; octets--) {
	new_length = (length << 8) + *p++;
	if (new_length < length)  /* overflow */
	    return (-1);
	length = new_length;
    }

    *buf = p; /* Advance the buffer */

    return (length);
}

/*
 * der_length_size: Return the number of bytes to encode a given length.
 */
unsigned int
gssint_der_length_size(unsigned int len)
{
    int i;

    if (len < 128)
	return (1);

    for (i = 0; len; i++) {
	len >>= 8;
    }

    return (i+1);
}

/*
 * put_der_length: Encode the supplied length into the buffer pointed to
 * by buf. max_length represents the maximum length of the buffer pointed
 * to by buff. We will advance buf to point to the character after the newly
 * DER encoded length. We return 0 on success or -l it the length cannot
 * be encoded in max_len characters.
 */
int
gssint_put_der_length(unsigned int length, unsigned char **buf, unsigned int max_len)
{
    unsigned char *s, *p;
    unsigned int buf_len = 0;
    int i, first;

    /* Oops */
    if (buf == 0 || max_len < 1)
	return (-1);

    s = *buf;

    /* Single byte is the length */
    if (length < 128) {
	*s++ = length;
	*buf = s;
	return (0);
    }

    /* First byte contains the number of octets */
    p = s + 1;

    /* Running total of the DER encoding length */
    buf_len = 0;

    /*
     * Encode MSB first. We do the encoding by setting a shift
     * factor to MSO_BIT (24 for 32 bit words) and then shifting the length
     * by the factor. We then encode the resulting low order byte.
     * We subtract 8 from the shift factor and repeat to ecnode the next
     * byte. We stop when the shift factor is zero or we've run out of
     * buffer to encode into.
     */
    first = 0;
    for (i = MSO_BIT; i >= 0 && buf_len <= max_len; i -= 8) {
	unsigned int v;
	v = (length >> i) & 0xff;
	if ((v) || first) {
	    buf_len += 1;
	    *p++ = v;
	    first = 1;
	}
    }
    if (i >= 0)			/* buffer overflow */
	return (-1);

    /*
     * We go back now and set the first byte to be the length with
     * the high order bit set.
     */
    *s = buf_len | 0x80;
    *buf = p;

    return (0);
}


/*
 *  glue routine for get_mech_type
 *
 */

OM_uint32 gssint_get_mech_type_oid(OID, token)
    gss_OID		OID;
    gss_buffer_t	token;
{
    unsigned char * buffer_ptr;
    size_t buflen, lenbytes, length, oidlen;

    /*
     * This routine reads the prefix of "token" in order to determine
     * its mechanism type. It assumes the encoding suggested in
     * Appendix B of RFC 1508. This format starts out as follows :
     *
     * tag for APPLICATION 0, Sequence[constructed, definite length]
     * length of remainder of token
     * tag of OBJECT IDENTIFIER
     * length of mechanism OID
     * encoding of mechanism OID
     * <the rest of the token>
     *
     * Numerically, this looks like :
     *
     * 0x60
     * <length> - could be multiple bytes
     * 0x06
     * <length> - assume only one byte, hence OID length < 127
     * <mech OID bytes>
     *
     * The routine fills in the OID value and returns an error as necessary.
     */

	if (OID == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	if ((token == NULL) || (token->value == NULL))
	return (GSS_S_DEFECTIVE_TOKEN);

    /* Skip past the APP/Sequnce byte and the token length */

    buffer_ptr = (unsigned char *) token->value;
    buflen = token->length;

    if (buflen < 2 || *buffer_ptr++ != 0x60)
	return (GSS_S_DEFECTIVE_TOKEN);
    length = *buffer_ptr++;
    buflen -= 2;

	/* check if token length is null */
	if (length == 0)
	    return (GSS_S_DEFECTIVE_TOKEN);

    if (length & 0x80) {
	lenbytes = length & 0x7f;
	if (lenbytes > 4 || lenbytes > buflen)
	    return (GSS_S_DEFECTIVE_TOKEN);
	buffer_ptr += lenbytes;
	buflen -= lenbytes;
    }

    if (buflen < 2 || *buffer_ptr++ != 0x06)
	return (GSS_S_DEFECTIVE_TOKEN);
    oidlen = *buffer_ptr++;
    buflen -= 2;
    if (oidlen > 0x7f || oidlen > buflen)
	return (GSS_S_DEFECTIVE_TOKEN);

    OID->length = oidlen;
    OID->elements = (void *) buffer_ptr;
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
    unsigned char *buf = NULL;
    const unsigned char tokId[] = "\x04\x01";
    const unsigned int tokIdLen = 2;
    const int mechOidLenLen = 2, mechOidTagLen = 1, nameLenLen = 4;
    int mechOidDERLen = 0;
    int mechOidLen = 0;

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

    /* determine the size of the buffer needed */
    mechOidDERLen = gssint_der_length_size(mech_type->length);
    name_buf->length = tokIdLen + mechOidLenLen +
	mechOidTagLen + mechOidDERLen +
	mech_type->length +
	nameLenLen + dispName.length;
    if ((name_buf->value = (void*)gssalloc_malloc(name_buf->length)) ==
	(void*)NULL) {
	name_buf->length = 0;
	(void) gss_release_buffer(&status, &dispName);
	return (GSS_S_FAILURE);
    }

    /* now create the name ..... */
    buf = (unsigned char *)name_buf->value;
    (void) memset(name_buf->value, 0, name_buf->length);
    (void) memcpy(buf, tokId, tokIdLen);
    buf += tokIdLen;

    /* spec allows only 2 bytes for the mech oid length */
    mechOidLen = mechOidDERLen + mechOidTagLen + mech_type->length;
    store_16_be(mechOidLen, buf);
    buf += 2;

    /*
     * DER Encoding of mech OID contains OID Tag (0x06), length and
     * mech OID value
     */
    *buf++ = 0x06;
    if (gssint_put_der_length(mech_type->length, &buf,
		       (name_buf->length - tokIdLen -2)) != 0) {
	name_buf->length = 0;
	free(name_buf->value);
	(void) gss_release_buffer(&status, &dispName);
	return (GSS_S_FAILURE);
    }

    (void) memcpy(buf, mech_type->elements, mech_type->length);
    buf += mech_type->length;

    /* spec designates the next 4 bytes for the name length */
    store_32_be(dispName.length, buf);
    buf += 4;

    /* for the final ingredient - add the name from gss_display_name */
    (void) memcpy(buf, dispName.value, dispName.length);

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
