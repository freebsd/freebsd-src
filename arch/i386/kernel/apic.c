/*
 *	Local APIC handling, local APIC timers
 *
 *	(c) 1999, 2000 Ingo Molnar <mingo@redhat.com>
 *
 *	Fixes
 *	Maciej W. Rozycki	:	Bits for genuine 82489DX APICs;
 *					thanks to Eric Gilmore
 *					and Rolf G. Tews
 *					for testing these extensively.
 *	Maciej W. Rozycki	:	Various updates and fixes.
 *	Mikael Pettersson	:	Power Management for UP-APIC.
 */

#include <linux/config.h>
#include <linux/init.h>

#include <linux/mm.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/mc146818rtc.h>
#include <linux/kernel_stat.h>

#include <asm/atomic.h>
#include <asm/smp.h>
#include <asm/mtrr.h>
#include <asm/mpspec.h>
#include <asm/pgalloc.h>
#include <asm/smpboot.h>

/* Using APIC to generate smp_local_timer_interrupt? */
int using_apic_timer = 0;

int prof_multiplier[NR_CPUS] = { 1, };
int prof_old_multiplier[NR_CPUS] = { 1, };
int prof_counter[NR_CPUS] = { 1, };

static int enabled_via_apicbase;

int get_maxlvt(void)
{
	unsigned int v, ver, maxlvt;

	v = apic_read(APIC_LVR);
	ver = GET_APIC_VERSION(v);
	/* 82489DXs do not report # of LVT entries. */
	maxlvt = APIC_INTEGRATED(ver) ? GET_APIC_MAXLVT(v) : 2;
	return maxlvt;
}

