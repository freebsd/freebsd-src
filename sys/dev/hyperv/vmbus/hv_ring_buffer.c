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
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <dev/hyperv/vmbus/vmbus_reg.h>
#include <dev/hyperv/vmbus/vmbus_brvar.h>

/* Amount of space available for write */
#define	VMBUS_BR_WAVAIL(r, w, z)	\
	(((w) >= (r)) ? ((z) - ((w) - (r))) : ((r) - (w)))

static uint32_t	copy_to_ring_buffer(const struct vmbus_txbr *tbr,
		    uint32_t start_write_offset, const uint8_t *src,
		    uint32_t src_len);
static uint32_t copy_from_ring_buffer(const struct vmbus_rxbr *rbr,
		    char *dest, uint32_t dest_len, uint32_t start_read_offset);

static int
vmbus_br_sysctl_state(SYSCTL_HANDLER_ARGS)
{
	const struct vmbus_br *br = arg1;
	uint32_t rindex, windex, imask, ravail, wavail;
	char state[256];

	rindex = br->vbr_rindex;
	windex = br->vbr_windex;
	imask = br->vbr_imask;
	wavail = VMBUS_BR_WAVAIL(rindex, windex, br->vbr_dsize);
	ravail = br->vbr_dsize - wavail;

	snprintf(state, sizeof(state),
	    "rindex:%u windex:%u imask:%u ravail:%u wavail:%u",
	    rindex, windex, imask, ravail, wavail);
	return sysctl_handle_string(oidp, state, sizeof(state), req);
}

/*
 * Binary bufring states.
 */
static int
vmbus_br_sysctl_state_bin(SYSCTL_HANDLER_ARGS)
{
#define BR_STATE_RIDX	0
#define BR_STATE_WIDX	1
#define BR_STATE_IMSK	2
#define BR_STATE_RSPC	3
#define BR_STATE_WSPC	4
#define BR_STATE_MAX	5

	const struct vmbus_br *br = arg1;
	uint32_t rindex, windex, wavail, state[BR_STATE_MAX];

	rindex = br->vbr_rindex;
	windex = br->vbr_windex;
	wavail = VMBUS_BR_WAVAIL(rindex, windex, br->vbr_dsize);

	state[BR_STATE_RIDX] = rindex;
	state[BR_STATE_WIDX] = windex;
	state[BR_STATE_IMSK] = br->vbr_imask;
	state[BR_STATE_WSPC] = wavail;
	state[BR_STATE_RSPC] = br->vbr_dsize - wavail;

	return sysctl_handle_opaque(oidp, state, sizeof(state), req);
}

void
vmbus_br_sysctl_create(struct sysctl_ctx_list *ctx, struct sysctl_oid *br_tree,
    struct vmbus_br *br, const char *name)
{
	struct sysctl_oid *tree;
	char desc[64];

	tree = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(br_tree), OID_AUTO,
	    name, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");
	if (tree == NULL)
		return;

	snprintf(desc, sizeof(desc), "%s state", name);
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "state",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    br, 0, vmbus_br_sysctl_state, "A", desc);

	snprintf(desc, sizeof(desc), "%s binary state", name);
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "state_bin",
	    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    br, 0, vmbus_br_sysctl_state_bin, "IU", desc);
}

void
vmbus_rxbr_intr_mask(struct vmbus_rxbr *rbr)
{
	rbr->rxbr_imask = 1;
	mb();
}

static __inline uint32_t
vmbus_rxbr_avail(const struct vmbus_rxbr *rbr)
{
	uint32_t rindex, windex;

	/* Get snapshot */
	rindex = rbr->rxbr_rindex;
	windex = rbr->rxbr_windex;

	return (rbr->rxbr_dsize -
	    VMBUS_BR_WAVAIL(rindex, windex, rbr->rxbr_dsize));
}

uint32_t
vmbus_rxbr_intr_unmask(struct vmbus_rxbr *rbr)
{
	rbr->rxbr_imask = 0;
	mb();

	/*
	 * Now check to see if the ring buffer is still empty.
	 * If it is not, we raced and we need to process new
	 * incoming channel packets.
	 */
	return vmbus_rxbr_avail(rbr);
}

