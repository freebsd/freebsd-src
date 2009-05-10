/*
 * HighPoint RR3xxx/4xxx RAID Driver for FreeBSD
 * Copyright (C) 2007-2008 HighPoint Technologies, Inc. All Rights Reserved.
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
 */

#ifndef _HPTIOP_H
#define _HPTIOP_H

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define DBG 0

#ifdef DBG
int hpt_iop_dbg_level = 0;
#define KdPrint(x)  do { if (hpt_iop_dbg_level) printf x; } while (0)
#define HPT_ASSERT(x) assert(x)
#else
#define KdPrint(x)
#define HPT_ASSERT(x)
#endif

#define HPT_SRB_MAX_REQ_SIZE                600
#define HPT_SRB_MAX_QUEUE_SIZE              0x100

/* beyond 64G mem */
#define HPT_SRB_FLAG_HIGH_MEM_ACESS         0x1
#define HPT_SRB_MAX_SIZE  ((sizeof(struct hpt_iop_srb) + 0x1f) & ~0x1f)
#ifndef offsetof
#define offsetof(TYPE, MEM) ((size_t)&((TYPE*)0)->MEM)
#endif

#ifndef MIN
#define MIN(a, b)		((a) < (b) ? (a) : (b))
#endif

#define HPT_IOCTL_MAGIC   0xA1B2C3D4
#define HPT_IOCTL_MAGIC32   0x1A2B3C4D

struct hpt_iopmu_itl {
	u_int32_t resrved0[4];
	u_int32_t inbound_msgaddr0;
	u_int32_t inbound_msgaddr1;
	u_int32_t outbound_msgaddr0;
	u_int32_t outbound_msgaddr1;
	u_int32_t inbound_doorbell;
	u_int32_t inbound_intstatus;
	u_int32_t inbound_intmask;
	u_int32_t outbound_doorbell;
	u_int32_t outbound_intstatus;
	u_int32_t outbound_intmask;
	u_int32_t reserved1[2];
	u_int32_t inbound_queue;
	u_int32_t outbound_queue;
};

#define IOPMU_QUEUE_EMPTY            0xffffffff
#define IOPMU_QUEUE_MASK_HOST_BITS   0xf0000000
#define IOPMU_QUEUE_ADDR_HOST_BIT    0x80000000
#define IOPMU_QUEUE_REQUEST_SIZE_BIT    0x40000000
#define IOPMU_QUEUE_REQUEST_RESULT_BIT   0x40000000
#define IOPMU_MAX_MEM_SUPPORT_MASK_64G 0xfffffff000000000ull
#define IOPMU_MAX_MEM_SUPPORT_MASK_32G 0xfffffff800000000ull

#define IOPMU_OUTBOUND_INT_MSG0      1
#define IOPMU_OUTBOUND_INT_MSG1      2
#define IOPMU_OUTBOUND_INT_DOORBELL  4
#define IOPMU_OUTBOUND_INT_POSTQUEUE 8
#define IOPMU_OUTBOUND_INT_PCI       0x10

#define IOPMU_INBOUND_INT_MSG0       1
#define IOPMU_INBOUND_INT_MSG1       2
#define IOPMU_INBOUND_INT_DOORBELL   4
#define IOPMU_INBOUND_INT_ERROR      8
#define IOPMU_INBOUND_INT_POSTQUEUE  0x10

#define MVIOP_QUEUE_LEN  512
struct hpt_iopmu_mv {
	u_int32_t inbound_head;
	u_int32_t inbound_tail;
	u_int32_t outbound_head;
	u_int32_t outbound_tail;
	u_int32_t inbound_msg;
	u_int32_t outbound_msg;
	u_int32_t reserve[10];
	u_int64_t inbound_q[MVIOP_QUEUE_LEN];
	u_int64_t outbound_q[MVIOP_QUEUE_LEN];
};

struct hpt_iopmv_regs {
	u_int32_t reserved[0x20400 / 4];
	u_int32_t inbound_doorbell;
	u_int32_t inbound_intmask;
	u_int32_t outbound_doorbell;
	u_int32_t outbound_intmask;
};

