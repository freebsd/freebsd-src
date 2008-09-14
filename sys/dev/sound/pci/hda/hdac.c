/*-
 * Copyright (c) 2006 Stephane E. Potvin <sepotvin@videotron.ca>
 * Copyright (c) 2006 Ariff Abdullah <ariff@FreeBSD.org>
 * Copyright (c) 2008 Alexander Motin <mav@FreeBSD.org>
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
 *    *             Alexander Motin <mav@FreeBSD.org>                   *
 *    *                                                                 *
 *    *   ....and various people from freebsd-multimedia@FreeBSD.org    *
 *    *                                                                 *
 *    * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

#include <dev/sound/pcm/sound.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sys/ctype.h>
#include <sys/taskqueue.h>

#include <dev/sound/pci/hda/hdac_private.h>
#include <dev/sound/pci/hda/hdac_reg.h>
#include <dev/sound/pci/hda/hda_reg.h>
#include <dev/sound/pci/hda/hdac.h>

#include "mixer_if.h"

#define HDA_DRV_TEST_REV	"20080913_0111"
#define HDA_WIDGET_PARSER_REV	2

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

#undef HDAC_MSI_ENABLED
#if __FreeBSD_version >= 700026 ||					\
    (__FreeBSD_version < 700000 && __FreeBSD_version >= 602106)
#define HDAC_MSI_ENABLED	1
#endif

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

/*
 * Make room for possible 4096 playback/record channels, in 100 years to come.
 */
#define HDAC_TRIGGER_NONE	0x00000000
#define HDAC_TRIGGER_PLAY	0x00000fff
#define HDAC_TRIGGER_REC	0x00fff000
#define HDAC_TRIGGER_UNSOL	0x80000000

#define HDA_MODEL_CONSTRUCT(vendor, model)	\
		(((uint32_t)(model) << 16) | ((vendor##_VENDORID) & 0xffff))

/* Controller models */

/* Intel */
#define INTEL_VENDORID		0x8086
#define HDA_INTEL_82801F	HDA_MODEL_CONSTRUCT(INTEL, 0x2668)
#define HDA_INTEL_63XXESB	HDA_MODEL_CONSTRUCT(INTEL, 0x269a)
#define HDA_INTEL_82801G	HDA_MODEL_CONSTRUCT(INTEL, 0x27d8)
#define HDA_INTEL_82801H	HDA_MODEL_CONSTRUCT(INTEL, 0x284b)
#define HDA_INTEL_82801I	HDA_MODEL_CONSTRUCT(INTEL, 0x293e)
#define HDA_INTEL_ALL		HDA_MODEL_CONSTRUCT(INTEL, 0xffff)

/* Nvidia */
#define NVIDIA_VENDORID		0x10de
#define HDA_NVIDIA_MCP51	HDA_MODEL_CONSTRUCT(NVIDIA, 0x026c)
#define HDA_NVIDIA_MCP55	HDA_MODEL_CONSTRUCT(NVIDIA, 0x0371)
#define HDA_NVIDIA_MCP61_1	HDA_MODEL_CONSTRUCT(NVIDIA, 0x03e4)
#define HDA_NVIDIA_MCP61_2	HDA_MODEL_CONSTRUCT(NVIDIA, 0x03f0)
#define HDA_NVIDIA_MCP65_1	HDA_MODEL_CONSTRUCT(NVIDIA, 0x044a)
#define HDA_NVIDIA_MCP65_2	HDA_MODEL_CONSTRUCT(NVIDIA, 0x044b)
#define HDA_NVIDIA_MCP67_1	HDA_MODEL_CONSTRUCT(NVIDIA, 0x055c)
#define HDA_NVIDIA_MCP67_2	HDA_MODEL_CONSTRUCT(NVIDIA, 0x055d)
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
#define HP_DC7700S_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x2801)
#define HP_DC7700_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0x2802)
#define HP_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(HP, 0xffff)
/* What is wrong with XN 2563 anyway? (Got the picture ?) */
#define HP_NX6325_SUBVENDORX	0x103c30b0

/* Dell */
#define DELL_VENDORID		0x1028
#define DELL_D630_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01f9)
#define DELL_D820_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x01cc)
#define DELL_V1500_SUBVENDOR	HDA_MODEL_CONSTRUCT(DELL, 0x0228)
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
#define ACER_A4520_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0x0127)
#define ACER_A4710_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0x012f)
#define ACER_A4715_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0x0133)
#define ACER_3681WXM_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0x0110)
#define ACER_T6292_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0x011b)
#define ACER_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(ACER, 0xffff)

/* Asus */
#define ASUS_VENDORID		0x1043
#define ASUS_A8X_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1153)
#define ASUS_U5F_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1263)
#define ASUS_W6F_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1263)
#define ASUS_A7M_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1323)
#define ASUS_F3JC_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1338)
#define ASUS_G2K_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1339)
#define ASUS_A7T_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x13c2)
#define ASUS_W2J_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1971)
#define ASUS_M5200_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x1993)
#define ASUS_P1AH2_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x81cb)
#define ASUS_M2NPVMX_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x81cb)
#define ASUS_M2V_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x81e7)
#define ASUS_P5BWD_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x81ec)
#define ASUS_M2N_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0x8234)
#define ASUS_A8NVMCSM_SUBVENDOR	HDA_MODEL_CONSTRUCT(NVIDIA, 0xcb84)
#define ASUS_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(ASUS, 0xffff)

/* IBM / Lenovo */
#define IBM_VENDORID		0x1014
#define IBM_M52_SUBVENDOR	HDA_MODEL_CONSTRUCT(IBM, 0x02f6)
#define IBM_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(IBM, 0xffff)

/* Lenovo */
#define LENOVO_VENDORID		0x17aa
#define LENOVO_3KN100_SUBVENDOR	HDA_MODEL_CONSTRUCT(LENOVO, 0x2066)
#define LENOVO_3KN200_SUBVENDOR	HDA_MODEL_CONSTRUCT(LENOVO, 0x384e)
#define LENOVO_TCA55_SUBVENDOR	HDA_MODEL_CONSTRUCT(LENOVO, 0x1015)
#define LENOVO_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(LENOVO, 0xffff)

/* Samsung */
#define SAMSUNG_VENDORID	0x144d
#define SAMSUNG_Q1_SUBVENDOR	HDA_MODEL_CONSTRUCT(SAMSUNG, 0xc027)
#define SAMSUNG_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(SAMSUNG, 0xffff)

/* Medion ? */
#define MEDION_VENDORID			0x161f
#define MEDION_MD95257_SUBVENDOR	HDA_MODEL_CONSTRUCT(MEDION, 0x203d)
#define MEDION_ALL_SUBVENDOR		HDA_MODEL_CONSTRUCT(MEDION, 0xffff)

/* Apple Computer Inc. */
#define APPLE_VENDORID		0x106b
#define APPLE_MB3_SUBVENDOR	HDA_MODEL_CONSTRUCT(APPLE, 0x00a1)

/* Sony */
#define SONY_VENDORID		0x104d
#define SONY_S5_SUBVENDOR	HDA_MODEL_CONSTRUCT(SONY, 0x81cc)
#define SONY_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(SONY, 0xffff)

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
#define FS_SI1848_SUBVENDOR	HDA_MODEL_CONSTRUCT(FS, 0x10cd)
#define FS_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(FS, 0xffff)

/* Fujitsu Limited */
#define FL_VENDORID		0x10cf
#define FL_S7020D_SUBVENDOR	HDA_MODEL_CONSTRUCT(FL, 0x1326)
#define FL_U1010_SUBVENDOR	HDA_MODEL_CONSTRUCT(FL, 0x142d)
#define FL_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(FL, 0xffff)

/* Toshiba */
#define TOSHIBA_VENDORID	0x1179
#define TOSHIBA_U200_SUBVENDOR	HDA_MODEL_CONSTRUCT(TOSHIBA, 0x0001)
#define TOSHIBA_A135_SUBVENDOR	HDA_MODEL_CONSTRUCT(TOSHIBA, 0xff01)
#define TOSHIBA_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(TOSHIBA, 0xffff)

/* Micro-Star International (MSI) */
#define MSI_VENDORID		0x1462
#define MSI_MS1034_SUBVENDOR	HDA_MODEL_CONSTRUCT(MSI, 0x0349)
#define MSI_MS034A_SUBVENDOR	HDA_MODEL_CONSTRUCT(MSI, 0x034a)
#define MSI_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(MSI, 0xffff)

/* Giga-Byte Technology */
#define GB_VENDORID		0x1458
#define GB_G33S2H_SUBVENDOR	HDA_MODEL_CONSTRUCT(GB, 0xa022)
#define GP_ALL_SUBVENDOR	HDA_MODEL_CONSTRUCT(GB, 0xffff)

/* Uniwill ? */
#define UNIWILL_VENDORID	0x1584
#define UNIWILL_9075_SUBVENDOR	HDA_MODEL_CONSTRUCT(UNIWILL, 0x9075)
#define UNIWILL_9080_SUBVENDOR	HDA_MODEL_CONSTRUCT(UNIWILL, 0x9080)


/* Misc constants.. */
#define HDA_AMP_VOL_DEFAULT	(-1)
#define HDA_AMP_MUTE_DEFAULT	(0xffffffff)
#define HDA_AMP_MUTE_NONE	(0)
#define HDA_AMP_MUTE_LEFT	(1 << 0)
#define HDA_AMP_MUTE_RIGHT	(1 << 1)
#define HDA_AMP_MUTE_ALL	(HDA_AMP_MUTE_LEFT | HDA_AMP_MUTE_RIGHT)

#define HDA_AMP_LEFT_MUTED(v)	((v) & (HDA_AMP_MUTE_LEFT))
#define HDA_AMP_RIGHT_MUTED(v)	(((v) & HDA_AMP_MUTE_RIGHT) >> 1)

#define HDA_ADC_MONITOR		(1 << 0)

#define HDA_CTL_OUT		1
#define HDA_CTL_IN		2

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
#define HDA_QUIRK_SENSEINV	(1 << 14)

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

#if __FreeBSD_version < 600000
#define taskqueue_drain(...)
#endif

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
	{ "senseinv", HDA_QUIRK_SENSEINV },
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

MALLOC_DEFINE(M_HDAC, "hdac", "High Definition Audio Controller");

const char *HDA_COLORS[16] = {"Unknown", "Black", "Grey", "Blue", "Green", "Red",
    "Orange", "Yellow", "Purple", "Pink", "Res.A", "Res.B", "Res.C", "Res.D",
    "White", "Other"};

const char *HDA_DEVS[16] = {"Line-out", "Speaker", "Headphones", "CD",
    "SPDIF-out", "Digital-out", "Modem-line", "Modem-handset", "Line-in",
    "AUX", "Mic", "Telephony", "SPDIF-in", "Digital-in", "Res.E", "Other"};

const char *HDA_CONNS[4] = {"Jack", "None", "Fixed", "Both"};

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
	{ HDA_INTEL_63XXESB, "Intel 631x/632xESB" },
	{ HDA_INTEL_82801G,  "Intel 82801G" },
	{ HDA_INTEL_82801H,  "Intel 82801H" },
	{ HDA_INTEL_82801I,  "Intel 82801I" },
	{ HDA_NVIDIA_MCP51,  "NVidia MCP51" },
	{ HDA_NVIDIA_MCP55,  "NVidia MCP55" },
	{ HDA_NVIDIA_MCP61_1, "NVidia MCP61" },
	{ HDA_NVIDIA_MCP61_2, "NVidia MCP61" },
	{ HDA_NVIDIA_MCP65_1, "NVidia MCP65" },
	{ HDA_NVIDIA_MCP65_2, "NVidia MCP65" },
	{ HDA_NVIDIA_MCP67_1, "NVidia MCP67" },
	{ HDA_NVIDIA_MCP67_2, "NVidia MCP67" },
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
#define HDA_CODEC_ALC267	HDA_CODEC_CONSTRUCT(REALTEK, 0x0267)
#define HDA_CODEC_ALC268	HDA_CODEC_CONSTRUCT(REALTEK, 0x0268)
#define HDA_CODEC_ALC269	HDA_CODEC_CONSTRUCT(REALTEK, 0x0269)
#define HDA_CODEC_ALC272	HDA_CODEC_CONSTRUCT(REALTEK, 0x0272)
#define HDA_CODEC_ALC660	HDA_CODEC_CONSTRUCT(REALTEK, 0x0660)
#define HDA_CODEC_ALC662	HDA_CODEC_CONSTRUCT(REALTEK, 0x0662)
#define HDA_CODEC_ALC663	HDA_CODEC_CONSTRUCT(REALTEK, 0x0663)
#define HDA_CODEC_ALC861	HDA_CODEC_CONSTRUCT(REALTEK, 0x0861)
#define HDA_CODEC_ALC861VD	HDA_CODEC_CONSTRUCT(REALTEK, 0x0862)
#define HDA_CODEC_ALC880	HDA_CODEC_CONSTRUCT(REALTEK, 0x0880)
#define HDA_CODEC_ALC882	HDA_CODEC_CONSTRUCT(REALTEK, 0x0882)
#define HDA_CODEC_ALC883	HDA_CODEC_CONSTRUCT(REALTEK, 0x0883)
#define HDA_CODEC_ALC885	HDA_CODEC_CONSTRUCT(REALTEK, 0x0885)
#define HDA_CODEC_ALC888	HDA_CODEC_CONSTRUCT(REALTEK, 0x0888)
#define HDA_CODEC_ALC889	HDA_CODEC_CONSTRUCT(REALTEK, 0x0889)
#define HDA_CODEC_ALCXXXX	HDA_CODEC_CONSTRUCT(REALTEK, 0xffff)

/* Analog Devices */
#define ANALOGDEVICES_VENDORID	0x11d4
#define HDA_CODEC_AD1981HD	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1981)
#define HDA_CODEC_AD1983	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1983)
#define HDA_CODEC_AD1984	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1984)
#define HDA_CODEC_AD1986A	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1986)
#define HDA_CODEC_AD1988	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x1988)
#define HDA_CODEC_AD1988B	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0x198b)
#define HDA_CODEC_ADXXXX	HDA_CODEC_CONSTRUCT(ANALOGDEVICES, 0xffff)

/* CMedia */
#define CMEDIA_VENDORID		0x434d
#define HDA_CODEC_CMI9880	HDA_CODEC_CONSTRUCT(CMEDIA, 0x4980)
#define HDA_CODEC_CMIXXXX	HDA_CODEC_CONSTRUCT(CMEDIA, 0xffff)

/* Sigmatel */
#define SIGMATEL_VENDORID	0x8384
#define HDA_CODEC_STAC9230X	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7612)
#define HDA_CODEC_STAC9230D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7613)
#define HDA_CODEC_STAC9229X	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7614)
#define HDA_CODEC_STAC9229D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7615)
#define HDA_CODEC_STAC9228X	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7616)
#define HDA_CODEC_STAC9228D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7617)
#define HDA_CODEC_STAC9227X	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7618)
#define HDA_CODEC_STAC9227D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7619)
#define HDA_CODEC_STAC9271D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7627)
#define HDA_CODEC_STAC9872AK	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7662)
#define HDA_CODEC_STAC9221	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7680)
#define HDA_CODEC_STAC922XD	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7681)
#define HDA_CODEC_STAC9221D	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7683)
#define HDA_CODEC_STAC9220	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x7690)
#define HDA_CODEC_STAC9205	HDA_CODEC_CONSTRUCT(SIGMATEL, 0x76a0)
#define HDA_CODEC_STACXXXX	HDA_CODEC_CONSTRUCT(SIGMATEL, 0xffff)

/* Silicon Image */
#define SII_VENDORID	0x1095
#define HDA_CODEC_SIIXXXX	HDA_CODEC_CONSTRUCT(SII, 0xffff)

/* Lucent/Agere */
#define AGERE_VENDORID	0x11c1
#define HDA_CODEC_AGEREXXXX	HDA_CODEC_CONSTRUCT(AGERE, 0xffff)

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

/* ATI */
#define HDA_CODEC_ATIXXXX	HDA_CODEC_CONSTRUCT(ATI, 0xffff)

/* NVIDIA */
#define HDA_CODEC_NVIDIAXXXX	HDA_CODEC_CONSTRUCT(NVIDIA, 0xffff)

/* Codecs */
static const struct {
	uint32_t id;
	char *name;
} hdac_codecs[] = {
	{ HDA_CODEC_ALC260,    "Realtek ALC260" },
	{ HDA_CODEC_ALC262,    "Realtek ALC262" },
	{ HDA_CODEC_ALC267,    "Realtek ALC267" },
	{ HDA_CODEC_ALC268,    "Realtek ALC268" },
	{ HDA_CODEC_ALC269,    "Realtek ALC269" },
	{ HDA_CODEC_ALC272,    "Realtek ALC272" },
	{ HDA_CODEC_ALC660,    "Realtek ALC660" },
	{ HDA_CODEC_ALC662,    "Realtek ALC662" },
	{ HDA_CODEC_ALC663,    "Realtek ALC663" },
	{ HDA_CODEC_ALC861,    "Realtek ALC861" },
	{ HDA_CODEC_ALC861VD,  "Realtek ALC861-VD" },
	{ HDA_CODEC_ALC880,    "Realtek ALC880" },
	{ HDA_CODEC_ALC882,    "Realtek ALC882" },
	{ HDA_CODEC_ALC883,    "Realtek ALC883" },
	{ HDA_CODEC_ALC885,    "Realtek ALC885" },
	{ HDA_CODEC_ALC888,    "Realtek ALC888" },
	{ HDA_CODEC_ALC889,    "Realtek ALC889" },
	{ HDA_CODEC_AD1981HD,  "Analog Devices AD1981HD" },
	{ HDA_CODEC_AD1983,    "Analog Devices AD1983" },
	{ HDA_CODEC_AD1984,    "Analog Devices AD1984" },
	{ HDA_CODEC_AD1986A,   "Analog Devices AD1986A" },
	{ HDA_CODEC_AD1988,    "Analog Devices AD1988" },
	{ HDA_CODEC_AD1988B,   "Analog Devices AD1988B" },
	{ HDA_CODEC_CMI9880,   "CMedia CMI9880" },
	{ HDA_CODEC_STAC9221,  "Sigmatel STAC9221" },
	{ HDA_CODEC_STAC9221D, "Sigmatel STAC9221D" },
	{ HDA_CODEC_STAC9220,  "Sigmatel STAC9220" },
	{ HDA_CODEC_STAC922XD, "Sigmatel STAC9220D/9223D" },
	{ HDA_CODEC_STAC9230X, "Sigmatel STAC9230X" },
	{ HDA_CODEC_STAC9230D, "Sigmatel STAC9230D" },
	{ HDA_CODEC_STAC9229X, "Sigmatel STAC9229X" },
	{ HDA_CODEC_STAC9229D, "Sigmatel STAC9229D" },
	{ HDA_CODEC_STAC9228X, "Sigmatel STAC9228X" },
	{ HDA_CODEC_STAC9228D, "Sigmatel STAC9228D" },
	{ HDA_CODEC_STAC9227X, "Sigmatel STAC9227X" },
	{ HDA_CODEC_STAC9227D, "Sigmatel STAC9227D" },
	{ HDA_CODEC_STAC9271D, "Sigmatel STAC9271D" },
	{ HDA_CODEC_STAC9205,  "Sigmatel STAC9205" },
	{ HDA_CODEC_STAC9872AK,"Sigmatel STAC9872AK" },
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
	{ HDA_CODEC_SIIXXXX,   "Silicon Image (Unknown)" },
	{ HDA_CODEC_AGEREXXXX, "Lucent/Agere Systems (Unknown)" },
	{ HDA_CODEC_CXXXXX,    "Conexant (Unknown)" },
	{ HDA_CODEC_VTXXXX,    "VIA (Unknown)" },
	{ HDA_CODEC_ATIXXXX,   "ATI (Unknown)" },
	{ HDA_CODEC_NVIDIAXXXX,"NVidia (Unknown)" },
};
#define HDAC_CODECS_LEN	(sizeof(hdac_codecs) / sizeof(hdac_codecs[0]))


/****************************************************************************
 * Function prototypes
 ****************************************************************************/
static void	hdac_intr_handler(void *);
static int	hdac_reset(struct hdac_softc *, int);
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
static void	hdac_probe_codec(struct hdac_codec *);
static void	hdac_probe_function(struct hdac_codec *, nid_t);
static int	hdac_pcmchannel_setup(struct hdac_chan *);

static void	hdac_attach2(void *);

static uint32_t	hdac_command_sendone_internal(struct hdac_softc *,
							uint32_t, int);
static void	hdac_command_send_internal(struct hdac_softc *,
					struct hdac_command_list *, int);

static int	hdac_probe(device_t);
static int	hdac_attach(device_t);
static int	hdac_detach(device_t);
static int	hdac_suspend(device_t);
static int	hdac_resume(device_t);
static void	hdac_widget_connection_select(struct hdac_widget *, uint8_t);
static void	hdac_audio_ctl_amp_set(struct hdac_audio_ctl *,
						uint32_t, int, int);
static struct	hdac_audio_ctl *hdac_audio_ctl_amp_get(struct hdac_devinfo *,
							nid_t, int, int, int);
static void	hdac_audio_ctl_amp_set_internal(struct hdac_softc *,
				nid_t, nid_t, int, int, int, int, int, int);
static struct	hdac_widget *hdac_widget_get(struct hdac_devinfo *, nid_t);

static int	hdac_rirb_flush(struct hdac_softc *sc);
static int	hdac_unsolq_flush(struct hdac_softc *sc);

static void	hdac_dump_pin_config(struct hdac_widget *w, uint32_t conf);

#define hdac_command(a1, a2, a3)	\
		hdac_command_sendone_internal(a1, a2, a3)

#define hdac_codec_id(c)							\
		((uint32_t)((c == NULL) ? 0x00000000 :	\
		((((uint32_t)(c)->vendor_id & 0x0000ffff) << 16) |	\
		((uint32_t)(c)->device_id & 0x0000ffff))))

static char *
hdac_codec_name(struct hdac_codec *codec)
{
	uint32_t id;
	int i;

	id = hdac_codec_id(codec);

	for (i = 0; i < HDAC_CODECS_LEN; i++) {
		if (HDA_DEV_MATCH(hdac_codecs[i].id, id))
			return (hdac_codecs[i].name);
	}

	return ((id == 0x00000000) ? "NULL Codec" : "Unknown Codec");
}

static char *
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
	return (buf);
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
hdac_audio_ctl_amp_get(struct hdac_devinfo *devinfo, nid_t nid, int dir,
						int index, int cnt)
{
	struct hdac_audio_ctl *ctl;
	int i, found = 0;

	if (devinfo == NULL || devinfo->function.audio.ctl == NULL)
		return (NULL);

	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0)
			continue;
		if (ctl->widget->nid != nid)
			continue;
		if (dir && ctl->ndir != dir)
			continue;
		if (index >= 0 && ctl->ndir == HDA_CTL_IN &&
		    ctl->dir == ctl->ndir && ctl->index != index)
			continue;
		found++;
		if (found == cnt || cnt <= 0)
			return (ctl);
	}

	return (NULL);
}

/*
 * Jack detection (Speaker/HP redirection) event handler.
 */
static void
hdac_hp_switch_handler(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as;
	struct hdac_softc *sc;
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	uint32_t val, res;
	int i, j;
	nid_t cad;

	if (devinfo == NULL || devinfo->codec == NULL ||
	    devinfo->codec->sc == NULL)
		return;

	sc = devinfo->codec->sc;
	cad = devinfo->codec->cad;
	as = devinfo->function.audio.as;
	for (i = 0; i < devinfo->function.audio.ascnt; i++) {
		if (as[i].hpredir < 0)
			continue;
	
		w = hdac_widget_get(devinfo, as[i].pins[15]);
		if (w == NULL || w->enable == 0 || w->type !=
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;

		res = hdac_command(sc,
		    HDA_CMD_GET_PIN_SENSE(cad, as[i].pins[15]), cad);

		HDA_BOOTVERBOSE(
			device_printf(sc->dev,
			    "Pin sense: nid=%d res=0x%08x\n",
			    as[i].pins[15], res);
		);

		res = HDA_CMD_GET_PIN_SENSE_PRESENCE_DETECT(res);
		if (devinfo->function.audio.quirks & HDA_QUIRK_SENSEINV)
			res ^= 1;

		/* (Un)Mute headphone pin. */
		ctl = hdac_audio_ctl_amp_get(devinfo,
		    as[i].pins[15], HDA_CTL_IN, -1, 1);
		if (ctl != NULL && ctl->mute) {
			/* If pin has muter - use it. */
			val = (res != 0) ? 0 : 1;
			if (val != ctl->forcemute) {
				ctl->forcemute = val;
				hdac_audio_ctl_amp_set(ctl,
				    HDA_AMP_MUTE_DEFAULT,
				    HDA_AMP_VOL_DEFAULT, HDA_AMP_VOL_DEFAULT);
			}
		} else {
			/* If there is no muter - disable pin output. */
			w = hdac_widget_get(devinfo, as[i].pins[15]);
			if (w != NULL && w->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) {
				if (res != 0)
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
		}
		/* (Un)Mute other pins. */
		for (j = 0; j < 15; j++) {
			if (as[i].pins[j] <= 0)
				continue;
			ctl = hdac_audio_ctl_amp_get(devinfo,
			    as[i].pins[j], HDA_CTL_IN, -1, 1);
			if (ctl != NULL && ctl->mute) {
				/* If pin has muter - use it. */
				val = (res != 0) ? 1 : 0;
				if (val == ctl->forcemute)
					continue;
				ctl->forcemute = val;
				hdac_audio_ctl_amp_set(ctl,
				    HDA_AMP_MUTE_DEFAULT,
				    HDA_AMP_VOL_DEFAULT, HDA_AMP_VOL_DEFAULT);
				continue;
			}
			/* If there is no muter - disable pin output. */
			w = hdac_widget_get(devinfo, as[i].pins[j]);
			if (w != NULL && w->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) {
				if (res != 0)
					val = w->wclass.pin.ctrl &
					    ~HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				else
					val = w->wclass.pin.ctrl |
					    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;
				if (val != w->wclass.pin.ctrl) {
					w->wclass.pin.ctrl = val;
					hdac_command(sc,
					    HDA_CMD_SET_PIN_WIDGET_CTRL(cad,
					    w->nid, w->wclass.pin.ctrl), cad);
				}
			}
		}
	}
}

/*
 * Callback for poll based jack detection.
 */
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

/*
 * Jack detection initializer.
 */
static void
hdac_hp_switch_init(struct hdac_devinfo *devinfo)
{
        struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_audio_as *as = devinfo->function.audio.as;
        struct hdac_widget *w;
        uint32_t id;
        int i, enable = 0, poll = 0;
        nid_t cad;
							
	id = hdac_codec_id(devinfo->codec);
	cad = devinfo->codec->cad;
	for (i = 0; i < devinfo->function.audio.ascnt; i++) {
		if (as[i].hpredir < 0)
			continue;
	
		w = hdac_widget_get(devinfo, as[i].pins[15]);
		if (w == NULL || w->enable == 0 || w->type !=
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if (HDA_PARAM_PIN_CAP_PRESENCE_DETECT_CAP(w->wclass.pin.cap) == 0 ||
		    (HDA_CONFIG_DEFAULTCONF_MISC(w->wclass.pin.config) & 1) != 0) {
			device_printf(sc->dev,
			    "No jack detection support at pin %d\n",
			    as[i].pins[15]);
			continue;
		}
		enable = 1;
		if (HDA_PARAM_AUDIO_WIDGET_CAP_UNSOL_CAP(w->param.widget_cap)) {
			hdac_command(sc,
			    HDA_CMD_SET_UNSOLICITED_RESPONSE(cad, w->nid,
			    HDA_CMD_SET_UNSOLICITED_RESPONSE_ENABLE |
			    HDAC_UNSOLTAG_EVENT_HP), cad);
		} else
			poll = 1;
		HDA_BOOTVERBOSE(
			device_printf(sc->dev,
			    "Enabling headphone/speaker "
			    "audio routing switching:\n");
			device_printf(sc->dev, "\tas=%d sense nid=%d [%s]\n",
			    i, w->nid, (poll != 0) ? "POLL" : "UNSOL");
		);
	}
	if (enable) {
		hdac_hp_switch_handler(devinfo);
		if (poll) {
			callout_reset(&sc->poll_jack, 1,
			    hdac_jack_poll_callback, devinfo);
		}
	}
}

/*
 * Unsolicited messages handler.
 */
static void
hdac_unsolicited_handler(struct hdac_codec *codec, uint32_t tag)
{
	struct hdac_softc *sc;
	struct hdac_devinfo *devinfo = NULL;
	int i;

	if (codec == NULL || codec->sc == NULL)
		return;

	sc = codec->sc;

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "Unsol Tag: 0x%08x\n", tag);
	);

	for (i = 0; i < codec->num_fgs; i++) {
		if (codec->fgs[i].node_type ==
		    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO) {
			devinfo = &codec->fgs[i];
			break;
		}
	}

	if (devinfo == NULL)
		return;

	switch (tag) {
	case HDAC_UNSOLTAG_EVENT_HP:
		hdac_hp_switch_handler(devinfo);
		break;
	default:
		device_printf(sc->dev, "Unknown unsol tag: 0x%08x!\n", tag);
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

	if (!(ch->flags & HDAC_CHN_RUNNING))
		return (0);

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	res = HDAC_READ_1(&sc->mem, ch->off + HDAC_SDSTS);
#endif

	/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
	HDA_BOOTVERBOSE(
		if (res & (HDAC_SDSTS_DESE | HDAC_SDSTS_FIFOE))
			device_printf(ch->pdevinfo->dev,
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
	uint32_t trigger;
	int i;

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

	trigger = 0;

	/* Was this a controller interrupt? */
	if (HDA_FLAG_MATCH(intsts, HDAC_INTSTS_CIS)) {
		rirb_base = (struct hdac_rirb *)sc->rirb_dma.dma_vaddr;
		rirbsts = HDAC_READ_1(&sc->mem, HDAC_RIRBSTS);
		/* Get as many responses that we can */
		while (HDA_FLAG_MATCH(rirbsts, HDAC_RIRBSTS_RINTFL)) {
			HDAC_WRITE_1(&sc->mem,
			    HDAC_RIRBSTS, HDAC_RIRBSTS_RINTFL);
			if (hdac_rirb_flush(sc) != 0)
				trigger |= HDAC_TRIGGER_UNSOL;
			rirbsts = HDAC_READ_1(&sc->mem, HDAC_RIRBSTS);
		}
		/* XXX to be removed */
		/* Clear interrupt and exit */
#ifdef HDAC_INTR_EXTRA
		HDAC_WRITE_4(&sc->mem, HDAC_INTSTS, HDAC_INTSTS_CIS);
#endif
	}

	if (intsts & HDAC_INTSTS_SIS_MASK) {
		for (i = 0; i < sc->num_chans; i++) {
			if ((intsts & (1 << (sc->chans[i].off >> 5))) &&
			    hdac_stream_intr(sc, &sc->chans[i]) != 0)
				trigger |= (1 << i);
		}
		/* XXX to be removed */
#ifdef HDAC_INTR_EXTRA
		HDAC_WRITE_4(&sc->mem, HDAC_INTSTS, intsts &
		    HDAC_INTSTS_SIS_MASK);
#endif
	}

	hdac_unlock(sc);

	for (i = 0; i < sc->num_chans; i++) {
		if (trigger & (1 << i))
			chn_intr(sc->chans[i].c);
	}
	if (trigger & HDAC_TRIGGER_UNSOL)
		taskqueue_enqueue(taskqueue_thread, &sc->unsolq_task);
}

/****************************************************************************
 * int hdac_reset(hdac_softc *, int)
 *
 * Reset the hdac to a quiescent and known state.
 ****************************************************************************/
static int
hdac_reset(struct hdac_softc *sc, int wakeup)
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
	
	/* If wakeup is not requested - leave the controller in reset state. */
	if (!wakeup)
		return (0);
	
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

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "    CORB size: %d\n", sc->corb_size);
		device_printf(sc->dev, "    RIRB size: %d\n", sc->rirb_size);
		device_printf(sc->dev, "      Streams: ISS=%d OSS=%d BSS=%d\n",
		    sc->num_iss, sc->num_oss, sc->num_bss);
	);

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
	    ((sc->flags & HDAC_F_DMA_NOCACHE) ? BUS_DMA_NOCACHE : 0),
	    &dma->dma_map);
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

#ifdef HDAC_MSI_ENABLED
	if ((sc->flags & HDAC_F_MSI) &&
	    (result = pci_msi_count(sc->dev)) == 1 &&
	    pci_alloc_msi(sc->dev, &result) == 0)
		irq->irq_rid = 0x1;
	else
#endif
		sc->flags &= ~HDAC_F_MSI;

	irq->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ,
	    &irq->irq_rid, RF_SHAREABLE | RF_ACTIVE);
	if (irq->irq_res == NULL) {
		device_printf(sc->dev, "%s: Unable to allocate irq\n",
		    __func__);
		goto hdac_irq_alloc_fail;
	}
	result = bus_setup_intr(sc->dev, irq->irq_res, INTR_MPSAFE | INTR_TYPE_AV,
	    NULL, hdac_intr_handler, sc, &irq->irq_handle);
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
#ifdef HDAC_MSI_ENABLED
	if ((sc->flags & HDAC_F_MSI) && irq->irq_rid == 0x1)
		pci_release_msi(sc->dev);
#endif
	irq->irq_handle = NULL;
	irq->irq_res = NULL;
	irq->irq_rid = 0x0;
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
 * void hdac_scan_codecs(struct hdac_softc *, int)
 *
 * Scan the bus for available codecs, starting with num.
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
			hdac_probe_codec(codec);
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
static void
hdac_probe_codec(struct hdac_codec *codec)
{
	struct hdac_softc *sc = codec->sc;
	uint32_t vendorid, revisionid, subnode;
	int startnode;
	int endnode;
	int i;
	nid_t cad = codec->cad;

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "Probing codec %d...\n", cad);
	);
	vendorid = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, 0x0, HDA_PARAM_VENDOR_ID),
	    cad);
	revisionid = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, 0x0, HDA_PARAM_REVISION_ID),
	    cad);
	codec->vendor_id = HDA_PARAM_VENDOR_ID_VENDOR_ID(vendorid);
	codec->device_id = HDA_PARAM_VENDOR_ID_DEVICE_ID(vendorid);
	codec->revision_id = HDA_PARAM_REVISION_ID_REVISION_ID(revisionid);
	codec->stepping_id = HDA_PARAM_REVISION_ID_STEPPING_ID(revisionid);

	if (vendorid == HDAC_INVALID && revisionid == HDAC_INVALID) {
		device_printf(sc->dev, "Codec #%d is not responding!"
		    " Probing aborted.\n", cad);
		return;
	}

	device_printf(sc->dev, "<HDA Codec #%d: %s>\n",
	    cad, hdac_codec_name(codec));
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "<HDA Codec ID: 0x%08x>\n",
		    hdac_codec_id(codec));
		device_printf(sc->dev, "       Vendor: 0x%04x\n",
		    codec->vendor_id);
		device_printf(sc->dev, "       Device: 0x%04x\n",
		    codec->device_id);
		device_printf(sc->dev, "     Revision: 0x%02x\n",
		    codec->revision_id);
		device_printf(sc->dev, "     Stepping: 0x%02x\n",
		    codec->stepping_id);
		device_printf(sc->dev, "PCI Subvendor: 0x%08x\n",
		    sc->pci_subvendor);
	);
	subnode = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, 0x0, HDA_PARAM_SUB_NODE_COUNT),
	    cad);
	startnode = HDA_PARAM_SUB_NODE_COUNT_START(subnode);
	endnode = startnode + HDA_PARAM_SUB_NODE_COUNT_TOTAL(subnode);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "\tstartnode=%d endnode=%d\n",
		    startnode, endnode);
	);
	
	codec->fgs = (struct hdac_devinfo *)malloc(sizeof(struct hdac_devinfo) *
	    (endnode - startnode), M_HDAC, M_NOWAIT | M_ZERO);
	if (codec->fgs == NULL) {
		device_printf(sc->dev, "%s: Unable to allocate function groups\n",
		    __func__);
		return;
	}

	for (i = startnode; i < endnode; i++)
		hdac_probe_function(codec, i);
	return;
}

