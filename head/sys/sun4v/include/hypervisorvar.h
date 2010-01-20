/*-
 * Copyright (c) 2006 Kip Macy
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


#ifndef	_MACHINE_HYPERVISORVAR_H_
#define	_MACHINE_HYPERVISORVAR_H_
/*
 * Trap types
 */
#define	FAST_TRAP		0x80	/* Function # in %o5 */
#define	CPU_TICK_NPT		0x81
#define	CPU_STICK_NPT		0x82
#define	MMU_MAP_ADDR		0x83
#define	MMU_UNMAP_ADDR		0x84
#define	TTRACE_ADDENTRY		0x85

#define CORE_TRAP               0xff

/*
 * Status returns in %o0.
 */
#define	H_EOK			0	/* Successful return */
#define	H_ENOCPU		1	/* Invalid CPU id */
#define	H_ENORADDR		2	/* Invalid real address */
#define	H_ENOINTR		3	/* Invalid interrupt id */
#define	H_EBADPGSZ		4	/* Invalid pagesize encoding */
#define	H_EBADTSB		5	/* Invalid TSB description */
#define	H_EINVAL		6	/* Invalid argument */
#define	H_EBADTRAP		7	/* Invalid function number */
#define	H_EBADALIGN		8	/* Invalid address alignment */
#define	H_EWOULDBLOCK		9	/* Cannot complete operation */
					/* without blocking */
#define	H_ENOACCESS		10	/* No access to resource */
#define	H_EIO			11	/* I/O error */
#define	H_ECPUERROR		12	/* CPU is in error state */
#define	H_ENOTSUPPORTED		13	/* Function not supported */
#define	H_ENOMAP		14	/* Mapping is not valid, */
					/* no translation exists */
#define H_ETOOMANY              15      /* Too many items specified / limit reached */
#define H_ECHANNEL              16      /* Invalid LDC channel */

#define	H_BREAK			-1	/* Console Break */
#define	H_HUP			-2	/* Console Break */

/*
 * Mondo CPU ID argument processing.
 */
#define	HV_SEND_MONDO_ENTRYDONE	0xffff

/*
 * Function numbers for CORE_TRAP.
 */
#define API_SET_VERSION         0x00
#define API_PUTCHAR             0x01
#define API_EXIT                0x02
#define API_GET_VERSION         0x03

/*
 * Function numbers for FAST_TRAP.
 */
#define	MACH_EXIT		0x00
#define	MACH_DESC		0x01
#define	MACH_SIR		0x02
#define MACH_SET_SOFT_STATE     0x03
#define MACH_GET_SOFT_STATE     0x04
#define MACH_WATCHDOG           0x05

#define CPU_START               0x10
#define CPU_STOP                0x11
#define	CPU_YIELD		0x12
#define CPU_QCONF		0x14
#define CPU_QINFO		0x15
#define CPU_MYID		0x16
#define	CPU_STATE		0x17
#define CPU_SET_RTBA		0x18
#define CPU_GET_RTBA		0x19


#define MMU_TSB_CTX0            0x20
#define MMU_TSB_CTXNON0		0x21
#define	MMU_DEMAP_PAGE		0x22
#define	MMU_DEMAP_CTX		0x23
#define	MMU_DEMAP_ALL		0x24
#define	MMU_MAP_PERM_ADDR	0x25
#define MMU_FAULT_AREA_CONF     0x26
#define MMU_ENABLE              0x27
#define MMU_UNMAP_PERM_ADDR     0x28
#define MMU_TSB_CTX0_INFO       0x29
#define MMU_TSB_CTXNON0_INFO    0x2a
#define MMU_FAULT_AREA_INFO     0x2b

/*
 * Bits for MMU functions flags argument:
 *	arg3 of MMU_MAP_ADDR
 *	arg3 of MMU_DEMAP_CTX
 *	arg2 of MMU_DEMAP_ALL
 */
#define	MAP_DTLB		0x1
#define	MAP_ITLB		0x2




#define	MEM_SCRUB		0x31
#define	MEM_SYNC		0x32
#define	CPU_MONDO_SEND		0x42
#define	TOD_GET			0x50
#define	TOD_SET			0x51
#define	CONS_GETCHAR		0x60
#define	CONS_PUTCHAR		0x61
#define CONS_READ               0x62
#define CONS_WRITE              0x63

#define	SVC_SEND		0x80
#define	SVC_RECV		0x81
#define	SVC_GETSTATUS		0x82
#define	SVC_SETSTATUS		0x83
#define	SVC_CLRSTATUS		0x84

