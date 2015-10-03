/*
 * Copyright (C) 2004-2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: gssapictx.c,v 1.29 2011/08/29 06:33:25 marka Exp $ */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/once.h>
#include <isc/print.h>
#include <isc/platform.h>
#include <isc/random.h>
#include <isc/string.h>
#include <isc/time.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/result.h>
#include <dns/types.h>
#include <dns/keyvalues.h>
#include <dns/log.h>

#include <dst/gssapi.h>
#include <dst/result.h>

#include "dst_internal.h"

/*
 * If we're using our own SPNEGO implementation (see configure.in),
 * pull it in now.  Otherwise, we just use whatever GSSAPI supplies.
 */
#if defined(GSSAPI) && defined(USE_ISC_SPNEGO)
#include "spnego.h"
#define	gss_accept_sec_context	gss_accept_sec_context_spnego
#define	gss_init_sec_context	gss_init_sec_context_spnego
#endif

/*
 * Solaris8 apparently needs an explicit OID set, and Solaris10 needs
 * one for anything but Kerberos.  Supplying an explicit OID set
 * doesn't appear to hurt anything in other implementations, so we
 * always use one.  If we're not using our own SPNEGO implementation,
 * we include SPNEGO's OID.
 */
#ifdef GSSAPI
#ifdef WIN32
#include <krb5/krb5.h>
#else
#include ISC_PLATFORM_KRB5HEADER
#endif

static unsigned char krb5_mech_oid_bytes[] = {
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x12, 0x01, 0x02, 0x02
};

#ifndef USE_ISC_SPNEGO
static unsigned char spnego_mech_oid_bytes[] = {
	0x2b, 0x06, 0x01, 0x05, 0x05, 0x02
};
#endif

static gss_OID_desc mech_oid_set_array[] = {
	{ sizeof(krb5_mech_oid_bytes), krb5_mech_oid_bytes },
#ifndef USE_ISC_SPNEGO
	{ sizeof(spnego_mech_oid_bytes), spnego_mech_oid_bytes },
#endif
};

static gss_OID_set_desc mech_oid_set = {
	sizeof(mech_oid_set_array) / sizeof(*mech_oid_set_array),
	mech_oid_set_array
};

#endif

#define REGION_TO_GBUFFER(r, gb) \
	do { \
		(gb).length = (r).length; \
		(gb).value = (r).base; \
	} while (0)

#define GBUFFER_TO_REGION(gb, r) \
	do { \
	  (r).length = (unsigned int)(gb).length; \
		(r).base = (gb).value; \
	} while (0)


#define RETERR(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto out; \
	} while (0)

#ifdef GSSAPI
static inline void
name_to_gbuffer(dns_name_t *name, isc_buffer_t *buffer,
		gss_buffer_desc *gbuffer)
{
	dns_name_t tname, *namep;
	isc_region_t r;
	isc_result_t result;

	if (!dns_name_isabsolute(name))
		namep = name;
	else
	{
		unsigned int labels;
		dns_name_init(&tname, NULL);
		labels = dns_name_countlabels(name);
		dns_name_getlabelsequence(name, 0, labels - 1, &tname);
		namep = &tname;
	}

	result = dns_name_toprincipal(namep, buffer);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	isc_buffer_putuint8(buffer, 0);
	isc_buffer_usedregion(buffer, &r);
	REGION_TO_GBUFFER(r, *gbuffer);
}

