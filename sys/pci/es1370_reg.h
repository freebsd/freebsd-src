/*
 * This supports the ENSONIQ AudioPCI board based on the ES1370.
 *
 * Copyright (c) 1998 Joachim Kuebart <joki@kuebart.stuttgart.netsurf.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Joachim Kuebart.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $FreeBSD$
 */

#ifndef _ES1370_REG_H
#define _ES1370_REG_H

#define ES1370_REG_CONTROL		0x00
#define ES1370_REG_STATUS		0x04
#define ES1370_REG_UART_DATA		0x08
#define ES1370_REG_UART_STATUS		0x09
#define ES1370_REG_UART_CONTROL		0x09
#define ES1370_REG_UART_TEST		0x0a
#define ES1370_REG_MEMPAGE		0x0c
#define ES1370_REG_CODEC		0x10
#define CODEC_INDEX_SHIFT		8
#define ES1370_REG_SERIAL_CONTROL	0x20
#define ES1370_REG_DAC1_SCOUNT		0x24
#define ES1370_REG_DAC2_SCOUNT		0x28
#define ES1370_REG_ADC_SCOUNT		0x2c

#define ES1370_REG_DAC1_FRAMEADR	0xc30
#define ES1370_REG_DAC1_FRAMECNT	0xc34
#define ES1370_REG_DAC2_FRAMEADR	0xc38
#define ES1370_REG_DAC2_FRAMECNT	0xc3c
#define ES1370_REG_ADC_FRAMEADR		0xd30
#define ES1370_REG_ADC_FRAMECNT		0xd34

#define DAC2_SRTODIV(x)	(((1411200 + (x) / 2) / (x) - 2) & 0x1fff)
#define DAC2_DIVTOSR(x)	(1411200 / ((x) + 2))

#define CTRL_ADC_STOP   0x80000000	/* 1 = ADC stopped */
#define CTRL_XCTL1      0x40000000	/* SERR pin if enabled */
#define CTRL_OPEN       0x20000000	/* no function, can be read and
					 * written */
#define CTRL_PCLKDIV    0x1fff0000	/* ADC/DAC2 clock divider */
#define CTRL_SH_PCLKDIV 16
#define CTRL_MSFMTSEL   0x00008000	/* MPEG serial data fmt: 0 = Sony, 1
					 * = I2S */
#define CTRL_M_SBB      0x00004000	/* DAC2 clock: 0 = PCLKDIV, 1 = MPEG */
#define CTRL_WTSRSEL    0x00003000	/* DAC1 clock freq: 0=5512, 1=11025,
					 * 2=22050, 3=44100 */
#define CTRL_SH_WTSRSEL 12
#define CTRL_DAC_SYNC   0x00000800	/* 1 = DAC2 runs off DAC1 clock */
#define CTRL_CCB_INTRM  0x00000400	/* 1 = CCB "voice" ints enabled */
#define CTRL_M_CB       0x00000200	/* recording source: 0 = ADC, 1 =
					 * MPEG */
#define CTRL_XCTL0      0x00000100	/* 0 = Line in, 1 = Line out */
#define CTRL_BREQ       0x00000080	/* 1 = test mode (internal mem test) */
#define CTRL_DAC1_EN    0x00000040	/* enable DAC1 */
#define CTRL_DAC2_EN    0x00000020	/* enable DAC2 */
#define CTRL_ADC_EN     0x00000010	/* enable ADC */
#define CTRL_UART_EN    0x00000008	/* enable MIDI uart */
#define CTRL_JYSTK_EN   0x00000004	/* enable Joystick port (presumably
					 * at address 0x200) */
#define CTRL_CDC_EN     0x00000002	/* enable serial (CODEC) interface */
#define CTRL_SERR_DIS   0x00000001	/* 1 = disable PCI SERR signal */

