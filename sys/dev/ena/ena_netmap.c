/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef DEV_NETMAP

#include "ena.h"
#include "ena_netmap.h"

static int ena_netmap_reg(struct netmap_adapter *, int);
static int ena_netmap_txsync(struct netmap_kring *, int);
static int ena_netmap_rxsync(struct netmap_kring *, int);

int
ena_netmap_attach(struct ena_adapter *adapter)
{
	struct netmap_adapter na;

	ena_trace(ENA_NETMAP, "netmap attach\n");

	bzero(&na, sizeof(na));
	na.na_flags = NAF_MOREFRAG;
	na.ifp = adapter->ifp;
	na.num_tx_desc = adapter->tx_ring_size;
	na.num_rx_desc = adapter->rx_ring_size;
	na.num_tx_rings = adapter->num_queues;
	na.num_rx_rings = adapter->num_queues;
	na.rx_buf_maxsize = adapter->buf_ring_size;
	na.nm_txsync = ena_netmap_txsync;
	na.nm_rxsync = ena_netmap_rxsync;
	na.nm_register = ena_netmap_reg;

	return (netmap_attach(&na));
}

static int
ena_netmap_reg(struct netmap_adapter *na, int onoff)
{
	struct ifnet *ifp = na->ifp;
	struct ena_adapter* adapter = ifp->if_softc;
	int rc;

	sx_xlock(&adapter->ioctl_sx);
	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
	ena_down(adapter);

	if (onoff) {
		ena_trace(ENA_NETMAP, "netmap on\n");
		nm_set_native_flags(na);
	} else {
		ena_trace(ENA_NETMAP, "netmap off\n");
		nm_clear_native_flags(na);
	}

	rc = ena_up(adapter);
	if (rc != 0) {
		ena_trace(ENA_WARNING, "ena_up failed with rc=%d\n", rc);
		adapter->reset_reason = ENA_REGS_RESET_DRIVER_INVALID_STATE;
		nm_clear_native_flags(na);
		ena_destroy_device(adapter, false);
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
		rc = ena_restore_device(adapter);
	}
	sx_unlock(&adapter->ioctl_sx);

	return (rc);
}

static int
ena_netmap_txsync(struct netmap_kring *kring, int flags)
{
	ena_trace(ENA_NETMAP, "netmap txsync\n");
	return (0);
}

static int
ena_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	ena_trace(ENA_NETMAP, "netmap rxsync\n");
	return (0);
}

#endif /* DEV_NETMAP */
