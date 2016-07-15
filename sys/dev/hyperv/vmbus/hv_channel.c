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

#include <machine/atomic.h>
#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/vmbus/hv_vmbus_priv.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_var.h>

static void 	vmbus_chan_send_event(hv_vmbus_channel* channel);
static void	vmbus_chan_update_evtflagcnt(struct vmbus_softc *,
		    const struct hv_vmbus_channel *);

static void	vmbus_chan_task(void *, int);
static void	vmbus_chan_task_nobatch(void *, int);
static void	vmbus_chan_detach_task(void *, int);

static void	vmbus_chan_msgproc_choffer(struct vmbus_softc *,
		    const struct vmbus_message *);
static void	vmbus_chan_msgproc_chrescind(struct vmbus_softc *,
		    const struct vmbus_message *);

/*
 * Vmbus channel message processing.
 */
static const vmbus_chanmsg_proc_t
vmbus_chan_msgprocs[VMBUS_CHANMSG_TYPE_MAX] = {
	VMBUS_CHANMSG_PROC(CHOFFER,	vmbus_chan_msgproc_choffer),
	VMBUS_CHANMSG_PROC(CHRESCIND,	vmbus_chan_msgproc_chrescind),

	VMBUS_CHANMSG_PROC_WAKEUP(CHOPEN_RESP),
	VMBUS_CHANMSG_PROC_WAKEUP(GPADL_CONNRESP),
	VMBUS_CHANMSG_PROC_WAKEUP(GPADL_DISCONNRESP)
};

/**
 *  @brief Trigger an event notification on the specified channel
 */
static void
vmbus_chan_send_event(hv_vmbus_channel *channel)
{
	struct vmbus_softc *sc = channel->vmbus_sc;
	uint32_t chanid = channel->ch_id;

	atomic_set_long(&sc->vmbus_tx_evtflags[chanid >> VMBUS_EVTFLAG_SHIFT],
	    1UL << (chanid & VMBUS_EVTFLAG_MASK));

	if (channel->ch_flags & VMBUS_CHAN_FLAG_HASMNF) {
		atomic_set_int(
		&sc->vmbus_mnf2->mnf_trigs[channel->ch_montrig_idx].mt_pending,
		channel->ch_montrig_mask);
	} else {
		hypercall_signal_event(channel->ch_monprm_dma.hv_paddr);
	}
}