/*
 * Probe codec function and add it to the list.
 */
static void
hdac_probe_function(struct hdac_codec *codec, nid_t nid)
{
	struct hdac_softc *sc = codec->sc;
	struct hdac_devinfo *devinfo = &codec->fgs[codec->num_fgs];
	uint32_t fctgrptype;
	uint32_t res;
	nid_t cad = codec->cad;

	fctgrptype = HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE(hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_FCT_GRP_TYPE), cad));

	devinfo->nid = nid;
	devinfo->node_type = fctgrptype;
	devinfo->codec = codec;

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad , nid, HDA_PARAM_SUB_NODE_COUNT), cad);

	devinfo->nodecnt = HDA_PARAM_SUB_NODE_COUNT_TOTAL(res);
	devinfo->startnode = HDA_PARAM_SUB_NODE_COUNT_START(res);
	devinfo->endnode = devinfo->startnode + devinfo->nodecnt;

	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "\tFound %s FG nid=%d startnode=%d endnode=%d total=%d\n",
		    (fctgrptype == HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO) ? "audio":
		    (fctgrptype == HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_MODEM) ? "modem":
		    "unknown", nid, devinfo->startnode, devinfo->endnode,
		    devinfo->nodecnt);
	);

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

	codec->num_fgs++;
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
					    "GHOST: nid=%d j=%d "
					    "entnum=%d index=%d res=0x%08x\n",
					    nid, j, entnum, i, res);
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
					    "Adding %d (nid=%d): "
					    "Max connection reached! max=%d\n",
					    addcnid, nid, max + 1);
					goto getconns_out;
				}
				w->connsenable[w->nconns] = 1;
				w->conns[w->nconns++] = addcnid++;
			}
			prevcnid = cnid;
		}
	}

getconns_out:
	return;
}

