/*-
 * Copyright 2016-2023 Microchip Technology, Inc. and/or its subsidiaries.
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


/*
 * Driver for the Microsemi Smart storage controllers
 */

#include "smartpqi_includes.h"

CTASSERT(BSD_SUCCESS == PQI_STATUS_SUCCESS);

/*
 * Supported devices
 */
struct pqi_ident
{
	u_int16_t		vendor;
	u_int16_t		device;
	u_int16_t		subvendor;
	u_int16_t		subdevice;
	int			hwif;
	char			*desc;
} pqi_identifiers[] = {
	/* (MSCC PM8205 8x12G based) */
	{0x9005, 0x028f, 0x103c, 0x600,  PQI_HWIF_SRCV, "P408i-p SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x601,  PQI_HWIF_SRCV, "P408e-p SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x602,  PQI_HWIF_SRCV, "P408i-a SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x603,  PQI_HWIF_SRCV, "P408i-c SR Gen10"},
	{0x9005, 0x028f, 0x1028, 0x1FE0, PQI_HWIF_SRCV, "SmartRAID 3162-8i/eDell"},
	{0x9005, 0x028f, 0x9005, 0x608,  PQI_HWIF_SRCV, "SmartRAID 3162-8i/e"},
	{0x9005, 0x028f, 0x103c, 0x609,  PQI_HWIF_SRCV, "P408i-sb SR G10"},

	/* (MSCC PM8225 8x12G based) */
	{0x9005, 0x028f, 0x103c, 0x650,  PQI_HWIF_SRCV, "E208i-p SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x651,  PQI_HWIF_SRCV, "E208e-p SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x652,  PQI_HWIF_SRCV, "E208i-c SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x654,  PQI_HWIF_SRCV, "E208i-a SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x655,  PQI_HWIF_SRCV, "P408e-m SR Gen10"},
	{0x9005, 0x028f, 0x9005, 0x659,  PQI_HWIF_SRCV, "2100C8iOXS"},

	/* (MSCC PM8221 8x12G based) */
	{0x9005, 0x028f, 0x103c, 0x700,  PQI_HWIF_SRCV, "P204i-c SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x701,  PQI_HWIF_SRCV, "P204i-b SR Gen10"},
	{0x9005, 0x028f, 0x193d, 0x1104, PQI_HWIF_SRCV, "UN RAID P2404-Mf-4i-2GB"},
	{0x9005, 0x028f, 0x193d, 0x1106, PQI_HWIF_SRCV, "UN RAID P2404-Mf-4i-1GB"},
	{0x9005, 0x028f, 0x193d, 0x1108, PQI_HWIF_SRCV, "UN RAID P4408-Ma-8i-2GB"},
	{0x9005, 0x028f, 0x193d, 0x1109, PQI_HWIF_SRCV, "UN RAID P4408-Mr-8i-2GB"},

	/* (MSCC PM8204 8x12G based) */
	{0x9005, 0x028f, 0x9005, 0x800,  PQI_HWIF_SRCV, "SmartRAID 3154-8i"},
	{0x9005, 0x028f, 0x9005, 0x801,  PQI_HWIF_SRCV, "SmartRAID 3152-8i"},
	{0x9005, 0x028f, 0x9005, 0x802,  PQI_HWIF_SRCV, "SmartRAID 3151-4i"},
	{0x9005, 0x028f, 0x9005, 0x803,  PQI_HWIF_SRCV, "SmartRAID 3101-4i"},
	{0x9005, 0x028f, 0x9005, 0x804,  PQI_HWIF_SRCV, "SmartRAID 3154-8e"},
	{0x9005, 0x028f, 0x9005, 0x805,  PQI_HWIF_SRCV, "SmartRAID 3102-8i"},
	{0x9005, 0x028f, 0x9005, 0x806,  PQI_HWIF_SRCV, "SmartRAID 3100"},
	{0x9005, 0x028f, 0x9005, 0x807,  PQI_HWIF_SRCV, "SmartRAID 3162-8i"},
	{0x9005, 0x028f, 0x152d, 0x8a22, PQI_HWIF_SRCV, "QS-8204-8i"},
	{0x9005, 0x028f, 0x193d, 0xf460, PQI_HWIF_SRCV, "UN RAID P460-M4"},
	{0x9005, 0x028f, 0x193d, 0xf461, PQI_HWIF_SRCV, "UN RAID P460-B4"},
	{0x9005, 0x028f, 0x1bd4, 0x004b, PQI_HWIF_SRCV, "PM8204-2GB"},
	{0x9005, 0x028f, 0x1bd4, 0x004c, PQI_HWIF_SRCV, "PM8204-4GB"},
	{0x9005, 0x028f, 0x193d, 0x1105, PQI_HWIF_SRCV, "UN RAID P4408-Mf-8i-2GB"},
	{0x9005, 0x028f, 0x193d, 0x1107, PQI_HWIF_SRCV, "UN RAID P4408-Mf-8i-4GB"},
	{0x9005, 0x028f, 0x1d8d, 0x800,	 PQI_HWIF_SRCV, "Fiberhome SmartRAID AIS-8204-8i"},
	{0x9005, 0x028f, 0x9005, 0x0808, PQI_HWIF_SRCV,	"SmartRAID 3101E-4i"},
	{0x9005, 0x028f, 0x9005, 0x0809, PQI_HWIF_SRCV, "SmartRAID 3102E-8i"},
	{0x9005, 0x028f, 0x9005, 0x080a, PQI_HWIF_SRCV, "SmartRAID 3152-8i/N"},
	{0x9005, 0x028f, 0x1cc4, 0x0101, PQI_HWIF_SRCV, "Ramaxel FBGF-RAD PM8204"},

	/* (MSCC PM8222 8x12G based) */
	{0x9005, 0x028f, 0x9005, 0x900,  PQI_HWIF_SRCV, "SmartHBA 2100-8i"},
	{0x9005, 0x028f, 0x9005, 0x901,  PQI_HWIF_SRCV, "SmartHBA 2100-4i"},
	{0x9005, 0x028f, 0x9005, 0x902,  PQI_HWIF_SRCV, "HBA 1100-8i"},
	{0x9005, 0x028f, 0x9005, 0x903,  PQI_HWIF_SRCV, "HBA 1100-4i"},
	{0x9005, 0x028f, 0x9005, 0x904,  PQI_HWIF_SRCV, "SmartHBA 2100-8e"},
	{0x9005, 0x028f, 0x9005, 0x905,  PQI_HWIF_SRCV, "HBA 1100-8e"},
	{0x9005, 0x028f, 0x9005, 0x906,  PQI_HWIF_SRCV, "SmartHBA 2100-4i4e"},
	{0x9005, 0x028f, 0x9005, 0x907,  PQI_HWIF_SRCV, "HBA 1100"},
	{0x9005, 0x028f, 0x9005, 0x908,  PQI_HWIF_SRCV, "SmartHBA 2100"},
	{0x9005, 0x028f, 0x9005, 0x90a,  PQI_HWIF_SRCV, "SmartHBA 2100A-8i"},
	{0x9005, 0x028f, 0x193d, 0x8460, PQI_HWIF_SRCV, "UN HBA H460-M1"},
	{0x9005, 0x028f, 0x193d, 0x8461, PQI_HWIF_SRCV, "UN HBA H460-B1"},
	{0x9005, 0x028f, 0x193d, 0xc460, PQI_HWIF_SRCV, "UN RAID P460-M2"},
	{0x9005, 0x028f, 0x193d, 0xc461, PQI_HWIF_SRCV, "UN RAID P460-B2"},
	{0x9005, 0x028f, 0x1bd4, 0x004a, PQI_HWIF_SRCV, "PM8222-SHBA"},
	{0x9005, 0x028f, 0x13fe, 0x8312, PQI_HWIF_SRCV, "MIC-8312BridgeB"},
	{0x9005, 0x028f, 0x1bd4, 0x004f, PQI_HWIF_SRCV, "PM8222-HBA"},
	{0x9005, 0x028f, 0x1d8d, 0x908,	 PQI_HWIF_SRCV, "Fiberhome SmartHBA AIS-8222-8i"},
	{0x9005, 0x028f, 0x1bd4, 0x006C, PQI_HWIF_SRCV, "RS0800M5E8i"},
	{0x9005, 0x028f, 0x1bd4, 0x006D, PQI_HWIF_SRCV, "RS0800M5H8i"},
	{0x9005, 0x028f, 0x1cc4, 0x0201, PQI_HWIF_SRCV, "Ramaxel FBGF-RAD PM8222"},

	/* (SRCx MSCC FVB 24x12G based) */
	{0x9005, 0x028f, 0x103c, 0x1001, PQI_HWIF_SRCV, "MSCC FVB"},

	/* (MSCC PM8241 24x12G based) */

	/* (MSCC PM8242 24x12G based) */
	{0x9005, 0x028f, 0x152d, 0x8a37, PQI_HWIF_SRCV, "QS-8242-24i"},
	{0x9005, 0x028f, 0x9005, 0x1300, PQI_HWIF_SRCV, "HBA 1100-8i8e"},
	{0x9005, 0x028f, 0x9005, 0x1301, PQI_HWIF_SRCV, "HBA 1100-24i"},
	{0x9005, 0x028f, 0x9005, 0x1302, PQI_HWIF_SRCV, "SmartHBA 2100-8i8e"},
	{0x9005, 0x028f, 0x9005, 0x1303, PQI_HWIF_SRCV, "SmartHBA 2100-24i"},
	{0x9005, 0x028f, 0x105b, 0x1321, PQI_HWIF_SRCV, "8242-24i"},
	{0x9005, 0x028f, 0x1bd4, 0x0045, PQI_HWIF_SRCV, "SMART-HBA 8242-24i"},
	{0x9005, 0x028f, 0x1bd4, 0x006B, PQI_HWIF_SRCV, "RS0800M5H24i"},
	{0x9005, 0x028f, 0x1bd4, 0x0070, PQI_HWIF_SRCV, "RS0800M5E24i"},

	/* (MSCC PM8236 16x12G based) */
	{0x9005, 0x028f, 0x152d, 0x8a24, PQI_HWIF_SRCV, "QS-8236-16i"},
	{0x9005, 0x028f, 0x9005, 0x1380, PQI_HWIF_SRCV, "SmartRAID 3154-16i"},
	{0x9005, 0x028f, 0x1bd4, 0x0046, PQI_HWIF_SRCV, "RAID 8236-16i"},
	{0x9005, 0x028f, 0x1d8d, 0x806,  PQI_HWIF_SRCV, "Fiberhome SmartRAID AIS-8236-16i"},
	{0x9005, 0x028f, 0x1cf2, 0x0B27, PQI_HWIF_SRCV, "ZTE SmartROC3100 SDPSA/B-18i 4G"},
	{0x9005, 0x028f, 0x1cf2, 0x0B45, PQI_HWIF_SRCV, "ZTE SmartROC3100 SDPSA/B_L-18i 2G"},
	{0x9005, 0x028f, 0x1cf2, 0x5445, PQI_HWIF_SRCV, "ZTE SmartROC3100 RM241-18i 2G"},
	{0x9005, 0x028f, 0x1cf2, 0x5446, PQI_HWIF_SRCV, "ZTE SmartROC3100 RM242-18i 4G"},
	{0x9005, 0x028f, 0x1cf2, 0x5449, PQI_HWIF_SRCV, "ZTE SmartROC3100 RS241-18i 2G"},
	{0x9005, 0x028f, 0x1cf2, 0x544A, PQI_HWIF_SRCV, "ZTE SmartROC3100 RS242-18i 4G"},
	{0x9005, 0x028f, 0x1cf2, 0x544D, PQI_HWIF_SRCV, "ZTE SmartROC3100 RM241B-18i 2G"},
	{0x9005, 0x028f, 0x1cf2, 0x544E, PQI_HWIF_SRCV, "ZTE SmartROC3100 RM242B-18i 4G"},
	{0x9005, 0x028f, 0x1bd4, 0x006F, PQI_HWIF_SRCV, "RS0804M5R16i"},



	/* (MSCC PM8237 24x12G based) */
	{0x9005, 0x028f, 0x103c, 0x1100, PQI_HWIF_SRCV, "P816i-a SR Gen10"},
	{0x9005, 0x028f, 0x103c, 0x1101, PQI_HWIF_SRCV, "P416ie-m SR G10"},

	/* (MSCC PM8238 16x12G based) */
	{0x9005, 0x028f, 0x152d, 0x8a23, PQI_HWIF_SRCV, "QS-8238-16i"},
	{0x9005, 0x028f, 0x9005, 0x1280, PQI_HWIF_SRCV, "HBA 1100-16i"},
	{0x9005, 0x028f, 0x9005, 0x1281, PQI_HWIF_SRCV, "HBA 1100-16e"},
	{0x9005, 0x028f, 0x105b, 0x1211, PQI_HWIF_SRCV, "8238-16i"},
	{0x9005, 0x028f, 0x1bd4, 0x0048, PQI_HWIF_SRCV, "SMART-HBA 8238-16i"},
	{0x9005, 0x028f, 0x9005, 0x1282, PQI_HWIF_SRCV, "SmartHBA 2100-16i"},
	{0x9005, 0x028f, 0x1d8d, 0x916,  PQI_HWIF_SRCV, "Fiberhome SmartHBA AIS-8238-16i"},
	{0x9005, 0x028f, 0x1458, 0x1000, PQI_HWIF_SRCV, "GIGABYTE SmartHBA CLN1832"},
	{0x9005, 0x028f, 0x1cf2, 0x0B29, PQI_HWIF_SRCV, "ZTE SmartIOC2100 SDPSA/B_I-18i"},
	{0x9005, 0x028f, 0x1cf2, 0x5447, PQI_HWIF_SRCV, "ZTE SmartIOC2100 RM243-18i"},
	{0x9005, 0x028f, 0x1cf2, 0x544B, PQI_HWIF_SRCV, "ZTE SmartIOC2100 RS243-18i"},
	{0x9005, 0x028f, 0x1cf2, 0x544F, PQI_HWIF_SRCV, "ZTE SmartIOC2100 RM243B-18i"},
	{0x9005, 0x028f, 0x1bd4, 0x0071, PQI_HWIF_SRCV, "RS0800M5H16i"},
	{0x9005, 0x028f, 0x1bd4, 0x0072, PQI_HWIF_SRCV, "RS0800M5E16i"},

	/* (MSCC PM8240 24x12G based) */
	{0x9005, 0x028f, 0x152d, 0x8a36, PQI_HWIF_SRCV, "QS-8240-24i"},
	{0x9005, 0x028f, 0x9005, 0x1200, PQI_HWIF_SRCV, "SmartRAID 3154-24i"},
	{0x9005, 0x028f, 0x9005, 0x1201, PQI_HWIF_SRCV, "SmartRAID 3154-8i16e"},
	{0x9005, 0x028f, 0x9005, 0x1202, PQI_HWIF_SRCV, "SmartRAID 3154-8i8e"},
	{0x9005, 0x028f, 0x1bd4, 0x0047, PQI_HWIF_SRCV, "RAID 8240-24i"},
	{0x9005, 0x028f, 0x1dfc, 0x3161, PQI_HWIF_SRCV, "NTCOM SAS3 RAID-24i"},
	{0x9005, 0x028f, 0x1F0C, 0x3161, PQI_HWIF_SRCV, "NT RAID 3100-24i"},

	/* Huawei ID's */
	{0x9005, 0x028f, 0x19e5, 0xd227, PQI_HWIF_SRCV, "SR465C-M 4G"},
	{0x9005, 0x028f, 0x19e5, 0xd22a, PQI_HWIF_SRCV, "SR765-M"},
	{0x9005, 0x028f, 0x19e5, 0xd228, PQI_HWIF_SRCV, "SR455C-M 2G"},
	{0x9005, 0x028f, 0x19e5, 0xd22c, PQI_HWIF_SRCV, "SR455C-M 4G"},
	{0x9005, 0x028f, 0x19e5, 0xd229, PQI_HWIF_SRCV, "SR155-M"},
	{0x9005, 0x028f, 0x19e5, 0xd22b, PQI_HWIF_SRCV, "SR455C-ME 4G"},

	/* (MSCC PM8252 8x12G based) */
	{0x9005, 0x028f, 0x193d, 0x110b, PQI_HWIF_SRCV, "UN HBA H4508-Mf-8i"},
	{0x9005, 0x028f, 0x1bd4, 0x0052, PQI_HWIF_SRCV, "MT0801M6E"},
	{0x9005, 0x028f, 0x1bd4, 0x0054, PQI_HWIF_SRCV, "MT0800M6H"},
	{0x9005, 0x028f, 0x1bd4, 0x0086, PQI_HWIF_SRCV, "RT0800M7E"},
	{0x9005, 0x028f, 0x1bd4, 0x0087, PQI_HWIF_SRCV, "RT0800M7H"},
	{0x9005, 0x028f, 0x1f51, 0x1001, PQI_HWIF_SRCV, "SmartHBA P6600-8i"},
	{0x9005, 0x028f, 0x1f51, 0x1003, PQI_HWIF_SRCV, "SmartHBA P6600-8e"},
	{0x9005, 0x028f, 0x9005, 0x1460, PQI_HWIF_SRCV, "HBA 1200"},
	{0x9005, 0x028f, 0x9005, 0x1461, PQI_HWIF_SRCV, "SmartHBA 2200"},
	{0x9005, 0x028f, 0x9005, 0x1462, PQI_HWIF_SRCV, "HBA 1200-8i"},

	/* (MSCC PM8254 32x12G based) */
	{0x9005, 0x028f, 0x1bd4, 0x0051, PQI_HWIF_SRCV, "MT0804M6R"},
	{0x9005, 0x028f, 0x1bd4, 0x0053, PQI_HWIF_SRCV, "MT0808M6R"},
	{0x9005, 0x028f, 0x1bd4, 0x0088, PQI_HWIF_SRCV, "RT0804M7R"},
	{0x9005, 0x028f, 0x1bd4, 0x0089, PQI_HWIF_SRCV, "RT0808M7R"},
	{0x9005, 0x028f, 0x1f51, 0x1002, PQI_HWIF_SRCV, "SmartRAID P7604-8i"},
	{0x9005, 0x028f, 0x1f51, 0x1004, PQI_HWIF_SRCV, "SmartRAID P7604-8e"},
	{0x9005, 0x028f, 0x9005, 0x14a0, PQI_HWIF_SRCV, "SmartRAID 3254-8i"},
	{0x9005, 0x028f, 0x9005, 0x14a1, PQI_HWIF_SRCV, "SmartRAID 3204-8i"},
	{0x9005, 0x028f, 0x9005, 0x14a2, PQI_HWIF_SRCV, "SmartRAID 3252-8i"},
	{0x9005, 0x028f, 0x9005, 0x14a4, PQI_HWIF_SRCV, "SmartRAID 3254-8i /e"},
	{0x9005, 0x028f, 0x9005, 0x14a5, PQI_HWIF_SRCV, "SmartRAID 3252-8i /e"},
	{0x9005, 0x028f, 0x9005, 0x14a6, PQI_HWIF_SRCV, "SmartRAID 3204-8i /e"},

	/* (MSCC PM8262 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x14c0, PQI_HWIF_SRCV, "SmartHBA 2200-16i"},
	{0x9005, 0x028f, 0x9005, 0x14c1, PQI_HWIF_SRCV, "HBA 1200-16i"},
	{0x9005, 0x028f, 0x9005, 0x14c3, PQI_HWIF_SRCV, "HBA 1200-16e"},
	{0x9005, 0x028f, 0x9005, 0x14c4, PQI_HWIF_SRCV, "HBA 1200-8e"},
	{0x9005, 0x028f, 0x1f51, 0x1005, PQI_HWIF_SRCV, "SmartHBA P6600-16i"},
	{0x9005, 0x028f, 0x1f51, 0x1007, PQI_HWIF_SRCV, "SmartHBA P6600-8i8e"},
	{0x9005, 0x028f, 0x1f51, 0x1009, PQI_HWIF_SRCV, "SmartHBA P6600-16e"},
	{0x9005, 0x028f, 0x1cf2, 0x54dc, PQI_HWIF_SRCV, "ZTE SmartIOC2200 RM346-16i"},
	{0x9005, 0x028f, 0x1cf2, 0x0806, PQI_HWIF_SRCV, "ZTE SmartIOC2200 RS346-16i"},

	/* (MSCC PM8264 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x14b0, PQI_HWIF_SRCV, "SmartRAID 3254-16i"},
	{0x9005, 0x028f, 0x9005, 0x14b1, PQI_HWIF_SRCV, "SmartRAID 3258-16i"},
	{0x9005, 0x028f, 0x1f51, 0x1006, PQI_HWIF_SRCV, "SmartRAID P7608-16i"},
	{0x9005, 0x028f, 0x1f51, 0x1008, PQI_HWIF_SRCV, "SmartRAID P7608-8i8e"},
	{0x9005, 0x028f, 0x1f51, 0x100a, PQI_HWIF_SRCV, "SmartRAID P7608-16e"},
	{0x9005, 0x028f, 0x1cf2, 0x54da, PQI_HWIF_SRCV, "ZTE SmartROC3200 RM344-16i 4G"},
	{0x9005, 0x028f, 0x1cf2, 0x54db, PQI_HWIF_SRCV, "ZTE SmartROC3200 RM345-16i 8G"},
	{0x9005, 0x028f, 0x1cf2, 0x0804, PQI_HWIF_SRCV, "ZTE SmartROC3200 RS344-16i 4G"},
	{0x9005, 0x028f, 0x1cf2, 0x0805, PQI_HWIF_SRCV, "ZTE SmartROC3200 RS345-16i 8G"},

	/* (MSCC PM8265 16x12G based) */
	{0x9005, 0x028f, 0x1590, 0x02dc, PQI_HWIF_SRCV, "SR416i-a Gen10+"},
	{0x9005, 0x028f, 0x9005, 0x1470, PQI_HWIF_SRCV, "SmartRAID 3200"},
	{0x9005, 0x028f, 0x9005, 0x1471, PQI_HWIF_SRCV, "SmartRAID 3254-16i /e"},
	{0x9005, 0x028f, 0x9005, 0x1472, PQI_HWIF_SRCV, "SmartRAID 3258-16i /e"},
	{0x9005, 0x028f, 0x9005, 0x1473, PQI_HWIF_SRCV, "SmartRAID 3284-16io /e/uC"},
	{0x9005, 0x028f, 0x9005, 0x1474, PQI_HWIF_SRCV, "SmartRAID 3254-16io /e"},
	{0x9005, 0x028f, 0x9005, 0x1475, PQI_HWIF_SRCV, "SmartRAID 3254-16e /e"},

	/* (MSCC PM8266 16x12G based) */
	{0x9005, 0x028f, 0x1014, 0x0718, PQI_HWIF_SRCV, "IBM 4-Port 24G SAS"},
	{0x9005, 0x028f, 0x9005, 0x1490, PQI_HWIF_SRCV, "HBA 1200p Ultra"},
	{0x9005, 0x028f, 0x9005, 0x1491, PQI_HWIF_SRCV, "SmartHBA 2200p Ultra"},
	{0x9005, 0x028f, 0x9005, 0x1402, PQI_HWIF_SRCV, "HBA Ultra 1200P-16i"},
	{0x9005, 0x028f, 0x9005, 0x1441, PQI_HWIF_SRCV, "HBA Ultra 1200P-32i"},

	/* (MSCC PM8268 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x14d0, PQI_HWIF_SRCV, "SmartRAID Ultra 3258P-16i"},

	/* (MSCC PM8269 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x1400, PQI_HWIF_SRCV, "SmartRAID Ultra 3258P-16i /e"},

	/* (MSCC PM8270 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x1410, PQI_HWIF_SRCV, "HBA Ultra 1200P-16e"},
	{0x9005, 0x028f, 0x9005, 0x1411, PQI_HWIF_SRCV, "HBA 1200 Ultra"},
	{0x9005, 0x028f, 0x9005, 0x1412, PQI_HWIF_SRCV, "SmartHBA 2200 Ultra"},
	{0x9005, 0x028f, 0x9005, 0x1463, PQI_HWIF_SRCV, "SmartHBA 2200-8io /e"},
	{0x9005, 0x028f, 0x9005, 0x14c2, PQI_HWIF_SRCV, "SmartHBA 2200-16io /e"},

	/* (MSCC PM8271 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x14e0, PQI_HWIF_SRCV, "SmartIOC PM8271"},

	/* (MSCC PM8272 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x1420, PQI_HWIF_SRCV, "SmartRAID Ultra 3254-16e"},

	/* (MSCC PM8273 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x1430, PQI_HWIF_SRCV, "SmartRAID Ultra 3254-16e /e"},

	/* (MSCC PM8274 16x12G based) */
	{0x9005, 0x028f, 0x1e93, 0x1000, PQI_HWIF_SRCV, "ByteHBA JGH43024-8"},
	{0x9005, 0x028f, 0x1e93, 0x1001, PQI_HWIF_SRCV, "ByteHBA JGH43034-8"},
	{0x9005, 0x028f, 0x1e93, 0x1005, PQI_HWIF_SRCV, "ByteHBA JGH43014-8"},

	/* (MSCC PM8275 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x14f0, PQI_HWIF_SRCV, "SmartIOC PM8275"},

	/* (MSCC PM8276 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x1480, PQI_HWIF_SRCV, "SmartRAID 3200 Ultra"},
	{0x9005, 0x028f, 0x1e93, 0x1002, PQI_HWIF_SRCV, "ByteHBA JGH44014-8"},

	/* (MSCC PM8278 16x12G based) */
	{0x9005, 0x028f, 0x9005, 0x1440, PQI_HWIF_SRCV, "SmartRAID Ultra 3258P-32i"},

	/* (MSCC PM8279 32x12G based) */
	{0x9005, 0x028f, 0x9005, 0x1450, PQI_HWIF_SRCV, "SmartRAID Ultra 3258P-32i /e"},
	{0x9005, 0x028f, 0x1590, 0x0294, PQI_HWIF_SRCV, "SR932i-p Gen10+"},
	{0x9005, 0x028f, 0x1590, 0x0381, PQI_HWIF_SRCV, "SR932i-p Gen11"},
	{0x9005, 0x028f, 0x1590, 0x0382, PQI_HWIF_SRCV, "SR308i-p Gen11"},
	{0x9005, 0x028f, 0x1590, 0x0383, PQI_HWIF_SRCV, "SR308i-o Gen11"},
	{0x9005, 0x028f, 0x1590, 0x02db, PQI_HWIF_SRCV, "SR416ie-m Gen11"},
	{0x9005, 0x028f, 0x1590, 0x032e, PQI_HWIF_SRCV, "SR416i-o Gen11"},
	{0x9005, 0x028f, 0x9005, 0x1452, PQI_HWIF_SRCV, "SmartRAID 3200p Ultra"},

	/* (MSCC HBA/SMARTHBA/CFF SmartRAID - Lenovo 8X12G 16X12G based)  */
	{0x9005, 0x028f, 0x1d49, 0x0220, PQI_HWIF_SRCV, "4350-8i SAS/SATA HBA"},
	{0x9005, 0x028f, 0x1d49, 0x0221, PQI_HWIF_SRCV, "4350-16i SAS/SATA HBA"},
	{0x9005, 0x028f, 0x1d49, 0x0520, PQI_HWIF_SRCV, "5350-8i"},
	{0x9005, 0x028f, 0x1d49, 0x0522, PQI_HWIF_SRCV, "5350-8i INTR"},
	{0x9005, 0x028f, 0x1d49, 0x0620, PQI_HWIF_SRCV, "9350-8i 2GB Flash"},
	{0x9005, 0x028f, 0x1d49, 0x0621, PQI_HWIF_SRCV, "9350-8i 2GB Flash INTR"},
	{0x9005, 0x028f, 0x1d49, 0x0622, PQI_HWIF_SRCV, "9350-16i 4GB Flash"},
	{0x9005, 0x028f, 0x1d49, 0x0623, PQI_HWIF_SRCV, "9350-16i 4GB Flash INTR"},

	{0, 0, 0, 0, 0, 0}
};

struct pqi_ident
pqi_family_identifiers[] = {
	{0x9005, 0x028f, 0, 0, PQI_HWIF_SRCV, "Smart Array Storage Controller"},
	{0, 0, 0, 0, 0, 0}
};

/*
 * Function to identify the installed adapter.
 */
static struct pqi_ident *
pqi_find_ident(device_t dev)
{
	struct pqi_ident *m;
	u_int16_t vendid, devid, sub_vendid, sub_devid;
	static long AllowWildcards = 0xffffffff;
	int result;

#ifdef DEVICE_HINT
	if (AllowWildcards == 0xffffffff)
	{
		result = resource_long_value("smartpqi", 0, "allow_wildcards", &AllowWildcards);

		/* the default case if the hint is not found is to allow wildcards */
		if (result != DEVICE_HINT_SUCCESS) {
			AllowWildcards = 1;
		}
	}

#endif

	vendid = pci_get_vendor(dev);
	devid = pci_get_device(dev);
	sub_vendid = pci_get_subvendor(dev);
	sub_devid = pci_get_subdevice(dev);

	for (m = pqi_identifiers; m->vendor != 0; m++) {
		if ((m->vendor == vendid) && (m->device == devid) &&
			(m->subvendor == sub_vendid) &&
			(m->subdevice == sub_devid)) {
			return (m);
		}
	}

	for (m = pqi_family_identifiers; m->vendor != 0; m++) {
		if ((m->vendor == vendid) && (m->device == devid)) {
			if (AllowWildcards != 0)
			{
				DBG_NOTE("Controller device ID matched using wildcards\n");
				return (m);
			}
			else
			{
				DBG_NOTE("Controller not probed because device ID wildcards are disabled\n")
				return (NULL);
			}
		}
	}

	return (NULL);
}

/*
 * Determine whether this is one of our supported adapters.
 */
static int
smartpqi_probe(device_t dev)
{
	struct pqi_ident *id;

	if ((id = pqi_find_ident(dev)) != NULL) {
		device_set_desc(dev, id->desc);
		return(BUS_PROBE_VENDOR);
	}

	return(ENXIO);
}

/*
 * Store Bus/Device/Function in softs
 */
void
pqisrc_save_controller_info(struct pqisrc_softstate *softs)
{
	device_t dev = softs->os_specific.pqi_dev;

	softs->bus_id = (uint32_t)pci_get_bus(dev);
	softs->device_id = (uint32_t)pci_get_device(dev);
	softs->func_id = (uint32_t)pci_get_function(dev);
}


static void read_device_hint_resource(struct pqisrc_softstate *softs,
		char *keyword,  uint32_t *value)
{
	DBG_FUNC("IN\n");

	device_t dev = softs->os_specific.pqi_dev;

	if (resource_long_value("smartpqi", device_get_unit(dev), keyword, (long *)value) == DEVICE_HINT_SUCCESS) {
		if (*value) {
			/* set resource to 1 for disabling the
			 * firmware feature in device hint file. */
			*value = 0;

		}
		else {
			/* set resource to 0 for enabling the
			 * firmware feature in device hint file. */
			*value = 1;
		}
	}
	else {
		/* Enabled by default */
		*value = 1;
	}

	DBG_NOTE("SmartPQI Device Hint: %s, Is it enabled = %u\n", keyword, *value);

	DBG_FUNC("OUT\n");
}

static void read_device_hint_decimal_value(struct pqisrc_softstate *softs,
		char *keyword, uint32_t *value)
{
	DBG_FUNC("IN\n");

	device_t dev = softs->os_specific.pqi_dev;

	if (resource_long_value("smartpqi", device_get_unit(dev), keyword, (long *)value) == DEVICE_HINT_SUCCESS) {
		/* Nothing to do here. Value reads
		 * directly from Device.Hint file */
	}
	else {
		/* Set to max to determine the value */
		*value = 0XFFFF;
	}

	DBG_FUNC("OUT\n");
}

static void smartpqi_read_all_device_hint_file_entries(struct pqisrc_softstate *softs)
{
	uint32_t value = 0;

	DBG_FUNC("IN\n");

	/* hint.smartpqi.0.stream_disable =  "0" */
	read_device_hint_resource(softs, STREAM_DETECTION, &value);
	softs->hint.stream_status = value;

	/* hint.smartpqi.0.sata_unique_wwn_disable =  "0" */
	read_device_hint_resource(softs, SATA_UNIQUE_WWN, &value);
	softs->hint.sata_unique_wwn_status = value;

	/* hint.smartpqi.0.aio_raid1_write_disable =  "0" */
	read_device_hint_resource(softs, AIO_RAID1_WRITE_BYPASS, &value);
	softs->hint.aio_raid1_write_status = value;

	/* hint.smartpqi.0.aio_raid5_write_disable =  "0" */
	read_device_hint_resource(softs, AIO_RAID5_WRITE_BYPASS, &value);
	softs->hint.aio_raid5_write_status = value;

	/* hint.smartpqi.0.aio_raid6_write_disable =  "0" */
	read_device_hint_resource(softs, AIO_RAID6_WRITE_BYPASS, &value);
	softs->hint.aio_raid6_write_status = value;

	/* hint.smartpqi.0.queue_depth =  "0" */
	read_device_hint_decimal_value(softs, ADAPTER_QUEUE_DEPTH, &value);
	softs->hint.queue_depth = value;

	/* hint.smartpqi.0.sg_count =  "0" */
	read_device_hint_decimal_value(softs, SCATTER_GATHER_COUNT, &value);
	softs->hint.sg_segments = value;

	/* hint.smartpqi.0.queue_count =  "0" */
	read_device_hint_decimal_value(softs, QUEUE_COUNT, &value);
	softs->hint.cpu_count = value;

	DBG_FUNC("IN\n");
}


/*
 * Allocate resources for our device, set up the bus interface.
 * Initialize the PQI related functionality, scan devices, register sim to
 * upper layer, create management interface device node etc.
 */
static int
smartpqi_attach(device_t dev)
{
	struct pqisrc_softstate *softs;
	struct pqi_ident *id = NULL;
	int error = BSD_SUCCESS;
	u_int32_t command = 0, i = 0;
	int card_index = device_get_unit(dev);
	rcb_t *rcbp = NULL;

	/*
	 * Initialise softc.
	 */
	softs = device_get_softc(dev);

	if (!softs) {
		printf("Could not get softc\n");
		error = EINVAL;
		goto out;
	}
	memset(softs, 0, sizeof(*softs));
	softs->os_specific.pqi_dev = dev;

	DBG_FUNC("IN\n");

	/* assume failure is 'not configured' */
	error = ENXIO;

	/*
	 * Verify that the adapter is correctly set up in PCI space.
	 */
	pci_enable_busmaster(softs->os_specific.pqi_dev);
	command = pci_read_config(softs->os_specific.pqi_dev, PCIR_COMMAND, 2);
	if ((command & PCIM_CMD_MEMEN) == 0) {
		DBG_ERR("memory window not available command = %d\n", command);
		error = ENXIO;
		goto out;
	}

	/*
	 * Detect the hardware interface version, set up the bus interface
	 * indirection.
	 */
	id = pqi_find_ident(dev);
	if (!id) {
		DBG_ERR("NULL return value from pqi_find_ident\n");
		goto out;
	}

	softs->os_specific.pqi_hwif = id->hwif;

	switch(softs->os_specific.pqi_hwif) {
		case PQI_HWIF_SRCV:
			DBG_INFO("set hardware up for PMC SRCv for %p\n", softs);
			break;
		default:
			softs->os_specific.pqi_hwif = PQI_HWIF_UNKNOWN;
			DBG_ERR("unknown hardware type\n");
			error = ENXIO;
			goto out;
	}

	pqisrc_save_controller_info(softs);

	/*
	 * Allocate the PCI register window.
	 */
	softs->os_specific.pqi_regs_rid0 = PCIR_BAR(0);
	if ((softs->os_specific.pqi_regs_res0 =
		bus_alloc_resource_any(softs->os_specific.pqi_dev, SYS_RES_MEMORY,
		&softs->os_specific.pqi_regs_rid0, RF_ACTIVE)) == NULL) {
		DBG_ERR("couldn't allocate register window 0\n");
		/* assume failure is 'out of memory' */
		error = ENOMEM;
		goto out;
	}

	bus_get_resource_start(softs->os_specific.pqi_dev, SYS_RES_MEMORY,
		softs->os_specific.pqi_regs_rid0);

	softs->pci_mem_handle.pqi_btag = rman_get_bustag(softs->os_specific.pqi_regs_res0);
	softs->pci_mem_handle.pqi_bhandle = rman_get_bushandle(softs->os_specific.pqi_regs_res0);
	/* softs->pci_mem_base_vaddr = (uintptr_t)rman_get_virtual(softs->os_specific.pqi_regs_res0); */
	softs->pci_mem_base_vaddr = (char *)rman_get_virtual(softs->os_specific.pqi_regs_res0);

	/*
	 * Allocate the parent bus DMA tag appropriate for our PCI interface.
	 *
	 * Note that some of these controllers are 64-bit capable.
	 */
	if (bus_dma_tag_create(bus_get_dma_tag(dev), 	/* parent */
				PAGE_SIZE, 0,		/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,/* lowaddr */
				BUS_SPACE_MAXADDR, 	/* highaddr */
				NULL, NULL, 		/* filter, filterarg */
				BUS_SPACE_MAXSIZE,	/* maxsize */
				BUS_SPACE_UNRESTRICTED,	/* nsegments */
				BUS_SPACE_MAXSIZE,	/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* No locking needed */
				&softs->os_specific.pqi_parent_dmat)) {
		DBG_ERR("can't allocate parent DMA tag\n");
		/* assume failure is 'out of memory' */
		error = ENOMEM;
		goto dma_out;
	}

	softs->os_specific.sim_registered = FALSE;
	softs->os_name = "FreeBSD ";

	smartpqi_read_all_device_hint_file_entries(softs);

	/* Initialize the PQI library */
	error = pqisrc_init(softs);
	if (error != PQI_STATUS_SUCCESS) {
		DBG_ERR("Failed to initialize pqi lib error = %d\n", error);
		error = ENXIO;
		goto out;
	}
	else {
		error = BSD_SUCCESS;
	}

        mtx_init(&softs->os_specific.cam_lock, "cam_lock", NULL, MTX_DEF);
        softs->os_specific.mtx_init = TRUE;
        mtx_init(&softs->os_specific.map_lock, "map_lock", NULL, MTX_DEF);

	callout_init(&softs->os_specific.wellness_periodic, 1);
	callout_init(&softs->os_specific.heartbeat_timeout_id, 1);

        /*
         * Create DMA tag for mapping buffers into controller-addressable space.
         */
        if (bus_dma_tag_create(softs->os_specific.pqi_parent_dmat,/* parent */
				PAGE_SIZE, 0,		/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				(bus_size_t)softs->pqi_cap.max_sg_elem*PAGE_SIZE,/* maxsize */
				softs->pqi_cap.max_sg_elem,	/* nsegments */
				BUS_SPACE_MAXSIZE,	/* maxsegsize */
				BUS_DMA_ALLOCNOW,		/* flags */
				busdma_lock_mutex,		/* lockfunc */
				&softs->os_specific.map_lock,	/* lockfuncarg*/
				&softs->os_specific.pqi_buffer_dmat)) {
		DBG_ERR("can't allocate buffer DMA tag for pqi_buffer_dmat\n");
		return (ENOMEM);
        }

	rcbp = &softs->rcb[1];
	for( i = 1;  i <= softs->pqi_cap.max_outstanding_io; i++, rcbp++ ) {
		if ((error = bus_dmamap_create(softs->os_specific.pqi_buffer_dmat, 0, &rcbp->cm_datamap)) != 0) {
			DBG_ERR("Cant create datamap for buf @"
			"rcbp = %p maxio = %u error = %d\n",
			rcbp, softs->pqi_cap.max_outstanding_io, error);
			goto dma_out;
		}
	}

	os_start_heartbeat_timer((void *)softs); /* Start the heart-beat timer */
	callout_reset(&softs->os_specific.wellness_periodic, 120 * hz,
			os_wellness_periodic, softs);

	error = pqisrc_scan_devices(softs);
	if (error != PQI_STATUS_SUCCESS) {
		DBG_ERR("Failed to scan lib error = %d\n", error);
		error = ENXIO;
		goto out;
	}
	else {
		error = BSD_SUCCESS;
	}

	error = register_sim(softs, card_index);
	if (error) {
		DBG_ERR("Failed to register sim index = %d error = %d\n",
			card_index, error);
		goto out;
	}

	smartpqi_target_rescan(softs);

	TASK_INIT(&softs->os_specific.event_task, 0, pqisrc_event_worker,softs);

	error = create_char_dev(softs, card_index);
	if (error) {
		DBG_ERR("Failed to register character device index=%d r=%d\n",
			card_index, error);
		goto out;
	}

	goto out;

dma_out:
	if (softs->os_specific.pqi_regs_res0 != NULL)
		bus_release_resource(softs->os_specific.pqi_dev, SYS_RES_MEMORY,
			softs->os_specific.pqi_regs_rid0,
			softs->os_specific.pqi_regs_res0);
out:
	DBG_FUNC("OUT error = %d\n", error);

	return(error);
}

/*
 * Deallocate resources for our device.
 */
static int
smartpqi_detach(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);
	int rval = BSD_SUCCESS;

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	/* kill the periodic event */
	callout_drain(&softs->os_specific.wellness_periodic);
	/* Kill the heart beat event */
	callout_drain(&softs->os_specific.heartbeat_timeout_id);

	if (!pqisrc_ctrl_offline(softs)) {
		rval = pqisrc_flush_cache(softs, PQISRC_NONE_CACHE_FLUSH_ONLY);
		if (rval != PQI_STATUS_SUCCESS) {
			DBG_ERR("Unable to flush adapter cache! rval = %d\n", rval);
			rval = EIO;
		} else {
			rval = BSD_SUCCESS;
		}
	}

	destroy_char_dev(softs);
	pqisrc_uninit(softs);
	deregister_sim(softs);
	pci_release_msi(dev);

	DBG_FUNC("OUT\n");

	return rval;
}

/*
 * Bring the controller to a quiescent state, ready for system suspend.
 */
static int
smartpqi_suspend(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	DBG_INFO("Suspending the device %p\n", softs);
	softs->os_specific.pqi_state |= SMART_STATE_SUSPEND;

	DBG_FUNC("OUT\n");

	return BSD_SUCCESS;
}

/*
 * Bring the controller back to a state ready for operation.
 */
static int
smartpqi_resume(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	softs->os_specific.pqi_state &= ~SMART_STATE_SUSPEND;

	DBG_FUNC("OUT\n");

	return BSD_SUCCESS;
}

/*
 * Do whatever is needed during a system shutdown.
 */
static int
smartpqi_shutdown(device_t dev)
{
	struct pqisrc_softstate *softs = device_get_softc(dev);
	int bsd_status = BSD_SUCCESS;
	int pqi_status;

	DBG_FUNC("IN\n");

	if (softs == NULL)
		return ENXIO;

	if (pqisrc_ctrl_offline(softs))
		return BSD_SUCCESS;

	pqi_status = pqisrc_flush_cache(softs, PQISRC_SHUTDOWN);
	if (pqi_status != PQI_STATUS_SUCCESS) {
		DBG_ERR("Unable to flush adapter cache! rval = %d\n", pqi_status);
		bsd_status = EIO;
	}

	DBG_FUNC("OUT\n");

	return bsd_status;
}


/*
 * PCI bus interface.
 */
static device_method_t pqi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		smartpqi_probe),
	DEVMETHOD(device_attach,	smartpqi_attach),
	DEVMETHOD(device_detach,	smartpqi_detach),
	DEVMETHOD(device_suspend,	smartpqi_suspend),
	DEVMETHOD(device_resume,	smartpqi_resume),
	DEVMETHOD(device_shutdown,	smartpqi_shutdown),
	{ 0, 0 }
};

static driver_t smartpqi_pci_driver = {
	"smartpqi",
	pqi_methods,
	sizeof(struct pqisrc_softstate)
};

DRIVER_MODULE(smartpqi, pci, smartpqi_pci_driver, 0, 0);
MODULE_DEPEND(smartpqi, pci, 1, 1, 1);
