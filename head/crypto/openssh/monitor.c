/* $OpenBSD: monitor.c,v 1.167 2017/02/03 23:05:57 djm Exp $ */
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

#include <sys/types.h>
#include <sys/socket.h>
#include "openbsd-compat/sys-tree.h"
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <pwd.h>
#include <signal.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#else
# ifdef HAVE_SYS_POLL_H
#  include <sys/poll.h>
# endif
#endif

#ifdef SKEY
#include <skey.h>
#endif

#ifdef WITH_OPENSSL
#include <openssl/dh.h>
#endif

#include "openbsd-compat/sys-queue.h"
#include "atomicio.h"
#include "xmalloc.h"
#include "ssh.h"
#include "key.h"
#include "buffer.h"
#include "hostfile.h"
#include "auth.h"
#include "cipher.h"
#include "kex.h"
#include "dh.h"
#include "auth-pam.h"
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
#include "misc.h"
#include "servconf.h"
#include "monitor.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "monitor_fdpass.h"
#include "compat.h"
#include "ssh2.h"
#include "authfd.h"
#include "match.h"
#include "ssherr.h"

#ifdef GSSAPI
static Gssctxt *gsscontext = NULL;
#endif

/* Imports */
extern ServerOptions options;
extern u_int utmp_len;
extern u_char session_id[];
extern Buffer auth_debug;
extern int auth_debug_init;
extern Buffer loginmsg;

/* State exported from the child */
static struct sshbuf *child_state;

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

static int monitor_read_log(struct monitor *);

static Authctxt *authctxt;

/* local state for key verify */
static u_char *key_blob = NULL;
static u_int key_bloblen = 0;
static int key_blobtype = MM_NOKEY;
static char *hostbased_cuser = NULL;
static char *hostbased_chost = NULL;
static char *auth_method = "unknown";
static char *auth_submethod = NULL;
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
#define MON_ALOG	0x0020	/* Log auth attempt without authenticating */

#define MON_AUTH	(MON_ISAUTH|MON_AUTHDECIDE)

#define MON_PERMIT	0x1000	/* Request is permitted */

struct mon_table mon_dispatch_proto20[] = {
#ifdef WITH_OPENSSL
    {MONITOR_REQ_MODULI, MON_ONCE, mm_answer_moduli},
#endif
    {MONITOR_REQ_SIGN, MON_ONCE, mm_answer_sign},
    {MONITOR_REQ_PWNAM, MON_ONCE, mm_answer_pwnamallow},
    {MONITOR_REQ_AUTHSERV, MON_ONCE, mm_answer_authserv},
    {MONITOR_REQ_AUTH2_READ_BANNER, MON_ONCE, mm_answer_auth2_read_banner},
    {MONITOR_REQ_AUTHPASSWORD, MON_AUTH, mm_answer_authpassword},
#ifdef USE_PAM
    {MONITOR_REQ_PAM_START, MON_ONCE, mm_answer_pam_start},
    {MONITOR_REQ_PAM_ACCOUNT, 0, mm_answer_pam_account},
    {MONITOR_REQ_PAM_INIT_CTX, MON_ONCE, mm_answer_pam_init_ctx},
    {MONITOR_REQ_PAM_QUERY, 0, mm_answer_pam_query},
    {MONITOR_REQ_PAM_RESPOND, MON_ONCE, mm_answer_pam_respond},
    {MONITOR_REQ_PAM_FREE_CTX, MON_ONCE|MON_AUTHDECIDE, mm_answer_pam_free_ctx},
#endif
#ifdef SSH_AUDIT_EVENTS
    {MONITOR_REQ_AUDIT_EVENT, MON_PERMIT, mm_answer_audit_event},
#endif
#ifdef BSD_AUTH
    {MONITOR_REQ_BSDAUTHQUERY, MON_ISAUTH, mm_answer_bsdauthquery},
    {MONITOR_REQ_BSDAUTHRESPOND, MON_AUTH, mm_answer_bsdauthrespond},
#endif
#ifdef SKEY
    {MONITOR_REQ_SKEYQUERY, MON_ISAUTH, mm_answer_skeyquery},
    {MONITOR_REQ_SKEYRESPOND, MON_AUTH, mm_answer_skeyrespond},
#endif
    {MONITOR_REQ_KEYALLOWED, MON_ISAUTH, mm_answer_keyallowed},
    {MONITOR_REQ_KEYVERIFY, MON_AUTH, mm_answer_keyverify},
#ifdef GSSAPI
    {MONITOR_REQ_GSSSETUP, MON_ISAUTH, mm_answer_gss_setup_ctx},
    {MONITOR_REQ_GSSSTEP, 0, mm_answer_gss_accept_ctx},
    {MONITOR_REQ_GSSUSEROK, MON_ONCE|MON_AUTHDECIDE, mm_answer_gss_userok},
    {MONITOR_REQ_GSSCHECKMIC, MON_ONCE, mm_answer_gss_checkmic},
#endif
    {0, 0, NULL}
};

