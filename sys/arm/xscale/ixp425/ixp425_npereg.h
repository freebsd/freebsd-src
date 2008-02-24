/*-
 * Copyright (c) 2006 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD: src/sys/arm/xscale/ixp425/ixp425_npereg.h,v 1.1 2006/11/19 23:55:23 sam Exp $
 */

/*-
 * Copyright (c) 2001-2005, Intel Corporation.
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
 * 3. Neither the name of the Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

#ifndef _IXP425_NPEREG_H_
#define _IXP425_NPEREG_H_

/* signature found as 1st word in a microcode image library */
#define IX_NPEDL_IMAGEMGR_SIGNATURE      0xDEADBEEF
/* marks end of header in a microcode image library */
#define IX_NPEDL_IMAGEMGR_END_OF_HEADER  0xFFFFFFFF

/*
 * Intel (R) IXP400 Software NPE Image ID Definition
 *
 * Definition of NPE Image ID to be passed to ixNpeDlNpeInitAndStart()
 * as input of type uint32_t which has the following fields format:
 *
 * Field		[Bit Location]
 * -----------------------------------
 * Device ID		[31 - 28]
 * NPE ID		[27 - 24]
 * NPE Functionality ID	[23 - 16]
 * Major Release Number	[15 -  8]
 * Minor Release Number	[7 - 0]
 */
#define IX_NPEDL_NPEID_FROM_IMAGEID_GET(imageId) \
    (((imageId) >> 24) & 0xf)
#define IX_NPEDL_DEVICEID_FROM_IMAGEID_GET(imageId) \
    (((imageId) >> 28) & 0xf)
#define IX_NPEDL_FUNCTIONID_FROM_IMAGEID_GET(imageId) \
    (((imageId) >> 16) & 0xff)
#define IX_NPEDL_MAJOR_FROM_IMAGEID_GET(imageId) \
    (((imageId) >> 8) & 0xff)
#define IX_NPEDL_MINOR_FROM_IMAGEID_GET(imageId) \
    (((imageId) >> 0) & 0xff)

/*
 * Instruction and Data Memory Size (in words) for each NPE 
 */
#ifndef __ixp46X
#define IX_NPEDL_INS_MEMSIZE_WORDS_NPEA     4096
#define IX_NPEDL_INS_MEMSIZE_WORDS_NPEB     2048
#define IX_NPEDL_INS_MEMSIZE_WORDS_NPEC     2048

#define IX_NPEDL_DATA_MEMSIZE_WORDS_NPEA    2048
#define IX_NPEDL_DATA_MEMSIZE_WORDS_NPEB    2048
#define IX_NPEDL_DATA_MEMSIZE_WORDS_NPEC    2048
#else
#define IX_NPEDL_INS_MEMSIZE_WORDS_NPEA     4096
#define IX_NPEDL_INS_MEMSIZE_WORDS_NPEB     4096
#define IX_NPEDL_INS_MEMSIZE_WORDS_NPEC     4096

#define IX_NPEDL_DATA_MEMSIZE_WORDS_NPEA    4096
#define IX_NPEDL_DATA_MEMSIZE_WORDS_NPEB    4096
#define IX_NPEDL_DATA_MEMSIZE_WORDS_NPEC    4096
#endif