static uint32_t
hdac_widget_pin_patch(uint32_t config, const char *str)
{
	char buf[256];
	char *key, *value, *rest, *bad;
	int ival, i;

	strlcpy(buf, str, sizeof(buf));
	rest = buf;
	while ((key = strsep(&rest, "=")) != NULL) {
		value = strsep(&rest, " \t");
		if (value == NULL)
			break;
		ival = strtol(value, &bad, 10);
		if (strcmp(key, "seq") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_SEQUENCE_MASK;
			config |= ((ival << HDA_CONFIG_DEFAULTCONF_SEQUENCE_SHIFT) &
			    HDA_CONFIG_DEFAULTCONF_SEQUENCE_MASK);
		} else if (strcmp(key, "as") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_ASSOCIATION_MASK;
			config |= ((ival << HDA_CONFIG_DEFAULTCONF_ASSOCIATION_SHIFT) &
			    HDA_CONFIG_DEFAULTCONF_ASSOCIATION_MASK);
		} else if (strcmp(key, "misc") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_MISC_MASK;
			config |= ((ival << HDA_CONFIG_DEFAULTCONF_MISC_SHIFT) &
			    HDA_CONFIG_DEFAULTCONF_MISC_MASK);
		} else if (strcmp(key, "color") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_COLOR_MASK;
			if (bad[0] == 0) {
				config |= ((ival << HDA_CONFIG_DEFAULTCONF_COLOR_SHIFT) &
				    HDA_CONFIG_DEFAULTCONF_COLOR_MASK);
			};
			for (i = 0; i < 16; i++) {
				if (strcasecmp(HDA_COLORS[i], value) == 0) {
					config |= (i << HDA_CONFIG_DEFAULTCONF_COLOR_SHIFT);
					break;
				}
			}
		} else if (strcmp(key, "ctype") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_CONNECTION_TYPE_MASK;
			config |= ((ival << HDA_CONFIG_DEFAULTCONF_CONNECTION_TYPE_SHIFT) &
			    HDA_CONFIG_DEFAULTCONF_CONNECTION_TYPE_MASK);
		} else if (strcmp(key, "device") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			if (bad[0] == 0) {
				config |= ((ival << HDA_CONFIG_DEFAULTCONF_DEVICE_SHIFT) &
				    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK);
				continue;
			};
			for (i = 0; i < 16; i++) {
				if (strcasecmp(HDA_DEVS[i], value) == 0) {
					config |= (i << HDA_CONFIG_DEFAULTCONF_DEVICE_SHIFT);
					break;
				}
			}
		} else if (strcmp(key, "loc") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_LOCATION_MASK;
			config |= ((ival << HDA_CONFIG_DEFAULTCONF_LOCATION_SHIFT) &
			    HDA_CONFIG_DEFAULTCONF_LOCATION_MASK);
		} else if (strcmp(key, "conn") == 0) {
			config &= ~HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK;
			if (bad[0] == 0) {
				config |= ((ival << HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_SHIFT) &
				    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
				continue;
			};
			for (i = 0; i < 4; i++) {
				if (strcasecmp(HDA_CONNS[i], value) == 0) {
					config |= (i << HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_SHIFT);
					break;
				}
			}
		}
	}
	return (config);
}

static uint32_t
hdac_widget_pin_getconfig(struct hdac_widget *w)
{
	struct hdac_softc *sc;
	uint32_t config, orig, id;
	nid_t cad, nid;
	char buf[32];
	const char *res = NULL, *patch = NULL;

	sc = w->devinfo->codec->sc;
	cad = w->devinfo->codec->cad;
	nid = w->nid;
	id = hdac_codec_id(w->devinfo->codec);

	config = hdac_command(sc,
	    HDA_CMD_GET_CONFIGURATION_DEFAULT(cad, nid),
	    cad);
	orig = config;

	HDA_BOOTVERBOSE(
		hdac_dump_pin_config(w, orig);
	);

	/* XXX: Old patches require complete review.
	 * Now they may create more problem then solve due to
	 * incorrect associations.
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
		}
	} else if (id == HDA_CODEC_ALC883 &&
	    (sc->pci_subvendor == MSI_MS034A_SUBVENDOR ||
	    HDA_DEV_MATCH(ACER_ALL_SUBVENDOR, sc->pci_subvendor))) {
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
		}
	} else if (id == HDA_CODEC_CXWAIKIKI && sc->pci_subvendor ==
	    HP_DV5000_SUBVENDOR) {
		switch (nid) {
		case 20:
		case 21:
			config &= ~HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK;
			config |= HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_NONE;
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
		case 12:
		case 14:
		case 16:
		case 31:
		case 32:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED);
			break;
		case 15:
			config &= ~(HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK);
			config |= (HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT |
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK);
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
		}
	}

	/* New patches */
	if (id == HDA_CODEC_AD1986A &&
	    (sc->pci_subvendor == ASUS_M2NPVMX_SUBVENDOR ||
	    sc->pci_subvendor == ASUS_A8NVMCSM_SUBVENDOR)) {
		switch (nid) {
		case 28: /* 5.1 out => 2.0 out + 2 inputs */
			patch = "device=Line-in as=8 seq=1";
			break;
		case 29:
			patch = "device=Mic as=8 seq=2";
			break;
		case 31: /* Lot of inputs configured with as=15 and unusable */
			patch = "as=8 seq=3";
			break;
		case 32:
			patch = "as=8 seq=4";
			break;
		case 34:
			patch = "as=8 seq=5";
			break;
		case 36:
			patch = "as=8 seq=6";
			break;
		}
	} else if (id == HDA_CODEC_ALC260 &&
	    HDA_DEV_MATCH(SONY_S5_SUBVENDOR, sc->pci_subvendor)) {
		switch (nid) {
		case 16:
			patch = "seq=15 device=Headphones";
			break;
		}
	} else if (id == HDA_CODEC_ALC268 &&
	    HDA_DEV_MATCH(ACER_ALL_SUBVENDOR, sc->pci_subvendor)) {
		switch (nid) {
		case 28:
			patch = "device=CD conn=fixed";
			break;
		}
	}

	if (patch != NULL)
		config = hdac_widget_pin_patch(config, patch);
	
	snprintf(buf, sizeof(buf), "cad%u.nid%u.config", cad, nid);
	if (resource_string_value(device_get_name(sc->dev),
	    device_get_unit(sc->dev), buf, &res) == 0) {
		if (strncmp(res, "0x", 2) == 0) {
			config = strtol(res + 2, NULL, 16);
		} else {
			config = hdac_widget_pin_patch(config, res);
		}
	}

	HDA_BOOTVERBOSE(
		if (config != orig)
			device_printf(sc->dev,
			    "Patching pin config nid=%u 0x%08x -> 0x%08x\n",
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
	id = hdac_codec_id(w->devinfo->codec);

	caps = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_PIN_CAP), cad);
	orig = caps;

	HDA_BOOTVERBOSE(
		if (caps != orig)
			device_printf(sc->dev,
			    "Patching pin caps nid=%u 0x%08x -> 0x%08x\n",
			    nid, orig, caps);
	);

	return (caps);
}

static void
hdac_widget_pin_parse(struct hdac_widget *w)
{
	struct hdac_softc *sc = w->devinfo->codec->sc;
	uint32_t config, pincap;
	const char *devstr, *connstr;
	nid_t cad = w->devinfo->codec->cad;
	nid_t nid = w->nid;

	config = hdac_widget_pin_getconfig(w);
	w->wclass.pin.config = config;

	pincap = hdac_widget_pin_getcaps(w);
	w->wclass.pin.cap = pincap;

	w->wclass.pin.ctrl = hdac_command(sc,
	    HDA_CMD_GET_PIN_WIDGET_CTRL(cad, nid), cad);

	if (HDA_PARAM_PIN_CAP_EAPD_CAP(pincap)) {
		w->param.eapdbtl = hdac_command(sc,
		    HDA_CMD_GET_EAPD_BTL_ENABLE(cad, nid), cad);
		w->param.eapdbtl &= 0x7;
		w->param.eapdbtl |= HDA_CMD_SET_EAPD_BTL_ENABLE_EAPD;
	} else
		w->param.eapdbtl = HDAC_INVALID;

	devstr = HDA_DEVS[(config & HDA_CONFIG_DEFAULTCONF_DEVICE_MASK) >>
	    HDA_CONFIG_DEFAULTCONF_DEVICE_SHIFT];

	connstr = HDA_CONNS[(config & HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK) >>
	    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_SHIFT];

	strlcat(w->name, ": ", sizeof(w->name));
	strlcat(w->name, devstr, sizeof(w->name));
	strlcat(w->name, " (", sizeof(w->name));
	strlcat(w->name, connstr, sizeof(w->name));
	strlcat(w->name, ")", sizeof(w->name));
}

static uint32_t
hdac_widget_getcaps(struct hdac_widget *w, int *waspin)
{
	struct hdac_softc *sc;
	uint32_t caps, orig, id;
	nid_t cad, nid, beeper = -1;

	sc = w->devinfo->codec->sc;
	cad = w->devinfo->codec->cad;
	nid = w->nid;
	id = hdac_codec_id(w->devinfo->codec);

	caps = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad, nid, HDA_PARAM_AUDIO_WIDGET_CAP),
	    cad);
	orig = caps;

	/* On some codecs beeper is an input pin, but it is not recordable
	   alone. Also most of BIOSes does not declare beeper pin.
	   Change beeper pin node type to beeper to help parser. */
	*waspin = 0;
	switch (id) {
	case HDA_CODEC_AD1988:
	case HDA_CODEC_AD1988B:
		beeper = 26;
		break;
	case HDA_CODEC_ALC260:
		beeper = 23;
		break;
	case HDA_CODEC_ALC262:
	case HDA_CODEC_ALC268:
	case HDA_CODEC_ALC880:
	case HDA_CODEC_ALC882:
	case HDA_CODEC_ALC883:
	case HDA_CODEC_ALC885:
	case HDA_CODEC_ALC888:
	case HDA_CODEC_ALC889:
		beeper = 29;
		break;
	}
	if (nid == beeper) {
		caps &= ~HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_MASK;
		caps |= HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET <<
		    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_SHIFT;
		*waspin = 1;
	}

	HDA_BOOTVERBOSE(
		if (caps != orig) {
			device_printf(sc->dev,
			    "Patching widget caps nid=%u 0x%08x -> 0x%08x\n",
			    nid, orig, caps);
		}
	);

	return (caps);
}

static void
hdac_widget_parse(struct hdac_widget *w)
{
	struct hdac_softc *sc = w->devinfo->codec->sc;
	uint32_t wcap, cap;
	char *typestr;
	nid_t cad = w->devinfo->codec->cad;
	nid_t nid = w->nid;

	wcap = hdac_widget_getcaps(w, &w->waspin);

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

	if (!(ch->flags & HDAC_CHN_RUNNING))
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

static void
hda_poll_callback(void *arg)
{
	struct hdac_softc *sc = arg;
	uint32_t trigger;
	int i, active = 0;

	if (sc == NULL)
		return;

	hdac_lock(sc);
	if (sc->polling == 0) {
		hdac_unlock(sc);
		return;
	}

	trigger = 0;
	for (i = 0; i < sc->num_chans; i++) {
		if ((sc->chans[i].flags & HDAC_CHN_RUNNING) == 0)
		    continue;
		active = 1;
		if (hda_poll_channel(&sc->chans[i]))
		    trigger |= (1 << i);
	}

	/* XXX */
	if (active)
		callout_reset(&sc->poll_hda, sc->poll_ticks,
		    hda_poll_callback, sc);

	hdac_unlock(sc);

	for (i = 0; i < sc->num_chans; i++) {
		if (trigger & (1 << i))
			chn_intr(sc->chans[i].c);
	}
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
	int ret;

	rirb_base = (struct hdac_rirb *)sc->rirb_dma.dma_vaddr;
	rirbwp = HDAC_READ_1(&sc->mem, HDAC_RIRBWP);
#if 0
	bus_dmamap_sync(sc->rirb_dma.dma_tag, sc->rirb_dma.dma_map,
	    BUS_DMASYNC_POSTREAD);
#endif

	ret = 0;

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
	if (hdac_rirb_flush(sc) != 0)
		hdac_unsolq_flush(sc);
	callout_reset(&sc->poll_hdac, sc->poll_ival, hdac_poll_callback, sc);
	hdac_unlock(sc);
}

static void
hdac_poll_reinit(struct hdac_softc *sc)
{
	int i, pollticks, min = 1000000;
	struct hdac_chan *ch;

	for (i = 0; i < sc->num_chans; i++) {
		if ((sc->chans[i].flags & HDAC_CHN_RUNNING) == 0)
			continue;
		ch = &sc->chans[i];
		pollticks = ((uint64_t)hz * ch->blksz) /
		    ((uint64_t)sndbuf_getbps(ch->b) *
		    sndbuf_getspd(ch->b));
		pollticks >>= 1;
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
		if (min > pollticks)
			min = pollticks;
	}
	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "%s: pollticks %d -> %d\n",
		    __func__, sc->poll_ticks, min);
	);
	sc->poll_ticks = min;
	if (min == 1000000)
		callout_stop(&sc->poll_hda);
	else
		callout_reset(&sc->poll_hda, 1, hda_poll_callback, sc);
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

	ch->flags &= ~HDAC_CHN_RUNNING;

	if (sc->polling != 0)
		hdac_poll_reinit(sc);

	ctl = HDAC_READ_4(&sc->mem, HDAC_INTCTL);
	ctl &= ~(1 << (ch->off >> 5));
	HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, ctl);
}

static void
hdac_stream_start(struct hdac_chan *ch)
{
	struct hdac_softc *sc = ch->devinfo->codec->sc;
	uint32_t ctl;

	ch->flags |= HDAC_CHN_RUNNING;

	if (sc->polling != 0)
		hdac_poll_reinit(sc);

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

	blksz = ch->blksz;
	blkcnt = ch->blkcnt;

	for (i = 0; i < blkcnt; i++, bdle++) {
		bdle->addrl = (uint32_t)addr;
		bdle->addrh = (uint32_t)(addr >> 32);
		bdle->len = blksz;
		bdle->ioc = 1;
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

	sc = ctl->widget->devinfo->codec->sc;
	cad = ctl->widget->devinfo->codec->cad;
	nid = ctl->widget->nid;

	/* Save new values if valid. */
	if (mute != HDA_AMP_MUTE_DEFAULT)
		ctl->muted = mute;
	if (left != HDA_AMP_VOL_DEFAULT)
		ctl->left = left;
	if (right != HDA_AMP_VOL_DEFAULT)
		ctl->right = right;
	/* Prepare effective values */
	if (ctl->forcemute) {
		lmute = 1;
		rmute = 1;
		left = 0;
		right = 0;
	} else {
		lmute = HDA_AMP_LEFT_MUTED(ctl->muted);
		rmute = HDA_AMP_RIGHT_MUTED(ctl->muted);
		left = ctl->left;
		right = ctl->right;
	}
	/* Apply effective values */
	if (ctl->dir & HDA_CTL_OUT)
		hdac_audio_ctl_amp_set_internal(sc, cad, nid, ctl->index,
		    lmute, rmute, left, right, 0);
	if (ctl->dir & HDA_CTL_IN)
    		hdac_audio_ctl_amp_set_internal(sc, cad, nid, ctl->index,
		    lmute, rmute, left, right, 1);
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
	struct hdac_pcm_devinfo *pdevinfo = data;
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_chan *ch;
	int i, ord = 0, chid;

	hdac_lock(sc);

	chid = (dir == PCMDIR_PLAY)?pdevinfo->play:pdevinfo->rec;
	ch = &sc->chans[chid];
	for (i = 0; i < sc->num_chans && i < chid; i++) {
		if (ch->dir == sc->chans[i].dir)
			ord++;
	}
	if (dir == PCMDIR_PLAY) {
		ch->off = (sc->num_iss + ord) << 5;
	} else {
		ch->off = ord << 5;
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
	ch->blksz = pdevinfo->chan_size / pdevinfo->chan_blkcnt;
	ch->blkcnt = pdevinfo->chan_blkcnt;
	hdac_unlock(sc);

	if (hdac_bdl_alloc(ch) != 0) {
		ch->blkcnt = 0;
		return (NULL);
	}

	if (sndbuf_alloc(ch->b, sc->chan_dmat,
	    (sc->flags & HDAC_F_DMA_NOCACHE) ? BUS_DMA_NOCACHE : 0,
	    pdevinfo->chan_size) != 0)
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
	struct hdac_audio_as *as = &ch->devinfo->function.audio.as[ch->as];
	struct hdac_widget *w;
	int i, chn, totalchn, c;
	nid_t cad = ch->devinfo->codec->cad;
	uint16_t fmt, dfmt;

	HDA_BOOTVERBOSE(
		device_printf(ch->pdevinfo->dev,
		    "PCMDIR_%s: Stream setup fmt=%08x speed=%d\n",
		    (ch->dir == PCMDIR_PLAY) ? "PLAY" : "REC",
		    ch->fmt, ch->spd);
	);
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

	if (ch->fmt & (AFMT_STEREO | AFMT_AC3)) {
		fmt |= 1;
		totalchn = 2;
	} else
		totalchn = 1;

	HDAC_WRITE_2(&sc->mem, ch->off + HDAC_SDFMT, fmt);
		
	dfmt = HDA_CMD_SET_DIGITAL_CONV_FMT1_DIGEN;
	if (ch->fmt & AFMT_AC3)
		dfmt |= HDA_CMD_SET_DIGITAL_CONV_FMT1_NAUDIO;

	chn = 0;
	for (i = 0; ch->io[i] != -1; i++) {
		w = hdac_widget_get(ch->devinfo, ch->io[i]);
		if (w == NULL)
			continue;

		if (as->hpredir >= 0 && i == as->pincnt)
			chn = 0;
		HDA_BOOTVERBOSE(
			device_printf(ch->pdevinfo->dev,
			    "PCMDIR_%s: Stream setup nid=%d: "
			    "fmt=0x%04x, dfmt=0x%04x\n",
			    (ch->dir == PCMDIR_PLAY) ? "PLAY" : "REC",
			    ch->io[i], fmt, dfmt);
		);
		hdac_command(sc,
		    HDA_CMD_SET_CONV_FMT(cad, ch->io[i], fmt), cad);
		if (HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap)) {
			hdac_command(sc,
			    HDA_CMD_SET_DIGITAL_CONV_FMT1(cad, ch->io[i], dfmt),
			    cad);
		}
		/* If HP redirection is enabled, but failed to use same
		   DAC make last DAC one to duplicate first one. */
		if (as->hpredir >= 0 && i == as->pincnt) {
			c = (ch->sid << 4);
		} else if (chn >= totalchn) {
			/* This is until OSS will support multichannel.
			   Should be: c = 0; to disable unused DAC */
			c = (ch->sid << 4);
		}else {
			c = (ch->sid << 4) | chn;
		}
		hdac_command(sc,
		    HDA_CMD_SET_CONV_STREAM_CHAN(cad, ch->io[i], c), cad);
		chn +=
		    HDA_PARAM_AUDIO_WIDGET_CAP_STEREO(w->param.widget_cap) ?
		    2 : 1;
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

	hdac_channel_setfragments(obj, data, blksz, ch->pdevinfo->chan_blkcnt);

	return (ch->blksz);
}

static void
hdac_channel_stop(struct hdac_softc *sc, struct hdac_chan *ch)
{
	struct hdac_devinfo *devinfo = ch->devinfo;
	struct hdac_widget *w;
	nid_t cad = devinfo->codec->cad;
	int i;

	hdac_stream_stop(ch);

	for (i = 0; ch->io[i] != -1; i++) {
		w = hdac_widget_get(ch->devinfo, ch->io[i]);
		if (w == NULL)
			continue;
		if (HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap)) {
			hdac_command(sc,
			    HDA_CMD_SET_DIGITAL_CONV_FMT1(cad, ch->io[i], 0),
			    cad);
		}
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

	if (!PCMTRIG_COMMON(go))
		return (0);

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

static int
hdac_audio_ctl_ossmixer_init(struct snd_mixer *m)
{
	struct hdac_pcm_devinfo *pdevinfo = mix_getdevinfo(m);
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w, *cw;
	struct hdac_audio_ctl *ctl;
	uint32_t mask, recmask, id;
	int i, j, softpcmvol;

	hdac_lock(sc);

	/* Make sure that in case of soft volume it won't stay muted. */
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		pdevinfo->left[i] = 100;
		pdevinfo->right[i] = 100;
	}

	mask = 0;
	recmask = 0;
	id = hdac_codec_id(devinfo->codec);

	/* Declate EAPD as ogain control. */
	if (pdevinfo->play >= 0) {
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX ||
			    w->param.eapdbtl == HDAC_INVALID ||
			    w->bindas != sc->chans[pdevinfo->play].as)
				continue;
			mask |= SOUND_MASK_OGAIN;
			break;
		}
	}

	/* Declare volume controls assigned to this association. */
	i = 0;
	ctl = NULL;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0)
			continue;
		if ((pdevinfo->play >= 0 &&
		    ctl->widget->bindas == sc->chans[pdevinfo->play].as) ||
		    (pdevinfo->rec >= 0 &&
		    ctl->widget->bindas == sc->chans[pdevinfo->rec].as) ||
		    (ctl->widget->bindas == -2 && pdevinfo->index == 0))
			mask |= ctl->ossmask;
	}

	/* Declare record sources available to this association. */
	if (pdevinfo->rec >= 0) {
		struct hdac_chan *ch = &sc->chans[pdevinfo->rec];
		for (i = 0; ch->io[i] != -1; i++) {
			w = hdac_widget_get(devinfo, ch->io[i]);
			if (w == NULL || w->enable == 0)
				continue;
			for (j = 0; j < w->nconns; j++) {
				if (w->connsenable[j] == 0)
					continue;
				cw = hdac_widget_get(devinfo, w->conns[j]);
				if (cw == NULL || cw->enable == 0)
					continue;
				if (cw->bindas != sc->chans[pdevinfo->rec].as &&
				    cw->bindas != -2)
					continue;
				recmask |= cw->ossmask;
			}
		}
	}

	/* Declare soft PCM and master volume if needed. */
	if (pdevinfo->play >= 0) {
		ctl = NULL;
		if ((mask & SOUND_MASK_PCM) == 0 ||
		    (devinfo->function.audio.quirks & HDA_QUIRK_SOFTPCMVOL)) {
			softpcmvol = 1;
			mask |= SOUND_MASK_PCM;
		} else {
			softpcmvol = 0;
			i = 0;
			while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
				if (ctl->enable == 0)
					continue;
				if (ctl->widget->bindas != sc->chans[pdevinfo->play].as &&
				    (ctl->widget->bindas != -2 || pdevinfo->index != 0))
					continue;
				if (!(ctl->ossmask & SOUND_MASK_PCM))
					continue;
				if (ctl->step > 0)
					break;
			}
		}

		if (softpcmvol == 1 || ctl == NULL) {
			pcm_setflags(pdevinfo->dev, pcm_getflags(pdevinfo->dev) | SD_F_SOFTPCMVOL);
			HDA_BOOTVERBOSE(
				device_printf(pdevinfo->dev,
				    "%s Soft PCM volume\n",
				    (softpcmvol == 1) ? "Forcing" : "Enabling");
			);
		}

		if ((mask & SOUND_MASK_VOLUME) == 0) {
			mask |= SOUND_MASK_VOLUME;
			mix_setparentchild(m, SOUND_MIXER_VOLUME,
			    SOUND_MASK_PCM);
			mix_setrealdev(m, SOUND_MIXER_VOLUME,
			    SOUND_MIXER_NONE);
			HDA_BOOTVERBOSE(
				device_printf(pdevinfo->dev,
				    "Forcing master volume with PCM\n");
			);
		}
	}

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
	struct hdac_pcm_devinfo *pdevinfo = mix_getdevinfo(m);
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w;
	struct hdac_audio_ctl *ctl;
	uint32_t mute;
	int lvol, rvol;
	int i, j;

	hdac_lock(sc);
	/* Save new values. */
	pdevinfo->left[dev] = left;
	pdevinfo->right[dev] = right;
	
	/* 'ogain' is the special case implemented with EAPD. */
	if (dev == SOUND_MIXER_OGAIN) {
		uint32_t orig;
		w = NULL;
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX ||
			    w->param.eapdbtl == HDAC_INVALID)
				continue;
			break;
		}
		if (i >= devinfo->endnode) {
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

	/* Recalculate all controls related to this OSS device. */
	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0 ||
		    !(ctl->ossmask & (1 << dev)))
			continue;
		if (!((pdevinfo->play >= 0 &&
		    ctl->widget->bindas == sc->chans[pdevinfo->play].as) ||
		    (pdevinfo->rec >= 0 &&
		    ctl->widget->bindas == sc->chans[pdevinfo->rec].as) ||
		    ctl->widget->bindas == -2))
			continue;

		lvol = 100;
		rvol = 100;
		for (j = 0; j < SOUND_MIXER_NRDEVICES; j++) {
			if (ctl->ossmask & (1 << j)) {
				lvol = lvol * pdevinfo->left[j] / 100;
				rvol = rvol * pdevinfo->right[j] / 100;
			}
		}
		mute = (left == 0) ? HDA_AMP_MUTE_LEFT : 0;
		mute |= (right == 0) ? HDA_AMP_MUTE_RIGHT : 0;
		lvol = (lvol * ctl->step + 50) / 100;
		rvol = (rvol * ctl->step + 50) / 100;
		hdac_audio_ctl_amp_set(ctl, mute, lvol, rvol);
	}
	hdac_unlock(sc);

	return (left | (right << 8));
}