#define	TTRACE_BUF_CONF		0x90
#define	TTRACE_BUF_INFO		0x91
#define	TTRACE_ENABLE		0x92
#define	TTRACE_FREEZE		0x93

#define	DUMP_BUF_UPDATE		0x94
#define DUMP_BUF_INFO           0x95

#define	INTR_DEVINO2SYSINO	0xa0
#define	INTR_GETENABLED	        0xa1
#define	INTR_SETENABLED	        0xa2
#define	INTR_GETSTATE	        0xa3
#define	INTR_SETSTATE	        0xa4
#define	INTR_GETTARGET	        0xa5
#define	INTR_SETTARGET	        0xa6

#define VINTR_GETCOOKIE         0xa7 
#define VINTR_SETCOOKIE         0xa8 
#define VINTR_GETENABLED        0xa9 
#define VINTR_SETENABLED        0xaa 
#define VINTR_GETSTATE          0xab 
#define VINTR_SETSTATE          0xac 
#define VINTR_GETTARGET         0xad 
#define VINTR_SETTARGET         0xae 


#define	PCI_IOMMU_MAP		0xb0
#define	PCI_IOMMU_DEMAP	        0xb1
#define	PCI_IOMMU_GETMAP	0xb2
#define	PCI_IOMMU_GETBYPASS	0xb3

#define	PCI_CONFIG_GET		0xb4
#define	PCI_CONFIG_PUT		0xb5

#define	PCI_PEEK		0xb6
#define	PCI_POKE		0xb7

#define	PCI_DMA_SYNC		0xb8

#define	PCI_MSIQ_CONF		0xc0
#define	PCI_MSIQ_INFO		0xc1
#define	PCI_MSIQ_GETVALID	0xc2
#define	PCI_MSIQ_SETVALID	0xc3
#define	PCI_MSIQ_GETSTATE	0xc4
#define	PCI_MSIQ_SETSTATE	0xc5
#define	PCI_MSIQ_GETHEAD	0xc6
#define	PCI_MSIQ_SETHEAD	0xc7
#define	PCI_MSIQ_GETTAIL	0xc8

#define	PCI_MSI_GETVALID	0xc9
#define	PCI_MSI_SETVALID	0xca
#define	PCI_MSI_GETMSIQ	        0xcb
#define	PCI_MSI_SETMSIQ	        0xcc
#define	PCI_MSI_GETSTATE	0xcd
#define	PCI_MSI_SETSTATE	0xce

#define	PCI_MSG_GETMSIQ	        0xd0
#define	PCI_MSG_SETMSIQ	        0xd1
#define	PCI_MSG_GETVALID	0xd2
#define	PCI_MSG_SETVALID	0xd3

#define LDC_TX_QCONF            0xe0
#define LDC_TX_QINFO            0xe1
#define LDC_TX_GET_STATE        0xe2
#define LDC_TX_SET_QTAIL        0xe3
#define LDC_RX_QCONF            0xe4
#define LDC_RX_QINFO            0xe5
#define LDC_RX_GET_STATE        0xe6
#define LDC_RX_SET_QHEAD        0xe7

#define LDC_SET_MAPTABLE        0xea
#define LDC_GET_MAPTABLE        0xeb
#define LDC_COPY                0xec
#define LDC_MAPIN               0xed
#define LDC_UNMAP               0xee
#define LDC_REVOKE              0xef


#define SIM_READ                0xf0
#define SIM_WRITE               0xf1


#define NIAGARA_GET_PERFREG     0x100
#define NIAGARA_SET_PERFREG     0x101

#define NIAGARA_MMUSTAT_CONF    0x102
#define NIAGARA_MMUSTAT_INFO    0x103

/*
 * Interrupt state manipulation definitions.
 */

#define	HV_INTR_IDLE_STATE	0
#define	HV_INTR_RECEIVED_STATE	1
#define	HV_INTR_DELIVERED_STATE	2

#define	HV_INTR_DISABLED	0
#define	HV_INTR_ENABLED		1

#ifndef LOCORE


#ifdef SET_MMU_STATS
#ifndef TTE4V_NPGSZ
#define	TTE4V_NPGSZ	8
#endif /* TTE4V_NPGSZ */
/*
 * MMU statistics structure for MMU_STAT_AREA
 */
