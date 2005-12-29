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
#include <errno.h>

#include "mech_switch.h"
#include "name.h"

OM_uint32 gss_duplicate_name(OM_uint32 *minor_status,
    const gss_name_t src_name,
    gss_name_t *dest_name)
{
	OM_uint32		major_status;
	struct _gss_name	*name = (struct _gss_name *) src_name;
	struct _gss_name	*new_name;
	struct _gss_mechanism_name *mn;

	*minor_status = 0;

	/*
	 * If this name has a value (i.e. it didn't come from
	 * gss_canonicalize_name(), we re-import the thing. Otherwise,
	 * we make an empty name to hold the MN copy.
	 */
	if (name->gn_value.value) {
		major_status = gss_import_name(minor_status,
		    &name->gn_value, &name->gn_type, dest_name);
		if (major_status != GSS_S_COMPLETE)
			return (major_status);
		new_name = (struct _gss_name *) *dest_name;
	} else {
		new_name = malloc(sizeof(struct _gss_name));
		if (!new_name) {
			*minor_status = ENOMEM;
			return (GSS_S_FAILURE);
		}
		memset(new_name, 0, sizeof(struct _gss_name));
		SLIST_INIT(&name->gn_mn);
		*dest_name = (gss_name_t) new_name;
	}

	/*
	 * Import the new name into any mechanisms listed in the
	 * original name. We could probably get away with only doing
	 * this if the original was canonical.
	 */
	SLIST_FOREACH(mn, &name->gn_mn, gmn_link) {
		_gss_find_mn(new_name, mn->gmn_mech_oid);
	}

	return (GSS_S_COMPLETE);
}
