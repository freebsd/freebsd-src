/* $OpenBSD: monitor_wrap.c,v 1.85 2015/05/01 03:23:51 djm Exp $ */
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
#include <sys/uio.h>

#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef WITH_OPENSSL
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/evp.h>
#endif

#include "openbsd-compat/sys-queue.h"
#include "xmalloc.h"
#include "ssh.h"
#ifdef WITH_OPENSSL
#include "dh.h"
#endif
#include "buffer.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "hostfile.h"
#include "auth.h"
#include "auth-options.h"
#include "packet.h"
#include "mac.h"
#include "log.h"
#ifdef TARGET_OS_MAC    /* XXX Broken krb5 headers on Mac */
#undef TARGET_OS_MAC
#include "zlib.h"
#define TARGET_OS_MAC 1
#else
#include "zlib.h"
#endif
#include "monitor.h"
#ifdef GSSAPI
#include "ssh-gss.h"
#endif
#include "monitor_wrap.h"
#include "atomicio.h"
#include "monitor_fdpass.h"
#include "misc.h"
#include "uuencode.h"

#include "channels.h"
#include "session.h"
#include "servconf.h"
#include "roaming.h"

#include "ssherr.h"

/* Imports */
extern int compat20;
extern z_stream incoming_stream;
extern z_stream outgoing_stream;
extern struct monitor *pmonitor;
extern Buffer loginmsg;
extern ServerOptions options;

void
mm_log_handler(LogLevel level, const char *msg, void *ctx)
{
	Buffer log_msg;
	struct monitor *mon = (struct monitor *)ctx;

	if (mon->m_log_sendfd == -1)
		fatal("%s: no log channel", __func__);

	buffer_init(&log_msg);
	/*
	 * Placeholder for packet length. Will be filled in with the actual
	 * packet length once the packet has been constucted. This saves
	 * fragile math.
	 */
	buffer_put_int(&log_msg, 0);

	buffer_put_int(&log_msg, level);
	buffer_put_cstring(&log_msg, msg);
	put_u32(buffer_ptr(&log_msg), buffer_len(&log_msg) - 4);
	if (atomicio(vwrite, mon->m_log_sendfd, buffer_ptr(&log_msg),
	    buffer_len(&log_msg)) != buffer_len(&log_msg))
		fatal("%s: write: %s", __func__, strerror(errno));
	buffer_free(&log_msg);
}

int
mm_is_monitor(void)
{
	/*
	 * m_pid is only set in the privileged part, and
	 * points to the unprivileged child.
	 */
	return (pmonitor && pmonitor->m_pid > 0);
}

void
mm_request_send(int sock, enum monitor_reqtype type, Buffer *m)
{
	u_int mlen = buffer_len(m);
	u_char buf[5];

	debug3("%s entering: type %d", __func__, type);

	put_u32(buf, mlen + 1);
	buf[4] = (u_char) type;		/* 1st byte of payload is mesg-type */
	if (atomicio(vwrite, sock, buf, sizeof(buf)) != sizeof(buf))
		fatal("%s: write: %s", __func__, strerror(errno));
	if (atomicio(vwrite, sock, buffer_ptr(m), mlen) != mlen)
		fatal("%s: write: %s", __func__, strerror(errno));
}

void
mm_request_receive(int sock, Buffer *m)
{
	u_char buf[4];
	u_int msg_len;

	debug3("%s entering", __func__);

	if (atomicio(read, sock, buf, sizeof(buf)) != sizeof(buf)) {
		if (errno == EPIPE)
			cleanup_exit(255);
		fatal("%s: read: %s", __func__, strerror(errno));
	}
	msg_len = get_u32(buf);
	if (msg_len > 256 * 1024)
		fatal("%s: read: bad msg_len %d", __func__, msg_len);
	buffer_clear(m);
	buffer_append_space(m, msg_len);
	if (atomicio(read, sock, buffer_ptr(m), msg_len) != msg_len)
		fatal("%s: read: %s", __func__, strerror(errno));
}

