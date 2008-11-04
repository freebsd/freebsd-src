/*
 * ng_ubt.c
 */

/*-
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ng_ubt.c,v 1.16 2003/10/10 19:15:06 max Exp $
 * $FreeBSD$
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_parse.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_busdma.h>

#include <sys/mbuf.h>

#include <netgraph/ng_message.h>
#include <netgraph/netgraph.h>
#include <netgraph/ng_parse.h>
#include <netgraph/bluetooth/include/ng_bluetooth.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_ubt.h>

#include <dev/usb2/bluetooth/usb2_bluetooth.h>
#include <dev/usb2/bluetooth/ng_ubt2_var.h>

/*
 * USB methods
 */

static device_probe_t ubt_probe;
static device_attach_t ubt_attach;
static device_detach_t ubt_detach;

static devclass_t ubt_devclass;

static device_method_t ubt_methods[] = {
	DEVMETHOD(device_probe, ubt_probe),
	DEVMETHOD(device_attach, ubt_attach),
	DEVMETHOD(device_detach, ubt_detach),
	{0, 0}
};

static driver_t ubt_driver = {
	.name = "ubt",
	.methods = ubt_methods,
	.size = sizeof(struct ubt_softc),
};

/*
 * Netgraph methods
 */

static ng_constructor_t ng_ubt_constructor;
static ng_shutdown_t ng_ubt_shutdown;
static ng_newhook_t ng_ubt_newhook;
static ng_connect_t ng_ubt_connect;
static ng_disconnect_t ng_ubt_disconnect;
static ng_rcvmsg_t ng_ubt_rcvmsg;
static ng_rcvdata_t ng_ubt_rcvdata;

/* Queue length */
static const struct ng_parse_struct_field ng_ubt_node_qlen_type_fields[] =
{
	{"queue", &ng_parse_int32_type,},
	{"qlen", &ng_parse_int32_type,},
	{NULL,}
};
static const struct ng_parse_type ng_ubt_node_qlen_type = {
	&ng_parse_struct_type,
	&ng_ubt_node_qlen_type_fields
};

/* Stat info */
static const struct ng_parse_struct_field ng_ubt_node_stat_type_fields[] =
{
	{"pckts_recv", &ng_parse_uint32_type,},
	{"bytes_recv", &ng_parse_uint32_type,},
	{"pckts_sent", &ng_parse_uint32_type,},
	{"bytes_sent", &ng_parse_uint32_type,},
	{"oerrors", &ng_parse_uint32_type,},
	{"ierrors", &ng_parse_uint32_type,},
	{NULL,}
};
static const struct ng_parse_type ng_ubt_node_stat_type = {
	&ng_parse_struct_type,
	&ng_ubt_node_stat_type_fields
};

/* Netgraph node command list */
static const struct ng_cmdlist ng_ubt_cmdlist[] = {
	{
		NGM_UBT_COOKIE,
		NGM_UBT_NODE_SET_DEBUG,
		"set_debug",
		&ng_parse_uint16_type,
		NULL
	},
	{
		NGM_UBT_COOKIE,
		NGM_UBT_NODE_GET_DEBUG,
		"get_debug",
		NULL,
		&ng_parse_uint16_type
	},
	{
		NGM_UBT_COOKIE,
		NGM_UBT_NODE_SET_QLEN,
		"set_qlen",
		&ng_ubt_node_qlen_type,
		NULL
	},
	{
		NGM_UBT_COOKIE,
		NGM_UBT_NODE_GET_QLEN,
		"get_qlen",
		&ng_ubt_node_qlen_type,
		&ng_ubt_node_qlen_type
	},
	{
		NGM_UBT_COOKIE,
		NGM_UBT_NODE_GET_STAT,
		"get_stat",
		NULL,
		&ng_ubt_node_stat_type
	},
	{
		NGM_UBT_COOKIE,
		NGM_UBT_NODE_RESET_STAT,
		"reset_stat",
		NULL,
		NULL
	},
	{0,}
};

/* Netgraph node type */
static struct ng_type typestruct = {
	.version = NG_ABI_VERSION,
	.name = NG_UBT_NODE_TYPE,
	.constructor = ng_ubt_constructor,
	.rcvmsg = ng_ubt_rcvmsg,
	.shutdown = ng_ubt_shutdown,
	.newhook = ng_ubt_newhook,
	.connect = ng_ubt_connect,
	.rcvdata = ng_ubt_rcvdata,
	.disconnect = ng_ubt_disconnect,
	.cmdlist = ng_ubt_cmdlist
};

/* USB methods */

static usb2_callback_t ubt_ctrl_write_callback;
static usb2_callback_t ubt_intr_read_callback;
static usb2_callback_t ubt_intr_read_clear_stall_callback;
static usb2_callback_t ubt_bulk_read_callback;
static usb2_callback_t ubt_bulk_read_clear_stall_callback;
static usb2_callback_t ubt_bulk_write_callback;
static usb2_callback_t ubt_bulk_write_clear_stall_callback;
static usb2_callback_t ubt_isoc_read_callback;
static usb2_callback_t ubt_isoc_write_callback;

static int ubt_modevent(module_t mod, int event, void *data);
static void ubt_intr_read_complete(node_p node, hook_p hook, void *arg1, int arg2);
static void ubt_bulk_read_complete(node_p node, hook_p hook, void *arg1, int arg2);
static void ubt_isoc_read_complete(node_p node, hook_p hook, void *arg1, int arg2);

