/* $OpenBSD: ssh-add.c,v 1.168 2023/07/06 22:17:59 dtucker Exp $ */
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

#ifdef WITH_OPENSSL
# include <openssl/evp.h>
# include "openbsd-compat/openssl-compat.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "xmalloc.h"
#include "ssh.h"
#include "log.h"
#include "sshkey.h"
#include "sshbuf.h"
#include "authfd.h"
#include "authfile.h"
#include "pathnames.h"
#include "misc.h"
#include "ssherr.h"
#include "digest.h"
#include "ssh-sk.h"
#include "sk-api.h"
#include "hostfile.h"

/* argv0 */
extern char *__progname;

/* Default files to add */
static char *default_files[] = {
#ifdef WITH_OPENSSL
	_PATH_SSH_CLIENT_ID_RSA,
#ifdef OPENSSL_HAS_ECC
	_PATH_SSH_CLIENT_ID_ECDSA,
	_PATH_SSH_CLIENT_ID_ECDSA_SK,
#endif
#endif /* WITH_OPENSSL */
	_PATH_SSH_CLIENT_ID_ED25519,
	_PATH_SSH_CLIENT_ID_ED25519_SK,
	_PATH_SSH_CLIENT_ID_XMSS,
	_PATH_SSH_CLIENT_ID_DSA,
	NULL
};

static int fingerprint_hash = SSH_FP_HASH_DEFAULT;

/* Default lifetime (0 == forever) */
static int lifetime = 0;

/* User has to confirm key use */
static int confirm = 0;

/* Maximum number of signatures (XMSS) */
static u_int maxsign = 0;
static u_int minleft = 0;

/* we keep a cache of one passphrase */
static char *pass = NULL;
static void
clear_pass(void)
{
	if (pass) {
		freezero(pass, strlen(pass));
		pass = NULL;
	}
}

static int
delete_one(int agent_fd, const struct sshkey *key, const char *comment,
    const char *path, int qflag)
{
	int r;

	if ((r = ssh_remove_identity(agent_fd, key)) != 0) {
		fprintf(stderr, "Could not remove identity \"%s\": %s\n",
		    path, ssh_err(r));
		return r;
	}
	if (!qflag) {
		fprintf(stderr, "Identity removed: %s %s (%s)\n", path,
		    sshkey_type(key), comment ? comment : "no comment");
	}
	return 0;
}

static int
delete_stdin(int agent_fd, int qflag)
{
	char *line = NULL, *cp;
	size_t linesize = 0;
	struct sshkey *key = NULL;
	int lnum = 0, r, ret = -1;

	while (getline(&line, &linesize, stdin) != -1) {
		lnum++;
		sshkey_free(key);
		key = NULL;
		line[strcspn(line, "\n")] = '\0';
		cp = line + strspn(line, " \t");
		if (*cp == '#' || *cp == '\0')
			continue;
		if ((key = sshkey_new(KEY_UNSPEC)) == NULL)
			fatal_f("sshkey_new");
		if ((r = sshkey_read(key, &cp)) != 0) {
			error_r(r, "(stdin):%d: invalid key", lnum);
			continue;
		}
		if (delete_one(agent_fd, key, cp, "(stdin)", qflag) == 0)
			ret = 0;
	}
	sshkey_free(key);
	free(line);
	return ret;
}

