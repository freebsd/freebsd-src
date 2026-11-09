/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_ASMC_ASMC_PIO_H_
#define _DEV_ASMC_ASMC_PIO_H_

struct asmc_softc;

/*
 * PIO backend functions.
 *
 * These mirror the MMIO entry points, except that getbyindex and getinfo are
 * one call here: the PIO protocol needs both halves inside a single hold of
 * sc_mtx, so the key name cannot be returned separately.
 */
int	asmc_pio_key_read(device_t dev, const char *key,
	    uint8_t *buf, uint8_t len);
int	asmc_pio_key_write(device_t dev, const char *key,
	    uint8_t *buf, uint8_t len);
int	asmc_pio_key_getinfo(device_t dev, const char *key,
	    uint8_t *len, char *type);
int	asmc_pio_key_dump_by_index(device_t dev, int index, char *key,
	    char *type, uint8_t *len);

#endif /* _DEV_ASMC_ASMC_PIO_H_ */
