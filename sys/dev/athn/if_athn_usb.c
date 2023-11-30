/*	$OpenBSD: if_athn_usb.c,v 1.65 2022/07/10 21:13:41 bluhm Exp $	*/

/*-
 * Copyright (c) 2011 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2018 Stefan Sperling <stsp@openbsd.org>
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
 */

/*
 * USB front-end for Atheros AR9271 and AR7010 chipsets.
 */

// #include "bpfilter.h"
#define NBPFILTER 0 // TODO ?
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/limits.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/firmware.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>
#include <sys/kdb.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <machine/bus.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>

#include "athnreg.h"
#include "ar5008reg.h"
#include "athnvar.h"

#include <openbsd/openbsd_usbdevs.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_device.h>
//#include <dev/usb/usb_core.h>
#include <dev/usb/usbdi_util.h>

#include "openbsd_adapt.h"
#include "if_athn_usb.h"
#include "if_athn_usb_fw.h"

unsigned int ifq_oactive = 0;

static const STRUCT_USB_HOST_ID athn_usb_devs[] = {
	{ USB_VP(USB_VENDOR_ATHEROS2, USB_PRODUCT_ATHEROS2_AR9271_1) },
	{ USB_VP(USB_VENDOR_ATHEROS2, USB_PRODUCT_ATHEROS2_AR9271_2) },
	{ USB_VP(USB_VENDOR_ATHEROS2, USB_PRODUCT_ATHEROS2_AR9271_3)},
	{ USB_VP(USB_VENDOR_AZUREWAVE, USB_PRODUCT_AZUREWAVE_AR9271_1) },
	{ USB_VP(USB_VENDOR_AZUREWAVE, USB_PRODUCT_AZUREWAVE_AR9271_2) },
	{ USB_VP(USB_VENDOR_AZUREWAVE, USB_PRODUCT_AZUREWAVE_AR9271_3) },
	{ USB_VP(USB_VENDOR_AZUREWAVE, USB_PRODUCT_AZUREWAVE_AR9271_4) },
	{ USB_VP(USB_VENDOR_AZUREWAVE, USB_PRODUCT_AZUREWAVE_AR9271_5) },
	{ USB_VP(USB_VENDOR_AZUREWAVE, USB_PRODUCT_AZUREWAVE_AR9271_6) },
	{ USB_VP(USB_VENDOR_DLINK2, USB_PRODUCT_DLINK2_AR9271) },
	{ USB_VP(USB_VENDOR_LITEON, USB_PRODUCT_LITEON_AR9271) },
	{ USB_VP(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_WNA1100) },
	{ USB_VP(USB_VENDOR_VIA, USB_PRODUCT_VIA_AR9271) },
	{ USB_VPI(USB_VENDOR_ACCTON, USB_PRODUCT_ACCTON_AR9280, ATHN_USB_FLAG_AR7010) },
	{ USB_VPI(USB_VENDOR_ACTIONTEC, USB_PRODUCT_ACTIONTEC_AR9287, ATHN_USB_FLAG_AR7010) },
	{ USB_VPI(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_WNDA3200, ATHN_USB_FLAG_AR7010) },
	{ USB_VPI(USB_VENDOR_PANASONIC, USB_PRODUCT_PANASONIC_N5HBZ0000055, ATHN_USB_FLAG_AR7010) },
	{ USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_UWABR100, ATHN_USB_FLAG_AR7010) },
	{ USB_VPI(USB_VENDOR_ATHEROS2, USB_PRODUCT_ATHEROS2_AR9280, ATHN_USB_FLAG_AR7010) },
	{ USB_VPI(USB_VENDOR_ATHEROS2, USB_PRODUCT_ATHEROS2_AR9287, ATHN_USB_FLAG_AR7010) }
};
#define athn_usb_lookup(uaa)	\
	(usbd_lookup_id_by_uaa(athn_usb_devs, sizeof(athn_usb_devs), uaa))

static device_probe_t	athn_usb_match;
static device_attach_t	athn_usb_attach;
static device_detach_t	athn_usb_detach;

// Temp
static void
ar9271_load_ani(struct athn_softc *sc)
{
#if NATHN_USB > 0
	/* Write ANI registers. */
	AR_WRITE(sc, AR_PHY_DESIRED_SZ, 0x6d4000e2);
	AR_WRITE(sc, AR_PHY_AGC_CTL1,   0x3139605e);
	AR_WRITE(sc, AR_PHY_FIND_SIG,   0x7ec84d2e);
	AR_WRITE(sc, AR_PHY_SFCORR_LOW, 0x06903881);
	AR_WRITE(sc, AR_PHY_SFCORR,     0x5ac640d0);
	AR_WRITE(sc, AR_PHY_CCK_DETECT, 0x803e68c8);
	AR_WRITE(sc, AR_PHY_TIMING5,    0xd00a8007);
	AR_WRITE(sc, AR_PHY_SFCORR_EXT, 0x05eea6d4);
	AR_WRITE_BARRIER(sc);
#endif	/* NATHN_USB */
}

void		athn_usb_attachhook(device_t self);
int		athn_usb_open_pipes(struct athn_usb_softc *);
void		athn_usb_close_pipes(struct athn_usb_softc *);
static int	athn_alloc_list(struct athn_usb_softc *sc, struct athn_usb_data data[],
				int ndata, int maxsz);
static int	athn_free_list(struct athn_usb_softc *sc, struct athn_usb_data data[],
						   int ndata);
int		athn_usb_alloc_rx_list(struct athn_usb_softc *);
void		athn_usb_free_rx_list(struct athn_usb_softc *);
int		athn_usb_alloc_tx_list(struct athn_usb_softc *);
void		athn_usb_free_tx_list(struct athn_usb_softc *);
int		athn_usb_alloc_tx_cmd(struct athn_usb_softc *);
void		athn_usb_free_tx_cmd(struct athn_usb_softc *);
void		athn_usb_task(void *, int);
void		athn_usb_do_async(struct athn_usb_softc *,
		    void (*)(struct athn_usb_softc *, void *), void *, int);
void		athn_usb_wait_async(struct athn_usb_softc *);
int		athn_usb_htc_msg(struct athn_usb_softc *, uint16_t, void *,
		    int);
int		athn_usb_htc_setup(struct athn_usb_softc *);
int		athn_usb_htc_connect_svc(struct athn_usb_softc *, uint16_t,
		    uint8_t, uint8_t, uint8_t *);
int		athn_usb_wmi_xcmd(struct athn_usb_softc *, uint16_t, void *,
		    int, void *);
int		athn_usb_read_rom(struct athn_softc *);
uint32_t	athn_usb_read(struct athn_softc *, uint32_t);
void		athn_usb_write(struct athn_softc *, uint32_t, uint32_t);
void		athn_usb_write_barrier(struct athn_softc *);
int		athn_usb_media_change(struct ifnet *);
void		athn_usb_next_scan(void *, int);
int		athn_usb_newstate(struct ieee80211vap *, enum ieee80211_state,
		    int);
//void		athn_usb_newstate_cb(struct athn_usb_softc *, void *);
void		athn_usb_newassoc(struct ieee80211com *,
		    struct ieee80211_node *, int);
void		athn_usb_newassoc_cb(struct athn_usb_softc *, void *);
struct ieee80211_node *athn_usb_node_alloc(struct ieee80211com *);
void		athn_usb_count_active_sta(void *, struct ieee80211_node *);
void		athn_usb_newauth_cb(struct athn_usb_softc *, void *);
int		athn_usb_newauth(struct ieee80211com *,
		    struct ieee80211_node *, int, uint16_t);
void		athn_usb_node_free(struct ieee80211com *,
		    struct ieee80211_node *);
