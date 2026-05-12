/* $OpenBSD: ssh-pkcs11-client.c,v 1.24 2025/07/30 10:17:13 dtucker Exp $ */
/*
 * Copyright (c) 2010 Markus Friedl.  All rights reserved.
 * Copyright (c) 2014 Pedro Martelletto. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "pathnames.h"
#include "xmalloc.h"
#include "sshbuf.h"
#include "log.h"
#include "misc.h"
#include "sshkey.h"
#include "authfd.h"
#include "atomicio.h"
#include "ssh-pkcs11.h"
#include "ssherr.h"

/* borrows code from sftp-server and ssh-agent */

/*
 * Maintain a list of ssh-pkcs11-helper subprocesses. These may be looked up
 * by provider path or their unique keyblobs.
 */
struct helper {
	char *path;
	pid_t pid;
	int fd;
	size_t nkeyblobs;
	struct sshbuf **keyblobs; /* XXX use a tree or something faster */
};
static struct helper **helpers;
static size_t nhelpers;

static struct helper *
helper_by_provider(const char *path)
{
	size_t i;

	for (i = 0; i < nhelpers; i++) {
		if (helpers[i] == NULL || helpers[i]->path == NULL ||
		    helpers[i]->fd == -1)
			continue;
		if (strcmp(helpers[i]->path, path) == 0)
			return helpers[i];
	}
	return NULL;
}

static struct helper *
helper_by_key(const struct sshkey *key)
{
	size_t i, j;
	struct sshbuf *keyblob = NULL;
	int r;

	if ((keyblob = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshkey_putb(key, keyblob)) != 0)
		fatal_fr(r, "serialise key");

	for (i = 0; i < nhelpers; i++) {
		if (helpers[i] == NULL)
			continue;
		for (j = 0; j < helpers[i]->nkeyblobs; j++) {
			if (sshbuf_equals(keyblob,
			    helpers[i]->keyblobs[j]) == 0) {
				sshbuf_free(keyblob);
				return helpers[i];
			}
		}
	}
	sshbuf_free(keyblob);
	return NULL;

}

static void
helper_add_key(struct helper *helper, struct sshkey *key)
{
	int r;

	helper->keyblobs = xrecallocarray(helper->keyblobs, helper->nkeyblobs,
	    helper->nkeyblobs + 1, sizeof(*helper->keyblobs));
	if ((helper->keyblobs[helper->nkeyblobs] = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshkey_putb(key, helper->keyblobs[helper->nkeyblobs])) != 0)
		fatal_fr(r, "shkey_putb failed");
	helper->nkeyblobs++;
	debug3_f("added %s key for provider %s, now has %zu keys",
	    sshkey_type(key), helper->path, helper->nkeyblobs);
}

static void
helper_terminate(struct helper *helper)
{
	size_t i;
	int found = 0;

	if (helper == NULL)
		return;
	if (helper->path == NULL)
		fatal_f("inconsistent helper");

	debug3_f("terminating helper for %s; remaining %zu keys",
	    helper->path, helper->nkeyblobs);

	close(helper->fd);
	/* XXX waitpid() */
	helper->fd = -1;
	helper->pid = -1;

	/* repack helpers */
	for (i = 0; i < nhelpers; i++) {
		if (helpers[i] == helper) {
			if (found)
				fatal_f("helper recorded more than once");
			found = 1;
		} else if (found)
			helpers[i - 1] = helpers[i];
	}
	if (found) {
		helpers = xrecallocarray(helpers, nhelpers,
		    nhelpers - 1, sizeof(*helpers));
		nhelpers--;
	}
	for (i = 0; i < helper->nkeyblobs; i++)
		sshbuf_free(helper->keyblobs[i]);
	free(helper->path);
	free(helper);
}

static void
send_msg(int fd, struct sshbuf *m)
{
	u_char buf[4];
	size_t mlen = sshbuf_len(m);
	int r;

	if (fd == -1)
		return;
	POKE_U32(buf, mlen);
	if (atomicio(vwrite, fd, buf, 4) != 4 ||
	    atomicio(vwrite, fd, sshbuf_mutable_ptr(m),
	    sshbuf_len(m)) != sshbuf_len(m))
		error("write to helper failed");
	if ((r = sshbuf_consume(m, mlen)) != 0)
		fatal_fr(r, "consume");
}

static int
recv_msg(int fd, struct sshbuf *m)
{
	u_int l, len;
	u_char c, buf[1024];
	int r;

	sshbuf_reset(m);
	if (fd == -1)
		return 0; /* XXX */
	if ((len = atomicio(read, fd, buf, 4)) != 4) {
		error("read from helper failed: %u", len);
		return (0); /* XXX */
	}
	len = PEEK_U32(buf);
	if (len > 256 * 1024)
		fatal("response too long: %u", len);
	/* read len bytes into m */
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		if (atomicio(read, fd, buf, l) != l) {
			error("response from helper failed.");
			return (0); /* XXX */
		}
		if ((r = sshbuf_put(m, buf, l)) != 0)
			fatal_fr(r, "sshbuf_put");
		len -= l;
	}
	if ((r = sshbuf_get_u8(m, &c)) != 0)
		fatal_fr(r, "parse type");
	return c;
}

