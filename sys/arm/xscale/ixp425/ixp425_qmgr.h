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
 * $FreeBSD$
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

#ifndef ARM_XSCALE_IXP425_QMGR_H
#define	ARM_XSCALE_IXP425_QMGR_H

#define	IX_QMGR_MAX_NUM_QUEUES		64
#define	IX_QMGR_MIN_QUEUPP_QID		32

#define IX_QMGR_MIN_ENTRY_SIZE_IN_WORDS 16

/* Total size of SRAM */
#define IX_QMGR_AQM_SRAM_SIZE_IN_BYTES 0x4000

#define	IX_QMGR_Q_PRIORITY_0		0
#define	IX_QMGR_Q_PRIORITY_1		1
#define	IX_QMGR_Q_PRIORITY_2		2
#define IX_QMGR_NUM_PRIORITY_LEVELS	3	/* number of priority levels */

#define	IX_QMGR_Q_STATUS_E_BIT_MASK	0x1	/* Empty */
#define	IX_QMGR_Q_STATUS_NE_BIT_MASK	0x2	/* Nearly Empty */
#define	IX_QMGR_Q_STATUS_NF_BIT_MASK	0x4	/* Nearly Full */
#define	IX_QMGR_Q_STATUS_F_BIT_MASK	0x8	/* Full */
#define	IX_QMGR_Q_STATUS_UF_BIT_MASK	0x10	/* Underflow */
#define	IX_QMGR_Q_STATUS_OF_BIT_MASK	0x20	/* Overflow */

#define	IX_QMGR_Q_SOURCE_ID_E		0 /* Q Empty after last read */
#define	IX_QMGR_Q_SOURCE_ID_NE		1 /* Q Nearly Empty after last read */
#define	IX_QMGR_Q_SOURCE_ID_NF		2 /* Q Nearly Full after last write */
#define	IX_QMGR_Q_SOURCE_ID_F		3 /* Q Full after last write */
#define	IX_QMGR_Q_SOURCE_ID_NOT_E	4 /* Q !Empty after last write */
#define	IX_QMGR_Q_SOURCE_ID_NOT_NE	5 /* Q !Nearly Empty after last write */
#define	IX_QMGR_Q_SOURCE_ID_NOT_NF	6 /* Q !Nearly Full after last read */
#define	IX_QMGR_Q_SOURCE_ID_NOT_F	7 /* Q !Full after last read */

#define IX_QMGR_UNDERFLOW_BIT_OFFSET	0x0	/* underflow bit mask */
#define IX_QMGR_OVERFLOW_BIT_OFFSET     0x1	/* overflow bit mask */

#define IX_QMGR_QUEACC0_OFFSET		0x0000	/* q 0 access register */
#define IX_QMGR_QUEACC_SIZE		0x4/*words*/

#define IX_QMGR_QUELOWSTAT0_OFFSET	0x400	/* Q status, q's 0-7 */
#define IX_QMGR_QUELOWSTAT1_OFFSET	0x404	/* Q status, q's 8-15 */
#define IX_QMGR_QUELOWSTAT2_OFFSET	0x408	/* Q status, q's 16-23 */
#define IX_QMGR_QUELOWSTAT3_OFFSET	0x40c	/* Q status, q's 24-31 */

/* Queue status register Q status bits mask */
#define IX_QMGR_QUELOWSTAT_QUE_STS_BITS_MASK 0xF
/* Size of queue 0-31 status register */
#define IX_QMGR_QUELOWSTAT_SIZE     0x4 /*words*/
#define IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD 8	/* # status/word */

#define IX_QMGR_QUEUOSTAT0_OFFSET	0x410	/* Q UF/OF status, q's 0-15 */
#define IX_QMGR_QUEUOSTAT1_OFFSET	0x414	/* Q UF/OF status, q's 16-31 */

#define IX_QMGR_QUEUOSTAT_NUM_QUE_PER_WORD 16	/* # UF/OF status/word */

