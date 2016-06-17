/*
 *  linux/arch/arm/kernel/entry-armv.S
 *
 *  Copyright (C) 1996,1997,1998 Russell King.
 *  ARM700 fix by Matthew Godbolt (linux-user@willothewisp.demon.co.uk)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Low-level vector interface routines
 *
 *  Note:  there is a StrongARM bug in the STMIA rn, {regs}^ instruction that causes
 *  it to save wrong values...  Be aware!
 */
#include <linux/config.h>
#include "entry-header.S"


#ifdef IOC_BASE
/* IOC / IOMD based hardware */
#include <asm/hardware/iomd.h>

		.equ	ioc_base_high, IOC_BASE & 0xff000000
		.equ	ioc_base_low, IOC_BASE & 0x00ff0000
		.macro	disable_fiq
		mov	r12, #ioc_base_high
		.if	ioc_base_low
		orr	r12, r12, #ioc_base_low
		.endif
		strb	r12, [r12, #0x38]	@ Disable FIQ register
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov	r4, #ioc_base_high		@ point at IOC
		.if	ioc_base_low
		orr	r4, r4, #ioc_base_low
		.endif
		ldrb	\irqstat, [r4, #IOMD_IRQREQB]	@ get high priority first
		ldr	\base, =irq_prio_h
		teq	\irqstat, #0
#ifdef IOMD_BASE
		ldreqb	\irqstat, [r4, #IOMD_DMAREQ]	@ get dma
		addeq	\base, \base, #256		@ irq_prio_h table size
		teqeq	\irqstat, #0
		bne	2406f
#endif
		ldreqb	\irqstat, [r4, #IOMD_IRQREQA]	@ get low priority
		addeq	\base, \base, #256		@ irq_prio_d table size
		teqeq	\irqstat, #0
#ifdef IOMD_IRQREQC
		ldreqb	\irqstat, [r4, #IOMD_IRQREQC]
		addeq	\base, \base, #256		@ irq_prio_l table size
		teqeq	\irqstat, #0
#endif
#ifdef IOMD_IRQREQD
		ldreqb	\irqstat, [r4, #IOMD_IRQREQD]
		addeq	\base, \base, #256		@ irq_prio_lc table size
		teqeq	\irqstat, #0
#endif
2406:		ldrneb	\irqnr, [\base, \irqstat]	@ get IRQ number
		.endm

/*
 * Interrupt table (incorporates priority).  Please note that we
 * rely on the order of these tables (see above code).
 */
		.macro	irq_prio_table
irq_prio_h:	.byte	 0, 8, 9, 8,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	12, 8, 9, 8,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	14,14,14,14,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	14,14,14,14,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	15,15,15,15,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
		.byte	13,13,13,13,10,10,10,10,11,11,11,11,10,10,10,10
#ifdef IOMD_BASE
irq_prio_d:	.byte	 0,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	20,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	22,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	22,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	23,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	23,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	22,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	22,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
		.byte	21,16,17,16,18,16,17,16,19,16,17,16,18,16,17,16
#endif
irq_prio_l:	.byte	 0, 0, 1, 0, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 4, 0, 1, 0, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 6, 6, 6, 6, 6, 6, 6, 6, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 6, 6, 6, 6, 6, 6, 6, 6, 3, 3, 3, 3, 3, 3, 3, 3
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
		.byte	 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
#ifdef IOMD_IRQREQC
irq_prio_lc:	.byte	24,24,25,24,26,26,26,26,27,27,27,27,27,27,27,27
		.byte	28,24,25,24,26,26,26,26,27,27,27,27,27,27,27,27
		.byte	29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29
		.byte	29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29
		.byte	30,30,30,30,30,30,30,30,27,27,27,27,27,27,27,27
		.byte	30,30,30,30,30,30,30,30,27,27,27,27,27,27,27,27
		.byte	29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29
		.byte	29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
		.byte	31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31
#endif
#ifdef IOMD_IRQREQD
irq_prio_ld:	.byte	40,40,41,40,42,42,42,42,43,43,43,43,43,43,43,43
		.byte	44,40,41,40,42,42,42,42,43,43,43,43,43,43,43,43
		.byte	45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45
		.byte	45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45
		.byte	46,46,46,46,46,46,46,46,43,43,43,43,43,43,43,43
		.byte	46,46,46,46,46,46,46,46,43,43,43,43,43,43,43,43
		.byte	45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45
		.byte	45,45,45,45,45,45,45,45,45,45,45,45,45,45,45,45
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
		.byte	47,47,47,47,47,47,47,47,47,47,47,47,47,47,47,47
#endif
		.endm

#elif defined(CONFIG_ARCH_EBSA110)

#define IRQ_STAT		0xff000000	/* read */

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, stat, base, tmp
		mov	\base, #IRQ_STAT
		ldrb	\stat, [\base]			@ get interrupts
		mov	\irqnr, #0
		tst	\stat, #15
		addeq	\irqnr, \irqnr, #4
		moveq	\stat, \stat, lsr #4
		tst	\stat, #3
		addeq	\irqnr, \irqnr, #2
		moveq	\stat, \stat, lsr #2
		tst	\stat, #1
		addeq	\irqnr, \irqnr, #1
		moveq	\stat, \stat, lsr #1
		tst	\stat, #1			@ bit 0 should be set
		.endm

		.macro	irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_SHARK)

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov	r4, #0xe0000000

		mov	\irqstat, #0x0C
		strb	\irqstat, [r4, #0x20]		@outb(0x0C, 0x20) /* Poll command */
		ldrb	\irqnr, [r4, #0x20]		@irq = inb(0x20) & 7
		and	\irqstat, \irqnr, #0x80
		teq	\irqstat, #0
		beq	43f
		and	\irqnr, \irqnr, #7
		teq	\irqnr, #2
		bne	44f
43:		mov	\irqstat, #0x0C
		strb	\irqstat, [r4, #0xa0]		@outb(0x0C, 0xA0) /* Poll command */
		ldrb	\irqnr, [r4, #0xa0]		@irq = (inb(0xA0) & 7) + 8
		and	\irqstat, \irqnr, #0x80
		teq	\irqstat, #0
		beq	44f
		and	\irqnr, \irqnr, #7
		add	\irqnr, \irqnr, #8
44:		teq	\irqstat, #0
		.endm

		.macro	irq_prio_table
		.endm

#elif defined(CONFIG_FOOTBRIDGE)
#include <asm/hardware/dec21285.h>

		.macro	disable_fiq
		.endm

		.equ	dc21285_high, ARMCSR_BASE & 0xff000000
		.equ	dc21285_low, ARMCSR_BASE & 0x00ffffff

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov	r4, #dc21285_high
		.if	dc21285_low
		orr	r4, r4, #dc21285_low
		.endif
		ldr	\irqstat, [r4, #0x180]		@ get interrupts

		mov	\irqnr, #IRQ_SDRAMPARITY
		tst	\irqstat, #IRQ_MASK_SDRAMPARITY
		bne	1001f

		tst	\irqstat, #IRQ_MASK_UART_RX
		movne	\irqnr, #IRQ_CONRX
		bne	1001f

		tst	\irqstat, #IRQ_MASK_DMA1
		movne	\irqnr, #IRQ_DMA1
		bne	1001f

		tst	\irqstat, #IRQ_MASK_DMA2
		movne	\irqnr, #IRQ_DMA2
		bne	1001f

		tst	\irqstat, #IRQ_MASK_IN0
		movne	\irqnr, #IRQ_IN0
		bne	1001f

		tst	\irqstat, #IRQ_MASK_IN1
		movne	\irqnr, #IRQ_IN1
		bne	1001f

		tst	\irqstat, #IRQ_MASK_IN2
		movne	\irqnr, #IRQ_IN2
		bne	1001f

		tst	\irqstat, #IRQ_MASK_IN3
		movne	\irqnr, #IRQ_IN3
		bne	1001f

		tst	\irqstat, #IRQ_MASK_PCI
		movne	\irqnr, #IRQ_PCI
		bne	1001f

		tst	\irqstat, #IRQ_MASK_DOORBELLHOST
		movne	\irqnr, #IRQ_DOORBELLHOST
		bne     1001f
	
		tst	\irqstat, #IRQ_MASK_I2OINPOST
		movne	\irqnr, #IRQ_I2OINPOST
		bne	1001f

		tst	\irqstat, #IRQ_MASK_TIMER1
		movne	\irqnr, #IRQ_TIMER1
		bne	1001f

		tst	\irqstat, #IRQ_MASK_TIMER2
		movne	\irqnr, #IRQ_TIMER2
		bne	1001f

		tst	\irqstat, #IRQ_MASK_TIMER3
		movne	\irqnr, #IRQ_TIMER3
		bne	1001f

		tst	\irqstat, #IRQ_MASK_UART_TX
		movne	\irqnr, #IRQ_CONTX
		bne	1001f

		tst	\irqstat, #IRQ_MASK_PCI_ABORT
		movne	\irqnr, #IRQ_PCI_ABORT
		bne	1001f

		tst	\irqstat, #IRQ_MASK_PCI_SERR
		movne	\irqnr, #IRQ_PCI_SERR
		bne	1001f

		tst	\irqstat, #IRQ_MASK_DISCARD_TIMER
		movne	\irqnr, #IRQ_DISCARD_TIMER
		bne	1001f

		tst	\irqstat, #IRQ_MASK_PCI_DPERR
		movne	\irqnr, #IRQ_PCI_DPERR
		bne	1001f

		tst	\irqstat, #IRQ_MASK_PCI_PERR
		movne	\irqnr, #IRQ_PCI_PERR
1001:
		.endm

		.macro	irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_NEXUSPCI)

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		ldr	\irqstat, =INTCONT_BASE
		ldr	\base, =soft_irq_mask
		ldr	\irqstat, [\irqstat]		@ get interrupts
		ldr	\base, [\base]
		mov	\irqnr, #0
		and	\irqstat, \irqstat, \base	@ mask out disabled ones
1001:		tst	\irqstat, #1
		addeq	\irqnr, \irqnr, #1
		moveq	\irqstat, \irqstat, lsr #1
		tsteq	\irqnr, #32
		beq	1001b
		teq	\irqnr, #32
		.endm

		.macro	irq_prio_table
		.ltorg
		.bss
ENTRY(soft_irq_mask)
		.word	0
		.text
		.endm

#elif defined(CONFIG_ARCH_TBOX)

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		ldr	\irqstat, =0xffff7000
		ldr	\irqstat, [\irqstat]		@ get interrupts
		ldr	\base, =soft_irq_mask
		ldr	\base, [\base]
		mov	\irqnr, #0
		and	\irqstat, \irqstat, \base	@ mask out disabled ones
1001:		tst	\irqstat, #1
		addeq	\irqnr, \irqnr, #1
		moveq	\irqstat, \irqstat, lsr #1
		tsteq	\irqnr, #32
		beq	1001b
		teq	\irqnr, #32
		.endm

		.macro	irq_prio_table
		.ltorg
		.bss
ENTRY(soft_irq_mask)
		.word	0
		.text
		.endm

#elif defined(CONFIG_ARCH_SA1100)

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov	r4, #0xfa000000			@ ICIP = 0xfa050000
		add	r4, r4, #0x00050000
		ldr	\irqstat, [r4]			@ get irqs
		ldr	\irqnr, [r4, #4]		@ ICMR = 0xfa050004
		ands	\irqstat, \irqstat, \irqnr
		mov	\irqnr, #0
		beq	1001f
		tst	\irqstat, #0xff
		moveq	\irqstat, \irqstat, lsr #8
		addeq	\irqnr, \irqnr, #8
		tsteq	\irqstat, #0xff
		moveq	\irqstat, \irqstat, lsr #8
		addeq	\irqnr, \irqnr, #8
		tsteq	\irqstat, #0xff
		moveq	\irqstat, \irqstat, lsr #8
		addeq	\irqnr, \irqnr, #8
		tst	\irqstat, #0x0f
		moveq	\irqstat, \irqstat, lsr #4
		addeq	\irqnr, \irqnr, #4
		tst	\irqstat, #0x03
		moveq	\irqstat, \irqstat, lsr #2
		addeq	\irqnr, \irqnr, #2
		tst	\irqstat, #0x01
		addeqs	\irqnr, \irqnr, #1
1001:
		.endm

		.macro	irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_L7200)
#include <asm/hardware.h>
	
		.equ	irq_base_addr,	IO_BASE_2

		.macro  disable_fiq
		.endm
 
		.macro  get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov     \irqstat, #irq_base_addr		@ Virt addr IRQ regs
		add	\irqstat, \irqstat, #0x00001000		@ Status reg
		ldr     \irqstat, [\irqstat, #0]		@ get interrupts
		mov     \irqnr, #0
1001:		tst     \irqstat, #1
		addeq   \irqnr, \irqnr, #1
		moveq   \irqstat, \irqstat, lsr #1
		tsteq   \irqnr, #32
		beq     1001b
		teq     \irqnr, #32
		.endm

		.macro  irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_INTEGRATOR)

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
/* FIXME: should not be using soo many LDRs here */
		ldr	\irqnr, =IO_ADDRESS(INTEGRATOR_IC_BASE)
		ldr	\irqstat, [\irqnr, #IRQ_STATUS]		@ get masked status
		ldr	\irqnr, =IO_ADDRESS(INTEGRATOR_HDR_BASE)
		ldr	\irqnr, [\irqnr, #(INTEGRATOR_HDR_IC_OFFSET+IRQ_STATUS)]
		orr	\irqstat, \irqstat, \irqnr, lsl #INTEGRATOR_CM_INT0

		mov	\irqnr, #0
1001:		tst	\irqstat, #1
		bne	1002f
		add	\irqnr, \irqnr, #1
		mov	\irqstat, \irqstat, lsr #1
		cmp	\irqnr, #22
		bcc	1001b
1002:		/* EQ will be set if we reach 22 */
		.endm

		.macro	irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_AT91RM9200)
#include <asm/hardware.h>

		.macro  disable_fiq
		.endm

		.macro  get_irqnr_and_base, irqnr, irqstat, base, tmp
		ldr	\base, =(AT91C_VA_BASE_SYS)	@ base virtual address of SYS peripherals
		ldr	\irqnr, [\base, #AIC_IVR]	@ read IRQ vector register: de-asserts nIRQ to processor (and clears interrupt)
		ldr	\irqstat, [\base, #AIC_ISR]	@ read interrupt source number
		teq	\irqstat, #0			@ ISR is 0 when no current interrupt, or spurious interrupt
		streq   \tmp, [\base, #AIC_EOICR]	@ not going to be handled further, then ACK it now.
		.endm

		.macro  irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_MX1ADS)

		.macro	disable_fiq
		.endm
#define AITC_NIVECSR   0x40
		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		ldr	\irqstat, =IO_ADDRESS(MX1ADS_AITC_BASE)
		@ Load offset & priority of the highest priority
		@ interrupt pending.
		ldr	\irqnr, [\irqstat, #AITC_NIVECSR]
		@ Shift off the priority leaving the offset or
		@ "interrupt number"
		mov	\irqnr, \irqnr, lsr #16
 		ldr	\irqstat, =1	@ dummy compare
		ldr	\base, =0xFFFF		// invalid interrupt
		cmp	\irqnr, \base
		bne	1001f
		ldr	\irqstat, =0
1001:
		tst	\irqstat, #1	@ to make the condition code = TRUE
		.endm

		.macro	irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_OMAHA)

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		
		/* Read all interrupts pending... */
		ldr	\irqnr, =IO_ADDRESS(PLAT_PERIPHERAL_BASE) + OMAHA_INTPND
		ldr	\irqstat, [\irqnr]		/* INTPND */

		/* All pending irqs are now in \irqstat */
		mov	\irqnr, #0
1001:		tst	\irqstat, #1
		bne	1002f
		add	\irqnr, \irqnr, #1
		mov	\irqstat, \irqstat, lsr #1
		cmp	\irqnr, #MAXIRQNUM
		bcc	1001b
1002:		/* EQ will be set if we reach MAXIRQNUM */

		.endm

		.macro	irq_prio_table
		.endm    

#elif defined(CONFIG_ARCH_CLPS711X)

#include <asm/hardware/clps7111.h>

		.macro	disable_fiq
		.endm

#if (INTSR2 - INTSR1) != (INTMR2 - INTMR1)
#error INTSR stride != INTMR stride
#endif

		.macro	get_irqnr_and_base, irqnr, stat, base, mask
		mov	\base, #CLPS7111_BASE
		ldr	\stat, [\base, #INTSR1]
		ldr	\mask, [\base, #INTMR1]
		mov	\irqnr, #4
		mov	\mask, \mask, lsl #16
		and	\stat, \stat, \mask, lsr #16
		movs	\stat, \stat, lsr #4
		bne	1001f

		add	\base, \base, #INTSR2 - INTSR1
		ldr	\stat, [\base, #INTSR1]
		ldr	\mask, [\base, #INTMR1]
		mov	\irqnr, #16
		mov	\mask, \mask, lsl #16
		and	\stat, \stat, \mask, lsr #16

1001:		tst	\stat, #255
		addeq	\irqnr, \irqnr, #8
		moveq	\stat, \stat, lsr #8
		tst	\stat, #15
		addeq	\irqnr, \irqnr, #4
		moveq	\stat, \stat, lsr #4
		tst	\stat, #3
		addeq	\irqnr, \irqnr, #2
		moveq	\stat, \stat, lsr #2
		tst	\stat, #1
		addeq	\irqnr, \irqnr, #1
		moveq	\stat, \stat, lsr #1
		tst	\stat, #1			@ bit 0 should be set
		.endm

		.macro	irq_prio_table
		.endm
	
#elif defined (CONFIG_ARCH_CAMELOT)
#include <asm/arch/platform.h>
#undef IRQ_MODE /* same name defined in asm/proc/ptrace.h */
#include <asm/arch/int_ctrl00.h>
	
		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
	
		ldr	\irqstat, =INT_ID(IO_ADDRESS(EXC_INT_CTRL00_BASE))
		ldr	\irqnr,[\irqstat]		
		cmp	\irqnr,#0
		subne	\irqnr,\irqnr,#1

	
		.endm

		.macro	irq_prio_table
		.endm

#elif defined(CONFIG_ARCH_ANAKIN)

		.macro	disable_fiq
		.endm

		.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp
		mov	\base, #IO_BASE
		mov	\irqstat, #INTERRUPT_CONTROLLER
		ldr	\tmp, =anakin_irq_mask
		ldr	\irqstat, [\base, \irqstat]
		ldr	\tmp, [\tmp]
		ands	\irqstat, \irqstat, \tmp
		ldrne	\tmp, =anakin_active_irqs
		strne	\irqstat, [\tmp]
		movne	\irqnr, #IRQ_ANAKIN
		.endm

		.macro	irq_prio_table
		.ltorg
		.bss
ENTRY(anakin_irq_mask)
		.word	0
ENTRY(anakin_active_irqs)
		.space	4
		.text
		.endm

#else
#error Unknown architecture
#endif

/*
 * Invalid mode handlers
 */
__pabt_invalid:	sub	sp, sp, #S_FRAME_SIZE		@ Allocate frame size in one go
		stmia	sp, {r0 - lr}			@ Save XXX r0 - lr
		ldr	r4, .LCabt
		mov	r1, #BAD_PREFETCH
		b	1f

__dabt_invalid:	sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - lr}			@ Save SVC r0 - lr [lr *should* be intact]
		ldr	r4, .LCabt
		mov	r1, #BAD_DATA
		b	1f

__irq_invalid:	sub	sp, sp, #S_FRAME_SIZE		@ Allocate space on stack for frame
		stmfd	sp, {r0 - lr}			@ Save r0 - lr
		ldr	r4, .LCirq
		mov	r1, #BAD_IRQ
		b	1f

__und_invalid:	sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - lr}
		ldr	r4, .LCund
		mov	r1, #BAD_UNDEFINSTR		@ int reason

1:		zero_fp
		ldmia	r4, {r5 - r7}			@ Get XXX pc, cpsr, old_r0
		add	r4, sp, #S_PC
		stmia	r4, {r5 - r7}			@ Save XXX pc, cpsr, old_r0
		mov	r0, sp
		and	r2, r6, #31			@ int mode
		b	SYMBOL_NAME(bad_mode)

/* We get here if an undefined instruction happens and the floating
 * point emulator is not present.  If the offending instruction was
 * a WFS, we just perform a normal return as if we had emulated the
 * operation.  This is a hack to allow some basic userland binaries
 * to run so that the emulator module proper can be loaded. --philb
 */
fpe_not_present:
		mov	pc, lr

/*
 * SVC mode handlers
 */
		.align	5
__dabt_svc:	sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - r12}			@ save r0 - r12
		ldr	r2, .LCabt
		add	r0, sp, #S_FRAME_SIZE
		ldmia	r2, {r2 - r4}			@ get pc, cpsr
		add	r5, sp, #S_SP
		mov	r1, lr
		stmia	r5, {r0 - r4}			@ save sp_SVC, lr_SVC, pc, cpsr, old_ro
		mrs	r9, cpsr			@ Enable interrupts if they were
		tst	r3, #I_BIT
		biceq	r9, r9, #I_BIT			@ previously
		mov	r0, r2				@ *** remove once everyones in sync
/*
 * This routine must not corrupt r9
 */
#ifdef MULTI_CPU
		ldr	r4, .LCprocfns			@ pass r0, r3 to
		mov	lr, pc				@ processor code
		ldr	pc, [r4]			@ call processor specific code
#else
		bl	cpu_data_abort
#endif
		msr	cpsr_c, r9
		mov	r2, sp
		bl	SYMBOL_NAME(do_DataAbort)
		mov	r0, #I_BIT | MODE_SVC
		msr	cpsr_c, r0
		ldr	r0, [sp, #S_PSR]
		msr	spsr, r0
		ldmia	sp, {r0 - pc}^			@ load r0 - pc, cpsr

		.align	5
__irq_svc:	sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - r12}			@ save r0 - r12
		ldr	r7, .LCirq
		add	r5, sp, #S_FRAME_SIZE
		ldmia	r7, {r7 - r9}
		add	r4, sp, #S_SP
		mov	r6, lr
		stmia	r4, {r5, r6, r7, r8, r9}	@ save sp_SVC, lr_SVC, pc, cpsr, old_ro
1:		get_irqnr_and_base r0, r6, r5, lr
		movne	r1, sp
		@
		@ routine called with r0 = irq number, r1 = struct pt_regs *
		@
		adrsvc	ne, lr, 1b
		bne	asm_do_IRQ
		ldr	r0, [sp, #S_PSR]		@ irqs are already disabled
		msr	spsr, r0
		ldmia	sp, {r0 - pc}^			@ load r0 - pc, cpsr

		.ltorg

		.align	5
__und_svc:	sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - r12}			@ save r0 - r12
		ldr	r7, .LCund
		mov	r6, lr
		ldmia	r7, {r7 - r9}
		add	r5, sp, #S_FRAME_SIZE
		add	r4, sp, #S_SP
		stmia	r4, {r5 - r9}			@ save sp_SVC, lr_SVC, pc, cpsr, old_ro

		adrsvc	al, r9, 1f			@ r9  = normal FP return
		bl	call_fpe			@ lr  = undefined instr return

		mov	r0, r5				@ unsigned long pc
		mov	r1, sp				@ struct pt_regs *regs
		bl	SYMBOL_NAME(do_undefinstr)

1:		mov	r0, #I_BIT | MODE_SVC
		msr	cpsr_c, r0
		ldr	lr, [sp, #S_PSR]		@ Get SVC cpsr
		msr	spsr, lr
		ldmia	sp, {r0 - pc}^			@ Restore SVC registers

		.align	5
__pabt_svc:	sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - r12}			@ save r0 - r12
		ldr	r2, .LCabt
		add	r0, sp, #S_FRAME_SIZE
		ldmia	r2, {r2 - r4}			@ get pc, cpsr
		add	r5, sp, #S_SP
		mov	r1, lr
		stmia	r5, {r0 - r4}			@ save sp_SVC, lr_SVC, pc, cpsr, old_ro
		mrs	r9, cpsr			@ Enable interrupts if they were
		tst	r3, #I_BIT
		biceq	r9, r9, #I_BIT			@ previously
		msr	cpsr_c, r9
		mov	r0, r2				@ address (pc)
		mov	r1, sp				@ regs
		bl	SYMBOL_NAME(do_PrefetchAbort)	@ call abort handler
		mov	r0, #I_BIT | MODE_SVC
		msr	cpsr_c, r0
		ldr	r0, [sp, #S_PSR]
		msr	spsr, r0
		ldmia	sp, {r0 - pc}^			@ load r0 - pc, cpsr

		.align	5
.LCirq:		.word	__temp_irq
.LCund:		.word	__temp_und
.LCabt:		.word	__temp_abt
#ifdef MULTI_CPU
.LCprocfns:	.word	SYMBOL_NAME(processor)
#endif
.LCfp:		.word	SYMBOL_NAME(fp_enter)

		irq_prio_table

/*
 * User mode handlers
 */
		.align	5
__dabt_usr:	sub	sp, sp, #S_FRAME_SIZE		@ Allocate frame size in one go
		stmia	sp, {r0 - r12}			@ save r0 - r12
		ldr	r7, .LCabt
		add	r5, sp, #S_PC
		ldmia	r7, {r2 - r4}			@ Get USR pc, cpsr
		stmia	r5, {r2 - r4}			@ Save USR pc, cpsr, old_r0
		stmdb	r5, {sp, lr}^
		alignment_trap r7, r7, __temp_abt
		zero_fp
		mov	r0, r2				@ remove once everyones in sync
#ifdef MULTI_CPU
		ldr	r4, .LCprocfns			@ pass r0, r3 to
		mov	lr, pc				@ processor code
		ldr	pc, [r4]			@ call processor specific code
#else
		bl	cpu_data_abort
#endif
		mov	r2, #MODE_SVC
		msr	cpsr_c, r2			@ Enable interrupts
		mov	r2, sp
		adrsvc	al, lr, ret_from_exception
		b	SYMBOL_NAME(do_DataAbort)

		.align	5
__irq_usr:	sub	sp, sp, #S_FRAME_SIZE
		stmia	sp, {r0 - r12}			@ save r0 - r12
		ldr	r4, .LCirq
		add	r8, sp, #S_PC
		ldmia	r4, {r5 - r7}			@ get saved PC, SPSR
		stmia	r8, {r5 - r7}			@ save pc, psr, old_r0
		stmdb	r8, {sp, lr}^
		alignment_trap r4, r7, __temp_irq
		zero_fp
1:		get_irqnr_and_base r0, r6, r5, lr
		movne	r1, sp
		adrsvc	ne, lr, 1b
		@
		@ routine called with r0 = irq number, r1 = struct pt_regs *
		@
		bne	asm_do_IRQ
		mov	why, #0
		get_current_task tsk
		b	ret_to_user

		.ltorg

		.align	5
__und_usr:	sub	sp, sp, #S_FRAME_SIZE		@ Allocate frame size in one go
		stmia	sp, {r0 - r12}			@ Save r0 - r12
		ldr	r4, .LCund
		add	r8, sp, #S_PC
		ldmia	r4, {r5 - r7}
		stmia	r8, {r5 - r7}			@ Save USR pc, cpsr, old_r0
		stmdb	r8, {sp, lr}^			@ Save user sp, lr
		alignment_trap r4, r7, __temp_und
		zero_fp
		tst	r6, #T_BIT			@ Thumb mode
		bne	fpundefinstr
		adrsvc	al, r9, ret_from_exception	@ r9  = normal FP return
		adrsvc	al, lr, fpundefinstr		@ lr  = undefined instr return

call_fpe:	get_current_task r10
		mov	r8, #1
		strb	r8, [r10, #TSK_USED_MATH]	@ set current->used_math
		ldr	r4, .LCfp
		add	r10, r10, #TSS_FPESAVE		@ r10 = workspace
		ldr	pc, [r4]			@ Call FP module USR entry point

fpundefinstr:	mov	r0, #MODE_SVC
		msr	cpsr_c, r0			@ Enable interrupts
		mov	r0, lr
		mov	r1, sp
		adrsvc	al, lr, ret_from_exception
		b	SYMBOL_NAME(do_undefinstr)

		.align	5
__pabt_usr:	sub	sp, sp, #S_FRAME_SIZE		@ Allocate frame size in one go
		stmia	sp, {r0 - r12}			@ Save r0 - r12
		ldr	r4, .LCabt
		add	r8, sp, #S_PC
		ldmia	r4, {r5 - r7}			@ Get USR pc, cpsr
		stmia	r8, {r5 - r7}			@ Save USR pc, cpsr, old_r0
		stmdb	r8, {sp, lr}^			@ Save sp_usr lr_usr
		alignment_trap r4, r7, __temp_abt
		zero_fp
		mov	r0, #MODE_SVC
		msr	cpsr_c, r0			@ Enable interrupts
		mov	r0, r5				@ address (pc)
		mov	r1, sp				@ regs
		bl	SYMBOL_NAME(do_PrefetchAbort)	@ call abort handler
		/* fall through */
/*
 * This is the return code to user mode for abort handlers
 */
ENTRY(ret_from_exception)
		disable_irq r1
		mov	why, #0
		get_current_task tsk
		b	ret_to_user

		.data
ENTRY(fp_enter)
		.word	fpe_not_present
		.text
/*
 * Register switch for ARMv3 and ARMv4 processors
 * r0 = previous, r1 = next, return previous.
 * previous and next are guaranteed not to be the same.
 */
ENTRY(__switch_to)
		stmfd	sp!, {r4 - sl, fp, lr}		@ Store most regs on stack
		mrs	ip, cpsr
		str	ip, [sp, #-4]!			@ Save cpsr_SVC
		str	sp, [r0, #TSS_SAVE]		@ Save sp_SVC
		ldr	sp, [r1, #TSS_SAVE]		@ Get saved sp_SVC
		ldr	r2, [r1, #TSS_DOMAIN]
		ldr	ip, [sp], #4
		mcr	p15, 0, r2, c3, c0		@ Set domain register
		msr	spsr, ip			@ Save tasks CPSR into SPSR for this return
		ldmfd	sp!, {r4 - sl, fp, pc}^		@ Load all regs saved previously

		.section ".text.init",#alloc,#execinstr
/*
 * Vector stubs.  NOTE that we only align 'vector_IRQ' to a cache line boundary,
 * and we rely on each stub being exactly 48 (1.5 cache lines) in size.  This
 * means that we only ever load two cache lines for this code, or one if we're
 * lucky.  We also copy this code to 0x200 so that we can use branches in the
 * vectors, rather than ldr's.
 */
		.align	5
__stubs_start:
/*
 * Interrupt dispatcher
 * Enter in IRQ mode, spsr = SVC/USR CPSR, lr = SVC/USR PC
 */
vector_IRQ:	@
		@ save mode specific registers
		@
		ldr	r13, .LCsirq
		sub	lr, lr, #4
		str	lr, [r13]			@ save lr_IRQ
		mrs	lr, spsr
		str	lr, [r13, #4]			@ save spsr_IRQ
		@
		@ now branch to the relevent MODE handling routine
		@
		mrs	r13, cpsr
		bic	r13, r13, #MODE_MASK
		orr	r13, r13, #I_BIT | MODE_SVC
		msr	spsr_c, r13			@ switch to SVC_32 mode

		and	lr, lr, #15
		ldr	lr, [pc, lr, lsl #2]
		movs	pc, lr				@ Changes mode and branches

.LCtab_irq:	.word	__irq_usr			@  0  (USR_26 / USR_32)
		.word	__irq_invalid			@  1  (FIQ_26 / FIQ_32)
		.word	__irq_invalid			@  2  (IRQ_26 / IRQ_32)
		.word	__irq_svc			@  3  (SVC_26 / SVC_32)
		.word	__irq_invalid			@  4
		.word	__irq_invalid			@  5
		.word	__irq_invalid			@  6
		.word	__irq_invalid			@  7
		.word	__irq_invalid			@  8
		.word	__irq_invalid			@  9
		.word	__irq_invalid			@  a
		.word	__irq_invalid			@  b
		.word	__irq_invalid			@  c
		.word	__irq_invalid			@  d
		.word	__irq_invalid			@  e
		.word	__irq_invalid			@  f

		.align	5

/*
 * Data abort dispatcher - dispatches it to the correct handler for the processor mode
 * Enter in ABT mode, spsr = USR CPSR, lr = USR PC
 */
vector_data:	@
		@ save mode specific registers
		@
		ldr	r13, .LCsabt
		sub	lr, lr, #8
		str	lr, [r13]
		mrs	lr, spsr
		str	lr, [r13, #4]
		@
		@ now branch to the relevent MODE handling routine
		@
		mrs	r13, cpsr
		bic	r13, r13, #MODE_MASK
		orr	r13, r13, #I_BIT | MODE_SVC
		msr	spsr_c, r13			@ switch to SVC_32 mode

		and	lr, lr, #15
		ldr	lr, [pc, lr, lsl #2]
		movs	pc, lr				@ Changes mode and branches

.LCtab_dabt:	.word	__dabt_usr			@  0  (USR_26 / USR_32)
		.word	__dabt_invalid			@  1  (FIQ_26 / FIQ_32)
		.word	__dabt_invalid			@  2  (IRQ_26 / IRQ_32)
		.word	__dabt_svc			@  3  (SVC_26 / SVC_32)
		.word	__dabt_invalid			@  4
		.word	__dabt_invalid			@  5
		.word	__dabt_invalid			@  6
		.word	__dabt_invalid			@  7
		.word	__dabt_invalid			@  8
		.word	__dabt_invalid			@  9
		.word	__dabt_invalid			@  a
		.word	__dabt_invalid			@  b
		.word	__dabt_invalid			@  c
		.word	__dabt_invalid			@  d
		.word	__dabt_invalid			@  e
		.word	__dabt_invalid			@  f

		.align	5

/*
 * Prefetch abort dispatcher - dispatches it to the correct handler for the processor mode
 * Enter in ABT mode, spsr = USR CPSR, lr = USR PC
 */
vector_prefetch:
		@
		@ save mode specific registers
		@
		ldr	r13, .LCsabt
		sub	lr, lr, #4
		str	lr, [r13]			@ save lr_ABT
		mrs	lr, spsr
		str	lr, [r13, #4]			@ save spsr_ABT
		@
		@ now branch to the relevent MODE handling routine
		@
		mrs	r13, cpsr
		bic	r13, r13, #MODE_MASK
		orr	r13, r13, #I_BIT | MODE_SVC
		msr	spsr_c, r13			@ switch to SVC_32 mode

		ands	lr, lr, #15
		ldr	lr, [pc, lr, lsl #2]
		movs	pc, lr

.LCtab_pabt:	.word	__pabt_usr			@  0 (USR_26 / USR_32)
		.word	__pabt_invalid			@  1 (FIQ_26 / FIQ_32)
		.word	__pabt_invalid			@  2 (IRQ_26 / IRQ_32)
		.word	__pabt_svc			@  3 (SVC_26 / SVC_32)
		.word	__pabt_invalid			@  4
		.word	__pabt_invalid			@  5
		.word	__pabt_invalid			@  6
		.word	__pabt_invalid			@  7
		.word	__pabt_invalid			@  8
		.word	__pabt_invalid			@  9
		.word	__pabt_invalid			@  a
		.word	__pabt_invalid			@  b
		.word	__pabt_invalid			@  c
		.word	__pabt_invalid			@  d
		.word	__pabt_invalid			@  e
		.word	__pabt_invalid			@  f

		.align	5

/*
 * Undef instr entry dispatcher - dispatches it to the correct handler for the processor mode
 * Enter in UND mode, spsr = SVC/USR CPSR, lr = SVC/USR PC
 */
vector_undefinstr:
		@
		@ save mode specific registers
		@
		ldr	r13, .LCsund
		str	lr, [r13]			@ save lr_UND
		mrs	lr, spsr
		str	lr, [r13, #4]			@ save spsr_UND
		@
		@ now branch to the relevent MODE handling routine
		@
		mrs	r13, cpsr
		bic	r13, r13, #MODE_MASK
		orr	r13, r13, #I_BIT | MODE_SVC
		msr	spsr_c, r13			@ switch to SVC_32 mode

		and	lr, lr, #15
		ldr	lr, [pc, lr, lsl #2]
		movs	pc, lr				@ Changes mode and branches

.LCtab_und:	.word	__und_usr			@  0 (USR_26 / USR_32)
		.word	__und_invalid			@  1 (FIQ_26 / FIQ_32)
		.word	__und_invalid			@  2 (IRQ_26 / IRQ_32)
		.word	__und_svc			@  3 (SVC_26 / SVC_32)
		.word	__und_invalid			@  4
		.word	__und_invalid			@  5
		.word	__und_invalid			@  6
		.word	__und_invalid			@  7
		.word	__und_invalid			@  8
		.word	__und_invalid			@  9
		.word	__und_invalid			@  a
		.word	__und_invalid			@  b
		.word	__und_invalid			@  c
		.word	__und_invalid			@  d
		.word	__und_invalid			@  e
		.word	__und_invalid			@  f

		.align	5

/*=============================================================================
 * Undefined FIQs
 *-----------------------------------------------------------------------------
 * Enter in FIQ mode, spsr = ANY CPSR, lr = ANY PC
 * MUST PRESERVE SVC SPSR, but need to switch to SVC mode to show our msg.
 * Basically to switch modes, we *HAVE* to clobber one register...  brain
 * damage alert!  I don't think that we can execute any code in here in any
 * other mode than FIQ...  Ok you can switch to another mode, but you can't
 * get out of that mode without clobbering one register.
 */
vector_FIQ:	disable_fiq
		subs	pc, lr, #4

/*=============================================================================
 * Address exception handler
 *-----------------------------------------------------------------------------
 * These aren't too critical.
 * (they're not supposed to happen, and won't happen in 32-bit data mode).
 */

vector_addrexcptn:
		b	vector_addrexcptn

/*
 * We group all the following data together to optimise
 * for CPUs with separate I & D caches.
 */
		.align	5

.LCvswi:	.word	vector_swi

.LCsirq:	.word	__temp_irq
.LCsund:	.word	__temp_und
.LCsabt:	.word	__temp_abt

__stubs_end:

		.equ	__real_stubs_start, .LCvectors + 0x200

.LCvectors:	swi	SYS_ERROR0
		b	__real_stubs_start + (vector_undefinstr - __stubs_start)
		ldr	pc, __real_stubs_start + (.LCvswi - __stubs_start)
		b	__real_stubs_start + (vector_prefetch - __stubs_start)
		b	__real_stubs_start + (vector_data - __stubs_start)
		b	__real_stubs_start + (vector_addrexcptn - __stubs_start)
		b	__real_stubs_start + (vector_IRQ - __stubs_start)
		b	__real_stubs_start + (vector_FIQ - __stubs_start)

ENTRY(__trap_init)
		stmfd	sp!, {r4 - r6, lr}

		adr	r1, .LCvectors			@ set up the vectors
		ldmia	r1, {r1, r2, r3, r4, r5, r6, ip, lr}
		stmia	r0, {r1, r2, r3, r4, r5, r6, ip, lr}

		add	r2, r0, #0x200
		adr	r0, __stubs_start		@ copy stubs to 0x200
		adr	r1, __stubs_end
1:		ldr	r3, [r0], #4
		str	r3, [r2], #4
		cmp	r0, r1
		blt	1b
		LOADREGS(fd, sp!, {r4 - r6, pc})

		.data

/*
 * Do not reorder these, and do not insert extra data between...
 */

__temp_irq:	.word	0				@ saved lr_irq
		.word	0				@ saved spsr_irq
		.word	-1				@ old_r0
__temp_und:	.word	0				@ Saved lr_und
		.word	0				@ Saved spsr_und
		.word	-1				@ old_r0
__temp_abt:	.word	0				@ Saved lr_abt
		.word	0				@ Saved spsr_abt
		.word	-1				@ old_r0

		.globl	SYMBOL_NAME(cr_alignment)
		.globl	SYMBOL_NAME(cr_no_alignment)
SYMBOL_NAME(cr_alignment):
		.space	4
SYMBOL_NAME(cr_no_alignment):
		.space	4