static void
log_cred(const gss_cred_id_t cred) {
	OM_uint32 gret, minor, lifetime;
	gss_name_t gname;
	gss_buffer_desc gbuffer;
	gss_cred_usage_t usage;
	const char *usage_text;
	char buf[1024];

	gret = gss_inquire_cred(&minor, cred, &gname, &lifetime, &usage, NULL);
	if (gret != GSS_S_COMPLETE) {
		gss_log(3, "failed gss_inquire_cred: %s",
			gss_error_tostring(gret, minor, buf, sizeof(buf)));
		return;
	}

	gret = gss_display_name(&minor, gname, &gbuffer, NULL);
	if (gret != GSS_S_COMPLETE)
		gss_log(3, "failed gss_display_name: %s",
			gss_error_tostring(gret, minor, buf, sizeof(buf)));
	else {
		switch (usage) {
		case GSS_C_BOTH:
			usage_text = "GSS_C_BOTH";
			break;
		case GSS_C_INITIATE:
			usage_text = "GSS_C_INITIATE";
			break;
		case GSS_C_ACCEPT:
			usage_text = "GSS_C_ACCEPT";
			break;
		default:
			usage_text = "???";
		}
		gss_log(3, "gss cred: \"%s\", %s, %lu", (char *)gbuffer.value,
			usage_text, (unsigned long)lifetime);
	}

	if (gret == GSS_S_COMPLETE) {
		if (gbuffer.length != 0U) {
			gret = gss_release_buffer(&minor, &gbuffer);
			if (gret != GSS_S_COMPLETE)
				gss_log(3, "failed gss_release_buffer: %s",
					gss_error_tostring(gret, minor, buf,
							   sizeof(buf)));
		}
	}

	gret = gss_release_name(&minor, &gname);
	if (gret != GSS_S_COMPLETE)
		gss_log(3, "failed gss_release_name: %s",
			gss_error_tostring(gret, minor, buf, sizeof(buf)));
}
#endif

#ifdef GSSAPI
/*
 * check for the most common configuration errors.
 *
 * The errors checked for are:
 *   - tkey-gssapi-credential doesn't start with DNS/
 *   - the default realm in /etc/krb5.conf and the
 *     tkey-gssapi-credential bind config option don't match
 *
 * Note that if tkey-gssapi-keytab is set then these configure checks
 * are not performed, and runtime errors from gssapi are used instead
 */
static void
check_config(const char *gss_name) {
	const char *p;
	krb5_context krb5_ctx;
	char *krb5_realm_name = NULL;

	if (strncasecmp(gss_name, "DNS/", 4) != 0) {
		gss_log(ISC_LOG_ERROR, "tkey-gssapi-credential (%s) "
			"should start with 'DNS/'", gss_name);
		return;
	}

	if (krb5_init_context(&krb5_ctx) != 0) {
		gss_log(ISC_LOG_ERROR, "Unable to initialise krb5 context");
		return;
	}
	if (krb5_get_default_realm(krb5_ctx, &krb5_realm_name) != 0) {
		gss_log(ISC_LOG_ERROR, "Unable to get krb5 default realm");
		krb5_free_context(krb5_ctx);
		return;
	}
	p = strchr(gss_name, '@');
	if (p == NULL) {
		gss_log(ISC_LOG_ERROR, "badly formatted "
			"tkey-gssapi-credentials (%s)", gss_name);
		krb5_free_context(krb5_ctx);
		return;
	}
	if (strcasecmp(p + 1, krb5_realm_name) != 0) {
		gss_log(ISC_LOG_ERROR, "default realm from krb5.conf (%s) "
			"does not match tkey-gssapi-credential (%s)",
			krb5_realm_name, gss_name);
		krb5_free_context(krb5_ctx);
		return;
	}
	krb5_free_context(krb5_ctx);
}
#endif

