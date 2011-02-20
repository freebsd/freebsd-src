/* $FreeBSD$ */
/*
 *  Copyright (c) 2009 LSI Corporation.
 *
 *
 *           Name:  mpi2_ra.h
 *          Title:  MPI RAID Accelerator messages and structures
 *  Creation Date:  April 13, 2009
 *
 *  mpi2_ra.h Version:  02.00.00
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-06-09  02.00.00  Initial version.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI2_RA_H
#define MPI2_RA_H

/* generic structure for RAID Accelerator Control Block */
typedef struct _MPI2_RAID_ACCELERATOR_CONTROL_BLOCK
{
    U32                 Reserved[8];                /* 0x00 */
    U32                 RaidAcceleratorCDB[1];      /* 0x20 */
} MPI2_RAID_ACCELERATOR_CONTROL_BLOCK,
  MPI2_POINTER PTR_MPI2_RAID_ACCELERATOR_CONTROL_BLOCK,
  Mpi2RAIDAcceleratorControlBlock_t,
  MPI2_POINTER pMpi2RAIDAcceleratorControlBlock_t;


/******************************************************************************
*
*        RAID Accelerator Messages
*
*******************************************************************************/

/* RAID Accelerator Request Message */
typedef struct _MPI2_RAID_ACCELERATOR_REQUEST
{
    U16                     Reserved0;                          /* 0x00 */
    U8                      ChainOffset;                        /* 0x02 */
    U8                      Function;                           /* 0x03 */
    U16                     Reserved1;                          /* 0x04 */
    U8                      Reserved2;                          /* 0x06 */
    U8                      MsgFlags;                           /* 0x07 */
    U8                      VP_ID;                              /* 0x08 */
    U8                      VF_ID;                              /* 0x09 */
    U16                     Reserved3;                          /* 0x0A */
    U64                     RaidAcceleratorControlBlockAddress; /* 0x0C */
    U8                      DmaEngineNumber;                    /* 0x14 */
    U8                      Reserved4;                          /* 0x15 */
    U16                     Reserved5;                          /* 0x16 */
    U32                     Reserved6;                          /* 0x18 */
    U32                     Reserved7;                          /* 0x1C */
    U32                     Reserved8;                          /* 0x20 */
} MPI2_RAID_ACCELERATOR_REQUEST, MPI2_POINTER PTR_MPI2_RAID_ACCELERATOR_REQUEST,
  Mpi2RAIDAcceleratorRequest_t, MPI2_POINTER pMpi2RAIDAcceleratorRequest_t;


/* RAID Accelerator Error Reply Message */
typedef struct _MPI2_RAID_ACCELERATOR_REPLY
{
    U16                     Reserved0;                      /* 0x00 */
    U8                      MsgLength;                      /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     Reserved1;                      /* 0x04 */
    U8                      Reserved2;                      /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U8                      VP_ID;                          /* 0x08 */
    U8                      VF_ID;                          /* 0x09 */
    U16                     Reserved3;                      /* 0x0A */
    U16                     Reserved4;                      /* 0x0C */
    U16                     IOCStatus;                      /* 0x0E */
    U32                     IOCLogInfo;                     /* 0x10 */
    U32                     ProductSpecificData[3];         /* 0x14 */
} MPI2_RAID_ACCELERATOR_REPLY, MPI2_POINTER PTR_MPI2_RAID_ACCELERATOR_REPLY,
  Mpi2RAIDAcceleratorReply_t, MPI2_POINTER pMpi2RAIDAcceleratorReply_t;


#endif


