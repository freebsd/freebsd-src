/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Original copyright (c) 2016 genua mbH (OpenBSD version)
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Copyright (c) 2022 ADISTA SAS (re-write for FreeBSD)
 *
 * Re-write for FreeBSD by Pierre Pronchery <pierre@defora.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the copyright holder nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: if_umb.c,v 1.5 2018/09/20 09:45:16 khorben Exp $
 * $OpenBSD: if_umb.c,v 1.18 2018/02/19 08:59:52 mpi Exp $
 */

/*
 * Mobile Broadband Interface Model specification:
 * http://www.usb.org/developers/docs/devclass_docs/MBIM10Errata1_073013.zip
 * Compliance testing guide
 * http://www.usb.org/developers/docs/devclass_docs/MBIM-Compliance-1.0.pdf
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/taskqueue.h>

#include <machine/_inttypes.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <dev/usb/usb.h>
#include <dev/usb/usb_cdc.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usbdi_util.h>
#include "usb_if.h"

#include "mbim.h"
#include "if_umbreg.h"

MALLOC_DECLARE(M_MBIM_CID_CONNECT);
MALLOC_DEFINE(M_MBIM_CID_CONNECT, "mbim_cid_connect",
		"Connection parameters for MBIM");

#ifdef UMB_DEBUG
#define DPRINTF(x...)							\
		do { if (umb_debug) log(LOG_DEBUG, x); } while (0)

#define DPRINTFN(n, x...)						\
		do { if (umb_debug >= (n)) log(LOG_DEBUG, x); } while (0)

#define DDUMPN(n, b, l)							\
		do {							\
			if (umb_debug >= (n))				\
				umb_dump((b), (l));			\
		} while (0)

const int	 umb_debug = 1;
static char	*umb_uuid2str(uint8_t [MBIM_UUID_LEN]);
static void	 umb_dump(void *, int);

#else
#define DPRINTF(x...)		do { } while (0)
#define DPRINTFN(n, x...)	do { } while (0)
#define DDUMPN(n, b, l)		do { } while (0)
#endif

#define DEVNAM(sc)		device_get_nameunit((sc)->sc_dev)

/*
 * State change timeout
 */
#define UMB_STATE_CHANGE_TIMEOUT	30

/*
 * State change flags
 */
#define UMB_NS_DONT_DROP	0x0001	/* do not drop below current state */
#define UMB_NS_DONT_RAISE	0x0002	/* do not raise below current state */

/*
 * Diagnostic macros
 */
const struct umb_valdescr umb_regstates[] = MBIM_REGSTATE_DESCRIPTIONS;
const struct umb_valdescr umb_dataclasses[] = MBIM_DATACLASS_DESCRIPTIONS;
const struct umb_valdescr umb_simstate[] = MBIM_SIMSTATE_DESCRIPTIONS;
const struct umb_valdescr umb_messages[] = MBIM_MESSAGES_DESCRIPTIONS;
const struct umb_valdescr umb_status[] = MBIM_STATUS_DESCRIPTIONS;
const struct umb_valdescr umb_cids[] = MBIM_CID_DESCRIPTIONS;
const struct umb_valdescr umb_pktstate[] = MBIM_PKTSRV_STATE_DESCRIPTIONS;
const struct umb_valdescr umb_actstate[] = MBIM_ACTIVATION_STATE_DESCRIPTIONS;
const struct umb_valdescr umb_error[] = MBIM_ERROR_DESCRIPTIONS;
const struct umb_valdescr umb_pintype[] = MBIM_PINTYPE_DESCRIPTIONS;
const struct umb_valdescr umb_istate[] = UMB_INTERNAL_STATE_DESCRIPTIONS;

#define umb_regstate(c)		umb_val2descr(umb_regstates, (c))
#define umb_dataclass(c)	umb_val2descr(umb_dataclasses, (c))
#define umb_simstate(s)		umb_val2descr(umb_simstate, (s))
#define umb_request2str(m)	umb_val2descr(umb_messages, (m))
#define umb_status2str(s)	umb_val2descr(umb_status, (s))
#define umb_cid2str(c)		umb_val2descr(umb_cids, (c))
#define umb_packet_state(s)	umb_val2descr(umb_pktstate, (s))
#define umb_activation(s)	umb_val2descr(umb_actstate, (s))
#define umb_error2str(e)	umb_val2descr(umb_error, (e))
#define umb_pin_type(t)		umb_val2descr(umb_pintype, (t))
#define umb_istate(s)		umb_val2descr(umb_istate, (s))

static device_probe_t umb_probe;
static device_attach_t umb_attach;
static device_detach_t umb_detach;
static device_suspend_t umb_suspend;
static device_resume_t umb_resume;
static void	 umb_attach_task(struct usb_proc_msg *);
static usb_handle_request_t umb_handle_request;
static int	 umb_deactivate(device_t);
static void	 umb_ncm_setup(struct umb_softc *, struct usb_config *);
static void	 umb_close_bulkpipes(struct umb_softc *);
static int	 umb_ioctl(if_t , u_long, caddr_t);
static void	 umb_init(void *);
static void	 umb_input(if_t , struct mbuf *);
static int	 umb_output(if_t , struct mbuf *,
		    const struct sockaddr *, struct route *);
static void	 umb_start(if_t );
static void	 umb_start_task(struct usb_proc_msg *);
#if 0
static void	 umb_watchdog(if_t );
#endif
static void	 umb_statechg_timeout(void *);

static int	 umb_mediachange(if_t );
static void	 umb_mediastatus(if_t , struct ifmediareq *);

static void	 umb_add_task(struct umb_softc *sc, usb_proc_callback_t,
		    struct usb_proc_msg *, struct usb_proc_msg *, int);
static void	 umb_newstate(struct umb_softc *, enum umb_state, int);
static void	 umb_state_task(struct usb_proc_msg *);
static void	 umb_up(struct umb_softc *);
static void	 umb_down(struct umb_softc *, int);

static void	 umb_get_response_task(struct usb_proc_msg *);

static void	 umb_decode_response(struct umb_softc *, void *, int);
static void	 umb_handle_indicate_status_msg(struct umb_softc *, void *,
		    int);
static void	 umb_handle_opendone_msg(struct umb_softc *, void *, int);
static void	 umb_handle_closedone_msg(struct umb_softc *, void *, int);
static int	 umb_decode_register_state(struct umb_softc *, void *, int);
static int	 umb_decode_devices_caps(struct umb_softc *, void *, int);
static int	 umb_decode_subscriber_status(struct umb_softc *, void *, int);
static int	 umb_decode_radio_state(struct umb_softc *, void *, int);
static int	 umb_decode_pin(struct umb_softc *, void *, int);
static int	 umb_decode_packet_service(struct umb_softc *, void *, int);
static int	 umb_decode_signal_state(struct umb_softc *, void *, int);
static int	 umb_decode_connect_info(struct umb_softc *, void *, int);
static int	 umb_decode_ip_configuration(struct umb_softc *, void *, int);
static void	 umb_rx(struct umb_softc *);
static usb_callback_t umb_rxeof;
static void	 umb_rxflush(struct umb_softc *);
static int	 umb_encap(struct umb_softc *, struct mbuf *, struct usb_xfer *);
static usb_callback_t umb_txeof;
static void	 umb_txflush(struct umb_softc *);
static void	 umb_decap(struct umb_softc *, struct usb_xfer *, int);

static usb_error_t	 umb_send_encap_command(struct umb_softc *, void *, int);
static int	 umb_get_encap_response(struct umb_softc *, void *, int *);
static void	 umb_ctrl_msg(struct umb_softc *, uint32_t, void *, int);

static void	 umb_open(struct umb_softc *);
static void	 umb_close(struct umb_softc *);

static int	 umb_setpin(struct umb_softc *, int, int, void *, int, void *,
		    int);
static void	 umb_setdataclass(struct umb_softc *);
static void	 umb_radio(struct umb_softc *, int);
static void	 umb_allocate_cid(struct umb_softc *);
static void	 umb_send_fcc_auth(struct umb_softc *);
static void	 umb_packet_service(struct umb_softc *, int);
static void	 umb_connect(struct umb_softc *);
static void	 umb_disconnect(struct umb_softc *);
static void	 umb_send_connect(struct umb_softc *, int);

static void	 umb_qry_ipconfig(struct umb_softc *);
static void	 umb_cmd(struct umb_softc *, int, int, const void *, int);
static void	 umb_cmd1(struct umb_softc *, int, int, const void *, int, uint8_t *);
static void	 umb_command_done(struct umb_softc *, void *, int);
static void	 umb_decode_cid(struct umb_softc *, uint32_t, void *, int);
static void	 umb_decode_qmi(struct umb_softc *, uint8_t *, int);

static usb_callback_t umb_intr;

static char	*umb_ntop(struct sockaddr *);

static const int umb_xfer_tout = USB_DEFAULT_TIMEOUT;

static uint8_t	 umb_uuid_basic_connect[] = MBIM_UUID_BASIC_CONNECT;
static uint8_t	 umb_uuid_context_internet[] = MBIM_UUID_CONTEXT_INTERNET;
static uint8_t	 umb_uuid_qmi_mbim[] = MBIM_UUID_QMI_MBIM;
static uint32_t	 umb_session_id = 0;

static const struct usb_config umb_config[UMB_N_TRANSFER] = {
	[UMB_INTR_RX] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.if_index = 1,
		.callback = umb_intr,
		.bufsize = sizeof (struct usb_cdc_notification),
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1},
		.usb_mode = USB_MODE_HOST,
	},
	[UMB_BULK_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.if_index = 0,
		.callback = umb_rxeof,
		.bufsize = 8 * 1024,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,.ext_buffer = 1},
		.usb_mode = USB_MODE_HOST,
	},
	[UMB_BULK_TX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.if_index = 0,
		.callback = umb_txeof,
		.bufsize = 8 * 1024,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,.ext_buffer = 1},
		.timeout = umb_xfer_tout,
		.usb_mode = USB_MODE_HOST,
	},
};

static device_method_t umb_methods[] = {
	/* USB interface */
	DEVMETHOD(usb_handle_request, umb_handle_request),

	/* Device interface */
	DEVMETHOD(device_probe, umb_probe),
	DEVMETHOD(device_attach, umb_attach),
	DEVMETHOD(device_detach, umb_detach),
	DEVMETHOD(device_suspend, umb_suspend),
	DEVMETHOD(device_resume, umb_resume),

	DEVMETHOD_END
};

static driver_t umb_driver = {
	.name = "umb",
	.methods = umb_methods,
	.size = sizeof (struct umb_softc),
};

MALLOC_DEFINE(M_USB_UMB, "USB UMB", "USB MBIM driver");

const int umb_delay = 4000;

/*
 * These devices require an "FCC Authentication" command.
 */
#ifndef USB_VENDOR_SIERRA
# define USB_VENDOR_SIERRA	0x1199
#endif
#ifndef USB_PRODUCT_SIERRA_EM7455
# define USB_PRODUCT_SIERRA_EM7455	0x9079
#endif
const struct usb_device_id umb_fccauth_devs[] = {
	{
		.match_flag_vendor = 1,
		.match_flag_product = 1,
		.idVendor = USB_VENDOR_SIERRA,
		.idProduct = USB_PRODUCT_SIERRA_EM7455
	}
};

static const uint8_t umb_qmi_alloc_cid[] = {
	0x01,
	0x0f, 0x00,		/* len */
	0x00,			/* QMUX flags */
	0x00,			/* service "ctl" */
	0x00,			/* CID */
	0x00,			/* QMI flags */
	0x01,			/* transaction */
	0x22, 0x00,		/* msg "Allocate CID" */
	0x04, 0x00,		/* TLV len */
	0x01, 0x01, 0x00, 0x02	/* TLV */
};

static const uint8_t umb_qmi_fcc_auth[] = {
	0x01,
	0x0c, 0x00,		/* len */
	0x00,			/* QMUX flags */
	0x02,			/* service "dms" */
#define UMB_QMI_CID_OFFS	5
	0x00,			/* CID (filled in later) */
	0x00,			/* QMI flags */
	0x01, 0x00,		/* transaction */
	0x5f, 0x55,		/* msg "Send FCC Authentication" */
	0x00, 0x00		/* TLV len */
};

