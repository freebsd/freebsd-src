/****************************************************************
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation.
 * All rights reserved.
 *
 * Copyright 1999 I2O Special Interest Group (I2O SIG).  All rights reserved.
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
 * conditions.  By accepting and/or using this header file, you agree to abide
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
 * on this header file.  Any distribution of such derivative work: (1) must
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
 * header file at the I2O SIG Web site.  Furthermore, to become a Registered
 * Developer of the I2O SIG, sign up at the Web site or call 415.750.8352
 * (United States).
 *
 * $FreeBSD$
 *
 ****************************************************************/

#if !defined(I2O_BASE_SCSI_HDR)
#define I2O_BASE_SCSI_HDR

#if ((defined(KERNEL) || defined(_KERNEL)) && defined(__FreeBSD__))
# if (KERN_VERSION < 3)
#  include    "i386/pci/i2omsg.h"          /* Include the Base Message file */
# else
#  include    "dev/asr/i2omsg.h"
# endif
#else
# include    "i2omsg.h"          /* Include the Base Message file */
#endif


#define I2OBSCSI_REV 1_5_1      /* Header file revision string */



/*****************************************************************************
 *
 *    I2OBSCSI.h -- I2O Base SCSI Device Class Message defintion file
 *
 *      This file contains information presented in Chapter 6, Section 6 & 7 of
 *      the I2O Specification.
 *
 *  Revision History: (Revision History tracks the revision number of the I2O
 *          specification)
 *
 *      .92 - First marked revsion used for Proof of Concept.
 *      .93 - Change to match the rev .93 of the spec.
 *      .95 - Updated to Rev .95 of 2/5/96.
 *     1.00 - Checked and Updated against spec version 1.00 4/9/96.
 *     1.xx - Updated to the 1.x version of the I2O Specification on 11/11/96.
 *     1.xx - 11/14/96
 *            1) Removed duplicate device type definitions.
 *            2) Added "DSC" to Detailed Status Code definitions.
 *            3) Changed SCSI-3 LUN fields from U64 to U8 array.
 *     1.xx   11/15/96 - Added #pragma statments for i960.
 *     1.xx   11/20/96 - Changed duplicate Bus Scan structure to Bus Reset.
 *     1.xx   12/05/96 - Added Auto Request Sense flag definition.
 *     1.5d   03/06/97 - Update for spec. draft version 1.5d.
 *            1) Converted SCSI bus adapter class to generic in i2oadptr.h.
 *            2) Fixed DSC reference:  changed from _BUS_SCAN to _BUS_RESET.
 *     1.5d   03/031/97 - Made AutoSense flag definition consistent with spec.
 *     1.5d   04/11/97 - Corrections from review cycle:
 *            1) Corrected typo in I2O_SCSI_PERIPHERAL_TYPE_PARALLEL.
 *            2) Corrected typo in I2O_SCSI_PORT_CONN_UNSHIELDED_P_HD.
 *     1.5.1  05/02/97 - Corrections from review cycle:
 *            1) Remove #include for i2omstor.h.
 *            2) Add revision string.
 *            3) Convert tabs to spaces.
 *            4) New disclaimer.
 *
 *****************************************************************************/

/*
    NOTES:

    Gets, reads, receives, etc. are all even numbered functions.
    Sets, writes, sends, etc. are all odd numbered functions.
    Functions that both send and receive data can be either but an attempt is made
    to use the function number that indicates the greater transfer amount.
    Functions that do not send or receive data use odd function numbers.

    Some functions are synonyms like read, receive and send, write.

    All common functions will have a code of less than 0x80.
    Unique functions to a class will start at 0x80.
    Executive Functions start at 0xA0.

    Utility Message function codes range from 0 - 0x1f
    Base Message function codes range from 0x20 - 0xfe
    Private Message function code is 0xff.
*/

PRAGMA_ALIGN_PUSH

PRAGMA_PACK_PUSH

/*
    SCSI Peripheral Class specific functions

    Although the names are SCSI Peripheral class specific, the values
    assigned are common with other classes when applicable.
*/

#define     I2O_SCSI_DEVICE_RESET           0x27
#define     I2O_SCSI_SCB_ABORT              0x83
#define     I2O_SCSI_SCB_EXEC               0x81

