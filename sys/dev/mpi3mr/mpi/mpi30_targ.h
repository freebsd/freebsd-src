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
 */

#ifndef MPI30_TARG_H
#define MPI30_TARG_H     1

/*****************************************************************************
 *              Command Buffer Formats                                       *
 ****************************************************************************/
typedef struct _MPI3_TARGET_SSP_CMD_BUFFER
{
    U8                      FrameType;                  /* 0x00 */
    U8                      Reserved01;                 /* 0x01 */
    U16                     InitiatorConnectionTag;     /* 0x02 */
    U32                     HashedSourceSASAddress;     /* 0x04 */
    U16                     Reserved08;                 /* 0x08 */
    U16                     Flags;                      /* 0x0A */
    U32                     Reserved0C;                 /* 0x0C */
    U16                     Tag;                        /* 0x10 */
    U16                     TargetPortTransferTag;      /* 0x12 */
    U32                     DataOffset;                 /* 0x14 */
    U8                      LogicalUnitNumber[8];       /* 0x18 */
    U8                      Reserved20;                 /* 0x20 */
    U8                      TaskAttribute;              /* 0x21 */
    U8                      Reserved22;                 /* 0x22 */
    U8                      AdditionalCDBLength;        /* 0x23 */
    U8                      CDB[16];                    /* 0x24 */
    /* AdditionalCDBBytes field starts here */          /* 0x34 */
} MPI3_TARGET_SSP_CMD_BUFFER, MPI3_POINTER PTR_MPI3_TARGET_SSP_CMD_BUFFER,
  Mpi3TargetSspCmdBuffer_t, MPI3_POINTER pMpi3TargetSspCmdBuffer_t;

typedef struct _MPI3_TARGET_SSP_TASK_BUFFER
{
    U8                      FrameType;                  /* 0x00 */
    U8                      Reserved01;                 /* 0x01 */
    U16                     InitiatorConnectionTag;     /* 0x02 */
    U32                     HashedSourceSASAddress;     /* 0x04 */
    U16                     Reserved08;                 /* 0x08 */
    U16                     Flags;                      /* 0x0A */
    U32                     Reserved0C;                 /* 0x0C */
    U16                     Tag;                        /* 0x10 */
    U16                     TargetPortTransferTag;      /* 0x12 */
    U32                     DataOffset;                 /* 0x14 */
    U8                      LogicalUnitNumber[8];       /* 0x18 */
    U16                     Reserved20;                 /* 0x20 */
    U8                      TaskManagementFunction;     /* 0x22 */
    U8                      Reserved23;                 /* 0x23 */
    U16                     ManagedTaskTag;             /* 0x24 */
    U16                     Reserved26;                 /* 0x26 */
    U32                     Reserved28[3];              /* 0x28 */
} MPI3_TARGET_SSP_TASK_BUFFER, MPI3_POINTER PTR_MPI3_TARGET_SSP_TASK_BUFFER,
  Mpi3TargetSspTaskBuffer_t, MPI3_POINTER pMpi3TargetSspTaskBuffer_t;

/**** Defines for the FrameType field ****/
#define MPI3_TARGET_FRAME_TYPE_COMMAND                      (0x06)
#define MPI3_TARGET_FRAME_TYPE_TASK                         (0x16)

/**** Defines for the HashedSourceSASAddress field ****/
#define MPI3_TARGET_HASHED_SAS_ADDRESS_MASK                 (0xFFFFFF00)
#define MPI3_TARGET_HASHED_SAS_ADDRESS_SHIFT                (8)


/*****************************************************************************
 *              Target Command Buffer Post Base Request Message              *
 ****************************************************************************/
