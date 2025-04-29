/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __X86_IOMMU_AMD_REG_H
#define	__X86_IOMMU_AMD_REG_H

/*
 * MMIO Registers. Offsets and bits definitions.
 */

#define	AMDIOMMU_DEVTAB_BASE	0x0000
#define	AMDIOMMU_CMDBUF_BASE	0x0008
#define	AMDIOMMU_EVNTLOG_BASE	0x0010
#define	AMDIOMMU_CTRL		0x0018
#define	AMDIOMMU_EXCL_BASE	0x0020
#define	AMDIOMMU_EXCL_RANGE	0x0028
#define	AMDIOMMU_EFR		0x0030
#define	AMDIOMMU_PPRLOG_BASE	0x0038
#define	AMDIOMMU_HWEV_UPPER	0x0040
#define	AMDIOMMU_HWEV_LOWER	0x0048
#define	AMDIOMMU_HWEV_STATUS	0x0050

#define	AMDIOMMU_SMIF_0		0x0060
#define	AMDIOMMU_SMIF_1		0x0068
#define	AMDIOMMU_SMIF_2		0x0070
#define	AMDIOMMU_SMIF_3		0x0078
#define	AMDIOMMU_SMIF_4		0x0080
#define	AMDIOMMU_SMIF_5		0x0088
#define	AMDIOMMU_SMIF_6		0x0090
#define	AMDIOMMU_SMIF_7		0x0098
#define	AMDIOMMU_SMIF_8		0x00a0
#define	AMDIOMMU_SMIF_9		0x00a8
#define	AMDIOMMU_SMIF_10	0x00b0
#define	AMDIOMMU_SMIF_11	0x00b8
#define	AMDIOMMU_SMIF_12	0x00c0
#define	AMDIOMMU_SMIF_13	0x00c8
#define	AMDIOMMU_SMIF_14	0x00d0
#define	AMDIOMMU_SMIF_15	0x00d8

#define	AMDIOMMU_VAPIC_LOG_BASE	0x00e0
#define	AMDIOMMU_VAPIC_LOG_TAIL	0x00e8
#define	AMDIOMMU_PPRLOGB_BASE	0x00f0
#define	AMDIOMMU_EVNTLOGB_BASE	0x00f0

#define	AMDIOMMU_DEVTAB_S1_BASE	0x0100
#define	AMDIOMMU_DEVTAB_S2_BASE	0x0108
#define	AMDIOMMU_DEVTAB_S3_BASE	0x0110
#define	AMDIOMMU_DEVTAB_S4_BASE	0x0118
#define	AMDIOMMU_DEVTAB_S5_BASE	0x0120
#define	AMDIOMMU_DEVTAB_S6_BASE	0x0128
#define	AMDIOMMU_DEVTAB_S7_BASE	0x0130

#define	AMDIOMMU_DSFX		0x0138
#define	AMDIOMMU_DSCX		0x0140
#define	AMDIOMMU_DSSX		0x0148

#define	AMDIOMMU_MSI_VEC0	0x0150
#define	AMDIOMMU_MSI_VEC1	0x0154
#define	AMDIOMMU_MSI_CAP_H	0x0158
#define	AMDIOMMU_MSI_ADDR_LOW	0x015c
#define	AMDIOMMU_MSI_ADDR_HIGH	0x0160
#define	AMDIOMMU_MSI_DATA	0x0164
#define	AMDIOMMU_MSI_MAPCAP	0x0168

#define	AMDIOMMU_PERFOPT	0x016c

#define	AMDIOMMU_x2APIC_CTRL	0x0170
#define	AMDIOMMU_PPRI_CTRL	0x0178
#define	AMDIOMMU_GALOGI_CTRL	0x0180

#define	AMDIOMMU_vIOMMU_STATUS	0x0190

#define	AMDIOMMU_MARC0_BASE	0x0200
#define	AMDIOMMU_MARC0_RELOC	0x0208
#define	AMDIOMMU_MARC0_LEN	0x0210
#define	AMDIOMMU_MARC1_BASE	0x0218
#define	AMDIOMMU_MARC1_RELOC	0x0220
#define	AMDIOMMU_MARC1_LEN	0x0228
#define	AMDIOMMU_MARC2_BASE	0x0230
#define	AMDIOMMU_MARC2_RELOC	0x0238
#define	AMDIOMMU_MARC2_LEN	0x0240
#define	AMDIOMMU_MARC3_BASE	0x0248
#define	AMDIOMMU_MARC3_RELOC	0x0250
#define	AMDIOMMU_MARC3_LEN	0x0258

#define	AMDIOMMU_EFR2		0x01a0

#define	AMDIOMMU_CMDBUF_HEAD	0x2000
#define	AMDIOMMU_CMDBUF_TAIL	0x2008
#define	AMDIOMMU_EVNTLOG_HEAD	0x2010
#define	AMDIOMMU_EVNTLOG_TAIL	0x2018
#define	AMDIOMMU_CMDEV_STATUS	0x2020
#define	AMDIOMMU_PPRLOG_HEAD	0x2030
#define	AMDIOMMU_PPRLOG_TAIL	0x2038
#define	AMDIOMMU_vAPICLOG_HEAD	0x2040
#define	AMDIOMMU_vAPICLOG_TAIL	0x2048
#define	AMDIOMMU_PPRLOGB_HEAD	0x2050
#define	AMDIOMMU_PPRLOGB_TAIL	0x2058
#define	AMDIOMMU_EVNTLOGB_HEAD	0x2070
#define	AMDIOMMU_EVNTLOGB_TAIL	0x2078
#define	AMDIOMMU_PPRLOG_AUR	0x2080
#define	AMDIOMMU_PPRLOG_EAI	0x2088
#define	AMDIOMMU_PPRLOGB_AUR	0x2090