/* USB config */
static const struct usb2_config ubt_config_if_0[UBT_IF_0_N_TRANSFER] = {

	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = UBT_BULK_WRITE_BUFFER_SIZE,
		.mh.flags = {.pipe_bof = 1,},
		.mh.callback = &ubt_bulk_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = UBT_BULK_READ_BUFFER_SIZE,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = &ubt_bulk_read_callback,
	},

	[2] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0x110,	/* bytes */
		.mh.callback = &ubt_intr_read_callback,
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = (sizeof(struct usb2_device_request) + UBT_CTRL_BUFFER_SIZE),
		.mh.callback = &ubt_ctrl_write_callback,
		.mh.timeout = 5000,	/* 5 seconds */
	},

	[4] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ubt_bulk_write_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[5] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ubt_bulk_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},

	[6] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.mh.bufsize = sizeof(struct usb2_device_request),
		.mh.callback = &ubt_intr_read_clear_stall_callback,
		.mh.timeout = 1000,	/* 1 second */
		.mh.interval = 50,	/* 50ms */
	},
};

/* USB config */
static const struct usb2_config
	ubt_config_if_1_full_speed[UBT_IF_1_N_TRANSFER] = {

	[0] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_read_callback,
	},

	[1] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_read_callback,
	},

	[2] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_write_callback,
	},

	[3] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_write_callback,
	},
};

/* USB config */
static const struct usb2_config
	ubt_config_if_1_high_speed[UBT_IF_1_N_TRANSFER] = {

	[0] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES * 8,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_read_callback,
	},

	[1] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES * 8,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_read_callback,
	},

	[2] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES * 8,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_write_callback,
	},

	[3] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.mh.frames = UBT_ISOC_NFRAMES * 8,
		.mh.flags = {.short_xfer_ok = 1,},
		.mh.callback = &ubt_isoc_write_callback,
	},
};

/*
 * Module
 */

DRIVER_MODULE(ng_ubt, ushub, ubt_driver, ubt_devclass, ubt_modevent, 0);
MODULE_VERSION(ng_ubt, NG_BLUETOOTH_VERSION);
MODULE_DEPEND(ng_ubt, netgraph, NG_ABI_VERSION, NG_ABI_VERSION, NG_ABI_VERSION);
MODULE_DEPEND(ng_ubt, usb2_bluetooth, 1, 1, 1);
MODULE_DEPEND(ng_ubt, usb2_core, 1, 1, 1);

/****************************************************************************
 ****************************************************************************
 **                              USB specific
 ****************************************************************************
 ****************************************************************************/

/*
 * Load/Unload the driver module
 */

static int
ubt_modevent(module_t mod, int event, void *data)
{
	int error;

	switch (event) {
	case MOD_LOAD:
		error = ng_newtype(&typestruct);
		if (error != 0) {
			printf("%s: Could not register "
			    "Netgraph node type, error=%d\n",
			    NG_UBT_NODE_TYPE, error);
		}
		break;

	case MOD_UNLOAD:
		error = ng_rmtype(&typestruct);
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}					/* ubt_modevent */

/*
 * If for some reason device should not be attached then put
 * VendorID/ProductID pair into the list below. The format is
 * as follows:
 *
 *	{ VENDOR_ID, PRODUCT_ID },
 *
 * where VENDOR_ID and PRODUCT_ID are hex numbers.
 */
static const struct usb2_device_id ubt_ignore_devs[] = {
	/* AVM USB Bluetooth-Adapter BlueFritz! v1.0 */
	{USB_VPI(USB_VENDOR_AVM, 0x2200, 0)},
};

/* List of supported bluetooth devices */
static const struct usb2_device_id ubt_devs[] = {
	/* Generic Bluetooth class devices. */
	{USB_IFACE_CLASS(UDCLASS_WIRELESS),
		USB_IFACE_SUBCLASS(UDSUBCLASS_RF),
	USB_IFACE_PROTOCOL(UDPROTO_BLUETOOTH)},

	/* AVM USB Bluetooth-Adapter BlueFritz! v2.0 */
	{USB_VPI(USB_VENDOR_AVM, 0x3800, 0)},
};

/*
 * Probe for a USB Bluetooth device
 */

static int
ubt_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST) {
		return (ENXIO);
	}
	if (uaa->info.bIfaceIndex != 0) {
		return (ENXIO);
	}
	if (usb2_lookup_id_by_uaa(ubt_ignore_devs,
	    sizeof(ubt_ignore_devs), uaa) == 0) {
		return (ENXIO);
	}
	return (usb2_lookup_id_by_uaa(ubt_devs, sizeof(ubt_devs), uaa));
}

/*
 * Attach the device
 */

