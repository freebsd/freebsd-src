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

#ifndef _MPI3MRVAR_H
#define _MPI3MRVAR_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/selinfo.h>
#include <sys/poll.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/endian.h>
#include <sys/sysent.h>
#include <sys/taskqueue.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <cam/scsi/smp_all.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include "mpi/mpi30_api.h"

#define MPI3MR_DRIVER_VERSION	"8.14.0.2.0"
#define MPI3MR_DRIVER_RELDATE	"9th Apr 2025"

#define MPI3MR_DRIVER_NAME	"mpi3mr"

#define MPI3MR_NAME_LENGTH	32
#define IOCNAME			"%s: "

#define MPI3MR_DEFAULT_MAX_IO_SIZE	(1 * 1024 * 1024)

#define SAS4116_CHIP_REV_A0	0
#define SAS4116_CHIP_REV_B0	1

#define MPI3MR_MAX_SECTORS	2048
#define MPI3MR_MAX_CMDS_LUN	7
#define MPI3MR_MAX_CDB_LENGTH	16
#define MPI3MR_MAX_LUN 		16895

#define MPI3MR_SATA_QDEPTH	32
#define MPI3MR_SAS_QDEPTH	64
#define MPI3MR_RAID_QDEPTH	128
#define MPI3MR_NVME_QDEPTH	128

/* Definitions for internal SGL and Chain SGL buffers */
#define MPI3MR_4K_PGSZ 		4096
#define MPI3MR_PAGE_SIZE_4K		4096
#define MPI3MR_DEFAULT_SGL_ENTRIES	256
#define MPI3MR_MAX_SGL_ENTRIES		2048

#define MPI3MR_AREQQ_SIZE	(2 * MPI3MR_4K_PGSZ)
#define MPI3MR_AREPQ_SIZE	(4 * MPI3MR_4K_PGSZ)
#define MPI3MR_AREQ_FRAME_SZ	128
#define MPI3MR_AREP_FRAME_SZ	16

#define MPI3MR_OPREQQ_SIZE	(8 * MPI3MR_4K_PGSZ)
#define MPI3MR_OPREPQ_SIZE	(4 * MPI3MR_4K_PGSZ)

/* Operational queue management definitions */
#define MPI3MR_OP_REQ_Q_QD		512
#define MPI3MR_OP_REP_Q_QD		1024
#define MPI3MR_OP_REP_Q_QD_A0		4096

#define MPI3MR_THRESHOLD_REPLY_COUNT	100

#define MPI3MR_SGEFLAGS_SYSTEM_SIMPLE_END_OF_LIST	\
	(MPI3_SGE_FLAGS_ELEMENT_TYPE_SIMPLE | MPI3_SGE_FLAGS_DLAS_SYSTEM | \
	 MPI3_SGE_FLAGS_END_OF_LIST)

#define MPI3MR_HOSTTAG_INVALID          0xFFFF
#define MPI3MR_HOSTTAG_INITCMDS         1
#define MPI3MR_HOSTTAG_IOCTLCMDS        2
#define MPI3MR_HOSTTAG_PELABORT         3
#define MPI3MR_HOSTTAG_PELWAIT          4
#define MPI3MR_HOSTTAG_TMS		5
#define MPI3MR_HOSTTAG_CFGCMDS		6

#define MAX_MGMT_ADAPTERS 8
#define MPI3MR_WAIT_BEFORE_CTRL_RESET 5

#define MPI3MR_RESET_REASON_OSTYPE_FREEBSD        0x4
#define MPI3MR_RESET_REASON_OSTYPE_SHIFT	  28
#define MPI3MR_RESET_REASON_IOCNUM_SHIFT          20

struct mpi3mr_mgmt_info {
	uint16_t count;
	struct mpi3mr_softc *sc_ptr[MAX_MGMT_ADAPTERS];
	int max_index;
};

extern char fmt_os_ver[16];

#define MPI3MR_OS_VERSION(raw_os_ver, fmt_os_ver)	sprintf(raw_os_ver, "%d", __FreeBSD_version); \
							sprintf(fmt_os_ver, "%c%c.%c%c.%c%c%c",\
								raw_os_ver[0], raw_os_ver[1], raw_os_ver[2],\
								raw_os_ver[3], raw_os_ver[4], raw_os_ver[5],\
								raw_os_ver[6]);
#define MPI3MR_NUM_DEVRMCMD             1
#define MPI3MR_HOSTTAG_DEVRMCMD_MIN     (MPI3MR_HOSTTAG_CFGCMDS + 1)
#define MPI3MR_HOSTTAG_DEVRMCMD_MAX     (MPI3MR_HOSTTAG_DEVRMCMD_MIN + \
                                                MPI3MR_NUM_DEVRMCMD - 1)
#define MPI3MR_INTERNALCMDS_RESVD       MPI3MR_HOSTTAG_DEVRMCMD_MAX

