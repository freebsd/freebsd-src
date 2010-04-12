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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/sysctl.h>

#include <machine/intr.h>
#include <machine/pal.h>

#include <vm/vm.h>
#include <vm/pmap.h>

/*
 * Offsets from the SAPIC base in memory. Most registers are accessed
 * by indexing using the SAPIC_IO_SELECT register.
 */
#define	SAPIC_IO_SELECT		0x00
#define	SAPIC_IO_WINDOW		0x10
#define	SAPIC_APIC_EOI		0x40

/*
 * Indexed registers.
 */
#define SAPIC_ID		0x00
#define SAPIC_VERSION		0x01
#define SAPIC_ARBITRATION_ID	0x02
#define SAPIC_RTE_BASE		0x10

/* Interrupt polarity. */
#define	SAPIC_POLARITY_HIGH	0
#define	SAPIC_POLARITY_LOW	1

/* Interrupt trigger. */
#define	SAPIC_TRIGGER_EDGE	0
#define	SAPIC_TRIGGER_LEVEL	1

/* Interrupt delivery mode. */
#define	SAPIC_DELMODE_FIXED	0
#define	SAPIC_DELMODE_LOWPRI	1
#define	SAPIC_DELMODE_PMI	2
#define	SAPIC_DELMODE_NMI	4
#define	SAPIC_DELMODE_INIT	5
#define	SAPIC_DELMODE_EXTINT	7

struct sapic {
	struct mtx	sa_mtx;
	uint64_t	sa_registers;	/* virtual address of sapic */
	u_int		sa_id;		/* I/O SAPIC Id */
	u_int		sa_base;	/* ACPI vector base */
	u_int		sa_limit;	/* last ACPI vector handled here */
};

struct sapic_rte {
	uint64_t	rte_vector		:8;
	uint64_t	rte_delivery_mode	:3;
	uint64_t	rte_destination_mode	:1;
	uint64_t	rte_delivery_status	:1;
	uint64_t	rte_polarity		:1;
	uint64_t	rte_rirr		:1;
	uint64_t	rte_trigger_mode	:1;
	uint64_t	rte_mask		:1;
	uint64_t	rte_flushen		:1;
	uint64_t	rte_reserved		:30;
	uint64_t	rte_destination_eid	:8;
	uint64_t	rte_destination_id	:8;
};

MALLOC_DEFINE(M_SAPIC, "sapic", "I/O SAPIC devices");

struct sapic *ia64_sapics[16];		/* XXX make this resizable */
int ia64_sapic_count;

static int sysctl_machdep_apic(SYSCTL_HANDLER_ARGS);

SYSCTL_OID(_machdep, OID_AUTO, apic, CTLTYPE_STRING|CTLFLAG_RD,
    NULL, 0, sysctl_machdep_apic, "A", "(x)APIC redirection table entries");

static __inline uint32_t
sapic_read(struct sapic *sa, int which)
{
	uint32_t value;

	ia64_st4((void *)(sa->sa_registers + SAPIC_IO_SELECT), which);
	ia64_mf_a();
	value = ia64_ld4((void *)(sa->sa_registers + SAPIC_IO_WINDOW));
	return (value);
}

static __inline void
sapic_write(struct sapic *sa, int which, uint32_t value)
{

	ia64_st4((void *)(sa->sa_registers + SAPIC_IO_SELECT), which);
	ia64_mf_a();
	ia64_st4((void *)(sa->sa_registers + SAPIC_IO_WINDOW), value);
	ia64_mf_a();
}

static __inline void
sapic_read_rte(struct sapic *sa, int which, struct sapic_rte *rte)
{
	uint32_t *p = (uint32_t *) rte;

	p[0] = sapic_read(sa, SAPIC_RTE_BASE + 2 * which);
	p[1] = sapic_read(sa, SAPIC_RTE_BASE + 2 * which + 1);
}

static __inline void
sapic_write_rte(struct sapic *sa, int which, struct sapic_rte *rte)
{
	uint32_t *p = (uint32_t *) rte;

	sapic_write(sa, SAPIC_RTE_BASE + 2 * which, p[0]);
	sapic_write(sa, SAPIC_RTE_BASE + 2 * which + 1, p[1]);
}

