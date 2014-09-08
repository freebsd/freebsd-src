/*
 * Copyright (c) 2014, LSI Corp.
 * All rights reserved.
 * Authors: Marian Choy
 * Support: freebsdraid@lsi.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the <ORGANIZATION> nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies,either expressed or implied, of the FreeBSD Project.
 *
 * Send feedback to: <megaraidfbsd@lsi.com>
 * Mail to: LSI Corporation, 1621 Barber Lane, Milpitas, CA 95035
 *    ATTN: MegaRaid FreeBSD
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef MRSAS_H
#define MRSAS_H

#include <sys/param.h>        /* defines used in kernel.h */
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/kernel.h>       /* types used in module initialization */
#include <sys/conf.h>         /* cdevsw struct */
#include <sys/uio.h>          /* uio struct */
#include <sys/malloc.h>
#include <sys/bus.h>          /* structs, prototypes for pci bus stuff */

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <machine/atomic.h>

#include <dev/pci/pcivar.h>   /* For pci_get macros! */
#include <dev/pci/pcireg.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/taskqueue.h>
#include <sys/poll.h>
#include <sys/selinfo.h>

/*
 * Device IDs and PCI
 */
#define MRSAS_TBOLT          0x005b
#define MRSAS_INVADER        0x005d 
#define MRSAS_FURY           0x005f 
#define MRSAS_PCI_BAR0       0x10
#define MRSAS_PCI_BAR1       0x14
#define MRSAS_PCI_BAR2       0x1C

/*
 * Firmware State Defines 
 */
#define MRSAS_FWSTATE_MAXCMD_MASK    0x0000FFFF 
#define MRSAS_FWSTATE_SGE_MASK       0x00FF0000 
#define MRSAS_FW_STATE_CHNG_INTERRUPT 1 

/*
 * Message Frame Defines
 */
#define MRSAS_SENSE_LEN   96 
#define MRSAS_FUSION_MAX_RESET_TRIES                3

/*
 * Miscellaneous Defines 
 */
#define BYTE_ALIGNMENT        1 
#define MRSAS_MAX_NAME_LENGTH 32  
#define MRSAS_VERSION "06.704.01.01-fbsd"        
#define MRSAS_ULONG_MAX     0xFFFFFFFFFFFFFFFF
#define MRSAS_DEFAULT_TIMEOUT 0x14 //temp 
#define DONE 0
#define MRSAS_PAGE_SIZE       4096
#define MRSAS_RESET_NOTICE_INTERVAL 5
#define MRSAS_IO_TIMEOUT 180000      /* 180 second timeout */
#define MRSAS_LDIO_QUEUE_DEPTH   70  /* 70 percent as default */
#define THRESHOLD_REPLY_COUNT 50

/* 
 Boolean types 
*/
#if (__FreeBSD_version < 901000)
	typedef enum _boolean { false, true } boolean;
#endif
enum err { SUCCESS, FAIL };

MALLOC_DECLARE(M_MRSAS);
SYSCTL_DECL(_hw_mrsas);

#define MRSAS_INFO      (1 << 0)
#define MRSAS_TRACE     (1 << 1)
#define MRSAS_FAULT     (1 << 2)
#define MRSAS_OCR               (1 << 3)
#define MRSAS_TOUT      MRSAS_OCR
#define MRSAS_AEN      (1 << 4)
#define MRSAS_PRL11    (1 << 5)

#define mrsas_dprint(sc, level, msg, args...)       \
do {                                                \
    if (sc->mrsas_debug & level)                    \
        device_printf(sc->mrsas_dev, msg, ##args);  \
} while (0)


/****************************************************************************
 * Raid Context structure which describes MegaRAID specific IO Paramenters
 * This resides at offset 0x60 where the SGL normally starts in MPT IO Frames
 ****************************************************************************/

typedef struct _RAID_CONTEXT {
    u_int8_t      Type:4;             // 0x00
    u_int8_t      nseg:4;             // 0x00
    u_int8_t      resvd0;             // 0x01
    u_int16_t     timeoutValue;       // 0x02 -0x03
    u_int8_t      regLockFlags;       // 0x04
    u_int8_t      resvd1;             // 0x05
    u_int16_t     VirtualDiskTgtId;   // 0x06 -0x07
    u_int64_t     regLockRowLBA;      // 0x08 - 0x0F
    u_int32_t     regLockLength;      // 0x10 - 0x13
    u_int16_t     nextLMId;           // 0x14 - 0x15
    u_int8_t      exStatus;           // 0x16
    u_int8_t      status;             // 0x17 status
    u_int8_t      RAIDFlags;  // 0x18 resvd[7:6],ioSubType[5:4],resvd[3:1],preferredCpu[0]
    u_int8_t      numSGE;        // 0x19 numSge; not including chain entries 
    u_int16_t     configSeqNum;   // 0x1A -0x1B
    u_int8_t      spanArm;            // 0x1C span[7:5], arm[4:0] 
    u_int8_t      resvd2[3];          // 0x1D-0x1f 
} RAID_CONTEXT;


/*************************************************************************
 * MPI2 Defines
 ************************************************************************/

#define MPI2_FUNCTION_IOC_INIT              (0x02) /* IOC Init */
#define MPI2_WHOINIT_HOST_DRIVER            (0x04)
#define MPI2_VERSION_MAJOR                  (0x02)
#define MPI2_VERSION_MINOR                  (0x00)
#define MPI2_VERSION_MAJOR_MASK             (0xFF00)
#define MPI2_VERSION_MAJOR_SHIFT            (8)
#define MPI2_VERSION_MINOR_MASK             (0x00FF)
#define MPI2_VERSION_MINOR_SHIFT            (0)
#define MPI2_VERSION ((MPI2_VERSION_MAJOR << MPI2_VERSION_MAJOR_SHIFT) | \
                      MPI2_VERSION_MINOR)
#define MPI2_HEADER_VERSION_UNIT            (0x10)
#define MPI2_HEADER_VERSION_DEV             (0x00)
#define MPI2_HEADER_VERSION_UNIT_MASK       (0xFF00)
#define MPI2_HEADER_VERSION_UNIT_SHIFT      (8)
#define MPI2_HEADER_VERSION_DEV_MASK        (0x00FF)
#define MPI2_HEADER_VERSION_DEV_SHIFT       (0)
#define MPI2_HEADER_VERSION ((MPI2_HEADER_VERSION_UNIT << 8) | MPI2_HEADER_VERSION_DEV)
#define MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR      (0x03)
#define MPI2_SCSIIO_EEDPFLAGS_INC_PRI_REFTAG    (0x8000)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_REFTAG      (0x0400)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_REMOVE_OP   (0x0003)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_APPTAG      (0x0200)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD       (0x0100)
#define MPI2_SCSIIO_EEDPFLAGS_INSERT_OP         (0x0004)
#define MPI2_FUNCTION_SCSI_IO_REQUEST           (0x00) /* SCSI IO */
#define MPI2_REQ_DESCRIPT_FLAGS_HIGH_PRIORITY   (0x06)
#define MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO         (0x00)
#define MPI2_SGE_FLAGS_64_BIT_ADDRESSING        (0x02)
#define MPI2_SCSIIO_CONTROL_WRITE               (0x01000000)
#define MPI2_SCSIIO_CONTROL_READ                (0x02000000)
#define MPI2_REQ_DESCRIPT_FLAGS_TYPE_MASK       (0x0E)
#define MPI2_RPY_DESCRIPT_FLAGS_UNUSED          (0x0F)
#define MPI2_RPY_DESCRIPT_FLAGS_SCSI_IO_SUCCESS (0x00)
#define MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK       (0x0F)
#define MPI2_WRSEQ_FLUSH_KEY_VALUE              (0x0)
#define MPI2_WRITE_SEQUENCE_OFFSET              (0x00000004)
#define MPI2_WRSEQ_1ST_KEY_VALUE                (0xF)
#define MPI2_WRSEQ_2ND_KEY_VALUE                (0x4)
#define MPI2_WRSEQ_3RD_KEY_VALUE                (0xB)
#define MPI2_WRSEQ_4TH_KEY_VALUE                (0x2)
#define MPI2_WRSEQ_5TH_KEY_VALUE                (0x7)
#define MPI2_WRSEQ_6TH_KEY_VALUE                (0xD)

#ifndef MPI2_POINTER
#define MPI2_POINTER     *
#endif


/***************************************
 * MPI2 Structures
 ***************************************/

typedef struct _MPI25_IEEE_SGE_CHAIN64
{
    u_int64_t                     Address;
    u_int32_t                     Length;
    u_int16_t                     Reserved1;
    u_int8_t                      NextChainOffset;
    u_int8_t                      Flags;
} MPI25_IEEE_SGE_CHAIN64, MPI2_POINTER PTR_MPI25_IEEE_SGE_CHAIN64,
    Mpi25IeeeSgeChain64_t, MPI2_POINTER pMpi25IeeeSgeChain64_t;

typedef struct _MPI2_SGE_SIMPLE_UNION
{
    u_int32_t            FlagsLength;
    union
    {
        u_int32_t        Address32;
        u_int64_t        Address64;
    } u;
} MPI2_SGE_SIMPLE_UNION, MPI2_POINTER PTR_MPI2_SGE_SIMPLE_UNION,
    Mpi2SGESimpleUnion_t, MPI2_POINTER pMpi2SGESimpleUnion_t;

typedef struct
{
    u_int8_t                      CDB[20];                    /* 0x00 */
    u_int32_t                     PrimaryReferenceTag;        /* 0x14 */
    u_int16_t                     PrimaryApplicationTag;      /* 0x18 */
    u_int16_t                     PrimaryApplicationTagMask;  /* 0x1A */
    u_int32_t                     TransferLength;             /* 0x1C */
} MPI2_SCSI_IO_CDB_EEDP32, MPI2_POINTER PTR_MPI2_SCSI_IO_CDB_EEDP32,
    Mpi2ScsiIoCdbEedp32_t, MPI2_POINTER pMpi2ScsiIoCdbEedp32_t;

typedef struct _MPI2_SGE_CHAIN_UNION
{
    u_int16_t                     Length;
    u_int8_t                      NextChainOffset;
    u_int8_t                      Flags;
    union
    {
        u_int32_t                 Address32;
        u_int64_t                 Address64;
    } u;
} MPI2_SGE_CHAIN_UNION, MPI2_POINTER PTR_MPI2_SGE_CHAIN_UNION,
    Mpi2SGEChainUnion_t, MPI2_POINTER pMpi2SGEChainUnion_t;

typedef struct _MPI2_IEEE_SGE_SIMPLE32
{
    u_int32_t                     Address;
    u_int32_t                     FlagsLength;
} MPI2_IEEE_SGE_SIMPLE32, MPI2_POINTER PTR_MPI2_IEEE_SGE_SIMPLE32,
    Mpi2IeeeSgeSimple32_t, MPI2_POINTER pMpi2IeeeSgeSimple32_t;
typedef struct _MPI2_IEEE_SGE_SIMPLE64
{
    u_int64_t                     Address;
    u_int32_t                     Length;
    u_int16_t                     Reserved1;
    u_int8_t                      Reserved2;
    u_int8_t                      Flags;
} MPI2_IEEE_SGE_SIMPLE64, MPI2_POINTER PTR_MPI2_IEEE_SGE_SIMPLE64,
    Mpi2IeeeSgeSimple64_t, MPI2_POINTER pMpi2IeeeSgeSimple64_t;

typedef union _MPI2_IEEE_SGE_SIMPLE_UNION
{
    MPI2_IEEE_SGE_SIMPLE32  Simple32;
    MPI2_IEEE_SGE_SIMPLE64  Simple64;
} MPI2_IEEE_SGE_SIMPLE_UNION, MPI2_POINTER PTR_MPI2_IEEE_SGE_SIMPLE_UNION,
    Mpi2IeeeSgeSimpleUnion_t, MPI2_POINTER pMpi2IeeeSgeSimpleUnion_t;

typedef MPI2_IEEE_SGE_SIMPLE32  MPI2_IEEE_SGE_CHAIN32;
typedef MPI2_IEEE_SGE_SIMPLE64  MPI2_IEEE_SGE_CHAIN64;

typedef union _MPI2_IEEE_SGE_CHAIN_UNION
{
    MPI2_IEEE_SGE_CHAIN32   Chain32;
    MPI2_IEEE_SGE_CHAIN64   Chain64;
} MPI2_IEEE_SGE_CHAIN_UNION, MPI2_POINTER PTR_MPI2_IEEE_SGE_CHAIN_UNION,
    Mpi2IeeeSgeChainUnion_t, MPI2_POINTER pMpi2IeeeSgeChainUnion_t;

typedef union _MPI2_SGE_IO_UNION
{
    MPI2_SGE_SIMPLE_UNION       MpiSimple;
    MPI2_SGE_CHAIN_UNION        MpiChain;
    MPI2_IEEE_SGE_SIMPLE_UNION  IeeeSimple;
    MPI2_IEEE_SGE_CHAIN_UNION   IeeeChain;
} MPI2_SGE_IO_UNION, MPI2_POINTER PTR_MPI2_SGE_IO_UNION,
    Mpi2SGEIOUnion_t, MPI2_POINTER pMpi2SGEIOUnion_t;

