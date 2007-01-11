/*-
 ****************************************************************
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation.
 * All rights reserved.
 *
 * Copyright 1999 I2O Special Interest Group (I2O SIG).	 All rights reserved.
 * All rights reserved
 *
 * TERMS AND CONDITIONS OF USE
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 *
 * This header file, and any modifications of this header file, are provided
 * contingent upon your agreement and adherence to the here-listed terms and
 * conditions.	By accepting and/or using this header file, you agree to abide
 * by these terms and conditions and that these terms and conditions will be
 * construed and governed in accordance with the laws of the State of California,
 * without reference to conflict-of-law provisions.  If you do not agree
 * to these terms and conditions, please delete this file, and any copies,
 * permanently, without making any use thereof.
 *
 * THIS HEADER FILE IS PROVIDED FREE OF CHARGE ON AN AS-IS BASIS WITHOUT
 * WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  I2O SIG DOES NOT WARRANT THAT THIS HEADER FILE WILL MEET THE
 * USER'S REQUIREMENTS OR THAT ITS OPERATION WILL BE UNINTERRUPTED OR
 * ERROR-FREE.
 *
 * I2O SIG DISCLAIMS ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF
 * ANY PROPRIETARY RIGHTS, RELATING TO THE IMPLEMENTATION OF THE I2O
 * SPECIFICATIONS.  I2O SIG DOES NOT WARRANT OR REPRESENT THAT SUCH
 * IMPLEMENTATIONS WILL NOT INFRINGE SUCH RIGHTS.
 *
 * THE USER OF THIS HEADER FILE SHALL HAVE NO RECOURSE TO I2O SIG FOR ANY
 * ACTUAL OR CONSEQUENTIAL DAMAGES INCLUDING, BUT NOT LIMITED TO, LOST DATA
 * OR LOST PROFITS ARISING OUT OF THE USE OR INABILITY TO USE THIS PROGRAM.
 *
 * I2O SIG grants the user of this header file a license to copy, distribute,
 * and modify it, for any purpose, under the following terms.  Any copying,
 * distribution, or modification of this header file must not delete or alter
 * the copyright notice of I2O SIG or any of these Terms and Conditions.
 *
 * Any distribution of this header file must not include a charge for the
 * header file (unless such charges are strictly for the physical acts of
 * copying or transferring copies).  However, distribution of a product in
 * which this header file is embedded may include a charge so long as any
 * such charge does not include any charge for the header file itself.
 *
 * Any modification of this header file constitutes a derivative work based
 * on this header file.	 Any distribution of such derivative work: (1) must
 * include prominent notices that the header file has been changed from the
 * original, together with the dates of any changes; (2) automatically includes
 * this same license to the original header file from I2O SIG, without any
 * restriction thereon from the distributing user; and (3) must include a
 * grant of license of the modified file under the same terms and conditions
 * as these Terms and Conditions.
 *
 * The I2O SIG Web site can be found at: http://www.i2osig.org
 *
 * The I2O SIG encourages you to deposit derivative works based on this
 * header file at the I2O SIG Web site.	 Furthermore, to become a Registered
 * Developer of the I2O SIG, sign up at the Web site or call 415.750.8352
 * (United States).
 *
 * $FreeBSD: src/sys/dev/asr/i2omsg.h,v 1.7 2005/01/06 01:42:29 imp Exp $
 *
 ****************************************************************/

/*********************************************************************
 * I2OMsg.h -- I2O Message defintion file
 *
 * This file contains information presented in Chapter 3, 4 and 6 of
 * the I2O(tm) Specification and most of the I2O Global defines and
 * Typedefs.
 **********************************************************************/

#if !defined(I2O_MESSAGE_HDR)
#define	I2O_MESSAGE_HDR

#define	I2OMSG_REV 1_5_4  /* I2OMsg header file revision string */

/*

   NOTES:

   Gets, reads, receives, etc. are all even numbered functions.
   Sets, writes, sends, etc. are all odd numbered functions.
   Functions that both send and receive data can be either but an attempt is
   made to use the function number that indicates the greater transfer amount.
   Functions that do not send or receive data use odd function numbers.

   Some functions are synonyms like read, receive and send, write.

   All common functions will have a code of less than 0x80.
   Unique functions to a class will start at 0x80.
   Executive Functions start at 0xA0.

   Utility Message function codes range from 0 - 0x1f
   Base Message function codes range from 0x20 - 0xfe
   Private Message function code is 0xff.
*/



#if ((defined(KERNEL) || defined(_KERNEL)) && defined(__FreeBSD__))
# if (KERN_VERSION < 3)
#  include "i386/pci/i2otypes.h"
# else
#  include "dev/asr/i2otypes.h"
# endif
#else
# include "i2otypes.h"
#endif


PRAGMA_ALIGN_PUSH

PRAGMA_PACK_PUSH

/* Set to 1 for 64 bit Context Fields */
#define	    I2O_64BIT_CONTEXT	       0

/****************************************************************************/

/* Common functions accross all classes. */

#define	   I2O_PRIVATE_MESSAGE			       0xFF

/****************************************************************************/
/* Class ID and Code Assignments */


#define	   I2O_CLASS_VERSION_10			       0x00
#define	   I2O_CLASS_VERSION_11			       0x01

/*    Class Code Names:	 Table 6-1 Class Code Assignments. */
#define	   I2O_CLASS_EXECUTIVE			       0x000
#define	   I2O_CLASS_DDM			       0x001
#define	   I2O_CLASS_RANDOM_BLOCK_STORAGE	       0x010
#define	   I2O_CLASS_SEQUENTIAL_STORAGE		       0x011
#define	   I2O_CLASS_LAN			       0x020
#define	   I2O_CLASS_WAN			       0x030
#define	   I2O_CLASS_FIBRE_CHANNEL_PORT		       0x040
#define	   I2O_CLASS_FIBRE_CHANNEL_PERIPHERAL	       0x041
#define	   I2O_CLASS_SCSI_PERIPHERAL		       0x051
#define	   I2O_CLASS_ATE_PORT			       0x060
#define	   I2O_CLASS_ATE_PERIPHERAL		       0x061
#define	   I2O_CLASS_FLOPPY_CONTROLLER		       0x070
#define	   I2O_CLASS_FLOPPY_DEVICE		       0x071
#define	   I2O_CLASS_BUS_ADAPTER_PORT		       0x080
/* Class Codes 0x090 - 0x09f are reserved for Peer-to-Peer classes */
#define	   I2O_CLASS_MATCH_ANYCLASS		       0xffffffff

#define	   I2O_SUBCLASS_i960			       0x001
#define	   I2O_SUBCLASS_HDM			       0x020
#define	   I2O_SUBCLASS_ISM			       0x021


/****************************************************************************/
/* Message Frame defines and structures	 */

/*   Defines for the Version_Status field. */

#define	   I2O_VERSION_10			       0x00
#define	   I2O_VERSION_11			       0x01

#define	   I2O_VERSION_OFFSET_NUMBER_MASK	       0x07
#define	   I2O_VERSION_OFFSET_SGL_TRL_OFFSET_MASK      0xF0

/*   Defines for the Message Flags Field. */
/*   Please Note the the FAIL bit is only set in the Transport Fail Message. */
#define	   I2O_MESSAGE_FLAGS_STATIC		       0x01
#define	   I2O_MESSAGE_FLAGS_64BIT_CONTEXT	       0x02
#define	   I2O_MESSAGE_FLAGS_MULTIPLE		       0x10
#define	   I2O_MESSAGE_FLAGS_FAIL		       0x20
#define	   I2O_MESSAGE_FLAGS_LAST		       0x40
#define	   I2O_MESSAGE_FLAGS_REPLY		       0x80

