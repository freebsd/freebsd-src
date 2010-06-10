/*	$NetBSD: uaudio.c,v 1.91 2004/11/05 17:46:14 kent Exp $	*/
/*	$FreeBSD$ */

/*-
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
 * USB audio specs: http://www.usb.org/developers/devclass_docs/audio10.pdf
 *                  http://www.usb.org/developers/devclass_docs/frmts10.pdf
 *                  http://www.usb.org/developers/devclass_docs/termt10.pdf
 */

/*
 * Also merged:
 *  $NetBSD: uaudio.c,v 1.94 2005/01/15 15:19:53 kent Exp $
 *  $NetBSD: uaudio.c,v 1.95 2005/01/16 06:02:19 dsainty Exp $
 *  $NetBSD: uaudio.c,v 1.96 2005/01/16 12:46:00 kent Exp $
 *  $NetBSD: uaudio.c,v 1.97 2005/02/24 08:19:38 martin Exp $
 */

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define	USB_DEBUG_VAR uaudio_debug
#include <dev/usb/usb_debug.h>

#include <dev/usb/quirk/usb_quirk.h>

#include <sys/reboot.h>			/* for bootverbose */

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/sound/pcm/sound.h>
#include <dev/sound/usb/uaudioreg.h>
#include <dev/sound/usb/uaudio.h>
#include <dev/sound/chip.h>
#include "feeder_if.h"

static int uaudio_default_rate = 0;		/* use rate list */
static int uaudio_default_bits = 32;
static int uaudio_default_channels = 0;		/* use default */

#ifdef USB_DEBUG
static int uaudio_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, uaudio, CTLFLAG_RW, 0, "USB uaudio");

SYSCTL_INT(_hw_usb_uaudio, OID_AUTO, debug, CTLFLAG_RW,
    &uaudio_debug, 0, "uaudio debug level");

TUNABLE_INT("hw.usb.uaudio.default_rate", &uaudio_default_rate);
SYSCTL_INT(_hw_usb_uaudio, OID_AUTO, default_rate, CTLFLAG_RW,
    &uaudio_default_rate, 0, "uaudio default sample rate");

TUNABLE_INT("hw.usb.uaudio.default_bits", &uaudio_default_bits);
SYSCTL_INT(_hw_usb_uaudio, OID_AUTO, default_bits, CTLFLAG_RW,
    &uaudio_default_bits, 0, "uaudio default sample bits");

TUNABLE_INT("hw.usb.uaudio.default_channels", &uaudio_default_channels);
SYSCTL_INT(_hw_usb_uaudio, OID_AUTO, default_channels, CTLFLAG_RW,
    &uaudio_default_channels, 0, "uaudio default sample channels");
#endif

#define	UAUDIO_NFRAMES		64	/* must be factor of 8 due HS-USB */
#define	UAUDIO_NCHANBUFS        2	/* number of outstanding request */
#define	UAUDIO_RECURSE_LIMIT   24	/* rounds */

#define	MAKE_WORD(h,l) (((h) << 8) | (l))
#define	BIT_TEST(bm,bno) (((bm)[(bno) / 8] >> (7 - ((bno) % 8))) & 1)
#define	UAUDIO_MAX_CHAN(x) (x)

struct uaudio_mixer_node {
	int32_t	minval;
	int32_t	maxval;
#define	MIX_MAX_CHAN 8
	int32_t	wValue[MIX_MAX_CHAN];	/* using nchan */
	uint32_t mul;
	uint32_t ctl;

	uint16_t wData[MIX_MAX_CHAN];	/* using nchan */
	uint16_t wIndex;

	uint8_t	update[(MIX_MAX_CHAN + 7) / 8];
	uint8_t	nchan;
	uint8_t	type;
#define	MIX_ON_OFF	1
#define	MIX_SIGNED_16	2
#define	MIX_UNSIGNED_16	3
#define	MIX_SIGNED_8	4
#define	MIX_SELECTOR	5
#define	MIX_UNKNOWN     6
#define	MIX_SIZE(n) ((((n) == MIX_SIGNED_16) || \
		      ((n) == MIX_UNSIGNED_16)) ? 2 : 1)
#define	MIX_UNSIGNED(n) ((n) == MIX_UNSIGNED_16)

#define	MAX_SELECTOR_INPUT_PIN 256
	uint8_t	slctrtype[MAX_SELECTOR_INPUT_PIN];
	uint8_t	class;

	struct uaudio_mixer_node *next;
};

struct uaudio_chan {
	struct pcmchan_caps pcm_cap;	/* capabilities */

	struct snd_dbuf *pcm_buf;
	const struct usb_config *usb_cfg;
	struct mtx *pcm_mtx;		/* lock protecting this structure */
	struct uaudio_softc *priv_sc;
	struct pcm_channel *pcm_ch;
	struct usb_xfer *xfer[UAUDIO_NCHANBUFS];
	const struct usb_audio_streaming_interface_descriptor *p_asid;
	const struct usb_audio_streaming_type1_descriptor *p_asf1d;
	const struct usb_audio_streaming_endpoint_descriptor *p_sed;
	const usb_endpoint_descriptor_audio_t *p_ed1;
	const usb_endpoint_descriptor_audio_t *p_ed2;
	const struct uaudio_format *p_fmt;

	uint8_t *buf;			/* pointer to buffer */
	uint8_t *start;			/* upper layer buffer start */
	uint8_t *end;			/* upper layer buffer end */
	uint8_t *cur;			/* current position in upper layer
					 * buffer */

	uint32_t intr_size;		/* in bytes */
	uint32_t intr_frames;		/* in units */
	uint32_t sample_rate;
	uint32_t frames_per_second;
	uint32_t sample_rem;
	uint32_t sample_curr;

	uint32_t format;
	uint32_t pcm_format[2];

	uint16_t bytes_per_frame[2];

	uint16_t sample_size;

	uint8_t	valid;
	uint8_t	iface_index;
	uint8_t	iface_alt_index;
};

#define	UMIDI_N_TRANSFER    4		/* units */
#define	UMIDI_CABLES_MAX   16		/* units */
#define	UMIDI_BULK_SIZE  1024		/* bytes */

struct umidi_sub_chan {
	struct usb_fifo_sc fifo;
	uint8_t *temp_cmd;
	uint8_t	temp_0[4];
	uint8_t	temp_1[4];
	uint8_t	state;
#define	UMIDI_ST_UNKNOWN   0		/* scan for command */
#define	UMIDI_ST_1PARAM    1
#define	UMIDI_ST_2PARAM_1  2
#define	UMIDI_ST_2PARAM_2  3
#define	UMIDI_ST_SYSEX_0   4
#define	UMIDI_ST_SYSEX_1   5
#define	UMIDI_ST_SYSEX_2   6

	uint8_t	read_open:1;
	uint8_t	write_open:1;
	uint8_t	unused:6;
};

struct umidi_chan {

	struct umidi_sub_chan sub[UMIDI_CABLES_MAX];
	struct mtx mtx;

	struct usb_xfer *xfer[UMIDI_N_TRANSFER];

	uint8_t	iface_index;
	uint8_t	iface_alt_index;

	uint8_t	flags;
#define	UMIDI_FLAG_READ_STALL  0x01
#define	UMIDI_FLAG_WRITE_STALL 0x02

	uint8_t	read_open_refcount;
	uint8_t	write_open_refcount;

	uint8_t	curr_cable;
	uint8_t	max_cable;
	uint8_t	valid;
};

struct uaudio_softc {
	struct sbuf sc_sndstat;
	struct sndcard_func sc_sndcard_func;
	struct uaudio_chan sc_rec_chan;
	struct uaudio_chan sc_play_chan;
	struct umidi_chan sc_midi_chan;

	struct usb_device *sc_udev;
	struct usb_xfer *sc_mixer_xfer[1];
	struct uaudio_mixer_node *sc_mixer_root;
	struct uaudio_mixer_node *sc_mixer_curr;

	uint32_t sc_mix_info;
	uint32_t sc_recsrc_info;

	uint16_t sc_audio_rev;
	uint16_t sc_mixer_count;

	uint8_t	sc_sndstat_valid;
	uint8_t	sc_mixer_iface_index;
	uint8_t	sc_mixer_iface_no;
	uint8_t	sc_mixer_chan;
	uint8_t	sc_pcm_registered:1;
	uint8_t	sc_mixer_init:1;
	uint8_t	sc_uq_audio_swap_lr:1;
	uint8_t	sc_uq_au_inp_async:1;
	uint8_t	sc_uq_au_no_xu:1;
	uint8_t	sc_uq_bad_adc:1;
};

struct uaudio_search_result {
	uint8_t	bit_input[(256 + 7) / 8];
	uint8_t	bit_output[(256 + 7) / 8];
	uint8_t	bit_visited[(256 + 7) / 8];
	uint8_t	recurse_level;
	uint8_t	id_max;
};

struct uaudio_terminal_node {
	union {
		const struct usb_descriptor *desc;
		const struct usb_audio_input_terminal *it;
		const struct usb_audio_output_terminal *ot;
		const struct usb_audio_mixer_unit_0 *mu;
		const struct usb_audio_selector_unit *su;
		const struct usb_audio_feature_unit *fu;
		const struct usb_audio_processing_unit_0 *pu;
		const struct usb_audio_extension_unit_0 *eu;
	}	u;
	struct uaudio_search_result usr;
	struct uaudio_terminal_node *root;
};

struct uaudio_format {
	uint16_t wFormat;
	uint8_t	bPrecision;
	uint32_t freebsd_fmt;
	const char *description;
};

static const struct uaudio_format uaudio_formats[] = {

	{UA_FMT_PCM8, 8, AFMT_U8, "8-bit U-LE PCM"},
	{UA_FMT_PCM8, 16, AFMT_U16_LE, "16-bit U-LE PCM"},
	{UA_FMT_PCM8, 24, AFMT_U24_LE, "24-bit U-LE PCM"},
	{UA_FMT_PCM8, 32, AFMT_U32_LE, "32-bit U-LE PCM"},

	{UA_FMT_PCM, 8, AFMT_S8, "8-bit S-LE PCM"},
	{UA_FMT_PCM, 16, AFMT_S16_LE, "16-bit S-LE PCM"},
	{UA_FMT_PCM, 24, AFMT_S24_LE, "24-bit S-LE PCM"},
	{UA_FMT_PCM, 32, AFMT_S32_LE, "32-bit S-LE PCM"},

	{UA_FMT_ALAW, 8, AFMT_A_LAW, "8-bit A-Law"},
	{UA_FMT_MULAW, 8, AFMT_MU_LAW, "8-bit mu-Law"},

	{0, 0, 0, NULL}
};

#define	UAC_OUTPUT	0
#define	UAC_INPUT	1
#define	UAC_EQUAL	2
#define	UAC_RECORD	3
#define	UAC_NCLASSES	4

#ifdef USB_DEBUG
static const char *uac_names[] = {
	"outputs", "inputs", "equalization", "record"
};

#endif

/* prototypes */

static device_probe_t uaudio_probe;
static device_attach_t uaudio_attach;
static device_detach_t uaudio_detach;

static usb_callback_t uaudio_chan_play_callback;
static usb_callback_t uaudio_chan_record_callback;
static usb_callback_t uaudio_mixer_write_cfg_callback;
static usb_callback_t umidi_read_clear_stall_callback;
static usb_callback_t umidi_bulk_read_callback;
static usb_callback_t umidi_write_clear_stall_callback;
static usb_callback_t umidi_bulk_write_callback;

static void	uaudio_chan_fill_info_sub(struct uaudio_softc *,
		    struct usb_device *, uint32_t, uint8_t, uint8_t);
static void	uaudio_chan_fill_info(struct uaudio_softc *,
		    struct usb_device *);
static void	uaudio_mixer_add_ctl_sub(struct uaudio_softc *,
		    struct uaudio_mixer_node *);
static void	uaudio_mixer_add_ctl(struct uaudio_softc *,
		    struct uaudio_mixer_node *);
static void	uaudio_mixer_add_input(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static void	uaudio_mixer_add_output(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static void	uaudio_mixer_add_mixer(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static void	uaudio_mixer_add_selector(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static uint32_t	uaudio_mixer_feature_get_bmaControls(
		    const struct usb_audio_feature_unit *, uint8_t);
static void	uaudio_mixer_add_feature(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static void	uaudio_mixer_add_processing_updown(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static void	uaudio_mixer_add_processing(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static void	uaudio_mixer_add_extension(struct uaudio_softc *,
		    const struct uaudio_terminal_node *, int);
static struct	usb_audio_cluster uaudio_mixer_get_cluster(uint8_t,
		    const struct uaudio_terminal_node *);
static uint16_t	uaudio_mixer_determine_class(const struct uaudio_terminal_node *,
		    struct uaudio_mixer_node *);
static uint16_t	uaudio_mixer_feature_name(const struct uaudio_terminal_node *,
		    struct uaudio_mixer_node *);
static const struct uaudio_terminal_node *uaudio_mixer_get_input(
		    const struct uaudio_terminal_node *, uint8_t);
static const struct uaudio_terminal_node *uaudio_mixer_get_output(
		    const struct uaudio_terminal_node *, uint8_t);
static void	uaudio_mixer_find_inputs_sub(struct uaudio_terminal_node *,
		    const uint8_t *, uint8_t, struct uaudio_search_result *);
static void	uaudio_mixer_find_outputs_sub(struct uaudio_terminal_node *,
		    uint8_t, uint8_t, struct uaudio_search_result *);
static void	uaudio_mixer_fill_info(struct uaudio_softc *,
		    struct usb_device *, void *);
static uint16_t	uaudio_mixer_get(struct usb_device *, uint8_t,
		    struct uaudio_mixer_node *);
static void	uaudio_mixer_ctl_set(struct uaudio_softc *,
		    struct uaudio_mixer_node *, uint8_t, int32_t val);
static usb_error_t uaudio_set_speed(struct usb_device *, uint8_t, uint32_t);
static int	uaudio_mixer_signext(uint8_t, int);
static int	uaudio_mixer_bsd2value(struct uaudio_mixer_node *, int32_t val);
static const void *uaudio_mixer_verify_desc(const void *, uint32_t);
static void	uaudio_mixer_init(struct uaudio_softc *);
static uint8_t	umidi_convert_to_usb(struct umidi_sub_chan *, uint8_t, uint8_t);
static struct	umidi_sub_chan *umidi_sub_by_fifo(struct usb_fifo *);
static void	umidi_start_read(struct usb_fifo *);
static void	umidi_stop_read(struct usb_fifo *);
static void	umidi_start_write(struct usb_fifo *);
static void	umidi_stop_write(struct usb_fifo *);
static int	umidi_open(struct usb_fifo *, int);
static int	umidi_ioctl(struct usb_fifo *, u_long cmd, void *, int);
static void	umidi_close(struct usb_fifo *, int);
static void	umidi_init(device_t dev);
static int32_t	umidi_probe(device_t dev);
static int32_t	umidi_detach(device_t dev);

#ifdef USB_DEBUG
static void	uaudio_chan_dump_ep_desc(
		    const usb_endpoint_descriptor_audio_t *);
static void	uaudio_mixer_dump_cluster(uint8_t,
		    const struct uaudio_terminal_node *);
static const char *uaudio_mixer_get_terminal_name(uint16_t);
#endif

static const struct usb_config
	uaudio_cfg_record[UAUDIO_NCHANBUFS] = {
	[0] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.frames = UAUDIO_NFRAMES,
		.flags = {.short_xfer_ok = 1,},
		.callback = &uaudio_chan_record_callback,
	},

	[1] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.frames = UAUDIO_NFRAMES,
		.flags = {.short_xfer_ok = 1,},
		.callback = &uaudio_chan_record_callback,
	},
};

static const struct usb_config
	uaudio_cfg_play[UAUDIO_NCHANBUFS] = {
	[0] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.frames = UAUDIO_NFRAMES,
		.flags = {.short_xfer_ok = 1,},
		.callback = &uaudio_chan_play_callback,
	},

	[1] = {
		.type = UE_ISOCHRONOUS,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = 0,	/* use "wMaxPacketSize * frames" */
		.frames = UAUDIO_NFRAMES,
		.flags = {.short_xfer_ok = 1,},
		.callback = &uaudio_chan_play_callback,
	},
};

static const struct usb_config
	uaudio_mixer_config[1] = {
	[0] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = (sizeof(struct usb_device_request) + 4),
		.callback = &uaudio_mixer_write_cfg_callback,
		.timeout = 1000,	/* 1 second */
	},
};

static const
uint8_t	umidi_cmd_to_len[16] = {
	[0x0] = 0,			/* reserved */
	[0x1] = 0,			/* reserved */
	[0x2] = 2,			/* bytes */
	[0x3] = 3,			/* bytes */
	[0x4] = 3,			/* bytes */
	[0x5] = 1,			/* bytes */
	[0x6] = 2,			/* bytes */
	[0x7] = 3,			/* bytes */
	[0x8] = 3,			/* bytes */
	[0x9] = 3,			/* bytes */
	[0xA] = 3,			/* bytes */
	[0xB] = 3,			/* bytes */
	[0xC] = 2,			/* bytes */
	[0xD] = 2,			/* bytes */
	[0xE] = 3,			/* bytes */
	[0xF] = 1,			/* bytes */
};

static const struct usb_config
	umidi_config[UMIDI_N_TRANSFER] = {
	[0] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = UMIDI_BULK_SIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &umidi_bulk_write_callback,
	},

	[1] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = UMIDI_BULK_SIZE,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = &umidi_bulk_read_callback,
	},

	[2] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request),
		.callback = &umidi_write_clear_stall_callback,
		.timeout = 1000,	/* 1 second */
		.interval = 50,	/* 50ms */
	},

	[3] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,	/* Control pipe */
		.direction = UE_DIR_ANY,
		.bufsize = sizeof(struct usb_device_request),
		.callback = &umidi_read_clear_stall_callback,
		.timeout = 1000,	/* 1 second */
		.interval = 50,	/* 50ms */
	},
};