struct mon_table mon_dispatch_postauth20[] = {
#ifdef WITH_OPENSSL
    {MONITOR_REQ_MODULI, 0, mm_answer_moduli},
#endif
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
	struct ssh *ssh = active_state;	/* XXX */
	struct mon_table *ent;
	int authenticated = 0, partial = 0;

	debug3("preauth child monitor started");

	close(pmonitor->m_recvfd);
	close(pmonitor->m_log_sendfd);
	pmonitor->m_log_sendfd = pmonitor->m_recvfd = -1;

	authctxt = _authctxt;
	memset(authctxt, 0, sizeof(*authctxt));

	authctxt->loginmsg = &loginmsg;

	mon_dispatch = mon_dispatch_proto20;
	/* Permit requests for moduli and signatures */
	monitor_permit(mon_dispatch, MONITOR_REQ_MODULI, 1);
	monitor_permit(mon_dispatch, MONITOR_REQ_SIGN, 1);

	/* The first few requests do not require asynchronous access */
	while (!authenticated) {
		partial = 0;
		auth_method = "unknown";
		auth_submethod = NULL;
		authenticated = (monitor_read(pmonitor, mon_dispatch, &ent) == 1);

		/* Special handling for multiple required authentications */
		if (options.num_auth_methods != 0) {
			if (authenticated &&
			    !auth2_update_methods_lists(authctxt,
			    auth_method, auth_submethod)) {
				debug3("%s: method %s: partial", __func__,
				    auth_method);
				authenticated = 0;
				partial = 1;
			}
		}

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
		if (ent->flags & (MON_AUTHDECIDE|MON_ALOG)) {
			auth_log(authctxt, authenticated, partial,
			    auth_method, auth_submethod);
			if (!partial && !authenticated)
				authctxt->failures++;
		}
	}

	if (!authctxt->valid)
		fatal("%s: authenticated invalid user", __func__);
	if (strcmp(auth_method, "unknown") == 0)
		fatal("%s: authentication method name unknown", __func__);

	debug("%s: %s has been authenticated by privileged process",
	    __func__, authctxt->user);
	ssh_packet_set_log_preamble(ssh, "user %s", authctxt->user);

	mm_get_keystate(pmonitor);

	/* Drain any buffered messages from the child */
	while (pmonitor->m_log_recvfd != -1 && monitor_read_log(pmonitor) == 0)
		;

	close(pmonitor->m_sendfd);
	close(pmonitor->m_log_recvfd);
	pmonitor->m_sendfd = pmonitor->m_log_recvfd = -1;
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
	close(pmonitor->m_recvfd);
	pmonitor->m_recvfd = -1;

	monitor_set_child_handler(pmonitor->m_pid);
	signal(SIGHUP, &monitor_child_handler);
	signal(SIGTERM, &monitor_child_handler);
	signal(SIGINT, &monitor_child_handler);
#ifdef SIGXFSZ
	signal(SIGXFSZ, SIG_IGN);
#endif

	mon_dispatch = mon_dispatch_postauth20;

	/* Permit requests for moduli and signatures */
	monitor_permit(mon_dispatch, MONITOR_REQ_MODULI, 1);
	monitor_permit(mon_dispatch, MONITOR_REQ_SIGN, 1);
	monitor_permit(mon_dispatch, MONITOR_REQ_TERM, 1);

	if (!no_pty_flag) {
		monitor_permit(mon_dispatch, MONITOR_REQ_PTY, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_PTYCLEANUP, 1);
	}

	for (;;)
		monitor_read(pmonitor, mon_dispatch, NULL);
}

static int
monitor_read_log(struct monitor *pmonitor)
{
	Buffer logmsg;
	u_int len, level;
	char *msg;

	buffer_init(&logmsg);

	/* Read length */
	buffer_append_space(&logmsg, 4);
	if (atomicio(read, pmonitor->m_log_recvfd,
	    buffer_ptr(&logmsg), buffer_len(&logmsg)) != buffer_len(&logmsg)) {
		if (errno == EPIPE) {
			buffer_free(&logmsg);
			debug("%s: child log fd closed", __func__);
			close(pmonitor->m_log_recvfd);
			pmonitor->m_log_recvfd = -1;
			return -1;
		}
		fatal("%s: log fd read: %s", __func__, strerror(errno));
	}
	len = buffer_get_int(&logmsg);
	if (len <= 4 || len > 8192)
		fatal("%s: invalid log message length %u", __func__, len);

	/* Read severity, message */
	buffer_clear(&logmsg);
	buffer_append_space(&logmsg, len);
	if (atomicio(read, pmonitor->m_log_recvfd,
	    buffer_ptr(&logmsg), buffer_len(&logmsg)) != buffer_len(&logmsg))
		fatal("%s: log fd read: %s", __func__, strerror(errno));

	/* Log it */
	level = buffer_get_int(&logmsg);
	msg = buffer_get_string(&logmsg, NULL);
	if (log_level_name(level) == NULL)
		fatal("%s: invalid log level %u (corrupted message?)",
		    __func__, level);
	do_log2(level, "%s [preauth]", msg);

	buffer_free(&logmsg);
	free(msg);

	return 0;
}

