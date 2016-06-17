/*
 *	arch/mips64/kernel/cpu-probe.c
 *
 *	Processor capabilities determination functions.
 *
 *	Copyright (C) xxxx  the Anonymous
 *	Copyright (C) 2003  Maciej W. Rozycki
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/stddef.h>

#include <asm/bugs.h>
#include <asm/cpu.h>
#include <asm/fpu.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

/*
 * Not all of the MIPS CPUs have the "wait" instruction available. Moreover,
 * the implementation of the "wait" feature differs between CPU families. This
 * points to the function that implements CPU specific wait.
 * The wait instruction stops the pipeline and reduces the power consumption of
 * the CPU very much.
 */
void (*cpu_wait)(void) = NULL;

static void r3081_wait(void)
{
	unsigned long cfg = read_c0_conf();
	write_c0_conf(cfg | R30XX_CONF_HALT);
}

static void r39xx_wait(void)
{
	unsigned long cfg = read_c0_conf();
	write_c0_conf(cfg | TX39_CONF_HALT);
}

static void r4k_wait(void)
{
	__asm__(".set\tmips3\n\t"
		"wait\n\t"
		".set\tmips0");
}

void au1k_wait(void)
{
#ifdef CONFIG_PM
	/* using the wait instruction makes CP0 counter unusable */
	__asm__(".set\tmips3\n\t"
		"wait\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		"nop\n\t"
		".set\tmips0");
#else
	__asm__("nop\n\t"
		"nop");
#endif
}

static inline void check_wait(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;

	printk("Checking for 'wait' instruction... ");
	switch (c->cputype) {
	case CPU_R3081:
	case CPU_R3081E:
		cpu_wait = r3081_wait;
		printk(" available.\n");
		break;
	case CPU_TX3927:
		cpu_wait = r39xx_wait;
		printk(" available.\n");
		break;
	case CPU_R4200:
/*	case CPU_R4300: */
	case CPU_R4600:
	case CPU_R4640:
	case CPU_R4650:
	case CPU_R4700:
	case CPU_R5000:
	case CPU_NEVADA:
	case CPU_RM7000:
	case CPU_TX49XX:
	case CPU_4KC:
	case CPU_4KEC:
	case CPU_4KSC:
	case CPU_5KC:
/*	case CPU_20KC:*/
	case CPU_24K:
	case CPU_25KF:
		cpu_wait = r4k_wait;
		printk(" available.\n");
		break;
	case CPU_AU1000:
	case CPU_AU1100:
	case CPU_AU1500:
		cpu_wait = au1k_wait;
		printk(" available.\n");
		break;
	default:
		printk(" unavailable.\n");
		break;
	}
}

static inline void align_mod(const int align, const int mod)
{
	asm volatile(
		".set	push\n\t"
		".set	noreorder\n\t"
		".balign %0\n\t"
		".rept	%1\n\t"
		"nop\n\t"
		".endr\n\t"
		".set	pop"
		:
		: "n" (align), "n" (mod));
}

