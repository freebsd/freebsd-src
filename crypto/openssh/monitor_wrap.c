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
RCSID("$OpenBSD: monitor_wrap.c,v 1.19 2002/09/26 11:38:43 markus Exp $");
RCSID("$FreeBSD$");

#include <openssl/bn.h>
#include <openssl/dh.h>

#include "ssh.h"
#include "dh.h"
#include "kex.h"
#include "auth.h"
#include "buffer.h"
#include "bufaux.h"
#include "packet.h"
#include "mac.h"
#include "log.h"
#include "zlib.h"
#include "monitor.h"
#include "monitor_wrap.h"
#include "xmalloc.h"
#include "atomicio.h"
#include "monitor_fdpass.h"
#include "getput.h"

#include "auth.h"
#include "channels.h"
#include "session.h"

/* Imports */
extern int compat20;
extern Newkeys *newkeys[];
extern z_stream incoming_stream;
extern z_stream outgoing_stream;
extern struct monitor *pmonitor;
extern Buffer input, output;

void
mm_request_send(int socket, enum monitor_reqtype type, Buffer *m)
{
	u_int mlen = buffer_len(m);
	u_char buf[5];

	debug3("%s entering: type %d", __func__, type);

	PUT_32BIT(buf, mlen + 1);
	buf[4] = (u_char) type;		/* 1st byte of payload is mesg-type */
	if (atomicio(write, socket, buf, sizeof(buf)) != sizeof(buf))
		fatal("%s: write", __func__);
	if (atomicio(write, socket, buffer_ptr(m), mlen) != mlen)
		fatal("%s: write", __func__);
}

void
mm_request_receive(int socket, Buffer *m)
{
	u_char buf[4];
	u_int msg_len;
	ssize_t res;

	debug3("%s entering", __func__);

	res = atomicio(read, socket, buf, sizeof(buf));
	if (res != sizeof(buf)) {
		if (res == 0)
			fatal_cleanup();
		fatal("%s: read: %ld", __func__, (long)res);
	}
	msg_len = GET_32BIT(buf);
	if (msg_len > 256 * 1024)
		fatal("%s: read: bad msg_len %d", __func__, msg_len);
	buffer_clear(m);
	buffer_append_space(m, msg_len);
	res = atomicio(read, socket, buffer_ptr(m), msg_len);
	if (res != msg_len)
		fatal("%s: read: %ld != msg_len", __func__, (long)res);
}

void
mm_request_receive_expect(int socket, enum monitor_reqtype type, Buffer *m)
{
	u_char rtype;

	debug3("%s entering: type %d", __func__, type);

	mm_request_receive(socket, m);
	rtype = buffer_get_char(m);
	if (rtype != type)
		fatal("%s: read: rtype %d != type %d", __func__,
		    rtype, type);
}

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

int
mm_key_sign(Key *key, u_char **sigp, u_int *lenp, u_char *data, u_int datalen)
{
	Kex *kex = *pmonitor->m_pkex;
	Buffer m;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_int(&m, kex->host_key_index(key));
	buffer_put_string(&m, data, datalen);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_SIGN, &m);

	debug3("%s: waiting for MONITOR_ANS_SIGN", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_SIGN, &m);
	*sigp  = buffer_get_string(&m, lenp);
	buffer_free(&m);

	return (0);
}

struct passwd *
mm_getpwnamallow(const char *login)
{
	Buffer m;
	struct passwd *pw;
	u_int pwlen;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_cstring(&m, login);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PWNAM, &m);

	debug3("%s: waiting for MONITOR_ANS_PWNAM", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_PWNAM, &m);

	if (buffer_get_char(&m) == 0) {
		buffer_free(&m);
		return (NULL);
	}
	pw = buffer_get_string(&m, &pwlen);
	if (pwlen != sizeof(struct passwd))
		fatal("%s: struct passwd size mismatch", __func__);
	pw->pw_name = buffer_get_string(&m, NULL);
	pw->pw_passwd = buffer_get_string(&m, NULL);
	pw->pw_gecos = buffer_get_string(&m, NULL);
#ifdef HAVE_PW_CLASS_IN_PASSWD
	pw->pw_class = buffer_get_string(&m, NULL);
