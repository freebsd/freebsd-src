/* $OpenBSD: ssh-pkcs11-client.c,v 1.5 2014/06/24 01:13:21 djm Exp $ */
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

#ifdef ENABLE_PKCS11

#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/socket.h>

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/rsa.h>

#include "pathnames.h"
#include "xmalloc.h"
#include "buffer.h"
#include "log.h"
#include "misc.h"
#include "key.h"
#include "authfd.h"
#include "atomicio.h"
#include "ssh-pkcs11.h"

/* borrows code from sftp-server and ssh-agent */

int fd = -1;
pid_t pid = -1;

static void
send_msg(Buffer *m)
{
	u_char buf[4];
	int mlen = buffer_len(m);

	put_u32(buf, mlen);
	if (atomicio(vwrite, fd, buf, 4) != 4 ||
	    atomicio(vwrite, fd, buffer_ptr(m),
	    buffer_len(m)) != buffer_len(m))
		error("write to helper failed");
	buffer_consume(m, mlen);
}

static int
recv_msg(Buffer *m)
{
	u_int l, len;
	u_char buf[1024];

	if ((len = atomicio(read, fd, buf, 4)) != 4) {
		error("read from helper failed: %u", len);
		return (0); /* XXX */
	}
	len = get_u32(buf);
	if (len > 256 * 1024)
		fatal("response too long: %u", len);
	/* read len bytes into m */
	buffer_clear(m);
	while (len > 0) {
		l = len;
		if (l > sizeof(buf))
			l = sizeof(buf);
		if (atomicio(read, fd, buf, l) != l) {
			error("response from helper failed.");
			return (0); /* XXX */
		}
		buffer_append(m, buf, l);
		len -= l;
	}
	return (buffer_get_char(m));
}

int
pkcs11_init(int interactive)
{
	return (0);
}

void
pkcs11_terminate(void)
{
	close(fd);
}

static int
pkcs11_rsa_private_encrypt(int flen, const u_char *from, u_char *to, RSA *rsa,
    int padding)
{
	Key key;
	u_char *blob, *signature = NULL;
	u_int blen, slen = 0;
	int ret = -1;
	Buffer msg;

	if (padding != RSA_PKCS1_PADDING)
		return (-1);
	key.type = KEY_RSA;
	key.rsa = rsa;
	if (key_to_blob(&key, &blob, &blen) == 0)
		return -1;
	buffer_init(&msg);
	buffer_put_char(&msg, SSH2_AGENTC_SIGN_REQUEST);
	buffer_put_string(&msg, blob, blen);
	buffer_put_string(&msg, from, flen);
	buffer_put_int(&msg, 0);
	free(blob);
	send_msg(&msg);
	buffer_clear(&msg);

	if (recv_msg(&msg) == SSH2_AGENT_SIGN_RESPONSE) {
		signature = buffer_get_string(&msg, &slen);
		if (slen <= (u_int)RSA_size(rsa)) {
			memcpy(to, signature, slen);
			ret = slen;
		}
		free(signature);
	}
	buffer_free(&msg);
	return (ret);
}

/* redirect the private key encrypt operation to the ssh-pkcs11-helper */
static int
wrap_key(RSA *rsa)
{
	static RSA_METHOD helper_rsa;

	memcpy(&helper_rsa, RSA_get_default_method(), sizeof(helper_rsa));
	helper_rsa.name = "ssh-pkcs11-helper";
	helper_rsa.rsa_priv_enc = pkcs11_rsa_private_encrypt;
	RSA_set_method(rsa, &helper_rsa);
	return (0);
}

static int
pkcs11_start_helper(void)
{
	int pair[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1) {
		error("socketpair: %s", strerror(errno));
		return (-1);
	}
	if ((pid = fork()) == -1) {
		error("fork: %s", strerror(errno));
		return (-1);
	} else if (pid == 0) {
		if ((dup2(pair[1], STDIN_FILENO) == -1) ||
		    (dup2(pair[1], STDOUT_FILENO) == -1)) {
			fprintf(stderr, "dup2: %s\n", strerror(errno));
			_exit(1);
		}
		close(pair[0]);
		close(pair[1]);
		execlp(_PATH_SSH_PKCS11_HELPER, _PATH_SSH_PKCS11_HELPER,
		    (char *) 0);
		fprintf(stderr, "exec: %s: %s\n", _PATH_SSH_PKCS11_HELPER,
		    strerror(errno));
		_exit(1);
	}
	close(pair[1]);
	fd = pair[0];
	return (0);
}

int
pkcs11_add_provider(char *name, char *pin, Key ***keysp)
{
	Key *k;
	int i, nkeys;
	u_char *blob;
	u_int blen;
	Buffer msg;

	if (fd < 0 && pkcs11_start_helper() < 0)
		return (-1);

	buffer_init(&msg);
	buffer_put_char(&msg, SSH_AGENTC_ADD_SMARTCARD_KEY);
	buffer_put_cstring(&msg, name);
	buffer_put_cstring(&msg, pin);
	send_msg(&msg);
	buffer_clear(&msg);

	if (recv_msg(&msg) == SSH2_AGENT_IDENTITIES_ANSWER) {
		nkeys = buffer_get_int(&msg);
		*keysp = xcalloc(nkeys, sizeof(Key *));
		for (i = 0; i < nkeys; i++) {
			blob = buffer_get_string(&msg, &blen);
			free(buffer_get_string(&msg, NULL));
			k = key_from_blob(blob, blen);
			wrap_key(k->rsa);
			(*keysp)[i] = k;
			free(blob);
		}
	} else {
		nkeys = -1;
	}
	buffer_free(&msg);
	return (nkeys);
}

int
pkcs11_del_provider(char *name)
{
	int ret = -1;
	Buffer msg;

	buffer_init(&msg);
	buffer_put_char(&msg, SSH_AGENTC_REMOVE_SMARTCARD_KEY);
	buffer_put_cstring(&msg, name);
	buffer_put_cstring(&msg, "");
	send_msg(&msg);
	buffer_clear(&msg);

	if (recv_msg(&msg) == SSH_AGENT_SUCCESS)
		ret = 0;
	buffer_free(&msg);
	return (ret);
}

#endif /* ENABLE_PKCS11 */
