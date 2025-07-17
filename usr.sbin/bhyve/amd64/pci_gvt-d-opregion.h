/** @file
  IGD OpRegion definition from Intel Integrated Graphics Device OpRegion
  Specification.

  https://01.org/sites/default/files/documentation/skl_opregion_rev0p5.pdf

  Copyright (c) 2016 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

/*
 * See
 * <https://github.com/tianocore/edk2-platforms/blob/82979ab1ca44101e0b92a9c4bda1dfe64a8249f6/Silicon/Intel/IntelSiliconPkg/Include/IndustryStandard/IgdOpRegion.h>
 */

#pragma once

#include <sys/types.h>

#define IGD_OPREGION_HEADER_SIGN "IntelGraphicsMem"
#define IGD_OPREGION_HEADER_MBOX1 BIT0
#define IGD_OPREGION_HEADER_MBOX2 BIT1
#define IGD_OPREGION_HEADER_MBOX3 BIT2
#define IGD_OPREGION_HEADER_MBOX4 BIT3
#define IGD_OPREGION_HEADER_MBOX5 BIT4

#define IGD_OPREGION_VBT_SIZE_6K (6 * 1024UL)

/**
  OpRegion structures:
  Sub-structures define the different parts of the OpRegion followed by the
  main structure representing the entire OpRegion.
  @note These structures are packed to 1 byte offsets because the exact
  data location is required by the supporting design specification due to
  the fact that the data is used by ASL and Graphics driver code compiled
  separately.
**/

///
/// OpRegion Mailbox 0 Header structure. The OpRegion Header is used to
/// identify a block of memory as the graphics driver OpRegion.
/// Offset 0x0, Size 0x100
///
struct igd_opregion_header {
	int8_t sign[0x10];  ///< Offset 0x00 OpRegion Signature
	uint32_t size;	    ///< Offset 0x10 OpRegion Size
	uint32_t over;	    ///< Offset 0x14 OpRegion Structure Version
	uint8_t sver[0x20]; ///< Offset 0x18 System BIOS Build Version
	uint8_t vver[0x10]; ///< Offset 0x38 Video BIOS Build Version
	uint8_t gver[0x10]; ///< Offset 0x48 Graphic Driver Build Version
	uint32_t mbox;	    ///< Offset 0x58 Supported Mailboxes
	uint32_t dmod;	    ///< Offset 0x5C Driver Model
	uint32_t pcon;	    ///< Offset 0x60 Platform Configuration
	int16_t dver[0x10]; ///< Offset 0x64 GOP Version
	uint8_t rm01[0x7C]; ///< Offset 0x84 Reserved Must be zero
} __packed;

///
/// OpRegion Mailbox 1 - Public ACPI Methods
/// Offset 0x100, Size 0x100
///
struct igd_opregion_mbox1 {
	uint32_t drdy;	    ///< Offset 0x100 Driver Readiness
	uint32_t csts;	    ///< Offset 0x104 Status
	uint32_t cevt;	    ///< Offset 0x108 Current Event
	uint8_t rm11[0x14]; ///< Offset 0x10C Reserved Must be Zero
	uint32_t didl[8];   ///< Offset 0x120 Supported Display Devices ID List
	uint32_t
	    cpdl[8]; ///< Offset 0x140 Currently Attached Display Devices List
	uint32_t
	    cadl[8]; ///< Offset 0x160 Currently Active Display Devices List
	uint32_t nadl[8];   ///< Offset 0x180 Next Active Devices List
	uint32_t aslp;	    ///< Offset 0x1A0 ASL Sleep Time Out
	uint32_t tidx;	    ///< Offset 0x1A4 Toggle Table Index
	uint32_t chpd;	    ///< Offset 0x1A8 Current Hotplug Enable Indicator
	uint32_t clid;	    ///< Offset 0x1AC Current Lid State Indicator
	uint32_t cdck;	    ///< Offset 0x1B0 Current Docking State Indicator
	uint32_t sxsw;	    ///< Offset 0x1B4 Display Switch Notification on Sx
			    ///< StateResume
	uint32_t evts;	    ///< Offset 0x1B8 Events supported by ASL
	uint32_t cnot;	    ///< Offset 0x1BC Current OS Notification
	uint32_t NRDY;	    ///< Offset 0x1C0 Driver Status
	uint8_t did2[0x1C]; ///< Offset 0x1C4 Extended Supported Devices ID
			    ///< List(DOD)
	uint8_t
	    cpd2[0x1C]; ///< Offset 0x1E0 Extended Attached Display Devices List
	uint8_t rm12[4]; ///< Offset 0x1FC - 0x1FF Reserved Must be zero
} __packed;

