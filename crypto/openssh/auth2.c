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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */
#include "includes.h"
RCSID("$OpenBSD: auth2.c,v 1.8 2000/05/08 17:42:24 markus Exp $");

#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>

#include "xmalloc.h"
#include "rsa.h"
#include "ssh.h"
#include "pty.h"
#include "packet.h"
#include "buffer.h"
#include "cipher.h"
#include "servconf.h"
#include "compat.h"
#include "channels.h"
#include "bufaux.h"
#include "ssh2.h"
#include "auth.h"
#include "session.h"
#include "dispatch.h"
#include "auth.h"
#include "key.h"
#include "kex.h"

#include "dsa.h"
#include "uidswap.h"

/* import */
extern ServerOptions options;
extern unsigned char *session_id2;
extern int session_id2_len;

/* protocol */

void	input_service_request(int type, int plen);
void	input_userauth_request(int type, int plen);
void	protocol_error(int type, int plen);

/* auth */
int	ssh2_auth_none(struct passwd *pw);
int	ssh2_auth_password(struct passwd *pw);
int	ssh2_auth_pubkey(struct passwd *pw, unsigned char *raw, unsigned int rlen);

/* helper */
struct passwd*	 auth_set_user(char *u, char *s);
int	user_dsa_key_allowed(struct passwd *pw, Key *key);

typedef struct Authctxt Authctxt;
struct Authctxt {
	char *user;
	char *service;
	struct passwd pw;
	int valid;
};
static Authctxt	*authctxt = NULL;
static int userauth_success = 0;

/*
 * loop until userauth_success == TRUE
 */

void
do_authentication2()
{
	/* turn off skey/kerberos, not supported by SSH2 */
#ifdef SKEY
	options.skey_authentication = 0;
#endif
#ifdef KRB4
	options.krb4_authentication = 0;
#endif

	dispatch_init(&protocol_error);
	dispatch_set(SSH2_MSG_SERVICE_REQUEST, &input_service_request);
	dispatch_run(DISPATCH_BLOCK, &userauth_success);
	do_authenticated2();
}

void
protocol_error(int type, int plen)
{
	log("auth: protocol error: type %d plen %d", type, plen);
	packet_start(SSH2_MSG_UNIMPLEMENTED);
	packet_put_int(0);
	packet_send();
	packet_write_wait();
}

void
input_service_request(int type, int plen)
{
	unsigned int len;
	int accept = 0;
	char *service = packet_get_string(&len);
	packet_done();

	if (strcmp(service, "ssh-userauth") == 0) {
		if (!userauth_success) {
			accept = 1;
			/* now we can handle user-auth requests */
			dispatch_set(SSH2_MSG_USERAUTH_REQUEST, &input_userauth_request);
		}
	}
	/* XXX all other service requests are denied */

	if (accept) {
		packet_start(SSH2_MSG_SERVICE_ACCEPT);
		packet_put_cstring(service);
		packet_send();
		packet_write_wait();
	} else {
		debug("bad service request %s", service);
		packet_disconnect("bad service request %s", service);
	}
	xfree(service);
}

