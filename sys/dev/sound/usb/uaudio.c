/*	$NetBSD: uaudio.c,v 1.41 2001/01/23 14:04:13 augustss Exp $	*/
/*	$FreeBSD$: */

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * USB audio specs: http://www.usb.org/developers/data/devclass/audio10.pdf
 *                  http://www.usb.org/developers/data/devclass/frmts10.pdf
 *                  http://www.usb.org/developers/data/devclass/termt10.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#endif
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/reboot.h>		/* for bootverbose */
#include <sys/select.h>
#include <sys/proc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#endif
#include <sys/poll.h>
#include <sys/sysctl.h>

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>
#elif defined(__FreeBSD__)
#include <dev/sound/pcm/sound.h>	/* XXXXX */
#include <dev/sound/chip.h>
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_quirks.h>

#include <dev/sound/usb/uaudioreg.h>
#include <dev/sound/usb/uaudio.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (uaudiodebug) logprintf x
#define DPRINTFN(n,x)	if (uaudiodebug>(n)) logprintf x
int	uaudiodebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, uaudio, CTLFLAG_RW, 0, "USB uaudio");
SYSCTL_INT(_hw_usb_uaudio, OID_AUTO, debug, CTLFLAG_RW,
	   &uaudiodebug, 0, "uaudio debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UAUDIO_NCHANBUFS 6	/* number of outstanding request */
#define UAUDIO_NFRAMES   20	/* ms of sound in each request */


#define MIX_MAX_CHAN 8
struct mixerctl {
	u_int16_t	wValue[MIX_MAX_CHAN]; /* using nchan */
	u_int16_t	wIndex;
	u_int8_t	nchan;
	u_int8_t	type;
#define MIX_ON_OFF	1
#define MIX_SIGNED_16	2
#define MIX_UNSIGNED_16	3
#define MIX_SIGNED_8	4
#define MIX_SIZE(n) ((n) == MIX_SIGNED_16 || (n) == MIX_UNSIGNED_16 ? 2 : 1)
#define MIX_UNSIGNED(n) ((n) == MIX_UNSIGNED_16)
	int		minval, maxval;
	u_int		delta;
	u_int		mul;
#if defined(__FreeBSD__) /* XXXXX */
	unsigned	ctl;
#else
	u_int8_t	class;
	char		ctlname[MAX_AUDIO_DEV_LEN];
	char		*ctlunit;
#endif
};
#define MAKE(h,l) (((h) << 8) | (l))

struct as_info {
	u_int8_t	alt;
	u_int8_t	encoding;
	usbd_interface_handle	ifaceh;
	usb_interface_descriptor_t *idesc;
	usb_endpoint_descriptor_audio_t *edesc;
	struct usb_audio_streaming_type1_descriptor *asf1desc;
};

struct chan {
	int	terminal;	/* terminal id */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	void	(*intr)(void *);	/* dma completion intr handler */
	void	*arg;		/* arg for intr() */
#else
	struct pcm_channel *pcm_ch;
#endif
	usbd_pipe_handle pipe;
	int	dir;		/* direction */

	u_int	sample_size;
	u_int	sample_rate;
	u_int	bytes_per_frame;
	u_int	fraction;	/* fraction/1000 is the extra samples/frame */
	u_int	residue;	/* accumulates the fractional samples */

	u_char	*start;		/* upper layer buffer start */
	u_char	*end;		/* upper layer buffer end */
	u_char	*cur;		/* current position in upper layer buffer */
	int	blksize;	/* chunk size to report up */
	int	transferred;	/* transferred bytes not reported up */

	char	nofrac;		/* don't do sample rate adjustment */

	int	curchanbuf;
	struct chanbuf {
		struct chan         *chan;
		usbd_xfer_handle xfer;
		u_char              *buffer;
		u_int16_t           sizes[UAUDIO_NFRAMES];
		u_int16_t	    size;
	} chanbufs[UAUDIO_NCHANBUFS];

	struct uaudio_softc *sc; /* our softc */
#if defined(__FreeBSD__)
	u_int32_t format;
	int	precision;
	int	channels;
#endif
};

struct uaudio_softc {
	USBBASEDEVICE sc_dev;		/* base device */
	usbd_device_handle sc_udev;	/* USB device */

	char	sc_dead;	/* The device is dead -- kill it */

	int	sc_ac_iface;	/* Audio Control interface */
	usbd_interface_handle	sc_ac_ifaceh;

	struct chan sc_chan;

	int	sc_curaltidx;

	int	sc_nullalt;

	int	sc_audio_rev;

	struct as_info *sc_alts;
	int	sc_nalts;
	int	sc_props;

	int	sc_altflags;
#define HAS_8     0x01
#define HAS_16    0x02
#define HAS_8U    0x04
#define HAS_ALAW  0x08
#define HAS_MULAW 0x10

	struct mixerctl *sc_ctls;
	int	sc_nctls;

	device_ptr_t sc_audiodev;
	char	sc_dying;
};

#define UAC_OUTPUT 0
#define UAC_INPUT  1
#define UAC_EQUAL  2

Static usbd_status	uaudio_identify_ac(struct uaudio_softc *sc,
					   usb_config_descriptor_t *cdesc);
Static usbd_status	uaudio_identify_as(struct uaudio_softc *sc,
					   usb_config_descriptor_t *cdesc);
Static usbd_status	uaudio_process_as(struct uaudio_softc *sc,
			    char *buf, int *offsp, int size,
			    usb_interface_descriptor_t *id);

Static void 		uaudio_add_alt(struct uaudio_softc *sc, 
				       struct as_info *ai);

Static usb_interface_descriptor_t *uaudio_find_iface(char *buf,
			    int size, int *offsp, int subtype);

Static void		uaudio_mixer_add_ctl(struct uaudio_softc *sc,
					     struct mixerctl *mp);

#if defined(__NetBSD__) || defined(__OpenBSD__)
Static char 		*uaudio_id_name(struct uaudio_softc *sc,
					usb_descriptor_t **dps, int id);
#endif

Static struct usb_audio_cluster uaudio_get_cluster(int id,
						   usb_descriptor_t **dps);
Static void		uaudio_add_input(struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps);
Static void 		uaudio_add_output(struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps);
Static void		uaudio_add_mixer(struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps);
Static void		uaudio_add_selector(struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps);
Static void		uaudio_add_feature(struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps);
Static void		uaudio_add_processing_updown(struct uaudio_softc *sc,
			         usb_descriptor_t *v, usb_descriptor_t **dps);
Static void		uaudio_add_processing(struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps);
Static void		uaudio_add_extension(struct uaudio_softc *sc,
			    usb_descriptor_t *v, usb_descriptor_t **dps);
Static usbd_status	uaudio_identify(struct uaudio_softc *sc, 
			    usb_config_descriptor_t *cdesc);

Static int 		uaudio_signext(int type, int val);
#if defined(__NetBSD__) || defined(__OpenBSD__)
Static int 		uaudio_value2bsd(struct mixerctl *mc, int val);
#endif
Static int 		uaudio_bsd2value(struct mixerctl *mc, int val);
Static int 		uaudio_get(struct uaudio_softc *sc, int type,
			    int which, int wValue, int wIndex, int len);
#if defined(__NetBSD__) || defined(__OpenBSD__)
Static int		uaudio_ctl_get(struct uaudio_softc *sc, int which,
			    struct mixerctl *mc, int chan);
#endif
Static void		uaudio_set(struct uaudio_softc *sc, int type,
			    int which, int wValue, int wIndex, int l, int v);
Static void 		uaudio_ctl_set(struct uaudio_softc *sc, int which,
			    struct mixerctl *mc, int chan, int val);

Static usbd_status	uaudio_set_speed(struct uaudio_softc *, int, u_int);

Static usbd_status	uaudio_chan_open(struct uaudio_softc *sc,
					 struct chan *ch);
Static void		uaudio_chan_close(struct uaudio_softc *sc,
					  struct chan *ch);
Static usbd_status	uaudio_chan_alloc_buffers(struct uaudio_softc *,
						  struct chan *);
Static void		uaudio_chan_free_buffers(struct uaudio_softc *,
						 struct chan *);

#if defined(__NetBSD__) || defined(__OpenBSD__)
Static void		uaudio_chan_set_param(struct chan *ch,
			    struct audio_params *param, u_char *start, 
			    u_char *end, int blksize);
#endif

Static void		uaudio_chan_ptransfer(struct chan *ch);
Static void		uaudio_chan_pintr(usbd_xfer_handle xfer, 
			    usbd_private_handle priv, usbd_status status);

Static void		uaudio_chan_rtransfer(struct chan *ch);
Static void		uaudio_chan_rintr(usbd_xfer_handle xfer, 
			    usbd_private_handle priv, usbd_status status);

#if defined(__NetBSD__) || defined(__OpenBSD__)
Static int		uaudio_open(void *, int);
Static void		uaudio_close(void *);
Static int		uaudio_drain(void *);
Static int		uaudio_query_encoding(void *, struct audio_encoding *);
Static int		uaudio_set_params(void *, int, int, 
			    struct audio_params *, struct audio_params *);
Static int		uaudio_round_blocksize(void *, int);
Static int		uaudio_trigger_output(void *, void *, void *,
					      int, void (*)(void *), void *,
					      struct audio_params *);
Static int		uaudio_trigger_input (void *, void *, void *,
					      int, void (*)(void *), void *,
					      struct audio_params *);
Static int		uaudio_halt_in_dma(void *);
Static int		uaudio_halt_out_dma(void *);
Static int		uaudio_getdev(void *, struct audio_device *);
Static int		uaudio_mixer_set_port(void *, mixer_ctrl_t *);
Static int		uaudio_mixer_get_port(void *, mixer_ctrl_t *);
Static int		uaudio_query_devinfo(void *, mixer_devinfo_t *);
Static int		uaudio_get_props(void *);

Static struct audio_hw_if uaudio_hw_if = {
	uaudio_open,
	uaudio_close,
	uaudio_drain,
	uaudio_query_encoding,
	uaudio_set_params,
	uaudio_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	uaudio_halt_out_dma,
	uaudio_halt_in_dma,
	NULL,
	uaudio_getdev,
	NULL,
	uaudio_mixer_set_port,
	uaudio_mixer_get_port,
	uaudio_query_devinfo,
	NULL,
	NULL,
	NULL,
	NULL,
	uaudio_get_props,
	uaudio_trigger_output,
	uaudio_trigger_input,
};

Static struct audio_device uaudio_device = {
	"USB audio",
	"",
	"uaudio"
};

#elif defined(__FreeBSD__)
Static int	audio_attach_mi(device_t);
Static void	uaudio_init_params(struct uaudio_softc * sc, struct chan *ch);

/* for NetBSD compatibirity */
#define	AUMODE_PLAY	0x01
#define	AUMODE_RECORD	0x02

#define	AUDIO_PROP_FULLDUPLEX	0x01

#define	AUDIO_ENCODING_ULAW		1
#define	AUDIO_ENCODING_ALAW		2
#define	AUDIO_ENCODING_SLINEAR_LE	6
#define	AUDIO_ENCODING_SLINEAR_BE	7
#define	AUDIO_ENCODING_ULINEAR_LE	8
#define	AUDIO_ENCODING_ULINEAR_BE	9

#endif	/* FreeBSD */


#if defined(__NetBSD__) || defined(__OpenBSD__)

USB_DECLARE_DRIVER(uaudio);

#elif defined(__FreeBSD__)

USB_DECLARE_DRIVER_INIT(uaudio,
		DEVMETHOD(device_suspend, bus_generic_suspend),
		DEVMETHOD(device_resume, bus_generic_resume),
		DEVMETHOD(device_shutdown, bus_generic_shutdown),
		DEVMETHOD(bus_print_child, bus_generic_print_child)
		);
#endif


USB_MATCH(uaudio)
{
	USB_MATCH_START(uaudio, uaa);
	usb_interface_descriptor_t *id;
	
	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	/* Trigger on the control interface. */
	if (id == NULL || 
	    id->bInterfaceClass != UCLASS_AUDIO ||
	    id->bInterfaceSubClass != USUBCLASS_AUDIOCONTROL ||
	    (usbd_get_quirks(uaa->device)->uq_flags & UQ_BAD_AUDIO))
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS_IFACESUBCLASS);
}

