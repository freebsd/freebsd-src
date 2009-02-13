/*-
 * Copyright (c) KATO Takenori, 1996.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

/*
 * PC-9801 specific definitions for DP8390/SMC8216 NICs.
 */

/*
 * Vendor types
 */
#define	ED_VENDOR_MISC		0xf0		/* others */

/*
 * Card types.
 *
 * Type  Card
 * 0x00  Allied Telesis CenterCom LA-98-T / SMC EtherEZ98.
 * 0x10  ** RESERVED **
 * 0x20  PLANET SMART COM 98 EN-2298 / ELECOM LANEED LD-BDN[123]A.
 * 0x30  MELCO EGY-98 / Contec C-NET(98)E-A/L-A.
 * 0x40  MELCO LGY-98, IND-SP, IND-SS / MACNICA NE2098(XXX).
 * 0x50  ICM DT-ET-25, DT-ET-T5, IF-2766ET, IF-2771ET /
 *       D-Link DE-298P{T,CAT}, DE-298{T,TP,CAT}.
 * 0x60  Allied Telesis SIC-98.
 * 0x70  ** RESERVED **
 * 0x80  NEC PC-9801-108.
 * 0x90  IO-DATA LA-98 / NEC PC-9801-77.
 * 0xa0  Contec C-NET(98).
 * 0xb0  Contec C-NET(98)E/L.
 * 0xc0  ** RESERVED **
 * 0xd0  Networld EC/EP-98X.
 * 0xe0  Soliton SB-9801 / Fujikura FN-9801 / Networld EC/EP-98S.
 * 0xf0  NextCom NC5098.
 */
#define	ED_TYPE98_BASE		0x80

#define	ED_TYPE98_GENERIC	0x80
#define	ED_TYPE98_BDN		0x82
#define	ED_TYPE98_EGY		0x83
#define	ED_TYPE98_LGY		0x84
#define	ED_TYPE98_ICM		0x85
#define	ED_TYPE98_SIC		0x86
#define	ED_TYPE98_108		0x88
#define	ED_TYPE98_LA98		0x89
#define	ED_TYPE98_CNET98	0x8a
#define	ED_TYPE98_CNET98EL	0x8b
#define	ED_TYPE98_NW98X		0x8d
#define	ED_TYPE98_SB98		0x8e
#define	ED_TYPE98_NC5098	0x8f

#define	ED_TYPE98(x)	(((x & 0xffff0000) >> 20) | ED_TYPE98_BASE)
#define	ED_TYPE98SUB(x)	((x & 0xf0000) >> 16)

/*
 * 		Definitions for C-NET(98) serise
 */
/*
 * Initial Register(on board JP1)
 */
#define	ED_CNET98_INIT		0xaaed		/* default */
#define	ED_CNET98_INIT2		0x55ed		/* another setting */

#define	ED_CNET98EL_PAGE_OFFSET	0x0000	/* Page offset for NIC access to mem */

/*
 *		Definitions for Soliton SB-9801
 */
/*
 * I/O port select register
 */
#define	ED_SB98_IO_INHIBIT	0x0040	/* XXX - shares printer port! */

/*
 *		Definitions for SMC EtherEZ98(SMC8498BTA)
 */
#define	ED_EZ98_NIC_OFFSET	0x100		/* I/O base offset to NIC */
#define	ED_EZ98_ASIC_OFFSET	0		/* I/O base offset to ASIC */
/*
 * XXX - The I/O address range is fragmented in the EtherEZ98;
 *      it occupies 16*2 I/O addresses, by the way.
 */
#define	ED_EZ98_IO_PORTS	16		/* # of i/o addresses used */
