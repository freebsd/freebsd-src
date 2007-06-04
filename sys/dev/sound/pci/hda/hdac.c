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
 *        interesting documents, especially UAA (Universal Audio Architecture).
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

#include <sys/ctype.h>

#include <dev/sound/pcm/sound.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/sound/pci/hda/hdac_private.h>
#include <dev/sound/pci/hda/hdac_reg.h>
#include <dev/sound/pci/hda/hda_reg.h>
#include <dev/sound/pci/hda/hdac.h>

#include "mixer_if.h"

#define HDA_DRV_TEST_REV	"20070505_0044"
#define HDA_WIDGET_PARSER_REV	1

SND_DECLARE_FILE("$FreeBSD$");

#define HDA_BOOTVERBOSE(stmt)	do {			\
	if (bootverbose != 0 || snd_verbose > 3) {	\
		stmt					\
	}						\
} while(0)

#if 1
#undef HDAC_INTR_EXTRA
#define HDAC_INTR_EXTRA		1
#endif

#define hdac_lock(sc)		snd_mtxlock((sc)->lock)
#define hdac_unlock(sc)		snd_mtxunlock((sc)->lock)
#define hdac_lockassert(sc)	snd_mtxassert((sc)->lock)
#define hdac_lockowned(sc)	mtx_owned((sc)->lock)

#define HDA_FLAG_MATCH(fl, v)	(((fl) & (v)) == (v))
#define HDA_DEV_MATCH(fl, v)	((fl) == (v) || \
				(fl) == 0xffffffff || \
				(((fl) & 0xffff0000) == 0xffff0000 && \
				((fl) & 0x0000ffff) == ((v) & 0x0000ffff)) || \
				(((fl) & 0x0000ffff) == 0x0000ffff && \
				((fl) & 0xffff0000) == ((v) & 0xffff0000)))
#define HDA_MATCH_ALL		0xffffffff
#define HDAC_INVALID		0xffffffff

/* Default controller / jack sense poll: 250ms */
#define HDAC_POLL_INTERVAL	max(hz >> 2, 1)

#define HDA_MODEL_CONSTRUCT(vendor, model)	\
		(((uint32_t)(model) << 16) | ((vendor##_VENDORID) & 0xffff))

/* Controller models */

/* Intel */
#define INTEL_VENDORID		0x8086
#define HDA_INTEL_82801F	HDA_MODEL_CONSTRUCT(INTEL, 0x2668)
#define HDA_INTEL_82801G	HDA_MODEL_CONSTRUCT(INTEL, 0x27d8)
#define HDA_INTEL_82801H	HDA_MODEL_CONSTRUCT(INTEL, 0x284b)
#define HDA_INTEL_63XXESB	HDA_MODEL_CONSTRUCT(INTEL, 0x269a)
#define HDA_INTEL_ALL		HDA_MODEL_CONSTRUCT(INTEL, 0xffff)

/* Nvidia */
#define NVIDIA_VENDORID		0x10de
#define HDA_NVIDIA_MCP51	HDA_MODEL_CONSTRUCT(NVIDIA, 0x026c)
#define HDA_NVIDIA_MCP55	HDA_MODEL_CONSTRUCT(NVIDIA, 0x0371)
#define HDA_NVIDIA_MCP61A	HDA_MODEL_CONSTRUCT(NVIDIA, 0x03e4)
#define HDA_NVIDIA_MCP61B	HDA_MODEL_CONSTRUCT(NVIDIA, 0x03f0)
#define HDA_NVIDIA_MCP65A	HDA_MODEL_CONSTRUCT(NVIDIA, 0x044a)
#define HDA_NVIDIA_MCP65B	HDA_MODEL_CONSTRUCT(NVIDIA, 0x044b)
#define HDA_NVIDIA_ALL		HDA_MODEL_CONSTRUCT(NVIDIA, 0xffff)

/* ATI */
#define ATI_VENDORID		0x1002
#define HDA_ATI_SB450		HDA_MODEL_CONSTRUCT(ATI, 0x437b)
#define HDA_ATI_SB600		HDA_MODEL_CONSTRUCT(ATI, 0x4383)
#define HDA_ATI_ALL		HDA_MODEL_CONSTRUCT(ATI, 0xffff)

/* VIA */
#define VIA_VENDORID		0x1106
#define HDA_VIA_VT82XX		HDA_MODEL_CONSTRUCT(VIA, 0x3288)
#define HDA_VIA_ALL		HDA_MODEL_CONSTRUCT(VIA, 0xffff)

/* SiS */
#define SIS_VENDORID		0x1039
#define HDA_SIS_966		HDA_MODEL_CONSTRUCT(SIS, 0x7502)
#define HDA_SIS_ALL		HDA_MODEL_CONSTRUCT(SIS, 0xffff)

/* OEM/subvendors */

/* Intel */
#define INTEL_D101GGC_SUBVENDOR	HDA_MODEL_CONSTRUCT(INTEL, 0xd600)

/* HP/Compaq */
#define HP_VENDORID		0x103c
#define HP_V3000_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30b5)
#define HP_NX7400_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30a2)
#define HP_NX6310_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30aa)
#define HP_NX6325_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30b0)
#define HP_XW4300_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x3013)
#define HP_3010_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x3010)
#define HP_DV5000_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x30a5)
#define HP_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0xffff)
/* What is wrong with XN 2563 anyway? (Got the picture ?) */
#define HP_NX6325_SUBVENDORX	0x103c30b0

/* Dell */
#define DELL_VENDORID		0x1028
#define DELL_D820_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01cc)
#define DELL_I1300_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01c9)
#define DELL_XPSM1210_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01d7)
#define DELL_OPLX745_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01da)
#define DELL_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0xffff)

/* Clevo */
#define CLEVO_VENDORID		0x1558
#define CLEVO_D900T_SUBVENDOR	HDA_MODEL_CONSTRUCT(CLEVO, 0x0900)
#define CLEVO_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(CLEVO, 0xffff)

/* Acer */
#define ACER_VENDORID		0x1025
#define ACER_A5050_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0x010f)
#define ACER_3681WXM_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0x0110)
#define ACER_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0xffff)

/* Asus */
#define ASUS_VENDORID		0x1043
#define ASUS_M5200_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1993)
#define ASUS_U5F_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1263)
#define ASUS_A8JC_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1153)
#define ASUS_P1AH2_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x81cb)
#define ASUS_A7M_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1323)
#define ASUS_A7T_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x13c2)
#define ASUS_W6F_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1263)
#define ASUS_W2J_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1971)
#define ASUS_F3JC_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1338)
#define ASUS_M2N_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x8234)
#define ASUS_M2NPVMX_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x81cb)
#define ASUS_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0xffff)

/* IBM / Lenovo */
#define IBM_VENDORID		0x1014
#define IBM_M52_SUBVENDOR	HDA_MODEL_CONSTRUCT(IBM, 0x02f6)
#define IBM_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(IBM, 0xffff)

/* Lenovo */
#define LENOVO_VENDORID		0x17aa
#define LENOVO_3KN100_SUBVENDOR	HDA_MODEL_CONSTRUCT(LENOVO, 0x2066)
#define LENOVO_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(LENOVO, 0xffff)

/* Samsung */
#define SAMSUNG_VENDORID	0x144d
#define SAMSUNG_Q1_SUBVENDOR	HDA_MODEL_CONSTRUCT(SAMSUNG, 0xc027)
#define SAMSUNG_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(SAMSUNG, 0xffff)

/* Medion ? */
#define MEDION_VENDORID			0x161f
#define MEDION_MD95257_SUBVENDOR	HDA_MODEL_CONSTRUCT(MEDION, 0x203d)
#define MEDION_ALL_SUBVENDOR		HDA_MODEL_CONSTRUCT(MEDION, 0xffff)

/*
 * Apple Intel MacXXXX seems using Sigmatel codec/vendor id
 * instead of their own, which is beyond my comprehension
 * (see HDA_CODEC_STAC9221 below).
 */
#define APPLE_INTEL_MAC		0x76808384

/* LG Electronics */
#define LG_VENDORID		0x1854
#define LG_LW20_SUBVENDOR	HDA_MODEL_CONSTRUCT(LG, 0x0018)
#define LG_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(LG, 0xffff)

/* Fujitsu Siemens */
#define FS_VENDORID		0x1734
#define FS_PA1510_SUBVENDOR	HDA_MODEL_CONSTRUCT(FS, 0x10b8)
#define FS_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(FS, 0xffff)

/* Toshiba */
#define TOSHIBA_VENDORID	0x1179
#define TOSHIBA_U200_SUBVENDOR	HDA_MODEL_CONSTRUCT(TOSHIBA, 0x0001)
#define TOSHIBA_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(TOSHIBA, 0xffff)

/* Micro-Star International (MSI) */
#define MSI_VENDORID		0x1462
#define MSI_MS1034_SUBVENDOR	HDA_MODEL_CONSTRUCT(MSI, 0x0349)
#define MSI_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(MSI, 0xffff)

/* Uniwill ? */
#define UNIWILL_VENDORID	0x1584
#define UNIWILL_9075_SUBVENDOR	HDA_MODEL_CONSTRUCT(UNIWILL, 0x9075)


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

#define HDA_DAC_LOCKED	(1 << 3)
#define HDA_ADC_LOCKED	(1 << 4)

#define HDA_CTL_OUT	(1 << 0)
#define HDA_CTL_IN	(1 << 1)
#define HDA_CTL_BOTH	(HDA_CTL_IN | HDA_CTL_OUT)

#define HDA_GPIO_MAX		8
/* 0 - 7 = GPIO , 8 = Flush */
#define HDA_QUIRK_GPIO0		(1 << 0)
#define HDA_QUIRK_GPIO1		(1 << 1)
#define HDA_QUIRK_GPIO2		(1 << 2)
#define HDA_QUIRK_GPIO3		(1 << 3)
#define HDA_QUIRK_GPIO4		(1 << 4)
#define HDA_QUIRK_GPIO5		(1 << 5)
#define HDA_QUIRK_GPIO6		(1 << 6)
#define HDA_QUIRK_GPIO7		(1 << 7)
#define HDA_QUIRK_GPIOFLUSH	(1 << 8)

/* 9 - 25 = anything else */
#define HDA_QUIRK_SOFTPCMVOL	(1 << 9)
#define HDA_QUIRK_FIXEDRATE	(1 << 10)
#define HDA_QUIRK_FORCESTEREO	(1 << 11)
#define HDA_QUIRK_EAPDINV	(1 << 12)
#define HDA_QUIRK_DMAPOS	(1 << 13)

/* 26 - 31 = vrefs */
#define HDA_QUIRK_IVREF50	(1 << 26)
#define HDA_QUIRK_IVREF80	(1 << 27)
#define HDA_QUIRK_IVREF100	(1 << 28)
#define HDA_QUIRK_OVREF50	(1 << 29)
#define HDA_QUIRK_OVREF80	(1 << 30)
#define HDA_QUIRK_OVREF100	(1 << 31)

#define HDA_QUIRK_IVREF		(HDA_QUIRK_IVREF50 | HDA_QUIRK_IVREF80 | \
							HDA_QUIRK_IVREF100)
#define HDA_QUIRK_OVREF		(HDA_QUIRK_OVREF50 | HDA_QUIRK_OVREF80 | \
							HDA_QUIRK_OVREF100)
#define HDA_QUIRK_VREF		(HDA_QUIRK_IVREF | HDA_QUIRK_OVREF)

#define SOUND_MASK_SKIP		(1 << 30)
#define SOUND_MASK_DISABLE	(1 << 31)

static const struct {
	char *key;
	uint32_t value;
} hdac_quirks_tab[] = {
	{ "gpio0", HDA_QUIRK_GPIO0 },
	{ "gpio1", HDA_QUIRK_GPIO1 },
	{ "gpio2", HDA_QUIRK_GPIO2 },
	{ "gpio3", HDA_QUIRK_GPIO3 },
	{ "gpio4", HDA_QUIRK_GPIO4 },
	{ "gpio5", HDA_QUIRK_GPIO5 },
	{ "gpio6", HDA_QUIRK_GPIO6 },
	{ "gpio7", HDA_QUIRK_GPIO7 },
	{ "gpioflush", HDA_QUIRK_GPIOFLUSH },
	{ "softpcmvol", HDA_QUIRK_SOFTPCMVOL },
	{ "fixedrate", HDA_QUIRK_FIXEDRATE },
	{ "forcestereo", HDA_QUIRK_FORCESTEREO },
	{ "eapdinv", HDA_QUIRK_EAPDINV },
	{ "dmapos", HDA_QUIRK_DMAPOS },
	{ "ivref50", HDA_QUIRK_IVREF50 },
	{ "ivref80", HDA_QUIRK_IVREF80 },
	{ "ivref100", HDA_QUIRK_IVREF100 },
	{ "ovref50", HDA_QUIRK_OVREF50 },
	{ "ovref80", HDA_QUIRK_OVREF80 },
	{ "ovref100", HDA_QUIRK_OVREF100 },
	{ "ivref", HDA_QUIRK_IVREF },
	{ "ovref", HDA_QUIRK_OVREF },
	{ "vref", HDA_QUIRK_VREF },
};
#define HDAC_QUIRKS_TAB_LEN	\
		(sizeof(hdac_quirks_tab) / sizeof(hdac_quirks_tab[0]))

#define HDA_BDL_MIN	2
#define HDA_BDL_MAX	256
#define HDA_BDL_DEFAULT	HDA_BDL_MIN

#define HDA_BLK_MIN	HDAC_DMA_ALIGNMENT
#define HDA_BLK_ALIGN	(~(HDA_BLK_MIN - 1))

#define HDA_BUFSZ_MIN		4096
#define HDA_BUFSZ_MAX		65536
#define HDA_BUFSZ_DEFAULT	16384

#define HDA_PARSE_MAXDEPTH	10

#define HDAC_UNSOLTAG_EVENT_HP		0x00
#define HDAC_UNSOLTAG_EVENT_TEST	0x01

MALLOC_DEFINE(M_HDAC, "hdac", "High Definition Audio Controller");

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
	{ HDA_INTEL_82801H,  "Intel 82801H" },
	{ HDA_INTEL_63XXESB, "Intel 631x/632xESB" },
	{ HDA_NVIDIA_MCP51,  "NVidia MCP51" },
	{ HDA_NVIDIA_MCP55,  "NVidia MCP55" },
	{ HDA_NVIDIA_MCP61A, "NVidia MCP61A" },
	{ HDA_NVIDIA_MCP61B, "NVidia MCP61B" },
	{ HDA_NVIDIA_MCP65A, "NVidia MCP65A" },
	{ HDA_NVIDIA_MCP65B, "NVidia MCP65B" },
	{ HDA_ATI_SB450,     "ATI SB450"    },
	{ HDA_ATI_SB600,     "ATI SB600"    },
	{ HDA_VIA_VT82XX,    "VIA VT8251/8237A" },
	{ HDA_SIS_966,       "SiS 966" },
	/* Unknown */
	{ HDA_INTEL_ALL,  "Intel (Unknown)"  },
	{ HDA_NVIDIA_ALL, "NVidia (Unknown)" },
	{ HDA_ATI_ALL,    "ATI (Unknown)"    },
	{ HDA_VIA_ALL,    "VIA (Unknown)"    },
	{ HDA_SIS_ALL,    "SiS (Unknown)"    },
};
#define HDAC_DEVICES_LEN (sizeof(hdac_devices) / sizeof(hdac_devices[0]))