#define MPI3MR_NUM_EVTACKCMD		4
#define MPI3MR_HOSTTAG_EVTACKCMD_MIN	(MPI3MR_HOSTTAG_DEVRMCMD_MAX + 1)
#define MPI3MR_HOSTTAG_EVTACKCMD_MAX	(MPI3MR_HOSTTAG_EVTACKCMD_MIN + \
						MPI3MR_NUM_EVTACKCMD - 1)

/* command/controller interaction timeout definitions in seconds */
#define MPI3MR_INTADMCMD_TIMEOUT		60
#define MPI3MR_PORTENABLE_TIMEOUT		300
#define MPI3MR_ABORTTM_TIMEOUT			60
#define MPI3MR_RESETTM_TIMEOUT			60
#define MPI3MR_TSUPDATE_INTERVAL		900
#define MPI3MR_DEFAULT_SHUTDOWN_TIME		120
#define	MPI3MR_RAID_ERRREC_RESET_TIMEOUT	180
#define	MPI3MR_RESET_HOST_IOWAIT_TIMEOUT	5
#define	MPI3MR_PREPARE_FOR_RESET_TIMEOUT	180
#define MPI3MR_RESET_ACK_TIMEOUT		30
#define MPI3MR_MUR_TIMEOUT			120

#define MPI3MR_CMD_NOTUSED	0x8000
#define MPI3MR_CMD_COMPLETE	0x0001
#define MPI3MR_CMD_PENDING	0x0002
#define MPI3MR_CMD_REPLYVALID	0x0004
#define MPI3MR_CMD_RESET	0x0008

#define MPI3MR_NUM_EVTREPLIES	64
#define MPI3MR_SENSEBUF_SZ	256
#define MPI3MR_SENSEBUF_FACTOR	3
#define MPI3MR_CHAINBUF_FACTOR	3

#define MPT3SAS_HOSTPGSZ_4KEXP 12

#define MPI3MR_INVALID_DEV_HANDLE 0xFFFF

/* Controller Reset related definitions */
#define MPI3MR_HOSTDIAG_UNLOCK_RETRY_COUNT	5
#define MPI3MR_MAX_SHUTDOWN_RETRY_COUNT		2

/* ResponseCode values */
#define MPI3MR_RI_MASK_RESPCODE		(0x000000FF)
#define MPI3MR_RSP_TM_COMPLETE		0x00
#define MPI3MR_RSP_INVALID_FRAME	0x02
#define MPI3MR_RSP_TM_NOT_SUPPORTED	0x04
#define MPI3MR_RSP_TM_FAILED		0x05
#define MPI3MR_RSP_TM_SUCCEEDED		0x08
#define MPI3MR_RSP_TM_INVALID_LUN	0x09
#define MPI3MR_RSP_TM_OVERLAPPED_TAG	0x0A
#define MPI3MR_RSP_IO_QUEUED_ON_IOC \
			MPI3_SCSITASKMGMT_RSPCODE_IO_QUEUED_ON_IOC

/* Definitions for the controller security status*/
#define MPI3MR_CTLR_SECURITY_STATUS_MASK        0x0C
#define MPI3MR_CTLR_SECURE_DBG_STATUS_MASK      0x02

#define MPI3MR_INVALID_DEVICE                   0x00
#define MPI3MR_CONFIG_SECURE_DEVICE             0x04
#define MPI3MR_HARD_SECURE_DEVICE               0x08
#define MPI3MR_TAMPERED_DEVICE			0x0C 

#define MPI3MR_DEFAULT_MDTS	(128 * 1024)
#define MPI3MR_DEFAULT_PGSZEXP	(12)
#define MPI3MR_MAX_IOCTL_TRANSFER_SIZE (1024 * 1024)

#define MPI3MR_DEVRMHS_RETRYCOUNT 3
#define MPI3MR_PELCMDS_RETRYCOUNT 3

#define MPI3MR_PERIODIC_DELAY	1	/* 1 second heartbeat/watchdog check */

#define	WRITE_SAME_32	0x0d

#define MPI3MR_TSUPDATE_INTERVAL	900

struct completion {
	unsigned int done;
	struct mtx lock;
};

typedef union {
	volatile unsigned int val;
	unsigned int val_rdonly;
} mpi3mr_atomic_t;

#define	mpi3mr_atomic_read(v)	atomic_load_acq_int(&(v)->val)
#define	mpi3mr_atomic_set(v,i)	atomic_store_rel_int(&(v)->val, i)
#define	mpi3mr_atomic_dec(v)	atomic_subtract_int(&(v)->val, 1)
#define	mpi3mr_atomic_inc(v)	atomic_add_int(&(v)->val, 1)
#define	mpi3mr_atomic_add(v, u)	atomic_add_int(&(v)->val, u)
#define	mpi3mr_atomic_sub(v, u)	atomic_subtract_int(&(v)->val, u)

/* IOCTL data transfer sge*/
#define MPI3MR_NUM_IOCTL_SGE		256
#define MPI3MR_IOCTL_SGE_SIZE		(8 * 1024)