static inline void mult_sh_align_mod(long *v1, long *v2, long *w,
				     const int align, const int mod)
{
	unsigned long flags;
	int m1, m2;
	long p, s, lv1, lv2, lw;

	/*
	 * We want the multiply and the shift to be isolated from the
	 * rest of the code to disable gcc optimizations.  Hence the
	 * asm statements that execute nothing, but make gcc not know
	 * what the values of m1, m2 and s are and what lv2 and p are
	 * used for.
	 */

	local_irq_save(flags);
	/*
	 * The following code leads to a wrong result of the first
	 * dsll32 when executed on R4000 rev. 2.2 or 3.0 (PRId
	 * 00000422 or 00000430, respectively).
	 *
	 * See "MIPS R4000PC/SC Errata, Processor Revision 2.2 and
	 * 3.0" by MIPS Technologies, Inc., errata #16 and #28 for
	 * details.  I got no permission to duplicate them here,
	 * sigh... --macro
	 */
	asm volatile(
		""
		: "=r" (m1), "=r" (m2), "=r" (s)
		: "0" (5), "1" (8), "2" (5));
	align_mod(align, mod);
	/*
	 * The trailing nop is needed to fullfill the two-instruction
	 * requirement between reading hi/lo and staring a mult/div.
	 * Leaving it out may cause gas insert a nop itself breaking
	 * the desired alignment of the next chunk.
	 */
	asm volatile(
		".set	push\n\t"
		".set	noat\n\t"
		".set	noreorder\n\t"
		".set	nomacro\n\t"
		"mult	%2, %3\n\t"
		"dsll32	%0, %4, %5\n\t"
		"mflo	$0\n\t"
		"dsll32	%1, %4, %5\n\t"
		"nop\n\t"
		".set	pop"
		: "=&r" (lv1), "=r" (lw)
		: "r" (m1), "r" (m2), "r" (s), "I" (0)
		: "hi", "lo", "accum");
	/* We have to use single integers for m1 and m2 and a double
	 * one for p to be sure the mulsidi3 gcc's RTL multiplication
	 * instruction has the workaround applied.  Older versions of
	 * gcc have correct umulsi3 and mulsi3, but other
	 * multiplication variants lack the workaround.
	 */
	asm volatile(
		""
		: "=r" (m1), "=r" (m2), "=r" (s)
		: "0" (m1), "1" (m2), "2" (s));
	align_mod(align, mod);
	p = m1 * m2;
	lv2 = s << 32;
	asm volatile(
		""
		: "=r" (lv2)
		: "0" (lv2), "r" (p));
	local_irq_restore(flags);

	*v1 = lv1;
	*v2 = lv2;
	*w = lw;
}

static inline void check_mult_sh(void)
{
	long v1[8], v2[8], w[8];
	int bug, fix, i;

	printk("Checking for the multiply/shift bug... ");

	/*
	 * Testing discovered false negatives for certain code offsets
	 * into cache lines.  Hence we test all possible offsets for
	 * the worst assumption of an R4000 I-cache line width of 32
	 * bytes.
	 *
	 * We can't use a loop as alignment directives need to be
	 * immediates.
	 */
	mult_sh_align_mod(&v1[0], &v2[0], &w[0], 32, 0);
	mult_sh_align_mod(&v1[1], &v2[1], &w[1], 32, 1);
	mult_sh_align_mod(&v1[2], &v2[2], &w[2], 32, 2);
	mult_sh_align_mod(&v1[3], &v2[3], &w[3], 32, 3);
	mult_sh_align_mod(&v1[4], &v2[4], &w[4], 32, 4);
	mult_sh_align_mod(&v1[5], &v2[5], &w[5], 32, 5);
	mult_sh_align_mod(&v1[6], &v2[6], &w[6], 32, 6);
	mult_sh_align_mod(&v1[7], &v2[7], &w[7], 32, 7);

	bug = 0;
	for (i = 0; i < 8; i++)
		if (v1[i] != w[i])
			bug = 1;
		
	if (bug == 0) {
		printk("no.\n");
		return;
	}

	printk("yes, workaround... ");

	fix = 1;
	for (i = 0; i < 8; i++)
		if (v2[i] != w[i])
			fix = 0;
		
	if (fix == 1) {
		printk("yes.\n");
		return;
	}

	printk("no.\n");
	panic("Reliable operation impossible!\n"
#ifndef CONFIG_CPU_R4000
	      "Configure for R4000 to enable the workaround."
#else
	      "Please report to <linux-mips@linux-mips.org>."
#endif
	      );
}

static volatile int daddi_ov __initdata = 0;

asmlinkage void __init do_daddi_ov(struct pt_regs *regs)
{
	daddi_ov = 1;
	regs->cp0_epc += 4;
}