static devclass_t uaudio_devclass;

static device_method_t uaudio_methods[] = {
	DEVMETHOD(device_probe, uaudio_probe),
	DEVMETHOD(device_attach, uaudio_attach),
	DEVMETHOD(device_detach, uaudio_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	{0, 0}
};

static driver_t uaudio_driver = {
	.name = "uaudio",
	.methods = uaudio_methods,
	.size = sizeof(struct uaudio_softc),
};

static int
uaudio_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);

	if (uaa->use_generic == 0)
		return (ENXIO);

	/* trigger on the control interface */

	if ((uaa->info.bInterfaceClass == UICLASS_AUDIO) &&
	    (uaa->info.bInterfaceSubClass == UISUBCLASS_AUDIOCONTROL)) {
		if (usb_test_quirk(uaa, UQ_BAD_AUDIO))
			return (ENXIO);
		else
			return (0);
	}

	/* check for MIDI stream */

	if ((uaa->info.bInterfaceClass == UICLASS_AUDIO) &&
	    (uaa->info.bInterfaceSubClass == UISUBCLASS_MIDISTREAM)) {
		return (0);
	}
	return (ENXIO);
}

static int
uaudio_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct uaudio_softc *sc = device_get_softc(dev);
	struct usb_interface_descriptor *id;
	device_t child;

	sc->sc_play_chan.priv_sc = sc;
	sc->sc_rec_chan.priv_sc = sc;
	sc->sc_udev = uaa->device;
	sc->sc_mixer_iface_index = uaa->info.bIfaceIndex;
	sc->sc_mixer_iface_no = uaa->info.bIfaceNum;

	if (usb_test_quirk(uaa, UQ_AUDIO_SWAP_LR))
		sc->sc_uq_audio_swap_lr = 1;

	if (usb_test_quirk(uaa, UQ_AU_INP_ASYNC))
		sc->sc_uq_au_inp_async = 1;

	if (usb_test_quirk(uaa, UQ_AU_NO_XU))
		sc->sc_uq_au_no_xu = 1;

	if (usb_test_quirk(uaa, UQ_BAD_ADC))
		sc->sc_uq_bad_adc = 1;

	umidi_init(dev);

	device_set_usb_desc(dev);

	id = usbd_get_interface_descriptor(uaa->iface);

	uaudio_chan_fill_info(sc, uaa->device);

	uaudio_mixer_fill_info(sc, uaa->device, id);

	DPRINTF("audio rev %d.%02x\n",
	    sc->sc_audio_rev >> 8,
	    sc->sc_audio_rev & 0xff);

	DPRINTF("%d mixer controls\n",
	    sc->sc_mixer_count);

	if (sc->sc_play_chan.valid) {
		device_printf(dev, "Play: %d Hz, %d ch, %s format\n",
		    sc->sc_play_chan.sample_rate,
		    sc->sc_play_chan.p_asf1d->bNrChannels,
		    sc->sc_play_chan.p_fmt->description);
	} else {
		device_printf(dev, "No playback!\n");
	}

	if (sc->sc_rec_chan.valid) {
		device_printf(dev, "Record: %d Hz, %d ch, %s format\n",
		    sc->sc_rec_chan.sample_rate,
		    sc->sc_rec_chan.p_asf1d->bNrChannels,
		    sc->sc_rec_chan.p_fmt->description);
	} else {
		device_printf(dev, "No recording!\n");
	}

	if (sc->sc_midi_chan.valid) {

		if (umidi_probe(dev)) {
			goto detach;
		}
		device_printf(dev, "MIDI sequencer\n");
	} else {
		device_printf(dev, "No midi sequencer\n");
	}

	DPRINTF("doing child attach\n");

	/* attach the children */

	sc->sc_sndcard_func.func = SCF_PCM;

	child = device_add_child(dev, "pcm", -1);

	if (child == NULL) {
		DPRINTF("out of memory\n");
		goto detach;
	}
	device_set_ivars(child, &sc->sc_sndcard_func);

	if (bus_generic_attach(dev)) {
		DPRINTF("child attach failed\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	uaudio_detach(dev);
	return (ENXIO);
}

static void
uaudio_pcm_setflags(device_t dev, uint32_t flags)
{
	pcm_setflags(dev, pcm_getflags(dev) | flags);
}

int
uaudio_attach_sub(device_t dev, kobj_class_t mixer_class, kobj_class_t chan_class)
{
	struct uaudio_softc *sc = device_get_softc(device_get_parent(dev));
	char status[SND_STATUSLEN];

	uaudio_mixer_init(sc);

	if (sc->sc_uq_audio_swap_lr) {
		DPRINTF("hardware has swapped left and right\n");
		/* uaudio_pcm_setflags(dev, SD_F_PSWAPLR); */
	}
	if (!(sc->sc_mix_info & SOUND_MASK_PCM)) {

		DPRINTF("emulating master volume\n");

		/*
		 * Emulate missing pcm mixer controller
		 * through FEEDER_VOLUME
		 */
		uaudio_pcm_setflags(dev, SD_F_SOFTPCMVOL);
	}
	if (mixer_init(dev, mixer_class, sc)) {
		goto detach;
	}
	sc->sc_mixer_init = 1;

	snprintf(status, sizeof(status), "at ? %s", PCM_KLDSTRING(snd_uaudio));

	if (pcm_register(dev, sc,
	    sc->sc_play_chan.valid ? 1 : 0,
	    sc->sc_rec_chan.valid ? 1 : 0)) {
		goto detach;
	}

	uaudio_pcm_setflags(dev, SD_F_MPSAFE);
	sc->sc_pcm_registered = 1;

	if (sc->sc_play_chan.valid) {
		pcm_addchan(dev, PCMDIR_PLAY, chan_class, sc);
	}
	if (sc->sc_rec_chan.valid) {
		pcm_addchan(dev, PCMDIR_REC, chan_class, sc);
	}
	pcm_setstatus(dev, status);

	return (0);			/* success */

detach:
	uaudio_detach_sub(dev);
	return (ENXIO);
}

int
uaudio_detach_sub(device_t dev)
{
	struct uaudio_softc *sc = device_get_softc(device_get_parent(dev));
	int error = 0;

repeat:
	if (sc->sc_pcm_registered) {
		error = pcm_unregister(dev);
	} else {
		if (sc->sc_mixer_init) {
			error = mixer_uninit(dev);
		}
	}

	if (error) {
		device_printf(dev, "Waiting for sound application to exit!\n");
		usb_pause_mtx(NULL, 2 * hz);
		goto repeat;		/* try again */
	}
	return (0);			/* success */
}

static int
uaudio_detach(device_t dev)
{
	struct uaudio_softc *sc = device_get_softc(dev);

	if (bus_generic_detach(dev)) {
		DPRINTF("detach failed!\n");
	}
	sbuf_delete(&sc->sc_sndstat);
	sc->sc_sndstat_valid = 0;

	umidi_detach(dev);

	return (0);
}

/*========================================================================*
 * AS - Audio Stream - routines
 *========================================================================*/

#ifdef USB_DEBUG
static void
uaudio_chan_dump_ep_desc(const usb_endpoint_descriptor_audio_t *ed)
{
	if (ed) {
		DPRINTF("endpoint=%p bLength=%d bDescriptorType=%d \n"
		    "bEndpointAddress=%d bmAttributes=0x%x \n"
		    "wMaxPacketSize=%d bInterval=%d \n"
		    "bRefresh=%d bSynchAddress=%d\n",
		    ed, ed->bLength, ed->bDescriptorType,
		    ed->bEndpointAddress, ed->bmAttributes,
		    UGETW(ed->wMaxPacketSize), ed->bInterval,
		    ed->bRefresh, ed->bSynchAddress);
	}
}

#endif

static void
uaudio_chan_fill_info_sub(struct uaudio_softc *sc, struct usb_device *udev,
    uint32_t rate, uint8_t channels, uint8_t bit_resolution)
{
	struct usb_descriptor *desc = NULL;
	const struct usb_audio_streaming_interface_descriptor *asid = NULL;
	const struct usb_audio_streaming_type1_descriptor *asf1d = NULL;
	const struct usb_audio_streaming_endpoint_descriptor *sed = NULL;
	const usb_endpoint_descriptor_audio_t *ed1 = NULL;
	const usb_endpoint_descriptor_audio_t *ed2 = NULL;
	struct usb_config_descriptor *cd = usbd_get_config_descriptor(udev);
	struct usb_interface_descriptor *id;
	const struct uaudio_format *p_fmt;
	struct uaudio_chan *chan;
	uint16_t curidx = 0xFFFF;
	uint16_t lastidx = 0xFFFF;
	uint16_t alt_index = 0;
	uint16_t wFormat;
	uint8_t ep_dir;
	uint8_t ep_type;
	uint8_t ep_sync;
	uint8_t bChannels;
	uint8_t bBitResolution;
	uint8_t x;
	uint8_t audio_if = 0;

	while ((desc = usb_desc_foreach(cd, desc))) {

		if ((desc->bDescriptorType == UDESC_INTERFACE) &&
		    (desc->bLength >= sizeof(*id))) {

			id = (void *)desc;

			if (id->bInterfaceNumber != lastidx) {
				lastidx = id->bInterfaceNumber;
				curidx++;
				alt_index = 0;

			} else {
				alt_index++;
			}

			if ((id->bInterfaceClass == UICLASS_AUDIO) &&
			    (id->bInterfaceSubClass == UISUBCLASS_AUDIOSTREAM)) {
				audio_if = 1;
			} else {
				audio_if = 0;
			}

			if ((id->bInterfaceClass == UICLASS_AUDIO) &&
			    (id->bInterfaceSubClass == UISUBCLASS_MIDISTREAM)) {

				/*
				 * XXX could allow multiple MIDI interfaces
				 * XXX
				 */

				if ((sc->sc_midi_chan.valid == 0) &&
				    usbd_get_iface(udev, curidx)) {
					sc->sc_midi_chan.iface_index = curidx;
					sc->sc_midi_chan.iface_alt_index = alt_index;
					sc->sc_midi_chan.valid = 1;
				}
			}
			asid = NULL;
			asf1d = NULL;
			ed1 = NULL;
			ed2 = NULL;
			sed = NULL;
		}
		if ((desc->bDescriptorType == UDESC_CS_INTERFACE) &&
		    (desc->bDescriptorSubtype == AS_GENERAL) &&
		    (desc->bLength >= sizeof(*asid))) {
			if (asid == NULL) {
				asid = (void *)desc;
			}
		}
		if ((desc->bDescriptorType == UDESC_CS_INTERFACE) &&
		    (desc->bDescriptorSubtype == FORMAT_TYPE) &&
		    (desc->bLength >= sizeof(*asf1d))) {
			if (asf1d == NULL) {
				asf1d = (void *)desc;
				if (asf1d->bFormatType != FORMAT_TYPE_I) {
					DPRINTFN(11, "ignored bFormatType = %d\n",
					    asf1d->bFormatType);
					asf1d = NULL;
					continue;
				}
				if (asf1d->bLength < (sizeof(*asf1d) +
				    (asf1d->bSamFreqType == 0) ? 6 :
				    (asf1d->bSamFreqType * 3))) {
					DPRINTFN(11, "'asf1d' descriptor is too short\n");
					asf1d = NULL;
					continue;
				}
			}
		}
		if ((desc->bDescriptorType == UDESC_ENDPOINT) &&
		    (desc->bLength >= sizeof(*ed1))) {
			if (ed1 == NULL) {
				ed1 = (void *)desc;
				if (UE_GET_XFERTYPE(ed1->bmAttributes) != UE_ISOCHRONOUS) {
					ed1 = NULL;
				}
			} else {
				if (ed2 == NULL) {
					ed2 = (void *)desc;
					if (UE_GET_XFERTYPE(ed2->bmAttributes) != UE_ISOCHRONOUS) {
						ed2 = NULL;
						continue;
					}
					if (ed2->bSynchAddress != 0) {
						DPRINTFN(11, "invalid endpoint: bSynchAddress != 0\n");
						ed2 = NULL;
						continue;
					}
					if (ed2->bEndpointAddress != ed1->bSynchAddress) {
						DPRINTFN(11, "invalid endpoint addresses: "
						    "ep[0]->bSynchAddress=0x%x "
						    "ep[1]->bEndpointAddress=0x%x\n",
						    ed1->bSynchAddress,
						    ed2->bEndpointAddress);
						ed2 = NULL;
						continue;
					}
				}
			}
		}
		if ((desc->bDescriptorType == UDESC_CS_ENDPOINT) &&
		    (desc->bDescriptorSubtype == AS_GENERAL) &&
		    (desc->bLength >= sizeof(*sed))) {
			if (sed == NULL) {
				sed = (void *)desc;
			}
		}
		if (audio_if && asid && asf1d && ed1 && sed) {

			ep_dir = UE_GET_DIR(ed1->bEndpointAddress);
			ep_type = UE_GET_ISO_TYPE(ed1->bmAttributes);
			ep_sync = 0;

			if ((sc->sc_uq_au_inp_async) &&
			    (ep_dir == UE_DIR_IN) && (ep_type == UE_ISO_ADAPT)) {
				ep_type = UE_ISO_ASYNC;
			}
			if ((ep_dir == UE_DIR_IN) && (ep_type == UE_ISO_ADAPT)) {
				ep_sync = 1;
			}
			if ((ep_dir != UE_DIR_IN) && (ep_type == UE_ISO_ASYNC)) {
				ep_sync = 1;
			}
			/* Ignore sync endpoint information until further. */
#if 0
			if (ep_sync && (!ed2)) {
				continue;
			}
			/*
			 * we can't handle endpoints that need a sync pipe
			 * yet
			 */

			if (ep_sync) {
				DPRINTF("skipped sync interface\n");
				audio_if = 0;
				continue;
			}
#endif

			wFormat = UGETW(asid->wFormatTag);
			bChannels = UAUDIO_MAX_CHAN(asf1d->bNrChannels);
			bBitResolution = asf1d->bBitResolution;

			if (asf1d->bSamFreqType == 0) {
				DPRINTFN(16, "Sample rate: %d-%dHz\n",
				    UA_SAMP_LO(asf1d), UA_SAMP_HI(asf1d));

				if ((rate >= UA_SAMP_LO(asf1d)) &&
				    (rate <= UA_SAMP_HI(asf1d))) {
					goto found_rate;
				}
			} else {

				for (x = 0; x < asf1d->bSamFreqType; x++) {
					DPRINTFN(16, "Sample rate = %dHz\n",
					    UA_GETSAMP(asf1d, x));

					if (rate == UA_GETSAMP(asf1d, x)) {
						goto found_rate;
					}
				}
			}

			audio_if = 0;
			continue;

	found_rate:

			for (p_fmt = uaudio_formats;
			    p_fmt->wFormat;
			    p_fmt++) {
				if ((p_fmt->wFormat == wFormat) &&
				    (p_fmt->bPrecision == bBitResolution)) {
					goto found_format;
				}
			}

			audio_if = 0;
			continue;

	found_format:

			if ((bChannels == channels) &&
			    (bBitResolution == bit_resolution)) {

				chan = (ep_dir == UE_DIR_IN) ?
				    &sc->sc_rec_chan :
				    &sc->sc_play_chan;

				if ((chan->valid == 0) && usbd_get_iface(udev, curidx)) {

					chan->valid = 1;
#ifdef USB_DEBUG
					uaudio_chan_dump_ep_desc(ed1);
					uaudio_chan_dump_ep_desc(ed2);

					if (sed->bmAttributes & UA_SED_FREQ_CONTROL) {
						DPRINTFN(2, "FREQ_CONTROL\n");
					}
					if (sed->bmAttributes & UA_SED_PITCH_CONTROL) {
						DPRINTFN(2, "PITCH_CONTROL\n");
					}
#endif
					DPRINTF("Sample rate = %dHz, channels = %d, "
					    "bits = %d, format = %s\n", rate, channels,
					    bit_resolution, p_fmt->description);

					chan->sample_rate = rate;
					chan->p_asid = asid;
					chan->p_asf1d = asf1d;
					chan->p_ed1 = ed1;
					chan->p_ed2 = ed2;
					chan->p_fmt = p_fmt;
					chan->p_sed = sed;
					chan->iface_index = curidx;
					chan->iface_alt_index = alt_index;

					if (ep_dir == UE_DIR_IN)
						chan->usb_cfg =
						    uaudio_cfg_record;
					else
						chan->usb_cfg =
						    uaudio_cfg_play;

					chan->sample_size = ((
					    UAUDIO_MAX_CHAN(chan->p_asf1d->bNrChannels) *
					    chan->p_asf1d->bBitResolution) / 8);

					if (sc->sc_sndstat_valid) {
						sbuf_printf(&sc->sc_sndstat, "\n\t"
						    "mode %d.%d:(%s) %dch, %d/%dbit, %s, %dHz",
						    curidx, alt_index,
						    (ep_dir == UE_DIR_IN) ? "input" : "output",
						    asf1d->bNrChannels, asf1d->bBitResolution,
						    asf1d->bSubFrameSize * 8,
						    p_fmt->description, rate);
					}
				}
			}
			audio_if = 0;
			continue;
		}
	}
}

/* This structure defines all the supported rates. */

static const uint32_t uaudio_rate_list[] = {
	96000,
	88000,
	80000,
	72000,
	64000,
	56000,
	48000,
	44100,
	40000,
	32000,
	24000,
	22050,
	16000,
	11025,
	8000,
	0
};

static void
uaudio_chan_fill_info(struct uaudio_softc *sc, struct usb_device *udev)
{
	uint32_t rate = uaudio_default_rate;
	uint8_t z;
	uint8_t bits = uaudio_default_bits;
	uint8_t y;
	uint8_t channels = uaudio_default_channels;
	uint8_t x;

	bits -= (bits % 8);
	if ((bits == 0) || (bits > 32)) {
		/* set a valid value */
		bits = 32;
	}
	if (channels == 0) {
		switch (usbd_get_speed(udev)) {
		case USB_SPEED_LOW:
		case USB_SPEED_FULL:
			/*
			 * Due to high bandwidth usage and problems
			 * with HIGH-speed split transactions we
			 * disable surround setups on FULL-speed USB
			 * by default
			 */
			channels = 2;
			break;
		default:
			channels = 16;
			break;
		}
	} else if (channels > 16) {
		channels = 16;
	}
	if (sbuf_new(&sc->sc_sndstat, NULL, 4096, SBUF_AUTOEXTEND)) {
		sc->sc_sndstat_valid = 1;
	}
	/* try to search for a valid config */

	for (x = channels; x; x--) {
		for (y = bits; y; y -= 8) {

			/* try user defined rate, if any */
			if (rate != 0)
				uaudio_chan_fill_info_sub(sc, udev, rate, x, y);

			/* try find a matching rate, if any */
			for (z = 0; uaudio_rate_list[z]; z++) {
				uaudio_chan_fill_info_sub(sc, udev, uaudio_rate_list[z], x, y);

				if (sc->sc_rec_chan.valid &&
				    sc->sc_play_chan.valid) {
					goto done;
				}
			}
		}
	}

done:
	if (sc->sc_sndstat_valid) {
		sbuf_finish(&sc->sc_sndstat);
	}
}

static void
uaudio_chan_play_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uaudio_chan *ch = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t total;
	uint32_t blockcount;
	uint32_t n;
	uint32_t offset;
	int actlen;
	int sumlen;

	usbd_xfer_status(xfer, &actlen, &sumlen, NULL, NULL);

	if (ch->end == ch->start) {
		DPRINTF("no buffer!\n");
		return;
	}

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
tr_transferred:
		if (actlen < sumlen) {
			DPRINTF("short transfer, "
			    "%d of %d bytes\n", actlen, sumlen);
		}
		chn_intr(ch->pcm_ch);

	case USB_ST_SETUP:
		if (ch->bytes_per_frame[1] > usbd_xfer_max_framelen(xfer)) {
			DPRINTF("bytes per transfer, %d, "
			    "exceeds maximum, %d!\n",
			    ch->bytes_per_frame[1],
			    usbd_xfer_max_framelen(xfer));
			break;
		}

		blockcount = ch->intr_frames;

		/* setup number of frames */
		usbd_xfer_set_frames(xfer, blockcount);

		/* reset total length */
		total = 0;

		/* setup frame lengths */
		for (n = 0; n != blockcount; n++) {
			ch->sample_curr += ch->sample_rem;
			if (ch->sample_curr >= ch->frames_per_second) {
				ch->sample_curr -= ch->frames_per_second;
				usbd_xfer_set_frame_len(xfer, n, ch->bytes_per_frame[1]);
				total += ch->bytes_per_frame[1];
			} else {
				usbd_xfer_set_frame_len(xfer, n, ch->bytes_per_frame[0]);
				total += ch->bytes_per_frame[0];
			}
		}

		DPRINTFN(6, "transfer %d bytes\n", total);

		offset = 0;

		pc = usbd_xfer_get_frame(xfer, 0);
		while (total > 0) {

			n = (ch->end - ch->cur);
			if (n > total) {
				n = total;
			}
			usbd_copy_in(pc, offset, ch->cur, n);

			total -= n;
			ch->cur += n;
			offset += n;

			if (ch->cur >= ch->end) {
				ch->cur = ch->start;
			}
		}

		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		if (error == USB_ERR_CANCELLED) {
			break;
		}
		goto tr_transferred;
	}
}