static const struct {
	uint16_t vendor;
	uint8_t reg;
	uint8_t mask;
	uint8_t enable;
} hdac_pcie_snoop[] = {
	{  INTEL_VENDORID, 0x00, 0x00, 0x00 },
	{    ATI_VENDORID, 0x42, 0xf8, 0x02 },
	{ NVIDIA_VENDORID, 0x4e, 0xf0, 0x0f },
};
#define HDAC_PCIESNOOP_LEN	\
			(sizeof(hdac_pcie_snoop) / sizeof(hdac_pcie_snoop[0]))

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
#define HDA_CODEC_ALC262	HDA_CODEC_CONSTRUCT(REALTEK, 0x0262)
#define HDA_CODEC_ALC861	HDA_CODEC_CONSTRUCT(REALTEK, 0x0861)
#define HDA_CODEC_ALC861VD	HDA_CODEC_CONSTRUCT(REALTEK, 0x0862)
#define HDA_CODEC_ALC880	HDA_CODEC_CONSTRUCT(REALTEK, 0x0880)
#define HDA_CODEC_ALC882	HDA_CODEC_CONSTRUCT(REALTEK, 0x0882)
#define HDA_CODEC_ALC883	HDA_CODEC_CONSTRUCT(REALTEK, 0x0883)
#define HDA_CODEC_ALC885	HDA_CODEC_CONSTRUCT(REALTEK, 0x0885)
#define HDA_CODEC_ALC888	HDA_CODEC_CONSTRUCT(REALTEK, 0x0888)
#define HDA_CODEC_ALCXXXX	HDA_CODEC_CONSTRUCT(REALTEK, 0xffff)

/* Analog Devices */
#define ANALOGDEVICES_VENDORID	0x11d4
#define HDA_CODEC_AD1981HD	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1981)
#define HDA_CODEC_AD1983	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1983)
#define HDA_CODEC_AD1986A	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1986)
#define HDA_CODEC_AD1988	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1988)
#define HDA_CODEC_ADXXXX	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0xffff)

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
#define HDA_CODEC_STAC9227	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7618)
#define HDA_CODEC_STAC9271D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7627)
#define HDA_CODEC_STACXXXX	HDA_CODEC_CONSTRUCT(SIGMATEL, 0xffff)

/*
 * Conexant
 *
 * Ok, the truth is, I don't have any idea at all whether
 * it is "Venice" or "Waikiki" or other unnamed CXyadayada. The only
 * place that tell me it is "Venice" is from its Windows driver INF.
 *
 *  Venice - CX?????
 * Waikiki - CX20551-22
 */
#define CONEXANT_VENDORID	0x14f1
#define HDA_CODEC_CXVENICE	HDA_CODEC_CONSTRUCT(CONEXANT, 0x5045)
#define HDA_CODEC_CXWAIKIKI	HDA_CODEC_CONSTRUCT(CONEXANT, 0x5047)
#define HDA_CODEC_CXXXXX	HDA_CODEC_CONSTRUCT(CONEXANT, 0xffff)

/* VIA */
#define HDA_CODEC_VT1708_8	HDA_CODEC_CONSTRUCT(VIA, 0x1708)
#define HDA_CODEC_VT1708_9	HDA_CODEC_CONSTRUCT(VIA, 0x1709)
#define HDA_CODEC_VT1708_A	HDA_CODEC_CONSTRUCT(VIA, 0x170a)
#define HDA_CODEC_VT1708_B	HDA_CODEC_CONSTRUCT(VIA, 0x170b)
#define HDA_CODEC_VT1709_0	HDA_CODEC_CONSTRUCT(VIA, 0xe710)
#define HDA_CODEC_VT1709_1	HDA_CODEC_CONSTRUCT(VIA, 0xe711)
#define HDA_CODEC_VT1709_2	HDA_CODEC_CONSTRUCT(VIA, 0xe712)
#define HDA_CODEC_VT1709_3	HDA_CODEC_CONSTRUCT(VIA, 0xe713)
#define HDA_CODEC_VT1709_4	HDA_CODEC_CONSTRUCT(VIA, 0xe714)
#define HDA_CODEC_VT1709_5	HDA_CODEC_CONSTRUCT(VIA, 0xe715)
#define HDA_CODEC_VT1709_6	HDA_CODEC_CONSTRUCT(VIA, 0xe716)
#define HDA_CODEC_VT1709_7	HDA_CODEC_CONSTRUCT(VIA, 0xe717)
#define HDA_CODEC_VTXXXX	HDA_CODEC_CONSTRUCT(VIA, 0xffff)


/* Codecs */
static const struct {
	uint32_t id;
	char *name;
} hdac_codecs[] = {
	{ HDA_CODEC_ALC260,    "Realtek ALC260" },
	{ HDA_CODEC_ALC262,    "Realtek ALC262" },
	{ HDA_CODEC_ALC861,    "Realtek ALC861" },
	{ HDA_CODEC_ALC861VD,  "Realtek ALC861-VD" },
	{ HDA_CODEC_ALC880,    "Realtek ALC880" },
	{ HDA_CODEC_ALC882,    "Realtek ALC882" },
	{ HDA_CODEC_ALC883,    "Realtek ALC883" },
	{ HDA_CODEC_ALC885,    "Realtek ALC885" },
	{ HDA_CODEC_ALC888,    "Realtek ALC888" },
	{ HDA_CODEC_AD1981HD,  "Analog Devices AD1981HD" },
	{ HDA_CODEC_AD1983,    "Analog Devices AD1983" },
	{ HDA_CODEC_AD1986A,   "Analog Devices AD1986A" },
	{ HDA_CODEC_AD1988,    "Analog Devices AD1988" },
	{ HDA_CODEC_CMI9880,   "CMedia CMI9880" },
	{ HDA_CODEC_STAC9221,  "Sigmatel STAC9221" },
	{ HDA_CODEC_STAC9221D, "Sigmatel STAC9221D" },
	{ HDA_CODEC_STAC9220,  "Sigmatel STAC9220" },
	{ HDA_CODEC_STAC922XD, "Sigmatel STAC9220D/9223D" },
	{ HDA_CODEC_STAC9227,  "Sigmatel STAC9227" },
	{ HDA_CODEC_STAC9271D, "Sigmatel STAC9271D" },
	{ HDA_CODEC_CXVENICE,  "Conexant Venice" },
	{ HDA_CODEC_CXWAIKIKI, "Conexant Waikiki" },
	{ HDA_CODEC_VT1708_8,  "VIA VT1708_8" },
	{ HDA_CODEC_VT1708_9,  "VIA VT1708_9" },
	{ HDA_CODEC_VT1708_A,  "VIA VT1708_A" },
	{ HDA_CODEC_VT1708_B,  "VIA VT1708_B" },
	{ HDA_CODEC_VT1709_0,  "VIA VT1709_0" },
	{ HDA_CODEC_VT1709_1,  "VIA VT1709_1" },
	{ HDA_CODEC_VT1709_2,  "VIA VT1709_2" },
	{ HDA_CODEC_VT1709_3,  "VIA VT1709_3" },
	{ HDA_CODEC_VT1709_4,  "VIA VT1709_4" },
	{ HDA_CODEC_VT1709_5,  "VIA VT1709_5" },
	{ HDA_CODEC_VT1709_6,  "VIA VT1709_6" },
	{ HDA_CODEC_VT1709_7,  "VIA VT1709_7" },
	/* Unknown codec */
	{ HDA_CODEC_ALCXXXX,   "Realtek (Unknown)" },
	{ HDA_CODEC_ADXXXX,    "Analog Devices (Unknown)" },
	{ HDA_CODEC_CMIXXXX,   "CMedia (Unknown)" },
	{ HDA_CODEC_STACXXXX,  "Sigmatel (Unknown)" },
	{ HDA_CODEC_CXXXXX,    "Conexant (Unknown)" },
	{ HDA_CODEC_VTXXXX,    "VIA (Unknown)" },
};
#define HDAC_CODECS_LEN	(sizeof(hdac_codecs) / sizeof(hdac_codecs[0]))

enum {
	HDAC_HP_SWITCH_CTL,
	HDAC_HP_SWITCH_CTRL,
	HDAC_HP_SWITCH_DEBUG
};

static const struct {
	uint32_t model;
	uint32_t id;
	int type;
	int inverted;
	int polling;
	int execsense;
	nid_t hpnid;
	nid_t spkrnid[8];
	nid_t eapdnid;
} hdac_hp_switch[] = {
	/* Specific OEM models */
	{ HP_V3000_SUBVENDOR, HDA_CODEC_CXVENICE, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 17, { 16, -1 }, 16 },
	/* { HP_XW4300_SUBVENDOR, HDA_CODEC_ALC260, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 21, { 16, 17, -1 }, -1 } */
	/*{ HP_3010_SUBVENDOR,  HDA_CODEC_ALC260, HDAC_HP_SWITCH_DEBUG,
	    0, 1, 0, 16, { 15, 18, 19, 20, 21, -1 }, -1 },*/
	{ HP_NX7400_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 6, { 5, -1 }, 5 },
	{ HP_NX6310_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 6, { 5, -1 }, 5 },
	{ HP_NX6325_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 6, { 5, -1 }, 5 },
	{ TOSHIBA_U200_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 6, { 5, -1 }, -1 },
	{ DELL_D820_SUBVENDOR, HDA_CODEC_STAC9220, HDAC_HP_SWITCH_CTRL,
	    0, 0, -1, 13, { 14, -1 }, -1 },
	{ DELL_I1300_SUBVENDOR, HDA_CODEC_STAC9220, HDAC_HP_SWITCH_CTRL,
	    0, 0, -1, 13, { 14, -1 }, -1 },
	{ DELL_OPLX745_SUBVENDOR, HDA_CODEC_AD1983, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 6, { 5, 7, -1 }, -1 },
	{ APPLE_INTEL_MAC, HDA_CODEC_STAC9221, HDAC_HP_SWITCH_CTRL,
	    0, 0, -1, 10, { 13, -1 }, -1 },
	{ LENOVO_3KN100_SUBVENDOR, HDA_CODEC_AD1986A, HDAC_HP_SWITCH_CTL,
	    1, 0, -1, 26, { 27, -1 }, -1 },
	{ LG_LW20_SUBVENDOR, HDA_CODEC_ALC880, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 27, { 20, -1 }, -1 },
	{ ACER_A5050_SUBVENDOR, HDA_CODEC_ALC883, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 20, { 21, -1 }, -1 },
	{ ACER_3681WXM_SUBVENDOR, HDA_CODEC_ALC883, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 20, { 21, -1 }, -1 },
	{ MSI_MS1034_SUBVENDOR, HDA_CODEC_ALC883, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 20, { 27, -1 }, -1 },
	/*
	 * All models that at least come from the same vendor with
	 * simmilar codec.
	 */
	{ HP_ALL_SUBVENDOR, HDA_CODEC_CXVENICE, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 17, { 16, -1 }, 16 },
	{ HP_ALL_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 6, { 5, -1 }, 5 },
	{ TOSHIBA_ALL_SUBVENDOR, HDA_CODEC_AD1981HD, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 6, { 5, -1 }, -1 },
	{ DELL_ALL_SUBVENDOR, HDA_CODEC_STAC9220, HDAC_HP_SWITCH_CTRL,
	    0, 0, -1, 13, { 14, -1 }, -1 },
	{ LENOVO_ALL_SUBVENDOR, HDA_CODEC_AD1986A, HDAC_HP_SWITCH_CTL,
	    1, 0, -1, 26, { 27, -1 }, -1 },
#if 0
	{ ACER_ALL_SUBVENDOR, HDA_CODEC_ALC883, HDAC_HP_SWITCH_CTL,
	    0, 0, -1, 20, { 21, -1 }, -1 },
#endif
};
#define HDAC_HP_SWITCH_LEN	\
		(sizeof(hdac_hp_switch) / sizeof(hdac_hp_switch[0]))