/*
 * IOMMU Control Register AMDIOMMU_CTRL fields
 */
#define	AMDIOMMU_CTRL_EN		0x0000000000000001ull	/* IOMMU En */
#define	AMDIOMMU_CTRL_HTTUN_EN		0x0000000000000002ull	/* HT Tun Trans En */
#define	AMDIOMMU_CTRL_EVNTLOG_EN	0x0000000000000004ull	/* Event Log En */
#define	AMDIOMMU_CTRL_EVENTINT_EN	0x0000000000000008ull	/* Event Log Intr En */
#define	AMDIOMMU_CTRL_COMWINT_EN	0x0000000000000010ull	/* Compl Wait Intr En */
#define	AMDIOMMU_CTRL_INVTOUT_MASK	0x00000000000000e0ull	/* IOTLB Inv Timeout*/
#define	AMDIOMMU_CTRL_INVTOUT_NO	0x0000000000000000ull
#define	AMDIOMMU_CTRL_INVTOUT_1MS	0x0000000000000020ull
#define	AMDIOMMU_CTRL_INVTOUT_10MS	0x0000000000000040ull
#define	AMDIOMMU_CTRL_INVTOUT_100MS	0x0000000000000060ull
#define	AMDIOMMU_CTRL_INVTOUT_1S	0x0000000000000080ull
#define	AMDIOMMU_CTRL_INVTOUT_10S	0x00000000000000a0ull
#define	AMDIOMMU_CTRL_INVTOUT_100S	0x00000000000000b0ull
#define	AMDIOMMU_CTRL_INVTOUT_RSRV	0x00000000000000e0ull
#define	AMDIOMMU_CTRL_PASSPW		0x0000000000000100ull	/* HT Pass Posted Wr */
#define	AMDIOMMU_CTRL_REPASSPW		0x0000000000000200ull	/* HT Resp Pass Posted Wr */
#define	AMDIOMMU_CTRL_COHERENT		0x0000000000000400ull	/* HT Coherent Reads */
#define	AMDIOMMU_CTRL_ISOC		0x0000000000000800ull	/* HT Isoc Reads */
#define	AMDIOMMU_CTRL_CMDBUF_EN		0x0000000000001000ull	/* Start CMD proc En */
#define	AMDIOMMU_CTRL_PPRLOG_EN		0x0000000000002000ull	/* Periph Page Req Log En */
#define	AMDIOMMU_CTRL_PPRINT_EN		0x0000000000004000ull	/* Periph Page Req Intr En */
#define	AMDIOMMU_CTRL_PPR_EN		0x0000000000008000ull	/* Periph Page Req En */
#define	AMDIOMMU_CTRL_GT_EN		0x0000000000010000ull	/* Guest En */
#define	AMDIOMMU_CTRL_GA_EN		0x0000000000020000ull	/* Guest vAPIC En */
#define	AMDIOMMU_CTRL_SMIF_EN		0x0000000000400000ull	/* SMI Filter En */
#define	AMDIOMMU_CTRL_SLFWB_DIS		0x0000000000800000ull	/* Self WriteBack Dis */
#define	AMDIOMMU_CTRL_SMIFLOG_EN	0x0000000001000000ull	/* SMI Filter Log En */
#define	AMDIOMMU_CTRL_GAM_EN_MASK	0x000000000e000000ull	/* Guest vAPIC Mode En */
#define	AMDIOMMU_CTRL_GAM_EN_vAPIC_GM0	0x0000000000000000ull	/* IRTE.GM = 0 */
#define	AMDIOMMU_CTRL_GAM_EN_vAPIC_GM1	0x0000000002000000ull	/* IRTE.GM = 1 */
#define	AMDIOMMU_CTRL_GALOG_EN		0x0000000010000000ull	/* Guest vAPIC GA Log En */
#define	AMDIOMMU_CTRL_GAINT_EN		0x0000000020000000ull	/* Guest vAPIC GA Intr En */
#define	AMDIOMMU_CTRL_DUALPPRLOG_MASK	0x00000000c0000000ull	/* Dual Periph Page Req Log En */
#define	AMDIOMMU_CTRL_DUALPPRLOG_A	0x0000000000000000ull	/* Use Log A */
#define	AMDIOMMU_CTRL_DUALPPRLOG_B	0x0000000040000000ull	/* Use Log B */
#define	AMDIOMMU_CTRL_DUALPPRLOG_SWAP	0x0000000080000000ull	/* Auto-swap on full */
#define	AMDIOMMU_CTRL_DUALPPRLOG_RSRV	0x00000000c0000000ull
#define	AMDIOMMU_CTRL_DUALEVNTLOG_MASK	0x0000000300000000ull	/* Dual Event Log En */
#define	AMDIOMMU_CTRL_DUALEVNTLOG_A	0x0000000000000000ull	/* Use Log A Buf */
#define	AMDIOMMU_CTRL_DUALEVNTLOG_B	0x0000000100000000ull	/* Use Log B Buf */
#define	AMDIOMMU_CTRL_DUALEVNTLOG_SWAP	0x0000000200000000ull	/* Auto-swap on full */
#define	AMDIOMMU_CTRL_DUALEVNTLOG_RSRV	0x0000000300000000ull
#define	AMDIOMMU_CTRL_DEVTABSEG_MASK	0x0000001c00000000ull	/* Dev Table Segm */
#define	AMDIOMMU_CTRL_DEVTABSEG_1	0x0000000000000000ull	/* 1 Segment */
#define	AMDIOMMU_CTRL_DEVTABSEG_2	0x0000000400000000ull	/* 2 Segments */
#define	AMDIOMMU_CTRL_DEVTABSEG_4	0x0000000800000000ull	/* 4 Segments */
#define	AMDIOMMU_CTRL_DEVTABSEG_8	0x0000000c00000000ull	/* 8 Segments */
#define	AMDIOMMU_CTRL_PRIVABRT_MASK	0x0000006000000000ull	/* Privilege Abort En */
#define	AMDIOMMU_CTRL_PRIVABRT_USR	0x0000000000000000ull	/* Privilege Abort User */
#define	AMDIOMMU_CTRL_PRIVABRT_ALL	0x0000002000000000ull	/* Privilege Abort Always */
#define	AMDIOMMU_CTRL_PPRAUTORSP_EN	0x0000008000000000ull	/* PPR Auto Resp En */
#define	AMDIOMMU_CTRL_MARC_EN		0x0000010000000000ull	/* Memory Addr Routing En */
#define	AMDIOMMU_CTRL_BLKSTOPMRK_EN	0x0000020000000000ull	/* Block StopMark En */
#define	AMDIOMMU_CTRL_PPRAUTORESPA_EN	0x0000040000000000ull	/* PPR Auto Resp Always En */
#define	AMDIOMMU_CTRL_NUMINTRREMAP_MASK	0x0000180000000000ull	/* Remapping MSI mode */
#define	AMDIOMMU_CTRL_NUMINTRREMAP_512	0x0000000000000000ull	/* 512 max */
#define	AMDIOMMU_CTRL_NUMINTRREMAP_2048	0x0000080000000000ull	/* 2048 max */
#define	AMDIOMMU_CTRL_EPH_EN		0x0000200000000000ull	/* Enh PPR Handling En */
#define	AMDIOMMU_CTRL_HADUP_MASK	0x0000c00000000000ull	/* Access and Dirty in host PT */
#define	AMDIOMMU_CTRL_GDUP_DIS		0x0001000000000000ull	/* Dis Dirty in guest PT */
#define	AMDIOMMU_CTRL_XT_EN		0x0004000000000000ull	/* x2APIC mode */
#define	AMDIOMMU_CTRL_INTCAPXT_EN	0x0008000000000000ull	/* x2APIC mode for IOMMU intrs */
#define	AMDIOMMU_CTRL_vCMD_EN		0x0010000000000000ull	/* vCMD buffer proc En */
#define	AMDIOMMU_CTRL_vIOMMU_EN		0x0020000000000000ull	/* vIOMMU En */
#define	AMDIOMMU_CTRL_GAUP_DIS		0x0040000000000000ull	/* Dis Access in guest PT */
#define	AMDIOMMU_CTRL_GAPPI_EN		0x0080000000000000ull	/* Guest APIC phys proc intr En */
#define	AMDIOMMU_CTRL_TMPM_EN		0x0100000000000000ull	/* Tiered Mem Page Migration En */
#define	AMDIOMMU_CTRL_GGCR3TRP_PHYS	0x0400000000000000ull	/* GCR3 is GPA (otherwise SPA) */
#define	AMDIOMMU_CTRL_IRTCACHE_DIS	0x0800000000000000ull	/* IRT Caching Dis */
#define	AMDIOMMU_CTRL_GSTBUFTRP_MODE	0x1000000000000000ull	/* See spec */
#define	AMDIOMMU_CTRL_SNPAVIC_MASK	0xe000000000000000ull	/* MBZ */