/*
 * Commutate specified record source.
 */
static uint32_t
hdac_audio_ctl_recsel_comm(struct hdac_pcm_devinfo *pdevinfo, uint32_t src, nid_t nid, int depth)
{
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_widget *w, *cw;
	struct hdac_audio_ctl *ctl;
	char buf[64];
	int i, muted;
	uint32_t res = 0;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
		
	for (i = 0; i < w->nconns; i++) {
		if (w->connsenable[i] == 0)
			continue;
		cw = hdac_widget_get(devinfo, w->conns[i]);
		if (cw == NULL || cw->enable == 0 || cw->bindas == -1)
			continue;
		/* Call recursively to trace signal to it's source if needed. */
		if ((src & cw->ossmask) != 0) {
			if (cw->ossdev < 0) {
				res |= hdac_audio_ctl_recsel_comm(pdevinfo, src,
				    w->conns[i], depth + 1);
			} else {
				res |= cw->ossmask;
			}
		}
		/* We have two special cases: mixers and others (selectors). */
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER) {
			ctl = hdac_audio_ctl_amp_get(devinfo,
			    w->nid, HDA_CTL_IN, i, 1);
			if (ctl == NULL) 
				continue;
			/* If we have input control on this node mute them
			 * according to requested sources. */
			muted = (src & cw->ossmask) ? 0 : 1;
	    		if (muted != ctl->forcemute) {
				ctl->forcemute = muted;
				hdac_audio_ctl_amp_set(ctl,
				    HDA_AMP_MUTE_DEFAULT,
				    HDA_AMP_VOL_DEFAULT, HDA_AMP_VOL_DEFAULT);
			}
			HDA_BOOTVERBOSE(
				device_printf(pdevinfo->dev,
				    "Recsel (%s): nid %d source %d %s\n",
				    hdac_audio_ctl_ossmixer_mask2allname(
				    src, buf, sizeof(buf)),
				    nid, i, muted?"mute":"unmute");
			);
		} else {
			if (w->nconns == 1)
				break;
			if ((src & cw->ossmask) == 0)
				continue;
			/* If we found requested source - select it and exit. */
			hdac_widget_connection_select(w, i);
			HDA_BOOTVERBOSE(
				device_printf(pdevinfo->dev,
				    "Recsel (%s): nid %d source %d select\n",
				    hdac_audio_ctl_ossmixer_mask2allname(
			    	    src, buf, sizeof(buf)),
				    nid, i);
			);
			break;
		}
	}
	return (res);
}

static uint32_t
hdac_audio_ctl_ossmixer_setrecsrc(struct snd_mixer *m, uint32_t src)
{
	struct hdac_pcm_devinfo *pdevinfo = mix_getdevinfo(m);
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_widget *w;
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_chan *ch;
	int i;
	uint32_t ret = 0xffffffff;

	hdac_lock(sc);

	/* Commutate requested recsrc for each ADC. */
	ch = &sc->chans[pdevinfo->rec];
	for (i = 0; ch->io[i] != -1; i++) {
		w = hdac_widget_get(devinfo, ch->io[i]);
		if (w == NULL || w->enable == 0)
			continue;
		ret &= hdac_audio_ctl_recsel_comm(pdevinfo, src, ch->io[i], 0);
	}

	hdac_unlock(sc);

	return ((ret == 0xffffffff)? 0 : ret);
}

static kobj_method_t hdac_audio_ctl_ossmixer_methods[] = {
	KOBJMETHOD(mixer_init,		hdac_audio_ctl_ossmixer_init),
	KOBJMETHOD(mixer_set,		hdac_audio_ctl_ossmixer_set),
	KOBJMETHOD(mixer_setrecsrc,	hdac_audio_ctl_ossmixer_setrecsrc),
	{ 0, 0 }
};
MIXER_DECLARE(hdac_audio_ctl_ossmixer);

static void
hdac_unsolq_task(void *context, int pending)
{
	struct hdac_softc *sc;

	sc = (struct hdac_softc *)context;

	hdac_lock(sc);
	hdac_unsolq_flush(sc);
	hdac_unlock(sc);
}

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

	device_printf(dev, "<HDA Driver Revision: %s>\n", HDA_DRV_TEST_REV);

	sc = device_get_softc(dev);
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

	TASK_INIT(&sc->unsolq_task, 0, hdac_unsolq_task, sc);

	sc->poll_ticks = 1000000;
	sc->poll_ival = HDAC_POLL_INTERVAL;
	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "polling", &i) == 0 && i != 0)
		sc->polling = 1;
	else
		sc->polling = 0;

	result = bus_dma_tag_create(NULL,	/* parent */
	    HDAC_DMA_ALIGNMENT,			/* alignment */
	    0,					/* boundary */
	    BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL,				/* filtfunc */
	    NULL,				/* fistfuncarg */
	    HDA_BUFSZ_MAX, 			/* maxsize */
	    1,					/* nsegments */
	    HDA_BUFSZ_MAX, 			/* maxsegsz */
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

#ifdef HDAC_MSI_ENABLED
	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "msi", &i) == 0 && i != 0 &&
	    pci_msi_count(dev) == 1)
		sc->flags |= HDAC_F_MSI;
	else
#endif
		sc->flags &= ~HDAC_F_MSI;

#if defined(__i386__) || defined(__amd64__)
	sc->flags |= HDAC_F_DMA_NOCACHE;

	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "snoop", &i) == 0 && i != 0) {
#else
	sc->flags &= ~HDAC_F_DMA_NOCACHE;
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
			sc->flags &= ~HDAC_F_DMA_NOCACHE;
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
				sc->flags |= HDAC_F_DMA_NOCACHE;
#endif
			}
			break;
		}
#if defined(__i386__) || defined(__amd64__)
	}
#endif

	HDA_BOOTVERBOSE(
		device_printf(dev, "DMA Coherency: %s / vendor=0x%04x\n",
		    (sc->flags & HDAC_F_DMA_NOCACHE) ?
		    "Uncacheable" : "PCIe snoop", vendor);
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
	HDA_BOOTVERBOSE(
		device_printf(dev, "Reset controller...\n");
	);
	hdac_reset(sc, 1);

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
	struct hdac_codec *codec = devinfo->codec;
	struct hdac_softc *sc = codec->sc;
	struct hdac_widget *w;
	uint32_t res;
	int i;
	nid_t cad, nid;

	cad = devinfo->codec->cad;
	nid = devinfo->nid;

	res = hdac_command(sc,
	    HDA_CMD_GET_PARAMETER(cad , nid, HDA_PARAM_GPIO_COUNT), cad);
	devinfo->function.audio.gpio = res;

	HDA_BOOTVERBOSE(
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
			w->ossdev = -1;
			w->bindas = -1;
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
					    "BUGGY outamp: nid=%d "
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
			if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX ||
			    w->waspin)
				ctls[cnt].ndir = HDA_CTL_IN;
			else 
				ctls[cnt].ndir = HDA_CTL_OUT;
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
					    "BUGGY inamp: nid=%d "
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
	    				ctls[cnt].ndir = HDA_CTL_IN;
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
				if (w->type ==
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
					ctls[cnt].ndir = HDA_CTL_OUT;
				else 
					ctls[cnt].ndir = HDA_CTL_IN;
				ctls[cnt++].dir = HDA_CTL_IN;
				break;
			}
		}
	}

	devinfo->function.audio.ctl = ctls;
}

static void
hdac_audio_as_parse(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_audio_as *as;
	struct hdac_widget *w;
	int i, j, cnt, max, type, dir, assoc, seq, first, hpredir;

	/* XXX This is redundant */
	max = 0;
	for (j = 0; j < 16; j++) {
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
				continue;
			if (HDA_CONFIG_DEFAULTCONF_ASSOCIATION(w->wclass.pin.config)
			    != j)
				continue;
			max++;
			if (j != 15)  /* There could be many 1-pin assocs #15 */
				break;
		}
	}

	devinfo->function.audio.ascnt = max;

	if (max < 1)
		return;

	as = (struct hdac_audio_as *)malloc(
	    sizeof(*as) * max, M_HDAC, M_ZERO | M_NOWAIT);

	if (as == NULL) {
		/* Blekh! */
		device_printf(sc->dev, "unable to allocate assocs!\n");
		devinfo->function.audio.ascnt = 0;
		return;
	}
	
	for (i = 0; i < max; i++) {
		as[i].hpredir = -1;
		as[i].chan = -1;
	}

	/* Scan associations skipping as=0. */
	cnt = 0;
	for (j = 1; j < 16; j++) {
		first = 16;
		hpredir = 0;
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
				continue;
			assoc = HDA_CONFIG_DEFAULTCONF_ASSOCIATION(w->wclass.pin.config);
			seq = HDA_CONFIG_DEFAULTCONF_SEQUENCE(w->wclass.pin.config);
			if (assoc != j) {
				continue;
			}
			KASSERT(cnt < max,
			    ("%s: Associations owerflow (%d of %d)",
			    __func__, cnt, max));
			type = w->wclass.pin.config &
			    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK;
			/* Get pin direction. */
			if (type == HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_OUT ||
			    type == HDA_CONFIG_DEFAULTCONF_DEVICE_SPEAKER ||
			    type == HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT ||
			    type == HDA_CONFIG_DEFAULTCONF_DEVICE_SPDIF_OUT ||
			    type == HDA_CONFIG_DEFAULTCONF_DEVICE_DIGITAL_OTHER_OUT)
				dir = HDA_CTL_OUT;
			else
				dir = HDA_CTL_IN;
			/* If this is a first pin - create new association. */
			if (as[cnt].pincnt == 0) {
				as[cnt].enable = 1;
				as[cnt].index = j;
				as[cnt].dir = dir;
			}
			if (seq < first)
				first = seq;
			/* Check association correctness. */
			if (as[cnt].pins[seq] != 0) {
				device_printf(sc->dev, "%s: Duplicate pin %d (%d) "
				    "in association %d! Disabling association.\n",
				    __func__, seq, w->nid, j);
				as[cnt].enable = 0;
			}
			if (dir != as[cnt].dir) {
				device_printf(sc->dev, "%s: Pin %d has wrong "
				    "direction for association %d! Disabling "
				    "association.\n",
				    __func__, w->nid, j);
				as[cnt].enable = 0;
			}
			/* Headphones with seq=15 may mean redirection. */
			if (type == HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT &&
			    seq == 15)
				hpredir = 1;
			as[cnt].pins[seq] = w->nid;
			as[cnt].pincnt++;
			/* Association 15 is a multiple unassociated pins. */
			if (j == 15)
				cnt++;
		}
		if (j != 15 && as[cnt].pincnt > 0) {
			if (hpredir && as[cnt].pincnt > 1)
				as[cnt].hpredir = first;
			cnt++;
		}
	}
	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "%d associations found\n", max);
		for (i = 0; i < max; i++) {
			device_printf(sc->dev,
			    "Association %d (%d) %s%s:\n",
			    i, as[i].index, (as[i].dir == HDA_CTL_IN)?"in":"out",
			    as[i].enable?"":" (disabled)");
			for (j = 0; j < 16; j++) {
				if (as[i].pins[j] == 0)
					continue;
				device_printf(sc->dev,
				    "  Pin nid=%d seq=%d\n",
				    as[i].pins[j], j);
			}
		}
	);

	devinfo->function.audio.as = as;
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
	{ ASUS_G2K_SUBVENDOR, HDA_CODEC_ALC660,
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
	{ ASUS_A8X_SUBVENDOR, HDA_CODEC_AD1986A,
	    HDA_QUIRK_EAPDINV, 0 },
	{ ASUS_F3JC_SUBVENDOR, HDA_CODEC_ALC861,
	    HDA_QUIRK_OVREF, 0 },
	{ UNIWILL_9075_SUBVENDOR, HDA_CODEC_ALC861,
	    HDA_QUIRK_OVREF, 0 },
	/*{ ASUS_M2N_SUBVENDOR, HDA_CODEC_AD1988,
	    HDA_QUIRK_IVREF80, HDA_QUIRK_IVREF50 | HDA_QUIRK_IVREF100 },*/
	{ MEDION_MD95257_SUBVENDOR, HDA_CODEC_ALC880,
	    HDA_QUIRK_GPIO1, 0 },
	{ LENOVO_3KN100_SUBVENDOR, HDA_CODEC_AD1986A,
	    HDA_QUIRK_EAPDINV | HDA_QUIRK_SENSEINV, 0 },
	{ SAMSUNG_Q1_SUBVENDOR, HDA_CODEC_AD1986A,
	    HDA_QUIRK_EAPDINV, 0 },
	{ APPLE_MB3_SUBVENDOR, HDA_CODEC_ALC885,
	    HDA_QUIRK_GPIO0 | HDA_QUIRK_OVREF50, 0},
	{ APPLE_INTEL_MAC, HDA_CODEC_STAC9221,
	    HDA_QUIRK_GPIO0 | HDA_QUIRK_GPIO1, 0 },
	{ DELL_D630_SUBVENDOR, HDA_CODEC_STAC9205,
	    HDA_QUIRK_GPIO0, 0 },
	{ DELL_V1500_SUBVENDOR, HDA_CODEC_STAC9205,
	    HDA_QUIRK_GPIO0, 0 },
	{ HDA_MATCH_ALL, HDA_CODEC_AD1988,
	    HDA_QUIRK_IVREF80, HDA_QUIRK_IVREF50 | HDA_QUIRK_IVREF100 },
	{ HDA_MATCH_ALL, HDA_CODEC_AD1988B,
	    HDA_QUIRK_IVREF80, HDA_QUIRK_IVREF50 | HDA_QUIRK_IVREF100 },
	{ HDA_MATCH_ALL, HDA_CODEC_CXVENICE,
	    0, HDA_QUIRK_FORCESTEREO }
};
#define HDAC_QUIRKS_LEN (sizeof(hdac_quirks) / sizeof(hdac_quirks[0]))

static void
hdac_vendor_patch_parse(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w;
	uint32_t id, subvendor;
	int i;

	id = hdac_codec_id(devinfo->codec);
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
	case HDA_CODEC_AD1986A:
		if (subvendor == ASUS_A8X_SUBVENDOR) {
			/*
			 * This is just plain ridiculous.. There
			 * are several A8 series that share the same
			 * pci id but works differently (EAPD).
			 */
			w = hdac_widget_get(devinfo, 26);
			if (w != NULL && w->type ==
			    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
			    (w->wclass.pin.config &
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK) !=
			    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_NONE)
				devinfo->function.audio.quirks &=
				    ~HDA_QUIRK_EAPDINV;
		}
		break;
	}
}

/*
 * Trace path from DAC to pin.
 */
static nid_t
hdac_audio_trace_dac(struct hdac_devinfo *devinfo, int as, int seq, nid_t nid,
    int dupseq, int min, int only, int depth)
{
	struct hdac_widget *w;
	int i, im = -1;
	nid_t m = 0, ret;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);
	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
	HDA_BOOTVERBOSE(
		if (!only) {
			device_printf(devinfo->codec->sc->dev,
			    " %*stracing via nid %d\n",
				depth + 1, "", w->nid);
		}
	);
	/* Use only unused widgets */
	if (w->bindas >= 0 && w->bindas != as) {
		HDA_BOOTVERBOSE(
			if (!only) {
				device_printf(devinfo->codec->sc->dev,
				    " %*snid %d busy by association %d\n",
					depth + 1, "", w->nid, w->bindas);
			}
		);
		return (0);
	}
	if (dupseq < 0) {
		if (w->bindseqmask != 0) {
			HDA_BOOTVERBOSE(
				if (!only) {
					device_printf(devinfo->codec->sc->dev,
					    " %*snid %d busy by seqmask %x\n",
						depth + 1, "", w->nid, w->bindseqmask);
				}
			);
			return (0);
		}
	} else {
		/* If this is headphones - allow duplicate first pin. */
		if (w->bindseqmask != 0 &&
		    (w->bindseqmask & (1 << dupseq)) == 0) {
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    " %*snid %d busy by seqmask %x\n",
					depth + 1, "", w->nid, w->bindseqmask);
			);
			return (0);
		}
	}
		
	switch (w->type) {
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT:
		/* Do not traverse input. AD1988 has digital monitor
		for which we are not ready. */
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT:
		/* If we are tracing HP take only dac of first pin. */
		if ((only == 0 || only == w->nid) &&
		    (w->nid >= min) && (dupseq < 0 || w->nid ==
		    devinfo->function.audio.as[as].dacs[dupseq]))
			m = w->nid;
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX:
		if (depth > 0)
			break;
		/* Fall */
	default:
		/* Find reachable DACs with smallest nid respecting constraints. */
		for (i = 0; i < w->nconns; i++) {
			if (w->connsenable[i] == 0)
				continue;
			if (w->selconn != -1 && w->selconn != i)
				continue;
			if ((ret = hdac_audio_trace_dac(devinfo, as, seq,
			    w->conns[i], dupseq, min, only, depth + 1)) != 0) {
				if (m == 0 || ret < m) {
					m = ret;
					im = i;
				}
				if (only || dupseq >= 0)
					break;
			}
		}
		if (m && only && ((w->nconns > 1 &&
		    w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER) ||
		    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR))
			w->selconn = im;
		break;
	}
	if (m && only) {
		w->bindas = as;
		w->bindseqmask |= (1 << seq);
	}
	HDA_BOOTVERBOSE(
		if (!only) {
			device_printf(devinfo->codec->sc->dev,
			    " %*snid %d returned %d\n",
				depth + 1, "", w->nid, m);
		}
	);
	return (m);
}

/*
 * Trace path from widget to ADC.
 */
