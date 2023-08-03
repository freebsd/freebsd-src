/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Nicole Graziano <nicole@nextbsd.org>
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
 */

/* $FreeBSD$ */
#include "if_em.h"
#include <sys/sbuf.h>
#include <machine/_inttypes.h>

#define em_mac_min e1000_82571
#define igb_mac_min e1000_82575

/*********************************************************************
 *  Driver version:
 *********************************************************************/
char em_driver_version[] = "7.7.8-fbsd";
char igb_driver_version[] = "2.5.19-fbsd";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into e1000_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static pci_vendor_info_t em_vendor_info_array[] =
{
	/* Intel(R) - lem-class legacy devices */
	PVID(0x8086, E1000_DEV_ID_82540EM, "Intel(R) Legacy PRO/1000 MT 82540EM"),
	PVID(0x8086, E1000_DEV_ID_82540EM_LOM, "Intel(R) Legacy PRO/1000 MT 82540EM (LOM)"),
	PVID(0x8086, E1000_DEV_ID_82540EP, "Intel(R) Legacy PRO/1000 MT 82540EP"),
	PVID(0x8086, E1000_DEV_ID_82540EP_LOM, "Intel(R) Legacy PRO/1000 MT 82540EP (LOM)"),
	PVID(0x8086, E1000_DEV_ID_82540EP_LP, "Intel(R) Legacy PRO/1000 MT 82540EP (Mobile)"),

	PVID(0x8086, E1000_DEV_ID_82541EI, "Intel(R) Legacy PRO/1000 MT 82541EI (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82541ER, "Intel(R) Legacy PRO/1000 82541ER"),
	PVID(0x8086, E1000_DEV_ID_82541ER_LOM, "Intel(R) Legacy PRO/1000 MT 82541ER"),
	PVID(0x8086, E1000_DEV_ID_82541EI_MOBILE, "Intel(R) Legacy PRO/1000 MT 82541EI (Mobile)"),
	PVID(0x8086, E1000_DEV_ID_82541GI, "Intel(R) Legacy PRO/1000 MT 82541GI"),
	PVID(0x8086, E1000_DEV_ID_82541GI_LF, "Intel(R) Legacy PRO/1000 GT 82541PI"),
	PVID(0x8086, E1000_DEV_ID_82541GI_MOBILE, "Intel(R) Legacy PRO/1000 MT 82541GI (Mobile)"),

	PVID(0x8086, E1000_DEV_ID_82542, "Intel(R) Legacy PRO/1000 82542 (Fiber)"),

	PVID(0x8086, E1000_DEV_ID_82543GC_FIBER, "Intel(R) Legacy PRO/1000 F 82543GC (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82543GC_COPPER, "Intel(R) Legacy PRO/1000 T 82543GC (Copper)"),

	PVID(0x8086, E1000_DEV_ID_82544EI_COPPER, "Intel(R) Legacy PRO/1000 XT 82544EI (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82544EI_FIBER, "Intel(R) Legacy PRO/1000 XF 82544EI (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82544GC_COPPER, "Intel(R) Legacy PRO/1000 T 82544GC (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82544GC_LOM, "Intel(R) Legacy PRO/1000 XT 82544GC (LOM)"),

	PVID(0x8086, E1000_DEV_ID_82545EM_COPPER, "Intel(R) Legacy PRO/1000 MT 82545EM (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82545EM_FIBER, "Intel(R) Legacy PRO/1000 MF 82545EM (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82545GM_COPPER, "Intel(R) Legacy PRO/1000 MT 82545GM (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82545GM_FIBER, "Intel(R) Legacy PRO/1000 MF 82545GM (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82545GM_SERDES, "Intel(R) Legacy PRO/1000 MB 82545GM (SERDES)"),

	PVID(0x8086, E1000_DEV_ID_82546EB_COPPER, "Intel(R) Legacy PRO/1000 MT 82546EB (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82546EB_FIBER, "Intel(R) Legacy PRO/1000 MF 82546EB (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82546EB_QUAD_COPPER, "Intel(R) Legacy PRO/1000 MT 82546EB (Quad Copper"),
	PVID(0x8086, E1000_DEV_ID_82546GB_COPPER, "Intel(R) Legacy PRO/1000 MT 82546GB (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82546GB_FIBER, "Intel(R) Legacy PRO/1000 MF 82546GB (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82546GB_SERDES, "Intel(R) Legacy PRO/1000 MB 82546GB (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82546GB_PCIE, "Intel(R) Legacy PRO/1000 P 82546GB (PCIe)"),
	PVID(0x8086, E1000_DEV_ID_82546GB_QUAD_COPPER, "Intel(R) Legacy PRO/1000 GT 82546GB (Quad Copper)"),
	PVID(0x8086, E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3, "Intel(R) Legacy PRO/1000 GT 82546GB (Quad Copper)"),

	PVID(0x8086, E1000_DEV_ID_82547EI, "Intel(R) Legacy PRO/1000 CT 82547EI"),
	PVID(0x8086, E1000_DEV_ID_82547EI_MOBILE, "Intel(R) Legacy PRO/1000 CT 82547EI (Mobile)"),
	PVID(0x8086, E1000_DEV_ID_82547GI, "Intel(R) Legacy PRO/1000 CT 82547GI"),

	/* Intel(R) - em-class devices */
	PVID(0x8086, E1000_DEV_ID_82571EB_COPPER, "Intel(R) PRO/1000 PT 82571EB/82571GB (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82571EB_FIBER, "Intel(R) PRO/1000 PF 82571EB/82571GB (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82571EB_SERDES, "Intel(R) PRO/1000 PB 82571EB (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82571EB_SERDES_DUAL, "Intel(R) PRO/1000 82571EB (Dual Mezzanine)"),
	PVID(0x8086, E1000_DEV_ID_82571EB_SERDES_QUAD, "Intel(R) PRO/1000 82571EB (Quad Mezzanine)"),
	PVID(0x8086, E1000_DEV_ID_82571EB_QUAD_COPPER, "Intel(R) PRO/1000 PT 82571EB/82571GB (Quad Copper)"),
	PVID(0x8086, E1000_DEV_ID_82571EB_QUAD_COPPER_LP, "Intel(R) PRO/1000 PT 82571EB/82571GB (Quad Copper)"),
	PVID(0x8086, E1000_DEV_ID_82571EB_QUAD_FIBER, "Intel(R) PRO/1000 PF 82571EB (Quad Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82571PT_QUAD_COPPER, "Intel(R) PRO/1000 PT 82571PT (Quad Copper)"),
	PVID(0x8086, E1000_DEV_ID_82572EI, "Intel(R) PRO/1000 PT 82572EI (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82572EI_COPPER, "Intel(R) PRO/1000 PT 82572EI (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82572EI_FIBER, "Intel(R) PRO/1000 PF 82572EI (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82572EI_SERDES, "Intel(R) PRO/1000 82572EI (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82573E, "Intel(R) PRO/1000 82573E (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82573E_IAMT, "Intel(R) PRO/1000 82573E AMT (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82573L, "Intel(R) PRO/1000 82573L"),
	PVID(0x8086, E1000_DEV_ID_82583V, "Intel(R) 82583V"),
	PVID(0x8086, E1000_DEV_ID_80003ES2LAN_COPPER_SPT, "Intel(R) 80003ES2LAN (Copper)"),
	PVID(0x8086, E1000_DEV_ID_80003ES2LAN_SERDES_SPT, "Intel(R) 80003ES2LAN (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_80003ES2LAN_COPPER_DPT, "Intel(R) 80003ES2LAN (Dual Copper)"),
	PVID(0x8086, E1000_DEV_ID_80003ES2LAN_SERDES_DPT, "Intel(R) 80003ES2LAN (Dual SERDES)"),
	PVID(0x8086, E1000_DEV_ID_ICH8_IGP_M_AMT, "Intel(R) 82566MM ICH8 AMT (Mobile)"),
	PVID(0x8086, E1000_DEV_ID_ICH8_IGP_AMT, "Intel(R) 82566DM ICH8 AMT"),
	PVID(0x8086, E1000_DEV_ID_ICH8_IGP_C, "Intel(R) 82566DC ICH8"),
	PVID(0x8086, E1000_DEV_ID_ICH8_IFE, "Intel(R) 82562V ICH8"),
	PVID(0x8086, E1000_DEV_ID_ICH8_IFE_GT, "Intel(R) 82562GT ICH8"),
	PVID(0x8086, E1000_DEV_ID_ICH8_IFE_G, "Intel(R) 82562G ICH8"),
	PVID(0x8086, E1000_DEV_ID_ICH8_IGP_M, "Intel(R) 82566MC ICH8"),
	PVID(0x8086, E1000_DEV_ID_ICH8_82567V_3, "Intel(R) 82567V-3 ICH8"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IGP_M_AMT, "Intel(R) 82567LM ICH9 AMT"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IGP_AMT, "Intel(R) 82566DM-2 ICH9 AMT"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IGP_C, "Intel(R) 82566DC-2 ICH9"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IGP_M, "Intel(R) 82567LF ICH9"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IGP_M_V, "Intel(R) 82567V ICH9"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IFE, "Intel(R) 82562V-2 ICH9"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IFE_GT, "Intel(R) 82562GT-2 ICH9"),
	PVID(0x8086, E1000_DEV_ID_ICH9_IFE_G, "Intel(R) 82562G-2 ICH9"),
	PVID(0x8086, E1000_DEV_ID_ICH9_BM, "Intel(R) 82567LM-4 ICH9"),
	PVID(0x8086, E1000_DEV_ID_82574L, "Intel(R) Gigabit CT 82574L"),
	PVID(0x8086, E1000_DEV_ID_82574LA, "Intel(R) 82574L-Apple"),
	PVID(0x8086, E1000_DEV_ID_ICH10_R_BM_LM, "Intel(R) 82567LM-2 ICH10"),
	PVID(0x8086, E1000_DEV_ID_ICH10_R_BM_LF, "Intel(R) 82567LF-2 ICH10"),
	PVID(0x8086, E1000_DEV_ID_ICH10_R_BM_V, "Intel(R) 82567V-2 ICH10"),
	PVID(0x8086, E1000_DEV_ID_ICH10_D_BM_LM, "Intel(R) 82567LM-3 ICH10"),
	PVID(0x8086, E1000_DEV_ID_ICH10_D_BM_LF, "Intel(R) 82567LF-3 ICH10"),
	PVID(0x8086, E1000_DEV_ID_ICH10_D_BM_V, "Intel(R) 82567V-4 ICH10"),
	PVID(0x8086, E1000_DEV_ID_PCH_M_HV_LM, "Intel(R) 82577LM"),
	PVID(0x8086, E1000_DEV_ID_PCH_M_HV_LC, "Intel(R) 82577LC"),
	PVID(0x8086, E1000_DEV_ID_PCH_D_HV_DM, "Intel(R) 82578DM"),
	PVID(0x8086, E1000_DEV_ID_PCH_D_HV_DC, "Intel(R) 82578DC"),
	PVID(0x8086, E1000_DEV_ID_PCH2_LV_LM, "Intel(R) 82579LM"),
	PVID(0x8086, E1000_DEV_ID_PCH2_LV_V, "Intel(R) 82579V"),
	PVID(0x8086, E1000_DEV_ID_PCH_LPT_I217_LM, "Intel(R) I217-LM LPT"),
	PVID(0x8086, E1000_DEV_ID_PCH_LPT_I217_V, "Intel(R) I217-V LPT"),
	PVID(0x8086, E1000_DEV_ID_PCH_LPTLP_I218_LM, "Intel(R) I218-LM LPTLP"),
	PVID(0x8086, E1000_DEV_ID_PCH_LPTLP_I218_V, "Intel(R) I218-V LPTLP"),
	PVID(0x8086, E1000_DEV_ID_PCH_I218_LM2, "Intel(R) I218-LM (2)"),
	PVID(0x8086, E1000_DEV_ID_PCH_I218_V2, "Intel(R) I218-V (2)"),
	PVID(0x8086, E1000_DEV_ID_PCH_I218_LM3, "Intel(R) I218-LM (3)"),
	PVID(0x8086, E1000_DEV_ID_PCH_I218_V3, "Intel(R) I218-V (3)"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_LM, "Intel(R) I219-LM SPT"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_V, "Intel(R) I219-V SPT"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_LM2, "Intel(R) I219-LM SPT-H(2)"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_V2, "Intel(R) I219-V SPT-H(2)"),
	PVID(0x8086, E1000_DEV_ID_PCH_LBG_I219_LM3, "Intel(R) I219-LM LBG(3)"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_LM4, "Intel(R) I219-LM SPT(4)"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_V4, "Intel(R) I219-V SPT(4)"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_LM5, "Intel(R) I219-LM SPT(5)"),
	PVID(0x8086, E1000_DEV_ID_PCH_SPT_I219_V5, "Intel(R) I219-V SPT(5)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CNP_I219_LM6, "Intel(R) I219-LM CNP(6)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CNP_I219_V6, "Intel(R) I219-V CNP(6)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CNP_I219_LM7, "Intel(R) I219-LM CNP(7)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CNP_I219_V7, "Intel(R) I219-V CNP(7)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ICP_I219_LM8, "Intel(R) I219-LM ICP(8)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ICP_I219_V8, "Intel(R) I219-V ICP(8)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ICP_I219_LM9, "Intel(R) I219-LM ICP(9)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ICP_I219_V9, "Intel(R) I219-V ICP(9)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CMP_I219_LM10, "Intel(R) I219-LM CMP(10)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CMP_I219_V10, "Intel(R) I219-V CMP(10)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CMP_I219_LM11, "Intel(R) I219-LM CMP(11)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CMP_I219_V11, "Intel(R) I219-V CMP(11)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CMP_I219_LM12, "Intel(R) I219-LM CMP(12)"),
	PVID(0x8086, E1000_DEV_ID_PCH_CMP_I219_V12, "Intel(R) I219-V CMP(12)"),
	PVID(0x8086, E1000_DEV_ID_PCH_TGP_I219_LM13, "Intel(R) I219-LM TGP(13)"),
	PVID(0x8086, E1000_DEV_ID_PCH_TGP_I219_V13, "Intel(R) I219-V TGP(13)"),
	PVID(0x8086, E1000_DEV_ID_PCH_TGP_I219_LM14, "Intel(R) I219-LM TGP(14)"),
	PVID(0x8086, E1000_DEV_ID_PCH_TGP_I219_V14, "Intel(R) I219-V GTP(14)"),
	PVID(0x8086, E1000_DEV_ID_PCH_TGP_I219_LM15, "Intel(R) I219-LM TGP(15)"),
	PVID(0x8086, E1000_DEV_ID_PCH_TGP_I219_V15, "Intel(R) I219-V TGP(15)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ADL_I219_LM16, "Intel(R) I219-LM ADL(16)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ADL_I219_V16, "Intel(R) I219-V ADL(16)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ADL_I219_LM17, "Intel(R) I219-LM ADL(17)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ADL_I219_V17, "Intel(R) I219-V ADL(17)"),
	PVID(0x8086, E1000_DEV_ID_PCH_MTP_I219_LM18, "Intel(R) I219-LM MTP(18)"),
	PVID(0x8086, E1000_DEV_ID_PCH_MTP_I219_V18, "Intel(R) I219-V MTP(18)"),
	PVID(0x8086, E1000_DEV_ID_PCH_MTP_I219_LM19, "Intel(R) I219-LM MTP(19)"),
	PVID(0x8086, E1000_DEV_ID_PCH_MTP_I219_V19, "Intel(R) I219-V MTP(19)"),
	PVID(0x8086, E1000_DEV_ID_PCH_LNL_I219_LM20, "Intel(R) I219-LM LNL(20)"),
	PVID(0x8086, E1000_DEV_ID_PCH_LNL_I219_V20, "Intel(R) I219-V LNL(20)"),
	PVID(0x8086, E1000_DEV_ID_PCH_LNL_I219_LM21, "Intel(R) I219-LM LNL(21)"),
	PVID(0x8086, E1000_DEV_ID_PCH_LNL_I219_V21, "Intel(R) I219-V LNL(21)"),
	PVID(0x8086, E1000_DEV_ID_PCH_RPL_I219_LM22, "Intel(R) I219-LM RPL(22)"),
	PVID(0x8086, E1000_DEV_ID_PCH_RPL_I219_V22, "Intel(R) I219-V RPL(22)"),
	PVID(0x8086, E1000_DEV_ID_PCH_RPL_I219_LM23, "Intel(R) I219-LM RPL(23)"),
	PVID(0x8086, E1000_DEV_ID_PCH_RPL_I219_V23, "Intel(R) I219-V RPL(23)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ARL_I219_LM24, "Intel(R) I219-LM ARL(24)"),
	PVID(0x8086, E1000_DEV_ID_PCH_ARL_I219_V24, "Intel(R) I219-V ARL(24)"),
	PVID(0x8086, E1000_DEV_ID_PCH_PTP_I219_LM25, "Intel(R) I219-LM PTP(25)"),
	PVID(0x8086, E1000_DEV_ID_PCH_PTP_I219_V25, "Intel(R) I219-V PTP(25)"),
	PVID(0x8086, E1000_DEV_ID_PCH_PTP_I219_LM26, "Intel(R) I219-LM PTP(26)"),
	PVID(0x8086, E1000_DEV_ID_PCH_PTP_I219_V26, "Intel(R) I219-V PTP(26)"),
	PVID(0x8086, E1000_DEV_ID_PCH_PTP_I219_LM27, "Intel(R) I219-LM PTP(27)"),
	PVID(0x8086, E1000_DEV_ID_PCH_PTP_I219_V27, "Intel(R) I219-V PTP(27)"),
	/* required last entry */
	PVID_END
};

static pci_vendor_info_t igb_vendor_info_array[] =
{
	/* Intel(R) - igb-class devices */
	PVID(0x8086, E1000_DEV_ID_82575EB_COPPER, "Intel(R) PRO/1000 82575EB (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82575EB_FIBER_SERDES, "Intel(R) PRO/1000 82575EB (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82575GB_QUAD_COPPER, "Intel(R) PRO/1000 VT 82575GB (Quad Copper)"),
	PVID(0x8086, E1000_DEV_ID_82576, "Intel(R) PRO/1000 82576"),
	PVID(0x8086, E1000_DEV_ID_82576_NS, "Intel(R) PRO/1000 82576NS"),
	PVID(0x8086, E1000_DEV_ID_82576_NS_SERDES, "Intel(R) PRO/1000 82576NS (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82576_FIBER, "Intel(R) PRO/1000 EF 82576 (Dual Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82576_SERDES, "Intel(R) PRO/1000 82576 (Dual SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82576_SERDES_QUAD, "Intel(R) PRO/1000 ET 82576 (Quad SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82576_QUAD_COPPER, "Intel(R) PRO/1000 ET 82576 (Quad Copper)"),
	PVID(0x8086, E1000_DEV_ID_82576_QUAD_COPPER_ET2, "Intel(R) PRO/1000 ET(2) 82576 (Quad Copper)"),
	PVID(0x8086, E1000_DEV_ID_82576_VF, "Intel(R) PRO/1000 82576 Virtual Function"),
	PVID(0x8086, E1000_DEV_ID_82580_COPPER, "Intel(R) I340 82580 (Copper)"),
	PVID(0x8086, E1000_DEV_ID_82580_FIBER, "Intel(R) I340 82580 (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_82580_SERDES, "Intel(R) I340 82580 (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_82580_SGMII, "Intel(R) I340 82580 (SGMII)"),
	PVID(0x8086, E1000_DEV_ID_82580_COPPER_DUAL, "Intel(R) I340-T2 82580 (Dual Copper)"),
	PVID(0x8086, E1000_DEV_ID_82580_QUAD_FIBER, "Intel(R) I340-F4 82580 (Quad Fiber)"),
	PVID(0x8086, E1000_DEV_ID_DH89XXCC_SERDES, "Intel(R) DH89XXCC (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_DH89XXCC_SGMII, "Intel(R) I347-AT4 DH89XXCC"),
	PVID(0x8086, E1000_DEV_ID_DH89XXCC_SFP, "Intel(R) DH89XXCC (SFP)"),
	PVID(0x8086, E1000_DEV_ID_DH89XXCC_BACKPLANE, "Intel(R) DH89XXCC (Backplane)"),
	PVID(0x8086, E1000_DEV_ID_I350_COPPER, "Intel(R) I350 (Copper)"),
	PVID(0x8086, E1000_DEV_ID_I350_FIBER, "Intel(R) I350 (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_I350_SERDES, "Intel(R) I350 (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_I350_SGMII, "Intel(R) I350 (SGMII)"),
	PVID(0x8086, E1000_DEV_ID_I350_VF, "Intel(R) I350 Virtual Function"),
	PVID(0x8086, E1000_DEV_ID_I210_COPPER, "Intel(R) I210 (Copper)"),
	PVID(0x8086, E1000_DEV_ID_I210_COPPER_IT, "Intel(R) I210 IT (Copper)"),
	PVID(0x8086, E1000_DEV_ID_I210_COPPER_OEM1, "Intel(R) I210 (OEM)"),
	PVID(0x8086, E1000_DEV_ID_I210_COPPER_FLASHLESS, "Intel(R) I210 Flashless (Copper)"),
	PVID(0x8086, E1000_DEV_ID_I210_SERDES_FLASHLESS, "Intel(R) I210 Flashless (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_I210_SGMII_FLASHLESS, "Intel(R) I210 Flashless (SGMII)"),
	PVID(0x8086, E1000_DEV_ID_I210_FIBER, "Intel(R) I210 (Fiber)"),
	PVID(0x8086, E1000_DEV_ID_I210_SERDES, "Intel(R) I210 (SERDES)"),
	PVID(0x8086, E1000_DEV_ID_I210_SGMII, "Intel(R) I210 (SGMII)"),
	PVID(0x8086, E1000_DEV_ID_I211_COPPER, "Intel(R) I211 (Copper)"),
	PVID(0x8086, E1000_DEV_ID_I354_BACKPLANE_1GBPS, "Intel(R) I354 (1.0 GbE Backplane)"),
	PVID(0x8086, E1000_DEV_ID_I354_BACKPLANE_2_5GBPS, "Intel(R) I354 (2.5 GbE Backplane)"),
	PVID(0x8086, E1000_DEV_ID_I354_SGMII, "Intel(R) I354 (SGMII)"),
	/* required last entry */
	PVID_END
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static void	*em_register(device_t);
static void	*igb_register(device_t);
static int	em_if_attach_pre(if_ctx_t);
static int	em_if_attach_post(if_ctx_t);
static int	em_if_detach(if_ctx_t);
static int	em_if_shutdown(if_ctx_t);
static int	em_if_suspend(if_ctx_t);
static int	em_if_resume(if_ctx_t);

static int	em_if_tx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static int	em_if_rx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static void	em_if_queues_free(if_ctx_t);

static uint64_t	em_if_get_counter(if_ctx_t, ift_counter);
static void	em_if_init(if_ctx_t);
static void	em_if_stop(if_ctx_t);
static void	em_if_media_status(if_ctx_t, struct ifmediareq *);
static int	em_if_media_change(if_ctx_t);
static int	em_if_mtu_set(if_ctx_t, uint32_t);
static void	em_if_timer(if_ctx_t, uint16_t);
static void	em_if_vlan_register(if_ctx_t, u16);
static void	em_if_vlan_unregister(if_ctx_t, u16);
static void	em_if_watchdog_reset(if_ctx_t);
static bool	em_if_needs_restart(if_ctx_t, enum iflib_restart_event);

static void	em_identify_hardware(if_ctx_t);
static int	em_allocate_pci_resources(if_ctx_t);
static void	em_free_pci_resources(if_ctx_t);
static void	em_reset(if_ctx_t);
static int	em_setup_interface(if_ctx_t);
static int	em_setup_msix(if_ctx_t);

static void	em_initialize_transmit_unit(if_ctx_t);
static void	em_initialize_receive_unit(if_ctx_t);

static void	em_if_intr_enable(if_ctx_t);
static void	em_if_intr_disable(if_ctx_t);
static void	igb_if_intr_enable(if_ctx_t);
static void	igb_if_intr_disable(if_ctx_t);
static int	em_if_rx_queue_intr_enable(if_ctx_t, uint16_t);
static int	em_if_tx_queue_intr_enable(if_ctx_t, uint16_t);
static int	igb_if_rx_queue_intr_enable(if_ctx_t, uint16_t);
static int	igb_if_tx_queue_intr_enable(if_ctx_t, uint16_t);
static void	em_if_multi_set(if_ctx_t);
static void	em_if_update_admin_status(if_ctx_t);
static void	em_if_debug(if_ctx_t);
static void	em_update_stats_counters(struct e1000_softc *);
static void	em_add_hw_stats(struct e1000_softc *);
static int	em_if_set_promisc(if_ctx_t, int);
static bool	em_if_vlan_filter_capable(if_ctx_t);
static bool	em_if_vlan_filter_used(if_ctx_t);
static void	em_if_vlan_filter_enable(struct e1000_softc *);
static void	em_if_vlan_filter_disable(struct e1000_softc *);
static void	em_if_vlan_filter_write(struct e1000_softc *);
static void	em_setup_vlan_hw_support(if_ctx_t ctx);
static int	em_sysctl_nvm_info(SYSCTL_HANDLER_ARGS);
static void	em_print_nvm_info(struct e1000_softc *);
static void	em_fw_version_locked(if_ctx_t);
static void	em_sbuf_fw_version(struct e1000_fw_version *, struct sbuf *);
static void	em_print_fw_version(struct e1000_softc *);
static int	em_sysctl_print_fw_version(SYSCTL_HANDLER_ARGS);
static int	em_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
static int	em_get_rs(SYSCTL_HANDLER_ARGS);
static void	em_print_debug_info(struct e1000_softc *);
static int 	em_is_valid_ether_addr(u8 *);
static bool	em_automask_tso(if_ctx_t);
static int	em_sysctl_int_delay(SYSCTL_HANDLER_ARGS);
static void	em_add_int_delay_sysctl(struct e1000_softc *, const char *,
		    const char *, struct em_int_delay_info *, int, int);
/* Management and WOL Support */
static void	em_init_manageability(struct e1000_softc *);
static void	em_release_manageability(struct e1000_softc *);
static void	em_get_hw_control(struct e1000_softc *);
static void	em_release_hw_control(struct e1000_softc *);
static void	em_get_wakeup(if_ctx_t);
static void	em_enable_wakeup(if_ctx_t);
static int	em_enable_phy_wakeup(struct e1000_softc *);
static void	em_disable_aspm(struct e1000_softc *);

int		em_intr(void *);

/* MSI-X handlers */
static int	em_if_msix_intr_assign(if_ctx_t, int);
static int	em_msix_link(void *);
static void	em_handle_link(void *);

static void	em_enable_vectors_82574(if_ctx_t);

static int	em_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	em_sysctl_eee(SYSCTL_HANDLER_ARGS);
static void	em_if_led_func(if_ctx_t, int);

static int	em_get_regs(SYSCTL_HANDLER_ARGS);

static void	lem_smartspeed(struct e1000_softc *);
static void	igb_configure_queues(struct e1000_softc *);
static void	em_flush_desc_rings(struct e1000_softc *);


/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/
static device_method_t em_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, em_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),
	DEVMETHOD_END
};

static device_method_t igb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, igb_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),
	DEVMETHOD_END
};


static driver_t em_driver = {
	"em", em_methods, sizeof(struct e1000_softc),
};

DRIVER_MODULE(em, pci, em_driver, 0, 0);

MODULE_DEPEND(em, pci, 1, 1, 1);
MODULE_DEPEND(em, ether, 1, 1, 1);
MODULE_DEPEND(em, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, em, em_vendor_info_array);

static driver_t igb_driver = {
	"igb", igb_methods, sizeof(struct e1000_softc),
};

DRIVER_MODULE(igb, pci, igb_driver, 0, 0);

MODULE_DEPEND(igb, pci, 1, 1, 1);
MODULE_DEPEND(igb, ether, 1, 1, 1);
MODULE_DEPEND(igb, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, igb, igb_vendor_info_array);

static device_method_t em_if_methods[] = {
	DEVMETHOD(ifdi_attach_pre, em_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, em_if_attach_post),
	DEVMETHOD(ifdi_detach, em_if_detach),
	DEVMETHOD(ifdi_shutdown, em_if_shutdown),
	DEVMETHOD(ifdi_suspend, em_if_suspend),
	DEVMETHOD(ifdi_resume, em_if_resume),
	DEVMETHOD(ifdi_init, em_if_init),
	DEVMETHOD(ifdi_stop, em_if_stop),
	DEVMETHOD(ifdi_msix_intr_assign, em_if_msix_intr_assign),
	DEVMETHOD(ifdi_intr_enable, em_if_intr_enable),
	DEVMETHOD(ifdi_intr_disable, em_if_intr_disable),
	DEVMETHOD(ifdi_tx_queues_alloc, em_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, em_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, em_if_queues_free),
	DEVMETHOD(ifdi_update_admin_status, em_if_update_admin_status),
	DEVMETHOD(ifdi_multi_set, em_if_multi_set),
	DEVMETHOD(ifdi_media_status, em_if_media_status),
	DEVMETHOD(ifdi_media_change, em_if_media_change),
	DEVMETHOD(ifdi_mtu_set, em_if_mtu_set),
	DEVMETHOD(ifdi_promisc_set, em_if_set_promisc),
	DEVMETHOD(ifdi_timer, em_if_timer),
	DEVMETHOD(ifdi_watchdog_reset, em_if_watchdog_reset),
	DEVMETHOD(ifdi_vlan_register, em_if_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, em_if_vlan_unregister),
	DEVMETHOD(ifdi_get_counter, em_if_get_counter),
	DEVMETHOD(ifdi_led_func, em_if_led_func),
	DEVMETHOD(ifdi_rx_queue_intr_enable, em_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, em_if_tx_queue_intr_enable),
	DEVMETHOD(ifdi_debug, em_if_debug),
	DEVMETHOD(ifdi_needs_restart, em_if_needs_restart),
	DEVMETHOD_END
};

static driver_t em_if_driver = {
	"em_if", em_if_methods, sizeof(struct e1000_softc)
};

static device_method_t igb_if_methods[] = {
	DEVMETHOD(ifdi_attach_pre, em_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, em_if_attach_post),
	DEVMETHOD(ifdi_detach, em_if_detach),
	DEVMETHOD(ifdi_shutdown, em_if_shutdown),
	DEVMETHOD(ifdi_suspend, em_if_suspend),
	DEVMETHOD(ifdi_resume, em_if_resume),
	DEVMETHOD(ifdi_init, em_if_init),
	DEVMETHOD(ifdi_stop, em_if_stop),
	DEVMETHOD(ifdi_msix_intr_assign, em_if_msix_intr_assign),
	DEVMETHOD(ifdi_intr_enable, igb_if_intr_enable),
	DEVMETHOD(ifdi_intr_disable, igb_if_intr_disable),
	DEVMETHOD(ifdi_tx_queues_alloc, em_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, em_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, em_if_queues_free),
	DEVMETHOD(ifdi_update_admin_status, em_if_update_admin_status),
	DEVMETHOD(ifdi_multi_set, em_if_multi_set),
	DEVMETHOD(ifdi_media_status, em_if_media_status),
	DEVMETHOD(ifdi_media_change, em_if_media_change),
	DEVMETHOD(ifdi_mtu_set, em_if_mtu_set),
	DEVMETHOD(ifdi_promisc_set, em_if_set_promisc),
	DEVMETHOD(ifdi_timer, em_if_timer),
	DEVMETHOD(ifdi_watchdog_reset, em_if_watchdog_reset),
	DEVMETHOD(ifdi_vlan_register, em_if_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, em_if_vlan_unregister),
	DEVMETHOD(ifdi_get_counter, em_if_get_counter),
	DEVMETHOD(ifdi_led_func, em_if_led_func),
	DEVMETHOD(ifdi_rx_queue_intr_enable, igb_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, igb_if_tx_queue_intr_enable),
	DEVMETHOD(ifdi_debug, em_if_debug),
	DEVMETHOD(ifdi_needs_restart, em_if_needs_restart),
	DEVMETHOD_END
};

static driver_t igb_if_driver = {
	"igb_if", igb_if_methods, sizeof(struct e1000_softc)
};

/*********************************************************************
 *  Tunable default values.
 *********************************************************************/

#define EM_TICKS_TO_USECS(ticks)	((1024 * (ticks) + 500) / 1000)
#define EM_USECS_TO_TICKS(usecs)	((1000 * (usecs) + 512) / 1024)

#define MAX_INTS_PER_SEC	8000
#define DEFAULT_ITR		(1000000000/(MAX_INTS_PER_SEC * 256))

/* Allow common code without TSO */
#ifndef CSUM_TSO
#define CSUM_TSO	0
#endif

static SYSCTL_NODE(_hw, OID_AUTO, em, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "EM driver parameters");

static int em_disable_crc_stripping = 0;
SYSCTL_INT(_hw_em, OID_AUTO, disable_crc_stripping, CTLFLAG_RDTUN,
    &em_disable_crc_stripping, 0, "Disable CRC Stripping");

static int em_tx_int_delay_dflt = EM_TICKS_TO_USECS(EM_TIDV);
static int em_rx_int_delay_dflt = EM_TICKS_TO_USECS(EM_RDTR);
SYSCTL_INT(_hw_em, OID_AUTO, tx_int_delay, CTLFLAG_RDTUN, &em_tx_int_delay_dflt,
    0, "Default transmit interrupt delay in usecs");
SYSCTL_INT(_hw_em, OID_AUTO, rx_int_delay, CTLFLAG_RDTUN, &em_rx_int_delay_dflt,
    0, "Default receive interrupt delay in usecs");

static int em_tx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_TADV);
static int em_rx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_RADV);
SYSCTL_INT(_hw_em, OID_AUTO, tx_abs_int_delay, CTLFLAG_RDTUN,
    &em_tx_abs_int_delay_dflt, 0,
    "Default transmit interrupt delay limit in usecs");
SYSCTL_INT(_hw_em, OID_AUTO, rx_abs_int_delay, CTLFLAG_RDTUN,
    &em_rx_abs_int_delay_dflt, 0,
    "Default receive interrupt delay limit in usecs");

static int em_smart_pwr_down = false;
SYSCTL_INT(_hw_em, OID_AUTO, smart_pwr_down, CTLFLAG_RDTUN, &em_smart_pwr_down,
    0, "Set to true to leave smart power down enabled on newer adapters");

static bool em_unsupported_tso = false;
SYSCTL_BOOL(_hw_em, OID_AUTO, unsupported_tso, CTLFLAG_RDTUN,
    &em_unsupported_tso, 0, "Allow unsupported em(4) TSO configurations");

/* Controls whether promiscuous also shows bad packets */
static int em_debug_sbp = false;
SYSCTL_INT(_hw_em, OID_AUTO, sbp, CTLFLAG_RDTUN, &em_debug_sbp, 0,
    "Show bad packets in promiscuous mode");

/* How many packets rxeof tries to clean at a time */
static int em_rx_process_limit = 100;
SYSCTL_INT(_hw_em, OID_AUTO, rx_process_limit, CTLFLAG_RDTUN,
    &em_rx_process_limit, 0,
    "Maximum number of received packets to process "
    "at a time, -1 means unlimited");

/* Energy efficient ethernet - default to OFF */
static int eee_setting = 1;
SYSCTL_INT(_hw_em, OID_AUTO, eee_setting, CTLFLAG_RDTUN, &eee_setting, 0,
    "Enable Energy Efficient Ethernet");

/*
** Tuneable Interrupt rate
*/
static int em_max_interrupt_rate = 8000;
SYSCTL_INT(_hw_em, OID_AUTO, max_interrupt_rate, CTLFLAG_RDTUN,
    &em_max_interrupt_rate, 0, "Maximum interrupts per second");



/* Global used in WOL setup with multiport cards */
static int global_quad_port_a = 0;

extern struct if_txrx igb_txrx;
extern struct if_txrx em_txrx;
extern struct if_txrx lem_txrx;

static struct if_shared_ctx em_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,
	.isc_tx_maxsize = EM_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tx_maxsegsize = PAGE_SIZE,
	.isc_tso_maxsize = EM_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = EM_TSO_SEG_SIZE,
	.isc_rx_maxsize = MJUM9BYTES,
	.isc_rx_nsegments = 1,
	.isc_rx_maxsegsize = MJUM9BYTES,
	.isc_nfl = 1,
	.isc_nrxqs = 1,
	.isc_ntxqs = 1,
	.isc_admin_intrcnt = 1,
	.isc_vendor_info = em_vendor_info_array,
	.isc_driver_version = em_driver_version,
	.isc_driver = &em_if_driver,
	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_TSO_INIT_IP | IFLIB_NEED_ZERO_CSUM,

	.isc_nrxd_min = {EM_MIN_RXD},
	.isc_ntxd_min = {EM_MIN_TXD},
	.isc_nrxd_max = {EM_MAX_RXD},
	.isc_ntxd_max = {EM_MAX_TXD},
	.isc_nrxd_default = {EM_DEFAULT_RXD},
	.isc_ntxd_default = {EM_DEFAULT_TXD},
};

static struct if_shared_ctx igb_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,
	.isc_tx_maxsize = EM_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tx_maxsegsize = PAGE_SIZE,
	.isc_tso_maxsize = EM_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = EM_TSO_SEG_SIZE,
	.isc_rx_maxsize = MJUM9BYTES,
	.isc_rx_nsegments = 1,
	.isc_rx_maxsegsize = MJUM9BYTES,
	.isc_nfl = 1,
	.isc_nrxqs = 1,
	.isc_ntxqs = 1,
	.isc_admin_intrcnt = 1,
	.isc_vendor_info = igb_vendor_info_array,
	.isc_driver_version = igb_driver_version,
	.isc_driver = &igb_if_driver,
	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_TSO_INIT_IP | IFLIB_NEED_ZERO_CSUM,

	.isc_nrxd_min = {EM_MIN_RXD},
	.isc_ntxd_min = {EM_MIN_TXD},
	.isc_nrxd_max = {IGB_MAX_RXD},
	.isc_ntxd_max = {IGB_MAX_TXD},
	.isc_nrxd_default = {EM_DEFAULT_RXD},
	.isc_ntxd_default = {EM_DEFAULT_TXD},
};

/*****************************************************************
 *
 * Dump Registers
 *
 ****************************************************************/
#define IGB_REGS_LEN 739

static int em_get_regs(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc = (struct e1000_softc *)arg1;
	struct e1000_hw *hw = &sc->hw;
	struct sbuf *sb;
	u32 *regs_buff;
	int rc;

	regs_buff = malloc(sizeof(u32) * IGB_REGS_LEN, M_DEVBUF, M_WAITOK);
	memset(regs_buff, 0, IGB_REGS_LEN * sizeof(u32));

	rc = sysctl_wire_old_buffer(req, 0);
	MPASS(rc == 0);
	if (rc != 0) {
		free(regs_buff, M_DEVBUF);
		return (rc);
	}

	sb = sbuf_new_for_sysctl(NULL, NULL, 32*400, req);
	MPASS(sb != NULL);
	if (sb == NULL) {
		free(regs_buff, M_DEVBUF);
		return (ENOMEM);
	}

	/* General Registers */
	regs_buff[0] = E1000_READ_REG(hw, E1000_CTRL);
	regs_buff[1] = E1000_READ_REG(hw, E1000_STATUS);
	regs_buff[2] = E1000_READ_REG(hw, E1000_CTRL_EXT);
	regs_buff[3] = E1000_READ_REG(hw, E1000_ICR);
	regs_buff[4] = E1000_READ_REG(hw, E1000_RCTL);
	regs_buff[5] = E1000_READ_REG(hw, E1000_RDLEN(0));
	regs_buff[6] = E1000_READ_REG(hw, E1000_RDH(0));
	regs_buff[7] = E1000_READ_REG(hw, E1000_RDT(0));
	regs_buff[8] = E1000_READ_REG(hw, E1000_RXDCTL(0));
	regs_buff[9] = E1000_READ_REG(hw, E1000_RDBAL(0));
	regs_buff[10] = E1000_READ_REG(hw, E1000_RDBAH(0));
	regs_buff[11] = E1000_READ_REG(hw, E1000_TCTL);
	regs_buff[12] = E1000_READ_REG(hw, E1000_TDBAL(0));
	regs_buff[13] = E1000_READ_REG(hw, E1000_TDBAH(0));
	regs_buff[14] = E1000_READ_REG(hw, E1000_TDLEN(0));
	regs_buff[15] = E1000_READ_REG(hw, E1000_TDH(0));
	regs_buff[16] = E1000_READ_REG(hw, E1000_TDT(0));
	regs_buff[17] = E1000_READ_REG(hw, E1000_TXDCTL(0));
	regs_buff[18] = E1000_READ_REG(hw, E1000_TDFH);
	regs_buff[19] = E1000_READ_REG(hw, E1000_TDFT);
	regs_buff[20] = E1000_READ_REG(hw, E1000_TDFHS);
	regs_buff[21] = E1000_READ_REG(hw, E1000_TDFPC);

	sbuf_printf(sb, "General Registers\n");
	sbuf_printf(sb, "\tCTRL\t %08x\n", regs_buff[0]);
	sbuf_printf(sb, "\tSTATUS\t %08x\n", regs_buff[1]);
	sbuf_printf(sb, "\tCTRL_EXT\t %08x\n\n", regs_buff[2]);

	sbuf_printf(sb, "Interrupt Registers\n");
	sbuf_printf(sb, "\tICR\t %08x\n\n", regs_buff[3]);

	sbuf_printf(sb, "RX Registers\n");
	sbuf_printf(sb, "\tRCTL\t %08x\n", regs_buff[4]);
	sbuf_printf(sb, "\tRDLEN\t %08x\n", regs_buff[5]);
	sbuf_printf(sb, "\tRDH\t %08x\n", regs_buff[6]);
	sbuf_printf(sb, "\tRDT\t %08x\n", regs_buff[7]);
	sbuf_printf(sb, "\tRXDCTL\t %08x\n", regs_buff[8]);
	sbuf_printf(sb, "\tRDBAL\t %08x\n", regs_buff[9]);
	sbuf_printf(sb, "\tRDBAH\t %08x\n\n", regs_buff[10]);

	sbuf_printf(sb, "TX Registers\n");
	sbuf_printf(sb, "\tTCTL\t %08x\n", regs_buff[11]);
	sbuf_printf(sb, "\tTDBAL\t %08x\n", regs_buff[12]);
	sbuf_printf(sb, "\tTDBAH\t %08x\n", regs_buff[13]);
	sbuf_printf(sb, "\tTDLEN\t %08x\n", regs_buff[14]);
	sbuf_printf(sb, "\tTDH\t %08x\n", regs_buff[15]);
	sbuf_printf(sb, "\tTDT\t %08x\n", regs_buff[16]);
	sbuf_printf(sb, "\tTXDCTL\t %08x\n", regs_buff[17]);
	sbuf_printf(sb, "\tTDFH\t %08x\n", regs_buff[18]);
	sbuf_printf(sb, "\tTDFT\t %08x\n", regs_buff[19]);
	sbuf_printf(sb, "\tTDFHS\t %08x\n", regs_buff[20]);
	sbuf_printf(sb, "\tTDFPC\t %08x\n\n", regs_buff[21]);

	free(regs_buff, M_DEVBUF);

#ifdef DUMP_DESCS
	{
		if_softc_ctx_t scctx = sc->shared;
		struct rx_ring *rxr = &rx_que->rxr;
		struct tx_ring *txr = &tx_que->txr;
		int ntxd = scctx->isc_ntxd[0];
		int nrxd = scctx->isc_nrxd[0];
		int j;

	for (j = 0; j < nrxd; j++) {
		u32 staterr = le32toh(rxr->rx_base[j].wb.upper.status_error);
		u32 length =  le32toh(rxr->rx_base[j].wb.upper.length);
		sbuf_printf(sb, "\tReceive Descriptor Address %d: %08" PRIx64 "  Error:%d  Length:%d\n", j, rxr->rx_base[j].read.buffer_addr, staterr, length);
	}

	for (j = 0; j < min(ntxd, 256); j++) {
		unsigned int *ptr = (unsigned int *)&txr->tx_base[j];

		sbuf_printf(sb, "\tTXD[%03d] [0]: %08x [1]: %08x [2]: %08x [3]: %08x  eop: %d DD=%d\n",
			    j, ptr[0], ptr[1], ptr[2], ptr[3], buf->eop,
			    buf->eop != -1 ? txr->tx_base[buf->eop].upper.fields.status & E1000_TXD_STAT_DD : 0);

	}
	}
#endif

	rc = sbuf_finish(sb);
	sbuf_delete(sb);
	return(rc);
}

static void *
em_register(device_t dev)
{
	return (&em_sctx_init);
}

static void *
igb_register(device_t dev)
{
	return (&igb_sctx_init);
}

static int
em_set_num_queues(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	int maxqueues;

	/* Sanity check based on HW */
	switch (sc->hw.mac.type) {
	case e1000_82576:
	case e1000_82580:
	case e1000_i350:
	case e1000_i354:
		maxqueues = 8;
		break;
	case e1000_i210:
	case e1000_82575:
		maxqueues = 4;
		break;
	case e1000_i211:
	case e1000_82574:
		maxqueues = 2;
		break;
	default:
		maxqueues = 1;
		break;
	}

	return (maxqueues);
}

#define LEM_CAPS \
    IFCAP_HWCSUM | IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING | \
    IFCAP_VLAN_HWCSUM | IFCAP_WOL | IFCAP_VLAN_HWFILTER | IFCAP_TSO4 | \
    IFCAP_LRO | IFCAP_VLAN_HWTSO | IFCAP_JUMBO_MTU | IFCAP_HWCSUM_IPV6

#define EM_CAPS \
    IFCAP_HWCSUM | IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING | \
    IFCAP_VLAN_HWCSUM | IFCAP_WOL | IFCAP_VLAN_HWFILTER | IFCAP_TSO4 | \
    IFCAP_LRO | IFCAP_VLAN_HWTSO | IFCAP_JUMBO_MTU | IFCAP_HWCSUM_IPV6 | \
    IFCAP_TSO6

#define IGB_CAPS \
    IFCAP_HWCSUM | IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING | \
    IFCAP_VLAN_HWCSUM | IFCAP_WOL | IFCAP_VLAN_HWFILTER | IFCAP_TSO4 | \
    IFCAP_LRO | IFCAP_VLAN_HWTSO | IFCAP_JUMBO_MTU | IFCAP_HWCSUM_IPV6 | \
    IFCAP_TSO6

/*********************************************************************
 *  Device initialization routine
 *
 *  The attach entry point is called when the driver is being loaded.
 *  This routine identifies the type of hardware, allocates all resources
 *  and initializes the hardware.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/
static int
em_if_attach_pre(if_ctx_t ctx)
{
	struct e1000_softc *sc;
	if_softc_ctx_t scctx;
	device_t dev;
	struct e1000_hw *hw;
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx_list;
	int error = 0;

	INIT_DEBUGOUT("em_if_attach_pre: begin");
	dev = iflib_get_dev(ctx);
	sc = iflib_get_softc(ctx);

	sc->ctx = sc->osdep.ctx = ctx;
	sc->dev = sc->osdep.dev = dev;
	scctx = sc->shared = iflib_get_softc_ctx(ctx);
	sc->media = iflib_get_media(ctx);
	hw = &sc->hw;

	sc->tx_process_limit = scctx->isc_ntxd[0];

	/* Determine hardware and mac info */
	em_identify_hardware(ctx);

	/* SYSCTL stuff */
	ctx_list = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "nvm",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    em_sysctl_nvm_info, "I", "NVM Information");

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "fw_version",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc, 0,
	    em_sysctl_print_fw_version, "A",
	    "Prints FW/NVM Versions");

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "debug",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    em_sysctl_debug_info, "I", "Debug Information");

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "fc",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    em_set_flowcntl, "I", "Flow Control");

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "reg_dump",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc, 0,
	    em_get_regs, "A", "Dump Registers");

	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "rs_dump",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    em_get_rs, "I", "Dump RS indexes");

	scctx->isc_tx_nsegments = EM_MAX_SCATTER;
	scctx->isc_nrxqsets_max = scctx->isc_ntxqsets_max = em_set_num_queues(ctx);
	if (bootverbose)
		device_printf(dev, "attach_pre capping queues at %d\n",
		    scctx->isc_ntxqsets_max);

	if (hw->mac.type >= igb_mac_min) {
		scctx->isc_txqsizes[0] = roundup2(scctx->isc_ntxd[0] * sizeof(union e1000_adv_tx_desc), EM_DBA_ALIGN);
		scctx->isc_rxqsizes[0] = roundup2(scctx->isc_nrxd[0] * sizeof(union e1000_adv_rx_desc), EM_DBA_ALIGN);
		scctx->isc_txd_size[0] = sizeof(union e1000_adv_tx_desc);
		scctx->isc_rxd_size[0] = sizeof(union e1000_adv_rx_desc);
		scctx->isc_txrx = &igb_txrx;
		scctx->isc_tx_tso_segments_max = EM_MAX_SCATTER;
		scctx->isc_tx_tso_size_max = EM_TSO_SIZE;
		scctx->isc_tx_tso_segsize_max = EM_TSO_SEG_SIZE;
		scctx->isc_capabilities = scctx->isc_capenable = IGB_CAPS;
		scctx->isc_tx_csum_flags = CSUM_TCP | CSUM_UDP | CSUM_TSO |
		     CSUM_IP6_TCP | CSUM_IP6_UDP;
		if (hw->mac.type != e1000_82575)
			scctx->isc_tx_csum_flags |= CSUM_SCTP | CSUM_IP6_SCTP;
		/*
		** Some new devices, as with ixgbe, now may
		** use a different BAR, so we need to keep
		** track of which is used.
		*/
		scctx->isc_msix_bar = pci_msix_table_bar(dev);
	} else if (hw->mac.type >= em_mac_min) {
		scctx->isc_txqsizes[0] = roundup2(scctx->isc_ntxd[0]* sizeof(struct e1000_tx_desc), EM_DBA_ALIGN);
		scctx->isc_rxqsizes[0] = roundup2(scctx->isc_nrxd[0] * sizeof(union e1000_rx_desc_extended), EM_DBA_ALIGN);
		scctx->isc_txd_size[0] = sizeof(struct e1000_tx_desc);
		scctx->isc_rxd_size[0] = sizeof(union e1000_rx_desc_extended);
		scctx->isc_txrx = &em_txrx;
		scctx->isc_tx_tso_segments_max = EM_MAX_SCATTER;
		scctx->isc_tx_tso_size_max = EM_TSO_SIZE;
		scctx->isc_tx_tso_segsize_max = EM_TSO_SEG_SIZE;
		scctx->isc_capabilities = scctx->isc_capenable = EM_CAPS;
		scctx->isc_tx_csum_flags = CSUM_TCP | CSUM_UDP | CSUM_IP_TSO |
		    CSUM_IP6_TCP | CSUM_IP6_UDP;

		/* Disable TSO on 82574L due to performance loss being investigated */
		if (hw->mac.type == e1000_82574)
			scctx->isc_capenable &= ~IFCAP_TSO;
		/*
		 * Disable TSO on SPT due to errata that downclocks DMA performance
		 * i218-i219 Specification Update 1.5.4.5
		 */
		if (hw->mac.type == e1000_pch_spt)
			scctx->isc_capenable &= ~IFCAP_TSO;

		/*
		 * We support MSI-X with 82574 only, but indicate to iflib(4)
		 * that it shall give MSI at least a try with other devices.
		 */
		if (hw->mac.type == e1000_82574) {
			scctx->isc_msix_bar = pci_msix_table_bar(dev);
		} else {
			scctx->isc_msix_bar = -1;
			scctx->isc_disable_msix = 1;
		}
	} else {
		scctx->isc_txqsizes[0] = roundup2((scctx->isc_ntxd[0] + 1) * sizeof(struct e1000_tx_desc), EM_DBA_ALIGN);
		scctx->isc_rxqsizes[0] = roundup2((scctx->isc_nrxd[0] + 1) * sizeof(struct e1000_rx_desc), EM_DBA_ALIGN);
		scctx->isc_txd_size[0] = sizeof(struct e1000_tx_desc);
		scctx->isc_rxd_size[0] = sizeof(struct e1000_rx_desc);
		scctx->isc_txrx = &lem_txrx;
		scctx->isc_tx_tso_segments_max = EM_MAX_SCATTER;
		scctx->isc_tx_tso_size_max = EM_TSO_SIZE;
		scctx->isc_tx_tso_segsize_max = EM_TSO_SEG_SIZE;
		scctx->isc_capabilities = scctx->isc_capenable = LEM_CAPS;
		if (em_unsupported_tso)
			scctx->isc_capabilities |= IFCAP_TSO6;
		scctx->isc_tx_csum_flags = CSUM_TCP | CSUM_UDP | CSUM_IP_TSO |
		    CSUM_IP6_TCP | CSUM_IP6_UDP;

		/* 82541ER doesn't do HW tagging */
		if (hw->device_id == E1000_DEV_ID_82541ER ||
		    hw->device_id == E1000_DEV_ID_82541ER_LOM) {
			scctx->isc_capabilities &= ~IFCAP_VLAN_HWTAGGING;
			scctx->isc_capenable = scctx->isc_capabilities;
		}
		/* This is the first e1000 chip and it does not do offloads */
		if (hw->mac.type == e1000_82542) {
			scctx->isc_capabilities &= ~(IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM |
			    IFCAP_HWCSUM_IPV6 | IFCAP_VLAN_HWTAGGING |
			    IFCAP_VLAN_HWFILTER | IFCAP_TSO | IFCAP_VLAN_HWTSO);
			scctx->isc_capenable = scctx->isc_capabilities;
		}
		/* These can't do TSO for various reasons */
		if (hw->mac.type < e1000_82544 || hw->mac.type == e1000_82547 ||
		    hw->mac.type == e1000_82547_rev_2) {
			scctx->isc_capabilities &= ~(IFCAP_TSO | IFCAP_VLAN_HWTSO);
			scctx->isc_capenable = scctx->isc_capabilities;
		}
		/* XXXKB: No IPv6 before this? */
		if (hw->mac.type < e1000_82545){
			scctx->isc_capabilities &= ~IFCAP_HWCSUM_IPV6;
			scctx->isc_capenable = scctx->isc_capabilities;
		}
		/* "PCI/PCI-X SDM 4.0" page 33 (b) - FDX requirement on these chips */
		if (hw->mac.type == e1000_82547 || hw->mac.type == e1000_82547_rev_2)
			scctx->isc_capenable &= ~(IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM |
			    IFCAP_HWCSUM_IPV6);

		/* INTx only */
		scctx->isc_msix_bar = 0;
	}

	/* Setup PCI resources */
	if (em_allocate_pci_resources(ctx)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_pci;
	}

	/*
	** For ICH8 and family we need to
	** map the flash memory, and this
	** must happen after the MAC is
	** identified
	*/
	if ((hw->mac.type == e1000_ich8lan) ||
	    (hw->mac.type == e1000_ich9lan) ||
	    (hw->mac.type == e1000_ich10lan) ||
	    (hw->mac.type == e1000_pchlan) ||
	    (hw->mac.type == e1000_pch2lan) ||
	    (hw->mac.type == e1000_pch_lpt)) {
		int rid = EM_BAR_TYPE_FLASH;
		sc->flash = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
		if (sc->flash == NULL) {
			device_printf(dev, "Mapping of Flash failed\n");
			error = ENXIO;
			goto err_pci;
		}
		/* This is used in the shared code */
		hw->flash_address = (u8 *)sc->flash;
		sc->osdep.flash_bus_space_tag =
		    rman_get_bustag(sc->flash);
		sc->osdep.flash_bus_space_handle =
		    rman_get_bushandle(sc->flash);
	}
	/*
	** In the new SPT device flash is not  a
	** separate BAR, rather it is also in BAR0,
	** so use the same tag and an offset handle for the
	** FLASH read/write macros in the shared code.
	*/
	else if (hw->mac.type >= e1000_pch_spt) {
		sc->osdep.flash_bus_space_tag =
		    sc->osdep.mem_bus_space_tag;
		sc->osdep.flash_bus_space_handle =
		    sc->osdep.mem_bus_space_handle
		    + E1000_FLASH_BASE_ADDR;
	}

	/* Do Shared Code initialization */
	error = e1000_setup_init_funcs(hw, true);
	if (error) {
		device_printf(dev, "Setup of Shared code failed, error %d\n",
		    error);
		error = ENXIO;
		goto err_pci;
	}

	em_setup_msix(ctx);
	e1000_get_bus_info(hw);

	/* Set up some sysctls for the tunable interrupt delays */
	em_add_int_delay_sysctl(sc, "rx_int_delay",
	    "receive interrupt delay in usecs", &sc->rx_int_delay,
	    E1000_REGISTER(hw, E1000_RDTR), em_rx_int_delay_dflt);
	em_add_int_delay_sysctl(sc, "tx_int_delay",
	    "transmit interrupt delay in usecs", &sc->tx_int_delay,
	    E1000_REGISTER(hw, E1000_TIDV), em_tx_int_delay_dflt);
	em_add_int_delay_sysctl(sc, "rx_abs_int_delay",
	    "receive interrupt delay limit in usecs",
	    &sc->rx_abs_int_delay,
	    E1000_REGISTER(hw, E1000_RADV),
	    em_rx_abs_int_delay_dflt);
	em_add_int_delay_sysctl(sc, "tx_abs_int_delay",
	    "transmit interrupt delay limit in usecs",
	    &sc->tx_abs_int_delay,
	    E1000_REGISTER(hw, E1000_TADV),
	    em_tx_abs_int_delay_dflt);
	em_add_int_delay_sysctl(sc, "itr",
	    "interrupt delay limit in usecs/4",
	    &sc->tx_itr,
	    E1000_REGISTER(hw, E1000_ITR),
	    DEFAULT_ITR);

	hw->mac.autoneg = DO_AUTO_NEG;
	hw->phy.autoneg_wait_to_complete = false;
	hw->phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;

	if (hw->mac.type < em_mac_min) {
		e1000_init_script_state_82541(hw, true);
		e1000_set_tbi_compatibility_82543(hw, true);
	}
	/* Copper options */
	if (hw->phy.media_type == e1000_media_type_copper) {
		hw->phy.mdix = AUTO_ALL_MODES;
		hw->phy.disable_polarity_correction = false;
		hw->phy.ms_type = EM_MASTER_SLAVE;
	}

	/*
	 * Set the frame limits assuming
	 * standard ethernet sized frames.
	 */
	scctx->isc_max_frame_size = hw->mac.max_frame_size =
	    ETHERMTU + ETHER_HDR_LEN + ETHERNET_FCS_SIZE;

	/*
	 * This controls when hardware reports transmit completion
	 * status.
	 */
	hw->mac.report_tx_early = 1;

	/* Allocate multicast array memory. */
	sc->mta = malloc(sizeof(u8) * ETHER_ADDR_LEN *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (sc->mta == NULL) {
		device_printf(dev, "Can not allocate multicast setup array\n");
		error = ENOMEM;
		goto err_late;
	}

	/* Clear the IFCAP_TSO auto mask */
	sc->tso_automasked = 0;

	/* Check SOL/IDER usage */
	if (e1000_check_reset_block(hw))
		device_printf(dev, "PHY reset is blocked"
			      " due to SOL/IDER session.\n");

	/* Sysctl for setting Energy Efficient Ethernet */
	hw->dev_spec.ich8lan.eee_disable = eee_setting;
	SYSCTL_ADD_PROC(ctx_list, child, OID_AUTO, "eee_control",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0,
	    em_sysctl_eee, "I", "Disable Energy Efficient Ethernet");

	/*
	** Start from a known state, this is
	** important in reading the nvm and
	** mac from that.
	*/
	e1000_reset_hw(hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (e1000_validate_nvm_checksum(hw) < 0) {
		/*
		** Some PCI-E parts fail the first check due to
		** the link being in sleep state, call it again,
		** if it fails a second time its a real issue.
		*/
		if (e1000_validate_nvm_checksum(hw) < 0) {
			device_printf(dev,
			    "The EEPROM Checksum Is Not Valid\n");
			error = EIO;
			goto err_late;
		}
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (e1000_read_mac_addr(hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
			      " address\n");
		error = EIO;
		goto err_late;
	}

	if (!em_is_valid_ether_addr(hw->mac.addr)) {
		if (sc->vf_ifp) {
			ether_gen_addr(iflib_get_ifp(ctx),
			    (struct ether_addr *)hw->mac.addr);
		} else {
			device_printf(dev, "Invalid MAC address\n");
			error = EIO;
			goto err_late;
		}
	}

	/* Save the EEPROM/NVM versions, must be done under IFLIB_CTX_LOCK */
	em_fw_version_locked(ctx);

	em_print_fw_version(sc);

	/*
	 * Get Wake-on-Lan and Management info for later use
	 */
	em_get_wakeup(ctx);

	/* Enable only WOL MAGIC by default */
	scctx->isc_capenable &= ~IFCAP_WOL;
	if (sc->wol != 0)
		scctx->isc_capenable |= IFCAP_WOL_MAGIC;

	iflib_set_mac(ctx, hw->mac.addr);

	return (0);

err_late:
	em_release_hw_control(sc);
err_pci:
	em_free_pci_resources(ctx);
	free(sc->mta, M_DEVBUF);

	return (error);
}

static int
em_if_attach_post(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;
	int error = 0;

	/* Setup OS specific network interface */
	error = em_setup_interface(ctx);
	if (error != 0) {
		device_printf(sc->dev, "Interface setup failed: %d\n", error);
		goto err_late;
	}

	em_reset(ctx);

	/* Initialize statistics */
	em_update_stats_counters(sc);
	hw->mac.get_link_status = 1;
	em_if_update_admin_status(ctx);
	em_add_hw_stats(sc);

	/* Non-AMT based hardware can now take control from firmware */
	if (sc->has_manage && !sc->has_amt)
		em_get_hw_control(sc);

	INIT_DEBUGOUT("em_if_attach_post: end");

	return (0);

err_late:
	/* upon attach_post() error, iflib calls _if_detach() to free resources. */
	return (error);
}

/*********************************************************************
 *  Device removal routine
 *
 *  The detach entry point is called when the driver is being removed.
 *  This routine stops the adapter and deallocates all the resources
 *  that were allocated for driver operation.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/
static int
em_if_detach(if_ctx_t ctx)
{
	struct e1000_softc	*sc = iflib_get_softc(ctx);

	INIT_DEBUGOUT("em_if_detach: begin");

	e1000_phy_hw_reset(&sc->hw);

	em_release_manageability(sc);
	em_release_hw_control(sc);
	em_free_pci_resources(ctx);
	free(sc->mta, M_DEVBUF);
	sc->mta = NULL;

	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
em_if_shutdown(if_ctx_t ctx)
{
	return em_if_suspend(ctx);
}

/*
 * Suspend/resume device methods.
 */
static int
em_if_suspend(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);

	em_release_manageability(sc);
	em_release_hw_control(sc);
	em_enable_wakeup(ctx);
	return (0);
}

static int
em_if_resume(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);

	if (sc->hw.mac.type == e1000_pch2lan)
		e1000_resume_workarounds_pchlan(&sc->hw);
	em_if_init(ctx);
	em_init_manageability(sc);

	return(0);
}

static int
em_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	int max_frame_size;
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = iflib_get_softc_ctx(ctx);

	IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

	switch (sc->hw.mac.type) {
	case e1000_82571:
	case e1000_82572:
	case e1000_ich9lan:
	case e1000_ich10lan:
	case e1000_pch2lan:
	case e1000_pch_lpt:
	case e1000_pch_spt:
	case e1000_pch_cnp:
	case e1000_pch_tgp:
	case e1000_pch_adp:
	case e1000_pch_mtp:
	case e1000_pch_ptp:
	case e1000_82574:
	case e1000_82583:
	case e1000_80003es2lan:
		/* 9K Jumbo Frame size */
		max_frame_size = 9234;
		break;
	case e1000_pchlan:
		max_frame_size = 4096;
		break;
	case e1000_82542:
	case e1000_ich8lan:
		/* Adapters that do not support jumbo frames */
		max_frame_size = ETHER_MAX_LEN;
		break;
	default:
		if (sc->hw.mac.type >= igb_mac_min)
			max_frame_size = 9234;
		else /* lem */
			max_frame_size = MAX_JUMBO_FRAME_SIZE;
	}
	if (mtu > max_frame_size - ETHER_HDR_LEN - ETHER_CRC_LEN) {
		return (EINVAL);
	}

	scctx->isc_max_frame_size = sc->hw.mac.max_frame_size =
	    mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
	return (0);
}

/*********************************************************************
 *  Init entry point
 *
 *  This routine is used in two ways. It is used by the stack as
 *  init entry point in network interface structure. It is also used
 *  by the driver as a hw/sw initialization routine to get to a
 *  consistent state.
 *
 **********************************************************************/
static void
em_if_init(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	if_t ifp = iflib_get_ifp(ctx);
	struct em_tx_queue *tx_que;
	int i;

	INIT_DEBUGOUT("em_if_init: begin");

	/* Get the latest mac address, User can use a LAA */
	bcopy(if_getlladdr(ifp), sc->hw.mac.addr,
	    ETHER_ADDR_LEN);

	/* Put the address into the Receive Address Array */
	e1000_rar_set(&sc->hw, sc->hw.mac.addr, 0);

	/*
	 * With the 82571 adapter, RAR[0] may be overwritten
	 * when the other port is reset, we make a duplicate
	 * in RAR[14] for that eventuality, this assures
	 * the interface continues to function.
	 */
	if (sc->hw.mac.type == e1000_82571) {
		e1000_set_laa_state_82571(&sc->hw, true);
		e1000_rar_set(&sc->hw, sc->hw.mac.addr,
		    E1000_RAR_ENTRIES - 1);
	}

	/* Initialize the hardware */
	em_reset(ctx);
	em_if_update_admin_status(ctx);

	for (i = 0, tx_que = sc->tx_queues; i < sc->tx_num_queues; i++, tx_que++) {
		struct tx_ring *txr = &tx_que->txr;

		txr->tx_rs_cidx = txr->tx_rs_pidx;

		/* Initialize the last processed descriptor to be the end of
		 * the ring, rather than the start, so that we avoid an
		 * off-by-one error when calculating how many descriptors are
		 * done in the credits_update function.
		 */
		txr->tx_cidx_processed = scctx->isc_ntxd[0] - 1;
	}

	/* Setup VLAN support, basic and offload if available */
	E1000_WRITE_REG(&sc->hw, E1000_VET, ETHERTYPE_VLAN);

	/* Clear bad data from Rx FIFOs */
	if (sc->hw.mac.type >= igb_mac_min)
		e1000_rx_fifo_flush_base(&sc->hw);

	/* Configure for OS presence */
	em_init_manageability(sc);

	/* Prepare transmit descriptors and buffers */
	em_initialize_transmit_unit(ctx);

	/* Setup Multicast table */
	em_if_multi_set(ctx);

	sc->rx_mbuf_sz = iflib_get_rx_mbuf_sz(ctx);
	em_initialize_receive_unit(ctx);

	/* Set up VLAN support and filter */
	em_setup_vlan_hw_support(ctx);

	/* Don't lose promiscuous settings */
	em_if_set_promisc(ctx, if_getflags(ifp));
	e1000_clear_hw_cntrs_base_generic(&sc->hw);

	/* MSI-X configuration for 82574 */
	if (sc->hw.mac.type == e1000_82574) {
		int tmp = E1000_READ_REG(&sc->hw, E1000_CTRL_EXT);

		tmp |= E1000_CTRL_EXT_PBA_CLR;
		E1000_WRITE_REG(&sc->hw, E1000_CTRL_EXT, tmp);
		/* Set the IVAR - interrupt vector routing. */
		E1000_WRITE_REG(&sc->hw, E1000_IVAR, sc->ivars);
	} else if (sc->intr_type == IFLIB_INTR_MSIX) /* Set up queue routing */
		igb_configure_queues(sc);

	/* this clears any pending interrupts */
	E1000_READ_REG(&sc->hw, E1000_ICR);
	E1000_WRITE_REG(&sc->hw, E1000_ICS, E1000_ICS_LSC);

	/* AMT based hardware can now take control from firmware */
	if (sc->has_manage && sc->has_amt)
		em_get_hw_control(sc);

	/* Set Energy Efficient Ethernet */
	if (sc->hw.mac.type >= igb_mac_min &&
	    sc->hw.phy.media_type == e1000_media_type_copper) {
		if (sc->hw.mac.type == e1000_i354)
			e1000_set_eee_i354(&sc->hw, true, true);
		else
			e1000_set_eee_i350(&sc->hw, true, true);
	}
}

/*********************************************************************
 *
 *  Fast Legacy/MSI Combined Interrupt Service routine
 *
 *********************************************************************/
int
em_intr(void *arg)
{
	struct e1000_softc *sc = arg;
	if_ctx_t ctx = sc->ctx;
	u32 reg_icr;

	reg_icr = E1000_READ_REG(&sc->hw, E1000_ICR);

	/* Hot eject? */
	if (reg_icr == 0xffffffff)
		return FILTER_STRAY;

	/* Definitely not our interrupt. */
	if (reg_icr == 0x0)
		return FILTER_STRAY;

	/*
	 * Starting with the 82571 chip, bit 31 should be used to
	 * determine whether the interrupt belongs to us.
	 */
	if (sc->hw.mac.type >= e1000_82571 &&
	    (reg_icr & E1000_ICR_INT_ASSERTED) == 0)
		return FILTER_STRAY;

	/*
	 * Only MSI-X interrupts have one-shot behavior by taking advantage
	 * of the EIAC register.  Thus, explicitly disable interrupts.  This
	 * also works around the MSI message reordering errata on certain
	 * systems.
	 */
	IFDI_INTR_DISABLE(ctx);

	/* Link status change */
	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC))
		em_handle_link(ctx);

	if (reg_icr & E1000_ICR_RXO)
		sc->rx_overruns++;

	return (FILTER_SCHEDULE_THREAD);
}

static int
em_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct em_rx_queue *rxq = &sc->rx_queues[rxqid];

	E1000_WRITE_REG(&sc->hw, E1000_IMS, rxq->eims);
	return (0);
}

static int
em_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct em_tx_queue *txq = &sc->tx_queues[txqid];

	E1000_WRITE_REG(&sc->hw, E1000_IMS, txq->eims);
	return (0);
}

static int
igb_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct em_rx_queue *rxq = &sc->rx_queues[rxqid];

	E1000_WRITE_REG(&sc->hw, E1000_EIMS, rxq->eims);
	return (0);
}

static int
igb_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct em_tx_queue *txq = &sc->tx_queues[txqid];

	E1000_WRITE_REG(&sc->hw, E1000_EIMS, txq->eims);
	return (0);
}

/*********************************************************************
 *
 *  MSI-X RX Interrupt Service routine
 *
 **********************************************************************/
static int
em_msix_que(void *arg)
{
	struct em_rx_queue *que = arg;

	++que->irqs;

	return (FILTER_SCHEDULE_THREAD);
}

/*********************************************************************
 *
 *  MSI-X Link Fast Interrupt Service routine
 *
 **********************************************************************/
static int
em_msix_link(void *arg)
{
	struct e1000_softc *sc = arg;
	u32 reg_icr;

	++sc->link_irq;
	MPASS(sc->hw.back != NULL);
	reg_icr = E1000_READ_REG(&sc->hw, E1000_ICR);

	if (reg_icr & E1000_ICR_RXO)
		sc->rx_overruns++;

	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC))
		em_handle_link(sc->ctx);

	/* Re-arm unconditionally */
	if (sc->hw.mac.type >= igb_mac_min) {
		E1000_WRITE_REG(&sc->hw, E1000_IMS, E1000_IMS_LSC);
		E1000_WRITE_REG(&sc->hw, E1000_EIMS, sc->link_mask);
	} else if (sc->hw.mac.type == e1000_82574) {
		E1000_WRITE_REG(&sc->hw, E1000_IMS, E1000_IMS_LSC |
		    E1000_IMS_OTHER);
		/*
		 * Because we must read the ICR for this interrupt it may
		 * clear other causes using autoclear, for this reason we
		 * simply create a soft interrupt for all these vectors.
		 */
		if (reg_icr)
			E1000_WRITE_REG(&sc->hw, E1000_ICS, sc->ims);
	} else
		E1000_WRITE_REG(&sc->hw, E1000_IMS, E1000_IMS_LSC);

	return (FILTER_HANDLED);
}

static void
em_handle_link(void *context)
{
	if_ctx_t ctx = context;
	struct e1000_softc *sc = iflib_get_softc(ctx);

	sc->hw.mac.get_link_status = 1;
	iflib_admin_intr_deferred(ctx);
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
static void
em_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	u_char fiber_type = IFM_1000_SX;

	INIT_DEBUGOUT("em_if_media_status: begin");

	iflib_admin_intr_deferred(ctx);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_active) {
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	if ((sc->hw.phy.media_type == e1000_media_type_fiber) ||
	    (sc->hw.phy.media_type == e1000_media_type_internal_serdes)) {
		if (sc->hw.mac.type == e1000_82545)
			fiber_type = IFM_1000_LX;
		ifmr->ifm_active |= fiber_type | IFM_FDX;
	} else {
		switch (sc->link_speed) {
		case 10:
			ifmr->ifm_active |= IFM_10_T;
			break;
		case 100:
			ifmr->ifm_active |= IFM_100_TX;
			break;
		case 1000:
			ifmr->ifm_active |= IFM_1000_T;
			break;
		}
		if (sc->link_duplex == FULL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
	}
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called when the user changes speed/duplex using
 *  media/mediopt option with ifconfig.
 *
 **********************************************************************/
static int
em_if_media_change(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct ifmedia *ifm = iflib_get_media(ctx);

	INIT_DEBUGOUT("em_if_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		sc->hw.mac.autoneg = DO_AUTO_NEG;
		sc->hw.phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
	case IFM_1000_LX:
	case IFM_1000_SX:
	case IFM_1000_T:
		sc->hw.mac.autoneg = DO_AUTO_NEG;
		sc->hw.phy.autoneg_advertised = ADVERTISE_1000_FULL;
		break;
	case IFM_100_TX:
		sc->hw.mac.autoneg = false;
		sc->hw.phy.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.mac.forced_speed_duplex = ADVERTISE_100_FULL;
		else
			sc->hw.mac.forced_speed_duplex = ADVERTISE_100_HALF;
		break;
	case IFM_10_T:
		sc->hw.mac.autoneg = false;
		sc->hw.phy.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.mac.forced_speed_duplex = ADVERTISE_10_FULL;
		else
			sc->hw.mac.forced_speed_duplex = ADVERTISE_10_HALF;
		break;
	default:
		device_printf(sc->dev, "Unsupported media type\n");
	}

	em_if_init(ctx);

	return (0);
}

static int
em_if_set_promisc(if_ctx_t ctx, int flags)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	u32 reg_rctl;
	int mcnt = 0;

	reg_rctl = E1000_READ_REG(&sc->hw, E1000_RCTL);
	reg_rctl &= ~(E1000_RCTL_SBP | E1000_RCTL_UPE);
	if (flags & IFF_ALLMULTI)
		mcnt = MAX_NUM_MULTICAST_ADDRESSES;
	else
		mcnt = min(if_llmaddr_count(ifp), MAX_NUM_MULTICAST_ADDRESSES);

	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES)
		reg_rctl &= (~E1000_RCTL_MPE);
	E1000_WRITE_REG(&sc->hw, E1000_RCTL, reg_rctl);

	if (flags & IFF_PROMISC) {
		reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		em_if_vlan_filter_disable(sc);
		/* Turn this on if you want to see bad packets */
		if (em_debug_sbp)
			reg_rctl |= E1000_RCTL_SBP;
		E1000_WRITE_REG(&sc->hw, E1000_RCTL, reg_rctl);
	} else {
		if (flags & IFF_ALLMULTI) {
			reg_rctl |= E1000_RCTL_MPE;
			reg_rctl &= ~E1000_RCTL_UPE;
			E1000_WRITE_REG(&sc->hw, E1000_RCTL, reg_rctl);
		}
		if (em_if_vlan_filter_used(ctx))
			em_if_vlan_filter_enable(sc);
	}
	return (0);
}

static u_int
em_copy_maddr(void *arg, struct sockaddr_dl *sdl, u_int idx)
{
	u8 *mta = arg;

	if (idx == MAX_NUM_MULTICAST_ADDRESSES)
		return (0);

	bcopy(LLADDR(sdl), &mta[idx * ETHER_ADDR_LEN], ETHER_ADDR_LEN);

	return (1);
}

/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/
static void
em_if_multi_set(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	u8  *mta; /* Multicast array memory */
	u32 reg_rctl = 0;
	int mcnt = 0;

	IOCTL_DEBUGOUT("em_set_multi: begin");

	mta = sc->mta;
	bzero(mta, sizeof(u8) * ETHER_ADDR_LEN * MAX_NUM_MULTICAST_ADDRESSES);

	if (sc->hw.mac.type == e1000_82542 &&
	    sc->hw.revision_id == E1000_REVISION_2) {
		reg_rctl = E1000_READ_REG(&sc->hw, E1000_RCTL);
		if (sc->hw.bus.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			e1000_pci_clear_mwi(&sc->hw);
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, E1000_RCTL, reg_rctl);
		msec_delay(5);
	}

	mcnt = if_foreach_llmaddr(ifp, em_copy_maddr, mta);

	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES)
		e1000_update_mc_addr_list(&sc->hw, mta, mcnt);

	reg_rctl = E1000_READ_REG(&sc->hw, E1000_RCTL);

	if (if_getflags(ifp) & IFF_PROMISC)
		reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
	else if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES ||
	    if_getflags(ifp) & IFF_ALLMULTI) {
		reg_rctl |= E1000_RCTL_MPE;
		reg_rctl &= ~E1000_RCTL_UPE;
	} else
		reg_rctl &= ~(E1000_RCTL_UPE | E1000_RCTL_MPE);

	E1000_WRITE_REG(&sc->hw, E1000_RCTL, reg_rctl);

	if (sc->hw.mac.type == e1000_82542 &&
	    sc->hw.revision_id == E1000_REVISION_2) {
		reg_rctl = E1000_READ_REG(&sc->hw, E1000_RCTL);
		reg_rctl &= ~E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, E1000_RCTL, reg_rctl);
		msec_delay(5);
		if (sc->hw.bus.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			e1000_pci_set_mwi(&sc->hw);
	}
}

/*********************************************************************
 *  Timer routine
 *
 *  This routine schedules em_if_update_admin_status() to check for
 *  link status and to gather statistics as well as to perform some
 *  controller-specific hardware patting.
 *
 **********************************************************************/
static void
em_if_timer(if_ctx_t ctx, uint16_t qid)
{

	if (qid != 0)
		return;

	iflib_admin_intr_deferred(ctx);
}

static void
em_if_update_admin_status(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;
	device_t dev = iflib_get_dev(ctx);
	u32 link_check, thstat, ctrl;
	bool automasked = false;

	link_check = thstat = ctrl = 0;
	/* Get the cached link value or read phy for real */
	switch (hw->phy.media_type) {
	case e1000_media_type_copper:
		if (hw->mac.get_link_status) {
			if (hw->mac.type == e1000_pch_spt)
				msec_delay(50);
			/* Do the work to read phy */
			e1000_check_for_link(hw);
			link_check = !hw->mac.get_link_status;
			if (link_check) /* ESB2 fix */
				e1000_cfg_on_link_up(hw);
		} else {
			link_check = true;
		}
		break;
	case e1000_media_type_fiber:
		e1000_check_for_link(hw);
		link_check = (E1000_READ_REG(hw, E1000_STATUS) &
			    E1000_STATUS_LU);
		break;
	case e1000_media_type_internal_serdes:
		e1000_check_for_link(hw);
		link_check = hw->mac.serdes_has_link;
		break;
	/* VF device is type_unknown */
	case e1000_media_type_unknown:
		e1000_check_for_link(hw);
		link_check = !hw->mac.get_link_status;
		/* FALLTHROUGH */
	default:
		break;
	}

	/* Check for thermal downshift or shutdown */
	if (hw->mac.type == e1000_i350) {
		thstat = E1000_READ_REG(hw, E1000_THSTAT);
		ctrl = E1000_READ_REG(hw, E1000_CTRL_EXT);
	}

	/* Now check for a transition */
	if (link_check && (sc->link_active == 0)) {
		e1000_get_speed_and_duplex(hw, &sc->link_speed,
		    &sc->link_duplex);
		/* Check if we must disable SPEED_MODE bit on PCI-E */
		if ((sc->link_speed != SPEED_1000) &&
		    ((hw->mac.type == e1000_82571) ||
		    (hw->mac.type == e1000_82572))) {
			int tarc0;
			tarc0 = E1000_READ_REG(hw, E1000_TARC(0));
			tarc0 &= ~TARC_SPEED_MODE_BIT;
			E1000_WRITE_REG(hw, E1000_TARC(0), tarc0);
		}
		if (bootverbose)
			device_printf(dev, "Link is up %d Mbps %s\n",
			    sc->link_speed,
			    ((sc->link_duplex == FULL_DUPLEX) ?
			    "Full Duplex" : "Half Duplex"));
		sc->link_active = 1;
		sc->smartspeed = 0;
		if ((ctrl & E1000_CTRL_EXT_LINK_MODE_MASK) ==
		    E1000_CTRL_EXT_LINK_MODE_GMII &&
		    (thstat & E1000_THSTAT_LINK_THROTTLE))
			device_printf(dev, "Link: thermal downshift\n");
		/* Delay Link Up for Phy update */
		if (((hw->mac.type == e1000_i210) ||
		    (hw->mac.type == e1000_i211)) &&
		    (hw->phy.id == I210_I_PHY_ID))
			msec_delay(I210_LINK_DELAY);
		/* Reset if the media type changed. */
		if (hw->dev_spec._82575.media_changed &&
		    hw->mac.type >= igb_mac_min) {
			hw->dev_spec._82575.media_changed = false;
			sc->flags |= IGB_MEDIA_RESET;
			em_reset(ctx);
		}
		/* Only do TSO on gigabit Ethernet for older chips due to errata */
		if (hw->mac.type < igb_mac_min)
			automasked = em_automask_tso(ctx);

		/* Automasking resets the interface, so don't mark it up yet */
		if (!automasked)
			iflib_link_state_change(ctx, LINK_STATE_UP,
			    IF_Mbps(sc->link_speed));
	} else if (!link_check && (sc->link_active == 1)) {
		sc->link_speed = 0;
		sc->link_duplex = 0;
		sc->link_active = 0;
		iflib_link_state_change(ctx, LINK_STATE_DOWN, 0);
	}
	em_update_stats_counters(sc);

	/* Reset LAA into RAR[0] on 82571 */
	if (hw->mac.type == e1000_82571 && e1000_get_laa_state_82571(hw))
		e1000_rar_set(hw, hw->mac.addr, 0);

	if (hw->mac.type < em_mac_min)
		lem_smartspeed(sc);
}

static void
em_if_watchdog_reset(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);

	/*
	 * Just count the event; iflib(4) will already trigger a
	 * sufficient reset of the controller.
	 */
	sc->watchdog_events++;
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 *
 **********************************************************************/
static void
em_if_stop(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);

	INIT_DEBUGOUT("em_if_stop: begin");

	/* I219 needs special flushing to avoid hangs */
	if (sc->hw.mac.type >= e1000_pch_spt && sc->hw.mac.type < igb_mac_min)
		em_flush_desc_rings(sc);

	e1000_reset_hw(&sc->hw);
	if (sc->hw.mac.type >= e1000_82544)
		E1000_WRITE_REG(&sc->hw, E1000_WUFC, 0);

	e1000_led_off(&sc->hw);
	e1000_cleanup_led(&sc->hw);
}