struct dma_memory_desc {
	U32 size;
	void *addr;
	bus_dma_tag_t tag;
	bus_dmamap_t dmamap;
	bus_addr_t dma_addr;
};

enum mpi3mr_iocstate {
        MRIOC_STATE_READY = 1,
        MRIOC_STATE_RESET,
        MRIOC_STATE_FAULT,
        MRIOC_STATE_BECOMING_READY,
        MRIOC_STATE_RESET_REQUESTED,
        MRIOC_STATE_UNRECOVERABLE,
        MRIOC_STATE_COUNT,
};

/* Init type definitions */
enum mpi3mr_init_type {
	MPI3MR_INIT_TYPE_INIT = 0,
	MPI3MR_INIT_TYPE_RESET,
	MPI3MR_INIT_TYPE_RESUME,
};

/* Reset reason code definitions*/
enum mpi3mr_reset_reason {
	MPI3MR_RESET_FROM_BRINGUP = 1,
	MPI3MR_RESET_FROM_FAULT_WATCH = 2,
	MPI3MR_RESET_FROM_IOCTL = 3,
	MPI3MR_RESET_FROM_EH_HOS = 4,
	MPI3MR_RESET_FROM_TM_TIMEOUT = 5,
	MPI3MR_RESET_FROM_IOCTL_TIMEOUT = 6,
	MPI3MR_RESET_FROM_MUR_FAILURE = 7,
	MPI3MR_RESET_FROM_CTLR_CLEANUP = 8,
	MPI3MR_RESET_FROM_CIACTIV_FAULT = 9,
	MPI3MR_RESET_FROM_PE_TIMEOUT = 10,
	MPI3MR_RESET_FROM_TSU_TIMEOUT = 11,
	MPI3MR_RESET_FROM_DELREQQ_TIMEOUT = 12,
	MPI3MR_RESET_FROM_DELREPQ_TIMEOUT = 13,
	MPI3MR_RESET_FROM_CREATEREPQ_TIMEOUT = 14,
	MPI3MR_RESET_FROM_CREATEREQQ_TIMEOUT = 15,
	MPI3MR_RESET_FROM_IOCFACTS_TIMEOUT = 16,
	MPI3MR_RESET_FROM_IOCINIT_TIMEOUT = 17,
	MPI3MR_RESET_FROM_EVTNOTIFY_TIMEOUT = 18,
	MPI3MR_RESET_FROM_EVTACK_TIMEOUT = 19,
	MPI3MR_RESET_FROM_CIACTVRST_TIMER = 20,
	MPI3MR_RESET_FROM_GETPKGVER_TIMEOUT = 21,
	MPI3MR_RESET_FROM_PELABORT_TIMEOUT = 22,
	MPI3MR_RESET_FROM_SYSFS = 23,
	MPI3MR_RESET_FROM_SYSFS_TIMEOUT = 24,
	MPI3MR_RESET_FROM_DIAG_BUFFER_POST_TIMEOUT = 25,
	MPI3MR_RESET_FROM_SCSIIO_TIMEOUT = 26,
	MPI3MR_RESET_FROM_FIRMWARE = 27,
	MPI3MR_DEFAULT_RESET_REASON = 28,
	MPI3MR_RESET_FROM_CFG_REQ_TIMEOUT = 29,
	MPI3MR_RESET_REASON_COUNT,
};

struct mpi3mr_compimg_ver
{
        U16 build_num;
        U16 cust_id;
        U8 ph_minor;
        U8 ph_major;
        U8 gen_minor;
        U8 gen_major;
};

struct mpi3mr_ioc_facts
{
        U32 ioc_capabilities;
        struct mpi3mr_compimg_ver fw_ver;
        U32 mpi_version;
        U16 max_reqs;
        U16 product_id;
        U16 op_req_sz;
	U16 reply_sz;
        U16 exceptions;
        U16 max_perids;
        U16 max_pds;
        U16 max_sasexpanders;
        U32 max_data_length;
        U16 max_sasinitiators;
        U16 max_enclosures;
        U16 max_pcieswitches;
        U16 max_nvme;
        U16 max_vds;
        U16 max_hpds;
        U16 max_advhpds;
        U16 max_raidpds;
        U16 min_devhandle;
        U16 max_devhandle;
	U16 max_op_req_q;
	U16 max_op_reply_q;
        U16 shutdown_timeout;
        U8 ioc_num;
        U8 who_init;
	U16 max_msix_vectors;
        U8 personality;
	U8 dma_mask;
        U8 protocol_flags;
        U8 sge_mod_mask;
        U8 sge_mod_value;
        U8 sge_mod_shift;
	U8 max_dev_per_tg;
	U16 max_io_throttle_group;
	U16 io_throttle_data_length;
	U16 io_throttle_low;
	U16 io_throttle_high;
};