int
monitor_read(struct monitor *pmonitor, struct mon_table *ent,
    struct mon_table **pent)
{
	Buffer m;
	int ret;
	u_char type;
	struct pollfd pfd[2];

	for (;;) {
		memset(&pfd, 0, sizeof(pfd));
		pfd[0].fd = pmonitor->m_sendfd;
		pfd[0].events = POLLIN;
		pfd[1].fd = pmonitor->m_log_recvfd;
		pfd[1].events = pfd[1].fd == -1 ? 0 : POLLIN;
		if (poll(pfd, pfd[1].fd == -1 ? 1 : 2, -1) == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			fatal("%s: poll: %s", __func__, strerror(errno));
		}
		if (pfd[1].revents) {
			/*
			 * Drain all log messages before processing next
			 * monitor request.
			 */
			monitor_read_log(pmonitor);
			continue;
		}
		if (pfd[0].revents)
			break;  /* Continues below */
	}

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
	    timingsafe_bcmp(key_blob, blob, key_bloblen))
		return (0);
	return (1);
}

static void
monitor_reset_key_state(void)
{
	/* reset state */
	free(key_blob);
	free(hostbased_cuser);
	free(hostbased_chost);
	key_blob = NULL;
	key_bloblen = 0;
	key_blobtype = MM_NOKEY;
	hostbased_cuser = NULL;
	hostbased_chost = NULL;
}

#ifdef WITH_OPENSSL
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
#endif

