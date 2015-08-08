/*	$OpenBSD: if_iwm.c,v 1.39 2015/03/23 00:35:19 jsg Exp $	*/

/*
 * Copyright (c) 2014 genua mbh <info@genua.de>
 * Copyright (c) 2014 Fixup Software Ltd.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 * Driver version we are currently based off of is
 * Linux 3.14.3 (tag id a2df521e42b1d9a23f620ac79dbfe8655a8391dd)
 *
 ***********************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/firmware.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/linker.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <net/bpf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_radiotap.h>

#include <if_iwmreg.h>
#include <if_iwmvar.h>
#include <if_iwm_debug.h>
#include <if_iwm_binding.h>
#include <if_iwm_util.h>
#include <if_iwm_pcie_trans.h>

static void
iwm_dma_map_mem(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
        if (error != 0)
                return;
	KASSERT(nsegs <= 2, ("too many DMA segments, %d should be <= 2",
	    nsegs));
	if (nsegs > 1)
		KASSERT(segs[1].ds_addr == segs[0].ds_addr + segs[0].ds_len,
		    ("fragmented DMA memory"));
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

/*
 * Send a command to the firmware.  We try to implement the Linux
 * driver interface for the routine.
 * mostly from if_iwn (iwn_cmd()).
 *
 * For now, we always copy the first part and map the second one (if it exists).
 */
int
iwm_send_cmd(struct iwm_softc *sc, struct iwm_host_cmd *hcmd)
{
	struct iwm_tx_ring *ring = &sc->txq[IWM_MVM_CMD_QUEUE];
	struct iwm_tfd *desc;
	struct iwm_tx_data *data;
	struct iwm_device_cmd *cmd = NULL;
	bus_addr_t paddr;
	uint32_t addr_lo;
	int error = 0, i, paylen, off;
	int code;
	int async, wantresp;

	code = hcmd->id;
	async = hcmd->flags & IWM_CMD_ASYNC;
	wantresp = hcmd->flags & IWM_CMD_WANT_SKB;

	for (i = 0, paylen = 0; i < nitems(hcmd->len); i++) {
		paylen += hcmd->len[i];
	}

	/* if the command wants an answer, busy sc_cmd_resp */
	if (wantresp) {
		KASSERT(!async, ("invalid async parameter"));
		while (sc->sc_wantresp != -1)
			msleep(&sc->sc_wantresp, &sc->sc_mtx, 0, "iwmcmdsl", 0);
		sc->sc_wantresp = ring->qid << 16 | ring->cur;
		IWM_DPRINTF(sc, IWM_DEBUG_CMD,
		    "wantresp is %x\n", sc->sc_wantresp);
	}

	/*
	 * Is the hardware still available?  (after e.g. above wait).
	 */
	if (sc->sc_flags & IWM_FLAG_STOPPED) {
		error = ENXIO;
		goto out;
	}

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	if (paylen > sizeof(cmd->data)) {
		IWM_DPRINTF(sc, IWM_DEBUG_CMD,
		    "large command paylen=%u len0=%u\n",
			paylen, hcmd->len[0]);
		/* Command is too large */
		if (sizeof(cmd->hdr) + paylen > IWM_RBUF_SIZE) {
			error = EINVAL;
			goto out;
		}
		error = bus_dmamem_alloc(ring->data_dmat, (void **)&cmd,
		    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &data->map);
		if (error != 0)
			goto out;
		error = bus_dmamap_load(ring->data_dmat, data->map,
		    cmd, paylen + sizeof(cmd->hdr), iwm_dma_map_mem,
		    &paddr, BUS_DMA_NOWAIT);
		if (error != 0)
			goto out;
	} else {
		cmd = &ring->cmd[ring->cur];
		paddr = data->cmd_paddr;
	}

	cmd->hdr.code = code;
	cmd->hdr.flags = 0;
	cmd->hdr.qid = ring->qid;
	cmd->hdr.idx = ring->cur;

	for (i = 0, off = 0; i < nitems(hcmd->data); i++) {
		if (hcmd->len[i] == 0)
			continue;
		memcpy(cmd->data + off, hcmd->data[i], hcmd->len[i]);
		off += hcmd->len[i];
	}
	KASSERT(off == paylen, ("off %d != paylen %d", off, paylen));

	/* lo field is not aligned */
	addr_lo = htole32((uint32_t)paddr);
	memcpy(&desc->tbs[0].lo, &addr_lo, sizeof(uint32_t));
	desc->tbs[0].hi_n_len  = htole16(iwm_get_dma_hi_addr(paddr)
	    | ((sizeof(cmd->hdr) + paylen) << 4));
	desc->num_tbs = 1;

	IWM_DPRINTF(sc, IWM_DEBUG_CMD,
	    "%s: iwm_send_cmd 0x%x size=%lu %s\n",
	    __func__,
	    code,
	    (unsigned long) (hcmd->len[0] + hcmd->len[1] + sizeof(cmd->hdr)),
	    async ? " (async)" : "");

	if (hcmd->len[0] > sizeof(cmd->data)) {
		bus_dmamap_sync(ring->data_dmat, data->map,
		    BUS_DMASYNC_PREWRITE);
	} else {
		bus_dmamap_sync(ring->cmd_dma.tag, ring->cmd_dma.map,
		    BUS_DMASYNC_PREWRITE);
	}
	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);

	IWM_SETBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);
	if (!iwm_poll_bit(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_VAL_MAC_ACCESS_EN,
	    (IWM_CSR_GP_CNTRL_REG_FLAG_MAC_CLOCK_READY |
	     IWM_CSR_GP_CNTRL_REG_FLAG_GOING_TO_SLEEP), 15000)) {
		device_printf(sc->sc_dev,
		    "%s: acquiring device failed\n", __func__);
		error = EBUSY;
		goto out;
	}

