/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2025, Broadcom Inc. All rights reserved.
 * Support: <fbsd-storage-driver.pdl@broadcom.com>
 *
 * Authors: Sumit Saxena <sumit.saxena@broadcom.com>
 *	    Chandrakanth Patil <chandrakanth.patil@broadcom.com>
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
 */

#include "mpi3mr.h"

#ifndef _MPI3MR_APP_H_
#define	_MPI3MR_APP_H_

#define MPI3MR_IOCTL_ADPTYPE_AVGFAMILY  1
#define MPI3MR_IOCTL_VERSION		0x06

#define MPI3MRDRVCMD    _IOWR('B', 1, struct mpi3mr_ioctl_drvcmd)
#define MPI3MRMPTCMD    _IOWR('B', 2, struct mpi3mr_ioctl_mptcmd)

#define MPI3MR_IOCTL_DEFAULT_TIMEOUT	(10)
#define PEND_IOCTLS_COMP_WAIT_TIME	(10)

#define MPI3MR_IOCTL_LOGDATA_MAX_ENTRIES	400
#define MPI3MR_IOCTL_LOGDATA_ENTRY_HEADER_SZ	0x5

#define GET_IOC_STATUS(ioc_status)	\
	ioc_status & MPI3_IOCSTATUS_STATUS_MASK

/* Encapsulated NVMe command definitions */
#define	MPI3MR_NVME_PRP_SIZE		8
#define	MPI3MR_NVME_CMD_PRP1_OFFSET	24
#define	MPI3MR_NVME_CMD_PRP2_OFFSET	32
#define	MPI3MR_NVME_CMD_SGL_OFFSET	24
#define MPI3MR_NVME_DATA_FORMAT_PRP	0
#define MPI3MR_NVME_DATA_FORMAT_SGL1	1
#define MPI3MR_NVME_DATA_FORMAT_SGL2	2

#define MPI3MR_NVMESGL_DATA_SEGMENT	0x00
#define MPI3MR_NVMESGL_LAST_SEGMENT	0x03

int mpi3mr_app_attach(struct mpi3mr_softc *);
void mpi3mr_app_detach(struct mpi3mr_softc *);

enum mpi3mr_ioctl_adp_state {
	MPI3MR_IOCTL_ADP_STATE_UNKNOWN		= 0,
	MPI3MR_IOCTL_ADP_STATE_OPERATIONAL	= 1,
	MPI3MR_IOCTL_ADP_STATE_FAULT		= 2,
	MPI3MR_IOCTL_ADP_STATE_IN_RESET		= 3,
	MPI3MR_IOCTL_ADP_STATE_UNRECOVERABLE	= 4,
};

enum mpi3mr_ioctl_data_dir {
	MPI3MR_APP_DDN,
	MPI3MR_APP_DDI,
	MPI3MR_APP_DDO,
};

enum mpi3mr_ioctl_drvcmds_opcode {
	MPI3MR_DRVRIOCTL_OPCODE_UNKNOWN		= 0,
	MPI3MR_DRVRIOCTL_OPCODE_ADPINFO		= 1,
	MPI3MR_DRVRIOCTL_OPCODE_ADPRESET	= 2,
	MPI3MR_DRVRIOCTL_OPCODE_TGTDEVINFO	= 3,
	MPI3MR_DRVRIOCTL_OPCODE_ALLTGTDEVINFO	= 4,
	MPI3MR_DRVRIOCTL_OPCODE_GETCHGCNT	= 5,
	MPI3MR_DRVRIOCTL_OPCODE_LOGDATAENABLE	= 6,
	MPI3MR_DRVRIOCTL_OPCODE_PELENABLE	= 7,
	MPI3MR_DRVRIOCTL_OPCODE_GETLOGDATA	= 8,
	MPI3MR_DRVRIOCTL_OPCODE_GETPCIINFO	= 100,
};

enum mpi3mr_ioctl_mpibuffer_type {
	MPI3MR_IOCTL_BUFTYPE_UNKNOWN,
	MPI3MR_IOCTL_BUFTYPE_RAIDMGMT_CMD,
	MPI3MR_IOCTL_BUFTYPE_RAIDMGMT_RESP,
	MPI3MR_IOCTL_BUFTYPE_DATA_IN,
	MPI3MR_IOCTL_BUFTYPE_DATA_OUT,
	MPI3MR_IOCTL_BUFTYPE_MPI_REPLY,
	MPI3MR_IOCTL_BUFTYPE_ERR_RESPONSE,
};

enum mpi3mr_ioctl_mpireply_type {
	MPI3MR_IOCTL_MPI_REPLY_BUFTYPE_UNKNOWN,
	MPI3MR_IOCTL_MPI_REPLY_BUFTYPE_STATUS,
	MPI3MR_IOCTL_MPI_REPLY_BUFTYPE_ADDRESS,
};

