/*
 * Copyright (c) 1998 HAYAKAWA Koichi.  All rights reserved.
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
 *	This product includes software developed by the author.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* $FreeBSD: src/sys/dev/cardbus/cardbusreg.h,v 1.1 1999/11/18 07:21:50 imp Exp $ */

typedef u_int32_t cardbusreg_t;
typedef int cardbus_intr_line_t;

typedef void *cardbus_chipset_tag_t;
typedef int cardbus_intr_handle_t;

typedef u_int16_t cardbus_vendor_id_t;
typedef u_int16_t cardbus_product_id_t;

#define	CARDBUS_ID_REG			0x00

#  define CARDBUS_VENDOR_SHIFT  0
#  define CARDBUS_VENDOR_MASK   0xffff
#  define CARDBUS_VENDOR(id) \
	    (((id) >> CARDBUS_VENDOR_SHIFT) & CARDBUS_VENDOR_MASK)

#  define CARDBUS_PRODUCT_SHIFT  16
#  define CARDBUS_PRODUCT_MASK   0xffff
#  define CARDBUS_PRODUCT(id) \
	    (((id) >> CARDBUS_PRODUCT_SHIFT) & CARDBUS_PRODUCT_MASK)


#define	CARDBUS_COMMAND_STATUS_REG  0x04

#  define CARDBUS_COMMAND_IO_ENABLE     0x00000001
#  define CARDBUS_COMMAND_MEM_ENABLE    0x00000002
#  define CARDBUS_COMMAND_MASTER_ENABLE 0x00000004


#define CARDBUS_CLASS_REG       0x08

/* BIST, Header Type, Latency Timer, Cache Line Size */
#define CARDBUS_BHLC_REG        0x0c

#define	CARDBUS_BIST_SHIFT        24
#define	CARDBUS_BIST_MASK       0xff
#define	CARDBUS_BIST(bhlcr) \
	    (((bhlcr) >> CARDBUS_BIST_SHIFT) & CARDBUS_BIST_MASK)

#define	CARDBUS_HDRTYPE_SHIFT     16
#define	CARDBUS_HDRTYPE_MASK    0xff
#define	CARDBUS_HDRTYPE(bhlcr) \
	    (((bhlcr) >> CARDBUS_HDRTYPE_SHIFT) & CARDBUS_HDRTYPE_MASK)

#define	CARDBUS_HDRTYPE_TYPE(bhlcr) \
	    (CARDBUS_HDRTYPE(bhlcr) & 0x7f)
#define	CARDBUS_HDRTYPE_MULTIFN(bhlcr) \
	    ((CARDBUS_HDRTYPE(bhlcr) & 0x80) != 0)

#define	CARDBUS_LATTIMER_SHIFT      8
#define	CARDBUS_LATTIMER_MASK    0xff
#define	CARDBUS_LATTIMER(bhlcr) \
	    (((bhlcr) >> CARDBUS_LATTIMER_SHIFT) & CARDBUS_LATTIMER_MASK)

#define	CARDBUS_CACHELINE_SHIFT     0
#define	CARDBUS_CACHELINE_MASK   0xff
#define	CARDBUS_CACHELINE(bhlcr) \
	    (((bhlcr) >> CARDBUS_CACHELINE_SHIFT) & CARDBUS_CACHELINE_MASK)


/* Base Resisters */
#define CARDBUS_BASE0_REG  0x10
#define CARDBUS_BASE1_REG  0x14
#define CARDBUS_BASE2_REG  0x18
#define CARDBUS_BASE3_REG  0x1C
#define CARDBUS_BASE4_REG  0x20
#define CARDBUS_BASE5_REG  0x24
#define CARDBUS_CIS_REG    0x28
#  define CARDBUS_CIS_ASIMASK 0x07
#  define CARDBUS_CIS_ADDRMASK 0x0ffffff8

#define	CARDBUS_INTERRUPT_REG   0x3c

