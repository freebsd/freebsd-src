/*-
 * Copyright (c) 2005 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include <gssapi/gssapi.h>
#include <string.h>

#include "mech_switch.h"

struct _gss_status_desc {
	OM_uint32	gs_status;
	const char*	gs_desc;
};

static struct _gss_status_desc _gss_status_descs[] = {
	GSS_S_BAD_MECH,		"An unsupported mechanism was requested",
	GSS_S_BAD_NAME,		"An invalid name was supplied",
	GSS_S_BAD_NAMETYPE,	"A supplied name was of an unsupported type",
	GSS_S_BAD_BINDINGS,	"Incorrect channel bindings were supplied",
	GSS_S_BAD_STATUS,	"An invalid status code was supplied",
	GSS_S_BAD_MIC,		"A token had an invalid MIC",
	GSS_S_NO_CRED,		"No credentials were supplied, or the "
				"credentials were unavailable or inaccessible",
	GSS_S_NO_CONTEXT,	"No context has been established",
	GSS_S_DEFECTIVE_TOKEN,	"A token was invalid",
	GSS_S_DEFECTIVE_CREDENTIAL, "A credential was invalid",
	GSS_S_CREDENTIALS_EXPIRED, "The referenced credentials have expired",
	GSS_S_CONTEXT_EXPIRED,	"The context has expired",
	GSS_S_FAILURE,		"Miscellaneous failure",
	GSS_S_BAD_QOP,		"The quality-of-protection requested could "
				"not be provided",
	GSS_S_UNAUTHORIZED,	"The operation is forbidden by local security "
				"policy",
	GSS_S_UNAVAILABLE,	"The operation or option is unavailable",
	GSS_S_DUPLICATE_ELEMENT, "The requested credential element already "
				"exists",
	GSS_S_NAME_NOT_MN,	"The provided name was not a mechanism name"
};
#define _gss_status_desc_count \
	sizeof(_gss_status_descs) / sizeof(_gss_status_descs[0])


OM_uint32
gss_display_status(OM_uint32 *minor_status,
    OM_uint32 status_value,
    int status_type,
    const gss_OID mech_type,
    OM_uint32 *message_content,
    gss_buffer_t status_string)
{
	OM_uint32 major_status;
	struct _gss_mech_switch *m;
	int i;
	const char *message;

	*minor_status = 0;
	switch (status_type) {
	case GSS_C_GSS_CODE:
		for (i = 0; i < _gss_status_desc_count; i++) {
			if (_gss_status_descs[i].gs_status == status_value) {
				message = _gss_status_descs[i].gs_desc;
				status_string->length = strlen(message);
				status_string->value = strdup(message);
				return (GSS_S_COMPLETE);
			}
		}

		/*
		 * Fall through to attempt to get some underlying
		 * implementation to describe the value.
		 */
	case GSS_C_MECH_CODE:
		SLIST_FOREACH(m, &_gss_mechs, gm_link) {
			if (mech_type &&
			    !_gss_oid_equal(&m->gm_mech_oid, mech_type))
				continue;
			major_status = m->gm_display_status(minor_status,
			    status_value, status_type, mech_type,
			    message_content, status_string);
			if (major_status == GSS_S_COMPLETE)
				return (GSS_S_COMPLETE);
		}
	}

	return (GSS_S_BAD_STATUS);
}
