/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1994 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Identity and host key generation and maintenance.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: ssh-keygen.c,v 1.31 2000/09/07 20:27:54 deraadt Exp $");

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>

#include "ssh.h"
#include "xmalloc.h"
#include "key.h"
#include "rsa.h"
#include "dsa.h"
#include "authfile.h"
#include "uuencode.h"

/* Number of bits in the RSA/DSA key.  This value can be changed on the command line. */
int bits = 1024;

/*
 * Flag indicating that we just want to change the passphrase.  This can be
 * set on the command line.
 */
int change_passphrase = 0;

/*
 * Flag indicating that we just want to change the comment.  This can be set
 * on the command line.
 */
int change_comment = 0;

int quiet = 0;

/* Flag indicating that we just want to see the key fingerprint */
int print_fingerprint = 0;

/* The identity file name, given on the command line or entered by the user. */
char identity_file[1024];
int have_identity = 0;

/* This is set to the passphrase if given on the command line. */
char *identity_passphrase = NULL;

/* This is set to the new passphrase if given on the command line. */
char *identity_new_passphrase = NULL;

/* This is set to the new comment if given on the command line. */
char *identity_comment = NULL;

/* Dump public key file in format used by real and the original SSH 2 */
int convert_to_ssh2 = 0;
int convert_from_ssh2 = 0;
int print_public = 0;
int dsa_mode = 0;

/* argv0 */
extern char *__progname;

char hostname[MAXHOSTNAMELEN];

void
ask_filename(struct passwd *pw, const char *prompt)
{
	char buf[1024];
	snprintf(identity_file, sizeof(identity_file), "%s/%s",
	    pw->pw_dir,
	    dsa_mode ? SSH_CLIENT_ID_DSA: SSH_CLIENT_IDENTITY);
	printf("%s (%s): ", prompt, identity_file);
	fflush(stdout);
	if (fgets(buf, sizeof(buf), stdin) == NULL)
		exit(1);
	if (strchr(buf, '\n'))
		*strchr(buf, '\n') = 0;
	if (strcmp(buf, "") != 0)
		strlcpy(identity_file, buf, sizeof(identity_file));
	have_identity = 1;
}

int
try_load_key(char *filename, Key *k)
{
	int success = 1;
	if (!load_private_key(filename, "", k, NULL)) {
		char *pass = read_passphrase("Enter passphrase: ", 1);
		if (!load_private_key(filename, pass, k, NULL)) {
			success = 0;
		}
		memset(pass, 0, strlen(pass));
		xfree(pass);
	}
	return success;
}

#define SSH_COM_MAGIC_BEGIN "---- BEGIN SSH2 PUBLIC KEY ----"
#define SSH_COM_MAGIC_END   "---- END SSH2 PUBLIC KEY ----"

void
do_convert_to_ssh2(struct passwd *pw)
{
	Key *k;
	int len;
	unsigned char *blob;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	k = key_new(KEY_DSA);
	if (!try_load_key(identity_file, k)) {
		fprintf(stderr, "load failed\n");
		exit(1);
	}
	dsa_make_key_blob(k, &blob, &len);
	fprintf(stdout, "%s\n", SSH_COM_MAGIC_BEGIN);
	fprintf(stdout,
	    "Comment: \"%d-bit DSA, converted from openssh by %s@%s\"\n",
	    BN_num_bits(k->dsa->p),
	    pw->pw_name, hostname);
	dump_base64(stdout, blob, len);
	fprintf(stdout, "%s\n", SSH_COM_MAGIC_END);
	key_free(k);
	xfree(blob);
	exit(0);
}