void		athn_usb_node_free_cb(struct athn_usb_softc *, void *);
int		athn_usb_ampdu_tx_start(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
void		athn_usb_ampdu_tx_start_cb(struct athn_usb_softc *, void *);
void		athn_usb_ampdu_tx_stop(struct ieee80211com *,
		    struct ieee80211_node *, uint8_t);
void		athn_usb_ampdu_tx_stop_cb(struct athn_usb_softc *, void *);
void		athn_usb_clean_nodes(void *, struct ieee80211_node *);
int		athn_usb_create_node(struct athn_usb_softc *,
		    struct ieee80211_node *);
int		athn_usb_node_set_rates(struct athn_usb_softc *,
		    struct ieee80211_node *);
int		athn_usb_remove_node(struct athn_usb_softc *,
		    struct ieee80211_node *);
void		athn_usb_rx_enable(struct athn_softc *);
int		athn_set_chan(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *);
int		athn_usb_switch_chan(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
void		athn_usb_updateedca(struct ieee80211com *);
void		athn_usb_updateedca_cb(struct athn_usb_softc *, void *);
void		athn_usb_updateslot(struct ieee80211com *);
void		athn_usb_updateslot_cb(struct athn_usb_softc *, void *);
int		athn_usb_set_key(struct ieee80211com *,
		    struct ieee80211_node *, struct ieee80211_key *);
void		athn_usb_set_key_cb(struct athn_usb_softc *, void *);
void		athn_usb_delete_key(struct ieee80211com *,
		    struct ieee80211_node *, struct ieee80211_key *);
void		athn_usb_delete_key_cb(struct athn_usb_softc *, void *);
void		athn_usb_bcneof(struct usb_xfer *xfer, void *priv,
			usb_error_t status);
void		athn_usb_swba(struct athn_usb_softc *);
void		athn_usb_tx_status(void *, struct ieee80211_node *);
void		athn_usb_rx_wmi_ctrl(struct athn_usb_softc *, uint8_t *, int);
void		athn_usb_intr(struct usb_xfer *, void *,
		    usb_error_t);
void		athn_usb_rx_radiotap(struct athn_softc *, struct mbuf *,
		    struct ar_rx_status *);
void		athn_usb_rx_frame(struct athn_usb_softc *, struct mbuf */*,
		    struct mbuf_list **/); //MichalP: Uses mbuf_list which doesn't exist in FreeBSD
void		athn_usb_rxeof(struct usb_xfer *, struct athn_usb_data*);
void		athn_usb_txeof(struct usb_xfer *, struct athn_usb_data *);
int		athn_usb_tx(struct athn_softc *, struct mbuf *,
		    struct ieee80211_node *);
void		athn_usb_start(struct ifnet *);
void		athn_usb_watchdog(struct ifnet *);
int		athn_usb_ioctl(struct ifnet *, u_long, caddr_t);
int		athn_usb_init(struct ifnet *);
void		athn_usb_stop(struct ifnet *);
void		ar9271_load_ani(struct athn_softc *);
int		ar5008_ccmp_decap(struct athn_softc *, struct mbuf *,
		    struct ieee80211_node *);
int		ar5008_ccmp_encap(struct mbuf *, u_int, struct ieee80211_key *);

/* Shortcut. */
#define athn_usb_wmi_cmd(sc, cmd_id)	\
	athn_usb_wmi_xcmd(sc, cmd_id, NULL, 0, NULL)

/* Extern functions. */
void		athn_led_init(struct athn_softc *);
void		athn_set_led(struct athn_softc *, int);
void		athn_btcoex_init(struct athn_softc *);
void		athn_set_rxfilter(struct athn_softc *, uint32_t);
int		athn_reset(struct athn_softc *, int);
void		athn_init_pll(struct athn_softc *,
		    const struct ieee80211_channel *);
int		athn_set_power_awake(struct athn_softc *);
void		athn_set_power_sleep(struct athn_softc *);
void		athn_reset_key(struct athn_softc *, int);
int		athn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		athn_delete_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		athn_rx_start(struct athn_softc *);
void		athn_set_sta_timers(struct athn_softc *);
void		athn_set_hostap_timers(struct athn_softc *);
void		athn_set_opmode(struct athn_softc *);
void		athn_set_bss(struct athn_softc *, struct ieee80211_node *);
int		athn_hw_reset(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *, int);
void		athn_updateedca(struct ieee80211com *);
void		athn_updateslot(struct ieee80211com *);

static void athn_if_intr_rx_callback(struct usb_xfer *xfer, usb_error_t error);
static void athn_if_intr_tx_callback(struct usb_xfer *xfer, usb_error_t error);
static void athn_if_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error);
static void athn_if_bulk_tx_callback(struct usb_xfer *xfer, usb_error_t error);

static struct usb_config athn_if_config[ATHN_N_XFER] = {
        [ATHN_BULK_TX] = { // MichalP: standard I/O
                .type = UE_BULK,
                .endpoint = AR_PIPE_TX_DATA,
                .direction = UE_DIR_OUT,
                .bufsize = 512,
                .flags = {.pipe_bof = 1,.force_short_xfer = 1,},
                .callback = athn_if_bulk_tx_callback,
				.timeout = ATHN_USB_TX_TIMEOUT
        }, // MichalP: standard I/O
        [ATHN_BULK_RX] = {
                .type = UE_BULK,
                .endpoint = AR_PIPE_RX_DATA,
                .direction = UE_DIR_IN, // .direction = UE_DIR_IN,
                .bufsize = 512,
                .flags = {.ext_buffer=1,.pipe_bof = 1,.short_xfer_ok = 1,},
                .callback = athn_if_bulk_rx_callback,
        }, // MichalP: what the device sends to us (CMD replies)
        [ATHN_BULK_IRQ] = {
                .type = UE_INTERRUPT,
                .endpoint = AR_PIPE_RX_INTR,
                .direction = UE_DIR_IN,
                .bufsize = 64,
                .flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
                .callback = athn_if_intr_rx_callback,
				.interval = 1
        }, // MichalP: what we are sending to the device (CMDs)
		[ATHN_BULK_CMD] = {
                .type = UE_INTERRUPT,
                .endpoint = AR_PIPE_TX_INTR,
                .direction = UE_DIR_OUT,
                .bufsize = 64,
                .flags = {.pipe_bof = 1,.force_short_xfer = 1,},
                .callback = athn_if_intr_tx_callback,
			    .timeout = 5000,	/* ms */
				.interval = 1
        }
};

#define	INFSLP	UINT64_MAX
#define NSEC_PER_SEC 1000000000

// TODO MichalP: we use msleep with mtx so this could be obsolete
#if OpenBSD_ONLY
int
tsleep_nsec(const void *ident, int priority, const char *wmesg,
    uint64_t nsecs)
{
	uint64_t to_ticks;
	int tick_nsec = (NSEC_PER_SEC + hz / 2) / hz;

	if (nsecs == INFSLP)
		return tsleep(ident, priority, wmesg, 0);
#ifdef DIAGNOSTIC
	if (nsecs == 0) {
		log(LOG_WARNING,
		    "%s: %s[%d]: %s: trying to sleep zero nanoseconds\n",
		    __func__, curproc->p_p->ps_comm, curproc->p_p->ps_pid,
		    wmesg);
	}
#endif
	/*
	 * We want to sleep at least nsecs nanoseconds worth of ticks.
	 *
	 *  - Clamp nsecs to prevent arithmetic overflow.
	 *
	 *  - Round nsecs up to account for any nanoseconds that do not
	 *    divide evenly into tick_nsec, otherwise we'll lose them to
	 *    integer division in the next step.  We add (tick_nsec - 1)
	 *    to keep from introducing a spurious tick if there are no
	 *    such nanoseconds, i.e. nsecs % tick_nsec == 0.
	 *
	 *  - Divide the rounded value to a count of ticks.  We divide
	 *    by (tick_nsec + 1) to discard the extra tick introduced if,
	 *    before rounding, nsecs % tick_nsec == 1.
	 *
	 *  - Finally, add a tick to the result.  We need to wait out
	 *    the current tick before we can begin counting our interval,
	 *    as we do not know how much time has elapsed since the
	 *    current tick began.
	 */
	nsecs = MIN(nsecs, UINT64_MAX - tick_nsec);
	to_ticks = (nsecs + tick_nsec - 1) / (tick_nsec + 1) + 1;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	return tsleep(ident, priority, wmesg, (int)to_ticks);
}
#endif

static device_method_t athn_usb_methods[] = {
	DEVMETHOD(device_probe,		athn_usb_match),
	DEVMETHOD(device_attach,	athn_usb_attach),
	DEVMETHOD(device_detach,	athn_usb_detach),

	DEVMETHOD_END
};

static driver_t athn_usb_driver = {
	.name = "athn",
	.methods = athn_usb_methods,
	.size = sizeof(struct athn_usb_softc)
};

/* Temporary to nofiy that the module was loaded TODO MichalP: this can be removed at somepoint*/
static int
athn_usb_load(struct module *m, int what, void *arg)
{
	int error = 0;

	switch (what) {
	case MOD_LOAD:
		uprintf("Athn KLD loaded.\n");
		break;
	case MOD_UNLOAD:
		uprintf("Athn KLD unloaded.\n");
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return(error);
}

DRIVER_MODULE(athn_usb, uhub, athn_usb_driver, athn_usb_load, NULL);
MODULE_DEPEND(athn_usb, usb, 1, 1, 1);
MODULE_DEPEND(athn_usb, wlan, 1, 1, 1);
MODULE_VERSION(athn_usb, 1);
USB_PNP_HOST_INFO(athn_usb_devs);

static int
athn_usb_match(device_t self)
{
	struct usb_attach_arg *uaa =  device_get_ivars(self);
	int result = 0;

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != 0)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	result = athn_usb_lookup(uaa);

	printf("athn_usb_lookup called with result %d \n", result);

	if (result == 0) {
		printf("serial: %s \n", usb_get_serial(uaa->device));
		printf("product: %s-%s \n", usb_get_manufacturer(uaa->device),
		    usb_get_product(uaa->device));
		printf("ProductID: 0x%x \n", uaa->info.idProduct);
		printf("VendorID: 0x%x \n", uaa->info.idVendor);
	}

	return result;
}

static int
athn_usb_attach(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct athn_usb_softc *usc =  device_get_softc(self);
	struct athn_softc *sc = &usc->sc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	usb_error_t error;
	struct usb_endpoint *ep, *ep_end;

	device_set_usb_desc(self);
	usc->sc_udev = uaa->device;
	sc->sc_dev = self;
	ic->ic_name = device_get_nameunit(self);

	usc->flags = uaa->driver_info;
	sc->flags |= ATHN_FLAG_USB;
#ifdef notyet
	/* Check if it is a combo WiFi+Bluetooth (WB193) device. */
	if (strncmp(product, "wb193", 5) == 0)
		sc->flags |= ATHN_FLAG_BTCOEX3WIRE;
#endif

	sc->ops.read = athn_usb_read;
	sc->ops.write = athn_usb_write;
	sc->ops.write_barrier = athn_usb_write_barrier;

	mtx_init(&sc->sc_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK, MTX_DEF);

	// MichalP calib_to timeout missing don't know how to put it all together
	TIMEOUT_TASK_INIT(taskqueue_thread, &sc->scan_to, 0, athn_usb_next_scan, sc);
	TASK_INIT(&sc->sc_task, 0, athn_usb_task, sc);
	mbufq_init(&sc->sc_snd, ifqmaxlen);

	error = usbd_transfer_setup(uaa->device, &uaa->info.bIfaceIndex, usc->sc_xfer, athn_if_config,
								ATHN_N_XFER, usc, &sc->sc_mtx);
	if (error) {
		device_printf(sc->sc_dev,
					  "could not allocate USB transfers, err=%s\n",
					  usbd_errstr(error));
		mtx_destroy(&sc->sc_mtx);
		return error;
	}

	// TODO MichalP just for debug purposes can be removed
	ep = usc->sc_udev->endpoints;
	ep_end = usc->sc_udev->endpoints + usc->sc_udev->endpoints_max;
	for (; ep != ep_end; ep++) {
		uint8_t eaddr;

		eaddr = ep->edesc->bEndpointAddress;
		device_printf(sc->sc_dev, "%s: endpoint: addr %u, direction %s, endpoint: %p \n", __func__,
					 UE_GET_ADDR(eaddr), UE_GET_DIR(eaddr) == UE_DIR_OUT ?  "output" : "input", ep);
	}

	if (athn_usb_open_pipes(usc) != 0)
		return error;

	/* Allocate xfer for firmware commands. */
	if (athn_usb_alloc_tx_cmd(usc) != 0)
		return error;

	athn_usb_attachhook(self);

	return 0;
}

static int
athn_usb_detach(device_t self)
{
	struct athn_usb_softc *usc = device_get_softc(self);
	struct athn_softc *sc = &usc->sc_sc;

	if (usc->sc_athn_attached)
		athn_detach(sc);

	/* Wait for all async commands to complete. */
	athn_usb_wait_async(usc);

	usbd_transfer_unsetup(usc->sc_xfer, ATHN_N_XFER);

	/* Abort and close Tx/Rx pipes. */
	athn_usb_close_pipes(usc);

	/* Free Tx/Rx buffers. */
	athn_usb_free_tx_cmd(usc);
	athn_usb_free_tx_list(usc);
	athn_usb_free_rx_list(usc);

	athn_usb_unload_firmware();
	printf("athn_usb_detach called \n");
	return (0);
}

static struct ieee80211vap *
athn_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211vap *vap;
	struct athn_vap     *uvp;

	if (!TAILQ_EMPTY(&ic->ic_vaps))
		return (NULL);

	uvp =  malloc(sizeof(struct athn_vap), M_80211_VAP, M_WAITOK | M_ZERO);
	vap = &uvp->vap;

	if (ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags, bssid) != 0) {
		/* out of memory */
		free(uvp, M_80211_VAP);
		return (NULL);
	}

		/* override state transition machine */
	uvp->newstate = vap->iv_newstate;
	vap->iv_newstate = athn_usb_newstate;

	vap->iv_ampdu_density = IEEE80211_HTCAP_MPDUDENSITY_8;
	vap->iv_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_64K;

	ieee80211_ratectl_init(vap);

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status, mac);
	ic->ic_opmode = opmode;

	return (vap);
}

static void
athn_vap_delete(struct ieee80211vap *vap)
{
	struct athn_vap *uvp = ATHN_VAP(vap);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	free(uvp, M_80211_VAP);
}

void
athn_usb_attachhook(device_t self)
{
	struct athn_usb_softc *usc = device_get_softc(self);
	struct athn_softc *sc = &usc->sc_sc;
	struct athn_ops *ops = &sc->ops;
	uint32_t val = 104;
#ifdef notyet
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
#endif
	int s, i, error;

	/* Load firmware. */
	error = athn_usb_load_firmware(usc);
	if (error != 0) {
		printf("Could not load firmware\n");
		return;
	}

	// TODO MichalP: this can be used as a starting point for echo command or firmware command
//	ATHN_LOCK(usc);
//	device_printf(sc->sc_dev, " %s:val = %d\n", __func__, val);
//	val = athn_usb_read(sc, AR_WMI_CMD_ECHO);
//	device_printf(sc->sc_dev, "%s: returned val = %d\n", __func__, val);
//	val = *(uint32_t*)usc->obuf;
//	device_printf(sc->sc_dev, "%s: casted val = %d\n", __func__, val);
//	ATHN_UNLOCK(&usc->sc_sc);
//
//	return;

	/* Setup the host transport communication interface. */
	error = athn_usb_htc_setup(usc);
	if (error != 0)
		return;

	/* We're now ready to attach the bus agnostic driver. */
#if OpenBSD_IEEE80211_API
	// TODO: MichalP needs proper FreeBSD adaptation because this uses code that is
	//  stubbed and/or commented
	error = athn_attach(sc);
	if (error != 0) {
		return;
	}
#endif

	usc->sc_athn_attached = 1;
#if OpenBSD_IEEE80211_API
	/* Override some operations for USB. */
	ifp->if_ioctl = athn_usb_ioctl;
	ifp->if_start = athn_usb_start;
	// TODO no member named 'if_watchdog' in 'struct ifnet'
	// ifp->if_watchdog = athn_usb_watchdog;
	ic->ic_node_alloc = athn_usb_node_alloc;
	ic->ic_newauth = athn_usb_newauth;
	ic->ic_newassoc = athn_usb_newassoc;
#ifndef IEEE80211_STA_ONLY
	usc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = athn_usb_node_free;
#endif
	ic->ic_updateslot = athn_usb_updateslot;
	ic->ic_updateedca = athn_usb_updateedca;
	ic->ic_set_key = athn_usb_set_key;
	ic->ic_delete_key = athn_usb_delete_key;
	ic->ic_vap_create = athn_vap_create;
	ic->ic_vap_delete = athn_vap_delete;
	ic->ic_ampdu_tx_start = athn_usb_ampdu_tx_start;
	ic->ic_ampdu_tx_stop = athn_usb_ampdu_tx_stop;
	ic->ic_newstate = athn_usb_newstate;

	ic->ic_media.ifm_change_cb = athn_usb_media_change;

	ops->rx_enable = athn_usb_rx_enable;

	/* Reset HW key cache entries. */
	for (i = 0; i < sc->kc_entries; i++)
		athn_reset_key(sc, i);

	ops->enable_antenna_diversity(sc);
#endif

#ifdef ATHN_BT_COEXISTENCE
	/* Configure bluetooth coexistence for combo chips. */
	if (sc->flags & ATHN_FLAG_BTCOEX)
		athn_btcoex_init(sc);
#endif
	/* Configure LED. */
#if OpenBSD_ONLY
	athn_led_init(sc);
#endif
}

int
athn_usb_open_pipes(struct athn_usb_softc *usc)
{
	struct athn_softc *sc = &usc->sc_sc;
	usb_error_t error;
#if OpenBSD_ONLY
	/* Init host async commands ring. */
	usc->cmdq.cur = usc->cmdq.next = usc->cmdq.queued = 0;
#endif
	/* Allocate Tx/Rx buffers. */
	if((error = athn_usb_alloc_rx_list(usc)) != 0) {
		device_printf(sc->sc_dev, "%s: could not allocate Tx xfers\n",
					  __func__);
		goto fail;
	}
	if ((error = athn_usb_alloc_tx_list(usc)) != 0) {
		device_printf(sc->sc_dev, "%s: could not allocate Tx xfers\n",
					  __func__);
		goto fail;
	}
#if OpenBSD_ONLY
	/* Steal one buffer for beacons. */
	usc->tx_bcn = TAILQ_FIRST(&usc->tx_free_list);
	TAILQ_REMOVE(&usc->tx_free_list, usc->tx_bcn, next);
#endif

	ATHN_LOCK(sc);
	usbd_transfer_start(usc->sc_xfer[ATHN_BULK_RX]);
	usbd_transfer_start(usc->sc_xfer[ATHN_BULK_IRQ]);
	ATHN_UNLOCK(sc);
	return 0;

 fail:
	athn_usb_close_pipes(usc);
	return (error);
}

void
athn_usb_close_pipes(struct athn_usb_softc *usc)
{
	struct athn_softc *sc = &usc->sc_sc;

	ATHN_LOCK(sc);
	athn_usb_free_rx_list(usc);
	athn_usb_free_tx_list(usc);
	athn_usb_free_tx_cmd(usc);
	ATHN_UNLOCK(sc);
}

static int
athn_alloc_list(struct athn_usb_softc *usc, struct athn_usb_data data[],
				int ndata, int maxsz)
{
	struct athn_softc *sc = &usc->sc_sc;
	int i, error;

	for (i = 0; i < ndata; i++) {
		struct athn_usb_data *dp = &data[i];
		dp->usc = usc;
		// TODO MichalP might be needed not sure
//		dp->m = NULL;
		dp->buf = malloc(maxsz, M_USBDEV, M_NOWAIT | M_ZERO);
		if (dp->buf == NULL) {
			device_printf(sc->sc_dev,
						  "could not allocate buffer\n");
			error = ENOMEM;
			goto fail;
		}

		// TODO MichalP need IEEE80211 api implemented
		//dp->ni = NULL;
	}

	return (0);
fail:
	athn_free_list(usc, data, ndata);
	return (error);
}

