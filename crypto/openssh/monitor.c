/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * Copyright 2002 Markus Friedl <markus@openbsd.org>
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
RCSID("$OpenBSD: monitor.c,v 1.62 2005/01/30 11:18:08 dtucker Exp $");

#include <openssl/dh.h>

#ifdef SKEY
#include <skey.h>
#endif

#include "ssh.h"
#include "auth.h"
#include "kex.h"
#include "dh.h"
#ifdef TARGET_OS_MAC	/* XXX Broken krb5 headers on Mac */
#undef TARGET_OS_MAC
#include "zlib.h"
#define TARGET_OS_MAC 1
#else
#include "zlib.h"
#endif
#include "packet.h"
#include "auth-options.h"
#include "sshpty.h"
#include "channels.h"
#include "session.h"
#include "sshlogin.h"
#include "canohost.h"
#include "log.h"
#include "servconf.h"
#include "monitor.h"
#include "monitor_mm.h"
#include "monitor_wrap.h"
#include "monitor_fdpass.h"
#include "xmalloc.h"
#include "misc.h"
#include "buffer.h"
#include "bufaux.h"
#include "compat.h"
#include "ssh2.h"

#ifdef GSSAPI
#include "ssh-gss.h"
static Gssctxt *gsscontext = NULL;
#endif

/* Imports */
extern ServerOptions options;
extern u_int utmp_len;
extern Newkeys *current_keys[];
extern z_stream incoming_stream;
extern z_stream outgoing_stream;
extern u_char session_id[];
extern Buffer input, output;
extern Buffer auth_debug;
extern int auth_debug_init;
extern Buffer loginmsg;

/* State exported from the child */

struct {
	z_stream incoming;
	z_stream outgoing;
	u_char *keyin;
	u_int keyinlen;
	u_char *keyout;
	u_int keyoutlen;
	u_char *ivin;
	u_int ivinlen;
	u_char *ivout;
	u_int ivoutlen;
	u_char *ssh1key;
	u_int ssh1keylen;
	int ssh1cipher;
	int ssh1protoflags;
	u_char *input;
	u_int ilen;
	u_char *output;
	u_int olen;
} child_state;

/* Functions on the monitor that answer unprivileged requests */

int mm_answer_moduli(int, Buffer *);
int mm_answer_sign(int, Buffer *);
int mm_answer_pwnamallow(int, Buffer *);
int mm_answer_auth2_read_banner(int, Buffer *);
int mm_answer_authserv(int, Buffer *);
int mm_answer_authpassword(int, Buffer *);
int mm_answer_bsdauthquery(int, Buffer *);
int mm_answer_bsdauthrespond(int, Buffer *);
int mm_answer_skeyquery(int, Buffer *);
int mm_answer_skeyrespond(int, Buffer *);
int mm_answer_keyallowed(int, Buffer *);
int mm_answer_keyverify(int, Buffer *);
int mm_answer_pty(int, Buffer *);
int mm_answer_pty_cleanup(int, Buffer *);
int mm_answer_term(int, Buffer *);
int mm_answer_rsa_keyallowed(int, Buffer *);
int mm_answer_rsa_challenge(int, Buffer *);
int mm_answer_rsa_response(int, Buffer *);
int mm_answer_sesskey(int, Buffer *);
int mm_answer_sessid(int, Buffer *);

#ifdef USE_PAM
int mm_answer_pam_start(int, Buffer *);
int mm_answer_pam_account(int, Buffer *);
int mm_answer_pam_init_ctx(int, Buffer *);
int mm_answer_pam_query(int, Buffer *);
int mm_answer_pam_respond(int, Buffer *);
int mm_answer_pam_free_ctx(int, Buffer *);
#endif

#ifdef GSSAPI
int mm_answer_gss_setup_ctx(int, Buffer *);
int mm_answer_gss_accept_ctx(int, Buffer *);
int mm_answer_gss_userok(int, Buffer *);
int mm_answer_gss_checkmic(int, Buffer *);
#endif

#ifdef SSH_AUDIT_EVENTS
int mm_answer_audit_event(int, Buffer *);
int mm_answer_audit_command(int, Buffer *);
#endif

static Authctxt *authctxt;
static BIGNUM *ssh1_challenge = NULL;	/* used for ssh1 rsa auth */

/* local state for key verify */
static u_char *key_blob = NULL;
static u_int key_bloblen = 0;
static int key_blobtype = MM_NOKEY;
static char *hostbased_cuser = NULL;
static char *hostbased_chost = NULL;
static char *auth_method = "unknown";
static u_int session_id2_len = 0;
static u_char *session_id2 = NULL;
static pid_t monitor_child_pid;

struct mon_table {
	enum monitor_reqtype type;
	int flags;
	int (*f)(int, Buffer *);
};

#define MON_ISAUTH	0x0004	/* Required for Authentication */
#define MON_AUTHDECIDE	0x0008	/* Decides Authentication */
#define MON_ONCE	0x0010	/* Disable after calling */

#define MON_AUTH	(MON_ISAUTH|MON_AUTHDECIDE)

#define MON_PERMIT	0x1000	/* Request is permitted */

struct mon_table mon_dispatch_proto20[] = {
    {MONITOR_REQ_MODULI, MON_ONCE, mm_answer_moduli},
    {MONITOR_REQ_SIGN, MON_ONCE, mm_answer_sign},
    {MONITOR_REQ_PWNAM, MON_ONCE, mm_answer_pwnamallow},
    {MONITOR_REQ_AUTHSERV, MON_ONCE, mm_answer_authserv},
    {MONITOR_REQ_AUTH2_READ_BANNER, MON_ONCE, mm_answer_auth2_read_banner},
    {MONITOR_REQ_AUTHPASSWORD, MON_AUTH, mm_answer_authpassword},
#ifdef USE_PAM
    {MONITOR_REQ_PAM_START, MON_ONCE, mm_answer_pam_start},
    {MONITOR_REQ_PAM_ACCOUNT, 0, mm_answer_pam_account},
    {MONITOR_REQ_PAM_INIT_CTX, MON_ISAUTH, mm_answer_pam_init_ctx},
    {MONITOR_REQ_PAM_QUERY, MON_ISAUTH, mm_answer_pam_query},
    {MONITOR_REQ_PAM_RESPOND, MON_ISAUTH, mm_answer_pam_respond},
    {MONITOR_REQ_PAM_FREE_CTX, MON_ONCE|MON_AUTHDECIDE, mm_answer_pam_free_ctx},
#endif
#ifdef SSH_AUDIT_EVENTS
    {MONITOR_REQ_AUDIT_EVENT, MON_PERMIT, mm_answer_audit_event},
#endif
#ifdef BSD_AUTH
    {MONITOR_REQ_BSDAUTHQUERY, MON_ISAUTH, mm_answer_bsdauthquery},
    {MONITOR_REQ_BSDAUTHRESPOND, MON_AUTH,mm_answer_bsdauthrespond},
#endif
#ifdef SKEY
    {MONITOR_REQ_SKEYQUERY, MON_ISAUTH, mm_answer_skeyquery},
    {MONITOR_REQ_SKEYRESPOND, MON_AUTH, mm_answer_skeyrespond},
#endif
    {MONITOR_REQ_KEYALLOWED, MON_ISAUTH, mm_answer_keyallowed},
    {MONITOR_REQ_KEYVERIFY, MON_AUTH, mm_answer_keyverify},
#ifdef GSSAPI
    {MONITOR_REQ_GSSSETUP, MON_ISAUTH, mm_answer_gss_setup_ctx},
    {MONITOR_REQ_GSSSTEP, MON_ISAUTH, mm_answer_gss_accept_ctx},
    {MONITOR_REQ_GSSUSEROK, MON_AUTH, mm_answer_gss_userok},
    {MONITOR_REQ_GSSCHECKMIC, MON_ISAUTH, mm_answer_gss_checkmic},
#endif
    {0, 0, NULL}
};

