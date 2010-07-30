/* $OpenBSD: authfd.c,v 1.82 2010/02/26 20:29:54 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for connecting the local authentication agent.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 implementation,
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
#include <sys/un.h>
#include <sys/socket.h>

#include <openssl/evp.h>

#include <openssl/crypto.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "ssh.h"
#include "rsa.h"
#include "buffer.h"
#include "key.h"
#include "authfd.h"
#include "cipher.h"
#include "kex.h"
#include "compat.h"
#include "log.h"
#include "atomicio.h"
#include "misc.h"

static int agent_present = 0;

/* helper */
int	decode_reply(int type);

/* macro to check for "agent failure" message */
#define agent_failed(x) \
    ((x == SSH_AGENT_FAILURE) || (x == SSH_COM_AGENT2_FAILURE) || \
    (x == SSH2_AGENT_FAILURE))

int
ssh_agent_present(void)
{
	int authfd;

	if (agent_present)
		return 1;
	if ((authfd = ssh_get_authentication_socket()) == -1)
		return 0;
	else {
		ssh_close_authentication_socket(authfd);
		return 1;
	}
}

/* Returns the number of the authentication fd, or -1 if there is none. */

int
ssh_get_authentication_socket(void)
{
	const char *authsocket;
	int sock;
	struct sockaddr_un sunaddr;

	authsocket = getenv(SSH_AUTHSOCKET_ENV_NAME);
	if (!authsocket)
		return -1;

	sunaddr.sun_family = AF_UNIX;
	strlcpy(sunaddr.sun_path, authsocket, sizeof(sunaddr.sun_path));

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	/* close on exec */
	if (fcntl(sock, F_SETFD, 1) == -1) {
		close(sock);
		return -1;
	}
	if (connect(sock, (struct sockaddr *)&sunaddr, sizeof sunaddr) < 0) {
		close(sock);
		return -1;
	}
	agent_present = 1;
	return sock;
}

static int
ssh_request_reply(AuthenticationConnection *auth, Buffer *request, Buffer *reply)
{
	u_int l, len;
	char buf[1024];

	/* Get the length of the message, and format it in the buffer. */
	len = buffer_len(request);
	put_u32(buf, len);

	/* Send the length and then the packet to the agent. */
	if (atomicio(vwrite, auth->fd, buf, 4) != 4 ||
	    atomicio(vwrite, auth->fd, buffer_ptr(request),
	    buffer_len(request)) != buffer_len(request)) {
		error("Error writing to authentication socket.");
		return 0;
	}
	/*
	 * Wait for response from the agent.  First read the length of the
	 * response packet.
	 */
	if (atomicio(read, auth->fd, buf, 4) != 4) {
	    error("Error reading response length from authentication socket.");
	    return 0;
	}

	/* Extract the length, and check it for sanity. */
	len = get_u32(buf);
	if (len > 256 * 1024)
		fatal("Authentication response too long: %u", len);

	/* Read the rest of the response in to the buffer. */
	buffer_clear(reply);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		if (atomicio(read, auth->fd, buf, l) != l) {
			error("Error reading response from authentication socket.");
			return 0;
		}
		buffer_append(reply, buf, l);
		len -= l;
	}
	return 1;
}

/*
 * Closes the agent socket if it should be closed (depends on how it was
 * obtained).  The argument must have been returned by
 * ssh_get_authentication_socket().
 */

void
ssh_close_authentication_socket(int sock)
{
	if (getenv(SSH_AUTHSOCKET_ENV_NAME))
		close(sock);
}

/*
 * Opens and connects a private socket for communication with the
 * authentication agent.  Returns the file descriptor (which must be
 * shut down and closed by the caller when no longer needed).
 * Returns NULL if an error occurred and the connection could not be
 * opened.
 */