/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
em_identify_hardware(if_ctx_t ctx)
{
	device_t dev = iflib_get_dev(ctx);
	struct e1000_softc *sc = iflib_get_softc(ctx);

	/* Make sure our PCI config space has the necessary stuff set */
	sc->hw.bus.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);

	/* Save off the information about this board */
	sc->hw.vendor_id = pci_get_vendor(dev);
	sc->hw.device_id = pci_get_device(dev);
	sc->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	sc->hw.subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	sc->hw.subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* Do Shared Code Init and Setup */
	if (e1000_set_mac_type(&sc->hw)) {
		device_printf(dev, "Setup init failure\n");
		return;
	}

	/* Are we a VF device? */
	if ((sc->hw.mac.type == e1000_vfadapt) ||
	    (sc->hw.mac.type == e1000_vfadapt_i350))
		sc->vf_ifp = 1;
	else
		sc->vf_ifp = 0;
}

static int
em_allocate_pci_resources(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	int rid, val;

	rid = PCIR_BAR(0);
	sc->memory = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->memory == NULL) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}
	sc->osdep.mem_bus_space_tag = rman_get_bustag(sc->memory);
	sc->osdep.mem_bus_space_handle =
	    rman_get_bushandle(sc->memory);
	sc->hw.hw_addr = (u8 *)&sc->osdep.mem_bus_space_handle;

	/* Only older adapters use IO mapping */
	if (sc->hw.mac.type < em_mac_min && sc->hw.mac.type > e1000_82543) {
		/* Figure our where our IO BAR is ? */
		for (rid = PCIR_BAR(0); rid < PCIR_CIS;) {
			val = pci_read_config(dev, rid, 4);
			if (EM_BAR_TYPE(val) == EM_BAR_TYPE_IO) {
				break;
			}
			rid += 4;
			/* check for 64bit BAR */
			if (EM_BAR_MEM_TYPE(val) == EM_BAR_MEM_TYPE_64BIT)
				rid += 4;
		}
		if (rid >= PCIR_CIS) {
			device_printf(dev, "Unable to locate IO BAR\n");
			return (ENXIO);
		}
		sc->ioport = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		    &rid, RF_ACTIVE);
		if (sc->ioport == NULL) {
			device_printf(dev, "Unable to allocate bus resource: "
			    "ioport\n");
			return (ENXIO);
		}
		sc->hw.io_base = 0;
		sc->osdep.io_bus_space_tag =
		    rman_get_bustag(sc->ioport);
		sc->osdep.io_bus_space_handle =
		    rman_get_bushandle(sc->ioport);
	}

	sc->hw.back = &sc->osdep;

	return (0);
}