static const struct {
	uint32_t model;
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
static void	hdac_dma_free(struct hdac_softc *, struct hdac_dma *);
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

static int	hdac_rirb_flush(struct hdac_softc *sc);
static int	hdac_unsolq_flush(struct hdac_softc *sc);

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
		if (HDA_DEV_MATCH(hdac_codecs[i].id, id))
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
	uint32_t val, id, res;
	int i = 0, j, forcemute;
	nid_t cad;

	if (devinfo == NULL || devinfo->codec == NULL ||
	    devinfo->codec->sc == NULL)
		return;

	sc = devinfo->codec->sc;
	cad = devinfo->codec->cad;
	id = hdac_codec_id(devinfo);
	for (i = 0; i < HDAC_HP_SWITCH_LEN; i++) {
		if (HDA_DEV_MATCH(hdac_hp_switch[i].model,
		    sc->pci_subvendor) &&
		    hdac_hp_switch[i].id == id)
			break;
	}

	if (i >= HDAC_HP_SWITCH_LEN)
		return;

	forcemute = 0;
	if (hdac_hp_switch[i].eapdnid != -1) {
		w = hdac_widget_get(devinfo, hdac_hp_switch[i].eapdnid);
		if (w != NULL && w->param.eapdbtl != HDAC_INVALID)
			forcemute = (w->param.eapdbtl &
			    HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD) ? 0 : 1;
	}

	if (hdac_hp_switch[i].execsense != -1)
		hdac_command(sc,
		    HDA_CMD_SET_PIN_SENSE(cad, hdac_hp_switch[i].hpnid,
		    hdac_hp_switch[i].execsense), cad);
	res = hdac_command(sc,
	    HDA_CMD_GET_PIN_SENSE(cad, hdac_hp_switch[i].hpnid), cad);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "HDA_DEBUG: Pin sense: nid=%d res=0x%08x\n",
		    hdac_hp_switch[i].hpnid, res);
	);
	res = HDA_CMD_GET_PIN_SENSE_PRESENCE_DETECT(res);
	res ^= hdac_hp_switch[i].inverted;

	switch (hdac_hp_switch[i].type) {
	case HDAC_HP_SWITCH_CTL:
		ctl = hdac_audio_ctl_amp_get(devinfo,
		    hdac_hp_switch[i].hpnid, 0, 1);
		if (ctl != NULL) {
			val = (res != 0 && forcemute == 0) ?
			    HDA_AMP_MUTE_NONE : HDA_AMP_MUTE_ALL;
			if (val != ctl->muted) {
				ctl->muted = val;
				hdac_audio_ctl_amp_set(ctl,
				    HDA_AMP_MUTE_DEFAULT, ctl->left,
				    ctl->right);
			}
		}
		for (j = 0; hdac_hp_switch[i].spkrnid[j] != -1; j++) {
			ctl = hdac_audio_ctl_amp_get(devinfo,
			    hdac_hp_switch[i].spkrnid[j], 0, 1);
			if (ctl == NULL)
				continue;
			val = (res != 0 || forcemute == 1) ?
			    HDA_AMP_MUTE_ALL : HDA_AMP_MUTE_NONE;
			if (val == ctl->muted)
				continue;
			ctl->muted = val;
			hdac_audio_ctl_amp_set(ctl, HDA_AMP_MUTE_DEFAULT,
			    ctl->left, ctl->right);
		}
		break;
	case HDAC_HP_SWITCH_CTRL:
		if (res != 0) {
			/* HP in */
			w = hdac_widget_get(devinfo, hdac_hp_switch[i].hpnid);
			if (w != NULL && w->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) {
				if (forcemute == 0)
					val = w->wclass.pin.ctrl |
					    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				else
					val = w->wclass.pin.ctrl &
					    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				if (val != w->wclass.pin.ctrl) {
					w->wclass.pin.ctrl = val;
					hdac_command(sc,
					    HDA_CMD_SET_PIN_WIDGET_CTRL(cad,
					    w->nid, w->wclass.pin.ctrl), cad);
				}
			}
			for (j = 0; hdac_hp_switch[i].spkrnid[j] != -1; j++) {
				w = hdac_widget_get(devinfo,
				    hdac_hp_switch[i].spkrnid[j]);
				if (w == NULL || w->type !=
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
					continue;
				val = w->wclass.pin.ctrl &
				    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				if (val == w->wclass.pin.ctrl)
					continue;
				w->wclass.pin.ctrl = val;
				hdac_command(sc, HDA_CMD_SET_PIN_WIDGET_CTRL(
				    cad, w->nid, w->wclass.pin.ctrl), cad);
			}
		} else {
			/* HP out */
			w = hdac_widget_get(devinfo, hdac_hp_switch[i].hpnid);
			if (w != NULL && w->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) {
				val = w->wclass.pin.ctrl &
				    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				if (val != w->wclass.pin.ctrl) {
					w->wclass.pin.ctrl = val;
					hdac_command(sc,
					    HDA_CMD_SET_PIN_WIDGET_CTRL(cad,
					    w->nid, w->wclass.pin.ctrl), cad);
				}
			}
			for (j = 0; hdac_hp_switch[i].spkrnid[j] != -1; j++) {
				w = hdac_widget_get(devinfo,
				    hdac_hp_switch[i].spkrnid[j]);
				if (w == NULL || w->type !=
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
					continue;
				if (forcemute == 0)
					val = w->wclass.pin.ctrl |
					    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				else
					val = w->wclass.pin.ctrl &
					    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				if (val == w->wclass.pin.ctrl)
					continue;
				w->wclass.pin.ctrl = val;
				hdac_command(sc, HDA_CMD_SET_PIN_WIDGET_CTRL(
				    cad, w->nid, w->wclass.pin.ctrl), cad);
			}
		}
		break;
	case HDAC_HP_SWITCH_DEBUG:
		if (hdac_hp_switch[i].execsense != -1)
			hdac_command(sc,
			    HDA_CMD_SET_PIN_SENSE(cad, hdac_hp_switch[i].hpnid,
			    hdac_hp_switch[i].execsense), cad);
		res = hdac_command(sc,
		    HDA_CMD_GET_PIN_SENSE(cad, hdac_hp_switch[i].hpnid), cad);
		device_printf(sc->dev,
		    "[ 0] HDA_DEBUG: Pin sense: nid=%d res=0x%08x\n",
		    hdac_hp_switch[i].hpnid, res);
		for (j = 0; hdac_hp_switch[i].spkrnid[j] != -1; j++) {
			w = hdac_widget_get(devinfo,
			    hdac_hp_switch[i].spkrnid[j]);
			if (w == NULL || w->type !=
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
				continue;
			if (hdac_hp_switch[i].execsense != -1)
				hdac_command(sc,
				    HDA_CMD_SET_PIN_SENSE(cad, w->nid,
				    hdac_hp_switch[i].execsense), cad);
			res = hdac_command(sc,
			    HDA_CMD_GET_PIN_SENSE(cad, w->nid), cad);
			device_printf(sc->dev,
			    "[%2d] HDA_DEBUG: Pin sense: nid=%d res=0x%08x\n",
			    j + 1, w->nid, res);
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
	device_t *devlist = NULL;
	int devcount, i;

	if (codec == NULL || codec->sc == NULL)
		return;

	sc = codec->sc;

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Unsol Tag: 0x%08x\n", tag);
	);

	device_get_children(sc->dev, &devlist, &devcount);
	for (i = 0; devlist != NULL && i < devcount; i++) {
		devinfo = (struct hdac_devinfo *)device_get_ivars(devlist[i]);
		if (devinfo != NULL && devinfo->node_type ==
		    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO &&
		    devinfo->codec != NULL &&
		    devinfo->codec->cad == codec->cad) {
			break;
		} else
			devinfo = NULL;
	}
	if (devlist != NULL)
		free(devlist, M_TEMP);

	if (devinfo == NULL)
		return;

	switch (tag) {
	case HDAC_UNSOLTAG_EVENT_HP:
		hdac_hp_switch_handler(devinfo);
		break;
	case HDAC_UNSOLTAG_EVENT_TEST:
		device_printf(sc->dev, "Unsol Test!\n");
		break;
	default:
		break;
	}
}

static int
hdac_stream_intr(struct hdac_softc *sc, struct hdac_chan *ch)
{
	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	uint32_t res;
#endif

	if (ch->blkcnt == 0)
		return (0);

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	res = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDSTS);
#endif

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	HDA_BOOTVERBOSE(
		if (res & (HDAC_SDSTS_DESE | HDAC_SDSTS_FIFOE))
			device_printf(sc->dev,
			    "PCMDIR_%s intr triggered beyond stream boundary:"
			    "%08x\n",
			    (ch->dir == PCMDIR_PLAY) ? "PLAY" : "REC", res);
	);
#endif

	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDSTS,
	    HDAC_SDSTS_DESE | HDAC_SDSTS_FIFOE | HDAC_SDSTS_BCIS );

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	if (res & HDAC_SDSTS_BCIS) {
#endif
		return (1);
	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	}
#endif

	return (0);
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
	struct hdac_rirb *rirb_base;
	uint32_t trigger = 0;

	sc = (struct hdac_softc *)context;

	hdac_lock(sc);
	if (sc->polling != 0) {
		hdac_unlock(sc);
		return;
	}
	/* Do we have anything to do? */
	intsts = HDAC_READ_4(&sc->mem, HDAC_INTSTS);
	if (!HDA_FLAG_MATCH(intsts, HDAC_INTSTS_GIS)) {
		hdac_unlock(sc);
		return;
	}

	/* Was this a controller interrupt? */
	if (HDA_FLAG_MATCH(intsts, HDAC_INTSTS_CIS)) {
		rirb_base = (struct hdac_rirb *)sc->rirb_dma.dma_vaddr;
		rirbsts = HDAC_READ_1(&sc->mem, HDAC_RIRBSTS);
		/* Get as many responses that we can */
		while (HDA_FLAG_MATCH(rirbsts, HDAC_RIRBSTS_RINTFL)) {
			HDAC_WRITE_1(&sc->mem,
			    HDAC_RIRBSTS, HDAC_RIRBSTS_RINTFL);
			hdac_rirb_flush(sc);
			rirbsts = HDAC_READ_1(&sc->mem, HDAC_RIRBSTS);
		}
		/* XXX to be removed */
		/* Clear interrupt and exit */
#ifdef HDAC_INTR_EXTRA
		HDAC_WRITE_4(&sc->mem, HDAC_INTSTS, HDAC_INTSTS_CIS);
#endif
	}

	hdac_unsolq_flush(sc);

	if (intsts & HDAC_INTSTS_SIS_MASK) {
		if ((intsts & (1 << sc->num_iss)) &&
		    hdac_stream_intr(sc, &sc->play) != 0)
			trigger |= 1;
		if ((intsts & (1 << 0)) &&
		    hdac_stream_intr(sc, &sc->rec) != 0)
			trigger |= 2;
		/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
		HDAC_WRITE_4(&sc->mem, HDAC_INTSTS, intsts &
		    HDAC_INTSTS_SIS_MASK);
#endif
	}

	hdac_unlock(sc);

	if (trigger & 1)
		chn_intr(sc->play.c);
	if (trigger & 2)
		chn_intr(sc->rec.c);
}

/****************************************************************************
 * int hdac_reset(hdac_softc *)
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
	 * Stop Control DMA engines.
	 */
	HDAC_WRITE_1(&sc->mem, HDAC_CORBCTL, 0x0);
	HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL, 0x0);

	/*
	 * Reset DMA position buffer.
	 */
	HDAC_WRITE_4(&sc->mem, HDAC_DPIBLBASE, 0x0);
	HDAC_WRITE_4(&sc->mem, HDAC_DPIBUBASE, 0x0);

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
		if (gctl & HDAC_GCTL_CRST)
			break;
		DELAY(10);
	} while (--count);
	if (!(gctl & HDAC_GCTL_CRST)) {
		device_printf(sc->dev, "Device stuck in reset\n");
		return (ENXIO);
	}

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

	sc->support_64bit = HDA_FLAG_MATCH(gcap, HDAC_GCAP_64OK);

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


/****************************************************************************
 * int hdac_dma_alloc
 *
 * This function allocate and setup a dma region (struct hdac_dma).
 * It must be freed by a corresponding hdac_dma_free.
 ****************************************************************************/
static int
hdac_dma_alloc(struct hdac_softc *sc, struct hdac_dma *dma, bus_size_t size)
{
	bus_size_t roundsz;
	int result;
	int lowaddr;

	roundsz = roundup2(size, HDAC_DMA_ALIGNMENT);
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
	    roundsz, 				/* maxsize */
	    1,					/* nsegments */
	    roundsz, 				/* maxsegsz */
	    0,					/* flags */
	    NULL,				/* lockfunc */
	    NULL,				/* lockfuncarg */
	    &dma->dma_tag);			/* dmat */
	if (result != 0) {
		device_printf(sc->dev, "%s: bus_dma_tag_create failed (%x)\n",
		    __func__, result);
		goto hdac_dma_alloc_fail;
	}

	/*
	 * Allocate DMA memory
	 */
	result = bus_dmamem_alloc(dma->dma_tag, (void **)&dma->dma_vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO |
	    ((sc->nocache != 0) ? BUS_DMA_NOCACHE : 0), &dma->dma_map);
	if (result != 0) {
		device_printf(sc->dev, "%s: bus_dmamem_alloc failed (%x)\n",
		    __func__, result);
		goto hdac_dma_alloc_fail;
	}

	dma->dma_size = roundsz;

	/*
	 * Map the memory
	 */
	result = bus_dmamap_load(dma->dma_tag, dma->dma_map,
	    (void *)dma->dma_vaddr, roundsz, hdac_dma_cb, (void *)dma, 0);
	if (result != 0 || dma->dma_paddr == 0) {
		if (result == 0)
			result = ENOMEM;
		device_printf(sc->dev, "%s: bus_dmamem_load failed (%x)\n",
		    __func__, result);
		goto hdac_dma_alloc_fail;
	}

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "%s: size=%ju -> roundsz=%ju\n",
		    __func__, (uintmax_t)size, (uintmax_t)roundsz);
	);

	return (0);

hdac_dma_alloc_fail:
	hdac_dma_free(sc, dma);

	return (result);
}


/****************************************************************************
 * void hdac_dma_free(struct hdac_softc *, struct hdac_dma *)
 *
 * Free a struct dhac_dma that has been previously allocated via the
 * hdac_dma_alloc function.
 ****************************************************************************/
static void
hdac_dma_free(struct hdac_softc *sc, struct hdac_dma *dma)
{
	if (dma->dma_map != NULL) {
#if 0
		/* Flush caches */
		bus_dmamap_sync(dma->dma_tag, dma->dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
#endif
		bus_dmamap_unload(dma->dma_tag, dma->dma_map);
	}
	if (dma->dma_vaddr != NULL) {
		bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
		dma->dma_vaddr = NULL;
	}
	dma->dma_map = NULL;
	if (dma->dma_tag != NULL) {
		bus_dma_tag_destroy(dma->dma_tag);
		dma->dma_tag = NULL;
	}
	dma->dma_size = 0;
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
	mem->mem_res = NULL;
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
		goto hdac_irq_alloc_fail;
	}
	result = snd_setup_intr(sc->dev, irq->irq_res, INTR_MPSAFE,
	    hdac_intr_handler, sc, &irq->irq_handle);
	if (result != 0) {
		device_printf(sc->dev,
		    "%s: Unable to setup interrupt handler (%x)\n",
		    __func__, result);
		goto hdac_irq_alloc_fail;
	}

	return (0);

hdac_irq_alloc_fail:
	hdac_irq_free(sc);

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
	if (irq->irq_res != NULL && irq->irq_handle != NULL)
		bus_teardown_intr(sc->dev, irq->irq_res, irq->irq_handle);
	if (irq->irq_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_IRQ, irq->irq_rid,
		    irq->irq_res);
	irq->irq_handle = NULL;
	irq->irq_res = NULL;
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

	if (sc->polling == 0) {
		/* Setup the interrupt threshold */
		HDAC_WRITE_2(&sc->mem, HDAC_RINTCNT, sc->rirb_size / 2);

		/* Enable Overrun and response received reporting */
#if 0
		HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL,
		    HDAC_RIRBCTL_RIRBOIC | HDAC_RIRBCTL_RINTCTL);
#else
		HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL, HDAC_RIRBCTL_RINTCTL);
#endif
	}

#if 0
	/*
	 * Make sure that the Host CPU cache doesn't contain any dirty
	 * cache lines that falls in the rirb. If I understood correctly, it
	 * should be sufficient to do this only once as the rirb is purely
	 * read-only from now on.
	 */
	bus_dmamap_sync(sc->rirb_dma.dma_tag, sc->rirb_dma.dma_map,
	    BUS_DMASYNC_PREREAD);
#endif
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

	statests = HDAC_READ_2(&sc->mem, HDAC_STATESTS);
	for (i = 0; i < HDAC_CODEC_MAX; i++) {
		if (HDAC_STATESTS_SDIWAKE(statests, i)) {
			/* We have found a codec. */
			codec = (struct hdac_codec *)malloc(sizeof(*codec),
			    M_HDAC, M_ZERO | M_NOWAIT);
			if (codec == NULL) {
				device_printf(sc->dev,
				    "Unable to allocate memory for codec\n");
				continue;
			}
			codec->commands = NULL;
			codec->responses_received = 0;
			codec->verbs_sent = 0;
			codec->sc = sc;
			codec->cad = i;
			sc->codecs[i] = codec;
			if (hdac_probe_codec(codec) != 0)
				break;
		}
	}
	/* All codecs have been probed, now try to attach drivers to them */
	/* bus_generic_attach(sc->dev); */
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

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Probing codec: %d\n", cad);
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

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: \tstartnode=%d endnode=%d\n",
		    startnode, endnode);
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
			HDA_BOOTVERBOSE(
				device_printf(sc->dev,
				    "HDA_DEBUG: \tFound AFG nid=%d "
				    "[startnode=%d endnode=%d]\n",
				    devinfo->nid, startnode, endnode);
			);
			return (1);
		}
	}

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: \tAFG not found\n");
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

	fctgrptype = HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE(hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_FCT_GRP_TYPE), cad));

	/* XXX For now, ignore other FG. */
	if (fctgrptype != HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO)
		return (NULL);

	devinfo = (struct hdac_devinfo *)malloc(sizeof(*devinfo), M_HDAC,
	    M_NOWAIT | M_ZERO);
	if (devinfo == NULL) {
		device_printf(sc->dev, "%s: Unable to allocate ivar\n",
		    __func__);
		return (NULL);
	}

	devinfo->nid = nid;
	devinfo->node_type = fctgrptype;
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
	int i, j, max, ents, entnum;
	nid_t cad = w->devinfo->codec->cad;
	nid_t nid = w->nid;
	nid_t cnid, addcnid, prevcnid;

	w->nconns = 0;

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_CONN_LIST_LENGTH), cad);

	ents = HDA_PARAM_CONN_LIST_LENGTH_LIST_LENGTH(res);

	if (ents < 1)
		return;

	entnum = HDA_PARAM_CONN_LIST_LENGTH_LONG_FORM(res) ? 2 : 4;
	max = (sizeof(w->conns) / sizeof(w->conns[0])) - 1;
	prevcnid = 0;

#define CONN_RMASK(e)		(1 << ((32 / (e)) - 1))
#define CONN_NMASK(e)		(CONN_RMASK(e) - 1)
#define CONN_RESVAL(r, e, n)	((r) >> ((32 / (e)) * (n)))
#define CONN_RANGE(r, e, n)	(CONN_RESVAL(r, e, n) & CONN_RMASK(e))
#define CONN_CNID(r, e, n)	(CONN_RESVAL(r, e, n) & CONN_NMASK(e))

	for (i = 0; i < ents; i += entnum) {
		res = hdac_command(sc,
		    HDA_CMD_GET_CONN_LIST_ENTRY(cad, nid, i), cad);
		for (j = 0; j < entnum; j++) {
			cnid = CONN_CNID(res, entnum, j);
			if (cnid == 0) {
				if (w->nconns < ents)
					device_printf(sc->dev,
					    "%s: nid=%d WARNING: zero cnid "
					    "entnum=%d j=%d index=%d "
					    "entries=%d found=%d res=0x%08x\n",
					    __func__, nid, entnum, j, i,
					    ents, w->nconns, res);
				else
					goto getconns_out;
			}
			if (cnid < w->devinfo->startnode ||
			    cnid >= w->devinfo->endnode) {
				HDA_BOOTVERBOSE(
					device_printf(sc->dev,
					    "%s: GHOST: nid=%d j=%d "
					    "entnum=%d index=%d res=0x%08x\n",
					    __func__, nid, j, entnum, i, res);
				);
			}
			if (CONN_RANGE(res, entnum, j) == 0)
				addcnid = cnid;
			else if (prevcnid == 0 || prevcnid >= cnid) {
				device_printf(sc->dev,
				    "%s: WARNING: Invalid child range "
				    "nid=%d index=%d j=%d entnum=%d "
				    "prevcnid=%d cnid=%d res=0x%08x\n",
				    __func__, nid, i, j, entnum, prevcnid,
				    cnid, res);
				addcnid = cnid;
			} else
				addcnid = prevcnid + 1;
			while (addcnid <= cnid) {
				if (w->nconns > max) {
					device_printf(sc->dev,
					    "%s: nid=%d: Adding %d: "
					    "Max connection reached! max=%d\n",
					    __func__, nid, addcnid, max + 1);
					goto getconns_out;
				}
				w->conns[w->nconns++] = addcnid++;
			}
			prevcnid = cnid;
		}
	}

