/* $OpenBSD: sshconnect1.c,v 1.71 2013/05/17 00:13:14 djm Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Code to connect to a remote host, and to perform the client side of the
 * login (authentication) dialog.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <openssl/bn.h>
#include <openssl/md5.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>

#include "xmalloc.h"
#include "ssh.h"
#include "ssh1.h"
#include "rsa.h"
#include "buffer.h"
#include "packet.h"
#include "key.h"
#include "cipher.h"
#include "kex.h"
#include "uidswap.h"
#include "log.h"
#include "readconf.h"
#include "authfd.h"
#include "sshconnect.h"
#include "authfile.h"
#include "misc.h"
#include "canohost.h"
#include "hostfile.h"
#include "auth.h"

/* Session id for the current session. */
u_char session_id[16];
u_int supported_authentications = 0;

extern Options options;
extern char *__progname;

/*
 * Checks if the user has an authentication agent, and if so, tries to
 * authenticate using the agent.
 */
static int
try_agent_authentication(void)
{
	int type;
	char *comment;
	AuthenticationConnection *auth;
	u_char response[16];
	u_int i;
	Key *key;
	BIGNUM *challenge;

	/* Get connection to the agent. */
	auth = ssh_get_authentication_connection();
	if (!auth)
		return 0;

	if ((challenge = BN_new()) == NULL)
		fatal("try_agent_authentication: BN_new failed");
	/* Loop through identities served by the agent. */
	for (key = ssh_get_first_identity(auth, &comment, 1);
	    key != NULL;
	    key = ssh_get_next_identity(auth, &comment, 1)) {

		/* Try this identity. */
		debug("Trying RSA authentication via agent with '%.100s'", comment);
		free(comment);

		/* Tell the server that we are willing to authenticate using this key. */
		packet_start(SSH_CMSG_AUTH_RSA);
		packet_put_bignum(key->rsa->n);
		packet_send();
		packet_write_wait();

		/* Wait for server's response. */
		type = packet_read();

		/* The server sends failure if it doesn't like our key or
		   does not support RSA authentication. */
		if (type == SSH_SMSG_FAILURE) {
			debug("Server refused our key.");
			key_free(key);
			continue;
		}
		/* Otherwise it should have sent a challenge. */
		if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
			packet_disconnect("Protocol error during RSA authentication: %d",
					  type);

		packet_get_bignum(challenge);
		packet_check_eom();

		debug("Received RSA challenge from server.");

		/* Ask the agent to decrypt the challenge. */
		if (!ssh_decrypt_challenge(auth, key, challenge, session_id, 1, response)) {
			/*
			 * The agent failed to authenticate this identifier
			 * although it advertised it supports this.  Just
			 * return a wrong value.
			 */
			logit("Authentication agent failed to decrypt challenge.");
			memset(response, 0, sizeof(response));
		}
		key_free(key);
		debug("Sending response to RSA challenge.");

		/* Send the decrypted challenge back to the server. */
		packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
		for (i = 0; i < 16; i++)
			packet_put_char(response[i]);
		packet_send();
		packet_write_wait();

		/* Wait for response from the server. */
		type = packet_read();

		/* The server returns success if it accepted the authentication. */
		if (type == SSH_SMSG_SUCCESS) {
			ssh_close_authentication_connection(auth);
			BN_clear_free(challenge);
			debug("RSA authentication accepted by server.");
			return 1;
		}
		/* Otherwise it should return failure. */
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error waiting RSA auth response: %d",
					  type);
	}
	ssh_close_authentication_connection(auth);
	BN_clear_free(challenge);
	debug("RSA authentication using agent refused.");
	return 0;
}

/*
 * Computes the proper response to a RSA challenge, and sends the response to
 * the server.
 */
