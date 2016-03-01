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
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include "hv_vmbus_priv.h"

static int 	vmbus_channel_create_gpadl_header(
			/* must be phys and virt contiguous*/
			void*				contig_buffer,
			/* page-size multiple */
			uint32_t 			size,
			hv_vmbus_channel_msg_info**	msg_info,
			uint32_t*			message_count);

static void 	vmbus_channel_set_event(hv_vmbus_channel* channel);

/**
 *  @brief Trigger an event notification on the specified channel
 */
static void
vmbus_channel_set_event(hv_vmbus_channel *channel)
{
	hv_vmbus_monitor_page *monitor_page;

	if (channel->offer_msg.monitor_allocated) {
		/* Each uint32_t represents 32 channels */
		synch_set_bit((channel->offer_msg.child_rel_id & 31),
			((uint32_t *)hv_vmbus_g_connection.send_interrupt_page
				+ ((channel->offer_msg.child_rel_id >> 5))));

		monitor_page = (hv_vmbus_monitor_page *)
			hv_vmbus_g_connection.monitor_pages;

		monitor_page++; /* Get the child to parent monitor page */

		synch_set_bit(channel->monitor_bit,
			(uint32_t *)&monitor_page->
				trigger_group[channel->monitor_group].u.pending);
	} else {
		hv_vmbus_set_event(channel);
	}

}

/**
 * @brief Open the specified channel
 */
int
hv_vmbus_channel_open(
	hv_vmbus_channel*		new_channel,
	uint32_t			send_ring_buffer_size,
	uint32_t			recv_ring_buffer_size,
	void*				user_data,
	uint32_t			user_data_len,
	hv_vmbus_pfn_channel_callback	pfn_on_channel_callback,
	void* 				context)
{

	int ret = 0;
	void *in, *out;
	hv_vmbus_channel_open_channel*	open_msg;
	hv_vmbus_channel_msg_info* 	open_info;

	mtx_lock(&new_channel->sc_lock);
	if (new_channel->state == HV_CHANNEL_OPEN_STATE) {
	    new_channel->state = HV_CHANNEL_OPENING_STATE;
	} else {
	    mtx_unlock(&new_channel->sc_lock);
	    if(bootverbose)
		printf("VMBUS: Trying to open channel <%p> which in "
		    "%d state.\n", new_channel, new_channel->state);
	    return (EINVAL);
	}
	mtx_unlock(&new_channel->sc_lock);

	new_channel->on_channel_callback = pfn_on_channel_callback;
	new_channel->channel_callback_context = context;

	/* Allocate the ring buffer */
	out = contigmalloc((send_ring_buffer_size + recv_ring_buffer_size),
	    M_DEVBUF, M_ZERO, 0UL, BUS_SPACE_MAXADDR, PAGE_SIZE, 0);
	KASSERT(out != NULL,
	    ("Error VMBUS: contigmalloc failed to allocate Ring Buffer!"));
	if (out == NULL)
		return (ENOMEM);

	in = ((uint8_t *) out + send_ring_buffer_size);

	new_channel->ring_buffer_pages = out;
	new_channel->ring_buffer_page_count = (send_ring_buffer_size +
	    recv_ring_buffer_size) >> PAGE_SHIFT;
	new_channel->ring_buffer_size = send_ring_buffer_size +
	    recv_ring_buffer_size;

	hv_vmbus_ring_buffer_init(
		&new_channel->outbound,
		out,
		send_ring_buffer_size);

	hv_vmbus_ring_buffer_init(
		&new_channel->inbound,
		in,
		recv_ring_buffer_size);

	/**
	 * Establish the gpadl for the ring buffer
	 */
	new_channel->ring_buffer_gpadl_handle = 0;

	ret = hv_vmbus_channel_establish_gpadl(new_channel,
		new_channel->outbound.ring_buffer,
		send_ring_buffer_size + recv_ring_buffer_size,
		&new_channel->ring_buffer_gpadl_handle);

	/**
	 * Create and init the channel open message
	 */
	open_info = (hv_vmbus_channel_msg_info*) malloc(
		sizeof(hv_vmbus_channel_msg_info) +
			sizeof(hv_vmbus_channel_open_channel),
		M_DEVBUF,
		M_NOWAIT);
	KASSERT(open_info != NULL,
	    ("Error VMBUS: malloc failed to allocate Open Channel message!"));

	if (open_info == NULL)
		return (ENOMEM);

	sema_init(&open_info->wait_sema, 0, "Open Info Sema");

	open_msg = (hv_vmbus_channel_open_channel*) open_info->msg;
	open_msg->header.message_type = HV_CHANNEL_MESSAGE_OPEN_CHANNEL;
	open_msg->open_id = new_channel->offer_msg.child_rel_id;
	open_msg->child_rel_id = new_channel->offer_msg.child_rel_id;
	open_msg->ring_buffer_gpadl_handle =
		new_channel->ring_buffer_gpadl_handle;
	open_msg->downstream_ring_buffer_page_offset = send_ring_buffer_size
		>> PAGE_SHIFT;
	open_msg->target_vcpu = new_channel->target_vcpu;

	if (user_data_len)
		memcpy(open_msg->user_data, user_data, user_data_len);

	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_INSERT_TAIL(
		&hv_vmbus_g_connection.channel_msg_anchor,
		open_info,
		msg_list_entry);
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	ret = hv_vmbus_post_message(
		open_msg, sizeof(hv_vmbus_channel_open_channel));

	if (ret != 0)
	    goto cleanup;

	ret = sema_timedwait(&open_info->wait_sema, 5 * hz); /* KYS 5 seconds */

	if (ret) {
	    if(bootverbose)
		printf("VMBUS: channel <%p> open timeout.\n", new_channel);
	    goto cleanup;
	}

	if (open_info->response.open_result.status == 0) {
	    new_channel->state = HV_CHANNEL_OPENED_STATE;
	    if(bootverbose)
		printf("VMBUS: channel <%p> open success.\n", new_channel);
	} else {
	    if(bootverbose)
		printf("Error VMBUS: channel <%p> open failed - %d!\n",
			new_channel, open_info->response.open_result.status);
	}

	cleanup:
	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(
		&hv_vmbus_g_connection.channel_msg_anchor,
		open_info,
		msg_list_entry);
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	sema_destroy(&open_info->wait_sema);
	free(open_info, M_DEVBUF);

	return (ret);
}