AuthenticationConnection *
ssh_get_authentication_connection(void)
{
	AuthenticationConnection *auth;
	int sock;

	sock = ssh_get_authentication_socket();

	/*
	 * Fail if we couldn't obtain a connection.  This happens if we
	 * exited due to a timeout.
	 */
	if (sock < 0)
		return NULL;

	auth = xmalloc(sizeof(*auth));
	auth->fd = sock;
	buffer_init(&auth->identities);
	auth->howmany = 0;

	return auth;
}

/*
 * Closes the connection to the authentication agent and frees any associated
 * memory.
 */

void
ssh_close_authentication_connection(AuthenticationConnection *auth)
{
	buffer_free(&auth->identities);
	close(auth->fd);
	xfree(auth);
}

/* Lock/unlock agent */
int
ssh_lock_agent(AuthenticationConnection *auth, int lock, const char *password)
{
	int type;
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, lock ? SSH_AGENTC_LOCK : SSH_AGENTC_UNLOCK);
	buffer_put_cstring(&msg, password);

	if (ssh_request_reply(auth, &msg, &msg) == 0) {
		buffer_free(&msg);
		return 0;
	}
	type = buffer_get_char(&msg);
	buffer_free(&msg);
	return decode_reply(type);
}

/*
 * Returns the first authentication identity held by the agent.
 */

int
ssh_get_num_identities(AuthenticationConnection *auth, int version)
{
	int type, code1 = 0, code2 = 0;
	Buffer request;

	switch (version) {
	case 1:
		code1 = SSH_AGENTC_REQUEST_RSA_IDENTITIES;
		code2 = SSH_AGENT_RSA_IDENTITIES_ANSWER;
		break;
	case 2:
		code1 = SSH2_AGENTC_REQUEST_IDENTITIES;
		code2 = SSH2_AGENT_IDENTITIES_ANSWER;
		break;
	default:
		return 0;
	}

	/*
	 * Send a message to the agent requesting for a list of the
	 * identities it can represent.
	 */
	buffer_init(&request);
	buffer_put_char(&request, code1);

	buffer_clear(&auth->identities);
	if (ssh_request_reply(auth, &request, &auth->identities) == 0) {
		buffer_free(&request);
		return 0;
	}
	buffer_free(&request);

	/* Get message type, and verify that we got a proper answer. */
	type = buffer_get_char(&auth->identities);
	if (agent_failed(type)) {
		return 0;
	} else if (type != code2) {
		fatal("Bad authentication reply message type: %d", type);
	}

	/* Get the number of entries in the response and check it for sanity. */
	auth->howmany = buffer_get_int(&auth->identities);
	if ((u_int)auth->howmany > 1024)
		fatal("Too many identities in authentication reply: %d",
		    auth->howmany);

	return auth->howmany;
}

Key *
ssh_get_first_identity(AuthenticationConnection *auth, char **comment, int version)
{
	/* get number of identities and return the first entry (if any). */
	if (ssh_get_num_identities(auth, version) > 0)
		return ssh_get_next_identity(auth, comment, version);
	return NULL;
}

Key *
ssh_get_next_identity(AuthenticationConnection *auth, char **comment, int version)
{
	int keybits;
	u_int bits;
	u_char *blob;
	u_int blen;
	Key *key = NULL;

	/* Return failure if no more entries. */
	if (auth->howmany <= 0)
		return NULL;

	/*
	 * Get the next entry from the packet.  These will abort with a fatal
	 * error if the packet is too short or contains corrupt data.
	 */
	switch (version) {
	case 1:
		key = key_new(KEY_RSA1);
		bits = buffer_get_int(&auth->identities);
		buffer_get_bignum(&auth->identities, key->rsa->e);
		buffer_get_bignum(&auth->identities, key->rsa->n);
		*comment = buffer_get_string(&auth->identities, NULL);
		keybits = BN_num_bits(key->rsa->n);
		if (keybits < 0 || bits != (u_int)keybits)
			logit("Warning: identity keysize mismatch: actual %d, announced %u",
			    BN_num_bits(key->rsa->n), bits);
		break;
	case 2:
		blob = buffer_get_string(&auth->identities, &blen);
		*comment = buffer_get_string(&auth->identities, NULL);
		key = key_from_blob(blob, blen);
		xfree(blob);
		break;
	default:
		return NULL;
	}
	/* Decrement the number of remaining entries. */
	auth->howmany--;
	return key;
}