static void
respond_to_rsa_challenge(BIGNUM * challenge, RSA * prv)
{
	u_char buf[32], response[16];
	MD5_CTX md;
	int i, len;

	/* Decrypt the challenge using the private key. */
	/* XXX think about Bleichenbacher, too */
	if (rsa_private_decrypt(challenge, challenge, prv) <= 0)
		packet_disconnect(
		    "respond_to_rsa_challenge: rsa_private_decrypt failed");

	/* Compute the response. */
	/* The response is MD5 of decrypted challenge plus session id. */
	len = BN_num_bytes(challenge);
	if (len <= 0 || (u_int)len > sizeof(buf))
		packet_disconnect(
		    "respond_to_rsa_challenge: bad challenge length %d", len);

	memset(buf, 0, sizeof(buf));
	BN_bn2bin(challenge, buf + sizeof(buf) - len);
	MD5_Init(&md);
	MD5_Update(&md, buf, 32);
	MD5_Update(&md, session_id, 16);
	MD5_Final(response, &md);

	debug("Sending response to host key RSA challenge.");

	/* Send the response back to the server. */
	packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
	for (i = 0; i < 16; i++)
		packet_put_char(response[i]);
	packet_send();
	packet_write_wait();

	memset(buf, 0, sizeof(buf));
	memset(response, 0, sizeof(response));
	memset(&md, 0, sizeof(md));
}

/*
 * Checks if the user has authentication file, and if so, tries to authenticate
 * the user using it.
 */
static int
try_rsa_authentication(int idx)
{
	BIGNUM *challenge;
	Key *public, *private;
	char buf[300], *passphrase, *comment, *authfile;
	int i, perm_ok = 1, type, quit;

	public = options.identity_keys[idx];
	authfile = options.identity_files[idx];
	comment = xstrdup(authfile);

	debug("Trying RSA authentication with key '%.100s'", comment);

	/* Tell the server that we are willing to authenticate using this key. */
	packet_start(SSH_CMSG_AUTH_RSA);
	packet_put_bignum(public->rsa->n);
	packet_send();
	packet_write_wait();

	/* Wait for server's response. */
	type = packet_read();

	/*
	 * The server responds with failure if it doesn't like our key or
	 * doesn't support RSA authentication.
	 */
	if (type == SSH_SMSG_FAILURE) {
		debug("Server refused our key.");
		free(comment);
		return 0;
	}
	/* Otherwise, the server should respond with a challenge. */
	if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
		packet_disconnect("Protocol error during RSA authentication: %d", type);

	/* Get the challenge from the packet. */
	if ((challenge = BN_new()) == NULL)
		fatal("try_rsa_authentication: BN_new failed");
	packet_get_bignum(challenge);
	packet_check_eom();

	debug("Received RSA challenge from server.");

	/*
	 * If the key is not stored in external hardware, we have to
	 * load the private key.  Try first with empty passphrase; if it
	 * fails, ask for a passphrase.
	 */
	if (public->flags & KEY_FLAG_EXT)
		private = public;
	else
		private = key_load_private_type(KEY_RSA1, authfile, "", NULL,
		    &perm_ok);
	if (private == NULL && !options.batch_mode && perm_ok) {
		snprintf(buf, sizeof(buf),
		    "Enter passphrase for RSA key '%.100s': ", comment);
		for (i = 0; i < options.number_of_password_prompts; i++) {
			passphrase = read_passphrase(buf, 0);
			if (strcmp(passphrase, "") != 0) {
				private = key_load_private_type(KEY_RSA1,
				    authfile, passphrase, NULL, NULL);
				quit = 0;
			} else {
				debug2("no passphrase given, try next key");
				quit = 1;
			}
			memset(passphrase, 0, strlen(passphrase));
			free(passphrase);
			if (private != NULL || quit)
				break;
			debug2("bad passphrase given, try again...");
		}
	}
	/* We no longer need the comment. */
	free(comment);

	if (private == NULL) {
		if (!options.batch_mode && perm_ok)
			error("Bad passphrase.");

		/* Send a dummy response packet to avoid protocol error. */
		packet_start(SSH_CMSG_AUTH_RSA_RESPONSE);
		for (i = 0; i < 16; i++)
			packet_put_char(0);
		packet_send();
		packet_write_wait();

		/* Expect the server to reject it... */
		packet_read_expect(SSH_SMSG_FAILURE);
		BN_clear_free(challenge);
		return 0;
	}

	/* Compute and send a response to the challenge. */
	respond_to_rsa_challenge(challenge, private->rsa);

	/* Destroy the private key unless it in external hardware. */
	if (!(private->flags & KEY_FLAG_EXT))
		key_free(private);

	/* We no longer need the challenge. */
	BN_clear_free(challenge);

	/* Wait for response from the server. */
	type = packet_read();
	if (type == SSH_SMSG_SUCCESS) {
		debug("RSA authentication accepted by server.");
		return 1;
	}
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error waiting RSA auth response: %d", type);
	debug("RSA authentication refused.");
	return 0;
}

