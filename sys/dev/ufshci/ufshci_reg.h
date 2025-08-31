/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef __UFSHCI_REG_H__
#define __UFSHCI_REG_H__

#include <sys/param.h>
#include <sys/endian.h>

/* UFSHCI 4.1, section 5.1 Register Map */
struct ufshci_registers {
	/* Host Capabilities (00h) */
	uint32_t cap;	  /* Host Controller Capabiities */
	uint32_t mcqcap;  /* Multi-Circular Queue Capability Register */
	uint32_t ver;	  /* UFS Version */
	uint32_t ext_cap; /* Extended Controller Capabilities */
	uint32_t hcpid;	  /* Product ID */
	uint32_t hcmid;	  /* Manufacturer ID */
	uint32_t ahit;	  /* Auto-Hibernate Idle Timer */
	uint32_t reserved1;
	/* Operation and Runtime (20h) */
	uint32_t is; /* Interrupt Status */
	uint32_t ie; /* Interrupt Enable */
	uint32_t reserved2;
	uint32_t hcsext;  /* Host Controller Status Extended */
	uint32_t hcs;	  /* Host Controller Status */
	uint32_t hce;	  /* Host Controller Enable */
	uint32_t uecpa;	  /* Host UIC Error Code PHY Adapter Layer */
	uint32_t uecdl;	  /* Host UIC Error Code Data Link Layer */
	uint32_t uecn;	  /* Host UIC Error Code Network Layer */
	uint32_t uect;	  /* Host UIC Error Code Transport Layer */
	uint32_t uecdme;  /* Host UIC Error Code DME */
	uint32_t utriacr; /* Interrupt Aggregation Control */
	/* UTP Transfer (50h) */
	uint32_t utrlba;  /* UTRL Base Address */
	uint32_t utrlbau; /* UTRL Base Address Upper 32-Bits */
	uint32_t utrldbr; /* UTRL DoorBell Register */
	uint32_t utrlclr; /* UTRL CLear Register */
	uint32_t utrlrsr; /* UTR Run-Stop Register */
	uint32_t utrlcnr; /* UTRL Completion Notification */
	uint64_t reserved3;
	/* UTP Task Managemeng (70h) */
	uint32_t utmrlba;  /* UTRL Base Address */
	uint32_t utmrlbau; /* UTMRL Base Address Upper 32-Bits */
	uint32_t utmrldbr; /* UTMRL DoorBell Register */
	uint32_t utmrlclr; /* UTMRL CLear Register */
	uint32_t utmrlrsr; /* UTM Run-Stop Register */
	uint8_t reserved4[12];
	/* UIC Command (90h) */
	uint32_t uiccmd;   /* UIC Command Register */
	uint32_t ucmdarg1; /* UIC Command Argument 1 */
	uint32_t ucmdarg2; /* UIC Command Argument 2 */
	uint32_t ucmdarg3; /* UIC Command Argument 3 */
	uint8_t reserved5[16];
	/* UMA (B0h) */
	uint8_t reserved6[16]; /* Reserved for Unified Memory Extension */
	/* Vendor Specific (C0h) */
	uint8_t vendor[64]; /* Vendor Specific Registers */
	/* Crypto (100h) */
	uint32_t ccap; /* Crypto Capability */
	uint32_t reserved7[511];
	/* Config (300h) */
	uint32_t config; /* Global Configuration */
	uint8_t reserved9[124];
	/* MCQ Configuration (380h) */
	uint32_t mcqconfig; /* MCQ Config Register */
	/* Event Specific Interrupt Lower Base Address */
	uint32_t esilba;
	/* Event Specific Interrupt Upper Base Address */
	uint32_t esiuba;
	/* TODO: Need to define SQ/CQ registers */
};

/* Register field definitions */
#define UFSHCI__REG__SHIFT (0)
#define UFSHCI__REG__MASK  (0)

/*
 * UFSHCI 4.1, section 5.2.1, Offset 00h: CAP
 * Controller Capabilities
 */
