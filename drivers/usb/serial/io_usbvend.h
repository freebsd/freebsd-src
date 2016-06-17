/************************************************************************
 *
 *	USBVEND.H		Vendor-specific USB definitions
 *
 *	NOTE: This must be kept in sync with the Edgeport firmware and
 *	must be kept backward-compatible with older firmware.
 *
 ************************************************************************
 *
 *	Copyright (C) 1998 Inside Out Networks, Inc.
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 ************************************************************************/

#if !defined(_USBVEND_H)
#define	_USBVEND_H

#ifndef __KERNEL__
#include "ionprag.h"	/* Extra I/O Networks pragmas */

#include <usbdi.h>

#include "iondef.h"	/* Standard I/O Networks definitions */
#endif

/************************************************************************
 *
 *		D e f i n e s   /   T y p e d e f s
 *
 ************************************************************************/

//
// Definitions of USB product IDs
// 

#define	USB_VENDOR_ID_ION	0x1608		// Our VID

//
// Definitions of USB product IDs (PID)
// We break the USB-defined PID into an OEM Id field (upper 6 bits)
// and a Device Id (bottom 10 bits). The Device Id defines what
// device this actually is regardless of what the OEM wants to
// call it.
//

// ION-device OEM IDs
#define	ION_OEM_ID_ION		0		// 00h Inside Out Networks
#define	ION_OEM_ID_NLYNX	1		// 01h NLynx Systems	  
#define	ION_OEM_ID_GENERIC	2		// 02h Generic OEM
#define	ION_OEM_ID_MAC		3		// 03h Mac Version
#define	ION_OEM_ID_MEGAWOLF	4		// 04h Lupusb OEM Mac version (MegaWolf)
#define	ION_OEM_ID_MULTITECH	5		// 05h Multitech Rapidports

	
// ION-device Device IDs
// Product IDs - assigned to match middle digit of serial number


// The ION_DEVICE_ID_GENERATION_2 bit (0x20) will be ORed into the existing edgeport
// PIDs to identify 80251+Netchip hardware.  This will guarantee that if a second
// generation edgeport device is plugged into a PC with an older (pre 2.0) driver,
// it will not enumerate.

#define ION_DEVICE_ID_GENERATION_2	0x020	// This bit is set in the PID if this edgeport hardware
															// is based on the 80251+Netchip.  

#define EDGEPORT_DEVICE_ID_MASK			0x3df	// Not including GEN_2 bit

#define	ION_DEVICE_ID_UNCONFIGURED_EDGE_DEVICE	0x000	// In manufacturing only
#define ION_DEVICE_ID_EDGEPORT_4		0x001	// Edgeport/4 RS232
//	ION_DEVICE_ID_HUBPORT_7			0x002	// Hubport/7 (Placeholder, not used by software)
#define ION_DEVICE_ID_RAPIDPORT_4		0x003	// Rapidport/4
#define ION_DEVICE_ID_EDGEPORT_4T		0x004	// Edgeport/4 RS232 for Telxon (aka "Fleetport")
#define ION_DEVICE_ID_EDGEPORT_2		0x005	// Edgeport/2 RS232
#define ION_DEVICE_ID_EDGEPORT_4I		0x006	// Edgeport/4 RS422
#define ION_DEVICE_ID_EDGEPORT_2I		0x007	// Edgeport/2 RS422/RS485
//	ION_DEVICE_ID_HUBPORT_4			0x008	// Hubport/4 (Placeholder, not used by software)
//	ION_DEVICE_ID_EDGEPORT_8_HANDBUILT	0x009	// Hand-built Edgeport/8 (Placeholder, used in middle digit of serial number only!)
//	ION_DEVICE_ID_MULTIMODEM_4X56		0x00A	// MultiTech version of RP/4 (Placeholder, used in middle digit of serial number only!)
#define	ION_DEVICE_ID_EDGEPORT_PARALLEL_PORT	0x00B	// Edgeport/(4)21 Parallel port (USS720)
#define	ION_DEVICE_ID_EDGEPORT_421		0x00C	// Edgeport/421 Hub+RS232+Parallel
#define	ION_DEVICE_ID_EDGEPORT_21		0x00D	// Edgeport/21  RS232+Parallel
#define ION_DEVICE_ID_EDGEPORT_8_DUAL_CPU	0x00E	// Half of an Edgeport/8 (the kind with 2 EP/4s on 1 PCB)
#define ION_DEVICE_ID_EDGEPORT_8		0x00F	// Edgeport/8 (single-CPU)
#define ION_DEVICE_ID_EDGEPORT_2_DIN		0x010	// Edgeport/2 RS232 with Apple DIN connector
#define ION_DEVICE_ID_EDGEPORT_4_DIN		0x011	// Edgeport/4 RS232 with Apple DIN connector
#define ION_DEVICE_ID_EDGEPORT_16_DUAL_CPU	0x012	// Half of an Edgeport/16 (the kind with 2 EP/8s)
#define ION_DEVICE_ID_EDGEPORT_COMPATIBLE	0x013	// Edgeport Compatible, for NCR, Axiohm etc. testing
#define ION_DEVICE_ID_EDGEPORT_8I		0x014	// Edgeport/8 RS422 (single-CPU)
#define ION_DEVICE_ID_MT4X56USB			0x1403	// OEM device