/* BAR offsets */
#define IX_NPEDL_REG_OFFSET_EXAD             0x00000000	/* Execution Address */
#define IX_NPEDL_REG_OFFSET_EXDATA           0x00000004	/* Execution Data */
#define IX_NPEDL_REG_OFFSET_EXCTL            0x00000008	/* Execution Control */
#define IX_NPEDL_REG_OFFSET_EXCT 	     0x0000000C	/* Execution Count */
#define IX_NPEDL_REG_OFFSET_AP0	             0x00000010	/* Action Point 0 */
#define IX_NPEDL_REG_OFFSET_AP1	             0x00000014	/* Action Point 1 */
#define IX_NPEDL_REG_OFFSET_AP2	             0x00000018	/* Action Point 2 */
#define IX_NPEDL_REG_OFFSET_AP3	             0x0000001C	/* Action Point 3 */
#define IX_NPEDL_REG_OFFSET_WFIFO            0x00000020	/* Watchpoint FIFO */
#define IX_NPEDL_REG_OFFSET_WC	             0x00000024	/* Watch Count */
#define IX_NPEDL_REG_OFFSET_PROFCT           0x00000028	/* Profile Count */
#define IX_NPEDL_REG_OFFSET_STAT	     0x0000002C	/* Messaging Status */
#define IX_NPEDL_REG_OFFSET_CTL	             0x00000030	/* Messaging Control */
#define IX_NPEDL_REG_OFFSET_MBST	     0x00000034	/* Mailbox Status */
#define IX_NPEDL_REG_OFFSET_FIFO	     0x00000038	/* Message FIFO */

/*
 * Reset value for Mailbox (MBST) register
 * NOTE that if used, it should be complemented with an NPE intruction
 * to clear the Mailbox at the NPE side as well
 */
#define IX_NPEDL_REG_RESET_MBST              0x0000F0F0

#define IX_NPEDL_MASK_WFIFO_VALID            0x80000000	/* VALID bit */
#define IX_NPEDL_MASK_STAT_OFNE              0x00010000	/* OFNE bit */
#define IX_NPEDL_MASK_STAT_IFNE              0x00080000	/* IFNE bit */

/*
 * EXCTL (Execution Control) Register commands 
*/
#define IX_NPEDL_EXCTL_CMD_NPE_STEP          0x01	/* Step 1 instruction */
#define IX_NPEDL_EXCTL_CMD_NPE_START         0x02	/* Start execution */
#define IX_NPEDL_EXCTL_CMD_NPE_STOP          0x03	/* Stop execution */
#define IX_NPEDL_EXCTL_CMD_NPE_CLR_PIPE      0x04	/* Clear ins pipeline */

/*
 * Read/write operations use address in EXAD and data in EXDATA.
 */
#define IX_NPEDL_EXCTL_CMD_RD_INS_MEM        0x10	/* Read ins memory */
#define IX_NPEDL_EXCTL_CMD_WR_INS_MEM        0x11	/* Write ins memory */
#define IX_NPEDL_EXCTL_CMD_RD_DATA_MEM       0x12	/* Read data memory */
#define IX_NPEDL_EXCTL_CMD_WR_DATA_MEM       0x13	/* Write data memory */
#define IX_NPEDL_EXCTL_CMD_RD_ECS_REG        0x14	/* Read ECS register */
#define IX_NPEDL_EXCTL_CMD_WR_ECS_REG        0x15	/* Write ECS register */

#define IX_NPEDL_EXCTL_CMD_CLR_PROFILE_CNT   0x0C	/* Clear Profile Count register */


/*
 * EXCTL (Execution Control) Register status bit masks
 */
#define IX_NPEDL_EXCTL_STATUS_RUN            0x80000000
#define IX_NPEDL_EXCTL_STATUS_STOP           0x40000000
#define IX_NPEDL_EXCTL_STATUS_CLEAR          0x20000000
#define IX_NPEDL_EXCTL_STATUS_ECS_K          0x00800000	/* pipeline Klean */

/*
 * Executing Context Stack (ECS) level registers 
 */
#define IX_NPEDL_ECS_BG_CTXT_REG_0           0x00	/* reg 0 @ bg ctx */
#define IX_NPEDL_ECS_BG_CTXT_REG_1           0x01	/* reg 1 @ bg ctx */
#define IX_NPEDL_ECS_BG_CTXT_REG_2           0x02	/* reg 2 @ bg ctx */

#define IX_NPEDL_ECS_PRI_1_CTXT_REG_0        0x04	/* reg 0 @ pri 1 ctx */
#define IX_NPEDL_ECS_PRI_1_CTXT_REG_1        0x05	/* reg 1 @ pri 1 ctx */
#define IX_NPEDL_ECS_PRI_1_CTXT_REG_2        0x06	/* reg 2 @ pri 1 ctx */

