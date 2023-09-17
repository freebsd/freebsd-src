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

#ifndef MPI30_IOC_H
#define MPI30_IOC_H     1

/*****************************************************************************
 *              IOC Messages                                                 *
 ****************************************************************************/

/*****************************************************************************
 *              IOCInit Request Message                                      *
 ****************************************************************************/
typedef struct _MPI3_IOC_INIT_REQUEST
{
    U16                   HostTag;                            /* 0x00 */
    U8                    IOCUseOnly02;                       /* 0x02 */
    U8                    Function;                           /* 0x03 */
    U16                   IOCUseOnly04;                       /* 0x04 */
    U8                    IOCUseOnly06;                       /* 0x06 */
    U8                    MsgFlags;                           /* 0x07 */
    U16                   ChangeCount;                        /* 0x08 */
    U16                   Reserved0A;                         /* 0x0A */
    MPI3_VERSION_UNION    MPIVersion;                         /* 0x0C */
    U64                   TimeStamp;                          /* 0x10 */
    U8                    Reserved18;                         /* 0x18 */
    U8                    WhoInit;                            /* 0x19 */
    U16                   Reserved1A;                         /* 0x1A */
    U16                   ReplyFreeQueueDepth;                /* 0x1C */
    U16                   Reserved1E;                         /* 0x1E */
    U64                   ReplyFreeQueueAddress;              /* 0x20 */
    U32                   Reserved28;                         /* 0x28 */
    U16                   SenseBufferFreeQueueDepth;          /* 0x2C */
    U16                   SenseBufferLength;                  /* 0x2E */
    U64                   SenseBufferFreeQueueAddress;        /* 0x30 */
    U64                   DriverInformationAddress;           /* 0x38 */
} MPI3_IOC_INIT_REQUEST, MPI3_POINTER PTR_MPI3_IOC_INIT_REQUEST,
  Mpi3IOCInitRequest_t, MPI3_POINTER pMpi3IOCInitRequest_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_IOCINIT_MSGFLAGS_HOSTMETADATA_MASK          (0x03)
#define MPI3_IOCINIT_MSGFLAGS_HOSTMETADATA_NOT_USED      (0x00)
#define MPI3_IOCINIT_MSGFLAGS_HOSTMETADATA_SEPARATED     (0x01)
#define MPI3_IOCINIT_MSGFLAGS_HOSTMETADATA_INLINE        (0x02)
#define MPI3_IOCINIT_MSGFLAGS_HOSTMETADATA_BOTH          (0x03)

/**** Defines for the WhoInit field ****/
#define MPI3_WHOINIT_NOT_INITIALIZED                     (0x00)
#define MPI3_WHOINIT_ROM_BIOS                            (0x02)
#define MPI3_WHOINIT_HOST_DRIVER                         (0x03)
#define MPI3_WHOINIT_MANUFACTURER                        (0x04)

/**** Defines for the DriverInformationAddress field */
typedef struct _MPI3_DRIVER_INFO_LAYOUT
{
    U32             InformationLength;                  /* 0x00 */
    U8              DriverSignature[12];                /* 0x04 */
    U8              OsName[16];                         /* 0x10 */
    U8              OsVersion[12];                      /* 0x20 */
    U8              DriverName[20];                     /* 0x2C */
    U8              DriverVersion[32];                  /* 0x40 */
    U8              DriverReleaseDate[20];              /* 0x60 */
    U32             DriverCapabilities;                 /* 0x74 */
} MPI3_DRIVER_INFO_LAYOUT, MPI3_POINTER PTR_MPI3_DRIVER_INFO_LAYOUT,
  Mpi3DriverInfoLayout_t, MPI3_POINTER pMpi3DriverInfoLayout_t;

/*****************************************************************************
 *              IOCFacts Request Message                                     *
 ****************************************************************************/
typedef struct _MPI3_IOC_FACTS_REQUEST
{
    U16                 HostTag;                            /* 0x00 */
    U8                  IOCUseOnly02;                       /* 0x02 */
    U8                  Function;                           /* 0x03 */
    U16                 IOCUseOnly04;                       /* 0x04 */
    U8                  IOCUseOnly06;                       /* 0x06 */
    U8                  MsgFlags;                           /* 0x07 */
    U16                 ChangeCount;                        /* 0x08 */
    U16                 Reserved0A;                         /* 0x0A */
    U32                 Reserved0C;                         /* 0x0C */
    MPI3_SGE_UNION      SGL;                                /* 0x10 */
} MPI3_IOC_FACTS_REQUEST, MPI3_POINTER PTR_MPI3_IOC_FACTS_REQUEST,
  Mpi3IOCFactsRequest_t, MPI3_POINTER pMpi3IOCFactsRequest_t;

/*****************************************************************************
 *              IOCFacts Data                                                *
 ****************************************************************************/
typedef struct _MPI3_IOC_FACTS_DATA
{
    U16                     IOCFactsDataLength;                 /* 0x00 */
    U16                     Reserved02;                         /* 0x02 */
    MPI3_VERSION_UNION      MPIVersion;                         /* 0x04 */
    MPI3_COMP_IMAGE_VERSION FWVersion;                          /* 0x08 */
    U32                     IOCCapabilities;                    /* 0x10 */
    U8                      IOCNumber;                          /* 0x14 */
    U8                      WhoInit;                            /* 0x15 */
    U16                     MaxMSIxVectors;                     /* 0x16 */
    U16                     MaxOutstandingRequests;             /* 0x18 */
    U16                     ProductID;                          /* 0x1A */
    U16                     IOCRequestFrameSize;                /* 0x1C */
    U16                     ReplyFrameSize;                     /* 0x1E */
    U16                     IOCExceptions;                      /* 0x20 */
    U16                     MaxPersistentID;                    /* 0x22 */
    U8                      SGEModifierMask;                    /* 0x24 */
    U8                      SGEModifierValue;                   /* 0x25 */
    U8                      SGEModifierShift;                   /* 0x26 */
    U8                      ProtocolFlags;                      /* 0x27 */
    U16                     MaxSASInitiators;                   /* 0x28 */
    U16                     MaxDataLength;                      /* 0x2A */
    U16                     MaxSASExpanders;                    /* 0x2C */
    U16                     MaxEnclosures;                      /* 0x2E */
    U16                     MinDevHandle;                       /* 0x30 */
    U16                     MaxDevHandle;                       /* 0x32 */
    U16                     MaxPCIeSwitches;                    /* 0x34 */
    U16                     MaxNVMe;                            /* 0x36 */
    U16                     Reserved38;                         /* 0x38 */
    U16                     MaxVDs;                             /* 0x3A */
    U16                     MaxHostPDs;                         /* 0x3C */
    U16                     MaxAdvHostPDs;                      /* 0x3E */
    U16                     MaxRAIDPDs;                         /* 0x40 */
    U16                     MaxPostedCmdBuffers;                /* 0x42 */
    U32                     Flags;                              /* 0x44 */
    U16                     MaxOperationalRequestQueues;        /* 0x48 */
    U16                     MaxOperationalReplyQueues;          /* 0x4A */
    U16                     ShutdownTimeout;                    /* 0x4C */
    U16                     Reserved4E;                         /* 0x4E */
    U32                     DiagTraceSize;                      /* 0x50 */
    U32                     DiagFwSize;                         /* 0x54 */
    U32                     DiagDriverSize;                     /* 0x58 */
    U8                      MaxHostPDNsCount;                   /* 0x5C */
    U8                      MaxAdvHostPDNsCount;                /* 0x5D */
    U8                      MaxRAIDPDNsCount;                   /* 0x5E */
    U8                      MaxDevicesPerThrottleGroup;         /* 0x5F */
    U16                     IOThrottleDataLength;               /* 0x60 */
    U16                     MaxIOThrottleGroup;                 /* 0x62 */
    U16                     IOThrottleLow;                      /* 0x64 */
    U16                     IOThrottleHigh;                     /* 0x66 */
} MPI3_IOC_FACTS_DATA, MPI3_POINTER PTR_MPI3_IOC_FACTS_DATA,
  Mpi3IOCFactsData_t, MPI3_POINTER pMpi3IOCFactsData_t;

/**** Defines for the IOCCapabilities field ****/
#define MPI3_IOCFACTS_CAPABILITY_NON_SUPERVISOR_MASK          (0x80000000)
#define MPI3_IOCFACTS_CAPABILITY_SUPERVISOR_IOC               (0x00000000)
#define MPI3_IOCFACTS_CAPABILITY_NON_SUPERVISOR_IOC           (0x80000000)
#define MPI3_IOCFACTS_CAPABILITY_INT_COALESCE_MASK            (0x00000600)
#define MPI3_IOCFACTS_CAPABILITY_INT_COALESCE_FIXED_THRESHOLD (0x00000000)
#define MPI3_IOCFACTS_CAPABILITY_INT_COALESCE_OUTSTANDING_IO  (0x00000200)
#define MPI3_IOCFACTS_CAPABILITY_COMPLETE_RESET_CAPABLE       (0x00000100)
#define MPI3_IOCFACTS_CAPABILITY_SEG_DIAG_TRACE_ENABLED       (0x00000080)
#define MPI3_IOCFACTS_CAPABILITY_SEG_DIAG_FW_ENABLED          (0x00000040)
#define MPI3_IOCFACTS_CAPABILITY_SEG_DIAG_DRIVER_ENABLED      (0x00000020)
#define MPI3_IOCFACTS_CAPABILITY_ADVANCED_HOST_PD_ENABLED     (0x00000010)
#define MPI3_IOCFACTS_CAPABILITY_RAID_CAPABLE                 (0x00000008)
#define MPI3_IOCFACTS_CAPABILITY_MULTIPATH_ENABLED            (0x00000002)
#define MPI3_IOCFACTS_CAPABILITY_COALESCE_CTRL_SUPPORTED      (0x00000001)

/**** WhoInit values are defined under IOCInit Request Message definition ****/

/**** Defines for the ProductID field ****/
#define MPI3_IOCFACTS_PID_TYPE_MASK                           (0xF000)
#define MPI3_IOCFACTS_PID_TYPE_SHIFT                          (12)
#define MPI3_IOCFACTS_PID_PRODUCT_MASK                        (0x0F00)
#define MPI3_IOCFACTS_PID_PRODUCT_SHIFT                       (8)
#define MPI3_IOCFACTS_PID_FAMILY_MASK                         (0x00FF)
#define MPI3_IOCFACTS_PID_FAMILY_SHIFT                        (0)

/**** Defines for the IOCExceptions field ****/
#define MPI3_IOCFACTS_EXCEPT_SECURITY_REKEY                   (0x2000)
#define MPI3_IOCFACTS_EXCEPT_SAS_DISABLED                     (0x1000)
#define MPI3_IOCFACTS_EXCEPT_SAFE_MODE                        (0x0800)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_MASK                (0x0700)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_NONE                (0x0000)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_LOCAL_VIA_MGMT      (0x0100)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_EXT_VIA_MGMT        (0x0200)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_DRIVE_EXT_VIA_MGMT  (0x0300)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_LOCAL_VIA_OOB       (0x0400)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_EXT_VIA_OOB         (0x0500)
#define MPI3_IOCFACTS_EXCEPT_SECURITY_KEY_DRIVE_EXT_VIA_OOB   (0x0600)
#define MPI3_IOCFACTS_EXCEPT_PCIE_DISABLED                    (0x0080)
#define MPI3_IOCFACTS_EXCEPT_PARTIAL_MEMORY_FAILURE           (0x0040)
#define MPI3_IOCFACTS_EXCEPT_MANUFACT_CHECKSUM_FAIL           (0x0020)
#define MPI3_IOCFACTS_EXCEPT_FW_CHECKSUM_FAIL                 (0x0010)
#define MPI3_IOCFACTS_EXCEPT_CONFIG_CHECKSUM_FAIL             (0x0008)
#define MPI3_IOCFACTS_EXCEPT_BOOTSTAT_MASK                    (0x0001)
#define MPI3_IOCFACTS_EXCEPT_BOOTSTAT_PRIMARY                 (0x0000)
#define MPI3_IOCFACTS_EXCEPT_BOOTSTAT_SECONDARY               (0x0001)