static int
vmbus_channel_sysctl_monalloc(SYSCTL_HANDLER_ARGS)
{
	struct hv_vmbus_channel *chan = arg1;
	int alloc = 0;

	if (chan->ch_flags & VMBUS_CHAN_FLAG_HASMNF)
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
	
	hv_vmbus_channel* primary_ch = channel->ch_prichan;

	if (primary_ch == NULL) {
		dev = channel->ch_dev;
		ch_id = channel->ch_id;
	} else {
		dev = primary_ch->ch_dev;
		ch_id = primary_ch->ch_id;
		sub_ch_id = channel->ch_subidx;
	}
	ctx = &channel->ch_sysctl_ctx;
	sysctl_ctx_init(ctx);
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
		    &channel->ch_id, 0, "channel id");
	}
	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(devch_id_sysctl), OID_AUTO,
	    "cpu", CTLFLAG_RD, &channel->ch_cpuid, 0, "owner CPU id");
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
	vmbus_chan_callback_t		cb,
	void				*cbarg)
{
	struct vmbus_softc *sc = new_channel->vmbus_sc;
	const struct vmbus_chanmsg_chopen_resp *resp;
	const struct vmbus_message *msg;
	struct vmbus_chanmsg_chopen *req;
	struct vmbus_msghc *mh;
	uint32_t status;
	int ret = 0;
	uint8_t *br;

	if (user_data_len > VMBUS_CHANMSG_CHOPEN_UDATA_SIZE) {
		device_printf(sc->vmbus_dev,
		    "invalid udata len %u for chan%u\n",
		    user_data_len, new_channel->ch_id);
		return EINVAL;
	}
	KASSERT((send_ring_buffer_size & PAGE_MASK) == 0,
	    ("send bufring size is not multiple page"));
	KASSERT((recv_ring_buffer_size & PAGE_MASK) == 0,
	    ("recv bufring size is not multiple page"));

	if (atomic_testandset_int(&new_channel->ch_stflags,
	    VMBUS_CHAN_ST_OPENED_SHIFT))
		panic("double-open chan%u", new_channel->ch_id);

	new_channel->ch_cb = cb;
	new_channel->ch_cbarg = cbarg;

	vmbus_chan_update_evtflagcnt(sc, new_channel);

	new_channel->ch_tq = VMBUS_PCPU_GET(new_channel->vmbus_sc, event_tq,
	    new_channel->ch_cpuid);
	if (new_channel->ch_flags & VMBUS_CHAN_FLAG_BATCHREAD) {
		TASK_INIT(&new_channel->ch_task, 0, vmbus_chan_task,
		    new_channel);
	} else {
		TASK_INIT(&new_channel->ch_task, 0, vmbus_chan_task_nobatch,
		    new_channel);
	}

	/*
	 * Allocate the TX+RX bufrings.
	 * XXX should use ch_dev dtag
	 */
	br = hyperv_dmamem_alloc(bus_get_dma_tag(sc->vmbus_dev),
	    PAGE_SIZE, 0, send_ring_buffer_size + recv_ring_buffer_size,
	    &new_channel->ch_bufring_dma, BUS_DMA_WAITOK | BUS_DMA_ZERO);
	if (br == NULL) {
		device_printf(sc->vmbus_dev, "bufring allocation failed\n");
		ret = ENOMEM;
		goto failed;
	}
	new_channel->ch_bufring = br;

	/* TX bufring comes first */
	hv_vmbus_ring_buffer_init(&new_channel->outbound,
	    br, send_ring_buffer_size);
	/* RX bufring immediately follows TX bufring */
	hv_vmbus_ring_buffer_init(&new_channel->inbound,
	    br + send_ring_buffer_size, recv_ring_buffer_size);

	/* Create sysctl tree for this channel */
	vmbus_channel_sysctl_create(new_channel);

	/*
	 * Connect the bufrings, both RX and TX, to this channel.
	 */
	ret = vmbus_chan_gpadl_connect(new_channel,
		new_channel->ch_bufring_dma.hv_paddr,
		send_ring_buffer_size + recv_ring_buffer_size,
		&new_channel->ch_bufring_gpadl);
	if (ret != 0) {
		device_printf(sc->vmbus_dev,
		    "failed to connect bufring GPADL to chan%u\n",
		    new_channel->ch_id);
		goto failed;
	}

	/*
	 * Open channel w/ the bufring GPADL on the target CPU.
	 */
	mh = vmbus_msghc_get(sc, sizeof(*req));
	if (mh == NULL) {
		device_printf(sc->vmbus_dev,
		    "can not get msg hypercall for chopen(chan%u)\n",
		    new_channel->ch_id);
		ret = ENXIO;
		goto failed;
	}

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_CHOPEN;
	req->chm_chanid = new_channel->ch_id;
	req->chm_openid = new_channel->ch_id;
	req->chm_gpadl = new_channel->ch_bufring_gpadl;
	req->chm_vcpuid = new_channel->ch_vcpuid;
	req->chm_rxbr_pgofs = send_ring_buffer_size >> PAGE_SHIFT;
	if (user_data_len)
		memcpy(req->chm_udata, user_data, user_data_len);

	ret = vmbus_msghc_exec(sc, mh);
	if (ret != 0) {
		device_printf(sc->vmbus_dev,
		    "chopen(chan%u) msg hypercall exec failed: %d\n",
		    new_channel->ch_id, ret);
		vmbus_msghc_put(sc, mh);
		goto failed;
	}

	msg = vmbus_msghc_wait_result(sc, mh);
	resp = (const struct vmbus_chanmsg_chopen_resp *)msg->msg_data;
	status = resp->chm_status;

	vmbus_msghc_put(sc, mh);

	if (status == 0) {
		if (bootverbose) {
			device_printf(sc->vmbus_dev, "chan%u opened\n",
			    new_channel->ch_id);
		}
		return 0;
	}

	device_printf(sc->vmbus_dev, "failed to open chan%u\n",
	    new_channel->ch_id);
	ret = ENXIO;

failed:
	if (new_channel->ch_bufring_gpadl) {
		hv_vmbus_channel_teardown_gpdal(new_channel,
		    new_channel->ch_bufring_gpadl);
		new_channel->ch_bufring_gpadl = 0;
	}
	if (new_channel->ch_bufring != NULL) {
		hyperv_dmamem_free(&new_channel->ch_bufring_dma,
		    new_channel->ch_bufring);
		new_channel->ch_bufring = NULL;
	}
	atomic_clear_int(&new_channel->ch_stflags, VMBUS_CHAN_ST_OPENED);
	return ret;
}