static void
uaudio_chan_record_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uaudio_chan *ch = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t n;
	uint32_t m;
	uint32_t blockcount;
	uint32_t offset0;
	uint32_t offset1;
	uint32_t mfl;
	int len;
	int actlen;
	int nframes;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, &nframes);
	mfl = usbd_xfer_max_framelen(xfer);

	if (ch->end == ch->start) {
		DPRINTF("no buffer!\n");
		return;
	}

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTFN(6, "transferred %d bytes\n", actlen);

		offset0 = 0;
		pc = usbd_xfer_get_frame(xfer, 0);

		for (n = 0; n != nframes; n++) {

			offset1 = offset0;
			len = usbd_xfer_frame_len(xfer, n);

			while (len > 0) {

				m = (ch->end - ch->cur);

				if (m > len) {
					m = len;
				}
				usbd_copy_out(pc, offset1, ch->cur, m);

				len -= m;
				offset1 += m;
				ch->cur += m;

				if (ch->cur >= ch->end) {
					ch->cur = ch->start;
				}
			}

			offset0 += mfl;
		}

		chn_intr(ch->pcm_ch);

	case USB_ST_SETUP:
tr_setup:
		blockcount = ch->intr_frames;

		usbd_xfer_set_frames(xfer, blockcount);
		for (n = 0; n < blockcount; n++) {
			usbd_xfer_set_frame_len(xfer, n, mfl);
		}

		usbd_transfer_submit(xfer);
		break;

	default:			/* Error */
		if (error == USB_ERR_CANCELLED) {
			break;
		}
		goto tr_setup;
	}
}

void   *
uaudio_chan_init(struct uaudio_softc *sc, struct snd_dbuf *b,
    struct pcm_channel *c, int dir)
{
	struct uaudio_chan *ch = ((dir == PCMDIR_PLAY) ?
	    &sc->sc_play_chan : &sc->sc_rec_chan);
	uint32_t buf_size;
	uint32_t frames;
	uint32_t format;
	uint16_t fps;
	uint8_t endpoint;
	uint8_t blocks;
	uint8_t iface_index;
	uint8_t alt_index;
	uint8_t fps_shift;
	usb_error_t err;

	fps = usbd_get_isoc_fps(sc->sc_udev);

	if (fps < 8000) {
		/* FULL speed USB */
		frames = 8;
	} else {
		/* HIGH speed USB */
		frames = UAUDIO_NFRAMES;
	}

	/* setup play/record format */

	ch->pcm_cap.fmtlist = ch->pcm_format;

	ch->pcm_format[0] = 0;
	ch->pcm_format[1] = 0;

	ch->pcm_cap.minspeed = ch->sample_rate;
	ch->pcm_cap.maxspeed = ch->sample_rate;

	/* setup mutex and PCM channel */

	ch->pcm_ch = c;
	ch->pcm_mtx = c->lock;

	format = ch->p_fmt->freebsd_fmt;

	switch (ch->p_asf1d->bNrChannels) {
	case 2:
		/* stereo */
		format = SND_FORMAT(format, 2, 0);
		break;
	case 1:
		/* mono */
		format = SND_FORMAT(format, 1, 0);
		break;
	default:
		/* surround and more */
		format = feeder_matrix_default_format(
		    SND_FORMAT(format, ch->p_asf1d->bNrChannels, 0));
		break;
	}

	ch->pcm_cap.fmtlist[0] = format;
	ch->pcm_cap.fmtlist[1] = 0;

	/* check if format is not supported */

	if (format == 0) {
		DPRINTF("The selected audio format is not supported\n");
		goto error;
	}

	/* set alternate interface corresponding to the mode */

	endpoint = ch->p_ed1->bEndpointAddress;
	iface_index = ch->iface_index;
	alt_index = ch->iface_alt_index;

	DPRINTF("endpoint=0x%02x, speed=%d, iface=%d alt=%d\n",
	    endpoint, ch->sample_rate, iface_index, alt_index);

	err = usbd_set_alt_interface_index(sc->sc_udev, iface_index, alt_index);
	if (err) {
		DPRINTF("setting of alternate index failed: %s!\n",
		    usbd_errstr(err));
		goto error;
	}
	usbd_set_parent_iface(sc->sc_udev, iface_index, sc->sc_mixer_iface_index);

	/*
	 * If just one sampling rate is supported,
	 * no need to call "uaudio_set_speed()".
	 * Roland SD-90 freezes by a SAMPLING_FREQ_CONTROL request.
	 */
	if (ch->p_asf1d->bSamFreqType != 1) {
		if (uaudio_set_speed(sc->sc_udev, endpoint, ch->sample_rate)) {
			/*
			 * If the endpoint is adaptive setting the speed may
			 * fail.
			 */
			DPRINTF("setting of sample rate failed! (continuing anyway)\n");
		}
	}
	if (usbd_transfer_setup(sc->sc_udev, &iface_index, ch->xfer,
	    ch->usb_cfg, UAUDIO_NCHANBUFS, ch, ch->pcm_mtx)) {
		DPRINTF("could not allocate USB transfers!\n");
		goto error;
	}

	fps_shift = usbd_xfer_get_fps_shift(ch->xfer[0]);

	/* down shift number of frames per second, if any */
	fps >>= fps_shift;
	frames >>= fps_shift;

	/* bytes per frame should not be zero */
	ch->bytes_per_frame[0] = ((ch->sample_rate / fps) * ch->sample_size);
	ch->bytes_per_frame[1] = (((ch->sample_rate + fps - 1) / fps) * ch->sample_size);

	/* setup data rate dithering, if any */
	ch->frames_per_second = fps;
	ch->sample_rem = ch->sample_rate % fps;
	ch->sample_curr = 0;
	ch->frames_per_second = fps;

	/* compute required buffer size */
	buf_size = (ch->bytes_per_frame[1] * frames);

	ch->intr_size = buf_size;
	ch->intr_frames = frames;

	DPRINTF("fps=%d sample_rem=%d\n", fps, ch->sample_rem);

	if (ch->intr_frames == 0) {
		DPRINTF("frame shift is too high!\n");
		goto error;
	}

	/* setup double buffering */
	buf_size *= 2;
	blocks = 2;

