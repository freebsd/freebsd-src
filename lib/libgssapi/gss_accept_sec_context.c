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
#include <string.h>
#include <errno.h>

#include "mech_switch.h"
#include "context.h"
#include "cred.h"
#include "name.h"

OM_uint32 gss_accept_sec_context(OM_uint32 *minor_status,
    gss_ctx_id_t *context_handle,
    const gss_cred_id_t acceptor_cred_handle,
    const gss_buffer_t input_token,
    const gss_channel_bindings_t input_chan_bindings,
    gss_name_t *src_name,
    gss_OID *mech_type,
    gss_buffer_t output_token,
    OM_uint32 *ret_flags,
    OM_uint32 *time_rec,
    gss_cred_id_t *delegated_cred_handle)
{
	OM_uint32 major_status, mech_ret_flags;
	struct _gss_mech_switch *m;
	struct _gss_context *ctx = (struct _gss_context *) *context_handle;
	struct _gss_cred *cred = (struct _gss_cred *) acceptor_cred_handle;
	struct _gss_mechanism_cred *mc;
	gss_cred_id_t acceptor_mc, delegated_mc;
	gss_name_t src_mn;
	int allocated_ctx;

	*minor_status = 0;
	if (src_name) *src_name = 0;
	if (mech_type) *mech_type = 0;
	if (ret_flags) *ret_flags = 0;
	if (time_rec) *time_rec = 0;
	if (delegated_cred_handle) *delegated_cred_handle = 0;
	output_token->length = 0;
	output_token->value = 0;

	/*
	 * If this is the first call (*context_handle is NULL), we must
	 * parse the input token to figure out the mechanism to use.
	 */
	if (*context_handle == GSS_C_NO_CONTEXT) {
		unsigned char *p = input_token->value;
		size_t len = input_token->length;
		size_t a, b;
		gss_OID_desc mech_oid;

		/*
		 * Token must start with [APPLICATION 0] SEQUENCE.
		 */
		if (len == 0 || *p != 0x60)
			return (GSS_S_DEFECTIVE_TOKEN);
		p++;
		len--;

		/*
		 * Decode the length and make sure it agrees with the
		 * token length.
		 */
		if (len == 0)
			return (GSS_S_DEFECTIVE_TOKEN);
		if ((*p & 0x80) == 0) {
			a = *p;
			p++;
			len--;
		} else {
			b = *p & 0x7f;
			p++;
			len--;
			if (len < b)
				return (GSS_S_DEFECTIVE_TOKEN);
			a = 0;
			while (b) {
				a = (a << 8) | *p;
				p++;
				len--;
				b--;
			}
		}
		if (a != len)
			return (GSS_S_DEFECTIVE_TOKEN);

		/*
		 * Decode the OID for the mechanism. Simplify life by
		 * assuming that the OID length is less than 128 bytes.
		 */
		if (len < 2 || *p != 0x06)
			return (GSS_S_DEFECTIVE_TOKEN);
		if ((p[1] & 0x80) || p[1] > (len - 2))
			return (GSS_S_DEFECTIVE_TOKEN);
		mech_oid.length = p[1];
		p += 2;
		len -= 2;
		mech_oid.elements = p;

		/*
		 * Now that we have a mechanism, we can find the
		 * implementation.
		 */
		ctx = malloc(sizeof(struct _gss_context));
		if (!ctx) {
			*minor_status = ENOMEM;
			return (GSS_S_DEFECTIVE_TOKEN);
		}
		memset(ctx, 0, sizeof(struct _gss_context));
		m = ctx->gc_mech = _gss_find_mech_switch(&mech_oid);
		if (!m) {
			free(ctx);
			return (GSS_S_BAD_MECH);
		}
		allocated_ctx = 1;
	} else {
		m = ctx->gc_mech;
		allocated_ctx = 0;
	}

	if (cred) {
		SLIST_FOREACH(mc, &cred->gc_mc, gmc_link)
			if (mc->gmc_mech == m)
				break;
		if (!mc)
			return (GSS_S_BAD_MECH);
		acceptor_mc = mc->gmc_cred;
	} else {
		acceptor_mc = GSS_C_NO_CREDENTIAL;
	}
	delegated_mc = GSS_C_NO_CREDENTIAL;
	
	major_status = m->gm_accept_sec_context(minor_status,
	    &ctx->gc_ctx,
	    acceptor_mc,
	    input_token,
	    input_chan_bindings,
	    &src_mn,
	    mech_type,
	    output_token,
	    &mech_ret_flags,
	    time_rec,
	    &delegated_mc);
	if (major_status != GSS_S_COMPLETE &&
	    major_status != GSS_S_CONTINUE_NEEDED)
		return (major_status);

	if (!src_name) {
		m->gm_release_name(minor_status, &src_mn);
	} else {
		/*
		 * Make a new name and mark it as an MN.
		 */
		struct _gss_name *name = _gss_make_name(m, src_mn);

		if (!name) {
			m->gm_release_name(minor_status, &src_mn);
			return (GSS_S_FAILURE);
		}
		*src_name = (gss_name_t) name;
	}

	if (mech_ret_flags & GSS_C_DELEG_FLAG) {
		if (!delegated_cred_handle) {
			m->gm_release_cred(minor_status, &delegated_mc);
			*ret_flags &= ~GSS_C_DELEG_FLAG;
		} else {
			struct _gss_cred *cred;
			struct _gss_mechanism_cred *mc;

			cred = malloc(sizeof(struct _gss_cred));
			if (!cred) {
				*minor_status = ENOMEM;
				return (GSS_S_FAILURE);
			}
			mc = malloc(sizeof(struct _gss_mechanism_cred));
			if (!mc) {
				free(cred);
				*minor_status = ENOMEM;
				return (GSS_S_FAILURE);
			}
			m->gm_inquire_cred(minor_status, delegated_mc,
			    0, 0, &cred->gc_usage, 0);
			mc->gmc_mech = m;
			mc->gmc_mech_oid = &m->gm_mech_oid;
			mc->gmc_cred = delegated_mc;
			SLIST_INSERT_HEAD(&cred->gc_mc, mc, gmc_link);

			*delegated_cred_handle = (gss_cred_id_t) cred;
		}
	}

	if (ret_flags)
		*ret_flags = mech_ret_flags;
	*context_handle = (gss_ctx_id_t) ctx;
	return (major_status);
}