int
mm_answer_sign(int sock, Buffer *m)
{
	struct ssh *ssh = active_state; 	/* XXX */
	extern int auth_sock;			/* XXX move to state struct? */
	struct sshkey *key;
	struct sshbuf *sigbuf = NULL;
	u_char *p = NULL, *signature = NULL;
	char *alg = NULL;
	size_t datlen, siglen, alglen;
	int r, is_proof = 0;
	u_int keyid;
	const char proof_req[] = "hostkeys-prove-00@openssh.com";

	debug3("%s", __func__);

	if ((r = sshbuf_get_u32(m, &keyid)) != 0 ||
	    (r = sshbuf_get_string(m, &p, &datlen)) != 0 ||
	    (r = sshbuf_get_cstring(m, &alg, &alglen)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	if (keyid > INT_MAX)
		fatal("%s: invalid key ID", __func__);

	/*
	 * Supported KEX types use SHA1 (20 bytes), SHA256 (32 bytes),
	 * SHA384 (48 bytes) and SHA512 (64 bytes).
	 *
	 * Otherwise, verify the signature request is for a hostkey
	 * proof.
	 *
	 * XXX perform similar check for KEX signature requests too?
	 * it's not trivial, since what is signed is the hash, rather
	 * than the full kex structure...
	 */
	if (datlen != 20 && datlen != 32 && datlen != 48 && datlen != 64) {
		/*
		 * Construct expected hostkey proof and compare it to what
		 * the client sent us.
		 */
		if (session_id2_len == 0) /* hostkeys is never first */
			fatal("%s: bad data length: %zu", __func__, datlen);
		if ((key = get_hostkey_public_by_index(keyid, ssh)) == NULL)
			fatal("%s: no hostkey for index %d", __func__, keyid);
		if ((sigbuf = sshbuf_new()) == NULL)
			fatal("%s: sshbuf_new", __func__);
		if ((r = sshbuf_put_cstring(sigbuf, proof_req)) != 0 ||
		    (r = sshbuf_put_string(sigbuf, session_id2,
		    session_id2_len)) != 0 ||
		    (r = sshkey_puts(key, sigbuf)) != 0)
			fatal("%s: couldn't prepare private key "
			    "proof buffer: %s", __func__, ssh_err(r));
		if (datlen != sshbuf_len(sigbuf) ||
		    memcmp(p, sshbuf_ptr(sigbuf), sshbuf_len(sigbuf)) != 0)
			fatal("%s: bad data length: %zu, hostkey proof len %zu",
			    __func__, datlen, sshbuf_len(sigbuf));
		sshbuf_free(sigbuf);
		is_proof = 1;
	}

	/* save session id, it will be passed on the first call */
	if (session_id2_len == 0) {
		session_id2_len = datlen;
		session_id2 = xmalloc(session_id2_len);
		memcpy(session_id2, p, session_id2_len);
	}

	if ((key = get_hostkey_by_index(keyid)) != NULL) {
		if ((r = sshkey_sign(key, &signature, &siglen, p, datlen, alg,
		    datafellows)) != 0)
			fatal("%s: sshkey_sign failed: %s",
			    __func__, ssh_err(r));
	} else if ((key = get_hostkey_public_by_index(keyid, ssh)) != NULL &&
	    auth_sock > 0) {
		if ((r = ssh_agent_sign(auth_sock, key, &signature, &siglen,
		    p, datlen, alg, datafellows)) != 0) {
			fatal("%s: ssh_agent_sign failed: %s",
			    __func__, ssh_err(r));
		}
	} else
		fatal("%s: no hostkey from index %d", __func__, keyid);

	debug3("%s: %s signature %p(%zu)", __func__,
	    is_proof ? "KEX" : "hostkey proof", signature, siglen);

	sshbuf_reset(m);
	if ((r = sshbuf_put_string(m, signature, siglen)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	free(alg);
	free(p);
	free(signature);

	mm_request_send(sock, MONITOR_ANS_SIGN, m);

	/* Turn on permissions for getpwnam */
	monitor_permit(mon_dispatch, MONITOR_REQ_PWNAM, 1);

	return (0);
}

/* Retrieves the password entry and also checks if the user is permitted */

int
mm_answer_pwnamallow(int sock, Buffer *m)
{
	struct ssh *ssh = active_state;	/* XXX */
	char *username;
	struct passwd *pwent;
	int allowed = 0;
	u_int i;

	debug3("%s", __func__);

	if (authctxt->attempt++ != 0)
		fatal("%s: multiple attempts for getpwnam", __func__);

	username = buffer_get_string(m, NULL);

	pwent = getpwnamallow(username);

	authctxt->user = xstrdup(username);
	setproctitle("%s [priv]", pwent ? username : "unknown");
	free(username);

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
#ifdef HAVE_STRUCT_PASSWD_PW_GECOS
	buffer_put_cstring(m, pwent->pw_gecos);
#endif
#ifdef HAVE_STRUCT_PASSWD_PW_CLASS
	buffer_put_cstring(m, pwent->pw_class);
#endif
	buffer_put_cstring(m, pwent->pw_dir);
	buffer_put_cstring(m, pwent->pw_shell);

 out:
	ssh_packet_set_log_preamble(ssh, "%suser %s",
	    authctxt->valid ? "authenticating" : "invalid ", authctxt->user);
	buffer_put_string(m, &options, sizeof(options));

#define M_CP_STROPT(x) do { \
		if (options.x != NULL) \
			buffer_put_cstring(m, options.x); \
	} while (0)
#define M_CP_STRARRAYOPT(x, nx) do { \
		for (i = 0; i < options.nx; i++) \
			buffer_put_cstring(m, options.x[i]); \
	} while (0)
	/* See comment in servconf.h */
	COPY_MATCH_STRING_OPTS();
#undef M_CP_STROPT
#undef M_CP_STRARRAYOPT

	/* Create valid auth method lists */
	if (auth2_setup_methods_lists(authctxt) != 0) {
		/*
		 * The monitor will continue long enough to let the child
		 * run to it's packet_disconnect(), but it must not allow any
		 * authentication to succeed.
		 */
		debug("%s: no valid authentication method lists", __func__);
	}

	debug3("%s: sending MONITOR_ANS_PWNAM: %d", __func__, allowed);
	mm_request_send(sock, MONITOR_ANS_PWNAM, m);

	/* Allow service/style information on the auth context */
	monitor_permit(mon_dispatch, MONITOR_REQ_AUTHSERV, 1);
	monitor_permit(mon_dispatch, MONITOR_REQ_AUTH2_READ_BANNER, 1);

#ifdef USE_PAM
	if (options.use_pam)
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_START, 1);
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
	free(banner);

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
		free(authctxt->style);
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

	if (!options.password_authentication)
		fatal("%s: password authentication not enabled", __func__);
	passwd = buffer_get_string(m, &plen);
	/* Only authenticate if the context is valid */
	authenticated = options.password_authentication &&
	    auth_password(authctxt, passwd);
	explicit_bzero(passwd, strlen(passwd));
	free(passwd);

	buffer_clear(m);
	buffer_put_int(m, authenticated);
#ifdef USE_PAM
	buffer_put_int(m, sshpam_get_maxtries_reached());
#endif

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

	if (!options.kbd_interactive_authentication)
		fatal("%s: kbd-int authentication not enabled", __func__);
	success = bsdauth_query(authctxt, &name, &infotxt, &numprompts,
	    &prompts, &echo_on) < 0 ? 0 : 1;

	buffer_clear(m);
	buffer_put_int(m, success);
	if (success)
		buffer_put_cstring(m, prompts[0]);

	debug3("%s: sending challenge success: %u", __func__, success);
	mm_request_send(sock, MONITOR_ANS_BSDAUTHQUERY, m);

	if (success) {
		free(name);
		free(infotxt);
		free(prompts);
		free(echo_on);
	}

	return (0);
}

int
mm_answer_bsdauthrespond(int sock, Buffer *m)
{
	char *response;
	int authok;

	if (!options.kbd_interactive_authentication)
		fatal("%s: kbd-int authentication not enabled", __func__);
	if (authctxt->as == NULL)
		fatal("%s: no bsd auth session", __func__);

	response = buffer_get_string(m, NULL);
	authok = options.challenge_response_authentication &&
	    auth_userresponse(authctxt->as, response, 0);
	authctxt->as = NULL;
	debug3("%s: <%s> = <%d>", __func__, response, authok);
	free(response);

	buffer_clear(m);
	buffer_put_int(m, authok);

	debug3("%s: sending authenticated: %d", __func__, authok);
	mm_request_send(sock, MONITOR_ANS_BSDAUTHRESPOND, m);

	auth_method = "keyboard-interactive";
	auth_submethod = "bsdauth";

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

	free(response);

	buffer_clear(m);
	buffer_put_int(m, authok);

	debug3("%s: sending authenticated: %d", __func__, authok);
	mm_request_send(sock, MONITOR_ANS_SKEYRESPOND, m);

	auth_method = "keyboard-interactive";
	auth_submethod = "skey";

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
	if (options.kbd_interactive_authentication)
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_INIT_CTX, 1);

	return (0);
}