struct mpi3mr_op_req_queue {
	U16 ci;
	U16 pi;
	U16 num_reqs;
	U8  qid;
	U8  reply_qid;
	U32 qsz;
	void *q_base;
	bus_dma_tag_t q_base_tag;
	bus_dmamap_t q_base_dmamap;
	bus_addr_t q_base_phys;
	struct mtx q_lock;
};

struct mpi3mr_op_reply_queue {
	U16 ci;
	U8 ephase;
	U8 qid;
	U16 num_replies;
	U32 qsz;
	bus_dma_tag_t q_base_tag;
	bus_dmamap_t q_base_dmamap;
	void *q_base;
	bus_addr_t q_base_phys;
	mpi3mr_atomic_t pend_ios;
	bool in_use;
	struct mtx q_lock;
};

struct irq_info {
	MPI3_REPLY_DESCRIPTORS_UNION	*post_queue;
	bus_dma_tag_t			buffer_dmat;
	struct resource			*irq;
	void				*intrhand;
	int				irq_rid;
};

struct mpi3mr_irq_context {
	struct mpi3mr_softc *sc;
	U16 msix_index;
	struct mpi3mr_op_reply_queue *op_reply_q;
	char name[MPI3MR_NAME_LENGTH];
	struct irq_info irq_info;
};

MALLOC_DECLARE(M_MPI3MR);
SYSCTL_DECL(_hw_mpi3mr);

typedef struct mpi3mr_drvr_cmd DRVR_CMD;
typedef void (*DRVR_CMD_CALLBACK)(struct mpi3mr_softc *mrioc, DRVR_CMD *drvrcmd);
struct mpi3mr_drvr_cmd {
	struct mtx lock;
	struct completion completion;
	void *reply;
	U8 *sensebuf;
	U8 iou_rc;
	U16 state;
	U16 dev_handle;
	U16 ioc_status;
	U32 ioc_loginfo;
	U8 is_waiting;
	U8 is_senseprst;
	U8 retry_count;
	U16 host_tag;
	DRVR_CMD_CALLBACK callback;
};

struct mpi3mr_cmd;
typedef void mpi3mr_evt_callback_t(struct mpi3mr_softc *, uintptr_t,
	Mpi3EventNotificationReply_t *reply);
typedef void mpi3mr_cmd_callback_t(struct mpi3mr_softc *,
	struct mpi3mr_cmd *cmd);

#define       MPI3MR_IOVEC_COUNT 2

enum mpi3mr_data_xfer_direction {
	MPI3MR_READ = 1,
	MPI3MR_WRITE,
};

enum mpi3mr_cmd_state {
	MPI3MR_CMD_STATE_FREE = 1,
	MPI3MR_CMD_STATE_BUSY,
	MPI3MR_CMD_STATE_IN_QUEUE,
	MPI3MR_CMD_STATE_IN_TM,
};

enum mpi3mr_target_state {
	MPI3MR_DEV_CREATED = 1,
	MPI3MR_DEV_REMOVE_HS_COMPLETED = 2,
};

struct mpi3mr_cmd {
	TAILQ_ENTRY(mpi3mr_cmd) 	next;
	struct mpi3mr_softc		*sc;
	union ccb			*ccb;
	void				*data;
	u_int				length;
	struct mpi3mr_target		*targ;
	u_int				data_dir;
	u_int				state;
	bus_dmamap_t			dmamap;
	struct scsi_sense_data		*sense;
	struct callout			callout;
	bool				callout_owner;
	U16				hosttag;
	U8				req_qidx;
	Mpi3SCSIIORequest_t		io_request;
};

struct mpi3mr_chain {
	bus_dmamap_t buf_dmamap;
	void *buf;
	bus_addr_t buf_phys;
};

struct mpi3mr_event_handle {
	TAILQ_ENTRY(mpi3mr_event_handle)	eh_list;
	mpi3mr_evt_callback_t		*callback;
	void				*data;
	uint8_t				mask[16];
};

struct mpi3mr_fw_event_work {
	U16			event;
	void			*event_data;
	TAILQ_ENTRY(mpi3mr_fw_event_work)	ev_link;
	U8			send_ack;
	U8			process_event;
	U32			event_context;
	U16			event_data_size;
};

/**
 * struct delayed_dev_rmhs_node - Delayed device removal node
 *
 * @list: list head
 * @handle: Device handle
 * @iou_rc: IO Unit Control Reason Code
 */
struct delayed_dev_rmhs_node {
	TAILQ_ENTRY(delayed_dev_rmhs_node) list;
	U16 handle;
	U8 iou_rc;
};

/**
 * struct delayed_evtack_node - Delayed event ack node
 *
 * @list: list head
 * @event: MPI3 event ID
 * @event_ctx: Event context
 */
struct delayed_evtack_node {
	TAILQ_ENTRY(delayed_evtack_node) list;
	U8 event;
	U32 event_ctx;
};

/* Reset types */
enum reset_type {
	MPI3MR_NO_RESET,
	MPI3MR_TRIGGER_SOFT_RESET,
};

