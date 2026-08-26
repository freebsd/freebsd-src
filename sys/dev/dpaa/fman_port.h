/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef DPAA_FMAN_PORT_H
#define	DPAA_FMAN_PORT_H

#define	FMAN_PORT_MAX_POOLS	4
struct fman_port_buffer_pool {
	uint8_t bpid;
	uint16_t size;
};

struct fman_port_params {
	uint32_t dflt_fqid;	/* Must not be 0 */
	uint32_t err_fqid;
	union {
		struct {
			int num_pools;
			struct fman_port_buffer_pool bpools[FMAN_PORT_MAX_POOLS];
		} rx_params;
		struct {
		} tx_params;
	};
};

int fman_port_get_id(device_t dev);

/*
 * Runtime toggle for the RX port's parser->KeyGen routing.  Default
 * (RX-init time) is parser->BMI-enqueue with no KG involvement; a
 * consumer flips this to true only *after* successfully binding a
 * KG scheme via fman_kg_alloc_hash_scheme().  Routing to KG with no
 * bound scheme drops the port's Rx traffic on the floor.  Only valid
 * on FMAN_PORT_TYPE_RX devices; no-op / undefined on TX/OH.
 */
void fman_port_rx_use_kg(device_t rx_port, bool enable);

#endif
