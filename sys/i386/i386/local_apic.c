/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * Local APIC support on Pentium and later processors.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/apicreg.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/md_var.h>
#include <machine/smp.h>
#include <machine/specialreg.h>

/*
 * We can handle up to 60 APICs via our logical cluster IDs, but currently
 * the physical IDs on Intel processors up to the Pentium 4 are limited to
 * 16.
 */
#define	MAX_APICID	16

/* Sanity checks on IDT vectors. */
CTASSERT(APIC_IO_INTS + APIC_NUM_IOINTS <= APIC_LOCAL_INTS);
CTASSERT(IPI_STOP < APIC_SPURIOUS_INT);

/*
 * Support for local APICs.  Local APICs manage interrupts on each
 * individual processor as opposed to I/O APICs which receive interrupts
 * from I/O devices and then forward them on to the local APICs.
 *
 * Local APICs can also send interrupts to each other thus providing the
 * mechanism for IPIs.
 */

struct lvt {
	u_int lvt_edgetrigger:1;
	u_int lvt_activehi:1;
	u_int lvt_masked:1;
	u_int lvt_active:1;
	u_int lvt_mode:16;
	u_int lvt_vector:8;
};

struct lapic {
	struct lvt la_lvts[LVT_MAX + 1];
	u_int la_id:8;
	u_int la_cluster:4;
	u_int la_cluster_id:2;
	u_int la_present:1;
} static lapics[MAX_APICID];

/* XXX: should thermal be an NMI? */

/* Global defaults for local APIC LVT entries. */
static struct lvt lvts[LVT_MAX + 1] = {
	{ 1, 1, 1, 1, APIC_LVT_DM_EXTINT, 0 },	/* LINT0: masked ExtINT */
	{ 1, 1, 0, 1, APIC_LVT_DM_NMI, 0 },	/* LINT1: NMI */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, 0 },	/* Timer: needs a vector */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, 0 },	/* Error: needs a vector */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, 0 },	/* PMC */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, 0 },	/* Thermal: needs a vector */
};

static inthand_t *ioint_handlers[] = {
	NULL,			/* 0 - 31 */
	IDTVEC(apic_isr1),	/* 32 - 63 */
	IDTVEC(apic_isr2),	/* 64 - 95 */
	IDTVEC(apic_isr3),	/* 96 - 127 */
	IDTVEC(apic_isr4),	/* 128 - 159 */
	IDTVEC(apic_isr5),	/* 160 - 191 */
	IDTVEC(apic_isr6),	/* 192 - 223 */
	IDTVEC(apic_isr7),	/* 224 - 255 */
};

volatile lapic_t *lapic;

static uint32_t
lvt_mode(struct lapic *la, u_int pin, uint32_t value)
{
	struct lvt *lvt;

	KASSERT(pin <= LVT_MAX, ("%s: pin %u out of range", __func__, pin));
	if (la->la_lvts[pin].lvt_active)
		lvt = &la->la_lvts[pin];
	else
		lvt = &lvts[pin];

	value &= ~(APIC_LVT_M | APIC_LVT_TM | APIC_LVT_IIPP | APIC_LVT_DM |
	    APIC_LVT_VECTOR);
	if (lvt->lvt_edgetrigger == 0)
		value |= APIC_LVT_TM;
	if (lvt->lvt_activehi == 0)
		value |= APIC_LVT_IIPP_INTALO;
	if (lvt->lvt_masked)
		value |= APIC_LVT_M;
	value |= lvt->lvt_mode;
	switch (lvt->lvt_mode) {
	case APIC_LVT_DM_NMI:
	case APIC_LVT_DM_SMI:
	case APIC_LVT_DM_INIT:
	case APIC_LVT_DM_EXTINT:
		if (!lvt->lvt_edgetrigger) {
			printf("lapic%u: Forcing LINT%u to edge trigger\n",
			    la->la_id, pin);
			value |= APIC_LVT_TM;
		}
		/* Use a vector of 0. */
		break;
	case APIC_LVT_DM_FIXED:
#if 0
		value |= lvt->lvt_vector;
#else
		panic("Fixed LINT pins not supported");
#endif
		break;
	default:
		panic("bad APIC LVT delivery mode: %#x\n", value);
	}
	return (value);
}

