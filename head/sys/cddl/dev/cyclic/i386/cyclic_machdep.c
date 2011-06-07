/*-
 * Copyright 2006-2008 John Birrell <jb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

static void enable(cyb_arg_t);
static void disable(cyb_arg_t);
static void reprogram(cyb_arg_t, hrtime_t);
static void xcall(cyb_arg_t, cpu_t *, cyc_func_t, void *);
static void cyclic_clock(struct trapframe *frame);

static cyc_backend_t	be	= {
	NULL,		/* cyb_configure */
	NULL,		/* cyb_unconfigure */
	enable,
	disable,
	reprogram,
	xcall,
	NULL		/* cyb_arg_t cyb_arg */
};

static void
cyclic_ap_start(void *dummy)
{
	/* Initialise the rest of the CPUs. */
	cyclic_clock_func = cyclic_clock;
	cyclic_mp_init();
}

SYSINIT(cyclic_ap_start, SI_SUB_SMP, SI_ORDER_ANY, cyclic_ap_start, NULL);

/*
 *  Machine dependent cyclic subsystem initialisation.
 */
static void
cyclic_machdep_init(void)
{
	/* Register the cyclic backend. */
	cyclic_init(&be);
}

static void
cyclic_machdep_uninit(void)
{
	/* De-register the cyclic backend. */
	cyclic_uninit();
}

/*
 * This function is the one registered by the machine dependent
 * initialiser as the callback for high speed timer events.
 */
static void
cyclic_clock(struct trapframe *frame)
{
	cpu_t *c = &solaris_cpu[curcpu];

	if (c->cpu_cyclic != NULL) {
		if (TRAPF_USERMODE(frame)) {
			c->cpu_profile_pc = 0;
			c->cpu_profile_upc = TRAPF_PC(frame);
		} else {
			c->cpu_profile_pc = TRAPF_PC(frame);
			c->cpu_profile_upc = 0;
		}

		c->cpu_intr_actv = 1;

		/* Fire any timers that are due. */
		cyclic_fire(c);

		c->cpu_intr_actv = 0;
	}
}

static void
enable(cyb_arg_t arg __unused)
{

}

static void
disable(cyb_arg_t arg __unused)
{

}

static void
reprogram(cyb_arg_t arg __unused, hrtime_t exp)
{
	struct bintime bt;
	struct timespec ts;

	ts.tv_sec = exp / 1000000000;
	ts.tv_nsec = exp % 1000000000;
	timespec2bintime(&ts, &bt);
	clocksource_cyc_set(&bt);
}

static void xcall(cyb_arg_t arg __unused, cpu_t *c, cyc_func_t func,
    void *param)
{
	cpuset_t cpus;

	CPU_SETOF(c->cpuid, &cpus);
	smp_rendezvous_cpus(cpus,
	    smp_no_rendevous_barrier, func, smp_no_rendevous_barrier, param);
}
