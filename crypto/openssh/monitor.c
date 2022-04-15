/* $OpenBSD: monitor.c,v 1.232 2022/02/25 02:09:27 djm Exp $ */
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
# include <stdint.h>
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

#ifdef WITH_OPENSSL
#include <openssl/dh.h>
#endif

#include "openbsd-compat/sys-tree.h"
#include "openbsd-compat/sys-queue.h"
#include "openbsd-compat/openssl-compat.h"

#include "atomicio.h"
#include "xmalloc.h"
#include "ssh.h"
#include "sshkey.h"
#include "sshbuf.h"
#include "hostfile.h"
#include "auth.h"
#include "cipher.h"
#include "kex.h"
#include "dh.h"
#include "auth-pam.h"
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
#include "sk-api.h"

#ifdef GSSAPI
static Gssctxt *gsscontext = NULL;
#endif

/* Imports */
extern ServerOptions options;
extern u_int utmp_len;
extern struct sshbuf *loginmsg;
extern struct sshauthopt *auth_opts; /* XXX move to permanent ssh->authctxt? */

/* State exported from the child */
static struct sshbuf *child_state;

/* Functions on the monitor that answer unprivileged requests */

int mm_answer_moduli(struct ssh *, int, struct sshbuf *);
int mm_answer_sign(struct ssh *, int, struct sshbuf *);
int mm_answer_pwnamallow(struct ssh *, int, struct sshbuf *);
int mm_answer_auth2_read_banner(struct ssh *, int, struct sshbuf *);
int mm_answer_authserv(struct ssh *, int, struct sshbuf *);
int mm_answer_authpassword(struct ssh *, int, struct sshbuf *);
int mm_answer_bsdauthquery(struct ssh *, int, struct sshbuf *);
int mm_answer_bsdauthrespond(struct ssh *, int, struct sshbuf *);
int mm_answer_keyallowed(struct ssh *, int, struct sshbuf *);
int mm_answer_keyverify(struct ssh *, int, struct sshbuf *);
int mm_answer_pty(struct ssh *, int, struct sshbuf *);
int mm_answer_pty_cleanup(struct ssh *, int, struct sshbuf *);
int mm_answer_term(struct ssh *, int, struct sshbuf *);
int mm_answer_rsa_keyallowed(struct ssh *, int, struct sshbuf *);
int mm_answer_rsa_challenge(struct ssh *, int, struct sshbuf *);
int mm_answer_rsa_response(struct ssh *, int, struct sshbuf *);
int mm_answer_sesskey(struct ssh *, int, struct sshbuf *);
int mm_answer_sessid(struct ssh *, int, struct sshbuf *);

#ifdef USE_PAM
int mm_answer_pam_start(struct ssh *, int, struct sshbuf *);
int mm_answer_pam_account(struct ssh *, int, struct sshbuf *);
int mm_answer_pam_init_ctx(struct ssh *, int, struct sshbuf *);
int mm_answer_pam_query(struct ssh *, int, struct sshbuf *);
int mm_answer_pam_respond(struct ssh *, int, struct sshbuf *);
int mm_answer_pam_free_ctx(struct ssh *, int, struct sshbuf *);
#endif

#ifdef GSSAPI
int mm_answer_gss_setup_ctx(struct ssh *, int, struct sshbuf *);
int mm_answer_gss_accept_ctx(struct ssh *, int, struct sshbuf *);
int mm_answer_gss_userok(struct ssh *, int, struct sshbuf *);
int mm_answer_gss_checkmic(struct ssh *, int, struct sshbuf *);
#endif

#ifdef SSH_AUDIT_EVENTS
int mm_answer_audit_event(struct ssh *, int, struct sshbuf *);
int mm_answer_audit_command(struct ssh *, int, struct sshbuf *);
#endif

static Authctxt *authctxt;

/* local state for key verify */
static u_char *key_blob = NULL;
static size_t key_bloblen = 0;
static u_int key_blobtype = MM_NOKEY;
static struct sshauthopt *key_opts = NULL;
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
	int (*f)(struct ssh *, int, struct sshbuf *);
};

#define MON_ISAUTH	0x0004	/* Required for Authentication */
#define MON_AUTHDECIDE	0x0008	/* Decides Authentication */
#define MON_ONCE	0x0010	/* Disable after calling */
#define MON_ALOG	0x0020	/* Log auth attempt without authenticating */

#define MON_AUTH	(MON_ISAUTH|MON_AUTHDECIDE)

#define MON_PERMIT	0x1000	/* Request is permitted */

static int monitor_read(struct ssh *, struct monitor *, struct mon_table *,
    struct mon_table **);