struct mpi3mr_reset {
	u_int type;
	U32 reason;
	int status;
	bool ioctl_reset_snapdump;
};

struct mpi3mr_softc {
	device_t mpi3mr_dev;
	struct cdev *mpi3mr_cdev;
	u_int mpi3mr_flags;
#define MPI3MR_FLAGS_SHUTDOWN		(1 << 0)
#define MPI3MR_FLAGS_DIAGRESET		(1 << 1)
#define	MPI3MR_FLAGS_ATTACH_DONE	(1 << 2)
#define	MPI3MR_FLAGS_PORT_ENABLE_DONE	(1 << 3)
	U8 id;
	int cpu_count;
	char name[MPI3MR_NAME_LENGTH];
	char driver_name[MPI3MR_NAME_LENGTH];
	int bars;
	bus_addr_t dma_loaddr;
	bus_addr_t dma_hiaddr;
	u_int mpi3mr_debug;
	struct mpi3mr_reset reset;
	int max_msix_vectors;
	int msix_count;
	bool  msix_enable;
	int io_cmds_highwater;
	int max_chains;
	uint32_t chain_frame_size;
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
	char fw_version[32];
	struct mpi3mr_chain *chains;
	struct callout periodic;
	struct callout device_check_callout;

	struct mpi3mr_cam_softc	*cam_sc;
	struct mpi3mr_cmd **cmd_list;
	TAILQ_HEAD(, mpi3mr_cmd) cmd_list_head;
	struct mtx cmd_pool_lock;

	struct resource			*mpi3mr_regs_resource;
	bus_space_handle_t		mpi3mr_bhandle;
	bus_space_tag_t			mpi3mr_btag;
	int				mpi3mr_regs_rid;

	bus_dma_tag_t			mpi3mr_parent_dmat;
	bus_dma_tag_t			buffer_dmat;

	int				num_reqs;
	int				num_replies;
	int				num_chains;

	TAILQ_HEAD(, mpi3mr_event_handle)	event_list;
	struct mpi3mr_event_handle		*mpi3mr_log_eh;
	struct intr_config_hook		mpi3mr_ich;
	
	struct mtx mpi3mr_mtx;
	struct mtx io_lock;
	U8 intr_enabled;
	TAILQ_HEAD(, delayed_dev_rmhs_node) delayed_rmhs_list;
	TAILQ_HEAD(, delayed_evtack_node) delayed_evtack_cmds_list;

	U16 num_admin_reqs;
	U32 admin_req_q_sz;
	U16 admin_req_pi;
	U16 admin_req_ci;
	bus_dma_tag_t admin_req_tag;
	bus_dmamap_t admin_req_dmamap;
	bus_addr_t admin_req_phys;
	U8 *admin_req;
	struct mtx admin_req_lock;

	U16 num_admin_replies;
	U32 admin_reply_q_sz;
	U16 admin_reply_ci;
	U8 admin_reply_ephase;
	bus_dma_tag_t admin_reply_tag;
	bus_dmamap_t admin_reply_dmamap;
	bus_addr_t admin_reply_phys;
	U8 *admin_reply;
	struct mtx admin_reply_lock;
	bool admin_in_use;

	U32 num_reply_bufs;
	bus_dma_tag_t			reply_buf_tag;
	bus_dmamap_t			reply_buf_dmamap;
	bus_addr_t			reply_buf_phys;
	U8				*reply_buf;
	bus_addr_t			reply_buf_dma_max_address;
	bus_addr_t			reply_buf_dma_min_address;

	U16 reply_free_q_sz;
	bus_dma_tag_t			reply_free_q_tag;
	bus_dmamap_t			reply_free_q_dmamap;
	bus_addr_t			reply_free_q_phys;
	U64				*reply_free_q;
	struct mtx reply_free_q_lock;
	U32 reply_free_q_host_index;
	
	U32 num_sense_bufs;
	bus_dma_tag_t			sense_buf_tag;
	bus_dmamap_t			sense_buf_dmamap;
	bus_addr_t			sense_buf_phys;
	U8				*sense_buf;
	
	U16 sense_buf_q_sz;
	bus_dma_tag_t			sense_buf_q_tag;
	bus_dmamap_t			sense_buf_q_dmamap;
	bus_addr_t			sense_buf_q_phys;
	U64				*sense_buf_q;
	struct mtx sense_buf_q_lock;
	U32 sense_buf_q_host_index;

	void				*nvme_encap_prp_list;
	bus_addr_t			nvme_encap_prp_list_dma;
	bus_dma_tag_t			nvme_encap_prp_list_dmatag;
	bus_dmamap_t			nvme_encap_prp_list_dma_dmamap;
	U32 nvme_encap_prp_sz;
	
	U32 ready_timeout;

	struct mpi3mr_irq_context *irq_ctx;

	U16 num_queues;		/* Number of request/reply queues */
	struct mpi3mr_op_req_queue *op_req_q;
	struct mpi3mr_op_reply_queue *op_reply_q;
	U16 num_hosttag_op_req_q;

