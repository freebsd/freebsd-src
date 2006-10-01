/*-
 * Copyright (c) 2006 Stephane E. Potvin <sepotvin@videotron.ca>
 * Copyright (c) 2006 Ariff Abdullah <ariff@FreeBSD.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Intel High Definition Audio (Controller) driver for FreeBSD. Be advised
 * that this driver still in its early stage, and possible of rewrite are
 * pretty much guaranteed. There are supposedly several distinct parent/child
 * busses to make this "perfect", but as for now and for the sake of
 * simplicity, everything is gobble up within single source.
 *
 * List of subsys:
 *     1) HDA Controller support
 *     2) HDA Codecs support, which may include
 *        - HDA
 *        - Modem
 *        - HDMI
 *     3) Widget parser - the real magic of why this driver works on so
 *        many hardwares with minimal vendor specific quirk. The original
 *        parser was written using Ruby and can be found at
 *        http://people.freebsd.org/~ariff/HDA/parser.rb . This crude
 *        ruby parser take the verbose dmesg dump as its input. Refer to
 *        http://www.microsoft.com/whdc/device/audio/default.mspx for various
 *        interesting documents, especiall UAA (Universal Audio Architecture).
 *     4) Possible vendor specific support.
 *        (snd_hda_intel, snd_hda_ati, etc..)
 *
 * Thanks to Ahmad Ubaidah Omar @ Defenxis Sdn. Bhd. for the
 * Compaq V3000 with Conexant HDA.
 *
 *    * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *    *                                                                 *
 *    *        This driver is a collaborative effort made by:           *
 *    *                                                                 *
 *    *          Stephane E. Potvin <sepotvin@videotron.ca>             *
 *    *               Andrea Bittau <a.bittau@cs.ucl.ac.uk>             *
 *    *               Wesley Morgan <morganw@chemikals.org>             *
 *    *              Daniel Eischen <deischen@FreeBSD.org>              *
 *    *             Maxime Guillaud <bsd-ports@mguillaud.net>           *
 *    *              Ariff Abdullah <ariff@FreeBSD.org>                 *
 *    *                                                                 *
 *    *   ....and various people from freebsd-multimedia@FreeBSD.org    *
 *    *                                                                 *
 *    * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

#include <dev/sound/pcm/sound.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/sound/pci/hda/hdac_private.h>
#include <dev/sound/pci/hda/hdac_reg.h>
#include <dev/sound/pci/hda/hda_reg.h>
#include <dev/sound/pci/hda/hdac.h>

#include "mixer_if.h"

#define HDA_DRV_TEST_REV	"20061001_0028"
#define HDA_WIDGET_PARSER_REV	1

SND_DECLARE_FILE("$FreeBSD$");

#undef HDA_DEBUG_ENABLED
#define HDA_DEBUG_ENABLED	1

#ifdef HDA_DEBUG_ENABLED
#define HDA_DEBUG_MSG(stmt)	do {	\
	if (bootverbose) {		\
		stmt			\
	}				\
} while(0)
#else
#define HDA_DEBUG_MSG(stmt)
#endif

#define HDA_BOOTVERBOSE_MSG(stmt)	do {	\
	if (bootverbose) {			\
		stmt				\
	}					\
} while(0)

#if 0
#undef HDAC_INTR_EXTRA
#define HDAC_INTR_EXTRA		1
#endif

#define hdac_lock(sc)	snd_mtxlock((sc)->lock)
#define hdac_unlock(sc)	snd_mtxunlock((sc)->lock)

#define HDA_MODEL_CONSTRUCT(vendor, model)	\
		(((uint32_t)(model) << 16) | ((vendor##_VENDORID) & 0xffff))

/* Controller models */

/* Intel */
#define INTEL_VENDORID		0x8086
#define HDA_INTEL_82801F	HDA_MODEL_CONSTRUCT(INTEL, 0x2668)
#define HDA_INTEL_82801G	HDA_MODEL_CONSTRUCT(INTEL, 0x27d8)
#define HDA_INTEL_ALL		HDA_MODEL_CONSTRUCT(INTEL, 0xffff)

/* Nvidia */
#define NVIDIA_VENDORID		0x10de
#define HDA_NVIDIA_MCP51	HDA_MODEL_CONSTRUCT(NVIDIA, 0x026c)
#define HDA_NVIDIA_MCP55	HDA_MODEL_CONSTRUCT(NVIDIA, 0x0371)
#define HDA_NVIDIA_ALL		HDA_MODEL_CONSTRUCT(NVIDIA, 0xffff)

/* ATI */
#define ATI_VENDORID		0x1002
#define HDA_ATI_SB450		HDA_MODEL_CONSTRUCT(ATI, 0x437b)
#define HDA_ATI_ALL		HDA_MODEL_CONSTRUCT(ATI, 0xffff)

/* OEM/subvendors */

/* HP/Compaq */
#define HP_VENDORID		0x103c
#define HP_V3000_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30b5)
#define HP_NX7400_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30a2)
#define HP_NX6310_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30aa)
#define HP_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0xffff)

/* Dell */
#define DELL_VENDORID		0x1028
#define DELL_D820_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01cc)
#define DELL_I1300_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01c9)
#define DELL_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0xffff)

/* Clevo */
#define CLEVO_VENDORID		0x1558
#define CLEVO_D900T_SUBVENDOR	HDA_MODEL_CONSTRUCT(CLEVO, 0x0900)
#define CLEVO_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(CLEVO, 0xffff)

/* Acer */
#define ACER_VENDORID		0x1025
#define ACER_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0xffff)


/* Misc constants.. */
#define HDA_AMP_MUTE_DEFAULT	(0xffffffff)
#define HDA_AMP_MUTE_NONE	(0)
#define HDA_AMP_MUTE_LEFT	(1 << 0)
#define HDA_AMP_MUTE_RIGHT	(1 << 1)
#define HDA_AMP_MUTE_ALL	(HDA_AMP_MUTE_LEFT | HDA_AMP_MUTE_RIGHT)

#define HDA_AMP_LEFT_MUTED(v)	((v) & (HDA_AMP_MUTE_LEFT))
#define HDA_AMP_RIGHT_MUTED(v)	(((v) & HDA_AMP_MUTE_RIGHT) >> 1)

#define HDA_DAC_PATH	(1 << 0)
#define HDA_ADC_PATH	(1 << 1)
#define HDA_ADC_RECSEL	(1 << 2)

#define HDA_CTL_OUT	0x1
#define HDA_CTL_IN	0x2
#define HDA_CTL_BOTH	(HDA_CTL_IN | HDA_CTL_OUT)

#define HDA_QUIRK_GPIO1		(1 << 0)
#define HDA_QUIRK_GPIO2		(1 << 1)
#define HDA_QUIRK_SOFTPCMVOL	(1 << 2)
#define HDA_QUIRK_FIXEDRATE	(1 << 3)
#define HDA_QUIRK_FORCESTEREO	(1 << 4)

#define HDA_BDL_MIN	2
#define HDA_BDL_MAX	256
#define HDA_BDL_DEFAULT	HDA_BDL_MIN

#define HDA_BUFSZ_MIN		4096
#define HDA_BUFSZ_MAX		65536
#define HDA_BUFSZ_DEFAULT	16384

#define HDA_PARSE_MAXDEPTH	10

#define HDAC_UNSOLTAG_EVENT_HP	0x00

static MALLOC_DEFINE(M_HDAC, "hdac", "High Definition Audio Controller");

enum {
	HDA_PARSE_MIXER,
	HDA_PARSE_DIRECT
};

/* Default */
static uint32_t hdac_fmt[] = {
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static struct pcmchan_caps hdac_caps = {48000, 48000, hdac_fmt, 0};

static const struct {
	uint32_t	model;
	char		*desc;
} hdac_devices[] = {
	{ HDA_INTEL_82801F,  "Intel 82801F" },
	{ HDA_INTEL_82801G,  "Intel 82801G" },
	{ HDA_NVIDIA_MCP51,  "NVidia MCP51" },
	{ HDA_NVIDIA_MCP55,  "NVidia MCP55" },
	{ HDA_ATI_SB450,     "ATI SB450"    },
	/* Unknown */
	{ HDA_INTEL_ALL,  "Intel (Unknown)"  },
	{ HDA_NVIDIA_ALL, "NVidia (Unknown)" },
	{ HDA_ATI_ALL,    "ATI (Unknown)"    },
};
#define HDAC_DEVICES_LEN (sizeof(hdac_devices) / sizeof(hdac_devices[0]))

static const struct {
	uint32_t	rate;
	int		valid;
	uint16_t	base;
	uint16_t	mul;
	uint16_t	div;
} hda_rate_tab[] = {
	{   8000, 1, 0x0000, 0x0000, 0x0500 },	/* (48000 * 1) / 6 */
	{   9600, 0, 0x0000, 0x0000, 0x0400 },	/* (48000 * 1) / 5 */
	{  12000, 0, 0x0000, 0x0000, 0x0300 },	/* (48000 * 1) / 4 */
	{  16000, 1, 0x0000, 0x0000, 0x0200 },	/* (48000 * 1) / 3 */
	{  18000, 0, 0x0000, 0x1000, 0x0700 },	/* (48000 * 3) / 8 */
	{  19200, 0, 0x0000, 0x0800, 0x0400 },	/* (48000 * 2) / 5 */
	{  24000, 0, 0x0000, 0x0000, 0x0100 },	/* (48000 * 1) / 2 */
	{  28800, 0, 0x0000, 0x1000, 0x0400 },	/* (48000 * 3) / 5 */
	{  32000, 1, 0x0000, 0x0800, 0x0200 },	/* (48000 * 2) / 3 */
	{  36000, 0, 0x0000, 0x1000, 0x0300 },	/* (48000 * 3) / 4 */
	{  38400, 0, 0x0000, 0x1800, 0x0400 },	/* (48000 * 4) / 5 */
	{  48000, 1, 0x0000, 0x0000, 0x0000 },	/* (48000 * 1) / 1 */
	{  64000, 0, 0x0000, 0x1800, 0x0200 },	/* (48000 * 4) / 3 */
	{  72000, 0, 0x0000, 0x1000, 0x0100 },	/* (48000 * 3) / 2 */
	{  96000, 1, 0x0000, 0x0800, 0x0000 },	/* (48000 * 2) / 1 */
	{ 144000, 0, 0x0000, 0x1000, 0x0000 },	/* (48000 * 3) / 1 */
	{ 192000, 1, 0x0000, 0x1800, 0x0000 },	/* (48000 * 4) / 1 */
	{   8820, 0, 0x4000, 0x0000, 0x0400 },	/* (44100 * 1) / 5 */
	{  11025, 1, 0x4000, 0x0000, 0x0300 },	/* (44100 * 1) / 4 */
	{  12600, 0, 0x4000, 0x0800, 0x0600 },	/* (44100 * 2) / 7 */
	{  14700, 0, 0x4000, 0x0000, 0x0200 },	/* (44100 * 1) / 3 */
	{  17640, 0, 0x4000, 0x0800, 0x0400 },	/* (44100 * 2) / 5 */
	{  18900, 0, 0x4000, 0x1000, 0x0600 },	/* (44100 * 3) / 7 */
	{  22050, 1, 0x4000, 0x0000, 0x0100 },	/* (44100 * 1) / 2 */
	{  25200, 0, 0x4000, 0x1800, 0x0600 },	/* (44100 * 4) / 7 */
	{  26460, 0, 0x4000, 0x1000, 0x0400 },	/* (44100 * 3) / 5 */
	{  29400, 0, 0x4000, 0x0800, 0x0200 },	/* (44100 * 2) / 3 */
	{  33075, 0, 0x4000, 0x1000, 0x0300 },	/* (44100 * 3) / 4 */
	{  35280, 0, 0x4000, 0x1800, 0x0400 },	/* (44100 * 4) / 5 */
	{  44100, 1, 0x4000, 0x0000, 0x0000 },	/* (44100 * 1) / 1 */
	{  58800, 0, 0x4000, 0x1800, 0x0200 },	/* (44100 * 4) / 3 */
	{  66150, 0, 0x4000, 0x1000, 0x0100 },	/* (44100 * 3) / 2 */
	{  88200, 1, 0x4000, 0x0800, 0x0000 },	/* (44100 * 2) / 1 */
	{ 132300, 0, 0x4000, 0x1000, 0x0000 },	/* (44100 * 3) / 1 */
	{ 176400, 1, 0x4000, 0x1800, 0x0000 },	/* (44100 * 4) / 1 */
};
#define HDA_RATE_TAB_LEN (sizeof(hda_rate_tab) / sizeof(hda_rate_tab[0]))

/* All codecs you can eat... */
#define HDA_CODEC_CONSTRUCT(vendor, id) \
		(((uint32_t)(vendor##_VENDORID) << 16) | ((id) & 0xffff))

/* Realtek */
#define REALTEK_VENDORID	0x10ec
#define HDA_CODEC_ALC260	HDA_CODEC_CONSTRUCT(REALTEK, 0x0260)
#define HDA_CODEC_ALC861	HDA_CODEC_CONSTRUCT(REALTEK, 0x0861)
#define HDA_CODEC_ALC880	HDA_CODEC_CONSTRUCT(REALTEK, 0x0880)
#define HDA_CODEC_ALC882	HDA_CODEC_CONSTRUCT(REALTEK, 0x0260)
#define HDA_CODEC_ALCXXXX	HDA_CODEC_CONSTRUCT(REALTEK, 0xffff)

/* Analog Device */
#define ANALOGDEVICE_VENDORID	0x11d4
#define HDA_CODEC_AD1981HD	HDA_CODEC_CONSTRUCT(ANALOGDEVICE, 0x1981)
#define HDA_CODEC_AD1983	HDA_CODEC_CONSTRUCT(ANALOGDEVICE, 0x1983)
#define HDA_CODEC_AD1986A	HDA_CODEC_CONSTRUCT(ANALOGDEVICE, 0x1986)
#define HDA_CODEC_ADXXXX	HDA_CODEC_CONSTRUCT(ANALOGDEVICE, 0xffff)

/* CMedia */
#define CMEDIA_VENDORID		0x434d
#define HDA_CODEC_CMI9880	HDA_CODEC_CONSTRUCT(CMEDIA, 0x4980)
#define HDA_CODEC_CMIXXXX	HDA_CODEC_CONSTRUCT(CMEDIA, 0xffff)

/* Sigmatel */
#define SIGMATEL_VENDORID	0x8384
#define HDA_CODEC_STAC9221	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7680)
#define HDA_CODEC_STAC9221D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7683)
#define HDA_CODEC_STAC9220	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7690)
#define HDA_CODEC_STAC922XD	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7681)
#define HDA_CODEC_STACXXXX	HDA_CODEC_CONSTRUCT(SIGMATEL, 0xffff)

/*
 * Conexant
 *
 * Ok, the truth is, I don't have any idea at all whether
 * it is "Venice" or "Waikiki" or other unnamed CXyadayada. The only
 * place that tell me it is "Venice" is from its Windows driver INF.
 */
#define CONEXANT_VENDORID	0x14f1
#define HDA_CODEC_CXVENICE	HDA_CODEC_CONSTRUCT(CONEXANT, 0x5045)
#define HDA_CODEC_CXXXXX	HDA_CODEC_CONSTRUCT(CONEXANT, 0xffff)


/* Codecs */
static const struct {
	uint32_t id;
	char *name;
} hdac_codecs[] = {
	{ HDA_CODEC_ALC260,    "Realtek ALC260" },
	{ HDA_CODEC_ALC861,    "Realtek ALC861" },
	{ HDA_CODEC_ALC880,    "Realtek ALC880" },
	{ HDA_CODEC_ALC882,    "Realtek ALC882" },
	{ HDA_CODEC_AD1981HD,  "Analog Device AD1981HD" },
	{ HDA_CODEC_AD1983,    "Analog Device AD1983" },
	{ HDA_CODEC_AD1986A,   "Analog Device AD1986A" },
	{ HDA_CODEC_CMI9880,   "CMedia CMI9880" },
	{ HDA_CODEC_STAC9221,  "Sigmatel STAC9221" },
	{ HDA_CODEC_STAC9221D, "Sigmatel STAC9221D" },
	{ HDA_CODEC_STAC9220,  "Sigmatel STAC9220" },
	{ HDA_CODEC_STAC922XD, "Sigmatel STAC9220D/9223D" },
	{ HDA_CODEC_CXVENICE,  "Conexant Venice" },
	/* Unknown codec */
	{ HDA_CODEC_ALCXXXX,   "Realtek (Unknown)" },
	{ HDA_CODEC_ADXXXX,    "Analog Device (Unknown)" },
	{ HDA_CODEC_CMIXXXX,   "CMedia (Unknown)" },
	{ HDA_CODEC_STACXXXX,  "Sigmatel (Unknown)" },
	{ HDA_CODEC_CXXXXX,    "Conexant (Unknown)" },
};
#define HDAC_CODECS_LEN	(sizeof(hdac_codecs) / sizeof(hdac_codecs[0]))

enum {
	HDAC_HP_SWITCH_CTL,
	HDAC_HP_SWITCH_CTRL
};

static const struct {
	uint32_t vendormask;
	uint32_t id;
	int type;
	nid_t hpnid;
	nid_t spkrnid[8];
	nid_t eapdnid;
} hdac_hp_switch[] = {
	/* Specific OEM models */
	{ HP_V3000_SUBVENDOR,  HDA_CODEC_CXVENICE, HDAC_HP_SWITCH_CTL,
	    17, { 16, -1 }, 16 },
	{ HP_NX7400_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	     6, {  5, -1 },  5 },
	{ HP_NX6310_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	     6, {  5, -1 },  5 },
	{ DELL_D820_SUBVENDOR, HDA_CODEC_STAC9220, HDAC_HP_SWITCH_CTRL,
	    13, { 14, -1 }, -1 },
	{ DELL_I1300_SUBVENDOR, HDA_CODEC_STAC9220, HDAC_HP_SWITCH_CTRL,
	    13, { 14, -1 }, -1 },
	/*
	 * All models that at least come from the same vendor with
	 * simmilar codec.
	 */
	{ HP_ALL_SUBVENDOR,  HDA_CODEC_CXVENICE, HDAC_HP_SWITCH_CTL,
	    17, { 16, -1 }, 16 },
	{ HP_ALL_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	     6, {  5, -1 },  5 },
	{ DELL_ALL_SUBVENDOR, HDA_CODEC_STAC9220, HDAC_HP_SWITCH_CTRL,
	    13, { 14, -1 }, -1 },
};
#define HDAC_HP_SWITCH_LEN	\
		(sizeof(hdac_hp_switch) / sizeof(hdac_hp_switch[0]))

static const struct {
	uint32_t vendormask;
	uint32_t id;
	nid_t eapdnid;
	int hp_switch;
} hdac_eapd_switch[] = {
	{ HP_V3000_SUBVENDOR, HDA_CODEC_CXVENICE, 16, 1 },
	{ HP_NX7400_SUBVENDOR, HDA_CODEC_AD1981HD, 5, 1 },
	{ HP_NX6310_SUBVENDOR, HDA_CODEC_AD1981HD, 5, 1 },
};
#define HDAC_EAPD_SWITCH_LEN	\
		(sizeof(hdac_eapd_switch) / sizeof(hdac_eapd_switch[0]))

/****************************************************************************
 * Function prototypes
 ****************************************************************************/
static void	hdac_intr_handler(void *);
static int	hdac_reset(struct hdac_softc *);
static int	hdac_get_capabilities(struct hdac_softc *);
static void	hdac_dma_cb(void *, bus_dma_segment_t *, int, int);
static int	hdac_dma_alloc(struct hdac_softc *,
					struct hdac_dma *, bus_size_t);
static void	hdac_dma_free(struct hdac_dma *);
static int	hdac_mem_alloc(struct hdac_softc *);
static void	hdac_mem_free(struct hdac_softc *);
static int	hdac_irq_alloc(struct hdac_softc *);
static void	hdac_irq_free(struct hdac_softc *);
static void	hdac_corb_init(struct hdac_softc *);
static void	hdac_rirb_init(struct hdac_softc *);
static void	hdac_corb_start(struct hdac_softc *);
static void	hdac_rirb_start(struct hdac_softc *);
static void	hdac_scan_codecs(struct hdac_softc *);
static int	hdac_probe_codec(struct hdac_codec *);
static struct	hdac_devinfo *hdac_probe_function(struct hdac_codec *, nid_t);
static void	hdac_add_child(struct hdac_softc *, struct hdac_devinfo *);

static void	hdac_attach2(void *);

static uint32_t	hdac_command_sendone_internal(struct hdac_softc *,
							uint32_t, int);
static void	hdac_command_send_internal(struct hdac_softc *,
					struct hdac_command_list *, int);

static int	hdac_probe(device_t);
static int	hdac_attach(device_t);
static int	hdac_detach(device_t);
static void	hdac_widget_connection_select(struct hdac_widget *, uint8_t);
static void	hdac_audio_ctl_amp_set(struct hdac_audio_ctl *,
						uint32_t, int, int);
static struct	hdac_audio_ctl *hdac_audio_ctl_amp_get(struct hdac_devinfo *,
							nid_t, int, int);
static void	hdac_audio_ctl_amp_set_internal(struct hdac_softc *,
				nid_t, nid_t, int, int, int, int, int, int);
static int	hdac_audio_ctl_ossmixer_getnextdev(struct hdac_devinfo *);
static struct	hdac_widget *hdac_widget_get(struct hdac_devinfo *, nid_t);

#define hdac_command(a1, a2, a3)	\
		hdac_command_sendone_internal(a1, a2, a3)

#define hdac_codec_id(d)						\
		((uint32_t)((d == NULL) ? 0x00000000 :			\
		((((uint32_t)(d)->vendor_id & 0x0000ffff) << 16) |	\
		((uint32_t)(d)->device_id & 0x0000ffff))))

static char *
hdac_codec_name(struct hdac_devinfo *devinfo)
{
	uint32_t id;
	int i;

	id = hdac_codec_id(devinfo);

	for (i = 0; i < HDAC_CODECS_LEN; i++) {
		if ((hdac_codecs[i].id & id) == id)
			return (hdac_codecs[i].name);
	}

	return ((id == 0x00000000) ? "NULL Codec" : "Unknown Codec");
}

static char *
hdac_audio_ctl_ossmixer_mask2name(uint32_t devmask)
{
	static char *ossname[] = SOUND_DEVICE_NAMES;
	static char *unknown = "???";
	int i;

	for (i = SOUND_MIXER_NRDEVICES - 1; i >= 0; i--) {
		if (devmask & (1 << i))
			return (ossname[i]);
	}
	return (unknown);
}

static void
hdac_audio_ctl_ossmixer_mask2allname(uint32_t mask, char *buf, size_t len)
{
	static char *ossname[] = SOUND_DEVICE_NAMES;
	int i, first = 1;

	bzero(buf, len);
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (mask & (1 << i)) {
			if (first == 0)
				strlcat(buf, ", ", len);
			strlcat(buf, ossname[i], len);
			first = 0;
		}
	}
}

static struct hdac_audio_ctl *
hdac_audio_ctl_each(struct hdac_devinfo *devinfo, int *index)
{
	if (devinfo == NULL ||
	    devinfo->node_type != HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO ||
	    index == NULL || devinfo->function.audio.ctl == NULL ||
	    devinfo->function.audio.ctlcnt < 1 ||
	    *index < 0 || *index >= devinfo->function.audio.ctlcnt)
		return (NULL);
	return (&devinfo->function.audio.ctl[(*index)++]);
}

static struct hdac_audio_ctl *
hdac_audio_ctl_amp_get(struct hdac_devinfo *devinfo, nid_t nid,
						int index, int cnt)
{
	struct hdac_audio_ctl *ctl, *retctl = NULL;
	int i, at, atindex, found = 0;

	if (devinfo == NULL || devinfo->function.audio.ctl == NULL)
		return (NULL);

	at = cnt;
	if (at == 0)
		at = 1;
	else if (at < 0)
		at = -1;
	atindex = index;
	if (atindex < 0)
		atindex = -1;

	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0 || ctl->widget == NULL)
			continue;
		if (!(ctl->widget->nid == nid && (atindex == -1 ||
		    ctl->index == atindex)))
			continue;
		found++;
		if (found == cnt)
			return (ctl);
		retctl = ctl;
	}

	return ((at == -1) ? retctl : NULL);
}

static void
hdac_hp_switch_handler(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc;
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	uint32_t id, res;
	int i = 0, j, forcemute;
	nid_t cad;

	if (devinfo == NULL || devinfo->codec == NULL ||
	    devinfo->codec->sc == NULL)
		return;

	sc = devinfo->codec->sc;
	cad = devinfo->codec->cad;
	id = hdac_codec_id(devinfo);
	for (i = 0; i < HDAC_HP_SWITCH_LEN; i++) {
		if ((hdac_hp_switch[i].vendormask & sc->pci_subvendor) ==
		    sc->pci_subvendor &&
		    hdac_hp_switch[i].id == id)
			break;
	}

	if (i >= HDAC_HP_SWITCH_LEN)
		return;

	forcemute = 0;
	if (hdac_hp_switch[i].eapdnid != -1) {
		w = hdac_widget_get(devinfo, hdac_hp_switch[i].eapdnid);
		if (w != NULL && w->param.eapdbtl != 0xffffffff)
			forcemute = (w->param.eapdbtl &
			    HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD) ? 0 : 1;
	}

	res = hdac_command(sc,
	    HDA_CMD_GET_PIN_SENSE(cad, hdac_hp_switch[i].hpnid), cad);
	HDA_BOOTVERBOSE_MSG(
		device_printf(sc->dev,
		    "Pin sense: nid=%d res=0x%08x\n",
		    hdac_hp_switch[i].hpnid, res);
	);
	res >>= 31;

	switch (hdac_hp_switch[i].type) {
	case HDAC_HP_SWITCH_CTL:
		ctl = hdac_audio_ctl_amp_get(devinfo,
		    hdac_hp_switch[i].hpnid, 0, 1);
		if (ctl != NULL) {
			ctl->muted = (res != 0 && forcemute == 0) ?
			    HDA_AMP_MUTE_NONE : HDA_AMP_MUTE_ALL;
			hdac_audio_ctl_amp_set(ctl,
			    HDA_AMP_MUTE_DEFAULT, ctl->left,
			    ctl->right);
		}
		for (j = 0; hdac_hp_switch[i].spkrnid[j] != -1; j++) {
			ctl = hdac_audio_ctl_amp_get(devinfo,
			    hdac_hp_switch[i].spkrnid[j], 0, 1);
			if (ctl != NULL) {
				ctl->muted = (res != 0 || forcemute == 1) ?
				    HDA_AMP_MUTE_ALL : HDA_AMP_MUTE_NONE;
				hdac_audio_ctl_amp_set(ctl,
				    HDA_AMP_MUTE_DEFAULT, ctl->left,
				    ctl->right);
			}
		}
		break;
	case HDAC_HP_SWITCH_CTRL:
		if (res != 0) {
			/* HP in */
			w = hdac_widget_get(devinfo, hdac_hp_switch[i].hpnid);
			if (w != NULL) {
				if (forcemute == 0)
					w->wclass.pin.ctrl |=
					    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				else
					w->wclass.pin.ctrl &=
					    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				hdac_command(sc,
				    HDA_CMD_SET_PIN_WIDGET_CTRL(cad, w->nid,
				    w->wclass.pin.ctrl), cad);
			}
			for (j = 0; hdac_hp_switch[i].spkrnid[j] != -1; j++) {
				w = hdac_widget_get(devinfo,
				    hdac_hp_switch[i].spkrnid[j]);
				if (w != NULL) {
					w->wclass.pin.ctrl &=
					    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
					hdac_command(sc,
					    HDA_CMD_SET_PIN_WIDGET_CTRL(cad,
					    w->nid,
					    w->wclass.pin.ctrl), cad);
				}
			}
		} else {
			/* HP out */
			w = hdac_widget_get(devinfo, hdac_hp_switch[i].hpnid);
			if (w != NULL) {
				w->wclass.pin.ctrl &=
				    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				hdac_command(sc,
				    HDA_CMD_SET_PIN_WIDGET_CTRL(cad, w->nid,
				    w->wclass.pin.ctrl), cad);
			}
			for (j = 0; hdac_hp_switch[i].spkrnid[j] != -1; j++) {
				w = hdac_widget_get(devinfo,
				    hdac_hp_switch[i].spkrnid[j]);
				if (w != NULL) {
					if (forcemute == 0)
						w->wclass.pin.ctrl |=
						    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
					else
						w->wclass.pin.ctrl &=
						    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
					hdac_command(sc,
					    HDA_CMD_SET_PIN_WIDGET_CTRL(cad,
					    w->nid,
					    w->wclass.pin.ctrl), cad);
				}
			}
		}
		break;
	default:
		break;
	}
}

static void
hdac_unsolicited_handler(struct hdac_codec *codec, uint32_t tag)
{
	struct hdac_softc *sc;
	struct hdac_devinfo *devinfo = NULL;
	device_t *devlist;
	int devcount, i;

	if (codec == NULL || codec->sc == NULL)
		return;

	sc = codec->sc;

	HDA_BOOTVERBOSE_MSG(
		device_printf(sc->dev, "Unsol Tag: 0x%08x\n", tag);
	);

	device_get_children(sc->dev, &devlist, &devcount);
	if (devcount != 0 && devlist != NULL) {
		for (i = 0; i < devcount; i++) {
			devinfo = (struct hdac_devinfo *)
			    device_get_ivars(devlist[i]);
			if (devinfo != NULL &&
			    devinfo->node_type ==
			    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO &&
			    devinfo->codec != NULL &&
			    devinfo->codec->cad == codec->cad) {
				break;
			} else
				devinfo = NULL;
		}
	}
	if (devinfo == NULL)
		return;

	switch (tag) {
	case HDAC_UNSOLTAG_EVENT_HP:
		hdac_hp_switch_handler(devinfo);
		break;
	default:
		break;
	}
}

static void
hdac_stream_intr(struct hdac_softc *sc, struct hdac_chan *ch)
{
	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	uint32_t res;
#endif

	if (ch->blkcnt == 0)
		return;

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	res = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDSTS);
#endif

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	if ((res & HDAC_SDSTS_DESE) || (res & HDAC_SDSTS_FIFOE))
		device_printf(sc->dev,
		    "PCMDIR_%s intr triggered beyond stream boundary: %08x\n",
		    (ch->dir == PCMDIR_PLAY) ? "PLAY" : "REC", res);
#endif

	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDSTS,
		     HDAC_SDSTS_DESE | HDAC_SDSTS_FIFOE | HDAC_SDSTS_BCIS );

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	if (res & HDAC_SDSTS_BCIS) {
#endif
		ch->prevptr = ch->ptr;
		ch->ptr += sndbuf_getblksz(ch->b);
		ch->ptr %= sndbuf_getsize(ch->b);
		hdac_unlock(sc);
		chn_intr(ch->c);
		hdac_lock(sc);
	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	}