#define IX_NPEDL_ECS_PRI_2_CTXT_REG_0        0x08	/* reg 0 @ pri 2 ctx */
#define IX_NPEDL_ECS_PRI_2_CTXT_REG_1        0x09	/* reg 1 @ pri 2 ctx */
#define IX_NPEDL_ECS_PRI_2_CTXT_REG_2        0x0A	/* reg 2 @ pri 2 ctx */

#define IX_NPEDL_ECS_DBG_CTXT_REG_0          0x0C	/* reg 0 @ debug ctx */
#define IX_NPEDL_ECS_DBG_CTXT_REG_1          0x0D	/* reg 1 @ debug ctx */
#define IX_NPEDL_ECS_DBG_CTXT_REG_2          0x0E	/* reg 2 @ debug ctx */

#define IX_NPEDL_ECS_INSTRUCT_REG            0x11	/* Instruction reg */

/*
 * Execution Access register reset values
 */
#define IX_NPEDL_ECS_BG_CTXT_REG_0_RESET     0xA0000000
#define IX_NPEDL_ECS_BG_CTXT_REG_1_RESET     0x01000000
#define IX_NPEDL_ECS_BG_CTXT_REG_2_RESET     0x00008000
#define IX_NPEDL_ECS_PRI_1_CTXT_REG_0_RESET  0x20000080
#define IX_NPEDL_ECS_PRI_1_CTXT_REG_1_RESET  0x01000000
#define IX_NPEDL_ECS_PRI_1_CTXT_REG_2_RESET  0x00008000
#define IX_NPEDL_ECS_PRI_2_CTXT_REG_0_RESET  0x20000080
#define IX_NPEDL_ECS_PRI_2_CTXT_REG_1_RESET  0x01000000
#define IX_NPEDL_ECS_PRI_2_CTXT_REG_2_RESET  0x00008000
#define IX_NPEDL_ECS_DBG_CTXT_REG_0_RESET    0x20000000
#define IX_NPEDL_ECS_DBG_CTXT_REG_1_RESET    0x00000000
#define IX_NPEDL_ECS_DBG_CTXT_REG_2_RESET    0x001E0000
#define IX_NPEDL_ECS_INSTRUCT_REG_RESET      0x1003C00F

/*
 * Masks used to read/write particular bits in Execution Access registers
 */

#define IX_NPEDL_MASK_ECS_REG_0_ACTIVE       0x80000000	/* Active bit */
#define IX_NPEDL_MASK_ECS_REG_0_NEXTPC       0x1FFF0000	/* NextPC bits */
#define IX_NPEDL_MASK_ECS_REG_0_LDUR         0x00000700	/* LDUR bits */

#define IX_NPEDL_MASK_ECS_REG_1_CCTXT        0x000F0000	/* NextPC bits */
#define IX_NPEDL_MASK_ECS_REG_1_SELCTXT      0x0000000F

#define IX_NPEDL_MASK_ECS_DBG_REG_2_IF       0x00100000	/* IF bit */
#define IX_NPEDL_MASK_ECS_DBG_REG_2_IE       0x00080000	/* IE bit */


/*
 * Bit-Offsets from LSB of particular bit-fields in Execution Access registers.
 */

#define IX_NPEDL_OFFSET_ECS_REG_0_NEXTPC     16 
#define IX_NPEDL_OFFSET_ECS_REG_0_LDUR        8

#define IX_NPEDL_OFFSET_ECS_REG_1_CCTXT      16
#define IX_NPEDL_OFFSET_ECS_REG_1_SELCTXT     0

/*
 * NPE core & co-processor instruction templates to load into NPE Instruction 
 * Register, for read/write of NPE register file registers.
 */

/*
 * Read an 8-bit NPE internal logical register
 * and return the value in the EXDATA register (aligned to MSB).
 * NPE Assembler instruction:  "mov8 d0, d0  &&& DBG_WrExec"
 */