static int
athn_free_list(struct athn_usb_softc *usc, struct athn_usb_data data[], int ndata)
{
	int i;

	for (i = 0; i < ndata; i++) {
		struct athn_usb_data *dp = &data[i];

		if (dp->buf != NULL) {
			free(dp->buf, M_USBDEV);
			dp->buf = NULL;
		}
// TODO MichalP need IEEE80211 api implemented
//		if (dp->ni != NULL) {
//			ieee80211_free_node(dp->ni);
//			dp->ni = NULL;
//		}
	}
	return 0;
}

int
athn_usb_alloc_rx_list(struct athn_usb_softc *usc)
{
	int i, error;

	error = athn_alloc_list(usc, usc->rx_data, ATHN_USB_RX_LIST_COUNT,
							ATHN_USB_RXBUFSZ);

	STAILQ_INIT(&usc->sc_rx_active);
	STAILQ_INIT(&usc->sc_rx_inactive);

	for (i = 0; i < ATHN_USB_RX_LIST_COUNT; i++)
		STAILQ_INSERT_HEAD(&usc->sc_rx_inactive, &usc->rx_data[i], next);

	return (error);
}

void
athn_usb_free_rx_list(struct athn_usb_softc *usc)
{
	int i;

	STAILQ_INIT(&usc->sc_rx_inactive);
	STAILQ_INIT(&usc->sc_rx_active);

	athn_free_list(usc, usc->rx_data, ATHN_USB_RX_LIST_COUNT);
}

int
athn_usb_alloc_tx_list(struct athn_usb_softc *usc)
{
	int i, error;

	error = athn_alloc_list(usc, usc->tx_data, ATHN_USB_TX_LIST_COUNT,
							ATHN_USB_TXBUFSZ);
	if (error != 0)
		return (error);

	STAILQ_INIT(&usc->sc_tx_inactive);

	for (i = 0; i != ATHN_N_XFER; i++) {
		STAILQ_INIT(&usc->sc_tx_active[i]);
		STAILQ_INIT(&usc->sc_tx_pending[i]);
	}

	for (i = 0; i < ATHN_USB_TX_LIST_COUNT; i++) {
		STAILQ_INSERT_HEAD(&usc->sc_tx_inactive, &usc->tx_data[i], next);
	}

	return (0);
}

void
athn_usb_free_tx_list(struct athn_usb_softc *usc)
{
	int i;

	STAILQ_INIT(&usc->sc_tx_inactive);

	for (i = 0; i != ATHN_N_XFER; i++) {
		STAILQ_INIT(&usc->sc_tx_active[i]);
		STAILQ_INIT(&usc->sc_tx_pending[i]);
	}

	athn_free_list(usc, usc->tx_data, ATHN_USB_TX_LIST_COUNT);
}

int
athn_usb_alloc_tx_cmd(struct athn_usb_softc *usc)
{
	struct athn_usb_data *data;
	int i, error = 0;

	error = athn_alloc_list(usc, usc->tx_cmd, ATHN_USB_HOST_CMD_RING_COUNT,
							ATHN_USB_TXCMDSZ);
	if (error != 0)
		return (error);

	STAILQ_INIT(&usc->sc_cmd_active);
	STAILQ_INIT(&usc->sc_cmd_inactive);
	STAILQ_INIT(&usc->sc_cmd_pending);
	STAILQ_INIT(&usc->sc_cmd_waiting);

	for (i = 0; i < ATHN_USB_HOST_CMD_RING_COUNT; i++)
		STAILQ_INSERT_HEAD(&usc->sc_cmd_inactive, &usc->tx_cmd[i], next);

	return (0);
}

void
athn_usb_free_tx_cmd(struct athn_usb_softc *usc)
{
	STAILQ_INIT(&usc->sc_cmd_active);
	STAILQ_INIT(&usc->sc_cmd_inactive);
	STAILQ_INIT(&usc->sc_cmd_pending);
	STAILQ_INIT(&usc->sc_cmd_waiting);

	athn_free_list(usc, usc->tx_cmd, ATHN_USB_HOST_CMD_RING_COUNT);
}

char *state2Str(int state) {
	switch (state) {
	case USB_ST_SETUP:
		return "USB_ST_SETUP";
	case USB_ST_TRANSFERRED:
		return "USB_ST_TRANSFERRED";
	case USB_ST_ERROR:
		return "USB_ST_ERROR";
	default:
		return "state2Str UNKNOWN";
	}
}


char * epType_str(uint8_t bmAttributes) {
	switch(bmAttributes) {
	case UE_CONTROL:
		return "UE_CONTROL";
	case UE_ISOCHRONOUS:
		return "UE_ISOCHRONOUS";
	case UE_BULK:
		return "UE_BULK";
	case UE_INTERRUPT:
		return "UE_INTERRUPT";
	default:
		return "UNKNOWN";
	}
}

static void
athn_if_intr_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct athn_usb_softc *usc = usbd_xfer_softc(xfer);
	struct ar_htc_frame_hdr *htc;
	struct ar_htc_msg_hdr *msg;
	uint16_t msg_id;
	uint8_t *buf;

	int actlen, sumlen;
	int state = USB_GET_STATE(xfer);
	char *state_str = state2Str(state);

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	printf("TTTT: INTR RX Xfer callback: error = %s, state = %s, actlen = %d, "
		   "sumlen = %d, endpoint = 0x%lx\n",
		   usbd_errstr(error), state_str, actlen, sumlen, (long)xfer->endpoint);

	switch(state) {
	case USB_ST_TRANSFERRED:
	{
		printf("TTTT: INTR RX Xfer state USB_ST_TRANSFERRED\n");

		buf = usbd_xfer_get_frame_buffer(xfer, 0);
		if (actlen >= 4 && *(uint32_t *)buf == htobe32(0x00c60000)) {
			buf += 4;
			actlen -= 4;
		}
		if ((actlen < sizeof(*htc)))
			return;

		htc = (struct ar_htc_frame_hdr *)buf;
		// Skip HTC header.
		buf += sizeof(*htc);
		actlen -= sizeof(*htc);

		if (htc->endpoint_id != 0) {
			printf("TTTT: htc->endpoint_id != 0\n");
			if (htc->endpoint_id != usc->ep_ctrl) {
				printf("TTTT: htc->endpoint_id != usc->ep_ctrl\n");
				return;
			}

			/// Remove trailer if present.
			if (htc->flags & AR_HTC_FLAG_TRAILER) {
				printf("TTTT: htc->flags & AR_HTC_FLAG_TRAILER\n");
				if (actlen < htc->control[0]) {
					printf("TTTT: actlen < htc->control[0]\n");
					return;
				}

				actlen -= htc->control[0];
			}
			 athn_usb_rx_wmi_ctrl(usc, buf, actlen);
			return;
		}

		// Endpoint 0 carries HTC messages.
		if (actlen < sizeof(*msg)) {
			printf("TTTT: htc->flags & AR_HTC_FLAG_TRAILER\n");
			return;
		}

		msg = (struct ar_htc_msg_hdr *)buf;
		msg_id = betoh16(msg->msg_id);
		printf("TTTT: Rx HTC msg_id %d, wait_msg_id %d \n", msg_id, usc->wait_msg_id);
		switch (msg_id) {
		case AR_HTC_MSG_READY:
			if (usc->wait_msg_id != msg_id)
				break;
			usc->wait_msg_id = 0;
			wakeup(&usc->wait_msg_id);
			break;
		case AR_HTC_MSG_CONN_SVC_RSP:
			if (usc->wait_msg_id != msg_id)
				break;
			if (usc->msg_conn_svc_rsp != NULL) {
				memcpy(usc->msg_conn_svc_rsp, &msg[1],
					   sizeof(struct ar_htc_msg_conn_svc_rsp));
			}
			usc->wait_msg_id = 0;
			wakeup(&usc->wait_msg_id);
			break;
		case AR_HTC_MSG_CONF_PIPE_RSP:
			if (usc->wait_msg_id != msg_id)
				break;
			usc->wait_msg_id = 0;
			wakeup(&usc->wait_msg_id);
			break;
		default:
			printf("HTC message %d ignored\n", msg_id);
			break;
		}
	}
	case USB_ST_SETUP:
		printf("USB_ST_SETUP called\n");
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;
	default: /* Error */
		break;
	}
	return;
}

static void
athn_if_intr_tx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct athn_usb_data *cmd;
	struct athn_usb_softc *usc = usbd_xfer_softc(xfer);
	struct athn_softc *sc = &usc->sc_sc;

	int actlen;
	int state = USB_GET_STATE(xfer);
	char *state_str = state2Str(state);

	ATHN_ASSERT_LOCKED(sc);

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	// MichalP: Debuginfo
	printf("TTTT: INTR TX Xfer callback: error = %s, state = %s, actlen = %d, endpoint = 0x%lx\n",
		   usbd_errstr(error), state_str, actlen, (long)xfer->endpoint);

	switch(state) {
	case USB_ST_TRANSFERRED:
		cmd = STAILQ_FIRST(&usc->sc_cmd_active);
		device_printf(sc->sc_dev, "%s: continue with USB_ST_TRANSFERRED cmd: %p\n", __func__, cmd);
		if (cmd == NULL)
			goto tr_setup;
		device_printf(sc->sc_dev, "%s: transfer done cmd: %p\n", __func__, cmd);
		STAILQ_REMOVE_HEAD(&usc->sc_cmd_active, next);
		// MichalP TODO: this still needs some thinking maybe separate function
		// different object that the thread sleeps on (cmd?)
		if (usc->wait_msg_id == AR_HTC_MSG_CONN_SVC_RSP) {
			device_printf(sc->sc_dev, "%s: we are waiting for a response cmd: %p\n", __func__, cmd);
			STAILQ_INSERT_TAIL(&usc->sc_cmd_waiting, cmd, next);
		} else {
			device_printf(sc->sc_dev, "%s: we DONT wait for response cmd: %p\n", __func__, cmd);
			wakeup(&usc->wait_msg_id);
			STAILQ_INSERT_TAIL(&usc->sc_cmd_inactive, cmd, next);
		}
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		cmd = STAILQ_FIRST(&usc->sc_cmd_pending);
		if (cmd == NULL) {
			device_printf(sc->sc_dev, "%s: empty pending queue cmd: %p\n", __func__, cmd);
			return;
		}
		device_printf(sc->sc_dev, "%s: continue with USB_ST_SETUP cmd: %p\n", __func__, cmd);
		STAILQ_REMOVE_HEAD(&usc->sc_cmd_pending, next);
		STAILQ_INSERT_TAIL(&usc->sc_cmd_active, cmd, next);
		usbd_xfer_set_frame_data(xfer, 0, cmd->buf, cmd->buflen);
		device_printf(sc->sc_dev, "%s: submitting transfer %p; buf=%p, buflen=%d\n",
					  __func__, cmd, cmd->buf, cmd->buflen);
		usbd_transfer_submit(xfer);
		break;
	default:
		cmd = STAILQ_FIRST(&usc->sc_cmd_active);
		device_printf(sc->sc_dev, "%s: continue with default %p\n", __func__, usc);
		if (cmd != NULL) {
			device_printf(sc->sc_dev, "%s: cmd not NULL %p\n", __func__, usc);
			STAILQ_REMOVE_HEAD(&usc->sc_cmd_active, next);
//			if (cmd->odata) {
//				STAILQ_INSERT_TAIL(&usc->sc_cmd_waiting, cmd, next_cmd);
//			} else {
//				wakeup(cmd);
//				otus_free_txcmd(sc, cmd);
//			}
		}
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
athn_if_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct athn_usb_softc *usc = usbd_xfer_softc(xfer);
	struct athn_softc *sc = &usc->sc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct athn_usb_data *data;

	int actlen;
	int state = USB_GET_STATE(xfer);
	char *state_str = state2Str(state);

	ATHN_ASSERT_LOCKED(sc);

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	// MichalP: Debuginfo
	printf("TTTT: BULK RX Xfer callback: error = %s, state = %s, actlen = %d, endpoint = 0x%lx\n",
		   usbd_errstr(error), state_str, actlen, (long)xfer->endpoint);

	switch(state) {
	case USB_ST_TRANSFERRED:
		device_printf(sc->sc_dev, "%s: USB_ST_TRANSFERRED called \n", __func__);
		data = STAILQ_FIRST(&usc->sc_rx_active);
		if (data == NULL)
			goto tr_setup;
		STAILQ_REMOVE_HEAD(&usc->sc_rx_active, next);
		athn_usb_rxeof(xfer, data);
		STAILQ_INSERT_TAIL(&usc->sc_rx_inactive, data, next);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
		device_printf(sc->sc_dev, "%s: USB_ST_SETUP called \n", __func__);
	tr_setup:
		device_printf(sc->sc_dev, "%s: USB_ST_SETUP after goto called \n", __func__);
		data = STAILQ_FIRST(&usc->sc_rx_inactive);
		if (data == NULL) {
			//KASSERT(m == NULL, ("mbuf isn't NULL"));
			return;
		}
		STAILQ_REMOVE_HEAD(&usc->sc_rx_inactive, next);
		STAILQ_INSERT_TAIL(&usc->sc_rx_active, data, next);
		usbd_xfer_set_frame_data(xfer, 0, data->buf,
								 usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;
	default:
		device_printf(sc->sc_dev, "%s: default called \n", __func__);
		/* needs it to the inactive queue due to a error. */
		data = STAILQ_FIRST(&usc->sc_rx_active);
		if (data != NULL) {
			STAILQ_REMOVE_HEAD(&usc->sc_rx_active, next);
			STAILQ_INSERT_TAIL(&usc->sc_rx_inactive, data, next);
		}
		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			counter_u64_add(ic->ic_ierrors, 1);
			goto tr_setup;
		}
		break;
	}
}