isc_result_t
dst_gssapi_acquirecred(dns_name_t *name, isc_boolean_t initiate,
		       gss_cred_id_t *cred)
{
#ifdef GSSAPI
	isc_result_t result;
	isc_buffer_t namebuf;
	gss_name_t gname;
	gss_buffer_desc gnamebuf;
	unsigned char array[DNS_NAME_MAXTEXT + 1];
	OM_uint32 gret, minor;
	OM_uint32 lifetime;
	gss_cred_usage_t usage;
	char buf[1024];

	REQUIRE(cred != NULL && *cred == NULL);

	/*
	 * XXXSRA In theory we could use GSS_C_NT_HOSTBASED_SERVICE
	 * here when we're in the acceptor role, which would let us
	 * default the hostname and use a compiled in default service
	 * name of "DNS", giving one less thing to configure in
	 * named.conf.	Unfortunately, this creates a circular
	 * dependency due to DNS-based realm lookup in at least one
	 * GSSAPI implementation (Heimdal).  Oh well.
	 */
	if (name != NULL) {
		isc_buffer_init(&namebuf, array, sizeof(array));
		name_to_gbuffer(name, &namebuf, &gnamebuf);
		gret = gss_import_name(&minor, &gnamebuf,
				       GSS_C_NO_OID, &gname);
		if (gret != GSS_S_COMPLETE) {
			check_config((char *)array);

			gss_log(3, "failed gss_import_name: %s",
				gss_error_tostring(gret, minor, buf,
						   sizeof(buf)));
			return (ISC_R_FAILURE);
		}
	} else
		gname = NULL;

	/* Get the credentials. */
	if (gname != NULL)
		gss_log(3, "acquiring credentials for %s",
			(char *)gnamebuf.value);
	else {
		/* XXXDCL does this even make any sense? */
		gss_log(3, "acquiring credentials for ?");
	}

	if (initiate)
		usage = GSS_C_INITIATE;
	else
		usage = GSS_C_ACCEPT;

	gret = gss_acquire_cred(&minor, gname, GSS_C_INDEFINITE,
				&mech_oid_set, usage, cred, NULL, &lifetime);

	if (gret != GSS_S_COMPLETE) {
		gss_log(3, "failed to acquire %s credentials for %s: %s",
			initiate ? "initiate" : "accept",
			(gname != NULL) ? (char *)gnamebuf.value : "?",
			gss_error_tostring(gret, minor, buf, sizeof(buf)));
		if (gname != NULL)
			check_config((char *)array);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	gss_log(4, "acquired %s credentials for %s",
		initiate ? "initiate" : "accept",
		(gname != NULL) ? (char *)gnamebuf.value : "?");

	log_cred(*cred);
	result = ISC_R_SUCCESS;

cleanup:
	if (gname != NULL) {
		gret = gss_release_name(&minor, &gname);
		if (gret != GSS_S_COMPLETE)
			gss_log(3, "failed gss_release_name: %s",
				gss_error_tostring(gret, minor, buf,
						   sizeof(buf)));
	}

	return (result);
#else
	REQUIRE(cred != NULL && *cred == NULL);

	UNUSED(name);
	UNUSED(initiate);
	UNUSED(cred);

	return (ISC_R_NOTIMPLEMENTED);
#endif
}

isc_boolean_t
dst_gssapi_identitymatchesrealmkrb5(dns_name_t *signer, dns_name_t *name,
				    dns_name_t *realm)
{
#ifdef GSSAPI
	char sbuf[DNS_NAME_FORMATSIZE];
	char nbuf[DNS_NAME_FORMATSIZE];
	char rbuf[DNS_NAME_FORMATSIZE];
	char *sname;
	char *rname;
	isc_buffer_t buffer;
	isc_result_t result;

	/*
	 * It is far, far easier to write the names we are looking at into
	 * a string, and do string operations on them.
	 */
	isc_buffer_init(&buffer, sbuf, sizeof(sbuf));
	result = dns_name_toprincipal(signer, &buffer);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	isc_buffer_putuint8(&buffer, 0);
	if (name != NULL)
		dns_name_format(name, nbuf, sizeof(nbuf));
	dns_name_format(realm, rbuf, sizeof(rbuf));

	/*
	 * Find the realm portion.  This is the part after the @.  If it
	 * does not exist, we don't have something we like, so we fail our
	 * compare.
	 */
	rname = strchr(sbuf, '@');
	if (rname == NULL)
		return (isc_boolean_false);
	*rname = '\0';
	rname++;

	/*
	 * Find the host portion of the signer's name.	We do this by
	 * searching for the first / character.  We then check to make
	 * certain the instance name is "host"
	 *
	 * This will work for
	 *    host/example.com@EXAMPLE.COM
	 */
	sname = strchr(sbuf, '/');
	if (sname == NULL)
		return (isc_boolean_false);
	*sname = '\0';
	sname++;
	if (strcmp(sbuf, "host") != 0)
		return (isc_boolean_false);

	/*
	 * Now, we do a simple comparison between the name and the realm.
	 */
	if (name != NULL) {
		if ((strcasecmp(sname, nbuf) == 0)
		    && (strcmp(rname, rbuf) == 0))
			return (isc_boolean_true);
	} else {
		if (strcmp(rname, rbuf) == 0)
			return (isc_boolean_true);
	}

	return (isc_boolean_false);
#else
	UNUSED(signer);
	UNUSED(name);
	UNUSED(realm);
	return (isc_boolean_false);
#endif
}

isc_boolean_t
dst_gssapi_identitymatchesrealmms(dns_name_t *signer, dns_name_t *name,
				  dns_name_t *realm)
{
#ifdef GSSAPI
	char sbuf[DNS_NAME_FORMATSIZE];
	char nbuf[DNS_NAME_FORMATSIZE];
	char rbuf[DNS_NAME_FORMATSIZE];
	char *sname;
	char *nname;
	char *rname;
	isc_buffer_t buffer;
	isc_result_t result;

	/*
	 * It is far, far easier to write the names we are looking at into
	 * a string, and do string operations on them.
	 */
	isc_buffer_init(&buffer, sbuf, sizeof(sbuf));
	result = dns_name_toprincipal(signer, &buffer);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	isc_buffer_putuint8(&buffer, 0);
	if (name != NULL)
		dns_name_format(name, nbuf, sizeof(nbuf));
	dns_name_format(realm, rbuf, sizeof(rbuf));

	/*
	 * Find the realm portion.  This is the part after the @.  If it
	 * does not exist, we don't have something we like, so we fail our
	 * compare.
	 */
	rname = strchr(sbuf, '@');
	if (rname == NULL)
		return (isc_boolean_false);
	sname = strchr(sbuf, '$');
	if (sname == NULL)
		return (isc_boolean_false);

	/*
	 * Verify that the $ and @ follow one another.
	 */
	if (rname - sname != 1)
		return (isc_boolean_false);

	/*
	 * Find the host portion of the signer's name.	Zero out the $ so
	 * it terminates the signer's name, and skip past the @ for
	 * the realm.
	 *
	 * All service principals in Microsoft format seem to be in
	 *    machinename$@EXAMPLE.COM
	 * format.
	 */
	rname++;
	*sname = '\0';
	sname = sbuf;

	/*
	 * Find the first . in the target name, and make it the end of
	 * the string.	 The rest of the name has to match the realm.
	 */
	if (name != NULL) {
		nname = strchr(nbuf, '.');
		if (nname == NULL)
			return (isc_boolean_false);
		*nname++ = '\0';
	}

	/*
	 * Now, we do a simple comparison between the name and the realm.
	 */
	if (name != NULL) {
		if ((strcasecmp(sname, nbuf) == 0)
		    && (strcmp(rname, rbuf) == 0)
		    && (strcasecmp(nname, rbuf) == 0))
			return (isc_boolean_true);
	} else {
		if (strcmp(rname, rbuf) == 0)
			return (isc_boolean_true);
	}


	return (isc_boolean_false);
#else
	UNUSED(signer);
	UNUSED(name);
	UNUSED(realm);
	return (isc_boolean_false);
#endif
}

isc_result_t
dst_gssapi_releasecred(gss_cred_id_t *cred) {
#ifdef GSSAPI
	OM_uint32 gret, minor;
	char buf[1024];

	REQUIRE(cred != NULL && *cred != NULL);

	gret = gss_release_cred(&minor, cred);
	if (gret != GSS_S_COMPLETE) {
		/* Log the error, but still free the credential's memory */
		gss_log(3, "failed releasing credential: %s",
			gss_error_tostring(gret, minor, buf, sizeof(buf)));
	}
	*cred = NULL;

	return(ISC_R_SUCCESS);
#else
	UNUSED(cred);

	return (ISC_R_NOTIMPLEMENTED);
#endif
}

#ifdef GSSAPI
/*
 * Format a gssapi error message info into a char ** on the given memory
 * context. This is used to return gssapi error messages back up the
 * call chain for reporting to the user.
 */
static void
gss_err_message(isc_mem_t *mctx, isc_uint32_t major, isc_uint32_t minor,
		char **err_message)
{
	char buf[1024];
	char *estr;

	if (err_message == NULL || mctx == NULL) {
		/* the caller doesn't want any error messages */
		return;
	}

	estr = gss_error_tostring(major, minor, buf, sizeof(buf));
	if (estr != NULL)
		(*err_message) = isc_mem_strdup(mctx, estr);
}
#endif

isc_result_t
dst_gssapi_initctx(dns_name_t *name, isc_buffer_t *intoken,
		   isc_buffer_t *outtoken, gss_ctx_id_t *gssctx,
		   isc_mem_t *mctx, char **err_message)
{
#ifdef GSSAPI
	isc_region_t r;
	isc_buffer_t namebuf;
	gss_name_t gname;
	OM_uint32 gret, minor, ret_flags, flags;
	gss_buffer_desc gintoken, *gintokenp, gouttoken = GSS_C_EMPTY_BUFFER;
	isc_result_t result;
	gss_buffer_desc gnamebuf;
	unsigned char array[DNS_NAME_MAXTEXT + 1];

	/* Client must pass us a valid gss_ctx_id_t here */
	REQUIRE(gssctx != NULL);
	REQUIRE(mctx != NULL);

	isc_buffer_init(&namebuf, array, sizeof(array));
	name_to_gbuffer(name, &namebuf, &gnamebuf);

	/* Get the name as a GSS name */
	gret = gss_import_name(&minor, &gnamebuf, GSS_C_NO_OID, &gname);
	if (gret != GSS_S_COMPLETE) {
		gss_err_message(mctx, gret, minor, err_message);
		result = ISC_R_FAILURE;
		goto out;
	}

	if (intoken != NULL) {
		/* Don't call gss_release_buffer for gintoken! */
		REGION_TO_GBUFFER(*intoken, gintoken);
		gintokenp = &gintoken;
	} else {
		gintokenp = NULL;
	}

	/*
	 * Note that we don't set GSS_C_SEQUENCE_FLAG as Windows DNS
	 * servers don't like it.
	 */
	flags = GSS_C_REPLAY_FLAG | GSS_C_MUTUAL_FLAG | GSS_C_INTEG_FLAG;

	gret = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL, gssctx,
				    gname, GSS_SPNEGO_MECHANISM, flags,
				    0, NULL, gintokenp,
				    NULL, &gouttoken, &ret_flags, NULL);

	if (gret != GSS_S_COMPLETE && gret != GSS_S_CONTINUE_NEEDED) {
		gss_err_message(mctx, gret, minor, err_message);
		if (err_message != NULL && *err_message != NULL)
			gss_log(3, "Failure initiating security context: %s",
				*err_message);
		else
			gss_log(3, "Failure initiating security context");

		result = ISC_R_FAILURE;
		goto out;
	}

	/*
	 * XXXSRA Not handled yet: RFC 3645 3.1.1: check ret_flags
	 * MUTUAL and INTEG flags, fail if either not set.
	 */

	/*
	 * RFC 2744 states the a valid output token has a non-zero length.
	 */
	if (gouttoken.length != 0U) {
		GBUFFER_TO_REGION(gouttoken, r);
		RETERR(isc_buffer_copyregion(outtoken, &r));
	}

	if (gret == GSS_S_COMPLETE)
		result = ISC_R_SUCCESS;
	else
		result = DNS_R_CONTINUE;

 out:
	if (gouttoken.length != 0U)
		(void)gss_release_buffer(&minor, &gouttoken);
	(void)gss_release_name(&minor, &gname);
	return (result);
#else
	UNUSED(name);
	UNUSED(intoken);
	UNUSED(outtoken);
	UNUSED(gssctx);
	UNUSED(mctx);
	UNUSED(err_message);

	return (ISC_R_NOTIMPLEMENTED);
#endif
}

