/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Debug key access for Apple SMC.
 *
 * Compiled only with the ASMC_DEBUG option: the dev.asmc.n.raw sysctls for
 * reading and writing arbitrary keys by name, and the boot-time dump of
 * every key the SMC reports.
 */

#include "opt_asmc.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <dev/asmc/asmcvar.h>
#include <dev/asmc/asmc_debug.h>

static int
asmc_key_dump(device_t dev, int number)
{
	char key[ASMC_KEYLEN + 1] = { 0 };
	char type[ASMC_TYPELEN + 1] = { 0 };
	uint8_t v[ASMC_MAXVAL];
	struct sbuf sb;
	char buf[128];
	uint8_t len = 0;
	int error;

	error = asmc_key_dump_by_index(dev, number, key, type, &len);
	if (error != 0)
		return (error);

	len = MIN(len, sizeof(v));
	memset(v, 0, sizeof(v));
	error = asmc_key_read(dev, key, v, len);
	if (error != 0)
		return (error);

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_printf(&sb, "key %d: %s, type %s (len %d), data",
	    number, key, type, len);
	for (int i = 0; i < len; i++)
		sbuf_printf(&sb, " %02x", v[i]);
	sbuf_finish(&sb);
	device_printf(dev, "%s\n", sbuf_data(&sb));
	sbuf_delete(&sb);

	return (0);
}

void
asmc_dumpall(device_t dev)
{
	struct asmc_softc *sc = device_get_softc(dev);

	if (sc->sc_nkeys == 0) {
		device_printf(dev, "asmc_dumpall: key count not available\n");
		return;
	}

	device_printf(dev, "asmc_dumpall: dumping %d keys\n", sc->sc_nkeys);
	for (int i = 0; i < sc->sc_nkeys; i++)
		asmc_key_dump(dev, i);
}

/*
 * Raw SMC key access sysctls - enables reading/writing any SMC key by name
 * Usage:
 *   sysctl dev.asmc.0.raw.key=TC0P   # Set key, auto-detects length
 *   sysctl dev.asmc.0.raw.value      # Read current value (hex bytes)
 *   sysctl dev.asmc.0.raw.value=01   # Write new value
 */
static int
asmc_raw_key_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	char newkey[ASMC_KEYLEN + 1];
	uint8_t keylen;
	int error;

	strlcpy(newkey, sc->sc_rawkey, sizeof(newkey));
	error = sysctl_handle_string(oidp, newkey, sizeof(newkey), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (strlen(newkey) != ASMC_KEYLEN)
		return (EINVAL);

	/* Get key info to auto-detect length and type */
	if (asmc_key_getinfo(dev, newkey, &keylen, sc->sc_rawtype) != 0)
		return (ENOENT);

	if (keylen > ASMC_MAXVAL)
		keylen = ASMC_MAXVAL;

	strlcpy(sc->sc_rawkey, newkey, sizeof(sc->sc_rawkey));
	sc->sc_rawlen = keylen;
	memset(sc->sc_rawval, 0, sizeof(sc->sc_rawval));

	/* Read the key value */
	asmc_key_read(dev, sc->sc_rawkey, sc->sc_rawval, sc->sc_rawlen);

	return (0);
}

static int
asmc_raw_value_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);
	char hexbuf[ASMC_MAXVAL * 2 + 1];
	int error, i;

	/* Refresh from SMC if a key has been selected. */
	if (sc->sc_rawkey[0] != '\0') {
		asmc_key_read(dev, sc->sc_rawkey, sc->sc_rawval,
		    sc->sc_rawlen > 0 ? sc->sc_rawlen : ASMC_MAXVAL);
	}

	/* Format as hex string */
	for (i = 0; i < sc->sc_rawlen && i < ASMC_MAXVAL; i++)
		snprintf(hexbuf + i * 2, 3, "%02x", sc->sc_rawval[i]);
	hexbuf[i * 2] = '\0';

	error = sysctl_handle_string(oidp, hexbuf, sizeof(hexbuf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/* Reject writes until a key is selected via raw.key. */
	if (sc->sc_rawkey[0] == '\0')
		return (EINVAL);

	memset(sc->sc_rawval, 0, sizeof(sc->sc_rawval));
	for (i = 0; i < sc->sc_rawlen && hexbuf[i * 2] &&
	    hexbuf[i * 2 + 1]; i++) {
		unsigned int val;
		char tmp[3] = { hexbuf[i * 2], hexbuf[i * 2 + 1], 0 };
		if (sscanf(tmp, "%02x", &val) == 1)
			sc->sc_rawval[i] = (uint8_t)val;
	}

	if (asmc_key_write(dev, sc->sc_rawkey, sc->sc_rawval, sc->sc_rawlen) != 0)
		return (EIO);

	return (0);
}

static int
asmc_raw_len_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);

	return (sysctl_handle_8(oidp, &sc->sc_rawlen, 0, req));
}

static int
asmc_raw_type_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t) arg1;
	struct asmc_softc *sc = device_get_softc(dev);

	return (sysctl_handle_string(oidp, sc->sc_rawtype,
	    sizeof(sc->sc_rawtype), req));
}

void
asmc_debug_sysctl_add(device_t dev, struct sysctl_ctx_list *ctx)
{
	struct asmc_softc *sc = device_get_softc(dev);

	sc->sc_raw_tree = SYSCTL_ADD_NODE(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "raw", CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Raw SMC key access");

	SYSCTL_ADD_PROC(ctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "key",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_key_sysctl, "A",
	    "SMC key name (4 chars)");

	SYSCTL_ADD_PROC(ctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "value",
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_value_sysctl, "A",
	    "SMC key value (hex string)");

	SYSCTL_ADD_PROC(ctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "len",
	    CTLTYPE_U8 | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_len_sysctl, "CU",
	    "SMC key value length");

	SYSCTL_ADD_PROC(ctx,
	    SYSCTL_CHILDREN(sc->sc_raw_tree),
	    OID_AUTO, "type",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    dev, 0, asmc_raw_type_sysctl, "A",
	    "SMC key type (4 chars)");
}