USB_ATTACH(uaudio)
{
	USB_ATTACH_START(uaudio, sc, uaa);
	usb_interface_descriptor_t *id;
	usb_config_descriptor_t *cdesc;
	char devinfo[1024];
	usbd_status err;
	int i, j, found;

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;

#if !defined(__FreeBSD__)
	printf(": %s\n", devinfo);
#endif

	sc->sc_udev = uaa->device;

	cdesc = usbd_get_config_descriptor(sc->sc_udev);
	if (cdesc == NULL) {
		printf("%s: failed to get configuration descriptor\n",
		       USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}

	err = uaudio_identify(sc, cdesc);
	if (err) {
		printf("%s: audio descriptors make no sense, error=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_ac_ifaceh = uaa->iface;
	/* Pick up the AS interface. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i] == NULL)
			continue;
		id = usbd_get_interface_descriptor(uaa->ifaces[i]);
		if (id == NULL)
			continue;
		found = 0;
		for (j = 0; j < sc->sc_nalts; j++) {
			if (id->bInterfaceNumber ==
			    sc->sc_alts[j].idesc->bInterfaceNumber) {
				sc->sc_alts[j].ifaceh = uaa->ifaces[i];
				found = 1;
			}
		}
		if (found)
			uaa->ifaces[i] = NULL;
	}

	for (j = 0; j < sc->sc_nalts; j++) {
		if (sc->sc_alts[j].ifaceh == NULL) {
			printf("%s: alt %d missing AS interface(s)\n",
			    USBDEVNAME(sc->sc_dev), j);
			USB_ATTACH_ERROR_RETURN;
		}
	}

	printf("%s: audio rev %d.%02x\n", USBDEVNAME(sc->sc_dev),
	       sc->sc_audio_rev >> 8, sc->sc_audio_rev & 0xff);

	sc->sc_chan.sc = sc;

	if (usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_AU_NO_FRAC)
		sc->sc_chan.nofrac = 1;

#ifndef USB_DEBUG
	if (bootverbose)
#endif
		printf("%s: %d mixer controls\n", USBDEVNAME(sc->sc_dev),
		    sc->sc_nctls);

#if !defined(__FreeBSD__)
	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));
#endif

	DPRINTF(("uaudio_attach: doing audio_attach_mi\n"));
#if defined(__OpenBSD__)
	audio_attach_mi(&uaudio_hw_if, sc, &sc->sc_dev);
#elif defined(__NetBSD__)
	sc->sc_audiodev = audio_attach_mi(&uaudio_hw_if, sc, &sc->sc_dev);
#elif defined(__FreeBSD__)
	sc->sc_dying = 0;
	if (audio_attach_mi(sc->sc_dev)) {
		printf("audio_attach_mi failed\n");
		USB_ATTACH_ERROR_RETURN;
	}
#endif

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uaudio_activate(device_ptr_t self, enum devact act)
{
	struct uaudio_softc *sc = (struct uaudio_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_audiodev != NULL)
			rv = config_deactivate(sc->sc_audiodev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uaudio_detach(device_ptr_t self, int flags)
{
	struct uaudio_softc *sc = (struct uaudio_softc *)self;
	int rv = 0;

	/* Wait for outstanding requests to complete. */
	usbd_delay_ms(sc->sc_udev, UAUDIO_NCHANBUFS * UAUDIO_NFRAMES);

	if (sc->sc_audiodev != NULL)
		rv = config_detach(sc->sc_audiodev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (rv);
}
#elif defined(__FreeBSD__)

USB_DETACH(uaudio)
{
	USB_DETACH_START(uaudio, sc);

	sc->sc_dying = 1;

#if 0 /* XXX */
	/* Wait for outstanding requests to complete. */
	usbd_delay_ms(sc->sc_udev, UAUDIO_NCHANBUFS * UAUDIO_NFRAMES);
#endif

	/* do nothing ? */
	return bus_generic_detach(sc->sc_dev);
}
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uaudio_query_encoding(void *addr, struct audio_encoding *fp)
{
	struct uaudio_softc *sc = addr;
	int flags = sc->sc_altflags;
	int idx;

	if (sc->sc_dying)
		return (EIO);
    
	if (sc->sc_nalts == 0 || flags == 0)
		return (ENXIO);

	idx = fp->index;
	switch (idx) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = flags&HAS_8U ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = flags&HAS_MULAW ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = flags&HAS_ALAW ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = flags&HAS_8 ? 0 : AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
        case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return (0);
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return (EINVAL);
	}
}
#endif

usb_interface_descriptor_t *
uaudio_find_iface(char *buf, int size, int *offsp, int subtype)
{
	usb_interface_descriptor_t *d;

	while (*offsp < size) {
		d = (void *)(buf + *offsp);
		*offsp += d->bLength;
		if (d->bDescriptorType == UDESC_INTERFACE &&
		    d->bInterfaceClass == UCLASS_AUDIO &&
		    d->bInterfaceSubClass == subtype)
			return (d);
	}
	return (NULL);
}

void
uaudio_mixer_add_ctl(struct uaudio_softc *sc, struct mixerctl *mc)
{
	int res;
	size_t len = sizeof(*mc) * (sc->sc_nctls + 1);
	struct mixerctl *nmc = sc->sc_nctls == 0 ?
	  malloc(len, M_USBDEV, M_NOWAIT) :
	  realloc(sc->sc_ctls, len, M_USBDEV, M_NOWAIT);

	if(nmc == NULL){
		printf("uaudio_mixer_add_ctl: no memory\n");
		return;
	}
	sc->sc_ctls = nmc;

	mc->delta = 0;
	if (mc->type != MIX_ON_OFF) {
		/* Determine min and max values. */
		mc->minval = uaudio_signext(mc->type, 
			uaudio_get(sc, GET_MIN, UT_READ_CLASS_INTERFACE, 
				   mc->wValue[0], mc->wIndex, 
				   MIX_SIZE(mc->type)));
		mc->maxval = 1 + uaudio_signext(mc->type, 
			uaudio_get(sc, GET_MAX, UT_READ_CLASS_INTERFACE,
				   mc->wValue[0], mc->wIndex,
				   MIX_SIZE(mc->type)));
		mc->mul = mc->maxval - mc->minval;
		if (mc->mul == 0)
			mc->mul = 1;
		res = uaudio_get(sc, GET_RES, UT_READ_CLASS_INTERFACE,
				 mc->wValue[0], mc->wIndex,
				 MIX_SIZE(mc->type));
		if (res > 0)
			mc->delta = (res * 256 + mc->mul/2) / mc->mul;
	} else {
		mc->minval = 0;
		mc->maxval = 1;
	}

	sc->sc_ctls[sc->sc_nctls++] = *mc;

#ifdef USB_DEBUG
	if (uaudiodebug > 2) {
		int i;
		DPRINTF(("uaudio_mixer_add_ctl: wValue=%04x",mc->wValue[0]));
		for (i = 1; i < mc->nchan; i++)
			DPRINTF((",%04x", mc->wValue[i]));
#if defined(__FreeBSD__)
		DPRINTF((" wIndex=%04x type=%d name='%s' unit='%s' "
			 "min=%d max=%d\n",
			 mc->wIndex, mc->type, mc->ctl,
			 mc->minval, mc->maxval));
#else
		DPRINTF((" wIndex=%04x type=%d ctl='%d' "
			 "min=%d max=%d\n",
			 mc->wIndex, mc->type, mc->ctlname, mc->ctlunit,
			 mc->minval, mc->maxval));
#endif
	}
#endif
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
char *
uaudio_id_name(struct uaudio_softc *sc, usb_descriptor_t **dps, int id)
{
	static char buf[32];
	sprintf(buf, "i%d", id);
	return (buf);
}
#endif

struct usb_audio_cluster
uaudio_get_cluster(int id, usb_descriptor_t **dps)
{
	struct usb_audio_cluster r;
	usb_descriptor_t *dp;
	int i;

	for (i = 0; i < 25; i++) { /* avoid infinite loops */
		dp = dps[id];
		if (dp == 0)
			goto bad;
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
#define p ((struct usb_audio_input_terminal *)dp)
			r.bNrChannels = p->bNrChannels;
			USETW(r.wChannelConfig, UGETW(p->wChannelConfig));
			r.iChannelNames = p->iChannelNames;
#undef p
			return (r);
		case UDESCSUB_AC_OUTPUT:
#define p ((struct usb_audio_output_terminal *)dp)
			id = p->bSourceId;
#undef p
			break;
		case UDESCSUB_AC_MIXER:
#define p ((struct usb_audio_mixer_unit *)dp)
			r = *(struct usb_audio_cluster *)
				&p->baSourceId[p->bNrInPins];
#undef p
			return (r);
		case UDESCSUB_AC_SELECTOR:
			/* XXX This is not really right */
#define p ((struct usb_audio_selector_unit *)dp)
			id = p->baSourceId[0];
#undef p
			break;
		case UDESCSUB_AC_FEATURE:
#define p ((struct usb_audio_feature_unit *)dp)
			id = p->bSourceId;
#undef p
			break;
		case UDESCSUB_AC_PROCESSING:
#define p ((struct usb_audio_processing_unit *)dp)
			r = *(struct usb_audio_cluster *)
				&p->baSourceId[p->bNrInPins];
#undef p
			return (r);
		case UDESCSUB_AC_EXTENSION:
#define p ((struct usb_audio_extension_unit *)dp)
			r = *(struct usb_audio_cluster *)
				&p->baSourceId[p->bNrInPins];
#undef p
			return (r);
		default:
			goto bad;
		}
	}
 bad:
	printf("uaudio_get_cluster: bad data\n");
	memset(&r, 0, sizeof r);
	return (r);

}

void
uaudio_add_input(struct uaudio_softc *sc, usb_descriptor_t *v, 
		 usb_descriptor_t **dps)
{
#ifdef USB_DEBUG
	struct usb_audio_input_terminal *d = 
		(struct usb_audio_input_terminal *)v;

	DPRINTFN(2,("uaudio_add_input: bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bNrChannels=%d wChannelConfig=%d "
		    "iChannelNames=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bNrChannels, UGETW(d->wChannelConfig),
		    d->iChannelNames, d->iTerminal));
#endif
}

void
uaudio_add_output(struct uaudio_softc *sc, usb_descriptor_t *v,
		  usb_descriptor_t **dps)
{
#ifdef USB_DEBUG
	struct usb_audio_output_terminal *d = 
		(struct usb_audio_output_terminal *)v;

	DPRINTFN(2,("uaudio_add_output: bTerminalId=%d wTerminalType=0x%04x "
		    "bAssocTerminal=%d bSourceId=%d iTerminal=%d\n",
		    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
		    d->bSourceId, d->iTerminal));
#endif
}

void
uaudio_add_mixer(struct uaudio_softc *sc, usb_descriptor_t *v,
		 usb_descriptor_t **dps)
{
	struct usb_audio_mixer_unit *d = (struct usb_audio_mixer_unit *)v;
	struct usb_audio_mixer_unit_1 *d1;
	int c, chs, ichs, ochs, i, o, bno, p, mo, mc, k;
	uByte *bm;
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_mixer: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));
	
	/* Compute the number of input channels */
	ichs = 0;
	for (i = 0; i < d->bNrInPins; i++)
		ichs += uaudio_get_cluster(d->baSourceId[i], dps).bNrChannels;

	/* and the number of output channels */
	d1 = (struct usb_audio_mixer_unit_1 *)&d->baSourceId[d->bNrInPins];
	ochs = d1->bNrChannels;
	DPRINTFN(2,("uaudio_add_mixer: ichs=%d ochs=%d\n", ichs, ochs));

	bm = d1->bmControls;
	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
#if !defined(__FreeBSD__)
	mix.class = -1;
#endif
	mix.type = MIX_SIGNED_16;
#if !defined(__FreeBSD__)	/* XXXXX */
	mix.ctlunit = AudioNvolume;
#endif

#define BIT(bno) ((bm[bno / 8] >> (7 - bno % 8)) & 1)
	for (p = i = 0; i < d->bNrInPins; i++) {
		chs = uaudio_get_cluster(d->baSourceId[i], dps).bNrChannels;
		mc = 0;
		for (c = 0; c < chs; c++) {
			mo = 0;
			for (o = 0; o < ochs; o++) {
				bno = (p + c) * ochs + o;
				if (BIT(bno))
					mo++;
			}
			if (mo == 1)
				mc++;
		}
		if (mc == chs && chs <= MIX_MAX_CHAN) {
			k = 0;
			for (c = 0; c < chs; c++)
				for (o = 0; o < ochs; o++) {
					bno = (p + c) * ochs + o;
					if (BIT(bno))
						mix.wValue[k++] = 
							MAKE(p+c+1, o+1);
				}
#if !defined(__FreeBSD__)
			sprintf(mix.ctlname, "mix%d-%s", d->bUnitId,
				uaudio_id_name(sc, dps, d->baSourceId[i]));
#endif
			mix.nchan = chs;
			uaudio_mixer_add_ctl(sc, &mix);
		} else {
			/* XXX */
		}
#undef BIT
		p += chs;
	}

}

void
uaudio_add_selector(struct uaudio_softc *sc, usb_descriptor_t *v,
		    usb_descriptor_t **dps)
{
#ifdef USB_DEBUG
	struct usb_audio_selector_unit *d =
		(struct usb_audio_selector_unit *)v;

	DPRINTFN(2,("uaudio_add_selector: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));
#endif
	printf("uaudio_add_selector: NOT IMPLEMENTED\n");
}

void
uaudio_add_feature(struct uaudio_softc *sc, usb_descriptor_t *v,
		   usb_descriptor_t **dps)
{
	struct usb_audio_feature_unit *d = (struct usb_audio_feature_unit *)v;
	uByte *ctls = d->bmaControls;
	int ctlsize = d->bControlSize;
	int nchan = (d->bLength - 7) / ctlsize;
#if !defined(__FreeBSD__)
	int srcId = d->bSourceId;
#endif
	u_int fumask, mmask, cmask;
	struct mixerctl mix;
	int chan, ctl, i, unit;

#define GET(i) (ctls[(i)*ctlsize] | \
		(ctlsize > 1 ? ctls[(i)*ctlsize+1] << 8 : 0))

	mmask = GET(0);
	/* Figure out what we can control */
	for (cmask = 0, chan = 1; chan < nchan; chan++) {
		DPRINTFN(9,("uaudio_add_feature: chan=%d mask=%x\n",
			    chan, GET(chan)));
		cmask |= GET(chan);
	}

#if !defined(__FreeBSD__)
	DPRINTFN(1,("uaudio_add_feature: bUnitId=%d bSourceId=%d, "
		    "%d channels, mmask=0x%04x, cmask=0x%04x\n", 
		    d->bUnitId, srcId, nchan, mmask, cmask));
#endif

	if (nchan > MIX_MAX_CHAN)
		nchan = MIX_MAX_CHAN;
	unit = d->bUnitId;
	mix.wIndex = MAKE(unit, sc->sc_ac_iface);
	for (ctl = MUTE_CONTROL; ctl < LOUDNESS_CONTROL; ctl++) {
		fumask = FU_MASK(ctl);
		DPRINTFN(4,("uaudio_add_feature: ctl=%d fumask=0x%04x\n",
			    ctl, fumask));
		if (mmask & fumask) {
			mix.nchan = 1;
			mix.wValue[0] = MAKE(ctl, 0);
		} else if (cmask & fumask) {
			mix.nchan = nchan - 1;
			for (i = 1; i < nchan; i++) {
				if (GET(i) & fumask)
					mix.wValue[i-1] = MAKE(ctl, i);
				else
					mix.wValue[i-1] = -1;
			}
		} else {
			continue;
		}
#undef GET

#if !defined(__FreeBSD__)
		mix.class = -1;	/* XXX */
#endif
		switch (ctl) {
		case MUTE_CONTROL:
			mix.type = MIX_ON_OFF;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_NRDEVICES;
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNmute);
			mix.ctlunit = "";
#endif
			break;
		case VOLUME_CONTROL:
			mix.type = MIX_SIGNED_16;
#if defined(__FreeBSD__)
			/* mix.ctl = SOUND_MIXER_VOLUME; */
			mix.ctl = SOUND_MIXER_PCM;
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNmaster);
			mix.ctlunit = AudioNvolume;
#endif
			break;
		case BASS_CONTROL:
			mix.type = MIX_SIGNED_8;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_BASS;
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNbass);
			mix.ctlunit = AudioNbass;
#endif
			break;
		case MID_CONTROL:
			mix.type = MIX_SIGNED_8;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNmid);
			mix.ctlunit = AudioNmid;
#endif
			break;
		case TREBLE_CONTROL:
			mix.type = MIX_SIGNED_8;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_TREBLE;
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNtreble);
			mix.ctlunit = AudioNtreble;
