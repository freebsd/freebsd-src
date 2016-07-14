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

static void	vmbus_chan_detach_task(void *, int);

static void	vmbus_channel_on_offers_delivered(struct vmbus_softc *,
		    const struct vmbus_message *);
static void	vmbus_chan_msgproc_choffer(struct vmbus_softc *,
		    const struct vmbus_message *);
static void	vmbus_chan_msgproc_chrescind(struct vmbus_softc *,
		    const struct vmbus_message *);

/*
 * Vmbus channel message processing.
 */

#define VMBUS_CHANMSG_PROC(name, func)	\
	[VMBUS_CHANMSG_TYPE_##name] = func
#define VMBUS_CHANMSG_PROC_WAKEUP(name)	\
	VMBUS_CHANMSG_PROC(name, vmbus_msghc_wakeup)

static const vmbus_chanmsg_proc_t
vmbus_chanmsg_process[VMBUS_CHANMSG_TYPE_MAX] = {
	VMBUS_CHANMSG_PROC(CHOFFER,	vmbus_chan_msgproc_choffer),
	VMBUS_CHANMSG_PROC(CHRESCIND,	vmbus_chan_msgproc_chrescind),
	VMBUS_CHANMSG_PROC(CHOFFER_DONE,vmbus_channel_on_offers_delivered),

	VMBUS_CHANMSG_PROC_WAKEUP(CHOPEN_RESP),
	VMBUS_CHANMSG_PROC_WAKEUP(GPADL_CONNRESP),
	VMBUS_CHANMSG_PROC_WAKEUP(GPADL_DISCONNRESP),
	VMBUS_CHANMSG_PROC_WAKEUP(CONNECT_RESP)
};

#undef VMBUS_CHANMSG_PROC_WAKEUP
#undef VMBUS_CHANMSG_PROC

static struct hv_vmbus_channel *
vmbus_chan_alloc(struct vmbus_softc *sc)
{
	struct hv_vmbus_channel *chan;

	chan = malloc(sizeof(*chan), M_DEVBUF, M_WAITOK | M_ZERO);

	chan->ch_monprm = hyperv_dmamem_alloc(bus_get_dma_tag(sc->vmbus_dev),
	    HYPERCALL_PARAM_ALIGN, 0, sizeof(struct hyperv_mon_param),
	    &chan->ch_monprm_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (chan->ch_monprm == NULL) {
		device_printf(sc->vmbus_dev, "monprm alloc failed\n");
		free(chan, M_DEVBUF);
		return NULL;
	}

	chan->vmbus_sc = sc;
	mtx_init(&chan->sc_lock, "vmbus multi channel", NULL, MTX_DEF);
	TAILQ_INIT(&chan->sc_list_anchor);
	TASK_INIT(&chan->ch_detach_task, 0, vmbus_chan_detach_task, chan);

	return chan;
}

static void
vmbus_chan_free(struct hv_vmbus_channel *chan)
{
	/* TODO: assert sub-channel list is empty */
	/* TODO: asset no longer on the primary channel's sub-channel list */
	/* TODO: asset no longer on the vmbus channel list */
	hyperv_dmamem_free(&chan->ch_monprm_dma, chan->ch_monprm);
	mtx_destroy(&chan->sc_lock);
	free(chan, M_DEVBUF);
}

static int
vmbus_chan_add(struct hv_vmbus_channel *newchan)
{
	struct vmbus_softc *sc = newchan->vmbus_sc;
	struct hv_vmbus_channel *prichan;

	if (newchan->ch_id == 0) {
		/*
		 * XXX
		 * Chan0 will neither be processed nor should be offered;
		 * skip it.
		 */
		device_printf(sc->vmbus_dev, "got chan0 offer, discard\n");
		return EINVAL;
	} else if (newchan->ch_id >= VMBUS_CHAN_MAX) {
		device_printf(sc->vmbus_dev, "invalid chan%u offer\n",
		    newchan->ch_id);
		return EINVAL;
	}
	sc->vmbus_chmap[newchan->ch_id] = newchan;

	if (bootverbose) {
		device_printf(sc->vmbus_dev, "chan%u subidx%u offer\n",
		    newchan->ch_id, newchan->ch_subidx);
	}

	mtx_lock(&sc->vmbus_prichan_lock);
	TAILQ_FOREACH(prichan, &sc->vmbus_prichans, ch_prilink) {
		if (memcmp(&prichan->ch_guid_type, &newchan->ch_guid_type,
		    sizeof(struct hyperv_guid)) == 0 &&
		    memcmp(&prichan->ch_guid_inst, &newchan->ch_guid_inst,
		    sizeof(struct hyperv_guid)) == 0)
			break;
	}
	if (VMBUS_CHAN_ISPRIMARY(newchan)) {
		if (prichan == NULL) {
			/* Install the new primary channel */
			TAILQ_INSERT_TAIL(&sc->vmbus_prichans, newchan,
			    ch_prilink);
			mtx_unlock(&sc->vmbus_prichan_lock);
			return 0;
		} else {
			mtx_unlock(&sc->vmbus_prichan_lock);
			device_printf(sc->vmbus_dev, "duplicated primary "
			    "chan%u\n", newchan->ch_id);
			return EINVAL;
		}
	} else { /* Sub-channel */
		if (prichan == NULL) {
			mtx_unlock(&sc->vmbus_prichan_lock);
			device_printf(sc->vmbus_dev, "no primary chan for "
			    "chan%u\n", newchan->ch_id);
			return EINVAL;
		}
		/*
		 * Found the primary channel for this sub-channel and
		 * move on.
		 *
		 * XXX refcnt prichan
		 */
	}
	mtx_unlock(&sc->vmbus_prichan_lock);

	/*
	 * This is a sub-channel; link it with the primary channel.
	 */
	KASSERT(!VMBUS_CHAN_ISPRIMARY(newchan),
	    ("new channel is not sub-channel"));
	KASSERT(prichan != NULL, ("no primary channel"));

	newchan->primary_channel = prichan;
	newchan->ch_dev = prichan->ch_dev;

	mtx_lock(&prichan->sc_lock);
	TAILQ_INSERT_TAIL(&prichan->sc_list_anchor, newchan, sc_list_entry);
	/*
	 * Bump up sub-channel count and notify anyone that is
	 * interested in this sub-channel, after this sub-channel
	 * is setup.
	 */
	prichan->subchan_cnt++;
	mtx_unlock(&prichan->sc_lock);
	wakeup(prichan);

	return 0;
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
vmbus_chan_cpu_default(struct hv_vmbus_channel *chan)
{
	/*
	 * By default, pin the channel to cpu0.  Devices having
	 * special channel-cpu mapping requirement should call
	 * vmbus_channel_cpu_{set,rr}().
	 */
	vmbus_channel_cpu_set(chan, 0);
}

static void
vmbus_chan_msgproc_choffer(struct vmbus_softc *sc,
    const struct vmbus_message *msg)
{
	const struct vmbus_chanmsg_choffer *offer;
	struct hv_vmbus_channel *chan;
	int error;

	offer = (const struct vmbus_chanmsg_choffer *)msg->msg_data;

	chan = vmbus_chan_alloc(sc);
	if (chan == NULL) {
		device_printf(sc->vmbus_dev, "allocate chan%u failed\n",
		    offer->chm_chanid);
		return;
	}

	chan->ch_id = offer->chm_chanid;
	chan->ch_subidx = offer->chm_subidx;
	chan->ch_guid_type = offer->chm_chtype;
	chan->ch_guid_inst = offer->chm_chinst;

	/* Batch reading is on by default */
	chan->ch_flags |= VMBUS_CHAN_FLAG_BATCHREAD;

	chan->ch_monprm->mp_connid = VMBUS_CONNID_EVENT;
	if (sc->vmbus_version != VMBUS_VERSION_WS2008)
		chan->ch_monprm->mp_connid = offer->chm_connid;

	if (offer->chm_flags1 & VMBUS_CHOFFER_FLAG1_HASMNF) {
		/*
		 * Setup MNF stuffs.
		 */
		chan->ch_flags |= VMBUS_CHAN_FLAG_HASMNF;
		chan->ch_montrig_idx = offer->chm_montrig / VMBUS_MONTRIG_LEN;
		if (chan->ch_montrig_idx >= VMBUS_MONTRIGS_MAX)
			panic("invalid monitor trigger %u", offer->chm_montrig);
		chan->ch_montrig_mask =
		    1 << (offer->chm_montrig % VMBUS_MONTRIG_LEN);
	}

	/* Select default cpu for this channel. */
	vmbus_chan_cpu_default(chan);

	error = vmbus_chan_add(chan);
	if (error) {
		device_printf(sc->vmbus_dev, "add chan%u failed: %d\n",
		    chan->ch_id, error);
		vmbus_chan_free(chan);
		return;
	}

	if (VMBUS_CHAN_ISPRIMARY(chan)) {
		/*
		 * Add device for this primary channel.
		 *
		 * NOTE:
		 * Error is ignored here; don't have much to do if error
		 * really happens.
		 */
		hv_vmbus_child_device_register(chan);
	}
}

/*
 * XXX pretty broken; need rework.
 */
static void
vmbus_chan_msgproc_chrescind(struct vmbus_softc *sc,
    const struct vmbus_message *msg)
{
	const struct vmbus_chanmsg_chrescind *note;
	struct hv_vmbus_channel *chan;

	note = (const struct vmbus_chanmsg_chrescind *)msg->msg_data;
	if (note->chm_chanid > VMBUS_CHAN_MAX) {
		device_printf(sc->vmbus_dev, "invalid rescinded chan%u\n",
		    note->chm_chanid);
		return;
	}

	if (bootverbose) {
		device_printf(sc->vmbus_dev, "chan%u rescinded\n",
		    note->chm_chanid);
	}

	chan = sc->vmbus_chmap[note->chm_chanid];
	if (chan == NULL)
		return;
	sc->vmbus_chmap[note->chm_chanid] = NULL;

	taskqueue_enqueue(taskqueue_thread, &chan->ch_detach_task);
}

static void
vmbus_chan_detach_task(void *xchan, int pending __unused)
{
	struct hv_vmbus_channel *chan = xchan;

	if (VMBUS_CHAN_ISPRIMARY(chan)) {
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
		mtx_lock(&pri_chan->sc_lock);
		TAILQ_REMOVE(&pri_chan->sc_list_anchor, chan, sc_list_entry);
		KASSERT(pri_chan->subchan_cnt > 0,
		    ("invalid subchan_cnt %d", pri_chan->subchan_cnt));
		pri_chan->subchan_cnt--;
		mtx_unlock(&pri_chan->sc_lock);
		wakeup(pri_chan);

		vmbus_chan_free(chan);
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

/*
 * Detach all devices and destroy the corresponding primary channels.
 */
void
vmbus_chan_destroy_all(struct vmbus_softc *sc)
{
	struct hv_vmbus_channel *chan;

	mtx_lock(&sc->vmbus_prichan_lock);
	while ((chan = TAILQ_FIRST(&sc->vmbus_prichans)) != NULL) {
		KASSERT(VMBUS_CHAN_ISPRIMARY(chan), ("not primary channel"));
		TAILQ_REMOVE(&sc->vmbus_prichans, chan, ch_prilink);
		mtx_unlock(&sc->vmbus_prichan_lock);

		hv_vmbus_child_device_unregister(chan);
		vmbus_chan_free(chan);

		mtx_lock(&sc->vmbus_prichan_lock);
	}
	bzero(sc->vmbus_chmap,
	    sizeof(struct hv_vmbus_channel *) * VMBUS_CHAN_MAX);
	mtx_unlock(&sc->vmbus_prichan_lock);
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
		if ((new_channel->ch_stflags & VMBUS_CHAN_ST_OPENED) == 0) {
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
	if (msg_type >= VMBUS_CHANMSG_TYPE_MAX) {
		device_printf(sc->vmbus_dev, "unknown message type 0x%x\n",
		    msg_type);
		return;
	}

	msg_proc = vmbus_chanmsg_process[msg_type];
	if (msg_proc != NULL)
		msg_proc(sc, msg);
}
