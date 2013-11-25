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

#include "opt_atpic.h"
#include "opt_hwpmc_hooks.h"

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/timeet.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <x86/apicreg.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/smp.h>
#include <machine/specialreg.h>

#ifdef DDB
#include <sys/interrupt.h>
#include <ddb/ddb.h>
#endif

#ifdef __amd64__
#define	SDT_APIC	SDT_SYSIGT
#define	SDT_APICT	SDT_SYSIGT
#define	GSEL_APIC	0
#else
#define	SDT_APIC	SDT_SYS386IGT
#define	SDT_APICT	SDT_SYS386TGT
#define	GSEL_APIC	GSEL(GCODE_SEL, SEL_KPL)
#endif

/* Sanity checks on IDT vectors. */
CTASSERT(APIC_IO_INTS + APIC_NUM_IOINTS == APIC_TIMER_INT);
CTASSERT(APIC_TIMER_INT < APIC_LOCAL_INTS);
CTASSERT(APIC_LOCAL_INTS == 240);
CTASSERT(IPI_STOP < APIC_SPURIOUS_INT);

/* Magic IRQ values for the timer and syscalls. */
#define	IRQ_TIMER	(NUM_IO_INTS + 1)
#define	IRQ_SYSCALL	(NUM_IO_INTS + 2)
#define	IRQ_DTRACE_RET	(NUM_IO_INTS + 3)
#define	IRQ_EVTCHN	(NUM_IO_INTS + 4)

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
	u_long la_timer_period;
	u_int la_timer_mode;
	uint32_t lvt_timer_cache;
	/* Include IDT_SYSCALL to make indexing easier. */
	int la_ioint_irqs[APIC_NUM_IOINTS + 1];
} static lapics[MAX_APIC_ID + 1];

/* Global defaults for local APIC LVT entries. */
static struct lvt lvts[LVT_MAX + 1] = {
	{ 1, 1, 1, 1, APIC_LVT_DM_EXTINT, 0 },	/* LINT0: masked ExtINT */
	{ 1, 1, 0, 1, APIC_LVT_DM_NMI, 0 },	/* LINT1: NMI */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, APIC_TIMER_INT },	/* Timer */
	{ 1, 1, 0, 1, APIC_LVT_DM_FIXED, APIC_ERROR_INT },	/* Error */
	{ 1, 1, 1, 1, APIC_LVT_DM_NMI, 0 },	/* PMC */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, APIC_THERMAL_INT },	/* Thermal */
	{ 1, 1, 1, 1, APIC_LVT_DM_FIXED, APIC_CMC_INT },	/* CMCI */
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


static u_int32_t lapic_timer_divisors[] = {
	APIC_TDCR_1, APIC_TDCR_2, APIC_TDCR_4, APIC_TDCR_8, APIC_TDCR_16,
	APIC_TDCR_32, APIC_TDCR_64, APIC_TDCR_128
};

extern inthand_t IDTVEC(rsvd);

volatile lapic_t *lapic;
vm_paddr_t lapic_paddr;
static u_long lapic_timer_divisor;
static struct eventtimer lapic_et;

static void	lapic_enable(void);
static void	lapic_resume(struct pic *pic, bool suspend_cancelled);
static void	lapic_timer_oneshot(struct lapic *,
		    u_int count, int enable_int);
static void	lapic_timer_periodic(struct lapic *,
		    u_int count, int enable_int);
static void	lapic_timer_stop(struct lapic *);
static void	lapic_timer_set_divisor(u_int divisor);
static uint32_t	lvt_mode(struct lapic *la, u_int pin, uint32_t value);
static int	lapic_et_start(struct eventtimer *et,
    sbintime_t first, sbintime_t period);
