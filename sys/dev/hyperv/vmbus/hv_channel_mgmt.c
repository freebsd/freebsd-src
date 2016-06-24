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
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>

#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

/*
 * Internal functions
 */

typedef void (*vmbus_msg_handler)(const hv_vmbus_channel_msg_header *msg);

typedef struct hv_vmbus_channel_msg_table_entry {
	hv_vmbus_channel_msg_type    messageType;
	vmbus_msg_handler   messageHandler;
} hv_vmbus_channel_msg_table_entry;

static void	vmbus_channel_on_offer_internal(void *context);
static void	vmbus_channel_on_offer_rescind_internal(void *context);

static void	vmbus_channel_on_offer(const hv_vmbus_channel_msg_header *hdr);
static void	vmbus_channel_on_open_result(
		    const hv_vmbus_channel_msg_header *hdr);
static void	vmbus_channel_on_offer_rescind(
		    const hv_vmbus_channel_msg_header *hdr);
static void	vmbus_channel_on_gpadl_created(
		    const hv_vmbus_channel_msg_header *hdr);
static void	vmbus_channel_on_gpadl_torndown(
		    const hv_vmbus_channel_msg_header *hdr);
static void	vmbus_channel_on_offers_delivered(
		    const hv_vmbus_channel_msg_header *hdr);
static void	vmbus_channel_on_version_response(
		    const hv_vmbus_channel_msg_header *hdr);

/**
 * Channel message dispatch table
 */
static const hv_vmbus_channel_msg_table_entry
    g_channel_message_table[HV_CHANNEL_MESSAGE_COUNT] = {
	{ HV_CHANNEL_MESSAGE_INVALID,
		NULL },
	{ HV_CHANNEL_MESSAGE_OFFER_CHANNEL,
		vmbus_channel_on_offer },
	{ HV_CHANNEL_MESSAGE_RESCIND_CHANNEL_OFFER,
		vmbus_channel_on_offer_rescind },
	{ HV_CHANNEL_MESSAGE_REQUEST_OFFERS,
		NULL },
	{ HV_CHANNEL_MESSAGE_ALL_OFFERS_DELIVERED,
		vmbus_channel_on_offers_delivered },
	{ HV_CHANNEL_MESSAGE_OPEN_CHANNEL,
		NULL },
	{ HV_CHANNEL_MESSAGE_OPEN_CHANNEL_RESULT,
		vmbus_channel_on_open_result },
	{ HV_CHANNEL_MESSAGE_CLOSE_CHANNEL,
		NULL },
	{ HV_CHANNEL_MESSAGEL_GPADL_HEADER,
		NULL },
	{ HV_CHANNEL_MESSAGE_GPADL_BODY,
		NULL },
	{ HV_CHANNEL_MESSAGE_GPADL_CREATED,
		vmbus_channel_on_gpadl_created },
	{ HV_CHANNEL_MESSAGE_GPADL_TEARDOWN,
		NULL },
	{ HV_CHANNEL_MESSAGE_GPADL_TORNDOWN,
		vmbus_channel_on_gpadl_torndown },
	{ HV_CHANNEL_MESSAGE_REL_ID_RELEASED,
		NULL },
	{ HV_CHANNEL_MESSAGE_INITIATED_CONTACT,
		NULL },
	{ HV_CHANNEL_MESSAGE_VERSION_RESPONSE,
		vmbus_channel_on_version_response },
	{ HV_CHANNEL_MESSAGE_UNLOAD,
		NULL }
};

typedef struct hv_work_item {
	struct task	work;
	void		(*callback)(void *);
	void*		context;
} hv_work_item;

static struct mtx	vmbus_chwait_lock;
MTX_SYSINIT(vmbus_chwait_lk, &vmbus_chwait_lock, "vmbus primarych wait lock",
    MTX_DEF);
static uint32_t		vmbus_chancnt;
static uint32_t		vmbus_devcnt;

#define VMBUS_CHANCNT_DONE	0x80000000

/**
 * Implementation of the work abstraction.
 */
static void
work_item_callback(void *work, int pending)
{
	struct hv_work_item *w = (struct hv_work_item *)work;

	w->callback(w->context);

	free(w, M_DEVBUF);
}

