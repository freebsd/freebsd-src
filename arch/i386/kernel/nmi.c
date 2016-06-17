/*
 *  linux/arch/i386/nmi.c
 *
 *  NMI watchdog support on APIC systems
 *
 *  Started by Ingo Molnar <mingo@redhat.com>
 *
 *  Fixes:
 *  Mikael Pettersson	: AMD K7 support for local APIC NMI watchdog.
 *  Mikael Pettersson	: Power Management for local APIC NMI watchdog.
 *  Mikael Pettersson	: Pentium 4 support for local APIC NMI watchdog.
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/mc146818rtc.h>
#include <linux/kernel_stat.h>

#include <asm/smp.h>
#include <asm/mtrr.h>
#include <asm/mpspec.h>

unsigned int nmi_watchdog = NMI_NONE;
static unsigned int nmi_hz = HZ;
unsigned int nmi_perfctr_msr;	/* the MSR to reset in NMI handler */
extern void show_registers(struct pt_regs *regs);

#define K7_EVNTSEL_ENABLE	(1 << 22)
#define K7_EVNTSEL_INT		(1 << 20)
#define K7_EVNTSEL_OS		(1 << 17)
#define K7_EVNTSEL_USR		(1 << 16)
#define K7_EVENT_CYCLES_PROCESSOR_IS_RUNNING	0x76
#define K7_NMI_EVENT		K7_EVENT_CYCLES_PROCESSOR_IS_RUNNING

#define P6_EVNTSEL0_ENABLE	(1 << 22)
#define P6_EVNTSEL_INT		(1 << 20)
#define P6_EVNTSEL_OS		(1 << 17)
#define P6_EVNTSEL_USR		(1 << 16)
#define P6_EVENT_CPU_CLOCKS_NOT_HALTED	0x79
#define P6_NMI_EVENT		P6_EVENT_CPU_CLOCKS_NOT_HALTED

#define MSR_P4_MISC_ENABLE	0x1A0
#define MSR_P4_MISC_ENABLE_PERF_AVAIL	(1<<7)
#define MSR_P4_MISC_ENABLE_PEBS_UNAVAIL	(1<<12)
#define MSR_P4_PERFCTR0		0x300
#define MSR_P4_CCCR0		0x360
#define P4_ESCR_EVENT_SELECT(N)	((N)<<25)
#define P4_ESCR_OS		(1<<3)
#define P4_ESCR_USR		(1<<2)
#define P4_CCCR_OVF_PMI		(1<<26)
#define P4_CCCR_THRESHOLD(N)	((N)<<20)
#define P4_CCCR_COMPLEMENT	(1<<19)
#define P4_CCCR_COMPARE		(1<<18)
#define P4_CCCR_REQUIRED	(3<<16)
#define P4_CCCR_ESCR_SELECT(N)	((N)<<13)
#define P4_CCCR_ENABLE		(1<<12)
/* Set up IQ_COUNTER0 to behave like a clock, by having IQ_CCCR0 filter
   CRU_ESCR0 (with any non-null event selector) through a complemented
   max threshold. [IA32-Vol3, Section 14.9.9] */
#define MSR_P4_IQ_COUNTER0	0x30C
#define MSR_P4_IQ_CCCR0		0x36C
#define MSR_P4_CRU_ESCR0	0x3B8
#define P4_NMI_CRU_ESCR0	(P4_ESCR_EVENT_SELECT(0x3F)|P4_ESCR_OS|P4_ESCR_USR)
#define P4_NMI_IQ_CCCR0	\
	(P4_CCCR_OVF_PMI|P4_CCCR_THRESHOLD(15)|P4_CCCR_COMPLEMENT|	\
	 P4_CCCR_COMPARE|P4_CCCR_REQUIRED|P4_CCCR_ESCR_SELECT(4)|P4_CCCR_ENABLE)

int __init check_nmi_watchdog (void)
{
	unsigned int prev_nmi_count[NR_CPUS];
	int j, cpu;

	printk(KERN_INFO "testing NMI watchdog ... ");

	for (j = 0; j < smp_num_cpus; j++) {
		cpu = cpu_logical_map(j);
		prev_nmi_count[cpu] = irq_stat[cpu].__nmi_count;
	}
	sti();
	mdelay((10*1000)/nmi_hz); // wait 10 ticks

	for (j = 0; j < smp_num_cpus; j++) {
		cpu = cpu_logical_map(j);
		if (nmi_count(cpu) - prev_nmi_count[cpu] <= 5) {
			printk("CPU#%d: NMI appears to be stuck!\n", cpu);
			return -1;
		}
	}
	printk("OK.\n");

	/* now that we know it works we can reduce NMI frequency to
	   something more reasonable; makes a difference in some configs */
	if (nmi_watchdog == NMI_LOCAL_APIC)
		nmi_hz = 1;

	return 0;
}

