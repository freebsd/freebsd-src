/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <popper.h>
#include <base64.h>
#include <pop_auth.h>
RCSID("$Id$");


#if defined(SASL) && defined(KRB5)
#include <gssapi.h>

extern krb5_context gssapi_krb5_context;

struct gss_state {
    gss_ctx_id_t context_hdl;
    gss_OID mech_oid;
    gss_name_t client_name;
    int stage;
};

static void
gss_set_error (struct gss_state *gs, int min_stat)
{
    OM_uint32 new_stat;
    OM_uint32 msg_ctx = 0;
    gss_buffer_desc status_string;
    OM_uint32 ret;

    do {
	char * cstr;

	ret = gss_display_status (&new_stat,
				  min_stat,
				  GSS_C_MECH_CODE,
				  gs->mech_oid,
				  &msg_ctx,
				  &status_string);
	if (asprintf(&cstr, "%.*s", (int)status_string.length,
		 (const char *)status_string.value) >= 0) {
	    pop_auth_set_error(cstr);
	    free(cstr);
	} else {
	    pop_auth_set_error("unknown error");
	}
	gss_release_buffer (&new_stat, &status_string);
    } while (!GSS_ERROR(ret) && msg_ctx != 0);
}

static int
gss_loop(POP *p, void *state,
	 /* const */ void *input, size_t input_length,
	 void **output, size_t *output_length)
{
    struct gss_state *gs = state;
    gss_buffer_desc real_input_token, real_output_token;
    gss_buffer_t input_token = &real_input_token,
	output_token = &real_output_token;
    OM_uint32 maj_stat, min_stat;
    gss_channel_bindings_t bindings = GSS_C_NO_CHANNEL_BINDINGS;

    if(gs->stage == 0) {
	/* we require an initial response, so ask for one if not
           present */
	gs->stage++;
	if(input == NULL && input_length == 0) {
	    /* XXX this could be done better */
	    fputs("+ \r\n", p->output);
	    fflush(p->output);
	    return POP_AUTH_CONTINUE;
	}
    }
    if(gs->stage == 1) {
	input_token->value = input;
	input_token->length = input_length;
	maj_stat =
	    gss_accept_sec_context (&min_stat,
				    &gs->context_hdl,
				    GSS_C_NO_CREDENTIAL,
				    input_token,
				    bindings,
				    &gs->client_name,
				    &gs->mech_oid,
				    output_token,
				    NULL,
				    NULL,
				    NULL);
	if (GSS_ERROR(maj_stat)) {
	    gss_set_error(gs, min_stat);
	    return POP_AUTH_FAILURE;
	}
	if (output_token->length != 0) {
	    *output = output_token->value;
	    *output_length = output_token->length;
	}
	if(maj_stat == GSS_S_COMPLETE)
	    gs->stage++;

	return POP_AUTH_CONTINUE;
    }

    if(gs->stage == 2) {
	/* send wanted protection levels */
	unsigned char x[4] = { 1, 0, 0, 0 };

	input_token->value = x;
	input_token->length = 4;

	maj_stat = gss_wrap(&min_stat,
			    gs->context_hdl,
			    FALSE,
			    GSS_C_QOP_DEFAULT,
			    input_token,
			    NULL,
			    output_token);
	if (GSS_ERROR(maj_stat)) {
	    gss_set_error(gs, min_stat);
	    return POP_AUTH_FAILURE;
	}
	*output = output_token->value;
	*output_length = output_token->length;
	gs->stage++;
	return POP_AUTH_CONTINUE;
    }
    if(gs->stage == 3) {
	/* receive protection levels and username */
	char *name;
	krb5_principal principal;
	gss_buffer_desc export_name;
	gss_OID oid;
	unsigned char *ptr;

	input_token->value = input;
	input_token->length = input_length;

	maj_stat = gss_unwrap (&min_stat,
			       gs->context_hdl,
			       input_token,
			       output_token,
			       NULL,
			       NULL);
	if (GSS_ERROR(maj_stat)) {
	    gss_set_error(gs, min_stat);
	    return POP_AUTH_FAILURE;
	}
	if(output_token->length < 5) {
	    pop_auth_set_error("response too short");
	    return POP_AUTH_FAILURE;
	}
	ptr = output_token->value;
	if(ptr[0] != 1) {
	    pop_auth_set_error("must use clear text");
	    return POP_AUTH_FAILURE;
	}
	memmove(output_token->value, ptr + 4, output_token->length - 4);
	ptr[output_token->length - 4] = '\0';

	maj_stat = gss_display_name(&min_stat, gs->client_name,
				    &export_name, &oid);
	if(maj_stat != GSS_S_COMPLETE) {
	    gss_set_error(gs, min_stat);
	    return POP_AUTH_FAILURE;
	}
	/* XXX kerberos */
	if(oid != GSS_KRB5_NT_PRINCIPAL_NAME) {
	    pop_auth_set_error("unexpected gss name type");
	    gss_release_buffer(&min_stat, &export_name);
	    return POP_AUTH_FAILURE;
	}
	name = malloc(export_name.length + 1);
	if(name == NULL) {
	    pop_auth_set_error("out of memory");
	    gss_release_buffer(&min_stat, &export_name);
	    return POP_AUTH_FAILURE;
	}
	memcpy(name, export_name.value, export_name.length);
	name[export_name.length] = '\0';
	gss_release_buffer(&min_stat, &export_name);
	krb5_parse_name(gssapi_krb5_context, name, &principal);

	if(!krb5_kuserok(gssapi_krb5_context, principal, ptr)) {
	    pop_auth_set_error("Permission denied");
	    return POP_AUTH_FAILURE;
	}


	strlcpy(p->user, ptr, sizeof(p->user));
	return POP_AUTH_COMPLETE;
    }
    return POP_AUTH_FAILURE;
}


static int
gss_init(POP *p, void **state)
{
    struct gss_state *gs = malloc(sizeof(*gs));
    if(gs == NULL) {
	pop_auth_set_error("out of memory");
	return POP_AUTH_FAILURE;
    }
    gs->context_hdl = GSS_C_NO_CONTEXT;
    gs->stage = 0;
    *state = gs;
    return POP_AUTH_CONTINUE;
}

static int
gss_cleanup(POP *p, void *state)
{
    OM_uint32 min_stat;
    struct gss_state *gs = state;
    if(gs->context_hdl != GSS_C_NO_CONTEXT)
	gss_delete_sec_context(&min_stat, &gs->context_hdl, GSS_C_NO_BUFFER);
    free(state);
    return POP_AUTH_CONTINUE;
}

struct auth_mech gssapi_mech = {
    "GSSAPI", gss_init, gss_loop, gss_cleanup
};

#endif /* KRB5 */