typedef union
{
    u_int8_t                      CDB32[32];
    MPI2_SCSI_IO_CDB_EEDP32 EEDP32;
    MPI2_SGE_SIMPLE_UNION   SGE;
} MPI2_SCSI_IO_CDB_UNION, MPI2_POINTER PTR_MPI2_SCSI_IO_CDB_UNION,
    Mpi2ScsiIoCdb_t, MPI2_POINTER pMpi2ScsiIoCdb_t;

/*
 * RAID SCSI IO Request Message
 * Total SGE count will be one less than  _MPI2_SCSI_IO_REQUEST
 */
typedef struct _MPI2_RAID_SCSI_IO_REQUEST
{
    u_int16_t                     DevHandle;                      /* 0x00 */
    u_int8_t                      ChainOffset;                    /* 0x02 */
    u_int8_t                      Function;                       /* 0x03 */
    u_int16_t                     Reserved1;                      /* 0x04 */
    u_int8_t                      Reserved2;                      /* 0x06 */
    u_int8_t                      MsgFlags;                       /* 0x07 */
    u_int8_t                      VP_ID;                          /* 0x08 */
    u_int8_t                      VF_ID;                          /* 0x09 */
    u_int16_t                     Reserved3;                      /* 0x0A */
    u_int32_t                     SenseBufferLowAddress;          /* 0x0C */
    u_int16_t                     SGLFlags;                       /* 0x10 */
    u_int8_t                      SenseBufferLength;              /* 0x12 */
    u_int8_t                      Reserved4;                      /* 0x13 */
    u_int8_t                      SGLOffset0;                     /* 0x14 */
    u_int8_t                      SGLOffset1;                     /* 0x15 */
    u_int8_t                      SGLOffset2;                     /* 0x16 */
    u_int8_t                      SGLOffset3;                     /* 0x17 */
    u_int32_t                     SkipCount;                      /* 0x18 */
    u_int32_t                     DataLength;                     /* 0x1C */
    u_int32_t                     BidirectionalDataLength;        /* 0x20 */
    u_int16_t                     IoFlags;                        /* 0x24 */
    u_int16_t                     EEDPFlags;                      /* 0x26 */
    u_int32_t                     EEDPBlockSize;                  /* 0x28 */
    u_int32_t                     SecondaryReferenceTag;          /* 0x2C */
    u_int16_t                     SecondaryApplicationTag;        /* 0x30 */
    u_int16_t                     ApplicationTagTranslationMask;  /* 0x32 */
    u_int8_t                      LUN[8];                         /* 0x34 */
    u_int32_t                     Control;                        /* 0x3C */
    MPI2_SCSI_IO_CDB_UNION  CDB;                            /* 0x40 */
    RAID_CONTEXT            RaidContext;                    /* 0x60 */
    MPI2_SGE_IO_UNION       SGL;                            /* 0x80 */
} MRSAS_RAID_SCSI_IO_REQUEST, MPI2_POINTER PTR_MRSAS_RAID_SCSI_IO_REQUEST,
    MRSASRaidSCSIIORequest_t, MPI2_POINTER pMRSASRaidSCSIIORequest_t;

/*
 * MPT RAID MFA IO Descriptor.
 */
typedef struct _MRSAS_RAID_MFA_IO_DESCRIPTOR {
    u_int32_t     RequestFlags    : 8;
    u_int32_t     MessageAddress1 : 24; /* bits 31:8*/
    u_int32_t     MessageAddress2;      /* bits 61:32 */
} MRSAS_RAID_MFA_IO_REQUEST_DESCRIPTOR,*PMRSAS_RAID_MFA_IO_REQUEST_DESCRIPTOR;

/* Default Request Descriptor */
typedef struct _MPI2_DEFAULT_REQUEST_DESCRIPTOR
{
    u_int8_t              RequestFlags;               /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int16_t             LMID;                       /* 0x04 */
    u_int16_t             DescriptorTypeDependent;    /* 0x06 */
} MPI2_DEFAULT_REQUEST_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_DEFAULT_REQUEST_DESCRIPTOR,
    Mpi2DefaultRequestDescriptor_t, MPI2_POINTER pMpi2DefaultRequestDescriptor_t;
    
/* High Priority Request Descriptor */
typedef struct _MPI2_HIGH_PRIORITY_REQUEST_DESCRIPTOR
{
    u_int8_t              RequestFlags;               /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int16_t             LMID;                       /* 0x04 */
    u_int16_t             Reserved1;                  /* 0x06 */
} MPI2_HIGH_PRIORITY_REQUEST_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_HIGH_PRIORITY_REQUEST_DESCRIPTOR,
    Mpi2HighPriorityRequestDescriptor_t,
    MPI2_POINTER pMpi2HighPriorityRequestDescriptor_t;
    
/* SCSI IO Request Descriptor */
typedef struct _MPI2_SCSI_IO_REQUEST_DESCRIPTOR
{
    u_int8_t              RequestFlags;               /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int16_t             LMID;                       /* 0x04 */
    u_int16_t             DevHandle;                  /* 0x06 */
} MPI2_SCSI_IO_REQUEST_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_SCSI_IO_REQUEST_DESCRIPTOR,
    Mpi2SCSIIORequestDescriptor_t, MPI2_POINTER pMpi2SCSIIORequestDescriptor_t;

/* SCSI Target Request Descriptor */
typedef struct _MPI2_SCSI_TARGET_REQUEST_DESCRIPTOR
{
    u_int8_t              RequestFlags;               /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int16_t             LMID;                       /* 0x04 */
    u_int16_t             IoIndex;                    /* 0x06 */
} MPI2_SCSI_TARGET_REQUEST_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_SCSI_TARGET_REQUEST_DESCRIPTOR,
    Mpi2SCSITargetRequestDescriptor_t,
    MPI2_POINTER pMpi2SCSITargetRequestDescriptor_t;

/* RAID Accelerator Request Descriptor */
typedef struct _MPI2_RAID_ACCEL_REQUEST_DESCRIPTOR
{
    u_int8_t              RequestFlags;               /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int16_t             LMID;                       /* 0x04 */
    u_int16_t             Reserved;                   /* 0x06 */
} MPI2_RAID_ACCEL_REQUEST_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_RAID_ACCEL_REQUEST_DESCRIPTOR,
    Mpi2RAIDAcceleratorRequestDescriptor_t,
    MPI2_POINTER pMpi2RAIDAcceleratorRequestDescriptor_t;

/* union of Request Descriptors */
typedef union _MRSAS_REQUEST_DESCRIPTOR_UNION
{
    MPI2_DEFAULT_REQUEST_DESCRIPTOR             Default;
    MPI2_HIGH_PRIORITY_REQUEST_DESCRIPTOR       HighPriority;
    MPI2_SCSI_IO_REQUEST_DESCRIPTOR             SCSIIO;
    MPI2_SCSI_TARGET_REQUEST_DESCRIPTOR         SCSITarget;
    MPI2_RAID_ACCEL_REQUEST_DESCRIPTOR          RAIDAccelerator;
    MRSAS_RAID_MFA_IO_REQUEST_DESCRIPTOR        MFAIo;
    union {
        struct {
            u_int32_t low;
            u_int32_t high;
        } u;
        u_int64_t Words;
    } addr;
} MRSAS_REQUEST_DESCRIPTOR_UNION;

/* Default Reply Descriptor */
typedef struct _MPI2_DEFAULT_REPLY_DESCRIPTOR
{
    u_int8_t              ReplyFlags;                 /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             DescriptorTypeDependent1;   /* 0x02 */
    u_int32_t             DescriptorTypeDependent2;   /* 0x04 */
} MPI2_DEFAULT_REPLY_DESCRIPTOR, MPI2_POINTER PTR_MPI2_DEFAULT_REPLY_DESCRIPTOR,
    Mpi2DefaultReplyDescriptor_t, MPI2_POINTER pMpi2DefaultReplyDescriptor_t;

/* Address Reply Descriptor */
typedef struct _MPI2_ADDRESS_REPLY_DESCRIPTOR
{
    u_int8_t              ReplyFlags;                 /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int32_t             ReplyFrameAddress;          /* 0x04 */
} MPI2_ADDRESS_REPLY_DESCRIPTOR, MPI2_POINTER PTR_MPI2_ADDRESS_REPLY_DESCRIPTOR,
    Mpi2AddressReplyDescriptor_t, MPI2_POINTER pMpi2AddressReplyDescriptor_t;

/* SCSI IO Success Reply Descriptor */
typedef struct _MPI2_SCSI_IO_SUCCESS_REPLY_DESCRIPTOR
{
    u_int8_t              ReplyFlags;                 /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int16_t             TaskTag;                    /* 0x04 */
    u_int16_t             Reserved1;                  /* 0x06 */
} MPI2_SCSI_IO_SUCCESS_REPLY_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_SCSI_IO_SUCCESS_REPLY_DESCRIPTOR,
    Mpi2SCSIIOSuccessReplyDescriptor_t,
    MPI2_POINTER pMpi2SCSIIOSuccessReplyDescriptor_t;

/* TargetAssist Success Reply Descriptor */
typedef struct _MPI2_TARGETASSIST_SUCCESS_REPLY_DESCRIPTOR
{
    u_int8_t              ReplyFlags;                 /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int8_t              SequenceNumber;             /* 0x04 */
    u_int8_t              Reserved1;                  /* 0x05 */
    u_int16_t             IoIndex;                    /* 0x06 */
} MPI2_TARGETASSIST_SUCCESS_REPLY_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_TARGETASSIST_SUCCESS_REPLY_DESCRIPTOR,
    Mpi2TargetAssistSuccessReplyDescriptor_t,
    MPI2_POINTER pMpi2TargetAssistSuccessReplyDescriptor_t;

/* Target Command Buffer Reply Descriptor */
typedef struct _MPI2_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR
{
    u_int8_t              ReplyFlags;                 /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int8_t              VP_ID;                      /* 0x02 */
    u_int8_t              Flags;                      /* 0x03 */
    u_int16_t             InitiatorDevHandle;         /* 0x04 */
    u_int16_t             IoIndex;                    /* 0x06 */
} MPI2_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR,
    Mpi2TargetCommandBufferReplyDescriptor_t,
    MPI2_POINTER pMpi2TargetCommandBufferReplyDescriptor_t;

/* RAID Accelerator Success Reply Descriptor */
typedef struct _MPI2_RAID_ACCELERATOR_SUCCESS_REPLY_DESCRIPTOR
{
    u_int8_t              ReplyFlags;                 /* 0x00 */
    u_int8_t              MSIxIndex;                  /* 0x01 */
    u_int16_t             SMID;                       /* 0x02 */
    u_int32_t             Reserved;                   /* 0x04 */
} MPI2_RAID_ACCELERATOR_SUCCESS_REPLY_DESCRIPTOR,
    MPI2_POINTER PTR_MPI2_RAID_ACCELERATOR_SUCCESS_REPLY_DESCRIPTOR,
    Mpi2RAIDAcceleratorSuccessReplyDescriptor_t,
    MPI2_POINTER pMpi2RAIDAcceleratorSuccessReplyDescriptor_t;

/* union of Reply Descriptors */
typedef union _MPI2_REPLY_DESCRIPTORS_UNION
{
    MPI2_DEFAULT_REPLY_DESCRIPTOR                   Default;
    MPI2_ADDRESS_REPLY_DESCRIPTOR                   AddressReply;
    MPI2_SCSI_IO_SUCCESS_REPLY_DESCRIPTOR           SCSIIOSuccess;
    MPI2_TARGETASSIST_SUCCESS_REPLY_DESCRIPTOR      TargetAssistSuccess;
    MPI2_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR     TargetCommandBuffer;
    MPI2_RAID_ACCELERATOR_SUCCESS_REPLY_DESCRIPTOR  RAIDAcceleratorSuccess;
    u_int64_t                                             Words;
} MPI2_REPLY_DESCRIPTORS_UNION, MPI2_POINTER PTR_MPI2_REPLY_DESCRIPTORS_UNION,
    Mpi2ReplyDescriptorsUnion_t, MPI2_POINTER pMpi2ReplyDescriptorsUnion_t;

typedef struct {
    volatile unsigned int val;
} atomic_t;

#define atomic_read(v)  atomic_load_acq_int(&(v)->val)
#define atomic_set(v,i) atomic_store_rel_int(&(v)->val, i)
#define atomic_dec(v)   atomic_fetchadd_int(&(v)->val, -1)
#define atomic_inc(v)   atomic_fetchadd_int(&(v)->val, 1)