/*
    Detailed Status Codes for SCSI operations

    The 16-bit Detailed Status Code field for SCSI operations is divided
    into two separate 8-bit fields.  The lower 8 bits are used to report
    Device Status information.  The upper 8 bits are used to report
    Adapter Status information.  The definitions for these two fields,
    however, will be consistent with the standard reply message frame
    structure declaration, which treats this as a single 16-bit field.
*/


/*  SCSI Device Completion Status Codes (defined by SCSI-2/3)*/

#define I2O_SCSI_DEVICE_DSC_MASK                0x00FF

#define I2O_SCSI_DSC_SUCCESS                    0x0000
#define I2O_SCSI_DSC_CHECK_CONDITION            0x0002
#define I2O_SCSI_DSC_BUSY                       0x0008
#define I2O_SCSI_DSC_RESERVATION_CONFLICT       0x0018
#define I2O_SCSI_DSC_COMMAND_TERMINATED         0x0022
#define I2O_SCSI_DSC_TASK_SET_FULL              0x0028
#define I2O_SCSI_DSC_ACA_ACTIVE                 0x0030

/*  SCSI Adapter Status Codes (based on CAM-1) */

#define I2O_SCSI_HBA_DSC_MASK                   0xFF00

#define I2O_SCSI_HBA_DSC_SUCCESS                0x0000

#define I2O_SCSI_HBA_DSC_REQUEST_ABORTED        0x0200
#define I2O_SCSI_HBA_DSC_UNABLE_TO_ABORT        0x0300
#define I2O_SCSI_HBA_DSC_COMPLETE_WITH_ERROR    0x0400
#define I2O_SCSI_HBA_DSC_ADAPTER_BUSY           0x0500
#define I2O_SCSI_HBA_DSC_REQUEST_INVALID        0x0600
#define I2O_SCSI_HBA_DSC_PATH_INVALID           0x0700
#define I2O_SCSI_HBA_DSC_DEVICE_NOT_PRESENT     0x0800
#define I2O_SCSI_HBA_DSC_UNABLE_TO_TERMINATE    0x0900
#define I2O_SCSI_HBA_DSC_SELECTION_TIMEOUT      0x0A00
#define I2O_SCSI_HBA_DSC_COMMAND_TIMEOUT        0x0B00

#define I2O_SCSI_HBA_DSC_MR_MESSAGE_RECEIVED    0x0D00
#define I2O_SCSI_HBA_DSC_SCSI_BUS_RESET         0x0E00
#define I2O_SCSI_HBA_DSC_PARITY_ERROR_FAILURE   0x0F00
#define I2O_SCSI_HBA_DSC_AUTOSENSE_FAILED       0x1000
#define I2O_SCSI_HBA_DSC_NO_ADAPTER             0x1100
#define I2O_SCSI_HBA_DSC_DATA_OVERRUN           0x1200
#define I2O_SCSI_HBA_DSC_UNEXPECTED_BUS_FREE    0x1300
#define I2O_SCSI_HBA_DSC_SEQUENCE_FAILURE       0x1400
#define I2O_SCSI_HBA_DSC_REQUEST_LENGTH_ERROR   0x1500
#define I2O_SCSI_HBA_DSC_PROVIDE_FAILURE        0x1600
#define I2O_SCSI_HBA_DSC_BDR_MESSAGE_SENT       0x1700
#define I2O_SCSI_HBA_DSC_REQUEST_TERMINATED     0x1800

#define I2O_SCSI_HBA_DSC_IDE_MESSAGE_SENT       0x3300
#define I2O_SCSI_HBA_DSC_RESOURCE_UNAVAILABLE   0x3400
#define I2O_SCSI_HBA_DSC_UNACKNOWLEDGED_EVENT   0x3500
#define I2O_SCSI_HBA_DSC_MESSAGE_RECEIVED       0x3600
#define I2O_SCSI_HBA_DSC_INVALID_CDB            0x3700
#define I2O_SCSI_HBA_DSC_LUN_INVALID            0x3800
#define I2O_SCSI_HBA_DSC_SCSI_TID_INVALID       0x3900
#define I2O_SCSI_HBA_DSC_FUNCTION_UNAVAILABLE   0x3A00
#define I2O_SCSI_HBA_DSC_NO_NEXUS               0x3B00
#define I2O_SCSI_HBA_DSC_SCSI_IID_INVALID       0x3C00
#define I2O_SCSI_HBA_DSC_CDB_RECEIVED           0x3D00
#define I2O_SCSI_HBA_DSC_LUN_ALREADY_ENABLED    0x3E00
#define I2O_SCSI_HBA_DSC_BUS_BUSY               0x3F00

