/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_APICVAR_H_
#define _MACHINE_APICVAR_H_

/*
 * Local && I/O APIC variable definitions.
 */

/*
 * Layout of local APIC interrupt vectors:
 *
 *	0xff (255)  +-------------+
 *                  |             | 15 (Spurious / IPIs / Local Interrupts)
 *	0xf0 (240)  +-------------+
 *                  |             | 14 (I/O Interrupts)
 *	0xe0 (224)  +-------------+
 *                  |             | 13 (I/O Interrupts)
 *	0xd0 (208)  +-------------+
 *                  |             | 12 (I/O Interrupts)
 *	0xc0 (192)  +-------------+
 *                  |             | 11 (I/O Interrupts)
 *	0xb0 (176)  +-------------+
 *                  |             | 10 (I/O Interrupts)
 *	0xa0 (160)  +-------------+
 *                  |             | 9 (I/O Interrupts)
 *	0x90 (144)  +-------------+
 *                  |             | 8 (I/O Interrupts / System Calls)
 *	0x80 (128)  +-------------+
 *                  |             | 7 (I/O Interrupts)
 *	0x70 (112)  +-------------+
 *                  |             | 6 (I/O Interrupts)
 *	0x60 (96)   +-------------+
 *                  |             | 5 (I/O Interrupts)
 *	0x50 (80)   +-------------+
 *                  |             | 4 (I/O Interrupts)
 *	0x40 (64)   +-------------+
 *                  |             | 3 (I/O Interrupts)
 *	0x30 (48)   +-------------+
 *                  |             | 2 (ATPIC Interrupts)
 *	0x20 (32)   +-------------+
 *                  |             | 1 (Exceptions, traps, faults, etc.)
 *	0x10 (16)   +-------------+
 *                  |             | 0 (Exceptions, traps, faults, etc.)
 *	0x00 (0)    +-------------+
 *
 * Note: 0x80 needs to be handled specially and not allocated to an
 * I/O device!
 */

#define	APIC_ID_ALL	0xff
#define	APIC_IO_INTS	(IDT_IO_INTS + 16)
#define	APIC_NUM_IOINTS	192

#define	APIC_LOCAL_INTS	240
#define	APIC_TIMER_INT	APIC_LOCAL_INTS
#define	APIC_ERROR_INT	(APIC_LOCAL_INTS + 1)
#define	APIC_THERMAL_INT (APIC_LOCAL_INTS + 2)

#define	APIC_IPI_INTS	(APIC_LOCAL_INTS + 3)
#define	IPI_AST		APIC_IPI_INTS		/* Generate software trap. */
#define	IPI_INVLTLB	(APIC_IPI_INTS + 1)	/* TLB Shootdown IPIs */
#define	IPI_INVLPG	(APIC_IPI_INTS + 2)
#define	IPI_INVLRNG	(APIC_IPI_INTS + 3)
#define	IPI_LAZYPMAP	(APIC_IPI_INTS + 4)	/* Lazy pmap release. */
#define	IPI_HARDCLOCK	(APIC_IPI_INTS + 8)	/* Inter-CPU clock handling. */
#define	IPI_STATCLOCK	(APIC_IPI_INTS + 9)
#define	IPI_RENDEZVOUS	(APIC_IPI_INTS + 10)	/* Inter-CPU rendezvous. */
#define	IPI_STOP	(APIC_IPI_INTS + 11)	/* Stop CPU until restarted. */

#define	APIC_SPURIOUS_INT 255

#define	LVT_LINT0	0
#define	LVT_LINT1	1
#define	LVT_TIMER	2
#define	LVT_ERROR	3
#define	LVT_PMC		4
#define	LVT_THERMAL	5
#define	LVT_MAX		LVT_THERMAL

#ifndef LOCORE

#define	APIC_IPI_DEST_SELF	-1
#define	APIC_IPI_DEST_ALL	-2
#define	APIC_IPI_DEST_OTHERS	-3

/*
 * An APIC enumerator is a psuedo bus driver that enumerates APIC's including
 * CPU's and I/O APIC's.
 */
struct apic_enumerator {
	const char *apic_name;
	int (*apic_probe)(void);
	int (*apic_probe_cpus)(void);
	int (*apic_setup_local)(void);
	int (*apic_setup_io)(void);
	SLIST_ENTRY(apic_enumerator) apic_next;
};

inthand_t
	IDTVEC(apic_isr1), IDTVEC(apic_isr2), IDTVEC(apic_isr3),
	IDTVEC(apic_isr4), IDTVEC(apic_isr5), IDTVEC(apic_isr6),
	IDTVEC(apic_isr7), IDTVEC(spuriousint);

u_int	apic_irq_to_idt(u_int irq);
u_int	apic_idt_to_irq(u_int vector);
void	apic_register_enumerator(struct apic_enumerator *enumerator);
void	*ioapic_create(uintptr_t addr, int32_t id, int intbase);
int	ioapic_disable_pin(void *cookie, u_int pin);
int	ioapic_get_vector(void *cookie, u_int pin);
int	ioapic_next_logical_cluster(void);
void	ioapic_register(void *cookie);
int	ioapic_remap_vector(void *cookie, u_int pin, int vector);
int	ioapic_set_extint(void *cookie, u_int pin);
int	ioapic_set_nmi(void *cookie, u_int pin);
int	ioapic_set_polarity(void *cookie, u_int pin, char activehi);
int	ioapic_set_triggermode(void *cookie, u_int pin, char edgetrigger);
int	ioapic_set_smi(void *cookie, u_int pin);
void	lapic_create(u_int apic_id, int boot_cpu);
void	lapic_disable(void);
void	lapic_dump(const char *str);
void	lapic_enable_intr(u_int vector);
void	lapic_eoi(void);
int	lapic_id(void);
void	lapic_init(uintptr_t addr);
int	lapic_intr_pending(u_int vector);
void	lapic_ipi_raw(register_t icrlo, u_int dest);
void	lapic_ipi_vectored(u_int vector, int dest);
int	lapic_ipi_wait(int delay);
void	lapic_handle_intr(struct intrframe frame);
void	lapic_set_logical_id(u_int apic_id, u_int cluster, u_int cluster_id);
int	lapic_set_lvt_mask(u_int apic_id, u_int lvt, u_char masked);
int	lapic_set_lvt_mode(u_int apic_id, u_int lvt, u_int32_t mode);
int	lapic_set_lvt_polarity(u_int apic_id, u_int lvt, u_char activehi);
int	lapic_set_lvt_triggermode(u_int apic_id, u_int lvt, u_char edgetrigger);
void	lapic_setup(void);

#endif /* !LOCORE */
#endif /* _MACHINE_APICVAR_H_ */
