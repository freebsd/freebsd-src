/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016-2023, Broadcom Inc. All rights reserved.
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
 */

#ifndef MPI30_INIT_H
#define MPI30_INIT_H     1

/*****************************************************************************
 *              SCSI Initiator Messages                                      *
 ****************************************************************************/

/*****************************************************************************
 *              SCSI IO Request Message                                      *
 ****************************************************************************/
typedef struct _MPI3_SCSI_IO_CDB_EEDP32
{
    U8              CDB[20];                            /* 0x00 */
    U32             PrimaryReferenceTag;                /* 0x14 */
    U16             PrimaryApplicationTag;              /* 0x18 */
    U16             PrimaryApplicationTagMask;          /* 0x1A */
    U32             TransferLength;                     /* 0x1C */
} MPI3_SCSI_IO_CDB_EEDP32, MPI3_POINTER PTR_MPI3_SCSI_IO_CDB_EEDP32,
  Mpi3ScsiIoCdbEedp32_t, MPI3_POINTER pMpi3ScsiIoCdbEedp32_t;

typedef union _MPI3_SCSI_IO_CDB_UNION
{
    U8                      CDB32[32];
    MPI3_SCSI_IO_CDB_EEDP32 EEDP32;
    MPI3_SGE_SIMPLE         SGE;
} MPI3_SCSI_IO_CDB_UNION, MPI3_POINTER PTR_MPI3_SCSI_IO_CDB_UNION,
  Mpi3ScsiIoCdb_t, MPI3_POINTER pMpi3ScsiIoCdb_t;

typedef struct _MPI3_SCSI_IO_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U16                     DevHandle;                      /* 0x0A */
    U32                     Flags;                          /* 0x0C */
    U32                     SkipCount;                      /* 0x10 */
    U32                     DataLength;                     /* 0x14 */
    U8                      LUN[8];                         /* 0x18 */
    MPI3_SCSI_IO_CDB_UNION  CDB;                            /* 0x20 */
    MPI3_SGE_UNION          SGL[4];                         /* 0x40 */
} MPI3_SCSI_IO_REQUEST, MPI3_POINTER PTR_MPI3_SCSI_IO_REQUEST,
  Mpi3SCSIIORequest_t, MPI3_POINTER pMpi3SCSIIORequest_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_SCSIIO_MSGFLAGS_METASGL_VALID                    (0x80)
#define MPI3_SCSIIO_MSGFLAGS_DIVERT_TO_FIRMWARE               (0x40)

/**** Defines for the Flags field ****/
#define MPI3_SCSIIO_FLAGS_LARGE_CDB                           (0x60000000)
#define MPI3_SCSIIO_FLAGS_CDB_16_OR_LESS                      (0x00000000)
#define MPI3_SCSIIO_FLAGS_CDB_GREATER_THAN_16                 (0x20000000)
#define MPI3_SCSIIO_FLAGS_CDB_IN_SEPARATE_BUFFER              (0x40000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_MASK                  (0x07000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_SIMPLEQ               (0x00000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_HEADOFQ               (0x01000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_ORDEREDQ              (0x02000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_ACAQ                  (0x04000000)
#define MPI3_SCSIIO_FLAGS_CMDPRI_MASK                         (0x00F00000)
#define MPI3_SCSIIO_FLAGS_CMDPRI_SHIFT                        (20)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_MASK                  (0x000C0000)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_NO_DATA_TRANSFER      (0x00000000)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_WRITE                 (0x00040000)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_READ                  (0x00080000)
#define MPI3_SCSIIO_FLAGS_DMAOPERATION_MASK                   (0x00030000)
#define MPI3_SCSIIO_FLAGS_DMAOPERATION_HOST_PI                (0x00010000)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_MASK                  (0x000000F0)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_IO_THROTTLING         (0x00000010)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_WRITE_SAME_TOO_LARGE  (0x00000020)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_PROD_SPECIFIC         (0x00000080)

/**** Defines for the SGL field ****/
#define MPI3_SCSIIO_METASGL_INDEX                             (3)

/*****************************************************************************
 *              SCSI IO Error Reply Message                                  *
 ****************************************************************************/
