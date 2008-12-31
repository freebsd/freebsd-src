/* $FreeBSD: src/sys/dev/mpt/mpilib/mpi_inb.h,v 1.1.12.1 2008/11/25 02:59:29 kensmith Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/*
 *  Copyright (c) 2003-2004 LSI Logic Corporation.
 *
 *
 *           Name:  mpi_inb.h
 *          Title:  MPI Inband structures and definitions
 *  Creation Date:  September 30, 2003
 *
 *    mpi_inb.h Version:  01.05.01
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-11-04  01.03.01  Original release.
 *  08-19-04  01.05.01  Original release for MPI v1.5.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_INB_H
#define MPI_INB_H

/******************************************************************************
*
*        I n b a n d    M e s s a g e s
*
*******************************************************************************/


/****************************************************************************/
/* Inband Buffer Post Request                                               */
/****************************************************************************/

typedef struct _MSG_INBAND_BUFFER_POST_REQUEST
{
    U8                      Reserved1;          /* 00h */
    U8                      BufferCount;        /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     Reserved4;          /* 0Ch */
    SGE_TRANS_SIMPLE_UNION  SGL;                /* 10h */
} MSG_INBAND_BUFFER_POST_REQUEST, MPI_POINTER PTR_MSG_INBAND_BUFFER_POST_REQUEST,
  MpiInbandBufferPostRequest_t , MPI_POINTER pMpiInbandBufferPostRequest_t;


typedef struct _WWN_FC_FORMAT
{
    U64                     NodeName;           /* 00h */
    U64                     PortName;           /* 08h */
} WWN_FC_FORMAT, MPI_POINTER PTR_WWN_FC_FORMAT,
  WwnFcFormat_t, MPI_POINTER pWwnFcFormat_t;

typedef struct _WWN_SAS_FORMAT
{
    U64                     WorldWideID;        /* 00h */
    U32                     Reserved1;          /* 08h */
    U32                     Reserved2;          /* 0Ch */
} WWN_SAS_FORMAT, MPI_POINTER PTR_WWN_SAS_FORMAT,
  WwnSasFormat_t, MPI_POINTER pWwnSasFormat_t;

typedef union _WWN_INBAND_FORMAT
{
    WWN_FC_FORMAT           Fc;
    WWN_SAS_FORMAT          Sas;
} WWN_INBAND_FORMAT, MPI_POINTER PTR_WWN_INBAND_FORMAT,
  WwnInbandFormat, MPI_POINTER pWwnInbandFormat;


/* Inband Buffer Post reply message */

typedef struct _MSG_INBAND_BUFFER_POST_REPLY
{
    U16                     Reserved1;          /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved4;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     TransferLength;     /* 14h */
    U32                     TransactionContext; /* 18h */
    WWN_INBAND_FORMAT       Wwn;                /* 1Ch */
    U32                     IOCIdentifier[4];   /* 2Ch */
} MSG_INBAND_BUFFER_POST_REPLY, MPI_POINTER PTR_MSG_INBAND_BUFFER_POST_REPLY,
  MpiInbandBufferPostReply_t, MPI_POINTER pMpiInbandBufferPostReply_t;


/****************************************************************************/
/* Inband Send Request                                                      */
/****************************************************************************/

typedef struct _MSG_INBAND_SEND_REQUEST
{
    U16                     Reserved1;          /* 00h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     Reserved4;          /* 0Ch */
    WWN_INBAND_FORMAT       Wwn;                /* 10h */
    U32                     Reserved5;          /* 20h */
    SGE_IO_UNION            SGL;                /* 24h */
} MSG_INBAND_SEND_REQUEST, MPI_POINTER PTR_MSG_INBAND_SEND_REQUEST,
  MpiInbandSendRequest_t , MPI_POINTER pMpiInbandSendRequest_t;


/* Inband Send reply message */

typedef struct _MSG_INBAND_SEND_REPLY
{
    U16                     Reserved1;          /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved4;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     ResponseLength;     /* 14h */
} MSG_INBAND_SEND_REPLY, MPI_POINTER PTR_MSG_INBAND_SEND_REPLY,
  MpiInbandSendReply_t, MPI_POINTER pMpiInbandSendReply_t;


/****************************************************************************/
/* Inband Response Request                                                  */
/****************************************************************************/

typedef struct _MSG_INBAND_RSP_REQUEST
{
    U16                     Reserved1;          /* 00h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     Reserved4;          /* 0Ch */
    WWN_INBAND_FORMAT       Wwn;                /* 10h */
    U32                     IOCIdentifier[4];   /* 20h */
    U32                     ResponseLength;     /* 30h */
    SGE_IO_UNION            SGL;                /* 34h */
} MSG_INBAND_RSP_REQUEST, MPI_POINTER PTR_MSG_INBAND_RSP_REQUEST,
  MpiInbandRspRequest_t , MPI_POINTER pMpiInbandRspRequest_t;


/* Inband Response reply message */

typedef struct _MSG_INBAND_RSP_REPLY
{
    U16                     Reserved1;          /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved4;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
} MSG_INBAND_RSP_REPLY, MPI_POINTER PTR_MSG_INBAND_RSP_REPLY,
  MpiInbandRspReply_t, MPI_POINTER pMpiInbandRspReply_t;


/****************************************************************************/
/* Inband Abort Request                                                     */
/****************************************************************************/

typedef struct _MSG_INBAND_ABORT_REQUEST
{
    U8                      Reserved1;          /* 00h */
    U8                      AbortType;          /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     Reserved4;          /* 0Ch */
    U32                     ContextToAbort;     /* 10h */
} MSG_INBAND_ABORT_REQUEST, MPI_POINTER PTR_MSG_INBAND_ABORT_REQUEST,
  MpiInbandAbortRequest_t , MPI_POINTER pMpiInbandAbortRequest_t;

#define MPI_INBAND_ABORT_TYPE_ALL_BUFFERS       (0x00)
#define MPI_INBAND_ABORT_TYPE_EXACT_BUFFER      (0x01)
#define MPI_INBAND_ABORT_TYPE_SEND_REQUEST      (0x02)
#define MPI_INBAND_ABORT_TYPE_RESPONSE_REQUEST  (0x03)


/* Inband Abort reply message */

typedef struct _MSG_INBAND_ABORT_REPLY
{
    U8                      Reserved1;          /* 00h */
    U8                      AbortType;          /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved4;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
} MSG_INBAND_ABORT_REPLY, MPI_POINTER PTR_MSG_INBAND_ABORT_REPLY,
  MpiInbandAbortReply_t, MPI_POINTER pMpiInbandAbortReply_t;


#endif