/* IOCInit Request message */
typedef struct _MPI2_IOC_INIT_REQUEST
{
    u_int8_t                      WhoInit;                        /* 0x00 */
    u_int8_t                      Reserved1;                      /* 0x01 */
    u_int8_t                      ChainOffset;                    /* 0x02 */
    u_int8_t                      Function;                       /* 0x03 */
    u_int16_t                     Reserved2;                      /* 0x04 */
    u_int8_t                      Reserved3;                      /* 0x06 */
    u_int8_t                      MsgFlags;                       /* 0x07 */
    u_int8_t                      VP_ID;                          /* 0x08 */
    u_int8_t                      VF_ID;                          /* 0x09 */
    u_int16_t                     Reserved4;                      /* 0x0A */
    u_int16_t                     MsgVersion;                     /* 0x0C */
    u_int16_t                     HeaderVersion;                  /* 0x0E */
    u_int32_t                     Reserved5;                      /* 0x10 */
    u_int16_t                     Reserved6;                      /* 0x14 */
    u_int8_t                      Reserved7;                      /* 0x16 */
    u_int8_t                      HostMSIxVectors;                /* 0x17 */
    u_int16_t                     Reserved8;                      /* 0x18 */
    u_int16_t                     SystemRequestFrameSize;         /* 0x1A */
    u_int16_t                     ReplyDescriptorPostQueueDepth;  /* 0x1C */
    u_int16_t                     ReplyFreeQueueDepth;            /* 0x1E */
    u_int32_t                     SenseBufferAddressHigh;         /* 0x20 */
    u_int32_t                     SystemReplyAddressHigh;         /* 0x24 */
    u_int64_t                     SystemRequestFrameBaseAddress;  /* 0x28 */
    u_int64_t                     ReplyDescriptorPostQueueAddress;/* 0x30 */
    u_int64_t                     ReplyFreeQueueAddress;          /* 0x38 */
    u_int64_t                     TimeStamp;                      /* 0x40 */
} MPI2_IOC_INIT_REQUEST, MPI2_POINTER PTR_MPI2_IOC_INIT_REQUEST,
    Mpi2IOCInitRequest_t, MPI2_POINTER pMpi2IOCInitRequest_t;

/*
 * MR private defines
 */
#define MR_PD_INVALID 0xFFFF
#define MAX_SPAN_DEPTH 8
#define MAX_QUAD_DEPTH MAX_SPAN_DEPTH
#define MAX_RAIDMAP_SPAN_DEPTH (MAX_SPAN_DEPTH)
#define MAX_ROW_SIZE 32
#define MAX_RAIDMAP_ROW_SIZE (MAX_ROW_SIZE)
#define MAX_LOGICAL_DRIVES 64
#define MAX_RAIDMAP_LOGICAL_DRIVES (MAX_LOGICAL_DRIVES)
#define MAX_RAIDMAP_VIEWS (MAX_LOGICAL_DRIVES)
#define MAX_ARRAYS 128
#define MAX_RAIDMAP_ARRAYS (MAX_ARRAYS)
#define MAX_PHYSICAL_DEVICES 256
#define MAX_RAIDMAP_PHYSICAL_DEVICES (MAX_PHYSICAL_DEVICES)
#define MR_DCMD_LD_MAP_GET_INFO    0x0300e101   // get the mapping information of this LD


/******************************************************************* 
 * RAID map related structures 
 ********************************************************************/
 
typedef struct _MR_DEV_HANDLE_INFO {
    u_int16_t  curDevHdl;   // the device handle currently used by fw to issue the command.
    u_int8_t   validHandles;      // bitmap of valid device handles.
    u_int8_t   reserved;
    u_int16_t  devHandle[2];      // 0x04 dev handles for all the paths.
} MR_DEV_HANDLE_INFO;    
 
typedef struct _MR_ARRAY_INFO {
    u_int16_t      pd[MAX_RAIDMAP_ROW_SIZE];
} MR_ARRAY_INFO;                       // 0x40, Total Size
 
typedef struct _MR_QUAD_ELEMENT {
    u_int64_t     logStart;                   // 0x00
    u_int64_t     logEnd;                     // 0x08
    u_int64_t     offsetInSpan;               // 0x10
    u_int32_t     diff;                       // 0x18
    u_int32_t     reserved1;                  // 0x1C
} MR_QUAD_ELEMENT;                      // 0x20, Total size
 
typedef struct _MR_SPAN_INFO {
    u_int32_t             noElements;             // 0x00
    u_int32_t             reserved1;              // 0x04
    MR_QUAD_ELEMENT quad[MAX_RAIDMAP_SPAN_DEPTH];   // 0x08
} MR_SPAN_INFO;                             // 0x108, Total size
    
typedef struct _MR_LD_SPAN_ {           // SPAN structure
    u_int64_t      startBlk;            // 0x00, starting block number in array
    u_int64_t      numBlks;             // 0x08, number of blocks
    u_int16_t      arrayRef;            // 0x10, array reference
	u_int8_t       spanRowSize;               // 0x11, span row size
    u_int8_t       spanRowDataSize;           // 0x12, span row data size
    u_int8_t       reserved[4];               // 0x13, reserved
} MR_LD_SPAN;                           // 0x18, Total Size

typedef struct _MR_SPAN_BLOCK_INFO {
    u_int64_t          num_rows;             // number of rows/span
    MR_LD_SPAN   span;                 // 0x08
    MR_SPAN_INFO block_span_info;      // 0x20
} MR_SPAN_BLOCK_INFO;

typedef struct _MR_LD_RAID {
    struct {
        u_int32_t     fpCapable           :1;
        u_int32_t     reserved5           :3;
        u_int32_t     ldPiMode            :4;
        u_int32_t     pdPiMode            :4; // Every Pd has to be same.
        u_int32_t     encryptionType      :8; // FDE or ctlr encryption (MR_LD_ENCRYPTION_TYPE)
        u_int32_t     fpWriteCapable      :1;
        u_int32_t     fpReadCapable       :1;
        u_int32_t     fpWriteAcrossStripe :1;
        u_int32_t     fpReadAcrossStripe  :1;
        u_int32_t     fpNonRWCapable      :1; // TRUE if supporting Non RW IO
        u_int32_t     reserved4           :7;
    } capability;                   // 0x00
    u_int32_t     reserved6;
    u_int64_t     size;             // 0x08, LD size in blocks

    u_int8_t      spanDepth;        // 0x10, Total Number of Spans
    u_int8_t      level;            // 0x11, RAID level
    u_int8_t      stripeShift;      // 0x12, shift-count to get stripe size (0=512, 1=1K, 7=64K, etc.)
    u_int8_t      rowSize;          // 0x13, number of disks in a row

    u_int8_t      rowDataSize;      // 0x14, number of data disks in a row
    u_int8_t      writeMode;        // 0x15, WRITE_THROUGH or WRITE_BACK
    u_int8_t      PRL;              // 0x16, To differentiate between RAID1 and RAID1E
    u_int8_t      SRL;              // 0x17

    u_int16_t     targetId;               // 0x18, ld Target Id.
    u_int8_t      ldState;          // 0x1a, state of ld, state corresponds to MR_LD_STATE
    u_int8_t      regTypeReqOnWrite;// 0x1b, Pre calculate region type requests based on MFC etc..
    u_int8_t      modFactor;        // 0x1c, same as rowSize,
    u_int8_t      regTypeReqOnRead; // 0x1d, region lock type used for read, valid only if regTypeOnReadIsValid=1
    u_int16_t     seqNum;                 // 0x1e, LD sequence number

    struct {
        u_int32_t ldSyncRequired:1;       // This LD requires sync command before completing
        u_int32_t regTypeReqOnReadLsValid:1; // Qualifier for regTypeOnRead
        u_int32_t reserved:30;
    } flags;                        // 0x20

    u_int8_t      LUN[8];           // 0x24, 8 byte LUN field used for SCSI
    u_int8_t      fpIoTimeoutForLd; // 0x2C, timeout value for FP IOs
    u_int8_t      reserved2[3];     // 0x2D
    u_int32_t     logicalBlockLength; // 0x30 Logical block size for the LD
    struct {
        u_int32_t LdPiExp:4;        // 0x34, P_I_EXPONENT for ReadCap 16
        u_int32_t LdLogicalBlockExp:4; // 0x34, LOGICAL BLOCKS PER PHYS BLOCK
        u_int32_t reserved1:24;     // 0x34
    } exponent;
    u_int8_t      reserved3[0x80-0x38]; // 0x38 
} MR_LD_RAID;                       // 0x80, Total Size

typedef struct _MR_LD_SPAN_MAP {
    MR_LD_RAID  ldRaid;                          // 0x00
    u_int8_t    dataArmMap[MAX_RAIDMAP_ROW_SIZE];  // 0x80, needed for GET_ARM() - R0/1/5 only.
    MR_SPAN_BLOCK_INFO  spanBlock[MAX_RAIDMAP_SPAN_DEPTH];  // 0xA0
} MR_LD_SPAN_MAP;                // 0x9E0

typedef struct _MR_FW_RAID_MAP {
    u_int32_t  totalSize;    // total size of this structure, including this field.
    union {
        struct {      // Simple method of version checking variables
            u_int32_t         maxLd;
            u_int32_t         maxSpanDepth;
            u_int32_t         maxRowSize;
            u_int32_t         maxPdCount;
            u_int32_t         maxArrays;
        } validationInfo;
        u_int32_t             version[5];
        u_int32_t             reserved1[5];
    } raid_desc;
    u_int32_t         ldCount;                 // count of lds.
    u_int32_t         Reserved1;
    u_int8_t          ldTgtIdToLd[MAX_RAIDMAP_LOGICAL_DRIVES+MAX_RAIDMAP_VIEWS]; // 0x20
    // This doesn't correspond to
    // FW Ld Tgt Id to LD, but will purge. For example: if tgt Id is 4
    // and FW LD is 2, and there is only one LD, FW will populate the
    // array like this. [0xFF, 0xFF, 0xFF, 0xFF, 0x0,.....]. This is to
    // help reduce the entire strcture size if there are few LDs or
    // driver is looking info for 1 LD only.
    u_int8_t          fpPdIoTimeoutSec;        // timeout value used by driver in FP IOs
    u_int8_t           reserved2[7];
    MR_ARRAY_INFO      arMapInfo[MAX_RAIDMAP_ARRAYS];              // 0x00a8
    MR_DEV_HANDLE_INFO devHndlInfo[MAX_RAIDMAP_PHYSICAL_DEVICES];  // 0x20a8
    MR_LD_SPAN_MAP     ldSpanMap[1]; // 0x28a8-[0-MAX_RAIDMAP_LOGICAL_DRIVES+MAX_RAIDMAP_VIEWS+1];
} MR_FW_RAID_MAP;                            // 0x3288, Total Size

typedef struct _LD_LOAD_BALANCE_INFO
{
    u_int8_t      loadBalanceFlag;
    u_int8_t      reserved1;
    u_int16_t     raid1DevHandle[2];
    atomic_t     scsi_pending_cmds[2];
    u_int64_t     last_accessed_block[2];
} LD_LOAD_BALANCE_INFO, *PLD_LOAD_BALANCE_INFO;

/* SPAN_SET is info caclulated from span info from Raid map per ld */
typedef struct _LD_SPAN_SET {
    u_int64_t  log_start_lba;
    u_int64_t  log_end_lba;
    u_int64_t  span_row_start;
    u_int64_t  span_row_end;
    u_int64_t  data_strip_start;
    u_int64_t  data_strip_end;
    u_int64_t  data_row_start;
    u_int64_t  data_row_end;
    u_int8_t   strip_offset[MAX_SPAN_DEPTH];
    u_int32_t  span_row_data_width;
    u_int32_t  diff;
    u_int32_t  reserved[2];
}LD_SPAN_SET, *PLD_SPAN_SET;

typedef struct LOG_BLOCK_SPAN_INFO {
    LD_SPAN_SET  span_set[MAX_SPAN_DEPTH];
}LD_SPAN_INFO, *PLD_SPAN_INFO;

#pragma pack(1)
typedef struct _MR_FW_RAID_MAP_ALL {
    MR_FW_RAID_MAP raidMap;
    MR_LD_SPAN_MAP ldSpanMap[MAX_LOGICAL_DRIVES - 1];
} MR_FW_RAID_MAP_ALL;
#pragma pack()

struct IO_REQUEST_INFO {
    u_int64_t ldStartBlock;
    u_int32_t numBlocks;
    u_int16_t ldTgtId;
    u_int8_t isRead;
    u_int16_t devHandle;
    u_int64_t pdBlock;
    u_int8_t fpOkForIo;
	u_int8_t IoforUnevenSpan;
    u_int8_t start_span;
    u_int8_t reserved;
    u_int64_t start_row;
};

typedef struct _MR_LD_TARGET_SYNC {
    u_int8_t  targetId;
    u_int8_t  reserved;
    u_int16_t seqNum;
} MR_LD_TARGET_SYNC;

#define IEEE_SGE_FLAGS_ADDR_MASK            (0x03)
#define IEEE_SGE_FLAGS_SYSTEM_ADDR          (0x00)
#define IEEE_SGE_FLAGS_IOCDDR_ADDR          (0x01)
#define IEEE_SGE_FLAGS_IOCPLB_ADDR          (0x02)
#define IEEE_SGE_FLAGS_IOCPLBNTA_ADDR       (0x03)
#define IEEE_SGE_FLAGS_CHAIN_ELEMENT        (0x80)
#define IEEE_SGE_FLAGS_END_OF_LIST          (0x40)

union desc_value {
    u_int64_t word;
    struct {
        u_int32_t low;
        u_int32_t high;
    } u;
};

/******************************************************************* 
 * Temporary command 
 ********************************************************************/