getconns_out:
	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "HDA_DEBUG: %s: nid=%d entries=%d found=%d\n",
		    __func__, nid, ents, w->nconns);
	);
	return;
}

static uint32_t
hdac_widget_pin_getconfig(struct hdac_widget *w)
{
	struct hdac_softc *sc;
	uint32_t config, orig, id;
	nid_t cad, nid;

	sc = w->devinfo->codec->sc;
	cad = w->devinfo->codec->cad;
	nid = w->nid;
	id = hdac_codec_id(w->devinfo);

	config = hdac_command(sc,
	    HDA_CMD_GET_CONFIGURATION_DEFAULT(cad, nid),
	    cad);
	orig = config;

	/*
	 * XXX REWRITE!!!! Don't argue!
	 */
	if (id == HDA_CODEC_ALC880 && sc->pci_subvendor == LG_LW20_SUBVENDOR) {
		switch (nid) {
		case 26:
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN;
			break;
		case 27:
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT;
			break;
		default:
			break;
		}
	} else if (id == HDA_CODEC_ALC880 &&
	    (sc->pci_subvendor == CLEVO_D900T_SUBVENDOR ||
	    sc->pci_subvendor == ASUS_M5200_SUBVENDOR)) {
		/*
		 * Super broken BIOS
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
	} else if (id == HDA_CODEC_ALC883 &&
	    HDA_DEV_MATCH(ACER_ALL_SUBVENDOR, sc->pci_subvendor)) {
		switch (nid) {
		case 25:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED);
			break;
		case 28:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_CD |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED);
			break;
		default:
			break;
		}
	} else if (id == HDA_CODEC_CXVENICE && sc->pci_subvendor ==
	    HP_V3000_SUBVENDOR) {
		switch (nid) {
		case 18:
			config &= ~HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_NONE;
			break;
		case 20:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED);
			break;
		case 21:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_CD |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED);
			break;
		default:
			break;
		}
	} else if (id == HDA_CODEC_CXWAIKIKI && sc->pci_subvendor ==
	    HP_DV5000_SUBVENDOR) {
		switch (nid) {
		case 20:
		case 21:
			config &= ~HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_NONE;
			break;
		default:
			break;
		}
	} else if (id == HDA_CODEC_ALC861 && sc->pci_subvendor ==
	    ASUS_W6F_SUBVENDOR) {
		switch (nid) {
		case 11:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_OUT |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED);
			break;
		case 15:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK);
			break;
		default:
			break;
		}
	} else if (id == HDA_CODEC_ALC861 && sc->pci_subvendor ==
	    UNIWILL_9075_SUBVENDOR) {
		switch (nid) {
		case 15:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK);
			break;
		default:
			break;
		}
	} else if (id == HDA_CODEC_AD1986A && sc->pci_subvendor ==
	    ASUS_M2NPVMX_SUBVENDOR) {
		switch (nid) {
		case 28:	/* LINE */
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN;
			break;
		case 29:	/* MIC */
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN;
			break;
		default:
			break;
		}
	}

	HDA_BOOTVERBOSE(
		if (config != orig)
			device_printf(sc->dev,
			    "HDA_DEBUG: Pin config nid=%u 0x%08x -> 0x%08x\n",
			    nid, orig, config);
	);

	return (config);
}

static uint32_t
hdac_widget_pin_getcaps(struct hdac_widget *w)
{
	struct hdac_softc *sc;
	uint32_t caps, orig, id;
	nid_t cad, nid;

	sc = w->devinfo->codec->sc;
	cad = w->devinfo->codec->cad;
	nid = w->nid;
	id = hdac_codec_id(w->devinfo);

	caps = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_PIN_CAP), cad);
	orig = caps;

	HDA_BOOTVERBOSE(
		if (caps != orig)
			device_printf(sc->dev,
			    "HDA_DEBUG: Pin caps nid=%u 0x%08x -> 0x%08x\n",
			    nid, orig, caps);
	);

	return (caps);
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

	pincap = hdac_widget_pin_getcaps(w);
	w->wclass.pin.cap = pincap;

	w->wclass.pin.ctrl = hdac_command(sc,
	    HDA_CMD_GET_PIN_WIDGET_CTRL(cad, nid), cad) &
	    ~(HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE |
	    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
	    HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE |
	    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE_MASK);

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
		w->param.eapdbtl = HDAC_INVALID;

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

static __inline int
hda_poll_channel(struct hdac_chan *ch)
{
	uint32_t sz, delta;
	volatile uint32_t ptr;

	if (ch->active == 0)
		return (0);

	sz = ch->blksz * ch->blkcnt;
	if (ch->dmapos != NULL)
		ptr = *(ch->dmapos);
	else
		ptr = HDAC_READ_4(&ch->devinfo->codec->sc->mem,
		    ch->off + HDAC_SDLPIB);
	ch->ptr = ptr;
	ptr %= sz;
	ptr &= ~(ch->blksz - 1);
	delta = (sz + ptr - ch->prevptr) % sz;

	if (delta < ch->blksz)
		return (0);

	ch->prevptr = ptr;

	return (1);
}

#define hda_chan_active(sc)	((sc)->play.active + (sc)->rec.active)

static void
hda_poll_callback(void *arg)
{
	struct hdac_softc *sc = arg;
	uint32_t trigger = 0;

	if (sc == NULL)
		return;

	hdac_lock(sc);
	if (sc->polling == 0 || hda_chan_active(sc) == 0) {
		hdac_unlock(sc);
		return;
	}

	trigger |= (hda_poll_channel(&sc->play) != 0) ? 1 : 0;
	trigger |= (hda_poll_channel(&sc->rec) != 0) ? 2 : 0;

	/* XXX */
	callout_reset(&sc->poll_hda, 1/*sc->poll_ticks*/,
	    hda_poll_callback, sc);

	hdac_unlock(sc);

	if (trigger & 1)
		chn_intr(sc->play.c);
	if (trigger & 2)
		chn_intr(sc->rec.c);
}

static int
hdac_rirb_flush(struct hdac_softc *sc)
{
	struct hdac_rirb *rirb_base, *rirb;
	struct hdac_codec *codec;
	struct hdac_command_list *commands;
	nid_t cad;
	uint32_t resp;
	uint8_t rirbwp;
	int ret = 0;

	rirb_base = (struct hdac_rirb *)sc->rirb_dma.dma_vaddr;
	rirbwp = HDAC_READ_1(&sc->mem, HDAC_RIRBWP);
#if 0
	bus_dmamap_sync(sc->rirb_dma.dma_tag, sc->rirb_dma.dma_map,
	    BUS_DMASYNC_POSTREAD);
#endif

	while (sc->rirb_rp != rirbwp) {
		sc->rirb_rp++;
		sc->rirb_rp %= sc->rirb_size;
		rirb = &rirb_base[sc->rirb_rp];
		cad = HDAC_RIRB_RESPONSE_EX_SDATA_IN(rirb->response_ex);
		if (cad < 0 || cad >= HDAC_CODEC_MAX ||
		    sc->codecs[cad] == NULL)
			continue;
		resp = rirb->response;
		codec = sc->codecs[cad];
		commands = codec->commands;
		if (rirb->response_ex & HDAC_RIRB_RESPONSE_EX_UNSOLICITED) {
			sc->unsolq[sc->unsolq_wp++] = (cad << 16) |
			    ((resp >> 26) & 0xffff);
			sc->unsolq_wp %= HDAC_UNSOLQ_MAX;
		} else if (commands != NULL && commands->num_commands > 0 &&
		    codec->responses_received < commands->num_commands)
			commands->responses[codec->responses_received++] =
			    resp;
		ret++;
	}

	return (ret);
}

static int
hdac_unsolq_flush(struct hdac_softc *sc)
{
	nid_t cad;
	uint32_t tag;
	int ret = 0;

	if (sc->unsolq_st == HDAC_UNSOLQ_READY) {
		sc->unsolq_st = HDAC_UNSOLQ_BUSY;
		while (sc->unsolq_rp != sc->unsolq_wp) {
			cad = sc->unsolq[sc->unsolq_rp] >> 16;
			tag = sc->unsolq[sc->unsolq_rp++] & 0xffff;
			sc->unsolq_rp %= HDAC_UNSOLQ_MAX;
			hdac_unsolicited_handler(sc->codecs[cad], tag);
			ret++;
		}
		sc->unsolq_st = HDAC_UNSOLQ_READY;
	}

	return (ret);
}

static void
hdac_poll_callback(void *arg)
{
	struct hdac_softc *sc = arg;
	if (sc == NULL)
		return;

	hdac_lock(sc);
	if (sc->polling == 0 || sc->poll_ival == 0) {
		hdac_unlock(sc);
		return;
	}
	hdac_rirb_flush(sc);
	hdac_unsolq_flush(sc);
	callout_reset(&sc->poll_hdac, sc->poll_ival, hdac_poll_callback, sc);
	hdac_unlock(sc);
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

	ch->active = 0;

	if (sc->polling != 0) {
		int pollticks;

		if (hda_chan_active(sc) == 0) {
			callout_stop(&sc->poll_hda);
			sc->poll_ticks = 1;
		} else {
			if (sc->play.active != 0)
				ch = &sc->play;
			else
				ch = &sc->rec;
			pollticks = ((uint64_t)hz * ch->blksz) /
			    ((uint64_t)sndbuf_getbps(ch->b) *
			    sndbuf_getspd(ch->b));
			pollticks >>= 2;
			if (pollticks > hz)
				pollticks = hz;
			if (pollticks < 1) {
				HDA_BOOTVERBOSE(
					device_printf(sc->dev,
					    "%s: pollticks=%d < 1 !\n",
					    __func__, pollticks);
				);
				pollticks = 1;
			}
			if (pollticks > sc->poll_ticks) {
				HDA_BOOTVERBOSE(
					device_printf(sc->dev,
					    "%s: pollticks %d -> %d\n",
					    __func__, sc->poll_ticks,
					    pollticks);
				);
				sc->poll_ticks = pollticks;
				callout_reset(&sc->poll_hda, 1,
				    hda_poll_callback, sc);
			}
		}
	} else {
		ctl = HDAC_READ_4(&sc->mem, HDAC_INTCTL);
		ctl &= ~(1 << (ch->off >> 5));
		HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, ctl);
	}
}

static void
hdac_stream_start(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	uint32_t ctl;

	if (sc->polling != 0) {
		int pollticks;

		pollticks = ((uint64_t)hz * ch->blksz) /
		    ((uint64_t)sndbuf_getbps(ch->b) * sndbuf_getspd(ch->b));
		pollticks >>= 2;
		if (pollticks > hz)
			pollticks = hz;
		if (pollticks < 1) {
			HDA_BOOTVERBOSE(
				device_printf(sc->dev,
				    "%s: pollticks=%d < 1 !\n",
				    __func__, pollticks);
			);
			pollticks = 1;
		}
		if (hda_chan_active(sc) == 0 || pollticks < sc->poll_ticks) {
			HDA_BOOTVERBOSE(
				if (hda_chan_active(sc) == 0) {
					device_printf(sc->dev,
					    "%s: pollticks=%d\n",
					    __func__, pollticks);
				} else {
					device_printf(sc->dev,
					    "%s: pollticks %d -> %d\n",
					    __func__, sc->poll_ticks,
					    pollticks);
				}
			);
			sc->poll_ticks = pollticks;
			callout_reset(&sc->poll_hda, 1, hda_poll_callback,
			    sc);
		}
		ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL0);
		ctl |= HDAC_SDCTL_RUN;
	} else {
		ctl = HDAC_READ_4(&sc->mem, HDAC_INTCTL);
		ctl |= 1 << (ch->off >> 5);
		HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, ctl);
		ctl = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDCTL0);
		ctl |= HDAC_SDCTL_IOCE | HDAC_SDCTL_FEIE | HDAC_SDCTL_DEIE |
		    HDAC_SDCTL_RUN;
	} 
	HDAC_WRITE_1(&sc->mem, ch->off + HDAC_SDCTL0, ctl);

	ch->active = 1;
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
	if (ctl & HDAC_SDCTL_SRST)
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
	struct hdac_bdle *bdle;
	uint64_t addr;
	uint32_t blksz, blkcnt;
	int i;

	addr = (uint64_t)sndbuf_getbufaddr(ch->b);
	bdle = (struct hdac_bdle *)ch->bdl_dma.dma_vaddr;

	if (sc->polling != 0) {
		blksz = ch->blksz * ch->blkcnt;
		blkcnt = 1;
	} else {
		blksz = ch->blksz;
		blkcnt = ch->blkcnt;
	}

	for (i = 0; i < blkcnt; i++, bdle++) {
		bdle->addrl = (uint32_t)addr;
		bdle->addrh = (uint32_t)(addr >> 32);
		bdle->len = blksz;
		bdle->ioc = 1 ^ sc->polling;
		addr += blksz;
	}

	HDAC_WRITE_4(&sc->mem, ch->off + HDAC_SDCBL, blksz * blkcnt);
	HDAC_WRITE_2(&sc->mem, ch->off + HDAC_SDLVI, blkcnt - 1);
	addr = ch->bdl_dma.dma_paddr;
	HDAC_WRITE_4(&sc->mem, ch->off + HDAC_SDBDPL, (uint32_t)addr);
	HDAC_WRITE_4(&sc->mem, ch->off + HDAC_SDBDPU, (uint32_t)(addr >> 32));
	if (ch->dmapos != NULL &&
	    !(HDAC_READ_4(&sc->mem, HDAC_DPIBLBASE) & 0x00000001)) {
		addr = sc->pos_dma.dma_paddr;
		HDAC_WRITE_4(&sc->mem, HDAC_DPIBLBASE,
		    ((uint32_t)addr & HDAC_DPLBASE_DPLBASE_MASK) | 0x00000001);
		HDAC_WRITE_4(&sc->mem, HDAC_DPIBUBASE, (uint32_t)(addr >> 32));
	}
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
	uint32_t response = HDAC_INVALID;

	if (!hdac_lockowned(sc))
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
	int timeout;
	int retry = 10;
	struct hdac_rirb *rirb_base;

	if (sc == NULL || sc->codecs[cad] == NULL || commands == NULL ||
	    commands->num_commands < 1)
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
#if 0
			bus_dmamap_sync(sc->corb_dma.dma_tag,
			    sc->corb_dma.dma_map, BUS_DMASYNC_PREWRITE);
#endif
			while (codec->verbs_sent != commands->num_commands &&
			    ((sc->corb_wp + 1) % sc->corb_size) != corbrp) {
				sc->corb_wp++;
				sc->corb_wp %= sc->corb_size;
				corb[sc->corb_wp] =
				    commands->verbs[codec->verbs_sent++];
			}

			/* Send the verbs to the codecs */
#if 0
			bus_dmamap_sync(sc->corb_dma.dma_tag,
			    sc->corb_dma.dma_map, BUS_DMASYNC_POSTWRITE);