#define UFSHCI_CAP_REG_NUTRS_SHIFT     (0)
#define UFSHCI_CAP_REG_NUTRS_MASK      (0xFF)
#define UFSHCI_CAP_REG_NORTT_SHIFT     (8)
#define UFSHCI_CAP_REG_NORTT_MASK      (0xFF)
#define UFSHCI_CAP_REG_NUTMRS_SHIFT    (16)
#define UFSHCI_CAP_REG_NUTMRS_MASK     (0x7)
#define UFSHCI_CAP_REG_EHSLUTRDS_SHIFT (22)
#define UFSHCI_CAP_REG_EHSLUTRDS_MASK  (0x1)
#define UFSHCI_CAP_REG_AUTOH8_SHIFT    (23)
#define UFSHCI_CAP_REG_AUTOH8_MASK     (0x1)
#define UFSHCI_CAP_REG_64AS_SHIFT      (24)
#define UFSHCI_CAP_REG_64AS_MASK       (0x1)
#define UFSHCI_CAP_REG_OODDS_SHIFT     (25)
#define UFSHCI_CAP_REG_OODDS_MASK      (0x1)
#define UFSHCI_CAP_REG_UICDMETMS_SHIFT (26)
#define UFSHCI_CAP_REG_UICDMETMS_MASK  (0x1)
#define UFSHCI_CAP_REG_CS_SHIFT	       (28)
#define UFSHCI_CAP_REG_CS_MASK	       (0x1)
#define UFSHCI_CAP_REG_LSDBS_SHIFT     (29)
#define UFSHCI_CAP_REG_LSDBS_MASK      (0x1)
#define UFSHCI_CAP_REG_MCQS_SHIFT      (30)
#define UFSHCI_CAP_REG_MCQS_MASK       (0x1)
#define UFSHCI_CAP_REG_EIS_SHIFT       (31)
#define UFSHCI_CAP_REG_EIS_MASK	       (0x1)

/*
 * UFSHCI 4.1, section 5.2.2, Offset 04h: MCQCAP
 * Multi-Circular Queue Capability Register
 */
#define UFSHCI_MCQCAP_REG_MAXQ_SHIFT	(0)
#define UFSHCI_MCQCAP_REG_MAXQ_MASK	(0xFF)
#define UFSHCI_MCQCAP_REG_SP_SHIFT	(8)
#define UFSHCI_MCQCAP_REG_SP_MASK	(0x1)
#define UFSHCI_MCQCAP_REG_RRP_SHIFT	(9)
#define UFSHCI_MCQCAP_REG_RRP_MASK	(0x1)
#define UFSHCI_MCQCAP_REG_EIS_SHIFT	(10)
#define UFSHCI_MCQCAP_REG_EIS_MASK	(0x1)
#define UFSHCI_MCQCAP_REG_QCFGPTR_SHIFT (16)
#define UFSHCI_MCQCAP_REG_QCFGPTR_MASK	(0xFF)
#define UFSHCI_MCQCAP_REG_MIAG_SHIFT	(24)
#define UFSHCI_MCQCAP_REG_MIAG_MASK	(0xFF)

/*
 * UFSHCI 4.1, section 5.2.3, Offset 08h: VER
 * UFS Version
 */
#define UFSHCI_VER_REG_VS_SHIFT	 (0)
#define UFSHCI_VER_REG_VS_MASK	 (0xF)
#define UFSHCI_VER_REG_MNR_SHIFT (4)
#define UFSHCI_VER_REG_MNR_MASK	 (0xF)
#define UFSHCI_VER_REG_MJR_SHIFT (8)
#define UFSHCI_VER_REG_MJR_MASK	 (0xFF)

/*
 * UFSHCI 4.1, section 5.2.4, Offset 0Ch: EXT_CAP
 * Extended Controller Capabilities
 */
#define UFSHCI_EXTCAP_REG_HOST_HINT_CACAHE_SIZE_SHIFT (0)
#define UFSHCI_EXTCAP_REG_HOST_HINT_CACAHE_SIZE_MASK  (0xFFFF)

/*
 * UFSHCI 4.1, section 5.2.5, Offset 10h: HCPID
 * Host Controller Identification Descriptor – Product ID
 */
#define UFSHCI_HCPID_REG_PID_SHIFT (0)
#define UFSHCI_HCPID_REG_PID_MASK  (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.2.6, Offset 14h: HCMID
 * Host Controller Identification Descriptor – Manufacturer ID
 */
#define UFSHCI_HCMID_REG_MIC_SHIFT (0)
#define UFSHCI_HCMID_REG_MIC_MASK  (0xFFFF)
#define UFSHCI_HCMID_REG_BI_SHIFT  (8)
#define UFSHCI_HCMID_REG_BI_MASK   (0xFFFF)

/*
 * UFSHCI 4.1, section 5.2.7, Offset 18h: AHIT
 * Auto-Hibernate Idle Timer
 */