/**
 * @brief Create a gpadl for the specified buffer
 */
static int
vmbus_channel_create_gpadl_header(
	void*				contig_buffer,
	uint32_t			size,	/* page-size multiple */
	hv_vmbus_channel_msg_info**	msg_info,
	uint32_t*			message_count)
{
	int				i;
	int				page_count;
	unsigned long long 		pfn;
	uint32_t			msg_size;
	hv_vmbus_channel_gpadl_header*	gpa_header;
	hv_vmbus_channel_gpadl_body*	gpadl_body;
	hv_vmbus_channel_msg_info*	msg_header;
	hv_vmbus_channel_msg_info*	msg_body;

	int pfnSum, pfnCount, pfnLeft, pfnCurr, pfnSize;

	page_count = size >> PAGE_SHIFT;
	pfn = hv_get_phys_addr(contig_buffer) >> PAGE_SHIFT;

	/*do we need a gpadl body msg */
	pfnSize = HV_MAX_SIZE_CHANNEL_MESSAGE
	    - sizeof(hv_vmbus_channel_gpadl_header)
	    - sizeof(hv_gpa_range);
	pfnCount = pfnSize / sizeof(uint64_t);

	if (page_count > pfnCount) { /* if(we need a gpadl body)	*/
	    /* fill in the header		*/
	    msg_size = sizeof(hv_vmbus_channel_msg_info)
		+ sizeof(hv_vmbus_channel_gpadl_header)
		+ sizeof(hv_gpa_range)
		+ pfnCount * sizeof(uint64_t);
	    msg_header = malloc(msg_size, M_DEVBUF, M_NOWAIT | M_ZERO);
	    KASSERT(
		msg_header != NULL,
		("Error VMBUS: malloc failed to allocate Gpadl Message!"));
	    if (msg_header == NULL)
		return (ENOMEM);

	    TAILQ_INIT(&msg_header->sub_msg_list_anchor);
	    msg_header->message_size = msg_size;

	    gpa_header = (hv_vmbus_channel_gpadl_header*) msg_header->msg;
	    gpa_header->range_count = 1;
	    gpa_header->range_buf_len = sizeof(hv_gpa_range)
		+ page_count * sizeof(uint64_t);
	    gpa_header->range[0].byte_offset = 0;
	    gpa_header->range[0].byte_count = size;
	    for (i = 0; i < pfnCount; i++) {
		gpa_header->range[0].pfn_array[i] = pfn + i;
	    }
	    *msg_info = msg_header;
	    *message_count = 1;

	    pfnSum = pfnCount;
	    pfnLeft = page_count - pfnCount;

	    /*
	     *  figure out how many pfns we can fit
	     */
	    pfnSize = HV_MAX_SIZE_CHANNEL_MESSAGE
		- sizeof(hv_vmbus_channel_gpadl_body);
	    pfnCount = pfnSize / sizeof(uint64_t);

	    /*
	     * fill in the body
	     */
	    while (pfnLeft) {
		if (pfnLeft > pfnCount) {
		    pfnCurr = pfnCount;
		} else {
		    pfnCurr = pfnLeft;
		}

		msg_size = sizeof(hv_vmbus_channel_msg_info) +
		    sizeof(hv_vmbus_channel_gpadl_body) +
		    pfnCurr * sizeof(uint64_t);
		msg_body = malloc(msg_size, M_DEVBUF, M_NOWAIT | M_ZERO);
		KASSERT(
		    msg_body != NULL,
		    ("Error VMBUS: malloc failed to allocate Gpadl msg_body!"));
		if (msg_body == NULL)
		    return (ENOMEM);

		msg_body->message_size = msg_size;
		(*message_count)++;
		gpadl_body =
		    (hv_vmbus_channel_gpadl_body*) msg_body->msg;
		/*
		 * gpadl_body->gpadl = kbuffer;
		 */
		for (i = 0; i < pfnCurr; i++) {
		    gpadl_body->pfn[i] = pfn + pfnSum + i;
		}

		TAILQ_INSERT_TAIL(
		    &msg_header->sub_msg_list_anchor,
		    msg_body,
		    msg_list_entry);
		pfnSum += pfnCurr;
		pfnLeft -= pfnCurr;
	    }
	} else { /* else everything fits in a header */

	    msg_size = sizeof(hv_vmbus_channel_msg_info) +
		sizeof(hv_vmbus_channel_gpadl_header) +
		sizeof(hv_gpa_range) +
		page_count * sizeof(uint64_t);
	    msg_header = malloc(msg_size, M_DEVBUF, M_NOWAIT | M_ZERO);
	    KASSERT(
		msg_header != NULL,
		("Error VMBUS: malloc failed to allocate Gpadl Message!"));
	    if (msg_header == NULL)
		return (ENOMEM);

	    msg_header->message_size = msg_size;

	    gpa_header = (hv_vmbus_channel_gpadl_header*) msg_header->msg;
	    gpa_header->range_count = 1;
	    gpa_header->range_buf_len = sizeof(hv_gpa_range) +
		page_count * sizeof(uint64_t);
	    gpa_header->range[0].byte_offset = 0;
	    gpa_header->range[0].byte_count = size;
	    for (i = 0; i < page_count; i++) {
		gpa_header->range[0].pfn_array[i] = pfn + i;
	    }

	    *msg_info = msg_header;
	    *message_count = 1;
	}

	return (0);
}