void clear_local_APIC(void)
{
	int maxlvt;
	unsigned long v;

	maxlvt = get_maxlvt();

	/*
	 * Masking an LVT entry on a P6 can trigger a local APIC error
	 * if the vector is zero. Mask LVTERR first to prevent this.
	 */
	if (maxlvt >= 3) {
		v = ERROR_APIC_VECTOR; /* any non-zero vector will do */
		apic_write_around(APIC_LVTERR, v | APIC_LVT_MASKED);
	}
	/*
	 * Careful: we have to set masks only first to deassert
	 * any level-triggered sources.
	 */
	v = apic_read(APIC_LVTT);
	apic_write_around(APIC_LVTT, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT0);
	apic_write_around(APIC_LVT0, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write_around(APIC_LVT1, v | APIC_LVT_MASKED);
	if (maxlvt >= 4) {
		v = apic_read(APIC_LVTPC);
		apic_write_around(APIC_LVTPC, v | APIC_LVT_MASKED);
	}

	/*
	 * Clean APIC state for other OSs:
	 */
	apic_write_around(APIC_LVTT, APIC_LVT_MASKED);
	apic_write_around(APIC_LVT0, APIC_LVT_MASKED);
	apic_write_around(APIC_LVT1, APIC_LVT_MASKED);
	if (maxlvt >= 3)
		apic_write_around(APIC_LVTERR, APIC_LVT_MASKED);
	if (maxlvt >= 4)
		apic_write_around(APIC_LVTPC, APIC_LVT_MASKED);
	v = GET_APIC_VERSION(apic_read(APIC_LVR));
	if (APIC_INTEGRATED(v)) {	/* !82489DX */
		if (maxlvt > 3)
			apic_write(APIC_ESR, 0);
		apic_read(APIC_ESR);
	}
}

void __init connect_bsp_APIC(void)
{
	if (pic_mode) {
		/*
		 * Do not trust the local APIC being empty at bootup.
		 */
		clear_local_APIC();
		/*
		 * PIC mode, enable APIC mode in the IMCR, i.e.
		 * connect BSP's local APIC to INT and NMI lines.
		 */
		printk("leaving PIC mode, enabling APIC mode.\n");
		outb(0x70, 0x22);
		outb(0x01, 0x23);
	}
}

void disconnect_bsp_APIC(void)
{
	if (pic_mode) {
		/*
		 * Put the board back into PIC mode (has an effect
		 * only on certain older boards).  Note that APIC
		 * interrupts, including IPIs, won't work beyond
		 * this point!  The only exception are INIT IPIs.
		 */
		printk("disabling APIC mode, entering PIC mode.\n");
		outb(0x70, 0x22);
		outb(0x00, 0x23);
	}
}

void disable_local_APIC(void)
{
	unsigned long value;

	clear_local_APIC();

	/*
	 * Disable APIC (implies clearing of registers
	 * for 82489DX!).
	 */
	value = apic_read(APIC_SPIV);
	value &= ~APIC_SPIV_APIC_ENABLED;
	apic_write_around(APIC_SPIV, value);

	if (enabled_via_apicbase) {
		unsigned int l, h;
		rdmsr(MSR_IA32_APICBASE, l, h);
		l &= ~MSR_IA32_APICBASE_ENABLE;
		wrmsr(MSR_IA32_APICBASE, l, h);
	}
}

/*
 * This is to verify that we're looking at a real local APIC.
 * Check these against your board if the CPUs aren't getting
 * started for no apparent reason.
 */
int __init verify_local_APIC(void)
{
	unsigned int reg0, reg1;

	/*
	 * The version register is read-only in a real APIC.
	 */
	reg0 = apic_read(APIC_LVR);
	Dprintk("Getting VERSION: %x\n", reg0);
	apic_write(APIC_LVR, reg0 ^ APIC_LVR_MASK);
	reg1 = apic_read(APIC_LVR);
	Dprintk("Getting VERSION: %x\n", reg1);

	/*
	 * The two version reads above should print the same
	 * numbers.  If the second one is different, then we
	 * poke at a non-APIC.
	 */
	if (reg1 != reg0)
		return 0;

	/*
	 * Check if the version looks reasonably.
	 */
	reg1 = GET_APIC_VERSION(reg0);
	if (reg1 == 0x00 || reg1 == 0xff)
		return 0;
	reg1 = get_maxlvt();
	if (reg1 < 0x02 || reg1 == 0xff)
		return 0;

	/*
	 * The ID register is read/write in a real APIC.
	 */
	reg0 = apic_read(APIC_ID);
	Dprintk("Getting ID: %x\n", reg0);
	apic_write(APIC_ID, reg0 ^ APIC_ID_MASK);
	reg1 = apic_read(APIC_ID);
	Dprintk("Getting ID: %x\n", reg1);
	apic_write(APIC_ID, reg0);
	if (reg1 != (reg0 ^ APIC_ID_MASK))
		return 0;

	/*
	 * The next two are just to see if we have sane values.
	 * They're only really relevant if we're in Virtual Wire
	 * compatibility mode, but most boxes are anymore.
	 */
	reg0 = apic_read(APIC_LVT0);
	Dprintk("Getting LVT0: %x\n", reg0);
	reg1 = apic_read(APIC_LVT1);
	Dprintk("Getting LVT1: %x\n", reg1);

	return 1;
}

void __init sync_Arb_IDs(void)
{
	/*
	 * Wait for idle.
	 */
	apic_wait_icr_idle();

	Dprintk("Synchronizing Arb IDs.\n");
	apic_write_around(APIC_ICR, APIC_DEST_ALLINC | APIC_INT_LEVELTRIG
				| APIC_DM_INIT);
}

extern void __error_in_apic_c (void);

/*
 * An initial setup of the virtual wire mode.
 */
void __init init_bsp_APIC(void)
{
	unsigned long value, ver;

	/*
	 * Don't do the setup now if we have a SMP BIOS as the
	 * through-I/O-APIC virtual wire mode might be active.
	 */
	if (smp_found_config || !cpu_has_apic)
		return;

	value = apic_read(APIC_LVR);
	ver = GET_APIC_VERSION(value);

	/*
	 * Do not trust the local APIC being empty at bootup.
	 */
	clear_local_APIC();

	/*
	 * Enable APIC.
	 */
	value = apic_read(APIC_SPIV);
	value &= ~APIC_VECTOR_MASK;
	value |= APIC_SPIV_APIC_ENABLED;
	value |= APIC_SPIV_FOCUS_DISABLED;
	value |= SPURIOUS_APIC_VECTOR;
	apic_write_around(APIC_SPIV, value);

	/*
	 * Set up the virtual wire mode.
	 */
	apic_write_around(APIC_LVT0, APIC_DM_EXTINT);
	value = APIC_DM_NMI;
	if (!APIC_INTEGRATED(ver))		/* 82489DX */
		value |= APIC_LVT_LEVEL_TRIGGER;
	apic_write_around(APIC_LVT1, value);
}

static unsigned long calculate_ldr(unsigned long old)
{
	unsigned long id;
	if(clustered_apic_mode == CLUSTERED_APIC_XAPIC)
		id = physical_to_logical_apicid(hard_smp_processor_id());
	else
		id = 1UL << smp_processor_id();
	return (old & ~APIC_LDR_MASK)|SET_APIC_LOGICAL_ID(id);
}

void __init setup_local_APIC (void)
{
	unsigned long value, ver, maxlvt;

	/* Pound the ESR really hard over the head with a big hammer - mbligh */
	if (esr_disable) {
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
	}

	value = apic_read(APIC_LVR);
	ver = GET_APIC_VERSION(value);

	if ((SPURIOUS_APIC_VECTOR & 0x0f) != 0x0f)
		__error_in_apic_c();

	/*
	 * Double-check wether this APIC is really registered.
	 * This is meaningless in clustered apic mode, so we skip it.
	 */
	if (!clustered_apic_mode && 
	    !test_bit(GET_APIC_ID(apic_read(APIC_ID)), &phys_cpu_present_map))
		BUG();

	/*
	 * Intel recommends to set DFR, LDR and TPR before enabling
	 * an APIC.  See e.g. "AP-388 82489DX User's Manual" (Intel
	 * document number 292116).  So here it goes...
	 */
	if (clustered_apic_mode != CLUSTERED_APIC_NUMAQ) {
		/*
		 * For NUMA-Q (clustered apic logical), the firmware does this
		 * for us. Otherwise put the APIC into clustered or flat
		 * delivery mode. Must be "all ones" explicitly for 82489DX.
		 */
		if(clustered_apic_mode == CLUSTERED_APIC_XAPIC)
			apic_write_around(APIC_DFR, APIC_DFR_CLUSTER);
		else
			apic_write_around(APIC_DFR, APIC_DFR_FLAT);

		/*
		 * Set up the logical destination ID.
		 */
		value = apic_read(APIC_LDR);
		apic_write_around(APIC_LDR, calculate_ldr(value));
	}

	/*
	 * Set Task Priority to 'accept all'. We never change this
	 * later on.
	 */
	value = apic_read(APIC_TASKPRI);
	value &= ~APIC_TPRI_MASK;
	apic_write_around(APIC_TASKPRI, value);

	/*
	 * Now that we are all set up, enable the APIC
	 */
	value = apic_read(APIC_SPIV);
	value &= ~APIC_VECTOR_MASK;
	/*
	 * Enable APIC
	 */
	value |= APIC_SPIV_APIC_ENABLED;

	/*
	 * Some unknown Intel IO/APIC (or APIC) errata is biting us with
	 * certain networking cards. If high frequency interrupts are
	 * happening on a particular IOAPIC pin, plus the IOAPIC routing
	 * entry is masked/unmasked at a high rate as well then sooner or
	 * later IOAPIC line gets 'stuck', no more interrupts are received
	 * from the device. If focus CPU is disabled then the hang goes
	 * away, oh well :-(
	 *
	 * [ This bug can be reproduced easily with a level-triggered
	 *   PCI Ne2000 networking cards and PII/PIII processors, dual
	 *   BX chipset. ]
	 */
	/*
	 * Actually disabling the focus CPU check just makes the hang less
	 * frequent as it makes the interrupt distributon model be more
	 * like LRU than MRU (the short-term load is more even across CPUs).
	 * See also the comment in end_level_ioapic_irq().  --macro
	 */
#if 1
	/* Enable focus processor (bit==0) */
	value &= ~APIC_SPIV_FOCUS_DISABLED;
#else
	/* Disable focus processor (bit==1) */
	value |= APIC_SPIV_FOCUS_DISABLED;
#endif
	/*
	 * Set spurious IRQ vector
	 */
	value |= SPURIOUS_APIC_VECTOR;
	apic_write_around(APIC_SPIV, value);

	/*
	 * Set up LVT0, LVT1:
	 *
	 * set up through-local-APIC on the BP's LINT0. This is not
	 * strictly necessery in pure symmetric-IO mode, but sometimes
	 * we delegate interrupts to the 8259A.
	 */
	/*
	 * TODO: set up through-local-APIC from through-I/O-APIC? --macro
	 */
	value = apic_read(APIC_LVT0) & APIC_LVT_MASKED;
	if (!smp_processor_id() && (pic_mode || !value)) {
		value = APIC_DM_EXTINT;
		printk("enabled ExtINT on CPU#%d\n", smp_processor_id());
	} else {
		value = APIC_DM_EXTINT | APIC_LVT_MASKED;
		printk("masked ExtINT on CPU#%d\n", smp_processor_id());
	}
	apic_write_around(APIC_LVT0, value);

	/*
	 * only the BP should see the LINT1 NMI signal, obviously.
	 */
	if (!smp_processor_id())
		value = APIC_DM_NMI;
	else
		value = APIC_DM_NMI | APIC_LVT_MASKED;
	if (!APIC_INTEGRATED(ver))		/* 82489DX */
		value |= APIC_LVT_LEVEL_TRIGGER;
	apic_write_around(APIC_LVT1, value);

	if (APIC_INTEGRATED(ver) && !esr_disable) {		/* !82489DX */
		maxlvt = get_maxlvt();
		if (maxlvt > 3)		/* Due to the Pentium erratum 3AP. */
			apic_write(APIC_ESR, 0);
		value = apic_read(APIC_ESR);
		printk("ESR value before enabling vector: %08lx\n", value);

		value = ERROR_APIC_VECTOR;      // enables sending errors
		apic_write_around(APIC_LVTERR, value);
		/*
		 * spec says clear errors after enabling vector.
		 */
		if (maxlvt > 3)
			apic_write(APIC_ESR, 0);
		value = apic_read(APIC_ESR);
		printk("ESR value after enabling vector: %08lx\n", value);
	} else {
		if (esr_disable)	
			/* 
			 * Something untraceble is creating bad interrupts on 
			 * secondary quads ... for the moment, just leave the
			 * ESR disabled - we can't do anything useful with the
			 * errors anyway - mbligh
			 */
			printk("Leaving ESR disabled.\n");
		else 
			printk("No ESR for 82489DX.\n");
	}

	if (nmi_watchdog == NMI_LOCAL_APIC)
		setup_apic_nmi_watchdog();
}

#ifdef CONFIG_PM

#include <linux/slab.h>
#include <linux/pm.h>

static struct {
	/* 'active' is true if the local APIC was enabled by us and
	   not the BIOS; this signifies that we are also responsible
	   for disabling it before entering apm/acpi suspend */
	int active;
	/* 'perfctr_pmdev' is here because the current (2.4.1) PM
	   callback system doesn't handle hierarchical dependencies */
	struct pm_dev *perfctr_pmdev;
	/* r/w apic fields */
	unsigned int apic_id;
	unsigned int apic_taskpri;
	unsigned int apic_ldr;
	unsigned int apic_dfr;
	unsigned int apic_spiv;
	unsigned int apic_lvtt;
	unsigned int apic_lvtpc;
	unsigned int apic_lvt0;
	unsigned int apic_lvt1;
	unsigned int apic_lvterr;
	unsigned int apic_tmict;
	unsigned int apic_tdcr;
} apic_pm_state;

static void apic_pm_suspend(void *data)
{
	unsigned long flags;

	if (apic_pm_state.perfctr_pmdev)
		pm_send(apic_pm_state.perfctr_pmdev, PM_SUSPEND, data);
	apic_pm_state.apic_id = apic_read(APIC_ID);
	apic_pm_state.apic_taskpri = apic_read(APIC_TASKPRI);
	apic_pm_state.apic_ldr = apic_read(APIC_LDR);
	apic_pm_state.apic_dfr = apic_read(APIC_DFR);
	apic_pm_state.apic_spiv = apic_read(APIC_SPIV);
	apic_pm_state.apic_lvtt = apic_read(APIC_LVTT);
	apic_pm_state.apic_lvtpc = apic_read(APIC_LVTPC);
	apic_pm_state.apic_lvt0 = apic_read(APIC_LVT0);
	apic_pm_state.apic_lvt1 = apic_read(APIC_LVT1);
	apic_pm_state.apic_lvterr = apic_read(APIC_LVTERR);
	apic_pm_state.apic_tmict = apic_read(APIC_TMICT);
	apic_pm_state.apic_tdcr = apic_read(APIC_TDCR);
	__save_flags(flags);
	__cli();
	disable_local_APIC();
	__restore_flags(flags);
}

static void apic_pm_resume(void *data)
{
	unsigned int l, h;
	unsigned long flags;

	__save_flags(flags);
	__cli();

	/*
	 * Make sure the APICBASE points to the right address
	 *
	 * FIXME! This will be wrong if we ever support suspend on
	 * SMP! We'll need to do this as part of the CPU restore!
	 */
	rdmsr(MSR_IA32_APICBASE, l, h);
	l &= ~MSR_IA32_APICBASE_BASE;
	l |= MSR_IA32_APICBASE_ENABLE | mp_lapic_addr;
	wrmsr(MSR_IA32_APICBASE, l, h);

	apic_write(APIC_LVTERR, ERROR_APIC_VECTOR | APIC_LVT_MASKED);
	apic_write(APIC_ID, apic_pm_state.apic_id);
	apic_write(APIC_DFR, apic_pm_state.apic_dfr);
	apic_write(APIC_LDR, apic_pm_state.apic_ldr);
	apic_write(APIC_TASKPRI, apic_pm_state.apic_taskpri);
	apic_write(APIC_SPIV, apic_pm_state.apic_spiv);
	apic_write(APIC_LVT0, apic_pm_state.apic_lvt0);
	apic_write(APIC_LVT1, apic_pm_state.apic_lvt1);
	apic_write(APIC_LVTPC, apic_pm_state.apic_lvtpc);
	apic_write(APIC_LVTT, apic_pm_state.apic_lvtt);
	apic_write(APIC_TDCR, apic_pm_state.apic_tdcr);
	apic_write(APIC_TMICT, apic_pm_state.apic_tmict);
	apic_write(APIC_ESR, 0);
	apic_read(APIC_ESR);
	apic_write(APIC_LVTERR, apic_pm_state.apic_lvterr);
	apic_write(APIC_ESR, 0);
	apic_read(APIC_ESR);
	__restore_flags(flags);
	if (apic_pm_state.perfctr_pmdev)
		pm_send(apic_pm_state.perfctr_pmdev, PM_RESUME, data);
}

static int apic_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch (rqst) {
	case PM_SUSPEND:
		apic_pm_suspend(data);
		break;
	case PM_RESUME:
		apic_pm_resume(data);
		break;
	}
	return 0;
}