typedef struct _MPI3_TARGET_CMD_BUF_POST_BASE_REQUEST
{
    U16                     HostTag;                    /* 0x00 */
    U8                      IOCUseOnly02;               /* 0x02 */
    U8                      Function;                   /* 0x03 */
    U16                     IOCUseOnly04;               /* 0x04 */
    U8                      IOCUseOnly06;               /* 0x06 */
    U8                      MsgFlags;                   /* 0x07 */
    U16                     ChangeCount;                /* 0x08 */
    U8                      BufferPostFlags;            /* 0x0A */
    U8                      Reserved0B;                 /* 0x0B */
    U16                     MinReplyQueueID;            /* 0x0C */
    U16                     MaxReplyQueueID;            /* 0x0E */
    U64                     BaseAddress;                /* 0x10 */
    U16                     CmdBufferLength;            /* 0x18 */
    U16                     TotalCmdBuffers;            /* 0x1A */
    U32                     Reserved1C;                 /* 0x1C */
} MPI3_TARGET_CMD_BUF_POST_BASE_REQUEST, MPI3_POINTER PTR_MPI3_TARGET_CMD_BUF_POST_BASE_REQUEST,
  Mpi3TargetCmdBufPostBaseRequest_t, MPI3_POINTER pMpi3TargetCmdBufPostBaseRequest_t;

/**** Defines for the BufferPostFlags field ****/
#define MPI3_CMD_BUF_POST_BASE_FLAGS_DLAS_MASK              (0x0C)
#define MPI3_CMD_BUF_POST_BASE_FLAGS_DLAS_SHIFT             (2)
#define MPI3_CMD_BUF_POST_BASE_FLAGS_DLAS_SYSTEM            (0x00)
#define MPI3_CMD_BUF_POST_BASE_FLAGS_DLAS_IOCUDP            (0x04)
#define MPI3_CMD_BUF_POST_BASE_FLAGS_DLAS_IOCCTL            (0x08)
#define MPI3_CMD_BUF_POST_BASE_FLAGS_AUTO_POST_ALL          (0x01)

/**** Defines for the CmdBufferLength field ****/
#define MPI3_CMD_BUF_POST_BASE_MIN_BUF_LENGTH               (0x34)
#define MPI3_CMD_BUF_POST_BASE_MAX_BUF_LENGTH               (0x3FC)

/*****************************************************************************
 *              Target Command Buffer Post List Request Message              *
 ****************************************************************************/
typedef struct _MPI3_TARGET_CMD_BUF_POST_LIST_REQUEST
{
    U16                     HostTag;                    /* 0x00 */
    U8                      IOCUseOnly02;               /* 0x02 */
    U8                      Function;                   /* 0x03 */
    U16                     IOCUseOnly04;               /* 0x04 */
    U8                      IOCUseOnly06;               /* 0x06 */
    U8                      MsgFlags;                   /* 0x07 */
    U16                     ChangeCount;                /* 0x08 */
    U16                     Reserved0A;                 /* 0x0A */
    U8                      CmdBufferCount;             /* 0x0C */
    U8                      Reserved0D[3];              /* 0x0D */
    U16                     IoIndex[2];                 /* 0x10 */
} MPI3_TARGET_CMD_BUF_POST_LIST_REQUEST, MPI3_POINTER PTR_MPI3_TARGET_CMD_BUF_POST_LIST_REQUEST,
  Mpi3TargetCmdBufPostListRequest_t, MPI3_POINTER pMpi3TargetCmdBufPostListRequest_t;


/*****************************************************************************
 *              Target Command Buffer Post Base List Reply Message           *
 ****************************************************************************/
typedef struct _MPI3_TARGET_CMD_BUF_POST_REPLY
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
    U8                      CmdBufferCount;                 /* 0x10 */
    U8                      Reserved11[3];                  /* 0x11 */
    U16                     IoIndex[2];                     /* 0x14 */
} MPI3_TARGET_CMD_BUF_POST_REPLY, MPI3_POINTER PTR_MPI3_TARGET_CMD_BUF_POST_REPLY,
  Mpi3TargetCmdBufPostReply_t, MPI3_POINTER pMpi3TargetCmdBufPostReply_t;


/*****************************************************************************
 *              Target Assist Request Message                                *
 ****************************************************************************/