void
mm_request_receive_expect(int sock, enum monitor_reqtype type, Buffer *m)
{
	u_char rtype;

	debug3("%s entering: type %d", __func__, type);

	mm_request_receive(sock, m);
	rtype = buffer_get_char(m);
	if (rtype != type)
		fatal("%s: read: rtype %d != type %d", __func__,
		    rtype, type);
}

#ifdef WITH_OPENSSL
DH *
mm_choose_dh(int min, int nbits, int max)
{
	BIGNUM *p, *g;
	int success = 0;
	Buffer m;

	buffer_init(&m);
	buffer_put_int(&m, min);
	buffer_put_int(&m, nbits);
	buffer_put_int(&m, max);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_MODULI, &m);

	debug3("%s: waiting for MONITOR_ANS_MODULI", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_MODULI, &m);

	success = buffer_get_char(&m);
	if (success == 0)
		fatal("%s: MONITOR_ANS_MODULI failed", __func__);

	if ((p = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);
	if ((g = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);
	buffer_get_bignum2(&m, p);
	buffer_get_bignum2(&m, g);

	debug3("%s: remaining %d", __func__, buffer_len(&m));
	buffer_free(&m);

	return (dh_new_group(g, p));
}
#endif

int
mm_key_sign(Key *key, u_char **sigp, u_int *lenp,
    const u_char *data, u_int datalen)
{
	struct kex *kex = *pmonitor->m_pkex;
	Buffer m;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_int(&m, kex->host_key_index(key, 0, active_state));
	buffer_put_string(&m, data, datalen);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_SIGN, &m);

	debug3("%s: waiting for MONITOR_ANS_SIGN", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_SIGN, &m);
	*sigp  = buffer_get_string(&m, lenp);
	buffer_free(&m);

	return (0);
}

struct passwd *
mm_getpwnamallow(const char *username)
{
	Buffer m;
	struct passwd *pw;
	u_int len, i;
	ServerOptions *newopts;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_cstring(&m, username);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PWNAM, &m);

	debug3("%s: waiting for MONITOR_ANS_PWNAM", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_PWNAM, &m);

	if (buffer_get_char(&m) == 0) {
		pw = NULL;
		goto out;
	}
	pw = buffer_get_string(&m, &len);
	if (len != sizeof(struct passwd))
		fatal("%s: struct passwd size mismatch", __func__);
	pw->pw_name = buffer_get_string(&m, NULL);
	pw->pw_passwd = buffer_get_string(&m, NULL);
#ifdef HAVE_STRUCT_PASSWD_PW_GECOS
	pw->pw_gecos = buffer_get_string(&m, NULL);
#endif
#ifdef HAVE_STRUCT_PASSWD_PW_CLASS
	pw->pw_class = buffer_get_string(&m, NULL);
#endif
	pw->pw_dir = buffer_get_string(&m, NULL);
	pw->pw_shell = buffer_get_string(&m, NULL);

out:
	/* copy options block as a Match directive may have changed some */
	newopts = buffer_get_string(&m, &len);
	if (len != sizeof(*newopts))
		fatal("%s: option block size mismatch", __func__);

#define M_CP_STROPT(x) do { \
		if (newopts->x != NULL) \
			newopts->x = buffer_get_string(&m, NULL); \
	} while (0)
#define M_CP_STRARRAYOPT(x, nx) do { \
		for (i = 0; i < newopts->nx; i++) \
			newopts->x[i] = buffer_get_string(&m, NULL); \
	} while (0)
	/* See comment in servconf.h */
	COPY_MATCH_STRING_OPTS();
#undef M_CP_STROPT
#undef M_CP_STRARRAYOPT

	copy_set_server_options(&options, newopts, 1);
	free(newopts);

	buffer_free(&m);

	return (pw);
}

