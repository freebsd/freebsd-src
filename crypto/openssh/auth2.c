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
RCSID("$OpenBSD: auth2.c,v 1.20 2000/10/14 12:16:56 markus Exp $");
RCSID("$FreeBSD$");

#include <openssl/dsa.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>

#include "xmalloc.h"
#include "rsa.h"
#include "ssh.h"
#include "pty.h"
#include "packet.h"
#include "buffer.h"
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
#include "auth-options.h"

#ifdef HAVE_LOGIN_CAP
#include <login_cap.h>
#endif /* HAVE_LOGIN_CAP */

/* import */
extern ServerOptions options;
extern unsigned char *session_id2;
extern int session_id2_len;

static Authctxt	*x_authctxt = NULL;
static int one = 1;

typedef struct Authmethod Authmethod;
struct Authmethod {
	char	*name;
	int	(*userauth)(Authctxt *authctxt);
	int	*enabled;
};

/* protocol */

void	input_service_request(int type, int plen, void *ctxt);
void	input_userauth_request(int type, int plen, void *ctxt);
void	protocol_error(int type, int plen, void *ctxt);


/* helper */
Authmethod	*authmethod_lookup(const char *name);
struct passwd	*pwcopy(struct passwd *pw);
int	user_dsa_key_allowed(struct passwd *pw, Key *key);
char	*authmethods_get(void);

/* auth */
int	userauth_none(Authctxt *authctxt);
int	userauth_passwd(Authctxt *authctxt);
int	userauth_pubkey(Authctxt *authctxt);
int	userauth_kbdint(Authctxt *authctxt);

Authmethod authmethods[] = {
	{"none",
		userauth_none,
		&one},
	{"publickey",
		userauth_pubkey,
		&options.dsa_authentication},
	{"keyboard-interactive",
		userauth_kbdint,
		&options.kbd_interactive_authentication},
	{"password",
		userauth_passwd,
		&options.password_authentication},
	{NULL, NULL, NULL}
};

/*
 * loop until authctxt->success == TRUE
 */

void
do_authentication2()
{
	Authctxt *authctxt = xmalloc(sizeof(*authctxt));
	memset(authctxt, 'a', sizeof(*authctxt));
	authctxt->valid = 0;
	authctxt->attempt = 0;
	authctxt->success = 0;
	x_authctxt = authctxt;		/*XXX*/

#ifdef KRB4
	/* turn off kerberos, not supported by SSH2 */
	options.krb4_authentication = 0;
#endif
	dispatch_init(&protocol_error);
	dispatch_set(SSH2_MSG_SERVICE_REQUEST, &input_service_request);
	dispatch_run(DISPATCH_BLOCK, &authctxt->success, authctxt);
	do_authenticated2();
}

void
protocol_error(int type, int plen, void *ctxt)
{
	log("auth: protocol error: type %d plen %d", type, plen);
	packet_start(SSH2_MSG_UNIMPLEMENTED);
	packet_put_int(0);
	packet_send();
	packet_write_wait();
}

