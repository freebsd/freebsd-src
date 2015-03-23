/*
 * This file is part of the Chelsio T4 Ethernet driver.
 *
 * Copyright (C) 2003-2014 Chelsio Communications.  All rights reserved.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the LICENSE file included in this
 * release for licensing terms and conditions.
 */
#ifndef __T4_CHIP_TYPE_H__
#define __T4_CHIP_TYPE_H__

/*
 * All T4 and later chips have their PCI-E Device IDs encoded as 0xVFPP where:
 *
 *   V  = "4" for T4; "5" for T5, etc. or
 *      = "a" for T4 FPGA; "b" for T4 FPGA, etc.
 *   F  = "0" for PF 0..3; "4".."7" for PF4..7; and "8" for VFs
 *   PP = adapter product designation
 *
 * We use the "version" (V) of the adpater to code the Chip Version above
 * but separate out the FPGA as a separate boolean as per above.
 */
#define CHELSIO_PCI_ID_VER(__DeviceID)	((__DeviceID) >> 12)
#define CHELSIO_PCI_ID_FUNC(__DeviceID)	(((__DeviceID) >> 8) & 0xf)
#define CHELSIO_PCI_ID_PROD(__DeviceID)	((__DeviceID) & 0xff)

#define CHELSIO_T4		0x4
#define CHELSIO_T4_FPGA		0xa
#define CHELSIO_T5		0x5
#define CHELSIO_T5_FPGA		0xb

/*
 * Translate a PCI Device ID to a base Chelsio Chip Version -- CHELSIO_T4,
 * CHELSIO_T5, etc.  If it weren't for the screwed up numbering of the FPGAs
 * we could do this simply as DeviceID >> 12 (because we know the real
 * encoding oc CHELSIO_Tx identifiers).  However, the FPGAs _do_ have weird
 * Device IDs so we need to do this translation here.  Note that only constant
 * arithmetic and comparisons can be done here since this is being used to
 * initialize static tables, etc.
 *
 * Finally: This will of course need to be expanded as future chips are
 * developed.
 */
#define CHELSIO_PCI_ID_CHIP_VERSION(__DeviceID) \
	(CHELSIO_PCI_ID_VER(__DeviceID) == CHELSIO_T4 || \
	 CHELSIO_PCI_ID_VER(__DeviceID) == CHELSIO_T4_FPGA \
	 ? CHELSIO_T4 \
	 : CHELSIO_T5)

/*
 * Internally we code the Chelsio T4 Family "Chip Code" as a tuple:
 *
 *     (Is FPGA, Chip Version, Chip Revision)
 *
 * where:
 *
 *     Is FPGA: is 0/1 indicating whether we're working with an FPGA
 *     Chip Version: is T4, T5, etc.
 *     Chip Revision: is the FAB "spin" of the Chip Version.
 */
#define CHELSIO_CHIP_CODE(version, revision) (((version) << 4) | (revision))
#define CHELSIO_CHIP_FPGA          0x100
#define CHELSIO_CHIP_VERSION(code) (((code) >> 4) & 0xf)
#define CHELSIO_CHIP_RELEASE(code) ((code) & 0xf)

enum chip_type {
	T4_A1 = CHELSIO_CHIP_CODE(CHELSIO_T4, 1),
	T4_A2 = CHELSIO_CHIP_CODE(CHELSIO_T4, 2),
	T4_FIRST_REV	= T4_A1,
	T4_LAST_REV	= T4_A2,

	T5_A0 = CHELSIO_CHIP_CODE(CHELSIO_T5, 0),
	T5_A1 = CHELSIO_CHIP_CODE(CHELSIO_T5, 0),
	T5_FIRST_REV	= T5_A0,
	T5_LAST_REV	= T5_A1,
};

static inline int is_t4(enum chip_type chip)
{
	return (CHELSIO_CHIP_VERSION(chip) == CHELSIO_T4);
}

static inline int is_t5(enum chip_type chip)
{

	return (CHELSIO_CHIP_VERSION(chip) == CHELSIO_T5);
}

static inline int is_fpga(enum chip_type chip)
{
	 return chip & CHELSIO_CHIP_FPGA;
}

#endif /* __T4_CHIP_TYPE_H__ */