static int	lapic_et_stop(struct eventtimer *et);

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
	u_int regs[4];
	int i, arat;

	/* Map the local APIC and setup the spurious interrupt handler. */
	KASSERT(trunc_page(addr) == addr,
	    ("local APIC not aligned on a page boundary"));
	lapic_paddr = addr;
	lapic = pmap_mapdev(addr, sizeof(lapic_t));
	setidt(APIC_SPURIOUS_INT, IDTVEC(spuriousint), SDT_APIC, SEL_KPL,
	    GSEL_APIC);

	/* Perform basic initialization of the BSP's local APIC. */
	lapic_enable();

	/* Set BSP's per-CPU local APIC ID. */
	PCPU_SET(apic_id, lapic_id());

	/* Local APIC timer interrupt. */
	setidt(APIC_TIMER_INT, IDTVEC(timerint), SDT_APIC, SEL_KPL, GSEL_APIC);

	/* Local APIC error interrupt. */
	setidt(APIC_ERROR_INT, IDTVEC(errorint), SDT_APIC, SEL_KPL, GSEL_APIC);

	/* XXX: Thermal interrupt */

	/* Local APIC CMCI. */
	setidt(APIC_CMC_INT, IDTVEC(cmcint), SDT_APICT, SEL_KPL, GSEL_APIC);

	if ((resource_int_value("apic", 0, "clock", &i) != 0 || i != 0)) {
		arat = 0;
		/* Intel CPUID 0x06 EAX[2] set if APIC timer runs in C3. */
		if (cpu_vendor_id == CPU_VENDOR_INTEL && cpu_high >= 6) {
			do_cpuid(0x06, regs);
			if ((regs[0] & CPUTPM1_ARAT) != 0)
				arat = 1;
		}
		bzero(&lapic_et, sizeof(lapic_et));
		lapic_et.et_name = "LAPIC";
		lapic_et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT |
		    ET_FLAGS_PERCPU;
		lapic_et.et_quality = 600;
		if (!arat) {
			lapic_et.et_flags |= ET_FLAGS_C3STOP;
			lapic_et.et_quality -= 200;
		}
		lapic_et.et_frequency = 0;
		/* We don't know frequency yet, so trying to guess. */
		lapic_et.et_min_period = 0x00001000LL;
		lapic_et.et_max_period = SBT_1S;
		lapic_et.et_start = lapic_et_start;
		lapic_et.et_stop = lapic_et_stop;
		lapic_et.et_priv = NULL;
		et_register(&lapic_et);
	}
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
	for (i = 0; i <= LVT_MAX; i++) {
		lapics[apic_id].la_lvts[i] = lvts[i];
		lapics[apic_id].la_lvts[i].lvt_active = 0;
	}
	for (i = 0; i <= APIC_NUM_IOINTS; i++)
	    lapics[apic_id].la_ioint_irqs[i] = -1;
	lapics[apic_id].la_ioint_irqs[IDT_SYSCALL - APIC_IO_INTS] = IRQ_SYSCALL;
	lapics[apic_id].la_ioint_irqs[APIC_TIMER_INT - APIC_IO_INTS] =
	    IRQ_TIMER;
#ifdef KDTRACE_HOOKS
	lapics[apic_id].la_ioint_irqs[IDT_DTRACE_RET - APIC_IO_INTS] =
	    IRQ_DTRACE_RET;
#endif
#ifdef XENHVM
	lapics[apic_id].la_ioint_irqs[IDT_EVTCHN - APIC_IO_INTS] = IRQ_EVTCHN;
#endif


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
	uint32_t maxlvt;

	maxlvt = (lapic->version & APIC_VER_MAXLVT) >> MAXLVTSHIFT;
	printf("cpu%d %s:\n", PCPU_GET(cpuid), str);
	printf("     ID: 0x%08x   VER: 0x%08x LDR: 0x%08x DFR: 0x%08x\n",
	    lapic->id, lapic->version, lapic->ldr, lapic->dfr);
	printf("  lint0: 0x%08x lint1: 0x%08x TPR: 0x%08x SVR: 0x%08x\n",
	    lapic->lvt_lint0, lapic->lvt_lint1, lapic->tpr, lapic->svr);
	printf("  timer: 0x%08x therm: 0x%08x err: 0x%08x",
	    lapic->lvt_timer, lapic->lvt_thermal, lapic->lvt_error);
	if (maxlvt >= LVT_PMC)
		printf(" pmc: 0x%08x", lapic->lvt_pcint);
	printf("\n");
	if (maxlvt >= LVT_CMCI)
		printf("   cmci: 0x%08x\n", lapic->lvt_cmci);
}

