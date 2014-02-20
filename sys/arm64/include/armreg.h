/*-
 * Copyright (c) 2013, 2014 Andrew Turner
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_ARMREG_H_
#define	_MACHINE_ARMREG_H_

/* Memory Attribute Indirection register */
/* TODO: Should this be in a pte header? */
#define	MAIR(attr, idx) ((attr) << ((idx) * 8))

/*
 * The various *PSR registers, e.g. cpsr or cpsr.
 *
 * When the exception is taken in AArch64:
 * M[4]   is 0 for AArch64 mode
 * M[3:2] is the exception level
 * M[1]   is unused
 * M[0]   is the SP select:
 *         0: always SP0
 *         1: current ELs SP
 */
#define	PSR_M_EL0t	0x00000000
#define	PSR_M_EL1t	0x00000004
#define	PSR_M_EL1h	0x00000005
#define	PSR_M_EL2t	0x00000008
#define	PSR_M_EL2h	0x00000009

#define	PSR_F		0x00000040
#define	PSR_I		0x00000080
#define	PSR_A		0x00000100
#define	PSR_D		0x00000200
#define	PSR_IL		0x00100000
#define	PSR_SS		0x00200000
#define	PSR_V		0x10000000
#define	PSR_C		0x20000000
#define	PSR_Z		0x40000000
#define	PSR_N		0x80000000

/* SCTLR bits */
#define	SCTLR_RES0	0xc8222400	/* Reserved, write 0 */
#define	SCTLR_RES1	0x30d00800	/* Reserved, write 1 */

#define	SCTLR_M		0x00000001
#define	SCTLR_A		0x00000002
#define	SCTLR_C		0x00000004
#define	SCTLR_SA	0x00000008
#define	SCTLR_SA0	0x00000010
#define	SCTLR_CP15BEN	0x00000020
#define	SCTLR_THEE	0x00000040
#define	SCTLR_ITD	0x00000080
#define	SCTLR_SED	0x00000100
#define	SCTLR_UMA	0x00000200
#define	SCTLR_I		0x00001000
#define	SCTLR_DZE	0x00004000
#define	SCTLR_UCT	0x00008000
#define	SCTLR_nTWI	0x00010000
#define	SCTLR_nTWE	0x00040000
#define	SCTLR_WXN	0x00080000
#define	SCTLR_EOE	0x01000000
#define	SCTLR_EE	0x02000000
#define	SCTLR_UCI	0x04000000

/* Translation Control Register */
#define	TCR_ASID_16	(1 << 36)

#define	TCR_IPS_SHIFT	32
#define	TCR_IPS_32BIT	(0 << TCR_IPS_SHIFT)
#define	TCR_IPS_36BIT	(1 << TCR_IPS_SHIFT)
#define	TCR_IPS_40BIT	(2 << TCR_IPS_SHIFT)
#define	TCR_IPS_42BIT	(3 << TCR_IPS_SHIFT)
#define	TCR_IPS_44BIT	(4 << TCR_IPS_SHIFT)
#define	TCR_IPS_48BIT	(5 << TCR_IPS_SHIFT)

#define	TCR_TG1_SHIFT	30
#define	TCR_TG1_16K	(1 << TCR_TG1_SHIFT)
#define	TCR_TG1_4K	(2 << TCR_TG1_SHIFT)
#define	TCR_TG1_64K	(3 << TCR_TG1_SHIFT)

#define	TCR_T1SZ_SHIFT	16
#define	TCR_T0SZ_SHIFT	0
#define	TCR_TxSZ(x)	(((x) << TCR_T1SZ_SHIFT) | ((x) << TCR_T0SZ_SHIFT))

#endif