#define I2O_SCSI_HBA_DSC_QUEUE_FROZEN           0x4000


/****************************************************************************/

/* SCSI Peripheral Device Parameter Groups */

/****************************************************************************/


/* SCSI Configuration and Operating Structures and Defines */


#define     I2O_SCSI_DEVICE_INFO_GROUP_NO               0x0000
#define     I2O_SCSI_DEVICE_BUS_PORT_INFO_GROUP_NO      0x0001


/* - 0000h - SCSI Device Information Parameters Group defines */

/* Device Type */

#define I2O_SCSI_DEVICE_TYPE_DIRECT         0x00
#define I2O_SCSI_DEVICE_TYPE_SEQUENTIAL     0x01
#define I2O_SCSI_DEVICE_TYPE_PRINTER        0x02
#define I2O_SCSI_DEVICE_TYPE_PROCESSOR      0x03
#define I2O_SCSI_DEVICE_TYPE_WORM           0x04
#define I2O_SCSI_DEVICE_TYPE_CDROM          0x05
#define I2O_SCSI_DEVICE_TYPE_SCANNER        0x06
#define I2O_SCSI_DEVICE_TYPE_OPTICAL        0x07
#define I2O_SCSI_DEVICE_TYPE_MEDIA_CHANGER  0x08
#define I2O_SCSI_DEVICE_TYPE_COMM           0x09
#define I2O_SCSI_DEVICE_GRAPHICS_1          0x0A
#define I2O_SCSI_DEVICE_GRAPHICS_2          0x0B
#define I2O_SCSI_DEVICE_TYPE_ARRAY_CONT     0x0C
#define I2O_SCSI_DEVICE_TYPE_SES            0x0D
#define I2O_SCSI_DEVICE_TYPE_UNKNOWN        0x1F

/* Flags */

#define I2O_SCSI_PERIPHERAL_TYPE_FLAG       0x01
#define I2O_SCSI_PERIPHERAL_TYPE_PARALLEL   0x00
#define I2O_SCSI_PERIPHERAL_TYPE_SERIAL     0x01

#define I2O_SCSI_RESERVED_FLAG              0x02

#define I2O_SCSI_DISCONNECT_FLAG            0x04
#define I2O_SCSI_DISABLE_DISCONNECT         0x00
#define I2O_SCSI_ENABLE_DISCONNECT          0x04

#define I2O_SCSI_MODE_MASK                  0x18
#define I2O_SCSI_MODE_SET_DATA              0x00
#define I2O_SCSI_MODE_SET_DEFAULT           0x08
#define I2O_SCSI_MODE_SET_SAFEST            0x10

#define I2O_SCSI_DATA_WIDTH_MASK            0x60
#define I2O_SCSI_DATA_WIDTH_8               0x00
#define I2O_SCSI_DATA_WIDTH_16              0x20
#define I2O_SCSI_DATA_WIDTH_32              0x40

#define I2O_SCSI_SYNC_NEGOTIATION_FLAG      0x80
#define I2O_SCSI_DISABLE_SYNC_NEGOTIATION   0x00
#define I2O_SCSI_ENABLE_SYNC_NEGOTIATION    0x80


/* - 0001h - SCSI Device Bus Port Info Parameters Group defines */

/* Physical */

#define I2O_SCSI_PORT_PHYS_OTHER            0x01
#define I2O_SCSI_PORT_PHYS_UNKNOWN          0x02
#define I2O_SCSI_PORT_PHYS_PARALLEL         0x03
#define I2O_SCSI_PORT_PHYS_FIBRE_CHANNEL    0x04
#define I2O_SCSI_PORT_PHYS_SERIAL_P1394     0x05
#define I2O_SCSI_PORT_PHYS_SERIAL_SSA       0x06

