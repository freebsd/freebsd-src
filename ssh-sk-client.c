/* $OpenBSD: ssh-sk-client.c,v 1.7 2020/01/23 07:10:22 dtucker Exp $ */
/*
 * Copyright (c) 2019 Google LLC
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
#include <sys/socket.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "ssherr.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "msg.h"
#include "digest.h"
#include "pathnames.h"
#include "ssh-sk.h"
#include "misc.h"

/* #define DEBUG_SK 1 */

static int
start_helper(int *fdp, pid_t *pidp, void (**osigchldp)(int))
{
	void (*osigchld)(int);
	int oerrno, pair[2], r = SSH_ERR_INTERNAL_ERROR;
	pid_t pid;
	char *helper, *verbosity = NULL;

	*fdp = -1;
	*pidp = 0;
	*osigchldp = SIG_DFL;

	helper = getenv("SSH_SK_HELPER");
	if (helper == NULL || strlen(helper) == 0)
		helper = _PATH_SSH_SK_HELPER;
	if (access(helper, X_OK) != 0) {
		oerrno = errno;
		error("%s: helper \"%s\" unusable: %s", __func__, helper,
		    strerror(errno));
		errno = oerrno;
		return SSH_ERR_SYSTEM_ERROR;
	}
#ifdef DEBUG_SK
	verbosity = "-vvv";
#endif

	/* Start helper */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1) {
		error("socketpair: %s", strerror(errno));
		return SSH_ERR_SYSTEM_ERROR;
	}
	osigchld = ssh_signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == -1) {
		oerrno = errno;
		error("fork: %s", strerror(errno));
		close(pair[0]);
		close(pair[1]);
		ssh_signal(SIGCHLD, osigchld);
		errno = oerrno;
		return SSH_ERR_SYSTEM_ERROR;
	}
	if (pid == 0) {
		if ((dup2(pair[1], STDIN_FILENO) == -1) ||
		    (dup2(pair[1], STDOUT_FILENO) == -1)) {
			error("%s: dup2: %s", __func__, ssh_err(r));
			_exit(1);
		}
		close(pair[0]);
		close(pair[1]);
		closefrom(STDERR_FILENO + 1);
		debug("%s: starting %s %s", __func__, helper,
		    verbosity == NULL ? "" : verbosity);
		execlp(helper, helper, verbosity, (char *)NULL);
		error("%s: execlp: %s", __func__, strerror(errno));
		_exit(1);
	}
	close(pair[1]);

	/* success */
	debug3("%s: started pid=%ld", __func__, (long)pid);
	*fdp = pair[0];
	*pidp = pid;
	*osigchldp = osigchld;
	return 0;
}

static int
reap_helper(pid_t pid)
{
	int status, oerrno;

	debug3("%s: pid=%ld", __func__, (long)pid);

	errno = 0;
	while (waitpid(pid, &status, 0) == -1) {
		if (errno == EINTR) {
			errno = 0;
			continue;
		}
		oerrno = errno;
		error("%s: waitpid: %s", __func__, strerror(errno));
		errno = oerrno;
		return SSH_ERR_SYSTEM_ERROR;
	}
	if (!WIFEXITED(status)) {
		error("%s: helper exited abnormally", __func__);
		return SSH_ERR_AGENT_FAILURE;
	} else if (WEXITSTATUS(status) != 0) {
		error("%s: helper exited with non-zero exit status", __func__);
		return SSH_ERR_AGENT_FAILURE;
	}
	return 0;
}