#define MVIOP_IOCTLCFG_SIZE	0x800
#define MVIOP_MU_QUEUE_ADDR_HOST_MASK   (~(0x1full))
#define MVIOP_MU_QUEUE_ADDR_HOST_BIT    4

#define MVIOP_MU_QUEUE_ADDR_IOP_HIGH32  0xffffffff
#define MVIOP_MU_QUEUE_REQUEST_RESULT_BIT   1
#define MVIOP_MU_QUEUE_REQUEST_RETURN_CONTEXT 2

#define MVIOP_MU_INBOUND_INT_MSG        1
#define MVIOP_MU_INBOUND_INT_POSTQUEUE  2
#define MVIOP_MU_OUTBOUND_INT_MSG       1
#define MVIOP_MU_OUTBOUND_INT_POSTQUEUE 2

#define MVIOP_CMD_TYPE_GET_CONFIG (1 << 5)
#define MVIOP_CMD_TYPE_SET_CONFIG (1 << 6)
#define MVIOP_CMD_TYPE_SCSI (1 << 7)
#define MVIOP_CMD_TYPE_IOCTL (1 << 8)
#define MVIOP_CMD_TYPE_BLOCK (1 << 9)

#define MVIOP_REQUEST_NUMBER_START_BIT 16

enum hpt_iopmu_message {
	/* host-to-iop messages */
	IOPMU_INBOUND_MSG0_NOP = 0,
	IOPMU_INBOUND_MSG0_RESET,
	IOPMU_INBOUND_MSG0_FLUSH,
	IOPMU_INBOUND_MSG0_SHUTDOWN,
	IOPMU_INBOUND_MSG0_STOP_BACKGROUND_TASK,
	IOPMU_INBOUND_MSG0_START_BACKGROUND_TASK,
	IOPMU_INBOUND_MSG0_MAX = 0xff,
	/* iop-to-host messages */
	IOPMU_OUTBOUND_MSG0_REGISTER_DEVICE_0 = 0x100,
	IOPMU_OUTBOUND_MSG0_REGISTER_DEVICE_MAX = 0x1ff,
	IOPMU_OUTBOUND_MSG0_UNREGISTER_DEVICE_0 = 0x200,
	IOPMU_OUTBOUND_MSG0_UNREGISTER_DEVICE_MAX = 0x2ff,
	IOPMU_OUTBOUND_MSG0_REVALIDATE_DEVICE_0 = 0x300,
	IOPMU_OUTBOUND_MSG0_REVALIDATE_DEVICE_MAX = 0x3ff,
};

#define IOP_REQUEST_FLAG_SYNC_REQUEST 1
#define IOP_REQUEST_FLAG_BIST_REQUEST 2
#define IOP_REQUEST_FLAG_REMAPPED     4
#define IOP_REQUEST_FLAG_OUTPUT_CONTEXT 8

enum hpt_iop_request_type {
	IOP_REQUEST_TYPE_GET_CONFIG = 0,
	IOP_REQUEST_TYPE_SET_CONFIG,
	IOP_REQUEST_TYPE_BLOCK_COMMAND,
	IOP_REQUEST_TYPE_SCSI_COMMAND,
	IOP_REQUEST_TYPE_IOCTL_COMMAND,
	IOP_REQUEST_TYPE_MAX
};

enum hpt_iop_result_type {
	IOP_RESULT_PENDING = 0,
	IOP_RESULT_SUCCESS,
	IOP_RESULT_FAIL,
	IOP_RESULT_BUSY,
	IOP_RESULT_RESET,
	IOP_RESULT_INVALID_REQUEST,
	IOP_RESULT_BAD_TARGET,
	IOP_RESULT_CHECK_CONDITION,
};

struct hpt_iop_request_header {
	u_int32_t size;
	u_int32_t type;
	u_int32_t flags;
	u_int32_t result;
	u_int64_t context; /* host context */
};

struct hpt_iop_request_get_config {
	struct hpt_iop_request_header header;
	u_int32_t interface_version;
	u_int32_t firmware_version;
	u_int32_t max_requests;
	u_int32_t request_size;
	u_int32_t max_sg_count;
	u_int32_t data_transfer_length;
	u_int32_t alignment_mask;
	u_int32_t max_devices;
	u_int32_t sdram_size;
};

