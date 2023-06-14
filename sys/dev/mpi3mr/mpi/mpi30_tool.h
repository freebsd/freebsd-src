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

#ifndef MPI30_TOOL_H
#define MPI30_TOOL_H     1

/*****************************************************************************
 *                     Toolbox Messages                                      *
 *****************************************************************************/

/*****************************************************************************
 *                     Clean Tool Request Message                            *
 *****************************************************************************/
typedef struct _MPI3_TOOL_CLEAN_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U8                      Tool;                           /* 0x0A */
    U8                      Reserved0B;                     /* 0x0B */
    U32                     Area;                           /* 0x0C */
} MPI3_TOOL_CLEAN_REQUEST, MPI3_POINTER PTR_MPI3_TOOL_CLEAN_REQUEST,
  Mpi3ToolCleanRequest_t, MPI3_POINTER pMpi3ToolCleanRequest_t;

/**** Defines for the Tool field ****/
#define MPI3_TOOLBOX_TOOL_CLEAN                             (0x01)
#define MPI3_TOOLBOX_TOOL_ISTWI_READ_WRITE                  (0x02)
#define MPI3_TOOLBOX_TOOL_DIAGNOSTIC_CLI                    (0x03)
#define MPI3_TOOLBOX_TOOL_LANE_MARGINING                    (0x04)
#define MPI3_TOOLBOX_TOOL_RECOVER_DEVICE                    (0x05)
#define MPI3_TOOLBOX_TOOL_LOOPBACK                          (0x06)

/**** Bitfield definitions for Area field ****/
#define MPI3_TOOLBOX_CLEAN_AREA_BIOS_BOOT_SERVICES          (0x00000008)
#define MPI3_TOOLBOX_CLEAN_AREA_ALL_BUT_MFG                 (0x00000002)
#define MPI3_TOOLBOX_CLEAN_AREA_NVSTORE                     (0x00000001)


/*****************************************************************************
 *                ISTWI Read Write Tool Request Message                      *
 *****************************************************************************/
typedef struct _MPI3_TOOL_ISTWI_READ_WRITE_REQUEST
{
    U16                               HostTag;               /* 0x00 */
    U8                                IOCUseOnly02;          /* 0x02 */
    U8                                Function;              /* 0x03 */
    U16                               IOCUseOnly04;          /* 0x04 */
    U8                                IOCUseOnly06;          /* 0x06 */
    U8                                MsgFlags;              /* 0x07 */
    U16                               ChangeCount;           /* 0x08 */
    U8                                Tool;                  /* 0x0A */
    U8                                Flags;                 /* 0x0B */
    U8                                DevIndex;              /* 0x0C */
    U8                                Action;                /* 0x0D */
    U16                               Reserved0E;            /* 0x0E */
    U16                               TxDataLength;          /* 0x10 */
    U16                               RxDataLength;          /* 0x12 */
    U32                               Reserved14[3];         /* 0x14 */
    MPI3_MAN11_ISTWI_DEVICE_FORMAT    IstwiDevice;           /* 0x20 */
    MPI3_SGE_UNION                    SGL;                   /* 0x30 */
} MPI3_TOOL_ISTWI_READ_WRITE_REQUEST, MPI3_POINTER PTR_MPI3_TOOL_ISTWI_READ_WRITE_REQUEST,
  Mpi3ToolIstwiReadWriteRequest_t, MPI3_POINTER pMpi3ToolIstwiReadWRiteRequest_t;

/**** Bitfield definitions for Flags field ****/
#define MPI3_TOOLBOX_ISTWI_FLAGS_AUTO_RESERVE_RELEASE       (0x80)
#define MPI3_TOOLBOX_ISTWI_FLAGS_ADDRESS_MODE_MASK          (0x04)
#define MPI3_TOOLBOX_ISTWI_FLAGS_ADDRESS_MODE_DEVINDEX      (0x00)
#define MPI3_TOOLBOX_ISTWI_FLAGS_ADDRESS_MODE_DEVICE_FIELD  (0x04)
#define MPI3_TOOLBOX_ISTWI_FLAGS_PAGE_ADDRESS_MASK          (0x03)

