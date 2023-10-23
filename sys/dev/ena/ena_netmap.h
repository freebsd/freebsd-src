/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2023 Amazon.com, Inc. or its affiliates.
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
 *
 */

#ifndef _ENA_NETMAP_H_
#define _ENA_NETMAP_H_

/* Undef (un)likely as they are defined in netmap_kern.h */
#ifdef likely
#undef likely
#endif /* likely */
#ifdef unlikely
#undef unlikely
#endif /* unlikely */

#include <sys/selinfo.h>

#include <net/netmap.h>

#include <dev/netmap/netmap_kern.h>

int ena_netmap_attach(struct ena_adapter *adapter);
int ena_netmap_alloc_rx_slot(struct ena_adapter *adapter,
    struct ena_ring *rx_ring, struct ena_rx_buffer *rx_info);
void ena_netmap_free_rx_slot(struct ena_adapter *adapter,
    struct ena_ring *rx_ring, struct ena_rx_buffer *rx_info);
bool ena_rx_ring_in_netmap(struct ena_adapter *adapter, int qid);
bool ena_tx_ring_in_netmap(struct ena_adapter *adapter, int qid);
void ena_netmap_reset_rx_ring(struct ena_adapter *adapter, int qid);
void ena_netmap_reset_tx_ring(struct ena_adapter *adapter, int qid);
void ena_netmap_unload(struct ena_adapter *adapter, bus_dmamap_t map);

#endif /* _ENA_NETMAP_H_ */
