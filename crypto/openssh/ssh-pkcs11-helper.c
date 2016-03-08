/* $OpenBSD: ssh-pkcs11-helper.c,v 1.11 2015/08/20 22:32:42 deraadt Exp $ */
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

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "xmalloc.h"
#include "buffer.h"
#include "log.h"
#include "misc.h"
#include "key.h"
#include "authfd.h"
#include "ssh-pkcs11.h"

#ifdef ENABLE_PKCS11

/* borrows code from sftp-server and ssh-agent */

struct pkcs11_keyinfo {
	Key		*key;
	char		*providername;
	TAILQ_ENTRY(pkcs11_keyinfo) next;
};

TAILQ_HEAD(, pkcs11_keyinfo) pkcs11_keylist;

#define MAX_MSG_LENGTH		10240 /*XXX*/

/* helper */
#define get_int()			buffer_get_int(&iqueue);
#define get_string(lenp)		buffer_get_string(&iqueue, lenp);

/* input and output queue */
Buffer iqueue;
Buffer oqueue;

static void
add_key(Key *k, char *name)
{
	struct pkcs11_keyinfo *ki;

	ki = xcalloc(1, sizeof(*ki));
	ki->providername = xstrdup(name);
	ki->key = k;
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
			key_free(ki->key);
			free(ki);
		}
	}
}

/* lookup matching 'private' key */
static Key *
lookup_key(Key *k)
{
	struct pkcs11_keyinfo *ki;

	TAILQ_FOREACH(ki, &pkcs11_keylist, next) {
		debug("check %p %s", ki, ki->providername);
		if (key_equal(k, ki->key))
			return (ki->key);
	}
	return (NULL);
}

static void
send_msg(Buffer *m)
{
	int mlen = buffer_len(m);

	buffer_put_int(&oqueue, mlen);
	buffer_append(&oqueue, buffer_ptr(m), mlen);
	buffer_consume(m, mlen);
}

static void
process_add(void)
{
	char *name, *pin;
	Key **keys;
	int i, nkeys;
	u_char *blob;
	u_int blen;
	Buffer msg;

	buffer_init(&msg);
	name = get_string(NULL);
	pin = get_string(NULL);
	if ((nkeys = pkcs11_add_provider(name, pin, &keys)) > 0) {
		buffer_put_char(&msg, SSH2_AGENT_IDENTITIES_ANSWER);
		buffer_put_int(&msg, nkeys);
		for (i = 0; i < nkeys; i++) {
			if (key_to_blob(keys[i], &blob, &blen) == 0)
				continue;
			buffer_put_string(&msg, blob, blen);
			buffer_put_cstring(&msg, name);
			free(blob);
			add_key(keys[i], name);
		}
		free(keys);
	} else {
		buffer_put_char(&msg, SSH_AGENT_FAILURE);
	}
	free(pin);
	free(name);
	send_msg(&msg);
	buffer_free(&msg);
}

static void
process_del(void)
{
	char *name, *pin;
	Buffer msg;

	buffer_init(&msg);
	name = get_string(NULL);
	pin = get_string(NULL);
	del_keys_by_name(name);
	if (pkcs11_del_provider(name) == 0)
		 buffer_put_char(&msg, SSH_AGENT_SUCCESS);
	else
		 buffer_put_char(&msg, SSH_AGENT_FAILURE);
	free(pin);
	free(name);
	send_msg(&msg);
	buffer_free(&msg);
}

static void
process_sign(void)
{
	u_char *blob, *data, *signature = NULL;
	u_int blen, dlen, slen = 0;
	int ok = -1;
	Key *key, *found;
	Buffer msg;

	blob = get_string(&blen);
	data = get_string(&dlen);
	(void)get_int(); /* XXX ignore flags */

	if ((key = key_from_blob(blob, blen)) != NULL) {
		if ((found = lookup_key(key)) != NULL) {
#ifdef WITH_OPENSSL
			int ret;

			slen = RSA_size(key->rsa);
			signature = xmalloc(slen);
			if ((ret = RSA_private_encrypt(dlen, data, signature,
			    found->rsa, RSA_PKCS1_PADDING)) != -1) {
				slen = ret;
				ok = 0;
			}
#endif /* WITH_OPENSSL */
		}
		key_free(key);
	}
	buffer_init(&msg);
	if (ok == 0) {
		buffer_put_char(&msg, SSH2_AGENT_SIGN_RESPONSE);
		buffer_put_string(&msg, signature, slen);
	} else {
		buffer_put_char(&msg, SSH_AGENT_FAILURE);
	}
	free(data);
	free(blob);
	free(signature);
	send_msg(&msg);
	buffer_free(&msg);
}