static int monitor_read_log(struct monitor *);

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
monitor_child_preauth(struct ssh *ssh, struct monitor *pmonitor)
{
	struct mon_table *ent;
	int authenticated = 0, partial = 0;

	debug3("preauth child monitor started");

	if (pmonitor->m_recvfd >= 0)
		close(pmonitor->m_recvfd);
	if (pmonitor->m_log_sendfd >= 0)
		close(pmonitor->m_log_sendfd);
	pmonitor->m_log_sendfd = pmonitor->m_recvfd = -1;

	authctxt = (Authctxt *)ssh->authctxt;
	memset(authctxt, 0, sizeof(*authctxt));
	ssh->authctxt = authctxt;

	authctxt->loginmsg = loginmsg;

	mon_dispatch = mon_dispatch_proto20;
	/* Permit requests for moduli and signatures */
	monitor_permit(mon_dispatch, MONITOR_REQ_MODULI, 1);
	monitor_permit(mon_dispatch, MONITOR_REQ_SIGN, 1);

	/* The first few requests do not require asynchronous access */
	while (!authenticated) {
		partial = 0;
		auth_method = "unknown";
		auth_submethod = NULL;
		auth2_authctxt_reset_info(authctxt);

		authenticated = (monitor_read(ssh, pmonitor,
		    mon_dispatch, &ent) == 1);

		/* Special handling for multiple required authentications */
		if (options.num_auth_methods != 0) {
			if (authenticated &&
			    !auth2_update_methods_lists(authctxt,
			    auth_method, auth_submethod)) {
				debug3_f("method %s: partial", auth_method);
				authenticated = 0;
				partial = 1;
			}
		}

		if (authenticated) {
			if (!(ent->flags & MON_AUTHDECIDE))
				fatal_f("unexpected authentication from %d",
				    ent->type);
			if (authctxt->pw->pw_uid == 0 &&
			    !auth_root_allowed(ssh, auth_method))
				authenticated = 0;
#ifdef USE_PAM
			/* PAM needs to perform account checks after auth */
			if (options.use_pam && authenticated) {
				struct sshbuf *m;

				if ((m = sshbuf_new()) == NULL)
					fatal("%s: sshbuf_new failed",
					    __func__);
				mm_request_receive_expect(pmonitor->m_sendfd,
				    MONITOR_REQ_PAM_ACCOUNT, m);
				authenticated = mm_answer_pam_account(
				    ssh, pmonitor->m_sendfd, m);
				sshbuf_free(m);
			}
#endif
		}
		if (ent->flags & (MON_AUTHDECIDE|MON_ALOG)) {
			auth_log(ssh, authenticated, partial,
			    auth_method, auth_submethod);
			if (!partial && !authenticated)
				authctxt->failures++;
			if (authenticated || partial) {
				auth2_update_session_info(authctxt,
				    auth_method, auth_submethod);
			}
		}
	}

	if (!authctxt->valid)
		fatal_f("authenticated invalid user");
	if (strcmp(auth_method, "unknown") == 0)
		fatal_f("authentication method name unknown");

	debug_f("user %s authenticated by privileged process", authctxt->user);
	ssh->authctxt = NULL;
	ssh_packet_set_log_preamble(ssh, "user %s", authctxt->user);

	mm_get_keystate(ssh, pmonitor);

	/* Drain any buffered messages from the child */
	while (pmonitor->m_log_recvfd != -1 && monitor_read_log(pmonitor) == 0)
		;

	if (pmonitor->m_recvfd >= 0)
		close(pmonitor->m_recvfd);
	if (pmonitor->m_log_sendfd >= 0)
		close(pmonitor->m_log_sendfd);
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
monitor_child_postauth(struct ssh *ssh, struct monitor *pmonitor)
{
	close(pmonitor->m_recvfd);
	pmonitor->m_recvfd = -1;

	monitor_set_child_handler(pmonitor->m_pid);
	ssh_signal(SIGHUP, &monitor_child_handler);
	ssh_signal(SIGTERM, &monitor_child_handler);
	ssh_signal(SIGINT, &monitor_child_handler);
#ifdef SIGXFSZ
	ssh_signal(SIGXFSZ, SIG_IGN);
#endif

	mon_dispatch = mon_dispatch_postauth20;

	/* Permit requests for moduli and signatures */
	monitor_permit(mon_dispatch, MONITOR_REQ_MODULI, 1);
	monitor_permit(mon_dispatch, MONITOR_REQ_SIGN, 1);
	monitor_permit(mon_dispatch, MONITOR_REQ_TERM, 1);

	if (auth_opts->permit_pty_flag) {
		monitor_permit(mon_dispatch, MONITOR_REQ_PTY, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_PTYCLEANUP, 1);
	}

	for (;;)
		monitor_read(ssh, pmonitor, mon_dispatch, NULL);
}

static int
monitor_read_log(struct monitor *pmonitor)
{
	struct sshbuf *logmsg;
	u_int len, level, forced;
	char *msg;
	u_char *p;
	int r;

	if ((logmsg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new");

	/* Read length */
	if ((r = sshbuf_reserve(logmsg, 4, &p)) != 0)
		fatal_fr(r, "reserve len");
	if (atomicio(read, pmonitor->m_log_recvfd, p, 4) != 4) {
		if (errno == EPIPE) {
			sshbuf_free(logmsg);
			debug_f("child log fd closed");
			close(pmonitor->m_log_recvfd);
			pmonitor->m_log_recvfd = -1;
			return -1;
		}
		fatal_f("log fd read: %s", strerror(errno));
	}
	if ((r = sshbuf_get_u32(logmsg, &len)) != 0)
		fatal_fr(r, "parse len");
	if (len <= 4 || len > 8192)
		fatal_f("invalid log message length %u", len);

	/* Read severity, message */
	sshbuf_reset(logmsg);
	if ((r = sshbuf_reserve(logmsg, len, &p)) != 0)
		fatal_fr(r, "reserve msg");
	if (atomicio(read, pmonitor->m_log_recvfd, p, len) != len)
		fatal_f("log fd read: %s", strerror(errno));
	if ((r = sshbuf_get_u32(logmsg, &level)) != 0 ||
	    (r = sshbuf_get_u32(logmsg, &forced)) != 0 ||
	    (r = sshbuf_get_cstring(logmsg, &msg, NULL)) != 0)
		fatal_fr(r, "parse");

	/* Log it */
	if (log_level_name(level) == NULL)
		fatal_f("invalid log level %u (corrupted message?)", level);
	sshlogdirect(level, forced, "%s [preauth]", msg);

	sshbuf_free(logmsg);
	free(msg);

	return 0;
}

static int
monitor_read(struct ssh *ssh, struct monitor *pmonitor, struct mon_table *ent,
    struct mon_table **pent)
{
	struct sshbuf *m;
	int r, ret;
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
			fatal_f("poll: %s", strerror(errno));
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

	if ((m = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new");

	mm_request_receive(pmonitor->m_sendfd, m);
	if ((r = sshbuf_get_u8(m, &type)) != 0)
		fatal_fr(r, "parse type");

	debug3_f("checking request %d", type);

	while (ent->f != NULL) {
		if (ent->type == type)
			break;
		ent++;
	}

	if (ent->f != NULL) {
		if (!(ent->flags & MON_PERMIT))
			fatal_f("unpermitted request %d", type);
		ret = (*ent->f)(ssh, pmonitor->m_sendfd, m);
		sshbuf_free(m);

		/* The child may use this request only once, disable it */
		if (ent->flags & MON_ONCE) {
			debug2_f("%d used once, disabling now", type);
			ent->flags &= ~MON_PERMIT;
		}

		if (pent != NULL)
			*pent = ent;

		return ret;
	}

	fatal_f("unsupported request: %d", type);

	/* NOTREACHED */
	return (-1);
}

/* allowed key state */
static int
monitor_allowed_key(const u_char *blob, u_int bloblen)
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
	sshauthopt_free(key_opts);
	key_blob = NULL;
	key_bloblen = 0;
	key_blobtype = MM_NOKEY;
	key_opts = NULL;
	hostbased_cuser = NULL;
	hostbased_chost = NULL;
}

#ifdef WITH_OPENSSL
int
mm_answer_moduli(struct ssh *ssh, int sock, struct sshbuf *m)
{
	DH *dh;
	const BIGNUM *dh_p, *dh_g;
	int r;
	u_int min, want, max;

	if ((r = sshbuf_get_u32(m, &min)) != 0 ||
	    (r = sshbuf_get_u32(m, &want)) != 0 ||
	    (r = sshbuf_get_u32(m, &max)) != 0)
		fatal_fr(r, "parse");

	debug3_f("got parameters: %d %d %d", min, want, max);
	/* We need to check here, too, in case the child got corrupted */
	if (max < min || want < min || max < want)
		fatal_f("bad parameters: %d %d %d", min, want, max);

	sshbuf_reset(m);

	dh = choose_dh(min, want, max);
	if (dh == NULL) {
		if ((r = sshbuf_put_u8(m, 0)) != 0)
			fatal_fr(r, "assemble empty");
		return (0);
	} else {
		/* Send first bignum */
		DH_get0_pqg(dh, &dh_p, NULL, &dh_g);
		if ((r = sshbuf_put_u8(m, 1)) != 0 ||
		    (r = sshbuf_put_bignum2(m, dh_p)) != 0 ||
		    (r = sshbuf_put_bignum2(m, dh_g)) != 0)
			fatal_fr(r, "assemble");

		DH_free(dh);
	}
	mm_request_send(sock, MONITOR_ANS_MODULI, m);
	return (0);
}
#endif

int
mm_answer_sign(struct ssh *ssh, int sock, struct sshbuf *m)
{
	extern int auth_sock;			/* XXX move to state struct? */
	struct sshkey *key;
	struct sshbuf *sigbuf = NULL;
	u_char *p = NULL, *signature = NULL;
	char *alg = NULL;
	size_t datlen, siglen, alglen;
	int r, is_proof = 0;
	u_int keyid, compat;
	const char proof_req[] = "hostkeys-prove-00@openssh.com";

	debug3_f("entering");

	if ((r = sshbuf_get_u32(m, &keyid)) != 0 ||
	    (r = sshbuf_get_string(m, &p, &datlen)) != 0 ||
	    (r = sshbuf_get_cstring(m, &alg, &alglen)) != 0 ||
	    (r = sshbuf_get_u32(m, &compat)) != 0)
		fatal_fr(r, "parse");
	if (keyid > INT_MAX)
		fatal_f("invalid key ID");

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
			fatal_f("bad data length: %zu", datlen);
		if ((key = get_hostkey_public_by_index(keyid, ssh)) == NULL)
			fatal_f("no hostkey for index %d", keyid);
		if ((sigbuf = sshbuf_new()) == NULL)
			fatal_f("sshbuf_new");
		if ((r = sshbuf_put_cstring(sigbuf, proof_req)) != 0 ||
		    (r = sshbuf_put_string(sigbuf, session_id2,
		    session_id2_len)) != 0 ||
		    (r = sshkey_puts(key, sigbuf)) != 0)
			fatal_fr(r, "assemble private key proof");
		if (datlen != sshbuf_len(sigbuf) ||
		    memcmp(p, sshbuf_ptr(sigbuf), sshbuf_len(sigbuf)) != 0)
			fatal_f("bad data length: %zu, hostkey proof len %zu",
			    datlen, sshbuf_len(sigbuf));
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
		    options.sk_provider, NULL, compat)) != 0)
			fatal_fr(r, "sign");
	} else if ((key = get_hostkey_public_by_index(keyid, ssh)) != NULL &&
	    auth_sock > 0) {
		if ((r = ssh_agent_sign(auth_sock, key, &signature, &siglen,
		    p, datlen, alg, compat)) != 0)
			fatal_fr(r, "agent sign");
	} else
		fatal_f("no hostkey from index %d", keyid);

	debug3_f("%s %s signature len=%zu", alg,
	    is_proof ? "hostkey proof" : "KEX", siglen);

	sshbuf_reset(m);
	if ((r = sshbuf_put_string(m, signature, siglen)) != 0)
		fatal_fr(r, "assemble");

	free(alg);
	free(p);
	free(signature);

	mm_request_send(sock, MONITOR_ANS_SIGN, m);

	/* Turn on permissions for getpwnam */
	monitor_permit(mon_dispatch, MONITOR_REQ_PWNAM, 1);

	return (0);
}

