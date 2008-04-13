/*-
 * Copyright (c) 1997-2001, 2005, Juniper Networks, Inc.
 * All rights reserved.
 *
 * ns16550.h -- NS16550 DUART Device Driver, used on Atlas, SCB and NIC
 *
 * Michael Beesley, April 1997
 * Highly leveraged from the Atlas device driver written by Jim Hayes
 *
 *	JNPR: ns16550.h,v 1.2.4.1 2007/09/10 07:51:14 girish
 * $FreeBSD$
 */

#ifndef __NS16550_H__
#define	__NS16550_H__

/* speed to initialize to during chip tests */
#define	SIO_TEST_SPEED 9600

/* default serial console speed if not set with sysctl or probed from boot */
#ifndef CONSPEED
#define	CONSPEED 9600
#endif

/* default serial gdb speed if not set with sysctl or probed from boot */
#ifndef GDBSPEED
#define	GDBSPEED CONSPEED
#endif

#define	IO_COMSIZE	8		/* 8250, 16x50 com controllers */

/*
 * NS16550 UART registers
 */

/* 8250 registers #[0-6]. */

#define	IER_ERXRDY	0x1
#define	IER_ETXRDY	0x2
#define	IER_ERLS	0x4
#define	IER_EMSC	0x8

#define	IIR_IMASK	0xf
#define	IIR_RXTOUT	0xc
#define	IIR_RLS		0x6
#define	IIR_RXRDY	0x4
#define	IIR_TXRDY	0x2
#define	IIR_NOPEND	0x1
#define	IIR_MLSC	0x0
#define	IIR_FIFO_MASK	0xc0	/* set if FIFOs are enabled */

#define	LCR_DLAB	0x80
#define	CFCR_DLAB	LCR_DLAB
#define	LCR_EFR_ENABLE	0xbf	/* magic to enable EFR on 16650 up */
#define	CFCR_EFR_ENABLE	LCR_EFR_ENABLE
#define	LCR_SBREAK	0x40
#define	CFCR_SBREAK	LCR_SBREAK
#define	LCR_PZERO	0x30
#define	CFCR_PZERO	LCR_PZERO
#define	LCR_PONE	0x20
#define	CFCR_PONE	LCR_PONE
#define	LCR_PEVEN	0x10
#define	CFCR_PEVEN	LCR_PEVEN
#define	LCR_PODD	0x00
#define	CFCR_PODD	LCR_PODD
#define	LCR_PENAB	0x08
#define	CFCR_PENAB	LCR_PENAB
#define	LCR_STOPB	0x04
#define	CFCR_STOPB	LCR_STOPB
#define	LCR_8BITS	0x03
#define	CFCR_8BITS	LCR_8BITS
#define	LCR_7BITS	0x02
#define	CFCR_7BITS	LCR_7BITS
#define	LCR_6BITS	0x01
#define	CFCR_6BITS	LCR_6BITS
#define	LCR_5BITS	0x00
#define	CFCR_5BITS	LCR_5BITS

#define	MCR_PRESCALE	0x80	/* only available on 16650 up */
#define	MCR_LOOPBACK	0x10
#define	MCR_IE		0x08
#define	MCR_IENABLE	MCR_IE
#define	MCR_DRS		0x04
#define	MCR_RTS		0x02
#define	MCR_DTR		0x01

#define	LSR_RCV_FIFO	0x80
#define	LSR_TEMT	0x40
#define	LSR_TSRE	LSR_TEMT
#define	LSR_THRE	0x20
#define	LSR_TXRDY	LSR_THRE
#define	LSR_BI		0x10
#define	LSR_FE		0x08
#define	LSR_PE		0x04
#define	LSR_OE		0x02
#define	LSR_RXRDY	0x01
#define	LSR_RCV_MASK	0x1f

#define	MSR_DCD		0x80
#define	MSR_RI		0x40
#define	MSR_DSR		0x20
#define	MSR_CTS		0x10
#define	MSR_DDCD	0x08
#define	MSR_TERI	0x04
#define	MSR_DDSR	0x02
#define	MSR_DCTS	0x01