static void
athn_if_bulk_tx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	uint8_t which = ATHN_BULK_TX;
	struct athn_usb_softc *usc = usbd_xfer_softc(xfer);
	struct athn_softc *sc = &usc->sc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct athn_usb_data *data;

	ATHN_ASSERT_LOCKED(sc);

	int actlen;
	int state = USB_GET_STATE(xfer);
	char *state_str = state2Str(state);

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	printf("TTTT: BULK TX Xfer callback: error = %s, state = %s, actlen = %d, endpoint = 0x%lx\n",
		   usbd_errstr(error), state_str, actlen, (long)xfer->endpoint);

	switch(state) {
	case USB_ST_TRANSFERRED:
		data = STAILQ_FIRST(&usc->sc_tx_active[which]);
		if (data == NULL)
			goto tr_setup;
		device_printf(sc->sc_dev, "%s: transfer done %p\n", __func__, data);
		STAILQ_REMOVE_HEAD(&usc->sc_tx_active[which], next);
		athn_usb_txeof(xfer, data);
		STAILQ_INSERT_TAIL(&usc->sc_tx_inactive, data, next);
	case USB_ST_SETUP:
	tr_setup:
		data = STAILQ_FIRST(&usc->sc_tx_pending[which]);
		if (data == NULL) {
			device_printf(sc->sc_dev, "%s: empty pending queue sc %p\n", __func__, sc);
			usc->sc_tx_n_active = 0;
		}
		STAILQ_REMOVE_HEAD(&usc->sc_tx_pending[which], next);
		STAILQ_INSERT_TAIL(&usc->sc_tx_active[which], data, next);
		usbd_xfer_set_frame_data(xfer, 0, data->buf, data->buflen);
		device_printf(sc->sc_dev, "%s: submitting transfer %p\n", __func__, data);
		usbd_transfer_submit(xfer);
		usc->sc_tx_n_active++;
		break;
	default:
		data = STAILQ_FIRST(&usc->sc_tx_active[which]);
		if (data != NULL) {
			STAILQ_REMOVE_HEAD(&usc->sc_tx_active[which], next);
			athn_usb_txeof(xfer, data);
			STAILQ_INSERT_TAIL(&usc->sc_tx_inactive, data, next);
		}
		counter_u64_add(ic->ic_oerrors, 1);

		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}

	taskqueue_enqueue(taskqueue_thread, &sc->sc_task);
}

void
athn_usb_task(void *arg, int pending)
{
#ifdef otus
	struct athn_usb_softc *usc = arg;
	struct athn_usb_host_cmd_ring *ring = &usc->cmdq;
	struct athn_usb_host_cmd *cmd;
	int s;

	// lock

	/* Process host commands. */
	while (ring->next != ring->cur) {
		cmd = &ring->cmd[ring->next];
		/* Invoke callback. */
		cmd->cb(usc, cmd->data);
		ring->queued--;
		ring->next = (ring->next + 1) % ATHN_USB_HOST_CMD_RING_COUNT;
	}
#endif
}

void
athn_usb_do_async(struct athn_usb_softc *usc,
    void (*cb)(struct athn_usb_softc *, void *), void *arg, int len)
{
#if OpenBSD_ONLY
	struct athn_usb_host_cmd_ring *ring = &usc->cmdq;
	struct athn_usb_host_cmd *cmd;
	int s;

	if (ring->queued == ATHN_USB_HOST_CMD_RING_COUNT) {
		printf("%s: host cmd queue overrun\n", device_get_name(usc->usb_dev));
		return;	/* XXX */
	}

	s = splusb();
	cmd = &ring->cmd[ring->cur];
	cmd->cb = cb;
	KASSERT(len <= sizeof(cmd->data), "athn_usb_do_async");
	memcpy(cmd->data, arg, len);
	ring->cur = (ring->cur + 1) % ATHN_USB_HOST_CMD_RING_COUNT;

	/* If there is no pending command already, schedule a task. */
	if (++ring->queued == 1)
		usb_add_task(usc->sc_udev, &usc->sc_task);
	splx(s);
#endif
}

void
athn_usb_wait_async(struct athn_usb_softc *usc)
{
	/* Wait for all queued asynchronous commands to complete. */
#if OpenBSD_ONLY
	usb_wait_task(usc->sc_udev, &usc->sc_task);
#endif
}

void
athn_usb_verify_fw(struct athn_usb_softc *usc)
{
	struct athn_softc *sc = &usc->sc_sc;
	struct athn_usb_data *data;

	ATHN_LOCK(sc);

	data = STAILQ_FIRST(&usc->sc_cmd_inactive);
	if (data == NULL) {
		device_printf(sc->sc_dev, "%s: no tx cmd buffers\n",
					  __func__);
		return;
	}
	STAILQ_REMOVE_HEAD(&usc->sc_cmd_inactive, next);
}

int
athn_usb_htc_msg(struct athn_usb_softc *usc, uint16_t msg_id, void *buf,
    int len)
{
	struct athn_softc *sc = &usc->sc_sc;
	struct athn_usb_data *cmd;
	struct ar_htc_frame_hdr *htc;
	struct ar_htc_msg_hdr *msg;
	int error = 0;

	device_printf(sc->sc_dev, "%s: message id: %d\n",  __func__, msg_id);

	ATHN_ASSERT_LOCKED(sc);

	cmd = STAILQ_FIRST(&usc->sc_cmd_inactive);
	if (cmd == NULL) {
		device_printf(sc->sc_dev, "%s: no tx cmd buffers\n",
					  __func__);
		return -1;
	}
	STAILQ_REMOVE_HEAD(&usc->sc_cmd_inactive, next);

	htc = (struct ar_htc_frame_hdr *)cmd->buf;
	memset(htc, 0, sizeof(*htc));
	htc->endpoint_id = 0;
	htc->payload_len = htobe16(sizeof(*msg) + len);

	msg = (struct ar_htc_msg_hdr *)&htc[1];
	msg->msg_id = htobe16(msg_id);

	memcpy(&msg[1], buf, len);

	cmd->buflen = sizeof(*htc) + sizeof(*msg) + len;
	device_printf(sc->sc_dev, "%s: prepare transfer %p; buf=%p, buflen=%d\n",
				  __func__, cmd, cmd->buf, cmd->buflen);

	STAILQ_INSERT_TAIL(&usc->sc_cmd_pending, cmd, next);
	usbd_transfer_start(usc->sc_xfer[ATHN_BULK_CMD]);

	return error;
}

int
athn_usb_htc_setup(struct athn_usb_softc *usc)
{
	struct ar_htc_msg_config_pipe cfg;
	int s, error;

	/*
	 * Connect WMI services to USB pipes.
	 */
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_CONTROL,
	    AR_PIPE_TX_INTR, AR_PIPE_RX_INTR, &usc->ep_ctrl);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_BEACON,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_bcn);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_CAB,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_cab);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_UAPSD,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_uapsd);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_MGMT,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_mgmt);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_BE,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_data[ATHN_QID_AC_BE]);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_BK,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_data[ATHN_QID_AC_BK]);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_VI,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_data[ATHN_QID_AC_VI]);
	if (error != 0)
		return (error);
	error = athn_usb_htc_connect_svc(usc, AR_SVC_WMI_DATA_VO,
	    AR_PIPE_TX_DATA, AR_PIPE_RX_DATA, &usc->ep_data[ATHN_QID_AC_VO]);
	if (error != 0)
		return (error);

	/* Set credits for WLAN Tx pipe. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.pipe_id = UE_GET_ADDR(AR_PIPE_TX_DATA);
	cfg.credits = (usc->flags & ATHN_USB_FLAG_AR7010) ? 45 : 33;

	ATHN_LOCK(&usc->sc_sc);

	usc->wait_msg_id = AR_HTC_MSG_CONF_PIPE_RSP;
	error = athn_usb_htc_msg(usc, AR_HTC_MSG_CONF_PIPE, &cfg, sizeof(cfg));
	if (error == 0 && usc->wait_msg_id != 0)
		error = msleep(&usc->wait_msg_id, &usc->sc_sc.sc_mtx, PCATCH, "athnhtc",
					   hz);
	usc->wait_msg_id = 0;

	ATHN_UNLOCK(&usc->sc_sc);

	if (error != 0) {
		printf("%s: could not configure pipe\n",
		    device_get_name(usc->usb_dev));
		return (error);
	}

	ATHN_LOCK(&usc->sc_sc);

	error = athn_usb_htc_msg(usc, AR_HTC_MSG_SETUP_COMPLETE, NULL, 0);
	if (error != 0) {
		printf("%s: could not complete setup\n",
		    device_get_name(usc->usb_dev));
		return (error);
	}

	ATHN_UNLOCK(&usc->sc_sc);

	return (0);
}

int
athn_usb_htc_connect_svc(struct athn_usb_softc *usc, uint16_t svc_id,
    uint8_t ul_pipe, uint8_t dl_pipe, uint8_t *endpoint_id)
{
	struct ar_htc_msg_conn_svc msg;
	struct ar_htc_msg_conn_svc_rsp rsp;
	int s, error;

	struct athn_softc *sc = &usc->sc_sc;
	device_printf(sc->sc_dev, "%s: configuring svc_id %d \n", __func__, svc_id);

	memset(&msg, 0, sizeof(msg));
	msg.svc_id = htobe16(svc_id);
	msg.dl_pipeid = UE_GET_ADDR(dl_pipe);
	msg.ul_pipeid = UE_GET_ADDR(ul_pipe);
	usc->msg_conn_svc_rsp = &rsp;

	ATHN_LOCK(&usc->sc_sc);

	usc->wait_msg_id = AR_HTC_MSG_CONN_SVC_RSP;
	error = athn_usb_htc_msg(usc, AR_HTC_MSG_CONN_SVC, &msg, sizeof(msg));
	/* Wait at most 1 second for response. */
	if (error == 0 && usc->wait_msg_id != 0)
		error = msleep(&usc->wait_msg_id, &usc->sc_sc.sc_mtx, PCATCH, "athnhtc", hz * 2);
	usc->wait_msg_id = 0;

	ATHN_UNLOCK(&usc->sc_sc);

	if (error != 0) {
		device_printf(sc->sc_dev, "%s: error waiting for service %d connection "
								  "error: %d \n", __func__, htobe16(svc_id), error);
		return (error);
	}
	if (rsp.status != AR_HTC_SVC_SUCCESS) {
		device_printf(sc->sc_dev, "%s: service %d connection failed, "
								  "error: %d\n", __func__, htobe16(svc_id), rsp.status);
		return (EIO);
	}

	device_printf(sc->sc_dev, "%s: service %d successfully connected to endpoint %d\n",
				  __func__, svc_id, rsp.endpoint_id);

	/* Return endpoint id. */
	*endpoint_id = rsp.endpoint_id;
	return (0);
}

int
athn_usb_wmi_xcmd(struct athn_usb_softc *usc, uint16_t cmd_id, void *ibuf,
    int ilen, void *obuf)
{
	struct athn_softc *sc = &usc->sc_sc;
	struct athn_usb_data *data;
	struct ar_htc_frame_hdr *htc;
	struct ar_wmi_cmd_hdr *wmi;
	int xferlen, error;

	device_printf(sc->sc_dev, "%s: called \n", __func__);

	ATHN_ASSERT_LOCKED(sc);

	data = STAILQ_FIRST(&usc->sc_cmd_inactive);
	if (data == NULL) {
		device_printf(sc->sc_dev, "%s: no tx cmd buffers\n",
					  __func__);
		return -1;
	}
	STAILQ_REMOVE_HEAD(&usc->sc_cmd_inactive, next);

	while (usc->wait_cmd_id) {
		/*
		 * The previous USB transfer is not done yet. We can't use
		 * data->xfer until it is done or we'll cause major confusion
		 * in the USB stack.
		 */
		msleep(&usc->wait_msg_id, &usc->sc_sc.sc_mtx, PCATCH, "athnwmx", hz);
	}
	xferlen = sizeof(*htc) + sizeof(*wmi) + ilen;
	data->buflen = xferlen;

	htc = (struct ar_htc_frame_hdr *)data->buf;
	memset(htc, 0, sizeof(*htc));
	htc->endpoint_id = usc->ep_ctrl;
	htc->payload_len = sizeof(*wmi) + ilen;

	wmi = (struct ar_wmi_cmd_hdr *)&htc[1];
	wmi->cmd_id = htobe16(cmd_id);
	usc->wmi_seq_no++;
	wmi->seq_no = usc->wmi_seq_no;

	memcpy(&wmi[1], ibuf, ilen);

	usc->wait_cmd_id = htobe16(cmd_id);
	STAILQ_INSERT_TAIL(&usc->sc_cmd_pending, data, next);
	usbd_transfer_start(usc->sc_xfer[ATHN_BULK_CMD]);

	/*
	 * Wait for WMI command complete interrupt. In case it does not fire
	 * wait until the USB transfer times out to avoid racing the transfer.
	 */
	error = msleep(&usc->wait_cmd_id, &usc->sc_sc.sc_mtx, PCATCH, "athnwmi", hz);
	if (error == EWOULDBLOCK) {
		printf("%s: firmware command 0x%x timed out\n",
			device_get_name(usc->usb_dev), cmd_id);
		error = ETIMEDOUT;
	}

	device_printf(sc->sc_dev, "%s: aftersleep\n", __func__);
	obuf = usc->obuf;

	/*
	 * Both the WMI command and transfer are done or have timed out.
	 * Allow other threads to enter this function and use data->xfer.
	 */
	usc->wait_cmd_id = 0;
	wakeup(&usc->wait_cmd_id);

	return (error);
}

int
athn_usb_read_rom(struct athn_softc *sc)
{
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	uint32_t addrs[8], vals[8], addr;
	uint16_t *eep;
	int i, j, error;

	/* Read EEPROM by blocks of 16 bytes. */
	eep = sc->eep;
	addr = AR_EEPROM_OFFSET(sc->eep_base);
	for (i = 0; i < sc->eep_size / 16; i++) {
		for (j = 0; j < 8; j++, addr += 4)
			addrs[j] = htobe32(addr);
		error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_REG_READ,
		    addrs, sizeof(addrs), vals);
		if (error != 0)
			break;
		for (j = 0; j < 8; j++)
			*eep++ = betoh32(vals[j]);
	}
	return (error);
}