struct mon_table mon_dispatch_postauth20[] = {
    {MONITOR_REQ_MODULI, 0, mm_answer_moduli},
    {MONITOR_REQ_SIGN, 0, mm_answer_sign},
    {MONITOR_REQ_PTY, 0, mm_answer_pty},
    {MONITOR_REQ_PTYCLEANUP, 0, mm_answer_pty_cleanup},
    {MONITOR_REQ_TERM, 0, mm_answer_term},
#ifdef SSH_AUDIT_EVENTS
    {MONITOR_REQ_AUDIT_EVENT, MON_PERMIT, mm_answer_audit_event},
    {MONITOR_REQ_AUDIT_COMMAND, MON_PERMIT, mm_answer_audit_command},
#endif
    {0, 0, NULL}
};

struct mon_table mon_dispatch_proto15[] = {
    {MONITOR_REQ_PWNAM, MON_ONCE, mm_answer_pwnamallow},
    {MONITOR_REQ_SESSKEY, MON_ONCE, mm_answer_sesskey},
    {MONITOR_REQ_SESSID, MON_ONCE, mm_answer_sessid},
    {MONITOR_REQ_AUTHPASSWORD, MON_AUTH, mm_answer_authpassword},
    {MONITOR_REQ_RSAKEYALLOWED, MON_ISAUTH, mm_answer_rsa_keyallowed},
    {MONITOR_REQ_KEYALLOWED, MON_ISAUTH, mm_answer_keyallowed},
    {MONITOR_REQ_RSACHALLENGE, MON_ONCE, mm_answer_rsa_challenge},
    {MONITOR_REQ_RSARESPONSE, MON_ONCE|MON_AUTHDECIDE, mm_answer_rsa_response},
#ifdef BSD_AUTH
    {MONITOR_REQ_BSDAUTHQUERY, MON_ISAUTH, mm_answer_bsdauthquery},
    {MONITOR_REQ_BSDAUTHRESPOND, MON_AUTH,mm_answer_bsdauthrespond},
#endif
#ifdef SKEY
    {MONITOR_REQ_SKEYQUERY, MON_ISAUTH, mm_answer_skeyquery},
    {MONITOR_REQ_SKEYRESPOND, MON_AUTH, mm_answer_skeyrespond},
#endif
#ifdef USE_PAM
    {MONITOR_REQ_PAM_START, MON_ONCE, mm_answer_pam_start},
    {MONITOR_REQ_PAM_ACCOUNT, 0, mm_answer_pam_account},
    {MONITOR_REQ_PAM_INIT_CTX, MON_ISAUTH, mm_answer_pam_init_ctx},
    {MONITOR_REQ_PAM_QUERY, MON_ISAUTH, mm_answer_pam_query},
    {MONITOR_REQ_PAM_RESPOND, MON_ISAUTH, mm_answer_pam_respond},
    {MONITOR_REQ_PAM_FREE_CTX, MON_ONCE|MON_AUTHDECIDE, mm_answer_pam_free_ctx},
#endif
#ifdef SSH_AUDIT_EVENTS
    {MONITOR_REQ_AUDIT_EVENT, MON_PERMIT, mm_answer_audit_event},
#endif
    {0, 0, NULL}
};

struct mon_table mon_dispatch_postauth15[] = {
    {MONITOR_REQ_PTY, MON_ONCE, mm_answer_pty},
    {MONITOR_REQ_PTYCLEANUP, MON_ONCE, mm_answer_pty_cleanup},
    {MONITOR_REQ_TERM, 0, mm_answer_term},
#ifdef SSH_AUDIT_EVENTS
    {MONITOR_REQ_AUDIT_EVENT, MON_PERMIT, mm_answer_audit_event},
    {MONITOR_REQ_AUDIT_COMMAND, MON_ONCE, mm_answer_audit_command},
#endif
    {0, 0, NULL}
};

struct mon_table *mon_dispatch;

/* Specifies if a certain message is allowed at the moment */

static void
monitor_permit(struct mon_table *ent, enum monitor_reqtype type, int permit)
{
	while (ent->f != NULL) {
		if (ent->type == type) {
			ent->flags &= ~MON_PERMIT;
			ent->flags |= permit ? MON_PERMIT : 0;
			return;
		}
		ent++;
	}
}

static void
monitor_permit_authentications(int permit)
{
	struct mon_table *ent = mon_dispatch;

	while (ent->f != NULL) {
		if (ent->flags & MON_AUTH) {
			ent->flags &= ~MON_PERMIT;
			ent->flags |= permit ? MON_PERMIT : 0;
		}
		ent++;
	}
}

void
monitor_child_preauth(Authctxt *_authctxt, struct monitor *pmonitor)
{
	struct mon_table *ent;
	int authenticated = 0;

	debug3("preauth child monitor started");

	authctxt = _authctxt;
	memset(authctxt, 0, sizeof(*authctxt));

	if (compat20) {
		mon_dispatch = mon_dispatch_proto20;

		/* Permit requests for moduli and signatures */
		monitor_permit(mon_dispatch, MONITOR_REQ_MODULI, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_SIGN, 1);
	} else {
		mon_dispatch = mon_dispatch_proto15;

		monitor_permit(mon_dispatch, MONITOR_REQ_SESSKEY, 1);
	}

	/* The first few requests do not require asynchronous access */
	while (!authenticated) {
		authenticated = monitor_read(pmonitor, mon_dispatch, &ent);
		if (authenticated) {
			if (!(ent->flags & MON_AUTHDECIDE))
				fatal("%s: unexpected authentication from %d",
				    __func__, ent->type);
			if (authctxt->pw->pw_uid == 0 &&
			    !auth_root_allowed(auth_method))
				authenticated = 0;
#ifdef USE_PAM
			/* PAM needs to perform account checks after auth */
			if (options.use_pam && authenticated) {
				Buffer m;

				buffer_init(&m);
				mm_request_receive_expect(pmonitor->m_sendfd,
				    MONITOR_REQ_PAM_ACCOUNT, &m);
				authenticated = mm_answer_pam_account(pmonitor->m_sendfd, &m);
				buffer_free(&m);
			}
#endif
		}

		if (ent->flags & MON_AUTHDECIDE) {
			auth_log(authctxt, authenticated, auth_method,
			    compat20 ? " ssh2" : "");
			if (!authenticated)
				authctxt->failures++;
		}
	}

	if (!authctxt->valid)
		fatal("%s: authenticated invalid user", __func__);

	debug("%s: %s has been authenticated by privileged process",
	    __func__, authctxt->user);

	mm_get_keystate(pmonitor);
}

static void
monitor_set_child_handler(pid_t pid)
{
	monitor_child_pid = pid;
}

static void
monitor_child_handler(int sig)
{
	kill(monitor_child_pid, sig);
}

void
monitor_child_postauth(struct monitor *pmonitor)
{
	monitor_set_child_handler(pmonitor->m_pid);
	signal(SIGHUP, &monitor_child_handler);
	signal(SIGTERM, &monitor_child_handler);

	if (compat20) {
		mon_dispatch = mon_dispatch_postauth20;

		/* Permit requests for moduli and signatures */
		monitor_permit(mon_dispatch, MONITOR_REQ_MODULI, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_SIGN, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_TERM, 1);
	} else {
		mon_dispatch = mon_dispatch_postauth15;
		monitor_permit(mon_dispatch, MONITOR_REQ_TERM, 1);
	}
	if (!no_pty_flag) {
		monitor_permit(mon_dispatch, MONITOR_REQ_PTY, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_PTYCLEANUP, 1);
	}

	for (;;)
		monitor_read(pmonitor, mon_dispatch, NULL);
}