void
lapic_setup(int boot)
{
	struct lapic *la;
	u_int32_t maxlvt;
	register_t saveintr;
	char buf[MAXCOMLEN + 1];

	la = &lapics[lapic_id()];
	KASSERT(la->la_present, ("missing APIC structure"));
	saveintr = intr_disable();
	maxlvt = (lapic->version & APIC_VER_MAXLVT) >> MAXLVTSHIFT;

	/* Initialize the TPR to allow all interrupts. */
	lapic_set_tpr(0);

	/* Setup spurious vector and enable the local APIC. */
	lapic_enable();

	/* Program LINT[01] LVT entries. */
	lapic->lvt_lint0 = lvt_mode(la, LVT_LINT0, lapic->lvt_lint0);
	lapic->lvt_lint1 = lvt_mode(la, LVT_LINT1, lapic->lvt_lint1);

	/* Program the PMC LVT entry if present. */
	if (maxlvt >= LVT_PMC)
		lapic->lvt_pcint = lvt_mode(la, LVT_PMC, lapic->lvt_pcint);

	/* Program timer LVT and setup handler. */
	la->lvt_timer_cache = lapic->lvt_timer =
	    lvt_mode(la, LVT_TIMER, lapic->lvt_timer);
	if (boot) {
		snprintf(buf, sizeof(buf), "cpu%d:timer", PCPU_GET(cpuid));
		intrcnt_add(buf, &la->la_timer_count);
	}

	/* Setup the timer if configured. */
	if (la->la_timer_mode != 0) {
		KASSERT(la->la_timer_period != 0, ("lapic%u: zero divisor",
		    lapic_id()));
		lapic_timer_set_divisor(lapic_timer_divisor);
		if (la->la_timer_mode == 1)
			lapic_timer_periodic(la, la->la_timer_period, 1);
		else
			lapic_timer_oneshot(la, la->la_timer_period, 1);
	}

	/* Program error LVT and clear any existing errors. */
	lapic->lvt_error = lvt_mode(la, LVT_ERROR, lapic->lvt_error);
	lapic->esr = 0;

	/* XXX: Thermal LVT */

	/* Program the CMCI LVT entry if present. */
	if (maxlvt >= LVT_CMCI)
		lapic->lvt_cmci = lvt_mode(la, LVT_CMCI, lapic->lvt_cmci);
	    
	intr_restore(saveintr);
}

void
lapic_reenable_pmc(void)
{
#ifdef HWPMC_HOOKS
	uint32_t value;

	value =  lapic->lvt_pcint;
	value &= ~APIC_LVT_M;
	lapic->lvt_pcint = value;
#endif
}

#ifdef HWPMC_HOOKS
static void
lapic_update_pmc(void *dummy)
{
	struct lapic *la;

	la = &lapics[lapic_id()];
	lapic->lvt_pcint = lvt_mode(la, LVT_PMC, lapic->lvt_pcint);
}
#endif

int
lapic_enable_pmc(void)
{
#ifdef HWPMC_HOOKS
	u_int32_t maxlvt;

	/* Fail if the local APIC is not present. */
	if (lapic == NULL)
		return (0);

	/* Fail if the PMC LVT is not present. */
	maxlvt = (lapic->version & APIC_VER_MAXLVT) >> MAXLVTSHIFT;
	if (maxlvt < LVT_PMC)
		return (0);

	lvts[LVT_PMC].lvt_masked = 0;

#ifdef SMP
	/*
	 * If hwpmc was loaded at boot time then the APs may not be
	 * started yet.  In that case, don't forward the request to
	 * them as they will program the lvt when they start.
	 */
	if (smp_started)
		smp_rendezvous(NULL, lapic_update_pmc, NULL, NULL);
	else
#endif
		lapic_update_pmc(NULL);
	return (1);
#else
	return (0);
#endif
}

void
lapic_disable_pmc(void)
{
#ifdef HWPMC_HOOKS
	u_int32_t maxlvt;

	/* Fail if the local APIC is not present. */
	if (lapic == NULL)
		return;

	/* Fail if the PMC LVT is not present. */
	maxlvt = (lapic->version & APIC_VER_MAXLVT) >> MAXLVTSHIFT;
	if (maxlvt < LVT_PMC)
		return;

	lvts[LVT_PMC].lvt_masked = 1;

#ifdef SMP
	/* The APs should always be started when hwpmc is unloaded. */
	KASSERT(mp_ncpus == 1 || smp_started, ("hwpmc unloaded too early"));
#endif
	smp_rendezvous(NULL, lapic_update_pmc, NULL, NULL);
#endif
}

