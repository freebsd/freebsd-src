/*
 * 
 * auth-rsa.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Mon Mar 27 01:46:52 1995 ylo
 * 
 * RSA-based authentication.  This code determines whether to admit a login
 * based on RSA authentication.  This file also contains functions to check
 * validity of the host key.
 * 
 */

#include "includes.h"
RCSID("$Id: auth-rsa.c,v 1.18 2000/02/11 10:59:11 markus Exp $");

#include "rsa.h"
#include "packet.h"
#include "xmalloc.h"
#include "ssh.h"
#include "mpaux.h"
#include "uidswap.h"
#include "servconf.h"

#include <ssl/rsa.h>
#include <ssl/md5.h>

/* Flags that may be set in authorized_keys options. */
extern int no_port_forwarding_flag;
extern int no_agent_forwarding_flag;
extern int no_x11_forwarding_flag;
extern int no_pty_flag;
extern char *forced_command;
extern struct envstring *custom_environment;

/*
 * Session identifier that is used to bind key exchange and authentication
 * responses to a particular session.
 */
extern unsigned char session_id[16];

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
auth_rsa_challenge_dialog(BIGNUM *e, BIGNUM *n)
{
	BIGNUM *challenge, *encrypted_challenge;
	RSA *pk;
	BN_CTX *ctx;
	unsigned char buf[32], mdbuf[16], response[16];
	MD5_CTX md;
	unsigned int i;
	int plen, len;

	encrypted_challenge = BN_new();
	challenge = BN_new();

	/* Generate a random challenge. */
	BN_rand(challenge, 256, 0, 0);
	ctx = BN_CTX_new();
	BN_mod(challenge, challenge, n, ctx);
	BN_CTX_free(ctx);

	/* Create the public key data structure. */
	pk = RSA_new();
	pk->e = BN_new();
	BN_copy(pk->e, e);
	pk->n = BN_new();
	BN_copy(pk->n, n);

	/* Encrypt the challenge with the public key. */
	rsa_public_encrypt(encrypted_challenge, challenge, pk);
	RSA_free(pk);

	/* Send the encrypted challenge to the client. */
	packet_start(SSH_SMSG_AUTH_RSA_CHALLENGE);
	packet_put_bignum(encrypted_challenge);
	packet_send();
	BN_clear_free(encrypted_challenge);
	packet_write_wait();

	/* Wait for a response. */
	packet_read_expect(&plen, SSH_CMSG_AUTH_RSA_RESPONSE);
	packet_integrity_check(plen, 16, SSH_CMSG_AUTH_RSA_RESPONSE);
	for (i = 0; i < 16; i++)
		response[i] = packet_get_char();

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
	extern ServerOptions options;
	char line[8192], file[1024];
	int authenticated;
	unsigned int bits;
	FILE *f;
	unsigned long linenum = 0;
	struct stat st;
	BIGNUM *e, *n;

	/* Temporarily use the user's uid. */
	temporarily_use_uid(pw->pw_uid);

	/* The authorized keys. */
	snprintf(file, sizeof file, "%.500s/%.100s", pw->pw_dir,
		 SSH_USER_PERMITTED_KEYS);

	/* Fail quietly if file does not exist */
	if (stat(file, &st) < 0) {
		/* Restore the privileged uid. */
		restore_uid();
		return 0;
	}
	/* Open the file containing the authorized keys. */
	f = fopen(file, "r");
	if (!f) {
		/* Restore the privileged uid. */
		restore_uid();
		packet_send_debug("Could not open %.900s for reading.", file);
		packet_send_debug("If your home is on an NFS volume, it may need to be world-readable.");
		return 0;
	}
	if (options.strict_modes) {
		int fail = 0;
		char buf[1024];
		/* Check open file in order to avoid open/stat races */
		if (fstat(fileno(f), &st) < 0 ||
		    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		    (st.st_mode & 022) != 0) {
			snprintf(buf, sizeof buf, "RSA authentication refused for %.100s: "
				 "bad ownership or modes for '%s'.", pw->pw_name, file);
			fail = 1;
		} else {
			/* Check path to SSH_USER_PERMITTED_KEYS */
			int i;
			static const char *check[] = {
				"", SSH_USER_DIR, NULL
			};
			for (i = 0; check[i]; i++) {
				snprintf(line, sizeof line, "%.500s/%.100s", pw->pw_dir, check[i]);
				if (stat(line, &st) < 0 ||
				    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
				    (st.st_mode & 022) != 0) {
					snprintf(buf, sizeof buf, "RSA authentication refused for %.100s: "
						 "bad ownership or modes for '%s'.", pw->pw_name, line);
					fail = 1;
					break;
				}
			}
		}
		if (fail) {
			log(buf);
			packet_send_debug(buf);
			restore_uid();
			return 0;
		}
	}
	/* Flag indicating whether authentication has succeeded. */
	authenticated = 0;

	e = BN_new();
	n = BN_new();

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
		if (!auth_rsa_read_key(&cp, &bits, e, n)) {
			debug("%.100s, line %lu: bad key syntax",
			      SSH_USER_PERMITTED_KEYS, linenum);
			packet_send_debug("%.100s, line %lu: bad key syntax",
				          SSH_USER_PERMITTED_KEYS, linenum);
			continue;
		}
		/* cp now points to the comment part. */

		/* Check if the we have found the desired key (identified by its modulus). */
		if (BN_cmp(n, client_n) != 0)
			continue;

		/* check the real bits  */
		if (bits != BN_num_bits(n))
			log("Warning: %s, line %ld: keysize mismatch: "
			    "actual %d vs. announced %d.",
			    file, linenum, BN_num_bits(n), bits);

		/* We have found the desired key. */

		/* Perform the challenge-response dialog for this key. */
		if (!auth_rsa_challenge_dialog(e, n)) {
			/* Wrong response. */
			verbose("Wrong response to RSA authentication challenge.");
			packet_send_debug("Wrong response to RSA authentication challenge.");
			continue;
		}
		/*
		 * Correct response.  The client has been successfully
		 * authenticated. Note that we have not yet processed the
		 * options; this will be reset if the options cause the
		 * authentication to be rejected.
		 */
		authenticated = 1;

		/* RSA part of authentication was accepted.  Now process the options. */
		if (options) {
			while (*options && *options != ' ' && *options != '\t') {
				cp = "no-port-forwarding";
				if (strncmp(options, cp, strlen(cp)) == 0) {
					packet_send_debug("Port forwarding disabled.");
					no_port_forwarding_flag = 1;
					options += strlen(cp);
					goto next_option;
				}
				cp = "no-agent-forwarding";
				if (strncmp(options, cp, strlen(cp)) == 0) {
					packet_send_debug("Agent forwarding disabled.");
					no_agent_forwarding_flag = 1;
					options += strlen(cp);
					goto next_option;
				}
				cp = "no-X11-forwarding";
				if (strncmp(options, cp, strlen(cp)) == 0) {
					packet_send_debug("X11 forwarding disabled.");
					no_x11_forwarding_flag = 1;
					options += strlen(cp);
					goto next_option;
				}
				cp = "no-pty";
				if (strncmp(options, cp, strlen(cp)) == 0) {
					packet_send_debug("Pty allocation disabled.");
					no_pty_flag = 1;
					options += strlen(cp);
					goto next_option;
				}
				cp = "command=\"";
				if (strncmp(options, cp, strlen(cp)) == 0) {
					int i;
					options += strlen(cp);
					forced_command = xmalloc(strlen(options) + 1);
					i = 0;
					while (*options) {
						if (*options == '"')
							break;
						if (*options == '\\' && options[1] == '"') {
							options += 2;
							forced_command[i++] = '"';
							continue;
						}
						forced_command[i++] = *options++;
					}
					if (!*options) {
						debug("%.100s, line %lu: missing end quote",
						      SSH_USER_PERMITTED_KEYS, linenum);
						packet_send_debug("%.100s, line %lu: missing end quote",
								  SSH_USER_PERMITTED_KEYS, linenum);
						continue;
					}
					forced_command[i] = 0;
					packet_send_debug("Forced command: %.900s", forced_command);
					options++;
					goto next_option;
				}
				cp = "environment=\"";
				if (strncmp(options, cp, strlen(cp)) == 0) {
					int i;
					char *s;
					struct envstring *new_envstring;
					options += strlen(cp);
					s = xmalloc(strlen(options) + 1);
					i = 0;
					while (*options) {
						if (*options == '"')
							break;
						if (*options == '\\' && options[1] == '"') {
							options += 2;
							s[i++] = '"';
							continue;
						}
						s[i++] = *options++;
					}
					if (!*options) {
						debug("%.100s, line %lu: missing end quote",
						      SSH_USER_PERMITTED_KEYS, linenum);
						packet_send_debug("%.100s, line %lu: missing end quote",
								  SSH_USER_PERMITTED_KEYS, linenum);
						continue;
					}
					s[i] = 0;
					packet_send_debug("Adding to environment: %.900s", s);
					debug("Adding to environment: %.900s", s);
					options++;
					new_envstring = xmalloc(sizeof(struct envstring));
					new_envstring->s = s;
					new_envstring->next = custom_environment;
					custom_environment = new_envstring;
					goto next_option;
				}
				cp = "from=\"";
				if (strncmp(options, cp, strlen(cp)) == 0) {
					char *patterns = xmalloc(strlen(options) + 1);
					int i;
					options += strlen(cp);
					i = 0;
					while (*options) {
						if (*options == '"')
							break;
						if (*options == '\\' && options[1] == '"') {
							options += 2;
							patterns[i++] = '"';
							continue;
						}
						patterns[i++] = *options++;
					}
					if (!*options) {
						debug("%.100s, line %lu: missing end quote",
						      SSH_USER_PERMITTED_KEYS, linenum);
						packet_send_debug("%.100s, line %lu: missing end quote",
								  SSH_USER_PERMITTED_KEYS, linenum);
						continue;
					}
					patterns[i] = 0;
					options++;
					if (!match_hostname(get_canonical_hostname(), patterns,
						     strlen(patterns)) &&
					    !match_hostname(get_remote_ipaddr(), patterns,
						     strlen(patterns))) {
						log("RSA authentication tried for %.100s with correct key but not from a permitted host (host=%.200s, ip=%.200s).",
						    pw->pw_name, get_canonical_hostname(),
						    get_remote_ipaddr());
						packet_send_debug("Your host '%.200s' is not permitted to use this key for login.",
						get_canonical_hostname());
						xfree(patterns);
						/* key invalid for this host, reset flags */
						authenticated = 0;
						no_agent_forwarding_flag = 0;
						no_port_forwarding_flag = 0;
						no_pty_flag = 0;
						no_x11_forwarding_flag = 0;
						while (custom_environment) {
							struct envstring *ce = custom_environment;
							custom_environment = ce->next;
							xfree(ce->s);
							xfree(ce);
						}
						if (forced_command) {
							xfree(forced_command);
							forced_command = NULL;
						}
						break;
					}
					xfree(patterns);
					/* Host name matches. */
					goto next_option;
				}
		bad_option:
				log("Bad options in %.100s file, line %lu: %.50s",
				    SSH_USER_PERMITTED_KEYS, linenum, options);
				packet_send_debug("Bad options in %.100s file, line %lu: %.50s",
						  SSH_USER_PERMITTED_KEYS, linenum, options);
				authenticated = 0;
				break;

		next_option:
				/*
				 * Skip the comma, and move to the next option
				 * (or break out if there are no more).
				 */
				if (!*options)
					fatal("Bugs in auth-rsa.c option processing.");
				if (*options == ' ' || *options == '\t')
					break;		/* End of options. */
				if (*options != ',')
					goto bad_option;
				options++;
				/* Process the next option. */
				continue;
			}
		}
		/*
		 * Break out of the loop if authentication was successful;
		 * otherwise continue searching.
		 */
		if (authenticated)
			break;
	}

	/* Restore the privileged uid. */
	restore_uid();

	/* Close the file. */
	fclose(f);

	BN_clear_free(n);
	BN_clear_free(e);

	if (authenticated)
		packet_send_debug("RSA authentication accepted.");

	/* Return authentication result. */
	return authenticated;
}
