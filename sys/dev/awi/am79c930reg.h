/* $NetBSD: am79c930reg.h,v 1.3 2000/03/22 11:22:22 onoe Exp $ */
/* $FreeBSD$ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 * Device register definitions gleaned from from the AMD "Am79C930
 * PCnet(tm)-Mobile Single Chip Wireless LAN Media Access Controller"
 * data sheet, AMD Pub #20183, Rev B, amendment/0, issue date August 1997.
 *
 * As of 1999/10/23, this was available from AMD's web site in PDF
 * form.
 */


/*
 * The 79c930 contains a bus interface unit, a media access
 * controller, and a tranceiver attachment interface.
 * The MAC contains an 80188 CPU core.
 * typical devices built around this chip typically add 32k or 64k of
 * memory for buffers.
 *
 * The 80188 runs firmware which handles most of the 802.11 gorp, and
 * communicates with the host using shared data structures in this
 * memory; the specifics of the shared memory layout are not covered
 * in this source file; see <dev/ic/am80211fw.h> for details of that layer.
 */

/*
 * Device Registers
 */

#define AM79C930_IO_BASE	0
#define AM79C930_IO_SIZE	16
#define AM79C930_IO_SIZE_BIG	40
#define AM79C930_IO_ALIGN	0x40	/* am79c930 decodes lower 6bits */


#define AM79C930_GCR	0	/* General Config Register */

#define AM79C930_GCR_SWRESET	0x80	/* software reset */
#define AM79C930_GCR_CORESET	0x40	/* core reset */
#define AM79C930_GCR_DISPWDN	0x20	/* disable powerdown */
#define AM79C930_GCR_ECWAIT	0x10	/* embedded controller wait */
#define AM79C930_GCR_ECINT	0x08 	/* interrupt from embedded ctrlr */
#define AM79C930_GCR_INT2EC	0x04 	/* interrupt to embedded ctrlr */
#define AM79C930_GCR_ENECINT	0x02 	/* enable interrupts from e.c. */
#define AM79C930_GCR_DAM	0x01 	/* direct access mode (read only) */

#define AM79C930_GCR_BITS "\020\1DAM\2ENECINT\3INT2EC\4ECINT\5ECWAIT\6DISPWDN\7CORESET\010SWRESET"

#define AM79C930_BSS	1	/* Bank Switching Select register */

#define AM79C930_BSS_ECATR	0x80 	/* E.C. ALE test read */
#define AM79C930_BSS_FS		0x20 	/* Flash Select */
#define AM79C930_BSS_MBS	0x18	/* Memory Bank Select */
#define AM79C930_BSS_EIOW	0x04 	/* Expand I/O Window */
#define AM79C930_BSS_TBS	0x03 	/* TAI Bank Select */

#define AM79C930_LMA_LO	2	/* Local Memory Address register (low byte) */

#define AM79C930_LMA_HI 3	/* Local Memory Address register (high byte) */

				/* set this bit to turn off ISAPnP version */
#define AM79C930_LMA_HI_ISAPWRDWN	0x80	
 
/*
 * mmm, inconsistancy in chip documentation:
 * According to page 79--80, all four of the following are equivalent
 * and address the single byte pointed at by BSS_{FS,MBS} | LMA_{HI,LO}
 * According to tables on p63 and p67, they're the LSB through MSB
 * of a 32-bit word.
 */

#define AM79C930_IODPA		4 /* I/O Data port A */
#define AM79C930_IODPB		5 /* I/O Data port B */
#define AM79C930_IODPC	        6 /* I/O Data port C */
#define AM79C930_IODPD		7 /* I/O Data port D */


/*
 * Tranceiver Attachment Interface Registers (TIR space)
 * (omitted for now, since host access to them is for diagnostic
 * purposes only).
 */

/*
 * memory space goo.
 */

#define AM79C930_MEM_SIZE	0x8000		 /* 32k */
#define AM79C930_MEM_BASE	0x0		 /* starting at 0 */