/*
 * Generates a random challenge, sends it to the agent, and waits for
 * response from the agent.  Returns true (non-zero) if the agent gave the
 * correct answer, zero otherwise.  Response type selects the style of
 * response desired, with 0 corresponding to protocol version 1.0 (no longer
 * supported) and 1 corresponding to protocol version 1.1.
 */

int
ssh_decrypt_challenge(AuthenticationConnection *auth,
    Key* key, BIGNUM *challenge,
    u_char session_id[16],
    u_int response_type,
    u_char response[16])
{
	Buffer buffer;
	int success = 0;
	int i;
	int type;

	if (key->type != KEY_RSA1)
		return 0;
	if (response_type == 0) {
		logit("Compatibility with ssh protocol version 1.0 no longer supported.");
		return 0;
	}
	buffer_init(&buffer);
	buffer_put_char(&buffer, SSH_AGENTC_RSA_CHALLENGE);
	buffer_put_int(&buffer, BN_num_bits(key->rsa->n));
	buffer_put_bignum(&buffer, key->rsa->e);
	buffer_put_bignum(&buffer, key->rsa->n);
	buffer_put_bignum(&buffer, challenge);
	buffer_append(&buffer, session_id, 16);
	buffer_put_int(&buffer, response_type);

	if (ssh_request_reply(auth, &buffer, &buffer) == 0) {
		buffer_free(&buffer);
		return 0;
	}
	type = buffer_get_char(&buffer);

	if (agent_failed(type)) {
		logit("Agent admitted failure to authenticate using the key.");
	} else if (type != SSH_AGENT_RSA_RESPONSE) {
		fatal("Bad authentication response: %d", type);
	} else {
		success = 1;
		/*
		 * Get the response from the packet.  This will abort with a
		 * fatal error if the packet is corrupt.
		 */
		for (i = 0; i < 16; i++)
			response[i] = (u_char)buffer_get_char(&buffer);
	}
	buffer_free(&buffer);
	return success;
}

/* ask agent to sign data, returns -1 on error, 0 on success */
int
ssh_agent_sign(AuthenticationConnection *auth,
    Key *key,
    u_char **sigp, u_int *lenp,
    u_char *data, u_int datalen)
{
	extern int datafellows;
	Buffer msg;
	u_char *blob;
	u_int blen;
	int type, flags = 0;
	int ret = -1;

	if (key_to_blob(key, &blob, &blen) == 0)
		return -1;

	if (datafellows & SSH_BUG_SIGBLOB)
		flags = SSH_AGENT_OLD_SIGNATURE;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_AGENTC_SIGN_REQUEST);
	buffer_put_string(&msg, blob, blen);
	buffer_put_string(&msg, data, datalen);
	buffer_put_int(&msg, flags);
	xfree(blob);

	if (ssh_request_reply(auth, &msg, &msg) == 0) {
		buffer_free(&msg);
		return -1;
	}
	type = buffer_get_char(&msg);
	if (agent_failed(type)) {
		logit("Agent admitted failure to sign using the key.");
	} else if (type != SSH2_AGENT_SIGN_RESPONSE) {
		fatal("Bad authentication response: %d", type);
	} else {
		ret = 0;
		*sigp = buffer_get_string(&msg, lenp);
	}
	buffer_free(&msg);
	return ret;
}

/* Encode key for a message to the agent. */