#define IX_QMGR_QUEUPPSTAT0_OFFSET	0x418	/* NE status, q's 32-63 */
#define IX_QMGR_QUEUPPSTAT1_OFFSET	0x41c	/* F status, q's 32-63 */

#define IX_QMGR_INT0SRCSELREG0_OFFSET	0x420	/* INT src select, q's 0-7 */
#define IX_QMGR_INT0SRCSELREG1_OFFSET	0x424	/* INT src select, q's 8-15 */
#define IX_QMGR_INT0SRCSELREG2_OFFSET	0x428	/* INT src select, q's 16-23 */
#define IX_QMGR_INT0SRCSELREG3_OFFSET	0x42c	/* INT src select, q's 24-31 */

#define IX_QMGR_INTSRC_NUM_QUE_PER_WORD 8	/* # INT src select/word */

#define IX_QMGR_QUEIEREG0_OFFSET	0x430	/* INT enable, q's 0-31 */
#define IX_QMGR_QUEIEREG1_OFFSET	0x434	/* INT enable, q's 32-63 */
#define IX_QMGR_QINTREG0_OFFSET		0x438	/* INT status, q's 0-31 */
#define IX_QMGR_QINTREG1_OFFSET		0x43c	/* INT status, q's 32-63 */

#define IX_QMGR_QUECONFIG_BASE_OFFSET	0x2000	/* Q config register, q 0 */

#define IX_QMGR_QUECONFIG_SIZE		0x100	/* total size of Q config regs*/

#define IX_QMGR_QUEBUFFER_SPACE_OFFSET	0x2100	/* start of SRAM */

/* Total bits in a word */
#define BITS_PER_WORD 32

/* Size of queue buffer space */
#define IX_QMGR_QUE_BUFFER_SPACE_SIZE 0x1F00

/*
 * This macro will return the address of the access register for the
 * queue  specified by qId
 */
#define IX_QMGR_Q_ACCESS_ADDR_GET(qId)\
        (((qId) * (IX_QMGR_QUEACC_SIZE * sizeof(uint32_t)))\
	 + IX_QMGR_QUEACC0_OFFSET)

/*
 * Bit location of bit-3 of INT0SRCSELREG0 register to enabled
 * sticky interrupt register.
 */
#define IX_QMGR_INT0SRCSELREG0_BIT3 3

/*
 * These defines are the bit offsets of the various fields of
 * the queue configuration register.
 */
#if 0
#define IX_QMGR_Q_CONFIG_WRPTR_OFFSET       0x00
#define IX_QMGR_Q_CONFIG_RDPTR_OFFSET       0x07
#define IX_QMGR_Q_CONFIG_BADDR_OFFSET       0x0E
#define IX_QMGR_Q_CONFIG_ESIZE_OFFSET       0x16
#define IX_QMGR_Q_CONFIG_BSIZE_OFFSET       0x18
#define IX_QMGR_Q_CONFIG_NE_OFFSET          0x1A
#define IX_QMGR_Q_CONFIG_NF_OFFSET          0x1D

#define IX_QMGR_NE_NF_CLEAR_MASK            0x03FFFFFF
#define IX_QMGR_NE_MASK                     0x7
#define IX_QMGR_NF_MASK                     0x7
#define IX_QMGR_SIZE_MASK                   0x3
#define IX_QMGR_ENTRY_SIZE_MASK             0x3
#define IX_QMGR_BADDR_MASK                  0x003FC000
#define IX_QMGR_RDPTR_MASK                  0x7F
#define IX_QMGR_WRPTR_MASK                  0x7F
#define IX_QMGR_RDWRPTR_MASK                0x00003FFF
#else
#define IX_QMGR_Q_CONFIG_WRPTR_OFFSET       0
#define IX_QMGR_WRPTR_MASK                  0x7F
#define IX_QMGR_Q_CONFIG_RDPTR_OFFSET       7
#define IX_QMGR_RDPTR_MASK                  0x7F
#define IX_QMGR_Q_CONFIG_BADDR_OFFSET       14
#define IX_QMGR_BADDR_MASK                  0x3FC000	/* XXX not used */
#define IX_QMGR_Q_CONFIG_ESIZE_OFFSET       22
#define IX_QMGR_ENTRY_SIZE_MASK             0x3
#define IX_QMGR_Q_CONFIG_BSIZE_OFFSET       24
#define IX_QMGR_SIZE_MASK                   0x3
#define IX_QMGR_Q_CONFIG_NE_OFFSET          26
#define IX_QMGR_NE_MASK                     0x7
#define IX_QMGR_Q_CONFIG_NF_OFFSET          29
#define IX_QMGR_NF_MASK                     0x7