enum  mpi3mr_ioctl_reset_type {
	MPI3MR_IOCTL_ADPRESET_UNKNOWN,
	MPI3MR_IOCTL_ADPRESET_SOFT,		
	MPI3MR_IOCTL_ADPRESET_DIAG_FAULT,
};

struct mpi3mr_ioctl_drvcmd {
        U8 mrioc_id;
        U8 opcode;
        U16 rsvd1;
        U32 rsvd2;
        void *data_in_buf;
        void *data_out_buf;
        U32 data_in_size;
        U32 data_out_size;
};

struct mpi3mr_ioctl_adpinfo {
	U32 adp_type;
	U32 rsvd1;
	U32 pci_dev_id;
	U32 pci_dev_hw_rev;
	U32 pci_subsys_dev_id;
	U32 pci_subsys_ven_id;
	U32 pci_dev:5;
	U32 pci_func:3;
	U32 pci_bus:8;
	U32 rsvd2:16;
	U32 pci_seg_id;
	U32 ioctl_ver;
	U8 adp_state;
	U8 rsvd3;
	U16 rsvd4;
	U32 rsvd5[2];
	Mpi3DriverInfoLayout_t driver_info;
};

struct mpi3mr_ioctl_pciinfo {
	U32	config_space[64];
};

struct mpi3mr_ioctl_tgtinfo {
        U32 target_id;
        U8 bus_id;
        U8 rsvd1;
        U16 rsvd2;
        U16 dev_handle;
        U16 persistent_id;
        U32 seq_num;
};

struct mpi3mr_device_map_info {
	U16 handle;
	U16 per_id;
	U32 target_id;
	U8 bus_id;
	U8 rsvd1;
	U16 rsvd2;
};

struct mpi3mr_ioctl_all_tgtinfo {
	U16 num_devices;
	U16 rsvd1;
        U32 rsvd2;
	struct mpi3mr_device_map_info dmi[1];
};

struct mpi3mr_ioctl_chgcnt {
	U16 change_count;
	U16 rsvd;
};

struct mpi3mr_ioctl_adpreset {
	U8 reset_type;
	U8 rsvd1;
	U16 rsvd2;
};

struct mpi3mr_ioctl_mptcmd {
        U8 mrioc_id;
        U8 rsvd1;       
        U16 timeout;
        U16 rsvd2;          
        U16 mpi_msg_size;
        void *mpi_msg_buf;
        void *buf_entry_list;
        U32 buf_entry_list_size;
};

struct mpi3mr_buf_entry {
	U8 buf_type;
	U8 rsvd1;
	U16 rsvd2;
	U32 buf_len;
	void *buffer;
};

struct mpi3mr_ioctl_buf_entry_list {
	U8 num_of_buf_entries;
	U8 rsvd1;
	U16 rsvd2;
	U32 rsvd3;
	struct mpi3mr_buf_entry buf_entry[1];
};

struct mpi3mr_ioctl_mpt_dma_buffer {
	void *user_buf;
	void *kern_buf;
	U32 user_buf_len;
	U32 kern_buf_len;
	bus_addr_t kern_buf_dma;
	bus_dma_tag_t kern_buf_dmatag;
	bus_dmamap_t kern_buf_dmamap;
	U8 data_dir;
	U16 num_dma_desc;
	struct dma_memory_desc *dma_desc;
};

struct mpi3mr_ioctl_mpirepbuf {
	U8 mpirep_type;
	U8 rsvd1;
	U16 rsvd2;
	U8 repbuf[1];
};

struct mpi3mr_nvme_pt_sge {
	U64 base_addr;
	U32 length;
	U16 rsvd;
	U8 rsvd1;
	U8 sub_type:4;
	U8 type:4;
};

struct mpi3mr_log_data_entry {
	U8 valid_entry;
	U8 rsvd1;
	U16 rsvd2;
	U8 data[1];
};

struct mpi3mr_ioctl_logdata_enable {
	U16 max_entries;
	U16 rsvd;
};

struct mpi3mr_ioctl_pel_enable {
	U16 pel_locale;
	U8 pel_class;
	U8 rsvd;
};

int
mpi3mr_pel_abort(struct mpi3mr_softc *sc);
void
mpi3mr_pel_getseq_complete(struct mpi3mr_softc *sc,
			   struct mpi3mr_drvr_cmd *drvr_cmd);
void
mpi3mr_issue_pel_wait(struct mpi3mr_softc *sc,
		     struct mpi3mr_drvr_cmd *drvr_cmd);
void
mpi3mr_pel_wait_complete(struct mpi3mr_softc *sc,
			 struct mpi3mr_drvr_cmd *drvr_cmd);
void 
mpi3mr_send_pel_getseq(struct mpi3mr_softc *sc,
		       struct mpi3mr_drvr_cmd *drvr_cmd);
void
mpi3mr_app_send_aen(struct mpi3mr_softc *sc);

#endif /* !_MPI3MR_API_H_ */