/**
 * @brief Establish a GPADL for the specified buffer
 */
int
hv_vmbus_channel_establish_gpadl(struct hv_vmbus_channel *channel,
    void *contig_buffer, uint32_t size, uint32_t *gpadl)
{
	return vmbus_chan_gpadl_connect(channel,
	    hv_get_phys_addr(contig_buffer), size, gpadl);
}

int
vmbus_chan_gpadl_connect(struct hv_vmbus_channel *chan, bus_addr_t paddr,
    int size, uint32_t *gpadl0)
{
	struct vmbus_softc *sc = chan->vmbus_sc;
	struct vmbus_msghc *mh;
	struct vmbus_chanmsg_gpadl_conn *req;
	const struct vmbus_message *msg;
	size_t reqsz;
	uint32_t gpadl, status;
	int page_count, range_len, i, cnt, error;
	uint64_t page_id;

	/*
	 * Preliminary checks.
	 */

	KASSERT((size & PAGE_MASK) == 0,
	    ("invalid GPA size %d, not multiple page size", size));
	page_count = size >> PAGE_SHIFT;

	KASSERT((paddr & PAGE_MASK) == 0,
	    ("GPA is not page aligned %jx", (uintmax_t)paddr));
	page_id = paddr >> PAGE_SHIFT;

	range_len = __offsetof(struct vmbus_gpa_range, gpa_page[page_count]);
	/*
	 * We don't support multiple GPA ranges.
	 */
	if (range_len > UINT16_MAX) {
		device_printf(sc->vmbus_dev, "GPA too large, %d pages\n",
		    page_count);
		return EOPNOTSUPP;
	}

	/*
	 * Allocate GPADL id.
	 */
	gpadl = vmbus_gpadl_alloc(sc);
	*gpadl0 = gpadl;

	/*
	 * Connect this GPADL to the target channel.
	 *
	 * NOTE:
	 * Since each message can only hold small set of page
	 * addresses, several messages may be required to
	 * complete the connection.
	 */
	if (page_count > VMBUS_CHANMSG_GPADL_CONN_PGMAX)
		cnt = VMBUS_CHANMSG_GPADL_CONN_PGMAX;
	else
		cnt = page_count;
	page_count -= cnt;

	reqsz = __offsetof(struct vmbus_chanmsg_gpadl_conn,
	    chm_range.gpa_page[cnt]);
	mh = vmbus_msghc_get(sc, reqsz);
	if (mh == NULL) {
		device_printf(sc->vmbus_dev,
		    "can not get msg hypercall for gpadl->chan%u\n",
		    chan->ch_id);
		return EIO;
	}

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_GPADL_CONN;
	req->chm_chanid = chan->ch_id;
	req->chm_gpadl = gpadl;
	req->chm_range_len = range_len;
	req->chm_range_cnt = 1;
	req->chm_range.gpa_len = size;
	req->chm_range.gpa_ofs = 0;
	for (i = 0; i < cnt; ++i)
		req->chm_range.gpa_page[i] = page_id++;

	error = vmbus_msghc_exec(sc, mh);
	if (error) {
		device_printf(sc->vmbus_dev,
		    "gpadl->chan%u msg hypercall exec failed: %d\n",
		    chan->ch_id, error);
		vmbus_msghc_put(sc, mh);
		return error;
	}

	while (page_count > 0) {
		struct vmbus_chanmsg_gpadl_subconn *subreq;

		if (page_count > VMBUS_CHANMSG_GPADL_SUBCONN_PGMAX)
			cnt = VMBUS_CHANMSG_GPADL_SUBCONN_PGMAX;
		else
			cnt = page_count;
		page_count -= cnt;

		reqsz = __offsetof(struct vmbus_chanmsg_gpadl_subconn,
		    chm_gpa_page[cnt]);
		vmbus_msghc_reset(mh, reqsz);

		subreq = vmbus_msghc_dataptr(mh);
		subreq->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_GPADL_SUBCONN;
		subreq->chm_gpadl = gpadl;
		for (i = 0; i < cnt; ++i)
			subreq->chm_gpa_page[i] = page_id++;

		vmbus_msghc_exec_noresult(mh);
	}
	KASSERT(page_count == 0, ("invalid page count %d", page_count));

	msg = vmbus_msghc_wait_result(sc, mh);
	status = ((const struct vmbus_chanmsg_gpadl_connresp *)
	    msg->msg_data)->chm_status;

	vmbus_msghc_put(sc, mh);

	if (status != 0) {
		device_printf(sc->vmbus_dev, "gpadl->chan%u failed: "
		    "status %u\n", chan->ch_id, status);
		return EIO;
	} else {
		if (bootverbose) {
			device_printf(sc->vmbus_dev, "gpadl->chan%u "
			    "succeeded\n", chan->ch_id);
		}
	}
	return 0;
}