/**
 * @brief Establish a GPADL for the specified buffer
 */
int
hv_vmbus_channel_establish_gpadl(
	hv_vmbus_channel*	channel,
	void*			contig_buffer,
	uint32_t		size, /* page-size multiple */
	uint32_t*		gpadl_handle)

{
	int ret = 0;
	hv_vmbus_channel_gpadl_header*	gpadl_msg;
	hv_vmbus_channel_gpadl_body*	gpadl_body;
	hv_vmbus_channel_msg_info*	msg_info;
	hv_vmbus_channel_msg_info*	sub_msg_info;
	uint32_t			msg_count;
	hv_vmbus_channel_msg_info*	curr;
	uint32_t			next_gpadl_handle;

	next_gpadl_handle = hv_vmbus_g_connection.next_gpadl_handle;
	atomic_add_int((int*) &hv_vmbus_g_connection.next_gpadl_handle, 1);

	ret = vmbus_channel_create_gpadl_header(
		contig_buffer, size, &msg_info, &msg_count);

	if(ret != 0) { /* if(allocation failed) return immediately */
	    /* reverse atomic_add_int above */
	    atomic_subtract_int((int*)
		    &hv_vmbus_g_connection.next_gpadl_handle, 1);
	    return ret;
	}

	sema_init(&msg_info->wait_sema, 0, "Open Info Sema");
	gpadl_msg = (hv_vmbus_channel_gpadl_header*) msg_info->msg;
	gpadl_msg->header.message_type = HV_CHANNEL_MESSAGEL_GPADL_HEADER;
	gpadl_msg->child_rel_id = channel->offer_msg.child_rel_id;
	gpadl_msg->gpadl = next_gpadl_handle;

	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_INSERT_TAIL(
		&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info,
		msg_list_entry);

	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	ret = hv_vmbus_post_message(
		gpadl_msg,
		msg_info->message_size -
		    (uint32_t) sizeof(hv_vmbus_channel_msg_info));

	if (ret != 0)
	    goto cleanup;

	if (msg_count > 1) {
	    TAILQ_FOREACH(curr,
		    &msg_info->sub_msg_list_anchor, msg_list_entry) {
		sub_msg_info = curr;
		gpadl_body =
		    (hv_vmbus_channel_gpadl_body*) sub_msg_info->msg;

		gpadl_body->header.message_type =
		    HV_CHANNEL_MESSAGE_GPADL_BODY;
		gpadl_body->gpadl = next_gpadl_handle;

		ret = hv_vmbus_post_message(
			gpadl_body,
			sub_msg_info->message_size
			    - (uint32_t) sizeof(hv_vmbus_channel_msg_info));
		 /* if (the post message failed) give up and clean up */
		if(ret != 0)
		    goto cleanup;
	    }
	}

	ret = sema_timedwait(&msg_info->wait_sema, 5 * hz); /* KYS 5 seconds*/
	if (ret != 0)
	    goto cleanup;

	*gpadl_handle = gpadl_msg->gpadl;

cleanup:

	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info, msg_list_entry);
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	sema_destroy(&msg_info->wait_sema);
	free(msg_info, M_DEVBUF);

	return (ret);
}

