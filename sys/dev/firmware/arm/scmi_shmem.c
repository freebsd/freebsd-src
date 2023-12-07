/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023 Arm Ltd
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <machine/atomic.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "mmio_sram_if.h"

#include "scmi_shmem.h"
#include "scmi.h"

#define INFLIGHT_NONE	0
#define INFLIGHT_REQ	1

struct shmem_softc {
	device_t		dev;
	device_t		parent;
	int			reg;
	int			inflight;
};

static void	scmi_shmem_read(device_t, bus_size_t, void *, bus_size_t);
static void	scmi_shmem_write(device_t, bus_size_t, const void *,
				 bus_size_t);
static void	scmi_shmem_acquire_channel(struct shmem_softc *);
static void	scmi_shmem_release_channel(struct shmem_softc *);

static int	shmem_probe(device_t);
static int	shmem_attach(device_t);
static int	shmem_detach(device_t);

static int
shmem_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "arm,scmi-shmem"))
		return (ENXIO);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	device_set_desc(dev, "ARM SCMI Shared Memory driver");

	return (BUS_PROBE_DEFAULT);
}

static int
shmem_attach(device_t dev)
{
	struct shmem_softc *sc;
	phandle_t node;
	int reg;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->parent = device_get_parent(dev);

	node = ofw_bus_get_node(dev);
	if (node == -1)
		return (ENXIO);

	OF_getencprop(node, "reg", &reg, sizeof(reg));

	dprintf("%s: reg %x\n", __func__, reg);

	sc->reg = reg;
	atomic_store_rel_int(&sc->inflight, INFLIGHT_NONE);

	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}

static int
shmem_detach(device_t dev)
{

	return (0);
}

static void
scmi_shmem_read(device_t dev, bus_size_t offset, void *buf, bus_size_t len)
{
	struct shmem_softc *sc;
	uint8_t *addr;
	int i;

	sc = device_get_softc(dev);

	addr = (uint8_t *)buf;

	for (i = 0; i < len; i++)
		addr[i] = MMIO_SRAM_READ_1(sc->parent, sc->reg + offset + i);
}

static void
scmi_shmem_write(device_t dev, bus_size_t offset, const void *buf,
    bus_size_t len)
{
	struct shmem_softc *sc;
	const uint8_t *addr;
	int i;

	sc = device_get_softc(dev);

	addr = (const uint8_t *)buf;

	for (i = 0; i < len; i++)
		MMIO_SRAM_WRITE_1(sc->parent, sc->reg + offset + i, addr[i]);
}

device_t
scmi_shmem_get(device_t dev, phandle_t node, int index)
{
	phandle_t *shmems;
	device_t shmem_dev;
	size_t len;

	len = OF_getencprop_alloc_multi(node, "shmem", sizeof(*shmems),
	    (void **)&shmems);
	if (len <= 0) {
		device_printf(dev, "%s: Can't get shmem node.\n", __func__);
		return (NULL);
	}

	if (index >= len) {
		OF_prop_free(shmems);
		return (NULL);
	}

	shmem_dev = OF_device_from_xref(shmems[index]);
	if (shmem_dev == NULL)
		device_printf(dev, "%s: Can't get shmem device.\n",
		    __func__);

	OF_prop_free(shmems);

	return (shmem_dev);
}

static void
scmi_shmem_acquire_channel(struct shmem_softc *sc)
{

	 while ((atomic_cmpset_acq_int(&sc->inflight, INFLIGHT_NONE,
	     INFLIGHT_REQ)) == 0)
		 DELAY(1000);
}

static void
scmi_shmem_release_channel(struct shmem_softc *sc)
{

	atomic_store_rel_int(&sc->inflight, INFLIGHT_NONE);
}