/*********************************************************************
 *
 *  Set up the MSI-X Interrupt handlers
 *
 **********************************************************************/
static int
em_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct em_rx_queue *rx_que = sc->rx_queues;
	struct em_tx_queue *tx_que = sc->tx_queues;
	int error, rid, i, vector = 0, rx_vectors;
	char buf[16];

	/* First set up ring resources */
	for (i = 0; i < sc->rx_num_queues; i++, rx_que++, vector++) {
		rid = vector + 1;
		snprintf(buf, sizeof(buf), "rxq%d", i);
		error = iflib_irq_alloc_generic(ctx, &rx_que->que_irq, rid, IFLIB_INTR_RXTX, em_msix_que, rx_que, rx_que->me, buf);
		if (error) {
			device_printf(iflib_get_dev(ctx), "Failed to allocate que int %d err: %d", i, error);
			sc->rx_num_queues = i + 1;
			goto fail;
		}

		rx_que->msix =  vector;

		/*
		 * Set the bit to enable interrupt
		 * in E1000_IMS -- bits 20 and 21
		 * are for RX0 and RX1, note this has
		 * NOTHING to do with the MSI-X vector
		 */
		if (sc->hw.mac.type == e1000_82574) {
			rx_que->eims = 1 << (20 + i);
			sc->ims |= rx_que->eims;
			sc->ivars |= (8 | rx_que->msix) << (i * 4);
		} else if (sc->hw.mac.type == e1000_82575)
			rx_que->eims = E1000_EICR_TX_QUEUE0 << vector;
		else
			rx_que->eims = 1 << vector;
	}
	rx_vectors = vector;

	vector = 0;
	for (i = 0; i < sc->tx_num_queues; i++, tx_que++, vector++) {
		snprintf(buf, sizeof(buf), "txq%d", i);
		tx_que = &sc->tx_queues[i];
		iflib_softirq_alloc_generic(ctx,
		    &sc->rx_queues[i % sc->rx_num_queues].que_irq,
		    IFLIB_INTR_TX, tx_que, tx_que->me, buf);

		tx_que->msix = (vector % sc->rx_num_queues);

		/*
		 * Set the bit to enable interrupt
		 * in E1000_IMS -- bits 22 and 23
		 * are for TX0 and TX1, note this has
		 * NOTHING to do with the MSI-X vector
		 */
		if (sc->hw.mac.type == e1000_82574) {
			tx_que->eims = 1 << (22 + i);
			sc->ims |= tx_que->eims;
			sc->ivars |= (8 | tx_que->msix) << (8 + (i * 4));
		} else if (sc->hw.mac.type == e1000_82575) {
			tx_que->eims = E1000_EICR_TX_QUEUE0 << i;
		} else {
			tx_que->eims = 1 << i;
		}
	}

	/* Link interrupt */
	rid = rx_vectors + 1;
	error = iflib_irq_alloc_generic(ctx, &sc->irq, rid, IFLIB_INTR_ADMIN, em_msix_link, sc, 0, "aq");

	if (error) {
		device_printf(iflib_get_dev(ctx), "Failed to register admin handler");
		goto fail;
	}
	sc->linkvec = rx_vectors;
	if (sc->hw.mac.type < igb_mac_min) {
		sc->ivars |=  (8 | rx_vectors) << 16;
		sc->ivars |= 0x80000000;
		/* Enable the "Other" interrupt type for link status change */
		sc->ims |= E1000_IMS_OTHER;
	}

	return (0);
