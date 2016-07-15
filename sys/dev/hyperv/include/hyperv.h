/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/**
 * HyperV definitions for messages that are sent between instances of the
 * Channel Management Library in separate partitions, or in some cases,
 * back to itself.
 */

#ifndef __HYPERV_H__
#define __HYPERV_H__

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/sema.h>
#include <sys/smp.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <amd64/include/xen/synch_bitops.h>
#include <amd64/include/atomic.h>
#include <dev/hyperv/include/hyperv_busdma.h>

typedef uint8_t	hv_bool_uint8_t;

#define HV_S_OK			0x00000000
#define HV_E_FAIL		0x80004005
#define HV_ERROR_NOT_SUPPORTED	0x80070032
#define HV_ERROR_MACHINE_LOCKED	0x800704F7

/*
 * VMBUS version is 32 bit, upper 16 bit for major_number and lower
 * 16 bit for minor_number.
 *
 * 0.13  --  Windows Server 2008
 * 1.1   --  Windows 7
 * 2.4   --  Windows 8
 * 3.0   --  Windows 8.1
 */
#define VMBUS_VERSION_WS2008		((0 << 16) | (13))
#define VMBUS_VERSION_WIN7		((1 << 16) | (1))
#define VMBUS_VERSION_WIN8		((2 << 16) | (4))
#define VMBUS_VERSION_WIN8_1		((3 << 16) | (0))

#define VMBUS_VERSION_MAJOR(ver)	(((uint32_t)(ver)) >> 16)
#define VMBUS_VERSION_MINOR(ver)	(((uint32_t)(ver)) & 0xffff)

struct hyperv_guid {
	uint8_t		hv_guid[16];
} __packed;

#define HYPERV_GUID_STRLEN	40

int	hyperv_guid2str(const struct hyperv_guid *, char *, size_t);

#define HW_MACADDR_LEN	6

/*
 * Common defines for Hyper-V ICs
 */
#define HV_ICMSGTYPE_NEGOTIATE		0
#define HV_ICMSGTYPE_HEARTBEAT		1
#define HV_ICMSGTYPE_KVPEXCHANGE	2
#define HV_ICMSGTYPE_SHUTDOWN		3
#define HV_ICMSGTYPE_TIMESYNC		4
#define HV_ICMSGTYPE_VSS		5

#define HV_ICMSGHDRFLAG_TRANSACTION	1
#define HV_ICMSGHDRFLAG_REQUEST		2
#define HV_ICMSGHDRFLAG_RESPONSE	4

typedef struct hv_vmbus_pipe_hdr {
	uint32_t flags;
	uint32_t msgsize;
} __packed hv_vmbus_pipe_hdr;

typedef struct hv_vmbus_ic_version {
	uint16_t major;
	uint16_t minor;
} __packed hv_vmbus_ic_version;

typedef struct hv_vmbus_icmsg_hdr {
	hv_vmbus_ic_version	icverframe;
	uint16_t		icmsgtype;
	hv_vmbus_ic_version	icvermsg;
	uint16_t		icmsgsize;
	uint32_t		status;
	uint8_t			ictransaction_id;
	uint8_t			icflags;
	uint8_t			reserved[2];
} __packed hv_vmbus_icmsg_hdr;

typedef struct hv_vmbus_icmsg_negotiate {
	uint16_t		icframe_vercnt;
	uint16_t		icmsg_vercnt;
	uint32_t		reserved;
	hv_vmbus_ic_version	icversion_data[1]; /* any size array */
} __packed hv_vmbus_icmsg_negotiate;

typedef struct hv_vmbus_shutdown_msg_data {
	uint32_t		reason_code;
	uint32_t		timeout_seconds;
	uint32_t 		flags;
	uint8_t			display_message[2048];
} __packed hv_vmbus_shutdown_msg_data;

typedef struct hv_vmbus_heartbeat_msg_data {
	uint64_t 		seq_num;
	uint32_t 		reserved[8];
} __packed hv_vmbus_heartbeat_msg_data;

typedef struct {
	/*
	 * offset in bytes from the start of ring data below
	 */
	volatile uint32_t       write_index;
	/*
	 * offset in bytes from the start of ring data below
	 */
	volatile uint32_t       read_index;
	/*
	 * NOTE: The interrupt_mask field is used only for channels, but
	 * vmbus connection also uses this data structure
	 */
	volatile uint32_t       interrupt_mask;
	/* pad it to PAGE_SIZE so that data starts on a page */
	uint8_t                 reserved[4084];

	/*
	 * WARNING: Ring data starts here
	 *  !!! DO NOT place any fields below this !!!
	 */
	uint8_t			buffer[0];	/* doubles as interrupt mask */
} __packed hv_vmbus_ring_buffer;

typedef struct {
	hv_vmbus_ring_buffer*	ring_buffer;
	struct mtx		ring_lock;
	uint32_t		ring_data_size;	/* ring_size */
} hv_vmbus_ring_buffer_info;

typedef void	(*vmbus_chan_callback_t)(void *);

