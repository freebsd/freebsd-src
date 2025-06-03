/*
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016-2025, Broadcom Inc. All rights reserved.
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 3. Neither the name of the Broadcom Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Mail to: Broadcom Inc 1320 Ridder Park Dr, San Jose, CA 95131
 *
 * Broadcom Inc. (Broadcom) MPI3MR Adapter FreeBSD
 *
 *
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version       Description
 *  --------  -----------  ------------------------------------------------------
 *  11-30-18  03.00.00.08  Corresponds to Fusion-MPT MPI 3.0 Specification Rev H.
 *  02-08-19  03.00.00.09  Corresponds to Fusion-MPT MPI 3.0 Specification Rev I.
 *  05-03-19  03.00.00.10  Corresponds to Fusion-MPT MPI 3.0 Specification Rev J.
 *  08-30-19  03.00.00.12  Corresponds to Fusion-MPT MPI 3.0 Specification Rev L.
 *  11-01-19  03.00.00.13  Corresponds to Fusion-MPT MPI 3.0 Specification Rev M.
 *  12-16-19  03.00.00.14  Corresponds to Fusion-MPT MPI 3.0 Specification Rev N.
 *  02-28-20  03.00.00.15  Corresponds to Fusion-MPT MPI 3.0 Specification Rev O.
 *  05-01-20  03.00.00.16  Corresponds to Fusion-MPT MPI 3.0 Specification Rev P.
 *  06-26-20  03.00.00.17  Corresponds to Fusion-MPT MPI 3.0 Specification Rev Q.
 *  08-28-20  03.00.00.18  Corresponds to Fusion-MPT MPI 3.0 Specification Rev R.
 *  10-30-20  03.00.00.19  Corresponds to Fusion-MPT MPI 3.0 Specification Rev S.
 *  12-18-20  03.00.00.20  Corresponds to Fusion-MPT MPI 3.0 Specification Rev T.
 *  02-09-21  03.00.20.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev T - Interim Release 1.
 *  02-26-21  03.00.21.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev U.
 *  04-16-21  03.00.21.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev U - Interim Release 1.
 *  04-28-21  03.00.21.02  Corresponds to Fusion-MPT MPI 3.0 Specification Rev U - Interim Release 2.
 *  05-28-21  03.00.22.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev V.
 *  07-23-21  03.00.22.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev V - Interim Release 1.
 *  09-03-21  03.00.23.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 23.
 *  10-23-21  03.00.23.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 23 - Interim Release 1.
 *  12-03-21  03.00.24.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 24.
 *  02-25-22  03.00.25.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 25.
 *  06-03-22  03.00.26.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 26.
 *  08-09-22  03.00.26.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 26 - Interim Release 1.
 *  09-02-22  03.00.27.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 27.
 *  10-20-22  03.00.27.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 27 - Interim Release 1.
 *  12-02-22  03.00.28.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 28.
 *  02-24-23  03.00.29.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 29.
 *  05-19-23  03.00.30.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 30.
 *  08-18-23  03.00.30.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 30 - Interim Release 1.
 *  11-17-23  03.00.31.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 31
 *  02-16-24  03.00.32.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 32
 *  02-23-24  03.00.32.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 32 - Interim Release 1.
 *  04-19-24  03.00.32.02  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 32 - Interim Release 2.
 *  05-10-24  03.00.33.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 33
 *  06-14-24  03.00.33.01  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 33 - Interim Release 1.
 *  07-26-24  03.00.34.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 34
 *  11-08-24  03.00.35.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 35
 *  02-14-25  03.00.36.00  Corresponds to Fusion-MPT MPI 3.0 Specification Rev 36
 */

#ifndef MPI30_TRANSPORT_H
#define MPI30_TRANSPORT_H     1

/*****************************************************************************
 *              Common version structure/union used in                       *
 *              messages and configuration pages                             *
 ****************************************************************************/

typedef struct _MPI3_VERSION_STRUCT
{
    U8      Dev;                                                        /* 0x00 */
    U8      Unit;                                                       /* 0x01 */
    U8      Minor;                                                      /* 0x02 */
    U8      Major;                                                      /* 0x03 */
} MPI3_VERSION_STRUCT, MPI3_POINTER PTR_MPI3_VERSION_STRUCT,
  Mpi3VersionStruct_t, MPI3_POINTER pMpi3VersionStruct_t;

typedef union _MPI3_VERSION_UNION
{
    MPI3_VERSION_STRUCT     Struct;
    U32                     Word;
} MPI3_VERSION_UNION, MPI3_POINTER PTR_MPI3_VERSION_UNION,
  Mpi3VersionUnion_t, MPI3_POINTER pMpi3VersionUnion_t;

/****** Version constants for this revision ****/
#define MPI3_VERSION_MAJOR                                              (3)
#define MPI3_VERSION_MINOR                                              (0)
#define MPI3_VERSION_UNIT                                               (36)
#define MPI3_VERSION_DEV                                                (0)

/****** DevHandle definitions *****/
#define MPI3_DEVHANDLE_INVALID                                          (0xFFFF)

/*****************************************************************************
 *              System Interface Register Definitions                        *
 ****************************************************************************/
typedef struct _MPI3_SYSIF_OPER_QUEUE_INDEXES
{
    U16         ProducerIndex;                                          /* 0x00 */
    U16         Reserved02;                                             /* 0x02 */
    U16         ConsumerIndex;                                          /* 0x04 */
    U16         Reserved06;                                             /* 0x06 */
} MPI3_SYSIF_OPER_QUEUE_INDEXES, MPI3_POINTER PTR_MPI3_SYSIF_OPER_QUEUE_INDEXES;