uint32_t
athn_usb_read(struct athn_softc *sc, uint32_t addr)
{
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	uint32_t val;
	int error;

	device_printf(sc->sc_dev, "%s: called \n", __func__);

	/* Flush pending writes for strict consistency. */
	athn_usb_write_barrier(sc);

	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_REG_READ,
	    &addr, sizeof(addr), &val);
	if (error != 0)
		device_printf(sc->sc_dev,
					  "%s: error \n",
					  __func__);
	return (val);
}

void
athn_usb_write(struct athn_softc *sc, uint32_t addr, uint32_t val)
{
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;

	usc->wbuf[usc->wcount].addr = htobe32(addr);
	usc->wbuf[usc->wcount].val  = htobe32(val);
	if (++usc->wcount == AR_MAX_WRITE_COUNT)
		athn_usb_write_barrier(sc);
}

void
athn_usb_write_barrier(struct athn_softc *sc)
{
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;

	if (usc->wcount == 0)
		return;	/* Nothing to write. */

	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_REG_WRITE,
	    usc->wbuf, usc->wcount * sizeof(usc->wbuf[0]), NULL);
	usc->wcount = 0;	/* Always flush buffer. */
}

int
athn_usb_media_change(struct ifnet *ifp)
{
	struct athn_usb_softc *usc = (struct athn_usb_softc *)ifp->if_softc;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return (error);

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING)) {
		athn_usb_stop(ifp);
		error = athn_usb_init(ifp);
	}

	return (error);
}

void
athn_usb_next_scan(void *arg, int pending)
{
	struct athn_usb_softc *usc = arg;
	struct athn_softc *sc = &usc->sc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

#if OpenBSD_IEEE80211_API
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&ic->ic_if);
#endif
}

// uncomment after investigation if it is needed to be async
// int
// athn_usb_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate,
//     int arg)
// {
// 	struct ieee80211com *ic = vap->iv_ic;
// 	struct athn_usb_softc *usc = ic->ic_softc;
// 	struct athn_vap *uvp = ATHN_VAP(vap);
// 	struct athn_usb_cmd_newstate cmd;

// 	/* Do it in a process context. */
// 	cmd.state = nstate;
// 	cmd.arg = arg;
// 	athn_usb_do_async(usc, athn_usb_newstate_cb, &cmd, sizeof(cmd));
// 	return (0);
// }

int
athn_usb_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate,
    int arg)
{
	struct athn_vap *uvp = ATHN_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct athn_usb_softc *usc = ic->ic_softc;
	struct athn_softc *sc = &usc->sc_sc;
	struct ieee80211_node *ni = ieee80211_ref_node(vap->iv_bss);
	enum ieee80211_state ostate;
	uint32_t reg, imask;
	int s, error;
#if OpenBSD_ONLY
	timeout_del(&sc->calib_to);
#endif
	IEEE80211_UNLOCK(ic);
	ATHN_LOCK(sc);
	ostate = vap->iv_state;

	if (ostate == IEEE80211_S_RUN && ic->ic_opmode == IEEE80211_M_STA) {
		athn_usb_remove_node(usc, ni);
		reg = AR_READ(sc, AR_RX_FILTER);
		reg = (reg & ~AR_RX_FILTER_MYBEACON) |
		    AR_RX_FILTER_BEACON;
		AR_WRITE(sc, AR_RX_FILTER, reg);
		AR_WRITE_BARRIER(sc);
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		athn_set_led(sc, 0);
		break;
	case IEEE80211_S_SCAN:
		/* Make the LED blink while scanning. */
		athn_set_led(sc, !sc->led_state);
		error = athn_usb_switch_chan(sc, ni->ni_chan, NULL);
		if (error)
			printf("%s: could not switch to channel %d\n",
			    device_get_name(usc->usb_dev), 0);
			    ieee80211_chan2ieee(ic, ni->ni_chan);
		// uncomment after investigation if it is needed to be async
		// if (!usbd_is_dying(usc->sc_udev))
		// 	timeout_add_msec(&sc->scan_to, 200);
		break;
	case IEEE80211_S_AUTH:
		athn_set_led(sc, 0);
		error = athn_usb_switch_chan(sc, ni->ni_chan, NULL);
		if (error)
			printf("%s: could not switch to channel %d\n",
			    device_get_name(usc->usb_dev), 0);
			    ieee80211_chan2ieee(ic, ni->ni_chan);
		break;
	case IEEE80211_S_ASSOC:
		break;
	case IEEE80211_S_RUN:
		athn_set_led(sc, 1);

		if (ic->ic_opmode == IEEE80211_M_MONITOR)
			break;

		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* Create node entry for our BSS */
			error = athn_usb_create_node(usc, ni);
			if (error)
				printf("%s: could not update firmware station "
				    "table\n", device_get_name(usc->usb_dev));
		}
		athn_set_bss(sc, ni);
		athn_usb_wmi_cmd(usc, AR_WMI_CMD_DISABLE_INTR);
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			athn_usb_switch_chan(sc, ni->ni_chan, NULL);
			athn_set_hostap_timers(sc);
			/* Enable software beacon alert interrupts. */
			imask = htobe32(AR_IMR_SWBA);
		} else
#endif
		{
			athn_set_sta_timers(sc);
			/* Enable beacon miss interrupts. */
			imask = htobe32(AR_IMR_BMISS);

			/* Stop receiving beacons from other BSS. */
			reg = AR_READ(sc, AR_RX_FILTER);
			reg = (reg & ~AR_RX_FILTER_BEACON) |
			    AR_RX_FILTER_MYBEACON;
			AR_WRITE(sc, AR_RX_FILTER, reg);
			AR_WRITE_BARRIER(sc);
		}
		athn_usb_wmi_xcmd(usc, AR_WMI_CMD_ENABLE_INTR,
		    &imask, sizeof(imask), NULL);
		break;
	default:
		break;
	}
#if OpenBSD_IEEE80211_API
	return sc->sc_newstate(ic, state, arg);
	splx(s);
	#endif
	ATHN_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return (uvp->newstate(vap, nstate, arg));
}

void
athn_usb_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni,
    int isnew)
{
#ifndef IEEE80211_STA_ONLY
	struct athn_usb_softc *usc = ic->ic_softc;
#if OpenBSD_IEEE80211_API
	if (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	    ic->ic_state != IEEE80211_S_RUN)
		return;
#endif
	/* Update the node's supported rates in a process context. */
	ieee80211_ref_node(ni);
	athn_usb_do_async(usc, athn_usb_newassoc_cb, &ni, sizeof(ni));
#endif
}

#ifndef IEEE80211_STA_ONLY
void
athn_usb_newassoc_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
	struct ieee80211_node *ni = *(void **)arg;
	struct athn_node *an = (struct athn_node *)ni;
	int s;
#if OpenBSD_IEEE80211_API
	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	s = splnet();
	/* NB: Node may have left before we got scheduled. */
	if (an->sta_index != 0)
		(void)athn_usb_node_set_rates(usc, ni);
	ieee80211_release_node(ic, ni);
	splx(s);
#endif
}
#endif

struct ieee80211_node *
athn_usb_node_alloc(struct ieee80211com *ic)
{
	struct athn_node *an;

	an = malloc(sizeof(struct athn_node), M_USBDEV, M_NOWAIT | M_ZERO);
	return (struct ieee80211_node *)an;
}


#ifndef IEEE80211_STA_ONLY
void
athn_usb_count_active_sta(void *arg, struct ieee80211_node *ni)
{
	int *nsta = arg;
	struct athn_node *an = (struct athn_node *)ni;

	if (an->sta_index == 0)
		return;
#if OpenBSD_IEEE80211_API
	if ((ni->ni_state == IEEE80211_STA_AUTH ||
	    ni->ni_state == IEEE80211_STA_ASSOC) &&
	    ni->ni_inact < IEEE80211_INACT_MAX)
		(*nsta)++;
#endif
}

struct athn_usb_newauth_cb_arg {
	struct ieee80211_node *ni;
	uint16_t seq;
};

void
athn_usb_newauth_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
	struct athn_usb_newauth_cb_arg *a = arg;
	struct ieee80211_node *ni = a->ni;
	uint16_t seq = a->seq;
	struct athn_node *an = (struct athn_node *)ni;
	int s, error = 0;

#if OpenBSD_IEEE80211_API
	if (ic->ic_state != IEEE80211_S_RUN)
		return;
#endif

	s = splnet();
	if (an->sta_index == 0) {
		error = athn_usb_create_node(usc, ni);
		if (error)
			printf("%s: could not add station %s to firmware "
			    "table\n", device_get_name(usc->usb_dev),
			    ether_sprintf(ni->ni_macaddr));
	}
#if OpenBSD_IEEE80211_API
	if (error == 0)
		ieee80211_auth_open_confirm(ic, ni, seq);
	ieee80211_unref_node(&ni);
#endif
	splx(s);
}
#endif

int
athn_usb_newauth(struct ieee80211com *ic, struct ieee80211_node *ni,
    int isnew, uint16_t seq)
{
#ifndef IEEE80211_STA_ONLY
	struct athn_usb_softc *usc = ic->ic_softc;
#if OpenBSD_IEEE80211_API
	struct ifnet *ifp = &ic->ic_if;
#endif
	struct athn_node *an = (struct athn_node *)ni;
	int nsta;
	struct athn_usb_newauth_cb_arg arg;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		return 0;

	if (!isnew && an->sta_index != 0) /* already in firmware table */
		return 0;

	/* Check if we have room in the firmware table. */
	nsta = 1; /* Account for default node. */
#if OpenBSD_IEEE80211_API
	ieee80211_iterate_nodes(ic, athn_usb_count_active_sta, &nsta);
	if (nsta >= AR_USB_MAX_STA) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: cannot authenticate station %s: firmware "
			    "table is full\n", device_get_name(usc->usb_dev),
			    ether_sprintf(ni->ni_macaddr));
		return ENOSPC;
	}
#endif

	/*
	 * In a process context, try to add this node to the
	 * firmware table and confirm the AUTH request.
	 */
	arg.ni = ieee80211_ref_node(ni);
	arg.seq = seq;
	athn_usb_do_async(usc, athn_usb_newauth_cb, &arg, sizeof(arg));
	return EBUSY;
#else
	return 0;
#endif /* IEEE80211_STA_ONLY */
}

#ifndef IEEE80211_STA_ONLY
void
athn_usb_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct athn_usb_softc *usc = ic->ic_softc;
	struct athn_node *an = (struct athn_node *)ni;

	/*
	 * Remove the node from the firmware table in a process context.
	 * Pass an index rather than the pointer which we will free.
	 */
	if (an->sta_index != 0)
		athn_usb_do_async(usc, athn_usb_node_free_cb,
		    &an->sta_index, sizeof(an->sta_index));
	usc->sc_node_free(ic, ni);
}

void
athn_usb_node_free_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
#if OpenBSD_IEEE80211_API
	struct ifnet *ifp = &ic->ic_if;
#endif
	uint8_t sta_index = *(uint8_t *)arg;
	int error;

	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_NODE_REMOVE,
	    &sta_index, sizeof(sta_index), NULL);
	if (error) {
		printf("%s: could not remove station %u from firmware table\n",
		    device_get_name(usc->usb_dev), sta_index);
		return;
	}
	usc->free_node_slots |= (1 << sta_index);
#if OpenBSD_IEEE80211_API
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: station %u removed from firmware table\n",
		    device_get_name(usc->usb_dev), sta_index);
#endif
}
#endif /* IEEE80211_STA_ONLY */

int
athn_usb_ampdu_tx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct athn_usb_softc *usc = ic->ic_softc;
	struct athn_node *an = (struct athn_node *)ni;
	struct athn_usb_aggr_cmd cmd;

	/* Do it in a process context. */
	cmd.sta_index = an->sta_index;
	cmd.tid = tid;
	athn_usb_do_async(usc, athn_usb_ampdu_tx_start_cb, &cmd, sizeof(cmd));
	return (0);
}

void
athn_usb_ampdu_tx_start_cb(struct athn_usb_softc *usc, void *arg)
{
	struct athn_usb_aggr_cmd *cmd = arg;
	struct ar_htc_target_aggr aggr;

	memset(&aggr, 0, sizeof(aggr));
	aggr.sta_index = cmd->sta_index;
	aggr.tidno = cmd->tid;
	aggr.aggr_enable = 1;
	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_TX_AGGR_ENABLE,
	    &aggr, sizeof(aggr), NULL);
}

void
athn_usb_ampdu_tx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct athn_usb_softc *usc = ic->ic_softc;
	struct athn_node *an = (struct athn_node *)ni;
	struct athn_usb_aggr_cmd cmd;

	/* Do it in a process context. */
	cmd.sta_index = an->sta_index;
	cmd.tid = tid;
	athn_usb_do_async(usc, athn_usb_ampdu_tx_stop_cb, &cmd, sizeof(cmd));
}

void
athn_usb_ampdu_tx_stop_cb(struct athn_usb_softc *usc, void *arg)
{
	struct athn_usb_aggr_cmd *cmd = arg;
	struct ar_htc_target_aggr aggr;

	memset(&aggr, 0, sizeof(aggr));
	aggr.sta_index = cmd->sta_index;
	aggr.tidno = cmd->tid;
	aggr.aggr_enable = 0;
	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_TX_AGGR_ENABLE,
	    &aggr, sizeof(aggr), NULL);
}

