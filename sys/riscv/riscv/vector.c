/*-
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <vm/uma.h>

#include <machine/vector.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/md_var.h>

static MALLOC_DEFINE(M_RVV_CTX, "rvv_ctx", "Contexts for user Vector state");

static void
vector_enable(void)
{
	uint64_t reg;

	reg = SSTATUS_VS_CLEAN;

	csr_set(sstatus, reg);
}

static void
vector_disable(void)
{
	uint64_t mask;

	mask = SSTATUS_VS_MASK;

	csr_clear(sstatus, mask);
}

static void
vector_state_store_common(struct pcb *p, void *datap)
{
	uint64_t vl;

	vector_enable();

	/* Store vector CSRs. */
	__asm __volatile(
		"csrr	%0, vstart\n\t"
		"csrr	%1, vtype\n\t"
		"csrr	%2, vl\n\t"
		"csrr	%3, vcsr\n\t"
		: "=r" (p->pcb_vstart), "=r" (p->pcb_vtype),
		  "=r" (p->pcb_vl), "=r" (p->pcb_vcsr) ::);

	if (datap == NULL)
		goto done;

	/* Store vector registers. */
	__asm __volatile(
		".option push\n\t"
		".option arch, +zve32x\n\t"
		"vsetvli	%0, x0, e8, m8, ta, ma\n\t"
		"vse8.v		v0, (%1)\n\t"
		"add		%1, %1, %0\n\t"
		"vse8.v		v8, (%1)\n\t"
		"add		%1, %1, %0\n\t"
		"vse8.v		v16, (%1)\n\t"
		"add		%1, %1, %0\n\t"
		"vse8.v		v24, (%1)\n\t"
		".option pop\n\t"
		: "=&r" (vl) : "r" (datap) : "memory");

done:
	vector_disable();
}

void
vector_state_store_savectx(struct pcb *p)
{

	if (!has_vector)
		return;

	/*
	 * savectx() will be called on panic with dumppcb as an argument.
	 * Save the vector CSR registers, but ignore the vector data.
	 */

	vector_state_store_common(p, NULL);
}

void
vector_state_store(struct thread *td)
{
	struct pcb *p;
	void *datap;

	p = td->td_pcb;

	KASSERT(p->pcb_vsaved != NULL, ("VS area is NULL"));

	datap = p->pcb_vsaved;

	vector_state_store_common(p, datap);
}

void
vector_state_restore(struct thread *td)
{
	struct pcb *p;
	void *datap;
	uint64_t vl;

	p = td->td_pcb;

	KASSERT(p->pcb_vsaved != NULL, ("VS area is NULL"));

	datap = p->pcb_vsaved;

	vector_enable();

	/* Restore vector registers. */
	__asm __volatile(
		".option push\n\t"
		".option arch, +zve32x\n\t"
		"vsetvli	%0, x0, e8, m8, ta, ma\n\t"
		"vle8.v		v0, (%1)\n\t"
		"add		%1, %1, %0\n\t"
		"vle8.v		v8, (%1)\n\t"
		"add		%1, %1, %0\n\t"
		"vle8.v		v16, (%1)\n\t"
		"add		%1, %1, %0\n\t"
		"vle8.v		v24, (%1)\n\t"
		".option pop\n\t"
		: "=&r" (vl) : "r" (datap) : "memory");

	/* Restore vector CSRs. */
	__asm __volatile(
		".option push\n\t"
		".option arch, +zve32x\n\t"
		"vsetvl	x0, %2, %1\n\t"
		".option pop\n\t"
		"csrw	vstart, %0\n\t"
		"csrw	vcsr, %3\n\t"
		:: "r" (p->pcb_vstart), "r" (p->pcb_vtype),
		   "r" (p->pcb_vl), "r" (p->pcb_vcsr));

	vector_disable();
}

int
vector_get_size(void)
{
	int len;

	/* RISC-V has 32 vector registers of vlenb size each. */

	vector_enable();
	len = csr_read(vlenb) * 32;
	vector_disable();

	return (len);
}

void
vector_state_init(struct thread *td)
{
	struct pcb *p;
	int len;

	p = td->td_pcb;

	KASSERT(p->pcb_vsaved == NULL, ("vsaved is already initialized"));

	len = vector_get_size();

	p->pcb_vsaved = malloc(len, M_RVV_CTX, M_WAITOK | M_ZERO);
}

void
vector_copy_thread(struct thread *td1, struct thread *td2)
{
	struct pcb *p1;
	struct pcb *p2;
	int len;

	/* Struct pcb already copied, now init the vector save area. */

	p1 = td1->td_pcb;
	p2 = td2->td_pcb;
	p2->pcb_vsaved = NULL;

	vector_state_init(td2);

	len = vector_get_size();

	memcpy(p2->pcb_vsaved, p1->pcb_vsaved, len);
}

static void
vector_thread_dtor(void *arg __unused, struct thread *td)
{
	void *datap;

	datap = td->td_pcb->pcb_vsaved;

	free(datap, M_RVV_CTX);
}

static void
vector_init(const void *dummy __unused)
{

	EVENTHANDLER_REGISTER(thread_dtor, vector_thread_dtor, NULL,
	    EVENTHANDLER_PRI_ANY);
}

SYSINIT(vector, SI_SUB_SMP, SI_ORDER_ANY, vector_init, NULL);