#define UFSHCI_AHIT_REG_AH8ITV_SHIFT (0)
#define UFSHCI_AHIT_REG_AH8ITV_MASK  (0x3FF)
#define UFSHCI_AHIT_REG_TS_SHIFT     (10)
#define UFSHCI_AHIT_REG_TS_MASK	     (0x7)

/*
 * UFSHCI 4.1, section 5.3.1, Offset 20h: IS
 * Interrupt Status
 */
#define UFSHCI_IS_REG_UTRCS_SHIFT  (0)
#define UFSHCI_IS_REG_UTRCS_MASK   (0x1)
#define UFSHCI_IS_REG_UDEPRI_SHIFT (1)
#define UFSHCI_IS_REG_UDEPRI_MASK  (0x1)
#define UFSHCI_IS_REG_UE_SHIFT	   (2)
#define UFSHCI_IS_REG_UE_MASK	   (0x1)
#define UFSHCI_IS_REG_UTMS_SHIFT   (3)
#define UFSHCI_IS_REG_UTMS_MASK	   (0x1)
#define UFSHCI_IS_REG_UPMS_SHIFT   (4)
#define UFSHCI_IS_REG_UPMS_MASK	   (0x1)
#define UFSHCI_IS_REG_UHXS_SHIFT   (5)
#define UFSHCI_IS_REG_UHXS_MASK	   (0x1)
#define UFSHCI_IS_REG_UHES_SHIFT   (6)
#define UFSHCI_IS_REG_UHES_MASK	   (0x1)
#define UFSHCI_IS_REG_ULLS_SHIFT   (7)
#define UFSHCI_IS_REG_ULLS_MASK	   (0x1)
#define UFSHCI_IS_REG_ULSS_SHIFT   (8)
#define UFSHCI_IS_REG_ULSS_MASK	   (0x1)
#define UFSHCI_IS_REG_UTMRCS_SHIFT (9)
#define UFSHCI_IS_REG_UTMRCS_MASK  (0x1)
#define UFSHCI_IS_REG_UCCS_SHIFT   (10)
#define UFSHCI_IS_REG_UCCS_MASK	   (0x1)
#define UFSHCI_IS_REG_DFES_SHIFT   (11)
#define UFSHCI_IS_REG_DFES_MASK	   (0x1)
#define UFSHCI_IS_REG_UTPES_SHIFT  (12)
#define UFSHCI_IS_REG_UTPES_MASK   (0x1)
#define UFSHCI_IS_REG_HCFES_SHIFT  (16)
#define UFSHCI_IS_REG_HCFES_MASK   (0x1)
#define UFSHCI_IS_REG_SBFES_SHIFT  (17)
#define UFSHCI_IS_REG_SBFES_MASK   (0x1)
#define UFSHCI_IS_REG_CEFES_SHIFT  (18)
#define UFSHCI_IS_REG_CEFES_MASK   (0x1)
#define UFSHCI_IS_REG_SQES_SHIFT   (19)
#define UFSHCI_IS_REG_SQES_MASK	   (0x1)
#define UFSHCI_IS_REG_CQES_SHIFT   (20)
#define UFSHCI_IS_REG_CQES_MASK	   (0x1)
#define UFSHCI_IS_REG_IAGES_SHIFT  (21)
#define UFSHCI_IS_REG_IAGES_MASK   (0x1)

/*
 * UFSHCI 4.1, section 5.3.2, Offset 24h: IE
 * Interrupt Enable
 */