static int
umb_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	usb_interface_descriptor_t *id;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if ((id = usbd_get_interface_descriptor(uaa->iface)) == NULL)
		return (ENXIO);

	/*
	 * If this function implements NCM, check if alternate setting
	 * 1 implements MBIM.
	 */
	if (id->bInterfaceClass == UICLASS_CDC &&
	    id->bInterfaceSubClass ==
	    UISUBCLASS_NETWORK_CONTROL_MODEL) {
		id = usbd_get_interface_descriptor(
				usbd_get_iface(uaa->device,
					uaa->info.bIfaceIndex + 1));
		if (id == NULL || id->bAlternateSetting != 1)
			return (ENXIO);
	}

#ifndef UISUBCLASS_MOBILE_BROADBAND_INTERFACE_MODEL
# define UISUBCLASS_MOBILE_BROADBAND_INTERFACE_MODEL 14
#endif
	if (id->bInterfaceClass == UICLASS_CDC &&
	    id->bInterfaceSubClass ==
	    UISUBCLASS_MOBILE_BROADBAND_INTERFACE_MODEL &&
	    id->bInterfaceProtocol == 0)
		return (BUS_PROBE_SPECIFIC);

	return (ENXIO);
}

static int
umb_attach(device_t dev)
{
	struct umb_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct usb_config config[UMB_N_TRANSFER];
	int	 v;
	const struct usb_cdc_union_descriptor *ud;
	const struct mbim_descriptor *md;
	int	 i;
	usb_interface_descriptor_t *id;
	struct usb_interface *iface;
	int	 data_ifaceno = -1;
	usb_error_t error;

	sc->sc_dev = dev;
	sc->sc_udev = uaa->device;

	memcpy(config, umb_config, sizeof (config));

	device_set_usb_desc(dev);

	sc->sc_ctrl_ifaceno = uaa->info.bIfaceNum;

	mtx_init(&sc->sc_mutex, device_get_nameunit(dev), NULL, MTX_DEF);

	/*
	 * Some MBIM hardware does not provide the mandatory CDC Union
	 * Descriptor, so we also look at matching Interface
	 * Association Descriptors to find out the MBIM Data Interface
	 * number.
	 */
	sc->sc_ver_maj = sc->sc_ver_min = -1;
	sc->sc_maxpktlen = MBIM_MAXSEGSZ_MINVAL;
	id = usbd_get_interface_descriptor(uaa->iface);

	ud = usbd_find_descriptor(sc->sc_udev, id, uaa->info.bIfaceIndex,
			UDESC_CS_INTERFACE, 0xff, UDESCSUB_CDC_UNION, 0xff);
	if (ud != NULL) {
		data_ifaceno = ud->bSlaveInterface[0];
	}

	md = usbd_find_descriptor(sc->sc_udev, id, uaa->info.bIfaceIndex,
			UDESC_CS_INTERFACE, 0xff, UDESCSUB_MBIM, 0xff);
	if (md != NULL) {
		v = UGETW(md->bcdMBIMVersion);
		sc->sc_ver_maj = MBIM_VER_MAJOR(v);
		sc->sc_ver_min = MBIM_VER_MINOR(v);
		sc->sc_ctrl_len = UGETW(md->wMaxControlMessage);
		/* Never trust a USB device! Could try to exploit us */
		if (sc->sc_ctrl_len < MBIM_CTRLMSG_MINLEN ||
				sc->sc_ctrl_len > MBIM_CTRLMSG_MAXLEN) {
			DPRINTF("control message len %d out of "
					"bounds [%d .. %d]\n",
					sc->sc_ctrl_len, MBIM_CTRLMSG_MINLEN,
					MBIM_CTRLMSG_MAXLEN);
			/* continue anyway */
		}
		sc->sc_maxpktlen = UGETW(md->wMaxSegmentSize);
		DPRINTFN(2, "ctrl_len=%d, maxpktlen=%d, cap=0x%x\n",
				sc->sc_ctrl_len, sc->sc_maxpktlen,
				md->bmNetworkCapabilities);
	}
	if (sc->sc_ver_maj < 0) {
		device_printf(dev, "error: missing MBIM descriptor\n");
		goto fail;
	}

	device_printf(dev, "version %d.%d\n", sc->sc_ver_maj,
	    sc->sc_ver_min);

	if (usbd_lookup_id_by_uaa(umb_fccauth_devs, sizeof (umb_fccauth_devs),
				  uaa)) {
		sc->sc_flags |= UMBFLG_FCC_AUTH_REQUIRED;
		sc->sc_cid = -1;
	}

	for (i = 0; i < sc->sc_udev->ifaces_max; i++) {
		iface = usbd_get_iface(sc->sc_udev, i);
		id = usbd_get_interface_descriptor(iface);
		if (id == NULL)
			break;

		if (id->bInterfaceNumber == data_ifaceno) {
			sc->sc_data_iface = iface;
			sc->sc_ifaces_index[0] = i;
			sc->sc_ifaces_index[1] = uaa->info.bIfaceIndex;
			break;
		}
	}
	if (sc->sc_data_iface == NULL) {
		device_printf(dev, "error: no data interface found\n");
		goto fail;
	}

	/*
	 * If this is a combined NCM/MBIM function, switch to
	 * alternate setting one to enable MBIM.
	 */
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id != NULL && id->bInterfaceClass == UICLASS_CDC &&
	    id->bInterfaceSubClass == UISUBCLASS_NETWORK_CONTROL_MODEL) {
		device_printf(sc->sc_dev, "combined NCM/MBIM\n");
		error = usbd_req_set_alt_interface_no(sc->sc_udev,
				NULL, uaa->info.bIfaceIndex, 1);
		if (error != USB_ERR_NORMAL_COMPLETION) {
			device_printf(dev, "error: Could not switch to"
					   " alternate setting for MBIM\n");
			goto fail;
		}
		sc->sc_ifaces_index[1] = uaa->info.bIfaceIndex + 1;
	}

	if (usb_proc_create(&sc->sc_taskqueue, &sc->sc_mutex,
				device_get_nameunit(sc->sc_dev),
				USB_PRI_MED) != 0)
		goto fail;

	DPRINTFN(2, "ctrl-ifno#%d: data-ifno#%d\n", sc->sc_ctrl_ifaceno,
	    data_ifaceno);

	usb_callout_init_mtx(&sc->sc_statechg_timer, &sc->sc_mutex, 0);

	umb_ncm_setup(sc, config);
	DPRINTFN(2, "%s: rx/tx size %d/%d\n", DEVNAM(sc),
			sc->sc_rx_bufsz, sc->sc_tx_bufsz);

	sc->sc_rx_buf = malloc(sc->sc_rx_bufsz, M_DEVBUF, M_WAITOK);
	sc->sc_tx_buf = malloc(sc->sc_tx_bufsz, M_DEVBUF, M_WAITOK);

	for (i = 0; i != 32; i++) {
		error = usbd_set_alt_interface_index(sc->sc_udev,
				sc->sc_ifaces_index[0], i);
		if (error)
			break;

		error = usbd_transfer_setup(sc->sc_udev, sc->sc_ifaces_index,
				sc->sc_xfer, config, UMB_N_TRANSFER,
				sc, &sc->sc_mutex);
		if (error == USB_ERR_NORMAL_COMPLETION)
			break;
	}
	if (error || (i == 32)) {
		device_printf(sc->sc_dev, "error: failed to setup xfers\n");
		goto fail;
	}

	sc->sc_resp_buf = malloc(sc->sc_ctrl_len, M_DEVBUF, M_WAITOK);
	sc->sc_ctrl_msg = malloc(sc->sc_ctrl_len, M_DEVBUF, M_WAITOK);

	sc->sc_info.regstate = MBIM_REGSTATE_UNKNOWN;
	sc->sc_info.pin_attempts_left = UMB_VALUE_UNKNOWN;
	sc->sc_info.rssi = UMB_VALUE_UNKNOWN;
	sc->sc_info.ber = UMB_VALUE_UNKNOWN;

	/* defer attaching the interface */
	mtx_lock(&sc->sc_mutex);
	umb_add_task(sc, umb_attach_task,
			&sc->sc_proc_attach_task[0].hdr,
			&sc->sc_proc_attach_task[1].hdr, 0);
	mtx_unlock(&sc->sc_mutex);

	return (0);

fail:
	umb_detach(sc->sc_dev);
	return (ENXIO);
}

static void
umb_attach_task(struct usb_proc_msg *msg)
{
	struct umb_task *task = (struct umb_task *)msg;
	struct umb_softc *sc = task->sc;
	if_t ifp;

	mtx_unlock(&sc->sc_mutex);

	CURVNET_SET_QUIET(vnet0);

	/* initialize the interface */
	sc->sc_if = ifp = if_alloc(IFT_MBIM);
	if_initname(ifp, "umb", device_get_unit(sc->sc_dev));

	if_setsoftc(ifp, sc);
	if_setflags(ifp, IFF_SIMPLEX | IFF_MULTICAST | IFF_POINTOPOINT);
	if_setioctlfn(ifp, umb_ioctl);
	if_setinputfn(ifp, umb_input);
	if_setoutputfn(ifp, umb_output);
	if_setstartfn(ifp, umb_start);
	if_setinitfn(ifp, umb_init);

#if 0
	if_setwatchdog(ifp, umb_watchdog);
#endif
	if_link_state_change(ifp, LINK_STATE_DOWN);
	ifmedia_init(&sc->sc_im, 0, umb_mediachange, umb_mediastatus);
	ifmedia_add(&sc->sc_im, IFM_NONE | IFM_AUTO, 0, NULL);

	if_setifheaderlen(ifp, sizeof (struct ncm_header16) +
	    sizeof (struct ncm_pointer16)); /* XXX - IFAPI */
	/* XXX hard-coded atm */
	if_setmtu(ifp, MIN(2048, sc->sc_maxpktlen));
	if_setsendqlen(ifp, ifqmaxlen);
	if_setsendqready(ifp);

	/* attach the interface */
	if_attach(ifp);
	bpfattach(ifp, DLT_RAW, 0);

	sc->sc_attached = 1;

	CURVNET_RESTORE();

	umb_init(sc);
	mtx_lock(&sc->sc_mutex);
}

static int
umb_detach(device_t dev)
{
	struct umb_softc *sc = device_get_softc(dev);
	if_t ifp = GET_IFP(sc);

	usb_proc_drain(&sc->sc_taskqueue);

	mtx_lock(&sc->sc_mutex);
	if (ifp != NULL && (if_getdrvflags(ifp) & IFF_DRV_RUNNING))
		umb_down(sc, 1);
	umb_close(sc);
	mtx_unlock(&sc->sc_mutex);

	usbd_transfer_unsetup(sc->sc_xfer, UMB_N_TRANSFER);

	free(sc->sc_tx_buf, M_DEVBUF);
	free(sc->sc_rx_buf, M_DEVBUF);

	usb_callout_drain(&sc->sc_statechg_timer);

	usb_proc_free(&sc->sc_taskqueue);

	mtx_destroy(&sc->sc_mutex);

	free(sc->sc_ctrl_msg, M_DEVBUF);
	free(sc->sc_resp_buf, M_DEVBUF);

	if (ifp != NULL && if_getsoftc(ifp)) {
		ifmedia_removeall(&sc->sc_im);
	}
	if (sc->sc_attached) {
		bpfdetach(ifp);
		if_detach(ifp);
		if_free(ifp);
		sc->sc_if = NULL;
	}

	return 0;
}

static void
umb_ncm_setup(struct umb_softc *sc, struct usb_config * config)
{
	usb_device_request_t req;
	struct ncm_ntb_parameters np;
	usb_error_t error;

	/* Query NTB transfers sizes */
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = NCM_GET_NTB_PARAMETERS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctrl_ifaceno);
	USETW(req.wLength, sizeof (np));
	mtx_lock(&sc->sc_mutex);
	error = usbd_do_request(sc->sc_udev, &sc->sc_mutex, &req, &np);
	mtx_unlock(&sc->sc_mutex);
	if (error == USB_ERR_NORMAL_COMPLETION &&
	    UGETW(np.wLength) == sizeof (np)) {
		config[UMB_BULK_RX].bufsize = UGETDW(np.dwNtbInMaxSize);
		config[UMB_BULK_TX].bufsize = UGETDW(np.dwNtbOutMaxSize);
	}
	sc->sc_rx_bufsz = config[UMB_BULK_RX].bufsize;
	sc->sc_tx_bufsz = config[UMB_BULK_TX].bufsize;
}

static int
umb_handle_request(device_t dev,
    const void *preq, void **pptr, uint16_t *plen,
    uint16_t offset, uint8_t *pstate)
{
	/* FIXME really implement */

	return (ENXIO);
}