static void
ssh_encode_identity_rsa1(Buffer *b, RSA *key, const char *comment)
{
	buffer_put_int(b, BN_num_bits(key->n));
	buffer_put_bignum(b, key->n);
	buffer_put_bignum(b, key->e);
	buffer_put_bignum(b, key->d);
	/* To keep within the protocol: p < q for ssh. in SSL p > q */
	buffer_put_bignum(b, key->iqmp);	/* ssh key->u */
	buffer_put_bignum(b, key->q);	/* ssh key->p, SSL key->q */
	buffer_put_bignum(b, key->p);	/* ssh key->q, SSL key->p */
	buffer_put_cstring(b, comment);
}

static void
ssh_encode_identity_ssh2(Buffer *b, Key *key, const char *comment)
{
	buffer_put_cstring(b, key_ssh_name(key));
	switch (key->type) {
	case KEY_RSA:
		buffer_put_bignum2(b, key->rsa->n);
		buffer_put_bignum2(b, key->rsa->e);
		buffer_put_bignum2(b, key->rsa->d);
		buffer_put_bignum2(b, key->rsa->iqmp);
		buffer_put_bignum2(b, key->rsa->p);
		buffer_put_bignum2(b, key->rsa->q);
		break;
	case KEY_RSA_CERT:
		if (key->cert == NULL || buffer_len(&key->cert->certblob) == 0)
			fatal("%s: no cert/certblob", __func__);
		buffer_put_string(b, buffer_ptr(&key->cert->certblob),
		    buffer_len(&key->cert->certblob));
		buffer_put_bignum2(b, key->rsa->d);
		buffer_put_bignum2(b, key->rsa->iqmp);
		buffer_put_bignum2(b, key->rsa->p);
		buffer_put_bignum2(b, key->rsa->q);
		break;
	case KEY_DSA:
		buffer_put_bignum2(b, key->dsa->p);
		buffer_put_bignum2(b, key->dsa->q);
		buffer_put_bignum2(b, key->dsa->g);
		buffer_put_bignum2(b, key->dsa->pub_key);
		buffer_put_bignum2(b, key->dsa->priv_key);
		break;
	case KEY_DSA_CERT:
		if (key->cert == NULL || buffer_len(&key->cert->certblob) == 0)
			fatal("%s: no cert/certblob", __func__);
		buffer_put_string(b, buffer_ptr(&key->cert->certblob),
		    buffer_len(&key->cert->certblob));
		buffer_put_bignum2(b, key->dsa->priv_key);
		break;
	}
	buffer_put_cstring(b, comment);
}

/*
 * Adds an identity to the authentication server.  This call is not meant to
 * be used by normal applications.
 */

int
ssh_add_identity_constrained(AuthenticationConnection *auth, Key *key,
    const char *comment, u_int life, u_int confirm)
{
	Buffer msg;
	int type, constrained = (life || confirm);

	buffer_init(&msg);

	switch (key->type) {
	case KEY_RSA1:
		type = constrained ?
		    SSH_AGENTC_ADD_RSA_ID_CONSTRAINED :
		    SSH_AGENTC_ADD_RSA_IDENTITY;
		buffer_put_char(&msg, type);
		ssh_encode_identity_rsa1(&msg, key->rsa, comment);
		break;
	case KEY_RSA:
	case KEY_RSA_CERT:
	case KEY_DSA:
	case KEY_DSA_CERT:
		type = constrained ?
		    SSH2_AGENTC_ADD_ID_CONSTRAINED :
		    SSH2_AGENTC_ADD_IDENTITY;
		buffer_put_char(&msg, type);
		ssh_encode_identity_ssh2(&msg, key, comment);
		break;
	default:
		buffer_free(&msg);
		return 0;
	}
	if (constrained) {
		if (life != 0) {
			buffer_put_char(&msg, SSH_AGENT_CONSTRAIN_LIFETIME);
			buffer_put_int(&msg, life);
		}
		if (confirm != 0)
			buffer_put_char(&msg, SSH_AGENT_CONSTRAIN_CONFIRM);
	}
	if (ssh_request_reply(auth, &msg, &msg) == 0) {
		buffer_free(&msg);
		return 0;
	}
	type = buffer_get_char(&msg);
	buffer_free(&msg);
	return decode_reply(type);
}

