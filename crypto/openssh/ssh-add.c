/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Created: Thu Apr  6 00:52:24 1995 ylo
 * Adds an identity to the authentication server, or removes an identity.
 */

#include "includes.h"
RCSID("$Id: ssh-add.c,v 1.15 1999/12/02 20:05:40 markus Exp $");

#include "rsa.h"
#include "ssh.h"
#include "xmalloc.h"
#include "authfd.h"
#include "fingerprint.h"

void
delete_file(AuthenticationConnection *ac, const char *filename)
{
	RSA *key;
	char *comment;

	key = RSA_new();
	if (!load_public_key(filename, key, &comment)) {
		printf("Bad key file %s: %s\n", filename, strerror(errno));
		return;
	}
	if (ssh_remove_identity(ac, key))
		fprintf(stderr, "Identity removed: %s (%s)\n", filename, comment);
	else
		fprintf(stderr, "Could not remove identity: %s\n", filename);
	RSA_free(key);
	xfree(comment);
}

void
delete_all(AuthenticationConnection *ac)
{
	/* Send a request to remove all identities. */
	if (ssh_remove_all_identities(ac))
		fprintf(stderr, "All identities removed.\n");
	else
		fprintf(stderr, "Failed to remove all identitities.\n");
}

char *
ssh_askpass(char *askpass, char *msg)
{
	pid_t pid;
	size_t len;
	char *nl, *pass;
	int p[2], status;
	char buf[1024];

	if (askpass == NULL)
		fatal("internal error: askpass undefined");
	if (pipe(p) < 0)
		fatal("ssh_askpass: pipe: %s", strerror(errno));
	if ((pid = fork()) < 0)
		fatal("ssh_askpass: fork: %s", strerror(errno));
	if (pid == 0) {
		close(p[0]);
		if (dup2(p[1], STDOUT_FILENO) < 0)
			fatal("ssh_askpass: dup2: %s", strerror(errno));
		execlp(askpass, askpass, msg, (char *) 0);
		fatal("ssh_askpass: exec(%s): %s", askpass, strerror(errno));
	}
	close(p[1]);
	len = read(p[0], buf, sizeof buf);
	close(p[0]);
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			break;
	if (len <= 1)
		return xstrdup("");
	nl = strchr(buf, '\n');
	if (nl)
		*nl = '\0';
	pass = xstrdup(buf);
	memset(buf, 0, sizeof(buf));
	return pass;
}

void
add_file(AuthenticationConnection *ac, const char *filename)
{
	RSA *key;
	RSA *public_key;
	char *saved_comment, *comment, *askpass = NULL;
	char buf[1024], msg[1024];
	int success;
	int interactive = isatty(STDIN_FILENO);

	key = RSA_new();
	public_key = RSA_new();
	if (!load_public_key(filename, public_key, &saved_comment)) {
		printf("Bad key file %s: %s\n", filename, strerror(errno));
		return;
	}
	RSA_free(public_key);

	if (!interactive && getenv("DISPLAY")) {
		if (getenv(SSH_ASKPASS_ENV))
			askpass = getenv(SSH_ASKPASS_ENV);
		else
			askpass = SSH_ASKPASS_DEFAULT;
	}

	/* At first, try empty passphrase */
	success = load_private_key(filename, "", key, &comment);
	if (!success) {
		printf("Need passphrase for %.200s\n", filename);
		if (!interactive && askpass == NULL) {
			xfree(saved_comment);
			return;
		}
		snprintf(msg, sizeof msg, "Enter passphrase for %.200s", saved_comment);
		for (;;) {
			char *pass;
			if (interactive) {
				snprintf(buf, sizeof buf, "%s: ", msg);
				pass = read_passphrase(buf, 1);
			} else {
				pass = ssh_askpass(askpass, msg);
			}
			if (strcmp(pass, "") == 0) {
				xfree(pass);
				xfree(saved_comment);
				return;
			}
			success = load_private_key(filename, pass, key, &comment);
			memset(pass, 0, strlen(pass));
			xfree(pass);
			if (success)
				break;
			strlcpy(msg, "Bad passphrase, try again", sizeof msg);
		}
	}
	xfree(saved_comment);

	if (ssh_add_identity(ac, key, comment))
		fprintf(stderr, "Identity added: %s (%s)\n", filename, comment);
	else
		fprintf(stderr, "Could not add identity: %s\n", filename);
	RSA_free(key);
	xfree(comment);
}

void
list_identities(AuthenticationConnection *ac, int fp)
{
	BIGNUM *e, *n;
	int status;
	char *comment;
	int had_identities;

	e = BN_new();
	n = BN_new();
	had_identities = 0;
	for (status = ssh_get_first_identity(ac, e, n, &comment);
	     status;
	     status = ssh_get_next_identity(ac, e, n, &comment)) {
		unsigned int bits = BN_num_bits(n);
		had_identities = 1;
		if (fp) {
			printf("%d %s %s\n", bits, fingerprint(e, n), comment);
		} else {
			char *ebuf, *nbuf;
			ebuf = BN_bn2dec(e);
			if (ebuf == NULL) {
				error("list_identities: BN_bn2dec(e) failed.");
			} else {
				nbuf = BN_bn2dec(n);
				if (nbuf == NULL) {
					error("list_identities: BN_bn2dec(n) failed.");
				} else {
					printf("%d %s %s %s\n", bits, ebuf, nbuf, comment);
					free(nbuf);
				}
				free(ebuf);
			}
		}
		xfree(comment);
	}
	BN_clear_free(e);
	BN_clear_free(n);
	if (!had_identities)
		printf("The agent has no identities.\n");
}

int
main(int argc, char **argv)
{
	AuthenticationConnection *ac = NULL;
	struct passwd *pw;
	char buf[1024];
	int no_files = 1;
	int i;
	int deleting = 0;

	/* check if RSA support exists */
	if (rsa_alive() == 0) {
		extern char *__progname;

		fprintf(stderr,
			"%s: no RSA support in libssl and libcrypto.  See ssl(8).\n",
			__progname);
		exit(1);
	}
	/* At first, get a connection to the authentication agent. */
	ac = ssh_get_authentication_connection();
	if (ac == NULL) {
		fprintf(stderr, "Could not open a connection to your authentication agent.\n");
		exit(1);
	}
	for (i = 1; i < argc; i++) {
		if ((strcmp(argv[i], "-l") == 0) ||
		    (strcmp(argv[i], "-L") == 0)) {
			list_identities(ac, argv[i][1] == 'l' ? 1 : 0);
			/* Don't default-add/delete if -l. */
			no_files = 0;
			continue;
		}
		if (strcmp(argv[i], "-d") == 0) {
			deleting = 1;
			continue;
		}
		if (strcmp(argv[i], "-D") == 0) {
			delete_all(ac);
			no_files = 0;
			continue;
		}
		no_files = 0;
		if (deleting)
			delete_file(ac, argv[i]);
		else
			add_file(ac, argv[i]);
	}
	if (no_files) {
		pw = getpwuid(getuid());
		if (!pw) {
			fprintf(stderr, "No user found with uid %d\n", (int) getuid());
			ssh_close_authentication_connection(ac);
			exit(1);
		}
		snprintf(buf, sizeof buf, "%s/%s", pw->pw_dir, SSH_CLIENT_IDENTITY);
		if (deleting)
			delete_file(ac, buf);
		else
			add_file(ac, buf);
	}
	ssh_close_authentication_connection(ac);
	exit(0);
}