#define UFSHCI_IE_REG_UTRCE_SHIFT   (0)
#define UFSHCI_IE_REG_UTRCE_MASK    (0x1)
#define UFSHCI_IE_REG_UDEPRIE_SHIFT (1)
#define UFSHCI_IE_REG_UDEPRIE_MASK  (0x1)
#define UFSHCI_IE_REG_UEE_SHIFT	    (2)
#define UFSHCI_IE_REG_UEE_MASK	    (0x1)
#define UFSHCI_IE_REG_UTMSE_SHIFT   (3)
#define UFSHCI_IE_REG_UTMSE_MASK    (0x1)
#define UFSHCI_IE_REG_UPMSE_SHIFT   (4)
#define UFSHCI_IE_REG_UPMSE_MASK    (0x1)
#define UFSHCI_IE_REG_UHXSE_SHIFT   (5)
#define UFSHCI_IE_REG_UHXSE_MASK    (0x1)
#define UFSHCI_IE_REG_UHESE_SHIFT   (6)
#define UFSHCI_IE_REG_UHESE_MASK    (0x1)
#define UFSHCI_IE_REG_ULLSE_SHIFT   (7)
#define UFSHCI_IE_REG_ULLSE_MASK    (0x1)
#define UFSHCI_IE_REG_ULSSE_SHIFT   (8)
#define UFSHCI_IE_REG_ULSSE_MASK    (0x1)
#define UFSHCI_IE_REG_UTMRCE_SHIFT  (9)
#define UFSHCI_IE_REG_UTMRCE_MASK   (0x1)
#define UFSHCI_IE_REG_UCCE_SHIFT    (10)
#define UFSHCI_IE_REG_UCCE_MASK	    (0x1)
#define UFSHCI_IE_REG_DFEE_SHIFT    (11)
#define UFSHCI_IE_REG_DFEE_MASK	    (0x1)
#define UFSHCI_IE_REG_UTPEE_SHIFT   (12)
#define UFSHCI_IE_REG_UTPEE_MASK    (0x1)
#define UFSHCI_IE_REG_HCFEE_SHIFT   (16)
#define UFSHCI_IE_REG_HCFEE_MASK    (0x1)
#define UFSHCI_IE_REG_SBFEE_SHIFT   (17)
#define UFSHCI_IE_REG_SBFEE_MASK    (0x1)
#define UFSHCI_IE_REG_CEFEE_SHIFT   (18)
#define UFSHCI_IE_REG_CEFEE_MASK    (0x1)
#define UFSHCI_IE_REG_SQEE_SHIFT    (19)
#define UFSHCI_IE_REG_SQEE_MASK	    (0x1)
#define UFSHCI_IE_REG_CQEE_SHIFT    (20)
#define UFSHCI_IE_REG_CQEE_MASK	    (0x1)
#define UFSHCI_IE_REG_IAGEE_SHIFT   (21)
#define UFSHCI_IE_REG_IAGEE_MASK    (0x1)

/*
 * UFSHCI 4.1, section 5.3.3, Offset 2Ch: HCSEXT
 * Host Controller Status Extended
 */
#define UFSHCI_HCSEXT_IIDUTPE_SHIFT	(0)
#define UFSHCI_HCSEXT_IIDUTPE_MASK	(0xF)
#define UFSHCI_HCSEXT_EXT_IIDUTPE_SHIFT (4)
#define UFSHCI_HCSEXT_EXT_IIDUTPE_MASK	(0xF)

/*
 * UFSHCI 4.1, section 5.3.4, Offset 30h: HCS
 * Host Controller Status
 */
#define UFSHCI_HCS_REG_DP_SHIFT	      (0)
#define UFSHCI_HCS_REG_DP_MASK	      (0x1)
#define UFSHCI_HCS_REG_UTRLRDY_SHIFT  (1)
#define UFSHCI_HCS_REG_UTRLRDY_MASK   (0x1)
#define UFSHCI_HCS_REG_UTMRLRDY_SHIFT (2)
#define UFSHCI_HCS_REG_UTMRLRDY_MASK  (0x1)
#define UFSHCI_HCS_REG_UCRDY_SHIFT    (3)
#define UFSHCI_HCS_REG_UCRDY_MASK     (0x1)
#define UFSHCI_HCS_REG_UPMCRS_SHIFT   (8)
#define UFSHCI_HCS_REG_UPMCRS_MASK    (0x7)
#define UFSHCI_HCS_REG_UTPEC_SHIFT    (12)
#define UFSHCI_HCS_REG_UTPEC_MASK     (0xF)
#define UFSHCI_HCS_REG_TTAGUTPE_SHIFT (16)
#define UFSHCI_HCS_REG_TTAGUTPE_MASK  (0xFF)
#define UFSHCI_HCS_REG_TLUNUTPE_SHIFT (24)
#define UFSHCI_HCS_REG_TLUNUTPE_MASK  (0xFF)

/*
 * UFSHCI 4.1, section 5.3.5, Offset 34h: HCE
 * Host Controller Enable
 */
#define UFSHCI_HCE_REG_HCE_SHIFT (0)
#define UFSHCI_HCE_REG_HCE_MASK	 (0x1)
#define UFSHCI_HCE_REG_CGE_SHIFT (1)
#define UFSHCI_HCE_REG_CGE_MASK	 (0x1)