#endif
			break;
		case GRAPHIC_EQUALIZER_CONTROL:
			continue; /* XXX don't add anything */
			break;
		case AGC_CONTROL:
			mix.type = MIX_ON_OFF;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNagc);
			mix.ctlunit = "";
#endif
			break;
		case DELAY_CONTROL:
			mix.type = MIX_UNSIGNED_16;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNdelay);
			mix.ctlunit = "4 ms";
#endif
			break;
		case BASS_BOOST_CONTROL:
			mix.type = MIX_ON_OFF;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNbassboost);
			mix.ctlunit = "";
#endif
			break;
		case LOUDNESS_CONTROL:
			mix.type = MIX_ON_OFF;
#if defined(__FreeBSD__)
			mix.ctl = SOUND_MIXER_LOUD;	/* Is this correct ? */
#else
			sprintf(mix.ctlname, "fea%d-%s-%s", unit,
				uaudio_id_name(sc, dps, srcId), 
				AudioNloudness);
			mix.ctlunit = "";
#endif
			break;
		}
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

void
uaudio_add_processing_updown(struct uaudio_softc *sc, usb_descriptor_t *v,
			     usb_descriptor_t **dps)
{
	struct usb_audio_processing_unit *d = 
	    (struct usb_audio_processing_unit *)v;
	struct usb_audio_processing_unit_1 *d1 =
	    (struct usb_audio_processing_unit_1 *)&d->baSourceId[d->bNrInPins];
	struct usb_audio_processing_unit_updown *ud =
	    (struct usb_audio_processing_unit_updown *)
		&d1->bmControls[d1->bControlSize];
	struct mixerctl mix;
	int i;

	DPRINTFN(2,("uaudio_add_processing_updown: bUnitId=%d bNrModes=%d\n",
		    d->bUnitId, ud->bNrModes));

	if (!(d1->bmControls[0] & UA_PROC_MASK(UD_MODE_SELECT_CONTROL))) {
		DPRINTF(("uaudio_add_processing_updown: no mode select\n"));
		return;
	}

	mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
	mix.nchan = 1;
	mix.wValue[0] = MAKE(UD_MODE_SELECT_CONTROL, 0);
#if !defined(__FreeBSD__)
	mix.class = -1;
#endif
	mix.type = MIX_ON_OFF;	/* XXX */
#if !defined(__FreeBSD__)
	mix.ctlunit = "";
	sprintf(mix.ctlname, "pro%d-mode", d->bUnitId);
#endif

	for (i = 0; i < ud->bNrModes; i++) {
		DPRINTFN(2,("uaudio_add_processing_updown: i=%d bm=0x%x\n",
			    i, UGETW(ud->waModes[i])));
		/* XXX */
	}
	uaudio_mixer_add_ctl(sc, &mix);
}

void
uaudio_add_processing(struct uaudio_softc *sc, usb_descriptor_t *v,
		      usb_descriptor_t **dps)
{
	struct usb_audio_processing_unit *d = 
	    (struct usb_audio_processing_unit *)v;
	struct usb_audio_processing_unit_1 *d1 =
	    (struct usb_audio_processing_unit_1 *)&d->baSourceId[d->bNrInPins];
	int ptype = UGETW(d->wProcessType);
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_processing: wProcessType=%d bUnitId=%d "
		    "bNrInPins=%d\n", ptype, d->bUnitId, d->bNrInPins));

	if (d1->bmControls[0] & UA_PROC_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(XX_ENABLE_CONTROL, 0);
#if !defined(__FreeBSD__)
		mix.class = -1;
#endif
		mix.type = MIX_ON_OFF;
#if !defined(__FreeBSD__)
		mix.ctlunit = "";
		sprintf(mix.ctlname, "pro%d.%d-enable", d->bUnitId, ptype);
#endif
		uaudio_mixer_add_ctl(sc, &mix);
	}

	switch(ptype) {
	case UPDOWNMIX_PROCESS:
		uaudio_add_processing_updown(sc, v, dps);
		break;
	case DOLBY_PROLOGIC_PROCESS:
	case P3D_STEREO_EXTENDER_PROCESS:
	case REVERBATION_PROCESS:
	case CHORUS_PROCESS:
	case DYN_RANGE_COMP_PROCESS:
	default:
#ifdef USB_DEBUG
		printf("uaudio_add_processing: unit %d, type=%d not impl.\n",
		       d->bUnitId, ptype);
#endif
		break;
	}
}

