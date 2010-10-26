/*-
 * Copyright (c) 2008 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * LSI MPT-Fusion Host Adapter FreeBSD userland interface
 *
 * $FreeBSD$
 */

#ifndef _MPS_IOCTL_H_
#define	_MPS_IOCTL_H_

#include <dev/mps/mpi/mpi2_type.h>
#include <dev/mps/mpi/mpi2.h>
#include <dev/mps/mpi/mpi2_cnfg.h>
#include <dev/mps/mpi/mpi2_sas.h>

/*
 * For the read header requests, the header should include the page
 * type or extended page type, page number, and page version.  The
 * buffer and length are unused.  The completed header is returned in
 * the 'header' member.
 *
 * For the read page and write page requests, 'buf' should point to a
 * buffer of 'len' bytes which holds the entire page (including the
 * header).
 *
 * All requests specify the page address in 'page_address'.
 */
struct mps_cfg_page_req {	
	MPI2_CONFIG_PAGE_HEADER header;
	uint32_t page_address;
	void	*buf;
	int	len;
	uint16_t ioc_status;
};

struct mps_ext_cfg_page_req {
	MPI2_CONFIG_EXTENDED_PAGE_HEADER header;
	uint32_t page_address;
	void	*buf;
	int	len;
	uint16_t ioc_status;
};

struct mps_raid_action {
	uint8_t action;
	uint8_t volume_bus;
	uint8_t volume_id;
	uint8_t phys_disk_num;
	uint32_t action_data_word;
	void *buf;
	int len;
	uint32_t volume_status;
	uint32_t action_data[4];
	uint16_t action_status;
	uint16_t ioc_status;
	uint8_t write;
};

struct mps_usr_command {
	void *req;
	uint32_t req_len;
	void *rpl;
	uint32_t rpl_len;
	void *buf;
	int len;
	uint32_t flags;
};

#define MPSIO_MPS_COMMAND_FLAG_VERBOSE 0x01
#define MPSIO_MPS_COMMAND_FLAG_DEBUG 0x02
#define	MPSIO_READ_CFG_HEADER	_IOWR('M', 200, struct mps_cfg_page_req)
#define	MPSIO_READ_CFG_PAGE	_IOWR('M', 201, struct mps_cfg_page_req)
#define	MPSIO_READ_EXT_CFG_HEADER _IOWR('M', 202, struct mps_ext_cfg_page_req)
#define	MPSIO_READ_EXT_CFG_PAGE	_IOWR('M', 203, struct mps_ext_cfg_page_req)
#define	MPSIO_WRITE_CFG_PAGE	_IOWR('M', 204, struct mps_cfg_page_req)
#define	MPSIO_RAID_ACTION	_IOWR('M', 205, struct mps_raid_action)
#define	MPSIO_MPS_COMMAND	_IOWR('M', 210, struct mps_usr_command)

#endif /* !_MPS_IOCTL_H_ */