/*
 * Tries to authenticate the user using combined rhosts or /etc/hosts.equiv
 * authentication and RSA host authentication.
 */
static int
try_rhosts_rsa_authentication(const char *local_user, Key * host_key)
{
	int type;
	BIGNUM *challenge;

	debug("Trying rhosts or /etc/hosts.equiv with RSA host authentication.");

	/* Tell the server that we are willing to authenticate using this key. */
	packet_start(SSH_CMSG_AUTH_RHOSTS_RSA);
	packet_put_cstring(local_user);
	packet_put_int(BN_num_bits(host_key->rsa->n));
	packet_put_bignum(host_key->rsa->e);
	packet_put_bignum(host_key->rsa->n);
	packet_send();
	packet_write_wait();

	/* Wait for server's response. */
	type = packet_read();

	/* The server responds with failure if it doesn't admit our
	   .rhosts authentication or doesn't know our host key. */
	if (type == SSH_SMSG_FAILURE) {
		debug("Server refused our rhosts authentication or host key.");
		return 0;
	}
	/* Otherwise, the server should respond with a challenge. */
	if (type != SSH_SMSG_AUTH_RSA_CHALLENGE)
		packet_disconnect("Protocol error during RSA authentication: %d", type);

	/* Get the challenge from the packet. */
	if ((challenge = BN_new()) == NULL)
		fatal("try_rhosts_rsa_authentication: BN_new failed");
	packet_get_bignum(challenge);
	packet_check_eom();

	debug("Received RSA challenge for host key from server.");

	/* Compute a response to the challenge. */
	respond_to_rsa_challenge(challenge, host_key->rsa);

	/* We no longer need the challenge. */
	BN_clear_free(challenge);

	/* Wait for response from the server. */
	type = packet_read();
	if (type == SSH_SMSG_SUCCESS) {
		debug("Rhosts or /etc/hosts.equiv with RSA host authentication accepted by server.");
		return 1;
	}
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error waiting RSA auth response: %d", type);
	debug("Rhosts or /etc/hosts.equiv with RSA host authentication refused.");
	return 0;
}

/*
 * Tries to authenticate with any string-based challenge/response system.
 * Note that the client code is not tied to s/key or TIS.
 */
static int
try_challenge_response_authentication(void)
{
	int type, i;
	u_int clen;
	char prompt[1024];
	char *challenge, *response;

	debug("Doing challenge response authentication.");

	for (i = 0; i < options.number_of_password_prompts; i++) {
		/* request a challenge */
		packet_start(SSH_CMSG_AUTH_TIS);
		packet_send();
		packet_write_wait();

		type = packet_read();
		if (type != SSH_SMSG_FAILURE &&
		    type != SSH_SMSG_AUTH_TIS_CHALLENGE) {
			packet_disconnect("Protocol error: got %d in response "
			    "to SSH_CMSG_AUTH_TIS", type);
		}
		if (type != SSH_SMSG_AUTH_TIS_CHALLENGE) {
			debug("No challenge.");
			return 0;
		}
		challenge = packet_get_string(&clen);
		packet_check_eom();
		snprintf(prompt, sizeof prompt, "%s%s", challenge,
		    strchr(challenge, '\n') ? "" : "\nResponse: ");
		free(challenge);
		if (i != 0)
			error("Permission denied, please try again.");
		if (options.cipher == SSH_CIPHER_NONE)
			logit("WARNING: Encryption is disabled! "
			    "Response will be transmitted in clear text.");
		response = read_passphrase(prompt, 0);
		if (strcmp(response, "") == 0) {
			free(response);
			break;
		}
		packet_start(SSH_CMSG_AUTH_TIS_RESPONSE);
		ssh_put_password(response);
		memset(response, 0, strlen(response));
		free(response);
		packet_send();
		packet_write_wait();
		type = packet_read();
		if (type == SSH_SMSG_SUCCESS)
			return 1;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response "
			    "to SSH_CMSG_AUTH_TIS_RESPONSE", type);
	}
	/* failure */
	return 0;
}

