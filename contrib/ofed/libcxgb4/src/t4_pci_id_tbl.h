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
#ifndef __T4_PCI_ID_TBL_H__
#define __T4_PCI_ID_TBL_H__

/*
 * The Os-Dependent code can defined cpp macros for creating a PCI Device ID
 * Table.  This is useful because it allows the PCI ID Table to be maintained
 * in a single place and all supporting OSes to get new PCI Device IDs
 * automatically.
 *
 * The macros are:
 *
 * CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN
 *   -- Used to start the definition of the PCI ID Table.
 *
 * CH_PCI_DEVICE_ID_FUNCTION
 *   -- The PCI Function Number to use in the PCI Device ID Table.  "0"
 *   -- for drivers attaching to PF0-3, "4" for drivers attaching to PF4,
 *   -- "8" for drivers attaching to SR-IOV Virtual Functions, etc.
 *
 * CH_PCI_DEVICE_ID_FUNCTION2 [optional]
 *   -- If defined, create a PCI Device ID Table with both
 *   -- CH_PCI_DEVICE_ID_FUNCTION and CH_PCI_DEVICE_ID_FUNCTION2 populated.
 *
 * CH_PCI_ID_TABLE_ENTRY(DeviceID)
 *   -- Used for the individual PCI Device ID entries.  Note that we will
 *   -- be adding a trailing comma (",") after all of the entries (and
 *   -- between the pairs of entries if CH_PCI_DEVICE_ID_FUNCTION2 is defined).
 *
 * CH_PCI_DEVICE_ID_TABLE_DEFINE_END
 *   -- Used to finish the definition of the PCI ID Table.  Note that we
 *   -- will be adding a trailing semi-colon (";") here.
 *
 * CH_PCI_DEVICE_ID_BYPASS_SUPPORTED [optional]
 *   -- If defined, indicates that the OS Driver has support for Bypass
 *   -- Adapters.
 */
#ifdef CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN

/*
 * Some sanity checks ...
 */
#ifndef CH_PCI_DEVICE_ID_FUNCTION
#error CH_PCI_DEVICE_ID_FUNCTION not defined!
#endif
#ifndef CH_PCI_ID_TABLE_ENTRY
#error CH_PCI_ID_TABLE_ENTRY not defined!
#endif
#ifndef CH_PCI_DEVICE_ID_TABLE_DEFINE_END
#error CH_PCI_DEVICE_ID_TABLE_DEFINE_END not defined!
#endif

/*
 * T4 and later ASICs use a PCI Device ID scheme of 0xVFPP where:
 *
 *   V  = "4" for T4; "5" for T5, etc.
 *   F  = "0" for PF 0..3; "4".."7" for PF4..7; and "8" for VFs
 *   PP = adapter product designation
 *
 * We use this consistency in order to create the proper PCI Device IDs
 * for the specified CH_PCI_DEVICE_ID_FUNCTION.
 */
#ifndef CH_PCI_DEVICE_ID_FUNCTION2
#define CH_PCI_ID_TABLE_FENTRY(__DeviceID) \
	CH_PCI_ID_TABLE_ENTRY((__DeviceID) | \
			      ((CH_PCI_DEVICE_ID_FUNCTION) << 8))
#else
#define CH_PCI_ID_TABLE_FENTRY(__DeviceID) \
	CH_PCI_ID_TABLE_ENTRY((__DeviceID) | \
			      ((CH_PCI_DEVICE_ID_FUNCTION) << 8)), \
	CH_PCI_ID_TABLE_ENTRY((__DeviceID) | \
			      ((CH_PCI_DEVICE_ID_FUNCTION2) << 8))
#endif

/* Note : The comments against each entry are used by the scripts in the vmware drivers
 * to correctly generate the pciid xml file, do not change the format currently used.
 */

CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN
	/*
	 * FPGAs:
	 *
	 * Unfortunately the FPGA PCI Device IDs don't follow the ASIC PCI
	 * Device ID numbering convetions for the Physical Functions.
	 */
#if CH_PCI_DEVICE_ID_FUNCTION != 8
	CH_PCI_ID_TABLE_ENTRY(0xa000),	/* PE10K FPGA */
	CH_PCI_ID_TABLE_ENTRY(0xb000),	/* PF0 T5 PE10K5 FPGA */
	CH_PCI_ID_TABLE_ENTRY(0xb001),	/* PF0 T5 PE10K FPGA */