static inline void check_daddi(void)
{
	extern asmlinkage void handle_daddi_ov(void);
	unsigned long flags;
	void *handler;
	long v;

	printk("Checking for the daddi bug... ");

	local_irq_save(flags);
	handler = set_except_vector(12, handle_daddi_ov);
	/*
	 * The following code fails to trigger an overflow exception
	 * when executed on R4000 rev. 2.2 or 3.0 (PRId 00000422 or
	 * 00000430, respectively).
	 *
	 * See "MIPS R4000PC/SC Errata, Processor Revision 2.2 and
	 * 3.0" by MIPS Technologies, Inc., erratum #23 for details.
	 * I got no permission to duplicate it here, sigh... --macro
	 */
	asm volatile(
		".set	push\n\t"
		".set	noat\n\t"
		".set	noreorder\n\t"
		".set	nomacro\n\t"
#ifdef HAVE_AS_SET_DADDI
		".set	daddi\n\t"
#endif
		"daddi	%0, %1, %2\n\t"
		".set	pop"
		: "=r" (v)
		: "r" (0x7fffffffffffedcd), "I" (0x1234));
	set_except_vector(12, handler);
	local_irq_restore(flags);

	if (daddi_ov) {
		printk("no.\n");
		return;
	}

	printk("yes, workaround... ");

	local_irq_save(flags);
	handler = set_except_vector(12, handle_daddi_ov);
	asm volatile(
		"daddi	%0, %1, %2"
		: "=r" (v)
		: "r" (0x7fffffffffffedcd), "I" (0x1234));
	set_except_vector(12, handler);
	local_irq_restore(flags);

	if (daddi_ov) {
		printk("yes.\n");
		return;
	}

	printk("no.\n");
	panic("Reliable operation impossible!\n"
#if !defined(CONFIG_CPU_R4000) && !defined(CONFIG_CPU_R4400)
	      "Configure for R4000 or R4400 to enable the workaround."
#else
	      "Please report to <linux-mips@linux-mips.org>."
#endif
	      );
}

static inline void check_daddiu(void)
{
	long v, w;

	printk("Checking for the daddiu bug... ");

	/*
	 * The following code leads to a wrong result of daddiu when
	 * executed on R4400 rev. 1.0 (PRId 00000440).
	 *
	 * See "MIPS R4400PC/SC Errata, Processor Revision 1.0" by
	 * MIPS Technologies, Inc., erratum #7 for details.
	 *
	 * According to "MIPS R4000PC/SC Errata, Processor Revision
	 * 2.2 and 3.0" by MIPS Technologies, Inc., erratum #41 this
	 * problem affects R4000 rev. 2.2 and 3.0 (PRId 00000422 and
	 * 00000430, respectively), too.  Testing failed to trigger it
	 * so far.
	 *
	 * I got no permission to duplicate the errata here, sigh...
	 * --macro
	 */
	asm volatile(
		".set	push\n\t"
		".set	noat\n\t"
		".set	noreorder\n\t"
		".set	nomacro\n\t"
#ifdef HAVE_AS_SET_DADDI
		".set	daddi\n\t"
#endif
		"daddiu	%0, %2, %3\n\t"
		"addiu	%1, $0, %3\n\t"
		"daddu	%1, %2\n\t"
		".set	pop"
		: "=&r" (v), "=&r" (w)
		: "r" (0x7fffffffffffedcd), "I" (0x1234));

	if (v == w) {
		printk("no.\n");
		return;
	}

	printk("yes, workaround... ");

	asm volatile(
		"daddiu	%0, %2, %3\n\t"
		"addiu	%1, $0, %3\n\t"
		"daddu	%1, %2"
		: "=&r" (v), "=&r" (w)
		: "r" (0x7fffffffffffedcd), "I" (0x1234));

	if (v == w) {
		printk("yes.\n");
		return;
	}

	printk("no.\n");
	panic("Reliable operation impossible!\n"
#if !defined(CONFIG_CPU_R4000) && !defined(CONFIG_CPU_R4400)
	      "Configure for R4000 or R4400 to enable the workaround."
#else
	      "Please report to <linux-mips@linux-mips.org>."
#endif
	      );
}

void __init check_bugs(void)
{
	check_wait();
	check_mult_sh();
	check_daddi();
	check_daddiu();
}

