/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * RSA-based authentication.  This code determines whether to admit a login
 * based on RSA authentication.  This file also contains functions to check
 * validity of the host key.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: auth-rsa.c,v 1.50 2001/12/28 14:50:54 markus Exp $");
RCSID("$FreeBSD$");

#include <openssl/rsa.h>
#include <openssl/md5.h>

#include "rsa.h"
#include "packet.h"
#include "xmalloc.h"
#include "ssh1.h"
#include "mpaux.h"
#include "uidswap.h"
#include "match.h"
#include "auth-options.h"
#include "pathnames.h"
#include "log.h"
#include "servconf.h"
#include "auth.h"
#include "hostfile.h"

/* import */
extern ServerOptions options;

/*
 * Session identifier that is used to bind key exchange and authentication
 * responses to a particular session.
 */
extern u_char session_id[16];

/*
 * The .ssh/authorized_keys file contains public keys, one per line, in the
 * following format:
 *   options bits e n comment
 * where bits, e and n are decimal numbers,
 * and comment is any string of characters up to newline.  The maximum
 * length of a line is 8000 characters.  See the documentation for a
 * description of the options.
 */

/*
 * Performs the RSA authentication challenge-response dialog with the client,
 * and returns true (non-zero) if the client gave the correct answer to
 * our challenge; returns zero if the client gives a wrong answer.
 */

int
auth_rsa_challenge_dialog(RSA *pk)
{
	BIGNUM *challenge, *encrypted_challenge;
	BN_CTX *ctx;
	u_char buf[32], mdbuf[16], response[16];
	MD5_CTX md;
	u_int i;
	int len;

	if ((encrypted_challenge = BN_new()) == NULL)
		fatal("auth_rsa_challenge_dialog: BN_new() failed");
	if ((challenge = BN_new()) == NULL)
		fatal("auth_rsa_challenge_dialog: BN_new() failed");

	/* Generate a random challenge. */
	BN_rand(challenge, 256, 0, 0);
	if ((ctx = BN_CTX_new()) == NULL)
		fatal("auth_rsa_challenge_dialog: BN_CTX_new() failed");
	BN_mod(challenge, challenge, pk->n, ctx);
	BN_CTX_free(ctx);

	/* Encrypt the challenge with the public key. */
	rsa_public_encrypt(encrypted_challenge, challenge, pk);

	/* Send the encrypted challenge to the client. */
	packet_start(SSH_SMSG_AUTH_RSA_CHALLENGE);
	packet_put_bignum(encrypted_challenge);
	packet_send();
	BN_clear_free(encrypted_challenge);
	packet_write_wait();

	/* Wait for a response. */
	packet_read_expect(SSH_CMSG_AUTH_RSA_RESPONSE);
	for (i = 0; i < 16; i++)
		response[i] = packet_get_char();
	packet_check_eom();

	/* The response is MD5 of decrypted challenge plus session id. */
	len = BN_num_bytes(challenge);
	if (len <= 0 || len > 32)
		fatal("auth_rsa_challenge_dialog: bad challenge length %d", len);
	memset(buf, 0, 32);
	BN_bn2bin(challenge, buf + 32 - len);
	MD5_Init(&md);
	MD5_Update(&md, buf, 32);
	MD5_Update(&md, session_id, 16);
	MD5_Final(mdbuf, &md);
	BN_clear_free(challenge);

	/* Verify that the response is the original challenge. */
	if (memcmp(response, mdbuf, 16) != 0) {
		/* Wrong answer. */
		return 0;
	}
	/* Correct answer. */
	return 1;
}

/*
 * Performs the RSA authentication dialog with the client.  This returns
 * 0 if the client could not be authenticated, and 1 if authentication was
 * successful.  This may exit if there is a serious protocol violation.
 */

