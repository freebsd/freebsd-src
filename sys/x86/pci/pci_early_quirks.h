/*-
 * Copyright (c) 2018 Johannes Lundberg
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

#ifndef _PCI_EARLY_QUIRKS_H_
#define	_PCI_EARLY_QUIRKS_H_

/*
 * TODO:
 * Make a common drm/gpu header that both base and out of tree
 * drm modules can use.
 */

#define	PCI_ANY_ID		(-1)
#define	PCI_VENDOR_INTEL	0x8086
#define	PCI_CLASS_VGA		0x0300

#define	INTEL_BSM		0x5c
#define	INTEL_GEN11_BSM_DW0	0xc0
#define	INTEL_GEN11_BSM_DW1	0xc4
#define	INTEL_BSM_MASK		(-(1u << 20))

#define	INTEL_GMCH_CTRL		0x52
#define	INTEL_GMCH_VGA_DISABLE  (1 << 1)
#define	SNB_GMCH_CTRL		0x50
#define	SNB_GMCH_GGMS_SHIFT	8 /* GTT Graphics Memory Size */
#define	SNB_GMCH_GGMS_MASK	0x3
#define	SNB_GMCH_GMS_SHIFT	3 /* Graphics Mode Select */
#define	SNB_GMCH_GMS_MASK	0x1f
#define	BDW_GMCH_GGMS_SHIFT	6
#define	BDW_GMCH_GGMS_MASK	0x3
#define	BDW_GMCH_GMS_SHIFT	8
#define	BDW_GMCH_GMS_MASK	0xff

#define	I830_GMCH_CTRL			0x52
#define	I830_GMCH_GMS_MASK		0x70
#define	I830_GMCH_GMS_LOCAL		0x10
#define	I830_GMCH_GMS_STOLEN_512	0x20
#define	I830_GMCH_GMS_STOLEN_1024	0x30
#define	I830_GMCH_GMS_STOLEN_8192	0x40

#define	I855_GMCH_GMS_MASK		0xF0
#define	I855_GMCH_GMS_STOLEN_0M		0x0
#define	I855_GMCH_GMS_STOLEN_1M		(0x1 << 4)
#define	I855_GMCH_GMS_STOLEN_4M		(0x2 << 4)
#define	I855_GMCH_GMS_STOLEN_8M		(0x3 << 4)
#define	I855_GMCH_GMS_STOLEN_16M	(0x4 << 4)
#define	I855_GMCH_GMS_STOLEN_32M	(0x5 << 4)
#define	I915_GMCH_GMS_STOLEN_48M	(0x6 << 4)
#define	I915_GMCH_GMS_STOLEN_64M	(0x7 << 4)
#define	G33_GMCH_GMS_STOLEN_128M	(0x8 << 4)
#define	G33_GMCH_GMS_STOLEN_256M	(0x9 << 4)
#define	INTEL_GMCH_GMS_STOLEN_96M	(0xa << 4)
#define	INTEL_GMCH_GMS_STOLEN_160M	(0xb << 4)
#define	INTEL_GMCH_GMS_STOLEN_224M	(0xc << 4)
#define	INTEL_GMCH_GMS_STOLEN_352M	(0xd << 4)

#define	INTEL_VGA_DEVICE(id, info) {		\
	0x8086,	id,				\
	info }

#define	INTEL_I810_IDS(info)					\
	INTEL_VGA_DEVICE(0x7121, info), /* I810 */		\
	INTEL_VGA_DEVICE(0x7123, info), /* I810_DC100 */	\
	INTEL_VGA_DEVICE(0x7125, info)  /* I810_E */

#define	INTEL_I815_IDS(info)					\
	INTEL_VGA_DEVICE(0x1132, info)  /* I815*/

#define	INTEL_I830_IDS(info)				\
	INTEL_VGA_DEVICE(0x3577, info)

#define	INTEL_I845G_IDS(info)				\
	INTEL_VGA_DEVICE(0x2562, info)

#define	INTEL_I85X_IDS(info)				\
	INTEL_VGA_DEVICE(0x3582, info), /* I855_GM */ \
	INTEL_VGA_DEVICE(0x358e, info)

#define	INTEL_I865G_IDS(info)				\
	INTEL_VGA_DEVICE(0x2572, info) /* I865_G */