static int __init setup_nmi_watchdog(char *str)
{
	int nmi;

	get_option(&str, &nmi);

	if (nmi >= NMI_INVALID)
		return 0;
	if (nmi == NMI_NONE)
		nmi_watchdog = nmi;
	/*
	 * If any other x86 CPU has a local APIC, then
	 * please test the NMI stuff there and send me the
	 * missing bits. Right now Intel P6/P4 and AMD K7 only.
	 */
	if ((nmi == NMI_LOCAL_APIC) &&
			(boot_cpu_data.x86_vendor == X86_VENDOR_INTEL) &&
			(boot_cpu_data.x86 == 6 || boot_cpu_data.x86 == 15))
		nmi_watchdog = nmi;
	if ((nmi == NMI_LOCAL_APIC) &&
			(boot_cpu_data.x86_vendor == X86_VENDOR_AMD) &&
	       	    ((boot_cpu_data.x86 == 6) || (boot_cpu_data.x86 == 15)))
		nmi_watchdog = nmi;
	/*
	 * We can enable the IO-APIC watchdog
	 * unconditionally.
	 */
	if (nmi == NMI_IO_APIC)
		nmi_watchdog = nmi;
	return 1;
}

__setup("nmi_watchdog=", setup_nmi_watchdog);

#ifdef CONFIG_PM

#include <linux/pm.h>

struct pm_dev *nmi_pmdev;

static void disable_apic_nmi_watchdog(void)
{
	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_AMD:
		wrmsr(MSR_K7_EVNTSEL0, 0, 0);
		break;
	case X86_VENDOR_INTEL:
		switch (boot_cpu_data.x86) {
		case 6:
			wrmsr(MSR_P6_EVNTSEL0, 0, 0);
			break;
		case 15:
			wrmsr(MSR_P4_IQ_CCCR0, 0, 0);
			wrmsr(MSR_P4_CRU_ESCR0, 0, 0);
			break;
		}
		break;
	}
}

static int nmi_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch (rqst) {
	case PM_SUSPEND:
		disable_apic_nmi_watchdog();
		break;
	case PM_RESUME:
		setup_apic_nmi_watchdog();
		break;
	}
	return 0;
}

static void nmi_pm_init(void)
{
	if (!nmi_pmdev)
		nmi_pmdev = apic_pm_register(PM_SYS_DEV, 0, nmi_pm_callback);
}

#define __pminit	/*empty*/

#else	/* CONFIG_PM */

static inline void nmi_pm_init(void) { }

#define __pminit	__init

#endif	/* CONFIG_PM */

/*
 * Activate the NMI watchdog via the local APIC.
 * Original code written by Keith Owens.
 */

static void __pminit clear_msr_range(unsigned int base, unsigned int n)
{
	unsigned int i;

	for(i = 0; i < n; ++i)
		wrmsr(base+i, 0, 0);
}

static void __pminit setup_k7_watchdog(void)
{
	unsigned int evntsel;

	nmi_perfctr_msr = MSR_K7_PERFCTR0;

	clear_msr_range(MSR_K7_EVNTSEL0, 4);
	clear_msr_range(MSR_K7_PERFCTR0, 4);

	evntsel = K7_EVNTSEL_INT
		| K7_EVNTSEL_OS
		| K7_EVNTSEL_USR
		| K7_NMI_EVENT;

	wrmsr(MSR_K7_EVNTSEL0, evntsel, 0);
	Dprintk("setting K7_PERFCTR0 to %08lx\n", -(cpu_khz/nmi_hz*1000));
	wrmsr(MSR_K7_PERFCTR0, -(cpu_khz/nmi_hz*1000), -1);
	apic_write(APIC_LVTPC, APIC_DM_NMI);
	evntsel |= K7_EVNTSEL_ENABLE;
	wrmsr(MSR_K7_EVNTSEL0, evntsel, 0);
}

static void __pminit setup_p6_watchdog(void)
{
	unsigned int evntsel;

	nmi_perfctr_msr = MSR_P6_PERFCTR0;

	clear_msr_range(MSR_P6_EVNTSEL0, 2);
	clear_msr_range(MSR_P6_PERFCTR0, 2);

	evntsel = P6_EVNTSEL_INT
		| P6_EVNTSEL_OS
		| P6_EVNTSEL_USR
		| P6_NMI_EVENT;

	wrmsr(MSR_P6_EVNTSEL0, evntsel, 0);
	Dprintk("setting P6_PERFCTR0 to %08lx\n", -(cpu_khz/nmi_hz*1000));
	wrmsr(MSR_P6_PERFCTR0, -(cpu_khz/nmi_hz*1000), 0);
	apic_write(APIC_LVTPC, APIC_DM_NMI);
	evntsel |= P6_EVNTSEL0_ENABLE;
	wrmsr(MSR_P6_EVNTSEL0, evntsel, 0);
}