	ch->buf = malloc(buf_size, M_DEVBUF, M_WAITOK | M_ZERO);
	if (ch->buf == NULL)
		goto error;
	if (sndbuf_setup(b, ch->buf, buf_size) != 0)
		goto error;
	if (sndbuf_resize(b, blocks, ch->intr_size)) 
		goto error;

	ch->start = ch->buf;
	ch->end = ch->buf + buf_size;
	ch->cur = ch->buf;
	ch->pcm_buf = b;

	if (ch->pcm_mtx == NULL) {
		DPRINTF("ERROR: PCM channels does not have a mutex!\n");
		goto error;
	}

	return (ch);

error:
	uaudio_chan_free(ch);
	return (NULL);
}

int
uaudio_chan_free(struct uaudio_chan *ch)
{
	if (ch->buf != NULL) {
		free(ch->buf, M_DEVBUF);
		ch->buf = NULL;
	}
	usbd_transfer_unsetup(ch->xfer, UAUDIO_NCHANBUFS);

	ch->valid = 0;

	return (0);
}

int
uaudio_chan_set_param_blocksize(struct uaudio_chan *ch, uint32_t blocksize)
{
	return (ch->intr_size);
}

int
uaudio_chan_set_param_fragments(struct uaudio_chan *ch, uint32_t blocksize,
    uint32_t blockcount)
{
	return (1);
}

int
uaudio_chan_set_param_speed(struct uaudio_chan *ch, uint32_t speed)
{
	if (speed != ch->sample_rate) {
		DPRINTF("rate conversion required\n");
	}
	return (ch->sample_rate);
}

int
uaudio_chan_getptr(struct uaudio_chan *ch)
{
	return (ch->cur - ch->start);
}

struct pcmchan_caps *
uaudio_chan_getcaps(struct uaudio_chan *ch)
{
	return (&ch->pcm_cap);
}

static struct pcmchan_matrix uaudio_chan_matrix_swap_2_0 = {
	.id = SND_CHN_MATRIX_DRV,
	.channels = 2,
	.ext = 0,
	.map = {
		/* Right */
		[0] = {
			.type = SND_CHN_T_FR,
			.members =
			    SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FC |
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BR |
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SR
		},
		/* Left */
		[1] = {
			.type = SND_CHN_T_FL,
			.members =
			    SND_CHN_T_MASK_FL | SND_CHN_T_MASK_FC |
			    SND_CHN_T_MASK_LF | SND_CHN_T_MASK_BL |
			    SND_CHN_T_MASK_BC | SND_CHN_T_MASK_SL
		},
		[2] = {
			.type = SND_CHN_T_MAX,
			.members = 0
		}
	},
	.mask = SND_CHN_T_MASK_FR | SND_CHN_T_MASK_FL,
	.offset = {  1,  0, -1, -1, -1, -1, -1, -1, -1,
		    -1, -1, -1, -1, -1, -1, -1, -1, -1  }
};

struct pcmchan_matrix *
uaudio_chan_getmatrix(struct uaudio_chan *ch, uint32_t format)
{
	struct uaudio_softc *sc;

	sc = ch->priv_sc;

	if (sc != NULL && sc->sc_uq_audio_swap_lr != 0 &&
	    AFMT_CHANNEL(format) == 2)
		return (&uaudio_chan_matrix_swap_2_0);

	return (feeder_matrix_format_map(format));
}

int
uaudio_chan_set_param_format(struct uaudio_chan *ch, uint32_t format)
{
	ch->format = format;
	return (0);
}

int
uaudio_chan_start(struct uaudio_chan *ch)
{
	ch->cur = ch->start;

#if (UAUDIO_NCHANBUFS != 2)
#error "please update code"
#endif
	if (ch->xfer[0]) {
		usbd_transfer_start(ch->xfer[0]);
	}
	if (ch->xfer[1]) {
		usbd_transfer_start(ch->xfer[1]);
	}
	return (0);
}

int
uaudio_chan_stop(struct uaudio_chan *ch)
{
#if (UAUDIO_NCHANBUFS != 2)
#error "please update code"
#endif
	usbd_transfer_stop(ch->xfer[0]);
	usbd_transfer_stop(ch->xfer[1]);
	return (0);
}

/*========================================================================*
 * AC - Audio Controller - routines
 *========================================================================*/

static void
uaudio_mixer_add_ctl_sub(struct uaudio_softc *sc, struct uaudio_mixer_node *mc)
{
	struct uaudio_mixer_node *p_mc_new =
	malloc(sizeof(*p_mc_new), M_USBDEV, M_WAITOK);

	if (p_mc_new) {
		bcopy(mc, p_mc_new, sizeof(*p_mc_new));
		p_mc_new->next = sc->sc_mixer_root;
		sc->sc_mixer_root = p_mc_new;
		sc->sc_mixer_count++;
	} else {
		DPRINTF("out of memory\n");
	}
}

static void
uaudio_mixer_add_ctl(struct uaudio_softc *sc, struct uaudio_mixer_node *mc)
{
	int32_t res;

	if (mc->class < UAC_NCLASSES) {
		DPRINTF("adding %s.%d\n",
		    uac_names[mc->class], mc->ctl);
	} else {
		DPRINTF("adding %d\n", mc->ctl);
	}

	if (mc->type == MIX_ON_OFF) {
		mc->minval = 0;
		mc->maxval = 1;
	} else if (mc->type == MIX_SELECTOR) {
	} else {

		/* determine min and max values */

		mc->minval = uaudio_mixer_get(sc->sc_udev, GET_MIN, mc);

		mc->minval = uaudio_mixer_signext(mc->type, mc->minval);

		mc->maxval = uaudio_mixer_get(sc->sc_udev, GET_MAX, mc);

		mc->maxval = uaudio_mixer_signext(mc->type, mc->maxval);

		/* check if max and min was swapped */

		if (mc->maxval < mc->minval) {
			res = mc->maxval;
			mc->maxval = mc->minval;
			mc->minval = res;
		}

		/* compute value range */
		mc->mul = mc->maxval - mc->minval;
		if (mc->mul == 0)
			mc->mul = 1;

		/* compute value alignment */
		res = uaudio_mixer_get(sc->sc_udev, GET_RES, mc);

		DPRINTF("Resolution = %d\n", (int)res);
	}

	uaudio_mixer_add_ctl_sub(sc, mc);

#ifdef USB_DEBUG
	if (uaudio_debug > 2) {
		uint8_t i;

		for (i = 0; i < mc->nchan; i++) {
			DPRINTF("[mix] wValue=%04x\n", mc->wValue[0]);
		}
		DPRINTF("[mix] wIndex=%04x type=%d ctl='%d' "
		    "min=%d max=%d\n",
		    mc->wIndex, mc->type, mc->ctl,
		    mc->minval, mc->maxval);
	}
#endif
}

static void
uaudio_mixer_add_input(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
#ifdef USB_DEBUG
	const struct usb_audio_input_terminal *d = iot[id].u.it;

	DPRINTFN(3, "bTerminalId=%d wTerminalType=0x%04x "
	    "bAssocTerminal=%d bNrChannels=%d wChannelConfig=%d "
	    "iChannelNames=%d\n",
	    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
	    d->bNrChannels, UGETW(d->wChannelConfig),
	    d->iChannelNames);
#endif
}

static void
uaudio_mixer_add_output(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
#ifdef USB_DEBUG
	const struct usb_audio_output_terminal *d = iot[id].u.ot;

	DPRINTFN(3, "bTerminalId=%d wTerminalType=0x%04x "
	    "bAssocTerminal=%d bSourceId=%d iTerminal=%d\n",
	    d->bTerminalId, UGETW(d->wTerminalType), d->bAssocTerminal,
	    d->bSourceId, d->iTerminal);
#endif
}

static void
uaudio_mixer_add_mixer(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
	struct uaudio_mixer_node mix;

	const struct usb_audio_mixer_unit_0 *d0 = iot[id].u.mu;
	const struct usb_audio_mixer_unit_1 *d1;

	uint32_t bno;			/* bit number */
	uint32_t p;			/* bit number accumulator */
	uint32_t mo;			/* matching outputs */
	uint32_t mc;			/* matching channels */
	uint32_t ichs;			/* input channels */
	uint32_t ochs;			/* output channels */
	uint32_t c;
	uint32_t chs;			/* channels */
	uint32_t i;
	uint32_t o;

	DPRINTFN(3, "bUnitId=%d bNrInPins=%d\n",
	    d0->bUnitId, d0->bNrInPins);

	/* compute the number of input channels */

	ichs = 0;
	for (i = 0; i < d0->bNrInPins; i++) {
		ichs += (uaudio_mixer_get_cluster(d0->baSourceId[i], iot)
		    .bNrChannels);
	}

	d1 = (const void *)(d0->baSourceId + d0->bNrInPins);

	/* and the number of output channels */

	ochs = d1->bNrChannels;

	DPRINTFN(3, "ichs=%d ochs=%d\n", ichs, ochs);

	bzero(&mix, sizeof(mix));

	mix.wIndex = MAKE_WORD(d0->bUnitId, sc->sc_mixer_iface_no);
	uaudio_mixer_determine_class(&iot[id], &mix);
	mix.type = MIX_SIGNED_16;

	if (uaudio_mixer_verify_desc(d0, ((ichs * ochs) + 7) / 8) == NULL) {
		return;
	}
	for (p = i = 0; i < d0->bNrInPins; i++) {
		chs = uaudio_mixer_get_cluster(d0->baSourceId[i], iot).bNrChannels;
		mc = 0;
		for (c = 0; c < chs; c++) {
			mo = 0;
			for (o = 0; o < ochs; o++) {
				bno = ((p + c) * ochs) + o;
				if (BIT_TEST(d1->bmControls, bno)) {
					mo++;
				}
			}
			if (mo == 1) {
				mc++;
			}
		}
		if ((mc == chs) && (chs <= MIX_MAX_CHAN)) {

			/* repeat bit-scan */

			mc = 0;
			for (c = 0; c < chs; c++) {
				for (o = 0; o < ochs; o++) {
					bno = ((p + c) * ochs) + o;
					if (BIT_TEST(d1->bmControls, bno)) {
						mix.wValue[mc++] = MAKE_WORD(p + c + 1, o + 1);
					}
				}
			}
			mix.nchan = chs;
			uaudio_mixer_add_ctl(sc, &mix);
		} else {
			/* XXX */
		}
		p += chs;
	}
}

static void
uaudio_mixer_add_selector(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
	const struct usb_audio_selector_unit *d = iot[id].u.su;
	struct uaudio_mixer_node mix;
	uint16_t i;

	DPRINTFN(3, "bUnitId=%d bNrInPins=%d\n",
	    d->bUnitId, d->bNrInPins);

	if (d->bNrInPins == 0) {
		return;
	}
	bzero(&mix, sizeof(mix));

	mix.wIndex = MAKE_WORD(d->bUnitId, sc->sc_mixer_iface_no);
	mix.wValue[0] = MAKE_WORD(0, 0);
	uaudio_mixer_determine_class(&iot[id], &mix);
	mix.nchan = 1;
	mix.type = MIX_SELECTOR;

	mix.ctl = SOUND_MIXER_NRDEVICES;
	mix.minval = 1;
	mix.maxval = d->bNrInPins;

	if (mix.maxval > MAX_SELECTOR_INPUT_PIN) {
		mix.maxval = MAX_SELECTOR_INPUT_PIN;
	}
	mix.mul = (mix.maxval - mix.minval);
	for (i = 0; i < MAX_SELECTOR_INPUT_PIN; i++) {
		mix.slctrtype[i] = SOUND_MIXER_NRDEVICES;
	}

	for (i = 0; i < mix.maxval; i++) {
		mix.slctrtype[i] = uaudio_mixer_feature_name
		    (&iot[d->baSourceId[i]], &mix);
	}

	mix.class = 0;			/* not used */

	uaudio_mixer_add_ctl(sc, &mix);
}

static uint32_t
uaudio_mixer_feature_get_bmaControls(const struct usb_audio_feature_unit *d,
    uint8_t index)
{
	uint32_t temp = 0;
	uint32_t offset = (index * d->bControlSize);

	if (d->bControlSize > 0) {
		temp |= d->bmaControls[offset];
		if (d->bControlSize > 1) {
			temp |= d->bmaControls[offset + 1] << 8;
			if (d->bControlSize > 2) {
				temp |= d->bmaControls[offset + 2] << 16;
				if (d->bControlSize > 3) {
					temp |= d->bmaControls[offset + 3] << 24;
				}
			}
		}
	}
	return (temp);
}

static void
uaudio_mixer_add_feature(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
	const struct usb_audio_feature_unit *d = iot[id].u.fu;
	struct uaudio_mixer_node mix;
	uint32_t fumask;
	uint32_t mmask;
	uint32_t cmask;
	uint16_t mixernumber;
	uint8_t nchan;
	uint8_t chan;
	uint8_t ctl;
	uint8_t i;

	if (d->bControlSize == 0) {
		return;
	}
	bzero(&mix, sizeof(mix));

	nchan = (d->bLength - 7) / d->bControlSize;
	mmask = uaudio_mixer_feature_get_bmaControls(d, 0);
	cmask = 0;

	if (nchan == 0) {
		return;
	}
	/* figure out what we can control */

	for (chan = 1; chan < nchan; chan++) {
		DPRINTFN(10, "chan=%d mask=%x\n",
		    chan, uaudio_mixer_feature_get_bmaControls(d, chan));

		cmask |= uaudio_mixer_feature_get_bmaControls(d, chan);
	}

	if (nchan > MIX_MAX_CHAN) {
		nchan = MIX_MAX_CHAN;
	}
	mix.wIndex = MAKE_WORD(d->bUnitId, sc->sc_mixer_iface_no);

	for (ctl = 1; ctl <= LOUDNESS_CONTROL; ctl++) {

		fumask = FU_MASK(ctl);

		DPRINTFN(5, "ctl=%d fumask=0x%04x\n",
		    ctl, fumask);

		if (mmask & fumask) {
			mix.nchan = 1;
			mix.wValue[0] = MAKE_WORD(ctl, 0);
		} else if (cmask & fumask) {
			mix.nchan = nchan - 1;
			for (i = 1; i < nchan; i++) {
				if (uaudio_mixer_feature_get_bmaControls(d, i) & fumask)
					mix.wValue[i - 1] = MAKE_WORD(ctl, i);
				else
					mix.wValue[i - 1] = -1;
			}
		} else {
			continue;
		}

		mixernumber = uaudio_mixer_feature_name(&iot[id], &mix);

		switch (ctl) {
		case MUTE_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctl = SOUND_MIXER_NRDEVICES;
			break;

		case VOLUME_CONTROL:
			mix.type = MIX_SIGNED_16;
			mix.ctl = mixernumber;
			break;

		case BASS_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctl = SOUND_MIXER_BASS;
			break;

		case MID_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
			break;

		case TREBLE_CONTROL:
			mix.type = MIX_SIGNED_8;
			mix.ctl = SOUND_MIXER_TREBLE;
			break;

		case GRAPHIC_EQUALIZER_CONTROL:
			continue;	/* XXX don't add anything */
			break;

		case AGC_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
			break;

		case DELAY_CONTROL:
			mix.type = MIX_UNSIGNED_16;
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
			break;

		case BASS_BOOST_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctl = SOUND_MIXER_NRDEVICES;	/* XXXXX */
			break;

		case LOUDNESS_CONTROL:
			mix.type = MIX_ON_OFF;
			mix.ctl = SOUND_MIXER_LOUD;	/* Is this correct ? */
			break;

		default:
			mix.type = MIX_UNKNOWN;
			break;
		}

		if (mix.type != MIX_UNKNOWN) {
			uaudio_mixer_add_ctl(sc, &mix);
		}
	}
}

