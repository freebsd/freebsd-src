/* $OpenBSD: ssh-agent.c,v 1.204 2015/07/08 20:24:02 markus Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * The authentication agent program.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
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
__RCSID("$FreeBSD$");

#include <sys/param.h>	/* MIN MAX */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#include "openbsd-compat/sys-queue.h"

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include "openbsd-compat/openssl-compat.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_UTIL_H
# include <util.h>
#endif

#include "xmalloc.h"
#include "ssh.h"
#include "rsa.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "authfd.h"
#include "compat.h"
#include "log.h"
#include "misc.h"
#include "digest.h"
#include "ssherr.h"

#ifdef ENABLE_PKCS11
#include "ssh-pkcs11.h"
#endif

#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>	/* For prctl() and PR_SET_DUMPABLE */
#endif

typedef enum {
	AUTH_UNUSED,
	AUTH_SOCKET,
	AUTH_CONNECTION
} sock_type;

typedef struct {
	int fd;
	sock_type type;
	struct sshbuf *input;
	struct sshbuf *output;
	struct sshbuf *request;
} SocketEntry;

u_int sockets_alloc = 0;
SocketEntry *sockets = NULL;

typedef struct identity {
	TAILQ_ENTRY(identity) next;
	struct sshkey *key;
	char *comment;
	char *provider;
	time_t death;
	u_int confirm;
} Identity;

typedef struct {
	int nentries;
	TAILQ_HEAD(idqueue, identity) idlist;
} Idtab;

/* private key table, one per protocol version */
Idtab idtable[3];

int max_fd = 0;

/* pid of shell == parent of agent */
pid_t parent_pid = -1;
time_t parent_alive_interval = 0;

/* pid of process for which cleanup_socket is applicable */
pid_t cleanup_pid = 0;

/* pathname and directory for AUTH_SOCKET */
char socket_name[PATH_MAX];
char socket_dir[PATH_MAX];

/* locking */
#define LOCK_SIZE	32
#define LOCK_SALT_SIZE	16
#define LOCK_ROUNDS	1
int locked = 0;
char lock_passwd[LOCK_SIZE];
char lock_salt[LOCK_SALT_SIZE];

extern char *__progname;

/* Default lifetime in seconds (0 == forever) */
static long lifetime = 0;

static int fingerprint_hash = SSH_FP_HASH_DEFAULT;

/*
 * Client connection count; incremented in new_socket() and decremented in
 * close_socket().  When it reaches 0, ssh-agent will exit.  Since it is
 * normally initialized to 1, it will never reach 0.  However, if the -x
 * option is specified, it is initialized to 0 in main(); in that case,
 * ssh-agent will exit as soon as it has had at least one client but no
 * longer has any.
 */
static int xcount = 1;

static void
close_socket(SocketEntry *e)
{
	int last = 0;

	if (e->type == AUTH_CONNECTION) {
		debug("xcount %d -> %d", xcount, xcount - 1);
		if (--xcount == 0)
			last = 1;
	}
	close(e->fd);
	e->fd = -1;
	e->type = AUTH_UNUSED;
	sshbuf_free(e->input);
	sshbuf_free(e->output);
	sshbuf_free(e->request);
	if (last)
		cleanup_exit(0);
}

static void
idtab_init(void)
{
	int i;

	for (i = 0; i <=2; i++) {
		TAILQ_INIT(&idtable[i].idlist);
		idtable[i].nentries = 0;
	}
}

/* return private key table for requested protocol version */
static Idtab *
idtab_lookup(int version)
{
	if (version < 1 || version > 2)
		fatal("internal error, bad protocol version %d", version);
	return &idtable[version];
}

static void
free_identity(Identity *id)
{
	sshkey_free(id->key);
	free(id->provider);
	free(id->comment);
	free(id);
}

/* return matching private key for given public key */
static Identity *
lookup_identity(struct sshkey *key, int version)
{
	Identity *id;

	Idtab *tab = idtab_lookup(version);
	TAILQ_FOREACH(id, &tab->idlist, next) {
		if (sshkey_equal(key, id->key))
			return (id);
	}
	return (NULL);
}

/* Check confirmation of keysign request */
static int
confirm_key(Identity *id)
{
	char *p;
	int ret = -1;

	p = sshkey_fingerprint(id->key, fingerprint_hash, SSH_FP_DEFAULT);
	if (p != NULL &&
	    ask_permission("Allow use of key %s?\nKey fingerprint %s.",
	    id->comment, p))
		ret = 0;
	free(p);

	return (ret);
}

