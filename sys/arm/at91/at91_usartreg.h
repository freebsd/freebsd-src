/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD: src/sys/arm/at91/at91_usartreg.h,v 1.2.8.1 2008/11/25 02:59:29 kensmith Exp $ */

#ifndef AT91USARTREG_H_
#define AT91USARTREG_H_

#define USART_CR		0x00 /* Control register */
#define USART_CR_RSTRX		(1UL << 2) /* Reset Receiver */
#define USART_CR_RSTTX		(1UL << 3) /* Reset Transmitter */
#define USART_CR_RXEN		(1UL << 4) /* Receiver Enable */
#define USART_CR_RXDIS		(1UL << 5) /* Receiver Disable */
#define USART_CR_TXEN		(1UL << 6) /* Transmitter Enable */
#define USART_CR_TXDIS		(1UL << 7) /* Transmitter Disable */
#define USART_CR_RSTSTA		(1UL << 8) /* Reset Status Bits */
#define USART_CR_STTBRK		(1UL << 9) /* Start Break */
#define USART_CR_STPBRK		(1UL << 10) /* Stop Break */
#define USART_CR_STTTO		(1UL << 11) /* Start Time-out */
#define USART_CR_SENDA		(1UL << 12) /* Send Address */
#define USART_CR_RSTIT		(1UL << 13) /* Reset Iterations */
#define USART_CR_RSTNACK	(1UL << 14) /* Reset Non Acknowledge */
#define USART_CR_RETTO		(1UL << 15) /* Rearm Time-out */
#define USART_CR_DTREN		(1UL << 16) /* Data Terminal ready Enable */
#define USART_CR_DTRDIS		(1UL << 17) /* Data Terminal ready Disable */
#define USART_CR_RTSEN		(1UL << 18) /* Request to Send enable */
#define USART_CR_RTSDIS		(1UL << 19) /* Request to Send Disable */

#define USART_MR		0x04 /* Mode register */
#define USART_MR_MODE_NORMAL	0	/* Normal/Async/3-wire rs-232 */
#define USART_MR_MODE_RS485	1	/* RS485 */
#define USART_MR_MODE_HWFLOW	2	/* Hardware flow control/handshake */
#define USART_MR_MODE_MODEM	3	/* Full modem protocol */
#define USART_MR_MODE_ISO7816T0 4	/* ISO7816 T=0 */
#define USART_MR_MODE_ISO7816T1 6	/* ISO7816 T=1 */
#define USART_MR_MODE_IRDA	8	/* IrDA mode */
#define USART_MR_USCLKS_MCK	(0U << 4) /* use MCK for baudclock */
#define USART_MR_USCLKS_MCKDIV	(1U << 4) /* use MCK/DIV for baudclock */
#define USART_MR_USCLKS_SCK	(3U << 4) /* use SCK (ext) for baudclock */
#define USART_MR_CHRL_5BITS	(0U << 6)
#define USART_MR_CHRL_6BITS	(1U << 6)
#define USART_MR_CHRL_7BITS	(2U << 6)
#define USART_MR_CHRL_8BITS	(3U << 6)
#define USART_MR_SYNC		(1U << 8) /* 1 -> sync 0 -> async */
#define USART_MR_PAR_EVEN	(0U << 9)
#define USART_MR_PAR_ODD	(1U << 9)
#define USART_MR_PAR_SPACE	(2U << 9)
#define USART_MR_PAR_MARK	(3U << 9)
#define USART_MR_PAR_NONE	(4U << 9)
#define USART_MR_PAR_MULTIDROP	(6U << 9)
#define USART_MR_NBSTOP_1	(0U << 12)
#define USART_MR_NBSTOP_1_5	(1U << 12)
#define USART_MR_NBSTOP_2	(2U << 12)
#define USART_MR_CHMODE_NORMAL	(0U << 14)
#define USART_MR_CHMODE_ECHO	(1U << 14)
#define USART_MR_CHMODE_LOOP	(2U << 14)
#define USART_MR_CHMODE_REMLOOP	(3U << 14)
#define USART_MR_MSBF		(1U << 16)
#define USART_MR_MODE9		(1U << 17)
#define USART_MR_CKLO_SCK	(1U << 18)
#define USART_MR_OVER16		0
#define USART_MR_OVER8		(1U << 19)
#define USART_MR_INACK		(1U << 20) /* Inhibit NACK generation */
#define USART_MR_DSNACK		(1U << 21) /* Disable Successive NACK */
#define USART_MR_MAXITERATION(x) ((x) << 24)
#define USART_MR_FILTER		(1U << 28) /* Filters for Ir lines */

#define USART_IER		0x08 /* Interrupt enable register */
#define USART_IDR		0x0c /* Interrupt disable register */
#define USART_IMR		0x10 /* Interrupt mask register */
#define USART_CSR		0x14 /* Channel status register */

#define USART_CSR_RXRDY		(1UL << 0) /* Receiver ready */
#define USART_CSR_TXRDY		(1UL << 1) /* Transmitter ready */
#define USART_CSR_RXBRK		(1UL << 2) /* Break received */
#define USART_CSR_ENDRX		(1UL << 3) /* End of Transfer RX from PDC */
#define USART_CSR_ENDTX		(1UL << 4) /* End of Transfer TX from PDC */
#define USART_CSR_OVRE		(1UL << 5) /* Overrun error */
#define USART_CSR_FRAME		(1UL << 6) /* Framing error */
#define USART_CSR_PARE		(1UL << 7) /* Parity Error */
#define USART_CSR_TIMEOUT	(1UL << 8) /* Timeout since start-timeout */
#define USART_CSR_TXEMPTY	(1UL << 9) /* Transmitter empty */
#define USART_CSR_ITERATION	(1UL << 10) /* max repetitions since RSIT */
#define USART_CSR_TXBUFE	(1UL << 11) /* Buffer empty from PDC */
#define USART_CSR_RXBUFF	(1UL << 12) /* Buffer full from PDC */
#define USART_CSR_NACK		(1UL << 13) /* NACK since last RSTNACK */
#define USART_CSR_RIIC		(1UL << 16) /* RI delta since last csr read */
#define USART_CSR_DSRIC		(1UL << 17) /* DSR delta */
#define USART_CSR_DCDIC		(1UL << 18) /* DCD delta */
#define USART_CSR_CTSIC		(1UL << 19) /* CTS delta */
#define USART_CSR_RI		(1UL << 20) /* RI status */
#define USART_CSR_DSR		(1UL << 21) /* DSR status */
#define USART_CSR_DCD		(1UL << 22) /* DCD status */
#define USART_CSR_CTS		(1UL << 23) /* CTS status */

#define USART_RHR		0x18 /* Receiver holding register */
#define USART_THR		0x1c /* Transmitter holding register */
#define USART_BRGR		0x20 /* Baud rate generator register */
#define USART_RTOR		0x24 /* Receiver time-out register */
#define USART_TTR		0x28 /* Transmitter timeguard register */
/* 0x2c to 0x3c reserved */
#define USART_FDRR		0x40 /* FI DI ratio register */
#define USART_NER		0x44 /* Number of errors register */
/* 0x48 reserved */
#define USART_IFR		0x48 /* IrDA filter register */

#endif /* AT91RM92REG_H_ */
