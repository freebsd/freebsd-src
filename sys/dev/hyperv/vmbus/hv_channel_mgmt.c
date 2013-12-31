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

#include <sys/param.h>
#include <sys/mbuf.h>

#include "hv_vmbus_priv.h"

typedef void (*hv_pfn_channel_msg_handler)(hv_vmbus_channel_msg_header* msg);

typedef struct hv_vmbus_channel_msg_table_entry {
	hv_vmbus_channel_msg_type    messageType;
	hv_pfn_channel_msg_handler   messageHandler;
} hv_vmbus_channel_msg_table_entry;

/*
 * Internal functions
 */

static void vmbus_channel_on_offer(hv_vmbus_channel_msg_header* hdr);
static void vmbus_channel_on_open_result(hv_vmbus_channel_msg_header* hdr);
static void vmbus_channel_on_offer_rescind(hv_vmbus_channel_msg_header* hdr);
static void vmbus_channel_on_gpadl_created(hv_vmbus_channel_msg_header* hdr);
static void vmbus_channel_on_gpadl_torndown(hv_vmbus_channel_msg_header* hdr);
static void vmbus_channel_on_offers_delivered(hv_vmbus_channel_msg_header* hdr);
static void vmbus_channel_on_version_response(hv_vmbus_channel_msg_header* hdr);
static void vmbus_channel_process_offer(void *context);

/**
 * Channel message dispatch table
 */
hv_vmbus_channel_msg_table_entry
    g_channel_message_table[HV_CHANNEL_MESSAGE_COUNT] = {
	{ HV_CHANNEL_MESSAGE_INVALID, NULL },
	{ HV_CHANNEL_MESSAGE_OFFER_CHANNEL, vmbus_channel_on_offer },
	{ HV_CHANNEL_MESSAGE_RESCIND_CHANNEL_OFFER,
		vmbus_channel_on_offer_rescind },
	{ HV_CHANNEL_MESSAGE_REQUEST_OFFERS, NULL },
	{ HV_CHANNEL_MESSAGE_ALL_OFFERS_DELIVERED,
		vmbus_channel_on_offers_delivered },
	{ HV_CHANNEL_MESSAGE_OPEN_CHANNEL, NULL },
	{ HV_CHANNEL_MESSAGE_OPEN_CHANNEL_RESULT,
		vmbus_channel_on_open_result },
	{ HV_CHANNEL_MESSAGE_CLOSE_CHANNEL, NULL },
	{ HV_CHANNEL_MESSAGEL_GPADL_HEADER, NULL },
	{ HV_CHANNEL_MESSAGE_GPADL_BODY, NULL },
	{ HV_CHANNEL_MESSAGE_GPADL_CREATED,
		vmbus_channel_on_gpadl_created },
	{ HV_CHANNEL_MESSAGE_GPADL_TEARDOWN, NULL },
	{ HV_CHANNEL_MESSAGE_GPADL_TORNDOWN,
		vmbus_channel_on_gpadl_torndown },
	{ HV_CHANNEL_MESSAGE_REL_ID_RELEASED, NULL },
	{ HV_CHANNEL_MESSAGE_INITIATED_CONTACT, NULL },
	{ HV_CHANNEL_MESSAGE_VERSION_RESPONSE,
		vmbus_channel_on_version_response },
	{ HV_CHANNEL_MESSAGE_UNLOAD, NULL }
};


/**
 * Implementation of the work abstraction.
 */
static void
work_item_callback(void *work, int pending)
{
	struct hv_work_item *w = (struct hv_work_item *)work;

	/*
	 * Serialize work execution.
	 */
	if (w->wq->work_sema != NULL) {
		sema_wait(w->wq->work_sema);
	}

	w->callback(w->context);

	if (w->wq->work_sema != NULL) {
		sema_post(w->wq->work_sema);
	} 

	free(w, M_DEVBUF);
}

