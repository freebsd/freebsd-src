/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * PIO backend for Apple SMC (pre-T2 Macs).
 *
 * The SMC is reached through three I/O ports: a data port, a command port
 * carrying the status nibble, and an interrupt source port.  Every
 * transaction is a command byte, a key name, a length, and then the value,
 * with a status handshake between each step.
 */

#include "opt_asmc.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/asmc/asmcvar.h>
#include <dev/asmc/asmc_pio.h>

/*
 * We need to make sure that the SMC acks the byte sent.
 * Just wait up to (amount * 10)  ms.
 */
static int
asmc_wait_ack(device_t dev, uint8_t val, int amount)
{
	struct asmc_softc *sc = device_get_softc(dev);

	val = val & ASMC_STATUS_MASK;

	for (u_int i = 0; i < amount; i++) {
		if ((ASMC_CMDPORT_READ(sc) & ASMC_STATUS_MASK) == val)
			return (0);
		DELAY(10);
	}

	return (1);
}

/*
 * We need to make sure that the SMC acks the byte sent.
 * Just wait up to 100 ms.
 */
static int
asmc_wait(device_t dev, uint8_t val)
{
#ifdef ASMC_DEBUG
	struct asmc_softc *sc;
#endif

	if (asmc_wait_ack(dev, val, 1000) == 0)
		return (0);

#ifdef ASMC_DEBUG
	sc = device_get_softc(dev);

	device_printf(dev, "%s failed: 0x%x, 0x%x\n", __func__,
	    val & ASMC_STATUS_MASK, ASMC_CMDPORT_READ(sc));
#endif
	return (1);
}

/*
 * Send the given command, retrying up to 10 times if
 * the acknowledgement fails.
 */
static int
asmc_command(device_t dev, uint8_t command)
{
	struct asmc_softc *sc = device_get_softc(dev);

	for (int i = 0; i < 10; i++) {
		ASMC_CMDPORT_WRITE(sc, command);
		if (asmc_wait_ack(dev, 0x0c, 100) == 0) {
			return (0);
		}
	}

#ifdef ASMC_DEBUG
	device_printf(dev, "%s failed: 0x%x, 0x%x\n", __func__, command,
	    ASMC_CMDPORT_READ(sc));
#endif
	return (1);
}

/* Push len bytes out the data port, awaiting status after each. */
static int
asmc_pio_send(device_t dev, const uint8_t *buf, int len, uint8_t status)
{
	struct asmc_softc *sc = device_get_softc(dev);

	for (int i = 0; i < len; i++) {
		ASMC_DATAPORT_WRITE(sc, buf[i]);
		if (asmc_wait(dev, status) != 0)
			return (1);
	}

	return (0);
}

/* Await status then pull each of len bytes from the data port. */
static int
asmc_pio_recv(device_t dev, uint8_t *buf, int len, uint8_t status)
{
	struct asmc_softc *sc = device_get_softc(dev);

	for (int i = 0; i < len; i++) {
		if (asmc_wait(dev, status) != 0)
			return (1);
		buf[i] = ASMC_DATAPORT_READ(sc);
	}

	return (0);
}

/* Fetch the name of the index'th key; the caller holds sc_mtx. */
static int
asmc_pio_getbyindex_locked(device_t dev, int index, uint8_t *key)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t index_buf[ASMC_KEYLEN];

	index_buf[0] = (index >> 24) & 0xff;
	index_buf[1] = (index >> 16) & 0xff;
	index_buf[2] = (index >> 8) & 0xff;
	index_buf[3] = index & 0xff;

	if (asmc_command(dev, ASMC_CMDGETBYINDEX) != 0)
		return (1);

	if (asmc_pio_send(dev, index_buf, ASMC_KEYLEN,
	    ASMC_STATUS_AWAIT_DATA) != 0)
		return (1);

	ASMC_DATAPORT_WRITE(sc, ASMC_KEYLEN);

	return (asmc_pio_recv(dev, key, ASMC_KEYLEN, ASMC_STATUS_DATA_READY));
}

/* Fetch the raw key info response; the caller holds sc_mtx. */
static int
asmc_pio_getinfo_locked(device_t dev, const uint8_t *key, uint8_t *info)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (asmc_command(dev, ASMC_CMDGETINFO) != 0)
		return (1);

	if (asmc_pio_send(dev, key, ASMC_KEYLEN, ASMC_STATUS_AWAIT_DATA) != 0)
		return (1);

	ASMC_DATAPORT_WRITE(sc, ASMC_KEYINFO_RESPLEN);

	return (asmc_pio_recv(dev, info, ASMC_KEYINFO_RESPLEN,
	    ASMC_STATUS_DATA_READY));
}