#endif
			HDAC_WRITE_2(&sc->mem, HDAC_CORBWP, sc->corb_wp);
		}

		timeout = 1000;
		while (hdac_rirb_flush(sc) == 0 && --timeout)
			DELAY(10);
	} while ((codec->verbs_sent != commands->num_commands ||
	    codec->responses_received != commands->num_commands) && --retry);

	if (retry == 0)
		device_printf(sc->dev,
		    "%s: TIMEOUT numcmd=%d, sent=%d, received=%d\n",
		    __func__, commands->num_commands, codec->verbs_sent,
		    codec->responses_received);

	codec->commands = NULL;
	codec->responses_received = 0;
	codec->verbs_sent = 0;

	hdac_unsolq_flush(sc);
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
	uint32_t model;
	uint16_t class, subclass;
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
		if (HDA_DEV_MATCH(hdac_devices[i].model, model) &&
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
		devinfo->function.audio.playcnt++;
	} else {
		ch = &sc->rec;
		ch->off = devinfo->function.audio.reccnt << 5;
		devinfo->function.audio.reccnt++;
	}
	if (devinfo->function.audio.quirks & HDA_QUIRK_FIXEDRATE) {
		ch->caps.minspeed = ch->caps.maxspeed = 48000;
		ch->pcmrates[0] = 48000;
		ch->pcmrates[1] = 0;
	}
	if (sc->pos_dma.dma_vaddr != NULL)
		ch->dmapos = (uint32_t *)(sc->pos_dma.dma_vaddr +
		    (sc->streamcnt * 8));
	else
		ch->dmapos = NULL;
	ch->sid = ++sc->streamcnt;
	ch->dir = dir;
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

	if (sndbuf_alloc(ch->b, sc->chan_dmat,
	    (sc->nocache != 0) ? BUS_DMA_NOCACHE : 0, sc->chan_size) != 0)
		return (NULL);

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
	uint32_t spd = 0, threshold;
	int i;

	for (i = 0; ch->pcmrates[i] != 0; i++) {
		spd = ch->pcmrates[i];
		threshold = spd + ((ch->pcmrates[i + 1] != 0) ?
		    ((ch->pcmrates[i + 1] - spd) >> 1) : 0);
		if (speed < threshold)
			break;
	}

	if (spd == 0)	/* impossible */
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
		HDA_BOOTVERBOSE(
			device_printf(sc->dev,
			    "HDA_DEBUG: PCMDIR_%s: Stream setup nid=%d "
			    "fmt=0x%08x\n",
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
hdac_channel_setfragments(kobj_t obj, void *data,
					uint32_t blksz, uint32_t blkcnt)
{
	struct hdac_chan *ch = data;
	struct hdac_softc *sc = ch->devinfo->codec->sc;

	blksz &= HDA_BLK_ALIGN;

	if (blksz > (sndbuf_getmaxsize(ch->b) / HDA_BDL_MIN))
		blksz = sndbuf_getmaxsize(ch->b) / HDA_BDL_MIN;
	if (blksz < HDA_BLK_MIN)
		blksz = HDA_BLK_MIN;
	if (blkcnt > HDA_BDL_MAX)
		blkcnt = HDA_BDL_MAX;
	if (blkcnt < HDA_BDL_MIN)
		blkcnt = HDA_BDL_MIN;

	while ((blksz * blkcnt) > sndbuf_getmaxsize(ch->b)) {
		if ((blkcnt >> 1) >= HDA_BDL_MIN)
			blkcnt >>= 1;
		else if ((blksz >> 1) >= HDA_BLK_MIN)
			blksz >>= 1;
		else
			break;
	}

	if ((sndbuf_getblksz(ch->b) != blksz ||
	    sndbuf_getblkcnt(ch->b) != blkcnt) &&
	    sndbuf_resize(ch->b, blkcnt, blksz) != 0)
		device_printf(sc->dev, "%s: failed blksz=%u blkcnt=%u\n",
		    __func__, blksz, blkcnt);

	ch->blksz = sndbuf_getblksz(ch->b);
	ch->blkcnt = sndbuf_getblkcnt(ch->b);

	return (1);
}

static int
hdac_channel_setblocksize(kobj_t obj, void *data, uint32_t blksz)
{
	struct hdac_chan *ch = data;
	struct hdac_softc *sc = ch->devinfo->codec->sc;

	hdac_channel_setfragments(obj, data, blksz, sc->chan_blkcnt);

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
	default:
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
	uint32_t ptr;

	hdac_lock(sc);
	if (sc->polling != 0)
		ptr = ch->ptr;
	else if (ch->dmapos != NULL)
		ptr = *(ch->dmapos);
	else
		ptr = HDAC_READ_4(&sc->mem, ch->off + HDAC_SDLPIB);
	hdac_unlock(sc);

	/*
	 * Round to available space and force 128 bytes aligment.
	 */
	ptr %= ch->blksz * ch->blkcnt;
	ptr &= HDA_BLK_ALIGN;

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
	KOBJMETHOD(channel_setfragments,	hdac_channel_setfragments),
	KOBJMETHOD(channel_trigger,		hdac_channel_trigger),
	KOBJMETHOD(channel_getptr,		hdac_channel_getptr),
	KOBJMETHOD(channel_getcaps,		hdac_channel_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(hdac_channel);

static void
hdac_jack_poll_callback(void *arg)
{
	struct hdac_devinfo *devinfo = arg;
	struct hdac_softc *sc;

	if (devinfo == NULL || devinfo->codec == NULL ||
	    devinfo->codec->sc == NULL)
		return;
	sc = devinfo->codec->sc;
	hdac_lock(sc);
	if (sc->poll_ival == 0) {
		hdac_unlock(sc);
		return;
	}
	hdac_hp_switch_handler(devinfo);
	callout_reset(&sc->poll_jack, sc->poll_ival,
	    hdac_jack_poll_callback, devinfo);
	hdac_unlock(sc);
}

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

	hdac_lock(sc);

	mask = 0;
	recmask = 0;

	id = hdac_codec_id(devinfo);
	cad = devinfo->codec->cad;
	for (i = 0; i < HDAC_HP_SWITCH_LEN; i++) {
		if (!(HDA_DEV_MATCH(hdac_hp_switch[i].model,
		    sc->pci_subvendor) && hdac_hp_switch[i].id == id))
			continue;
		w = hdac_widget_get(devinfo, hdac_hp_switch[i].hpnid);
		if (w == NULL || w->enable == 0 || w->type !=
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if (hdac_hp_switch[i].polling != 0)
			callout_reset(&sc->poll_jack, 1,
			    hdac_jack_poll_callback, devinfo);
		else if (HDA_PARAM_AUDIO_WIDGET_CAP_UNSOL_CAP(w->param.widget_cap))
			hdac_command(sc,
			    HDA_CMD_SET_UNSOLICITED_RESPONSE(cad, w->nid,
			    HDA_CMD_SET_UNSOLICITED_RESPONSE_ENABLE |
			    HDAC_UNSOLTAG_EVENT_HP), cad);
		else
			continue;
		hdac_hp_switch_handler(devinfo);
		HDA_BOOTVERBOSE(
			device_printf(sc->dev,
			    "HDA_DEBUG: Enabling headphone/speaker "
			    "audio routing switching:\n");
			device_printf(sc->dev,
			    "HDA_DEBUG: \tindex=%d nid=%d "
			    "pci_subvendor=0x%08x "
			    "codec=0x%08x [%s]\n",
			    i, w->nid, sc->pci_subvendor, id,
			    (hdac_hp_switch[i].polling != 0) ? "POLL" :
			    "UNSOL");
		);
		break;
	}
	for (i = 0; i < HDAC_EAPD_SWITCH_LEN; i++) {
		if (!(HDA_DEV_MATCH(hdac_eapd_switch[i].model,
		    sc->pci_subvendor) &&
		    hdac_eapd_switch[i].id == id))
			continue;
		w = hdac_widget_get(devinfo, hdac_eapd_switch[i].eapdnid);
		if (w == NULL || w->enable == 0)
			break;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX ||
		    w->param.eapdbtl == HDAC_INVALID)
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
	} else
		softpcmvol = (devinfo->function.audio.quirks &
		    HDA_QUIRK_SOFTPCMVOL) ? 1 : 0;

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
		pcm_setflags(sc->dev, pcm_getflags(sc->dev) | SD_F_SOFTPCMVOL);
		HDA_BOOTVERBOSE(
			device_printf(sc->dev,
			    "HDA_DEBUG: %s Soft PCM volume\n",
			    (softpcmvol == 1) ?
			    "Forcing" : "Enabling");
		);
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
		} else if (id == HDA_CODEC_STAC9221) {
			mask |= SOUND_MASK_VOLUME;
			while ((ctl = hdac_audio_ctl_each(devinfo, &i)) !=
			    NULL) {
				if (ctl->widget == NULL)
					continue;
				if (ctl->widget->type ==
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT &&
				    ctl->index == 0 && (ctl->widget->nid == 2 ||
				    ctl->widget->enable != 0)) {
					ctl->enable = 1;
					ctl->ossmask = SOUND_MASK_VOLUME;
					ctl->ossval = 100 | (100 << 8);
				} else if (ctl->enable == 0)
					continue;
				else
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
				if (!HDA_FLAG_MATCH(ctl->ossmask,
				    SOUND_MASK_VOLUME | SOUND_MASK_PCM))
					continue;
				if (!(ctl->mute == 1 && ctl->step == 0))
					ctl->enable = 0;
			}
		}
	}

	recmask &= ~(SOUND_MASK_PCM | SOUND_MASK_RECLEV | SOUND_MASK_SPEAKER |
	    SOUND_MASK_BASS | SOUND_MASK_TREBLE | SOUND_MASK_IGAIN |
	    SOUND_MASK_OGAIN);
	recmask &= (1 << SOUND_MIXER_NRDEVICES) - 1;
	mask &= (1 << SOUND_MIXER_NRDEVICES) - 1;

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
		uint32_t orig;
		/*if (left != right || !(left == 0 || left == 1)) {
			hdac_unlock(sc);
			return (-1);
		}*/
		id = hdac_codec_id(devinfo);
		for (i = 0; i < HDAC_EAPD_SWITCH_LEN; i++) {
			if (HDA_DEV_MATCH(hdac_eapd_switch[i].model,
			    sc->pci_subvendor) &&
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
		    w->param.eapdbtl == HDAC_INVALID) {
			hdac_unlock(sc);
			return (-1);
		}
		orig = w->param.eapdbtl;
		if (left == 0)
			w->param.eapdbtl &= ~HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
		else
			w->param.eapdbtl |= HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
		if (orig != w->param.eapdbtl) {
			uint32_t val;

			if (hdac_eapd_switch[i].hp_switch != 0)
				hdac_hp_switch_handler(devinfo);
			val = w->param.eapdbtl;
			if (devinfo->function.audio.quirks & HDA_QUIRK_EAPDINV)
				val ^= HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
			hdac_command(sc,
			    HDA_CMD_SET_EAPD_BTL_ENABLE(devinfo->codec->cad,
			    w->nid, val), devinfo->codec->cad);
		}
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
		if (w == NULL || w->enable == 0)
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
				if (!(w->pflags & HDA_ADC_LOCKED))
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
	int i;
	uint16_t vendor;
	uint8_t v;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return (ENOMEM);
	}

	sc->lock = snd_mtxcreate(device_get_nameunit(dev), HDAC_MTX_NAME);
	sc->dev = dev;
	sc->pci_subvendor = (uint32_t)pci_get_subdevice(sc->dev) << 16;
	sc->pci_subvendor |= (uint32_t)pci_get_subvendor(sc->dev) & 0x0000ffff;
	vendor = pci_get_vendor(dev);

	if (sc->pci_subvendor == HP_NX6325_SUBVENDORX) {
		/* Screw nx6325 - subdevice/subvendor swapped */
		sc->pci_subvendor = HP_NX6325_SUBVENDOR;
	}

	callout_init(&sc->poll_hda, CALLOUT_MPSAFE);
	callout_init(&sc->poll_hdac, CALLOUT_MPSAFE);
	callout_init(&sc->poll_jack, CALLOUT_MPSAFE);

	sc->poll_ticks = 1;
	sc->poll_ival = HDAC_POLL_INTERVAL;
	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "polling", &i) == 0 && i != 0)
		sc->polling = 1;
	else
		sc->polling = 0;

	sc->chan_size = pcm_getbuffersize(dev,
	    HDA_BUFSZ_MIN, HDA_BUFSZ_DEFAULT, HDA_BUFSZ_MAX);

	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "blocksize", &i) == 0 && i > 0) {
		i &= HDA_BLK_ALIGN;
		if (i < HDA_BLK_MIN)
			i = HDA_BLK_MIN;
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
	    sc->chan_size, 			/* maxsize */
	    1,					/* nsegments */
	    sc->chan_size, 			/* maxsegsz */
	    0,					/* flags */
	    NULL,				/* lockfunc */
	    NULL,				/* lockfuncarg */
	    &sc->chan_dmat);			/* dmat */
	if (result != 0) {
		device_printf(dev, "%s: bus_dma_tag_create failed (%x)\n",
		     __func__, result);
		snd_mtxfree(sc->lock);
		free(sc, M_DEVBUF);
		return (ENXIO);
	}


	sc->hdabus = NULL;
	for (i = 0; i < HDAC_CODEC_MAX; i++)
		sc->codecs[i] = NULL;

	pci_enable_busmaster(dev);

	if (vendor == INTEL_VENDORID) {
		/* TCSEL -> TC0 */
		v = pci_read_config(dev, 0x44, 1);
		pci_write_config(dev, 0x44, v & 0xf8, 1);
		HDA_BOOTVERBOSE(
			device_printf(dev, "TCSEL: 0x%02d -> 0x%02d\n", v,
			    pci_read_config(dev, 0x44, 1));
		);
	}

#if defined(__i386__) || defined(__amd64__)
	sc->nocache = 1;

	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "snoop", &i) == 0 && i != 0) {
#else
	sc->nocache = 0;
#endif
		/*
		 * Try to enable PCIe snoop to avoid messing around with
		 * uncacheable DMA attribute. Since PCIe snoop register
		 * config is pretty much vendor specific, there are no
		 * general solutions on how to enable it, forcing us (even
		 * Microsoft) to enable uncacheable or write combined DMA
		 * by default.
		 *
		 * http://msdn2.microsoft.com/en-us/library/ms790324.aspx
		 */
		for (i = 0; i < HDAC_PCIESNOOP_LEN; i++) {
			if (hdac_pcie_snoop[i].vendor != vendor)
				continue;
			sc->nocache = 0;
			if (hdac_pcie_snoop[i].reg == 0x00)
				break;
			v = pci_read_config(dev, hdac_pcie_snoop[i].reg, 1);
			if ((v & hdac_pcie_snoop[i].enable) ==
			    hdac_pcie_snoop[i].enable)
				break;
			v &= hdac_pcie_snoop[i].mask;
			v |= hdac_pcie_snoop[i].enable;
			pci_write_config(dev, hdac_pcie_snoop[i].reg, v, 1);
			v = pci_read_config(dev, hdac_pcie_snoop[i].reg, 1);
			if ((v & hdac_pcie_snoop[i].enable) !=
			    hdac_pcie_snoop[i].enable) {
				HDA_BOOTVERBOSE(
					device_printf(dev,
					    "WARNING: Failed to enable PCIe "
					    "snoop!\n");
				);
#if defined(__i386__) || defined(__amd64__)
				sc->nocache = 1;
#endif
			}
			break;
		}
#if defined(__i386__) || defined(__amd64__)
	}
#endif

	HDA_BOOTVERBOSE(
		device_printf(dev, "DMA Coherency: %s / vendor=0x%04x\n",
		    (sc->nocache == 0) ? "PCIe snoop" : "Uncacheable", vendor);
	);

	/* Allocate resources */
	result = hdac_mem_alloc(sc);
	if (result != 0)
		goto hdac_attach_fail;
	result = hdac_irq_alloc(sc);
	if (result != 0)
		goto hdac_attach_fail;

	/* Get Capabilities */
	result = hdac_get_capabilities(sc);
	if (result != 0)
		goto hdac_attach_fail;

	/* Allocate CORB and RIRB dma memory */
	result = hdac_dma_alloc(sc, &sc->corb_dma,
	    sc->corb_size * sizeof(uint32_t));
	if (result != 0)
		goto hdac_attach_fail;
	result = hdac_dma_alloc(sc, &sc->rirb_dma,
	    sc->rirb_size * sizeof(struct hdac_rirb));
	if (result != 0)
		goto hdac_attach_fail;

	/* Quiesce everything */
	hdac_reset(sc);

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

	return (0);