/*
 * UFSHCI 4.1, section 5.3.6, Offset 38h: UECPA
 * Host UIC Error Code PHY Adapter Layer
 */
#define UFSHCI_UECPA_REG_EC_SHIFT  (0)
#define UFSHCI_UECPA_REG_EC_MASK   (0xF)
#define UFSHCI_UECPA_REG_ERR_SHIFT (31)
#define UFSHCI_UECPA_REG_ERR_MASK  (0x1)

/*
 * UFSHCI 4.1, section 5.3.7, Offset 3Ch: UECDL
 * Host UIC Error Code Data Link Layer
 */
#define UFSHCI_UECDL_REG_EC_SHIFT  (0)
#define UFSHCI_UECDL_REG_EC_MASK   (0xFFFF)
#define UFSHCI_UECDL_REG_ERR_SHIFT (31)
#define UFSHCI_UECDL_REG_ERR_MASK  (0x1)

/*
 * UFSHCI 4.1, section 5.3.8, Offset 40h: UECN
 * Host UIC Error Code Network Layer
 */
#define UFSHCI_UECN_REG_EC_SHIFT  (0)
#define UFSHCI_UECN_REG_EC_MASK	  (0x7)
#define UFSHCI_UECN_REG_ERR_SHIFT (31)
#define UFSHCI_UECN_REG_ERR_MASK  (0x1)

/*
 * UFSHCI 4.1, section 5.3.9, Offset 44h: UECT
 * Host UIC Error Code Transport Layer
 */
#define UFSHCI_UECT_REG_EC_SHIFT  (0)
#define UFSHCI_UECT_REG_EC_MASK	  (0x7F)
#define UFSHCI_UECT_REG_ERR_SHIFT (31)
#define UFSHCI_UECT_REG_ERR_MASK  (0x1)

/*
 * UFSHCI 4.1, section 5.3.10, Offset 48h: UECDME
 * Host UIC Error Code
 */
#define UFSHCI_UECDME_REG_EC_SHIFT  (0)
#define UFSHCI_UECDME_REG_EC_MASK   (0xF)
#define UFSHCI_UECDME_REG_ERR_SHIFT (31)
#define UFSHCI_UECDME_REG_ERR_MASK  (0x1)

/*
 * UFSHCI 4.1, section 5.4.1, Offset 50h: UTRLBA
 * UTP Transfer Request List Base Address
 */
#define UFSHCI_UTRLBA_REG_UTRLBA_SHIFT (0)
#define UFSHCI_UTRLBA_REG_UTRLBA_MASK  (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.4.2, Offset 54h: UTRLBAU
 * UTP Transfer Request List Base Address Upper 32-bits
 */
#define UFSHCI_UTRLBAU_REG_UTRLBAU_SHIFT (0)
#define UFSHCI_UTRLBAU_REG_UTRLBAU_MASK	 (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.4.3, Offset 58h: UTRLDBR
 * UTP Transfer Request List Door Bell Register
 */
#define UFSHCI_UTRLDBR_REG_UTRLDBR_SHIFT (0)
#define UFSHCI_UTRLDBR_REG_UTRLDBR_MASK	 (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.4.4, Offset 5Ch: UTRLCLR
 * UTP Transfer Request List Clear Register
 */
#define UFSHCI_UTRLCLR_REG_UTRLCLR_SHIFT (0)
#define UFSHCI_UTRLCLR_REG_UTRLCLR_MASK	 (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.4.5, Offset 60h: UTRLRSR
 * UTP Transfer Request List Run Stop Register
 */
#define UFSHCI_UTRLRSR_REG_UTRLRSR_SHIFT (0)
#define UFSHCI_UTRLRSR_REG_UTRLRSR_MASK	 (0x1)

/*
 * UFSHCI 4.1, section 5.4.6, Offset 64h: UTRLCNR
 * UTP Transfer Request List Completion Notification Register
 */
#define UFSHCI_UTRLCNR_REG_UTRLCNR_SHIFT (0)
#define UFSHCI_UTRLCNR_REG_UTRLCNR_MASK	 (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.5.1, Offset 70h: UTMRLBA
 * UTP Task Management Request List Base Address
 */
#define UFSHCI_UTMRLBA_REG_UTMRLBA_SHIFT (0)
#define UFSHCI_UTMRLBA_REG_UTMRLBA_MASK	 (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.5.2, Offset 74h: UTMRLBAU
 * UTP Task Management Request List Base Address Upper 32-bits
 */