/*
 * IOMMU Extended Feature Register AMDIOMMU_EFR fields
 */
#define	AMDIOMMU_EFR_XT_SUP		0x0000000000000004ull	/* x2APIC */
#define	AMDIOMMU_EFR_HWEV_SUP		0x0000000000000100ull	/* HW Event regs */
#define	AMDIOMMU_EFR_PC_SUP		0x0000000000000200ull	/* Perf counters */
#define	AMDIOMMU_EFR_HATS_MASK		0x0000000000000c00ull	/* Host Addr Trans Size */
#define	AMDIOMMU_EFR_HATS_4LVL		0x0000000000000000ull
#define	AMDIOMMU_EFR_HATS_5LVL		0x0000000000000400ull
#define	AMDIOMMU_EFR_HATS_6LVL		0x0000000000000800ull
#define	AMDIOMMU_EFR_DEVTBLSEG_MASK	0x000000c000000000ull	/* DevTbl segmentation */
#define	AMDIOMMU_EFR_DEVTBLSEG_SHIFT	38

/* IOMMU Command Pointers (Head/Tail) registers fields */
#define	AMDIOMMU_CMDPTR_MASK		0x000000000007fff0ull

/* IOMMU Command Buffer Base fields */
#define	AMDIOMMU_CMDBUF_BASE_SZSHIFT	56			/* Shift for size */
#define	AMDIOMMU_CMDBUF_MAX		(512 * 1024)