#endif
}

/****************************************************************************
 * void hdac_intr_handler(void *)
 *
 * Interrupt handler. Processes interrupts received from the hdac.
 ****************************************************************************/
static void
hdac_intr_handler(void *context)
{
	struct hdac_softc *sc;
	uint32_t intsts;
	uint8_t rirbsts;
	uint8_t rirbwp;
	struct hdac_rirb *rirb_base, *rirb;
	nid_t ucad;
	uint32_t utag;

	sc = (struct hdac_softc *)context;

	hdac_lock(sc);
	/* Do we have anything to do? */
	intsts = HDAC_READ_4(&sc->mem, HDAC_INTSTS);
	if ((intsts & HDAC_INTSTS_GIS) != HDAC_INTSTS_GIS) {
		hdac_unlock(sc);
		return;
	}

	/* Was this a controller interrupt? */
	if ((intsts & HDAC_INTSTS_CIS) == HDAC_INTSTS_CIS) {
		rirb_base = (struct hdac_rirb *)sc->rirb_dma.dma_vaddr;
		rirbsts = HDAC_READ_1(&sc->mem, HDAC_RIRBSTS);
		/* Get as many responses that we can */
		while ((rirbsts & HDAC_RIRBSTS_RINTFL) == HDAC_RIRBSTS_RINTFL) {
			HDAC_WRITE_1(&sc->mem, HDAC_RIRBSTS, HDAC_RIRBSTS_RINTFL);
			rirbwp = HDAC_READ_1(&sc->mem, HDAC_RIRBWP);
			bus_dmamap_sync(sc->rirb_dma.dma_tag, sc->rirb_dma.dma_map,
			    BUS_DMASYNC_POSTREAD);
			while (sc->rirb_rp != rirbwp) {
				sc->rirb_rp++;
				sc->rirb_rp %= sc->rirb_size;
				rirb = &rirb_base[sc->rirb_rp];
				if (rirb->response_ex & HDAC_RIRB_RESPONSE_EX_UNSOLICITED) {
					ucad = HDAC_RIRB_RESPONSE_EX_SDATA_IN(rirb->response_ex);
					utag = rirb->response >> 26;
					if (ucad > -1 && ucad < HDAC_CODEC_MAX &&
					    sc->codecs[ucad] != NULL) {
						sc->unsolq[sc->unsolq_wp++] =
						    (ucad << 16) |
						    (utag & 0xffff);
						sc->unsolq_wp %= HDAC_UNSOLQ_MAX;
					}
				}
			}
			rirbsts = HDAC_READ_1(&sc->mem, HDAC_RIRBSTS);
		}
		/* XXX to be removed */
		/* Clear interrupt and exit */
#ifdef HDAC_INTR_EXTRA
		HDAC_WRITE_4(&sc->mem, HDAC_INTSTS, HDAC_INTSTS_CIS);
#endif
	}
	if ((intsts & HDAC_INTSTS_SIS_MASK)) {
		if (intsts & (1 << sc->num_iss))
			hdac_stream_intr(sc, &sc->play);
		if (intsts & (1 << 0))
			hdac_stream_intr(sc, &sc->rec);
		/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
		HDAC_WRITE_4(&sc->mem, HDAC_INTSTS, intsts & HDAC_INTSTS_SIS_MASK);
#endif
	}

	if (sc->unsolq_st == HDAC_UNSOLQ_READY) {
		sc->unsolq_st = HDAC_UNSOLQ_BUSY;
		while (sc->unsolq_rp != sc->unsolq_wp) {
			ucad = sc->unsolq[sc->unsolq_rp] >> 16;
			utag = sc->unsolq[sc->unsolq_rp++] & 0xffff;
			sc->unsolq_rp %= HDAC_UNSOLQ_MAX;
			hdac_unsolicited_handler(sc->codecs[ucad], utag);
		}
		sc->unsolq_st = HDAC_UNSOLQ_READY;
	}

	hdac_unlock(sc);
}

/****************************************************************************
 * int had_reset(hdac_softc *)
 *
 * Reset the hdac to a quiescent and known state.
 ****************************************************************************/
static int
hdac_reset(struct hdac_softc *sc)
{
	uint32_t gctl;
	int count, i;

	/*
	 * Stop all Streams DMA engine
	 */
	for (i = 0; i < sc->num_iss; i++)
		HDAC_WRITE_4(&sc->mem, HDAC_ISDCTL(sc, i), 0x0);
	for (i = 0; i < sc->num_oss; i++)
		HDAC_WRITE_4(&sc->mem, HDAC_OSDCTL(sc, i), 0x0);
	for (i = 0; i < sc->num_bss; i++)
		HDAC_WRITE_4(&sc->mem, HDAC_BSDCTL(sc, i), 0x0);

	/*
	 * Stop Control DMA engines
	 */
	HDAC_WRITE_1(&sc->mem, HDAC_CORBCTL, 0x0);
	HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL, 0x0);

	/*
	 * Reset the controller. The reset must remain asserted for
	 * a minimum of 100us.
	 */
	gctl = HDAC_READ_4(&sc->mem, HDAC_GCTL);
	HDAC_WRITE_4(&sc->mem, HDAC_GCTL, gctl & ~HDAC_GCTL_CRST);
	count = 10000;
	do {
		gctl = HDAC_READ_4(&sc->mem, HDAC_GCTL);
		if (!(gctl & HDAC_GCTL_CRST))
			break;
		DELAY(10);
	} while	(--count);
	if (gctl & HDAC_GCTL_CRST) {
		device_printf(sc->dev, "Unable to put hdac in reset\n");
		return (ENXIO);
	}
	DELAY(100);
	gctl = HDAC_READ_4(&sc->mem, HDAC_GCTL);
	HDAC_WRITE_4(&sc->mem, HDAC_GCTL, gctl | HDAC_GCTL_CRST);
	count = 10000;
	do {
		gctl = HDAC_READ_4(&sc->mem, HDAC_GCTL);
		if ((gctl & HDAC_GCTL_CRST))
			break;
		DELAY(10);
	} while (--count);
	if (!(gctl & HDAC_GCTL_CRST)) {
		device_printf(sc->dev, "Device stuck in reset\n");
		return (ENXIO);
	}

	/*
	 * Enable unsolicited interrupt.
	 */
	gctl = HDAC_READ_4(&sc->mem, HDAC_GCTL);
	HDAC_WRITE_4(&sc->mem, HDAC_GCTL, gctl | HDAC_GCTL_UNSOL);

	/*
	 * Wait for codecs to finish their own reset sequence. The delay here
	 * should be of 250us but for some reasons, on it's not enough on my
	 * computer. Let's use twice as much as necessary to make sure that
	 * it's reset properly.
	 */
	DELAY(1000);

	return (0);
}


/****************************************************************************
 * int hdac_get_capabilities(struct hdac_softc *);
 *
 * Retreive the general capabilities of the hdac;
 *	Number of Input Streams
 *	Number of Output Streams
 *	Number of bidirectional Streams
 *	64bit ready
 *	CORB and RIRB sizes
 ****************************************************************************/
