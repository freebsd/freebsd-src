/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2022 Dmitry Salychev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_DPAA2_BP_H
#define	_DPAA2_BP_H

#include <sys/bus.h>

/* Maximum resources per DPBP: 1 DPMCP. */
#define DPAA2_BP_MAX_RESOURCES	1

/**
 * @brief Attributes of the DPBP object.
 *
 * id:		 DPBP object ID.
 * bpid:	 Hardware buffer pool ID; should be used as an argument in
 *		 acquire/release operations on buffers.
 */
struct dpaa2_bp_attr {
	uint32_t		 id;
	uint16_t		 bpid;
};

/**
 * @brief Configuration/state of the buffer pool.
 */
struct dpaa2_bp_conf {
	uint8_t			 bdi;
	uint8_t			 state; /* bitmask */
	uint32_t		 free_bufn;
};

/**
 * @brief Software context for the DPAA2 Buffer Pool driver.
 */
struct dpaa2_bp_softc {
	device_t		 dev;
	struct dpaa2_bp_attr	 attr;
	struct resource 	*res[DPAA2_BP_MAX_RESOURCES];
};

extern struct resource_spec dpaa2_bp_spec[];

#endif /* _DPAA2_BP_H */