///
/// OpRegion Mailbox 2 - Software SCI Interface
/// Offset 0x200, Size 0x100
///
struct igd_opregion_mbox2 {
	uint32_t scic; ///< Offset 0x200 Software SCI Command / Status / Data
	uint32_t parm; ///< Offset 0x204 Software SCI Parameters
	uint32_t dslp; ///< Offset 0x208 Driver Sleep Time Out
	uint8_t rm21[0xF4]; ///< Offset 0x20C - 0x2FF Reserved Must be zero
} __packed;

///
/// OpRegion Mailbox 3 - BIOS/Driver Notification - ASLE Support
/// Offset 0x300, Size 0x100
///
struct igd_opregion_mbox3 {
	uint32_t ardy;	     ///< Offset 0x300 Driver Readiness
	uint32_t aslc;	     ///< Offset 0x304 ASLE Interrupt Command / Status
	uint32_t tche;	     ///< Offset 0x308 Technology Enabled Indicator
	uint32_t alsi;	     ///< Offset 0x30C Current ALS Luminance Reading
	uint32_t bclp;	     ///< Offset 0x310 Requested Backlight Brightness
	uint32_t pfit;	     ///< Offset 0x314 Panel Fitting State or Request
	uint32_t cblv;	     ///< Offset 0x318 Current Brightness Level
	uint16_t bclm[0x14]; ///< Offset 0x31C Backlight Brightness Levels Duty
			     ///< Cycle Mapping Table
	uint32_t cpfm;	     ///< Offset 0x344 Current Panel Fitting Mode
	uint32_t epfm;	     ///< Offset 0x348 Enabled Panel Fitting Modes
	uint8_t plut[0x4A];  ///< Offset 0x34C Panel Look Up Table & Identifier
	uint32_t pfmb; ///< Offset 0x396 PWM Frequency and Minimum Brightness
	uint32_t ccdv; ///< Offset 0x39A Color Correction Default Values
	uint32_t pcft; ///< Offset 0x39E Power Conservation Features
	uint32_t srot; ///< Offset 0x3A2 Supported Rotation Angles
	uint32_t iuer; ///< Offset 0x3A6 Intel Ultrabook(TM) Event Register
	uint64_t fdss; ///< Offset 0x3AA DSS Buffer address allocated for IFFS
		       ///< feature
	uint32_t fdsp; ///< Offset 0x3B2 Size of DSS buffer
	uint32_t stat; ///< Offset 0x3B6 State Indicator
	uint64_t rvda; ///< Offset 0x3BA Absolute/Relative Address of Raw VBT
		       ///< Data from OpRegion Base
	uint32_t rvds;	     ///< Offset 0x3C2 Raw VBT Data Size
	uint8_t rsvd2[0x3A]; ///< Offset 0x3C6 - 0x3FF  Reserved Must be zero.
			     ///< Bug in spec 0x45(69)
} __packed;

///
/// OpRegion Mailbox 4 - VBT Video BIOS Table
/// Offset 0x400, Size 0x1800
///
struct igd_opregion_mbox4 {
	uint8_t rvbt[IGD_OPREGION_VBT_SIZE_6K]; ///< Offset 0x400 - 0x1BFF Raw
						///< VBT Data
} __packed;

///
/// OpRegion Mailbox 5 - BIOS/Driver Notification - Data storage BIOS to Driver
/// data sync Offset 0x1C00, Size 0x400
///
struct igd_opregion_mbox5 {
	uint32_t phed;	     ///< Offset 0x1C00 Panel Header
	uint8_t bddc[0x100]; ///< Offset 0x1C04 Panel EDID (DDC data)
	uint8_t rm51[0x2FC]; ///< Offset 0x1D04 - 0x1FFF Reserved Must be zero
} __packed;

///
/// IGD OpRegion Structure
///
struct igd_opregion {
	struct igd_opregion_header
	    header; ///< OpRegion header (Offset 0x0, Size 0x100)
	struct igd_opregion_mbox1 mbox1; ///< Mailbox 1: Public ACPI Methods
					 ///< (Offset 0x100, Size 0x100)
	struct igd_opregion_mbox2 mbox2; ///< Mailbox 2: Software SCI Interface
					 ///< (Offset 0x200, Size 0x100)
	struct igd_opregion_mbox3
	    mbox3; ///< Mailbox 3: BIOS to Driver Notification (Offset 0x300,
		   ///< Size 0x100)
	struct igd_opregion_mbox4 mbox4; ///< Mailbox 4: Video BIOS Table (VBT)
					 ///< (Offset 0x400, Size 0x1800)
	struct igd_opregion_mbox5
	    mbox5; ///< Mailbox 5: BIOS to Driver Notification Extension (Offset
		   ///< 0x1C00, Size 0x400)
} __packed;

///
/// VBT Header Structure
///
struct vbt_header {
	uint8_t product_string[20];
	uint16_t version;
	uint16_t header_size;
	uint16_t table_size;
	uint8_t checksum;
	uint8_t reserved1;
	uint32_t bios_data_offset;
	uint32_t aim_data_offset[4];
} __packed;
