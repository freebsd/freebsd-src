/* $OpenBSD: authfd.c,v 1.134 2023/12/18 14:46:56 djm Exp $ */
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

#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#include "xmalloc.h"
#include "ssh.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "authfd.h"
#include "cipher.h"
#include "log.h"
#include "atomicio.h"
#include "misc.h"
#include "ssherr.h"

#define MAX_AGENT_IDENTITIES	2048		/* Max keys in agent reply */
#define MAX_AGENT_REPLY_LEN	(256 * 1024)	/* Max bytes in agent reply */

/* macro to check for "agent failure" message */
#define agent_failed(x) \
    ((x == SSH_AGENT_FAILURE) || \
    (x == SSH_COM_AGENT2_FAILURE) || \
    (x == SSH2_AGENT_FAILURE))

/* Convert success/failure response from agent to a err.h status */
static int
decode_reply(u_char type)
{
	if (agent_failed(type))
		return SSH_ERR_AGENT_FAILURE;
	else if (type == SSH_AGENT_SUCCESS)
		return 0;
	else
		return SSH_ERR_INVALID_FORMAT;
}

/*
 * Opens an authentication socket at the provided path and stores the file
 * descriptor in fdp. Returns 0 on success and an error on failure.
 */
int
ssh_get_authentication_socket_path(const char *authsocket, int *fdp)
{
	int sock, oerrno;
	struct sockaddr_un sunaddr;

	debug3_f("path '%s'", authsocket);
	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	strlcpy(sunaddr.sun_path, authsocket, sizeof(sunaddr.sun_path));

	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		return SSH_ERR_SYSTEM_ERROR;

	/* close on exec */
	if (fcntl(sock, F_SETFD, FD_CLOEXEC) == -1 ||
	    connect(sock, (struct sockaddr *)&sunaddr, sizeof(sunaddr)) == -1) {
		oerrno = errno;
		close(sock);
		errno = oerrno;
		return SSH_ERR_SYSTEM_ERROR;
	}
	if (fdp != NULL)
		*fdp = sock;
	else
		close(sock);
	return 0;
}

/*
 * Opens the default authentication socket and stores the file descriptor in
 * fdp. Returns 0 on success and an error on failure.
 */
int
ssh_get_authentication_socket(int *fdp)
{
	const char *authsocket;

	if (fdp != NULL)
		*fdp = -1;

	authsocket = getenv(SSH_AUTHSOCKET_ENV_NAME);
	if (authsocket == NULL || *authsocket == '\0')
		return SSH_ERR_AGENT_NOT_PRESENT;

	return ssh_get_authentication_socket_path(authsocket, fdp);
}

/* Communicate with agent: send request and read reply */
static int
ssh_request_reply(int sock, struct sshbuf *request, struct sshbuf *reply)
{
	int r;
	size_t l, len;
	char buf[1024];

	/* Get the length of the message, and format it in the buffer. */
	len = sshbuf_len(request);
	POKE_U32(buf, len);

	/* Send the length and then the packet to the agent. */
	if (atomicio(vwrite, sock, buf, 4) != 4 ||
	    atomicio(vwrite, sock, sshbuf_mutable_ptr(request),
	    sshbuf_len(request)) != sshbuf_len(request))
		return SSH_ERR_AGENT_COMMUNICATION;
	/*
	 * Wait for response from the agent.  First read the length of the
	 * response packet.
	 */
	if (atomicio(read, sock, buf, 4) != 4)
	    return SSH_ERR_AGENT_COMMUNICATION;

	/* Extract the length, and check it for sanity. */
	len = PEEK_U32(buf);
	if (len > MAX_AGENT_REPLY_LEN)
		return SSH_ERR_INVALID_FORMAT;

	/* Read the rest of the response in to the buffer. */
	sshbuf_reset(reply);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		if (atomicio(read, sock, buf, l) != l)
			return SSH_ERR_AGENT_COMMUNICATION;
		if ((r = sshbuf_put(reply, buf, l)) != 0)
			return r;
		len -= l;
	}
	return 0;
}

