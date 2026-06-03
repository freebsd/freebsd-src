/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_ASMC_ASMCMMIO_H_
#define _DEV_ASMC_ASMCMMIO_H_

struct asmc_softc;

/*
 * MMIO register offsets.
 */
#define ASMC_MMIO_DATA		0x0000
#define ASMC_MMIO_KEY_NAME	0x0078
#define ASMC_MMIO_DATA_LEN	0x007D
#define ASMC_MMIO_SMC_ID	0x007E
#define ASMC_MMIO_CMD		0x007F
#define ASMC_MMIO_STATUS	0x4005
#define ASMC_MMIO_MIN_SIZE	0x4006
#define ASMC_MMIO_STATUS_READY	0x20	/* Bit 5 */
#define ASMC_MMIO_MAX_WAIT	24

/*
 * T2-specific keys.
 */
#define ASMC_KEY_LDKN		"LDKN"	/* RO; 1 byte, firmware version */
#define ASMC_KEY_BCLM		"BCLM"	/* RW; 1 byte, battery charge limit 0-100 */
#define ASMC_KEY_FANMANUAL_T2	"F%dMd"	/* RW; 1 byte per fan (T2) */

/*
 * MMIO backend functions.
 */
int	asmc_mmio_probe(device_t dev);
void	asmc_mmio_detach(device_t dev, struct asmc_softc *sc);
int	asmc_mmio_key_read(device_t dev, const char *key,
	    uint8_t *buf, uint8_t len);
int	asmc_mmio_key_write(device_t dev, const char *key,
	    uint8_t *buf, uint8_t len);
int	asmc_mmio_key_getinfo(device_t dev, const char *key,
	    uint8_t *len, char *type);
int	asmc_mmio_key_getbyindex(device_t dev, int index, char *key);

/*
 * IEEE 754 float <-> uint32 conversion for T2 fan RPM values.
 */
uint32_t	asmc_float_to_u32(uint32_t d);
uint32_t	asmc_u32_to_float(uint32_t d);

/*
 * T2-specific sysctls.
 */
int	asmc_bclm_sysctl(SYSCTL_HANDLER_ARGS);

#endif /* _DEV_ASMC_ASMCMMIO_H_ */