/**
 * @brief Teardown the specified GPADL handle
 */
int
hv_vmbus_channel_teardown_gpdal(
	hv_vmbus_channel*	channel,
	uint32_t		gpadl_handle)
{
	int					ret = 0;
	hv_vmbus_channel_gpadl_teardown*	msg;
	hv_vmbus_channel_msg_info*		info;

	info = (hv_vmbus_channel_msg_info *)
		malloc(	sizeof(hv_vmbus_channel_msg_info) +
			sizeof(hv_vmbus_channel_gpadl_teardown),
				M_DEVBUF, M_NOWAIT);
	KASSERT(info != NULL,
	    ("Error VMBUS: malloc failed to allocate Gpadl Teardown Msg!"));
	if (info == NULL) {
	    ret = ENOMEM;
	    goto cleanup;
	}

	sema_init(&info->wait_sema, 0, "Open Info Sema");

	msg = (hv_vmbus_channel_gpadl_teardown*) info->msg;

	msg->header.message_type = HV_CHANNEL_MESSAGE_GPADL_TEARDOWN;
	msg->child_rel_id = channel->offer_msg.child_rel_id;
	msg->gpadl = gpadl_handle;

	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_INSERT_TAIL(&hv_vmbus_g_connection.channel_msg_anchor,
			info, msg_list_entry);
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	ret = hv_vmbus_post_message(msg,
			sizeof(hv_vmbus_channel_gpadl_teardown));
	if (ret != 0) 
	    goto cleanup;
	
	ret = sema_timedwait(&info->wait_sema, 5 * hz); /* KYS 5 seconds */

cleanup:
	/*
	 * Received a torndown response
	 */
	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(&hv_vmbus_g_connection.channel_msg_anchor,
			info, msg_list_entry);
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	sema_destroy(&info->wait_sema);
	free(info, M_DEVBUF);

	return (ret);
}