static int
hdac_get_capabilities(struct hdac_softc *sc)
{
	uint16_t gcap;
	uint8_t corbsize, rirbsize;

	gcap = HDAC_READ_2(&sc->mem, HDAC_GCAP);
	sc->num_iss = HDAC_GCAP_ISS(gcap);
	sc->num_oss = HDAC_GCAP_OSS(gcap);
	sc->num_bss = HDAC_GCAP_BSS(gcap);

	sc->support_64bit = (gcap & HDAC_GCAP_64OK) == HDAC_GCAP_64OK;

	corbsize = HDAC_READ_1(&sc->mem, HDAC_CORBSIZE);
	if ((corbsize & HDAC_CORBSIZE_CORBSZCAP_256) ==
	    HDAC_CORBSIZE_CORBSZCAP_256)
		sc->corb_size = 256;
	else if ((corbsize & HDAC_CORBSIZE_CORBSZCAP_16) ==
	    HDAC_CORBSIZE_CORBSZCAP_16)
		sc->corb_size = 16;
	else if ((corbsize & HDAC_CORBSIZE_CORBSZCAP_2) ==
	    HDAC_CORBSIZE_CORBSZCAP_2)
		sc->corb_size = 2;
	else {
		device_printf(sc->dev, "%s: Invalid corb size (%x)\n",
		    __func__, corbsize);
		return (ENXIO);
	}

	rirbsize = HDAC_READ_1(&sc->mem, HDAC_RIRBSIZE);
	if ((rirbsize & HDAC_RIRBSIZE_RIRBSZCAP_256) ==
	    HDAC_RIRBSIZE_RIRBSZCAP_256)
		sc->rirb_size = 256;
	else if ((rirbsize & HDAC_RIRBSIZE_RIRBSZCAP_16) ==
	    HDAC_RIRBSIZE_RIRBSZCAP_16)
		sc->rirb_size = 16;
	else if ((rirbsize & HDAC_RIRBSIZE_RIRBSZCAP_2) ==
	    HDAC_RIRBSIZE_RIRBSZCAP_2)
		sc->rirb_size = 2;
	else {
		device_printf(sc->dev, "%s: Invalid rirb size (%x)\n",
		    __func__, rirbsize);
		return (ENXIO);
	}

	return (0);
}


/****************************************************************************
 * void hdac_dma_cb
 *
 * This function is called by bus_dmamap_load when the mapping has been
 * established. We just record the physical address of the mapping into
 * the struct hdac_dma passed in.
 ****************************************************************************/
static void
hdac_dma_cb(void *callback_arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct hdac_dma *dma;

	if (error == 0) {
		dma = (struct hdac_dma *)callback_arg;
		dma->dma_paddr = segs[0].ds_addr;
	}
}

static void
hdac_dma_nocache(void *ptr)
{
	pt_entry_t *pte;
	vm_offset_t va;

	va = (vm_offset_t)ptr;
	pte = vtopte(va);
	if (pte)  {
		*pte |= PG_N;
		invltlb();
	}
}

/****************************************************************************
 * int hdac_dma_alloc
 *
 * This function allocate and setup a dma region (struct hdac_dma).
 * It must be freed by a corresponding hdac_dma_free.
 ****************************************************************************/
static int
hdac_dma_alloc(struct hdac_softc *sc, struct hdac_dma *dma, bus_size_t size)
{
	int result;
	int lowaddr;

	lowaddr = (sc->support_64bit) ? BUS_SPACE_MAXADDR :
	    BUS_SPACE_MAXADDR_32BIT;
	bzero(dma, sizeof(*dma));

	/*
	 * Create a DMA tag
	 */
	result = bus_dma_tag_create(NULL,	/* parent */
	    HDAC_DMA_ALIGNMENT,			/* alignment */
	    0,					/* boundary */
	    lowaddr,				/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL,				/* filtfunc */
	    NULL,				/* fistfuncarg */
	    size, 				/* maxsize */
	    1,					/* nsegments */
	    size, 				/* maxsegsz */
	    0,					/* flags */
	    NULL,				/* lockfunc */
	    NULL,				/* lockfuncarg */
	    &dma->dma_tag);			/* dmat */
	if (result != 0) {
		device_printf(sc->dev, "%s: bus_dma_tag_create failed (%x)\n",
		    __func__, result);
		goto fail;
	}

	/*
	 * Allocate DMA memory
	 */
	result = bus_dmamem_alloc(dma->dma_tag, (void **) &dma->dma_vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &dma->dma_map);
	if (result != 0) {
		device_printf(sc->dev, "%s: bus_dmamem_alloc failed (%x)\n",
		    __func__, result);
		goto fail;
	}

	/*
	 * Map the memory
	 */
	result = bus_dmamap_load(dma->dma_tag, dma->dma_map,
	    (void *)dma->dma_vaddr, size, hdac_dma_cb, (void *)dma,
	    BUS_DMA_NOWAIT);
	if (result != 0 || dma->dma_paddr == 0) {
		device_printf(sc->dev, "%s: bus_dmamem_load failed (%x)\n",
		    __func__, result);
		goto fail;
	}
	bzero((void *)dma->dma_vaddr, size);
	hdac_dma_nocache(dma->dma_vaddr);

	return (0);
fail:
	if (dma->dma_map != NULL)
		bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
	if (dma->dma_tag != NULL)
		bus_dma_tag_destroy(dma->dma_tag);
	return (result);
}


/****************************************************************************
 * void hdac_dma_free(struct hdac_dma *)
 *
 * Free a struct dhac_dma that has been previously allocated via the
 * hdac_dma_alloc function.
 ****************************************************************************/
static void
hdac_dma_free(struct hdac_dma *dma)
{
	if (dma->dma_tag != NULL) {
		/* Flush caches */
		bus_dmamap_sync(dma->dma_tag, dma->dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dma->dma_tag, dma->dma_map);
		bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
		bus_dma_tag_destroy(dma->dma_tag);
	}
}

/****************************************************************************
 * int hdac_mem_alloc(struct hdac_softc *)
 *
 * Allocate all the bus resources necessary to speak with the physical
 * controller.
 ****************************************************************************/
static int
hdac_mem_alloc(struct hdac_softc *sc)
{
	struct hdac_mem *mem;

	mem = &sc->mem;
	mem->mem_rid = PCIR_BAR(0);
	mem->mem_res = bus_alloc_resource_any(sc->dev, SYS_RES_MEMORY,
	    &mem->mem_rid, RF_ACTIVE);
	if (mem->mem_res == NULL) {
		device_printf(sc->dev,
		    "%s: Unable to allocate memory resource\n", __func__);
		return (ENOMEM);
	}
	mem->mem_tag = rman_get_bustag(mem->mem_res);
	mem->mem_handle = rman_get_bushandle(mem->mem_res);

	return (0);
}

/****************************************************************************
 * void hdac_mem_free(struct hdac_softc *)
 *
 * Free up resources previously allocated by hdac_mem_alloc.
 ****************************************************************************/
static void
hdac_mem_free(struct hdac_softc *sc)
{
	struct hdac_mem *mem;

	mem = &sc->mem;
	if (mem->mem_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY, mem->mem_rid,
		    mem->mem_res);
}

/****************************************************************************
 * int hdac_irq_alloc(struct hdac_softc *)
 *
 * Allocate and setup the resources necessary for interrupt handling.
 ****************************************************************************/
static int
hdac_irq_alloc(struct hdac_softc *sc)
{
	struct hdac_irq *irq;
	int result;

	irq = &sc->irq;
	irq->irq_rid = 0x0;
	irq->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ,
	    &irq->irq_rid, RF_SHAREABLE | RF_ACTIVE);
	if (irq->irq_res == NULL) {
		device_printf(sc->dev, "%s: Unable to allocate irq\n",
		    __func__);
		goto fail;
	}
	result = snd_setup_intr(sc->dev, irq->irq_res, INTR_MPSAFE,
		hdac_intr_handler, sc, &irq->irq_handle);
	if (result != 0) {
		device_printf(sc->dev,
		    "%s: Unable to setup interrupt handler (%x)\n",
		    __func__, result);
		goto fail;
	}

	return (0);

fail:
	if (irq->irq_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ, irq->irq_rid,
		    irq->irq_res);
	return (ENXIO);
}

/****************************************************************************
 * void hdac_irq_free(struct hdac_softc *)
 *
 * Free up resources previously allocated by hdac_irq_alloc.
 ****************************************************************************/
static void
hdac_irq_free(struct hdac_softc *sc)
{
	struct hdac_irq *irq;

	irq = &sc->irq;
	if (irq->irq_handle != NULL)
		bus_teardown_intr(sc->dev, irq->irq_res, irq->irq_handle);
	if (irq->irq_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ, irq->irq_rid,
		    irq->irq_res);
}

/****************************************************************************
 * void hdac_corb_init(struct hdac_softc *)
 *
 * Initialize the corb registers for operations but do not start it up yet.
 * The CORB engine must not be running when this function is called.
 ****************************************************************************/
static void
hdac_corb_init(struct hdac_softc *sc)
{
	uint8_t corbsize;
	uint64_t corbpaddr;

	/* Setup the CORB size. */
	switch (sc->corb_size) {
	case 256:
		corbsize = HDAC_CORBSIZE_CORBSIZE(HDAC_CORBSIZE_CORBSIZE_256);
		break;
	case 16:
		corbsize = HDAC_CORBSIZE_CORBSIZE(HDAC_CORBSIZE_CORBSIZE_16);
		break;
	case 2:
		corbsize = HDAC_CORBSIZE_CORBSIZE(HDAC_CORBSIZE_CORBSIZE_2);
		break;
	default:
		panic("%s: Invalid CORB size (%x)\n", __func__, sc->corb_size);
	}
	HDAC_WRITE_1(&sc->mem, HDAC_CORBSIZE, corbsize);

	/* Setup the CORB Address in the hdac */
	corbpaddr = (uint64_t)sc->corb_dma.dma_paddr;
	HDAC_WRITE_4(&sc->mem, HDAC_CORBLBASE, (uint32_t)corbpaddr);
	HDAC_WRITE_4(&sc->mem, HDAC_CORBUBASE, (uint32_t)(corbpaddr >> 32));

	/* Set the WP and RP */
	sc->corb_wp = 0;
	HDAC_WRITE_2(&sc->mem, HDAC_CORBWP, sc->corb_wp);
	HDAC_WRITE_2(&sc->mem, HDAC_CORBRP, HDAC_CORBRP_CORBRPRST);
	/*
	 * The HDA specification indicates that the CORBRPRST bit will always
	 * read as zero. Unfortunately, it seems that at least the 82801G
	 * doesn't reset the bit to zero, which stalls the corb engine.
	 * manually reset the bit to zero before continuing.
	 */
	HDAC_WRITE_2(&sc->mem, HDAC_CORBRP, 0x0);

	/* Enable CORB error reporting */
#if 0
	HDAC_WRITE_1(&sc->mem, HDAC_CORBCTL, HDAC_CORBCTL_CMEIE);
#endif
}

/****************************************************************************
 * void hdac_rirb_init(struct hdac_softc *)
 *
 * Initialize the rirb registers for operations but do not start it up yet.
 * The RIRB engine must not be running when this function is called.
 ****************************************************************************/
static void
hdac_rirb_init(struct hdac_softc *sc)
{
	uint8_t rirbsize;
	uint64_t rirbpaddr;

	/* Setup the RIRB size. */
	switch (sc->rirb_size) {
	case 256:
		rirbsize = HDAC_RIRBSIZE_RIRBSIZE(HDAC_RIRBSIZE_RIRBSIZE_256);
		break;
	case 16:
		rirbsize = HDAC_RIRBSIZE_RIRBSIZE(HDAC_RIRBSIZE_RIRBSIZE_16);
		break;
	case 2:
		rirbsize = HDAC_RIRBSIZE_RIRBSIZE(HDAC_RIRBSIZE_RIRBSIZE_2);
		break;
	default:
		panic("%s: Invalid RIRB size (%x)\n", __func__, sc->rirb_size);
	}
	HDAC_WRITE_1(&sc->mem, HDAC_RIRBSIZE, rirbsize);

	/* Setup the RIRB Address in the hdac */
	rirbpaddr = (uint64_t)sc->rirb_dma.dma_paddr;
	HDAC_WRITE_4(&sc->mem, HDAC_RIRBLBASE, (uint32_t)rirbpaddr);
	HDAC_WRITE_4(&sc->mem, HDAC_RIRBUBASE, (uint32_t)(rirbpaddr >> 32));

	/* Setup the WP and RP */
	sc->rirb_rp = 0;
	HDAC_WRITE_2(&sc->mem, HDAC_RIRBWP, HDAC_RIRBWP_RIRBWPRST);

	/* Setup the interrupt threshold */
	HDAC_WRITE_2(&sc->mem, HDAC_RINTCNT, sc->rirb_size / 2);

	/* Enable Overrun and response received reporting */
#if 0
	HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL,
	    HDAC_RIRBCTL_RIRBOIC | HDAC_RIRBCTL_RINTCTL);
#else
	HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL, HDAC_RIRBCTL_RINTCTL);
#endif

	/*
	 * Make sure that the Host CPU cache doesn't contain any dirty
	 * cache lines that falls in the rirb. If I understood correctly, it
	 * should be sufficient to do this only once as the rirb is purely
	 * read-only from now on.
	 */
	bus_dmamap_sync(sc->rirb_dma.dma_tag, sc->rirb_dma.dma_map,
	    BUS_DMASYNC_PREREAD);
}

/****************************************************************************
 * void hdac_corb_start(hdac_softc *)
 *
 * Startup the corb DMA engine
 ****************************************************************************/
static void
hdac_corb_start(struct hdac_softc *sc)
{
	uint32_t corbctl;

	corbctl = HDAC_READ_1(&sc->mem, HDAC_CORBCTL);
	corbctl |= HDAC_CORBCTL_CORBRUN;
	HDAC_WRITE_1(&sc->mem, HDAC_CORBCTL, corbctl);
}

/****************************************************************************
 * void hdac_rirb_start(hdac_softc *)
 *
 * Startup the rirb DMA engine
 ****************************************************************************/
static void
hdac_rirb_start(struct hdac_softc *sc)
{
	uint32_t rirbctl;

	rirbctl = HDAC_READ_1(&sc->mem, HDAC_RIRBCTL);
	rirbctl |= HDAC_RIRBCTL_RIRBDMAEN;
	HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL, rirbctl);
}


/****************************************************************************
 * void hdac_scan_codecs(struct hdac_softc *)
 *
 * Scan the bus for available codecs.
 ****************************************************************************/
static void
hdac_scan_codecs(struct hdac_softc *sc)
{
	struct hdac_codec *codec;
	int i;
	uint16_t statests;

	SLIST_INIT(&sc->codec_list);

	statests = HDAC_READ_2(&sc->mem, HDAC_STATESTS);
	for (i = 0; i < HDAC_CODEC_MAX; i++) {
		if (HDAC_STATESTS_SDIWAKE(statests, i)) {
			/* We have found a codec. */
			hdac_unlock(sc);
			codec = (struct hdac_codec *)malloc(sizeof(*codec),
			    M_HDAC, M_ZERO | M_NOWAIT);
			hdac_lock(sc);
			if (codec == NULL) {
				device_printf(sc->dev,
				    "Unable to allocate memory for codec\n");
				continue;
			}
			codec->verbs_sent = 0;
			codec->sc = sc;
			codec->cad = i;
			sc->codecs[i] = codec;
			SLIST_INSERT_HEAD(&sc->codec_list, codec, next_codec);
			if (hdac_probe_codec(codec) != 0)
				break;
		}
	}
	/* All codecs have been probed, now try to attach drivers to them */
	bus_generic_attach(sc->dev);
}

/****************************************************************************
 * void hdac_probe_codec(struct hdac_softc *, int)
 *
 * Probe a the given codec_id for available function groups.
 ****************************************************************************/
static int
hdac_probe_codec(struct hdac_codec *codec)
{
	struct hdac_softc *sc = codec->sc;
	struct hdac_devinfo *devinfo;
	uint32_t vendorid, revisionid, subnode;
	int startnode;
	int endnode;
	int i;
	nid_t cad = codec->cad;

	HDA_DEBUG_MSG(
		device_printf(sc->dev, "%s: Probing codec: %d\n",
		    __func__, cad);
	);
	vendorid = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, 0x0, HDA_PARAM_VENDOR_ID),
	    cad);
	revisionid = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, 0x0, HDA_PARAM_REVISION_ID),
	    cad);
	subnode = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, 0x0, HDA_PARAM_SUB_NODE_COUNT),
	    cad);
	startnode = HDA_PARAM_SUB_NODE_COUNT_START(subnode);
	endnode = startnode + HDA_PARAM_SUB_NODE_COUNT_TOTAL(subnode);

	HDA_DEBUG_MSG(
		device_printf(sc->dev, "%s: \tstartnode=%d endnode=%d\n",
		    __func__, startnode, endnode);
	);
	for (i = startnode; i < endnode; i++) {
		devinfo = hdac_probe_function(codec, i);
		if (devinfo != NULL) {
			/* XXX Ignore other FG. */
			devinfo->vendor_id =
			    HDA_PARAM_VENDOR_ID_VENDOR_ID(vendorid);
			devinfo->device_id =
			    HDA_PARAM_VENDOR_ID_DEVICE_ID(vendorid);
			devinfo->revision_id =
			    HDA_PARAM_REVISION_ID_REVISION_ID(revisionid);
			devinfo->stepping_id =
			    HDA_PARAM_REVISION_ID_STEPPING_ID(revisionid);
			HDA_DEBUG_MSG(
				device_printf(sc->dev,
				    "%s: \tFound AFG nid=%d "
				    "[startnode=%d endnode=%d]\n",
				    __func__, devinfo->nid,
				    startnode, endnode);
			);
			return (1);
		}
	}

	HDA_DEBUG_MSG(
		device_printf(sc->dev, "%s: \tAFG not found\n",
		    __func__);
	);
	return (0);
}

static struct hdac_devinfo *
hdac_probe_function(struct hdac_codec *codec, nid_t nid)
{
	struct hdac_softc *sc = codec->sc;
	struct hdac_devinfo *devinfo;
	uint32_t fctgrptype;
	nid_t cad = codec->cad;

	fctgrptype = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_FCT_GRP_TYPE), cad);

	/* XXX For now, ignore other FG. */
	if (HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE(fctgrptype) !=
	    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO)
		return (NULL);

	hdac_unlock(sc);
	devinfo = (struct hdac_devinfo *)malloc(sizeof(*devinfo), M_HDAC,
	    M_NOWAIT | M_ZERO);
	hdac_lock(sc);
	if (devinfo == NULL) {
		device_printf(sc->dev, "%s: Unable to allocate ivar\n",
		    __func__);
		return (NULL);
	}

	devinfo->nid = nid;
	devinfo->node_type = HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE(fctgrptype);
	devinfo->codec = codec;

	hdac_add_child(sc, devinfo);

	return (devinfo);
}

static void
hdac_add_child(struct hdac_softc *sc, struct hdac_devinfo *devinfo)
{
	devinfo->dev = device_add_child(sc->dev, NULL, -1);
	device_set_ivars(devinfo->dev, (void *)devinfo);
	/* XXX - Print more information when booting verbose??? */
}

static void
hdac_widget_connection_parse(struct hdac_widget *w)
{
	struct hdac_softc *sc = w->devinfo->codec->sc;
	uint32_t res;
	int i, j, max, found, entnum, cnid;
	nid_t cad = w->devinfo->codec->cad;
	nid_t nid = w->nid;

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_CONN_LIST_LENGTH), cad);

	w->nconns = HDA_PARAM_CONN_LIST_LENGTH_LIST_LENGTH(res);

	if (w->nconns < 1)
		return;

	entnum = HDA_PARAM_CONN_LIST_LENGTH_LONG_FORM(res) ? 2 : 4;
	res = 0;
	i = 0;
	found = 0;
	max = (sizeof(w->conns) / sizeof(w->conns[0])) - 1;

	while (i < w->nconns) {
		res = hdac_command(sc,
		    HDA_CMD_GET_CONN_LIST_ENTRY(cad, nid, i), cad);
		for (j = 0; j < entnum; j++) {
			cnid = res;
			cnid >>= (32 / entnum) * j;
			cnid &= (1 << (32 / entnum)) - 1;
			if (cnid == 0)
				continue;
			if (found > max) {
				device_printf(sc->dev,
				    "node %d: Adding %d: "
				    "Max connection reached!\n",
				    nid, cnid);
				continue;
			}
			w->conns[found++] = cnid;
		}
		i += entnum;
	}

	HDA_BOOTVERBOSE_MSG(
		if (w->nconns != found) {
			device_printf(sc->dev,
			    "node %d: WARNING!!! Connection "
			    "length=%d != found=%d\n",
			    nid, w->nconns, found);
		}
	);
}