/**** Definitions for the Action field ****/
#define MPI3_TOOLBOX_ISTWI_ACTION_RESERVE_BUS               (0x00)
#define MPI3_TOOLBOX_ISTWI_ACTION_RELEASE_BUS               (0x01)
#define MPI3_TOOLBOX_ISTWI_ACTION_RESET                     (0x02)
#define MPI3_TOOLBOX_ISTWI_ACTION_READ_DATA                 (0x03)
#define MPI3_TOOLBOX_ISTWI_ACTION_WRITE_DATA                (0x04)
#define MPI3_TOOLBOX_ISTWI_ACTION_SEQUENCE                  (0x05)


/**** Defines for the IstwiDevice field - refer to struct definition in mpi30_cnfg.h ****/


/*****************************************************************************
 *                ISTWI Read Write Tool Reply Message                        *
 *****************************************************************************/
typedef struct _MPI3_TOOL_ISTWI_READ_WRITE_REPLY
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
    U16                     IstwiStatus;                    /* 0x10 */
    U16                     Reserved12;                     /* 0x12 */
    U16                     TxDataCount;                    /* 0x14 */
    U16                     RxDataCount;                    /* 0x16 */
} MPI3_TOOL_ISTWI_READ_WRITE_REPLY, MPI3_POINTER PTR_MPI3_TOOL_ISTWI_READ_WRITE_REPLY,
  Mpi3ToolIstwiReadWriteReply_t, MPI3_POINTER pMpi3ToolIstwiReadWRiteReply_t;



/*****************************************************************************
 *               Diagnostic CLI Tool Request Message                         *
 *****************************************************************************/
typedef struct _MPI3_TOOL_DIAGNOSTIC_CLI_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U8                      Tool;                           /* 0x0A */
    U8                      Reserved0B;                     /* 0x0B */
    U32                     CommandDataLength;              /* 0x0C */
    U32                     ResponseDataLength;             /* 0x10 */
    U32                     Reserved14[3];                  /* 0x14 */
    MPI3_SGE_UNION          SGL;                            /* 0x20 */
} MPI3_TOOL_DIAGNOSTIC_CLI_REQUEST, MPI3_POINTER PTR_MPI3_TOOL_DIAGNOSTIC_CLI_REQUEST,
  Mpi3ToolDiagnosticCliRequest_t, MPI3_POINTER pMpi3ToolDiagnosticCliRequest_t;


/*****************************************************************************
 *               Diagnostic CLI Tool Reply Message                           *
 *****************************************************************************/
typedef struct _MPI3_TOOL_DIAGNOSTIC_CLI_REPLY
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
    U32                     ReturnedDataLength;             /* 0x10 */
} MPI3_TOOL_DIAGNOSTIC_CLI_REPLY, MPI3_POINTER PTR_MPI3_TOOL_DIAGNOSTIC_CLI_REPLY,
  Mpi3ToolDiagnosticCliReply_t, MPI3_POINTER pMpi3ToolDiagnosticCliReply_t;


/*****************************************************************************
 *                Lane Margining Tool Request Message                        *
 *****************************************************************************/
typedef struct _MPI3_TOOL_LANE_MARGIN_REQUEST
{
    U16                               HostTag;               /* 0x00 */
    U8                                IOCUseOnly02;          /* 0x02 */
    U8                                Function;              /* 0x03 */
    U16                               IOCUseOnly04;          /* 0x04 */
    U8                                IOCUseOnly06;          /* 0x06 */
    U8                                MsgFlags;              /* 0x07 */
    U16                               ChangeCount;           /* 0x08 */
    U8                                Tool;                  /* 0x0A */
    U8                                Reserved0B;            /* 0x0B */
    U8                                Action;                /* 0x0C */
    U8                                SwitchPort;            /* 0x0D */
    U16                               DevHandle;             /* 0x0E */
    U8                                StartLane;             /* 0x10 */
    U8                                NumLanes;              /* 0x11 */
    U16                               Reserved12;            /* 0x12 */
    U32                               Reserved14[3];         /* 0x14 */
    MPI3_SGE_UNION                    SGL;                   /* 0x20 */
} MPI3_TOOL_LANE_MARGIN_REQUEST, MPI3_POINTER PTR_MPI3_TOOL_LANE_MARGIN_REQUEST,
  Mpi3ToolIstwiLaneMarginRequest_t, MPI3_POINTER pMpi3ToolLaneMarginRequest_t;