typedef volatile struct _MPI3_SYSIF_REGISTERS
{
    U64                             IOCInformation;                     /* 0x00   */
    MPI3_VERSION_UNION              Version;                            /* 0x08   */
    U32                             Reserved0C[2];                      /* 0x0C   */
    U32                             IOCConfiguration;                   /* 0x14   */
    U32                             Reserved18;                         /* 0x18   */
    U32                             IOCStatus;                          /* 0x1C   */
    U32                             Reserved20;                         /* 0x20   */
    U32                             AdminQueueNumEntries;               /* 0x24   */
    U64                             AdminRequestQueueAddress;           /* 0x28   */
    U64                             AdminReplyQueueAddress;             /* 0x30   */
    U32                             Reserved38[2];                      /* 0x38   */
    U32                             CoalesceControl;                    /* 0x40   */
    U32                             Reserved44[1007];                   /* 0x44   */
    U16                             AdminRequestQueuePI;                /* 0x1000 */
    U16                             Reserved1002;                       /* 0x1002 */
    U16                             AdminReplyQueueCI;                  /* 0x1004 */
    U16                             Reserved1006;                       /* 0x1006 */
    MPI3_SYSIF_OPER_QUEUE_INDEXES   OperQueueIndexes[383];              /* 0x1008 */
    U32                             Reserved1C00;                       /* 0x1C00 */
    U32                             WriteSequence;                      /* 0x1C04 */
    U32                             HostDiagnostic;                     /* 0x1C08 */
    U32                             Reserved1C0C;                       /* 0x1C0C */
    U32                             Fault;                              /* 0x1C10 */
    U32                             FaultInfo[3];                       /* 0x1C14 */
    U32                             Reserved1C20[4];                    /* 0x1C20 */
    U64                             HCBAddress;                         /* 0x1C30 */
    U32                             HCBSize;                            /* 0x1C38 */
    U32                             Reserved1C3C;                       /* 0x1C3C */
    U32                             ReplyFreeHostIndex;                 /* 0x1C40 */
    U32                             SenseBufferFreeHostIndex;           /* 0x1C44 */
    U32                             Reserved1C48[2];                    /* 0x1C48 */
    U64                             DiagRWData;                         /* 0x1C50 */
    U64                             DiagRWAddress;                      /* 0x1C58 */
    U16                             DiagRWControl;                      /* 0x1C60 */
    U16                             DiagRWStatus;                       /* 0x1C62 */
    U32                             Reserved1C64[35];                   /* 0x1C64 */
    U32                             Scratchpad[4];                      /* 0x1CF0 */
    U32                             Reserved1D00[192];                  /* 0x1D00 */
    U32                             DeviceAssignedRegisters[2048];      /* 0x2000 */
} MPI3_SYSIF_REGS, MPI3_POINTER PTR_MPI3_SYSIF_REGS,
  Mpi3SysIfRegs_t, MPI3_POINTER pMpi3SysIfRegs_t;

/**** Defines for the IOCInformation register ****/
#define MPI3_SYSIF_IOC_INFO_LOW_OFFSET                                  (0x00000000)
#define MPI3_SYSIF_IOC_INFO_HIGH_OFFSET                                 (0x00000004)
#define MPI3_SYSIF_IOC_INFO_LOW_TIMEOUT_MASK                            (0xFF000000)
#define MPI3_SYSIF_IOC_INFO_LOW_TIMEOUT_SHIFT                           (24)
#define MPI3_SYSIF_IOC_INFO_LOW_HCB_DISABLED                            (0x00000001)

/**** Defines for the IOCConfiguration register ****/
#define MPI3_SYSIF_IOC_CONFIG_OFFSET                                    (0x00000014)
#define MPI3_SYSIF_IOC_CONFIG_OPER_RPY_ENT_SZ                           (0x00F00000)
#define MPI3_SYSIF_IOC_CONFIG_OPER_RPY_ENT_SZ_SHIFT                     (20)
#define MPI3_SYSIF_IOC_CONFIG_OPER_REQ_ENT_SZ                           (0x000F0000)
#define MPI3_SYSIF_IOC_CONFIG_OPER_REQ_ENT_SZ_SHIFT                     (16)
#define MPI3_SYSIF_IOC_CONFIG_SHUTDOWN_MASK                             (0x0000C000)
#define MPI3_SYSIF_IOC_CONFIG_SHUTDOWN_SHIFT                            (14)
#define MPI3_SYSIF_IOC_CONFIG_SHUTDOWN_NO                               (0x00000000)
#define MPI3_SYSIF_IOC_CONFIG_SHUTDOWN_NORMAL                           (0x00004000)
#define MPI3_SYSIF_IOC_CONFIG_DEVICE_SHUTDOWN_SEND_REQ                  (0x00002000)
#define MPI3_SYSIF_IOC_CONFIG_DIAG_SAVE                                 (0x00000010)
#define MPI3_SYSIF_IOC_CONFIG_ENABLE_IOC                                (0x00000001)

/**** Defines for the IOCStatus register ****/
#define MPI3_SYSIF_IOC_STATUS_OFFSET                                    (0x0000001C)
#define MPI3_SYSIF_IOC_STATUS_RESET_HISTORY                             (0x00000010)
#define MPI3_SYSIF_IOC_STATUS_SHUTDOWN_MASK                             (0x0000000C)
#define MPI3_SYSIF_IOC_STATUS_SHUTDOWN_SHIFT                            (0x00000002)
#define MPI3_SYSIF_IOC_STATUS_SHUTDOWN_NONE                             (0x00000000)
#define MPI3_SYSIF_IOC_STATUS_SHUTDOWN_IN_PROGRESS                      (0x00000004)
#define MPI3_SYSIF_IOC_STATUS_SHUTDOWN_COMPLETE                         (0x00000008)
#define MPI3_SYSIF_IOC_STATUS_FAULT                                     (0x00000002)
#define MPI3_SYSIF_IOC_STATUS_READY                                     (0x00000001)

/**** Defines for the AdminQueueNumEntries register ****/
#define MPI3_SYSIF_ADMIN_Q_NUM_ENTRIES_OFFSET                           (0x00000024)
#define MPI3_SYSIF_ADMIN_Q_NUM_ENTRIES_REQ_MASK                         (0x0FFF)
#define MPI3_SYSIF_ADMIN_Q_NUM_ENTRIES_REQ_SHIFT                        (0)
#define MPI3_SYSIF_ADMIN_Q_NUM_ENTRIES_REPLY_OFFSET                     (0x00000026)
#define MPI3_SYSIF_ADMIN_Q_NUM_ENTRIES_REPLY_MASK                       (0x0FFF0000)
#define MPI3_SYSIF_ADMIN_Q_NUM_ENTRIES_REPLY_SHIFT                      (16)

/**** Defines for the AdminRequestQueueAddress register ****/
#define MPI3_SYSIF_ADMIN_REQ_Q_ADDR_LOW_OFFSET                          (0x00000028)
#define MPI3_SYSIF_ADMIN_REQ_Q_ADDR_HIGH_OFFSET                         (0x0000002C)

/**** Defines for the AdminReplyQueueAddress register ****/
#define MPI3_SYSIF_ADMIN_REPLY_Q_ADDR_LOW_OFFSET                        (0x00000030)
#define MPI3_SYSIF_ADMIN_REPLY_Q_ADDR_HIGH_OFFSET                       (0x00000034)