#define SCTRL_P2ENDINC    0x00380000	/* */
#define SCTRL_SH_P2ENDINC 19
#define SCTRL_P2STINC     0x00070000	/* */
#define SCTRL_SH_P2STINC  16
#define SCTRL_R1LOOPSEL   0x00008000	/* 0 = loop mode */
#define SCTRL_P2LOOPSEL   0x00004000	/* 0 = loop mode */
#define SCTRL_P1LOOPSEL   0x00002000	/* 0 = loop mode */
#define SCTRL_P2PAUSE     0x00001000	/* 1 = pause mode */
#define SCTRL_P1PAUSE     0x00000800	/* 1 = pause mode */
#define SCTRL_R1INTEN     0x00000400	/* enable interrupt */
#define SCTRL_P2INTEN     0x00000200	/* enable interrupt */
#define SCTRL_P1INTEN     0x00000100	/* enable interrupt */
#define SCTRL_P1SCTRLD    0x00000080	/* reload sample count register for
					 * DAC1 */
#define SCTRL_P2DACSEN    0x00000040	/* 1 = DAC2 play back last sample
					 * when disabled */
#define SCTRL_R1SEB       0x00000020	/* 1 = 16bit */
#define SCTRL_R1SMB       0x00000010	/* 1 = stereo */
#define SCTRL_R1FMT       0x00000030	/* format mask */
#define SCTRL_SH_R1FMT    4
#define SCTRL_P2SEB       0x00000008	/* 1 = 16bit */
#define SCTRL_P2SMB       0x00000004	/* 1 = stereo */
#define SCTRL_P2FMT       0x0000000c	/* format mask */
#define SCTRL_SH_P2FMT    2
#define SCTRL_P1SEB       0x00000002	/* 1 = 16bit */
#define SCTRL_P1SMB       0x00000001	/* 1 = stereo */
#define SCTRL_P1FMT       0x00000003	/* format mask */
#define SCTRL_SH_P1FMT    0

#define STAT_INTR       0x80000000	/* wired or of all interrupt bits */
#define STAT_CSTAT      0x00000400	/* 1 = codec busy or codec write in
					 * progress */
#define STAT_CBUSY      0x00000200	/* 1 = codec busy */
#define STAT_CWRIP      0x00000100	/* 1 = codec write in progress */
#define STAT_VC         0x00000060	/* CCB int source, 0=DAC1, 1=DAC2,
					 * 2=ADC, 3=undef */
#define STAT_SH_VC      5
#define STAT_MCCB       0x00000010	/* CCB int pending */
#define STAT_UART       0x00000008	/* UART int pending */
#define STAT_DAC1       0x00000004	/* DAC1 int pending */
#define STAT_DAC2       0x00000002	/* DAC2 int pending */
#define STAT_ADC        0x00000001	/* ADC int pending */

#define CODEC_OMIX1	0x10
#define CODEC_OMIX2	0x11
#define CODEC_LIMIX1	0x12
#define CODEC_RIMIX1	0x13
#define CODEC_LIMIX2	0x14
#define CODEC_RIMIX2	0x15
#define CODEC_RES_PD	0x16
#define CODEC_CSEL	0x17
#define CODEC_ADSEL	0x18
#define CODEC_MGAIN	0x19

#define ES_BUFFSIZE 0x20000		/* We're PCI! Use a large buffer */

struct es_info {
        bus_space_tag_t st;
        bus_space_handle_t sh;

        bus_dma_tag_t   parent_dmat;
        bus_dmamap_t    dmam_in, dmam_out;

        /* Contents of board's registers */
        u_long          ctrl;
        u_long          sctrl;
};

/* es1371.c functions */
u_int          es1371_wait_src_ready(snddev_info *);
void           es1371_src_write(snddev_info *, u_short, u_short);
u_int          es1371_adc_rate(snddev_info *, u_int, int);
u_int          es1371_dac1_rate(snddev_info *, u_int, int);
u_int          es1371_dac2_rate(snddev_info *, u_int, int);
int            mixer_rdch(snddev_info *s, unsigned int ch, int *arg);
int            mixer_wrch(snddev_info *s, unsigned int ch, int val);
void           wrcodec(snddev_info *s, unsigned addr, unsigned data);
unsigned       rdcodec(snddev_info *s, unsigned addr);
int            mixer_ioctl_1371(snddev_info *, u_long, caddr_t, int,  struct proc *);
int            es_init_1371(snddev_info *);

#ifndef OSS_GETVERSION
#define OSS_GETVERSION _IOR ('M', 118, int)
#endif

int es_debug;	/* set via sysctl to enable debugging messages */
#endif