/*
 * Probe whether cpu has config register by trying to play with
 * alternate cache bit and see whether it matters.
 * It's used by cpu_probe to distinguish between R3000A and R3081.
 */
static inline int cpu_has_confreg(void)
{
#ifdef CONFIG_CPU_R3000
	extern unsigned long r3k_cache_size(unsigned long);
	unsigned long size1, size2;
	unsigned long cfg = read_c0_conf();

	size1 = r3k_cache_size(ST0_ISC);
	write_c0_conf(cfg ^ R30XX_CONF_AC);
	size2 = r3k_cache_size(ST0_ISC);
	write_c0_conf(cfg);
	return size1 != size2;
#else
	return 0;
#endif
}

/*
 * Get the FPU Implementation/Revision.
 */
static inline unsigned long cpu_get_fpu_id(void)
{
	unsigned long tmp, fpu_id;

	tmp = read_c0_status();
	__enable_fpu();
	fpu_id = read_32bit_cp1_register(CP1_REVISION);
	write_c0_status(tmp);
	return fpu_id;
}

/*
 * Check the CPU has an FPU the official way.
 */
static inline int __cpu_has_fpu(void)
{
	return ((cpu_get_fpu_id() & 0xff00) != FPIR_IMP_NONE);
}

#define R4K_OPTS (MIPS_CPU_TLB | MIPS_CPU_4KEX | MIPS_CPU_4KTLB \
		| MIPS_CPU_COUNTER)

