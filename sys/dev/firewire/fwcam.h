/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_FIREWIRE_FWCAM_H_
#define _DEV_FIREWIRE_FWCAM_H_

/*
 * IIDC 1394-based Digital Camera Specification v1.30
 * Register offsets relative to command_regs_base
 */

/* Section 1.1 - Camera initialize register */
#define IIDC_INITIALIZE		0x000

/* Section 1.2 - Inquiry registers for video format/mode/frame rate */
#define IIDC_V_FORMAT_INQ	0x100
#define IIDC_V_MODE_INQ(f)	(0x180 + (f) * 4)
#define IIDC_V_RATE_INQ(f, m)	(0x200 + (f) * 0x20 + (m) * 4)

/* Section 1.3 - Inquiry register for basic function */
#define IIDC_BASIC_FUNC_INQ	0x400

/* Section 1.4 - Inquiry registers for feature presence */
#define IIDC_FEATURE_HI_INQ	0x404
#define IIDC_FEATURE_LO_INQ	0x408

/* Section 1.5 - Inquiry registers for feature elements (per feature) */
#define IIDC_BRIGHTNESS_INQ	0x500
#define IIDC_AUTO_EXPOSURE_INQ	0x504
#define IIDC_SHARPNESS_INQ	0x508
#define IIDC_WHITE_BAL_INQ	0x50C
#define IIDC_HUE_INQ		0x510
#define IIDC_SATURATION_INQ	0x514
#define IIDC_GAMMA_INQ		0x518
#define IIDC_SHUTTER_INQ	0x51C
#define IIDC_GAIN_INQ		0x520
#define IIDC_IRIS_INQ		0x524
#define IIDC_FOCUS_INQ		0x528
#define IIDC_TEMPERATURE_INQ	0x52C
#define IIDC_TRIGGER_INQ	0x530

/* Section 1.6 - Status and control registers for camera */
#define IIDC_CUR_V_FRM_RATE	0x600
#define IIDC_CUR_V_MODE		0x604
#define IIDC_CUR_V_FORMAT	0x608
#define IIDC_ISO_CHANNEL	0x60C
#define IIDC_CAMERA_POWER	0x610
#define IIDC_ISO_EN		0x614
#define IIDC_MEMORY_SAVE	0x618
#define IIDC_ONE_SHOT		0x61C
#define IIDC_MEM_SAVE_CH	0x620
#define IIDC_CUR_MEM_CH		0x624

/* Section 1.7 - Status and control register for features */
#define IIDC_BRIGHTNESS		0x800
#define IIDC_AUTO_EXPOSURE	0x804
#define IIDC_SHARPNESS		0x808
#define IIDC_WHITE_BALANCE	0x80C
#define IIDC_HUE		0x810
#define IIDC_SATURATION		0x814
#define IIDC_GAMMA		0x818
#define IIDC_SHUTTER		0x81C
#define IIDC_GAIN		0x820
#define IIDC_IRIS		0x824
#define IIDC_FOCUS		0x828
#define IIDC_TEMPERATURE	0x82C
#define IIDC_TRIGGER_MODE	0x830
#define IIDC_ZOOM		0x880
#define IIDC_PAN		0x884
#define IIDC_TILT		0x888

/* Video format indices (CUR_V_FORMAT register) */
#define IIDC_FMT_VGA		0	/* Format_0: VGA non-compressed */
#define IIDC_FMT_SVGA1		1	/* Format_1: Super VGA (1) */
#define IIDC_FMT_SVGA2		2	/* Format_2: Super VGA (2) */
#define IIDC_FMT_STILL		6	/* Format_6: Still image */
#define IIDC_FMT_PARTIAL	7	/* Format_7: Partial/scalable */

/* V_FORMAT_INQ bits */
#define IIDC_FORMAT_VGA		(1 << 31)	/* Format_0: VGA */
#define IIDC_FORMAT_SVGA1	(1 << 30)	/* Format_1: Super VGA (1) */
#define IIDC_FORMAT_SVGA2	(1 << 29)	/* Format_2: Super VGA (2) */
#define IIDC_FORMAT_STILL	(1 << 25)	/* Format_6: Still image */
#define IIDC_FORMAT_PARTIAL	(1 << 24)	/* Format_7: Partial image */