/*
 * When we write to the ring buffer, check if the host needs to be
 * signaled.
 *
 * The contract:
 * - The host guarantees that while it is draining the TX bufring,
 *   it will set the br_imask to indicate it does not need to be
 *   interrupted when new data are added.
 * - The host guarantees that it will completely drain the TX bufring
 *   before exiting the read loop.  Further, once the TX bufring is
 *   empty, it will clear the br_imask and re-check to see if new
 *   data have arrived.
 */
static boolean_t
hv_ring_buffer_needsig_on_write(uint32_t old_write_location,
    const struct vmbus_txbr *tbr)
{
	mb();
	if (tbr->txbr_imask)
		return (FALSE);

	/* XXX only compiler fence is needed */
	/* Read memory barrier */
	rmb();

	/*
	 * This is the only case we need to signal when the
	 * ring transitions from being empty to non-empty.
	 */
	if (old_write_location == tbr->txbr_rindex)
		return (TRUE);

	return (FALSE);
}

static void
vmbus_br_setup(struct vmbus_br *br, void *buf, int blen)
{
	br->vbr = buf;
	br->vbr_dsize = blen - sizeof(struct vmbus_bufring);
}

void
vmbus_rxbr_init(struct vmbus_rxbr *rbr)
{
	mtx_init(&rbr->rxbr_lock, "vmbus_rxbr", NULL, MTX_SPIN);
}

void
vmbus_rxbr_deinit(struct vmbus_rxbr *rbr)
{
	mtx_destroy(&rbr->rxbr_lock);
}

void
vmbus_rxbr_setup(struct vmbus_rxbr *rbr, void *buf, int blen)
{
	vmbus_br_setup(&rbr->rxbr, buf, blen);
}

void
vmbus_txbr_init(struct vmbus_txbr *tbr)
{
	mtx_init(&tbr->txbr_lock, "vmbus_txbr", NULL, MTX_SPIN);
}

void
vmbus_txbr_deinit(struct vmbus_txbr *tbr)
{
	mtx_destroy(&tbr->txbr_lock);
}

void
vmbus_txbr_setup(struct vmbus_txbr *tbr, void *buf, int blen)
{
	vmbus_br_setup(&tbr->txbr, buf, blen);
}

static __inline uint32_t
vmbus_txbr_avail(const struct vmbus_txbr *tbr)
{
	uint32_t rindex, windex;

	/* Get snapshot */
	rindex = tbr->txbr_rindex;
	windex = tbr->txbr_windex;

	return VMBUS_BR_WAVAIL(rindex, windex, tbr->txbr_dsize);
}

int
vmbus_txbr_write(struct vmbus_txbr *tbr, const struct iovec iov[], int iovlen,
    boolean_t *need_sig)
{
	int i = 0;
	uint32_t byte_avail_to_write;
	uint32_t old_write_location;
	uint32_t total_bytes_to_write = 0;
	volatile uint32_t next_write_location;
	uint64_t prev_indices = 0;

	for (i = 0; i < iovlen; i++)
		total_bytes_to_write += iov[i].iov_len;
	total_bytes_to_write += sizeof(uint64_t);

	mtx_lock_spin(&tbr->txbr_lock);

	byte_avail_to_write = vmbus_txbr_avail(tbr);

	/*
	 * NOTE:
	 * If this write is going to make br_windex same as br_rindex,
	 * i.e. the available space for write is same as the write size,
	 * we can't do it then, since br_windex == br_rindex means that
	 * the bufring is empty.
	 */
	if (byte_avail_to_write <= total_bytes_to_write) {
		mtx_unlock_spin(&tbr->txbr_lock);
		return (EAGAIN);
	}

	/*
	 * Copy the scattered channel packet to the TX bufring.
	 */
	next_write_location = tbr->txbr_windex;

	old_write_location = next_write_location;

	for (i = 0; i < iovlen; i++) {
		next_write_location = copy_to_ring_buffer(tbr,
		    next_write_location, iov[i].iov_base, iov[i].iov_len);
	}

	/*
	 * Set the offset of the current channel packet.
	 */
	prev_indices = ((uint64_t)tbr->txbr_windex) << 32;
	next_write_location = copy_to_ring_buffer(tbr,
	    next_write_location, (char *)&prev_indices, sizeof(uint64_t));

	/*
	 * XXX only compiler fence is needed.
	 * Full memory barrier before upding the write index. 
	 */
	mb();

	/*
	 * Now, update the write index.
	 */
	tbr->txbr_windex = next_write_location;

	mtx_unlock_spin(&tbr->txbr_lock);

	*need_sig = hv_ring_buffer_needsig_on_write(old_write_location, tbr);

	return (0);
}

