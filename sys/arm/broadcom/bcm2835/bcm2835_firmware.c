/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Andrew Turner
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/broadcom/bcm2835/bcm2835_firmware.h>
#include <arm/broadcom/bcm2835/bcm2835_mbox.h>
#include <arm/broadcom/bcm2835/bcm2835_mbox_prop.h>
#include <arm/broadcom/bcm2835/bcm2835_vcbus.h>

struct bcm2835_firmware_softc {
	struct simplebus_softc	sc;
	phandle_t	sc_mbox;
};

static struct ofw_compat_data compat_data[] = {
	{"raspberrypi,bcm2835-firmware",	1},
	{NULL,					0}
};

static int sysctl_bcm2835_firmware_get_revision(SYSCTL_HANDLER_ARGS);

static int
bcm2835_firmware_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "BCM2835 Firmware");
	return (BUS_PROBE_DEFAULT);
}

static int
bcm2835_firmware_attach(device_t dev)
{
	struct bcm2835_firmware_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;
	phandle_t node, mbox;
	int rv;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	rv = OF_getencprop(node, "mboxes", &mbox, sizeof(mbox));
	if (rv <= 0) {
		device_printf(dev, "can't read mboxes property\n");
		return (ENXIO);
	}
	sc->sc_mbox = mbox;

	OF_device_register_xref(OF_xref_from_node(node), dev);

	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "revision",
	    CTLTYPE_UINT | CTLFLAG_RD, sc, sizeof(*sc),
	    sysctl_bcm2835_firmware_get_revision, "IU",
	    "Firmware revision");

	/* The firmwaare doesn't have a ranges property */
	sc->sc.flags |= SB_FLAG_NO_RANGES;
	return (simplebus_attach(dev));
}

int
bcm2835_firmware_property(device_t dev, uint32_t prop, void *data, size_t len)
{
	struct {
		struct bcm2835_mbox_hdr hdr;
		struct bcm2835_mbox_tag_hdr tag_hdr;
		uint32_t data[];
	} *msg_hdr;
	size_t msg_len;
	int err;

	/*
	 * The message is processed in 32-bit chunks so must be a multiple
	 * of 32-bits.
	 */
	if ((len & (sizeof(uint32_t) - 1)) != 0)
		return (EINVAL);

	msg_len = sizeof(*msg_hdr) + len + sizeof(uint32_t);
	msg_hdr = malloc(sizeof(*msg_hdr) + msg_len + sizeof(uint32_t),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	msg_hdr->hdr.buf_size = msg_len;
	msg_hdr->hdr.code = BCM2835_MBOX_CODE_REQ;
	msg_hdr->tag_hdr.tag = prop;
	msg_hdr->tag_hdr.val_buf_size = len;
	memcpy(msg_hdr->data, data, len);
	msg_hdr->data[len / sizeof(uint32_t)] = 0;

	err = bcm2835_mbox_property(msg_hdr, msg_len);
	if (err == 0) {
		memcpy(data, msg_hdr->data, len);
	}

	free(msg_hdr, M_DEVBUF);

	return (err);
}

static int
sysctl_bcm2835_firmware_get_revision(SYSCTL_HANDLER_ARGS)
{
	struct bcm2835_firmware_softc *sc = arg1;
	uint32_t rev;
	int err;

	if (bcm2835_firmware_property(sc->sc.dev,
	    BCM2835_MBOX_TAG_FIRMWARE_REVISION, &rev, sizeof(rev)) != 0)
		return (ENXIO);

	err = sysctl_handle_int(oidp, &rev, sizeof(rev), req);
	if (err || !req->newptr) /* error || read request */
		return (err);

	/* write request */
	return (EINVAL);
}

static device_method_t bcm2835_firmware_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bcm2835_firmware_probe),
	DEVMETHOD(device_attach,	bcm2835_firmware_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(bcm2835_firmware, bcm2835_firmware_driver,
    bcm2835_firmware_methods, sizeof(struct bcm2835_firmware_softc),
    simplebus_driver);

EARLY_DRIVER_MODULE(bcm2835_firmware, simplebus, bcm2835_firmware_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LATE);
MODULE_DEPEND(bcm2835_firmware, mbox, 1, 1, 1);
