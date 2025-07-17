/* #pragma ident	"@(#)g_dsp_status.c	1.17	04/02/23 SMI" */

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
 *  glue routine gss_display_status
 *
 */

#include "mglueP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* local function */
static OM_uint32 displayMajor(OM_uint32, OM_uint32 *, gss_buffer_t);

OM_uint32 KRB5_CALLCONV
gss_display_status (minor_status,
                    status_value,
                    status_type,
                    req_mech_type,
                    message_context,
                    status_string)

OM_uint32 *		minor_status;
OM_uint32		status_value;
int			status_type;
gss_OID			req_mech_type;
OM_uint32 *		message_context;
gss_buffer_t		status_string;

{
    gss_OID		mech_type = (gss_OID) req_mech_type;
    gss_mechanism	mech;
    gss_OID_desc	m_oid = { 0, 0 };

    if (minor_status != NULL)
	*minor_status = 0;

    if (status_string != GSS_C_NO_BUFFER) {
	status_string->length = 0;
	status_string->value = NULL;
    }

    if (minor_status == NULL ||
	message_context == NULL ||
	status_string == GSS_C_NO_BUFFER)

	return (GSS_S_CALL_INACCESSIBLE_WRITE);

    /* we handle major status codes, and the mechs do the minor */
    if (status_type == GSS_C_GSS_CODE)
	return (displayMajor(status_value, message_context,
			     status_string));

    /*
     * must be the minor status - let mechs do the work
     * select the appropriate underlying mechanism routine and
     * call it.
     */

    /* In this version, we only handle status codes that have been
       mapped to a flat numbering space.  Look up the value we got
       passed.  If it's not found, complain.  */
    if (status_value == 0) {
	status_string->value = gssalloc_strdup("Unknown error");
	if (status_string->value == NULL) {
	    *minor_status = ENOMEM;
	    map_errcode(minor_status);
	    return GSS_S_FAILURE;
	}
	status_string->length = strlen(status_string->value);
	*message_context = 0;
	*minor_status = 0;
	return GSS_S_COMPLETE;
    }
    {
	int err;
	OM_uint32 m_status = 0, status;

	err = gssint_mecherrmap_get(status_value, &m_oid, &m_status);
	if (err) {
	    *minor_status = err;
	    map_errcode(minor_status);
	    return GSS_S_BAD_STATUS;
	}
	if (m_oid.length == 0) {
	    /* Magic flag for com_err values.  */
	    status = g_display_com_err_status(minor_status, m_status, status_string);
	    if (status != GSS_S_COMPLETE)
		map_errcode(minor_status);
	    return status;
	}
	mech_type = &m_oid;
	status_value = m_status;
    }

    mech = gssint_get_mechanism (mech_type);

    if (mech && mech->gss_display_status) {
	OM_uint32 r;

	r = mech->gss_display_status(minor_status,
				     status_value, status_type, mech_type,
				     message_context, status_string);
	/* How's this for weird?  If we get an error returning the
	   mechanism-specific error code, we save away the
	   mechanism-specific error code describing the error.  */
	if (r != GSS_S_COMPLETE)
	    map_error(minor_status, mech);
	return r;
    }

    if (!mech)
	return (GSS_S_BAD_MECH);

    return (GSS_S_UNAVAILABLE);
}

/*
 * function to map the major error codes
 * it uses case statements so that the strings could be wrapped by gettext
 * msgCtxt is interpreted as:
 *	0 - first call
 *	1 - routine error
 *	>= 2 - the supplementary error code bit shifted by 1
 */