static void
uaudio_mixer_add_processing_updown(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
	const struct usb_audio_processing_unit_0 *d0 = iot[id].u.pu;
	const struct usb_audio_processing_unit_1 *d1 =
	(const void *)(d0->baSourceId + d0->bNrInPins);
	const struct usb_audio_processing_unit_updown *ud =
	(const void *)(d1->bmControls + d1->bControlSize);
	struct uaudio_mixer_node mix;
	uint8_t i;

	if (uaudio_mixer_verify_desc(d0, sizeof(*ud)) == NULL) {
		return;
	}
	if (uaudio_mixer_verify_desc(d0, sizeof(*ud) + (2 * ud->bNrModes))
	    == NULL) {
		return;
	}
	DPRINTFN(3, "bUnitId=%d bNrModes=%d\n",
	    d0->bUnitId, ud->bNrModes);

	if (!(d1->bmControls[0] & UA_PROC_MASK(UD_MODE_SELECT_CONTROL))) {
		DPRINTF("no mode select\n");
		return;
	}
	bzero(&mix, sizeof(mix));

	mix.wIndex = MAKE_WORD(d0->bUnitId, sc->sc_mixer_iface_no);
	mix.nchan = 1;
	mix.wValue[0] = MAKE_WORD(UD_MODE_SELECT_CONTROL, 0);
	uaudio_mixer_determine_class(&iot[id], &mix);
	mix.type = MIX_ON_OFF;		/* XXX */

	for (i = 0; i < ud->bNrModes; i++) {
		DPRINTFN(3, "i=%d bm=0x%x\n", i, UGETW(ud->waModes[i]));
		/* XXX */
	}

	uaudio_mixer_add_ctl(sc, &mix);
}

static void
uaudio_mixer_add_processing(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
	const struct usb_audio_processing_unit_0 *d0 = iot[id].u.pu;
	const struct usb_audio_processing_unit_1 *d1 =
	(const void *)(d0->baSourceId + d0->bNrInPins);
	struct uaudio_mixer_node mix;
	uint16_t ptype;

	bzero(&mix, sizeof(mix));

	ptype = UGETW(d0->wProcessType);

	DPRINTFN(3, "wProcessType=%d bUnitId=%d "
	    "bNrInPins=%d\n", ptype, d0->bUnitId, d0->bNrInPins);

	if (d1->bControlSize == 0) {
		return;
	}
	if (d1->bmControls[0] & UA_PROC_ENABLE_MASK) {
		mix.wIndex = MAKE_WORD(d0->bUnitId, sc->sc_mixer_iface_no);
		mix.nchan = 1;
		mix.wValue[0] = MAKE_WORD(XX_ENABLE_CONTROL, 0);
		uaudio_mixer_determine_class(&iot[id], &mix);
		mix.type = MIX_ON_OFF;
		uaudio_mixer_add_ctl(sc, &mix);
	}
	switch (ptype) {
	case UPDOWNMIX_PROCESS:
		uaudio_mixer_add_processing_updown(sc, iot, id);
		break;

	case DOLBY_PROLOGIC_PROCESS:
	case P3D_STEREO_EXTENDER_PROCESS:
	case REVERBATION_PROCESS:
	case CHORUS_PROCESS:
	case DYN_RANGE_COMP_PROCESS:
	default:
		DPRINTF("unit %d, type=%d is not implemented\n",
		    d0->bUnitId, ptype);
		break;
	}
}

static void
uaudio_mixer_add_extension(struct uaudio_softc *sc,
    const struct uaudio_terminal_node *iot, int id)
{
	const struct usb_audio_extension_unit_0 *d0 = iot[id].u.eu;
	const struct usb_audio_extension_unit_1 *d1 =
	(const void *)(d0->baSourceId + d0->bNrInPins);
	struct uaudio_mixer_node mix;

	DPRINTFN(3, "bUnitId=%d bNrInPins=%d\n",
	    d0->bUnitId, d0->bNrInPins);

	if (sc->sc_uq_au_no_xu) {
		return;
	}
	if (d1->bControlSize == 0) {
		return;
	}
	if (d1->bmControls[0] & UA_EXT_ENABLE_MASK) {

		bzero(&mix, sizeof(mix));

		mix.wIndex = MAKE_WORD(d0->bUnitId, sc->sc_mixer_iface_no);
		mix.nchan = 1;
		mix.wValue[0] = MAKE_WORD(UA_EXT_ENABLE, 0);
		uaudio_mixer_determine_class(&iot[id], &mix);
		mix.type = MIX_ON_OFF;

		uaudio_mixer_add_ctl(sc, &mix);
	}
}

static const void *
uaudio_mixer_verify_desc(const void *arg, uint32_t len)
{
	const struct usb_audio_mixer_unit_1 *d1;
	const struct usb_audio_extension_unit_1 *e1;
	const struct usb_audio_processing_unit_1 *u1;

	union {
		const struct usb_descriptor *desc;
		const struct usb_audio_input_terminal *it;
		const struct usb_audio_output_terminal *ot;
		const struct usb_audio_mixer_unit_0 *mu;
		const struct usb_audio_selector_unit *su;
		const struct usb_audio_feature_unit *fu;
		const struct usb_audio_processing_unit_0 *pu;
		const struct usb_audio_extension_unit_0 *eu;
	}     u;

	u.desc = arg;

	if (u.desc == NULL) {
		goto error;
	}
	if (u.desc->bDescriptorType != UDESC_CS_INTERFACE) {
		goto error;
	}
	switch (u.desc->bDescriptorSubtype) {
	case UDESCSUB_AC_INPUT:
		len += sizeof(*u.it);
		break;

	case UDESCSUB_AC_OUTPUT:
		len += sizeof(*u.ot);
		break;

	case UDESCSUB_AC_MIXER:
		len += sizeof(*u.mu);

		if (u.desc->bLength < len) {
			goto error;
		}
		len += u.mu->bNrInPins;

		if (u.desc->bLength < len) {
			goto error;
		}
		d1 = (const void *)(u.mu->baSourceId + u.mu->bNrInPins);

		len += sizeof(*d1);
		break;

	case UDESCSUB_AC_SELECTOR:
		len += sizeof(*u.su);

		if (u.desc->bLength < len) {
			goto error;
		}
		len += u.su->bNrInPins;
		break;

	case UDESCSUB_AC_FEATURE:
		len += (sizeof(*u.fu) + 1);
		break;

	case UDESCSUB_AC_PROCESSING:
		len += sizeof(*u.pu);

		if (u.desc->bLength < len) {
			goto error;
		}
		len += u.pu->bNrInPins;

		if (u.desc->bLength < len) {
			goto error;
		}
		u1 = (const void *)(u.pu->baSourceId + u.pu->bNrInPins);

		len += sizeof(*u1);

		if (u.desc->bLength < len) {
			goto error;
		}
		len += u1->bControlSize;

		break;

	case UDESCSUB_AC_EXTENSION:
		len += sizeof(*u.eu);

		if (u.desc->bLength < len) {
			goto error;
		}
		len += u.eu->bNrInPins;

		if (u.desc->bLength < len) {
			goto error;
		}
		e1 = (const void *)(u.eu->baSourceId + u.eu->bNrInPins);

		len += sizeof(*e1);

		if (u.desc->bLength < len) {
			goto error;
		}
		len += e1->bControlSize;
		break;

	default:
		goto error;
	}

	if (u.desc->bLength < len) {
		goto error;
	}
	return (u.desc);

error:
	if (u.desc) {
		DPRINTF("invalid descriptor, type=%d, "
		    "sub_type=%d, len=%d of %d bytes\n",
		    u.desc->bDescriptorType,
		    u.desc->bDescriptorSubtype,
		    u.desc->bLength, len);
	}
	return (NULL);
}

#ifdef USB_DEBUG
static void
uaudio_mixer_dump_cluster(uint8_t id, const struct uaudio_terminal_node *iot)
{
	static const char *channel_names[16] = {
		"LEFT", "RIGHT", "CENTER", "LFE",
		"LEFT_SURROUND", "RIGHT_SURROUND", "LEFT_CENTER", "RIGHT_CENTER",
		"SURROUND", "LEFT_SIDE", "RIGHT_SIDE", "TOP",
		"RESERVED12", "RESERVED13", "RESERVED14", "RESERVED15",
	};
	uint16_t cc;
	uint8_t i;
	const struct usb_audio_cluster cl = uaudio_mixer_get_cluster(id, iot);

	cc = UGETW(cl.wChannelConfig);

	DPRINTF("cluster: bNrChannels=%u iChannelNames=%u wChannelConfig="
	    "0x%04x:\n", cl.iChannelNames, cl.bNrChannels, cc);

	for (i = 0; cc; i++) {
		if (cc & 1) {
			DPRINTF(" - %s\n", channel_names[i]);
		}
		cc >>= 1;
	}
}

#endif

static struct usb_audio_cluster
uaudio_mixer_get_cluster(uint8_t id, const struct uaudio_terminal_node *iot)
{
	struct usb_audio_cluster r;
	const struct usb_descriptor *dp;
	uint8_t i;

	for (i = 0; i < UAUDIO_RECURSE_LIMIT; i++) {	/* avoid infinite loops */
		dp = iot[id].u.desc;
		if (dp == NULL) {
			goto error;
		}
		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
			r.bNrChannels = iot[id].u.it->bNrChannels;
			r.wChannelConfig[0] = iot[id].u.it->wChannelConfig[0];
			r.wChannelConfig[1] = iot[id].u.it->wChannelConfig[1];
			r.iChannelNames = iot[id].u.it->iChannelNames;
			goto done;

		case UDESCSUB_AC_OUTPUT:
			id = iot[id].u.ot->bSourceId;
			break;

		case UDESCSUB_AC_MIXER:
			r = *(const struct usb_audio_cluster *)
			    &iot[id].u.mu->baSourceId[iot[id].u.mu->
			    bNrInPins];
			goto done;

		case UDESCSUB_AC_SELECTOR:
			if (iot[id].u.su->bNrInPins > 0) {
				/* XXX This is not really right */
				id = iot[id].u.su->baSourceId[0];
			}
			break;

		case UDESCSUB_AC_FEATURE:
			id = iot[id].u.fu->bSourceId;
			break;

		case UDESCSUB_AC_PROCESSING:
			r = *((const struct usb_audio_cluster *)
			    &iot[id].u.pu->baSourceId[iot[id].u.pu->
			    bNrInPins]);
			goto done;

		case UDESCSUB_AC_EXTENSION:
			r = *((const struct usb_audio_cluster *)
			    &iot[id].u.eu->baSourceId[iot[id].u.eu->
			    bNrInPins]);
			goto done;

		default:
			goto error;
		}
	}
error:
	DPRINTF("bad data\n");
	bzero(&r, sizeof(r));
done:
	return (r);
}

#ifdef USB_DEBUG

struct uaudio_tt_to_string {
	uint16_t terminal_type;
	const char *desc;
};

static const struct uaudio_tt_to_string uaudio_tt_to_string[] = {

	/* USB terminal types */
	{UAT_UNDEFINED, "UAT_UNDEFINED"},
	{UAT_STREAM, "UAT_STREAM"},
	{UAT_VENDOR, "UAT_VENDOR"},

	/* input terminal types */
	{UATI_UNDEFINED, "UATI_UNDEFINED"},
	{UATI_MICROPHONE, "UATI_MICROPHONE"},
	{UATI_DESKMICROPHONE, "UATI_DESKMICROPHONE"},
	{UATI_PERSONALMICROPHONE, "UATI_PERSONALMICROPHONE"},
	{UATI_OMNIMICROPHONE, "UATI_OMNIMICROPHONE"},
	{UATI_MICROPHONEARRAY, "UATI_MICROPHONEARRAY"},
	{UATI_PROCMICROPHONEARR, "UATI_PROCMICROPHONEARR"},

	/* output terminal types */
	{UATO_UNDEFINED, "UATO_UNDEFINED"},
	{UATO_SPEAKER, "UATO_SPEAKER"},
	{UATO_HEADPHONES, "UATO_HEADPHONES"},
	{UATO_DISPLAYAUDIO, "UATO_DISPLAYAUDIO"},
	{UATO_DESKTOPSPEAKER, "UATO_DESKTOPSPEAKER"},
	{UATO_ROOMSPEAKER, "UATO_ROOMSPEAKER"},
	{UATO_COMMSPEAKER, "UATO_COMMSPEAKER"},
	{UATO_SUBWOOFER, "UATO_SUBWOOFER"},

	/* bidir terminal types */
	{UATB_UNDEFINED, "UATB_UNDEFINED"},
	{UATB_HANDSET, "UATB_HANDSET"},
	{UATB_HEADSET, "UATB_HEADSET"},
	{UATB_SPEAKERPHONE, "UATB_SPEAKERPHONE"},
	{UATB_SPEAKERPHONEESUP, "UATB_SPEAKERPHONEESUP"},
	{UATB_SPEAKERPHONEECANC, "UATB_SPEAKERPHONEECANC"},

	/* telephony terminal types */
	{UATT_UNDEFINED, "UATT_UNDEFINED"},
	{UATT_PHONELINE, "UATT_PHONELINE"},
	{UATT_TELEPHONE, "UATT_TELEPHONE"},
	{UATT_DOWNLINEPHONE, "UATT_DOWNLINEPHONE"},

	/* external terminal types */
	{UATE_UNDEFINED, "UATE_UNDEFINED"},
	{UATE_ANALOGCONN, "UATE_ANALOGCONN"},
	{UATE_LINECONN, "UATE_LINECONN"},
	{UATE_LEGACYCONN, "UATE_LEGACYCONN"},
	{UATE_DIGITALAUIFC, "UATE_DIGITALAUIFC"},
	{UATE_SPDIF, "UATE_SPDIF"},
	{UATE_1394DA, "UATE_1394DA"},
	{UATE_1394DV, "UATE_1394DV"},

	/* embedded function terminal types */
	{UATF_UNDEFINED, "UATF_UNDEFINED"},
	{UATF_CALIBNOISE, "UATF_CALIBNOISE"},
	{UATF_EQUNOISE, "UATF_EQUNOISE"},
	{UATF_CDPLAYER, "UATF_CDPLAYER"},
	{UATF_DAT, "UATF_DAT"},
	{UATF_DCC, "UATF_DCC"},
	{UATF_MINIDISK, "UATF_MINIDISK"},
	{UATF_ANALOGTAPE, "UATF_ANALOGTAPE"},
	{UATF_PHONOGRAPH, "UATF_PHONOGRAPH"},
	{UATF_VCRAUDIO, "UATF_VCRAUDIO"},
	{UATF_VIDEODISCAUDIO, "UATF_VIDEODISCAUDIO"},
	{UATF_DVDAUDIO, "UATF_DVDAUDIO"},
	{UATF_TVTUNERAUDIO, "UATF_TVTUNERAUDIO"},
	{UATF_SATELLITE, "UATF_SATELLITE"},
	{UATF_CABLETUNER, "UATF_CABLETUNER"},
	{UATF_DSS, "UATF_DSS"},
	{UATF_RADIORECV, "UATF_RADIORECV"},
	{UATF_RADIOXMIT, "UATF_RADIOXMIT"},
	{UATF_MULTITRACK, "UATF_MULTITRACK"},
	{UATF_SYNTHESIZER, "UATF_SYNTHESIZER"},

	/* unknown */
	{0x0000, "UNKNOWN"},
};