static int
ubt_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ubt_softc *sc = device_get_softc(dev);
	const struct usb2_config *isoc_setup;
	struct usb2_endpoint_descriptor *ed;
	uint16_t wMaxPacketSize;
	uint8_t alt_index;
	uint8_t iface_index;
	uint8_t i;
	uint8_t j;

	device_set_usb2_desc(dev);

	snprintf(sc->sc_name, sizeof(sc->sc_name),
	    "%s", device_get_nameunit(dev));

	/*
	 * Initialize device softc structure
	 */

	/* state */
	sc->sc_debug = NG_UBT_WARN_LEVEL;
	sc->sc_flags = 0;
	NG_UBT_STAT_RESET(sc->sc_stat);

	/* control pipe */
	NG_BT_MBUFQ_INIT(&sc->sc_cmdq, UBT_DEFAULT_QLEN);

	/* bulk-out pipe */
	NG_BT_MBUFQ_INIT(&sc->sc_aclq, UBT_DEFAULT_QLEN);

	/* isoc-out pipe */
	NG_BT_MBUFQ_INIT(&sc->sc_scoq,
	    (usb2_get_speed(uaa->device) == USB_SPEED_HIGH) ?
	    (2 * UBT_ISOC_NFRAMES * 8) :
	    (2 * UBT_ISOC_NFRAMES));

	/* isoc-in pipe */
	NG_BT_MBUFQ_INIT(&sc->sc_sciq,
	    (usb2_get_speed(uaa->device) == USB_SPEED_HIGH) ?
	    (2 * UBT_ISOC_NFRAMES * 8) :
	    (2 * UBT_ISOC_NFRAMES));

	/* netgraph part */
	sc->sc_node = NULL;
	sc->sc_hook = NULL;

	/*
	 * Configure Bluetooth USB device. Discover all required USB
	 * interfaces and endpoints.
	 *
	 * USB device must present two interfaces:
	 * 1) Interface 0 that has 3 endpoints
	 *	1) Interrupt endpoint to receive HCI events
	 *	2) Bulk IN endpoint to receive ACL data
	 *	3) Bulk OUT endpoint to send ACL data
	 *
	 * 2) Interface 1 then has 2 endpoints
	 *	1) Isochronous IN endpoint to receive SCO data
 	 *	2) Isochronous OUT endpoint to send SCO data
	 *
	 * Interface 1 (with isochronous endpoints) has several alternate
	 * configurations with different packet size.
	 */

	/*
	 * Interface 0
	 */

	mtx_init(&sc->sc_mtx, "ubt lock", NULL, MTX_DEF | MTX_RECURSE);

	iface_index = 0;
	if (usb2_transfer_setup
	    (uaa->device, &iface_index, sc->sc_xfer_if_0, ubt_config_if_0,
	    UBT_IF_0_N_TRANSFER, sc, &sc->sc_mtx)) {
		device_printf(dev, "Could not allocate transfers "
		    "for interface 0!\n");
		goto detach;
	}
	/*
	 * Interface 1
	 * (search alternate settings, and find
	 *  the descriptor with the largest
	 *  wMaxPacketSize)
	 */
	isoc_setup =
	    ((usb2_get_speed(uaa->device) == USB_SPEED_HIGH) ?
	    ubt_config_if_1_high_speed :
	    ubt_config_if_1_full_speed);

	wMaxPacketSize = 0;

	/* search through all the descriptors looking for bidir mode */

	alt_index = 0 - 1;
	i = 0;
	j = 0;
	while (1) {
		uint16_t temp;

		ed = usb2_find_edesc(
		    usb2_get_config_descriptor(uaa->device), 1, i, j);
		if (ed == NULL) {
			if (j == 0) {
				/* end of interfaces */
				break;
			} else {
				/* next interface */
				j = 0;
				i++;
				continue;
			}
		}
		temp = UGETW(ed->wMaxPacketSize);
		if (temp > wMaxPacketSize) {
			wMaxPacketSize = temp;
			alt_index = i;
		}
		j++;
	}

	if (usb2_set_alt_interface_index(uaa->device, 1, alt_index)) {
		device_printf(dev, "Could not set alternate "
		    "setting %d for interface 1!\n", alt_index);
		goto detach;
	}
	iface_index = 1;
	if (usb2_transfer_setup
	    (uaa->device, &iface_index, sc->sc_xfer_if_1,
	    isoc_setup, UBT_IF_1_N_TRANSFER, sc, &sc->sc_mtx)) {
		device_printf(dev, "Could not allocate transfers "
		    "for interface 1!\n");
		goto detach;
	}
	/* create Netgraph node */

	if (ng_make_node_common(&typestruct, &sc->sc_node) != 0) {
		printf("%s: Could not create Netgraph node\n",
		    sc->sc_name);
		sc->sc_node = NULL;
		goto detach;
	}
	/* name node */

	if (ng_name_node(sc->sc_node, sc->sc_name) != 0) {
		printf("%s: Could not name Netgraph node\n",
		    sc->sc_name);
		NG_NODE_UNREF(sc->sc_node);
		sc->sc_node = NULL;
		goto detach;
	}
	NG_NODE_SET_PRIVATE(sc->sc_node, sc);
	NG_NODE_FORCE_WRITER(sc->sc_node);

	/* claim all interfaces on the device */

	for (i = 1;; i++) {

		if (usb2_get_iface(uaa->device, i) == NULL) {
			break;
		}
		usb2_set_parent_iface(uaa->device, i, uaa->info.bIfaceIndex);
	}

	return (0);			/* success */

detach:
	ubt_detach(dev);

	return (ENXIO);
}

/*
 * Detach the device
 */

int
ubt_detach(device_t dev)
{
	struct ubt_softc *sc = device_get_softc(dev);

	/* destroy Netgraph node */

	if (sc->sc_node != NULL) {
		NG_NODE_SET_PRIVATE(sc->sc_node, NULL);
		ng_rmnode_self(sc->sc_node);
		sc->sc_node = NULL;
	}
	/* free USB transfers, if any */

	usb2_transfer_unsetup(sc->sc_xfer_if_0, UBT_IF_0_N_TRANSFER);

	usb2_transfer_unsetup(sc->sc_xfer_if_1, UBT_IF_1_N_TRANSFER);

	mtx_destroy(&sc->sc_mtx);

	/* destroy queues */

	NG_BT_MBUFQ_DESTROY(&sc->sc_cmdq);
	NG_BT_MBUFQ_DESTROY(&sc->sc_aclq);
	NG_BT_MBUFQ_DESTROY(&sc->sc_scoq);
	NG_BT_MBUFQ_DESTROY(&sc->sc_sciq);

	return (0);
}

static void
ubt_ctrl_write_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct usb2_device_request req;
	struct mbuf *m;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