fail:
	iflib_irq_free(ctx, &sc->irq);
	rx_que = sc->rx_queues;
	for (int i = 0; i < sc->rx_num_queues; i++, rx_que++)
		iflib_irq_free(ctx, &rx_que->que_irq);
	return (error);
}

static void
igb_configure_queues(struct e1000_softc *sc)
{
	struct e1000_hw *hw = &sc->hw;
	struct em_rx_queue *rx_que;
	struct em_tx_queue *tx_que;
	u32 tmp, ivar = 0, newitr = 0;

	/* First turn on RSS capability */
	if (hw->mac.type != e1000_82575)
		E1000_WRITE_REG(hw, E1000_GPIE,
		    E1000_GPIE_MSIX_MODE | E1000_GPIE_EIAME |
		    E1000_GPIE_PBA | E1000_GPIE_NSICR);

	/* Turn on MSI-X */
	switch (hw->mac.type) {
	case e1000_82580:
	case e1000_i350:
	case e1000_i354:
	case e1000_i210:
	case e1000_i211:
	case e1000_vfadapt:
	case e1000_vfadapt_i350:
		/* RX entries */
		for (int i = 0; i < sc->rx_num_queues; i++) {
			u32 index = i >> 1;
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			rx_que = &sc->rx_queues[i];
			if (i & 1) {
				ivar &= 0xFF00FFFF;
				ivar |= (rx_que->msix | E1000_IVAR_VALID) << 16;
			} else {
				ivar &= 0xFFFFFF00;
				ivar |= rx_que->msix | E1000_IVAR_VALID;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
		}
		/* TX entries */
		for (int i = 0; i < sc->tx_num_queues; i++) {
			u32 index = i >> 1;
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			tx_que = &sc->tx_queues[i];
			if (i & 1) {
				ivar &= 0x00FFFFFF;
				ivar |= (tx_que->msix | E1000_IVAR_VALID) << 24;
			} else {
				ivar &= 0xFFFF00FF;
				ivar |= (tx_que->msix | E1000_IVAR_VALID) << 8;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
			sc->que_mask |= tx_que->eims;
		}

		/* And for the link interrupt */
		ivar = (sc->linkvec | E1000_IVAR_VALID) << 8;
		sc->link_mask = 1 << sc->linkvec;
		E1000_WRITE_REG(hw, E1000_IVAR_MISC, ivar);
		break;
	case e1000_82576:
		/* RX entries */
		for (int i = 0; i < sc->rx_num_queues; i++) {
			u32 index = i & 0x7; /* Each IVAR has two entries */
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			rx_que = &sc->rx_queues[i];
			if (i < 8) {
				ivar &= 0xFFFFFF00;
				ivar |= rx_que->msix | E1000_IVAR_VALID;
			} else {
				ivar &= 0xFF00FFFF;
				ivar |= (rx_que->msix | E1000_IVAR_VALID) << 16;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
			sc->que_mask |= rx_que->eims;
		}
		/* TX entries */
		for (int i = 0; i < sc->tx_num_queues; i++) {
			u32 index = i & 0x7; /* Each IVAR has two entries */
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			tx_que = &sc->tx_queues[i];
			if (i < 8) {
				ivar &= 0xFFFF00FF;
				ivar |= (tx_que->msix | E1000_IVAR_VALID) << 8;
			} else {
				ivar &= 0x00FFFFFF;
				ivar |= (tx_que->msix | E1000_IVAR_VALID) << 24;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
			sc->que_mask |= tx_que->eims;
		}

		/* And for the link interrupt */
		ivar = (sc->linkvec | E1000_IVAR_VALID) << 8;
		sc->link_mask = 1 << sc->linkvec;
		E1000_WRITE_REG(hw, E1000_IVAR_MISC, ivar);
		break;

	case e1000_82575:
		/* enable MSI-X support*/
		tmp = E1000_READ_REG(hw, E1000_CTRL_EXT);
		tmp |= E1000_CTRL_EXT_PBA_CLR;
		/* Auto-Mask interrupts upon ICR read. */
		tmp |= E1000_CTRL_EXT_EIAME;
		tmp |= E1000_CTRL_EXT_IRCA;
		E1000_WRITE_REG(hw, E1000_CTRL_EXT, tmp);

		/* Queues */
		for (int i = 0; i < sc->rx_num_queues; i++) {
			rx_que = &sc->rx_queues[i];
			tmp = E1000_EICR_RX_QUEUE0 << i;
			tmp |= E1000_EICR_TX_QUEUE0 << i;
			rx_que->eims = tmp;
			E1000_WRITE_REG_ARRAY(hw, E1000_MSIXBM(0),
			    i, rx_que->eims);
			sc->que_mask |= rx_que->eims;
		}

		/* Link */
		E1000_WRITE_REG(hw, E1000_MSIXBM(sc->linkvec),
		    E1000_EIMS_OTHER);
		sc->link_mask |= E1000_EIMS_OTHER;
	default:
		break;
	}

	/* Set the starting interrupt rate */
	if (em_max_interrupt_rate > 0)
		newitr = (4000000 / em_max_interrupt_rate) & 0x7FFC;

	if (hw->mac.type == e1000_82575)
		newitr |= newitr << 16;
	else
		newitr |= E1000_EITR_CNT_IGNR;

	for (int i = 0; i < sc->rx_num_queues; i++) {
		rx_que = &sc->rx_queues[i];
		E1000_WRITE_REG(hw, E1000_EITR(rx_que->msix), newitr);
	}

	return;
}

static void
em_free_pci_resources(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct em_rx_queue *que = sc->rx_queues;
	device_t dev = iflib_get_dev(ctx);

	/* Release all MSI-X queue resources */
	if (sc->intr_type == IFLIB_INTR_MSIX)
		iflib_irq_free(ctx, &sc->irq);

	if (que != NULL) {
		for (int i = 0; i < sc->rx_num_queues; i++, que++) {
			iflib_irq_free(ctx, &que->que_irq);
		}
	}

	if (sc->memory != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->memory), sc->memory);
		sc->memory = NULL;
	}

	if (sc->flash != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->flash), sc->flash);
		sc->flash = NULL;
	}

	if (sc->ioport != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT,
		    rman_get_rid(sc->ioport), sc->ioport);
		sc->ioport = NULL;
	}
}