static int
delete_file(int agent_fd, const char *filename, int key_only, int qflag)
{
	struct sshkey *public, *cert = NULL;
	char *certpath = NULL, *comment = NULL;
	int r, ret = -1;

	if (strcmp(filename, "-") == 0)
		return delete_stdin(agent_fd, qflag);

	if ((r = sshkey_load_public(filename, &public,  &comment)) != 0) {
		printf("Bad key file %s: %s\n", filename, ssh_err(r));
		return -1;
	}
	if (delete_one(agent_fd, public, comment, filename, qflag) == 0)
		ret = 0;

	if (key_only)
		goto out;

	/* Now try to delete the corresponding certificate too */
	free(comment);
	comment = NULL;
	xasprintf(&certpath, "%s-cert.pub", filename);
	if ((r = sshkey_load_public(certpath, &cert, &comment)) != 0) {
		if (r != SSH_ERR_SYSTEM_ERROR || errno != ENOENT)
			error_r(r, "Failed to load certificate \"%s\"", certpath);
		goto out;
	}

	if (!sshkey_equal_public(cert, public))
		fatal("Certificate %s does not match private key %s",
		    certpath, filename);

	if (delete_one(agent_fd, cert, comment, certpath, qflag) == 0)
		ret = 0;

 out:
	sshkey_free(cert);
	sshkey_free(public);
	free(certpath);
	free(comment);

	return ret;
}

/* Send a request to remove all identities. */
static int
delete_all(int agent_fd, int qflag)
{
	int ret = -1;

	/*
	 * Since the agent might be forwarded, old or non-OpenSSH, when asked
	 * to remove all keys, attempt to remove both protocol v.1 and v.2
	 * keys.
	 */
	if (ssh_remove_all_identities(agent_fd, 2) == 0)
		ret = 0;
	/* ignore error-code for ssh1 */
	ssh_remove_all_identities(agent_fd, 1);

	if (ret != 0)
		fprintf(stderr, "Failed to remove all identities.\n");
	else if (!qflag)
		fprintf(stderr, "All identities removed.\n");

	return ret;
}

