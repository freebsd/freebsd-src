/*
 *
 * authfd.c
 *
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 *
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * Created: Wed Mar 29 01:30:28 1995 ylo
 *
 * Functions for connecting the local authentication agent.
 *
 * $FreeBSD$
 */

#include "includes.h"
RCSID("$Id: authfd.c,v 1.19 2000/04/29 18:11:52 markus Exp $");

#include "ssh.h"
#include "rsa.h"
#include "authfd.h"
#include "buffer.h"
#include "bufaux.h"
#include "xmalloc.h"
#include "getput.h"

#include <openssl/rsa.h>

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
	buffer_init(&auth->packet);
	buffer_init(&auth->identities);
	auth->howmany = 0;

	return auth;
}

/*
 * Closes the connection to the authentication agent and frees any associated
 * memory.
 */

void
ssh_close_authentication_connection(AuthenticationConnection *ac)
{
	buffer_free(&ac->packet);
	buffer_free(&ac->identities);
	close(ac->fd);
	xfree(ac);
}

/*
 * Returns the first authentication identity held by the agent.
 * Returns true if an identity is available, 0 otherwise.
 * The caller must initialize the integers before the call, and free the
 * comment after a successful call (before calling ssh_get_next_identity).
 */

int
ssh_get_first_identity(AuthenticationConnection *auth,
		       BIGNUM *e, BIGNUM *n, char **comment)
{
	unsigned char msg[8192];
	int len, l;

	/*
	 * Send a message to the agent requesting for a list of the
	 * identities it can represent.
	 */
	msg[0] = 0;
	msg[1] = 0;
	msg[2] = 0;
	msg[3] = 1;
	msg[4] = SSH_AGENTC_REQUEST_RSA_IDENTITIES;
	if (atomicio(write, auth->fd, msg, 5) != 5) {
		error("write auth->fd: %.100s", strerror(errno));
		return 0;
	}
	/* Read the length of the response.  XXX implement timeouts here. */
	len = 4;
	while (len > 0) {
		l = read(auth->fd, msg + 4 - len, len);
		if (l <= 0) {
			error("read auth->fd: %.100s", strerror(errno));
			return 0;
		}
		len -= l;
	}

	/*
	 * Extract the length, and check it for sanity.  (We cannot trust
	 * authentication agents).
	 */
	len = GET_32BIT(msg);
	if (len < 1 || len > 256 * 1024)
		fatal("Authentication reply message too long: %d\n", len);

	/* Read the packet itself. */
	buffer_clear(&auth->identities);
	while (len > 0) {
		l = len;
		if (l > sizeof(msg))
			l = sizeof(msg);
		l = read(auth->fd, msg, l);
		if (l <= 0)
			fatal("Incomplete authentication reply.");
		buffer_append(&auth->identities, (char *) msg, l);
		len -= l;
	}

	/* Get message type, and verify that we got a proper answer. */
	buffer_get(&auth->identities, (char *) msg, 1);
	if (msg[0] != SSH_AGENT_RSA_IDENTITIES_ANSWER)
		fatal("Bad authentication reply message type: %d", msg[0]);

	/* Get the number of entries in the response and check it for sanity. */
	auth->howmany = buffer_get_int(&auth->identities);
	if (auth->howmany > 1024)
		fatal("Too many identities in authentication reply: %d\n", auth->howmany);

	/* Return the first entry (if any). */
	return ssh_get_next_identity(auth, e, n, comment);
}

/*
 * Returns the next authentication identity for the agent.  Other functions
 * can be called between this and ssh_get_first_identity or two calls of this
 * function.  This returns 0 if there are no more identities.  The caller
 * must free comment after a successful return.
 */

