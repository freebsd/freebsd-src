/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2023 Dmitry Salychev
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include "dpaa2_types.h"

#define COMPARE_TYPE(t, v)		(strncmp((v), (t), strlen((v))) == 0)

/**
 * @brief Convert DPAA2 device type to string.
 */
const char *
dpaa2_ttos(enum dpaa2_dev_type type)
{
	switch (type) {
	case DPAA2_DEV_MC:
		return ("mc"); /* NOTE: to print as information only. */
	case DPAA2_DEV_RC:
		return ("dprc");
	case DPAA2_DEV_IO:
		return ("dpio");
	case DPAA2_DEV_NI:
		return ("dpni");
	case DPAA2_DEV_MCP:
		return ("dpmcp");
	case DPAA2_DEV_BP:
		return ("dpbp");
	case DPAA2_DEV_CON:
		return ("dpcon");
	case DPAA2_DEV_MAC:
		return ("dpmac");
	case DPAA2_DEV_MUX:
		return ("dpdmux");
	case DPAA2_DEV_SW:
		return ("dpsw");
	default:
		break;
	}

	return ("notype");
}

/**
 * @brief Convert string to DPAA2 device type.
 */
enum dpaa2_dev_type
dpaa2_stot(const char *str)
{
	if (COMPARE_TYPE(str, "dprc")) {
		return (DPAA2_DEV_RC);
	} else if (COMPARE_TYPE(str, "dpio")) {
		return (DPAA2_DEV_IO);
	} else if (COMPARE_TYPE(str, "dpni")) {
		return (DPAA2_DEV_NI);
	} else if (COMPARE_TYPE(str, "dpmcp")) {
		return (DPAA2_DEV_MCP);
	} else if (COMPARE_TYPE(str, "dpbp")) {
		return (DPAA2_DEV_BP);
	} else if (COMPARE_TYPE(str, "dpcon")) {
		return (DPAA2_DEV_CON);
	} else if (COMPARE_TYPE(str, "dpmac")) {
		return (DPAA2_DEV_MAC);
	} else if (COMPARE_TYPE(str, "dpdmux")) {
		return (DPAA2_DEV_MUX);
	} else if (COMPARE_TYPE(str, "dpsw")) {
		return (DPAA2_DEV_SW);
	}

	return (DPAA2_DEV_NOTYPE);
}

/**
 * @brief Callback to obtain a physical address of the only DMA segment mapped.
 */
void
dpaa2_dmamap_oneseg_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error == 0) {
		KASSERT(nseg == 1, ("%s: too many segments: nseg=%d\n",
		    __func__, nseg));
		*(bus_addr_t *)arg = segs[0].ds_addr;
	} else {
		panic("%s: error=%d\n", __func__, error);
	}
}