// BlackBox OEM devices
#define ION_DEVICE_ID_BB_EDGEPORT_4		0x001	// Edgeport/4 RS232
#define ION_DEVICE_ID_BB_EDGEPORT_4T		0x004	// Edgeport/4 RS232 for Telxon (aka "Fleetport")
#define ION_DEVICE_ID_BB_EDGEPORT_2		0x005	// Edgeport/2 RS232
#define ION_DEVICE_ID_BB_EDGEPORT_4I		0x006	// Edgeport/4 RS422
#define ION_DEVICE_ID_BB_EDGEPORT_2I		0x007	// Edgeport/2 RS422/RS485
#define	ION_DEVICE_ID_BB_EDGEPORT_421		0x00C	// Edgeport/421 Hub+RS232+Parallel
#define	ION_DEVICE_ID_BB_EDGEPORT_21		0x00D	// Edgeport/21  RS232+Parallel
#define ION_DEVICE_ID_BB_EDGEPORT_8_DUAL_CPU	0x00E	// Half of an Edgeport/8 (the kind with 2 EP/4s on 1 PCB)
#define ION_DEVICE_ID_BB_EDGEPORT_8		0x00F	// Edgeport/8 (single-CPU)
#define ION_DEVICE_ID_BB_EDGEPORT_2_DIN		0x010	// Edgeport/2 RS232 with Apple DIN connector
#define ION_DEVICE_ID_BB_EDGEPORT_4_DIN		0x011	// Edgeport/4 RS232 with Apple DIN connector
#define ION_DEVICE_ID_BB_EDGEPORT_16_DUAL_CPU	0x012	// Half of an Edgeport/16 (the kind with 2 EP/8s)
#define ION_DEVICE_ID_BB_EDGEPORT_8I		0x014	// Edgeport/8 RS422 (single-CPU)


/* Edgeport TI based devices */
#define ION_DEVICE_ID_TI_EDGEPORT_4		0x0201	/* Edgeport/4 RS232 */
#define ION_DEVICE_ID_TI_EDGEPORT_2		0x0205	/* Edgeport/2 RS232 */
#define ION_DEVICE_ID_TI_EDGEPORT_4I		0x0206	/* Edgeport/4i RS422 */
#define ION_DEVICE_ID_TI_EDGEPORT_2I		0x0207	/* Edgeport/2i RS422/RS485 */
#define ION_DEVICE_ID_TI_EDGEPORT_421		0x020C	/* Edgeport/421 4 hub 2 RS232 + Parallel (lucent on a different hub port) */
#define ION_DEVICE_ID_TI_EDGEPORT_21		0x020D	/* Edgeport/21 2 RS232 + Parallel (lucent on a different hub port) */
#define ION_DEVICE_ID_TI_EDGEPORT_1		0x0215	/* Edgeport/1 RS232 */
#define ION_DEVICE_ID_TI_EDGEPORT_42		0x0217	/* Edgeport/42 4 hub 2 RS232 */
#define ION_DEVICE_ID_TI_EDGEPORT_22		0x021A	/* Edgeport/22  Edgeport/22I is an Edgeport/4 with ports 1&2 RS422 and ports 3&4 RS232 */
#define ION_DEVICE_ID_TI_EDGEPORT_2C		0x021B	/* Edgeport/2c RS232 */