void
monitor_sync(struct monitor *pmonitor)
{
	if (options.compression) {
		/* The member allocation is not visible, so sync it */
		mm_share_sync(&pmonitor->m_zlib, &pmonitor->m_zback);
	}
}

int
monitor_read(struct monitor *pmonitor, struct mon_table *ent,
    struct mon_table **pent)
{
	Buffer m;
	int ret;
	u_char type;

	buffer_init(&m);

	mm_request_receive(pmonitor->m_sendfd, &m);
	type = buffer_get_char(&m);

	debug3("%s: checking request %d", __func__, type);

	while (ent->f != NULL) {
		if (ent->type == type)
			break;
		ent++;
	}

	if (ent->f != NULL) {
		if (!(ent->flags & MON_PERMIT))
			fatal("%s: unpermitted request %d", __func__,
			    type);
		ret = (*ent->f)(pmonitor->m_sendfd, &m);
		buffer_free(&m);

		/* The child may use this request only once, disable it */
		if (ent->flags & MON_ONCE) {
			debug2("%s: %d used once, disabling now", __func__,
			    type);
			ent->flags &= ~MON_PERMIT;
		}

		if (pent != NULL)
			*pent = ent;

		return ret;
	}

	fatal("%s: unsupported request: %d", __func__, type);

	/* NOTREACHED */
	return (-1);
}

/* allowed key state */
static int
monitor_allowed_key(u_char *blob, u_int bloblen)
{
	/* make sure key is allowed */
	if (key_blob == NULL || key_bloblen != bloblen ||
	    memcmp(key_blob, blob, key_bloblen))
		return (0);
	return (1);
}

static void
monitor_reset_key_state(void)
{
	/* reset state */
	if (key_blob != NULL)
		xfree(key_blob);
	if (hostbased_cuser != NULL)
		xfree(hostbased_cuser);
	if (hostbased_chost != NULL)
		xfree(hostbased_chost);
	key_blob = NULL;
	key_bloblen = 0;
	key_blobtype = MM_NOKEY;
	hostbased_cuser = NULL;
	hostbased_chost = NULL;
}

int
mm_answer_moduli(int sock, Buffer *m)
{
	DH *dh;
	int min, want, max;

	min = buffer_get_int(m);
	want = buffer_get_int(m);
	max = buffer_get_int(m);

	debug3("%s: got parameters: %d %d %d",
	    __func__, min, want, max);
	/* We need to check here, too, in case the child got corrupted */
	if (max < min || want < min || max < want)
		fatal("%s: bad parameters: %d %d %d",
		    __func__, min, want, max);

	buffer_clear(m);

	dh = choose_dh(min, want, max);
	if (dh == NULL) {
		buffer_put_char(m, 0);
		return (0);
	} else {
		/* Send first bignum */
		buffer_put_char(m, 1);
		buffer_put_bignum2(m, dh->p);
		buffer_put_bignum2(m, dh->g);

		DH_free(dh);
	}
	mm_request_send(sock, MONITOR_ANS_MODULI, m);
	return (0);
}

int
mm_answer_sign(int sock, Buffer *m)
{
	Key *key;
	u_char *p;
	u_char *signature;
	u_int siglen, datlen;
	int keyid;

	debug3("%s", __func__);

	keyid = buffer_get_int(m);
	p = buffer_get_string(m, &datlen);

	if (datlen != 20)
		fatal("%s: data length incorrect: %u", __func__, datlen);

	/* save session id, it will be passed on the first call */
	if (session_id2_len == 0) {
		session_id2_len = datlen;
		session_id2 = xmalloc(session_id2_len);
		memcpy(session_id2, p, session_id2_len);
	}

	if ((key = get_hostkey_by_index(keyid)) == NULL)
		fatal("%s: no hostkey from index %d", __func__, keyid);
	if (key_sign(key, &signature, &siglen, p, datlen) < 0)
		fatal("%s: key_sign failed", __func__);

	debug3("%s: signature %p(%u)", __func__, signature, siglen);

	buffer_clear(m);
	buffer_put_string(m, signature, siglen);

	xfree(p);
	xfree(signature);

	mm_request_send(sock, MONITOR_ANS_SIGN, m);

	/* Turn on permissions for getpwnam */
	monitor_permit(mon_dispatch, MONITOR_REQ_PWNAM, 1);

	return (0);
}

/* Retrieves the password entry and also checks if the user is permitted */

int
mm_answer_pwnamallow(int sock, Buffer *m)
{
	char *username;
	struct passwd *pwent;
	int allowed = 0;

	debug3("%s", __func__);

	if (authctxt->attempt++ != 0)
		fatal("%s: multiple attempts for getpwnam", __func__);

	username = buffer_get_string(m, NULL);

	pwent = getpwnamallow(username);

	authctxt->user = xstrdup(username);
	setproctitle("%s [priv]", pwent ? username : "unknown");
	xfree(username);

	buffer_clear(m);

	if (pwent == NULL) {
		buffer_put_char(m, 0);
		authctxt->pw = fakepw();
		goto out;
	}

	allowed = 1;
	authctxt->pw = pwent;
	authctxt->valid = 1;

	buffer_put_char(m, 1);
	buffer_put_string(m, pwent, sizeof(struct passwd));
	buffer_put_cstring(m, pwent->pw_name);
	buffer_put_cstring(m, "*");
	buffer_put_cstring(m, pwent->pw_gecos);
#ifdef HAVE_PW_CLASS_IN_PASSWD
	buffer_put_cstring(m, pwent->pw_class);
#endif
	buffer_put_cstring(m, pwent->pw_dir);
	buffer_put_cstring(m, pwent->pw_shell);

 out:
	debug3("%s: sending MONITOR_ANS_PWNAM: %d", __func__, allowed);
	mm_request_send(sock, MONITOR_ANS_PWNAM, m);

	/* For SSHv1 allow authentication now */
	if (!compat20)
		monitor_permit_authentications(1);
	else {
		/* Allow service/style information on the auth context */
		monitor_permit(mon_dispatch, MONITOR_REQ_AUTHSERV, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_AUTH2_READ_BANNER, 1);
	}

#ifdef USE_PAM
	if (options.use_pam)
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_START, 1);
#endif
#ifdef SSH_AUDIT_EVENTS
	monitor_permit(mon_dispatch, MONITOR_REQ_AUDIT_COMMAND, 1);
#endif

	return (0);
}

int mm_answer_auth2_read_banner(int sock, Buffer *m)
{
	char *banner;

	buffer_clear(m);
	banner = auth2_read_banner();
	buffer_put_cstring(m, banner != NULL ? banner : "");
	mm_request_send(sock, MONITOR_ANS_AUTH2_READ_BANNER, m);

	if (banner != NULL)
		xfree(banner);

	return (0);
}

int
mm_answer_authserv(int sock, Buffer *m)
{
	monitor_permit_authentications(1);

	authctxt->service = buffer_get_string(m, NULL);
	authctxt->style = buffer_get_string(m, NULL);
	debug3("%s: service=%s, style=%s",
	    __func__, authctxt->service, authctxt->style);

	if (strlen(authctxt->style) == 0) {
		xfree(authctxt->style);
		authctxt->style = NULL;
	}

	return (0);
}

