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

#include "opt_hwpmc_hooks.h"
#include "opt_kdtrace.h"

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/apicreg.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/md_var.h>
#include <machine/smp.h>
#include <machine/specialreg.h>

#ifdef DDB
#include <sys/interrupt.h>
#include <ddb/ddb.h>
#endif

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
cyclic_clock_func_t	lapic_cyclic_clock_func[MAXCPU];
#endif

/* Sanity checks on IDT vectors. */
CTASSERT(APIC_IO_INTS + APIC_NUM_IOINTS == APIC_TIMER_INT);
CTASSERT(APIC_TIMER_INT < APIC_LOCAL_INTS);
CTASSERT(APIC_LOCAL_INTS == 240);
CTASSERT(IPI_STOP < APIC_SPURIOUS_INT);

#define	LAPIC_TIMER_HZ_DIVIDER		2
#define	LAPIC_TIMER_STATHZ_DIVIDER	15
#define	LAPIC_TIMER_PROFHZ_DIVIDER	3

/* Magic IRQ values for the timer and syscalls. */
#define	IRQ_TIMER	(NUM_IO_INTS + 1)
#define	IRQ_SYSCALL	(NUM_IO_INTS + 2)

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
	u_long *la_timer_count;
	u_long la_hard_ticks;
	u_long la_stat_ticks;
	u_long la_prof_ticks;
} static lapics[MAX_APIC_ID + 1];

/* XXX: should thermal be an NMI? */

/* Global defaults for local APIC LVT entries. */
static struct lvt lvts[LVT_MAX + 1] = {
	{ 1, 1, 1, 1, APIC_LVT_DM_EXTINT, 0 },	/* LINT0: masked ExtINT */
	{ 1, 1, 0, 1, APIC_LVT_DM_NMI, 0 },	/* LINT1: NMI */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, APIC_TIMER_INT },	/* Timer */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, APIC_ERROR_INT },	/* Error */
	{ 1, 1, 0, 1, APIC_LVT_DM_NMI, 0 },	/* PMC */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, APIC_THERMAL_INT },	/* Thermal */
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

/* Include IDT_SYSCALL to make indexing easier. */
static u_int ioint_irqs[APIC_NUM_IOINTS + 1];

static u_int32_t lapic_timer_divisors[] = { 
	APIC_TDCR_1, APIC_TDCR_2, APIC_TDCR_4, APIC_TDCR_8, APIC_TDCR_16,
	APIC_TDCR_32, APIC_TDCR_64, APIC_TDCR_128
};

extern inthand_t IDTVEC(rsvd);

volatile lapic_t *lapic;
vm_paddr_t lapic_paddr;
static u_long lapic_timer_divisor, lapic_timer_period, lapic_timer_hz;

static void	lapic_enable(void);
static void	lapic_resume(struct pic *pic);
static void	lapic_timer_enable_intr(void);
static void	lapic_timer_oneshot(u_int count);
static void	lapic_timer_periodic(u_int count);
static void	lapic_timer_set_divisor(u_int divisor);
static uint32_t	lvt_mode(struct lapic *la, u_int pin, uint32_t value);

struct pic lapic_pic = { .pic_resume = lapic_resume };

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
		value |= lvt->lvt_vector;
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
lapic_init(vm_paddr_t addr)
{

	/* Map the local APIC and setup the spurious interrupt handler. */
	KASSERT(trunc_page(addr) == addr,
	    ("local APIC not aligned on a page boundary"));
	lapic = pmap_mapdev(addr, sizeof(lapic_t));
	lapic_paddr = addr;
	setidt(APIC_SPURIOUS_INT, IDTVEC(spuriousint), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));

	/* Perform basic initialization of the BSP's local APIC. */
	lapic_enable();
	ioint_irqs[IDT_SYSCALL - APIC_IO_INTS] = IRQ_SYSCALL;

	/* Set BSP's per-CPU local APIC ID. */
	PCPU_SET(apic_id, lapic_id());

	/* Local APIC timer interrupt. */
	setidt(APIC_TIMER_INT, IDTVEC(timerint), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	ioint_irqs[APIC_TIMER_INT - APIC_IO_INTS] = IRQ_TIMER;

	/* XXX: error/thermal interrupts */
}

/*
 * Create a local APIC instance.
 */