#define ION_DEVICE_ID_TI_EDGEPORT_421_BOOT	0x0240	/* Edgeport/421 in boot mode */
#define ION_DEVICE_ID_TI_EDGEPORT_421_DOWN	0x0241	/* Edgeport/421 in download mode first interface is 2 RS232 (Note that the second interface of this multi interface device should be a standard USB class 7 printer port) */
#define ION_DEVICE_ID_TI_EDGEPORT_21_BOOT	0x0242	/* Edgeport/21 in boot mode */
#define ION_DEVICE_ID_TI_EDGEPORT_21_DOWN	0x0243	/*Edgeport/42 in download mode: first interface is 2 RS232 (Note that the second interface of this multi interface device should be a standard USB class 7 printer port) */


#define	MAKE_USB_PRODUCT_ID( OemId, DeviceId )					\
			( (__u16) (((OemId) << 10) || (DeviceId)) )

#define	DEVICE_ID_FROM_USB_PRODUCT_ID( ProductId )				\
			( (__u16) ((ProductId) & (EDGEPORT_DEVICE_ID_MASK)) )

#define	OEM_ID_FROM_USB_PRODUCT_ID( ProductId )					\
			( (__u16) (((ProductId) >> 10) & 0x3F) )

//
// Definitions of parameters for download code. Note that these are
// specific to a given version of download code and must change if the
// corresponding download code changes.
//

// TxCredits value below which driver won't bother sending (to prevent too many small writes).
// Send only if above 25%
#define EDGE_FW_GET_TX_CREDITS_SEND_THRESHOLD(InitialCredit)	(max(((InitialCredit) / 4), EDGE_FW_BULK_MAX_PACKET_SIZE))

#define	EDGE_FW_BULK_MAX_PACKET_SIZE		64	// Max Packet Size for Bulk In Endpoint (EP1)
#define EDGE_FW_BULK_READ_BUFFER_SIZE		1024	// Size to use for Bulk reads

#define	EDGE_FW_INT_MAX_PACKET_SIZE		32	// Max Packet Size for Interrupt In Endpoint
							// Note that many units were shipped with MPS=16, we
							// force an upgrade to this value).
#define EDGE_FW_INT_INTERVAL			2	// 2ms polling on IntPipe


//
// Definitions of I/O Networks vendor-specific requests
// for default endpoint
//
//	bmRequestType = 00100000	Set vendor-specific, to device
//	bmRequestType = 10100000	Get vendor-specific, to device
//
// These are the definitions for the bRequest field for the
// above bmRequestTypes.
//
// For the read/write Edgeport memory commands, the parameters
// are as follows:
//		wValue = 16-bit address
//		wIndex = unused (though we could put segment 00: or FF: here)
//		wLength = # bytes to read/write (max 64)
//							

#define USB_REQUEST_ION_RESET_DEVICE	0	// Warm reboot Edgeport, retaining USB address
#define USB_REQUEST_ION_GET_EPIC_DESC	1	// Get Edgeport Compatibility Descriptor
// unused				2	// Unused, available
#define USB_REQUEST_ION_READ_RAM	3	// Read  EdgePort RAM at specified addr
#define USB_REQUEST_ION_WRITE_RAM	4	// Write EdgePort RAM at specified addr
#define USB_REQUEST_ION_READ_ROM	5	// Read  EdgePort ROM at specified addr
#define USB_REQUEST_ION_WRITE_ROM	6	// Write EdgePort ROM at specified addr
#define USB_REQUEST_ION_EXEC_DL_CODE	7	// Begin execution of RAM-based download
						// code by jumping to address in wIndex:wValue
//					8	// Unused, available
#define USB_REQUEST_ION_ENABLE_SUSPEND	9	// Enable/Disable suspend feature
						// (wValue != 0: Enable; wValue = 0: Disable)


//
// Define parameter values for our vendor-specific commands
//


// Values for iDownloadFile
#define	EDGE_DOWNLOAD_FILE_NONE		0	// No download requested
#define	EDGE_DOWNLOAD_FILE_INTERNAL	0xFF	// Download the file compiled into driver (930 version)
#define	EDGE_DOWNLOAD_FILE_I930		0xFF	// Download the file compiled into driver (930 version)
#define	EDGE_DOWNLOAD_FILE_80251	0xFE	// Download the file compiled into driver (80251 version)



/*
 *	Special addresses for READ/WRITE_RAM/ROM
 */

