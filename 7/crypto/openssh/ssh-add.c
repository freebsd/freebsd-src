/* $OpenBSD: ssh-add.c,v 1.90 2007/09/09 11:38:01 sobrado Exp $ */
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <openssl/evp.h>
#include "openbsd-compat/openssl-compat.h"

#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "ssh.h"
#include "rsa.h"
#include "log.h"
#include "key.h"
#include "buffer.h"
#include "authfd.h"
#include "authfile.h"
#include "pathnames.h"
#include "misc.h"

/* argv0 */
extern char *__progname;

/* Default files to add */
static char *default_files[] = {
	_PATH_SSH_CLIENT_ID_RSA,
	_PATH_SSH_CLIENT_ID_DSA,
	_PATH_SSH_CLIENT_IDENTITY,
	NULL
};

/* Default lifetime (0 == forever) */
static int lifetime = 0;

/* User has to confirm key use */
static int confirm = 0;

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
	Key *private;
	char *comment = NULL;
	char msg[1024];
	int fd, perms_ok, ret = -1;

	if ((fd = open(filename, O_RDONLY)) < 0) {
		perror(filename);
		return -1;
	}

	/*
	 * Since we'll try to load a keyfile multiple times, permission errors
	 * will occur multiple times, so check perms first and bail if wrong.
	 */
	perms_ok = key_perm_ok(fd, filename);
	close(fd);
	if (!perms_ok)
		return -1;

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
			snprintf(msg, sizeof msg,
			    "Bad passphrase, try again for %.200s: ", comment);
		}
	}

	if (ssh_add_identity_constrained(ac, private, comment, lifetime,
	    confirm)) {
		fprintf(stderr, "Identity added: %s (%s)\n", filename, comment);
		ret = 0;
		if (lifetime != 0)
			fprintf(stderr,
			    "Lifetime set to %d seconds\n", lifetime);
		if (confirm != 0)
			fprintf(stderr,
			    "The user has to confirm each use of the key\n");
	} else if (ssh_add_identity(ac, private, comment)) {
		fprintf(stderr, "Identity added: %s (%s)\n", filename, comment);
		ret = 0;
	} else {
		fprintf(stderr, "Could not add identity: %s\n", filename);
	}

	xfree(comment);
	key_free(private);

	return ret;
}

static int
update_card(AuthenticationConnection *ac, int add, const char *id)
{
	char *pin;
	int ret = -1;

	pin = read_passphrase("Enter passphrase for smartcard: ", RP_ALLOW_STDIN);
	if (pin == NULL)
		return -1;

	if (ssh_update_card(ac, add, id, pin, lifetime, confirm)) {
		fprintf(stderr, "Card %s: %s\n",
		    add ? "added" : "removed", id);
		ret = 0;
	} else {
		fprintf(stderr, "Could not %s card: %s\n",
		    add ? "add" : "remove", id);
		ret = -1;
	}
	xfree(pin);
	return ret;
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
lock_agent(AuthenticationConnection *ac, int lock)
{
	char prompt[100], *p1, *p2;
	int passok = 1, ret = -1;

	strlcpy(prompt, "Enter lock password: ", sizeof(prompt));
	p1 = read_passphrase(prompt, RP_ALLOW_STDIN);
	if (lock) {
		strlcpy(prompt, "Again: ", sizeof prompt);
		p2 = read_passphrase(prompt, RP_ALLOW_STDIN);
		if (strcmp(p1, p2) != 0) {
			fprintf(stderr, "Passwords do not match.\n");
			passok = 0;
		}
		memset(p2, 0, strlen(p2));
		xfree(p2);
	}
	if (passok && ssh_lock_agent(ac, lock, p1)) {
		fprintf(stderr, "Agent %slocked.\n", lock ? "" : "un");
		ret = 0;
	} else
		fprintf(stderr, "Failed to %slock agent.\n", lock ? "" : "un");
	memset(p1, 0, strlen(p1));
	xfree(p1);
	return (ret);
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
	fprintf(stderr, "usage: %s [options] [file ...]\n", __progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -l          List fingerprints of all identities.\n");
	fprintf(stderr, "  -L          List public key parameters of all identities.\n");
	fprintf(stderr, "  -d          Delete identity.\n");
	fprintf(stderr, "  -D          Delete all identities.\n");
	fprintf(stderr, "  -x          Lock agent.\n");
	fprintf(stderr, "  -X          Unlock agent.\n");
	fprintf(stderr, "  -t life     Set lifetime (in seconds) when adding identities.\n");
	fprintf(stderr, "  -c          Require confirmation to sign using identities\n");
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

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	__progname = ssh_get_progname(argv[0]);
	init_rng();
	seed_rng();

	SSLeay_add_all_algorithms();

	/* At first, get a connection to the authentication agent. */
	ac = ssh_get_authentication_connection();
	if (ac == NULL) {
		fprintf(stderr,
		    "Could not open a connection to your authentication agent.\n");
		exit(2);
	}
	while ((ch = getopt(argc, argv, "lLcdDxXe:s:t:")) != -1) {
		switch (ch) {
		case 'l':
		case 'L':
			if (list_identities(ac, ch == 'l' ? 1 : 0) == -1)
				ret = 1;
			goto done;
		case 'x':
		case 'X':
			if (lock_agent(ac, ch == 'x' ? 1 : 0) == -1)
				ret = 1;
			goto done;
		case 'c':
			confirm = 1;
			break;
		case 'd':
			deleting = 1;
			break;
		case 'D':
			if (delete_all(ac) == -1)
				ret = 1;
			goto done;
		case 's':
			sc_reader_id = optarg;
			break;
		case 'e':
			deleting = 1;
			sc_reader_id = optarg;
			break;
		case 't':
			if ((lifetime = convtime(optarg)) == -1) {
				fprintf(stderr, "Invalid lifetime\n");
				ret = 1;
				goto done;
			}
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
		struct stat st;
		int count = 0;

		if ((pw = getpwuid(getuid())) == NULL) {
			fprintf(stderr, "No user found with uid %u\n",
			    (u_int)getuid());
			ret = 1;
			goto done;
		}

		for (i = 0; default_files[i]; i++) {
			snprintf(buf, sizeof(buf), "%s/%s", pw->pw_dir,
			    default_files[i]);
			if (stat(buf, &st) < 0)
				continue;
			if (do_file(ac, deleting, buf) == -1)
				ret = 1;
			else
				count++;
		}
		if (count == 0)
			ret = 1;
	} else {
		for (i = 0; i < argc; i++) {
			if (do_file(ac, deleting, argv[i]) == -1)
				ret = 1;
		}
	}
	clear_pass();

done:
	ssh_close_authentication_connection(ac);
	return ret;
}