static void
send_status(SocketEntry *e, int success)
{
	int r;

	if ((r = sshbuf_put_u32(e->output, 1)) != 0 ||
	    (r = sshbuf_put_u8(e->output, success ?
	    SSH_AGENT_SUCCESS : SSH_AGENT_FAILURE)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
}

/* send list of supported public keys to 'client' */
static void
process_request_identities(SocketEntry *e, int version)
{
	Idtab *tab = idtab_lookup(version);
	Identity *id;
	struct sshbuf *msg;
	int r;

	if ((msg = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((r = sshbuf_put_u8(msg, (version == 1) ?
	    SSH_AGENT_RSA_IDENTITIES_ANSWER :
	    SSH2_AGENT_IDENTITIES_ANSWER)) != 0 ||
	    (r = sshbuf_put_u32(msg, tab->nentries)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	TAILQ_FOREACH(id, &tab->idlist, next) {
		if (id->key->type == KEY_RSA1) {
#ifdef WITH_SSH1
			if ((r = sshbuf_put_u32(msg,
			    BN_num_bits(id->key->rsa->n))) != 0 ||
			    (r = sshbuf_put_bignum1(msg,
			    id->key->rsa->e)) != 0 ||
			    (r = sshbuf_put_bignum1(msg,
			    id->key->rsa->n)) != 0)
				fatal("%s: buffer error: %s",
				    __func__, ssh_err(r));
#endif
		} else {
			u_char *blob;
			size_t blen;

			if ((r = sshkey_to_blob(id->key, &blob, &blen)) != 0) {
				error("%s: sshkey_to_blob: %s", __func__,
				    ssh_err(r));
				continue;
			}
			if ((r = sshbuf_put_string(msg, blob, blen)) != 0)
				fatal("%s: buffer error: %s",
				    __func__, ssh_err(r));
			free(blob);
		}
		if ((r = sshbuf_put_cstring(msg, id->comment)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
	}
	if ((r = sshbuf_put_stringb(e->output, msg)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	sshbuf_free(msg);
}

#ifdef WITH_SSH1
/* ssh1 only */
static void
process_authentication_challenge1(SocketEntry *e)
{
	u_char buf[32], mdbuf[16], session_id[16];
	u_int response_type;
	BIGNUM *challenge;
	Identity *id;
	int r, len;
	struct sshbuf *msg;
	struct ssh_digest_ctx *md;
	struct sshkey *key;

	if ((msg = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((key = sshkey_new(KEY_RSA1)) == NULL)
		fatal("%s: sshkey_new failed", __func__);
	if ((challenge = BN_new()) == NULL)
		fatal("%s: BN_new failed", __func__);

	if ((r = sshbuf_get_u32(e->request, NULL)) != 0 || /* ignored */
	    (r = sshbuf_get_bignum1(e->request, key->rsa->e)) != 0 ||
	    (r = sshbuf_get_bignum1(e->request, key->rsa->n)) != 0 ||
	    (r = sshbuf_get_bignum1(e->request, challenge)))
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	/* Only protocol 1.1 is supported */
	if (sshbuf_len(e->request) == 0)
		goto failure;
	if ((r = sshbuf_get(e->request, session_id, sizeof(session_id))) != 0 ||
	    (r = sshbuf_get_u32(e->request, &response_type)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	if (response_type != 1)
		goto failure;

	id = lookup_identity(key, 1);
	if (id != NULL && (!id->confirm || confirm_key(id) == 0)) {
		struct sshkey *private = id->key;
		/* Decrypt the challenge using the private key. */
		if ((r = rsa_private_decrypt(challenge, challenge,
		    private->rsa) != 0)) {
			fatal("%s: rsa_public_encrypt: %s", __func__,
			    ssh_err(r));
			goto failure;	/* XXX ? */
		}

		/* The response is MD5 of decrypted challenge plus session id */
		len = BN_num_bytes(challenge);
		if (len <= 0 || len > 32) {
			logit("%s: bad challenge length %d", __func__, len);
			goto failure;
		}
		memset(buf, 0, 32);
		BN_bn2bin(challenge, buf + 32 - len);
		if ((md = ssh_digest_start(SSH_DIGEST_MD5)) == NULL ||
		    ssh_digest_update(md, buf, 32) < 0 ||
		    ssh_digest_update(md, session_id, 16) < 0 ||
		    ssh_digest_final(md, mdbuf, sizeof(mdbuf)) < 0)
			fatal("%s: md5 failed", __func__);
		ssh_digest_free(md);

		/* Send the response. */
		if ((r = sshbuf_put_u8(msg, SSH_AGENT_RSA_RESPONSE)) != 0 ||
		    (r = sshbuf_put(msg, mdbuf, sizeof(mdbuf))) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
		goto send;
	}

 failure:
	/* Unknown identity or protocol error.  Send failure. */
	if ((r = sshbuf_put_u8(msg, SSH_AGENT_FAILURE)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
 send:
	if ((r = sshbuf_put_stringb(e->output, msg)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	sshkey_free(key);
	BN_clear_free(challenge);
	sshbuf_free(msg);
}
#endif

/* ssh2 only */
static void
process_sign_request2(SocketEntry *e)
{
	u_char *blob, *data, *signature = NULL;
	size_t blen, dlen, slen = 0;
	u_int compat = 0, flags;
	int r, ok = -1;
	struct sshbuf *msg;
	struct sshkey *key;
	struct identity *id;

	if ((msg = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((r = sshbuf_get_string(e->request, &blob, &blen)) != 0 ||
	    (r = sshbuf_get_string(e->request, &data, &dlen)) != 0 ||
	    (r = sshbuf_get_u32(e->request, &flags)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	if (flags & SSH_AGENT_OLD_SIGNATURE)
		compat = SSH_BUG_SIGBLOB;
	if ((r = sshkey_from_blob(blob, blen, &key)) != 0) {
		error("%s: cannot parse key blob: %s", __func__, ssh_err(ok));
		goto send;
	}
	if ((id = lookup_identity(key, 2)) == NULL) {
		verbose("%s: %s key not found", __func__, sshkey_type(key));
		goto send;
	}
	if (id->confirm && confirm_key(id) != 0) {
		verbose("%s: user refused key", __func__);
		goto send;
	}
	if ((r = sshkey_sign(id->key, &signature, &slen,
	    data, dlen, compat)) != 0) {
		error("%s: sshkey_sign: %s", __func__, ssh_err(ok));
		goto send;
	}
	/* Success */
	ok = 0;
 send:
	sshkey_free(key);
	if (ok == 0) {
		if ((r = sshbuf_put_u8(msg, SSH2_AGENT_SIGN_RESPONSE)) != 0 ||
		    (r = sshbuf_put_string(msg, signature, slen)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
	} else if ((r = sshbuf_put_u8(msg, SSH_AGENT_FAILURE)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	if ((r = sshbuf_put_stringb(e->output, msg)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	sshbuf_free(msg);
	free(data);
	free(blob);
	free(signature);
}

/* shared */
static void
process_remove_identity(SocketEntry *e, int version)
{
	size_t blen;
	int r, success = 0;
	struct sshkey *key = NULL;
	u_char *blob;
#ifdef WITH_SSH1
	u_int bits;
#endif /* WITH_SSH1 */

	switch (version) {
#ifdef WITH_SSH1
	case 1:
		if ((key = sshkey_new(KEY_RSA1)) == NULL) {
			error("%s: sshkey_new failed", __func__);
			return;
		}
		if ((r = sshbuf_get_u32(e->request, &bits)) != 0 ||
		    (r = sshbuf_get_bignum1(e->request, key->rsa->e)) != 0 ||
		    (r = sshbuf_get_bignum1(e->request, key->rsa->n)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));

		if (bits != sshkey_size(key))
			logit("Warning: identity keysize mismatch: "
			    "actual %u, announced %u",
			    sshkey_size(key), bits);
		break;
#endif /* WITH_SSH1 */
	case 2:
		if ((r = sshbuf_get_string(e->request, &blob, &blen)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
		if ((r = sshkey_from_blob(blob, blen, &key)) != 0)
			error("%s: sshkey_from_blob failed: %s",
			    __func__, ssh_err(r));
		free(blob);
		break;
	}
	if (key != NULL) {
		Identity *id = lookup_identity(key, version);
		if (id != NULL) {
			/*
			 * We have this key.  Free the old key.  Since we
			 * don't want to leave empty slots in the middle of
			 * the array, we actually free the key there and move
			 * all the entries between the empty slot and the end
			 * of the array.
			 */
			Idtab *tab = idtab_lookup(version);
			if (tab->nentries < 1)
				fatal("process_remove_identity: "
				    "internal error: tab->nentries %d",
				    tab->nentries);
			TAILQ_REMOVE(&tab->idlist, id, next);
			free_identity(id);
			tab->nentries--;
			success = 1;
		}
		sshkey_free(key);
	}
	send_status(e, success);
}

static void
process_remove_all_identities(SocketEntry *e, int version)
{
	Idtab *tab = idtab_lookup(version);
	Identity *id;

	/* Loop over all identities and clear the keys. */
	for (id = TAILQ_FIRST(&tab->idlist); id;
	    id = TAILQ_FIRST(&tab->idlist)) {
		TAILQ_REMOVE(&tab->idlist, id, next);
		free_identity(id);
	}

	/* Mark that there are no identities. */
	tab->nentries = 0;

	/* Send success. */
	send_status(e, 1);
}

/* removes expired keys and returns number of seconds until the next expiry */
static time_t
reaper(void)
{
	time_t deadline = 0, now = monotime();
	Identity *id, *nxt;
	int version;
	Idtab *tab;

	for (version = 1; version < 3; version++) {
		tab = idtab_lookup(version);
		for (id = TAILQ_FIRST(&tab->idlist); id; id = nxt) {
			nxt = TAILQ_NEXT(id, next);
			if (id->death == 0)
				continue;
			if (now >= id->death) {
				debug("expiring key '%s'", id->comment);
				TAILQ_REMOVE(&tab->idlist, id, next);
				free_identity(id);
				tab->nentries--;
			} else
				deadline = (deadline == 0) ? id->death :
				    MIN(deadline, id->death);
		}
	}
	if (deadline == 0 || deadline <= now)
		return 0;
	else
		return (deadline - now);
}

/*
 * XXX this and the corresponding serialisation function probably belongs
 * in key.c
 */
#ifdef WITH_SSH1
static int
agent_decode_rsa1(struct sshbuf *m, struct sshkey **kp)
{
	struct sshkey *k = NULL;
	int r = SSH_ERR_INTERNAL_ERROR;

	*kp = NULL;
	if ((k = sshkey_new_private(KEY_RSA1)) == NULL)
		return SSH_ERR_ALLOC_FAIL;

	if ((r = sshbuf_get_u32(m, NULL)) != 0 ||		/* ignored */
	    (r = sshbuf_get_bignum1(m, k->rsa->n)) != 0 ||
	    (r = sshbuf_get_bignum1(m, k->rsa->e)) != 0 ||
	    (r = sshbuf_get_bignum1(m, k->rsa->d)) != 0 ||
	    (r = sshbuf_get_bignum1(m, k->rsa->iqmp)) != 0 ||
	    /* SSH1 and SSL have p and q swapped */
	    (r = sshbuf_get_bignum1(m, k->rsa->q)) != 0 ||	/* p */
	    (r = sshbuf_get_bignum1(m, k->rsa->p)) != 0) 	/* q */
		goto out;

	/* Generate additional parameters */
	if ((r = rsa_generate_additional_parameters(k->rsa)) != 0)
		goto out;
	/* enable blinding */
	if (RSA_blinding_on(k->rsa, NULL) != 1) {
		r = SSH_ERR_LIBCRYPTO_ERROR;
		goto out;
	}

	r = 0; /* success */
 out:
	if (r == 0)
		*kp = k;
	else
		sshkey_free(k);
	return r;
}
#endif /* WITH_SSH1 */

static void
process_add_identity(SocketEntry *e, int version)
{
	Idtab *tab = idtab_lookup(version);
	Identity *id;
	int success = 0, confirm = 0;
	u_int seconds;
	char *comment = NULL;
	time_t death = 0;
	struct sshkey *k = NULL;
	u_char ctype;
	int r = SSH_ERR_INTERNAL_ERROR;

	switch (version) {
#ifdef WITH_SSH1
	case 1:
		r = agent_decode_rsa1(e->request, &k);
		break;
#endif /* WITH_SSH1 */
	case 2:
		r = sshkey_private_deserialize(e->request, &k);
		break;
	}
	if (r != 0 || k == NULL ||
	    (r = sshbuf_get_cstring(e->request, &comment, NULL)) != 0) {
		error("%s: decode private key: %s", __func__, ssh_err(r));
		goto err;
	}

	while (sshbuf_len(e->request)) {
		if ((r = sshbuf_get_u8(e->request, &ctype)) != 0) {
			error("%s: buffer error: %s", __func__, ssh_err(r));
			goto err;
		}
		switch (ctype) {
		case SSH_AGENT_CONSTRAIN_LIFETIME:
			if ((r = sshbuf_get_u32(e->request, &seconds)) != 0) {
				error("%s: bad lifetime constraint: %s",
				    __func__, ssh_err(r));
				goto err;
			}
			death = monotime() + seconds;
			break;
		case SSH_AGENT_CONSTRAIN_CONFIRM:
			confirm = 1;
			break;
		default:
			error("%s: Unknown constraint %d", __func__, ctype);
 err:
			sshbuf_reset(e->request);
			free(comment);
			sshkey_free(k);
			goto send;
		}
	}

	success = 1;
	if (lifetime && !death)
		death = monotime() + lifetime;
	if ((id = lookup_identity(k, version)) == NULL) {
		id = xcalloc(1, sizeof(Identity));
		id->key = k;
		TAILQ_INSERT_TAIL(&tab->idlist, id, next);
		/* Increment the number of identities. */
		tab->nentries++;
	} else {
		sshkey_free(k);
		free(id->comment);
	}
	id->comment = comment;
	id->death = death;
	id->confirm = confirm;
send:
	send_status(e, success);
}

/* XXX todo: encrypt sensitive data with passphrase */
static void
process_lock_agent(SocketEntry *e, int lock)
{
	int r, success = 0, delay;
	char *passwd, passwdhash[LOCK_SIZE];
	static u_int fail_count = 0;
	size_t pwlen;

	if ((r = sshbuf_get_cstring(e->request, &passwd, &pwlen)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	if (pwlen == 0) {
		debug("empty password not supported");
	} else if (locked && !lock) {
		if (bcrypt_pbkdf(passwd, pwlen, lock_salt, sizeof(lock_salt),
		    passwdhash, sizeof(passwdhash), LOCK_ROUNDS) < 0)
			fatal("bcrypt_pbkdf");
		if (timingsafe_bcmp(passwdhash, lock_passwd, LOCK_SIZE) == 0) {
			debug("agent unlocked");
			locked = 0;
			fail_count = 0;
			explicit_bzero(lock_passwd, sizeof(lock_passwd));
			success = 1;
		} else {
			/* delay in 0.1s increments up to 10s */
			if (fail_count < 100)
				fail_count++;
			delay = 100000 * fail_count;
			debug("unlock failed, delaying %0.1lf seconds",
			    (double)delay/1000000);
			usleep(delay);
		}
		explicit_bzero(passwdhash, sizeof(passwdhash));
	} else if (!locked && lock) {
		debug("agent locked");
		locked = 1;
		arc4random_buf(lock_salt, sizeof(lock_salt));
		if (bcrypt_pbkdf(passwd, pwlen, lock_salt, sizeof(lock_salt),
		    lock_passwd, sizeof(lock_passwd), LOCK_ROUNDS) < 0)
			fatal("bcrypt_pbkdf");
		success = 1;
	}
	explicit_bzero(passwd, pwlen);
	free(passwd);
	send_status(e, success);
}

static void
no_identities(SocketEntry *e, u_int type)
{
	struct sshbuf *msg;
	int r;

	if ((msg = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((r = sshbuf_put_u8(msg,
	    (type == SSH_AGENTC_REQUEST_RSA_IDENTITIES) ?
	    SSH_AGENT_RSA_IDENTITIES_ANSWER :
	    SSH2_AGENT_IDENTITIES_ANSWER)) != 0 ||
	    (r = sshbuf_put_u32(msg, 0)) != 0 ||
	    (r = sshbuf_put_stringb(e->output, msg)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	sshbuf_free(msg);
}

#ifdef ENABLE_PKCS11
static void
process_add_smartcard_key(SocketEntry *e)
{
	char *provider = NULL, *pin;
	int r, i, version, count = 0, success = 0, confirm = 0;
	u_int seconds;
	time_t death = 0;
	u_char type;
	struct sshkey **keys = NULL, *k;
	Identity *id;
	Idtab *tab;

	if ((r = sshbuf_get_cstring(e->request, &provider, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(e->request, &pin, NULL)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	while (sshbuf_len(e->request)) {
		if ((r = sshbuf_get_u8(e->request, &type)) != 0)
			fatal("%s: buffer error: %s", __func__, ssh_err(r));
		switch (type) {
		case SSH_AGENT_CONSTRAIN_LIFETIME:
			if ((r = sshbuf_get_u32(e->request, &seconds)) != 0)
				fatal("%s: buffer error: %s",
				    __func__, ssh_err(r));
			death = monotime() + seconds;
			break;
		case SSH_AGENT_CONSTRAIN_CONFIRM:
			confirm = 1;
			break;
		default:
			error("process_add_smartcard_key: "
			    "Unknown constraint type %d", type);
			goto send;
		}
	}
	if (lifetime && !death)
		death = monotime() + lifetime;

	count = pkcs11_add_provider(provider, pin, &keys);
	for (i = 0; i < count; i++) {
		k = keys[i];
		version = k->type == KEY_RSA1 ? 1 : 2;
		tab = idtab_lookup(version);
		if (lookup_identity(k, version) == NULL) {
			id = xcalloc(1, sizeof(Identity));
			id->key = k;
			id->provider = xstrdup(provider);
			id->comment = xstrdup(provider); /* XXX */
			id->death = death;
			id->confirm = confirm;
			TAILQ_INSERT_TAIL(&tab->idlist, id, next);
			tab->nentries++;
			success = 1;
		} else {
			sshkey_free(k);
		}
		keys[i] = NULL;
	}
send:
	free(pin);
	free(provider);
	free(keys);
	send_status(e, success);
}

static void
process_remove_smartcard_key(SocketEntry *e)
{
	char *provider = NULL, *pin = NULL;
	int r, version, success = 0;
	Identity *id, *nxt;
	Idtab *tab;

	if ((r = sshbuf_get_cstring(e->request, &provider, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(e->request, &pin, NULL)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	free(pin);

	for (version = 1; version < 3; version++) {
		tab = idtab_lookup(version);
		for (id = TAILQ_FIRST(&tab->idlist); id; id = nxt) {
			nxt = TAILQ_NEXT(id, next);
			/* Skip file--based keys */
			if (id->provider == NULL)
				continue;
			if (!strcmp(provider, id->provider)) {
				TAILQ_REMOVE(&tab->idlist, id, next);
				free_identity(id);
				tab->nentries--;
			}
		}
	}
	if (pkcs11_del_provider(provider) == 0)
		success = 1;
	else
		error("process_remove_smartcard_key:"
		    " pkcs11_del_provider failed");
	free(provider);
	send_status(e, success);
}
#endif /* ENABLE_PKCS11 */

/* dispatch incoming messages */

static void
process_message(SocketEntry *e)
{
	u_int msg_len;
	u_char type;
	const u_char *cp;
	int r;

	if (sshbuf_len(e->input) < 5)
		return;		/* Incomplete message. */
	cp = sshbuf_ptr(e->input);
	msg_len = PEEK_U32(cp);
	if (msg_len > 256 * 1024) {
		close_socket(e);
		return;
	}
	if (sshbuf_len(e->input) < msg_len + 4)
		return;

	/* move the current input to e->request */
	sshbuf_reset(e->request);
	if ((r = sshbuf_get_stringb(e->input, e->request)) != 0 ||
	    (r = sshbuf_get_u8(e->request, &type)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	/* check wheter agent is locked */
	if (locked && type != SSH_AGENTC_UNLOCK) {
		sshbuf_reset(e->request);
		switch (type) {
		case SSH_AGENTC_REQUEST_RSA_IDENTITIES:
		case SSH2_AGENTC_REQUEST_IDENTITIES:
			/* send empty lists */
			no_identities(e, type);
			break;
		default:
			/* send a fail message for all other request types */
			send_status(e, 0);
		}
		return;
	}

	debug("type %d", type);
	switch (type) {
	case SSH_AGENTC_LOCK:
	case SSH_AGENTC_UNLOCK:
		process_lock_agent(e, type == SSH_AGENTC_LOCK);
		break;
#ifdef WITH_SSH1
	/* ssh1 */
	case SSH_AGENTC_RSA_CHALLENGE:
		process_authentication_challenge1(e);
		break;
	case SSH_AGENTC_REQUEST_RSA_IDENTITIES:
		process_request_identities(e, 1);
		break;
	case SSH_AGENTC_ADD_RSA_IDENTITY:
	case SSH_AGENTC_ADD_RSA_ID_CONSTRAINED:
		process_add_identity(e, 1);
		break;
	case SSH_AGENTC_REMOVE_RSA_IDENTITY:
		process_remove_identity(e, 1);
		break;
#endif
	case SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES:
		process_remove_all_identities(e, 1); /* safe for !WITH_SSH1 */
		break;
	/* ssh2 */
	case SSH2_AGENTC_SIGN_REQUEST:
		process_sign_request2(e);
		break;
	case SSH2_AGENTC_REQUEST_IDENTITIES:
		process_request_identities(e, 2);
		break;
	case SSH2_AGENTC_ADD_IDENTITY:
	case SSH2_AGENTC_ADD_ID_CONSTRAINED:
		process_add_identity(e, 2);
		break;
	case SSH2_AGENTC_REMOVE_IDENTITY:
		process_remove_identity(e, 2);
		break;
	case SSH2_AGENTC_REMOVE_ALL_IDENTITIES:
		process_remove_all_identities(e, 2);
		break;
#ifdef ENABLE_PKCS11
	case SSH_AGENTC_ADD_SMARTCARD_KEY:
	case SSH_AGENTC_ADD_SMARTCARD_KEY_CONSTRAINED:
		process_add_smartcard_key(e);
		break;
	case SSH_AGENTC_REMOVE_SMARTCARD_KEY:
		process_remove_smartcard_key(e);
		break;
#endif /* ENABLE_PKCS11 */
	default:
		/* Unknown message.  Respond with failure. */
		error("Unknown message %d", type);
		sshbuf_reset(e->request);
		send_status(e, 0);
		break;
	}
}

static void
new_socket(sock_type type, int fd)
{
	u_int i, old_alloc, new_alloc;

	if (type == AUTH_CONNECTION) {
		debug("xcount %d -> %d", xcount, xcount + 1);
		++xcount;
	}
	set_nonblock(fd);

	if (fd > max_fd)
		max_fd = fd;

	for (i = 0; i < sockets_alloc; i++)
		if (sockets[i].type == AUTH_UNUSED) {
			sockets[i].fd = fd;
			if ((sockets[i].input = sshbuf_new()) == NULL)
				fatal("%s: sshbuf_new failed", __func__);
			if ((sockets[i].output = sshbuf_new()) == NULL)
				fatal("%s: sshbuf_new failed", __func__);
			if ((sockets[i].request = sshbuf_new()) == NULL)
				fatal("%s: sshbuf_new failed", __func__);
			sockets[i].type = type;
			return;
		}
	old_alloc = sockets_alloc;
	new_alloc = sockets_alloc + 10;
	sockets = xreallocarray(sockets, new_alloc, sizeof(sockets[0]));
	for (i = old_alloc; i < new_alloc; i++)
		sockets[i].type = AUTH_UNUSED;
	sockets_alloc = new_alloc;
	sockets[old_alloc].fd = fd;
	if ((sockets[old_alloc].input = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((sockets[old_alloc].output = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	if ((sockets[old_alloc].request = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __func__);
	sockets[old_alloc].type = type;
}

static int
prepare_select(fd_set **fdrp, fd_set **fdwp, int *fdl, u_int *nallocp,
    struct timeval **tvpp)
{
	u_int i, sz;
	int n = 0;
	static struct timeval tv;
	time_t deadline;

	for (i = 0; i < sockets_alloc; i++) {
		switch (sockets[i].type) {
		case AUTH_SOCKET:
		case AUTH_CONNECTION:
			n = MAX(n, sockets[i].fd);
			break;
		case AUTH_UNUSED:
			break;
		default:
			fatal("Unknown socket type %d", sockets[i].type);
			break;
		}
	}

	sz = howmany(n+1, NFDBITS) * sizeof(fd_mask);
	if (*fdrp == NULL || sz > *nallocp) {
		free(*fdrp);
		free(*fdwp);
		*fdrp = xmalloc(sz);
		*fdwp = xmalloc(sz);
		*nallocp = sz;
	}
	if (n < *fdl)
		debug("XXX shrink: %d < %d", n, *fdl);
	*fdl = n;
	memset(*fdrp, 0, sz);
	memset(*fdwp, 0, sz);

	for (i = 0; i < sockets_alloc; i++) {
		switch (sockets[i].type) {
		case AUTH_SOCKET:
		case AUTH_CONNECTION:
			FD_SET(sockets[i].fd, *fdrp);
			if (sshbuf_len(sockets[i].output) > 0)
				FD_SET(sockets[i].fd, *fdwp);
			break;
		default:
			break;
		}
	}
	deadline = reaper();
	if (parent_alive_interval != 0)
		deadline = (deadline == 0) ? parent_alive_interval :
		    MIN(deadline, parent_alive_interval);
	if (deadline == 0) {
		*tvpp = NULL;
	} else {
		tv.tv_sec = deadline;
		tv.tv_usec = 0;
		*tvpp = &tv;
	}
	return (1);
}

static void
after_select(fd_set *readset, fd_set *writeset)
{
	struct sockaddr_un sunaddr;
	socklen_t slen;
	char buf[1024];
	int len, sock, r;
	u_int i, orig_alloc;
	uid_t euid;
	gid_t egid;

	for (i = 0, orig_alloc = sockets_alloc; i < orig_alloc; i++)
		switch (sockets[i].type) {
		case AUTH_UNUSED:
			break;
		case AUTH_SOCKET:
			if (FD_ISSET(sockets[i].fd, readset)) {
				slen = sizeof(sunaddr);
				sock = accept(sockets[i].fd,
				    (struct sockaddr *)&sunaddr, &slen);
				if (sock < 0) {
					error("accept from AUTH_SOCKET: %s",
					    strerror(errno));
					break;
				}
				if (getpeereid(sock, &euid, &egid) < 0) {
					error("getpeereid %d failed: %s",
					    sock, strerror(errno));
					close(sock);
					break;
				}
				if ((euid != 0) && (getuid() != euid)) {
					error("uid mismatch: "
					    "peer euid %u != uid %u",
					    (u_int) euid, (u_int) getuid());
					close(sock);
					break;
				}
				new_socket(AUTH_CONNECTION, sock);
			}
			break;
		case AUTH_CONNECTION:
			if (sshbuf_len(sockets[i].output) > 0 &&
			    FD_ISSET(sockets[i].fd, writeset)) {
				len = write(sockets[i].fd,
				    sshbuf_ptr(sockets[i].output),
				    sshbuf_len(sockets[i].output));
				if (len == -1 && (errno == EAGAIN ||
				    errno == EWOULDBLOCK ||
				    errno == EINTR))
					continue;
				if (len <= 0) {
					close_socket(&sockets[i]);
					break;
				}
				if ((r = sshbuf_consume(sockets[i].output,
				    len)) != 0)
					fatal("%s: buffer error: %s",
					    __func__, ssh_err(r));
			}
			if (FD_ISSET(sockets[i].fd, readset)) {
				len = read(sockets[i].fd, buf, sizeof(buf));
				if (len == -1 && (errno == EAGAIN ||
				    errno == EWOULDBLOCK ||
				    errno == EINTR))
					continue;
				if (len <= 0) {
					close_socket(&sockets[i]);
					break;
				}
				if ((r = sshbuf_put(sockets[i].input,
				    buf, len)) != 0)
					fatal("%s: buffer error: %s",
					    __func__, ssh_err(r));
				explicit_bzero(buf, sizeof(buf));
				process_message(&sockets[i]);
			}
			break;
		default:
			fatal("Unknown type %d", sockets[i].type);
		}
}

static void
cleanup_socket(void)
{
	if (cleanup_pid != 0 && getpid() != cleanup_pid)
		return;
	debug("%s: cleanup", __func__);
	if (socket_name[0])
		unlink(socket_name);
	if (socket_dir[0])
		rmdir(socket_dir);
}

void
cleanup_exit(int i)
{
	cleanup_socket();
	_exit(i);
}

/*ARGSUSED*/
static void
cleanup_handler(int sig)
{
	cleanup_socket();
#ifdef ENABLE_PKCS11
	pkcs11_terminate();
#endif
	_exit(2);
}

static void
check_parent_exists(void)
{
	/*
	 * If our parent has exited then getppid() will return (pid_t)1,
	 * so testing for that should be safe.
	 */
	if (parent_pid != -1 && getppid() != parent_pid) {
		/* printf("Parent has died - Authentication agent exiting.\n"); */
		cleanup_socket();
		_exit(2);
	}
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: ssh-agent [-c | -s] [-Dd] [-a bind_address] [-E fingerprint_hash]\n"
	    "                 [-t life] [command [arg ...]]\n"
	    "       ssh-agent [-c | -s] -k\n");
	fprintf(stderr, "  -x          Exit when the last client disconnects.\n");
	exit(1);
}

int
main(int ac, char **av)
{
	int c_flag = 0, d_flag = 0, D_flag = 0, k_flag = 0, s_flag = 0;
	int sock, fd, ch, result, saved_errno;
	u_int nalloc;
	char *shell, *format, *pidstr, *agentsocket = NULL;
	fd_set *readsetp = NULL, *writesetp = NULL;
#ifdef HAVE_SETRLIMIT
	struct rlimit rlim;
#endif
	extern int optind;
	extern char *optarg;
	pid_t pid;
	char pidstrbuf[1 + 3 * sizeof pid];
	struct timeval *tvp = NULL;
	size_t len;
	mode_t prev_mask;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	/* drop */
	setegid(getgid());
	setgid(getgid());
	setuid(geteuid());

#if defined(HAVE_PRCTL) && defined(PR_SET_DUMPABLE)
	/* Disable ptrace on Linux without sgid bit */
	prctl(PR_SET_DUMPABLE, 0);
#endif

#ifdef WITH_OPENSSL
	OpenSSL_add_all_algorithms();
#endif

	__progname = ssh_get_progname(av[0]);
	seed_rng();

	while ((ch = getopt(ac, av, "cDdksE:a:t:x")) != -1) {
		switch (ch) {
		case 'E':
			fingerprint_hash = ssh_digest_alg_by_name(optarg);
			if (fingerprint_hash == -1)
				fatal("Invalid hash algorithm \"%s\"", optarg);
			break;
		case 'c':
			if (s_flag)
				usage();
			c_flag++;
			break;
		case 'k':
			k_flag++;
			break;
		case 's':
			if (c_flag)
				usage();
			s_flag++;
			break;
		case 'd':
			if (d_flag || D_flag)
				usage();
			d_flag++;
			break;
		case 'D':
			if (d_flag || D_flag)
				usage();
			D_flag++;
			break;
		case 'a':
			agentsocket = optarg;
			break;
		case 't':
			if ((lifetime = convtime(optarg)) == -1) {
				fprintf(stderr, "Invalid lifetime\n");
				usage();
			}
			break;
		case 'x':
			xcount = 0;
			break;
		default:
			usage();
		}
	}
	ac -= optind;
	av += optind;

	if (ac > 0 && (c_flag || k_flag || s_flag || d_flag || D_flag))
		usage();

	if (ac == 0 && !c_flag && !s_flag) {
		shell = getenv("SHELL");
		if (shell != NULL && (len = strlen(shell)) > 2 &&
		    strncmp(shell + len - 3, "csh", 3) == 0)
			c_flag = 1;
	}
	if (k_flag) {
		const char *errstr = NULL;

		pidstr = getenv(SSH_AGENTPID_ENV_NAME);
		if (pidstr == NULL) {
			fprintf(stderr, "%s not set, cannot kill agent\n",
			    SSH_AGENTPID_ENV_NAME);
			exit(1);
		}
		pid = (int)strtonum(pidstr, 2, INT_MAX, &errstr);
		if (errstr) {
			fprintf(stderr,
			    "%s=\"%s\", which is not a good PID: %s\n",
			    SSH_AGENTPID_ENV_NAME, pidstr, errstr);
			exit(1);
		}
		if (kill(pid, SIGTERM) == -1) {
			perror("kill");
			exit(1);
		}
		format = c_flag ? "unsetenv %s;\n" : "unset %s;\n";
		printf(format, SSH_AUTHSOCKET_ENV_NAME);
		printf(format, SSH_AGENTPID_ENV_NAME);
		printf("echo Agent pid %ld killed;\n", (long)pid);
		exit(0);
	}
	parent_pid = getpid();

	if (agentsocket == NULL) {
		/* Create private directory for agent socket */
		mktemp_proto(socket_dir, sizeof(socket_dir));
		if (mkdtemp(socket_dir) == NULL) {
			perror("mkdtemp: private socket dir");
			exit(1);
		}
		snprintf(socket_name, sizeof socket_name, "%s/agent.%ld", socket_dir,
		    (long)parent_pid);
	} else {
		/* Try to use specified agent socket */
		socket_dir[0] = '\0';
		strlcpy(socket_name, agentsocket, sizeof socket_name);
	}

	/*
	 * Create socket early so it will exist before command gets run from
	 * the parent.
	 */
	prev_mask = umask(0177);
	sock = unix_listener(socket_name, SSH_LISTEN_BACKLOG, 0);
	if (sock < 0) {
		/* XXX - unix_listener() calls error() not perror() */
		*socket_name = '\0'; /* Don't unlink any existing file */
		cleanup_exit(1);
	}
	umask(prev_mask);

	/*
	 * Fork, and have the parent execute the command, if any, or present
	 * the socket data.  The child continues as the authentication agent.
	 */
	if (D_flag || d_flag) {
		log_init(__progname,
		    d_flag ? SYSLOG_LEVEL_DEBUG3 : SYSLOG_LEVEL_INFO,
		    SYSLOG_FACILITY_AUTH, 1);
		format = c_flag ? "setenv %s %s;\n" : "%s=%s; export %s;\n";
		printf(format, SSH_AUTHSOCKET_ENV_NAME, socket_name,
		    SSH_AUTHSOCKET_ENV_NAME);
		printf("echo Agent pid %ld;\n", (long)parent_pid);
		goto skip;
	}
	pid = fork();
	if (pid == -1) {
		perror("fork");
		cleanup_exit(1);
	}
	if (pid != 0) {		/* Parent - execute the given command. */
		close(sock);
		snprintf(pidstrbuf, sizeof pidstrbuf, "%ld", (long)pid);
		if (ac == 0) {
			format = c_flag ? "setenv %s %s;\n" : "%s=%s; export %s;\n";
			printf(format, SSH_AUTHSOCKET_ENV_NAME, socket_name,
			    SSH_AUTHSOCKET_ENV_NAME);
			printf(format, SSH_AGENTPID_ENV_NAME, pidstrbuf,
			    SSH_AGENTPID_ENV_NAME);
			printf("echo Agent pid %ld;\n", (long)pid);
			exit(0);
		}
		if (setenv(SSH_AUTHSOCKET_ENV_NAME, socket_name, 1) == -1 ||
		    setenv(SSH_AGENTPID_ENV_NAME, pidstrbuf, 1) == -1) {
			perror("setenv");
			exit(1);
		}
		execvp(av[0], av);
		perror(av[0]);
		exit(1);
	}
	/* child */
	log_init(__progname, SYSLOG_LEVEL_INFO, SYSLOG_FACILITY_AUTH, 0);

	if (setsid() == -1) {
		error("setsid: %s", strerror(errno));
		cleanup_exit(1);
	}

	(void)chdir("/");
	if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
		/* XXX might close listen socket */
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
			close(fd);
	}

#ifdef HAVE_SETRLIMIT
	/* deny core dumps, since memory contains unencrypted private keys */
	rlim.rlim_cur = rlim.rlim_max = 0;
	if (setrlimit(RLIMIT_CORE, &rlim) < 0) {
		error("setrlimit RLIMIT_CORE: %s", strerror(errno));
		cleanup_exit(1);
	}
#endif

skip:

	cleanup_pid = getpid();

#ifdef ENABLE_PKCS11
	pkcs11_init(0);
#endif
	new_socket(AUTH_SOCKET, sock);
	if (ac > 0)
		parent_alive_interval = 10;
	idtab_init();
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, (d_flag | D_flag) ? cleanup_handler : SIG_IGN);
	signal(SIGHUP, cleanup_handler);
	signal(SIGTERM, cleanup_handler);
	nalloc = 0;

	while (1) {
		prepare_select(&readsetp, &writesetp, &max_fd, &nalloc, &tvp);
		result = select(max_fd + 1, readsetp, writesetp, NULL, tvp);
		saved_errno = errno;
		if (parent_alive_interval != 0)
			check_parent_exists();
		(void) reaper();	/* remove expired keys */
		if (result < 0) {
			if (saved_errno == EINTR)
				continue;
			fatal("select: %s", strerror(saved_errno));
		} else if (result > 0)
			after_select(readsetp, writesetp);
	}
	/* NOTREACHED */
}