tr_transferred:

		if (xfer->error) {
			NG_UBT_STAT_OERROR(sc->sc_stat);
		} else {
			NG_UBT_STAT_BYTES_SENT(sc->sc_stat, xfer->actlen);
			NG_UBT_STAT_PCKTS_SENT(sc->sc_stat);
		}

	case USB_ST_SETUP:

		/* get next mbuf, if any */

		NG_BT_MBUFQ_DEQUEUE(&sc->sc_cmdq, m);

		if (m == NULL) {
			NG_UBT_INFO(sc, "HCI command queue is empty\n");
			return;
		}
		/*
		 * check HCI command frame size and
		 * copy it to USB transfer buffer:
		 */

		if (m->m_pkthdr.len > UBT_CTRL_BUFFER_SIZE) {
			panic("HCI command frame too big, size=%zd, len=%d\n",
			    UBT_CTRL_BUFFER_SIZE, m->m_pkthdr.len);
		}
		/* initialize a USB control request and then schedule it */

		bzero(&req, sizeof(req));

		req.bmRequestType = UBT_HCI_REQUEST;
		USETW(req.wLength, m->m_pkthdr.len);

		NG_UBT_INFO(sc, "Sending control request, bmRequestType=0x%02x, "
		    "wLength=%d\n", req.bmRequestType, UGETW(req.wLength));

		usb2_copy_in(xfer->frbuffers, 0, &req, sizeof(req));
		usb2_m_copy_in(xfer->frbuffers + 1, 0, m, 0, m->m_pkthdr.len);

		xfer->frlengths[0] = sizeof(req);
		xfer->frlengths[1] = m->m_pkthdr.len;
		xfer->nframes = xfer->frlengths[1] ? 2 : 1;

		NG_FREE_M(m);

		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			/* ignore */
			return;
		}
		goto tr_transferred;
	}
}

static void
ubt_intr_read_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct mbuf *m;
	uint32_t max_len;
	uint8_t *ptr;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		/* allocate a new mbuf */

		MGETHDR(m, M_DONTWAIT, MT_DATA);

		if (m == NULL) {
			goto tr_setup;
		}
		MCLGET(m, M_DONTWAIT);

		if (!(m->m_flags & M_EXT)) {
			NG_FREE_M(m);
			goto tr_setup;
		}
		if (!(sc->sc_flags & UBT_HAVE_FRAME_TYPE)) {
			*mtod(m, uint8_t *)= NG_HCI_EVENT_PKT;
			m->m_pkthdr.len = m->m_len = 1;
		} else {
			m->m_pkthdr.len = m->m_len = 0;
		}

		max_len = (MCLBYTES - m->m_len);

		if (xfer->actlen > max_len) {
			xfer->actlen = max_len;
		}
		ptr = ((uint8_t *)(m->m_data)) + m->m_len;

		usb2_copy_out(xfer->frbuffers, 0, ptr, xfer->actlen);

		m->m_pkthdr.len += xfer->actlen;
		m->m_len += xfer->actlen;

		NG_UBT_INFO(sc, "got %d bytes from interrupt "
		    "pipe\n", xfer->actlen);

		sc->sc_intr_buffer = m;

	case USB_ST_SETUP:
tr_setup:

		if (sc->sc_intr_buffer) {
			ng_send_fn(sc->sc_node, NULL, ubt_intr_read_complete, NULL, 0);
			return;
		}
		if (sc->sc_flags & UBT_FLAG_INTR_STALL) {
			usb2_transfer_start(sc->sc_xfer_if_0[6]);
			return;
		}
		xfer->frlengths[0] = xfer->max_data_length;

		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UBT_FLAG_INTR_STALL;
			usb2_transfer_start(sc->sc_xfer_if_0[6]);
		}
		return;

	}
}

static void
ubt_intr_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_if_0[2];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UBT_FLAG_INTR_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ubt_intr_read_complete(node_p node, hook_p hook, void *arg1, int arg2)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(node);
	struct mbuf *m;
	ng_hci_event_pkt_t *hdr;
	int error;

	if (sc == NULL) {
		return;
	}
	mtx_lock(&sc->sc_mtx);

	m = sc->sc_intr_buffer;

	if (m) {

		sc->sc_intr_buffer = NULL;

		hdr = mtod(m, ng_hci_event_pkt_t *);

		if ((sc->sc_hook == NULL) || NG_HOOK_NOT_VALID(sc->sc_hook)) {
			NG_UBT_INFO(sc, "No upstream hook\n");
			goto done;
		}
		NG_UBT_STAT_BYTES_RECV(sc->sc_stat, m->m_pkthdr.len);

		if (m->m_pkthdr.len < sizeof(*hdr)) {
			NG_UBT_INFO(sc, "Packet too short\n");
			goto done;
		}
		if (hdr->length == (m->m_pkthdr.len - sizeof(*hdr))) {
			NG_UBT_INFO(sc, "Got complete HCI event frame, "
			    "pktlen=%d, length=%d\n",
			    m->m_pkthdr.len, hdr->length);

			NG_UBT_STAT_PCKTS_RECV(sc->sc_stat);

			NG_SEND_DATA_ONLY(error, sc->sc_hook, m);

			m = NULL;

			if (error != 0) {
				NG_UBT_STAT_IERROR(sc->sc_stat);
			}
		} else {
			NG_UBT_ERR(sc, "Invalid HCI event frame size, "
			    "length=%d, pktlen=%d\n",
			    hdr->length, m->m_pkthdr.len);

			NG_UBT_STAT_IERROR(sc->sc_stat);
		}
	}
done:
	if (m) {
		NG_FREE_M(m);
	}
	/* start USB transfer if not already started */

	usb2_transfer_start(sc->sc_xfer_if_0[2]);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
ubt_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct mbuf *m;
	uint32_t max_len;
	uint8_t *ptr;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		/* allocate new mbuf */

		MGETHDR(m, M_DONTWAIT, MT_DATA);

		if (m == NULL) {
			goto tr_setup;
		}
		MCLGET(m, M_DONTWAIT);

		if (!(m->m_flags & M_EXT)) {
			NG_FREE_M(m);
			goto tr_setup;
		}
		if (!(sc->sc_flags & UBT_HAVE_FRAME_TYPE)) {
			*mtod(m, uint8_t *)= NG_HCI_ACL_DATA_PKT;
			m->m_pkthdr.len = m->m_len = 1;
		} else {
			m->m_pkthdr.len = m->m_len = 0;
		}

		max_len = (MCLBYTES - m->m_len);

		if (xfer->actlen > max_len) {
			xfer->actlen = max_len;
		}
		ptr = ((uint8_t *)(m->m_data)) + m->m_len;

		usb2_copy_out(xfer->frbuffers, 0, ptr, xfer->actlen);

		m->m_pkthdr.len += xfer->actlen;
		m->m_len += xfer->actlen;

		NG_UBT_INFO(sc, "got %d bytes from bulk-in "
		    "pipe\n", xfer->actlen);

		sc->sc_bulk_in_buffer = m;

	case USB_ST_SETUP:
tr_setup:
		if (sc->sc_bulk_in_buffer) {
			ng_send_fn(sc->sc_node, NULL, ubt_bulk_read_complete, NULL, 0);
			return;
		}
		if (sc->sc_flags & UBT_FLAG_READ_STALL) {
			usb2_transfer_start(sc->sc_xfer_if_0[5]);
			return;
		}
		xfer->frlengths[0] = xfer->max_data_length;

		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			sc->sc_flags |= UBT_FLAG_READ_STALL;
			usb2_transfer_start(sc->sc_xfer_if_0[5]);
		}
		return;

	}
}

static void
ubt_bulk_read_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_if_0[1];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UBT_FLAG_READ_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ubt_bulk_read_complete(node_p node, hook_p hook, void *arg1, int arg2)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(node);
	struct mbuf *m;
	ng_hci_acldata_pkt_t *hdr;
	uint16_t len;
	int error;

	if (sc == NULL) {
		return;
	}
	mtx_lock(&sc->sc_mtx);

	m = sc->sc_bulk_in_buffer;

	if (m) {

		sc->sc_bulk_in_buffer = NULL;

		hdr = mtod(m, ng_hci_acldata_pkt_t *);

		if (sc->sc_hook == NULL || NG_HOOK_NOT_VALID(sc->sc_hook)) {
			NG_UBT_INFO(sc, "No upstream hook\n");
			goto done;
		}
		NG_UBT_STAT_BYTES_RECV(sc->sc_stat, m->m_pkthdr.len);

		if (m->m_pkthdr.len < sizeof(*hdr)) {
			NG_UBT_INFO(sc, "Packet too short\n");
			goto done;
		}
		len = le16toh(hdr->length);

		if (len == (m->m_pkthdr.len - sizeof(*hdr))) {
			NG_UBT_INFO(sc, "Got complete ACL data frame, "
			    "pktlen=%d, length=%d\n",
			    m->m_pkthdr.len, len);

			NG_UBT_STAT_PCKTS_RECV(sc->sc_stat);

			NG_SEND_DATA_ONLY(error, sc->sc_hook, m);

			m = NULL;

			if (error != 0) {
				NG_UBT_STAT_IERROR(sc->sc_stat);
			}
		} else {
			NG_UBT_ERR(sc, "Invalid ACL frame size, "
			    "length=%d, pktlen=%d\n",
			    len, m->m_pkthdr.len);

			NG_UBT_STAT_IERROR(sc->sc_stat);
		}
	}
done:
	if (m) {
		NG_FREE_M(m);
	}
	/* start USB transfer if not already started */

	usb2_transfer_start(sc->sc_xfer_if_0[1]);

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
ubt_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct mbuf *m;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		NG_UBT_INFO(sc, "sent %d bytes to bulk-out "
		    "pipe\n", xfer->actlen);
		NG_UBT_STAT_BYTES_SENT(sc->sc_stat, xfer->actlen);
		NG_UBT_STAT_PCKTS_SENT(sc->sc_stat);

	case USB_ST_SETUP:
		if (sc->sc_flags & UBT_FLAG_WRITE_STALL) {
			usb2_transfer_start(sc->sc_xfer_if_0[4]);
			return;
		}
		/* get next mbuf, if any */

		NG_BT_MBUFQ_DEQUEUE(&sc->sc_aclq, m);

		if (m == NULL) {
			NG_UBT_INFO(sc, "ACL data queue is empty\n");
			return;
		}
		/*
		 * check ACL data frame size and
		 * copy it back to a linear USB
		 * transfer buffer:
		 */

		if (m->m_pkthdr.len > UBT_BULK_WRITE_BUFFER_SIZE) {
			panic("ACL data frame too big, size=%d, len=%d\n",
			    UBT_BULK_WRITE_BUFFER_SIZE,
			    m->m_pkthdr.len);
		}
		usb2_m_copy_in(xfer->frbuffers, 0, m, 0, m->m_pkthdr.len);

		NG_UBT_INFO(sc, "bulk-out transfer has been started, "
		    "len=%d\n", m->m_pkthdr.len);

		xfer->frlengths[0] = m->m_pkthdr.len;

		NG_FREE_M(m);

		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {

			NG_UBT_WARN(sc, "bulk-out transfer failed: %s\n",
			    usb2_errstr(xfer->error));

			NG_UBT_STAT_OERROR(sc->sc_stat);

			/* try to clear stall first */
			sc->sc_flags |= UBT_FLAG_WRITE_STALL;
			usb2_transfer_start(sc->sc_xfer_if_0[4]);
		}
		return;

	}
}

static void
ubt_bulk_write_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct usb2_xfer *xfer_other = sc->sc_xfer_if_0[0];

	if (usb2_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		sc->sc_flags &= ~UBT_FLAG_WRITE_STALL;
		usb2_transfer_start(xfer_other);
	}
	return;
}

static void
ubt_isoc_read_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	ng_hci_scodata_pkt_t hdr;
	struct mbuf *m;
	uint8_t *ptr;
	uint32_t max_len;
	uint32_t n;
	uint32_t offset;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