char *
mm_auth2_read_banner(void)
{
	Buffer m;
	char *banner;

	debug3("%s entering", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_AUTH2_READ_BANNER, &m);
	buffer_clear(&m);

	mm_request_receive_expect(pmonitor->m_recvfd,
	    MONITOR_ANS_AUTH2_READ_BANNER, &m);
	banner = buffer_get_string(&m, NULL);
	buffer_free(&m);

	/* treat empty banner as missing banner */
	if (strlen(banner) == 0) {
		free(banner);
		banner = NULL;
	}
	return (banner);
}

/* Inform the privileged process about service and style */

void
mm_inform_authserv(char *service, char *style)
{
	Buffer m;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_cstring(&m, service);
	buffer_put_cstring(&m, style ? style : "");

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_AUTHSERV, &m);

	buffer_free(&m);
}

/* Do the password authentication */
int
mm_auth_password(Authctxt *authctxt, char *password)
{
	Buffer m;
	int authenticated = 0;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_cstring(&m, password);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_AUTHPASSWORD, &m);

	debug3("%s: waiting for MONITOR_ANS_AUTHPASSWORD", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_AUTHPASSWORD, &m);

	authenticated = buffer_get_int(&m);

	buffer_free(&m);

	debug3("%s: user %sauthenticated",
	    __func__, authenticated ? "" : "not ");
	return (authenticated);
}

int
mm_user_key_allowed(struct passwd *pw, Key *key, int pubkey_auth_attempt)
{
	return (mm_key_allowed(MM_USERKEY, NULL, NULL, key,
	    pubkey_auth_attempt));
}

int
mm_hostbased_key_allowed(struct passwd *pw, char *user, char *host,
    Key *key)
{
	return (mm_key_allowed(MM_HOSTKEY, user, host, key, 0));
}

int
mm_auth_rhosts_rsa_key_allowed(struct passwd *pw, char *user,
    char *host, Key *key)
{
	int ret;

	key->type = KEY_RSA; /* XXX hack for key_to_blob */
	ret = mm_key_allowed(MM_RSAHOSTKEY, user, host, key, 0);
	key->type = KEY_RSA1;
	return (ret);
}

int
mm_key_allowed(enum mm_keytype type, char *user, char *host, Key *key,
    int pubkey_auth_attempt)
{
	Buffer m;
	u_char *blob;
	u_int len;
	int allowed = 0, have_forced = 0;

	debug3("%s entering", __func__);

	/* Convert the key to a blob and the pass it over */
	if (!key_to_blob(key, &blob, &len))
		return (0);

	buffer_init(&m);
	buffer_put_int(&m, type);
	buffer_put_cstring(&m, user ? user : "");
	buffer_put_cstring(&m, host ? host : "");
	buffer_put_string(&m, blob, len);
	buffer_put_int(&m, pubkey_auth_attempt);
	free(blob);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_KEYALLOWED, &m);

	debug3("%s: waiting for MONITOR_ANS_KEYALLOWED", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_KEYALLOWED, &m);

	allowed = buffer_get_int(&m);

	/* fake forced command */
	auth_clear_options();
	have_forced = buffer_get_int(&m);
	forced_command = have_forced ? xstrdup("true") : NULL;

	buffer_free(&m);

	return (allowed);
}

/*
 * This key verify needs to send the key type along, because the
 * privileged parent makes the decision if the key is allowed
 * for authentication.
 */

int
mm_key_verify(Key *key, u_char *sig, u_int siglen, u_char *data, u_int datalen)
{
	Buffer m;
	u_char *blob;
	u_int len;
	int verified = 0;

	debug3("%s entering", __func__);

	/* Convert the key to a blob and the pass it over */
	if (!key_to_blob(key, &blob, &len))
		return (0);

	buffer_init(&m);
	buffer_put_string(&m, blob, len);
	buffer_put_string(&m, sig, siglen);
	buffer_put_string(&m, data, datalen);
	free(blob);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_KEYVERIFY, &m);

	debug3("%s: waiting for MONITOR_ANS_KEYVERIFY", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_KEYVERIFY, &m);

	verified = buffer_get_int(&m);

	buffer_free(&m);

	return (verified);
}