/* perfctr driver should call this instead of pm_register() */
struct pm_dev *apic_pm_register(pm_dev_t type,
				unsigned long id,
				pm_callback callback)
{
	struct pm_dev *dev;

	if (!apic_pm_state.active)
		return pm_register(type, id, callback);
	if (apic_pm_state.perfctr_pmdev)
		return NULL;	/* we're busy */
	dev = kmalloc(sizeof(struct pm_dev), GFP_KERNEL);
	if (dev) {
		memset(dev, 0, sizeof(*dev));
		dev->type = type;
		dev->id = id;
		dev->callback = callback;
		apic_pm_state.perfctr_pmdev = dev;
	}
	return dev;
}

/* perfctr driver should call this instead of pm_unregister() */
void apic_pm_unregister(struct pm_dev *dev)
{
	if (!apic_pm_state.active) {
		pm_unregister(dev);
	} else if (dev == apic_pm_state.perfctr_pmdev) {
		apic_pm_state.perfctr_pmdev = NULL;
		kfree(dev);
	}
}

static void __init apic_pm_init1(void)
{
	/* can't pm_register() at this early stage in the boot process
	   (causes an immediate reboot), so just set the flag */
	apic_pm_state.active = 1;
}

static void __init apic_pm_init2(void)
{
	if (apic_pm_state.active)
		pm_register(PM_SYS_DEV, 0, apic_pm_callback);
}

