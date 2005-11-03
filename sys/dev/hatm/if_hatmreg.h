/*-
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * $FreeBSD: src/sys/dev/hatm/if_hatmreg.h,v 1.2 2005/01/06 01:42:43 imp Exp $
 *
 * Fore HE driver for NATM
 */

/* check configuration */
#if HE_CONFIG_VPI_BITS + HE_CONFIG_VCI_BITS > 12
#error "hatm: too many bits configured for VPI/VCI"
#endif

#define HE_MAX_VCCS	(1 << (HE_CONFIG_VPI_BITS + HE_CONFIG_VCI_BITS))

#define HE_VPI_MASK	((1 << (HE_CONFIG_VPI_BITS))-1)
#define HE_VCI_MASK	((1 << (HE_CONFIG_VCI_BITS))-1)

#define HE_VPI(CID)	(((CID) >> HE_CONFIG_VCI_BITS) & HE_VPI_MASK)
#define HE_VCI(CID)	((CID) & HE_VCI_MASK)

#define HE_CID(VPI,VCI) ((((VPI) & HE_VPI_MASK) << HE_CONFIG_VCI_BITS) | \
	((VCI) & HE_VCI_MASK))


/* GEN_CNTL_0 register */
#define HE_PCIR_GEN_CNTL_0		0x40
#define HE_PCIM_CTL0_64BIT		(1 << 0)
#define HE_PCIM_CTL0_IGNORE_TIMEOUT	(1 << 1)
#define HE_PCIM_CTL0_INIT_ENB		(1 << 2)
#define HE_PCIM_CTL0_MRM		(1 << 4)
#define HE_PCIM_CTL0_MRL		(1 << 5)
#define HE_PCIM_CTL0_BIGENDIAN		(1 << 16)
#define HE_PCIM_CTL0_INT_PROC_ENB	(1 << 25)

/*
 * Memory registers
 */
#define HE_REGO_FLASH			0x00000
#define HE_REGO_RESET_CNTL		0x80000
#define HE_REGM_RESET_STATE		(1 << 6)
#define HE_REGO_HOST_CNTL		0x80004
#define HE_REGM_HOST_BUS64		(1 << 27)
#define HE_REGM_HOST_DESC_RD64		(1 << 26)
#define HE_REGM_HOST_DATA_RD64		(1 << 25)
#define HE_REGM_HOST_DATA_WR64		(1 << 24)
#define HE_REGM_HOST_PROM_SEL		(1 << 12)
#define HE_REGM_HOST_PROM_WREN		(1 << 11)
#define HE_REGM_HOST_PROM_DATA_OUT	(1 << 10)
#define HE_REGS_HOST_PROM_DATA_OUT	10
#define HE_REGM_HOST_PROM_DATA_IN	(1 << 9)
#define HE_REGS_HOST_PROM_DATA_IN	9
#define HE_REGM_HOST_PROM_CLOCK		(1 << 8)
#define HE_REGM_HOST_PROM_BITS		(0x00001f00)
#define HE_REGM_HOST_QUICK_RD		(1 << 7)
#define HE_REGM_HOST_QUICK_WR		(1 << 6)
#define HE_REGM_HOST_OUTFF_ENB		(1 << 5)
#define HE_REGM_HOST_CMDFF_ENB		(1 << 4)
#define HE_REGO_LB_SWAP			0x80008
#define HE_REGM_LBSWAP_RNUM		(0xf << 27)
#define HE_REGS_LBSWAP_RNUM		27
#define HE_REGM_LBSWAP_DATA_WR_SWAP	(1 << 20)
#define HE_REGM_LBSWAP_DESC_RD_SWAP	(1 << 19)
#define HE_REGM_LBSWAP_DATA_RD_SWAP	(1 << 18)
#define HE_REGM_LBSWAP_INTR_SWAP	(1 << 17)
#define HE_REGM_LBSWAP_DESC_WR_SWAP	(1 << 16)
#define HE_REGM_LBSWAP_BIG_ENDIAN	(1 << 14)
#define HE_REGM_LBSWAP_XFER_SIZE	(1 << 7)