int
mm_answer_pam_account(int sock, Buffer *m)
{
	u_int ret;

	if (!options.use_pam)
		fatal("%s: PAM not enabled", __func__);

	ret = do_pam_account();

	buffer_put_int(m, ret);
	buffer_put_string(m, buffer_ptr(&loginmsg), buffer_len(&loginmsg));

	mm_request_send(sock, MONITOR_ANS_PAM_ACCOUNT, m);

	return (ret);
}

static void *sshpam_ctxt, *sshpam_authok;
extern KbdintDevice sshpam_device;

int
mm_answer_pam_init_ctx(int sock, Buffer *m)
{
	debug3("%s", __func__);
	if (!options.kbd_interactive_authentication)
		fatal("%s: kbd-int authentication not enabled", __func__);
	if (sshpam_ctxt != NULL)
		fatal("%s: already called", __func__);
	sshpam_ctxt = (sshpam_device.init_ctx)(authctxt);
	sshpam_authok = NULL;
	buffer_clear(m);
	if (sshpam_ctxt != NULL) {
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_FREE_CTX, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_QUERY, 1);
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
	char *name = NULL, *info = NULL, **prompts = NULL;
	u_int i, num = 0, *echo_on = 0;
	int ret;

	debug3("%s", __func__);
	sshpam_authok = NULL;
	if (sshpam_ctxt == NULL)
		fatal("%s: no context", __func__);
	ret = (sshpam_device.query)(sshpam_ctxt, &name, &info,
	    &num, &prompts, &echo_on);
	if (ret == 0 && num == 0)
		sshpam_authok = sshpam_ctxt;
	if (num > 1 || name == NULL || info == NULL)
		fatal("sshpam_device.query failed");
	monitor_permit(mon_dispatch, MONITOR_REQ_PAM_RESPOND, 1);
	buffer_clear(m);
	buffer_put_int(m, ret);
	buffer_put_cstring(m, name);
	free(name);
	buffer_put_cstring(m, info);
	free(info);
	buffer_put_int(m, sshpam_get_maxtries_reached());
	buffer_put_int(m, num);
	for (i = 0; i < num; ++i) {
		buffer_put_cstring(m, prompts[i]);
		free(prompts[i]);
		buffer_put_int(m, echo_on[i]);
	}
	free(prompts);
	free(echo_on);
	auth_method = "keyboard-interactive";
	auth_submethod = "pam";
	mm_request_send(sock, MONITOR_ANS_PAM_QUERY, m);
	return (0);
}

int
mm_answer_pam_respond(int sock, Buffer *m)
{
	char **resp;
	u_int i, num;
	int ret;

	debug3("%s", __func__);
	if (sshpam_ctxt == NULL)
		fatal("%s: no context", __func__);
	sshpam_authok = NULL;
	num = buffer_get_int(m);
	if (num > 0) {
		resp = xcalloc(num, sizeof(char *));
		for (i = 0; i < num; ++i)
			resp[i] = buffer_get_string(m, NULL);
		ret = (sshpam_device.respond)(sshpam_ctxt, num, resp);
		for (i = 0; i < num; ++i)
			free(resp[i]);
		free(resp);
	} else {
		ret = (sshpam_device.respond)(sshpam_ctxt, num, NULL);
	}
	buffer_clear(m);
	buffer_put_int(m, ret);
	mm_request_send(sock, MONITOR_ANS_PAM_RESPOND, m);
	auth_method = "keyboard-interactive";
	auth_submethod = "pam";
	if (ret == 0)
		sshpam_authok = sshpam_ctxt;
	return (0);
}