#else	/* CONFIG_PM */

static inline void apic_pm_init1(void) { }
static inline void apic_pm_init2(void) { }

#endif	/* CONFIG_PM */

/*
 * Detect and enable local APICs on non-SMP boards.
 * Original code written by Keir Fraser.
 */

/*
 * Knob to control our willingness to enable the local APIC.
 */
int enable_local_apic __initdata = 0; /* -1=force-disable, +1=force-enable */

static int __init lapic_disable(char *str)
{
	enable_local_apic = -1;
	clear_bit(X86_FEATURE_APIC, boot_cpu_data.x86_capability);
	return 0;
}
__setup("nolapic", lapic_disable);

static int __init lapic_enable(char *str)
{
	enable_local_apic = 1;
	return 0;
}
__setup("lapic", lapic_enable);

static int __init detect_init_APIC (void)
{
	u32 h, l, features;
	extern void get_cpu_vendor(struct cpuinfo_x86*);

	/* Disabled by DMI scan or kernel option? */
	if (enable_local_apic < 0)
		return -1;

	/* Workaround for us being called before identify_cpu(). */
	get_cpu_vendor(&boot_cpu_data);

	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_AMD:
		if (boot_cpu_data.x86 == 6 && boot_cpu_data.x86_model > 1)
			break;
		if (boot_cpu_data.x86 == 15 && cpu_has_apic)
			break;
		goto no_apic;
	case X86_VENDOR_INTEL:
		if (boot_cpu_data.x86 == 6 ||
		    (boot_cpu_data.x86 == 15 && (cpu_has_apic || enable_local_apic > 0)) ||
		    (boot_cpu_data.x86 == 5 && cpu_has_apic))
			break;
		goto no_apic;
	default:
		goto no_apic;
	}

	if (!cpu_has_apic) {
		/*
		 * Some BIOSes disable the local APIC in the
		 * APIC_BASE MSR. This can only be done in
		 * software for Intel P6 and AMD K7 (Model > 1).
		 */
		rdmsr(MSR_IA32_APICBASE, l, h);
		if (!(l & MSR_IA32_APICBASE_ENABLE)) {
			printk("Local APIC disabled by BIOS -- reenabling.\n");
			l &= ~MSR_IA32_APICBASE_BASE;
			l |= MSR_IA32_APICBASE_ENABLE | APIC_DEFAULT_PHYS_BASE;
			wrmsr(MSR_IA32_APICBASE, l, h);
			enabled_via_apicbase = 1;
		}
	}
	/*
	 * The APIC feature bit should now be enabled
	 * in `cpuid'
	 */
	features = cpuid_edx(1);
	if (!(features & (1 << X86_FEATURE_APIC))) {
		printk("Could not enable APIC!\n");
		return -1;
	}
	set_bit(X86_FEATURE_APIC, &boot_cpu_data.x86_capability);
	mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;

	/* The BIOS may have set up the APIC at some other address */
	rdmsr(MSR_IA32_APICBASE, l, h);
	if (l & MSR_IA32_APICBASE_ENABLE)
		mp_lapic_addr = l & MSR_IA32_APICBASE_BASE;

	if (nmi_watchdog != NMI_NONE)
		nmi_watchdog = NMI_LOCAL_APIC;

	printk("Found and enabled local APIC!\n");

	apic_pm_init1();

	return 0;