static uint32_t
hdac_widget_pin_getconfig(struct hdac_widget *w)
{
	struct hdac_softc *sc;
	uint32_t config, id;
	nid_t cad, nid;

	sc = w->devinfo->codec->sc;
	cad = w->devinfo->codec->cad;
	nid = w->nid;
	id = hdac_codec_id(w->devinfo);

	config = hdac_command(sc,
	    HDA_CMD_GET_CONFIGURATION_DEFAULT(cad, nid),
	    cad);
	if (id == HDA_CODEC_ALC880 &&
	    sc->pci_subvendor == CLEVO_D900T_SUBVENDOR) {
		/*
		 * Super broken BIOS: Clevo D900T
		 */
		switch (nid) {
		case 20:
			break;
		case 21:
			break;
		case 22:
			break;
		case 23:
			break;
		case 24:	/* MIC1 */
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN;
			break;
		case 25:	/* XXX MIC2 */
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN;
			break;
		case 26:	/* LINE1 */
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN;
			break;
		case 27:	/* XXX LINE2 */
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN;
			break;
		case 28:	/* CD */
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_CD;
			break;
		case 30:
			break;
		case 31:
			break;
		default:
			break;
		}
	}

	return (config);
}

static void
hdac_widget_pin_parse(struct hdac_widget *w)
{
	struct hdac_softc *sc = w->devinfo->codec->sc;
	uint32_t config, pincap;
	char *devstr, *connstr;
	nid_t cad = w->devinfo->codec->cad;
	nid_t nid = w->nid;

	config = hdac_widget_pin_getconfig(w);
	w->wclass.pin.config = config;

	pincap = hdac_command(sc,
		HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_PIN_CAP), cad);
	w->wclass.pin.cap = pincap;

	w->wclass.pin.ctrl = hdac_command(sc,
		HDA_CMD_GET_PIN_WIDGET_CTRL(cad, nid), cad) &
		~(HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE |
		HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
		HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE);

	if (HDA_PARAM_PIN_CAP_HEADPHONE_CAP(pincap))
		w->wclass.pin.ctrl |= HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE;
	if (HDA_PARAM_PIN_CAP_OUTPUT_CAP(pincap))
		w->wclass.pin.ctrl |= HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
	if (HDA_PARAM_PIN_CAP_INPUT_CAP(pincap))
		w->wclass.pin.ctrl |= HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE;
	if (HDA_PARAM_PIN_CAP_EAPD_CAP(pincap)) {
		w->param.eapdbtl = hdac_command(sc,
		    HDA_CMD_GET_EAPD_BTL_ENABLE(cad, nid), cad);
		w->param.eapdbtl &= 0x7;
		w->param.eapdbtl |= HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
	} else
		w->param.eapdbtl = 0xffffffff;

	switch (config & HDA_CONFIG_DEFAULTCONF_DEVICE_MASK) {
	case HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_OUT:
		devstr = "line out";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_SPEAKER:
		devstr = "speaker";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT:
		devstr = "headphones out";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_CD:
		devstr = "CD";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_SPDIF_OUT:
		devstr = "SPDIF out";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_DIGITAL_OTHER_OUT:
		devstr = "digital (other) out";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_MODEM_LINE:
		devstr = "modem, line side";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_MODEM_HANDSET:
		devstr = "modem, handset side";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN:
		devstr = "line in";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_AUX:
		devstr = "AUX";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN:
		devstr = "Mic in";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_TELEPHONY:
		devstr = "telephony";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_SPDIF_IN:
		devstr = "SPDIF in";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_DIGITAL_OTHER_IN:
		devstr = "digital (other) in";
		break;
	case HDA_CONFIG_DEFAULTCONF_DEVICE_OTHER:
		devstr = "other";
		break;
	default:
		devstr = "unknown";
		break;
	}

	switch (config & HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK) {
	case HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK:
		connstr = "jack";
		break;
	case HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_NONE:
		connstr = "none";
		break;
	case HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED:
		connstr = "fixed";
		break;
	case HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_BOTH:
		connstr = "jack / fixed";
		break;
	default:
		connstr = "unknown";
		break;
	}

	strlcat(w->name, ": ", sizeof(w->name));
	strlcat(w->name, devstr, sizeof(w->name));
	strlcat(w->name, " (", sizeof(w->name));
	strlcat(w->name, connstr, sizeof(w->name));
	strlcat(w->name, ")", sizeof(w->name));
}

static void
hdac_widget_parse(struct hdac_widget *w)
{
	struct hdac_softc *sc = w->devinfo->codec->sc;
	uint32_t wcap, cap;
	char *typestr;
	nid_t cad = w->devinfo->codec->cad;
	nid_t nid = w->nid;

	wcap = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_AUDIO_WIDGET_CAP),
	    cad);
	w->param.widget_cap = wcap;
	w->type = HDA_PARAM_AUDIO_WIDGET_CAP_TYPE(wcap);

	switch (w->type) {
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT:
		typestr = "audio output";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT:
		typestr = "audio input";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER:
		typestr = "audio mixer";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
		typestr = "audio selector";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX:
		typestr = "pin";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_POWER_WIDGET:
		typestr = "power widget";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_VOLUME_WIDGET:
		typestr = "volume widget";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET:
		typestr = "beep widget";
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_VENDOR_WIDGET:
		typestr = "vendor widget";
		break;
	default:
		typestr = "unknown type";
		break;
	}

	strlcpy(w->name, typestr, sizeof(w->name));

	if (HDA_PARAM_AUDIO_WIDGET_CAP_POWER_CTRL(wcap)) {
		hdac_command(sc,
		    HDA_CMD_SET_POWER_STATE(cad, nid, HDA_CMD_POWER_STATE_D0),
		    cad);
		DELAY(1000);
	}

	hdac_widget_connection_parse(w);

	if (HDA_PARAM_AUDIO_WIDGET_CAP_OUT_AMP(wcap)) {
		if (HDA_PARAM_AUDIO_WIDGET_CAP_AMP_OVR(wcap))
			w->param.outamp_cap =
			    hdac_command(sc,
			    HDA_CMD_GET_PARAMETER(cad, nid,
			    HDA_PARAM_OUTPUT_AMP_CAP), cad);
		else
			w->param.outamp_cap =
			    w->devinfo->function.audio.outamp_cap;
	} else
		w->param.outamp_cap = 0;

	if (HDA_PARAM_AUDIO_WIDGET_CAP_IN_AMP(wcap)) {
		if (HDA_PARAM_AUDIO_WIDGET_CAP_AMP_OVR(wcap))
			w->param.inamp_cap =
			    hdac_command(sc,
			    HDA_CMD_GET_PARAMETER(cad, nid,
			    HDA_PARAM_INPUT_AMP_CAP), cad);
		else
			w->param.inamp_cap =
			    w->devinfo->function.audio.inamp_cap;
	} else
		w->param.inamp_cap = 0;

	if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT ||
	    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT) {
		if (HDA_PARAM_AUDIO_WIDGET_CAP_FORMAT_OVR(wcap)) {
			cap = hdac_command(sc,
			    HDA_CMD_GET_PARAMETER(cad, nid,
			    HDA_PARAM_SUPP_STREAM_FORMATS), cad);
			w->param.supp_stream_formats = (cap != 0) ? cap :
			    w->devinfo->function.audio.supp_stream_formats;
			cap = hdac_command(sc,
			    HDA_CMD_GET_PARAMETER(cad, nid,
			    HDA_PARAM_SUPP_PCM_SIZE_RATE), cad);
			w->param.supp_pcm_size_rate = (cap != 0) ? cap :
			    w->devinfo->function.audio.supp_pcm_size_rate;
		} else {
			w->param.supp_stream_formats =
			    w->devinfo->function.audio.supp_stream_formats;
			w->param.supp_pcm_size_rate =
			    w->devinfo->function.audio.supp_pcm_size_rate;
		}
	} else {
		w->param.supp_stream_formats = 0;
		w->param.supp_pcm_size_rate = 0;
	}

	if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
		hdac_widget_pin_parse(w);
}

static struct hdac_widget *
hdac_widget_get(struct hdac_devinfo *devinfo, nid_t nid)
{
	if (devinfo == NULL || devinfo->widget == NULL ||
		    nid < devinfo->startnode || nid >= devinfo->endnode)
		return (NULL);
	return (&devinfo->widget[nid - devinfo->startnode]);
}

static void
hdac_stream_stop(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	uint32_t ctl;

	ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL0);
	ctl &= ~(HDAC_SDCTL_IOCE | HDAC_SDCTL_FEIE | HDAC_SDCTL_DEIE |
	    HDAC_SDCTL_RUN);
	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDCTL0, ctl);

	ctl = HDAC_READ_4(&sc->mem, HDAC_INTCTL);
	ctl &= ~(1 << (ch->off >> 5));
	HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, ctl);
}

static void
hdac_stream_start(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	uint32_t ctl;

	ctl = HDAC_READ_4(&sc->mem, HDAC_INTCTL);
	ctl |= 1 << (ch->off >> 5);
	HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, ctl);

	ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL0);
	ctl |= HDAC_SDCTL_IOCE | HDAC_SDCTL_FEIE | HDAC_SDCTL_DEIE |
	    HDAC_SDCTL_RUN;
	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDCTL0, ctl);
}

static void
hdac_stream_reset(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	int timeout = 1000;
	int to = timeout;
	uint32_t ctl;

	ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL0);
	ctl |= HDAC_SDCTL_SRST;
	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDCTL0, ctl);
	do {
		ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL0);
		if (ctl & HDAC_SDCTL_SRST)
			break;
		DELAY(10);
	} while (--to);
	if (!(ctl & HDAC_SDCTL_SRST)) {
		device_printf(sc->dev, "timeout in reset\n");
	}
	ctl &= ~HDAC_SDCTL_SRST;
	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDCTL0, ctl);
	to = timeout;
	do {
		ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL0);
		if (!(ctl & HDAC_SDCTL_SRST))
			break;
		DELAY(10);
	} while (--to);
	if ((ctl & HDAC_SDCTL_SRST))
		device_printf(sc->dev, "can't reset!\n");
}

static void
hdac_stream_setid(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	uint32_t ctl;

	ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL2);
	ctl &= ~HDAC_SDCTL2_STRM_MASK;
	ctl |= ch->sid << HDAC_SDCTL2_STRM_SHIFT;
	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDCTL2, ctl);
}

static void
hdac_bdl_setup(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	uint64_t addr;
	int blks, size, blocksize;
	struct hdac_bdle *bdle;
	int i;

	addr = (uint64_t)sndbuf_getbufaddr(ch->b);
	size = sndbuf_getsize(ch->b);
	blocksize = sndbuf_getblksz(ch->b);
	blks = size / blocksize;
	bdle = (struct hdac_bdle*)ch->bdl_dma.dma_vaddr;

	for (i = 0; i < blks; i++, bdle++) {
		bdle->addrl = (uint32_t)addr;
		bdle->addrh = (uint32_t)(addr >> 32);
		bdle->len = blocksize;
		bdle->ioc = 1;

		addr += blocksize;
	}

	HDAC_WRITE_4(&sc->mem, ch->off + HDAC_SDCBL, size);
	HDAC_WRITE_2(&sc->mem, ch->off + HDAC_SDLVI, blks - 1);
	addr = ch->bdl_dma.dma_paddr;
	HDAC_WRITE_4(&sc->mem, ch->off + HDAC_SDBDPL, (uint32_t)addr);
	HDAC_WRITE_4(&sc->mem, ch->off + HDAC_SDBDPU, (uint32_t)(addr >> 32));
}

static int
hdac_bdl_alloc(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	int rc;

	rc = hdac_dma_alloc(sc, &ch->bdl_dma,
	    sizeof(struct hdac_bdle) * HDA_BDL_MAX);
	if (rc) {
		device_printf(sc->dev, "can't alloc bdl\n");
		return (rc);
	}
	hdac_dma_nocache(ch->bdl_dma.dma_vaddr);

	return (0);
}

static void
hdac_audio_ctl_amp_set_internal(struct hdac_softc *sc, nid_t cad, nid_t nid,
					int index, int lmute, int rmute,
					int left, int right, int dir)
{
	uint16_t v = 0;

	if (sc == NULL)
		return;

	if (left != right || lmute != rmute) {
		v = (1 << (15 - dir)) | (1 << 13) | (index << 8) |
		    (lmute << 7) | left;
		hdac_command(sc,
			HDA_CMD_SET_AMP_GAIN_MUTE(cad, nid, v), cad);
		v = (1 << (15 - dir)) | (1 << 12) | (index << 8) |
		    (rmute << 7) | right;
	} else
		v = (1 << (15 - dir)) | (3 << 12) | (index << 8) |
		    (lmute << 7) | left;

	hdac_command(sc,
	    HDA_CMD_SET_AMP_GAIN_MUTE(cad, nid, v), cad);
}

static void
hdac_audio_ctl_amp_set(struct hdac_audio_ctl *ctl, uint32_t mute,
						int left, int right)
{
	struct hdac_softc *sc;
	nid_t nid, cad;
	int lmute, rmute;

	if (ctl == NULL || ctl->widget == NULL ||
	    ctl->widget->devinfo == NULL ||
	    ctl->widget->devinfo->codec == NULL ||
	    ctl->widget->devinfo->codec->sc == NULL)
		return;

	sc = ctl->widget->devinfo->codec->sc;
	cad = ctl->widget->devinfo->codec->cad;
	nid = ctl->widget->nid;

	if (mute == HDA_AMP_MUTE_DEFAULT) {
		lmute = HDA_AMP_LEFT_MUTED(ctl->muted);
		rmute = HDA_AMP_RIGHT_MUTED(ctl->muted);
	} else {
		lmute = HDA_AMP_LEFT_MUTED(mute);
		rmute = HDA_AMP_RIGHT_MUTED(mute);
	}

	if (ctl->dir & HDA_CTL_OUT)
		hdac_audio_ctl_amp_set_internal(sc, cad, nid, ctl->index,
		    lmute, rmute, left, right, 0);
	if (ctl->dir & HDA_CTL_IN)
		hdac_audio_ctl_amp_set_internal(sc, cad, nid, ctl->index,
		    lmute, rmute, left, right, 1);
	ctl->left = left;
	ctl->right = right;
}

static void
hdac_widget_connection_select(struct hdac_widget *w, uint8_t index)
{
	if (w == NULL || w->nconns < 1 || index > (w->nconns - 1))
		return;
	hdac_command(w->devinfo->codec->sc,
	    HDA_CMD_SET_CONNECTION_SELECT_CONTROL(w->devinfo->codec->cad,
	    w->nid, index), w->devinfo->codec->cad);
	w->selconn = index;
}


/****************************************************************************
 * uint32_t hdac_command_sendone_internal
 *
 * Wrapper function that sends only one command to a given codec
 ****************************************************************************/
static uint32_t
hdac_command_sendone_internal(struct hdac_softc *sc, uint32_t verb, nid_t cad)
{
	struct hdac_command_list cl;
	uint32_t response = 0xffffffff;

	if (!mtx_owned(sc->lock))
		device_printf(sc->dev, "WARNING!!!! mtx not owned!!!!\n");
	cl.num_commands = 1;
	cl.verbs = &verb;
	cl.responses = &response;

	hdac_command_send_internal(sc, &cl, cad);

	return (response);
}

/****************************************************************************
 * hdac_command_send_internal
 *
 * Send a command list to the codec via the corb. We queue as much verbs as
 * we can and msleep on the codec. When the interrupt get the responses
 * back from the rirb, it will wake us up so we can queue the remaining verbs
 * if any.
 ****************************************************************************/
static void
hdac_command_send_internal(struct hdac_softc *sc,
			struct hdac_command_list *commands, nid_t cad)
{
	struct hdac_codec *codec;
	int corbrp;
	uint32_t *corb;
	uint8_t rirbwp;
	int timeout;
	int retry = 10;
	struct hdac_rirb *rirb_base, *rirb;
	nid_t ucad;
	uint32_t utag;

	if (sc == NULL || sc->codecs[cad] == NULL || commands == NULL)
		return;

	codec = sc->codecs[cad];
	codec->commands = commands;
	codec->responses_received = 0;
	codec->verbs_sent = 0;
	corb = (uint32_t *)sc->corb_dma.dma_vaddr;
	rirb_base = (struct hdac_rirb *)sc->rirb_dma.dma_vaddr;

	do {
		if (codec->verbs_sent != commands->num_commands) {
			/* Queue as many verbs as possible */
			corbrp = HDAC_READ_2(&sc->mem, HDAC_CORBRP);
			bus_dmamap_sync(sc->corb_dma.dma_tag,
			    sc->corb_dma.dma_map, BUS_DMASYNC_PREWRITE);
			while (codec->verbs_sent != commands->num_commands &&
			    ((sc->corb_wp + 1) % sc->corb_size) != corbrp) {
				sc->corb_wp++;
				sc->corb_wp %= sc->corb_size;
				corb[sc->corb_wp] =
				    commands->verbs[codec->verbs_sent++];
			}

			/* Send the verbs to the codecs */
			bus_dmamap_sync(sc->corb_dma.dma_tag,
			    sc->corb_dma.dma_map, BUS_DMASYNC_POSTWRITE);
			HDAC_WRITE_2(&sc->mem, HDAC_CORBWP, sc->corb_wp);
		}

		timeout = 1000;
		do {
			rirbwp = HDAC_READ_1(&sc->mem, HDAC_RIRBWP);
			bus_dmamap_sync(sc->rirb_dma.dma_tag, sc->rirb_dma.dma_map,
			    BUS_DMASYNC_POSTREAD);
			if (sc->rirb_rp != rirbwp) {
				do {
					sc->rirb_rp++;
					sc->rirb_rp %= sc->rirb_size;
					rirb = &rirb_base[sc->rirb_rp];
					if (rirb->response_ex & HDAC_RIRB_RESPONSE_EX_UNSOLICITED) {
						ucad = HDAC_RIRB_RESPONSE_EX_SDATA_IN(rirb->response_ex);
						utag = rirb->response >> 26;
						if (ucad > -1 && ucad < HDAC_CODEC_MAX &&
						    sc->codecs[ucad] != NULL) {
							sc->unsolq[sc->unsolq_wp++] =
							    (ucad << 16) |
							    (utag & 0xffff);
							sc->unsolq_wp %= HDAC_UNSOLQ_MAX;
						}
					} else if (codec->responses_received < commands->num_commands)
						codec->commands->responses[codec->responses_received++] =
						    rirb->response;
				} while (sc->rirb_rp != rirbwp);
				break;
			}
			DELAY(10);
		} while (--timeout);
	} while ((codec->verbs_sent != commands->num_commands ||
	    	codec->responses_received != commands->num_commands) &&
		--retry);

	if (retry == 0)
		device_printf(sc->dev,
			"%s: TIMEOUT numcmd=%d, sent=%d, received=%d\n",
			__func__, commands->num_commands,
			codec->verbs_sent, codec->responses_received);

	codec->verbs_sent = 0;