int
pkcs11_init(int interactive)
{
	return 0;
}

void
pkcs11_terminate(void)
{
	size_t i;

	debug3_f("terminating %zu helpers", nhelpers);
	for (i = 0; i < nhelpers; i++)
		helper_terminate(helpers[i]);
}

int
pkcs11_sign(struct sshkey *key,
    u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen,
    const char *alg, const char *sk_provider,
    const char *sk_pin, u_int compat)
{
	struct sshbuf *msg = NULL;
	struct helper *helper;
	int status, r;
	u_char *signature = NULL;
	size_t signature_len = 0;
	int ret = SSH_ERR_INTERNAL_ERROR;

	if (sigp != NULL)
		*sigp = NULL;
	if (lenp != NULL)
		*lenp = 0;

	if ((helper = helper_by_key(key)) == NULL || helper->fd == -1)
		fatal_f("no helper for %s key", sshkey_type(key));

	if ((msg = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_u8(msg, SSH2_AGENTC_SIGN_REQUEST)) != 0 ||
	    (r = sshkey_puts_plain(key, msg)) != 0 ||
	    (r = sshbuf_put_string(msg, data, datalen)) != 0 ||
	    (r = sshbuf_put_cstring(msg, alg == NULL ? "" : alg)) != 0 ||
	    (r = sshbuf_put_u32(msg, compat)) != 0)
		fatal_fr(r, "compose");
	send_msg(helper->fd, msg);
	sshbuf_reset(msg);

	if ((status = recv_msg(helper->fd, msg)) != SSH2_AGENT_SIGN_RESPONSE) {
		/* XXX translate status to something useful */
		debug_fr(r, "recv_msg");
		ret = SSH_ERR_AGENT_FAILURE;
		goto fail;
	}

	if ((r = sshbuf_get_string(msg, &signature, &signature_len)) != 0)
		fatal_fr(r, "parse");

	/* success */
	if (sigp != NULL) {
		*sigp = signature;
		signature = NULL;
	}
	if (lenp != NULL)
		*lenp = signature_len;
	ret = 0;

 fail:
	free(signature);
	sshbuf_free(msg);
	return ret;
}

/*
 * Make a private PKCS#11-backed certificate by grafting a previously-loaded
 * PKCS#11 private key and a public certificate key.
 */
int
pkcs11_make_cert(const struct sshkey *priv,
    const struct sshkey *certpub, struct sshkey **certprivp)
{
	struct helper *helper = NULL;
	struct sshkey *ret;
	int r;

	if ((helper = helper_by_key(priv)) == NULL || helper->fd == -1)
		fatal_f("no helper for %s key", sshkey_type(priv));

	debug3_f("private key type %s cert type %s on provider %s",
	    sshkey_type(priv), sshkey_type(certpub), helper->path);

	*certprivp = NULL;
	if (!sshkey_is_cert(certpub) || sshkey_is_cert(priv) ||
	    !sshkey_equal_public(priv, certpub)) {
		error_f("private key %s doesn't match cert %s",
		    sshkey_type(priv), sshkey_type(certpub));
		return SSH_ERR_INVALID_ARGUMENT;
	}
	*certprivp = NULL;
	if ((r = sshkey_from_private(priv, &ret)) != 0)
		fatal_fr(r, "copy key");

	ret->flags |= SSHKEY_FLAG_EXT;
	if ((r = sshkey_to_certified(ret)) != 0 ||
	    (r = sshkey_cert_copy(certpub, ret)) != 0)
		fatal_fr(r, "graft certificate");

	helper_add_key(helper, ret);

	debug3_f("provider %s: %zu remaining keys",
	    helper->path, helper->nkeyblobs);

	/* success */
	*certprivp = ret;
	return 0;
}