no_apic:
	printk("No local APIC present or hardware disabled\n");
	return -1;
}

void __init init_apic_mappings(void)
{
	unsigned long apic_phys;

	/*
	 * If no local APIC can be found then set up a fake all
	 * zeroes page to simulate the local APIC and another
	 * one for the IO-APIC.
	 */
	if (!smp_found_config && detect_init_APIC()) {
		apic_phys = (unsigned long) alloc_bootmem_pages(PAGE_SIZE);
		apic_phys = __pa(apic_phys);
	} else
		apic_phys = mp_lapic_addr;

	set_fixmap_nocache(FIX_APIC_BASE, apic_phys);
	Dprintk("mapped APIC to %08lx (%08lx)\n", APIC_BASE, apic_phys);

	/*
	 * Fetch the APIC ID of the BSP in case we have a
	 * default configuration (or the MP table is broken).
	 */
	if (boot_cpu_physical_apicid == -1U)
		boot_cpu_physical_apicid = GET_APIC_ID(apic_read(APIC_ID));

#ifdef CONFIG_X86_IO_APIC
	{
		unsigned long ioapic_phys, idx = FIX_IO_APIC_BASE_0;
		int i;

		for (i = 0; i < nr_ioapics; i++) {
			if (smp_found_config) {
				ioapic_phys = mp_ioapics[i].mpc_apicaddr;
				if (!ioapic_phys) {
					printk(KERN_ERR "WARNING: bogus zero IO-APIC address found in MPTABLE, disabling IO/APIC support!\n");

					smp_found_config = 0;
					skip_ioapic_setup = 1;
					goto fake_ioapic_page;
				}
			} else {
fake_ioapic_page:
				ioapic_phys = (unsigned long) alloc_bootmem_pages(PAGE_SIZE);
				ioapic_phys = __pa(ioapic_phys);
			}
			set_fixmap_nocache(idx, ioapic_phys);
			Dprintk("mapped IOAPIC to %08lx (%08lx)\n",
					__fix_to_virt(idx), ioapic_phys);
			idx++;
		}
	}
#endif
}