	if (sc->unsolq_st == HDAC_UNSOLQ_READY) {
		sc->unsolq_st = HDAC_UNSOLQ_BUSY;
		while (sc->unsolq_rp != sc->unsolq_wp) {
			ucad = sc->unsolq[sc->unsolq_rp] >> 16;
			utag = sc->unsolq[sc->unsolq_rp++] & 0xffff;
			sc->unsolq_rp %= HDAC_UNSOLQ_MAX;
			hdac_unsolicited_handler(sc->codecs[ucad], utag);
		}
		sc->unsolq_st = HDAC_UNSOLQ_READY;
	}
}


/****************************************************************************
 * Device Methods
 ****************************************************************************/

/****************************************************************************
 * int hdac_probe(device_t)
 *
 * Probe for the presence of an hdac. If none is found, check for a generic
 * match using the subclass of the device.
 ****************************************************************************/
static int
hdac_probe(device_t dev)
{
	int i, result;
	uint32_t model, class, subclass;
	char desc[64];

	model = (uint32_t)pci_get_device(dev) << 16;
	model |= (uint32_t)pci_get_vendor(dev) & 0x0000ffff;
	class = pci_get_class(dev);
	subclass = pci_get_subclass(dev);

	bzero(desc, sizeof(desc));
	result = ENXIO;
	for (i = 0; i < HDAC_DEVICES_LEN; i++) {
		if (hdac_devices[i].model == model) {
		    	strlcpy(desc, hdac_devices[i].desc, sizeof(desc));
		    	result = BUS_PROBE_DEFAULT;
			break;
		}
		if ((hdac_devices[i].model & model) == model &&
		    class == PCIC_MULTIMEDIA &&
		    subclass == PCIS_MULTIMEDIA_HDA) {
		    	strlcpy(desc, hdac_devices[i].desc, sizeof(desc));
		    	result = BUS_PROBE_GENERIC;
			break;
		}
	}
	if (result == ENXIO && class == PCIC_MULTIMEDIA &&
	    subclass == PCIS_MULTIMEDIA_HDA) {
		strlcpy(desc, "Generic", sizeof(desc));
	    	result = BUS_PROBE_GENERIC;
	}
	if (result != ENXIO) {
		strlcat(desc, " High Definition Audio Controller",
		    sizeof(desc));
		device_set_desc_copy(dev, desc);
	}

	return (result);
}

static void *
hdac_channel_init(kobj_t obj, void *data, struct snd_dbuf *b,
					struct pcm_channel *c, int dir)
{
	struct hdac_devinfo *devinfo = data;
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_chan *ch;

	hdac_lock(sc);
	if (dir == PCMDIR_PLAY) {
		ch = &sc->play;
		ch->off = (sc->num_iss + devinfo->function.audio.playcnt) << 5;
		ch->dir = PCMDIR_PLAY;
		ch->sid = ++sc->streamcnt;
		devinfo->function.audio.playcnt++;
	} else {
		ch = &sc->rec;
		ch->off = devinfo->function.audio.reccnt << 5;
		ch->dir = PCMDIR_REC;
		ch->sid = ++sc->streamcnt;
		devinfo->function.audio.reccnt++;
	}
	if (devinfo->function.audio.quirks & HDA_QUIRK_FIXEDRATE) {
		ch->caps.minspeed = ch->caps.maxspeed = 48000;
		ch->pcmrates[0] = 48000;
		ch->pcmrates[1] = 0;
	}
	ch->b = b;
	ch->c = c;
	ch->devinfo = devinfo;
	ch->blksz = sc->chan_size / sc->chan_blkcnt;
	ch->blkcnt = sc->chan_blkcnt;
	hdac_unlock(sc);

	if (hdac_bdl_alloc(ch) != 0) {
		ch->blkcnt = 0;
		return (NULL);
	}

	if (sndbuf_alloc(ch->b, sc->chan_dmat, sc->chan_size) != 0)
		return (NULL);

	hdac_dma_nocache(ch->b->buf);

	return (ch);
}

static int
hdac_channel_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct hdac_chan *ch = data;
	int i;

	for (i = 0; ch->caps.fmtlist[i] != 0; i++) {
		if (format == ch->caps.fmtlist[i]) {
			ch->fmt = format;
			return (0);
		}
	}

	return (EINVAL);
}

static int
hdac_channel_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct hdac_chan *ch = data;
	uint32_t spd = 0;
	int i;

	for (i = 0; ch->pcmrates[i] != 0; i++) {
		spd = ch->pcmrates[i];
		if (spd >= speed)
			break;
	}

	if (spd == 0)
		ch->spd = 48000;
	else
		ch->spd = spd;

	return (ch->spd);
}

static void
hdac_stream_setup(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	int i;
	nid_t cad = ch->devinfo->codec->cad;
	uint16_t fmt;

	/*
	 *  8bit = 0
	 * 16bit = 1
	 * 20bit = 2
	 * 24bit = 3
	 * 32bit = 4
	 */
	fmt = 0;
	if (ch->fmt & AFMT_S16_LE)
		fmt |= ch->bit16 << 4;
	else if (ch->fmt & AFMT_S32_LE)
		fmt |= ch->bit32 << 4;
	else
		fmt |= 1 << 4;

	for (i = 0; i < HDA_RATE_TAB_LEN; i++) {
		if (hda_rate_tab[i].valid && ch->spd == hda_rate_tab[i].rate) {
			fmt |= hda_rate_tab[i].base;
			fmt |= hda_rate_tab[i].mul;
			fmt |= hda_rate_tab[i].div;
			break;
		}
	}

	if (ch->fmt & AFMT_STEREO)
		fmt |= 1;

	HDAC_WRITE_2(&sc->mem, ch->off + HDAC_SDFMT, fmt);

	for (i = 0; ch->io[i] != -1; i++) {
		HDA_BOOTVERBOSE_MSG(
			device_printf(sc->dev,
			    "PCMDIR_%s: Stream setup nid=%d fmt=0x%08x\n",
			    (ch->dir == PCMDIR_PLAY) ? "PLAY" : "REC",
			    ch->io[i], fmt);
		);
		hdac_command(sc,
		    HDA_CMD_SET_CONV_FMT(cad, ch->io[i], fmt), cad);
		hdac_command(sc,
		    HDA_CMD_SET_CONV_STREAM_CHAN(cad, ch->io[i],
		    ch->sid << 4), cad);
	}
}

static int
hdac_channel_setblocksize(kobj_t obj, void *data, uint32_t blocksize)
{
	struct hdac_chan *ch = data;

	sndbuf_resize(ch->b, ch->blkcnt, ch->blksz);

	return (ch->blksz);
}

static void
hdac_channel_stop(struct hdac_softc *sc, struct hdac_chan *ch)
{
	struct hdac_devinfo *devinfo = ch->devinfo;
	nid_t cad = devinfo->codec->cad;
	int i;

	hdac_stream_stop(ch);

	for (i = 0; ch->io[i] != -1; i++) {
		hdac_command(sc,
		    HDA_CMD_SET_CONV_STREAM_CHAN(cad, ch->io[i],
		    0), cad);
	}
}

static void
hdac_channel_start(struct hdac_softc *sc, struct hdac_chan *ch)
{
	ch->ptr = 0;
	ch->prevptr = 0;
	hdac_stream_stop(ch);
	hdac_stream_reset(ch);
	hdac_bdl_setup(ch);
	hdac_stream_setid(ch);
	hdac_stream_setup(ch);
	hdac_stream_start(ch);
}

static int
hdac_channel_trigger(kobj_t obj, void *data, int go)
{
	struct hdac_chan *ch = data;
	struct hdac_softc *sc = ch->devinfo->codec->sc;

	hdac_lock(sc);
	switch (go) {
	case PCMTRIG_START:
		hdac_channel_start(sc, ch);
		break;
	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		hdac_channel_stop(sc, ch);
		break;
	}
	hdac_unlock(sc);

	return (0);
}

static int
hdac_channel_getptr(kobj_t obj, void *data)
{
	struct hdac_chan *ch = data;
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	int sz, delta;
	uint32_t ptr;

	hdac_lock(sc);
	ptr = HDAC_READ_4(&sc->mem, ch->off + HDAC_SDLPIB);
	hdac_unlock(sc);

	sz = sndbuf_getsize(ch->b);
	ptr %= sz;

	if (ch->dir == PCMDIR_REC) {
		delta = ptr % sndbuf_getblksz(ch->b);
		if (delta != 0) {
			ptr -= delta;
			if (ptr < delta)
				ptr = sz - delta;
			else
				ptr -= delta;
		}
	}

	return (ptr);
}

static struct pcmchan_caps *
hdac_channel_getcaps(kobj_t obj, void *data)
{
	return (&((struct hdac_chan *)data)->caps);
}

static kobj_method_t hdac_channel_methods[] = {
	KOBJMETHOD(channel_init,		hdac_channel_init),
	KOBJMETHOD(channel_setformat,		hdac_channel_setformat),
	KOBJMETHOD(channel_setspeed,		hdac_channel_setspeed),
	KOBJMETHOD(channel_setblocksize,	hdac_channel_setblocksize),
	KOBJMETHOD(channel_trigger,		hdac_channel_trigger),
	KOBJMETHOD(channel_getptr,		hdac_channel_getptr),
	KOBJMETHOD(channel_getcaps,		hdac_channel_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(hdac_channel);

static int
hdac_audio_ctl_ossmixer_init(struct snd_mixer *m)
{
	struct hdac_devinfo *devinfo = mix_getdevinfo(m);
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w, *cw;
	struct hdac_audio_ctl *ctl;
	uint32_t mask, recmask, id;
	int i, j, softpcmvol;
	nid_t cad;

	if (resource_int_value(device_get_name(sc->dev),
	    device_get_unit(sc->dev), "softpcmvol", &softpcmvol) == 0)
		softpcmvol = (softpcmvol != 0) ? 1 : 0;
	else
		softpcmvol = (devinfo->function.audio.quirks &
		    HDA_QUIRK_SOFTPCMVOL) ?
		    1 : 0;

	hdac_lock(sc);

	mask = 0;
	recmask = 0;

	id = hdac_codec_id(devinfo);
	cad = devinfo->codec->cad;
	for (i = 0; i < HDAC_HP_SWITCH_LEN; i++) {
		if (!((hdac_hp_switch[i].vendormask & sc->pci_subvendor) ==
		    sc->pci_subvendor &&
		    hdac_hp_switch[i].id == id))
			continue;
		w = hdac_widget_get(devinfo, hdac_hp_switch[i].hpnid);
		if (w != NULL && w->enable != 0
		    && w->type ==
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
		    HDA_PARAM_AUDIO_WIDGET_CAP_UNSOL_CAP(w->param.widget_cap)) {
			hdac_command(sc,
			    HDA_CMD_SET_UNSOLICITED_RESPONSE(cad,
			    w->nid,
			    HDA_CMD_SET_UNSOLICITED_RESPONSE_ENABLE|
			    HDAC_UNSOLTAG_EVENT_HP), cad);
			hdac_hp_switch_handler(devinfo);
		}
		break;
	}
	for (i = 0; i < HDAC_EAPD_SWITCH_LEN; i++) {
		if (!((hdac_eapd_switch[i].vendormask & sc->pci_subvendor) ==
		    sc->pci_subvendor && hdac_eapd_switch[i].id == id))
			continue;
		w = hdac_widget_get(devinfo, hdac_eapd_switch[i].eapdnid);
		if (w == NULL || w->enable == 0)
			break;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX ||
		    w->param.eapdbtl == 0xffffffff)
			break;
		mask |= SOUND_MASK_OGAIN;
		break;
	}

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		mask |= w->ctlflags;
		if (!(w->pflags & HDA_ADC_RECSEL))
			continue;
		for (j = 0; j < w->nconns; j++) {
			cw = hdac_widget_get(devinfo, w->conns[j]);
			if (cw == NULL || cw->enable == 0)
				continue;
			recmask |= cw->ctlflags;
		}
	}

	if (!(mask & SOUND_MASK_PCM)) {
		softpcmvol = 1;
		mask |= SOUND_MASK_PCM;
	}

	i = 0;
	ctl = NULL;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->widget == NULL || ctl->enable == 0)
			continue;
		if (!(ctl->ossmask & SOUND_MASK_PCM))
			continue;
		if (ctl->step > 0)
			break;
	}

	if (softpcmvol == 1 || ctl == NULL) {
		struct snddev_info *d = NULL;
		d = device_get_softc(sc->dev);
		if (d != NULL) {
			d->flags |= SD_F_SOFTPCMVOL;
			HDA_BOOTVERBOSE_MSG(
				device_printf(sc->dev,
				    "%s Soft PCM volume\n",
				    (softpcmvol == 1) ?
				    "Forcing" : "Enabling");
			);
		}
		i = 0;
		/*
		 * XXX Temporary quirk for STAC9220, until the parser
		 *     become smarter.
		 */
		if (id == HDA_CODEC_STAC9220) {
			mask |= SOUND_MASK_VOLUME;
			while ((ctl = hdac_audio_ctl_each(devinfo, &i)) !=
			    NULL) {
				if (ctl->widget == NULL || ctl->enable == 0)
					continue;
				if (ctl->widget->nid == 11 && ctl->index == 0) {
					ctl->ossmask = SOUND_MASK_VOLUME;
					ctl->ossval = 100 | (100 << 8);
				} else
					ctl->ossmask &= ~SOUND_MASK_VOLUME;
			}
		} else {
			mix_setparentchild(m, SOUND_MIXER_VOLUME,
			    SOUND_MASK_PCM);
			if (!(mask & SOUND_MASK_VOLUME))
				mix_setrealdev(m, SOUND_MIXER_VOLUME,
				    SOUND_MIXER_NONE);
			while ((ctl = hdac_audio_ctl_each(devinfo, &i)) !=
			    NULL) {
				if (ctl->widget == NULL || ctl->enable == 0)
					continue;
				if ((ctl->ossmask & (SOUND_MASK_VOLUME |
				    SOUND_MASK_PCM)) != (SOUND_MASK_VOLUME |
				    SOUND_MASK_PCM))
					continue;
				if (!(ctl->mute == 1 && ctl->step == 0))
					ctl->enable = 0;
			}
		}
	}

	recmask &= ~(SOUND_MASK_PCM | SOUND_MASK_RECLEV | SOUND_MASK_SPEAKER);

	mix_setrecdevs(m, recmask);
	mix_setdevs(m, mask);

	hdac_unlock(sc);

	return (0);
}

static int
hdac_audio_ctl_ossmixer_set(struct snd_mixer *m, unsigned dev,
					unsigned left, unsigned right)
{
	struct hdac_devinfo *devinfo = mix_getdevinfo(m);
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	uint32_t id, mute;
	int lvol, rvol, mlvol, mrvol;
	int i = 0;

	hdac_lock(sc);
	if (dev == SOUND_MIXER_OGAIN) {
		/*if (left != right || !(left == 0 || left == 1)) {
			hdac_unlock(sc);
			return (-1);
		}*/
		id = hdac_codec_id(devinfo);
		for (i = 0; i < HDAC_EAPD_SWITCH_LEN; i++) {
			if ((hdac_eapd_switch[i].vendormask &
			    sc->pci_subvendor) == sc->pci_subvendor &&
			    hdac_eapd_switch[i].id == id)
				break;
		}
		if (i >= HDAC_EAPD_SWITCH_LEN) {
			hdac_unlock(sc);
			return (-1);
		}
		w = hdac_widget_get(devinfo, hdac_eapd_switch[i].eapdnid);
		if (w == NULL ||
		    w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX ||
		    w->param.eapdbtl == 0xffffffff) {
			hdac_unlock(sc);
			return (-1);
		}
		if (left == 0)
			w->param.eapdbtl &= ~HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
		else
			w->param.eapdbtl |= HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
		if (hdac_eapd_switch[i].hp_switch != 0)
			hdac_hp_switch_handler(devinfo);
		hdac_command(sc,
		    HDA_CMD_SET_EAPD_BTL_ENABLE(devinfo->codec->cad, w->nid,
		    w->param.eapdbtl), devinfo->codec->cad);
		hdac_unlock(sc);
		return (left | (left << 8));
	}
	if (dev == SOUND_MIXER_VOLUME)
		devinfo->function.audio.mvol = left | (right << 8);

	mlvol = devinfo->function.audio.mvol & 0x7f;
	mrvol = (devinfo->function.audio.mvol >> 8) & 0x7f;
	lvol = 0;
	rvol = 0;

	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->widget == NULL || ctl->enable == 0 ||
		    !(ctl->ossmask & (1 << dev)))
			continue;
		switch (dev) {
		case SOUND_MIXER_VOLUME:
			lvol = ((ctl->ossval & 0x7f) * left) / 100;
			lvol = (lvol * ctl->step) / 100;
			rvol = (((ctl->ossval >> 8) & 0x7f) * right) / 100;
			rvol = (rvol * ctl->step) / 100;
			break;
		default:
			if (ctl->ossmask & SOUND_MASK_VOLUME) {
				lvol = (left * mlvol) / 100;
				lvol = (lvol * ctl->step) / 100;
				rvol = (right * mrvol) / 100;
				rvol = (rvol * ctl->step) / 100;
			} else {
				lvol = (left * ctl->step) / 100;
				rvol = (right * ctl->step) / 100;
			}
			ctl->ossval = left | (right << 8);
			break;
		}
		mute = 0;
		if (ctl->step < 1) {
			mute |= (left == 0) ? HDA_AMP_MUTE_LEFT :
			    (ctl->muted & HDA_AMP_MUTE_LEFT);
			mute |= (right == 0) ? HDA_AMP_MUTE_RIGHT :
			    (ctl->muted & HDA_AMP_MUTE_RIGHT);
		} else {
			mute |= (lvol == 0) ? HDA_AMP_MUTE_LEFT :
			    (ctl->muted & HDA_AMP_MUTE_LEFT);
			mute |= (rvol == 0) ? HDA_AMP_MUTE_RIGHT :
			    (ctl->muted & HDA_AMP_MUTE_RIGHT);
		}
		hdac_audio_ctl_amp_set(ctl, mute, lvol, rvol);
	}
	hdac_unlock(sc);

	return (left | (right << 8));
}

static int
hdac_audio_ctl_ossmixer_setrecsrc(struct snd_mixer *m, uint32_t src)
{
	struct hdac_devinfo *devinfo = mix_getdevinfo(m);
	struct hdac_widget *w, *cw;
	struct hdac_softc *sc = devinfo->codec->sc;
	uint32_t ret = src, target;
	int i, j;

	target = 0;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (src & (1 << i)) {
			target = 1 << i;
			break;
		}
	}

	hdac_lock(sc);

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL && w->enable == 0)
			continue;
		if (!(w->pflags & HDA_ADC_RECSEL))
			continue;
		for (j = 0; j < w->nconns; j++) {
			cw = hdac_widget_get(devinfo, w->conns[j]);
			if (cw == NULL || cw->enable == 0)
				continue;
			if ((target == SOUND_MASK_VOLUME &&
			    cw->type !=
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER) ||
			    (target != SOUND_MASK_VOLUME &&
			    cw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER))
				continue;
			if (cw->ctlflags & target) {
				hdac_widget_connection_select(w, j);
				ret = target;
				j += w->nconns;
			}
		}
	}

	hdac_unlock(sc);

	return (ret);
}

static kobj_method_t hdac_audio_ctl_ossmixer_methods[] = {
	KOBJMETHOD(mixer_init,		hdac_audio_ctl_ossmixer_init),
	KOBJMETHOD(mixer_set,		hdac_audio_ctl_ossmixer_set),
	KOBJMETHOD(mixer_setrecsrc,	hdac_audio_ctl_ossmixer_setrecsrc),
	{ 0, 0 }
};
MIXER_DECLARE(hdac_audio_ctl_ossmixer);

