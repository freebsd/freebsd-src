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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pcpu.h>

#include <machine/frame.h>
#include <machine/globals.h>
#include <machine/intr_machdep.h>

#define	NCPU	MAXCPU

struct	intr_handler intr_handlers[NPIL];
struct	intr_queue intr_queues[NCPU];
struct	intr_vector intr_vectors[NIV];

void
intr_dequeue(struct trapframe *tf)
{
	struct intr_queue *iq;
	struct iqe *iqe;
	u_long head;
	u_long next;
	u_long tail;

	iq = PCPU_GET(iq);
	for (head = iq->iq_head;; head = next) {
		for (tail = iq->iq_tail; tail != head;) {
			iqe = &iq->iq_queue[tail];
			if (iqe->iqe_func != NULL)
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
	if (vec != -1) {
		intr_vectors[vec].iv_func = ivf;
		intr_vectors[vec].iv_arg = iva;
		intr_vectors[vec].iv_pri = pri;
	}
	intr_handlers[pri].ih_func = ihf;
}