/* Defines for Request Status Codes:  Table 3-1 Reply Status Codes.  */

#define	   I2O_REPLY_STATUS_SUCCESS		       0x00
#define	   I2O_REPLY_STATUS_ABORT_DIRTY		       0x01
#define	   I2O_REPLY_STATUS_ABORT_NO_DATA_TRANSFER     0x02
#define	   I2O_REPLY_STATUS_ABORT_PARTIAL_TRANSFER     0x03
#define	   I2O_REPLY_STATUS_ERROR_DIRTY		       0x04
#define	   I2O_REPLY_STATUS_ERROR_NO_DATA_TRANSFER     0x05
#define	   I2O_REPLY_STATUS_ERROR_PARTIAL_TRANSFER     0x06
#define	   I2O_REPLY_STATUS_PROCESS_ABORT_DIRTY	       0x08
#define	   I2O_REPLY_STATUS_PROCESS_ABORT_NO_DATA_TRANSFER   0x09
#define	   I2O_REPLY_STATUS_PROCESS_ABORT_PARTIAL_TRANSFER   0x0A
#define	   I2O_REPLY_STATUS_TRANSACTION_ERROR	       0x0B
#define	   I2O_REPLY_STATUS_PROGRESS_REPORT	       0x80

/* DetailedStatusCode defines for ALL messages: Table 3-2 Detailed Status Codes.  */

#define	   I2O_DETAIL_STATUS_SUCCESS			    0x0000
#define	   I2O_DETAIL_STATUS_BAD_KEY			    0x0002
#define	   I2O_DETAIL_STATUS_TCL_ERROR			    0x0003
#define	   I2O_DETAIL_STATUS_REPLY_BUFFER_FULL		    0x0004
#define	   I2O_DETAIL_STATUS_NO_SUCH_PAGE		    0x0005
#define	   I2O_DETAIL_STATUS_INSUFFICIENT_RESOURCE_SOFT	    0x0006
#define	   I2O_DETAIL_STATUS_INSUFFICIENT_RESOURCE_HARD	    0x0007
#define	   I2O_DETAIL_STATUS_CHAIN_BUFFER_TOO_LARGE	    0x0009
#define	   I2O_DETAIL_STATUS_UNSUPPORTED_FUNCTION	    0x000A
#define	   I2O_DETAIL_STATUS_DEVICE_LOCKED		    0x000B
#define	   I2O_DETAIL_STATUS_DEVICE_RESET		    0x000C
#define	   I2O_DETAIL_STATUS_INAPPROPRIATE_FUNCTION	    0x000D
#define	   I2O_DETAIL_STATUS_INVALID_INITIATOR_ADDRESS	    0x000E
#define	   I2O_DETAIL_STATUS_INVALID_MESSAGE_FLAGS	    0x000F
#define	   I2O_DETAIL_STATUS_INVALID_OFFSET		    0x0010
#define	   I2O_DETAIL_STATUS_INVALID_PARAMETER		    0x0011
#define	   I2O_DETAIL_STATUS_INVALID_REQUEST		    0x0012
#define	   I2O_DETAIL_STATUS_INVALID_TARGET_ADDRESS	    0x0013
#define	   I2O_DETAIL_STATUS_MESSAGE_TOO_LARGE		    0x0014
#define	   I2O_DETAIL_STATUS_MESSAGE_TOO_SMALL		    0x0015
#define	   I2O_DETAIL_STATUS_MISSING_PARAMETER		    0x0016
#define	   I2O_DETAIL_STATUS_TIMEOUT			    0x0017
#define	   I2O_DETAIL_STATUS_UNKNOWN_ERROR		    0x0018
#define	   I2O_DETAIL_STATUS_UNKNOWN_FUNCTION		    0x0019
#define	   I2O_DETAIL_STATUS_UNSUPPORTED_VERSION	    0x001A
#define	   I2O_DEATIL_STATUS_DEVICE_BUSY		    0x001B
#define	   I2O_DETAIL_STATUS_DEVICE_NOT_AVAILABLE	    0x001C

/* Common I2O Field sizes  */

#define	   I2O_TID_SZ				       12
#define	   I2O_FUNCTION_SZ			       8
#define	   I2O_UNIT_ID_SZ			       16
#define	   I2O_SEGMENT_NUMBER_SZ		       12

#define	   I2O_IOP_ID_SZ			       12
#define	   I2O_GROUP_ID_SZ			       16
#define	   I2O_IOP_STATE_SZ			       8
#define	   I2O_MESSENGER_TYPE_SZ		       8

#define	   I2O_CLASS_ID_SZ			       12
#define	   I2O_CLASS_ORGANIZATION_ID_SZ		       16

#define	   I2O_4BIT_VERSION_SZ			       4
#define	   I2O_8BIT_FLAGS_SZ			       8
#define	   I2O_COMMON_LENGTH_FIELD_SZ		       16

#define	   I2O_DEVID_DESCRIPTION_SZ		       16
#define	   I2O_DEVID_VENDOR_INFO_SZ		       16
#define	   I2O_DEVID_PRODUCT_INFO_SZ		       16
#define	   I2O_DEVID_REV_LEVEL_SZ		       8
#define	   I2O_MODULE_NAME_SZ			       24

#define	   I2O_BIOS_INFO_SZ			       8

#define	   I2O_RESERVED_4BITS			       4
#define	   I2O_RESERVED_8BITS			       8
#define	   I2O_RESERVED_12BITS			       12
#define	   I2O_RESERVED_16BITS			       16
#define	   I2O_RESERVED_20BITS			       20
#define	   I2O_RESERVED_24BITS			       24
#define	   I2O_RESERVED_28BITS			       28


typedef	   U32	      I2O_PARAMETER_TID;


#if	I2O_64BIT_CONTEXT
typedef	   U64	      I2O_INITIATOR_CONTEXT;
typedef	   U64	      I2O_TRANSACTION_CONTEXT;
#else
typedef	   U32	      I2O_INITIATOR_CONTEXT;
typedef	   U32	      I2O_TRANSACTION_CONTEXT;
#endif

/*  Serial Number format defines */

#define	   I2O_SERIAL_FORMAT_UNKNOWN		       0
#define	   I2O_SERIAL_FORMAT_BINARY		       1
#define	   I2O_SERIAL_FORMAT_ASCII		       2
#define	   I2O_SERIAL_FORMAT_UNICODE		       3
#define	   I2O_SERIAL_FORMAT_LAN_MAC		       4
#define	   I2O_SERIAL_FORMAT_WAN		       5

/* Special TID Assignments */

#define	   I2O_IOP_TID				       0
#define	   I2O_HOST_TID				       1


/****************************************************************************/

/* I2O Message Frame common for all messages  */

typedef struct _I2O_MESSAGE_FRAME {
   U8			       VersionOffset;
   U8			       MsgFlags;
   U16			       MessageSize;
#if  (defined(__BORLANDC__)) || defined(_DPT_BIG_ENDIAN) || (defined(sparc))
   U32			       TargetAddress;
#else
   BF			       TargetAddress:I2O_TID_SZ;
   BF			       InitiatorAddress:I2O_TID_SZ;
   BF			       Function:I2O_FUNCTION_SZ;
#endif
   I2O_INITIATOR_CONTEXT       InitiatorContext;
} I2O_MESSAGE_FRAME, *PI2O_MESSAGE_FRAME;


/****************************************************************************/

/* Transaction Reply Lists (TRL) Control Word structure */