#define HE_REGO_LB_MEM_ADDR		0x8000C
#define HE_REGO_LB_MEM_DATA		0x80010
#define HE_REGO_LB_MEM_ACCESS		0x80014
#define HE_REGM_LB_MEM_HNDSHK		(1 << 30)
#define HE_REGM_LB_MEM_READ		0x3
#define HE_REGM_LB_MEM_WRITE		0x7

#define HE_REGO_SDRAM_CNTL		0x80018
#define HE_REGM_SDRAM_64BIT		(1 << 3)
#define HE_REGO_INT_FIFO		0x8001C
#define HE_REGM_INT_FIFO_CLRA		(1 << 8)
#define HE_REGM_INT_FIFO_CLRB		(1 << 9)
#define HE_REGM_INT_FIFO_CLRC		(1 << 10)
#define HE_REGM_INT_FIFO_CLRD		(1 << 11)
#define HE_REGO_ABORT_ADDR		0x80020

#define HE_REGO_IRQ0_BASE		0x80080
#define HE_REGO_IRQ_BASE(Q)		(HE_REGO_IRQ0_BASE + (Q) * 0x10 + 0x00)
#define HE_REGM_IRQ_BASE_TAIL		0x3ff
#define HE_REGO_IRQ_HEAD(Q)		(HE_REGO_IRQ0_BASE + (Q) * 0x10 + 0x04)
#define HE_REGS_IRQ_HEAD_SIZE		22
#define HE_REGS_IRQ_HEAD_THRESH		12
#define HE_REGS_IRQ_HEAD_HEAD		2
#define HE_REGO_IRQ_CNTL(Q)		(HE_REGO_IRQ0_BASE + (Q) * 0x10 + 0x08)
#define HE_REGM_IRQ_A			(0 << 2)
#define HE_REGM_IRQ_B			(1 << 2)
#define HE_REGM_IRQ_C			(2 << 2)
#define HE_REGM_IRQ_D			(3 << 2)
#define HE_REGO_IRQ_DATA(Q)		(HE_REGO_IRQ0_BASE + (Q) * 0x10 + 0x0C)

#define HE_REGO_GRP_1_0_MAP		0x800C0
#define HE_REGO_GRP_3_2_MAP		0x800C4
#define HE_REGO_GRP_5_4_MAP		0x800C8
#define HE_REGO_GRP_7_6_MAP		0x800CC

/*
 * Receive buffer pools
 */
#define HE_REGO_G0_RBPS_S	0x80400
#define HE_REGO_G0_RBPS_T	0x80404
#define HE_REGO_G0_RBPS_QI	0x80408
#define HE_REGO_G0_RBPS_BL	0x8040C

#define HE_REGO_RBP_S(K,G)	(HE_REGO_G0_RBPS_S + (K) * 0x10 + (G) * 0x20)
#define HE_REGO_RBP_T(K,G)	(HE_REGO_G0_RBPS_T + (K) * 0x10 + (G) * 0x20)
#define HE_REGO_RBP_QI(K,G)	(HE_REGO_G0_RBPS_QI + (K) * 0x10 + (G) * 0x20)
#define HE_REGO_RBP_BL(K,G)	(HE_REGO_G0_RBPS_BL + (K) * 0x10 + (G) * 0x20)

#define HE_REGS_RBP_HEAD	3
#define HE_REGS_RBP_TAIL	3
#define HE_REGS_RBP_SIZE	14
#define HE_REGM_RBP_INTR_ENB	(1 << 13)
#define HE_REGS_RBP_THRESH	0

/*
 * Receive buffer return queues
 */
#define HE_REGO_G0_RBRQ_ST	0x80500
#define HE_REGO_G0_RBRQ_H	0x80504
#define HE_REGO_G0_RBRQ_Q	0x80508
#define HE_REGO_G0_RBRQ_I	0x8050C