/****************************************************************************
 * int hdac_attach(device_t)
 *
 * Attach the device into the kernel. Interrupts usually won't be enabled
 * when this function is called. Setup everything that doesn't require
 * interrupts and defer probing of codecs until interrupts are enabled.
 ****************************************************************************/
static int
hdac_attach(device_t dev)
{
	struct hdac_softc *sc;
	int result;
	int i = 0;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return (ENOMEM);
	}
	sc->dev = dev;
	sc->pci_subvendor = pci_get_subdevice(sc->dev) << 16;
	sc->pci_subvendor |= pci_get_subvendor(sc->dev);

	sc->chan_size = pcm_getbuffersize(dev,
	    HDA_BUFSZ_MIN, HDA_BUFSZ_DEFAULT, HDA_BUFSZ_DEFAULT);
	if (resource_int_value(device_get_name(sc->dev),
	    device_get_unit(sc->dev), "blocksize", &i) == 0 &&
	    i > 0) {
		sc->chan_blkcnt = sc->chan_size / i;
		i = 0;
		while (sc->chan_blkcnt >> i)
			i++;
		sc->chan_blkcnt = 1 << (i - 1);
		if (sc->chan_blkcnt < HDA_BDL_MIN)
			sc->chan_blkcnt = HDA_BDL_MIN;
		else if (sc->chan_blkcnt > HDA_BDL_MAX)
			sc->chan_blkcnt = HDA_BDL_MAX;
	} else
		sc->chan_blkcnt = HDA_BDL_DEFAULT;

	result = bus_dma_tag_create(NULL,	/* parent */
	    HDAC_DMA_ALIGNMENT,			/* alignment */
	    0,					/* boundary */
	    BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL,				/* filtfunc */
	    NULL,				/* fistfuncarg */
	    sc->chan_size, 				/* maxsize */
	    1,					/* nsegments */
	    sc->chan_size, 				/* maxsegsz */
	    0,					/* flags */
	    NULL,				/* lockfunc */
	    NULL,				/* lockfuncarg */
	    &sc->chan_dmat);			/* dmat */
	if (result != 0) {
		device_printf(sc->dev, "%s: bus_dma_tag_create failed (%x)\n",
		     __func__, result);
		free(sc, M_DEVBUF);
		return (ENXIO);
	}


	sc->hdabus = NULL;
	for (i = 0; i < HDAC_CODEC_MAX; i++)
		sc->codecs[i] = NULL;

	pci_enable_busmaster(dev);

	/* Initialize driver mutex */
	sc->lock = snd_mtxcreate(device_get_nameunit(dev), HDAC_MTX_NAME);

	/* Allocate resources */
	result = hdac_mem_alloc(sc);
	if (result != 0)
		goto fail;
	result = hdac_irq_alloc(sc);
	if (result != 0)
		goto fail;

	/* Get Capabilities */
	result = hdac_get_capabilities(sc);
	if (result != 0)
		goto fail;

	/* Allocate CORB and RIRB dma memory */
	result = hdac_dma_alloc(sc, &sc->corb_dma,
	    sc->corb_size * sizeof(uint32_t));
	if (result != 0)
		goto fail;
	result = hdac_dma_alloc(sc, &sc->rirb_dma,
	    sc->rirb_size * sizeof(struct hdac_rirb));
	if (result != 0)
		goto fail;

	/* Quiesce everything */
	hdac_reset(sc);

	/* Disable PCI-Express QOS */
	pci_write_config(sc->dev, 0x44,
	    pci_read_config(sc->dev, 0x44, 1) & 0xf8, 1);

	/* Initialize the CORB and RIRB */
	hdac_corb_init(sc);
	hdac_rirb_init(sc);

	/* Defer remaining of initialization until interrupts are enabled */
	sc->intrhook.ich_func = hdac_attach2;
	sc->intrhook.ich_arg = (void *)sc;
	if (cold == 0 || config_intrhook_establish(&sc->intrhook) != 0) {
		sc->intrhook.ich_func = NULL;
		hdac_attach2((void *)sc);
	}

	return(0);

fail:
	hdac_dma_free(&sc->rirb_dma);
	hdac_dma_free(&sc->corb_dma);
	hdac_irq_free(sc);
	hdac_mem_free(sc);
	snd_mtxfree(sc->lock);

	return(ENXIO);
}

static void
hdac_audio_parse(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w;
	uint32_t res;
	int i;
	nid_t cad, nid;

	cad = devinfo->codec->cad;
	nid = devinfo->nid;

	hdac_command(sc,
	    HDA_CMD_SET_POWER_STATE(cad, nid, HDA_CMD_POWER_STATE_D0), cad);

	DELAY(100);

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad , nid, HDA_PARAM_SUB_NODE_COUNT), cad);

	devinfo->nodecnt = HDA_PARAM_SUB_NODE_COUNT_TOTAL(res);
	devinfo->startnode = HDA_PARAM_SUB_NODE_COUNT_START(res);
	devinfo->endnode = devinfo->startnode + devinfo->nodecnt;

	HDA_BOOTVERBOSE_MSG(
		device_printf(sc->dev, "       Vendor: 0x%08x\n",
		    devinfo->vendor_id);
		device_printf(sc->dev, "       Device: 0x%08x\n",
		    devinfo->device_id);
		device_printf(sc->dev, "     Revision: 0x%08x\n",
		    devinfo->revision_id);
		device_printf(sc->dev, "     Stepping: 0x%08x\n",
		    devinfo->stepping_id);
		device_printf(sc->dev, "PCI Subvendor: 0x%08x\n",
		    sc->pci_subvendor);
		device_printf(sc->dev, "        Nodes: start=%d "
		    "endnode=%d total=%d\n",
		    devinfo->startnode, devinfo->endnode, devinfo->nodecnt);
	);

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_SUPP_STREAM_FORMATS),
	    cad);
	devinfo->function.audio.supp_stream_formats = res;

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_SUPP_PCM_SIZE_RATE),
	    cad);
	devinfo->function.audio.supp_pcm_size_rate = res;

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_OUTPUT_AMP_CAP),
	    cad);
	devinfo->function.audio.outamp_cap = res;

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_INPUT_AMP_CAP),
	    cad);
	devinfo->function.audio.inamp_cap = res;

	if (devinfo->nodecnt > 0) {
		hdac_unlock(sc);
		devinfo->widget = (struct hdac_widget *)malloc(
		    sizeof(*(devinfo->widget)) * devinfo->nodecnt, M_HDAC,
		    M_NOWAIT | M_ZERO);
		hdac_lock(sc);
	} else
		devinfo->widget = NULL;

	if (devinfo->widget == NULL) {
		device_printf(sc->dev, "unable to allocate widgets!\n");
		devinfo->endnode = devinfo->startnode;
		devinfo->nodecnt = 0;
		return;
	}

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL)
			device_printf(sc->dev, "Ghost widget! nid=%d!\n", i);
		else {
			w->devinfo = devinfo;
			w->nid = i;
			w->enable = 1;
			w->selconn = -1;
			w->pflags = 0;
			w->ctlflags = 0;
			w->param.eapdbtl = 0xffffffff;
			hdac_widget_parse(w);
		}
	}
}

static void
hdac_audio_ctl_parse(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_audio_ctl *ctls;
	struct hdac_widget *w, *cw;
	int i, j, cnt, max, ocap, icap;

	/* XXX This is redundant */
	max = 0;
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->param.outamp_cap != 0)
			max++;
		if (w->param.inamp_cap != 0) {
			switch (w->type) {
			case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
			case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER:
				for (j = 0; j < w->nconns; j++) {
					cw = hdac_widget_get(devinfo,
					    w->conns[j]);
					if (cw == NULL || cw->enable == 0)
						continue;
					max++;
				}
				break;
			default:
				max++;
				break;
			}
		}
	}

	devinfo->function.audio.ctlcnt = max;

	if (max < 1)
		return;

	hdac_unlock(sc);
	ctls = (struct hdac_audio_ctl *)malloc(
	    sizeof(*ctls) * max, M_HDAC, M_ZERO | M_NOWAIT);
	hdac_lock(sc);

	if (ctls == NULL) {
		/* Blekh! */
		device_printf(sc->dev, "unable to allocate ctls!\n");
		devinfo->function.audio.ctlcnt = 0;
		return;
	}

	cnt = 0;
	for (i = devinfo->startnode; cnt < max && i < devinfo->endnode; i++) {
		if (cnt >= max) {
			device_printf(sc->dev, "%s: Ctl overflow!\n",
			    __func__);
			break;
		}
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		ocap = w->param.outamp_cap;
		icap = w->param.inamp_cap;
		if (ocap != 0) {
			ctls[cnt].enable = 1;
			ctls[cnt].widget = w;
			ctls[cnt].mute =
			    HDA_PARAM_OUTPUT_AMP_CAP_MUTE_CAP(ocap);
			ctls[cnt].step =
			    HDA_PARAM_OUTPUT_AMP_CAP_NUMSTEPS(ocap);
			ctls[cnt].size =
			    HDA_PARAM_OUTPUT_AMP_CAP_STEPSIZE(ocap);
			ctls[cnt].offset =
			    HDA_PARAM_OUTPUT_AMP_CAP_OFFSET(ocap);
			ctls[cnt].left = ctls[cnt].offset;
			ctls[cnt].right = ctls[cnt].offset;
			ctls[cnt++].dir = HDA_CTL_OUT;
		}

		if (icap != 0) {
			switch (w->type) {
			case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
			case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER:
				for (j = 0; j < w->nconns; j++) {
					if (cnt >= max) {
						device_printf(sc->dev,
						    "%s: Ctl overflow!\n",
						    __func__);
						break;
					}
					cw = hdac_widget_get(devinfo,
					    w->conns[j]);
					if (cw == NULL || cw->enable == 0)
						continue;
					ctls[cnt].enable = 1;
					ctls[cnt].widget = w;
					ctls[cnt].childwidget = cw;
					ctls[cnt].index = j;
					ctls[cnt].mute =
					    HDA_PARAM_INPUT_AMP_CAP_MUTE_CAP(icap);
					ctls[cnt].step =
					    HDA_PARAM_INPUT_AMP_CAP_NUMSTEPS(icap);
					ctls[cnt].size =
					    HDA_PARAM_INPUT_AMP_CAP_STEPSIZE(icap);
					ctls[cnt].offset =
					    HDA_PARAM_INPUT_AMP_CAP_OFFSET(icap);
					ctls[cnt].left = ctls[cnt].offset;
					ctls[cnt].right = ctls[cnt].offset;
					ctls[cnt++].dir = HDA_CTL_IN;
				}
				break;
			default:
				if (cnt >= max) {
					device_printf(sc->dev,
					    "%s: Ctl overflow!\n",
					    __func__);
					break;
				}
				ctls[cnt].enable = 1;
				ctls[cnt].widget = w;
				ctls[cnt].mute =
				    HDA_PARAM_INPUT_AMP_CAP_MUTE_CAP(icap);
				ctls[cnt].step =
				    HDA_PARAM_INPUT_AMP_CAP_NUMSTEPS(icap);
				ctls[cnt].size =
				    HDA_PARAM_INPUT_AMP_CAP_STEPSIZE(icap);
				ctls[cnt].offset =
				    HDA_PARAM_INPUT_AMP_CAP_OFFSET(icap);
				ctls[cnt].left = ctls[cnt].offset;
				ctls[cnt].right = ctls[cnt].offset;
				ctls[cnt++].dir = HDA_CTL_IN;
				break;
			}
		}
	}

	devinfo->function.audio.ctl = ctls;
}

static void
hdac_vendor_patch_parse(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w;
	uint32_t id;
	int i;

	/*
	 * XXX Fixed rate quirk. Other than 48000
	 *     sounds pretty much like train wreck.
	 */
	devinfo->function.audio.quirks |= HDA_QUIRK_FIXEDRATE;
	/*
	 * XXX Force stereo quirk. Monoural recording / playback
	 *     on few codecs (especially ALC880) seems broken or
	 *     or perhaps unsupported.
	 */
	devinfo->function.audio.quirks |= HDA_QUIRK_FORCESTEREO;
	id = hdac_codec_id(devinfo);
	switch (id) {
	case HDA_CODEC_ALC260:
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type !=
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT)
				continue;
			if (w->nid != 5)
				w->enable = 0;
		}
		break;
	case HDA_CODEC_ALC880:
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT &&
			    w->nid != 9 && w->nid != 29) {
					w->enable = 0;
			} else if (w->type !=
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET &&
			    w->nid == 29) {
				w->type = HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET;
				w->param.widget_cap &= ~HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_MASK;
				w->param.widget_cap |=
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET <<
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_SHIFT;
				strlcpy(w->name, "beep widget", sizeof(w->name));
			}
		}
		break;
	case HDA_CODEC_AD1986A:
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type !=
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT)
				continue;
			if (w->nid != 3)
				w->enable = 0;
		}
		break;
	case HDA_CODEC_STAC9221:
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type !=
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT)
				continue;
			if (w->nid != 2)
				w->enable = 0;
		}
		break;
	case HDA_CODEC_STAC9221D:
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT &&
			    w->nid != 6)
				w->enable = 0;

		}
		break;
	case HDA_CODEC_CXVENICE:
		devinfo->function.audio.quirks &= ~HDA_QUIRK_FORCESTEREO;
		break;
	default:
		break;
	}
	if ((HDA_CODEC_STACXXXX & id) == id) {
		/* Sigmatel codecs need soft PCM volume emulation */
		devinfo->function.audio.quirks |= HDA_QUIRK_SOFTPCMVOL;
	}
	if ((ACER_ALL_SUBVENDOR & devinfo->codec->sc->pci_subvendor) ==
	    devinfo->codec->sc->pci_subvendor) {
		/* Acer */
		devinfo->function.audio.quirks |= HDA_QUIRK_GPIO1;
	}
}

static int
hdac_audio_ctl_ossmixer_getnextdev(struct hdac_devinfo *devinfo)
{
	int *dev = &devinfo->function.audio.ossidx;

	while (*dev < SOUND_MIXER_NRDEVICES) {
		switch (*dev) {
		case SOUND_MIXER_VOLUME:
		case SOUND_MIXER_BASS:
		case SOUND_MIXER_TREBLE:
		case SOUND_MIXER_PCM:
		case SOUND_MIXER_SPEAKER:
		case SOUND_MIXER_LINE:
		case SOUND_MIXER_MIC:
		case SOUND_MIXER_CD:
		case SOUND_MIXER_RECLEV:
		case SOUND_MIXER_OGAIN:	/* reserved for EAPD switch */
			(*dev)++;
			break;
		default:
			return (*dev)++;
			break;
		}
	}

	return (-1);
}

static int
hdac_widget_find_dac_path(struct hdac_devinfo *devinfo, nid_t nid, int depth)
{
	struct hdac_widget *w;
	int i, ret = 0;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);
	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
	switch (w->type) {
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT:
		w->pflags |= HDA_DAC_PATH;
		ret = 1;
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER:
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
		for (i = 0; i < w->nconns; i++) {
			if (hdac_widget_find_dac_path(devinfo,
			    w->conns[i], depth + 1) != 0) {
				if (w->selconn == -1)
					w->selconn = i;
				ret = 1;
				w->pflags |= HDA_DAC_PATH;
			}
		}
		break;
	default:
		break;
	}
	return (ret);
}

static int
hdac_widget_find_adc_path(struct hdac_devinfo *devinfo, nid_t nid, int depth)
{
	struct hdac_widget *w;
	int i, conndev, ret = 0;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);
	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
	switch (w->type) {
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT:
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR:
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER:
		for (i = 0; i < w->nconns; i++) {
			if (hdac_widget_find_adc_path(devinfo, w->conns[i],
			    depth + 1) != 0) {
				if (w->selconn == -1)
					w->selconn = i;
				w->pflags |= HDA_ADC_PATH;
				ret = 1;
			}
		}
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX:
		conndev = w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
		if (HDA_PARAM_PIN_CAP_INPUT_CAP(w->wclass.pin.cap) &&
		    (conndev == HDA_CONFIG_DEFAULTCONF_DEVICE_CD ||
		    conndev == HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN ||
		    conndev == HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN)) {
			w->pflags |= HDA_ADC_PATH;
			ret = 1;
		}
		break;
	/*case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER:
		if (w->pflags & HDA_DAC_PATH) {
			w->pflags |= HDA_ADC_PATH;
			ret = 1;
		}
		break;*/
	default:
		break;
	}
	return (ret);
}

static uint32_t
hdac_audio_ctl_outamp_build(struct hdac_devinfo *devinfo,
				nid_t nid, nid_t pnid, int index, int depth)
{
	struct hdac_widget *w, *pw;
	struct hdac_audio_ctl *ctl;
	uint32_t fl = 0;
	int i, ossdev, conndev, strategy;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);

	pw = hdac_widget_get(devinfo, pnid);
	strategy = devinfo->function.audio.parsing_strategy;

	if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER
	    || w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR) {
		for (i = 0; i < w->nconns; i++) {
			fl |= hdac_audio_ctl_outamp_build(devinfo, w->conns[i],
			    w->nid, i, depth + 1);
		}
		w->ctlflags |= fl;
		return (fl);
	} else if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT &&
	    (w->pflags & HDA_DAC_PATH)) {
		i = 0;
		while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
			if (ctl->enable == 0 || ctl->widget == NULL)
				continue;
			if ((ctl->widget->nid == w->nid) ||
			    (ctl->widget->nid == pnid && ctl->index == index &&
			    (ctl->dir & HDA_CTL_IN)) ||
			    (ctl->widget->nid == pnid && pw != NULL &&
			    pw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR &&
			    (pw->nconns < 2 || pw->selconn == index ||
			    pw->selconn == -1) &&
			    (ctl->dir & HDA_CTL_OUT)) ||
			    (strategy == HDA_PARSE_DIRECT &&
			    ctl->widget->nid == w->nid)) {
				if (pw != NULL && pw->selconn == -1)
					pw->selconn = index;
				fl |= SOUND_MASK_VOLUME;
				fl |= SOUND_MASK_PCM;
				ctl->ossmask |= SOUND_MASK_VOLUME;
				ctl->ossmask |= SOUND_MASK_PCM;
				ctl->ossdev = SOUND_MIXER_PCM;
			}
		}
		w->ctlflags |= fl;
		return (fl);
	} else if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX
	    && HDA_PARAM_PIN_CAP_INPUT_CAP(w->wclass.pin.cap) &&
	    (w->pflags & HDA_ADC_PATH)) {
		conndev = w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
		i = 0;
		while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
			if (ctl->enable == 0 || ctl->widget == NULL)
				continue;
			if (((ctl->widget->nid == pnid && ctl->index == index &&
			    (ctl->dir & HDA_CTL_IN)) ||
			    (ctl->widget->nid == pnid && pw != NULL &&
			    pw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR &&
			    (pw->nconns < 2 || pw->selconn == index ||
			    pw->selconn == -1) &&
			    (ctl->dir & HDA_CTL_OUT)) ||
			    (strategy == HDA_PARSE_DIRECT &&
			    ctl->widget->nid == w->nid)) &&
			    (ctl->ossmask & ~SOUND_MASK_VOLUME) == 0) {
				if (pw != NULL && pw->selconn == -1)
					pw->selconn = index;
				ossdev = 0;
				switch (conndev) {
				case HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN:
					ossdev = SOUND_MIXER_MIC;
					break;
				case HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN:
					ossdev = SOUND_MIXER_LINE;
					break;
				case HDA_CONFIG_DEFAULTCONF_DEVICE_CD:
					ossdev = SOUND_MIXER_CD;
					break;
				default:
					ossdev =
					    hdac_audio_ctl_ossmixer_getnextdev(
					    devinfo);
					if (ossdev < 0)
						ossdev = 0;
					break;
				}
				if (strategy == HDA_PARSE_MIXER) {
					fl |= SOUND_MASK_VOLUME;
					ctl->ossmask |= SOUND_MASK_VOLUME;
				}
				fl |= 1 << ossdev;
				ctl->ossmask |= 1 << ossdev;
				ctl->ossdev = ossdev;
			}
		}
		w->ctlflags |= fl;
		return (fl);
	} else if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET) {
		i = 0;
		while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
			if (ctl->enable == 0 || ctl->widget == NULL)
				continue;
			if (((ctl->widget->nid == pnid && ctl->index == index &&
			    (ctl->dir & HDA_CTL_IN)) ||
			    (ctl->widget->nid == pnid && pw != NULL &&
			    pw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR &&
			    (pw->nconns < 2 || pw->selconn == index ||
			    pw->selconn == -1) &&
			    (ctl->dir & HDA_CTL_OUT)) ||
			    (strategy == HDA_PARSE_DIRECT &&
			    ctl->widget->nid == w->nid)) &&
			    (ctl->ossmask & ~SOUND_MASK_VOLUME) == 0) {
				if (pw != NULL && pw->selconn == -1)
					pw->selconn = index;
				fl |= SOUND_MASK_VOLUME;
				fl |= SOUND_MASK_SPEAKER;
				ctl->ossmask |= SOUND_MASK_VOLUME;
				ctl->ossmask |= SOUND_MASK_SPEAKER;
				ctl->ossdev = SOUND_MIXER_SPEAKER;
			}
		}
		w->ctlflags |= fl;
		return (fl);
	}
	return (0);
}