tr_transferred:
		offset = 0;

		for (n = 0; n < xfer->nframes; n++) {

			if (xfer->frlengths[n] >= sizeof(hdr)) {

				usb2_copy_out(xfer->frbuffers, offset,
				    &hdr, sizeof(hdr));

				if (hdr.length == (xfer->frlengths[n] - sizeof(hdr))) {

					NG_UBT_INFO(sc, "got complete SCO data "
					    "frame, length=%d\n", hdr.length);

					if (!NG_BT_MBUFQ_FULL(&sc->sc_sciq)) {

						/* allocate a new mbuf */

						MGETHDR(m, M_DONTWAIT, MT_DATA);

						if (m == NULL) {
							goto tr_setup;
						}
						MCLGET(m, M_DONTWAIT);

						if (!(m->m_flags & M_EXT)) {
							NG_FREE_M(m);
							goto tr_setup;
						}
						/*
						 * fix SCO data frame header
						 * if required
						 */

						if (!(sc->sc_flags & UBT_HAVE_FRAME_TYPE)) {
							*mtod(m, uint8_t *)= NG_HCI_SCO_DATA_PKT;
							m->m_pkthdr.len = m->m_len = 1;
						} else {
							m->m_pkthdr.len = m->m_len = 0;
						}

						max_len = (MCLBYTES - m->m_len);

						if (xfer->frlengths[n] > max_len) {
							xfer->frlengths[n] = max_len;
						}
						ptr = ((uint8_t *)(m->m_data)) + m->m_len;

						usb2_copy_out
						    (xfer->frbuffers, offset,
						    ptr, xfer->frlengths[n]);

						m->m_pkthdr.len += xfer->frlengths[n];
						m->m_len += xfer->frlengths[n];

						NG_BT_MBUFQ_ENQUEUE(&sc->sc_sciq, m);
					}
				}
			}
			offset += xfer->max_frame_size;
		}

	case USB_ST_SETUP:
tr_setup:

		if (NG_BT_MBUFQ_LEN(&sc->sc_sciq) > 0) {
			ng_send_fn(sc->sc_node, NULL, ubt_isoc_read_complete, NULL, 0);
		}
		for (n = 0; n < xfer->nframes; n++) {
			xfer->frlengths[n] = xfer->max_frame_size;
		}

		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			/* ignore */
			return;
		}
		goto tr_transferred;
	}
}

static void
ubt_isoc_read_complete(node_p node, hook_p hook, void *arg1, int arg2)
{
	ubt_softc_p sc = NG_NODE_PRIVATE(node);
	struct mbuf *m;
	int error;

	if (sc == NULL) {
		return;
	}
	mtx_lock(&sc->sc_mtx);

	while (1) {

		NG_BT_MBUFQ_DEQUEUE(&sc->sc_sciq, m);

		if (m == NULL) {
			break;
		}
		if (sc->sc_hook == NULL || NG_HOOK_NOT_VALID(sc->sc_hook)) {
			NG_UBT_INFO(sc, "No upstream hook\n");
			goto done;
		}
		NG_UBT_INFO(sc, "Got complete SCO data frame, "
		    "pktlen=%d bytes\n", m->m_pkthdr.len);

		NG_UBT_STAT_PCKTS_RECV(sc->sc_stat);
		NG_UBT_STAT_BYTES_RECV(sc->sc_stat, m->m_pkthdr.len);

		NG_SEND_DATA_ONLY(error, sc->sc_hook, m);

		m = NULL;

		if (error) {
			NG_UBT_STAT_IERROR(sc->sc_stat);
		}
done:
		if (m) {
			NG_FREE_M(m);
		}
	}

	mtx_unlock(&sc->sc_mtx);

	return;
}

static void
ubt_isoc_write_callback(struct usb2_xfer *xfer)
{
	struct ubt_softc *sc = xfer->priv_sc;
	struct mbuf *m;
	uint32_t n;
	uint32_t len;
	uint32_t offset;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
tr_transferred:
		if (xfer->error) {
			NG_UBT_STAT_OERROR(sc->sc_stat);
		} else {
			NG_UBT_STAT_BYTES_SENT(sc->sc_stat, xfer->actlen);
			NG_UBT_STAT_PCKTS_SENT(sc->sc_stat);
		}

	case USB_ST_SETUP:
		offset = 0;

		for (n = 0; n < xfer->nframes; n++) {

			NG_BT_MBUFQ_DEQUEUE(&sc->sc_scoq, m);

			if (m) {
				len = min(xfer->max_frame_size, m->m_pkthdr.len);

				usb2_m_copy_in(xfer->frbuffers, offset, m, 0, len);

				NG_FREE_M(m);

				xfer->frlengths[n] = len;
				offset += len;
			} else {
				xfer->frlengths[n] = 0;
			}
		}

		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error == USB_ERR_CANCELLED) {
			/* ignore */
			return;
		}
		goto tr_transferred;
	}
}

/****************************************************************************
 ****************************************************************************
 **                        Netgraph specific
 ****************************************************************************
 ****************************************************************************/

/*
 * Netgraph node constructor.
 * Do not allow to create node of this type:
 */

static int
ng_ubt_constructor(node_p node)
{
	return (EINVAL);
}

/*
 * Netgraph node destructor.
 * Destroy node only when device has been detached:
 */

static int
ng_ubt_shutdown(node_p node)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(node);

	/* Let old node go */
	NG_NODE_SET_PRIVATE(node, NULL);
	NG_NODE_UNREF(node);

	if (sc == NULL) {
		goto done;
	}
	mtx_lock(&sc->sc_mtx);

	/* Create Netgraph node */
	if (ng_make_node_common(&typestruct, &sc->sc_node) != 0) {
		printf("%s: Could not create Netgraph node\n",
		    sc->sc_name);
		sc->sc_node = NULL;
		goto done;
	}
	/* Name node */
	if (ng_name_node(sc->sc_node, sc->sc_name) != 0) {
		printf("%s: Could not name Netgraph node\n",
		    sc->sc_name);
		NG_NODE_UNREF(sc->sc_node);
		sc->sc_node = NULL;
		goto done;
	}
	NG_NODE_SET_PRIVATE(sc->sc_node, sc);
	NG_NODE_FORCE_WRITER(sc->sc_node);