static int
umb_suspend(device_t dev)
{
	device_printf(dev, "Suspending\n");
	return (0);
}

static int
umb_resume(device_t dev)
{
	device_printf(dev, "Resuming\n");
	return (0);
}

static int
umb_deactivate(device_t dev)
{
	struct umb_softc *sc = device_get_softc(dev);
	if_t ifp = GET_IFP(sc);

	if (ifp != NULL) {
		if_dead(ifp);
	}
	sc->sc_dying = 1;
	return 0;
}

static void
umb_close_bulkpipes(struct umb_softc *sc)
{
	if_t ifp = GET_IFP(sc);

	if_setdrvflagbits(ifp, 0, (IFF_DRV_RUNNING | IFF_DRV_OACTIVE));

	umb_rxflush(sc);
	umb_txflush(sc);

	usbd_transfer_stop(sc->sc_xfer[UMB_BULK_RX]);
	usbd_transfer_stop(sc->sc_xfer[UMB_BULK_TX]);
}

static int
umb_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct umb_softc *sc = if_getsoftc(ifp);
	struct in_ifaddr *ia = (struct in_ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;
	struct umb_parameter mp;

	if (sc->sc_dying)
		return EIO;

	switch (cmd) {
	case SIOCSIFADDR:
		switch (ia->ia_ifa.ifa_addr->sa_family) {
		case AF_INET:
			break;
#ifdef INET6
		case AF_INET6:
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	case SIOCSIFFLAGS:
		mtx_lock(&sc->sc_mutex);
		umb_add_task(sc, umb_state_task,
				&sc->sc_proc_state_task[0].hdr,
				&sc->sc_proc_state_task[1].hdr, 1);
		mtx_unlock(&sc->sc_mutex);
		break;
	case SIOCGUMBINFO:
		error = copyout(&sc->sc_info, ifr->ifr_ifru.ifru_data,
		    sizeof (sc->sc_info));
		break;
	case SIOCSUMBPARAM:
		error = priv_check(curthread, PRIV_NET_SETIFPHYS);
		if (error)
			break;

		if ((error = copyin(ifr->ifr_ifru.ifru_data, &mp, sizeof (mp))) != 0)
			break;

		if ((error = umb_setpin(sc, mp.op, mp.is_puk, mp.pin, mp.pinlen,
		    mp.newpin, mp.newpinlen)) != 0)
			break;

		if (mp.apnlen < 0 || mp.apnlen > sizeof (sc->sc_info.apn)) {
			error = EINVAL;
			break;
		}
		sc->sc_roaming = mp.roaming ? 1 : 0;
		memset(sc->sc_info.apn, 0, sizeof (sc->sc_info.apn));
		memcpy(sc->sc_info.apn, mp.apn, mp.apnlen);
		sc->sc_info.apnlen = mp.apnlen;
		memset(sc->sc_info.username, 0, sizeof (sc->sc_info.username));
		memcpy(sc->sc_info.username, mp.username, mp.usernamelen);
		sc->sc_info.usernamelen = mp.usernamelen;
		memset(sc->sc_info.password, 0, sizeof (sc->sc_info.password));
		memcpy(sc->sc_info.password, mp.password, mp.passwordlen);
		sc->sc_info.passwordlen = mp.passwordlen;
		sc->sc_info.preferredclasses = mp.preferredclasses;
		umb_setdataclass(sc);
		break;
	case SIOCGUMBPARAM:
		memset(&mp, 0, sizeof (mp));
		memcpy(mp.apn, sc->sc_info.apn, sc->sc_info.apnlen);
		mp.apnlen = sc->sc_info.apnlen;
		mp.roaming = sc->sc_roaming;
		mp.preferredclasses = sc->sc_info.preferredclasses;
		error = copyout(&mp, ifr->ifr_ifru.ifru_data, sizeof (mp));
		break;
	case SIOCSIFMTU:
		/* Does this include the NCM headers and tail? */
		if (ifr->ifr_mtu > if_getmtu(ifp)) {
			error = EINVAL;
			break;
		}
		if_setmtu(ifp, ifr->ifr_mtu);
		break;
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_im, cmd);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static void
umb_init(void *arg)
{
	struct umb_softc *sc = arg;

	mtx_lock(&sc->sc_mutex);
	umb_add_task(sc, umb_start_task,
			&sc->sc_proc_start_task[0].hdr,
			&sc->sc_proc_start_task[1].hdr, 0);
	mtx_unlock(&sc->sc_mutex);
}

static void
umb_input(if_t ifp, struct mbuf *m)
{
	struct mbuf *mn;
	struct epoch_tracker et;

	while (m) {
		mn = m->m_nextpkt;
		m->m_nextpkt = NULL;

		NET_EPOCH_ENTER(et);
		BPF_MTAP(ifp, m);

		CURVNET_SET_QUIET(if_getvnet(ifp));

		netisr_dispatch(NETISR_IP, m);
		m = mn;

		CURVNET_RESTORE();
		NET_EPOCH_EXIT(et);
	}
}

static int
umb_output(if_t ifp, struct mbuf *m, const struct sockaddr *dst,
    struct route *rtp)
{
	int error;

	DPRINTFN(10, "%s: enter\n", __func__);

	switch (dst->sa_family) {
#ifdef INET6
	case AF_INET6:
		/* fall through */
#endif
	case AF_INET:
		break;

		/* silently drop dhclient packets */
	case AF_UNSPEC:
		m_freem(m);
		return (0);

		/* drop other packet types */
	default:
		m_freem(m);
		return (EAFNOSUPPORT);
	}

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	error = if_transmit(ifp, m);
	if (error) {
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return (ENOBUFS);
	}

	return (0);
}

static void
umb_start(if_t ifp)
{
	struct umb_softc *sc = if_getsoftc(ifp);

	if (sc->sc_dying || !(if_getdrvflags(ifp) & IFF_DRV_RUNNING))
		return;

	mtx_lock(&sc->sc_mutex);
	usbd_transfer_start(sc->sc_xfer[UMB_BULK_TX]);
	mtx_unlock(&sc->sc_mutex);
}

static void
umb_start_task(struct usb_proc_msg *msg)
{
	struct umb_task *task = (struct umb_task *)msg;
	struct umb_softc *sc = task->sc;
	if_t ifp = GET_IFP(sc);

	DPRINTF("%s()\n", __func__);

	mtx_assert(&sc->sc_mutex, MA_OWNED);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, 0);

	/* start interrupt transfer */
	usbd_transfer_start(sc->sc_xfer[UMB_INTR_RX]);

	umb_open(sc);
}

#if 0
static void
umb_watchdog(if_t ifp)
{
	struct umb_softc *sc = if_getsoftc(ifp);

	if (sc->sc_dying)
		return;

	if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	device_printf(sc->sc_dev, "watchdog timeout\n");
	usbd_transfer_drain(sc->sc_xfer[UMB_BULK_TX]);
	return;
}
#endif

static void
umb_statechg_timeout(void *arg)
{
	struct umb_softc *sc = arg;
	if_t ifp = GET_IFP(sc);

	mtx_assert(&sc->sc_mutex, MA_OWNED);

	if (sc->sc_info.regstate != MBIM_REGSTATE_ROAMING || sc->sc_roaming)
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_DEBUG, "%s: state change timeout\n",
					DEVNAM(sc));

	umb_add_task(sc, umb_state_task,
			&sc->sc_proc_state_task[0].hdr,
			&sc->sc_proc_state_task[1].hdr, 0);
}

static int
umb_mediachange(if_t  ifp)
{
	return 0;
}

static void
umb_mediastatus(if_t  ifp, struct ifmediareq * imr)
{
	switch (if_getlinkstate(ifp)) {
	case LINK_STATE_UP:
		imr->ifm_status = IFM_AVALID | IFM_ACTIVE;
		break;
	case LINK_STATE_DOWN:
		imr->ifm_status = IFM_AVALID;
		break;
	default:
		imr->ifm_status = 0;
		break;
	}
}

static void
umb_add_task(struct umb_softc *sc, usb_proc_callback_t callback,
		struct usb_proc_msg *t0, struct usb_proc_msg *t1, int sync)
{
	struct umb_task * task;

	mtx_assert(&sc->sc_mutex, MA_OWNED);

	if (usb_proc_is_gone(&sc->sc_taskqueue)) {
		return;
	}

	task = usb_proc_msignal(&sc->sc_taskqueue, t0, t1);

	task->hdr.pm_callback = callback;
	task->sc = sc;

	if (sync) {
		usb_proc_mwait(&sc->sc_taskqueue, t0, t1);
	}
}

static void
umb_newstate(struct umb_softc *sc, enum umb_state newstate, int flags)
{
	if_t ifp = GET_IFP(sc);

	if (newstate == sc->sc_state)
		return;
	if (((flags & UMB_NS_DONT_DROP) && newstate < sc->sc_state) ||
	    ((flags & UMB_NS_DONT_RAISE) && newstate > sc->sc_state))
		return;
	if (if_getflags(ifp) & IFF_DEBUG)
		log(LOG_DEBUG, "%s: state going %s from '%s' to '%s'\n",
		    DEVNAM(sc), newstate > sc->sc_state ? "up" : "down",
		    umb_istate(sc->sc_state), umb_istate(newstate));
	sc->sc_state = newstate;
	umb_add_task(sc, umb_state_task,
			&sc->sc_proc_state_task[0].hdr,
			&sc->sc_proc_state_task[1].hdr, 0);
}

static void
umb_state_task(struct usb_proc_msg *msg)
{
	struct umb_task *task = (struct umb_task *)msg;
	struct umb_softc *sc = task->sc;
	if_t ifp = GET_IFP(sc);
	struct ifreq ifr;
	int	 state;

	DPRINTF("%s()\n", __func__);

	if (sc->sc_info.regstate == MBIM_REGSTATE_ROAMING && !sc->sc_roaming) {
		/*
		 * Query the registration state until we're with the home
		 * network again.
		 */
		umb_cmd(sc, MBIM_CID_REGISTER_STATE, MBIM_CMDOP_QRY, NULL, 0);
		return;
	}

	if (if_getflags(ifp) & IFF_UP)
		umb_up(sc);
	else
		umb_down(sc, 0);

	state = (sc->sc_state == UMB_S_UP) ? LINK_STATE_UP : LINK_STATE_DOWN;
	if (if_getlinkstate(ifp) != state) {
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_DEBUG, "%s: link state changed from %s to %s\n",
			    DEVNAM(sc),
			    (if_getlinkstate(ifp) == LINK_STATE_UP)
			    ? "up" : "down",
			    (state == LINK_STATE_UP) ? "up" : "down");
		if_link_state_change(ifp, state); /* XXX - IFAPI */
		if (state != LINK_STATE_UP) {
			/*
			 * Purge any existing addresses
			 */
			memset(sc->sc_info.ipv4dns, 0,
			    sizeof (sc->sc_info.ipv4dns));
			mtx_unlock(&sc->sc_mutex);
			CURVNET_SET_QUIET(if_getvnet(ifp));
			if (in_control(NULL, SIOCGIFADDR, (caddr_t)&ifr, ifp,
				       curthread) == 0 &&
			    satosin(&ifr.ifr_addr)->sin_addr.s_addr !=
			    INADDR_ANY) {
				in_control(NULL, SIOCDIFADDR, (caddr_t)&ifr,
					   ifp, curthread);
			}
			CURVNET_RESTORE();
			mtx_lock(&sc->sc_mutex);
		}
		if_link_state_change(ifp, state);
	}
}

