/*-
 * Copyright (c) 2017 Microsoft Corp.
 * Copyright (c) 2023 Yuri <yuri@aetern.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cdefs.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/evdev/input.h>

#include <dev/hid/hid.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus_xact.h>
#include <dev/hyperv/utilities/hv_utilreg.h>
#include <dev/hyperv/utilities/vmbus_icreg.h>
#include <dev/hyperv/utilities/vmbus_icvar.h>

#include "hid_if.h"
#include "vmbus_if.h"

#define	HV_HID_VER_MAJOR	2
#define	HV_HID_VER_MINOR	0
#define	HV_HID_VER		(HV_HID_VER_MINOR | (HV_HID_VER_MAJOR) << 16)

#define	HV_BUFSIZ		(4 * PAGE_SIZE)
#define	HV_HID_RINGBUFF_SEND_SZ	(10 * PAGE_SIZE)
#define	HV_HID_RINGBUFF_RECV_SZ	(10 * PAGE_SIZE)

typedef struct {
	device_t		dev;
	struct mtx		mtx;
	/* vmbus */
	struct vmbus_channel	*hs_chan;
	struct vmbus_xact_ctx	*hs_xact_ctx;
	uint8_t			*buf;
	int			buflen;
	/* hid */
	struct hid_device_info	hdi;
	hid_intr_t		*intr;
	bool			intr_on;
	void			*intr_ctx;
	uint8_t			*rdesc;
} hv_hid_sc;

typedef enum {
	SH_PROTO_REQ,
	SH_PROTO_RESP,
	SH_DEVINFO,
	SH_DEVINFO_ACK,
	SH_INPUT_REPORT,
} sh_msg_type;

typedef struct {
	sh_msg_type	type;
	uint32_t	size;
} __packed sh_msg_hdr;

typedef struct {
	sh_msg_hdr	hdr;
	char		data[];
} __packed sh_msg;

typedef struct {
	sh_msg_hdr	hdr;
	uint32_t	ver;
} __packed sh_proto_req;

typedef struct {
	sh_msg_hdr	hdr;
	uint32_t	ver;
	uint32_t	app;
} __packed sh_proto_resp;

typedef struct {
	u_int		size;
	u_short		vendor;
	u_short		product;
	u_short		version;
	u_short		reserved[11];
} __packed sh_devinfo;

/* Copied from linux/hid.h */
typedef struct {
	uint8_t		bDescriptorType;
	uint16_t	wDescriptorLength;
} __packed sh_hcdesc;

typedef struct {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint16_t	bcdHID;
	uint8_t		bCountryCode;
	uint8_t		bNumDescriptors;
	sh_hcdesc	hcdesc[1];
} __packed sh_hdesc;

typedef struct {
	sh_msg_hdr	hdr;
	sh_devinfo	devinfo;
	sh_hdesc	hdesc;
} __packed sh_devinfo_resp;

typedef struct {
	sh_msg_hdr	hdr;
	uint8_t		rsvd;
} __packed sh_devinfo_ack;

typedef struct {
	sh_msg_hdr	hdr;
	char		buffer[];
} __packed sh_input_report;

typedef enum {
	HV_HID_MSG_INVALID,
	HV_HID_MSG_DATA,
} hv_hid_msg_type;

typedef struct {
	hv_hid_msg_type	type;
	uint32_t	size;
	char		data[];
} hv_hid_pmsg;

typedef struct {
	hv_hid_msg_type	type;
	uint32_t	size;
	union {
		sh_msg		msg;
		sh_proto_req	req;
		sh_proto_resp	resp;
		sh_devinfo_resp	dresp;
		sh_devinfo_ack	ack;
		sh_input_report	irep;
	};
} hv_hid_msg;

#define	HV_HID_REQ_SZ	(sizeof(hv_hid_pmsg) + sizeof(sh_proto_req))
#define	HV_HID_RESP_SZ	(sizeof(hv_hid_pmsg) + sizeof(sh_proto_resp))
#define	HV_HID_ACK_SZ	(sizeof(hv_hid_pmsg) + sizeof(sh_devinfo_ack))

/* Somewhat arbitrary, enough to get the devinfo response */
#define	HV_HID_REQ_MAX	256
#define	HV_HID_RESP_MAX	256

static const struct vmbus_ic_desc vmbus_hid_descs[] = {
	{
		.ic_guid = { .hv_guid = {
		    0x9e, 0xb6, 0xa8, 0xcf, 0x4a, 0x5b, 0xc0, 0x4c,
		    0xb9, 0x8b, 0x8b, 0xa1, 0xa1, 0xf3, 0xf9, 0x5a} },
		.ic_desc = "Hyper-V HID device"
	},
	VMBUS_IC_DESC_END
};