/*
 * This part sets up the APIC 32 bit clock in LVTT1, with HZ interrupts
 * per second. We assume that the caller has already set up the local
 * APIC.
 *
 * The APIC timer is not exactly sync with the external timer chip, it
 * closely follows bus clocks.
 */

/*
 * The timer chip is already set up at HZ interrupts per second here,
 * but we do not accept timer interrupts yet. We only allow the BP
 * to calibrate.
 */
static unsigned int __init get_8254_timer_count(void)
{
	extern spinlock_t i8253_lock;
	unsigned long flags;

	unsigned int count;

	spin_lock_irqsave(&i8253_lock, flags);

	outb_p(0x00, 0x43);
	count = inb_p(0x40);
	count |= inb_p(0x40) << 8;

	spin_unlock_irqrestore(&i8253_lock, flags);

	return count;
}

void __init wait_8254_wraparound(void)
{
	unsigned int curr_count, prev_count=~0;
	int delta;

	curr_count = get_8254_timer_count();

	do {
		prev_count = curr_count;
		curr_count = get_8254_timer_count();
		delta = curr_count-prev_count;

	/*
	 * This limit for delta seems arbitrary, but it isn't, it's
	 * slightly above the level of error a buggy Mercury/Neptune
	 * chipset timer can cause.
	 */

	} while (delta < 300);
}

/*
 * This function sets up the local APIC timer, with a timeout of
 * 'clocks' APIC bus clock. During calibration we actually call
 * this function twice on the boot CPU, once with a bogus timeout
 * value, second time for real. The other (noncalibrating) CPUs
 * call this function only once, with the real, calibrated value.
 *
 * We do reads before writes even if unnecessary, to get around the
 * P5 APIC double write bug.
 */

#define APIC_DIVISOR 16

void __setup_APIC_LVTT(unsigned int clocks)
{
	unsigned int lvtt1_value, tmp_value;

	lvtt1_value = SET_APIC_TIMER_BASE(APIC_TIMER_BASE_DIV) |
			APIC_LVT_TIMER_PERIODIC | LOCAL_TIMER_VECTOR;
	apic_write_around(APIC_LVTT, lvtt1_value);

	/*
	 * Divide PICLK by 16
	 */
	tmp_value = apic_read(APIC_TDCR);
	apic_write_around(APIC_TDCR, (tmp_value
				& ~(APIC_TDR_DIV_1 | APIC_TDR_DIV_TMBASE))
				| APIC_TDR_DIV_16);

	apic_write_around(APIC_TMICT, clocks/APIC_DIVISOR);
}

void setup_APIC_timer(void * data)
{
	unsigned int clocks = (unsigned int) data, slice, t0, t1;
	unsigned long flags;
	int delta;

	__save_flags(flags);
	__sti();
	/*
	 * ok, Intel has some smart code in their APIC that knows
	 * if a CPU was in 'hlt' lowpower mode, and this increases
	 * its APIC arbitration priority. To avoid the external timer
	 * IRQ APIC event being in synchron with the APIC clock we
	 * introduce an interrupt skew to spread out timer events.
	 *
	 * The number of slices within a 'big' timeslice is smp_num_cpus+1
	 */

	slice = clocks / (smp_num_cpus+1);
	printk("cpu: %d, clocks: %d, slice: %d\n", smp_processor_id(), clocks, slice);

	/*
	 * Wait for IRQ0's slice:
	 */
	wait_8254_wraparound();

	__setup_APIC_LVTT(clocks);

	t0 = apic_read(APIC_TMICT)*APIC_DIVISOR;
	/* Wait till TMCCT gets reloaded from TMICT... */
	do {
		t1 = apic_read(APIC_TMCCT)*APIC_DIVISOR;
		delta = (int)(t0 - t1 - slice*(smp_processor_id()+1));
	} while (delta >= 0);
	/* Now wait for our slice for real. */
	do {
		t1 = apic_read(APIC_TMCCT)*APIC_DIVISOR;
		delta = (int)(t0 - t1 - slice*(smp_processor_id()+1));
	} while (delta < 0);

	__setup_APIC_LVTT(clocks);

	printk("CPU%d<T0:%d,T1:%d,D:%d,S:%d,C:%d>\n", smp_processor_id(), t0, t1, delta, slice, clocks);

	__restore_flags(flags);
}