int
mm_answer_pam_free_ctx(int sock, Buffer *m)
{
	int r = sshpam_authok != NULL && sshpam_authok == sshpam_ctxt;

	debug3("%s", __func__);
	if (sshpam_ctxt == NULL)
		fatal("%s: no context", __func__);
	(sshpam_device.free_ctx)(sshpam_ctxt);
	sshpam_ctxt = sshpam_authok = NULL;
	buffer_clear(m);
	mm_request_send(sock, MONITOR_ANS_PAM_FREE_CTX, m);
	/* Allow another attempt */
	monitor_permit(mon_dispatch, MONITOR_REQ_PAM_INIT_CTX, 1);
	auth_method = "keyboard-interactive";
	auth_submethod = "pam";
	return r;
}
#endif

int
mm_answer_keyallowed(int sock, Buffer *m)
{
	Key *key;
	char *cuser, *chost;
	u_char *blob;
	u_int bloblen, pubkey_auth_attempt;
	enum mm_keytype type = 0;
	int allowed = 0;

	debug3("%s entering", __func__);

	type = buffer_get_int(m);
	cuser = buffer_get_string(m, NULL);
	chost = buffer_get_string(m, NULL);
	blob = buffer_get_string(m, &bloblen);
	pubkey_auth_attempt = buffer_get_int(m);

	key = key_from_blob(blob, bloblen);

	debug3("%s: key_from_blob: %p", __func__, key);

	if (key != NULL && authctxt->valid) {
		/* These should not make it past the privsep child */
		if (key_type_plain(key->type) == KEY_RSA &&
		    (datafellows & SSH_BUG_RSASIGMD5) != 0)
			fatal("%s: passed a SSH_BUG_RSASIGMD5 key", __func__);

		switch (type) {
		case MM_USERKEY:
			allowed = options.pubkey_authentication &&
			    !auth2_userkey_already_used(authctxt, key) &&
			    match_pattern_list(sshkey_ssh_name(key),
			    options.pubkey_key_types, 0) == 1 &&
			    user_key_allowed(authctxt->pw, key,
			    pubkey_auth_attempt);
			pubkey_auth_info(authctxt, key, NULL);
			auth_method = "publickey";
			if (options.pubkey_authentication &&
			    (!pubkey_auth_attempt || allowed != 1))
				auth_clear_options();
			break;
		case MM_HOSTKEY:
			allowed = options.hostbased_authentication &&
			    match_pattern_list(sshkey_ssh_name(key),
			    options.hostbased_key_types, 0) == 1 &&
			    hostbased_key_allowed(authctxt->pw,
			    cuser, chost, key);
			pubkey_auth_info(authctxt, key,
			    "client user \"%.100s\", client host \"%.100s\"",
			    cuser, chost);
			auth_method = "hostbased";
			break;
		default:
			fatal("%s: unknown key type %d", __func__, type);
			break;
		}
	}

	debug3("%s: key %p is %s",
	    __func__, key, allowed ? "allowed" : "not allowed");

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
	} else {
		/* Log failed attempt */
		auth_log(authctxt, 0, 0, auth_method, NULL);
		free(blob);
		free(cuser);
		free(chost);
	}

	buffer_clear(m);
	buffer_put_int(m, allowed);
	buffer_put_int(m, forced_command != NULL);

	mm_request_send(sock, MONITOR_ANS_KEYALLOWED, m);

	return (0);
}