/**
 * @brief Create work item
 */
static int
hv_queue_work_item(
	void (*callback)(void *), void *context)
{
	struct hv_work_item *w = malloc(sizeof(struct hv_work_item),
					M_DEVBUF, M_NOWAIT);
	KASSERT(w != NULL, ("Error VMBUS: Failed to allocate WorkItem\n"));
	if (w == NULL)
	    return (ENOMEM);

	w->callback = callback;
	w->context = context;

	TASK_INIT(&w->work, 0, work_item_callback, w);

	return (taskqueue_enqueue(taskqueue_thread, &w->work));
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
					M_WAITOK | M_ZERO);

	mtx_init(&channel->sc_lock, "vmbus multi channel", NULL, MTX_DEF);
	TAILQ_INIT(&channel->sc_list_anchor);

	return (channel);
}

/**
 * @brief Release the resources used by the vmbus channel object
 */
void
hv_vmbus_free_vmbus_channel(hv_vmbus_channel* channel)
{
	mtx_destroy(&channel->sc_lock);
	free(channel, M_DEVBUF);
}

/**
 * @brief Process the offer by creating a channel/device
 * associated with this offer
 */
static void
vmbus_channel_process_offer(hv_vmbus_channel *new_channel)
{
	hv_vmbus_channel*	channel;
	int			ret;
	uint32_t                relid;

	relid = new_channel->offer_msg.child_rel_id;
	/*
	 * Make sure this is a new offer
	 */
	mtx_lock(&hv_vmbus_g_connection.channel_lock);
	if (relid == 0) {
		/*
		 * XXX channel0 will not be processed; skip it.
		 */
		printf("VMBUS: got channel0 offer\n");
	} else {
		hv_vmbus_g_connection.channels[relid] = new_channel;
	}

	TAILQ_FOREACH(channel, &hv_vmbus_g_connection.channel_anchor,
	    list_entry) {
		if (memcmp(&channel->offer_msg.offer.interface_type,
		    &new_channel->offer_msg.offer.interface_type,
		    sizeof(hv_guid)) == 0 &&
		    memcmp(&channel->offer_msg.offer.interface_instance,
		    &new_channel->offer_msg.offer.interface_instance,
		    sizeof(hv_guid)) == 0)
			break;
	}

	if (channel == NULL) {
		/* Install the new primary channel */
		TAILQ_INSERT_TAIL(&hv_vmbus_g_connection.channel_anchor,
		    new_channel, list_entry);
	}
	mtx_unlock(&hv_vmbus_g_connection.channel_lock);

	if (channel != NULL) {
		/*
		 * Check if this is a sub channel.
		 */
		if (new_channel->offer_msg.offer.sub_channel_index != 0) {
			/*
			 * It is a sub channel offer, process it.
			 */
			new_channel->primary_channel = channel;
			new_channel->device = channel->device;
			mtx_lock(&channel->sc_lock);
			TAILQ_INSERT_TAIL(&channel->sc_list_anchor,
			    new_channel, sc_list_entry);
			mtx_unlock(&channel->sc_lock);

			if (bootverbose) {
				printf("VMBUS get multi-channel offer, "
				    "rel=%u, sub=%u\n",
				    new_channel->offer_msg.child_rel_id,
				    new_channel->offer_msg.offer.sub_channel_index);	
			}

			/* Insert new channel into channel_anchor. */
			mtx_lock(&hv_vmbus_g_connection.channel_lock);
			TAILQ_INSERT_TAIL(&hv_vmbus_g_connection.channel_anchor,
			    new_channel, list_entry);				
			mtx_unlock(&hv_vmbus_g_connection.channel_lock);

			if(bootverbose)
				printf("VMBUS: new multi-channel offer <%p>, "
				    "its primary channel is <%p>.\n",
				    new_channel, new_channel->primary_channel);

			new_channel->state = HV_CHANNEL_OPEN_STATE;

			/*
			 * Bump up sub-channel count and notify anyone that is
			 * interested in this sub-channel, after this sub-channel
			 * is setup.
			 */
			mtx_lock(&channel->sc_lock);
			channel->subchan_cnt++;
			mtx_unlock(&channel->sc_lock);
			wakeup(channel);

			return;
		}

		printf("VMBUS: duplicated primary channel%u\n",
		    new_channel->offer_msg.child_rel_id);
		hv_vmbus_free_vmbus_channel(new_channel);
		return;
	}

	new_channel->state = HV_CHANNEL_OPEN_STATE;

	/*
	 * Start the process of binding this offer to the driver
	 * (We need to set the device field before calling
	 * hv_vmbus_child_device_add())
	 */
	new_channel->device = hv_vmbus_child_device_create(
	    new_channel->offer_msg.offer.interface_type,
	    new_channel->offer_msg.offer.interface_instance, new_channel);

	/*
	 * Add the new device to the bus. This will kick off device-driver
	 * binding which eventually invokes the device driver's AddDevice()
	 * method.
	 */
	ret = hv_vmbus_child_device_register(new_channel->device);
	if (ret != 0) {
		mtx_lock(&hv_vmbus_g_connection.channel_lock);
		TAILQ_REMOVE(&hv_vmbus_g_connection.channel_anchor,
		    new_channel, list_entry);
		mtx_unlock(&hv_vmbus_g_connection.channel_lock);
		hv_vmbus_free_vmbus_channel(new_channel);
	}

	mtx_lock(&vmbus_chwait_lock);
	vmbus_devcnt++;
	mtx_unlock(&vmbus_chwait_lock);
	wakeup(&vmbus_devcnt);
}