void
mm_send_keystate(struct monitor *monitor)
{
	struct ssh *ssh = active_state;		/* XXX */
	struct sshbuf *m;
	int r;

	if ((m = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((r = ssh_packet_get_state(ssh, m)) != 0)
		fatal("%s: get_state failed: %s",
		    __func__, ssh_err(r));
	mm_request_send(monitor->m_recvfd, MONITOR_REQ_KEYEXPORT, m);
	debug3("%s: Finished sending state", __func__);
	sshbuf_free(m);
}

int
mm_pty_allocate(int *ptyfd, int *ttyfd, char *namebuf, size_t namebuflen)
{
	Buffer m;
	char *p, *msg;
	int success = 0, tmp1 = -1, tmp2 = -1;

	/* Kludge: ensure there are fds free to receive the pty/tty */
	if ((tmp1 = dup(pmonitor->m_recvfd)) == -1 ||
	    (tmp2 = dup(pmonitor->m_recvfd)) == -1) {
		error("%s: cannot allocate fds for pty", __func__);
		if (tmp1 > 0)
			close(tmp1);
		if (tmp2 > 0)
			close(tmp2);
		return 0;
	}
	close(tmp1);
	close(tmp2);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PTY, &m);

	debug3("%s: waiting for MONITOR_ANS_PTY", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_PTY, &m);

	success = buffer_get_int(&m);
	if (success == 0) {
		debug3("%s: pty alloc failed", __func__);
		buffer_free(&m);
		return (0);
	}
	p = buffer_get_string(&m, NULL);
	msg = buffer_get_string(&m, NULL);
	buffer_free(&m);

	strlcpy(namebuf, p, namebuflen); /* Possible truncation */
	free(p);

	buffer_append(&loginmsg, msg, strlen(msg));
	free(msg);

	if ((*ptyfd = mm_receive_fd(pmonitor->m_recvfd)) == -1 ||
	    (*ttyfd = mm_receive_fd(pmonitor->m_recvfd)) == -1)
		fatal("%s: receive fds failed", __func__);

	/* Success */
	return (1);
}

void
mm_session_pty_cleanup2(Session *s)
{
	Buffer m;

	if (s->ttyfd == -1)
		return;
	buffer_init(&m);
	buffer_put_cstring(&m, s->tty);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PTYCLEANUP, &m);
	buffer_free(&m);

	/* closed dup'ed master */
	if (s->ptymaster != -1 && close(s->ptymaster) < 0)
		error("close(s->ptymaster/%d): %s",
		    s->ptymaster, strerror(errno));

	/* unlink pty from session */
	s->ttyfd = -1;
}

#ifdef USE_PAM
void
mm_start_pam(Authctxt *authctxt)
{
	Buffer m;

	debug3("%s entering", __func__);
	if (!options.use_pam)
		fatal("UsePAM=no, but ended up in %s anyway", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PAM_START, &m);

	buffer_free(&m);
}

u_int
mm_do_pam_account(void)
{
	Buffer m;
	u_int ret;
	char *msg;

	debug3("%s entering", __func__);
	if (!options.use_pam)
		fatal("UsePAM=no, but ended up in %s anyway", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PAM_ACCOUNT, &m);

	mm_request_receive_expect(pmonitor->m_recvfd,
	    MONITOR_ANS_PAM_ACCOUNT, &m);
	ret = buffer_get_int(&m);
	msg = buffer_get_string(&m, NULL);
	buffer_append(&loginmsg, msg, strlen(msg));
	free(msg);

	buffer_free(&m);

	debug3("%s returning %d", __func__, ret);

	return (ret);
}