static int
monitor_valid_userblob(u_char *data, u_int datalen)
{
	Buffer b;
	u_char *p;
	char *userstyle, *cp;
	u_int len;
	int fail = 0;

	buffer_init(&b);
	buffer_append(&b, data, datalen);

	if (datafellows & SSH_OLD_SESSIONID) {
		p = buffer_ptr(&b);
		len = buffer_len(&b);
		if ((session_id2 == NULL) ||
		    (len < session_id2_len) ||
		    (timingsafe_bcmp(p, session_id2, session_id2_len) != 0))
			fail++;
		buffer_consume(&b, session_id2_len);
	} else {
		p = buffer_get_string(&b, &len);
		if ((session_id2 == NULL) ||
		    (len != session_id2_len) ||
		    (timingsafe_bcmp(p, session_id2, session_id2_len) != 0))
			fail++;
		free(p);
	}
	if (buffer_get_char(&b) != SSH2_MSG_USERAUTH_REQUEST)
		fail++;
	cp = buffer_get_cstring(&b, NULL);
	xasprintf(&userstyle, "%s%s%s", authctxt->user,
	    authctxt->style ? ":" : "",
	    authctxt->style ? authctxt->style : "");
	if (strcmp(userstyle, cp) != 0) {
		logit("wrong user name passed to monitor: "
		    "expected %s != %.100s", userstyle, cp);
		fail++;
	}
	free(userstyle);
	free(cp);
	buffer_skip_string(&b);
	if (datafellows & SSH_BUG_PKAUTH) {
		if (!buffer_get_char(&b))
			fail++;
	} else {
		cp = buffer_get_cstring(&b, NULL);
		if (strcmp("publickey", cp) != 0)
			fail++;
		free(cp);
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
	char *p, *userstyle;
	u_int len;
	int fail = 0;

	buffer_init(&b);
	buffer_append(&b, data, datalen);

	p = buffer_get_string(&b, &len);
	if ((session_id2 == NULL) ||
	    (len != session_id2_len) ||
	    (timingsafe_bcmp(p, session_id2, session_id2_len) != 0))
		fail++;
	free(p);

	if (buffer_get_char(&b) != SSH2_MSG_USERAUTH_REQUEST)
		fail++;
	p = buffer_get_cstring(&b, NULL);
	xasprintf(&userstyle, "%s%s%s", authctxt->user,
	    authctxt->style ? ":" : "",
	    authctxt->style ? authctxt->style : "");
	if (strcmp(userstyle, p) != 0) {
		logit("wrong user name passed to monitor: expected %s != %.100s",
		    userstyle, p);
		fail++;
	}
	free(userstyle);
	free(p);
	buffer_skip_string(&b);	/* service */
	p = buffer_get_cstring(&b, NULL);
	if (strcmp(p, "hostbased") != 0)
		fail++;
	free(p);
	buffer_skip_string(&b);	/* pkalg */
	buffer_skip_string(&b);	/* pkblob */

	/* verify client host, strip trailing dot if necessary */
	p = buffer_get_string(&b, NULL);
	if (((len = strlen(p)) > 0) && p[len - 1] == '.')
		p[len - 1] = '\0';
	if (strcmp(p, chost) != 0)
		fail++;
	free(p);

	/* verify client user */
	p = buffer_get_string(&b, NULL);
	if (strcmp(p, cuser) != 0)
		fail++;
	free(p);

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
	    __func__, key, (verified == 1) ? "verified" : "unverified");

	/* If auth was successful then record key to ensure it isn't reused */
	if (verified == 1 && key_blobtype == MM_USERKEY)
		auth2_record_userkey(authctxt, key);
	else
		key_free(key);

	free(blob);
	free(signature);
	free(data);

	auth_method = key_blobtype == MM_USERKEY ? "publickey" : "hostbased";

	monitor_reset_key_state();

	buffer_clear(m);
	buffer_put_int(m, verified);
	mm_request_send(sock, MONITOR_ANS_KEYVERIFY, m);

	return (verified == 1);
}

static void
mm_record_login(Session *s, struct passwd *pw)
{
	struct ssh *ssh = active_state;	/* XXX */
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
		    (struct sockaddr *)&from, &fromlen) < 0) {
			debug("getpeername: %.100s", strerror(errno));
			cleanup_exit(255);
		}
	}
	/* Record that there was a login on that tty from the remote host. */
	record_login(s->pid, s->tty, pw->pw_name, pw->pw_uid,
	    session_get_remote_name_or_ip(ssh, utmp_len, options.use_dns),
	    (struct sockaddr *)&from, fromlen);
}

static void
mm_session_close(Session *s)
{
	debug3("%s: session %d pid %ld", __func__, s->self, (long)s->pid);
	if (s->ttyfd != -1) {
		debug3("%s: tty %s ptyfd %d", __func__, s->tty, s->ptyfd);
		session_pty_cleanup2(s);
	}
	session_unused(s->self);
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

	if (mm_send_fd(sock, s->ptyfd) == -1 ||
	    mm_send_fd(sock, s->ttyfd) == -1)
		fatal("%s: send fds failed", __func__);

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

	debug3("%s: tty %s ptyfd %d", __func__, s->tty, s->ttyfd);

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
	free(tty);
	return (0);
}

int
mm_answer_term(int sock, Buffer *req)
{
	extern struct monitor *pmonitor;
	int res, status;

	debug3("%s: tearing down sessions", __func__);

	/* The child is terminating */
	session_destroy_all(&mm_session_close);

#ifdef USE_PAM
	if (options.use_pam)
		sshpam_cleanup();
#endif

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
	free(cmd);
	return (0);
}
#endif /* SSH_AUDIT_EVENTS */

void
monitor_apply_keystate(struct monitor *pmonitor)
{
	struct ssh *ssh = active_state;	/* XXX */
	struct kex *kex;
	int r;

	debug3("%s: packet_set_state", __func__);
	if ((r = ssh_packet_set_state(ssh, child_state)) != 0)
                fatal("%s: packet_set_state: %s", __func__, ssh_err(r));
	sshbuf_free(child_state);
	child_state = NULL;

	if ((kex = ssh->kex) != NULL) {
		/* XXX set callbacks */
#ifdef WITH_OPENSSL
		kex->kex[KEX_DH_GRP1_SHA1] = kexdh_server;
		kex->kex[KEX_DH_GRP14_SHA1] = kexdh_server;
		kex->kex[KEX_DH_GRP14_SHA256] = kexdh_server;
		kex->kex[KEX_DH_GRP16_SHA512] = kexdh_server;
		kex->kex[KEX_DH_GRP18_SHA512] = kexdh_server;
		kex->kex[KEX_DH_GEX_SHA1] = kexgex_server;
		kex->kex[KEX_DH_GEX_SHA256] = kexgex_server;
# ifdef OPENSSL_HAS_ECC
		kex->kex[KEX_ECDH_SHA2] = kexecdh_server;
# endif
#endif /* WITH_OPENSSL */
		kex->kex[KEX_C25519_SHA256] = kexc25519_server;
		kex->load_host_public_key=&get_hostkey_public_by_type;
		kex->load_host_private_key=&get_hostkey_private_by_type;
		kex->host_key_index=&get_hostkey_index;
		kex->sign = sshd_hostkey_sign;
	}
}

