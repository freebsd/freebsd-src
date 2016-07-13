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
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>

#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

typedef void	(*vmbus_chanmsg_proc_t)
		(struct vmbus_softc *, const struct vmbus_message *);

static struct hv_vmbus_channel *hv_vmbus_allocate_channel(struct vmbus_softc *);
static void	vmbus_channel_on_offer_internal(struct vmbus_softc *,
		    const hv_vmbus_channel_offer_channel *offer);
static void	vmbus_chan_detach_task(void *, int);

static void	vmbus_channel_on_offer(struct vmbus_softc *,
		    const struct vmbus_message *);
static void	vmbus_channel_on_offer_rescind(struct vmbus_softc *,
		    const struct vmbus_message *);
static void	vmbus_channel_on_offers_delivered(struct vmbus_softc *,
		    const struct vmbus_message *);

/**
 * Channel message dispatch table
 */
static const vmbus_chanmsg_proc_t
vmbus_chanmsg_process[HV_CHANNEL_MESSAGE_COUNT] = {
	[HV_CHANNEL_MESSAGE_OFFER_CHANNEL] =
		vmbus_channel_on_offer,
	[HV_CHANNEL_MESSAGE_RESCIND_CHANNEL_OFFER] =
		vmbus_channel_on_offer_rescind,
	[HV_CHANNEL_MESSAGE_ALL_OFFERS_DELIVERED] =
		vmbus_channel_on_offers_delivered,
	[HV_CHANNEL_MESSAGE_OPEN_CHANNEL_RESULT] =
		vmbus_msghc_wakeup,
	[HV_CHANNEL_MESSAGE_GPADL_CREATED] =
		vmbus_msghc_wakeup,
	[HV_CHANNEL_MESSAGE_GPADL_TORNDOWN] =
		vmbus_msghc_wakeup,
	[HV_CHANNEL_MESSAGE_VERSION_RESPONSE] =
		vmbus_msghc_wakeup
};

/**
 * @brief Allocate and initialize a vmbus channel object
 */
static struct hv_vmbus_channel *
hv_vmbus_allocate_channel(struct vmbus_softc *sc)
{
	struct hv_vmbus_channel *channel;

	channel = malloc(sizeof(*channel), M_DEVBUF, M_WAITOK | M_ZERO);
	channel->vmbus_sc = sc;

	mtx_init(&channel->sc_lock, "vmbus multi channel", NULL, MTX_DEF);
	TAILQ_INIT(&channel->sc_list_anchor);
	TASK_INIT(&channel->ch_detach_task, 0, vmbus_chan_detach_task, channel);

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
	struct vmbus_softc *sc = new_channel->vmbus_sc;
	hv_vmbus_channel*	channel;
	uint32_t                relid;

	relid = new_channel->ch_id;
	/*
	 * Make sure this is a new offer
	 */
	mtx_lock(&sc->vmbus_chlist_lock);
	if (relid == 0) {
		/*
		 * XXX channel0 will not be processed; skip it.
		 */
		printf("VMBUS: got channel0 offer\n");
	} else {
		sc->vmbus_chmap[relid] = new_channel;
	}

	TAILQ_FOREACH(channel, &sc->vmbus_chlist, ch_link) {
		if (memcmp(&channel->ch_guid_type, &new_channel->ch_guid_type,
		    sizeof(hv_guid)) == 0 &&
		    memcmp(&channel->ch_guid_inst, &new_channel->ch_guid_inst,
		    sizeof(hv_guid)) == 0)
			break;
	}

	if (channel == NULL) {
		/* Install the new primary channel */
		TAILQ_INSERT_TAIL(&sc->vmbus_chlist, new_channel, ch_link);
	}
	mtx_unlock(&sc->vmbus_chlist_lock);

	if (bootverbose) {
		char logstr[64];

		logstr[0] = '\0';
		if (channel != NULL) {
			snprintf(logstr, sizeof(logstr), ", primary chan%u",
			    channel->ch_id);
		}
		device_printf(sc->vmbus_dev, "chan%u subchanid%u offer%s\n",
		    new_channel->ch_id,
		    new_channel->ch_subidx, logstr);
	}

	if (channel != NULL) {
		/*
		 * Check if this is a sub channel.
		 */
		if (new_channel->ch_subidx != 0) {
			/*
			 * It is a sub channel offer, process it.
			 */
			new_channel->primary_channel = channel;
			new_channel->ch_dev = channel->ch_dev;
			mtx_lock(&channel->sc_lock);
			TAILQ_INSERT_TAIL(&channel->sc_list_anchor,
			    new_channel, sc_list_entry);
			mtx_unlock(&channel->sc_lock);

			/*
			 * Insert the new channel to the end of the global
			 * channel list.
			 *
			 * NOTE:
			 * The new sub-channel MUST be inserted AFTER it's
			 * primary channel, so that the primary channel will
			 * be found in the above loop for its baby siblings.
			 */
			mtx_lock(&sc->vmbus_chlist_lock);
			TAILQ_INSERT_TAIL(&sc->vmbus_chlist, new_channel,
			    ch_link);
			mtx_unlock(&sc->vmbus_chlist_lock);

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
		    new_channel->ch_id);
		hv_vmbus_free_vmbus_channel(new_channel);
		return;
	}

	new_channel->state = HV_CHANNEL_OPEN_STATE;

	/*
	 * Add the new device to the bus. This will kick off device-driver
	 * binding which eventually invokes the device driver's AddDevice()
	 * method.
	 *
	 * NOTE:
	 * Error is ignored here; don't have much to do if error really
	 * happens.
	 */
	hv_vmbus_child_device_register(new_channel);
}