/* ISO_CHANNEL register fields */
#define IIDC_ISO_CH_MASK	0xf0000000	/* bits [0..3] */
#define IIDC_ISO_CH_SHIFT	28
#define IIDC_ISO_SPEED_MASK	0x03000000	/* bits [6..7] */
#define IIDC_ISO_SPEED_SHIFT	24

/* CAMERA_POWER register */
#define IIDC_POWER_ON		(1 << 31)	/* bit [0] */

/* ISO_EN register */
#define IIDC_ISO_EN_ON		(1 << 31)	/* bit [0] */

/* BASIC_FUNC_INQ bits */
#define IIDC_ADV_FEATURE_INQ	(1 << 31)	/* bit [0] */
#define IIDC_CAM_POWER_CTRL	(1 << 15)	/* bit [16] */
#define IIDC_ONE_SHOT_INQ	(1 << 12)	/* bit [19] */
#define IIDC_MULTI_SHOT_INQ	(1 << 11)	/* bit [20] */

/* FEATURE_HI_INQ bits (feature presence, section 1.4) */
#define IIDC_HAS_BRIGHTNESS	(1 << 31)
#define IIDC_HAS_AUTO_EXPOSURE	(1 << 30)
#define IIDC_HAS_SHARPNESS	(1 << 29)
#define IIDC_HAS_WHITE_BALANCE	(1 << 28)
#define IIDC_HAS_HUE		(1 << 27)
#define IIDC_HAS_SATURATION	(1 << 26)
#define IIDC_HAS_GAMMA		(1 << 25)
#define IIDC_HAS_SHUTTER	(1 << 24)
#define IIDC_HAS_GAIN		(1 << 23)
#define IIDC_HAS_IRIS		(1 << 22)
#define IIDC_HAS_FOCUS		(1 << 21)
#define IIDC_HAS_TEMPERATURE	(1 << 20)
#define IIDC_HAS_TRIGGER	(1 << 19)

/* Config ROM: IIDC unit-dependent directory command_regs_base key */
#define IIDC_CROM_CMD_BASE	(CSRTYPE_C | 0x00)	/* 0x40 */

/*
 * Frame size limits for Format_0 (VGA non-compressed):
 * Mode_1: 320x240 YUV422 = 153,600 bytes
 * Mode_2: 640x480 YUV411 = 460,800 bytes
 * Mode_3: 640x480 YUV422 = 614,400 bytes
 */
/*
 * ioctl interface
 */
struct fwcam_mode {
	uint8_t		format;		/* IIDC video format (0-7) */
	uint8_t		mode;		/* IIDC video mode (0-7) */
	uint8_t		framerate;	/* IIDC frame rate (0-7) */
	uint8_t		_pad;
	uint32_t	frame_size;	/* computed frame size in bytes (read-only) */
};

struct fwcam_feature {
	uint32_t	id;		/* FWCAM_FEAT_* */
	uint32_t	flags;		/* FWCAM_FEATF_* (from INQ, read-only) */
	uint32_t	min;		/* minimum value (from INQ, read-only) */
	uint32_t	max;		/* maximum value (from INQ, read-only) */
	uint32_t	value;		/* current value / value to set */
	uint32_t	value2;		/* second value (white balance V) */
};

/* Feature IDs (index into IIDC feature register space) */
#define FWCAM_FEAT_BRIGHTNESS	0
#define FWCAM_FEAT_AUTO_EXPOSURE 1
#define FWCAM_FEAT_SHARPNESS	2
#define FWCAM_FEAT_WHITE_BALANCE 3
#define FWCAM_FEAT_HUE		4
#define FWCAM_FEAT_SATURATION	5
#define FWCAM_FEAT_GAMMA	6
#define FWCAM_FEAT_SHUTTER	7
#define FWCAM_FEAT_GAIN		8
#define FWCAM_FEAT_IRIS		9
#define FWCAM_FEAT_FOCUS	10
#define FWCAM_FEAT_TEMPERATURE	11
#define FWCAM_FEAT_TRIGGER	12
#define FWCAM_FEAT_ZOOM		13
#define FWCAM_FEAT_PAN		14
#define FWCAM_FEAT_TILT		15
#define FWCAM_FEAT_MAX		16