static int
add_file(int agent_fd, const char *filename, int key_only, int qflag,
    const char *skprovider, struct dest_constraint **dest_constraints,
    size_t ndest_constraints)
{
	struct sshkey *private, *cert;
	char *comment = NULL;
	char msg[1024], *certpath = NULL;
	int r, fd, ret = -1;
	size_t i;
	u_int32_t left;
	struct sshbuf *keyblob;
	struct ssh_identitylist *idlist;

	if (strcmp(filename, "-") == 0) {
		fd = STDIN_FILENO;
		filename = "(stdin)";
	} else if ((fd = open(filename, O_RDONLY)) == -1) {
		perror(filename);
		return -1;
	}

	/*
	 * Since we'll try to load a keyfile multiple times, permission errors
	 * will occur multiple times, so check perms first and bail if wrong.
	 */
	if (fd != STDIN_FILENO) {
		if (sshkey_perm_ok(fd, filename) != 0) {
			close(fd);
			return -1;
		}
	}
	if ((r = sshbuf_load_fd(fd, &keyblob)) != 0) {
		fprintf(stderr, "Error loading key \"%s\": %s\n",
		    filename, ssh_err(r));
		sshbuf_free(keyblob);
		close(fd);
		return -1;
	}
	close(fd);

	/* At first, try empty passphrase */
	if ((r = sshkey_parse_private_fileblob(keyblob, "", &private,
	    &comment)) != 0 && r != SSH_ERR_KEY_WRONG_PASSPHRASE) {
		fprintf(stderr, "Error loading key \"%s\": %s\n",
		    filename, ssh_err(r));
		goto fail_load;
	}
	/* try last */
	if (private == NULL && pass != NULL) {
		if ((r = sshkey_parse_private_fileblob(keyblob, pass, &private,
		    &comment)) != 0 && r != SSH_ERR_KEY_WRONG_PASSPHRASE) {
			fprintf(stderr, "Error loading key \"%s\": %s\n",
			    filename, ssh_err(r));
			goto fail_load;
		}
	}
	if (private == NULL) {
		/* clear passphrase since it did not work */
		clear_pass();
		snprintf(msg, sizeof msg, "Enter passphrase for %s%s: ",
		    filename, confirm ? " (will confirm each use)" : "");
		for (;;) {
			pass = read_passphrase(msg, RP_ALLOW_STDIN);
			if (strcmp(pass, "") == 0)
				goto fail_load;
			if ((r = sshkey_parse_private_fileblob(keyblob, pass,
			    &private, &comment)) == 0)
				break;
			else if (r != SSH_ERR_KEY_WRONG_PASSPHRASE) {
				fprintf(stderr,
				    "Error loading key \"%s\": %s\n",
				    filename, ssh_err(r));
 fail_load:
				clear_pass();
				sshbuf_free(keyblob);
				return -1;
			}
			clear_pass();
			snprintf(msg, sizeof msg,
			    "Bad passphrase, try again for %s%s: ", filename,
			    confirm ? " (will confirm each use)" : "");
		}
	}
	if (comment == NULL || *comment == '\0')
		comment = xstrdup(filename);
	sshbuf_free(keyblob);

	/* For XMSS */
	if ((r = sshkey_set_filename(private, filename)) != 0) {
		fprintf(stderr, "Could not add filename to private key: %s (%s)\n",
		    filename, comment);
		goto out;
	}
	if (maxsign && minleft &&
	    (r = ssh_fetch_identitylist(agent_fd, &idlist)) == 0) {
		for (i = 0; i < idlist->nkeys; i++) {
			if (!sshkey_equal_public(idlist->keys[i], private))
				continue;
			left = sshkey_signatures_left(idlist->keys[i]);
			if (left < minleft) {
				fprintf(stderr,
				    "Only %d signatures left.\n", left);
				break;
			}
			fprintf(stderr, "Skipping update: ");
			if (left == minleft) {
				fprintf(stderr,
				    "required signatures left (%d).\n", left);
			} else {
				fprintf(stderr,
				    "more signatures left (%d) than"
				    " required (%d).\n", left, minleft);
			}
			ssh_free_identitylist(idlist);
			goto out;
		}
		ssh_free_identitylist(idlist);
	}

	if (sshkey_is_sk(private)) {
		if (skprovider == NULL) {
			fprintf(stderr, "Cannot load FIDO key %s "
			    "without provider\n", filename);
			goto out;
		}
	} else {
		/* Don't send provider constraint for other keys */
		skprovider = NULL;
	}

	if ((r = ssh_add_identity_constrained(agent_fd, private, comment,
	    lifetime, confirm, maxsign, skprovider,
	    dest_constraints, ndest_constraints)) == 0) {
		ret = 0;
		if (!qflag) {
			fprintf(stderr, "Identity added: %s (%s)\n",
			    filename, comment);
			if (lifetime != 0) {
				fprintf(stderr,
				    "Lifetime set to %d seconds\n", lifetime);
			}
			if (confirm != 0) {
				fprintf(stderr, "The user must confirm "
				    "each use of the key\n");
			}
		}
	} else {
		fprintf(stderr, "Could not add identity \"%s\": %s\n",
		    filename, ssh_err(r));
	}

	/* Skip trying to load the cert if requested */
	if (key_only)
		goto out;

	/* Now try to add the certificate flavour too */
	xasprintf(&certpath, "%s-cert.pub", filename);
	if ((r = sshkey_load_public(certpath, &cert, NULL)) != 0) {
		if (r != SSH_ERR_SYSTEM_ERROR || errno != ENOENT)
			error_r(r, "Failed to load certificate \"%s\"", certpath);
		goto out;
	}

	if (!sshkey_equal_public(cert, private)) {
		error("Certificate %s does not match private key %s",
		    certpath, filename);
		sshkey_free(cert);
		goto out;
	}

	/* Graft with private bits */
	if ((r = sshkey_to_certified(private)) != 0) {
		error_fr(r, "sshkey_to_certified");
		sshkey_free(cert);
		goto out;
	}
	if ((r = sshkey_cert_copy(cert, private)) != 0) {
		error_fr(r, "sshkey_cert_copy");
		sshkey_free(cert);
		goto out;
	}
	sshkey_free(cert);

	if ((r = ssh_add_identity_constrained(agent_fd, private, comment,
	    lifetime, confirm, maxsign, skprovider,
	    dest_constraints, ndest_constraints)) != 0) {
		error_r(r, "Certificate %s (%s) add failed", certpath,
		    private->cert->key_id);
		goto out;
	}
	/* success */
	if (!qflag) {
		fprintf(stderr, "Certificate added: %s (%s)\n", certpath,
		    private->cert->key_id);
		if (lifetime != 0) {
			fprintf(stderr, "Lifetime set to %d seconds\n",
			    lifetime);
		}
		if (confirm != 0) {
			fprintf(stderr, "The user must confirm each use "
			    "of the key\n");
		}
	}

 out:
	free(certpath);
	free(comment);
	sshkey_free(private);

	return ret;
}