void
vmbus_channel_cpu_set(struct hv_vmbus_channel *chan, int cpu)
{
	KASSERT(cpu >= 0 && cpu < mp_ncpus, ("invalid cpu %d", cpu));

	if (hv_vmbus_protocal_version == HV_VMBUS_VERSION_WS2008 ||
	    hv_vmbus_protocal_version == HV_VMBUS_VERSION_WIN7) {
		/* Only cpu0 is supported */
		cpu = 0;
	}

	chan->target_cpu = cpu;
	chan->target_vcpu = VMBUS_PCPU_GET(vmbus_get_softc(), vcpuid, cpu);

	if (bootverbose) {
		printf("vmbus_chan%u: assigned to cpu%u [vcpu%u]\n",
		    chan->offer_msg.child_rel_id,
		    chan->target_cpu, chan->target_vcpu);
	}
}

/**
 * Array of device guids that are performance critical. We try to distribute
 * the interrupt load for these devices across all online cpus. 
 */
static const hv_guid high_perf_devices[] = {
	{HV_NIC_GUID, },
	{HV_IDE_GUID, },
	{HV_SCSI_GUID, },
};

enum {
	PERF_CHN_NIC = 0,
	PERF_CHN_IDE,
	PERF_CHN_SCSI,
	MAX_PERF_CHN,
};

/*
 * We use this static number to distribute the channel interrupt load.
 */
static uint32_t next_vcpu;

/**
 * Starting with Win8, we can statically distribute the incoming
 * channel interrupt load by binding a channel to VCPU. We
 * implement here a simple round robin scheme for distributing
 * the interrupt load.
 * We will bind channels that are not performance critical to cpu 0 and
 * performance critical channels (IDE, SCSI and Network) will be uniformly
 * distributed across all available CPUs.
 */