isc_result_t
dst_gssapi_acceptctx(gss_cred_id_t cred,
		     const char *gssapi_keytab,
		     isc_region_t *intoken, isc_buffer_t **outtoken,
		     gss_ctx_id_t *ctxout, dns_name_t *principal,
		     isc_mem_t *mctx)
{
#ifdef GSSAPI
	isc_region_t r;
	isc_buffer_t namebuf;
	gss_buffer_desc gnamebuf = GSS_C_EMPTY_BUFFER, gintoken,
			gouttoken = GSS_C_EMPTY_BUFFER;
	OM_uint32 gret, minor;
	gss_ctx_id_t context = GSS_C_NO_CONTEXT;
	gss_name_t gname = NULL;
	isc_result_t result;
	char buf[1024];

	REQUIRE(outtoken != NULL && *outtoken == NULL);

	REGION_TO_GBUFFER(*intoken, gintoken);

	if (*ctxout == NULL)
		context = GSS_C_NO_CONTEXT;
	else
		context = *ctxout;

	if (gssapi_keytab != NULL) {
#if defined(ISC_PLATFORM_GSSAPI_KRB5_HEADER) || defined(WIN32)
		gret = gsskrb5_register_acceptor_identity(gssapi_keytab);
		if (gret != GSS_S_COMPLETE) {
			gss_log(3, "failed "
				"gsskrb5_register_acceptor_identity(%s): %s",
				gssapi_keytab,
				gss_error_tostring(gret, 0, buf, sizeof(buf)));
			return (DNS_R_INVALIDTKEY);
		}
#else
		/*
		 * Minimize memory leakage by only setting KRB5_KTNAME
		 * if it needs to change.
		 */
		const char *old = getenv("KRB5_KTNAME");
		if (old == NULL || strcmp(old, gssapi_keytab) != 0) {
			char *kt = malloc(strlen(gssapi_keytab) + 13);
			if (kt == NULL)
				return (ISC_R_NOMEMORY);
			sprintf(kt, "KRB5_KTNAME=%s", gssapi_keytab);
			if (putenv(kt) != 0)
				return (ISC_R_NOMEMORY);
		}
#endif
	}

	log_cred(cred);

	gret = gss_accept_sec_context(&minor, &context, cred, &gintoken,
				      GSS_C_NO_CHANNEL_BINDINGS, &gname,
				      NULL, &gouttoken, NULL, NULL, NULL);

	result = ISC_R_FAILURE;

	switch (gret) {
	case GSS_S_COMPLETE:
		result = ISC_R_SUCCESS;
		break;
	case GSS_S_CONTINUE_NEEDED:
		result = DNS_R_CONTINUE;
		break;
	case GSS_S_DEFECTIVE_TOKEN:
	case GSS_S_DEFECTIVE_CREDENTIAL:
	case GSS_S_BAD_SIG:
	case GSS_S_DUPLICATE_TOKEN:
	case GSS_S_OLD_TOKEN:
	case GSS_S_NO_CRED:
	case GSS_S_CREDENTIALS_EXPIRED:
	case GSS_S_BAD_BINDINGS:
	case GSS_S_NO_CONTEXT:
	case GSS_S_BAD_MECH:
	case GSS_S_FAILURE:
		result = DNS_R_INVALIDTKEY;
		/* fall through */
	default:
		gss_log(3, "failed gss_accept_sec_context: %s",
			gss_error_tostring(gret, minor, buf, sizeof(buf)));
		return (result);
	}

	if (gouttoken.length > 0U) {
		RETERR(isc_buffer_allocate(mctx, outtoken,
					   (unsigned int)gouttoken.length));
		GBUFFER_TO_REGION(gouttoken, r);
		RETERR(isc_buffer_copyregion(*outtoken, &r));
		(void)gss_release_buffer(&minor, &gouttoken);
	}

	if (gret == GSS_S_COMPLETE) {
		gret = gss_display_name(&minor, gname, &gnamebuf, NULL);
		if (gret != GSS_S_COMPLETE) {
			gss_log(3, "failed gss_display_name: %s",
				gss_error_tostring(gret, minor,
						   buf, sizeof(buf)));
			RETERR(ISC_R_FAILURE);
		}

		/*
		 * Compensate for a bug in Solaris8's implementation
		 * of gss_display_name().  Should be harmless in any
		 * case, since principal names really should not
		 * contain null characters.
		 */
		if (gnamebuf.length > 0U &&
		    ((char *)gnamebuf.value)[gnamebuf.length - 1] == '\0')
			gnamebuf.length--;

		gss_log(3, "gss-api source name (accept) is %.*s",
			(int)gnamebuf.length, (char *)gnamebuf.value);

		GBUFFER_TO_REGION(gnamebuf, r);
		isc_buffer_init(&namebuf, r.base, r.length);
		isc_buffer_add(&namebuf, r.length);

		RETERR(dns_name_fromtext(principal, &namebuf, dns_rootname,
					 0, NULL));

		if (gnamebuf.length != 0U) {
			gret = gss_release_buffer(&minor, &gnamebuf);
			if (gret != GSS_S_COMPLETE)
				gss_log(3, "failed gss_release_buffer: %s",
					gss_error_tostring(gret, minor, buf,
							   sizeof(buf)));
		}
	}

	*ctxout = context;

 out:
	if (gname != NULL) {
		gret = gss_release_name(&minor, &gname);
		if (gret != GSS_S_COMPLETE)
			gss_log(3, "failed gss_release_name: %s",
				gss_error_tostring(gret, minor, buf,
						   sizeof(buf)));
	}

	return (result);
#else
	UNUSED(cred);
	UNUSED(gssapi_keytab);
	UNUSED(intoken);
	UNUSED(outtoken);
	UNUSED(ctxout);
	UNUSED(principal);
	UNUSED(mctx);

	return (ISC_R_NOTIMPLEMENTED);
#endif
}