/**** Defines for the CoalesceControl register ****/
#define MPI3_SYSIF_COALESCE_CONTROL_OFFSET                              (0x00000040)
#define MPI3_SYSIF_COALESCE_CONTROL_ENABLE_MASK                         (0xC0000000)
#define MPI3_SYSIF_COALESCE_CONTROL_ENABLE_SHIFT                        (30)
#define MPI3_SYSIF_COALESCE_CONTROL_ENABLE_NO_CHANGE                    (0x00000000)
#define MPI3_SYSIF_COALESCE_CONTROL_ENABLE_DISABLE                      (0x40000000)
#define MPI3_SYSIF_COALESCE_CONTROL_ENABLE_ENABLE                       (0xC0000000)
#define MPI3_SYSIF_COALESCE_CONTROL_VALID                               (0x20000000)
#define MPI3_SYSIF_COALESCE_CONTROL_MSIX_IDX_MASK                       (0x01FF0000)
#define MPI3_SYSIF_COALESCE_CONTROL_MSIX_IDX_SHIFT                      (16)
#define MPI3_SYSIF_COALESCE_CONTROL_TIMEOUT_MASK                        (0x0000FF00)
#define MPI3_SYSIF_COALESCE_CONTROL_TIMEOUT_SHIFT                       (8)
#define MPI3_SYSIF_COALESCE_CONTROL_DEPTH_MASK                          (0x000000FF)
#define MPI3_SYSIF_COALESCE_CONTROL_DEPTH_SHIFT                         (0)

/**** Defines for the AdminRequestQueuePI register ****/
#define MPI3_SYSIF_ADMIN_REQ_Q_PI_OFFSET                                (0x00001000)

/**** Defines for the AdminReplyQueueCI register ****/
#define MPI3_SYSIF_ADMIN_REPLY_Q_CI_OFFSET                              (0x00001004)

/**** Defines for the OperationalRequestQueuePI register */
#define MPI3_SYSIF_OPER_REQ_Q_PI_OFFSET                                 (0x00001008)
#define MPI3_SYSIF_OPER_REQ_Q_N_PI_OFFSET(N)                            (MPI3_SYSIF_OPER_REQ_Q_PI_OFFSET + (((N)-1)*8)) /* N = 1, 2, 3, ..., 255 */

/**** Defines for the OperationalReplyQueueCI register */
#define MPI3_SYSIF_OPER_REPLY_Q_CI_OFFSET                               (0x0000100C)
#define MPI3_SYSIF_OPER_REPLY_Q_N_CI_OFFSET(N)                          (MPI3_SYSIF_OPER_REPLY_Q_CI_OFFSET + (((N)-1)*8)) /* N = 1, 2, 3, ..., 255 */

/**** Defines for the WriteSequence register *****/
#define MPI3_SYSIF_WRITE_SEQUENCE_OFFSET                                (0x00001C04)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_MASK                        (0x0000000F)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_SHIFT                       (0)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_FLUSH                       (0x0)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_1ST                         (0xF)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_2ND                         (0x4)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_3RD                         (0xB)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_4TH                         (0x2)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_5TH                         (0x7)
#define MPI3_SYSIF_WRITE_SEQUENCE_KEY_VALUE_6TH                         (0xD)

/**** Defines for the HostDiagnostic register *****/
#define MPI3_SYSIF_HOST_DIAG_OFFSET                                     (0x00001C08)
#define MPI3_SYSIF_HOST_DIAG_RESET_ACTION_MASK                          (0x00000700)
#define MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SHIFT                         (8)
#define MPI3_SYSIF_HOST_DIAG_RESET_ACTION_NO_RESET                      (0x00000000)
#define MPI3_SYSIF_HOST_DIAG_RESET_ACTION_SOFT_RESET                    (0x00000100)
#define MPI3_SYSIF_HOST_DIAG_RESET_ACTION_HOST_CONTROL_BOOT_RESET       (0x00000200)
#define MPI3_SYSIF_HOST_DIAG_RESET_ACTION_COMPLETE_RESET                (0x00000300)
#define MPI3_SYSIF_HOST_DIAG_RESET_ACTION_DIAG_FAULT                    (0x00000700)
#define MPI3_SYSIF_HOST_DIAG_SAVE_IN_PROGRESS                           (0x00000080)
#define MPI3_SYSIF_HOST_DIAG_SECURE_BOOT                                (0x00000040)
#define MPI3_SYSIF_HOST_DIAG_CLEAR_INVALID_FW_IMAGE                     (0x00000020)
#define MPI3_SYSIF_HOST_DIAG_INVALID_FW_IMAGE                           (0x00000010)
#define MPI3_SYSIF_HOST_DIAG_HCBENABLE                                  (0x00000008)
#define MPI3_SYSIF_HOST_DIAG_HCBMODE                                    (0x00000004)
#define MPI3_SYSIF_HOST_DIAG_DIAG_RW_ENABLE                             (0x00000002)
#define MPI3_SYSIF_HOST_DIAG_DIAG_WRITE_ENABLE                          (0x00000001)

/**** Defines for the Fault register ****/
#define MPI3_SYSIF_FAULT_OFFSET                                         (0x00001C10)
#define MPI3_SYSIF_FAULT_CODE_MASK                                      (0x0000FFFF)
#define MPI3_SYSIF_FAULT_CODE_SHIFT                                     (0)
#define MPI3_SYSIF_FAULT_CODE_DIAG_FAULT_RESET                          (0x0000F000)
#define MPI3_SYSIF_FAULT_CODE_CI_ACTIVATION_RESET                       (0x0000F001)
#define MPI3_SYSIF_FAULT_CODE_SOFT_RESET_IN_PROGRESS                    (0x0000F002)
#define MPI3_SYSIF_FAULT_CODE_COMPLETE_RESET_NEEDED                     (0x0000F003)
#define MPI3_SYSIF_FAULT_CODE_SOFT_RESET_NEEDED                         (0x0000F004)
#define MPI3_SYSIF_FAULT_CODE_POWER_CYCLE_REQUIRED                      (0x0000F005)
#define MPI3_SYSIF_FAULT_CODE_TEMP_THRESHOLD_EXCEEDED                   (0x0000F006)
#define MPI3_SYSIF_FAULT_CODE_INSUFFICIENT_PCI_SLOT_POWER               (0x0000F007)

/**** Defines for FaultCodeAdditionalInfo registers ****/
#define MPI3_SYSIF_FAULT_INFO0_OFFSET                                   (0x00001C14)
#define MPI3_SYSIF_FAULT_INFO1_OFFSET                                   (0x00001C18)
#define MPI3_SYSIF_FAULT_INFO2_OFFSET                                   (0x00001C1C)