/*
 * Tries to authenticate with plain passwd authentication.
 */
static int
try_password_authentication(char *prompt)
{
	int type, i;
	char *password;

	debug("Doing password authentication.");
	if (options.cipher == SSH_CIPHER_NONE)
		logit("WARNING: Encryption is disabled! Password will be transmitted in clear text.");
	for (i = 0; i < options.number_of_password_prompts; i++) {
		if (i != 0)
			error("Permission denied, please try again.");
		password = read_passphrase(prompt, 0);
		packet_start(SSH_CMSG_AUTH_PASSWORD);
		ssh_put_password(password);
		memset(password, 0, strlen(password));
		free(password);
		packet_send();
		packet_write_wait();

		type = packet_read();
		if (type == SSH_SMSG_SUCCESS)
			return 1;
		if (type != SSH_SMSG_FAILURE)
			packet_disconnect("Protocol error: got %d in response to passwd auth", type);
	}
	/* failure */
	return 0;
}

/*
 * SSH1 key exchange
 */
void
ssh_kex(char *host, struct sockaddr *hostaddr)
{
	int i;
	BIGNUM *key;
	Key *host_key, *server_key;
	int bits, rbits;
	int ssh_cipher_default = SSH_CIPHER_3DES;
	u_char session_key[SSH_SESSION_KEY_LENGTH];
	u_char cookie[8];
	u_int supported_ciphers;
	u_int server_flags, client_flags;
	u_int32_t rnd = 0;

	debug("Waiting for server public key.");

	/* Wait for a public key packet from the server. */
	packet_read_expect(SSH_SMSG_PUBLIC_KEY);

	/* Get cookie from the packet. */
	for (i = 0; i < 8; i++)
		cookie[i] = packet_get_char();

	/* Get the public key. */
	server_key = key_new(KEY_RSA1);
	bits = packet_get_int();
	packet_get_bignum(server_key->rsa->e);
	packet_get_bignum(server_key->rsa->n);

	rbits = BN_num_bits(server_key->rsa->n);
	if (bits != rbits) {
		logit("Warning: Server lies about size of server public key: "
		    "actual size is %d bits vs. announced %d.", rbits, bits);
		logit("Warning: This may be due to an old implementation of ssh.");
	}
	/* Get the host key. */
	host_key = key_new(KEY_RSA1);
	bits = packet_get_int();
	packet_get_bignum(host_key->rsa->e);
	packet_get_bignum(host_key->rsa->n);

	rbits = BN_num_bits(host_key->rsa->n);
	if (bits != rbits) {
		logit("Warning: Server lies about size of server host key: "
		    "actual size is %d bits vs. announced %d.", rbits, bits);
		logit("Warning: This may be due to an old implementation of ssh.");
	}

	/* Get protocol flags. */
	server_flags = packet_get_int();
	packet_set_protocol_flags(server_flags);

	supported_ciphers = packet_get_int();
	supported_authentications = packet_get_int();
	packet_check_eom();

	debug("Received server public key (%d bits) and host key (%d bits).",
	    BN_num_bits(server_key->rsa->n), BN_num_bits(host_key->rsa->n));

	if (verify_host_key(host, hostaddr, host_key) == -1)
		fatal("Host key verification failed.");

	client_flags = SSH_PROTOFLAG_SCREEN_NUMBER | SSH_PROTOFLAG_HOST_IN_FWD_OPEN;

	derive_ssh1_session_id(host_key->rsa->n, server_key->rsa->n, cookie, session_id);

	/* Generate a session key. */
	arc4random_stir();

	/*
	 * Generate an encryption key for the session.   The key is a 256 bit
	 * random number, interpreted as a 32-byte key, with the least
	 * significant 8 bits being the first byte of the key.
	 */
	for (i = 0; i < 32; i++) {
		if (i % 4 == 0)
			rnd = arc4random();
		session_key[i] = rnd & 0xff;
		rnd >>= 8;
	}

	/*
	 * According to the protocol spec, the first byte of the session key
	 * is the highest byte of the integer.  The session key is xored with
	 * the first 16 bytes of the session id.
	 */
	if ((key = BN_new()) == NULL)
		fatal("ssh_kex: BN_new failed");
	if (BN_set_word(key, 0) == 0)
		fatal("ssh_kex: BN_set_word failed");
	for (i = 0; i < SSH_SESSION_KEY_LENGTH; i++) {
		if (BN_lshift(key, key, 8) == 0)
			fatal("ssh_kex: BN_lshift failed");
		if (i < 16) {
			if (BN_add_word(key, session_key[i] ^ session_id[i])
			    == 0)
				fatal("ssh_kex: BN_add_word failed");
		} else {
			if (BN_add_word(key, session_key[i]) == 0)
				fatal("ssh_kex: BN_add_word failed");
		}
	}

	/*
	 * Encrypt the integer using the public key and host key of the
	 * server (key with smaller modulus first).
	 */
	if (BN_cmp(server_key->rsa->n, host_key->rsa->n) < 0) {
		/* Public key has smaller modulus. */
		if (BN_num_bits(host_key->rsa->n) <
		    BN_num_bits(server_key->rsa->n) + SSH_KEY_BITS_RESERVED) {
			fatal("respond_to_rsa_challenge: host_key %d < server_key %d + "
			    "SSH_KEY_BITS_RESERVED %d",
			    BN_num_bits(host_key->rsa->n),
			    BN_num_bits(server_key->rsa->n),
			    SSH_KEY_BITS_RESERVED);
		}
		rsa_public_encrypt(key, key, server_key->rsa);
		rsa_public_encrypt(key, key, host_key->rsa);
	} else {
		/* Host key has smaller modulus (or they are equal). */
		if (BN_num_bits(server_key->rsa->n) <
		    BN_num_bits(host_key->rsa->n) + SSH_KEY_BITS_RESERVED) {
			fatal("respond_to_rsa_challenge: server_key %d < host_key %d + "
			    "SSH_KEY_BITS_RESERVED %d",
			    BN_num_bits(server_key->rsa->n),
			    BN_num_bits(host_key->rsa->n),
			    SSH_KEY_BITS_RESERVED);
		}
		rsa_public_encrypt(key, key, host_key->rsa);
		rsa_public_encrypt(key, key, server_key->rsa);
	}

	/* Destroy the public keys since we no longer need them. */
	key_free(server_key);
	key_free(host_key);

	if (options.cipher == SSH_CIPHER_NOT_SET) {
		if (cipher_mask_ssh1(1) & supported_ciphers & (1 << ssh_cipher_default))
			options.cipher = ssh_cipher_default;
	} else if (options.cipher == SSH_CIPHER_INVALID ||
	    !(cipher_mask_ssh1(1) & (1 << options.cipher))) {
		logit("No valid SSH1 cipher, using %.100s instead.",
		    cipher_name(ssh_cipher_default));
		options.cipher = ssh_cipher_default;
	}
	/* Check that the selected cipher is supported. */
	if (!(supported_ciphers & (1 << options.cipher)))
		fatal("Selected cipher type %.100s not supported by server.",
		    cipher_name(options.cipher));

	debug("Encryption type: %.100s", cipher_name(options.cipher));

	/* Send the encrypted session key to the server. */
	packet_start(SSH_CMSG_SESSION_KEY);
	packet_put_char(options.cipher);

	/* Send the cookie back to the server. */
	for (i = 0; i < 8; i++)
		packet_put_char(cookie[i]);

	/* Send and destroy the encrypted encryption key integer. */
	packet_put_bignum(key);
	BN_clear_free(key);

	/* Send protocol flags. */
	packet_put_int(client_flags);

	/* Send the packet now. */
	packet_send();
	packet_write_wait();

	debug("Sent encrypted session key.");

	/* Set the encryption key. */
	packet_set_encryption_key(session_key, SSH_SESSION_KEY_LENGTH, options.cipher);

	/* We will no longer need the session key here.  Destroy any extra copies. */
	memset(session_key, 0, sizeof(session_key));

	/*
	 * Expect a success message from the server.  Note that this message
	 * will be received in encrypted form.
	 */
	packet_read_expect(SSH_SMSG_SUCCESS);

	debug("Received encrypted confirmation.");
}