#define IX_NPEDL_INSTR_RD_REG_BYTE    0x0FC00000

/*
 * Read a 16-bit NPE internal logical register
 * and return the value in the EXDATA register (aligned to MSB).
 * NPE Assembler instruction:  "mov16 d0, d0  &&& DBG_WrExec"
 */
#define IX_NPEDL_INSTR_RD_REG_SHORT   0x0FC08010

/*
 * Read a 16-bit NPE internal logical register
 * and return the value in the EXDATA register.
 * NPE Assembler instruction:  "mov32 d0, d0  &&& DBG_WrExec"
 */
#define IX_NPEDL_INSTR_RD_REG_WORD    0x0FC08210

/*
 * Write an 8-bit NPE internal logical register.
 * NPE Assembler instruction:  "mov8 d0, #0"
 */
#define IX_NPEDL_INSTR_WR_REG_BYTE    0x00004000

/*
 * Write a 16-bit NPE internal logical register.
 * NPE Assembler instruction:  "mov16 d0, #0"
 */
#define IX_NPEDL_INSTR_WR_REG_SHORT   0x0000C000

/*
 * Write a 16-bit NPE internal logical register.
 * NPE Assembler instruction:  "cprd32 d0    &&& DBG_RdInFIFO"
 */
#define IX_NPEDL_INSTR_RD_FIFO        0x0F888220    

/*
 * Reset Mailbox (MBST) register
 * NPE Assembler instruction:  "mov32 d0, d0  &&& DBG_ClearM"
 */
#define IX_NPEDL_INSTR_RESET_MBOX     0x0FAC8210


/*
 * Bit-offsets from LSB, of particular bit-fields in an NPE instruction
 */
#define IX_NPEDL_OFFSET_INSTR_SRC              4	/* src operand */
#define IX_NPEDL_OFFSET_INSTR_DEST             9	/* dest operand */
#define IX_NPEDL_OFFSET_INSTR_COPROC          18	/* coprocessor ins */

/*
 * Masks used to read/write particular bits of an NPE Instruction
 */

/**
 * Mask the bits of 16-bit data value (least-sig 5 bits) to be used in
 * SRC field of immediate-mode NPE instruction
 */
#define IX_NPEDL_MASK_IMMED_INSTR_SRC_DATA         0x1F 

/**
 * Mask the bits of 16-bit data value (most-sig 11 bits) to be used in
 * COPROC field of immediate-mode NPE instruction
 */
#define IX_NPEDL_MASK_IMMED_INSTR_COPROC_DATA      0xFFE0

/**
 * LSB offset of the bit-field of 16-bit data value (most-sig 11 bits)
 * to be used in COPROC field of immediate-mode NPE instruction
 */
#define IX_NPEDL_OFFSET_IMMED_INSTR_COPROC_DATA    5

/**
 * Number of left-shifts required to align most-sig 11 bits of 16-bit
 * data value into COPROC field of immediate-mode NPE instruction
 */
#define IX_NPEDL_DISPLACE_IMMED_INSTR_COPROC_DATA \
     (IX_NPEDL_OFFSET_INSTR_COPROC - IX_NPEDL_OFFSET_IMMED_INSTR_COPROC_DATA)

/**
 * LDUR value used with immediate-mode NPE Instructions by the NpeDl
 * for writing to NPE internal logical registers
 */
#define IX_NPEDL_WR_INSTR_LDUR                     1

/**
 * LDUR value used with NON-immediate-mode NPE Instructions by the NpeDl
 * for reading from NPE internal logical registers
 */
#define IX_NPEDL_RD_INSTR_LDUR                     0


/**
 * NPE internal Context Store registers.
 */
