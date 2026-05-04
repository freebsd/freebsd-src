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

#endif
