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
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/random.h>
#include <sys/selinfo.h>
#include <sys/systm.h>
#include <sys/time.h>

#include <dev/random/random_adaptors.h>
#include <dev/random/randomdev.h>

static struct mtx	dummy_random_mtx;

/* Used to fake out unused random calls in random_adaptor */
static void
random_null_func(void)
{
}

static int
dummy_random_poll(int events __unused, struct thread *td __unused)
{

	return (0);
}

static int
dummy_random_block(int flag)
{
	int error = 0;

	mtx_lock(&dummy_random_mtx);

	/* Blocking logic */
	while (!error) {
		if (flag & O_NONBLOCK)
			error = EWOULDBLOCK;
		else {
			printf("random: dummy device blocking on read.\n");
			error = msleep(&dummy_random_block,
			    &dummy_random_mtx,
			    PUSER | PCATCH, "block", 0);
		}
	}
	mtx_unlock(&dummy_random_mtx);

	return (error);
}

static void
dummy_random_init(void)
{

	mtx_init(&dummy_random_mtx, "sleep mtx for dummy_random",
	    NULL, MTX_DEF);
}

static void
dummy_random_deinit(void)
{

	mtx_destroy(&dummy_random_mtx);
}

struct random_adaptor dummy_random = {
	.ident = "Dummy entropy device that always blocks",
	.init = dummy_random_init,
	.deinit = dummy_random_deinit,
	.block = dummy_random_block,
	.poll = dummy_random_poll,
	.read = (random_read_func_t *)random_null_func,
	.reseed = (random_reseed_func_t *)random_null_func,
	.seeded = 0, /* This device can never be seeded */
	.priority = 1, /* Bottom priority, so goes to last position */
};

static int
dummy_random_modevent(module_t mod __unused, int type, void *unused __unused)
{

	switch (type) {
	case MOD_LOAD:
		random_adaptor_register("dummy", &dummy_random);
		EVENTHANDLER_INVOKE(random_adaptor_attach,
		    &dummy_random);

		return (0);
	}

	return (EINVAL);
}

RANDOM_ADAPTOR_MODULE(dummy, dummy_random_modevent, 1);