#define	   I2O_TRL_FLAGS_SINGLE_FIXED_LENGTH	       0x00
#define	   I2O_TRL_FLAGS_SINGLE_VARIABLE_LENGTH	       0x40
#define	   I2O_TRL_FLAGS_MULTIPLE_FIXED_LENGTH	       0x80

typedef struct _I2O_TRL_CONTROL_WORD {
   U8			       TrlCount;
   U8			       TrlElementSize;
   U8			       reserved;
   U8			       TrlFlags;
#if	I2O_64BIT_CONTEXT
   U32			       Padding;		  /* Padding for 64 bit */
#endif
} I2O_TRL_CONTROL_WORD, *PI2O_TRL_CONTROL_WORD;

/****************************************************************************/

/* I2O Successful Single Transaction Reply Message Frame structure. */

typedef struct _I2O_SINGLE_REPLY_MESSAGE_FRAME {
   I2O_MESSAGE_FRAME	       StdMessageFrame;
   I2O_TRANSACTION_CONTEXT     TransactionContext;
   U16			       DetailedStatusCode;
   U8			       reserved;
   U8			       ReqStatus;
/*			       ReplyPayload	   */
} I2O_SINGLE_REPLY_MESSAGE_FRAME, *PI2O_SINGLE_REPLY_MESSAGE_FRAME;


/****************************************************************************/

/* I2O Successful Multiple Transaction Reply Message Frame structure. */

typedef struct _I2O_MULTIPLE_REPLY_MESSAGE_FRAME {
   I2O_MESSAGE_FRAME	       StdMessageFrame;
   I2O_TRL_CONTROL_WORD	       TrlControlWord;
   U16			       DetailedStatusCode;
   U8			       reserved;
   U8			       ReqStatus;
/*			       TransactionDetails[]	   */
} I2O_MULTIPLE_REPLY_MESSAGE_FRAME, *PI2O_MULTIPLE_REPLY_MESSAGE_FRAME;


/****************************************************************************/

/* I2O Private Message Frame structure. */

typedef struct _I2O_PRIVATE_MESSAGE_FRAME {
   I2O_MESSAGE_FRAME	       StdMessageFrame;
   I2O_TRANSACTION_CONTEXT     TransactionContext;
   U16			       XFunctionCode;
   U16			       OrganizationID;
/*			       PrivatePayload[]	       */
} I2O_PRIVATE_MESSAGE_FRAME, *PI2O_PRIVATE_MESSAGE_FRAME;


/****************************************************************************/

/* Message Failure Severity Codes */

#define	   I2O_SEVERITY_FORMAT_ERROR		       0x1
#define	   I2O_SEVERITY_PATH_ERROR		       0x2
#define	   I2O_SEVERITY_PATH_STATE		       0x4
#define	   I2O_SEVERITY_CONGESTION		       0x8

/* Transport Failure Codes: Table 3-3 Mesasge Failure Codes */

#define	   I2O_FAILURE_CODE_TRANSPORT_SERVICE_SUSPENDED	   0x81
#define	   I2O_FAILURE_CODE_TRANSPORT_SERVICE_TERMINATED   0x82
#define	   I2O_FAILURE_CODE_TRANSPORT_CONGESTION	   0x83
#define	   I2O_FAILURE_CODE_TRANSPORT_FAIL		   0x84
#define	   I2O_FAILURE_CODE_TRANSPORT_STATE_ERROR	   0x85
#define	   I2O_FAILURE_CODE_TRANSPORT_TIME_OUT		   0x86
#define	   I2O_FAILURE_CODE_TRANSPORT_ROUTING_FAILURE	   0x87
#define	   I2O_FAILURE_CODE_TRANSPORT_INVALID_VERSION	   0x88
#define	   I2O_FAILURE_CODE_TRANSPORT_INVALID_OFFSET	   0x89
#define	   I2O_FAILURE_CODE_TRANSPORT_INVALID_MSG_FLAGS	   0x8A
#define	   I2O_FAILURE_CODE_TRANSPORT_FRAME_TOO_SMALL	   0x8B
#define	   I2O_FAILURE_CODE_TRANSPORT_FRAME_TOO_LARGE	   0x8C
#define	   I2O_FAILURE_CODE_TRANSPORT_INVALID_TARGET_ID	   0x8D
#define	   I2O_FAILURE_CODE_TRANSPORT_INVALID_INITIATOR_ID 0x8E
#define	   I2O_FAILURE_CODE_TRANSPORT_INVALID_INITIATOR_CONTEXT	   0x8F
#define	   I2O_FAILURE_CODE_TRANSPORT_UNKNOWN_FAILURE	   0xFF

/* IOP_ID and Severity sizes */

#define	   I2O_FAILCODE_SEVERITY_SZ			   8
#define	   I2O_FAILCODE_CODE_SZ				   8

/* I2O Transport Message Reply for Message Failure. */

typedef struct _I2O_FAILURE_REPLY_MESSAGE_FRAME {
    I2O_MESSAGE_FRAME		StdMessageFrame;
    I2O_TRANSACTION_CONTEXT	TransactionContext;
#   if (defined(_DPT_BIG_ENDIAN) || defined(sparc) || defined(__BORLANDC__))
	U32			LowestVersion;
	U32			reserved;
#   else
	U8			LowestVersion;
	U8			HighestVersion;
/*	BF			Severity:I2O_FAILCODE_SEVERITY_SZ; */
/*	BF			FailureCode:I2O_FAILCODE_CODE_SZ; */
/* Due to our compiler padding this structure and making it larger than
 * it really is (4 bytes larger), we are re-defining these two fields
 */
	U8			Severity;
	U8			FailureCode;
	BF			reserved:I2O_RESERVED_4BITS;
	BF			FailingHostUnitID:I2O_UNIT_ID_SZ;
	BF			reserved1:12;
#   endif
    U32				AgeLimit;
    U32				PreservedMFA;
} I2O_FAILURE_REPLY_MESSAGE_FRAME, *PI2O_FAILURE_REPLY_MESSAGE_FRAME;

/* I2O Transport Message Reply for Transaction Error. */

typedef struct _I2O_TRANSACTION_ERROR_REPLY_MESSAGE_FRAME {
   I2O_MESSAGE_FRAME	       StdMessageFrame;
   I2O_TRANSACTION_CONTEXT     TransactionContext;
   U16			       DetailedStatusCode;
   U8			       reserved;
   U8			       ReqStatus;   /* Should always be Transaction Error */
   U32			       ErrorOffset;
   U8			       BitOffset;
   U8			       reserved1;
   U16			       reserved2;
} I2O_TRANSACTION_ERROR_REPLY_MESSAGE_FRAME, *PI2O_TRANSACTION_ERROR_REPLY_MESSAGE_FRAME;

/****************************************************************************/

/*  Misc. commonly used structures */

/* Class ID Block */

typedef struct _I2O_CLASS_ID {
#if (defined(_DPT_BIG_ENDIAN) || defined(sparc))
   U32			       Class;
#else
   BF			       Class:I2O_CLASS_ID_SZ;
   BF			       Version:I2O_4BIT_VERSION_SZ;
   BF			       OrganizationID:I2O_CLASS_ORGANIZATION_ID_SZ;
#endif
} I2O_CLASS_ID, *PI2O_CLASS_ID;


#define	   I2O_MAX_SERIAL_NUMBER_SZ		       256

typedef struct _I2O_SERIAL_INFO {
   U8			       SerialNumberLength;
   U8			       SerialNumberFormat;
   U8			       SerialNumber[I2O_MAX_SERIAL_NUMBER_SZ];
} I2O_SERIAL_INFO, *PI2O_SERIAL_INFO;