#define HE_REGO_RBRQ_ST(G)	(HE_REGO_G0_RBRQ_ST + (G) * 0x10)
#define HE_REGO_RBRQ_H(G)	(HE_REGO_G0_RBRQ_H + (G) * 0x10)
#define HE_REGO_RBRQ_Q(G)	(HE_REGO_G0_RBRQ_Q + (G) * 0x10)
#define HE_REGO_RBRQ_I(G)	(HE_REGO_G0_RBRQ_I + (G) * 0x10)

#define HE_REGS_RBRQ_HEAD	3
#define HE_REGS_RBRQ_THRESH	13
#define HE_REGS_RBRQ_SIZE	0
#define HE_REGS_RBRQ_TIME	8
#define HE_REGS_RBRQ_COUNT	0

/*
 * Intermediate queues
 */
#define HE_REGO_G0_INMQ_S	0x80580
#define HE_REGO_G0_INMQ_L	0x80584
#define HE_REGO_INMQ_S(G)	(HE_REGO_G0_INMQ_S + (G) * 8)
#define HE_REGO_INMQ_L(G)	(HE_REGO_G0_INMQ_L + (G) * 8)

#define HE_REGO_RHCONFIG		0x805C0
#define HE_REGM_RHCONFIG_PHYENB		(1 << 10)
#define HE_REGS_RHCONFIG_OAM_GID	7
#define HE_REGS_RHCONFIG_PTMR_PRE	0

/*
 * Transmit buffer return queues
 */
#define HE_REGO_TBRQ0_B_T	0x80600
#define HE_REGO_TBRQ0_H		0x80604
#define HE_REGO_TBRQ0_S		0x80608
#define HE_REGO_TBRQ0_THRESH	0x8060C

#define HE_REGO_TBRQ_B_T(G)	(HE_REGO_TBRQ0_B_T + (G) * 0x10)
#define HE_REGO_TBRQ_H(G)	(HE_REGO_TBRQ0_H + (G) * 0x10)
#define HE_REGO_TBRQ_S(G)	(HE_REGO_TBRQ0_S + (G) * 0x10)
#define HE_REGO_TBRQ_THRESH(G)	(HE_REGO_TBRQ0_THRESH + (G) * 0x10)

#define HE_REGS_TBRQ_HEAD	2

/*
 * Transmit packet descriptor ready queue
 */
#define HE_REGO_TPDRQ_H		0x80680
#define HE_REGS_TPDRQ_H_H	3
/* #define HE_REGM_TPDRQ_H_H	((HE_CONFIG_TPDRQ_SIZE - 1) << 3) */
#define HE_REGO_TPDRQ_T		0x80684
#define HE_REGS_TPDRQ_T_T	3
/* #define HE_REGM_TPDRQ_T_T	((HE_CONFIG_TPDRQ_SIZE - 1) << 3) */
#define HE_REGO_TPDRQ_S		0x80688

#define HE_REGO_UBUFF_BA	0x8068C

#define HE_REGO_RLBF0_H		0x806C0
#define HE_REGO_RLBF0_T		0x806C4
#define HE_REGO_RLBF1_H		0x806C8
#define HE_REGO_RLBF1_T		0x806CC
#define HE_REGO_RLBF_H(N)	(HE_REGO_RLBF0_H + (N) * 8)
#define HE_REGO_RLBF_T(N)	(HE_REGO_RLBF0_T + (N) * 8)

#define HE_REGO_RLBC_H		0x806D0
#define HE_REGO_RLBC_T		0x806D4
#define HE_REGO_RLBC_H2		0x806D8
#define HE_REGO_TLBF_H		0x806E0
#define HE_REGO_TLBF_T		0x806E4

#define HE_REGO_RLBF0_C		0x806E8
#define HE_REGO_RLBF1_C		0x806EC
#define HE_REGO_RLBF_C(N)	(HE_REGO_RLBF0_C + (N) * 4)

#define HE_REGO_RXTHRSH		0x806F0
#define HE_REGO_LITHRSH		0x806F4

