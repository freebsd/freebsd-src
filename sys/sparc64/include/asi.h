/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: BSDI: asi.h,v 1.3 1997/08/08 14:31:42 torek
 * $FreeBSD$
 */

#ifndef	_MACHINE_ASI_H_
#define	_MACHINE_ASI_H_

/*
 * Standard v9 asis
 */
#define	ASI_N					0x4
#define	ASI_NL					0xc
#define	ASI_AIUP				0x10
#define	ASI_AIUS				0x11
#define	ASI_AIUSL				0x19
#define	ASI_P					0x80
#define	ASI_S					0x81
#define	ASI_PNF					0x82
#define	ASI_SNF					0x83
#define	ASI_PL					0x88
#define	ASI_PNFL				0x8a
#define	ASI_SNFL				0x8b

/*
 * UltraSPARC extensions
 */
#define	ASI_PHYS_USE_EC				0x14
#define	ASI_PHYS_BYPASS_EC_WITH_EBIT		0x15
#define	ASI_PHYS_USE_EC_L			0x1c
#define	ASI_PHYS_BYPASS_EC_WITH_EBIT_L		0x1d

#define	ASI_NUCLEUS_QUAD_LDD			0x24
#define	ASI_NUCLEUS_QUAD_LDD_L			0x2c

#define	ASI_LSU_CTL_REG				0x45

#define	ASI_INTR_DISPATCH_STATUS		0x48
#define	ASI_INTR_RECEIVE			0x49
#define	ASI_UPA_CONFIG_REG			0x4a

#define	ASI_IMMU_TAG_TARGET_REG			0x50
#define	ASI_IMMU				0x50
#define		AA_IMMU_TTR			0x0
#define		AA_IMMU_SFSR			0x18
#define		AA_IMMU_TSB			0x28
#define		AA_IMMU_TAR			0x30

#define	ASI_IMMU_TSB_8KB_PTR_REG		0x51
#define	ASI_IMMU_TSB_64KB_PTR_REG		0x52
#define	ASI_ITLB_DATA_IN_REG			0x54
#define	ASI_ITLB_DATA_ACCESS_REG		0x55
#define	ASI_ITLB_TAG_READ_REG			0x56
#define	ASI_IMMU_DEMAP				0x57

#define	ASI_DMMU_TAG_TARGET_REG			0x58
#define	ASI_DMMU				0x58
#define		AA_DMMU_TTR			0x0
#define		AA_DMMU_PCXR			0x8
#define		AA_DMMU_SCXR			0x10
#define		AA_DMMU_SFSR			0x18
#define		AA_DMMU_SFAR			0x20
#define		AA_DMMU_TSB			0x28
#define		AA_DMMU_TAR			0x30
#define		AA_DMMU_VWPR			0x38
#define		AA_DMMU_PWPR			0x40

#define	ASI_DCACHE_DATA				0x46
#define	ASI_DCACHE_TAG				0x47
#define	ASI_ECACHE_TAG_DATA			0x4e

#define	ASI_DMMU_TSB_8KB_PTR_REG		0x59
#define	ASI_DMMU_TSB_64KB_PTR_REG		0x5a
#define	ASI_DMMU_TSB_DIRECT_PTR_REG 		0x5b
#define	ASI_DTLB_DATA_IN_REG			0x5c
#define	ASI_DTLB_DATA_ACCESS_REG		0x5d
#define	ASI_DTLB_TAG_READ_REG			0x5e
#define	ASI_DMMU_DEMAP				0x5f

#define	ASI_ICACHE_INSTR			0x66
#define	ASI_ICACHE_TAG				0x67
#define	ASI_ICACHE_PRE_DECODE			0x6e
#define	ASI_ICACHE_PRE_NEXT_FIELD		0x6f

#define	ASI_BLK_AUIP				0x70
#define	ASI_BLK_AIUS				0x71

#define	ASI_ECACHE_W				0x76

#define	ASI_SDB_INTR_W				0x77
#define		AA_SDB_INTR_D0			0x40
#define		AA_SDB_INTR_D1			0x50
#define		AA_SDB_INTR_D2			0x60
#define		AA_INTR_SEND			0x70

#define	ASI_BLK_AIUPL				0x78
#define	ASI_BLK_AIUSL				0x79

#define	ASI_ECACHE_R				0x7e

#define	ASI_SDB_INTR_R				0x7f

#define	ASI_BLK_COMMIT_P			0xe0
#define	ASI_BLK_COMMIT_S			0xe1
#define	ASI_BLK_P				0xf0
#define	ASI_BLK_S				0xf1
#define	ASI_BLK_PL				0xf8
#define	ASI_BLK_SL				0xf9

#endif /* !_MACHINE_ASI_H_ */
