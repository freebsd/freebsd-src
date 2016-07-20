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