/* Set up MSI or MSI-X */
static int
em_setup_msix(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);

	if (sc->hw.mac.type == e1000_82574) {
		em_enable_vectors_82574(ctx);
	}
	return (0);
}

/*********************************************************************
 *
 *  Workaround for SmartSpeed on 82541 and 82547 controllers
 *
 **********************************************************************/
static void
lem_smartspeed(struct e1000_softc *sc)
{
	u16 phy_tmp;

	if (sc->link_active || (sc->hw.phy.type != e1000_phy_igp) ||
	    sc->hw.mac.autoneg == 0 ||
	    (sc->hw.phy.autoneg_advertised & ADVERTISE_1000_FULL) == 0)
		return;

	if (sc->smartspeed == 0) {
		/* If Master/Slave config fault is asserted twice,
		 * we assume back-to-back */
		e1000_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if (!(phy_tmp & SR_1000T_MS_CONFIG_FAULT))
			return;
		e1000_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if (phy_tmp & SR_1000T_MS_CONFIG_FAULT) {
			e1000_read_phy_reg(&sc->hw,
			    PHY_1000T_CTRL, &phy_tmp);
			if(phy_tmp & CR_1000T_MS_ENABLE) {
				phy_tmp &= ~CR_1000T_MS_ENABLE;
				e1000_write_phy_reg(&sc->hw,
				    PHY_1000T_CTRL, phy_tmp);
				sc->smartspeed++;
				if(sc->hw.mac.autoneg &&
				   !e1000_copper_link_autoneg(&sc->hw) &&
				   !e1000_read_phy_reg(&sc->hw,
				    PHY_CONTROL, &phy_tmp)) {
					phy_tmp |= (MII_CR_AUTO_NEG_EN |
						    MII_CR_RESTART_AUTO_NEG);
					e1000_write_phy_reg(&sc->hw,
					    PHY_CONTROL, phy_tmp);
				}
			}
		}
		return;
	} else if(sc->smartspeed == EM_SMARTSPEED_DOWNSHIFT) {
		/* If still no link, perhaps using 2/3 pair cable */
		e1000_read_phy_reg(&sc->hw, PHY_1000T_CTRL, &phy_tmp);
		phy_tmp |= CR_1000T_MS_ENABLE;
		e1000_write_phy_reg(&sc->hw, PHY_1000T_CTRL, phy_tmp);
		if(sc->hw.mac.autoneg &&
		   !e1000_copper_link_autoneg(&sc->hw) &&
		   !e1000_read_phy_reg(&sc->hw, PHY_CONTROL, &phy_tmp)) {
			phy_tmp |= (MII_CR_AUTO_NEG_EN |
				    MII_CR_RESTART_AUTO_NEG);
			e1000_write_phy_reg(&sc->hw, PHY_CONTROL, phy_tmp);
		}
	}
	/* Restart process after EM_SMARTSPEED_MAX iterations */
	if(sc->smartspeed++ == EM_SMARTSPEED_MAX)
		sc->smartspeed = 0;
}

/*********************************************************************
 *
 *  Initialize the DMA Coalescing feature
 *
 **********************************************************************/
static void
igb_init_dmac(struct e1000_softc *sc, u32 pba)
{
	device_t	dev = sc->dev;
	struct e1000_hw *hw = &sc->hw;
	u32 		dmac, reg = ~E1000_DMACR_DMAC_EN;
	u16		hwm;
	u16		max_frame_size;

	if (hw->mac.type == e1000_i211)
		return;

	max_frame_size = sc->shared->isc_max_frame_size;
	if (hw->mac.type > e1000_82580) {

		if (sc->dmac == 0) { /* Disabling it */
			E1000_WRITE_REG(hw, E1000_DMACR, reg);
			return;
		} else
			device_printf(dev, "DMA Coalescing enabled\n");

		/* Set starting threshold */
		E1000_WRITE_REG(hw, E1000_DMCTXTH, 0);

		hwm = 64 * pba - max_frame_size / 16;
		if (hwm < 64 * (pba - 6))
			hwm = 64 * (pba - 6);
		reg = E1000_READ_REG(hw, E1000_FCRTC);
		reg &= ~E1000_FCRTC_RTH_COAL_MASK;
		reg |= ((hwm << E1000_FCRTC_RTH_COAL_SHIFT)
		    & E1000_FCRTC_RTH_COAL_MASK);
		E1000_WRITE_REG(hw, E1000_FCRTC, reg);


		dmac = pba - max_frame_size / 512;
		if (dmac < pba - 10)
			dmac = pba - 10;
		reg = E1000_READ_REG(hw, E1000_DMACR);
		reg &= ~E1000_DMACR_DMACTHR_MASK;
		reg |= ((dmac << E1000_DMACR_DMACTHR_SHIFT)
		    & E1000_DMACR_DMACTHR_MASK);

		/* transition to L0x or L1 if available..*/
		reg |= (E1000_DMACR_DMAC_EN | E1000_DMACR_DMAC_LX_MASK);

		/* Check if status is 2.5Gb backplane connection
		* before configuration of watchdog timer, which is
		* in msec values in 12.8usec intervals
		* watchdog timer= msec values in 32usec intervals
		* for non 2.5Gb connection
		*/
		if (hw->mac.type == e1000_i354) {
			int status = E1000_READ_REG(hw, E1000_STATUS);
			if ((status & E1000_STATUS_2P5_SKU) &&
			    (!(status & E1000_STATUS_2P5_SKU_OVER)))
				reg |= ((sc->dmac * 5) >> 6);
			else
				reg |= (sc->dmac >> 5);
		} else {
			reg |= (sc->dmac >> 5);
		}

		E1000_WRITE_REG(hw, E1000_DMACR, reg);

		E1000_WRITE_REG(hw, E1000_DMCRTRH, 0);

		/* Set the interval before transition */
		reg = E1000_READ_REG(hw, E1000_DMCTLX);
		if (hw->mac.type == e1000_i350)
			reg |= IGB_DMCTLX_DCFLUSH_DIS;
		/*
		** in 2.5Gb connection, TTLX unit is 0.4 usec
		** which is 0x4*2 = 0xA. But delay is still 4 usec
		*/
		if (hw->mac.type == e1000_i354) {
			int status = E1000_READ_REG(hw, E1000_STATUS);
			if ((status & E1000_STATUS_2P5_SKU) &&
			    (!(status & E1000_STATUS_2P5_SKU_OVER)))
				reg |= 0xA;
			else
				reg |= 0x4;
		} else {
			reg |= 0x4;
		}

		E1000_WRITE_REG(hw, E1000_DMCTLX, reg);

		/* free space in tx packet buffer to wake from DMA coal */
		E1000_WRITE_REG(hw, E1000_DMCTXTH, (IGB_TXPBSIZE -
		    (2 * max_frame_size)) >> 6);

		/* make low power state decision controlled by DMA coal */
		reg = E1000_READ_REG(hw, E1000_PCIEMISC);
		reg &= ~E1000_PCIEMISC_LX_DECISION;
		E1000_WRITE_REG(hw, E1000_PCIEMISC, reg);

	} else if (hw->mac.type == e1000_82580) {
		u32 reg = E1000_READ_REG(hw, E1000_PCIEMISC);
		E1000_WRITE_REG(hw, E1000_PCIEMISC,
		    reg & ~E1000_PCIEMISC_LX_DECISION);
		E1000_WRITE_REG(hw, E1000_DMACR, 0);
	}
}
/*********************************************************************
 * The 3 following flush routines are used as a workaround in the
 * I219 client parts and only for them.
 *
 * em_flush_tx_ring - remove all descriptors from the tx_ring
 *
 * We want to clear all pending descriptors from the TX ring.
 * zeroing happens when the HW reads the regs. We assign the ring itself as
 * the data of the next descriptor. We don't care about the data we are about
 * to reset the HW.
 **********************************************************************/
static void
em_flush_tx_ring(struct e1000_softc *sc)
{
	struct e1000_hw		*hw = &sc->hw;
	struct tx_ring		*txr = &sc->tx_queues->txr;
	struct e1000_tx_desc	*txd;
	u32			tctl, txd_lower = E1000_TXD_CMD_IFCS;
	u16			size = 512;

	tctl = E1000_READ_REG(hw, E1000_TCTL);
	E1000_WRITE_REG(hw, E1000_TCTL, tctl | E1000_TCTL_EN);

	txd = &txr->tx_base[txr->tx_cidx_processed];

	/* Just use the ring as a dummy buffer addr */
	txd->buffer_addr = txr->tx_paddr;
	txd->lower.data = htole32(txd_lower | size);
	txd->upper.data = 0;

	/* flush descriptors to memory before notifying the HW */
	wmb();

	E1000_WRITE_REG(hw, E1000_TDT(0), txr->tx_cidx_processed);
	mb();
	usec_delay(250);
}

/*********************************************************************
 * em_flush_rx_ring - remove all descriptors from the rx_ring
 *
 * Mark all descriptors in the RX ring as consumed and disable the rx ring
 **********************************************************************/
static void
em_flush_rx_ring(struct e1000_softc *sc)
{
	struct e1000_hw	*hw = &sc->hw;
	u32		rctl, rxdctl;

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);
	E1000_WRITE_FLUSH(hw);
	usec_delay(150);

	rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(0));
	/* zero the lower 14 bits (prefetch and host thresholds) */
	rxdctl &= 0xffffc000;
	/*
	 * update thresholds: prefetch threshold to 31, host threshold to 1
	 * and make sure the granularity is "descriptors" and not "cache lines"
	 */
	rxdctl |= (0x1F | (1 << 8) | E1000_RXDCTL_THRESH_UNIT_DESC);
	E1000_WRITE_REG(hw, E1000_RXDCTL(0), rxdctl);

	/* momentarily enable the RX ring for the changes to take effect */
	E1000_WRITE_REG(hw, E1000_RCTL, rctl | E1000_RCTL_EN);
	E1000_WRITE_FLUSH(hw);
	usec_delay(150);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);
}

/*********************************************************************
 * em_flush_desc_rings - remove all descriptors from the descriptor rings
 *
 * In I219, the descriptor rings must be emptied before resetting the HW
 * or before changing the device state to D3 during runtime (runtime PM).
 *
 * Failure to do this will cause the HW to enter a unit hang state which can
 * only be released by PCI reset on the device
 *
 **********************************************************************/
static void
em_flush_desc_rings(struct e1000_softc *sc)
{
	struct e1000_hw	*hw = &sc->hw;
	device_t dev = sc->dev;
	u16		hang_state;
	u32		fext_nvm11, tdlen;

	/* First, disable MULR fix in FEXTNVM11 */
	fext_nvm11 = E1000_READ_REG(hw, E1000_FEXTNVM11);
	fext_nvm11 |= E1000_FEXTNVM11_DISABLE_MULR_FIX;
	E1000_WRITE_REG(hw, E1000_FEXTNVM11, fext_nvm11);

	/* do nothing if we're not in faulty state, or if the queue is empty */
	tdlen = E1000_READ_REG(hw, E1000_TDLEN(0));
	hang_state = pci_read_config(dev, PCICFG_DESC_RING_STATUS, 2);
	if (!(hang_state & FLUSH_DESC_REQUIRED) || !tdlen)
		return;
	em_flush_tx_ring(sc);

	/* recheck, maybe the fault is caused by the rx ring */
	hang_state = pci_read_config(dev, PCICFG_DESC_RING_STATUS, 2);
	if (hang_state & FLUSH_DESC_REQUIRED)
		em_flush_rx_ring(sc);
}


/*********************************************************************
 *
 *  Initialize the hardware to a configuration as specified by the
 *  sc structure.
 *
 **********************************************************************/
static void
em_reset(if_ctx_t ctx)
{
	device_t dev = iflib_get_dev(ctx);
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	struct e1000_hw *hw = &sc->hw;
	u32 rx_buffer_size;
	u32 pba;

	INIT_DEBUGOUT("em_reset: begin");
	/* Let the firmware know the OS is in control */
	em_get_hw_control(sc);

	/* Set up smart power down as default off on newer adapters. */
	if (!em_smart_pwr_down && (hw->mac.type == e1000_82571 ||
	    hw->mac.type == e1000_82572)) {
		u16 phy_tmp = 0;

		/* Speed up time to link by disabling smart power down. */
		e1000_read_phy_reg(hw, IGP02E1000_PHY_POWER_MGMT, &phy_tmp);
		phy_tmp &= ~IGP02E1000_PM_SPD;
		e1000_write_phy_reg(hw, IGP02E1000_PHY_POWER_MGMT, phy_tmp);
	}

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 */
	switch (hw->mac.type) {
	/* 82547: Total Packet Buffer is 40K */
	case e1000_82547:
	case e1000_82547_rev_2:
		if (hw->mac.max_frame_size > 8192)
			pba = E1000_PBA_22K; /* 22K for Rx, 18K for Tx */
		else
			pba = E1000_PBA_30K; /* 30K for Rx, 10K for Tx */
		break;
	/* 82571/82572/80003es2lan: Total Packet Buffer is 48K */
	case e1000_82571:
	case e1000_82572:
	case e1000_80003es2lan:
			pba = E1000_PBA_32K; /* 32K for Rx, 16K for Tx */
		break;
	/* 82573: Total Packet Buffer is 32K */
	case e1000_82573:
			pba = E1000_PBA_12K; /* 12K for Rx, 20K for Tx */
		break;
	case e1000_82574:
	case e1000_82583:
			pba = E1000_PBA_20K; /* 20K for Rx, 20K for Tx */
		break;
	case e1000_ich8lan:
		pba = E1000_PBA_8K;
		break;
	case e1000_ich9lan:
	case e1000_ich10lan:
		/* Boost Receive side for jumbo frames */
		if (hw->mac.max_frame_size > 4096)
			pba = E1000_PBA_14K;
		else
			pba = E1000_PBA_10K;
		break;
	case e1000_pchlan:
	case e1000_pch2lan:
	case e1000_pch_lpt:
	case e1000_pch_spt:
	case e1000_pch_cnp:
	case e1000_pch_tgp:
	case e1000_pch_adp:
	case e1000_pch_mtp:
	case e1000_pch_ptp:
		pba = E1000_PBA_26K;
		break;
	case e1000_82575:
		pba = E1000_PBA_32K;
		break;
	case e1000_82576:
	case e1000_vfadapt:
		pba = E1000_READ_REG(hw, E1000_RXPBS);
		pba &= E1000_RXPBS_SIZE_MASK_82576;
		break;
	case e1000_82580:
	case e1000_i350:
	case e1000_i354:
	case e1000_vfadapt_i350:
		pba = E1000_READ_REG(hw, E1000_RXPBS);
		pba = e1000_rxpbs_adjust_82580(pba);
		break;
	case e1000_i210:
	case e1000_i211:
		pba = E1000_PBA_34K;
		break;
	default:
		/* Remaining devices assumed to have a Packet Buffer of 64K. */
		if (hw->mac.max_frame_size > 8192)
			pba = E1000_PBA_40K; /* 40K for Rx, 24K for Tx */
		else
			pba = E1000_PBA_48K; /* 48K for Rx, 16K for Tx */
	}

	/* Special needs in case of Jumbo frames */
	if ((hw->mac.type == e1000_82575) && (if_getmtu(ifp) > ETHERMTU)) {
		u32 tx_space, min_tx, min_rx;
		pba = E1000_READ_REG(hw, E1000_PBA);
		tx_space = pba >> 16;
		pba &= 0xffff;
		min_tx = (hw->mac.max_frame_size +
		    sizeof(struct e1000_tx_desc) - ETHERNET_FCS_SIZE) * 2;
		min_tx = roundup2(min_tx, 1024);
		min_tx >>= 10;
		min_rx = hw->mac.max_frame_size;
		min_rx = roundup2(min_rx, 1024);
		min_rx >>= 10;
		if (tx_space < min_tx &&
		    ((min_tx - tx_space) < pba)) {
			pba = pba - (min_tx - tx_space);
			/*
			 * if short on rx space, rx wins
			 * and must trump tx adjustment
			 */
			if (pba < min_rx)
				pba = min_rx;
		}
		E1000_WRITE_REG(hw, E1000_PBA, pba);
	}

	if (hw->mac.type < igb_mac_min)
		E1000_WRITE_REG(hw, E1000_PBA, pba);

	INIT_DEBUGOUT1("em_reset: pba=%dK",pba);

	/*
	 * These parameters control the automatic generation (Tx) and
	 * response (Rx) to Ethernet PAUSE frames.
	 * - High water mark should allow for at least two frames to be
	 *   received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitrary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 * - The pause time is fairly large at 1000 x 512ns = 512 usec.
	 */
	rx_buffer_size = (pba & 0xffff) << 10;
	hw->fc.high_water = rx_buffer_size -
	    roundup2(hw->mac.max_frame_size, 1024);
	hw->fc.low_water = hw->fc.high_water - 1500;

	if (sc->fc) /* locally set flow control value? */
		hw->fc.requested_mode = sc->fc;
	else
		hw->fc.requested_mode = e1000_fc_full;

	if (hw->mac.type == e1000_80003es2lan)
		hw->fc.pause_time = 0xFFFF;
	else
		hw->fc.pause_time = EM_FC_PAUSE_TIME;

	hw->fc.send_xon = true;

	/* Device specific overrides/settings */
	switch (hw->mac.type) {
	case e1000_pchlan:
		/* Workaround: no TX flow ctrl for PCH */
		hw->fc.requested_mode = e1000_fc_rx_pause;
		hw->fc.pause_time = 0xFFFF; /* override */
		if (if_getmtu(ifp) > ETHERMTU) {
			hw->fc.high_water = 0x3500;
			hw->fc.low_water = 0x1500;
		} else {
			hw->fc.high_water = 0x5000;
			hw->fc.low_water = 0x3000;
		}
		hw->fc.refresh_time = 0x1000;
		break;
	case e1000_pch2lan:
	case e1000_pch_lpt:
	case e1000_pch_spt:
	case e1000_pch_cnp:
	case e1000_pch_tgp:
	case e1000_pch_adp:
	case e1000_pch_mtp:
	case e1000_pch_ptp:
		hw->fc.high_water = 0x5C20;
		hw->fc.low_water = 0x5048;
		hw->fc.pause_time = 0x0650;
		hw->fc.refresh_time = 0x0400;
		/* Jumbos need adjusted PBA */
		if (if_getmtu(ifp) > ETHERMTU)
			E1000_WRITE_REG(hw, E1000_PBA, 12);
		else
			E1000_WRITE_REG(hw, E1000_PBA, 26);
		break;
	case e1000_82575:
	case e1000_82576:
		/* 8-byte granularity */
		hw->fc.low_water = hw->fc.high_water - 8;
		break;
	case e1000_82580:
	case e1000_i350:
	case e1000_i354:
	case e1000_i210:
	case e1000_i211:
	case e1000_vfadapt:
	case e1000_vfadapt_i350:
		/* 16-byte granularity */
		hw->fc.low_water = hw->fc.high_water - 16;
		break;
	case e1000_ich9lan:
	case e1000_ich10lan:
		if (if_getmtu(ifp) > ETHERMTU) {
			hw->fc.high_water = 0x2800;
			hw->fc.low_water = hw->fc.high_water - 8;
			break;
		}
		/* FALLTHROUGH */
	default:
		if (hw->mac.type == e1000_80003es2lan)
			hw->fc.pause_time = 0xFFFF;
		break;
	}

	/* I219 needs some special flushing to avoid hangs */
	if (sc->hw.mac.type >= e1000_pch_spt && sc->hw.mac.type < igb_mac_min)
		em_flush_desc_rings(sc);

	/* Issue a global reset */
	e1000_reset_hw(hw);
	if (hw->mac.type >= igb_mac_min) {
		E1000_WRITE_REG(hw, E1000_WUC, 0);
	} else {
		E1000_WRITE_REG(hw, E1000_WUFC, 0);
		em_disable_aspm(sc);
	}
	if (sc->flags & IGB_MEDIA_RESET) {
		e1000_setup_init_funcs(hw, true);
		e1000_get_bus_info(hw);
		sc->flags &= ~IGB_MEDIA_RESET;
	}
	/* and a re-init */
	if (e1000_init_hw(hw) < 0) {
		device_printf(dev, "Hardware Initialization Failed\n");
		return;
	}
	if (hw->mac.type >= igb_mac_min)
		igb_init_dmac(sc, pba);

	E1000_WRITE_REG(hw, E1000_VET, ETHERTYPE_VLAN);
	e1000_get_phy_info(hw);
	e1000_check_for_link(hw);
}

/*
 * Initialise the RSS mapping for NICs that support multiple transmit/
 * receive rings.
 */

#define RSSKEYLEN 10
static void
em_initialize_rss_mapping(struct e1000_softc *sc)
{
	uint8_t  rss_key[4 * RSSKEYLEN];
	uint32_t reta = 0;
	struct e1000_hw	*hw = &sc->hw;
	int i;

	/*
	 * Configure RSS key
	 */
	arc4rand(rss_key, sizeof(rss_key), 0);
	for (i = 0; i < RSSKEYLEN; ++i) {
		uint32_t rssrk = 0;

		rssrk = EM_RSSRK_VAL(rss_key, i);
		E1000_WRITE_REG(hw,E1000_RSSRK(i), rssrk);
	}

	/*
	 * Configure RSS redirect table in following fashion:
	 * (hash & ring_cnt_mask) == rdr_table[(hash & rdr_table_mask)]
	 */
	for (i = 0; i < sizeof(reta); ++i) {
		uint32_t q;

		q = (i % sc->rx_num_queues) << 7;
		reta |= q << (8 * i);
	}

	for (i = 0; i < 32; ++i)
		E1000_WRITE_REG(hw, E1000_RETA(i), reta);

	E1000_WRITE_REG(hw, E1000_MRQC, E1000_MRQC_RSS_ENABLE_2Q |
			E1000_MRQC_RSS_FIELD_IPV4_TCP |
			E1000_MRQC_RSS_FIELD_IPV4 |
			E1000_MRQC_RSS_FIELD_IPV6_TCP_EX |
			E1000_MRQC_RSS_FIELD_IPV6_EX |
			E1000_MRQC_RSS_FIELD_IPV6);
}

