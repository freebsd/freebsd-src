/* $OpenBSD: auth2-hostbased.c,v 1.26 2016/03/07 19:02:43 djm Exp $ */
/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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

#include <sys/types.h>

#include <pwd.h>
#include <string.h>
#include <stdarg.h>

#include "xmalloc.h"
#include "ssh2.h"
#include "packet.h"
#include "buffer.h"
#include "log.h"
#include "misc.h"
#include "servconf.h"
#include "compat.h"
#include "key.h"
#include "hostfile.h"
#include "auth.h"
#include "canohost.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "pathnames.h"
#include "match.h"

/* import */
extern ServerOptions options;
extern u_char *session_id2;
extern u_int session_id2_len;

static int
userauth_hostbased(Authctxt *authctxt)
{
	Buffer b;
	Key *key = NULL;
	char *pkalg, *cuser, *chost, *service;
	u_char *pkblob, *sig;
	u_int alen, blen, slen;
	int pktype;
	int authenticated = 0;

	if (!authctxt->valid) {
		debug2("userauth_hostbased: disabled because of invalid user");
		return 0;
	}
	pkalg = packet_get_string(&alen);
	pkblob = packet_get_string(&blen);
	chost = packet_get_string(NULL);
	cuser = packet_get_string(NULL);
	sig = packet_get_string(&slen);

	debug("userauth_hostbased: cuser %s chost %s pkalg %s slen %d",
	    cuser, chost, pkalg, slen);
#ifdef DEBUG_PK
	debug("signature:");
	buffer_init(&b);
	buffer_append(&b, sig, slen);
	buffer_dump(&b);
	buffer_free(&b);
#endif
	pktype = key_type_from_name(pkalg);
	if (pktype == KEY_UNSPEC) {
		/* this is perfectly legal */
		logit("userauth_hostbased: unsupported "
		    "public key algorithm: %s", pkalg);
		goto done;
	}
	key = key_from_blob(pkblob, blen);
	if (key == NULL) {
		error("userauth_hostbased: cannot decode key: %s", pkalg);
		goto done;
	}
	if (key->type != pktype) {
		error("userauth_hostbased: type mismatch for decoded key "
		    "(received %d, expected %d)", key->type, pktype);
		goto done;
	}
	if (key_type_plain(key->type) == KEY_RSA &&
	    (datafellows & SSH_BUG_RSASIGMD5) != 0) {
		error("Refusing RSA key because peer uses unsafe "
		    "signature format");
		goto done;
	}
	if (match_pattern_list(sshkey_ssh_name(key),
	    options.hostbased_key_types, 0) != 1) {
		logit("%s: key type %s not in HostbasedAcceptedKeyTypes",
		    __func__, sshkey_type(key));
		goto done;
	}

	service = datafellows & SSH_BUG_HBSERVICE ? "ssh-userauth" :
	    authctxt->service;
	buffer_init(&b);
	buffer_put_string(&b, session_id2, session_id2_len);
	/* reconstruct packet */
	buffer_put_char(&b, SSH2_MSG_USERAUTH_REQUEST);
	buffer_put_cstring(&b, authctxt->user);
	buffer_put_cstring(&b, service);
	buffer_put_cstring(&b, "hostbased");
	buffer_put_string(&b, pkalg, alen);
	buffer_put_string(&b, pkblob, blen);
	buffer_put_cstring(&b, chost);
	buffer_put_cstring(&b, cuser);
#ifdef DEBUG_PK
	buffer_dump(&b);
#endif

	pubkey_auth_info(authctxt, key,
	    "client user \"%.100s\", client host \"%.100s\"", cuser, chost);

	/* test for allowed key and correct signature */
	authenticated = 0;
	if (PRIVSEP(hostbased_key_allowed(authctxt->pw, cuser, chost, key)) &&
	    PRIVSEP(key_verify(key, sig, slen, buffer_ptr(&b),
			buffer_len(&b))) == 1)
		authenticated = 1;

	buffer_free(&b);
done:
	debug2("userauth_hostbased: authenticated %d", authenticated);
	if (key != NULL)
		key_free(key);
	free(pkalg);
	free(pkblob);
	free(cuser);
	free(chost);
	free(sig);
	return authenticated;
}

