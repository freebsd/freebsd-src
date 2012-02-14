/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD
 * $FreeBSD$
 */
#ifndef _RMI_MSGRING_H_
#define _RMI_MSGRING_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <machine/cpuregs.h>
#include <machine/cpufunc.h>
#include <mips/rmi/rmi_mips_exts.h>

#define	MSGRNG_TX_BUF_REG	0
#define	MSGRNG_RX_BUF_REG	1
#define	MSGRNG_MSG_STATUS_REG	2
#define	MSGRNG_MSG_CONFIG_REG	3
#define MSGRNG_MSG_BUCKSIZE_REG	4

#define	MSGRNG_CC_0_REG		16
#define	MSGRNG_CC_1_REG		17
#define	MSGRNG_CC_2_REG		18
#define	MSGRNG_CC_3_REG		19
#define	MSGRNG_CC_4_REG		20
#define	MSGRNG_CC_5_REG		21
#define	MSGRNG_CC_6_REG		22
#define	MSGRNG_CC_7_REG		23
#define	MSGRNG_CC_8_REG		24
#define	MSGRNG_CC_9_REG		25
#define	MSGRNG_CC_10_REG	26
#define	MSGRNG_CC_11_REG	27
#define	MSGRNG_CC_12_REG	28
#define	MSGRNG_CC_13_REG	29
#define	MSGRNG_CC_14_REG	30
#define	MSGRNG_CC_15_REG	31

/* Station IDs */
#define	MSGRNG_STNID_CPU0	0x00
#define	MSGRNG_STNID_CPU1	0x08
#define	MSGRNG_STNID_CPU2	0x10
#define	MSGRNG_STNID_CPU3	0x18
#define	MSGRNG_STNID_CPU4	0x20
#define	MSGRNG_STNID_CPU5	0x28
#define	MSGRNG_STNID_CPU6	0x30
#define	MSGRNG_STNID_CPU7	0x38
#define	MSGRNG_STNID_XGS0_TX	64
#define	MSGRNG_STNID_XMAC0_00_TX	64
#define	MSGRNG_STNID_XMAC0_01_TX	65
#define	MSGRNG_STNID_XMAC0_02_TX	66
#define	MSGRNG_STNID_XMAC0_03_TX	67
#define	MSGRNG_STNID_XMAC0_04_TX	68
#define	MSGRNG_STNID_XMAC0_05_TX	69
#define	MSGRNG_STNID_XMAC0_06_TX	70
#define	MSGRNG_STNID_XMAC0_07_TX	71
#define	MSGRNG_STNID_XMAC0_08_TX	72
#define	MSGRNG_STNID_XMAC0_09_TX	73
#define	MSGRNG_STNID_XMAC0_10_TX	74
#define	MSGRNG_STNID_XMAC0_11_TX	75
#define	MSGRNG_STNID_XMAC0_12_TX	76
#define	MSGRNG_STNID_XMAC0_13_TX	77
#define	MSGRNG_STNID_XMAC0_14_TX	78
#define	MSGRNG_STNID_XMAC0_15_TX	79

#define	MSGRNG_STNID_XGS1_TX		80
#define	MSGRNG_STNID_XMAC1_00_TX	80
#define	MSGRNG_STNID_XMAC1_01_TX	81
#define	MSGRNG_STNID_XMAC1_02_TX	82
#define	MSGRNG_STNID_XMAC1_03_TX	83
#define	MSGRNG_STNID_XMAC1_04_TX	84
#define	MSGRNG_STNID_XMAC1_05_TX	85
#define	MSGRNG_STNID_XMAC1_06_TX	86
#define	MSGRNG_STNID_XMAC1_07_TX	87
#define	MSGRNG_STNID_XMAC1_08_TX	88
#define	MSGRNG_STNID_XMAC1_09_TX	89
#define	MSGRNG_STNID_XMAC1_10_TX	90
#define	MSGRNG_STNID_XMAC1_11_TX	91
#define	MSGRNG_STNID_XMAC1_12_TX	92
#define	MSGRNG_STNID_XMAC1_13_TX	93
#define	MSGRNG_STNID_XMAC1_14_TX	94
#define	MSGRNG_STNID_XMAC1_15_TX	95

#define	MSGRNG_STNID_GMAC		96
#define	MSGRNG_STNID_GMACJFR_0		96
#define	MSGRNG_STNID_GMACRFR_0		97
#define	MSGRNG_STNID_GMACTX0		98
#define	MSGRNG_STNID_GMACTX1		99
#define	MSGRNG_STNID_GMACTX2		100
#define	MSGRNG_STNID_GMACTX3		101
#define	MSGRNG_STNID_GMACJFR_1		102
#define	MSGRNG_STNID_GMACRFR_1		103

#define	MSGRNG_STNID_DMA		104
#define	MSGRNG_STNID_DMA_0		104
#define	MSGRNG_STNID_DMA_1		105
#define	MSGRNG_STNID_DMA_2		106
#define	MSGRNG_STNID_DMA_3		107