#define	INTEL_I915G_IDS(info)				\
	INTEL_VGA_DEVICE(0x2582, info), /* I915_G */ \
	INTEL_VGA_DEVICE(0x258a, info)  /* E7221_G */

#define	INTEL_I915GM_IDS(info)				\
	INTEL_VGA_DEVICE(0x2592, info) /* I915_GM */

#define	INTEL_I945G_IDS(info)				\
	INTEL_VGA_DEVICE(0x2772, info) /* I945_G */

#define	INTEL_I945GM_IDS(info)				\
	INTEL_VGA_DEVICE(0x27a2, info), /* I945_GM */ \
	INTEL_VGA_DEVICE(0x27ae, info)  /* I945_GME */

#define	INTEL_I965G_IDS(info)				\
	INTEL_VGA_DEVICE(0x2972, info), /* I946_GZ */	\
	INTEL_VGA_DEVICE(0x2982, info),	/* G35_G */	\
	INTEL_VGA_DEVICE(0x2992, info),	/* I965_Q */	\
	INTEL_VGA_DEVICE(0x29a2, info)	/* I965_G */

#define	INTEL_G33_IDS(info)				\
	INTEL_VGA_DEVICE(0x29b2, info), /* Q35_G */ \
	INTEL_VGA_DEVICE(0x29c2, info),	/* G33_G */ \
	INTEL_VGA_DEVICE(0x29d2, info)	/* Q33_G */

#define	INTEL_I965GM_IDS(info)				\
	INTEL_VGA_DEVICE(0x2a02, info),	/* I965_GM */ \
	INTEL_VGA_DEVICE(0x2a12, info)  /* I965_GME */

#define	INTEL_GM45_IDS(info)				\
	INTEL_VGA_DEVICE(0x2a42, info) /* GM45_G */

#define	INTEL_G45_IDS(info)				\
	INTEL_VGA_DEVICE(0x2e02, info), /* IGD_E_G */ \
	INTEL_VGA_DEVICE(0x2e12, info), /* Q45_G */ \
	INTEL_VGA_DEVICE(0x2e22, info), /* G45_G */ \
	INTEL_VGA_DEVICE(0x2e32, info), /* G41_G */ \
	INTEL_VGA_DEVICE(0x2e42, info), /* B43_G */ \
	INTEL_VGA_DEVICE(0x2e92, info)	/* B43_G.1 */

#define	INTEL_PINEVIEW_IDS(info)			\
	INTEL_VGA_DEVICE(0xa001, info),			\
	INTEL_VGA_DEVICE(0xa011, info)

#define	INTEL_IRONLAKE_D_IDS(info) \
	INTEL_VGA_DEVICE(0x0042, info)

#define	INTEL_IRONLAKE_M_IDS(info) \
	INTEL_VGA_DEVICE(0x0046, info)

#define	INTEL_SNB_D_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x0102, info), \
	INTEL_VGA_DEVICE(0x010A, info)

#define	INTEL_SNB_D_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x0112, info), \
	INTEL_VGA_DEVICE(0x0122, info)

#define	INTEL_SNB_D_IDS(info) \
	INTEL_SNB_D_GT1_IDS(info), \
	INTEL_SNB_D_GT2_IDS(info)

#define	INTEL_SNB_M_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x0106, info)

#define	INTEL_SNB_M_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x0116, info), \
	INTEL_VGA_DEVICE(0x0126, info)

#define	INTEL_SNB_M_IDS(info) \
	INTEL_SNB_M_GT1_IDS(info), \
	INTEL_SNB_M_GT2_IDS(info)

#define	INTEL_IVB_M_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x0156, info) /* GT1 mobile */

#define	INTEL_IVB_M_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x0166, info) /* GT2 mobile */

#define	INTEL_IVB_M_IDS(info) \
	INTEL_IVB_M_GT1_IDS(info), \
	INTEL_IVB_M_GT2_IDS(info)

#define	INTEL_IVB_D_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x0152, info), /* GT1 desktop */ \
	INTEL_VGA_DEVICE(0x015a, info)  /* GT1 server */

#define	INTEL_IVB_D_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x0162, info), /* GT2 desktop */ \
	INTEL_VGA_DEVICE(0x016a, info)  /* GT2 server */