void
uaudio_add_extension(struct uaudio_softc *sc, usb_descriptor_t *v,
		     usb_descriptor_t **dps)
{
	struct usb_audio_extension_unit *d = 
	    (struct usb_audio_extension_unit *)v;
	struct usb_audio_extension_unit_1 *d1 =
	    (struct usb_audio_extension_unit_1 *)&d->baSourceId[d->bNrInPins];
	struct mixerctl mix;

	DPRINTFN(2,("uaudio_add_extension: bUnitId=%d bNrInPins=%d\n",
		    d->bUnitId, d->bNrInPins));

	if (usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_AU_NO_XU)
		return;

	if (d1->bmControls[0] & UA_EXT_ENABLE_MASK) {
		mix.wIndex = MAKE(d->bUnitId, sc->sc_ac_iface);
		mix.nchan = 1;
		mix.wValue[0] = MAKE(UA_EXT_ENABLE, 0);
#if !defined(__FreeBSD__)
		mix.class = -1;
#endif
		mix.type = MIX_ON_OFF;
#if !defined(__FreeBSD__)
		mix.ctlunit = "";
		sprintf(mix.ctlname, "ext%d-enable", d->bUnitId);
#endif
		uaudio_mixer_add_ctl(sc, &mix);
	}
}

usbd_status
uaudio_identify(struct uaudio_softc *sc, usb_config_descriptor_t *cdesc)
{
	usbd_status err;

	err = uaudio_identify_ac(sc, cdesc);
	if (err)
		return (err);
	return (uaudio_identify_as(sc, cdesc));
}

void
uaudio_add_alt(struct uaudio_softc *sc, struct as_info *ai)
{
	size_t len = sizeof(*ai) * (sc->sc_nalts + 1);
	struct as_info *nai = sc->sc_nalts == 0 ?
	  malloc(len, M_USBDEV, M_NOWAIT) :
	  realloc(sc->sc_alts, len, M_USBDEV, M_NOWAIT);

	if (nai == NULL) {
		printf("uaudio_add_alt: no memory\n");
		return;
	}

	sc->sc_alts = nai;
	DPRINTFN(2,("uaudio_add_alt: adding alt=%d, enc=%d\n",
		    ai->alt, ai->encoding));
	sc->sc_alts[sc->sc_nalts++] = *ai;
}

usbd_status
uaudio_process_as(struct uaudio_softc *sc, char *buf, int *offsp,
		  int size, usb_interface_descriptor_t *id)
#define offs (*offsp)
{
	struct usb_audio_streaming_interface_descriptor *asid;
	struct usb_audio_streaming_type1_descriptor *asf1d;
	usb_endpoint_descriptor_audio_t *ed;
	struct usb_audio_streaming_endpoint_descriptor *sed;
	int format, chan, prec, enc;
	int dir, type;
	struct as_info ai;

	asid = (void *)(buf + offs);
	if (asid->bDescriptorType != UDESC_CS_INTERFACE ||
	    asid->bDescriptorSubtype != AS_GENERAL)
		return (USBD_INVAL);
	offs += asid->bLength;
	if (offs > size)
		return (USBD_INVAL);
	asf1d = (void *)(buf + offs);
	if (asf1d->bDescriptorType != UDESC_CS_INTERFACE ||
	    asf1d->bDescriptorSubtype != FORMAT_TYPE)
		return (USBD_INVAL);
	offs += asf1d->bLength;
	if (offs > size)
		return (USBD_INVAL);

	if (asf1d->bFormatType != FORMAT_TYPE_I) {
		printf("%s: ignored setting with type %d format\n",
		       USBDEVNAME(sc->sc_dev), UGETW(asid->wFormatTag));
		return (USBD_NORMAL_COMPLETION);
	}

	ed = (void *)(buf + offs);
	if (ed->bDescriptorType != UDESC_ENDPOINT)
		return (USBD_INVAL);
	DPRINTF(("uaudio_process_as: endpoint bLength=%d bDescriptorType=%d "
		 "bEndpointAddress=%d bmAttributes=0x%x wMaxPacketSize=%d "
		 "bInterval=%d bRefresh=%d bSynchAddress=%d\n",
		 ed->bLength, ed->bDescriptorType, ed->bEndpointAddress,
		 ed->bmAttributes, UGETW(ed->wMaxPacketSize),
		 ed->bInterval, ed->bRefresh, ed->bSynchAddress));
	offs += ed->bLength;
	if (offs > size)
		return (USBD_INVAL);
	if (UE_GET_XFERTYPE(ed->bmAttributes) != UE_ISOCHRONOUS)
		return (USBD_INVAL);

	dir = UE_GET_DIR(ed->bEndpointAddress);
	type = UE_GET_ISO_TYPE(ed->bmAttributes);
	if ((usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_AU_INP_ASYNC) &&
	    dir == UE_DIR_IN && type == UE_ISO_ADAPT)
		type = UE_ISO_ASYNC;

	/* We can't handle endpoints that need a sync pipe yet. */
	if (dir == UE_DIR_IN ? type == UE_ISO_ADAPT : type == UE_ISO_ASYNC) {
		printf("%s: ignored %sput endpoint of type %s\n",
		       USBDEVNAME(sc->sc_dev),
		       dir == UE_DIR_IN ? "in" : "out",
		       dir == UE_DIR_IN ? "adaptive" : "async");
		return (USBD_NORMAL_COMPLETION);
	}
	
	sed = (void *)(buf + offs);
	if (sed->bDescriptorType != UDESC_CS_ENDPOINT ||
	    sed->bDescriptorSubtype != AS_GENERAL)
		return (USBD_INVAL);
	offs += sed->bLength;
	if (offs > size)
		return (USBD_INVAL);
	
	format = UGETW(asid->wFormatTag);
	chan = asf1d->bNrChannels;
	prec = asf1d->bBitResolution;
	if (prec != 8 && prec != 16) {
#ifdef USB_DEBUG
		printf("%s: ignored setting with precision %d\n",
		       USBDEVNAME(sc->sc_dev), prec);
#endif
		return (USBD_NORMAL_COMPLETION);
	}
	switch (format) {
	case UA_FMT_PCM:
		sc->sc_altflags |= prec == 8 ? HAS_8 : HAS_16;
		enc = AUDIO_ENCODING_SLINEAR_LE;
		break;
	case UA_FMT_PCM8:
		enc = AUDIO_ENCODING_ULINEAR_LE;
		sc->sc_altflags |= HAS_8U;
		break;
	case UA_FMT_ALAW:
		enc = AUDIO_ENCODING_ALAW;
		sc->sc_altflags |= HAS_ALAW;
		break;
	case UA_FMT_MULAW:
		enc = AUDIO_ENCODING_ULAW;
		sc->sc_altflags |= HAS_MULAW;
		break;
	default:
		printf("%s: ignored setting with format %d\n",
		       USBDEVNAME(sc->sc_dev), format);
		return (USBD_NORMAL_COMPLETION);
	}
	DPRINTFN(1,("uaudio_identify: alt=%d enc=%d chan=%d prec=%d\n",
		    id->bAlternateSetting, enc, chan, prec));
	ai.alt = id->bAlternateSetting;
	ai.encoding = enc;
	ai.idesc = id;
	ai.edesc = ed;
	ai.asf1desc = asf1d;
	uaudio_add_alt(sc, &ai);
	sc->sc_chan.terminal = asid->bTerminalLink; /* XXX */
	sc->sc_chan.dir |= dir == UE_DIR_OUT ? AUMODE_PLAY : AUMODE_RECORD;
	return (USBD_NORMAL_COMPLETION);
}
#undef offs
	