/**** Defines for HCBAddress register ****/
#define MPI3_SYSIF_HCB_ADDRESS_LOW_OFFSET                               (0x00001C30)
#define MPI3_SYSIF_HCB_ADDRESS_HIGH_OFFSET                              (0x00001C34)

/**** Defines for HCBSize register ****/
#define MPI3_SYSIF_HCB_SIZE_OFFSET                                      (0x00001C38)
#define MPI3_SYSIF_HCB_SIZE_SIZE_MASK                                   (0xFFFFF000)
#define MPI3_SYSIF_HCB_SIZE_SIZE_SHIFT                                  (12)
#define MPI3_SYSIF_HCB_SIZE_HCDW_ENABLE                                 (0x00000001)

/**** Defines for ReplyFreeHostIndex register ****/
#define MPI3_SYSIF_REPLY_FREE_HOST_INDEX_OFFSET                         (0x00001C40)

/**** Defines for SenseBufferFreeHostIndex register ****/
#define MPI3_SYSIF_SENSE_BUF_FREE_HOST_INDEX_OFFSET                     (0x00001C44)

/**** Defines for DiagRWData register ****/
#define MPI3_SYSIF_DIAG_RW_DATA_LOW_OFFSET                              (0x00001C50)
#define MPI3_SYSIF_DIAG_RW_DATA_HIGH_OFFSET                             (0x00001C54)

/**** Defines for DiagRWAddress ****/
#define MPI3_SYSIF_DIAG_RW_ADDRESS_LOW_OFFSET                           (0x00001C58)
#define MPI3_SYSIF_DIAG_RW_ADDRESS_HIGH_OFFSET                          (0x00001C5C)

/**** Defines for DiagRWControl register ****/
#define MPI3_SYSIF_DIAG_RW_CONTROL_OFFSET                               (0x00001C60)
#define MPI3_SYSIF_DIAG_RW_CONTROL_LEN_MASK                             (0x00000030)
#define MPI3_SYSIF_DIAG_RW_CONTROL_LEN_SHIFT                            (4)
#define MPI3_SYSIF_DIAG_RW_CONTROL_LEN_1BYTE                            (0x00000000)
#define MPI3_SYSIF_DIAG_RW_CONTROL_LEN_2BYTES                           (0x00000010)
#define MPI3_SYSIF_DIAG_RW_CONTROL_LEN_4BYTES                           (0x00000020)
#define MPI3_SYSIF_DIAG_RW_CONTROL_LEN_8BYTES                           (0x00000030)
#define MPI3_SYSIF_DIAG_RW_CONTROL_RESET                                (0x00000004)
#define MPI3_SYSIF_DIAG_RW_CONTROL_DIR_MASK                             (0x00000002)
#define MPI3_SYSIF_DIAG_RW_CONTROL_DIR_SHIFT                            (1)
#define MPI3_SYSIF_DIAG_RW_CONTROL_DIR_READ                             (0x00000000)
#define MPI3_SYSIF_DIAG_RW_CONTROL_DIR_WRITE                            (0x00000002)
#define MPI3_SYSIF_DIAG_RW_CONTROL_START                                (0x00000001)

/**** Defines for DiagRWStatus register ****/
#define MPI3_SYSIF_DIAG_RW_STATUS_OFFSET                                (0x00001C62)
#define MPI3_SYSIF_DIAG_RW_STATUS_STATUS_MASK                           (0x0000000E)
#define MPI3_SYSIF_DIAG_RW_STATUS_STATUS_SHIFT                          (1)
#define MPI3_SYSIF_DIAG_RW_STATUS_STATUS_SUCCESS                        (0x00000000)
#define MPI3_SYSIF_DIAG_RW_STATUS_STATUS_INV_ADDR                       (0x00000002)
#define MPI3_SYSIF_DIAG_RW_STATUS_STATUS_ACC_ERR                        (0x00000004)
#define MPI3_SYSIF_DIAG_RW_STATUS_STATUS_PAR_ERR                        (0x00000006)
#define MPI3_SYSIF_DIAG_RW_STATUS_BUSY                                  (0x00000001)

/**** Defines for Scratchpad registers ****/
#define MPI3_SYSIF_SCRATCHPAD0_OFFSET                                   (0x00001CF0)
#define MPI3_SYSIF_SCRATCHPAD1_OFFSET                                   (0x00001CF4)
#define MPI3_SYSIF_SCRATCHPAD2_OFFSET                                   (0x00001CF8)
#define MPI3_SYSIF_SCRATCHPAD3_OFFSET                                   (0x00001CFC)

/**** Defines for Device Assigned registers ****/
#define MPI3_SYSIF_DEVICE_ASSIGNED_REGS_OFFSET                          (0x00002000)

/**** Default Defines for Diag Save Timeout ****/
#define MPI3_SYSIF_DIAG_SAVE_TIMEOUT                                    (60)    /* seconds */

/*****************************************************************************
 *              Reply Descriptors                                            *
 ****************************************************************************/

/*****************************************************************************
 *              Default Reply Descriptor                                     *
 ****************************************************************************/
typedef struct _MPI3_DEFAULT_REPLY_DESCRIPTOR
{
    U32             DescriptorTypeDependent1[2];    /* 0x00 */
    U16             RequestQueueCI;                 /* 0x08 */
    U16             RequestQueueID;                 /* 0x0A */
    U16             DescriptorTypeDependent2;       /* 0x0C */
    U16             ReplyFlags;                     /* 0x0E */
} MPI3_DEFAULT_REPLY_DESCRIPTOR, MPI3_POINTER PTR_MPI3_DEFAULT_REPLY_DESCRIPTOR,
  Mpi3DefaultReplyDescriptor_t, MPI3_POINTER pMpi3DefaultReplyDescriptor_t;

/**** Defines for the ReplyFlags field ****/
#define MPI3_REPLY_DESCRIPT_FLAGS_PHASE_MASK                       (0x0001)
#define MPI3_REPLY_DESCRIPT_FLAGS_PHASE_SHIFT                      (0)
#define MPI3_REPLY_DESCRIPT_FLAGS_TYPE_MASK                        (0xF000)
#define MPI3_REPLY_DESCRIPT_FLAGS_TYPE_SHIFT                       (12)
#define MPI3_REPLY_DESCRIPT_FLAGS_TYPE_ADDRESS_REPLY               (0x0000)
#define MPI3_REPLY_DESCRIPT_FLAGS_TYPE_SUCCESS                     (0x1000)
#define MPI3_REPLY_DESCRIPT_FLAGS_TYPE_TARGET_COMMAND_BUFFER       (0x2000)
#define MPI3_REPLY_DESCRIPT_FLAGS_TYPE_STATUS                      (0x3000)