static void
process(void)
{
	u_int msg_len;
	u_int buf_len;
	u_int consumed;
	u_int type;
	u_char *cp;

	buf_len = buffer_len(&iqueue);
	if (buf_len < 5)
		return;		/* Incomplete message. */
	cp = buffer_ptr(&iqueue);
	msg_len = get_u32(cp);
	if (msg_len > MAX_MSG_LENGTH) {
		error("bad message len %d", msg_len);
		cleanup_exit(11);
	}
	if (buf_len < msg_len + 4)
		return;
	buffer_consume(&iqueue, 4);
	buf_len -= 4;
	type = buffer_get_char(&iqueue);
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
	if (buf_len < buffer_len(&iqueue)) {
		error("iqueue grew unexpectedly");
		cleanup_exit(255);
	}
	consumed = buf_len - buffer_len(&iqueue);
	if (msg_len < consumed) {
		error("msg_len %d < consumed %d", msg_len, consumed);
		cleanup_exit(255);
	}
	if (msg_len > consumed)
		buffer_consume(&iqueue, msg_len - consumed);
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
	fd_set *rset, *wset;
	int in, out, max, log_stderr = 0;
	ssize_t len, olen, set_size;
	SyslogFacility log_facility = SYSLOG_FACILITY_AUTH;
	LogLevel log_level = SYSLOG_LEVEL_ERROR;
	char buf[4*4096];

	extern char *__progname;

	TAILQ_INIT(&pkcs11_keylist);
	pkcs11_init(0);

	seed_rng();
	__progname = ssh_get_progname(argv[0]);

	log_init(__progname, log_level, log_facility, log_stderr);

	in = STDIN_FILENO;
	out = STDOUT_FILENO;

	max = 0;
	if (in > max)
		max = in;
	if (out > max)
		max = out;

	buffer_init(&iqueue);
	buffer_init(&oqueue);

	set_size = howmany(max + 1, NFDBITS) * sizeof(fd_mask);
	rset = xmalloc(set_size);
	wset = xmalloc(set_size);

	for (;;) {
		memset(rset, 0, set_size);
		memset(wset, 0, set_size);

		/*
		 * Ensure that we can read a full buffer and handle
		 * the worst-case length packet it can generate,
		 * otherwise apply backpressure by stopping reads.
		 */
		if (buffer_check_alloc(&iqueue, sizeof(buf)) &&
		    buffer_check_alloc(&oqueue, MAX_MSG_LENGTH))
			FD_SET(in, rset);

		olen = buffer_len(&oqueue);
		if (olen > 0)
			FD_SET(out, wset);

		if (select(max+1, rset, wset, NULL, NULL) < 0) {
			if (errno == EINTR)
				continue;
			error("select: %s", strerror(errno));
			cleanup_exit(2);
		}

		/* copy stdin to iqueue */
		if (FD_ISSET(in, rset)) {
			len = read(in, buf, sizeof buf);
			if (len == 0) {
				debug("read eof");
				cleanup_exit(0);
			} else if (len < 0) {
				error("read: %s", strerror(errno));
				cleanup_exit(1);
			} else {
				buffer_append(&iqueue, buf, len);
			}
		}
		/* send oqueue to stdout */
		if (FD_ISSET(out, wset)) {
			len = write(out, buffer_ptr(&oqueue), olen);
			if (len < 0) {
				error("write: %s", strerror(errno));
				cleanup_exit(1);
			} else {
				buffer_consume(&oqueue, len);
			}
		}

		/*
		 * Process requests from client if we can fit the results
		 * into the output buffer, otherwise stop processing input
		 * and let the output queue drain.
		 */
		if (buffer_check_alloc(&oqueue, MAX_MSG_LENGTH))
			process();
	}
}
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