void *
mm_sshpam_init_ctx(Authctxt *authctxt)
{
	Buffer m;
	int success;

	debug3("%s", __func__);
	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PAM_INIT_CTX, &m);
	debug3("%s: waiting for MONITOR_ANS_PAM_INIT_CTX", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_PAM_INIT_CTX, &m);
	success = buffer_get_int(&m);
	if (success == 0) {
		debug3("%s: pam_init_ctx failed", __func__);
		buffer_free(&m);
		return (NULL);
	}
	buffer_free(&m);
	return (authctxt);
}

int
mm_sshpam_query(void *ctx, char **name, char **info,
    u_int *num, char ***prompts, u_int **echo_on)
{
	Buffer m;
	u_int i;
	int ret;

	debug3("%s", __func__);
	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PAM_QUERY, &m);
	debug3("%s: waiting for MONITOR_ANS_PAM_QUERY", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_PAM_QUERY, &m);
	ret = buffer_get_int(&m);
	debug3("%s: pam_query returned %d", __func__, ret);
	*name = buffer_get_string(&m, NULL);
	*info = buffer_get_string(&m, NULL);
	*num = buffer_get_int(&m);
	if (*num > PAM_MAX_NUM_MSG)
		fatal("%s: recieved %u PAM messages, expected <= %u",
		    __func__, *num, PAM_MAX_NUM_MSG);
	*prompts = xcalloc((*num + 1), sizeof(char *));
	*echo_on = xcalloc((*num + 1), sizeof(u_int));
	for (i = 0; i < *num; ++i) {
		(*prompts)[i] = buffer_get_string(&m, NULL);
		(*echo_on)[i] = buffer_get_int(&m);
	}
	buffer_free(&m);
	return (ret);
}

int
mm_sshpam_respond(void *ctx, u_int num, char **resp)
{
	Buffer m;
	u_int i;
	int ret;

	debug3("%s", __func__);
	buffer_init(&m);
	buffer_put_int(&m, num);
	for (i = 0; i < num; ++i)
		buffer_put_cstring(&m, resp[i]);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PAM_RESPOND, &m);
	debug3("%s: waiting for MONITOR_ANS_PAM_RESPOND", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_PAM_RESPOND, &m);
	ret = buffer_get_int(&m);
	debug3("%s: pam_respond returned %d", __func__, ret);
	buffer_free(&m);
	return (ret);
}

void
mm_sshpam_free_ctx(void *ctxtp)
{
	Buffer m;

	debug3("%s", __func__);
	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PAM_FREE_CTX, &m);
	debug3("%s: waiting for MONITOR_ANS_PAM_FREE_CTX", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_PAM_FREE_CTX, &m);
	buffer_free(&m);
}
#endif /* USE_PAM */

/* Request process termination */

void
mm_terminate(void)
{
	Buffer m;

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_TERM, &m);
	buffer_free(&m);
}

#ifdef WITH_SSH1
int
mm_ssh1_session_key(BIGNUM *num)
{
	int rsafail;
	Buffer m;

	buffer_init(&m);
	buffer_put_bignum2(&m, num);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_SESSKEY, &m);

	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_SESSKEY, &m);

	rsafail = buffer_get_int(&m);
	buffer_get_bignum2(&m, num);

	buffer_free(&m);

	return (rsafail);
}
#endif

static void
mm_chall_setup(char **name, char **infotxt, u_int *numprompts,
    char ***prompts, u_int **echo_on)
{
	*name = xstrdup("");
	*infotxt = xstrdup("");
	*numprompts = 1;
	*prompts = xcalloc(*numprompts, sizeof(char *));
	*echo_on = xcalloc(*numprompts, sizeof(u_int));
	(*echo_on)[0] = 0;
}

int
mm_bsdauth_query(void *ctx, char **name, char **infotxt,
   u_int *numprompts, char ***prompts, u_int **echo_on)
{
	Buffer m;
	u_int success;
	char *challenge;

	debug3("%s: entering", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_BSDAUTHQUERY, &m);

	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_BSDAUTHQUERY,
	    &m);
	success = buffer_get_int(&m);
	if (success == 0) {
		debug3("%s: no challenge", __func__);
		buffer_free(&m);
		return (-1);
	}

	/* Get the challenge, and format the response */
	challenge  = buffer_get_string(&m, NULL);
	buffer_free(&m);

	mm_chall_setup(name, infotxt, numprompts, prompts, echo_on);
	(*prompts)[0] = challenge;

	debug3("%s: received challenge: %s", __func__, challenge);

	return (0);
}