/****************************************************************************/
/* Hardware Resource Table (HRT) and Logical Configuration Table (LCT) */
/****************************************************************************/

/* Bus Type Code defines */

#define	   I2O_LOCAL_BUS			       0
#define	   I2O_ISA_BUS				       1
#define	   I2O_EISA_BUS				       2
#define	   I2O_MCA_BUS				       3
#define	   I2O_PCI_BUS				       4
#define	   I2O_PCMCIA_BUS			       5
#define	   I2O_NUBUS_BUS			       6
#define	   I2O_CARDBUS_BUS			       7
#define	   I2O_OTHER_BUS			       0x80

#define	   I2O_HRT_STATE_SZ			       4
#define	   I2O_HRT_BUS_NUMBER_SZ		       8
#define	   I2O_HRT_BUS_TYPE_SZ			       8


/* Bus Structures */

/* PCI Bus */
typedef struct _I2O_PCI_BUS_INFO {
   U8			       PciFunctionNumber;
   U8			       PciDeviceNumber;
   U8			       PciBusNumber;
   U8			       reserved;
   U16			       PciVendorID;
   U16			       PciDeviceID;
} I2O_PCI_BUS_INFO, *PI2O_PCI_BUS_INFO;

/* Local Bus */
typedef struct _I2O_LOCAL_BUS_INFO {
   U16			       LbBaseIOPort;
   U16			       reserved;
   U32			       LbBaseMemoryAddress;
} I2O_LOCAL_BUS_INFO, *PI2O_LOCAL_BUS_INFO;

/* ISA Bus */
typedef struct _I2O_ISA_BUS_INFO {
   U16			       IsaBaseIOPort;
   U8			       CSN;
   U8			       reserved;
   U32			       IsaBaseMemoryAddress;
} I2O_ISA_BUS_INFO, *PI2O_ISA_BUS_INFO;

/* EISA Bus */
typedef struct _I2O_EISA_BUS_INFO {
   U16			       EisaBaseIOPort;
   U8			       reserved;
   U8			       EisaSlotNumber;
   U32			       EisaBaseMemoryAddress;
} I2O_EISA_BUS_INFO, *PI2O_EISA_BUS_INFO;

/* MCA Bus */
typedef struct _I2O_MCA_BUS_INFO {
   U16			       McaBaseIOPort;
   U8			       reserved;
   U8			       McaSlotNumber;
   U32			       McaBaseMemoryAddress;
} I2O_MCA_BUS_INFO, *PI2O_MCA_BUS_INFO;

/* Other Bus */
typedef struct _I2O_OTHER_BUS_INFO {
   U16			       BaseIOPort;
   U16			       reserved;
   U32			       BaseMemoryAddress;
} I2O_OTHER_BUS_INFO, *PI2O_OTHER_BUS_INFO;


/* HRT Entry Block */

typedef struct _I2O_HRT_ENTRY {
   U32			       AdapterID;
#if (defined(_DPT_BIG_ENDIAN) || defined(sparc))
   U32			       ControllingTID;
#else
   BF			       ControllingTID:I2O_TID_SZ;
   BF			       AdapterState:I2O_HRT_STATE_SZ;
   BF			       BusNumber:I2O_HRT_BUS_NUMBER_SZ;
   BF			       BusType:I2O_HRT_BUS_TYPE_SZ;
#endif
   union {
       /* PCI Bus */
       I2O_PCI_BUS_INFO	       PCIBus;

       /* Local Bus */
       I2O_LOCAL_BUS_INFO      LocalBus;

       /* ISA Bus */
       I2O_ISA_BUS_INFO	       ISABus;

       /* EISA Bus */
       I2O_EISA_BUS_INFO       EISABus;

       /* MCA Bus */
       I2O_MCA_BUS_INFO	       MCABus;

       /* Other. */
       I2O_OTHER_BUS_INFO      OtherBus;
   }uBus;
} I2O_HRT_ENTRY, *PI2O_HRT_ENTRY;


/* I2O Hardware Resource Table structure. */

typedef struct _I2O_HRT {
   U16			       NumberEntries;
   U8			       EntryLength;
   U8			       HRTVersion;
   U32			       CurrentChangeIndicator;
   I2O_HRT_ENTRY	       HRTEntry[1];
} I2O_HRT, *PI2O_HRT;


/****************************************************************************/
/* Logical Configuration Table	*/
/****************************************************************************/

/* I2O Logical Configuration Table structures. */

#define	   I2O_IDENTITY_TAG_SZ			       8

/* I2O Logical Configuration Table Device Flags */

#define	   I2O_LCT_DEVICE_FLAGS_CONF_DIALOG_REQUEST	       0x01
#define	   I2O_LCT_DEVICE_FLAGS_MORE_THAN_1_USER	       0x02
#define	   I2O_LCT_DEVICE_FLAGS_PEER_SERVICE_DISABLED	       0x10
#define	   I2O_LCT_DEVICE_FLAGS_MANAGEMENT_SERVICE_DISABLED    0x20

/* LCT Entry Block */

typedef struct _I2O_LCT_ENTRY {
#if (defined(_DPT_BIG_ENDIAN) || defined(sparc))
   U32			       TableEntrySize;
#else
   BF			       TableEntrySize:I2O_COMMON_LENGTH_FIELD_SZ;
   BF			       LocalTID:I2O_TID_SZ;
   BF			       reserved:I2O_4BIT_VERSION_SZ;
#endif
   U32			       ChangeIndicator;
   U32			       DeviceFlags;
   I2O_CLASS_ID		       ClassID;
   U32			       SubClassInfo;
#if (defined(__BORLANDC__)) || defined(_DPT_BIG_ENDIAN) || (defined(sparc))
   U32			       UserTID;
#else
   BF			       UserTID:I2O_TID_SZ;
   BF			       ParentTID:I2O_TID_SZ;
   BF			       BiosInfo:I2O_BIOS_INFO_SZ;
#endif
   U8			       IdentityTag[I2O_IDENTITY_TAG_SZ];
   U32			       EventCapabilities;
} I2O_LCT_ENTRY, *PI2O_LCT_ENTRY;


/* I2O Logical Configuration Table structure. */

typedef struct _I2O_LCT {
#if (defined(_DPT_BIG_ENDIAN) || defined(sparc))
   U32			       TableSize;
#else
   BF			       TableSize:I2O_COMMON_LENGTH_FIELD_SZ;
   BF			       BootDeviceTID:I2O_TID_SZ;
   BF			       LctVer:I2O_4BIT_VERSION_SZ;
#endif
   U32			       IopFlags;
   U32			       CurrentChangeIndicator;
   I2O_LCT_ENTRY	       LCTEntry[1];
} I2O_LCT, *PI2O_LCT;


/****************************************************************************/

/* Memory Addressing structures and defines. */

/* SglFlags defines. */

#define	   I2O_SGL_FLAGS_LAST_ELEMENT		       0x80
#define	   I2O_SGL_FLAGS_END_OF_BUFFER		       0x40

#define	   I2O_SGL_FLAGS_IGNORE_ELEMENT		       0x00
#define	   I2O_SGL_FLAGS_TRANSPORT_ELEMENT	       0x04
#define	   I2O_SGL_FLAGS_BIT_BUCKET_ELEMENT	       0x08
#define	   I2O_SGL_FLAGS_IMMEDIATE_DATA_ELEMENT	       0x0C
#define	   I2O_SGL_FLAGS_SIMPLE_ADDRESS_ELEMENT	       0x10
#define	   I2O_SGL_FLAGS_PAGE_LIST_ADDRESS_ELEMENT     0x20
#define	   I2O_SGL_FLAGS_CHAIN_POINTER_ELEMENT	       0x30
#define	   I2O_SGL_FLAGS_LONG_TRANSACTION_ELEMENT      0x40
#define	   I2O_SGL_FLAGS_SHORT_TRANSACTION_ELEMENT     0x70
#define	   I2O_SGL_FLAGS_SGL_ATTRIBUTES_ELEMENT	       0x7C

