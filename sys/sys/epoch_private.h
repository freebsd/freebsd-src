/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018, Matthew Macy <mmacy@freebsd.org>
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
 *
 * $FreeBSD$
 */

#ifndef _SYS_EPOCH_PRIVATE_H_
#define _SYS_EPOCH_PRIVATE_H_
#ifndef _KERNEL
#error "no user serviceable parts"
#else
#include <ck_epoch.h>
#include <sys/kpilite.h>

#include <sys/mutex.h>

extern void epoch_adjust_prio(struct thread *td, u_char prio);
#ifndef _SYS_SYSTM_H_
extern void    critical_exit_preempt(void);
#endif

#ifdef __amd64__
#define EPOCH_ALIGN CACHE_LINE_SIZE*2
#else
#define EPOCH_ALIGN CACHE_LINE_SIZE
#endif

/*
 * Standalone (_sa) routines for thread state manipulation
 */
static __inline void
critical_enter_sa(void *tdarg)
{
	struct thread_lite *td;

	td = tdarg;
	td->td_critnest++;
	__compiler_membar();
}

static __inline void
critical_exit_sa(void *tdarg)
{
	struct thread_lite *td;

	td = tdarg;
	MPASS(td->td_critnest > 0);
	__compiler_membar();
	td->td_critnest--;
	__compiler_membar();
	if (__predict_false(td->td_owepreempt != 0))
		critical_exit_preempt();
}

typedef struct epoch_thread {
#ifdef INVARIANTS
	uint64_t et_magic_pre;
#endif
	TAILQ_ENTRY(epoch_thread) et_link;	/* Epoch queue. */
	struct thread *et_td;		/* pointer to thread in section */
	ck_epoch_section_t et_section; /* epoch section object */
#ifdef INVARIANTS
	uint64_t et_magic_post;
#endif
} *epoch_thread_t;
TAILQ_HEAD (epoch_tdlist, epoch_thread);

typedef struct epoch_record {
	ck_epoch_record_t er_record;
	volatile struct epoch_tdlist er_tdlist;
	volatile uint32_t er_gen;
	uint32_t er_cpuid;
} __aligned(EPOCH_ALIGN)     *epoch_record_t;

struct epoch {
	struct ck_epoch e_epoch __aligned(EPOCH_ALIGN);
	struct epoch_record *e_pcpu_dom[MAXMEMDOM] __aligned(EPOCH_ALIGN);
	int	e_idx;
	int	e_flags;
	struct epoch_record *e_pcpu[0];
};

#define INIT_CHECK(epoch)							\
	do {											\
		if (__predict_false((epoch) == NULL))		\
			return;									\
	} while (0)

static __inline void
epoch_enter_preempt(epoch_t epoch, epoch_tracker_t et)
{
	struct epoch_record *er;
	struct epoch_thread *etd;
	struct thread_lite *td;
	MPASS(cold || epoch != NULL);
	INIT_CHECK(epoch);
	etd = (void *)et;
#ifdef INVARIANTS
	MPASS(epoch->e_flags & EPOCH_PREEMPT);
	etd->et_magic_pre = EPOCH_MAGIC0;
	etd->et_magic_post = EPOCH_MAGIC1;
#endif
	td = (struct thread_lite *)curthread;
	etd->et_td = (void*)td;
	td->td_epochnest++;
	critical_enter_sa(td);
	sched_pin_lite(td);

	td->td_pre_epoch_prio = td->td_priority;
	er = epoch->e_pcpu[curcpu];
	TAILQ_INSERT_TAIL(&er->er_tdlist, etd, et_link);
	ck_epoch_begin(&er->er_record, (ck_epoch_section_t *)&etd->et_section);
	critical_exit_sa(td);
}

static __inline void
epoch_enter(epoch_t epoch)
{
	ck_epoch_record_t *record;
	struct thread_lite *td;
	MPASS(cold || epoch != NULL);
	INIT_CHECK(epoch);
	td = (struct thread_lite *)curthread;

	td->td_epochnest++;
	critical_enter_sa(td);
	record = &epoch->e_pcpu[curcpu]->er_record;
	ck_epoch_begin(record, NULL);
}

static __inline void
epoch_exit_preempt(epoch_t epoch, epoch_tracker_t et)
{
	struct epoch_record *er;
	struct epoch_thread *etd;
	struct thread_lite *td;

	INIT_CHECK(epoch);
	td = (struct thread_lite *)curthread;
	critical_enter_sa(td);
	sched_unpin_lite(td);
	MPASS(td->td_epochnest);
	td->td_epochnest--;
	er = epoch->e_pcpu[curcpu];
	MPASS(epoch->e_flags & EPOCH_PREEMPT);
	etd = (void *)et;
#ifdef INVARIANTS
	MPASS(etd != NULL);
	MPASS(etd->et_td == (struct thread *)td);
	MPASS(etd->et_magic_pre == EPOCH_MAGIC0);
	MPASS(etd->et_magic_post == EPOCH_MAGIC1);
	etd->et_magic_pre = 0;
	etd->et_magic_post = 0;
	etd->et_td = (void*)0xDEADBEEF;
#endif
	ck_epoch_end(&er->er_record,
		(ck_epoch_section_t *)&etd->et_section);
	TAILQ_REMOVE(&er->er_tdlist, etd, et_link);
	er->er_gen++;
	if (__predict_false(td->td_pre_epoch_prio != td->td_priority))
		epoch_adjust_prio((struct thread *)td, td->td_pre_epoch_prio);
	critical_exit_sa(td);
}

static __inline void
epoch_exit(epoch_t epoch)
{
	ck_epoch_record_t *record;
	struct thread_lite *td;

	INIT_CHECK(epoch);
	td = (struct thread_lite *)curthread;
	MPASS(td->td_epochnest);
	td->td_epochnest--;
	record = &epoch->e_pcpu[curcpu]->er_record;
	ck_epoch_end(record, NULL);
	critical_exit_sa(td);
}
#endif /* _KERNEL */
#endif /* _SYS_EPOCH_PRIVATE_H_ */