/*
 * In this function we calibrate APIC bus clocks to the external
 * timer. Unfortunately we cannot use jiffies and the timer irq
 * to calibrate, since some later bootup code depends on getting
 * the first irq? Ugh.
 *
 * We want to do the calibration only once since we
 * want to have local timer irqs syncron. CPUs connected
 * by the same APIC bus have the very same bus frequency.
 * And we want to have irqs off anyways, no accidental
 * APIC irq that way.
 */

int __init calibrate_APIC_clock(void)
{
	unsigned long long t1 = 0, t2 = 0;
	long tt1, tt2;
	long result;
	int i;
	const int LOOPS = HZ/10;

	printk("calibrating APIC timer ...\n");

	/*
	 * Put whatever arbitrary (but long enough) timeout
	 * value into the APIC clock, we just want to get the
	 * counter running for calibration.
	 */
	__setup_APIC_LVTT(1000000000);

	/*
	 * The timer chip counts down to zero. Let's wait
	 * for a wraparound to start exact measurement:
	 * (the current tick might have been already half done)
	 */

	wait_8254_wraparound();

	/*
	 * We wrapped around just now. Let's start:
	 */
	if (cpu_has_tsc)
		rdtscll(t1);
	tt1 = apic_read(APIC_TMCCT);

	/*
	 * Let's wait LOOPS wraprounds:
	 */
	for (i = 0; i < LOOPS; i++)
		wait_8254_wraparound();

	tt2 = apic_read(APIC_TMCCT);
	if (cpu_has_tsc)
		rdtscll(t2);

	/*
	 * The APIC bus clock counter is 32 bits only, it
	 * might have overflown, but note that we use signed
	 * longs, thus no extra care needed.
	 *
	 * underflown to be exact, as the timer counts down ;)
	 */

	result = (tt1-tt2)*APIC_DIVISOR/LOOPS;

	if (cpu_has_tsc)
		printk("..... CPU clock speed is %ld.%04ld MHz.\n",
			((long)(t2-t1)/LOOPS)/(1000000/HZ),
			((long)(t2-t1)/LOOPS)%(1000000/HZ));

	printk("..... host bus clock speed is %ld.%04ld MHz.\n",
		result/(1000000/HZ),
		result%(1000000/HZ));

	return result;
}

static unsigned int calibration_result;

void __init setup_APIC_clocks (void)
{
	printk("Using local APIC timer interrupts.\n");
	using_apic_timer = 1;

	__cli();

	calibration_result = calibrate_APIC_clock();
	/*
	 * Now set up the timer for real.
	 */
	setup_APIC_timer((void *)calibration_result);

	__sti();

	/* and update all other cpus */
	smp_call_function(setup_APIC_timer, (void *)calibration_result, 1, 1);
}

void __init disable_APIC_timer(void)
{
	if (using_apic_timer) {
		unsigned long v;

		v = apic_read(APIC_LVTT);
		apic_write_around(APIC_LVTT, v | APIC_LVT_MASKED);
	}
}

void enable_APIC_timer(void)
{
	if (using_apic_timer) {
		unsigned long v;

		v = apic_read(APIC_LVTT);
		apic_write_around(APIC_LVTT, v & ~APIC_LVT_MASKED);
	}
}

/*
 * the frequency of the profiling timer can be changed
 * by writing a multiplier value into /proc/profile.
 */
int setup_profiling_timer(unsigned int multiplier)
{
	int i;

	/*
	 * Sanity check. [at least 500 APIC cycles should be
	 * between APIC interrupts as a rule of thumb, to avoid
	 * irqs flooding us]
	 */
	if ( (!multiplier) || (calibration_result/multiplier < 500))
		return -EINVAL;

	/* 
	 * Set the new multiplier for each CPU. CPUs don't start using the
	 * new values until the next timer interrupt in which they do process
	 * accounting. At that time they also adjust their APIC timers
	 * accordingly.
	 */
	for (i = 0; i < NR_CPUS; ++i)
		prof_multiplier[i] = multiplier;

	return 0;
}

#undef APIC_DIVISOR

/*
 * Local timer interrupt handler. It does both profiling and
 * process statistics/rescheduling.
 *
 * We do profiling in every local tick, statistics/rescheduling
 * happen only every 'profiling multiplier' ticks. The default
 * multiplier is 1 and it can be changed by writing the new multiplier
 * value into /proc/profile.
 */