static int
lapic_et_start(struct eventtimer *et, sbintime_t first, sbintime_t period)
{
	struct lapic *la;
	u_long value;

	la = &lapics[PCPU_GET(apic_id)];
	if (et->et_frequency == 0) {
		/* Start off with a divisor of 2 (power on reset default). */
		lapic_timer_divisor = 2;
		/* Try to calibrate the local APIC timer. */
		do {
			lapic_timer_set_divisor(lapic_timer_divisor);
			lapic_timer_oneshot(la, APIC_TIMER_MAX_COUNT, 0);
			DELAY(1000000);
			value = APIC_TIMER_MAX_COUNT - lapic->ccr_timer;
			if (value != APIC_TIMER_MAX_COUNT)
				break;
			lapic_timer_divisor <<= 1;
		} while (lapic_timer_divisor <= 128);
		if (lapic_timer_divisor > 128)
			panic("lapic: Divisor too big");
		if (bootverbose)
			printf("lapic: Divisor %lu, Frequency %lu Hz\n",
			    lapic_timer_divisor, value);
		et->et_frequency = value;
		et->et_min_period = (0x00000002LLU << 32) / et->et_frequency;
		et->et_max_period = (0xfffffffeLLU << 32) / et->et_frequency;
	}
	if (la->la_timer_mode == 0)
		lapic_timer_set_divisor(lapic_timer_divisor);
	if (period != 0) {
		la->la_timer_mode = 1;
		la->la_timer_period = ((uint32_t)et->et_frequency * period) >> 32;
		lapic_timer_periodic(la, la->la_timer_period, 1);
	} else {
		la->la_timer_mode = 2;
		la->la_timer_period = ((uint32_t)et->et_frequency * first) >> 32;
		lapic_timer_oneshot(la, la->la_timer_period, 1);
	}
	return (0);
}

static int
lapic_et_stop(struct eventtimer *et)
{
	struct lapic *la = &lapics[PCPU_GET(apic_id)];

	la->la_timer_mode = 0;
	lapic_timer_stop(la);
	return (0);
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
lapic_resume(struct pic *pic, bool suspend_cancelled)
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

	isrc = intr_lookup_source(apic_idt_to_irq(PCPU_GET(apic_id),
	    vector));
	intr_execute_handlers(isrc, frame);
}