int
ssh_get_next_identity(AuthenticationConnection *auth,
		      BIGNUM *e, BIGNUM *n, char **comment)
{
	unsigned int bits;

	/* Return failure if no more entries. */
	if (auth->howmany <= 0)
		return 0;

	/*
	 * Get the next entry from the packet.  These will abort with a fatal
	 * error if the packet is too short or contains corrupt data.
	 */
	bits = buffer_get_int(&auth->identities);
	buffer_get_bignum(&auth->identities, e);
	buffer_get_bignum(&auth->identities, n);
	*comment = buffer_get_string(&auth->identities, NULL);

	if (bits != BN_num_bits(n))
		log("Warning: identity keysize mismatch: actual %d, announced %u",
		    BN_num_bits(n), bits);

	/* Decrement the number of remaining entries. */
	auth->howmany--;

	return 1;
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
		      BIGNUM* e, BIGNUM *n, BIGNUM *challenge,
		      unsigned char session_id[16],
		      unsigned int response_type,
		      unsigned char response[16])
{
	Buffer buffer;
	unsigned char buf[8192];
	int len, l, i;

	/* Response type 0 is no longer supported. */
	if (response_type == 0)
		fatal("Compatibility with ssh protocol version 1.0 no longer supported.");

	/* Format a message to the agent. */
	buf[0] = SSH_AGENTC_RSA_CHALLENGE;
	buffer_init(&buffer);
	buffer_append(&buffer, (char *) buf, 1);
	buffer_put_int(&buffer, BN_num_bits(n));
	buffer_put_bignum(&buffer, e);
	buffer_put_bignum(&buffer, n);
	buffer_put_bignum(&buffer, challenge);
	buffer_append(&buffer, (char *) session_id, 16);
	buffer_put_int(&buffer, response_type);

	/* Get the length of the message, and format it in the buffer. */
	len = buffer_len(&buffer);
	PUT_32BIT(buf, len);

	/* Send the length and then the packet to the agent. */
	if (atomicio(write, auth->fd, buf, 4) != 4 ||
	    atomicio(write, auth->fd, buffer_ptr(&buffer),
	    buffer_len(&buffer)) != buffer_len(&buffer)) {
		error("Error writing to authentication socket.");
error_cleanup:
		buffer_free(&buffer);
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
			goto error_cleanup;
		}
		len -= l;
	}

	/* Extract the length, and check it for sanity. */
	len = GET_32BIT(buf);
	if (len > 256 * 1024)
		fatal("Authentication response too long: %d", len);

	/* Read the rest of the response in tothe buffer. */
	buffer_clear(&buffer);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		l = read(auth->fd, buf, l);
		if (l <= 0) {
			error("Error reading response from authentication socket.");
			goto error_cleanup;
		}
		buffer_append(&buffer, (char *) buf, l);
		len -= l;
	}

	/* Get the type of the packet. */
	buffer_get(&buffer, (char *) buf, 1);

	/* Check for agent failure message. */
	if (buf[0] == SSH_AGENT_FAILURE) {
		log("Agent admitted failure to authenticate using the key.");
		goto error_cleanup;
	}
	/* Now it must be an authentication response packet. */
	if (buf[0] != SSH_AGENT_RSA_RESPONSE)
		fatal("Bad authentication response: %d", buf[0]);

	/*
	 * Get the response from the packet.  This will abort with a fatal
	 * error if the packet is corrupt.
	 */
	for (i = 0; i < 16; i++)
		response[i] = buffer_get_char(&buffer);

	/* The buffer containing the packet is no longer needed. */
	buffer_free(&buffer);

	/* Correct answer. */
	return 1;
}

/*
 * Adds an identity to the authentication server.  This call is not meant to
 * be used by normal applications.
 */

int
ssh_add_identity(AuthenticationConnection *auth,
		 RSA * key, const char *comment)
{
	Buffer buffer;
	unsigned char buf[8192];
	int len, l, type;

	/* Format a message to the agent. */
	buffer_init(&buffer);
	buffer_put_char(&buffer, SSH_AGENTC_ADD_RSA_IDENTITY);
	buffer_put_int(&buffer, BN_num_bits(key->n));
	buffer_put_bignum(&buffer, key->n);
	buffer_put_bignum(&buffer, key->e);
	buffer_put_bignum(&buffer, key->d);
	/* To keep within the protocol: p < q for ssh. in SSL p > q */
	buffer_put_bignum(&buffer, key->iqmp);	/* ssh key->u */
	buffer_put_bignum(&buffer, key->q);	/* ssh key->p, SSL key->q */
	buffer_put_bignum(&buffer, key->p);	/* ssh key->q, SSL key->p */
	buffer_put_string(&buffer, comment, strlen(comment));

	/* Get the length of the message, and format it in the buffer. */
	len = buffer_len(&buffer);
	PUT_32BIT(buf, len);

	/* Send the length and then the packet to the agent. */
	if (atomicio(write, auth->fd, buf, 4) != 4 ||
	    atomicio(write, auth->fd, buffer_ptr(&buffer),
	    buffer_len(&buffer)) != buffer_len(&buffer)) {
		error("Error writing to authentication socket.");
error_cleanup:
		buffer_free(&buffer);
		return 0;
	}
	/* Wait for response from the agent.  First read the length of the
	   response packet. */
	len = 4;
	while (len > 0) {
		l = read(auth->fd, buf + 4 - len, len);
		if (l <= 0) {
			error("Error reading response length from authentication socket.");
			goto error_cleanup;
		}
		len -= l;
	}

	/* Extract the length, and check it for sanity. */
	len = GET_32BIT(buf);
	if (len > 256 * 1024)
		fatal("Add identity response too long: %d", len);

	/* Read the rest of the response in tothe buffer. */
	buffer_clear(&buffer);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		l = read(auth->fd, buf, l);
		if (l <= 0) {
			error("Error reading response from authentication socket.");
			goto error_cleanup;
		}
		buffer_append(&buffer, (char *) buf, l);
		len -= l;
	}

	/* Get the type of the packet. */
	type = buffer_get_char(&buffer);
	switch (type) {
	case SSH_AGENT_FAILURE:
		buffer_free(&buffer);
		return 0;
	case SSH_AGENT_SUCCESS:
		buffer_free(&buffer);
		return 1;
	default:
		fatal("Bad response to add identity from authentication agent: %d",
		      type);
	}
	/* NOTREACHED */
	return 0;
}