/**** Definitions for the Action field ****/
#define MPI3_TOOLBOX_LM_ACTION_ENTER                         (0x00)
#define MPI3_TOOLBOX_LM_ACTION_EXIT                          (0x01)
#define MPI3_TOOLBOX_LM_ACTION_READ                          (0x02)
#define MPI3_TOOLBOX_LM_ACTION_WRITE                         (0x03)

typedef struct _MPI3_LANE_MARGIN_ELEMENT
{
    U16                               Control;                /* 0x00 */
    U16                               Status;                 /* 0x02 */
} MPI3_LANE_MARGIN_ELEMENT, MPI3_POINTER PTR_MPI3_LANE_MARGIN_ELEMENT,
  Mpi3LaneMarginElement_t, MPI3_POINTER pMpi3LaneMarginElement_t;

/*****************************************************************************
 *                Lane Margining Tool Reply Message                          *
 *****************************************************************************/
typedef struct _MPI3_TOOL_LANE_MARGIN_REPLY
{
    U16                               HostTag;               /* 0x00 */
    U8                                IOCUseOnly02;          /* 0x02 */
    U8                                Function;              /* 0x03 */
    U16                               IOCUseOnly04;          /* 0x04 */
    U8                                IOCUseOnly06;          /* 0x06 */
    U8                                MsgFlags;              /* 0x07 */
    U16                               IOCUseOnly08;          /* 0x08 */
    U16                               IOCStatus;             /* 0x0A */
    U32                               IOCLogInfo;            /* 0x0C */
    U32                               ReturnedDataLength;    /* 0x10 */
} MPI3_TOOL_LANE_MARGIN_REPLY, MPI3_POINTER PTR_MPI3_TOOL_LANE_MARGIN_REPLY,
  Mpi3ToolLaneMarginReply_t, MPI3_POINTER pMpi3ToolLaneMarginReply_t;

/*****************************************************************************
 *               Recover Device Request Message                              *
 *****************************************************************************/
typedef struct _MPI3_TOOL_RECOVER_DEVICE_REQUEST
{
    U16                               HostTag;               /* 0x00 */
    U8                                IOCUseOnly02;          /* 0x02 */
    U8                                Function;              /* 0x03 */
    U16                               IOCUseOnly04;          /* 0x04 */
    U8                                IOCUseOnly06;          /* 0x06 */
    U8                                MsgFlags;              /* 0x07 */
    U16                               ChangeCount;           /* 0x08 */
    U8                                Tool;                  /* 0x0A */
    U8                                Reserved0B;            /* 0x0B */
    U8                                Action;                /* 0x0C */
    U8                                Reserved0D;            /* 0x0D */
    U16                               DevHandle;             /* 0x0E */
} MPI3_TOOL_RECOVER_DEVICE_REQUEST, MPI3_POINTER PTR_MPI3_TOOL_RECOVER_DEVICE_REQUEST,
  Mpi3ToolRecoverDeviceRequest_t, MPI3_POINTER pMpi3ToolRecoverDeviceRequest_t;

/**** Bitfield definitions for the Action field ****/
#define MPI3_TOOLBOX_RD_ACTION_START                        (0x01)
#define MPI3_TOOLBOX_RD_ACTION_GET_STATUS                   (0x02)
#define MPI3_TOOLBOX_RD_ACTION_ABORT                        (0x03)

/*****************************************************************************
 *               Recover Device Reply Message                                *
 *****************************************************************************/
typedef struct _MPI3_TOOL_RECOVER_DEVICE_REPLY
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
    U8                      Status;                         /* 0x10 */
    U8                      Reserved11;                     /* 0x11 */
    U16                     Reserved1C;                     /* 0x12 */
} MPI3_TOOL_RECOVER_DEVICE_REPLY, MPI3_POINTER PTR_MPI3_TOOL_RECOVER_DEVICE_REPLY,
  Mpi3ToolRecoverDeviceReply_t, MPI3_POINTER pMpi3ToolRecoverDeviceReply_t;

/**** Bitfield definitions for the Status field ****/
#define MPI3_TOOLBOX_RD_STATUS_NOT_NEEDED                   (0x01)
#define MPI3_TOOLBOX_RD_STATUS_NEEDED                       (0x02)
#define MPI3_TOOLBOX_RD_STATUS_IN_PROGRESS                  (0x03)
#define MPI3_TOOLBOX_RD_STATUS_ABORTING                     (0x04)

/*****************************************************************************
 *               Loopback Tool Request Message                               *
 *****************************************************************************/