struct mrsas_tmp_dcmd {
    bus_dma_tag_t      tmp_dcmd_tag;    // tag for tmp DMCD cmd  
    bus_dmamap_t       tmp_dcmd_dmamap; // dmamap for tmp DCMD cmd 
    void               *tmp_dcmd_mem;   // virtual addr of tmp DCMD cmd 
    bus_addr_t         tmp_dcmd_phys_addr; //physical addr of tmp DCMD 
};

/******************************************************************* 
 * Register set, included legacy controllers 1068 and 1078, 
 * structure extended for 1078 registers
 ********************************************************************/
#pragma pack(1)
typedef struct _mrsas_register_set {
    u_int32_t     doorbell;                       /*0000h*/
    u_int32_t     fusion_seq_offset;              /*0004h*/
    u_int32_t     fusion_host_diag;               /*0008h*/
    u_int32_t     reserved_01;                    /*000Ch*/

    u_int32_t     inbound_msg_0;                  /*0010h*/
    u_int32_t     inbound_msg_1;                  /*0014h*/
    u_int32_t     outbound_msg_0;                 /*0018h*/
    u_int32_t     outbound_msg_1;                 /*001Ch*/

    u_int32_t     inbound_doorbell;               /*0020h*/
    u_int32_t     inbound_intr_status;            /*0024h*/
    u_int32_t     inbound_intr_mask;              /*0028h*/

    u_int32_t     outbound_doorbell;              /*002Ch*/
    u_int32_t     outbound_intr_status;           /*0030h*/
    u_int32_t     outbound_intr_mask;             /*0034h*/

    u_int32_t     reserved_1[2];                  /*0038h*/

    u_int32_t     inbound_queue_port;             /*0040h*/
    u_int32_t     outbound_queue_port;            /*0044h*/

    u_int32_t     reserved_2[9];                  /*0048h*/
    u_int32_t     reply_post_host_index;          /*006Ch*/
    u_int32_t     reserved_2_2[12];               /*0070h*/

    u_int32_t     outbound_doorbell_clear;        /*00A0h*/

    u_int32_t     reserved_3[3];                  /*00A4h*/

    u_int32_t     outbound_scratch_pad ;          /*00B0h*/
    u_int32_t     outbound_scratch_pad_2;         /*00B4h*/

    u_int32_t     reserved_4[2];                  /*00B8h*/

    u_int32_t     inbound_low_queue_port ;        /*00C0h*/

    u_int32_t     inbound_high_queue_port ;       /*00C4h*/

    u_int32_t     reserved_5;                     /*00C8h*/
    u_int32_t         res_6[11];                  /*CCh*/
    u_int32_t         host_diag;
    u_int32_t         seq_offset;
    u_int32_t     index_registers[807];           /*00CCh*/

} mrsas_reg_set;
#pragma pack()

/*******************************************************************
 * Firmware Interface Defines
 *******************************************************************
 * MFI stands for MegaRAID SAS FW Interface. This is just a moniker
 * for protocol between the software and firmware. Commands are
 * issued using "message frames".
 ******************************************************************/
/*
 * FW posts its state in upper 4 bits of outbound_msg_0 register
 */
#define MFI_STATE_MASK                          0xF0000000
#define MFI_STATE_UNDEFINED                     0x00000000
#define MFI_STATE_BB_INIT                       0x10000000
#define MFI_STATE_FW_INIT                       0x40000000
#define MFI_STATE_WAIT_HANDSHAKE                0x60000000
#define MFI_STATE_FW_INIT_2                     0x70000000
#define MFI_STATE_DEVICE_SCAN                   0x80000000
#define MFI_STATE_BOOT_MESSAGE_PENDING          0x90000000
#define MFI_STATE_FLUSH_CACHE                   0xA0000000
#define MFI_STATE_READY                         0xB0000000
#define MFI_STATE_OPERATIONAL                   0xC0000000
#define MFI_STATE_FAULT                         0xF0000000
#define MFI_RESET_REQUIRED                      0x00000001
#define MFI_RESET_ADAPTER                       0x00000002
#define MEGAMFI_FRAME_SIZE                      64
#define MRSAS_MFI_FRAME_SIZE                    1024 
#define MRSAS_MFI_SENSE_SIZE                    128 

/*
 * During FW init, clear pending cmds & reset state using inbound_msg_0
 *
 * ABORT        : Abort all pending cmds
 * READY        : Move from OPERATIONAL to READY state; discard queue info
 * MFIMODE      : Discard (possible) low MFA posted in 64-bit mode (??)
 * CLR_HANDSHAKE: FW is waiting for HANDSHAKE from BIOS or Driver
 * HOTPLUG      : Resume from Hotplug
 * MFI_STOP_ADP : Send signal to FW to stop processing
 */

#define WRITE_SEQUENCE_OFFSET           (0x0000000FC) // I20
#define HOST_DIAGNOSTIC_OFFSET          (0x000000F8)  // I20
#define DIAG_WRITE_ENABLE                       (0x00000080)
#define DIAG_RESET_ADAPTER                      (0x00000004)

#define MFI_ADP_RESET                           0x00000040
#define MFI_INIT_ABORT                          0x00000001
#define MFI_INIT_READY                          0x00000002
#define MFI_INIT_MFIMODE                        0x00000004
#define MFI_INIT_CLEAR_HANDSHAKE                0x00000008
#define MFI_INIT_HOTPLUG                        0x00000010
#define MFI_STOP_ADP                            0x00000020
#define MFI_RESET_FLAGS                         MFI_INIT_READY| \
                                                MFI_INIT_MFIMODE| \
                                                MFI_INIT_ABORT

/*
 * MFI frame flags 
 */
#define MFI_FRAME_POST_IN_REPLY_QUEUE           0x0000
#define MFI_FRAME_DONT_POST_IN_REPLY_QUEUE      0x0001
#define MFI_FRAME_SGL32                         0x0000
#define MFI_FRAME_SGL64                         0x0002
#define MFI_FRAME_SENSE32                       0x0000
#define MFI_FRAME_SENSE64                       0x0004
#define MFI_FRAME_DIR_NONE                      0x0000
#define MFI_FRAME_DIR_WRITE                     0x0008
#define MFI_FRAME_DIR_READ                      0x0010
#define MFI_FRAME_DIR_BOTH                      0x0018
#define MFI_FRAME_IEEE                          0x0020

/*
 * Definition for cmd_status
 */
#define MFI_CMD_STATUS_POLL_MODE                0xFF

/*
 * MFI command opcodes
 */
#define MFI_CMD_INIT                            0x00
#define MFI_CMD_LD_READ                         0x01
#define MFI_CMD_LD_WRITE                        0x02
#define MFI_CMD_LD_SCSI_IO                      0x03
#define MFI_CMD_PD_SCSI_IO                      0x04
#define MFI_CMD_DCMD                            0x05
#define MFI_CMD_ABORT                           0x06
#define MFI_CMD_SMP                             0x07
#define MFI_CMD_STP                             0x08
#define MFI_CMD_INVALID                         0xff

#define MR_DCMD_CTRL_GET_INFO                   0x01010000
#define MR_DCMD_LD_GET_LIST                     0x03010000
#define MR_DCMD_CTRL_CACHE_FLUSH                0x01101000
#define MR_FLUSH_CTRL_CACHE                     0x01
#define MR_FLUSH_DISK_CACHE                     0x02

#define MR_DCMD_CTRL_SHUTDOWN                   0x01050000
#define MR_DCMD_HIBERNATE_SHUTDOWN              0x01060000
#define MR_ENABLE_DRIVE_SPINDOWN                0x01

#define MR_DCMD_CTRL_EVENT_GET_INFO             0x01040100
#define MR_DCMD_CTRL_EVENT_GET                  0x01040300
#define MR_DCMD_CTRL_EVENT_WAIT                 0x01040500
#define MR_DCMD_LD_GET_PROPERTIES               0x03030000

#define MR_DCMD_CLUSTER                         0x08000000
#define MR_DCMD_CLUSTER_RESET_ALL               0x08010100
#define MR_DCMD_CLUSTER_RESET_LD                0x08010200
#define MR_DCMD_PD_LIST_QUERY                   0x02010100

#define MR_DCMD_CTRL_MISC_CPX                   0x0100e200
#define MR_DCMD_CTRL_MISC_CPX_INIT_DATA_GET     0x0100e201
#define MR_DCMD_CTRL_MISC_CPX_QUEUE_DATA        0x0100e202
#define MR_DCMD_CTRL_MISC_CPX_UNREGISTER        0x0100e203
#define MAX_MR_ROW_SIZE                         32
#define MR_CPX_DIR_WRITE                        1
#define MR_CPX_DIR_READ                         0
#define MR_CPX_VERSION                          1

#define MR_DCMD_CTRL_IO_METRICS_GET             0x01170200   // get IO metrics

#define MR_EVT_CFG_CLEARED                      0x0004

#define MR_EVT_LD_STATE_CHANGE                  0x0051
#define MR_EVT_PD_INSERTED                      0x005b
#define MR_EVT_PD_REMOVED                       0x0070
#define MR_EVT_LD_CREATED                       0x008a
#define MR_EVT_LD_DELETED                       0x008b
#define MR_EVT_FOREIGN_CFG_IMPORTED             0x00db
#define MR_EVT_LD_OFFLINE                       0x00fc
#define MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED     0x0152
#define MR_EVT_CTRL_PERF_COLLECTION             0x017e

/*
 * MFI command completion codes
 */
enum MFI_STAT {
    MFI_STAT_OK = 0x00,
    MFI_STAT_INVALID_CMD = 0x01,
    MFI_STAT_INVALID_DCMD = 0x02,
    MFI_STAT_INVALID_PARAMETER = 0x03,
    MFI_STAT_INVALID_SEQUENCE_NUMBER = 0x04,
    MFI_STAT_ABORT_NOT_POSSIBLE = 0x05,
    MFI_STAT_APP_HOST_CODE_NOT_FOUND = 0x06,
    MFI_STAT_APP_IN_USE = 0x07,
    MFI_STAT_APP_NOT_INITIALIZED = 0x08,
    MFI_STAT_ARRAY_INDEX_INVALID = 0x09,
    MFI_STAT_ARRAY_ROW_NOT_EMPTY = 0x0a,
    MFI_STAT_CONFIG_RESOURCE_CONFLICT = 0x0b,
    MFI_STAT_DEVICE_NOT_FOUND = 0x0c,
    MFI_STAT_DRIVE_TOO_SMALL = 0x0d,
    MFI_STAT_FLASH_ALLOC_FAIL = 0x0e,
    MFI_STAT_FLASH_BUSY = 0x0f,
    MFI_STAT_FLASH_ERROR = 0x10,
    MFI_STAT_FLASH_IMAGE_BAD = 0x11,
    MFI_STAT_FLASH_IMAGE_INCOMPLETE = 0x12,
    MFI_STAT_FLASH_NOT_OPEN = 0x13,
    MFI_STAT_FLASH_NOT_STARTED = 0x14,
    MFI_STAT_FLUSH_FAILED = 0x15,
    MFI_STAT_HOST_CODE_NOT_FOUNT = 0x16,
    MFI_STAT_LD_CC_IN_PROGRESS = 0x17,
    MFI_STAT_LD_INIT_IN_PROGRESS = 0x18,
    MFI_STAT_LD_LBA_OUT_OF_RANGE = 0x19,
    MFI_STAT_LD_MAX_CONFIGURED = 0x1a,
    MFI_STAT_LD_NOT_OPTIMAL = 0x1b,
    MFI_STAT_LD_RBLD_IN_PROGRESS = 0x1c,
    MFI_STAT_LD_RECON_IN_PROGRESS = 0x1d,
    MFI_STAT_LD_WRONG_RAID_LEVEL = 0x1e,
    MFI_STAT_MAX_SPARES_EXCEEDED = 0x1f,
    MFI_STAT_MEMORY_NOT_AVAILABLE = 0x20,
    MFI_STAT_MFC_HW_ERROR = 0x21,
    MFI_STAT_NO_HW_PRESENT = 0x22,
    MFI_STAT_NOT_FOUND = 0x23,
    MFI_STAT_NOT_IN_ENCL = 0x24,
    MFI_STAT_PD_CLEAR_IN_PROGRESS = 0x25,
    MFI_STAT_PD_TYPE_WRONG = 0x26,
    MFI_STAT_PR_DISABLED = 0x27,
    MFI_STAT_ROW_INDEX_INVALID = 0x28,
    MFI_STAT_SAS_CONFIG_INVALID_ACTION = 0x29,
    MFI_STAT_SAS_CONFIG_INVALID_DATA = 0x2a,
    MFI_STAT_SAS_CONFIG_INVALID_PAGE = 0x2b,
    MFI_STAT_SAS_CONFIG_INVALID_TYPE = 0x2c,
    MFI_STAT_SCSI_DONE_WITH_ERROR = 0x2d,
    MFI_STAT_SCSI_IO_FAILED = 0x2e,
    MFI_STAT_SCSI_RESERVATION_CONFLICT = 0x2f,
    MFI_STAT_SHUTDOWN_FAILED = 0x30,
    MFI_STAT_TIME_NOT_SET = 0x31,
    MFI_STAT_WRONG_STATE = 0x32,
    MFI_STAT_LD_OFFLINE = 0x33,
    MFI_STAT_PEER_NOTIFICATION_REJECTED = 0x34,
    MFI_STAT_PEER_NOTIFICATION_FAILED = 0x35,
    MFI_STAT_RESERVATION_IN_PROGRESS = 0x36,
    MFI_STAT_I2C_ERRORS_DETECTED = 0x37,
    MFI_STAT_PCI_ERRORS_DETECTED = 0x38,
    MFI_STAT_CONFIG_SEQ_MISMATCH = 0x67,

