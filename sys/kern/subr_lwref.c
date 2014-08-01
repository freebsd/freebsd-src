/*-
 * Copyright (c) 2014 Gleb Smirnoff <glebius@FreeBSD.org>
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>		/* XXXGL: M_TEMP */
#include <sys/mutex.h>
#include <sys/lwref.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/pcpu.h>

#include <machine/frame.h>
#include <machine/pcb.h>

struct lwref {
	void		*ptr;
	counter_u64_t	refcnt;
	struct mtx	mtx;
};

static void lwref_change_action(void *v);

#ifdef INVARIANTS
extern void Xrendezvous(void);
extern void Xtimerint(void);
#endif
 
lwref_t
lwref_alloc(void *ptr, int flags)
{
	lwref_t lwr;

	lwr = malloc(sizeof(*lwr), M_TEMP, flags | M_ZERO);
	if (lwr == NULL)
		return (NULL);
	lwr->refcnt = counter_u64_alloc(flags);
	if (lwr->refcnt == NULL) {
		free(lwr, M_TEMP);
		return (NULL);
	}
	lwr->ptr = ptr;
	mtx_init(&lwr->mtx, "lwref", NULL, MTX_DEF);

	return (lwr);
}

struct lwref_change_ctx {
	lwref_t		lwr;
	void		*newptr;
	counter_u64_t	newcnt;
	u_int		cpu;
	u_int		oldcnt;
};

static void
lwref_fixup_rip(register_t *rip, const char *p)
{

	if (*rip >= (register_t )lwref_acquire &&
	    *rip < (register_t )lwref_acquire_ponr) {
		if (p)
			printf("%s: %p\n", p, (void *)*rip);
		*rip = (register_t )lwref_acquire;
	}
}

static void
lwref_fixup_td(void *arg)
{
	struct thread *td = arg;
	register_t *rbp;

	/*
	 * The timer interrupt trapframe is 3 functions deep:
	 * Xtimerint -> lapic_handle_timer -> mi_switch -> sched_switch,
	 * so in 99% this would work:
	 *
	 * tf = (struct trapframe *)
	 *   ((register_t *)(***(void ****)(td->td_pcb->pcb_rbp)) + 2);
	 *
	 */
	for (rbp = (register_t *)td->td_pcb->pcb_rbp;
	    rbp && rbp < (register_t *)*rbp;
	    rbp = (register_t *)*rbp) {
		struct trapframe *tf;
		register_t rip = (register_t )*(rbp + 1);

		if (
		    rip == (register_t )timerint_ret ||
		    rip == (register_t )apic_isr1_ret ||
		    rip == (register_t )apic_isr2_ret ||
		    rip == (register_t )apic_isr3_ret ||
		    rip == (register_t )apic_isr4_ret ||
		    rip == (register_t )apic_isr5_ret ||
		    rip == (register_t )apic_isr6_ret ||
		    rip == (register_t )apic_isr7_ret ||
		    rip == (register_t )ipi_intr_bitmap_handler_ret
		    ) {
			struct trapframe *tf;

			tf = (struct trapframe *)(rbp + 2);
			lwref_fixup_rip(&tf->tf_rip, __func__);
		}

		tf = (struct trapframe *)(rbp + 2);
		if (tf->tf_rip > (register_t )lwref_acquire &&
		    tf->tf_rip < (register_t )lwref_acquire_ponr)
			panic("lwref deteceted\n");
	}
}

static void
lwref_change_action(void *v)
{
	struct lwref_change_ctx *ctx = v;
	lwref_t lwr = ctx->lwr;
	struct trapframe *tf;

	atomic_add_int(&ctx->oldcnt, *(uint64_t *)zpcpu_get(lwr->refcnt));

	lwr->ptr = ctx->newptr;
	lwr->refcnt = ctx->newcnt;

	sched_foreach_on_runq(lwref_fixup_td);

	if (ctx->cpu == curcpu)
		/* We are not in IPI. */
		return;

	/*
	 *  We are in IPI and we need to check the trap frame of
	 *  the IPI, whether we interrupted lwref_acquire().
	 *
	 *  We are two functions deep below the trap frame:
	 *  Xrendezvous -> smp_rendezvous_action -> lwref_change_action
	 */
	KASSERT(__builtin_return_address(0) > (void *)&smp_rendezvous_action &&
	    __builtin_return_address(0) <= (void *)((char *)&smp_rendezvous_action + 506) &&
	    __builtin_return_address(1) > (void *)&Xrendezvous &&
	    __builtin_return_address(1) <= (void *)((char *)&Xrendezvous + 201),
	    ("%p called via invalid path: 0 %p 1 %p", __func__,
	    __builtin_return_address(0), __builtin_return_address(1)));

	/* After pushed %rbp and %rip begins the trap frame. */
	tf = (struct trapframe *)
	    ((register_t *)__builtin_frame_address(1) + 2);
	lwref_fixup_rip(&tf->tf_rip, __func__);
}

int
lwref_change(lwref_t lwr, void *newptr, void (*freefn)(void *, void *),
    void *freearg)
{
	struct lwref_change_ctx ctx;
	counter_u64_t orefcnt;
	void *optr;

	ctx.newcnt = counter_u64_alloc(M_WAITOK);	/* XXXGL */
	ctx.oldcnt = 0;

	mtx_lock(&lwr->mtx);
	optr = lwr->ptr;
	orefcnt = lwr->refcnt;
	ctx.lwr = lwr;
	ctx.newptr = newptr;
	ctx.cpu = curcpu;	/* XXXGL: race */
	smp_rendezvous(smp_no_rendevous_barrier, lwref_change_action,
	    smp_no_rendevous_barrier, &ctx);
	mtx_unlock(&lwr->mtx);

	if (ctx.oldcnt == 0) {
		(freefn)(freearg, optr);
		counter_u64_free(orefcnt);
	} else
		printf("Leaking %p with cnt %p %u (%ju) refs\n",
		    optr, orefcnt, ctx.oldcnt, (uintmax_t )counter_u64_fetch(orefcnt));

	return (0);
}