typedef struct _MPI3_TOOL_LOOPBACK_REQUEST
{
    U16                               HostTag;               /* 0x00 */
    U8                                IOCUseOnly02;          /* 0x02 */
    U8                                Function;              /* 0x03 */
    U16                               IOCUseOnly04;          /* 0x04 */
    U8                                IOCUseOnly06;          /* 0x06 */
    U8                                MsgFlags;              /* 0x07 */
    U16                               ChangeCount;           /* 0x08 */
    U8                                Tool;                  /* 0x0A */
    U8                                Reserved0B;            /* 0x0B */
    U32                               Reserved0C;            /* 0x0C */
    U64                               Phys;                  /* 0x10 */
} MPI3_TOOL_LOOPBACK_REQUEST, MPI3_POINTER PTR_MPI3_TOOL_LOOPBACK_REQUEST,
  Mpi3ToolLoopbackRequest_t, MPI3_POINTER pMpi3ToolLoopbackRequest_t;

/*****************************************************************************
 *               Loopback Tool Reply Message                                 *
 *****************************************************************************/
typedef struct _MPI3_TOOL_LOOPBACK_REPLY
{
    U16                               HostTag;               /* 0x00 */
    U8                                IOCUseOnly02;          /* 0x02 */
    U8                                Function;              /* 0x03 */
    U16                               IOCUseOnly04;          /* 0x04 */
    U8                                IOCUseOnly06;          /* 0x06 */
    U8                                MsgFlags;              /* 0x07 */
    U16                               IOCUseOnly08;          /* 0x08 */
    U16                               IOCStatus;             /* 0x0A */
    U32                               IOCLogInfo;            /* 0x0C */
    U64                               TestedPhys;            /* 0x10 */
    U64                               FailedPhys;            /* 0x18 */
} MPI3_TOOL_LOOPBACK_REPLY, MPI3_POINTER PTR_MPI3_TOOL_LOOPBACK_REPLY,
  Mpi3ToolLoopbackReply_t, MPI3_POINTER pMpi3ToolLoopbackReply_t;


/*****************************************************************************
 *                     Diagnostic Buffer Messages                            *
 *****************************************************************************/

/*****************************************************************************
 *               Diagnostic Buffer Post Request Message                      *
 *****************************************************************************/
typedef struct _MPI3_DIAG_BUFFER_POST_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U16                     Reserved0A;                     /* 0x0A */
    U8                      Type;                           /* 0x0C */
    U8                      Reserved0D;                     /* 0x0D */
    U16                     Reserved0E;                     /* 0x0E */
    U64                     Address;                        /* 0x10 */
    U32                     Length;                         /* 0x18 */
    U32                     Reserved1C;                     /* 0x1C */
} MPI3_DIAG_BUFFER_POST_REQUEST, MPI3_POINTER PTR_MPI3_DIAG_BUFFER_POST_REQUEST,
  Mpi3DiagBufferPostRequest_t, MPI3_POINTER pMpi3DiagBufferPostRequest_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_DIAG_BUFFER_POST_MSGFLAGS_SEGMENTED            (0x01)

/**** Defines for the Type field ****/
#define MPI3_DIAG_BUFFER_TYPE_TRACE                         (0x01)
#define MPI3_DIAG_BUFFER_TYPE_FW                            (0x02)
#define MPI3_DIAG_BUFFER_TYPE_DRIVER                        (0x10)
#define MPI3_DIAG_BUFFER_TYPE_FDL                           (0x20)
#define MPI3_DIAG_BUFFER_MIN_PRODUCT_SPECIFIC               (0xF0)
#define MPI3_DIAG_BUFFER_MAX_PRODUCT_SPECIFIC               (0xFF)


/*****************************************************************************
 *                 DRIVER DIAGNOSTIC Buffer                                  *
 *****************************************************************************/
typedef struct _MPI3_DRIVER_BUFFER_HEADER
{
    U32                     Signature;                      /* 0x00 */
    U16                     HeaderSize;                     /* 0x04 */
    U16                     RTTFileHeaderOffset;            /* 0x06 */
    U32                     Flags;                          /* 0x08 */
    U32                     CircularBufferSize;             /* 0x0C */
    U32                     LogicalBufferEnd;               /* 0x10 */
    U32                     LogicalBufferStart;             /* 0x14 */
    U32                     IOCUseOnly18[2];                /* 0x18 */
    U32                     Reserved20[760];                /* 0x20  - 0xBFC */
    U32                     ReservedRTTRACE[256];           /* 0xC00 - 0xFFC */
} MPI3_DRIVER_BUFFER_HEADER, MPI3_POINTER PTR_MPI3_DRIVER_BUFFER_HEADER,
  Mpi3DriverBufferHeader_t, MPI3_POINTER pMpi3DriverBufferHeader_t;

