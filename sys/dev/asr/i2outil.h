/* $FreeBSD: src/sys/dev/asr/i2outil.h,v 1.1.2.1 2000/09/21 20:33:50 msmith Exp $ */
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
 ****************************************************************/

/*********************************************************************
 * I2OUtil.h -- I2O Utility Class Message defintion file
 *
 * This file contains information presented in Chapter 6 of the I2O
 * Specification.
 **********************************************************************/

#if !defined(I2O_UTILITY_HDR)
#define I2O_UTILITY_HDR

#define I2OUTIL_REV 1_5_4  /* I2OUtil header file revision string */

#if ((defined(KERNEL) || defined(_KERNEL)) && defined(__FreeBSD__))
# if (KERN_VERSION < 3)
#  include   "i386/pci/i2omsg.h"      /* Include the Base Message file */
# else
#  include   "dev/asr/i2omsg.h"
# endif
#else
# include   "i2omsg.h"      /* Include the Base Message file */
#endif


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

/* Utility Message class functions. */

#define    I2O_UTIL_NOP                                0x00
#define    I2O_UTIL_ABORT                              0x01
#define    I2O_UTIL_CLAIM                              0x09
#define    I2O_UTIL_CLAIM_RELEASE                      0x0B
#define    I2O_UTIL_CONFIG_DIALOG                      0x10
#define    I2O_UTIL_DEVICE_RESERVE                     0x0D
#define    I2O_UTIL_DEVICE_RELEASE                     0x0F
#define    I2O_UTIL_EVENT_ACKNOWLEDGE                  0x14
#define    I2O_UTIL_EVENT_REGISTER                     0x13
#define    I2O_UTIL_LOCK                               0x17
#define    I2O_UTIL_LOCK_RELEASE                       0x19
#define    I2O_UTIL_PARAMS_GET                         0x06
#define    I2O_UTIL_PARAMS_SET                         0x05
#define    I2O_UTIL_REPLY_FAULT_NOTIFY                 0x15

/****************************************************************************/

/* ABORT Abort type defines. */

#define    I2O_ABORT_TYPE_EXACT_ABORT                  0x00
#define    I2O_ABORT_TYPE_FUNCTION_ABORT               0x01
#define    I2O_ABORT_TYPE_TRANSACTION_ABORT            0x02
#define    I2O_ABORT_TYPE_WILD_ABORT                   0x03
#define    I2O_ABORT_TYPE_CLEAN_EXACT_ABORT            0x04
#define    I2O_ABORT_TYPE_CLEAN_FUNCTION_ABORT         0x05
#define    I2O_ABORT_TYPE_CLEAN_TRANSACTION_ABORT      0x06
#define    I2O_ABORT_TYPE_CLEAN_WILD_ABORT             0x07

/* UtilAbort Function Message Frame structure. */

typedef struct _I2O_UTIL_ABORT_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
#   if (defined(_DPT_BIG_ENDIAN) || defined(sparc))
        U32                    reserved;
#   else
        U16                    reserved;
        U8                     AbortType;
        U8                     FunctionToAbort;
#   endif
    I2O_TRANSACTION_CONTEXT    TransactionContextToAbort;
} I2O_UTIL_ABORT_MESSAGE, *PI2O_UTIL_ABORT_MESSAGE;


typedef struct _I2O_UTIL_ABORT_REPLY {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        CountOfAbortedMessages;
} I2O_UTIL_ABORT_REPLY, *PI2O_UTIL_ABORT_REPLY;


/****************************************************************************/

/* Claim Flag defines */

#define    I2O_CLAIM_FLAGS_EXCLUSIVE                   0x0001 /* Reserved */
#define    I2O_CLAIM_FLAGS_RESET_SENSITIVE             0x0002
#define    I2O_CLAIM_FLAGS_STATE_SENSITIVE             0x0004
#define    I2O_CLAIM_FLAGS_CAPACITY_SENSITIVE          0x0008
#define    I2O_CLAIM_FLAGS_PEER_SERVICE_DISABLED       0x0010
#define    I2O_CLAIM_FLAGS_MGMT_SERVICE_DISABLED       0x0020

/* Claim Type defines */