static inline void cpu_probe_legacy(struct cpuinfo_mips *c)
{
	switch (c->processor_id & 0xff00) {
	case PRID_IMP_R2000:
		c->cputype = CPU_R2000;
		c->isa_level = MIPS_CPU_ISA_I;
		c->options = MIPS_CPU_TLB | MIPS_CPU_NOFPUEX |
		             MIPS_CPU_LLSC;
		if (__cpu_has_fpu())
			c->options |= MIPS_CPU_FPU;
		c->tlbsize = 64;
		break;
	case PRID_IMP_R3000:
		if ((c->processor_id & 0xff) == PRID_REV_R3000A)
			if (cpu_has_confreg())
				c->cputype = CPU_R3081E;
			else
				c->cputype = CPU_R3000A;
		else
			c->cputype = CPU_R3000;
		c->isa_level = MIPS_CPU_ISA_I;
		c->options = MIPS_CPU_TLB | MIPS_CPU_NOFPUEX |
		             MIPS_CPU_LLSC;
		if (__cpu_has_fpu())
			c->options |= MIPS_CPU_FPU;
		c->tlbsize = 64;
		break;
	case PRID_IMP_R4000:
		if ((c->processor_id & 0xff) >= PRID_REV_R4400)
			c->cputype = CPU_R4400SC;
		else
			c->cputype = CPU_R4000SC;
		c->isa_level = MIPS_CPU_ISA_III;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_WATCH | MIPS_CPU_VCE |
		             MIPS_CPU_LLSC;
		c->tlbsize = 48;
		break;
	case PRID_IMP_VR41XX:
		switch (c->processor_id & 0xf0) {
#ifndef CONFIG_VR4181
		case PRID_REV_VR4111:
			c->cputype = CPU_VR4111;
			break;
#else
		case PRID_REV_VR4181:
			c->cputype = CPU_VR4181;
			break;
#endif
		case PRID_REV_VR4121:
			c->cputype = CPU_VR4121;
			break;
		case PRID_REV_VR4122:
			if ((c->processor_id & 0xf) < 0x3)
				c->cputype = CPU_VR4122;
			else
				c->cputype = CPU_VR4181A;
			break;
		case PRID_REV_VR4130:
			if ((c->processor_id & 0xf) < 0x4)
				c->cputype = CPU_VR4131;
			else
				c->cputype = CPU_VR4133;
			break;
		default:
			printk(KERN_INFO "Unexpected CPU of NEC VR4100 series\n");
			c->cputype = CPU_VR41XX;
			break;
		}
		c->isa_level = MIPS_CPU_ISA_III;
		c->options = R4K_OPTS;
		c->tlbsize = 32;
		break;
	case PRID_IMP_R4300:
		c->cputype = CPU_R4300;
		c->isa_level = MIPS_CPU_ISA_III;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_LLSC;
		c->tlbsize = 32;
		break;
	case PRID_IMP_R4600:
		c->cputype = CPU_R4600;
		c->isa_level = MIPS_CPU_ISA_III;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_LLSC;
		c->tlbsize = 48;
		break;
	#if 0
 	case PRID_IMP_R4650:
		/*
		 * This processor doesn't have an MMU, so it's not
		 * "real easy" to run Linux on it. It is left purely
		 * for documentation.  Commented out because it shares
		 * it's c0_prid id number with the TX3900.
		 */
 		c->cputype = CPU_R4650;
	 	c->isa_level = MIPS_CPU_ISA_III;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_LLSC;
	        c->tlbsize = 48;
		break;
	#endif
	case PRID_IMP_TX39:
		c->isa_level = MIPS_CPU_ISA_I;
		c->options = MIPS_CPU_TLB;

		if ((c->processor_id & 0xf0) == (PRID_REV_TX3927 & 0xf0)) {
			c->cputype = CPU_TX3927;
			c->tlbsize = 64;
		} else {
			switch (c->processor_id & 0xff) {
			case PRID_REV_TX3912:
				c->cputype = CPU_TX3912;
				c->tlbsize = 32;
				break;
			case PRID_REV_TX3922:
				c->cputype = CPU_TX3922;
				c->tlbsize = 64;
				break;
			default:
				c->cputype = CPU_UNKNOWN;
				break;
			}
		}
		break;
	case PRID_IMP_R4700:
		c->cputype = CPU_R4700;
		c->isa_level = MIPS_CPU_ISA_III;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_LLSC;
		c->tlbsize = 48;
		break;
	case PRID_IMP_TX49:
		c->cputype = CPU_TX49XX;
		c->isa_level = MIPS_CPU_ISA_III;
		c->options = R4K_OPTS | MIPS_CPU_LLSC;
		if (!(c->processor_id & 0x08))
			c->options |= MIPS_CPU_FPU | MIPS_CPU_32FPR;
		c->tlbsize = 48;
		break;
	case PRID_IMP_R5000:
		c->cputype = CPU_R5000;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_LLSC;
		c->tlbsize = 48;
		break;
	case PRID_IMP_R5432:
		c->cputype = CPU_R5432;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_WATCH | MIPS_CPU_LLSC;
		c->tlbsize = 48;
		break;
	case PRID_IMP_R5500:
		c->cputype = CPU_R5500;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_WATCH | MIPS_CPU_LLSC;
		c->tlbsize = 48;
		break;
	case PRID_IMP_NEVADA:
		c->cputype = CPU_NEVADA;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_DIVEC | MIPS_CPU_LLSC;
		c->tlbsize = 48;
		break;
	case PRID_IMP_R6000:
		c->cputype = CPU_R6000;
		c->isa_level = MIPS_CPU_ISA_II;
		c->options = MIPS_CPU_TLB | MIPS_CPU_FPU |
		             MIPS_CPU_LLSC;
		c->tlbsize = 32;
		break;
	case PRID_IMP_R6000A:
		c->cputype = CPU_R6000A;
		c->isa_level = MIPS_CPU_ISA_II;
		c->options = MIPS_CPU_TLB | MIPS_CPU_FPU |
		             MIPS_CPU_LLSC;
		c->tlbsize = 32;
		break;
	case PRID_IMP_RM7000:
		c->cputype = CPU_RM7000;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_LLSC;
		/*
		 * Undocumented RM7000:  Bit 29 in the info register of
		 * the RM7000 v2.0 indicates if the TLB has 48 or 64
		 * entries.
		 *
		 * 29      1 =>    64 entry JTLB
		 *         0 =>    48 entry JTLB
		 */
		c->tlbsize = (read_c0_info() & (1 << 29)) ? 64 : 48;
		break;
	case PRID_IMP_RM9000:
		c->cputype = CPU_RM9000;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = R4K_OPTS | MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_LLSC;
		/*
		 * Bit 29 in the info register of the RM9000
		 * indicates if the TLB has 48 or 64 entries.
		 *
		 * 29      1 =>    64 entry JTLB
		 *         0 =>    48 entry JTLB
		 */
		c->tlbsize = (read_c0_info() & (1 << 29)) ? 64 : 48;
		break;
	case PRID_IMP_R8000:
		c->cputype = CPU_R8000;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = MIPS_CPU_TLB | MIPS_CPU_4KEX |
		             MIPS_CPU_FPU | MIPS_CPU_32FPR |
		             MIPS_CPU_LLSC;
		c->tlbsize = 384;      /* has weird TLB: 3-way x 128 */
		break;
	case PRID_IMP_R10000:
		c->cputype = CPU_R10000;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = MIPS_CPU_TLB | MIPS_CPU_4KEX |
		             MIPS_CPU_FPU | MIPS_CPU_32FPR |
			     MIPS_CPU_COUNTER | MIPS_CPU_WATCH |
		             MIPS_CPU_LLSC;
		c->tlbsize = 64;
		break;
	case PRID_IMP_R12000:
		c->cputype = CPU_R12000;
		c->isa_level = MIPS_CPU_ISA_IV;
		c->options = MIPS_CPU_TLB | MIPS_CPU_4KEX |
		             MIPS_CPU_FPU | MIPS_CPU_32FPR |
			     MIPS_CPU_COUNTER | MIPS_CPU_WATCH |
		             MIPS_CPU_LLSC;
		c->tlbsize = 64;
		break;
	default:
		c->cputype = CPU_UNKNOWN;
		break;
	}
}