/**** Defines for the ProtocolFlags field ****/
#define MPI3_IOCFACTS_PROTOCOL_SAS                            (0x0010)
#define MPI3_IOCFACTS_PROTOCOL_SATA                           (0x0008)
#define MPI3_IOCFACTS_PROTOCOL_NVME                           (0x0004)
#define MPI3_IOCFACTS_PROTOCOL_SCSI_INITIATOR                 (0x0002)
#define MPI3_IOCFACTS_PROTOCOL_SCSI_TARGET                    (0x0001)

/**** Defines for the MaxDataLength field ****/
#define MPI3_IOCFACTS_MAX_DATA_LENGTH_NOT_REPORTED            (0x0000)

/**** Defines for the Flags field ****/
#define MPI3_IOCFACTS_FLAGS_SIGNED_NVDATA_REQUIRED            (0x00010000)
#define MPI3_IOCFACTS_FLAGS_DMA_ADDRESS_WIDTH_MASK            (0x0000FF00)
#define MPI3_IOCFACTS_FLAGS_DMA_ADDRESS_WIDTH_SHIFT           (8)
#define MPI3_IOCFACTS_FLAGS_INITIAL_PORT_ENABLE_MASK          (0x00000030)
#define MPI3_IOCFACTS_FLAGS_INITIAL_PORT_ENABLE_NOT_STARTED   (0x00000000)
#define MPI3_IOCFACTS_FLAGS_INITIAL_PORT_ENABLE_IN_PROGRESS   (0x00000010)
#define MPI3_IOCFACTS_FLAGS_INITIAL_PORT_ENABLE_COMPLETE      (0x00000020)
#define MPI3_IOCFACTS_FLAGS_PERSONALITY_MASK                  (0x0000000F)
#define MPI3_IOCFACTS_FLAGS_PERSONALITY_EHBA                  (0x00000000)
#define MPI3_IOCFACTS_FLAGS_PERSONALITY_RAID_DDR              (0x00000002)

/**** Defines for the IOThrottleDataLength field ****/
#define MPI3_IOCFACTS_IO_THROTTLE_DATA_LENGTH_NOT_REQUIRED    (0x0000)

/**** Defines for the IOThrottleDataLength field ****/
#define MPI3_IOCFACTS_MAX_IO_THROTTLE_GROUP_NOT_REQUIRED      (0x0000)

/*****************************************************************************
 *              Management Passthrough Request Message                      *
 ****************************************************************************/
typedef struct _MPI3_MGMT_PASSTHROUGH_REQUEST
{
    U16                 HostTag;                        /* 0x00 */
    U8                  IOCUseOnly02;                   /* 0x02 */
    U8                  Function;                       /* 0x03 */
    U16                 IOCUseOnly04;                   /* 0x04 */
    U8                  IOCUseOnly06;                   /* 0x06 */
    U8                  MsgFlags;                       /* 0x07 */
    U16                 ChangeCount;                    /* 0x08 */
    U16                 Reserved0A;                     /* 0x0A */
    U32                 Reserved0C[5];                  /* 0x0C */
    MPI3_SGE_UNION      CommandSGL;                     /* 0x20 */
    MPI3_SGE_UNION      ResponseSGL;                    /* 0x30 */
} MPI3_MGMT_PASSTHROUGH_REQUEST, MPI3_POINTER PTR_MPI3_MGMT_PASSTHROUGH_REQUEST,
  Mpi3MgmtPassthroughRequest_t, MPI3_POINTER pMpi3MgmtPassthroughRequest_t;

/*****************************************************************************
 *              CreateRequestQueue Request Message                        *
 ****************************************************************************/
typedef struct _MPI3_CREATE_REQUEST_QUEUE_REQUEST
{
    U16             HostTag;                            /* 0x00 */
    U8              IOCUseOnly02;                       /* 0x02 */
    U8              Function;                           /* 0x03 */
    U16             IOCUseOnly04;                       /* 0x04 */
    U8              IOCUseOnly06;                       /* 0x06 */
    U8              MsgFlags;                           /* 0x07 */
    U16             ChangeCount;                        /* 0x08 */
    U8              Flags;                              /* 0x0A */
    U8              Burst;                              /* 0x0B */
    U16             Size;                               /* 0x0C */
    U16             QueueID;                            /* 0x0E */
    U16             ReplyQueueID;                       /* 0x10 */
    U16             Reserved12;                         /* 0x12 */
    U32             Reserved14;                         /* 0x14 */
    U64             BaseAddress;                        /* 0x18 */
} MPI3_CREATE_REQUEST_QUEUE_REQUEST, MPI3_POINTER PTR_MPI3_CREATE_REQUEST_QUEUE_REQUEST,
  Mpi3CreateRequestQueueRequest_t, MPI3_POINTER pMpi3CreateRequestQueueRequest_t;

/**** Defines for the Flags field ****/
#define MPI3_CREATE_REQUEST_QUEUE_FLAGS_SEGMENTED_MASK          (0x80)
#define MPI3_CREATE_REQUEST_QUEUE_FLAGS_SEGMENTED_SEGMENTED     (0x80)
#define MPI3_CREATE_REQUEST_QUEUE_FLAGS_SEGMENTED_CONTIGUOUS    (0x00)

/**** Defines for the Size field ****/
#define MPI3_CREATE_REQUEST_QUEUE_SIZE_MINIMUM                  (2)

/*****************************************************************************
 *              DeleteRequestQueue Request Message                        *
 ****************************************************************************/
typedef struct _MPI3_DELETE_REQUEST_QUEUE_REQUEST
{
    U16             HostTag;                            /* 0x00 */
    U8              IOCUseOnly02;                       /* 0x02 */
    U8              Function;                           /* 0x03 */
    U16             IOCUseOnly04;                       /* 0x04 */
    U8              IOCUseOnly06;                       /* 0x06 */
    U8              MsgFlags;                           /* 0x07 */
    U16             ChangeCount;                        /* 0x08 */
    U16             QueueID;                            /* 0x0A */
} MPI3_DELETE_REQUEST_QUEUE_REQUEST, MPI3_POINTER PTR_MPI3_DELETE_REQUEST_QUEUE_REQUEST,
  Mpi3DeleteRequestQueueRequest_t, MPI3_POINTER pMpi3DeleteRequestQueueRequest_t;


/*****************************************************************************
 *              CreateReplyQueue Request Message                          *
 ****************************************************************************/
typedef struct _MPI3_CREATE_REPLY_QUEUE_REQUEST
{
    U16             HostTag;                            /* 0x00 */
    U8              IOCUseOnly02;                       /* 0x02 */
    U8              Function;                           /* 0x03 */
    U16             IOCUseOnly04;                       /* 0x04 */
    U8              IOCUseOnly06;                       /* 0x06 */
    U8              MsgFlags;                           /* 0x07 */
    U16             ChangeCount;                        /* 0x08 */
    U8              Flags;                              /* 0x0A */
    U8              Reserved0B;                         /* 0x0B */
    U16             Size;                               /* 0x0C */
    U16             QueueID;                            /* 0x0E */
    U16             MSIxIndex;                          /* 0x10 */
    U16             Reserved12;                         /* 0x12 */
    U32             Reserved14;                         /* 0x14 */
    U64             BaseAddress;                        /* 0x18 */
} MPI3_CREATE_REPLY_QUEUE_REQUEST, MPI3_POINTER PTR_MPI3_CREATE_REPLY_QUEUE_REQUEST,
  Mpi3CreateReplyQueueRequest_t, MPI3_POINTER pMpi3CreateReplyQueueRequest_t;

/**** Defines for the Flags field ****/
#define MPI3_CREATE_REPLY_QUEUE_FLAGS_SEGMENTED_MASK            (0x80)
#define MPI3_CREATE_REPLY_QUEUE_FLAGS_SEGMENTED_SEGMENTED       (0x80)
#define MPI3_CREATE_REPLY_QUEUE_FLAGS_SEGMENTED_CONTIGUOUS      (0x00)
#define MPI3_CREATE_REPLY_QUEUE_FLAGS_COALESCE_DISABLE          (0x02)
#define MPI3_CREATE_REPLY_QUEUE_FLAGS_INT_ENABLE_MASK           (0x01)
#define MPI3_CREATE_REPLY_QUEUE_FLAGS_INT_ENABLE_DISABLE        (0x00)
#define MPI3_CREATE_REPLY_QUEUE_FLAGS_INT_ENABLE_ENABLE         (0x01)

/**** Defines for the Size field ****/
#define MPI3_CREATE_REPLY_QUEUE_SIZE_MINIMUM                    (2)

/*****************************************************************************
 *              DeleteReplyQueue Request Message                          *
 ****************************************************************************/
typedef struct _MPI3_DELETE_REPLY_QUEUE_REQUEST
{
    U16             HostTag;                            /* 0x00 */
    U8              IOCUseOnly02;                       /* 0x02 */
    U8              Function;                           /* 0x03 */
    U16             IOCUseOnly04;                       /* 0x04 */
    U8              IOCUseOnly06;                       /* 0x06 */
    U8              MsgFlags;                           /* 0x07 */
    U16             ChangeCount;                        /* 0x08 */
    U16             QueueID;                            /* 0x0A */
} MPI3_DELETE_REPLY_QUEUE_REQUEST, MPI3_POINTER PTR_MPI3_DELETE_REPLY_QUEUE_REQUEST,
  Mpi3DeleteReplyQueueRequest_t, MPI3_POINTER pMpi3DeleteReplyQueueRequest_t;


/*****************************************************************************
 *              PortEnable Request Message                                   *
 ****************************************************************************/
typedef struct _MPI3_PORT_ENABLE_REQUEST
{
    U16             HostTag;                            /* 0x00 */
    U8              IOCUseOnly02;                       /* 0x02 */
    U8              Function;                           /* 0x03 */
    U16             IOCUseOnly04;                       /* 0x04 */
    U8              IOCUseOnly06;                       /* 0x06 */
    U8              MsgFlags;                           /* 0x07 */
    U16             ChangeCount;                        /* 0x08 */
    U16             Reserved0A;                         /* 0x0A */
} MPI3_PORT_ENABLE_REQUEST, MPI3_POINTER PTR_MPI3_PORT_ENABLE_REQUEST,
  Mpi3PortEnableRequest_t, MPI3_POINTER pMpi3PortEnableRequest_t;


/*****************************************************************************
 *              IOC Events and Event Management                              *
 ****************************************************************************/