static void
hv_vmbus_channel_close_internal(hv_vmbus_channel *channel)
{
	int ret = 0;
	hv_vmbus_channel_close_channel* msg;
	hv_vmbus_channel_msg_info* info;

	channel->state = HV_CHANNEL_OPEN_STATE;
	channel->sc_creation_callback = NULL;

	/*
	 * Grab the lock to prevent race condition when a packet received
	 * and unloading driver is in the process.
	 */
	mtx_lock(&channel->inbound_lock);
	channel->on_channel_callback = NULL;
	mtx_unlock(&channel->inbound_lock);

	/**
	 * Send a closing message
	 */
	info = (hv_vmbus_channel_msg_info *)
		malloc(	sizeof(hv_vmbus_channel_msg_info) +
			sizeof(hv_vmbus_channel_close_channel),
				M_DEVBUF, M_NOWAIT);
	KASSERT(info != NULL, ("VMBUS: malloc failed hv_vmbus_channel_close!"));
	if(info == NULL)
	    return;

	msg = (hv_vmbus_channel_close_channel*) info->msg;
	msg->header.message_type = HV_CHANNEL_MESSAGE_CLOSE_CHANNEL;
	msg->child_rel_id = channel->offer_msg.child_rel_id;

	ret = hv_vmbus_post_message(
		msg, sizeof(hv_vmbus_channel_close_channel));

	/* Tear down the gpadl for the channel's ring buffer */
	if (channel->ring_buffer_gpadl_handle) {
		hv_vmbus_channel_teardown_gpdal(channel,
			channel->ring_buffer_gpadl_handle);
	}

	/* TODO: Send a msg to release the childRelId */

	/* cleanup the ring buffers for this channel */
	hv_ring_buffer_cleanup(&channel->outbound);
	hv_ring_buffer_cleanup(&channel->inbound);

	contigfree(channel->ring_buffer_pages, channel->ring_buffer_size,
	    M_DEVBUF);

	free(info, M_DEVBUF);
}

/**
 * @brief Close the specified channel
 */
void
hv_vmbus_channel_close(hv_vmbus_channel *channel)
{
	hv_vmbus_channel*	sub_channel;

	if (channel->primary_channel != NULL) {
		/*
		 * We only close multi-channels when the primary is
		 * closed.
		 */
		return;
	}

	/*
	 * Close all multi-channels first.
	 */
	TAILQ_FOREACH(sub_channel, &channel->sc_list_anchor,
	    sc_list_entry) {
		if (sub_channel->state != HV_CHANNEL_OPENED_STATE)
			continue;
		hv_vmbus_channel_close_internal(sub_channel);
	}
	/*
	 * Then close the primary channel.
	 */
	hv_vmbus_channel_close_internal(channel);
}

/**
 * @brief Send the specified buffer on the given channel
 */
int
hv_vmbus_channel_send_packet(
	hv_vmbus_channel*	channel,
	void*			buffer,
	uint32_t		buffer_len,
	uint64_t		request_id,
	hv_vmbus_packet_type	type,
	uint32_t		flags)
{
	int			ret = 0;
	hv_vm_packet_descriptor	desc;
	uint32_t		packet_len;
	uint64_t		aligned_data;
	uint32_t		packet_len_aligned;
	boolean_t		need_sig;
	hv_vmbus_sg_buffer_list	buffer_list[3];

	packet_len = sizeof(hv_vm_packet_descriptor) + buffer_len;
	packet_len_aligned = HV_ALIGN_UP(packet_len, sizeof(uint64_t));
	aligned_data = 0;

	/* Setup the descriptor */
	desc.type = type;   /* HV_VMBUS_PACKET_TYPE_DATA_IN_BAND;             */
	desc.flags = flags; /* HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED */
			    /* in 8-bytes granularity */
	desc.data_offset8 = sizeof(hv_vm_packet_descriptor) >> 3;
	desc.length8 = (uint16_t) (packet_len_aligned >> 3);
	desc.transaction_id = request_id;

	buffer_list[0].data = &desc;
	buffer_list[0].length = sizeof(hv_vm_packet_descriptor);

	buffer_list[1].data = buffer;
	buffer_list[1].length = buffer_len;

	buffer_list[2].data = &aligned_data;
	buffer_list[2].length = packet_len_aligned - packet_len;

	ret = hv_ring_buffer_write(&channel->outbound, buffer_list, 3,
	    &need_sig);

	/* TODO: We should determine if this is optional */
	if (ret == 0 && need_sig) {
		vmbus_channel_set_event(channel);
	}

	return (ret);
}