void
input_userauth_request(int type, int plen)
{
	static void (*authlog) (const char *fmt,...) = verbose;
	static int attempt = 0;
	unsigned int len, rlen;
	int authenticated = 0;
	char *raw, *user, *service, *method, *authmsg = NULL;
	struct passwd *pw;

	if (++attempt == AUTH_FAIL_MAX)
		packet_disconnect("too many failed userauth_requests");

	raw = packet_get_raw(&rlen);
	if (plen != rlen)
		fatal("plen != rlen");
	user = packet_get_string(&len);
	service = packet_get_string(&len);
	method = packet_get_string(&len);
	debug("userauth-request for user %s service %s method %s", user, service, method);

	/* XXX we only allow the ssh-connection service */
	pw = auth_set_user(user, service);
	if (pw && strcmp(service, "ssh-connection")==0) {
		if (strcmp(method, "none") == 0) {
			authenticated =	ssh2_auth_none(pw);
		} else if (strcmp(method, "password") == 0) {
			authenticated =	ssh2_auth_password(pw);
		} else if (strcmp(method, "publickey") == 0) {
			authenticated =	ssh2_auth_pubkey(pw, raw, rlen);
		}
	}
	if (authenticated && pw && pw->pw_uid == 0 && !options.permit_root_login) {
		authenticated = 0;
		log("ROOT LOGIN REFUSED FROM %.200s",
		    get_canonical_hostname());
	}

	/* Raise logging level */
	if (authenticated == 1 ||
	    attempt == AUTH_FAIL_LOG ||
	    strcmp(method, "password") == 0)
		authlog = log;

	/* Log before sending the reply */
	if (authenticated == 1) {
		authmsg = "Accepted";
	} else if (authenticated == 0) {
		authmsg = "Failed";
	} else {
		authmsg = "Postponed";
	}
	authlog("%s %s for %.200s from %.200s port %d ssh2",
		authmsg,
		method,
		pw && pw->pw_uid == 0 ? "ROOT" : user,
		get_remote_ipaddr(),
		get_remote_port());

	/* XXX todo: check if multiple auth methods are needed */
	if (authenticated == 1) {
		/* turn off userauth */
		dispatch_set(SSH2_MSG_USERAUTH_REQUEST, &protocol_error);
		packet_start(SSH2_MSG_USERAUTH_SUCCESS);
		packet_send();
		packet_write_wait();
		/* now we can break out */
		userauth_success = 1;
	} else if (authenticated == 0) {
		packet_start(SSH2_MSG_USERAUTH_FAILURE);
		packet_put_cstring("publickey,password");	/* XXX dynamic */
		packet_put_char(0);				/* XXX partial success, unused */
		packet_send();
		packet_write_wait();
	}

	xfree(service);
	xfree(user);
	xfree(method);
}

int
ssh2_auth_none(struct passwd *pw)
{
	packet_done();
	return auth_password(pw, "");
}
int
ssh2_auth_password(struct passwd *pw)
{
	char *password;
	int authenticated = 0;
	int change;
	unsigned int len;
	change = packet_get_char();
	if (change)
		log("password change not supported");
	password = packet_get_string(&len);
	packet_done();
	if (options.password_authentication &&
	    auth_password(pw, password) == 1)
		authenticated = 1;
	memset(password, 0, len);
	xfree(password);
	return authenticated;
}
int
ssh2_auth_pubkey(struct passwd *pw, unsigned char *raw, unsigned int rlen)
{
	Buffer b;
	Key *key;
	char *pkalg, *pkblob, *sig;
	unsigned int alen, blen, slen;
	int have_sig;
	int authenticated = 0;

	if (options.dsa_authentication == 0) {
		debug("pubkey auth disabled");
		return 0;
	}
	if (datafellows & SSH_BUG_PUBKEYAUTH) {
		log("bug compatibility with ssh-2.0.13 pubkey not implemented");
		return 0;
	}
	have_sig = packet_get_char();
	pkalg = packet_get_string(&alen);
	if (strcmp(pkalg, KEX_DSS) != 0) {
		xfree(pkalg);
		log("bad pkalg %s", pkalg);	/*XXX*/
		return 0;
	}
	pkblob = packet_get_string(&blen);
	key = dsa_key_from_blob(pkblob, blen);
	if (key != NULL) {
		if (have_sig) {
			sig = packet_get_string(&slen);
			packet_done();
			buffer_init(&b);
			buffer_append(&b, session_id2, session_id2_len);
			buffer_put_char(&b, SSH2_MSG_USERAUTH_REQUEST);
			if (slen + 4 > rlen)
				fatal("bad rlen/slen");
			buffer_append(&b, raw, rlen - slen - 4);
#ifdef DEBUG_DSS
			buffer_dump(&b);
#endif
			/* test for correct signature */
			if (user_dsa_key_allowed(pw, key) &&
			    dsa_verify(key, sig, slen, buffer_ptr(&b), buffer_len(&b)) == 1)
				authenticated = 1;
			buffer_clear(&b);
			xfree(sig);
		} else {
			packet_done();
			debug("test key...");
			/* test whether pkalg/pkblob are acceptable */
			/* XXX fake reply and always send PK_OK ? */
			/*
			 * XXX this allows testing whether a user is allowed
			 * to login: if you happen to have a valid pubkey this
			 * message is sent. the message is NEVER sent at all
			 * if a user is not allowed to login. is this an
			 * issue? -markus
			 */
			if (user_dsa_key_allowed(pw, key)) {
				packet_start(SSH2_MSG_USERAUTH_PK_OK);
				packet_put_string(pkalg, alen);
				packet_put_string(pkblob, blen);
				packet_send();
				packet_write_wait();
				authenticated = -1;
			}
		}
		key_free(key);
	}
	xfree(pkalg);
	xfree(pkblob);
	return authenticated;
}

