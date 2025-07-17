/*-
 * Copyright (c) 2023 Juniper Networks, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/bus.h>

#include <dev/spibus/spi.h>
#include "spibus_if.h"
#include "tpm_if.h"
#include "tpm20.h"

#define	TPM_BASE_ADDR		0xD40000
#define	TPM_SPI_HEADER_SIZE	4
#define	TPM_WAIT_STATES		50

static void
tpm_insert_wait(device_t dev)
{
	device_t parent = device_get_parent(dev);
	int wait = TPM_WAIT_STATES;
	struct spi_command spic = SPI_COMMAND_INITIALIZER;

	uint8_t txb = 0;
	uint8_t rxb = 0;

	spic.tx_cmd = &txb;
	spic.rx_cmd = &rxb;
	spic.tx_cmd_sz = 1;
	spic.rx_cmd_sz = 1;
	spic.flags = SPI_FLAG_KEEP_CS;
	do {
		SPIBUS_TRANSFER(parent, dev, &spic);
	} while (--wait > 0 && (rxb & 0x1) == 0);
}

static inline int
tpm_spi_read_n(device_t dev, bus_size_t off, void *buf, size_t size)
{
	struct spi_command spic = SPI_COMMAND_INITIALIZER;
	uint8_t tx[4] = {0};
	uint8_t rx[4] = {0};
	int err;

	if (size > sizeof(rx))
		return (EINVAL);
	off += TPM_BASE_ADDR;
	tx[0] = 0x80 | (size - 1); /* Write (size) bytes */
	tx[1] = (off >> 16) & 0xff;
	tx[2] = (off >> 8) & 0xff;
	tx[3] = off & 0xff;

	spic.tx_cmd = tx;
	spic.tx_cmd_sz = sizeof(tx);
	spic.rx_cmd = rx;
	spic.rx_cmd_sz = sizeof(tx);
	spic.flags = SPI_FLAG_KEEP_CS;

	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &spic);

	if (!(rx[3] & 0x1)) {
		tpm_insert_wait(dev);
	}
	memset(tx, 0, sizeof(tx));
	spic.tx_cmd_sz = spic.rx_cmd_sz = size;
	spic.flags = 0;
	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &spic);
	memcpy(buf, &rx[0], size);

	return (err);
}

static inline int
tpm_spi_write_n(device_t dev, bus_size_t off, void *buf, size_t size)
{
	struct spi_command spic = SPI_COMMAND_INITIALIZER;
	uint8_t tx[8] = {0};
	uint8_t rx[8] = {0};
	int err;

	off += TPM_BASE_ADDR;
	tx[0] = 0x00 | (size - 1); /* Write (size) bytes */
	tx[1] = (off >> 16) & 0xff;
	tx[2] = (off >> 8) & 0xff;
	tx[3] = off & 0xff;

	memcpy(&tx[4], buf, size);

	spic.tx_cmd = tx;
	spic.tx_cmd_sz = size + TPM_SPI_HEADER_SIZE;
	spic.rx_cmd = rx;
	spic.rx_cmd_sz = size + TPM_SPI_HEADER_SIZE;

	err = SPIBUS_TRANSFER(device_get_parent(dev), dev, &spic);

	return (err);
}

/* Override accessors */
static inline uint8_t
spi_read_1(device_t dev, bus_size_t off)
{
	uint8_t rx_byte;

	tpm_spi_read_n(dev, off, &rx_byte, 1);

	return (rx_byte);
}

static inline uint32_t
spi_read_4(device_t dev, bus_size_t off)
{
	uint32_t rx_word = 0;

	tpm_spi_read_n(dev, off, &rx_word, 4);
	rx_word = le32toh(rx_word);

	return (rx_word);
}

static inline void
spi_write_1(device_t dev, bus_size_t off, uint8_t val)
{
	tpm_spi_write_n(dev, off, &val, 1);
}

static inline void
spi_write_4(device_t dev, bus_size_t off, uint32_t val)
{
	uint32_t tmp = htole32(val);
	tpm_spi_write_n(dev, off, &tmp, 4);
}

static device_method_t tpm_spibus_methods[] = {
	DEVMETHOD(tpm_read_4,	spi_read_4),
	DEVMETHOD(tpm_read_1,	spi_read_1),
	DEVMETHOD(tpm_write_4,	spi_write_4),
	DEVMETHOD(tpm_write_1,	spi_write_1),
	DEVMETHOD_END
};

DEFINE_CLASS_0(tpm_spi, tpm_spi_driver, tpm_spibus_methods,
    sizeof(struct tpm_sc));