/* Split a key info response; type needs ASMC_TYPELEN + 1 bytes. */
static void
asmc_keyinfo_decode(const uint8_t *info, uint8_t *len, char *type)
{
	if (len != NULL)
		*len = info[0];
	if (type != NULL) {
		memcpy(type, &info[1], ASMC_TYPELEN);
		type[ASMC_TYPELEN] = '\0';
	}
}

int
asmc_pio_key_read(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int error = 1, try = 0;

	mtx_lock_spin(&sc->sc_mtx);

begin:
	if (asmc_command(dev, ASMC_CMDREAD) != 0)
		goto out;

	if (asmc_pio_send(dev, (const uint8_t *)key, ASMC_KEYLEN,
	    ASMC_STATUS_AWAIT_DATA) != 0)
		goto out;

	ASMC_DATAPORT_WRITE(sc, len);

	if (asmc_pio_recv(dev, buf, len, ASMC_STATUS_DATA_READY) != 0)
		goto out;

	error = 0;
out:
	if (error != 0) {
		if (++try < 10)
			goto begin;
		device_printf(dev, "%s for key %s failed %d times, giving up\n",
		    __func__, key, try);
	}

	mtx_unlock_spin(&sc->sc_mtx);

	return (error);
}

/*
 * Get key info (length and type) from SMC using command 0x13.
 * If len is non-NULL, stores the key's value length.
 * If type is non-NULL, stores the 4-char type string; it needs 5 bytes.
 */
int
asmc_pio_key_getinfo(device_t dev, const char *key, uint8_t *len, char *type)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t info[ASMC_KEYINFO_RESPLEN];
	int error = -1, try = 0;

	mtx_lock_spin(&sc->sc_mtx);

	do {
		if (asmc_pio_getinfo_locked(dev, (const uint8_t *)key,
		    info) == 0) {
			error = 0;
			break;
		}
	} while (++try < ASMC_MAXRETRIES);

	mtx_unlock_spin(&sc->sc_mtx);

	if (error == 0)
		asmc_keyinfo_decode(info, len, type);

	return (error);
}

/*
 * Helper function to get key info by index (for sensor detection).
 */
int
asmc_pio_key_dump_by_index(device_t dev, int index, char *key_out,
    char *type_out, uint8_t *len_out)
{
	struct asmc_softc *sc = device_get_softc(dev);
	uint8_t key_buf[ASMC_KEYLEN];
	uint8_t info_buf[ASMC_KEYINFO_RESPLEN];
	int error = ENXIO, try = 0;

	mtx_lock_spin(&sc->sc_mtx);

	do {
		if (asmc_pio_getbyindex_locked(dev, index, key_buf) == 0 &&
		    asmc_pio_getinfo_locked(dev, key_buf, info_buf) == 0) {
			error = 0;
			break;
		}
	} while (++try < ASMC_MAXRETRIES);

	mtx_unlock_spin(&sc->sc_mtx);

	if (error != 0)
		return (error);

	memcpy(key_out, key_buf, ASMC_KEYLEN);
	key_out[ASMC_KEYLEN] = '\0';
	asmc_keyinfo_decode(info_buf, len_out, type_out);

	return (0);
}

int
asmc_pio_key_write(device_t dev, const char *key, uint8_t *buf, uint8_t len)
{
	struct asmc_softc *sc = device_get_softc(dev);
	int error = -1, try = 0;

	mtx_lock_spin(&sc->sc_mtx);

begin:
	ASMC_DPRINTF(("cmd port: cmd write\n"));
	if (asmc_command(dev, ASMC_CMDWRITE) != 0)
		goto out;

	ASMC_DPRINTF(("data port: key\n"));
	if (asmc_pio_send(dev, (const uint8_t *)key, ASMC_KEYLEN,
	    ASMC_STATUS_AWAIT_DATA) != 0)
		goto out;

	ASMC_DPRINTF(("data port: length\n"));
	ASMC_DATAPORT_WRITE(sc, len);

	ASMC_DPRINTF(("data port: buffer\n"));
	for (int i = 0; i < len; i++) {
		if (asmc_wait(dev, ASMC_STATUS_AWAIT_DATA) != 0)
			goto out;
		ASMC_DATAPORT_WRITE(sc, buf[i]);
	}

	error = 0;
out:
	if (error != 0) {
		if (++try < 10)
			goto begin;
		device_printf(dev, "%s for key %s failed %d times, giving up\n",
		    __func__, key, try);
	}

	mtx_unlock_spin(&sc->sc_mtx);

	return (error);
}