static int
update_card(int agent_fd, int add, const char *id, int qflag,
    struct dest_constraint **dest_constraints, size_t ndest_constraints)
{
	char *pin = NULL;
	int r, ret = -1;

	if (add) {
		if ((pin = read_passphrase("Enter passphrase for PKCS#11: ",
		    RP_ALLOW_STDIN)) == NULL)
			return -1;
	}

	if ((r = ssh_update_card(agent_fd, add, id, pin == NULL ? "" : pin,
	    lifetime, confirm, dest_constraints, ndest_constraints)) == 0) {
		ret = 0;
		if (!qflag) {
			fprintf(stderr, "Card %s: %s\n",
			    add ? "added" : "removed", id);
		}
	} else {
		fprintf(stderr, "Could not %s card \"%s\": %s\n",
		    add ? "add" : "remove", id, ssh_err(r));
		ret = -1;
	}
	free(pin);
	return ret;
}

static int
test_key(int agent_fd, const char *filename)
{
	struct sshkey *key = NULL;
	u_char *sig = NULL;
	const char *alg = NULL;
	size_t slen = 0;
	int r, ret = -1;
	char data[1024];

	if ((r = sshkey_load_public(filename, &key, NULL)) != 0) {
		error_r(r, "Couldn't read public key %s", filename);
		return -1;
	}
	if (sshkey_type_plain(key->type) == KEY_RSA)
		alg = "rsa-sha2-256";
	arc4random_buf(data, sizeof(data));
	if ((r = ssh_agent_sign(agent_fd, key, &sig, &slen, data, sizeof(data),
	    alg, 0)) != 0) {
		error_r(r, "Agent signature failed for %s", filename);
		goto done;
	}
	if ((r = sshkey_verify(key, sig, slen, data, sizeof(data),
	    alg, 0, NULL)) != 0) {
		error_r(r, "Signature verification failed for %s", filename);
		goto done;
	}
	/* success */
	ret = 0;
 done:
	free(sig);
	sshkey_free(key);
	return ret;
}

static int
list_identities(int agent_fd, int do_fp)
{
	char *fp;
	int r;
	struct ssh_identitylist *idlist;
	u_int32_t left;
	size_t i;

	if ((r = ssh_fetch_identitylist(agent_fd, &idlist)) != 0) {
		if (r != SSH_ERR_AGENT_NO_IDENTITIES)
			fprintf(stderr, "error fetching identities: %s\n",
			    ssh_err(r));
		else
			printf("The agent has no identities.\n");
		return -1;
	}
	for (i = 0; i < idlist->nkeys; i++) {
		if (do_fp) {
			fp = sshkey_fingerprint(idlist->keys[i],
			    fingerprint_hash, SSH_FP_DEFAULT);
			printf("%u %s %s (%s)\n", sshkey_size(idlist->keys[i]),
			    fp == NULL ? "(null)" : fp, idlist->comments[i],
			    sshkey_type(idlist->keys[i]));
			free(fp);
		} else {
			if ((r = sshkey_write(idlist->keys[i], stdout)) != 0) {
				fprintf(stderr, "sshkey_write: %s\n",
				    ssh_err(r));
				continue;
			}
			fprintf(stdout, " %s", idlist->comments[i]);
			left = sshkey_signatures_left(idlist->keys[i]);
			if (left > 0)
				fprintf(stdout,
				    " [signatures left %d]", left);
			fprintf(stdout, "\n");
		}
	}
	ssh_free_identitylist(idlist);
	return 0;
}

