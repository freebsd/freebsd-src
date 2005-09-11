/*
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
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

#include <openssl/rand.h>
#include <openssl/crypto.h>

#include "ssh.h"
#include "misc.h"
#include "xmalloc.h"
#include "atomicio.h"
#include "pathnames.h"
#include "log.h"

/*
 * Portable OpenSSH PRNG seeding:
 * If OpenSSL has not "internally seeded" itself (e.g. pulled data from
 * /dev/random), then we execute a "ssh-rand-helper" program which
 * collects entropy and writes it to stdout. The child program must
 * write at least RANDOM_SEED_SIZE bytes. The child is run with stderr
 * attached, so error/debugging output should be visible.
 *
 * XXX: we should tell the child how many bytes we need.
 */

RCSID("$Id: entropy.c,v 1.49 2005/07/17 07:26:44 djm Exp $");

#ifndef OPENSSL_PRNG_ONLY
#define RANDOM_SEED_SIZE 48
static uid_t original_uid, original_euid;
#endif

void
seed_rng(void)
{
#ifndef OPENSSL_PRNG_ONLY
	int devnull;
	int p[2];
	pid_t pid;
	int ret;
	unsigned char buf[RANDOM_SEED_SIZE];
	mysig_t old_sigchld;

	if (RAND_status() == 1) {
		debug3("RNG is ready, skipping seeding");
		return;
	}

	debug3("Seeding PRNG from %s", SSH_RAND_HELPER);

	if ((devnull = open("/dev/null", O_RDWR)) == -1)
		fatal("Couldn't open /dev/null: %s", strerror(errno));
	if (pipe(p) == -1)
		fatal("pipe: %s", strerror(errno));

	old_sigchld = signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == -1)
		fatal("Couldn't fork: %s", strerror(errno));
	if (pid == 0) {
		dup2(devnull, STDIN_FILENO);
		dup2(p[1], STDOUT_FILENO);
		/* Keep stderr open for errors */
		close(p[0]);
		close(p[1]);
		close(devnull);

		if (original_uid != original_euid &&
		    ( seteuid(getuid()) == -1 ||
		      setuid(original_uid) == -1) ) {
			fprintf(stderr, "(rand child) setuid(%li): %s\n",
			    (long int)original_uid, strerror(errno));
			_exit(1);
		}

		execl(SSH_RAND_HELPER, "ssh-rand-helper", NULL);
		fprintf(stderr, "(rand child) Couldn't exec '%s': %s\n",
		    SSH_RAND_HELPER, strerror(errno));
		_exit(1);
	}

	close(devnull);
	close(p[1]);

	memset(buf, '\0', sizeof(buf));
	ret = atomicio(read, p[0], buf, sizeof(buf));
	if (ret == -1)
		fatal("Couldn't read from ssh-rand-helper: %s",
		    strerror(errno));
	if (ret != sizeof(buf))
		fatal("ssh-rand-helper child produced insufficient data");

	close(p[0]);

	if (waitpid(pid, &ret, 0) == -1)
		fatal("Couldn't wait for ssh-rand-helper completion: %s",
		    strerror(errno));
	signal(SIGCHLD, old_sigchld);

	/* We don't mind if the child exits upon a SIGPIPE */
	if (!WIFEXITED(ret) &&
	    (!WIFSIGNALED(ret) || WTERMSIG(ret) != SIGPIPE))
		fatal("ssh-rand-helper terminated abnormally");
	if (WEXITSTATUS(ret) != 0)
		fatal("ssh-rand-helper exit with exit status %d", ret);

	RAND_add(buf, sizeof(buf), sizeof(buf));
	memset(buf, '\0', sizeof(buf));

#endif /* OPENSSL_PRNG_ONLY */
	if (RAND_status() != 1)
		fatal("PRNG is not seeded");
}

void
init_rng(void)
{
	/*
	 * OpenSSL version numbers: MNNFFPPS: major minor fix patch status
	 * We match major, minor, fix and status (not patch)
	 */
	if ((SSLeay() ^ OPENSSL_VERSION_NUMBER) & ~0xff0L)
		fatal("OpenSSL version mismatch. Built against %lx, you "
		    "have %lx", OPENSSL_VERSION_NUMBER, SSLeay());

#ifndef OPENSSL_PRNG_ONLY
	if ((original_uid = getuid()) == -1)
		fatal("getuid: %s", strerror(errno));
	if ((original_euid = geteuid()) == -1)
		fatal("geteuid: %s", strerror(errno));
#endif
}