typedef struct _MPI3_SCSI_IO_REPLY
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     IOCUseOnly08;                   /* 0x08 */
    U16                     IOCStatus;                      /* 0x0A */
    U32                     IOCLogInfo;                     /* 0x0C */
    U8                      SCSIStatus;                     /* 0x10 */
    U8                      SCSIState;                      /* 0x11 */
    U16                     DevHandle;                      /* 0x12 */
    U32                     TransferCount;                  /* 0x14 */
    U32                     SenseCount;                     /* 0x18 */
    U32                     ResponseData;                   /* 0x1C */
    U16                     TaskTag;                        /* 0x20 */
    U16                     SCSIStatusQualifier;            /* 0x22 */
    U32                     EEDPErrorOffset;                /* 0x24 */
    U16                     EEDPObservedAppTag;             /* 0x28 */
    U16                     EEDPObservedGuard;              /* 0x2A */
    U32                     EEDPObservedRefTag;             /* 0x2C */
    U64                     SenseDataBufferAddress;         /* 0x30 */
} MPI3_SCSI_IO_REPLY, MPI3_POINTER PTR_MPI3_SCSI_IO_REPLY,
  Mpi3SCSIIOReply_t, MPI3_POINTER pMpi3SCSIIOReply_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_SCSIIO_REPLY_MSGFLAGS_REFTAG_OBSERVED_VALID        (0x01)
#define MPI3_SCSIIO_REPLY_MSGFLAGS_APPTAG_OBSERVED_VALID        (0x02)
#define MPI3_SCSIIO_REPLY_MSGFLAGS_GUARD_OBSERVED_VALID         (0x04)

/**** Defines for the SCSIStatus field ****/
#define MPI3_SCSI_STATUS_GOOD                   (0x00)
#define MPI3_SCSI_STATUS_CHECK_CONDITION        (0x02)
#define MPI3_SCSI_STATUS_CONDITION_MET          (0x04)
#define MPI3_SCSI_STATUS_BUSY                   (0x08)
#define MPI3_SCSI_STATUS_INTERMEDIATE           (0x10)
#define MPI3_SCSI_STATUS_INTERMEDIATE_CONDMET   (0x14)
#define MPI3_SCSI_STATUS_RESERVATION_CONFLICT   (0x18)
#define MPI3_SCSI_STATUS_COMMAND_TERMINATED     (0x22)
#define MPI3_SCSI_STATUS_TASK_SET_FULL          (0x28)
#define MPI3_SCSI_STATUS_ACA_ACTIVE             (0x30)
#define MPI3_SCSI_STATUS_TASK_ABORTED           (0x40)

/**** Defines for the SCSIState field ****/
#define MPI3_SCSI_STATE_SENSE_MASK              (0x03)
#define MPI3_SCSI_STATE_SENSE_VALID             (0x00)
#define MPI3_SCSI_STATE_SENSE_FAILED            (0x01)
#define MPI3_SCSI_STATE_SENSE_BUFF_Q_EMPTY      (0x02)
#define MPI3_SCSI_STATE_SENSE_NOT_AVAILABLE     (0x03)
#define MPI3_SCSI_STATE_NO_SCSI_STATUS          (0x04)
#define MPI3_SCSI_STATE_TERMINATED              (0x08)
#define MPI3_SCSI_STATE_RESPONSE_DATA_VALID     (0x10)

/**** Defines for the ResponseData field ****/
#define MPI3_SCSI_RSP_RESPONSECODE_MASK         (0x000000FF)
#define MPI3_SCSI_RSP_RESPONSECODE_SHIFT        (0)
#define MPI3_SCSI_RSP_ARI2_MASK                 (0x0000FF00)
#define MPI3_SCSI_RSP_ARI2_SHIFT                (8)
#define MPI3_SCSI_RSP_ARI1_MASK                 (0x00FF0000)
#define MPI3_SCSI_RSP_ARI1_SHIFT                (16)
#define MPI3_SCSI_RSP_ARI0_MASK                 (0xFF000000)
#define MPI3_SCSI_RSP_ARI0_SHIFT                (24)

/**** Defines for the TaskTag field ****/
#define MPI3_SCSI_TASKTAG_UNKNOWN               (0xFFFF)