static int
lock_agent(int agent_fd, int lock)
{
	char prompt[100], *p1, *p2;
	int r, passok = 1, ret = -1;

	strlcpy(prompt, "Enter lock password: ", sizeof(prompt));
	p1 = read_passphrase(prompt, RP_ALLOW_STDIN);
	if (lock) {
		strlcpy(prompt, "Again: ", sizeof prompt);
		p2 = read_passphrase(prompt, RP_ALLOW_STDIN);
		if (strcmp(p1, p2) != 0) {
			fprintf(stderr, "Passwords do not match.\n");
			passok = 0;
		}
		freezero(p2, strlen(p2));
	}
	if (passok) {
		if ((r = ssh_lock_agent(agent_fd, lock, p1)) == 0) {
			fprintf(stderr, "Agent %slocked.\n", lock ? "" : "un");
			ret = 0;
		} else {
			fprintf(stderr, "Failed to %slock agent: %s\n",
			    lock ? "" : "un", ssh_err(r));
		}
	}
	freezero(p1, strlen(p1));
	return (ret);
}

static int
load_resident_keys(int agent_fd, const char *skprovider, int qflag,
    struct dest_constraint **dest_constraints, size_t ndest_constraints)
{
	struct sshsk_resident_key **srks;
	size_t nsrks, i;
	struct sshkey *key;
	int r, ok = 0;
	char *fp;

	pass = read_passphrase("Enter PIN for authenticator: ", RP_ALLOW_STDIN);
	if ((r = sshsk_load_resident(skprovider, NULL, pass, 0,
	    &srks, &nsrks)) != 0) {
		error_r(r, "Unable to load resident keys");
		return r;
	}
	for (i = 0; i < nsrks; i++) {
		key = srks[i]->key;
		if ((fp = sshkey_fingerprint(key,
		    fingerprint_hash, SSH_FP_DEFAULT)) == NULL)
			fatal_f("sshkey_fingerprint failed");
		if ((r = ssh_add_identity_constrained(agent_fd, key, "",
		    lifetime, confirm, maxsign, skprovider,
		    dest_constraints, ndest_constraints)) != 0) {
			error("Unable to add key %s %s",
			    sshkey_type(key), fp);
			free(fp);
			ok = r;
			continue;
		}
		if (ok == 0)
			ok = 1;
		if (!qflag) {
			fprintf(stderr, "Resident identity added: %s %s\n",
			    sshkey_type(key), fp);
			if (lifetime != 0) {
				fprintf(stderr,
				    "Lifetime set to %d seconds\n", lifetime);
			}
			if (confirm != 0) {
				fprintf(stderr, "The user must confirm "
				    "each use of the key\n");
			}
		}
		free(fp);
	}
	sshsk_free_resident_keys(srks, nsrks);
	if (nsrks == 0)
		return SSH_ERR_KEY_NOT_FOUND;
	return ok == 1 ? 0 : ok;
}

static int
do_file(int agent_fd, int deleting, int key_only, char *file, int qflag,
    const char *skprovider, struct dest_constraint **dest_constraints,
    size_t ndest_constraints)
{
	if (deleting) {
		if (delete_file(agent_fd, file, key_only, qflag) == -1)
			return -1;
	} else {
		if (add_file(agent_fd, file, key_only, qflag, skprovider,
		    dest_constraints, ndest_constraints) == -1)
			return -1;
	}
	return 0;
}