int
mm_answer_authpassword(int sock, Buffer *m)
{
	static int call_count;
	char *passwd;
	int authenticated;
	u_int plen;

	passwd = buffer_get_string(m, &plen);
	/* Only authenticate if the context is valid */
	authenticated = options.password_authentication &&
	    auth_password(authctxt, passwd);
	memset(passwd, 0, strlen(passwd));
	xfree(passwd);

	buffer_clear(m);
	buffer_put_int(m, authenticated);

	debug3("%s: sending result %d", __func__, authenticated);
	mm_request_send(sock, MONITOR_ANS_AUTHPASSWORD, m);

	call_count++;
	if (plen == 0 && call_count == 1)
		auth_method = "none";
	else
		auth_method = "password";

	/* Causes monitor loop to terminate if authenticated */
	return (authenticated);
}

#ifdef BSD_AUTH
int
mm_answer_bsdauthquery(int sock, Buffer *m)
{
	char *name, *infotxt;
	u_int numprompts;
	u_int *echo_on;
	char **prompts;
	u_int success;

	success = bsdauth_query(authctxt, &name, &infotxt, &numprompts,
	    &prompts, &echo_on) < 0 ? 0 : 1;

	buffer_clear(m);
	buffer_put_int(m, success);
	if (success)
		buffer_put_cstring(m, prompts[0]);

	debug3("%s: sending challenge success: %u", __func__, success);
	mm_request_send(sock, MONITOR_ANS_BSDAUTHQUERY, m);

	if (success) {
		xfree(name);
		xfree(infotxt);
		xfree(prompts);
		xfree(echo_on);
	}

	return (0);
}

int
mm_answer_bsdauthrespond(int sock, Buffer *m)
{
	char *response;
	int authok;

	if (authctxt->as == 0)
		fatal("%s: no bsd auth session", __func__);

	response = buffer_get_string(m, NULL);
	authok = options.challenge_response_authentication &&
	    auth_userresponse(authctxt->as, response, 0);
	authctxt->as = NULL;
	debug3("%s: <%s> = <%d>", __func__, response, authok);
	xfree(response);

	buffer_clear(m);
	buffer_put_int(m, authok);

	debug3("%s: sending authenticated: %d", __func__, authok);
	mm_request_send(sock, MONITOR_ANS_BSDAUTHRESPOND, m);

	auth_method = "bsdauth";

	return (authok != 0);
}
#endif

#ifdef SKEY
int
mm_answer_skeyquery(int sock, Buffer *m)
{
	struct skey skey;
	char challenge[1024];
	u_int success;

	success = _compat_skeychallenge(&skey, authctxt->user, challenge,
	    sizeof(challenge)) < 0 ? 0 : 1;

	buffer_clear(m);
	buffer_put_int(m, success);
	if (success)
		buffer_put_cstring(m, challenge);

	debug3("%s: sending challenge success: %u", __func__, success);
	mm_request_send(sock, MONITOR_ANS_SKEYQUERY, m);

	return (0);
}

int
mm_answer_skeyrespond(int sock, Buffer *m)
{
	char *response;
	int authok;

	response = buffer_get_string(m, NULL);

	authok = (options.challenge_response_authentication &&
	    authctxt->valid &&
	    skey_haskey(authctxt->pw->pw_name) == 0 &&
	    skey_passcheck(authctxt->pw->pw_name, response) != -1);

	xfree(response);

	buffer_clear(m);
	buffer_put_int(m, authok);

	debug3("%s: sending authenticated: %d", __func__, authok);
	mm_request_send(sock, MONITOR_ANS_SKEYRESPOND, m);

	auth_method = "skey";

	return (authok != 0);
}
#endif

#ifdef USE_PAM
int
mm_answer_pam_start(int sock, Buffer *m)
{
	if (!options.use_pam)
		fatal("UsePAM not set, but ended up in %s anyway", __func__);

	start_pam(authctxt);

	monitor_permit(mon_dispatch, MONITOR_REQ_PAM_ACCOUNT, 1);

	return (0);
}

int
mm_answer_pam_account(int sock, Buffer *m)
{
	u_int ret;

	if (!options.use_pam)
		fatal("UsePAM not set, but ended up in %s anyway", __func__);

	ret = do_pam_account();

	buffer_put_int(m, ret);
	buffer_append(&loginmsg, "\0", 1);
	buffer_put_cstring(m, buffer_ptr(&loginmsg));
	buffer_clear(&loginmsg);

	mm_request_send(sock, MONITOR_ANS_PAM_ACCOUNT, m);

	return (ret);
}

static void *sshpam_ctxt, *sshpam_authok;
extern KbdintDevice sshpam_device;

int
mm_answer_pam_init_ctx(int sock, Buffer *m)
{

	debug3("%s", __func__);
	authctxt->user = buffer_get_string(m, NULL);
	sshpam_ctxt = (sshpam_device.init_ctx)(authctxt);
	sshpam_authok = NULL;
	buffer_clear(m);
	if (sshpam_ctxt != NULL) {
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_FREE_CTX, 1);
		buffer_put_int(m, 1);
	} else {
		buffer_put_int(m, 0);
	}
	mm_request_send(sock, MONITOR_ANS_PAM_INIT_CTX, m);
	return (0);
}

int
mm_answer_pam_query(int sock, Buffer *m)
{
	char *name, *info, **prompts;
	u_int num, *echo_on;
	int i, ret;

	debug3("%s", __func__);
	sshpam_authok = NULL;
	ret = (sshpam_device.query)(sshpam_ctxt, &name, &info, &num, &prompts, &echo_on);
	if (ret == 0 && num == 0)
		sshpam_authok = sshpam_ctxt;
	if (num > 1 || name == NULL || info == NULL)
		ret = -1;
	buffer_clear(m);
	buffer_put_int(m, ret);
	buffer_put_cstring(m, name);
	xfree(name);
	buffer_put_cstring(m, info);
	xfree(info);
	buffer_put_int(m, num);
	for (i = 0; i < num; ++i) {
		buffer_put_cstring(m, prompts[i]);
		xfree(prompts[i]);
		buffer_put_int(m, echo_on[i]);
	}
	if (prompts != NULL)
		xfree(prompts);
	if (echo_on != NULL)
		xfree(echo_on);
	mm_request_send(sock, MONITOR_ANS_PAM_QUERY, m);
	return (0);
}

int
mm_answer_pam_respond(int sock, Buffer *m)
{
	char **resp;
	u_int num;
	int i, ret;

	debug3("%s", __func__);
	sshpam_authok = NULL;
	num = buffer_get_int(m);
	if (num > 0) {
		resp = xmalloc(num * sizeof(char *));
		for (i = 0; i < num; ++i)
			resp[i] = buffer_get_string(m, NULL);
		ret = (sshpam_device.respond)(sshpam_ctxt, num, resp);
		for (i = 0; i < num; ++i)
			xfree(resp[i]);
		xfree(resp);
	} else {
		ret = (sshpam_device.respond)(sshpam_ctxt, num, NULL);
	}
	buffer_clear(m);
	buffer_put_int(m, ret);
	mm_request_send(sock, MONITOR_ANS_PAM_RESPOND, m);
	auth_method = "keyboard-interactive/pam";
	if (ret == 0)
		sshpam_authok = sshpam_ctxt;
	return (0);
}

int
mm_answer_pam_free_ctx(int sock, Buffer *m)
{

	debug3("%s", __func__);
	(sshpam_device.free_ctx)(sshpam_ctxt);
	buffer_clear(m);
	mm_request_send(sock, MONITOR_ANS_PAM_FREE_CTX, m);
	return (sshpam_authok == sshpam_ctxt);
}
#endif