/*
 * Disconnect the GPA from the target channel
 */
int
hv_vmbus_channel_teardown_gpdal(struct hv_vmbus_channel *chan, uint32_t gpadl)
{
	struct vmbus_softc *sc = chan->vmbus_sc;
	struct vmbus_msghc *mh;
	struct vmbus_chanmsg_gpadl_disconn *req;
	int error;

	mh = vmbus_msghc_get(sc, sizeof(*req));
	if (mh == NULL) {
		device_printf(sc->vmbus_dev,
		    "can not get msg hypercall for gpa x->chan%u\n",
		    chan->ch_id);
		return EBUSY;
	}

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_GPADL_DISCONN;
	req->chm_chanid = chan->ch_id;
	req->chm_gpadl = gpadl;

	error = vmbus_msghc_exec(sc, mh);
	if (error) {
		device_printf(sc->vmbus_dev,
		    "gpa x->chan%u msg hypercall exec failed: %d\n",
		    chan->ch_id, error);
		vmbus_msghc_put(sc, mh);
		return error;
	}

	vmbus_msghc_wait_result(sc, mh);
	/* Discard result; no useful information */
	vmbus_msghc_put(sc, mh);

	return 0;
}

static void
hv_vmbus_channel_close_internal(hv_vmbus_channel *channel)
{
	struct vmbus_softc *sc = channel->vmbus_sc;
	struct vmbus_msghc *mh;
	struct vmbus_chanmsg_chclose *req;
	struct taskqueue *tq = channel->ch_tq;
	int error;

	/* TODO: stringent check */
	atomic_clear_int(&channel->ch_stflags, VMBUS_CHAN_ST_OPENED);

	sysctl_ctx_free(&channel->ch_sysctl_ctx);

	/*
	 * Set ch_tq to NULL to avoid more requests be scheduled
	 */
	channel->ch_tq = NULL;
	taskqueue_drain(tq, &channel->ch_task);
	channel->ch_cb = NULL;

	/**
	 * Send a closing message
	 */

	mh = vmbus_msghc_get(sc, sizeof(*req));
	if (mh == NULL) {
		device_printf(sc->vmbus_dev,
		    "can not get msg hypercall for chclose(chan%u)\n",
		    channel->ch_id);
		return;
	}

	req = vmbus_msghc_dataptr(mh);
	req->chm_hdr.chm_type = VMBUS_CHANMSG_TYPE_CHCLOSE;
	req->chm_chanid = channel->ch_id;

	error = vmbus_msghc_exec_noresult(mh);
	vmbus_msghc_put(sc, mh);

	if (error) {
		device_printf(sc->vmbus_dev,
		    "chclose(chan%u) msg hypercall exec failed: %d\n",
		    channel->ch_id, error);
		return;
	} else if (bootverbose) {
		device_printf(sc->vmbus_dev, "close chan%u\n",
		    channel->ch_id);
	}

	/* Tear down the gpadl for the channel's ring buffer */
	if (channel->ch_bufring_gpadl) {
		hv_vmbus_channel_teardown_gpdal(channel,
		    channel->ch_bufring_gpadl);
		channel->ch_bufring_gpadl = 0;
	}

	/* TODO: Send a msg to release the childRelId */

	/* cleanup the ring buffers for this channel */
	hv_ring_buffer_cleanup(&channel->outbound);
	hv_ring_buffer_cleanup(&channel->inbound);

	if (channel->ch_bufring != NULL) {
		hyperv_dmamem_free(&channel->ch_bufring_dma,
		    channel->ch_bufring);
		channel->ch_bufring = NULL;
	}
}