#define	INTEL_IVB_D_IDS(info) \
	INTEL_IVB_D_GT1_IDS(info), \
	INTEL_IVB_D_GT2_IDS(info)

#define	INTEL_IVB_Q_IDS(info) \
	INTEL_QUANTA_VGA_DEVICE(info) /* Quanta transcode */

#define	INTEL_HSW_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x0402, info), /* GT1 desktop */ \
	INTEL_VGA_DEVICE(0x040a, info), /* GT1 server */ \
	INTEL_VGA_DEVICE(0x040B, info), /* GT1 reserved */ \
	INTEL_VGA_DEVICE(0x040E, info), /* GT1 reserved */ \
	INTEL_VGA_DEVICE(0x0C02, info), /* SDV GT1 desktop */ \
	INTEL_VGA_DEVICE(0x0C0A, info), /* SDV GT1 server */ \
	INTEL_VGA_DEVICE(0x0C0B, info), /* SDV GT1 reserved */ \
	INTEL_VGA_DEVICE(0x0C0E, info), /* SDV GT1 reserved */ \
	INTEL_VGA_DEVICE(0x0A02, info), /* ULT GT1 desktop */ \
	INTEL_VGA_DEVICE(0x0A0A, info), /* ULT GT1 server */ \
	INTEL_VGA_DEVICE(0x0A0B, info), /* ULT GT1 reserved */ \
	INTEL_VGA_DEVICE(0x0D02, info), /* CRW GT1 desktop */ \
	INTEL_VGA_DEVICE(0x0D0A, info), /* CRW GT1 server */ \
	INTEL_VGA_DEVICE(0x0D0B, info), /* CRW GT1 reserved */ \
	INTEL_VGA_DEVICE(0x0D0E, info), /* CRW GT1 reserved */ \
	INTEL_VGA_DEVICE(0x0406, info), /* GT1 mobile */ \
	INTEL_VGA_DEVICE(0x0C06, info), /* SDV GT1 mobile */ \
	INTEL_VGA_DEVICE(0x0A06, info), /* ULT GT1 mobile */ \
	INTEL_VGA_DEVICE(0x0A0E, info), /* ULX GT1 mobile */ \
	INTEL_VGA_DEVICE(0x0D06, info)  /* CRW GT1 mobile */

#define	INTEL_HSW_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x0412, info), /* GT2 desktop */ \
	INTEL_VGA_DEVICE(0x041a, info), /* GT2 server */ \
	INTEL_VGA_DEVICE(0x041B, info), /* GT2 reserved */ \
	INTEL_VGA_DEVICE(0x041E, info), /* GT2 reserved */ \
	INTEL_VGA_DEVICE(0x0C12, info), /* SDV GT2 desktop */ \
	INTEL_VGA_DEVICE(0x0C1A, info), /* SDV GT2 server */ \
	INTEL_VGA_DEVICE(0x0C1B, info), /* SDV GT2 reserved */ \
	INTEL_VGA_DEVICE(0x0C1E, info), /* SDV GT2 reserved */ \
	INTEL_VGA_DEVICE(0x0A12, info), /* ULT GT2 desktop */ \
	INTEL_VGA_DEVICE(0x0A1A, info), /* ULT GT2 server */ \
	INTEL_VGA_DEVICE(0x0A1B, info), /* ULT GT2 reserved */ \
	INTEL_VGA_DEVICE(0x0D12, info), /* CRW GT2 desktop */ \
	INTEL_VGA_DEVICE(0x0D1A, info), /* CRW GT2 server */ \
	INTEL_VGA_DEVICE(0x0D1B, info), /* CRW GT2 reserved */ \
	INTEL_VGA_DEVICE(0x0D1E, info), /* CRW GT2 reserved */ \
	INTEL_VGA_DEVICE(0x0416, info), /* GT2 mobile */ \
	INTEL_VGA_DEVICE(0x0426, info), /* GT2 mobile */ \
	INTEL_VGA_DEVICE(0x0C16, info), /* SDV GT2 mobile */ \
	INTEL_VGA_DEVICE(0x0A16, info), /* ULT GT2 mobile */ \
	INTEL_VGA_DEVICE(0x0A1E, info), /* ULX GT2 mobile */ \
	INTEL_VGA_DEVICE(0x0D16, info)  /* CRW GT2 mobile */

