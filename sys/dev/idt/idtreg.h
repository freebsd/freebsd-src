/*-
 * Copyright (c) 2000, 2001 Richard Hodges and Matriplex, inc.
 * All rights reserved.
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
 *	This product includes software developed by Matriplex, inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 ******************************************************************************
 *
 * This driver is derived from the Nicstar driver by Mark Tinguely, and
 * some of the original driver still exists here.  Those portions are...
 *   Copyright (c) 1996, 1997, 1998, 1999 Mark Tinguely
 *   All rights reserved.
 *
 ******************************************************************************
 *
 * $FreeBSD: src/sys/dev/idt/idtreg.h,v 1.3 2007/01/09 04:26:51 imp Exp $
 */

#define	IDT_VERSION	"IDT 1.101"
#define	CBR_VERBOSE	1	/* show CBR time slots */

#define	IDT_MAX_CBRSLOTS 2100	/* no smaller than value assigned to card */
#define	IDT_MAX_CBRQUEUE  64	/* max number of CBR connections: 1k each */

/* AAL types */
#define	IDTAAL0		0
#define	IDTAAL1		1
#define	IDTAAL3_4	3
#define	IDTAAL5		5

#define	NICCBR	1
#define	NICVBR	2
#define	NICABR	3
#define	NICUBR	4

/* NICStAR Operation Registers */
#define	REGCMD	0x10		/* command          w */
#define	REGCFG	0x14		/* configuration  r/w */
#define	REGSTAT	0x18		/* status         r/w */
#define	REGRSQB	0x1c		/* RSQ base         w */
#define	REGRSQT	0x20		/* RSQ tail         r */
#define	REGRSQH	0x24		/* RSQ head         w */
#define	REGCDC	0x28		/* cell drop cnt  r/c */
#define	REGVPEC	0x2c		/* vci/vpi er cnt r/c */
#define	REGICC	0x30		/* invalid cell   r/c */
#define	REGRAWT	0x34		/* raw cell tail    r */
#define	REGTMR	0x38		/* timer            r */
#define	REGTSTB	0x3c		/* TST base       r/w */
#define	REGTSQB	0x40		/* TSQ base         w */
#define	REGTSQT	0x44		/* TSQ tail         r */
#define	REGTSQH	0x48		/* TSQ head         w */
#define	REGGP	0x4c		/* general purp   r/w */
#define	REGVMSK	0x50		/* vci/vpi mask     w */