/* return 1 if given hostkey is allowed */
int
hostbased_key_allowed(struct passwd *pw, const char *cuser, char *chost,
    Key *key)
{
	struct ssh *ssh = active_state; /* XXX */
	const char *resolvedname, *ipaddr, *lookup, *reason;
	HostStatus host_status;
	int len;
	char *fp;

	if (auth_key_is_revoked(key))
		return 0;

	resolvedname = auth_get_canonical_hostname(ssh, options.use_dns);
	ipaddr = ssh_remote_ipaddr(ssh);

	debug2("%s: chost %s resolvedname %s ipaddr %s", __func__,
	    chost, resolvedname, ipaddr);

	if (((len = strlen(chost)) > 0) && chost[len - 1] == '.') {
		debug2("stripping trailing dot from chost %s", chost);
		chost[len - 1] = '\0';
	}

	if (options.hostbased_uses_name_from_packet_only) {
		if (auth_rhosts2(pw, cuser, chost, chost) == 0) {
			debug2("%s: auth_rhosts2 refused "
			    "user \"%.100s\" host \"%.100s\" (from packet)",
			    __func__, cuser, chost);
			return 0;
		}
		lookup = chost;
	} else {
		if (strcasecmp(resolvedname, chost) != 0)
			logit("userauth_hostbased mismatch: "
			    "client sends %s, but we resolve %s to %s",
			    chost, ipaddr, resolvedname);
		if (auth_rhosts2(pw, cuser, resolvedname, ipaddr) == 0) {
			debug2("%s: auth_rhosts2 refused "
			    "user \"%.100s\" host \"%.100s\" addr \"%.100s\"",
			    __func__, cuser, resolvedname, ipaddr);
			return 0;
		}
		lookup = resolvedname;
	}
	debug2("%s: access allowed by auth_rhosts2", __func__);

	if (key_is_cert(key) && 
	    key_cert_check_authority(key, 1, 0, lookup, &reason)) {
		error("%s", reason);
		auth_debug_add("%s", reason);
		return 0;
	}

	host_status = check_key_in_hostfiles(pw, key, lookup,
	    _PATH_SSH_SYSTEM_HOSTFILE,
	    options.ignore_user_known_hosts ? NULL : _PATH_SSH_USER_HOSTFILE);

	/* backward compat if no key has been found. */
	if (host_status == HOST_NEW) {
		host_status = check_key_in_hostfiles(pw, key, lookup,
		    _PATH_SSH_SYSTEM_HOSTFILE2,
		    options.ignore_user_known_hosts ? NULL :
		    _PATH_SSH_USER_HOSTFILE2);
	}

	if (host_status == HOST_OK) {
		if (key_is_cert(key)) {
			if ((fp = sshkey_fingerprint(key->cert->signature_key,
			    options.fingerprint_hash, SSH_FP_DEFAULT)) == NULL)
				fatal("%s: sshkey_fingerprint fail", __func__);
			verbose("Accepted certificate ID \"%s\" signed by "
			    "%s CA %s from %s@%s", key->cert->key_id,
			    key_type(key->cert->signature_key), fp,
			    cuser, lookup);
		} else {
			if ((fp = sshkey_fingerprint(key,
			    options.fingerprint_hash, SSH_FP_DEFAULT)) == NULL)
				fatal("%s: sshkey_fingerprint fail", __func__);
			verbose("Accepted %s public key %s from %s@%s",
			    key_type(key), fp, cuser, lookup);
		}
		free(fp);
	}

	return (host_status == HOST_OK);
}

Authmethod method_hostbased = {
	"hostbased",
	userauth_hostbased,
	&options.hostbased_authentication
};
