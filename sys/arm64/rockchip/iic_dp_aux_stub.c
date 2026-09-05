/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw <b1nc0d3x@gmail.com>
 *
 * Stub for iic_dp_aux_add_bus when drm2 is not in the kernel.
 *
 * rk_cdn_dp calls iic_dp_aux_add_bus to publish an AUX-backed iicbus
 * (used for EDID DDC reads).  drm2 normally provides the
 * implementation in drm_dp_iic_helper.c.  On drmcompat-only kernels
 * (e.g. RP64KERN_DRMCOMPAT) drm2 is removed and rk_cdn_dp's call
 * site needs a symbol to link against.
 *
 * The stub returns ENXIO so rk_cdn_dp's existing graceful-degradation
 * path runs: it logs "AUX-backed iicbus unavailable; continuing
 * without it" and leaves sc->aux_iicbus / sc->aux_iic_adapter NULL.
 * EDID reads then fall back to whatever the drmcompat-side helper
 * eventually wires up (Phase 11+).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/errno.h>

int	iic_dp_aux_add_bus(device_t dev, const char *name,
	    int (*ch)(device_t idev, int mode, uint8_t write_byte,
		uint8_t *read_byte), void *priv, device_t *bus,
	    device_t *adapter);

int
iic_dp_aux_add_bus(device_t dev __unused, const char *name __unused,
    int (*ch)(device_t, int, uint8_t, uint8_t *) __unused,
    void *priv __unused, device_t *bus, device_t *adapter)
{
	if (bus != NULL)
		*bus = NULL;
	if (adapter != NULL)
		*adapter = NULL;
	return (ENXIO);
}