/* Communicate with agent: sent request, read and decode status reply */
static int
ssh_request_reply_decode(int sock, struct sshbuf *request)
{
	struct sshbuf *reply;
	int r;
	u_char type;

	if ((reply = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = ssh_request_reply(sock, request, reply)) != 0 ||
	    (r = sshbuf_get_u8(reply, &type)) != 0 ||
	    (r = decode_reply(type)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(reply);
	return r;
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

/* Lock/unlock agent */
int
ssh_lock_agent(int sock, int lock, const char *password)
{
	int r;
	u_char type = lock ? SSH_AGENTC_LOCK : SSH_AGENTC_UNLOCK;
	struct sshbuf *msg;

	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_u8(msg, type)) != 0 ||
	    (r = sshbuf_put_cstring(msg, password)) != 0 ||
	    (r = ssh_request_reply_decode(sock, msg)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(msg);
	return r;
}


static int
deserialise_identity2(struct sshbuf *ids, struct sshkey **keyp, char **commentp)
{
	int r;
	char *comment = NULL;
	const u_char *blob;
	size_t blen;

	if ((r = sshbuf_get_string_direct(ids, &blob, &blen)) != 0 ||
	    (r = sshbuf_get_cstring(ids, &comment, NULL)) != 0)
		goto out;
	if ((r = sshkey_from_blob(blob, blen, keyp)) != 0)
		goto out;
	if (commentp != NULL) {
		*commentp = comment;
		comment = NULL;
	}
	r = 0;
 out:
	free(comment);
	return r;
}

/*
 * Fetch list of identities held by the agent.
 */
int
ssh_fetch_identitylist(int sock, struct ssh_identitylist **idlp)
{
	u_char type;
	u_int32_t num, i;
	struct sshbuf *msg;
	struct ssh_identitylist *idl = NULL;
	int r;

	/*
	 * Send a message to the agent requesting for a list of the
	 * identities it can represent.
	 */
	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_u8(msg, SSH2_AGENTC_REQUEST_IDENTITIES)) != 0)
		goto out;

	if ((r = ssh_request_reply(sock, msg, msg)) != 0)
		goto out;

	/* Get message type, and verify that we got a proper answer. */
	if ((r = sshbuf_get_u8(msg, &type)) != 0)
		goto out;
	if (agent_failed(type)) {
		r = SSH_ERR_AGENT_FAILURE;
		goto out;
	} else if (type != SSH2_AGENT_IDENTITIES_ANSWER) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}

	/* Get the number of entries in the response and check it for sanity. */
	if ((r = sshbuf_get_u32(msg, &num)) != 0)
		goto out;
	if (num > MAX_AGENT_IDENTITIES) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (num == 0) {
		r = SSH_ERR_AGENT_NO_IDENTITIES;
		goto out;
	}

	/* Deserialise the response into a list of keys/comments */
	if ((idl = calloc(1, sizeof(*idl))) == NULL ||
	    (idl->keys = calloc(num, sizeof(*idl->keys))) == NULL ||
	    (idl->comments = calloc(num, sizeof(*idl->comments))) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	for (i = 0; i < num;) {
		if ((r = deserialise_identity2(msg, &(idl->keys[i]),
		    &(idl->comments[i]))) != 0) {
			if (r == SSH_ERR_KEY_TYPE_UNKNOWN) {
				/* Gracefully skip unknown key types */
				num--;
				continue;
			} else
				goto out;
		}
		i++;
	}
	idl->nkeys = num;
	*idlp = idl;
	idl = NULL;
	r = 0;
 out:
	sshbuf_free(msg);
	if (idl != NULL)
		ssh_free_identitylist(idl);
	return r;
}

void
ssh_free_identitylist(struct ssh_identitylist *idl)
{
	size_t i;

	if (idl == NULL)
		return;
	for (i = 0; i < idl->nkeys; i++) {
		if (idl->keys != NULL)
			sshkey_free(idl->keys[i]);
		if (idl->comments != NULL)
			free(idl->comments[i]);
	}
	free(idl->keys);
	free(idl->comments);
	free(idl);
}

/*
 * Check if the ssh agent has a given key.
 * Returns 0 if found, or a negative SSH_ERR_* error code on failure.
 */
int
ssh_agent_has_key(int sock, const struct sshkey *key)
{
	int r, ret = SSH_ERR_KEY_NOT_FOUND;
	size_t i;
	struct ssh_identitylist *idlist = NULL;

	if ((r = ssh_fetch_identitylist(sock, &idlist)) != 0) {
		return r;
	}

	for (i = 0; i < idlist->nkeys; i++) {
		if (sshkey_equal_public(idlist->keys[i], key)) {
			ret = 0;
			break;
		}
	}

	ssh_free_identitylist(idlist);
	return ret;
}

/*
 * Sends a challenge (typically from a server via ssh(1)) to the agent,
 * and waits for a response from the agent.
 * Returns true (non-zero) if the agent gave the correct answer, zero
 * otherwise.
 */


/* encode signature algorithm in flag bits, so we can keep the msg format */
static u_int
agent_encode_alg(const struct sshkey *key, const char *alg)
{
	if (alg != NULL && sshkey_type_plain(key->type) == KEY_RSA) {
		if (strcmp(alg, "rsa-sha2-256") == 0 ||
		    strcmp(alg, "rsa-sha2-256-cert-v01@openssh.com") == 0)
			return SSH_AGENT_RSA_SHA2_256;
		if (strcmp(alg, "rsa-sha2-512") == 0 ||
		    strcmp(alg, "rsa-sha2-512-cert-v01@openssh.com") == 0)
			return SSH_AGENT_RSA_SHA2_512;
	}
	return 0;
}

/* ask agent to sign data, returns err.h code on error, 0 on success */
int
ssh_agent_sign(int sock, const struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen, const char *alg, u_int compat)
{
	struct sshbuf *msg;
	u_char *sig = NULL, type = 0;
	size_t len = 0;
	u_int flags = 0;
	int r = SSH_ERR_INTERNAL_ERROR;

	*sigp = NULL;
	*lenp = 0;

	if (datalen > SSH_KEY_MAX_SIGN_DATA_SIZE)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	flags |= agent_encode_alg(key, alg);
	if ((r = sshbuf_put_u8(msg, SSH2_AGENTC_SIGN_REQUEST)) != 0 ||
	    (r = sshkey_puts(key, msg)) != 0 ||
	    (r = sshbuf_put_string(msg, data, datalen)) != 0 ||
	    (r = sshbuf_put_u32(msg, flags)) != 0)
		goto out;
	if ((r = ssh_request_reply(sock, msg, msg)) != 0)
		goto out;
	if ((r = sshbuf_get_u8(msg, &type)) != 0)
		goto out;
	if (agent_failed(type)) {
		r = SSH_ERR_AGENT_FAILURE;
		goto out;
	} else if (type != SSH2_AGENT_SIGN_RESPONSE) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshbuf_get_string(msg, &sig, &len)) != 0)
		goto out;
	/* Check what we actually got back from the agent. */
	if ((r = sshkey_check_sigtype(sig, len, alg)) != 0)
		goto out;
	/* success */
	*sigp = sig;
	*lenp = len;
	sig = NULL;
	len = 0;
	r = 0;
 out:
	freezero(sig, len);
	sshbuf_free(msg);
	return r;
}