#define MPI3_EVENT_LOG_DATA                         (0x01)
#define MPI3_EVENT_CHANGE                           (0x02)
#define MPI3_EVENT_GPIO_INTERRUPT                   (0x04)
#define MPI3_EVENT_CABLE_MGMT                       (0x06)
#define MPI3_EVENT_DEVICE_ADDED                     (0x07)
#define MPI3_EVENT_DEVICE_INFO_CHANGED              (0x08)
#define MPI3_EVENT_PREPARE_FOR_RESET                (0x09)
#define MPI3_EVENT_COMP_IMAGE_ACT_START             (0x0A)
#define MPI3_EVENT_ENCL_DEVICE_ADDED                (0x0B)
#define MPI3_EVENT_ENCL_DEVICE_STATUS_CHANGE        (0x0C)
#define MPI3_EVENT_DEVICE_STATUS_CHANGE             (0x0D)
#define MPI3_EVENT_ENERGY_PACK_CHANGE               (0x0E)
#define MPI3_EVENT_SAS_DISCOVERY                    (0x11)
#define MPI3_EVENT_SAS_BROADCAST_PRIMITIVE          (0x12)
#define MPI3_EVENT_SAS_NOTIFY_PRIMITIVE             (0x13)
#define MPI3_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE    (0x14)
#define MPI3_EVENT_SAS_INIT_TABLE_OVERFLOW          (0x15)
#define MPI3_EVENT_SAS_TOPOLOGY_CHANGE_LIST         (0x16)
#define MPI3_EVENT_SAS_PHY_COUNTER                  (0x18)
#define MPI3_EVENT_SAS_DEVICE_DISCOVERY_ERROR       (0x19)
#define MPI3_EVENT_PCIE_TOPOLOGY_CHANGE_LIST        (0x20)
#define MPI3_EVENT_PCIE_ENUMERATION                 (0x22)
#define MPI3_EVENT_PCIE_ERROR_THRESHOLD             (0x23)
#define MPI3_EVENT_HARD_RESET_RECEIVED              (0x40)
#define MPI3_EVENT_DIAGNOSTIC_BUFFER_STATUS_CHANGE  (0x50)
#define MPI3_EVENT_MIN_PRODUCT_SPECIFIC             (0x60)
#define MPI3_EVENT_MAX_PRODUCT_SPECIFIC             (0x7F)


/*****************************************************************************
 *              Event Notification Request Message                           *
 ****************************************************************************/
#define MPI3_EVENT_NOTIFY_EVENTMASK_WORDS           (4)

typedef struct _MPI3_EVENT_NOTIFICATION_REQUEST
{
    U16             HostTag;                                            /* 0x00 */
    U8              IOCUseOnly02;                                       /* 0x02 */
    U8              Function;                                           /* 0x03 */
    U16             IOCUseOnly04;                                       /* 0x04 */
    U8              IOCUseOnly06;                                       /* 0x06 */
    U8              MsgFlags;                                           /* 0x07 */
    U16             ChangeCount;                                        /* 0x08 */
    U16             Reserved0A;                                         /* 0x0A */
    U16             SASBroadcastPrimitiveMasks;                         /* 0x0C */
    U16             SASNotifyPrimitiveMasks;                            /* 0x0E */
    U32             EventMasks[MPI3_EVENT_NOTIFY_EVENTMASK_WORDS];      /* 0x10 */
} MPI3_EVENT_NOTIFICATION_REQUEST, MPI3_POINTER PTR_MPI3_EVENT_NOTIFICATION_REQUEST,
  Mpi3EventNotificationRequest_t, MPI3_POINTER pMpi3EventNotificationRequest_t;

/**** Defines for the SASBroadcastPrimitiveMasks field - use MPI3_EVENT_PRIMITIVE_ values ****/

/**** Defines for the SASNotifyPrimitiveMasks field - use MPI3_EVENT_NOTIFY_ values ****/

/**** Defines for the EventMasks field - use MPI3_EVENT_ values ****/

/*****************************************************************************
 *              Event Notification Reply Message                             *
 ****************************************************************************/
typedef struct _MPI3_EVENT_NOTIFICATION_REPLY
{
    U16             HostTag;                /* 0x00 */
    U8              IOCUseOnly02;           /* 0x02 */
    U8              Function;               /* 0x03 */
    U16             IOCUseOnly04;           /* 0x04 */
    U8              IOCUseOnly06;           /* 0x06 */
    U8              MsgFlags;               /* 0x07 */
    U16             IOCUseOnly08;           /* 0x08 */
    U16             IOCStatus;              /* 0x0A */
    U32             IOCLogInfo;             /* 0x0C */
    U8              EventDataLength;        /* 0x10 */
    U8              Event;                  /* 0x11 */
    U16             IOCChangeCount;         /* 0x12 */
    U32             EventContext;           /* 0x14 */
    U32             EventData[1];           /* 0x18 */
} MPI3_EVENT_NOTIFICATION_REPLY, MPI3_POINTER PTR_MPI3_EVENT_NOTIFICATION_REPLY,
  Mpi3EventNotificationReply_t, MPI3_POINTER pMpi3EventNotificationReply_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_EVENT_NOTIFY_MSGFLAGS_ACK_MASK                        (0x01)
#define MPI3_EVENT_NOTIFY_MSGFLAGS_ACK_REQUIRED                    (0x01)
#define MPI3_EVENT_NOTIFY_MSGFLAGS_ACK_NOT_REQUIRED                (0x00)
#define MPI3_EVENT_NOTIFY_MSGFLAGS_EVENT_ORIGINALITY_MASK          (0x02)
#define MPI3_EVENT_NOTIFY_MSGFLAGS_EVENT_ORIGINALITY_ORIGINAL      (0x00)
#define MPI3_EVENT_NOTIFY_MSGFLAGS_EVENT_ORIGINALITY_REPLAY        (0x02)

/**** Defines for the Event field - use MPI3_EVENT_ values ****/


/*****************************************************************************
 *              GPIO Interrupt Event                                         *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_GPIO_INTERRUPT
{
    U8              GPIONum;            /* 0x00 */
    U8              Reserved01[3];      /* 0x01 */
} MPI3_EVENT_DATA_GPIO_INTERRUPT, MPI3_POINTER PTR_MPI3_EVENT_DATA_GPIO_INTERRUPT,
  Mpi3EventDataGpioInterrupt_t, MPI3_POINTER pMpi3EventDataGpioInterrupt_t;


/*****************************************************************************
 *              Cable Management Event                                       *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_CABLE_MANAGEMENT
{
    U32             ActiveCablePowerRequirement;    /* 0x00 */
    U8              Status;                         /* 0x04 */
    U8              ReceptacleID;                   /* 0x05 */
    U16             Reserved06;                     /* 0x06 */
} MPI3_EVENT_DATA_CABLE_MANAGEMENT, MPI3_POINTER PTR_MPI3_EVENT_DATA_CABLE_MANAGEMENT,
  Mpi3EventDataCableManagement_t, MPI3_POINTER pMpi3EventDataCableManagement_t;

/**** Defines for the ActiveCablePowerRequirement field ****/
#define MPI3_EVENT_CABLE_MGMT_ACT_CABLE_PWR_INVALID     (0xFFFFFFFF)

/**** Defines for the Status field ****/
#define MPI3_EVENT_CABLE_MGMT_STATUS_INSUFFICIENT_POWER        (0x00)
#define MPI3_EVENT_CABLE_MGMT_STATUS_PRESENT                   (0x01)
#define MPI3_EVENT_CABLE_MGMT_STATUS_DEGRADED                  (0x02)


/*****************************************************************************
 *              Event Ack Request Message                                    *
 ****************************************************************************/
typedef struct _MPI3_EVENT_ACK_REQUEST
{
    U16             HostTag;            /* 0x00 */
    U8              IOCUseOnly02;       /* 0x02 */
    U8              Function;           /* 0x03 */
    U16             IOCUseOnly04;       /* 0x04 */
    U8              IOCUseOnly06;       /* 0x06 */
    U8              MsgFlags;           /* 0x07 */
    U16             ChangeCount;        /* 0x08 */
    U16             Reserved0A;         /* 0x0A */
    U8              Event;              /* 0x0C */
    U8              Reserved0D[3];      /* 0x0D */
    U32             EventContext;       /* 0x10 */
} MPI3_EVENT_ACK_REQUEST, MPI3_POINTER PTR_MPI3_EVENT_ACK_REQUEST,
  Mpi3EventAckRequest_t, MPI3_POINTER pMpi3EventAckRequest_t;

/**** Defines for the Event field - use MPI3_EVENT_ values ****/


/*****************************************************************************
 *              Prepare for Reset Event                                      *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_PREPARE_FOR_RESET
{
    U8              ReasonCode;         /* 0x00 */
    U8              Reserved01;         /* 0x01 */
    U16             Reserved02;         /* 0x02 */
} MPI3_EVENT_DATA_PREPARE_FOR_RESET, MPI3_POINTER PTR_MPI3_EVENT_DATA_PREPARE_FOR_RESET,
  Mpi3EventDataPrepareForReset_t, MPI3_POINTER pMpi3EventDataPrepareForReset_t;

/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_PREPARE_RESET_RC_START                (0x01)
#define MPI3_EVENT_PREPARE_RESET_RC_ABORT                (0x02)


/*****************************************************************************
 *              Component Image Activation Start Event                       *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_COMP_IMAGE_ACTIVATION
{
    U32            Reserved00;         /* 0x00 */
} MPI3_EVENT_DATA_COMP_IMAGE_ACTIVATION, MPI3_POINTER PTR_MPI3_EVENT_DATA_COMP_IMAGE_ACTIVATION,
  Mpi3EventDataCompImageActivation_t, MPI3_POINTER pMpi3EventDataCompImageActivation_t;

/*****************************************************************************
 *              Device Added Event                                           *
 ****************************************************************************/
/*
 * The Device Added Event Data is exactly the same as Device Page 0 data
 * (including the Configuration Page header). So, please use/refer to
 * MPI3_DEVICE_PAGE0  structure for Device Added Event data.
 */

/****************************************************************************
 *              Device Info Changed Event                                   *
 ****************************************************************************/
/*
 * The Device Info Changed Event Data is exactly the same as Device Page 0 data
 * (including the Configuration Page header). So, please use/refer to
 * MPI3_DEVICE_PAGE0  structure for Device Added Event data.
 */

/*****************************************************************************
 *              Device Status Change Event                                  *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_DEVICE_STATUS_CHANGE
{
    U16             TaskTag;            /* 0x00 */
    U8              ReasonCode;         /* 0x02 */
    U8              IOUnitPort;         /* 0x03 */
    U16             ParentDevHandle;    /* 0x04 */
    U16             DevHandle;          /* 0x06 */
    U64             WWID;               /* 0x08 */
    U8              LUN[8];             /* 0x10 */
} MPI3_EVENT_DATA_DEVICE_STATUS_CHANGE, MPI3_POINTER PTR_MPI3_EVENT_DATA_DEVICE_STATUS_CHANGE,
  Mpi3EventDataDeviceStatusChange_t, MPI3_POINTER pMpi3EventDataDeviceStatusChange_t;

