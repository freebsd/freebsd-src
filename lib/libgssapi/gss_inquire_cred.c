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
#include <stdlib.h>
#include <errno.h>

#include "mech_switch.h"
#include "name.h"
#include "cred.h"

OM_uint32
gss_inquire_cred(OM_uint32 *minor_status,
    const gss_cred_id_t cred_handle,
    gss_name_t *name_ret,
    OM_uint32 *lifetime,
    gss_cred_usage_t *cred_usage,
    gss_OID_set *mechanisms)
{
	OM_uint32 major_status;
	struct _gss_mech_switch *m;
	struct _gss_cred *cred = (struct _gss_cred *) cred_handle;
	struct _gss_mechanism_cred *mc;
	struct _gss_name *name;
	struct _gss_mechanism_name *mn;
	OM_uint32 min_lifetime;

	*minor_status = 0;
	if (name_ret)
		*name_ret = 0;
	if (lifetime)
		*lifetime = 0;
	if (cred_usage)
		*cred_usage = 0;

	if (name_ret) {
		name = malloc(sizeof(struct _gss_name));
		if (!name) {
			*minor_status = ENOMEM;
			return (GSS_S_FAILURE);
		}
		memset(name, 0, sizeof(struct _gss_name));
		SLIST_INIT(&name->gn_mn);
	} else {
		name = 0;
	}

	if (mechanisms) {
		major_status = gss_create_empty_oid_set(minor_status,
		    mechanisms);
		if (major_status) {
			if (name) free(name);
			return (major_status);
		}
	}

	min_lifetime = GSS_C_INDEFINITE;
	if (cred) {
		SLIST_FOREACH(mc, &cred->gc_mc, gmc_link) {
			gss_name_t mc_name;
			OM_uint32 mc_lifetime;

			major_status = mc->gmc_mech->gm_inquire_cred(minor_status,
			    mc->gmc_cred, &mc_name, &mc_lifetime, NULL, NULL);
			if (major_status)
				continue;

			if (name) {
				mn = malloc(sizeof(struct _gss_mechanism_name));
				if (!mn) {
					mc->gmc_mech->gm_release_name(minor_status,
					    &mc_name);
					continue;
				}
				mn->gmn_mech = mc->gmc_mech;
				mn->gmn_mech_oid = mc->gmc_mech_oid;
				mn->gmn_name = mc_name;
				SLIST_INSERT_HEAD(&name->gn_mn, mn, gmn_link);
			} else {
				mc->gmc_mech->gm_release_name(minor_status,
				    &mc_name);
			}

			if (mc_lifetime < min_lifetime)
				min_lifetime = mc_lifetime;

			if (mechanisms)
				gss_add_oid_set_member(minor_status,
				    mc->gmc_mech_oid, mechanisms);
		}
	} else {
		SLIST_FOREACH(m, &_gss_mechs, gm_link) {
			gss_name_t mc_name;
			OM_uint32 mc_lifetime;

			major_status = m->gm_inquire_cred(minor_status,
			    GSS_C_NO_CREDENTIAL, &mc_name, &mc_lifetime,
			    cred_usage, NULL);
			if (major_status)
				continue;

			if (name && mc_name) {
				mn = malloc(
					sizeof(struct _gss_mechanism_name));
				if (!mn) {
					mc->gmc_mech->gm_release_name(
						minor_status, &mc_name);
					continue;
				}
				mn->gmn_mech = mc->gmc_mech;
				mn->gmn_mech_oid = mc->gmc_mech_oid;
				mn->gmn_name = mc_name;
				SLIST_INSERT_HEAD(&name->gn_mn, mn, gmn_link);
			} else if (mc_name) {
				mc->gmc_mech->gm_release_name(minor_status,
				    &mc_name);
			}

			if (mc_lifetime < min_lifetime)
				min_lifetime = mc_lifetime;

			if (mechanisms)
				gss_add_oid_set_member(minor_status,
				    &m->gm_mech_oid, mechanisms);
		}

		if ((*mechanisms)->count == 0) {
			gss_release_oid_set(minor_status, mechanisms);
			*minor_status = 0;
			return (GSS_S_NO_CRED);
		}
	}

	*minor_status = 0;
	if (name_ret)
		*name_ret = (gss_name_t) name;
	if (lifetime)
		*lifetime = min_lifetime;
	if (cred && cred_usage)
		*cred_usage = cred->gc_usage;
	return (GSS_S_COMPLETE);
}