#define	INTEL_HSW_GT3_IDS(info) \
	INTEL_VGA_DEVICE(0x0422, info), /* GT3 desktop */ \
	INTEL_VGA_DEVICE(0x042a, info), /* GT3 server */ \
	INTEL_VGA_DEVICE(0x042B, info), /* GT3 reserved */ \
	INTEL_VGA_DEVICE(0x042E, info), /* GT3 reserved */ \
	INTEL_VGA_DEVICE(0x0C22, info), /* SDV GT3 desktop */ \
	INTEL_VGA_DEVICE(0x0C2A, info), /* SDV GT3 server */ \
	INTEL_VGA_DEVICE(0x0C2B, info), /* SDV GT3 reserved */ \
	INTEL_VGA_DEVICE(0x0C2E, info), /* SDV GT3 reserved */ \
	INTEL_VGA_DEVICE(0x0A22, info), /* ULT GT3 desktop */ \
	INTEL_VGA_DEVICE(0x0A2A, info), /* ULT GT3 server */ \
	INTEL_VGA_DEVICE(0x0A2B, info), /* ULT GT3 reserved */ \
	INTEL_VGA_DEVICE(0x0D22, info), /* CRW GT3 desktop */ \
	INTEL_VGA_DEVICE(0x0D2A, info), /* CRW GT3 server */ \
	INTEL_VGA_DEVICE(0x0D2B, info), /* CRW GT3 reserved */ \
	INTEL_VGA_DEVICE(0x0D2E, info), /* CRW GT3 reserved */ \
	INTEL_VGA_DEVICE(0x0C26, info), /* SDV GT3 mobile */ \
	INTEL_VGA_DEVICE(0x0A26, info), /* ULT GT3 mobile */ \
	INTEL_VGA_DEVICE(0x0A2E, info), /* ULT GT3 reserved */ \
	INTEL_VGA_DEVICE(0x0D26, info)  /* CRW GT3 mobile */

#define	INTEL_HSW_IDS(info) \
	INTEL_HSW_GT1_IDS(info), \
	INTEL_HSW_GT2_IDS(info), \
	INTEL_HSW_GT3_IDS(info)

#define	INTEL_VLV_IDS(info) \
	INTEL_VGA_DEVICE(0x0f30, info), \
	INTEL_VGA_DEVICE(0x0f31, info), \
	INTEL_VGA_DEVICE(0x0f32, info), \
	INTEL_VGA_DEVICE(0x0f33, info), \
	INTEL_VGA_DEVICE(0x0157, info), \
	INTEL_VGA_DEVICE(0x0155, info)

#define	INTEL_BDW_GT1_IDS(info)  \
	INTEL_VGA_DEVICE(0x1602, info), /* GT1 ULT */ \
	INTEL_VGA_DEVICE(0x1606, info), /* GT1 ULT */ \
	INTEL_VGA_DEVICE(0x160B, info), /* GT1 Iris */ \
	INTEL_VGA_DEVICE(0x160E, info), /* GT1 ULX */ \
	INTEL_VGA_DEVICE(0x160A, info), /* GT1 Server */ \
	INTEL_VGA_DEVICE(0x160D, info)  /* GT1 Workstation */

#define	INTEL_BDW_GT2_IDS(info)  \
	INTEL_VGA_DEVICE(0x1612, info), /* GT2 Halo */	\
	INTEL_VGA_DEVICE(0x1616, info), /* GT2 ULT */ \
	INTEL_VGA_DEVICE(0x161B, info), /* GT2 ULT */ \
	INTEL_VGA_DEVICE(0x161E, info), /* GT2 ULX */ \
	INTEL_VGA_DEVICE(0x161A, info), /* GT2 Server */ \
	INTEL_VGA_DEVICE(0x161D, info)  /* GT2 Workstation */

#define	INTEL_BDW_GT3_IDS(info) \
	INTEL_VGA_DEVICE(0x1622, info), /* ULT */ \
	INTEL_VGA_DEVICE(0x1626, info), /* ULT */ \
	INTEL_VGA_DEVICE(0x162B, info), /* Iris */ \
	INTEL_VGA_DEVICE(0x162E, info),  /* ULX */\
	INTEL_VGA_DEVICE(0x162A, info), /* Server */ \
	INTEL_VGA_DEVICE(0x162D, info)  /* Workstation */