static uint32_t
hdac_audio_ctl_inamp_build(struct hdac_devinfo *devinfo, nid_t nid, int depth)
{
	struct hdac_widget *w, *cw;
	struct hdac_audio_ctl *ctl;
	uint32_t fl;
	int i;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
	/*if (!(w->pflags & HDA_ADC_PATH))
		return (0);
	if (!(w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT ||
	    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR))
		return (0);*/
	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0 || ctl->widget == NULL)
			continue;
		if (ctl->widget->nid == nid) {
			ctl->ossmask |= SOUND_MASK_RECLEV;
			w->ctlflags |= SOUND_MASK_RECLEV;
			return (SOUND_MASK_RECLEV);
		}
	}
	for (i = 0; i < w->nconns; i++) {
		cw = hdac_widget_get(devinfo, w->conns[i]);
		if (cw == NULL || cw->enable == 0)
			continue;
		if (cw->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR)
			continue;
		fl = hdac_audio_ctl_inamp_build(devinfo, cw->nid, depth + 1);
		if (fl != 0) {
			cw->ctlflags |= fl;
			w->ctlflags |= fl;
			return (fl);
		}
	}
	return (0);
}

static int
hdac_audio_ctl_recsel_build(struct hdac_devinfo *devinfo, nid_t nid, int depth)
{
	struct hdac_widget *w, *cw;
	int i, child = 0;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
	/*if (!(w->pflags & HDA_ADC_PATH))
		return (0);
	if (!(w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT ||
	    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR))
		return (0);*/
	/* XXX weak! */
	for (i = 0; i < w->nconns; i++) {
		cw = hdac_widget_get(devinfo, w->conns[i]);
		if (cw == NULL)
			continue;
		if (++child > 1) {
			w->pflags |= HDA_ADC_RECSEL;
			return (1);
		}
	}
	for (i = 0; i < w->nconns; i++) {
		if (hdac_audio_ctl_recsel_build(devinfo,
		    w->conns[i], depth + 1) != 0)
			return (1);
	}
	return (0);
}

static int
hdac_audio_build_tree_strategy(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w, *cw;
	int i, j, conndev, found_dac = 0;
	int strategy;

	strategy = devinfo->function.audio.parsing_strategy;

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if (!HDA_PARAM_PIN_CAP_OUTPUT_CAP(w->wclass.pin.cap))
			continue;
		conndev = w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
		if (!(conndev == HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT ||
		    conndev == HDA_CONFIG_DEFAULTCONF_DEVICE_SPEAKER ||
		    conndev == HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_OUT))
			continue;
		for (j = 0; j < w->nconns; j++) {
			cw = hdac_widget_get(devinfo, w->conns[j]);
			if (cw == NULL || cw->enable == 0)
				continue;
			if (strategy == HDA_PARSE_MIXER && !(cw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER ||
			    cw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR))
				continue;
			if (hdac_widget_find_dac_path(devinfo, cw->nid, 0)
			    != 0) {
				if (w->selconn == -1)
					w->selconn = j;
				w->pflags |= HDA_DAC_PATH;
				found_dac++;
			}
		}
	}

	return (found_dac);
}

static void
hdac_audio_build_tree(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	int i, j, dacs, strategy;

	/* Construct DAC path */
	strategy = HDA_PARSE_MIXER;
	devinfo->function.audio.parsing_strategy = strategy;
	HDA_BOOTVERBOSE_MSG(
		device_printf(devinfo->codec->sc->dev,
		    "HWiP: HDA Widget Parser - Revision %d\n",
		    HDA_WIDGET_PARSER_REV);
	);
	dacs = hdac_audio_build_tree_strategy(devinfo);
	if (dacs == 0) {
		HDA_BOOTVERBOSE_MSG(
			device_printf(devinfo->codec->sc->dev,
			    "HWiP: 0 DAC found! Retrying parser "
			    "using HDA_PARSE_DIRECT strategy.\n");
		);
		strategy = HDA_PARSE_DIRECT;
		devinfo->function.audio.parsing_strategy = strategy;
		dacs = hdac_audio_build_tree_strategy(devinfo);
	}

	HDA_BOOTVERBOSE_MSG(
		device_printf(devinfo->codec->sc->dev,
		    "HWiP: Found %d DAC(s) using HDA_PARSE_%s strategy.\n",
		    dacs, (strategy == HDA_PARSE_MIXER) ? "MIXER" : "DIRECT");
	);

	/* Construct ADC path */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT)
			continue;
		(void)hdac_widget_find_adc_path(devinfo, w->nid, 0);
	}

	/* Output mixers */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if ((strategy == HDA_PARSE_MIXER &&
		    (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER ||
		    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR)
		    && (w->pflags & HDA_DAC_PATH)) ||
		    (strategy == HDA_PARSE_DIRECT && (w->pflags &
		    (HDA_DAC_PATH | HDA_ADC_PATH)))) {
			w->ctlflags |= hdac_audio_ctl_outamp_build(devinfo,
			    w->nid, devinfo->startnode - 1, 0, 0);
		} else if (w->type ==
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET) {
			j = 0;
			while ((ctl = hdac_audio_ctl_each(devinfo, &j)) !=
			    NULL) {
				if (ctl->enable == 0 || ctl->widget == NULL)
					continue;
				if (ctl->widget->nid != w->nid)
					continue;
				ctl->ossmask |= SOUND_MASK_VOLUME;
				ctl->ossmask |= SOUND_MASK_SPEAKER;
				ctl->ossdev = SOUND_MIXER_SPEAKER;
				w->ctlflags |= SOUND_MASK_VOLUME;
				w->ctlflags |= SOUND_MASK_SPEAKER;
			}
		}
	}

	/* Input mixers (rec) */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (!(w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT &&
		    w->pflags & HDA_ADC_PATH))
			continue;
		hdac_audio_ctl_inamp_build(devinfo, w->nid, 0);
		hdac_audio_ctl_recsel_build(devinfo, w->nid, 0);
	}
}

#define HDA_COMMIT_CONN	(1 << 0)
#define HDA_COMMIT_CTRL	(1 << 1)
#define HDA_COMMIT_EAPD	(1 << 2)
#define HDA_COMMIT_GPIO	(1 << 3)
#define HDA_COMMIT_ALL	(HDA_COMMIT_CONN | HDA_COMMIT_CTRL | \
				HDA_COMMIT_EAPD | HDA_COMMIT_GPIO)

static void
hdac_audio_commit(struct hdac_devinfo *devinfo, uint32_t cfl)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w;
	nid_t cad;
	int i;

	if (!(cfl & HDA_COMMIT_ALL))
		return;

	cad = devinfo->codec->cad;

	if ((cfl & HDA_COMMIT_GPIO)) {
		if (devinfo->function.audio.quirks & HDA_QUIRK_GPIO1) {
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_ENABLE_MASK(cad, 0x01,
			    0x01), cad);
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_DIRECTION(cad, 0x01,
			    0x01), cad);
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_DATA(cad, 0x01,
			    0x01), cad);
		}
		if (devinfo->function.audio.quirks & HDA_QUIRK_GPIO2) {
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_ENABLE_MASK(cad, 0x01,
			    0x02), cad);
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_DIRECTION(cad, 0x01,
			    0x02), cad);
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_DATA(cad, 0x01,
			    0x02), cad);
		}
	}

	for (i = 0; i < devinfo->nodecnt; i++) {
		w = &devinfo->widget[i];
		if (w == NULL || w->enable == 0)
			continue;
		if (cfl & HDA_COMMIT_CONN) {
			if (w->selconn == -1)
				w->selconn = 0;
			if (w->nconns > 0)
				hdac_widget_connection_select(w, w->selconn);
		}
		if ((cfl & HDA_COMMIT_CTRL) &&
		    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) {
			if ((w->pflags & (HDA_DAC_PATH | HDA_ADC_PATH)) ==
			    (HDA_DAC_PATH | HDA_ADC_PATH))
				device_printf(sc->dev, "WARNING: node %d "
				    "participate both for DAC/ADC!\n", w->nid);
			if (w->pflags & HDA_DAC_PATH) {
				w->wclass.pin.ctrl &=
				    ~HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE;
				if ((w->wclass.pin.config &
				    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK) !=
				    HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT)
					w->wclass.pin.ctrl &=
					    ~HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE;
			} else if (w->pflags & HDA_ADC_PATH) {
				w->wclass.pin.ctrl &=
				    ~(HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
				    HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE);
			} else
				w->wclass.pin.ctrl &= ~(
				    HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE |
				    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
				    HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE);
			hdac_command(sc,
			    HDA_CMD_SET_PIN_WIDGET_CTRL(cad, w->nid,
			    w->wclass.pin.ctrl), cad);
		}
		if ((cfl & HDA_COMMIT_EAPD) &&
		    w->param.eapdbtl != 0xffffffff)
			hdac_command(sc,
			    HDA_CMD_SET_EAPD_BTL_ENABLE(cad, w->nid,
			    w->param.eapdbtl), cad);

		DELAY(1000);
	}
}

static void
hdac_audio_ctl_commit(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_audio_ctl *ctl;
	int i;

	devinfo->function.audio.mvol = 100 | (100 << 8);
	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0 || ctl->widget == NULL) {
			HDA_BOOTVERBOSE_MSG(
				device_printf(sc->dev, "[%2d] Ctl nid=%d",
				    i, (ctl->widget != NULL) ?
				    ctl->widget->nid : -1);
				if (ctl->childwidget != NULL)
					printf(" childnid=%d",
					    ctl->childwidget->nid);
				if (ctl->widget == NULL)
					printf(" NULL WIDGET!");
				printf(" DISABLED\n");
			);
			continue;
		}
		HDA_BOOTVERBOSE_MSG(
			if (ctl->ossmask == 0) {
				device_printf(sc->dev, "[%2d] Ctl nid=%d",
				    i, ctl->widget->nid);
				if (ctl->childwidget != NULL)
					printf(" childnid=%d",
					ctl->childwidget->nid);
				printf(" Bind to NONE\n");
		}
		);
		if (ctl->step > 0) {
			ctl->ossval = (ctl->left * 100) / ctl->step;
			ctl->ossval |= ((ctl->right * 100) / ctl->step) << 8;
		} else
			ctl->ossval = 0;
		hdac_audio_ctl_amp_set(ctl, HDA_AMP_MUTE_DEFAULT,
		    ctl->left, ctl->right);
	}
}

static int
hdac_pcmchannel_setup(struct hdac_devinfo *devinfo, int dir)
{
	struct hdac_chan *ch;
	struct hdac_widget *w;
	uint32_t cap, fmtcap, pcmcap, path;
	int i, type, ret, max;

	if (dir == PCMDIR_PLAY) {
		type = HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT;
		ch = &devinfo->codec->sc->play;
		path = HDA_DAC_PATH;
	} else {
		type = HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT;
		ch = &devinfo->codec->sc->rec;
		path = HDA_ADC_PATH;
	}

	ch->caps = hdac_caps;
	ch->caps.fmtlist = ch->fmtlist;
	ch->bit16 = 1;
	ch->bit32 = 0;
	ch->pcmrates[0] = 48000;
	ch->pcmrates[1] = 0;

	ret = 0;
	fmtcap = devinfo->function.audio.supp_stream_formats;
	pcmcap = devinfo->function.audio.supp_pcm_size_rate;
	max = (sizeof(ch->io) / sizeof(ch->io[0])) - 1;

	for (i = devinfo->startnode; i < devinfo->endnode && ret < max; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0 || w->type != type ||
		    (w->pflags & path) == 0)
			continue;
		cap = w->param.widget_cap;
		/*if (HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(cap))
			continue;*/
		if (!HDA_PARAM_AUDIO_WIDGET_CAP_STEREO(cap))
			continue;
		cap = w->param.supp_stream_formats;
		/*if (HDA_PARAM_SUPP_STREAM_FORMATS_AC3(cap)) {
		}
		if (HDA_PARAM_SUPP_STREAM_FORMATS_FLOAT32(cap)) {
		}*/
		if (!HDA_PARAM_SUPP_STREAM_FORMATS_PCM(cap))
			continue;
		ch->io[ret++] = i;
		fmtcap &= w->param.supp_stream_formats;
		pcmcap &= w->param.supp_pcm_size_rate;
	}
	ch->io[ret] = -1;

	ch->supp_stream_formats = fmtcap;
	ch->supp_pcm_size_rate = pcmcap;

	/*
	 *  8bit = 0
	 * 16bit = 1
	 * 20bit = 2
	 * 24bit = 3
	 * 32bit = 4
	 */
	if (ret > 0) {
		cap = pcmcap;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_16BIT(cap))
			ch->bit16 = 1;
		else if (HDA_PARAM_SUPP_PCM_SIZE_RATE_8BIT(cap))
			ch->bit16 = 0;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_32BIT(cap))
			ch->bit32 = 4;
		else if (HDA_PARAM_SUPP_PCM_SIZE_RATE_24BIT(cap))
			ch->bit32 = 3;
		else if (HDA_PARAM_SUPP_PCM_SIZE_RATE_20BIT(cap))
			ch->bit32 = 2;
		i = 0;
		if (!(devinfo->function.audio.quirks & HDA_QUIRK_FORCESTEREO))
			ch->fmtlist[i++] = AFMT_S16_LE;
		ch->fmtlist[i++] = AFMT_S16_LE | AFMT_STEREO;
		if (ch->bit32 > 0) {
			if (!(devinfo->function.audio.quirks &
			    HDA_QUIRK_FORCESTEREO))
				ch->fmtlist[i++] = AFMT_S32_LE;
			ch->fmtlist[i++] = AFMT_S32_LE | AFMT_STEREO;
		}
		ch->fmtlist[i] = 0;
		i = 0;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_8KHZ(cap))
			ch->pcmrates[i++] = 8000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_11KHZ(cap))
			ch->pcmrates[i++] = 11025;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_16KHZ(cap))
			ch->pcmrates[i++] = 16000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_22KHZ(cap))
			ch->pcmrates[i++] = 22050;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_32KHZ(cap))
			ch->pcmrates[i++] = 32000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_44KHZ(cap))
			ch->pcmrates[i++] = 44100;
		/* if (HDA_PARAM_SUPP_PCM_SIZE_RATE_48KHZ(cap)) */
		ch->pcmrates[i++] = 48000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_88KHZ(cap))
			ch->pcmrates[i++] = 88200;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_96KHZ(cap))
			ch->pcmrates[i++] = 96000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_176KHZ(cap))
			ch->pcmrates[i++] = 176400;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_192KHZ(cap))
			ch->pcmrates[i++] = 192000;
		/* if (HDA_PARAM_SUPP_PCM_SIZE_RATE_384KHZ(cap)) */
		ch->pcmrates[i] = 0;
		if (i > 0) {
			ch->caps.minspeed = ch->pcmrates[0];
			ch->caps.maxspeed = ch->pcmrates[i - 1];
		}
	}

	return (ret);
}

static void
hdac_dump_ctls(struct hdac_devinfo *devinfo, const char *banner, uint32_t flag)
{
	struct hdac_audio_ctl *ctl;
	struct hdac_softc *sc = devinfo->codec->sc;
	int i;
	uint32_t fl = 0;


	if (flag == 0) {
		fl = SOUND_MASK_VOLUME | SOUND_MASK_PCM |
		    SOUND_MASK_CD | SOUND_MASK_LINE | SOUND_MASK_RECLEV |
		    SOUND_MASK_MIC | SOUND_MASK_SPEAKER | SOUND_MASK_OGAIN;
	}

	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0 || ctl->widget == NULL ||
		    ctl->widget->enable == 0)
			continue;
		if ((flag == 0 && (ctl->ossmask & ~fl)) ||
		    (flag != 0 && (ctl->ossmask & flag))) {
			if (banner != NULL) {
				device_printf(sc->dev, "\n");
				device_printf(sc->dev, "%s\n", banner);
			}
			goto hdac_ctl_dump_it_all;
		}
	}

	return;

hdac_ctl_dump_it_all:
	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0 || ctl->widget == NULL ||
		    ctl->widget->enable == 0)
			continue;
		if (!((flag == 0 && (ctl->ossmask & ~fl)) ||
		    (flag != 0 && (ctl->ossmask & flag))))
			continue;
		if (flag == 0) {
			device_printf(sc->dev, "\n");
			device_printf(sc->dev, "Unknown Ctl (OSS: %s)\n",
			    hdac_audio_ctl_ossmixer_mask2name(ctl->ossmask));
		}
		device_printf(sc->dev, "   |\n");
		device_printf(sc->dev, "   +-  nid: %2d index: %2d ",
		    ctl->widget->nid, ctl->index);
		if (ctl->childwidget != NULL)
			printf("(nid: %2d) ", ctl->childwidget->nid);
		else
			printf("          ");
		printf("mute: %d step: %3d size: %3d off: %3d dir=0x%x ossmask=0x%08x\n",
		    ctl->mute, ctl->step, ctl->size, ctl->offset, ctl->dir,
		    ctl->ossmask);
	}
}