/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_DEV_STAT_RC_MOVED                                (0x01)
#define MPI3_EVENT_DEV_STAT_RC_HIDDEN                               (0x02)
#define MPI3_EVENT_DEV_STAT_RC_NOT_HIDDEN                           (0x03)
#define MPI3_EVENT_DEV_STAT_RC_ASYNC_NOTIFICATION                   (0x04)
#define MPI3_EVENT_DEV_STAT_RC_INT_DEVICE_RESET_STRT                (0x20)
#define MPI3_EVENT_DEV_STAT_RC_INT_DEVICE_RESET_CMP                 (0x21)
#define MPI3_EVENT_DEV_STAT_RC_INT_TASK_ABORT_STRT                  (0x22)
#define MPI3_EVENT_DEV_STAT_RC_INT_TASK_ABORT_CMP                   (0x23)
#define MPI3_EVENT_DEV_STAT_RC_INT_IT_NEXUS_RESET_STRT              (0x24)
#define MPI3_EVENT_DEV_STAT_RC_INT_IT_NEXUS_RESET_CMP               (0x25)
#define MPI3_EVENT_DEV_STAT_RC_PCIE_HOT_RESET_FAILED                (0x30)
#define MPI3_EVENT_DEV_STAT_RC_EXPANDER_REDUCED_FUNC_STRT           (0x40)
#define MPI3_EVENT_DEV_STAT_RC_EXPANDER_REDUCED_FUNC_CMP            (0x41)
#define MPI3_EVENT_DEV_STAT_RC_VD_NOT_RESPONDING                    (0x50)

/*****************************************************************************
 *              Energy Pack Change Event                                    *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_ENERGY_PACK_CHANGE
{
    U32             Reserved00;         /* 0x00 */
    U16             ShutdownTimeout;    /* 0x04 */
    U16             Reserved06;         /* 0x06 */
} MPI3_EVENT_DATA_ENERGY_PACK_CHANGE, MPI3_POINTER PTR_MPI3_EVENT_DATA_ENERGY_PACK_CHANGE,
  Mpi3EventDataEnergyPackChange_t, MPI3_POINTER pMpi3EventDataEnergyPackChange_t;

/*****************************************************************************
 *              SAS Discovery Event                                          *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_SAS_DISCOVERY
{
    U8              Flags;              /* 0x00 */
    U8              ReasonCode;         /* 0x01 */
    U8              IOUnitPort;         /* 0x02 */
    U8              Reserved03;         /* 0x03 */
    U32             DiscoveryStatus;    /* 0x04 */
} MPI3_EVENT_DATA_SAS_DISCOVERY, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_DISCOVERY,
  Mpi3EventDataSasDiscovery_t, MPI3_POINTER pMpi3EventDataSasDiscovery_t;

/**** Defines for the Flags field ****/
#define MPI3_EVENT_SAS_DISC_FLAGS_DEVICE_CHANGE                 (0x02)
#define MPI3_EVENT_SAS_DISC_FLAGS_IN_PROGRESS                   (0x01)

/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_SAS_DISC_RC_STARTED                          (0x01)
#define MPI3_EVENT_SAS_DISC_RC_COMPLETED                        (0x02)

/**** Defines for the DiscoveryStatus field ****/
#define MPI3_SAS_DISC_STATUS_MAX_ENCLOSURES_EXCEED            (0x80000000)
#define MPI3_SAS_DISC_STATUS_MAX_EXPANDERS_EXCEED             (0x40000000)
#define MPI3_SAS_DISC_STATUS_MAX_DEVICES_EXCEED               (0x20000000)
#define MPI3_SAS_DISC_STATUS_MAX_TOPO_PHYS_EXCEED             (0x10000000)
#define MPI3_SAS_DISC_STATUS_INVALID_CEI                      (0x00010000)
#define MPI3_SAS_DISC_STATUS_FECEI_MISMATCH                   (0x00008000)
#define MPI3_SAS_DISC_STATUS_MULTIPLE_DEVICES_IN_SLOT         (0x00004000)
#define MPI3_SAS_DISC_STATUS_NECEI_MISMATCH                   (0x00002000)
#define MPI3_SAS_DISC_STATUS_TOO_MANY_SLOTS                   (0x00001000)
#define MPI3_SAS_DISC_STATUS_EXP_MULTI_SUBTRACTIVE            (0x00000800)
#define MPI3_SAS_DISC_STATUS_MULTI_PORT_DOMAIN                (0x00000400)
#define MPI3_SAS_DISC_STATUS_TABLE_TO_SUBTRACTIVE_LINK        (0x00000200)
#define MPI3_SAS_DISC_STATUS_UNSUPPORTED_DEVICE               (0x00000100)
#define MPI3_SAS_DISC_STATUS_TABLE_LINK                       (0x00000080)
#define MPI3_SAS_DISC_STATUS_SUBTRACTIVE_LINK                 (0x00000040)
#define MPI3_SAS_DISC_STATUS_SMP_CRC_ERROR                    (0x00000020)
#define MPI3_SAS_DISC_STATUS_SMP_FUNCTION_FAILED              (0x00000010)
#define MPI3_SAS_DISC_STATUS_SMP_TIMEOUT                      (0x00000008)
#define MPI3_SAS_DISC_STATUS_MULTIPLE_PORTS                   (0x00000004)
#define MPI3_SAS_DISC_STATUS_INVALID_SAS_ADDRESS              (0x00000002)
#define MPI3_SAS_DISC_STATUS_LOOP_DETECTED                    (0x00000001)


/*****************************************************************************
 *              SAS Broadcast Primitive Event                                *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_SAS_BROADCAST_PRIMITIVE
{
    U8              PhyNum;         /* 0x00 */
    U8              IOUnitPort;     /* 0x01 */
    U8              PortWidth;      /* 0x02 */
    U8              Primitive;      /* 0x03 */
} MPI3_EVENT_DATA_SAS_BROADCAST_PRIMITIVE, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_BROADCAST_PRIMITIVE,
  Mpi3EventDataSasBroadcastPrimitive_t, MPI3_POINTER pMpi3EventDataSasBroadcastPrimitive_t;

/**** Defines for the Primitive field ****/
#define MPI3_EVENT_BROADCAST_PRIMITIVE_CHANGE                 (0x01)
#define MPI3_EVENT_BROADCAST_PRIMITIVE_SES                    (0x02)
#define MPI3_EVENT_BROADCAST_PRIMITIVE_EXPANDER               (0x03)
#define MPI3_EVENT_BROADCAST_PRIMITIVE_ASYNCHRONOUS_EVENT     (0x04)
#define MPI3_EVENT_BROADCAST_PRIMITIVE_RESERVED3              (0x05)
#define MPI3_EVENT_BROADCAST_PRIMITIVE_RESERVED4              (0x06)
#define MPI3_EVENT_BROADCAST_PRIMITIVE_CHANGE0_RESERVED       (0x07)
#define MPI3_EVENT_BROADCAST_PRIMITIVE_CHANGE1_RESERVED       (0x08)


/*****************************************************************************
 *              SAS Notify Primitive Event                                   *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_SAS_NOTIFY_PRIMITIVE
{
    U8              PhyNum;         /* 0x00 */
    U8              IOUnitPort;     /* 0x01 */
    U8              Reserved02;     /* 0x02 */
    U8              Primitive;      /* 0x03 */
} MPI3_EVENT_DATA_SAS_NOTIFY_PRIMITIVE, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_NOTIFY_PRIMITIVE,
  Mpi3EventDataSasNotifyPrimitive_t, MPI3_POINTER pMpi3EventDataSasNotifyPrimitive_t;

/**** Defines for the Primitive field ****/
#define MPI3_EVENT_NOTIFY_PRIMITIVE_ENABLE_SPINUP         (0x01)
#define MPI3_EVENT_NOTIFY_PRIMITIVE_POWER_LOSS_EXPECTED   (0x02)
#define MPI3_EVENT_NOTIFY_PRIMITIVE_RESERVED1             (0x03)
#define MPI3_EVENT_NOTIFY_PRIMITIVE_RESERVED2             (0x04)


/*****************************************************************************
 *              SAS Topology Change List Event                               *
 ****************************************************************************/
#ifndef MPI3_EVENT_SAS_TOPO_PHY_COUNT
#define MPI3_EVENT_SAS_TOPO_PHY_COUNT           (1)
#endif  /* MPI3_EVENT_SAS_TOPO_PHY_COUNT */

typedef struct _MPI3_EVENT_SAS_TOPO_PHY_ENTRY
{
    U16             AttachedDevHandle;      /* 0x00 */
    U8              LinkRate;               /* 0x02 */
    U8              Status;                 /* 0x03 */
} MPI3_EVENT_SAS_TOPO_PHY_ENTRY, MPI3_POINTER PTR_MPI3_EVENT_SAS_TOPO_PHY_ENTRY,
  Mpi3EventSasTopoPhyEntry_t, MPI3_POINTER pMpi3EventSasTopoPhyEntry_t;

/**** Defines for the LinkRate field ****/
#define MPI3_EVENT_SAS_TOPO_LR_CURRENT_MASK                 (0xF0)
#define MPI3_EVENT_SAS_TOPO_LR_CURRENT_SHIFT                (4)
#define MPI3_EVENT_SAS_TOPO_LR_PREV_MASK                    (0x0F)
#define MPI3_EVENT_SAS_TOPO_LR_PREV_SHIFT                   (0)
#define MPI3_EVENT_SAS_TOPO_LR_UNKNOWN_LINK_RATE            (0x00)
#define MPI3_EVENT_SAS_TOPO_LR_PHY_DISABLED                 (0x01)
#define MPI3_EVENT_SAS_TOPO_LR_NEGOTIATION_FAILED           (0x02)
#define MPI3_EVENT_SAS_TOPO_LR_SATA_OOB_COMPLETE            (0x03)
#define MPI3_EVENT_SAS_TOPO_LR_PORT_SELECTOR                (0x04)
#define MPI3_EVENT_SAS_TOPO_LR_SMP_RESET_IN_PROGRESS        (0x05)
#define MPI3_EVENT_SAS_TOPO_LR_UNSUPPORTED_PHY              (0x06)
#define MPI3_EVENT_SAS_TOPO_LR_RATE_6_0                     (0x0A)
#define MPI3_EVENT_SAS_TOPO_LR_RATE_12_0                    (0x0B)
#define MPI3_EVENT_SAS_TOPO_LR_RATE_22_5                    (0x0C)

/**** Defines for the PhyStatus field ****/
#define MPI3_EVENT_SAS_TOPO_PHY_STATUS_MASK                 (0xC0)
#define MPI3_EVENT_SAS_TOPO_PHY_STATUS_SHIFT                (6)
#define MPI3_EVENT_SAS_TOPO_PHY_STATUS_ACCESSIBLE           (0x00)
#define MPI3_EVENT_SAS_TOPO_PHY_STATUS_NO_EXIST             (0x40)
#define MPI3_EVENT_SAS_TOPO_PHY_STATUS_VACANT               (0x80)
#define MPI3_EVENT_SAS_TOPO_PHY_RC_MASK                     (0x0F)
#define MPI3_EVENT_SAS_TOPO_PHY_RC_TARG_NOT_RESPONDING      (0x02)
#define MPI3_EVENT_SAS_TOPO_PHY_RC_PHY_CHANGED              (0x03)
#define MPI3_EVENT_SAS_TOPO_PHY_RC_NO_CHANGE                (0x04)
#define MPI3_EVENT_SAS_TOPO_PHY_RC_DELAY_NOT_RESPONDING     (0x05)
#define MPI3_EVENT_SAS_TOPO_PHY_RC_RESPONDING               (0x06)


typedef struct _MPI3_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST
{
    U16                             EnclosureHandle;                            /* 0x00 */
    U16                             ExpanderDevHandle;                          /* 0x02 */
    U8                              NumPhys;                                    /* 0x04 */
    U8                              Reserved05[3];                              /* 0x05 */
    U8                              NumEntries;                                 /* 0x08 */
    U8                              StartPhyNum;                                /* 0x09 */
    U8                              ExpStatus;                                  /* 0x0A */
    U8                              IOUnitPort;                                 /* 0x0B */
    MPI3_EVENT_SAS_TOPO_PHY_ENTRY   PhyEntry[MPI3_EVENT_SAS_TOPO_PHY_COUNT];    /* 0x0C */
} MPI3_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST,
  Mpi3EventDataSasTopologyChangeList_t, MPI3_POINTER pMpi3EventDataSasTopologyChangeList_t;