#define	INTEL_BDW_RSVD_IDS(info) \
	INTEL_VGA_DEVICE(0x1632, info), /* ULT */ \
	INTEL_VGA_DEVICE(0x1636, info), /* ULT */ \
	INTEL_VGA_DEVICE(0x163B, info), /* Iris */ \
	INTEL_VGA_DEVICE(0x163E, info), /* ULX */ \
	INTEL_VGA_DEVICE(0x163A, info), /* Server */ \
	INTEL_VGA_DEVICE(0x163D, info)  /* Workstation */

#define	INTEL_BDW_IDS(info) \
	INTEL_BDW_GT1_IDS(info), \
	INTEL_BDW_GT2_IDS(info), \
	INTEL_BDW_GT3_IDS(info), \
	INTEL_BDW_RSVD_IDS(info)

#define	INTEL_CHV_IDS(info) \
	INTEL_VGA_DEVICE(0x22b0, info), \
	INTEL_VGA_DEVICE(0x22b1, info), \
	INTEL_VGA_DEVICE(0x22b2, info), \
	INTEL_VGA_DEVICE(0x22b3, info)

#define	INTEL_SKL_GT1_IDS(info)	\
	INTEL_VGA_DEVICE(0x1906, info), /* ULT GT1 */ \
	INTEL_VGA_DEVICE(0x190E, info), /* ULX GT1 */ \
	INTEL_VGA_DEVICE(0x1902, info), /* DT  GT1 */ \
	INTEL_VGA_DEVICE(0x190B, info), /* Halo GT1 */ \
	INTEL_VGA_DEVICE(0x190A, info) /* SRV GT1 */

#define	INTEL_SKL_GT2_IDS(info)	\
	INTEL_VGA_DEVICE(0x1916, info), /* ULT GT2 */ \
	INTEL_VGA_DEVICE(0x1921, info), /* ULT GT2F */ \
	INTEL_VGA_DEVICE(0x191E, info), /* ULX GT2 */ \
	INTEL_VGA_DEVICE(0x1912, info), /* DT  GT2 */ \
	INTEL_VGA_DEVICE(0x191B, info), /* Halo GT2 */ \
	INTEL_VGA_DEVICE(0x191A, info), /* SRV GT2 */ \
	INTEL_VGA_DEVICE(0x191D, info)  /* WKS GT2 */

#define	INTEL_SKL_GT3_IDS(info) \
	INTEL_VGA_DEVICE(0x1923, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x1926, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x1927, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x192B, info), /* Halo GT3 */ \
	INTEL_VGA_DEVICE(0x192D, info)  /* SRV GT3 */

#define	INTEL_SKL_GT4_IDS(info) \
	INTEL_VGA_DEVICE(0x1932, info), /* DT GT4 */ \
	INTEL_VGA_DEVICE(0x193B, info), /* Halo GT4 */ \
	INTEL_VGA_DEVICE(0x193D, info), /* WKS GT4 */ \
	INTEL_VGA_DEVICE(0x192A, info), /* SRV GT4 */ \
	INTEL_VGA_DEVICE(0x193A, info)  /* SRV GT4e */

#define	INTEL_SKL_IDS(info)	 \
	INTEL_SKL_GT1_IDS(info), \
	INTEL_SKL_GT2_IDS(info), \
	INTEL_SKL_GT3_IDS(info), \
	INTEL_SKL_GT4_IDS(info)

#define	INTEL_BXT_IDS(info) \
	INTEL_VGA_DEVICE(0x0A84, info), \
	INTEL_VGA_DEVICE(0x1A84, info), \
	INTEL_VGA_DEVICE(0x1A85, info), \
	INTEL_VGA_DEVICE(0x5A84, info), /* APL HD Graphics 505 */ \
	INTEL_VGA_DEVICE(0x5A85, info)  /* APL HD Graphics 500 */

#define	INTEL_GLK_IDS(info) \
	INTEL_VGA_DEVICE(0x3184, info), \
	INTEL_VGA_DEVICE(0x3185, info)

