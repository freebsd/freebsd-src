/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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

#ifndef _SIBA_SIBA_IDS_H_
#define	_SIBA_SIBA_IDS_H_

/*
 * Constants and structures for SiBa bus enumeration.
 */

struct siba_devid {
	uint16_t	 sd_vendor;
	uint16_t	 sd_device;
	uint8_t		 sd_rev;
	char		*sd_desc;
};
#define	SIBA_DEV(_vendor, _cid, _rev, _msg)			\
	{ SIBA_VID_##_vendor, SIBA_DEVID_##_cid, _rev, _msg }

/*
 * Device IDs
 */
#define SIBA_DEVID_ANY			0xffff
#define	SIBA_DEVID_CHIPCOMMON		0x800
#define	SIBA_DEVID_ILINE20		0x801
#define	SIBA_DEVID_SDRAM		0x803
#define	SIBA_DEVID_PCI			0x804
#define	SIBA_DEVID_MIPS			0x805
#define	SIBA_DEVID_ETHERNET		0x806
#define	SIBA_DEVID_MODEM		0x807
#define	SIBA_DEVID_USB11_HOSTDEV	0x808
#define	SIBA_DEVID_ADSL			0x809
#define	SIBA_DEVID_ILINE100		0x80a
#define	SIBA_DEVID_IPSEC		0x80b
#define	SIBA_DEVID_PCMCIA		0x80d
#define	SIBA_DEVID_INTERNAL_MEM		0x80e
#define	SIBA_DEVID_SDRAMDDR		0x80f
#define	SIBA_DEVID_EXTIF		0x811
#define	SIBA_DEVID_80211		0x812
#define	SIBA_DEVID_MIPS_3302		0x816
#define	SIBA_DEVID_USB11_HOST		0x817
#define	SIBA_DEVID_USB11_DEV		0x818
#define	SIBA_DEVID_USB20_HOST		0x819
#define	SIBA_DEVID_USB20_DEV		0x81a
#define	SIBA_DEVID_SDIO_HOST		0x81b
#define	SIBA_DEVID_ROBOSWITCH		0x81c
#define	SIBA_DEVID_PARA_ATA		0x81d
#define	SIBA_DEVID_SATA_XORDMA		0x81e
#define	SIBA_DEVID_ETHERNET_GBIT	0x81f
#define	SIBA_DEVID_PCIE			0x820
#define	SIBA_DEVID_MIMO_PHY		0x821
#define	SIBA_DEVID_SRAM_CTRLR		0x822
#define	SIBA_DEVID_MINI_MACPHY		0x823
#define	SIBA_DEVID_ARM_1176		0x824
#define	SIBA_DEVID_ARM_7TDMI		0x825

/*
 * Vendor IDs
 */
#define SIBA_VID_ANY		0xffff
#define SIBA_VID_BROADCOM	0x4243

/*
 * Revision IDs
 */
#define SIBA_REV_ANY		0xff

#endif /*_SIBA_SIBA_IDS_H_ */
