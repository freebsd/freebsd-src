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

#ifndef MPI30_PCI_H
#define MPI30_PCI_H     1

/*****************************************************************************
 *              NVMe Encapsulated Request Message                            *
 ****************************************************************************/
#ifndef MPI3_NVME_ENCAP_CMD_MAX
#define MPI3_NVME_ENCAP_CMD_MAX               (1)
#endif  /* MPI3_NVME_ENCAP_CMD_MAX */

typedef struct _MPI3_NVME_ENCAPSULATED_REQUEST
{
    U16                     HostTag;                           /* 0x00 */
    U8                      IOCUseOnly02;                      /* 0x02 */
    U8                      Function;                          /* 0x03 */
    U16                     IOCUseOnly04;                      /* 0x04 */
    U8                      IOCUseOnly06;                      /* 0x06 */
    U8                      MsgFlags;                          /* 0x07 */
    U16                     ChangeCount;                       /* 0x08 */
    U16                     DevHandle;                         /* 0x0A */
    U16                     EncapsulatedCommandLength;         /* 0x0C */
    U16                     Flags;                             /* 0x0E */
    U32                     DataLength;                        /* 0x10 */
    U32                     Reserved14[3];                     /* 0x14 */
    U32                     Command[MPI3_NVME_ENCAP_CMD_MAX];  /* 0x20 */     /* variable length */
} MPI3_NVME_ENCAPSULATED_REQUEST, MPI3_POINTER PTR_MPI3_NVME_ENCAPSULATED_REQUEST,
  Mpi3NVMeEncapsulatedRequest_t, MPI3_POINTER pMpi3NVMeEncapsulatedRequest_t;

/**** Defines for the Flags field ****/
#define MPI3_NVME_FLAGS_FORCE_ADMIN_ERR_REPLY_MASK      (0x0002)
#define MPI3_NVME_FLAGS_FORCE_ADMIN_ERR_REPLY_SHIFT     (1)
#define MPI3_NVME_FLAGS_FORCE_ADMIN_ERR_REPLY_FAIL_ONLY (0x0000)
#define MPI3_NVME_FLAGS_FORCE_ADMIN_ERR_REPLY_ALL       (0x0002)
#define MPI3_NVME_FLAGS_SUBMISSIONQ_MASK                (0x0001)
#define MPI3_NVME_FLAGS_SUBMISSIONQ_SHIFT               (0)
#define MPI3_NVME_FLAGS_SUBMISSIONQ_IO                  (0x0000)
#define MPI3_NVME_FLAGS_SUBMISSIONQ_ADMIN               (0x0001)


/*****************************************************************************
 *              NVMe Encapsulated Error Reply Message                        *
 ****************************************************************************/
typedef struct _MPI3_NVME_ENCAPSULATED_ERROR_REPLY
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
    U32                     NVMeCompletionEntry[4];     /* 0x10 */
} MPI3_NVME_ENCAPSULATED_ERROR_REPLY, MPI3_POINTER PTR_MPI3_NVME_ENCAPSULATED_ERROR_REPLY,
  Mpi3NVMeEncapsulatedErrorReply_t, MPI3_POINTER pMpi3NVMeEncapsulatedErrorReply_t;

#endif  /* MPI30_PCI_H */