#define	INTEL_KBL_GT1_IDS(info)	\
	INTEL_VGA_DEVICE(0x5913, info), /* ULT GT1.5 */ \
	INTEL_VGA_DEVICE(0x5915, info), /* ULX GT1.5 */ \
	INTEL_VGA_DEVICE(0x5906, info), /* ULT GT1 */ \
	INTEL_VGA_DEVICE(0x590E, info), /* ULX GT1 */ \
	INTEL_VGA_DEVICE(0x5902, info), /* DT  GT1 */ \
	INTEL_VGA_DEVICE(0x5908, info), /* Halo GT1 */ \
	INTEL_VGA_DEVICE(0x590B, info), /* Halo GT1 */ \
	INTEL_VGA_DEVICE(0x590A, info) /* SRV GT1 */

#define	INTEL_KBL_GT2_IDS(info)	\
	INTEL_VGA_DEVICE(0x5916, info), /* ULT GT2 */ \
	INTEL_VGA_DEVICE(0x5917, info), /* Mobile GT2 */ \
	INTEL_VGA_DEVICE(0x5921, info), /* ULT GT2F */ \
	INTEL_VGA_DEVICE(0x591E, info), /* ULX GT2 */ \
	INTEL_VGA_DEVICE(0x5912, info), /* DT  GT2 */ \
	INTEL_VGA_DEVICE(0x591B, info), /* Halo GT2 */ \
	INTEL_VGA_DEVICE(0x591A, info), /* SRV GT2 */ \
	INTEL_VGA_DEVICE(0x591D, info) /* WKS GT2 */

#define	INTEL_KBL_GT3_IDS(info) \
	INTEL_VGA_DEVICE(0x5923, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x5926, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x5927, info) /* ULT GT3 */

#define	INTEL_KBL_GT4_IDS(info) \
	INTEL_VGA_DEVICE(0x593B, info) /* Halo GT4 */

#define	INTEL_KBL_IDS(info) \
	INTEL_KBL_GT1_IDS(info), \
	INTEL_KBL_GT2_IDS(info), \
	INTEL_KBL_GT3_IDS(info), \
	INTEL_KBL_GT4_IDS(info)

/* CFL S */
#define	INTEL_CFL_S_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x3E90, info), /* SRV GT1 */ \
	INTEL_VGA_DEVICE(0x3E93, info), /* SRV GT1 */ \
	INTEL_VGA_DEVICE(0x3E99, info)  /* SRV GT1 */

#define	INTEL_CFL_S_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x3E91, info), /* SRV GT2 */ \
	INTEL_VGA_DEVICE(0x3E92, info), /* SRV GT2 */ \
	INTEL_VGA_DEVICE(0x3E96, info), /* SRV GT2 */ \
	INTEL_VGA_DEVICE(0x3E98, info), /* SRV GT2 */ \
	INTEL_VGA_DEVICE(0x3E9A, info)  /* SRV GT2 */

/* CFL H */
#define	INTEL_CFL_H_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x3E9B, info), /* Halo GT2 */ \
	INTEL_VGA_DEVICE(0x3E94, info)  /* Halo GT2 */

/* CFL U GT1 */
#define	INTEL_CFL_U_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x3EA1, info), \
	INTEL_VGA_DEVICE(0x3EA4, info)

/* CFL U GT2 */
#define	INTEL_CFL_U_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x3EA0, info), \
	INTEL_VGA_DEVICE(0x3EA3, info), \
	INTEL_VGA_DEVICE(0x3EA9, info)

/* CFL U GT3 */
#define	INTEL_CFL_U_GT3_IDS(info) \
	INTEL_VGA_DEVICE(0x3EA2, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x3EA5, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x3EA6, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x3EA7, info), /* ULT GT3 */ \
	INTEL_VGA_DEVICE(0x3EA8, info)  /* ULT GT3 */

#define	INTEL_CFL_IDS(info)	   \
	INTEL_CFL_S_GT1_IDS(info), \
	INTEL_CFL_S_GT2_IDS(info), \
	INTEL_CFL_H_GT2_IDS(info), \
	INTEL_CFL_U_GT1_IDS(info), \
	INTEL_CFL_U_GT2_IDS(info), \
	INTEL_CFL_U_GT3_IDS(info)