#ifndef IEEE80211_STA_ONLY
/* Try to find a node we can evict to make room in the firmware table. */
void
athn_usb_clean_nodes(void *arg, struct ieee80211_node *ni)
{
	struct athn_usb_softc *usc = arg;
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
	struct athn_node *an = (struct athn_node *)ni;

	/*
	 * Don't remove the default node (used for management frames).
	 * Nodes which are not in the firmware table also have index zero.
	 */
	if (an->sta_index == 0)
		return;

#if OpenBSD_IEEE80211_API
	/* Remove non-associated nodes. */
	if (ni->ni_state != IEEE80211_STA_AUTH &&
	    ni->ni_state != IEEE80211_STA_ASSOC) {
		athn_usb_remove_node(usc, ni);
		return;
	}

	/*
	 * Kick off inactive associated nodes. This won't help
	 * immediately but will help if the new STA retries later.
	 */
	if (ni->ni_inact >= IEEE80211_INACT_MAX) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_AUTH_EXPIRE);
		ieee80211_node_leave(ic, ni);
	}
#endif
}
#endif

int
athn_usb_create_node(struct athn_usb_softc *usc, struct ieee80211_node *ni)
{
	struct athn_node *an = (struct athn_node *)ni;
	struct ar_htc_target_sta sta;
	int error, sta_index;
#ifndef IEEE80211_STA_ONLY
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
#if OpenBSD_IEEE80211_API
	struct ifnet *ifp = &ic->ic_if;
#endif

	/* Firmware cannot handle more than 8 STAs. Try to make room first. */
#if OpenBSD_IEEE80211_API
	if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		ieee80211_iterate_nodes(ic, athn_usb_clean_nodes, usc);
#endif
#endif
	if (usc->free_node_slots == 0x00)
		return ENOBUFS;

	sta_index = ffs(usc->free_node_slots) - 1;
	if (sta_index < 0 || sta_index >= AR_USB_MAX_STA)
		return ENOSPC;

	/* Create node entry on target. */
	memset(&sta, 0, sizeof(sta));
	IEEE80211_ADDR_COPY(sta.macaddr, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(sta.bssid, ni->ni_bssid);
	sta.sta_index = sta_index;
	sta.maxampdu = 0xffff;
	if (ni->ni_flags & IEEE80211_NODE_HT)
		sta.flags |= htobe16(AR_HTC_STA_HT);
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_NODE_CREATE,
	    &sta, sizeof(sta), NULL);
	if (error != 0)
		return (error);
	an->sta_index = sta_index;
	usc->free_node_slots &= ~(1 << an->sta_index);

#if OpenBSD_IEEE80211_API
#ifndef IEEE80211_STA_ONLY
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: station %u (%s) added to firmware table\n",
		    device_get_name(usc->usb_dev), sta_index,
		    ether_sprintf(ni->ni_macaddr));
#endif
#endif
	return athn_usb_node_set_rates(usc, ni);
}

int
athn_usb_node_set_rates(struct athn_usb_softc *usc, struct ieee80211_node *ni)
{
	struct athn_node *an = (struct athn_node *)ni;
	struct ar_htc_target_rate rate;
	int i, j;

	/* Setup supported rates. */
	memset(&rate, 0, sizeof(rate));
	rate.sta_index = an->sta_index;
	rate.isnew = 1;
	rate.lg_rates.rs_nrates = ni->ni_rates.rs_nrates;
	memcpy(rate.lg_rates.rs_rates, ni->ni_rates.rs_rates,
	    ni->ni_rates.rs_nrates);
	if (ni->ni_flags & IEEE80211_NODE_HT) {
		rate.capflags |= htobe32(AR_RC_HT_FLAG);
		/* Setup HT rates. */
	#if OpenBSD_IEEE80211_API
		for (i = 0, j = 0; i < IEEE80211_HT_NUM_MCS; i++) {
			if (!isset(ni->ni_rxmcs, i))
				continue;
			if (j >= AR_HTC_RATE_MAX)
				break;
			rate.ht_rates.rs_rates[j++] = i;
		}
	#endif
		rate.ht_rates.rs_nrates = j;

#if OpenBSD_IEEE80211_API
		if (ni->ni_rxmcs[1]) /* dual-stream MIMO rates */
			rate.capflags |= htobe32(AR_RC_DS_FLAG);
#endif
#ifdef notyet
		if (ni->ni_htcaps & IEEE80211_HTCAP_CBW20_40)
			rate.capflags |= htobe32(AR_RC_40_FLAG);
		if (ni->ni_htcaps & IEEE80211_HTCAP_SGI40)
			rate.capflags |= htobe32(AR_RC_SGI_FLAG);
		if (ni->ni_htcaps & IEEE80211_HTCAP_SGI20)
			rate.capflags |= htobe32(AR_RC_SGI_FLAG);
#endif
	}

	return athn_usb_wmi_xcmd(usc, AR_WMI_CMD_RC_RATE_UPDATE,
	    &rate, sizeof(rate), NULL);
}

int
athn_usb_remove_node(struct athn_usb_softc *usc, struct ieee80211_node *ni)
{
	struct athn_node *an = (struct athn_node *)ni;
	int error;
#ifndef IEEE80211_STA_ONLY
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
#if OpenBSD_IEEE80211_API
	struct ifnet *ifp = &ic->ic_if;
#endif
#endif

	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_NODE_REMOVE,
	    &an->sta_index, sizeof(an->sta_index), NULL);
	if (error) {
		printf("%s: could not remove station %u (%s) from "
		    "firmware table\n", device_get_name(usc->usb_dev), an->sta_index,
		    ether_sprintf(ni->ni_macaddr));
		return error;
	}
#if OpenBSD_IEEE80211_API
#ifndef IEEE80211_STA_ONLY
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: station %u (%s) removed from firmware table\n",
		    device_get_name(usc->usb_dev), an->sta_index,
		    ether_sprintf(ni->ni_macaddr));
#endif
#endif
	usc->free_node_slots |= (1 << an->sta_index);
	an->sta_index = 0;
	return 0;
}

void
athn_usb_rx_enable(struct athn_softc *sc)
{
	AR_WRITE(sc, AR_CR, AR_CR_RXE);
	AR_WRITE_BARRIER(sc);
}

int
athn_usb_switch_chan(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	uint16_t mode;
	int error;

	/* Disable interrupts. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_DISABLE_INTR);
	if (error != 0)
		goto reset;
	/* Stop all Tx queues. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_DRAIN_TXQ_ALL);
	if (error != 0)
		goto reset;
	/* Stop Rx. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_STOP_RECV);
	if (error != 0)
		goto reset;

	/* If band or bandwidth changes, we need to do a full reset. */
	if (c->ic_flags != sc->curchan->ic_flags ||
	    ((extc != NULL) ^ (sc->curchanext != NULL))) {
		DPRINTFN(2, ("channel band switch\n"));
		goto reset;
	}

	error = athn_set_chan(sc, c, extc);
	if (AR_SREV_9271(sc) && error == 0)
		ar9271_load_ani(sc);
	if (error != 0) {
 reset:		/* Error found, try a full reset. */
		DPRINTFN(3, ("needs a full reset\n"));
		error = athn_hw_reset(sc, c, extc, 0);
		if (error != 0)	/* Hopeless case. */
			return (error);

		error = athn_set_chan(sc, c, extc);
		if (AR_SREV_9271(sc) && error == 0)
			ar9271_load_ani(sc);
		if (error != 0)
			return (error);
	}

	sc->ops.set_txpower(sc, c, extc);

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_START_RECV);
	if (error != 0)
		return (error);
	athn_rx_start(sc);

	mode = htobe16(IEEE80211_IS_CHAN_2GHZ(c) ?
	    AR_HTC_MODE_11NG : AR_HTC_MODE_11NA);
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_SET_MODE,
	    &mode, sizeof(mode), NULL);
	if (error != 0)
		return (error);

	/* Re-enable interrupts. */
	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_ENABLE_INTR);
	return (error);
}

void
athn_usb_updateedca(struct ieee80211com *ic)
{
	struct athn_usb_softc *usc = ic->ic_softc;

	/* Do it in a process context. */
	athn_usb_do_async(usc, athn_usb_updateedca_cb, NULL, 0);
}

void
athn_usb_updateedca_cb(struct athn_usb_softc *usc, void *arg)
{
	int s;

	s = splnet();
	athn_updateedca(&usc->sc_sc.sc_ic);
	splx(s);
}

void
athn_usb_updateslot(struct ieee80211com *ic)
{
	struct athn_usb_softc *usc = ic->ic_softc;

	return;	/* XXX */
	/* Do it in a process context. */
	athn_usb_do_async(usc, athn_usb_updateslot_cb, NULL, 0);
}

void
athn_usb_updateslot_cb(struct athn_usb_softc *usc, void *arg)
{
	int s;

	s = splnet();
	athn_updateslot(&usc->sc_sc.sc_ic);
	splx(s);
}

int
athn_usb_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct athn_usb_softc *usc = ic->ic_softc;
	struct athn_usb_cmd_key cmd;

	/* Defer setting of WEP keys until interface is brought up. */
#if OpenBSD_IEEE80211_API
	if ((ic->ic_if.if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING))
		return (0);
#endif

	/* Do it in a process context. */
	cmd.ni = (ni != NULL) ? ieee80211_ref_node(ni) : NULL;
	cmd.key = k;
	athn_usb_do_async(usc, athn_usb_set_key_cb, &cmd, sizeof(cmd));
	usc->sc_key_tasks++;
	return EBUSY;
}

void
athn_usb_set_key_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
	struct athn_usb_cmd_key *cmd = arg;
	int s;

	usc->sc_key_tasks--;

	s = splnet();
	athn_usb_write_barrier(&usc->sc_sc);
	athn_set_key(ic, cmd->ni, cmd->key);
#if OpenBSD_IEEE80211_API
	if (usc->sc_key_tasks == 0) {
		DPRINTF(("marking port %s valid\n",
		    ether_sprintf(cmd->ni->ni_macaddr)));
		cmd->ni->ni_port_valid = 1;
		ieee80211_set_link_state(ic, LINK_STATE_UP);
	}
	if (cmd->ni != NULL)
		ieee80211_release_node(ic, cmd->ni);
	splx(s);
#endif
}

void
athn_usb_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct athn_usb_softc *usc = ic->ic_softc;
	struct athn_usb_cmd_key cmd;
#if OpenBSD_IEEE80211_API
	if (!(ic->ic_if.if_flags & IFF_RUNNING) ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;	/* Nothing to do. */
#endif
	/* Do it in a process context. */
	cmd.ni = (ni != NULL) ? ieee80211_ref_node(ni) : NULL;
	cmd.key = k;
	athn_usb_do_async(usc, athn_usb_delete_key_cb, &cmd, sizeof(cmd));
}

void
athn_usb_delete_key_cb(struct athn_usb_softc *usc, void *arg)
{
	struct ieee80211com *ic = &usc->sc_sc.sc_ic;
	struct athn_usb_cmd_key *cmd = arg;
	int s;

	s = splnet();
	athn_delete_key(ic, cmd->ni, cmd->key);
#if OpenBSD_IEEE80211_API
	if (cmd->ni != NULL)
		ieee80211_release_node(ic, cmd->ni);
	splx(s);
#endif
}

#ifndef IEEE80211_STA_ONLY
void
athn_usb_bcneof(struct usb_xfer *xfer, void *priv,
    usb_error_t status)
{
	struct athn_usb_data *data = priv;
#if OpenBSD_ONLY
	struct athn_usb_softc *usc = data->sc;

	if (__predict_false(status == USB_ERR_STALLED))
		usbd_clear_endpoint_stall_async(usc->tx_data_pipe);
	usc->tx_bcn = data;
#endif
}

/*
 * Process Software Beacon Alert interrupts.
 */
