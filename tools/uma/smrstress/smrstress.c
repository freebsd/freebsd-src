/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Jeffrey Roberson <jeff@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD$
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/smp.h>
#include <sys/smr.h>

#include <vm/uma.h>

#include <machine/stdarg.h>

static uma_zone_t smrs_zone;
static smr_t smrs_smr;

static int smrs_cpus;
static int smrs_writers;
static int smrs_started;
static int smrs_iterations = 10000000;
static int smrs_failures = 0;
static volatile int smrs_completed;

struct smrs {
	int		generation;
	volatile u_int	count;
};

uintptr_t smrs_current;

static void
smrs_error(struct smrs *smrs, const char *fmt, ...)
{
	va_list ap;

	atomic_add_int(&smrs_failures, 1);
	printf("SMR ERROR: wr_seq %d, rd_seq %d, c_seq %d, generation %d, count %d ",
	    smrs_smr->c_shared->s_wr.seq, smrs_smr->c_shared->s_rd_seq,
	    zpcpu_get(smrs_smr)->c_seq, smrs->generation, smrs->count);
	va_start(ap, fmt);
	(void)vprintf(fmt, ap);
	va_end(ap);
}

static void
smrs_read(void)
{
	struct smrs *cur;
	int cnt;

	/* Wait for the writer to exit. */
	while (smrs_completed == 0) {
		smr_enter(smrs_smr);
		cur = (void *)atomic_load_acq_ptr(&smrs_current);
		if (cur->generation == -1)
			smrs_error(cur, "read early: Use after free!\n");
		atomic_add_int(&cur->count, 1);
		DELAY(100);
		cnt = atomic_fetchadd_int(&cur->count, -1);
		if (cur->generation == -1)
			smrs_error(cur, "read late: Use after free!\n");
		else if (cnt <= 0)
			smrs_error(cur, "Invalid ref\n");
		smr_exit(smrs_smr);
		maybe_yield();
	}
}

static void
smrs_write(void)
{
	struct smrs *cur;
	int i;

	for (i = 0; i < smrs_iterations; i++) {
		cur = uma_zalloc_smr(smrs_zone, M_WAITOK);
		atomic_thread_fence_rel();
		cur = (void *)atomic_swap_ptr(&smrs_current, (uintptr_t)cur);
		uma_zfree_smr(smrs_zone, cur);
	}
}

static void
smrs_thread(void *arg)
{
	int rw = (intptr_t)arg;

	if (rw < smrs_writers)
		smrs_write();
	else
		smrs_read();
	atomic_add_int(&smrs_completed, 1);
	kthread_exit();
}

static void
smrs_start(void)
{
	struct smrs *cur;
	int i;

	smrs_cpus = mp_ncpus;
	if (mp_ncpus > 3)
		smrs_writers = 2;
	else
		smrs_writers = 1;
	smrs_started = smrs_cpus;
	smrs_completed = 0;
	atomic_store_rel_ptr(&smrs_current,
	    (uintptr_t)uma_zalloc_smr(smrs_zone, M_WAITOK));
	for (i = 0; i < smrs_started; i++)
		kthread_add((void (*)(void *))smrs_thread,
		    (void *)(intptr_t)i, curproc, NULL, 0, 0, "smrs-%d", i);

	while (smrs_completed != smrs_started)
		pause("prf", hz/2);

	cur = (void *)smrs_current;
	smrs_current = (uintptr_t)NULL;
	uma_zfree_smr(smrs_zone, cur);

	printf("Completed %d loops with %d failures\n",
	    smrs_iterations, smrs_failures);
}

static int
smrs_ctor(void *mem, int size, void *arg, int flags)
{
	static int smr_generation = 0;
	struct smrs *smrs = mem;

	if (smrs->generation != -1 && smrs->generation != 0)
		smrs_error(smrs, "ctor: Invalid smr generation on ctor\n");
	else if (smrs->count != 0)
		smrs_error(smrs, "ctor: Invalid count\n");
	smrs->generation = ++smr_generation;

	return (0);
}


static void
smrs_dtor(void *mem, int size, void *arg)
{
	struct smrs *smrs = mem;

	if (smrs->generation == -1)
		smrs_error(smrs, "dtor: Invalid generation");
	smrs->generation = -1;
	if (smrs->count != 0)
		smrs_error(smrs, "dtor: Invalid count\n");
}


static void
smrs_init(void)
{

	smrs_zone = uma_zcreate("smrs", sizeof(struct smrs),
	    smrs_ctor,  smrs_dtor, NULL, NULL, UMA_ALIGN_PTR,
	    UMA_ZONE_SMR | UMA_ZONE_ZINIT);
        smrs_smr = uma_zone_get_smr(smrs_zone);
}

static void
smrs_fini(void)
{

	uma_zdestroy(smrs_zone);
}

static int
smrs_modevent(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		smrs_init();
		smrs_start();
		break;
	case MOD_UNLOAD:
		smrs_fini();
		break;
	default:
		break;
	}
	return (0);
}

moduledata_t smrs_meta = {
	"smrstress",
	smrs_modevent,
	NULL
};
DECLARE_MODULE(smrstress, smrs_meta, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(smrstress, 1);