#else
	CH_PCI_ID_TABLE_FENTRY(0xa000),	/* PE10K FPGA */
	CH_PCI_ID_TABLE_FENTRY(0xb000),	/* PF0 T5 PE10K5 FPGA */
	CH_PCI_ID_TABLE_FENTRY(0xb001),	/* PF0 T5 PE10K FPGA */
#endif

	/*
	 *  These FPGAs seem to be used only by the csiostor driver
	 */
#if ((CH_PCI_DEVICE_ID_FUNCTION == 5) || (CH_PCI_DEVICE_ID_FUNCTION == 6))
	CH_PCI_ID_TABLE_ENTRY(0xa001),	/* PF1 PE10K FPGA FCOE */
	CH_PCI_ID_TABLE_ENTRY(0xa002),	/* PE10K FPGA iSCSI */
#endif

	/*
	 * T4 adapters:
	 */
	CH_PCI_ID_TABLE_FENTRY(0x4000),	/* T440-dbg */
	CH_PCI_ID_TABLE_FENTRY(0x4001),	/* T420-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4002),	/* T422-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4003),	/* T440-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4004),	/* T420-bch */
	CH_PCI_ID_TABLE_FENTRY(0x4005),	/* T440-bch */
	CH_PCI_ID_TABLE_FENTRY(0x4006),	/* T440-ch */
	CH_PCI_ID_TABLE_FENTRY(0x4007),	/* T420-so */
	CH_PCI_ID_TABLE_FENTRY(0x4008),	/* T420-cx */
	CH_PCI_ID_TABLE_FENTRY(0x4009),	/* T420-bt */
	CH_PCI_ID_TABLE_FENTRY(0x400a),	/* T404-bt */
#ifdef CH_PCI_DEVICE_ID_BYPASS_SUPPORTED
	CH_PCI_ID_TABLE_FENTRY(0x400b),	/* B420-sr */
	CH_PCI_ID_TABLE_FENTRY(0x400c),	/* B404-bt */
#endif
	CH_PCI_ID_TABLE_FENTRY(0x400d),	/* T480-cr */
	CH_PCI_ID_TABLE_FENTRY(0x400e),	/* T440-LP-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4080),	/* Custom T480-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4081),	/* Custom T440-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4082),	/* Custom T420-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4083),	/* Custom T420-xaui */
	CH_PCI_ID_TABLE_FENTRY(0x4084),	/* Custom T440-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4085),	/* Custom T420-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4086),	/* Custom T440-bt */
	CH_PCI_ID_TABLE_FENTRY(0x4087),	/* Custom T440-cr */
	CH_PCI_ID_TABLE_FENTRY(0x4088),	/* Custom T440 2-xaui, 2-xfi */

	/*
	 * T5 adapters:
	 */
	CH_PCI_ID_TABLE_FENTRY(0x5000),	/* T580-dbg */
	CH_PCI_ID_TABLE_FENTRY(0x5001),	/* T520-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5002),	/* T522-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5003),	/* T540-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5004),	/* T520-bch */
	CH_PCI_ID_TABLE_FENTRY(0x5005),	/* T540-bch */
	CH_PCI_ID_TABLE_FENTRY(0x5006),	/* T540-ch */
	CH_PCI_ID_TABLE_FENTRY(0x5007),	/* T520-so */
	CH_PCI_ID_TABLE_FENTRY(0x5008),	/* T520-cx */
	CH_PCI_ID_TABLE_FENTRY(0x5009),	/* T520-bt */
	CH_PCI_ID_TABLE_FENTRY(0x500a),	/* T504-bt */
#ifdef CH_PCI_DEVICE_ID_BYPASS_SUPPORTED
	CH_PCI_ID_TABLE_FENTRY(0x500b),	/* B520-sr */
	CH_PCI_ID_TABLE_FENTRY(0x500c),	/* B504-bt */
#endif
	CH_PCI_ID_TABLE_FENTRY(0x500d),	/* T580-cr */
	CH_PCI_ID_TABLE_FENTRY(0x500e),	/* T540-LP-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5010),	/* T580-LP-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5011),	/* T520-LL-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5012),	/* T560-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5013),	/* T580-chr */
	CH_PCI_ID_TABLE_FENTRY(0x5014),	/* T580-so */
	CH_PCI_ID_TABLE_FENTRY(0x5015),	/* T502-bt */
	CH_PCI_ID_TABLE_FENTRY(0x5080),	/* Custom T540-cr */
	CH_PCI_ID_TABLE_FENTRY(0x5081),	/* Custom T540-LL-cr */
CH_PCI_DEVICE_ID_TABLE_DEFINE_END;

#endif /* CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN */

#endif /* __T4_PCI_ID_TBL_H__ */