#define HE_REGO_LBARB		0x80700
#define HE_REGS_LBARB_SLICE	28
#define HE_REGS_LBARB_RNUM	23
#define HE_REGS_LBARB_THPRI	21
#define HE_REGS_LBARB_RHPRI	19
#define HE_REGS_LBARB_TLPRI	17
#define HE_REGS_LBARB_RLPRI	15
#define HE_REGS_LBARB_BUS_MULT	8
#define HE_REGS_LBARB_NET_PREF	0

#define HE_REGO_SDRAMCON		0x80704
#define HE_REGM_SDRAMCON_BANK		(1 << 14)
#define HE_REGM_SDRAMCON_WIDE		(1 << 13)
#define HE_REGM_SDRAMCON_TWRWAIT	(1 << 12)
#define HE_REGM_SDRAMCON_TRPWAIT	(1 << 11)
#define HE_REGM_SDRAMCON_TRASWAIT	(1 << 10)
#define HE_REGS_SDRAMCON_REF		0

#define HE_REGO_RCCSTAT			0x8070C
#define HE_REGM_RCCSTAT_PROG		(1 << 0)

#define HE_REGO_TCMCONFIG		0x80740
#define HE_REGS_TCMCONFIG_BANK_WAIT	6
#define HE_REGS_TCMCONFIG_RW_WAIT	2
#define HE_REGS_TCMCONFIG_TYPE		0

#define HE_REGO_TSRB_BA		0x80744
#define HE_REGO_TSRC_BA		0x80748
#define HE_REGO_TMABR_BA	0x8074C
#define HE_REGO_TPD_BA		0x80750
#define HE_REGO_TSRD_BA		0x80758

#define HE_REGO_TXCONFIG		0x80760
#define HE_REGS_TXCONFIG_THRESH		22
#define HE_REGM_TXCONFIG_UTMODE		(1 << 21)
#define HE_REGS_TXCONFIG_VCI_MASK	17
#define HE_REGS_TXCONFIG_LBFREE		0

#define HE_REGO_TXAAL5_PROTO		0x80764

#define HE_REGO_RCMCONFIG		0x80780
#define HE_REGS_RCMCONFIG_BANK_WAIT	6
#define HE_REGS_RCMCONFIG_RW_WAIT	2
#define HE_REGS_RCMCONFIG_TYPE	0

#define HE_REGO_RCMRSRB_BA		0x80784
#define HE_REGO_RCMLBM_BA		0x80788
#define HE_REGO_RCMABR_BA		0x8078C

#define HE_REGO_RCCONFIG		0x807C0
#define HE_REGS_RCCONFIG_UTDELAY	11
#define HE_REGM_RCCONFIG_WRAP_MODE	(1 << 10)
#define HE_REGM_RCCONFIG_UT_MODE	(1 << 9)
#define HE_REGM_RCCONFIG_RXENB		(1 << 8)
#define HE_REGS_RCCONFIG_VP		4
#define HE_REGS_RCCONFIG_VC		0

#define HE_REGO_MCC		0x807C4
#define HE_REGO_OEC		0x807C8
#define HE_REGO_DCC		0x807CC
#define HE_REGO_CEC		0x807D0

#define HE_REGO_HSP_BA		0x807F0

#define HE_REGO_LBCONFIG	0x807F4

#define HE_REGO_CON_DAT		0x807F8
#define HE_REGO_CON_CTL		0x807FC
#define HE_REGM_CON_MBOX	(2 << 30)
#define HE_REGM_CON_TCM		(1 << 30)
#define HE_REGM_CON_RCM		(0 << 30)
#define HE_REGM_CON_WE		(1 << 29)
#define HE_REGM_CON_STATUS	(1 << 28)
#define HE_REGM_CON_DIS3	(1 << 22)
#define HE_REGM_CON_DIS2	(1 << 21)
#define HE_REGM_CON_DIS1	(1 << 20)
#define HE_REGM_CON_DIS0	(1 << 19)
#define HE_REGS_CON_DIS		19
#define HE_REGS_CON_ADDR	0

#define HE_REGO_SUNI		0x80800
#define HE_REGO_SUNI_END	0x80C00

#define HE_REGO_END		0x100000

/*
 * MBOX registers
 */
