/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MMIO backend for Apple SMC (T2 and later Macs).
 *
 * T2 Macs expose the SMC via memory-mapped registers instead of I/O ports.
 * Protocol: clear status, write key/cmd, poll for ready, read result.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/asmc/asmcvar.h>
#include <dev/asmc/asmcmmio.h>

/*
 * Wait for MMIO status register bit 5 (ready) with exponential backoff.
 * Caller must hold sc_mtx.
 */
static int
asmc_mmio_wait(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int i;
	uint8_t status;
	int delay_us = 10;

	for (i = 0; i < ASMC_MMIO_MAX_WAIT; i++) {
		status = bus_read_1(sc->sc_iomem, ASMC_MMIO_STATUS);
		if (status & ASMC_MMIO_STATUS_READY)
			return (0);
		DELAY(delay_us);
		if (delay_us < 3200)
			delay_us *= 2;
	}

	return (ETIMEDOUT);
}

int
asmc_mmio_key_read(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint32_t key_int;
	int error, i;
	uint8_t cmd_result, rlen;

	if (len > ASMC_MAXVAL)
		return (EINVAL);

	mtx_lock_spin(&sc->sc_mtx);

	/* Clear status if non-zero */
	if (bus_read_1(sc->sc_iomem, ASMC_MMIO_STATUS))
		bus_write_1(sc->sc_iomem, ASMC_MMIO_STATUS, 0);

	/* Write key name as raw 4 bytes */
	memcpy(&key_int, key, 4);
	bus_write_4(sc->sc_iomem, ASMC_MMIO_KEY_NAME, key_int);

	/* Write SMC ID (always 0) and command */
	bus_write_1(sc->sc_iomem, ASMC_MMIO_SMC_ID, 0);
	bus_write_1(sc->sc_iomem, ASMC_MMIO_CMD, ASMC_CMDREAD);

	/* Wait for ready */
	error = asmc_mmio_wait(dev);
	if (error != 0) {
		uint8_t st = bus_read_1(sc->sc_iomem, ASMC_MMIO_STATUS);
		uint8_t cm = bus_read_1(sc->sc_iomem, ASMC_MMIO_CMD);
		mtx_unlock_spin(&sc->sc_mtx);
		device_printf(dev,
		    "%s: timeout key %.4s status=0x%02x cmd=0x%02x\n",
		    __func__, key, st, cm);
		return (error);
	}

	/* Check command result (0 = success, 0x84 = key not found) */
	cmd_result = bus_read_1(sc->sc_iomem, ASMC_MMIO_CMD);
	if (cmd_result != 0) {
		mtx_unlock_spin(&sc->sc_mtx);
		device_printf(dev,
		    "%s: key %.4s cmd error 0x%02x\n",
		    __func__, key, cmd_result);
		return (EIO);
	}

	/* Read data length and data bytes; zero-fill remainder */
	rlen = bus_read_1(sc->sc_iomem, ASMC_MMIO_DATA_LEN);
	rlen = MIN(rlen, len);
	for (i = rlen; i < len; i++)
		buf[i] = 0;
	for (i = 0; i < rlen; i++)
		buf[i] = bus_read_1(sc->sc_iomem, ASMC_MMIO_DATA + i);

	mtx_unlock_spin(&sc->sc_mtx);
	return (0);
}

int
asmc_mmio_key_write(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint32_t key_int;
	int error, i;
	uint8_t cmd_result;

	if (len > ASMC_MAXVAL)
		return (EINVAL);

	mtx_lock_spin(&sc->sc_mtx);

	/* Clear status */
	bus_write_1(sc->sc_iomem, ASMC_MMIO_STATUS, 0);

	/* Write data bytes first */
	for (i = 0; i < len; i++)
		bus_write_1(sc->sc_iomem, ASMC_MMIO_DATA + i, buf[i]);

	/* Write key name as raw 4 bytes */
	memcpy(&key_int, key, 4);
	bus_write_4(sc->sc_iomem, ASMC_MMIO_KEY_NAME, key_int);

	/* Write length, SMC ID, command */
	bus_write_1(sc->sc_iomem, ASMC_MMIO_DATA_LEN, len);
	bus_write_1(sc->sc_iomem, ASMC_MMIO_SMC_ID, 0);
	bus_write_1(sc->sc_iomem, ASMC_MMIO_CMD, ASMC_CMDWRITE);

	/* Wait for ready */
	error = asmc_mmio_wait(dev);
	if (error != 0) {
		mtx_unlock_spin(&sc->sc_mtx);
		device_printf(dev, "%s: timeout writing key %.4s\n",
		    __func__, key);
		return (error);
	}

	cmd_result = bus_read_1(sc->sc_iomem, ASMC_MMIO_CMD);
	mtx_unlock_spin(&sc->sc_mtx);

	return (cmd_result == 0 ? 0 : EIO);
}