/**** Defines for the RequestQueueID field ****/
#define MPI3_REPLY_DESCRIPT_REQUEST_QUEUE_ID_INVALID               (0xFFFF)

/*****************************************************************************
 *              Address Reply Descriptor                                     *
 ****************************************************************************/
typedef struct _MPI3_ADDRESS_REPLY_DESCRIPTOR
{
    U64             ReplyFrameAddress;              /* 0x00 */
    U16             RequestQueueCI;                 /* 0x08 */
    U16             RequestQueueID;                 /* 0x0A */
    U16             Reserved0C;                     /* 0x0C */
    U16             ReplyFlags;                     /* 0x0E */
} MPI3_ADDRESS_REPLY_DESCRIPTOR, MPI3_POINTER PTR_MPI3_ADDRESS_REPLY_DESCRIPTOR,
  Mpi3AddressReplyDescriptor_t, MPI3_POINTER pMpi3AddressReplyDescriptor_t;

/*****************************************************************************
 *              Success Reply Descriptor                                     *
 ****************************************************************************/
typedef struct _MPI3_SUCCESS_REPLY_DESCRIPTOR
{
    U32             Reserved00[2];                  /* 0x00 */
    U16             RequestQueueCI;                 /* 0x08 */
    U16             RequestQueueID;                 /* 0x0A */
    U16             HostTag;                        /* 0x0C */
    U16             ReplyFlags;                     /* 0x0E */
} MPI3_SUCCESS_REPLY_DESCRIPTOR, MPI3_POINTER PTR_MPI3_SUCCESS_REPLY_DESCRIPTOR,
  Mpi3SuccessReplyDescriptor_t, MPI3_POINTER pMpi3SuccessReplyDescriptor_t;

/*****************************************************************************
 *              Target Command Buffer Reply Descriptor                       *
 ****************************************************************************/
typedef struct _MPI3_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR
{
    U32             Reserved00;                     /* 0x00 */
    U16             InitiatorDevHandle;             /* 0x04 */
    U8              PhyNum;                         /* 0x06 */
    U8              Reserved07;                     /* 0x07 */
    U16             RequestQueueCI;                 /* 0x08 */
    U16             RequestQueueID;                 /* 0x0A */
    U16             IOIndex;                        /* 0x0C */
    U16             ReplyFlags;                     /* 0x0E */
} MPI3_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR, MPI3_POINTER PTR_MPI3_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR,
  Mpi3TargetCommandBufferReplyDescriptor_t, MPI3_POINTER pMpi3TargetCommandBufferReplyDescriptor_t;

/**** See Default Reply Descriptor Defines above for definitions in the ReplyFlags field ****/

/*****************************************************************************
 *              Status Reply Descriptor                                      *
 ****************************************************************************/
typedef struct _MPI3_STATUS_REPLY_DESCRIPTOR
{
    U16             IOCStatus;                      /* 0x00 */
    U16             Reserved02;                     /* 0x02 */
    U32             IOCLogInfo;                     /* 0x04 */
    U16             RequestQueueCI;                 /* 0x08 */
    U16             RequestQueueID;                 /* 0x0A */
    U16             HostTag;                        /* 0x0C */
    U16             ReplyFlags;                     /* 0x0E */
} MPI3_STATUS_REPLY_DESCRIPTOR, MPI3_POINTER PTR_MPI3_STATUS_REPLY_DESCRIPTOR,
  Mpi3StatusReplyDescriptor_t, MPI3_POINTER pMpi3StatusReplyDescriptor_t;

/**** Use MPI3_IOCSTATUS_ defines for the IOCStatus field ****/

/**** Use MPI3_IOCLOGINFO_ defines for the IOCLogInfo field ****/

/*****************************************************************************
 *              Union of Reply Descriptors                                   *
 ****************************************************************************/
typedef union _MPI3_REPLY_DESCRIPTORS_UNION
{
    MPI3_DEFAULT_REPLY_DESCRIPTOR               Default;
    MPI3_ADDRESS_REPLY_DESCRIPTOR               AddressReply;
    MPI3_SUCCESS_REPLY_DESCRIPTOR               Success;
    MPI3_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR TargetCommandBuffer;
    MPI3_STATUS_REPLY_DESCRIPTOR                Status;
    U32                                         Words[4];
} MPI3_REPLY_DESCRIPTORS_UNION, MPI3_POINTER PTR_MPI3_REPLY_DESCRIPTORS_UNION,
  Mpi3ReplyDescriptorsUnion_t, MPI3_POINTER pMpi3ReplyDescriptorsUnion_t;


/*****************************************************************************
 *              Scatter Gather Elements                                      *
 ****************************************************************************/

/*****************************************************************************
 *              Common structure for Simple, Chain, and Last Chain           *
 *              scatter gather elements                                      *
 ****************************************************************************/
typedef struct _MPI3_SGE_COMMON
{
    U64             Address;                           /* 0x00 */
    U32             Length;                            /* 0x08 */
    U8              Reserved0C[3];                     /* 0x0C */
    U8              Flags;                             /* 0x0F */
} MPI3_SGE_SIMPLE, MPI3_POINTER PTR_MPI3_SGE_SIMPLE,
  Mpi3SGESimple_t, MPI3_POINTER pMpi3SGESimple_t,
  MPI3_SGE_CHAIN, MPI3_POINTER PTR_MPI3_SGE_CHAIN,
  Mpi3SGEChain_t, MPI3_POINTER pMpi3SGEChain_t,
  MPI3_SGE_LAST_CHAIN, MPI3_POINTER PTR_MPI3_SGE_LAST_CHAIN,
  Mpi3SGELastChain_t, MPI3_POINTER pMpi3SGELastChain_t;

/*****************************************************************************
 *              Bit Bucket scatter gather element                            *
 ****************************************************************************/
typedef struct _MPI3_SGE_BIT_BUCKET
{
    U64             Reserved00;                        /* 0x00 */
    U32             Length;                            /* 0x08 */
    U8              Reserved0C[3];                     /* 0x0C */
    U8              Flags;                             /* 0x0F */
} MPI3_SGE_BIT_BUCKET, MPI3_POINTER PTR_MPI3_SGE_BIT_BUCKET,
  Mpi3SGEBitBucket_t, MPI3_POINTER pMpi3SGEBitBucket_t;