/* Encode key for a message to the agent. */

static int
encode_dest_constraint_hop(struct sshbuf *m,
    const struct dest_constraint_hop *dch)
{
	struct sshbuf *b;
	u_int i;
	int r;

	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_cstring(b, dch->user)) != 0 ||
	    (r = sshbuf_put_cstring(b, dch->hostname)) != 0 ||
	    (r = sshbuf_put_string(b, NULL, 0)) != 0) /* reserved */
		goto out;
	for (i = 0; i < dch->nkeys; i++) {
		if ((r = sshkey_puts(dch->keys[i], b)) != 0 ||
		    (r = sshbuf_put_u8(b, dch->key_is_ca[i] != 0)) != 0)
			goto out;
	}
	if ((r = sshbuf_put_stringb(m, b)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	return r;
}

static int
encode_dest_constraint(struct sshbuf *m, const struct dest_constraint *dc)
{
	struct sshbuf *b;
	int r;

	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = encode_dest_constraint_hop(b, &dc->from)) != 0 ||
	    (r = encode_dest_constraint_hop(b, &dc->to)) != 0 ||
	    (r = sshbuf_put_string(b, NULL, 0)) != 0) /* reserved */
		goto out;
	if ((r = sshbuf_put_stringb(m, b)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(b);
	return r;
}

static int
encode_constraints(struct sshbuf *m, u_int life, u_int confirm,
    u_int maxsign, const char *provider,
    struct dest_constraint **dest_constraints, size_t ndest_constraints,
    int cert_only, struct sshkey **certs, size_t ncerts)
{
	int r;
	struct sshbuf *b = NULL;
	size_t i;

	if (life != 0) {
		if ((r = sshbuf_put_u8(m, SSH_AGENT_CONSTRAIN_LIFETIME)) != 0 ||
		    (r = sshbuf_put_u32(m, life)) != 0)
			goto out;
	}
	if (confirm != 0) {
		if ((r = sshbuf_put_u8(m, SSH_AGENT_CONSTRAIN_CONFIRM)) != 0)
			goto out;
	}
	if (maxsign != 0) {
		if ((r = sshbuf_put_u8(m, SSH_AGENT_CONSTRAIN_MAXSIGN)) != 0 ||
		    (r = sshbuf_put_u32(m, maxsign)) != 0)
			goto out;
	}
	if (provider != NULL) {
		if ((r = sshbuf_put_u8(m,
		    SSH_AGENT_CONSTRAIN_EXTENSION)) != 0 ||
		    (r = sshbuf_put_cstring(m,
		    "sk-provider@openssh.com")) != 0 ||
		    (r = sshbuf_put_cstring(m, provider)) != 0)
			goto out;
	}
	if (dest_constraints != NULL && ndest_constraints > 0) {
		if ((b = sshbuf_new()) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		for (i = 0; i < ndest_constraints; i++) {
			if ((r = encode_dest_constraint(b,
			    dest_constraints[i])) != 0)
				goto out;
		}
		if ((r = sshbuf_put_u8(m,
		    SSH_AGENT_CONSTRAIN_EXTENSION)) != 0 ||
		    (r = sshbuf_put_cstring(m,
		    "restrict-destination-v00@openssh.com")) != 0 ||
		    (r = sshbuf_put_stringb(m, b)) != 0)
			goto out;
		sshbuf_free(b);
		b = NULL;
	}
	if (ncerts != 0) {
		if ((b = sshbuf_new()) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		for (i = 0; i < ncerts; i++) {
			if ((r = sshkey_puts(certs[i], b)) != 0)
				goto out;
		}
		if ((r = sshbuf_put_u8(m,
		    SSH_AGENT_CONSTRAIN_EXTENSION)) != 0 ||
		    (r = sshbuf_put_cstring(m,
		    "associated-certs-v00@openssh.com")) != 0 ||
		    (r = sshbuf_put_u8(m, cert_only != 0)) != 0 ||
		    (r = sshbuf_put_stringb(m, b)) != 0)
			goto out;
		sshbuf_free(b);
		b = NULL;
	}
	r = 0;
 out:
	sshbuf_free(b);
	return r;
}

/*
 * Adds an identity to the authentication server.
 * This call is intended only for use by ssh-add(1) and like applications.
 */
int
ssh_add_identity_constrained(int sock, struct sshkey *key,
    const char *comment, u_int life, u_int confirm, u_int maxsign,
    const char *provider, struct dest_constraint **dest_constraints,
    size_t ndest_constraints)
{
	struct sshbuf *msg;
	int r, constrained = (life || confirm || maxsign ||
	    provider || dest_constraints);
	u_char type;

	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	switch (key->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA:
	case KEY_RSA_CERT:
	case KEY_DSA:
	case KEY_DSA_CERT:
	case KEY_ECDSA:
	case KEY_ECDSA_CERT:
	case KEY_ECDSA_SK:
	case KEY_ECDSA_SK_CERT:
#endif
	case KEY_ED25519:
	case KEY_ED25519_CERT:
	case KEY_ED25519_SK:
	case KEY_ED25519_SK_CERT:
	case KEY_XMSS:
	case KEY_XMSS_CERT:
		type = constrained ?
		    SSH2_AGENTC_ADD_ID_CONSTRAINED :
		    SSH2_AGENTC_ADD_IDENTITY;
		if ((r = sshbuf_put_u8(msg, type)) != 0 ||
		    (r = sshkey_private_serialize_maxsign(key, msg, maxsign,
		    0)) != 0 ||
		    (r = sshbuf_put_cstring(msg, comment)) != 0)
			goto out;
		break;
	default:
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if (constrained &&
	    (r = encode_constraints(msg, life, confirm, maxsign,
	    provider, dest_constraints, ndest_constraints, 0, NULL, 0)) != 0)
		goto out;
	if ((r = ssh_request_reply_decode(sock, msg)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(msg);
	return r;
}

/*
 * Removes an identity from the authentication server.
 * This call is intended only for use by ssh-add(1) and like applications.
 */
int
ssh_remove_identity(int sock, const struct sshkey *key)
{
	struct sshbuf *msg;
	int r;
	u_char *blob = NULL;
	size_t blen;

	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	if (key->type != KEY_UNSPEC) {
		if ((r = sshkey_to_blob(key, &blob, &blen)) != 0)
			goto out;
		if ((r = sshbuf_put_u8(msg,
		    SSH2_AGENTC_REMOVE_IDENTITY)) != 0 ||
		    (r = sshbuf_put_string(msg, blob, blen)) != 0)
			goto out;
	} else {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	if ((r = ssh_request_reply_decode(sock, msg)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	if (blob != NULL)
		freezero(blob, blen);
	sshbuf_free(msg);
	return r;
}

/*
 * Add/remove an token-based identity from the authentication server.
 * This call is intended only for use by ssh-add(1) and like applications.
 */
int
ssh_update_card(int sock, int add, const char *reader_id, const char *pin,
    u_int life, u_int confirm,
    struct dest_constraint **dest_constraints, size_t ndest_constraints,
    int cert_only, struct sshkey **certs, size_t ncerts)
{
	struct sshbuf *msg;
	int r, constrained = (life || confirm || dest_constraints || certs);
	u_char type;

	if (add) {
		type = constrained ?
		    SSH_AGENTC_ADD_SMARTCARD_KEY_CONSTRAINED :
		    SSH_AGENTC_ADD_SMARTCARD_KEY;
	} else
		type = SSH_AGENTC_REMOVE_SMARTCARD_KEY;

	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_u8(msg, type)) != 0 ||
	    (r = sshbuf_put_cstring(msg, reader_id)) != 0 ||
	    (r = sshbuf_put_cstring(msg, pin)) != 0)
		goto out;
	if (constrained &&
	    (r = encode_constraints(msg, life, confirm, 0, NULL,
	    dest_constraints, ndest_constraints,
	    cert_only, certs, ncerts)) != 0)
		goto out;
	if ((r = ssh_request_reply_decode(sock, msg)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(msg);
	return r;
}

/*
 * Removes all identities from the agent.
 * This call is intended only for use by ssh-add(1) and like applications.
 *
 * This supports the SSH protocol 1 message to because, when clearing all
 * keys from an agent, we generally want to clear both protocol v1 and v2
 * keys.
 */
int
ssh_remove_all_identities(int sock, int version)
{
	struct sshbuf *msg;
	u_char type = (version == 1) ?
	    SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES :
	    SSH2_AGENTC_REMOVE_ALL_IDENTITIES;
	int r;

	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_u8(msg, type)) != 0)
		goto out;
	if ((r = ssh_request_reply_decode(sock, msg)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(msg);
	return r;
}

/* Binds a session ID to a hostkey via the initial KEX signature. */
int
ssh_agent_bind_hostkey(int sock, const struct sshkey *key,
    const struct sshbuf *session_id, const struct sshbuf *signature,
    int forwarding)
{
	struct sshbuf *msg;
	int r;

	if (key == NULL || session_id == NULL || signature == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_u8(msg, SSH_AGENTC_EXTENSION)) != 0 ||
	    (r = sshbuf_put_cstring(msg, "session-bind@openssh.com")) != 0 ||
	    (r = sshkey_puts(key, msg)) != 0 ||
	    (r = sshbuf_put_stringb(msg, session_id)) != 0 ||
	    (r = sshbuf_put_stringb(msg, signature)) != 0 ||
	    (r = sshbuf_put_u8(msg, forwarding ? 1 : 0)) != 0)
		goto out;
	if ((r = ssh_request_reply_decode(sock, msg)) != 0)
		goto out;
	/* success */
	r = 0;
 out:
	sshbuf_free(msg);
	return r;
}