/* set and get current user */

struct passwd*
auth_get_user(void)
{
	return (authctxt != NULL && authctxt->valid) ? &authctxt->pw : NULL;
}

struct passwd*
auth_set_user(char *u, char *s)
{
	struct passwd *pw, *copy;

	if (authctxt == NULL) {
		authctxt = xmalloc(sizeof(*authctxt));
		authctxt->valid = 0;
		authctxt->user = xstrdup(u);
		authctxt->service = xstrdup(s);
		setproctitle("%s", u);
		pw = getpwnam(u);
		if (!pw || !allowed_user(pw)) {
			log("auth_set_user: illegal user %s", u);
			return NULL;
		}
		copy = &authctxt->pw;
		memset(copy, 0, sizeof(*copy));
		copy->pw_name = xstrdup(pw->pw_name);
		copy->pw_passwd = xstrdup(pw->pw_passwd);
		copy->pw_uid = pw->pw_uid;
		copy->pw_gid = pw->pw_gid;
		copy->pw_dir = xstrdup(pw->pw_dir);
		copy->pw_shell = xstrdup(pw->pw_shell);
		copy->pw_class = xstrdup(pw->pw_class);
		copy->pw_expire = pw->pw_expire;
		copy->pw_change = pw->pw_change;
		authctxt->valid = 1;
	} else {
		if (strcmp(u, authctxt->user) != 0 ||
		    strcmp(s, authctxt->service) != 0) {
			log("auth_set_user: missmatch: (%s,%s)!=(%s,%s)",
			    u, s, authctxt->user, authctxt->service);
			return NULL;
		}
	}
	return auth_get_user();
}

/* return 1 if user allows given key */
int
user_dsa_key_allowed(struct passwd *pw, Key *key)
{
	char line[8192], file[1024];
	int found_key = 0;
	unsigned int bits = -1;
	FILE *f;
	unsigned long linenum = 0;
	struct stat st;
	Key *found;

	/* Temporarily use the user's uid. */
	temporarily_use_uid(pw->pw_uid);

	/* The authorized keys. */
	snprintf(file, sizeof file, "%.500s/%.100s", pw->pw_dir,
	    SSH_USER_PERMITTED_KEYS2);

	/* Fail quietly if file does not exist */
	if (stat(file, &st) < 0) {
		/* Restore the privileged uid. */
		restore_uid();
		return 0;
	}
	/* Open the file containing the authorized keys. */
	f = fopen(file, "r");
	if (!f) {
		/* Restore the privileged uid. */
		restore_uid();
		return 0;
	}
	if (options.strict_modes) {
		int fail = 0;
		char buf[1024];
		/* Check open file in order to avoid open/stat races */
		if (fstat(fileno(f), &st) < 0 ||
		    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		    (st.st_mode & 022) != 0) {
			snprintf(buf, sizeof buf, "DSA authentication refused for %.100s: "
			    "bad ownership or modes for '%s'.", pw->pw_name, file);
			fail = 1;
		} else {
			/* Check path to SSH_USER_PERMITTED_KEYS */
			int i;
			static const char *check[] = {
				"", SSH_USER_DIR, NULL
			};
			for (i = 0; check[i]; i++) {
				snprintf(line, sizeof line, "%.500s/%.100s",
				    pw->pw_dir, check[i]);
				if (stat(line, &st) < 0 ||
				    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
				    (st.st_mode & 022) != 0) {
					snprintf(buf, sizeof buf,
					    "DSA authentication refused for %.100s: "
					    "bad ownership or modes for '%s'.",
					    pw->pw_name, line);
					fail = 1;
					break;
				}
			}
		}
		if (fail) {
			log(buf);
			fclose(f);
			restore_uid();
			return 0;
		}
	}
	found_key = 0;
	found = key_new(KEY_DSA);

	while (fgets(line, sizeof(line), f)) {
		char *cp;
		linenum++;
		/* Skip leading whitespace, empty and comment lines. */
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '\n' || *cp == '#')
			continue;
		bits = key_read(found, &cp);
		if (bits == 0)
			continue;
		if (key_equal(found, key)) {
			found_key = 1;
			debug("matching key found: file %s, line %ld",
			    file, linenum);
			break;
		}
	}
	restore_uid();
	fclose(f);
	key_free(found);
	return found_key;
}