struct hpt_iop_request_set_config {
	struct hpt_iop_request_header header;
	u_int32_t iop_id;
	u_int16_t vbus_id;
	u_int16_t max_host_request_size;
	u_int32_t reserve[6];
};

struct hpt_iopsg {
	u_int32_t size;
	u_int32_t eot; /* non-zero: end of table */
	u_int64_t pci_address;
};

#define IOP_BLOCK_COMMAND_READ     1
#define IOP_BLOCK_COMMAND_WRITE    2
#define IOP_BLOCK_COMMAND_VERIFY   3
#define IOP_BLOCK_COMMAND_FLUSH    4
#define IOP_BLOCK_COMMAND_SHUTDOWN 5
struct hpt_iop_request_block_command {
	struct hpt_iop_request_header header;
	u_int8_t     channel;
	u_int8_t     target;
	u_int8_t     lun;
	u_int8_t     pad1;
	u_int16_t    command; /* IOP_BLOCK_COMMAND_{READ,WRITE} */
	u_int16_t    sectors;
	u_int64_t    lba;
	struct hpt_iopsg sg_list[1];
};

struct hpt_iop_request_scsi_command {
	struct hpt_iop_request_header header;
	u_int8_t     channel;
	u_int8_t     target;
	u_int8_t     lun;
	u_int8_t     pad1;
	u_int8_t     cdb[16];
	u_int32_t    dataxfer_length;
	struct hpt_iopsg sg_list[1];
};

struct hpt_iop_request_ioctl_command {
	struct hpt_iop_request_header header;
	u_int32_t    ioctl_code;
	u_int32_t    inbuf_size;
	u_int32_t    outbuf_size;
	u_int32_t    bytes_returned;
	u_int8_t     buf[1];
	/* out data should be put at buf[(inbuf_size+3)&~3] */
};

struct hpt_iop_ioctl_param {
	u_int32_t        Magic;                 /* used to check if it's a valid ioctl packet */
	u_int32_t        dwIoControlCode;       /* operation control code */
	unsigned long    lpInBuffer;            /* input data buffer */
	u_int32_t        nInBufferSize;         /* size of input data buffer */
	unsigned long    lpOutBuffer;           /* output data buffer */
	u_int32_t        nOutBufferSize;        /* size of output data buffer */
	unsigned long    lpBytesReturned;       /* count of HPT_U8s returned */
} __packed;

#define HPT_IOCTL_FLAG_OPEN 1
#define HPT_CTL_CODE_BSD_TO_IOP(x) ((x)-0xff00)

#if __FreeBSD_version>503000
typedef struct cdev * ioctl_dev_t;
#else
typedef dev_t ioctl_dev_t;
#endif

#if __FreeBSD_version >= 500000
typedef struct thread * ioctl_thread_t;
#else
typedef struct proc * ioctl_thread_t;
#endif

struct hpt_iop_hba {
	struct hptiop_adapter_ops *ops;
	union {
		struct {
			struct hpt_iopmu_itl *mu;
		} itl;
		struct {
			struct hpt_iopmv_regs *regs;
			struct hpt_iopmu_mv *mu;
		} mv;
	} u;
	
	struct hpt_iop_hba    *next;
	
	u_int32_t             firmware_version;
	u_int32_t             interface_version;
	u_int32_t             max_devices;
	u_int32_t             max_requests;
	u_int32_t             max_request_size;
	u_int32_t             max_sg_count;

	u_int32_t             msg_done;

	device_t              pcidev;
	u_int32_t             pciunit;
	ioctl_dev_t           ioctl_dev;

	bus_dma_tag_t         parent_dmat;
	bus_dma_tag_t         io_dmat;
	bus_dma_tag_t         srb_dmat;
	bus_dma_tag_t	      ctlcfg_dmat;
	
	bus_dmamap_t          srb_dmamap;
	bus_dmamap_t          ctlcfg_dmamap;

	struct resource       *bar0_res;
	bus_space_tag_t       bar0t;
	bus_space_handle_t    bar0h;
	int                   bar0_rid;