struct sapic *
sapic_lookup(u_int irq, u_int *vecp)
{
	struct sapic_rte rte;
	struct sapic *sa;
	int i;

	for (i = 0; i < ia64_sapic_count; i++) {
		sa = ia64_sapics[i];
		if (irq >= sa->sa_base && irq <= sa->sa_limit) {
			if (vecp != NULL) {
				mtx_lock_spin(&sa->sa_mtx);
				sapic_read_rte(sa, irq - sa->sa_base, &rte);
				mtx_unlock_spin(&sa->sa_mtx);
				*vecp = rte.rte_vector;
			}
			return (sa);
		}
	}

	return (NULL);
}


int
sapic_bind_intr(u_int irq, struct pcpu *pc)
{
	struct sapic_rte rte;
	struct sapic *sa;

	sa = sapic_lookup(irq, NULL);
	if (sa == NULL)
		return (EINVAL);

	mtx_lock_spin(&sa->sa_mtx);
	sapic_read_rte(sa, irq - sa->sa_base, &rte);
	rte.rte_destination_id = (pc->pc_md.lid >> 24) & 255;
	rte.rte_destination_eid = (pc->pc_md.lid >> 16) & 255;
	rte.rte_delivery_mode = SAPIC_DELMODE_FIXED;
	sapic_write_rte(sa, irq - sa->sa_base, &rte);
	mtx_unlock_spin(&sa->sa_mtx);
	return (0);
}

int
sapic_config_intr(u_int irq, enum intr_trigger trig, enum intr_polarity pol)
{
	struct sapic_rte rte;
	struct sapic *sa;

	sa = sapic_lookup(irq, NULL);
	if (sa == NULL)
		return (EINVAL);

	mtx_lock_spin(&sa->sa_mtx);
	sapic_read_rte(sa, irq - sa->sa_base, &rte);
	if (trig != INTR_TRIGGER_CONFORM)
		rte.rte_trigger_mode = (trig == INTR_TRIGGER_EDGE) ?
		    SAPIC_TRIGGER_EDGE : SAPIC_TRIGGER_LEVEL;
	else
		rte.rte_trigger_mode = (irq < 16) ? SAPIC_TRIGGER_EDGE :
		    SAPIC_TRIGGER_LEVEL;
	if (pol != INTR_POLARITY_CONFORM)
		rte.rte_polarity = (pol == INTR_POLARITY_HIGH) ?
		    SAPIC_POLARITY_HIGH : SAPIC_POLARITY_LOW;
	else
		rte.rte_polarity = (irq < 16) ? SAPIC_POLARITY_HIGH :
		    SAPIC_POLARITY_LOW;
	sapic_write_rte(sa, irq - sa->sa_base, &rte);
	mtx_unlock_spin(&sa->sa_mtx);
	return (0);
}

struct sapic *
sapic_create(u_int id, u_int base, uint64_t address)
{
	struct sapic_rte rte;
	struct sapic *sa;
	u_int i, max;

	sa = malloc(sizeof(struct sapic), M_SAPIC, M_ZERO | M_NOWAIT);
	if (sa == NULL)
		return (NULL);

	sa->sa_id = id;
	sa->sa_base = base;
	sa->sa_registers = (uintptr_t)pmap_mapdev(address, 1048576);

	mtx_init(&sa->sa_mtx, "I/O SAPIC lock", NULL, MTX_SPIN);

	max = (sapic_read(sa, SAPIC_VERSION) >> 16) & 0xff;
	sa->sa_limit = base + max;

	ia64_sapics[ia64_sapic_count++] = sa;

	/*
	 * Initialize all RTEs with a default trigger mode and polarity.
	 * This may be changed later by calling sapic_config_intr(). We
	 * mask all interrupts by default.
	 */
	bzero(&rte, sizeof(rte));
	rte.rte_mask = 1;
	for (i = base; i <= sa->sa_limit; i++) {
		rte.rte_trigger_mode = (i < 16) ? SAPIC_TRIGGER_EDGE :
		    SAPIC_TRIGGER_LEVEL;
		rte.rte_polarity = (i < 16) ? SAPIC_POLARITY_HIGH :
		    SAPIC_POLARITY_LOW;
		sapic_write_rte(sa, i - base, &rte);
	}

	return (sa);
}