/* CNL */
#define	INTEL_CNL_IDS(info) \
	INTEL_VGA_DEVICE(0x5A51, info), \
	INTEL_VGA_DEVICE(0x5A59, info), \
	INTEL_VGA_DEVICE(0x5A41, info), \
	INTEL_VGA_DEVICE(0x5A49, info), \
	INTEL_VGA_DEVICE(0x5A52, info), \
	INTEL_VGA_DEVICE(0x5A5A, info), \
	INTEL_VGA_DEVICE(0x5A42, info), \
	INTEL_VGA_DEVICE(0x5A4A, info), \
	INTEL_VGA_DEVICE(0x5A50, info), \
	INTEL_VGA_DEVICE(0x5A40, info), \
	INTEL_VGA_DEVICE(0x5A54, info), \
	INTEL_VGA_DEVICE(0x5A5C, info), \
	INTEL_VGA_DEVICE(0x5A44, info), \
	INTEL_VGA_DEVICE(0x5A4C, info)

/* ICL */
#define	INTEL_ICL_PORT_F_IDS(info) \
	INTEL_VGA_DEVICE(0x8A50, info), \
	INTEL_VGA_DEVICE(0x8A52, info), \
	INTEL_VGA_DEVICE(0x8A53, info), \
	INTEL_VGA_DEVICE(0x8A54, info), \
	INTEL_VGA_DEVICE(0x8A56, info), \
	INTEL_VGA_DEVICE(0x8A57, info), \
	INTEL_VGA_DEVICE(0x8A58, info), \
	INTEL_VGA_DEVICE(0x8A59, info), \
	INTEL_VGA_DEVICE(0x8A5A, info), \
	INTEL_VGA_DEVICE(0x8A5B, info), \
	INTEL_VGA_DEVICE(0x8A5C, info), \
	INTEL_VGA_DEVICE(0x8A70, info), \
	INTEL_VGA_DEVICE(0x8A71, info)

#define	INTEL_ICL_11_IDS(info) \
	INTEL_ICL_PORT_F_IDS(info), \
	INTEL_VGA_DEVICE(0x8A51, info), \
	INTEL_VGA_DEVICE(0x8A5D, info)

/* EHL */
#define INTEL_EHL_IDS(info) \
	INTEL_VGA_DEVICE(0x4541, info), \
	INTEL_VGA_DEVICE(0x4551, info), \
	INTEL_VGA_DEVICE(0x4555, info), \
	INTEL_VGA_DEVICE(0x4557, info), \
	INTEL_VGA_DEVICE(0x4570, info), \
	INTEL_VGA_DEVICE(0x4571, info)

/* JSL */
#define INTEL_JSL_IDS(info) \
	INTEL_VGA_DEVICE(0x4E51, info), \
	INTEL_VGA_DEVICE(0x4E55, info), \
	INTEL_VGA_DEVICE(0x4E57, info), \
	INTEL_VGA_DEVICE(0x4E61, info), \
	INTEL_VGA_DEVICE(0x4E71, info)

/* TGL */
#define INTEL_TGL_12_GT1_IDS(info) \
	INTEL_VGA_DEVICE(0x9A60, info), \
	INTEL_VGA_DEVICE(0x9A68, info), \
	INTEL_VGA_DEVICE(0x9A70, info)

#define INTEL_TGL_12_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x9A40, info), \
	INTEL_VGA_DEVICE(0x9A49, info), \
	INTEL_VGA_DEVICE(0x9A59, info), \
	INTEL_VGA_DEVICE(0x9A78, info), \
	INTEL_VGA_DEVICE(0x9AC0, info), \
	INTEL_VGA_DEVICE(0x9AC9, info), \
	INTEL_VGA_DEVICE(0x9AD9, info), \
	INTEL_VGA_DEVICE(0x9AF8, info)

#define INTEL_TGL_12_IDS(info) \
	INTEL_TGL_12_GT1_IDS(info), \
	INTEL_TGL_12_GT2_IDS(info)