	struct mpi3mr_drvr_cmd init_cmds;
	struct mpi3mr_ioc_facts facts;
	U16 reply_sz;
	U16 op_reply_sz;

	U32 event_masks[MPI3_EVENT_NOTIFY_EVENTMASK_WORDS];

	char fwevt_worker_name[MPI3MR_NAME_LENGTH];
	struct workqueue_struct	*fwevt_worker_thread;
	struct mtx fwevt_lock;
	struct mtx target_lock;
	
	U16 max_host_ios;
	U32 max_sgl_entries;
	bus_dma_tag_t	chain_sgl_list_tag;
	struct mpi3mr_chain *chain_sgl_list;
	U16  chain_bitmap_sz;
	void *chain_bitmap;
	struct mtx chain_buf_lock;
	U16 chain_buf_count;

	struct mpi3mr_drvr_cmd ioctl_cmds;
	struct mpi3mr_drvr_cmd host_tm_cmds;
	struct mpi3mr_drvr_cmd dev_rmhs_cmds[MPI3MR_NUM_DEVRMCMD];
	struct mpi3mr_drvr_cmd evtack_cmds[MPI3MR_NUM_EVTACKCMD];
	struct mpi3mr_drvr_cmd cfg_cmds;

	U16 devrem_bitmap_sz;
	void *devrem_bitmap;

	U16 dev_handle_bitmap_sz;
	void *removepend_bitmap;

	U16 evtack_cmds_bitmap_sz;
	void *evtack_cmds_bitmap;

	U32 ts_update_counter;
	U8 reset_in_progress;
        U8 unrecoverable;
        U8 block_ioctls;
        U8 in_prep_ciactv_rst;
        U16 prep_ciactv_rst_counter;
        struct mtx reset_mutex;

	U8 prepare_for_reset;
	U16 prepare_for_reset_timeout_counter;

	U16 diagsave_timeout;
        int logging_level;
        U16 flush_io_count;

        Mpi3DriverInfoLayout_t driver_info;
        
	U16 change_count;
	
	U8 *log_data_buffer;
	U16 log_data_buffer_index;
	U16 log_data_entry_size;

        U8 pel_wait_pend;
        U8 pel_abort_requested;
        U8 pel_class;
        U16 pel_locale;
        
	struct mpi3mr_drvr_cmd pel_cmds;
        struct mpi3mr_drvr_cmd pel_abort_cmd;
        U32 newest_seqnum;
        void *pel_seq_number;
        bus_addr_t pel_seq_number_dma;
	bus_dma_tag_t pel_seq_num_dmatag;
	bus_dmamap_t pel_seq_num_dmamap;
        U32 pel_seq_number_sz;
	
	struct selinfo mpi3mr_select;
	U32 mpi3mr_poll_waiting;
	U32 mpi3mr_aen_triggered;

	U16 wait_for_port_enable;
	U16 track_mapping_events;
	U16 pending_map_events;
	mpi3mr_atomic_t fw_outstanding;
	mpi3mr_atomic_t pend_ioctls;
	struct proc *watchdog_thread;
	void   *watchdog_chan;
	void   *tm_chan;
	u_int8_t remove_in_progress;
	u_int8_t watchdog_thread_active;
	u_int8_t do_timedout_reset;
	bool allow_ios;
	bool secure_ctrl;
	mpi3mr_atomic_t pend_large_data_sz;
	
	u_int32_t io_throttle_data_length;
	u_int32_t io_throttle_high;
	u_int32_t io_throttle_low;
	u_int16_t num_io_throttle_group;
	u_int iot_enable;
	struct mpi3mr_throttle_group_info *throttle_groups;

	struct dma_memory_desc ioctl_sge[MPI3MR_NUM_IOCTL_SGE];
	struct dma_memory_desc ioctl_chain_sge;
	struct dma_memory_desc ioctl_resp_sge;
	bool ioctl_sges_allocated;
	struct proc *timestamp_thread_proc;
	void   *timestamp_chan;
	u_int8_t timestamp_thread_active;
	U32 ts_update_interval;
};

static __inline uint64_t
mpi3mr_regread64(struct mpi3mr_softc *sc, uint32_t offset)
{
	return bus_space_read_8(sc->mpi3mr_btag, sc->mpi3mr_bhandle, offset);
}

static __inline void
mpi3mr_regwrite64(struct mpi3mr_softc *sc, uint32_t offset, uint64_t val)
{
	bus_space_write_8(sc->mpi3mr_btag, sc->mpi3mr_bhandle, offset, val);
}

static __inline uint32_t
mpi3mr_regread(struct mpi3mr_softc *sc, uint32_t offset)
{
	return bus_space_read_4(sc->mpi3mr_btag, sc->mpi3mr_bhandle, offset);
}