/*****************************************************************************
 *              Extended EEDP scatter gather element                         *
 ****************************************************************************/
typedef struct _MPI3_SGE_EXTENDED_EEDP
{
    U8              UserDataSize;                      /* 0x00 */
    U8              Reserved01;                        /* 0x01 */
    U16             EEDPFlags;                         /* 0x02 */
    U32             SecondaryReferenceTag;             /* 0x04 */
    U16             SecondaryApplicationTag;           /* 0x08 */
    U16             ApplicationTagTranslationMask;     /* 0x0A */
    U16             Reserved0C;                        /* 0x0C */
    U8              ExtendedOperation;                 /* 0x0E */
    U8              Flags;                             /* 0x0F */
} MPI3_SGE_EXTENDED_EEDP, MPI3_POINTER PTR_MPI3_SGE_EXTENDED_EEDP,
  Mpi3SGEExtendedEEDP_t, MPI3_POINTER pMpi3SGEExtendedEEDP_t;

/*****************************************************************************
 *              Union of scatter gather elements                             *
 ****************************************************************************/
typedef union _MPI3_SGE_UNION
{
    MPI3_SGE_SIMPLE                 Simple;
    MPI3_SGE_CHAIN                  Chain;
    MPI3_SGE_LAST_CHAIN             LastChain;
    MPI3_SGE_BIT_BUCKET             BitBucket;
    MPI3_SGE_EXTENDED_EEDP          Eedp;
    U32                             Words[4];
} MPI3_SGE_UNION, MPI3_POINTER PTR_MPI3_SGE_UNION,
  Mpi3SGEUnion_t, MPI3_POINTER pMpi3SGEUnion_t;

/**** Definitions for the Flags field ****/
#define MPI3_SGE_FLAGS_ELEMENT_TYPE_MASK        (0xF0)
#define MPI3_SGE_FLAGS_ELEMENT_TYPE_SHIFT       (4)
#define MPI3_SGE_FLAGS_ELEMENT_TYPE_SIMPLE      (0x00)
#define MPI3_SGE_FLAGS_ELEMENT_TYPE_BIT_BUCKET  (0x10)
#define MPI3_SGE_FLAGS_ELEMENT_TYPE_CHAIN       (0x20)
#define MPI3_SGE_FLAGS_ELEMENT_TYPE_LAST_CHAIN  (0x30)
#define MPI3_SGE_FLAGS_ELEMENT_TYPE_EXTENDED    (0xF0)
#define MPI3_SGE_FLAGS_END_OF_LIST              (0x08)
#define MPI3_SGE_FLAGS_END_OF_BUFFER            (0x04)
#define MPI3_SGE_FLAGS_DLAS_MASK                (0x03)
#define MPI3_SGE_FLAGS_DLAS_SHIFT               (0)
#define MPI3_SGE_FLAGS_DLAS_SYSTEM              (0x00)
#define MPI3_SGE_FLAGS_DLAS_IOC_UDP             (0x01)
#define MPI3_SGE_FLAGS_DLAS_IOC_CTL             (0x02)

/**** Definitions for the ExtendedOperation field of Extended element ****/
#define MPI3_SGE_EXT_OPER_EEDP                  (0x00)

/**** Definitions for the EEDPFlags field of Extended EEDP element ****/
#define MPI3_EEDPFLAGS_INCR_PRI_REF_TAG                 (0x8000)
#define MPI3_EEDPFLAGS_INCR_SEC_REF_TAG                 (0x4000)
#define MPI3_EEDPFLAGS_INCR_PRI_APP_TAG                 (0x2000)
#define MPI3_EEDPFLAGS_INCR_SEC_APP_TAG                 (0x1000)
#define MPI3_EEDPFLAGS_ESC_PASSTHROUGH                  (0x0800)
#define MPI3_EEDPFLAGS_CHK_REF_TAG                      (0x0400)
#define MPI3_EEDPFLAGS_CHK_APP_TAG                      (0x0200)
#define MPI3_EEDPFLAGS_CHK_GUARD                        (0x0100)
#define MPI3_EEDPFLAGS_ESC_MODE_MASK                    (0x00C0)
#define MPI3_EEDPFLAGS_ESC_MODE_SHIFT                   (6)
#define MPI3_EEDPFLAGS_ESC_MODE_DO_NOT_DISABLE          (0x0040)
#define MPI3_EEDPFLAGS_ESC_MODE_APPTAG_DISABLE          (0x0080)
#define MPI3_EEDPFLAGS_ESC_MODE_APPTAG_REFTAG_DISABLE   (0x00C0)
#define MPI3_EEDPFLAGS_HOST_GUARD_MASK                  (0x0030)
#define MPI3_EEDPFLAGS_HOST_GUARD_SHIFT                 (4)
#define MPI3_EEDPFLAGS_HOST_GUARD_T10_CRC               (0x0000)
#define MPI3_EEDPFLAGS_HOST_GUARD_IP_CHKSUM             (0x0010)
#define MPI3_EEDPFLAGS_HOST_GUARD_OEM_SPECIFIC          (0x0020)
#define MPI3_EEDPFLAGS_PT_REF_TAG                       (0x0008)
#define MPI3_EEDPFLAGS_EEDP_OP_MASK                     (0x0007)
#define MPI3_EEDPFLAGS_EEDP_OP_SHIFT                    (0)
#define MPI3_EEDPFLAGS_EEDP_OP_CHECK                    (0x0001)
#define MPI3_EEDPFLAGS_EEDP_OP_STRIP                    (0x0002)
#define MPI3_EEDPFLAGS_EEDP_OP_CHECK_REMOVE             (0x0003)
#define MPI3_EEDPFLAGS_EEDP_OP_INSERT                   (0x0004)
#define MPI3_EEDPFLAGS_EEDP_OP_REPLACE                  (0x0006)
#define MPI3_EEDPFLAGS_EEDP_OP_CHECK_REGEN              (0x0007)

/**** Definitions for the UserDataSize field of Extended EEDP element ****/
#define MPI3_EEDP_UDS_512                           (0x01)
#define MPI3_EEDP_UDS_520                           (0x02)
#define MPI3_EEDP_UDS_4080                          (0x03)
#define MPI3_EEDP_UDS_4088                          (0x04)
#define MPI3_EEDP_UDS_4096                          (0x05)
#define MPI3_EEDP_UDS_4104                          (0x06)
#define MPI3_EEDP_UDS_4160                          (0x07)

