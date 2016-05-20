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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

static int 	vmbus_channel_create_gpadl_header(
			/* must be phys and virt contiguous*/
			void*				contig_buffer,
			/* page-size multiple */
			uint32_t 			size,
			hv_vmbus_channel_msg_info**	msg_info,
			uint32_t*			message_count);

static void 	vmbus_channel_set_event(hv_vmbus_channel* channel);
static void	VmbusProcessChannelEvent(void* channel, int pending);

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
			hv_vmbus_g_connection.monitor_page_2;

		synch_set_bit(channel->monitor_bit,
			(uint32_t *)&monitor_page->
				trigger_group[channel->monitor_group].u.pending);
	} else {
		hv_vmbus_set_event(channel);
	}

}

static int
vmbus_channel_sysctl_monalloc(SYSCTL_HANDLER_ARGS)
{
	struct hv_vmbus_channel *chan = arg1;
	int alloc = 0;

	if (chan->offer_msg.monitor_allocated)
		alloc = 1;
	return sysctl_handle_int(oidp, &alloc, 0, req);
}

static void
vmbus_channel_sysctl_create(hv_vmbus_channel* channel)
{
	device_t dev;
	struct sysctl_oid *devch_sysctl;
	struct sysctl_oid *devch_id_sysctl, *devch_sub_sysctl;
	struct sysctl_oid *devch_id_in_sysctl, *devch_id_out_sysctl;
	struct sysctl_ctx_list *ctx;
	uint32_t ch_id;
	uint16_t sub_ch_id;
	char name[16];
	
	hv_vmbus_channel* primary_ch = channel->primary_channel;

	if (primary_ch == NULL) {
		dev = channel->device->device;
		ch_id = channel->offer_msg.child_rel_id;
	} else {
		dev = primary_ch->device->device;
		ch_id = primary_ch->offer_msg.child_rel_id;
		sub_ch_id = channel->offer_msg.offer.sub_channel_index;
	}
	ctx = device_get_sysctl_ctx(dev);
	/* This creates dev.DEVNAME.DEVUNIT.channel tree */
	devch_sysctl = SYSCTL_ADD_NODE(ctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, "channel", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");
	/* This creates dev.DEVNAME.DEVUNIT.channel.CHANID tree */
	snprintf(name, sizeof(name), "%d", ch_id);
	devch_id_sysctl = SYSCTL_ADD_NODE(ctx,
	    	    SYSCTL_CHILDREN(devch_sysctl),
	    	    OID_AUTO, name, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

	if (primary_ch != NULL) {
		devch_sub_sysctl = SYSCTL_ADD_NODE(ctx,
			SYSCTL_CHILDREN(devch_id_sysctl),
			OID_AUTO, "sub", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");
		snprintf(name, sizeof(name), "%d", sub_ch_id);
		devch_id_sysctl = SYSCTL_ADD_NODE(ctx,
			SYSCTL_CHILDREN(devch_sub_sysctl),
			OID_AUTO, name, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(devch_id_sysctl),
		    OID_AUTO, "chanid", CTLFLAG_RD,
		    &channel->offer_msg.child_rel_id, 0, "channel id");
	}
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(devch_id_sysctl), OID_AUTO,
	    "cpu", CTLFLAG_RD, &channel->target_cpu, 0, "owner CPU id");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(devch_id_sysctl), OID_AUTO,
	    "monitor_allocated", CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    channel, 0, vmbus_channel_sysctl_monalloc, "I",
	    "is monitor allocated to this channel");

	devch_id_in_sysctl = SYSCTL_ADD_NODE(ctx,
                    SYSCTL_CHILDREN(devch_id_sysctl),
                    OID_AUTO,
		    "in",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");
	devch_id_out_sysctl = SYSCTL_ADD_NODE(ctx,
                    SYSCTL_CHILDREN(devch_id_sysctl),
                    OID_AUTO,
		    "out",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");
	hv_ring_buffer_stat(ctx,
		SYSCTL_CHILDREN(devch_id_in_sysctl),
		&(channel->inbound),
		"inbound ring buffer stats");
	hv_ring_buffer_stat(ctx,
		SYSCTL_CHILDREN(devch_id_out_sysctl),
		&(channel->outbound),
		"outbound ring buffer stats");
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

	vmbus_on_channel_open(new_channel);

	new_channel->rxq = hv_vmbus_g_context.hv_event_queue[new_channel->target_cpu];
	TASK_INIT(&new_channel->channel_task, 0, VmbusProcessChannelEvent, new_channel);

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

	/* Create sysctl tree for this channel */
	vmbus_channel_sysctl_create(new_channel);

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

	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_INSERT_TAIL(
		&hv_vmbus_g_connection.channel_msg_anchor,
		open_info,
		msg_list_entry);
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

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
	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(
		&hv_vmbus_g_connection.channel_msg_anchor,
		open_info,
		msg_list_entry);
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);
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

	next_gpadl_handle = atomic_fetchadd_int(
	    &hv_vmbus_g_connection.next_gpadl_handle, 1);

	ret = vmbus_channel_create_gpadl_header(
		contig_buffer, size, &msg_info, &msg_count);

	if(ret != 0) {
		/*
		 * XXX
		 * We can _not_ even revert the above incremental,
		 * if multiple GPADL establishments are running
		 * parallelly, decrement the global next_gpadl_handle
		 * is calling for _big_ trouble.  A better solution
		 * is to have a 0-based GPADL id bitmap ...
		 */
		return ret;
	}

	sema_init(&msg_info->wait_sema, 0, "Open Info Sema");
	gpadl_msg = (hv_vmbus_channel_gpadl_header*) msg_info->msg;
	gpadl_msg->header.message_type = HV_CHANNEL_MESSAGEL_GPADL_HEADER;
	gpadl_msg->child_rel_id = channel->offer_msg.child_rel_id;
	gpadl_msg->gpadl = next_gpadl_handle;

	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_INSERT_TAIL(
		&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info,
		msg_list_entry);

	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

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

	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(&hv_vmbus_g_connection.channel_msg_anchor,
		msg_info, msg_list_entry);
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

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

	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_INSERT_TAIL(&hv_vmbus_g_connection.channel_msg_anchor,
			info, msg_list_entry);
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

	ret = hv_vmbus_post_message(msg,
			sizeof(hv_vmbus_channel_gpadl_teardown));
	if (ret != 0) 
	    goto cleanup;
	
	ret = sema_timedwait(&info->wait_sema, 5 * hz); /* KYS 5 seconds */

cleanup:
	/*
	 * Received a torndown response
	 */
	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_REMOVE(&hv_vmbus_g_connection.channel_msg_anchor,
			info, msg_list_entry);
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);
	sema_destroy(&info->wait_sema);
	free(info, M_DEVBUF);

	return (ret);
}

static void
hv_vmbus_channel_close_internal(hv_vmbus_channel *channel)
{
	int ret = 0;
	struct taskqueue *rxq = channel->rxq;
	hv_vmbus_channel_close_channel* msg;
	hv_vmbus_channel_msg_info* info;

	channel->state = HV_CHANNEL_OPEN_STATE;

	/*
	 * set rxq to NULL to avoid more requests be scheduled
	 */
	channel->rxq = NULL;
	taskqueue_drain(rxq, &channel->channel_task);
	channel->on_channel_callback = NULL;

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
	boolean_t				need_sig;
	uint32_t				packet_len;
	uint32_t				page_buflen;
	uint32_t				packetLen_aligned;
	hv_vmbus_sg_buffer_list			buffer_list[4];
	hv_vmbus_channel_packet_page_buffer	desc;
	uint32_t				descSize;
	uint64_t				alignedData = 0;

	if (page_count > HV_MAX_PAGE_BUFFER_COUNT)
		return (EINVAL);

	/*
	 * Adjust the size down since hv_vmbus_channel_packet_page_buffer
	 *  is the largest size we support
	 */
	descSize = __offsetof(hv_vmbus_channel_packet_page_buffer, range);
	page_buflen = sizeof(hv_vmbus_page_buffer) * page_count;
	packet_len = descSize + page_buflen + buffer_len;
	packetLen_aligned = HV_ALIGN_UP(packet_len, sizeof(uint64_t));

	/* Setup the descriptor */
	desc.type = HV_VMBUS_PACKET_TYPE_DATA_USING_GPA_DIRECT;
	desc.flags = HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED;
	/* in 8-bytes granularity */
	desc.data_offset8 = (descSize + page_buflen) >> 3;
	desc.length8 = (uint16_t) (packetLen_aligned >> 3);
	desc.transaction_id = request_id;
	desc.range_count = page_count;

	buffer_list[0].data = &desc;
	buffer_list[0].length = descSize;

	buffer_list[1].data = page_buffers;
	buffer_list[1].length = page_buflen;

	buffer_list[2].data = buffer;
	buffer_list[2].length = buffer_len;

	buffer_list[3].data = &alignedData;
	buffer_list[3].length = packetLen_aligned - packet_len;

	ret = hv_ring_buffer_write(&channel->outbound, buffer_list, 4,
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
	hv_vm_packet_descriptor	desc;

	*buffer_actual_len = 0;
	*request_id = 0;

	ret = hv_ring_buffer_peek(
		&channel->inbound, &desc,
		sizeof(hv_vm_packet_descriptor));

	if (ret != 0)
	    return (0);

	packetLen = desc.length8 << 3;
	*buffer_actual_len = packetLen;

	if (packetLen > buffer_len)
	    return (ENOBUFS);

	*request_id = desc.transaction_id;

	/* Copy over the entire packet to the user buffer */
	ret = hv_ring_buffer_read(&channel->inbound, buffer, packetLen, 0);

	return (0);
}


/**
 * Process a channel event notification
 */
static void
VmbusProcessChannelEvent(void* context, int pending)
{
	void* arg;
	uint32_t bytes_to_read;
	hv_vmbus_channel* channel = (hv_vmbus_channel*)context;
	boolean_t is_batched_reading;

	/**
	 * Find the channel based on this relid and invokes
	 * the channel callback to process the event
	 */

	if (channel == NULL) {
		return;
	}
	/**
	 * To deal with the race condition where we might
	 * receive a packet while the relevant driver is
	 * being unloaded, dispatch the callback while
	 * holding the channel lock. The unloading driver
	 * will acquire the same channel lock to set the
	 * callback to NULL. This closes the window.
	 */

	if (channel->on_channel_callback != NULL) {
		arg = channel->channel_callback_context;
		is_batched_reading = channel->batched_reading;
		/*
		 * Optimize host to guest signaling by ensuring:
		 * 1. While reading the channel, we disable interrupts from
		 *    host.
		 * 2. Ensure that we process all posted messages from the host
		 *    before returning from this callback.
		 * 3. Once we return, enable signaling from the host. Once this
		 *    state is set we check to see if additional packets are
		 *    available to read. In this case we repeat the process.
		 */
		do {
			if (is_batched_reading)
				hv_ring_buffer_read_begin(&channel->inbound);

			channel->on_channel_callback(arg);

			if (is_batched_reading)
				bytes_to_read =
				    hv_ring_buffer_read_end(&channel->inbound);
			else
				bytes_to_read = 0;
		} while (is_batched_reading && (bytes_to_read != 0));
	}
}