int
asmc_mmio_key_getinfo(device_t dev, const char *key, uint8_t *len, char *type)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint32_t key_int;
	int error, i;
	uint8_t cmd_result;

	mtx_lock_spin(&sc->sc_mtx);

	/* Clear status */
	bus_write_1(sc->sc_iomem, ASMC_MMIO_STATUS, 0);

	/* Write key name as raw 4 bytes */
	memcpy(&key_int, key, 4);
	bus_write_4(sc->sc_iomem, ASMC_MMIO_KEY_NAME, key_int);

	bus_write_1(sc->sc_iomem, ASMC_MMIO_SMC_ID, 0);
	bus_write_1(sc->sc_iomem, ASMC_MMIO_CMD, ASMC_CMDGETINFO);

	error = asmc_mmio_wait(dev);
	if (error != 0) {
		mtx_unlock_spin(&sc->sc_mtx);
		return (error);
	}

	cmd_result = bus_read_1(sc->sc_iomem, ASMC_MMIO_CMD);
	if (cmd_result != 0) {
		mtx_unlock_spin(&sc->sc_mtx);
		return (EIO);
	}

	/*
	 * GETINFO response layout (MMIO):
	 *   data[0..3] = type code (4 chars)
	 *   data[4]    = reserved
	 *   data[5]    = data length
	 *   data[6]    = flags/attributes
	 */
	if (type != NULL) {
		for (i = 0; i < ASMC_TYPELEN; i++)
			type[i] = bus_read_1(sc->sc_iomem,
			    ASMC_MMIO_DATA + i);
		type[ASMC_TYPELEN] = '\0';
	}
	if (len != NULL)
		*len = bus_read_1(sc->sc_iomem, ASMC_MMIO_DATA + 5);

	mtx_unlock_spin(&sc->sc_mtx);
	return (0);
}

int
asmc_mmio_key_getbyindex(device_t dev, int index, char *key)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint32_t idx_val;
	int error, i;
	uint8_t cmd_result;

	mtx_lock_spin(&sc->sc_mtx);

	bus_write_1(sc->sc_iomem, ASMC_MMIO_STATUS, 0);

	/* Write index as big-endian 4 bytes to key name register */
	idx_val = htobe32(index);
	bus_write_4(sc->sc_iomem, ASMC_MMIO_KEY_NAME, idx_val);

	bus_write_1(sc->sc_iomem, ASMC_MMIO_SMC_ID, 0);
	bus_write_1(sc->sc_iomem, ASMC_MMIO_CMD, ASMC_CMDGETBYINDEX);

	error = asmc_mmio_wait(dev);
	if (error != 0) {
		mtx_unlock_spin(&sc->sc_mtx);
		return (error);
	}

	cmd_result = bus_read_1(sc->sc_iomem, ASMC_MMIO_CMD);
	if (cmd_result != 0) {
		mtx_unlock_spin(&sc->sc_mtx);
		return (EIO);
	}

	/* Result: 4-byte key name in DATA */
	for (i = 0; i < ASMC_KEYLEN; i++)
		key[i] = bus_read_1(sc->sc_iomem, ASMC_MMIO_DATA + i);
	key[ASMC_KEYLEN] = '\0';

	mtx_unlock_spin(&sc->sc_mtx);
	return (0);
}

/*
 * Validate MMIO and detect T2.
 * Check that status register is accessible and LDKN firmware version >= 2.
 */
