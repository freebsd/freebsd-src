/* $FreeBSD$ */
/*
 *  Copyright (c) 2001 LSI Logic Corporation.
 *
 *
 *           Name:  MPI_RAID.H
 *          Title:  MPI RAID message and structures
 *  Creation Date:  February 27, 2001
 *
 *    MPI Version:  01.02.04
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  02-27-01  01.01.01  Original release for this file.
 *  03-27-01  01.01.02  Added structure offset comments.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  09-28-01  01.02.02  Major rework for MPI v1.2 Integrated RAID changes.
 *  10-04-01  01.02.03  Added ActionData defines for
 *                      MPI_RAID_ACTION_DELETE_VOLUME action.
 *  11-01-01  01.02.04  Added define for MPI_RAID_ACTION_ADATA_DO_NOT_SYNC.
 *  --------------------------------------------------------------------------
 */
/*
 * Copyright (c) 2002 by Matthew Jacob
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the original LSI Logic
 *    copyright notice at the beginning of the file and the above copyright
 *    notice immediately after it, both without modification, this list of
 *    conditions, and the following disclaimer and note.
 * 2. The name of the author or LSI Logic may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND LSI LOGIC AND CONTRIBUTORS
 *``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Note: this copyright information has been added with the express written
 * consent of LSI Logic (available upon request) in order to clarify the
 * original LSI Logic copyright in order to allow for unencumbered binary
 * and source redistribution of the MPI API definitions.
 */

#ifndef MPI_RAID_H
#define MPI_RAID_H


/******************************************************************************
*
*        R A I D    M e s s a g e s
*
*******************************************************************************/


/****************************************************************************/
/* RAID Volume Request                                                      */
/****************************************************************************/

typedef struct _MSG_RAID_ACTION
{
    U8                      Action;             /* 00h */
    U8                      Reserved1;          /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U8                      VolumeID;           /* 04h */
    U8                      VolumeBus;          /* 05h */
    U8                      PhysDiskNum;        /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     Reserved2;          /* 0Ch */
    U32                     ActionDataWord;     /* 10h */
    SGE_SIMPLE_UNION        ActionDataSGE;      /* 14h */
} MSG_RAID_ACTION_REQUEST, MPI_POINTER PTR_MSG_RAID_ACTION_REQUEST,
  MpiRaidActionRequest_t , MPI_POINTER pMpiRaidActionRequest_t;


/* RAID Action request Action values */

#define MPI_RAID_ACTION_STATUS                      (0x00)
#define MPI_RAID_ACTION_INDICATOR_STRUCT            (0x01)
#define MPI_RAID_ACTION_CREATE_VOLUME               (0x02)
#define MPI_RAID_ACTION_DELETE_VOLUME               (0x03)
#define MPI_RAID_ACTION_DISABLE_VOLUME              (0x04)
#define MPI_RAID_ACTION_ENABLE_VOLUME               (0x05)
#define MPI_RAID_ACTION_QUIESCE_PHYS_IO             (0x06)
#define MPI_RAID_ACTION_ENABLE_PHYS_IO              (0x07)
#define MPI_RAID_ACTION_CHANGE_VOLUME_SETTINGS      (0x08)
#define MPI_RAID_ACTION_PHYSDISK_OFFLINE            (0x0A)
#define MPI_RAID_ACTION_PHYSDISK_ONLINE             (0x0B)
#define MPI_RAID_ACTION_CHANGE_PHYSDISK_SETTINGS    (0x0C)
#define MPI_RAID_ACTION_CREATE_PHYSDISK             (0x0D)
#define MPI_RAID_ACTION_DELETE_PHYSDISK             (0x0E)
#define MPI_RAID_ACTION_FAIL_PHYSDISK               (0x0F)
#define MPI_RAID_ACTION_REPLACE_PHYSDISK            (0x10)

/* ActionDataWord defines for use with MPI_RAID_ACTION_CREATE_VOLUME action */
#define MPI_RAID_ACTION_ADATA_DO_NOT_SYNC           (0x00000001)

/* ActionDataWord defines for use with MPI_RAID_ACTION_DELETE_VOLUME action */
#define MPI_RAID_ACTION_ADATA_KEEP_PHYS_DISKS       (0x00000000)
#define MPI_RAID_ACTION_ADATA_DEL_PHYS_DISKS        (0x00000001)


/* RAID Action reply message */

typedef struct _MSG_RAID_ACTION_REPLY
{
    U8                      Action;             /* 00h */
    U8                      Reserved;           /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      VolumeID;           /* 04h */
    U8                      VolumeBus;          /* 05h */
    U8                      PhysDiskNum;        /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     ActionStatus;       /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     VolumeStatus;       /* 14h */
    U32                     ActionData;         /* 18h */
} MSG_RAID_ACTION_REPLY, MPI_POINTER PTR_MSG_RAID_ACTION_REPLY,
  MpiRaidActionReply_t, MPI_POINTER pMpiRaidActionReply_t;


/* RAID Volume reply ActionStatus values */

#define MPI_RAID_ACTION_ASTATUS_SUCCESS             (0x0000)
#define MPI_RAID_ACTION_ASTATUS_INVALID_ACTION      (0x0001)
#define MPI_RAID_ACTION_ASTATUS_FAILURE             (0x0002)
#define MPI_RAID_ACTION_ASTATUS_IN_PROGRESS         (0x0003)


/* RAID Volume reply RAID Volume Indicator structure */

typedef struct _MPI_RAID_VOL_INDICATOR
{
    U64                     TotalBlocks;        /* 00h */
    U64                     BlocksRemaining;    /* 08h */
} MPI_RAID_VOL_INDICATOR, MPI_POINTER PTR_MPI_RAID_VOL_INDICATOR,
  MpiRaidVolIndicator_t, MPI_POINTER pMpiRaidVolIndicator_t;


/****************************************************************************/
/* SCSI IO RAID Passthrough Request                                         */
/****************************************************************************/

typedef struct _MSG_SCSI_IO_RAID_PT_REQUEST
{
    U8                      PhysDiskNum;        /* 00h */
    U8                      Reserved1;          /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U8                      CDBLength;          /* 04h */
    U8                      SenseBufferLength;  /* 05h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      LUN[8];             /* 0Ch */
    U32                     Control;            /* 14h */
    U8                      CDB[16];            /* 18h */
    U32                     DataLength;         /* 28h */
    U32                     SenseBufferLowAddr; /* 2Ch */
    SGE_IO_UNION            SGL;                /* 30h */
} MSG_SCSI_IO_RAID_PT_REQUEST, MPI_POINTER PTR_MSG_SCSI_IO_RAID_PT_REQUEST,
  SCSIIORaidPassthroughRequest_t, MPI_POINTER pSCSIIORaidPassthroughRequest_t;


/* SCSI IO RAID Passthrough reply structure */

typedef struct _MSG_SCSI_IO_RAID_PT_REPLY
{
    U8                      PhysDiskNum;        /* 00h */
    U8                      Reserved1;          /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      CDBLength;          /* 04h */
    U8                      SenseBufferLength;  /* 05h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      SCSIStatus;         /* 0Ch */
    U8                      SCSIState;          /* 0Dh */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     TransferCount;      /* 14h */
    U32                     SenseCount;         /* 18h */
    U32                     ResponseInfo;       /* 1Ch */
} MSG_SCSI_IO_RAID_PT_REPLY, MPI_POINTER PTR_MSG_SCSI_IO_RAID_PT_REPLY,
  SCSIIORaidPassthroughReply_t, MPI_POINTER pSCSIIORaidPassthroughReply_t;


#endif