/*
 * Caller should make sure that all sub-channels have
 * been added to 'chan' and all to-be-closed channels
 * are not being opened.
 */
void
hv_vmbus_channel_close(struct hv_vmbus_channel *chan)
{
	int subchan_cnt;

	if (!VMBUS_CHAN_ISPRIMARY(chan)) {
		/*
		 * Sub-channel is closed when its primary channel
		 * is closed; done.
		 */
		return;
	}

	/*
	 * Close all sub-channels, if any.
	 */
	subchan_cnt = chan->ch_subchan_cnt;
	if (subchan_cnt > 0) {
		struct hv_vmbus_channel **subchan;
		int i;

		subchan = vmbus_get_subchan(chan, subchan_cnt);
		for (i = 0; i < subchan_cnt; ++i)
			hv_vmbus_channel_close_internal(subchan[i]);
		vmbus_rel_subchan(subchan, subchan_cnt);
	}

	/* Then close the primary channel. */
	hv_vmbus_channel_close_internal(chan);
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
	uint16_t		type,
	uint16_t		flags)
{
	int			ret = 0;
	struct vmbus_chanpkt pkt;
	uint32_t		packet_len;
	uint64_t		aligned_data;
	uint32_t		packet_len_aligned;
	boolean_t		need_sig;
	struct iovec		iov[3];

	packet_len = sizeof(pkt) + buffer_len;
	packet_len_aligned = roundup2(packet_len, VMBUS_CHANPKT_SIZE_ALIGN);
	aligned_data = 0;

	/*
	 * Setup channel packet.
	 */
	pkt.cp_hdr.cph_type = type;
	pkt.cp_hdr.cph_flags = flags;
	pkt.cp_hdr.cph_data_ofs = sizeof(pkt) >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.cp_hdr.cph_len = packet_len_aligned >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.cp_hdr.cph_xactid = request_id;

	iov[0].iov_base = &pkt;
	iov[0].iov_len = sizeof(pkt);

	iov[1].iov_base = buffer;
	iov[1].iov_len = buffer_len;

	iov[2].iov_base = &aligned_data;
	iov[2].iov_len = packet_len_aligned - packet_len;

	ret = hv_ring_buffer_write(&channel->outbound, iov, 3, &need_sig);

	/* TODO: We should determine if this is optional */
	if (ret == 0 && need_sig)
		vmbus_chan_send_event(channel);

	return (ret);
}

int
vmbus_chan_send_sglist(struct hv_vmbus_channel *chan,
    struct vmbus_gpa sg[], int sglen, void *data, int dlen, uint64_t xactid)
{
	struct vmbus_chanpkt_sglist pkt;
	int pktlen, pad_pktlen, hlen, error;
	struct iovec iov[4];
	boolean_t send_evt;
	uint64_t pad = 0;

	KASSERT(sglen < VMBUS_CHAN_SGLIST_MAX,
	    ("invalid sglist len %d", sglen));