static nid_t
hdac_audio_trace_adc(struct hdac_devinfo *devinfo, int as, int seq, nid_t nid,
    int only, int depth)
{
	struct hdac_widget *w, *wc;
	int i, j;
	nid_t res = 0;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);
	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    " %*stracing via nid %d\n",
			depth + 1, "", w->nid);
	);
	/* Use only unused widgets */
	if (w->bindas >= 0 && w->bindas != as) {
		HDA_BOOTVERBOSE(
			device_printf(devinfo->codec->sc->dev,
			    " %*snid %d busy by association %d\n",
				depth + 1, "", w->nid, w->bindas);
		);
		return (0);
	}
		
	switch (w->type) {
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT:
		/* If we are tracing HP take only dac of first pin. */
		if (only == w->nid)
			res = 1;
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX:
		if (depth > 0)
			break;
		/* Fall */
	default:
		/* Try to find reachable ADCs with specified nid. */
		for (j = devinfo->startnode; j < devinfo->endnode; j++) {
			wc = hdac_widget_get(devinfo, j);
			if (wc == NULL || wc->enable == 0)
				continue;
			for (i = 0; i < wc->nconns; i++) {
				if (wc->connsenable[i] == 0)
					continue;
				if (wc->conns[i] != nid)
					continue;
				if (hdac_audio_trace_adc(devinfo, as, seq,
				    j, only, depth + 1) != 0) {
					res = 1;
					if (((wc->nconns > 1 &&
					    wc->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER) ||
					    wc->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR) &&
					    wc->selconn == -1)
						wc->selconn = i;
				}
			}
		}
		break;
	}
	if (res) {
		w->bindas = as;
		w->bindseqmask |= (1 << seq);
	}
	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    " %*snid %d returned %d\n",
			depth + 1, "", w->nid, res);
	);
	return (res);
}

/*
 * Erase trace path of the specified association.
 */
static void
hdac_audio_undo_trace(struct hdac_devinfo *devinfo, int as, int seq)
{
	struct hdac_widget *w;
	int i;
	
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->bindas == as) {
			if (seq >= 0) {
				w->bindseqmask &= ~(1 << seq);
				if (w->bindseqmask == 0) {
					w->bindas = -1;
					w->selconn = -1;
				}
			} else {
				w->bindas = -1;
				w->bindseqmask = 0;
				w->selconn = -1;
			}
		}
	}
}

/*
 * Trace association path from DAC to output
 */
static int
hdac_audio_trace_as_out(struct hdac_devinfo *devinfo, int as, int seq)
{
	struct hdac_audio_as *ases = devinfo->function.audio.as;
	int i, hpredir;
	nid_t min, res;

	/* Find next pin */
	for (i = seq; ases[as].pins[i] == 0 && i < 16; i++)
		;
	/* Check if there is no any left. If so - we succeded. */
	if (i == 16)
		return (1);
	
	hpredir = (i == 15 && ases[as].fakeredir == 0)?ases[as].hpredir:-1;
	min = 0;
	res = 0;
	do {
		HDA_BOOTVERBOSE(
			device_printf(devinfo->codec->sc->dev,
			    " Tracing pin %d with min nid %d",
			    ases[as].pins[i], min);
			if (hpredir >= 0)
				printf(" and hpredir %d\n", hpredir);
			else
				printf("\n");
		);
		/* Trace this pin taking min nid into account. */
		res = hdac_audio_trace_dac(devinfo, as, i,
		    ases[as].pins[i], hpredir, min, 0, 0);
		if (res == 0) {
			/* If we failed - return to previous and redo it. */
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    " Unable to trace pin %d seq %d with min "
				    "nid %d hpredir %d\n",
				    ases[as].pins[i], i, min, hpredir);
			);
			return (0);
		}
		HDA_BOOTVERBOSE(
			device_printf(devinfo->codec->sc->dev,
			    " Pin %d traced to DAC %d%s\n",
			    ases[as].pins[i], res,
			    ases[as].fakeredir?" with fake redirection":"");
		);
		/* Trace again to mark the path */
		hdac_audio_trace_dac(devinfo, as, i,
		    ases[as].pins[i], hpredir, min, res, 0);
		ases[as].dacs[i] = res;
		/* We succeded, so call next. */
		if (hdac_audio_trace_as_out(devinfo, as, i + 1))
			return (1);
		/* If next failed, we should retry with next min */
		hdac_audio_undo_trace(devinfo, as, i);
		ases[as].dacs[i] = 0;
		min = res + 1;
	} while (1);
}

/*
 * Trace association path from input to ADC
 */
static int
hdac_audio_trace_as_in(struct hdac_devinfo *devinfo, int as)
{
	struct hdac_audio_as *ases = devinfo->function.audio.as;
	struct hdac_widget *w;
	int i, j, k;

	for (j = devinfo->startnode; j < devinfo->endnode; j++) {
		w = hdac_widget_get(devinfo, j);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT)
			continue;
		if (w->bindas >= 0 && w->bindas != as)
			continue;

		/* Find next pin */
		for (i = 0; i < 16; i++) {
			if (ases[as].pins[i] == 0)
				continue;
	
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    " Tracing pin %d to ADC %d\n",
				    ases[as].pins[i], j);
			);
			/* Trace this pin taking goal into account. */
			if (hdac_audio_trace_adc(devinfo, as, i,
			    ases[as].pins[i], j, 0) == 0) {
				/* If we failed - return to previous and redo it. */
				HDA_BOOTVERBOSE(
					device_printf(devinfo->codec->sc->dev,
					    " Unable to trace pin %d to ADC %d\n",
					    ases[as].pins[i], j);
				);
				hdac_audio_undo_trace(devinfo, as, -1);
				for (k = 0; k < 16; k++)
					ases[as].dacs[k] = 0;
				break;
			}
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    " Traced to ADC %d\n",
				    j);
			);
			ases[as].dacs[i] = j;
		}
		if (i == 16)
			return (1);
	}
	return (0);
}

/*
 * Trace input monitor path from mixer to output association.
 */
static nid_t
hdac_audio_trace_to_out(struct hdac_devinfo *devinfo, nid_t nid, int depth)
{
	struct hdac_audio_as *ases = devinfo->function.audio.as;
	struct hdac_widget *w, *wc;
	int i, j;
	nid_t res = 0;

	if (depth > HDA_PARSE_MAXDEPTH)
		return (0);
	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (0);
	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    " %*stracing via nid %d\n",
			depth + 1, "", w->nid);
	);
	/* Use only unused widgets */
	if (depth > 0 && w->bindas != -1) {
		if (w->bindas < 0 || ases[w->bindas].dir == HDA_CTL_OUT) {
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    " %*snid %d found output association %d\n",
					depth + 1, "", w->nid, w->bindas);
			);
			return (1);
		} else {
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    " %*snid %d busy by input association %d\n",
					depth + 1, "", w->nid, w->bindas);
			);
			return (0);
		}
	}
		
	switch (w->type) {
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT:
		/* Do not traverse input. AD1988 has digital monitor
		for which we are not ready. */
		break;
	case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX:
		if (depth > 0)
			break;
		/* Fall */
	default:
		/* Try to find reachable ADCs with specified nid. */
		for (j = devinfo->startnode; j < devinfo->endnode; j++) {
			wc = hdac_widget_get(devinfo, j);
			if (wc == NULL || wc->enable == 0)
				continue;
			for (i = 0; i < wc->nconns; i++) {
				if (wc->connsenable[i] == 0)
					continue;
				if (wc->conns[i] != nid)
					continue;
				if (hdac_audio_trace_to_out(devinfo,
				    j, depth + 1) != 0) {
					res = 1;
					if (wc->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR &&
					    wc->selconn == -1)
						wc->selconn = i;
				}
			}
		}
		break;
	}
	if (res)
		w->bindas = -2;

	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    " %*snid %d returned %d\n",
			depth + 1, "", w->nid, res);
	);
	return (res);
}

/*
 * Trace extra associations (beeper, monitor)
 */
static void
hdac_audio_trace_as_extra(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_widget *w;
	int j;

	/* Input monitor */
	/* Find mixer associated with input, but supplying signal
	   for output associations. Hope it will be input monitor. */
	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    "Tracing input monitor\n");
	);
	for (j = devinfo->startnode; j < devinfo->endnode; j++) {
		w = hdac_widget_get(devinfo, j);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER)
			continue;
		if (w->bindas < 0 || as[w->bindas].dir != HDA_CTL_IN)
			continue;
		HDA_BOOTVERBOSE(
			device_printf(devinfo->codec->sc->dev,
			    " Tracing nid %d to out\n",
			    j);
		);
		if (hdac_audio_trace_to_out(devinfo, w->nid, 0)) {
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    " nid %d is input monitor\n",
					w->nid);
			);
			w->pflags |= HDA_ADC_MONITOR;
			w->ossdev = SOUND_MIXER_IMIX;
		}
	}

	/* Beeper */
	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    "Tracing beeper\n");
	);
	for (j = devinfo->startnode; j < devinfo->endnode; j++) {
		w = hdac_widget_get(devinfo, j);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET)
			continue;
		HDA_BOOTVERBOSE(
			device_printf(devinfo->codec->sc->dev,
			    " Tracing nid %d to out\n",
			    j);
		);
		hdac_audio_trace_to_out(devinfo, w->nid, 0);
		w->bindas = -2;
	}
}

/*
 * Bind assotiations to PCM channels
 */
static void
hdac_audio_bind_as(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_audio_as *as = devinfo->function.audio.as;
	int j, cnt = 0, free;

	for (j = 0; j < devinfo->function.audio.ascnt; j++) {
		if (as[j].enable)
			cnt++;
	}
	if (sc->num_chans == 0) {
		sc->chans = (struct hdac_chan *)malloc(
		    sizeof(struct hdac_chan) * cnt,
		    M_HDAC, M_ZERO | M_NOWAIT);
		if (sc->chans == NULL) {
			device_printf(devinfo->codec->sc->dev,
			    "Channels memory allocation failed!\n");
			return;
		}
	} else {
		sc->chans = (struct hdac_chan *)realloc(sc->chans, 
		    sizeof(struct hdac_chan) * cnt,
		    M_HDAC, M_ZERO | M_NOWAIT);
		if (sc->chans == NULL) {
			sc->num_chans = 0;
			device_printf(devinfo->codec->sc->dev,
			    "Channels memory allocation failed!\n");
			return;
		}
	}
	free = sc->num_chans;
	sc->num_chans += cnt;

	for (j = free; j < free + cnt; j++) {
		devinfo->codec->sc->chans[j].devinfo = devinfo;
		devinfo->codec->sc->chans[j].as = -1;
	}

	/* Assign associations in order of their numbers, */
	free = 0;
	for (j = 0; j < devinfo->function.audio.ascnt; j++) {
		if (as[j].enable == 0)
			continue;
		
		as[j].chan = free;
		devinfo->codec->sc->chans[free].as = j;
		if (as[j].dir == HDA_CTL_IN) {
			devinfo->codec->sc->chans[free].dir = PCMDIR_REC;
			devinfo->function.audio.reccnt++;
		} else {
			devinfo->codec->sc->chans[free].dir = PCMDIR_PLAY;
			devinfo->function.audio.playcnt++;
		}
		hdac_pcmchannel_setup(&devinfo->codec->sc->chans[free]);
		free++;
	}
}

static void
hdac_audio_disable_nonaudio(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w;
	int i;

	/* Disable power and volume widgets. */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_POWER_WIDGET ||
		    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_VOLUME_WIDGET) {
			w->enable = 0;
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev, 
				    " Disabling nid %d due to it's"
				    " non-audio type.\n",
				    w->nid);
			);
		}
	}
}

static void
hdac_audio_disable_useless(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w, *cw;
	struct hdac_audio_ctl *ctl;
	int done, found, i, j, k;

	/* Disable useless pins. */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
		    (w->wclass.pin.config &
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK) ==
		    HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_NONE) {
			w->enable = 0;
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev, 
				    " Disabling pin nid %d due"
				    " to None connectivity.\n",
				    w->nid);
			);
		}
	}
	do {
		done = 1;
		/* Disable and mute controls for disabled widgets. */
		i = 0;
		while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
			if (ctl->enable == 0)
				continue;
			if (ctl->widget->enable == 0 ||
			    (ctl->childwidget != NULL &&
			    ctl->childwidget->enable == 0)) {
				ctl->forcemute = 1;
				ctl->muted = HDA_AMP_MUTE_ALL;
				ctl->left = 0;
				ctl->right = 0;
				ctl->enable = 0;
				if (ctl->ndir == HDA_CTL_IN)
					ctl->widget->connsenable[ctl->index] = 0;
				done = 0;
				HDA_BOOTVERBOSE(
					device_printf(devinfo->codec->sc->dev, 
					    " Disabling ctl %d nid %d cnid %d due"
					    " to disabled widget.\n", i,
					    ctl->widget->nid,
					    (ctl->childwidget != NULL)?
					    ctl->childwidget->nid:-1);
				);
			}
		}
		/* Disable useless widgets. */
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			w = hdac_widget_get(devinfo, i);
			if (w == NULL || w->enable == 0)
				continue;
			/* Disable inputs with disabled child widgets. */
			for (j = 0; j < w->nconns; j++) {
				if (w->connsenable[j]) {
					cw = hdac_widget_get(devinfo, w->conns[j]);
					if (cw == NULL || cw->enable == 0) {
						w->connsenable[j] = 0;
						HDA_BOOTVERBOSE(
							device_printf(devinfo->codec->sc->dev, 
							    " Disabling nid %d connection %d due"
							    " to disabled child widget.\n",
							    i, j);
						);
					}
				}
			}
			if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR &&
			    w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER)
				continue;
			/* Disable mixers and selectors without inputs. */
			found = 0;
			for (j = 0; j < w->nconns; j++) {
				if (w->connsenable[j]) {
					found = 1;
					break;
				}
			}
			if (found == 0) {
				w->enable = 0;
				done = 0;
				HDA_BOOTVERBOSE(
					device_printf(devinfo->codec->sc->dev, 
					    " Disabling nid %d due to all it's"
					    " inputs disabled.\n", w->nid);
				);
			}
			/* Disable nodes without consumers. */
			if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_SELECTOR &&
			    w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER)
				continue;
			found = 0;
			for (k = devinfo->startnode; k < devinfo->endnode; k++) {
				cw = hdac_widget_get(devinfo, k);
				if (cw == NULL || cw->enable == 0)
					continue;
				for (j = 0; j < cw->nconns; j++) {
					if (cw->connsenable[j] && cw->conns[j] == i) {
						found = 1;
						break;
					}
				}
			}
			if (found == 0) {
				w->enable = 0;
				done = 0;
				HDA_BOOTVERBOSE(
					device_printf(devinfo->codec->sc->dev, 
					    " Disabling nid %d due to all it's"
					    " consumers disabled.\n", w->nid);
				);
			}
		}
	} while (done == 0);

}

static void
hdac_audio_disable_unas(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_widget *w, *cw;
	struct hdac_audio_ctl *ctl;
	int i, j, k;

	/* Disable unassosiated widgets. */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->bindas == -1) {
			w->enable = 0;
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev, 
				    " Disabling unassociated nid %d.\n",
				    w->nid);
			);
		}
	}
	/* Disable input connections on input pin and
	 * output on output. */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if (w->bindas < 0)
			continue;
		if (as[w->bindas].dir == HDA_CTL_IN) {
			for (j = 0; j < w->nconns; j++) {
				if (w->connsenable[j] == 0)
					continue;
				w->connsenable[j] = 0;
				HDA_BOOTVERBOSE(
					device_printf(devinfo->codec->sc->dev, 
					    " Disabling connection to input pin "
					    "nid %d conn %d.\n",
					    i, j);
				);
			}
			ctl = hdac_audio_ctl_amp_get(devinfo, w->nid,
			    HDA_CTL_IN, -1, 1);
			if (ctl && ctl->enable) {
				ctl->forcemute = 1;
				ctl->muted = HDA_AMP_MUTE_ALL;
				ctl->left = 0;
				ctl->right = 0;
				ctl->enable = 0;
			}
		} else {
			ctl = hdac_audio_ctl_amp_get(devinfo, w->nid,
			    HDA_CTL_OUT, -1, 1);
			if (ctl && ctl->enable) {
				ctl->forcemute = 1;
				ctl->muted = HDA_AMP_MUTE_ALL;
				ctl->left = 0;
				ctl->right = 0;
				ctl->enable = 0;
			}
			for (k = devinfo->startnode; k < devinfo->endnode; k++) {
				cw = hdac_widget_get(devinfo, k);
				if (cw == NULL || cw->enable == 0)
					continue;
				for (j = 0; j < cw->nconns; j++) {
					if (cw->connsenable[j] && cw->conns[j] == i) {
						cw->connsenable[j] = 0;
						HDA_BOOTVERBOSE(
							device_printf(devinfo->codec->sc->dev, 
							    " Disabling connection from output pin "
							    "nid %d conn %d cnid %d.\n",
							    k, j, i);
						);
						if (cw->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
						    cw->nconns > 1)
							continue;
						ctl = hdac_audio_ctl_amp_get(devinfo, k,
		    				    HDA_CTL_IN, j, 1);
						if (ctl && ctl->enable) {
							ctl->forcemute = 1;
							ctl->muted = HDA_AMP_MUTE_ALL;
							ctl->left = 0;
							ctl->right = 0;
							ctl->enable = 0;
						}
					}
				}
			}
		}
	}
}

static void
hdac_audio_disable_notselected(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_widget *w;
	int i, j;

	/* On playback path we can safely disable all unseleted inputs. */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->nconns <= 1)
			continue;
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER)
			continue;
		if (w->bindas < 0 || as[w->bindas].dir == HDA_CTL_IN)
			continue;
		for (j = 0; j < w->nconns; j++) {
			if (w->connsenable[j] == 0)
				continue;
			if (w->selconn < 0 || w->selconn == j)
				continue;
			w->connsenable[j] = 0;
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev, 
				    " Disabling unselected connection "
				    "nid %d conn %d.\n",
				    i, j);
			);
		}
	}
}

static void
hdac_audio_disable_crossas(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w, *cw;
	struct hdac_audio_ctl *ctl;
	int i, j;

	/* Disable crossassociatement connections. */
	/* ... using selectors */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->nconns <= 1)
			continue;
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_MIXER)
			continue;
		if (w->bindas == -2)
			continue;
		for (j = 0; j < w->nconns; j++) {
			if (w->connsenable[j] == 0)
				continue;
			cw = hdac_widget_get(devinfo, w->conns[j]);
			if (cw == NULL || w->enable == 0)
				continue;
			if (w->bindas == cw->bindas || cw->bindas == -2)
				continue;
			w->connsenable[j] = 0;
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev, 
				    " Disabling crossassociatement connection "
				    "nid %d conn %d cnid %d.\n",
				    i, j, cw->nid);
			);
		}
	}
	/* ... using controls */
	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0 || ctl->childwidget == NULL)
			continue;
		if (ctl->widget->bindas == -2 ||
		    ctl->childwidget->bindas == -2)
			continue;
		if (ctl->widget->bindas != ctl->childwidget->bindas) {
			ctl->forcemute = 1;
			ctl->muted = HDA_AMP_MUTE_ALL;
			ctl->left = 0;
			ctl->right = 0;
			ctl->enable = 0;
			if (ctl->ndir == HDA_CTL_IN)
				ctl->widget->connsenable[ctl->index] = 0;
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev, 
				    " Disabling crossassociatement connection "
				    "ctl %d nid %d cnid %d.\n", i,
				    ctl->widget->nid,
				    ctl->childwidget->nid);
			);
		}
	}

}

#define HDA_CTL_GIVE(ctl)	((ctl)->step?1:0)

/*
 * Find controls to control amplification for source.
 */