static void
igb_initialize_rss_mapping(struct e1000_softc *sc)
{
	struct e1000_hw *hw = &sc->hw;
	int i;
	int queue_id;
	u32 reta;
	u32 rss_key[10], mrqc, shift = 0;

	/* XXX? */
	if (hw->mac.type == e1000_82575)
		shift = 6;

	/*
	 * The redirection table controls which destination
	 * queue each bucket redirects traffic to.
	 * Each DWORD represents four queues, with the LSB
	 * being the first queue in the DWORD.
	 *
	 * This just allocates buckets to queues using round-robin
	 * allocation.
	 *
	 * NOTE: It Just Happens to line up with the default
	 * RSS allocation method.
	 */

	/* Warning FM follows */
	reta = 0;
	for (i = 0; i < 128; i++) {
#ifdef RSS
		queue_id = rss_get_indirection_to_bucket(i);
		/*
		 * If we have more queues than buckets, we'll
		 * end up mapping buckets to a subset of the
		 * queues.
		 *
		 * If we have more buckets than queues, we'll
		 * end up instead assigning multiple buckets
		 * to queues.
		 *
		 * Both are suboptimal, but we need to handle
		 * the case so we don't go out of bounds
		 * indexing arrays and such.
		 */
		queue_id = queue_id % sc->rx_num_queues;
#else
		queue_id = (i % sc->rx_num_queues);
#endif
		/* Adjust if required */
		queue_id = queue_id << shift;

		/*
		 * The low 8 bits are for hash value (n+0);
		 * The next 8 bits are for hash value (n+1), etc.
		 */
		reta = reta >> 8;
		reta = reta | ( ((uint32_t) queue_id) << 24);
		if ((i & 3) == 3) {
			E1000_WRITE_REG(hw, E1000_RETA(i >> 2), reta);
			reta = 0;
		}
	}

	/* Now fill in hash table */

	/*
	 * MRQC: Multiple Receive Queues Command
	 * Set queuing to RSS control, number depends on the device.
	 */
	mrqc = E1000_MRQC_ENABLE_RSS_MQ;

#ifdef RSS
	/* XXX ew typecasting */
	rss_getkey((uint8_t *) &rss_key);
#else
	arc4rand(&rss_key, sizeof(rss_key), 0);
#endif
	for (i = 0; i < 10; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_RSSRK(0), i, rss_key[i]);

	/*
	 * Configure the RSS fields to hash upon.
	 */
	mrqc |= (E1000_MRQC_RSS_FIELD_IPV4 |
	    E1000_MRQC_RSS_FIELD_IPV4_TCP);
	mrqc |= (E1000_MRQC_RSS_FIELD_IPV6 |
	    E1000_MRQC_RSS_FIELD_IPV6_TCP);
	mrqc |=( E1000_MRQC_RSS_FIELD_IPV4_UDP |
	    E1000_MRQC_RSS_FIELD_IPV6_UDP);
	mrqc |=( E1000_MRQC_RSS_FIELD_IPV6_UDP_EX |
	    E1000_MRQC_RSS_FIELD_IPV6_TCP_EX);

	E1000_WRITE_REG(hw, E1000_MRQC, mrqc);
}

/*********************************************************************
 *
 *  Setup networking device structure and register interface media.
 *
 **********************************************************************/