// Version 1 (original) format of DeviceParams
#define	EDGE_MANUF_DESC_ADDR_V1		0x00FF7F00
#define	EDGE_MANUF_DESC_LEN_V1		sizeof(EDGE_MANUF_DESCRIPTOR_V1)

// Version 2 format of DeviceParams. This format is longer (3C0h)
// and starts lower in memory, at the uppermost 1K in ROM.
#define	EDGE_MANUF_DESC_ADDR		0x00FF7C00
#define	EDGE_MANUF_DESC_LEN		sizeof(struct edge_manuf_descriptor)

// Boot params descriptor
#define	EDGE_BOOT_DESC_ADDR		0x00FF7FC0
#define	EDGE_BOOT_DESC_LEN		sizeof(struct edge_boot_descriptor)

// Define the max block size that may be read or written
// in a read/write RAM/ROM command.
#define	MAX_SIZE_REQ_ION_READ_MEM	( (__u16) 64 )
#define	MAX_SIZE_REQ_ION_WRITE_MEM	( (__u16) 64 )


//
// Notes for the following two ION vendor-specific param descriptors:
//
//	1.	These have a standard USB descriptor header so they look like a
//		normal descriptor.
//	2.	Any strings in the structures are in USB-defined string
//		descriptor format, so that they may be separately retrieved,
//		if necessary, with a minimum of work on the 930. This also
//		requires them to be in UNICODE format, which, for English at
//		least, simply means extending each __u8 into a __u16.
//	3.	For all fields, 00 means 'uninitialized'.
//	4.	All unused areas should be set to 00 for future expansion.
//

// This structure is ver 2 format. It contains ALL USB descriptors as
// well as the configuration parameters that were in the original V1
// structure. It is NOT modified when new boot code is downloaded; rather,
// these values are set or modified by manufacturing. It is located at
// xC00-xFBF (length 3C0h) in the ROM.
// This structure is a superset of the v1 structure and is arranged so
// that all of the v1 fields remain at the same address. We are just
// adding more room to the front of the structure to hold the descriptors.
//
// The actual contents of this structure are defined in a 930 assembly
// file, converted to a binary image, and then written by the serialization
// program. The C definition of this structure just defines a dummy
// area for general USB descriptors and the descriptor tables (the root
// descriptor starts at xC00). At the bottom of the structure are the
// fields inherited from the v1 structure.

#define MAX_SERIALNUMBER_LEN	12
#define MAX_ASSEMBLYNUMBER_LEN	14

struct edge_manuf_descriptor {

	__u16	RootDescTable[0x10];			// C00 Root of descriptor tables (just a placeholder)
	__u8	DescriptorArea[0x2E0];			// C20 Descriptors go here, up to 2E0h (just a placeholder)

							//     Start of v1-compatible section
	__u8	Length;					// F00 Desc length for what follows, per USB (= C0h )
	__u8	DescType;				// F01 Desc type, per USB (=DEVICE type)
	__u8	DescVer;				// F02 Desc version/format (currently 2)
	__u8	NumRootDescEntries;			// F03 # entries in RootDescTable

	__u8	RomSize;				// F04 Size of ROM/E2PROM in K
	__u8	RamSize;				// F05 Size of external RAM in K
	__u8	CpuRev;					// F06 CPU revision level (chg only if s/w visible)
	__u8	BoardRev;				// F07 PCB revision level (chg only if s/w visible)

	__u8	NumPorts;				// F08 Number of ports
	__u8	DescDate[3];				// F09 MM/DD/YY when descriptor template was compiler,
							//	   so host can track changes to USB-only descriptors.

	__u8	SerNumLength;				// F0C USB string descriptor len
	__u8	SerNumDescType;				// F0D USB descriptor type (=STRING type)
	__u16	SerialNumber[MAX_SERIALNUMBER_LEN];	// F0E "01-01-000100" Unicode Serial Number

	__u8	AssemblyNumLength;			// F26 USB string descriptor len
	__u8	AssemblyNumDescType;			// F27 USB descriptor type (=STRING type)
	__u16	AssemblyNumber[MAX_ASSEMBLYNUMBER_LEN];	// F28 "350-1000-01-A " assembly number