/**
 * @brief Send a range of single-page buffer packets using
 * a GPADL Direct packet type
 */
int
hv_vmbus_channel_send_packet_pagebuffer(
	hv_vmbus_channel*	channel,
	hv_vmbus_page_buffer	page_buffers[],
	uint32_t		page_count,
	void*			buffer,
	uint32_t		buffer_len,
	uint64_t		request_id)
{

	int					ret = 0;
	int					i = 0;
	boolean_t				need_sig;
	uint32_t				packet_len;
	uint32_t				packetLen_aligned;
	hv_vmbus_sg_buffer_list			buffer_list[3];
	hv_vmbus_channel_packet_page_buffer	desc;
	uint32_t				descSize;
	uint64_t				alignedData = 0;

	if (page_count > HV_MAX_PAGE_BUFFER_COUNT)
		return (EINVAL);

	/*
	 * Adjust the size down since hv_vmbus_channel_packet_page_buffer
	 *  is the largest size we support
	 */
	descSize = sizeof(hv_vmbus_channel_packet_page_buffer) -
			((HV_MAX_PAGE_BUFFER_COUNT - page_count) *
			sizeof(hv_vmbus_page_buffer));
	packet_len = descSize + buffer_len;
	packetLen_aligned = HV_ALIGN_UP(packet_len, sizeof(uint64_t));

	/* Setup the descriptor */
	desc.type = HV_VMBUS_PACKET_TYPE_DATA_USING_GPA_DIRECT;
	desc.flags = HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
	desc.data_offset8 = descSize >> 3; /* in 8-bytes granularity */
	desc.length8 = (uint16_t) (packetLen_aligned >> 3);
	desc.transaction_id = request_id;
	desc.range_count = page_count;

	for (i = 0; i < page_count; i++) {
		desc.range[i].length = page_buffers[i].length;
		desc.range[i].offset = page_buffers[i].offset;
		desc.range[i].pfn = page_buffers[i].pfn;
	}

	buffer_list[0].data = &desc;
	buffer_list[0].length = descSize;

	buffer_list[1].data = buffer;
	buffer_list[1].length = buffer_len;

	buffer_list[2].data = &alignedData;
	buffer_list[2].length = packetLen_aligned - packet_len;

	ret = hv_ring_buffer_write(&channel->outbound, buffer_list, 3,
	    &need_sig);

	/* TODO: We should determine if this is optional */
	if (ret == 0 && need_sig) {
		vmbus_channel_set_event(channel);
	}

	return (ret);
}

/**
 * @brief Send a multi-page buffer packet using a GPADL Direct packet type
 */
