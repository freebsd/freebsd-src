/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Adds an identity to the authentication server, or removes an identity.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * SSH2 implementation,
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
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
RCSID("$OpenBSD: ssh-add.c,v 1.50 2002/01/29 14:27:57 markus Exp $");
RCSID("$FreeBSD$");

#include <openssl/evp.h>

#include "ssh.h"
#include "rsa.h"
#include "log.h"
#include "xmalloc.h"
#include "key.h"
#include "authfd.h"
#include "authfile.h"
#include "pathnames.h"
#include "readpass.h"

/* argv0 */
extern char *__progname;

/* Default files to add */
static char *default_files[] = {
	_PATH_SSH_CLIENT_ID_RSA,
	_PATH_SSH_CLIENT_ID_DSA,
	_PATH_SSH_CLIENT_IDENTITY, 
	NULL
};


/* we keep a cache of one passphrases */
static char *pass = NULL;
static void
clear_pass(void)
{
	if (pass) {
		memset(pass, 0, strlen(pass));
		xfree(pass);
		pass = NULL;
	}
}

static int
delete_file(AuthenticationConnection *ac, const char *filename)
{
	Key *public;
	char *comment = NULL;
	int ret = -1;

	public = key_load_public(filename, &comment);
	if (public == NULL) {
		printf("Bad key file %s\n", filename);
		return -1;
	}
	if (ssh_remove_identity(ac, public)) {
		fprintf(stderr, "Identity removed: %s (%s)\n", filename, comment);
		ret = 0;
	} else
		fprintf(stderr, "Could not remove identity: %s\n", filename);

	key_free(public);
	xfree(comment);

	return ret;
}

/* Send a request to remove all identities. */
static int
delete_all(AuthenticationConnection *ac)
{
	int ret = -1;

	if (ssh_remove_all_identities(ac, 1))
		ret = 0;
	/* ignore error-code for ssh2 */
	ssh_remove_all_identities(ac, 2);

	if (ret == 0)
		fprintf(stderr, "All identities removed.\n");
	else
		fprintf(stderr, "Failed to remove all identities.\n");

	return ret;
}

static int
add_file(AuthenticationConnection *ac, const char *filename)
{
	struct stat st;
	Key *private;
	char *comment = NULL;
	char msg[1024];
	int ret = -1;

	if (stat(filename, &st) < 0) {
		perror(filename);
		return -1;
	}
	/* At first, try empty passphrase */
	private = key_load_private(filename, "", &comment);
	if (comment == NULL)
		comment = xstrdup(filename);
	/* try last */
	if (private == NULL && pass != NULL)
		private = key_load_private(filename, pass, NULL);
	if (private == NULL) {
		/* clear passphrase since it did not work */
		clear_pass();
		snprintf(msg, sizeof msg, "Enter passphrase for %.200s: ",
		   comment);
		for (;;) {
			pass = read_passphrase(msg, RP_ALLOW_STDIN);
			if (strcmp(pass, "") == 0) {
				clear_pass();
				xfree(comment);
				return -1;
			}
			private = key_load_private(filename, pass, &comment);
			if (private != NULL)
				break;
			clear_pass();
			strlcpy(msg, "Bad passphrase, try again: ", sizeof msg);
		}
	}
	if (ssh_add_identity(ac, private, comment)) {
		fprintf(stderr, "Identity added: %s (%s)\n", filename, comment);
		ret = 0;
	} else
		fprintf(stderr, "Could not add identity: %s\n", filename);

	xfree(comment);
	key_free(private);

	return ret;
}

static int
update_card(AuthenticationConnection *ac, int add, const char *id)
{
	if (ssh_update_card(ac, add, id)) {
		fprintf(stderr, "Card %s: %s\n",
		    add ? "added" : "removed", id);
		return 0;
	} else {
		fprintf(stderr, "Could not %s card: %s\n",
		    add ? "add" : "remove", id);
		return -1;
	}
}

static int
list_identities(AuthenticationConnection *ac, int do_fp)
{
	Key *key;
	char *comment, *fp;
	int had_identities = 0;
	int version;

	for (version = 1; version <= 2; version++) {
		for (key = ssh_get_first_identity(ac, &comment, version);
		    key != NULL;
		    key = ssh_get_next_identity(ac, &comment, version)) {
			had_identities = 1;
			if (do_fp) {
				fp = key_fingerprint(key, SSH_FP_MD5,
				    SSH_FP_HEX);
				printf("%d %s %s (%s)\n",
				    key_size(key), fp, comment, key_type(key));
				xfree(fp);
			} else {
				if (!key_write(key, stdout))
					fprintf(stderr, "key_write failed");
				fprintf(stdout, " %s\n", comment);
			}
			key_free(key);
			xfree(comment);
		}
	}
	if (!had_identities) {
		printf("The agent has no identities.\n");
		return -1;
	}
	return 0;
}

static int
do_file(AuthenticationConnection *ac, int deleting, char *file)
{
	if (deleting) {
		if (delete_file(ac, file) == -1)
			return -1;
	} else {
		if (add_file(ac, file) == -1)
			return -1;
	}
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [options]\n", __progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -l          List fingerprints of all identities.\n");
	fprintf(stderr, "  -L          List public key parameters of all identities.\n");
	fprintf(stderr, "  -d          Delete identity.\n");
	fprintf(stderr, "  -D          Delete all identities.\n");
#ifdef SMARTCARD
	fprintf(stderr, "  -s reader   Add key in smartcard reader.\n");
	fprintf(stderr, "  -e reader   Remove key in smartcard reader.\n");
#endif
}

int
main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	AuthenticationConnection *ac = NULL;
	char *sc_reader_id = NULL;
	int i, ch, deleting = 0, ret = 0;

	SSLeay_add_all_algorithms();

	/* At first, get a connection to the authentication agent. */
	ac = ssh_get_authentication_connection();
	if (ac == NULL) {
		fprintf(stderr, "Could not open a connection to your authentication agent.\n");
		exit(2);
	}
	while ((ch = getopt(argc, argv, "lLdDe:s:")) != -1) {
		switch (ch) {
		case 'l':
		case 'L':
			if (list_identities(ac, ch == 'l' ? 1 : 0) == -1)
				ret = 1;
			goto done;
			break;
		case 'd':
			deleting = 1;
			break;
		case 'D':
			if (delete_all(ac) == -1)
				ret = 1;
			goto done;
			break;
		case 's':
			sc_reader_id = optarg;
			break;
		case 'e':
			deleting = 1;
			sc_reader_id = optarg;
			break;
		default:
			usage();
			ret = 1;
			goto done;
		}
	}
	argc -= optind;
	argv += optind;
	if (sc_reader_id != NULL) {
		if (update_card(ac, !deleting, sc_reader_id) == -1)
			ret = 1;
		goto done;
	}
	if (argc == 0) {
		char buf[MAXPATHLEN];
		struct passwd *pw;

		if ((pw = getpwuid(getuid())) == NULL) {
			fprintf(stderr, "No user found with uid %u\n",
			    (u_int)getuid());
			ret = 1;
			goto done;
		}

		for(i = 0; default_files[i]; i++) {
			snprintf(buf, sizeof(buf), "%s/%s", pw->pw_dir, 
			    default_files[i]);
			if (do_file(ac, deleting, buf) == -1)
				ret = 1;
		}
	} else {
		for(i = 0; i < argc; i++) {
			if (do_file(ac, deleting, argv[i]) == -1)
				ret = 1;
		}
	}
	clear_pass();

done:
	ssh_close_authentication_connection(ac);
	return ret;
}