/*
 * Removes an identity from the authentication server.  This call is not
 * meant to be used by normal applications.
 */

int
ssh_remove_identity(AuthenticationConnection *auth, RSA *key)
{
	Buffer buffer;
	unsigned char buf[8192];
	int len, l, type;

	/* Format a message to the agent. */
	buffer_init(&buffer);
	buffer_put_char(&buffer, SSH_AGENTC_REMOVE_RSA_IDENTITY);
	buffer_put_int(&buffer, BN_num_bits(key->n));
	buffer_put_bignum(&buffer, key->e);
	buffer_put_bignum(&buffer, key->n);

	/* Get the length of the message, and format it in the buffer. */
	len = buffer_len(&buffer);
	PUT_32BIT(buf, len);

	/* Send the length and then the packet to the agent. */
	if (atomicio(write, auth->fd, buf, 4) != 4 ||
	    atomicio(write, auth->fd, buffer_ptr(&buffer),
	    buffer_len(&buffer)) != buffer_len(&buffer)) {
		error("Error writing to authentication socket.");
error_cleanup:
		buffer_free(&buffer);
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
			goto error_cleanup;
		}
		len -= l;
	}

	/* Extract the length, and check it for sanity. */
	len = GET_32BIT(buf);
	if (len > 256 * 1024)
		fatal("Remove identity response too long: %d", len);

	/* Read the rest of the response in tothe buffer. */
	buffer_clear(&buffer);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		l = read(auth->fd, buf, l);
		if (l <= 0) {
			error("Error reading response from authentication socket.");
			goto error_cleanup;
		}
		buffer_append(&buffer, (char *) buf, l);
		len -= l;
	}

	/* Get the type of the packet. */
	type = buffer_get_char(&buffer);
	switch (type) {
	case SSH_AGENT_FAILURE:
		buffer_free(&buffer);
		return 0;
	case SSH_AGENT_SUCCESS:
		buffer_free(&buffer);
		return 1;
	default:
		fatal("Bad response to remove identity from authentication agent: %d",
		      type);
	}
	/* NOTREACHED */
	return 0;
}

/*
 * Removes all identities from the agent.  This call is not meant to be used
 * by normal applications.
 */

int
ssh_remove_all_identities(AuthenticationConnection *auth)
{
	Buffer buffer;
	unsigned char buf[8192];
	int len, l, type;

	/* Get the length of the message, and format it in the buffer. */
	PUT_32BIT(buf, 1);
	buf[4] = SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES;

	/* Send the length and then the packet to the agent. */
	if (atomicio(write, auth->fd, buf, 5) != 5) {
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
		fatal("Remove identity response too long: %d", len);

	/* Read the rest of the response into the buffer. */
	buffer_init(&buffer);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		l = read(auth->fd, buf, l);
		if (l <= 0) {
			error("Error reading response from authentication socket.");
			buffer_free(&buffer);
			return 0;
		}
		buffer_append(&buffer, (char *) buf, l);
		len -= l;
	}

	/* Get the type of the packet. */
	type = buffer_get_char(&buffer);
	switch (type) {
	case SSH_AGENT_FAILURE:
		buffer_free(&buffer);
		return 0;
	case SSH_AGENT_SUCCESS:
		buffer_free(&buffer);
		return 1;
	default:
		fatal("Bad response to remove identity from authentication agent: %d",
		      type);
	}
	/* NOTREACHED */
	return 0;
}