    MFI_STAT_INVALID_STATUS = 0xFF
};

/*
 * Number of mailbox bytes in DCMD message frame
 */
#define MFI_MBOX_SIZE                           12

enum MR_EVT_CLASS {

        MR_EVT_CLASS_DEBUG = -2,
        MR_EVT_CLASS_PROGRESS = -1,
        MR_EVT_CLASS_INFO = 0,
        MR_EVT_CLASS_WARNING = 1,
        MR_EVT_CLASS_CRITICAL = 2,
        MR_EVT_CLASS_FATAL = 3,
        MR_EVT_CLASS_DEAD = 4,

};

enum MR_EVT_LOCALE {

        MR_EVT_LOCALE_LD = 0x0001,
        MR_EVT_LOCALE_PD = 0x0002,
        MR_EVT_LOCALE_ENCL = 0x0004,
        MR_EVT_LOCALE_BBU = 0x0008,
        MR_EVT_LOCALE_SAS = 0x0010,
        MR_EVT_LOCALE_CTRL = 0x0020,
        MR_EVT_LOCALE_CONFIG = 0x0040,
        MR_EVT_LOCALE_CLUSTER = 0x0080,
        MR_EVT_LOCALE_ALL = 0xffff,

};

enum MR_EVT_ARGS {

        MR_EVT_ARGS_NONE,
        MR_EVT_ARGS_CDB_SENSE,
        MR_EVT_ARGS_LD,
        MR_EVT_ARGS_LD_COUNT,
        MR_EVT_ARGS_LD_LBA,
        MR_EVT_ARGS_LD_OWNER,
        MR_EVT_ARGS_LD_LBA_PD_LBA,
        MR_EVT_ARGS_LD_PROG,
        MR_EVT_ARGS_LD_STATE,
        MR_EVT_ARGS_LD_STRIP,
        MR_EVT_ARGS_PD,
        MR_EVT_ARGS_PD_ERR,
        MR_EVT_ARGS_PD_LBA,
        MR_EVT_ARGS_PD_LBA_LD,
        MR_EVT_ARGS_PD_PROG,
        MR_EVT_ARGS_PD_STATE,
        MR_EVT_ARGS_PCI,
        MR_EVT_ARGS_RATE,
        MR_EVT_ARGS_STR,
        MR_EVT_ARGS_TIME,
        MR_EVT_ARGS_ECC,
        MR_EVT_ARGS_LD_PROP,
        MR_EVT_ARGS_PD_SPARE,
        MR_EVT_ARGS_PD_INDEX,
        MR_EVT_ARGS_DIAG_PASS,
        MR_EVT_ARGS_DIAG_FAIL,
        MR_EVT_ARGS_PD_LBA_LBA,
        MR_EVT_ARGS_PORT_PHY,
        MR_EVT_ARGS_PD_MISSING,
        MR_EVT_ARGS_PD_ADDRESS,
        MR_EVT_ARGS_BITMAP,
        MR_EVT_ARGS_CONNECTOR,
        MR_EVT_ARGS_PD_PD,
        MR_EVT_ARGS_PD_FRU,
        MR_EVT_ARGS_PD_PATHINFO,
		MR_EVT_ARGS_PD_POWER_STATE,
        MR_EVT_ARGS_GENERIC,
};


/*
 * Thunderbolt (and later) Defines 
 */
#define MRSAS_MAX_SZ_CHAIN_FRAME                  1024
#define MFI_FUSION_ENABLE_INTERRUPT_MASK (0x00000009)
#define MRSAS_MPI2_RAID_DEFAULT_IO_FRAME_SIZE     256
#define MRSAS_MPI2_FUNCTION_PASSTHRU_IO_REQUEST   0xF0
#define MRSAS_MPI2_FUNCTION_LD_IO_REQUEST         0xF1
#define MRSAS_LOAD_BALANCE_FLAG                   0x1
#define MRSAS_DCMD_MBOX_PEND_FLAG                 0x1
#define HOST_DIAG_WRITE_ENABLE                      0x80
#define HOST_DIAG_RESET_ADAPTER                     0x4
#define MRSAS_TBOLT_MAX_RESET_TRIES              3
#define MRSAS_MAX_MFI_CMDS                       32

/*
 * Invader Defines 
 */
#define MPI2_TYPE_CUDA                              0x2
#define MPI25_SAS_DEVICE0_FLAGS_ENABLED_FAST_PATH   0x4000
#define MR_RL_FLAGS_GRANT_DESTINATION_CPU0          0x00
#define MR_RL_FLAGS_GRANT_DESTINATION_CPU1          0x10
#define MR_RL_FLAGS_GRANT_DESTINATION_CUDA          0x80
#define MR_RL_FLAGS_SEQ_NUM_ENABLE                  0x8

/* 
 * T10 PI defines 
 */
#define MR_PROT_INFO_TYPE_CONTROLLER              0x8
#define MRSAS_SCSI_VARIABLE_LENGTH_CMD            0x7f
#define MRSAS_SCSI_SERVICE_ACTION_READ32          0x9
#define MRSAS_SCSI_SERVICE_ACTION_WRITE32         0xB
#define MRSAS_SCSI_ADDL_CDB_LEN                   0x18
#define MRSAS_RD_WR_PROTECT_CHECK_ALL             0x20
#define MRSAS_RD_WR_PROTECT_CHECK_NONE            0x60
#define MRSAS_SCSIBLOCKSIZE                       512

/*
 * Raid context flags
 */
#define MR_RAID_CTX_RAID_FLAGS_IO_SUB_TYPE_SHIFT   0x4
#define MR_RAID_CTX_RAID_FLAGS_IO_SUB_TYPE_MASK    0x30
typedef enum MR_RAID_FLAGS_IO_SUB_TYPE {
        MR_RAID_FLAGS_IO_SUB_TYPE_NONE = 0,
        MR_RAID_FLAGS_IO_SUB_TYPE_SYSTEM_PD = 1,
} MR_RAID_FLAGS_IO_SUB_TYPE;

/*
 * Request descriptor types
 */
#define MRSAS_REQ_DESCRIPT_FLAGS_LD_IO           0x7
#define MRSAS_REQ_DESCRIPT_FLAGS_MFA             0x1
#define MRSAS_REQ_DESCRIPT_FLAGS_NO_LOCK         0x2
#define MRSAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT      1
#define MRSAS_FP_CMD_LEN      16
#define MRSAS_FUSION_IN_RESET 0

#define RAID_CTX_SPANARM_ARM_SHIFT      (0)
#define RAID_CTX_SPANARM_ARM_MASK       (0x1f)
#define RAID_CTX_SPANARM_SPAN_SHIFT     (5)
#define RAID_CTX_SPANARM_SPAN_MASK      (0xE0)

/*    
 * Define region lock types
 */
typedef enum    _REGION_TYPE {
    REGION_TYPE_UNUSED       = 0,    // lock is currently not active
    REGION_TYPE_SHARED_READ  = 1,    // shared lock (for reads)
    REGION_TYPE_SHARED_WRITE = 2,
    REGION_TYPE_EXCLUSIVE    = 3,    // exclusive lock (for writes)
} REGION_TYPE;

/* 
 * MR private defines 
 */
#define MR_PD_INVALID 0xFFFF
#define MAX_SPAN_DEPTH 8
#define MAX_RAIDMAP_SPAN_DEPTH (MAX_SPAN_DEPTH)
#define MAX_ROW_SIZE 32
#define MAX_RAIDMAP_ROW_SIZE (MAX_ROW_SIZE)
#define MAX_LOGICAL_DRIVES 64
#define MAX_RAIDMAP_LOGICAL_DRIVES (MAX_LOGICAL_DRIVES)
#define MAX_RAIDMAP_VIEWS (MAX_LOGICAL_DRIVES)
#define MAX_ARRAYS 128
#define MAX_RAIDMAP_ARRAYS (MAX_ARRAYS)
#define MAX_PHYSICAL_DEVICES 256
#define MAX_RAIDMAP_PHYSICAL_DEVICES (MAX_PHYSICAL_DEVICES)
#define MR_DCMD_LD_MAP_GET_INFO 0x0300e101 

/*
 * SCSI-CAM Related Defines 
 */
#define MRSAS_SCSI_MAX_LUNS     0   //zero for now 	
#define MRSAS_SCSI_INITIATOR_ID 255
#define MRSAS_SCSI_MAX_CMDS     8
#define MRSAS_SCSI_MAX_CDB_LEN  16
#define MRSAS_SCSI_SENSE_BUFFERSIZE 96
#define MRSAS_MAX_SGL           70
#define MRSAS_MAX_IO_SIZE       (256 * 1024)
#define MRSAS_INTERNAL_CMDS     32

/* Request types */
#define MRSAS_REQ_TYPE_INTERNAL_CMD     0x0
#define MRSAS_REQ_TYPE_AEN_FETCH        0x1
#define MRSAS_REQ_TYPE_PASSTHRU         0x2
#define MRSAS_REQ_TYPE_GETSET_PARAM     0x3
#define MRSAS_REQ_TYPE_SCSI_IO          0x4

/* Request states */
#define MRSAS_REQ_STATE_FREE            0
#define MRSAS_REQ_STATE_BUSY            1
#define MRSAS_REQ_STATE_TRAN            2
#define MRSAS_REQ_STATE_COMPLETE        3

enum mrsas_req_flags {
    MRSAS_DIR_UNKNOWN = 0x1,
    MRSAS_DIR_IN = 0x2,
    MRSAS_DIR_OUT = 0x4,
    MRSAS_DIR_NONE = 0x8,
};

/* 
 * Adapter Reset States 
 */
enum {
    MRSAS_HBA_OPERATIONAL                 = 0,
    MRSAS_ADPRESET_SM_INFAULT             = 1,
    MRSAS_ADPRESET_SM_FW_RESET_SUCCESS    = 2,
    MRSAS_ADPRESET_SM_OPERATIONAL         = 3,
    MRSAS_HW_CRITICAL_ERROR               = 4,
    MRSAS_ADPRESET_INPROG_SIGN            = 0xDEADDEAD,
};

/* 
 * MPT Command Structure 
 */
struct mrsas_mpt_cmd {
    MRSAS_RAID_SCSI_IO_REQUEST  *io_request;
    bus_addr_t      io_request_phys_addr;
    MPI2_SGE_IO_UNION *chain_frame;
    bus_addr_t      chain_frame_phys_addr;
    u_int32_t       sge_count;
    u_int8_t        *sense;
    bus_addr_t      sense_phys_addr;
    u_int8_t        retry_for_fw_reset;
    MRSAS_REQUEST_DESCRIPTOR_UNION *request_desc;
    u_int32_t       sync_cmd_idx; //For getting MFI cmd from list when complete
    u_int32_t       index;
    u_int8_t        flags;
    u_int8_t        load_balance;
    bus_size_t      length;       // request length 
    u_int32_t       error_code;   // error during request dmamap load  
    bus_dmamap_t    data_dmamap;        
    void            *data;
    union ccb       *ccb_ptr;     // pointer to ccb 
    struct callout  cm_callout;
    struct mrsas_softc  *sc;
    TAILQ_ENTRY(mrsas_mpt_cmd)  next;
};

/* 
 * MFI Command Structure 
 */
struct mrsas_mfi_cmd {
    union mrsas_frame   *frame;
    bus_dmamap_t        frame_dmamap;   // mfi frame dmamap 
    void                *frame_mem;     // mfi frame virtual addr 
    bus_addr_t          frame_phys_addr; // mfi frame physical addr 
    u_int8_t            *sense;
    bus_dmamap_t        sense_dmamap;   // mfi sense dmamap 
    void                *sense_mem;     // mfi sense virtual addr 
    bus_addr_t          sense_phys_addr;
    u_int32_t           index;
    u_int8_t            sync_cmd;
    u_int8_t            cmd_status;
    u_int8_t            abort_aen;
    u_int8_t            retry_for_fw_reset;
    struct mrsas_softc  *sc;
    union ccb           *ccb_ptr; 
    union {
        struct {
            u_int16_t smid;
            u_int16_t resvd;
        } context;
        u_int32_t frame_count;
    } cmd_id; 
    TAILQ_ENTRY(mrsas_mfi_cmd)  next;
};


/*
 * define constants for device list query options
 */
enum MR_PD_QUERY_TYPE {
    MR_PD_QUERY_TYPE_ALL                = 0,
    MR_PD_QUERY_TYPE_STATE              = 1,
    MR_PD_QUERY_TYPE_POWER_STATE        = 2,
    MR_PD_QUERY_TYPE_MEDIA_TYPE         = 3,
    MR_PD_QUERY_TYPE_SPEED              = 4,
    MR_PD_QUERY_TYPE_EXPOSED_TO_HOST    = 5,
};