int
hv_vmbus_channel_send_packet_multipagebuffer(
	hv_vmbus_channel*		channel,
	hv_vmbus_multipage_buffer*	multi_page_buffer,
	void*				buffer,
	uint32_t			buffer_len,
	uint64_t			request_id)
{

	int			ret = 0;
	uint32_t		desc_size;
	boolean_t		need_sig;
	uint32_t		packet_len;
	uint32_t		packet_len_aligned;
	uint32_t		pfn_count;
	uint64_t		aligned_data = 0;
	hv_vmbus_sg_buffer_list	buffer_list[3];
	hv_vmbus_channel_packet_multipage_buffer desc;

	pfn_count =
	    HV_NUM_PAGES_SPANNED(
		    multi_page_buffer->offset,
		    multi_page_buffer->length);

	if ((pfn_count == 0) || (pfn_count > HV_MAX_MULTIPAGE_BUFFER_COUNT))
	    return (EINVAL);
	/*
	 * Adjust the size down since hv_vmbus_channel_packet_multipage_buffer
	 * is the largest size we support
	 */
	desc_size =
	    sizeof(hv_vmbus_channel_packet_multipage_buffer) -
		    ((HV_MAX_MULTIPAGE_BUFFER_COUNT - pfn_count) *
			sizeof(uint64_t));
	packet_len = desc_size + buffer_len;
	packet_len_aligned = HV_ALIGN_UP(packet_len, sizeof(uint64_t));

	/*
	 * Setup the descriptor
	 */
	desc.type = HV_VMBUS_PACKET_TYPE_DATA_USING_GPA_DIRECT;
	desc.flags = HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
	desc.data_offset8 = desc_size >> 3; /* in 8-bytes granularity */
	desc.length8 = (uint16_t) (packet_len_aligned >> 3);
	desc.transaction_id = request_id;
	desc.range_count = 1;

	desc.range.length = multi_page_buffer->length;
	desc.range.offset = multi_page_buffer->offset;

	memcpy(desc.range.pfn_array, multi_page_buffer->pfn_array,
		pfn_count * sizeof(uint64_t));

	buffer_list[0].data = &desc;
	buffer_list[0].length = desc_size;

	buffer_list[1].data = buffer;
	buffer_list[1].length = buffer_len;

	buffer_list[2].data = &aligned_data;
	buffer_list[2].length = packet_len_aligned - packet_len;

	ret = hv_ring_buffer_write(&channel->outbound, buffer_list, 3,
	    &need_sig);

	/* TODO: We should determine if this is optional */
	if (ret == 0 && need_sig) {
	    vmbus_channel_set_event(channel);
	}

	return (ret);
}

/**
 * @brief Retrieve the user packet on the specified channel
 */
int
hv_vmbus_channel_recv_packet(
	hv_vmbus_channel*	channel,
	void*			Buffer,
	uint32_t		buffer_len,
	uint32_t*		buffer_actual_len,
	uint64_t*		request_id)
{
	int			ret;
	uint32_t		user_len;
	uint32_t		packet_len;
	hv_vm_packet_descriptor	desc;

	*buffer_actual_len = 0;
	*request_id = 0;

	ret = hv_ring_buffer_peek(&channel->inbound, &desc,
		sizeof(hv_vm_packet_descriptor));
	if (ret != 0)
		return (0);

	packet_len = desc.length8 << 3;
	user_len = packet_len - (desc.data_offset8 << 3);

	*buffer_actual_len = user_len;

	if (user_len > buffer_len)
		return (EINVAL);

	*request_id = desc.transaction_id;

	/* Copy over the packet to the user buffer */
	ret = hv_ring_buffer_read(&channel->inbound, Buffer, user_len,
		(desc.data_offset8 << 3));

	return (0);
}

/**
 * @brief Retrieve the raw packet on the specified channel
 */
int
hv_vmbus_channel_recv_packet_raw(
	hv_vmbus_channel*	channel,
	void*			buffer,
	uint32_t		buffer_len,
	uint32_t*		buffer_actual_len,
	uint64_t*		request_id)
{
	int		ret;
	uint32_t	packetLen;
	uint32_t	userLen;
	hv_vm_packet_descriptor	desc;

	*buffer_actual_len = 0;
	*request_id = 0;

	ret = hv_ring_buffer_peek(
		&channel->inbound, &desc,
		sizeof(hv_vm_packet_descriptor));

	if (ret != 0)
	    return (0);

	packetLen = desc.length8 << 3;
	userLen = packetLen - (desc.data_offset8 << 3);

	*buffer_actual_len = packetLen;

	if (packetLen > buffer_len)
	    return (ENOBUFS);

	*request_id = desc.transaction_id;

	/* Copy over the entire packet to the user buffer */
	ret = hv_ring_buffer_read(&channel->inbound, buffer, packetLen, 0);

	return (0);
}
