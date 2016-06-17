/* $Id: cs4215.h,v 1.8 2000/10/27 07:01:38 uzi Exp $
 * drivers/sbus/audio/cs4215.h
 *
 * Copyright (C) 1997 Rudolf Koenig (rfkoenig@immd4.informatik.uni-erlangen.de)
 * Used with dbri.h
 */

#ifndef _CS4215_H_
#define _CS4215_H_

struct cs4215 {
	__u8	data[4];	/* Data mode: Time slots 5-8 */
	__u8	ctrl[4];	/* Ctrl mode: Time slots 1-4 */
	__u8	onboard;
	__u8	offset;		/* Bit offset from frame sync to time slot 1 */
	volatile __u32	status;
	volatile __u32	version;
};


/*
 * Control mode first 
 */

/* Time Slot 1, Status register */
#define CS4215_CLB	(1<<2)	/* Control Latch Bit */
#define CS4215_OLB	(1<<3)	/* 1: line: 2.0V, speaker 4V */
				/* 0: line: 2.8V, speaker 8V */
#define CS4215_MLB	(1<<4)	/* 1: Microphone: 20dB gain disabled */
#define CS4215_RSRVD_1  (1<<5)


/* Time Slot 2, Data Format Register */
#define CS4215_DFR_LINEAR16	0
#define CS4215_DFR_ULAW		1
#define CS4215_DFR_ALAW		2
#define CS4215_DFR_LINEAR8	3
#define CS4215_DFR_STEREO	(1<<2)
static struct {
	unsigned short freq;
	unsigned char  xtal;
	unsigned char  csval;
} CS4215_FREQ[] = {
	{	 8000,	(1<<4),	(0<<3)	},
	{	16000,	(1<<4),	(1<<3)	},
	{	27429,	(1<<4),	(2<<3)	},	/* Actually 24428.57 */
	{	32000,	(1<<4),	(3<<3)	},
	/* {	 NA,	(1<<4),	(4<<3)	}, */
	/* {	 NA,	(1<<4),	(5<<3)	}, */
	{	48000,	(1<<4),	(6<<3)	},
	{	 9600,	(1<<4),	(7<<3)	},
	{	 5513,	(2<<4),	(0<<3)	},	/* Actually 5512.5 */
	{	11025,	(2<<4),	(1<<3)	},
	{	18900,	(2<<4),	(2<<3)	},
	{	22050,	(2<<4),	(3<<3)	},
	{	37800,	(2<<4),	(4<<3)	},
	{	44100,	(2<<4),	(5<<3)	},
	{	33075,	(2<<4),	(6<<3)	},
	{	 6615,	(2<<4),	(7<<3)	},
	{	    0,	0,	0	}
};
#define CS4215_HPF	(1<<7)	/* High Pass Filter, 1: Enabled */

#define CS4215_12_MASK	0xfcbf	/* Mask off reserved bits in slot 1 & 2 */

/* Time Slot 3, Serial Port Control register */
#define CS4215_XEN	(1<<0)	/* 0: Enable serial output */
#define CS4215_XCLK	(1<<1)	/* 1: Master mode: Generate SCLK */
#define CS4215_BSEL_64	(0<<2)	/* Bitrate: 64 bits per frame */
#define CS4215_BSEL_128	(1<<2)
#define CS4215_BSEL_256	(2<<2)
#define CS4215_MCK_MAST (0<<4)	/* Master clock */
#define CS4215_MCK_XTL1 (1<<4)	/* 24.576 MHz clock source */
#define CS4215_MCK_XTL2 (2<<4)	/* 16.9344 MHz clock source */
#define CS4215_MCK_CLK1 (3<<4)	/* Clockin, 256 x Fs */
#define CS4215_MCK_CLK2 (4<<4)	/* Clockin, see DFR */

/* Time Slot 4, Test Register */
#define CS4215_DAD	(1<<0)	/* 0:Digital-Dig loop, 1:Dig-Analog-Dig loop */
#define CS4215_ENL	(1<<1)	/* Enable Loopback Testing */

/* Time Slot 5, Parallel Port Register */
/* Read only here and the same as the in data mode */

/* Time Slot 6, Reserved  */

/* Time Slot 7, Version Register  */
#define CS4215_VERSION_MASK 0xf	/* Known versions 0/C, 1/D, 2/E */

/* Time Slot 8, Reserved  */



/*
 * Data mode
 */
/* Time Slot 1-2: Left Channel Data, 2-3: Right Channel Data  */

/* Time Slot 5, Output Setting  */
#define CS4215_LO(v)	v	/* Left Output Attenuation 0x3f: -94.5 dB */
#define CS4215_LE	(1<<6)	/* Line Out Enable */
#define CS4215_HE	(1<<7)	/* Headphone Enable */

/* Time Slot 6, Output Setting  */
#define CS4215_RO(v)	v	/* Right Output Attenuation 0x3f: -94.5 dB */
#define CS4215_SE	(1<<6)	/* Speaker Enable */
#define CS4215_ADI	(1<<7)	/* A/D Data Invalid: Busy in calibration */

/* Time Slot 7, Input Setting */
#define CS4215_LG(v)	v	/* Left Gain Setting 0xf: 22.5 dB */
#define CS4215_IS	(1<<4)	/* Input Select: 1=Microphone, 0=Line */
#define CS4215_OVR	(1<<5)	/* 1: Overrange condition occurred */
#define CS4215_PIO0	(1<<6)	/* Parallel I/O 0 */
#define CS4215_PIO1	(1<<7)

/* Time Slot 8, Input Setting */
#define CS4215_RG(v)	v	/* Right Gain Setting 0xf: 22.5 dB */
#define CS4215_MA(v)	(v<<4)	/* Monitor Path Attenuation 0xf: mute */

#endif /* _CS4215_H_ */
