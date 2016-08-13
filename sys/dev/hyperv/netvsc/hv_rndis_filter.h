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
 * Defines
 */

/* Destroy or preserve channel on filter/netvsc teardown */
#define HV_RF_NV_DESTROY_CHANNEL	TRUE
#define HV_RF_NV_RETAIN_CHANNEL		FALSE

/*
 * Number of page buffers to reserve for the RNDIS filter packet in the
 * transmitted message.
 */
#define HV_RF_NUM_TX_RESERVED_PAGE_BUFS	1


/*
 * Data types
 */

typedef enum {
	RNDIS_DEV_UNINITIALIZED = 0,
	RNDIS_DEV_INITIALIZING,
	RNDIS_DEV_INITIALIZED,
	RNDIS_DEV_DATAINITIALIZED,
} rndis_device_state;

typedef struct rndis_request_ {
	STAILQ_ENTRY(rndis_request_)	mylist_entry;
	struct sema			wait_sema;	

	/*
	 * The max response size is sizeof(rndis_msg) + PAGE_SIZE.
	 *
	 * XXX
	 * This is ugly and should be cleaned up once we busdma-fy
	 * RNDIS request bits.
	 */
	rndis_msg			response_msg;
	uint8_t				buf_resp[PAGE_SIZE];

	/* Simplify allocation by having a netvsc packet inline */
	struct hn_send_ctx		send_ctx;

	/*
	 * The max request size is sizeof(rndis_msg) + PAGE_SIZE.
	 *
	 * NOTE:
	 * This is required for the large request like RSS settings.
	 *
	 * XXX
	 * This is ugly and should be cleaned up once we busdma-fy
	 * RNDIS request bits.
	 */
	rndis_msg			request_msg;
	uint8_t				buf_req[PAGE_SIZE];

	/* Fixme:  Poor man's semaphore. */
	uint32_t			halt_complete_flag;
} rndis_request;

typedef struct rndis_device_ {
	netvsc_dev			*net_dev;

	rndis_device_state		state;
	uint32_t			link_status;
	uint32_t			new_request_id;

	struct mtx			req_lock;

	STAILQ_HEAD(RQ, rndis_request_)	myrequest_list;

	uint8_t				hw_mac_addr[ETHER_ADDR_LEN];
} rndis_device;

/*
 * Externs
 */
struct hn_softc;
struct hn_rx_ring;

int hv_rf_on_receive(netvsc_dev *net_dev,
    struct hn_rx_ring *rxr, netvsc_packet *pkt);
void hv_rf_receive_rollup(netvsc_dev *net_dev);
void hv_rf_channel_rollup(struct hn_rx_ring *rxr, struct hn_tx_ring *txr);
int hv_rf_on_device_add(struct hn_softc *sc, void *additl_info, int nchan,
    struct hn_rx_ring *rxr);
int hv_rf_on_device_remove(struct hn_softc *sc, boolean_t destroy_channel);
int hv_rf_on_open(struct hn_softc *sc);
int hv_rf_on_close(struct hn_softc *sc);

#endif  /* __HV_RNDIS_FILTER_H__ */

