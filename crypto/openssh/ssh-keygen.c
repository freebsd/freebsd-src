/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1994 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Mon Mar 27 02:26:40 1995 ylo
 * Identity and host key generation and maintenance.
 */

#include "includes.h"
RCSID("$Id: ssh-keygen.c,v 1.16 2000/02/04 14:34:09 markus Exp $");

#include "rsa.h"
#include "ssh.h"
#include "xmalloc.h"
#include "fingerprint.h"

/* Generated private key. */
RSA *private_key;

/* Generated public key. */
RSA *public_key;

/* Number of bits in the RSA key.  This value can be changed on the command line. */
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

/* argv0 */
extern char *__progname;

void
ask_filename(struct passwd *pw, const char *prompt)
{
	char buf[1024];
	snprintf(identity_file, sizeof(identity_file), "%s/%s",
		 pw->pw_dir, SSH_CLIENT_IDENTITY);
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

void
do_fingerprint(struct passwd *pw)
{
	FILE *f;
	BIGNUM *e, *n;
	RSA *public_key;
	char *comment = NULL, *cp, *ep, line[16*1024];
	int i, skip = 0, num = 1, invalid = 1;
	struct stat st;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	
	public_key = RSA_new();
	if (load_public_key(identity_file, public_key, &comment)) {
		printf("%d %s %s\n", BN_num_bits(public_key->n),
		    fingerprint(public_key->e, public_key->n),
		    comment);
		RSA_free(public_key);
		exit(0);
	}
	RSA_free(public_key);

	f = fopen(identity_file, "r");
	if (f != NULL) {
		n = BN_new();
		e = BN_new();
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
			if (auth_rsa_read_key(&cp, &i, e, n)) {
				invalid = 0;
				comment = *cp ? cp : comment;
				printf("%d %s %s\n", BN_num_bits(n),
				    fingerprint(e, n),
				    comment ? comment : "no comment");
			}
		}
		BN_free(e);
		BN_free(n);
		fclose(f);
	}
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
	RSA *private_key;

	if (!have_identity)
		ask_filename(pw, "Enter file in which the key is");
	if (stat(identity_file, &st) < 0) {
		perror(identity_file);
		exit(1);
	}
	public_key = RSA_new();
	if (!load_public_key(identity_file, public_key, NULL)) {
		printf("%s is not a valid key file.\n", identity_file);
		exit(1);
	}
	/* Clear the public key since we are just about to load the whole file. */
	RSA_free(public_key);

	/* Try to load the file with empty passphrase. */
	private_key = RSA_new();
	if (!load_private_key(identity_file, "", private_key, &comment)) {
		if (identity_passphrase)
			old_passphrase = xstrdup(identity_passphrase);
		else
			old_passphrase = read_passphrase("Enter old passphrase: ", 1);
		if (!load_private_key(identity_file, old_passphrase, private_key, &comment)) {
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
	if (!save_private_key(identity_file, passphrase1, private_key, comment)) {
		printf("Saving the key failed: %s: %s.\n",
		       identity_file, strerror(errno));
		memset(passphrase1, 0, strlen(passphrase1));
		xfree(passphrase1);
		RSA_free(private_key);
		xfree(comment);
		exit(1);
	}
	/* Destroy the passphrase and the copy of the key in memory. */
	memset(passphrase1, 0, strlen(passphrase1));
	xfree(passphrase1);
	RSA_free(private_key);	/* Destroys contents */
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
	RSA *private_key;
	char *passphrase;
	struct stat st;
	FILE *f;
	char *tmpbuf;

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
	public_key = RSA_new();
	if (!load_public_key(identity_file, public_key, NULL)) {
		printf("%s is not a valid key file.\n", identity_file);
		exit(1);
	}
	private_key = RSA_new();

	if (load_private_key(identity_file, "", private_key, &comment))
		passphrase = xstrdup("");
	else {
		if (identity_passphrase)
			passphrase = xstrdup(identity_passphrase);
		else if (identity_new_passphrase)
			passphrase = xstrdup(identity_new_passphrase);
		else
			passphrase = read_passphrase("Enter passphrase: ", 1);
		/* Try to load using the passphrase. */
		if (!load_private_key(identity_file, passphrase, private_key, &comment)) {
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
			RSA_free(private_key);
			exit(1);
		}
		if (strchr(new_comment, '\n'))
			*strchr(new_comment, '\n') = 0;
	}

	/* Save the file using the new passphrase. */
	if (!save_private_key(identity_file, passphrase, private_key, new_comment)) {
		printf("Saving the key failed: %s: %s.\n",
		       identity_file, strerror(errno));
		memset(passphrase, 0, strlen(passphrase));
		xfree(passphrase);
		RSA_free(private_key);
		xfree(comment);
		exit(1);
	}
	memset(passphrase, 0, strlen(passphrase));
	xfree(passphrase);
	RSA_free(private_key);

	strlcat(identity_file, ".pub", sizeof(identity_file));
	f = fopen(identity_file, "w");
	if (!f) {
		printf("Could not save your public key in %s\n", identity_file);
		exit(1);
	}
	fprintf(f, "%d ", BN_num_bits(public_key->n));
	tmpbuf = BN_bn2dec(public_key->e);
	fprintf(f, "%s ", tmpbuf);
	free(tmpbuf);
	tmpbuf = BN_bn2dec(public_key->n);
	fprintf(f, "%s %s\n", tmpbuf, new_comment);
	free(tmpbuf);
	fclose(f);

	xfree(comment);

	printf("The comment in your key file has been changed.\n");
	exit(0);
}

void
usage(void)
{
	printf("ssh-keygen version %s\n", SSH_VERSION);
	printf("Usage: %s [-b bits] [-p] [-c] [-l] [-f file] [-P pass] [-N new-pass] [-C comment]\n", __progname);
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
	char *tmpbuf;
	int opt;
	struct stat st;
	FILE *f;
	char hostname[MAXHOSTNAMELEN];
	extern int optind;
	extern char *optarg;

	/* check if RSA support exists */
	if (rsa_alive() == 0) {
		fprintf(stderr,
			"%s: no RSA support in libssl and libcrypto.  See ssl(8).\n",
			__progname);
		exit(1);
	}
	/* we need this for the home * directory.  */
	pw = getpwuid(getuid());
	if (!pw) {
		printf("You don't exist, go away!\n");
		exit(1);
	}

	while ((opt = getopt(ac, av, "qpclb:f:P:N:C:")) != EOF) {
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
	if (print_fingerprint)
		do_fingerprint(pw);
	if (change_passphrase)
		do_change_passphrase(pw);
	if (change_comment)
		do_change_comment(pw);

	arc4random_stir();

	if (quiet)
		rsa_set_verbose(0);

	/* Generate the rsa key pair. */
	private_key = RSA_new();
	public_key = RSA_new();
	rsa_generate_key(private_key, public_key, bits);

	if (!have_identity)
		ask_filename(pw, "Enter file in which to save the key");

	/* Create ~/.ssh directory if it doesn\'t already exist. */
	snprintf(dotsshdir, sizeof dotsshdir, "%s/%s", pw->pw_dir, SSH_USER_DIR);
	if (strstr(identity_file, dotsshdir) != NULL &&
	    stat(dotsshdir, &st) < 0) {
		if (mkdir(dotsshdir, 0755) < 0)
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
		if (gethostname(hostname, sizeof(hostname)) < 0) {
			perror("gethostname");
			exit(1);
		}
		snprintf(comment, sizeof comment, "%s@%s", pw->pw_name, hostname);
	}

	/* Save the key with the given passphrase and comment. */
	if (!save_private_key(identity_file, passphrase1, private_key, comment)) {
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
	RSA_free(private_key);
	arc4random_stir();

	if (!quiet)
		printf("Your identification has been saved in %s.\n", identity_file);

	strlcat(identity_file, ".pub", sizeof(identity_file));
	f = fopen(identity_file, "w");
	if (!f) {
		printf("Could not save your public key in %s\n", identity_file);
		exit(1);
	}
	fprintf(f, "%d ", BN_num_bits(public_key->n));
	tmpbuf = BN_bn2dec(public_key->e);
	fprintf(f, "%s ", tmpbuf);
	free(tmpbuf);
	tmpbuf = BN_bn2dec(public_key->n);
	fprintf(f, "%s %s\n", tmpbuf, comment);
	free(tmpbuf);
	fclose(f);

	if (!quiet) {
		printf("Your public key has been saved in %s.\n", identity_file);
		printf("The key fingerprint is:\n");
		printf("%d %s %s\n", BN_num_bits(public_key->n),
		       fingerprint(public_key->e, public_key->n),
		       comment);
	}
	exit(0);
}