/* Append string 's' to a NULL-terminated array of strings */
static void
stringlist_append(char ***listp, const char *s)
{
	size_t i = 0;

	if (*listp == NULL)
		*listp = xcalloc(2, sizeof(**listp));
	else {
		for (i = 0; (*listp)[i] != NULL; i++)
			; /* count */
		*listp = xrecallocarray(*listp, i + 1, i + 2, sizeof(**listp));
	}
	(*listp)[i] = xstrdup(s);
}

static void
parse_dest_constraint_hop(const char *s, struct dest_constraint_hop *dch,
    char **hostkey_files)
{
	char *user = NULL, *host, *os, *path;
	size_t i;
	struct hostkeys *hostkeys;
	const struct hostkey_entry *hke;
	int r, want_ca;

	memset(dch, '\0', sizeof(*dch));
	os = xstrdup(s);
	if ((host = strchr(os, '@')) == NULL)
		host = os;
	else {
		*host++ = '\0';
		user = os;
	}
	cleanhostname(host);
	/* Trivial case: username@ (all hosts) */
	if (*host == '\0') {
		if (user == NULL) {
			fatal("Invalid key destination constraint \"%s\": "
			    "does not specify user or host", s);
		}
		dch->user = xstrdup(user);
		/* other fields left blank */
		free(os);
		return;
	}
	if (hostkey_files == NULL)
		fatal_f("no hostkey files");
	/* Otherwise we need to look up the keys for this hostname */
	hostkeys = init_hostkeys();
	for (i = 0; hostkey_files[i]; i++) {
		path = tilde_expand_filename(hostkey_files[i], getuid());
		debug2_f("looking up host keys for \"%s\" in %s", host, path);
                load_hostkeys(hostkeys, host, path, 0);
		free(path);
	}
	dch->user = user == NULL ? NULL : xstrdup(user);
	dch->hostname = xstrdup(host);
	for (i = 0; i < hostkeys->num_entries; i++) {
		hke = hostkeys->entries + i;
		want_ca = hke->marker == MRK_CA;
		if (hke->marker != MRK_NONE && !want_ca)
			continue;
		debug3_f("%s%s%s: adding %s %skey from %s:%lu as key %u",
		    user == NULL ? "": user, user == NULL ? "" : "@",
		    host, sshkey_type(hke->key), want_ca ? "CA " : "",
		    hke->file, hke->line, dch->nkeys);
		dch->keys = xrecallocarray(dch->keys, dch->nkeys,
		    dch->nkeys + 1, sizeof(*dch->keys));
		dch->key_is_ca = xrecallocarray(dch->key_is_ca, dch->nkeys,
		    dch->nkeys + 1, sizeof(*dch->key_is_ca));
		if ((r = sshkey_from_private(hke->key,
		    &(dch->keys[dch->nkeys]))) != 0)
			fatal_fr(r, "sshkey_from_private");
		dch->key_is_ca[dch->nkeys] = want_ca;
		dch->nkeys++;
	}
	if (dch->nkeys == 0)
		fatal("No host keys found for destination \"%s\"", host);
	free_hostkeys(hostkeys);
	free(os);
	return;
}

static void
parse_dest_constraint(const char *s, struct dest_constraint ***dcp,
    size_t *ndcp, char **hostkey_files)
{
	struct dest_constraint *dc;
	char *os, *cp;

	dc = xcalloc(1, sizeof(*dc));
	os = xstrdup(s);
	if ((cp = strchr(os, '>')) == NULL) {
		/* initial hop; no 'from' hop specified */
		parse_dest_constraint_hop(os, &dc->to, hostkey_files);
	} else {
		/* two hops specified */
		*(cp++) = '\0';
		parse_dest_constraint_hop(os, &dc->from, hostkey_files);
		parse_dest_constraint_hop(cp, &dc->to, hostkey_files);
		if (dc->from.user != NULL) {
			fatal("Invalid key constraint %s: cannot specify "
			    "user on 'from' host", os);
		}
	}
	/* XXX eliminate or error on duplicates */
	debug2_f("constraint %zu: %s%s%s (%u keys) > %s%s%s (%u keys)", *ndcp,
	    dc->from.user ? dc->from.user : "", dc->from.user ? "@" : "",
	    dc->from.hostname ? dc->from.hostname : "(ORIGIN)", dc->from.nkeys,
	    dc->to.user ? dc->to.user : "", dc->to.user ? "@" : "",
	    dc->to.hostname ? dc->to.hostname : "(ANY)", dc->to.nkeys);
	*dcp = xrecallocarray(*dcp, *ndcp, *ndcp + 1, sizeof(**dcp));
	(*dcp)[(*ndcp)++] = dc;
	free(os);
}


