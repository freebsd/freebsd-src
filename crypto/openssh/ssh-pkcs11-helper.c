/* $OpenBSD: ssh-pkcs11-helper.c,v 1.27 2024/08/15 00:51:51 djm Exp $ */
/*
 * Copyright (c) 2010 Markus Friedl.  All rights reserved.
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
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include "openbsd-compat/sys-queue.h"

#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "sshbuf.h"
#include "log.h"
#include "misc.h"
#include "sshkey.h"
#include "authfd.h"
#include "ssh-pkcs11.h"
#include "ssherr.h"

#ifdef ENABLE_PKCS11

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/rsa.h>

/* borrows code from sftp-server and ssh-agent */

struct pkcs11_keyinfo {
	struct sshkey	*key;
	char		*providername, *label;
	TAILQ_ENTRY(pkcs11_keyinfo) next;
};

TAILQ_HEAD(, pkcs11_keyinfo) pkcs11_keylist;

#define MAX_MSG_LENGTH		10240 /*XXX*/

/* input and output queue */
struct sshbuf *iqueue;
struct sshbuf *oqueue;

static void
add_key(struct sshkey *k, char *name, char *label)
{
	struct pkcs11_keyinfo *ki;

	ki = xcalloc(1, sizeof(*ki));
	ki->providername = xstrdup(name);
	ki->key = k;
	ki->label = xstrdup(label);
	TAILQ_INSERT_TAIL(&pkcs11_keylist, ki, next);
}

static void
del_keys_by_name(char *name)
{
	struct pkcs11_keyinfo *ki, *nxt;

	for (ki = TAILQ_FIRST(&pkcs11_keylist); ki; ki = nxt) {
		nxt = TAILQ_NEXT(ki, next);
		if (!strcmp(ki->providername, name)) {
			TAILQ_REMOVE(&pkcs11_keylist, ki, next);
			free(ki->providername);
			free(ki->label);
			sshkey_free(ki->key);
			free(ki);
		}
	}
}

/* lookup matching 'private' key */
static struct sshkey *
lookup_key(struct sshkey *k)
{
	struct pkcs11_keyinfo *ki;

	TAILQ_FOREACH(ki, &pkcs11_keylist, next) {
		debug("check %s %s %s", sshkey_type(ki->key),
		    ki->providername, ki->label);
		if (sshkey_equal(k, ki->key))
			return (ki->key);
	}
	return (NULL);
}

static void
send_msg(struct sshbuf *m)
{
	int r;

	if ((r = sshbuf_put_stringb(oqueue, m)) != 0)
		fatal_fr(r, "enqueue");
}

static void
process_add(void)
{
	char *name, *pin;
	struct sshkey **keys = NULL;
	int r, i, nkeys;
	u_char *blob;
	size_t blen;
	struct sshbuf *msg;
	char **labels = NULL;

	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &pin, NULL)) != 0)
		fatal_fr(r, "parse");
	if ((nkeys = pkcs11_add_provider(name, pin, &keys, &labels)) > 0) {
		if ((r = sshbuf_put_u8(msg,
		    SSH2_AGENT_IDENTITIES_ANSWER)) != 0 ||
		    (r = sshbuf_put_u32(msg, nkeys)) != 0)
			fatal_fr(r, "compose");
		for (i = 0; i < nkeys; i++) {
			if ((r = sshkey_to_blob(keys[i], &blob, &blen)) != 0) {
				debug_fr(r, "encode key");
				continue;
			}
			if ((r = sshbuf_put_string(msg, blob, blen)) != 0 ||
			    (r = sshbuf_put_cstring(msg, labels[i])) != 0)
				fatal_fr(r, "compose key");
			free(blob);
			add_key(keys[i], name, labels[i]);
			free(labels[i]);
		}
	} else if ((r = sshbuf_put_u8(msg, SSH_AGENT_FAILURE)) != 0 ||
	    (r = sshbuf_put_u32(msg, -nkeys)) != 0)
		fatal_fr(r, "compose");
	free(labels);
	free(keys); /* keys themselves are transferred to pkcs11_keylist */
	free(pin);
	free(name);
	send_msg(msg);
	sshbuf_free(msg);
}