/* IOMMU Event Log Base register fields */
#define	AMDIOMMU_EVNTLOG_BASE_SZSHIFT	56			/* Shift for size */
#define	AMDIOMMU_EVNTLOG_MIN		256
#define	AMDIOMMU_EVNTLOG_MAX		32768

/* IOMMU Hardware Event Status register fields */
#define	AMDIOMMU_HWEVS_HEV		0x00000001		/* HW Ev Valid */
#define	AMDIOMMU_HWEVS_HEO		0x00000002		/* HW Ev Overfl */

/*
 * IOMMU Command and Event Status register fields.
 * From the spec, all defined bits are either RO or RW1C.  As a consequence,
 * single bit can be safely written to the register to clean a specific
 * condition.
 */
#define	AMDIOMMU_CMDEVS_EVOVRFLW	0x00000001
#define	AMDIOMMU_CMDEVS_EVLOGINT	0x00000002
#define	AMDIOMMU_CMDEVS_COMWAITINT	0x00000004
#define	AMDIOMMU_CMDEVS_EVLOGRUN	0x00000008
#define	AMDIOMMU_CMDEVS_CMDBUFRUN	0x00000010
#define	AMDIOMMU_CMDEVS_PPROVRFLW	0x00000020
#define	AMDIOMMU_CMDEVS_PPRINT		0x00000040
#define	AMDIOMMU_CMDEVS_PPRLOGRUN	0x00000080
#define	AMDIOMMU_CMDEVS_GALOGRUN	0x00000100
#define	AMDIOMMU_CMDEVS_GALOVRFLW	0x00000200
#define	AMDIOMMU_CMDEVS_GAINT		0x00000400
#define	AMDIOMMU_CMDEVS_PPROVRFLWB	0x00000800
#define	AMDIOMMU_CMDEVS_PPRLOGACTIVE	0x00001000
#define	AMDIOMMU_CMDEVS_RESV1		0x00002000
#define	AMDIOMMU_CMDEVS_RESV2		0x00004000
#define	AMDIOMMU_CMDEVS_EVOVRFLWB	0x00008000
#define	AMDIOMMU_CMDEVS_EVLOGACTIVE	0x00010000
#define	AMDIOMMU_CMDEVS_PPROVRFLWEB	0x00020000
#define	AMDIOMMU_CMDEVS_PPROVRFLWE	0x00040000

/*
 * IOMMU Extended Feature2 register fields.
 * All currently defined bits are RO.
 */
#define	AMDIOMMU_EFR2_TMPMSUP		0x0000000000000004ull	/* Tiered Mem Migration */
#define	AMDIOMMU_EFR2_GCR3TRPM		0x0000000000000008ull	/* GPA based GCR3 pointer in DTE */
#define	AMDIOMMU_EFR2_GAPPID		0x0000000000000010ull	/* masking of GAPIC PPI */
#define	AMDIOMMU_EFR2_SNPAVIC_MASK	0x00000000000000e0ull	/* SNP-enabled Adv intr features */
#define	AMDIOMMU_EFR2_SNPAVIC_NO	0x0000000000000000ull	/* No features supported */
#define	AMDIOMMU_EFR2_SNPAVIC_REMAPV	0x0000000000000020ull	/* Intr remapping with GVAPIC */
#define	AMDIOMMU_EFR2_NUMINTRREMAP_MASK	0x0000000000000300ull	/* Number of remapped intr per dev */
#define	AMDIOMMU_EFR2_NUMINTRREMAP_512	0x0000000000000000ull	/* 512 */
#define	AMDIOMMU_EFR2_NUMINTRREMAP_2048	0x0000000000000100ull	/* 2048 */
#define	AMDIOMMU_EFR2_HTRANGEIGN	0x0000000000000800ull	/* HT range is regular GPA */

/*
 * Device Table Entry (DTE)
 */