#define	   I2O_SGL_FLAGS_BC0			       0x01
#define	   I2O_SGL_FLAGS_BC1			       0x02
#define	   I2O_SGL_FLAGS_DIR			       0x04
#define	   I2O_SGL_FLAGS_LOCAL_ADDRESS		       0x08

#define	   I2O_SGL_FLAGS_CONTEXT_COUNT_MASK	       0x03
#define	   I2O_SGL_FLAGS_ADDRESS_MODE_MASK	       0x3C
#define	   I2O_SGL_FLAGS_NO_CONTEXT		       0x00

/*  Scatter/Gather Truth Table */

/*

typedef enum _SG_TYPE {
   INVALID,
   Ignore,
   TransportDetails,
   BitBucket,
   ImmediateData,
   Simple,
   PageList,
   ChainPointer,
   ShortTransaction,
   LongTransaction,
   SGLAttributes,
   INVALID/ReservedLongFormat,
   INVALID/ReservedShortFormat
} SG_TYPE, *PSG_TYPE;


   0x00	   Ignore;
   0x04	   TransportDetails;
   0x08	   BitBucket;
   0x0C	   ImmediateData;
   0x10	   Simple;
   0x14	   Simple;
   0x18	   Simple;
   0x1C	   Simple;
   0x20	   PageList;
   0x24	   PageList;
   0x28	   PageList;
   0x2C	   PageList;
   0x30	   ChainPointer;
   0x34	   INVALID;
   0x38	   ChainPointer;
   0x3C	   INVALID;
   0x40	   LongTransaction;
   0x44	   INVALID/ReservedLongFormat;
   0x48	   BitBucket;
   0x4C	   ImmediateData;
   0x50	   Simple;
   0x54	   Simple;
   0x58	   Simple;
   0x5C	   Simple;
   0x60	   PageList;
   0x64	   PageList;
   0x68	   PageList;
   0x6C	   PageList;
   0x70	   ShortTransaction;
   0x74	   INVALID/ReservedShortFormat;
   0x78	   INVALID/ReservedShortFormat;
   0x7C	   SGLAttributes;
*/


/* 32 Bit Context Field defines */

#define	   I2O_SGL_FLAGS_CONTEXT32_NULL		       0x00
#define	   I2O_SGL_FLAGS_CONTEXT32_U32		       0x01
#define	   I2O_SGL_FLAGS_CONTEXT32_U64		       0x02
#define	   I2O_SGL_FLAGS_CONTEXT32_U96		       0x03

#define	   I2O_SGL_FLAGS_CONTEXT32_NULL_SZ	       0x00
#define	   I2O_SGL_FLAGS_CONTEXT32_U32_SZ	       0x04
#define	   I2O_SGL_FLAGS_CONTEXT32_U64_SZ	       0x08
#define	   I2O_SGL_FLAGS_CONTEXT32_U96_SZ	       0x0C

/* 64 Bit Context Field defines */

#define	   I2O_SGL_FLAGS_CONTEXT64_NULL		       0x00
#define	   I2O_SGL_FLAGS_CONTEXT64_U64		       0x01
#define	   I2O_SGL_FLAGS_CONTEXT64_U128		       0x02
#define	   I2O_SGL_FLAGS_CONTEXT64_U192		       0x03

#define	   I2O_SGL_FLAGS_CONTEXT64_NULL_SZ	       0x00
#define	   I2O_SGL_FLAGS_CONTEXT64_U64_SZ	       0x08
#define	   I2O_SGL_FLAGS_CONTEXT64_U128_SZ	       0x10
#define	   I2O_SGL_FLAGS_CONTEXT64_U192_SZ	       0x18

/* SGL Attribute Element defines */

#define	   I2O_SGL_ATTRIBUTE_FLAGS_BIT_BUCKET_HINT     0x0400
#define	   I2O_SGL_ATTRIBUTE_FLAGS_IMMEDIATE_DATA_HINT 0x0200
#define	   I2O_SGL_ATTRIBUTE_FLAGS_LOCAL_ADDRESS_HINT  0x0100
#define	   I2O_SGL_ATTRIBUTE_FLAGS_32BIT_TRANSACTION   0x0000
#define	   I2O_SGL_ATTRIBUTE_FLAGS_64BIT_TRANSACTION   0x0004
#define	   I2O_SGL_ATTRIBUTE_FLAGS_32BIT_LOCAL_ADDRESS 0x0000

/* SG Size defines */

#define	   I2O_SG_COUNT_SZ			       24
#define	   I2O_SG_FLAGS_SZ			       8

/* Standard Flags and Count fields for SG Elements */

typedef struct _I2O_FLAGS_COUNT {
#if (defined(__BORLANDC__)) || defined(_DPT_BIG_ENDIAN) || (defined(sparc))
   U32			       Count;
#else
   BF			       Count:I2O_SG_COUNT_SZ;
   BF			       Flags:I2O_SG_FLAGS_SZ;
#endif
} I2O_FLAGS_COUNT, *PI2O_FLAGS_COUNT;

/* Bit Bucket Element */

typedef struct _I2O_SGE_BIT_BUCKET_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       BufferContext;
} I2O_SGE_BIT_BUCKET_ELEMENT, *PI2O_SGE_BIT_BUCKET_ELEMENT;

/* Chain Addressing Scatter-Gather Element */

typedef struct _I2O_SGE_CHAIN_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       PhysicalAddress;
} I2O_SGE_CHAIN_ELEMENT, *PI2O_SGE_CHAIN_ELEMENT;

/* Chain Addressing with Context Scatter-Gather Element */

typedef struct _I2O_SGE_CHAIN_CONTEXT_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       Context[1];
   U32			       PhysicalAddress;
} I2O_SGE_CHAIN_CONTEXT_ELEMENT, *PI2O_SGE_CHAIN_CONTEXT_ELEMENT;

/* Ignore Scatter-Gather Element */

typedef struct _I2O_SGE_IGNORE_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
} I2O_SGE_IGNORE_ELEMENT, *PI2O_SGE_IGNORE_ELEMENT;

/* Immediate Data Element */

typedef struct _I2O_SGE_IMMEDIATE_DATA_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
} I2O_SGE_IMMEDIATE_DATA_ELEMENT, *PI2O_SGE_IMMEDIATE_DATA_ELEMENT;

/* Immediate Data with Context Element */

typedef struct _I2O_SGE_IMMEDIATE_DATA_CONTEXT_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       BufferContext;
} I2O_SGE_IMMEDIATE_DATA_CONTEXT_ELEMENT, *PI2O_SGE_IMMEDIATE_DATA_CONTEXT_ELEMENT;

/* Long Transaction Parameters Element */

typedef struct _I2O_SGE_LONG_TRANSACTION_ELEMENT {
#if (defined(__BORLANDC__))
   U32			       LongElementLength;
#else
   BF			       LongElementLength:I2O_SG_COUNT_SZ;
   BF			       Flags:I2O_SG_FLAGS_SZ;
#endif
   U32			       BufferContext;
} I2O_SGE_LONG_TRANSACTION_ELEMENT, *PI2O_SGE_LONG_TRANSACTION_ELEMENT;

/* Page List Scatter-Gather Element */