static int
hdac_audio_ctl_source_amp(struct hdac_devinfo *devinfo, nid_t nid, int index,
    int ossdev, int ctlable, int depth, int need)
{
	struct hdac_widget *w, *wc;
	struct hdac_audio_ctl *ctl;
	int i, j, conns = 0, rneed;
	
	if (depth > HDA_PARSE_MAXDEPTH)
		return (need);

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return (need);

	/* Count number of active inputs. */
	if (depth > 0) {
		for (j = 0; j < w->nconns; j++) {
			if (w->connsenable[j])
				conns++;
		}
	}

	/* If this is not a first step - use input mixer.
	   Pins have common input ctl so care must be taken. */
	if (depth > 0 && ctlable && (conns == 1 ||
	    w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)) {
		ctl = hdac_audio_ctl_amp_get(devinfo, w->nid, HDA_CTL_IN,
		    index, 1);
		if (ctl) {
			if (HDA_CTL_GIVE(ctl) & need)
				ctl->ossmask |= (1 << ossdev);
			else
				ctl->possmask |= (1 << ossdev);
			need &= ~HDA_CTL_GIVE(ctl);
		}
	}
	
	/* If widget has own ossdev - not traverse it.
	   It will be traversed on it's own. */
	if (w->ossdev >= 0 && depth > 0)
		return (need);

	/* We must not traverse pin */
	if ((w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT ||
	    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) &&
	    depth > 0)
		return (need);
	
	/* record that this widget exports such signal, */
	w->ossmask |= (1 << ossdev);

	/* If signals mixed, we can't assign controls farther.
	 * Ignore this on depth zero. Caller must knows why.
	 * Ignore this for static selectors if this input selected.
	 */
	if (conns > 1)
		ctlable = 0;

	if (ctlable) {
		ctl = hdac_audio_ctl_amp_get(devinfo, w->nid, HDA_CTL_OUT, -1, 1);
		if (ctl) {
			if (HDA_CTL_GIVE(ctl) & need)
				ctl->ossmask |= (1 << ossdev);
			else
				ctl->possmask |= (1 << ossdev);
			need &= ~HDA_CTL_GIVE(ctl);
		}
	}
	
	rneed = 0;
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		wc = hdac_widget_get(devinfo, i);
		if (wc == NULL || wc->enable == 0)
			continue;
		for (j = 0; j < wc->nconns; j++) {
			if (wc->connsenable[j] && wc->conns[j] == nid) {
				rneed |= hdac_audio_ctl_source_amp(devinfo,
				    wc->nid, j, ossdev, ctlable, depth + 1, need);
			}
		}
	}
	rneed &= need;
	
	return (rneed);
}

/*
 * Find controls to control amplification for destination.
 */
static void
hdac_audio_ctl_dest_amp(struct hdac_devinfo *devinfo, nid_t nid,
    int ossdev, int depth, int need)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_widget *w, *wc;
	struct hdac_audio_ctl *ctl;
	int i, j, consumers;
	
	if (depth > HDA_PARSE_MAXDEPTH)
		return;

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return;

	if (depth > 0) {
		/* If this node produce output for several consumers,
		   we can't touch it. */
		consumers = 0;
		for (i = devinfo->startnode; i < devinfo->endnode; i++) {
			wc = hdac_widget_get(devinfo, i);
			if (wc == NULL || wc->enable == 0)
				continue;
			for (j = 0; j < wc->nconns; j++) {
				if (wc->connsenable[j] && wc->conns[j] == nid)
					consumers++;
			}
		}
		/* The only exception is if real HP redirection is configured
		   and this is a duplication point.
		   XXX: Actually exception is not completely correct.
		   XXX: Duplication point check is not perfect. */
		if ((consumers == 2 && (w->bindas < 0 ||
		    as[w->bindas].hpredir < 0 || as[w->bindas].fakeredir ||
		    (w->bindseqmask & (1 << 15)) == 0)) ||
		    consumers > 2)
			return;

		/* Else use it's output mixer. */
		ctl = hdac_audio_ctl_amp_get(devinfo, w->nid,
		    HDA_CTL_OUT, -1, 1);
		if (ctl) {
			if (HDA_CTL_GIVE(ctl) & need)
				ctl->ossmask |= (1 << ossdev);
			else
				ctl->possmask |= (1 << ossdev);
			need &= ~HDA_CTL_GIVE(ctl);
		}
	}
	
	/* We must not traverse pin */
	if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
	    depth > 0)
		return;
	
	for (i = 0; i < w->nconns; i++) {
		int tneed = need;
		if (w->connsenable[i] == 0)
			continue;
		ctl = hdac_audio_ctl_amp_get(devinfo, w->nid,
		    HDA_CTL_IN, i, 1);
		if (ctl) {
			if (HDA_CTL_GIVE(ctl) & tneed)
				ctl->ossmask |= (1 << ossdev);
			else
				ctl->possmask |= (1 << ossdev);
			tneed &= ~HDA_CTL_GIVE(ctl);
		}
		hdac_audio_ctl_dest_amp(devinfo, w->conns[i], ossdev,
		    depth + 1, tneed);
	}
}

/*
 * Assign OSS names to sound sources
 */
static void
hdac_audio_assign_names(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_widget *w;
	int i, j;
	int type = -1, use, used = 0;
	static const int types[7][13] = {
	    { SOUND_MIXER_LINE, SOUND_MIXER_LINE1, SOUND_MIXER_LINE2, 
	      SOUND_MIXER_LINE3, -1 },	/* line */
	    { SOUND_MIXER_MONITOR, SOUND_MIXER_MIC, -1 }, /* int mic */
	    { SOUND_MIXER_MIC, SOUND_MIXER_MONITOR, -1 }, /* ext mic */
	    { SOUND_MIXER_CD, -1 },	/* cd */
	    { SOUND_MIXER_SPEAKER, -1 },	/* speaker */
	    { SOUND_MIXER_DIGITAL1, SOUND_MIXER_DIGITAL2, SOUND_MIXER_DIGITAL3,
	      -1 },	/* digital */
	    { SOUND_MIXER_LINE, SOUND_MIXER_LINE1, SOUND_MIXER_LINE2,
	      SOUND_MIXER_LINE3, SOUND_MIXER_PHONEIN, SOUND_MIXER_PHONEOUT,
	      SOUND_MIXER_VIDEO, SOUND_MIXER_RADIO, SOUND_MIXER_DIGITAL1,
	      SOUND_MIXER_DIGITAL2, SOUND_MIXER_DIGITAL3, SOUND_MIXER_MONITOR,
	      -1 }	/* others */
	};

	/* Surely known names */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->bindas == -1)
			continue;
		use = -1;
		switch (w->type) {
		case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX:
			if (as[w->bindas].dir == HDA_CTL_OUT)
				break;
			type = -1;
			switch (w->wclass.pin.config & HDA_CONFIG_DEFAULTCONF_DEVICE_MASK) {
			case HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN:
				type = 0;
				break;
			case HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN:
				if ((w->wclass.pin.config & HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK)
				    == HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK)
					break;
				type = 1;
				break;
			case HDA_CONFIG_DEFAULTCONF_DEVICE_CD:
				type = 3;
				break;
			case HDA_CONFIG_DEFAULTCONF_DEVICE_SPEAKER:
				type = 4;
				break;
			case HDA_CONFIG_DEFAULTCONF_DEVICE_SPDIF_IN:
			case HDA_CONFIG_DEFAULTCONF_DEVICE_DIGITAL_OTHER_IN:
				type = 5;
				break;
			}
			if (type == -1)
				break;
			j = 0;
			while (types[type][j] >= 0 &&
			    (used & (1 << types[type][j])) != 0) {
				j++;
			}
			if (types[type][j] >= 0)
				use = types[type][j];
			break;
		case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT:
			use = SOUND_MIXER_PCM;
			break;
		case HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET:
			use = SOUND_MIXER_SPEAKER;
			break;
		default:
			break;
		}
		if (use >= 0) {
			w->ossdev = use;
			used |= (1 << use);
		}
	}
	/* Semi-known names */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->ossdev >= 0)
			continue;
		if (w->bindas == -1)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if (as[w->bindas].dir == HDA_CTL_OUT)
			continue;
		type = -1;
		switch (w->wclass.pin.config & HDA_CONFIG_DEFAULTCONF_DEVICE_MASK) {
		case HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_OUT:
		case HDA_CONFIG_DEFAULTCONF_DEVICE_SPEAKER:
		case HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT:
		case HDA_CONFIG_DEFAULTCONF_DEVICE_AUX:
			type = 0;
			break;
		case HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN:
			type = 2;
			break;
		case HDA_CONFIG_DEFAULTCONF_DEVICE_SPDIF_OUT:
		case HDA_CONFIG_DEFAULTCONF_DEVICE_DIGITAL_OTHER_OUT:
			type = 5;
			break;
		}
		if (type == -1)
			break;
		j = 0;
		while (types[type][j] >= 0 &&
		    (used & (1 << types[type][j])) != 0) {
			j++;
		}
		if (types[type][j] >= 0) {
			w->ossdev = types[type][j];
			used |= (1 << types[type][j]);
		}
	}
	/* Others */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->ossdev >= 0)
			continue;
		if (w->bindas == -1)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if (as[w->bindas].dir == HDA_CTL_OUT)
			continue;
		j = 0;
		while (types[6][j] >= 0 &&
		    (used & (1 << types[6][j])) != 0) {
			j++;
		}
		if (types[6][j] >= 0) {
			w->ossdev = types[6][j];
			used |= (1 << types[6][j]);
		}
	}
}

static void
hdac_audio_build_tree(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	int j, res;

	HDA_BOOTVERBOSE(
		device_printf(devinfo->codec->sc->dev,
		    "HWiP: HDA Widget Parser - Revision %d\n",
		    HDA_WIDGET_PARSER_REV);
	);
	
	/* Trace all associations in order of their numbers, */
	for (j = 0; j < devinfo->function.audio.ascnt; j++) {
		if (as[j].enable == 0)
			continue;
		HDA_BOOTVERBOSE(
			device_printf(devinfo->codec->sc->dev,
			    "Tracing association %d (%d)\n", j, as[j].index);
		);
		if (as[j].dir == HDA_CTL_OUT) {
retry:
			res = hdac_audio_trace_as_out(devinfo, j, 0);
			if (res == 0 && as[j].hpredir >= 0 &&
			    as[j].fakeredir == 0) {
				/* If codec can't do analog HP redirection
				   try to make it using one more DAC. */
				as[j].fakeredir = 1;
				goto retry;
			}
		} else {
			res = hdac_audio_trace_as_in(devinfo, j);
		}
		if (res) {
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    "Association %d (%d) trace succeded\n",
				    j, as[j].index);
			);
		} else {
			HDA_BOOTVERBOSE(
				device_printf(devinfo->codec->sc->dev,
				    "Association %d (%d) trace failed\n",
				    j, as[j].index);
			);
			as[j].enable = 0;
		}
	}

	/* Trace mixer and beeper pseudo associations. */
	hdac_audio_trace_as_extra(devinfo);
}

static void
hdac_audio_assign_mixers(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_audio_ctl *ctl;
	struct hdac_widget *w;
	int i;

	/* Assign mixers to the tree. */
	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT ||
		    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_BEEP_WIDGET ||
		    (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
		    as[w->bindas].dir == HDA_CTL_IN)) {
			if (w->ossdev < 0)
				continue;
			hdac_audio_ctl_source_amp(devinfo, w->nid, -1,
			    w->ossdev, 1, 0, 1);
		} else if ((w->pflags & HDA_ADC_MONITOR) != 0) {
			if (w->ossdev < 0)
				continue;
			if (hdac_audio_ctl_source_amp(devinfo, w->nid, -1,
			    w->ossdev, 1, 0, 1)) {
				/* If we are unable to control input monitor
				   as source - try to control it as destination. */
				hdac_audio_ctl_dest_amp(devinfo, w->nid,
				    w->ossdev, 0, 1);
			}
		} else if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT) {
			hdac_audio_ctl_dest_amp(devinfo, w->nid,
			    SOUND_MIXER_RECLEV, 0, 1);
		} else if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX &&
		    as[w->bindas].dir == HDA_CTL_OUT) {
			hdac_audio_ctl_dest_amp(devinfo, w->nid,
			    SOUND_MIXER_VOLUME, 0, 1);
		}
	}
	/* Treat unrequired as possible. */
	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->ossmask == 0)
			ctl->ossmask = ctl->possmask;
	}
}

static void
hdac_audio_prepare_pin_ctrl(struct hdac_devinfo *devinfo)
{
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_widget *w;
	uint32_t pincap;
	int i;

	for (i = 0; i < devinfo->nodecnt; i++) {
		w = &devinfo->widget[i];
		if (w == NULL)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;

		pincap = w->wclass.pin.cap;

		/* Disable everything. */
		w->wclass.pin.ctrl &= ~(
		    HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE |
		    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE |
		    HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE |
		    HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE_MASK);

		if (w->enable == 0 ||
		    w->bindas < 0 || as[w->bindas].enable == 0) {
			/* Pin is unused so left it disabled. */
			continue;
		} else if (as[w->bindas].dir == HDA_CTL_IN) {
			/* Input pin, configure for input. */
			if (HDA_PARAM_PIN_CAP_INPUT_CAP(pincap))
				w->wclass.pin.ctrl |=
				    HDA_CMD_SET_PIN_WIDGET_CTRL_IN_ENABLE;

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
		} else {
			/* Output pin, configure for output. */
			if (HDA_PARAM_PIN_CAP_OUTPUT_CAP(pincap))
				w->wclass.pin.ctrl |=
				    HDA_CMD_SET_PIN_WIDGET_CTRL_OUT_ENABLE;

			if (HDA_PARAM_PIN_CAP_HEADPHONE_CAP(pincap) &&
			    (w->wclass.pin.config &
			    HDA_CONFIG_DEFAULTCONF_DEVICE_MASK) ==
			    HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT)
				w->wclass.pin.ctrl |=
				    HDA_CMD_SET_PIN_WIDGET_CTRL_HPHN_ENABLE;

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
		}
	}
}

static void
hdac_audio_commit(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w;
	nid_t cad;
	uint32_t gdata, gmask, gdir;
	int commitgpio, numgpio;
	int i;

	cad = devinfo->codec->cad;

	if (sc->pci_subvendor == APPLE_INTEL_MAC)
		hdac_command(sc, HDA_CMD_12BIT(cad, devinfo->nid,
		    0x7e7, 0), cad);

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

	for (i = 0; i < devinfo->nodecnt; i++) {
		w = &devinfo->widget[i];
		if (w == NULL)
			continue;
		if (w->selconn == -1)
			w->selconn = 0;
		if (w->nconns > 0)
			hdac_widget_connection_select(w, w->selconn);
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX) {
			hdac_command(sc,
			    HDA_CMD_SET_PIN_WIDGET_CTRL(cad, w->nid,
			    w->wclass.pin.ctrl), cad);
		}
		if (w->param.eapdbtl != HDAC_INVALID) {
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
	struct hdac_audio_ctl *ctl;
	int i, z;

	i = 0;
	while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
		if (ctl->enable == 0) {
			/* Mute disabled controls. */
			hdac_audio_ctl_amp_set(ctl, HDA_AMP_MUTE_ALL, 0, 0);
			continue;
		}
		/* Init controls to 0dB amplification. */
		z = ctl->offset;
		if (z > ctl->step)
			z = ctl->step;
		hdac_audio_ctl_amp_set(ctl, HDA_AMP_MUTE_NONE, z, z);
	}
}

static void
hdac_powerup(struct hdac_devinfo *devinfo)
{
	struct hdac_softc *sc = devinfo->codec->sc;
	nid_t cad = devinfo->codec->cad;
	int i;

	hdac_command(sc,
	    HDA_CMD_SET_POWER_STATE(cad,
	    devinfo->nid, HDA_CMD_POWER_STATE_D0),
	    cad);
	DELAY(100);

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		hdac_command(sc,
		    HDA_CMD_SET_POWER_STATE(cad,
		    i, HDA_CMD_POWER_STATE_D0),
		    cad);
	}
	DELAY(1000);
}

static int
hdac_pcmchannel_setup(struct hdac_chan *ch)
{
	struct hdac_devinfo *devinfo = ch->devinfo;
	struct hdac_audio_as *as = devinfo->function.audio.as;
	struct hdac_widget *w;
	uint32_t cap, fmtcap, pcmcap;
	int i, j, ret, max;

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

	for (i = 0; i < 16 && ret < max; i++) {
		/* Check as is correct */
		if (ch->as < 0)
			break;
		/* Cound only present DACs */
		if (as[ch->as].dacs[i] <= 0)
			continue;
		/* Ignore duplicates */
		for (j = 0; j < ret; j++) {
			if (ch->io[j] == as[ch->as].dacs[i])
				break;
		}
		if (j < ret)
			continue;

		w = hdac_widget_get(devinfo, as[ch->as].dacs[i]);
		if (w == NULL || w->enable == 0)
			continue;
		if (!HDA_PARAM_AUDIO_WIDGET_CAP_STEREO(w->param.widget_cap))
			continue;
		cap = w->param.supp_stream_formats;
		/*if (HDA_PARAM_SUPP_STREAM_FORMATS_FLOAT32(cap)) {
		}*/
		if (!HDA_PARAM_SUPP_STREAM_FORMATS_PCM(cap) &&
		    !HDA_PARAM_SUPP_STREAM_FORMATS_AC3(cap))
			continue;
		/* Many codec does not declare AC3 support on SPDIF.
		   I don't beleave that they doesn't support it! */
		if (HDA_PARAM_AUDIO_WIDGET_CAP_DIGITAL(w->param.widget_cap))
			cap |= HDA_PARAM_SUPP_STREAM_FORMATS_AC3_MASK;
		if (ret == 0) {
			fmtcap = cap;
			pcmcap = w->param.supp_pcm_size_rate;
		} else {
			fmtcap &= cap;
			pcmcap &= w->param.supp_pcm_size_rate;
		}
		ch->io[ret++] = as[ch->as].dacs[i];
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
		i = 0;
		if (HDA_PARAM_SUPP_STREAM_FORMATS_PCM(fmtcap)) {
			if (HDA_PARAM_SUPP_PCM_SIZE_RATE_16BIT(pcmcap))
				ch->bit16 = 1;
			else if (HDA_PARAM_SUPP_PCM_SIZE_RATE_8BIT(pcmcap))
				ch->bit16 = 0;
			if (HDA_PARAM_SUPP_PCM_SIZE_RATE_32BIT(pcmcap))
				ch->bit32 = 4;
			else if (HDA_PARAM_SUPP_PCM_SIZE_RATE_24BIT(pcmcap))
				ch->bit32 = 3;
			else if (HDA_PARAM_SUPP_PCM_SIZE_RATE_20BIT(pcmcap))
				ch->bit32 = 2;
			if (!(devinfo->function.audio.quirks & HDA_QUIRK_FORCESTEREO))
				ch->fmtlist[i++] = AFMT_S16_LE;
			ch->fmtlist[i++] = AFMT_S16_LE | AFMT_STEREO;
			if (ch->bit32 > 0) {
				if (!(devinfo->function.audio.quirks &
				    HDA_QUIRK_FORCESTEREO))
					ch->fmtlist[i++] = AFMT_S32_LE;
				ch->fmtlist[i++] = AFMT_S32_LE | AFMT_STEREO;
			}
		}
		if (HDA_PARAM_SUPP_STREAM_FORMATS_AC3(fmtcap)) {
			ch->fmtlist[i++] = AFMT_AC3;
		}
		ch->fmtlist[i] = 0;
		i = 0;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_8KHZ(pcmcap))
			ch->pcmrates[i++] = 8000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_11KHZ(pcmcap))
			ch->pcmrates[i++] = 11025;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_16KHZ(pcmcap))
			ch->pcmrates[i++] = 16000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_22KHZ(pcmcap))
			ch->pcmrates[i++] = 22050;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_32KHZ(pcmcap))
			ch->pcmrates[i++] = 32000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_44KHZ(pcmcap))
			ch->pcmrates[i++] = 44100;
		/* if (HDA_PARAM_SUPP_PCM_SIZE_RATE_48KHZ(pcmcap)) */
		ch->pcmrates[i++] = 48000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_88KHZ(pcmcap))
			ch->pcmrates[i++] = 88200;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_96KHZ(pcmcap))
			ch->pcmrates[i++] = 96000;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_176KHZ(pcmcap))
			ch->pcmrates[i++] = 176400;
		if (HDA_PARAM_SUPP_PCM_SIZE_RATE_192KHZ(pcmcap))
			ch->pcmrates[i++] = 192000;
		/* if (HDA_PARAM_SUPP_PCM_SIZE_RATE_384KHZ(pcmcap)) */
		ch->pcmrates[i] = 0;
		if (i > 0) {
			ch->caps.minspeed = ch->pcmrates[0];
			ch->caps.maxspeed = ch->pcmrates[i - 1];
		}
	}

	return (ret);
}