static int
em_setup_interface(if_ctx_t ctx)
{
	if_t ifp = iflib_get_ifp(ctx);
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;

	INIT_DEBUGOUT("em_setup_interface: begin");

	/* Single Queue */
	if (sc->tx_num_queues == 1) {
		if_setsendqlen(ifp, scctx->isc_ntxd[0] - 1);
		if_setsendqready(ifp);
	}

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	if (sc->hw.phy.media_type == e1000_media_type_fiber ||
	    sc->hw.phy.media_type == e1000_media_type_internal_serdes) {
		u_char fiber_type = IFM_1000_SX;	/* default type */

		if (sc->hw.mac.type == e1000_82545)
			fiber_type = IFM_1000_LX;
		ifmedia_add(sc->media, IFM_ETHER | fiber_type | IFM_FDX, 0, NULL);
		ifmedia_add(sc->media, IFM_ETHER | fiber_type, 0, NULL);
	} else {
		ifmedia_add(sc->media, IFM_ETHER | IFM_10_T, 0, NULL);
		ifmedia_add(sc->media, IFM_ETHER | IFM_10_T | IFM_FDX, 0, NULL);
		ifmedia_add(sc->media, IFM_ETHER | IFM_100_TX, 0, NULL);
		ifmedia_add(sc->media, IFM_ETHER | IFM_100_TX | IFM_FDX, 0, NULL);
		if (sc->hw.phy.type != e1000_phy_ife) {
			ifmedia_add(sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
			ifmedia_add(sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
		}
	}
	ifmedia_add(sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(sc->media, IFM_ETHER | IFM_AUTO);
	return (0);
}

static int
em_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int ntxqs, int ntxqsets)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	int error = E1000_SUCCESS;
	struct em_tx_queue *que;
	int i, j;

	MPASS(sc->tx_num_queues > 0);
	MPASS(sc->tx_num_queues == ntxqsets);

	/* First allocate the top level queue structs */
	if (!(sc->tx_queues =
	    (struct em_tx_queue *) malloc(sizeof(struct em_tx_queue) *
	    sc->tx_num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(iflib_get_dev(ctx), "Unable to allocate queue memory\n");
		return(ENOMEM);
	}

	for (i = 0, que = sc->tx_queues; i < sc->tx_num_queues; i++, que++) {
		/* Set up some basics */

		struct tx_ring *txr = &que->txr;
		txr->sc = que->sc = sc;
		que->me = txr->me =  i;

		/* Allocate report status array */
		if (!(txr->tx_rsq = (qidx_t *) malloc(sizeof(qidx_t) * scctx->isc_ntxd[0], M_DEVBUF, M_NOWAIT | M_ZERO))) {
			device_printf(iflib_get_dev(ctx), "failed to allocate rs_idxs memory\n");
			error = ENOMEM;
			goto fail;
		}
		for (j = 0; j < scctx->isc_ntxd[0]; j++)
			txr->tx_rsq[j] = QIDX_INVALID;
		/* get the virtual and physical address of the hardware queues */
		txr->tx_base = (struct e1000_tx_desc *)vaddrs[i*ntxqs];
		txr->tx_paddr = paddrs[i*ntxqs];
	}

	if (bootverbose)
		device_printf(iflib_get_dev(ctx),
		    "allocated for %d tx_queues\n", sc->tx_num_queues);
	return (0);
fail:
	em_if_queues_free(ctx);
	return (error);
}

static int
em_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int nrxqs, int nrxqsets)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	int error = E1000_SUCCESS;
	struct em_rx_queue *que;
	int i;

	MPASS(sc->rx_num_queues > 0);
	MPASS(sc->rx_num_queues == nrxqsets);

	/* First allocate the top level queue structs */
	if (!(sc->rx_queues =
	    (struct em_rx_queue *) malloc(sizeof(struct em_rx_queue) *
	    sc->rx_num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(iflib_get_dev(ctx), "Unable to allocate queue memory\n");
		error = ENOMEM;
		goto fail;
	}

	for (i = 0, que = sc->rx_queues; i < nrxqsets; i++, que++) {
		/* Set up some basics */
		struct rx_ring *rxr = &que->rxr;
		rxr->sc = que->sc = sc;
		rxr->que = que;
		que->me = rxr->me =  i;

		/* get the virtual and physical address of the hardware queues */
		rxr->rx_base = (union e1000_rx_desc_extended *)vaddrs[i*nrxqs];
		rxr->rx_paddr = paddrs[i*nrxqs];
	}
 
	if (bootverbose)
		device_printf(iflib_get_dev(ctx),
		    "allocated for %d rx_queues\n", sc->rx_num_queues);

	return (0);
fail:
	em_if_queues_free(ctx);
	return (error);
}

static void
em_if_queues_free(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct em_tx_queue *tx_que = sc->tx_queues;
	struct em_rx_queue *rx_que = sc->rx_queues;

	if (tx_que != NULL) {
		for (int i = 0; i < sc->tx_num_queues; i++, tx_que++) {
			struct tx_ring *txr = &tx_que->txr;
			if (txr->tx_rsq == NULL)
				break;

			free(txr->tx_rsq, M_DEVBUF);
			txr->tx_rsq = NULL;
		}
		free(sc->tx_queues, M_DEVBUF);
		sc->tx_queues = NULL;
	}

	if (rx_que != NULL) {
		free(sc->rx_queues, M_DEVBUF);
		sc->rx_queues = NULL;
	}
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
em_initialize_transmit_unit(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	struct em_tx_queue *que;
	struct tx_ring	*txr;
	struct e1000_hw	*hw = &sc->hw;
	u32 tctl, txdctl = 0, tarc, tipg = 0;

	INIT_DEBUGOUT("em_initialize_transmit_unit: begin");

	for (int i = 0; i < sc->tx_num_queues; i++, txr++) {
		u64 bus_addr;
		caddr_t offp, endp;

		que = &sc->tx_queues[i];
		txr = &que->txr;
		bus_addr = txr->tx_paddr;

		/* Clear checksum offload context. */
		offp = (caddr_t)&txr->csum_flags;
		endp = (caddr_t)(txr + 1);
		bzero(offp, endp - offp);

		/* Base and Len of TX Ring */
		E1000_WRITE_REG(hw, E1000_TDLEN(i),
		    scctx->isc_ntxd[0] * sizeof(struct e1000_tx_desc));
		E1000_WRITE_REG(hw, E1000_TDBAH(i),
		    (u32)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(i),
		    (u32)bus_addr);
		/* Init the HEAD/TAIL indices */
		E1000_WRITE_REG(hw, E1000_TDT(i), 0);
		E1000_WRITE_REG(hw, E1000_TDH(i), 0);

		HW_DEBUGOUT2("Base = %x, Length = %x\n",
		    E1000_READ_REG(hw, E1000_TDBAL(i)),
		    E1000_READ_REG(hw, E1000_TDLEN(i)));

		txdctl = 0; /* clear txdctl */
		txdctl |= 0x1f; /* PTHRESH */
		txdctl |= 1 << 8; /* HTHRESH */
		txdctl |= 1 << 16;/* WTHRESH */
		txdctl |= 1 << 22; /* Reserved bit 22 must always be 1 */
		txdctl |= E1000_TXDCTL_GRAN;
		txdctl |= 1 << 25; /* LWTHRESH */

		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
	}

	/* Set the default values for the Tx Inter Packet Gap timer */
	switch (hw->mac.type) {
	case e1000_80003es2lan:
		tipg = DEFAULT_82543_TIPG_IPGR1;
		tipg |= DEFAULT_80003ES2LAN_TIPG_IPGR2 <<
		    E1000_TIPG_IPGR2_SHIFT;
		break;
	case e1000_82542:
		tipg = DEFAULT_82542_TIPG_IPGT;
		tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	default:
		if (hw->phy.media_type == e1000_media_type_fiber ||
		    hw->phy.media_type == e1000_media_type_internal_serdes)
			tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
		tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	}

	E1000_WRITE_REG(hw, E1000_TIPG, tipg);
	E1000_WRITE_REG(hw, E1000_TIDV, sc->tx_int_delay.value);

	if(hw->mac.type >= e1000_82540)
		E1000_WRITE_REG(hw, E1000_TADV,
		    sc->tx_abs_int_delay.value);

	if (hw->mac.type == e1000_82571 || hw->mac.type == e1000_82572) {
		tarc = E1000_READ_REG(hw, E1000_TARC(0));
		tarc |= TARC_SPEED_MODE_BIT;
		E1000_WRITE_REG(hw, E1000_TARC(0), tarc);
	} else if (hw->mac.type == e1000_80003es2lan) {
		/* errata: program both queues to unweighted RR */
		tarc = E1000_READ_REG(hw, E1000_TARC(0));
		tarc |= 1;
		E1000_WRITE_REG(hw, E1000_TARC(0), tarc);
		tarc = E1000_READ_REG(hw, E1000_TARC(1));
		tarc |= 1;
		E1000_WRITE_REG(hw, E1000_TARC(1), tarc);
	} else if (hw->mac.type == e1000_82574) {
		tarc = E1000_READ_REG(hw, E1000_TARC(0));
		tarc |= TARC_ERRATA_BIT;
		if ( sc->tx_num_queues > 1) {
			tarc |= (TARC_COMPENSATION_MODE | TARC_MQ_FIX);
			E1000_WRITE_REG(hw, E1000_TARC(0), tarc);
			E1000_WRITE_REG(hw, E1000_TARC(1), tarc);
		} else
			E1000_WRITE_REG(hw, E1000_TARC(0), tarc);
	}

	if (sc->tx_int_delay.value > 0)
		sc->txd_cmd |= E1000_TXD_CMD_IDE;

	/* Program the Transmit Control Register */
	tctl = E1000_READ_REG(hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	if (hw->mac.type >= e1000_82571)
		tctl |= E1000_TCTL_MULR;

	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(hw, E1000_TCTL, tctl);

	/* SPT and KBL errata workarounds */
	if (hw->mac.type == e1000_pch_spt) {
		u32 reg;
		reg = E1000_READ_REG(hw, E1000_IOSFPC);
		reg |= E1000_RCTL_RDMTS_HEX;
		E1000_WRITE_REG(hw, E1000_IOSFPC, reg);
		/* i218-i219 Specification Update 1.5.4.5 */
		reg = E1000_READ_REG(hw, E1000_TARC(0));
		reg &= ~E1000_TARC0_CB_MULTIQ_3_REQ;
		reg |= E1000_TARC0_CB_MULTIQ_2_REQ;
		E1000_WRITE_REG(hw, E1000_TARC(0), reg);
	}
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
#define BSIZEPKT_ROUNDUP ((1<<E1000_SRRCTL_BSIZEPKT_SHIFT)-1)

static void
em_initialize_receive_unit(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	if_t ifp = iflib_get_ifp(ctx);
	struct e1000_hw	*hw = &sc->hw;
	struct em_rx_queue *que;
	int i;
	uint32_t rctl, rxcsum;

	INIT_DEBUGOUT("em_initialize_receive_units: begin");

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	/* Do not disable if ever enabled on this hardware */
	if ((hw->mac.type != e1000_82574) && (hw->mac.type != e1000_82583))
		E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	/* Setup the Receive Control Register */
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM |
	    E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
	    (hw->mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	/* Do not store bad packets */
	rctl &= ~E1000_RCTL_SBP;

	/* Enable Long Packet receive */
	if (if_getmtu(ifp) > ETHERMTU)
		rctl |= E1000_RCTL_LPE;
	else
		rctl &= ~E1000_RCTL_LPE;

	/* Strip the CRC */
	if (!em_disable_crc_stripping)
		rctl |= E1000_RCTL_SECRC;

	if (hw->mac.type >= e1000_82540) {
		E1000_WRITE_REG(hw, E1000_RADV,
		    sc->rx_abs_int_delay.value);

		/*
		 * Set the interrupt throttling rate. Value is calculated
		 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns)
		 */
		E1000_WRITE_REG(hw, E1000_ITR, DEFAULT_ITR);
	}
	E1000_WRITE_REG(hw, E1000_RDTR, sc->rx_int_delay.value);

	if (hw->mac.type >= em_mac_min) {
		uint32_t rfctl;
		/* Use extended rx descriptor formats */
		rfctl = E1000_READ_REG(hw, E1000_RFCTL);
		rfctl |= E1000_RFCTL_EXTEN;

		/*
		 * When using MSI-X interrupts we need to throttle
		 * using the EITR register (82574 only)
		 */
		if (hw->mac.type == e1000_82574) {
			for (int i = 0; i < 4; i++)
				E1000_WRITE_REG(hw, E1000_EITR_82574(i),
				    DEFAULT_ITR);
			/* Disable accelerated acknowledge */
			rfctl |= E1000_RFCTL_ACK_DIS;
		}
		E1000_WRITE_REG(hw, E1000_RFCTL, rfctl);
	}

	/* Set up L3 and L4 csum Rx descriptor offloads */
	rxcsum = E1000_READ_REG(hw, E1000_RXCSUM);
	if (if_getcapenable(ifp) & IFCAP_RXCSUM) {
		rxcsum |= E1000_RXCSUM_TUOFL | E1000_RXCSUM_IPOFL;
		if (hw->mac.type > e1000_82575)
			rxcsum |= E1000_RXCSUM_CRCOFL;
		else if (hw->mac.type < em_mac_min &&
		    if_getcapenable(ifp) & IFCAP_HWCSUM_IPV6)
			rxcsum |= E1000_RXCSUM_IPV6OFL;
	} else {
		rxcsum &= ~(E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		if (hw->mac.type > e1000_82575)
			rxcsum &= ~E1000_RXCSUM_CRCOFL;
		else if (hw->mac.type < em_mac_min)
			rxcsum &= ~E1000_RXCSUM_IPV6OFL;
	}

	if (sc->rx_num_queues > 1) {
		/* RSS hash needed in the Rx descriptor */
		rxcsum |= E1000_RXCSUM_PCSD;

		if (hw->mac.type >= igb_mac_min)
			igb_initialize_rss_mapping(sc);
		else
			em_initialize_rss_mapping(sc);
	}
	E1000_WRITE_REG(hw, E1000_RXCSUM, rxcsum);

	/*
	 * XXX TEMPORARY WORKAROUND: on some systems with 82573
	 * long latencies are observed, like Lenovo X60. This
	 * change eliminates the problem, but since having positive
	 * values in RDTR is a known source of problems on other
	 * platforms another solution is being sought.
	 */
	if (hw->mac.type == e1000_82573)
		E1000_WRITE_REG(hw, E1000_RDTR, 0x20);

	for (i = 0, que = sc->rx_queues; i < sc->rx_num_queues; i++, que++) {
		struct rx_ring *rxr = &que->rxr;
		/* Setup the Base and Length of the Rx Descriptor Ring */
		u64 bus_addr = rxr->rx_paddr;
#if 0
		u32 rdt = sc->rx_num_queues -1;  /* default */
#endif

		E1000_WRITE_REG(hw, E1000_RDLEN(i),
		    scctx->isc_nrxd[0] * sizeof(union e1000_rx_desc_extended));
		E1000_WRITE_REG(hw, E1000_RDBAH(i), (u32)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i), (u32)bus_addr);
		/* Setup the Head and Tail Descriptor Pointers */
		E1000_WRITE_REG(hw, E1000_RDH(i), 0);
		E1000_WRITE_REG(hw, E1000_RDT(i), 0);
	}

	/*
	 * Set PTHRESH for improved jumbo performance
	 * According to 10.2.5.11 of Intel 82574 Datasheet,
	 * RXDCTL(1) is written whenever RXDCTL(0) is written.
	 * Only write to RXDCTL(1) if there is a need for different
	 * settings.
	 */
	if ((hw->mac.type == e1000_ich9lan || hw->mac.type == e1000_pch2lan ||
	    hw->mac.type == e1000_ich10lan) && if_getmtu(ifp) > ETHERMTU) {
		u32 rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(0));
		E1000_WRITE_REG(hw, E1000_RXDCTL(0), rxdctl | 3);
	} else if (hw->mac.type == e1000_82574) {
		for (int i = 0; i < sc->rx_num_queues; i++) {
			u32 rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
			rxdctl |= 0x20; /* PTHRESH */
			rxdctl |= 4 << 8; /* HTHRESH */
			rxdctl |= 4 << 16;/* WTHRESH */
			rxdctl |= 1 << 24; /* Switch to granularity */
			E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
		}
	} else if (hw->mac.type >= igb_mac_min) {
		u32 psize, srrctl = 0;

		if (if_getmtu(ifp) > ETHERMTU) {
			psize = scctx->isc_max_frame_size;
			/* are we on a vlan? */
			if (if_vlantrunkinuse(ifp))
				psize += VLAN_TAG_SIZE;

			if (sc->vf_ifp)
				e1000_rlpml_set_vf(hw, psize);
			else
				E1000_WRITE_REG(hw, E1000_RLPML, psize);
		}

		/* Set maximum packet buffer len */
		srrctl |= (sc->rx_mbuf_sz + BSIZEPKT_ROUNDUP) >>
		    E1000_SRRCTL_BSIZEPKT_SHIFT;

		/*
		 * If TX flow control is disabled and there's >1 queue defined,
		 * enable DROP.
		 *
		 * This drops frames rather than hanging the RX MAC for all queues.
		 */
		if ((sc->rx_num_queues > 1) &&
		    (sc->fc == e1000_fc_none ||
		     sc->fc == e1000_fc_rx_pause)) {
			srrctl |= E1000_SRRCTL_DROP_EN;
		}
			/* Setup the Base and Length of the Rx Descriptor Rings */
		for (i = 0, que = sc->rx_queues; i < sc->rx_num_queues; i++, que++) {
			struct rx_ring *rxr = &que->rxr;
			u64 bus_addr = rxr->rx_paddr;
			u32 rxdctl;

#ifdef notyet
			/* Configure for header split? -- ignore for now */
			rxr->hdr_split = igb_header_split;
#else
			srrctl |= E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;
#endif

			E1000_WRITE_REG(hw, E1000_RDLEN(i),
					scctx->isc_nrxd[0] * sizeof(struct e1000_rx_desc));
			E1000_WRITE_REG(hw, E1000_RDBAH(i),
					(uint32_t)(bus_addr >> 32));
			E1000_WRITE_REG(hw, E1000_RDBAL(i),
					(uint32_t)bus_addr);
			E1000_WRITE_REG(hw, E1000_SRRCTL(i), srrctl);
			/* Enable this Queue */
			rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
			rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
			rxdctl &= 0xFFF00000;
			rxdctl |= IGB_RX_PTHRESH;
			rxdctl |= IGB_RX_HTHRESH << 8;
			rxdctl |= IGB_RX_WTHRESH << 16;
			E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
		}		
	} else if (hw->mac.type >= e1000_pch2lan) {
		if (if_getmtu(ifp) > ETHERMTU)
			e1000_lv_jumbo_workaround_ich8lan(hw, true);
		else
			e1000_lv_jumbo_workaround_ich8lan(hw, false);
	}

	/* Make sure VLAN Filters are off */
	rctl &= ~E1000_RCTL_VFE;

	/* Set up packet buffer size, overridden by per queue srrctl on igb */
	if (hw->mac.type < igb_mac_min) {
		if (sc->rx_mbuf_sz > 2048 && sc->rx_mbuf_sz <= 4096)
			rctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX;
		else if (sc->rx_mbuf_sz > 4096 && sc->rx_mbuf_sz <= 8192)
			rctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX;
		else if (sc->rx_mbuf_sz > 8192)
			rctl |= E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX;
		else {
			rctl |= E1000_RCTL_SZ_2048;
			rctl &= ~E1000_RCTL_BSEX;
		}
	} else
		rctl |= E1000_RCTL_SZ_2048;

	/*
	 * rctl bits 11:10 are as follows
	 * lem: reserved
	 * em: DTYPE
	 * igb: reserved
	 * and should be 00 on all of the above
	 */
	rctl &= ~0x00000C00;

	/* Write out the settings */
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

	return;
}

static void
em_if_vlan_register(if_ctx_t ctx, u16 vtag)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	u32 index, bit;

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	sc->shadow_vfta[index] |= (1 << bit);
	++sc->num_vlans;
	em_if_vlan_filter_write(sc);
}

static void
em_if_vlan_unregister(if_ctx_t ctx, u16 vtag)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	u32 index, bit;

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	sc->shadow_vfta[index] &= ~(1 << bit);
	--sc->num_vlans;
	em_if_vlan_filter_write(sc);
}

static bool
em_if_vlan_filter_capable(if_ctx_t ctx)
{
	if_t ifp = iflib_get_ifp(ctx);

	if ((if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER) &&
	    !em_disable_crc_stripping)
		return (true);

	return (false);
}

static bool
em_if_vlan_filter_used(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);

	if (!em_if_vlan_filter_capable(ctx))
		return (false);

	for (int i = 0; i < EM_VFTA_SIZE; i++)
		if (sc->shadow_vfta[i] != 0)
			return (true);

	return (false);
}

static void
em_if_vlan_filter_enable(struct e1000_softc *sc)
{
	struct e1000_hw *hw = &sc->hw;
	u32 reg;

	reg = E1000_READ_REG(hw, E1000_RCTL);
	reg &= ~E1000_RCTL_CFIEN;
	reg |= E1000_RCTL_VFE;
	E1000_WRITE_REG(hw, E1000_RCTL, reg);
}

static void
em_if_vlan_filter_disable(struct e1000_softc *sc)
{
	struct e1000_hw *hw = &sc->hw;
	u32 reg;

	reg = E1000_READ_REG(hw, E1000_RCTL);
	reg &= ~(E1000_RCTL_VFE | E1000_RCTL_CFIEN);
	E1000_WRITE_REG(hw, E1000_RCTL, reg);
}

static void
em_if_vlan_filter_write(struct e1000_softc *sc)
{
	struct e1000_hw *hw = &sc->hw;

	if (sc->vf_ifp)
		return;

	/* Disable interrupts for lem-class devices during the filter change */
	if (hw->mac.type < em_mac_min)
		em_if_intr_disable(sc->ctx);

	for (int i = 0; i < EM_VFTA_SIZE; i++)
		if (sc->shadow_vfta[i] != 0) {
			/* XXXKB: incomplete VF support, we return early above */
			if (sc->vf_ifp)
				e1000_vfta_set_vf(hw, sc->shadow_vfta[i], true);
			else
				e1000_write_vfta(hw, i, sc->shadow_vfta[i]);
		}

	/* Re-enable interrupts for lem-class devices */
	if (hw->mac.type < em_mac_min)
		em_if_intr_enable(sc->ctx);
}

static void
em_setup_vlan_hw_support(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;
	if_t ifp = iflib_get_ifp(ctx);
	u32 reg;

	/* XXXKB: Return early if we are a VF until VF decap and filter management
	 * is ready and tested.
	 */
	if (sc->vf_ifp)
		return;

	if (if_getcapenable(ifp) & IFCAP_VLAN_HWTAGGING &&
	    !em_disable_crc_stripping) {
		reg = E1000_READ_REG(hw, E1000_CTRL);
		reg |= E1000_CTRL_VME;
		E1000_WRITE_REG(hw, E1000_CTRL, reg);
	} else {
		reg = E1000_READ_REG(hw, E1000_CTRL);
		reg &= ~E1000_CTRL_VME;
		E1000_WRITE_REG(hw, E1000_CTRL, reg);
	}

	/* If we aren't doing HW filtering, we're done */
	if (!em_if_vlan_filter_capable(ctx))  {
		em_if_vlan_filter_disable(sc);
		return;
	}

	/*
	 * A soft reset zero's out the VFTA, so
	 * we need to repopulate it now.
	 * We also insert VLAN 0 in the filter list, so we pass VLAN 0 tagged
	 * traffic through. This will write the entire table.
	 */
	em_if_vlan_register(ctx, 0);

	/* Enable the Filter Table */
	em_if_vlan_filter_enable(sc);
}

static void
em_if_intr_enable(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;
	u32 ims_mask = IMS_ENABLE_MASK;

	if (sc->intr_type == IFLIB_INTR_MSIX) {
		E1000_WRITE_REG(hw, EM_EIAC, sc->ims);
		ims_mask |= sc->ims;
	}
	E1000_WRITE_REG(hw, E1000_IMS, ims_mask);
	E1000_WRITE_FLUSH(hw);
}

static void
em_if_intr_disable(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;

	if (sc->intr_type == IFLIB_INTR_MSIX)
		E1000_WRITE_REG(hw, EM_EIAC, 0);
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	E1000_WRITE_FLUSH(hw);
}

static void
igb_if_intr_enable(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;
	u32 mask;

	if (__predict_true(sc->intr_type == IFLIB_INTR_MSIX)) {
		mask = (sc->que_mask | sc->link_mask);
		E1000_WRITE_REG(hw, E1000_EIAC, mask);
		E1000_WRITE_REG(hw, E1000_EIAM, mask);
		E1000_WRITE_REG(hw, E1000_EIMS, mask);
		E1000_WRITE_REG(hw, E1000_IMS, E1000_IMS_LSC);
	} else
		E1000_WRITE_REG(hw, E1000_IMS, IMS_ENABLE_MASK);
	E1000_WRITE_FLUSH(hw);
}

static void
igb_if_intr_disable(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;

	if (__predict_true(sc->intr_type == IFLIB_INTR_MSIX)) {
		E1000_WRITE_REG(hw, E1000_EIMC, 0xffffffff);
		E1000_WRITE_REG(hw, E1000_EIAC, 0);
	}
	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	E1000_WRITE_FLUSH(hw);
}

/*
 * Bit of a misnomer, what this really means is
 * to enable OS management of the system... aka
 * to disable special hardware management features
 */
static void
em_init_manageability(struct e1000_softc *sc)
{
	/* A shared code workaround */
#define E1000_82542_MANC2H E1000_MANC2H
	if (sc->has_manage) {
		int manc2h = E1000_READ_REG(&sc->hw, E1000_MANC2H);
		int manc = E1000_READ_REG(&sc->hw, E1000_MANC);

		/* disable hardware interception of ARP */
		manc &= ~(E1000_MANC_ARP_EN);

		/* enable receiving management packets to the host */
		manc |= E1000_MANC_EN_MNG2HOST;
#define E1000_MNG2HOST_PORT_623 (1 << 5)
#define E1000_MNG2HOST_PORT_664 (1 << 6)
		manc2h |= E1000_MNG2HOST_PORT_623;
		manc2h |= E1000_MNG2HOST_PORT_664;
		E1000_WRITE_REG(&sc->hw, E1000_MANC2H, manc2h);
		E1000_WRITE_REG(&sc->hw, E1000_MANC, manc);
	}
}

/*
 * Give control back to hardware management
 * controller if there is one.
 */
static void
em_release_manageability(struct e1000_softc *sc)
{
	if (sc->has_manage) {
		int manc = E1000_READ_REG(&sc->hw, E1000_MANC);

		/* re-enable hardware interception of ARP */
		manc |= E1000_MANC_ARP_EN;
		manc &= ~E1000_MANC_EN_MNG2HOST;

		E1000_WRITE_REG(&sc->hw, E1000_MANC, manc);
	}
}

/*
 * em_get_hw_control sets the {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded. For AMT version type f/w
 * this means that the network i/f is open.
 */
static void
em_get_hw_control(struct e1000_softc *sc)
{
	u32 ctrl_ext, swsm;

	if (sc->vf_ifp)
		return;

	if (sc->hw.mac.type == e1000_82573) {
		swsm = E1000_READ_REG(&sc->hw, E1000_SWSM);
		E1000_WRITE_REG(&sc->hw, E1000_SWSM,
		    swsm | E1000_SWSM_DRV_LOAD);
		return;
	}
	/* else */
	ctrl_ext = E1000_READ_REG(&sc->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&sc->hw, E1000_CTRL_EXT,
	    ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
}

/*
 * em_release_hw_control resets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is no longer loaded. For AMT versions of the
 * f/w this means that the network i/f is closed.
 */
static void
em_release_hw_control(struct e1000_softc *sc)
{
	u32 ctrl_ext, swsm;

	if (!sc->has_manage)
		return;

	if (sc->hw.mac.type == e1000_82573) {
		swsm = E1000_READ_REG(&sc->hw, E1000_SWSM);
		E1000_WRITE_REG(&sc->hw, E1000_SWSM,
		    swsm & ~E1000_SWSM_DRV_LOAD);
		return;
	}
	/* else */
	ctrl_ext = E1000_READ_REG(&sc->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&sc->hw, E1000_CTRL_EXT,
	    ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
	return;
}

static int
em_is_valid_ether_addr(u8 *addr)
{
	char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN))) {
		return (false);
	}

	return (true);
}

static bool
em_automask_tso(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = iflib_get_softc_ctx(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	if (!em_unsupported_tso && sc->link_speed &&
	    sc->link_speed != SPEED_1000 && scctx->isc_capenable & IFCAP_TSO) {
		device_printf(sc->dev, "Disabling TSO for 10/100 Ethernet.\n");
		sc->tso_automasked = scctx->isc_capenable & IFCAP_TSO;
		scctx->isc_capenable &= ~IFCAP_TSO;
		if_setcapenablebit(ifp, 0, IFCAP_TSO);
		/* iflib_init_locked handles ifnet hwassistbits */
		iflib_request_reset(ctx);
		return true;
	} else if (sc->link_speed == SPEED_1000 && sc->tso_automasked) {
		device_printf(sc->dev, "Re-enabling TSO for GbE.\n");
		scctx->isc_capenable |= sc->tso_automasked;
		if_setcapenablebit(ifp, sc->tso_automasked, 0);
		sc->tso_automasked = 0;
		/* iflib_init_locked handles ifnet hwassistbits */
		iflib_request_reset(ctx);
		return true;
	}

	return false;
}

/*
** Parse the interface capabilities with regard
** to both system management and wake-on-lan for
** later use.
*/
static void
em_get_wakeup(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	u16 eeprom_data = 0, device_id, apme_mask;

	sc->has_manage = e1000_enable_mng_pass_thru(&sc->hw);
	apme_mask = EM_EEPROM_APME;

	switch (sc->hw.mac.type) {
	case e1000_82542:
	case e1000_82543:
		break;
	case e1000_82544:
		e1000_read_nvm(&sc->hw,
		    NVM_INIT_CONTROL2_REG, 1, &eeprom_data);
		apme_mask = EM_82544_APME;
		break;
	case e1000_82546:
	case e1000_82546_rev_3:
		if (sc->hw.bus.func == 1) {
			e1000_read_nvm(&sc->hw,
			    NVM_INIT_CONTROL3_PORT_B, 1, &eeprom_data);
			break;
		} else
			e1000_read_nvm(&sc->hw,
			    NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
		break;
	case e1000_82573:
	case e1000_82583:
		sc->has_amt = true;
		/* FALLTHROUGH */
	case e1000_82571:
	case e1000_82572:
	case e1000_80003es2lan:
		if (sc->hw.bus.func == 1) {
			e1000_read_nvm(&sc->hw,
			    NVM_INIT_CONTROL3_PORT_B, 1, &eeprom_data);
			break;
		} else
			e1000_read_nvm(&sc->hw,
			    NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
		break;
	case e1000_ich8lan:
	case e1000_ich9lan:
	case e1000_ich10lan:
	case e1000_pchlan:
	case e1000_pch2lan:
	case e1000_pch_lpt:
	case e1000_pch_spt:
	case e1000_82575:	/* listing all igb devices */
	case e1000_82576:
	case e1000_82580:
	case e1000_i350:
	case e1000_i354:
	case e1000_i210:
	case e1000_i211:
	case e1000_vfadapt:
	case e1000_vfadapt_i350:
		apme_mask = E1000_WUC_APME;
		sc->has_amt = true;
		eeprom_data = E1000_READ_REG(&sc->hw, E1000_WUC);
		break;
	default:
		e1000_read_nvm(&sc->hw,
		    NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
		break;
	}
	if (eeprom_data & apme_mask)
		sc->wol = (E1000_WUFC_MAG | E1000_WUFC_MC);
	/*
	 * We have the eeprom settings, now apply the special cases
	 * where the eeprom may be wrong or the board won't support
	 * wake on lan on a particular port
	 */
	device_id = pci_get_device(dev);
	switch (device_id) {
	case E1000_DEV_ID_82546GB_PCIE:
		sc->wol = 0;
		break;
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546GB_FIBER:
		/* Wake events only supported on port A for dual fiber
		 * regardless of eeprom setting */
		if (E1000_READ_REG(&sc->hw, E1000_STATUS) &
		    E1000_STATUS_FUNC_1)
			sc->wol = 0;
		break;
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
		/* if quad port adapter, disable WoL on all but port A */
		if (global_quad_port_a != 0)
			sc->wol = 0;
		/* Reset for multiple quad port adapters */
		if (++global_quad_port_a == 4)
			global_quad_port_a = 0;
		break;
	case E1000_DEV_ID_82571EB_FIBER:
		/* Wake events only supported on port A for dual fiber
		 * regardless of eeprom setting */
		if (E1000_READ_REG(&sc->hw, E1000_STATUS) &
		    E1000_STATUS_FUNC_1)
			sc->wol = 0;
		break;
	case E1000_DEV_ID_82571EB_QUAD_COPPER:
	case E1000_DEV_ID_82571EB_QUAD_FIBER:
	case E1000_DEV_ID_82571EB_QUAD_COPPER_LP:
		/* if quad port adapter, disable WoL on all but port A */
		if (global_quad_port_a != 0)
			sc->wol = 0;
		/* Reset for multiple quad port adapters */
		if (++global_quad_port_a == 4)
			global_quad_port_a = 0;
		break;
	}
	return;
}


/*
 * Enable PCI Wake On Lan capability
 */
static void
em_enable_wakeup(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int error = 0;
	u32 pmc, ctrl, ctrl_ext, rctl;
	u16 status;

	if (pci_find_cap(dev, PCIY_PMG, &pmc) != 0)
		return;

	/*
	 * Determine type of Wakeup: note that wol
	 * is set with all bits on by default.
	 */
	if ((if_getcapenable(ifp) & IFCAP_WOL_MAGIC) == 0)
		sc->wol &= ~E1000_WUFC_MAG;

	if ((if_getcapenable(ifp) & IFCAP_WOL_UCAST) == 0)
		sc->wol &= ~E1000_WUFC_EX;

	if ((if_getcapenable(ifp) & IFCAP_WOL_MCAST) == 0)
		sc->wol &= ~E1000_WUFC_MC;
	else {
		rctl = E1000_READ_REG(&sc->hw, E1000_RCTL);
		rctl |= E1000_RCTL_MPE;
		E1000_WRITE_REG(&sc->hw, E1000_RCTL, rctl);
	}

	if (!(sc->wol & (E1000_WUFC_EX | E1000_WUFC_MAG | E1000_WUFC_MC)))
		goto pme;

	/* Advertise the wakeup capability */
	ctrl = E1000_READ_REG(&sc->hw, E1000_CTRL);
	ctrl |= (E1000_CTRL_SWDPIN2 | E1000_CTRL_SWDPIN3);
	E1000_WRITE_REG(&sc->hw, E1000_CTRL, ctrl);

	/* Keep the laser running on Fiber adapters */
	if (sc->hw.phy.media_type == e1000_media_type_fiber ||
	    sc->hw.phy.media_type == e1000_media_type_internal_serdes) {
		ctrl_ext = E1000_READ_REG(&sc->hw, E1000_CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_SDP3_DATA;
		E1000_WRITE_REG(&sc->hw, E1000_CTRL_EXT, ctrl_ext);
	}

	if ((sc->hw.mac.type == e1000_ich8lan) ||
	    (sc->hw.mac.type == e1000_pchlan) ||
	    (sc->hw.mac.type == e1000_ich9lan) ||
	    (sc->hw.mac.type == e1000_ich10lan))
		e1000_suspend_workarounds_ich8lan(&sc->hw);

	if ( sc->hw.mac.type >= e1000_pchlan) {
		error = em_enable_phy_wakeup(sc);
		if (error)
			goto pme;
	} else {
		/* Enable wakeup by the MAC */
		E1000_WRITE_REG(&sc->hw, E1000_WUC, E1000_WUC_PME_EN);
		E1000_WRITE_REG(&sc->hw, E1000_WUFC, sc->wol);
	}

	if (sc->hw.phy.type == e1000_phy_igp_3)
		e1000_igp3_phy_powerdown_workaround_ich8lan(&sc->hw);

pme:
	status = pci_read_config(dev, pmc + PCIR_POWER_STATUS, 2);
	status &= ~(PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE);
	if (!error && (if_getcapenable(ifp) & IFCAP_WOL))
		status |= PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE;
	pci_write_config(dev, pmc + PCIR_POWER_STATUS, status, 2);

	return;
}

/*
 * WOL in the newer chipset interfaces (pchlan)
 * require thing to be copied into the phy
 */
static int
em_enable_phy_wakeup(struct e1000_softc *sc)
{
	struct e1000_hw *hw = &sc->hw;
	u32 mreg, ret = 0;
	u16 preg;

	/* copy MAC RARs to PHY RARs */
	e1000_copy_rx_addrs_to_phy_ich8lan(hw);

	/* copy MAC MTA to PHY MTA */
	for (int i = 0; i < hw->mac.mta_reg_count; i++) {
		mreg = E1000_READ_REG_ARRAY(hw, E1000_MTA, i);
		e1000_write_phy_reg(hw, BM_MTA(i), (u16)(mreg & 0xFFFF));
		e1000_write_phy_reg(hw, BM_MTA(i) + 1,
		    (u16)((mreg >> 16) & 0xFFFF));
	}

	/* configure PHY Rx Control register */
	e1000_read_phy_reg(hw, BM_RCTL, &preg);
	mreg = E1000_READ_REG(hw, E1000_RCTL);
	if (mreg & E1000_RCTL_UPE)
		preg |= BM_RCTL_UPE;
	if (mreg & E1000_RCTL_MPE)
		preg |= BM_RCTL_MPE;
	preg &= ~(BM_RCTL_MO_MASK);
	if (mreg & E1000_RCTL_MO_3)
		preg |= (((mreg & E1000_RCTL_MO_3) >> E1000_RCTL_MO_SHIFT)
				<< BM_RCTL_MO_SHIFT);
	if (mreg & E1000_RCTL_BAM)
		preg |= BM_RCTL_BAM;
	if (mreg & E1000_RCTL_PMCF)
		preg |= BM_RCTL_PMCF;
	mreg = E1000_READ_REG(hw, E1000_CTRL);
	if (mreg & E1000_CTRL_RFCE)
		preg |= BM_RCTL_RFCE;
	e1000_write_phy_reg(hw, BM_RCTL, preg);

	/* enable PHY wakeup in MAC register */
	E1000_WRITE_REG(hw, E1000_WUC,
	    E1000_WUC_PHY_WAKE | E1000_WUC_PME_EN | E1000_WUC_APME);
	E1000_WRITE_REG(hw, E1000_WUFC, sc->wol);

	/* configure and enable PHY wakeup in PHY registers */
	e1000_write_phy_reg(hw, BM_WUFC, sc->wol);
	e1000_write_phy_reg(hw, BM_WUC, E1000_WUC_PME_EN);

	/* activate PHY wakeup */
	ret = hw->phy.ops.acquire(hw);
	if (ret) {
		printf("Could not acquire PHY\n");
		return ret;
	}
	e1000_write_phy_reg_mdic(hw, IGP01E1000_PHY_PAGE_SELECT,
	                         (BM_WUC_ENABLE_PAGE << IGP_PAGE_SHIFT));
	ret = e1000_read_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, &preg);
	if (ret) {
		printf("Could not read PHY page 769\n");
		goto out;
	}
	preg |= BM_WUC_ENABLE_BIT | BM_WUC_HOST_WU_BIT;
	ret = e1000_write_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, preg);
	if (ret)
		printf("Could not set PHY Host Wakeup bit\n");
out:
	hw->phy.ops.release(hw);

	return ret;
}

static void
em_if_led_func(if_ctx_t ctx, int onoff)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);

	if (onoff) {
		e1000_setup_led(&sc->hw);
		e1000_led_on(&sc->hw);
	} else {
		e1000_led_off(&sc->hw);
		e1000_cleanup_led(&sc->hw);
	}
}

/*
 * Disable the L0S and L1 LINK states
 */
static void
em_disable_aspm(struct e1000_softc *sc)
{
	int base, reg;
	u16 link_cap,link_ctrl;
	device_t dev = sc->dev;

	switch (sc->hw.mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		break;
	default:
		return;
	}
	if (pci_find_cap(dev, PCIY_EXPRESS, &base) != 0)
		return;
	reg = base + PCIER_LINK_CAP;
	link_cap = pci_read_config(dev, reg, 2);
	if ((link_cap & PCIEM_LINK_CAP_ASPM) == 0)
		return;
	reg = base + PCIER_LINK_CTL;
	link_ctrl = pci_read_config(dev, reg, 2);
	link_ctrl &= ~PCIEM_LINK_CTL_ASPMC;
	pci_write_config(dev, reg, link_ctrl, 2);
	return;
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
em_update_stats_counters(struct e1000_softc *sc)
{
	u64 prev_xoffrxc = sc->stats.xoffrxc;

	if(sc->hw.phy.media_type == e1000_media_type_copper ||
	   (E1000_READ_REG(&sc->hw, E1000_STATUS) & E1000_STATUS_LU)) {
		sc->stats.symerrs += E1000_READ_REG(&sc->hw, E1000_SYMERRS);
		sc->stats.sec += E1000_READ_REG(&sc->hw, E1000_SEC);
	}
	sc->stats.crcerrs += E1000_READ_REG(&sc->hw, E1000_CRCERRS);
	sc->stats.mpc += E1000_READ_REG(&sc->hw, E1000_MPC);
	sc->stats.scc += E1000_READ_REG(&sc->hw, E1000_SCC);
	sc->stats.ecol += E1000_READ_REG(&sc->hw, E1000_ECOL);

	sc->stats.mcc += E1000_READ_REG(&sc->hw, E1000_MCC);
	sc->stats.latecol += E1000_READ_REG(&sc->hw, E1000_LATECOL);
	sc->stats.colc += E1000_READ_REG(&sc->hw, E1000_COLC);
	sc->stats.dc += E1000_READ_REG(&sc->hw, E1000_DC);
	sc->stats.rlec += E1000_READ_REG(&sc->hw, E1000_RLEC);
	sc->stats.xonrxc += E1000_READ_REG(&sc->hw, E1000_XONRXC);
	sc->stats.xontxc += E1000_READ_REG(&sc->hw, E1000_XONTXC);
	sc->stats.xoffrxc += E1000_READ_REG(&sc->hw, E1000_XOFFRXC);
	/*
	 ** For watchdog management we need to know if we have been
	 ** paused during the last interval, so capture that here.
	*/
	if (sc->stats.xoffrxc != prev_xoffrxc)
		sc->shared->isc_pause_frames = 1;
	sc->stats.xofftxc += E1000_READ_REG(&sc->hw, E1000_XOFFTXC);
	sc->stats.fcruc += E1000_READ_REG(&sc->hw, E1000_FCRUC);
	sc->stats.prc64 += E1000_READ_REG(&sc->hw, E1000_PRC64);
	sc->stats.prc127 += E1000_READ_REG(&sc->hw, E1000_PRC127);
	sc->stats.prc255 += E1000_READ_REG(&sc->hw, E1000_PRC255);
	sc->stats.prc511 += E1000_READ_REG(&sc->hw, E1000_PRC511);
	sc->stats.prc1023 += E1000_READ_REG(&sc->hw, E1000_PRC1023);
	sc->stats.prc1522 += E1000_READ_REG(&sc->hw, E1000_PRC1522);
	sc->stats.gprc += E1000_READ_REG(&sc->hw, E1000_GPRC);
	sc->stats.bprc += E1000_READ_REG(&sc->hw, E1000_BPRC);
	sc->stats.mprc += E1000_READ_REG(&sc->hw, E1000_MPRC);
	sc->stats.gptc += E1000_READ_REG(&sc->hw, E1000_GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	sc->stats.gorc += E1000_READ_REG(&sc->hw, E1000_GORCL) +
	    ((u64)E1000_READ_REG(&sc->hw, E1000_GORCH) << 32);
	sc->stats.gotc += E1000_READ_REG(&sc->hw, E1000_GOTCL) +
	    ((u64)E1000_READ_REG(&sc->hw, E1000_GOTCH) << 32);

	sc->stats.rnbc += E1000_READ_REG(&sc->hw, E1000_RNBC);
	sc->stats.ruc += E1000_READ_REG(&sc->hw, E1000_RUC);
	sc->stats.rfc += E1000_READ_REG(&sc->hw, E1000_RFC);
	sc->stats.roc += E1000_READ_REG(&sc->hw, E1000_ROC);
	sc->stats.rjc += E1000_READ_REG(&sc->hw, E1000_RJC);

	sc->stats.tor += E1000_READ_REG(&sc->hw, E1000_TORH);
	sc->stats.tot += E1000_READ_REG(&sc->hw, E1000_TOTH);

	sc->stats.tpr += E1000_READ_REG(&sc->hw, E1000_TPR);
	sc->stats.tpt += E1000_READ_REG(&sc->hw, E1000_TPT);
	sc->stats.ptc64 += E1000_READ_REG(&sc->hw, E1000_PTC64);
	sc->stats.ptc127 += E1000_READ_REG(&sc->hw, E1000_PTC127);
	sc->stats.ptc255 += E1000_READ_REG(&sc->hw, E1000_PTC255);
	sc->stats.ptc511 += E1000_READ_REG(&sc->hw, E1000_PTC511);
	sc->stats.ptc1023 += E1000_READ_REG(&sc->hw, E1000_PTC1023);
	sc->stats.ptc1522 += E1000_READ_REG(&sc->hw, E1000_PTC1522);
	sc->stats.mptc += E1000_READ_REG(&sc->hw, E1000_MPTC);
	sc->stats.bptc += E1000_READ_REG(&sc->hw, E1000_BPTC);

	/* Interrupt Counts */

	sc->stats.iac += E1000_READ_REG(&sc->hw, E1000_IAC);
	sc->stats.icrxptc += E1000_READ_REG(&sc->hw, E1000_ICRXPTC);
	sc->stats.icrxatc += E1000_READ_REG(&sc->hw, E1000_ICRXATC);
	sc->stats.ictxptc += E1000_READ_REG(&sc->hw, E1000_ICTXPTC);
	sc->stats.ictxatc += E1000_READ_REG(&sc->hw, E1000_ICTXATC);
	sc->stats.ictxqec += E1000_READ_REG(&sc->hw, E1000_ICTXQEC);
	sc->stats.ictxqmtc += E1000_READ_REG(&sc->hw, E1000_ICTXQMTC);
	sc->stats.icrxdmtc += E1000_READ_REG(&sc->hw, E1000_ICRXDMTC);
	sc->stats.icrxoc += E1000_READ_REG(&sc->hw, E1000_ICRXOC);

	if (sc->hw.mac.type >= e1000_82543) {
		sc->stats.algnerrc +=
		E1000_READ_REG(&sc->hw, E1000_ALGNERRC);
		sc->stats.rxerrc +=
		E1000_READ_REG(&sc->hw, E1000_RXERRC);
		sc->stats.tncrs +=
		E1000_READ_REG(&sc->hw, E1000_TNCRS);
		sc->stats.cexterr +=
		E1000_READ_REG(&sc->hw, E1000_CEXTERR);
		sc->stats.tsctc +=
		E1000_READ_REG(&sc->hw, E1000_TSCTC);
		sc->stats.tsctfc +=
		E1000_READ_REG(&sc->hw, E1000_TSCTFC);
	}
}

static uint64_t
em_if_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_COLLISIONS:
		return (sc->stats.colc);
	case IFCOUNTER_IERRORS:
		return (sc->dropped_pkts + sc->stats.rxerrc +
		    sc->stats.crcerrs + sc->stats.algnerrc +
		    sc->stats.ruc + sc->stats.roc +
		    sc->stats.mpc + sc->stats.cexterr);
	case IFCOUNTER_OERRORS:
		return (sc->stats.ecol + sc->stats.latecol +
		    sc->watchdog_events);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

/* em_if_needs_restart - Tell iflib when the driver needs to be reinitialized
 * @ctx: iflib context
 * @event: event code to check
 *
 * Defaults to returning true for unknown events.
 *
 * @returns true if iflib needs to reinit the interface
 */
static bool
em_if_needs_restart(if_ctx_t ctx __unused, enum iflib_restart_event event)
{
	switch (event) {
	case IFLIB_RESTART_VLAN_CONFIG:
		return (false);
	default:
		return (true);
	}
}

/* Export a single 32-bit register via a read-only sysctl. */
static int
em_sysctl_reg_handler(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc;
	u_int val;

	sc = oidp->oid_arg1;
	val = E1000_READ_REG(&sc->hw, oidp->oid_arg2);
	return (sysctl_handle_int(oidp, &val, 0, req));
}

/*
 * Add sysctl variables, one per statistic, to the system.
 */
static void
em_add_hw_stats(struct e1000_softc *sc)
{
	device_t dev = iflib_get_dev(sc->ctx);
	struct em_tx_queue *tx_que = sc->tx_queues;
	struct em_rx_queue *rx_que = sc->rx_queues;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct e1000_hw_stats *stats = &sc->stats;

	struct sysctl_oid *stat_node, *queue_node, *int_node;
	struct sysctl_oid_list *stat_list, *queue_list, *int_list;

#define QUEUE_NAME_LEN 32
	char namebuf[QUEUE_NAME_LEN];

	/* Driver Statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "dropped",
			CTLFLAG_RD, &sc->dropped_pkts,
			"Driver dropped packets");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "link_irq",
			CTLFLAG_RD, &sc->link_irq,
			"Link MSI-X IRQ Handled");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "rx_overruns",
			CTLFLAG_RD, &sc->rx_overruns,
			"RX overruns");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_timeouts",
			CTLFLAG_RD, &sc->watchdog_events,
			"Watchdog timeouts");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "device_control",
	    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    sc, E1000_CTRL, em_sysctl_reg_handler, "IU",
	    "Device Control Register");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rx_control",
	    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    sc, E1000_RCTL, em_sysctl_reg_handler, "IU",
	    "Receiver Control Register");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_high_water",
			CTLFLAG_RD, &sc->hw.fc.high_water, 0,
			"Flow Control High Watermark");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_low_water",
			CTLFLAG_RD, &sc->hw.fc.low_water, 0,
			"Flow Control Low Watermark");

	for (int i = 0; i < sc->tx_num_queues; i++, tx_que++) {
		struct tx_ring *txr = &tx_que->txr;
		snprintf(namebuf, QUEUE_NAME_LEN, "queue_tx_%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "TX Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_head",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    E1000_TDH(txr->me), em_sysctl_reg_handler, "IU",
		    "Transmit Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_tail",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    E1000_TDT(txr->me), em_sysctl_reg_handler, "IU",
		    "Transmit Descriptor Tail");
		SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO, "tx_irq",
				CTLFLAG_RD, &txr->tx_irq,
				"Queue MSI-X Transmit Interrupts");
	}

	for (int j = 0; j < sc->rx_num_queues; j++, rx_que++) {
		struct rx_ring *rxr = &rx_que->rxr;
		snprintf(namebuf, QUEUE_NAME_LEN, "queue_rx_%d", j);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "RX Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_head",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    E1000_RDH(rxr->me), em_sysctl_reg_handler, "IU",
		    "Receive Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_tail",
		    CTLTYPE_UINT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, sc,
		    E1000_RDT(rxr->me), em_sysctl_reg_handler, "IU",
		    "Receive Descriptor Tail");
		SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO, "rx_irq",
				CTLFLAG_RD, &rxr->rx_irq,
				"Queue MSI-X Receive Interrupts");
	}

	/* MAC stats get their own sub node */

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac_stats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Statistics");
	stat_list = SYSCTL_CHILDREN(stat_node);

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "excess_coll",
			CTLFLAG_RD, &stats->ecol,
			"Excessive collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "single_coll",
			CTLFLAG_RD, &stats->scc,
			"Single collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "multiple_coll",
			CTLFLAG_RD, &stats->mcc,
			"Multiple collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "late_coll",
			CTLFLAG_RD, &stats->latecol,
			"Late collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "collision_count",
			CTLFLAG_RD, &stats->colc,
			"Collision Count");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "symbol_errors",
			CTLFLAG_RD, &sc->stats.symerrs,
			"Symbol Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "sequence_errors",
			CTLFLAG_RD, &sc->stats.sec,
			"Sequence Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "defer_count",
			CTLFLAG_RD, &sc->stats.dc,
			"Defer Count");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "missed_packets",
			CTLFLAG_RD, &sc->stats.mpc,
			"Missed Packets");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_no_buff",
			CTLFLAG_RD, &sc->stats.rnbc,
			"Receive No Buffers");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_undersize",
			CTLFLAG_RD, &sc->stats.ruc,
			"Receive Undersize");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_fragmented",
			CTLFLAG_RD, &sc->stats.rfc,
			"Fragmented Packets Received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_oversize",
			CTLFLAG_RD, &sc->stats.roc,
			"Oversized Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_jabber",
			CTLFLAG_RD, &sc->stats.rjc,
			"Recevied Jabber");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_errs",
			CTLFLAG_RD, &sc->stats.rxerrc,
			"Receive Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "crc_errs",
			CTLFLAG_RD, &sc->stats.crcerrs,
			"CRC errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "alignment_errs",
			CTLFLAG_RD, &sc->stats.algnerrc,
			"Alignment Errors");
	/* On 82575 these are collision counts */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "coll_ext_errs",
			CTLFLAG_RD, &sc->stats.cexterr,
			"Collision/Carrier extension errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_recvd",
			CTLFLAG_RD, &sc->stats.xonrxc,
			"XON Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_txd",
			CTLFLAG_RD, &sc->stats.xontxc,
			"XON Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_recvd",
			CTLFLAG_RD, &sc->stats.xoffrxc,
			"XOFF Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_txd",
			CTLFLAG_RD, &sc->stats.xofftxc,
			"XOFF Transmitted");

	/* Packet Reception Stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_recvd",
			CTLFLAG_RD, &sc->stats.tpr,
			"Total Packets Received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_recvd",
			CTLFLAG_RD, &sc->stats.gprc,
			"Good Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_recvd",
			CTLFLAG_RD, &sc->stats.bprc,
			"Broadcast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_recvd",
			CTLFLAG_RD, &sc->stats.mprc,
			"Multicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_64",
			CTLFLAG_RD, &sc->stats.prc64,
			"64 byte frames received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_65_127",
			CTLFLAG_RD, &sc->stats.prc127,
			"65-127 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_128_255",
			CTLFLAG_RD, &sc->stats.prc255,
			"128-255 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_256_511",
			CTLFLAG_RD, &sc->stats.prc511,
			"256-511 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_512_1023",
			CTLFLAG_RD, &sc->stats.prc1023,
			"512-1023 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_1024_1522",
			CTLFLAG_RD, &sc->stats.prc1522,
			"1023-1522 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_recvd",
			CTLFLAG_RD, &sc->stats.gorc,
			"Good Octets Received");

	/* Packet Transmission Stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
			CTLFLAG_RD, &sc->stats.gotc,
			"Good Octets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_txd",
			CTLFLAG_RD, &sc->stats.tpt,
			"Total Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
			CTLFLAG_RD, &sc->stats.gptc,
			"Good Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_txd",
			CTLFLAG_RD, &sc->stats.bptc,
			"Broadcast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_txd",
			CTLFLAG_RD, &sc->stats.mptc,
			"Multicast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_64",
			CTLFLAG_RD, &sc->stats.ptc64,
			"64 byte frames transmitted ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_65_127",
			CTLFLAG_RD, &sc->stats.ptc127,
			"65-127 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_128_255",
			CTLFLAG_RD, &sc->stats.ptc255,
			"128-255 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_256_511",
			CTLFLAG_RD, &sc->stats.ptc511,
			"256-511 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_512_1023",
			CTLFLAG_RD, &sc->stats.ptc1023,
			"512-1023 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_1024_1522",
			CTLFLAG_RD, &sc->stats.ptc1522,
			"1024-1522 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tso_txd",
			CTLFLAG_RD, &sc->stats.tsctc,
			"TSO Contexts Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tso_ctx_fail",
			CTLFLAG_RD, &sc->stats.tsctfc,
			"TSO Contexts Failed");


	/* Interrupt Stats */

	int_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "interrupts",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Interrupt Statistics");
	int_list = SYSCTL_CHILDREN(int_node);

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "asserts",
			CTLFLAG_RD, &sc->stats.iac,
			"Interrupt Assertion Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_pkt_timer",
			CTLFLAG_RD, &sc->stats.icrxptc,
			"Interrupt Cause Rx Pkt Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_abs_timer",
			CTLFLAG_RD, &sc->stats.icrxatc,
			"Interrupt Cause Rx Abs Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_pkt_timer",
			CTLFLAG_RD, &sc->stats.ictxptc,
			"Interrupt Cause Tx Pkt Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_abs_timer",
			CTLFLAG_RD, &sc->stats.ictxatc,
			"Interrupt Cause Tx Abs Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_queue_empty",
			CTLFLAG_RD, &sc->stats.ictxqec,
			"Interrupt Cause Tx Queue Empty Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_queue_min_thresh",
			CTLFLAG_RD, &sc->stats.ictxqmtc,
			"Interrupt Cause Tx Queue Min Thresh Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_desc_min_thresh",
			CTLFLAG_RD, &sc->stats.icrxdmtc,
			"Interrupt Cause Rx Desc Min Thresh Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_overrun",
			CTLFLAG_RD, &sc->stats.icrxoc,
			"Interrupt Cause Receiver Overrun Count");
}