#define	MSGRNG_STNID_XGS0FR		112
#define	MSGRNG_STNID_XMAC0JFR		112
#define	MSGRNG_STNID_XMAC0RFR		113

#define	MSGRNG_STNID_XGS1FR		114
#define	MSGRNG_STNID_XMAC1JFR		114
#define	MSGRNG_STNID_XMAC1RFR		115
#define	MSGRNG_STNID_SEC		120
#define	MSGRNG_STNID_SEC0		120
#define	MSGRNG_STNID_SEC1		121
#define	MSGRNG_STNID_SEC2		122
#define	MSGRNG_STNID_SEC3		123
#define	MSGRNG_STNID_PK0		124
#define	MSGRNG_STNID_SEC_RSA		124
#define	MSGRNG_STNID_SEC_RSVD0		125
#define	MSGRNG_STNID_SEC_RSVD1		126
#define	MSGRNG_STNID_SEC_RSVD2		127

#define	MSGRNG_STNID_GMAC1		80
#define	MSGRNG_STNID_GMAC1_FR_0		81
#define	MSGRNG_STNID_GMAC1_TX0		82
#define	MSGRNG_STNID_GMAC1_TX1		83
#define	MSGRNG_STNID_GMAC1_TX2		84
#define	MSGRNG_STNID_GMAC1_TX3		85
#define	MSGRNG_STNID_GMAC1_FR_1		87
#define	MSGRNG_STNID_GMAC0		96
#define	MSGRNG_STNID_GMAC0_FR_0		97
#define	MSGRNG_STNID_GMAC0_TX0		98
#define	MSGRNG_STNID_GMAC0_TX1		99
#define	MSGRNG_STNID_GMAC0_TX2		100
#define	MSGRNG_STNID_GMAC0_TX3		101
#define	MSGRNG_STNID_GMAC0_FR_1		103
#define	MSGRNG_STNID_CMP_0		108
#define	MSGRNG_STNID_CMP_1		109
#define	MSGRNG_STNID_CMP_2		110
#define	MSGRNG_STNID_CMP_3		111
#define	MSGRNG_STNID_PCIE_0		116
#define	MSGRNG_STNID_PCIE_1		117
#define	MSGRNG_STNID_PCIE_2		118
#define	MSGRNG_STNID_PCIE_3		119
#define	MSGRNG_STNID_XLS_PK0		121

#define	MSGRNG_CODE_MAC		0
#define	MSGRNG_CODE_XGMAC	2
#define	MSGRNG_CODE_SEC		0
#define	MSGRNG_CODE_BOOT_WAKEUP	200
#define	MSGRNG_CODE_SPI4	3

#define	msgrng_read_status()	read_c2_register32(MSGRNG_MSG_STATUS_REG, 0)
#define	msgrng_read_config()	read_c2_register32(MSGRNG_MSG_CONFIG_REG, 0)
#define	msgrng_write_config(v)	write_c2_register32(MSGRNG_MSG_CONFIG_REG, 0, v)
#define	msgrng_read_bucksize(b)	read_c2_register32(MSGRNG_MSG_BUCKSIZE_REG, b)
#define	msgrng_write_bucksize(b, v)	write_c2_register32(MSGRNG_MSG_BUCKSIZE_REG, b, v)
#define	msgrng_read_cc(r, s)	read_c2_register32(r, s)
#define	msgrng_write_cc(r, v, s)	write_c2_register32(r, s, v)

#define	msgrng_load_rx_msg0()	read_c2_register64(MSGRNG_RX_BUF_REG, 0)
#define	msgrng_load_rx_msg1()	read_c2_register64(MSGRNG_RX_BUF_REG, 1)
#define	msgrng_load_rx_msg2()	read_c2_register64(MSGRNG_RX_BUF_REG, 2)
#define	msgrng_load_rx_msg3()	read_c2_register64(MSGRNG_RX_BUF_REG, 3)

#define	msgrng_load_tx_msg0(v)	write_c2_register64(MSGRNG_TX_BUF_REG, 0, v)
#define	msgrng_load_tx_msg1(v)	write_c2_register64(MSGRNG_TX_BUF_REG, 1, v)
#define	msgrng_load_tx_msg2(v)	write_c2_register64(MSGRNG_TX_BUF_REG, 2, v)
#define	msgrng_load_tx_msg3(v)	write_c2_register64(MSGRNG_TX_BUF_REG, 3, v)

static __inline void 
msgrng_send(unsigned int stid)
{
	__asm__ volatile (
	    ".set	push\n"
	    ".set	noreorder\n"
	    "move	$8, %0\n"
	    "c2		0x80001\n"	/* msgsnd $8 */
	    ".set	pop\n"
	    :: "r" (stid): "$8"
	);
}

static __inline void 
msgrng_receive(unsigned int pri)
{
	__asm__ volatile (
	    ".set	push\n"
	    ".set	noreorder\n"
	    "move	$8, %0\n"
	    "c2		0x80002\n"    /* msgld $8 */
	    ".set	pop\n"
	    :: "r" (pri): "$8"
	);
}