struct amdiommu_dte {
	u_int		v:1;		/* Valid */
	u_int		tv:1;		/* Translation Valid */
	u_int		rsrv0:5;
	u_int		had:2;		/* Host Access Dirty */
	u_int		pgmode:3;	/* Paging Mode */
	uint64_t	ptroot:40;	/* Page Table Root */
	u_int		ppr:1;		/* PPR En */
	u_int		gprp:1;		/* Guest PPR Resp with PASID */
	u_int		giov:1;		/* Guest IO Prot Valid */
	u_int		gv:1;		/* Guest Translation Valid */
	u_int		glx:2;		/* Guest Levels Translated */
	u_int		gcr3root0:3;	/* GCR3 root pointer part */
	u_int		ir:1;		/* Read Perm */
	u_int		iw:1;		/* Write Perm */
	u_int		rsrv1:1;
	u_int		domainid:16;	/* domain tag */
	u_int		gcr3root1:16;	/* GCR3 root pointer part */
	u_int		i:1;		/* IOTLB En */
	u_int		se:1;		/* Suppress IO Fault Events */
	u_int		sa:1;		/* Suppress All IO Fault Events */
	u_int		pioctl:2;	/* Port IO Control */
	u_int		cache:1;	/* IOTLB Cache Hint */
	u_int		sd:1;		/* Snoop Disable */
	u_int		ex:1;		/* Allow Exclusion */
	u_int		sysmgt:2;	/* System Management Msg Handling */
	u_int		sats:1;		/* Secure/Non-secure ATS */
	u_int		gcr3root2:21;	/* GCR3 root pointer part */
	u_int		iv:1;		/* Intr Map Valid */
	u_int		inttablen:4;	/* log2 Intr Table Len */
	u_int		ig:1;		/* Ignore Unmapped Interrupts */
	uint64_t	intrroot:46;	/* Interrupt Table Root (-low 6bits) */
	u_int		rsrv2:2;
	u_int		gpm:2;		/* Guest Paging Mode */
	u_int		initpass:1;	/* INIT pass-through */
	u_int		eintpass:1;	/* ExtInt pass-through */
	u_int		nmipass:1;	/* NMI pass-through */
	u_int		hptmode:1;	/* Host Page Table Mode Hint */
	u_int		intctl:2;	/* Interrupt Control */
	u_int		lint0pass:1;	/* LINT0 pass-through */
	u_int		lint1pass:1;	/* LINT1 pass-through */
	u_int		rsrv3:15;
	u_int		vimu:1;		/* Virtualize IOMMU En */
	u_int		gdevid:16;	/* Guest Dev Id */
	u_int		gid:16;		/* Guest Id */
	u_int		rsrv4:5;
	u_int		rsrv5:1;	/* Not Checked, sw avail */
	u_int		attrv:1;	/* Attr Override Valid */
	u_int		mode0fc:1;	/* Replace for PTE.FC */
	u_int		snoopattr:8;	/* GuestPTE.PAT -> ATS.N xlat */
} __packed;
_Static_assert(sizeof(struct amdiommu_dte) == 8 * sizeof(uint32_t), "DTE");

#define	AMDIOMMU_DTE_HAD_NAND		0x0	/* No Access, No Dirty */
#define	AMDIOMMU_DTE_HAD_AND		0x1	/* Access, No Dirty */
#define	AMDIOMMU_DTE_HAD_RSRV		0x2
#define	AMDIOMMU_DTE_HAD_AD		0x3	/* Access, Dirty */

#define	AMDIOMMU_DTE_PGMODE_1T1		0x0	/* SPA = GPA */
#define	AMDIOMMU_DTE_PGMODE_1LV		0x1	/* 1 Level PT */
#define	AMDIOMMU_DTE_PGMODE_2LV		0x2	/* 2 Level PT */
#define	AMDIOMMU_DTE_PGMODE_3LV		0x3	/* 3 Level PT */
#define	AMDIOMMU_DTE_PGMODE_4LV		0x4	/* 4 Level PT */
#define	AMDIOMMU_DTE_PGMODE_5LV		0x5	/* 5 Level PT */
#define	AMDIOMMU_DTE_PGMODE_6LV		0x6	/* 6 Level PT */
#define	AMDIOMMU_DTE_PGMODE_RSRV	0x7

#define	AMDIOMMU_DTE_GLX_1LV		0x0	/* 1 Level GCR3 */
#define	AMDIOMMU_DTE_GLX_2LV		0x1	/* 2 Level GCR3 */
#define	AMDIOMMU_DTE_GLX_3LV		0x2	/* 3 Level GCR3 */
#define	AMDIOMMU_DTE_GLX_RSRV		0x3

#define	AMDIOMMU_DTE_PIOCTL_DIS		0x0
#define	AMDIOMMU_DTE_PIOCTL_EN		0x1
#define	AMDIOMMU_DTE_PIOCTL_MAP		0x2
#define	AMDIOMMU_DTE_PIOCTL_RSRV	0x3

#define	AMDIOMMU_DTE_SYSMGT_DIS		0x0	/* Target Abort */
#define	AMDIOMMU_DTE_SYSMGT_FW		0x0	/* Forwarded All */
#define	AMDIOMMU_DTE_SYSMGT_FWI		0x0	/* Forwarded INT */
#define	AMDIOMMU_DTE_SYSMGT_T		0x0	/* Translated */

#define	AMDIOMMU_DTE_GPM_4LV		0x0	/* 4 Level */
#define	AMDIOMMU_DTE_GPM_5LV		0x1	/* 5 Level */
#define	AMDIOMMU_DTE_GPM_RSRV1		0x2
#define	AMDIOMMU_DTE_GPM_RSRV2		0x3

#define	AMDIOMMU_DTE_INTCTL_DIS		0x0	/* Target Abort */
#define	AMDIOMMU_DTE_INTCTL_FW		0x1	/* Forward Unmapped */
#define	AMDIOMMU_DTE_INTCTL_MAP		0x2	/* Forward Remapped */
#define	AMDIOMMU_DTE_INTCTL_RSRV	0x3

#define	AMDIOMMU_PGTBL_MAXLVL		6

/*
 * Page Table Entry (PTE/PDE)
 */