static const char *
uaudio_mixer_get_terminal_name(uint16_t terminal_type)
{
	const struct uaudio_tt_to_string *uat = uaudio_tt_to_string;

	while (uat->terminal_type) {
		if (uat->terminal_type == terminal_type) {
			break;
		}
		uat++;
	}
	if (uat->terminal_type == 0) {
		DPRINTF("unknown terminal type (0x%04x)", terminal_type);
	}
	return (uat->desc);
}

#endif

static uint16_t
uaudio_mixer_determine_class(const struct uaudio_terminal_node *iot,
    struct uaudio_mixer_node *mix)
{
	uint16_t terminal_type = 0x0000;
	const struct uaudio_terminal_node *input[2];
	const struct uaudio_terminal_node *output[2];

	input[0] = uaudio_mixer_get_input(iot, 0);
	input[1] = uaudio_mixer_get_input(iot, 1);

	output[0] = uaudio_mixer_get_output(iot, 0);
	output[1] = uaudio_mixer_get_output(iot, 1);

	/*
	 * check if there is only
	 * one output terminal:
	 */
	if (output[0] && (!output[1])) {
		terminal_type = UGETW(output[0]->u.ot->wTerminalType);
	}
	/*
	 * If the only output terminal is USB,
	 * the class is UAC_RECORD.
	 */
	if ((terminal_type & 0xff00) == (UAT_UNDEFINED & 0xff00)) {

		mix->class = UAC_RECORD;
		if (input[0] && (!input[1])) {
			terminal_type = UGETW(input[0]->u.it->wTerminalType);
		} else {
			terminal_type = 0;
		}
		goto done;
	}
	/*
	 * if the unit is connected to just
	 * one input terminal, the
	 * class is UAC_INPUT:
	 */
	if (input[0] && (!input[1])) {
		mix->class = UAC_INPUT;
		terminal_type = UGETW(input[0]->u.it->wTerminalType);
		goto done;
	}
	/*
	 * Otherwise, the class is UAC_OUTPUT.
	 */
	mix->class = UAC_OUTPUT;
done:
	return (terminal_type);
}

struct uaudio_tt_to_feature {
	uint16_t terminal_type;
	uint16_t feature;
};

static const struct uaudio_tt_to_feature uaudio_tt_to_feature[] = {

	{UAT_STREAM, SOUND_MIXER_PCM},

	{UATI_MICROPHONE, SOUND_MIXER_MIC},
	{UATI_DESKMICROPHONE, SOUND_MIXER_MIC},
	{UATI_PERSONALMICROPHONE, SOUND_MIXER_MIC},
	{UATI_OMNIMICROPHONE, SOUND_MIXER_MIC},
	{UATI_MICROPHONEARRAY, SOUND_MIXER_MIC},
	{UATI_PROCMICROPHONEARR, SOUND_MIXER_MIC},

	{UATO_SPEAKER, SOUND_MIXER_SPEAKER},
	{UATO_DESKTOPSPEAKER, SOUND_MIXER_SPEAKER},
	{UATO_ROOMSPEAKER, SOUND_MIXER_SPEAKER},
	{UATO_COMMSPEAKER, SOUND_MIXER_SPEAKER},

	{UATE_ANALOGCONN, SOUND_MIXER_LINE},
	{UATE_LINECONN, SOUND_MIXER_LINE},
	{UATE_LEGACYCONN, SOUND_MIXER_LINE},

	{UATE_DIGITALAUIFC, SOUND_MIXER_ALTPCM},
	{UATE_SPDIF, SOUND_MIXER_ALTPCM},
	{UATE_1394DA, SOUND_MIXER_ALTPCM},
	{UATE_1394DV, SOUND_MIXER_ALTPCM},

	{UATF_CDPLAYER, SOUND_MIXER_CD},

	{UATF_SYNTHESIZER, SOUND_MIXER_SYNTH},

	{UATF_VIDEODISCAUDIO, SOUND_MIXER_VIDEO},
	{UATF_DVDAUDIO, SOUND_MIXER_VIDEO},
	{UATF_TVTUNERAUDIO, SOUND_MIXER_VIDEO},

	/* telephony terminal types */
	{UATT_UNDEFINED, SOUND_MIXER_PHONEIN},	/* SOUND_MIXER_PHONEOUT */
	{UATT_PHONELINE, SOUND_MIXER_PHONEIN},	/* SOUND_MIXER_PHONEOUT */
	{UATT_TELEPHONE, SOUND_MIXER_PHONEIN},	/* SOUND_MIXER_PHONEOUT */
	{UATT_DOWNLINEPHONE, SOUND_MIXER_PHONEIN},	/* SOUND_MIXER_PHONEOUT */

	{UATF_RADIORECV, SOUND_MIXER_RADIO},
	{UATF_RADIOXMIT, SOUND_MIXER_RADIO},

	{UAT_UNDEFINED, SOUND_MIXER_VOLUME},
	{UAT_VENDOR, SOUND_MIXER_VOLUME},
	{UATI_UNDEFINED, SOUND_MIXER_VOLUME},

	/* output terminal types */
	{UATO_UNDEFINED, SOUND_MIXER_VOLUME},
	{UATO_DISPLAYAUDIO, SOUND_MIXER_VOLUME},
	{UATO_SUBWOOFER, SOUND_MIXER_VOLUME},
	{UATO_HEADPHONES, SOUND_MIXER_VOLUME},

	/* bidir terminal types */
	{UATB_UNDEFINED, SOUND_MIXER_VOLUME},
	{UATB_HANDSET, SOUND_MIXER_VOLUME},
	{UATB_HEADSET, SOUND_MIXER_VOLUME},
	{UATB_SPEAKERPHONE, SOUND_MIXER_VOLUME},
	{UATB_SPEAKERPHONEESUP, SOUND_MIXER_VOLUME},
	{UATB_SPEAKERPHONEECANC, SOUND_MIXER_VOLUME},

	/* external terminal types */
	{UATE_UNDEFINED, SOUND_MIXER_VOLUME},

	/* embedded function terminal types */
	{UATF_UNDEFINED, SOUND_MIXER_VOLUME},
	{UATF_CALIBNOISE, SOUND_MIXER_VOLUME},
	{UATF_EQUNOISE, SOUND_MIXER_VOLUME},
	{UATF_DAT, SOUND_MIXER_VOLUME},
	{UATF_DCC, SOUND_MIXER_VOLUME},
	{UATF_MINIDISK, SOUND_MIXER_VOLUME},
	{UATF_ANALOGTAPE, SOUND_MIXER_VOLUME},
	{UATF_PHONOGRAPH, SOUND_MIXER_VOLUME},
	{UATF_VCRAUDIO, SOUND_MIXER_VOLUME},
	{UATF_SATELLITE, SOUND_MIXER_VOLUME},
	{UATF_CABLETUNER, SOUND_MIXER_VOLUME},
	{UATF_DSS, SOUND_MIXER_VOLUME},
	{UATF_MULTITRACK, SOUND_MIXER_VOLUME},
	{0xffff, SOUND_MIXER_VOLUME},

	/* default */
	{0x0000, SOUND_MIXER_VOLUME},
};

static uint16_t
uaudio_mixer_feature_name(const struct uaudio_terminal_node *iot,
    struct uaudio_mixer_node *mix)
{
	const struct uaudio_tt_to_feature *uat = uaudio_tt_to_feature;
	uint16_t terminal_type = uaudio_mixer_determine_class(iot, mix);

	if ((mix->class == UAC_RECORD) && (terminal_type == 0)) {
		return (SOUND_MIXER_IMIX);
	}
	while (uat->terminal_type) {
		if (uat->terminal_type == terminal_type) {
			break;
		}
		uat++;
	}

	DPRINTF("terminal_type=%s (0x%04x) -> %d\n",
	    uaudio_mixer_get_terminal_name(terminal_type),
	    terminal_type, uat->feature);

	return (uat->feature);
}

const static struct uaudio_terminal_node *
uaudio_mixer_get_input(const struct uaudio_terminal_node *iot, uint8_t index)
{
	struct uaudio_terminal_node *root = iot->root;
	uint8_t n;

	n = iot->usr.id_max;
	do {
		if (iot->usr.bit_input[n / 8] & (1 << (n % 8))) {
			if (!index--) {
				return (root + n);
			}
		}
	} while (n--);

	return (NULL);
}

const static struct uaudio_terminal_node *
uaudio_mixer_get_output(const struct uaudio_terminal_node *iot, uint8_t index)
{
	struct uaudio_terminal_node *root = iot->root;
	uint8_t n;

	n = iot->usr.id_max;
	do {
		if (iot->usr.bit_output[n / 8] & (1 << (n % 8))) {
			if (!index--) {
				return (root + n);
			}
		}
	} while (n--);

	return (NULL);
}

static void
uaudio_mixer_find_inputs_sub(struct uaudio_terminal_node *root,
    const uint8_t *p_id, uint8_t n_id,
    struct uaudio_search_result *info)
{
	struct uaudio_terminal_node *iot;
	uint8_t n;
	uint8_t i;

	if (info->recurse_level >= UAUDIO_RECURSE_LIMIT) {
		return;
	}
	info->recurse_level++;

	for (n = 0; n < n_id; n++) {

		i = p_id[n];

		if (info->bit_visited[i / 8] & (1 << (i % 8))) {
			/* don't go into a circle */
			DPRINTF("avoided going into a circle at id=%d!\n", i);
			continue;
		} else {
			info->bit_visited[i / 8] |= (1 << (i % 8));
		}

		iot = (root + i);

		if (iot->u.desc == NULL) {
			continue;
		}
		switch (iot->u.desc->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
			info->bit_input[i / 8] |= (1 << (i % 8));
			break;

		case UDESCSUB_AC_FEATURE:
			uaudio_mixer_find_inputs_sub
			    (root, &iot->u.fu->bSourceId, 1, info);
			break;

		case UDESCSUB_AC_OUTPUT:
			uaudio_mixer_find_inputs_sub
			    (root, &iot->u.ot->bSourceId, 1, info);
			break;

		case UDESCSUB_AC_MIXER:
			uaudio_mixer_find_inputs_sub
			    (root, iot->u.mu->baSourceId,
			    iot->u.mu->bNrInPins, info);
			break;

		case UDESCSUB_AC_SELECTOR:
			uaudio_mixer_find_inputs_sub
			    (root, iot->u.su->baSourceId,
			    iot->u.su->bNrInPins, info);
			break;

		case UDESCSUB_AC_PROCESSING:
			uaudio_mixer_find_inputs_sub
			    (root, iot->u.pu->baSourceId,
			    iot->u.pu->bNrInPins, info);
			break;

		case UDESCSUB_AC_EXTENSION:
			uaudio_mixer_find_inputs_sub
			    (root, iot->u.eu->baSourceId,
			    iot->u.eu->bNrInPins, info);
			break;

		case UDESCSUB_AC_HEADER:
		default:
			break;
		}
	}
	info->recurse_level--;
}

static void
uaudio_mixer_find_outputs_sub(struct uaudio_terminal_node *root, uint8_t id,
    uint8_t n_id, struct uaudio_search_result *info)
{
	struct uaudio_terminal_node *iot = (root + id);
	uint8_t j;

	j = n_id;
	do {
		if ((j != id) && ((root + j)->u.desc) &&
		    ((root + j)->u.desc->bDescriptorSubtype == UDESCSUB_AC_OUTPUT)) {

			/*
			 * "j" (output) <--- virtual wire <--- "id" (input)
			 *
			 * if "j" has "id" on the input, then "id" have "j" on
			 * the output, because they are connected:
			 */
			if ((root + j)->usr.bit_input[id / 8] & (1 << (id % 8))) {
				iot->usr.bit_output[j / 8] |= (1 << (j % 8));
			}
		}
	} while (j--);
}