#define PUTPW(b, id) \
	do { \
		if ((r = sshbuf_put_string(b, \
		    &pwent->id, sizeof(pwent->id))) != 0) \
			fatal_fr(r, "assemble %s", #id); \
	} while (0)

/* Retrieves the password entry and also checks if the user is permitted */
int
mm_answer_pwnamallow(struct ssh *ssh, int sock, struct sshbuf *m)
{
	struct passwd *pwent;
	int r, allowed = 0;
	u_int i;

	debug3_f("entering");

	if (authctxt->attempt++ != 0)
		fatal_f("multiple attempts for getpwnam");

	if ((r = sshbuf_get_cstring(m, &authctxt->user, NULL)) != 0)
		fatal_fr(r, "parse");

	pwent = getpwnamallow(ssh, authctxt->user);

	setproctitle("%s [priv]", pwent ? authctxt->user : "unknown");

	sshbuf_reset(m);

	if (pwent == NULL) {
		if ((r = sshbuf_put_u8(m, 0)) != 0)
			fatal_fr(r, "assemble fakepw");
		authctxt->pw = fakepw();
		goto out;
	}

	allowed = 1;
	authctxt->pw = pwent;
	authctxt->valid = 1;

	/* XXX send fake class/dir/shell, etc. */
	if ((r = sshbuf_put_u8(m, 1)) != 0)
		fatal_fr(r, "assemble ok");
	PUTPW(m, pw_uid);
	PUTPW(m, pw_gid);
#ifdef HAVE_STRUCT_PASSWD_PW_CHANGE
	PUTPW(m, pw_change);
#endif
#ifdef HAVE_STRUCT_PASSWD_PW_EXPIRE
	PUTPW(m, pw_expire);
#endif
	if ((r = sshbuf_put_cstring(m, pwent->pw_name)) != 0 ||
	    (r = sshbuf_put_cstring(m, "*")) != 0 ||
#ifdef HAVE_STRUCT_PASSWD_PW_GECOS
	    (r = sshbuf_put_cstring(m, pwent->pw_gecos)) != 0 ||
#endif
#ifdef HAVE_STRUCT_PASSWD_PW_CLASS
	    (r = sshbuf_put_cstring(m, pwent->pw_class)) != 0 ||
#endif
	    (r = sshbuf_put_cstring(m, pwent->pw_dir)) != 0 ||
	    (r = sshbuf_put_cstring(m, pwent->pw_shell)) != 0)
		fatal_fr(r, "assemble pw");

 out:
	ssh_packet_set_log_preamble(ssh, "%suser %s",
	    authctxt->valid ? "authenticating" : "invalid ", authctxt->user);
	if ((r = sshbuf_put_string(m, &options, sizeof(options))) != 0)
		fatal_fr(r, "assemble options");

#define M_CP_STROPT(x) do { \
		if (options.x != NULL && \
		    (r = sshbuf_put_cstring(m, options.x)) != 0) \
			fatal_fr(r, "assemble %s", #x); \
	} while (0)
#define M_CP_STRARRAYOPT(x, nx) do { \
		for (i = 0; i < options.nx; i++) { \
			if ((r = sshbuf_put_cstring(m, options.x[i])) != 0) \
				fatal_fr(r, "assemble %s", #x); \
		} \
	} while (0)
	/* See comment in servconf.h */
	COPY_MATCH_STRING_OPTS();
#undef M_CP_STROPT
#undef M_CP_STRARRAYOPT

	/* Create valid auth method lists */
	if (auth2_setup_methods_lists(authctxt) != 0) {
		/*
		 * The monitor will continue long enough to let the child
		 * run to its packet_disconnect(), but it must not allow any
		 * authentication to succeed.
		 */
		debug_f("no valid authentication method lists");
	}

	debug3_f("sending MONITOR_ANS_PWNAM: %d", allowed);
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

int mm_answer_auth2_read_banner(struct ssh *ssh, int sock, struct sshbuf *m)
{
	char *banner;
	int r;

	sshbuf_reset(m);
	banner = auth2_read_banner();
	if ((r = sshbuf_put_cstring(m, banner != NULL ? banner : "")) != 0)
		fatal_fr(r, "assemble");
	mm_request_send(sock, MONITOR_ANS_AUTH2_READ_BANNER, m);
	free(banner);

	return (0);
}

int
mm_answer_authserv(struct ssh *ssh, int sock, struct sshbuf *m)
{
	int r;

	monitor_permit_authentications(1);

	if ((r = sshbuf_get_cstring(m, &authctxt->service, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(m, &authctxt->style, NULL)) != 0)
		fatal_fr(r, "parse");
	debug3_f("service=%s, style=%s", authctxt->service, authctxt->style);

	if (strlen(authctxt->style) == 0) {
		free(authctxt->style);
		authctxt->style = NULL;
	}

	return (0);
}

/*
 * Check that the key type appears in the supplied pattern list, ignoring
 * mismatches in the signature algorithm. (Signature algorithm checks are
 * performed in the unprivileged authentication code).
 * Returns 1 on success, 0 otherwise.
 */
static int
key_base_type_match(const char *method, const struct sshkey *key,
    const char *list)
{
	char *s, *l, *ol = xstrdup(list);
	int found = 0;

	l = ol;
	for ((s = strsep(&l, ",")); s && *s != '\0'; (s = strsep(&l, ","))) {
		if (sshkey_type_from_name(s) == key->type) {
			found = 1;
			break;
		}
	}
	if (!found) {
		error("%s key type %s is not in permitted list %s", method,
		    sshkey_ssh_name(key), list);
	}

	free(ol);
	return found;
}

int
mm_answer_authpassword(struct ssh *ssh, int sock, struct sshbuf *m)
{
	static int call_count;
	char *passwd;
	int r, authenticated;
	size_t plen;

	if (!options.password_authentication)
		fatal_f("password authentication not enabled");
	if ((r = sshbuf_get_cstring(m, &passwd, &plen)) != 0)
		fatal_fr(r, "parse");
	/* Only authenticate if the context is valid */
	authenticated = options.password_authentication &&
	    auth_password(ssh, passwd);
	freezero(passwd, plen);

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, authenticated)) != 0)
		fatal_fr(r, "assemble");
#ifdef USE_PAM
	if ((r = sshbuf_put_u32(m, sshpam_get_maxtries_reached())) != 0)
		fatal_fr(r, "assemble PAM");
#endif

	debug3("%s: sending result %d", __func__, authenticated);
	debug3_f("sending result %d", authenticated);
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
mm_answer_bsdauthquery(struct ssh *ssh, int sock, struct sshbuf *m)
{
	char *name, *infotxt;
	u_int numprompts, *echo_on, success;
	char **prompts;
	int r;

	if (!options.kbd_interactive_authentication)
		fatal_f("kbd-int authentication not enabled");
	success = bsdauth_query(authctxt, &name, &infotxt, &numprompts,
	    &prompts, &echo_on) < 0 ? 0 : 1;

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, success)) != 0)
		fatal_fr(r, "assemble");
	if (success) {
		if ((r = sshbuf_put_cstring(m, prompts[0])) != 0)
			fatal_fr(r, "assemble prompt");
	}

	debug3_f("sending challenge success: %u", success);
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
mm_answer_bsdauthrespond(struct ssh *ssh, int sock, struct sshbuf *m)
{
	char *response;
	int r, authok;

	if (!options.kbd_interactive_authentication)
		fatal_f("kbd-int authentication not enabled");
	if (authctxt->as == NULL)
		fatal_f("no bsd auth session");

	if ((r = sshbuf_get_cstring(m, &response, NULL)) != 0)
		fatal_fr(r, "parse");
	authok = options.kbd_interactive_authentication &&
	    auth_userresponse(authctxt->as, response, 0);
	authctxt->as = NULL;
	debug3_f("<%s> = <%d>", response, authok);
	free(response);

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, authok)) != 0)
		fatal_fr(r, "assemble");

	debug3_f("sending authenticated: %d", authok);
	mm_request_send(sock, MONITOR_ANS_BSDAUTHRESPOND, m);

	auth_method = "keyboard-interactive";
	auth_submethod = "bsdauth";

	return (authok != 0);
}
#endif