/*****************************************************************************
 *              SCSI Task Management Request Message                         *
 ****************************************************************************/
typedef struct _MPI3_SCSI_TASK_MGMT_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U16                     DevHandle;                      /* 0x0A */
    U16                     TaskHostTag;                    /* 0x0C */
    U8                      TaskType;                       /* 0x0E */
    U8                      Reserved0F;                     /* 0x0F */
    U16                     TaskRequestQueueID;             /* 0x10 */
    U8                      IOCUseOnly12;                   /* 0x12 */
    U8                      Reserved13;                     /* 0x13 */
    U32                     Reserved14;                     /* 0x14 */
    U8                      LUN[8];                         /* 0x18 */
} MPI3_SCSI_TASK_MGMT_REQUEST, MPI3_POINTER PTR_MPI3_SCSI_TASK_MGMT_REQUEST,
  Mpi3SCSITaskMgmtRequest_t, MPI3_POINTER pMpi3SCSITaskMgmtRequest_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_SCSITASKMGMT_MSGFLAGS_DO_NOT_SEND_TASK_IU      (0x08)

/**** Defines for the TaskType field ****/
#define MPI3_SCSITASKMGMT_TASKTYPE_ABORT_TASK               (0x01)
#define MPI3_SCSITASKMGMT_TASKTYPE_ABORT_TASK_SET           (0x02)
#define MPI3_SCSITASKMGMT_TASKTYPE_TARGET_RESET             (0x03)
#define MPI3_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET       (0x05)
#define MPI3_SCSITASKMGMT_TASKTYPE_CLEAR_TASK_SET           (0x06)
#define MPI3_SCSITASKMGMT_TASKTYPE_QUERY_TASK               (0x07)
#define MPI3_SCSITASKMGMT_TASKTYPE_CLEAR_ACA                (0x08)
#define MPI3_SCSITASKMGMT_TASKTYPE_QUERY_TASK_SET           (0x09)
#define MPI3_SCSITASKMGMT_TASKTYPE_QUERY_ASYNC_EVENT        (0x0A)
#define MPI3_SCSITASKMGMT_TASKTYPE_I_T_NEXUS_RESET          (0x0B)


/*****************************************************************************
 *              SCSI Task Management Reply Message                           *
 ****************************************************************************/
typedef struct _MPI3_SCSI_TASK_MGMT_REPLY
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     IOCUseOnly08;                   /* 0x08 */
    U16                     IOCStatus;                      /* 0x0A */
    U32                     IOCLogInfo;                     /* 0x0C */
    U32                     TerminationCount;               /* 0x10 */
    U32                     ResponseData;                   /* 0x14 */
    U32                     Reserved18;                     /* 0x18 */
} MPI3_SCSI_TASK_MGMT_REPLY, MPI3_POINTER PTR_MPI3_SCSI_TASK_MGMT_REPLY,
  Mpi3SCSITaskMgmtReply_t, MPI3_POINTER pMpi3SCSITaskMgmtReply_t;

/**** Defines for the ResponseData field - use MPI3_SCSI_RSP_ defines ****/

/**** Defines for the ResponseCode field - Byte 0 of ResponseData  ****/
#define MPI3_SCSITASKMGMT_RSPCODE_TM_COMPLETE                (0x00)
#define MPI3_SCSITASKMGMT_RSPCODE_INVALID_FRAME              (0x02)
#define MPI3_SCSITASKMGMT_RSPCODE_TM_FUNCTION_NOT_SUPPORTED  (0x04)
#define MPI3_SCSITASKMGMT_RSPCODE_TM_FAILED                  (0x05)
#define MPI3_SCSITASKMGMT_RSPCODE_TM_SUCCEEDED               (0x08)
#define MPI3_SCSITASKMGMT_RSPCODE_TM_INVALID_LUN             (0x09)
#define MPI3_SCSITASKMGMT_RSPCODE_TM_OVERLAPPED_TAG          (0x0A)

#define MPI3_SCSITASKMGMT_RSPCODE_IO_QUEUED_ON_IOC           (0x80)
#define MPI3_SCSITASKMGMT_RSPCODE_TM_NVME_DENIED             (0x81)

#endif  /* MPI30_INIT_H */