usbd_status
uaudio_identify_as(struct uaudio_softc *sc, usb_config_descriptor_t *cdesc)
{
	usb_interface_descriptor_t *id;
	usbd_status err;
	char *buf;
	int size, offs;

	size = UGETW(cdesc->wTotalLength);
	buf = (char *)cdesc;

	/* Locate the AudioStreaming interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(buf, size, &offs, USUBCLASS_AUDIOSTREAM);
	if (id == NULL)
		return (USBD_INVAL);

	sc->sc_chan.terminal = -1;
	sc->sc_chan.dir = 0;

	/* Loop through all the alternate settings. */
	while (offs <= size) {
		DPRINTFN(2, ("uaudio_identify: interface %d\n",
		    id->bInterfaceNumber));
		switch (id->bNumEndpoints) {
		case 0:
			DPRINTFN(2, ("uaudio_identify: AS null alt=%d\n",
				     id->bAlternateSetting));
			sc->sc_nullalt = id->bAlternateSetting;
			break;
		case 1:
			err = uaudio_process_as(sc, buf, &offs, size, id);
			break;
		default:
#ifdef USB_DEBUG
			printf("%s: ignored audio interface with %d "
			       "endpoints\n",
			       USBDEVNAME(sc->sc_dev), id->bNumEndpoints);
#endif
			break;
		}
		id = uaudio_find_iface(buf, size, &offs,USUBCLASS_AUDIOSTREAM);
		if (id == NULL)
			break;
	}
	if (offs > size)
		return (USBD_INVAL);
	DPRINTF(("uaudio_identify_as: %d alts available\n", sc->sc_nalts));
	if (sc->sc_chan.terminal < 0) {
		printf("%s: no useable endpoint found\n", 
		       USBDEVNAME(sc->sc_dev));
		return (USBD_INVAL);
	}

#ifndef NO_RECORDING
	if (sc->sc_chan.dir == (AUMODE_PLAY | AUMODE_RECORD))
		sc->sc_props |= AUDIO_PROP_FULLDUPLEX;
#endif
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
uaudio_identify_ac(struct uaudio_softc *sc, usb_config_descriptor_t *cdesc)
{
	usb_interface_descriptor_t *id;
	struct usb_audio_control_descriptor *acdp;
	usb_descriptor_t *dp, *dps[256];
	char *buf, *ibuf, *ibufend;
	int size, offs, aclen, ndps, i;

	size = UGETW(cdesc->wTotalLength);
	buf = (char *)cdesc;

	/* Locate the AudioControl interface descriptor. */
	offs = 0;
	id = uaudio_find_iface(buf, size, &offs, USUBCLASS_AUDIOCONTROL);
	if (id == NULL)
		return (USBD_INVAL);
	if (offs + sizeof *acdp > size)
		return (USBD_INVAL);
	sc->sc_ac_iface = id->bInterfaceNumber;
	DPRINTFN(2,("uaudio_identify: AC interface is %d\n", sc->sc_ac_iface));

	/* A class-specific AC interface header should follow. */
	ibuf = buf + offs;
	acdp = (struct usb_audio_control_descriptor *)ibuf;
	if (acdp->bDescriptorType != UDESC_CS_INTERFACE ||
	    acdp->bDescriptorSubtype != UDESCSUB_AC_HEADER)
		return (USBD_INVAL);
	aclen = UGETW(acdp->wTotalLength);
	if (offs + aclen > size)
		return (USBD_INVAL);

	if (!(usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_BAD_ADC) &&
	     UGETW(acdp->bcdADC) != UAUDIO_VERSION)
		return (USBD_INVAL);

	sc->sc_audio_rev = UGETW(acdp->bcdADC);
	DPRINTFN(2,("uaudio_identify: found AC header, vers=%03x, len=%d\n",
		 sc->sc_audio_rev, aclen));

	sc->sc_nullalt = -1;

	/* Scan through all the AC specific descriptors */
	ibufend = ibuf + aclen;
	dp = (usb_descriptor_t *)ibuf;
	ndps = 0;
	memset(dps, 0, sizeof dps);
	for (;;) {
		ibuf += dp->bLength;
		if (ibuf >= ibufend)
			break;
		dp = (usb_descriptor_t *)ibuf;
		if (ibuf + dp->bLength > ibufend)
			return (USBD_INVAL);
		if (dp->bDescriptorType != UDESC_CS_INTERFACE) {
			printf("uaudio_identify: skip desc type=0x%02x\n",
			       dp->bDescriptorType);
			continue;
		}
		i = ((struct usb_audio_input_terminal *)dp)->bTerminalId;
		dps[i] = dp;
		if (i > ndps)
			ndps = i;
	}
	ndps++;

	for (i = 0; i < ndps; i++) {
		dp = dps[i];
		if (dp == NULL)
			continue;
		DPRINTF(("uaudio_identify: subtype=%d\n", 
			 dp->bDescriptorSubtype));
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_HEADER:
			printf("uaudio_identify: unexpected AC header\n");
			break;
		case UDESCSUB_AC_INPUT:
			uaudio_add_input(sc, dp, dps);
			break;
		case UDESCSUB_AC_OUTPUT:
			uaudio_add_output(sc, dp, dps);
			break;
		case UDESCSUB_AC_MIXER:
			uaudio_add_mixer(sc, dp, dps);
			break;
		case UDESCSUB_AC_SELECTOR:
			uaudio_add_selector(sc, dp, dps);
			break;
		case UDESCSUB_AC_FEATURE:
			uaudio_add_feature(sc, dp, dps);
			break;
		case UDESCSUB_AC_PROCESSING:
			uaudio_add_processing(sc, dp, dps);
			break;
		case UDESCSUB_AC_EXTENSION:
			uaudio_add_extension(sc, dp, dps);
			break;
		default:
			printf("uaudio_identify: bad AC desc subtype=0x%02x\n",
			       dp->bDescriptorSubtype);
			break;
		}
	}
	return (USBD_NORMAL_COMPLETION);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uaudio_query_devinfo(void *addr, mixer_devinfo_t *mi)
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int n, nctls;

	DPRINTFN(2,("uaudio_query_devinfo: index=%d\n", mi->index));
	if (sc->sc_dying)
		return (EIO);
    
	n = mi->index;
	nctls = sc->sc_nctls;

	if (n < 0 || n >= nctls) {
		switch (n - nctls) {
		case UAC_OUTPUT:
			mi->type = AUDIO_MIXER_CLASS;
			mi->mixer_class = nctls + UAC_OUTPUT;
			mi->next = mi->prev = AUDIO_MIXER_LAST;
			strcpy(mi->label.name, AudioCoutputs);
			return (0);
		case UAC_INPUT:
			mi->type = AUDIO_MIXER_CLASS;
			mi->mixer_class = nctls + UAC_INPUT;
			mi->next = mi->prev = AUDIO_MIXER_LAST;
			strcpy(mi->label.name, AudioCinputs);
			return (0);
		case UAC_EQUAL:
			mi->type = AUDIO_MIXER_CLASS;
			mi->mixer_class = nctls + UAC_EQUAL;
			mi->next = mi->prev = AUDIO_MIXER_LAST;
			strcpy(mi->label.name, AudioCequalization);
			return (0);
		default:
			return (ENXIO);
		}
	}
	mc = &sc->sc_ctls[n];
	strncpy(mi->label.name, mc->ctlname, MAX_AUDIO_DEV_LEN);
	mi->mixer_class = mc->class;
	mi->next = mi->prev = AUDIO_MIXER_LAST;	/* XXX */
	switch (mc->type) {
	case MIX_ON_OFF:
		mi->type = AUDIO_MIXER_ENUM;
		mi->un.e.num_mem = 2;
		strcpy(mi->un.e.member[0].label.name, AudioNoff);
		mi->un.e.member[0].ord = 0;
		strcpy(mi->un.e.member[1].label.name, AudioNon);
		mi->un.e.member[1].ord = 1;
		break;
	default:
		mi->type = AUDIO_MIXER_VALUE;
		strncpy(mi->un.v.units.name, mc->ctlunit, MAX_AUDIO_DEV_LEN);
		mi->un.v.num_channels = mc->nchan;
		mi->un.v.delta = mc->delta;
		break;
	}
	return (0);
}

int
uaudio_open(void *addr, int flags)
{
	struct uaudio_softc *sc = addr;

        DPRINTF(("uaudio_open: sc=%p\n", sc));
	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_chan.terminal < 0)
		return (ENXIO);

	if ((flags & FREAD) && !(sc->sc_chan.dir & AUMODE_RECORD))
		return (EACCES);
	if ((flags & FWRITE) && !(sc->sc_chan.dir & AUMODE_PLAY))
		return (EACCES);

        sc->sc_chan.intr = 0;

        return (0);
}

/*
 * Close function is called at splaudio().
 */
void
uaudio_close(void *addr)
{
	struct uaudio_softc *sc = addr;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("uaudio_close: sc=%p\n", sc));
	uaudio_halt_in_dma(sc);
	uaudio_halt_out_dma(sc);

	sc->sc_chan.intr = 0;
}

int
uaudio_drain(void *addr)
{
	struct uaudio_softc *sc = addr;

	if (sc->sc_dying)
		return (EIO);

	usbd_delay_ms(sc->sc_udev, UAUDIO_NCHANBUFS * UAUDIO_NFRAMES);

	return (0);
}

int
uaudio_halt_out_dma(void *addr)
{
	struct uaudio_softc *sc = addr;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("uaudio_halt_out_dma: enter\n"));
	if (sc->sc_chan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_chan);
		sc->sc_chan.pipe = 0;
		uaudio_chan_free_buffers(sc, &sc->sc_chan);
	}
        return (0);
}

int
uaudio_halt_in_dma(void *addr)
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_halt_in_dma: enter\n"));
	if (sc->sc_chan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_chan);
		sc->sc_chan.pipe = 0;
		uaudio_chan_free_buffers(sc, &sc->sc_chan);
	}
        return (0);
}

int
uaudio_getdev(void *addr, struct audio_device *retp)
{
	struct uaudio_softc *sc = addr;

	DPRINTF(("uaudio_mixer_getdev:\n"));
	if (sc->sc_dying)
		return (EIO);
    
	*retp = uaudio_device;
        return (0);
}

/*
 * Make sure the block size is large enough to hold all outstanding transfers.
 */
int
uaudio_round_blocksize(void *addr, int blk)
{
	struct uaudio_softc *sc = addr;
	int bpf;

	if (sc->sc_dying)
		return (EIO);

	bpf = sc->sc_chan.bytes_per_frame + sc->sc_chan.sample_size;
	/* XXX */
	bpf *= UAUDIO_NFRAMES * UAUDIO_NCHANBUFS;

	bpf = (bpf + 15) &~ 15;

	if (blk < bpf)
		blk = bpf;

#ifdef DIAGNOSTIC
	if (blk <= 0) {
		printf("uaudio_round_blocksize: blk=%d\n", blk);
		blk = 512;
	}
#endif

	DPRINTFN(1,("uaudio_round_blocksize: blk=%d\n", blk));
	return (blk);
}

int
uaudio_get_props(void *addr)
{
	struct uaudio_softc *sc = addr;

	return (sc->sc_props);
}
#endif	/* NetBSD or OpenBSD */


int
uaudio_get(struct uaudio_softc *sc, int which, int type, int wValue,
	   int wIndex, int len)
{
	usb_device_request_t req;
	u_int8_t data[4];
	usbd_status err;
	int val;

	if (sc->sc_dying)
		return (EIO);

	if (wValue == -1)
		return (0);

	req.bmRequestType = type;
	req.bRequest = which;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, len);
	DPRINTFN(2,("uaudio_get: type=0x%02x req=0x%02x wValue=0x%04x "
		    "wIndex=0x%04x len=%d\n", 
		    type, which, wValue, wIndex, len));
	err = usbd_do_request(sc->sc_udev, &req, &data);
	if (err) {
		DPRINTF(("uaudio_get: err=%s\n", usbd_errstr(err)));
		return (-1);
	}
	switch (len) {
	case 1:
		val = data[0];
		break;
	case 2:
		val = data[0] | (data[1] << 8);
		break;
	default:
		DPRINTF(("uaudio_get: bad length=%d\n", len));
		return (-1);
	}
	DPRINTFN(2,("uaudio_get: val=%d\n", val));
	return (val);
}

void
uaudio_set(struct uaudio_softc *sc, int which, int type, int wValue,
	   int wIndex, int len, int val)
{
	usb_device_request_t req;
	u_int8_t data[4];
	usbd_status err;

	if (sc->sc_dying)
		return;

	if (wValue == -1)
		return;

	req.bmRequestType = type;
	req.bRequest = which;
	USETW(req.wValue, wValue);
	USETW(req.wIndex, wIndex);
	USETW(req.wLength, len);
	switch (len) {
	case 1:
		data[0] = val;
		break;
	case 2:
		data[0] = val;
		data[1] = val >> 8;
		break;
	default:
		return;
	}
	DPRINTFN(2,("uaudio_set: type=0x%02x req=0x%02x wValue=0x%04x "
		    "wIndex=0x%04x len=%d, val=%d\n", 
		    type, which, wValue, wIndex, len, val & 0xffff));
	err = usbd_do_request(sc->sc_udev, &req, &data);
#ifdef USB_DEBUG
	if (err)
		DPRINTF(("uaudio_set: err=%d\n", err));
#endif
}

