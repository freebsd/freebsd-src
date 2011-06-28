/*-
 * Copyright (c) 2011 Adrian Chadd, Xenion Pty Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This implements an empty DFS module.
 */
#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/errno.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <sys/socket.h>
 
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

#include <dev/ath/if_athvar.h>
#include <dev/ath/if_athdfs.h>

#include <dev/ath/ath_hal/ah_desc.h>

/*
 * Methods which are required
 */

/*
 * Attach DFS to the given interface
 */
int
ath_dfs_attach(struct ath_softc *sc)
{
	return 1;
}

/*
 * Detach DFS from the given interface
 */
int
ath_dfs_detach(struct ath_softc *sc)
{
	return 1;
}

/*
 * Enable radar check
 */
void
ath_dfs_radar_enable(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	/* Check if the current channel is radar-enabled */
	if (! IEEE80211_IS_CHAN_DFS(chan))
		return;
}

/*
 * Process DFS related PHY errors
 */
void
ath_dfs_process_phy_err(struct ath_softc *sc, const char *buf,
    uint64_t tsf, struct ath_rx_status *rxstat)
{

}

/*
 * Process the radar events and determine whether a DFS event has occured.
 *
 * This is designed to run outside of the RX processing path.
 * The RX path will call ath_dfs_tasklet_needed() to see whether
 * the task/callback running this routine needs to be called.
 */
int
ath_dfs_process_radar_event(struct ath_softc *sc,
    struct ieee80211_channel *chan)
{
	return 0;
}

/*
 * Determine whether the the DFS check task needs to be queued.
 *
 * This is called in the RX task when the current batch of packets
 * have been received. It will return whether there are any radar
 * events for ath_dfs_process_radar_event() to handle.
 */
int
ath_dfs_tasklet_needed(struct ath_softc *sc, struct ieee80211_channel *chan)
{
	return 0;
}

/*
 * Handle ioctl requests from the diagnostic interface
 */
int
ath_ioctl_phyerr(struct ath_softc *sc, struct ath_diag *ad)
{
	return 1;
}

/*
 * Get the current DFS thresholds from the HAL
 */
int
ath_dfs_get_thresholds(struct ath_softc *sc, HAL_PHYERR_PARAM *param)
{
	ath_hal_getdfsthresh(sc->sc_ah, param);
	return 1;
}