static inline void decode_config1(struct cpuinfo_mips *c)
{
	unsigned long config0 = read_c0_config();
	unsigned long config1;

	if ((config0 & (1 << 31)) == 0)
		return;			/* actually wort a panic() */

	/* MIPS32 or MIPS64 compliant CPU. Read Config 1 register. */
	c->options = MIPS_CPU_TLB | MIPS_CPU_4KEX |
		MIPS_CPU_4KTLB | MIPS_CPU_COUNTER | MIPS_CPU_DIVEC |
		MIPS_CPU_LLSC;
	config1 = read_c0_config1();
	if (config1 & (1 << 3))
		c->options |= MIPS_CPU_WATCH;
	if (config1 & (1 << 2))
		c->options |= MIPS_CPU_MIPS16;
	if (config1 & (1 << 1))
		c->options |= MIPS_CPU_EJTAG;
	if (config1 & 1) {
		c->options |= MIPS_CPU_FPU;
		c->options |= MIPS_CPU_32FPR;
	}
	c->scache.flags = MIPS_CACHE_NOT_PRESENT;

	c->tlbsize = ((config1 >> 25) & 0x3f) + 1;
}

static inline void cpu_probe_mips(struct cpuinfo_mips *c)
{
	decode_config1(c);
	switch (c->processor_id & 0xff00) {
	case PRID_IMP_4KC:
		c->cputype = CPU_4KC;
		c->isa_level = MIPS_CPU_ISA_M32;
		break;
	case PRID_IMP_4KEC:
		c->cputype = CPU_4KEC;
		c->isa_level = MIPS_CPU_ISA_M32;
		break;
	case PRID_IMP_4KSC:
		c->cputype = CPU_4KSC;
		c->isa_level = MIPS_CPU_ISA_M32;
		break;
	case PRID_IMP_5KC:
		c->cputype = CPU_5KC;
		c->isa_level = MIPS_CPU_ISA_M64;
		break;
	case PRID_IMP_20KC:
		c->cputype = CPU_20KC;
		c->isa_level = MIPS_CPU_ISA_M64;
		break;
	case PRID_IMP_24K:
		c->cputype = CPU_24K;
		c->isa_level = MIPS_CPU_ISA_M32;
		break;
	case PRID_IMP_25KF:
		c->cputype = CPU_25KF;
		c->isa_level = MIPS_CPU_ISA_M64;
		/* Probe for L2 cache */
		c->scache.flags &= ~MIPS_CACHE_NOT_PRESENT;
		break;
	default:
		c->cputype = CPU_UNKNOWN;
		break;
	}
}

