/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 *
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 *	form: src/sys/i386/isa/intr_machdep.c,v 1.57 2001/07/20
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/vmmeter.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>

#define	MAX_STRAY_LOG	5

CTASSERT((1 << IV_SHIFT) == sizeof(struct intr_vector));
CTASSERT((1 << IQE_SHIFT) == sizeof(struct iqe));

ih_func_t *intr_handlers[NPIL];
struct	intr_vector intr_vectors[NIV];

u_long	intr_stray_count[NIV];

/* protect the intr_vectors table */
static struct	mtx intr_table_lock;

static void intr_stray_level(struct trapframe *tf);
static void intr_stray_vector(void *cookie);

void
intr_dequeue(struct trapframe *tf)
{
	struct intr_queue *iq;
	struct iqe *iqe;
	u_long head;
	u_long next;
	u_long tail;

	iq = PCPU_PTR(iq);
	for (head = iq->iq_head;; head = next) {
		for (tail = iq->iq_tail; tail != head;) {
			iqe = &iq->iq_queue[tail];
			atomic_add_long(&intrcnt[iqe->iqe_vec], 1);
			KASSERT(iqe->iqe_func != NULL,
			    ("intr_dequeue: iqe->iqe_func NULL"));
			iqe->iqe_func(iqe->iqe_arg);
			tail = (tail + 1) & IQ_MASK;
		}
		iq->iq_tail = tail;
		next = iq->iq_head;
		if (head == next)
			break;
	}
}

void
intr_setup(int pri, ih_func_t *ihf, int vec, iv_func_t *ivf, void *iva)
{
	u_long ps;

	ps = intr_disable();
	if (vec != -1) {
		intr_vectors[vec].iv_func = ivf;
		intr_vectors[vec].iv_arg = iva;
		intr_vectors[vec].iv_pri = pri;
		intr_vectors[vec].iv_vec = vec;
	}
	intr_handlers[pri] = ihf;
	intr_restore(ps);
}

static void
intr_stray_level(struct trapframe *tf)
{
	printf("stray level interrupt %d\n", tf->tf_level);
}

static void
intr_stray_vector(void *cookie)
{
	struct intr_vector *iv;

	iv = cookie;
	if (intr_stray_count[iv->iv_vec] < MAX_STRAY_LOG) {
		printf("stray vector interrupt %d\n", iv->iv_vec);
		atomic_add_long(&intr_stray_count[iv->iv_vec], 1);
		if (intr_stray_count[iv->iv_vec] >= MAX_STRAY_LOG)
			printf("got %d stray interrupt %d's: not logging "
			    "anymore\n", MAX_STRAY_LOG, iv->iv_vec);
	}
}

void
intr_init1()
{
	int i;

	/* Mark all interrupts as being stray. */
	for (i = 0; i < NPIL; i++)
		intr_handlers[i] = intr_stray_level;
	for (i = 0; i < NIV; i++) {
		intr_vectors[i].iv_func = intr_stray_vector;
		intr_vectors[i].iv_arg = &intr_vectors[i];
		intr_vectors[i].iv_pri = PIL_LOW;
		intr_vectors[i].iv_vec = i;
	}
	intr_handlers[PIL_LOW] = intr_dequeue;
}

void
intr_init2()
{

	mtx_init(&intr_table_lock, "ithread table lock", NULL, MTX_SPIN);
}

/* Schedule a heavyweight interrupt process. */
static void 
sched_ithd(void *cookie)
{
	struct intr_vector *iv;
	int error;

	iv = cookie;
#ifdef notyet
	error = ithread_schedule(iv->iv_ithd);
#else
	error = ithread_schedule(iv->iv_ithd, 0);
#endif
	if (error == EINVAL)
		intr_stray_vector(iv);
}

int
inthand_add(const char *name, int vec, void (*handler)(void *), void *arg,
    int flags, void **cookiep)
{
	struct intr_vector *iv;
	struct ithd *ithd;		/* descriptor for the IRQ */
	int errcode = 0;
	int created_ithd = 0;

	/*
	 * Work around a race where more than one CPU may be registering
	 * handlers on the same IRQ at the same time.
	 */
	iv = &intr_vectors[vec];
	mtx_lock_spin(&intr_table_lock);
	ithd = iv->iv_ithd;
	mtx_unlock_spin(&intr_table_lock);
	if (ithd == NULL) {
		errcode = ithread_create(&ithd, vec, 0, NULL, NULL, "intr%d:",
		    vec);
		if (errcode)
			return (errcode);
		mtx_lock_spin(&intr_table_lock);
		if (iv->iv_ithd == NULL) {
			iv->iv_ithd = ithd;
			created_ithd++;
			mtx_unlock_spin(&intr_table_lock);
		} else {
			struct ithd *orphan;

			orphan = ithd;
			ithd = iv->iv_ithd;
			mtx_unlock_spin(&intr_table_lock);
			ithread_destroy(orphan);
		}
	}

	errcode = ithread_add_handler(ithd, name, handler, arg,
	    ithread_priority(flags), flags, cookiep);
	
	if ((flags & INTR_FAST) == 0 || errcode) {
		intr_setup(PIL_ITHREAD, intr_dequeue, vec, sched_ithd, iv);
		errcode = 0;
	}

	if (errcode)
		return (errcode);
	
	if (flags & INTR_FAST)
		intr_setup(PIL_FAST, intr_dequeue, vec, handler, arg);

	intr_stray_count[vec] = 0;
	/* XXX: name is not yet used. */
	return (0);
}

int
inthand_remove(int vec, void *cookie)
{
	struct intr_vector *iv;
	int error;
	
	error = ithread_remove_handler(cookie);
	if (error == 0) {
		/*
		 * XXX: maybe this should be done regardless of whether
		 * ithread_remove_handler() succeeded?
		 */
		iv = &intr_vectors[vec];
		mtx_lock_spin(&intr_table_lock);
		if (iv->iv_ithd == NULL) {
			intr_setup(PIL_ITHREAD, intr_dequeue, vec,
			    intr_stray_vector, iv);
		} else {
			intr_setup(PIL_LOW, intr_dequeue, vec, sched_ithd, iv);
		}
		mtx_unlock_spin(&intr_table_lock);
	}
	return (error);
}