	hlen = __offsetof(struct vmbus_chanpkt_sglist, cp_gpa[sglen]);
	pktlen = hlen + dlen;
	pad_pktlen = roundup2(pktlen, VMBUS_CHANPKT_SIZE_ALIGN);

	pkt.cp_hdr.cph_type = VMBUS_CHANPKT_TYPE_GPA;
	pkt.cp_hdr.cph_flags = VMBUS_CHANPKT_FLAG_RC;
	pkt.cp_hdr.cph_data_ofs = hlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.cp_hdr.cph_len = pad_pktlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.cp_hdr.cph_xactid = xactid;
	pkt.cp_rsvd = 0;
	pkt.cp_gpa_cnt = sglen;

	iov[0].iov_base = &pkt;
	iov[0].iov_len = sizeof(pkt);
	iov[1].iov_base = sg;
	iov[1].iov_len = sizeof(struct vmbus_gpa) * sglen;
	iov[2].iov_base = data;
	iov[2].iov_len = dlen;
	iov[3].iov_base = &pad;
	iov[3].iov_len = pad_pktlen - pktlen;

	error = hv_ring_buffer_write(&chan->outbound, iov, 4, &send_evt);
	if (!error && send_evt)
		vmbus_chan_send_event(chan);
	return error;
}

int
vmbus_chan_send_prplist(struct hv_vmbus_channel *chan,
    struct vmbus_gpa_range *prp, int prp_cnt, void *data, int dlen,
    uint64_t xactid)
{
	struct vmbus_chanpkt_prplist pkt;
	int pktlen, pad_pktlen, hlen, error;
	struct iovec iov[4];
	boolean_t send_evt;
	uint64_t pad = 0;

	KASSERT(prp_cnt < VMBUS_CHAN_PRPLIST_MAX,
	    ("invalid prplist entry count %d", prp_cnt));

	hlen = __offsetof(struct vmbus_chanpkt_prplist,
	    cp_range[0].gpa_page[prp_cnt]);
	pktlen = hlen + dlen;
	pad_pktlen = roundup2(pktlen, VMBUS_CHANPKT_SIZE_ALIGN);

	pkt.cp_hdr.cph_type = VMBUS_CHANPKT_TYPE_GPA;
	pkt.cp_hdr.cph_flags = VMBUS_CHANPKT_FLAG_RC;
	pkt.cp_hdr.cph_data_ofs = hlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.cp_hdr.cph_len = pad_pktlen >> VMBUS_CHANPKT_SIZE_SHIFT;
	pkt.cp_hdr.cph_xactid = xactid;
	pkt.cp_rsvd = 0;
	pkt.cp_range_cnt = 1;

	iov[0].iov_base = &pkt;
	iov[0].iov_len = sizeof(pkt);
	iov[1].iov_base = prp;
	iov[1].iov_len = __offsetof(struct vmbus_gpa_range, gpa_page[prp_cnt]);
	iov[2].iov_base = data;
	iov[2].iov_len = dlen;
	iov[3].iov_base = &pad;
	iov[3].iov_len = pad_pktlen - pktlen;

	error = hv_ring_buffer_write(&chan->outbound, iov, 4, &send_evt);
	if (!error && send_evt)
		vmbus_chan_send_event(chan);
	return error;
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

static void
vmbus_chan_task(void *xchan, int pending __unused)
{
	struct hv_vmbus_channel *chan = xchan;
	vmbus_chan_callback_t cb = chan->ch_cb;
	void *cbarg = chan->ch_cbarg;

	/*
	 * Optimize host to guest signaling by ensuring:
	 * 1. While reading the channel, we disable interrupts from
	 *    host.
	 * 2. Ensure that we process all posted messages from the host
	 *    before returning from this callback.
	 * 3. Once we return, enable signaling from the host. Once this
	 *    state is set we check to see if additional packets are
	 *    available to read. In this case we repeat the process.
	 *
	 * NOTE: Interrupt has been disabled in the ISR.
	 */
	for (;;) {
		uint32_t left;

		cb(cbarg);

		left = hv_ring_buffer_read_end(&chan->inbound);
		if (left == 0) {
			/* No more data in RX bufring; done */
			break;
		}
		hv_ring_buffer_read_begin(&chan->inbound);
	}
}

static void
vmbus_chan_task_nobatch(void *xchan, int pending __unused)
{
	struct hv_vmbus_channel *chan = xchan;

	chan->ch_cb(chan->ch_cbarg);
}

static __inline void
vmbus_event_flags_proc(struct vmbus_softc *sc, volatile u_long *event_flags,
    int flag_cnt)
{
	int f;

	for (f = 0; f < flag_cnt; ++f) {
		uint32_t chid_base;
		u_long flags;
		int chid_ofs;

		if (event_flags[f] == 0)
			continue;

		flags = atomic_swap_long(&event_flags[f], 0);
		chid_base = f << VMBUS_EVTFLAG_SHIFT;

		while ((chid_ofs = ffsl(flags)) != 0) {
			struct hv_vmbus_channel *channel;

			--chid_ofs; /* NOTE: ffsl is 1-based */
			flags &= ~(1UL << chid_ofs);

			channel = sc->vmbus_chmap[chid_base + chid_ofs];

			/* if channel is closed or closing */
			if (channel == NULL || channel->ch_tq == NULL)
				continue;

			if (channel->ch_flags & VMBUS_CHAN_FLAG_BATCHREAD)
				hv_ring_buffer_read_begin(&channel->inbound);
			taskqueue_enqueue(channel->ch_tq, &channel->ch_task);
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
	vmbus_event_flags_proc(sc, eventf->evt_flags,
	    VMBUS_PCPU_GET(sc, event_flags_cnt, cpu));
}

void
vmbus_event_proc_compat(struct vmbus_softc *sc, int cpu)
{
	struct vmbus_evtflags *eventf;

	eventf = VMBUS_PCPU_GET(sc, event_flags, cpu) + VMBUS_SINT_MESSAGE;
	if (atomic_testandclear_long(&eventf->evt_flags[0], 0)) {
		vmbus_event_flags_proc(sc, sc->vmbus_rx_evtflags,
		    VMBUS_CHAN_MAX_COMPAT >> VMBUS_EVTFLAG_SHIFT);
	}
}

static void
vmbus_chan_update_evtflagcnt(struct vmbus_softc *sc,
    const struct hv_vmbus_channel *chan)
{
	volatile int *flag_cnt_ptr;
	int flag_cnt;

	flag_cnt = (chan->ch_id / VMBUS_EVTFLAG_LEN) + 1;
	flag_cnt_ptr = VMBUS_PCPU_PTR(sc, event_flags_cnt, chan->ch_cpuid);

	for (;;) {
		int old_flag_cnt;

		old_flag_cnt = *flag_cnt_ptr;
		if (old_flag_cnt >= flag_cnt)
			break;
		if (atomic_cmpset_int(flag_cnt_ptr, old_flag_cnt, flag_cnt)) {
			if (bootverbose) {
				device_printf(sc->vmbus_dev,
				    "channel%u update cpu%d flag_cnt to %d\n",
				    chan->ch_id, chan->ch_cpuid, flag_cnt);
			}
			break;
		}
	}
}

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
	mtx_init(&chan->ch_subchan_lock, "vmbus subchan", NULL, MTX_DEF);
	TAILQ_INIT(&chan->ch_subchans);
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
	mtx_destroy(&chan->ch_subchan_lock);
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
		/*
		 * Sub-channel will have the same type GUID and instance
		 * GUID as its primary channel.
		 */
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

	newchan->ch_prichan = prichan;
	newchan->ch_dev = prichan->ch_dev;

	mtx_lock(&prichan->ch_subchan_lock);
	TAILQ_INSERT_TAIL(&prichan->ch_subchans, newchan, ch_sublink);
	/*
	 * Bump up sub-channel count and notify anyone that is
	 * interested in this sub-channel, after this sub-channel
	 * is setup.
	 */
	prichan->ch_subchan_cnt++;
	mtx_unlock(&prichan->ch_subchan_lock);
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

	chan->ch_cpuid = cpu;
	chan->ch_vcpuid = VMBUS_PCPU_GET(chan->vmbus_sc, vcpuid, cpu);

	if (bootverbose) {
		printf("vmbus_chan%u: assigned to cpu%u [vcpu%u]\n",
		    chan->ch_id, chan->ch_cpuid, chan->ch_vcpuid);
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
		vmbus_add_child(chan);
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
		vmbus_delete_child(chan);
		/* NOTE: DO NOT free primary channel for now */
	} else {
		struct vmbus_softc *sc = chan->vmbus_sc;
		struct hv_vmbus_channel *pri_chan = chan->ch_prichan;
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
		mtx_lock(&pri_chan->ch_subchan_lock);
		TAILQ_REMOVE(&pri_chan->ch_subchans, chan, ch_sublink);
		KASSERT(pri_chan->ch_subchan_cnt > 0,
		    ("invalid subchan_cnt %d", pri_chan->ch_subchan_cnt));
		pri_chan->ch_subchan_cnt--;
		mtx_unlock(&pri_chan->ch_subchan_lock);
		wakeup(pri_chan);

		vmbus_chan_free(chan);
	}
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

		vmbus_delete_child(chan);
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

	if (TAILQ_EMPTY(&primary->ch_subchans)) {
		return outgoing_channel;
	}

	if (smp_pro_id >= MAXCPU) {
		return outgoing_channel;
	}

	cur_vcpu = VMBUS_PCPU_GET(primary->vmbus_sc, vcpuid, smp_pro_id);
	
	/* XXX need lock */
	TAILQ_FOREACH(new_channel, &primary->ch_subchans, ch_sublink) {
		if ((new_channel->ch_stflags & VMBUS_CHAN_ST_OPENED) == 0) {
			continue;
		}

		if (new_channel->ch_vcpuid == cur_vcpu){
			return new_channel;
		}

		old_cpu_distance = ((outgoing_channel->ch_vcpuid > cur_vcpu) ?
		    (outgoing_channel->ch_vcpuid - cur_vcpu) :
		    (cur_vcpu - outgoing_channel->ch_vcpuid));

		new_cpu_distance = ((new_channel->ch_vcpuid > cur_vcpu) ?
		    (new_channel->ch_vcpuid - cur_vcpu) :
		    (cur_vcpu - new_channel->ch_vcpuid));

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

	mtx_lock(&pri_chan->ch_subchan_lock);

	while (pri_chan->ch_subchan_cnt < subchan_cnt)
		mtx_sleep(pri_chan, &pri_chan->ch_subchan_lock, 0, "subch", 0);

	i = 0;
	TAILQ_FOREACH(chan, &pri_chan->ch_subchans, ch_sublink) {
		/* TODO: refcnt chan */
		ret[i] = chan;

		++i;
		if (i == subchan_cnt)
			break;
	}
	KASSERT(i == subchan_cnt, ("invalid subchan count %d, should be %d",
	    pri_chan->ch_subchan_cnt, subchan_cnt));

	mtx_unlock(&pri_chan->ch_subchan_lock);

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
	mtx_lock(&pri_chan->ch_subchan_lock);
	while (pri_chan->ch_subchan_cnt > 0)
		mtx_sleep(pri_chan, &pri_chan->ch_subchan_lock, 0, "dsubch", 0);
	mtx_unlock(&pri_chan->ch_subchan_lock);
}

void
vmbus_chan_msgproc(struct vmbus_softc *sc, const struct vmbus_message *msg)
{
	vmbus_chanmsg_proc_t msg_proc;
	uint32_t msg_type;

	msg_type = ((const struct vmbus_chanmsg_hdr *)msg->msg_data)->chm_type;
	KASSERT(msg_type < VMBUS_CHANMSG_TYPE_MAX,
	    ("invalid message type %u", msg_type));

	msg_proc = vmbus_chan_msgprocs[msg_type];
	if (msg_proc != NULL)
		msg_proc(sc, msg);
}