#endif
	pw->pw_dir = buffer_get_string(&m, NULL);
	pw->pw_shell = buffer_get_string(&m, NULL);
	buffer_free(&m);

	return (pw);
}

char *mm_auth2_read_banner(void)
{
	Buffer m;
	char *banner;

	debug3("%s entering", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_AUTH2_READ_BANNER, &m);
	buffer_clear(&m);

	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_AUTH2_READ_BANNER, &m);
	banner = buffer_get_string(&m, NULL);
	buffer_free(&m);

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
mm_user_key_allowed(struct passwd *pw, Key *key)
{
	return (mm_key_allowed(MM_USERKEY, NULL, NULL, key));
}

int
mm_hostbased_key_allowed(struct passwd *pw, char *user, char *host,
    Key *key)
{
	return (mm_key_allowed(MM_HOSTKEY, user, host, key));
}

int
mm_auth_rhosts_rsa_key_allowed(struct passwd *pw, char *user,
    char *host, Key *key)
{
	int ret;

	key->type = KEY_RSA; /* XXX hack for key_to_blob */
	ret = mm_key_allowed(MM_RSAHOSTKEY, user, host, key);
	key->type = KEY_RSA1;
	return (ret);
}

static void
mm_send_debug(Buffer *m)
{
	char *msg;

	while (buffer_len(m)) {
		msg = buffer_get_string(m, NULL);
		debug3("%s: Sending debug: %s", __func__, msg);
		packet_send_debug("%s", msg);
		xfree(msg);
	}
}

int
mm_key_allowed(enum mm_keytype type, char *user, char *host, Key *key)
{
	Buffer m;
	u_char *blob;
	u_int len;
	int allowed = 0;

	debug3("%s entering", __func__);

	/* Convert the key to a blob and the pass it over */
	if (!key_to_blob(key, &blob, &len))
		return (0);

	buffer_init(&m);
	buffer_put_int(&m, type);
	buffer_put_cstring(&m, user ? user : "");
	buffer_put_cstring(&m, host ? host : "");
	buffer_put_string(&m, blob, len);
	xfree(blob);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_KEYALLOWED, &m);

	debug3("%s: waiting for MONITOR_ANS_KEYALLOWED", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_KEYALLOWED, &m);

	allowed = buffer_get_int(&m);

	/* Send potential debug messages */
	mm_send_debug(&m);

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
	xfree(blob);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_KEYVERIFY, &m);

	debug3("%s: waiting for MONITOR_ANS_KEYVERIFY", __func__);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_KEYVERIFY, &m);

	verified = buffer_get_int(&m);

	buffer_free(&m);

	return (verified);
}

/* Export key state after authentication */
Newkeys *
mm_newkeys_from_blob(u_char *blob, int blen)
{
	Buffer b;
	u_int len;
	Newkeys *newkey = NULL;
	Enc *enc;
	Mac *mac;
	Comp *comp;

	debug3("%s: %p(%d)", __func__, blob, blen);
#ifdef DEBUG_PK
	dump_base64(stderr, blob, blen);
#endif
	buffer_init(&b);
	buffer_append(&b, blob, blen);

	newkey = xmalloc(sizeof(*newkey));
	enc = &newkey->enc;
	mac = &newkey->mac;
	comp = &newkey->comp;

	/* Enc structure */
	enc->name = buffer_get_string(&b, NULL);
	buffer_get(&b, &enc->cipher, sizeof(enc->cipher));
	enc->enabled = buffer_get_int(&b);
	enc->block_size = buffer_get_int(&b);
	enc->key = buffer_get_string(&b, &enc->key_len);
	enc->iv = buffer_get_string(&b, &len);
	if (len != enc->block_size)
		fatal("%s: bad ivlen: expected %u != %u", __func__,
		    enc->block_size, len);

	if (enc->name == NULL || cipher_by_name(enc->name) != enc->cipher)
		fatal("%s: bad cipher name %s or pointer %p", __func__,
		    enc->name, enc->cipher);

	/* Mac structure */
	mac->name = buffer_get_string(&b, NULL);
	if (mac->name == NULL || mac_init(mac, mac->name) == -1)
		fatal("%s: can not init mac %s", __func__, mac->name);
	mac->enabled = buffer_get_int(&b);
	mac->key = buffer_get_string(&b, &len);
	if (len > mac->key_len)
		fatal("%s: bad mac key length: %u > %d", __func__, len,
		    mac->key_len);
	mac->key_len = len;

	/* Comp structure */
	comp->type = buffer_get_int(&b);
	comp->enabled = buffer_get_int(&b);
	comp->name = buffer_get_string(&b, NULL);

	len = buffer_len(&b);
	if (len != 0)
		error("newkeys_from_blob: remaining bytes in blob %u", len);
	buffer_free(&b);
	return (newkey);
}