int
auth_rsa(struct passwd *pw, BIGNUM *client_n)
{
	char line[8192], *file;
	int authenticated;
	u_int bits;
	FILE *f;
	u_long linenum = 0;
	struct stat st;
	Key *key;
	char *fp;

	/* no user given */
	if (pw == NULL)
		return 0;

	/* Temporarily use the user's uid. */
	temporarily_use_uid(pw);

	/* The authorized keys. */
	file = authorized_keys_file(pw);
	debug("trying public RSA key file %s", file);

	/* Fail quietly if file does not exist */
	if (stat(file, &st) < 0) {
		/* Restore the privileged uid. */
		restore_uid();
		xfree(file);
		return 0;
	}
	/* Open the file containing the authorized keys. */
	f = fopen(file, "r");
	if (!f) {
		/* Restore the privileged uid. */
		restore_uid();
		packet_send_debug("Could not open %.900s for reading.", file);
		packet_send_debug("If your home is on an NFS volume, it may need to be world-readable.");
		xfree(file);
		return 0;
	}
	if (options.strict_modes &&
	    secure_filename(f, file, pw, line, sizeof(line)) != 0) {
		xfree(file);
		fclose(f);
		log("Authentication refused: %s", line);
		packet_send_debug("Authentication refused: %s", line);
		restore_uid();
		return 0;
	}
	/* Flag indicating whether authentication has succeeded. */
	authenticated = 0;

	key = key_new(KEY_RSA1);

	/*
	 * Go though the accepted keys, looking for the current key.  If
	 * found, perform a challenge-response dialog to verify that the
	 * user really has the corresponding private key.
	 */
	while (fgets(line, sizeof(line), f)) {
		char *cp;
		char *options;

		linenum++;

		/* Skip leading whitespace, empty and comment lines. */
		for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
			;
		if (!*cp || *cp == '\n' || *cp == '#')
			continue;

		/*
		 * Check if there are options for this key, and if so,
		 * save their starting address and skip the option part
		 * for now.  If there are no options, set the starting
		 * address to NULL.
		 */
		if (*cp < '0' || *cp > '9') {
			int quoted = 0;
			options = cp;
			for (; *cp && (quoted || (*cp != ' ' && *cp != '\t')); cp++) {
				if (*cp == '\\' && cp[1] == '"')
					cp++;	/* Skip both */
				else if (*cp == '"')
					quoted = !quoted;
			}
		} else
			options = NULL;

		/* Parse the key from the line. */
		if (hostfile_read_key(&cp, &bits, key) == 0) {
			debug("%.100s, line %lu: non ssh1 key syntax",
			    file, linenum);
			continue;
		}
		/* cp now points to the comment part. */

		/* Check if the we have found the desired key (identified by its modulus). */
		if (BN_cmp(key->rsa->n, client_n) != 0)
			continue;

		/* check the real bits  */
		if (bits != BN_num_bits(key->rsa->n))
			log("Warning: %s, line %lu: keysize mismatch: "
			    "actual %d vs. announced %d.",
			    file, linenum, BN_num_bits(key->rsa->n), bits);

		/* We have found the desired key. */
		/*
		 * If our options do not allow this key to be used,
		 * do not send challenge.
		 */
		if (!auth_parse_options(pw, options, file, linenum))
			continue;

		/* Perform the challenge-response dialog for this key. */
		if (!auth_rsa_challenge_dialog(key->rsa)) {
			/* Wrong response. */
			verbose("Wrong response to RSA authentication challenge.");
			packet_send_debug("Wrong response to RSA authentication challenge.");
			/*
			 * Break out of the loop. Otherwise we might send
			 * another challenge and break the protocol.
			 */
			break;
		}
		/*
		 * Correct response.  The client has been successfully
		 * authenticated. Note that we have not yet processed the
		 * options; this will be reset if the options cause the
		 * authentication to be rejected.
		 * Break out of the loop if authentication was successful;
		 * otherwise continue searching.
		 */
		authenticated = 1;

	 	fp = key_fingerprint(key, SSH_FP_MD5, SSH_FP_HEX);
		verbose("Found matching %s key: %s",
		    key_type(key), fp);
		xfree(fp);

		break;
	}

	/* Restore the privileged uid. */
	restore_uid();

	/* Close the file. */
	xfree(file);
	fclose(f);

	key_free(key);

	if (authenticated)
		packet_send_debug("RSA authentication accepted.");
	else
		auth_clear_options();

	/* Return authentication result. */
	return authenticated;
}