static struct helper *
pkcs11_start_helper(const char *path)
{
	int pair[2];
	char *prog, *verbosity = NULL;
	struct helper *helper;
	pid_t pid;

	if (nhelpers >= INT_MAX)
		fatal_f("too many helpers");
	debug3_f("start helper for %s", path);
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1) {
		error_f("socketpair: %s", strerror(errno));
		return NULL;
	}
	helper = xcalloc(1, sizeof(*helper));
	if ((pid = fork()) == -1) {
		error_f("fork: %s", strerror(errno));
		close(pair[0]);
		close(pair[1]);
		free(helper);
		return NULL;
	} else if (pid == 0) {
		if ((dup2(pair[1], STDIN_FILENO) == -1) ||
		    (dup2(pair[1], STDOUT_FILENO) == -1)) {
			fprintf(stderr, "dup2: %s\n", strerror(errno));
			_exit(1);
		}
		close(pair[0]);
		close(pair[1]);
		prog = getenv("SSH_PKCS11_HELPER");
		if (prog == NULL || strlen(prog) == 0)
			prog = _PATH_SSH_PKCS11_HELPER;
		if (log_level_get() >= SYSLOG_LEVEL_DEBUG1)
			verbosity = "-vvv";
		debug_f("starting %s %s", prog,
		    verbosity == NULL ? "" : verbosity);
		execlp(prog, prog, verbosity, (char *)NULL);
		fprintf(stderr, "exec: %s: %s\n", prog, strerror(errno));
		_exit(1);
	}
	close(pair[1]);
	helper->fd = pair[0];
	helper->path = xstrdup(path);
	helper->pid = pid;
	debug3_f("helper %zu for \"%s\" on fd %d pid %ld", nhelpers,
	    helper->path, helper->fd, (long)helper->pid);
	helpers = xrecallocarray(helpers, nhelpers,
	    nhelpers + 1, sizeof(*helpers));
	helpers[nhelpers++] = helper;
	return helper;
}

int
pkcs11_add_provider(char *name, char *pin, struct sshkey ***keysp,
    char ***labelsp)
{
	struct sshkey *k;
	int r, type;
	char *label;
	u_int ret = -1, nkeys, i;
	struct sshbuf *msg;
	struct helper *helper;

	if ((helper = helper_by_provider(name)) == NULL &&
	    (helper = pkcs11_start_helper(name)) == NULL)
		return -1;

	debug3_f("add %s", helper->path);

	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_put_u8(msg, SSH_AGENTC_ADD_SMARTCARD_KEY)) != 0 ||
	    (r = sshbuf_put_cstring(msg, name)) != 0 ||
	    (r = sshbuf_put_cstring(msg, pin)) != 0)
		fatal_fr(r, "compose");
	send_msg(helper->fd, msg);
	sshbuf_reset(msg);

	type = recv_msg(helper->fd, msg);
	debug3_f("response %d", type);
	if (type == SSH2_AGENT_IDENTITIES_ANSWER) {
		if ((r = sshbuf_get_u32(msg, &nkeys)) != 0)
			fatal_fr(r, "parse nkeys");
		debug3_f("helper return %u keys", nkeys);
		*keysp = xcalloc(nkeys, sizeof(struct sshkey *));
		if (labelsp)
			*labelsp = xcalloc(nkeys, sizeof(char *));
		for (i = 0; i < nkeys; i++) {
			/* XXX clean up properly instead of fatal() */
			if ((r = sshkey_froms(msg, &k)) != 0 ||
			    (r = sshbuf_get_cstring(msg, &label, NULL)) != 0)
				fatal_fr(r, "parse key");
			k->flags |= SSHKEY_FLAG_EXT;
			helper_add_key(helper, k);
			(*keysp)[i] = k;
			if (labelsp)
				(*labelsp)[i] = label;
			else
				free(label);
		}
		/* success */
		ret = 0;
	} else if (type == SSH2_AGENT_FAILURE) {
		if ((r = sshbuf_get_u32(msg, &nkeys)) != 0)
			error_fr(r, "failed to parse failure response");
	}
	if (ret != 0) {
		debug_f("no keys; terminate helper");
		helper_terminate(helper);
	}
	sshbuf_free(msg);
	return ret == 0 ? (int)nkeys : -1;
}

int
pkcs11_del_provider(char *name)
{
	struct helper *helper;

	/*
	 * ssh-agent deletes keys before calling this, so the helper entry
	 * should be gone before we get here.
	 */
	debug3_f("delete %s", name ? name : "(null)");
	if ((helper = helper_by_provider(name)) != NULL)
		helper_terminate(helper);
	return 0;
}

void
pkcs11_key_free(struct sshkey *key)
{
	struct helper *helper;
	struct sshbuf *keyblob = NULL;
	size_t i;
	int r, found = 0;

	debug3_f("free %s key", sshkey_type(key));

	if ((helper = helper_by_key(key)) == NULL || helper->fd == -1)
		fatal_f("no helper for %s key", sshkey_type(key));
	if ((keyblob = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshkey_putb(key, keyblob)) != 0)
		fatal_fr(r, "serialise key");

	/* repack keys */
	for (i = 0; i < helper->nkeyblobs; i++) {
		if (sshbuf_equals(keyblob, helper->keyblobs[i]) == 0) {
			if (found)
				fatal_f("key recorded more than once");
			found = 1;
		} else if (found)
			helper->keyblobs[i - 1] = helper->keyblobs[i];
	}
	if (found) {
		helper->keyblobs = xrecallocarray(helper->keyblobs,
		    helper->nkeyblobs, helper->nkeyblobs - 1,
		    sizeof(*helper->keyblobs));
		helper->nkeyblobs--;
	}
	if (helper->nkeyblobs == 0)
		helper_terminate(helper);
}