#ifdef USE_PAM
int
mm_answer_pam_start(struct ssh *ssh, int sock, struct sshbuf *m)
{
	if (!options.use_pam)
		fatal("UsePAM not set, but ended up in %s anyway", __func__);

	start_pam(ssh);

	monitor_permit(mon_dispatch, MONITOR_REQ_PAM_ACCOUNT, 1);
	if (options.kbd_interactive_authentication)
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_INIT_CTX, 1);

	return (0);
}

int
mm_answer_pam_account(struct ssh *ssh, int sock, struct sshbuf *m)
{
	u_int ret;
	int r;

	if (!options.use_pam)
		fatal("%s: PAM not enabled", __func__);

	ret = do_pam_account();

	if ((r = sshbuf_put_u32(m, ret)) != 0 ||
	    (r = sshbuf_put_stringb(m, loginmsg)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	mm_request_send(sock, MONITOR_ANS_PAM_ACCOUNT, m);

	return (ret);
}

static void *sshpam_ctxt, *sshpam_authok;
extern KbdintDevice sshpam_device;

int
mm_answer_pam_init_ctx(struct ssh *ssh, int sock, struct sshbuf *m)
{
	u_int ok = 0;
	int r;

	debug3("%s", __func__);
	if (!options.kbd_interactive_authentication)
		fatal("%s: kbd-int authentication not enabled", __func__);
	if (sshpam_ctxt != NULL)
		fatal("%s: already called", __func__);
	sshpam_ctxt = (sshpam_device.init_ctx)(authctxt);
	sshpam_authok = NULL;
	sshbuf_reset(m);
	if (sshpam_ctxt != NULL) {
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_FREE_CTX, 1);
		monitor_permit(mon_dispatch, MONITOR_REQ_PAM_QUERY, 1);
		ok = 1;
	}
	if ((r = sshbuf_put_u32(m, ok)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	mm_request_send(sock, MONITOR_ANS_PAM_INIT_CTX, m);
	return (0);
}

int
mm_answer_pam_query(struct ssh *ssh, int sock, struct sshbuf *m)
{
	char *name = NULL, *info = NULL, **prompts = NULL;
	u_int i, num = 0, *echo_on = 0;
	int r, ret;

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
	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, ret)) != 0 ||
	    (r = sshbuf_put_cstring(m, name)) != 0 ||
	    (r = sshbuf_put_cstring(m, info)) != 0 ||
	    (r = sshbuf_put_u32(m, sshpam_get_maxtries_reached())) != 0 ||
	    (r = sshbuf_put_u32(m, num)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	free(name);
	free(info);
	for (i = 0; i < num; ++i) {
		if ((r = sshbuf_put_cstring(m, prompts[i])) != 0 ||
		    (r = sshbuf_put_u32(m, echo_on[i])) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
		free(prompts[i]);
	}
	free(prompts);
	free(echo_on);
	auth_method = "keyboard-interactive";
	auth_submethod = "pam";
	mm_request_send(sock, MONITOR_ANS_PAM_QUERY, m);
	return (0);
}

int
mm_answer_pam_respond(struct ssh *ssh, int sock, struct sshbuf *m)
{
	char **resp;
	u_int i, num;
	int r, ret;

	debug3("%s", __func__);
	if (sshpam_ctxt == NULL)
		fatal("%s: no context", __func__);
	sshpam_authok = NULL;
	if ((r = sshbuf_get_u32(m, &num)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	if (num > 0) {
		resp = xcalloc(num, sizeof(char *));
		for (i = 0; i < num; ++i) {
			if ((r = sshbuf_get_cstring(m, &(resp[i]), NULL)) != 0)
				fatal("%s: buffer error: %s",
				    __func__, ssh_err(r));
		}
		ret = (sshpam_device.respond)(sshpam_ctxt, num, resp);
		for (i = 0; i < num; ++i)
			free(resp[i]);
		free(resp);
	} else {
		ret = (sshpam_device.respond)(sshpam_ctxt, num, NULL);
	}
	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, ret)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	mm_request_send(sock, MONITOR_ANS_PAM_RESPOND, m);
	auth_method = "keyboard-interactive";
	auth_submethod = "pam";
	if (ret == 0)
		sshpam_authok = sshpam_ctxt;
	return (0);
}

int
mm_answer_pam_free_ctx(struct ssh *ssh, int sock, struct sshbuf *m)
{
	int r = sshpam_authok != NULL && sshpam_authok == sshpam_ctxt;

	debug3("%s", __func__);
	if (sshpam_ctxt == NULL)
		fatal("%s: no context", __func__);
	(sshpam_device.free_ctx)(sshpam_ctxt);
	sshpam_ctxt = sshpam_authok = NULL;
	sshbuf_reset(m);
	mm_request_send(sock, MONITOR_ANS_PAM_FREE_CTX, m);
	/* Allow another attempt */
	monitor_permit(mon_dispatch, MONITOR_REQ_PAM_INIT_CTX, 1);
	auth_method = "keyboard-interactive";
	auth_submethod = "pam";
	return r;
}
#endif

int
mm_answer_keyallowed(struct ssh *ssh, int sock, struct sshbuf *m)
{
	struct sshkey *key = NULL;
	char *cuser, *chost;
	u_int pubkey_auth_attempt;
	u_int type = 0;
	int r, allowed = 0;
	struct sshauthopt *opts = NULL;

	debug3_f("entering");
	if ((r = sshbuf_get_u32(m, &type)) != 0 ||
	    (r = sshbuf_get_cstring(m, &cuser, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(m, &chost, NULL)) != 0 ||
	    (r = sshkey_froms(m, &key)) != 0 ||
	    (r = sshbuf_get_u32(m, &pubkey_auth_attempt)) != 0)
		fatal_fr(r, "parse");

	if (key != NULL && authctxt->valid) {
		/* These should not make it past the privsep child */
		if (sshkey_type_plain(key->type) == KEY_RSA &&
		    (ssh->compat & SSH_BUG_RSASIGMD5) != 0)
			fatal_f("passed a SSH_BUG_RSASIGMD5 key");

		switch (type) {
		case MM_USERKEY:
			auth_method = "publickey";
			if (!options.pubkey_authentication)
				break;
			if (auth2_key_already_used(authctxt, key))
				break;
			if (!key_base_type_match(auth_method, key,
			    options.pubkey_accepted_algos))
				break;
			allowed = user_key_allowed(ssh, authctxt->pw, key,
			    pubkey_auth_attempt, &opts);
			break;
		case MM_HOSTKEY:
			auth_method = "hostbased";
			if (!options.hostbased_authentication)
				break;
			if (auth2_key_already_used(authctxt, key))
				break;
			if (!key_base_type_match(auth_method, key,
			    options.hostbased_accepted_algos))
				break;
			allowed = hostbased_key_allowed(ssh, authctxt->pw,
			    cuser, chost, key);
			auth2_record_info(authctxt,
			    "client user \"%.100s\", client host \"%.100s\"",
			    cuser, chost);
			break;
		default:
			fatal_f("unknown key type %u", type);
			break;
		}
	}

	debug3_f("%s authentication%s: %s key is %s", auth_method,
	    pubkey_auth_attempt ? "" : " test",
	    (key == NULL || !authctxt->valid) ? "invalid" : sshkey_type(key),
	    allowed ? "allowed" : "not allowed");

	auth2_record_key(authctxt, 0, key);

	/* clear temporarily storage (used by verify) */
	monitor_reset_key_state();

	if (allowed) {
		/* Save temporarily for comparison in verify */
		if ((r = sshkey_to_blob(key, &key_blob, &key_bloblen)) != 0)
			fatal_fr(r, "sshkey_to_blob");
		key_blobtype = type;
		key_opts = opts;
		hostbased_cuser = cuser;
		hostbased_chost = chost;
	} else {
		/* Log failed attempt */
		auth_log(ssh, 0, 0, auth_method, NULL);
		free(cuser);
		free(chost);
	}
	sshkey_free(key);

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, allowed)) != 0)
		fatal_fr(r, "assemble");
	if (opts != NULL && (r = sshauthopt_serialise(opts, m, 1)) != 0)
		fatal_fr(r, "sshauthopt_serialise");
	mm_request_send(sock, MONITOR_ANS_KEYALLOWED, m);

	if (!allowed)
		sshauthopt_free(opts);

	return (0);
}

static int
monitor_valid_userblob(struct ssh *ssh, const u_char *data, u_int datalen)
{
	struct sshbuf *b;
	struct sshkey *hostkey = NULL;
	const u_char *p;
	char *userstyle, *cp;
	size_t len;
	u_char type;
	int hostbound = 0, r, fail = 0;

	if ((b = sshbuf_from(data, datalen)) == NULL)
		fatal_f("sshbuf_from");

	if (ssh->compat & SSH_OLD_SESSIONID) {
		p = sshbuf_ptr(b);
		len = sshbuf_len(b);
		if ((session_id2 == NULL) ||
		    (len < session_id2_len) ||
		    (timingsafe_bcmp(p, session_id2, session_id2_len) != 0))
			fail++;
		if ((r = sshbuf_consume(b, session_id2_len)) != 0)
			fatal_fr(r, "consume");
	} else {
		if ((r = sshbuf_get_string_direct(b, &p, &len)) != 0)
			fatal_fr(r, "parse sessionid");
		if ((session_id2 == NULL) ||
		    (len != session_id2_len) ||
		    (timingsafe_bcmp(p, session_id2, session_id2_len) != 0))
			fail++;
	}
	if ((r = sshbuf_get_u8(b, &type)) != 0)
		fatal_fr(r, "parse type");
	if (type != SSH2_MSG_USERAUTH_REQUEST)
		fail++;
	if ((r = sshbuf_get_cstring(b, &cp, NULL)) != 0)
		fatal_fr(r, "parse userstyle");
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
	if ((r = sshbuf_skip_string(b)) != 0 ||	/* service */
	    (r = sshbuf_get_cstring(b, &cp, NULL)) != 0)
		fatal_fr(r, "parse method");
	if (strcmp("publickey", cp) != 0) {
		if (strcmp("publickey-hostbound-v00@openssh.com", cp) == 0)
			hostbound = 1;
		else
			fail++;
	}
	free(cp);
	if ((r = sshbuf_get_u8(b, &type)) != 0)
		fatal_fr(r, "parse pktype");
	if (type == 0)
		fail++;
	if ((r = sshbuf_skip_string(b)) != 0 ||	/* pkalg */
	    (r = sshbuf_skip_string(b)) != 0 ||	/* pkblob */
	    (hostbound && (r = sshkey_froms(b, &hostkey)) != 0))
		fatal_fr(r, "parse pk");
	if (sshbuf_len(b) != 0)
		fail++;
	sshbuf_free(b);
	if (hostkey != NULL) {
		/*
		 * Ensure this is actually one of our hostkeys; unfortunately
		 * can't check ssh->kex->initial_hostkey directly at this point
		 * as packet state has not yet been exported to monitor.
		 */
		if (get_hostkey_index(hostkey, 1, ssh) == -1)
			fatal_f("hostbound hostkey does not match");
		sshkey_free(hostkey);
	}
	return (fail == 0);
}

static int
monitor_valid_hostbasedblob(const u_char *data, u_int datalen,
    const char *cuser, const char *chost)
{
	struct sshbuf *b;
	const u_char *p;
	char *cp, *userstyle;
	size_t len;
	int r, fail = 0;
	u_char type;

	if ((b = sshbuf_from(data, datalen)) == NULL)
		fatal_f("sshbuf_new");
	if ((r = sshbuf_get_string_direct(b, &p, &len)) != 0)
		fatal_fr(r, "parse sessionid");

	if ((session_id2 == NULL) ||
	    (len != session_id2_len) ||
	    (timingsafe_bcmp(p, session_id2, session_id2_len) != 0))
		fail++;

	if ((r = sshbuf_get_u8(b, &type)) != 0)
		fatal_fr(r, "parse type");
	if (type != SSH2_MSG_USERAUTH_REQUEST)
		fail++;
	if ((r = sshbuf_get_cstring(b, &cp, NULL)) != 0)
		fatal_fr(r, "parse userstyle");
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
	if ((r = sshbuf_skip_string(b)) != 0 ||	/* service */
	    (r = sshbuf_get_cstring(b, &cp, NULL)) != 0)
		fatal_fr(r, "parse method");
	if (strcmp(cp, "hostbased") != 0)
		fail++;
	free(cp);
	if ((r = sshbuf_skip_string(b)) != 0 ||	/* pkalg */
	    (r = sshbuf_skip_string(b)) != 0)	/* pkblob */
		fatal_fr(r, "parse pk");

	/* verify client host, strip trailing dot if necessary */
	if ((r = sshbuf_get_cstring(b, &cp, NULL)) != 0)
		fatal_fr(r, "parse host");
	if (((len = strlen(cp)) > 0) && cp[len - 1] == '.')
		cp[len - 1] = '\0';
	if (strcmp(cp, chost) != 0)
		fail++;
	free(cp);

	/* verify client user */
	if ((r = sshbuf_get_cstring(b, &cp, NULL)) != 0)
		fatal_fr(r, "parse ruser");
	if (strcmp(cp, cuser) != 0)
		fail++;
	free(cp);

	if (sshbuf_len(b) != 0)
		fail++;
	sshbuf_free(b);
	return (fail == 0);
}

int
mm_answer_keyverify(struct ssh *ssh, int sock, struct sshbuf *m)
{
	struct sshkey *key;
	const u_char *signature, *data, *blob;
	char *sigalg = NULL, *fp = NULL;
	size_t signaturelen, datalen, bloblen;
	int r, ret, req_presence = 0, req_verify = 0, valid_data = 0;
	int encoded_ret;
	struct sshkey_sig_details *sig_details = NULL;

	if ((r = sshbuf_get_string_direct(m, &blob, &bloblen)) != 0 ||
	    (r = sshbuf_get_string_direct(m, &signature, &signaturelen)) != 0 ||
	    (r = sshbuf_get_string_direct(m, &data, &datalen)) != 0 ||
	    (r = sshbuf_get_cstring(m, &sigalg, NULL)) != 0)
		fatal_fr(r, "parse");

	if (hostbased_cuser == NULL || hostbased_chost == NULL ||
	  !monitor_allowed_key(blob, bloblen))
		fatal_f("bad key, not previously allowed");

	/* Empty signature algorithm means NULL. */
	if (*sigalg == '\0') {
		free(sigalg);
		sigalg = NULL;
	}

	/* XXX use sshkey_froms here; need to change key_blob, etc. */
	if ((r = sshkey_from_blob(blob, bloblen, &key)) != 0)
		fatal_fr(r, "parse key");

	switch (key_blobtype) {
	case MM_USERKEY:
		valid_data = monitor_valid_userblob(ssh, data, datalen);
		auth_method = "publickey";
		break;
	case MM_HOSTKEY:
		valid_data = monitor_valid_hostbasedblob(data, datalen,
		    hostbased_cuser, hostbased_chost);
		auth_method = "hostbased";
		break;
	default:
		valid_data = 0;
		break;
	}
	if (!valid_data)
		fatal_f("bad %s signature data blob",
		    key_blobtype == MM_USERKEY ? "userkey" :
		    (key_blobtype == MM_HOSTKEY ? "hostkey" : "unknown"));

	if ((fp = sshkey_fingerprint(key, options.fingerprint_hash,
	    SSH_FP_DEFAULT)) == NULL)
		fatal_f("sshkey_fingerprint failed");

	ret = sshkey_verify(key, signature, signaturelen, data, datalen,
	    sigalg, ssh->compat, &sig_details);
	debug3_f("%s %s signature using %s %s%s%s", auth_method,
	    sshkey_type(key), sigalg == NULL ? "default" : sigalg,
	    (ret == 0) ? "verified" : "unverified",
	    (ret != 0) ? ": " : "", (ret != 0) ? ssh_err(ret) : "");

	if (ret == 0 && key_blobtype == MM_USERKEY && sig_details != NULL) {
		req_presence = (options.pubkey_auth_options &
		    PUBKEYAUTH_TOUCH_REQUIRED) ||
		    !key_opts->no_require_user_presence;
		if (req_presence &&
		    (sig_details->sk_flags & SSH_SK_USER_PRESENCE_REQD) == 0) {
			error("public key %s %s signature for %s%s from %.128s "
			    "port %d rejected: user presence "
			    "(authenticator touch) requirement not met ",
			    sshkey_type(key), fp,
			    authctxt->valid ? "" : "invalid user ",
			    authctxt->user, ssh_remote_ipaddr(ssh),
			    ssh_remote_port(ssh));
			ret = SSH_ERR_SIGNATURE_INVALID;
		}
		req_verify = (options.pubkey_auth_options &
		    PUBKEYAUTH_VERIFY_REQUIRED) || key_opts->require_verify;
		if (req_verify &&
		    (sig_details->sk_flags & SSH_SK_USER_VERIFICATION_REQD) == 0) {
			error("public key %s %s signature for %s%s from %.128s "
			    "port %d rejected: user verification requirement "
			    "not met ", sshkey_type(key), fp,
			    authctxt->valid ? "" : "invalid user ",
			    authctxt->user, ssh_remote_ipaddr(ssh),
			    ssh_remote_port(ssh));
			ret = SSH_ERR_SIGNATURE_INVALID;
		}
	}
	auth2_record_key(authctxt, ret == 0, key);

	if (key_blobtype == MM_USERKEY)
		auth_activate_options(ssh, key_opts);
	monitor_reset_key_state();

	sshbuf_reset(m);

	/* encode ret != 0 as positive integer, since we're sending u32 */
	encoded_ret = (ret != 0);
	if ((r = sshbuf_put_u32(m, encoded_ret)) != 0 ||
	    (r = sshbuf_put_u8(m, sig_details != NULL)) != 0)
		fatal_fr(r, "assemble");
	if (sig_details != NULL) {
		if ((r = sshbuf_put_u32(m, sig_details->sk_counter)) != 0 ||
		    (r = sshbuf_put_u8(m, sig_details->sk_flags)) != 0)
			fatal_fr(r, "assemble sk");
	}
	sshkey_sig_details_free(sig_details);
	mm_request_send(sock, MONITOR_ANS_KEYVERIFY, m);

	free(sigalg);
	free(fp);
	sshkey_free(key);

	return ret == 0;
}

static void
mm_record_login(struct ssh *ssh, Session *s, struct passwd *pw)
{
	socklen_t fromlen;
	struct sockaddr_storage from;

	/*
	 * Get IP address of client. If the connection is not a socket, let
	 * the address be 0.0.0.0.
	 */
	memset(&from, 0, sizeof(from));
	fromlen = sizeof(from);
	if (ssh_packet_connection_is_on_socket(ssh)) {
		if (getpeername(ssh_packet_get_connection_in(ssh),
		    (struct sockaddr *)&from, &fromlen) == -1) {
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
	debug3_f("session %d pid %ld", s->self, (long)s->pid);
	if (s->ttyfd != -1) {
		debug3_f("tty %s ptyfd %d", s->tty, s->ptyfd);
		session_pty_cleanup2(s);
	}
	session_unused(s->self);
}

int
mm_answer_pty(struct ssh *ssh, int sock, struct sshbuf *m)
{
	extern struct monitor *pmonitor;
	Session *s;
	int r, res, fd0;

	debug3_f("entering");

	sshbuf_reset(m);
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

	if ((r = sshbuf_put_u32(m, 1)) != 0 ||
	    (r = sshbuf_put_cstring(m, s->tty)) != 0)
		fatal_fr(r, "assemble");

	/* We need to trick ttyslot */
	if (dup2(s->ttyfd, 0) == -1)
		fatal_f("dup2");

	mm_record_login(ssh, s, authctxt->pw);

	/* Now we can close the file descriptor again */
	close(0);

	/* send messages generated by record_login */
	if ((r = sshbuf_put_stringb(m, loginmsg)) != 0)
		fatal_fr(r, "assemble loginmsg");
	sshbuf_reset(loginmsg);

	mm_request_send(sock, MONITOR_ANS_PTY, m);

	if (mm_send_fd(sock, s->ptyfd) == -1 ||
	    mm_send_fd(sock, s->ttyfd) == -1)
		fatal_f("send fds failed");

	/* make sure nothing uses fd 0 */
	if ((fd0 = open(_PATH_DEVNULL, O_RDONLY)) == -1)
		fatal_f("open(/dev/null): %s", strerror(errno));
	if (fd0 != 0)
		error_f("fd0 %d != 0", fd0);

	/* slave side of pty is not needed */
	close(s->ttyfd);
	s->ttyfd = s->ptyfd;
	/* no need to dup() because nobody closes ptyfd */
	s->ptymaster = s->ptyfd;

	debug3_f("tty %s ptyfd %d", s->tty, s->ttyfd);

	return (0);

 error:
	if (s != NULL)
		mm_session_close(s);
	if ((r = sshbuf_put_u32(m, 0)) != 0)
		fatal_fr(r, "assemble 0");
	mm_request_send(sock, MONITOR_ANS_PTY, m);
	return (0);
}

int
mm_answer_pty_cleanup(struct ssh *ssh, int sock, struct sshbuf *m)
{
	Session *s;
	char *tty;
	int r;

	debug3_f("entering");

	if ((r = sshbuf_get_cstring(m, &tty, NULL)) != 0)
		fatal_fr(r, "parse tty");
	if ((s = session_by_tty(tty)) != NULL)
		mm_session_close(s);
	sshbuf_reset(m);
	free(tty);
	return (0);
}

int
mm_answer_term(struct ssh *ssh, int sock, struct sshbuf *req)
{
	extern struct monitor *pmonitor;
	int res, status;

	debug3_f("tearing down sessions");

	/* The child is terminating */
	session_destroy_all(ssh, &mm_session_close);

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
mm_answer_audit_event(struct ssh *ssh, int socket, struct sshbuf *m)
{
	u_int n;
	ssh_audit_event_t event;
	int r;

	debug3("%s entering", __func__);

	if ((r = sshbuf_get_u32(m, &n)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	event = (ssh_audit_event_t)n;
	switch (event) {
	case SSH_AUTH_FAIL_PUBKEY:
	case SSH_AUTH_FAIL_HOSTBASED:
	case SSH_AUTH_FAIL_GSSAPI:
	case SSH_LOGIN_EXCEED_MAXTRIES:
	case SSH_LOGIN_ROOT_DENIED:
	case SSH_CONNECTION_CLOSE:
	case SSH_INVALID_USER:
		audit_event(ssh, event);
		break;
	default:
		fatal("Audit event type %d not permitted", event);
	}

	return (0);
}

int
mm_answer_audit_command(struct ssh *ssh, int socket, struct sshbuf *m)
{
	char *cmd;
	int r;

	debug3("%s entering", __func__);
	if ((r = sshbuf_get_cstring(m, &cmd, NULL)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	/* sanity check command, if so how? */
	audit_run_command(cmd);
	free(cmd);
	return (0);
}
#endif /* SSH_AUDIT_EVENTS */

void
monitor_clear_keystate(struct ssh *ssh, struct monitor *pmonitor)
{
	ssh_clear_newkeys(ssh, MODE_IN);
	ssh_clear_newkeys(ssh, MODE_OUT);
	sshbuf_free(child_state);
	child_state = NULL;
}

void
monitor_apply_keystate(struct ssh *ssh, struct monitor *pmonitor)
{
	struct kex *kex;
	int r;

	debug3_f("packet_set_state");
	if ((r = ssh_packet_set_state(ssh, child_state)) != 0)
		fatal_fr(r, "packet_set_state");
	sshbuf_free(child_state);
	child_state = NULL;
	if ((kex = ssh->kex) == NULL)
		fatal_f("internal error: ssh->kex == NULL");
	if (session_id2_len != sshbuf_len(ssh->kex->session_id)) {
		fatal_f("incorrect session id length %zu (expected %u)",
		    sshbuf_len(ssh->kex->session_id), session_id2_len);
	}
	if (memcmp(sshbuf_ptr(ssh->kex->session_id), session_id2,
	    session_id2_len) != 0)
		fatal_f("session ID mismatch");
	/* XXX set callbacks */
#ifdef WITH_OPENSSL
	kex->kex[KEX_DH_GRP1_SHA1] = kex_gen_server;
	kex->kex[KEX_DH_GRP14_SHA1] = kex_gen_server;
	kex->kex[KEX_DH_GRP14_SHA256] = kex_gen_server;
	kex->kex[KEX_DH_GRP16_SHA512] = kex_gen_server;
	kex->kex[KEX_DH_GRP18_SHA512] = kex_gen_server;
	kex->kex[KEX_DH_GEX_SHA1] = kexgex_server;
	kex->kex[KEX_DH_GEX_SHA256] = kexgex_server;
# ifdef OPENSSL_HAS_ECC
	kex->kex[KEX_ECDH_SHA2] = kex_gen_server;
# endif
#endif /* WITH_OPENSSL */
	kex->kex[KEX_C25519_SHA256] = kex_gen_server;
	kex->kex[KEX_KEM_SNTRUP761X25519_SHA512] = kex_gen_server;
	kex->load_host_public_key=&get_hostkey_public_by_type;
	kex->load_host_private_key=&get_hostkey_private_by_type;
	kex->host_key_index=&get_hostkey_index;
	kex->sign = sshd_hostkey_sign;
}

/* This function requires careful sanity checking */

void
mm_get_keystate(struct ssh *ssh, struct monitor *pmonitor)
{
	debug3_f("Waiting for new keys");

	if ((child_state = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	mm_request_receive_expect(pmonitor->m_sendfd, MONITOR_REQ_KEYEXPORT,
	    child_state);
	debug3_f("GOT new keys");
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
#ifdef SO_ZEROIZE
	int on = 1;
#endif

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
		fatal_f("socketpair: %s", strerror(errno));
#ifdef SO_ZEROIZE
	if (setsockopt(pair[0], SOL_SOCKET, SO_ZEROIZE, &on, sizeof(on)) == -1)
		error("setsockopt SO_ZEROIZE(0): %.100s", strerror(errno));
	if (setsockopt(pair[1], SOL_SOCKET, SO_ZEROIZE, &on, sizeof(on)) == -1)
		error("setsockopt SO_ZEROIZE(1): %.100s", strerror(errno));
#endif
	FD_CLOSEONEXEC(pair[0]);
	FD_CLOSEONEXEC(pair[1]);
	mon->m_recvfd = pair[0];
	mon->m_sendfd = pair[1];

	if (do_logfds) {
		if (pipe(pair) == -1)
			fatal_f("pipe: %s", strerror(errno));
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
mm_answer_gss_setup_ctx(struct ssh *ssh, int sock, struct sshbuf *m)
{
	gss_OID_desc goid;
	OM_uint32 major;
	size_t len;
	u_char *p;
	int r;

	if (!options.gss_authentication)
		fatal_f("GSSAPI authentication not enabled");

	if ((r = sshbuf_get_string(m, &p, &len)) != 0)
		fatal_fr(r, "parse");
	goid.elements = p;
	goid.length = len;

	major = ssh_gssapi_server_ctx(&gsscontext, &goid);

	free(goid.elements);

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, major)) != 0)
		fatal_fr(r, "assemble");

	mm_request_send(sock, MONITOR_ANS_GSSSETUP, m);

	/* Now we have a context, enable the step */
	monitor_permit(mon_dispatch, MONITOR_REQ_GSSSTEP, 1);

	return (0);
}

int
mm_answer_gss_accept_ctx(struct ssh *ssh, int sock, struct sshbuf *m)
{
	gss_buffer_desc in;
	gss_buffer_desc out = GSS_C_EMPTY_BUFFER;
	OM_uint32 major, minor;
	OM_uint32 flags = 0; /* GSI needs this */
	int r;

	if (!options.gss_authentication)
		fatal_f("GSSAPI authentication not enabled");

	if ((r = ssh_gssapi_get_buffer_desc(m, &in)) != 0)
		fatal_fr(r, "ssh_gssapi_get_buffer_desc");
	major = ssh_gssapi_accept_ctx(gsscontext, &in, &out, &flags);
	free(in.value);

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, major)) != 0 ||
	    (r = sshbuf_put_string(m, out.value, out.length)) != 0 ||
	    (r = sshbuf_put_u32(m, flags)) != 0)
		fatal_fr(r, "assemble");
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
mm_answer_gss_checkmic(struct ssh *ssh, int sock, struct sshbuf *m)
{
	gss_buffer_desc gssbuf, mic;
	OM_uint32 ret;
	int r;

	if (!options.gss_authentication)
		fatal_f("GSSAPI authentication not enabled");

	if ((r = ssh_gssapi_get_buffer_desc(m, &gssbuf)) != 0 ||
	    (r = ssh_gssapi_get_buffer_desc(m, &mic)) != 0)
		fatal_fr(r, "ssh_gssapi_get_buffer_desc");

	ret = ssh_gssapi_checkmic(gsscontext, &gssbuf, &mic);

	free(gssbuf.value);
	free(mic.value);

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, ret)) != 0)
		fatal_fr(r, "assemble");

	mm_request_send(sock, MONITOR_ANS_GSSCHECKMIC, m);

	if (!GSS_ERROR(ret))
		monitor_permit(mon_dispatch, MONITOR_REQ_GSSUSEROK, 1);

	return (0);
}

int
mm_answer_gss_userok(struct ssh *ssh, int sock, struct sshbuf *m)
{
	int r, authenticated;
	const char *displayname;

	if (!options.gss_authentication)
		fatal_f("GSSAPI authentication not enabled");

	authenticated = authctxt->valid && ssh_gssapi_userok(authctxt->user);

	sshbuf_reset(m);
	if ((r = sshbuf_put_u32(m, authenticated)) != 0)
		fatal_fr(r, "assemble");

	debug3_f("sending result %d", authenticated);
	mm_request_send(sock, MONITOR_ANS_GSSUSEROK, m);

	auth_method = "gssapi-with-mic";

	if ((displayname = ssh_gssapi_displayname()) != NULL)
		auth2_record_info(authctxt, "%s", displayname);

	/* Monitor loop will terminate if authenticated */
	return (authenticated);
}
#endif /* GSSAPI */