int
asmc_mmio_probe(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);
	rman_res_t size;
	uint8_t status, ldkn;
	int error;

	size = rman_get_size(sc->sc_iomem);
	if (size < ASMC_MMIO_MIN_SIZE) {
		device_printf(dev, "MMIO region too small (%jd < %d)\n",
		    (intmax_t)size, ASMC_MMIO_MIN_SIZE);
		return (ENXIO);
	}

	/* Check status register isn't stuck at 0xFF */
	status = bus_read_1(sc->sc_iomem, ASMC_MMIO_STATUS);
	if (status == 0xFF) {
		device_printf(dev, "MMIO status register reads 0xFF\n");
		return (ENXIO);
	}

	/*
	 * We need the mutex initialized before calling mmio_key_read,
	 * but attach hasn't done it yet. Initialize early.
	 */
	if (!mtx_initialized(&sc->sc_mtx))
		mtx_init(&sc->sc_mtx, "asmc", NULL, MTX_SPIN);

	/* Read LDKN (firmware version) -- must be >= 2 for MMIO */
	error = asmc_mmio_key_read(dev, ASMC_KEY_LDKN, &ldkn, 1);
	if (error != 0) {
		device_printf(dev, "MMIO: failed to read LDKN key\n");
		return (ENXIO);
	}

	if (ldkn < 2) {
		device_printf(dev, "MMIO: LDKN=%d (need >= 2)\n", ldkn);
		return (ENXIO);
	}

	device_printf(dev, "MMIO: LDKN=%d, T2 SMC detected\n", ldkn);
	sc->sc_is_t2 = 1;

	return (0);
}

void
asmc_mmio_detach(device_t dev, struct asmc_softc *sc)
{

	if (sc->sc_iomem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rid_mem,
		    sc->sc_iomem);
		sc->sc_iomem = NULL;
	}
	sc->sc_is_mmio = false;
	sc->sc_is_t2 = 0;
}

/*
 * Convert IEEE 754 float (as u32) to unsigned integer.
 * Kernel soft-float: extract integer part only.
 * Used for T2 fan RPM values (always positive, reasonable range).
 */
uint32_t
asmc_float_to_u32(uint32_t d)
{
	int32_t exp;
	uint32_t fr;

	/* Negative or zero */
	if (d == 0 || (d >> 31) != 0)
		return (0);

	exp = (int32_t)((d >> 23) & 0xff) - 0x7f;
	fr = d & 0x7fffff;	/* 23-bit mantissa */

	if (exp < 0)
		return (0);
	if (exp > 23) {
		if (exp > 30)
			return (0xffffffffu);
		return ((1u << exp) | (fr << (exp - 23)));
	}
	/* Normal case: 0 <= exp <= 23 */
	return ((1u << exp) + (fr >> (23 - exp)));
}

/*
 * Convert unsigned integer to IEEE 754 float (as u32).
 * Only handles values in fan RPM range (0-65535).
 */
uint32_t
asmc_u32_to_float(uint32_t d)
{
	uint32_t dc, bc, exp;

	if (d == 0)
		return (0);

	/* Find highest set bit position */
	dc = d;
	bc = 0;
	while (dc >>= 1)
		++bc;

	bc = MIN(bc, 30);

	exp = 0x7f + bc;

	/*
	 * Mantissa: strip the implicit leading 1-bit and place
	 * remaining bits into the 23-bit mantissa field.
	 */
	if (bc >= 23)
		return ((exp << 23) | ((d >> (bc - 23)) & 0x7fffff));
	else
		return ((exp << 23) | ((d << (23 - bc)) & 0x7fffff));
}

/*
 * Battery charge limit sysctl (T2 Macs).
 * BCLM key: 1 byte, 0-100 (percentage).
 */
int
asmc_bclm_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	uint8_t bclm;
	int val, error;

	error = asmc_mmio_key_read(dev, ASMC_KEY_BCLM, &bclm, 1);
	if (error != 0)
		return (EIO);

	val = (int)bclm;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (val < 0 || val > 100)
		return (EINVAL);

	bclm = (uint8_t)val;
	error = asmc_mmio_key_write(dev, ASMC_KEY_BCLM, &bclm, 1);

	return (error != 0 ? EIO : 0);
}