struct mmu_stat_one {
	uint64_t	hit_ctx0[TTE4V_NPGSZ];
	uint64_t	hit_ctxn0[TTE4V_NPGSZ];
	uint64_t	tsb_miss;
	uint64_t	tlb_miss;	/* miss, no TSB set */
	uint64_t	map_ctx0[TTE4V_NPGSZ];
	uint64_t	map_ctxn0[TTE4V_NPGSZ];
};

struct mmu_stat {
	struct mmu_stat_one	immu_stat;
	struct mmu_stat_one	dmmu_stat;
	uint64_t		set_ctx0;
	uint64_t		set_ctxn0;
};
#endif /* SET_MMU_STATS */

#endif /* _ASM */

/*
 * CPU States
 */
#define	CPU_STATE_INVALID	0x0
#define	CPU_STATE_IDLE		0x1	/* cpu not started */
#define	CPU_STATE_GUEST		0x2	/* cpu running guest code */
#define	CPU_STATE_ERROR		0x3	/* cpu is in the error state */
#define	CPU_STATE_LAST_PUBLIC	CPU_STATE_ERROR	/* last valid state */

/*
 * MMU fault status area
 */

#define	MMFSA_TYPE_	0x00	/* fault type */
#define	MMFSA_ADDR_	0x08	/* fault address */
#define	MMFSA_CTX_	0x10	/* fault context */

#define	MMFSA_I_	0x00		/* start of fields for I */
#define	MMFSA_I_TYPE	(MMFSA_I_ + MMFSA_TYPE_) /* instruction fault type */
#define	MMFSA_I_ADDR	(MMFSA_I_ + MMFSA_ADDR_) /* instruction fault address */
#define	MMFSA_I_CTX	(MMFSA_I_ + MMFSA_CTX_)	/* instruction fault context */

#define	MMFSA_D_	0x40		/* start of fields for D */
#define	MMFSA_D_TYPE	(MMFSA_D_ + MMFSA_TYPE_) /* data fault type */
#define	MMFSA_D_ADDR	(MMFSA_D_ + MMFSA_ADDR_) /* data fault address */
#define	MMFSA_D_CTX	(MMFSA_D_ + MMFSA_CTX_)	/* data fault context */

#define	MMFSA_F_FMISS	1	/* fast miss */
#define	MMFSA_F_FPROT	2	/* fast protection */
#define	MMFSA_F_MISS	3	/* mmu miss */
#define	MMFSA_F_INVRA	4	/* invalid RA */
#define	MMFSA_F_PRIV	5	/* privilege violation */
#define	MMFSA_F_PROT	6	/* protection violation */
#define	MMFSA_F_NFO	7	/* NFO access */
#define	MMFSA_F_SOPG	8	/* so page */
#define	MMFSA_F_INVVA	9	/* invalid VA */
#define	MMFSA_F_INVASI	10	/* invalid ASI */
#define	MMFSA_F_NCATM	11	/* non-cacheable atomic */
#define	MMFSA_F_PRVACT	12	/* privileged action */
#define	MMFSA_F_WPT	13	/* watchpoint hit */
#define	MMFSA_F_UNALIGN	14	/* unaligned access */
#define	MMFSA_F_INVPGSZ	15	/* invalid page size */

#define	MMFSA_SIZE	0x80	/* in bytes, 64 byte aligned */

/*
 * MMU fault status - MMFSA_IFS and MMFSA_DFS
 */
#define	MMFS_FV		0x00000001
#define	MMFS_OW		0x00000002
#define	MMFS_W		0x00000004
#define	MMFS_PR		0x00000008
#define	MMFS_CT		0x00000030
#define	MMFS_E		0x00000040
#define	MMFS_FT		0x00003f80
#define	MMFS_ME		0x00004000
#define	MMFS_TM		0x00008000
#define	MMFS_ASI	0x00ff0000
#define	MMFS_NF		0x01000000

/*
 * DMA sync parameter definitions
 */
#define	HVIO_DMA_SYNC_DIR_TO_DEV	0x01
#define	HVIO_DMA_SYNC_DIR_FROM_DEV	0x02

#ifdef SIMULATOR
#define MAGIC_TRAP_ON	ta	0x77
#define MAGIC_TRAP_OFF	ta	0x78
#define MAGIC_EXIT	ta	0x71
#else
#define MAGIC_TRAP_ON	nop
#define MAGIC_TRAP_OFF	nop
#define MAGIC_EXIT	nop
#endif


#endif	/*_MACHINE_HYPERVISORVAR_H_ */