/*
 * Removes an identity from the authentication server.  This call is not
 * meant to be used by normal applications.
 */

int
ssh_remove_identity(AuthenticationConnection *auth, Key *key)
{
	Buffer msg;
	int type;
	u_char *blob;
	u_int blen;

	buffer_init(&msg);

	if (key->type == KEY_RSA1) {
		buffer_put_char(&msg, SSH_AGENTC_REMOVE_RSA_IDENTITY);
		buffer_put_int(&msg, BN_num_bits(key->rsa->n));
		buffer_put_bignum(&msg, key->rsa->e);
		buffer_put_bignum(&msg, key->rsa->n);
	} else if (key_type_plain(key->type) == KEY_DSA ||
	    key_type_plain(key->type) == KEY_RSA) {
		key_to_blob(key, &blob, &blen);
		buffer_put_char(&msg, SSH2_AGENTC_REMOVE_IDENTITY);
		buffer_put_string(&msg, blob, blen);
		xfree(blob);
	} else {
		buffer_free(&msg);
		return 0;
	}
	if (ssh_request_reply(auth, &msg, &msg) == 0) {
		buffer_free(&msg);
		return 0;
	}
	type = buffer_get_char(&msg);
	buffer_free(&msg);
	return decode_reply(type);
}

int
ssh_update_card(AuthenticationConnection *auth, int add,
    const char *reader_id, const char *pin, u_int life, u_int confirm)
{
	Buffer msg;
	int type, constrained = (life || confirm);

	if (add) {
		type = constrained ?
		    SSH_AGENTC_ADD_SMARTCARD_KEY_CONSTRAINED :
		    SSH_AGENTC_ADD_SMARTCARD_KEY;
	} else
		type = SSH_AGENTC_REMOVE_SMARTCARD_KEY;

	buffer_init(&msg);
	buffer_put_char(&msg, type);
	buffer_put_cstring(&msg, reader_id);
	buffer_put_cstring(&msg, pin);

	if (constrained) {
		if (life != 0) {
			buffer_put_char(&msg, SSH_AGENT_CONSTRAIN_LIFETIME);
			buffer_put_int(&msg, life);
		}
		if (confirm != 0)
			buffer_put_char(&msg, SSH_AGENT_CONSTRAIN_CONFIRM);
	}

	if (ssh_request_reply(auth, &msg, &msg) == 0) {
		buffer_free(&msg);
		return 0;
	}
	type = buffer_get_char(&msg);
	buffer_free(&msg);
	return decode_reply(type);
}

/*
 * Removes all identities from the agent.  This call is not meant to be used
 * by normal applications.
 */

int
ssh_remove_all_identities(AuthenticationConnection *auth, int version)
{
	Buffer msg;
	int type;
	int code = (version==1) ?
		SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES :
		SSH2_AGENTC_REMOVE_ALL_IDENTITIES;

	buffer_init(&msg);
	buffer_put_char(&msg, code);

	if (ssh_request_reply(auth, &msg, &msg) == 0) {
		buffer_free(&msg);
		return 0;
	}
	type = buffer_get_char(&msg);
	buffer_free(&msg);
	return decode_reply(type);
}

int
decode_reply(int type)
{
	switch (type) {
	case SSH_AGENT_FAILURE:
	case SSH_COM_AGENT2_FAILURE:
	case SSH2_AGENT_FAILURE:
		logit("SSH_AGENT_FAILURE");
		return 0;
	case SSH_AGENT_SUCCESS:
		return 1;
	default:
		fatal("Bad response from authentication agent: %d", type);
	}
	/* NOTREACHED */
	return 0;
}