int
mm_bsdauth_respond(void *ctx, u_int numresponses, char **responses)
{
	Buffer m;
	int authok;

	debug3("%s: entering", __func__);
	if (numresponses != 1)
		return (-1);

	buffer_init(&m);
	buffer_put_cstring(&m, responses[0]);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_BSDAUTHRESPOND, &m);

	mm_request_receive_expect(pmonitor->m_recvfd,
	    MONITOR_ANS_BSDAUTHRESPOND, &m);

	authok = buffer_get_int(&m);
	buffer_free(&m);

	return ((authok == 0) ? -1 : 0);
}

#ifdef SKEY
int
mm_skey_query(void *ctx, char **name, char **infotxt,
   u_int *numprompts, char ***prompts, u_int **echo_on)
{
	Buffer m;
	u_int success;
	char *challenge;

	debug3("%s: entering", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_SKEYQUERY, &m);

	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_SKEYQUERY,
	    &m);
	success = buffer_get_int(&m);
	if (success == 0) {
		debug3("%s: no challenge", __func__);
		buffer_free(&m);
		return (-1);
	}

	/* Get the challenge, and format the response */
	challenge  = buffer_get_string(&m, NULL);
	buffer_free(&m);

	debug3("%s: received challenge: %s", __func__, challenge);

	mm_chall_setup(name, infotxt, numprompts, prompts, echo_on);

	xasprintf(*prompts, "%s%s", challenge, SKEY_PROMPT);
	free(challenge);

	return (0);
}

int
mm_skey_respond(void *ctx, u_int numresponses, char **responses)
{
	Buffer m;
	int authok;

	debug3("%s: entering", __func__);
	if (numresponses != 1)
		return (-1);

	buffer_init(&m);
	buffer_put_cstring(&m, responses[0]);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_SKEYRESPOND, &m);

	mm_request_receive_expect(pmonitor->m_recvfd,
	    MONITOR_ANS_SKEYRESPOND, &m);

	authok = buffer_get_int(&m);
	buffer_free(&m);

	return ((authok == 0) ? -1 : 0);
}
#endif /* SKEY */

void
mm_ssh1_session_id(u_char session_id[16])
{
	Buffer m;
	int i;

	debug3("%s entering", __func__);

	buffer_init(&m);
	for (i = 0; i < 16; i++)
		buffer_put_char(&m, session_id[i]);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_SESSID, &m);
	buffer_free(&m);
}

#ifdef WITH_SSH1
int
mm_auth_rsa_key_allowed(struct passwd *pw, BIGNUM *client_n, Key **rkey)
{
	Buffer m;
	Key *key;
	u_char *blob;
	u_int blen;
	int allowed = 0, have_forced = 0;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_bignum2(&m, client_n);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_RSAKEYALLOWED, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_RSAKEYALLOWED, &m);

	allowed = buffer_get_int(&m);

	/* fake forced command */
	auth_clear_options();
	have_forced = buffer_get_int(&m);
	forced_command = have_forced ? xstrdup("true") : NULL;

	if (allowed && rkey != NULL) {
		blob = buffer_get_string(&m, &blen);
		if ((key = key_from_blob(blob, blen)) == NULL)
			fatal("%s: key_from_blob failed", __func__);
		*rkey = key;
		free(blob);
	}
	buffer_free(&m);

	return (allowed);
}