void
do_convert_from_ssh2(struct passwd *pw)
{
	Key *k;
	int blen;
	char line[1024], *p;
	char blob[8096];
	char encoded[8096];
	struct stat st;
	int escaped = 0;
	FILE *fp;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	fp = fopen(identity_file, "r");
	if (fp == NULL) {
		perror(identity_file);
		exit(1);
	}
	encoded[0] = '\0';
	while (fgets(line, sizeof(line), fp)) {
		if (!(p = strchr(line, '\n'))) {
			fprintf(stderr, "input line too long.\n");
			exit(1);
		}
		if (p > line && p[-1] == '\\')
			escaped++;
		if (strncmp(line, "----", 4) == 0 ||
		    strstr(line, ": ") != NULL) {
			fprintf(stderr, "ignore: %s", line);
			continue;
		}
		if (escaped) {
			escaped--;
			fprintf(stderr, "escaped: %s", line);
			continue;
		}
		*p = '\0';
		strlcat(encoded, line, sizeof(encoded));
	}
	blen = uudecode(encoded, (unsigned char *)blob, sizeof(blob));
	if (blen < 0) {
		fprintf(stderr, "uudecode failed.\n");
		exit(1);
	}
	k = dsa_key_from_blob(blob, blen);
	if (!key_write(k, stdout))
		fprintf(stderr, "key_write failed");
	key_free(k);
	fprintf(stdout, "\n");
	fclose(fp);
	exit(0);
}

void
do_print_public(struct passwd *pw)
{
	Key *k;
	int len;
	unsigned char *blob;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	k = key_new(KEY_DSA);
	if (!try_load_key(identity_file, k)) {
		fprintf(stderr, "load failed\n");
		exit(1);
	}
	dsa_make_key_blob(k, &blob, &len);
	if (!key_write(k, stdout))
		fprintf(stderr, "key_write failed");
	key_free(k);
	xfree(blob);
	fprintf(stdout, "\n");
	exit(0);
}

void
do_fingerprint(struct passwd *pw)
{
	/* XXX RSA1 only */

	FILE *f;
	Key *public;
	char *comment = NULL, *cp, *ep, line[16*1024];
	int i, skip = 0, num = 1, invalid = 1;
	unsigned int ignore;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	public = key_new(KEY_RSA);
	if (load_public_key(identity_file, public, &comment)) {
		printf("%d %s %s\n", BN_num_bits(public->rsa->n),
		    key_fingerprint(public), comment);
		key_free(public);
		exit(0);
	}

	f = fopen(identity_file, "r");
	if (f != NULL) {
		while (fgets(line, sizeof(line), f)) {
			i = strlen(line) - 1;
			if (line[i] != '\n') {
				error("line %d too long: %.40s...", num, line);
				skip = 1;
				continue;
			}
			num++;
			if (skip) {
				skip = 0;
				continue;
			}
			line[i] = '\0';

			/* Skip leading whitespace, empty and comment lines. */
			for (cp = line; *cp == ' ' || *cp == '\t'; cp++)
				;
			if (!*cp || *cp == '\n' || *cp == '#')
				continue ;
			i = strtol(cp, &ep, 10);
			if (i == 0 || ep == NULL || (*ep != ' ' && *ep != '\t')) {
				int quoted = 0;
				comment = cp;
				for (; *cp && (quoted || (*cp != ' ' && *cp != '\t')); cp++) {
					if (*cp == '\\' && cp[1] == '"')
						cp++;	/* Skip both */
					else if (*cp == '"')
						quoted = !quoted;
				}
				if (!*cp)
					continue;
				*cp++ = '\0';
			}
			ep = cp;
			if (auth_rsa_read_key(&cp, &ignore, public->rsa->e, public->rsa->n)) {
				invalid = 0;
				comment = *cp ? cp : comment;
				printf("%d %s %s\n", key_size(public),
				    key_fingerprint(public),
				    comment ? comment : "no comment");
			}
		}
		fclose(f);
	}
	key_free(public);
	if (invalid) {
		printf("%s is not a valid key file.\n", identity_file);
		exit(1);
	}
	exit(0);
}

/*
 * Perform changing a passphrase.  The argument is the passwd structure
 * for the current user.
 */