struct hv_work_queue*
hv_work_queue_create(char* name)
{
	static unsigned int	qid = 0;
	char			qname[64];
	int			pri;
	struct hv_work_queue*	wq;

	wq = malloc(sizeof(struct hv_work_queue), M_DEVBUF, M_NOWAIT | M_ZERO);
	KASSERT(wq != NULL, ("Error VMBUS: Failed to allocate work_queue\n"));
	if (wq == NULL)
	    return (NULL);

	/*
	 * We use work abstraction to handle messages
	 * coming from the host and these are typically offers.
	 * Some FreeBsd drivers appear to have a concurrency issue
	 * where probe/attach needs to be serialized. We ensure that
	 * by having only one thread process work elements in a 
	 * specific queue by serializing work execution.
	 *
	 */
	if (strcmp(name, "vmbusQ") == 0) {
	    pri = PI_DISK;
	} else { /* control */
	    pri = PI_NET;
	    /*
	     * Initialize semaphore for this queue by pointing
	     * to the globale semaphore used for synchronizing all
	     * control messages.
	     */
	    wq->work_sema = &hv_vmbus_g_connection.control_sema;
	}

	sprintf(qname, "hv_%s_%u", name, qid);

	/*
	 * Fixme:  FreeBSD 8.2 has a different prototype for
	 * taskqueue_create(), and for certain other taskqueue functions.
	 * We need to research the implications of these changes.
	 * Fixme:  Not sure when the changes were introduced.
	 */
	wq->queue = taskqueue_create(qname, M_NOWAIT, taskqueue_thread_enqueue,
	    &wq->queue
	    #if __FreeBSD_version < 800000
	    , &wq->proc
	    #endif
	    );

	if (wq->queue == NULL) {
	    free(wq, M_DEVBUF);
	    return (NULL);
	}

	if (taskqueue_start_threads(&wq->queue, 1, pri, "%s taskq", qname)) {
	    taskqueue_free(wq->queue);
	    free(wq, M_DEVBUF);
	    return (NULL);
	}

	qid++;

	return (wq);
}

void
hv_work_queue_close(struct hv_work_queue *wq)
{
	/*
	 * KYS: Need to drain the taskqueue
	 * before we close the hv_work_queue.
	 */
	/*KYS: taskqueue_drain(wq->tq, ); */
	taskqueue_free(wq->queue);
	free(wq, M_DEVBUF);
}

/**
 * @brief Create work item
 */
int
hv_queue_work_item(
	struct hv_work_queue *wq,
	void (*callback)(void *), void *context)
{
	struct hv_work_item *w = malloc(sizeof(struct hv_work_item),
					M_DEVBUF, M_NOWAIT | M_ZERO);
	KASSERT(w != NULL, ("Error VMBUS: Failed to allocate WorkItem\n"));
	if (w == NULL)
	    return (ENOMEM);

	w->callback = callback;
	w->context = context;
	w->wq = wq;

	TASK_INIT(&w->work, 0, work_item_callback, w);

	return (taskqueue_enqueue(wq->queue, &w->work));
}

/**
 * @brief Rescind the offer by initiating a device removal
 */
static void
vmbus_channel_process_rescind_offer(void *context)
{
	hv_vmbus_channel* channel = (hv_vmbus_channel*) context;
	hv_vmbus_child_device_unregister(channel->device);
}

/**
 * @brief Allocate and initialize a vmbus channel object
 */
hv_vmbus_channel*
hv_vmbus_allocate_channel(void)
{
	hv_vmbus_channel* channel;

	channel = (hv_vmbus_channel*) malloc(
					sizeof(hv_vmbus_channel),
					M_DEVBUF,
					M_NOWAIT | M_ZERO);
	KASSERT(channel != NULL, ("Error VMBUS: Failed to allocate channel!"));
	if (channel == NULL)
	    return (NULL);

	mtx_init(&channel->inbound_lock, "channel inbound", NULL, MTX_DEF);

	channel->control_work_queue = hv_work_queue_create("control");

	if (channel->control_work_queue == NULL) {
	    mtx_destroy(&channel->inbound_lock);
	    free(channel, M_DEVBUF);
	    return (NULL);
	}

	return (channel);
}

/**
 * @brief Release the vmbus channel object itself
 */
static inline void
ReleaseVmbusChannel(void *context)
{
	hv_vmbus_channel* channel = (hv_vmbus_channel*) context;
	hv_work_queue_close(channel->control_work_queue);
	free(channel, M_DEVBUF);
}

/**
 * @brief Release the resources used by the vmbus channel object
 */
void
hv_vmbus_free_vmbus_channel(hv_vmbus_channel* channel)
{
	mtx_destroy(&channel->inbound_lock);
	/*
	 * We have to release the channel's workqueue/thread in
	 *  the vmbus's workqueue/thread context
	 * ie we can't destroy ourselves
	 */
	hv_queue_work_item(hv_vmbus_g_connection.work_queue,
	    ReleaseVmbusChannel, (void *) channel);
}

/**
 * @brief Process the offer by creating a channel/device
 * associated with this offer
 */