hdac_attach_fail:
	hdac_irq_free(sc);
	hdac_dma_free(sc, &sc->rirb_dma);
	hdac_dma_free(sc, &sc->corb_dma);
	hdac_mem_free(sc);
	snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);

	return (ENXIO);
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

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad , nid, HDA_PARAM_GPIO_COUNT), cad);
	devinfo->function.audio.gpio = res;

	HDA_BOOTVERBOSE(
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
		device_printf(sc->dev, "    CORB size: %d\n", sc->corb_size);
		device_printf(sc->dev, "    RIRB size: %d\n", sc->rirb_size);
		device_printf(sc->dev, "      Streams: ISS=%d OSS=%d BSS=%d\n",
		    sc->num_iss, sc->num_oss, sc->num_bss);
		device_printf(sc->dev, "         GPIO: 0x%08x\n",
		    devinfo->function.audio.gpio);
		device_printf(sc->dev, "               NumGPIO=%d NumGPO=%d "
		    "NumGPI=%d GPIWake=%d GPIUnsol=%d\n",
		    HDA_PARAM_GPIO_COUNT_NUM_GPIO(devinfo->function.audio.gpio),
		    HDA_PARAM_GPIO_COUNT_NUM_GPO(devinfo->function.audio.gpio),
		    HDA_PARAM_GPIO_COUNT_NUM_GPI(devinfo->function.audio.gpio),
		    HDA_PARAM_GPIO_COUNT_GPI_WAKE(devinfo->function.audio.gpio),
		    HDA_PARAM_GPIO_COUNT_GPI_UNSOL(devinfo->function.audio.gpio));
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

	if (devinfo->nodecnt > 0)
		devinfo->widget = (struct hdac_widget *)malloc(
		    sizeof(*(devinfo->widget)) * devinfo->nodecnt, M_HDAC,
		    M_NOWAIT | M_ZERO);
	else
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
			w->param.eapdbtl = HDAC_INVALID;
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
	int mute, offset, step, size;

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

	ctls = (struct hdac_audio_ctl *)malloc(
	    sizeof(*ctls) * max, M_HDAC, M_ZERO | M_NOWAIT);

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
			mute = HDA_PARAM_OUTPUT_AMP_CAP_MUTE_CAP(ocap);
			step = HDA_PARAM_OUTPUT_AMP_CAP_NUMSTEPS(ocap);
			size = HDA_PARAM_OUTPUT_AMP_CAP_STEPSIZE(ocap);
			offset = HDA_PARAM_OUTPUT_AMP_CAP_OFFSET(ocap);
			/*if (offset > step) {
				HDA_BOOTVERBOSE(
					device_printf(sc->dev,
					    "HDA_DEBUG: BUGGY outamp: nid=%d "
					    "[offset=%d > step=%d]\n",
					    w->nid, offset, step);
				);
				offset = step;
			}*/
			ctls[cnt].enable = 1;
			ctls[cnt].widget = w;
			ctls[cnt].mute = mute;
			ctls[cnt].step = step;
			ctls[cnt].size = size;
			ctls[cnt].offset = offset;
			ctls[cnt].left = offset;
			ctls[cnt].right = offset;
			ctls[cnt++].dir = HDA_CTL_OUT;
		}

		if (icap != 0) {
			mute = HDA_PARAM_OUTPUT_AMP_CAP_MUTE_CAP(icap);
			step = HDA_PARAM_OUTPUT_AMP_CAP_NUMSTEPS(icap);
			size = HDA_PARAM_OUTPUT_AMP_CAP_STEPSIZE(icap);
			offset = HDA_PARAM_OUTPUT_AMP_CAP_OFFSET(icap);
			/*if (offset > step) {
				HDA_BOOTVERBOSE(
					device_printf(sc->dev,
					    "HDA_DEBUG: BUGGY inamp: nid=%d "
					    "[offset=%d > step=%d]\n",
					    w->nid, offset, step);
				);
				offset = step;
			}*/
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
					ctls[cnt].mute = mute;
					ctls[cnt].step = step;
					ctls[cnt].size = size;
					ctls[cnt].offset = offset;
					ctls[cnt].left = offset;
					ctls[cnt].right = offset;
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
				ctls[cnt].mute = mute;
				ctls[cnt].step = step;
				ctls[cnt].size = size;
				ctls[cnt].offset = offset;
				ctls[cnt].left = offset;
				ctls[cnt].right = offset;
				ctls[cnt++].dir = HDA_CTL_IN;
				break;
			}
		}
	}

	devinfo->function.audio.ctl = ctls;
}

static const struct {
	uint32_t model;
	uint32_t id;
	uint32_t set, unset;
} hdac_quirks[] = {
	/*
	 * XXX Force stereo quirk. Monoural recording / playback
	 *     on few codecs (especially ALC880) seems broken or
	 *     perhaps unsupported.
	 */
	{ HDA_MATCH_ALL, HDA_MATCH_ALL,
	    HDA_QUIRK_FORCESTEREO | HDA_QUIRK_IVREF, 0 },
	{ ACER_ALL_SUBVENDOR, HDA_MATCH_ALL,
	    HDA_QUIRK_GPIO0, 0 },
	{ ASUS_M5200_SUBVENDOR, HDA_CODEC_ALC880,
	    HDA_QUIRK_GPIO0, 0 },
	{ ASUS_A7M_SUBVENDOR, HDA_CODEC_ALC880,
	    HDA_QUIRK_GPIO0, 0 },
	{ ASUS_A7T_SUBVENDOR, HDA_CODEC_ALC882,
	    HDA_QUIRK_GPIO0, 0 },
	{ ASUS_W2J_SUBVENDOR, HDA_CODEC_ALC882,
	    HDA_QUIRK_GPIO0, 0 },
	{ ASUS_U5F_SUBVENDOR, HDA_CODEC_AD1986A,
	    HDA_QUIRK_EAPDINV, 0 },
	{ ASUS_A8JC_SUBVENDOR, HDA_CODEC_AD1986A,
	    HDA_QUIRK_EAPDINV, 0 },
	{ ASUS_F3JC_SUBVENDOR, HDA_CODEC_ALC861,
	    HDA_QUIRK_OVREF, 0 },
	{ ASUS_W6F_SUBVENDOR, HDA_CODEC_ALC861,
	    HDA_QUIRK_OVREF, 0 },
	{ UNIWILL_9075_SUBVENDOR, HDA_CODEC_ALC861,
	    HDA_QUIRK_OVREF, 0 },
	/*{ ASUS_M2N_SUBVENDOR, HDA_CODEC_AD1988,
	    HDA_QUIRK_IVREF80, HDA_QUIRK_IVREF50 | HDA_QUIRK_IVREF100 },*/
	{ MEDION_MD95257_SUBVENDOR, HDA_CODEC_ALC880,
	    HDA_QUIRK_GPIO1, 0 },
	{ LENOVO_3KN100_SUBVENDOR, HDA_CODEC_AD1986A,
	    HDA_QUIRK_EAPDINV, 0 },
	{ SAMSUNG_Q1_SUBVENDOR, HDA_CODEC_AD1986A,
	    HDA_QUIRK_EAPDINV, 0 },
	{ APPLE_INTEL_MAC, HDA_CODEC_STAC9221,
	    HDA_QUIRK_GPIO0 | HDA_QUIRK_GPIO1, 0 },
	{ HDA_MATCH_ALL, HDA_CODEC_AD1988,
	    HDA_QUIRK_IVREF80, HDA_QUIRK_IVREF50 | HDA_QUIRK_IVREF100 },
	{ HDA_MATCH_ALL, HDA_CODEC_CXVENICE,
	    0, HDA_QUIRK_FORCESTEREO },
	{ HDA_MATCH_ALL, HDA_CODEC_STACXXXX,
	    HDA_QUIRK_SOFTPCMVOL, 0 }
};
#define HDAC_QUIRKS_LEN (sizeof(hdac_quirks) / sizeof(hdac_quirks[0]))