static int __pminit setup_p4_watchdog(void)
{
	unsigned int misc_enable, dummy;

	rdmsr(MSR_P4_MISC_ENABLE, misc_enable, dummy);
	if (!(misc_enable & MSR_P4_MISC_ENABLE_PERF_AVAIL))
		return 0;

	nmi_perfctr_msr = MSR_P4_IQ_COUNTER0;

	if (!(misc_enable & MSR_P4_MISC_ENABLE_PEBS_UNAVAIL))
		clear_msr_range(0x3F1, 2);
	/* MSR 0x3F0 seems to have a default value of 0xFC00, but current
	   docs doesn't fully define it, so leave it alone for now. */
	clear_msr_range(0x3A0, 31);
	clear_msr_range(0x3C0, 6);
	clear_msr_range(0x3C8, 6);
	clear_msr_range(0x3E0, 2);
	clear_msr_range(MSR_P4_CCCR0, 18);
	clear_msr_range(MSR_P4_PERFCTR0, 18);

	wrmsr(MSR_P4_CRU_ESCR0, P4_NMI_CRU_ESCR0, 0);
	wrmsr(MSR_P4_IQ_CCCR0, P4_NMI_IQ_CCCR0 & ~P4_CCCR_ENABLE, 0);
	Dprintk("setting P4_IQ_COUNTER0 to 0x%08lx\n", -(cpu_khz/nmi_hz*1000));
	wrmsr(MSR_P4_IQ_COUNTER0, -(cpu_khz/nmi_hz*1000), -1);
	apic_write(APIC_LVTPC, APIC_DM_NMI);
	wrmsr(MSR_P4_IQ_CCCR0, P4_NMI_IQ_CCCR0, 0);
	return 1;
}

void __pminit setup_apic_nmi_watchdog (void)
{
	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_AMD:
		if (boot_cpu_data.x86 != 6 && boot_cpu_data.x86 != 15)
			return;
		setup_k7_watchdog();
		break;
	case X86_VENDOR_INTEL:
		switch (boot_cpu_data.x86) {
		case 6:
			setup_p6_watchdog();
			break;
		case 15:
			if (!setup_p4_watchdog())
				return;
			break;
		default:
			return;
		}
		break;
	default:
		return;
	}
	nmi_pm_init();
}

static spinlock_t nmi_print_lock = SPIN_LOCK_UNLOCKED;

/*
 * the best way to detect whether a CPU has a 'hard lockup' problem
 * is to check it's local APIC timer IRQ counts. If they are not
 * changing then that CPU has some problem.
 *
 * as these watchdog NMI IRQs are generated on every CPU, we only
 * have to check the current processor.
 *
 * since NMIs dont listen to _any_ locks, we have to be extremely
 * careful not to rely on unsafe variables. The printk might lock
 * up though, so we have to break up any console locks first ...
 * [when there will be more tty-related locks, break them up
 *  here too!]
 */

static unsigned int
	last_irq_sums [NR_CPUS],
	alert_counter [NR_CPUS];

void touch_nmi_watchdog (void)
{
	int i;

	/*
	 * Just reset the alert counters, (other CPUs might be
	 * spinning on locks we hold):
	 */
	for (i = 0; i < smp_num_cpus; i++)
		alert_counter[i] = 0;
}

void nmi_watchdog_tick (struct pt_regs * regs)
{

	/*
	 * Since current-> is always on the stack, and we always switch
	 * the stack NMI-atomically, it's safe to use smp_processor_id().
	 */
	int sum, cpu = smp_processor_id();

	sum = apic_timer_irqs[cpu];

	if (last_irq_sums[cpu] == sum) {
		/*
		 * Ayiee, looks like this CPU is stuck ...
		 * wait a few IRQs (5 seconds) before doing the oops ...
		 */
		alert_counter[cpu]++;
		if (alert_counter[cpu] == 5*nmi_hz) {
			spin_lock(&nmi_print_lock);
			/*
			 * We are in trouble anyway, lets at least try
			 * to get a message out.
			 */
			bust_spinlocks(1);
			printk("NMI Watchdog detected LOCKUP on CPU%d, eip %08lx, registers:\n", cpu, regs->eip);
			show_registers(regs);
			printk("console shuts up ...\n");
			console_silent();
			spin_unlock(&nmi_print_lock);
			bust_spinlocks(0);
			do_exit(SIGSEGV);
		}
	} else {
		last_irq_sums[cpu] = sum;
		alert_counter[cpu] = 0;
	}
	if (nmi_perfctr_msr) {
		if (nmi_perfctr_msr == MSR_P4_IQ_COUNTER0) {
			/*
			 * P4 quirks:
			 * - An overflown perfctr will assert its interrupt
			 *   until the OVF flag in its CCCR is cleared.
			 * - LVTPC is masked on interrupt and must be
			 *   unmasked by the LVTPC handler.
			 */
			wrmsr(MSR_P4_IQ_CCCR0, P4_NMI_IQ_CCCR0, 0);
			apic_write(APIC_LVTPC, APIC_DM_NMI);
		}
		wrmsr(nmi_perfctr_msr, -(cpu_khz/nmi_hz*1000), -1);
	}
}