static void
vmbus_channel_process_offer(void *context)
{
	int			ret;
	hv_vmbus_channel*	new_channel;
	boolean_t		f_new;
	hv_vmbus_channel*	channel;

	new_channel = (hv_vmbus_channel*) context;
	f_new = TRUE;
	channel = NULL;

	/*
	 * Make sure this is a new offer
	 */
	mtx_lock_spin(&hv_vmbus_g_connection.channel_lock);

	TAILQ_FOREACH(channel, &hv_vmbus_g_connection.channel_anchor,
	    list_entry)
	{
	    if (!memcmp(
		&channel->offer_msg.offer.interface_type,
		&new_channel->offer_msg.offer.interface_type,
		sizeof(hv_guid))
		&& !memcmp(
		    &channel->offer_msg.offer.interface_instance,
		    &new_channel->offer_msg.offer.interface_instance,
		    sizeof(hv_guid))) {
		f_new = FALSE;
		break;
	    }
	}

	if (f_new) {
	    /* Insert at tail */
	    TAILQ_INSERT_TAIL(
		&hv_vmbus_g_connection.channel_anchor,
		new_channel,
		list_entry);
	}
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_lock);

	if (!f_new) {
	    hv_vmbus_free_vmbus_channel(new_channel);
	    return;
	}

	/*
	 * Start the process of binding this offer to the driver
	 * (We need to set the device field before calling
	 * hv_vmbus_child_device_add())
	 */
	new_channel->device = hv_vmbus_child_device_create(
	    new_channel->offer_msg.offer.interface_type,
	    new_channel->offer_msg.offer.interface_instance, new_channel);

	/*
	 *  TODO - the HV_CHANNEL_OPEN_STATE flag should not be set below
	 *  but in the "open" channel request. The ret != 0 logic below
	 *  doesn't take into account that a channel
	 *  may have been opened successfully
	 */

	/*
	 * Add the new device to the bus. This will kick off device-driver
	 * binding which eventually invokes the device driver's AddDevice()
	 * method.
	 */
	ret = hv_vmbus_child_device_register(new_channel->device);
	if (ret != 0) {
	    mtx_lock_spin(&hv_vmbus_g_connection.channel_lock);
	    TAILQ_REMOVE(
		&hv_vmbus_g_connection.channel_anchor,
		new_channel,
		list_entry);
	    mtx_unlock_spin(&hv_vmbus_g_connection.channel_lock);
	    hv_vmbus_free_vmbus_channel(new_channel);
	} else {
	    /*
	     * This state is used to indicate a successful open
	     * so that when we do close the channel normally,
	     * we can clean up properly
	     */
	    new_channel->state = HV_CHANNEL_OPEN_STATE;

	}
}

/**
 * @brief Handler for channel offers from Hyper-V/Azure
 *
 * Handler for channel offers from vmbus in parent partition. We ignore
 * all offers except network and storage offers. For each network and storage
 * offers, we create a channel object and queue a work item to the channel
 * object to process the offer synchronously
 */
static void
vmbus_channel_on_offer(hv_vmbus_channel_msg_header* hdr)
{
	hv_vmbus_channel_offer_channel* offer;
	hv_vmbus_channel* new_channel;

	offer = (hv_vmbus_channel_offer_channel*) hdr;

	hv_guid *guidType;
	hv_guid *guidInstance;

	guidType = &offer->offer.interface_type;
	guidInstance = &offer->offer.interface_instance;

	/* Allocate the channel object and save this offer */
	new_channel = hv_vmbus_allocate_channel();
	if (new_channel == NULL)
	    return;

	memcpy(&new_channel->offer_msg, offer,
	    sizeof(hv_vmbus_channel_offer_channel));
	new_channel->monitor_group = (uint8_t) offer->monitor_id / 32;
	new_channel->monitor_bit = (uint8_t) offer->monitor_id % 32;

	/* TODO: Make sure the offer comes from our parent partition */
	hv_queue_work_item(
	    new_channel->control_work_queue,
	    vmbus_channel_process_offer,
	    new_channel);
}

/**
 * @brief Rescind offer handler.
 *
 * We queue a work item to process this offer
 * synchronously
 */
static void
vmbus_channel_on_offer_rescind(hv_vmbus_channel_msg_header* hdr)
{
	hv_vmbus_channel_rescind_offer*	rescind;
	hv_vmbus_channel*		channel;

	rescind = (hv_vmbus_channel_rescind_offer*) hdr;

	channel = hv_vmbus_get_channel_from_rel_id(rescind->child_rel_id);
	if (channel == NULL) 
	    return;

	hv_queue_work_item(channel->control_work_queue,
	    vmbus_channel_process_rescind_offer, channel);
}