static void
hdac_vendor_patch_parse(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	uint32_t id, subvendor;
	int i;

	id = hdac_codec_id(devinfo);
	subvendor = devinfo->codec->sc->pci_subvendor;

	/*
	 * Quirks
	 */
	for (i = 0; i < HDAC_QUIRKS_LEN; i++) {
		if (!(HDA_DEV_MATCH(hdac_quirks[i].model, subvendor) &&
		    HDA_DEV_MATCH(hdac_quirks[i].id, id)))
			continue;
		if (hdac_quirks[i].set != 0)
			devinfo->function.audio.quirks |=
			    hdac_quirks[i].set;
		if (hdac_quirks[i].unset != 0)
			devinfo->function.audio.quirks &=
			    ~(hdac_quirks[i].unset);
	}

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
		if (subvendor == HP_XW4300_SUBVENDOR) {
			ctl = hdac_audio_ctl_amp_get(devinfo, 16, 0, 1);
			if (ctl != NULL && ctl->widget != NULL) {
				ctl->ossmask = SOUND_MASK_SPEAKER;
				ctl->widget->ctlflags |= SOUND_MASK_SPEAKER;
			}
			ctl = hdac_audio_ctl_amp_get(devinfo, 17, 0, 1);
			if (ctl != NULL && ctl->widget != NULL) {
				ctl->ossmask = SOUND_MASK_SPEAKER;
				ctl->widget->ctlflags |= SOUND_MASK_SPEAKER;
			}
		} else if (subvendor == HP_3010_SUBVENDOR) {
			ctl = hdac_audio_ctl_amp_get(devinfo, 17, 0, 1);
			if (ctl != NULL && ctl->widget != NULL) {
				ctl->ossmask = SOUND_MASK_SPEAKER;
				ctl->widget->ctlflags |= SOUND_MASK_SPEAKER;
			}
			ctl = hdac_audio_ctl_amp_get(devinfo, 21, 0, 1);
			if (ctl != NULL && ctl->widget != NULL) {
				ctl->ossmask = SOUND_MASK_SPEAKER;
				ctl->widget->ctlflags |= SOUND_MASK_SPEAKER;
			}
		}
		break;
	case HDA_CODEC_ALC861:
		ctl = hdac_audio_ctl_amp_get(devinfo, 21, 2, 1);
		if (ctl != NULL)
			ctl->muted = HDA_AMP_MUTE_ALL;
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
				w->type =
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET;
				w->param.widget_cap &=
				    ~HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_MASK;
				w->param.widget_cap |=
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET <<
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_SHIFT;
				strlcpy(w->name, "beep widget", sizeof(w->name));
			}
		}
		break;
	case HDA_CODEC_ALC883:
		/*
		 * nid: 24/25 = External (jack) or Internal (fixed) Mic.
		 *              Clear vref cap for jack connectivity.
		 */
		w = hdac_widget_get(devinfo, 24);
		if (w != NULL && w->enable != 0 && w->type ==
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
		    (w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK) ==
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK)
			w->wclass.pin.cap &= ~(
			    HDA_PARAM_PIN_CAP_VREF_CTRL_100_MASK |
			    HDA_PARAM_PIN_CAP_VREF_CTRL_80_MASK |
			    HDA_PARAM_PIN_CAP_VREF_CTRL_50_MASK);
		w = hdac_widget_get(devinfo, 25);
		if (w != NULL && w->enable != 0 && w->type ==
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
		    (w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK) ==
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK)
			w->wclass.pin.cap &= ~(
			    HDA_PARAM_PIN_CAP_VREF_CTRL_100_MASK |
			    HDA_PARAM_PIN_CAP_VREF_CTRL_80_MASK |
			    HDA_PARAM_PIN_CAP_VREF_CTRL_50_MASK);
		/*
		 * nid: 26 = Line-in, leave it alone.
		 */
		break;
	case HDA_CODEC_AD1981HD:
		w = hdac_widget_get(devinfo, 11);
		if (w != NULL && w->enable != 0 && w->nconns > 3)
			w->selconn = 3;
		if (subvendor == IBM_M52_SUBVENDOR) {
			ctl = hdac_audio_ctl_amp_get(devinfo, 7, 0, 1);
			if (ctl != NULL)
				ctl->ossmask = SOUND_MASK_SPEAKER;
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
		if (subvendor == ASUS_M2NPVMX_SUBVENDOR) {
			/* nid 28 is mic, nid 29 is line-in */
			w = hdac_widget_get(devinfo, 15);
			if (w != NULL)
				w->selconn = 2;
			w = hdac_widget_get(devinfo, 16);
			if (w != NULL)
				w->selconn = 1;
		}
		break;
	case HDA_CODEC_AD1988:
		/*w = hdac_widget_get(devinfo, 12);
		if (w != NULL) {
			w->selconn = 1;
			w->pflags |= HDA_ADC_LOCKED;
		}
		w = hdac_widget_get(devinfo, 13);
		if (w != NULL) {
			w->selconn = 4;
			w->pflags |= HDA_ADC_LOCKED;
		}
		w = hdac_widget_get(devinfo, 14);
		if (w != NULL) {
			w->selconn = 2;
			w->pflags |= HDA_ADC_LOCKED;
		}*/
		ctl = hdac_audio_ctl_amp_get(devinfo, 57, 0, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_IGAIN;
			ctl->widget->ctlflags |= SOUND_MASK_IGAIN;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 58, 0, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_IGAIN;
			ctl->widget->ctlflags |= SOUND_MASK_IGAIN;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 60, 0, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_IGAIN;
			ctl->widget->ctlflags |= SOUND_MASK_IGAIN;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 32, 0, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_MIC | SOUND_MASK_VOLUME;
			ctl->widget->ctlflags |= SOUND_MASK_MIC;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 32, 4, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_MIC | SOUND_MASK_VOLUME;
			ctl->widget->ctlflags |= SOUND_MASK_MIC;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 32, 1, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_LINE | SOUND_MASK_VOLUME;
			ctl->widget->ctlflags |= SOUND_MASK_LINE;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 32, 7, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_SPEAKER | SOUND_MASK_VOLUME;
			ctl->widget->ctlflags |= SOUND_MASK_SPEAKER;
		}
		break;
	case HDA_CODEC_STAC9221:
		/*
		 * Dell XPS M1210 need all DACs for each output jacks
		 */
		if (subvendor == DELL_XPSM1210_SUBVENDOR)
			break;
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
	case HDA_CODEC_STAC9227:
		w = hdac_widget_get(devinfo, 8);
		if (w != NULL)
			w->enable = 0;
		w = hdac_widget_get(devinfo, 9);
		if (w != NULL)
			w->enable = 0;
		break;
	case HDA_CODEC_CXWAIKIKI:
		if (subvendor == HP_DV5000_SUBVENDOR) {
			w = hdac_widget_get(devinfo, 27);
			if (w != NULL)
				w->enable = 0;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 16, 0, 1);
		if (ctl != NULL)
			ctl->ossmask = SOUND_MASK_SKIP;
		ctl = hdac_audio_ctl_amp_get(devinfo, 25, 0, 1);
		if (ctl != NULL && ctl->childwidget != NULL &&
		    ctl->childwidget->enable != 0) {
			ctl->ossmask = SOUND_MASK_PCM | SOUND_MASK_VOLUME;
			ctl->childwidget->ctlflags |= SOUND_MASK_PCM;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 25, 1, 1);
		if (ctl != NULL && ctl->childwidget != NULL &&
		    ctl->childwidget->enable != 0) {
			ctl->ossmask = SOUND_MASK_LINE | SOUND_MASK_VOLUME;
			ctl->childwidget->ctlflags |= SOUND_MASK_LINE;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 25, 2, 1);
		if (ctl != NULL && ctl->childwidget != NULL &&
		    ctl->childwidget->enable != 0) {
			ctl->ossmask = SOUND_MASK_MIC | SOUND_MASK_VOLUME;
			ctl->childwidget->ctlflags |= SOUND_MASK_MIC;
		}
		ctl = hdac_audio_ctl_amp_get(devinfo, 26, 0, 1);
		if (ctl != NULL) {
			ctl->ossmask = SOUND_MASK_SKIP;
			/* XXX mixer \=rec mic broken.. why?!? */
			/* ctl->widget->ctlflags |= SOUND_MASK_MIC; */
		}
		break;
	default:
		break;
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
		case SOUND_MIXER_IGAIN:
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
			/* XXX This should be compressed! */
			if (((ctl->widget->nid == w->nid) ||
			    (ctl->widget->nid == pnid && ctl->index == index &&
			    (ctl->dir & HDA_CTL_IN)) ||
			    (ctl->widget->nid == pnid && pw != NULL &&
			    pw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR &&
			    (pw->nconns < 2 || pw->selconn == index ||
			    pw->selconn == -1) &&
			    (ctl->dir & HDA_CTL_OUT)) ||
			    (strategy == HDA_PARSE_DIRECT &&
			    ctl->widget->nid == w->nid)) &&
			    !(ctl->ossmask & ~SOUND_MASK_VOLUME)) {
				/*if (pw != NULL && pw->selconn == -1)
					pw->selconn = index;
				fl |= SOUND_MASK_VOLUME;
				fl |= SOUND_MASK_PCM;
				ctl->ossmask |= SOUND_MASK_VOLUME;
				ctl->ossmask |= SOUND_MASK_PCM;
				ctl->ossdev = SOUND_MIXER_PCM;*/
				if (!(w->ctlflags & SOUND_MASK_PCM) ||
				    (pw != NULL &&
				    !(pw->ctlflags & SOUND_MASK_PCM))) {
					fl |= SOUND_MASK_VOLUME;
					fl |= SOUND_MASK_PCM;
					ctl->ossmask |= SOUND_MASK_VOLUME;
					ctl->ossmask |= SOUND_MASK_PCM;
					ctl->ossdev = SOUND_MIXER_PCM;
					w->ctlflags |= SOUND_MASK_VOLUME;
					w->ctlflags |= SOUND_MASK_PCM;
					if (pw != NULL) {
						if (pw->selconn == -1)
							pw->selconn = index;
						pw->ctlflags |=
						    SOUND_MASK_VOLUME;
						pw->ctlflags |=
						    SOUND_MASK_PCM;
					}
				}
			}
		}
		w->ctlflags |= fl;
		return (fl);
	} else if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
	    HDA_PARAM_PIN_CAP_INPUT_CAP(w->wclass.pin.cap) &&
	    (w->pflags & HDA_ADC_PATH)) {
		conndev = w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
		i = 0;
		while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
			if (ctl->enable == 0 || ctl->widget == NULL)
				continue;
			/* XXX This should be compressed! */
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
			    !(ctl->ossmask & ~SOUND_MASK_VOLUME)) {
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
			/* XXX This should be compressed! */
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
			    !(ctl->ossmask & ~SOUND_MASK_VOLUME)) {
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
	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    "HDA_DEBUG: HWiP: HDA Widget Parser - Revision %d\n",
		    HDA_WIDGET_PARSER_REV);
	);
	dacs = hdac_audio_build_tree_strategy(devinfo);
	if (dacs == 0) {
		HDA_BOOTVERBOSE(
			device_printf(devinfo->codec->sc->dev,
			    "HDA_DEBUG: HWiP: 0 DAC path found! "
			    "Retrying parser "
			    "using HDA_PARSE_DIRECT strategy.\n");
		);
		strategy = HDA_PARSE_DIRECT;
		devinfo->function.audio.parsing_strategy = strategy;
		dacs = hdac_audio_build_tree_strategy(devinfo);
	}

	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    "HDA_DEBUG: HWiP: Found %d DAC path using HDA_PARSE_%s "
		    "strategy.\n",
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
#define HDA_COMMIT_MISC	(1 << 4)
#define HDA_COMMIT_ALL	(HDA_COMMIT_CONN | HDA_COMMIT_CTRL | \
			HDA_COMMIT_EAPD | HDA_COMMIT_GPIO | HDA_COMMIT_MISC)

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

	if ((cfl & HDA_COMMIT_MISC)) {
		if (sc->pci_subvendor == APPLE_INTEL_MAC)
			hdac_command(sc, HDA_CMD_12BIT(cad, devinfo->nid,
			    0x7e7, 0), cad);
	}

	if (cfl & HDA_COMMIT_GPIO) {
		uint32_t gdata, gmask, gdir;
		int commitgpio, numgpio;

		gdata = 0;
		gmask = 0;
		gdir = 0;
		commitgpio = 0;

		numgpio = HDA_PARAM_GPIO_COUNT_NUM_GPIO(
		    devinfo->function.audio.gpio);

		if (devinfo->function.audio.quirks & HDA_QUIRK_GPIOFLUSH)
			commitgpio = (numgpio > 0) ? 1 : 0;
		else {
			for (i = 0; i < numgpio && i < HDA_GPIO_MAX; i++) {
				if (!(devinfo->function.audio.quirks &
				    (1 << i)))
					continue;
				if (commitgpio == 0) {
					commitgpio = 1;
					HDA_BOOTVERBOSE(
						gdata = hdac_command(sc,
						    HDA_CMD_GET_GPIO_DATA(cad,
						    devinfo->nid), cad);
						gmask = hdac_command(sc,
						    HDA_CMD_GET_GPIO_ENABLE_MASK(cad,
						    devinfo->nid), cad);
						gdir = hdac_command(sc,
						    HDA_CMD_GET_GPIO_DIRECTION(cad,
						    devinfo->nid), cad);
						device_printf(sc->dev,
						    "GPIO init: data=0x%08x "
						    "mask=0x%08x dir=0x%08x\n",
						    gdata, gmask, gdir);
						gdata = 0;
						gmask = 0;
						gdir = 0;
					);
				}
				gdata |= 1 << i;
				gmask |= 1 << i;
				gdir |= 1 << i;
			}
		}

		if (commitgpio != 0) {
			HDA_BOOTVERBOSE(
				device_printf(sc->dev,
				    "GPIO commit: data=0x%08x mask=0x%08x "
				    "dir=0x%08x\n",
				    gdata, gmask, gdir);
			);
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_ENABLE_MASK(cad, devinfo->nid,
			    gmask), cad);
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_DIRECTION(cad, devinfo->nid,
			    gdir), cad);
			hdac_command(sc,
			    HDA_CMD_SET_GPIO_DATA(cad, devinfo->nid,
			    gdata), cad);
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
		    	uint32_t pincap;

			pincap = w->wclass.pin.cap;

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
				if ((devinfo->function.audio.quirks & HDA_QUIRK_OVREF100) &&
				    HDA_PARAM_PIN_CAP_VREF_CTRL_100(pincap))
					w->wclass.pin.ctrl |=
					    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE(
					    HDA_CMD_PIN_WIDGET_CTRL_VREF_ENABLE_100);
				else if ((devinfo->function.audio.quirks & HDA_QUIRK_OVREF80) &&
				    HDA_PARAM_PIN_CAP_VREF_CTRL_80(pincap))
					w->wclass.pin.ctrl |=
					    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE(
					    HDA_CMD_PIN_WIDGET_CTRL_VREF_ENABLE_80);
				else if ((devinfo->function.audio.quirks & HDA_QUIRK_OVREF50) &&
				    HDA_PARAM_PIN_CAP_VREF_CTRL_50(pincap))
					w->wclass.pin.ctrl |=
					    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE(
					    HDA_CMD_PIN_WIDGET_CTRL_VREF_ENABLE_50);
			} else if (w->pflags & HDA_ADC_PATH) {
				w->wclass.pin.ctrl &=
				    ~(HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
				    HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE);
				if ((devinfo->function.audio.quirks & HDA_QUIRK_IVREF100) &&
				    HDA_PARAM_PIN_CAP_VREF_CTRL_100(pincap))
					w->wclass.pin.ctrl |=
					    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE(
					    HDA_CMD_PIN_WIDGET_CTRL_VREF_ENABLE_100);
				else if ((devinfo->function.audio.quirks & HDA_QUIRK_IVREF80) &&
				    HDA_PARAM_PIN_CAP_VREF_CTRL_80(pincap))
					w->wclass.pin.ctrl |=
					    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE(
					    HDA_CMD_PIN_WIDGET_CTRL_VREF_ENABLE_80);
				else if ((devinfo->function.audio.quirks & HDA_QUIRK_IVREF50) &&
				    HDA_PARAM_PIN_CAP_VREF_CTRL_50(pincap))
					w->wclass.pin.ctrl |=
					    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE(
					    HDA_CMD_PIN_WIDGET_CTRL_VREF_ENABLE_50);
			} else
				w->wclass.pin.ctrl &= ~(
				    HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE |
				    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
				    HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE |
				    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE_MASK);
			hdac_command(sc,
			    HDA_CMD_SET_PIN_WIDGET_CTRL(cad, w->nid,
			    w->wclass.pin.ctrl), cad);
		}
		if ((cfl & HDA_COMMIT_EAPD) &&
		    w->param.eapdbtl != HDAC_INVALID) {
		    	uint32_t val;

			val = w->param.eapdbtl;
			if (devinfo->function.audio.quirks &
			    HDA_QUIRK_EAPDINV)
				val ^= HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
			hdac_command(sc,
			    HDA_CMD_SET_EAPD_BTL_ENABLE(cad, w->nid,
			    val), cad);

		}
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
			HDA_BOOTVERBOSE(
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
		HDA_BOOTVERBOSE(
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
		    !(w->pflags & path))
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
		if (ret == 0) {
			fmtcap = w->param.supp_stream_formats;
			pcmcap = w->param.supp_pcm_size_rate;
		} else {
			fmtcap &= w->param.supp_stream_formats;
			pcmcap &= w->param.supp_pcm_size_rate;
		}
		ch->io[ret++] = i;
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
		    ctl->widget->enable == 0 || (ctl->ossmask &
		    (SOUND_MASK_SKIP | SOUND_MASK_DISABLE)))
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
	if (HDA_PARAM_PIN_CAP_VREF_CTRL(pincap)) {
		printf(" VREF[");
		if (HDA_PARAM_PIN_CAP_VREF_CTRL_50(pincap))
			printf(" 50");
		if (HDA_PARAM_PIN_CAP_VREF_CTRL_80(pincap))
			printf(" 80");
		if (HDA_PARAM_PIN_CAP_VREF_CTRL_100(pincap))
			printf(" 100");
		if (HDA_PARAM_PIN_CAP_VREF_CTRL_GROUND(pincap))
			printf(" GROUND");
		if (HDA_PARAM_PIN_CAP_VREF_CTRL_HIZ(pincap))
			printf(" HIZ");
		printf(" ]");
	}
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
	device_printf(sc->dev, "     %s amp: 0x%08x\n", banner, cap);
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
		if (w->param.eapdbtl != HDAC_INVALID)
			device_printf(sc->dev, "           EAPD: 0x%08x\n",
			    w->param.eapdbtl);
		if (HDA_PARAM_AUDIO_WIDGET_CAP_OUT_AMP(w->param.widget_cap) &&
		    w->param.outamp_cap != 0)
			hdac_dump_amp(sc, w->param.outamp_cap, "Output");
		if (HDA_PARAM_AUDIO_WIDGET_CAP_IN_AMP(w->param.widget_cap) &&
		    w->param.inamp_cap != 0)
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

static int
hdac_dump_dac_internal(struct hdac_devinfo *devinfo, nid_t nid, int depth)
{
	struct hdac_widget *w, *cw;
	struct hdac_softc *sc = devinfo->codec->sc;
	int i;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0 || !(w->pflags & HDA_DAC_PATH))
		return (0);

	if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) {
		device_printf(sc->dev, "\n");
		device_printf(sc->dev, "    nid=%d [%s]\n", w->nid, w->name);
		device_printf(sc->dev, "      ^\n");
		device_printf(sc->dev, "      |\n");
		device_printf(sc->dev, "      +-----<------+\n");
	} else {
		device_printf(sc->dev, "                   ^\n");
		device_printf(sc->dev, "                   |\n");
		device_printf(sc->dev, "               ");
		printf("  nid=%d [%s]\n", w->nid, w->name);
	}

	if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT) {
		return (1);
	} else if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER) {
		for (i = 0; i < w->nconns; i++) {
			cw = hdac_widget_get(devinfo, w->conns[i]);
			if (cw == NULL || cw->enable == 0 || cw->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
				continue;
			if (hdac_dump_dac_internal(devinfo, cw->nid,
			    depth + 1) != 0)
				return (1);
		}
	} else if ((w->type ==
	    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR ||
	    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) &&
	    w->selconn > -1 && w->selconn < w->nconns) {
		if (hdac_dump_dac_internal(devinfo, w->conns[w->selconn],
		    depth + 1) != 0)
			return (1);
	}

	return (0);
}

static void
hdac_dump_dac(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w;
	struct hdac_softc *sc = devinfo->codec->sc;
	int i, printed = 0;

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX ||
		    !(w->pflags & HDA_DAC_PATH))
			continue;
		if (printed == 0) {
			printed = 1;
			device_printf(sc->dev, "\n");
			device_printf(sc->dev, "Playback path:\n");
		}
		hdac_dump_dac_internal(devinfo, w->nid, 0);
	}
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
hdac_release_resources(struct hdac_softc *sc)
{
	struct hdac_devinfo *devinfo = NULL;
	device_t *devlist = NULL;
	int i, devcount;

	if (sc == NULL)
		return;

	hdac_lock(sc);
	sc->polling = 0;
	sc->poll_ival = 0;
	callout_stop(&sc->poll_hdac);
	callout_stop(&sc->poll_jack);
	hdac_reset(sc);
	hdac_unlock(sc);
	callout_drain(&sc->poll_hdac);
	callout_drain(&sc->poll_jack);

	hdac_irq_free(sc);

	device_get_children(sc->dev, &devlist, &devcount);
	for (i = 0; devlist != NULL && i < devcount; i++) {
		devinfo = (struct hdac_devinfo *)device_get_ivars(devlist[i]);
		if (devinfo == NULL)
			continue;
		if (devinfo->widget != NULL)
			free(devinfo->widget, M_HDAC);
		if (devinfo->node_type ==
		    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO &&
		    devinfo->function.audio.ctl != NULL)
			free(devinfo->function.audio.ctl, M_HDAC);
		free(devinfo, M_HDAC);
		device_delete_child(sc->dev, devlist[i]);
	}
	if (devlist != NULL)
		free(devlist, M_TEMP);

	for (i = 0; i < HDAC_CODEC_MAX; i++) {
		if (sc->codecs[i] != NULL)
			free(sc->codecs[i], M_HDAC);
		sc->codecs[i] = NULL;
	}

	hdac_dma_free(sc, &sc->pos_dma);
	hdac_dma_free(sc, &sc->rirb_dma);
	hdac_dma_free(sc, &sc->corb_dma);
	if (sc->play.blkcnt > 0)
		hdac_dma_free(sc, &sc->play.bdl_dma);
	if (sc->rec.blkcnt > 0)
		hdac_dma_free(sc, &sc->rec.bdl_dma);
	if (sc->chan_dmat != NULL) {
		bus_dma_tag_destroy(sc->chan_dmat);
		sc->chan_dmat = NULL;
	}
	hdac_mem_free(sc);
	snd_mtxfree(sc->lock);
	free(sc, M_DEVBUF);
}

/* This function surely going to make its way into upper level someday. */
static void
hdac_config_fetch(struct hdac_softc *sc, uint32_t *on, uint32_t *off)
{
	const char *res = NULL;
	int i = 0, j, k, len, inv;

	if (on != NULL)
		*on = 0;
	if (off != NULL)
		*off = 0;
	if (sc == NULL)
		return;
	if (resource_string_value(device_get_name(sc->dev),
	    device_get_unit(sc->dev), "config", &res) != 0)
		return;
	if (!(res != NULL && strlen(res) > 0))
		return;
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: HDA Config:");
	);
	for (;;) {
		while (res[i] != '\0' &&
		    (res[i] == ',' || isspace(res[i]) != 0))
			i++;
		if (res[i] == '\0') {
			HDA_BOOTVERBOSE(
				printf("\n");
			);
			return;
		}
		j = i;
		while (res[j] != '\0' &&
		    !(res[j] == ',' || isspace(res[j]) != 0))
			j++;
		len = j - i;
		if (len > 2 && strncmp(res + i, "no", 2) == 0)
			inv = 2;
		else
			inv = 0;
		for (k = 0; len > inv && k < HDAC_QUIRKS_TAB_LEN; k++) {
			if (strncmp(res + i + inv,
			    hdac_quirks_tab[k].key, len - inv) != 0)
				continue;
			if (len - inv != strlen(hdac_quirks_tab[k].key))
				break;
			HDA_BOOTVERBOSE(
				printf(" %s%s", (inv != 0) ? "no" : "",
				    hdac_quirks_tab[k].key);
			);
			if (inv == 0 && on != NULL)
				*on |= hdac_quirks_tab[k].value;
			else if (inv != 0 && off != NULL)
				*off |= hdac_quirks_tab[k].value;
			break;
		}
		i = j;
	}
}