typedef struct _MPI3_TARGET_ASSIST_REQUEST
{
    U16                     HostTag;                    /* 0x00 */
    U8                      IOCUseOnly02;               /* 0x02 */
    U8                      Function;                   /* 0x03 */
    U16                     IOCUseOnly04;               /* 0x04 */
    U8                      IOCUseOnly06;               /* 0x06 */
    U8                      MsgFlags;                   /* 0x07 */
    U16                     ChangeCount;                /* 0x08 */
    U16                     DevHandle;                  /* 0x0A */
    U32                     Flags;                      /* 0x0C */
    U16                     Reserved10;                 /* 0x10 */
    U16                     QueueTag;                   /* 0x12 */
    U16                     IoIndex;                    /* 0x14 */
    U16                     InitiatorConnectionTag;     /* 0x16 */
    U32                     IOCUseOnly18;               /* 0x18 */
    U32                     DataLength;                 /* 0x1C */
    U32                     PortTransferLength;         /* 0x20 */
    U32                     PrimaryReferenceTag;        /* 0x24 */
    U16                     PrimaryApplicationTag;      /* 0x28 */
    U16                     PrimaryApplicationTagMask;  /* 0x2A */
    U32                     RelativeOffset;             /* 0x2C */
    MPI3_SGE_UNION          SGL[5];                     /* 0x30 */
} MPI3_TARGET_ASSIST_REQUEST, MPI3_POINTER PTR_MPI3_TARGET_ASSIST_REQUEST,
  Mpi3TargetAssistRequest_t, MPI3_POINTER pMpi3TargetAssistRequest_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_TARGET_ASSIST_MSGFLAGS_METASGL_VALID           (0x80)

/**** Defines for the Flags field ****/
#define MPI3_TARGET_ASSIST_FLAGS_IOC_USE_ONLY_23_MASK       (0x00800000)
#define MPI3_TARGET_ASSIST_FLAGS_IOC_USE_ONLY_23_SHIFT      (23)
#define MPI3_TARGET_ASSIST_FLAGS_IOC_USE_ONLY_22_MASK       (0x00400000)
#define MPI3_TARGET_ASSIST_FLAGS_IOC_USE_ONLY_22_SHIFT      (22)
#define MPI3_TARGET_ASSIST_FLAGS_REPOST_CMD_BUFFER          (0x00200000)
#define MPI3_TARGET_ASSIST_FLAGS_AUTO_STATUS                (0x00100000)
#define MPI3_TARGET_ASSIST_FLAGS_DATADIRECTION_MASK         (0x000C0000)
#define MPI3_TARGET_ASSIST_FLAGS_DATADIRECTION_SHIFT        (18)
#define MPI3_TARGET_ASSIST_FLAGS_DATADIRECTION_WRITE        (0x00040000)
#define MPI3_TARGET_ASSIST_FLAGS_DATADIRECTION_READ         (0x00080000)
#define MPI3_TARGET_ASSIST_FLAGS_DMAOPERATION_MASK          (0x00030000)
#define MPI3_TARGET_ASSIST_FLAGS_DMAOPERATION_SHIFT         (16)
#define MPI3_TARGET_ASSIST_FLAGS_DMAOPERATION_HOST_PI       (0x00010000)

/**** Defines for the SGL field ****/
#define MPI3_TARGET_ASSIST_METASGL_INDEX                    (4)

/*****************************************************************************
 *              Target Status Send Request Message                           *
 ****************************************************************************/
typedef struct _MPI3_TARGET_STATUS_SEND_REQUEST
{
    U16                     HostTag;                    /* 0x00 */
    U8                      IOCUseOnly02;               /* 0x02 */
    U8                      Function;                   /* 0x03 */
    U16                     IOCUseOnly04;               /* 0x04 */
    U8                      IOCUseOnly06;               /* 0x06 */
    U8                      MsgFlags;                   /* 0x07 */
    U16                     ChangeCount;                /* 0x08 */
    U16                     DevHandle;                  /* 0x0A */
    U16                     ResponseIULength;           /* 0x0C */
    U16                     Flags;                      /* 0x0E */
    U16                     Reserved10;                 /* 0x10 */
    U16                     QueueTag;                   /* 0x12 */
    U16                     IoIndex;                    /* 0x14 */
    U16                     InitiatorConnectionTag;     /* 0x16 */
    U32                     IOCUseOnly18[6];            /* 0x18 */
    U32                     IOCUseOnly30[4];            /* 0x30 */
    MPI3_SGE_UNION          SGL;                        /* 0x40 */
} MPI3_TARGET_STATUS_SEND_REQUEST, MPI3_POINTER PTR_MPI3_TARGET_STATUS_SEND_REQUEST,
  Mpi3TargetStatusSendRequest_t, MPI3_POINTER pMpi3TargetStatusSendRequest_t;