/* This function requries careful sanity checking */

void
mm_get_keystate(struct monitor *pmonitor)
{
	debug3("%s: Waiting for new keys", __func__);

	if ((child_state = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	mm_request_receive_expect(pmonitor->m_sendfd, MONITOR_REQ_KEYEXPORT,
	    child_state);
	debug3("%s: GOT new keys", __func__);
}


/* XXX */

#define FD_CLOSEONEXEC(x) do { \
	if (fcntl(x, F_SETFD, FD_CLOEXEC) == -1) \
		fatal("fcntl(%d, F_SETFD)", x); \
} while (0)

static void
monitor_openfds(struct monitor *mon, int do_logfds)
{
	int pair[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
		fatal("%s: socketpair: %s", __func__, strerror(errno));
	FD_CLOSEONEXEC(pair[0]);
	FD_CLOSEONEXEC(pair[1]);
	mon->m_recvfd = pair[0];
	mon->m_sendfd = pair[1];

	if (do_logfds) {
		if (pipe(pair) == -1)
			fatal("%s: pipe: %s", __func__, strerror(errno));
		FD_CLOSEONEXEC(pair[0]);
		FD_CLOSEONEXEC(pair[1]);
		mon->m_log_recvfd = pair[0];
		mon->m_log_sendfd = pair[1];
	} else
		mon->m_log_recvfd = mon->m_log_sendfd = -1;
}

#define MM_MEMSIZE	65536

struct monitor *
monitor_init(void)
{
	struct monitor *mon;

	mon = xcalloc(1, sizeof(*mon));
	monitor_openfds(mon, 1);

	return mon;
}

void
monitor_reinit(struct monitor *mon)
{
	monitor_openfds(mon, 0);
}

#ifdef GSSAPI
int
mm_answer_gss_setup_ctx(int sock, Buffer *m)
{
	gss_OID_desc goid;
	OM_uint32 major;
	u_int len;

	if (!options.gss_authentication)
		fatal("%s: GSSAPI authentication not enabled", __func__);

	goid.elements = buffer_get_string(m, &len);
	goid.length = len;

	major = ssh_gssapi_server_ctx(&gsscontext, &goid);

	free(goid.elements);

	buffer_clear(m);
	buffer_put_int(m, major);

	mm_request_send(sock, MONITOR_ANS_GSSSETUP, m);

	/* Now we have a context, enable the step */
	monitor_permit(mon_dispatch, MONITOR_REQ_GSSSTEP, 1);

	return (0);
}

int
mm_answer_gss_accept_ctx(int sock, Buffer *m)
{
	gss_buffer_desc in;
	gss_buffer_desc out = GSS_C_EMPTY_BUFFER;
	OM_uint32 major, minor;
	OM_uint32 flags = 0; /* GSI needs this */
	u_int len;

	if (!options.gss_authentication)
		fatal("%s: GSSAPI authentication not enabled", __func__);

	in.value = buffer_get_string(m, &len);
	in.length = len;
	major = ssh_gssapi_accept_ctx(gsscontext, &in, &out, &flags);
	free(in.value);

	buffer_clear(m);
	buffer_put_int(m, major);
	buffer_put_string(m, out.value, out.length);
	buffer_put_int(m, flags);
	mm_request_send(sock, MONITOR_ANS_GSSSTEP, m);

	gss_release_buffer(&minor, &out);

	if (major == GSS_S_COMPLETE) {
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

	if (!options.gss_authentication)
		fatal("%s: GSSAPI authentication not enabled", __func__);

	gssbuf.value = buffer_get_string(m, &len);
	gssbuf.length = len;
	mic.value = buffer_get_string(m, &len);
	mic.length = len;

	ret = ssh_gssapi_checkmic(gsscontext, &gssbuf, &mic);

	free(gssbuf.value);
	free(mic.value);

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

	if (!options.gss_authentication)
		fatal("%s: GSSAPI authentication not enabled", __func__);

	authenticated = authctxt->valid && ssh_gssapi_userok(authctxt->user);

	buffer_clear(m);
	buffer_put_int(m, authenticated);

	debug3("%s: sending result %d", __func__, authenticated);
	mm_request_send(sock, MONITOR_ANS_GSSUSEROK, m);

	auth_method = "gssapi-with-mic";

	/* Monitor loop will terminate if authenticated */
	return (authenticated);
}
#endif /* GSSAPI */