#ifdef SND_DYNSYSCTL
static int
sysctl_hdac_polling(SYSCTL_HANDLER_ARGS)
{
	struct hdac_softc *sc;
	struct hdac_devinfo *devinfo;
	device_t dev;
	uint32_t ctl;
	int err, val;

	dev = oidp->oid_arg1;
	devinfo = pcm_getdevinfo(dev);
	if (devinfo == NULL || devinfo->codec == NULL ||
	    devinfo->codec->sc == NULL)
		return (EINVAL);
	sc = devinfo->codec->sc;
	hdac_lock(sc);
	val = sc->polling;
	hdac_unlock(sc);
	err = sysctl_handle_int(oidp, &val, 0, req);

	if (err != 0 || req->newptr == NULL)
		return (err);
	if (val < 0 || val > 1)
		return (EINVAL);

	hdac_lock(sc);
	if (val != sc->polling) {
		if (hda_chan_active(sc) != 0)
			err = EBUSY;
		else if (val == 0) {
			callout_stop(&sc->poll_hdac);
			hdac_unlock(sc);
			callout_drain(&sc->poll_hdac);
			hdac_lock(sc);
			HDAC_WRITE_2(&sc->mem, HDAC_RINTCNT,
			    sc->rirb_size / 2);
			ctl = HDAC_READ_1(&sc->mem, HDAC_RIRBCTL);
			ctl |= HDAC_RIRBCTL_RINTCTL;
			HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL, ctl);
			HDAC_WRITE_4(&sc->mem, HDAC_INTCTL,
			    HDAC_INTCTL_CIE | HDAC_INTCTL_GIE);
			sc->polling = 0;
			DELAY(1000);
		} else {
			HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, 0);
			HDAC_WRITE_2(&sc->mem, HDAC_RINTCNT, 0);
			ctl = HDAC_READ_1(&sc->mem, HDAC_RIRBCTL);
			ctl &= ~HDAC_RIRBCTL_RINTCTL;
			HDAC_WRITE_1(&sc->mem, HDAC_RIRBCTL, ctl);
			callout_reset(&sc->poll_hdac, 1, hdac_poll_callback,
			    sc);
			sc->polling = 1;
			DELAY(1000);
		}
	}
	hdac_unlock(sc);

	return (err);
}

static int
sysctl_hdac_polling_interval(SYSCTL_HANDLER_ARGS)
{
	struct hdac_softc *sc;
	struct hdac_devinfo *devinfo;
	device_t dev;
	int err, val;

	dev = oidp->oid_arg1;
	devinfo = pcm_getdevinfo(dev);
	if (devinfo == NULL || devinfo->codec == NULL ||
	    devinfo->codec->sc == NULL)
		return (EINVAL);
	sc = devinfo->codec->sc;
	hdac_lock(sc);
	val = ((uint64_t)sc->poll_ival * 1000) / hz;
	hdac_unlock(sc);
	err = sysctl_handle_int(oidp, &val, 0, req);

	if (err != 0 || req->newptr == NULL)
		return (err);

	if (val < 1)
		val = 1;
	if (val > 5000)
		val = 5000;
	val = ((uint64_t)val * hz) / 1000;
	if (val < 1)
		val = 1;
	if (val > (hz * 5))
		val = hz * 5;

	hdac_lock(sc);
	sc->poll_ival = val;
	hdac_unlock(sc);

	return (err);
}

#ifdef SND_DEBUG
static int
sysctl_hdac_dump(SYSCTL_HANDLER_ARGS)
{
	struct hdac_softc *sc;
	struct hdac_devinfo *devinfo;
	struct hdac_widget *w;
	device_t dev;
	uint32_t res, execres;
	int i, err, val;
	nid_t cad;

	dev = oidp->oid_arg1;
	devinfo = pcm_getdevinfo(dev);
	if (devinfo == NULL || devinfo->codec == NULL ||
	    devinfo->codec->sc == NULL)
		return (EINVAL);
	val = 0;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL || val == 0)
		return (err);
	sc = devinfo->codec->sc;
	cad = devinfo->codec->cad;
	hdac_lock(sc);
	device_printf(dev, "HDAC Dump AFG [nid=%d]:\n", devinfo->nid);
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->type !=
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		execres = hdac_command(sc, HDA_CMD_SET_PIN_SENSE(cad, w->nid, 0),
		    cad);
		res = hdac_command(sc, HDA_CMD_GET_PIN_SENSE(cad, w->nid), cad);
		device_printf(dev, "nid=%-3d exec=0x%08x sense=0x%08x [%s]\n",
		    w->nid, execres, res,
		    (w->enable == 0) ? "DISABLED" : "ENABLED");
	}
	device_printf(dev,
	    "NumGPIO=%d NumGPO=%d NumGPI=%d GPIWake=%d GPIUnsol=%d\n",
	    HDA_PARAM_GPIO_COUNT_NUM_GPIO(devinfo->function.audio.gpio),
	    HDA_PARAM_GPIO_COUNT_NUM_GPO(devinfo->function.audio.gpio),
	    HDA_PARAM_GPIO_COUNT_NUM_GPI(devinfo->function.audio.gpio),
	    HDA_PARAM_GPIO_COUNT_GPI_WAKE(devinfo->function.audio.gpio),
	    HDA_PARAM_GPIO_COUNT_GPI_UNSOL(devinfo->function.audio.gpio));
	if (1 || HDA_PARAM_GPIO_COUNT_NUM_GPI(devinfo->function.audio.gpio) > 0) {
		device_printf(dev, " GPI:");
		res = hdac_command(sc,
		    HDA_CMD_GET_GPI_DATA(cad, devinfo->nid), cad);
		printf(" data=0x%08x", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPI_WAKE_ENABLE_MASK(cad, devinfo->nid),
		    cad);
		printf(" wake=0x%08x", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPI_UNSOLICITED_ENABLE_MASK(cad, devinfo->nid),
		    cad);
		printf(" unsol=0x%08x", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPI_STICKY_MASK(cad, devinfo->nid), cad);
		printf(" sticky=0x%08x\n", res);
	}
	if (1 || HDA_PARAM_GPIO_COUNT_NUM_GPO(devinfo->function.audio.gpio) > 0) {
		device_printf(dev, " GPO:");
		res = hdac_command(sc,
		    HDA_CMD_GET_GPO_DATA(cad, devinfo->nid), cad);
		printf(" data=0x%08x\n", res);
	}
	if (1 || HDA_PARAM_GPIO_COUNT_NUM_GPIO(devinfo->function.audio.gpio) > 0) {
		device_printf(dev, "GPI0:");
		res = hdac_command(sc,
		    HDA_CMD_GET_GPIO_DATA(cad, devinfo->nid), cad);
		printf(" data=0x%08x", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPIO_ENABLE_MASK(cad, devinfo->nid), cad);
		printf(" enable=0x%08x", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPIO_DIRECTION(cad, devinfo->nid), cad);
		printf(" direction=0x%08x\n", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPIO_WAKE_ENABLE_MASK(cad, devinfo->nid), cad);
		device_printf(dev, "      wake=0x%08x", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPIO_UNSOLICITED_ENABLE_MASK(cad, devinfo->nid),
		    cad);
		printf("  unsol=0x%08x", res);
		res = hdac_command(sc,
		    HDA_CMD_GET_GPIO_STICKY_MASK(cad, devinfo->nid), cad);
		printf("    sticky=0x%08x\n", res);
	}
	hdac_unlock(sc);
	return (0);
}
#endif
#endif

static void
hdac_attach2(void *arg)
{
	struct hdac_softc *sc;
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	uint32_t quirks_on, quirks_off;
	int pcnt, rcnt;
	int i;
	char status[SND_STATUSLEN];
	device_t *devlist = NULL;
	int devcount;
	struct hdac_devinfo *devinfo = NULL;

	sc = (struct hdac_softc *)arg;

	hdac_config_fetch(sc, &quirks_on, &quirks_off);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: HDA Config: on=0x%08x off=0x%08x\n",
		    quirks_on, quirks_off);
	);

	hdac_lock(sc);

	/* Remove ourselves from the config hooks */
	if (sc->intrhook.ich_func != NULL) {
		config_intrhook_disestablish(&sc->intrhook);
		sc->intrhook.ich_func = NULL;
	}

	/* Start the corb and rirb engines */
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Starting CORB Engine...\n");
	);
	hdac_corb_start(sc);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Starting RIRB Engine...\n");
	);
	hdac_rirb_start(sc);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "HDA_DEBUG: Enabling controller interrupt...\n");
	);
	if (sc->polling == 0)
		HDAC_WRITE_4(&sc->mem, HDAC_INTCTL,
		    HDAC_INTCTL_CIE | HDAC_INTCTL_GIE);
	HDAC_WRITE_4(&sc->mem, HDAC_GCTL, HDAC_READ_4(&sc->mem, HDAC_GCTL) |
	    HDAC_GCTL_UNSOL);

	DELAY(1000);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Scanning HDA codecs...\n");
	);
	hdac_scan_codecs(sc);

	device_get_children(sc->dev, &devlist, &devcount);
	for (i = 0; devlist != NULL && i < devcount; i++) {
		devinfo = (struct hdac_devinfo *)device_get_ivars(devlist[i]);
		if (devinfo != NULL && devinfo->node_type ==
		    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO) {
			break;
		} else
			devinfo = NULL;
	}
	if (devlist != NULL)
		free(devlist, M_TEMP);

	if (devinfo == NULL) {
		hdac_unlock(sc);
		device_printf(sc->dev, "Audio Function Group not found!\n");
		hdac_release_resources(sc);
		return;
	}

	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "HDA_DEBUG: Parsing AFG nid=%d cad=%d\n",
		    devinfo->nid, devinfo->codec->cad);
	);
	hdac_audio_parse(devinfo);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Parsing Ctls...\n");
	);
	hdac_audio_ctl_parse(devinfo);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Parsing vendor patch...\n");
	);
	hdac_vendor_patch_parse(devinfo);
	if (quirks_on != 0)
		devinfo->function.audio.quirks |= quirks_on;
	if (quirks_off != 0)
		devinfo->function.audio.quirks &= ~quirks_off;

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
		if (ctl->ossmask & SOUND_MASK_DISABLE)
			ctl->enable = 0;
		w = ctl->widget;
		if (w->enable == 0 ||
		    HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap))
			ctl->enable = 0;
		w = ctl->childwidget;
		if (w == NULL)
			continue;
		if (w->enable == 0 ||
		    HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap))
			ctl->enable = 0;
	}

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Building AFG tree...\n");
	);
	hdac_audio_build_tree(devinfo);

	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->ossmask & (SOUND_MASK_SKIP | SOUND_MASK_DISABLE))
			ctl->ossmask = 0;
	}
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: AFG commit...\n");
	);
	hdac_audio_commit(devinfo, HDA_COMMIT_ALL);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: Ctls commit...\n");
	);
	hdac_audio_ctl_commit(devinfo);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: PCMDIR_PLAY setup...\n");
	);
	pcnt = hdac_pcmchannel_setup(devinfo, PCMDIR_PLAY);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA_DEBUG: PCMDIR_REC setup...\n");
	);
	rcnt = hdac_pcmchannel_setup(devinfo, PCMDIR_REC);

	hdac_unlock(sc);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "HDA_DEBUG: OSS mixer initialization...\n");
	);

	/*
	 * There is no point of return after this. If the driver failed,
	 * so be it. Let the detach procedure do all the cleanup.
	 */
	if (mixer_init(sc->dev, &hdac_audio_ctl_ossmixer_class, devinfo) != 0)
		device_printf(sc->dev, "Can't register mixer\n");

	if (pcnt > 0)
		pcnt = 1;
	if (rcnt > 0)
		rcnt = 1;

	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "HDA_DEBUG: Registering PCM channels...\n");
	);
	if (pcm_register(sc->dev, devinfo, pcnt, rcnt) != 0)
		device_printf(sc->dev, "Can't register PCM\n");

	sc->registered++;

	if ((devinfo->function.audio.quirks & HDA_QUIRK_DMAPOS) &&
	    hdac_dma_alloc(sc, &sc->pos_dma,
	    (sc->num_iss + sc->num_oss + sc->num_bss) * 8) != 0) {
		HDA_BOOTVERBOSE(
			device_printf(sc->dev,
			    "Failed to allocate DMA pos buffer (non-fatal)\n");
		);
	}

	for (i = 0; i < pcnt; i++)
		pcm_addchan(sc->dev, PCMDIR_PLAY, &hdac_channel_class, devinfo);
	for (i = 0; i < rcnt; i++)
		pcm_addchan(sc->dev, PCMDIR_REC, &hdac_channel_class, devinfo);

#ifdef SND_DYNSYSCTL
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(sc->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)), OID_AUTO,
	    "polling", CTLTYPE_INT | CTLFLAG_RW, sc->dev, sizeof(sc->dev),
	    sysctl_hdac_polling, "I", "Enable polling mode");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(sc->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)), OID_AUTO,
	    "polling_interval", CTLTYPE_INT | CTLFLAG_RW, sc->dev,
	    sizeof(sc->dev), sysctl_hdac_polling_interval, "I",
	    "Controller/Jack Sense polling interval (1-1000 ms)");
#ifdef SND_DEBUG
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(sc->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)), OID_AUTO,
	    "dump", CTLTYPE_INT | CTLFLAG_RW, sc->dev, sizeof(sc->dev),
	    sysctl_hdac_dump, "I", "Dump states");
#endif
#endif

	snprintf(status, SND_STATUSLEN, "at memory 0x%lx irq %ld %s [%s]",
	    rman_get_start(sc->mem.mem_res), rman_get_start(sc->irq.irq_res),
	    PCM_KLDSTRING(snd_hda), HDA_DRV_TEST_REV);
	pcm_setstatus(sc->dev, status);
	device_printf(sc->dev, "<HDA Codec: %s>\n", hdac_codec_name(devinfo));
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "<HDA Codec ID: 0x%08x>\n",
		    hdac_codec_id(devinfo));
	);
	device_printf(sc->dev, "<HDA Driver Revision: %s>\n",
	    HDA_DRV_TEST_REV);

	HDA_BOOTVERBOSE(
		if (devinfo->function.audio.quirks != 0) {
			device_printf(sc->dev, "\n");
			device_printf(sc->dev, "HDA config/quirks:");
			for (i = 0; i < HDAC_QUIRKS_TAB_LEN; i++) {
				if ((devinfo->function.audio.quirks &
				    hdac_quirks_tab[i].value) ==
				    hdac_quirks_tab[i].value)
					printf(" %s", hdac_quirks_tab[i].key);
			}
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

	if (sc->polling != 0) {
		hdac_lock(sc);
		callout_reset(&sc->poll_hdac, 1, hdac_poll_callback, sc);
		hdac_unlock(sc);
	}
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
	struct hdac_devinfo *devinfo = NULL;
	int err;

	devinfo = (struct hdac_devinfo *)pcm_getdevinfo(dev);
	if (devinfo != NULL && devinfo->codec != NULL)
		sc = devinfo->codec->sc;
	if (sc == NULL)
		return (0);

	if (sc->registered > 0) {
		err = pcm_unregister(dev);
		if (err != 0)
			return (err);
	}

	hdac_release_resources(sc);

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