/* Electrical */

#define I2O_SCSI_PORT_ELEC_OTHER            0x01
#define I2O_SCSI_PORT_ELEC_UNKNOWN          0x02
#define I2O_SCSI_PORT_ELEC_SINGLE_ENDED     0x03
#define I2O_SCSI_PORT_ELEC_DIFFERENTIAL     0x04
#define I2O_SCSI_PORT_ELEC_LOW_VOLT_DIFF    0x05
#define I2O_SCSI_PORT_ELEC_OPTICAL          0x06

/* Isochronous */

#define I2O_SCSI_PORT_ISOC_NO               0x00
#define I2O_SCSI_PORT_ISOC_YES              0x01
#define I2O_SCSI_PORT_ISOC_UNKNOWN          0x02

/* Connector Type */

#define I2O_SCSI_PORT_CONN_OTHER            0x01
#define I2O_SCSI_PORT_CONN_UNKNOWN          0x02
#define I2O_SCSI_PORT_CONN_NONE             0x03
#define I2O_SCSI_PORT_CONN_SHIELDED_A_HD    0x04
#define I2O_SCSI_PORT_CONN_UNSHIELDED_A_HD  0x05
#define I2O_SCSI_PORT_CONN_SHIELDED_A_LD    0x06
#define I2O_SCSI_PORT_CONN_UNSHIELDED_A_LD  0x07
#define I2O_SCSI_PORT_CONN_SHIELDED_P_HD    0x08
#define I2O_SCSI_PORT_CONN_UNSHIELDED_P_HD  0x09
#define I2O_SCSI_PORT_CONN_SCA_I            0x0A
#define I2O_SCSI_PORT_CONN_SCA_II           0x0B
#define I2O_SCSI_PORT_CONN_FC_DB9           0x0C
#define I2O_SCSI_PORT_CONN_FC_FIBRE         0x0D
#define I2O_SCSI_PORT_CONN_FC_SCA_II_40     0x0E
#define I2O_SCSI_PORT_CONN_FC_SCA_II_20     0x0F
#define I2O_SCSI_PORT_CONN_FC_BNC           0x10

/* Connector Gender */

#define I2O_SCSI_PORT_CONN_GENDER_OTHER     0x01
#define I2O_SCSI_PORT_CONN_GENDER_UNKOWN    0x02
#define I2O_SCSI_PORT_CONN_GENDER_FEMALE    0x03
#define I2O_SCSI_PORT_CONN_GENDER_MALE      0x04


/* SCSI Device Group 0000h - Device Information Parameter Group */

typedef struct _I2O_SCSI_DEVICE_INFO_SCALAR {
    U8          DeviceType;
    U8          Flags;
    U16         Reserved2;
    U32         Identifier;
    U8          LunInfo[8]; /* SCSI-2 8-bit scalar LUN goes into offset 1 */
    U32         QueueDepth;
    U8          Reserved1a;
    U8          NegOffset;
    U8          NegDataWidth;
    U8          Reserved1b;
    U64         NegSyncRate;

} I2O_SCSI_DEVICE_INFO_SCALAR, *PI2O_SCSI_DEVICE_INFO_SCALAR;


/* SCSI Device Group 0001h - Bus Port Information Parameter Group */

typedef struct _I2O_SCSI_BUS_PORT_INFO_SCALAR {
    U8          PhysicalInterface;
    U8          ElectricalInterface;
    U8          Isochronous;
    U8          ConnectorType;
    U8          ConnectorGender;
    U8          Reserved1;
    U16         Reserved2;
    U32         MaxNumberDevices;
} I2O_SCSI_BUS_PORT_INFO_SCALAR, *PI2O_SCSI_BUS_PORT_INFO_SCALAR;



/****************************************************************************/

/* I2O SCSI Peripheral Event Indicator Assignment */

#define I2O_SCSI_EVENT_SCSI_SMART               0x00000010


/****************************************************************************/

/* SCSI Peripheral Class Specific Message Definitions */

/****************************************************************************/


/****************************************************************************/

/* I2O SCSI Peripheral Successful Completion Reply Message Frame */