/*
 * Authenticate user
 */
void
ssh_userauth1(const char *local_user, const char *server_user, char *host,
    Sensitive *sensitive)
{
	int i, type;

	if (supported_authentications == 0)
		fatal("ssh_userauth1: server supports no auth methods");

	/* Send the name of the user to log in as on the server. */
	packet_start(SSH_CMSG_USER);
	packet_put_cstring(server_user);
	packet_send();
	packet_write_wait();

	/*
	 * The server should respond with success if no authentication is
	 * needed (the user has no password).  Otherwise the server responds
	 * with failure.
	 */
	type = packet_read();

	/* check whether the connection was accepted without authentication. */
	if (type == SSH_SMSG_SUCCESS)
		goto success;
	if (type != SSH_SMSG_FAILURE)
		packet_disconnect("Protocol error: got %d in response to SSH_CMSG_USER", type);

	/*
	 * Try .rhosts or /etc/hosts.equiv authentication with RSA host
	 * authentication.
	 */
	if ((supported_authentications & (1 << SSH_AUTH_RHOSTS_RSA)) &&
	    options.rhosts_rsa_authentication) {
		for (i = 0; i < sensitive->nkeys; i++) {
			if (sensitive->keys[i] != NULL &&
			    sensitive->keys[i]->type == KEY_RSA1 &&
			    try_rhosts_rsa_authentication(local_user,
			    sensitive->keys[i]))
				goto success;
		}
	}
	/* Try RSA authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_RSA)) &&
	    options.rsa_authentication) {
		/*
		 * Try RSA authentication using the authentication agent. The
		 * agent is tried first because no passphrase is needed for
		 * it, whereas identity files may require passphrases.
		 */
		if (try_agent_authentication())
			goto success;

		/* Try RSA authentication for each identity. */
		for (i = 0; i < options.num_identity_files; i++)
			if (options.identity_keys[i] != NULL &&
			    options.identity_keys[i]->type == KEY_RSA1 &&
			    try_rsa_authentication(i))
				goto success;
	}
	/* Try challenge response authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_TIS)) &&
	    options.challenge_response_authentication && !options.batch_mode) {
		if (try_challenge_response_authentication())
			goto success;
	}
	/* Try password authentication if the server supports it. */
	if ((supported_authentications & (1 << SSH_AUTH_PASSWORD)) &&
	    options.password_authentication && !options.batch_mode) {
		char prompt[80];

		snprintf(prompt, sizeof(prompt), "%.30s@%.128s's password: ",
		    server_user, host);
		if (try_password_authentication(prompt))
			goto success;
	}
	/* All authentication methods have failed.  Exit with an error message. */
	fatal("Permission denied.");
	/* NOTREACHED */

 success:
	return;	/* need statement after label */
}