#define	AMDIOMMU_PTE_PR			0x0001	/* Present, AKA V */
#define	AMDIOMMU_IGN1			0x0002
#define	AMDIOMMU_IGN2			0x0004
#define	AMDIOMMU_IGN3			0x0008
#define	AMDIOMMU_IGN4			0x0010
#define	AMDIOMMU_PTE_A			0x0020	/* Accessed */
#define	AMDIOMMU_PTE_D			0x0040	/* Dirty */
#define	AMDIOMMU_IGN5			0x0080
#define	AMDIOMMU_IGN6			0x0100
#define	AMDIOMMU_PTE_NLVL_MASK		0x0e00	/* Next Level */
#define	AMDIOMMU_PTE_NLVL_SHIFT		9
#define	AMDIOMMU_PTE_NLVL_7h		0x0e00	/* Magic Next Level */
#define	AMDIOMMU_PTE_PA_MASK		0x000ffffffffff000ull
#define	AMDIOMMU_PTE_PA_SHIFT		12
#define	AMDIOMMU_PTE_PMC_MASK		0x0600000000000000ull	/* Page Migr */
#define	AMDIOMMU_PTE_U			0x0800000000000000ull	/* ATS.U */
#define	AMDIOMMU_PTE_FC			0x1000000000000000ull	/* Force Coh */
#define	AMDIOMMU_PTE_IR			0x2000000000000000ull	/* Read Perm */
#define	AMDIOMMU_PTE_IW			0x4000000000000000ull	/* Write Perm */
#define	AMDIOMMU_PTE_IGN7		0x8000000000000000ull

/*
 * IRTEs
 */

/* vAPIC is not enabled, guestmode = 0 */
struct amdiommu_irte_basic_novapic {
	u_int	remapen:1;	/* 0 - Target Abort */
	u_int	supiopf:1;	/* Supress IO_PAGE_FAULT events */
	u_int	inttype:3;
	u_int	rqeoi:1;	/* Request EOI */
	u_int	dm:1;		/* Dest Mode */
	u_int	guestmode:1;	/* MBZ */
	u_int	dest:8;		/* Destination APIC */
	u_int	vector:8;
	u_int	rsrv:8;
} __packed;
_Static_assert(sizeof(struct amdiommu_irte_basic_novapic) ==
    1 * sizeof(uint32_t), "IRTE 1");

/* vAPIC is enabled, guestmode = 0 */
struct amdiommu_irte_basic_vapic {
	u_int	remapen:1;	/* 0 - Target Abort */
	u_int	supiopf:1;	/* Supress IO_PAGE_FAULT events */
	u_int	inttype:3;
	u_int	rqeoi:1;	/* Request EOI */
	u_int	dm:1;		/* Dest Mode */
	u_int	guestmode:1;	/* MBZ */
	u_int	dest:8;		/* Destination APIC */
	u_int	rsrv0:16;
	u_int	rsrv1:32;
	u_int	vector:8;
	u_int	rsrv2:24;
	u_int	rsrv3:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_irte_basic_vapic) ==
    4 * sizeof(uint32_t), "IRTE 2");

/* vAPIC is enabled, guestmode = 1 */
struct amdiommu_irte_guest_vapic {
	u_int	remapen:1;	/* 0 - Target Abort */
	u_int	supiopf:1;	/* Supress IO_PAGE_FAULT events */
	u_int	galogintr:1;
	u_int	rsrv0:2;
	u_int	gappidis:1;	/* supress GAPPI */
	u_int	isrun:1;	/* Guest Running hint */
	u_int	guestmode:1;	/* MB1 */
	u_int	dest:8;		/* Destination APIC for dorbell */
	u_int	rsrv1:16;
	u_int	gatag:32;
	u_int	vector:8;
	u_int	rsrv2:4;
	uint64_t vapicrp:40;	/* 51:12 bits of SPA for APIC backing page */
	u_int	rsrv3:12;
} __packed;
_Static_assert(sizeof(struct amdiommu_irte_guest_vapic) ==
    4 * sizeof(uint32_t), "IRTE 3");

/* vAPIC is enabled, guestmode = 0, x2APIC */
struct amdiommu_irte_basic_vapic_x2 {
	u_int	remapen:1;	/* 0 - Target Abort */
	u_int	supiopf:1;	/* Supress IO_PAGE_FAULT events */
	u_int	inttype:3;
	u_int	rqeoi:1;	/* Request EOI */
	u_int	dm:1;		/* Dest Mode */
	u_int	guestmode:1;	/* MBZ */
	u_int	dest0:24;	/* Destination APIC 23:0 */
	u_int	rsrv0:32;
	u_int	vector:8;
	u_int	rsrv1:24;
	u_int	rsrv2:24;
	u_int	dest1:8;	/* Destination APIC 31:24 */
} __packed;
_Static_assert(sizeof(struct amdiommu_irte_basic_vapic_x2) ==
    4 * sizeof(uint32_t), "IRTE 4");

/* vAPIC is enabled, guestmode = 1, x2APIC */
struct amdiommu_irte_guest_vapic_x2 {
	u_int	remapen:1;	/* 0 - Target Abort */
	u_int	supiopf:1;	/* Supress IO_PAGE_FAULT events */
	u_int	galogintr:1;
	u_int	rsrv0:2;
	u_int	gappidis:1;	/* supress GAPPI */
	u_int	isrun:1;	/* Guest Running hint */
	u_int	guestmode:1;	/* MB1 */
	u_int	dest0:24;	/* Destination APIC for dorbell 23:0 */
	u_int	gatag:32;
	u_int	vector:8;
	u_int	rsrv2:4;
	uint64_t vapicrp:40;	/* 51:12 bits of SPA for APIC backing page */
	u_int	rsrv3:4;
	u_int	dest1:8;	/* Destination APIC 31:24 */
} __packed;
_Static_assert(sizeof(struct amdiommu_irte_guest_vapic_x2) ==
    4 * sizeof(uint32_t), "IRTE 5");