	__u8	OemAssyNumLength;			// F44 USB string descriptor len
	__u8	OemAssyNumDescType;			// F45 USB descriptor type (=STRING type)
	__u16	OemAssyNumber[MAX_ASSEMBLYNUMBER_LEN];	// F46 "xxxxxxxxxxxxxx" OEM assembly number

	__u8	ManufDateLength;			// F62 USB string descriptor len
	__u8	ManufDateDescType;			// F63 USB descriptor type (=STRING type)
	__u16	ManufDate[6];				// F64 "MMDDYY" manufacturing date

	__u8	Reserved3[0x4D];			// F70 -- unused, set to 0 --

	__u8	UartType;				// FBD Uart Type
	__u8	IonPid;					// FBE Product ID, == LSB of USB DevDesc.PID
							//     (Note: Edgeport/4s before 11/98 will have
							//		00 here instead of 01)
	__u8	IonConfig;				// FBF Config byte for ION manufacturing use
							// FBF end of structure, total len = 3C0h

};


#define MANUF_DESC_VER_1	1	// Original definition of MANUF_DESC
#define MANUF_DESC_VER_2	2	// Ver 2, starts at xC00h len 3C0h


// Uart Types
// Note: Since this field was added only recently, all Edgeport/4 units
// shipped before 11/98 will have 00 in this field. Therefore,
// both 00 and 01 values mean '654.
#define MANUF_UART_EXAR_654_EARLY	0	// Exar 16C654 in Edgeport/4s before 11/98
#define MANUF_UART_EXAR_654		1	// Exar 16C654
#define MANUF_UART_EXAR_2852		2	// Exar 16C2852 

//
// Note: The CpuRev and BoardRev values do not conform to manufacturing
// revisions; they are to be incremented only when the CPU or hardware
// changes in a software-visible way, such that the 930 software or
// the host driver needs to handle the hardware differently.
//

// Values of bottom 5 bits of CpuRev & BoardRev for
// Implementation 0 (ie, 930-based)
#define	MANUF_CPU_REV_AD4		1	// 930 AD4, with EP1 Rx bug (needs RXSPM)
#define	MANUF_CPU_REV_AD5		2	// 930 AD5, with above bug (supposedly) fixed
#define	MANUF_CPU_80251			0x20	// Intel 80251


#define MANUF_BOARD_REV_A		1	// Original version, == Manuf Rev A
#define MANUF_BOARD_REV_B		2	// Manuf Rev B, wakeup interrupt works
#define MANUF_BOARD_REV_C		3	// Manuf Rev C, 2/4 ports, rs232/rs422
#define MANUF_BOARD_REV_GENERATION_2	0x20	// Second generaiton edgeport




// Values of bottom 5 bits of CpuRev & BoardRev for
// Implementation 1 (ie, 251+Netchip-based)
#define	MANUF_CPU_REV_1			1	// C251TB Rev 1 (Need actual Intel rev here)

#define MANUF_BOARD_REV_A		1	// First rev of 251+Netchip design



#define	MANUF_SERNUM_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->SerialNumber)
#define	MANUF_ASSYNUM_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->AssemblyNumber)
#define	MANUF_OEMASSYNUM_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->OemAssyNumber)
#define	MANUF_MANUFDATE_LENGTH		sizeof(((struct edge_manuf_descriptor *)0)->ManufDate)

#define	MANUF_ION_CONFIG_MASTER		0x80	// 1=Master mode, 0=Normal
#define	MANUF_ION_CONFIG_DIAG		0x40	// 1=Run h/w diags, 0=norm
#define	MANUF_ION_CONFIG_DIAG_NO_LOOP	0x20	// As above but no ext loopback test


//
// This structure describes parameters for the boot code, and
// is programmed along with new boot code. These are values
// which are specific to a given build of the boot code. It
// is exactly 64 bytes long and is fixed at address FF:xFC0
// - FF:xFFF. Note that the 930-mandated UCONFIG bytes are
// included in this structure.
//
struct edge_boot_descriptor {
	__u8		Length;			// C0 Desc length, per USB (= 40h)
	__u8		DescType;		// C1 Desc type, per USB (= DEVICE type)
	__u8		DescVer;		// C2 Desc version/format
	__u8		Reserved1;		// C3 -- unused, set to 0 --

	__u16		BootCodeLength;		// C4 Boot code goes from FF:0000 to FF:(len-1)
						//	  (LE format)