static void
usage(void)
{
	fprintf(stderr,
"usage: ssh-add [-cDdKkLlqvXx] [-E fingerprint_hash] [-H hostkey_file]\n"
"               [-h destination_constraint] [-S provider] [-t life]\n"
#ifdef WITH_XMSS
"               [-M maxsign] [-m minleft]\n"
#endif
"               [file ...]\n"
"       ssh-add -s pkcs11\n"
"       ssh-add -e pkcs11\n"
"       ssh-add -T pubkey ...\n"
	);
}

int
main(int argc, char **argv)
{
	extern char *optarg;
	extern int optind;
	int agent_fd;
	char *pkcs11provider = NULL, *skprovider = NULL;
	char **dest_constraint_strings = NULL, **hostkey_files = NULL;
	int r, i, ch, deleting = 0, ret = 0, key_only = 0, do_download = 0;
	int xflag = 0, lflag = 0, Dflag = 0, qflag = 0, Tflag = 0;
	SyslogFacility log_facility = SYSLOG_FACILITY_AUTH;
	LogLevel log_level = SYSLOG_LEVEL_INFO;
	struct dest_constraint **dest_constraints = NULL;
	size_t ndest_constraints = 0;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	__progname = ssh_get_progname(argv[0]);
	seed_rng();

	log_init(__progname, log_level, log_facility, 1);

	setvbuf(stdout, NULL, _IOLBF, 0);

	/* First, get a connection to the authentication agent. */
	switch (r = ssh_get_authentication_socket(&agent_fd)) {
	case 0:
		break;
	case SSH_ERR_AGENT_NOT_PRESENT:
		fprintf(stderr, "Could not open a connection to your "
		    "authentication agent.\n");
		exit(2);
	default:
		fprintf(stderr, "Error connecting to agent: %s\n", ssh_err(r));
		exit(2);
	}

	skprovider = getenv("SSH_SK_PROVIDER");

	while ((ch = getopt(argc, argv, "vkKlLcdDTxXE:e:h:H:M:m:qs:S:t:")) != -1) {
		switch (ch) {
		case 'v':
			if (log_level == SYSLOG_LEVEL_INFO)
				log_level = SYSLOG_LEVEL_DEBUG1;
			else if (log_level < SYSLOG_LEVEL_DEBUG3)
				log_level++;
			break;
		case 'E':
			fingerprint_hash = ssh_digest_alg_by_name(optarg);
			if (fingerprint_hash == -1)
				fatal("Invalid hash algorithm \"%s\"", optarg);
			break;
		case 'H':
			stringlist_append(&hostkey_files, optarg);
			break;
		case 'h':
			stringlist_append(&dest_constraint_strings, optarg);
			break;
		case 'k':
			key_only = 1;
			break;
		case 'K':
			do_download = 1;
			break;
		case 'l':
		case 'L':
			if (lflag != 0)
				fatal("-%c flag already specified", lflag);
			lflag = ch;
			break;
		case 'x':
		case 'X':
			if (xflag != 0)
				fatal("-%c flag already specified", xflag);
			xflag = ch;
			break;
		case 'c':
			confirm = 1;
			break;
		case 'm':
			minleft = (u_int)strtonum(optarg, 1, UINT_MAX, NULL);
			if (minleft == 0) {
				usage();
				ret = 1;
				goto done;
			}
			break;
		case 'M':
			maxsign = (u_int)strtonum(optarg, 1, UINT_MAX, NULL);
			if (maxsign == 0) {
				usage();
				ret = 1;
				goto done;
			}
			break;
		case 'd':
			deleting = 1;
			break;
		case 'D':
			Dflag = 1;
			break;
		case 's':
			pkcs11provider = optarg;
			break;
		case 'S':
			skprovider = optarg;
			break;
		case 'e':
			deleting = 1;
			pkcs11provider = optarg;
			break;
		case 't':
			if ((lifetime = convtime(optarg)) == -1 ||
			    lifetime < 0 || (u_long)lifetime > UINT32_MAX) {
				fprintf(stderr, "Invalid lifetime\n");
				ret = 1;
				goto done;
			}
			break;
		case 'q':
			qflag = 1;
			break;
		case 'T':
			Tflag = 1;
			break;
		default:
			usage();
			ret = 1;
			goto done;
		}
	}
	log_init(__progname, log_level, log_facility, 1);

	if ((xflag != 0) + (lflag != 0) + (Dflag != 0) > 1)
		fatal("Invalid combination of actions");
	else if (xflag) {
		if (lock_agent(agent_fd, xflag == 'x' ? 1 : 0) == -1)
			ret = 1;
		goto done;
	} else if (lflag) {
		if (list_identities(agent_fd, lflag == 'l' ? 1 : 0) == -1)
			ret = 1;
		goto done;
	} else if (Dflag) {
		if (delete_all(agent_fd, qflag) == -1)
			ret = 1;
		goto done;
	}

#ifdef ENABLE_SK_INTERNAL
	if (skprovider == NULL)
		skprovider = "internal";
#endif

	if (hostkey_files == NULL) {
		/* use defaults from readconf.c */
		stringlist_append(&hostkey_files, _PATH_SSH_USER_HOSTFILE);
		stringlist_append(&hostkey_files, _PATH_SSH_USER_HOSTFILE2);
		stringlist_append(&hostkey_files, _PATH_SSH_SYSTEM_HOSTFILE);
		stringlist_append(&hostkey_files, _PATH_SSH_SYSTEM_HOSTFILE2);
	}
	if (dest_constraint_strings != NULL) {
		for (i = 0; dest_constraint_strings[i] != NULL; i++) {
			parse_dest_constraint(dest_constraint_strings[i],
			  &dest_constraints, &ndest_constraints, hostkey_files);
		}
	}

	argc -= optind;
	argv += optind;
	if (Tflag) {
		if (argc <= 0)
			fatal("no keys to test");
		for (r = i = 0; i < argc; i++)
			r |= test_key(agent_fd, argv[i]);
		ret = r == 0 ? 0 : 1;
		goto done;
	}
	if (pkcs11provider != NULL) {
		if (update_card(agent_fd, !deleting, pkcs11provider,
		    qflag, dest_constraints, ndest_constraints) == -1)
			ret = 1;
		goto done;
	}
	if (do_download) {
		if (skprovider == NULL)
			fatal("Cannot download keys without provider");
		if (load_resident_keys(agent_fd, skprovider, qflag,
		    dest_constraints, ndest_constraints) != 0)
			ret = 1;
		goto done;
	}
	if (argc == 0) {
		char buf[PATH_MAX];
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
			if (stat(buf, &st) == -1)
				continue;
			if (do_file(agent_fd, deleting, key_only, buf,
			    qflag, skprovider,
			    dest_constraints, ndest_constraints) == -1)
				ret = 1;
			else
				count++;
		}
		if (count == 0)
			ret = 1;
	} else {
		for (i = 0; i < argc; i++) {
			if (do_file(agent_fd, deleting, key_only,
			    argv[i], qflag, skprovider,
			    dest_constraints, ndest_constraints) == -1)
				ret = 1;
		}
	}
done:
	clear_pass();
	ssh_close_authentication_socket(agent_fd);
	return ret;
}
