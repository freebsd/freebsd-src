/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Copyright (C) 1998 by the Board of Trustees
 *    of Leland Stanford Junior University.
 * Copyright (C) 1998 Digital Equipment Corporation
 *
 * This file is part of the SimOS distribution.
 * See LICENSE file for terms of the license.
 *
 */




#ifndef _SIMOS_SCSI_H
#define _SIMOS_SCSI_H

#define SIMOS_SCSI_ADDR       0xfffffcc500000000
#define SIMOS_SCSI_ADDR_32    0xffffffffa5000000
#define SIMOS_SCSI_MAXDMA_LEN 128
#define SIMOS_SCSI_MAXTARG    16
#define SIMOS_SCSI_MAXLUN     16

#define SIMOS_SCSI_REGS     ((struct SimOS_SCSI *)SIMOS_SCSI_ADDR)
#define SIMOS_SCSI_REGS_32  ((struct SimOS_SCSI *)SIMOS_SCSI_ADDR_32)

typedef unsigned long SCSIReg;


typedef struct SimOS_SCSI {
   SCSIReg startIO;   /* write-only */
   SCSIReg done[SIMOS_SCSI_MAXTARG];  /* read-write (write=ack) */

   SCSIReg target;                     /* data fields */
   SCSIReg lun;
   SCSIReg cmd[12];
   SCSIReg length;
   SCSIReg sgLen;
   struct {
      SCSIReg pAddr;
      SCSIReg len;
   } sgMap[SIMOS_SCSI_MAXDMA_LEN];
   
   
} SimOS_SCSI;

#endif
