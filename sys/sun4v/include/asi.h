/*-
 * Copyright (c) 2006 Kip Macy
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
 * $FreeBSD: src/sys/sun4v/include/asi.h,v 1.2 2006/10/09 04:45:18 kmacy Exp $
 */

#ifndef	_MACHINE_ASI_H_
#define	_MACHINE_ASI_H_

/*
 *  UltraSPARC Architecture 2005 ASIs
 */
#define ASI_N                         0x04  /* ASI_NUCLEUS                           */

#define ASI_NL                        0x0c  /* ASI_NUCLEUS_LITTLE                    */

#define ASI_AIUP                      0x10  /* ASI_AS_IF_USER_PRIMARY                */
#define ASI_AIUS                      0x11  /* ASI_AS_IF_USER_SECONDARY              */

#define ASI_REAL                      0x14
#define ASI_REAL_IO                   0x15
#define ASI_BLK_AIUP                  0x16  /* ASI_BLOCK_AS_IF_USER_PRIMARY          */
#define ASI_BLK_AIUS                  0x17  /* ASI_BLOCK_AS_IF_USER_SECONDARY        */
#define ASI_AIUPL                     0x18  /* ASI_AS_IF_USER_PRIMARY_LITTLE         */
#define ASI_AIUSL                     0x19  /* ASI_AS_IF_USER_SECONDARY_LITTLE       */

#define ASI_REAL_L                    0x1C  /* ASI_REAL_LITTLE                       */
#define ASI_REAL_IO_L                 0x1D  /* ASI_REAL_IO_LITTLE                    */
#define ASI_BLK_AIUPL                 0x1E  /* ASI_BLOCK_AS_IF_USER_PRIMARY_LITTLE   */
#define ASI_BLK_AIUSL                 0x1F  /* ASI_BLOCK_AS_IF_USER_SECONDARY_LITTLE */
#define ASI_SCRATCHPAD                0x20
#define ASI_MMU_CONTEXTID             0x21
#define ASI_LDTD_AIUP                 0x22  /* ASI_LOAD_TWIN_DW_AS_IF_USER_PRIMARY   */
#define ASI_LDSTBI_AIUP               0x22
#define ASI_LDTD_AIUS                 0x23  /* ASI_LOAD_TWIN_DW_AS_IF_USER_SECONDARY */
#define ASI_LDSTBI_AIUS               0x23
#define ASI_QUEUE                     0x25
#define ASI_LDTD_REAL                 0x26  /* ASI_LOAD_TWIN_DW_REAL                 */
#define ASI_STBI_REAL                 0x26
#define ASI_LDTD_N                    0x27  /* ASI_LOAD_TWIN_DW_NUCLEUS              */
#define ASI_LDSTBI_N                  0x27

#define ASI_LDTD_AIUPL                0x2A  /* ASI_LD_TWIN_DW_AS_IF_USER_PRIMARY_LITTLE   */
#define ASI_LDTD_AIUSL                0x2B  /* ASI_LD_TWIN_DW_AS_IF_USER_SECONDARY_LITTLE */

#define ASI_LDTD_REAL_L               0x2E  /* ASI_LOAD_TWIN_DW_REAL_LITTLE           */
#define ASI_LDTD_NL                   0x2F  /* ASI_LOAD_TWIN_DW_NUCLEUS_LITTLE        */



#define ASI_P                         0x80  /* ASI_PRIMARY                            */
#define ASI_S                         0x81  /* ASI_SECONDARY                          */
#define ASI_PNF                       0x82  /* ASI_PRIMARY_NO_FAULT                   */
#define ASI_SNF                       0x83  /* ASI_SECONDARY_NO_FAULT                 */

#define ASI_PL                        0x88  /* ASI_PRIMARY_LITTLE                     */
#define ASI_SL                        0x89  /* ASI_SECONDARY_LITTLE                   */
#define ASI_PNFL                      0x8a  /* ASI_PRIMARY_NO_FAULT_LITTLE            */
#define ASI_SNFL                      0x8b  /* ASI_SECONDARY_NO_FAULT_LITTLE          */

#define ASI_PST8_P                    0xc0
#define ASI_PST8_S                    0xc1
#define ASI_PST16_P                   0xc2
#define ASI_PST16_S                   0xc3
#define ASI_PST32_P                   0xc4
#define ASI_PST32_S                   0xc5