isc_result_t
dst_gssapi_deletectx(isc_mem_t *mctx, gss_ctx_id_t *gssctx)
{
#ifdef GSSAPI
	OM_uint32 gret, minor;
	char buf[1024];

	UNUSED(mctx);

	REQUIRE(gssctx != NULL && *gssctx != NULL);

	/* Delete the context from the GSS provider */
	gret = gss_delete_sec_context(&minor, gssctx, GSS_C_NO_BUFFER);
	if (gret != GSS_S_COMPLETE) {
		/* Log the error, but still free the context's memory */
		gss_log(3, "Failure deleting security context %s",
			gss_error_tostring(gret, minor, buf, sizeof(buf)));
	}
	return(ISC_R_SUCCESS);
#else
	UNUSED(mctx);
	UNUSED(gssctx);
	return (ISC_R_NOTIMPLEMENTED);
#endif
}

char *
gss_error_tostring(isc_uint32_t major, isc_uint32_t minor,
		   char *buf, size_t buflen) {
#ifdef GSSAPI
	gss_buffer_desc msg_minor = GSS_C_EMPTY_BUFFER,
			msg_major = GSS_C_EMPTY_BUFFER;
	OM_uint32 msg_ctx, minor_stat;

	/* Handle major status */
	msg_ctx = 0;
	(void)gss_display_status(&minor_stat, major, GSS_C_GSS_CODE,
				 GSS_C_NULL_OID, &msg_ctx, &msg_major);

	/* Handle minor status */
	msg_ctx = 0;
	(void)gss_display_status(&minor_stat, minor, GSS_C_MECH_CODE,
				 GSS_C_NULL_OID, &msg_ctx, &msg_minor);

	snprintf(buf, buflen, "GSSAPI error: Major = %s, Minor = %s.",
		(char *)msg_major.value, (char *)msg_minor.value);

	if (msg_major.length != 0U)
		(void)gss_release_buffer(&minor_stat, &msg_major);
	if (msg_minor.length != 0U)
		(void)gss_release_buffer(&minor_stat, &msg_minor);
	return(buf);
#else
	snprintf(buf, buflen, "GSSAPI error: Major = %u, Minor = %u.",
		 major, minor);

	return (buf);
#endif
}

void
gss_log(int level, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	isc_log_vwrite(dns_lctx, DNS_LOGCATEGORY_GENERAL,
		       DNS_LOGMODULE_TKEY, ISC_LOG_DEBUG(level), fmt, ap);
	va_end(ap);
}

/*! \file */
