/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *	$Id: vpo.h,v 1.1 1997/08/14 13:57:45 msmith Exp $
 *
 */
#ifndef __VP03_H
#define __VP03_H

#define barrier() __asm__("": : :"memory")

#define VP0_INITIATOR	0x7

#define VP0_SECTOR_SIZE	512
#define VP0_BUFFER_SIZE	0x12000

#define VP0_SPL() splbio()

#define VP0_ESELECT_TIMEOUT	1
#define VP0_ECMD_TIMEOUT	2
#define VP0_ECONNECT		3
#define VP0_ESTATUS_TIMEOUT	4
#define VP0_EDATA_OVERFLOW	5	
#define VP0_EDISCONNECT		6
#define VP0_EPPDATA_TIMEOUT	7
#define VP0_ENOPORT		9
#define VP0_EINITFAILED		10
#define VP0_EINTR		12

#define VP0_EOTHER		13

#define VP0_OPENNINGS	1

#define n(flags) (~(flags) & (flags))

/*
 * VP0 timings.
 */
#define MHZ_16_IO_DURATION	62

#define VP0_SPP_WRITE_PULSE	253
#define VP0_NIBBLE_READ_PULSE	486

/*
 * VP0 connections.
 */
#define H_AUTO		n(AUTOFEED)
#define H_nAUTO		AUTOFEED
#define H_STROBE	n(STROBE)
#define H_nSTROBE	STROBE
#define H_BSY		n(nBUSY)
#define H_nBSY		n_BUSY
#define H_SEL		SELECT
#define H_nSEL		n(SELECT)
#define H_ERR		ERROR
#define H_nERR		n(ERROR)
#define H_ACK		nACK
#define H_nACK		n(nACK)
#define H_FLT		nFAULT
#define H_nFLT		n(nFAULT)
#define H_SELIN		n(SELECTIN)
#define H_nSELIN	SELECTIN
#define H_INIT		nINIT
#define H_nINIT		n(nINIT)

struct vpo_sense {
	struct scsi_sense cmd;
	unsigned int stat;
	unsigned int count;
};

struct vpo_data {
	unsigned short vpo_unit;

	int vpo_stat;
	int vpo_count;
	int vpo_error;

	struct ppb_status vpo_status;
	struct vpo_sense vpo_sense;

	unsigned char vpo_buffer[VP0_BUFFER_SIZE];

	struct ppb_device vpo_dev;
	struct scsi_link sc_link;
};

#endif