void
athn_usb_swba(struct athn_usb_softc *usc)
{
	struct athn_softc *sc = &usc->sc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct athn_usb_data *data;
	struct ieee80211_frame *wh;
	struct ar_stream_hdr *hdr;
	struct ar_htc_frame_hdr *htc;
	struct ar_tx_bcn *bcn;
	struct mbuf *m;
	int error;
#if OpenBSD_IEEE80211_API
	if (ic->ic_dtim_count == 0)
		ic->ic_dtim_count = ic->ic_dtim_period - 1;
	else
		ic->ic_dtim_count--;
#endif
#if OpenBSD_ONLY
	/* Make sure previous beacon has been sent. */
	if (usc->tx_bcn == NULL)
		return;
	data = usc->tx_bcn;
#endif
#if OpenBSD_IEEE80211_API
	/* Get new beacon. */
	m = ieee80211_beacon_alloc(ic, ic->ic_bss);
	if (__predict_false(m == NULL))
		return;
	/* Assign sequence number. */
	wh = mtod(m, struct ieee80211_frame *);
	*(uint16_t *)&wh->i_seq[0] =
	    htole16(ic->ic_bss->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ic->ic_bss->ni_txseq++;

	hdr = (struct ar_stream_hdr *)data->buf;
	hdr->tag = htole16(AR_USB_TX_STREAM_TAG);
	hdr->len = htole16(sizeof(*htc) + sizeof(*bcn) + m->m_pkthdr.len);

	htc = (struct ar_htc_frame_hdr *)&hdr[1];
	memset(htc, 0, sizeof(*htc));
	htc->endpoint_id = usc->ep_bcn;
	htc->payload_len = htobe16(sizeof(*bcn) + m->m_pkthdr.len);

	bcn = (struct ar_tx_bcn *)&htc[1];
	memset(bcn, 0, sizeof(*bcn));
	bcn->vif_idx = 0;

	m_copydata(m, 0, m->m_pkthdr.len, &bcn[1]);
#endif
#if OpenBSD_ONLY
	usbd_setup_xfer(data->xfer, usc->tx_data_pipe, data, data->buf,
	    sizeof(*hdr) + sizeof(*htc) + sizeof(*bcn) + m->m_pkthdr.len,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY, ATHN_USB_TX_TIMEOUT,
	    athn_usb_bcneof);

	m_freem(m);
	usc->tx_bcn = NULL;
	error = usbd_transfer_start(data->xfer);
	if (__predict_false(error != USBD_IN_PROGRESS && error != 0))
		usc->tx_bcn = data;
#endif
}
#endif

/* Update current transmit rate for a node based on firmware Tx status. */
void
athn_usb_tx_status(void *arg, struct ieee80211_node *ni)
{
	struct ar_wmi_evt_txstatus *ts = arg;
	struct athn_node *an = (struct athn_node *)ni;
	uint8_t rate_index = (ts->rate & AR_HTC_TXSTAT_RATE);

	if (an->sta_index != ts->cookie) /* Tx report for a different node */
		return;
#if OpenBSD_IEEE80211_API
	if (ts->flags & AR_HTC_TXSTAT_MCS) {
		if (isset(ni->ni_rxmcs, rate_index))
			ni->ni_txmcs = rate_index;
	} else if (rate_index < ni->ni_rates.rs_nrates)
		ni->ni_txrate = rate_index;
#endif
}

void
athn_usb_rx_wmi_ctrl(struct athn_usb_softc *usc, uint8_t *buf, int len)
{
	struct ar_wmi_cmd_hdr *wmi;
	uint16_t cmd_id;

	if (__predict_false(len < sizeof(*wmi)))
		return;
	wmi = (struct ar_wmi_cmd_hdr *)buf;
	cmd_id = betoh16(wmi->cmd_id);

	if (!(cmd_id & AR_WMI_EVT_FLAG)) {
		if (usc->wait_cmd_id != cmd_id)
			return;	/* Unexpected reply. */
		if (usc->obuf != NULL) {
			/* Copy answer into caller supplied buffer. */
			memcpy(usc->obuf, &wmi[1], len - sizeof(*wmi));
		}
		/* Notify caller of completion. */
		wakeup(&usc->wait_cmd_id);
		return;
	}
	switch (cmd_id & 0xfff) {
#ifndef IEEE80211_STA_ONLY
	case AR_WMI_EVT_SWBA:
		athn_usb_swba(usc);
		break;
#endif
	case AR_WMI_EVT_TXSTATUS: {
		struct ar_wmi_evt_txstatus_list *tsl;
		int i;

		tsl = (struct ar_wmi_evt_txstatus_list *)&wmi[1];
		for (i = 0; i < tsl->count && i < nitems(tsl->ts); i++) {
			struct ieee80211com *ic = &usc->sc_sc.sc_ic;
#if OpenBSD_IEEE80211_API
			struct athn_node *an = (struct athn_node *)ic->ic_bss;
#endif
			struct ar_wmi_evt_txstatus *ts = &tsl->ts[i];
			uint8_t qid;

			/* Skip the node we use to send management frames. */
			if (ts->cookie == 0)
				continue;

			/* Skip Tx reports for non-data frame endpoints. */
			qid = (ts->rate & AR_HTC_TXSTAT_EPID) >>
				AR_HTC_TXSTAT_EPID_SHIFT;
			if (qid != usc->ep_data[ATHN_QID_AC_BE] &&
			    qid != usc->ep_data[ATHN_QID_AC_BK-1] &&
			    qid != usc->ep_data[ATHN_QID_AC_VI-1] &&
			    qid != usc->ep_data[ATHN_QID_AC_VO-1])
				continue;
#if OpenBSD_IEEE80211_API
			if (ts->cookie == an->sta_index)
				athn_usb_tx_status(ts, ic->ic_bss);
			else
				ieee80211_iterate_nodes(ic, athn_usb_tx_status,
				    ts);
#endif
		}
		break;
	}
	case AR_WMI_EVT_FATAL:
		printf("%s: fatal firmware error\n", device_get_name(usc->usb_dev));
		break;
	default:
		DPRINTF(("WMI event %d ignored\n", cmd_id));
		break;
	}
}

#if NBPFILTER > 0
void
athn_usb_rx_radiotap(struct athn_softc *sc, struct mbuf *m,
    struct ar_rx_status *rs)
{
#define IEEE80211_RADIOTAP_F_SHORTGI	0x80	/* XXX from FBSD */

	struct athn_rx_radiotap_header *tap = &sc->sc_rxtap;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf mb;
	uint8_t rate;

	tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
	tap->wr_tsft = htole64(betoh64(rs->rs_tstamp));
	tap->wr_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
	tap->wr_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
	tap->wr_dbm_antsignal = rs->rs_rssi;
	/* XXX noise. */
	tap->wr_antenna = rs->rs_antenna;
	tap->wr_rate = 0;	/* In case it can't be found below. */
	rate = rs->rs_rate;
	if (rate & 0x80) {		/* HT. */
		/* Bit 7 set means HT MCS instead of rate. */
		tap->wr_rate = rate;
		if (!(rs->rs_flags & AR_RXS_FLAG_GI))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTGI;

	} else if (rate & 0x10) {	/* CCK. */
		if (rate & 0x04)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		switch (rate & ~0x14) {
		case 0xb: tap->wr_rate =   2; break;
		case 0xa: tap->wr_rate =   4; break;
		case 0x9: tap->wr_rate =  11; break;
		case 0x8: tap->wr_rate =  22; break;
		}
	} else {			/* OFDM. */
		switch (rate) {
		case 0xb: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0xa: tap->wr_rate =  24; break;
		case 0xe: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xd: tap->wr_rate =  72; break;
		case 0x8: tap->wr_rate =  96; break;
		case 0xc: tap->wr_rate = 108; break;
		}
	}
	mb.m_data = (caddr_t)tap;
	mb.m_len = sc->sc_rxtap_len;
	mb.m_next = m;
	mb.m_nextpkt = NULL;
	mb.m_type = 0;
	mb.m_flags = 0;
	bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
}
#endif


void
athn_usb_rx_frame(struct athn_usb_softc *usc, struct mbuf *m/*,
    struct mbuf_list *ml*/)
{
#if OpenBSD_ONLY
	struct athn_softc *sc = &usc->sc_sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct ieee80211_rxinfo rxi;

	struct ar_htc_frame_hdr *htc;
	struct ar_rx_status *rs;
	uint16_t datalen;
	int s;

	if (__predict_false(m->m_len < sizeof(*htc)))
		goto skip;
	htc = mtod(m, struct ar_htc_frame_hdr *);
	if (__predict_false(htc->endpoint_id == 0)) {
		DPRINTF(("bad endpoint %d\n", htc->endpoint_id));
		goto skip;
	}
	if (htc->flags & AR_HTC_FLAG_TRAILER) {
		if (m->m_len < htc->control[0])
			goto skip;
		m_adj(m, -(int)htc->control[0]);
	}
	m_adj(m, sizeof(*htc));	/* Strip HTC header. */

	if (__predict_false(m->m_len < sizeof(*rs)))
		goto skip;
	rs = mtod(m, struct ar_rx_status *);

	/* Make sure that payload fits. */
	datalen = betoh16(rs->rs_datalen);
	if (__predict_false(m->m_len < sizeof(*rs) + datalen))
		goto skip;

	if (__predict_false(datalen < sizeof(*wh) + IEEE80211_CRC_LEN))
		goto skip;

	if (rs->rs_status != 0) {
		if (rs->rs_status & AR_RXS_RXERR_DECRYPT)
			ic->ic_stats.is_ccmp_dec_errs++;
		goto skip;
	}

	m_adj(m, sizeof(*rs));	/* Strip Rx status. */

	s = splnet();

	/* Grab a reference to the source node. */

	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);

	/* Remove any HW padding after the 802.11 header. */
	if (!(wh->i_fc[0] & IEEE80211_FC0_TYPE_CTL)) {

		u_int hdrlen = ieee80211_get_hdrlen(wh);
		if (hdrlen & 3) {
			memmove((caddr_t)wh + 2, wh, hdrlen);
			m_adj(m, 2);
		}
		wh = mtod(m, struct ieee80211_frame *);
	}

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL))
		athn_usb_rx_radiotap(sc, m, rs);
#endif
	/* Trim 802.11 FCS after radiotap. */
	m_adj(m, -IEEE80211_CRC_LEN);

	/* Send the frame to the 802.11 layer. */

	memset(&rxi, 0, sizeof(rxi));
	rxi.rxi_rssi = rs->rs_rssi + AR_USB_DEFAULT_NF;
	rxi.rxi_tstamp = betoh64(rs->rs_tstamp);

	if (!(wh->i_fc[0] & IEEE80211_FC0_TYPE_CTL) &&
	    (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) &&
	    (ic->ic_flags & IEEE80211_F_RSNON) &&
	    (ni->ni_flags & IEEE80211_NODE_RXPROT) &&
	    (ni->ni_rsncipher == IEEE80211_CIPHER_CCMP ||
	    (IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    ni->ni_rsngroupcipher == IEEE80211_CIPHER_CCMP))) {
		if (ar5008_ccmp_decap(sc, m, ni) != 0) {
			ieee80211_release_node(ic, ni);
			splx(s);
			goto skip;
		}
		rxi.rxi_flags |= IEEE80211_RXI_HWDEC;
	}
	ieee80211_inputm(ifp, m, ni, &rxi, ml);


	/* Node is no longer needed. */
	ieee80211_release_node(ic, ni);

	splx(s);
	return;
 skip:
	m_freem(m);
#endif
}

void
athn_usb_rxeof(struct usb_xfer *xfer, struct athn_usb_data *data)
{
#if OpenBSD_ONLY
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
#endif
	struct athn_usb_softc *usc = usbd_xfer_softc(xfer);
#if OpenBSD_IEEE80211_API
	struct ifnet *ifp = &sc->sc_ic.ic_if;
#endif
	struct athn_usb_rx_stream *stream = &usc->rx_stream;
	char *buf = data->buf;
	struct ar_stream_hdr *hdr;
	struct mbuf *m;
	uint16_t pktlen;
	int off;
	int32_t len;

#if OpenBSD_ONLY
	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTF(("RX status=%d\n", status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(usc->rx_data_pipe);
		if (status != USBD_CANCELLED)
			goto resubmit;
		return;
	}
#endif
	usbd_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (stream->left > 0) {
		if (len >= stream->left) {
			/* We have all our pktlen bytes now. */
			if (__predict_true(stream->m != NULL)) {
				memcpy(mtod(stream->m, uint8_t *) +
				    stream->moff, buf, stream->left);
				athn_usb_rx_frame(usc, stream->m/*, &ml*/); // MichalP: mbuf_list doesn't exist in FreeBSD
				stream->m = NULL;
			}
			/* Next header is 32-bit aligned. */
			off = (stream->left + 3) & ~3;
			buf += off;
			len -= off;
			stream->left = 0;
		} else {
			/* Still need more bytes, save what we have. */
			if (__predict_true(stream->m != NULL)) {
				memcpy(mtod(stream->m, uint8_t *) +
				    stream->moff, buf, len);
				stream->moff += len;
			}
			stream->left -= len;
			goto resubmit;
		}
	}
	KASSERT(stream->left == 0, "athn_usb_rxeof");
	while (len >= sizeof(*hdr)) {
		hdr = (struct ar_stream_hdr *)buf;
		if (hdr->tag != htole16(AR_USB_RX_STREAM_TAG)) {
			DPRINTF(("invalid tag 0x%x\n", hdr->tag));
			break;
		}
		pktlen = letoh16(hdr->len);
		buf += sizeof(*hdr);
		len -= sizeof(*hdr);
#if OpenBSD_ONLY // MichalP: mbuf_list operations needs proper refactoring
		if (__predict_true(pktlen <= MCLBYTES)) {
			/* Allocate an mbuf to store the next pktlen bytes. */
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (__predict_true(m != NULL)) {
				m->m_pkthdr.len = m->m_len = pktlen;
				if (pktlen > MHLEN) {
					MCLGET(m, M_DONTWAIT);
					if (!(m->m_flags & M_EXT)) {
						m_free(m);
						m = NULL;
					}
				}
			}
		} else	/* Drop frames larger than MCLBYTES. */
			m = NULL;
#endif
		/*
		 * NB: m can be NULL, in which case the next pktlen bytes
		 * will be discarded from the Rx stream.
		 */
		if (pktlen > len) {
			/* Need more bytes, save what we have. */
			stream->m = m;	/* NB: m can be NULL. */
			if (__predict_true(stream->m != NULL)) {
				memcpy(mtod(stream->m, uint8_t *), buf, len);
				stream->moff = len;
			}
			stream->left = pktlen - len;
			goto resubmit;
		}
		if (__predict_true(m != NULL)) {
			/* We have all the pktlen bytes in this xfer. */
			memcpy(mtod(m, uint8_t *), buf, pktlen);
			athn_usb_rx_frame(usc, m/*, &ml*/); // MichalP: Another mbuf_list
		}

		/* Next header is 32-bit aligned. */
		off = (pktlen + 3) & ~3;
		buf += off;
		len -= off;
	}
#if OpenBSD_IEEE80211_API
	// TODO Refactor functions which use mbuf_list.
	// mbuf_list is OpenBSD specific and if_input uses the mbuf directly.
	// In the next steps, we should remove mbuf_list support from this driver.
	if_input(ifp, m);
#endif

	resubmit:
		printf("Resubmit called"); // MichalP: Temp printf call to maintain resubmit goto
	/* Setup a new transfer. */
#if OpenBSD_ONLY
	usbd_setup_xfer(xfer, usc->rx_data_pipe, data, data->buf,
	    ATHN_USB_RXBUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USBD_NO_TIMEOUT, athn_usb_rxeof);
	(void)usbd_transfer_start(xfer);
#endif
}

void
athn_usb_txeof(struct usb_xfer *xfer, struct athn_usb_data* data)
{
	struct athn_usb_softc *usc = usbd_xfer_softc(xfer);
	struct athn_softc *sc = &usc->sc_sc;
#if OpenBSD_IEEE80211_API
	struct ifnet *ifp = &sc->sc_ic.ic_if;
#endif

	device_printf(sc->sc_dev, "%s: called; data=%p\n", __func__, data);

	ATHN_ASSERT_LOCKED(sc);

	if (usc->sc_tx_n_active == 0) {
		device_printf(sc->sc_dev,
					  "%s: completed but tx_active=0\n",
					  __func__);
	} else {
		usc->sc_tx_n_active--;
	}

//	if (data->m) {
		/* XXX status? */
		/* XXX we get TX status via the RX path.. */
//		ieee80211_tx_complete(data->ni, data->m, 0);
//		data->m = NULL;
//		data->ni = NULL;
//	}
}

int
athn_usb_tx(struct athn_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	struct athn_node *an = (struct athn_node *)ni;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct athn_usb_data *data;
	struct ar_stream_hdr *hdr;
	struct ar_htc_frame_hdr *htc;
	struct ar_tx_frame *txf;
	struct ar_tx_mgmt *txm;
	caddr_t frm;
	uint16_t qos;
	uint8_t qid, tid = 0;
	int hasqos, xferlen, error;

	wh = mtod(m, struct ieee80211_frame *);
#if OpenBSD_IEEE80211_API
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_get_txkey(ic, wh, ni);
		if (k->k_cipher == IEEE80211_CIPHER_CCMP) {
			u_int hdrlen = ieee80211_get_hdrlen(wh);
			if (ar5008_ccmp_encap(m, hdrlen, k) != 0)
				return (ENOBUFS);
		} else {
			if ((m = ieee80211_encrypt(ic, m, k)) == NULL)
				return (ENOBUFS);
			k = NULL; /* skip hardware crypto further below */
		}
		wh = mtod(m, struct ieee80211_frame *);
	}
	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
		qid = ieee80211_up_to_ac(ic, tid);
	} else
		qid = ATHN_QID_AC_BE;