/**
 *
 * @brief Invoked when all offers have been delivered.
 */
static void
vmbus_channel_on_offers_delivered(hv_vmbus_channel_msg_header* hdr)
{
}

/**
 * @brief Open result handler.
 *
 * This is invoked when we received a response
 * to our channel open request. Find the matching request, copy the
 * response and signal the requesting thread.
 */
static void
vmbus_channel_on_open_result(hv_vmbus_channel_msg_header* hdr)
{
	hv_vmbus_channel_open_result*	result;
	hv_vmbus_channel_msg_info*	msg_info;
	hv_vmbus_channel_msg_header*	requestHeader;
	hv_vmbus_channel_open_channel*	openMsg;

	result = (hv_vmbus_channel_open_result*) hdr;

	/*
	 * Find the open msg, copy the result and signal/unblock the wait event
	 */
	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	TAILQ_FOREACH(msg_info, &hv_vmbus_g_connection.channel_msg_anchor,
	    msg_list_entry) {
	    requestHeader = (hv_vmbus_channel_msg_header*) msg_info->msg;

	    if (requestHeader->message_type ==
		    HV_CHANNEL_MESSAGE_OPEN_CHANNEL) {
		openMsg = (hv_vmbus_channel_open_channel*) msg_info->msg;
		if (openMsg->child_rel_id == result->child_rel_id
		    && openMsg->open_id == result->open_id) {
		    memcpy(&msg_info->response.open_result, result,
			sizeof(hv_vmbus_channel_open_result));
		    sema_post(&msg_info->wait_sema);
		    break;
		}
	    }
	}
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

}

/**
 * @brief GPADL created handler.
 *
 * This is invoked when we received a response
 * to our gpadl create request. Find the matching request, copy the
 * response and signal the requesting thread.
 */
static void
vmbus_channel_on_gpadl_created(hv_vmbus_channel_msg_header* hdr)
{
	hv_vmbus_channel_gpadl_created*		gpadl_created;
	hv_vmbus_channel_msg_info*		msg_info;
	hv_vmbus_channel_msg_header*		request_header;
	hv_vmbus_channel_gpadl_header*		gpadl_header;

	gpadl_created = (hv_vmbus_channel_gpadl_created*) hdr;

	/* Find the establish msg, copy the result and signal/unblock
	 * the wait event
	 */
	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_FOREACH(msg_info, &hv_vmbus_g_connection.channel_msg_anchor,
		msg_list_entry) {
	    request_header = (hv_vmbus_channel_msg_header*) msg_info->msg;
	    if (request_header->message_type ==
		    HV_CHANNEL_MESSAGEL_GPADL_HEADER) {
		gpadl_header =
		    (hv_vmbus_channel_gpadl_header*) request_header;

		if ((gpadl_created->child_rel_id == gpadl_header->child_rel_id)
		    && (gpadl_created->gpadl == gpadl_header->gpadl)) {
		    memcpy(&msg_info->response.gpadl_created,
			gpadl_created,
			sizeof(hv_vmbus_channel_gpadl_created));
		    sema_post(&msg_info->wait_sema);
		    break;
		}
	    }
	}
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);
}

/**
 * @brief GPADL torndown handler.
 *
 * This is invoked when we received a respons
 * to our gpadl teardown request. Find the matching request, copy the
 * response and signal the requesting thread
 */
static void
vmbus_channel_on_gpadl_torndown(hv_vmbus_channel_msg_header* hdr)
{
	hv_vmbus_channel_gpadl_torndown*	gpadl_torndown;
	hv_vmbus_channel_msg_info*		msg_info;
	hv_vmbus_channel_msg_header*		requestHeader;
	hv_vmbus_channel_gpadl_teardown*	gpadlTeardown;

	gpadl_torndown = (hv_vmbus_channel_gpadl_torndown*)hdr;

	/*
	 * Find the open msg, copy the result and signal/unblock the
	 * wait event.
	 */

	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);

	TAILQ_FOREACH(msg_info, &hv_vmbus_g_connection.channel_msg_anchor,
		msg_list_entry) {
	    requestHeader = (hv_vmbus_channel_msg_header*) msg_info->msg;

	    if (requestHeader->message_type
		    == HV_CHANNEL_MESSAGE_GPADL_TEARDOWN) {
		gpadlTeardown =
		    (hv_vmbus_channel_gpadl_teardown*) requestHeader;

		if (gpadl_torndown->gpadl == gpadlTeardown->gpadl) {
		    memcpy(&msg_info->response.gpadl_torndown,
			gpadl_torndown,
			sizeof(hv_vmbus_channel_gpadl_torndown));
		    sema_post(&msg_info->wait_sema);
		    break;
		}
	    }
	}
    mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);
}