inline void smp_local_timer_interrupt(struct pt_regs * regs)
{
	int user = user_mode(regs);
	int cpu = smp_processor_id();

	/*
	 * The profiling function is SMP safe. (nothing can mess
	 * around with "current", and the profiling counters are
	 * updated with atomic operations). This is especially
	 * useful with a profiling multiplier != 1
	 */
	if (!user)
		x86_do_profile(regs->eip);

	if (--prof_counter[cpu] <= 0) {
		/*
		 * The multiplier may have changed since the last time we got
		 * to this point as a result of the user writing to
		 * /proc/profile. In this case we need to adjust the APIC
		 * timer accordingly.
		 *
		 * Interrupts are already masked off at this point.
		 */
		prof_counter[cpu] = prof_multiplier[cpu];
		if (prof_counter[cpu] != prof_old_multiplier[cpu]) {
			__setup_APIC_LVTT(calibration_result/prof_counter[cpu]);
			prof_old_multiplier[cpu] = prof_counter[cpu];
		}

#ifdef CONFIG_SMP
		update_process_times(user);
#endif
	}

	/*
	 * We take the 'long' return path, and there every subsystem
	 * grabs the apropriate locks (kernel lock/ irq lock).
	 *
	 * we might want to decouple profiling from the 'long path',
	 * and do the profiling totally in assembly.
	 *
	 * Currently this isn't too much of an issue (performance wise),
	 * we can take more than 100K local irqs per second on a 100 MHz P5.
	 */
}

/*
 * Local APIC timer interrupt. This is the most natural way for doing
 * local interrupts, but local timer interrupts can be emulated by
 * broadcast interrupts too. [in case the hw doesn't support APIC timers]
 *
 * [ if a single-CPU system runs an SMP kernel then we call the local
 *   interrupt as well. Thus we cannot inline the local irq ... ]
 */
unsigned int apic_timer_irqs [NR_CPUS];

void smp_apic_timer_interrupt(struct pt_regs * regs)
{
	int cpu = smp_processor_id();

	/*
	 * the NMI deadlock-detector uses this.
	 */
	apic_timer_irqs[cpu]++;

	/*
	 * NOTE! We'd better ACK the irq immediately,
	 * because timer handling can be slow.
	 */
	ack_APIC_irq();
	/*
	 * update_process_times() expects us to have done irq_enter().
	 * Besides, if we don't timer interrupts ignore the global
	 * interrupt lock, which is the WrongThing (tm) to do.
	 */
	irq_enter(cpu, 0);
	smp_local_timer_interrupt(regs);
	irq_exit(cpu, 0);

	if (softirq_pending(cpu))
		do_softirq();
}

/*
 * This interrupt should _never_ happen with our APIC/SMP architecture
 */
asmlinkage void smp_spurious_interrupt(void)
{
	unsigned long v;

	/*
	 * Check if this really is a spurious interrupt and ACK it
	 * if it is a vectored one.  Just in case...
	 * Spurious interrupts should not be ACKed.
	 */
	v = apic_read(APIC_ISR + ((SPURIOUS_APIC_VECTOR & ~0x1f) >> 1));
	if (v & (1 << (SPURIOUS_APIC_VECTOR & 0x1f)))
		ack_APIC_irq();

	/* see sw-dev-man vol 3, chapter 7.4.13.5 */
	printk(KERN_INFO "spurious APIC interrupt on CPU#%d, should never happen.\n",
			smp_processor_id());
}

/*
 * This interrupt should never happen with our APIC/SMP architecture
 */

asmlinkage void smp_error_interrupt(void)
{
	unsigned long v, v1;

	/* First tickle the hardware, only then report what went on. -- REW */
	v = apic_read(APIC_ESR);
	apic_write(APIC_ESR, 0);
	v1 = apic_read(APIC_ESR);
	ack_APIC_irq();
	atomic_inc(&irq_err_count);

	/* Here is what the APIC error bits mean:
	   0: Send CS error
	   1: Receive CS error
	   2: Send accept error
	   3: Receive accept error
	   4: Reserved
	   5: Send illegal vector
	   6: Received illegal vector
	   7: Illegal register address
	*/
	printk (KERN_ERR "APIC error on CPU%d: %02lx(%02lx)\n",
	        smp_processor_id(), v , v1);
}

/*
 * This initializes the IO-APIC and APIC hardware if this is
 * a UP kernel.
 */
int __init APIC_init_uniprocessor (void)
{
	if (enable_local_apic < 0)
		clear_bit(X86_FEATURE_APIC, boot_cpu_data.x86_capability);

	if (!smp_found_config && !cpu_has_apic)
		return -1;

	/*
	 * Complain if the BIOS pretends there is one.
	 */
	if (!cpu_has_apic && APIC_INTEGRATED(apic_version[boot_cpu_physical_apicid])) {
		printk(KERN_ERR "BIOS bug, local APIC #%d not detected!...\n",
			boot_cpu_physical_apicid);
		return -1;
	}

	verify_local_APIC();

	connect_bsp_APIC();

	phys_cpu_present_map = 1 << boot_cpu_physical_apicid;

	apic_pm_init2();

	setup_local_APIC();

	if (nmi_watchdog == NMI_LOCAL_APIC)
		check_nmi_watchdog();
#ifdef CONFIG_X86_IO_APIC
	if (smp_found_config)
		if (!skip_ioapic_setup && nr_ioapics)
			setup_IO_APIC();
#endif
	setup_APIC_clocks();

	return 0;
}