BIGNUM *
mm_auth_rsa_generate_challenge(Key *key)
{
	Buffer m;
	BIGNUM *challenge;
	u_char *blob;
	u_int blen;

	debug3("%s entering", __func__);

	if ((challenge = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);

	key->type = KEY_RSA;    /* XXX cheat for key_to_blob */
	if (key_to_blob(key, &blob, &blen) == 0)
		fatal("%s: key_to_blob failed", __func__);
	key->type = KEY_RSA1;

	buffer_init(&m);
	buffer_put_string(&m, blob, blen);
	free(blob);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_RSACHALLENGE, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_RSACHALLENGE, &m);

	buffer_get_bignum2(&m, challenge);
	buffer_free(&m);

	return (challenge);
}

int
mm_auth_rsa_verify_response(Key *key, BIGNUM *p, u_char response[16])
{
	Buffer m;
	u_char *blob;
	u_int blen;
	int success = 0;

	debug3("%s entering", __func__);

	key->type = KEY_RSA;    /* XXX cheat for key_to_blob */
	if (key_to_blob(key, &blob, &blen) == 0)
		fatal("%s: key_to_blob failed", __func__);
	key->type = KEY_RSA1;

	buffer_init(&m);
	buffer_put_string(&m, blob, blen);
	buffer_put_string(&m, response, 16);
	free(blob);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_RSARESPONSE, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_RSARESPONSE, &m);

	success = buffer_get_int(&m);
	buffer_free(&m);

	return (success);
}
#endif

#ifdef SSH_AUDIT_EVENTS
void
mm_audit_event(ssh_audit_event_t event)
{
	Buffer m;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_int(&m, event);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_AUDIT_EVENT, &m);
	buffer_free(&m);
}

void
mm_audit_run_command(const char *command)
{
	Buffer m;

	debug3("%s entering command %s", __func__, command);

	buffer_init(&m);
	buffer_put_cstring(&m, command);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_AUDIT_COMMAND, &m);
	buffer_free(&m);
}
#endif /* SSH_AUDIT_EVENTS */

#ifdef GSSAPI
OM_uint32
mm_ssh_gssapi_server_ctx(Gssctxt **ctx, gss_OID goid)
{
	Buffer m;
	OM_uint32 major;

	/* Client doesn't get to see the context */
	*ctx = NULL;

	buffer_init(&m);
	buffer_put_string(&m, goid->elements, goid->length);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_GSSSETUP, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_GSSSETUP, &m);

	major = buffer_get_int(&m);

	buffer_free(&m);
	return (major);
}

OM_uint32
mm_ssh_gssapi_accept_ctx(Gssctxt *ctx, gss_buffer_desc *in,
    gss_buffer_desc *out, OM_uint32 *flags)
{
	Buffer m;
	OM_uint32 major;
	u_int len;

	buffer_init(&m);
	buffer_put_string(&m, in->value, in->length);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_GSSSTEP, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_GSSSTEP, &m);

	major = buffer_get_int(&m);
	out->value = buffer_get_string(&m, &len);
	out->length = len;
	if (flags)
		*flags = buffer_get_int(&m);

	buffer_free(&m);

	return (major);
}

OM_uint32
mm_ssh_gssapi_checkmic(Gssctxt *ctx, gss_buffer_t gssbuf, gss_buffer_t gssmic)
{
	Buffer m;
	OM_uint32 major;

	buffer_init(&m);
	buffer_put_string(&m, gssbuf->value, gssbuf->length);
	buffer_put_string(&m, gssmic->value, gssmic->length);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_GSSCHECKMIC, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_GSSCHECKMIC,
	    &m);

	major = buffer_get_int(&m);
	buffer_free(&m);
	return(major);
}

int
mm_ssh_gssapi_userok(char *user)
{
	Buffer m;
	int authenticated = 0;

	buffer_init(&m);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_GSSUSEROK, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_GSSUSEROK,
				  &m);

	authenticated = buffer_get_int(&m);

	buffer_free(&m);
	debug3("%s: user %sauthenticated",__func__, authenticated ? "" : "not ");
	return (authenticated);
}
#endif /* GSSAPI */