void
do_change_passphrase(struct passwd *pw)
{
	char *comment;
	char *old_passphrase, *passphrase1, *passphrase2;
	struct stat st;
	Key *private;
	Key *public;
	int type = dsa_mode ? KEY_DSA : KEY_RSA;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}

	if (type == KEY_RSA) {
		/* XXX this works currently only for RSA */
		public = key_new(type);
		if (!load_public_key(identity_file, public, NULL)) {
			printf("%s is not a valid key file.\n", identity_file);
			exit(1);
		}
		/* Clear the public key since we are just about to load the whole file. */
		key_free(public);
	}

	/* Try to load the file with empty passphrase. */
	private = key_new(type);
	if (!load_private_key(identity_file, "", private, &comment)) {
		if (identity_passphrase)
			old_passphrase = xstrdup(identity_passphrase);
		else
			old_passphrase = read_passphrase("Enter old passphrase: ", 1);
		if (!load_private_key(identity_file, old_passphrase, private, &comment)) {
			memset(old_passphrase, 0, strlen(old_passphrase));
			xfree(old_passphrase);
			printf("Bad passphrase.\n");
			exit(1);
		}
		memset(old_passphrase, 0, strlen(old_passphrase));
		xfree(old_passphrase);
	}
	printf("Key has comment '%s'\n", comment);

	/* Ask the new passphrase (twice). */
	if (identity_new_passphrase) {
		passphrase1 = xstrdup(identity_new_passphrase);
		passphrase2 = NULL;
	} else {
		passphrase1 =
			read_passphrase("Enter new passphrase (empty for no passphrase): ", 1);
		passphrase2 = read_passphrase("Enter same passphrase again: ", 1);

		/* Verify that they are the same. */
		if (strcmp(passphrase1, passphrase2) != 0) {
			memset(passphrase1, 0, strlen(passphrase1));
			memset(passphrase2, 0, strlen(passphrase2));
			xfree(passphrase1);
			xfree(passphrase2);
			printf("Pass phrases do not match.  Try again.\n");
			exit(1);
		}
		/* Destroy the other copy. */
		memset(passphrase2, 0, strlen(passphrase2));
		xfree(passphrase2);
	}

	/* Save the file using the new passphrase. */
	if (!save_private_key(identity_file, passphrase1, private, comment)) {
		printf("Saving the key failed: %s: %s.\n",
		       identity_file, strerror(errno));
		memset(passphrase1, 0, strlen(passphrase1));
		xfree(passphrase1);
		key_free(private);
		xfree(comment);
		exit(1);
	}
	/* Destroy the passphrase and the copy of the key in memory. */
	memset(passphrase1, 0, strlen(passphrase1));
	xfree(passphrase1);
	key_free(private);		 /* Destroys contents */
	xfree(comment);

	printf("Your identification has been saved with the new passphrase.\n");
	exit(0);
}

/*
 * Change the comment of a private key file.
 */
void
do_change_comment(struct passwd *pw)
{
	char new_comment[1024], *comment;
	Key *private;
	Key *public;
	char *passphrase;
	struct stat st;
	FILE *f;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	/*
	 * Try to load the public key from the file the verify that it is
	 * readable and of the proper format.
	 */
	public = key_new(KEY_RSA);
	if (!load_public_key(identity_file, public, NULL)) {
		printf("%s is not a valid key file.\n", identity_file);
		exit(1);
	}

	private = key_new(KEY_RSA);
	if (load_private_key(identity_file, "", private, &comment))
		passphrase = xstrdup("");
	else {
		if (identity_passphrase)
			passphrase = xstrdup(identity_passphrase);
		else if (identity_new_passphrase)
			passphrase = xstrdup(identity_new_passphrase);
		else
			passphrase = read_passphrase("Enter passphrase: ", 1);
		/* Try to load using the passphrase. */
		if (!load_private_key(identity_file, passphrase, private, &comment)) {
			memset(passphrase, 0, strlen(passphrase));
			xfree(passphrase);
			printf("Bad passphrase.\n");
			exit(1);
		}
	}
	printf("Key now has comment '%s'\n", comment);

	if (identity_comment) {
		strlcpy(new_comment, identity_comment, sizeof(new_comment));
	} else {
		printf("Enter new comment: ");
		fflush(stdout);
		if (!fgets(new_comment, sizeof(new_comment), stdin)) {
			memset(passphrase, 0, strlen(passphrase));
			key_free(private);
			exit(1);
		}
		if (strchr(new_comment, '\n'))
			*strchr(new_comment, '\n') = 0;
	}

	/* Save the file using the new passphrase. */
	if (!save_private_key(identity_file, passphrase, private, new_comment)) {
		printf("Saving the key failed: %s: %s.\n",
		       identity_file, strerror(errno));
		memset(passphrase, 0, strlen(passphrase));
		xfree(passphrase);
		key_free(private);
		xfree(comment);
		exit(1);
	}
	memset(passphrase, 0, strlen(passphrase));
	xfree(passphrase);
	key_free(private);

	strlcat(identity_file, ".pub", sizeof(identity_file));
	f = fopen(identity_file, "w");
	if (!f) {
		printf("Could not save your public key in %s\n", identity_file);
		exit(1);
	}
	if (!key_write(public, f))
		fprintf(stderr, "write key failed");
	key_free(public);
	fprintf(f, " %s\n", new_comment);
	fclose(f);

	xfree(comment);

	printf("The comment in your key file has been changed.\n");
	exit(0);
}

