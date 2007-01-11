/*      $NetBSD: sa11x0_ppcreg.h,v 1.2 2001/07/30 12:19:04 rjs Exp $	*/

/*-
 * Copyright (c) 2001, The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/sa11x0/sa11x0_ppcreg.h,v 1.1 2004/05/14 11:46:45 cognet Exp $
 *
 */

/* SA11[01]0 PPC (peripheral pin controller) */

/* size of I/O space */
#define SAPPC_NPORTS	13

#define SAPPC_PDR	0x00	/* pin direction register */

#define SAPPC_PSR	0x04	/* pin state register */

#define SAPPC_PAR	0x08	/* pin assignment register */
#define PAR_UPR		0x01000	/* UART pin assignment */
#define PAR_SPR		0x40000	/* SSP pin assignment */

#define SAPPC_SDR	0x0C	/* sleep mode direction register */

#define SAPPC_PFR	0x10	/* pin flag register */
#define PFR_LCD		0x00001	/* LCD controller flag */
#define PFR_SP1TX	0x01000	/* serial port 1 Tx flag */
#define PFR_SP1RX	0x02000	/* serial port 1 Rx flag */
#define PFR_SP2TX	0x04000	/* serial port 2 Tx flag */
#define PFR_SP2RX	0x08000	/* serial port 2 Rx flag */
#define PFR_SP3TX	0x10000	/* serial port 3 Tx flag */
#define PFR_SP3RX	0x20000	/* serial port 3 Rx flag */
#define PFR_SP4		0x40000	/* serial port 4 flag */

/* MCP control register 1 */
#define SAMCP_CR1	0x30	/* MCP control register 1 */