#define	AMDIOMMU_IRTE_INTTYPE_FIXED	0
#define	AMDIOMMU_IRTE_INTTYPE_ARBITR	1

#define	AMDIOMMU_IRTE_DM_LOGICAL	1
#define	AMDIOMMU_IRTE_DM_PHYSICAL	1

/*
 * Commands
 */

struct amdiommu_cmd_generic {
	u_int	w0:32;
	union {
		u_int ww1:32;
		struct {
			u_int	w1:28;
			u_int	op:4;
		};
	};
	u_int	w2:32;
	u_int	w3:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_generic) ==
    4 * sizeof(uint32_t), "CMD_GENERIC");

#define	AMDIOMMU_CMD_SZ_SHIFT	4	/* Shift for cmd count
					   to ring offset */
#define	AMDIOMMU_CMD_SZ		sizeof(struct amdiommu_cmd_generic)
					/* Command size */
_Static_assert((1 << AMDIOMMU_CMD_SZ_SHIFT) == AMDIOMMU_CMD_SZ,
    "CMD size shift");

struct amdiommu_cmd_completion_wait {
	u_int	s:1;
	u_int	i:1;
	u_int	f:1;
	u_int	address0:29;	/* Store Address 31:3 */
	u_int	address1:20;	/* Store Address 51:32 */
	u_int	rsrv:8;
	u_int	op:4;
	u_int	data0:32;
	u_int	data1:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_completion_wait) ==
    4 * sizeof(uint32_t), "CMD_COMPLETION_WAIT");

struct amdiommu_cmd_invalidate_devtab_entry {
	u_int	devid:16;
	u_int	rsrv0:16;
	u_int	rsrv1:28;
	u_int	op:4;
	u_int	rsrv2:32;
	u_int	rsrv3:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_invalidate_devtab_entry) ==
    4 * sizeof(uint32_t), "CMD_INVALIDATE_DEVTAB_ENTRY");

struct amdiommu_cmd_invalidate_iommu_pages {
	u_int	pasid:20;
	u_int	rsrv0:12;
	u_int	domainid:16;
	u_int	rsrv1:12;
	u_int	op:4;
	u_int	s:1;
	u_int	pde:1;
	u_int	gn:1;
	u_int	rsrv2:9;
	uint64_t address:52;	/* Address 63:12 */
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_invalidate_iommu_pages) ==
    4 * sizeof(uint32_t), "CMD_INVALIDATE_IOMMU_PAGES");

struct amdiommu_cmd_invalidate_iotlb_pages {
	u_int	devid:16;
	u_int	pasid1:8;
	u_int	maxpend0:8;
	u_int	queueid:16;
	u_int	pasid0:8;
	u_int	pasid2:4;
	u_int	op:4;
	u_int	s:1;
	u_int	rsrv0:1;
	u_int	gn:1;
	u_int	rsrv1:1;
	u_int	type:2;
	u_int	rsrv2:6;
	uint64_t address:52;	/* Address 63:12 */
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_invalidate_iotlb_pages) ==
    4 * sizeof(uint32_t), "CMD_INVALIDATE_IOTLB_PAGES");

struct amdiommu_cmd_invalidate_interrupt_table {
	u_int	devid:16;
	u_int	rsrv0:16;
	u_int	rsrv1:28;
	u_int	op:4;
	u_int	rsrv2:32;
	u_int	rsrv3:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_invalidate_interrupt_table) ==
    4 * sizeof(uint32_t), "CMD_INVALIDATE_INTERRUPT_TABLE");

struct amdiommu_cmd_prefetch_iommu_pages {
	u_int	devid:16;
	u_int	rsrv0:8;
	u_int	pfcount:8;
	u_int	pasid:20;
	u_int	rsrv1:8;
	u_int	op:4;
	u_int	s:1;
	u_int	rsrv2:1;
	u_int	gn:1;
	u_int	rsrv3:1;
	u_int	inval:1;	/* Invalidate First */
	u_int	rsrv4:7;
	uint64_t address:52;	/* Address 63:12 */
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_prefetch_iommu_pages) ==
    4 * sizeof(uint32_t), "CMD_PREFETCH_IOMMU_PAGES");

struct amdiommu_cmd_complete_ppr_request {
	u_int	devid:16;
	u_int	rsrv0:16;
	u_int	pasid:20;
	u_int	rsrv1:8;
	u_int	op:4;
	u_int	rsrv2:2;
	u_int	gn:1;
	u_int	rsrv3:29;
	u_int	compltag:16;
	u_int	rsrv4:16;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_complete_ppr_request) ==
    4 * sizeof(uint32_t), "CMD_COMPLETE_PPR_REQUEST");

struct amdiommu_cmd_invalidate_iommu_all {
	u_int	rsrv0:32;
	u_int	op:4;
	u_int	rsrv1:28;
	u_int	rsrv2:32;
	u_int	rsrv3:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_invalidate_iommu_all) ==
    4 * sizeof(uint32_t), "CMD_INVALIDATE_IOMMU_ALL");

