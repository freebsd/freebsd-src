/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
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
 *
 * $FreeBSD$
 */

#ifndef __HV_RNDIS_FILTER_H__
#define __HV_RNDIS_FILTER_H__

#include <sys/param.h>
#include <net/ethernet.h>
#include <dev/hyperv/netvsc/if_hnvar.h>

/*
 * Externs
 */
struct hn_rx_ring;

void hv_rf_on_receive(struct hn_softc *sc, struct hn_rx_ring *rxr,
    const void *data, int dlen);
void hv_rf_channel_rollup(struct hn_rx_ring *rxr, struct hn_tx_ring *txr);
int hv_rf_on_device_add(struct hn_softc *sc, void *additl_info, int *nchan);
int hv_rf_on_device_remove(struct hn_softc *sc);
int hv_rf_on_open(struct hn_softc *sc);
int hv_rf_on_close(struct hn_softc *sc);

#endif  /* __HV_RNDIS_FILTER_H__ */