#endif
	/* Grab a Tx buffer from our free list. */
#if OpenBSD_ONLY
	data = TAILQ_FIRST(&usc->tx_free_list);
	TAILQ_REMOVE(&usc->tx_free_list, data, next);
#endif

#if NBPFILTER > 0
	/* XXX Change radiotap Tx header for USB (no txrate). */
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct athn_tx_radiotap_header *tap = &sc->sc_txtap;
		struct mbuf mb;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	/* NB: We don't take advantage of USB Tx stream mode for now. */
	hdr = (struct ar_stream_hdr *)data->buf;
	hdr->tag = htole16(AR_USB_TX_STREAM_TAG);

	htc = (struct ar_htc_frame_hdr *)&hdr[1];
	memset(htc, 0, sizeof(*htc));
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_DATA) {
		htc->endpoint_id = usc->ep_data[qid];

		txf = (struct ar_tx_frame *)&htc[1];
		memset(txf, 0, sizeof(*txf));
		txf->data_type = AR_HTC_NORMAL;
		txf->node_idx = an->sta_index;
		txf->vif_idx = 0;
		txf->tid = tid;
#if OpenBSD_IEEE80211_API
		if (m->m_pkthdr.len + IEEE80211_CRC_LEN > ic->ic_rtsthreshold)
			txf->flags |= htobe32(AR_HTC_TX_RTSCTS);
		else if (ic->ic_flags & IEEE80211_F_USEPROT) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				txf->flags |= htobe32(AR_HTC_TX_CTSONLY);
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				txf->flags |= htobe32(AR_HTC_TX_RTSCTS);
		}

		if (k != NULL) {
			/* Map 802.11 cipher to hardware encryption type. */
			if (k->k_cipher == IEEE80211_CIPHER_CCMP) {
				txf->key_type = AR_ENCR_TYPE_AES;
			} else
				panic("unsupported cipher");
			/*
			 * NB: The key cache entry index is stored in the key
			 * private field when the key is installed.
			 */
			txf->key_idx = (uintptr_t)k->k_priv;
		} else
			txf->key_idx = 0xff;
#endif
		txf->cookie = an->sta_index;
		frm = (caddr_t)&txf[1];
	} else {
		htc->endpoint_id = usc->ep_mgmt;

		txm = (struct ar_tx_mgmt *)&htc[1];
		memset(txm, 0, sizeof(*txm));
		txm->node_idx = an->sta_index;
		txm->vif_idx = 0;
		txm->key_idx = 0xff;
		txm->cookie = an->sta_index;
		frm = (caddr_t)&txm[1];
	}
	/* Copy payload. */
	m_copydata(m, 0, m->m_pkthdr.len, frm);
	frm += m->m_pkthdr.len;
	m_freem(m);

	/* Finalize headers. */
	htc->payload_len = htobe16(frm - (caddr_t)&htc[1]);
	hdr->len = htole16(frm - (caddr_t)&hdr[1]);
	xferlen = frm - data->buf;

	data->buflen = xferlen;

	STAILQ_INSERT_TAIL(&usc->sc_tx_pending[ATHN_BULK_TX], data, next);
	usbd_transfer_start(usc->sc_xfer[ATHN_BULK_TX]);

#if OpenBSD_IEEE80211_API
	ieee80211_release_node(ic, ni);
#endif
	return (0);
}

void
athn_usb_start(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive())
		return;

	for (;;) {
#if OpenBSD_ONLY
		if (TAILQ_EMPTY(&usc->tx_free_list)) {
			ifq_set_oactive();
			break;
		}
#endif
		/* Send pending management frames first. */
#if OpenBSD_IEEE80211_API
		m = mq_dequeue(&ic->ic_mgtq);
		if (m != NULL) {
			ni = m->m_pkthdr.ph_cookie;
			goto sendit;
		}
		if (ic->ic_state != IEEE80211_S_RUN)
			break;
#endif

		/* Encapsulate and send data frames. */
		ALTQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
#if NBPFILTER > 0
		f (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
#if OpenBSD_IEEE80211_API
		if ((m = ieee80211_encap(ifp, m, &ni)) == NULL)
			continue;
#endif
 sendit:
#if NBPFILTER > 0
		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_OUT);
#endif
		if (athn_usb_tx(sc, m, ni) != 0) {
#if OpenBSD_IEEE80211_API
			ieee80211_release_node(ic, ni);
#endif
			continue;
		}

		sc->sc_tx_timer = 5;
	}
}

void
athn_usb_watchdog(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", device_get_name(sc->sc_dev));
			athn_usb_init(ifp);
			return;
		}
	}
#if OpenBSD_IEEE80211_API
	ieee80211_watchdog(ifp);
#endif
}

int
athn_usb_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				error = athn_usb_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				athn_usb_stop(ifp);
		}
		break;
#if OpenBSD_IEEE80211_API
	case SIOCS80211CHANNEL:
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
			    (IFF_UP | IFF_RUNNING)) {
				athn_usb_switch_chan(sc, ic->ic_ibss_chan,
				    NULL);
			}
			error = 0;
		}
		break;
#endif
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET) {
		error = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING)) {
			athn_usb_stop(ifp);
			error = athn_usb_init(ifp);
		}
	}
	return (error);
}

int
athn_usb_init(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	struct athn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_channel *c, *extc;
	struct athn_usb_data *data;
	struct ar_htc_target_vif hvif;
	struct ar_htc_target_sta sta;
	struct ar_htc_cap_target hic;
	uint16_t mode;
	int i, error;

#if OpenBSD_IEEE80211_API
	c = ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	extc = NULL;

	/* In case a new MAC address has been configured. */
	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
#endif
	error = athn_set_power_awake(sc);
	if (error != 0)
		goto fail;

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_FLUSH_RECV);
	if (error != 0)
		goto fail;

	error = athn_hw_reset(sc, c, extc, 1);
	if (error != 0)
		goto fail;

	ops->set_txpower(sc, c, extc);

	mode = htobe16(IEEE80211_IS_CHAN_2GHZ(c) ?
	    AR_HTC_MODE_11NG : AR_HTC_MODE_11NA);
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_SET_MODE,
	    &mode, sizeof(mode), NULL);
	if (error != 0)
		goto fail;

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_ATH_INIT);
	if (error != 0)
		goto fail;

	error = athn_usb_wmi_cmd(usc, AR_WMI_CMD_START_RECV);
	if (error != 0)
		goto fail;

	athn_rx_start(sc);

	/* Create main interface on target. */
	memset(&hvif, 0, sizeof(hvif));
	hvif.index = 0;
#if OpenBSD_IEEE80211_API
	IEEE80211_ADDR_COPY(hvif.myaddr, ic->ic_myaddr);
#endif
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		hvif.opmode = htobe32(AR_HTC_M_STA);
		break;
	case IEEE80211_M_MONITOR:
		hvif.opmode = htobe32(AR_HTC_M_MONITOR);
		break;
#ifndef IEEE80211_STA_ONLY
	case IEEE80211_M_IBSS:
		hvif.opmode = htobe32(AR_HTC_M_IBSS);
		break;
	case IEEE80211_M_AHDEMO:
		hvif.opmode = htobe32(AR_HTC_M_AHDEMO);
		break;
	case IEEE80211_M_HOSTAP:
		hvif.opmode = htobe32(AR_HTC_M_HOSTAP);
		break;
	case IEEE80211_M_MBSS:
		break;
	case IEEE80211_M_WDS:
		break;
#endif
	}
#if OpenBSD_IEEE80211_API
	hvif.rtsthreshold = htobe16(ic->ic_rtsthreshold);
#endif
	DPRINTF(("creating VAP\n"));
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_VAP_CREATE,
	    &hvif, sizeof(hvif), NULL);
	if (error != 0)
		goto fail;

	/* Create a fake node to send management frames before assoc. */
	memset(&sta, 0, sizeof(sta));
#if OpenBSD_IEEE80211_API
	IEEE80211_ADDR_COPY(sta.macaddr, ic->ic_myaddr);
#endif
	sta.sta_index = 0;
	sta.is_vif_sta = 1;
	sta.vif_index = hvif.index;
	sta.maxampdu = 0xffff;
	DPRINTF(("creating default node\n"));
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_NODE_CREATE,
	    &sta, sizeof(sta), NULL);
	if (error != 0)
		goto fail;
	usc->free_node_slots = ~(1 << sta.sta_index);

	/* Update target capabilities. */
	memset(&hic, 0, sizeof(hic));
	hic.ampdu_limit = htobe32(0x0000ffff);
	hic.ampdu_subframes = 20;
	hic.txchainmask = sc->txchainmask;
	DPRINTF(("updating target configuration\n"));
	error = athn_usb_wmi_xcmd(usc, AR_WMI_CMD_TARGET_IC_UPDATE,
	    &hic, sizeof(hic), NULL);
	if (error != 0)
		goto fail;

#if OpenBSD_ONLY
	/* Queue Rx xfers. */
	for (i = 0; i < ATHN_USB_RX_LIST_COUNT; i++) {
		data = &usc->rx_data[i];
		usbd_setup_xfer(data->xfer, usc->rx_data_pipe, data, data->buf,
		    ATHN_USB_RXBUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, athn_usb_rxeof);
		error = usbd_transfer_start(data->xfer);
		if (error != 0 && error != USBD_IN_PROGRESS)
			goto fail;
	}
#endif
	/* We're ready to go. */
	ifp->if_flags |= IFF_RUNNING;
	ifq_clr_oactive();

#ifdef notyet
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		/* Install WEP keys. */
		for (i = 0; i < IEEE80211_WEP_NKID; i++)
			athn_usb_set_key(ic, NULL, &ic->ic_nw_keys[i]);
	}
#endif
#if OpenBSD_IEEE80211_API
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
#endif
	athn_usb_wait_async(usc);
	return (0);
 fail:
	athn_usb_stop(ifp);
	return (error);
}

void
athn_usb_stop(struct ifnet *ifp)
{
	struct athn_softc *sc = ifp->if_softc;
	struct athn_usb_softc *usc = (struct athn_usb_softc *)sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ar_htc_target_vif hvif;
	uint8_t sta_index;
	int s;

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_RUNNING;
	ifq_clr_oactive();

	s = splusb();
#if OpenBSD_IEEE80211_API
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
#endif

	/* Wait for all async commands to complete. */
	athn_usb_wait_async(usc);

#if OpenBSD_ONLY
	timeout_del(&sc->scan_to);
	timeout_del(&sc->calib_to);
#endif

	/* Remove all non-default nodes. */
	for (sta_index = 1; sta_index < AR_USB_MAX_STA; sta_index++) {
		if (usc->free_node_slots & (1 << sta_index))
			continue;
		(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_NODE_REMOVE,
		    &sta_index, sizeof(sta_index), NULL);
	}

	/* Remove main interface. This also invalidates our default node. */
	memset(&hvif, 0, sizeof(hvif));
	hvif.index = 0;
#if OpenBSD_IEEE80211_API
	IEEE80211_ADDR_COPY(hvif.myaddr, ic->ic_myaddr);
#endif
	(void)athn_usb_wmi_xcmd(usc, AR_WMI_CMD_VAP_REMOVE,
	    &hvif, sizeof(hvif), NULL);

	usc->free_node_slots = 0xff;

	(void)athn_usb_wmi_cmd(usc, AR_WMI_CMD_DISABLE_INTR);
	(void)athn_usb_wmi_cmd(usc, AR_WMI_CMD_DRAIN_TXQ_ALL);
	(void)athn_usb_wmi_cmd(usc, AR_WMI_CMD_STOP_RECV);

	athn_reset(sc, 0);
	athn_init_pll(sc, NULL);
	athn_set_power_awake(sc);
	athn_reset(sc, 1);
	athn_init_pll(sc, NULL);
	athn_set_power_sleep(sc);

#if OpenBSD_ONLY
	/* Abort Tx/Rx. */
	usbd_abort_pipe(usc->tx_data_pipe);
	usbd_abort_pipe(usc->rx_data_pipe);
#endif

	/* Free Tx/Rx buffers. */
	athn_usb_free_tx_list(usc);
	athn_usb_free_rx_list(usc);
	splx(s);

	/* Flush Rx stream. */
	m_freem(usc->rx_stream.m);
	usc->rx_stream.m = NULL;
	usc->rx_stream.left = 0;
}