struct amdiommu_cmd_insert_guest_event {
	u_int	rsrv0:32;
	u_int	guestid:16;
	u_int	rsrv1:12;
	u_int	op:4;
	u_int	rsrv2:32;
	u_int	rsrv3:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_insert_guest_event) ==
    4 * sizeof(uint32_t), "CMD_INSERT_GUEST_EVENT");

struct amdiommu_cmd_reset_vmmio {
	u_int	guestid:16;
	u_int	rsrv0:11;
	u_int	all:1;
	u_int	rsrv1:3;
	u_int	vcmd:1;
	u_int	rsrv2:27;
	u_int	op:4;
	u_int	rsrv3:32;
	u_int	rsrv4:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_cmd_reset_vmmio) ==
    4 * sizeof(uint32_t), "CMD_RESET_VMMIO");

#define AMDIOMMU_CMD_COMPLETION_WAIT		0x1
#define AMDIOMMU_CMD_INVALIDATE_DEVTAB_ENTRY	0x2
#define AMDIOMMU_CMD_INVALIDATE_IOMMU_PAGES	0x3
#define AMDIOMMU_CMD_INVALIDATE_IOTLB_PAGES	0x4
#define AMDIOMMU_CMD_INVALIDATE_INTERRUPT_TABLE	0x5
#define AMDIOMMU_CMD_PREFETCH_IOMMU_PAGES	0x6
#define AMDIOMMU_CMD_COMPLETE_PPR_REQUEST	0x7
#define AMDIOMMU_CMD_INVALIDATE_IOMMU_ALL	0x8
#define AMDIOMMU_CMD_INSERT_GUEST_EVENT		0x9
#define AMDIOMMU_CMD_RESET_VMMIO		0xa

/*
 * Logging
 */
struct amdiommu_event_generic {
	u_int	w0:32;
	union {
		u_int ww1:32;
		struct {
			u_int	w1:28;
			u_int	code:4;
		};
	};
	u_int	w2:32;
	u_int	w3:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_event_generic) ==
    4 * sizeof(uint32_t), "EVENT_GENERIC");

#define	AMDIOMMU_EV_SZ_SHIFT	4	/* Shift for event count
					   to ring offset */
#define	AMDIOMMU_EV_SZ		sizeof(struct amdiommu_event_generic)
					/* Event size */
_Static_assert((1 << AMDIOMMU_EV_SZ_SHIFT) == AMDIOMMU_EV_SZ,
    "Event size shift");

struct amdiommu_event_ill_dev_table_entry {
	u_int	devid:16;
	u_int	pasid1:4;
	u_int	rsrv0:7;
	u_int	vnr:1;
	u_int	rsrv1:1;
	u_int	vevent:1;
	u_int	vptr:1;
	u_int	vcmd:1;
	u_int	pasid:16;
	u_int	gn:1;
	u_int	rsrv2:2;
	u_int	i:1;
	u_int	rsrv3:1;
	u_int	rw:1;
	u_int	rsrv4:1;
	u_int	rz:1;
	u_int	tr:1;
	u_int	rsrv5:3;
	u_int	code:4;
	u_int	rsrv6:2;
	u_int	addr1:30;
	u_int	addr2:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_event_ill_dev_table_entry) ==
    4 * sizeof(uint32_t), "EVENT_ILLEGAL_DEV_TABLE_ENTRY");

struct amdiommu_event_io_page_fault_entry {
	u_int	devid:16;
	u_int	pasid1:4;
	u_int	rsrv0:7;
	u_int	vnr:1;
	u_int	rsrv1:1;
	u_int	vevent:1;
	u_int	vptr:1;
	u_int	vcmd:1;
	u_int	pasid:16;	/* also domain id */
	u_int	gn:1;
	u_int	nx:1;
	u_int	us:1;
	u_int	i:1;
	u_int	pr:1;
	u_int	rw:1;
	u_int	pe:1;
	u_int	rz:1;
	u_int	tr:1;
	u_int	rsrv2:3;
	u_int	code:4;
	u_int	addr1:32;
	u_int	addr2:32;
} __packed;
_Static_assert(sizeof(struct amdiommu_event_io_page_fault_entry) ==
    4 * sizeof(uint32_t), "EVENT_IO_PAGE_FAULT_ENTRY");

#define	AMDIOMMU_EV_ILL_DEV_TABLE_ENTRY		0x1
#define	AMDIOMMU_EV_IO_PAGE_FAULT		0x2
#define	AMDIOMMU_EV_DEV_TAB_HW_ERROR		0x3
#define	AMDIOMMU_EV_PAGE_TAB_HW_ERROR		0x4
#define	AMDIOMMU_EV_ILL_CMD_ERROR		0x5
#define	AMDIOMMU_EV_CMD_HW_ERROR		0x6
#define	AMDIOMMU_EV_IOTLB_INV_TIMEOUT		0x7
#define	AMDIOMMU_EV_INVALID_DEV_REQ		0x8
#define	AMDIOMMU_EV_INVALID_PPR_REQ		0x9
#define	AMDIOMMU_EV_COUNTER_ZERO		0xa	/* Typo in table 42? */

#endif	/* __X86_IOMMU_AMD_REG_H */