typedef struct hv_vmbus_channel {
	device_t			ch_dev;
	struct vmbus_softc		*vmbus_sc;
	uint32_t			ch_flags;	/* VMBUS_CHAN_FLAG_ */
	uint32_t			ch_id;		/* channel id */

	/*
	 * These are based on the offer_msg.monitor_id.
	 * Save it here for easy access.
	 */
	int				ch_montrig_idx;	/* MNF trig index */
	uint32_t			ch_montrig_mask;/* MNF trig mask */

	/*
	 * send to parent
	 */
	hv_vmbus_ring_buffer_info	outbound;
	/*
	 * receive from parent
	 */
	hv_vmbus_ring_buffer_info	inbound;

	struct taskqueue		*ch_tq;
	struct task			ch_task;
	vmbus_chan_callback_t		ch_cb;
	void				*ch_cbarg;

	struct hyperv_mon_param		*ch_monprm;
	struct hyperv_dma		ch_monprm_dma;

	int				ch_cpuid;	/* owner cpu */
	/*
	 * Virtual cpuid for ch_cpuid; it is used to communicate cpuid
	 * related information w/ Hyper-V.  If MSR_HV_VP_INDEX does not
	 * exist, ch_vcpuid will always be 0 for compatibility.
	 */
	uint32_t			ch_vcpuid;

	/*
	 * If this is a primary channel, ch_subchan* fields
	 * contain sub-channels belonging to this primary
	 * channel.
	 */
	struct mtx			ch_subchan_lock;
	TAILQ_HEAD(, hv_vmbus_channel)	ch_subchans;
	int				ch_subchan_cnt;

	/* If this is a sub-channel */
	TAILQ_ENTRY(hv_vmbus_channel)	ch_sublink;	/* sub-channel link */
	struct hv_vmbus_channel		*ch_prichan;	/* owner primary chan */

	/*
	 * Driver private data
	 */
	void				*hv_chan_priv1;
	void				*hv_chan_priv2;
	void				*hv_chan_priv3;

	void				*ch_bufring;	/* TX+RX bufrings */
	struct hyperv_dma		ch_bufring_dma;
	uint32_t			ch_bufring_gpadl;

	struct task			ch_detach_task;
	TAILQ_ENTRY(hv_vmbus_channel)	ch_prilink;	/* primary chan link */
	uint32_t			ch_subidx;	/* subchan index */
	volatile uint32_t		ch_stflags;	/* atomic-op */
							/* VMBUS_CHAN_ST_ */
	struct hyperv_guid		ch_guid_type;
	struct hyperv_guid		ch_guid_inst;

	struct sysctl_ctx_list		ch_sysctl_ctx;
} hv_vmbus_channel;

#define VMBUS_CHAN_ISPRIMARY(chan)	((chan)->ch_subidx == 0)

#define VMBUS_CHAN_FLAG_HASMNF		0x0001
/*
 * If this flag is set, this channel's interrupt will be masked in ISR,
 * and the RX bufring will be drained before this channel's interrupt is
 * unmasked.
 *
 * This flag is turned on by default.  Drivers can turn it off according
 * to their own requirement.
 */
#define VMBUS_CHAN_FLAG_BATCHREAD	0x0002

#define VMBUS_CHAN_ST_OPENED_SHIFT	0
#define VMBUS_CHAN_ST_OPENED		(1 << VMBUS_CHAN_ST_OPENED_SHIFT)

static inline void
hv_set_channel_read_state(hv_vmbus_channel* channel, boolean_t on)
{
	if (!on)
		channel->ch_flags &= ~VMBUS_CHAN_FLAG_BATCHREAD;
	else
		channel->ch_flags |= VMBUS_CHAN_FLAG_BATCHREAD;
}

int		hv_vmbus_channel_open(
				hv_vmbus_channel*	channel,
				uint32_t		send_ring_buffer_size,
				uint32_t		recv_ring_buffer_size,
				void*			user_data,
				uint32_t		user_data_len,
				vmbus_chan_callback_t	cb,
				void			*cbarg);

void		hv_vmbus_channel_close(hv_vmbus_channel *channel);

int		hv_vmbus_channel_establish_gpadl(
				hv_vmbus_channel*	channel,
				/* must be phys and virt contiguous */
				void*			contig_buffer,
				/*  page-size multiple	*/
				uint32_t		size,
				uint32_t*		gpadl_handle);

int		hv_vmbus_channel_teardown_gpdal(
				hv_vmbus_channel*	channel,
				uint32_t		gpadl_handle);

int		vmbus_chan_gpadl_connect(struct hv_vmbus_channel *chan,
		    bus_addr_t paddr, int size, uint32_t *gpadl);

struct hv_vmbus_channel* vmbus_select_outgoing_channel(struct hv_vmbus_channel *promary);

void		vmbus_channel_cpu_set(struct hv_vmbus_channel *chan, int cpu);
void		vmbus_channel_cpu_rr(struct hv_vmbus_channel *chan);
struct hv_vmbus_channel **
		vmbus_get_subchan(struct hv_vmbus_channel *pri_chan, int subchan_cnt);
void		vmbus_rel_subchan(struct hv_vmbus_channel **subchan, int subchan_cnt);
void		vmbus_drain_subchan(struct hv_vmbus_channel *pri_chan);

/**
 * @brief Get physical address from virtual
 */
static inline unsigned long
hv_get_phys_addr(void *virt)
{
	unsigned long ret;
	ret = (vtophys(virt) | ((vm_offset_t) virt & PAGE_MASK));
	return (ret);
}

static __inline struct hv_vmbus_channel *
vmbus_get_channel(device_t dev)
{
	return device_get_ivars(dev);
}

#endif  /* __HYPERV_H__ */