#define MR_EVT_CFG_CLEARED                              0x0004
#define MR_EVT_LD_STATE_CHANGE                          0x0051
#define MR_EVT_PD_INSERTED                              0x005b
#define MR_EVT_PD_REMOVED                               0x0070
#define MR_EVT_LD_CREATED                               0x008a
#define MR_EVT_LD_DELETED                               0x008b
#define MR_EVT_FOREIGN_CFG_IMPORTED                     0x00db
#define MR_EVT_LD_OFFLINE                               0x00fc
#define MR_EVT_CTRL_HOST_BUS_SCAN_REQUESTED             0x0152

enum MR_PD_STATE {
    MR_PD_STATE_UNCONFIGURED_GOOD   = 0x00,
    MR_PD_STATE_UNCONFIGURED_BAD    = 0x01,
    MR_PD_STATE_HOT_SPARE           = 0x02,
    MR_PD_STATE_OFFLINE             = 0x10,
    MR_PD_STATE_FAILED              = 0x11,
    MR_PD_STATE_REBUILD             = 0x14,
    MR_PD_STATE_ONLINE              = 0x18,
    MR_PD_STATE_COPYBACK            = 0x20,
    MR_PD_STATE_SYSTEM              = 0x40,
 };

 /*
 * defines the physical drive address structure
 */
#pragma pack(1)
struct MR_PD_ADDRESS {
    u_int16_t     deviceId;
    u_int16_t     enclDeviceId;

    union {
        struct {
            u_int8_t  enclIndex;
            u_int8_t  slotNumber;
        } mrPdAddress;
        struct {
            u_int8_t  enclPosition;
            u_int8_t  enclConnectorIndex;
        } mrEnclAddress;
    } u1;
    u_int8_t      scsiDevType;
    union {
        u_int8_t      connectedPortBitmap;
        u_int8_t      connectedPortNumbers;
    } u2;
    u_int64_t     sasAddr[2];
};
#pragma pack()

/*
 * defines the physical drive list structure
 */
#pragma pack(1)
struct MR_PD_LIST {
    u_int32_t             size;
    u_int32_t             count;
    struct MR_PD_ADDRESS   addr[1];
};
#pragma pack()

#pragma pack(1)
struct mrsas_pd_list {
    u_int16_t             tid;
    u_int8_t             driveType;
    u_int8_t             driveState;
};
#pragma pack()

 /*
 * defines the logical drive reference structure
 */
typedef union  _MR_LD_REF {        // LD reference structure
    struct {
        u_int8_t      targetId;     // LD target id (0 to MAX_TARGET_ID)
        u_int8_t      reserved;     // reserved to make in line with MR_PD_REF
        u_int16_t     seqNum;       // Sequence Number
    } ld_context;
    u_int32_t     ref;              // shorthand reference to full 32-bits
} MR_LD_REF;                        // 4 bytes


/*
 * defines the logical drive list structure
 */
#pragma pack(1)
struct MR_LD_LIST {
    u_int32_t     ldCount;          // number of LDs
    u_int32_t     reserved;         // pad to 8-byte boundary
    struct {
        MR_LD_REF   ref;            // LD reference
        u_int8_t    state;          // current LD state (MR_LD_STATE)
        u_int8_t    reserved[3];    // pad to 8-byte boundary
        u_int64_t   size;           // LD size
    } ldList[MAX_LOGICAL_DRIVES];
}; 
#pragma pack()

/*
 * SAS controller properties
 */
#pragma pack(1)
struct mrsas_ctrl_prop {
    u_int16_t seq_num;
    u_int16_t pred_fail_poll_interval;
    u_int16_t intr_throttle_count;
    u_int16_t intr_throttle_timeouts;
    u_int8_t rebuild_rate;
    u_int8_t patrol_read_rate;
    u_int8_t bgi_rate;
    u_int8_t cc_rate;
    u_int8_t recon_rate;
    u_int8_t cache_flush_interval;
    u_int8_t spinup_drv_count;
    u_int8_t spinup_delay;
    u_int8_t cluster_enable;
    u_int8_t coercion_mode;
    u_int8_t alarm_enable;
    u_int8_t disable_auto_rebuild;
    u_int8_t disable_battery_warn;
    u_int8_t ecc_bucket_size;
    u_int16_t ecc_bucket_leak_rate;
    u_int8_t restore_hotspare_on_insertion;
    u_int8_t expose_encl_devices;
    u_int8_t maintainPdFailHistory;
    u_int8_t disallowHostRequestReordering;
    u_int8_t abortCCOnError;  // set TRUE to abort CC on detecting an inconsistency
    u_int8_t loadBalanceMode;     // load balance mode (MR_LOAD_BALANCE_MODE)
    u_int8_t disableAutoDetectBackplane;  // 0 - use auto detect logic of backplanes 
                                          // like SGPIO, i2c SEP using h/w mechansim 
                                          // like GPIO pins.
                                          // 1 - disable auto detect SGPIO,
                                          // 2 - disable i2c SEP auto detect
                                          // 3 - disable both auto detect
    u_int8_t snapVDSpace;  // % of source LD to be reserved for a VDs snapshot in
                           // snapshot repository, for metadata and user data.
                           // 1=5%, 2=10%, 3=15% and so on.
    /*
     * Add properties that can be controlled by a bit in the following structure.
     */
    struct {
        u_int32_t     copyBackDisabled            : 1;  // set TRUE to disable copyBack 
                                                        // (0=copback enabled)
        u_int32_t     SMARTerEnabled              : 1;
        u_int32_t     prCorrectUnconfiguredAreas  : 1;
        u_int32_t     useFdeOnly                  : 1;
        u_int32_t     disableNCQ                  : 1;
        u_int32_t     SSDSMARTerEnabled           : 1;
        u_int32_t     SSDPatrolReadEnabled        : 1;
        u_int32_t     enableSpinDownUnconfigured  : 1;
        u_int32_t     autoEnhancedImport          : 1;
        u_int32_t     enableSecretKeyControl      : 1;
        u_int32_t     disableOnlineCtrlReset      : 1;
        u_int32_t     allowBootWithPinnedCache    : 1;
        u_int32_t     disableSpinDownHS           : 1;
        u_int32_t     enableJBOD                  : 1;
        u_int32_t     reserved                    :18;
    } OnOffProperties;
    u_int8_t      autoSnapVDSpace;  // % of source LD to be reserved for auto 
                                    // snapshot in snapshot repository, for 
                                    // metadata and user data.
                                    // 1=5%, 2=10%, 3=15% and so on.
    u_int8_t      viewSpace;        // snapshot writeable VIEWs capacity as a % 
                                    // of source LD capacity. 0=READ only.
                                    // 1=5%, 2=10%, 3=15% and so on
    u_int16_t     spinDownTime;     // # of idle minutes before device is spun 
                                    // down (0=use FW defaults).
    u_int8_t      reserved[24];

}; 
#pragma pack()


/*
 * SAS controller information
 */
//#pragma pack(1)
struct mrsas_ctrl_info {
    /* 
     * PCI device information 
     */
    struct {
        u_int16_t vendor_id;
        u_int16_t device_id;
        u_int16_t sub_vendor_id;
        u_int16_t sub_device_id;
        u_int8_t reserved[24];
    } __packed pci;
    /* 
     * Host interface information 
     */
    struct {
        u_int8_t PCIX:1;
        u_int8_t PCIE:1;
        u_int8_t iSCSI:1;
        u_int8_t SAS_3G:1;
        u_int8_t reserved_0:4;
        u_int8_t reserved_1[6];
        u_int8_t port_count;
        u_int64_t port_addr[8];
    } __packed host_interface;
    /*
     * Device (backend) interface information
     */
    struct {
        u_int8_t SPI:1;
        u_int8_t SAS_3G:1;
        u_int8_t SATA_1_5G:1;
        u_int8_t SATA_3G:1;
        u_int8_t reserved_0:4;
        u_int8_t reserved_1[6];
        u_int8_t port_count;
        u_int64_t port_addr[8];
    } __packed device_interface;

    /*
     * List of components residing in flash. All str are null terminated
     */
    u_int32_t image_check_word;
    u_int32_t image_component_count;

    struct {
        char name[8];
        char version[32];
        char build_date[16];
        char built_time[16];
    } __packed image_component[8];
    /*
     * List of flash components that have been flashed on the card, but
     * are not in use, pending reset of the adapter. This list will be
     * empty if a flash operation has not occurred. All stings are null
     * terminated
     */
    u_int32_t pending_image_component_count;

    struct {
        char name[8];
        char version[32];
        char build_date[16];
        char build_time[16];
    } __packed pending_image_component[8];

    u_int8_t max_arms;
    u_int8_t max_spans;
    u_int8_t max_arrays;
    u_int8_t max_lds;
    char product_name[80];
    char serial_no[32];

    /*
     * Other physical/controller/operation information. Indicates the
     * presence of the hardware
     */
    struct {
        u_int32_t bbu:1;
        u_int32_t alarm:1;
        u_int32_t nvram:1;
        u_int32_t uart:1;
        u_int32_t reserved:28;
    } __packed hw_present;

    u_int32_t current_fw_time;

    /*
     * Maximum data transfer sizes
     */
    u_int16_t max_concurrent_cmds;
    u_int16_t max_sge_count;
    u_int32_t max_request_size;

    /*
     * Logical and physical device counts
     */
    u_int16_t ld_present_count;
    u_int16_t ld_degraded_count;
    u_int16_t ld_offline_count;

    u_int16_t pd_present_count;
    u_int16_t pd_disk_present_count;
    u_int16_t pd_disk_pred_failure_count;
    u_int16_t pd_disk_failed_count;

    /*
     * Memory size information
     */
    u_int16_t nvram_size;
    u_int16_t memory_size;
    u_int16_t flash_size;

    /*
     * Error counters
     */
    u_int16_t mem_correctable_error_count;
    u_int16_t mem_uncorrectable_error_count;

    /*
     * Cluster information
     */
    u_int8_t cluster_permitted;
    u_int8_t cluster_active;

    /*
     * Additional max data transfer sizes
     */
    u_int16_t max_strips_per_io;

    /*
     * Controller capabilities structures
     */
    struct {
        u_int32_t raid_level_0:1;
        u_int32_t raid_level_1:1;
        u_int32_t raid_level_5:1;
        u_int32_t raid_level_1E:1;
        u_int32_t raid_level_6:1;
        u_int32_t reserved:27;
    } __packed raid_levels;

    struct {
        u_int32_t rbld_rate:1;
        u_int32_t cc_rate:1;
        u_int32_t bgi_rate:1;
        u_int32_t recon_rate:1;
        u_int32_t patrol_rate:1;
        u_int32_t alarm_control:1;
        u_int32_t cluster_supported:1;
        u_int32_t bbu:1;
        u_int32_t spanning_allowed:1;
        u_int32_t dedicated_hotspares:1;
        u_int32_t revertible_hotspares:1;
        u_int32_t foreign_config_import:1;
        u_int32_t self_diagnostic:1;
        u_int32_t mixed_redundancy_arr:1;
        u_int32_t global_hot_spares:1;
        u_int32_t reserved:17;
    } __packed adapter_operations;

    struct {
        u_int32_t read_policy:1;
        u_int32_t write_policy:1;
        u_int32_t io_policy:1;
        u_int32_t access_policy:1;
        u_int32_t disk_cache_policy:1;
        u_int32_t reserved:27;
    } __packed ld_operations;

    struct {
        u_int8_t min;
        u_int8_t max;
        u_int8_t reserved[2];
    } __packed stripe_sz_ops;

    struct {
        u_int32_t force_online:1;
        u_int32_t force_offline:1;
        u_int32_t force_rebuild:1;
        u_int32_t reserved:29;
    } __packed pd_operations;

    struct {
        u_int32_t ctrl_supports_sas:1;
        u_int32_t ctrl_supports_sata:1;
        u_int32_t allow_mix_in_encl:1;
        u_int32_t allow_mix_in_ld:1;
        u_int32_t allow_sata_in_cluster:1;
        u_int32_t reserved:27;
    } __packed pd_mix_support;

    /*
     * Define ECC single-bit-error bucket information
     */
    u_int8_t ecc_bucket_count;
    u_int8_t reserved_2[11];

    /*
     * Include the controller properties (changeable items)
     */
    struct mrsas_ctrl_prop properties;

    /*
     * Define FW pkg version (set in envt v'bles on OEM basis)
     */
    char package_version[0x60];

	/*
	* If adapterOperations.supportMoreThan8Phys is set, and deviceInterface.portCount is greater than 8,
	* SAS Addrs for first 8 ports shall be populated in deviceInterface.portAddr, and the rest shall be
	* populated in deviceInterfacePortAddr2.
	*/
	u_int64_t         deviceInterfacePortAddr2[8]; //0x6a0 
	u_int8_t          reserved3[128];              //0x6e0 
						     
	struct {                                //0x760
		u_int16_t minPdRaidLevel_0                : 4;
		u_int16_t maxPdRaidLevel_0                : 12;

		u_int16_t minPdRaidLevel_1                : 4;
		u_int16_t maxPdRaidLevel_1                : 12;