/**** Defines for the Type field ****/
#define MPI3_DRIVER_DIAG_BUFFER_HEADER_SIGNATURE_CIRCULAR                (0x43495243)

/**** Defines for the Flags field ****/
#define MPI3_DRIVER_DIAG_BUFFER_HEADER_FLAGS_CIRCULAR_BUF_FORMAT_MASK    (0x00000003)
#define MPI3_DRIVER_DIAG_BUFFER_HEADER_FLAGS_CIRCULAR_BUF_FORMAT_ASCII   (0x00000000)
#define MPI3_DRIVER_DIAG_BUFFER_HEADER_FLAGS_CIRCULAR_BUF_FORMAT_RTTRACE (0x00000001)

/*****************************************************************************
 *               Diagnostic Buffer Manage Request Message                      *
 *****************************************************************************/
typedef struct _MPI3_DIAG_BUFFER_MANAGE_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U16                     Reserved0A;                     /* 0x0A */
    U8                      Type;                           /* 0x0C */
    U8                      Action;                         /* 0x0D */
    U16                     Reserved0E;                     /* 0x0E */
} MPI3_DIAG_BUFFER_MANAGE_REQUEST, MPI3_POINTER PTR_MPI3_DIAG_BUFFER_MANAGE_REQUEST,
  Mpi3DiagBufferManageRequest_t, MPI3_POINTER pMpi3DiagBufferManageRequest_t;

/**** Defines for the Type field - use MPI3_DIAG_BUFFER_TYPE_ values ****/

/**** Defined for the Action field ****/
#define MPI3_DIAG_BUFFER_ACTION_RELEASE                     (0x01)
#define MPI3_DIAG_BUFFER_ACTION_PAUSE                       (0x02)
#define MPI3_DIAG_BUFFER_ACTION_RESUME                      (0x03)

/*****************************************************************************
 *               Diagnostic Buffer Upload Request Message                    *
 *****************************************************************************/
typedef struct _MPI3_DIAG_BUFFER_UPLOAD_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U16                     Reserved0A;                     /* 0x0A */
    U8                      Type;                           /* 0x0C */
    U8                      Flags;                          /* 0x0D */
    U16                     Reserved0E;                     /* 0x0E */
    U64                     Context;                        /* 0x10 */
    U32                     Reserved18;                     /* 0x18 */
    U32                     Reserved1C;                     /* 0x1C */
    MPI3_SGE_UNION          SGL;                            /* 0x20 */
} MPI3_DIAG_BUFFER_UPLOAD_REQUEST, MPI3_POINTER PTR_MPI3_DIAG_BUFFER_UPLOAD_REQUEST,
  Mpi3DiagBufferUploadRequest_t, MPI3_POINTER pMpi3DiagBufferUploadRequest_t;

/**** Defines for the Type field - use MPI3_DIAG_BUFFER_TYPE_ values ****/

/**** Defined for the Flags field ****/
#define MPI3_DIAG_BUFFER_UPLOAD_FLAGS_FORMAT_MASK           (0x01)
#define MPI3_DIAG_BUFFER_UPLOAD_FLAGS_FORMAT_DECODED        (0x00)
#define MPI3_DIAG_BUFFER_UPLOAD_FLAGS_FORMAT_ENCODED        (0x01)

/*****************************************************************************
 *               Diagnostic Buffer Upload Reply Message                      *
 *****************************************************************************/
typedef struct _MPI3_DIAG_BUFFER_UPLOAD_REPLY
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
    U64                     Context;                        /* 0x10 */
    U32                     ReturnedDataLength;             /* 0x18 */
    U32                     Reserved1C;                     /* 0x1C */
} MPI3_DIAG_BUFFER_UPLOAD_REPLY, MPI3_POINTER PTR_MPI3_DIAG_BUFFER_UPLOAD_REPLY,
  Mpi3DiagBufferUploadReply_t, MPI3_POINTER pMpi3DiagBufferUploadReply_t;

#endif /* MPI30_TOOL_H */