static void
vmbus_channel_select_defcpu(struct hv_vmbus_channel *channel)
{
	uint32_t current_cpu;
	int i;
	boolean_t is_perf_channel = FALSE;
	const hv_guid *guid = &channel->offer_msg.offer.interface_type;

	for (i = PERF_CHN_NIC; i < MAX_PERF_CHN; i++) {
		if (memcmp(guid->data, high_perf_devices[i].data,
		    sizeof(hv_guid)) == 0) {
			is_perf_channel = TRUE;
			break;
		}
	}

	if (!is_perf_channel) {
		/* Stick to cpu0 */
		vmbus_channel_cpu_set(channel, 0);
		return;
	}
	/* mp_ncpus should have the number cpus currently online */
	current_cpu = (++next_vcpu % mp_ncpus);
	vmbus_channel_cpu_set(channel, current_cpu);
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
vmbus_channel_on_offer(const hv_vmbus_channel_msg_header *hdr)
{
	const hv_vmbus_channel_offer_channel *offer;
	hv_vmbus_channel_offer_channel *copied;

	offer = (const hv_vmbus_channel_offer_channel *)hdr;

	// copy offer data
	copied = malloc(sizeof(*copied), M_DEVBUF, M_NOWAIT);
	if (copied == NULL) {
		printf("fail to allocate memory\n");
		return;
	}

	memcpy(copied, hdr, sizeof(*copied));
	hv_queue_work_item(vmbus_channel_on_offer_internal, copied);

	mtx_lock(&vmbus_chwait_lock);
	if ((vmbus_chancnt & VMBUS_CHANCNT_DONE) == 0)
		vmbus_chancnt++;
	mtx_unlock(&vmbus_chwait_lock);
}

static void
vmbus_channel_on_offer_internal(void* context)
{
	hv_vmbus_channel* new_channel;

	hv_vmbus_channel_offer_channel* offer = (hv_vmbus_channel_offer_channel*)context;
	/* Allocate the channel object and save this offer */
	new_channel = hv_vmbus_allocate_channel();

	/*
	 * By default we setup state to enable batched
	 * reading. A specific service can choose to
	 * disable this prior to opening the channel.
	 */
	new_channel->batched_reading = TRUE;

	new_channel->signal_event_param =
	    (hv_vmbus_input_signal_event *)
	    (HV_ALIGN_UP((unsigned long)
		&new_channel->signal_event_buffer,
		HV_HYPERCALL_PARAM_ALIGN));

 	new_channel->signal_event_param->connection_id.as_uint32_t = 0;	
	new_channel->signal_event_param->connection_id.u.id =
	    HV_VMBUS_EVENT_CONNECTION_ID;
	new_channel->signal_event_param->flag_number = 0;
	new_channel->signal_event_param->rsvd_z = 0;

	if (hv_vmbus_protocal_version != HV_VMBUS_VERSION_WS2008) {
		new_channel->is_dedicated_interrupt =
		    (offer->is_dedicated_interrupt != 0);
		new_channel->signal_event_param->connection_id.u.id =
		    offer->connection_id;
	}

	memcpy(&new_channel->offer_msg, offer,
	    sizeof(hv_vmbus_channel_offer_channel));
	new_channel->monitor_group = (uint8_t) offer->monitor_id / 32;
	new_channel->monitor_bit = (uint8_t) offer->monitor_id % 32;

	/* Select default cpu for this channel. */
	vmbus_channel_select_defcpu(new_channel);

	vmbus_channel_process_offer(new_channel);

	free(offer, M_DEVBUF);
}

/**
 * @brief Rescind offer handler.
 *
 * We queue a work item to process this offer
 * synchronously
 */
static void
vmbus_channel_on_offer_rescind(const hv_vmbus_channel_msg_header *hdr)
{
	const hv_vmbus_channel_rescind_offer *rescind;
	hv_vmbus_channel*		channel;

	rescind = (const hv_vmbus_channel_rescind_offer *)hdr;

	channel = hv_vmbus_g_connection.channels[rescind->child_rel_id];
	if (channel == NULL)
	    return;

	hv_queue_work_item(vmbus_channel_on_offer_rescind_internal, channel);
	hv_vmbus_g_connection.channels[rescind->child_rel_id] = NULL;
}

static void
vmbus_channel_on_offer_rescind_internal(void *context)
{
	hv_vmbus_channel*               channel;

	channel = (hv_vmbus_channel*)context;
	if (HV_VMBUS_CHAN_ISPRIMARY(channel)) {
		/* Only primary channel owns the hv_device */
		hv_vmbus_child_device_unregister(channel->device);
	}
}

/**
 *
 * @brief Invoked when all offers have been delivered.
 */
static void
vmbus_channel_on_offers_delivered(
    const hv_vmbus_channel_msg_header *hdr __unused)
{

	mtx_lock(&vmbus_chwait_lock);
	vmbus_chancnt |= VMBUS_CHANCNT_DONE;
	mtx_unlock(&vmbus_chwait_lock);
	wakeup(&vmbus_chancnt);
}

/**
 * @brief Open result handler.
 *
 * This is invoked when we received a response
 * to our channel open request. Find the matching request, copy the
 * response and signal the requesting thread.
 */
static void
vmbus_channel_on_open_result(const hv_vmbus_channel_msg_header *hdr)
{
	const hv_vmbus_channel_open_result *result;
	hv_vmbus_channel_msg_info*	msg_info;
	hv_vmbus_channel_msg_header*	requestHeader;
	hv_vmbus_channel_open_channel*	openMsg;

	result = (const hv_vmbus_channel_open_result *)hdr;

	/*
	 * Find the open msg, copy the result and signal/unblock the wait event
	 */
	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);

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
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

}

