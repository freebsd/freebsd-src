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
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/selinfo.h>
#include <sys/systm.h>

#include <dev/random/random_adaptors.h>
#include <dev/random/randomdev.h>

static struct mtx	pseudo_random_block_mtx;

/* Used to fake out unused random calls in random_adaptor */
void
random_null_func(void)
{
}

static int
pseudo_random_block_read(void *buf __unused, int c __unused)
{

	mtx_lock(&pseudo_random_block_mtx);

	printf("random(4) device is blocking.\n");
	msleep(pseudo_random_block_read, &pseudo_random_block_mtx, 0,
	    "block", 0);

	mtx_unlock(&pseudo_random_block_mtx);

	return (0);
}

static void
pseudo_random_block_init(void)
{

	mtx_init(&pseudo_random_block_mtx, "sleep mtx for random_block",
	    NULL, MTX_DEF);
}

static void
pseudo_random_block_deinit(void)
{

	mtx_destroy(&pseudo_random_block_mtx);
}

struct random_adaptor pseudo_random_block = {
	.ident = "pseudo-RNG that always blocks",
	.init = pseudo_random_block_init,
	.deinit = pseudo_random_block_deinit,
	.read = pseudo_random_block_read,
	.write = (random_write_func_t *)random_null_func,
	.reseed = (random_reseed_func_t *)random_null_func,
	.seeded = 1,
};

static int
pseudo_random_panic_read(void *buf, int c)
{

	panic("Insert a witty panic msg in here.");

	return (0);
}

struct random_adaptor pseudo_random_panic = {
	.ident = "pseudo-RNG that always panics on first read(2)",
	.init = (random_init_func_t *)random_null_func,
	.deinit = (random_deinit_func_t *)random_null_func,
	.read = pseudo_random_panic_read,
	.write = (random_write_func_t *)random_null_func,
	.reseed = (random_reseed_func_t *)random_null_func,
	.seeded = 1,
};

static int
pseudo_random_modevent(module_t mod, int type, void *unused)
{

	switch (type) {
	case MOD_LOAD:
		random_adaptor_register("block", &pseudo_random_block);
		EVENTHANDLER_INVOKE(random_adaptor_attach,
		    &pseudo_random_block);

		random_adaptor_register("panic", &pseudo_random_panic);

		return (0);
	}

	return (EINVAL);
}

RANDOM_ADAPTOR_MODULE(pseudo, pseudo_random_modevent, 1);
