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

#include "opt_wlan.h"

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

#include <dev/iwm/if_iwmreg.h>
#include <dev/iwm/if_iwmvar.h>
#include <dev/iwm/if_iwm_debug.h>
#include <dev/iwm/if_iwm_util.h>
#include <dev/iwm/if_iwm_pcie_trans.h>
#include <dev/iwm/if_iwm_time_event.h>

/*
 * For the high priority TE use a time event type that has similar priority to
 * the FW's action scan priority.
 */
#define IWM_MVM_ROC_TE_TYPE_NORMAL IWM_TE_P2P_DEVICE_DISCOVERABLE
#define IWM_MVM_ROC_TE_TYPE_MGMT_TX IWM_TE_P2P_CLIENT_ASSOC

/* used to convert from time event API v2 to v1 */
#define IWM_TE_V2_DEP_POLICY_MSK (IWM_TE_V2_DEP_OTHER | IWM_TE_V2_DEP_TSF |\
			     IWM_TE_V2_EVENT_SOCIOPATHIC)
static inline uint16_t
iwm_te_v2_get_notify(uint16_t policy)
{
	return le16toh(policy) & IWM_TE_V2_NOTIF_MSK;
}

static inline uint16_t
iwm_te_v2_get_dep_policy(uint16_t policy)
{
	return (le16toh(policy) & IWM_TE_V2_DEP_POLICY_MSK) >>
		IWM_TE_V2_PLACEMENT_POS;
}

static inline uint16_t
iwm_te_v2_get_absence(uint16_t policy)
{
	return (le16toh(policy) & IWM_TE_V2_ABSENCE) >> IWM_TE_V2_ABSENCE_POS;
}

static void
iwm_mvm_te_v2_to_v1(const struct iwm_time_event_cmd_v2 *cmd_v2,
	struct iwm_time_event_cmd_v1 *cmd_v1)
{
	cmd_v1->id_and_color = cmd_v2->id_and_color;
	cmd_v1->action = cmd_v2->action;
	cmd_v1->id = cmd_v2->id;
	cmd_v1->apply_time = cmd_v2->apply_time;
	cmd_v1->max_delay = cmd_v2->max_delay;
	cmd_v1->depends_on = cmd_v2->depends_on;
	cmd_v1->interval = cmd_v2->interval;
	cmd_v1->duration = cmd_v2->duration;
	if (cmd_v2->repeat == IWM_TE_V2_REPEAT_ENDLESS)
		cmd_v1->repeat = htole32(IWM_TE_V1_REPEAT_ENDLESS);
	else
		cmd_v1->repeat = htole32(cmd_v2->repeat);
	cmd_v1->max_frags = htole32(cmd_v2->max_frags);
	cmd_v1->interval_reciprocal = 0; /* unused */

	cmd_v1->dep_policy = htole32(iwm_te_v2_get_dep_policy(cmd_v2->policy));
	cmd_v1->is_present = htole32(!iwm_te_v2_get_absence(cmd_v2->policy));
	cmd_v1->notify = htole32(iwm_te_v2_get_notify(cmd_v2->policy));
}

static int
iwm_mvm_send_time_event_cmd(struct iwm_softc *sc,
	const struct iwm_time_event_cmd_v2 *cmd)
{
	struct iwm_time_event_cmd_v1 cmd_v1;

	if (sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_TIME_EVENT_API_V2)
		return iwm_mvm_send_cmd_pdu(sc, IWM_TIME_EVENT_CMD,
		    IWM_CMD_SYNC, sizeof(*cmd), cmd);

	iwm_mvm_te_v2_to_v1(cmd, &cmd_v1);
	return iwm_mvm_send_cmd_pdu(sc, IWM_TIME_EVENT_CMD, IWM_CMD_SYNC,
	    sizeof(cmd_v1), &cmd_v1);
}

static int
iwm_mvm_time_event_send_add(struct iwm_softc *sc, struct iwm_node *in,
	void *te_data, struct iwm_time_event_cmd_v2 *te_cmd)
{
	int ret;

	IWM_DPRINTF(sc, IWM_DEBUG_CMD | IWM_DEBUG_RESET,
	    "Add new TE, duration %d TU\n", le32toh(te_cmd->duration));

	ret = iwm_mvm_send_time_event_cmd(sc, te_cmd);
	if (ret) {
		IWM_DPRINTF(sc, IWM_DEBUG_CMD | IWM_DEBUG_RESET,
		    "%s: Couldn't send IWM_TIME_EVENT_CMD: %d\n",
		    __func__, ret);
	}

	return ret;
}

void
iwm_mvm_protect_session(struct iwm_softc *sc, struct iwm_node *in,
	uint32_t duration, uint32_t max_delay)
{
	struct iwm_time_event_cmd_v2 time_cmd;

	memset(&time_cmd, 0, sizeof(time_cmd));

	time_cmd.action = htole32(IWM_FW_CTXT_ACTION_ADD);
	time_cmd.id_and_color =
	    htole32(IWM_FW_CMD_ID_AND_COLOR(IWM_DEFAULT_MACID, IWM_DEFAULT_COLOR));
	time_cmd.id = htole32(IWM_TE_BSS_STA_AGGRESSIVE_ASSOC);

	time_cmd.apply_time = htole32(0);

	time_cmd.max_frags = IWM_TE_V2_FRAG_NONE;
	time_cmd.max_delay = htole32(max_delay);
	/* TODO: why do we need to interval = bi if it is not periodic? */
	time_cmd.interval = htole32(1);
	time_cmd.duration = htole32(duration);
	time_cmd.repeat = 1;
	time_cmd.policy
	    = htole16(IWM_TE_V2_NOTIF_HOST_EVENT_START |
	        IWM_TE_V2_NOTIF_HOST_EVENT_END |
		IWM_T2_V2_START_IMMEDIATELY);

	iwm_mvm_time_event_send_add(sc, in, /*te_data*/NULL, &time_cmd);
}
