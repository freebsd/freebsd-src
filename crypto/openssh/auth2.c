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
RCSID("$OpenBSD: auth2.c,v 1.95 2002/08/22 21:33:58 markus Exp $");
RCSID("$FreeBSD$");

#include "canohost.h"
#include "ssh2.h"
#include "xmalloc.h"
#include "packet.h"
#include "log.h"
#include "servconf.h"
#include "compat.h"
#include "auth.h"
#include "dispatch.h"
#include "pathnames.h"
#include "monitor_wrap.h"

/* import */
extern ServerOptions options;
extern u_char *session_id2;
extern int session_id2_len;

Authctxt *x_authctxt = NULL;

/* methods */

extern Authmethod method_none;
extern Authmethod method_pubkey;
extern Authmethod method_passwd;
extern Authmethod method_kbdint;
extern Authmethod method_hostbased;

Authmethod *authmethods[] = {
	&method_none,
	&method_pubkey,
	&method_passwd,
	&method_kbdint,
	&method_hostbased,
	NULL
};

/* protocol */

static void input_service_request(int, u_int32_t, void *);
static void input_userauth_request(int, u_int32_t, void *);

/* helper */
static Authmethod *authmethod_lookup(const char *);
static char *authmethods_get(void);
int user_key_allowed(struct passwd *, Key *);
int hostbased_key_allowed(struct passwd *, const char *, char *, Key *);

/*
 * loop until authctxt->success == TRUE
 */

Authctxt *
do_authentication2(void)
{
	Authctxt *authctxt = authctxt_new();

	x_authctxt = authctxt;		/*XXX*/

	/* challenge-response is implemented via keyboard interactive */
	if (options.challenge_response_authentication)
		options.kbd_interactive_authentication = 1;
	if (options.pam_authentication_via_kbd_int)
		options.kbd_interactive_authentication = 1;
	if (use_privsep)
		options.pam_authentication_via_kbd_int = 0;

	dispatch_init(&dispatch_protocol_error);
	dispatch_set(SSH2_MSG_SERVICE_REQUEST, &input_service_request);
	dispatch_run(DISPATCH_BLOCK, &authctxt->success, authctxt);

	return (authctxt);
}

