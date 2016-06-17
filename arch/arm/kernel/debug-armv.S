/*
 *  linux/arch/arm/kernel/debug-armv.S
 *
 *  Copyright (C) 1994-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  32-bit debugging code
 */
#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/hardware.h>

		.text

/*
 * Some debugging routines (useful if you've got MM problems and
 * printk isn't working).  For DEBUGGING ONLY!!!  Do not leave
 * references to these in a production kernel!
 */
#if defined(CONFIG_ARCH_RPC) || defined(CONFIG_ARCH_RISCSTATION)
		.macro	addruart,rx

/* note, errors can occur in the head code BEFORE the memory map has been
 * setup (and therefore the mapping for the IO) so the #if statement allows
 * the address of the uart to be changed to the physical address instead of
 * the logical
 *
 * (bjd)
 */
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		movne	\rx, #0xe0000000
		moveq	\rx, #0x03000000
		orr	\rx, \rx, #0x00010000
		orr	\rx, \rx, #0x00000fe0
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx]
		.endm

		.macro	busyuart,rd,rx
1001:		ldrb	\rd, [\rx, #0x14]
		and	\rd, \rd, #0x60
		teq	\rd, #0x60
		bne	1001b
		.endm

		.macro	waituart,rd,rx
1001:		ldrb	\rd, [\rx, #0x18]
		tst	\rd, #0x10
		beq	1001b
		.endm

#elif defined(CONFIG_ARCH_EBSA110)
		.macro	addruart,rx
		mov	\rx, #0xf0000000
		orr	\rx, \rx, #0x00000be0
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx]
		.endm

		.macro	busyuart,rd,rx
1002:		ldrb	\rd, [\rx, #0x14]
		and	\rd, \rd, #0x60
		teq	\rd, #0x60
		bne	1002b
		.endm

		.macro	waituart,rd,rx
1001:		ldrb	\rd, [\rx, #0x18]
		tst	\rd, #0x10
		beq	1001b
		.endm

#elif defined(CONFIG_ARCH_SHARK)
		.macro	addruart,rx
		mov	\rx, #0xe0000000
		orr	\rx, \rx, #0x000003f8
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx]
		.endm

		.macro	busyuart,rd,rx
		mov	\rd, #0
1001:		add	\rd, \rd, #1
		teq	\rd, #0x10000
		bne	1001b
		.endm

		.macro	waituart,rd,rx
		.endm

#elif defined(CONFIG_FOOTBRIDGE)

#include <asm/hardware/dec21285.h>

#ifndef CONFIG_DEBUG_DC21285_PORT
	/* For NetWinder debugging */
		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx, #0x7c000000	@ physical
		movne	\rx, #0xff000000	@ virtual
		orr	\rx, \rx, #0x000003f8
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx]
		.endm

		.macro	busyuart,rd,rx
1002:		ldrb	\rd, [\rx, #0x5]
		and	\rd, \rd, #0x60
		teq	\rd, #0x60
		bne	1002b
		.endm

		.macro	waituart,rd,rx
1001:		ldrb	\rd, [\rx, #0x6]
		tst	\rd, #0x10
		beq	1001b
		.endm
#else
	/* For EBSA285 debugging */
		.equ	dc21285_high, ARMCSR_BASE & 0xff000000
		.equ	dc21285_low,  ARMCSR_BASE & 0x00ffffff

		.macro	addruart,rx
		mov	\rx, #dc21285_high
		.if	dc21285_low
		orr	\rx, \rx, #dc21285_low
		.endif
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #0x160]	@ UARTDR
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #0x178]	@ UARTFLG
		tst	\rd, #1 << 3
		bne	1001b
		.endm

		.macro	waituart,rd,rx
		.endm
#endif
#elif defined(CONFIG_ARCH_FTVPCI)
		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		movne	\rx, #0xe0000000
		moveq	\rx, #0x10000000
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #0xc]
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #0x4]
		tst	\rd, #1 << 2
		beq	1001b
		.endm

		.macro	waituart,rd,rx
		.endm

#elif defined(CONFIG_ARCH_SA1100)

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx, #0x80000000	@ physical base address
		movne	\rx, #0xf8000000	@ virtual address

		@ We probe for the active serial port here, coherently with
		@ the comment in include/asm-arm/arch-sa1100/uncompress.h.
		@ We assume r1 can be clobbered.

		@ see if Ser3 is active
		add	\rx, \rx, #0x00050000
		ldr	r1, [\rx, #UTCR3]
		tst	r1, #UTCR3_TXE

		@ if Ser3 is inactive, then try Ser1
		addeq	\rx, \rx, #(0x00010000 - 0x00050000)
		ldreq	r1, [\rx, #UTCR3]
		tsteq	r1, #UTCR3_TXE

		@ if Ser1 is inactive, then try Ser2
		addeq	\rx, \rx, #(0x00030000 - 0x00010000)
		ldreq	r1, [\rx, #UTCR3]
		tsteq	r1, #UTCR3_TXE

		@ if all ports are inactive, then there is nothing we can do
		moveq	pc, lr
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #UTDR]
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #UTSR1]
		tst	\rd, #UTSR1_TNF
		beq	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #UTSR1]
		tst	\rd, #UTSR1_TBY
		bne	1001b
		.endm

#elif defined(CONFIG_ARCH_CLPS7500)
		.macro	addruart,rx
		mov	\rx, #0xe0000000
		orr	\rx, \rx, #0x00010000
		orr	\rx, \rx, #0x00000be0
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx]
		.endm

		.macro	busyuart,rd,rx
		.endm

		.macro	waituart,rd,rx
1001:		ldrb	\rd, [\rx, #0x14]
		tst	\rd, #0x20
		beq	1001b
		.endm

#elif defined(CONFIG_ARCH_L7200)

		.equ	io_virt, IO_BASE
		.equ	io_phys, IO_START

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx, #io_phys		@ physical base address
		movne	\rx, #io_virt		@ virtual address
		add	\rx, \rx, #0x00044000	@ UART1
@		add	\rx, \rx, #0x00045000	@ UART2
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #0x0]	@ UARTDR
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #0x18]	@ UARTFLG
		tst	\rd, #1 << 5		@ UARTFLGUTXFF - 1 when full
		bne	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #0x18]	@ UARTFLG
		tst	\rd, #1 << 3		@ UARTFLGUBUSY - 1 when busy
		bne	1001b
		.endm

#elif defined(CONFIG_ARCH_INTEGRATOR)

#include <asm/hardware/serial_amba.h>

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx, #0x16000000	@ physical base address
		movne	\rx, #0xf0000000	@ virtual base
		addne	\rx, \rx, #0x16000000 >> 4
		.endm

		.macro	senduart,rd,rx
		strb	\rd, [\rx, #AMBA_UARTDR]
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #0x18]	@ UARTFLG
		tst	\rd, #1 << 5		@ UARTFLGUTXFF - 1 when full
		bne	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #0x18]	@ UARTFLG
		tst	\rd, #1 << 3		@ UARTFLGUBUSY - 1 when busy
		bne	1001b
		.endm
#elif defined(CONFIG_ARCH_AT91RM9200)

		.macro  addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1				@ MMU enabled?
		ldreq	\rx, =AT91C_BASE_SYS		@ System peripherals (phys address)
		ldrne	\rx, =AT91C_VA_BASE_SYS		@ System peripherals (virt address)
		.endm

		.macro  senduart,rd,rx
		strb    \rd, [\rx, #DBGU_THR]		@ DBGU_THR
		.endm

		.macro  waituart,rd,rx
1001:		ldr     \rd, [\rx, #DBGU_CSR]		@ DBGU_CSR
		tst	\rd, #AT91C_DBGU_TXRDY		@ DBGU_TXRDY = 1 when ready to transmit
		beq	1001b
		.endm

		.macro  busyuart,rd,rx
1001:		ldr	\rd, [\rx, #DBGU_CSR]		@ DBGU_CSR
		tst	\rd, #AT91C_DBGU_TXEMPTY	@ DBGU_TXEMPTY = 1 when transmission complete
		beq	1001b
		.endm

#elif defined(CONFIG_ARCH_CLPS711X)

#include <asm/hardware/clps7111.h>

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx, #CLPS7111_PHYS_BASE
		movne	\rx, #CLPS7111_VIRT_BASE
#ifndef CONFIG_DEBUG_CLPS711X_UART2
		add	\rx, \rx, #0x0000	@ UART1
#else
		add	\rx, \rx, #0x1000	@ UART2
#endif
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #0x0480]	@ UARTDR
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #0x0140]	@ SYSFLGx
		tst	\rd, #1 << 11		@ UBUSYx
		bne	1001b
		.endm

		.macro	busyuart,rd,rx
		tst	\rx, #0x1000		@ UART2 does not have CTS here
		bne	1002f
1001:		ldr	\rd, [\rx, #0x0140]	@ SYSFLGx
		tst	\rd, #1 << 8		@ CTS
		bne	1001b
1002:
		.endm
#elif defined(CONFIG_ARCH_OMAHA)

#include <asm/hardware/serial_omaha.h>

		.macro	addruart,rx
		.endm

		.macro	senduart,rd,rx
		.endm

		.macro	waituart,rd,rx
		.endm

		.macro	busyuart,rd,rx
		.endm

#elif defined(CONFIG_ARCH_ANAKIN)

//#//include <asm/arch/serial_reg.h>

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx, #IO_START
		movne	\rx, #IO_BASE
		add	\rx, \rx, #UART0
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #0x14]	@ tx
		ldr	\rd, [\rx, #0x18]
		orr	\rd, \rd, #SENDREQUEST
		str	\rd, [\rx, #0x18]
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #0x10]
		tst	\rd, #TXEMPTY
		beq	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #0x10]
		tst	\rd, #CTS
		bne	1001b
		.endm

#elif defined(CONFIG_ARCH_CAMELOT)

#include <asm/arch/excalibur.h>
#define UART00_TYPE
#include <asm/arch/uart00.h>

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		ldr	\rx, =EXC_UART00_BASE	@ physical base address
		orrne	\rx, \rx, #0xff000000	@ virtual base
		orrne	\rx, \rx, #0x00f00000
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #UART_TD(0)]
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #UART_TSR(0)]
		and 	\rd, \rd,  #UART_TSR_TX_LEVEL_MSK
		cmp	\rd, #15
		beq	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, #UART_TSR(0)]
		ands 	\rd, \rd,  #UART_TSR_TX_LEVEL_MSK
		bne	1001b
		.endm

#elif defined(CONFIG_ARCH_MX1ADS)

		.macro	addruart,rx
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		moveq	\rx, #0x00000000	@ physical
		movne	\rx, #0xf0000000	@ virtual
		orr	\rx, \rx, #0x00200000
		orr	\rx, \rx, #0x00006000	@ UART1 offset
		.endm

		.macro	senduart,rd,rx
		str	\rd, [\rx, #0x40]	@ TXDATA
		.endm

		.macro	waituart,rd,rx
#if	0
/* REVISIT: This isn't right. I believe we should be checking CTS here instead
 * to see if the user has entered <CTRL><S> to pause console output.
 * gdavis@mvista.com 28apr03
 */
1001:		ldr	\rd, [\rx, #0x94]	@ SR1
		tst	\rd, #1 << 13		@ TRDY
		bne	1001b			@ wait until TX FIFO ready
#endif
		.endm

		.macro	busyuart,rd,rx
1002:		ldr	\rd, [\rx, #0x98]	@ SR2
		tst	\rd, #1 << 3		@ TXDC
		beq	1002b			@ wait until transmit done
		.endm



#else
#error Unknown architecture
#endif

/*
 * Useful debugging routines
 */
ENTRY(printhex8)
		mov	r1, #8
		b	printhex

ENTRY(printhex4)
		mov	r1, #4
		b	printhex

ENTRY(printhex2)
		mov	r1, #2
printhex:	adr	r2, hexbuf
		add	r3, r2, r1
		mov	r1, #0
		strb	r1, [r3]
1:		and	r1, r0, #15
		mov	r0, r0, lsr #4
		cmp	r1, #10
		addlt	r1, r1, #'0'
		addge	r1, r1, #'a' - 10
		strb	r1, [r3, #-1]!
		teq	r3, r2
		bne	1b
		mov	r0, r2
		b	printascii

		.ltorg

ENTRY(printascii)
		addruart r3
		b	2f
1:		waituart r2, r3
		senduart r1, r3
		busyuart r2, r3
		teq	r1, #'\n'
		moveq	r1, #'\r'
		beq	1b
2:		teq	r0, #0
		ldrneb	r1, [r0], #1
		teqne	r1, #0
		bne	1b
		mov	pc, lr

ENTRY(printch)
		addruart r3
		mov	r1, r0
		mov	r0, #0
		b	1b

hexbuf:		.space 16