static __inline void 
msgrng_wait(unsigned int mask)
{
	__asm__ volatile (
	    ".set	push\n"
	    ".set	noreorder\n"
	    "move	$8, %0\n"
	    "c2		0x80003\n"    /* msgwait $8 */
	    ".set	pop\n"
	    :: "r" (mask): "$8"
	);
}

static __inline uint32_t
msgrng_access_enable(void)
{
	uint32_t sr = mips_rd_status();

	mips_wr_status((sr & ~MIPS_SR_INT_IE) | MIPS_SR_COP_2_BIT);
	return (sr);
}

static __inline void
msgrng_restore(uint32_t sr)
{

	mips_wr_status(sr);
}

struct msgrng_msg {
	uint64_t msg0;
	uint64_t msg1;
	uint64_t msg2;
	uint64_t msg3;
};

static __inline int
message_send(unsigned int size, unsigned int code,
    unsigned int stid, struct msgrng_msg *msg)
{
	unsigned int dest = 0;
	unsigned long long status = 0;
	int i = 0;

	/* 
	 * Make sure that all the writes pending at the cpu are flushed.
	 * Any writes pending on CPU will not be see by devices. L1/L2
	 * caches are coherent with IO, so no cache flush needed.
	 */ 
	__asm __volatile ("sync");

	/* Load TX message buffers */
	msgrng_load_tx_msg0(msg->msg0);
	msgrng_load_tx_msg1(msg->msg1);
	msgrng_load_tx_msg2(msg->msg2);
	msgrng_load_tx_msg3(msg->msg3);
	dest = ((size - 1) << 16) | (code << 8) | stid;

	/*
	 * Retry a few times on credit fail, this should be a 
	 * transient condition, unless there is a configuration
	 * failure, or the receiver is stuck.
	 */
	for (i = 0; i < 8; i++) {
		msgrng_send(dest);
		status = msgrng_read_status();
		KASSERT((status & 0x2) == 0, ("Send pending fail!"));
		if ((status & 0x4) == 0)
			return (0);
	}

	/* If there is a credit failure, return error */
	return (status & 0x06);
}

static __inline int 
message_receive(int bucket, int *size, int *code, int *stid,
    struct msgrng_msg *msg)
{
	uint32_t status = 0, tmp = 0;
       
	msgrng_receive(bucket); 

	/* wait for load pending to clear */
	do {
           status = msgrng_read_status();
	} while ((status & 0x08) != 0);

	/* receive error bits */
	tmp = status & 0x30;
	if (tmp != 0)
		return (tmp);

	*size = ((status & 0xc0) >> 6) + 1;
	*code = (status & 0xff00) >> 8;
	*stid = (status & 0x7f0000) >> 16;
	msg->msg0 = msgrng_load_rx_msg0();
	msg->msg1 = msgrng_load_rx_msg1();
	msg->msg2 = msgrng_load_rx_msg2();
	msg->msg3 = msgrng_load_rx_msg3();
	return (0);
}

#define	MSGRNG_STN_RX_QSIZE	256
#define	MSGRNG_NSTATIONS	128
#define	MSGRNG_CORE_NBUCKETS	8

struct stn_cc {
	unsigned short counters[16][8];
};

struct bucket_size {
	unsigned short bucket[MSGRNG_NSTATIONS];
};

extern struct bucket_size bucket_sizes;

extern struct stn_cc cc_table_cpu_0;
extern struct stn_cc cc_table_cpu_1;
extern struct stn_cc cc_table_cpu_2;
extern struct stn_cc cc_table_cpu_3;
extern struct stn_cc cc_table_cpu_4;
extern struct stn_cc cc_table_cpu_5;
extern struct stn_cc cc_table_cpu_6;
extern struct stn_cc cc_table_cpu_7;
extern struct stn_cc cc_table_xgs_0;
extern struct stn_cc cc_table_xgs_1;
extern struct stn_cc cc_table_gmac;
extern struct stn_cc cc_table_dma;
extern struct stn_cc cc_table_sec;

extern struct bucket_size xls_bucket_sizes;

extern struct stn_cc xls_cc_table_cpu_0;
extern struct stn_cc xls_cc_table_cpu_1;
extern struct stn_cc xls_cc_table_cpu_2;
extern struct stn_cc xls_cc_table_cpu_3;
extern struct stn_cc xls_cc_table_gmac0;
extern struct stn_cc xls_cc_table_gmac1;
extern struct stn_cc xls_cc_table_cmp;
extern struct stn_cc xls_cc_table_pcie;
extern struct stn_cc xls_cc_table_dma;
extern struct stn_cc xls_cc_table_sec;

typedef void (*msgring_handler)(int, int, int, int, struct msgrng_msg *, void *);
int register_msgring_handler(int startb, int endb, msgring_handler action,
		    void *arg);
uint32_t xlr_msgring_handler(uint8_t bucket_mask, uint32_t max_messages);
void xlr_msgring_cpu_init(void);
void xlr_msgring_config(void);

#endif