#define HE_REGO_CS_STPER0	0x000
#define HE_REGO_CS_STPER(G)	(HE_REGO_CS_STPER0 + (G))
#define HE_REGN_CS_STPER	32
#define HE_REGO_CS_STTIM0	0x020
#define HE_REGO_CS_STTIM(G)	(HE_REGO_CS_STTIM0 + (G))
#define HE_REGO_CS_TGRLD0	0x040
#define HE_REGO_CS_TGRLD(G)	(HE_REGO_CS_TGRLD0 + (G))
#define HE_REGO_CS_ERTHR0	0x50
#define HE_REGO_CS_ERTHR1	0x51
#define HE_REGO_CS_ERTHR2	0x52
#define HE_REGO_CS_ERTHR3	0x53
#define HE_REGO_CS_ERTHR4	0x54
#define HE_REGO_CS_ERCTL0	0x55
#define HE_REGO_CS_ERCTL1	0x56
#define HE_REGO_CS_ERCTL2	0x57
#define HE_REGO_CS_ERSTAT0	0x58
#define HE_REGO_CS_ERSTAT1	0x59
#define HE_REGO_CS_RTCCT	0x60
#define HE_REGO_CS_RTFWC	0x61
#define HE_REGO_CS_RTFWR	0x62
#define HE_REGO_CS_RTFTC	0x63
#define HE_REGO_CS_RTATR	0x64
#define HE_REGO_CS_TFBSET	0x70
#define HE_REGO_CS_TFBADD	0x71
#define HE_REGO_CS_TFBSUB	0x72
#define HE_REGO_CS_WCRMAX	0x73
#define HE_REGO_CS_WCRMIN	0x74
#define HE_REGO_CS_WCRINC	0x75
#define HE_REGO_CS_WCRDEC	0x76
#define HE_REGO_CS_WCRCEIL	0x77
#define HE_REGO_CS_BWDCNT	0x78
#define HE_REGO_CS_OTPPER	0x80
#define HE_REGO_CS_OTWPER	0x81
#define HE_REGO_CS_OTTLIM	0x82
#define HE_REGO_CS_OTTCNT	0x83
#define HE_REGO_CS_HGRRT0	0x90
#define HE_REGO_CS_HGRRT(G)	(HE_REGO_CS_HGRRT0 + (G))
#define HE_REGO_CS_ORPTRS	0xA0
#define HE_REGO_RCON_CLOSE	0x100
#define HE_REGO_CS_END		0x101

#define HE_REGT_CS_ERTHR {						\
	{			/* 155 */				\
	  { 0x000800ea, 0x000400ea, 0x000200ea },	/* ERTHR0 */	\
	  { 0x000C3388, 0x00063388, 0x00033388 },	/* ERTHR1 */	\
	  { 0x00101018, 0x00081018, 0x00041018 },	/* ERTHR2 */	\
	  { 0x00181dac, 0x000c1dac, 0x00061dac },	/* ERTHR3 */	\
	  { 0x0028051a, 0x0014051a, 0x000a051a },	/* ERTHR4 */	\
	}, {			/* 622 */				\
	  { 0x000800fa, 0x000400fa, 0x000200fa },	/* ERTHR0 */	\
	  { 0x000c33cb, 0x000633cb, 0x000333cb },	/* ERTHR1 */	\
	  { 0x0010101b, 0x0008101b, 0x0004101b },	/* ERTHR2 */	\
	  { 0x00181dac, 0x000c1dac, 0x00061dac },	/* ERTHR3 */	\
	  { 0x00280600, 0x00140600, 0x000a0600 },	/* ERTHR4 */	\
	}								\
}

#define HE_REGT_CS_ERCTL {						\
	{ 0x0235e4b1, 0x4701, 0x64b1 },	/* 155 */			\
	{ 0x023de8b3, 0x1801, 0x68b3 }	/* 622 */			\
}

#define HE_REGT_CS_ERSTAT {						\
	{ 0x1280, 0x64b1 },		/* 155 */			\
	{ 0x1280, 0x68b3 },		/* 622 */			\
}