/* TODO: add GUID support to devmatch(8) to export vmbus_hid_descs directly */
const struct {
	char *guid;
} vmbus_hid_descs_pnp[] = {{ "cfa8b69e-5b4a-4cc0-b98b-8ba1a1f3f95a" }};

static int hv_hid_attach(device_t dev);
static int hv_hid_detach(device_t dev);

static int
hv_hid_connect_vsp(hv_hid_sc *sc)
{
	struct vmbus_xact	*xact;
	hv_hid_msg		*req;
	const hv_hid_msg	*resp;
	size_t			resplen;
	int			ret;

	xact = vmbus_xact_get(sc->hs_xact_ctx, HV_HID_REQ_SZ);
	if (xact == NULL) {
		device_printf(sc->dev, "no xact for init");
		return (ENODEV);
	}
	req = vmbus_xact_req_data(xact);
	req->type = HV_HID_MSG_DATA;
	req->size = sizeof(sh_proto_req);
	req->req.hdr.type = SH_PROTO_REQ;
	req->req.hdr.size = sizeof(u_int);
	req->req.ver = HV_HID_VER;

	vmbus_xact_activate(xact);
	ret = vmbus_chan_send(sc->hs_chan,
	    VMBUS_CHANPKT_TYPE_INBAND,
	    VMBUS_CHANPKT_FLAG_RC,
	    req, HV_HID_REQ_SZ, (uint64_t)(uintptr_t)xact);
	if (ret != 0) {
		device_printf(sc->dev, "failed to send proto req\n");
		vmbus_xact_deactivate(xact);
		return (ret);
	}
	resp = vmbus_chan_xact_wait(sc->hs_chan, xact, &resplen, true);
	if (resplen != HV_HID_RESP_SZ || !resp->resp.app) {
		device_printf(sc->dev, "proto req failed\n");
		ret = ENODEV;
	}

	vmbus_xact_put(xact);
	return (ret);
}

static void
hv_hid_receive(hv_hid_sc *sc, struct vmbus_chanpkt_hdr *pkt)
{
	const hv_hid_msg	*msg;
	sh_msg_type		msg_type;
	uint32_t		msg_len;
	void			*rdesc;

	msg = VMBUS_CHANPKT_CONST_DATA(pkt);
	msg_len = VMBUS_CHANPKT_DATALEN(pkt);

	if (msg->type != HV_HID_MSG_DATA)
		return;

	if (msg_len <= sizeof(hv_hid_pmsg)) {
		device_printf(sc->dev, "invalid packet length\n");
		return;
	}
	msg_type = msg->msg.hdr.type;
	switch (msg_type) {
	case SH_PROTO_RESP: {
		struct vmbus_xact_ctx *xact_ctx;

		xact_ctx = sc->hs_xact_ctx;
		if (xact_ctx != NULL) {
			vmbus_xact_ctx_wakeup(xact_ctx,
			    VMBUS_CHANPKT_CONST_DATA(pkt),
			    VMBUS_CHANPKT_DATALEN(pkt));
		}
		break;
	}
	case SH_DEVINFO: {
		struct vmbus_xact	*xact;
		struct hid_device_info	*hdi;
		hv_hid_msg		ack;
		const sh_devinfo	*devinfo;
		const sh_hdesc		*hdesc;

		/* Send ack */
		ack.type = HV_HID_MSG_DATA;
		ack.size = sizeof(sh_devinfo_ack);
		ack.ack.hdr.type = SH_DEVINFO_ACK;
		ack.ack.hdr.size = 1;
		ack.ack.rsvd = 0;

		xact = vmbus_xact_get(sc->hs_xact_ctx, HV_HID_ACK_SZ);
		if (xact == NULL)
			break;
		vmbus_xact_activate(xact);
		(void) vmbus_chan_send(sc->hs_chan, VMBUS_CHANPKT_TYPE_INBAND,
		    0, &ack, HV_HID_ACK_SZ, (uint64_t)(uintptr_t)xact);
		vmbus_xact_deactivate(xact);
		vmbus_xact_put(xact);

		/* Check for resume from hibernation */
		if (sc->rdesc != NULL)
			break;

		/* Parse devinfo response */
		devinfo = &msg->dresp.devinfo;
		hdesc = &msg->dresp.hdesc;
		if (hdesc->bLength == 0)
			break;
		hdi = &sc->hdi;
		memset(hdi, 0, sizeof(*hdi));
		hdi->rdescsize = le16toh(hdesc->hcdesc[0].wDescriptorLength);
		if (hdi->rdescsize == 0)
			break;
		strlcpy(hdi->name, "Hyper-V", sizeof(hdi->name));
		hdi->idBus = BUS_VIRTUAL;
		hdi->idVendor = le16toh(devinfo->vendor);
		hdi->idProduct = le16toh(devinfo->product);
		hdi->idVersion = le16toh(devinfo->version);
		/* Save rdesc copy */
		rdesc = malloc(hdi->rdescsize, M_DEVBUF, M_WAITOK | M_ZERO);
		memcpy(rdesc, (const uint8_t *)hdesc + hdesc->bLength,
		    hdi->rdescsize);
		mtx_lock(&sc->mtx);
		sc->rdesc = rdesc;
		wakeup(sc);
		mtx_unlock(&sc->mtx);
		break;
	}
	case SH_INPUT_REPORT: {
		mtx_lock(&sc->mtx);
		if (sc->intr != NULL && sc->intr_on)
			sc->intr(sc->intr_ctx,
			    __DECONST(void *, msg->irep.buffer),
			    msg->irep.hdr.size);
		mtx_unlock(&sc->mtx);
		break;
	}
	default:
		break;
	}
}