static void
uaudio_mixer_fill_info(struct uaudio_softc *sc, struct usb_device *udev,
    void *desc)
{
	const struct usb_audio_control_descriptor *acdp;
	struct usb_config_descriptor *cd = usbd_get_config_descriptor(udev);
	const struct usb_descriptor *dp;
	const struct usb_audio_unit *au;
	struct uaudio_terminal_node *iot = NULL;
	uint16_t wTotalLen;
	uint8_t ID_max = 0;		/* inclusive */
	uint8_t i;

	desc = usb_desc_foreach(cd, desc);

	if (desc == NULL) {
		DPRINTF("no Audio Control header\n");
		goto done;
	}
	acdp = desc;

	if ((acdp->bLength < sizeof(*acdp)) ||
	    (acdp->bDescriptorType != UDESC_CS_INTERFACE) ||
	    (acdp->bDescriptorSubtype != UDESCSUB_AC_HEADER)) {
		DPRINTF("invalid Audio Control header\n");
		goto done;
	}
	/* "wTotalLen" is allowed to be corrupt */
	wTotalLen = UGETW(acdp->wTotalLength) - acdp->bLength;

	/* get USB audio revision */
	sc->sc_audio_rev = UGETW(acdp->bcdADC);

	DPRINTFN(3, "found AC header, vers=%03x, len=%d\n",
	    sc->sc_audio_rev, wTotalLen);

	if (sc->sc_audio_rev != UAUDIO_VERSION) {

		if (sc->sc_uq_bad_adc) {

		} else {
			DPRINTF("invalid audio version\n");
			goto done;
		}
	}
	iot = malloc(sizeof(struct uaudio_terminal_node) * 256, M_TEMP,
	    M_WAITOK | M_ZERO);

	if (iot == NULL) {
		DPRINTF("no memory!\n");
		goto done;
	}
	while ((desc = usb_desc_foreach(cd, desc))) {

		dp = desc;

		if (dp->bLength > wTotalLen) {
			break;
		} else {
			wTotalLen -= dp->bLength;
		}

		au = uaudio_mixer_verify_desc(dp, 0);

		if (au) {
			iot[au->bUnitId].u.desc = (const void *)au;
			if (au->bUnitId > ID_max) {
				ID_max = au->bUnitId;
			}
		}
	}

	DPRINTF("Maximum ID=%d\n", ID_max);

	/*
	 * determine sourcing inputs for
	 * all nodes in the tree:
	 */
	i = ID_max;
	do {
		uaudio_mixer_find_inputs_sub(iot, &i, 1, &((iot + i)->usr));
	} while (i--);

	/*
	 * determine outputs for
	 * all nodes in the tree:
	 */
	i = ID_max;
	do {
		uaudio_mixer_find_outputs_sub(iot, i, ID_max, &((iot + i)->usr));
	} while (i--);

	/* set "id_max" and "root" */

	i = ID_max;
	do {
		(iot + i)->usr.id_max = ID_max;
		(iot + i)->root = iot;
	} while (i--);

#ifdef USB_DEBUG
	i = ID_max;
	do {
		uint8_t j;

		if (iot[i].u.desc == NULL) {
			continue;
		}
		DPRINTF("id %d:\n", i);

		switch (iot[i].u.desc->bDescriptorSubtype) {
		case UDESCSUB_AC_INPUT:
			DPRINTF(" - AC_INPUT type=%s\n",
			    uaudio_mixer_get_terminal_name
			    (UGETW(iot[i].u.it->wTerminalType)));
			uaudio_mixer_dump_cluster(i, iot);
			break;

		case UDESCSUB_AC_OUTPUT:
			DPRINTF(" - AC_OUTPUT type=%s "
			    "src=%d\n", uaudio_mixer_get_terminal_name
			    (UGETW(iot[i].u.ot->wTerminalType)),
			    iot[i].u.ot->bSourceId);
			break;

		case UDESCSUB_AC_MIXER:
			DPRINTF(" - AC_MIXER src:\n");
			for (j = 0; j < iot[i].u.mu->bNrInPins; j++) {
				DPRINTF("   - %d\n", iot[i].u.mu->baSourceId[j]);
			}
			uaudio_mixer_dump_cluster(i, iot);
			break;

		case UDESCSUB_AC_SELECTOR:
			DPRINTF(" - AC_SELECTOR src:\n");
			for (j = 0; j < iot[i].u.su->bNrInPins; j++) {
				DPRINTF("   - %d\n", iot[i].u.su->baSourceId[j]);
			}
			break;

		case UDESCSUB_AC_FEATURE:
			DPRINTF(" - AC_FEATURE src=%d\n", iot[i].u.fu->bSourceId);
			break;

		case UDESCSUB_AC_PROCESSING:
			DPRINTF(" - AC_PROCESSING src:\n");
			for (j = 0; j < iot[i].u.pu->bNrInPins; j++) {
				DPRINTF("   - %d\n", iot[i].u.pu->baSourceId[j]);
			}
			uaudio_mixer_dump_cluster(i, iot);
			break;

		case UDESCSUB_AC_EXTENSION:
			DPRINTF(" - AC_EXTENSION src:\n");
			for (j = 0; j < iot[i].u.eu->bNrInPins; j++) {
				DPRINTF("%d ", iot[i].u.eu->baSourceId[j]);
			}
			uaudio_mixer_dump_cluster(i, iot);
			break;

		default:
			DPRINTF("unknown audio control (subtype=%d)\n",
			    iot[i].u.desc->bDescriptorSubtype);
		}

		DPRINTF("Inputs to this ID are:\n");

		j = ID_max;
		do {
			if (iot[i].usr.bit_input[j / 8] & (1 << (j % 8))) {
				DPRINTF("  -- ID=%d\n", j);
			}
		} while (j--);

		DPRINTF("Outputs from this ID are:\n");

		j = ID_max;
		do {
			if (iot[i].usr.bit_output[j / 8] & (1 << (j % 8))) {
				DPRINTF("  -- ID=%d\n", j);
			}
		} while (j--);

	} while (i--);
#endif

	/*
	 * scan the config to create a linked
	 * list of "mixer" nodes:
	 */

	i = ID_max;
	do {
		dp = iot[i].u.desc;

		if (dp == NULL) {
			continue;
		}
		DPRINTFN(11, "id=%d subtype=%d\n",
		    i, dp->bDescriptorSubtype);

		switch (dp->bDescriptorSubtype) {
		case UDESCSUB_AC_HEADER:
			DPRINTF("unexpected AC header\n");
			break;

		case UDESCSUB_AC_INPUT:
			uaudio_mixer_add_input(sc, iot, i);
			break;

		case UDESCSUB_AC_OUTPUT:
			uaudio_mixer_add_output(sc, iot, i);
			break;

		case UDESCSUB_AC_MIXER:
			uaudio_mixer_add_mixer(sc, iot, i);
			break;

		case UDESCSUB_AC_SELECTOR:
			uaudio_mixer_add_selector(sc, iot, i);
			break;

		case UDESCSUB_AC_FEATURE:
			uaudio_mixer_add_feature(sc, iot, i);
			break;

		case UDESCSUB_AC_PROCESSING:
			uaudio_mixer_add_processing(sc, iot, i);
			break;

		case UDESCSUB_AC_EXTENSION:
			uaudio_mixer_add_extension(sc, iot, i);
			break;

		default:
			DPRINTF("bad AC desc subtype=0x%02x\n",
			    dp->bDescriptorSubtype);
			break;
		}

	} while (i--);

done:
	if (iot) {
		free(iot, M_TEMP);
	}
}

static uint16_t
uaudio_mixer_get(struct usb_device *udev, uint8_t what,
    struct uaudio_mixer_node *mc)
{
	struct usb_device_request req;
	uint16_t val;
	uint16_t len = MIX_SIZE(mc->type);
	uint8_t data[4];
	usb_error_t err;

	if (mc->wValue[0] == -1) {
		return (0);
	}
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = what;
	USETW(req.wValue, mc->wValue[0]);
	USETW(req.wIndex, mc->wIndex);
	USETW(req.wLength, len);

	err = usbd_do_request(udev, NULL, &req, data);
	if (err) {
		DPRINTF("err=%s\n", usbd_errstr(err));
		return (0);
	}
	if (len < 1) {
		data[0] = 0;
	}
	if (len < 2) {
		data[1] = 0;
	}
	val = (data[0] | (data[1] << 8));

	DPRINTFN(3, "val=%d\n", val);

	return (val);
}

static void
uaudio_mixer_write_cfg_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_device_request req;
	struct uaudio_softc *sc = usbd_xfer_softc(xfer);
	struct uaudio_mixer_node *mc = sc->sc_mixer_curr;
	struct usb_page_cache *pc;
	uint16_t len;
	uint8_t repeat = 1;
	uint8_t update;
	uint8_t chan;
	uint8_t buf[2];

	DPRINTF("\n");

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
tr_transferred:
	case USB_ST_SETUP:
tr_setup:

		if (mc == NULL) {
			mc = sc->sc_mixer_root;
			sc->sc_mixer_curr = mc;
			sc->sc_mixer_chan = 0;
			repeat = 0;
		}
		while (mc) {
			while (sc->sc_mixer_chan < mc->nchan) {

				len = MIX_SIZE(mc->type);

				chan = sc->sc_mixer_chan;

				sc->sc_mixer_chan++;

				update = ((mc->update[chan / 8] & (1 << (chan % 8))) &&
				    (mc->wValue[chan] != -1));

				mc->update[chan / 8] &= ~(1 << (chan % 8));

				if (update) {

					req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
					req.bRequest = SET_CUR;
					USETW(req.wValue, mc->wValue[chan]);
					USETW(req.wIndex, mc->wIndex);
					USETW(req.wLength, len);

					if (len > 0) {
						buf[0] = (mc->wData[chan] & 0xFF);
					}
					if (len > 1) {
						buf[1] = (mc->wData[chan] >> 8) & 0xFF;
					}
					pc = usbd_xfer_get_frame(xfer, 0);
					usbd_copy_in(pc, 0, &req, sizeof(req));
					pc = usbd_xfer_get_frame(xfer, 1);
					usbd_copy_in(pc, 0, buf, len);

					usbd_xfer_set_frame_len(xfer, 0, sizeof(req));
					usbd_xfer_set_frame_len(xfer, 1, len);
					usbd_xfer_set_frames(xfer, len ? 2 : 1);
					usbd_transfer_submit(xfer);
					return;
				}
			}

			mc = mc->next;
			sc->sc_mixer_curr = mc;
			sc->sc_mixer_chan = 0;
		}

		if (repeat) {
			goto tr_setup;
		}
		break;

	default:			/* Error */
		DPRINTF("error=%s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED) {
			/* do nothing - we are detaching */
			break;
		}
		goto tr_transferred;
	}
}

static usb_error_t
uaudio_set_speed(struct usb_device *udev, uint8_t endpt, uint32_t speed)
{
	struct usb_device_request req;
	uint8_t data[3];

	DPRINTFN(6, "endpt=%d speed=%u\n", endpt, speed);

	req.bmRequestType = UT_WRITE_CLASS_ENDPOINT;
	req.bRequest = SET_CUR;
	USETW2(req.wValue, SAMPLING_FREQ_CONTROL, 0);
	USETW(req.wIndex, endpt);
	USETW(req.wLength, 3);
	data[0] = speed;
	data[1] = speed >> 8;
	data[2] = speed >> 16;

	return (usbd_do_request(udev, NULL, &req, data));
}

static int
uaudio_mixer_signext(uint8_t type, int val)
{
	if (!MIX_UNSIGNED(type)) {
		if (MIX_SIZE(type) == 2) {
			val = (int16_t)val;
		} else {
			val = (int8_t)val;
		}
	}
	return (val);
}

static int
uaudio_mixer_bsd2value(struct uaudio_mixer_node *mc, int32_t val)
{
	if (mc->type == MIX_ON_OFF) {
		val = (val != 0);
	} else if (mc->type == MIX_SELECTOR) {
		if ((val < mc->minval) ||
		    (val > mc->maxval)) {
			val = mc->minval;
		}
	} else {

		/* compute actual volume */
		val = (val * mc->mul) / 255;

		/* add lower offset */
		val = val + mc->minval;

		/* make sure we don't write a value out of range */
		if (val > mc->maxval)
			val = mc->maxval;
		else if (val < mc->minval)
			val = mc->minval;
	}

	DPRINTFN(6, "type=0x%03x val=%d min=%d max=%d val=%d\n",
	    mc->type, val, mc->minval, mc->maxval, val);
	return (val);
}

static void
uaudio_mixer_ctl_set(struct uaudio_softc *sc, struct uaudio_mixer_node *mc,
    uint8_t chan, int32_t val)
{
	val = uaudio_mixer_bsd2value(mc, val);

	mc->update[chan / 8] |= (1 << (chan % 8));
	mc->wData[chan] = val;

	/* start the transfer, if not already started */

	usbd_transfer_start(sc->sc_mixer_xfer[0]);
}

static void
uaudio_mixer_init(struct uaudio_softc *sc)
{
	struct uaudio_mixer_node *mc;
	int32_t i;

	for (mc = sc->sc_mixer_root; mc;
	    mc = mc->next) {

		if (mc->ctl != SOUND_MIXER_NRDEVICES) {
			/*
			 * Set device mask bits. See
			 * /usr/include/machine/soundcard.h
			 */
			sc->sc_mix_info |= (1 << mc->ctl);
		}
		if ((mc->ctl == SOUND_MIXER_NRDEVICES) &&
		    (mc->type == MIX_SELECTOR)) {

			for (i = mc->minval; (i > 0) && (i <= mc->maxval); i++) {
				if (mc->slctrtype[i - 1] == SOUND_MIXER_NRDEVICES) {
					continue;
				}
				sc->sc_recsrc_info |= 1 << mc->slctrtype[i - 1];
			}
		}
	}
}

int
uaudio_mixer_init_sub(struct uaudio_softc *sc, struct snd_mixer *m)
{
	DPRINTF("\n");

	if (usbd_transfer_setup(sc->sc_udev, &sc->sc_mixer_iface_index,
	    sc->sc_mixer_xfer, uaudio_mixer_config, 1, sc,
	    mixer_get_lock(m))) {
		DPRINTFN(0, "could not allocate USB "
		    "transfer for audio mixer!\n");
		return (ENOMEM);
	}
	if (!(sc->sc_mix_info & SOUND_MASK_VOLUME)) {
		mix_setparentchild(m, SOUND_MIXER_VOLUME, SOUND_MASK_PCM);
		mix_setrealdev(m, SOUND_MIXER_VOLUME, SOUND_MIXER_NONE);
	}
	mix_setdevs(m, sc->sc_mix_info);
	mix_setrecdevs(m, sc->sc_recsrc_info);
	return (0);
}

int
uaudio_mixer_uninit_sub(struct uaudio_softc *sc)
{
	DPRINTF("\n");

	usbd_transfer_unsetup(sc->sc_mixer_xfer, 1);

	return (0);
}

void
uaudio_mixer_set(struct uaudio_softc *sc, unsigned type,
    unsigned left, unsigned right)
{
	struct uaudio_mixer_node *mc;

	for (mc = sc->sc_mixer_root; mc;
	    mc = mc->next) {

		if (mc->ctl == type) {
			if (mc->nchan == 2) {
				/* set Right */
				uaudio_mixer_ctl_set(sc, mc, 1, (int)(right * 255) / 100);
			}
			/* set Left or Mono */
			uaudio_mixer_ctl_set(sc, mc, 0, (int)(left * 255) / 100);
		}
	}
}

uint32_t
uaudio_mixer_setrecsrc(struct uaudio_softc *sc, uint32_t src)
{
	struct uaudio_mixer_node *mc;
	uint32_t mask;
	uint32_t temp;
	int32_t i;

	for (mc = sc->sc_mixer_root; mc;
	    mc = mc->next) {

		if ((mc->ctl == SOUND_MIXER_NRDEVICES) &&
		    (mc->type == MIX_SELECTOR)) {

			/* compute selector mask */

			mask = 0;
			for (i = mc->minval; (i > 0) && (i <= mc->maxval); i++) {
				mask |= (1 << mc->slctrtype[i - 1]);
			}

			temp = mask & src;
			if (temp == 0) {
				continue;
			}
			/* find the first set bit */
			temp = (-temp) & temp;

			/* update "src" */
			src &= ~mask;
			src |= temp;

			for (i = mc->minval; (i > 0) && (i <= mc->maxval); i++) {
				if (temp != (1 << mc->slctrtype[i - 1])) {
					continue;
				}
				uaudio_mixer_ctl_set(sc, mc, 0, i);
				break;
			}
		}
	}
	return (src);
}

/*========================================================================*
 * MIDI support routines
 *========================================================================*/

static void
umidi_read_clear_stall_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct umidi_chan *chan = usbd_xfer_softc(xfer);
	struct usb_xfer *xfer_other = chan->xfer[1];

	if (usbd_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		chan->flags &= ~UMIDI_FLAG_READ_STALL;
		usbd_transfer_start(xfer_other);
	}
}

static void
umidi_bulk_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct umidi_chan *chan = usbd_xfer_softc(xfer);
	struct umidi_sub_chan *sub;
	struct usb_page_cache *pc;
	uint8_t buf[1];
	uint8_t cmd_len;
	uint8_t cn;
	uint16_t pos;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		DPRINTF("actlen=%d bytes\n", actlen);

		if (actlen == 0) {
			/* should not happen */
			goto tr_error;
		}
		pos = 0;
		pc = usbd_xfer_get_frame(xfer, 0);

		while (actlen >= 4) {

			usbd_copy_out(pc, pos, buf, 1);

			cmd_len = umidi_cmd_to_len[buf[0] & 0xF];	/* command length */
			cn = buf[0] >> 4;	/* cable number */
			sub = &chan->sub[cn];

			if (cmd_len && (cn < chan->max_cable) && sub->read_open) {
				usb_fifo_put_data(sub->fifo.fp[USB_FIFO_RX], pc,
				    pos + 1, cmd_len, 1);
			} else {
				/* ignore the command */
			}

			actlen -= 4;
			pos += 4;
		}

	case USB_ST_SETUP:
		DPRINTF("start\n");

		if (chan->flags & UMIDI_FLAG_READ_STALL) {
			usbd_transfer_start(chan->xfer[3]);
			return;
		}
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		return;

	default:
tr_error:

		DPRINTF("error=%s\n", usbd_errstr(error));

		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			chan->flags |= UMIDI_FLAG_READ_STALL;
			usbd_transfer_start(chan->xfer[3]);
		}
		return;

	}
}

static void
umidi_write_clear_stall_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct umidi_chan *chan = usbd_xfer_softc(xfer);
	struct usb_xfer *xfer_other = chan->xfer[0];

	if (usbd_clear_stall_callback(xfer, xfer_other)) {
		DPRINTF("stall cleared\n");
		chan->flags &= ~UMIDI_FLAG_WRITE_STALL;
		usbd_transfer_start(xfer_other);
	}
}

/*
 * The following statemachine, that converts MIDI commands to
 * USB MIDI packets, derives from Linux's usbmidi.c, which
 * was written by "Clemens Ladisch":
 *
 * Returns:
 *    0: No command
 * Else: Command is complete
 */
static uint8_t
umidi_convert_to_usb(struct umidi_sub_chan *sub, uint8_t cn, uint8_t b)
{
	uint8_t p0 = (cn << 4);

	if (b >= 0xf8) {
		sub->temp_0[0] = p0 | 0x0f;
		sub->temp_0[1] = b;
		sub->temp_0[2] = 0;
		sub->temp_0[3] = 0;
		sub->temp_cmd = sub->temp_0;
		return (1);

	} else if (b >= 0xf0) {
		switch (b) {
		case 0xf0:		/* system exclusive begin */
			sub->temp_1[1] = b;
			sub->state = UMIDI_ST_SYSEX_1;
			break;
		case 0xf1:		/* MIDI time code */
		case 0xf3:		/* song select */
			sub->temp_1[1] = b;
			sub->state = UMIDI_ST_1PARAM;
			break;
		case 0xf2:		/* song position pointer */
			sub->temp_1[1] = b;
			sub->state = UMIDI_ST_2PARAM_1;
			break;
		case 0xf4:		/* unknown */
		case 0xf5:		/* unknown */
			sub->state = UMIDI_ST_UNKNOWN;
			break;
		case 0xf6:		/* tune request */
			sub->temp_1[0] = p0 | 0x05;
			sub->temp_1[1] = 0xf6;
			sub->temp_1[2] = 0;
			sub->temp_1[3] = 0;
			sub->temp_cmd = sub->temp_1;
			sub->state = UMIDI_ST_UNKNOWN;
			return (1);

		case 0xf7:		/* system exclusive end */
			switch (sub->state) {
			case UMIDI_ST_SYSEX_0:
				sub->temp_1[0] = p0 | 0x05;
				sub->temp_1[1] = 0xf7;
				sub->temp_1[2] = 0;
				sub->temp_1[3] = 0;
				sub->temp_cmd = sub->temp_1;
				sub->state = UMIDI_ST_UNKNOWN;
				return (1);
			case UMIDI_ST_SYSEX_1:
				sub->temp_1[0] = p0 | 0x06;
				sub->temp_1[2] = 0xf7;
				sub->temp_1[3] = 0;
				sub->temp_cmd = sub->temp_1;
				sub->state = UMIDI_ST_UNKNOWN;
				return (1);
			case UMIDI_ST_SYSEX_2:
				sub->temp_1[0] = p0 | 0x07;
				sub->temp_1[3] = 0xf7;
				sub->temp_cmd = sub->temp_1;
				sub->state = UMIDI_ST_UNKNOWN;
				return (1);
			}
			sub->state = UMIDI_ST_UNKNOWN;
			break;
		}
	} else if (b >= 0x80) {
		sub->temp_1[1] = b;
		if ((b >= 0xc0) && (b <= 0xdf)) {
			sub->state = UMIDI_ST_1PARAM;
		} else {
			sub->state = UMIDI_ST_2PARAM_1;
		}
	} else {			/* b < 0x80 */
		switch (sub->state) {
		case UMIDI_ST_1PARAM:
			if (sub->temp_1[1] < 0xf0) {
				p0 |= sub->temp_1[1] >> 4;
			} else {
				p0 |= 0x02;
				sub->state = UMIDI_ST_UNKNOWN;
			}
			sub->temp_1[0] = p0;
			sub->temp_1[2] = b;
			sub->temp_1[3] = 0;
			sub->temp_cmd = sub->temp_1;
			return (1);
		case UMIDI_ST_2PARAM_1:
			sub->temp_1[2] = b;
			sub->state = UMIDI_ST_2PARAM_2;
			break;
		case UMIDI_ST_2PARAM_2:
			if (sub->temp_1[1] < 0xf0) {
				p0 |= sub->temp_1[1] >> 4;
				sub->state = UMIDI_ST_2PARAM_1;
			} else {
				p0 |= 0x03;
				sub->state = UMIDI_ST_UNKNOWN;
			}
			sub->temp_1[0] = p0;
			sub->temp_1[3] = b;
			sub->temp_cmd = sub->temp_1;
			return (1);
		case UMIDI_ST_SYSEX_0:
			sub->temp_1[1] = b;
			sub->state = UMIDI_ST_SYSEX_1;
			break;
		case UMIDI_ST_SYSEX_1:
			sub->temp_1[2] = b;
			sub->state = UMIDI_ST_SYSEX_2;
			break;
		case UMIDI_ST_SYSEX_2:
			sub->temp_1[0] = p0 | 0x04;
			sub->temp_1[3] = b;
			sub->temp_cmd = sub->temp_1;
			sub->state = UMIDI_ST_SYSEX_0;
			return (1);
		}
	}
	return (0);
}

static void
umidi_bulk_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct umidi_chan *chan = usbd_xfer_softc(xfer);
	struct umidi_sub_chan *sub;
	struct usb_page_cache *pc;
	uint32_t actlen;
	uint16_t total_length;
	uint8_t buf;
	uint8_t start_cable;
	uint8_t tr_any;
	int len;

	usbd_xfer_status(xfer, &len, NULL, NULL, NULL);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTF("actlen=%d bytes\n", len);

	case USB_ST_SETUP:

		DPRINTF("start\n");

		if (chan->flags & UMIDI_FLAG_WRITE_STALL) {
			usbd_transfer_start(chan->xfer[2]);
			return;
		}
		total_length = 0;	/* reset */
		start_cable = chan->curr_cable;
		tr_any = 0;
		pc = usbd_xfer_get_frame(xfer, 0);

		while (1) {

			/* round robin de-queueing */

			sub = &chan->sub[chan->curr_cable];

			if (sub->write_open) {
				usb_fifo_get_data(sub->fifo.fp[USB_FIFO_TX],
				    pc, total_length, 1, &actlen, 0);
			} else {
				actlen = 0;
			}

			if (actlen) {
				usbd_copy_out(pc, total_length, &buf, 1);

				tr_any = 1;

				DPRINTF("byte=0x%02x\n", buf);

				if (umidi_convert_to_usb(sub, chan->curr_cable, buf)) {

					DPRINTF("sub= %02x %02x %02x %02x\n",
					    sub->temp_cmd[0], sub->temp_cmd[1],
					    sub->temp_cmd[2], sub->temp_cmd[3]);

					usbd_copy_in(pc, total_length,
					    sub->temp_cmd, 4);

					total_length += 4;

					if (total_length >= UMIDI_BULK_SIZE) {
						break;
					}
				} else {
					continue;
				}
			}
			chan->curr_cable++;
			if (chan->curr_cable >= chan->max_cable) {
				chan->curr_cable = 0;
			}
			if (chan->curr_cable == start_cable) {
				if (tr_any == 0) {
					break;
				}
				tr_any = 0;
			}
		}

		if (total_length) {
			usbd_xfer_set_frame_len(xfer, 0, total_length);
			usbd_transfer_submit(xfer);
		}
		return;

	default:			/* Error */

		DPRINTF("error=%s\n", usbd_errstr(error));

		if (error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			chan->flags |= UMIDI_FLAG_WRITE_STALL;
			usbd_transfer_start(chan->xfer[2]);
		}
		return;

	}
}

static struct umidi_sub_chan *
umidi_sub_by_fifo(struct usb_fifo *fifo)
{
	struct umidi_chan *chan = usb_fifo_softc(fifo);
	struct umidi_sub_chan *sub;
	uint32_t n;

	for (n = 0; n < UMIDI_CABLES_MAX; n++) {
		sub = &chan->sub[n];
		if ((sub->fifo.fp[USB_FIFO_RX] == fifo) ||
		    (sub->fifo.fp[USB_FIFO_TX] == fifo)) {
			return (sub);
		}
	}

	panic("%s:%d cannot find usb_fifo!\n",
	    __FILE__, __LINE__);

	return (NULL);
}

static void
umidi_start_read(struct usb_fifo *fifo)
{
	struct umidi_chan *chan = usb_fifo_softc(fifo);

	usbd_transfer_start(chan->xfer[1]);
}

static void
umidi_stop_read(struct usb_fifo *fifo)
{
	struct umidi_chan *chan = usb_fifo_softc(fifo);
	struct umidi_sub_chan *sub = umidi_sub_by_fifo(fifo);

	DPRINTF("\n");

	sub->read_open = 0;

	if (--(chan->read_open_refcount) == 0) {
		/*
		 * XXX don't stop the read transfer here, hence that causes
		 * problems with some MIDI adapters
		 */
		DPRINTF("(stopping read transfer)\n");
	}
}

static void
umidi_start_write(struct usb_fifo *fifo)
{
	struct umidi_chan *chan = usb_fifo_softc(fifo);

	usbd_transfer_start(chan->xfer[0]);
}

static void
umidi_stop_write(struct usb_fifo *fifo)
{
	struct umidi_chan *chan = usb_fifo_softc(fifo);
	struct umidi_sub_chan *sub = umidi_sub_by_fifo(fifo);

	DPRINTF("\n");

	sub->write_open = 0;

	if (--(chan->write_open_refcount) == 0) {
		DPRINTF("(stopping write transfer)\n");
		usbd_transfer_stop(chan->xfer[2]);
		usbd_transfer_stop(chan->xfer[0]);
	}
}

static int
umidi_open(struct usb_fifo *fifo, int fflags)
{
	struct umidi_chan *chan = usb_fifo_softc(fifo);
	struct umidi_sub_chan *sub = umidi_sub_by_fifo(fifo);

	if (fflags & FREAD) {
		if (usb_fifo_alloc_buffer(fifo, 4, (1024 / 4))) {
			return (ENOMEM);
		}
		mtx_lock(&chan->mtx);
		chan->read_open_refcount++;
		sub->read_open = 1;
		mtx_unlock(&chan->mtx);
	}
	if (fflags & FWRITE) {
		if (usb_fifo_alloc_buffer(fifo, 32, (1024 / 32))) {
			return (ENOMEM);
		}
		/* clear stall first */
		mtx_lock(&chan->mtx);
		chan->flags |= UMIDI_FLAG_WRITE_STALL;
		chan->write_open_refcount++;
		sub->write_open = 1;

		/* reset */
		sub->state = UMIDI_ST_UNKNOWN;
		mtx_unlock(&chan->mtx);
	}
	return (0);			/* success */
}

static void
umidi_close(struct usb_fifo *fifo, int fflags)
{
	if (fflags & FREAD) {
		usb_fifo_free_buffer(fifo);
	}
	if (fflags & FWRITE) {
		usb_fifo_free_buffer(fifo);
	}
}


static int
umidi_ioctl(struct usb_fifo *fifo, u_long cmd, void *data,
    int fflags)
{
	return (ENODEV);
}

static void
umidi_init(device_t dev)
{
	struct uaudio_softc *sc = device_get_softc(dev);
	struct umidi_chan *chan = &sc->sc_midi_chan;

	mtx_init(&chan->mtx, "umidi lock", NULL, MTX_DEF | MTX_RECURSE);
}

static struct usb_fifo_methods umidi_fifo_methods = {
	.f_start_read = &umidi_start_read,
	.f_start_write = &umidi_start_write,
	.f_stop_read = &umidi_stop_read,
	.f_stop_write = &umidi_stop_write,
	.f_open = &umidi_open,
	.f_close = &umidi_close,
	.f_ioctl = &umidi_ioctl,
	.basename[0] = "umidi",
};

static int32_t
umidi_probe(device_t dev)
{
	struct uaudio_softc *sc = device_get_softc(dev);
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct umidi_chan *chan = &sc->sc_midi_chan;
	struct umidi_sub_chan *sub;
	int unit = device_get_unit(dev);
	int error;
	uint32_t n;

	if (usbd_set_alt_interface_index(sc->sc_udev, chan->iface_index,
	    chan->iface_alt_index)) {
		DPRINTF("setting of alternate index failed!\n");
		goto detach;
	}
	usbd_set_parent_iface(sc->sc_udev, chan->iface_index, sc->sc_mixer_iface_index);

	error = usbd_transfer_setup(uaa->device, &chan->iface_index,
	    chan->xfer, umidi_config, UMIDI_N_TRANSFER,
	    chan, &chan->mtx);
	if (error) {
		DPRINTF("error=%s\n", usbd_errstr(error));
		goto detach;
	}
	if ((chan->max_cable > UMIDI_CABLES_MAX) ||
	    (chan->max_cable == 0)) {
		chan->max_cable = UMIDI_CABLES_MAX;
	}

	for (n = 0; n < chan->max_cable; n++) {

		sub = &chan->sub[n];

		error = usb_fifo_attach(sc->sc_udev, chan, &chan->mtx,
		    &umidi_fifo_methods, &sub->fifo, unit, n,
		    chan->iface_index,
		    UID_ROOT, GID_OPERATOR, 0644);
		if (error) {
			goto detach;
		}
	}

	mtx_lock(&chan->mtx);

	/* clear stall first */
	chan->flags |= UMIDI_FLAG_READ_STALL;

	/*
	 * NOTE: at least one device will not work properly unless
	 * the BULK pipe is open all the time.
	 */
	usbd_transfer_start(chan->xfer[1]);

	mtx_unlock(&chan->mtx);

	return (0);			/* success */

detach:
	return (ENXIO);			/* failure */
}

static int32_t
umidi_detach(device_t dev)
{
	struct uaudio_softc *sc = device_get_softc(dev);
	struct umidi_chan *chan = &sc->sc_midi_chan;
	uint32_t n;

	for (n = 0; n < UMIDI_CABLES_MAX; n++) {
		usb_fifo_detach(&chan->sub[n].fifo);
	}

	mtx_lock(&chan->mtx);

	usbd_transfer_stop(chan->xfer[3]);
	usbd_transfer_stop(chan->xfer[1]);

	mtx_unlock(&chan->mtx);

	usbd_transfer_unsetup(chan->xfer, UMIDI_N_TRANSFER);

	mtx_destroy(&chan->mtx);

	return (0);
}

DRIVER_MODULE(uaudio, uhub, uaudio_driver, uaudio_devclass, NULL, 0);
MODULE_DEPEND(uaudio, usb, 1, 1, 1);
MODULE_DEPEND(uaudio, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(uaudio, 1);