/**
 * @brief GPADL created handler.
 *
 * This is invoked when we received a response
 * to our gpadl create request. Find the matching request, copy the
 * response and signal the requesting thread.
 */
static void
vmbus_channel_on_gpadl_created(const hv_vmbus_channel_msg_header *hdr)
{
	const hv_vmbus_channel_gpadl_created *gpadl_created;
	hv_vmbus_channel_msg_info*		msg_info;
	hv_vmbus_channel_msg_header*		request_header;
	hv_vmbus_channel_gpadl_header*		gpadl_header;

	gpadl_created = (const hv_vmbus_channel_gpadl_created *)hdr;

	/* Find the establish msg, copy the result and signal/unblock
	 * the wait event
	 */
	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
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
	mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);
}

/**
 * @brief GPADL torndown handler.
 *
 * This is invoked when we received a respons
 * to our gpadl teardown request. Find the matching request, copy the
 * response and signal the requesting thread
 */
static void
vmbus_channel_on_gpadl_torndown(const hv_vmbus_channel_msg_header *hdr)
{
	const hv_vmbus_channel_gpadl_torndown *gpadl_torndown;
	hv_vmbus_channel_msg_info*		msg_info;
	hv_vmbus_channel_msg_header*		requestHeader;
	hv_vmbus_channel_gpadl_teardown*	gpadlTeardown;

	gpadl_torndown = (const hv_vmbus_channel_gpadl_torndown *)hdr;

	/*
	 * Find the open msg, copy the result and signal/unblock the
	 * wait event.
	 */

	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);

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
    mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);
}

/**
 * @brief Version response handler.
 *
 * This is invoked when we received a response
 * to our initiate contact request. Find the matching request, copy th
 * response and signal the requesting thread.
 */
static void
vmbus_channel_on_version_response(const hv_vmbus_channel_msg_header *hdr)
{
	hv_vmbus_channel_msg_info*		msg_info;
	hv_vmbus_channel_msg_header*		requestHeader;
	hv_vmbus_channel_initiate_contact*	initiate;
	const hv_vmbus_channel_version_response *versionResponse;

	versionResponse = (const hv_vmbus_channel_version_response *)hdr;

	mtx_lock(&hv_vmbus_g_connection.channel_msg_lock);
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
    mtx_unlock(&hv_vmbus_g_connection.channel_msg_lock);

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

	mtx_lock(&hv_vmbus_g_connection.channel_lock);

	while (!TAILQ_EMPTY(&hv_vmbus_g_connection.channel_anchor)) {
	    channel = TAILQ_FIRST(&hv_vmbus_g_connection.channel_anchor);
	    TAILQ_REMOVE(&hv_vmbus_g_connection.channel_anchor,
			    channel, list_entry);

	    if (HV_VMBUS_CHAN_ISPRIMARY(channel)) {
		/* Only primary channel owns the hv_device */
		hv_vmbus_child_device_unregister(channel->device);
	    }
	    hv_vmbus_free_vmbus_channel(channel);
	}
	bzero(hv_vmbus_g_connection.channels,
	    sizeof(hv_vmbus_channel*) * VMBUS_CHAN_MAX);
	mtx_unlock(&hv_vmbus_g_connection.channel_lock);
}

/**
 * @brief Select the best outgoing channel
 * 
 * The channel whose vcpu binding is closest to the currect vcpu will
 * be selected.
 * If no multi-channel, always select primary channel
 * 
 * @param primary - primary channel
 */