	struct resource       *bar2_res;
	bus_space_tag_t	      bar2t;
	bus_space_handle_t    bar2h;
	int                   bar2_rid;
	
	/* to release */
	u_int8_t              *uncached_ptr;
	void		      *ctlcfg_ptr;
	/* for scsi request block */
	struct hpt_iop_srb    *srb_list;
	/* for interrupt */
	struct resource       *irq_res;
	void                  *irq_handle;

	/* for ioctl and set/get config */
	struct resource	      *ctlcfg_res;
	void		      *ctlcfg_handle;
	u_int64_t             ctlcfgcmd_phy;
	u_int32_t             config_done;

	/* other resources */
	struct cam_sim        *sim;
	struct cam_path       *path;
	void                  *req;
#if (__FreeBSD_version >= 500000)
	struct mtx            lock;
#else
	int                   hpt_splx;
#endif
#define HPT_IOCTL_FLAG_OPEN     1
	u_int32_t             flag;
	struct hpt_iop_srb* srb[HPT_SRB_MAX_QUEUE_SIZE];
};

struct hptiop_adapter_ops {
	int  (*iop_wait_ready)(struct hpt_iop_hba *hba, u_int32_t millisec);
	int  (*internal_memalloc)(struct hpt_iop_hba *hba);
	int  (*internal_memfree)(struct hpt_iop_hba *hba);
	int  (*alloc_pci_res)(struct hpt_iop_hba *hba);
	void (*release_pci_res)(struct hpt_iop_hba *hba);
	void (*enable_intr)(struct hpt_iop_hba *hba);
	void (*disable_intr)(struct hpt_iop_hba *hba);
	int  (*get_config)(struct hpt_iop_hba *hba,
				struct hpt_iop_request_get_config *config);
	int  (*set_config)(struct hpt_iop_hba *hba,
				struct hpt_iop_request_set_config *config);
	int  (*iop_intr)(struct hpt_iop_hba *hba);
	void (*post_msg)(struct hpt_iop_hba *hba, u_int32_t msg);
	void (*post_req)(struct hpt_iop_hba *hba, struct hpt_iop_srb *srb, bus_dma_segment_t *segs, int nsegs);
	int (*do_ioctl)(struct hpt_iop_hba *hba, struct hpt_iop_ioctl_param * pParams);	
};

struct hpt_iop_srb {
	u_int8_t             req[HPT_SRB_MAX_REQ_SIZE];
	struct hpt_iop_hba   *hba;
	union ccb            *ccb;
	struct hpt_iop_srb   *next;
	bus_dmamap_t         dma_map;
	u_int64_t            phy_addr;
	u_int32_t            srb_flag;
	int                  index;
};

#if __FreeBSD_version >= 500000
#define hptiop_lock_adapter(hba)   mtx_lock(&(hba)->lock)
#define hptiop_unlock_adapter(hba) mtx_unlock(&(hba)->lock)
#else
static __inline void hptiop_lock_adapter(struct hpt_iop_hba *hba)
{
	hba->hpt_splx = splcam();
}
static __inline void hptiop_unlock_adapter(struct hpt_iop_hba *hba)
{
	splx(hba->hpt_splx);
}
#endif

#define HPT_OSM_TIMEOUT (20*hz)  /* timeout value for OS commands */

#define HPT_DO_IOCONTROL    _IOW('H', 0, struct hpt_iop_ioctl_param)
#define HPT_SCAN_BUS        _IO('H', 1)

static  __inline int hptiop_sleep(struct hpt_iop_hba *hba, void *ident,
				int priority, const char *wmesg, int timo)
{

	int retval;

#if __FreeBSD_version >= 500000
	retval = msleep(ident, &hba->lock, priority, wmesg, timo);
#else
	asleep(ident, priority, wmesg, timo);
	hptiop_unlock_adapter(hba);
	retval = await(priority, timo);
	hptiop_lock_adapter(hba);
#endif

	return retval;

}

#if __FreeBSD_version < 501000
#define READ_16             0x88
#define WRITE_16            0x8a
#define SERVICE_ACTION_IN   0x9e
#endif

#define HPT_DEV_MAJOR   200

#endif