#if 0
	iwm_update_sched(sc, ring->qid, ring->cur, 0, 0);
#endif
	IWM_DPRINTF(sc, IWM_DEBUG_CMD,
	    "sending command 0x%x qid %d, idx %d\n",
	    code, ring->qid, ring->cur);

	/* Kick command ring. */
	ring->cur = (ring->cur + 1) % IWM_TX_RING_COUNT;
	IWM_WRITE(sc, IWM_HBUS_TARG_WRPTR, ring->qid << 8 | ring->cur);

	if (!async) {
		/* m..m-mmyy-mmyyyy-mym-ym m-my generation */
		int generation = sc->sc_generation;
		error = msleep(desc, &sc->sc_mtx, PCATCH, "iwmcmd", hz);
		if (error == 0) {
			/* if hardware is no longer up, return error */
			if (generation != sc->sc_generation) {
				error = ENXIO;
			} else {
				hcmd->resp_pkt = (void *)sc->sc_cmd_resp;
			}
		}
	}
 out:
	if (cmd && paylen > sizeof(cmd->data))
		bus_dmamem_free(ring->data_dmat, cmd, data->map);
	if (wantresp && error != 0) {
		iwm_free_resp(sc, hcmd);
	}

	return error;
}

/* iwlwifi: mvm/utils.c */
int
iwm_mvm_send_cmd_pdu(struct iwm_softc *sc, uint8_t id,
	uint32_t flags, uint16_t len, const void *data)
{
	struct iwm_host_cmd cmd = {
		.id = id,
		.len = { len, },
		.data = { data, },
		.flags = flags,
	};

	return iwm_send_cmd(sc, &cmd);
}

/* iwlwifi: mvm/utils.c */
int
iwm_mvm_send_cmd_status(struct iwm_softc *sc,
	struct iwm_host_cmd *cmd, uint32_t *status)
{
	struct iwm_rx_packet *pkt;
	struct iwm_cmd_response *resp;
	int error, resp_len;

	KASSERT((cmd->flags & IWM_CMD_WANT_SKB) == 0,
	    ("invalid command"));
	cmd->flags |= IWM_CMD_SYNC | IWM_CMD_WANT_SKB;

	if ((error = iwm_send_cmd(sc, cmd)) != 0)
		return error;
	pkt = cmd->resp_pkt;

	/* Can happen if RFKILL is asserted */
	if (!pkt) {
		error = 0;
		goto out_free_resp;
	}

	if (pkt->hdr.flags & IWM_CMD_FAILED_MSK) {
		error = EIO;
		goto out_free_resp;
	}

	resp_len = iwm_rx_packet_payload_len(pkt);
	if (resp_len != sizeof(*resp)) {
		error = EIO;
		goto out_free_resp;
	}

	resp = (void *)pkt->data;
	*status = le32toh(resp->status);
 out_free_resp:
	iwm_free_resp(sc, cmd);
	return error;
}

/* iwlwifi/mvm/utils.c */
int
iwm_mvm_send_cmd_pdu_status(struct iwm_softc *sc, uint8_t id,
	uint16_t len, const void *data, uint32_t *status)
{
	struct iwm_host_cmd cmd = {
		.id = id,
		.len = { len, },
		.data = { data, },
	};

	return iwm_mvm_send_cmd_status(sc, &cmd, status);
}

void
iwm_free_resp(struct iwm_softc *sc, struct iwm_host_cmd *hcmd)
{
	KASSERT(sc->sc_wantresp != -1, ("already freed"));
	KASSERT((hcmd->flags & (IWM_CMD_WANT_SKB|IWM_CMD_SYNC))
	    == (IWM_CMD_WANT_SKB|IWM_CMD_SYNC), ("invalid flags"));
	sc->sc_wantresp = -1;
	wakeup(&sc->sc_wantresp);
}