static void
hdac_dump_audio_formats(struct hdac_softc *sc, uint32_t fcap, uint32_t pcmcap)
{
	uint32_t cap;

	cap = fcap;
	if (cap != 0) {
		device_printf(sc->dev, "     Stream cap: 0x%08x\n", cap);
		device_printf(sc->dev, "         Format:");
		if (HDA_PARAM_SUPP_STREAM_FORMATS_AC3(cap))
			printf(" AC3");
		if (HDA_PARAM_SUPP_STREAM_FORMATS_FLOAT32(cap))
			printf(" FLOAT32");
		if (HDA_PARAM_SUPP_STREAM_FORMATS_PCM(cap))
			printf(" PCM");
		printf("\n");
	}
	cap = pcmcap;
	if (cap != 0) {
		device_printf(sc->dev, "        PCM cap: 0x%08x\n", cap);
		device_printf(sc->dev, "       PCM size:");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_8BIT(cap))
			printf(" 8");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_16BIT(cap))
			printf(" 16");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_20BIT(cap))
			printf(" 20");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_24BIT(cap))
			printf(" 24");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_32BIT(cap))
			printf(" 32");
		printf("\n");
		device_printf(sc->dev, "       PCM rate:");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_8KHZ(cap))
			printf(" 8");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_11KHZ(cap))
			printf(" 11");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_16KHZ(cap))
			printf(" 16");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_22KHZ(cap))
			printf(" 22");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_32KHZ(cap))
			printf(" 32");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_44KHZ(cap))
			printf(" 44");
		printf(" 48");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_88KHZ(cap))
			printf(" 88");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_96KHZ(cap))
			printf(" 96");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_176KHZ(cap))
			printf(" 176");
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_192KHZ(cap))
			printf(" 192");
		printf("\n");
	}
}

static void
hdac_dump_pin(struct hdac_softc *sc, struct hdac_widget *w)
{
	uint32_t pincap, wcap;

	pincap = w->wclass.pin.cap;
	wcap = w->param.widget_cap;

	device_printf(sc->dev, "        Pin cap: 0x%08x\n", pincap);
	device_printf(sc->dev, "                ");
	if (HDA_PARAM_PIN_CAP_IMP_SENSE_CAP(pincap))
		printf(" ISC");
	if (HDA_PARAM_PIN_CAP_TRIGGER_REQD(pincap))
		printf(" TRQD");
	if (HDA_PARAM_PIN_CAP_PRESENCE_DETECT_CAP(pincap))
		printf(" PDC");
	if (HDA_PARAM_PIN_CAP_HEADPHONE_CAP(pincap))
		printf(" HP");
	if (HDA_PARAM_PIN_CAP_OUTPUT_CAP(pincap))
		printf(" OUT");
	if (HDA_PARAM_PIN_CAP_INPUT_CAP(pincap))
		printf(" IN");
	if (HDA_PARAM_PIN_CAP_BALANCED_IO_PINS(pincap))
		printf(" BAL");
	if (HDA_PARAM_PIN_CAP_EAPD_CAP(pincap))
		printf(" EAPD");
	if (HDA_PARAM_AUDIO_WIDGET_CAP_UNSOL_CAP(wcap))
		printf(" : UNSOL");
	printf("\n");
	device_printf(sc->dev, "     Pin config: 0x%08x\n",
	    w->wclass.pin.config);
	device_printf(sc->dev, "    Pin control: 0x%08x", w->wclass.pin.ctrl);
	if (w->wclass.pin.ctrl & HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE)
		printf(" HP");
	if (w->wclass.pin.ctrl & HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE)
		printf(" IN");
	if (w->wclass.pin.ctrl & HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE)
		printf(" OUT");
	printf("\n");
}

static void
hdac_dump_amp(struct hdac_softc *sc, uint32_t cap, char *banner)
{
	device_printf(sc->dev, "     %s amp: 0x%0x\n", banner, cap);
	device_printf(sc->dev, "                 "
	    "mute=%d step=%d size=%d offset=%d\n",
	    HDA_PARAM_OUTPUT_AMP_CAP_MUTE_CAP(cap),
	    HDA_PARAM_OUTPUT_AMP_CAP_NUMSTEPS(cap),
	    HDA_PARAM_OUTPUT_AMP_CAP_STEPSIZE(cap),
	    HDA_PARAM_OUTPUT_AMP_CAP_OFFSET(cap));
}

static void
hdac_dump_nodes(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w, *cw;
	int i, j;

	device_printf(sc->dev, "\n");
	device_printf(sc->dev, "Default Parameter\n");
	device_printf(sc->dev, "-----------------\n");
	hdac_dump_audio_formats(sc,
	    devinfo->function.audio.supp_stream_formats,
	    devinfo->function.audio.supp_pcm_size_rate);
	device_printf(sc->dev, "         IN amp: 0x%08x\n",
	    devinfo->function.audio.inamp_cap);
	device_printf(sc->dev, "        OUT amp: 0x%08x\n",
	    devinfo->function.audio.outamp_cap);
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL) {
			device_printf(sc->dev, "Ghost widget nid=%d\n", i);
			continue;
		}
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "            nid: %d [%s]%s\n", w->nid,
		    HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap) ?
		    "DIGITAL" : "ANALOG",
		    (w->enable == 0) ? " [DISABLED]" : "");
		device_printf(sc->dev, "           name: %s\n", w->name);
		device_printf(sc->dev, "     widget_cap: 0x%08x\n",
		    w->param.widget_cap);
		device_printf(sc->dev, "    Parse flags: 0x%08x\n",
		    w->pflags);
		device_printf(sc->dev, "      Ctl flags: 0x%08x\n",
		    w->ctlflags);
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT ||
		    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT) {
			hdac_dump_audio_formats(sc,
			    w->param.supp_stream_formats,
			    w->param.supp_pcm_size_rate);
		} else if (w->type ==
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			hdac_dump_pin(sc, w);
		if (w->param.eapdbtl != 0xffffffff)
			device_printf(sc->dev, "           EAPD: 0x%08x\n",
			    w->param.eapdbtl);
		if (HDA_PARAM_AUDIO_WIDGET_CAP_OUT_AMP(w->param.widget_cap))
			hdac_dump_amp(sc, w->param.outamp_cap, "Output");
		if (HDA_PARAM_AUDIO_WIDGET_CAP_IN_AMP(w->param.widget_cap))
			hdac_dump_amp(sc, w->param.inamp_cap, " Input");
		device_printf(sc->dev, "    connections: %d\n", w->nconns);
		for (j = 0; j < w->nconns; j++) {
			cw = hdac_widget_get(devinfo, w->conns[j]);
			device_printf(sc->dev, "          |\n");
			device_printf(sc->dev, "          + <- nid=%d [%s]",
			    w->conns[j], (cw == NULL) ? "GHOST!" : cw->name);
			if (cw == NULL)
				printf(" [UNKNOWN]");
			else if (cw->enable == 0)
				printf(" [DISABLED]");
			if (w->nconns > 1 && w->selconn == j && w->type !=
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER)
				printf(" (selected)");
			printf("\n");
		}
	}

}

static void
hdac_dump_dac(struct hdac_devinfo *devinfo)
{
	/* XXX TODO */
}

static void
hdac_dump_adc(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w, *cw;
	struct hdac_softc *sc = devinfo->codec->sc;
	int i, j;
	int printed = 0;
	char ossdevs[256];

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (!(w->pflags & HDA_ADC_RECSEL))
			continue;
		if (printed == 0) {
			printed = 1;
			device_printf(sc->dev, "\n");
			device_printf(sc->dev, "Recording sources:\n");
		}
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "    nid=%d [%s]\n", w->nid, w->name);
		for (j = 0; j < w->nconns; j++) {
			cw = hdac_widget_get(devinfo, w->conns[j]);
			if (cw == NULL || cw->enable == 0)
				continue;
			hdac_audio_ctl_ossmixer_mask2allname(cw->ctlflags,
			    ossdevs, sizeof(ossdevs));
			device_printf(sc->dev, "      |\n");
			device_printf(sc->dev, "      + <- nid=%d [%s]",
			    cw->nid, cw->name);
			if (strlen(ossdevs) > 0) {
				printf(" [recsrc: %s]", ossdevs);
			}
			printf("\n");
		}
	}
}

static void
hdac_dump_pcmchannels(struct hdac_softc *sc, int pcnt, int rcnt)
{
	nid_t *nids;

	if (pcnt > 0) {
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "   PCM Playback: %d\n", pcnt);
		hdac_dump_audio_formats(sc, sc->play.supp_stream_formats,
		    sc->play.supp_pcm_size_rate);
		device_printf(sc->dev, "            DAC:");
		for (nids = sc->play.io; *nids != -1; nids++)
			printf(" %d", *nids);
		printf("\n");
	}

	if (rcnt > 0) {
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "     PCM Record: %d\n", rcnt);
		hdac_dump_audio_formats(sc, sc->play.supp_stream_formats,
		    sc->rec.supp_pcm_size_rate);
		device_printf(sc->dev, "            ADC:");
		for (nids = sc->rec.io; *nids != -1; nids++)
			printf(" %d", *nids);
		printf("\n");
	}
}

static void
hdac_attach2(void *arg)
{
	struct hdac_softc *sc;
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	int pcnt, rcnt;
	int i;
	char status[SND_STATUSLEN];
	device_t *devlist;
	int devcount;
	struct hdac_devinfo *devinfo = NULL;

	sc = (struct hdac_softc *)arg;


	hdac_lock(sc);

	/* Remove ourselves from the config hooks */
	if (sc->intrhook.ich_func != NULL) {
		config_intrhook_disestablish(&sc->intrhook);
		sc->intrhook.ich_func = NULL;
	}

	/* Start the corb and rirb engines */
	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: Starting CORB Engine...\n");
	);
	hdac_corb_start(sc);
	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: Starting RIRB Engine...\n");
	);
	hdac_rirb_start(sc);

	HDA_DEBUG_MSG(
		device_printf(sc->dev,
		    "HDA_DEBUG: Enabling controller interrupt...\n");
	);
	HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, HDAC_INTCTL_CIE | HDAC_INTCTL_GIE);

	DELAY(1000);

	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: Scanning HDA codecs...\n");
	);
	hdac_scan_codecs(sc);

	device_get_children(sc->dev, &devlist, &devcount);
	if (devcount != 0 && devlist != NULL) {
		for (i = 0; i < devcount; i++) {
			devinfo = (struct hdac_devinfo *)
			    device_get_ivars(devlist[i]);
			if (devinfo != NULL &&
					devinfo->node_type ==
					HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO) {
				break;
			} else
				devinfo = NULL;
		}
	}

	if (devinfo == NULL) {
		hdac_unlock(sc);
		device_printf(sc->dev, "Audio Function Group not found!\n");
		return;
	}

	HDA_DEBUG_MSG(
		device_printf(sc->dev,
		    "HDA_DEBUG: Parsing AFG nid=%d cad=%d\n",
		    devinfo->nid, devinfo->codec->cad);
	);
	hdac_audio_parse(devinfo);
	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: Parsing Ctls...\n");
	);
	hdac_audio_ctl_parse(devinfo);
	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: Parsing vendor patch...\n");
	);
	hdac_vendor_patch_parse(devinfo);

	/* XXX Disable all DIGITAL path. */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL)
			continue;
		if (HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap)) {
			w->enable = 0;
			continue;
		}
		/* XXX Disable useless pin ? */
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
		    (w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK) ==
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_NONE)
			w->enable = 0;
	}
	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->widget == NULL)
			continue;
		w = ctl->widget;
		if (w->enable == 0)
			ctl->enable = 0;
		if (HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap))
			ctl->enable = 0;
		w = ctl->childwidget;
		if (w == NULL)
			continue;
		if (w->enable == 0 ||
		    HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap))
			ctl->enable = 0;
	}

	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: Building AFG tree...\n");
	);
	hdac_audio_build_tree(devinfo);

	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: AFG commit...\n");
	);
	hdac_audio_commit(devinfo, HDA_COMMIT_ALL);
	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: Ctls commit...\n");
	);
	hdac_audio_ctl_commit(devinfo);

	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: PCMDIR_PLAY setup...\n");
	);
	pcnt = hdac_pcmchannel_setup(devinfo, PCMDIR_PLAY);
	HDA_DEBUG_MSG(
		device_printf(sc->dev, "HDA_DEBUG: PCMDIR_REC setup...\n");
	);
	rcnt = hdac_pcmchannel_setup(devinfo, PCMDIR_REC);

	hdac_unlock(sc);
	HDA_DEBUG_MSG(
		device_printf(sc->dev,
		    "HDA_DEBUG: OSS mixer initialization...\n");
	);
	if (mixer_init(sc->dev, &hdac_audio_ctl_ossmixer_class, devinfo)) {
		device_printf(sc->dev, "Can't register mixer\n");
	}

	if (pcnt > 0)
		pcnt = 1;
	if (rcnt > 0)
		rcnt = 1;

	HDA_DEBUG_MSG(
		device_printf(sc->dev,
		    "HDA_DEBUG: Registering PCM channels...\n");
	);
	if (pcm_register(sc->dev, devinfo, pcnt, rcnt)) {
		device_printf(sc->dev, "Can't register PCM\n");
	}

	sc->registered++;

	for (i = 0; i < pcnt; i++)
		pcm_addchan(sc->dev, PCMDIR_PLAY, &hdac_channel_class, devinfo);
	for (i = 0; i < rcnt; i++)
		pcm_addchan(sc->dev, PCMDIR_REC, &hdac_channel_class, devinfo);

	snprintf(status, SND_STATUSLEN, "at memory 0x%lx irq %ld %s [%s]",
			rman_get_start(sc->mem.mem_res),
			rman_get_start(sc->irq.irq_res),
			PCM_KLDSTRING(snd_hda), HDA_DRV_TEST_REV);
	pcm_setstatus(sc->dev, status);
	device_printf(sc->dev, "<HDA Codec: %s>\n", hdac_codec_name(devinfo));
	device_printf(sc->dev, "<HDA Driver Revision: %s>\n", HDA_DRV_TEST_REV);

	HDA_BOOTVERBOSE_MSG(
		if (devinfo->function.audio.quirks != 0) {
			device_printf(sc->dev, "\n");
			device_printf(sc->dev, "HDA quirks:");
			if (devinfo->function.audio.quirks &
			    HDA_QUIRK_GPIO1)
				printf(" GPIO1");
			if (devinfo->function.audio.quirks &
			    HDA_QUIRK_GPIO2)
				printf(" GPIO2");
			if (devinfo->function.audio.quirks &
			    HDA_QUIRK_SOFTPCMVOL)
				printf(" SOFTPCMVOL");
			if (devinfo->function.audio.quirks &
			    HDA_QUIRK_FIXEDRATE)
				printf(" FIXEDRATE");
			if (devinfo->function.audio.quirks &
			    HDA_QUIRK_FORCESTEREO)
				printf(" FORCESTEREO");
			printf("\n");
		}
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "+-------------------+\n");
		device_printf(sc->dev, "| DUMPING HDA NODES |\n");
		device_printf(sc->dev, "+-------------------+\n");
		hdac_dump_nodes(devinfo);
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "+------------------------+\n");
		device_printf(sc->dev, "| DUMPING HDA AMPLIFIERS |\n");
		device_printf(sc->dev, "+------------------------+\n");
		device_printf(sc->dev, "\n");
		i = 0;
		while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
			device_printf(sc->dev, "%3d: nid=%d", i,
			    (ctl->widget != NULL) ? ctl->widget->nid : -1);
			if (ctl->childwidget != NULL)
				printf(" cnid=%d", ctl->childwidget->nid);
			printf(" dir=0x%x index=%d "
			    "ossmask=0x%08x ossdev=%d%s\n",
			    ctl->dir, ctl->index,
			    ctl->ossmask, ctl->ossdev,
			    (ctl->enable == 0) ? " [DISABLED]" : "");
		}
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "+-----------------------------------+\n");
		device_printf(sc->dev, "| DUMPING HDA AUDIO/VOLUME CONTROLS |\n");
		device_printf(sc->dev, "+-----------------------------------+\n");
		hdac_dump_ctls(devinfo, "Master Volume (OSS: vol)", SOUND_MASK_VOLUME);
		hdac_dump_ctls(devinfo, "PCM Volume (OSS: pcm)", SOUND_MASK_PCM);
		hdac_dump_ctls(devinfo, "CD Volume (OSS: cd)", SOUND_MASK_CD);
		hdac_dump_ctls(devinfo, "Microphone Volume (OSS: mic)", SOUND_MASK_MIC);
		hdac_dump_ctls(devinfo, "Line-in Volume (OSS: line)", SOUND_MASK_LINE);
		hdac_dump_ctls(devinfo, "Recording Level (OSS: rec)", SOUND_MASK_RECLEV);
		hdac_dump_ctls(devinfo, "Speaker/Beep (OSS: speaker)", SOUND_MASK_SPEAKER);
		hdac_dump_ctls(devinfo, NULL, 0);
		hdac_dump_dac(devinfo);
		hdac_dump_adc(devinfo);
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "+--------------------------------------+\n");
		device_printf(sc->dev, "| DUMPING PCM Playback/Record Channels |\n");
		device_printf(sc->dev, "+--------------------------------------+\n");
		hdac_dump_pcmchannels(sc, pcnt, rcnt);
	);
}

/****************************************************************************
 * int hdac_detach(device_t)
 *
 * Detach and free up resources utilized by the hdac device.
 ****************************************************************************/
static int
hdac_detach(device_t dev)
{
	struct hdac_softc *sc = NULL;
	device_t *devlist;
	int devcount;
	struct hdac_devinfo *devinfo = NULL;
	struct hdac_codec *codec, *codec_tmp;
	int i;

	devinfo = (struct hdac_devinfo *)pcm_getdevinfo(dev);
	if (devinfo != NULL && devinfo->codec != NULL)
		sc = devinfo->codec->sc;
	if (sc == NULL)
		return (EINVAL);

	if (sc->registered > 0) {
		i = pcm_unregister(dev);
		if (i)
			return (i);
	}

	sc->registered = 0;

	/* Lock the mutex before messing with the dma engines */
	hdac_lock(sc);
	hdac_reset(sc);
	hdac_unlock(sc);
	snd_mtxfree(sc->lock);
	sc->lock = NULL;

	device_get_children(sc->dev, &devlist, &devcount);
	if (devcount != 0 && devlist != NULL) {
		for (i = 0; i < devcount; i++) {
			devinfo = (struct hdac_devinfo *)
			    device_get_ivars(devlist[i]);
			if (devinfo == NULL)
				continue;
			if (devinfo->widget != NULL) {
				free(devinfo->widget, M_HDAC);
			}
			if (devinfo->node_type ==
			    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO &&
			    devinfo->function.audio.ctl != NULL) {
				free(devinfo->function.audio.ctl, M_HDAC);
			}

			free(devinfo, M_HDAC);
			device_delete_child(sc->dev, devlist[i]);
		}
		free(devlist, M_TEMP);
	}

	SLIST_FOREACH_SAFE(codec, &sc->codec_list, next_codec, codec_tmp) {
		SLIST_REMOVE(&sc->codec_list, codec, hdac_codec,
		    next_codec);
		free((void *)codec, M_HDAC);
	}

	hdac_dma_free(&sc->rirb_dma);
	hdac_dma_free(&sc->corb_dma);
	if (sc->play.blkcnt)
		hdac_dma_free(&sc->play.bdl_dma);
	if (sc->rec.blkcnt)
		hdac_dma_free(&sc->rec.bdl_dma);
	hdac_irq_free(sc);
	hdac_mem_free(sc);
	free(sc, M_DEVBUF);

	return (0);
}

static device_method_t hdac_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		hdac_probe),
	DEVMETHOD(device_attach,	hdac_attach),
	DEVMETHOD(device_detach,	hdac_detach),
	{ 0, 0 }
};

static driver_t hdac_driver = {
	"pcm",
	hdac_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_hda, pci, hdac_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_hda, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(snd_hda, 1);
