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
RCSID("$OpenBSD: authfd.c,v 1.29 2000/10/09 21:51:00 markus Exp $");
RCSID("$FreeBSD$");

#include "ssh.h"
#include "rsa.h"
#include "buffer.h"
#include "bufaux.h"
#include "xmalloc.h"
#include "getput.h"

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/evp.h>
#include "key.h"
#include "authfd.h"
#include "kex.h"
#include "dsa.h"
#include "compat.h"

/* helper */
int	decode_reply(int type);

/* macro to check for "agent failure" message */
#define agent_failed(x) \
    ((x == SSH_AGENT_FAILURE) || (x == SSH_COM_AGENT2_FAILURE))

/* Returns the number of the authentication fd, or -1 if there is none. */

int
ssh_get_authentication_socket()
{
	const char *authsocket;
	int sock, len;
	struct sockaddr_un sunaddr;

	authsocket = getenv(SSH_AUTHSOCKET_ENV_NAME);
	if (!authsocket)
		return -1;

	sunaddr.sun_family = AF_UNIX;
	strlcpy(sunaddr.sun_path, authsocket, sizeof(sunaddr.sun_path));
	sunaddr.sun_len = len = SUN_LEN(&sunaddr)+1;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	/* close on exec */
	if (fcntl(sock, F_SETFD, 1) == -1) {
		close(sock);
		return -1;
	}
	if (connect(sock, (struct sockaddr *) & sunaddr, len) < 0) {
		close(sock);
		return -1;
	}
	return sock;
}