#define UFSHCI_UTMRLBAU_REG_UTMRLBAU_SHIFT (0)
#define UFSHCI_UTMRLBAU_REG_UTMRLBAU_MASK  (0xFFFFFFFF)

/*
 * UFSHCI 4.1, section 5.5.3, Offset 78h: UTMRLDBR
 * UTP Task Management Request List Door Bell Register
 */
#define UFSHCI_UTMRLDBR_REG_UTMRLDBR_SHIFT (0)
#define UFSHCI_UTMRLDBR_REG_UTMRLDBR_MASK  (0xFF)

/*
 * UFSHCI 4.1, section 5.5.4, Offset 7Ch: UTMRLCLR
 * UTP Task Management Request List CLear Register
 */
#define UFSHCI_UTMRLCLR_REG_UTMRLCLR_SHIFT (0)
#define UFSHCI_UTMRLCLR_REG_UTMRLCLR_MASK  (0xFF)

/*
 * UFSHCI 4.1, section 5.5.5, Offset 80h: UTMRLRSR
 * UTP Task Management Request List Run Stop Register
 */
#define UFSHCI_UTMRLRSR_REG_UTMRLRSR_SHIFT (0)
#define UFSHCI_UTMRLRSR_REG_UTMRLRSR_MASK  (0xFF)

/*
 * UFSHCI 4.1, section 5.6.1
 * Offset 90h: UICCMD – UIC Command
 */
#define UFSHCI_UICCMD_REG_CMDOP_SHIFT (0)
#define UFSHCI_UICCMD_REG_CMDOP_MASK  (0xFF)

/*
 * UFSHCI 4.1, section 5.6.2
 * Offset 94h: UICCMDARG1 – UIC Command Argument 1
 */
#define UFSHCI_UICCMDARG1_REG_ARG1_SHIFT	       (0)
#define UFSHCI_UICCMDARG1_REG_ARG1_MASK		       (0xFFFFFFFF)
#define UFSHCI_UICCMDARG1_REG_GEN_SELECTOR_INDEX_SHIFT (0)
#define UFSHCI_UICCMDARG1_REG_GEN_SELECTOR_INDEX_MASK  (0xFFFF)
#define UFSHCI_UICCMDARG1_REG_MIB_ATTR_SHIFT	       (16)
#define UFSHCI_UICCMDARG1_REG_MIB_ATTR_MASK	       (0xFFFF)

/*
 * UFSHCI 4.1, section 5.6.3
 * Offset 98h: UICCMDARG2 – UIC Command Argument 2
 */
#define UFSHCI_UICCMDARG2_REG_ARG2_SHIFT	  (0)
#define UFSHCI_UICCMDARG2_REG_ARG2_MASK		  (0xFFFFFFFF)
#define UFSHCI_UICCMDARG2_REG_ERROR_CODE_SHIFT	  (0)
#define UFSHCI_UICCMDARG2_REG_ERROR_CODE_MASK	  (0xFF)
#define UFSHCI_UICCMDARG2_REG_ATTR_SET_TYPE_SHIFT (16)
#define UFSHCI_UICCMDARG2_REG_ATTR_SET_TYPE_MASK  (0xFF)

/*
 * UFSHCI 4.1, section 5.6.4
 * Offset 9Ch: UICCMDARG3 – UIC Command Argument 3
 */
#define UFSHCI_UICCMDARG3_REG_ARG3_SHIFT (0)
#define UFSHCI_UICCMDARG3_REG_ARG3_MASK	 (0xFFFFFFFF)

/* Helper macro to combine *_MASK and *_SHIFT defines */
#define UFSHCIM(name) (name##_MASK << name##_SHIFT)

/* Helper macro to extract value from x */
#define UFSHCIV(name, x) (((x) >> name##_SHIFT) & name##_MASK)

/* Helper macro to construct a field value */
#define UFSHCIF(name, x) (((x)&name##_MASK) << name##_SHIFT)

#define UFSHCI_DUMP_REG(ctrlr, member)                                        \
	do {                                                                  \
		uint32_t _val = ufshci_mmio_read_4(ctrlr, member);            \
		ufshci_printf(ctrlr, "  %-15s (0x%03lx) : 0x%08x\n", #member, \
		    ufshci_mmio_offsetof(member), _val);                      \
	} while (0)

#endif /* __UFSHCI_REG_H__ */