int
mm_newkeys_to_blob(int mode, u_char **blobp, u_int *lenp)
{
	Buffer b;
	int len;
	Enc *enc;
	Mac *mac;
	Comp *comp;
	Newkeys *newkey = newkeys[mode];

	debug3("%s: converting %p", __func__, newkey);

	if (newkey == NULL) {
		error("%s: newkey == NULL", __func__);
		return 0;
	}
	enc = &newkey->enc;
	mac = &newkey->mac;
	comp = &newkey->comp;

	buffer_init(&b);
	/* Enc structure */
	buffer_put_cstring(&b, enc->name);
	/* The cipher struct is constant and shared, you export pointer */
	buffer_append(&b, &enc->cipher, sizeof(enc->cipher));
	buffer_put_int(&b, enc->enabled);
	buffer_put_int(&b, enc->block_size);
	buffer_put_string(&b, enc->key, enc->key_len);
	packet_get_keyiv(mode, enc->iv, enc->block_size);
	buffer_put_string(&b, enc->iv, enc->block_size);

	/* Mac structure */
	buffer_put_cstring(&b, mac->name);
	buffer_put_int(&b, mac->enabled);
	buffer_put_string(&b, mac->key, mac->key_len);

	/* Comp structure */
	buffer_put_int(&b, comp->type);
	buffer_put_int(&b, comp->enabled);
	buffer_put_cstring(&b, comp->name);

	len = buffer_len(&b);
	if (lenp != NULL)
		*lenp = len;
	if (blobp != NULL) {
		*blobp = xmalloc(len);
		memcpy(*blobp, buffer_ptr(&b), len);
	}
	memset(buffer_ptr(&b), 0, len);
	buffer_free(&b);
	return len;
}

static void
mm_send_kex(Buffer *m, Kex *kex)
{
	buffer_put_string(m, kex->session_id, kex->session_id_len);
	buffer_put_int(m, kex->we_need);
	buffer_put_int(m, kex->hostkey_type);
	buffer_put_int(m, kex->kex_type);
	buffer_put_string(m, buffer_ptr(&kex->my), buffer_len(&kex->my));
	buffer_put_string(m, buffer_ptr(&kex->peer), buffer_len(&kex->peer));
	buffer_put_int(m, kex->flags);
	buffer_put_cstring(m, kex->client_version_string);
	buffer_put_cstring(m, kex->server_version_string);
}

