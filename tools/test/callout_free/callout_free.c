/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Eric van Gyzen
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Free a pending callout.  This was useful for testing the
 * "show callout_last" ddb command.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/systm.h>

static struct callout callout_free;
static struct mtx callout_free_mutex;
static int callout_free_arg;

static void
callout_free_func(void *arg)
{
	printf("squirrel!\n");
	mtx_destroy(&callout_free_mutex);
	memset(&callout_free, 'C', sizeof(callout_free));
}

static int
callout_free_load(module_t mod, int cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MOD_LOAD:
		mtx_init(&callout_free_mutex, "callout_free", NULL, MTX_DEF);
		/*
		 * Do not pass CALLOUT_RETURNUNLOCKED so the callout
		 * subsystem will unlock the "destroyed" mutex.
		 */
		callout_init_mtx(&callout_free, &callout_free_mutex, 0);
		printf("callout_free_func = %p\n", callout_free_func);
		printf("callout_free_arg = %p\n", &callout_free_arg);
		callout_reset(&callout_free, hz/10, callout_free_func,
		    &callout_free_arg);
		error = 0;
		break;

	case MOD_UNLOAD:
		error = 0;
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

DEV_MODULE(callout_free, callout_free_load, NULL);