void
lapic_create(u_int apic_id, int boot_cpu)
{
	int i;

	if (apic_id > MAX_APIC_ID) {
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
	printf("  timer: 0x%08x therm: 0x%08x err: 0x%08x pcm: 0x%08x\n",
	    lapic->lvt_timer, lapic->lvt_thermal, lapic->lvt_error,
	    lapic->lvt_pcint);
}

void
lapic_setup(int boot)
{
	struct lapic *la;
	u_int32_t maxlvt;
	register_t eflags;
	char buf[MAXCOMLEN + 1];

	la = &lapics[lapic_id()];
	KASSERT(la->la_present, ("missing APIC structure"));
	eflags = intr_disable();
	maxlvt = (lapic->version & APIC_VER_MAXLVT) >> MAXLVTSHIFT;

	/* Initialize the TPR to allow all interrupts. */
	lapic_set_tpr(0);

	/* Setup spurious vector and enable the local APIC. */
	lapic_enable();

	/* Program LINT[01] LVT entries. */
	lapic->lvt_lint0 = lvt_mode(la, LVT_LINT0, lapic->lvt_lint0);
	lapic->lvt_lint1 = lvt_mode(la, LVT_LINT1, lapic->lvt_lint1);
#ifdef	HWPMC_HOOKS
	/* Program the PMC LVT entry if present. */
	if (maxlvt >= LVT_PMC)
		lapic->lvt_pcint = lvt_mode(la, LVT_PMC, lapic->lvt_pcint);
#endif

	/* Program timer LVT and setup handler. */
	lapic->lvt_timer = lvt_mode(la, LVT_TIMER, lapic->lvt_timer);
	if (boot) {
		snprintf(buf, sizeof(buf), "cpu%d: timer", PCPU_GET(cpuid));
		intrcnt_add(buf, &la->la_timer_count);
	}

	/* We don't setup the timer during boot on the BSP until later. */
	if (!(boot && PCPU_GET(cpuid) == 0)) {
		KASSERT(lapic_timer_period != 0, ("lapic%u: zero divisor",
		    lapic_id()));
		lapic_timer_set_divisor(lapic_timer_divisor);
		lapic_timer_periodic(lapic_timer_period);
		lapic_timer_enable_intr();
	}

	/* XXX: Error and thermal LVTs */

	if (strcmp(cpu_vendor, "AuthenticAMD") == 0) {
		/*
		 * Detect the presence of C1E capability mostly on latest
		 * dual-cores (or future) k8 family.  This feature renders
		 * the local APIC timer dead, so we disable it by reading
		 * the Interrupt Pending Message register and clearing both
		 * C1eOnCmpHalt (bit 28) and SmiOnCmpHalt (bit 27).
		 * 
		 * Reference:
		 *   "BIOS and Kernel Developer's Guide for AMD NPT
		 *    Family 0Fh Processors"
		 *   #32559 revision 3.00
		 */
		if ((cpu_id & 0x00000f00) == 0x00000f00 &&
		    (cpu_id & 0x0fff0000) >=  0x00040000) {
			uint64_t msr;

			msr = rdmsr(0xc0010055);
			if (msr & 0x18000000)
				wrmsr(0xc0010055, msr & ~0x18000000ULL);
		}
	}

	intr_restore(eflags);
}

/*
 * Called by cpu_initclocks() on the BSP to setup the local APIC timer so
 * that it can drive hardclock, statclock, and profclock.  This function
 * returns true if it is able to use the local APIC timer to drive the
 * clocks and false if it is not able.
 */
int
lapic_setup_clock(void)
{
	u_long value;

	/* Can't drive the timer without a local APIC. */
	if (lapic == NULL)
		return (0);

	/* Start off with a divisor of 2 (power on reset default). */
	lapic_timer_divisor = 2;

	/* Try to calibrate the local APIC timer. */
	do {
		lapic_timer_set_divisor(lapic_timer_divisor);
		lapic_timer_oneshot(APIC_TIMER_MAX_COUNT);
		DELAY(2000000);
		value = APIC_TIMER_MAX_COUNT - lapic->ccr_timer;
		if (value != APIC_TIMER_MAX_COUNT)
			break;
		lapic_timer_divisor <<= 1;
	} while (lapic_timer_divisor <= 128);
	if (lapic_timer_divisor > 128)
		panic("lapic: Divisor too big");
	value /= 2;
	if (bootverbose)
		printf("lapic: Divisor %lu, Frequency %lu hz\n",
		    lapic_timer_divisor, value);

	/*
	 * We will drive the timer at a small multiple of hz and drive
	 * both of the other timers with similarly small but relatively
	 * prime divisors.
	 */
	lapic_timer_hz = hz * LAPIC_TIMER_HZ_DIVIDER;
	stathz = lapic_timer_hz / LAPIC_TIMER_STATHZ_DIVIDER;
	profhz = lapic_timer_hz / LAPIC_TIMER_PROFHZ_DIVIDER;
	lapic_timer_period = value / lapic_timer_hz;

	/*
	 * Start up the timer on the BSP.  The APs will kick off their
	 * timer during lapic_setup().
	 */
	lapic_timer_periodic(lapic_timer_period);
	lapic_timer_enable_intr();
	return (1);
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

static void
lapic_enable(void)
{
	u_int32_t value;

	/* Program the spurious vector to enable the local APIC. */
	value = lapic->svr;
	value &= ~(APIC_SVR_VECTOR | APIC_SVR_FOCUS);
	value |= (APIC_SVR_FEN | APIC_SVR_SWEN | APIC_SPURIOUS_INT);
	lapic->svr = value;
}

/* Reset the local APIC on the BSP during resume. */
static void
lapic_resume(struct pic *pic)
{

	lapic_setup(0);
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
lapic_set_lvt_polarity(u_int apic_id, u_int pin, enum intr_polarity pol)
{

	if (pin > LVT_MAX || pol == INTR_POLARITY_CONFORM)
		return (EINVAL);
	if (apic_id == APIC_ID_ALL) {
		lvts[pin].lvt_activehi = (pol == INTR_POLARITY_HIGH);
		if (bootverbose)
			printf("lapic:");
	} else {
		KASSERT(lapics[apic_id].la_present,
		    ("%s: missing APIC %u", __func__, apic_id));
		lapics[apic_id].la_lvts[pin].lvt_active = 1;
		lapics[apic_id].la_lvts[pin].lvt_activehi =
		    (pol == INTR_POLARITY_HIGH);
		if (bootverbose)
			printf("lapic%u:", apic_id);
	}
	if (bootverbose)
		printf(" LINT%u polarity: %s\n", pin,
		    pol == INTR_POLARITY_HIGH ? "high" : "low");
	return (0);
}

int
lapic_set_lvt_triggermode(u_int apic_id, u_int pin, enum intr_trigger trigger)
{

	if (pin > LVT_MAX || trigger == INTR_TRIGGER_CONFORM)
		return (EINVAL);
	if (apic_id == APIC_ID_ALL) {
		lvts[pin].lvt_edgetrigger = (trigger == INTR_TRIGGER_EDGE);
		if (bootverbose)
			printf("lapic:");
	} else {
		KASSERT(lapics[apic_id].la_present,
		    ("%s: missing APIC %u", __func__, apic_id));
		lapics[apic_id].la_lvts[pin].lvt_edgetrigger =
		    (trigger == INTR_TRIGGER_EDGE);
		lapics[apic_id].la_lvts[pin].lvt_active = 1;
		if (bootverbose)
			printf("lapic%u:", apic_id);
	}
	if (bootverbose)
		printf(" LINT%u trigger: %s\n", pin,
		    trigger == INTR_TRIGGER_EDGE ? "edge" : "level");
	return (0);
}

/*
 * Adjust the TPR of the current CPU so that it blocks all interrupts below
 * the passed in vector.
 */
void
lapic_set_tpr(u_int vector)
{
#ifdef CHEAP_TPR
	lapic->tpr = vector;
#else
	u_int32_t tpr;

	tpr = lapic->tpr & ~APIC_TPR_PRIO;
	tpr |= vector;
	lapic->tpr = tpr;
#endif
}

void
lapic_eoi(void)
{

	lapic->eoi = 0;
}

void
lapic_handle_intr(int vector, struct trapframe *frame)
{
	struct intsrc *isrc;

	if (vector == -1)
		panic("Couldn't get vector from ISR!");
	isrc = intr_lookup_source(apic_idt_to_irq(vector));
	intr_execute_handlers(isrc, frame);
}

void
lapic_handle_timer(struct trapframe *frame)
{
	struct lapic *la;

	/* Send EOI first thing. */
	lapic_eoi();

#if defined(SMP) && !defined(SCHED_ULE)
	/*
	 * Don't do any accounting for the disabled HTT cores, since it
	 * will provide misleading numbers for the userland.
	 *
	 * No locking is necessary here, since even if we loose the race
	 * when hlt_cpus_mask changes it is not a big deal, really.
	 *
	 * Don't do that for ULE, since ULE doesn't consider hlt_cpus_mask
	 * and unlike other schedulers it actually schedules threads to
	 * those CPUs.
	 */
	if ((hlt_cpus_mask & (1 << PCPU_GET(cpuid))) != 0)
		return;
#endif

	/* Look up our local APIC structure for the tick counters. */
	la = &lapics[PCPU_GET(apic_id)];
	(*la->la_timer_count)++;
	critical_enter();

#ifdef KDTRACE_HOOKS
	/*
	 * If the DTrace hooks are configured and a callback function
	 * has been registered, then call it to process the high speed
	 * timers.
	 */
	int cpu = PCPU_GET(cpuid);
	if (lapic_cyclic_clock_func[cpu] != NULL)
		(*lapic_cyclic_clock_func[cpu])(frame);
#endif

	/* Fire hardclock at hz. */
	la->la_hard_ticks += hz;
	if (la->la_hard_ticks >= lapic_timer_hz) {
		la->la_hard_ticks -= lapic_timer_hz;
		if (PCPU_GET(cpuid) == 0)
			hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
		else
			hardclock_cpu(TRAPF_USERMODE(frame));
	}

	/* Fire statclock at stathz. */
	la->la_stat_ticks += stathz;
	if (la->la_stat_ticks >= lapic_timer_hz) {
		la->la_stat_ticks -= lapic_timer_hz;
		statclock(TRAPF_USERMODE(frame));
	}

	/* Fire profclock at profhz, but only when needed. */
	la->la_prof_ticks += profhz;
	if (la->la_prof_ticks >= lapic_timer_hz) {
		la->la_prof_ticks -= lapic_timer_hz;
		if (profprocs != 0)
			profclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	}
	critical_exit();
}

static void
lapic_timer_set_divisor(u_int divisor)
{

	KASSERT(powerof2(divisor), ("lapic: invalid divisor %u", divisor));
	KASSERT(ffs(divisor) <= sizeof(lapic_timer_divisors) /
	    sizeof(u_int32_t), ("lapic: invalid divisor %u", divisor));
	lapic->dcr_timer = lapic_timer_divisors[ffs(divisor) - 1];
}

static void
lapic_timer_oneshot(u_int count)
{
	u_int32_t value;

	value = lapic->lvt_timer;
	value &= ~APIC_LVTT_TM;
	value |= APIC_LVTT_TM_ONE_SHOT;
	lapic->lvt_timer = value;
	lapic->icr_timer = count;
}

static void
lapic_timer_periodic(u_int count)
{
	u_int32_t value;

	value = lapic->lvt_timer;
	value &= ~APIC_LVTT_TM;
	value |= APIC_LVTT_TM_PERIODIC;
	lapic->lvt_timer = value;
	lapic->icr_timer = count;
}

static void
lapic_timer_enable_intr(void)
{
	u_int32_t value;

	value = lapic->lvt_timer;
	value &= ~APIC_LVT_M;
	lapic->lvt_timer = value;
}

/* Request a free IDT vector to be used by the specified IRQ. */
u_int
apic_alloc_vector(u_int irq)
{
	u_int vector;

	KASSERT(irq < NUM_IO_INTS, ("Invalid IRQ %u", irq));

	/*
	 * Search for a free vector.  Currently we just use a very simple
	 * algorithm to find the first free vector.
	 */
	mtx_lock_spin(&icu_lock);
	for (vector = 0; vector < APIC_NUM_IOINTS; vector++) {
		if (ioint_irqs[vector] != 0)
			continue;
		ioint_irqs[vector] = irq;
		mtx_unlock_spin(&icu_lock);
		return (vector + APIC_IO_INTS);
	}
	mtx_unlock_spin(&icu_lock);
	panic("Couldn't find an APIC vector for IRQ %u", irq);
}

/*
 * Request 'count' free contiguous IDT vectors to be used by 'count'
 * IRQs.  'count' must be a power of two and the vectors will be
 * aligned on a boundary of 'align'.  If the request cannot be
 * satisfied, 0 is returned.
 */
u_int
apic_alloc_vectors(u_int *irqs, u_int count, u_int align)
{
	u_int first, run, vector;

	KASSERT(powerof2(count), ("bad count"));
	KASSERT(powerof2(align), ("bad align"));
	KASSERT(align >= count, ("align < count"));
#ifdef INVARIANTS
	for (run = 0; run < count; run++)
		KASSERT(irqs[run] < NUM_IO_INTS, ("Invalid IRQ %u at index %u",
		    irqs[run], run));
#endif

	/*
	 * Search for 'count' free vectors.  As with apic_alloc_vector(),
	 * this just uses a simple first fit algorithm.
	 */
	run = 0;
	first = 0;
	mtx_lock_spin(&icu_lock);
	for (vector = 0; vector < APIC_NUM_IOINTS; vector++) {

		/* Vector is in use, end run. */
		if (ioint_irqs[vector] != 0) {
			run = 0;
			first = 0;
			continue;
		}

		/* Start a new run if run == 0 and vector is aligned. */
		if (run == 0) {
			if ((vector & (align - 1)) != 0)
				continue;
			first = vector;
		}
		run++;

		/* Keep looping if the run isn't long enough yet. */
		if (run < count)
			continue;

		/* Found a run, assign IRQs and return the first vector. */
		for (vector = 0; vector < count; vector++)
			ioint_irqs[first + vector] = irqs[vector];
		mtx_unlock_spin(&icu_lock);
		return (first + APIC_IO_INTS);
	}
	mtx_unlock_spin(&icu_lock);
	printf("APIC: Couldn't find APIC vectors for %u IRQs\n", count);
	return (0);
}

void
apic_enable_vector(u_int vector)
{

	KASSERT(vector != IDT_SYSCALL, ("Attempt to overwrite syscall entry"));
	KASSERT(ioint_handlers[vector / 32] != NULL,
	    ("No ISR handler for vector %u", vector));
	setidt(vector, ioint_handlers[vector / 32], SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
}

void
apic_disable_vector(u_int vector)
{

	KASSERT(vector != IDT_SYSCALL, ("Attempt to overwrite syscall entry"));
	KASSERT(ioint_handlers[vector / 32] != NULL,
	    ("No ISR handler for vector %u", vector));
	setidt(vector, &IDTVEC(rsvd), SDT_SYS386TGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
}

/* Release an APIC vector when it's no longer in use. */
void
apic_free_vector(u_int vector, u_int irq)
{
	KASSERT(vector >= APIC_IO_INTS && vector != IDT_SYSCALL &&
	    vector <= APIC_IO_INTS + APIC_NUM_IOINTS,
	    ("Vector %u does not map to an IRQ line", vector));
	KASSERT(irq < NUM_IO_INTS, ("Invalid IRQ %u", irq));
	KASSERT(ioint_irqs[vector - APIC_IO_INTS] == irq, ("IRQ mismatch"));
	mtx_lock_spin(&icu_lock);
	ioint_irqs[vector - APIC_IO_INTS] = 0;
	mtx_unlock_spin(&icu_lock);
}

/* Map an IDT vector (APIC) to an IRQ (interrupt source). */
u_int
apic_idt_to_irq(u_int vector)
{

	KASSERT(vector >= APIC_IO_INTS && vector != IDT_SYSCALL &&
	    vector <= APIC_IO_INTS + APIC_NUM_IOINTS,
	    ("Vector %u does not map to an IRQ line", vector));
	return (ioint_irqs[vector - APIC_IO_INTS]);
}

#ifdef DDB
/*
 * Dump data about APIC IDT vector mappings.
 */
DB_SHOW_COMMAND(apic, db_show_apic)
{
	struct intsrc *isrc;
	int i, verbose;
	u_int irq;

	if (strcmp(modif, "vv") == 0)
		verbose = 2;
	else if (strcmp(modif, "v") == 0)
		verbose = 1;
	else
		verbose = 0;
	for (i = 0; i < APIC_NUM_IOINTS + 1 && !db_pager_quit; i++) {
		irq = ioint_irqs[i];
		if (irq != 0 && irq != IRQ_SYSCALL) {
			db_printf("vec 0x%2x -> ", i + APIC_IO_INTS);
			if (irq == IRQ_TIMER)
				db_printf("lapic timer\n");
			else if (irq < NUM_IO_INTS) {
				isrc = intr_lookup_source(irq);
				if (isrc == NULL || verbose == 0)
					db_printf("IRQ %u\n", irq);
				else
					db_dump_intr_event(isrc->is_event,
					    verbose == 2);
			} else
				db_printf("IRQ %u ???\n", irq);
		}
	}
}

static void
dump_mask(const char *prefix, uint32_t v, int base)
{
	int i, first;

	first = 1;
	for (i = 0; i < 32; i++)
		if (v & (1 << i)) {
			if (first) {
				db_printf("%s:", prefix);
				first = 0;
			}
			db_printf(" %02x", base + i);
		}
	if (!first)
		db_printf("\n");
}

/* Show info from the lapic regs for this CPU. */
DB_SHOW_COMMAND(lapic, db_show_lapic)
{
	uint32_t v;

	db_printf("lapic ID = %d\n", lapic_id());
	v = lapic->version;
	db_printf("version  = %d.%d\n", (v & APIC_VER_VERSION) >> 4,
	    v & 0xf);
	db_printf("max LVT  = %d\n", (v & APIC_VER_MAXLVT) >> MAXLVTSHIFT);
	v = lapic->svr;
	db_printf("SVR      = %02x (%s)\n", v & APIC_SVR_VECTOR,
	    v & APIC_SVR_ENABLE ? "enabled" : "disabled");
	db_printf("TPR      = %02x\n", lapic->tpr);

#define dump_field(prefix, index)					\
	dump_mask(__XSTRING(prefix ## index), lapic->prefix ## index,	\
	    index * 32)

	db_printf("In-service Interrupts:\n");
	dump_field(isr, 0);
	dump_field(isr, 1);
	dump_field(isr, 2);
	dump_field(isr, 3);
	dump_field(isr, 4);
	dump_field(isr, 5);
	dump_field(isr, 6);
	dump_field(isr, 7);

	db_printf("TMR Interrupts:\n");
	dump_field(tmr, 0);
	dump_field(tmr, 1);
	dump_field(tmr, 2);
	dump_field(tmr, 3);
	dump_field(tmr, 4);
	dump_field(tmr, 5);
	dump_field(tmr, 6);
	dump_field(tmr, 7);

	db_printf("IRR Interrupts:\n");
	dump_field(irr, 0);
	dump_field(irr, 1);
	dump_field(irr, 2);
	dump_field(irr, 3);
	dump_field(irr, 4);
	dump_field(irr, 5);
	dump_field(irr, 6);
	dump_field(irr, 7);

#undef dump_field
}
#endif

/*
 * APIC probing support code.  This includes code to manage enumerators.
 */

static SLIST_HEAD(, apic_enumerator) enumerators =
	SLIST_HEAD_INITIALIZER(enumerators);
static struct apic_enumerator *best_enum;
	
void
apic_register_enumerator(struct apic_enumerator *enumerator)
{
#ifndef XEN
#ifdef INVARIANTS
	struct apic_enumerator *apic_enum;

	SLIST_FOREACH(apic_enum, &enumerators, apic_next) {
		if (apic_enum == enumerator)
			panic("%s: Duplicate register of %s", __func__,
			    enumerator->apic_name);
	}
#endif
	SLIST_INSERT_HEAD(&enumerators, enumerator, apic_next);
#endif
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
SYSINIT(apic_init, SI_SUB_CPU, SI_ORDER_SECOND, apic_init, NULL);

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
	lapic_setup(1);
	intr_register_pic(&lapic_pic);
	if (bootverbose)
		lapic_dump("BSP");

	/* Enable the MSI "pic". */
	msi_init();
}
SYSINIT(apic_setup_io, SI_SUB_INTR, SI_ORDER_SECOND, apic_setup_io, NULL);

#ifdef SMP
/*
 * Inter Processor Interrupt functions.  The lapic_ipi_*() functions are
 * private to the sys/i386 code.  The public interface for the rest of the
 * kernel is defined in mp_machdep.c.
 */
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

#define	BEFORE_SPIN	1000000
#ifdef DETECT_DEADLOCK
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

	/* Wait for an earlier IPI to finish. */
	if (!lapic_ipi_wait(BEFORE_SPIN)) {
		if (panicstr != NULL)
			return;
		else
			panic("APIC: Previous IPI is stuck");
	}

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
		 * We could skip this wait entirely, EXCEPT it probably
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