int
uaudio_signext(int type, int val)
{
	if (!MIX_UNSIGNED(type)) {
		if (MIX_SIZE(type) == 2)
			val = (int16_t)val;
		else
			val = (int8_t)val;
	}
	return (val);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uaudio_value2bsd(struct mixerctl *mc, int val)
{
	DPRINTFN(5, ("uaudio_value2bsd: type=%03x val=%d min=%d max=%d ",
		     mc->type, val, mc->minval, mc->maxval));
	if (mc->type == MIX_ON_OFF)
		val = val != 0;
	else
		val = ((uaudio_signext(mc->type, val) - mc->minval) * 256
			+ mc->mul/2) / mc->mul;
	DPRINTFN(5, ("val'=%d\n", val));
	return (val);
}
#endif

int
uaudio_bsd2value(struct mixerctl *mc, int val)
{
	DPRINTFN(5,("uaudio_bsd2value: type=%03x val=%d min=%d max=%d ",
		    mc->type, val, mc->minval, mc->maxval));
	if (mc->type == MIX_ON_OFF)
		val = val != 0;
	else
		val = (val + mc->delta/2) * mc->mul / 256 + mc->minval;
	DPRINTFN(5, ("val'=%d\n", val));
	return (val);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uaudio_ctl_get(struct uaudio_softc *sc, int which, struct mixerctl *mc, 
	       int chan)
{
	int val;

	DPRINTFN(5,("uaudio_ctl_get: which=%d chan=%d\n", which, chan));
	val = uaudio_get(sc, which, UT_READ_CLASS_INTERFACE, mc->wValue[chan],
			 mc->wIndex, MIX_SIZE(mc->type));
	return (uaudio_value2bsd(mc, val));
}
#endif

void
uaudio_ctl_set(struct uaudio_softc *sc, int which, struct mixerctl *mc,
	       int chan, int val)
{
	val = uaudio_bsd2value(mc, val);
	uaudio_set(sc, which, UT_WRITE_CLASS_INTERFACE, mc->wValue[chan],
		   mc->wIndex, MIX_SIZE(mc->type), val);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
uaudio_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN], val;

	DPRINTFN(2,("uaudio_mixer_get_port: index=%d\n", cp->dev));

	if (sc->sc_dying)
		return (EIO);
    
	n = cp->dev;
	if (n < 0 || n >= sc->sc_nctls)
		return (ENXIO);
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);
		cp->un.ord = uaudio_ctl_get(sc, GET_CUR, mc, 0);
	} else {
		if (cp->type != AUDIO_MIXER_VALUE)
			return (EINVAL);
		if (cp->un.value.num_channels != 1 &&
		    cp->un.value.num_channels != mc->nchan)
			return (EINVAL);
		for (i = 0; i < mc->nchan; i++)
			vals[i] = uaudio_ctl_get(sc, GET_CUR, mc, i);
		if (cp->un.value.num_channels == 1 && mc->nchan != 1) {
			for (val = 0, i = 0; i < mc->nchan; i++)
				val += vals[i];
			vals[0] = val / mc->nchan;
		}
		for (i = 0; i < cp->un.value.num_channels; i++)
			cp->un.value.level[i] = vals[i];
	}

	return (0);
}
    
int
uaudio_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct uaudio_softc *sc = addr;
	struct mixerctl *mc;
	int i, n, vals[MIX_MAX_CHAN];

	DPRINTFN(2,("uaudio_mixer_set_port: index = %d\n", cp->dev));
	if (sc->sc_dying)
		return (EIO);
    
	n = cp->dev;
	if (n < 0 || n >= sc->sc_nctls)
		return (ENXIO);
	mc = &sc->sc_ctls[n];

	if (mc->type == MIX_ON_OFF) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return (EINVAL);
		uaudio_ctl_set(sc, SET_CUR, mc, 0, cp->un.ord);
	} else {
		if (cp->type != AUDIO_MIXER_VALUE)
			return (EINVAL);
		if (cp->un.value.num_channels == 1)
			for (i = 0; i < mc->nchan; i++)
				vals[i] = cp->un.value.level[0];
		else if (cp->un.value.num_channels == mc->nchan)
			for (i = 0; i < mc->nchan; i++)
				vals[i] = cp->un.value.level[i];
		else
			return (EINVAL);
		for (i = 0; i < mc->nchan; i++)
			uaudio_ctl_set(sc, SET_CUR, mc, i, vals[i]);
	}
	return (0);
}

int
uaudio_trigger_input(void *addr, void *start, void *end, int blksize,
		     void (*intr)(void *), void *arg,
		     struct audio_params *param)
{
	struct uaudio_softc *sc = addr;
	struct chan *ch = &sc->sc_chan;
	usbd_status err;
	int i, s;

	if (sc->sc_dying)
		return (EIO);

	DPRINTFN(3,("uaudio_trigger_input: sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize));

	uaudio_chan_set_param(ch, param, start, end, blksize);
	DPRINTFN(3,("uaudio_trigger_input: sample_size=%d bytes/frame=%d "
		    "fraction=0.%03d\n", ch->sample_size, ch->bytes_per_frame,
		    ch->fraction));

	err = uaudio_chan_alloc_buffers(sc, ch);
	if (err)
		return (EIO);

	err = uaudio_chan_open(sc, ch);
	if (err) {
		uaudio_chan_free_buffers(sc, ch);
		return (EIO);
	}

	sc->sc_chan.intr = intr;
	sc->sc_chan.arg = arg;

	s = splusb();
	for (i = 0; i < UAUDIO_NCHANBUFS-1; i++) /* XXX -1 shouldn't be needed */
		uaudio_chan_rtransfer(ch);
	splx(s);

        return (0);
}
    
int
uaudio_trigger_output(void *addr, void *start, void *end, int blksize,
		      void (*intr)(void *), void *arg,
		      struct audio_params *param)
{
	struct uaudio_softc *sc = addr;
	struct chan *ch = &sc->sc_chan;
	usbd_status err;
	int i, s;

	if (sc->sc_dying)
		return (EIO);

	DPRINTFN(3,("uaudio_trigger_output: sc=%p start=%p end=%p "
		    "blksize=%d\n", sc, start, end, blksize));

	uaudio_chan_set_param(ch, param, start, end, blksize);
	DPRINTFN(3,("uaudio_trigger_output: sample_size=%d bytes/frame=%d "
		    "fraction=0.%03d\n", ch->sample_size, ch->bytes_per_frame,
		    ch->fraction));

	err = uaudio_chan_alloc_buffers(sc, ch);
	if (err)
		return (EIO);

	err = uaudio_chan_open(sc, ch);
	if (err) {
		uaudio_chan_free_buffers(sc, ch);
		return (EIO);
	}

	sc->sc_chan.intr = intr;
	sc->sc_chan.arg = arg;

	s = splusb();
	for (i = 0; i < UAUDIO_NCHANBUFS-1; i++) /* XXX */
		uaudio_chan_ptransfer(ch);
	splx(s);

        return (0);
}
#endif	/* NetBSD or OpenBSD */

/* Set up a pipe for a channel. */
usbd_status
uaudio_chan_open(struct uaudio_softc *sc, struct chan *ch)
{
	struct as_info *as = &sc->sc_alts[sc->sc_curaltidx];
	int endpt = as->edesc->bEndpointAddress;
	usbd_status err;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("uaudio_open_chan: endpt=0x%02x, speed=%d, alt=%d\n", 
		 endpt, ch->sample_rate, as->alt));

	/* Set alternate interface corresponding to the mode. */
	err = usbd_set_interface(as->ifaceh, as->alt);
	if (err)
		return (err);

	/* Some devices do not support this request, so ignore errors. */
#ifdef USB_DEBUG
	err = uaudio_set_speed(sc, endpt, ch->sample_rate);
	if (err)
		DPRINTF(("uaudio_chan_open: set_speed failed err=%s\n",
			 usbd_errstr(err)));
#else
	(void)uaudio_set_speed(sc, endpt, ch->sample_rate);
#endif

	DPRINTF(("uaudio_open_chan: create pipe to 0x%02x\n", endpt));
	err = usbd_open_pipe(as->ifaceh, endpt, 0, &ch->pipe);
	return (err);
}

void
uaudio_chan_close(struct uaudio_softc *sc, struct chan *ch)
{
	struct as_info *as = &sc->sc_alts[sc->sc_curaltidx];

	if (sc->sc_dying)
		return ;

	if (sc->sc_nullalt >= 0) {
		DPRINTF(("uaudio_close_chan: set null alt=%d\n",
			 sc->sc_nullalt));
		usbd_set_interface(as->ifaceh, sc->sc_nullalt);
	}
	usbd_abort_pipe(ch->pipe);
	usbd_close_pipe(ch->pipe);
}

usbd_status
uaudio_chan_alloc_buffers(struct uaudio_softc *sc, struct chan *ch)
{
	usbd_xfer_handle xfer;
	void *buf;
	int i, size;

	size = (ch->bytes_per_frame + ch->sample_size) * UAUDIO_NFRAMES;
	for (i = 0; i < UAUDIO_NCHANBUFS; i++) {
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			goto bad;
		ch->chanbufs[i].xfer = xfer;
		buf = usbd_alloc_buffer(xfer, size);
		if (buf == 0) {
			i++;
			goto bad;
		}
		ch->chanbufs[i].buffer = buf;
		ch->chanbufs[i].chan = ch;
	}

	return (USBD_NORMAL_COMPLETION);

bad:
	while (--i >= 0)
		/* implicit buffer free */
		usbd_free_xfer(ch->chanbufs[i].xfer);
	return (USBD_NOMEM);
}

void
uaudio_chan_free_buffers(struct uaudio_softc *sc, struct chan *ch)
{
	int i;

	for (i = 0; i < UAUDIO_NCHANBUFS; i++)
		usbd_free_xfer(ch->chanbufs[i].xfer);
}

/* Called at splusb() */
void
uaudio_chan_ptransfer(struct chan *ch)
{
	struct chanbuf *cb;
	int i, n, size, residue, total;

	if (ch->sc->sc_dying)
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < UAUDIO_NFRAMES; i++) {
		size = ch->bytes_per_frame;
		residue += ch->fraction;
		if (residue >= USB_FRAMES_PER_SECOND) {
			if (!ch->nofrac)
				size += ch->sample_size;
			residue -= USB_FRAMES_PER_SECOND;
		}
		cb->sizes[i] = size;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

	/* 
	 * Transfer data from upper layer buffer to channel buffer, taking
	 * care of wrapping the upper layer buffer.
	 */
	n = min(total, ch->end - ch->cur);
	memcpy(cb->buffer, ch->cur, n);
	ch->cur += n;
	if (ch->cur >= ch->end)
		ch->cur = ch->start;
	if (total > n) {
		total -= n;
		memcpy(cb->buffer + n, ch->cur, total);
		ch->cur += total;
	}

#ifdef USB_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF(("uaudio_chan_ptransfer: buffer=%p, residue=0.%03d\n",
			 cb->buffer, ch->residue));
		for (i = 0; i < UAUDIO_NFRAMES; i++) {
			DPRINTF(("   [%d] length %d\n", i, cb->sizes[i]));
		}
	}
#endif

	DPRINTFN(5,("uaudio_chan_transfer: ptransfer xfer=%p\n", cb->xfer));
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes, 
			     UAUDIO_NFRAMES, USBD_NO_COPY, 
			     uaudio_chan_pintr);

	(void)usbd_transfer(cb->xfer);
}

void
uaudio_chan_pintr(usbd_xfer_handle xfer, usbd_private_handle priv,
		  usbd_status status)
{
	struct chanbuf *cb = priv;
	struct chan *ch = cb->chan;
	u_int32_t count;
	int s;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("uaudio_chan_pintr: count=%d, transferred=%d\n",
		    count, ch->transferred));