static void
umb_up(struct umb_softc *sc)
{
	if_t ifp = GET_IFP(sc);

	switch (sc->sc_state) {
	case UMB_S_DOWN:
		DPRINTF("init: opening ...\n");
		umb_open(sc);
		break;
	case UMB_S_OPEN:
		if (sc->sc_flags & UMBFLG_FCC_AUTH_REQUIRED) {
			if (sc->sc_cid == -1) {
				DPRINTF("init: allocating CID ...\n");
				umb_allocate_cid(sc);
				break;
			} else
				umb_newstate(sc, UMB_S_CID, UMB_NS_DONT_DROP);
		} else {
			DPRINTF("init: turning radio on ...\n");
			umb_radio(sc, 1);
			break;
		}
		/*FALLTHROUGH*/
	case UMB_S_CID:
		DPRINTF("init: sending FCC auth ...\n");
		umb_send_fcc_auth(sc);
		break;
	case UMB_S_RADIO:
		DPRINTF("init: checking SIM state ...\n");
		umb_cmd(sc, MBIM_CID_SUBSCRIBER_READY_STATUS, MBIM_CMDOP_QRY,
		    NULL, 0);
		break;
	case UMB_S_SIMREADY:
		DPRINTF("init: attaching ...\n");
		umb_packet_service(sc, 1);
		break;
	case UMB_S_ATTACHED:
		sc->sc_tx_seq = 0;
		DPRINTF("init: connecting ...\n");
		umb_connect(sc);
		break;
	case UMB_S_CONNECTED:
		DPRINTF("init: getting IP config ...\n");
		umb_qry_ipconfig(sc);
		break;
	case UMB_S_UP:
		DPRINTF("init: reached state UP\n");
		if (!(if_getflags(ifp) & IFF_DRV_RUNNING)) {
			if_setdrvflagbits(ifp, IFF_DRV_RUNNING, 0);
			if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
			umb_rx(sc);
		}
		break;
	}
	if (sc->sc_state < UMB_S_UP)
		usb_callout_reset(&sc->sc_statechg_timer,
		    UMB_STATE_CHANGE_TIMEOUT * hz, umb_statechg_timeout, sc);
	else {
		usb_callout_stop(&sc->sc_statechg_timer);
	}
	return;
}

static void
umb_down(struct umb_softc *sc, int force)
{
	umb_close_bulkpipes(sc);

	switch (sc->sc_state) {
	case UMB_S_UP:
	case UMB_S_CONNECTED:
		DPRINTF("stop: disconnecting ...\n");
		umb_disconnect(sc);
		if (!force)
			break;
		/*FALLTHROUGH*/
	case UMB_S_ATTACHED:
		DPRINTF("stop: detaching ...\n");
		umb_packet_service(sc, 0);
		if (!force)
			break;
		/*FALLTHROUGH*/
	case UMB_S_SIMREADY:
	case UMB_S_RADIO:
		DPRINTF("stop: turning radio off ...\n");
		umb_radio(sc, 0);
		if (!force)
			break;
		/*FALLTHROUGH*/
	case UMB_S_CID:
	case UMB_S_OPEN:
	case UMB_S_DOWN:
		/* Do not close the device */
		DPRINTF("stop: reached state DOWN\n");
		break;
	}
	if (force)
		sc->sc_state = UMB_S_OPEN;

	if (sc->sc_state > UMB_S_OPEN)
		usb_callout_reset(&sc->sc_statechg_timer,
		    UMB_STATE_CHANGE_TIMEOUT * hz, umb_statechg_timeout, sc);
	else
		usb_callout_stop(&sc->sc_statechg_timer);
}

static void
umb_get_response_task(struct usb_proc_msg *msg)
{
	struct umb_task *task = (struct umb_task *)msg;
	struct umb_softc *sc = task->sc;
	int	 len;

	DPRINTF("%s()\n", __func__);
	/*
	 * Function is required to send on RESPONSE_AVAILABLE notification for
	 * each encapsulated response that is to be processed by the host.
	 * But of course, we can receive multiple notifications before the
	 * response task is run.
	 */
	while (sc->sc_nresp > 0) {
		--sc->sc_nresp;
		len = sc->sc_ctrl_len;
		if (umb_get_encap_response(sc, sc->sc_resp_buf, &len))
			umb_decode_response(sc, sc->sc_resp_buf, len);
	}
}

static void
umb_decode_response(struct umb_softc *sc, void *response, int len)
{
	struct mbim_msghdr *hdr = response;
	struct mbim_fragmented_msg_hdr *fraghdr;
	uint32_t type;

	DPRINTFN(3, "got response: len %d\n", len);
	DDUMPN(4, response, len);

	if (len < sizeof (*hdr) || le32toh(hdr->len) != len) {
		/*
		 * We should probably cancel a transaction, but since the
		 * message is too short, we cannot decode the transaction
		 * id (tid) and hence don't know, whom to cancel. Must wait
		 * for the timeout.
		 */
		DPRINTF("received short response (len %d)\n",
		    len);
		return;
	}

	/*
	 * XXX FIXME: if message is fragmented, store it until last frag
	 *	is received and then re-assemble all fragments.
	 */
	type = le32toh(hdr->type);
	switch (type) {
	case MBIM_INDICATE_STATUS_MSG:
	case MBIM_COMMAND_DONE:
		fraghdr = response;
		if (le32toh(fraghdr->frag.nfrag) != 1) {
			DPRINTF("discarding fragmented messages\n");
			return;
		}
		break;
	default:
		break;
	}

	DPRINTF("<- rcv %s (tid %u)\n", umb_request2str(type),
	    le32toh(hdr->tid));
	switch (type) {
	case MBIM_FUNCTION_ERROR_MSG:
	case MBIM_HOST_ERROR_MSG:
	{
		struct mbim_f2h_hosterr *e;
		int	 err;

		if (len >= sizeof (*e)) {
			e = response;
			err = le32toh(e->err);

			DPRINTF("%s message, error %s (tid %u)\n",
			    umb_request2str(type),
			    umb_error2str(err), le32toh(hdr->tid));
			if (err == MBIM_ERROR_NOT_OPENED)
				umb_newstate(sc, UMB_S_DOWN, 0);
		}
		break;
	}
	case MBIM_INDICATE_STATUS_MSG:
		umb_handle_indicate_status_msg(sc, response, len);
		break;
	case MBIM_OPEN_DONE:
		umb_handle_opendone_msg(sc, response, len);
		break;
	case MBIM_CLOSE_DONE:
		umb_handle_closedone_msg(sc, response, len);
		break;
	case MBIM_COMMAND_DONE:
		umb_command_done(sc, response, len);
		break;
	default:
		DPRINTF("discard message %s\n",
		    umb_request2str(type));
		break;
	}
}

static void
umb_handle_indicate_status_msg(struct umb_softc *sc, void *data, int len)
{
	struct mbim_f2h_indicate_status *m = data;
	uint32_t infolen;
	uint32_t cid;

	if (len < sizeof (*m)) {
		DPRINTF("discard short %s message\n",
		    umb_request2str(le32toh(m->hdr.type)));
		return;
	}
	if (memcmp(m->devid, umb_uuid_basic_connect, sizeof (m->devid))) {
		DPRINTF("discard %s message for other UUID '%s'\n",
		    umb_request2str(le32toh(m->hdr.type)),
		    umb_uuid2str(m->devid));
		return;
	}
	infolen = le32toh(m->infolen);
	if (len < sizeof (*m) + infolen) {
		DPRINTF("discard truncated %s message (want %d, got %d)\n",
		    umb_request2str(le32toh(m->hdr.type)),
		    (int)sizeof (*m) + infolen, len);
		return;
	}

	cid = le32toh(m->cid);
	DPRINTF("indicate %s status\n", umb_cid2str(cid));
	umb_decode_cid(sc, cid, m->info, infolen);
}

static void
umb_handle_opendone_msg(struct umb_softc *sc, void *data, int len)
{
	struct mbim_f2h_openclosedone *resp = data;
	if_t ifp = GET_IFP(sc);
	uint32_t status;

	status = le32toh(resp->status);
	if (status == MBIM_STATUS_SUCCESS) {
		if (sc->sc_maxsessions == 0) {
			umb_cmd(sc, MBIM_CID_DEVICE_CAPS, MBIM_CMDOP_QRY, NULL,
			    0);
			umb_cmd(sc, MBIM_CID_PIN, MBIM_CMDOP_QRY, NULL, 0);
			umb_cmd(sc, MBIM_CID_REGISTER_STATE, MBIM_CMDOP_QRY,
			    NULL, 0);
		}
		umb_newstate(sc, UMB_S_OPEN, UMB_NS_DONT_DROP);
	} else if (if_getflags(ifp) & IFF_DEBUG)
		log(LOG_ERR, "%s: open error: %s\n", DEVNAM(sc),
		    umb_status2str(status));
	return;
}

static void
umb_handle_closedone_msg(struct umb_softc *sc, void *data, int len)
{
	struct mbim_f2h_openclosedone *resp = data;
	uint32_t status;

	status = le32toh(resp->status);
	if (status == MBIM_STATUS_SUCCESS)
		umb_newstate(sc, UMB_S_DOWN, 0);
	else
		DPRINTF("close error: %s\n",
		    umb_status2str(status));
	return;
}

static inline void
umb_getinfobuf(char *in, int inlen, uint32_t offs, uint32_t sz,
    void *out, size_t outlen)
{
	offs = le32toh(offs);
	sz = le32toh(sz);
	memset(out, 0, outlen);
	if ((uint64_t)inlen >= (uint64_t)offs + (uint64_t)sz)
		memcpy(out, in + offs, MIN(sz, outlen));
}

static inline int
umb_padding(void *data, int len, size_t sz)
{
	char *p = data;
	int np = 0;

	while (len < sz && (len % 4) != 0) {
		*p++ = '\0';
		len++;
		np++;
	}
	return np;
}

static inline int
umb_addstr(void *buf, size_t bufsz, int *offs, void *str, int slen,
    uint32_t *offsmember, uint32_t *sizemember)
{
	if (*offs + slen > bufsz)
		return 0;

	*sizemember = htole32((uint32_t)slen);
	if (slen && str) {
		*offsmember = htole32((uint32_t)*offs);
		memcpy((char *)buf + *offs, str, slen);
		*offs += slen;
		*offs += umb_padding(buf, *offs, bufsz);
	} else
		*offsmember = htole32(0);
	return 1;
}

static void
umb_in_len2mask(struct in_addr *mask, int len)
{
	int i;
	u_char *p;

	p = (u_char *)mask;
	memset(mask, 0, sizeof (*mask));
	for (i = 0; i < len / 8; i++)
		p[i] = 0xff;
	if (len % 8)
		p[i] = (0xff00 >> (len % 8)) & 0xff;
}

static int
umb_decode_register_state(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_registration_state_info *rs = data;
	if_t ifp = GET_IFP(sc);

	if (len < sizeof (*rs))
		return 0;
	sc->sc_info.nwerror = le32toh(rs->nwerror);
	sc->sc_info.regstate = le32toh(rs->regstate);
	sc->sc_info.regmode = le32toh(rs->regmode);
	sc->sc_info.cellclass = le32toh(rs->curcellclass);

	/* XXX should we remember the provider_id? */
	umb_getinfobuf(data, len, rs->provname_offs, rs->provname_size,
	    sc->sc_info.provider, sizeof (sc->sc_info.provider));
	umb_getinfobuf(data, len, rs->roamingtxt_offs, rs->roamingtxt_size,
	    sc->sc_info.roamingtxt, sizeof (sc->sc_info.roamingtxt));

	DPRINTFN(2, "%s, availclass 0x%x, class 0x%x, regmode %d\n",
	    umb_regstate(sc->sc_info.regstate),
	    le32toh(rs->availclasses), sc->sc_info.cellclass,
	    sc->sc_info.regmode);

	if (sc->sc_info.regstate == MBIM_REGSTATE_ROAMING &&
	    !sc->sc_roaming &&
	    sc->sc_info.activation == MBIM_ACTIVATION_STATE_ACTIVATED) {
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_INFO,
			    "%s: disconnecting from roaming network\n",
			    DEVNAM(sc));
		umb_disconnect(sc);
	}
	return 1;
}

static int
umb_decode_devices_caps(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_device_caps *dc = data;

	if (len < sizeof (*dc))
		return 0;
	sc->sc_maxsessions = le32toh(dc->max_sessions);
	sc->sc_info.supportedclasses = le32toh(dc->dataclass);
	umb_getinfobuf(data, len, dc->devid_offs, dc->devid_size,
	    sc->sc_info.devid, sizeof (sc->sc_info.devid));
	umb_getinfobuf(data, len, dc->fwinfo_offs, dc->fwinfo_size,
	    sc->sc_info.fwinfo, sizeof (sc->sc_info.fwinfo));
	umb_getinfobuf(data, len, dc->hwinfo_offs, dc->hwinfo_size,
	    sc->sc_info.hwinfo, sizeof (sc->sc_info.hwinfo));
	DPRINTFN(2, "max sessions %d, supported classes 0x%x\n",
	    sc->sc_maxsessions, sc->sc_info.supportedclasses);
	return 1;
}