/**** Defines for the ExpStatus field ****/
#define MPI3_EVENT_SAS_TOPO_ES_NO_EXPANDER              (0x00)
#define MPI3_EVENT_SAS_TOPO_ES_NOT_RESPONDING           (0x02)
#define MPI3_EVENT_SAS_TOPO_ES_RESPONDING               (0x03)
#define MPI3_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING     (0x04)

/*****************************************************************************
 *              SAS PHY Counter Event                                        *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_SAS_PHY_COUNTER
{
    U64             TimeStamp;              /* 0x00 */
    U32             Reserved08;             /* 0x08 */
    U8              PhyEventCode;           /* 0x0C */
    U8              PhyNum;                 /* 0x0D */
    U16             Reserved0E;             /* 0x0E */
    U32             PhyEventInfo;           /* 0x10 */
    U8              CounterType;            /* 0x14 */
    U8              ThresholdWindow;        /* 0x15 */
    U8              TimeUnits;              /* 0x16 */
    U8              Reserved17;             /* 0x17 */
    U32             EventThreshold;         /* 0x18 */
    U16             ThresholdFlags;         /* 0x1C */
    U16             Reserved1E;             /* 0x1E */
} MPI3_EVENT_DATA_SAS_PHY_COUNTER, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_PHY_COUNTER,
  Mpi3EventDataSasPhyCounter_t, MPI3_POINTER pMpi3EventDataSasPhyCounter_t;

/**** Defines for the PhyEventCode field - use MPI3_SASPHY3_EVENT_CODE_ defines ****/

/**** Defines for the CounterType field - use MPI3_SASPHY3_COUNTER_TYPE_ defines ****/

/**** Defines for the TimeUnits field - use MPI3_SASPHY3_TIME_UNITS_ defines ****/

/**** Defines for the ThresholdFlags field - use MPI3_SASPHY3_TFLAGS_ defines ****/


/*****************************************************************************
 *              SAS Device Discovery Error Event                             *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_SAS_DEVICE_DISC_ERR
{
    U16             DevHandle;              /* 0x00 */
    U8              ReasonCode;             /* 0x02 */
    U8              IOUnitPort;             /* 0x03 */
    U32             Reserved04;             /* 0x04 */
    U64             SASAddress;             /* 0x08 */
} MPI3_EVENT_DATA_SAS_DEVICE_DISC_ERR, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_DEVICE_DISC_ERR,
  Mpi3EventDataSasDeviceDiscErr_t, MPI3_POINTER pMpi3EventDataSasDeviceDiscErr_t;

/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_SAS_DISC_ERR_RC_SMP_FAILED          (0x01)
#define MPI3_EVENT_SAS_DISC_ERR_RC_SMP_TIMEOUT         (0x02)

/*****************************************************************************
 *              PCIe Enumeration Event                                       *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_PCIE_ENUMERATION
{
    U8              Flags;                  /* 0x00 */
    U8              ReasonCode;             /* 0x01 */
    U8              IOUnitPort;             /* 0x02 */
    U8              Reserved03;             /* 0x03 */
    U32             EnumerationStatus;      /* 0x04 */
} MPI3_EVENT_DATA_PCIE_ENUMERATION, MPI3_POINTER PTR_MPI3_EVENT_DATA_PCIE_ENUMERATION,
  Mpi3EventDataPcieEnumeration_t, MPI3_POINTER pMpi3EventDataPcieEnumeration_t;

/**** Defines for the Flags field ****/
#define MPI3_EVENT_PCIE_ENUM_FLAGS_DEVICE_CHANGE            (0x02)
#define MPI3_EVENT_PCIE_ENUM_FLAGS_IN_PROGRESS              (0x01)

/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_PCIE_ENUM_RC_STARTED                     (0x01)
#define MPI3_EVENT_PCIE_ENUM_RC_COMPLETED                   (0x02)

/**** Defines for the EnumerationStatus field ****/
#define MPI3_EVENT_PCIE_ENUM_ES_MAX_SWITCH_DEPTH_EXCEED     (0x80000000)
#define MPI3_EVENT_PCIE_ENUM_ES_MAX_SWITCHES_EXCEED         (0x40000000)
#define MPI3_EVENT_PCIE_ENUM_ES_MAX_DEVICES_EXCEED          (0x20000000)
#define MPI3_EVENT_PCIE_ENUM_ES_RESOURCES_EXHAUSTED         (0x10000000)


/*****************************************************************************
 *              PCIe Topology Change List Event                              *
 ****************************************************************************/
#ifndef MPI3_EVENT_PCIE_TOPO_PORT_COUNT
#define MPI3_EVENT_PCIE_TOPO_PORT_COUNT         (1)
#endif  /* MPI3_EVENT_PCIE_TOPO_PORT_COUNT */

typedef struct _MPI3_EVENT_PCIE_TOPO_PORT_ENTRY
{
    U16             AttachedDevHandle;      /* 0x00 */
    U8              PortStatus;             /* 0x02 */
    U8              Reserved03;             /* 0x03 */
    U8              CurrentPortInfo;        /* 0x04 */
    U8              Reserved05;             /* 0x05 */
    U8              PreviousPortInfo;       /* 0x06 */
    U8              Reserved07;             /* 0x07 */
} MPI3_EVENT_PCIE_TOPO_PORT_ENTRY, MPI3_POINTER PTR_MPI3_EVENT_PCIE_TOPO_PORT_ENTRY,
  Mpi3EventPcieTopoPortEntry_t, MPI3_POINTER pMpi3EventPcieTopoPortEntry_t;

/**** Defines for the PortStatus field ****/
#define MPI3_EVENT_PCIE_TOPO_PS_NOT_RESPONDING          (0x02)
#define MPI3_EVENT_PCIE_TOPO_PS_PORT_CHANGED            (0x03)
#define MPI3_EVENT_PCIE_TOPO_PS_NO_CHANGE               (0x04)
#define MPI3_EVENT_PCIE_TOPO_PS_DELAY_NOT_RESPONDING    (0x05)
#define MPI3_EVENT_PCIE_TOPO_PS_RESPONDING              (0x06)

/**** Defines for the CurrentPortInfo and PreviousPortInfo field ****/
#define MPI3_EVENT_PCIE_TOPO_PI_LANES_MASK              (0xF0)
#define MPI3_EVENT_PCIE_TOPO_PI_LANES_UNKNOWN           (0x00)
#define MPI3_EVENT_PCIE_TOPO_PI_LANES_1                 (0x10)
#define MPI3_EVENT_PCIE_TOPO_PI_LANES_2                 (0x20)
#define MPI3_EVENT_PCIE_TOPO_PI_LANES_4                 (0x30)
#define MPI3_EVENT_PCIE_TOPO_PI_LANES_8                 (0x40)
#define MPI3_EVENT_PCIE_TOPO_PI_LANES_16                (0x50)

#define MPI3_EVENT_PCIE_TOPO_PI_RATE_MASK               (0x0F)
#define MPI3_EVENT_PCIE_TOPO_PI_RATE_UNKNOWN            (0x00)
#define MPI3_EVENT_PCIE_TOPO_PI_RATE_DISABLED           (0x01)
#define MPI3_EVENT_PCIE_TOPO_PI_RATE_2_5                (0x02)
#define MPI3_EVENT_PCIE_TOPO_PI_RATE_5_0                (0x03)
#define MPI3_EVENT_PCIE_TOPO_PI_RATE_8_0                (0x04)
#define MPI3_EVENT_PCIE_TOPO_PI_RATE_16_0               (0x05)
#define MPI3_EVENT_PCIE_TOPO_PI_RATE_32_0               (0x06)

typedef struct _MPI3_EVENT_DATA_PCIE_TOPOLOGY_CHANGE_LIST
{
    U16                                 EnclosureHandle;                                /* 0x00 */
    U16                                 SwitchDevHandle;                                /* 0x02 */
    U8                                  NumPorts;                                       /* 0x04 */
    U8                                  Reserved05[3];                                  /* 0x05 */
    U8                                  NumEntries;                                     /* 0x08 */
    U8                                  StartPortNum;                                   /* 0x09 */
    U8                                  SwitchStatus;                                   /* 0x0A */
    U8                                  IOUnitPort;                                     /* 0x0B */
    U32                                 Reserved0C;                                     /* 0x0C */
    MPI3_EVENT_PCIE_TOPO_PORT_ENTRY     PortEntry[MPI3_EVENT_PCIE_TOPO_PORT_COUNT];     /* 0x10 */
} MPI3_EVENT_DATA_PCIE_TOPOLOGY_CHANGE_LIST, MPI3_POINTER PTR_MPI3_EVENT_DATA_PCIE_TOPOLOGY_CHANGE_LIST,
  Mpi3EventDataPcieTopologyChangeList_t, MPI3_POINTER pMpi3EventDataPcieTopologyChangeList_t;

/**** Defines for the SwitchStatus field ****/
#define MPI3_EVENT_PCIE_TOPO_SS_NO_PCIE_SWITCH          (0x00)
#define MPI3_EVENT_PCIE_TOPO_SS_NOT_RESPONDING          (0x02)
#define MPI3_EVENT_PCIE_TOPO_SS_RESPONDING              (0x03)
#define MPI3_EVENT_PCIE_TOPO_SS_DELAY_NOT_RESPONDING    (0x04)

/*****************************************************************************
 *              PCIe Error Threshold Event                                  *
 ****************************************************************************/

typedef struct _MPI3_EVENT_DATA_PCIE_ERROR_THRESHOLD
{
    U64                                 Timestamp;          /* 0x00 */
    U8                                  ReasonCode;         /* 0x08 */
    U8                                  Port;               /* 0x09 */
    U16                                 SwitchDevHandle;    /* 0x0A */
    U8                                  Error;              /* 0x0C */
    U8                                  Action;             /* 0x0D */
    U16                                 ThresholdCount;     /* 0x0E */
    U16                                 AttachedDevHandle;  /* 0x10 */
    U16                                 Reserved12;         /* 0x12 */
    U32                                 Reserved14;         /* 0x14 */
} MPI3_EVENT_DATA_PCIE_ERROR_THRESHOLD, MPI3_POINTER PTR_MPI3_EVENT_DATA_PCIE_ERROR_THRESHOLD,
  Mpi3EventDataPcieErrorThreshold_t, MPI3_POINTER pMpi3EventDataPcieErrorThreshold_t;


/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_PCI_ERROR_RC_THRESHOLD_EXCEEDED          (0x00)
#define MPI3_EVENT_PCI_ERROR_RC_ESCALATION                  (0x01)

/**** Defines for the Error field - use MPI3_PCIEIOUNIT3_ERROR_ values ****/

/**** Defines for the Action field - use MPI3_PCIEIOUNIT3_ACTION_ values ****/

/****************************************************************************
 *              Enclosure Device Added Event                                *
 ****************************************************************************/
/*
 * The Enclosure Device Added Event Data is exactly the same as Enclosure
 *  Page 0 data (including the Configuration Page header). So, please
 *  use/refer to MPI3_ENCLOSURE_PAGE0  structure for Enclosure Device Added
 *  Event data.
 */