int
ssh_request_reply(AuthenticationConnection *auth, Buffer *request, Buffer *reply)
{
	int l, len;
	char buf[1024];

	/* Get the length of the message, and format it in the buffer. */
	len = buffer_len(request);
	PUT_32BIT(buf, len);

	/* Send the length and then the packet to the agent. */
	if (atomicio(write, auth->fd, buf, 4) != 4 ||
	    atomicio(write, auth->fd, buffer_ptr(request),
	    buffer_len(request)) != buffer_len(request)) {
		error("Error writing to authentication socket.");
		return 0;
	}
	/*
	 * Wait for response from the agent.  First read the length of the
	 * response packet.
	 */
	len = 4;
	while (len > 0) {
		l = read(auth->fd, buf + 4 - len, len);
		if (l <= 0) {
			error("Error reading response length from authentication socket.");
			return 0;
		}
		len -= l;
	}

	/* Extract the length, and check it for sanity. */
	len = GET_32BIT(buf);
	if (len > 256 * 1024)
		fatal("Authentication response too long: %d", len);

	/* Read the rest of the response in to the buffer. */
	buffer_clear(reply);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		l = read(auth->fd, buf, l);
		if (l <= 0) {
			error("Error reading response from authentication socket.");
			return 0;
		}
		buffer_append(reply, (char *) buf, l);
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
ssh_get_authentication_connection()
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

/*
 * Returns the first authentication identity held by the agent.
 */

Key *
ssh_get_first_identity(AuthenticationConnection *auth, char **comment, int version)
{
	int type, code1 = 0, code2 = 0;
	Buffer request;

	switch(version){
	case 1:
		code1 = SSH_AGENTC_REQUEST_RSA_IDENTITIES;
		code2 = SSH_AGENT_RSA_IDENTITIES_ANSWER;
		break;
	case 2:
		code1 = SSH2_AGENTC_REQUEST_IDENTITIES;
		code2 = SSH2_AGENT_IDENTITIES_ANSWER;
		break;
	default:
		return NULL;
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
		return NULL;
	}
	buffer_free(&request);

	/* Get message type, and verify that we got a proper answer. */
	type = buffer_get_char(&auth->identities);
	if (agent_failed(type)) {
		return NULL;
	} else if (type != code2) {
		fatal("Bad authentication reply message type: %d", type);
	}

	/* Get the number of entries in the response and check it for sanity. */
	auth->howmany = buffer_get_int(&auth->identities);
	if (auth->howmany > 1024)
		fatal("Too many identities in authentication reply: %d\n",
		    auth->howmany);

	/* Return the first entry (if any). */
	return ssh_get_next_identity(auth, comment, version);
}

Key *
ssh_get_next_identity(AuthenticationConnection *auth, char **comment, int version)
{
	unsigned int bits;
	unsigned char *blob;
	unsigned int blen;
	Key *key = NULL;

	/* Return failure if no more entries. */
	if (auth->howmany <= 0)
		return NULL;

	/*
	 * Get the next entry from the packet.  These will abort with a fatal
	 * error if the packet is too short or contains corrupt data.
	 */
	switch(version){
	case 1:
		key = key_new(KEY_RSA);
		bits = buffer_get_int(&auth->identities);
		buffer_get_bignum(&auth->identities, key->rsa->e);
		buffer_get_bignum(&auth->identities, key->rsa->n);
		*comment = buffer_get_string(&auth->identities, NULL);
		if (bits != BN_num_bits(key->rsa->n))
			log("Warning: identity keysize mismatch: actual %d, announced %u",
			    BN_num_bits(key->rsa->n), bits);
		break;
	case 2:
		blob = buffer_get_string(&auth->identities, &blen);
		*comment = buffer_get_string(&auth->identities, NULL);
		key = dsa_key_from_blob(blob, blen);
		xfree(blob);
		break;
	default:
		return NULL;
		break;
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
    unsigned char session_id[16],
    unsigned int response_type,
    unsigned char response[16])
{
	Buffer buffer;
	int success = 0;
	int i;
	int type;

	if (key->type != KEY_RSA)
		return 0;
	if (response_type == 0) {
		log("Compatibility with ssh protocol version 1.0 no longer supported.");
		return 0;
	}
	buffer_init(&buffer);
	buffer_put_char(&buffer, SSH_AGENTC_RSA_CHALLENGE);
	buffer_put_int(&buffer, BN_num_bits(key->rsa->n));
	buffer_put_bignum(&buffer, key->rsa->e);
	buffer_put_bignum(&buffer, key->rsa->n);
	buffer_put_bignum(&buffer, challenge);
	buffer_append(&buffer, (char *) session_id, 16);
	buffer_put_int(&buffer, response_type);

	if (ssh_request_reply(auth, &buffer, &buffer) == 0) {
		buffer_free(&buffer);
		return 0;
	}
	type = buffer_get_char(&buffer);

	if (agent_failed(type)) {
		log("Agent admitted failure to authenticate using the key.");
	} else if (type != SSH_AGENT_RSA_RESPONSE) {
		fatal("Bad authentication response: %d", type);
	} else {
		success = 1;
		/*
		 * Get the response from the packet.  This will abort with a
		 * fatal error if the packet is corrupt.
		 */
		for (i = 0; i < 16; i++)
			response[i] = buffer_get_char(&buffer);
	}
	buffer_free(&buffer);
	return success;
}

/* ask agent to sign data, returns -1 on error, 0 on success */
int
ssh_agent_sign(AuthenticationConnection *auth,
    Key *key,
    unsigned char **sigp, int *lenp,
    unsigned char *data, int datalen)
{
	extern int datafellows;
	Buffer msg;
	unsigned char *blob;
	unsigned int blen;
	int type, flags = 0;
	int ret = -1;

	if (dsa_make_key_blob(key, &blob, &blen) == 0)
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
		log("Agent admitted failure to sign using the key.");
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

void
ssh_encode_identity_rsa(Buffer *b, RSA *key, const char *comment)
{
	buffer_clear(b);
	buffer_put_char(b, SSH_AGENTC_ADD_RSA_IDENTITY);
	buffer_put_int(b, BN_num_bits(key->n));
	buffer_put_bignum(b, key->n);
	buffer_put_bignum(b, key->e);
	buffer_put_bignum(b, key->d);
	/* To keep within the protocol: p < q for ssh. in SSL p > q */
	buffer_put_bignum(b, key->iqmp);	/* ssh key->u */
	buffer_put_bignum(b, key->q);	/* ssh key->p, SSL key->q */
	buffer_put_bignum(b, key->p);	/* ssh key->q, SSL key->p */
	buffer_put_string(b, comment, strlen(comment));
}

void
ssh_encode_identity_dsa(Buffer *b, DSA *key, const char *comment)
{
	buffer_clear(b);
	buffer_put_char(b, SSH2_AGENTC_ADD_IDENTITY);
	buffer_put_cstring(b, KEX_DSS);
	buffer_put_bignum2(b, key->p);
	buffer_put_bignum2(b, key->q);
	buffer_put_bignum2(b, key->g);
	buffer_put_bignum2(b, key->pub_key);
	buffer_put_bignum2(b, key->priv_key);
	buffer_put_string(b, comment, strlen(comment));
}

/*
 * Adds an identity to the authentication server.  This call is not meant to
 * be used by normal applications.
 */

int
ssh_add_identity(AuthenticationConnection *auth, Key *key, const char *comment)
{
	Buffer msg;
	int type;

	buffer_init(&msg);

	switch (key->type) {
	case KEY_RSA:
		ssh_encode_identity_rsa(&msg, key->rsa, comment);
		break;
	case KEY_DSA:
		ssh_encode_identity_dsa(&msg, key->dsa, comment);
		break;
	default:
		buffer_free(&msg);
		return 0;
		break;
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
	unsigned char *blob;
	unsigned int blen;

	buffer_init(&msg);

	if (key->type == KEY_RSA) {
		buffer_put_char(&msg, SSH_AGENTC_REMOVE_RSA_IDENTITY);
		buffer_put_int(&msg, BN_num_bits(key->rsa->n));
		buffer_put_bignum(&msg, key->rsa->e);
		buffer_put_bignum(&msg, key->rsa->n);
	} else if (key->type == KEY_DSA) {
		dsa_make_key_blob(key, &blob, &blen);
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
		log("SSH_AGENT_FAILURE");
		return 0;
	case SSH_AGENT_SUCCESS:
		return 1;
	default:
		fatal("Bad response from authentication agent: %d", type);
	}
	/* NOTREACHED */
	return 0;
}