void
vmbus_channel_cpu_set(struct hv_vmbus_channel *chan, int cpu)
{
	KASSERT(cpu >= 0 && cpu < mp_ncpus, ("invalid cpu %d", cpu));

	if (chan->vmbus_sc->vmbus_version == VMBUS_VERSION_WS2008 ||
	    chan->vmbus_sc->vmbus_version == VMBUS_VERSION_WIN7) {
		/* Only cpu0 is supported */
		cpu = 0;
	}

	chan->target_cpu = cpu;
	chan->target_vcpu = VMBUS_PCPU_GET(chan->vmbus_sc, vcpuid, cpu);

	if (bootverbose) {
		printf("vmbus_chan%u: assigned to cpu%u [vcpu%u]\n",
		    chan->ch_id,
		    chan->target_cpu, chan->target_vcpu);
	}
}

void
vmbus_channel_cpu_rr(struct hv_vmbus_channel *chan)
{
	static uint32_t vmbus_chan_nextcpu;
	int cpu;

	cpu = atomic_fetchadd_int(&vmbus_chan_nextcpu, 1) % mp_ncpus;
	vmbus_channel_cpu_set(chan, cpu);
}

static void
vmbus_channel_select_defcpu(struct hv_vmbus_channel *chan)
{
	/*
	 * By default, pin the channel to cpu0.  Devices having
	 * special channel-cpu mapping requirement should call
	 * vmbus_channel_cpu_{set,rr}().
	 */
	vmbus_channel_cpu_set(chan, 0);
}

/**
 * @brief Handler for channel offers from Hyper-V/Azure
 *
 * Handler for channel offers from vmbus in parent partition.
 */
static void
vmbus_channel_on_offer(struct vmbus_softc *sc, const struct vmbus_message *msg)
{
	const hv_vmbus_channel_offer_channel *offer;

	/* New channel is offered by vmbus */
	vmbus_scan_newchan(sc);

	offer = (const hv_vmbus_channel_offer_channel *)msg->msg_data;
	vmbus_channel_on_offer_internal(sc, offer);
}

