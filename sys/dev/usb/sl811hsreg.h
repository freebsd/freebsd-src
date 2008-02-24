/*	$NetBSD$	*/
/*	$FreeBSD: src/sys/dev/usb/sl811hsreg.h,v 1.1 2005/07/14 15:57:00 takawata Exp $	*/


/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tetsuya Isaki.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ScanLogic SL811HS/T USB Host Controller
 */

#define SL11_IDX_ADDR	(0x00)
#define SL11_IDX_DATA	(0x01)
#define SL11_PORTSIZE	(0x02)

#define SL11_E0BASE	(0x00)		/* Base of Control0 */
#define SL11_E0CTRL	(0x00)		/* Host Control Register */
#define SL11_E0ADDR	(0x01)		/* Host Base Address */
#define SL11_E0LEN	(0x02)		/* Host Base Length */
#define SL11_E0STAT	(0x03)		/* USB Status (Read) */
#define SL11_E0PID	SL11_E0STAT	/* Host PID, Device Endpoint (Write) */
#define SL11_E0CONT	(0x04)		/* Transfer Count (Read) */
#define SL11_E0DEV	SL11_E0CONT	/* Host Device Address (Write) */

#define SL11_E1BASE	(0x08)		/* Base of Control1 */
#define SL11_E1CTRL	(SL11_E1BASE + SL11_E0CTRL)
#define SL11_E1ADDR	(SL11_E1BASE + SL11_E0ADDR)
#define SL11_E1LEN	(SL11_E1BASE + SL11_E0LEN)
#define SL11_E1STAT	(SL11_E1BASE + SL11_E0STAT)
#define SL11_E1PID	(SL11_E1BASE + SL11_E0PID)
#define SL11_E1CONT	(SL11_E1BASE + SL11_E0CONT)
#define SL11_E1DEV	(SL11_E1BASE + SL11_E0DEV)

#define SL11_CTRL	(0x05)		/* Control Register1 */
#define SL11_IER	(0x06)		/* Interrupt Enable Register */
#define SL11_ISR	(0x0d)		/* Interrupt Status Register */
#define SL11_DATA	(0x0e)		/* SOF Counter Low (Write) */
#define SL11_REV	SL11_DATA	/* HW Revision Register (Read) */
#define SL811_CSOF	(0x0f)		/* SOF Counter High(R), Control2(W) */
#define SL11_MEM	(0x10)		/* Memory Buffer (0x10 - 0xff) */

#define SL11_EPCTRL_ARM		(0x01)
#define SL11_EPCTRL_ENABLE	(0x02)
#define SL11_EPCTRL_DIRECTION	(0x04)
#define SL11_EPCTRL_ISO		(0x10)
#define SL11_EPCTRL_SOF		(0x20)
#define SL11_EPCTRL_DATATOGGLE	(0x40)
#define SL11_EPCTRL_PREAMBLE	(0x80)

#define SL11_EPPID_PIDMASK	(0xf0)
#define SL11_EPPID_EPMASK	(0x0f)

#define SL11_EPSTAT_ACK		(0x01)
#define SL11_EPSTAT_ERROR	(0x02)
#define SL11_EPSTAT_TIMEOUT	(0x04)
#define SL11_EPSTAT_SEQUENCE	(0x08)
#define SL11_EPSTAT_SETUP	(0x10)
#define SL11_EPSTAT_OVERFLOW	(0x20)
#define SL11_EPSTAT_NAK		(0x40)
#define SL11_EPSTAT_STALL	(0x80)

#define SL11_CTRL_ENABLESOF	(0x01)
#define SL11_CTRL_EOF2		(0x04)
#define SL11_CTRL_RESETENGINE	(0x08)
#define SL11_CTRL_JKSTATE	(0x10)
#define SL11_CTRL_LOWSPEED	(0x20)
#define SL11_CTRL_SUSPEND	(0x40)

#define SL11_IER_USBA		(0x01)	/* USB-A done */
#define SL11_IER_USBB		(0x02)	/* USB-B done */
#define SL11_IER_BABBLE		(0x04)	/* Babble detection */
#define SL11_IER_SOFTIMER	(0x10)	/* 1ms SOF timer */
#define SL11_IER_INSERT		(0x20)	/* Slave Insert/Remove detection */
#define SL11_IER_RESET		(0x40)	/* USB Reset/Resume */

#define SL11_ISR_USBA		(0x01)	/* USB-A done */
#define SL11_ISR_USBB		(0x02)	/* USB-B done */
#define SL11_ISR_BABBLE		(0x04)	/* Babble detection */
#define SL11_ISR_SOFTIMER	(0x10)	/* 1ms SOF timer */
#define SL11_ISR_INSERT		(0x20)	/* Slave Insert/Remove detection */
#define SL11_ISR_RESET		(0x40)	/* USB Reset/Resume */
#define SL11_ISR_DATA		(0x80)	/* Value of the Data+ pin */

#define SL11_REV_USBA		(0x01)	/* USB-A */
#define SL11_REV_USBB		(0x02)	/* USB-B */
#define SL11_REV_REVMASK	(0xf0)	/* HW Revision */
#define SL11_REV_REVSL11H	(0x00)	/* HW is SL11H */
#define SL11_REV_REVSL811HS	(0x10)	/* HW is SL811HS */

#define SL811_CSOF_SOFMASK	(0x3f)	/* SOF High Counter */
#define SL811_CSOF_POLARITY	(0x40)	/* Change polarity */
#define SL811_CSOF_MASTER	(0x80)	/* Master/Slave selection */

