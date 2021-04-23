/* $OpenBSD: ssh-sk-client.c,v 1.8 2020/10/18 11:32:02 djm Exp $ */
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
	int oerrno, pair[2];
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
		error_f("helper \"%s\" unusable: %s", helper, strerror(errno));
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
			error_f("dup2: %s", strerror(errno));
			_exit(1);
		}
		close(pair[0]);
		close(pair[1]);
		closefrom(STDERR_FILENO + 1);
		debug_f("starting %s %s", helper,
		    verbosity == NULL ? "" : verbosity);
		execlp(helper, helper, verbosity, (char *)NULL);
		error_f("execlp: %s", strerror(errno));
		_exit(1);
	}
	close(pair[1]);

	/* success */
	debug3_f("started pid=%ld", (long)pid);
	*fdp = pair[0];
	*pidp = pid;
	*osigchldp = osigchld;
	return 0;
}

static int
reap_helper(pid_t pid)
{
	int status, oerrno;

	debug3_f("pid=%ld", (long)pid);

	errno = 0;
	while (waitpid(pid, &status, 0) == -1) {
		if (errno == EINTR) {
			errno = 0;
			continue;
		}
		oerrno = errno;
		error_f("waitpid: %s", strerror(errno));
		errno = oerrno;
		return SSH_ERR_SYSTEM_ERROR;
	}
	if (!WIFEXITED(status)) {
		error_f("helper exited abnormally");
		return SSH_ERR_AGENT_FAILURE;
	} else if (WEXITSTATUS(status) != 0) {
		error_f("helper exited with non-zero exit status");
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
		error_fr(r, "compose");
		goto out;
	}
	if ((r = ssh_msg_send(fd, SSH_SK_HELPER_VERSION, req)) != 0) {
		error_fr(r, "send");
		goto out;
	}
	if ((r = ssh_msg_recv(fd, resp)) != 0) {
		error_fr(r, "receive");
		goto out;
	}
	if ((r = sshbuf_get_u8(resp, &version)) != 0) {
		error_fr(r, "parse version");
		goto out;
	}
	if (version != SSH_SK_HELPER_VERSION) {
		error_f("unsupported version: got %u, expected %u",
		    version, SSH_SK_HELPER_VERSION);
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshbuf_get_u32(resp, &rtype)) != 0) {
		error_fr(r, "parse message type");
		goto out;
	}
	if (rtype == SSH_SK_HELPER_ERROR) {
		if ((r = sshbuf_get_u32(resp, &rerr)) != 0) {
			error_fr(r, "parse");
			goto out;
		}
		debug_f("helper returned error -%u", rerr);
		/* OpenSSH error values are negative; encoded as -err on wire */
		if (rerr == 0 || rerr >= INT_MAX)
			r = SSH_ERR_INTERNAL_ERROR;
		else
			r = -(int)rerr;
		goto out;
	} else if (rtype != type) {
		error_f("helper returned incorrect message type %u, "
		    "expecting %u", rtype, type);
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
		error_fr(r, "encode key");
		goto out;
	}
	if ((r = sshbuf_put_stringb(req, kbuf)) != 0 ||
	    (r = sshbuf_put_cstring(req, provider)) != 0 ||
	    (r = sshbuf_put_string(req, data, datalen)) != 0 ||
	    (r = sshbuf_put_cstring(req, NULL)) != 0 || /* alg */
	    (r = sshbuf_put_u32(req, compat)) != 0 ||
	    (r = sshbuf_put_cstring(req, pin)) != 0) {
		error_fr(r, "compose");
		goto out;
	}

	if ((fp = sshkey_fingerprint(key, SSH_FP_HASH_DEFAULT,
	    SSH_FP_DEFAULT)) == NULL) {
		error_f("sshkey_fingerprint failed");
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = client_converse(req, &resp, SSH_SK_HELPER_SIGN)) != 0)
		goto out;

	if ((r = sshbuf_get_string(resp, sigp, lenp)) != 0) {
		error_fr(r, "parse signature");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(resp) != 0) {
		error_f("trailing data in response");
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
		error_fr(r, "compose");
		goto out;
	}

	if ((r = client_converse(req, &resp, SSH_SK_HELPER_ENROLL)) != 0)
		goto out;

	if ((r = sshbuf_get_stringb(resp, kbuf)) != 0 ||
	    (r = sshbuf_get_stringb(resp, abuf)) != 0) {
		error_fr(r, "parse");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (sshbuf_len(resp) != 0) {
		error_f("trailing data in response");
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if ((r = sshkey_private_deserialize(kbuf, &key)) != 0) {
		error_fr(r, "encode");
		goto out;
	}
	if (attest != NULL && (r = sshbuf_putb(attest, abuf)) != 0) {
		error_fr(r, "encode attestation information");
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
		error_fr(r, "compose");
		goto out;
	}

	if ((r = client_converse(req, &resp, SSH_SK_HELPER_LOAD_RESIDENT)) != 0)
		goto out;

	while (sshbuf_len(resp) != 0) {
		/* key, comment */
		if ((r = sshbuf_get_stringb(resp, kbuf)) != 0 ||
		    (r = sshbuf_get_cstring(resp, NULL, NULL)) != 0) {
			error_fr(r, "parse signature");
			r = SSH_ERR_INVALID_FORMAT;
			goto out;
		}
		if ((r = sshkey_private_deserialize(kbuf, &key)) != 0) {
			error_fr(r, "decode key");
			goto out;
		}
		if ((tmp = recallocarray(keys, nkeys, nkeys + 1,
		    sizeof(*keys))) == NULL) {
			error_f("recallocarray keys failed");
			goto out;
		}
		debug_f("keys[%zu]: %s %s", nkeys, sshkey_type(key),
		    key->sk_application);
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