/**
 * @brief Version response handler.
 *
 * This is invoked when we received a response
 * to our initiate contact request. Find the matching request, copy th
 * response and signal the requesting thread.
 */
static void
vmbus_channel_on_version_response(hv_vmbus_channel_msg_header* hdr)
{
	hv_vmbus_channel_msg_info*		msg_info;
	hv_vmbus_channel_msg_header*		requestHeader;
	hv_vmbus_channel_initiate_contact*	initiate;
	hv_vmbus_channel_version_response*	versionResponse;

	versionResponse = (hv_vmbus_channel_version_response*)hdr;

	mtx_lock_spin(&hv_vmbus_g_connection.channel_msg_lock);
	TAILQ_FOREACH(msg_info, &hv_vmbus_g_connection.channel_msg_anchor,
	    msg_list_entry) {
	    requestHeader = (hv_vmbus_channel_msg_header*) msg_info->msg;
	    if (requestHeader->message_type
		== HV_CHANNEL_MESSAGE_INITIATED_CONTACT) {
		initiate =
		    (hv_vmbus_channel_initiate_contact*) requestHeader;
		memcpy(&msg_info->response.version_response,
		    versionResponse,
		    sizeof(hv_vmbus_channel_version_response));
		sema_post(&msg_info->wait_sema);
	    }
	}
    mtx_unlock_spin(&hv_vmbus_g_connection.channel_msg_lock);

}

/**
 * @brief Handler for channel protocol messages.
 *
 * This is invoked in the vmbus worker thread context.
 */
void
hv_vmbus_on_channel_message(void *context)
{
	hv_vmbus_message*		msg;
	hv_vmbus_channel_msg_header*	hdr;
	int				size;

	msg = (hv_vmbus_message*) context;
	hdr = (hv_vmbus_channel_msg_header*) msg->u.payload;
	size = msg->header.payload_size;

	if (hdr->message_type >= HV_CHANNEL_MESSAGE_COUNT) {
	    free(msg, M_DEVBUF);
	    return;
	}

	if (g_channel_message_table[hdr->message_type].messageHandler) {
	    g_channel_message_table[hdr->message_type].messageHandler(hdr);
	}

	/* Free the msg that was allocated in VmbusOnMsgDPC() */
	free(msg, M_DEVBUF);
}

/**
 *  @brief Send a request to get all our pending offers.
 */
int
hv_vmbus_request_channel_offers(void)
{
	int				ret;
	hv_vmbus_channel_msg_header*	msg;
	hv_vmbus_channel_msg_info*	msg_info;

	msg_info = (hv_vmbus_channel_msg_info *)
	    malloc(sizeof(hv_vmbus_channel_msg_info)
		    + sizeof(hv_vmbus_channel_msg_header), M_DEVBUF, M_NOWAIT);

	if (msg_info == NULL) {
	    if(bootverbose)
		printf("Error VMBUS: malloc failed for Request Offers\n");
	    return (ENOMEM);
	}

	msg = (hv_vmbus_channel_msg_header*) msg_info->msg;
	msg->message_type = HV_CHANNEL_MESSAGE_REQUEST_OFFERS;

	ret = hv_vmbus_post_message(msg, sizeof(hv_vmbus_channel_msg_header));

	if (msg_info)
	    free(msg_info, M_DEVBUF);

	return (ret);
}

/**
 * @brief Release channels that are unattached/unconnected (i.e., no drivers associated)
 */
void
hv_vmbus_release_unattached_channels(void) 
{
	hv_vmbus_channel *channel;

	mtx_lock_spin(&hv_vmbus_g_connection.channel_lock);

	while (!TAILQ_EMPTY(&hv_vmbus_g_connection.channel_anchor)) {
	    channel = TAILQ_FIRST(&hv_vmbus_g_connection.channel_anchor);
	    TAILQ_REMOVE(&hv_vmbus_g_connection.channel_anchor,
			    channel, list_entry);

	    hv_vmbus_child_device_unregister(channel->device);
	    hv_vmbus_free_vmbus_channel(channel);
	}
	mtx_unlock_spin(&hv_vmbus_g_connection.channel_lock);
}