typedef struct _I2O_SGE_PAGE_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       PhysicalAddress[1];
} I2O_SGE_PAGE_ELEMENT , *PI2O_SGE_PAGE_ELEMENT ;

/* Page List with Context Scatter-Gather Element */

typedef struct _I2O_SGE_PAGE_CONTEXT_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       BufferContext[1];
   U32			       PhysicalAddress[1];
} I2O_SGE_PAGE_CONTEXT_ELEMENT, *PI2O_SGE_PAGE_CONTEXT_ELEMENT;

/* SGL Attribute Element */

typedef struct _I2O_SGE_SGL_ATTRIBUTES_ELEMENT {
   U16			       SglAttributeFlags;
   U8			       ElementLength;
   U8			       Flags;
   U32			       PageFrameSize;
} I2O_SGE_SGL_ATTRIBUTES_ELEMENT, *PI2O_SGE_SGL_ATTRIBUTES_ELEMENT;

/* Short Transaction Parameters Element */

typedef struct _I2O_SGE_SHORT_TRANSACTION_ELEMENT {
   U16			       ClassFields;
   U8			       ElementLength;
   U8			       Flags;
   U32			       BufferContext;
} I2O_SGE_SHORT_TRANSACTION_ELEMENT, *PI2O_SGE_SHORT_TRANSACTION_ELEMENT;

/* Simple Addressing Scatter-Gather Element */

typedef struct _I2O_SGE_SIMPLE_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       PhysicalAddress;
} I2O_SGE_SIMPLE_ELEMENT, *PI2O_SGE_SIMPLE_ELEMENT;

/* Simple Addressing with Context Scatter-Gather Element */

typedef struct _I2O_SGE_SIMPLE_CONTEXT_ELEMENT {
   I2O_FLAGS_COUNT	       FlagsCount;
   U32			       BufferContext[1];
   U32			       PhysicalAddress;
} I2O_SGE_SIMPLE_CONTEXT_ELEMENT, *PI2O_SGE_SIMPLE_CONTEXT_ELEMENT;

/* Transport Detail Element */

typedef struct _I2O_SGE_TRANSPORT_ELEMENT {
#if (defined(__BORLANDC__))
   U32			       LongElementLength;
#else
   BF			       LongElementLength:I2O_SG_COUNT_SZ;
   BF			       Flags:I2O_SG_FLAGS_SZ;
#endif
} I2O_SGE_TRANSPORT_ELEMENT, *PI2O_SGE_TRANSPORT_ELEMENT;

typedef struct _I2O_SG_ELEMENT {
   union {
       /* Bit Bucket Element */
       I2O_SGE_BIT_BUCKET_ELEMENT	   BitBucket;

       /* Chain Addressing Element */
       I2O_SGE_CHAIN_ELEMENT		   Chain;

       /* Chain Addressing with Context Element */
       I2O_SGE_CHAIN_CONTEXT_ELEMENT	   ChainContext;

       /* Ignore Scatter-Gather Element */
       I2O_SGE_IGNORE_ELEMENT		   Ignore;

       /* Immediate Data Element */
       I2O_SGE_IMMEDIATE_DATA_ELEMENT	   ImmediateData;

       /* Immediate Data with Context Element */
       I2O_SGE_IMMEDIATE_DATA_CONTEXT_ELEMENT  ImmediateDataContext;

       /* Long Transaction Parameters Element */
       I2O_SGE_LONG_TRANSACTION_ELEMENT	   LongTransaction;

       /* Page List Element */
       I2O_SGE_PAGE_ELEMENT		   Page;

       /* Page List with Context Element */
       I2O_SGE_PAGE_CONTEXT_ELEMENT	   PageContext;

       /* SGL Attribute Element */
       I2O_SGE_SGL_ATTRIBUTES_ELEMENT	   SGLAttribute;

       /* Short Transaction Parameters Element */
       I2O_SGE_SHORT_TRANSACTION_ELEMENT   ShortTransaction;

       /* Simple Addressing Element */
       I2O_SGE_SIMPLE_ELEMENT		   Simple[1];

       /* Simple Addressing with Context Element */
       I2O_SGE_SIMPLE_CONTEXT_ELEMENT	   SimpleContext[1];

       /* Transport Detail Element */
       I2O_SGE_TRANSPORT_ELEMENT	   Transport;
#if (defined(sun) && defined(u))
/* there is a macro defined in Solaris sys/user.h for u, rename this to uSG */
   } uSG ;
#else
   } u ;
#endif
} I2O_SG_ELEMENT, *PI2O_SG_ELEMENT;

/****************************************************************************/
/*  Basic Parameter Group Access */
/****************************************************************************/

/* Operation Function Numbers */

#define	  I2O_PARAMS_OPERATION_FIELD_GET	       0x0001
#define	  I2O_PARAMS_OPERATION_LIST_GET		       0x0002
#define	  I2O_PARAMS_OPERATION_MORE_GET		       0x0003
#define	  I2O_PARAMS_OPERATION_SIZE_GET		       0x0004
#define	  I2O_PARAMS_OPERATION_TABLE_GET	       0x0005
#define	  I2O_PARAMS_OPERATION_FIELD_SET	       0x0006
#define	  I2O_PARAMS_OPERATION_LIST_SET		       0x0007
#define	  I2O_PARAMS_OPERATION_ROW_ADD		       0x0008
#define	  I2O_PARAMS_OPERATION_ROW_DELETE	       0x0009
#define	  I2O_PARAMS_OPERATION_TABLE_CLEAR	       0x000A

/* Operations List Header */

typedef struct _I2O_PARAM_OPERATIONS_LIST_HEADER {
   U16			       OperationCount;
   U16			       Reserved;
} I2O_PARAM_OPERATIONS_LIST_HEADER, *PI2O_PARAM_OPERATIONS_LIST_HEADER;

/* Results List Header */

typedef struct _I2O_PARAM_RESULTS_LIST_HEADER {
   U16			       ResultCount;
   U16			       Reserved;
} I2O_PARAM_RESULTS_LIST_HEADER, *PI2O_PARAM_RESULTS_LIST_HEADER;

/* Read Operation Result Block Template Structure */

typedef struct _I2O_PARAM_READ_OPERATION_RESULT {
   U16			       BlockSize;
   U8			       BlockStatus;
   U8			       ErrorInfoSize;
   /*			       Operations Results	   */
   /*			       Pad (if any)		   */
   /*			       ErrorInformation (if any)   */
} I2O_PARAM_READ_OPERATION_RESULT, *PI2O_PARAM_READ_OPERATION_RESULT;

typedef struct _I2O_TABLE_READ_OPERATION_RESULT {
   U16			       BlockSize;
   U8			       BlockStatus;
   U8			       ErrorInfoSize;
   U16			       RowCount;
   U16			       MoreFlag;
   /*			       Operations Results	   */
   /*			       Pad (if any)		   */
   /*			       ErrorInformation (if any)   */
} I2O_TABLE_READ_OPERATION_RESULT, *PI2O_TABLE_READ_OPERATION_RESULT;

/* Error Information Template Structure */

typedef struct _I2O_PARAM_ERROR_INFO_TEMPLATE {
   U16			       OperationCode;
   U16			       GroupNumber;
   U16			       FieldIdx;
   U8			       AdditionalStatus;
   U8			       NumberKeys;
   /*			       List of Key Values (variable)   */
   /*			       Pad (if any)		       */
} I2O_PARAM_ERROR_INFO_TEMPLATE, *PI2O_PARAM_ERROR_INFO_TEMPLATE;

/* Operation Template for Specific Fields */