static __inline void
mpi3mr_regwrite(struct mpi3mr_softc *sc, uint32_t offset, uint32_t val)
{
	bus_space_write_4(sc->mpi3mr_btag, sc->mpi3mr_bhandle, offset, val);
}

#define MPI3MR_INFO	(1 << 0)	/* Basic info */
#define MPI3MR_FAULT	(1 << 1)	/* Hardware faults */
#define MPI3MR_EVENT	(1 << 2)	/* Event data from the controller */
#define MPI3MR_LOG	(1 << 3)	/* Log data from the controller */
#define MPI3MR_RECOVERY	(1 << 4)	/* Command error recovery tracing */
#define MPI3MR_ERROR	(1 << 5)	/* Fatal driver/OS APIs failure */
#define MPI3MR_XINFO	(1 << 6)	/* Additional info logs*/
#define MPI3MR_TRACE	(1 << 7)	/* Trace functions */
#define MPI3MR_IOT	(1 << 8)	/* IO throttling related debugs */
#define MPI3MR_DEBUG_TM	(1 << 9)	/* Task management related debugs */
#define MPI3MR_DEBUG_IOCTL	(1 << 10)	/* IOCTL related debugs */

#define mpi3mr_printf(sc, args...)				\
	device_printf((sc)->mpi3mr_dev, ##args)

#define mpi3mr_print_field(sc, msg, args...)		\
	printf("\t" msg, ##args)

#define mpi3mr_vprintf(sc, args...)			\
do {							\
	if (bootverbose)				\
		mpi3mr_printf(sc, ##args);			\
} while (0)

#define mpi3mr_dprint(sc, level, msg, args...)		\
do {							\
	if ((sc)->mpi3mr_debug & (level))			\
		device_printf((sc)->mpi3mr_dev, msg, ##args);	\
} while (0)

#define MPI3MR_PRINTFIELD_START(sc, tag...)	\
	mpi3mr_printf((sc), ##tag);		\
	mpi3mr_print_field((sc), ":\n")
#define MPI3MR_PRINTFIELD_END(sc, tag)		\
	mpi3mr_printf((sc), tag "\n")
#define MPI3MR_PRINTFIELD(sc, facts, attr, fmt)	\
	mpi3mr_print_field((sc), #attr ": " #fmt "\n", (facts)->attr)

#define mpi3mr_kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg) \
    kproc_create(func, farg, proc_ptr, flags, stackpgs, fmtstr, arg)
#define mpi3mr_kproc_exit(arg)	kproc_exit(arg)

#if defined(CAM_PRIORITY_XPT)
#define MPI3MR_PRIORITY_XPT	CAM_PRIORITY_XPT
#else
#define MPI3MR_PRIORITY_XPT	5
#endif

static __inline void
mpi3mr_clear_bit(int b, volatile void *p)
{
	atomic_clear_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline void
mpi3mr_set_bit(int b, volatile void *p)
{
	atomic_set_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
}

static __inline int
mpi3mr_test_bit(int b, volatile void *p)
{
	return ((volatile int *)p)[b >> 5] & (1 << (b & 0x1f));
}

static __inline int
mpi3mr_test_and_set_bit(int b, volatile void *p)
{
	int ret = ((volatile int *)p)[b >> 5] & (1 << (b & 0x1f));

	atomic_set_int(((volatile int *)p) + (b >> 5), 1 << (b & 0x1f));
	return ret;
}

static __inline int
mpi3mr_find_first_zero_bit(void *p, int bit_count)
{
	int i, sz, j=0;
	U8 *loc;

	sz = bit_count % 8 ? (bit_count / 8 + 1) : (bit_count / 8);
	loc = malloc(sz, M_MPI3MR, M_NOWAIT | M_ZERO);

	memcpy(loc, p, sz);

	for (i = 0; i < sz; i++) {
		j = 0;
		while (j < 8) {
			if (!((loc[i] >> j) & 0x1))
				goto out;
			j++;
		}
	}
out:
	free(loc, M_MPI3MR);
	return (i + j);
}

#define MPI3MR_DIV_ROUND_UP(n,d)       (((n) + (d) - 1) / (d))

void
init_completion(struct completion *completion);

void
complete(struct completion *completion);

void wait_for_completion_timeout(struct completion *completion,
	    U32 timeout);
void wait_for_completion_timeout_tm(struct completion *completion,
	    U32 timeout, struct mpi3mr_softc *sc);
void mpi3mr_add_sg_single(void *paddr, U8 flags, U32 length,
    bus_addr_t dma_addr);
void mpi3mr_enable_interrupts(struct mpi3mr_softc *sc);
void mpi3mr_disable_interrupts(struct mpi3mr_softc *sc);
void mpi3mr_memaddr_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error);
int mpi3mr_submit_admin_cmd(struct mpi3mr_softc *mrioc, void *admin_req,
    U16 admin_req_sz);
int mpi3mr_submit_io(struct mpi3mr_softc *mrioc,
    struct mpi3mr_op_req_queue *op_req_q, U8 *req);
int
mpi3mr_alloc_interrupts(struct mpi3mr_softc *sc, U16 setup_one);

void mpi3mr_cleanup_ioc(struct mpi3mr_softc *sc);
int mpi3mr_initialize_ioc(struct mpi3mr_softc *sc, U8 reason);
void mpi3mr_build_zero_len_sge(void *paddr);
int mpi3mr_issue_event_notification(struct mpi3mr_softc *sc);
int
mpi3mr_register_events(struct mpi3mr_softc *sc);
void mpi3mr_process_op_reply_desc(struct mpi3mr_softc *sc,
    Mpi3DefaultReplyDescriptor_t *reply_desc, U64 *reply_dma);
struct mpi3mr_cmd *
mpi3mr_get_command(struct mpi3mr_softc *sc);
void
mpi3mr_release_command(struct mpi3mr_cmd *cmd);
int
mpi3mr_complete_io_cmd(struct mpi3mr_softc *sc,
    struct mpi3mr_irq_context *irq_context);
int
mpi3mr_cam_detach(struct mpi3mr_softc *sc);
int
mpi3mr_cam_attach(struct mpi3mr_softc *sc);
struct mpi3mr_target *
mpi3mr_find_target_by_per_id(struct mpi3mr_cam_softc *cam_sc,
    uint16_t per_id);
struct mpi3mr_target *
mpi3mr_find_target_by_dev_handle(struct mpi3mr_cam_softc *cam_sc,
    uint16_t dev_handle);
int mpi3mr_create_device(struct mpi3mr_softc *sc,
    Mpi3DevicePage0_t *dev_pg0);
void
mpi3mr_unmap_request(struct mpi3mr_softc *sc, struct mpi3mr_cmd *cmd);
void
init_completion(struct completion *completion);
void
complete(struct completion *completion);
void wait_for_completion_timeout(struct completion *completion,
	    U32 timeout);
void
poll_for_command_completion(struct mpi3mr_softc *sc,
       struct mpi3mr_drvr_cmd *cmd, U16 wait);
int
mpi3mr_alloc_requests(struct mpi3mr_softc *sc);
void
mpi3mr_watchdog(void *arg);
int mpi3mr_issue_port_enable(struct mpi3mr_softc *mrioc, U8 async);
void
mpi3mr_isr(void *privdata);
int
mpi3mr_alloc_msix_queues(struct mpi3mr_softc *sc);
void
mpi3mr_destory_mtx(struct mpi3mr_softc *sc);
void
mpi3mr_free_mem(struct mpi3mr_softc *sc);
void
mpi3mr_cleanup_interrupts(struct mpi3mr_softc *sc);
int mpi3mr_setup_irqs(struct mpi3mr_softc *sc);
void mpi3mr_cleanup_event_taskq(struct mpi3mr_softc *sc);
void
mpi3mr_hexdump(void *buf, int sz, int format);
int mpi3mr_soft_reset_handler(struct mpi3mr_softc *sc,
	U16 reset_reason, bool snapdump);
void
mpi3mrsas_release_simq_reinit(struct mpi3mr_cam_softc *cam_sc);
void
mpi3mr_watchdog_thread(void *arg);
void mpi3mr_timestamp_thread(void *arg);
void mpi3mr_add_device(struct mpi3mr_softc *sc, U16 per_id);
int mpi3mr_remove_device(struct mpi3mr_softc *sc, U16 handle);
int
mpi3mrsas_register_events(struct mpi3mr_softc *sc);
int mpi3mr_process_event_ack(struct mpi3mr_softc *sc, U8 event,
	U32 event_ctx);
int mpi3mr_remove_device_from_os(struct mpi3mr_softc *sc, U16 handle);
void mpi3mr_remove_device_from_list(struct mpi3mr_softc *sc, struct mpi3mr_target *target,
				    bool must_delete);
void mpi3mr_update_device(struct mpi3mr_softc *mrioc,
    struct mpi3mr_target *tgtdev, Mpi3DevicePage0_t *dev_pg0, bool is_added);
void mpi3mr_app_save_logdata(struct mpi3mr_softc *sc, char *event_data, U16 event_data_size);
void mpi3mr_set_io_divert_for_all_vd_in_tg(struct mpi3mr_softc *sc,
	struct mpi3mr_throttle_group_info *tg, U8 divert_value);
enum mpi3mr_iocstate mpi3mr_get_iocstate(struct mpi3mr_softc *sc);
void mpi3mr_poll_pend_io_completions(struct mpi3mr_softc *sc);
void int_to_lun(unsigned int lun, U8 *req_lun);
void trigger_reset_from_watchdog(struct mpi3mr_softc *sc, U8 reset_type, U16 reset_reason);
void mpi3mr_alloc_ioctl_dma_memory(struct mpi3mr_softc *sc);
int mpi3mr_cfg_get_driver_pg1(struct mpi3mr_softc *sc);
#endif /*MPI3MR_H_INCLUDED*/
