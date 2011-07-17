/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __XLP_UART_H__
#define __XLP_UART_H__

/* UART Specific registers */
#define XLP_UART_RX_DATA_REG		0x40
#define XLP_UART_TX_DATA_REG		0x40

#define XLP_UART_INT_EN_REG		0x41
#define XLP_UART_INT_ID_REG		0x42
#define XLP_UART_FIFO_CTL_REG		0x42
#define XLP_UART_LINE_CTL_REG		0x43
#define XLP_UART_MODEM_CTL_REG		0x44
#define XLP_UART_LINE_STS_REG		0x45
#define XLP_UART_MODEM_STS_REG		0x46

#define XLP_UART_DIVISOR0_REG		0x40
#define XLP_UART_DIVISOR1_REG		0x41

#define XLP_UART_BASE_BAUD		(133000000/16)
#define XLP_UART_BAUD_DIVISOR(baud)	(XLP_UART_BASE_BAUD / baud)

/* LCR mask values */
#define LCR_5BITS			0x00
#define LCR_6BITS			0x01
#define LCR_7BITS			0x02
#define LCR_8BITS			0x03
#define LCR_STOPB			0x04
#define LCR_PENAB			0x08
#define LCR_PODD			0x00
#define LCR_PEVEN			0x10
#define LCR_PONE			0x20
#define LCR_PZERO			0x30
#define LCR_SBREAK			0x40
#define LCR_EFR_ENABLE			0xbf
#define LCR_DLAB			0x80

/* MCR mask values */
#define MCR_DTR				0x01
#define MCR_RTS				0x02
#define MCR_DRS				0x04
#define MCR_IE				0x08
#define MCR_LOOPBACK			0x10

/* FCR mask values */
#define FCR_RCV_RST			0x02
#define FCR_XMT_RST			0x04
#define FCR_RX_LOW			0x00
#define FCR_RX_MEDL			0x40
#define FCR_RX_MEDH			0x80
#define FCR_RX_HIGH			0xc0

/* IER mask values */
#define IER_ERXRDY			0x1
#define IER_ETXRDY			0x2
#define IER_ERLS			0x4
#define IER_EMSC			0x8

/* uart IRQ info */
#define XLP_NODE0_UART0_IRQ		17
#define XLP_NODE1_UART0_IRQ		18
#define XLP_NODE2_UART0_IRQ		19
#define XLP_NODE3_UART0_IRQ		20
#define XLP_NODE0_UART1_IRQ		21
#define XLP_NODE1_UART1_IRQ		22
#define XLP_NODE2_UART1_IRQ		23
#define XLP_NODE3_UART1_IRQ		24

#if !defined(LOCORE) && !defined(__ASSEMBLY__)

#define	nlm_rdreg_uart(b, r)		nlm_read_reg_kseg(b,r)
#define	nlm_wreg_uart(b, r, v)		nlm_write_reg_kseg(b,r,v)
#define nlm_pcibase_uart(node, inst)	nlm_pcicfg_base(XLP_IO_UART_OFFSET(node, inst))
#define nlm_regbase_uart(node, inst)	nlm_pcibase_uart(node, inst)

static __inline__ void
nlm_uart_set_baudrate(uint64_t base, int baud)
{
	uint32_t lcr;

	lcr = nlm_rdreg_uart(base, XLP_UART_LINE_CTL_REG);

	/* enable divisor register, and write baud values */
	nlm_wreg_uart(base, XLP_UART_LINE_CTL_REG, lcr | (1 << 7));
	nlm_wreg_uart(base, XLP_UART_DIVISOR0_REG,
			(XLP_UART_BAUD_DIVISOR(baud) & 0xff));
	nlm_wreg_uart(base, XLP_UART_DIVISOR1_REG,
			((XLP_UART_BAUD_DIVISOR(baud) >> 8) & 0xff));

	/* restore default lcr */
	nlm_wreg_uart(base, XLP_UART_LINE_CTL_REG, lcr);
}

static __inline__ void
nlm_outbyte (uint64_t base, char c)
{
	uint32_t lsr;

	for (;;) {
		lsr = nlm_rdreg_uart(base, XLP_UART_LINE_STS_REG);
		if (lsr & 0x20) break;
	}

	nlm_wreg_uart(base, XLP_UART_TX_DATA_REG, (int)c);
}

static __inline__ char
nlm_inbyte (uint64_t base)
{
	int data, lsr;

	for(;;) {
		lsr = nlm_rdreg_uart(base, XLP_UART_LINE_STS_REG);
		if (lsr & 0x80) {	/* parity/frame/break-error - push a zero */
			data = 0;
			break;
		}
		if (lsr & 0x01) { 	/* Rx data */
			data = nlm_rdreg_uart(base, XLP_UART_RX_DATA_REG);
			break;
		}
	}

	return (char)data;
}

static __inline__ int
nlm_uart_init(uint64_t base, int baud, int databits, int stopbits,
		int parity, int int_en, int loopback)
{
	uint32_t lcr;

	lcr = 0;
	if (databits >= 8)
		lcr |= LCR_8BITS;
	else if (databits == 7)
		lcr |= LCR_7BITS;
	else if (databits == 6)
		lcr |= LCR_6BITS;
	else
		lcr |= LCR_5BITS;

	if (stopbits > 1)
		lcr |= LCR_STOPB;

	lcr |= parity << 3;

	/* setup default lcr */
	nlm_wreg_uart(base, XLP_UART_LINE_CTL_REG, lcr);

	/* Reset the FIFOs */
	nlm_wreg_uart(base, XLP_UART_LINE_CTL_REG, FCR_RCV_RST | FCR_XMT_RST);

	nlm_uart_set_baudrate(base, baud);

	if (loopback)
		nlm_wreg_uart(base, XLP_UART_MODEM_CTL_REG, 0x1f);

	if (int_en)
		nlm_wreg_uart(base, XLP_UART_INT_EN_REG, IER_ERXRDY | IER_ETXRDY);

	return 0;
}

#endif /* !LOCORE && !__ASSEMBLY__ */
#endif /* __XLP_UART_H__ */