int
sapic_enable(struct sapic *sa, u_int irq, u_int vector)
{
	struct sapic_rte rte;
	uint64_t lid = ia64_get_lid();

	mtx_lock_spin(&sa->sa_mtx);
	sapic_read_rte(sa, irq - sa->sa_base, &rte);
	rte.rte_destination_id = (lid >> 24) & 255;
	rte.rte_destination_eid = (lid >> 16) & 255;
	rte.rte_delivery_mode = SAPIC_DELMODE_FIXED;
	rte.rte_vector = vector;
	rte.rte_mask = 0;
	sapic_write_rte(sa, irq - sa->sa_base, &rte);
	mtx_unlock_spin(&sa->sa_mtx);
	return (0);
}

void
sapic_eoi(struct sapic *sa, u_int vector)
{

	ia64_st4((void *)(sa->sa_registers + SAPIC_APIC_EOI), vector);
	ia64_mf_a();
}

/* Expected to be called with interrupts disabled. */
void
sapic_mask(struct sapic *sa, u_int irq)
{
	struct sapic_rte rte;

	mtx_lock_spin(&sa->sa_mtx);
	sapic_read_rte(sa, irq - sa->sa_base, &rte);
	rte.rte_mask = 1;
	sapic_write_rte(sa, irq - sa->sa_base, &rte);
	mtx_unlock_spin(&sa->sa_mtx);
}

/* Expected to be called with interrupts disabled. */
void
sapic_unmask(struct sapic *sa, u_int irq)
{
	struct sapic_rte rte;

	mtx_lock_spin(&sa->sa_mtx);
	sapic_read_rte(sa, irq - sa->sa_base, &rte);
	rte.rte_mask = 0;
	sapic_write_rte(sa, irq - sa->sa_base, &rte);
	mtx_unlock_spin(&sa->sa_mtx);
}

static int
sysctl_machdep_apic(SYSCTL_HANDLER_ARGS)
{
	char buf[80];
	struct sapic_rte rte;
	struct sapic *sa;
	int apic, count, error, index, len;

	len = sprintf(buf, "\n    APIC Idx: Id,EId : RTE\n");
	error = SYSCTL_OUT(req, buf, len);
	if (error)
		return (error);

	for (apic = 0; apic < ia64_sapic_count; apic++) {
		sa = ia64_sapics[apic];
		count = sa->sa_limit - sa->sa_base + 1;
		for (index = 0; index < count; index++) {
			mtx_lock_spin(&sa->sa_mtx);
			sapic_read_rte(sa, index, &rte);
			mtx_unlock_spin(&sa->sa_mtx);
			if (rte.rte_vector == 0)
				continue;
			len = sprintf(buf,
    "    0x%02x %3d: (%02x,%02x): %3d %d %d %s %s %s %s %s\n",
			    sa->sa_id, index,
			    rte.rte_destination_id, rte.rte_destination_eid,
			    rte.rte_vector, rte.rte_delivery_mode,
			    rte.rte_destination_mode,
			    rte.rte_delivery_status ? "DS" : "  ",
			    rte.rte_polarity ? "low-active " : "high-active",
			    rte.rte_rirr ? "RIRR" : "    ",
			    rte.rte_trigger_mode ? "level" : "edge ",
			    rte.rte_flushen ? "F" : " ");
			error = SYSCTL_OUT(req, buf, len);
			if (error)
				return (error);
		}
	}

	return (0);
}

#ifdef DDB

#include <ddb/ddb.h>

void
sapic_print(struct sapic *sa, u_int irq)
{
	struct sapic_rte rte;

	db_printf("sapic=%u, irq=%u: ", sa->sa_id, irq);
	sapic_read_rte(sa, irq - sa->sa_base, &rte);
	db_printf("%3d %x->%x:%x %d %s %s %s %s %s %s\n", rte.rte_vector,
	    rte.rte_delivery_mode,
	    rte.rte_destination_id, rte.rte_destination_eid,
	    rte.rte_destination_mode,
	    rte.rte_delivery_status ? "DS" : "  ",
	    rte.rte_polarity ? "low-active " : "high-active",
	    rte.rte_rirr ? "RIRR" : "    ",
	    rte.rte_trigger_mode ? "level" : "edge ",
	    rte.rte_flushen ? "F" : " ",
	    rte.rte_mask ? "(masked)" : "");
}

#endif