done:
	if (sc) {
		mtx_unlock(&sc->sc_mtx);
	}
	return (0);
}

/*
 * Create new hook.
 * There can only be one.
 */

static int
ng_ubt_newhook(node_p node, hook_p hook, char const *name)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(node);
	int error = 0;

	if (strcmp(name, NG_UBT_HOOK) != 0) {
		return (EINVAL);
	}
	mtx_lock(&sc->sc_mtx);

	if (sc->sc_hook != NULL) {
		error = EISCONN;
	} else {
		sc->sc_hook = hook;
	}

	mtx_unlock(&sc->sc_mtx);

	return (error);
}

/*
 * Connect hook.
 * Start incoming USB transfers
 */

static int
ng_ubt_connect(hook_p hook)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));

	NG_HOOK_FORCE_QUEUE(NG_HOOK_PEER(hook));

	mtx_lock(&sc->sc_mtx);

	sc->sc_flags |= (UBT_FLAG_READ_STALL |
	    UBT_FLAG_WRITE_STALL |
	    UBT_FLAG_INTR_STALL);

	/* start intr transfer */
	usb2_transfer_start(sc->sc_xfer_if_0[2]);

	/* start bulk-in transfer */
	usb2_transfer_start(sc->sc_xfer_if_0[1]);

	/* start bulk-out transfer */
	usb2_transfer_start(sc->sc_xfer_if_0[0]);

	/* start control-out transfer */
	usb2_transfer_start(sc->sc_xfer_if_0[3]);
#if 0
	XXX can enable this XXX

	/* start isoc-in transfer */
	     usb2_transfer_start(sc->sc_xfer_if_1[0]);

	usb2_transfer_start(sc->sc_xfer_if_1[1]);

	/* start isoc-out transfer */
	usb2_transfer_start(sc->sc_xfer_if_1[2]);
	usb2_transfer_start(sc->sc_xfer_if_1[3]);
#endif

	mtx_unlock(&sc->sc_mtx);

	return (0);
}

/*
 * Disconnect hook
 */

static int
ng_ubt_disconnect(hook_p hook)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	int error = 0;

	if (sc != NULL) {

		mtx_lock(&sc->sc_mtx);

		if (hook != sc->sc_hook) {
			error = EINVAL;
		} else {

			/* stop intr transfer */
			usb2_transfer_stop(sc->sc_xfer_if_0[2]);
			usb2_transfer_stop(sc->sc_xfer_if_0[6]);

			/* stop bulk-in transfer */
			usb2_transfer_stop(sc->sc_xfer_if_0[1]);
			usb2_transfer_stop(sc->sc_xfer_if_0[5]);

			/* stop bulk-out transfer */
			usb2_transfer_stop(sc->sc_xfer_if_0[0]);
			usb2_transfer_stop(sc->sc_xfer_if_0[4]);

			/* stop control transfer */
			usb2_transfer_stop(sc->sc_xfer_if_0[3]);

			/* stop isoc-in transfer */
			usb2_transfer_stop(sc->sc_xfer_if_1[0]);
			usb2_transfer_stop(sc->sc_xfer_if_1[1]);

			/* stop isoc-out transfer */
			usb2_transfer_stop(sc->sc_xfer_if_1[2]);
			usb2_transfer_stop(sc->sc_xfer_if_1[3]);

			/* cleanup queues */
			NG_BT_MBUFQ_DRAIN(&sc->sc_cmdq);
			NG_BT_MBUFQ_DRAIN(&sc->sc_aclq);
			NG_BT_MBUFQ_DRAIN(&sc->sc_scoq);
			NG_BT_MBUFQ_DRAIN(&sc->sc_sciq);

			sc->sc_hook = NULL;
		}

		mtx_unlock(&sc->sc_mtx);
	}
	return (error);
}

/*
 * Process control message
 */