/*
 * Map the local APIC and setup necessary interrupt vectors.
 */
void
lapic_init(uintptr_t addr)
{
	u_int32_t value;

	/* Map the local APIC and setup the spurious interrupt handler. */
	KASSERT(trunc_page(addr) == addr,
	    ("local APIC not aligned on a page boundary"));
	lapic = (lapic_t *)pmap_mapdev(addr, sizeof(lapic_t));
	setidt(APIC_SPURIOUS_INT, IDTVEC(spuriousint), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	/* Perform basic initialization of the BSP's local APIC. */
	value = lapic->svr;
	value &= ~(APIC_SVR_VECTOR | APIC_SVR_FOCUS);
	value |= (APIC_SVR_FEN | APIC_SVR_SWEN | APIC_SPURIOUS_INT);
	lapic->svr = value;

	/* Set BSP's per-CPU local APIC ID. */
	PCPU_SET(apic_id, lapic_id());

	/* XXX: timer/error/thermal interrupts */
}

/*
 * Create a local APIC instance.
 */
void
lapic_create(u_int apic_id, int boot_cpu)
{
	int i;

	if (apic_id > MAX_APICID) {
		printf("APIC: Ignoring local APIC with ID %d\n", apic_id);
		if (boot_cpu)
			panic("Can't ignore BSP");
		return;
	}
	KASSERT(!lapics[apic_id].la_present, ("duplicate local APIC %u",
	    apic_id));

	/*
	 * Assume no local LVT overrides and a cluster of 0 and
	 * intra-cluster ID of 0.
	 */
	lapics[apic_id].la_present = 1;
	lapics[apic_id].la_id = apic_id;
	for (i = 0; i < LVT_MAX; i++) {
		lapics[apic_id].la_lvts[i] = lvts[i];
		lapics[apic_id].la_lvts[i].lvt_active = 0;
	}

#ifdef SMP
	cpu_add(apic_id, boot_cpu);
#endif
}

/*
 * Dump contents of local APIC registers
 */
void
lapic_dump(const char* str)
{

	printf("cpu%d %s:\n", PCPU_GET(cpuid), str);
	printf("     ID: 0x%08x   VER: 0x%08x LDR: 0x%08x DFR: 0x%08x\n",
	    lapic->id, lapic->version, lapic->ldr, lapic->dfr);
	printf("  lint0: 0x%08x lint1: 0x%08x TPR: 0x%08x SVR: 0x%08x\n",
	    lapic->lvt_lint0, lapic->lvt_lint1, lapic->tpr, lapic->svr);
}

void
lapic_enable_intr(u_int irq)
{
	u_int vector;

	vector = apic_irq_to_idt(irq);
	KASSERT(vector != IDT_SYSCALL, ("Attempt to overwrite syscall entry"));
	KASSERT(ioint_handlers[vector / 32] != NULL,
	    ("No ISR handler for IRQ %u", irq));
	setidt(vector, ioint_handlers[vector / 32], SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
}

void
lapic_setup(void)
{
	struct lapic *la;
	u_int32_t value, maxlvt;
	register_t eflags;

	la = &lapics[lapic_id()];
	KASSERT(la->la_present, ("missing APIC structure"));
	eflags = intr_disable();
	maxlvt = (lapic->version & APIC_VER_MAXLVT) >> MAXLVTSHIFT;

	/* Program LINT[01] LVT entries. */
	lapic->lvt_lint0 = lvt_mode(la, LVT_LINT0, lapic->lvt_lint0);
	lapic->lvt_lint1 = lvt_mode(la, LVT_LINT1, lapic->lvt_lint1);

	/* XXX: more LVT entries */

	/* Clear the TPR. */
	value = lapic->tpr;
	value &= ~APIC_TPR_PRIO;
	lapic->tpr = value;

	/* Use the cluster model for logical IDs. */
	value = lapic->dfr;
	value &= ~APIC_DFR_MODEL_MASK;
	value |= APIC_DFR_MODEL_CLUSTER;
	lapic->dfr = value;

	/* Set this APIC's logical ID. */
	value = lapic->ldr;
	value &= ~APIC_ID_MASK;
	value |= (la->la_cluster << APIC_ID_CLUSTER_SHIFT |
	    1 << la->la_cluster_id) << APIC_ID_SHIFT;
	lapic->ldr = value;

	/* Setup spurious vector and enable the local APIC. */
	value = lapic->svr;
	value &= ~(APIC_SVR_VECTOR | APIC_SVR_FOCUS);
	value |= (APIC_SVR_FEN | APIC_SVR_SWEN | APIC_SPURIOUS_INT);
	lapic->svr = value;
	intr_restore(eflags);
}

void
lapic_disable(void)
{
	uint32_t value;

	/* Software disable the local APIC. */
	value = lapic->svr;
	value &= ~APIC_SVR_SWEN;
	lapic->svr = value;
}

int
lapic_id(void)
{

	KASSERT(lapic != NULL, ("local APIC is not mapped"));
	return (lapic->id >> APIC_ID_SHIFT);
}

int
lapic_intr_pending(u_int vector)
{
	volatile u_int32_t *irr;

	/*
	 * The IRR registers are an array of 128-bit registers each of
	 * which only describes 32 interrupts in the low 32 bits..  Thus,
	 * we divide the vector by 32 to get the 128-bit index.  We then
	 * multiply that index by 4 to get the equivalent index from
	 * treating the IRR as an array of 32-bit registers.  Finally, we
	 * modulus the vector by 32 to determine the individual bit to
	 * test.
	 */
	irr = &lapic->irr0;
	return (irr[(vector / 32) * 4] & 1 << (vector % 32));
}

void
lapic_set_logical_id(u_int apic_id, u_int cluster, u_int cluster_id)
{
	struct lapic *la;

	KASSERT(lapics[apic_id].la_present, ("%s: APIC %u doesn't exist",
	    __func__, apic_id));
	KASSERT(cluster <= APIC_MAX_CLUSTER, ("%s: cluster %u too big",
	    __func__, cluster));
	KASSERT(cluster_id <= APIC_MAX_INTRACLUSTER_ID,
	    ("%s: intra cluster id %u too big", __func__, cluster_id));
	la = &lapics[apic_id];
	la->la_cluster = cluster;
	la->la_cluster_id = cluster_id;
}

int
lapic_set_lvt_mask(u_int apic_id, u_int pin, u_char masked)
{

	if (pin > LVT_MAX)
		return (EINVAL);
	if (apic_id == APIC_ID_ALL) {
		lvts[pin].lvt_masked = masked;
		if (bootverbose)
			printf("lapic:");
	} else {
		KASSERT(lapics[apic_id].la_present,
		    ("%s: missing APIC %u", __func__, apic_id));
		lapics[apic_id].la_lvts[pin].lvt_masked = masked;
		lapics[apic_id].la_lvts[pin].lvt_active = 1;
		if (bootverbose)
			printf("lapic%u:", apic_id);
	}
	if (bootverbose)
		printf(" LINT%u %s\n", pin, masked ? "masked" : "unmasked");
	return (0);
}

int
lapic_set_lvt_mode(u_int apic_id, u_int pin, u_int32_t mode)
{
	struct lvt *lvt;

	if (pin > LVT_MAX)
		return (EINVAL);
	if (apic_id == APIC_ID_ALL) {
		lvt = &lvts[pin];
		if (bootverbose)
			printf("lapic:");
	} else {
		KASSERT(lapics[apic_id].la_present,
		    ("%s: missing APIC %u", __func__, apic_id));
		lvt = &lapics[apic_id].la_lvts[pin];
		lvt->lvt_active = 1;
		if (bootverbose)
			printf("lapic%u:", apic_id);
	}
	lvt->lvt_mode = mode;
	switch (mode) {
	case APIC_LVT_DM_NMI:
	case APIC_LVT_DM_SMI:
	case APIC_LVT_DM_INIT:
	case APIC_LVT_DM_EXTINT:
		lvt->lvt_edgetrigger = 1;
		lvt->lvt_activehi = 1;
		if (mode == APIC_LVT_DM_EXTINT)
			lvt->lvt_masked = 1;
		else
			lvt->lvt_masked = 0;
		break;
	default:
		panic("Unsupported delivery mode: 0x%x\n", mode);
	}
	if (bootverbose) {
		printf(" Routing ");
		switch (mode) {
		case APIC_LVT_DM_NMI:
			printf("NMI");
			break;
		case APIC_LVT_DM_SMI:
			printf("SMI");
			break;
		case APIC_LVT_DM_INIT:
			printf("INIT");
			break;
		case APIC_LVT_DM_EXTINT:
			printf("ExtINT");
			break;
		}
		printf(" -> LINT%u\n", pin);
	}
	return (0);
}

int
lapic_set_lvt_polarity(u_int apic_id, u_int pin, u_char activehi)
{

	if (pin > LVT_MAX)
		return (EINVAL);
	if (apic_id == APIC_ID_ALL) {
		lvts[pin].lvt_activehi = activehi;
		if (bootverbose)
			printf("lapic:");
	} else {
		KASSERT(lapics[apic_id].la_present,
		    ("%s: missing APIC %u", __func__, apic_id));
		lapics[apic_id].la_lvts[pin].lvt_active = 1;
		lapics[apic_id].la_lvts[pin].lvt_activehi = activehi;
		if (bootverbose)
			printf("lapic%u:", apic_id);
	}
	if (bootverbose)
		printf(" LINT%u polarity: active-%s\n", pin,
		    activehi ? "hi" : "lo");
	return (0);
}

int
lapic_set_lvt_triggermode(u_int apic_id, u_int pin, u_char edgetrigger)
{

	if (pin > LVT_MAX)
		return (EINVAL);
	if (apic_id == APIC_ID_ALL) {
		lvts[pin].lvt_edgetrigger = edgetrigger;
		if (bootverbose)
			printf("lapic:");
	} else {
		KASSERT(lapics[apic_id].la_present,
		    ("%s: missing APIC %u", __func__, apic_id));
		lapics[apic_id].la_lvts[pin].lvt_edgetrigger = edgetrigger;
		lapics[apic_id].la_lvts[pin].lvt_active = 1;
		if (bootverbose)
			printf("lapic%u:", apic_id);
	}
	if (bootverbose)
		printf(" LINT%u trigger: %s\n", pin,
		    edgetrigger ? "edge" : "level");
	return (0);
}

void
lapic_eoi(void)
{

	lapic->eoi = 0;
}

void
lapic_handle_intr(struct intrframe frame)
{
	struct intsrc *isrc;

	if (frame.if_vec == -1)
		panic("Couldn't get vector from ISR!");
	isrc = intr_lookup_source(apic_idt_to_irq(frame.if_vec));
	intr_execute_handlers(isrc, &frame);
}

/* Translate between IDT vectors and IRQ vectors. */
u_int
apic_irq_to_idt(u_int irq)
{
	u_int vector;

	KASSERT(irq < NUM_IO_INTS, ("Invalid IRQ %u", irq));
	vector = irq + APIC_IO_INTS;
	if (vector >= IDT_SYSCALL)
		vector++;
	return (vector);
}

u_int
apic_idt_to_irq(u_int vector)
{

	KASSERT(vector >= APIC_IO_INTS && vector != IDT_SYSCALL &&
	    vector <= APIC_IO_INTS + NUM_IO_INTS,
	    ("Vector %u does not map to an IRQ line", vector));
	if (vector > IDT_SYSCALL)
		vector--;
	return (vector - APIC_IO_INTS);
}

/*
 * APIC probing support code.  This includes code to manage enumerators.
 */

static SLIST_HEAD(, apic_enumerator) enumerators =
	SLIST_HEAD_INITIALIZER(enumerators);
static struct apic_enumerator *best_enum;
	
void
apic_register_enumerator(struct apic_enumerator *enumerator)
{
#ifdef INVARIANTS
	struct apic_enumerator *apic_enum;

	SLIST_FOREACH(apic_enum, &enumerators, apic_next) {
		if (apic_enum == enumerator)
			panic("%s: Duplicate register of %s", __func__,
			    enumerator->apic_name);
	}
#endif
	SLIST_INSERT_HEAD(&enumerators, enumerator, apic_next);
}

/*
 * Probe the APIC enumerators, enumerate CPUs, and initialize the
 * local APIC.
 */
static void
apic_init(void *dummy __unused)
{
	struct apic_enumerator *enumerator;
	uint64_t apic_base;
	int retval, best;

	/* We only support built in local APICs. */
	if (!(cpu_feature & CPUID_APIC))
		return;

	/* Don't probe if APIC mode is disabled. */
	if (resource_disabled("apic", 0))
		return;

	/* First, probe all the enumerators to find the best match. */
	best_enum = NULL;
	best = 0;
	SLIST_FOREACH(enumerator, &enumerators, apic_next) {
		retval = enumerator->apic_probe();
		if (retval > 0)
			continue;
		if (best_enum == NULL || best < retval) {
			best_enum = enumerator;
			best = retval;
		}
	}
	if (best_enum == NULL) {
		if (bootverbose)
			printf("APIC: Could not find any APICs.\n");
		return;
	}

	if (bootverbose)
		printf("APIC: Using the %s enumerator.\n",
		    best_enum->apic_name);

	/*
	 * To work around an errata, we disable the local APIC on some
	 * CPUs during early startup.  We need to turn the local APIC back
	 * on on such CPUs now.
	 */
	if (cpu == CPU_686 && strcmp(cpu_vendor, "GenuineIntel") == 0 &&
	    (cpu_id & 0xff0) == 0x610) {
		apic_base = rdmsr(MSR_APICBASE);
		apic_base |= APICBASE_ENABLED;
		wrmsr(MSR_APICBASE, apic_base);
	}

	/* Second, probe the CPU's in the system. */
	retval = best_enum->apic_probe_cpus();
	if (retval != 0)
		printf("%s: Failed to probe CPUs: returned %d\n",
		    best_enum->apic_name, retval);

	/* Third, initialize the local APIC. */
	retval = best_enum->apic_setup_local();
	if (retval != 0)
		printf("%s: Failed to setup the local APIC: returned %d\n",
		    best_enum->apic_name, retval);
}
SYSINIT(apic_init, SI_SUB_CPU, SI_ORDER_FIRST, apic_init, NULL)

/*
 * Setup the I/O APICs.
 */
static void
apic_setup_io(void *dummy __unused)
{
	int retval;

	if (best_enum == NULL)
		return;
	retval = best_enum->apic_setup_io();
	if (retval != 0)
		printf("%s: Failed to setup I/O APICs: returned %d\n",
		    best_enum->apic_name, retval);

	/*
	 * Finish setting up the local APIC on the BSP once we know how to
	 * properly program the LINT pins.
	 */
	lapic_setup();
	if (bootverbose)
		lapic_dump("BSP");
}
SYSINIT(apic_setup_io, SI_SUB_INTR, SI_ORDER_SECOND, apic_setup_io, NULL)

#ifdef SMP
/*
 * Inter Processor Interrupt functions.  The lapic_ipi_*() functions are
 * private the sys/i386 code.  The public interface for the rest of the
 * kernel is defined in mp_machdep.c.
 */
#define DETECT_DEADLOCK

int
lapic_ipi_wait(int delay)
{
	int x, incr;

	/*
	 * Wait delay loops for IPI to be sent.  This is highly bogus
	 * since this is sensitive to CPU clock speed.  If delay is
	 * -1, we wait forever.
	 */
	if (delay == -1) {
		incr = 0;
		delay = 1;
	} else
		incr = 1;
	for (x = 0; x < delay; x += incr) {
		if ((lapic->icr_lo & APIC_DELSTAT_MASK) == APIC_DELSTAT_IDLE)
			return (1);
		ia32_pause();
	}
	return (0);
}

void
lapic_ipi_raw(register_t icrlo, u_int dest)
{
	register_t value, eflags;

	/* XXX: Need more sanity checking of icrlo? */
	KASSERT(lapic != NULL, ("%s called too early", __func__));
	KASSERT((dest & ~(APIC_ID_MASK >> APIC_ID_SHIFT)) == 0,
	    ("%s: invalid dest field", __func__));
	KASSERT((icrlo & APIC_ICRLO_RESV_MASK) == 0,
	    ("%s: reserved bits set in ICR LO register", __func__));

	/* Set destination in ICR HI register if it is being used. */
	eflags = intr_disable();
	if ((icrlo & APIC_DEST_MASK) == APIC_DEST_DESTFLD) {
		value = lapic->icr_hi;
		value &= ~APIC_ID_MASK;
		value |= dest << APIC_ID_SHIFT;
		lapic->icr_hi = value;
	}

	/* Program the contents of the IPI and dispatch it. */
	value = lapic->icr_lo;
	value &= APIC_ICRLO_RESV_MASK;
	value |= icrlo;
	lapic->icr_lo = value;
	intr_restore(eflags);
}

#ifdef DETECT_DEADLOCK
#define	BEFORE_SPIN	1000000
#define	AFTER_SPIN	1000
#endif

void
lapic_ipi_vectored(u_int vector, int dest)
{
	register_t icrlo, destfield;

	KASSERT((vector & ~APIC_VECTOR_MASK) == 0,
	    ("%s: invalid vector %d", __func__, vector));

	icrlo = vector | APIC_DELMODE_FIXED | APIC_DESTMODE_PHY |
	    APIC_LEVEL_DEASSERT | APIC_TRIGMOD_EDGE;
	destfield = 0;
	switch (dest) {
	case APIC_IPI_DEST_SELF:
		icrlo |= APIC_DEST_SELF;
		break;
	case APIC_IPI_DEST_ALL:
		icrlo |= APIC_DEST_ALLISELF;
		break;
	case APIC_IPI_DEST_OTHERS:
		icrlo |= APIC_DEST_ALLESELF;
		break;
	default:
		KASSERT((dest & ~(APIC_ID_MASK >> APIC_ID_SHIFT)) == 0,
		    ("%s: invalid destination 0x%x", __func__, dest));
		destfield = dest;
	}

#ifdef DETECT_DEADLOCK
	/* Check for an earlier stuck IPI. */
	if (!lapic_ipi_wait(BEFORE_SPIN))
		panic("APIC: Previous IPI is stuck");
#endif

	lapic_ipi_raw(icrlo, destfield);

#ifdef DETECT_DEADLOCK
	/* Wait for IPI to be delivered. */
	if (!lapic_ipi_wait(AFTER_SPIN)) {
#ifdef needsattention
		/*
		 * XXX FIXME:
		 *
		 * The above function waits for the message to actually be
		 * delivered.  It breaks out after an arbitrary timeout
		 * since the message should eventually be delivered (at
		 * least in theory) and that if it wasn't we would catch
		 * the failure with the check above when the next IPI is
		 * sent.
		 *
		 * We could skiip this wait entirely, EXCEPT it probably
		 * protects us from other routines that assume that the
		 * message was delivered and acted upon when this function
		 * returns.
		 */
		printf("APIC: IPI might be stuck\n");
#else /* !needsattention */
		/* Wait until mesage is sent without a timeout. */
		while (lapic->icr_lo & APIC_DELSTAT_PEND)
			ia32_pause();
#endif /* needsattention */
	}
#endif /* DETECT_DEADLOCK */
}
#endif /* SMP */