void
usage(void)
{
	printf("Usage: %s [-lpqxXydc] [-b bits] [-f file] [-C comment] [-N new-pass] [-P pass]\n", __progname);
	exit(1);
}

/*
 * Main program for key management.
 */
int
main(int ac, char **av)
{
	char dotsshdir[16 * 1024], comment[1024], *passphrase1, *passphrase2;
	struct passwd *pw;
	int opt;
	struct stat st;
	FILE *f;
	Key *private;
	Key *public;
	extern int optind;
	extern char *optarg;

	SSLeay_add_all_algorithms();

	/* we need this for the home * directory.  */
	pw = getpwuid(getuid());
	if (!pw) {
		printf("You don't exist, go away!\n");
		exit(1);
	}
	if (gethostname(hostname, sizeof(hostname)) < 0) {
		perror("gethostname");
		exit(1);
	}

	while ((opt = getopt(ac, av, "dqpclRxXyb:f:P:N:C:")) != EOF) {
		switch (opt) {
		case 'b':
			bits = atoi(optarg);
			if (bits < 512 || bits > 32768) {
				printf("Bits has bad value.\n");
				exit(1);
			}
			break;

		case 'l':
			print_fingerprint = 1;
			break;

		case 'p':
			change_passphrase = 1;
			break;

		case 'c':
			change_comment = 1;
			break;

		case 'f':
			strlcpy(identity_file, optarg, sizeof(identity_file));
			have_identity = 1;
			break;

		case 'P':
			identity_passphrase = optarg;
			break;

		case 'N':
			identity_new_passphrase = optarg;
			break;

		case 'C':
			identity_comment = optarg;
			break;

		case 'q':
			quiet = 1;
			break;

		case 'R':
			if (rsa_alive() == 0)
				exit(1);
			else
				exit(0);
			break;

		case 'x':
			convert_to_ssh2 = 1;
			break;

		case 'X':
			convert_from_ssh2 = 1;
			break;

		case 'y':
			print_public = 1;
			break;

		case 'd':
			dsa_mode = 1;
			break;

		case '?':
		default:
			usage();
		}
	}
	if (optind < ac) {
		printf("Too many arguments.\n");
		usage();
	}
	if (change_passphrase && change_comment) {
		printf("Can only have one of -p and -c.\n");
		usage();
	}
	/* check if RSA support is needed and exists */
	if (dsa_mode == 0 && rsa_alive() == 0) {
		fprintf(stderr,
			"%s: no RSA support in libssl and libcrypto.  See ssl(8).\n",
			__progname);
		exit(1);
	}
	if (print_fingerprint)
		do_fingerprint(pw);
	if (change_passphrase)
		do_change_passphrase(pw);
	if (change_comment)
		do_change_comment(pw);
	if (convert_to_ssh2)
		do_convert_to_ssh2(pw);
	if (convert_from_ssh2)
		do_convert_from_ssh2(pw);
	if (print_public)
		do_print_public(pw);

	arc4random_stir();

	if (dsa_mode != 0) {
		if (!quiet)
			printf("Generating DSA parameter and key.\n");
		public = private = dsa_generate_key(bits);
		if (private == NULL) {
			fprintf(stderr, "dsa_generate_keys failed");
			exit(1);
		}
	} else {
		if (quiet)
			rsa_set_verbose(0);
		/* Generate the rsa key pair. */
		public = key_new(KEY_RSA);
		private = key_new(KEY_RSA);
		rsa_generate_key(private->rsa, public->rsa, bits);
	}

	if (!have_identity)
		ask_filename(pw, "Enter file in which to save the key");

	/* Create ~/.ssh directory if it doesn\'t already exist. */
	snprintf(dotsshdir, sizeof dotsshdir, "%s/%s", pw->pw_dir, SSH_USER_DIR);
	if (strstr(identity_file, dotsshdir) != NULL &&
	    stat(dotsshdir, &st) < 0) {
		if (mkdir(dotsshdir, 0700) < 0)
			error("Could not create directory '%s'.", dotsshdir);
		else if (!quiet)
			printf("Created directory '%s'.\n", dotsshdir);
	}
	/* If the file already exists, ask the user to confirm. */
	if (stat(identity_file, &st) >= 0) {
		char yesno[3];
		printf("%s already exists.\n", identity_file);
		printf("Overwrite (y/n)? ");
		fflush(stdout);
		if (fgets(yesno, sizeof(yesno), stdin) == NULL)
			exit(1);
		if (yesno[0] != 'y' && yesno[0] != 'Y')
			exit(1);
	}
	/* Ask for a passphrase (twice). */
	if (identity_passphrase)
		passphrase1 = xstrdup(identity_passphrase);
	else if (identity_new_passphrase)
		passphrase1 = xstrdup(identity_new_passphrase);
	else {
passphrase_again:
		passphrase1 =
			read_passphrase("Enter passphrase (empty for no passphrase): ", 1);
		passphrase2 = read_passphrase("Enter same passphrase again: ", 1);
		if (strcmp(passphrase1, passphrase2) != 0) {
			/* The passphrases do not match.  Clear them and retry. */
			memset(passphrase1, 0, strlen(passphrase1));
			memset(passphrase2, 0, strlen(passphrase2));
			xfree(passphrase1);
			xfree(passphrase2);
			printf("Passphrases do not match.  Try again.\n");
			goto passphrase_again;
		}
		/* Clear the other copy of the passphrase. */
		memset(passphrase2, 0, strlen(passphrase2));
		xfree(passphrase2);
	}

	if (identity_comment) {
		strlcpy(comment, identity_comment, sizeof(comment));
	} else {
		/* Create default commend field for the passphrase. */
		snprintf(comment, sizeof comment, "%s@%s", pw->pw_name, hostname);
	}

	/* Save the key with the given passphrase and comment. */
	if (!save_private_key(identity_file, passphrase1, private, comment)) {
		printf("Saving the key failed: %s: %s.\n",
		    identity_file, strerror(errno));
		memset(passphrase1, 0, strlen(passphrase1));
		xfree(passphrase1);
		exit(1);
	}
	/* Clear the passphrase. */
	memset(passphrase1, 0, strlen(passphrase1));
	xfree(passphrase1);

	/* Clear the private key and the random number generator. */
	if (private != public) {
		key_free(private);
	}
	arc4random_stir();

	if (!quiet)
		printf("Your identification has been saved in %s.\n", identity_file);

	strlcat(identity_file, ".pub", sizeof(identity_file));
	f = fopen(identity_file, "w");
	if (!f) {
		printf("Could not save your public key in %s\n", identity_file);
		exit(1);
	}
	if (!key_write(public, f))
		fprintf(stderr, "write key failed");
	fprintf(f, " %s\n", comment);
	fclose(f);

	if (!quiet) {
		printf("Your public key has been saved in %s.\n",
		    identity_file);
		printf("The key fingerprint is:\n");
		printf("%s %s\n", key_fingerprint(public), comment);
	}

	key_free(public);
	exit(0);
}