static void
mm_append_debug(Buffer *m)
{
	if (auth_debug_init && buffer_len(&auth_debug)) {
		debug3("%s: Appending debug messages for child", __func__);
		buffer_append(m, buffer_ptr(&auth_debug),
		    buffer_len(&auth_debug));
		buffer_clear(&auth_debug);
	}
}

int
mm_answer_keyallowed(int sock, Buffer *m)
{
	Key *key;
	char *cuser, *chost;
	u_char *blob;
	u_int bloblen;
	enum mm_keytype type = 0;
	int allowed = 0;

	debug3("%s entering", __func__);

	type = buffer_get_int(m);
	cuser = buffer_get_string(m, NULL);
	chost = buffer_get_string(m, NULL);
	blob = buffer_get_string(m, &bloblen);

	key = key_from_blob(blob, bloblen);

	if ((compat20 && type == MM_RSAHOSTKEY) ||
	    (!compat20 && type != MM_RSAHOSTKEY))
		fatal("%s: key type and protocol mismatch", __func__);

	debug3("%s: key_from_blob: %p", __func__, key);

	if (key != NULL && authctxt->valid) {
		switch(type) {
		case MM_USERKEY:
			allowed = options.pubkey_authentication &&
			    user_key_allowed(authctxt->pw, key);
			break;
		case MM_HOSTKEY:
			allowed = options.hostbased_authentication &&
			    hostbased_key_allowed(authctxt->pw,
			    cuser, chost, key);
			break;
		case MM_RSAHOSTKEY:
			key->type = KEY_RSA1; /* XXX */
			allowed = options.rhosts_rsa_authentication &&
			    auth_rhosts_rsa_key_allowed(authctxt->pw,
			    cuser, chost, key);
			break;
		default:
			fatal("%s: unknown key type %d", __func__, type);
			break;
		}
	}
	if (key != NULL)
		key_free(key);

	/* clear temporarily storage (used by verify) */
	monitor_reset_key_state();

	if (allowed) {
		/* Save temporarily for comparison in verify */
		key_blob = blob;
		key_bloblen = bloblen;
		key_blobtype = type;
		hostbased_cuser = cuser;
		hostbased_chost = chost;
	}

	debug3("%s: key %p is %s",
	    __func__, key, allowed ? "allowed" : "disallowed");

	buffer_clear(m);
	buffer_put_int(m, allowed);
	buffer_put_int(m, forced_command != NULL);

	mm_append_debug(m);

	mm_request_send(sock, MONITOR_ANS_KEYALLOWED, m);

	if (type == MM_RSAHOSTKEY)
		monitor_permit(mon_dispatch, MONITOR_REQ_RSACHALLENGE, allowed);

	return (0);
}

static int
monitor_valid_userblob(u_char *data, u_int datalen)
{
	Buffer b;
	char *p;
	u_int len;
	int fail = 0;

	buffer_init(&b);
	buffer_append(&b, data, datalen);

	if (datafellows & SSH_OLD_SESSIONID) {
		p = buffer_ptr(&b);
		len = buffer_len(&b);
		if ((session_id2 == NULL) ||
		    (len < session_id2_len) ||
		    (memcmp(p, session_id2, session_id2_len) != 0))
			fail++;
		buffer_consume(&b, session_id2_len);
	} else {
		p = buffer_get_string(&b, &len);
		if ((session_id2 == NULL) ||
		    (len != session_id2_len) ||
		    (memcmp(p, session_id2, session_id2_len) != 0))
			fail++;
		xfree(p);
	}
	if (buffer_get_char(&b) != SSH2_MSG_USERAUTH_REQUEST)
		fail++;
	p = buffer_get_string(&b, NULL);
	if (strcmp(authctxt->user, p) != 0) {
		logit("wrong user name passed to monitor: expected %s != %.100s",
		    authctxt->user, p);
		fail++;
	}
	xfree(p);
	buffer_skip_string(&b);
	if (datafellows & SSH_BUG_PKAUTH) {
		if (!buffer_get_char(&b))
			fail++;
	} else {
		p = buffer_get_string(&b, NULL);
		if (strcmp("publickey", p) != 0)
			fail++;
		xfree(p);
		if (!buffer_get_char(&b))
			fail++;
		buffer_skip_string(&b);
	}
	buffer_skip_string(&b);
	if (buffer_len(&b) != 0)
		fail++;
	buffer_free(&b);
	return (fail == 0);
}

static int
monitor_valid_hostbasedblob(u_char *data, u_int datalen, char *cuser,
    char *chost)
{
	Buffer b;
	char *p;
	u_int len;
	int fail = 0;

	buffer_init(&b);
	buffer_append(&b, data, datalen);

	p = buffer_get_string(&b, &len);
	if ((session_id2 == NULL) ||
	    (len != session_id2_len) ||
	    (memcmp(p, session_id2, session_id2_len) != 0))
		fail++;
	xfree(p);

	if (buffer_get_char(&b) != SSH2_MSG_USERAUTH_REQUEST)
		fail++;
	p = buffer_get_string(&b, NULL);
	if (strcmp(authctxt->user, p) != 0) {
		logit("wrong user name passed to monitor: expected %s != %.100s",
		    authctxt->user, p);
		fail++;
	}
	xfree(p);
	buffer_skip_string(&b);	/* service */
	p = buffer_get_string(&b, NULL);
	if (strcmp(p, "hostbased") != 0)
		fail++;
	xfree(p);
	buffer_skip_string(&b);	/* pkalg */
	buffer_skip_string(&b);	/* pkblob */

	/* verify client host, strip trailing dot if necessary */
	p = buffer_get_string(&b, NULL);
	if (((len = strlen(p)) > 0) && p[len - 1] == '.')
		p[len - 1] = '\0';
	if (strcmp(p, chost) != 0)
		fail++;
	xfree(p);

	/* verify client user */
	p = buffer_get_string(&b, NULL);
	if (strcmp(p, cuser) != 0)
		fail++;
	xfree(p);

	if (buffer_len(&b) != 0)
		fail++;
	buffer_free(&b);
	return (fail == 0);
}

int
mm_answer_keyverify(int sock, Buffer *m)
{
	Key *key;
	u_char *signature, *data, *blob;
	u_int signaturelen, datalen, bloblen;
	int verified = 0;
	int valid_data = 0;

	blob = buffer_get_string(m, &bloblen);
	signature = buffer_get_string(m, &signaturelen);
	data = buffer_get_string(m, &datalen);

	if (hostbased_cuser == NULL || hostbased_chost == NULL ||
	  !monitor_allowed_key(blob, bloblen))
		fatal("%s: bad key, not previously allowed", __func__);

	key = key_from_blob(blob, bloblen);
	if (key == NULL)
		fatal("%s: bad public key blob", __func__);

	switch (key_blobtype) {
	case MM_USERKEY:
		valid_data = monitor_valid_userblob(data, datalen);
		break;
	case MM_HOSTKEY:
		valid_data = monitor_valid_hostbasedblob(data, datalen,
		    hostbased_cuser, hostbased_chost);
		break;
	default:
		valid_data = 0;
		break;
	}
	if (!valid_data)
		fatal("%s: bad signature data blob", __func__);

	verified = key_verify(key, signature, signaturelen, data, datalen);
	debug3("%s: key %p signature %s",
	    __func__, key, verified ? "verified" : "unverified");

	key_free(key);
	xfree(blob);
	xfree(signature);
	xfree(data);

	auth_method = key_blobtype == MM_USERKEY ? "publickey" : "hostbased";

	monitor_reset_key_state();

	buffer_clear(m);
	buffer_put_int(m, verified);
	mm_request_send(sock, MONITOR_ANS_KEYVERIFY, m);

	return (verified);
}