static void
hdac_dump_ctls(struct hdac_pcm_devinfo *pdevinfo, const char *banner, uint32_t flag)
{
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_audio_ctl *ctl;
	struct hdac_softc *sc = devinfo->codec->sc;
	char buf[64];
	int i, j, printed;

	if (flag == 0) {
		flag = ~(SOUND_MASK_VOLUME | SOUND_MASK_PCM |
		    SOUND_MASK_CD | SOUND_MASK_LINE | SOUND_MASK_RECLEV |
		    SOUND_MASK_MIC | SOUND_MASK_SPEAKER | SOUND_MASK_OGAIN |
		    SOUND_MASK_IMIX | SOUND_MASK_MONITOR);
	}

	for (j = 0; j < SOUND_MIXER_NRDEVICES; j++) {
		if ((flag & (1 << j)) == 0)
			continue;
		i = 0;
		printed = 0;
		while ((ctl = hdac_audio_ctl_each(devinfo, &i)) != NULL) {
			if (ctl->enable == 0 ||
			    ctl->widget->enable == 0)
				continue;
			if (!((pdevinfo->play >= 0 &&
			    ctl->widget->bindas == sc->chans[pdevinfo->play].as) ||
			    (pdevinfo->rec >= 0 &&
			    ctl->widget->bindas == sc->chans[pdevinfo->rec].as) ||
			    (ctl->widget->bindas == -2 && pdevinfo->index == 0)))
				continue;
			if ((ctl->ossmask & (1 << j)) == 0)
				continue;

	    		if (printed == 0) {
				device_printf(pdevinfo->dev, "\n");
				if (banner != NULL) {
					device_printf(pdevinfo->dev, "%s", banner);
				} else {
					device_printf(pdevinfo->dev, "Unknown Ctl");
				}
				printf(" (OSS: %s)\n",
				    hdac_audio_ctl_ossmixer_mask2allname(1 << j,
				    buf, sizeof(buf)));
				device_printf(pdevinfo->dev, "   |\n");
				printed = 1;
			}
			device_printf(pdevinfo->dev, "   +- ctl %2d (nid %3d %s", i,
				ctl->widget->nid,
				(ctl->ndir == HDA_CTL_IN)?"in ":"out");
			if (ctl->ndir == HDA_CTL_IN && ctl->ndir == ctl->dir)
				printf(" %2d): ", ctl->index);
			else
				printf("):    ");
			if (ctl->step > 0) {
				printf("%+d/%+ddB (%d steps)%s\n",
			    	    (0 - ctl->offset) * (ctl->size + 1) / 4,
				    (ctl->step - ctl->offset) * (ctl->size + 1) / 4,
				    ctl->step + 1,
				    ctl->mute?" + mute":"");
			} else
				printf("%s\n", ctl->mute?"mute":"");
		}
	}
}

static void
hdac_dump_audio_formats(device_t dev, uint32_t fcap, uint32_t pcmcap)
{
	uint32_t cap;

	cap = fcap;
	if (cap != 0) {
		device_printf(dev, "     Stream cap: 0x%08x\n", cap);
		device_printf(dev, "         Format:");
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
		device_printf(dev, "        PCM cap: 0x%08x\n", cap);
		device_printf(dev, "       PCM size:");
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
		device_printf(dev, "       PCM rate:");
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
	if (w->wclass.pin.ctrl & HDA_CMD_SET_PIN_WIDGET_CTRL_VREF_ENABLE_MASK)
		printf(" VREFs");
	printf("\n");
}

static void
hdac_dump_pin_config(struct hdac_widget *w, uint32_t conf)
{
	struct hdac_softc *sc = w->devinfo->codec->sc;

	device_printf(sc->dev, "nid %d 0x%08x as %2d seq %2d %13s %5s "
	    "jack %2d loc %2d color %7s misc %d%s\n",
	    w->nid, conf,
	    HDA_CONFIG_DEFAULTCONF_ASSOCIATION(conf),
	    HDA_CONFIG_DEFAULTCONF_SEQUENCE(conf),
	    HDA_DEVS[HDA_CONFIG_DEFAULTCONF_DEVICE(conf)],
	    HDA_CONNS[HDA_CONFIG_DEFAULTCONF_CONNECTIVITY(conf)],
	    HDA_CONFIG_DEFAULTCONF_CONNECTION_TYPE(conf),
	    HDA_CONFIG_DEFAULTCONF_LOCATION(conf),
	    HDA_COLORS[HDA_CONFIG_DEFAULTCONF_COLOR(conf)],
	    HDA_CONFIG_DEFAULTCONF_MISC(conf),
	    (w->enable == 0)?" [DISABLED]":"");
}

static void
hdac_dump_pin_configs(struct hdac_devinfo *devinfo)
{
	struct hdac_widget *w;
	int i;

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		hdac_dump_pin_config(w, w->wclass.pin.config);
	}
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
	static char *ossname[] = SOUND_DEVICE_NAMES;
	struct hdac_widget *w, *cw;
	char buf[64];
	int i, j;

	device_printf(sc->dev, "\n");
	device_printf(sc->dev, "Default Parameter\n");
	device_printf(sc->dev, "-----------------\n");
	hdac_dump_audio_formats(sc->dev,
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
		device_printf(sc->dev, "    Parse flags: 0x%x\n",
		    w->pflags);
		device_printf(sc->dev, "    Association: %d (0x%08x)\n",
		    w->bindas, w->bindseqmask);
		device_printf(sc->dev, "            OSS: %s",
		    hdac_audio_ctl_ossmixer_mask2allname(w->ossmask, buf, sizeof(buf)));
		if (w->ossdev >= 0)
		    printf(" (%s)", ossname[w->ossdev]);
		printf("\n");
		if (w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_OUTPUT ||
		    w->type == HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT) {
			hdac_dump_audio_formats(sc->dev,
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
		if (w->nconns > 0)
			device_printf(sc->dev, "          |\n");
		for (j = 0; j < w->nconns; j++) {
			cw = hdac_widget_get(devinfo, w->conns[j]);
			device_printf(sc->dev, "          + %s<- nid=%d [%s]",
			    (w->connsenable[j] == 0)?"[DISABLED] ":"",
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
hdac_dump_dst_nid(struct hdac_pcm_devinfo *pdevinfo, nid_t nid, int depth)
{
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_widget *w, *cw;
	char buf[64];
	int i, printed = 0;

	if (depth > HDA_PARSE_MAXDEPTH)
		return;

	w = hdac_widget_get(devinfo, nid);
	if (w == NULL || w->enable == 0)
		return;

	if (depth == 0)
		device_printf(pdevinfo->dev, "%*s", 4, "");
	else
		device_printf(pdevinfo->dev, "%*s  + <- ", 4 + (depth - 1) * 7, "");
	printf("nid=%d [%s]", w->nid, w->name);

	if (depth > 0) {
		if (w->ossmask == 0) {
			printf("\n");
			return;
		}
		printf(" [src: %s]", 
		    hdac_audio_ctl_ossmixer_mask2allname(
			w->ossmask, buf, sizeof(buf)));
		if (w->ossdev >= 0) {
			printf("\n");
			return;
		}
	}
	printf("\n");
		
	for (i = 0; i < w->nconns; i++) {
		if (w->connsenable[i] == 0)
			continue;
		cw = hdac_widget_get(devinfo, w->conns[i]);
		if (cw == NULL || cw->enable == 0 || cw->bindas == -1)
			continue;
		if (printed == 0) {
			device_printf(pdevinfo->dev, "%*s  |\n", 4 + (depth) * 7, "");
			printed = 1;
		}
		hdac_dump_dst_nid(pdevinfo, w->conns[i], depth + 1);
	}

}

static void
hdac_dump_dac(struct hdac_pcm_devinfo *pdevinfo)
{
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w;
	int i, printed = 0;

	if (pdevinfo->play < 0)
		return;

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
			continue;
		if (w->bindas != sc->chans[pdevinfo->play].as)
			continue;
		if (printed == 0) {
			printed = 1;
			device_printf(pdevinfo->dev, "\n");
			device_printf(pdevinfo->dev, "Playback:\n");
		}
		device_printf(pdevinfo->dev, "\n");
		hdac_dump_dst_nid(pdevinfo, i, 0);
	}
}

static void
hdac_dump_adc(struct hdac_pcm_devinfo *pdevinfo)
{
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_softc *sc = devinfo->codec->sc;
	struct hdac_widget *w;
	int i;
	int printed = 0;

	if (pdevinfo->rec < 0)
		return;

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if (w->type != HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_AUDIO_INPUT)
			continue;
		if (w->bindas != sc->chans[pdevinfo->rec].as)
			continue;
		if (printed == 0) {
			printed = 1;
			device_printf(pdevinfo->dev, "\n");
			device_printf(pdevinfo->dev, "Record:\n");
		}
		device_printf(pdevinfo->dev, "\n");
		hdac_dump_dst_nid(pdevinfo, i, 0);
	}
}

static void
hdac_dump_mix(struct hdac_pcm_devinfo *pdevinfo)
{
	struct hdac_devinfo *devinfo = pdevinfo->devinfo;
	struct hdac_widget *w;
	int i;
	int printed = 0;

	if (pdevinfo->index != 0)
		return;

	for (i = devinfo->startnode; i < devinfo->endnode; i++) {
		w = hdac_widget_get(devinfo, i);
		if (w == NULL || w->enable == 0)
			continue;
		if ((w->pflags & HDA_ADC_MONITOR) == 0)
			continue;
		if (printed == 0) {
			printed = 1;
			device_printf(pdevinfo->dev, "\n");
			device_printf(pdevinfo->dev, "Input Mix:\n");
		}
		device_printf(pdevinfo->dev, "\n");
		hdac_dump_dst_nid(pdevinfo, i, 0);
	}
}

static void
hdac_dump_pcmchannels(struct hdac_pcm_devinfo *pdevinfo)
{
	struct hdac_softc *sc = pdevinfo->devinfo->codec->sc;
	nid_t *nids;
	int i;

	if (pdevinfo->play >= 0) {
		i = pdevinfo->play;
		device_printf(pdevinfo->dev, "\n");
		device_printf(pdevinfo->dev, "Playback:\n");
		device_printf(pdevinfo->dev, "\n");
		hdac_dump_audio_formats(pdevinfo->dev, sc->chans[i].supp_stream_formats,
		    sc->chans[i].supp_pcm_size_rate);
		device_printf(pdevinfo->dev, "            DAC:");
		for (nids = sc->chans[i].io; *nids != -1; nids++)
			printf(" %d", *nids);
		printf("\n");
	}
	if (pdevinfo->rec >= 0) {
		i = pdevinfo->rec;
		device_printf(pdevinfo->dev, "\n");
		device_printf(pdevinfo->dev, "Record:\n");
		device_printf(pdevinfo->dev, "\n");
		hdac_dump_audio_formats(pdevinfo->dev, sc->chans[i].supp_stream_formats,
		    sc->chans[i].supp_pcm_size_rate);
		device_printf(pdevinfo->dev, "            ADC:");
		for (nids = sc->chans[i].io; *nids != -1; nids++)
			printf(" %d", *nids);
		printf("\n");
	}
}

static void
hdac_release_resources(struct hdac_softc *sc)
{
        int i, j;

	if (sc == NULL)
		return;

	hdac_lock(sc);
	sc->polling = 0;
	sc->poll_ival = 0;
	callout_stop(&sc->poll_hda);
	callout_stop(&sc->poll_hdac);
	callout_stop(&sc->poll_jack);
	hdac_reset(sc, 0);
	hdac_unlock(sc);
	taskqueue_drain(taskqueue_thread, &sc->unsolq_task);
	callout_drain(&sc->poll_hda);
	callout_drain(&sc->poll_hdac);
	callout_drain(&sc->poll_jack);

	hdac_irq_free(sc);

	for (i = 0; i < HDAC_CODEC_MAX; i++) {
		if (sc->codecs[i] == NULL)
			continue;
		for (j = 0; j < sc->codecs[i]->num_fgs; j++) {
			free(sc->codecs[i]->fgs[j].widget, M_HDAC);
			if (sc->codecs[i]->fgs[j].node_type ==
			    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO) {
				free(sc->codecs[i]->fgs[j].function.audio.ctl,
				    M_HDAC);
				free(sc->codecs[i]->fgs[j].function.audio.as,
				    M_HDAC);
				free(sc->codecs[i]->fgs[j].function.audio.devs,
				    M_HDAC);
			}
		}
		free(sc->codecs[i]->fgs, M_HDAC);
		free(sc->codecs[i], M_HDAC);
		sc->codecs[i] = NULL;
	}

	hdac_dma_free(sc, &sc->pos_dma);
	hdac_dma_free(sc, &sc->rirb_dma);
	hdac_dma_free(sc, &sc->corb_dma);
	for (i = 0; i < sc->num_chans; i++) {
    		if (sc->chans[i].blkcnt > 0)
    			hdac_dma_free(sc, &sc->chans[i].bdl_dma);
	}
	free(sc->chans, M_HDAC);
	if (sc->chan_dmat != NULL) {
		bus_dma_tag_destroy(sc->chan_dmat);
		sc->chan_dmat = NULL;
	}
	hdac_mem_free(sc);
	snd_mtxfree(sc->lock);
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
		device_printf(sc->dev, "HDA Config:");
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
	device_t dev;
	uint32_t ctl;
	int err, val;

	dev = oidp->oid_arg1;
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);
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
		if (val == 0) {
			callout_stop(&sc->poll_hda);
			callout_stop(&sc->poll_hdac);
			hdac_unlock(sc);
			callout_drain(&sc->poll_hda);
			callout_drain(&sc->poll_hdac);
			hdac_lock(sc);
			sc->polling = 0;
			ctl = HDAC_READ_4(&sc->mem, HDAC_INTCTL);
			ctl |= HDAC_INTCTL_GIE;
			HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, ctl);
		} else {
			ctl = HDAC_READ_4(&sc->mem, HDAC_INTCTL);
			ctl &= ~HDAC_INTCTL_GIE;
			HDAC_WRITE_4(&sc->mem, HDAC_INTCTL, ctl);
			hdac_unlock(sc);
			taskqueue_drain(taskqueue_thread, &sc->unsolq_task);
			hdac_lock(sc);
			sc->polling = 1;
			hdac_poll_reinit(sc);
			callout_reset(&sc->poll_hdac, 1, hdac_poll_callback, sc);
		}
	}
	hdac_unlock(sc);

	return (err);
}

static int
sysctl_hdac_polling_interval(SYSCTL_HANDLER_ARGS)
{
	struct hdac_softc *sc;
	device_t dev;
	int err, val;

	dev = oidp->oid_arg1;
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);
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

static int
sysctl_hdac_pindump(SYSCTL_HANDLER_ARGS)
{
	struct hdac_softc *sc;
	struct hdac_codec *codec;
	struct hdac_devinfo *devinfo;
	struct hdac_widget *w;
	device_t dev;
	uint32_t res, pincap, delay;
	int codec_index, fg_index;
	int i, err, val;
	nid_t cad;

	dev = oidp->oid_arg1;
	sc = device_get_softc(dev);
	if (sc == NULL)
		return (EINVAL);
	val = 0;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL || val == 0)
		return (err);
	
	/* XXX: Temporary. For debugging. */
	if (val == 100) {
		hdac_suspend(dev);
		return (0);
	} else if (val == 101) {
		hdac_resume(dev);
		return (0);
	}
	
	hdac_lock(sc);
	for (codec_index = 0; codec_index < HDAC_CODEC_MAX; codec_index++) {
		codec = sc->codecs[codec_index];
		if (codec == NULL)
			continue;
		cad = codec->cad;
		for (fg_index = 0; fg_index < codec->num_fgs; fg_index++) {
			devinfo = &codec->fgs[fg_index];
			if (devinfo->node_type !=
			    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO)
				continue;

			device_printf(dev, "Dumping AFG cad=%d nid=%d pins:\n",
			    codec_index, devinfo->nid);
			for (i = devinfo->startnode; i < devinfo->endnode; i++) {
					w = hdac_widget_get(devinfo, i);
				if (w == NULL || w->type !=
				    HDA_PARAM_AUDIO_WIDGET_CAP_TYPE_PIN_COMPLEX)
					continue;
				hdac_dump_pin_config(w, w->wclass.pin.config);
				pincap = w->wclass.pin.cap;
				device_printf(dev, "       Caps: %2s %3s %2s %4s %4s",
				    HDA_PARAM_PIN_CAP_INPUT_CAP(pincap)?"IN":"",
				    HDA_PARAM_PIN_CAP_OUTPUT_CAP(pincap)?"OUT":"",
				    HDA_PARAM_PIN_CAP_HEADPHONE_CAP(pincap)?"HP":"",
				    HDA_PARAM_PIN_CAP_EAPD_CAP(pincap)?"EAPD":"",
				    HDA_PARAM_PIN_CAP_VREF_CTRL(pincap)?"VREF":"");
				if (HDA_PARAM_PIN_CAP_IMP_SENSE_CAP(pincap) ||
				    HDA_PARAM_PIN_CAP_PRESENCE_DETECT_CAP(pincap)) {
					if (HDA_PARAM_PIN_CAP_TRIGGER_REQD(pincap)) {
						delay = 0;
						hdac_command(sc,
						    HDA_CMD_SET_PIN_SENSE(cad, w->nid, 0), cad);
						do {
							res = hdac_command(sc,
							    HDA_CMD_GET_PIN_SENSE(cad, w->nid), cad);
							if (res != 0x7fffffff && res != 0xffffffff)
								break;
							DELAY(10);
						} while (++delay < 10000);
					} else {
						delay = 0;
						res = hdac_command(sc, HDA_CMD_GET_PIN_SENSE(cad,
						    w->nid), cad);
					}
					printf(" Sense: 0x%08x", res);
					if (delay > 0)
						printf(" delay %dus", delay * 10);
				}
				printf("\n");
			}
			device_printf(dev,
			    "NumGPIO=%d NumGPO=%d NumGPI=%d GPIWake=%d GPIUnsol=%d\n",
			    HDA_PARAM_GPIO_COUNT_NUM_GPIO(devinfo->function.audio.gpio),
			    HDA_PARAM_GPIO_COUNT_NUM_GPO(devinfo->function.audio.gpio),
			    HDA_PARAM_GPIO_COUNT_NUM_GPI(devinfo->function.audio.gpio),
			    HDA_PARAM_GPIO_COUNT_GPI_WAKE(devinfo->function.audio.gpio),
			    HDA_PARAM_GPIO_COUNT_GPI_UNSOL(devinfo->function.audio.gpio));
			if (HDA_PARAM_GPIO_COUNT_NUM_GPI(devinfo->function.audio.gpio) > 0) {
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
			if (HDA_PARAM_GPIO_COUNT_NUM_GPO(devinfo->function.audio.gpio) > 0) {
				device_printf(dev, " GPO:");
				res = hdac_command(sc,
				    HDA_CMD_GET_GPO_DATA(cad, devinfo->nid), cad);
				printf(" data=0x%08x\n", res);
			}
			if (HDA_PARAM_GPIO_COUNT_NUM_GPIO(devinfo->function.audio.gpio) > 0) {
				device_printf(dev, "GPIO:");
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
		}
	}
	hdac_unlock(sc);
	return (0);
}
#endif

static void
hdac_attach2(void *arg)
{
	struct hdac_codec *codec;
	struct hdac_softc *sc;
	struct hdac_audio_ctl *ctl;
	uint32_t quirks_on, quirks_off;
	int codec_index, fg_index;
	int i, pdev, rdev, dmaalloc = 0;
	struct hdac_devinfo *devinfo;

	sc = (struct hdac_softc *)arg;

	hdac_config_fetch(sc, &quirks_on, &quirks_off);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "HDA Config: on=0x%08x off=0x%08x\n",
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
		device_printf(sc->dev, "Starting CORB Engine...\n");
	);
	hdac_corb_start(sc);
	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "Starting RIRB Engine...\n");
	);
	hdac_rirb_start(sc);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "Enabling controller interrupt...\n");
	);
	HDAC_WRITE_4(&sc->mem, HDAC_GCTL, HDAC_READ_4(&sc->mem, HDAC_GCTL) |
	    HDAC_GCTL_UNSOL);
	if (sc->polling == 0) {
		HDAC_WRITE_4(&sc->mem, HDAC_INTCTL,
		    HDAC_INTCTL_CIE | HDAC_INTCTL_GIE);
	} else {
		callout_reset(&sc->poll_hdac, 1, hdac_poll_callback, sc);
	}
	DELAY(1000);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev,
		    "Scanning HDA codecs ...\n");
	);
	hdac_scan_codecs(sc);
	
	for (codec_index = 0; codec_index < HDAC_CODEC_MAX; codec_index++) {
		codec = sc->codecs[codec_index];
		if (codec == NULL)
			continue;
		for (fg_index = 0; fg_index < codec->num_fgs; fg_index++) {
			devinfo = &codec->fgs[fg_index];
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "\n");
			);
			if (devinfo->node_type !=
			    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO) {
				HDA_BOOTVERBOSE(
					device_printf(sc->dev,
					    "Power down unsupported non-audio FG"
					    " cad=%d nid=%d to the D3 state...\n",
					    codec->cad, devinfo->nid);
				);
				hdac_command(sc,
				    HDA_CMD_SET_POWER_STATE(codec->cad,
				    devinfo->nid, HDA_CMD_POWER_STATE_D3),
				    codec->cad);
				continue;
			}

			HDA_BOOTVERBOSE(
				device_printf(sc->dev,
				    "Power up audio FG cad=%d nid=%d...\n",
				    devinfo->codec->cad, devinfo->nid);
			);
			hdac_powerup(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Parsing audio FG...\n");
			);
			hdac_audio_parse(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Parsing Ctls...\n");
			);
		    	hdac_audio_ctl_parse(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Parsing vendor patch...\n");
			);
			hdac_vendor_patch_parse(devinfo);
			devinfo->function.audio.quirks |= quirks_on;
			devinfo->function.audio.quirks &= ~quirks_off;

			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Disabling nonaudio...\n");
			);
			hdac_audio_disable_nonaudio(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Disabling useless...\n");
			);
			hdac_audio_disable_useless(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Patched pins configuration:\n");
				hdac_dump_pin_configs(devinfo);
				device_printf(sc->dev, "Parsing pin associations...\n");
			);
			hdac_audio_as_parse(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Building AFG tree...\n");
			);
			hdac_audio_build_tree(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Disabling unassociated "
				    "widgets...\n");
			);
			hdac_audio_disable_unas(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Disabling nonselected "
				    "inputs...\n");
			);
			hdac_audio_disable_notselected(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Disabling useless...\n");
			);
			hdac_audio_disable_useless(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Disabling "
				    "crossassociatement connections...\n");
			);
			hdac_audio_disable_crossas(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Disabling useless...\n");
			);
			hdac_audio_disable_useless(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Binding associations to channels...\n");
			);
			hdac_audio_bind_as(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Assigning names to signal sources...\n");
			);
			hdac_audio_assign_names(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Assigning mixers to the tree...\n");
			);
			hdac_audio_assign_mixers(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Preparing pin controls...\n");
			);
			hdac_audio_prepare_pin_ctrl(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(sc->dev, "AFG commit...\n");
		    	);
			hdac_audio_commit(devinfo);
		    	HDA_BOOTVERBOSE(
				device_printf(sc->dev, "Ctls commit...\n");
			);
			hdac_audio_ctl_commit(devinfo);
		    	HDA_BOOTVERBOSE(
				device_printf(sc->dev, "HP switch init...\n");
			);
			hdac_hp_switch_init(devinfo);

			if ((devinfo->function.audio.quirks & HDA_QUIRK_DMAPOS) &&
			    dmaalloc == 0) {
				if (hdac_dma_alloc(sc, &sc->pos_dma,
				    (sc->num_iss + sc->num_oss + sc->num_bss) * 8) != 0) {
					HDA_BOOTVERBOSE(
						device_printf(sc->dev, "Failed to "
						    "allocate DMA pos buffer "
						    "(non-fatal)\n");
					);
				} else
					dmaalloc = 1;
			}
			
			i = devinfo->function.audio.playcnt;
			if (devinfo->function.audio.reccnt > i)
				i = devinfo->function.audio.reccnt;
			devinfo->function.audio.devs =
			    (struct hdac_pcm_devinfo *)malloc(
			    sizeof(struct hdac_pcm_devinfo) * i,
			    M_HDAC, M_ZERO | M_NOWAIT);
			if (devinfo->function.audio.devs == NULL) {
				device_printf(sc->dev,
				    "Unable to allocate memory for devices\n");
				continue;
			}
			devinfo->function.audio.num_devs = i;
			for (i = 0; i < devinfo->function.audio.num_devs; i++) {
				devinfo->function.audio.devs[i].index = i;
				devinfo->function.audio.devs[i].devinfo = devinfo;
				devinfo->function.audio.devs[i].play = -1;
				devinfo->function.audio.devs[i].rec = -1;
			}
			pdev = 0;
			rdev = 0;
			for (i = 0; i < devinfo->function.audio.ascnt; i++) {
				if (devinfo->function.audio.as[i].enable == 0)
					continue;
				if (devinfo->function.audio.as[i].dir ==
				    HDA_CTL_IN) {
					devinfo->function.audio.devs[rdev].rec
					    = devinfo->function.audio.as[i].chan;
					sc->chans[devinfo->function.audio.as[i].chan].pdevinfo = 
					    &devinfo->function.audio.devs[rdev];
					rdev++;
				} else {
					devinfo->function.audio.devs[pdev].play
					    = devinfo->function.audio.as[i].chan;
					sc->chans[devinfo->function.audio.as[i].chan].pdevinfo = 
					    &devinfo->function.audio.devs[pdev];
					pdev++;
				}
			}
			for (i = 0; i < devinfo->function.audio.num_devs; i++) {
				struct hdac_pcm_devinfo *pdevinfo = 
				    &devinfo->function.audio.devs[i];
				pdevinfo->dev =
				    device_add_child(sc->dev, "pcm", -1);
				device_set_ivars(pdevinfo->dev,
				     (void *)pdevinfo);
			}

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
					device_printf(sc->dev, "%3d: nid %3d %s (%s) index %d", i,
					    (ctl->widget != NULL) ? ctl->widget->nid : -1,
					    (ctl->ndir == HDA_CTL_IN)?"in ":"out",
					    (ctl->dir == HDA_CTL_IN)?"in ":"out",
					    ctl->index);
					if (ctl->childwidget != NULL)
						printf(" cnid %3d", ctl->childwidget->nid);
					else
						printf("         ");
					printf(" ossmask=0x%08x\n",
					    ctl->ossmask);
					device_printf(sc->dev, 
					    "       mute: %d step: %3d size: %3d off: %3d%s\n",
					    ctl->mute, ctl->step, ctl->size, ctl->offset,
					    (ctl->enable == 0) ? " [DISABLED]" : 
					    ((ctl->ossmask == 0) ? " [UNUSED]" : ""));
				}
			);
		}
	}
	hdac_unlock(sc);

	HDA_BOOTVERBOSE(
		device_printf(sc->dev, "\n");
	);

	bus_generic_attach(sc->dev);

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
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(sc->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)), OID_AUTO,
	    "pindump", CTLTYPE_INT | CTLFLAG_RW, sc->dev, sizeof(sc->dev),
	    sysctl_hdac_pindump, "I", "Dump pin states/data");
