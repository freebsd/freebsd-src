/*-
 * Copyright (c) 2013 Arthur Mesh <arthurmesh@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/random.h>
#include <sys/systm.h>

#include <dev/random/live_entropy_sources.h>
#include <dev/random/random_adaptors.h>
#include <dev/random/randomdev.h>

static int random_example_read(void *, int);

struct random_adaptor random_example = {
	.ident = "Example RNG",
	.source = RANDOM_PURE_BOGUS,	/* Make sure this is in
					 * sys/random.h and is unique */
	.read = random_example_read,
};

/*
 * Used under the license provided @ http://xkcd.com/221/
 * http://creativecommons.org/licenses/by-nc/2.5/
 */
static uint8_t
getRandomNumber(void)
{
	return 4;   /* chosen by fair dice roll, guaranteed to be random */
}

static int
random_example_read(void *buf, int c)
{
	uint8_t *b;
	int count;

	b = buf;

	for (count = 0; count < c; count++)
		b[count] = getRandomNumber();

	printf("returning %d bytes of pure randomness\n", c);
	return (c);
}

static int
random_example_modevent(module_t mod, int type, void *unused)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		live_entropy_source_register(&random_example);
		break;

	case MOD_UNLOAD:
		live_entropy_source_deregister(&random_example);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

LIVE_ENTROPY_SRC_MODULE(live_entropy_source_example, random_example_modevent, 1);