static int
umb_decode_subscriber_status(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_subscriber_ready_info *si = data;
	if_t ifp = GET_IFP(sc);
	int	npn;

	if (len < sizeof (*si))
		return 0;
	sc->sc_info.sim_state = le32toh(si->ready);

	umb_getinfobuf(data, len, si->sid_offs, si->sid_size,
	    sc->sc_info.sid, sizeof (sc->sc_info.sid));
	umb_getinfobuf(data, len, si->icc_offs, si->icc_size,
	    sc->sc_info.iccid, sizeof (sc->sc_info.iccid));

	npn = le32toh(si->no_pn);
	if (npn > 0)
		umb_getinfobuf(data, len, si->pn[0].offs, si->pn[0].size,
		    sc->sc_info.pn, sizeof (sc->sc_info.pn));
	else
		memset(sc->sc_info.pn, 0, sizeof (sc->sc_info.pn));

	if (sc->sc_info.sim_state == MBIM_SIMSTATE_LOCKED)
		sc->sc_info.pin_state = UMB_PIN_REQUIRED;
	if (if_getflags(ifp) & IFF_DEBUG)
		log(LOG_INFO, "%s: SIM %s\n", DEVNAM(sc),
		    umb_simstate(sc->sc_info.sim_state));
	if (sc->sc_info.sim_state == MBIM_SIMSTATE_INITIALIZED)
		umb_newstate(sc, UMB_S_SIMREADY, UMB_NS_DONT_DROP);
	return 1;
}

static int
umb_decode_radio_state(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_radio_state_info *rs = data;
	if_t ifp = GET_IFP(sc);

	if (len < sizeof (*rs))
		return 0;

	sc->sc_info.hw_radio_on =
	    (le32toh(rs->hw_state) == MBIM_RADIO_STATE_ON) ? 1 : 0;
	sc->sc_info.sw_radio_on =
	    (le32toh(rs->sw_state) == MBIM_RADIO_STATE_ON) ? 1 : 0;
	if (!sc->sc_info.hw_radio_on) {
		device_printf(sc->sc_dev, "radio is disabled by hardware switch\n");
		/*
		 * XXX do we need a time to poll the state of the rfkill switch
		 *	or will the device send an unsolicited notification
		 *	in case the state changes?
		 */
		umb_newstate(sc, UMB_S_OPEN, 0);
	} else if (!sc->sc_info.sw_radio_on) {
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_INFO, "%s: radio is off\n", DEVNAM(sc));
		umb_newstate(sc, UMB_S_OPEN, 0);
	} else
		umb_newstate(sc, UMB_S_RADIO, UMB_NS_DONT_DROP);
	return 1;
}

static int
umb_decode_pin(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_pin_info *pi = data;
	if_t ifp = GET_IFP(sc);
	uint32_t	attempts_left;

	if (len < sizeof (*pi))
		return 0;

	attempts_left = le32toh(pi->remaining_attempts);
	if (attempts_left != 0xffffffff)
		sc->sc_info.pin_attempts_left = attempts_left;

	switch (le32toh(pi->state)) {
	case MBIM_PIN_STATE_UNLOCKED:
		sc->sc_info.pin_state = UMB_PIN_UNLOCKED;
		break;
	case MBIM_PIN_STATE_LOCKED:
		switch (le32toh(pi->type)) {
		case MBIM_PIN_TYPE_PIN1:
			sc->sc_info.pin_state = UMB_PIN_REQUIRED;
			break;
		case MBIM_PIN_TYPE_PUK1:
			sc->sc_info.pin_state = UMB_PUK_REQUIRED;
			break;
		case MBIM_PIN_TYPE_PIN2:
		case MBIM_PIN_TYPE_PUK2:
			/* Assume that PIN1 was accepted */
			sc->sc_info.pin_state = UMB_PIN_UNLOCKED;
			break;
		}
		break;
	}
	if (if_getflags(ifp) & IFF_DEBUG)
		log(LOG_INFO, "%s: %s state %s (%d attempts left)\n",
		    DEVNAM(sc), umb_pin_type(le32toh(pi->type)),
		    (le32toh(pi->state) == MBIM_PIN_STATE_UNLOCKED) ?
			"unlocked" : "locked",
		    le32toh(pi->remaining_attempts));

	/*
	 * In case the PIN was set after IFF_UP, retrigger the state machine
	 */
	umb_add_task(sc, umb_state_task,
			&sc->sc_proc_state_task[0].hdr,
			&sc->sc_proc_state_task[1].hdr, 0);
	return 1;
}

static int
umb_decode_packet_service(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_packet_service_info *psi = data;
	int	 state, highestclass;
	uint64_t up_speed, down_speed;
	if_t ifp = GET_IFP(sc);

	if (len < sizeof (*psi))
		return 0;

	sc->sc_info.nwerror = le32toh(psi->nwerror);
	state = le32toh(psi->state);
	highestclass = le32toh(psi->highest_dataclass);
	up_speed = le64toh(psi->uplink_speed);
	down_speed = le64toh(psi->downlink_speed);
	if (sc->sc_info.packetstate  != state ||
	    sc->sc_info.uplink_speed != up_speed ||
	    sc->sc_info.downlink_speed != down_speed) {
		if (if_getflags(ifp) & IFF_DEBUG) {
			log(LOG_INFO, "%s: packet service ", DEVNAM(sc));
			if (sc->sc_info.packetstate  != state)
				log(LOG_INFO, "changed from %s to ",
				    umb_packet_state(sc->sc_info.packetstate));
			log(LOG_INFO, "%s, class %s, speed: %" PRIu64 " up / %" PRIu64 " down\n",
			    umb_packet_state(state),
			    umb_dataclass(highestclass), up_speed, down_speed);
		}
	}
	sc->sc_info.packetstate = state;
	sc->sc_info.highestclass = highestclass;
	sc->sc_info.uplink_speed = up_speed;
	sc->sc_info.downlink_speed = down_speed;

	if (sc->sc_info.regmode == MBIM_REGMODE_AUTOMATIC) {
		/*
		 * For devices using automatic registration mode, just proceed,
		 * once registration has completed.
		 */
		if (if_getflags(ifp) & IFF_UP) {
			switch (sc->sc_info.regstate) {
			case MBIM_REGSTATE_HOME:
			case MBIM_REGSTATE_ROAMING:
			case MBIM_REGSTATE_PARTNER:
				umb_newstate(sc, UMB_S_ATTACHED,
				    UMB_NS_DONT_DROP);
				break;
			default:
				break;
			}
		} else
			umb_newstate(sc, UMB_S_SIMREADY, UMB_NS_DONT_RAISE);
	} else switch (sc->sc_info.packetstate) {
	case MBIM_PKTSERVICE_STATE_ATTACHED:
		umb_newstate(sc, UMB_S_ATTACHED, UMB_NS_DONT_DROP);
		break;
	case MBIM_PKTSERVICE_STATE_DETACHED:
		umb_newstate(sc, UMB_S_SIMREADY, UMB_NS_DONT_RAISE);
		break;
	}
	return 1;
}

static int
umb_decode_signal_state(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_signal_state *ss = data;
	if_t ifp = GET_IFP(sc);
	int	 rssi;

	if (len < sizeof (*ss))
		return 0;

	if (le32toh(ss->rssi) == 99)
		rssi = UMB_VALUE_UNKNOWN;
	else {
		rssi = -113 + 2 * le32toh(ss->rssi);
		if ((if_getflags(ifp) & IFF_DEBUG) && sc->sc_info.rssi != rssi &&
		    sc->sc_state >= UMB_S_CONNECTED)
			log(LOG_INFO, "%s: rssi %d dBm\n", DEVNAM(sc), rssi);
	}
	sc->sc_info.rssi = rssi;
	sc->sc_info.ber = le32toh(ss->err_rate);
	if (sc->sc_info.ber == -99)
		sc->sc_info.ber = UMB_VALUE_UNKNOWN;
	return 1;
}

static int
umb_decode_connect_info(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_connect_info *ci = data;
	if_t ifp = GET_IFP(sc);
	int	 act;

	if (len < sizeof (*ci))
		return 0;

	if (le32toh(ci->sessionid) != umb_session_id) {
		DPRINTF("discard connection info for session %u\n",
		    le32toh(ci->sessionid));
		return 1;
	}
	if (memcmp(ci->context, umb_uuid_context_internet,
	    sizeof (ci->context))) {
		DPRINTF("discard connection info for other context\n");
		return 1;
	}
	act = le32toh(ci->activation);
	if (sc->sc_info.activation != act) {
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_INFO, "%s: connection %s\n", DEVNAM(sc),
			    umb_activation(act));
		if ((if_getflags(ifp) & IFF_DEBUG) &&
		    le32toh(ci->iptype) != MBIM_CONTEXT_IPTYPE_DEFAULT &&
		    le32toh(ci->iptype) != MBIM_CONTEXT_IPTYPE_IPV4)
			log(LOG_DEBUG, "%s: got iptype %d connection\n",
			    DEVNAM(sc), le32toh(ci->iptype));

		sc->sc_info.activation = act;
		sc->sc_info.nwerror = le32toh(ci->nwerror);

		if (sc->sc_info.activation == MBIM_ACTIVATION_STATE_ACTIVATED)
			umb_newstate(sc, UMB_S_CONNECTED, UMB_NS_DONT_DROP);
		else if (sc->sc_info.activation ==
		    MBIM_ACTIVATION_STATE_DEACTIVATED)
			umb_newstate(sc, UMB_S_ATTACHED, 0);
		/* else: other states are purely transitional */
	}
	return 1;
}

static int
umb_add_inet_config(struct umb_softc *sc, struct in_addr ip, u_int prefixlen,
    struct in_addr gw)
{
	if_t ifp = GET_IFP(sc);
	struct in_aliasreq ifra;
	struct sockaddr_in *sin;
	int	 rv;

	memset(&ifra, 0, sizeof (ifra));
	sin = (struct sockaddr_in *)&ifra.ifra_addr;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof (*sin);
	sin->sin_addr = ip;

	sin = (struct sockaddr_in *)&ifra.ifra_dstaddr;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof (*sin);
	sin->sin_addr = gw;

	sin = (struct sockaddr_in *)&ifra.ifra_mask;
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof (*sin);
	umb_in_len2mask(&sin->sin_addr,
	    MIN(prefixlen, sizeof (struct in_addr) * 8));

	mtx_unlock(&sc->sc_mutex);
	CURVNET_SET_QUIET(if_getvnet(ifp));
	rv = in_control(NULL, SIOCAIFADDR, (caddr_t)&ifra, ifp, curthread);
	CURVNET_RESTORE();
	mtx_lock(&sc->sc_mutex);
	if (rv != 0) {
		device_printf(sc->sc_dev, "unable to set IPv4 address, error %d\n",
		    rv);
		return rv;
	}

	if (if_getflags(ifp) & IFF_DEBUG)
		log(LOG_INFO, "%s: IPv4 addr %s, mask %s, "
		    "gateway %s\n", DEVNAM(sc),
		    umb_ntop(sintosa(&ifra.ifra_addr)),
		    umb_ntop(sintosa(&ifra.ifra_mask)),
		    umb_ntop(sintosa(&ifra.ifra_dstaddr)));

	return 0;
}