void
lapic_handle_timer(struct trapframe *frame)
{
	struct lapic *la;
	struct trapframe *oldframe;
	struct thread *td;

	/* Send EOI first thing. */
	lapic_eoi();

#if defined(SMP) && !defined(SCHED_ULE)
	/*
	 * Don't do any accounting for the disabled HTT cores, since it
	 * will provide misleading numbers for the userland.
	 *
	 * No locking is necessary here, since even if we lose the race
	 * when hlt_cpus_mask changes it is not a big deal, really.
	 *
	 * Don't do that for ULE, since ULE doesn't consider hlt_cpus_mask
	 * and unlike other schedulers it actually schedules threads to
	 * those CPUs.
	 */
	if (CPU_ISSET(PCPU_GET(cpuid), &hlt_cpus_mask))
		return;
#endif

	/* Look up our local APIC structure for the tick counters. */
	la = &lapics[PCPU_GET(apic_id)];
	(*la->la_timer_count)++;
	critical_enter();
	if (lapic_et.et_active) {
		td = curthread;
		td->td_intr_nesting_level++;
		oldframe = td->td_intr_frame;
		td->td_intr_frame = frame;
		lapic_et.et_event_cb(&lapic_et, lapic_et.et_arg);
		td->td_intr_frame = oldframe;
		td->td_intr_nesting_level--;
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
lapic_timer_oneshot(struct lapic *la, u_int count, int enable_int)
{
	u_int32_t value;

	value = la->lvt_timer_cache;
	value &= ~APIC_LVTT_TM;
	value |= APIC_LVTT_TM_ONE_SHOT;
	if (enable_int)
		value &= ~APIC_LVT_M;
	lapic->lvt_timer = value;
	lapic->icr_timer = count;
}

static void
lapic_timer_periodic(struct lapic *la, u_int count, int enable_int)
{
	u_int32_t value;

	value = la->lvt_timer_cache;
	value &= ~APIC_LVTT_TM;
	value |= APIC_LVTT_TM_PERIODIC;
	if (enable_int)
		value &= ~APIC_LVT_M;
	lapic->lvt_timer = value;
	lapic->icr_timer = count;
}

static void
lapic_timer_stop(struct lapic *la)
{
	u_int32_t value;

	value = la->lvt_timer_cache;
	value &= ~APIC_LVTT_TM;
	value |= APIC_LVT_M;
	lapic->lvt_timer = value;
}

void
lapic_handle_cmc(void)
{

	lapic_eoi();
	cmc_intr();
}

/*
 * Called from the mca_init() to activate the CMC interrupt if this CPU is
 * responsible for monitoring any MC banks for CMC events.  Since mca_init()
 * is called prior to lapic_setup() during boot, this just needs to unmask
 * this CPU's LVT_CMCI entry.
 */
void
lapic_enable_cmc(void)
{
	u_int apic_id;

#ifdef DEV_ATPIC
	if (lapic == NULL)
		return;
#endif
	apic_id = PCPU_GET(apic_id);
	KASSERT(lapics[apic_id].la_present,
	    ("%s: missing APIC %u", __func__, apic_id));
	lapics[apic_id].la_lvts[LVT_CMCI].lvt_masked = 0;
	lapics[apic_id].la_lvts[LVT_CMCI].lvt_active = 1;
	if (bootverbose)
		printf("lapic%u: CMCI unmasked\n", apic_id);
}

void
lapic_handle_error(void)
{
	u_int32_t esr;

	/*
	 * Read the contents of the error status register.  Write to
	 * the register first before reading from it to force the APIC
	 * to update its value to indicate any errors that have
	 * occurred since the previous write to the register.
	 */
	lapic->esr = 0;
	esr = lapic->esr;

	printf("CPU%d: local APIC error 0x%x\n", PCPU_GET(cpuid), esr);
	lapic_eoi();
}

u_int
apic_cpuid(u_int apic_id)
{
#ifdef SMP
	return apic_cpuids[apic_id];
#else
	return 0;
#endif
}

/* Request a free IDT vector to be used by the specified IRQ. */
u_int
apic_alloc_vector(u_int apic_id, u_int irq)
{
	u_int vector;

	KASSERT(irq < NUM_IO_INTS, ("Invalid IRQ %u", irq));

	/*
	 * Search for a free vector.  Currently we just use a very simple
	 * algorithm to find the first free vector.
	 */
	mtx_lock_spin(&icu_lock);
	for (vector = 0; vector < APIC_NUM_IOINTS; vector++) {
		if (lapics[apic_id].la_ioint_irqs[vector] != -1)
			continue;
		lapics[apic_id].la_ioint_irqs[vector] = irq;
		mtx_unlock_spin(&icu_lock);
		return (vector + APIC_IO_INTS);
	}
	mtx_unlock_spin(&icu_lock);
	return (0);
}

/*
 * Request 'count' free contiguous IDT vectors to be used by 'count'
 * IRQs.  'count' must be a power of two and the vectors will be
 * aligned on a boundary of 'align'.  If the request cannot be
 * satisfied, 0 is returned.
 */
u_int
apic_alloc_vectors(u_int apic_id, u_int *irqs, u_int count, u_int align)
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
		if (lapics[apic_id].la_ioint_irqs[vector] != -1) {
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
			lapics[apic_id].la_ioint_irqs[first + vector] =
			    irqs[vector];
		mtx_unlock_spin(&icu_lock);
		return (first + APIC_IO_INTS);
	}
	mtx_unlock_spin(&icu_lock);
	printf("APIC: Couldn't find APIC vectors for %u IRQs\n", count);
	return (0);
}

/*
 * Enable a vector for a particular apic_id.  Since all lapics share idt
 * entries and ioint_handlers this enables the vector on all lapics.  lapics
 * which do not have the vector configured would report spurious interrupts
 * should it fire.
 */
void
apic_enable_vector(u_int apic_id, u_int vector)
{

	KASSERT(vector != IDT_SYSCALL, ("Attempt to overwrite syscall entry"));
	KASSERT(ioint_handlers[vector / 32] != NULL,
	    ("No ISR handler for vector %u", vector));
#ifdef KDTRACE_HOOKS
	KASSERT(vector != IDT_DTRACE_RET,
	    ("Attempt to overwrite DTrace entry"));
#endif
	setidt(vector, ioint_handlers[vector / 32], SDT_APIC, SEL_KPL,
	    GSEL_APIC);
}

void
apic_disable_vector(u_int apic_id, u_int vector)
{

	KASSERT(vector != IDT_SYSCALL, ("Attempt to overwrite syscall entry"));
#ifdef KDTRACE_HOOKS
	KASSERT(vector != IDT_DTRACE_RET,
	    ("Attempt to overwrite DTrace entry"));
#endif
	KASSERT(ioint_handlers[vector / 32] != NULL,
	    ("No ISR handler for vector %u", vector));
#ifdef notyet
	/*
	 * We can not currently clear the idt entry because other cpus
	 * may have a valid vector at this offset.
	 */
	setidt(vector, &IDTVEC(rsvd), SDT_APICT, SEL_KPL, GSEL_APIC);
#endif
}

/* Release an APIC vector when it's no longer in use. */
void
apic_free_vector(u_int apic_id, u_int vector, u_int irq)
{
	struct thread *td;

	KASSERT(vector >= APIC_IO_INTS && vector != IDT_SYSCALL &&
	    vector <= APIC_IO_INTS + APIC_NUM_IOINTS,
	    ("Vector %u does not map to an IRQ line", vector));
	KASSERT(irq < NUM_IO_INTS, ("Invalid IRQ %u", irq));
	KASSERT(lapics[apic_id].la_ioint_irqs[vector - APIC_IO_INTS] ==
	    irq, ("IRQ mismatch"));
#ifdef KDTRACE_HOOKS
	KASSERT(vector != IDT_DTRACE_RET,
	    ("Attempt to overwrite DTrace entry"));
#endif

	/*
	 * Bind us to the cpu that owned the vector before freeing it so
	 * we don't lose an interrupt delivery race.
	 */
	td = curthread;
	if (!rebooting) {
		thread_lock(td);
		if (sched_is_bound(td))
			panic("apic_free_vector: Thread already bound.\n");
		sched_bind(td, apic_cpuid(apic_id));
		thread_unlock(td);
	}
	mtx_lock_spin(&icu_lock);
	lapics[apic_id].la_ioint_irqs[vector - APIC_IO_INTS] = -1;
	mtx_unlock_spin(&icu_lock);
	if (!rebooting) {
		thread_lock(td);
		sched_unbind(td);
		thread_unlock(td);
	}
}

/* Map an IDT vector (APIC) to an IRQ (interrupt source). */
u_int
apic_idt_to_irq(u_int apic_id, u_int vector)
{
	int irq;

	KASSERT(vector >= APIC_IO_INTS && vector != IDT_SYSCALL &&
	    vector <= APIC_IO_INTS + APIC_NUM_IOINTS,
	    ("Vector %u does not map to an IRQ line", vector));
#ifdef KDTRACE_HOOKS
	KASSERT(vector != IDT_DTRACE_RET,
	    ("Attempt to overwrite DTrace entry"));
#endif
	irq = lapics[apic_id].la_ioint_irqs[vector - APIC_IO_INTS];
	if (irq < 0)
		irq = 0;
	return (irq);
}

#ifdef DDB
/*
 * Dump data about APIC IDT vector mappings.
 */
DB_SHOW_COMMAND(apic, db_show_apic)
{
	struct intsrc *isrc;
	int i, verbose;
	u_int apic_id;
	u_int irq;

	if (strcmp(modif, "vv") == 0)
		verbose = 2;
	else if (strcmp(modif, "v") == 0)
		verbose = 1;
	else
		verbose = 0;
	for (apic_id = 0; apic_id <= MAX_APIC_ID; apic_id++) {
		if (lapics[apic_id].la_present == 0)
			continue;
		db_printf("Interrupts bound to lapic %u\n", apic_id);
		for (i = 0; i < APIC_NUM_IOINTS + 1 && !db_pager_quit; i++) {
			irq = lapics[apic_id].la_ioint_irqs[i];
			if (irq == -1 || irq == IRQ_SYSCALL)
				continue;
#ifdef KDTRACE_HOOKS
			if (irq == IRQ_DTRACE_RET)
				continue;
#endif
#ifdef XENHVM
			if (irq == IRQ_EVTCHN)
				continue;
#endif
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
 * We have to look for CPU's very, very early because certain subsystems
 * want to know how many CPU's we have extremely early on in the boot
 * process.
 */
static void
apic_init(void *dummy __unused)
{
	struct apic_enumerator *enumerator;
#ifndef __amd64__
	uint64_t apic_base;
#endif
	int retval, best;

	/* We only support built in local APICs. */
	if (!(cpu_feature & CPUID_APIC))
		return;

	/* Don't probe if APIC mode is disabled. */
	if (resource_disabled("apic", 0))
		return;

	/* Probe all the enumerators to find the best match. */
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
#ifndef DEV_ATPIC
		panic("running without device atpic requires a local APIC");
#endif
		return;
	}

	if (bootverbose)
		printf("APIC: Using the %s enumerator.\n",
		    best_enum->apic_name);

#ifndef __amd64__
	/*
	 * To work around an errata, we disable the local APIC on some
	 * CPUs during early startup.  We need to turn the local APIC back
	 * on on such CPUs now.
	 */
	if (cpu == CPU_686 && cpu_vendor_id == CPU_VENDOR_INTEL &&
	    (cpu_id & 0xff0) == 0x610) {
		apic_base = rdmsr(MSR_APICBASE);
		apic_base |= APICBASE_ENABLED;
		wrmsr(MSR_APICBASE, apic_base);
	}
#endif

	/* Probe the CPU's in the system. */
	retval = best_enum->apic_probe_cpus();
	if (retval != 0)
		printf("%s: Failed to probe CPUs: returned %d\n",
		    best_enum->apic_name, retval);

}
SYSINIT(apic_init, SI_SUB_TUNABLES - 1, SI_ORDER_SECOND, apic_init, NULL);

/*
 * Setup the local APIC.  We have to do this prior to starting up the APs
 * in the SMP case.
 */
static void
apic_setup_local(void *dummy __unused)
{
	int retval;
 
	if (best_enum == NULL)
		return;

	/* Initialize the local APIC. */
	retval = best_enum->apic_setup_local();
	if (retval != 0)
		printf("%s: Failed to setup the local APIC: returned %d\n",
		    best_enum->apic_name, retval);
}
SYSINIT(apic_setup_local, SI_SUB_CPU, SI_ORDER_SECOND, apic_setup_local, NULL);

/*
 * Setup the I/O APICs.
 */
static void
apic_setup_io(void *dummy __unused)
{
	int retval;

	if (best_enum == NULL)
		return;

	/*
	 * Local APIC must be registered before other PICs and pseudo PICs
	 * for proper suspend/resume order.
	 */
#ifndef XEN
	intr_register_pic(&lapic_pic);
#endif

	retval = best_enum->apic_setup_io();
	if (retval != 0)
		printf("%s: Failed to setup I/O APICs: returned %d\n",
		    best_enum->apic_name, retval);
#ifdef XEN
	return;
#endif
	/*
	 * Finish setting up the local APIC on the BSP once we know how to
	 * properly program the LINT pins.
	 */
	lapic_setup(1);
	if (bootverbose)
		lapic_dump("BSP");

	/* Enable the MSI "pic". */
	msi_init();
}
SYSINIT(apic_setup_io, SI_SUB_INTR, SI_ORDER_SECOND, apic_setup_io, NULL);

#ifdef SMP
/*
 * Inter Processor Interrupt functions.  The lapic_ipi_*() functions are
 * private to the MD code.  The public interface for the rest of the
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
	register_t value, saveintr;

	/* XXX: Need more sanity checking of icrlo? */
	KASSERT(lapic != NULL, ("%s called too early", __func__));
	KASSERT((dest & ~(APIC_ID_MASK >> APIC_ID_SHIFT)) == 0,
	    ("%s: invalid dest field", __func__));
	KASSERT((icrlo & APIC_ICRLO_RESV_MASK) == 0,
	    ("%s: reserved bits set in ICR LO register", __func__));

	/* Set destination in ICR HI register if it is being used. */
	saveintr = intr_disable();
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
	intr_restore(saveintr);
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

	icrlo = APIC_DESTMODE_PHY | APIC_TRIGMOD_EDGE;

	/*
	 * IPI_STOP_HARD is just a "fake" vector used to send a NMI.
	 * Use special rules regard NMI if passed, otherwise specify
	 * the vector.
	 */
	if (vector == IPI_STOP_HARD)
		icrlo |= APIC_DELMODE_NMI | APIC_LEVEL_ASSERT;
	else
		icrlo |= vector | APIC_DELMODE_FIXED | APIC_LEVEL_DEASSERT;
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
