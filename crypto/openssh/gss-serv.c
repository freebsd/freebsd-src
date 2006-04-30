/*	$OpenBSD: gss-serv.c,v 1.8 2005/08/30 22:08:05 djm Exp $	*/

/*
 * Copyright (c) 2001-2003 Simon Wilkinson. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#ifdef GSSAPI

#include "bufaux.h"
#include "compat.h"
#include "auth.h"
#include "log.h"
#include "channels.h"
#include "session.h"
#include "servconf.h"
#include "monitor_wrap.h"
#include "xmalloc.h"
#include "getput.h"

#include "ssh-gss.h"

extern ServerOptions options;

static ssh_gssapi_client gssapi_client =
    { GSS_C_EMPTY_BUFFER, GSS_C_EMPTY_BUFFER,
    GSS_C_NO_CREDENTIAL, NULL, {NULL, NULL, NULL}};

ssh_gssapi_mech gssapi_null_mech =
    { NULL, NULL, {0, NULL}, NULL, NULL, NULL, NULL};

#ifdef KRB5
extern ssh_gssapi_mech gssapi_kerberos_mech;
#endif

ssh_gssapi_mech* supported_mechs[]= {
#ifdef KRB5
	&gssapi_kerberos_mech,
#endif
	&gssapi_null_mech,
};

/* Unpriviledged */
void
ssh_gssapi_supported_oids(gss_OID_set *oidset)
{
	int i = 0;
	OM_uint32 min_status;
	int present;
	gss_OID_set supported;

	gss_create_empty_oid_set(&min_status, oidset);
	gss_indicate_mechs(&min_status, &supported);

	while (supported_mechs[i]->name != NULL) {
		if (GSS_ERROR(gss_test_oid_set_member(&min_status,
		    &supported_mechs[i]->oid, supported, &present)))
			present = 0;
		if (present)
			gss_add_oid_set_member(&min_status,
			    &supported_mechs[i]->oid, oidset);
		i++;
	}
}


/* Wrapper around accept_sec_context
 * Requires that the context contains:
 *    oid
 *    credentials	(from ssh_gssapi_acquire_cred)
 */
/* Priviledged */
OM_uint32
ssh_gssapi_accept_ctx(Gssctxt *ctx, gss_buffer_desc *recv_tok,
    gss_buffer_desc *send_tok, OM_uint32 *flags)
{
	OM_uint32 status;
	gss_OID mech;

	ctx->major = gss_accept_sec_context(&ctx->minor,
	    &ctx->context, ctx->creds, recv_tok,
	    GSS_C_NO_CHANNEL_BINDINGS, &ctx->client, &mech,
	    send_tok, flags, NULL, &ctx->client_creds);

	if (GSS_ERROR(ctx->major))
		ssh_gssapi_error(ctx);

	if (ctx->client_creds)
		debug("Received some client credentials");
	else
		debug("Got no client credentials");

	status = ctx->major;

	/* Now, if we're complete and we have the right flags, then
	 * we flag the user as also having been authenticated
	 */

	if (((flags == NULL) || ((*flags & GSS_C_MUTUAL_FLAG) &&
	    (*flags & GSS_C_INTEG_FLAG))) && (ctx->major == GSS_S_COMPLETE)) {
		if (ssh_gssapi_getclient(ctx, &gssapi_client))
			fatal("Couldn't convert client name");
	}

	return (status);
}

/*
 * This parses an exported name, extracting the mechanism specific portion
 * to use for ACL checking. It verifies that the name belongs the mechanism
 * originally selected.
 */
static OM_uint32
ssh_gssapi_parse_ename(Gssctxt *ctx, gss_buffer_t ename, gss_buffer_t name)
{
	u_char *tok;
	OM_uint32 offset;
	OM_uint32 oidl;

	tok=ename->value;

	/*
	 * Check that ename is long enough for all of the fixed length
	 * header, and that the initial ID bytes are correct
	 */

	if (ename->length<6 || memcmp(tok,"\x04\x01", 2)!=0)
		return GSS_S_FAILURE;

	/*
	 * Extract the OID, and check it. Here GSSAPI breaks with tradition
	 * and does use the OID type and length bytes. To confuse things
	 * there are two lengths - the first including these, and the
	 * second without.
	 */

	oidl = GET_16BIT(tok+2); /* length including next two bytes */
	oidl = oidl-2; /* turn it into the _real_ length of the variable OID */

	/*
	 * Check the BER encoding for correct type and length, that the
	 * string is long enough and that the OID matches that in our context
	 */
	if (tok[4] != 0x06 || tok[5] != oidl ||
	    ename->length < oidl+6 ||
	    !ssh_gssapi_check_oid(ctx,tok+6,oidl))
		return GSS_S_FAILURE;

	offset = oidl+6;

	if (ename->length < offset+4)
		return GSS_S_FAILURE;

	name->length = GET_32BIT(tok+offset);
	offset += 4;

	if (ename->length < offset+name->length)
		return GSS_S_FAILURE;

	name->value = xmalloc(name->length+1);
	memcpy(name->value,tok+offset,name->length);
	((char *)name->value)[name->length] = 0;

	return GSS_S_COMPLETE;
}