/****************************************************************************
 *              Enclosure Device Changed Event                              *
 ****************************************************************************/
/*
 * The Enclosure Device Change Event Data is exactly the same as Enclosure
 *  Page 0 data (including the Configuration Page header). So, please
 *  use/refer to MPI3_ENCLOSURE_PAGE0  structure for Enclosure Device Change
 *  Event data.
 */

/*****************************************************************************
 *              SAS Initiator Device Status Change Event                     *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE
{
    U8              ReasonCode;             /* 0x00 */
    U8              IOUnitPort;             /* 0x01 */
    U16             DevHandle;              /* 0x02 */
    U32             Reserved04;             /* 0x04 */
    U64             SASAddress;             /* 0x08 */
} MPI3_EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_INIT_DEV_STATUS_CHANGE,
  Mpi3EventDataSasInitDevStatusChange_t, MPI3_POINTER pMpi3EventDataSasInitDevStatusChange_t;

/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_SAS_INIT_RC_ADDED                (0x01)
#define MPI3_EVENT_SAS_INIT_RC_NOT_RESPONDING       (0x02)


/*****************************************************************************
 *              SAS Initiator Device Table Overflow Event                    *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_SAS_INIT_TABLE_OVERFLOW
{
    U16             MaxInit;                /* 0x00 */
    U16             CurrentInit;            /* 0x02 */
    U32             Reserved04;             /* 0x04 */
    U64             SASAddress;             /* 0x08 */
} MPI3_EVENT_DATA_SAS_INIT_TABLE_OVERFLOW, MPI3_POINTER PTR_MPI3_EVENT_DATA_SAS_INIT_TABLE_OVERFLOW,
  Mpi3EventDataSasInitTableOverflow_t, MPI3_POINTER pMpi3EventDataSasInitTableOverflow_t;


/*****************************************************************************
 *              Hard Reset Received Event                                    *
 ****************************************************************************/
typedef struct _MPI3_EVENT_DATA_HARD_RESET_RECEIVED
{
    U8              Reserved00;             /* 0x00 */
    U8              IOUnitPort;             /* 0x01 */
    U16             Reserved02;             /* 0x02 */
} MPI3_EVENT_DATA_HARD_RESET_RECEIVED, MPI3_POINTER PTR_MPI3_EVENT_DATA_HARD_RESET_RECEIVED,
  Mpi3EventDataHardResetReceived_t, MPI3_POINTER pMpi3EventDataHardResetReceived_t;


/*****************************************************************************
 *               Diagnostic Tool Events                                      *
 *****************************************************************************/

/*****************************************************************************
 *               Diagnostic Buffer Status Change Event                       *
 *****************************************************************************/
typedef struct _MPI3_EVENT_DATA_DIAG_BUFFER_STATUS_CHANGE
{
    U8              Type;                   /* 0x00 */
    U8              ReasonCode;             /* 0x01 */
    U16             Reserved02;             /* 0x02 */
    U32             Reserved04;             /* 0x04 */
} MPI3_EVENT_DATA_DIAG_BUFFER_STATUS_CHANGE, MPI3_POINTER PTR_MPI3_EVENT_DATA_DIAG_BUFFER_STATUS_CHANGE,
  Mpi3EventDataDiagBufferStatusChange_t, MPI3_POINTER pMpi3EventDataDiagBufferStatusChange_t;

/**** Defines for the Type field - use MPI3_DIAG_BUFFER_TYPE_ values ****/

/**** Defines for the ReasonCode field ****/
#define MPI3_EVENT_DIAG_BUFFER_STATUS_CHANGE_RC_RELEASED             (0x01)
#define MPI3_EVENT_DIAG_BUFFER_STATUS_CHANGE_RC_PAUSED               (0x02)
#define MPI3_EVENT_DIAG_BUFFER_STATUS_CHANGE_RC_RESUMED              (0x03)

/*****************************************************************************
 *              Persistent Event Logs                                       *
 ****************************************************************************/

/**** Definitions for the Locale field ****/
#define MPI3_PEL_LOCALE_FLAGS_NON_BLOCKING_BOOT_EVENT   (0x0200)
#define MPI3_PEL_LOCALE_FLAGS_BLOCKING_BOOT_EVENT       (0x0100)
#define MPI3_PEL_LOCALE_FLAGS_PCIE                      (0x0080)
#define MPI3_PEL_LOCALE_FLAGS_CONFIGURATION             (0x0040)
#define MPI3_PEL_LOCALE_FLAGS_CONTROLER                 (0x0020)
#define MPI3_PEL_LOCALE_FLAGS_SAS                       (0x0010)
#define MPI3_PEL_LOCALE_FLAGS_EPACK                     (0x0008)
#define MPI3_PEL_LOCALE_FLAGS_ENCLOSURE                 (0x0004)
#define MPI3_PEL_LOCALE_FLAGS_PD                        (0x0002)
#define MPI3_PEL_LOCALE_FLAGS_VD                        (0x0001)

/**** Definitions for the Class field ****/
#define MPI3_PEL_CLASS_DEBUG                            (0x00)
#define MPI3_PEL_CLASS_PROGRESS                         (0x01)
#define MPI3_PEL_CLASS_INFORMATIONAL                    (0x02)
#define MPI3_PEL_CLASS_WARNING                          (0x03)
#define MPI3_PEL_CLASS_CRITICAL                         (0x04)
#define MPI3_PEL_CLASS_FATAL                            (0x05)
#define MPI3_PEL_CLASS_FAULT                            (0x06)

/**** Definitions for the ClearType field ****/
#define MPI3_PEL_CLEARTYPE_CLEAR                        (0x00)

/**** Definitions for the WaitTime field ****/
#define MPI3_PEL_WAITTIME_INFINITE_WAIT                 (0x00)

/**** Definitions for the Action field ****/
#define MPI3_PEL_ACTION_GET_SEQNUM                      (0x01)
#define MPI3_PEL_ACTION_MARK_CLEAR                      (0x02)
#define MPI3_PEL_ACTION_GET_LOG                         (0x03)
#define MPI3_PEL_ACTION_GET_COUNT                       (0x04)
#define MPI3_PEL_ACTION_WAIT                            (0x05)
#define MPI3_PEL_ACTION_ABORT                           (0x06)
#define MPI3_PEL_ACTION_GET_PRINT_STRINGS               (0x07)
#define MPI3_PEL_ACTION_ACKNOWLEDGE                     (0x08)

/**** Definitions for the LogStatus field ****/
#define MPI3_PEL_STATUS_SUCCESS                         (0x00)
#define MPI3_PEL_STATUS_NOT_FOUND                       (0x01)
#define MPI3_PEL_STATUS_ABORTED                         (0x02)
#define MPI3_PEL_STATUS_NOT_READY                       (0x03)

/****************************************************************************
 *              PEL Sequence Numbers                                        *
 ****************************************************************************/
typedef struct _MPI3_PEL_SEQ
{
    U32                             Newest;                                   /* 0x00 */
    U32                             Oldest;                                   /* 0x04 */
    U32                             Clear;                                    /* 0x08 */
    U32                             Shutdown;                                 /* 0x0C */
    U32                             Boot;                                     /* 0x10 */
    U32                             LastAcknowledged;                         /* 0x14 */
} MPI3_PEL_SEQ, MPI3_POINTER PTR_MPI3_PEL_SEQ,
  Mpi3PELSeq_t, MPI3_POINTER pMpi3PELSeq_t;

/****************************************************************************
 *              PEL Entry                                                   *
 ****************************************************************************/

typedef struct _MPI3_PEL_ENTRY
{
    U64                             TimeStamp;                                /* 0x00 */
    U32                             SequenceNumber;                           /* 0x08 */
    U16                             LogCode;                                  /* 0x0C */
    U16                             ArgType;                                  /* 0x0E */
    U16                             Locale;                                   /* 0x10 */
    U8                              Class;                                    /* 0x12 */
    U8                              Flags;                                    /* 0x13 */
    U8                              ExtNum;                                   /* 0x14 */
    U8                              NumExts;                                  /* 0x15 */
    U8                              ArgDataSize;                              /* 0x16 */
    U8                              FixedFormatStringsSize;                   /* 0x17 */
    U32                             Reserved18[2];                            /* 0x18 */
    U32                             PELInfo[24];                              /* 0x20 - 0x7F */
} MPI3_PEL_ENTRY, MPI3_POINTER PTR_MPI3_PEL_ENTRY,
  Mpi3PELEntry_t, MPI3_POINTER pMpi3PELEntry_t;


/**** Definitions for the Flags field ****/

#define MPI3_PEL_FLAGS_COMPLETE_RESET_NEEDED                  (0x02)
#define MPI3_PEL_FLAGS_ACK_NEEDED                             (0x01)

/****************************************************************************
 *              PEL Event List                                              *
 ****************************************************************************/
typedef struct _MPI3_PEL_LIST
{
    U32                             LogCount;                                 /* 0x00 */
    U32                             Reserved04;                               /* 0x04 */
    MPI3_PEL_ENTRY                  Entry[1];                                 /* 0x08 */  /* variable length */
} MPI3_PEL_LIST, MPI3_POINTER PTR_MPI3_PEL_LIST,
  Mpi3PELList_t, MPI3_POINTER pMpi3PELList_t;

/****************************************************************************
 *              PEL Count Data                                              *
 ****************************************************************************/
typedef U32 MPI3_PEL_LOG_COUNT, MPI3_POINTER PTR_MPI3_PEL_LOG_COUNT,
            Mpi3PELLogCount_t, MPI3_POINTER pMpi3PELLogCount_t;

/****************************************************************************
 *              PEL Arg Map                                                 *
 ****************************************************************************/
typedef struct _MPI3_PEL_ARG_MAP
{
    U8                              ArgType;                                 /* 0x00 */
    U8                              Length;                                  /* 0x01 */
    U16                             StartLocation;                           /* 0x02 */
} MPI3_PEL_ARG_MAP, MPI3_POINTER PTR_MPI3_PEL_ARG_MAP,
  Mpi3PELArgMap_t, MPI3_POINTER pMpi3PELArgMap_t;

/**** Definitions for the ArgType field ****/
#define MPI3_PEL_ARG_MAP_ARG_TYPE_APPEND_STRING                (0x00)
#define MPI3_PEL_ARG_MAP_ARG_TYPE_INTEGER                      (0x01)
#define MPI3_PEL_ARG_MAP_ARG_TYPE_STRING                       (0x02)
#define MPI3_PEL_ARG_MAP_ARG_TYPE_BIT_FIELD                    (0x03)


/****************************************************************************
 *              PEL Print String                                            *
 ****************************************************************************/
typedef struct _MPI3_PEL_PRINT_STRING
{
    U16                             LogCode;                                  /* 0x00 */
    U16                             StringLength;                             /* 0x02 */
    U8                              NumArgMap;                                /* 0x04 */
    U8                              Reserved05[3];                            /* 0x05 */
    MPI3_PEL_ARG_MAP                ArgMap[1];                                /* 0x08 */  /* variable length */
    /*                              FormatString - offset must be calculated */           /* variable length */
} MPI3_PEL_PRINT_STRING, MPI3_POINTER PTR_MPI3_PEL_PRINT_STRING,
  Mpi3PELPrintString_t, MPI3_POINTER pMpi3PELPrintString_t;

/****************************************************************************
 *              PEL Print String List                                       *
 ****************************************************************************/