static int
ng_ubt_rcvmsg(node_p node, item_p item, hook_p lasthook)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(node);
	struct ng_mesg *msg = NULL, *rsp = NULL;
	struct ng_bt_mbufq *q = NULL;
	int error = 0, queue, qlen;

	if (sc == NULL) {
		NG_FREE_ITEM(item);
		return (EHOSTDOWN);
	}
	mtx_lock(&sc->sc_mtx);

	NGI_GET_MSG(item, msg);

	switch (msg->header.typecookie) {
	case NGM_GENERIC_COOKIE:
		switch (msg->header.cmd) {
		case NGM_TEXT_STATUS:
			NG_MKRESPONSE(rsp, msg, NG_TEXTRESPONSE, M_NOWAIT);
			if (rsp == NULL)
				error = ENOMEM;
			else
				snprintf(rsp->data, NG_TEXTRESPONSE,
				    "Hook: %s\n" \
				    "Flags: %#x\n" \
				    "Debug: %d\n" \
				    "CMD queue: [have:%d,max:%d]\n" \
				    "ACL queue: [have:%d,max:%d]\n" \
				    "SCO queue: [have:%d,max:%d]",
				    (sc->sc_hook != NULL) ? NG_UBT_HOOK : "",
				    sc->sc_flags,
				    sc->sc_debug,
				    NG_BT_MBUFQ_LEN(&sc->sc_cmdq),
				    sc->sc_cmdq.maxlen,
				    NG_BT_MBUFQ_LEN(&sc->sc_aclq),
				    sc->sc_aclq.maxlen,
				    NG_BT_MBUFQ_LEN(&sc->sc_scoq),
				    sc->sc_scoq.maxlen);
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	case NGM_UBT_COOKIE:
		switch (msg->header.cmd) {
		case NGM_UBT_NODE_SET_DEBUG:
			if (msg->header.arglen != sizeof(ng_ubt_node_debug_ep))
				error = EMSGSIZE;
			else
				sc->sc_debug =
				    *((ng_ubt_node_debug_ep *) (msg->data));
			break;

		case NGM_UBT_NODE_GET_DEBUG:
			NG_MKRESPONSE(rsp, msg, sizeof(ng_ubt_node_debug_ep),
			    M_NOWAIT);
			if (rsp == NULL)
				error = ENOMEM;
			else
				*((ng_ubt_node_debug_ep *) (rsp->data)) =
				    sc->sc_debug;
			break;

		case NGM_UBT_NODE_SET_QLEN:
			if (msg->header.arglen != sizeof(ng_ubt_node_qlen_ep))
				error = EMSGSIZE;
			else {
				queue = ((ng_ubt_node_qlen_ep *)
				    (msg->data))->queue;
				qlen = ((ng_ubt_node_qlen_ep *)
				    (msg->data))->qlen;

				if (qlen <= 0) {
					error = EINVAL;
					break;
				}
				switch (queue) {
				case NGM_UBT_NODE_QUEUE_CMD:
					q = &sc->sc_cmdq;
					break;

				case NGM_UBT_NODE_QUEUE_ACL:
					q = &sc->sc_aclq;
					break;

				case NGM_UBT_NODE_QUEUE_SCO:
					q = &sc->sc_scoq;
					break;

				default:
					q = NULL;
					error = EINVAL;
					break;
				}

				if (q != NULL) {
					q->maxlen = qlen;
				}
			}
			break;

		case NGM_UBT_NODE_GET_QLEN:
			if (msg->header.arglen != sizeof(ng_ubt_node_qlen_ep)) {
				error = EMSGSIZE;
				break;
			}
			queue = ((ng_ubt_node_qlen_ep *) (msg->data))->queue;
			switch (queue) {
			case NGM_UBT_NODE_QUEUE_CMD:
				q = &sc->sc_cmdq;
				break;

			case NGM_UBT_NODE_QUEUE_ACL:
				q = &sc->sc_aclq;
				break;

			case NGM_UBT_NODE_QUEUE_SCO:
				q = &sc->sc_scoq;
				break;

			default:
				q = NULL;
				error = EINVAL;
				break;
			}

			if (q != NULL) {
				NG_MKRESPONSE(rsp, msg,
				    sizeof(ng_ubt_node_qlen_ep), M_NOWAIT);
				if (rsp == NULL) {
					error = ENOMEM;
					break;
				}
				((ng_ubt_node_qlen_ep *) (rsp->data))->queue =
				    queue;
				((ng_ubt_node_qlen_ep *) (rsp->data))->qlen =
				    q->maxlen;
			}
			break;

		case NGM_UBT_NODE_GET_STAT:
			NG_MKRESPONSE(rsp, msg, sizeof(ng_ubt_node_stat_ep),
			    M_NOWAIT);
			if (rsp == NULL)
				error = ENOMEM;
			else {
				bcopy(&sc->sc_stat, rsp->data,
				    sizeof(ng_ubt_node_stat_ep));
			}
			break;

		case NGM_UBT_NODE_RESET_STAT:

			NG_UBT_STAT_RESET(sc->sc_stat);
			break;

		default:
			error = EINVAL;
			break;
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	NG_RESPOND_MSG(error, node, item, rsp);
	NG_FREE_MSG(msg);

	mtx_unlock(&sc->sc_mtx);

	return (error);
}

/*
 * Process data
 */

static int
ng_ubt_rcvdata(hook_p hook, item_p item)
{
	struct ubt_softc *sc = NG_NODE_PRIVATE(NG_HOOK_NODE(hook));
	struct mbuf *m;
	struct ng_bt_mbufq *q;
	struct usb2_xfer *xfer;
	int error = 0;

	if (sc == NULL) {
		error = EHOSTDOWN;
		goto done;
	}
	mtx_lock(&sc->sc_mtx);

	if (hook != sc->sc_hook) {
		error = EINVAL;
		goto done;
	}
	/* deatch mbuf and get HCI frame type */
	NGI_GET_M(item, m);

	/* process HCI frame */
	switch (*mtod(m, uint8_t *)) {	/* XXX call m_pullup ? */
	case NG_HCI_CMD_PKT:
		xfer = sc->sc_xfer_if_0[3];
		q = &sc->sc_cmdq;
		break;

	case NG_HCI_ACL_DATA_PKT:
		xfer = sc->sc_xfer_if_0[0];
		q = &sc->sc_aclq;
		break;

	case NG_HCI_SCO_DATA_PKT:
		xfer = NULL;
		q = &sc->sc_scoq;
		break;

	default:
		NG_UBT_ERR(sc, "Dropping unsupported HCI frame, "
		    "type=0x%02x, pktlen=%d\n",
		    *mtod(m, uint8_t *),
		    m->m_pkthdr.len);

		NG_FREE_M(m);
		error = EINVAL;
		goto done;
	}

	/* loose frame type, if required */
	if (!(sc->sc_flags & UBT_NEED_FRAME_TYPE)) {
		m_adj(m, sizeof(uint8_t));
	}
	if (NG_BT_MBUFQ_FULL(q)) {
		NG_UBT_ERR(sc, "Dropping HCI frame 0x%02x, len=%d. "
		    "Queue full\n", *mtod(m, uint8_t *),
		    m->m_pkthdr.len);
		NG_FREE_M(m);
	} else {
		NG_BT_MBUFQ_ENQUEUE(q, m);
	}

	if (xfer) {
		usb2_transfer_start(xfer);
	}
done:
	NG_FREE_ITEM(item);

	if (sc) {
		mtx_unlock(&sc->sc_mtx);
	}
	return (error);
}
