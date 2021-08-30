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

#define RANDOM_SEED_SIZE 48

#ifdef WITH_OPENSSL

#include <sys/types.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "openbsd-compat/openssl-compat.h"

#include "ssh.h"
#include "misc.h"
#include "xmalloc.h"
#include "atomicio.h"
#include "pathnames.h"
#include "log.h"
#include "sshbuf.h"
#include "ssherr.h"

/*
 * Portable OpenSSH PRNG seeding:
 * If OpenSSL has not "internally seeded" itself (e.g. pulled data from
 * /dev/random), then collect RANDOM_SEED_SIZE bytes of randomness from
 * PRNGd.
 */
#ifndef OPENSSL_PRNG_ONLY

void
rexec_send_rng_seed(struct sshbuf *m)
{
	u_char buf[RANDOM_SEED_SIZE];
	size_t len = sizeof(buf);
	int r;

	if (RAND_bytes(buf, sizeof(buf)) <= 0) {
		error("Couldn't obtain random bytes (error %ld)",
		    ERR_get_error());
		len = 0;
	}
	if ((r = sshbuf_put_string(m, buf, len)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	explicit_bzero(buf, sizeof(buf));
}

void
rexec_recv_rng_seed(struct sshbuf *m)
{
	const u_char *buf = NULL;
	size_t len = 0;
	int r;

	if ((r = sshbuf_get_string_direct(m, &buf, &len)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));

	debug3("rexec_recv_rng_seed: seeding rng with %lu bytes",
	    (unsigned long)len);
	RAND_add(buf, len, len);
}
#endif /* OPENSSL_PRNG_ONLY */

void
seed_rng(void)
{
	unsigned char buf[RANDOM_SEED_SIZE];

	/* Initialise libcrypto */
	ssh_libcrypto_init();

	if (!ssh_compatible_openssl(OPENSSL_VERSION_NUMBER,
	    OpenSSL_version_num()))
		fatal("OpenSSL version mismatch. Built against %lx, you "
		    "have %lx", (u_long)OPENSSL_VERSION_NUMBER,
		    OpenSSL_version_num());

#ifndef OPENSSL_PRNG_ONLY
	if (RAND_status() == 1)
		debug3("RNG is ready, skipping seeding");
	else {
		if (seed_from_prngd(buf, sizeof(buf)) == -1)
			fatal("Could not obtain seed from PRNGd");
		RAND_add(buf, sizeof(buf), sizeof(buf));
	}
#endif /* OPENSSL_PRNG_ONLY */

	if (RAND_status() != 1)
		fatal("PRNG is not seeded");

	/* Ensure arc4random() is primed */
	arc4random_buf(buf, sizeof(buf));
	explicit_bzero(buf, sizeof(buf));
}

#else /* WITH_OPENSSL */

#include <stdlib.h>
#include <string.h>

/* Actual initialisation is handled in arc4random() */
void
seed_rng(void)
{
	unsigned char buf[RANDOM_SEED_SIZE];

	/* Ensure arc4random() is primed */
	arc4random_buf(buf, sizeof(buf));
	explicit_bzero(buf, sizeof(buf));
}

#endif /* WITH_OPENSSL */