static int
umb_decode_ip_configuration(struct umb_softc *sc, void *data, int len)
{
	struct mbim_cid_ip_configuration_info *ic = data;
	if_t ifp = GET_IFP(sc);
	uint32_t avail_v4;
	uint32_t val;
	int	 n, i;
	int	 off;
	struct mbim_cid_ipv4_element ipv4elem;
	struct in_addr addr, gw;
	int	 state = -1;
	int	 rv;

	if (len < sizeof (*ic))
		return 0;
	if (le32toh(ic->sessionid) != umb_session_id) {
		DPRINTF("ignore IP configuration for session id %d\n",
		    le32toh(ic->sessionid));
		return 0;
	}

	/*
	 * IPv4 configuration
	 */
	avail_v4 = le32toh(ic->ipv4_available);
	if ((avail_v4 & (MBIM_IPCONF_HAS_ADDRINFO | MBIM_IPCONF_HAS_GWINFO)) ==
	    (MBIM_IPCONF_HAS_ADDRINFO | MBIM_IPCONF_HAS_GWINFO)) {
		n = le32toh(ic->ipv4_naddr);
		off = le32toh(ic->ipv4_addroffs);

		if (n == 0 || off + sizeof (ipv4elem) > len)
			goto tryv6;
		if (n != 1 && if_getflags(ifp) & IFF_DEBUG)
			log(LOG_INFO, "%s: more than one IPv4 addr: %d\n",
			    DEVNAM(sc), n);

		/* Only pick the first one */
		memcpy(&ipv4elem, (char *)data + off, sizeof (ipv4elem));
		ipv4elem.prefixlen = le32toh(ipv4elem.prefixlen);
		addr.s_addr = ipv4elem.addr;

		off = le32toh(ic->ipv4_gwoffs);
		if (off + sizeof (gw) > len)
			goto done;
		memcpy(&gw, (char *)data + off, sizeof (gw));

		rv = umb_add_inet_config(sc, addr, ipv4elem.prefixlen, gw);
		if (rv == 0)
			state = UMB_S_UP;
	}

	memset(sc->sc_info.ipv4dns, 0, sizeof (sc->sc_info.ipv4dns));
	if (avail_v4 & MBIM_IPCONF_HAS_DNSINFO) {
		n = le32toh(ic->ipv4_ndnssrv);
		off = le32toh(ic->ipv4_dnssrvoffs);
		i = 0;
		while (n-- > 0) {
			if (off + sizeof (addr) > len)
				break;
			memcpy(&addr, (char *)data + off, sizeof(addr));
			if (i < UMB_MAX_DNSSRV)
				sc->sc_info.ipv4dns[i++] = addr;
			off += sizeof(addr);
		}
	}

	if ((avail_v4 & MBIM_IPCONF_HAS_MTUINFO)) {
		val = le32toh(ic->ipv4_mtu);
		if (if_getmtu(ifp) != val && val <= sc->sc_maxpktlen) {
			if_setmtu(ifp, val);
			if (if_getmtu(ifp) > val)
				if_setmtu(ifp, val);
			if (if_getflags(ifp) & IFF_DEBUG)
				log(LOG_INFO, "%s: MTU %d\n", DEVNAM(sc), val);
		}
	}

	avail_v4 = le32toh(ic->ipv6_available);
	if ((if_getflags(ifp) & IFF_DEBUG) && avail_v4 & MBIM_IPCONF_HAS_ADDRINFO) {
		/* XXX FIXME: IPv6 configuration missing */
		log(LOG_INFO, "%s: ignoring IPv6 configuration\n", DEVNAM(sc));
	}
	if (state != -1)
		umb_newstate(sc, state, 0);

tryv6:
done:
	return 1;
}

static void
umb_rx(struct umb_softc *sc)
{
	mtx_assert(&sc->sc_mutex, MA_OWNED);

	usbd_transfer_start(sc->sc_xfer[UMB_BULK_RX]);
}

static void
umb_rxeof(struct usb_xfer *xfer, usb_error_t status)
{
	struct umb_softc *sc = usbd_xfer_softc(xfer);
	if_t ifp = GET_IFP(sc);
	int actlen;
	int aframes;
	int i;

	DPRINTF("%s(%u): state=%u\n", __func__, status, USB_GET_STATE(xfer));

	mtx_assert(&sc->sc_mutex, MA_OWNED);

	usbd_xfer_status(xfer, &actlen, NULL, &aframes, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("received %u bytes in %u frames\n", actlen, aframes);

		if (actlen == 0) {
			if (sc->sc_rx_nerr >= 4)
				/* throttle transfers */
				usbd_xfer_set_interval(xfer, 500);
			else
				sc->sc_rx_nerr++;
		}
		else {
			/* disable throttling */
			usbd_xfer_set_interval(xfer, 0);
			sc->sc_rx_nerr = 0;
		}

		for(i = 0; i < aframes; i++) {
			umb_decap(sc, xfer, i);
		}

		/* fall through */
	case USB_ST_SETUP:
		usbd_xfer_set_frame_data(xfer, 0, sc->sc_rx_buf,
				sc->sc_rx_bufsz);
		usbd_xfer_set_frames(xfer, 1);
		usbd_transfer_submit(xfer);

		umb_rxflush(sc);
		break;
	default:
		DPRINTF("rx error: %s\n", usbd_errstr(status));

		/* disable throttling */
		usbd_xfer_set_interval(xfer, 0);

		if (status != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			usbd_xfer_set_frames(xfer, 0);
			usbd_transfer_submit(xfer);
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		}
		else if (++sc->sc_rx_nerr > 100) {
			log(LOG_ERR, "%s: too many rx errors, disabling\n",
			    DEVNAM(sc));
			umb_deactivate(sc->sc_dev);
		}
		break;
	}
}

static void
umb_rxflush(struct umb_softc *sc)
{
	if_t ifp = GET_IFP(sc);
	struct mbuf *m;

	mtx_assert(&sc->sc_mutex, MA_OWNED);

	for (;;) {
		_IF_DEQUEUE(&sc->sc_rx_queue, m);
		if (m == NULL)
			break;

		/*
		 * The USB xfer has been resubmitted so it's safe to unlock now.
		 */
		mtx_unlock(&sc->sc_mutex);
		CURVNET_SET_QUIET(if_getvnet(ifp));
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
			if_input(ifp, m);
		else
			m_freem(m);
		CURVNET_RESTORE();
		mtx_lock(&sc->sc_mutex);
	}
}

static int
umb_encap(struct umb_softc *sc, struct mbuf *m, struct usb_xfer *xfer)
{
	struct ncm_header16 *hdr;
	struct ncm_pointer16 *ptr;
	int	 len;

	KASSERT(sc->sc_tx_m == NULL,
			("Assertion failed in umb_encap()"));

	/* All size constraints have been validated by the caller! */
	hdr = (struct ncm_header16 *)sc->sc_tx_buf;
	ptr = (struct ncm_pointer16 *)(hdr + 1);

	USETDW(hdr->dwSignature, NCM_HDR16_SIG);
	USETW(hdr->wHeaderLength, sizeof (*hdr));
	USETW(hdr->wSequence, sc->sc_tx_seq);
	sc->sc_tx_seq++;
	USETW(hdr->wNdpIndex, sizeof (*hdr));

	len = m->m_pkthdr.len;
	USETDW(ptr->dwSignature, MBIM_NCM_NTH16_SIG(umb_session_id));
	USETW(ptr->wLength, sizeof (*ptr));
	USETW(ptr->wNextNdpIndex, 0);
	USETW(ptr->dgram[0].wDatagramIndex, MBIM_HDR16_LEN);
	USETW(ptr->dgram[0].wDatagramLen, len);
	USETW(ptr->dgram[1].wDatagramIndex, 0);
	USETW(ptr->dgram[1].wDatagramLen, 0);

	KASSERT(len + MBIM_HDR16_LEN <= sc->sc_tx_bufsz,
			("Assertion failed in umb_encap()"));
	m_copydata(m, 0, len, (char *)(ptr + 1));
	sc->sc_tx_m = m;
	len += MBIM_HDR16_LEN;
	USETW(hdr->wBlockLength, len);

	usbd_xfer_set_frame_data(xfer, 0, sc->sc_tx_buf, len);
	usbd_xfer_set_interval(xfer, 0);
	usbd_xfer_set_frames(xfer, 1);

	DPRINTFN(3, "%s: encap %d bytes\n", DEVNAM(sc), len);
	DDUMPN(5, sc->sc_tx_buf, len);
	return 0;
}

static void
umb_txeof(struct usb_xfer *xfer, usb_error_t status)
{
	struct umb_softc *sc = usbd_xfer_softc(xfer);
	if_t ifp = GET_IFP(sc);
	struct mbuf *m;

	DPRINTF("%s(%u) state=%u\n", __func__, status, USB_GET_STATE(xfer));

	mtx_assert(&sc->sc_mutex, MA_OWNED);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);

		umb_txflush(sc);

		/* fall through */
	case USB_ST_SETUP:
tr_setup:
		if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
			break;

		m = if_dequeue(ifp); /* XXX - IFAPI */
		if (m == NULL)
			break;

		if (umb_encap(sc, m, xfer)) {
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			umb_txflush(sc);
			break;
		}

		BPF_MTAP(ifp, m);

		if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
		usbd_transfer_submit(xfer);

		break;

	default:
		umb_txflush(sc);

		/* count output errors */
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		DPRINTF("tx error: %s\n",
				usbd_errstr(status));

		if (status != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
umb_txflush(struct umb_softc *sc)
{
	mtx_assert(&sc->sc_mutex, MA_OWNED);

	if (sc->sc_tx_m != NULL) {
		m_freem(sc->sc_tx_m);
		sc->sc_tx_m = NULL;
	}
}

static void
umb_decap(struct umb_softc *sc, struct usb_xfer *xfer, int frame)
{
	if_t ifp = GET_IFP(sc);
	char *buf;
	int len;
	char	*dp;
	struct ncm_header16 *hdr16;
	struct ncm_header32 *hdr32;
	struct ncm_pointer16 *ptr16;
	struct ncm_pointer16_dgram *dgram16;
	struct ncm_pointer32_dgram *dgram32;
	uint32_t hsig, psig;
	int	 hlen, blen;
	int	 ptrlen, ptroff, dgentryoff;
	uint32_t doff, dlen;
	struct mbuf *m;

	usbd_xfer_frame_data(xfer, frame, (void **)&buf, &len);
	DPRINTFN(4, "recv %d bytes\n", len);
	DDUMPN(5, buf, len);
	if (len < sizeof (*hdr16))
		goto toosmall;

	hdr16 = (struct ncm_header16 *)buf;
	hsig = UGETDW(hdr16->dwSignature);
	hlen = UGETW(hdr16->wHeaderLength);
	if (len < hlen)
		goto toosmall;
	if (len > sc->sc_rx_bufsz) {
		DPRINTF("packet too large (%d)\n", len);
		goto fail;
	}
	switch (hsig) {
	case NCM_HDR16_SIG:
		blen = UGETW(hdr16->wBlockLength);
		ptroff = UGETW(hdr16->wNdpIndex);
		if (hlen != sizeof (*hdr16)) {
			DPRINTF("%s: bad header len %d for NTH16 (exp %zu)\n",
			    DEVNAM(sc), hlen, sizeof (*hdr16));
			goto fail;
		}
		break;
	case NCM_HDR32_SIG:
		hdr32 = (struct ncm_header32 *)hdr16;
		blen = UGETDW(hdr32->dwBlockLength);
		ptroff = UGETDW(hdr32->dwNdpIndex);
		if (hlen != sizeof (*hdr32)) {
			DPRINTF("%s: bad header len %d for NTH32 (exp %zu)\n",
			    DEVNAM(sc), hlen, sizeof (*hdr32));
			goto fail;
		}
		break;
	default:
		DPRINTF("%s: unsupported NCM header signature (0x%08x)\n",
		    DEVNAM(sc), hsig);
		goto fail;
	}
	if (len < blen) {
		DPRINTF("%s: bad NTB len (%d) for %d bytes of data\n",
		    DEVNAM(sc), blen, len);
		goto fail;
	}

	if (len < ptroff)
		goto toosmall;
	ptr16 = (struct ncm_pointer16 *)(buf + ptroff);
	psig = UGETDW(ptr16->dwSignature);
	ptrlen = UGETW(ptr16->wLength);
	if ((uint64_t)len < (uint64_t)ptrlen + (uint64_t)ptroff)
		goto toosmall;
	if (!MBIM_NCM_NTH16_ISISG(psig) && !MBIM_NCM_NTH32_ISISG(psig)) {
		DPRINTF("%s: unsupported NCM pointer signature (0x%08x)\n",
		    DEVNAM(sc), psig);
		goto fail;
	}

	switch (hsig) {
	case NCM_HDR16_SIG:
		dgentryoff = offsetof(struct ncm_pointer16, dgram);
		break;
	case NCM_HDR32_SIG:
		dgentryoff = offsetof(struct ncm_pointer32, dgram);
		break;
	default:
		goto fail;
	}

	while (dgentryoff < ptrlen) {
		switch (hsig) {
		case NCM_HDR16_SIG:
			if (ptroff + dgentryoff < sizeof (*dgram16))
				goto done;
			dgram16 = (struct ncm_pointer16_dgram *)
			    (buf + ptroff + dgentryoff);
			dgentryoff += sizeof (*dgram16);
			dlen = UGETW(dgram16->wDatagramLen);
			doff = UGETW(dgram16->wDatagramIndex);
			break;
		case NCM_HDR32_SIG:
			if (ptroff + dgentryoff < sizeof (*dgram32))
				goto done;
			dgram32 = (struct ncm_pointer32_dgram *)
			    (buf + ptroff + dgentryoff);
			dgentryoff += sizeof (*dgram32);
			dlen = UGETDW(dgram32->dwDatagramLen);
			doff = UGETDW(dgram32->dwDatagramIndex);
			break;
		default:
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			goto done;
		}

		/* Terminating zero entry */
		if (dlen == 0 || doff == 0)
			break;
		if ((uint64_t)len < (uint64_t)dlen + (uint64_t)doff) {
			/* Skip giant datagram but continue processing */
			DPRINTF("%s: datagram too large (%d @ off %d)\n",
			    DEVNAM(sc), dlen, doff);
			continue;
		}

		dp = buf + doff;
		DPRINTFN(3, "%s: decap %d bytes\n", DEVNAM(sc), dlen);
		m = m_devget(dp, dlen, 0, ifp, NULL);
		if (m == NULL) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			continue;
		}

		/* enqueue for later when the lock can be released */
		_IF_ENQUEUE(&sc->sc_rx_queue, m);

		if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

	}
done:
	sc->sc_rx_nerr = 0;
	return;
toosmall:
	DPRINTF("%s: packet too small (%d)\n", DEVNAM(sc), len);
fail:
	if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
}