#define ASI_PST8_PL                   0xc8
#define ASI_PST8_SL                   0xc9
#define ASI_PST16_PL                  0xca
#define ASI_PST16_SL                  0xcb
#define ASI_PST32_PL                  0xcc
#define ASI_PST32_SL                  0xcd

#define ASI_FL8_P                     0xd0
#define ASI_FL8_S                     0xd1
#define ASI_FL16_P                    0xd2
#define ASI_FL16_S                    0xd3

#define ASI_FL8_PL                    0xd8
#define ASI_FL8_SL                    0xd9
#define ASI_FL16_PL                   0xda
#define ASI_FL16_SL                   0xdb

#define ASI_LDTD_P                    0xe2  /* ASI_LOAD_TWIN_DW_PRIMARY              */
#define ASI_LDSTBI_P                  0xe2

#define ASI_LDTD_S                    0xe3  /* ASI_LOAD_TWIN_DW_SECONDARY            */

#define ASI_LDTD_PL                   0xea  /* ASI_LOAD_TWIN_DW_PRIMARY_LITTLE       */
#define ASI_LDTD_SL                   0xeb  /* ASI_LOAD_TWIN_DW_SECONDARY_LITTLE     */

#define ASI_BLK_P                     0xf0  /* ASI_BLOCK_PRIMARY                     */
#define ASI_BLK_S                     0xf1  /* ASI_BLOCK_SECONDARY                   */

#define ASI_BLK_PL                    0xf8  /* ASI_BLOCK_PRIMARY_LITTLE              */
#define ASI_BLK_SL                    0xf9  /* ASI_BLOCK_SECONDARY_LITTLE            */



#define ASI_SCRATCHPAD_0_REG          0x00
#define ASI_SCRATCHPAD_1_REG          0x08
#define ASI_SCRATCHPAD_2_REG          0x10
#define ASI_SCRATCHPAD_3_REG          0x18
#define ASI_SCRATCHPAD_6_REG          0x30
#define ASI_SCRATCHPAD_7_REG          0x38


#define SCRATCH_REG_MMFSA             ASI_SCRATCHPAD_0_REG
#define SCRATCH_REG_PCPU              ASI_SCRATCHPAD_1_REG
#define SCRATCH_REG_HASH_KERNEL       ASI_SCRATCHPAD_2_REG
#define SCRATCH_REG_TSB_KERNEL        ASI_SCRATCHPAD_3_REG
#define SCRATCH_REG_HASH_USER         ASI_SCRATCHPAD_6_REG
#define SCRATCH_REG_TSB_USER          ASI_SCRATCHPAD_7_REG

#define MMU_CID_P                     0x08
#define MMU_CID_S                     0x10

#define CPU_MONDO_QUEUE_HEAD          0x3c0
#define CPU_MONDO_QUEUE_TAIL          0x3c8
#define DEV_MONDO_QUEUE_HEAD          0x3d0
#define DEV_MONDO_QUEUE_TAIL          0x3d8
#define RESUMABLE_ERROR_QUEUE_HEAD    0x3e0
#define RESUMABLE_ERROR_QUEUE_TAIL    0x3e8
#define NONRESUMABLE_ERROR_QUEUE_HEAD 0x3f0
#define NONRESUMABLE_ERROR_QUEUE_TAIL 0x3f8

#define Q(queue_head) (queue_head >> 4)


/*
 * sparc64 compat for the loader
 */
#define	AA_IMMU_TAR			        0x30
#define	AA_DMMU_TAR			        0x30

#define ASI_UPA_CONFIG_REG                      0x4a    /* US-I, II */
#define	ASI_IMMU				0x50
#define	ASI_ITLB_DATA_IN_REG			0x54
#define	ASI_ITLB_DATA_ACCESS_REG		0x55
#define	ASI_ITLB_TAG_READ_REG			0x56
#define	ASI_DMMU				0x58
#define	ASI_DTLB_DATA_IN_REG			0x5c
#define	ASI_DTLB_DATA_ACCESS_REG		0x5d
#define	ASI_DTLB_TAG_READ_REG			0x5e

#endif /* !_MACHINE_ASI_H_ */