static void
process_del(void)
{
	char *name, *pin;
	struct sshbuf *msg;
	int r;

	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((r = sshbuf_get_cstring(iqueue, &name, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(iqueue, &pin, NULL)) != 0)
		fatal_fr(r, "parse");
	del_keys_by_name(name);
	if ((r = sshbuf_put_u8(msg, pkcs11_del_provider(name) == 0 ?
	    SSH_AGENT_SUCCESS : SSH_AGENT_FAILURE)) != 0)
		fatal_fr(r, "compose");
	free(pin);
	free(name);
	send_msg(msg);
	sshbuf_free(msg);
}

static void
process_sign(void)
{
	u_char *blob, *data, *signature = NULL;
	size_t blen, dlen;
	u_int slen = 0;
	int len, r, ok = -1;
	struct sshkey *key = NULL, *found;
	struct sshbuf *msg;
#ifdef WITH_OPENSSL
	RSA *rsa = NULL;
#ifdef OPENSSL_HAS_ECC
	EC_KEY *ecdsa = NULL;
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */

	/* XXX support SHA2 signature flags */
	if ((r = sshbuf_get_string(iqueue, &blob, &blen)) != 0 ||
	    (r = sshbuf_get_string(iqueue, &data, &dlen)) != 0 ||
	    (r = sshbuf_get_u32(iqueue, NULL)) != 0)
		fatal_fr(r, "parse");

	if ((r = sshkey_from_blob(blob, blen, &key)) != 0)
		fatal_fr(r, "decode key");
	if ((found = lookup_key(key)) == NULL)
		goto reply;

	/* XXX use pkey API properly for signing */
	switch (key->type) {
#ifdef WITH_OPENSSL
	case KEY_RSA:
		if ((rsa = EVP_PKEY_get1_RSA(found->pkey)) == NULL)
			fatal_f("no RSA in pkey");
		if ((len = RSA_size(rsa)) < 0)
			fatal_f("bad RSA length");
		signature = xmalloc(len);
		if ((len = RSA_private_encrypt(dlen, data, signature,
		    rsa, RSA_PKCS1_PADDING)) < 0) {
			error_f("RSA_private_encrypt failed");
			goto reply;
		}
		slen = (u_int)len;
		break;
#ifdef OPENSSL_HAS_ECC
	case KEY_ECDSA:
		if ((ecdsa = EVP_PKEY_get1_EC_KEY(found->pkey)) == NULL)
			fatal_f("no ECDSA in pkey");
		if ((len = ECDSA_size(ecdsa)) < 0)
			fatal_f("bad ECDSA length");
		slen = (u_int)len;
		signature = xmalloc(slen);
		/* "The parameter type is ignored." */
		if (!ECDSA_sign(-1, data, dlen, signature, &slen, ecdsa)) {
			error_f("ECDSA_sign failed");
			goto reply;
		}
		break;
#endif /* OPENSSL_HAS_ECC */
#endif /* WITH_OPENSSL */
	default:
		fatal_f("unsupported key type %d", key->type);
	}
	/* success */
	ok = 0;
 reply:
	if ((msg = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if (ok == 0) {
		if ((r = sshbuf_put_u8(msg, SSH2_AGENT_SIGN_RESPONSE)) != 0 ||
		    (r = sshbuf_put_string(msg, signature, slen)) != 0)
			fatal_fr(r, "compose response");
	} else {
		if ((r = sshbuf_put_u8(msg, SSH2_AGENT_FAILURE)) != 0)
			fatal_fr(r, "compose failure response");
	}
	sshkey_free(key);
	RSA_free(rsa);
#if defined(WITH_OPENSSL) && defined(OPENSSL_HAS_ECC)
	EC_KEY_free(ecdsa);
#endif
	free(data);
	free(blob);
	free(signature);
	send_msg(msg);
	sshbuf_free(msg);
}

static void
process(void)
{
	u_int msg_len;
	u_int buf_len;
	u_int consumed;
	u_char type;
	const u_char *cp;
	int r;

	buf_len = sshbuf_len(iqueue);
	if (buf_len < 5)
		return;		/* Incomplete message. */
	cp = sshbuf_ptr(iqueue);
	msg_len = get_u32(cp);
	if (msg_len > MAX_MSG_LENGTH) {
		error("bad message len %d", msg_len);
		cleanup_exit(11);
	}
	if (buf_len < msg_len + 4)
		return;
	if ((r = sshbuf_consume(iqueue, 4)) != 0 ||
	    (r = sshbuf_get_u8(iqueue, &type)) != 0)
		fatal_fr(r, "parse type/len");
	buf_len -= 4;
	switch (type) {
	case SSH_AGENTC_ADD_SMARTCARD_KEY:
		debug("process_add");
		process_add();
		break;
	case SSH_AGENTC_REMOVE_SMARTCARD_KEY:
		debug("process_del");
		process_del();
		break;
	case SSH2_AGENTC_SIGN_REQUEST:
		debug("process_sign");
		process_sign();
		break;
	default:
		error("Unknown message %d", type);
		break;
	}
	/* discard the remaining bytes from the current packet */
	if (buf_len < sshbuf_len(iqueue)) {
		error("iqueue grew unexpectedly");
		cleanup_exit(255);
	}
	consumed = buf_len - sshbuf_len(iqueue);
	if (msg_len < consumed) {
		error("msg_len %d < consumed %d", msg_len, consumed);
		cleanup_exit(255);
	}
	if (msg_len > consumed) {
		if ((r = sshbuf_consume(iqueue, msg_len - consumed)) != 0)
			fatal_fr(r, "consume");
	}
}

void
cleanup_exit(int i)
{
	/* XXX */
	_exit(i);
}


int
main(int argc, char **argv)
{
	int r, ch, in, out, log_stderr = 0;
	ssize_t len;
	SyslogFacility log_facility = SYSLOG_FACILITY_AUTH;
	LogLevel log_level = SYSLOG_LEVEL_ERROR;
	char buf[4*4096];
	extern char *__progname;
	struct pollfd pfd[2];

	__progname = ssh_get_progname(argv[0]);
	seed_rng();
	TAILQ_INIT(&pkcs11_keylist);

	log_init(__progname, log_level, log_facility, log_stderr);

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			log_stderr = 1;
			if (log_level == SYSLOG_LEVEL_ERROR)
				log_level = SYSLOG_LEVEL_DEBUG1;
			else if (log_level < SYSLOG_LEVEL_DEBUG3)
				log_level++;
			break;
		default:
			fprintf(stderr, "usage: %s [-v]\n", __progname);
			exit(1);
		}
	}

	log_init(__progname, log_level, log_facility, log_stderr);

	pkcs11_init(0);
	in = STDIN_FILENO;
	out = STDOUT_FILENO;

	if ((iqueue = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");
	if ((oqueue = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");

	while (1) {
		memset(pfd, 0, sizeof(pfd));
		pfd[0].fd = in;
		pfd[1].fd = out;

		/*
		 * Ensure that we can read a full buffer and handle
		 * the worst-case length packet it can generate,
		 * otherwise apply backpressure by stopping reads.
		 */
		if ((r = sshbuf_check_reserve(iqueue, sizeof(buf))) == 0 &&
		    (r = sshbuf_check_reserve(oqueue, MAX_MSG_LENGTH)) == 0)
			pfd[0].events = POLLIN;
		else if (r != SSH_ERR_NO_BUFFER_SPACE)
			fatal_fr(r, "reserve");

		if (sshbuf_len(oqueue) > 0)
			pfd[1].events = POLLOUT;

		if ((r = poll(pfd, 2, -1 /* INFTIM */)) <= 0) {
			if (r == 0 || errno == EINTR)
				continue;
			fatal("poll: %s", strerror(errno));
		}

		/* copy stdin to iqueue */
		if ((pfd[0].revents & (POLLIN|POLLHUP|POLLERR)) != 0) {
			len = read(in, buf, sizeof buf);
			if (len == 0) {
				debug("read eof");
				cleanup_exit(0);
			} else if (len < 0) {
				error("read: %s", strerror(errno));
				cleanup_exit(1);
			} else if ((r = sshbuf_put(iqueue, buf, len)) != 0)
				fatal_fr(r, "sshbuf_put");
		}
		/* send oqueue to stdout */
		if ((pfd[1].revents & (POLLOUT|POLLHUP)) != 0) {
			len = write(out, sshbuf_ptr(oqueue),
			    sshbuf_len(oqueue));
			if (len < 0) {
				error("write: %s", strerror(errno));
				cleanup_exit(1);
			} else if ((r = sshbuf_consume(oqueue, len)) != 0)
				fatal_fr(r, "consume");
		}

		/*
		 * Process requests from client if we can fit the results
		 * into the output buffer, otherwise stop processing input
		 * and let the output queue drain.
		 */
		if ((r = sshbuf_check_reserve(oqueue, MAX_MSG_LENGTH)) == 0)
			process();
		else if (r != SSH_ERR_NO_BUFFER_SPACE)
			fatal_fr(r, "reserve");
	}
}

#else /* WITH_OPENSSL */
void
cleanup_exit(int i)
{
	_exit(i);
}

int
main(int argc, char **argv)
{
	fprintf(stderr, "PKCS#11 code is not enabled\n");
	return 1;
}
#endif /* WITH_OPENSSL */
#else /* ENABLE_PKCS11 */
int
main(int argc, char **argv)
{
	extern char *__progname;

	__progname = ssh_get_progname(argv[0]);
	log_init(__progname, SYSLOG_LEVEL_ERROR, SYSLOG_FACILITY_AUTH, 0);
	fatal("PKCS#11 support disabled at compile time");
}
#endif /* ENABLE_PKCS11 */