#define HE_REGT_CS_RTFWR {						\
	0xf424,				/* 155 */			\
	0x14585				/* 622 */			\
}

#define HE_REGT_CS_RTATR {						\
	0x4680,				/* 155 */			\
	0x4680				/* 622 */			\
}

#define HE_REGT_CS_BWALLOC {						\
	{ 0x000563b7, 0x64b1, 0x5ab1, 0xe4b1, 0xdab1, 0x64b1 }, /* 155 */\
	{ 0x00159ece, 0x68b3, 0x5eb3, 0xe8b3, 0xdeb3, 0x68b3 }, /* 622 */\
}

#define HE_REGT_CS_ORCF {						\
	{ 0x6, 0x1e },			/* 155 */			\
	{ 0x5, 0x14 }			/* 622 */			\
}

/* 
 * TSRs - NR is relative to the starting number of the block
 */
#define HE_REGO_TSRA(BASE,CID,NR) ((BASE) + ((CID) << 3) + (NR))
#define HE_REGO_TSRB(BASE,CID,NR) ((BASE) + ((CID) << 2) + (NR))
#define HE_REGO_TSRC(BASE,CID,NR) ((BASE) + ((CID) << 1) + (NR))
#define HE_REGO_TSRD(BASE,CID)    ((BASE) + (CID))

#define HE_REGM_TSR0_CONN_STATE		(7 << 28)
#define HE_REGS_TSR0_CONN_STATE		28
#define HE_REGM_TSR0_USE_WMIN		(1 << 23)
#define HE_REGM_TSR0_GROUP		(7 << 18)
#define HE_REGS_TSR0_GROUP		18
#define HE_REGM_TSR0_TRAFFIC		(3 << 16)
#define HE_REGS_TSR0_TRAFFIC		16
#define HE_REGM_TSR0_TRAFFIC_CBR	0
#define HE_REGM_TSR0_TRAFFIC_UBR	1
#define HE_REGM_TSR0_TRAFFIC_ABR	2
#define HE_REGM_TSR0_PROT		(1 << 15)
#define HE_REGM_TSR0_AAL		(3 << 12)
#define HE_REGS_TSR0_AAL		12
#define HE_REGM_TSR0_AAL_5		0
#define HE_REGM_TSR0_AAL_0		1
#define HE_REGM_TSR0_AAL_0T		2
#define HE_REGM_TSR0_HALT_ER		(1 << 11)
#define HE_REGM_TSR0_MARK_CI		(1 << 10)
#define HE_REGM_TSR0_MARK_ER		(1 << 9)
#define HE_REGM_TSR0_UPDATE_GER		(1 << 8)
#define HE_REGM_TSR0_RC			0xff

#define HE_REGM_TSR1_PCR		(0x7fff << 16)
#define HE_REGS_TSR1_PCR		16
#define HE_REGM_TSR1_MCR		(0x7fff << 0)
#define HE_REGS_TSR1_MCR		0

#define HE_REGM_TSR2_ACR		(0x7fff << 16)
#define HE_REGS_TSR2_ACR		16

#define HE_REGM_TSR3_NRM		(0xff << 24)
#define HE_REGS_TSR3_NRM		24
#define HE_REGM_TSR3_CRM		(0xff << 0)
#define HE_REGS_TSR3_CRM		0

#define HE_REGM_TSR4_FLUSH		(1 << 31)
#define HE_REGM_TSR4_SESS_END		(1 << 30)
#define HE_REGM_TSR4_OAM_CRC10		(1 << 28)
#define HE_REGM_TSR4_NULL_CRC10		(1 << 27)
#define HE_REGM_TSR4_PROT		(1 << 26)
#define HE_REGM_TSR4_AAL		(3 << 24)
#define HE_REGS_TSR4_AAL		24
#define HE_REGM_TSR4_AAL_5		0
#define HE_REGM_TSR4_AAL_0		1
#define HE_REGM_TSR4_AAL_0T		2

#define HE_REGM_TSR9_INIT		0x00100000