/* Extract the client details from a given context. This can only reliably
 * be called once for a context */

/* Priviledged (called from accept_secure_ctx) */
OM_uint32
ssh_gssapi_getclient(Gssctxt *ctx, ssh_gssapi_client *client)
{
	int i = 0;

	gss_buffer_desc ename;

	client->mech = NULL;

	while (supported_mechs[i]->name != NULL) {
		if (supported_mechs[i]->oid.length == ctx->oid->length &&
		    (memcmp(supported_mechs[i]->oid.elements,
		    ctx->oid->elements, ctx->oid->length) == 0))
			client->mech = supported_mechs[i];
		i++;
	}

	if (client->mech == NULL)
		return GSS_S_FAILURE;

	if ((ctx->major = gss_display_name(&ctx->minor, ctx->client,
	    &client->displayname, NULL))) {
		ssh_gssapi_error(ctx);
		return (ctx->major);
	}

	if ((ctx->major = gss_export_name(&ctx->minor, ctx->client,
	    &ename))) {
		ssh_gssapi_error(ctx);
		return (ctx->major);
	}

	if ((ctx->major = ssh_gssapi_parse_ename(ctx,&ename,
	    &client->exportedname))) {
		return (ctx->major);
	}

	/* We can't copy this structure, so we just move the pointer to it */
	client->creds = ctx->client_creds;
	ctx->client_creds = GSS_C_NO_CREDENTIAL;
	return (ctx->major);
}

/* As user - called on fatal/exit */
void
ssh_gssapi_cleanup_creds(void)
{
	if (gssapi_client.store.filename != NULL) {
		/* Unlink probably isn't sufficient */
		debug("removing gssapi cred file\"%s\"", gssapi_client.store.filename);
		unlink(gssapi_client.store.filename);
	}
}

/* As user */
void
ssh_gssapi_storecreds(void)
{
	if (gssapi_client.mech && gssapi_client.mech->storecreds) {
		(*gssapi_client.mech->storecreds)(&gssapi_client);
	} else
		debug("ssh_gssapi_storecreds: Not a GSSAPI mechanism");
}

/* This allows GSSAPI methods to do things to the childs environment based
 * on the passed authentication process and credentials.
 */
/* As user */
void
ssh_gssapi_do_child(char ***envp, u_int *envsizep)
{

	if (gssapi_client.store.envvar != NULL &&
	    gssapi_client.store.envval != NULL) {

		debug("Setting %s to %s", gssapi_client.store.envvar,
		gssapi_client.store.envval);
		child_set_env(envp, envsizep, gssapi_client.store.envvar,
		    gssapi_client.store.envval);
	}
}

/* Priviledged */
int
ssh_gssapi_userok(char *user)
{
	OM_uint32 lmin;

	if (gssapi_client.exportedname.length == 0 ||
	    gssapi_client.exportedname.value == NULL) {
		debug("No suitable client data");
		return 0;
	}
	if (gssapi_client.mech && gssapi_client.mech->userok)
		if ((*gssapi_client.mech->userok)(&gssapi_client, user))
			return 1;
		else {
			/* Destroy delegated credentials if userok fails */
			gss_release_buffer(&lmin, &gssapi_client.displayname);
			gss_release_buffer(&lmin, &gssapi_client.exportedname);
			gss_release_cred(&lmin, &gssapi_client.creds);
			memset(&gssapi_client, 0, sizeof(ssh_gssapi_client));
			return 0;
		}
	else
		debug("ssh_gssapi_userok: Unknown GSSAPI mechanism");
	return (0);
}

/* Priviledged */
OM_uint32
ssh_gssapi_checkmic(Gssctxt *ctx, gss_buffer_t gssbuf, gss_buffer_t gssmic)
{
	ctx->major = gss_verify_mic(&ctx->minor, ctx->context,
	    gssbuf, gssmic, NULL);

	return (ctx->major);
}

#endif