/* RKL */
#define INTEL_RKL_IDS(info) \
	INTEL_VGA_DEVICE(0x4C80, info), \
	INTEL_VGA_DEVICE(0x4C8A, info), \
	INTEL_VGA_DEVICE(0x4C8B, info), \
	INTEL_VGA_DEVICE(0x4C8C, info), \
	INTEL_VGA_DEVICE(0x4C90, info), \
	INTEL_VGA_DEVICE(0x4C9A, info)

/* DG1 */
#define INTEL_DG1_IDS(info) \
	INTEL_VGA_DEVICE(0x4905, info), \
	INTEL_VGA_DEVICE(0x4906, info), \
	INTEL_VGA_DEVICE(0x4907, info), \
	INTEL_VGA_DEVICE(0x4908, info), \
	INTEL_VGA_DEVICE(0x4909, info)

/* ADL-S */
#define INTEL_ADLS_IDS(info) \
	INTEL_VGA_DEVICE(0x4680, info), \
	INTEL_VGA_DEVICE(0x4682, info), \
	INTEL_VGA_DEVICE(0x4688, info), \
	INTEL_VGA_DEVICE(0x468A, info), \
	INTEL_VGA_DEVICE(0x468B, info), \
	INTEL_VGA_DEVICE(0x4690, info), \
	INTEL_VGA_DEVICE(0x4692, info), \
	INTEL_VGA_DEVICE(0x4693, info)

/* ADL-P */
#define INTEL_ADLP_IDS(info) \
	INTEL_VGA_DEVICE(0x46A0, info), \
	INTEL_VGA_DEVICE(0x46A1, info), \
	INTEL_VGA_DEVICE(0x46A2, info), \
	INTEL_VGA_DEVICE(0x46A3, info), \
	INTEL_VGA_DEVICE(0x46A6, info), \
	INTEL_VGA_DEVICE(0x46A8, info), \
	INTEL_VGA_DEVICE(0x46AA, info), \
	INTEL_VGA_DEVICE(0x462A, info), \
	INTEL_VGA_DEVICE(0x4626, info), \
	INTEL_VGA_DEVICE(0x4628, info), \
	INTEL_VGA_DEVICE(0x46B0, info), \
	INTEL_VGA_DEVICE(0x46B1, info), \
	INTEL_VGA_DEVICE(0x46B2, info), \
	INTEL_VGA_DEVICE(0x46B3, info), \
	INTEL_VGA_DEVICE(0x46C0, info), \
	INTEL_VGA_DEVICE(0x46C1, info), \
	INTEL_VGA_DEVICE(0x46C2, info), \
	INTEL_VGA_DEVICE(0x46C3, info)

/* ADL-N */
#define	INTEL_ADLN_IDS(info) \
	INTEL_VGA_DEVICE(0x46D0, info), \
	INTEL_VGA_DEVICE(0x46D1, info), \
	INTEL_VGA_DEVICE(0x46D2, info)

/* RPL-S */
#define INTEL_RPLS_IDS(info) \
	INTEL_VGA_DEVICE(0xA780, info), \
	INTEL_VGA_DEVICE(0xA781, info), \
	INTEL_VGA_DEVICE(0xA782, info), \
	INTEL_VGA_DEVICE(0xA783, info), \
	INTEL_VGA_DEVICE(0xA788, info), \
	INTEL_VGA_DEVICE(0xA789, info), \
	INTEL_VGA_DEVICE(0xA78A, info), \
	INTEL_VGA_DEVICE(0xA78B, info)

/* RPL-U */
#define	INTEL_RPLU_IDS(info) \
	INTEL_VGA_DEVICE(0xA721, info), \
	INTEL_VGA_DEVICE(0xA7A1, info), \
	INTEL_VGA_DEVICE(0xA7A9, info), \
	INTEL_VGA_DEVICE(0xA7AC, info), \
	INTEL_VGA_DEVICE(0xA7AD, info)

/* RPL-P */
#define	INTEL_RPLP_IDS(info) \
	INTEL_RPLU_IDS(info), \
	INTEL_VGA_DEVICE(0xA720, info), \
	INTEL_VGA_DEVICE(0xA7A0, info), \
	INTEL_VGA_DEVICE(0xA7A8, info), \
	INTEL_VGA_DEVICE(0xA7AA, info), \
	INTEL_VGA_DEVICE(0xA7AB, info)

#endif /* _PCI_EARLY_QUIRKS_H_ */