#define    I2O_CLAIM_TYPE_PRIMARY_USER                 0x01
#define    I2O_CLAIM_TYPE_AUTHORIZED_USER              0x02
#define    I2O_CLAIM_TYPE_SECONDARY_USER               0x03
#define    I2O_CLAIM_TYPE_MANAGEMENT_USER              0x04

/* UtilClaim Function Message Frame structure. */

typedef struct _I2O_UTIL_CLAIM_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U16                        ClaimFlags;
    U8                         reserved;
    U8                         ClaimType;
} I2O_UTIL_CLAIM_MESSAGE, *PI2O_UTIL_CLAIM_MESSAGE;


/****************************************************************************/

/* Claim Release Flag defines */

#define    I2O_RELEASE_FLAGS_CONDITIONAL               0x0001

/* UtilClaimRelease Function Message Frame structure. */

typedef struct _I2O_UTIL_CLAIM_RELEASE_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U16                        ReleaseFlags;
    U8                         reserved;
    U8                         ClaimType;
} I2O_UTIL_CLAIM_RELEASE_MESSAGE, *PI2O_UTIL_CLAIM_RELEASE_MESSAGE;


/****************************************************************************/

/*  UtilConfigDialog Function Message Frame structure */

typedef struct _I2O_UTIL_CONFIG_DIALOG_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        PageNumber;
    I2O_SG_ELEMENT             SGL;
} I2O_UTIL_CONFIG_DIALOG_MESSAGE, *PI2O_UTIL_CONFIG_DIALOG_MESSAGE;


/****************************************************************************/

/*  Event Acknowledge Function Message Frame structure */

typedef struct _I2O_UTIL_EVENT_ACK_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        EventIndicator;
    U32                        EventData[1];
} I2O_UTIL_EVENT_ACK_MESSAGE, *PI2O_UTIL_EVENT_ACK_MESSAGE;

/* Event Ack Reply structure */

typedef struct _I2O_UTIL_EVENT_ACK_REPLY {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        EventIndicator;
    U32                        EventData[1];
} I2O_UTIL_EVENT_ACK_REPLY, *PI2O_UTIL_EVENT_ACK_REPLY;


/****************************************************************************/

/* Event Indicator Mask Flags */

#define    I2O_EVENT_IND_STATE_CHANGE                  0x80000000
#define    I2O_EVENT_IND_GENERAL_WARNING               0x40000000
#define    I2O_EVENT_IND_CONFIGURATION_FLAG            0x20000000
/* #define    I2O_EVENT_IND_RESERVE_RELEASE               0x10000000 */
#define    I2O_EVENT_IND_LOCK_RELEASE                  0x10000000
#define    I2O_EVENT_IND_CAPABILITY_CHANGE             0x08000000
#define    I2O_EVENT_IND_DEVICE_RESET                  0x04000000
#define    I2O_EVENT_IND_EVENT_MASK_MODIFIED           0x02000000
#define    I2O_EVENT_IND_FIELD_MODIFIED                0x01000000
#define    I2O_EVENT_IND_VENDOR_EVENT                  0x00800000
#define    I2O_EVENT_IND_DEVICE_STATE                  0x00400000

/* Event Data for generic Events */

#define    I2O_EVENT_STATE_CHANGE_NORMAL               0x00
#define    I2O_EVENT_STATE_CHANGE_SUSPENDED            0x01
#define    I2O_EVENT_STATE_CHANGE_RESTART              0x02
#define    I2O_EVENT_STATE_CHANGE_NA_RECOVER           0x03
#define    I2O_EVENT_STATE_CHANGE_NA_NO_RECOVER        0x04
#define    I2O_EVENT_STATE_CHANGE_QUIESCE_REQUEST      0x05
#define    I2O_EVENT_STATE_CHANGE_FAILED               0x10
#define    I2O_EVENT_STATE_CHANGE_FAULTED              0x11

#define    I2O_EVENT_GEN_WARNING_NORMAL                0x00
#define    I2O_EVENT_GEN_WARNING_ERROR_THRESHOLD       0x01
#define    I2O_EVENT_GEN_WARNING_MEDIA_FAULT           0x02

#define    I2O_EVENT_CAPABILITY_OTHER                  0x01
#define    I2O_EVENT_CAPABILITY_CHANGED                0x02

#define    I2O_EVENT_SENSOR_STATE_CHANGED              0x01