#endif
}

/****************************************************************************
 * int hdac_suspend(device_t)
 *
 * Suspend and power down HDA bus and codecs.
 ****************************************************************************/
static int
hdac_suspend(device_t dev)
{
	struct hdac_softc *sc;
	struct hdac_codec *codec;
	struct hdac_devinfo *devinfo;
	int codec_index, fg_index, i;

	HDA_BOOTVERBOSE(
		device_printf(dev, "Suspend...\n");
	);

	sc = device_get_softc(dev);
	hdac_lock(sc);
	
	HDA_BOOTVERBOSE(
		device_printf(dev, "Stop streams...\n");
	);
	for (i = 0; i < sc->num_chans; i++) {
		if (sc->chans[i].flags & HDAC_CHN_RUNNING) {
			sc->chans[i].flags |= HDAC_CHN_SUSPEND;
			hdac_channel_stop(sc, &sc->chans[i]);
		}
	}

	for (codec_index = 0; codec_index < HDAC_CODEC_MAX; codec_index++) {
		codec = sc->codecs[codec_index];
		if (codec == NULL)
			continue;
		for (fg_index = 0; fg_index < codec->num_fgs; fg_index++) {
			devinfo = &codec->fgs[fg_index];
			HDA_BOOTVERBOSE(
				device_printf(dev,
				    "Power down FG"
				    " cad=%d nid=%d to the D3 state...\n",
				    codec->cad, devinfo->nid);
			);
			hdac_command(sc,
			    HDA_CMD_SET_POWER_STATE(codec->cad,
			    devinfo->nid, HDA_CMD_POWER_STATE_D3),
			    codec->cad);
		}
	}

	HDA_BOOTVERBOSE(
		device_printf(dev, "Reset controller...\n");
	);
	callout_stop(&sc->poll_hda);
	callout_stop(&sc->poll_hdac);
	callout_stop(&sc->poll_jack);
	hdac_reset(sc, 0);
	hdac_unlock(sc);
	taskqueue_drain(taskqueue_thread, &sc->unsolq_task);
	callout_drain(&sc->poll_hda);
	callout_drain(&sc->poll_hdac);
	callout_drain(&sc->poll_jack);

	HDA_BOOTVERBOSE(
		device_printf(dev, "Suspend done\n");
	);

	return (0);
}

/****************************************************************************
 * int hdac_resume(device_t)
 *
 * Powerup and restore HDA bus and codecs state.
 ****************************************************************************/
static int
hdac_resume(device_t dev)
{
	struct hdac_softc *sc;
	struct hdac_codec *codec;
	struct hdac_devinfo *devinfo;
	int codec_index, fg_index, i;

	HDA_BOOTVERBOSE(
		device_printf(dev, "Resume...\n");
	);

	sc = device_get_softc(dev);
	hdac_lock(sc);

	/* Quiesce everything */
	HDA_BOOTVERBOSE(
		device_printf(dev, "Reset controller...\n");
	);
	hdac_reset(sc, 1);

	/* Initialize the CORB and RIRB */
	hdac_corb_init(sc);
	hdac_rirb_init(sc);

	/* Start the corb and rirb engines */
	HDA_BOOTVERBOSE(
		device_printf(dev, "Starting CORB Engine...\n");
	);
	hdac_corb_start(sc);
	HDA_BOOTVERBOSE(
		device_printf(dev, "Starting RIRB Engine...\n");
	);
	hdac_rirb_start(sc);

	HDA_BOOTVERBOSE(
		device_printf(dev,
		    "Enabling controller interrupt...\n");
	);
	HDAC_WRITE_4(&sc->mem, HDAC_GCTL, HDAC_READ_4(&sc->mem, HDAC_GCTL) |
	    HDAC_GCTL_UNSOL);
	if (sc->polling == 0) {
		HDAC_WRITE_4(&sc->mem, HDAC_INTCTL,
		    HDAC_INTCTL_CIE | HDAC_INTCTL_GIE);
	} else {
		callout_reset(&sc->poll_hdac, 1, hdac_poll_callback, sc);
	}
	DELAY(1000);

	for (codec_index = 0; codec_index < HDAC_CODEC_MAX; codec_index++) {
		codec = sc->codecs[codec_index];
		if (codec == NULL)
			continue;
		for (fg_index = 0; fg_index < codec->num_fgs; fg_index++) {
			devinfo = &codec->fgs[fg_index];
			if (devinfo->node_type !=
			    HDA_PARAM_FCT_GRP_TYPE_NODE_TYPE_AUDIO) {
				HDA_BOOTVERBOSE(
					device_printf(dev,
					    "Power down unsupported non-audio FG"
					    " cad=%d nid=%d to the D3 state...\n",
					    codec->cad, devinfo->nid);
				);
				hdac_command(sc,
				    HDA_CMD_SET_POWER_STATE(codec->cad,
				    devinfo->nid, HDA_CMD_POWER_STATE_D3),
				    codec->cad);
				continue;
			}

			HDA_BOOTVERBOSE(
				device_printf(dev,
				    "Power up audio FG cad=%d nid=%d...\n",
				    devinfo->codec->cad, devinfo->nid);
			);
			hdac_powerup(devinfo);
			HDA_BOOTVERBOSE(
				device_printf(dev, "AFG commit...\n");
		    	);
			hdac_audio_commit(devinfo);
		    	HDA_BOOTVERBOSE(
				device_printf(dev, "Ctls commit...\n");
			);
			hdac_audio_ctl_commit(devinfo);
		    	HDA_BOOTVERBOSE(
				device_printf(dev, "HP switch init...\n");
			);
			hdac_hp_switch_init(devinfo);

			hdac_unlock(sc);
			for (i = 0; i < devinfo->function.audio.num_devs; i++) {
				struct hdac_pcm_devinfo *pdevinfo = 
				    &devinfo->function.audio.devs[i];
				HDA_BOOTVERBOSE(
					device_printf(pdevinfo->dev,
					    "OSS mixer reinitialization...\n");
				);
				if (mixer_reinit(pdevinfo->dev) == -1)
					device_printf(pdevinfo->dev,
					    "unable to reinitialize the mixer\n");
			}
			hdac_lock(sc);
		}
	}

	HDA_BOOTVERBOSE(
		device_printf(dev, "Start streams...\n");
	);
	for (i = 0; i < sc->num_chans; i++) {
		if (sc->chans[i].flags & HDAC_CHN_SUSPEND) {
			sc->chans[i].flags &= ~HDAC_CHN_SUSPEND;
			hdac_channel_start(sc, &sc->chans[i]);
		}
	}

	hdac_unlock(sc);

	HDA_BOOTVERBOSE(
		device_printf(dev, "Resume done\n");
	);

	return (0);
}
/****************************************************************************
 * int hdac_detach(device_t)
 *
 * Detach and free up resources utilized by the hdac device.
 ****************************************************************************/
static int
hdac_detach(device_t dev)
{
	struct hdac_softc *sc;
	device_t *devlist = NULL;
	int i, devcount;

	sc = device_get_softc(dev);

	device_get_children(dev, &devlist, &devcount);
	for (i = 0; devlist != NULL && i < devcount; i++)
		device_delete_child(dev, devlist[i]);
	if (devlist != NULL)
		free(devlist, M_TEMP);

	hdac_release_resources(sc);

	return (0);
}

static device_method_t hdac_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		hdac_probe),
	DEVMETHOD(device_attach,	hdac_attach),
	DEVMETHOD(device_detach,	hdac_detach),
	DEVMETHOD(device_suspend,	hdac_suspend),
	DEVMETHOD(device_resume,	hdac_resume),
	{ 0, 0 }
};

static driver_t hdac_driver = {
	"hdac",
	hdac_methods,
	sizeof(struct hdac_softc),
};

static devclass_t hdac_devclass;

DRIVER_MODULE(snd_hda, pci, hdac_driver, hdac_devclass, 0, 0);
MODULE_DEPEND(snd_hda, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(snd_hda, 1);

static int
hdac_pcm_probe(device_t dev)
{
	struct hdac_pcm_devinfo *pdevinfo =
	    (struct hdac_pcm_devinfo *)device_get_ivars(dev);
	char buf[128];

	snprintf(buf, sizeof(buf), "HDA codec #%d %s PCM #%d",
	    pdevinfo->devinfo->codec->cad,
	    hdac_codec_name(pdevinfo->devinfo->codec),
	    pdevinfo->index);
	device_set_desc_copy(dev, buf);
	return (0);
}

static int
hdac_pcm_attach(device_t dev)
{
	struct hdac_pcm_devinfo *pdevinfo =
	    (struct hdac_pcm_devinfo *)device_get_ivars(dev);
	struct hdac_softc *sc = pdevinfo->devinfo->codec->sc;
	char status[SND_STATUSLEN];
	int i;

	pdevinfo->chan_size = pcm_getbuffersize(dev,
	    HDA_BUFSZ_MIN, HDA_BUFSZ_DEFAULT, HDA_BUFSZ_MAX);

	HDA_BOOTVERBOSE(
		device_printf(dev, "+--------------------------------------+\n");
		device_printf(dev, "| DUMPING PCM Playback/Record Channels |\n");
		device_printf(dev, "+--------------------------------------+\n");
		hdac_dump_pcmchannels(pdevinfo);
		device_printf(dev, "\n");
		device_printf(dev, "+--------------------------------+\n");
		device_printf(dev, "| DUMPING Playback/Record Pathes |\n");
		device_printf(dev, "+--------------------------------+\n");
		hdac_dump_dac(pdevinfo);
		hdac_dump_adc(pdevinfo);
		hdac_dump_mix(pdevinfo);
		device_printf(dev, "\n");
		device_printf(dev, "+-------------------------+\n");
		device_printf(dev, "| DUMPING Volume Controls |\n");
		device_printf(dev, "+-------------------------+\n");
		hdac_dump_ctls(pdevinfo, "Master Volume", SOUND_MASK_VOLUME);
		hdac_dump_ctls(pdevinfo, "PCM Volume", SOUND_MASK_PCM);
		hdac_dump_ctls(pdevinfo, "CD Volume", SOUND_MASK_CD);
		hdac_dump_ctls(pdevinfo, "Microphone Volume", SOUND_MASK_MIC);
		hdac_dump_ctls(pdevinfo, "Microphone2 Volume", SOUND_MASK_MONITOR);
		hdac_dump_ctls(pdevinfo, "Line-in Volume", SOUND_MASK_LINE);
		hdac_dump_ctls(pdevinfo, "Speaker/Beep Volume", SOUND_MASK_SPEAKER);
		hdac_dump_ctls(pdevinfo, "Recording Level", SOUND_MASK_RECLEV);
		hdac_dump_ctls(pdevinfo, "Input Mix Level", SOUND_MASK_IMIX);
		hdac_dump_ctls(pdevinfo, NULL, 0);
		device_printf(dev, "\n");
	);

	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "blocksize", &i) == 0 && i > 0) {
		i &= HDA_BLK_ALIGN;
		if (i < HDA_BLK_MIN)
			i = HDA_BLK_MIN;
		pdevinfo->chan_blkcnt = pdevinfo->chan_size / i;
		i = 0;
		while (pdevinfo->chan_blkcnt >> i)
			i++;
		pdevinfo->chan_blkcnt = 1 << (i - 1);
		if (pdevinfo->chan_blkcnt < HDA_BDL_MIN)
			pdevinfo->chan_blkcnt = HDA_BDL_MIN;
		else if (pdevinfo->chan_blkcnt > HDA_BDL_MAX)
			pdevinfo->chan_blkcnt = HDA_BDL_MAX;
	} else
		pdevinfo->chan_blkcnt = HDA_BDL_DEFAULT;

	/* 
	 * We don't register interrupt handler with snd_setup_intr
	 * in pcm device. Mark pcm device as MPSAFE manually.
	 */
	pcm_setflags(dev, pcm_getflags(dev) | SD_F_MPSAFE);

	HDA_BOOTVERBOSE(
		device_printf(dev, "OSS mixer initialization...\n");
	);
	if (mixer_init(dev, &hdac_audio_ctl_ossmixer_class, pdevinfo) != 0)
		device_printf(dev, "Can't register mixer\n");

	HDA_BOOTVERBOSE(
		device_printf(dev, "Registering PCM channels...\n");
	);
	if (pcm_register(dev, pdevinfo, (pdevinfo->play >= 0)?1:0,
	    (pdevinfo->rec >= 0)?1:0) != 0)
		device_printf(dev, "Can't register PCM\n");

	pdevinfo->registered++;

	if (pdevinfo->play >= 0)
		pcm_addchan(dev, PCMDIR_PLAY, &hdac_channel_class, pdevinfo);
	if (pdevinfo->rec >= 0)
		pcm_addchan(dev, PCMDIR_REC, &hdac_channel_class, pdevinfo);

	snprintf(status, SND_STATUSLEN, "at %s cad %d %s [%s]",
	    device_get_nameunit(sc->dev), pdevinfo->devinfo->codec->cad,
	    PCM_KLDSTRING(snd_hda), HDA_DRV_TEST_REV);
	pcm_setstatus(dev, status);

	return (0);
}

static int
hdac_pcm_detach(device_t dev)
{
	struct hdac_pcm_devinfo *pdevinfo =
	    (struct hdac_pcm_devinfo *)device_get_ivars(dev);
	int err;

	if (pdevinfo->registered > 0) {
		err = pcm_unregister(dev);
		if (err != 0)
			return (err);
	}

	return (0);
}

static device_method_t hdac_pcm_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		hdac_pcm_probe),
	DEVMETHOD(device_attach,	hdac_pcm_attach),
	DEVMETHOD(device_detach,	hdac_pcm_detach),
	{ 0, 0 }
};

static driver_t hdac_pcm_driver = {
	"pcm",
	hdac_pcm_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_hda_pcm, hdac, hdac_pcm_driver, pcm_devclass, 0, 0);

