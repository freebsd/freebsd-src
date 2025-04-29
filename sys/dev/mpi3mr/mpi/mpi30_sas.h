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

#ifndef MPI30_SAS_H
#define MPI30_SAS_H     1

/*****************************************************************************
 *              SAS Device Info Definitions                                  *
 ****************************************************************************/
#define MPI3_SAS_DEVICE_INFO_SSP_TARGET             (0x00000100)
#define MPI3_SAS_DEVICE_INFO_STP_SATA_TARGET        (0x00000080)
#define MPI3_SAS_DEVICE_INFO_SMP_TARGET             (0x00000040)
#define MPI3_SAS_DEVICE_INFO_SSP_INITIATOR          (0x00000020)
#define MPI3_SAS_DEVICE_INFO_STP_INITIATOR          (0x00000010)
#define MPI3_SAS_DEVICE_INFO_SMP_INITIATOR          (0x00000008)
#define MPI3_SAS_DEVICE_INFO_DEVICE_TYPE_MASK       (0x00000007)
#define MPI3_SAS_DEVICE_INFO_DEVICE_TYPE_SHIFT      (0)
#define MPI3_SAS_DEVICE_INFO_DEVICE_TYPE_NO_DEVICE  (0x00000000)
#define MPI3_SAS_DEVICE_INFO_DEVICE_TYPE_END_DEVICE (0x00000001)
#define MPI3_SAS_DEVICE_INFO_DEVICE_TYPE_EXPANDER   (0x00000002)

/*****************************************************************************
 *              SMP Passthrough Request Message                              *
 ****************************************************************************/
typedef struct _MPI3_SMP_PASSTHROUGH_REQUEST
{
    U16                     HostTag;                        /* 0x00 */
    U8                      IOCUseOnly02;                   /* 0x02 */
    U8                      Function;                       /* 0x03 */
    U16                     IOCUseOnly04;                   /* 0x04 */
    U8                      IOCUseOnly06;                   /* 0x06 */
    U8                      MsgFlags;                       /* 0x07 */
    U16                     ChangeCount;                    /* 0x08 */
    U8                      Reserved0A;                     /* 0x0A */
    U8                      IOUnitPort;                     /* 0x0B */
    U32                     Reserved0C[3];                  /* 0x0C */
    U64                     SASAddress;                     /* 0x18 */
    MPI3_SGE_SIMPLE         RequestSGE;                     /* 0x20 */
    MPI3_SGE_SIMPLE         ResponseSGE;                    /* 0x30 */
} MPI3_SMP_PASSTHROUGH_REQUEST, MPI3_POINTER PTR_MPI3_SMP_PASSTHROUGH_REQUEST,
  Mpi3SmpPassthroughRequest_t, MPI3_POINTER pMpi3SmpPassthroughRequest_t;

/*****************************************************************************
 *              SMP Passthrough Reply Message                               *
 ****************************************************************************/
typedef struct _MPI3_SMP_PASSTHROUGH_REPLY
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
    U16                     ResponseDataLength;             /* 0x10 */
    U16                     Reserved12;                     /* 0x12 */
} MPI3_SMP_PASSTHROUGH_REPLY, MPI3_POINTER PTR_MPI3_SMP_PASSTHROUGH_REPLY,
  Mpi3SmpPassthroughReply_t, MPI3_POINTER pMpi3SmpPassthroughReply_t;

#endif  /* MPI30_SAS_H */
