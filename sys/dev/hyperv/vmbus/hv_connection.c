/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include "hv_vmbus_priv.h"

/*
 * Globals
 */
hv_vmbus_connection hv_vmbus_g_connection =
	{ .connect_state = HV_DISCONNECTED,
	  .next_gpadl_handle = 0xE1E10, };

uint32_t hv_vmbus_protocal_version = HV_VMBUS_VERSION_WS2008;

static uint32_t
hv_vmbus_get_next_version(uint32_t current_ver)
{
	switch (current_ver) {
	case (HV_VMBUS_VERSION_WIN7):
		return(HV_VMBUS_VERSION_WS2008);

	case (HV_VMBUS_VERSION_WIN8):
		return(HV_VMBUS_VERSION_WIN7);

	case (HV_VMBUS_VERSION_WIN8_1):
		return(HV_VMBUS_VERSION_WIN8);

	case (HV_VMBUS_VERSION_WS2008):
	default:
		return(HV_VMBUS_VERSION_INVALID);
	}
}

/**
 * Negotiate the highest supported hypervisor version.
 */
static int
hv_vmbus_negotiate_version(hv_vmbus_channel_msg_info *msg_info,
	uint32_t version)
{
	int					ret = 0;
	hv_vmbus_channel_initiate_contact	*msg;

	sema_init(&msg_info->wait_sema, 0, "Msg Info Sema");
	msg = (hv_vmbus_channel_initiate_contact*) msg_info->msg;

	msg->header.message_type = HV_CHANNEL_MESSAGE_INITIATED_CONTACT;
	msg->vmbus_version_requested = version;

	msg->interrupt_page = hv_get_phys_addr(
		hv_vmbus_g_connection.interrupt_page);

	msg->monitor_page_1 = hv_get_phys_addr(
		hv_vmbus_g_connection.monitor_page_1);

	msg->monitor_page_2 = hv_get_phys_addr(
		hv_vmbus_g_connection.monitor_page_2);

	/**
	 * Add to list before we send the request since we may receive the
	 * response before returning from this routine
	 */
	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);

	TAILQ_INSERT_TAIL(
		&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info,
		msg_list_entry);

	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

	ret = hv_vmbus_post_message(
		msg,
		sizeof(hv_vmbus_channel_initiate_contact));

	if (ret != 0) {
		mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
		TAILQ_REMOVE(
			&hv_vmbus_g_connection.channel_msg_anchor,
			msg_info,
			msg_list_entry);
		mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);
		return (ret);
	}

	/**
	 * Wait for the connection response
	 */
	ret = sema_timedwait(&msg_info->wait_sema, 5 * hz); /* KYS 5 seconds */

	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(
		&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info,
		msg_list_entry);
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

	/**
	 * Check if successful
	 */
	if (msg_info->response.version_response.version_supported) {
		hv_vmbus_g_connection.connect_state = HV_CONNECTED;
	} else {
		ret = ECONNREFUSED;
	}

	return (ret);
}

/**
 * Send a connect request on the partition service connection
 */