#define IX_QMGR_RDWRPTR_MASK                0x00003FFF
#define IX_QMGR_NE_NF_CLEAR_MASK            0x03FFFFFF
#endif

#define IX_QMGR_BASE_ADDR_16_WORD_ALIGN     64
#define IX_QMGR_BASE_ADDR_16_WORD_SHIFT     6

#define IX_QMGR_AQM_ADDRESS_SPACE_SIZE_IN_WORDS 0x1000

/* Base address of AQM SRAM */
#define IX_QMGR_AQM_SRAM_BASE_ADDRESS_OFFSET \
((IX_QMGR_QUECONFIG_BASE_OFFSET) + (IX_QMGR_QUECONFIG_SIZE))

/* Min buffer size used for generating buffer size in QUECONFIG */
#define IX_QMGR_MIN_BUFFER_SIZE 16

/* Reset values of QMgr hardware registers */
#define IX_QMGR_QUELOWSTAT_RESET_VALUE    0x33333333
#define IX_QMGR_QUEUOSTAT_RESET_VALUE     0x00000000
#define IX_QMGR_QUEUPPSTAT0_RESET_VALUE   0xFFFFFFFF
#define IX_QMGR_QUEUPPSTAT1_RESET_VALUE   0x00000000
#define IX_QMGR_INT0SRCSELREG_RESET_VALUE 0x00000000
#define IX_QMGR_QUEIEREG_RESET_VALUE      0x00000000
#define IX_QMGR_QINTREG_RESET_VALUE       0xFFFFFFFF
#define IX_QMGR_QUECONFIG_RESET_VALUE     0x00000000

#define IX_QMGR_QUELOWSTAT_BITS_PER_Q \
	(BITS_PER_WORD/IX_QMGR_QUELOWSTAT_NUM_QUE_PER_WORD)

#define IX_QMGR_QUELOWSTAT_QID_MASK 0x7
#define IX_QMGR_Q_CONFIG_ADDR_GET(qId)\
        (((qId) * sizeof(uint32_t)) + IX_QMGR_QUECONFIG_BASE_OFFSET)

#define IX_QMGR_ENTRY1_OFFSET 0
#define IX_QMGR_ENTRY2_OFFSET 1
#define IX_QMGR_ENTRY4_OFFSET 3

typedef void qconfig_hand_t(int, void *);

int	ixpqmgr_qconfig(int qId, int qSizeInWords, int ne, int nf, int srcSel,
	    qconfig_hand_t *cb, void *cbarg);
int	ixpqmgr_qwrite(int qId, uint32_t entry);
int	ixpqmgr_qread(int qId, uint32_t *entry);
int	ixpqmgr_qreadm(int qId, uint32_t n, uint32_t *p);
uint32_t ixpqmgr_getqstatus(int qId);
uint32_t ixpqmgr_getqconfig(int qId);
void	ixpqmgr_notify_enable(int qId, int srcSel);
void	ixpqmgr_notify_disable(int qId);
void	ixpqmgr_dump(void);

#endif /* ARM_XSCALE_IXP425_QMGR_H */