static void
input_service_request(int type, u_int32_t seq, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	u_int len;
	int acceptit = 0;
	char *service = packet_get_string(&len);
	packet_check_eom();

	if (authctxt == NULL)
		fatal("input_service_request: no authctxt");

	if (strcmp(service, "ssh-userauth") == 0) {
		if (!authctxt->success) {
			acceptit = 1;
			/* now we can handle user-auth requests */
			dispatch_set(SSH2_MSG_USERAUTH_REQUEST, &input_userauth_request);
		}
	}
	/* XXX all other service requests are denied */

	if (acceptit) {
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

static void
input_userauth_request(int type, u_int32_t seq, void *ctxt)
{
	Authctxt *authctxt = ctxt;
	Authmethod *m = NULL;
	char *user, *service, *method, *style = NULL;
	int authenticated = 0;
#ifdef HAVE_LOGIN_CAP
	login_cap_t *lc;
	const char *from_host, *from_ip;

        from_host = get_canonical_hostname(options.verify_reverse_mapping);
        from_ip = get_remote_ipaddr();
#endif

	if (authctxt == NULL)
		fatal("input_userauth_request: no authctxt");

	user = packet_get_string(NULL);
	service = packet_get_string(NULL);
	method = packet_get_string(NULL);
	debug("userauth-request for user %s service %s method %s", user, service, method);
	debug("attempt %d failures %d", authctxt->attempt, authctxt->failures);

	if ((style = strchr(user, ':')) != NULL)
		*style++ = 0;

	if (authctxt->attempt++ == 0) {
		/* setup auth context */
		authctxt->pw = PRIVSEP(getpwnamallow(user));
		if (authctxt->pw && strcmp(service, "ssh-connection")==0) {
			authctxt->valid = 1;
			debug2("input_userauth_request: setting up authctxt for %s", user);
#ifdef USE_PAM
			PRIVSEP(start_pam(authctxt->pw->pw_name));
#endif
		} else {
			log("input_userauth_request: illegal user %s", user);
#ifdef USE_PAM
			PRIVSEP(start_pam("NOUSER"));
#endif
		}
		setproctitle("%s%s", authctxt->pw ? user : "unknown",
		    use_privsep ? " [net]" : "");
		authctxt->user = xstrdup(user);
		authctxt->service = xstrdup(service);
		authctxt->style = style ? xstrdup(style) : NULL;
		if (use_privsep)
			mm_inform_authserv(service, style);
	} else if (strcmp(user, authctxt->user) != 0 ||
	    strcmp(service, authctxt->service) != 0) {
		packet_disconnect("Change of username or service not allowed: "
		    "(%s,%s) -> (%s,%s)",
		    authctxt->user, authctxt->service, user, service);
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

	/* reset state */
	auth2_challenge_stop(authctxt);
	authctxt->postponed = 0;

	/* try to authenticate user */
	m = authmethod_lookup(method);
	if (m != NULL) {
		debug2("input_userauth_request: try method %s", method);
		authenticated =	m->userauth(authctxt);
	}
	userauth_finish(authctxt, authenticated, method);

	xfree(service);
	xfree(user);
	xfree(method);
}

void
userauth_finish(Authctxt *authctxt, int authenticated, char *method)
{
	char *methods;

	if (!authctxt->valid && authenticated)
		fatal("INTERNAL ERROR: authenticated invalid user %s",
		    authctxt->user);

	/* Special handling for root */
	if (!use_privsep &&
	    authenticated && authctxt->pw->pw_uid == 0 &&
	    !auth_root_allowed(method))
		authenticated = 0;

#ifdef USE_PAM
	if (!use_privsep && authenticated && authctxt->user && 
	    !do_pam_account(authctxt->user, NULL))
		authenticated = 0;
#endif /* USE_PAM */

#ifdef _UNICOS
	if (authenticated && cray_access_denied(authctxt->user)) {
		authenticated = 0;
		fatal("Access denied for user %s.",authctxt->user);
	}
#endif /* _UNICOS */

	/* Log before sending the reply */
	auth_log(authctxt, authenticated, method, " ssh2");

	if (authctxt->postponed)
		return;

	/* XXX todo: check if multiple auth methods are needed */
	if (authenticated == 1) {
		/* turn off userauth */
		dispatch_set(SSH2_MSG_USERAUTH_REQUEST, &dispatch_protocol_ignore);
		packet_start(SSH2_MSG_USERAUTH_SUCCESS);
		packet_send();
		packet_write_wait();
		/* now we can break out */
		authctxt->success = 1;
	} else {
		if (authctxt->failures++ > AUTH_FAIL_MAX) {
			packet_disconnect(AUTH_FAIL_MSG, authctxt->user);
		}
#ifdef _UNICOS
		if (strcmp(method, "password") == 0)
			cray_login_failure(authctxt->user, IA_UDBERR);
#endif /* _UNICOS */
		methods = authmethods_get();
		packet_start(SSH2_MSG_USERAUTH_FAILURE);
		packet_put_cstring(methods);
		packet_put_char(0);	/* XXX partial success, unused */
		packet_send();
		packet_write_wait();
		xfree(methods);
	}
}

/* get current user */

struct passwd*
auth_get_user(void)
{
	return (x_authctxt != NULL && x_authctxt->valid) ? x_authctxt->pw : NULL;
}

#define	DELIM	","

static char *
authmethods_get(void)
{
	Buffer b;
	char *list;
	int i;

	buffer_init(&b);
	for (i = 0; authmethods[i] != NULL; i++) {
		if (strcmp(authmethods[i]->name, "none") == 0)
			continue;
		if (authmethods[i]->enabled != NULL &&
		    *(authmethods[i]->enabled) != 0) {
			if (buffer_len(&b) > 0)
				buffer_append(&b, ",", 1);
			buffer_append(&b, authmethods[i]->name,
			    strlen(authmethods[i]->name));
		}
	}
	buffer_append(&b, "\0", 1);
	list = xstrdup(buffer_ptr(&b));
	buffer_free(&b);
	return list;
}

static Authmethod *
authmethod_lookup(const char *name)
{
	int i;

	if (name != NULL)
		for (i = 0; authmethods[i] != NULL; i++)
			if (authmethods[i]->enabled != NULL &&
			    *(authmethods[i]->enabled) != 0 &&
			    strcmp(name, authmethods[i]->name) == 0)
				return authmethods[i];
	debug2("Unrecognized authentication method name: %s",
	    name ? name : "NULL");
	return NULL;
}