static void
em_fw_version_locked(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;
	struct e1000_fw_version *fw_ver = &sc->fw_ver;
	uint16_t eep = 0;

	/*
	 * em_fw_version_locked() must run under the IFLIB_CTX_LOCK to meet the
	 * NVM locking model, so we do it in em_if_attach_pre() and store the
	 * info in the softc
	 */
	ASSERT_CTX_LOCK_HELD(hw);

	*fw_ver = (struct e1000_fw_version){0};

	if (hw->mac.type >= igb_mac_min) {
		/*
		 * Use the Shared Code for igb(4)
		 */
		e1000_get_fw_version(hw, fw_ver);
	} else {
		/*
		 * Otherwise, EEPROM version should be present on (almost?) all
		 * devices here
		 */
		if(e1000_read_nvm(hw, NVM_VERSION, 1, &eep)) {
			INIT_DEBUGOUT("can't get EEPROM version");
			return;
		}

		fw_ver->eep_major = (eep & NVM_MAJOR_MASK) >> NVM_MAJOR_SHIFT;
		fw_ver->eep_minor = (eep & NVM_MINOR_MASK) >> NVM_MINOR_SHIFT;
		fw_ver->eep_build = (eep & NVM_IMAGE_ID_MASK);
	}
}

static void
em_sbuf_fw_version(struct e1000_fw_version *fw_ver, struct sbuf *buf)
{
	const char *space = "";

	if (fw_ver->eep_major || fw_ver->eep_minor || fw_ver->eep_build) {
		sbuf_printf(buf, "EEPROM V%d.%d-%d", fw_ver->eep_major,
			    fw_ver->eep_minor, fw_ver->eep_build);
		space = " ";
	}

	if (fw_ver->invm_major || fw_ver->invm_minor || fw_ver->invm_img_type) {
		sbuf_printf(buf, "%sNVM V%d.%d imgtype%d",
			    space, fw_ver->invm_major, fw_ver->invm_minor,
			    fw_ver->invm_img_type);
		space = " ";
	}

	if (fw_ver->or_valid) {
		sbuf_printf(buf, "%sOption ROM V%d-b%d-p%d",
			    space, fw_ver->or_major, fw_ver->or_build,
			    fw_ver->or_patch);
		space = " ";
	}

	if (fw_ver->etrack_id)
		sbuf_printf(buf, "%seTrack 0x%08x", space, fw_ver->etrack_id);
}

static void
em_print_fw_version(struct e1000_softc *sc )
{
	device_t dev = sc->dev;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_auto();
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return;
	}

	em_sbuf_fw_version(&sc->fw_ver, buf);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	else if (sbuf_len(buf))
		device_printf(dev, "%s\n", sbuf_data(buf));

	sbuf_delete(buf);
}

static int
em_sysctl_print_fw_version(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc = (struct e1000_softc *)arg1;
	device_t dev = sc->dev;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	em_sbuf_fw_version(&sc->fw_ver, buf);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);

	return (0);
}

/**********************************************************************
 *
 *  This routine provides a way to dump out the adapter eeprom,
 *  often a useful debug/service tool. This only dumps the first
 *  32 words, stuff that matters is in that extent.
 *
 **********************************************************************/
static int
em_sysctl_nvm_info(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc = (struct e1000_softc *)arg1;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	/*
	 * This value will cause a hex dump of the
	 * first 32 16-bit words of the EEPROM to
	 * the screen.
	 */
	if (result == 1)
		em_print_nvm_info(sc);

	return (error);
}

static void
em_print_nvm_info(struct e1000_softc *sc)
{
	struct e1000_hw *hw = &sc->hw;
	struct sx *iflib_ctx_lock = iflib_ctx_lock_get(sc->ctx);
	u16 eeprom_data;
	int i, j, row = 0;

	/* Its a bit crude, but it gets the job done */
	printf("\nInterface EEPROM Dump:\n");
	printf("Offset\n0x0000  ");

	/* We rely on the IFLIB_CTX_LOCK as part of NVM locking model */
	sx_xlock(iflib_ctx_lock);
	ASSERT_CTX_LOCK_HELD(hw);
	for (i = 0, j = 0; i < 32; i++, j++) {
		if (j == 8) { /* Make the offset block */
			j = 0; ++row;
			printf("\n0x00%x0  ",row);
		}
		e1000_read_nvm(hw, i, 1, &eeprom_data);
		printf("%04x ", eeprom_data);
	}
	sx_xunlock(iflib_ctx_lock);
	printf("\n");
}

static int
em_sysctl_int_delay(SYSCTL_HANDLER_ARGS)
{
	struct em_int_delay_info *info;
	struct e1000_softc *sc;
	u32 regval;
	int error, usecs, ticks;

	info = (struct em_int_delay_info *) arg1;
	usecs = info->value;
	error = sysctl_handle_int(oidp, &usecs, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (usecs < 0 || usecs > EM_TICKS_TO_USECS(65535))
		return (EINVAL);
	info->value = usecs;
	ticks = EM_USECS_TO_TICKS(usecs);
	if (info->offset == E1000_ITR)	/* units are 256ns here */
		ticks *= 4;

	sc = info->sc;

	regval = E1000_READ_OFFSET(&sc->hw, info->offset);
	regval = (regval & ~0xffff) | (ticks & 0xffff);
	/* Handle a few special cases. */
	switch (info->offset) {
	case E1000_RDTR:
		break;
	case E1000_TIDV:
		if (ticks == 0) {
			sc->txd_cmd &= ~E1000_TXD_CMD_IDE;
			/* Don't write 0 into the TIDV register. */
			regval++;
		} else
			sc->txd_cmd |= E1000_TXD_CMD_IDE;
		break;
	}
	E1000_WRITE_OFFSET(&sc->hw, info->offset, regval);
	return (0);
}

static void
em_add_int_delay_sysctl(struct e1000_softc *sc, const char *name,
	const char *description, struct em_int_delay_info *info,
	int offset, int value)
{
	info->sc = sc;
	info->offset = offset;
	info->value = value;
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(sc->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
	    OID_AUTO, name, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    info, 0, em_sysctl_int_delay, "I", description);
}

/*
 * Set flow control using sysctl:
 * Flow control values:
 *      0 - off
 *      1 - rx pause
 *      2 - tx pause
 *      3 - full
 */
static int
em_set_flowcntl(SYSCTL_HANDLER_ARGS)
{
	int error;
	static int input = 3; /* default is full */
	struct e1000_softc	*sc = (struct e1000_softc *) arg1;

	error = sysctl_handle_int(oidp, &input, 0, req);

	if ((error) || (req->newptr == NULL))
		return (error);

	if (input == sc->fc) /* no change? */
		return (error);

	switch (input) {
	case e1000_fc_rx_pause:
	case e1000_fc_tx_pause:
	case e1000_fc_full:
	case e1000_fc_none:
		sc->hw.fc.requested_mode = input;
		sc->fc = input;
		break;
	default:
		/* Do nothing */
		return (error);
	}

	sc->hw.fc.current_mode = sc->hw.fc.requested_mode;
	e1000_force_mac_fc(&sc->hw);
	return (error);
}

/*
 * Manage Energy Efficient Ethernet:
 * Control values:
 *     0/1 - enabled/disabled
 */
static int
em_sysctl_eee(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc = (struct e1000_softc *) arg1;
	int error, value;

	value = sc->hw.dev_spec.ich8lan.eee_disable;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	sc->hw.dev_spec.ich8lan.eee_disable = (value != 0);
	em_if_init(sc->ctx);

	return (0);
}

static int
em_sysctl_debug_info(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct e1000_softc *) arg1;
		em_print_debug_info(sc);
	}

	return (error);
}

static int
em_get_rs(SYSCTL_HANDLER_ARGS)
{
	struct e1000_softc *sc = (struct e1000_softc *) arg1;
	int error;
	int result;

	result = 0;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr || result != 1)
		return (error);
	em_dump_rs(sc);

	return (error);
}

static void
em_if_debug(if_ctx_t ctx)
{
	em_dump_rs(iflib_get_softc(ctx));
}

/*
 * This routine is meant to be fluid, add whatever is
 * needed for debugging a problem.  -jfv
 */
static void
em_print_debug_info(struct e1000_softc *sc)
{
	device_t dev = iflib_get_dev(sc->ctx);
	if_t ifp = iflib_get_ifp(sc->ctx);
	struct tx_ring *txr = &sc->tx_queues->txr;
	struct rx_ring *rxr = &sc->rx_queues->rxr;

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		printf("Interface is RUNNING ");
	else
		printf("Interface is NOT RUNNING\n");

	if (if_getdrvflags(ifp) & IFF_DRV_OACTIVE)
		printf("and INACTIVE\n");
	else
		printf("and ACTIVE\n");

	for (int i = 0; i < sc->tx_num_queues; i++, txr++) {
		device_printf(dev, "TX Queue %d ------\n", i);
		device_printf(dev, "hw tdh = %d, hw tdt = %d\n",
			E1000_READ_REG(&sc->hw, E1000_TDH(i)),
			E1000_READ_REG(&sc->hw, E1000_TDT(i)));

	}
	for (int j=0; j < sc->rx_num_queues; j++, rxr++) {
		device_printf(dev, "RX Queue %d ------\n", j);
		device_printf(dev, "hw rdh = %d, hw rdt = %d\n",
			E1000_READ_REG(&sc->hw, E1000_RDH(j)),
			E1000_READ_REG(&sc->hw, E1000_RDT(j)));
	}
}

/*
 * 82574 only:
 * Write a new value to the EEPROM increasing the number of MSI-X
 * vectors from 3 to 5, for proper multiqueue support.
 */
static void
em_enable_vectors_82574(if_ctx_t ctx)
{
	struct e1000_softc *sc = iflib_get_softc(ctx);
	struct e1000_hw *hw = &sc->hw;
	device_t dev = iflib_get_dev(ctx);
	u16 edata;

	e1000_read_nvm(hw, EM_NVM_PCIE_CTRL, 1, &edata);
	if (bootverbose)
		device_printf(dev, "EM_NVM_PCIE_CTRL = %#06x\n", edata);
	if (((edata & EM_NVM_MSIX_N_MASK) >> EM_NVM_MSIX_N_SHIFT) != 4) {
		device_printf(dev, "Writing to eeprom: increasing "
		    "reported MSI-X vectors from 3 to 5...\n");
		edata &= ~(EM_NVM_MSIX_N_MASK);
		edata |= 4 << EM_NVM_MSIX_N_SHIFT;
		e1000_write_nvm(hw, EM_NVM_PCIE_CTRL, 1, &edata);
		e1000_update_nvm_checksum(hw);
		device_printf(dev, "Writing to eeprom: done\n");
	}
}