static void
mm_record_login(Session *s, struct passwd *pw)
{
	socklen_t fromlen;
	struct sockaddr_storage from;

	/*
	 * Get IP address of client. If the connection is not a socket, let
	 * the address be 0.0.0.0.
	 */
	memset(&from, 0, sizeof(from));
	fromlen = sizeof(from);
	if (packet_connection_is_on_socket()) {
		if (getpeername(packet_get_connection_in(),
			(struct sockaddr *) & from, &fromlen) < 0) {
			debug("getpeername: %.100s", strerror(errno));
			cleanup_exit(255);
		}
	}
	/* Record that there was a login on that tty from the remote host. */
	record_login(s->pid, s->tty, pw->pw_name, pw->pw_uid,
	    get_remote_name_or_ip(utmp_len, options.use_dns),
	    (struct sockaddr *)&from, fromlen);
}

static void
mm_session_close(Session *s)
{
	debug3("%s: session %d pid %ld", __func__, s->self, (long)s->pid);
	if (s->ttyfd != -1) {
		debug3("%s: tty %s ptyfd %d",  __func__, s->tty, s->ptyfd);
		session_pty_cleanup2(s);
	}
	s->used = 0;
}

int
mm_answer_pty(int sock, Buffer *m)
{
	extern struct monitor *pmonitor;
	Session *s;
	int res, fd0;

	debug3("%s entering", __func__);

	buffer_clear(m);
	s = session_new();
	if (s == NULL)
		goto error;
	s->authctxt = authctxt;
	s->pw = authctxt->pw;
	s->pid = pmonitor->m_pid;
	res = pty_allocate(&s->ptyfd, &s->ttyfd, s->tty, sizeof(s->tty));
	if (res == 0)
		goto error;
	pty_setowner(authctxt->pw, s->tty);

	buffer_put_int(m, 1);
	buffer_put_cstring(m, s->tty);

	/* We need to trick ttyslot */
	if (dup2(s->ttyfd, 0) == -1)
		fatal("%s: dup2", __func__);

	mm_record_login(s, authctxt->pw);

	/* Now we can close the file descriptor again */
	close(0);

	/* send messages generated by record_login */
	buffer_put_string(m, buffer_ptr(&loginmsg), buffer_len(&loginmsg));
	buffer_clear(&loginmsg);

	mm_request_send(sock, MONITOR_ANS_PTY, m);

	mm_send_fd(sock, s->ptyfd);
	mm_send_fd(sock, s->ttyfd);

	/* make sure nothing uses fd 0 */
	if ((fd0 = open(_PATH_DEVNULL, O_RDONLY)) < 0)
		fatal("%s: open(/dev/null): %s", __func__, strerror(errno));
	if (fd0 != 0)
		error("%s: fd0 %d != 0", __func__, fd0);

	/* slave is not needed */
	close(s->ttyfd);
	s->ttyfd = s->ptyfd;
	/* no need to dup() because nobody closes ptyfd */
	s->ptymaster = s->ptyfd;

	debug3("%s: tty %s ptyfd %d",  __func__, s->tty, s->ttyfd);

	return (0);

 error:
	if (s != NULL)
		mm_session_close(s);
	buffer_put_int(m, 0);
	mm_request_send(sock, MONITOR_ANS_PTY, m);
	return (0);
}

int
mm_answer_pty_cleanup(int sock, Buffer *m)
{
	Session *s;
	char *tty;

	debug3("%s entering", __func__);

	tty = buffer_get_string(m, NULL);
	if ((s = session_by_tty(tty)) != NULL)
		mm_session_close(s);
	buffer_clear(m);
	xfree(tty);
	return (0);
}

int
mm_answer_sesskey(int sock, Buffer *m)
{
	BIGNUM *p;
	int rsafail;

	/* Turn off permissions */
	monitor_permit(mon_dispatch, MONITOR_REQ_SESSKEY, 0);

	if ((p = BN_new()) == NULL)
		fatal("%s: BN_new", __func__);

	buffer_get_bignum2(m, p);

	rsafail = ssh1_session_key(p);

	buffer_clear(m);
	buffer_put_int(m, rsafail);
	buffer_put_bignum2(m, p);

	BN_clear_free(p);

	mm_request_send(sock, MONITOR_ANS_SESSKEY, m);

	/* Turn on permissions for sessid passing */
	monitor_permit(mon_dispatch, MONITOR_REQ_SESSID, 1);

	return (0);
}

int
mm_answer_sessid(int sock, Buffer *m)
{
	int i;

	debug3("%s entering", __func__);

	if (buffer_len(m) != 16)
		fatal("%s: bad ssh1 session id", __func__);
	for (i = 0; i < 16; i++)
		session_id[i] = buffer_get_char(m);

	/* Turn on permissions for getpwnam */
	monitor_permit(mon_dispatch, MONITOR_REQ_PWNAM, 1);

	return (0);
}

int
mm_answer_rsa_keyallowed(int sock, Buffer *m)
{
	BIGNUM *client_n;
	Key *key = NULL;
	u_char *blob = NULL;
	u_int blen = 0;
	int allowed = 0;

	debug3("%s entering", __func__);

	if (options.rsa_authentication && authctxt->valid) {
		if ((client_n = BN_new()) == NULL)
			fatal("%s: BN_new", __func__);
		buffer_get_bignum2(m, client_n);
		allowed = auth_rsa_key_allowed(authctxt->pw, client_n, &key);
		BN_clear_free(client_n);
	}
	buffer_clear(m);
	buffer_put_int(m, allowed);
	buffer_put_int(m, forced_command != NULL);

	/* clear temporarily storage (used by generate challenge) */
	monitor_reset_key_state();

	if (allowed && key != NULL) {
		key->type = KEY_RSA;	/* cheat for key_to_blob */
		if (key_to_blob(key, &blob, &blen) == 0)
			fatal("%s: key_to_blob failed", __func__);
		buffer_put_string(m, blob, blen);

		/* Save temporarily for comparison in verify */
		key_blob = blob;
		key_bloblen = blen;
		key_blobtype = MM_RSAUSERKEY;
	}
	if (key != NULL)
		key_free(key);

	mm_append_debug(m);

	mm_request_send(sock, MONITOR_ANS_RSAKEYALLOWED, m);

	monitor_permit(mon_dispatch, MONITOR_REQ_RSACHALLENGE, allowed);
	monitor_permit(mon_dispatch, MONITOR_REQ_RSARESPONSE, 0);
	return (0);
}

int
mm_answer_rsa_challenge(int sock, Buffer *m)
{
	Key *key = NULL;
	u_char *blob;
	u_int blen;

	debug3("%s entering", __func__);

	if (!authctxt->valid)
		fatal("%s: authctxt not valid", __func__);
	blob = buffer_get_string(m, &blen);
	if (!monitor_allowed_key(blob, blen))
		fatal("%s: bad key, not previously allowed", __func__);
	if (key_blobtype != MM_RSAUSERKEY && key_blobtype != MM_RSAHOSTKEY)
		fatal("%s: key type mismatch", __func__);
	if ((key = key_from_blob(blob, blen)) == NULL)
		fatal("%s: received bad key", __func__);

	if (ssh1_challenge)
		BN_clear_free(ssh1_challenge);
	ssh1_challenge = auth_rsa_generate_challenge(key);

	buffer_clear(m);
	buffer_put_bignum2(m, ssh1_challenge);

	debug3("%s sending reply", __func__);
	mm_request_send(sock, MONITOR_ANS_RSACHALLENGE, m);

	monitor_permit(mon_dispatch, MONITOR_REQ_RSARESPONSE, 1);

	xfree(blob);
	key_free(key);
	return (0);
}