struct hv_vmbus_channel *
vmbus_select_outgoing_channel(struct hv_vmbus_channel *primary)
{
	hv_vmbus_channel *new_channel = NULL;
	hv_vmbus_channel *outgoing_channel = primary;
	int old_cpu_distance = 0;
	int new_cpu_distance = 0;
	int cur_vcpu = 0;
	int smp_pro_id = PCPU_GET(cpuid);

	if (TAILQ_EMPTY(&primary->sc_list_anchor)) {
		return outgoing_channel;
	}

	if (smp_pro_id >= MAXCPU) {
		return outgoing_channel;
	}

	cur_vcpu = VMBUS_PCPU_GET(vmbus_get_softc(), vcpuid, smp_pro_id);
	
	TAILQ_FOREACH(new_channel, &primary->sc_list_anchor, sc_list_entry) {
		if (new_channel->state != HV_CHANNEL_OPENED_STATE){
			continue;
		}

		if (new_channel->target_vcpu == cur_vcpu){
			return new_channel;
		}

		old_cpu_distance = ((outgoing_channel->target_vcpu > cur_vcpu) ?
		    (outgoing_channel->target_vcpu - cur_vcpu) :
		    (cur_vcpu - outgoing_channel->target_vcpu));

		new_cpu_distance = ((new_channel->target_vcpu > cur_vcpu) ?
		    (new_channel->target_vcpu - cur_vcpu) :
		    (cur_vcpu - new_channel->target_vcpu));

		if (old_cpu_distance < new_cpu_distance) {
			continue;
		}

		outgoing_channel = new_channel;
	}

	return(outgoing_channel);
}

void
vmbus_scan(void)
{
	uint32_t chancnt;

	mtx_lock(&vmbus_chwait_lock);
	while ((vmbus_chancnt & VMBUS_CHANCNT_DONE) == 0)
		mtx_sleep(&vmbus_chancnt, &vmbus_chwait_lock, 0, "waitch", 0);
	chancnt = vmbus_chancnt & ~VMBUS_CHANCNT_DONE;

	while (vmbus_devcnt != chancnt)
		mtx_sleep(&vmbus_devcnt, &vmbus_chwait_lock, 0, "waitdev", 0);
	mtx_unlock(&vmbus_chwait_lock);
}

struct hv_vmbus_channel **
vmbus_get_subchan(struct hv_vmbus_channel *pri_chan, int subchan_cnt)
{
	struct hv_vmbus_channel **ret, *chan;
	int i;

	ret = malloc(subchan_cnt * sizeof(struct hv_vmbus_channel *), M_TEMP,
	    M_WAITOK);

	mtx_lock(&pri_chan->sc_lock);

	while (pri_chan->subchan_cnt < subchan_cnt)
		mtx_sleep(pri_chan, &pri_chan->sc_lock, 0, "subch", 0);

	i = 0;
	TAILQ_FOREACH(chan, &pri_chan->sc_list_anchor, sc_list_entry) {
		/* TODO: refcnt chan */
		ret[i] = chan;

		++i;
		if (i == subchan_cnt)
			break;
	}
	KASSERT(i == subchan_cnt, ("invalid subchan count %d, should be %d",
	    pri_chan->subchan_cnt, subchan_cnt));

	mtx_unlock(&pri_chan->sc_lock);

	return ret;
}

void
vmbus_rel_subchan(struct hv_vmbus_channel **subchan, int subchan_cnt __unused)
{

	free(subchan, M_TEMP);
}

void
vmbus_chan_msgproc(struct vmbus_softc *sc, const struct vmbus_message *msg)
{
	const hv_vmbus_channel_msg_table_entry *entry;
	const hv_vmbus_channel_msg_header *hdr;
	hv_vmbus_channel_msg_type msg_type;

	hdr = (const hv_vmbus_channel_msg_header *)msg->msg_data;
	msg_type = hdr->message_type;

	if (msg_type >= HV_CHANNEL_MESSAGE_COUNT) {
		device_printf(sc->vmbus_dev, "unknown message type 0x%x\n",
		    msg_type);
		return;
	}

	entry = &g_channel_message_table[msg_type];
	if (entry->messageHandler)
		entry->messageHandler(hdr);
}