void
input_service_request(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	unsigned int len;
	int accept = 0;
	char *service = packet_get_string(&len);
	packet_done();

	if (authctxt == NULL)
		fatal("input_service_request: no authctxt");

	if (strcmp(service, "ssh-userauth") == 0) {
		if (!authctxt->success) {
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
input_userauth_request(int type, int plen, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	Authmethod *m = NULL;
	int authenticated = 0;
	char *user, *service, *method, *authmsg = NULL;
#ifdef HAVE_LOGIN_CAP
	login_cap_t *lc;
#endif /* HAVE_LOGIN_CAP */
#if defined(HAVE_LOGIN_CAP) || defined(LOGIN_ACCESS)
	const char *from_host, *from_ip;

	from_host = get_canonical_hostname();
	from_ip = get_remote_ipaddr();
#endif /* HAVE_LOGIN_CAP || LOGIN_ACCESS */

	if (authctxt == NULL)
		fatal("input_userauth_request: no authctxt");
	if (authctxt->attempt++ >= AUTH_FAIL_MAX)
		packet_disconnect("too many failed userauth_requests");

	user = packet_get_string(NULL);
	service = packet_get_string(NULL);
	method = packet_get_string(NULL);
	debug("userauth-request for user %s service %s method %s", user, service, method);
	debug("attempt #%d", authctxt->attempt);

	if (authctxt->attempt == 1) { 
		/* setup auth context */
		struct passwd *pw = NULL;
		setproctitle("%s", user);
		pw = getpwnam(user);
		if (pw && allowed_user(pw) && strcmp(service, "ssh-connection")==0) {
			authctxt->pw = pwcopy(pw);
			authctxt->valid = 1;
			debug2("input_userauth_request: setting up authctxt for %s", user);
#ifdef USE_PAM
			start_pam(pw);
#endif
		} else {
			log("input_userauth_request: illegal user %s", user);
		}
		authctxt->user = xstrdup(user);
		authctxt->service = xstrdup(service);
	} else if (authctxt->valid) {
		if (strcmp(user, authctxt->user) != 0 ||
		    strcmp(service, authctxt->service) != 0) {
			log("input_userauth_request: missmatch: (%s,%s)!=(%s,%s)",
			    user, service, authctxt->user, authctxt->service);
			authctxt->valid = 0;
		}
	}

#ifdef HAVE_LOGIN_CAP
	if (authctxt->pw != NULL) {
		lc = login_getpwclass(authctxt->pw);
		if (lc == NULL)
			lc = login_getclassbyname(NULL, authctxt->pw);
		if (!auth_hostok(lc, from_host, from_ip)) {
			log("Denied connection for %.200s from %.200s [%.200s].",
			    authctxt->pw->pw_name, from_host, from_ip);
			packet_disconnect("Sorry, you are not allowed to connect.");
		}
		if (!auth_timeok(lc, time(NULL))) {
			log("LOGIN %.200s REFUSED (TIME) FROM %.200s",
			    authctxt->pw->pw_name, from_host);
			packet_disconnect("Logins not available right now.");
		}
		login_close(lc);
		lc = NULL;
	}
#endif  /* HAVE_LOGIN_CAP */
#ifdef LOGIN_ACCESS
	if (authctxt->pw != NULL &&
	    !login_access(authctxt->pw->pw_name, from_host)) {
		log("Denied connection for %.200s from %.200s [%.200s].",
		    authctxt->pw->pw_name, from_host, from_ip);
		packet_disconnect("Sorry, you are not allowed to connect.");
	}
#endif /* LOGIN_ACCESS */

	m = authmethod_lookup(method);
	if (m != NULL) {
		debug2("input_userauth_request: try method %s", method);
		authenticated =	m->userauth(authctxt);
	} else {
		debug2("input_userauth_request: unsupported method %s", method);
	}
	if (!authctxt->valid && authenticated == 1) {
		log("input_userauth_request: INTERNAL ERROR: authenticated invalid user %s service %s", user, method);
		authenticated = 0;
	}

	/* Special handling for root */
	if (authenticated == 1 &&
	    authctxt->valid && authctxt->pw->pw_uid == 0 && !options.permit_root_login) {
		authenticated = 0;
		log("ROOT LOGIN REFUSED FROM %.200s", get_canonical_hostname());
	}

#ifdef USE_PAM
	if (authenticated && authctxt->user && !do_pam_account(authctxt->user, NULL))
		authenticated = 0;
#endif /* USE_PAM */

	/* Log before sending the reply */
	userauth_log(authctxt, authenticated, method);
	userauth_reply(authctxt, authenticated);

	xfree(service);
	xfree(user);
	xfree(method);
}


void
userauth_log(Authctxt *authctxt, int authenticated, char *method)
{
	void (*authlog) (const char *fmt,...) = verbose;
	char *user = NULL, *authmsg = NULL;

	/* Raise logging level */
	if (authenticated == 1 ||
	    !authctxt->valid ||
	    authctxt->attempt >= AUTH_FAIL_LOG ||
	    strcmp(method, "password") == 0)
		authlog = log;

	if (authenticated == 1) {
		authmsg = "Accepted";
	} else if (authenticated == 0) {
		authmsg = "Failed";
	} else {
		authmsg = "Postponed";
	}

	if (authctxt->valid) {
		user = authctxt->pw->pw_uid == 0 ? "ROOT" : authctxt->user;
	} else {
		user = "NOUSER";
	}

	authlog("%s %s for %.200s from %.200s port %d ssh2",
	    authmsg,
	    method,
	    user,
	    get_remote_ipaddr(),
	    get_remote_port());
}

void   
userauth_reply(Authctxt *authctxt, int authenticated)
{
	/* XXX todo: check if multiple auth methods are needed */
	if (authenticated == 1) {
		/* turn off userauth */
		dispatch_set(SSH2_MSG_USERAUTH_REQUEST, &protocol_error);
		packet_start(SSH2_MSG_USERAUTH_SUCCESS);
		packet_send();
		packet_write_wait();
		/* now we can break out */
		authctxt->success = 1;
	} else if (authenticated == 0) {
		char *methods = authmethods_get();
		packet_start(SSH2_MSG_USERAUTH_FAILURE);
		packet_put_cstring(methods);
		packet_put_char(0);	/* XXX partial success, unused */
		packet_send();
		packet_write_wait();
		xfree(methods);
	} else {
		/* do nothing, we did already send a reply */
	}
}

int
userauth_none(Authctxt *authctxt)
{
	/* disable method "none", only allowed one time */
	Authmethod *m = authmethod_lookup("none");
	if (m != NULL)
		m->enabled = NULL;
	packet_done();
#ifdef USE_PAM
	return authctxt->valid ? auth_pam_password(authctxt->pw, "") : 0;
#else /* !USE_PAM */
	return authctxt->valid ? auth_password(authctxt->pw, "") : 0;
#endif /* USE_PAM */
}

int
userauth_passwd(Authctxt *authctxt)
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
	if (authctxt->valid &&
#ifdef USE_PAM
	    auth_pam_password(authctxt->pw, password) == 1
#else
	    auth_password(authctxt->pw, password) == 1
#endif
	    )
		authenticated = 1;
	memset(password, 0, len);
	xfree(password);
	return authenticated;
}

int
userauth_kbdint(Authctxt *authctxt)
{
	int authenticated = 0;
	char *lang = NULL;
	char *devs = NULL;

	lang = packet_get_string(NULL);
	devs = packet_get_string(NULL);
	packet_done();

	debug("keyboard-interactive language %s devs %s", lang, devs);
#ifdef SKEY
	/* XXX hardcoded, we should look at devs */
	if (options.skey_authentication != 0)
		authenticated = auth2_skey(authctxt);
#endif
	xfree(lang);
	xfree(devs);
	return authenticated;
}

int
userauth_pubkey(Authctxt *authctxt)
{
	Buffer b;
	Key *key;
	char *pkalg, *pkblob, *sig;
	unsigned int alen, blen, slen;
	int have_sig;
	int authenticated = 0;

	if (!authctxt->valid) {
		debug2("userauth_pubkey: disabled because of invalid user");
		return 0;
	}
	have_sig = packet_get_char();
	pkalg = packet_get_string(&alen);
	if (strcmp(pkalg, KEX_DSS) != 0) {
		log("bad pkalg %s", pkalg);	/*XXX*/
		xfree(pkalg);
		return 0;
	}
	pkblob = packet_get_string(&blen);
	key = dsa_key_from_blob(pkblob, blen);
	if (key != NULL) {
		if (have_sig) {
			sig = packet_get_string(&slen);
			packet_done();
			buffer_init(&b);
			if (datafellows & SSH_OLD_SESSIONID) {
				buffer_append(&b, session_id2, session_id2_len);
			} else {
				buffer_put_string(&b, session_id2, session_id2_len);
			}
			/* reconstruct packet */
			buffer_put_char(&b, SSH2_MSG_USERAUTH_REQUEST);
			buffer_put_cstring(&b, authctxt->user);
			buffer_put_cstring(&b,
			    datafellows & SSH_BUG_PUBKEYAUTH ?
			    "ssh-userauth" :
			    authctxt->service);
			buffer_put_cstring(&b, "publickey");
			buffer_put_char(&b, have_sig);
			buffer_put_cstring(&b, KEX_DSS);
			buffer_put_string(&b, pkblob, blen);
#ifdef DEBUG_DSS
			buffer_dump(&b);
#endif
			/* test for correct signature */
			if (user_dsa_key_allowed(authctxt->pw, key) &&
			    dsa_verify(key, sig, slen, buffer_ptr(&b), buffer_len(&b)) == 1)
				authenticated = 1;
			buffer_clear(&b);
			xfree(sig);
		} else {
			debug("test whether pkalg/pkblob are acceptable");
			packet_done();

			/* XXX fake reply and always send PK_OK ? */
			/*
			 * XXX this allows testing whether a user is allowed
			 * to login: if you happen to have a valid pubkey this
			 * message is sent. the message is NEVER sent at all
			 * if a user is not allowed to login. is this an
			 * issue? -markus
			 */
			if (user_dsa_key_allowed(authctxt->pw, key)) {
				packet_start(SSH2_MSG_USERAUTH_PK_OK);
				packet_put_string(pkalg, alen);
				packet_put_string(pkblob, blen);
				packet_send();
				packet_write_wait();
				authenticated = -1;
			}
		}
		if (authenticated != 1)
			auth_clear_options();
		key_free(key);
	}
	xfree(pkalg);
	xfree(pkblob);
	return authenticated;
}

/* get current user */

struct passwd*
auth_get_user(void)
{
	return (x_authctxt != NULL && x_authctxt->valid) ? x_authctxt->pw : NULL;
}

#define	DELIM	","

char *
authmethods_get(void)
{
	Authmethod *method = NULL;
	unsigned int size = 0;
	char *list;

	for (method = authmethods; method->name != NULL; method++) {
		if (strcmp(method->name, "none") == 0)
			continue;
		if (method->enabled != NULL && *(method->enabled) != 0) {
			if (size != 0)
				size += strlen(DELIM);
			size += strlen(method->name);
		}
	}
	size++;			/* trailing '\0' */
	list = xmalloc(size);
	list[0] = '\0';

	for (method = authmethods; method->name != NULL; method++) {
		if (strcmp(method->name, "none") == 0)
			continue;
		if (method->enabled != NULL && *(method->enabled) != 0) {
			if (list[0] != '\0')
				strlcat(list, DELIM, size);
			strlcat(list, method->name, size);
		}
	}
	return list;
}

Authmethod *
authmethod_lookup(const char *name)
{
	Authmethod *method = NULL;
	if (name != NULL)
		for (method = authmethods; method->name != NULL; method++)
			if (method->enabled != NULL &&
			    *(method->enabled) != 0 &&
			    strcmp(name, method->name) == 0)
				return method;
	debug2("Unrecognized authentication method name: %s", name ? name : "NULL");
	return NULL;
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

	if (pw == NULL)
		return 0;

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
			snprintf(buf, sizeof buf,
			    "%s authentication refused for %.100s: "
			    "bad ownership or modes for '%s'.",
			    key_type(key), pw->pw_name, file);
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
					    "%s authentication refused for %.100s: "
					    "bad ownership or modes for '%s'.",
					    key_type(key), pw->pw_name, line);
					fail = 1;
					break;
				}
			}
		}
		if (fail) {
			fclose(f);
			log("%s",buf);
			restore_uid();
			return 0;
		}
	}
	found_key = 0;
	found = key_new(key->type);

	while (fgets(line, sizeof(line), f)) {
		char *cp, *options = NULL;
		linenum++;
		/* Skip leading whitespace, empty and comment lines. */
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '\n' || *cp == '#')
			continue;

		bits = key_read(found, &cp);
		if (bits == 0) {
			/* no key?  check if there are options for this key */
			int quoted = 0;
			options = cp;
			for (; *cp && (quoted || (*cp != ' ' && *cp != '\t')); cp++) {
				if (*cp == '\\' && cp[1] == '"')
					cp++;	/* Skip both */
				else if (*cp == '"')
					quoted = !quoted;
			}
			/* Skip remaining whitespace. */
			for (; *cp == ' ' || *cp == '\t'; cp++)
				;
			bits = key_read(found, &cp);
			if (bits == 0) {
				/* still no key?  advance to next line*/
				continue;
			}
		}
		if (key_equal(found, key) &&
		    auth_parse_options(pw, options, linenum) == 1) {
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

struct passwd *
pwcopy(struct passwd *pw)
{
	struct passwd *copy = xmalloc(sizeof(*copy));
	memset(copy, 0, sizeof(*copy));
	copy->pw_name = xstrdup(pw->pw_name);
	copy->pw_passwd = xstrdup(pw->pw_passwd);
	copy->pw_uid = pw->pw_uid;
	copy->pw_gid = pw->pw_gid;
	copy->pw_class = xstrdup(pw->pw_class);
	copy->pw_dir = xstrdup(pw->pw_dir);
	copy->pw_shell = xstrdup(pw->pw_shell);
	copy->pw_expire = pw->pw_expire;
	copy->pw_change = pw->pw_change;
	return copy;
}
