/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:36:43  rpd
 */
/*
  Copyright 1988, 1989, 1990, 1991, 1992
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/pc98/boot2/table.c,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $");

#include "boot.h"

/*  Segment Descriptor
 *
 * 31          24         19   16                 7           0
 * ------------------------------------------------------------
 * |             | |B| |A|       | |   |1|0|E|W|A|            |
 * | BASE 31..24 |G|/|0|V| LIMIT |P|DPL|  TYPE   | BASE 23:16 |
 * |             | |D| |L| 19..16| |   |1|1|C|R|A|            |
 * ------------------------------------------------------------
 * |                             |                            |
 * |        BASE 15..0           |       LIMIT 15..0          |
 * |                             |                            |
 * ------------------------------------------------------------
 */

struct seg_desc {
	unsigned short	limit_15_0;
	unsigned short	base_15_0;
	unsigned char	base_23_16;
	unsigned char	p_dpl_type;
	unsigned char	g_b_a_limit;
	unsigned char	base_31_24;
	};

#define RUN	0		/* not really 0, but filled in at boot time */

struct seg_desc	Gdt[] = {
	{0x0, 0x0, 0x0, 0x0, 0x0, 0x0},		/* 0x0 : null */
	{0xFFFF, 0x0, 0x0, 0x9F, 0xCF, 0x0},	/* 0x08 : kernel code */
			/* 0x9E? */
	{0xFFFF, 0x0, 0x0, 0x93, 0xCF, 0x0},	/* 0x10 : kernel data */
			/* 0x92? */
	{0xFFFF, RUN, RUN, 0x9E, 0x40, 0x0},	/* 0x18 : boot code */
	/*
	 * The limit of boot data should be more than or equal to 0x9FFFF
	 * for saving BIOS parameter and EPSON machine ID into 2'nd T-VRAM,
	 * because base address is normally 0x10000.
	 */
	{0xFFFF, RUN, RUN, 0x92, 0x4F, 0x0},	/* 0x20 : boot data */
	{0xFFFF, RUN, RUN, 0x9E, 0x0, 0x0},	/* 0x28 : boot code, 16 bits */
	{0xFFFF, 0x0, 0x0, 0x92, 0x0, 0x0},	/* 0x30 : boot data, 16 bits */
#ifdef BDE_DEBUGGER
	/* More for bdb. */
	{},					/* BIOS_TMP_INDEX = 7 : null */
	{},					/* TSS_INDEX = 8 : null */
	{0xFFFF, 0x0, 0x0, 0xB2, 0x40, 0x0},	/* DS_286_INDEX = 9 */
	{0xFFFF, 0x0, 0x0, 0xB2, 0x40, 0x0},	/* ES_286_INDEX = 10 */
	{},					/* Unused = 11 : null */
	{0x7FFF, 0x8000, 0xB, 0xB2, 0x40, 0x0},	/* COLOR_INDEX = 12 */
	{0x7FFF, 0x0, 0xB, 0xB2, 0x40, 0x0},	/* MONO_INDEX = 13 */
	{0xFFFF, RUN, RUN, 0x9A, 0x40, 0x0},	/* DB_CS_INDEX = 14 */
	{0xFFFF, RUN, RUN, 0x9A, 0x0, 0x0},	/* DB_CS16_INDEX = 15 */
	{0xFFFF, RUN, RUN, 0x92, 0x40, 0x0},	/* DB_DS_INDEX = 16 */
	{8*18-1, RUN, RUN, 0x92, 0x40, 0x0},	/* GDT_INDEX = 17 */
#endif /* BDE_DEBUGGER */
};

#ifdef BDE_DEBUGGER
struct idt_desc {
	unsigned short	entry_15_0;
	unsigned short	selector;
	unsigned char	padding;
	unsigned char	p_dpl_type;
	unsigned short	entry_31_16;
};

struct idt_desc	Idt[] = {
	{},					/* Null (int 0) */
	{RUN, 0x70, 0, 0x8E, 0},		/* DEBUG_VECTOR = 1 */
	{},					/* Null (int 2) */
	{RUN, 0x70, 0, 0xEE, 0},		/* BREAKPOINT_VECTOR = 3 */
};
#endif /* BDE_DEBUGGER */

struct pseudo_desc {
	unsigned short	limit;
	unsigned short	base_low;
	unsigned short	base_high;
	};

struct pseudo_desc Gdtr = { sizeof Gdt - 1, RUN, RUN };
#ifdef BDE_DEBUGGER
struct pseudo_desc Idtr_prot = { sizeof Idt - 1, RUN, RUN };
struct pseudo_desc Idtr_real = { 0x400 - 1, 0x0, 0x0 };
#endif

/*
 * All initialized data is defined in one file to reduce space wastage from
 * fragmentation.
 */
char *devs[] = { "wd", "dk", "fd", "wt", "da", "dk", "fd", 0 };
unsigned tw_chars = 0x5C2D2F7C;	/* "\-/|" */
