/*-
 * Copyright (c) 2001 Doug Rabson
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
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <machine/sapicvar.h>
#include <machine/sapicreg.h>

static MALLOC_DEFINE(M_SAPIC, "sapic", "I/O SAPIC devices");

struct sapic_rte {
	u_int64_t	rte_vector		:8;
	u_int64_t	rte_delivery_mode	:3;
	u_int64_t	rte_destination_mode	:1;
	u_int64_t	rte_delivery_status	:1;
	u_int64_t	rte_polarity		:1;
	u_int64_t	rte_rirr		:1;
	u_int64_t	rte_trigger_mode	:1;
	u_int64_t	rte_mask		:1;
	u_int64_t	rte_flushen		:1;
	u_int64_t	rte_reserved		:30;
	u_int64_t	rte_destination_eid	:8;
	u_int64_t	rte_destination_id	:8;
};

static u_int32_t
sapic_read(struct sapic *sa, int which)
{
	vm_offset_t reg = sa->sa_registers;

	*(volatile u_int32_t *) (reg + SAPIC_IO_SELECT) = which;
	ia64_mf();
	ia64_mf_a();
	return *(volatile u_int32_t *) (reg + SAPIC_IO_WINDOW);
}

static void
sapic_write(struct sapic *sa, int which, u_int32_t value)
{
	vm_offset_t reg = sa->sa_registers;

	*(volatile u_int32_t *) (reg + SAPIC_IO_SELECT) = which;
	ia64_mf();
	ia64_mf_a();
	*(volatile u_int32_t *) (reg + SAPIC_IO_WINDOW) = value;
	ia64_mf();
	ia64_mf_a();
}

#if 0

static void
sapic_read_rte(struct sapic *sa, int which,
	       struct sapic_rte *rte)
{
	u_int32_t *p = (u_int32_t *) rte;
	critical_t c;
	c = cpu_critical_enter();
	p[0] = sapic_read(sa, SAPIC_RTE_BASE + 2*which);
	p[1] = sapic_read(sa, SAPIC_RTE_BASE + 2*which + 1);
	cpu_critical_exit(c);
}

#endif

static void
sapic_write_rte(struct sapic *sa, int which,
		struct sapic_rte *rte)
{
	u_int32_t *p = (u_int32_t *) rte;
	critical_t c;
	c = cpu_critical_enter();
	sapic_write(sa, SAPIC_RTE_BASE + 2*which, p[0]);
	sapic_write(sa, SAPIC_RTE_BASE + 2*which + 1, p[1]);
	cpu_critical_exit(c);
}

struct sapic *
sapic_create(int id, int base, u_int64_t address)
{
	struct sapic *sa;
	int max;

	sa = malloc(sizeof(struct sapic), M_SAPIC, M_NOWAIT);
	if (!sa)
		return 0;

	sa->sa_id = id;
	sa->sa_base = base;
	sa->sa_registers = IA64_PHYS_TO_RR6(address);

	max = (sapic_read(sa, SAPIC_VERSION) >> 16) & 0xff;
	sa->sa_limit = base + max;

	ia64_add_sapic(sa);

	return sa;
}

void
sapic_enable(struct sapic *sa, int input, int vector,
	     int trigger_mode, int polarity)
{
	struct sapic_rte rte;
	u_int64_t lid = ia64_get_lid();

	bzero(&rte, sizeof(rte));
	rte.rte_destination_id = (lid >> 24) & 15;
	rte.rte_destination_eid = (lid >> 16) & 15;
	rte.rte_trigger_mode = trigger_mode;
	rte.rte_polarity = polarity;
	rte.rte_delivery_mode = 0; /* fixed */
	rte.rte_vector = vector;
	sapic_write_rte(sa, input, &rte);
}

void
sapic_eoi(struct sapic *sa, int vector)
{
	vm_offset_t reg = sa->sa_registers;

	*(volatile u_int32_t *) (reg + SAPIC_APIC_EOI) = vector;
}