/**** Defines for the Flags field ****/
#define MPI3_TSS_FLAGS_IOC_USE_ONLY_6_MASK              (0x0040)
#define MPI3_TSS_FLAGS_IOC_USE_ONLY_6_SHIFT             (6)
#define MPI3_TSS_FLAGS_REPOST_CMD_BUFFER                (0x0020)
#define MPI3_TSS_FLAGS_AUTO_SEND_GOOD_STATUS            (0x0010)


/*****************************************************************************
 *              Standard Target Mode Reply Message                           *
 ****************************************************************************/
typedef struct _MPI3_TARGET_STANDARD_REPLY
{
    U16                     HostTag;                    /* 0x00 */
    U8                      IOCUseOnly02;               /* 0x02 */
    U8                      Function;                   /* 0x03 */
    U16                     IOCUseOnly04;               /* 0x04 */
    U8                      IOCUseOnly06;               /* 0x06 */
    U8                      MsgFlags;                   /* 0x07 */
    U16                     IOCUseOnly08;               /* 0x08 */
    U16                     IOCStatus;                  /* 0x0A */
    U32                     IOCLogInfo;                 /* 0x0C */
    U32                     TransferCount;              /* 0x10 */
} MPI3_TARGET_STANDARD_REPLY, MPI3_POINTER PTR_MPI3_TARGET_STANDARD_REPLY,
  Mpi3TargetStandardReply_t, MPI3_POINTER pMpi3TargetStandardReply_t;


/*****************************************************************************
 *              Target Mode Abort Request Message                            *
 ****************************************************************************/
typedef struct _MPI3_TARGET_MODE_ABORT_REQUEST
{
    U16                     HostTag;                    /* 0x00 */
    U8                      IOCUseOnly02;               /* 0x02 */
    U8                      Function;                   /* 0x03 */
    U16                     IOCUseOnly04;               /* 0x04 */
    U8                      IOCUseOnly06;               /* 0x06 */
    U8                      MsgFlags;                   /* 0x07 */
    U16                     ChangeCount;                /* 0x08 */
    U8                      AbortType;                  /* 0x0A */
    U8                      Reserved0B;                 /* 0x0B */
    U16                     RequestQueueIDToAbort;      /* 0x0C */
    U16                     HostTagToAbort;             /* 0x0E */
    U16                     DevHandle;                  /* 0x10 */
    U8                      IOCUseOnly12;               /* 0x12 */
    U8                      Reserved13;                 /* 0x13 */
} MPI3_TARGET_MODE_ABORT_REQUEST, MPI3_POINTER PTR_MPI3_TARGET_MODE_ABORT_REQUEST,
  Mpi3TargetModeAbortRequest_t, MPI3_POINTER pMpi3TargetModeAbortRequest_t;

/**** Defines for the AbortType field ****/
#define MPI3_TARGET_MODE_ABORT_ALL_CMD_BUFFERS              (0x00)
#define MPI3_TARGET_MODE_ABORT_EXACT_IO_REQUEST             (0x01)
#define MPI3_TARGET_MODE_ABORT_ALL_COMMANDS                 (0x02)
#define MPI3_TARGET_MODE_ABORT_ALL_COMMANDS_DEVHANDLE       (0x03)

/*****************************************************************************
 *              Target Mode Abort Reply Message                              *
 ****************************************************************************/
typedef struct _MPI3_TARGET_MODE_ABORT_REPLY
{
    U16                     HostTag;                    /* 0x00 */
    U8                      IOCUseOnly02;               /* 0x02 */
    U8                      Function;                   /* 0x03 */
    U16                     IOCUseOnly04;               /* 0x04 */
    U8                      IOCUseOnly06;               /* 0x06 */
    U8                      MsgFlags;                   /* 0x07 */
    U16                     IOCUseOnly08;               /* 0x08 */
    U16                     IOCStatus;                  /* 0x0A */
    U32                     IOCLogInfo;                 /* 0x0C */
    U32                     AbortCount;                 /* 0x10 */
} MPI3_TARGET_MODE_ABORT_REPLY, MPI3_POINTER PTR_MPI3_TARGET_MODE_ABORT_REPLY,
  Mpi3TargetModeAbortReply_t, MPI3_POINTER pMpi3TargetModeAbortReply_t;

#endif  /* MPI30_TARG_H */