int
hv_vmbus_connect(void) {
	int					ret = 0;
	uint32_t				version;
	hv_vmbus_channel_msg_info*		msg_info = NULL;

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

	TAILQ_INIT(&hv_vmbus_g_connection.channel_msg_anchor);
	mtx_init(&hv_vmbus_g_connection.channel_msg_lock, "vmbus channel msg",
		NULL, MTX_DEF);

	TAILQ_INIT(&hv_vmbus_g_connection.channel_anchor);
	mtx_init(&hv_vmbus_g_connection.channel_lock, "vmbus channel",
		NULL, MTX_DEF);

	/**
	 * Setup the vmbus event connection for channel interrupt abstraction
	 * stuff
	 */
	hv_vmbus_g_connection.interrupt_page = malloc(
					PAGE_SIZE, M_DEVBUF,
					M_WAITOK | M_ZERO);

	hv_vmbus_g_connection.recv_interrupt_page =
		hv_vmbus_g_connection.interrupt_page;

	hv_vmbus_g_connection.send_interrupt_page =
		((uint8_t *) hv_vmbus_g_connection.interrupt_page +
		    (PAGE_SIZE >> 1));

	/**
	 * Set up the monitor notification facility. The 1st page for
	 * parent->child and the 2nd page for child->parent
	 */
	hv_vmbus_g_connection.monitor_page_1 = malloc(
		PAGE_SIZE,
		M_DEVBUF,
		M_WAITOK | M_ZERO);
	hv_vmbus_g_connection.monitor_page_2 = malloc(
		PAGE_SIZE,
		M_DEVBUF,
		M_WAITOK | M_ZERO);

	msg_info = (hv_vmbus_channel_msg_info*)
		malloc(sizeof(hv_vmbus_channel_msg_info) +
			sizeof(hv_vmbus_channel_initiate_contact),
			M_DEVBUF, M_WAITOK | M_ZERO);

	hv_vmbus_g_connection.channels = malloc(sizeof(hv_vmbus_channel*) *
		HV_CHANNEL_MAX_COUNT,
		M_DEVBUF, M_WAITOK | M_ZERO);
	/*
	 * Find the highest vmbus version number we can support.
	 */
	version = HV_VMBUS_VERSION_CURRENT;

	do {
		ret = hv_vmbus_negotiate_version(msg_info, version);
		if (ret == EWOULDBLOCK) {
			/*
			 * We timed out.
			 */
			goto cleanup;
		}

		if (hv_vmbus_g_connection.connect_state == HV_CONNECTED)
			break;

		version = hv_vmbus_get_next_version(version);
	} while (version != HV_VMBUS_VERSION_INVALID);

	hv_vmbus_protocal_version = version;
	if (bootverbose)
		printf("VMBUS: Protocol Version: %d.%d\n",
		    version >> 16, version & 0xFFFF);

	sema_destroy(&msg_info->wait_sema);
	free(msg_info, M_DEVBUF);

	return (0);

	/*
	 * Cleanup after failure!
	 */
	cleanup:

	hv_vmbus_g_connection.connect_state = HV_DISCONNECTED;

	mtx_destroy(&hv_vmbus_g_connection.channel_lock);
	mtx_destroy(&hv_vmbus_g_connection.channel_msg_lock);

	if (hv_vmbus_g_connection.interrupt_page != NULL) {
		free(hv_vmbus_g_connection.interrupt_page, M_DEVBUF);
		hv_vmbus_g_connection.interrupt_page = NULL;
	}

	free(hv_vmbus_g_connection.monitor_page_1, M_DEVBUF);
	free(hv_vmbus_g_connection.monitor_page_2, M_DEVBUF);

	if (msg_info) {
		sema_destroy(&msg_info->wait_sema);
		free(msg_info, M_DEVBUF);
	}

	free(hv_vmbus_g_connection.channels, M_DEVBUF);
	return (ret);
}

/**
 * Send a disconnect request on the partition service connection
 */
int
hv_vmbus_disconnect(void) {
	int			 ret = 0;
	hv_vmbus_channel_unload  msg;

	msg.message_type = HV_CHANNEL_MESSAGE_UNLOAD;

	ret = hv_vmbus_post_message(&msg, sizeof(hv_vmbus_channel_unload));

	free(hv_vmbus_g_connection.interrupt_page, M_DEVBUF);

	mtx_destroy(&hv_vmbus_g_connection.channel_msg_lock);

	free(hv_vmbus_g_connection.channels, M_DEVBUF);
	hv_vmbus_g_connection.connect_state = HV_DISCONNECTED;

	return (ret);
}

/**
 * Handler for events
 */