typedef struct _I2O_SCSI_SUCCESS_REPLY_MESSAGE_FRAME {
    I2O_SINGLE_REPLY_MESSAGE_FRAME StdReplyFrame;
    U32                     TransferCount;
} I2O_SCSI_SUCCESS_REPLY_MESSAGE_FRAME, *PI2O_SCSI_SUCCESS_REPLY_MESSAGE_FRAME;


/****************************************************************************/

/* I2O SCSI Peripheral Error Report Reply Message Frame */

#ifdef _WIN64
#define I2O_SCSI_SENSE_DATA_SZ      44
#else
#define I2O_SCSI_SENSE_DATA_SZ      40
#endif

typedef struct _I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME {
    I2O_SINGLE_REPLY_MESSAGE_FRAME StdReplyFrame;
    U32                     TransferCount;
    U32                     AutoSenseTransferCount;
    U8                      SenseData[I2O_SCSI_SENSE_DATA_SZ];
} I2O_SCSI_ERROR_REPLY_MESSAGE_FRAME, *PI2O_SCSI_ERROR_REPLY_MESSAGE_FRAME;


/****************************************************************************/

/* I2O SCSI Device Reset Message Frame */

typedef struct _I2O_SCSI_DEVICE_RESET_MESSAGE {
    I2O_MESSAGE_FRAME       StdMessageFrame;
    I2O_TRANSACTION_CONTEXT TransactionContext;
} I2O_SCSI_DEVICE_RESET_MESSAGE, *PI2O_SCSI_DEVICE_RESET_MESSAGE;


/****************************************************************************/

/* I2O SCSI Control Block Abort Message Frame */

typedef struct _I2O_SCSI_SCB_ABORT_MESSAGE {
    I2O_MESSAGE_FRAME       StdMessageFrame;
    I2O_TRANSACTION_CONTEXT TransactionContext;
    I2O_TRANSACTION_CONTEXT TransactionContextToAbort;
} I2O_SCSI_SCB_ABORT_MESSAGE, *PI2O_SCSI_SCB_ABORT_MESSAGE;


/****************************************************************************/

/* I2O SCSI Control Block Execute Message Frame */

#define  I2O_SCSI_CDB_LENGTH                16

typedef U16     I2O_SCB_FLAGS;

#define I2O_SCB_FLAG_XFER_DIR_MASK          0xC000
#define I2O_SCB_FLAG_NO_DATA_XFER           0x0000
#define I2O_SCB_FLAG_XFER_FROM_DEVICE       0x4000
#define I2O_SCB_FLAG_XFER_TO_DEVICE         0x8000

#define I2O_SCB_FLAG_ENABLE_DISCONNECT      0x2000

#define I2O_SCB_FLAG_TAG_TYPE_MASK          0x0380
#define I2O_SCB_FLAG_NO_TAG_QUEUEING        0x0000
#define I2O_SCB_FLAG_SIMPLE_QUEUE_TAG       0x0080
#define I2O_SCB_FLAG_HEAD_QUEUE_TAG         0x0100
#define I2O_SCB_FLAG_ORDERED_QUEUE_TAG      0x0180
#define I2O_SCB_FLAG_ACA_QUEUE_TAG          0x0200

#define I2O_SCB_FLAG_AUTOSENSE_MASK         0x0060
#define I2O_SCB_FLAG_DISABLE_AUTOSENSE      0x0000
#define I2O_SCB_FLAG_SENSE_DATA_IN_MESSAGE  0x0020
#define I2O_SCB_FLAG_SENSE_DATA_IN_BUFFER   0x0060

typedef struct _I2O_SCSI_SCB_EXECUTE_MESSAGE {
    I2O_MESSAGE_FRAME       StdMessageFrame;
    I2O_TRANSACTION_CONTEXT TransactionContext;
    U8                      CDBLength;
    U8                      Reserved;
    I2O_SCB_FLAGS           SCBFlags;
    U8                      CDB[I2O_SCSI_CDB_LENGTH];
    U32                     ByteCount;
    I2O_SG_ELEMENT          SGL;
} I2O_SCSI_SCB_EXECUTE_MESSAGE, *PI2O_SCSI_SCB_EXECUTE_MESSAGE;


PRAGMA_PACK_POP

PRAGMA_ALIGN_POP

#endif      /* I2O_BASE_SCSI_HDR */