static usb_error_t
umb_send_encap_command(struct umb_softc *sc, void *data, int len)
{
	usb_device_request_t req;

	if (len > sc->sc_ctrl_len)
		return USB_ERR_INVAL;

	/* XXX FIXME: if (total len > sc->sc_ctrl_len) => must fragment */
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_ENCAPSULATED_COMMAND;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctrl_ifaceno);
	USETW(req.wLength, len);
	mtx_unlock(&sc->sc_mutex);
	DELAY(umb_delay);
	mtx_lock(&sc->sc_mutex);
	return usbd_do_request_flags(sc->sc_udev, &sc->sc_mutex, &req, data, 0,
			NULL, umb_xfer_tout);
}

static int
umb_get_encap_response(struct umb_softc *sc, void *buf, int *len)
{
	usb_device_request_t req;
	usb_error_t err;
	uint16_t l = *len;

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UCDC_GET_ENCAPSULATED_RESPONSE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctrl_ifaceno);
	USETW(req.wLength, l);
	/* XXX FIXME: re-assemble fragments */

	mtx_unlock(&sc->sc_mutex);
	DELAY(umb_delay);
	mtx_lock(&sc->sc_mutex);
	err = usbd_do_request_flags(sc->sc_udev, &sc->sc_mutex, &req, buf,
			USB_SHORT_XFER_OK, &l, umb_xfer_tout);
	if (err == USB_ERR_NORMAL_COMPLETION) {
		*len = l;
		return 1;
	}
	DPRINTF("ctrl recv: %s\n", usbd_errstr(err));
	return 0;
}

static void
umb_ctrl_msg(struct umb_softc *sc, uint32_t req, void *data, int len)
{
	if_t ifp = GET_IFP(sc);
	uint32_t tid;
	struct mbim_msghdr *hdr = data;
	usb_error_t err;

	if (sc->sc_dying)
		return;
	if (len < sizeof (*hdr))
		return;
	tid = ++sc->sc_tid;

	hdr->type = htole32(req);
	hdr->len = htole32(len);
	hdr->tid = htole32(tid);

#ifdef UMB_DEBUG
	if (umb_debug) {
		const char *op, *str;
		if (req == MBIM_COMMAND_MSG) {
			struct mbim_h2f_cmd *c = data;
			if (le32toh(c->op) == MBIM_CMDOP_SET)
				op = "set";
			else
				op = "qry";
			str = umb_cid2str(le32toh(c->cid));
		} else {
			op = "snd";
			str = umb_request2str(req);
		}
		DPRINTF("-> %s %s (tid %u)\n", op, str, tid);
	}
#endif
	err = umb_send_encap_command(sc, data, len);
	if (err != USB_ERR_NORMAL_COMPLETION) {
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_ERR, "%s: send %s msg (tid %u) failed: %s\n",
			    DEVNAM(sc), umb_request2str(req), tid,
			    usbd_errstr(err));

		/* will affect other transactions, too */
		usbd_transfer_stop(sc->sc_xfer[UMB_INTR_RX]);
	} else {
		DPRINTFN(2, "sent %s (tid %u)\n",
		    umb_request2str(req), tid);
		DDUMPN(3, data, len);
	}
	return;
}

static void
umb_open(struct umb_softc *sc)
{
	struct mbim_h2f_openmsg msg;

	memset(&msg, 0, sizeof (msg));
	msg.maxlen = htole32(sc->sc_ctrl_len);
	umb_ctrl_msg(sc, MBIM_OPEN_MSG, &msg, sizeof (msg));
	return;
}

static void
umb_close(struct umb_softc *sc)
{
	struct mbim_h2f_closemsg msg;

	memset(&msg, 0, sizeof (msg));
	umb_ctrl_msg(sc, MBIM_CLOSE_MSG, &msg, sizeof (msg));
}

static int
umb_setpin(struct umb_softc *sc, int op, int is_puk, void *pin, int pinlen,
    void *newpin, int newpinlen)
{
	struct mbim_cid_pin cp;
	int	 off;

	if (pinlen == 0)
		return 0;
	if (pinlen < 0 || pinlen > MBIM_PIN_MAXLEN ||
	    newpinlen < 0 || newpinlen > MBIM_PIN_MAXLEN ||
	    op < 0 || op > MBIM_PIN_OP_CHANGE ||
	    (is_puk && op != MBIM_PIN_OP_ENTER))
		return EINVAL;

	memset(&cp, 0, sizeof (cp));
	cp.type = htole32(is_puk ? MBIM_PIN_TYPE_PUK1 : MBIM_PIN_TYPE_PIN1);

	off = offsetof(struct mbim_cid_pin, data);
	if (!umb_addstr(&cp, sizeof (cp), &off, pin, pinlen,
	    &cp.pin_offs, &cp.pin_size))
		return EINVAL;

	cp.op  = htole32(op);
	if (newpinlen) {
		if (!umb_addstr(&cp, sizeof (cp), &off, newpin, newpinlen,
		    &cp.newpin_offs, &cp.newpin_size))
			return EINVAL;
	} else {
		if ((op == MBIM_PIN_OP_CHANGE) || is_puk)
			return EINVAL;
		if (!umb_addstr(&cp, sizeof (cp), &off, NULL, 0,
		    &cp.newpin_offs, &cp.newpin_size))
			return EINVAL;
	}
	mtx_lock(&sc->sc_mutex);
	umb_cmd(sc, MBIM_CID_PIN, MBIM_CMDOP_SET, &cp, off);
	mtx_unlock(&sc->sc_mutex);
	return 0;
}

static void
umb_setdataclass(struct umb_softc *sc)
{
	struct mbim_cid_registration_state rs;
	uint32_t	 classes;

	if (sc->sc_info.supportedclasses == MBIM_DATACLASS_NONE)
		return;

	memset(&rs, 0, sizeof (rs));
	rs.regaction = htole32(MBIM_REGACTION_AUTOMATIC);
	classes = sc->sc_info.supportedclasses;
	if (sc->sc_info.preferredclasses != MBIM_DATACLASS_NONE)
		classes &= sc->sc_info.preferredclasses;
	rs.data_class = htole32(classes);
	mtx_lock(&sc->sc_mutex);
	umb_cmd(sc, MBIM_CID_REGISTER_STATE, MBIM_CMDOP_SET, &rs, sizeof (rs));
	mtx_unlock(&sc->sc_mutex);
}

static void
umb_radio(struct umb_softc *sc, int on)
{
	struct mbim_cid_radio_state s;

	DPRINTF("set radio %s\n", on ? "on" : "off");
	memset(&s, 0, sizeof (s));
	s.state = htole32(on ? MBIM_RADIO_STATE_ON : MBIM_RADIO_STATE_OFF);
	umb_cmd(sc, MBIM_CID_RADIO_STATE, MBIM_CMDOP_SET, &s, sizeof (s));
}

static void
umb_allocate_cid(struct umb_softc *sc)
{
	umb_cmd1(sc, MBIM_CID_DEVICE_CAPS, MBIM_CMDOP_SET,
	    umb_qmi_alloc_cid, sizeof (umb_qmi_alloc_cid), umb_uuid_qmi_mbim);
}

static void
umb_send_fcc_auth(struct umb_softc *sc)
{
	uint8_t	 fccauth[sizeof (umb_qmi_fcc_auth)];

	if (sc->sc_cid == -1) {
		DPRINTF("missing CID, cannot send FCC auth\n");
		umb_allocate_cid(sc);
		return;
	}
	memcpy(fccauth, umb_qmi_fcc_auth, sizeof (fccauth));
	fccauth[UMB_QMI_CID_OFFS] = sc->sc_cid;
	umb_cmd1(sc, MBIM_CID_DEVICE_CAPS, MBIM_CMDOP_SET,
	    fccauth, sizeof (fccauth), umb_uuid_qmi_mbim);
}

static void
umb_packet_service(struct umb_softc *sc, int attach)
{
	struct mbim_cid_packet_service	s;

	DPRINTF("%s packet service\n",
	    attach ? "attach" : "detach");
	memset(&s, 0, sizeof (s));
	s.action = htole32(attach ?
	    MBIM_PKTSERVICE_ACTION_ATTACH : MBIM_PKTSERVICE_ACTION_DETACH);
	umb_cmd(sc, MBIM_CID_PACKET_SERVICE, MBIM_CMDOP_SET, &s, sizeof (s));
}

static void
umb_connect(struct umb_softc *sc)
{
	if_t ifp = GET_IFP(sc);

	if (sc->sc_info.regstate == MBIM_REGSTATE_ROAMING && !sc->sc_roaming) {
		log(LOG_INFO, "%s: connection disabled in roaming network\n",
		    DEVNAM(sc));
		return;
	}
	if (if_getflags(ifp) & IFF_DEBUG)
		log(LOG_DEBUG, "%s: connecting ...\n", DEVNAM(sc));
	umb_send_connect(sc, MBIM_CONNECT_ACTIVATE);
}

static void
umb_disconnect(struct umb_softc *sc)
{
	if_t ifp = GET_IFP(sc);

	if (if_getflags(ifp) & IFF_DEBUG)
		log(LOG_DEBUG, "%s: disconnecting ...\n", DEVNAM(sc));
	umb_send_connect(sc, MBIM_CONNECT_DEACTIVATE);
}

static void
umb_send_connect(struct umb_softc *sc, int command)
{
	struct mbim_cid_connect *c;
	int	 off;

	/* Too large for the stack */
	mtx_unlock(&sc->sc_mutex);
	c = malloc(sizeof (*c), M_MBIM_CID_CONNECT, M_WAITOK | M_ZERO);
	mtx_lock(&sc->sc_mutex);
	c->sessionid = htole32(umb_session_id);
	c->command = htole32(command);
	off = offsetof(struct mbim_cid_connect, data);
	if (!umb_addstr(c, sizeof (*c), &off, sc->sc_info.apn,
	    sc->sc_info.apnlen, &c->access_offs, &c->access_size))
		goto done;
	if (!umb_addstr(c, sizeof (*c), &off, sc->sc_info.username,
	    sc->sc_info.usernamelen, &c->user_offs, &c->user_size))
		goto done;
	if (!umb_addstr(c, sizeof (*c), &off, sc->sc_info.password,
	    sc->sc_info.passwordlen, &c->passwd_offs, &c->passwd_size))
		goto done;
	c->authprot = htole32(MBIM_AUTHPROT_NONE);
	c->compression = htole32(MBIM_COMPRESSION_NONE);
	c->iptype = htole32(MBIM_CONTEXT_IPTYPE_IPV4);
	memcpy(c->context, umb_uuid_context_internet, sizeof (c->context));
	umb_cmd(sc, MBIM_CID_CONNECT, MBIM_CMDOP_SET, c, off);
done:
	free(c, M_MBIM_CID_CONNECT);
	return;
}

static void
umb_qry_ipconfig(struct umb_softc *sc)
{
	struct mbim_cid_ip_configuration_info ipc;

	memset(&ipc, 0, sizeof (ipc));
	ipc.sessionid = htole32(umb_session_id);
	umb_cmd(sc, MBIM_CID_IP_CONFIGURATION, MBIM_CMDOP_QRY,
	    &ipc, sizeof (ipc));
}

static void
umb_cmd(struct umb_softc *sc, int cid, int op, const void *data, int len)
{
	umb_cmd1(sc, cid, op, data, len, umb_uuid_basic_connect);
}