typedef struct _I2O_PARAM_OPERATION_SPECIFIC_TEMPLATE {
   U16			       Operation;
   U16			       GroupNumber;
   U16			       FieldCount;
   U16			       FieldIdx[1];
   /*			       Pad (if any)		       */
} I2O_PARAM_OPERATION_SPECIFIC_TEMPLATE, *PI2O_PARAM_OPERATION_SPECIFIC_TEMPLATE;

/* Operation Template for All Fields */

typedef struct _I2O_PARAM_OPERATION_ALL_TEMPLATE {
   U16			       Operation;
   U16			       GroupNumber;
   U16			       FieldCount;
   /*			       Pad (if any)		       */
} I2O_PARAM_OPERATION_ALL_TEMPLATE, *PI2O_PARAM_OPERATION_ALL_TEMPLATE;

/* Operation Template for All List Fields */

typedef struct _I2O_PARAM_OPERATION_ALL_LIST_TEMPLATE {
   U16			       Operation;
   U16			       GroupNumber;
   U16			       FieldCount;
   U16			       KeyCount;
   U8			       KeyValue;
   /*			       Pad (if any)		       */
} I2O_PARAM_OPERATION_ALL_LIST_TEMPLATE, *PI2O_PARAM_OPERATION_ALL_LIST_TEMPLATE;

/* Modify Operation Result Block Template Structure */

typedef struct _I2O_PARAM_MODIFY_OPERATION_RESULT {
   U16			       BlockSize;
   U8			       BlockStatus;
   U8			       ErrorInfoSize;
   /*			       ErrorInformation (if any)   */
} I2O_PARAM_MODIFY_OPERATION_RESULT, *PI2O_PARAM_MODIFY_OPERATION_RESULT;

/* Operation Template for Row Delete  */

typedef struct _I2O_PARAM_OPERATION_ROW_DELETE_TEMPLATE {
   U16			       Operation;
   U16			       GroupNumber;
   U16			       RowCount;
   U8			       KeyValue;
} I2O_PARAM_OPERATION_ROW_DELETE_TEMPLATE, *PI2O_PARAM_OPERATION_ROW_DELETE_TEMPLATE;

/* Operation Template for Table Clear  */

typedef struct _I2O_PARAM_OPERATION_TABLE_CLEAR_TEMPLATE {
   U16			       Operation;
   U16			       GroupNumber;
} I2O_PARAM_OPERATION_TABLE_CLEAR_TEMPLATE, *PI2O_PARAM_OPERATION_TABLE_CLEAR_TEMPLATE;

/* Status codes and Error Information for Parameter functions */

#define	  I2O_PARAMS_STATUS_SUCCESS		   0x00
#define	  I2O_PARAMS_STATUS_BAD_KEY_ABORT	   0x01
#define	  I2O_PARAMS_STATUS_BAD_KEY_CONTINUE	   0x02
#define	  I2O_PARAMS_STATUS_BUFFER_FULL		   0x03
#define	  I2O_PARAMS_STATUS_BUFFER_TOO_SMALL	   0x04
#define	  I2O_PARAMS_STATUS_FIELD_UNREADABLE	   0x05
#define	  I2O_PARAMS_STATUS_FIELD_UNWRITEABLE	   0x06
#define	  I2O_PARAMS_STATUS_INSUFFICIENT_FIELDS	   0x07
#define	  I2O_PARAMS_STATUS_INVALID_GROUP_ID	   0x08
#define	  I2O_PARAMS_STATUS_INVALID_OPERATION	   0x09
#define	  I2O_PARAMS_STATUS_NO_KEY_FIELD	   0x0A
#define	  I2O_PARAMS_STATUS_NO_SUCH_FIELD	   0x0B
#define	  I2O_PARAMS_STATUS_NON_DYNAMIC_GROUP	   0x0C
#define	  I2O_PARAMS_STATUS_OPERATION_ERROR	   0x0D
#define	  I2O_PARAMS_STATUS_SCALAR_ERROR	   0x0E
#define	  I2O_PARAMS_STATUS_TABLE_ERROR		   0x0F
#define	  I2O_PARAMS_STATUS_WRONG_GROUP_TYPE	   0x10


/****************************************************************************/
/* GROUP Parameter Groups */
/****************************************************************************/

/* GROUP Configuration and Operating Structures and Defines */

/* Groups Numbers */

#define	   I2O_UTIL_PARAMS_DESCRIPTOR_GROUP_NO		0xF000
#define	   I2O_UTIL_PHYSICAL_DEVICE_TABLE_GROUP_NO	0xF001
#define	   I2O_UTIL_CLAIMED_TABLE_GROUP_NO		0xF002
#define	   I2O_UTIL_USER_TABLE_GROUP_NO			0xF003
#define	   I2O_UTIL_PRIVATE_MESSAGE_EXTENSIONS_GROUP_NO 0xF005
#define	   I2O_UTIL_AUTHORIZED_USER_TABLE_GROUP_NO	0xF006
#define	   I2O_UTIL_DEVICE_IDENTITY_GROUP_NO		0xF100
#define	   I2O_UTIL_DDM_IDENTITY_GROUP_NO		0xF101
#define	   I2O_UTIL_USER_INFORMATION_GROUP_NO		0xF102
#define	   I2O_UTIL_SGL_OPERATING_LIMITS_GROUP_NO	0xF103
#define	   I2O_UTIL_SENSORS_GROUP_NO			0xF200

/* UTIL Group F000h - GROUP DESCRIPTORS Parameter Group */

#define	   I2O_UTIL_GROUP_PROPERTIES_GROUP_TABLE       0x01
#define	   I2O_UTIL_GROUP_PROPERTIES_ROW_ADDITION      0x02
#define	   I2O_UTIL_GROUP_PROPERTIES_ROW_DELETION      0x04
#define	   I2O_UTIL_GROUP_PROPERTIES_CLEAR_OPERATION   0x08

typedef struct _I2O_UTIL_GROUP_DESCRIPTOR_TABLE {
   U16			       GroupNumber;
   U16			       FieldCount;
   U16			       RowCount;
   U8			       Properties;
   U8			       reserved;
} I2O_UTIL_GROUP_DESCRIPTOR_TABLE, *PI2O_UTIL_GROUP_DESCRIPTOR_TABLE;

/* UTIL Group F001h - Physical Device Table Parameter Group */

typedef struct _I2O_UTIL_PHYSICAL_DEVICE_TABLE {
   U32			       AdapterID;
} I2O_UTIL_PHYSICAL_DEVICE_TABLE, *PI2O_UTIL_PHYSICAL_DEVICE_TABLE;

/* UTIL Group F002h - Claimed Table Parameter Group */

typedef struct _I2O_UTIL_CLAIMED_TABLE {
   U16			       ClaimedTID;
} I2O_UTIL_CLAIMED_TABLE, *PI2O_UTIL_CLAIMED_TABLE;

/* UTIL Group F003h - User Table Parameter Group */

typedef struct _I2O_UTIL_USER_TABLE {
   U16			       Instance;
   U16			       UserTID;
   U8			       ClaimType;
   U8			       reserved1;
   U16			       reserved2;
} I2O_UTIL_USER_TABLE, *PI2O_UTIL_USER_TABLE;

/* UTIL Group F005h - Private Message Extensions Parameter Group */

typedef struct _I2O_UTIL_PRIVATE_MESSAGE_EXTENSIONS_TABLE {
   U16			       ExtInstance;
   U16			       OrganizationID;
   U16			       XFunctionCode;
} I2O_UTIL_PRIVATE_MESSAGE_EXTENSIONS_TABLE, *PI2O_UTIL_PRIVATE_MESSAGE_EXTENSIONS_TABLE;