		u_int16_t minPdRaidLevel_5                : 4;
		u_int16_t maxPdRaidLevel_5                : 12;

		u_int16_t minPdRaidLevel_1E               : 4;
		u_int16_t maxPdRaidLevel_1E               : 12;

		u_int16_t minPdRaidLevel_6                : 4;
		u_int16_t maxPdRaidLevel_6                : 12;

		u_int16_t minPdRaidLevel_10               : 4;
		u_int16_t maxPdRaidLevel_10               : 12;

		u_int16_t minPdRaidLevel_50               : 4;
		u_int16_t maxPdRaidLevel_50               : 12;

		u_int16_t minPdRaidLevel_60               : 4;
		u_int16_t maxPdRaidLevel_60               : 12;

		u_int16_t minPdRaidLevel_1E_RLQ0          : 4;
		u_int16_t maxPdRaidLevel_1E_RLQ0          : 12;

		u_int16_t minPdRaidLevel_1E0_RLQ0         : 4;
		u_int16_t maxPdRaidLevel_1E0_RLQ0         : 12;

		u_int16_t reserved[6];                    
	} pdsForRaidLevels;

	u_int16_t maxPds;                             //0x780 
	u_int16_t maxDedHSPs;                         //0x782 
	u_int16_t maxGlobalHSPs;                      //0x784 
	u_int16_t ddfSize;                            //0x786 
	u_int8_t  maxLdsPerArray;                     //0x788 
	u_int8_t  partitionsInDDF;                    //0x789 
	u_int8_t  lockKeyBinding;                     //0x78a 
	u_int8_t  maxPITsPerLd;                       //0x78b 
	u_int8_t  maxViewsPerLd;                      //0x78c 
	u_int8_t  maxTargetId;                        //0x78d 
	u_int16_t maxBvlVdSize;                       //0x78e 

	u_int16_t maxConfigurableSSCSize;             //0x790 
	u_int16_t currentSSCsize;                     //0x792 

	char    expanderFwVersion[12];          //0x794 

	u_int16_t PFKTrialTimeRemaining;              //0x7A0 

	u_int16_t cacheMemorySize;                    //0x7A2 

	struct {                                //0x7A4
		u_int32_t     supportPIcontroller         :1;         
		u_int32_t     supportLdPIType1            :1;         
		u_int32_t     supportLdPIType2            :1;         
		u_int32_t     supportLdPIType3            :1;         
		u_int32_t     supportLdBBMInfo            :1;         
		u_int32_t     supportShieldState          :1;         
		u_int32_t     blockSSDWriteCacheChange    :1;         
		u_int32_t     supportSuspendResumeBGops   :1;         
		u_int32_t     supportEmergencySpares      :1;         
		u_int32_t     supportSetLinkSpeed         :1;         
		u_int32_t     supportBootTimePFKChange    :1;         
		u_int32_t     supportJBOD                 :1;         
		u_int32_t     disableOnlinePFKChange      :1;         
		u_int32_t     supportPerfTuning           :1;         
		u_int32_t     supportSSDPatrolRead        :1;         
		u_int32_t     realTimeScheduler           :1;         
								
		u_int32_t     supportResetNow             :1;         
		u_int32_t     supportEmulatedDrives       :1;         
		u_int32_t     headlessMode                :1;         
		u_int32_t     dedicatedHotSparesLimited   :1;         
								
								
		u_int32_t     supportUnevenSpans          :1;
		u_int32_t     reserved                    :11;        
	} adapterOperations2;

	u_int8_t  driverVersion[32];                  //0x7A8 
	u_int8_t  maxDAPdCountSpinup60;               //0x7C8 
	u_int8_t  temperatureROC;                     //0x7C9 
	u_int8_t  temperatureCtrl;                    //0x7CA 
	u_int8_t  reserved4;                          //0x7CB
	u_int16_t maxConfigurablePds;                 //0x7CC 
						    

	u_int8_t  reserved5[2];                       //0x7CD reserved for future use

	/*
	* HA cluster information
	*/
	struct {
		u_int32_t     peerIsPresent               :1;         
		u_int32_t     peerIsIncompatible          :1;         
								
		u_int32_t     hwIncompatible              :1;         
		u_int32_t     fwVersionMismatch           :1;         
		u_int32_t     ctrlPropIncompatible        :1;         
		u_int32_t     premiumFeatureMismatch      :1;         
		u_int32_t     reserved                    :26;
	} cluster;

	char clusterId[16];                     //0x7D4 

	u_int8_t          pad[0x800-0x7E4];           //0x7E4
} __packed; 

/* 
 * Ld and PD Max Support Defines 
 */
#define MRSAS_MAX_PD                        256
#define MRSAS_MAX_LD                        64

/*
 * When SCSI mid-layer calls driver's reset routine, driver waits for
 * MRSAS_RESET_WAIT_TIME seconds for all outstanding IO to complete. Note
 * that the driver cannot _actually_ abort or reset pending commands. While
 * it is waiting for the commands to complete, it prints a diagnostic message
 * every MRSAS_RESET_NOTICE_INTERVAL seconds
 */
#define MRSAS_RESET_WAIT_TIME                 180
#define MRSAS_INTERNAL_CMD_WAIT_TIME          180
#define MRSAS_IOC_INIT_WAIT_TIME              60 
#define MRSAS_RESET_NOTICE_INTERVAL           5
#define MRSAS_IOCTL_CMD                       0
#define MRSAS_DEFAULT_CMD_TIMEOUT             90
#define MRSAS_THROTTLE_QUEUE_DEPTH            16

/* 
 * FW reports the maximum of number of commands that it can accept (maximum
 * commands that can be outstanding) at any time. The driver must report a
 * lower number to the mid layer because it can issue a few internal commands
 * itself (E.g, AEN, abort cmd, IOCTLs etc). The number of commands it needs
 * is shown below
 */
#define MRSAS_INT_CMDS                        32
#define MRSAS_SKINNY_INT_CMDS                 5
#define MRSAS_MAX_MSIX_QUEUES                 16

/*
 * FW can accept both 32 and 64 bit SGLs. We want to allocate 32/64 bit
 * SGLs based on the size of bus_addr_t
 */
#define IS_DMA64                               (sizeof(bus_addr_t) == 8)

#define MFI_XSCALE_OMR0_CHANGE_INTERRUPT     0x00000001  // MFI state change interrupt 
#define MFI_INTR_FLAG_REPLY_MESSAGE          0x00000001
#define MFI_INTR_FLAG_FIRMWARE_STATE_CHANGE  0x00000002
#define MFI_G2_OUTBOUND_DOORBELL_CHANGE_INTERRUPT 0x00000004 //MFI state change interrupt

#define MFI_OB_INTR_STATUS_MASK                 0x00000002
#define MFI_POLL_TIMEOUT_SECS                   60

#define MFI_REPLY_1078_MESSAGE_INTERRUPT        0x80000000
#define MFI_REPLY_GEN2_MESSAGE_INTERRUPT        0x00000001
#define MFI_GEN2_ENABLE_INTERRUPT_MASK          0x00000001
#define MFI_REPLY_SKINNY_MESSAGE_INTERRUPT      0x40000000
#define MFI_SKINNY_ENABLE_INTERRUPT_MASK        (0x00000001)
#define MFI_1068_PCSR_OFFSET                    0x84
#define MFI_1068_FW_HANDSHAKE_OFFSET            0x64
#define MFI_1068_FW_READY                       0xDDDD0000

#pragma pack(1)
struct mrsas_sge32 {
    u_int32_t phys_addr;
    u_int32_t length;
};
#pragma pack()

#pragma pack(1)
struct mrsas_sge64 {
    u_int64_t phys_addr;
    u_int32_t length;
};
#pragma pack()

#pragma pack()
union mrsas_sgl {
    struct mrsas_sge32 sge32[1];
    struct mrsas_sge64 sge64[1];
};
#pragma pack()

#pragma pack(1)
struct mrsas_header {
    u_int8_t cmd;                 /*00e */
    u_int8_t sense_len;           /*01h */
    u_int8_t cmd_status;          /*02h */
    u_int8_t scsi_status;         /*03h */

    u_int8_t target_id;           /*04h */
    u_int8_t lun;                 /*05h */
    u_int8_t cdb_len;             /*06h */
    u_int8_t sge_count;           /*07h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t timeout;            /*12h */
    u_int32_t data_xferlen;       /*14h */
};
#pragma pack()

#pragma pack(1)
struct mrsas_init_frame {
    u_int8_t cmd;                 /*00h */
    u_int8_t reserved_0;          /*01h */
    u_int8_t cmd_status;          /*02h */

    u_int8_t reserved_1;          /*03h */
    u_int32_t reserved_2;         /*04h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t reserved_3;         /*12h */
    u_int32_t data_xfer_len;      /*14h */

    u_int32_t queue_info_new_phys_addr_lo;  /*18h */
    u_int32_t queue_info_new_phys_addr_hi;  /*1Ch */
    u_int32_t queue_info_old_phys_addr_lo;  /*20h */
    u_int32_t queue_info_old_phys_addr_hi;  /*24h */
    u_int32_t driver_ver_lo;      /*28h */
    u_int32_t driver_ver_hi;      /*2Ch */
    u_int32_t reserved_4[4];      /*30h */
};
#pragma pack()

#pragma pack(1)
struct mrsas_io_frame {
    u_int8_t cmd;                 /*00h */
    u_int8_t sense_len;           /*01h */
    u_int8_t cmd_status;          /*02h */
    u_int8_t scsi_status;         /*03h */

    u_int8_t target_id;           /*04h */
    u_int8_t access_byte;         /*05h */
    u_int8_t reserved_0;          /*06h */
    u_int8_t sge_count;           /*07h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t timeout;            /*12h */
    u_int32_t lba_count;          /*14h */

    u_int32_t sense_buf_phys_addr_lo;     /*18h */
    u_int32_t sense_buf_phys_addr_hi;     /*1Ch */

    u_int32_t start_lba_lo;       /*20h */
    u_int32_t start_lba_hi;       /*24h */

    union mrsas_sgl sgl;  /*28h */
};
#pragma pack()

#pragma pack(1)
struct mrsas_pthru_frame {
    u_int8_t cmd;                 /*00h */
    u_int8_t sense_len;           /*01h */
    u_int8_t cmd_status;          /*02h */
    u_int8_t scsi_status;         /*03h */

    u_int8_t target_id;           /*04h */
    u_int8_t lun;                 /*05h */
    u_int8_t cdb_len;             /*06h */
    u_int8_t sge_count;           /*07h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t timeout;            /*12h */
    u_int32_t data_xfer_len;      /*14h */

    u_int32_t sense_buf_phys_addr_lo;     /*18h */
    u_int32_t sense_buf_phys_addr_hi;     /*1Ch */

    u_int8_t cdb[16];             /*20h */
    union mrsas_sgl sgl;  /*30h */
};
#pragma pack()

#pragma pack(1)
struct mrsas_dcmd_frame {
    u_int8_t cmd;                 /*00h */
    u_int8_t reserved_0;          /*01h */
    u_int8_t cmd_status;          /*02h */
    u_int8_t reserved_1[4];       /*03h */
    u_int8_t sge_count;           /*07h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t timeout;            /*12h */

    u_int32_t data_xfer_len;      /*14h */
    u_int32_t opcode;             /*18h */

    union {                 /*1Ch */
        u_int8_t b[12];
        u_int16_t s[6];
        u_int32_t w[3];
    } mbox;

    union mrsas_sgl sgl;  /*28h */
};
#pragma pack()

#pragma pack(1)
struct mrsas_abort_frame {
    u_int8_t cmd;                 /*00h */
    u_int8_t reserved_0;          /*01h */
    u_int8_t cmd_status;          /*02h */

    u_int8_t reserved_1;          /*03h */
    u_int32_t reserved_2;         /*04h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t reserved_3;         /*12h */
    u_int32_t reserved_4;         /*14h */

    u_int32_t abort_context;      /*18h */
    u_int32_t pad_1;              /*1Ch */

    u_int32_t abort_mfi_phys_addr_lo;     /*20h */
    u_int32_t abort_mfi_phys_addr_hi;     /*24h */

    u_int32_t reserved_5[6];      /*28h */
};
#pragma pack()

#pragma pack(1)
struct mrsas_smp_frame {
    u_int8_t cmd;                 /*00h */
    u_int8_t reserved_1;          /*01h */
    u_int8_t cmd_status;          /*02h */
    u_int8_t connection_status;   /*03h */

    u_int8_t reserved_2[3];       /*04h */
    u_int8_t sge_count;           /*07h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t timeout;            /*12h */

    u_int32_t data_xfer_len;      /*14h */
    u_int64_t sas_addr;           /*18h */

    union {
        struct mrsas_sge32 sge32[2];  /* [0]: resp [1]: req */
        struct mrsas_sge64 sge64[2];  /* [0]: resp [1]: req */
    } sgl;
};
#pragma pack()


#pragma pack(1)
struct mrsas_stp_frame {
    u_int8_t cmd;                 /*00h */
    u_int8_t reserved_1;          /*01h */
    u_int8_t cmd_status;          /*02h */
    u_int8_t reserved_2;          /*03h */