/*  UtilEventRegister Function Message Frame structure */

typedef struct _I2O_UTIL_EVENT_REGISTER_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        EventMask;
} I2O_UTIL_EVENT_REGISTER_MESSAGE, *PI2O_UTIL_EVENT_REGISTER_MESSAGE;

/* UtilEventRegister Reply structure */

typedef struct _I2O_UTIL_EVENT_REGISTER_REPLY {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        EventIndicator;
    U32                        EventData[1];
} I2O_UTIL_EVENT_REGISTER_REPLY, *PI2O_UTIL_EVENT_REGISTER_REPLY;


/****************************************************************************/

/* UtilLock Function Message Frame structure. */

typedef struct _I2O_UTIL_LOCK_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
} I2O_UTIL_LOCK_MESSAGE, *PI2O_UTIL_LOCK_MESSAGE;

/****************************************************************************/

/* UtilLockRelease Function Message Frame structure. */

typedef struct _I2O_UTIL_LOCK_RELEASE_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
} I2O_UTIL_LOCK_RELEASE_MESSAGE, *PI2O_UTIL_LOCK_RELEASE_MESSAGE;


/****************************************************************************/

/* UtilNOP Function Message Frame structure. */

typedef struct _I2O_UTIL_NOP_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
} I2O_UTIL_NOP_MESSAGE, *PI2O_UTIL_NOP_MESSAGE;


/****************************************************************************/

/* UtilParamsGet Message Frame structure. */

typedef struct _I2O_UTIL_PARAMS_GET_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        OperationFlags;
    I2O_SG_ELEMENT             SGL;
} I2O_UTIL_PARAMS_GET_MESSAGE, *PI2O_UTIL_PARAMS_GET_MESSAGE;


/****************************************************************************/

/* UtilParamsSet Message Frame structure. */

typedef struct _I2O_UTIL_PARAMS_SET_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U32                        OperationFlags;
    I2O_SG_ELEMENT             SGL;
} I2O_UTIL_PARAMS_SET_MESSAGE, *PI2O_UTIL_PARAMS_SET_MESSAGE;


/****************************************************************************/

/* UtilReplyFaultNotify Message for Message Failure. */

typedef struct _I2O_UTIL_REPLY_FAULT_NOTIFY_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
    U8                         LowestVersion;
    U8                         HighestVersion;
    BF                         Severity:I2O_FAILCODE_SEVERITY_SZ;
    BF                         FailureCode:I2O_FAILCODE_CODE_SZ;
    BF                         FailingIOP_ID:I2O_IOP_ID_SZ;
    BF                         reserved:I2O_RESERVED_4BITS;
    BF                         FailingHostUnitID:I2O_UNIT_ID_SZ;
    U32                        AgeLimit;
#if I2O_64BIT_CONTEXT
    PI2O_MESSAGE_FRAME         OriginalMFA;
#else
    PI2O_MESSAGE_FRAME         OriginalMFALowPart;
    U32                        OriginalMFAHighPart;  /* Always 0000 */
#endif
} I2O_UTIL_REPLY_FAULT_NOTIFY_MESSAGE, *PI2O_UTIL_REPLY_FAULT_NOTIFY_MESSAGE;


/****************************************************************************/

/* Device Reserve Function Message Frame structure. */
/* NOTE:  This was previously called the Reserve Message */

typedef struct _I2O_UTIL_DEVICE_RESERVE_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
} I2O_UTIL_DEVICE_RESERVE_MESSAGE, *PI2O_UTIL_DEVICE_RESERVE_MESSAGE;


/****************************************************************************/

/* Device Release Function Message Frame structure. */
/* NOTE:  This was previously called the ReserveRelease Message */

typedef struct _I2O_UTIL_DEVICE_RELEASE_MESSAGE {
    I2O_MESSAGE_FRAME          StdMessageFrame;
    I2O_TRANSACTION_CONTEXT    TransactionContext;
} I2O_UTIL_DEVICE_RELEASE_MESSAGE, *PI2O_UTIL_DEVICE_RELEASE_MESSAGE;


/****************************************************************************/

PRAGMA_PACK_POP
PRAGMA_ALIGN_POP

#endif    /* I2O_UTILITY_HDR  */