static inline void cpu_probe_alchemy(struct cpuinfo_mips *c)
{
	decode_config1(c);
	switch (c->processor_id & 0xff00) {
	case PRID_IMP_AU1_REV1:
	case PRID_IMP_AU1_REV2:
		switch ((c->processor_id >> 24) & 0xff) {
		case 0:
 			c->cputype = CPU_AU1000;
			break;
		case 1:
			c->cputype = CPU_AU1500;
			break;
		case 2:
			c->cputype = CPU_AU1100;
			break;
		default:
			panic("Unknown Au Core!");
			break;
		}
		c->isa_level = MIPS_CPU_ISA_M32;
		break;
	}
}

static inline void cpu_probe_sibyte(struct cpuinfo_mips *c)
{
	decode_config1(c);
	switch (c->processor_id & 0xff00) {
	case PRID_IMP_SB1:
		c->cputype = CPU_SB1;
		c->isa_level = MIPS_CPU_ISA_M64;
		c->options = MIPS_CPU_TLB | MIPS_CPU_4KEX |
		             MIPS_CPU_COUNTER | MIPS_CPU_DIVEC |
		             MIPS_CPU_MCHECK | MIPS_CPU_EJTAG |
		             MIPS_CPU_WATCH | MIPS_CPU_LLSC;
#ifndef CONFIG_SB1_PASS_1_WORKAROUNDS
		/* FPU in pass1 is known to have issues. */
		c->options |= MIPS_CPU_FPU | MIPS_CPU_32FPR;
#endif
		break;
	default:
		c->cputype = CPU_UNKNOWN;
		break;
	}
}

static inline void cpu_probe_sandcraft(struct cpuinfo_mips *c)
{
	decode_config1(c);
	switch (c->processor_id & 0xff00) {
	case PRID_IMP_SR71000:
		c->cputype = CPU_SR71000;
		c->isa_level = MIPS_CPU_ISA_M64;
		c->options = MIPS_CPU_TLB | MIPS_CPU_4KEX |
		             MIPS_CPU_4KTLB | MIPS_CPU_FPU |
		             MIPS_CPU_COUNTER | MIPS_CPU_MCHECK;
		c->scache.ways = 8;
		c->tlbsize = 64;
		break;
	default:
		c->cputype = CPU_UNKNOWN;
		break;
	}
}

__init void cpu_probe(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;

	c->processor_id	= PRID_IMP_UNKNOWN;
	c->fpu_id	= FPIR_IMP_NONE;
	c->cputype	= CPU_UNKNOWN;

	c->processor_id = read_c0_prid();
	switch (c->processor_id & 0xff0000) {
	case PRID_COMP_LEGACY:
		cpu_probe_legacy(c);
		break;
	case PRID_COMP_MIPS:
		cpu_probe_mips(c);
		break;
	case PRID_COMP_ALCHEMY:
		cpu_probe_alchemy(c);
		break;
	case PRID_COMP_SIBYTE:
		cpu_probe_sibyte(c);
		break;

	case PRID_COMP_SANDCRAFT:
		cpu_probe_sandcraft(c);
		break;
	default:
		c->cputype = CPU_UNKNOWN;
	}
	if (c->options & MIPS_CPU_FPU)
		c->fpu_id = cpu_get_fpu_id();
}

__init void cpu_report(void)
{
	struct cpuinfo_mips *c = &current_cpu_data;

	printk("CPU revision is: %08x\n", c->processor_id);
	if (c->options & MIPS_CPU_FPU)
		printk("FPU revision is: %08x\n", c->fpu_id);
}