	__u8		MajorVersion;		// C6 Firmware version: xx.
	__u8		MinorVersion;		// C7			yy.
	__u16		BuildNumber;		// C8			zzzz (LE format)
	
	__u16		EnumRootDescTable;	// CA Root of ROM-based descriptor table
	__u8		NumDescTypes;		// CC Number of supported descriptor types

	__u8		Reserved4;		// CD Fix Compiler Packing

	__u16		Capabilities;		// CE-CF Capabilities flags (LE format)
	__u8		Reserved2[0x28];	// D0 -- unused, set to 0 --
	__u8		UConfig0;		// F8 930-defined CPU configuration byte 0
	__u8		UConfig1;		// F9 930-defined CPU configuration byte 1
	__u8		Reserved3[6];		// FA -- unused, set to 0 --
						// FF end of structure, total len = 80
};


#define BOOT_DESC_VER_1		1	// Original definition of BOOT_PARAMS
#define BOOT_DESC_VER_2		2	// 2nd definition, descriptors not included in boot


	// Capabilities flags

#define	BOOT_CAP_RESET_CMD	0x0001	// If set, boot correctly supports ION_RESET_DEVICE



/************************************************************************
                 T I   U M P   D E F I N I T I O N S
 ***********************************************************************/

//************************************************************************
//	TI I2C Format Definitions
//************************************************************************
#define I2C_DESC_TYPE_INFO_BASIC	1
#define I2C_DESC_TYPE_FIRMWARE_BASIC	2
#define I2C_DESC_TYPE_DEVICE		3
#define I2C_DESC_TYPE_CONFIG		4
#define I2C_DESC_TYPE_STRING		5
#define I2C_DESC_TYPE_FIRMWARE_BLANK 	0xf2

#define I2C_DESC_TYPE_MAX		5
// 3410 may define types 6, 7 for other firmware downloads

// Special section defined by ION
#define I2C_DESC_TYPE_ION		0	// Not defined by TI


struct ti_i2c_desc
{
	__u8	Type;			// Type of descriptor
	__u16	Size;			// Size of data only not including header
	__u8	CheckSum;		// Checksum (8 bit sum of data only)
	__u8	Data[0];		// Data starts here
}__attribute__((packed));

struct ti_i2c_firmware_rec 
{
	__u8	Ver_Major;		// Firmware Major version number
	__u8	Ver_Minor;		// Firmware Minor version number
	__u8	Data[0];		// Download starts here
}__attribute__((packed));


// Structure of header of download image in fw_down.h
struct ti_i2c_image_header
{
	__u16	Length;
	__u8	CheckSum;
}__attribute__((packed));

struct ti_basic_descriptor
{
	__u8	Power;		// Self powered
				// bit 7: 1 - power switching supported
				//        0 - power switching not supported
				//
				// bit 0: 1 - self powered
				//        0 - bus powered
				//
				//
	__u16	HubVid;		// VID HUB
	__u16	HubPid;		// PID HUB
	__u16	DevPid;		// PID Edgeport
	__u8	HubTime;	// Time for power on to power good
	__u8	HubCurrent;	// HUB Current = 100ma
} __attribute__((packed));


#define TI_GET_CPU_REVISION(x)		(__u8)((((x)>>4)&0x0f))
#define TI_GET_BOARD_REVISION(x)	(__u8)(((x)&0x0f))

#define TI_I2C_SIZE_MASK		0x1f  // 5 bits
#define TI_GET_I2C_SIZE(x)		((((x) & TI_I2C_SIZE_MASK)+1)*256)

#define TI_MAX_I2C_SIZE			( 16 * 1024 )

/* TI USB 5052 definitions */
struct edge_ti_manuf_descriptor
{
	__u8 IonConfig;		//  Config byte for ION manufacturing use
	__u8 IonConfig2;	//  Expansion
	__u8 Version;		//  Verqsion
	__u8 CpuRev_BoardRev;	//  CPU revision level (0xF0) and Board Rev Level (0x0F)
	__u8 NumPorts;		//  Number of ports	for this UMP
	__u8 NumVirtualPorts;	//  Number of Virtual ports
	__u8 HubConfig1;	//  Used to configure the Hub
	__u8 HubConfig2;	//  Used to configure the Hub
	__u8 TotalPorts;	//  Total Number of Com Ports for the entire device (All UMPs)
	__u8 Reserved;
}__attribute__((packed));


#endif		// if !defined()