void
mm_send_keystate(struct monitor *pmonitor)
{
	Buffer m;
	u_char *blob, *p;
	u_int bloblen, plen;

	buffer_init(&m);

	if (!compat20) {
		u_char iv[24];
		u_char *key;
		u_int ivlen, keylen;

		buffer_put_int(&m, packet_get_protocol_flags());

		buffer_put_int(&m, packet_get_ssh1_cipher());

		debug3("%s: Sending ssh1 KEY+IV", __func__);
		keylen = packet_get_encryption_key(NULL);
		key = xmalloc(keylen+1);	/* add 1 if keylen == 0 */
		keylen = packet_get_encryption_key(key);
		buffer_put_string(&m, key, keylen);
		memset(key, 0, keylen);
		xfree(key);

		ivlen = packet_get_keyiv_len(MODE_OUT);
		packet_get_keyiv(MODE_OUT, iv, ivlen);
		buffer_put_string(&m, iv, ivlen);
		ivlen = packet_get_keyiv_len(MODE_OUT);
		packet_get_keyiv(MODE_IN, iv, ivlen);
		buffer_put_string(&m, iv, ivlen);
		goto skip;
	} else {
		/* Kex for rekeying */
		mm_send_kex(&m, *pmonitor->m_pkex);
	}

	debug3("%s: Sending new keys: %p %p",
	    __func__, newkeys[MODE_OUT], newkeys[MODE_IN]);

	/* Keys from Kex */
	if (!mm_newkeys_to_blob(MODE_OUT, &blob, &bloblen))
		fatal("%s: conversion of newkeys failed", __func__);

	buffer_put_string(&m, blob, bloblen);
	xfree(blob);

	if (!mm_newkeys_to_blob(MODE_IN, &blob, &bloblen))
		fatal("%s: conversion of newkeys failed", __func__);

	buffer_put_string(&m, blob, bloblen);
	xfree(blob);

	buffer_put_int(&m, packet_get_seqnr(MODE_OUT));
	buffer_put_int(&m, packet_get_seqnr(MODE_IN));

	debug3("%s: New keys have been sent", __func__);
 skip:
	/* More key context */
	plen = packet_get_keycontext(MODE_OUT, NULL);
	p = xmalloc(plen+1);
	packet_get_keycontext(MODE_OUT, p);
	buffer_put_string(&m, p, plen);
	xfree(p);

	plen = packet_get_keycontext(MODE_IN, NULL);
	p = xmalloc(plen+1);
	packet_get_keycontext(MODE_IN, p);
	buffer_put_string(&m, p, plen);
	xfree(p);

	/* Compression state */
	debug3("%s: Sending compression state", __func__);
	buffer_put_string(&m, &outgoing_stream, sizeof(outgoing_stream));
	buffer_put_string(&m, &incoming_stream, sizeof(incoming_stream));

	/* Network I/O buffers */
	buffer_put_string(&m, buffer_ptr(&input), buffer_len(&input));
	buffer_put_string(&m, buffer_ptr(&output), buffer_len(&output));

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_KEYEXPORT, &m);
	debug3("%s: Finished sending state", __func__);

	buffer_free(&m);
}

int
mm_pty_allocate(int *ptyfd, int *ttyfd, char *namebuf, int namebuflen)
{
	Buffer m;
	char *p;
	int success = 0;

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
	buffer_free(&m);

	strlcpy(namebuf, p, namebuflen); /* Possible truncation */
	xfree(p);

	*ptyfd = mm_receive_fd(pmonitor->m_recvfd);
	*ttyfd = mm_receive_fd(pmonitor->m_recvfd);

	/* Success */
	return (1);
}

void
mm_session_pty_cleanup2(void *session)
{
	Session *s = session;
	Buffer m;

	if (s->ttyfd == -1)
		return;
	buffer_init(&m);
	buffer_put_cstring(&m, s->tty);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PTYCLEANUP, &m);
	buffer_free(&m);

	/* closed dup'ed master */
	if (close(s->ptymaster) < 0)
		error("close(s->ptymaster): %s", strerror(errno));

	/* unlink pty from session */
	s->ttyfd = -1;
}

#ifdef USE_PAM
void
mm_start_pam(char *user)
{
	Buffer m;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_cstring(&m, user);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_PAM_START, &m);

	buffer_free(&m);
}