int
mm_answer_rsa_response(int sock, Buffer *m)
{
	Key *key = NULL;
	u_char *blob, *response;
	u_int blen, len;
	int success;

	debug3("%s entering", __func__);

	if (!authctxt->valid)
		fatal("%s: authctxt not valid", __func__);
	if (ssh1_challenge == NULL)
		fatal("%s: no ssh1_challenge", __func__);

	blob = buffer_get_string(m, &blen);
	if (!monitor_allowed_key(blob, blen))
		fatal("%s: bad key, not previously allowed", __func__);
	if (key_blobtype != MM_RSAUSERKEY && key_blobtype != MM_RSAHOSTKEY)
		fatal("%s: key type mismatch: %d", __func__, key_blobtype);
	if ((key = key_from_blob(blob, blen)) == NULL)
		fatal("%s: received bad key", __func__);
	response = buffer_get_string(m, &len);
	if (len != 16)
		fatal("%s: received bad response to challenge", __func__);
	success = auth_rsa_verify_response(key, ssh1_challenge, response);

	xfree(blob);
	key_free(key);
	xfree(response);

	auth_method = key_blobtype == MM_RSAUSERKEY ? "rsa" : "rhosts-rsa";

	/* reset state */
	BN_clear_free(ssh1_challenge);
	ssh1_challenge = NULL;
	monitor_reset_key_state();

	buffer_clear(m);
	buffer_put_int(m, success);
	mm_request_send(sock, MONITOR_ANS_RSARESPONSE, m);

	return (success);
}

int
mm_answer_term(int sock, Buffer *req)
{
	extern struct monitor *pmonitor;
	int res, status;

	debug3("%s: tearing down sessions", __func__);

	/* The child is terminating */
	session_destroy_all(&mm_session_close);

	while (waitpid(pmonitor->m_pid, &status, 0) == -1)
		if (errno != EINTR)
			exit(1);

	res = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

	/* Terminate process */
	exit(res);
}

#ifdef SSH_AUDIT_EVENTS
/* Report that an audit event occurred */
int
mm_answer_audit_event(int socket, Buffer *m)
{
	ssh_audit_event_t event;

	debug3("%s entering", __func__);

	event = buffer_get_int(m);
	buffer_free(m);
	switch(event) {
	case SSH_AUTH_FAIL_PUBKEY:
	case SSH_AUTH_FAIL_HOSTBASED:
	case SSH_AUTH_FAIL_GSSAPI:
	case SSH_LOGIN_EXCEED_MAXTRIES:
	case SSH_LOGIN_ROOT_DENIED:
	case SSH_CONNECTION_CLOSE:
	case SSH_INVALID_USER:
		audit_event(event);
		break;
	default:
		fatal("Audit event type %d not permitted", event);
	}

	return (0);
}

int
mm_answer_audit_command(int socket, Buffer *m)
{
	u_int len;
	char *cmd;

	debug3("%s entering", __func__);
	cmd = buffer_get_string(m, &len);
	/* sanity check command, if so how? */
	audit_run_command(cmd);
	xfree(cmd);
	buffer_free(m);
	return (0);
}
#endif /* SSH_AUDIT_EVENTS */

void
monitor_apply_keystate(struct monitor *pmonitor)
{
	if (compat20) {
		set_newkeys(MODE_IN);
		set_newkeys(MODE_OUT);
	} else {
		packet_set_protocol_flags(child_state.ssh1protoflags);
		packet_set_encryption_key(child_state.ssh1key,
		    child_state.ssh1keylen, child_state.ssh1cipher);
		xfree(child_state.ssh1key);
	}

	/* for rc4 and other stateful ciphers */
	packet_set_keycontext(MODE_OUT, child_state.keyout);
	xfree(child_state.keyout);
	packet_set_keycontext(MODE_IN, child_state.keyin);
	xfree(child_state.keyin);

	if (!compat20) {
		packet_set_iv(MODE_OUT, child_state.ivout);
		xfree(child_state.ivout);
		packet_set_iv(MODE_IN, child_state.ivin);
		xfree(child_state.ivin);
	}

	memcpy(&incoming_stream, &child_state.incoming,
	    sizeof(incoming_stream));
	memcpy(&outgoing_stream, &child_state.outgoing,
	    sizeof(outgoing_stream));

	/* Update with new address */
	if (options.compression)
		mm_init_compression(pmonitor->m_zlib);

	/* Network I/O buffers */
	/* XXX inefficient for large buffers, need: buffer_init_from_string */
	buffer_clear(&input);
	buffer_append(&input, child_state.input, child_state.ilen);
	memset(child_state.input, 0, child_state.ilen);
	xfree(child_state.input);

	buffer_clear(&output);
	buffer_append(&output, child_state.output, child_state.olen);
	memset(child_state.output, 0, child_state.olen);
	xfree(child_state.output);
}

static Kex *
mm_get_kex(Buffer *m)
{
	Kex *kex;
	void *blob;
	u_int bloblen;

	kex = xmalloc(sizeof(*kex));
	memset(kex, 0, sizeof(*kex));
	kex->session_id = buffer_get_string(m, &kex->session_id_len);
	if ((session_id2 == NULL) ||
	    (kex->session_id_len != session_id2_len) ||
	    (memcmp(kex->session_id, session_id2, session_id2_len) != 0))
		fatal("mm_get_get: internal error: bad session id");
	kex->we_need = buffer_get_int(m);
	kex->kex[KEX_DH_GRP1_SHA1] = kexdh_server;
	kex->kex[KEX_DH_GRP14_SHA1] = kexdh_server;
	kex->kex[KEX_DH_GEX_SHA1] = kexgex_server;
	kex->server = 1;
	kex->hostkey_type = buffer_get_int(m);
	kex->kex_type = buffer_get_int(m);
	blob = buffer_get_string(m, &bloblen);
	buffer_init(&kex->my);
	buffer_append(&kex->my, blob, bloblen);
	xfree(blob);
	blob = buffer_get_string(m, &bloblen);
	buffer_init(&kex->peer);
	buffer_append(&kex->peer, blob, bloblen);
	xfree(blob);
	kex->done = 1;
	kex->flags = buffer_get_int(m);
	kex->client_version_string = buffer_get_string(m, NULL);
	kex->server_version_string = buffer_get_string(m, NULL);
	kex->load_host_key=&get_hostkey_by_type;
	kex->host_key_index=&get_hostkey_index;

	return (kex);
}

/* This function requries careful sanity checking */