void
hv_vmbus_on_events(int cpu)
{
	int bit;
	int dword;
	void *page_addr;
	uint32_t* recv_interrupt_page = NULL;
	int rel_id;
	int maxdword;
	hv_vmbus_synic_event_flags *event;
	/* int maxdword = PAGE_SIZE >> 3; */

	KASSERT(cpu <= mp_maxid, ("VMBUS: hv_vmbus_on_events: "
	    "cpu out of range!"));

	if ((hv_vmbus_protocal_version == HV_VMBUS_VERSION_WS2008) ||
	    (hv_vmbus_protocal_version == HV_VMBUS_VERSION_WIN7)) {
		maxdword = HV_MAX_NUM_CHANNELS_SUPPORTED >> 5;
		/*
		 * receive size is 1/2 page and divide that by 4 bytes
		 */
		recv_interrupt_page =
		    hv_vmbus_g_connection.recv_interrupt_page;
	} else {
		/*
		 * On Host with Win8 or above, the event page can be
		 * checked directly to get the id of the channel
		 * that has the pending interrupt.
		 */
		maxdword = HV_EVENT_FLAGS_DWORD_COUNT;
		page_addr = hv_vmbus_g_context.syn_ic_event_page[cpu];
		event = (hv_vmbus_synic_event_flags *)
		    page_addr + HV_VMBUS_MESSAGE_SINT;
		recv_interrupt_page = event->flags32;
	}

	/*
	 * Check events
	 */
	if (recv_interrupt_page != NULL) {
	    for (dword = 0; dword < maxdword; dword++) {
		if (recv_interrupt_page[dword]) {
		    for (bit = 0; bit < HV_CHANNEL_DWORD_LEN; bit++) {
			if (synch_test_and_clear_bit(bit,
			    (uint32_t *) &recv_interrupt_page[dword])) {
			    rel_id = (dword << 5) + bit;
			    if (rel_id == 0) {
				/*
				 * Special case -
				 * vmbus channel protocol msg.
				 */
				continue;
			    } else {
				hv_vmbus_channel * channel = hv_vmbus_g_connection.channels[rel_id];
				/* if channel is closed or closing */
				if (channel == NULL || channel->rxq == NULL)
					continue;

				if (channel->batched_reading)
					hv_ring_buffer_read_begin(&channel->inbound);
				taskqueue_enqueue_fast(channel->rxq, &channel->channel_task);
			    }
			}
		    }
		}
	    }
	}

	return;
}

/**
 * Send a msg on the vmbus's message connection
 */
int hv_vmbus_post_message(void *buffer, size_t bufferLen)
{
	hv_vmbus_connection_id connId;
	sbintime_t time = SBT_1MS;
	int retries;
	int ret;

	connId.as_uint32_t = 0;
	connId.u.id = HV_VMBUS_MESSAGE_CONNECTION_ID;

	/*
	 * We retry to cope with transient failures caused by host side's
	 * insufficient resources. 20 times should suffice in practice.
	 */
	for (retries = 0; retries < 20; retries++) {
		ret = hv_vmbus_post_msg_via_msg_ipc(connId, 1, buffer,
						    bufferLen);
		if (ret == HV_STATUS_SUCCESS)
			return (0);

		pause_sbt("pstmsg", time, 0, C_HARDCLOCK);
		if (time < SBT_1S * 2)
			time *= 2;
	}

	KASSERT(ret == HV_STATUS_SUCCESS,
		("Error VMBUS: Message Post Failed, ret=%d\n", ret));

	return (EAGAIN);
}

/**
 * Send an event notification to the parent
 */
int
hv_vmbus_set_event(hv_vmbus_channel *channel) {
	int ret = 0;
	uint32_t child_rel_id = channel->offer_msg.child_rel_id;

	/* Each uint32_t represents 32 channels */

	synch_set_bit(child_rel_id & 31,
		(((uint32_t *)hv_vmbus_g_connection.send_interrupt_page
			+ (child_rel_id >> 5))));
	ret = hv_vmbus_signal_event(channel->signal_event_param);

	return (ret);
}