/*****************************************************************************
 *              Standard Message Structures                                  *
 ****************************************************************************/

/*****************************************************************************
 *              Request Message Header for all request messages              *
 ****************************************************************************/
typedef struct _MPI3_REQUEST_HEADER
{
    U16             HostTag;                    /* 0x00 */
    U8              IOCUseOnly02;               /* 0x02 */
    U8              Function;                   /* 0x03 */
    U16             IOCUseOnly04;               /* 0x04 */
    U8              IOCUseOnly06;               /* 0x06 */
    U8              MsgFlags;                   /* 0x07 */
    U16             ChangeCount;                /* 0x08 */
    U16             FunctionDependent;          /* 0x0A */
} MPI3_REQUEST_HEADER, MPI3_POINTER PTR_MPI3_REQUEST_HEADER,
  Mpi3RequestHeader_t, MPI3_POINTER pMpi3RequestHeader_t;

/*****************************************************************************
 *              Default Reply                                                *
 ****************************************************************************/
typedef struct _MPI3_DEFAULT_REPLY
{
    U16             HostTag;                    /* 0x00 */
    U8              IOCUseOnly02;               /* 0x02 */
    U8              Function;                   /* 0x03 */
    U16             IOCUseOnly04;               /* 0x04 */
    U8              IOCUseOnly06;               /* 0x06 */
    U8              MsgFlags;                   /* 0x07 */
    U16             IOCUseOnly08;               /* 0x08 */
    U16             IOCStatus;                  /* 0x0A */
    U32             IOCLogInfo;                 /* 0x0C */
} MPI3_DEFAULT_REPLY, MPI3_POINTER PTR_MPI3_DEFAULT_REPLY,
  Mpi3DefaultReply_t, MPI3_POINTER pMpi3DefaultReply_t;

/**** Defines for the HostTag field ****/
#define MPI3_HOST_TAG_INVALID                       (0xFFFF)

/**** Defines for message Function ****/
/* I/O Controller functions */
#define MPI3_FUNCTION_IOC_FACTS                     (0x01) /* IOC Facts */
#define MPI3_FUNCTION_IOC_INIT                      (0x02) /* IOC Init */
#define MPI3_FUNCTION_PORT_ENABLE                   (0x03) /* Port Enable */
#define MPI3_FUNCTION_EVENT_NOTIFICATION            (0x04) /* Event Notification */
#define MPI3_FUNCTION_EVENT_ACK                     (0x05) /* Event Acknowledge */
#define MPI3_FUNCTION_CI_DOWNLOAD                   (0x06) /* Component Image Download */
#define MPI3_FUNCTION_CI_UPLOAD                     (0x07) /* Component Image Upload */
#define MPI3_FUNCTION_IO_UNIT_CONTROL               (0x08) /* IO Unit Control */
#define MPI3_FUNCTION_PERSISTENT_EVENT_LOG          (0x09) /* Persistent Event Log */
#define MPI3_FUNCTION_MGMT_PASSTHROUGH              (0x0A) /* Management Passthrough */
#define MPI3_FUNCTION_CONFIG                        (0x10) /* Configuration */

/* SCSI Initiator I/O functions */
#define MPI3_FUNCTION_SCSI_IO                       (0x20) /* SCSI IO */
#define MPI3_FUNCTION_SCSI_TASK_MGMT                (0x21) /* SCSI Task Management */
#define MPI3_FUNCTION_SMP_PASSTHROUGH               (0x22) /* SMP Passthrough */
#define MPI3_FUNCTION_NVME_ENCAPSULATED             (0x24) /* NVMe Encapsulated */

/* SCSI Target I/O functions */
#define MPI3_FUNCTION_TARGET_ASSIST                 (0x30) /* Target Assist */
#define MPI3_FUNCTION_TARGET_STATUS_SEND            (0x31) /* Target Status Send */
#define MPI3_FUNCTION_TARGET_MODE_ABORT             (0x32) /* Target Mode Abort */
#define MPI3_FUNCTION_TARGET_CMD_BUF_POST_BASE      (0x33) /* Target Command Buffer Post Base */
#define MPI3_FUNCTION_TARGET_CMD_BUF_POST_LIST      (0x34) /* Target Command Buffer Post List */

/* Queue Management functions */
#define MPI3_FUNCTION_CREATE_REQUEST_QUEUE          (0x70)  /* Create an operational request queue */
#define MPI3_FUNCTION_DELETE_REQUEST_QUEUE          (0x71)  /* Delete an operational request queue */
#define MPI3_FUNCTION_CREATE_REPLY_QUEUE            (0x72)  /* Create an operational reply queue */
#define MPI3_FUNCTION_DELETE_REPLY_QUEUE            (0x73)  /* Delete an operational reply queue */

/* Diagnostic Tools */
#define MPI3_FUNCTION_TOOLBOX                       (0x80) /* Toolbox */
#define MPI3_FUNCTION_DIAG_BUFFER_POST              (0x81) /* Post a Diagnostic Buffer to the I/O Unit */
#define MPI3_FUNCTION_DIAG_BUFFER_MANAGE            (0x82) /* Manage a Diagnostic Buffer */
#define MPI3_FUNCTION_DIAG_BUFFER_UPLOAD            (0x83) /* Upload a Diagnostic Buffer */

/* Miscellaneous functions */
#define MPI3_FUNCTION_MIN_IOC_USE_ONLY              (0xC0)  /* Beginning of IOC Use Only range of function codes */
#define MPI3_FUNCTION_MAX_IOC_USE_ONLY              (0xEF)  /* End of IOC Use Only range of function codes */
#define MPI3_FUNCTION_MIN_PRODUCT_SPECIFIC          (0xF0)  /* Beginning of the product-specific range of function codes */
#define MPI3_FUNCTION_MAX_PRODUCT_SPECIFIC          (0xFF)  /* End of the product-specific range of function codes */

/**** Defines for IOCStatus ****/
#define MPI3_IOCSTATUS_LOG_INFO_AVAILABLE           (0x8000)
#define MPI3_IOCSTATUS_STATUS_MASK                  (0x7FFF)
#define MPI3_IOCSTATUS_STATUS_SHIFT                 (0)