void
mm_get_keystate(struct monitor *pmonitor)
{
	Buffer m;
	u_char *blob, *p;
	u_int bloblen, plen;
	u_int32_t seqnr, packets;
	u_int64_t blocks;

	debug3("%s: Waiting for new keys", __func__);

	buffer_init(&m);
	mm_request_receive_expect(pmonitor->m_sendfd, MONITOR_REQ_KEYEXPORT, &m);
	if (!compat20) {
		child_state.ssh1protoflags = buffer_get_int(&m);
		child_state.ssh1cipher = buffer_get_int(&m);
		child_state.ssh1key = buffer_get_string(&m,
		    &child_state.ssh1keylen);
		child_state.ivout = buffer_get_string(&m,
		    &child_state.ivoutlen);
		child_state.ivin = buffer_get_string(&m, &child_state.ivinlen);
		goto skip;
	} else {
		/* Get the Kex for rekeying */
		*pmonitor->m_pkex = mm_get_kex(&m);
	}

	blob = buffer_get_string(&m, &bloblen);
	current_keys[MODE_OUT] = mm_newkeys_from_blob(blob, bloblen);
	xfree(blob);

	debug3("%s: Waiting for second key", __func__);
	blob = buffer_get_string(&m, &bloblen);
	current_keys[MODE_IN] = mm_newkeys_from_blob(blob, bloblen);
	xfree(blob);

	/* Now get sequence numbers for the packets */
	seqnr = buffer_get_int(&m);
	blocks = buffer_get_int64(&m);
	packets = buffer_get_int(&m);
	packet_set_state(MODE_OUT, seqnr, blocks, packets);
	seqnr = buffer_get_int(&m);
	blocks = buffer_get_int64(&m);
	packets = buffer_get_int(&m);
	packet_set_state(MODE_IN, seqnr, blocks, packets);

 skip:
	/* Get the key context */
	child_state.keyout = buffer_get_string(&m, &child_state.keyoutlen);
	child_state.keyin  = buffer_get_string(&m, &child_state.keyinlen);

	debug3("%s: Getting compression state", __func__);
	/* Get compression state */
	p = buffer_get_string(&m, &plen);
	if (plen != sizeof(child_state.outgoing))
		fatal("%s: bad request size", __func__);
	memcpy(&child_state.outgoing, p, sizeof(child_state.outgoing));
	xfree(p);

	p = buffer_get_string(&m, &plen);
	if (plen != sizeof(child_state.incoming))
		fatal("%s: bad request size", __func__);
	memcpy(&child_state.incoming, p, sizeof(child_state.incoming));
	xfree(p);

	/* Network I/O buffers */
	debug3("%s: Getting Network I/O buffers", __func__);
	child_state.input = buffer_get_string(&m, &child_state.ilen);
	child_state.output = buffer_get_string(&m, &child_state.olen);

	buffer_free(&m);
}


/* Allocation functions for zlib */
void *
mm_zalloc(struct mm_master *mm, u_int ncount, u_int size)
{
	size_t len = (size_t) size * ncount;
	void *address;

	if (len == 0 || ncount > SIZE_T_MAX / size)
		fatal("%s: mm_zalloc(%u, %u)", __func__, ncount, size);

	address = mm_malloc(mm, len);

	return (address);
}

void
mm_zfree(struct mm_master *mm, void *address)
{
	mm_free(mm, address);
}

void
mm_init_compression(struct mm_master *mm)
{
	outgoing_stream.zalloc = (alloc_func)mm_zalloc;
	outgoing_stream.zfree = (free_func)mm_zfree;
	outgoing_stream.opaque = mm;

	incoming_stream.zalloc = (alloc_func)mm_zalloc;
	incoming_stream.zfree = (free_func)mm_zfree;
	incoming_stream.opaque = mm;
}

/* XXX */

#define FD_CLOSEONEXEC(x) do { \
	if (fcntl(x, F_SETFD, 1) == -1) \
		fatal("fcntl(%d, F_SETFD)", x); \
} while (0)

static void
monitor_socketpair(int *pair)
{
#ifdef HAVE_SOCKETPAIR
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
		fatal("%s: socketpair", __func__);
#else
	fatal("%s: UsePrivilegeSeparation=yes not supported",
	    __func__);
#endif
	FD_CLOSEONEXEC(pair[0]);
	FD_CLOSEONEXEC(pair[1]);
}

#define MM_MEMSIZE	65536

struct monitor *
monitor_init(void)
{
	struct monitor *mon;
	int pair[2];

	mon = xmalloc(sizeof(*mon));

	mon->m_pid = 0;
	monitor_socketpair(pair);

	mon->m_recvfd = pair[0];
	mon->m_sendfd = pair[1];

	/* Used to share zlib space across processes */
	if (options.compression) {
		mon->m_zback = mm_create(NULL, MM_MEMSIZE);
		mon->m_zlib = mm_create(mon->m_zback, 20 * MM_MEMSIZE);

		/* Compression needs to share state across borders */
		mm_init_compression(mon->m_zlib);
	}

	return mon;
}

void
monitor_reinit(struct monitor *mon)
{
	int pair[2];

	monitor_socketpair(pair);

	mon->m_recvfd = pair[0];
	mon->m_sendfd = pair[1];
}

#ifdef GSSAPI
int
mm_answer_gss_setup_ctx(int sock, Buffer *m)
{
	gss_OID_desc goid;
	OM_uint32 major;
	u_int len;

	goid.elements = buffer_get_string(m, &len);
	goid.length = len;

	major = ssh_gssapi_server_ctx(&gsscontext, &goid);

	xfree(goid.elements);

	buffer_clear(m);
	buffer_put_int(m, major);

	mm_request_send(sock,MONITOR_ANS_GSSSETUP, m);

	/* Now we have a context, enable the step */
	monitor_permit(mon_dispatch, MONITOR_REQ_GSSSTEP, 1);

	return (0);
}

int
mm_answer_gss_accept_ctx(int sock, Buffer *m)
{
	gss_buffer_desc in;
	gss_buffer_desc out = GSS_C_EMPTY_BUFFER;
	OM_uint32 major,minor;
	OM_uint32 flags = 0; /* GSI needs this */
	u_int len;

	in.value = buffer_get_string(m, &len);
	in.length = len;
	major = ssh_gssapi_accept_ctx(gsscontext, &in, &out, &flags);
	xfree(in.value);

	buffer_clear(m);
	buffer_put_int(m, major);
	buffer_put_string(m, out.value, out.length);
	buffer_put_int(m, flags);
	mm_request_send(sock, MONITOR_ANS_GSSSTEP, m);

	gss_release_buffer(&minor, &out);

	if (major==GSS_S_COMPLETE) {
		monitor_permit(mon_dispatch, MONITOR_REQ_GSSSTEP, 0);
		monitor_permit(mon_dispatch, MONITOR_REQ_GSSUSEROK, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_GSSCHECKMIC, 1);
	}
	return (0);
}

int
mm_answer_gss_checkmic(int sock, Buffer *m)
{
	gss_buffer_desc gssbuf, mic;
	OM_uint32 ret;
	u_int len;

	gssbuf.value = buffer_get_string(m, &len);
	gssbuf.length = len;
	mic.value = buffer_get_string(m, &len);
	mic.length = len;

	ret = ssh_gssapi_checkmic(gsscontext, &gssbuf, &mic);

	xfree(gssbuf.value);
	xfree(mic.value);

	buffer_clear(m);
	buffer_put_int(m, ret);

	mm_request_send(sock, MONITOR_ANS_GSSCHECKMIC, m);

	if (!GSS_ERROR(ret))
		monitor_permit(mon_dispatch, MONITOR_REQ_GSSUSEROK, 1);

	return (0);
}

int
mm_answer_gss_userok(int sock, Buffer *m)
{
	int authenticated;

	authenticated = authctxt->valid && ssh_gssapi_userok(authctxt->user);

	buffer_clear(m);
	buffer_put_int(m, authenticated);

	debug3("%s: sending result %d", __func__, authenticated);
	mm_request_send(sock, MONITOR_ANS_GSSUSEROK, m);

	auth_method="gssapi-with-mic";

	/* Monitor loop will terminate if authenticated */
	return (authenticated);
}
#endif /* GSSAPI */