void *
mm_pam_init_ctx(Authctxt *authctxt)
{
	Buffer m;
	int success;

	debug3("%s", __func__);
	buffer_init(&m);
	buffer_put_cstring(&m, authctxt->user);
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
mm_pam_query(void *ctx, char **name, char **info,
    u_int *num, char ***prompts, u_int **echo_on)
{
	Buffer m;
	int i, ret;

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
	*prompts = xmalloc((*num + 1) * sizeof(char *));
	*echo_on = xmalloc((*num + 1) * sizeof(u_int));
	for (i = 0; i < *num; ++i) {
		(*prompts)[i] = buffer_get_string(&m, NULL);
		(*echo_on)[i] = buffer_get_int(&m);
	}
	buffer_free(&m);
	return (ret);
}

int
mm_pam_respond(void *ctx, u_int num, char **resp)
{
	Buffer m;
	int i, ret;

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
mm_pam_free_ctx(void *ctxtp)
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

static void
mm_chall_setup(char **name, char **infotxt, u_int *numprompts,
    char ***prompts, u_int **echo_on)
{
	*name = xstrdup("");
	*infotxt = xstrdup("");
	*numprompts = 1;
	*prompts = xmalloc(*numprompts * sizeof(char *));
	*echo_on = xmalloc(*numprompts * sizeof(u_int));
	(*echo_on)[0] = 0;
}

int
mm_bsdauth_query(void *ctx, char **name, char **infotxt,
   u_int *numprompts, char ***prompts, u_int **echo_on)
{
	Buffer m;
	int res;
	char *challenge;

	debug3("%s: entering", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_BSDAUTHQUERY, &m);

	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_BSDAUTHQUERY,
	    &m);
	res = buffer_get_int(&m);
	if (res == -1) {
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
	int len, res;
	char *p, *challenge;

	debug3("%s: entering", __func__);

	buffer_init(&m);
	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_SKEYQUERY, &m);

	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_SKEYQUERY,
	    &m);
	res = buffer_get_int(&m);
	if (res == -1) {
		debug3("%s: no challenge", __func__);
		buffer_free(&m);
		return (-1);
	}

	/* Get the challenge, and format the response */
	challenge  = buffer_get_string(&m, NULL);
	buffer_free(&m);

	debug3("%s: received challenge: %s", __func__, challenge);

	mm_chall_setup(name, infotxt, numprompts, prompts, echo_on);

	len = strlen(challenge) + strlen(SKEY_PROMPT) + 1;
	p = xmalloc(len);
	strlcpy(p, challenge, len);
	strlcat(p, SKEY_PROMPT, len);
	(*prompts)[0] = p;
	xfree(challenge);

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
#endif

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

int
mm_auth_rsa_key_allowed(struct passwd *pw, BIGNUM *client_n, Key **rkey)
{
	Buffer m;
	Key *key;
	u_char *blob;
	u_int blen;
	int allowed = 0;

	debug3("%s entering", __func__);

	buffer_init(&m);
	buffer_put_bignum2(&m, client_n);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_RSAKEYALLOWED, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_RSAKEYALLOWED, &m);

	allowed = buffer_get_int(&m);

	if (allowed && rkey != NULL) {
		blob = buffer_get_string(&m, &blen);
		if ((key = key_from_blob(blob, blen)) == NULL)
			fatal("%s: key_from_blob failed", __func__);
		*rkey = key;
		xfree(blob);
	}
	mm_send_debug(&m);
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
	xfree(blob);

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
	xfree(blob);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_RSARESPONSE, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_RSARESPONSE, &m);

	success = buffer_get_int(&m);
	buffer_free(&m);

	return (success);
}

#ifdef KRB4
int
mm_auth_krb4(Authctxt *authctxt, void *_auth, char **client, void *_reply)
{
	KTEXT auth, reply;
 	Buffer m;
	u_int rlen;
	int success = 0;
	char *p;

	debug3("%s entering", __func__);
	auth = _auth;
	reply = _reply;

	buffer_init(&m);
	buffer_put_string(&m, auth->dat, auth->length);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_KRB4, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_KRB4, &m);

	success = buffer_get_int(&m);
	if (success) {
		*client = buffer_get_string(&m, NULL);
		p = buffer_get_string(&m, &rlen);
		if (rlen >= MAX_KTXT_LEN)
			fatal("%s: reply from monitor too large", __func__);
		reply->length = rlen;
		memcpy(reply->dat, p, rlen);
		memset(p, 0, rlen);
		xfree(p);
	}
	buffer_free(&m);
	return (success); 
}
#endif

#ifdef KRB5
int
mm_auth_krb5(void *ctx, void *argp, char **userp, void *resp)
{
	krb5_data *tkt, *reply;
	Buffer m;
	int success;

	debug3("%s entering", __func__);
	tkt = (krb5_data *) argp;
	reply = (krb5_data *) resp;

	buffer_init(&m);
	buffer_put_string(&m, tkt->data, tkt->length);

	mm_request_send(pmonitor->m_recvfd, MONITOR_REQ_KRB5, &m);
	mm_request_receive_expect(pmonitor->m_recvfd, MONITOR_ANS_KRB5, &m);

	success = buffer_get_int(&m);
	if (success) {
		u_int len;

		*userp = buffer_get_string(&m, NULL);
		reply->data = buffer_get_string(&m, &len);
		reply->length = len;
	} else {
		memset(reply, 0, sizeof(*reply));
		*userp = NULL;
	}

	buffer_free(&m);
	return (success);
}
#endif