int
scmi_shmem_prepare_msg(device_t dev, struct scmi_req *req, bool polling)
{
	struct shmem_softc *sc;
	struct scmi_smt_header hdr = {};
	uint32_t channel_status;

	sc = device_get_softc(dev);

	/* Get exclusive write access to channel */
	scmi_shmem_acquire_channel(sc);

	/* Read channel status */
	scmi_shmem_read(dev, SMT_OFFSET_CHAN_STATUS, &channel_status,
	    SMT_SIZE_CHAN_STATUS);
	if ((channel_status & SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE) == 0) {
		scmi_shmem_release_channel(sc);
		device_printf(dev, "Shmem channel busy. Abort !.\n");
		return (EBUSY);
	}

	/* Update header */
	hdr.channel_status &= ~SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE;
	hdr.msg_header = htole32(req->msg_header);
	hdr.length = htole32(sizeof(req->msg_header) + req->in_size);
	if (!polling)
		hdr.flags |= SCMI_SHMEM_FLAG_INTR_ENABLED;
	else
		hdr.flags &= ~SCMI_SHMEM_FLAG_INTR_ENABLED;

	/* Write header */
	scmi_shmem_write(dev, 0, &hdr, SMT_SIZE_HEADER);

	/* Write request payload if any */
	if (req->in_size)
		scmi_shmem_write(dev, SMT_SIZE_HEADER, req->in_buf,
		    req->in_size);

	return (0);
}

void
scmi_shmem_clear_channel(device_t dev)
{
	uint32_t channel_status = 0;

	if (dev == NULL)
		return;

	channel_status |= SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE;
	scmi_shmem_write(dev, SMT_OFFSET_CHAN_STATUS, &channel_status,
	    SMT_SIZE_CHAN_STATUS);
}

int
scmi_shmem_read_msg_header(device_t dev, uint32_t *msg_header)
{
	uint32_t length, header;

	/* Read and check length. */
	scmi_shmem_read(dev, SMT_OFFSET_LENGTH, &length, SMT_SIZE_LENGTH);
	if (le32toh(length) < sizeof(header))
		return (EINVAL);

	/* Read header. */
	scmi_shmem_read(dev, SMT_OFFSET_MSG_HEADER, &header,
	    SMT_SIZE_MSG_HEADER);

	*msg_header = le32toh(header);

	return (0);
}

int
scmi_shmem_read_msg_payload(device_t dev, uint8_t *buf, uint32_t buf_len)
{
	uint32_t length, payld_len;

	/* Read length. */
	scmi_shmem_read(dev, SMT_OFFSET_LENGTH, &length, SMT_SIZE_LENGTH);
	payld_len = le32toh(length) - SCMI_MSG_HDR_SIZE;

	if (payld_len > buf_len) {
		device_printf(dev,
		    "RX payload %dbytes exceeds buflen %dbytes. Truncate.\n",
		    payld_len, buf_len);
		payld_len = buf_len;
	}

	/* Read response payload */
	scmi_shmem_read(dev, SMT_SIZE_HEADER, buf, payld_len);

	return (0);
}

void
scmi_shmem_tx_complete(device_t dev)
{
	struct shmem_softc *sc;

	sc = device_get_softc(dev);
	scmi_shmem_release_channel(sc);
}

bool scmi_shmem_poll_msg(device_t dev, uint32_t msg_header)
{
	uint32_t status, header;

	scmi_shmem_read(dev, SMT_OFFSET_MSG_HEADER, &header,
	    SMT_SIZE_MSG_HEADER);
	/* Bail out if it is NOT what we were polling for. */
	if (le32toh(header) != msg_header)
		return (false);

	scmi_shmem_read(dev, SMT_OFFSET_CHAN_STATUS, &status,
	    SMT_SIZE_CHAN_STATUS);

	return (status & (SCMI_SHMEM_CHAN_STAT_CHANNEL_ERROR |
	    SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE));
}

static device_method_t shmem_methods[] = {
	DEVMETHOD(device_probe,		shmem_probe),
	DEVMETHOD(device_attach,	shmem_attach),
	DEVMETHOD(device_detach,	shmem_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_1(shmem, shmem_driver, shmem_methods, sizeof(struct shmem_softc),
    simplebus_driver);

EARLY_DRIVER_MODULE(shmem, mmio_sram, shmem_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(scmi, 1);