    u_int8_t target_id;           /*04h */
    u_int8_t reserved_3[2];       /*05h */
    u_int8_t sge_count;           /*07h */

    u_int32_t context;            /*08h */
    u_int32_t pad_0;              /*0Ch */

    u_int16_t flags;              /*10h */
    u_int16_t timeout;            /*12h */

    u_int32_t data_xfer_len;      /*14h */

    u_int16_t fis[10];            /*18h */
    u_int32_t stp_flags;

    union {
        struct mrsas_sge32 sge32[2];  /* [0]: resp [1]: data */
        struct mrsas_sge64 sge64[2];  /* [0]: resp [1]: data */
    } sgl;
};
#pragma pack()

union mrsas_frame {
    struct mrsas_header hdr;
    struct mrsas_init_frame init;
    struct mrsas_io_frame io;
    struct mrsas_pthru_frame pthru;
    struct mrsas_dcmd_frame dcmd;
    struct mrsas_abort_frame abort;
    struct mrsas_smp_frame smp;
    struct mrsas_stp_frame stp;
    u_int8_t raw_bytes[64];
};

#pragma pack(1)
union mrsas_evt_class_locale {

        struct {
                u_int16_t locale;
                u_int8_t reserved;
                int8_t class;
        } __packed members;
        
        u_int32_t word;
                
} __packed;

#pragma pack()


#pragma pack(1)
struct mrsas_evt_log_info {
        u_int32_t newest_seq_num;
        u_int32_t oldest_seq_num;
        u_int32_t clear_seq_num;
        u_int32_t shutdown_seq_num;
        u_int32_t boot_seq_num;
                                    
} __packed;

#pragma pack()

struct mrsas_progress {

	u_int16_t progress;
	u_int16_t elapsed_seconds;

} __packed;

struct mrsas_evtarg_ld {

	u_int16_t target_id;
	u_int8_t ld_index;
	u_int8_t reserved;

} __packed;

struct mrsas_evtarg_pd {
	u_int16_t device_id;
	u_int8_t encl_index;
	u_int8_t slot_number;

} __packed;

struct mrsas_evt_detail {

	u_int32_t seq_num;
	u_int32_t time_stamp;
	u_int32_t code;
	union mrsas_evt_class_locale cl;
	u_int8_t arg_type;
	u_int8_t reserved1[15];

	union {
		struct {
			struct mrsas_evtarg_pd pd;
			u_int8_t cdb_length;
			u_int8_t sense_length;
			u_int8_t reserved[2];
			u_int8_t cdb[16];
			u_int8_t sense[64];
		} __packed cdbSense;

		struct mrsas_evtarg_ld ld;

		struct {
			struct mrsas_evtarg_ld ld;
			u_int64_t count;
		} __packed ld_count;

		struct {
			u_int64_t lba;
			struct mrsas_evtarg_ld ld;
		} __packed ld_lba;

		struct {
			struct mrsas_evtarg_ld ld;
			u_int32_t prevOwner;
			u_int32_t newOwner;
		} __packed ld_owner;

		struct {
			u_int64_t ld_lba;
			u_int64_t pd_lba;
			struct mrsas_evtarg_ld ld;
			struct mrsas_evtarg_pd pd;
		} __packed ld_lba_pd_lba;

		struct {
			struct mrsas_evtarg_ld ld;
			struct mrsas_progress prog;
		} __packed ld_prog;

		struct {
			struct mrsas_evtarg_ld ld;
			u_int32_t prev_state;
			u_int32_t new_state;
		} __packed ld_state;

		struct {
			u_int64_t strip;
			struct mrsas_evtarg_ld ld;
		} __packed ld_strip;

		struct mrsas_evtarg_pd pd;

		struct {
			struct mrsas_evtarg_pd pd;
			u_int32_t err;
		} __packed pd_err;

		struct {
			u_int64_t lba;
			struct mrsas_evtarg_pd pd;
		} __packed pd_lba;

		struct {
			u_int64_t lba;
			struct mrsas_evtarg_pd pd;
			struct mrsas_evtarg_ld ld;
		} __packed pd_lba_ld;

		struct {
			struct mrsas_evtarg_pd pd;
			struct mrsas_progress prog;
		} __packed pd_prog;

		struct {
			struct mrsas_evtarg_pd pd;
			u_int32_t prevState;
			u_int32_t newState;
		} __packed pd_state;

		struct {
			u_int16_t vendorId;
			u_int16_t deviceId;
			u_int16_t subVendorId;
			u_int16_t subDeviceId;
		} __packed pci;

		u_int32_t rate;
		char str[96];

		struct {
			u_int32_t rtc;
			u_int32_t elapsedSeconds;
		} __packed time;

		struct {
			u_int32_t ecar;
			u_int32_t elog;
			char str[64];
		} __packed ecc;

		u_int8_t b[96];
		u_int16_t s[48];
		u_int32_t w[24];
		u_int64_t d[12];
	} args;

	char description[128];

} __packed;


/*******************************************************************
 * per-instance data
 ********************************************************************/
struct mrsas_softc {
    device_t           mrsas_dev;         // bus device
    struct cdev        *mrsas_cdev;       // controller device
    uint16_t           device_id;         // pci device
    struct resource    *reg_res;          // register interface window
    int                reg_res_id;        // register resource id
    bus_space_tag_t    bus_tag;           // bus space tag
    bus_space_handle_t bus_handle;        // bus space handle
    bus_dma_tag_t      mrsas_parent_tag;  // bus dma parent tag
    bus_dma_tag_t      verbuf_tag;        // verbuf tag
    bus_dmamap_t       verbuf_dmamap;     // verbuf dmamap
    void               *verbuf_mem;        // verbuf mem
    bus_addr_t         verbuf_phys_addr;   // verbuf physical addr
    bus_dma_tag_t      sense_tag;         // bus dma verbuf tag
    bus_dmamap_t       sense_dmamap;      // bus dma verbuf dmamap
    void               *sense_mem;        // pointer to sense buf
    bus_addr_t         sense_phys_addr;    // bus dma verbuf mem
    bus_dma_tag_t      io_request_tag;    // bus dma io request tag
    bus_dmamap_t       io_request_dmamap; // bus dma io request dmamap
    void               *io_request_mem;   // bus dma io request mem
    bus_addr_t         io_request_phys_addr; // io request physical address
    bus_dma_tag_t      chain_frame_tag;    // bus dma chain frame tag
    bus_dmamap_t       chain_frame_dmamap; // bus dma chain frame dmamap
    void               *chain_frame_mem;   // bus dma chain frame mem
    bus_addr_t         chain_frame_phys_addr; // chain frame phys address
    bus_dma_tag_t      reply_desc_tag;    // bus dma io request tag
    bus_dmamap_t       reply_desc_dmamap; // bus dma io request dmamap
    void               *reply_desc_mem;    // bus dma io request mem
    bus_addr_t         reply_desc_phys_addr; // bus dma io request mem
    bus_dma_tag_t      ioc_init_tag;    // bus dma io request tag
    bus_dmamap_t       ioc_init_dmamap; // bus dma io request dmamap
    void               *ioc_init_mem;   // bus dma io request mem
    bus_addr_t         ioc_init_phys_mem; // io request physical address
    bus_dma_tag_t      data_tag;          // bus dma data from OS tag
    struct cam_sim     *sim_0;            // SIM pointer
    struct cam_sim     *sim_1;            // SIM pointer
    struct cam_path    *path_0;           // ldio path pointer to CAM
    struct cam_path    *path_1;           // syspd path pointer to CAM
    struct mtx sim_lock;                  // sim lock
    struct mtx pci_lock;                  // serialize pci access
    struct mtx io_lock;                   // IO lock
    struct mtx ioctl_lock;                // IOCTL lock
    struct mtx mpt_cmd_pool_lock;         // lock for cmd pool linked list
    struct mtx mfi_cmd_pool_lock;         // lock for cmd pool linked list
    struct mtx raidmap_lock;              // lock for raid map access/update
    struct mtx aen_lock;                  // aen lock
    uint32_t           max_fw_cmds;       // Max commands from FW
    uint32_t           max_num_sge;       // Max number of SGEs
    struct resource    *mrsas_irq;        // interrupt interface window
    void               *intr_handle;      // handle
    int                irq_id;            // intr resource id
    struct mrsas_mpt_cmd   **mpt_cmd_list;
    struct mrsas_mfi_cmd   **mfi_cmd_list;
    TAILQ_HEAD(, mrsas_mpt_cmd) mrsas_mpt_cmd_list_head;
    TAILQ_HEAD(, mrsas_mfi_cmd) mrsas_mfi_cmd_list_head;
    bus_addr_t         req_frames_desc_phys;
    u_int8_t           *req_frames_desc;
    u_int8_t           *req_desc;
    bus_addr_t         io_request_frames_phys;
    u_int8_t           *io_request_frames;
    bus_addr_t         reply_frames_desc_phys;
    u_int16_t          last_reply_idx;
    u_int32_t          reply_q_depth;
    u_int32_t          request_alloc_sz;
    u_int32_t          reply_alloc_sz;
    u_int32_t          io_frames_alloc_sz;
    u_int32_t          chain_frames_alloc_sz;
    u_int16_t          max_sge_in_main_msg;
    u_int16_t          max_sge_in_chain;
    u_int8_t           chain_offset_io_request;
    u_int8_t           chain_offset_mfi_pthru;
    u_int32_t          map_sz;
    u_int64_t          map_id;
    struct mrsas_mfi_cmd *map_update_cmd;
    struct mrsas_mfi_cmd *aen_cmd;	
    u_int8_t           fast_path_io;
    void*              chan;
    void*              ocr_chan;
    u_int8_t           adprecovery;
    u_int8_t           remove_in_progress;
    u_int8_t           ocr_thread_active;
    u_int8_t           do_timedout_reset;
    u_int32_t          reset_in_progress;
    u_int32_t          reset_count;
    bus_dma_tag_t      raidmap_tag[2];    // bus dma tag for RAID map
    bus_dmamap_t       raidmap_dmamap[2]; // bus dma dmamap RAID map
    void               *raidmap_mem[2];   // bus dma mem RAID map
    bus_addr_t         raidmap_phys_addr[2]; // RAID map physical address
    bus_dma_tag_t      mficmd_frame_tag;      // tag for mfi frame
    bus_dma_tag_t      mficmd_sense_tag;      // tag for mfi sense
    bus_dma_tag_t      evt_detail_tag;        // event detail tag
    bus_dmamap_t       evt_detail_dmamap;     // event detail dmamap
    struct mrsas_evt_detail   *evt_detail_mem;        // event detail mem
    bus_addr_t         evt_detail_phys_addr;   // event detail physical addr
    bus_dma_tag_t      ctlr_info_tag;    // tag for get ctlr info cmd
    bus_dmamap_t       ctlr_info_dmamap; // get ctlr info cmd dmamap
    void               *ctlr_info_mem;   // get ctlr info cmd virtual addr
    bus_addr_t         ctlr_info_phys_addr; //get ctlr info cmd physical addr
    u_int32_t          max_sectors_per_req;
    u_int8_t           disableOnlineCtrlReset;
    atomic_t           fw_outstanding;
    u_int32_t          mrsas_debug;
    u_int32_t          mrsas_io_timeout;
    u_int32_t          mrsas_fw_fault_check_delay;
	u_int32_t          io_cmds_highwater; 
	u_int8_t           UnevenSpanSupport;
    struct sysctl_ctx_list   sysctl_ctx;
    struct sysctl_oid        *sysctl_tree;
    struct proc              *ocr_thread;
    u_int32_t	last_seq_num;
    bus_dma_tag_t      el_info_tag;    // tag for get event log info cmd
    bus_dmamap_t       el_info_dmamap; // get event log info cmd dmamap
    void               *el_info_mem;   // get event log info cmd virtual addr
    bus_addr_t         el_info_phys_addr; //get event log info cmd physical addr
    struct mrsas_pd_list pd_list[MRSAS_MAX_PD];
    struct mrsas_pd_list local_pd_list[MRSAS_MAX_PD];
    u_int8_t           ld_ids[MRSAS_MAX_LD];
    struct taskqueue    *ev_tq;	//taskqueue for events
    struct task     	ev_task;
    u_int32_t          CurLdCount;
    u_int64_t          reset_flags;
    LD_LOAD_BALANCE_INFO load_balance_info[MAX_LOGICAL_DRIVES];
    LD_SPAN_INFO log_to_span[MAX_LOGICAL_DRIVES];
};

/* Compatibility shims for different OS versions */
#if __FreeBSD_version >= 800001
#define mrsas_kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg) \
    kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg)
#define mrsas_kproc_exit(arg)   kproc_exit(arg)
#else
#define mrsas_kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg) \
    kthread_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg)
#define mrsas_kproc_exit(arg)   kthread_exit(arg)
#endif

static __inline void
clear_bit(int b, volatile void *p)
{
    atomic_clear_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
set_bit(int b, volatile void *p)
{
    atomic_set_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
test_bit(int b, volatile void *p)
{
    return ((volatile int *)p)[b >> 5] & (1 << (b & 0x1f));
}

#endif  /* MRSAS_H */