typedef struct _MPI3_PEL_PRINT_STRING_LIST
{
    U32                             NumPrintStrings;                           /* 0x00 */
    U32                             ResidualBytesRemain;                       /* 0x04 */
    U32                             Reserved08[2];                             /* 0x08 */
    MPI3_PEL_PRINT_STRING           PrintString[1];                            /* 0x10 */  /* variable length */
} MPI3_PEL_PRINT_STRING_LIST, MPI3_POINTER PTR_MPI3_PEL_PRINT_STRING_LIST,
  Mpi3PELPrintStringList_t, MPI3_POINTER pMpi3PELPrintStringList_t;


/****************************************************************************
 *              PEL Request Msg - generic to allow header decoding          *
 ****************************************************************************/
#ifndef MPI3_PEL_ACTION_SPECIFIC_MAX
#define MPI3_PEL_ACTION_SPECIFIC_MAX               (1)
#endif  /* MPI3_PEL_ACTION_SPECIFIC_MAX */

typedef struct _MPI3_PEL_REQUEST
{
    U16                             HostTag;                                         /* 0x00 */
    U8                              IOCUseOnly02;                                    /* 0x02 */
    U8                              Function;                                        /* 0x03 */
    U16                             IOCUseOnly04;                                    /* 0x04 */
    U8                              IOCUseOnly06;                                    /* 0x06 */
    U8                              MsgFlags;                                        /* 0x07 */
    U16                             ChangeCount;                                     /* 0x08 */
    U8                              Action;                                          /* 0x0A */
    U8                              Reserved0B;                                      /* 0x0B */
    U32                             ActionSpecific[MPI3_PEL_ACTION_SPECIFIC_MAX];    /* 0x0C */  /* variable length */
} MPI3_PEL_REQUEST, MPI3_POINTER PTR_MPI3_PEL_REQUEST,
  Mpi3PELRequest_t, MPI3_POINTER pMpi3PELRequest_t;

/****************************************************************************
 *              PEL ACTION Get Sequence Nembers                             *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_GET_SEQUENCE_NUMBERS
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             Reserved0C[5];                            /* 0x0C */
    MPI3_SGE_UNION                  SGL;                                      /* 0x20 */
} MPI3_PEL_REQ_ACTION_GET_SEQUENCE_NUMBERS, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_GET_SEQUENCE_NUMBERS,
  Mpi3PELReqActionGetSequenceNumbers_t, MPI3_POINTER pMpi3PELReqActionGetSequenceNumbers_t;

/****************************************************************************
 *              PEL ACTION Clear Log                                        *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_CLEAR_LOG_MARKER
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U8                              ClearType;                                /* 0x0C */
    U8                              Reserved0D[3];                            /* 0x0D */
} MPI3_PEL_REQ_ACTION_CLEAR_LOG_MARKER, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_CLEAR_LOG_MARKER,
  Mpi3PELReqActionClearLogMMarker_t, MPI3_POINTER pMpi3PELReqActionClearLogMMarker_t;

/****************************************************************************
 *              PEL ACTION Get Log                                          *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_GET_LOG
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             StartingSequenceNumber;                   /* 0x0C */
    U16                             Locale;                                   /* 0x10 */
    U8                              Class;                                    /* 0x12 */
    U8                              Reserved13;                               /* 0x13 */
    U32                             Reserved14[3];                            /* 0x14 */
    MPI3_SGE_UNION                  SGL;                                      /* 0x20 */
} MPI3_PEL_REQ_ACTION_GET_LOG, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_GET_LOG,
  Mpi3PELReqActionGetLog_t, MPI3_POINTER pMpi3PELReqActionGetLog_t;

/****************************************************************************
 *              PEL ACTION Get Count                                        *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_GET_COUNT
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             StartingSequenceNumber;                   /* 0x0C */
    U16                             Locale;                                   /* 0x10 */
    U8                              Class;                                    /* 0x12 */
    U8                              Reserved13;                               /* 0x13 */
    U32                             Reserved14[3];                            /* 0x14 */
    MPI3_SGE_UNION                  SGL;                                      /* 0x20 */
} MPI3_PEL_REQ_ACTION_GET_COUNT, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_GET_COUNT,
  Mpi3PELReqActionGetCount_t, MPI3_POINTER pMpi3PELReqActionGetCount_t;

/****************************************************************************
 *              PEL ACTION Wait                                             *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_WAIT
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             StartingSequenceNumber;                   /* 0x0C */
    U16                             Locale;                                   /* 0x10 */
    U8                              Class;                                    /* 0x12 */
    U8                              Reserved13;                               /* 0x13 */
    U16                             WaitTime;                                 /* 0x14 */
    U16                             Reserved16;                               /* 0x16 */
    U32                             Reserved18[2];                            /* 0x18 */
} MPI3_PEL_REQ_ACTION_WAIT, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_WAIT,
  Mpi3PELReqActionWait_t, MPI3_POINTER pMpi3PELReqActionWait_t;

/****************************************************************************
 *              PEL ACTION Abort                                            *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_ABORT
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             Reserved0C;                               /* 0x0C */
    U16                             AbortHostTag;                             /* 0x10 */
    U16                             Reserved12;                               /* 0x12 */
    U32                             Reserved14;                               /* 0x14 */
} MPI3_PEL_REQ_ACTION_ABORT, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_ABORT,
  Mpi3PELReqActionAbort_t, MPI3_POINTER pMpi3PELReqActionAbort_t;

/****************************************************************************
 *              PEL ACTION Get Print Strings                                *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_GET_PRINT_STRINGS
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             Reserved0C;                               /* 0x0C */
    U16                             StartLogCode;                             /* 0x10 */
    U16                             Reserved12;                               /* 0x12 */
    U32                             Reserved14[3];                            /* 0x14 */
    MPI3_SGE_UNION                  SGL;                                      /* 0x20 */
} MPI3_PEL_REQ_ACTION_GET_PRINT_STRINGS, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_GET_PRINT_STRINGS,
  Mpi3PELReqActionGetPrintStrings_t, MPI3_POINTER pMpi3PELReqActionGetPrintStrings_t;

/****************************************************************************
 *              PEL ACTION Acknowledge                                      *
 ****************************************************************************/
typedef struct _MPI3_PEL_REQ_ACTION_ACKNOWLEDGE
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             SequenceNumber;                           /* 0x0C */
    U32                             Reserved10;                               /* 0x10 */
} MPI3_PEL_REQ_ACTION_ACKNOWLEDGE, MPI3_POINTER PTR_MPI3_PEL_REQ_ACTION_ACKNOWLEDGE,
  Mpi3PELReqActionAcknowledge_t, MPI3_POINTER pMpi3PELReqActionAcknowledge_t;

/**** Definitions for the MsgFlags field ****/
#define MPI3_PELACKNOWLEDGE_MSGFLAGS_SAFE_MODE_EXIT_MASK                     (0x03)
#define MPI3_PELACKNOWLEDGE_MSGFLAGS_SAFE_MODE_EXIT_NO_GUIDANCE              (0x00)
#define MPI3_PELACKNOWLEDGE_MSGFLAGS_SAFE_MODE_EXIT_CONTINUE_OP              (0x01)
#define MPI3_PELACKNOWLEDGE_MSGFLAGS_SAFE_MODE_EXIT_TRANSITION_TO_FAULT      (0x02)

/****************************************************************************
 *              PEL Reply                                                   *
 ****************************************************************************/
typedef struct _MPI3_PEL_REPLY
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             IOCUseOnly08;                             /* 0x08 */
    U16                             IOCStatus;                                /* 0x0A */
    U32                             IOCLogInfo;                               /* 0x0C */
    U8                              Action;                                   /* 0x10 */
    U8                              Reserved11;                               /* 0x11 */
    U16                             Reserved12;                               /* 0x12 */
    U16                             PELogStatus;                              /* 0x14 */
    U16                             Reserved16;                               /* 0x16 */
    U32                             TransferLength;                           /* 0x18 */
} MPI3_PEL_REPLY, MPI3_POINTER PTR_MPI3_PEL_REPLY,
  Mpi3PELReply_t, MPI3_POINTER pMpi3PELReply_t;


/*****************************************************************************
 *              Component Image Download                                     *
 ****************************************************************************/
typedef struct _MPI3_CI_DOWNLOAD_REQUEST
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Action;                                   /* 0x0A */
    U8                              Reserved0B;                               /* 0x0B */
    U32                             Signature1;                               /* 0x0C */
    U32                             TotalImageSize;                           /* 0x10 */
    U32                             ImageOffset;                              /* 0x14 */
    U32                             SegmentSize;                              /* 0x18 */
    U32                             Reserved1C;                               /* 0x1C */
    MPI3_SGE_UNION                  SGL;                                      /* 0x20 */
} MPI3_CI_DOWNLOAD_REQUEST, MPI3_POINTER PTR_MPI3_CI_DOWNLOAD_REQUEST,
  Mpi3CIDownloadRequest_t,   MPI3_POINTER pMpi3CIDownloadRequest_t;

/**** Definitions for the MsgFlags field ****/
#define MPI3_CI_DOWNLOAD_MSGFLAGS_LAST_SEGMENT                 (0x80)
#define MPI3_CI_DOWNLOAD_MSGFLAGS_FORCE_FMC_ENABLE             (0x40)
#define MPI3_CI_DOWNLOAD_MSGFLAGS_SIGNED_NVDATA                (0x20)
#define MPI3_CI_DOWNLOAD_MSGFLAGS_WRITE_CACHE_FLUSH_MASK       (0x03)
#define MPI3_CI_DOWNLOAD_MSGFLAGS_WRITE_CACHE_FLUSH_FAST       (0x00)
#define MPI3_CI_DOWNLOAD_MSGFLAGS_WRITE_CACHE_FLUSH_MEDIUM     (0x01)
#define MPI3_CI_DOWNLOAD_MSGFLAGS_WRITE_CACHE_FLUSH_SLOW       (0x02)

/**** Definitions for the Action field ****/
#define MPI3_CI_DOWNLOAD_ACTION_DOWNLOAD                       (0x01)
#define MPI3_CI_DOWNLOAD_ACTION_ONLINE_ACTIVATION              (0x02)
#define MPI3_CI_DOWNLOAD_ACTION_OFFLINE_ACTIVATION             (0x03)
#define MPI3_CI_DOWNLOAD_ACTION_GET_STATUS                     (0x04)
#define MPI3_CI_DOWNLOAD_ACTION_CANCEL_OFFLINE_ACTIVATION      (0x05)

typedef struct _MPI3_CI_DOWNLOAD_REPLY
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             IOCUseOnly08;                             /* 0x08 */
    U16                             IOCStatus;                                /* 0x0A */
    U32                             IOCLogInfo;                               /* 0x0C */
    U8                              Flags;                                    /* 0x10 */
    U8                              CacheDirty;                               /* 0x11 */
    U8                              PendingCount;                             /* 0x12 */
    U8                              Reserved13;                               /* 0x13 */
} MPI3_CI_DOWNLOAD_REPLY, MPI3_POINTER PTR_MPI3_CI_DOWNLOAD_REPLY,
  Mpi3CIDownloadReply_t,  MPI3_POINTER pMpi3CIDownloadReply_t;