#define HE_REGM_TSR11_ICR		(0x7fff << 16)
#define HE_REGS_TSR11_ICR		16
#define HE_REGM_TSR11_TRM		(0x7 << 13)
#define HE_REGS_TSR11_TRM		13
#define HE_REGM_TSR11_NRM		(0x7 << 10)
#define HE_REGS_TSR11_NRM		10
#define HE_REGM_TSR11_ADTF		0x3ff
#define HE_REGS_TSR11_ADTF		0

#define HE_REGM_TSR13_RDF		(0xf << 23)
#define HE_REGS_TSR13_RDF		23
#define HE_REGM_TSR13_RIF		(0xf << 19)
#define HE_REGS_TSR13_RIF		19
#define HE_REGM_TSR13_CDF		(0x7 << 16)
#define HE_REGS_TSR13_CDF		16
#define HE_REGM_TSR13_CRM		0xffff
#define HE_REGS_TSR13_CRM		0

#define HE_REGM_TSR14_CBR_DELETE	(1 << 31)
#define HE_REGM_TSR14_ABR_CLOSE		(1 << 16)

/*
 * RSRs
 */
#define HE_REGO_RSRA(BASE,CID,NR) ((BASE) + ((CID) << 3) + (NR))
#define HE_REGO_RSRB(BASE,CID,NR) ((BASE) + ((CID) << 1) + (NR))

#define HE_REGM_RSR0_PTI7		(1 << 15)
#define HE_REGM_RSR0_RM			(1 << 14)
#define HE_REGM_RSR0_F5OAM		(1 << 13)
#define HE_REGM_RSR0_STARTPDU		(1 << 10)
#define HE_REGM_RSR0_OPEN		(1 << 6)
#define HE_REGM_RSR0_PPD		(1 << 5)
#define HE_REGM_RSR0_EPD		(1 << 4)
#define HE_REGM_RSR0_TCPCS		(1 << 3)
#define HE_REGM_RSR0_AAL		0x7
#define HE_REGM_RSR0_AAL_5		0x0
#define HE_REGM_RSR0_AAL_0		0x1
#define HE_REGM_RSR0_AAL_0T		0x2
#define HE_REGM_RSR0_AAL_RAW		0x3
#define HE_REGM_RSR0_AAL_RAWCRC10	0x4

#define HE_REGM_RSR1_AQI		(1 << 20)
#define HE_REGM_RSR1_RBPL_ONLY		(1 << 19)
#define HE_REGM_RSR1_GROUP		(7 << 16)
#define HE_REGS_RSR1_GROUP		16

#define HE_REGM_RSR4_AQI		(1 << 30)
#define HE_REGM_RSR4_GROUP		(7 << 27)
#define HE_REGS_RSR4_GROUP		27
#define HE_REGM_RSR4_RBPL_ONLY		(1 << 26)

/*
 * Relative to RCMABR_BA
 */
#define HE_REGO_CM_GQTBL	0x000
#define HE_REGL_CM_GQTBL	0x100
#define HE_REGO_CM_RGTBL	0x100
#define HE_REGL_CM_RGTBL	0x100
#define HE_REGO_CM_TNRMTBL	0x200
#define HE_REGL_CM_TNRMTBL	0x100
#define HE_REGO_CM_ORCF		0x300
#define HE_REGL_CM_ORCF		0x100
#define HE_REGO_CM_RTGTBL	0x400
#define HE_REGL_CM_RTGTBL	0x200
#define HE_REGO_CM_IRCF		0x600
#define HE_REGL_CM_IRCF		0x200

/*
 * Interrupt Status
 */
#define HE_REGM_ITYPE		0xf8
#define HE_REGM_IGROUP		0x07
#define HE_REGM_ITYPE_TBRQ	(0x0 << 3)
#define HE_REGM_ITYPE_TPD	(0x1 << 3)
#define HE_REGM_ITYPE_RBPS	(0x2 << 3)
#define HE_REGM_ITYPE_RBPL	(0x3 << 3)
#define HE_REGM_ITYPE_RBRQ	(0x4 << 3)
#define HE_REGM_ITYPE_RBRQT	(0x5 << 3)
#define HE_REGM_ITYPE_PHYS	(0x6 << 3)
#define HE_REGM_ITYPE_UNKNOWN	0xf8
#define HE_REGM_ITYPE_ERR	0x80
#define HE_REGM_ITYPE_PERR	0x81
#define HE_REGM_ITYPE_ABORT	0x82
#define HE_REGM_ITYPE_INVALID	0xf8