/* UTIL Group F006h - Authorized User Table Parameter Group */

typedef struct _I2O_UTIL_AUTHORIZED_USER_TABLE {
   U16			       AlternateTID;
} I2O_UTIL_AUTHORIZED_USER_TABLE, *PI2O_UTIL_AUTHORIZED_USER_TABLE;

/* UTIL Group F100h - Device Identity Parameter Group */

typedef struct _I2O_UTIL_DEVICE_IDENTITY_SCALAR {
   U32			       ClassID;
   U16			       OwnerTID;
   U16			       ParentTID;
   U8			       VendorInfo[I2O_DEVID_VENDOR_INFO_SZ];
   U8			       ProductInfo[I2O_DEVID_PRODUCT_INFO_SZ];
   U8			       Description[I2O_DEVID_DESCRIPTION_SZ];
   U8			       ProductRevLevel[I2O_DEVID_REV_LEVEL_SZ];
   U8			       SNFormat;
   U8			       SerialNumber[I2O_MAX_SERIAL_NUMBER_SZ];
} I2O_UTIL_DEVICE_IDENTITY_SCALAR, *PI2O_UTIL_DEVICE_IDENTITY_SCALAR;

/* UTIL Group F101h - DDM Identity Parameter Group */

typedef struct _I2O_UTIL_DDM_IDENTITY_SCALAR {
   U16			       DdmTID;
   U8			       ModuleName[I2O_MODULE_NAME_SZ];
   U8			       ModuleRevLevel[I2O_DEVID_REV_LEVEL_SZ];
   U8			       SNFormat;
   U8			       SerialNumber[I2O_MAX_SERIAL_NUMBER_SZ];
} I2O_UTIL_DDM_IDENTITY_SCALAR, *PI2O_UTIL_DDM_IDENTITY_SCALAR;

/* UTIL Group F102h - User Information Parameter Group */

#define	   I2O_USER_DEVICE_NAME_SZ		       64
#define	   I2O_USER_SERVICE_NAME_SZ		       64
#define	   I2O_USER_PHYSICAL_LOCATION_SZ	       64

typedef struct _I2O_UTIL_USER_INFORMATION_SCALAR {
   U8			       DeviceName[I2O_USER_DEVICE_NAME_SZ];
   U8			       ServiceName[I2O_USER_SERVICE_NAME_SZ];
   U8			       PhysicalLocation[I2O_USER_PHYSICAL_LOCATION_SZ];
   U32			       InstanceNumber;
} I2O_UTIL_USER_INFORMATION_SCALAR, *PI2O_UTIL_USER_INFORMATION_SCALAR;

/* UTIL Group F103h - SGL Operating Limits Parameter Group */

typedef struct _I2O_UTIL_SGL_OPERATING_LIMITS_SCALAR {
   U32			       SglChainSize;
   U32			       SglChainSizeMax;
   U32			       SglChainSizeTarget;
   U16			       SglFragCount;
   U16			       SglFragCountMax;
   U16			       SglFragCountTarget;
} I2O_UTIL_SGL_OPERATING_LIMITS_SCALAR, *PI2O_UTIL_SGL_OPERATING_LIMITS_SCALAR;

/* UTIL Group F200h - Sensors Parameter Group */

#define	   I2O_SENSOR_COMPONENT_OTHER		       0x00
#define	   I2O_SENSOR_COMPONENT_PLANAR_LOGIC_BOARD     0x01
#define	   I2O_SENSOR_COMPONENT_CPU		       0x02
#define	   I2O_SENSOR_COMPONENT_CHASSIS		       0x03
#define	   I2O_SENSOR_COMPONENT_POWER_SUPPLY	       0x04
#define	   I2O_SENSOR_COMPONENT_STORAGE		       0x05
#define	   I2O_SENSOR_COMPONENT_EXTERNAL	       0x06

#define	   I2O_SENSOR_SENSOR_CLASS_ANALOG	       0x00
#define	   I2O_SENSOR_SENSOR_CLASS_DIGITAL	       0x01

#define	   I2O_SENSOR_SENSOR_TYPE_OTHER		       0x00
#define	   I2O_SENSOR_SENSOR_TYPE_THERMAL	       0x01
#define	   I2O_SENSOR_SENSOR_TYPE_DC_VOLTAGE	       0x02
#define	   I2O_SENSOR_SENSOR_TYPE_AC_VOLTAGE	       0x03
#define	   I2O_SENSOR_SENSOR_TYPE_DC_CURRENT	       0x04
#define	   I2O_SENSOR_SENSOR_TYPE_AC_CURRENT	       0x05
#define	   I2O_SENSOR_SENSOR_TYPE_DOOR_OPEN	       0x06
#define	   I2O_SENSOR_SENSOR_TYPE_FAN_OPERATIONAL      0x07

#define	   I2O_SENSOR_SENSOR_STATE_NORMAL	       0x00
#define	   I2O_SENSOR_SENSOR_STATE_ABNORMAL	       0x01
#define	   I2O_SENSOR_SENSOR_STATE_UNKNOWN	       0x02
#define	   I2O_SENSOR_SENSOR_STATE_LOW_CAT	       0x03
#define	   I2O_SENSOR_SENSOR_STATE_LOW		       0x04
#define	   I2O_SENSOR_SENSOR_STATE_LOW_WARNING	       0x05
#define	   I2O_SENSOR_SENSOR_STATE_HIGH_WARNING	       0x06
#define	   I2O_SENSOR_SENSOR_STATE_HIGH		       0x07
#define	   I2O_SENSOR_SENSOR_STATE_HIGH_CAT	       0x08

#define	   I2O_SENSOR_EVENT_ENABLE_STATE_CHANGE	       0x0001
#define	   I2O_SENSOR_EVENT_ENABLE_LOW_CATASTROPHIC    0x0002
#define	   I2O_SENSOR_EVENT_ENABLE_LOW_READING	       0x0004
#define	   I2O_SENSOR_EVENT_ENABLE_LOW_WARNING	       0x0008
#define	   I2O_SENSOR_EVENT_ENABLE_CHANGE_TO_NORMAL    0x0010
#define	   I2O_SENSOR_EVENT_ENABLE_HIGH_WARNING	       0x0020
#define	   I2O_SENSOR_EVENT_ENABLE_HIGH_READING	       0x0040
#define	   I2O_SENSOR_EVENT_ENABLE_HIGH_CATASTROPHIC   0x0080


typedef struct _I2O_UTIL_SENSORS_TABLE {
   U16			       SensorInstance;
   U8			       Component;
   U16			       ComponentInstance;
   U8			       SensorClass;
   U8			       SensorType;
   S8			       ScalingExponent;
   S32			       ActualReading;
   S32			       MinimumReading;
   S32			       Low2LowCatThreshold;
   S32			       LowCat2LowThreshold;
   S32			       LowWarn2LowThreshold;
   S32			       Low2LowWarnThreshold;
   S32			       Norm2LowWarnThreshold;
   S32			       LowWarn2NormThreshold;
   S32			       NominalReading;
   S32			       HiWarn2NormThreshold;
   S32			       Norm2HiWarnThreshold;
   S32			       High2HiWarnThreshold;
   S32			       HiWarn2HighThreshold;
   S32			       HiCat2HighThreshold;
   S32			       Hi2HiCatThreshold;
   S32			       MaximumReading;
   U8			       SensorState;
   U16			       EventEnable;
} I2O_UTIL_SENSORS_TABLE, *PI2O_UTIL_SENSORS_TABLE;


PRAGMA_PACK_POP

PRAGMA_ALIGN_POP

#endif	  /* I2O_MESSAGE_HDR */