static void
hv_hid_read_channel(struct vmbus_channel *channel, void *ctx)
{
	hv_hid_sc	*sc;
	uint8_t		*buf;
	int		buflen;
	int		ret;

	sc = ctx;
	buf = sc->buf;
	buflen = sc->buflen;
	for (;;) {
		struct vmbus_chanpkt_hdr *pkt;
		int rcvd;

		pkt = (struct vmbus_chanpkt_hdr *)buf;
		rcvd = buflen;
		ret = vmbus_chan_recv_pkt(channel, pkt, &rcvd);
		if (__predict_false(ret == ENOBUFS)) {
			buflen = sc->buflen * 2;
			while (buflen < rcvd)
				buflen *= 2;
			buf = malloc(buflen, M_DEVBUF, M_WAITOK | M_ZERO);
			device_printf(sc->dev, "expand recvbuf %d -> %d\n",
			    sc->buflen, buflen);
			free(sc->buf, M_DEVBUF);
			sc->buf = buf;
			sc->buflen = buflen;
			continue;
		} else if (__predict_false(ret == EAGAIN)) {
			/* No more channel packets; done! */
			break;
		}
		KASSERT(ret == 0, ("vmbus_chan_recv_pkt failed: %d", ret));

		switch (pkt->cph_type) {
		case VMBUS_CHANPKT_TYPE_COMP:
		case VMBUS_CHANPKT_TYPE_RXBUF:
			device_printf(sc->dev, "unhandled event: %d\n",
			    pkt->cph_type);
			break;
		case VMBUS_CHANPKT_TYPE_INBAND:
			hv_hid_receive(sc, pkt);
			break;
		default:
			device_printf(sc->dev, "unknown event: %d\n",
			    pkt->cph_type);
			break;
		}
	}
}

static int
hv_hid_probe(device_t dev)
{
	device_t			bus;
	const struct vmbus_ic_desc	*d;

	if (resource_disabled(device_get_name(dev), 0))
		return (ENXIO);

	bus = device_get_parent(dev);
	for (d = vmbus_hid_descs; d->ic_desc != NULL; ++d) {
		if (VMBUS_PROBE_GUID(bus, dev, &d->ic_guid) == 0) {
			device_set_desc(dev, d->ic_desc);
			return (BUS_PROBE_DEFAULT);
		}
	}

	return (ENXIO);
}