typedef enum
{
    IX_NPEDL_CTXT_REG_STEVT = 0,  /**< identifies STEVT   */
    IX_NPEDL_CTXT_REG_STARTPC,    /**< identifies STARTPC */
    IX_NPEDL_CTXT_REG_REGMAP,     /**< identifies REGMAP  */
    IX_NPEDL_CTXT_REG_CINDEX,     /**< identifies CINDEX  */
    IX_NPEDL_CTXT_REG_MAX         /**< Total number of Context Store registers */
} IxNpeDlCtxtRegNum;


/*
 * NPE Context Store register logical addresses
 */
#define IX_NPEDL_CTXT_REG_ADDR_STEVT      0x0000001B
#define IX_NPEDL_CTXT_REG_ADDR_STARTPC    0x0000001C
#define IX_NPEDL_CTXT_REG_ADDR_REGMAP     0x0000001E
#define IX_NPEDL_CTXT_REG_ADDR_CINDEX     0x0000001F

/*
 * NPE Context Store register reset values
 */

/**
 * Reset value of STEVT NPE internal Context Store register
 *        (STEVT = off, 0x80)
 */
#define IX_NPEDL_CTXT_REG_RESET_STEVT     0x80

/**
 * Reset value of STARTPC NPE internal Context Store register
 *        (STARTPC = 0x0000)
 */
#define IX_NPEDL_CTXT_REG_RESET_STARTPC   0x0000

/**
 * Reset value of REGMAP NPE internal Context Store register
 *        (REGMAP = d0->p0, d8->p2, d16->p4)
 */
#define IX_NPEDL_CTXT_REG_RESET_REGMAP    0x0820

/**
 * Reset value of CINDEX NPE internal Context Store register
 *        (CINDEX = 0)
 */
#define IX_NPEDL_CTXT_REG_RESET_CINDEX    0x00


/*
 * Numeric range of context levels available on an NPE
 */
#define IX_NPEDL_CTXT_NUM_MIN             0
#define IX_NPEDL_CTXT_NUM_MAX             15


/**
 * Number of Physical registers currently supported
 *        Initial NPE implementations will have a 32-word register file.
 *        Later implementations may have a 64-word register file.
 */
#define IX_NPEDL_TOTAL_NUM_PHYS_REG               32

/**
 * LSB-offset of Regmap number in Physical NPE register address, used
 *        for Physical To Logical register address mapping in the NPE
 */
#define IX_NPEDL_OFFSET_PHYS_REG_ADDR_REGMAP      1

/**
 * Mask to extract a logical NPE register address from a physical
 *        register address, used for Physical To Logical address mapping
 */
#define IX_NPEDL_MASK_PHYS_REG_ADDR_LOGICAL_ADDR   0x1

/*
 * NPE Message/Mailbox interface.
 */
#define	IX_NPESTAT	IX_NPEDL_REG_OFFSET_STAT	/* status register */
#define	IX_NPECTL	IX_NPEDL_REG_OFFSET_CTL		/* control register */
#define	IX_NPEFIFO	IX_NPEDL_REG_OFFSET_FIFO	/* FIFO register */

/* control register */
#define	IX_NPECTL_OFE		0x00010000	/* output fifo enable */
#define	IX_NPECTL_IFE		0x00020000	/* input fifo enable */
#define	IX_NPECTL_OFWE		0x01000000	/* output fifo write enable */
#define	IX_NPECTL_IFWE		0x02000000	/* input fifo write enable */

/* status register */
#define	IX_NPESTAT_OFNE		0x00010000	/* output fifo not empty */
#define	IX_NPESTAT_IFNF		0x00020000	/* input fifo not full */
#define	IX_NPESTAT_OFNF		0x00040000	/* output fifo not full */
#define	IX_NPESTAT_IFNE		0x00080000	/* input fifo not empty */
#define	IX_NPESTAT_MBINT	0x00100000	/* Mailbox interrupt */
#define	IX_NPESTAT_IFINT	0x00200000	/* input fifo interrupt */
#define	IX_NPESTAT_OFINT	0x00400000	/* output fifo interrupt */
#define	IX_NPESTAT_WFINT	0x00800000	/* watch fifo interrupt */
#endif /* _IXP425_NPEREG_H_ */