#ifdef DIAGNOSTIC
	if (count != cb->size) {
		printf("uaudio_chan_pintr: count(%d) != size(%d)\n",
		       count, cb->size);
	}
#endif

	ch->transferred += cb->size;
#if defined(__FreeBSD__)
	/* s = spltty(); */
	s = splhigh();
	chn_intr(ch->pcm_ch);
	splx(s);
#else
	s = splaudio();
	/* Call back to upper layer */
	while (ch->transferred >= ch->blksize) {
		ch->transferred -= ch->blksize;
		DPRINTFN(5,("uaudio_chan_pintr: call %p(%p)\n", 
			    ch->intr, ch->arg));
		ch->intr(ch->arg);
	}
	splx(s);
#endif

	/* start next transfer */
	uaudio_chan_ptransfer(ch);
}

/* Called at splusb() */
void
uaudio_chan_rtransfer(struct chan *ch)
{
	struct chanbuf *cb;
	int i, size, residue, total;

	if (ch->sc->sc_dying)
		return;

	/* Pick the next channel buffer. */
	cb = &ch->chanbufs[ch->curchanbuf];
	if (++ch->curchanbuf >= UAUDIO_NCHANBUFS)
		ch->curchanbuf = 0;

	/* Compute the size of each frame in the next transfer. */
	residue = ch->residue;
	total = 0;
	for (i = 0; i < UAUDIO_NFRAMES; i++) {
		size = ch->bytes_per_frame;
		residue += ch->fraction;
		if (residue >= USB_FRAMES_PER_SECOND) {
			if (!ch->nofrac)
				size += ch->sample_size;
			residue -= USB_FRAMES_PER_SECOND;
		}
		cb->sizes[i] = size;
		total += size;
	}
	ch->residue = residue;
	cb->size = total;

#ifdef USB_DEBUG
	if (uaudiodebug > 8) {
		DPRINTF(("uaudio_chan_rtransfer: buffer=%p, residue=0.%03d\n",
			 cb->buffer, ch->residue));
		for (i = 0; i < UAUDIO_NFRAMES; i++) {
			DPRINTF(("   [%d] length %d\n", i, cb->sizes[i]));
		}
	}
#endif

	DPRINTFN(5,("uaudio_chan_rtransfer: transfer xfer=%p\n", cb->xfer));
	/* Fill the request */
	usbd_setup_isoc_xfer(cb->xfer, ch->pipe, cb, cb->sizes, 
			     UAUDIO_NFRAMES, USBD_NO_COPY, 
			     uaudio_chan_rintr);

	(void)usbd_transfer(cb->xfer);
}

void
uaudio_chan_rintr(usbd_xfer_handle xfer, usbd_private_handle priv,
		  usbd_status status)
{
	struct chanbuf *cb = priv;
	struct chan *ch = cb->chan;
	u_int32_t count;
	int s, n;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("uaudio_chan_rintr: count=%d, transferred=%d\n",
		    count, ch->transferred));

	if (count < cb->size) {
		/* if the device fails to keep up, copy last byte */
		u_char b = count ? cb->buffer[count-1] : 0;
		while (count < cb->size)
			cb->buffer[count++] = b;
	}

#ifdef DIAGNOSTIC
	if (count != cb->size) {
		printf("uaudio_chan_rintr: count(%d) != size(%d)\n",
		       count, cb->size);
	}
#endif

	/* 
	 * Transfer data from channel buffer to upper layer buffer, taking
	 * care of wrapping the upper layer buffer.
	 */
	n = min(count, ch->end - ch->cur);
	memcpy(ch->cur, cb->buffer, n);
	ch->cur += n;
	if (ch->cur >= ch->end)
		ch->cur = ch->start;
	if (count > n) {
		memcpy(ch->cur, cb->buffer + n, count - n);
		ch->cur += count - n;
	}

	/* Call back to upper layer */
	ch->transferred += cb->size;
#if defined(__FreeBSD__)
	s = spltty();
	chn_intr(ch->pcm_ch);
	splx(s);
#else
	s = splaudio();
	while (ch->transferred >= ch->blksize) {
		ch->transferred -= ch->blksize;
		DPRINTFN(5,("uaudio_chan_rintr: call %p(%p)\n", 
			    ch->intr, ch->arg));
		ch->intr(ch->arg);
	}
	splx(s);
#endif

	/* start next transfer */
	uaudio_chan_rtransfer(ch);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
void
uaudio_chan_set_param(struct chan *ch, struct audio_params *param,
		      u_char *start, u_char *end, int blksize)
{
	int samples_per_frame, sample_size;

	sample_size = param->precision * param->channels / 8;
	samples_per_frame = param->sample_rate / USB_FRAMES_PER_SECOND;
	ch->fraction = param->sample_rate % USB_FRAMES_PER_SECOND;
	ch->sample_size = sample_size;
	ch->sample_rate = param->sample_rate;
	ch->bytes_per_frame = samples_per_frame * sample_size;
	ch->residue = 0;

	ch->start = start;
	ch->end = end;
	ch->cur = start;
	ch->blksize = blksize;
	ch->transferred = 0;

	ch->curchanbuf = 0;
}