static int
hv_hid_attach(device_t dev)
{
	device_t	child;
	hv_hid_sc	*sc;
	int		ret;

	sc = device_get_softc(dev);
	sc->dev = dev;
	mtx_init(&sc->mtx, "hvhid lock", NULL, MTX_DEF);
	sc->hs_chan = vmbus_get_channel(dev);
	sc->hs_xact_ctx = vmbus_xact_ctx_create(bus_get_dma_tag(dev),
	    HV_HID_REQ_MAX, HV_HID_RESP_MAX, 0);
	if (sc->hs_xact_ctx == NULL) {
		ret = ENOMEM;
		goto out;
	}
	sc->buflen = HV_BUFSIZ;
	sc->buf = malloc(sc->buflen, M_DEVBUF, M_WAITOK | M_ZERO);
	vmbus_chan_set_readbatch(sc->hs_chan, false);
	ret = vmbus_chan_open(sc->hs_chan, HV_HID_RINGBUFF_SEND_SZ,
	    HV_HID_RINGBUFF_RECV_SZ, NULL, 0, hv_hid_read_channel, sc);
	if (ret != 0)
		goto out;
	ret = hv_hid_connect_vsp(sc);
	if (ret != 0)
		goto out;

	/* Wait until we have devinfo (or arbitrary timeout of 3s) */
	mtx_lock(&sc->mtx);
	if (sc->rdesc == NULL)
		ret = mtx_sleep(sc, &sc->mtx, 0, "hvhid", hz * 3);
	mtx_unlock(&sc->mtx);
	if (ret != 0) {
		ret = ENODEV;
		goto out;
	}
	child = device_add_child(sc->dev, "hidbus", DEVICE_UNIT_ANY);
	if (child == NULL) {
		device_printf(sc->dev, "failed to add hidbus\n");
		ret = ENOMEM;
		goto out;
	}
	device_set_ivars(child, &sc->hdi);
	bus_attach_children(dev);
out:
	if (ret != 0)
		hv_hid_detach(dev);
	return (ret);
}

static int
hv_hid_detach(device_t dev)
{
	hv_hid_sc	*sc;
	int		ret;

	sc = device_get_softc(dev);
	ret = bus_generic_detach(dev);
	if (ret != 0)
		return (ret);
	if (sc->hs_xact_ctx != NULL)
		vmbus_xact_ctx_destroy(sc->hs_xact_ctx);
	vmbus_chan_close(vmbus_get_channel(dev));
	free(sc->buf, M_DEVBUF);
	free(sc->rdesc, M_DEVBUF);
	mtx_destroy(&sc->mtx);

	return (0);
}

static void
hv_hid_intr_setup(device_t dev, device_t child __unused, hid_intr_t intr,
    void *ctx, struct hid_rdesc_info *rdesc)
{
	hv_hid_sc	*sc;

	if (intr == NULL)
		return;

	sc = device_get_softc(dev);
	sc->intr = intr;
	sc->intr_on = false;
	sc->intr_ctx = ctx;
	rdesc->rdsize = rdesc->isize;
}

static void
hv_hid_intr_unsetup(device_t dev, device_t child __unused)
{
	hv_hid_sc	*sc;

	sc = device_get_softc(dev);
	sc->intr = NULL;
	sc->intr_on = false;
	sc->intr_ctx = NULL;
}

static int
hv_hid_intr_start(device_t dev, device_t child __unused)
{
	hv_hid_sc	*sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
	sc->intr_on = true;
	mtx_unlock(&sc->mtx);
	return (0);
}

static int
hv_hid_intr_stop(device_t dev, device_t child __unused)
{
	hv_hid_sc	*sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
	sc->intr_on = false;
	mtx_unlock(&sc->mtx);
	return (0);
}

static int
hv_hid_get_rdesc(device_t dev, device_t child __unused, void *buf,
    hid_size_t len)
{
	hv_hid_sc	*sc;

	sc = device_get_softc(dev);
	if (len < sc->hdi.rdescsize)
		return (EMSGSIZE);
	memcpy(buf, sc->rdesc, len);
	return (0);
}

static device_method_t hv_hid_methods[] = {
	DEVMETHOD(device_probe,		hv_hid_probe),
	DEVMETHOD(device_attach,	hv_hid_attach),
	DEVMETHOD(device_detach,	hv_hid_detach),

	DEVMETHOD(hid_intr_setup,	hv_hid_intr_setup),
	DEVMETHOD(hid_intr_unsetup,	hv_hid_intr_unsetup),
	DEVMETHOD(hid_intr_start,	hv_hid_intr_start),
	DEVMETHOD(hid_intr_stop,	hv_hid_intr_stop),

	DEVMETHOD(hid_get_rdesc,	hv_hid_get_rdesc),
	DEVMETHOD_END,
};

static driver_t hv_hid_driver = {
	.name = "hvhid",
	.methods = hv_hid_methods,
	.size = sizeof(hv_hid_sc),
};

DRIVER_MODULE(hv_hid, vmbus, hv_hid_driver, NULL, NULL);
MODULE_VERSION(hv_hid, 1);
MODULE_DEPEND(hv_hid, hidbus, 1, 1, 1);
MODULE_DEPEND(hv_hid, hms, 1, 1, 1);
MODULE_DEPEND(hv_hid, vmbus, 1, 1, 1);
MODULE_PNP_INFO("Z:classid", vmbus, hv_hid, vmbus_hid_descs_pnp,
    nitems(vmbus_hid_descs_pnp));
