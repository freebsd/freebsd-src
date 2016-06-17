#ifndef _ASM_HW_IRQ_H
#define _ASM_HW_IRQ_H

#ifndef __ASSEMBLY__
/*
 *	linux/include/asm/hw_irq.h
 *
 *	(C) 1992, 1993 Linus Torvalds, (C) 1997 Ingo Molnar
 *
 *	moved some of the old arch/i386/kernel/irq.h to here. VY
 *
 *	IRQ/IPI changes taken from work by Thomas Radke
 *	<tomsoft@informatik.tu-chemnitz.de>
 *
 *	hacked by Andi Kleen for x86-64.
 * 
 *  $Id: hw_irq.h,v 1.31 2003/02/18 18:35:55 ak Exp $
 */

#include <linux/config.h>
#include <linux/stddef.h>
#include <asm/atomic.h>
#include <asm/irq.h>

#endif

/*
 * IDT vectors usable for external interrupt sources start
 * at 0x20:
 */
#define FIRST_EXTERNAL_VECTOR	0x20

#define IA32_SYSCALL_VECTOR	0x80


/*
 * Vectors 0x20-0x2f are used for ISA interrupts.
 */

/*
 * Special IRQ vectors used by the SMP architecture, 0xf0-0xff
 *
 *  some of the following vectors are 'rare', they are merged
 *  into a single vector (CALL_FUNCTION_VECTOR) to save vector space.
 *  TLB, reschedule and local APIC vectors are performance-critical.
 *
 *  Vectors 0xf0-0xf9 are free (reserved for future Linux use).
 */
#define SPURIOUS_APIC_VECTOR	0xff
#define ERROR_APIC_VECTOR	0xfe
#define INVALIDATE_TLB_VECTOR	0xfd
#define RESCHEDULE_VECTOR	0xfc
/* 0xfa free */
#define CALL_FUNCTION_VECTOR	0xfb

/*
 * Local APIC timer IRQ vector is on a different priority level,
 * to work around the 'lost local interrupt if more than 2 IRQ
 * sources per level' errata.
 */
#define LOCAL_TIMER_VECTOR	0xef

/*
 * First APIC vector available to drivers: (vectors 0x30-0xee)
 * we start at 0x31 to spread out vectors evenly between priority
 * levels. (0x80 is the syscall vector)
 */
#define FIRST_DEVICE_VECTOR	0x31
#define FIRST_SYSTEM_VECTOR	0xef

#ifndef __ASSEMBLY__
extern int irq_vector[NR_IRQS];
#define IO_APIC_VECTOR(irq)	irq_vector[irq]

/*
 * Various low-level irq details needed by irq.c, process.c,
 * time.c, io_apic.c and smp.c
 *
 * Interrupt entry/exit code at both C and assembly level
 */

extern void mask_irq(unsigned int irq);
extern void unmask_irq(unsigned int irq);
extern void disable_8259A_irq(unsigned int irq);
extern void enable_8259A_irq(unsigned int irq);
extern int i8259A_irq_pending(unsigned int irq);
extern void make_8259A_irq(unsigned int irq);
extern void init_8259A(int aeoi);
extern void FASTCALL(send_IPI_self(int vector));
extern void init_VISWS_APIC_irqs(void);
extern void setup_IO_APIC(void);
extern void disable_IO_APIC(void);
extern void print_IO_APIC(void);
extern int IO_APIC_get_PCI_irq_vector(int bus, int slot, int fn);
extern void send_IPI(int dest, int vector);

extern unsigned long io_apic_irqs;

extern atomic_t irq_err_count;
extern atomic_t irq_mis_count;

extern char _stext, _etext;

#define IO_APIC_IRQ(x) (((x) >= 16) || ((1<<(x)) & io_apic_irqs))

#include <asm/ptrace.h>

extern void reschedule_interrupt(void);
extern void invalidate_interrupt(void);
extern void call_function_interrupt(void);
extern void apic_timer_interrupt(void);
extern void spurious_interrupt(void);
extern void error_interrupt(void);

#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)

#define BUILD_IRQ(nr) \
asmlinkage void IRQ_NAME(nr); \
__asm__( \
"\n"__ALIGN_STR "\n" \
SYMBOL_NAME_STR(IRQ) #nr "_interrupt:\n\t" \
	"push $" #nr "-256 ; " \
	"jmp common_interrupt");

extern unsigned long prof_cpu_mask;
extern unsigned int * prof_buffer;
extern unsigned long prof_len;
extern unsigned long prof_shift;

/*
 * x86 profiling function, SMP safe. We might want to do this in
 * assembly totally?
 */
static inline void x86_do_profile (unsigned long eip)
{
	if (!prof_buffer)
		return;

	/*
	 * Only measure the CPUs specified by /proc/irq/prof_cpu_mask.
	 * (default is all CPUs.)
	 */
	if (!((1<<smp_processor_id()) & prof_cpu_mask))
		return;

	eip -= (unsigned long) &_stext;
	eip >>= prof_shift;
	/*
	 * Don't ignore out-of-bounds EIP values silently,
	 * put them into the last histogram slot, so if
	 * present, they will show up as a sharp peak.
	 */
	if (eip > prof_len-1)
		eip = prof_len-1;
	atomic_inc((atomic_t *)&prof_buffer[eip]);
}

#ifdef CONFIG_SMP /*more of this file should probably be ifdefed SMP */
static inline void hw_resend_irq(struct hw_interrupt_type *h, unsigned int i) {
	if (IO_APIC_IRQ(i))
		send_IPI_self(IO_APIC_VECTOR(i));
}
#else
static inline void hw_resend_irq(struct hw_interrupt_type *h, unsigned int i) {}
#endif

#endif

#endif /* _ASM_HW_IRQ_H */