int
vmbus_rxbr_peek(struct vmbus_rxbr *rbr, void *data, int dlen)
{
	uint32_t bytesAvailToRead;
	uint32_t nextReadLocation = 0;

	mtx_lock_spin(&rbr->rxbr_lock);

	bytesAvailToRead = vmbus_rxbr_avail(rbr);
	if (bytesAvailToRead < dlen) {
		mtx_unlock_spin(&rbr->rxbr_lock);
		return (EAGAIN);
	}

	nextReadLocation = rbr->rxbr_rindex;
	nextReadLocation = copy_from_ring_buffer(rbr, data, dlen,
	    nextReadLocation);

	mtx_unlock_spin(&rbr->rxbr_lock);

	return (0);
}

int
vmbus_rxbr_read(struct vmbus_rxbr *rbr, void *data, int dlen, uint32_t offset)
{
	uint32_t bytes_avail_to_read;
	uint32_t next_read_location = 0;
	uint64_t prev_indices = 0;

	KASSERT(dlen > 0, ("invalid dlen %d", dlen));

	mtx_lock_spin(&rbr->rxbr_lock);

	bytes_avail_to_read = vmbus_rxbr_avail(rbr);
	if (bytes_avail_to_read < dlen) {
		mtx_unlock_spin(&rbr->rxbr_lock);
		return (EAGAIN);
	}

	/*
	 * Copy channel packet from RX bufring.
	 */
	next_read_location = (rbr->rxbr_rindex + offset) % rbr->rxbr_dsize;
	next_read_location = copy_from_ring_buffer(rbr, data, dlen,
	    next_read_location);

	/*
	 * Discard this channel packet's start offset, which is useless
	 * for us.
	 */
	next_read_location = copy_from_ring_buffer(rbr,
	    (char *)&prev_indices, sizeof(uint64_t), next_read_location);

	/*
	 * XXX only compiler fence is needed.
	 * Make sure all reads are done before we update the read index since
	 * the writer may start writing to the read area once the read index
	 * is updated.
	 */
	wmb();

	/*
	 * Update the read index
	 */
	rbr->rxbr_rindex = next_read_location;

	mtx_unlock_spin(&rbr->rxbr_lock);

	return (0);
}

static uint32_t
copy_to_ring_buffer(const struct vmbus_txbr *tbr,
    uint32_t start_write_offset, const uint8_t *src, uint32_t src_len)
{
	char *ring_buffer = tbr->txbr_data;
	uint32_t ring_buffer_size = tbr->txbr_dsize;
	uint32_t fragLen;

	if (src_len > ring_buffer_size - start_write_offset) {
		/* Wrap-around detected! */
		fragLen = ring_buffer_size - start_write_offset;
		memcpy(ring_buffer + start_write_offset, src, fragLen);
		memcpy(ring_buffer, src + fragLen, src_len - fragLen);
	} else {
		memcpy(ring_buffer + start_write_offset, src, src_len);
	}

	start_write_offset += src_len;
	start_write_offset %= ring_buffer_size;

	return (start_write_offset);
}

static uint32_t
copy_from_ring_buffer(const struct vmbus_rxbr *rbr, char *dest,
    uint32_t dest_len, uint32_t start_read_offset)
{
	uint32_t fragLen;
	char *ring_buffer = rbr->rxbr_data;
	uint32_t ring_buffer_size = rbr->rxbr_dsize;

	if (dest_len > ring_buffer_size - start_read_offset) {
		/* Wrap-around detected at the src */
		fragLen = ring_buffer_size - start_read_offset;
		memcpy(dest, ring_buffer + start_read_offset, fragLen);
		memcpy(dest + fragLen, ring_buffer, dest_len - fragLen);
	} else {
		memcpy(dest, ring_buffer + start_read_offset, dest_len);
	}

	start_read_offset += dest_len;
	start_read_offset %= ring_buffer_size;

	return (start_read_offset);
}