#define	FCR_ENABLE	0x01
#define	FIFO_ENABLE	FCR_ENABLE
#define	FCR_RCV_RST	0x02
#define	FIFO_RCV_RST	FCR_RCV_RST
#define	FCR_XMT_RST	0x04
#define	FIFO_XMT_RST	FCR_XMT_RST
#define	FCR_DMA		0x08
#define	FIFO_DMA_MODE	FCR_DMA
#define	FCR_RX_LOW	0x00
#define	FIFO_RX_LOW	FCR_RX_LOW
#define	FCR_RX_MEDL	0x40
#define	FIFO_RX_MEDL	FCR_RX_MEDL
#define	FCR_RX_MEDH	0x80
#define	FIFO_RX_MEDH	FCR_RX_MEDH
#define	FCR_RX_HIGH	0xc0
#define	FIFO_RX_HIGH	FCR_RX_HIGH

/* 16650 registers #2,[4-7].  Access enabled by LCR_EFR_ENABLE. */

#define	EFR_CTS		0x80
#define	EFR_AUTOCTS	EFR_CTS
#define	EFR_RTS		0x40
#define	EFR_AUTORTS	EFR_RTS
#define	EFR_EFE		0x10	/* enhanced functions enable */

#define	com_data	0	/* data register (R) */
#define	com_rdata	0	/* data register (R) */
#define	com_tdata	0	/* data register (W) */
#define	com_dlbl	0	/* divisor latch low (W) */
#define	com_dlbh	0x4	/* divisor latch high (W) */
#define	com_ier		0x4	/* interrupt enable (W) */
#define	com_iir		0x8	/* interrupt identification (R) */
#define	com_fifo	0x8	/* FIFO control (W) */
#define	com_lctl	0xc	/* line control register (R/W) */
#define	com_cfcr	0xc	/* line control register (R/W) */
#define	com_mcr		0x10	/* modem control register (R/W) */
#define	com_lsr		0x14	/* line status register (R/W) */
#define	com_msr		0x18	/* modem status register (R/W) */

#define	NS16550_HZ	(33300000)	/* 33.3 Mhz */
#define	DEFAULT_RCLK	(33300000)
#define	NS16550_PAD(x)

/*
 * ns16550_device: Structure to lay down over the device registers
 * Note: all accesses are 8-bit reads and writes
 */
typedef struct {
	volatile u_int32_t data;	/* data register (R/W) */
	volatile u_int32_t ier;		/* interrupt enable (W) */
	volatile u_int32_t iir;		/* interrupt identification (R) */
	volatile u_int32_t cfcr;	/* line control register (R/W) */
	volatile u_int32_t mcr;		/* modem control register (R/W) */
	volatile u_int32_t lsr;		/* line status register (R/W) */
	volatile u_int32_t msr;		/* modem status register (R/W) */
	volatile u_int32_t scr;		/* scratch register (R/W) */
} ns16550_device;


#define	com_lcr	com_cfcr
#define	com_efr	com_fifo


#define	NS16550_SYNC	__asm __volatile ("sync")


#define	NS16550_DEVICE		(1<<0)
#define	TI16C752B_DEVICE	(1<<1)
#define	fifo			iir		/* 16550 fifo control (W) */

/* 16 bit baud rate divisor (lower byte in dca_data, upper in dca_ier) */
#define	BRTC(x)		(NS16550_HZ / (16*(x)))

#define	PA_2_K1VA(a)	(MIPS_UNCACHED_MEMORY_ADDR | (a))

#ifdef	COMBRD
#undef	COMBRD
#define	COMBRD(x)	(NS16550_HZ / (16*(x)))
#endif

void uart_post_init(u_int32_t addr);
void puts_post(u_int32_t addr, const char *char_p);
void hexout_post(u_int32_t addr, u_int32_t value, int num_chars);

#endif /* __NS16550_H__ */

/* end of file */