int
uaudio_set_params(void *addr, int setmode, int usemode,
		  struct audio_params *play, struct audio_params *rec)
{
	struct uaudio_softc *sc = addr;
	int flags = sc->sc_altflags;
	int factor;
	int enc, i, j;
	void (*swcode)(void *, u_char *buf, int cnt);
	struct audio_params *p;
	int mode;

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_chan.pipe != NULL)
		return (EBUSY);

	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;
		if ((sc->sc_chan.dir & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;

		factor = 1;
		swcode = 0;
		enc = p->encoding;
		switch (enc) {
		case AUDIO_ENCODING_SLINEAR_BE:
			if (p->precision == 16) {
				swcode = swap_bytes;
				enc = AUDIO_ENCODING_SLINEAR_LE;
			} else if (p->precision == 8 && !(flags & HAS_8)) {
				swcode = change_sign8;
				enc = AUDIO_ENCODING_ULINEAR_LE;
			}
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (p->precision == 8 && !(flags & HAS_8)) {
				swcode = change_sign8;
				enc = AUDIO_ENCODING_ULINEAR_LE;
			}
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (p->precision == 16) {
				if (mode == AUMODE_PLAY)
					swcode = swap_bytes_change_sign16_le;
				else
					swcode = change_sign16_swap_bytes_le;
				enc = AUDIO_ENCODING_SLINEAR_LE;
			} else if (p->precision == 8 && !(flags & HAS_8U)) {
				swcode = change_sign8;
				enc = AUDIO_ENCODING_SLINEAR_LE;
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (p->precision == 16) {
				swcode = change_sign16_le;
				enc = AUDIO_ENCODING_SLINEAR_LE;
			} else if (p->precision == 8 && !(flags & HAS_8U)) {
				swcode = change_sign8;
				enc = AUDIO_ENCODING_SLINEAR_LE;
			}
			break;
		case AUDIO_ENCODING_ULAW:
			if (!(flags & HAS_MULAW)) {
				if (mode == AUMODE_PLAY &&
				    (flags & HAS_16)) {
					swcode = mulaw_to_slinear16_le;
					factor = 2;
					enc = AUDIO_ENCODING_SLINEAR_LE;
				} else if (flags & HAS_8U) {
					if (mode == AUMODE_PLAY)
						swcode = mulaw_to_ulinear8;
					else
						swcode = ulinear8_to_mulaw;
					enc = AUDIO_ENCODING_ULINEAR_LE;
				} else if (flags & HAS_8) {
					if (mode == AUMODE_PLAY)
						swcode = mulaw_to_slinear8;
					else
						swcode = slinear8_to_mulaw;
					enc = AUDIO_ENCODING_SLINEAR_LE;
				} else
					return (EINVAL);
			}
			break;
		case AUDIO_ENCODING_ALAW:
			if (!(flags & HAS_ALAW)) {
				if (mode == AUMODE_PLAY &&
				    (flags & HAS_16)) {
					swcode = alaw_to_slinear16_le;
					factor = 2;
					enc = AUDIO_ENCODING_SLINEAR_LE;
				} else if (flags & HAS_8U) {
					if (mode == AUMODE_PLAY)
						swcode = alaw_to_ulinear8;
					else
						swcode = ulinear8_to_alaw;
					enc = AUDIO_ENCODING_ULINEAR_LE;
				} else if (flags & HAS_8) {
					if (mode == AUMODE_PLAY)
						swcode = alaw_to_slinear8;
					else
						swcode = slinear8_to_alaw;
					enc = AUDIO_ENCODING_SLINEAR_LE;
				} else
					return (EINVAL);
			}
			break;
		default:
			return (EINVAL);
		}
		/* XXX do some other conversions... */

		DPRINTF(("uaudio_set_params: chan=%d prec=%d enc=%d rate=%ld\n",
			 p->channels, p->precision, enc, p->sample_rate));

		for (i = 0; i < sc->sc_nalts; i++) {
			struct usb_audio_streaming_type1_descriptor *a1d =
				sc->sc_alts[i].asf1desc;
			if (p->channels == a1d->bNrChannels &&
			    p->precision == a1d->bBitResolution &&
			    enc == sc->sc_alts[i].encoding &&
			    (mode == AUMODE_PLAY ? UE_DIR_OUT : UE_DIR_IN) ==
			    UE_GET_DIR(sc->sc_alts[i].edesc->bEndpointAddress)) {
				if (a1d->bSamFreqType == UA_SAMP_CONTNUOUS) {
					DPRINTFN(2,("uaudio_set_params: cont %d-%d\n",
					    UA_SAMP_LO(a1d), UA_SAMP_HI(a1d)));
					if (UA_SAMP_LO(a1d) < p->sample_rate &&
					    p->sample_rate < UA_SAMP_HI(a1d))
						goto found;
				} else {
					for (j = 0; j < a1d->bSamFreqType; j++) {
						DPRINTFN(2,("uaudio_set_params: disc #"
						    "%d: %d\n", j, UA_GETSAMP(a1d, j)));
						/* XXX allow for some slack */
						if (UA_GETSAMP(a1d, j) ==
						    p->sample_rate)
							goto found;
					}
				}
			}
		}
		return (EINVAL);

	found:
		p->sw_code = swcode;
		p->factor  = factor;
		if (usemode == mode)
			sc->sc_curaltidx = i;
	}

	DPRINTF(("uaudio_set_params: use altidx=%d, altno=%d\n", 
		 sc->sc_curaltidx, 
		 sc->sc_alts[sc->sc_curaltidx].idesc->bAlternateSetting));
	
	return (0);
}
#endif /* NetBSD or OpenBSD */

usbd_status
uaudio_set_speed(struct uaudio_softc *sc, int endpt, u_int speed)
{
	usb_device_request_t req;
	u_int8_t data[3];

	DPRINTFN(5,("uaudio_set_speed: endpt=%d speed=%u\n", endpt, speed));
	req.bmRequestType = UT_WRITE_CLASS_ENDPOINT;
	req.bRequest = SET_CUR;
	USETW2(req.wValue, SAMPLING_FREQ_CONTROL, 0);
	USETW(req.wIndex, endpt);
	USETW(req.wLength, 3);
	data[0] = speed;
	data[1] = speed >> 8;
	data[2] = speed >> 16;

	return (usbd_do_request(sc->sc_udev, &req, &data));
}


#if defined(__FreeBSD__)
/************************************************************/
void
uaudio_init_params(struct uaudio_softc *sc, struct chan *ch)
{
	int i, j, enc;
	int samples_per_frame, sample_size;

	switch(ch->format & 0x0000FFFF) {
	case AFMT_U8:
		enc = AUDIO_ENCODING_ULINEAR_LE;
		ch->precision = 8;
		break;
	case AFMT_S8:
		enc = AUDIO_ENCODING_SLINEAR_LE;
		ch->precision = 8;
		break;
	case AFMT_A_LAW:	/* ? */
		enc = AUDIO_ENCODING_ALAW;
		ch->precision = 8;
		break;
	case AFMT_MU_LAW:	/* ? */
		enc = AUDIO_ENCODING_ULAW;
		ch->precision = 8;
		break;
	case AFMT_S16_LE:
		enc = AUDIO_ENCODING_SLINEAR_LE;
		ch->precision = 16;
		break;
	case AFMT_S16_BE:
		enc = AUDIO_ENCODING_SLINEAR_BE;
		ch->precision = 16;
		break;
	case AFMT_U16_LE:
		enc = AUDIO_ENCODING_ULINEAR_LE;
		ch->precision = 16;
		break;
	case AFMT_U16_BE:
		enc = AUDIO_ENCODING_ULINEAR_BE;
		ch->precision = 16;
		break;
	default:
		enc = 0;
		ch->precision = 16;
		printf("Unknown format %x\n", ch->format);
	}

	if (ch->format & AFMT_STEREO) {
		ch->channels = 2;
	} else {
		ch->channels = 1;
	}

/*	for (mode =  ......	 */
		for (i = 0; i < sc->sc_nalts; i++) {
			struct usb_audio_streaming_type1_descriptor *a1d =
				sc->sc_alts[i].asf1desc;
			if (ch->channels == a1d->bNrChannels &&
			    ch->precision == a1d->bBitResolution &&
#if 1
			    enc == sc->sc_alts[i].encoding) {
#else
			    enc == sc->sc_alts[i].encoding &&
			    (mode == AUMODE_PLAY ? UE_DIR_OUT : UE_DIR_IN) ==
			    UE_GET_DIR(sc->sc_alts[i].edesc->bEndpointAddress)) {
#endif
				if (a1d->bSamFreqType == UA_SAMP_CONTNUOUS) {
					DPRINTFN(2,("uaudio_set_params: cont %d-%d\n",
					    UA_SAMP_LO(a1d), UA_SAMP_HI(a1d)));
					if (UA_SAMP_LO(a1d) < ch->sample_rate &&
					    ch->sample_rate < UA_SAMP_HI(a1d)) {
						sc->sc_curaltidx = i;
						goto found;
					}
				} else {
					for (j = 0; j < a1d->bSamFreqType; j++) {
						DPRINTFN(2,("uaudio_set_params: disc #"
						    "%d: %d\n", j, UA_GETSAMP(a1d, j)));
						/* XXX allow for some slack */
						if (UA_GETSAMP(a1d, j) ==
						    ch->sample_rate) {
							sc->sc_curaltidx = i;
							goto found;
						}
					}
				}
			}
		}
		/* return (EINVAL); */

	found:
#if 0 /* XXX */
		p->sw_code = swcode;
		p->factor  = factor;
		if (usemode == mode)
			sc->sc_curaltidx = i;
#endif
/*	} */

	sample_size = ch->precision * ch->channels / 8;
	samples_per_frame = ch->sample_rate / USB_FRAMES_PER_SECOND;
	ch->fraction = ch->sample_rate % USB_FRAMES_PER_SECOND;
	ch->sample_size = sample_size;
	ch->bytes_per_frame = samples_per_frame * sample_size;
	ch->residue = 0;

	ch->cur = ch->start;
	ch->transferred = 0;
	ch->curchanbuf = 0;
}

void
uaudio_query_formats(device_t dev, u_int32_t *pfmt, u_int32_t *rfmt)
{
	int i, pn=0, rn=0;
	int prec, dir;
	u_int32_t fmt;
	struct uaudio_softc *sc;

	struct usb_audio_streaming_type1_descriptor *a1d;

	sc = device_get_softc(dev);

	for (i = 0; i < sc->sc_nalts; i++) {
		fmt = 0;
		a1d = sc->sc_alts[i].asf1desc;
		prec = a1d->bBitResolution;	/* precision */

		switch (sc->sc_alts[i].encoding) {
		case AUDIO_ENCODING_ULINEAR_LE:
			if (prec == 8) {
				fmt = AFMT_U8;
			} else if (prec == 16) {
				fmt = AFMT_U16_LE;
			}
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			if (prec == 8) {
				fmt = AFMT_S8;
			} else if (prec == 16) {
				fmt = AFMT_S16_LE;
			}
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (prec == 16) {
				fmt = AFMT_U16_BE;
			}
			break;
		case AUDIO_ENCODING_SLINEAR_BE:
			if (prec == 16) {
				fmt = AFMT_S16_BE;
			}
			break;
		case AUDIO_ENCODING_ALAW:
			if (prec == 8) {
				fmt = AFMT_A_LAW;
			}
			break;
		case AUDIO_ENCODING_ULAW:
			if (prec == 8) {
				fmt = AFMT_MU_LAW;
			}
			break;
		}

		if (fmt != 0) {
			if (a1d->bNrChannels == 2) {	/* stereo/mono */
				fmt |= AFMT_STEREO;
			} else if (a1d->bNrChannels != 1) {
				fmt = 0;
			}
		}

		if (fmt != 0) {
			dir= UE_GET_DIR(sc->sc_alts[i].edesc->bEndpointAddress);
			if (dir == UE_DIR_OUT) {
				pfmt[pn++] = fmt;
			} else if (dir == UE_DIR_IN) {
				rfmt[rn++] = fmt;
			}
		}

		if ((pn > 8*2) || (rn > 8*2))
			break;
	}
	pfmt[pn] = 0;
	rfmt[rn] = 0;
	return;
}

void
uaudio_chan_set_param_pcm_dma_buff(device_t dev, u_char *start, u_char *end,
		struct pcm_channel *pc)
{
	struct uaudio_softc *sc;
	struct chan *ch;

	sc = device_get_softc(dev);
	ch = &sc->sc_chan;

	ch->start = start;
	ch->end = end;

	ch->pcm_ch = pc;

	return;
}

void
uaudio_chan_set_param_blocksize(device_t dev, u_int32_t blocksize)
{
	struct uaudio_softc *sc;
	struct chan *ch;

	sc = device_get_softc(dev);
	ch = &sc->sc_chan;

	ch->blksize = blocksize;

	return;
}

void
uaudio_chan_set_param_speed(device_t dev, u_int32_t speed)
{
	struct uaudio_softc *sc;
	struct chan *ch;

	sc = device_get_softc(dev);
	ch = &sc->sc_chan;

	ch->sample_rate = speed;

	return;
}

int
uaudio_chan_getptr(device_t dev)
{
	struct uaudio_softc *sc;
	struct chan *ch;
	int ptr;

	sc = device_get_softc(dev);
	ch = &sc->sc_chan;

	ptr = ch->cur - ch->start;

	return ptr;
}

void
uaudio_chan_set_param_format(device_t dev, u_int32_t format)
{
	struct uaudio_softc *sc;
	struct chan *ch;

	sc = device_get_softc(dev);
	ch = &sc->sc_chan;

	ch->format = format;

	return;
}

int
uaudio_halt_out_dma(device_t dev)
{
	struct uaudio_softc *sc;

	sc = device_get_softc(dev);

	DPRINTF(("uaudio_halt_out_dma: enter\n"));
	if (sc->sc_chan.pipe != NULL) {
		uaudio_chan_close(sc, &sc->sc_chan);
		sc->sc_chan.pipe = 0;
		uaudio_chan_free_buffers(sc, &sc->sc_chan);
	}
        return (0);
}
    
int
uaudio_trigger_output(device_t dev)
{
	struct uaudio_softc *sc;
	struct chan *ch;
	usbd_status err;
	int i, s;

	sc = device_get_softc(dev);
	ch = &sc->sc_chan;

	if (sc->sc_dying)
		return (EIO);

	uaudio_init_params(sc, ch);

	err = uaudio_chan_alloc_buffers(sc, ch);
	if (err)
		return (EIO);

	err = uaudio_chan_open(sc, ch);
	if (err) {
		uaudio_chan_free_buffers(sc, ch);
		return (EIO);
	}

	s = splusb();
	for (i = 0; i < UAUDIO_NCHANBUFS-1; i++) /* XXX */
		uaudio_chan_ptransfer(ch);
	splx(s);

        return (0);
}

u_int32_t
uaudio_query_mix_info(device_t dev)
{
	int i;
	u_int32_t mask = 0;
	struct uaudio_softc *sc;
	struct mixerctl *mc;

	sc = device_get_softc(dev);
	for (i=0; i < sc->sc_nctls; i++) {
		mc = &sc->sc_ctls[i];
		if (mc->ctl != SOUND_MIXER_NRDEVICES) {
			/* Set device mask bits. 
			   See /usr/include/machine/soundcard.h */
			mask |= (1 << mc->ctl);
		}
	}
	return mask;
}

void
uaudio_mixer_set(device_t dev, unsigned type, unsigned left, unsigned right)
{
	int i;
	struct uaudio_softc *sc;
	struct mixerctl *mc;

	sc = device_get_softc(dev);
	for (i=0; i < sc->sc_nctls; i++) {
		mc = &sc->sc_ctls[i];
		if (mc->ctl == type) {
			if (mc->nchan == 2) {
				/* set Right */
				uaudio_ctl_set(sc, SET_CUR, mc, 1, (int)(right*256)/100);
			}
			/* set Left or Mono */
			uaudio_ctl_set(sc, SET_CUR, mc, 0, (int)(left*256)/100);
		}
	}
	return;
}

Static int
audio_attach_mi(device_t dev)
{
	device_t child;
	struct sndcard_func *func;

	/* Attach the children. */
	/* PCM Audio */
	func = malloc(sizeof(struct sndcard_func), M_DEVBUF, M_NOWAIT);
	if (func == NULL)
		return (ENOMEM);
	bzero(func, sizeof(*func));
	func->func = SCF_PCM;
	child = device_add_child(dev, "pcm", -1);
	device_set_ivars(child, func);

	bus_generic_attach(dev);

	return 0; /* XXXXX */
}

DRIVER_MODULE(uaudio, uhub, uaudio_driver, uaudio_devclass, usbd_driver_load, 0);
MODULE_VERSION(uaudio, 1);

#endif