/*
 * Serial EEPROM
 */
#define HE_EEPROM_PROD_ID	0x08
#define HE_EEPROM_PROD_ID_LEN	30
#define HE_EEPROM_REV		0x26
#define HE_EEPROM_REV_LEN	4
#define HE_EEPROM_M_SN		0x3A
#define HE_EEPROM_MEDIA		0x3E
#define HE_EEPROM_MAC		0x42

#define HE_MEDIA_UTP155	0x06
#define HE_MEDIA_MMF155	0x26
#define HE_MEDIA_MMF622	0x27
#define HE_MEDIA_SMF155	0x46
#define HE_MEDIA_SMF622	0x47

#define HE_622_CLOCK		66667000
#define HE_155_CLOCK		50000000

/*
 * Statistics
 */
struct fatm_statshe {
};

/*
 * Queue entries
 */
/* Receive Buffer Pool Queue entry */
struct he_rbpen {
	uint32_t	phys;		/* physical address */
	uint32_t	handle;		/* handle or virtual address */
};
/* Receive Buffer Return Queue entry */
struct he_rbrqen {
	uint32_t	addr;		/* handle and flags */
	uint32_t	len;		/* length and CID */
};
#define HE_REGM_RBRQ_ADDR		0xFFFFFFC0
#define HE_REGS_RBRQ_ADDR		6
#define HE_REGM_RBRQ_FLAGS		0x0000003F
#define HE_REGM_RBRQ_HBUF_ERROR		(1 << 0)
#define HE_REGM_RBRQ_CON_CLOSED		(1 << 1)
#define HE_REGM_RBRQ_AAL5_PROT		(1 << 2)
#define HE_REGM_RBRQ_END_PDU		(1 << 3)
#define HE_REGM_RBRQ_LEN_ERROR		(1 << 4)
#define HE_REGM_RBRQ_CRC_ERROR		(1 << 5)
#define HE_REGM_RBRQ_CID		(0x1fff << 16)
#define HE_REGS_RBRQ_CID		16
#define HE_REGM_RBRQ_LEN		0xffff

/* Transmit Packet Descriptor Ready Queue entry */
struct he_tpdrqen {
	uint32_t	tpd;		/* physical address */
	uint32_t	cid;		/* connection id */
};
/* Transmit buffer return queue */
struct he_tbrqen {
	uint32_t	addr;		/* handle and flags */
};
#define HE_REGM_TBRQ_ADDR	0xffffffc0
#define HE_REGM_TBRQ_FLAGS	0x0000000a
#define HE_REGM_TBRQ_EOS	0x00000008
#define HE_REGM_TBRQ_MULT	0x00000002

struct he_tpd {
	uint32_t	addr;		/* handle or virtual address and flags */
	uint32_t	res;		/* reserved */
	struct {
	    uint32_t	addr;		/* buffer address */
	    uint32_t	len;		/* buffer length and flags */
	}		bufs[3];
};
#define HE_REGM_TPD_ADDR	0xffffffC0
#define HE_REGS_TPD_ADDR	6
#define HE_REGM_TPD_INTR	0x0001
#define HE_REGM_TPD_CLP		0x0002
#define HE_REGM_TPD_EOS		0x0004
#define HE_REGM_TPD_PTI		0x0038
#define HE_REGS_TPD_PTI		3
#define HE_REGM_TPD_LST		0x80000000

/* 
 * The HOST STATUS PAGE
 */
struct he_hsp {
	struct {
		uint32_t	tbrq_tail;
		uint32_t	res1[15];
		uint32_t	rbrq_tail;
		uint32_t	res2[15];
	} group[8];
};

#define HE_MAX_PDU	(65535)