/* Common IOCStatus values for all replies */
#define MPI3_IOCSTATUS_SUCCESS                      (0x0000)
#define MPI3_IOCSTATUS_INVALID_FUNCTION             (0x0001)
#define MPI3_IOCSTATUS_BUSY                         (0x0002)
#define MPI3_IOCSTATUS_INVALID_SGL                  (0x0003)
#define MPI3_IOCSTATUS_INTERNAL_ERROR               (0x0004)
#define MPI3_IOCSTATUS_INSUFFICIENT_RESOURCES       (0x0006)
#define MPI3_IOCSTATUS_INVALID_FIELD                (0x0007)
#define MPI3_IOCSTATUS_INVALID_STATE                (0x0008)
#define MPI3_IOCSTATUS_SHUTDOWN_ACTIVE              (0x0009)
#define MPI3_IOCSTATUS_INSUFFICIENT_POWER           (0x000A)
#define MPI3_IOCSTATUS_INVALID_CHANGE_COUNT         (0x000B)
#define MPI3_IOCSTATUS_ALLOWED_CMD_BLOCK            (0x000C)
#define MPI3_IOCSTATUS_SUPERVISOR_ONLY              (0x000D)
#define MPI3_IOCSTATUS_FAILURE                      (0x001F)

/* Config IOCStatus values */
#define MPI3_IOCSTATUS_CONFIG_INVALID_ACTION        (0x0020)
#define MPI3_IOCSTATUS_CONFIG_INVALID_TYPE          (0x0021)
#define MPI3_IOCSTATUS_CONFIG_INVALID_PAGE          (0x0022)
#define MPI3_IOCSTATUS_CONFIG_INVALID_DATA          (0x0023)
#define MPI3_IOCSTATUS_CONFIG_NO_DEFAULTS           (0x0024)
#define MPI3_IOCSTATUS_CONFIG_CANT_COMMIT           (0x0025)

/* SCSI IO IOCStatus values */
#define MPI3_IOCSTATUS_SCSI_RECOVERED_ERROR         (0x0040)
#define MPI3_IOCSTATUS_SCSI_TM_NOT_SUPPORTED        (0x0041)
#define MPI3_IOCSTATUS_SCSI_INVALID_DEVHANDLE       (0x0042)
#define MPI3_IOCSTATUS_SCSI_DEVICE_NOT_THERE        (0x0043)
#define MPI3_IOCSTATUS_SCSI_DATA_OVERRUN            (0x0044)
#define MPI3_IOCSTATUS_SCSI_DATA_UNDERRUN           (0x0045)
#define MPI3_IOCSTATUS_SCSI_IO_DATA_ERROR           (0x0046)
#define MPI3_IOCSTATUS_SCSI_PROTOCOL_ERROR          (0x0047)
#define MPI3_IOCSTATUS_SCSI_TASK_TERMINATED         (0x0048)
#define MPI3_IOCSTATUS_SCSI_RESIDUAL_MISMATCH       (0x0049)
#define MPI3_IOCSTATUS_SCSI_TASK_MGMT_FAILED        (0x004A)
#define MPI3_IOCSTATUS_SCSI_IOC_TERMINATED          (0x004B)
#define MPI3_IOCSTATUS_SCSI_EXT_TERMINATED          (0x004C)

/* SCSI Initiator and SCSI Target end-to-end data protection values */
#define MPI3_IOCSTATUS_EEDP_GUARD_ERROR             (0x004D)
#define MPI3_IOCSTATUS_EEDP_REF_TAG_ERROR           (0x004E)
#define MPI3_IOCSTATUS_EEDP_APP_TAG_ERROR           (0x004F)

/* SCSI Target IOCStatus values */
#define MPI3_IOCSTATUS_TARGET_INVALID_IO_INDEX      (0x0062)
#define MPI3_IOCSTATUS_TARGET_ABORTED               (0x0063)
#define MPI3_IOCSTATUS_TARGET_NO_CONN_RETRYABLE     (0x0064)
#define MPI3_IOCSTATUS_TARGET_NO_CONNECTION         (0x0065)
#define MPI3_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH   (0x006A)
#define MPI3_IOCSTATUS_TARGET_DATA_OFFSET_ERROR     (0x006D)
#define MPI3_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA   (0x006E)
#define MPI3_IOCSTATUS_TARGET_IU_TOO_SHORT          (0x006F)
#define MPI3_IOCSTATUS_TARGET_ACK_NAK_TIMEOUT       (0x0070)
#define MPI3_IOCSTATUS_TARGET_NAK_RECEIVED          (0x0071)

/* Serial Attached SCSI IOCStatus values */
#define MPI3_IOCSTATUS_SAS_SMP_REQUEST_FAILED       (0x0090)
#define MPI3_IOCSTATUS_SAS_SMP_DATA_OVERRUN         (0x0091)

/* Diagnostic Buffer Post/Release IOCStatus values */
#define MPI3_IOCSTATUS_DIAGNOSTIC_RELEASED          (0x00A0)

/* Component Image Upload/Download */
#define MPI3_IOCSTATUS_CI_UNSUPPORTED               (0x00B0)
#define MPI3_IOCSTATUS_CI_UPDATE_SEQUENCE           (0x00B1)
#define MPI3_IOCSTATUS_CI_VALIDATION_FAILED         (0x00B2)
#define MPI3_IOCSTATUS_CI_KEY_UPDATE_PENDING        (0x00B3)
#define MPI3_IOCSTATUS_CI_KEY_UPDATE_NOT_POSSIBLE   (0x00B4)

/* Security values */
#define MPI3_IOCSTATUS_SECURITY_KEY_REQUIRED        (0x00C0)
#define MPI3_IOCSTATUS_SECURITY_VIOLATION           (0x00C1)

/* Request and Reply Queues related IOCStatus values */
#define MPI3_IOCSTATUS_INVALID_QUEUE_ID             (0x0F00)
#define MPI3_IOCSTATUS_INVALID_QUEUE_SIZE           (0x0F01)
#define MPI3_IOCSTATUS_INVALID_MSIX_VECTOR          (0x0F02)
#define MPI3_IOCSTATUS_INVALID_REPLY_QUEUE_ID       (0x0F03)
#define MPI3_IOCSTATUS_INVALID_QUEUE_DELETION       (0x0F04)

/**** Defines for IOCLogInfo ****/
#define MPI3_IOCLOGINFO_TYPE_MASK               (0xF0000000)
#define MPI3_IOCLOGINFO_TYPE_SHIFT              (28)
#define MPI3_IOCLOGINFO_TYPE_NONE               (0x0)
#define MPI3_IOCLOGINFO_TYPE_SAS                (0x3)
#define MPI3_IOCLOGINFO_LOG_DATA_MASK           (0x0FFFFFFF)
#define MPI3_IOCLOGINFO_LOG_DATA_SHIFT          (0)

#endif  /* MPI30_TRANSPORT_H */