static void
vmbus_channel_on_offer_internal(struct vmbus_softc *sc,
    const hv_vmbus_channel_offer_channel *offer)
{
	hv_vmbus_channel* new_channel;

	/*
	 * Allocate the channel object and save this offer
	 */
	new_channel = hv_vmbus_allocate_channel(sc);
	new_channel->ch_id = offer->child_rel_id;
	new_channel->ch_subidx = offer->offer.sub_channel_index;
	new_channel->ch_guid_type = offer->offer.interface_type;
	new_channel->ch_guid_inst = offer->offer.interface_instance;

	/* Batch reading is on by default */
	new_channel->ch_flags |= VMBUS_CHAN_FLAG_BATCHREAD;
	if (offer->monitor_allocated)
		new_channel->ch_flags |= VMBUS_CHAN_FLAG_HASMNF;

	new_channel->ch_monprm = hyperv_dmamem_alloc(
	    bus_get_dma_tag(sc->vmbus_dev),
	    HYPERCALL_PARAM_ALIGN, 0, sizeof(struct hyperv_mon_param),
	    &new_channel->ch_monprm_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (new_channel->ch_monprm == NULL) {
		device_printf(sc->vmbus_dev, "monprm alloc failed\n");
		/* XXX */
		mtx_destroy(&new_channel->sc_lock);
		free(new_channel, M_DEVBUF);
		return;
	}
	new_channel->ch_monprm->mp_connid = VMBUS_CONNID_EVENT;
	if (sc->vmbus_version != VMBUS_VERSION_WS2008)
		new_channel->ch_monprm->mp_connid = offer->connection_id;

	new_channel->monitor_group = (uint8_t) offer->monitor_id / 32;
	new_channel->monitor_bit = (uint8_t) offer->monitor_id % 32;

	/* Select default cpu for this channel. */
	vmbus_channel_select_defcpu(new_channel);

	vmbus_channel_process_offer(new_channel);
}

/**
 * @brief Rescind offer handler.
 *
 * We queue a work item to process this offer
 * synchronously.
 *
 * XXX pretty broken; need rework.
 */
static void
vmbus_channel_on_offer_rescind(struct vmbus_softc *sc,
    const struct vmbus_message *msg)
{
	const hv_vmbus_channel_rescind_offer *rescind;
	hv_vmbus_channel*		channel;

	rescind = (const hv_vmbus_channel_rescind_offer *)msg->msg_data;
	if (bootverbose) {
		device_printf(sc->vmbus_dev, "chan%u rescind\n",
		    rescind->child_rel_id);
	}

	channel = sc->vmbus_chmap[rescind->child_rel_id];
	if (channel == NULL)
	    return;
	sc->vmbus_chmap[rescind->child_rel_id] = NULL;

	taskqueue_enqueue(taskqueue_thread, &channel->ch_detach_task);
}

static void
vmbus_chan_detach_task(void *xchan, int pending __unused)
{
	struct hv_vmbus_channel *chan = xchan;

	if (HV_VMBUS_CHAN_ISPRIMARY(chan)) {
		/* Only primary channel owns the device */
		hv_vmbus_child_device_unregister(chan);
		/* NOTE: DO NOT free primary channel for now */
	} else {
		struct vmbus_softc *sc = chan->vmbus_sc;
		struct hv_vmbus_channel *pri_chan = chan->primary_channel;
		struct vmbus_chanmsg_chfree *req;
		struct vmbus_msghc *mh;
		int error;

		mh = vmbus_msghc_get(sc, sizeof(*req));
		if (mh == NULL) {
			device_printf(sc->vmbus_dev,
			    "can not get msg hypercall for chfree(chan%u)\n",
			    chan->ch_id);
			goto remove;
		}

		req = vmbus_msghc_dataptr(mh);
		req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_CHFREE;
		req->chm_chanid = chan->ch_id;

		error = vmbus_msghc_exec_noresult(mh);
		vmbus_msghc_put(sc, mh);

		if (error) {
			device_printf(sc->vmbus_dev,
			    "chfree(chan%u) failed: %d",
			    chan->ch_id, error);
			/* NOTE: Move on! */
		} else {
			if (bootverbose) {
				device_printf(sc->vmbus_dev, "chan%u freed\n",
				    chan->ch_id);
			}
		}
remove:
		mtx_lock(&sc->vmbus_chlist_lock);
		TAILQ_REMOVE(&sc->vmbus_chlist, chan, ch_link);
		mtx_unlock(&sc->vmbus_chlist_lock);

		mtx_lock(&pri_chan->sc_lock);
		TAILQ_REMOVE(&pri_chan->sc_list_anchor, chan, sc_list_entry);
		KASSERT(pri_chan->subchan_cnt > 0,
		    ("invalid subchan_cnt %d", pri_chan->subchan_cnt));
		pri_chan->subchan_cnt--;
		mtx_unlock(&pri_chan->sc_lock);
		wakeup(pri_chan);

		hv_vmbus_free_vmbus_channel(chan);
	}
}

/**
 *
 * @brief Invoked when all offers have been delivered.
 */
static void
vmbus_channel_on_offers_delivered(struct vmbus_softc *sc,
    const struct vmbus_message *msg __unused)
{

	/* No more new channels for the channel request. */
	vmbus_scan_done(sc);
}

/**
 * @brief Release channels that are unattached/unconnected (i.e., no drivers associated)
 */
void
hv_vmbus_release_unattached_channels(struct vmbus_softc *sc)
{
	hv_vmbus_channel *channel;

	mtx_lock(&sc->vmbus_chlist_lock);

	while (!TAILQ_EMPTY(&sc->vmbus_chlist)) {
	    channel = TAILQ_FIRST(&sc->vmbus_chlist);
	    TAILQ_REMOVE(&sc->vmbus_chlist, channel, ch_link);

	    if (HV_VMBUS_CHAN_ISPRIMARY(channel)) {
		/* Only primary channel owns the device */
		hv_vmbus_child_device_unregister(channel);
	    }
	    hv_vmbus_free_vmbus_channel(channel);
	}
	bzero(sc->vmbus_chmap,
	    sizeof(struct hv_vmbus_channel *) * VMBUS_CHAN_MAX);

	mtx_unlock(&sc->vmbus_chlist_lock);
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

	cur_vcpu = VMBUS_PCPU_GET(primary->vmbus_sc, vcpuid, smp_pro_id);
	
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
vmbus_drain_subchan(struct hv_vmbus_channel *pri_chan)
{
	mtx_lock(&pri_chan->sc_lock);
	while (pri_chan->subchan_cnt > 0)
		mtx_sleep(pri_chan, &pri_chan->sc_lock, 0, "dsubch", 0);
	mtx_unlock(&pri_chan->sc_lock);
}

void
vmbus_chan_msgproc(struct vmbus_softc *sc, const struct vmbus_message *msg)
{
	vmbus_chanmsg_proc_t msg_proc;
	uint32_t msg_type;

	msg_type = ((const struct vmbus_chanmsg_hdr *)msg->msg_data)->chm_type;
	if (msg_type >= HV_CHANNEL_MESSAGE_COUNT) {
		device_printf(sc->vmbus_dev, "unknown message type 0x%x\n",
		    msg_type);
		return;
	}

	msg_proc = vmbus_chanmsg_process[msg_type];
	if (msg_proc != NULL)
		msg_proc(sc, msg);
}