static void
umb_cmd1(struct umb_softc *sc, int cid, int op, const void *data, int len,
    uint8_t *uuid)
{
	struct mbim_h2f_cmd *cmd;
	int	totlen;

	/* XXX FIXME support sending fragments */
	if (sizeof (*cmd) + len > sc->sc_ctrl_len) {
		DPRINTF("set %s msg too long: cannot send\n",
		    umb_cid2str(cid));
		return;
	}
	cmd = sc->sc_ctrl_msg;
	memset(cmd, 0, sizeof (*cmd));
	cmd->frag.nfrag = htole32(1);
	memcpy(cmd->devid, uuid, sizeof (cmd->devid));
	cmd->cid = htole32(cid);
	cmd->op = htole32(op);
	cmd->infolen = htole32(len);
	totlen = sizeof (*cmd);
	if (len > 0) {
		memcpy(cmd + 1, data, len);
		totlen += len;
	}
	umb_ctrl_msg(sc, MBIM_COMMAND_MSG, cmd, totlen);
}

static void
umb_command_done(struct umb_softc *sc, void *data, int len)
{
	struct mbim_f2h_cmddone *cmd = data;
	if_t ifp = GET_IFP(sc);
	uint32_t status;
	uint32_t cid;
	uint32_t infolen;
	int	 qmimsg = 0;

	if (len < sizeof (*cmd)) {
		DPRINTF("discard short %s message\n",
		    umb_request2str(le32toh(cmd->hdr.type)));
		return;
	}
	cid = le32toh(cmd->cid);
	if (memcmp(cmd->devid, umb_uuid_basic_connect, sizeof (cmd->devid))) {
		if (memcmp(cmd->devid, umb_uuid_qmi_mbim,
		    sizeof (cmd->devid))) {
			DPRINTF("discard %s message for other UUID '%s'\n",
			    umb_request2str(le32toh(cmd->hdr.type)),
			    umb_uuid2str(cmd->devid));
			return;
		} else
			qmimsg = 1;
	}

	status = le32toh(cmd->status);
	switch (status) {
	case MBIM_STATUS_SUCCESS:
		break;
	case MBIM_STATUS_NOT_INITIALIZED:
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_ERR, "%s: SIM not initialized (PIN missing)\n",
			    DEVNAM(sc));
		return;
	case MBIM_STATUS_PIN_REQUIRED:
		sc->sc_info.pin_state = UMB_PIN_REQUIRED;
		/*FALLTHROUGH*/
	default:
		if (if_getflags(ifp) & IFF_DEBUG)
			log(LOG_ERR, "%s: set/qry %s failed: %s\n", DEVNAM(sc),
			    umb_cid2str(cid), umb_status2str(status));
		return;
	}

	infolen = le32toh(cmd->infolen);
	if (len < sizeof (*cmd) + infolen) {
		DPRINTF("discard truncated %s message (want %d, got %d)\n",
		    umb_cid2str(cid),
		    (int)sizeof (*cmd) + infolen, len);
		return;
	}
	if (qmimsg) {
		if (sc->sc_flags & UMBFLG_FCC_AUTH_REQUIRED)
			umb_decode_qmi(sc, cmd->info, infolen);
	} else {
		DPRINTFN(2, "set/qry %s done\n",
		    umb_cid2str(cid));
		umb_decode_cid(sc, cid, cmd->info, infolen);
	}
}

static void
umb_decode_cid(struct umb_softc *sc, uint32_t cid, void *data, int len)
{
	int	 ok = 1;

	switch (cid) {
	case MBIM_CID_DEVICE_CAPS:
		ok = umb_decode_devices_caps(sc, data, len);
		break;
	case MBIM_CID_SUBSCRIBER_READY_STATUS:
		ok = umb_decode_subscriber_status(sc, data, len);
		break;
	case MBIM_CID_RADIO_STATE:
		ok = umb_decode_radio_state(sc, data, len);
		break;
	case MBIM_CID_PIN:
		ok = umb_decode_pin(sc, data, len);
		break;
	case MBIM_CID_REGISTER_STATE:
		ok = umb_decode_register_state(sc, data, len);
		break;
	case MBIM_CID_PACKET_SERVICE:
		ok = umb_decode_packet_service(sc, data, len);
		break;
	case MBIM_CID_SIGNAL_STATE:
		ok = umb_decode_signal_state(sc, data, len);
		break;
	case MBIM_CID_CONNECT:
		ok = umb_decode_connect_info(sc, data, len);
		break;
	case MBIM_CID_IP_CONFIGURATION:
		ok = umb_decode_ip_configuration(sc, data, len);
		break;
	default:
		/*
		 * Note: the above list is incomplete and only contains
		 *	mandatory CIDs from the BASIC_CONNECT set.
		 *	So alternate values are not unusual.
		 */
		DPRINTFN(4, "ignore %s\n", umb_cid2str(cid));
		break;
	}
	if (!ok)
		DPRINTF("discard %s with bad info length %d\n",
		    umb_cid2str(cid), len);
	return;
}

static void
umb_decode_qmi(struct umb_softc *sc, uint8_t *data, int len)
{
	uint8_t	srv;
	uint16_t msg, tlvlen;
	uint32_t val;

#define UMB_QMI_QMUXLEN		6
	if (len < UMB_QMI_QMUXLEN)
		goto tooshort;

	srv = data[4];
	data += UMB_QMI_QMUXLEN;
	len -= UMB_QMI_QMUXLEN;

#define UMB_GET16(p)	((uint16_t)*p | (uint16_t)*(p + 1) << 8)
#define UMB_GET32(p)	((uint32_t)*p | (uint32_t)*(p + 1) << 8 | \
			    (uint32_t)*(p + 2) << 16 |(uint32_t)*(p + 3) << 24)
	switch (srv) {
	case 0:	/* ctl */
#define UMB_QMI_CTLLEN		6
		if (len < UMB_QMI_CTLLEN)
			goto tooshort;
		msg = UMB_GET16(&data[2]);
		tlvlen = UMB_GET16(&data[4]);
		data += UMB_QMI_CTLLEN;
		len -= UMB_QMI_CTLLEN;
		break;
	case 2:	/* dms  */
#define UMB_QMI_DMSLEN		7
		if (len < UMB_QMI_DMSLEN)
			goto tooshort;
		msg = UMB_GET16(&data[3]);
		tlvlen = UMB_GET16(&data[5]);
		data += UMB_QMI_DMSLEN;
		len -= UMB_QMI_DMSLEN;
		break;
	default:
		DPRINTF("discard QMI message for unknown service type %d\n",
		    srv);
		return;
	}

	if (len < tlvlen)
		goto tooshort;

#define UMB_QMI_TLVLEN		3
	while (len > 0) {
		if (len < UMB_QMI_TLVLEN)
			goto tooshort;
		tlvlen = UMB_GET16(&data[1]);
		if (len < UMB_QMI_TLVLEN + tlvlen)
			goto tooshort;
		switch (data[0]) {
		case 1:	/* allocation info */
			if (msg == 0x0022) {	/* Allocate CID */
				if (tlvlen != 2 || data[3] != 2) /* dms */
					break;
				sc->sc_cid = data[4];
				DPRINTF("QMI CID %d allocated\n",
				    sc->sc_cid);
				umb_newstate(sc, UMB_S_CID, UMB_NS_DONT_DROP);
			}
			break;
		case 2:	/* response */
			if (tlvlen != sizeof (val))
				break;
			val = UMB_GET32(&data[3]);
			switch (msg) {
			case 0x0022:	/* Allocate CID */
				if (val != 0) {
					log(LOG_ERR, "%s: allocation of QMI CID"
					    " failed, error 0x%x\n", DEVNAM(sc),
					    val);
					/* XXX how to proceed? */
					return;
				}
				break;
			case 0x555f:	/* Send FCC Authentication */
				if (val == 0)
					DPRINTF("%s: send FCC "
					    "Authentication succeeded\n",
					    DEVNAM(sc));
				else if (val == 0x001a0001)
					DPRINTF("%s: FCC Authentication "
					    "not required\n", DEVNAM(sc));
				else
					log(LOG_INFO, "%s: send FCC "
					    "Authentication failed, "
					    "error 0x%x\n", DEVNAM(sc), val);

				/* FCC Auth is needed only once after power-on*/
				sc->sc_flags &= ~UMBFLG_FCC_AUTH_REQUIRED;

				/* Try to proceed anyway */
				DPRINTF("init: turning radio on ...\n");
				umb_radio(sc, 1);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		data += UMB_QMI_TLVLEN + tlvlen;
		len -= UMB_QMI_TLVLEN + tlvlen;
	}
	return;

tooshort:
	DPRINTF("discard short QMI message\n");
	return;
}

static void
umb_intr(struct usb_xfer *xfer, usb_error_t status)
{
	struct umb_softc *sc = usbd_xfer_softc(xfer);
	struct usb_cdc_notification notification;
	struct usb_page_cache *pc;
	if_t ifp = GET_IFP(sc);
	int	 total_len;

	mtx_assert(&sc->sc_mutex, MA_OWNED);

	/* FIXME use actlen or total_len? */
	usbd_xfer_status(xfer, &total_len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("Received %d bytes\n", total_len);

		if (total_len < UCDC_NOTIFICATION_LENGTH) {
			DPRINTF("short notification (%d<%d)\n",
					total_len, UCDC_NOTIFICATION_LENGTH);
			return;
		}

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, &notification, sizeof (notification));

		if (notification.bmRequestType != UCDC_NOTIFICATION) {
			DPRINTF("unexpected notification (type=0x%02x)\n",
					notification.bmRequestType);
			return;
		}

		switch (notification.bNotification) {
		case UCDC_N_NETWORK_CONNECTION:
			if (if_getflags(ifp) & IFF_DEBUG)
				log(LOG_DEBUG, "%s: network %sconnected\n",
						DEVNAM(sc),
						UGETW(notification.wValue)
						? "" : "dis");
			break;
		case UCDC_N_RESPONSE_AVAILABLE:
			DPRINTFN(2, "umb_intr: response available\n");
			++sc->sc_nresp;
			umb_add_task(sc, umb_get_response_task,
					&sc->sc_proc_get_response_task[0].hdr,
					&sc->sc_proc_get_response_task[1].hdr,
					0);
			break;
		case UCDC_N_CONNECTION_SPEED_CHANGE:
			DPRINTFN(2, "umb_intr: connection speed changed\n");
			break;
		default:
			DPRINTF("unexpected notification (0x%02x)\n",
					notification.bNotification);
			break;
		}
		/* fallthrough */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;
	default:
		if (status != USB_ERR_CANCELLED) {
			/* start clear stall */
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

/*
 * Diagnostic routines
 */
static char *
umb_ntop(struct sockaddr *sa)
{
#define NUMBUFS		4
	static char astr[NUMBUFS][INET_ADDRSTRLEN];
	static unsigned nbuf = 0;
	char	*s;

	s = astr[nbuf++];
	if (nbuf >= NUMBUFS)
		nbuf = 0;

	switch (sa->sa_family) {
	case AF_INET:
	default:
		inet_ntop(AF_INET, &satosin(sa)->sin_addr, s, sizeof (astr[0]));
		break;
	case AF_INET6:
		inet_ntop(AF_INET6, &satosin6(sa)->sin6_addr, s,
		    sizeof (astr[0]));
		break;
	}
	return s;
}

#ifdef UMB_DEBUG
static char *
umb_uuid2str(uint8_t uuid[MBIM_UUID_LEN])
{
	static char uuidstr[2 * MBIM_UUID_LEN + 5];

#define UUID_BFMT	"%02X"
#define UUID_SEP	"-"
	snprintf(uuidstr, sizeof (uuidstr),
	    UUID_BFMT UUID_BFMT UUID_BFMT UUID_BFMT UUID_SEP
	    UUID_BFMT UUID_BFMT UUID_SEP
	    UUID_BFMT UUID_BFMT UUID_SEP
	    UUID_BFMT UUID_BFMT UUID_SEP
	    UUID_BFMT UUID_BFMT UUID_BFMT UUID_BFMT UUID_BFMT UUID_BFMT,
	    uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5],
	    uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11],
	    uuid[12], uuid[13], uuid[14], uuid[15]);
	return uuidstr;
}

static void
umb_dump(void *buf, int len)
{
	int	 i = 0;
	uint8_t	*c = buf;

	if (len == 0)
		return;
	while (i < len) {
		if ((i % 16) == 0) {
			if (i > 0)
				log(LOG_DEBUG, "\n");
			log(LOG_DEBUG, "%4d:  ", i);
		}
		log(LOG_DEBUG, " %02x", *c);
		c++;
		i++;
	}
	log(LOG_DEBUG, "\n");
}
#endif /* UMB_DEBUG */

DRIVER_MODULE(umb, uhub, umb_driver, NULL, NULL);
MODULE_DEPEND(umb, usb, 1, 1, 1);
