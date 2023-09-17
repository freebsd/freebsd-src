/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
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

#ifndef	_DPAA2_CON_H
#define	_DPAA2_CON_H

#include <sys/rman.h>
#include <sys/bus.h>
#include <sys/queue.h>

#include "dpaa2_types.h"
#include "dpaa2_mcp.h"

/* Maximum resources per DPCON: 1 DPMCP. */
#define DPAA2_CON_MAX_RESOURCES	1

/**
 * @brief Attributes of the DPCON object.
 *
 * id:		DPCON object ID.
 * chan_id:	QBMan channel ID to be used for dequeue operations.
 * prior_num:	Number of priorities for the DPCON channel (1-8).
 */
struct dpaa2_con_attr {
	uint32_t		 id;
	uint16_t		 chan_id;
	uint8_t			 prior_num;
};

/**
 * @brief Software context for the DPAA2 Concentrator driver.
 */
struct dpaa2_con_softc {
	device_t		 dev;
	struct resource 	*res[DPAA2_CON_MAX_RESOURCES];
	struct dpaa2_con_attr	 attr;
};

extern struct resource_spec dpaa2_con_spec[];

#endif /* _DPAA2_CON_H */