static int
client_converse(struct sshbuf *msg, struct sshbuf **respp, u_int type)
{
	int oerrno, fd, r2, ll, r = SSH_ERR_INTERNAL_ERROR;
	u_int rtype, rerr;
	pid_t pid;
	u_char version;
	void (*osigchld)(int);
	struct sshbuf *req = NULL, *resp = NULL;
	*respp = NULL;

	if ((r = start_helper(&fd, &pid, &osigchld)) != 0)
		return r;

	if ((req = sshbuf_new()) == NULL || (resp = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	/* Request preamble: type, log_on_stderr, log_level */
	ll = log_level_get();
	if ((r = sshbuf_put_u32(req, type)) != 0 ||
	   (r = sshbuf_put_u8(req, log_is_on_stderr() != 0)) != 0 ||
	   (r = sshbuf_put_u32(req, ll < 0 ? 0 : ll)) != 0 ||
	   (r = sshbuf_putb(req, msg)) != 0) {
		error("%s: build: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((r = ssh_msg_send(fd, SSH_SK_HELPER_VERSION, req)) != 0) {
		error("%s: send: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((r = ssh_msg_recv(fd, resp)) != 0) {
		error("%s: receive: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((r = sshbuf_get_u8(resp, &version)) != 0) {
		error("%s: parse version: %s", __func__, ssh_err(r));
		goto out;
	}
	if (version != SSH_SK_HELPER_VERSION) {
		error("%s: unsupported version: got %u, expected %u",
		    __func__, version, SSH_SK_HELPER_VERSION);
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshbuf_get_u32(resp, &rtype)) != 0) {
		error("%s: parse message type: %s", __func__, ssh_err(r));
		goto out;
	}
	if (rtype == SSH_SK_HELPER_ERROR) {
		if ((r = sshbuf_get_u32(resp, &rerr)) != 0) {
			error("%s: parse error: %s", __func__, ssh_err(r));
			goto out;
		}
		debug("%s: helper returned error -%u", __func__, rerr);
		/* OpenSSH error values are negative; encoded as -err on wire */
		if (rerr == 0 || rerr >= INT_MAX)
			r = SSH_ERR_INTERNAL_ERROR;
		else
			r = -(int)rerr;
		goto out;
	} else if (rtype != type) {
		error("%s: helper returned incorrect message type %u, "
		    "expecting %u", __func__, rtype, type);
		r = SSH_ERR_INTERNAL_ERROR;
		goto out;
	}
	/* success */
	r = 0;
 out:
	oerrno = errno;
	close(fd);
	if ((r2 = reap_helper(pid)) != 0) {
		if (r == 0) {
			r = r2;
			oerrno = errno;
		}
	}
	if (r == 0) {
		*respp = resp;
		resp = NULL;
	}
	sshbuf_free(req);
	sshbuf_free(resp);
	ssh_signal(SIGCHLD, osigchld);
	errno = oerrno;
	return r;

}

int
sshsk_sign(const char *provider, struct sshkey *key,
    u_char **sigp, size_t *lenp, const u_char *data, size_t datalen,
    u_int compat, const char *pin)
{
	int oerrno, r = SSH_ERR_INTERNAL_ERROR;
	char *fp = NULL;
	struct sshbuf *kbuf = NULL, *req = NULL, *resp = NULL;

	*sigp = NULL;
	*lenp = 0;

#ifndef ENABLE_SK
	return SSH_ERR_KEY_TYPE_UNKNOWN;
#endif

	if ((kbuf = sshbuf_new()) == NULL ||
	    (req = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if ((r = sshkey_private_serialize(key, kbuf)) != 0) {
		error("%s: serialize private key: %s", __func__, ssh_err(r));
		goto out;
	}
	if ((r = sshbuf_put_stringb(req, kbuf)) != 0 ||
	    (r = sshbuf_put_cstring(req, provider)) != 0 ||
	    (r = sshbuf_put_string(req, data, datalen)) != 0 ||
	    (r = sshbuf_put_cstring(req, NULL)) != 0 || /* alg */
	    (r = sshbuf_put_u32(req, compat)) != 0 ||
	    (r = sshbuf_put_cstring(req, pin)) != 0) {
		error("%s: compose: %s", __func__, ssh_err(r));
		goto out;
	}

	if ((fp = sshkey_fingerprint(key, SSH_FP_HASH_DEFAULT,
	    SSH_FP_DEFAULT)) == NULL) {
		error("%s: sshkey_fingerprint failed", __func__);
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = client_converse(req, &resp, SSH_SK_HELPER_SIGN)) != 0)
		goto out;

	if ((r = sshbuf_get_string(resp, sigp, lenp)) != 0) {
		error("%s: parse signature: %s", __func__, ssh_err(r));
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(resp) != 0) {
		error("%s: trailing data in response", __func__);
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	/* success */
	r = 0;
 out:
	oerrno = errno;
	if (r != 0) {
		freezero(*sigp, *lenp);
		*sigp = NULL;
		*lenp = 0;
	}
	sshbuf_free(kbuf);
	sshbuf_free(req);
	sshbuf_free(resp);
	errno = oerrno;
	return r;
}

int
sshsk_enroll(int type, const char *provider_path, const char *device,
    const char *application, const char *userid, uint8_t flags,
    const char *pin, struct sshbuf *challenge_buf,
    struct sshkey **keyp, struct sshbuf *attest)
{
	int oerrno, r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *kbuf = NULL, *abuf = NULL, *req = NULL, *resp = NULL;
	struct sshkey *key = NULL;

	*keyp = NULL;
	if (attest != NULL)
		sshbuf_reset(attest);

#ifndef ENABLE_SK
	return SSH_ERR_KEY_TYPE_UNKNOWN;
#endif

	if (type < 0)
		return SSH_ERR_INVALID_ARGUMENT;

	if ((abuf = sshbuf_new()) == NULL ||
	    (kbuf = sshbuf_new()) == NULL ||
	    (req = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if ((r = sshbuf_put_u32(req, (u_int)type)) != 0 ||
	    (r = sshbuf_put_cstring(req, provider_path)) != 0 ||
	    (r = sshbuf_put_cstring(req, device)) != 0 ||
	    (r = sshbuf_put_cstring(req, application)) != 0 ||
	    (r = sshbuf_put_cstring(req, userid)) != 0 ||
	    (r = sshbuf_put_u8(req, flags)) != 0 ||
	    (r = sshbuf_put_cstring(req, pin)) != 0 ||
	    (r = sshbuf_put_stringb(req, challenge_buf)) != 0) {
		error("%s: compose: %s", __func__, ssh_err(r));
		goto out;
	}

	if ((r = client_converse(req, &resp, SSH_SK_HELPER_ENROLL)) != 0)
		goto out;

	if ((r = sshbuf_get_stringb(resp, kbuf)) != 0 ||
	    (r = sshbuf_get_stringb(resp, abuf)) != 0) {
		error("%s: parse signature: %s", __func__, ssh_err(r));
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(resp) != 0) {
		error("%s: trailing data in response", __func__);
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshkey_private_deserialize(kbuf, &key)) != 0) {
		error("Unable to parse private key: %s", ssh_err(r));
		goto out;
	}
	if (attest != NULL && (r = sshbuf_putb(attest, abuf)) != 0) {
		error("%s: buffer error: %s", __func__, ssh_err(r));
		goto out;
	}

	/* success */
	r = 0;
	*keyp = key;
	key = NULL;
 out:
	oerrno = errno;
	sshkey_free(key);
	sshbuf_free(kbuf);
	sshbuf_free(abuf);
	sshbuf_free(req);
	sshbuf_free(resp);
	errno = oerrno;
	return r;
}

int
sshsk_load_resident(const char *provider_path, const char *device,
    const char *pin, struct sshkey ***keysp, size_t *nkeysp)
{
	int oerrno, r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *kbuf = NULL, *req = NULL, *resp = NULL;
	struct sshkey *key = NULL, **keys = NULL, **tmp;
	size_t i, nkeys = 0;

	*keysp = NULL;
	*nkeysp = 0;

	if ((resp = sshbuf_new()) == NULL ||
	    (kbuf = sshbuf_new()) == NULL ||
	    (req = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}

	if ((r = sshbuf_put_cstring(req, provider_path)) != 0 ||
	    (r = sshbuf_put_cstring(req, device)) != 0 ||
	    (r = sshbuf_put_cstring(req, pin)) != 0) {
		error("%s: compose: %s", __func__, ssh_err(r));
		goto out;
	}

	if ((r = client_converse(req, &resp, SSH_SK_HELPER_LOAD_RESIDENT)) != 0)
		goto out;

	while (sshbuf_len(resp) != 0) {
		/* key, comment */
		if ((r = sshbuf_get_stringb(resp, kbuf)) != 0 ||
		    (r = sshbuf_get_cstring(resp, NULL, NULL)) != 0) {
			error("%s: parse signature: %s", __func__, ssh_err(r));
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if ((r = sshkey_private_deserialize(kbuf, &key)) != 0) {
			error("Unable to parse private key: %s", ssh_err(r));
			goto out;
		}
		if ((tmp = recallocarray(keys, nkeys, nkeys + 1,
		    sizeof(*keys))) == NULL) {
			error("%s: recallocarray keys failed", __func__);
			goto out;
		}
		debug("%s: keys[%zu]: %s %s", __func__,
		    nkeys, sshkey_type(key), key->sk_application);
		keys = tmp;
		keys[nkeys++] = key;
		key = NULL;
	}

	/* success */
	r = 0;
	*keysp = keys;
	*nkeysp = nkeys;
	keys = NULL;
	nkeys = 0;
 out:
	oerrno = errno;
	for (i = 0; i < nkeys; i++)
		sshkey_free(keys[i]);
	free(keys);
	sshkey_free(key);
	sshbuf_free(kbuf);
	sshbuf_free(req);
	sshbuf_free(resp);
	errno = oerrno;
	return r;
}