static OM_uint32
displayMajor(status, msgCtxt, outStr)
OM_uint32 status;
OM_uint32 *msgCtxt;
gss_buffer_t outStr;
{
	OM_uint32 oneVal, mask = 0x1, currErr;
	char *errStr = NULL;
	int i, haveErr = 0;

	/* take care of the success value first */
	if (status == GSS_S_COMPLETE)
	    errStr = _("The routine completed successfully");
	else if (*msgCtxt == 0 && (oneVal = GSS_CALLING_ERROR(status))) {
		switch (oneVal) {
		case GSS_S_CALL_INACCESSIBLE_READ:
			errStr = _("A required input parameter could not be "
				   "read");
			break;

		case GSS_S_CALL_INACCESSIBLE_WRITE:
			errStr = _("A required output parameter could not be "
				   "written");
			break;

		case GSS_S_CALL_BAD_STRUCTURE:
			errStr = _("A parameter was malformed");
			break;

		default:
			errStr = _("An invalid status code was supplied");
			break;
		}

		/* we now need to determine new value of msgCtxt */
		if (GSS_ROUTINE_ERROR(status))
			*msgCtxt = 1;
		else if ((oneVal = GSS_SUPPLEMENTARY_INFO(status)) != 0)
			*msgCtxt = (OM_uint32)(oneVal << 1);
		else
			*msgCtxt = 0;

	} else if ((*msgCtxt == 0 || *msgCtxt == 1) &&
		(oneVal = GSS_ROUTINE_ERROR(status))) {
		switch (oneVal) {
		case GSS_S_BAD_MECH:
			errStr = _("An unsupported mechanism was requested");
			break;

		case GSS_S_BAD_NAME:
			errStr = _("An invalid name was supplied");
			break;

		case GSS_S_BAD_NAMETYPE:
			errStr = _("A supplied name was of an unsupported "
				   "type");
			break;

		case GSS_S_BAD_BINDINGS:
			errStr = _("Incorrect channel bindings were supplied");
			break;

		case GSS_S_BAD_SIG: /* same as GSS_S_BAD_MIC: */
			errStr = _("A token had an invalid Message Integrity "
				   "Check (MIC)");
			break;

		case GSS_S_NO_CRED:
			errStr = _("No credentials were supplied, or the "
				   "credentials were unavailable or "
				   "inaccessible");
			break;

		case GSS_S_NO_CONTEXT:
			errStr = _("No context has been established");
			break;

		case GSS_S_DEFECTIVE_TOKEN:
			errStr = _("Invalid token was supplied");
			break;

		case GSS_S_DEFECTIVE_CREDENTIAL:
			errStr = _("Invalid credential was supplied");
			break;

		case GSS_S_CREDENTIALS_EXPIRED:
			errStr = _("The referenced credential has expired");
			break;

		case GSS_S_CONTEXT_EXPIRED:
			errStr = _("The referenced context has expired");
			break;

		case GSS_S_FAILURE:
			errStr = _("Unspecified GSS failure.  Minor code "
				   "may provide more information");
			break;

		case GSS_S_BAD_QOP:
			errStr = _("The quality-of-protection (QOP) "
				   "requested could not be provided");
			break;

		case GSS_S_UNAUTHORIZED:
			errStr = _("The operation is forbidden by local "
				   "security policy");
			break;

		case GSS_S_UNAVAILABLE:
			errStr = _("The operation or option is not "
				   "available or unsupported");
			break;

		case GSS_S_DUPLICATE_ELEMENT:
			errStr = _("The requested credential element "
				   "already exists");
			break;

		case GSS_S_NAME_NOT_MN:
			errStr = _("The provided name was not mechanism "
				   "specific (MN)");
			break;

		case GSS_S_BAD_STATUS:
		default:
			errStr = _("An invalid status code was supplied");
		}

		/* we must determine if the caller should call us again */
		if ((oneVal = GSS_SUPPLEMENTARY_INFO(status)) != 0)
			*msgCtxt = (OM_uint32)(oneVal << 1);
		else
			*msgCtxt = 0;

	} else if ((*msgCtxt == 0 || *msgCtxt >= 2) &&
		(oneVal = GSS_SUPPLEMENTARY_INFO(status))) {
		/*
		 * if msgCtxt is not 0, then it should encode
		 * the supplementary error code we should be printing
		 */
		if (*msgCtxt >= 2)
			oneVal = (OM_uint32) (*msgCtxt) >> 1;
		else
			oneVal = GSS_SUPPLEMENTARY_INFO(status);

		/* we display the errors LSB first */
		for (i = 0; i < 16; i++) {
			if (oneVal & mask) {
				haveErr = 1;
				break;
			}
			mask <<= 1;
		}

		/* isolate the bit or if not found set to illegal value */
		if (haveErr)
			currErr = oneVal & mask;
		else
			currErr = 1 << 17; /* illegal value */

		switch (currErr) {
		case GSS_S_CONTINUE_NEEDED:
			errStr = _("The routine must be called again to "
				   "complete its function");
			break;

		case GSS_S_DUPLICATE_TOKEN:
			errStr = _("The token was a duplicate of an earlier "
				   "token");
			break;

		case GSS_S_OLD_TOKEN:
			errStr = _("The token's validity period has expired");
			break;

		case GSS_S_UNSEQ_TOKEN:
			errStr = _("A later token has already been processed");
			break;

		case GSS_S_GAP_TOKEN:
			errStr = _("An expected per-message token was not "
				   "received");
			break;

		default:
			errStr = _("An invalid status code was supplied");
		}

		/*
		 * we must check if there is any other supplementary errors
		 * if found, then turn off current bit, and store next value
		 * in msgCtxt shifted by 1 bit
		 */
		if (!haveErr)
			*msgCtxt = 0;
		else if (GSS_SUPPLEMENTARY_INFO(oneVal) ^ mask)
			*msgCtxt = (OM_uint32)
				((GSS_SUPPLEMENTARY_INFO(oneVal) ^ mask) << 1);
		else
			*msgCtxt = 0;
	}

	if (errStr == NULL)
		errStr = "An invalid status code was supplied";

	/* now copy the status code and return to caller */
	outStr->length = strlen(errStr);
	outStr->value = gssalloc_strdup(errStr);
	if (outStr->value == NULL) {
		outStr->length = 0;
		return (GSS_S_FAILURE);
	}

	return (GSS_S_COMPLETE);
} /* displayMajor */
