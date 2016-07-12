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
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <machine/atomic.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/hyperv_reg.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

/*
 * Globals
 */
hv_vmbus_connection hv_vmbus_g_connection =
	{ .connect_state = HV_DISCONNECTED,
	  .next_gpadl_handle = 0xE1E10, };

uint32_t hv_vmbus_protocal_version;

/**
 * Send a connect request on the partition service connection
 */
int
hv_vmbus_connect(struct vmbus_softc *sc)
{
	/**
	 * Make sure we are not connecting or connected
	 */
	if (hv_vmbus_g_connection.connect_state != HV_DISCONNECTED) {
		return (-1);
	}

	/**
	 * Initialize the vmbus connection
	 */
	hv_vmbus_g_connection.connect_state = HV_CONNECTING;

	TAILQ_INIT(&hv_vmbus_g_connection.channel_anchor);
	mtx_init(&hv_vmbus_g_connection.channel_lock, "vmbus channel",
		NULL, MTX_DEF);

	hv_vmbus_g_connection.channels = malloc(sizeof(hv_vmbus_channel*) *
	    VMBUS_CHAN_MAX, M_DEVBUF, M_WAITOK | M_ZERO);

	hv_vmbus_g_connection.connect_state = HV_CONNECTED;

	return (0);
}

/**
 * Send a disconnect request on the partition service connection
 */
int
hv_vmbus_disconnect(void)
{

	free(hv_vmbus_g_connection.channels, M_DEVBUF);
	hv_vmbus_g_connection.connect_state = HV_DISCONNECTED;

	return (0);
}

static __inline void
vmbus_event_flags_proc(volatile u_long *event_flags, int flag_cnt)
{
	int f;

	for (f = 0; f < flag_cnt; ++f) {
		uint32_t rel_id_base;
		u_long flags;
		int bit;

		if (event_flags[f] == 0)
			continue;

		flags = atomic_swap_long(&event_flags[f], 0);
		rel_id_base = f << VMBUS_EVTFLAG_SHIFT;

		while ((bit = ffsl(flags)) != 0) {
			struct hv_vmbus_channel *channel;
			uint32_t rel_id;

			--bit;	/* NOTE: ffsl is 1-based */
			flags &= ~(1UL << bit);

			rel_id = rel_id_base + bit;
			channel = hv_vmbus_g_connection.channels[rel_id];

			/* if channel is closed or closing */
			if (channel == NULL || channel->rxq == NULL)
				continue;

			if (channel->batched_reading)
				hv_ring_buffer_read_begin(&channel->inbound);
			taskqueue_enqueue(channel->rxq, &channel->channel_task);
		}
	}
}

void
vmbus_event_proc(struct vmbus_softc *sc, int cpu)
{
	struct vmbus_evtflags *eventf;

	/*
	 * On Host with Win8 or above, the event page can be checked directly
	 * to get the id of the channel that has the pending interrupt.
	 */
	eventf = VMBUS_PCPU_GET(sc, event_flags, cpu) + VMBUS_SINT_MESSAGE;
	vmbus_event_flags_proc(eventf->evt_flags,
	    VMBUS_PCPU_GET(sc, event_flags_cnt, cpu));
}

void
vmbus_event_proc_compat(struct vmbus_softc *sc, int cpu)
{
	struct vmbus_evtflags *eventf;

	eventf = VMBUS_PCPU_GET(sc, event_flags, cpu) + VMBUS_SINT_MESSAGE;
	if (atomic_testandclear_long(&eventf->evt_flags[0], 0)) {
		vmbus_event_flags_proc(sc->vmbus_rx_evtflags,
		    VMBUS_CHAN_MAX_COMPAT >> VMBUS_EVTFLAG_SHIFT);
	}
}

/**
 * Send an event notification to the parent
 */
int
hv_vmbus_set_event(hv_vmbus_channel *channel)
{

	return hv_vmbus_signal_event(channel->signal_event_param);
}

void
vmbus_on_channel_open(const struct hv_vmbus_channel *chan)
{
	volatile int *flag_cnt_ptr;
	int flag_cnt;

	flag_cnt = (chan->offer_msg.child_rel_id / VMBUS_EVTFLAG_LEN) + 1;
	flag_cnt_ptr = VMBUS_PCPU_PTR(chan->vmbus_sc, event_flags_cnt,
	    chan->target_cpu);

	for (;;) {
		int old_flag_cnt;

		old_flag_cnt = *flag_cnt_ptr;
		if (old_flag_cnt >= flag_cnt)
			break;
		if (atomic_cmpset_int(flag_cnt_ptr, old_flag_cnt, flag_cnt)) {
			if (bootverbose) {
				printf("VMBUS: channel%u update "
				    "cpu%d flag_cnt to %d\n",
				    chan->offer_msg.child_rel_id,
				    chan->target_cpu, flag_cnt);
			}
			break;
		}
	}
}