/* Feature flags (from INQ register bits) */
#define FWCAM_FEATF_PRESENT	(1 << 0)  /* feature is present */
#define FWCAM_FEATF_ONOFF	(1 << 1)  /* supports on/off */
#define FWCAM_FEATF_AUTO	(1 << 2)  /* supports auto mode */
#define FWCAM_FEATF_MANUAL	(1 << 3)  /* supports manual mode */

struct fwcam_info {
	uint32_t	formats;	/* V_FORMAT_INQ bitmask */
	uint32_t	basic_func;	/* BASIC_FUNC_INQ */
	uint32_t	features_hi;	/* FEATURE_HI_INQ */
	uint32_t	features_lo;	/* FEATURE_LO_INQ */
	uint8_t		cur_format;
	uint8_t		cur_mode;
	uint8_t		cur_framerate;
	uint8_t		state;		/* FWCAM_STATE_* */
	uint32_t	frame_size;
	uint32_t	frame_dropped;
	uint8_t		iso_channel;	/* active ISO receive channel */
	uint8_t		_pad[3];
};

#define FWCAM_GMODE	_IOR('C', 1, struct fwcam_mode)
#define FWCAM_SMODE	_IOWR('C', 2, struct fwcam_mode)
#define FWCAM_GFEAT	_IOWR('C', 3, struct fwcam_feature)
#define FWCAM_SFEAT	_IOW('C', 4, struct fwcam_feature)
#define FWCAM_GINFO	_IOR('C', 5, struct fwcam_info)

/* fwcam state values (visible to userland via fwcam_info.state) */
#define FWCAM_STATE_IDLE	0
#define FWCAM_STATE_PROBED	1
#define FWCAM_STATE_STREAMING	2
#define FWCAM_STATE_DETACHING	3

/*
 * Internal constants
 */
#define FWCAM_MAX_FRAME_SIZE	(640 * 480 * 3)	/* 921,600 (RGB24) */
#define FWCAM_ISO_NCHUNK	256	/* receive DMA chunks */
#define FWCAM_ISO_PKTSIZE	2048	/* max iso packet size (MCLBYTES) */

#ifdef _KERNEL
struct fwcam_softc {
	struct firewire_dev_comm fd;	/* must be first */
	struct mtx		mtx;
	struct cdev		*cdev;

	/* Remote camera node */
	struct fw_device	*fwdev;

	/* IIDC command register addressing */
	uint16_t		cmd_hi;		/* always 0xffff */
	uint32_t		cmd_lo;		/* 0xf0000000 | (base << 2) */

	/* Capabilities from INQ registers */
	uint32_t		formats;	/* V_FORMAT_INQ */
	uint32_t		basic_func;	/* BASIC_FUNC_INQ */
	uint32_t		features_hi;	/* FEATURE_HI_INQ */
	uint32_t		features_lo;	/* FEATURE_LO_INQ */

	/* Current settings */
	uint8_t			cur_format;
	uint8_t			cur_mode;
	uint8_t			cur_framerate;
	uint8_t			iso_channel;
	uint8_t			iso_speed;

	/* Deferred probe task */
	struct task		probe_task;

	/* Isochronous receive */
	int			dma_ch;		/* IR DMA channel, -1 if none */
	int			iso_active;	/* iso_input running */

	/* Frame assembly (double buffer) */
	uint8_t			*frame_buf;	/* frame being assembled */
	uint8_t			*read_buf;	/* completed frame for read() */
	uint32_t		frame_size;	/* expected frame size (bytes) */
	uint32_t		frame_offset;	/* write position in frame_buf */
	int			frame_ready;	/* read_buf has valid frame */
	int			read_in_progress; /* uiomove active on read_buf */
	int			frame_dropped;	/* dropped frame count */
	int			open_count;	/* cdev open count */
	struct selinfo		rsel;		/* poll/select/kqueue */

	/* State: one of FWCAM_STATE_* */
	int			state;
};

#define FWCAM_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define FWCAM_UNLOCK(sc)	mtx_unlock(&(sc)->mtx)

#endif /* _KERNEL */

#endif /* _DEV_FIREWIRE_FWCAM_H_ */