/**** Definitions for the Flags field ****/
#define MPI3_CI_DOWNLOAD_FLAGS_DOWNLOAD_IN_PROGRESS                  (0x80)
#define MPI3_CI_DOWNLOAD_FLAGS_ACTIVATION_FAILURE                    (0x40)
#define MPI3_CI_DOWNLOAD_FLAGS_OFFLINE_ACTIVATION_REQUIRED           (0x20)
#define MPI3_CI_DOWNLOAD_FLAGS_KEY_UPDATE_PENDING                    (0x10)
#define MPI3_CI_DOWNLOAD_FLAGS_ACTIVATION_STATUS_MASK                (0x0E)
#define MPI3_CI_DOWNLOAD_FLAGS_ACTIVATION_STATUS_NOT_NEEDED          (0x00)
#define MPI3_CI_DOWNLOAD_FLAGS_ACTIVATION_STATUS_AWAITING            (0x02)
#define MPI3_CI_DOWNLOAD_FLAGS_ACTIVATION_STATUS_ONLINE_PENDING      (0x04)
#define MPI3_CI_DOWNLOAD_FLAGS_ACTIVATION_STATUS_OFFLINE_PENDING     (0x06)
#define MPI3_CI_DOWNLOAD_FLAGS_COMPATIBLE                            (0x01)

/*****************************************************************************
 *              Component Image Upload                                       *
 ****************************************************************************/
typedef struct _MPI3_CI_UPLOAD_REQUEST
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U16                             Reserved0A;                               /* 0x0A */
    U32                             Signature1;                               /* 0x0C */
    U32                             Reserved10;                               /* 0x10 */
    U32                             ImageOffset;                              /* 0x14 */
    U32                             SegmentSize;                              /* 0x18 */
    U32                             Reserved1C;                               /* 0x1C */
    MPI3_SGE_UNION                  SGL;                                      /* 0x20 */
} MPI3_CI_UPLOAD_REQUEST, MPI3_POINTER PTR_MPI3_CI_UPLOAD_REQUEST,
  Mpi3CIUploadRequest_t,   MPI3_POINTER pMpi3CIUploadRequest_t;

/**** Defines for the MsgFlags field ****/
#define MPI3_CI_UPLOAD_MSGFLAGS_LOCATION_MASK                        (0x01)
#define MPI3_CI_UPLOAD_MSGFLAGS_LOCATION_PRIMARY                     (0x00)
#define MPI3_CI_UPLOAD_MSGFLAGS_LOCATION_SECONDARY                   (0x01)
#define MPI3_CI_UPLOAD_MSGFLAGS_FORMAT_MASK                          (0x02)
#define MPI3_CI_UPLOAD_MSGFLAGS_FORMAT_FLASH                         (0x00)
#define MPI3_CI_UPLOAD_MSGFLAGS_FORMAT_EXECUTABLE                    (0x02)

/**** Defines for Signature1 field - use MPI3_IMAGE_HEADER_SIGNATURE1_ defines */

/*****************************************************************************
 *              IO Unit Control                                              *
 ****************************************************************************/

/**** Definitions for the Operation field ****/
#define MPI3_CTRL_OP_FORCE_FULL_DISCOVERY                            (0x01)
#define MPI3_CTRL_OP_LOOKUP_MAPPING                                  (0x02)
#define MPI3_CTRL_OP_UPDATE_TIMESTAMP                                (0x04)
#define MPI3_CTRL_OP_GET_TIMESTAMP                                   (0x05)
#define MPI3_CTRL_OP_GET_IOC_CHANGE_COUNT                            (0x06)
#define MPI3_CTRL_OP_CHANGE_PROFILE                                  (0x07)
#define MPI3_CTRL_OP_REMOVE_DEVICE                                   (0x10)
#define MPI3_CTRL_OP_CLOSE_PERSISTENT_CONNECTION                     (0x11)
#define MPI3_CTRL_OP_HIDDEN_ACK                                      (0x12)
#define MPI3_CTRL_OP_CLEAR_DEVICE_COUNTERS                           (0x13)
#define MPI3_CTRL_OP_SEND_SAS_PRIMITIVE                              (0x20)
#define MPI3_CTRL_OP_SAS_PHY_CONTROL                                 (0x21)
#define MPI3_CTRL_OP_READ_INTERNAL_BUS                               (0x23)
#define MPI3_CTRL_OP_WRITE_INTERNAL_BUS                              (0x24)
#define MPI3_CTRL_OP_PCIE_LINK_CONTROL                               (0x30)

/**** Depending on the Operation selected, the various ParamX fields         *****/
/****  contain defined data values. These indexes help identify those values *****/
#define MPI3_CTRL_OP_LOOKUP_MAPPING_PARAM8_LOOKUP_METHOD_INDEX       (0x00)
#define MPI3_CTRL_OP_UPDATE_TIMESTAMP_PARAM64_TIMESTAMP_INDEX        (0x00)
#define MPI3_CTRL_OP_CHANGE_PROFILE_PARAM8_PROFILE_ID_INDEX          (0x00)
#define MPI3_CTRL_OP_REMOVE_DEVICE_PARAM16_DEVHANDLE_INDEX           (0x00)
#define MPI3_CTRL_OP_CLOSE_PERSIST_CONN_PARAM16_DEVHANDLE_INDEX      (0x00)
#define MPI3_CTRL_OP_HIDDEN_ACK_PARAM16_DEVHANDLE_INDEX              (0x00)
#define MPI3_CTRL_OP_CLEAR_DEVICE_COUNTERS_PARAM16_DEVHANDLE_INDEX   (0x00)
#define MPI3_CTRL_OP_SEND_SAS_PRIM_PARAM8_PHY_INDEX                  (0x00)
#define MPI3_CTRL_OP_SEND_SAS_PRIM_PARAM8_PRIMSEQ_INDEX              (0x01)
#define MPI3_CTRL_OP_SEND_SAS_PRIM_PARAM32_PRIMITIVE_INDEX           (0x00)
#define MPI3_CTRL_OP_SAS_PHY_CONTROL_PARAM8_ACTION_INDEX             (0x00)
#define MPI3_CTRL_OP_SAS_PHY_CONTROL_PARAM8_PHY_INDEX                (0x01)
#define MPI3_CTRL_OP_READ_INTERNAL_BUS_PARAM64_ADDRESS_INDEX         (0x00)
#define MPI3_CTRL_OP_WRITE_INTERNAL_BUS_PARAM64_ADDRESS_INDEX        (0x00)
#define MPI3_CTRL_OP_WRITE_INTERNAL_BUS_PARAM32_VALUE_INDEX          (0x00)
#define MPI3_CTRL_OP_PCIE_LINK_CONTROL_PARAM8_ACTION_INDEX           (0x00)
#define MPI3_CTRL_OP_PCIE_LINK_CONTROL_PARAM8_LINK_INDEX             (0x01)

/**** Definitions for the LookupMethod field in LOOKUP_MAPPING reqs ****/
#define MPI3_CTRL_LOOKUP_METHOD_WWID_ADDRESS                         (0x01)
#define MPI3_CTRL_LOOKUP_METHOD_ENCLOSURE_SLOT                       (0x02)
#define MPI3_CTRL_LOOKUP_METHOD_SAS_DEVICE_NAME                      (0x03)
#define MPI3_CTRL_LOOKUP_METHOD_PERSISTENT_ID                        (0x04)

/**** Definitions for IoUnitControl Lookup Mapping Method Parameters ****/
#define MPI3_CTRL_LOOKUP_METHOD_WWIDADDR_PARAM16_DEVH_INDEX             (0)
#define MPI3_CTRL_LOOKUP_METHOD_WWIDADDR_PARAM64_WWID_INDEX             (0)
#define MPI3_CTRL_LOOKUP_METHOD_ENCLSLOT_PARAM16_SLOTNUM_INDEX          (0)
#define MPI3_CTRL_LOOKUP_METHOD_ENCLSLOT_PARAM64_ENCLOSURELID_INDEX     (0)
#define MPI3_CTRL_LOOKUP_METHOD_SASDEVNAME_PARAM16_DEVH_INDEX           (0)
#define MPI3_CTRL_LOOKUP_METHOD_SASDEVNAME_PARAM64_DEVNAME_INDEX        (0)
#define MPI3_CTRL_LOOKUP_METHOD_PERSISTID_PARAM16_DEVH_INDEX            (0)
#define MPI3_CTRL_LOOKUP_METHOD_PERSISTID_PARAM16_PERSISTENT_ID_INDEX   (1)

/*** Definitions for IoUnitControl Reply fields ****/
#define MPI3_CTRL_LOOKUP_METHOD_VALUE16_DEVH_INDEX                      (0)
#define MPI3_CTRL_GET_TIMESTAMP_VALUE64_TIMESTAMP_INDEX                 (0)
#define MPI3_CTRL_GET_IOC_CHANGE_COUNT_VALUE16_CHANGECOUNT_INDEX        (0)
#define MPI3_CTRL_READ_INTERNAL_BUS_VALUE32_VALUE_INDEX                 (0)

/**** Definitions for the PrimSeq field in SEND_SAS_PRIMITIVE reqs ****/
#define MPI3_CTRL_PRIMFLAGS_SINGLE                                   (0x01)
#define MPI3_CTRL_PRIMFLAGS_TRIPLE                                   (0x03)
#define MPI3_CTRL_PRIMFLAGS_REDUNDANT                                (0x06)

/**** Definitions for the Action field in PCIE_LINK_CONTROL  and SAS_PHY_CONTROL reqs ****/
#define MPI3_CTRL_ACTION_NOP                                         (0x00)
#define MPI3_CTRL_ACTION_LINK_RESET                                  (0x01)
#define MPI3_CTRL_ACTION_HARD_RESET                                  (0x02)
#define MPI3_CTRL_ACTION_CLEAR_ERROR_LOG                             (0x05)

typedef struct _MPI3_IOUNIT_CONTROL_REQUEST
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             ChangeCount;                              /* 0x08 */
    U8                              Reserved0A;                               /* 0x0A */
    U8                              Operation;                                /* 0x0B */
    U32                             Reserved0C;                               /* 0x0C */
    U64                             Param64[2];                               /* 0x10 */
    U32                             Param32[4];                               /* 0x20 */
    U16                             Param16[4];                               /* 0x30 */
    U8                              Param8[8];                                /* 0x38 */
} MPI3_IOUNIT_CONTROL_REQUEST, MPI3_POINTER PTR_MPI3_IOUNIT_CONTROL_REQUEST,
  Mpi3IoUnitControlRequest_t, MPI3_POINTER pMpi3IoUnitControlRequest_t;


typedef struct _MPI3_IOUNIT_CONTROL_REPLY
{
    U16                             HostTag;                                  /* 0x00 */
    U8                              IOCUseOnly02;                             /* 0x02 */
    U8                              Function;                                 /* 0x03 */
    U16                             IOCUseOnly04;                             /* 0x04 */
    U8                              IOCUseOnly06;                             /* 0x06 */
    U8                              MsgFlags;                                 /* 0x07 */
    U16                             IOCUseOnly08;                             /* 0x08 */
    U16                             IOCStatus;                                /* 0x0A */
    U32                             IOCLogInfo;                               /* 0x0C */
    U64                             Value64[2];                               /* 0x10 */
    U32                             Value32[4];                               /* 0x20 */
    U16                             Value16[4];                               /* 0x30 */
    U8                              Value8[8];                                /* 0x38 */
} MPI3_IOUNIT_CONTROL_REPLY, MPI3_POINTER PTR_MPI3_IOUNIT_CONTROL_REPLY,
  Mpi3IoUnitControlReply_t, MPI3_POINTER pMpi3IoUnitControlReply_t;

#endif  /* MPI30_IOC_H */


