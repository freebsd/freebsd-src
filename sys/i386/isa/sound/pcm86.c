/*
 * PC-9801-86 PCM driver for FreeBSD(98).
 *
 * Copyright (c) 1995  NAGAO Tadaaki (ABTK)
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: pcm86.c,v 2.4 1996/01/24 19:53:34 abtk Exp $
 */

/*
 * !! NOTE !! :
 *   This file DOES NOT belong to the VoxWare distribution though it works
 *   as part of the VoxWare drivers.  It is FreeBSD(98) original.
 *   -- Nagao (nagao@cs.titech.ac.jp)
 */


#include <i386/isa/sound/sound_config.h>

#ifdef CONFIGURE_SOUNDCARD

#if !defined(EXCLUDE_PCM86) && !defined(EXCLUDE_AUDIO)


/*
 * Constants
 */

#define YES		1
#define NO		0

#define IMODE_NONE	0
#define IMODE_INPUT	1
#define IMODE_OUTPUT	2

/* PC-9801-86 specific constants */
#define	PCM86_IOBASE	0xa460	/* PCM I/O ports */
#define PCM86_FIFOSIZE	32768	/* There is a 32kB FIFO buffer on 86-board */

/* XXX -- These values should be chosen appropriately. */
#define PCM86_INTRSIZE_OUT	1024
#define PCM86_INTRSIZE_IN	(PCM86_FIFOSIZE / 2 - 128)
#define DEFAULT_VOLUME		15	/* 0(min) - 15(max) */


/*
 * Switches for debugging and experiments
 */

/* #define PCM86_DEBUG */

#ifdef PCM86_DEBUG
# ifdef DEB
#  undef DEB
# endif
# define DEB(x) x
#endif


/*
 * Private variables and types
 */

typedef unsigned char pcm_data;

enum board_type {
    NO_SUPPORTED_BOARD = 0,
    PC980186_FAMILY = 1,
    PC980173_FAMILY = 2
};

static char *board_name[] = {
    /* Each must be of the length less than 32 bytes. */
    "No supported board",
    "PC-9801-86 soundboard",
    "PC-9801-73 soundboard"
};

/* Current status of the driver */
static struct {
    int			iobase;
    int			irq;
    enum board_type	board_type;
    int			opened;
    int			format;
    int			bytes;
    int			chipspeedno;
    int			chipspeed;
    int			speed;
    int			stereo;
    int			volume;
    int			intr_busy;
    int			intr_size;
    int			intr_mode;
    int			intr_last;
    int			intr_trailer;
    pcm_data *		pdma_buf;
    int			pdma_count;
    int			pdma_chunkcount;
    int			acc;
    int			last_l;
    int			last_r;
} pcm_s;

static struct {
    pcm_data		buff[4];
    int			size;
} tmpbuf;

static int my_dev = 0;
static char pcm_initialized = NO;

/* 86-board supports only the following rates. */
static int rates_tbl[8] = {
#ifndef WAVEMASTER_FREQ
    44100, 33075, 22050, 16538, 11025, 8269, 5513, 4134
#else
    /*
     * It is said that Q-Vision's WaveMaster of some earlier lot(s?) has
     * sampling rates incompatible with PC-9801-86.
     * But I'm not sure whether the following rates are correct, especially
     * 4000Hz.
     */
    44100, 33075, 22050, 16000, 11025, 8000, 5510, 4000
#endif
};

/* u-law to linear translation table */
static pcm_data ulaw2linear[256] = {
    130, 134, 138, 142, 146, 150, 154, 158, 
    162, 166, 170, 174, 178, 182, 186, 190, 
    193, 195, 197, 199, 201, 203, 205, 207, 
    209, 211, 213, 215, 217, 219, 221, 223, 
    225, 226, 227, 228, 229, 230, 231, 232, 
    233, 234, 235, 236, 237, 238, 239, 240, 
    240, 241, 241, 242, 242, 243, 243, 244, 
    244, 245, 245, 246, 246, 247, 247, 248, 
    248, 248, 249, 249, 249, 249, 250, 250, 
    250, 250, 251, 251, 251, 251, 252, 252, 
    252, 252, 252, 252, 253, 253, 253, 253, 
    253, 253, 253, 253, 254, 254, 254, 254, 
    254, 254, 254, 254, 254, 254, 254, 254, 
    255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 255, 255, 255, 255, 
    125, 121, 117, 113, 109, 105, 101,  97, 
     93,  89,  85,  81,  77,  73,  69,  65, 
     62,  60,  58,  56,  54,  52,  50,  48, 
     46,  44,  42,  40,  38,  36,  34,  32, 
     30,  29,  28,  27,  26,  25,  24,  23, 
     22,  21,  20,  19,  18,  17,  16,  15, 
     15,  14,  14,  13,  13,  12,  12,  11, 
     11,  10,  10,   9,   9,   8,   8,   7, 
      7,   7,   6,   6,   6,   6,   5,   5, 
      5,   5,   4,   4,   4,   4,   3,   3, 
      3,   3,   3,   3,   2,   2,   2,   2, 
      2,   2,   2,   2,   1,   1,   1,   1, 
      1,   1,   1,   1,   1,   1,   1,   1, 
      0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0,   0,   0
};

/* linear to u-law translation table */
static pcm_data linear2ulaw[256] = {
    255, 231, 219, 211, 205, 201, 197, 193, 
    190, 188, 186, 184, 182, 180, 178, 176, 
    175, 174, 173, 172, 171, 170, 169, 168, 
    167, 166, 165, 164, 163, 162, 161, 160, 
    159, 159, 158, 158, 157, 157, 156, 156, 
    155, 155, 154, 154, 153, 153, 152, 152, 
    151, 151, 150, 150, 149, 149, 148, 148, 
    147, 147, 146, 146, 145, 145, 144, 144, 
    143, 143, 143, 143, 142, 142, 142, 142, 
    141, 141, 141, 141, 140, 140, 140, 140, 
    139, 139, 139, 139, 138, 138, 138, 138, 
    137, 137, 137, 137, 136, 136, 136, 136, 
    135, 135, 135, 135, 134, 134, 134, 134, 
    133, 133, 133, 133, 132, 132, 132, 132, 
    131, 131, 131, 131, 130, 130, 130, 130, 
    129, 129, 129, 129, 128, 128, 128, 128, 
      0,   0,   0,   0,   0,   1,   1,   1, 
      1,   2,   2,   2,   2,   3,   3,   3, 
      3,   4,   4,   4,   4,   5,   5,   5, 
      5,   6,   6,   6,   6,   7,   7,   7, 
      7,   8,   8,   8,   8,   9,   9,   9, 
      9,  10,  10,  10,  10,  11,  11,  11, 
     11,  12,  12,  12,  12,  13,  13,  13, 
     13,  14,  14,  14,  14,  15,  15,  15, 
     15,  16,  16,  17,  17,  18,  18,  19, 
     19,  20,  20,  21,  21,  22,  22,  23, 
     23,  24,  24,  25,  25,  26,  26,  27, 
     27,  28,  28,  29,  29,  30,  30,  31, 
     31,  32,  33,  34,  35,  36,  37,  38, 
     39,  40,  41,  42,  43,  44,  45,  46, 
     47,  48,  50,  52,  54,  56,  58,  60, 
     62,  65,  69,  73,  77,  83,  91, 103
};


/*
 * Prototypes
 */

static int pcm86_detect(struct address_info *);

static int pcm86_open(int, int);
static void pcm86_close(int);
static void pcm86_output_block(int, unsigned long, int, int, int);
static void pcm86_start_input(int, unsigned long, int, int, int);
static int pcm86_ioctl(int, unsigned int, unsigned int, int);
static int pcm86_prepare_for_input(int, int, int);
static int pcm86_prepare_for_output(int, int, int);
static void pcm86_reset(int);
static void pcm86_halt_xfer(int);

static void dsp73_send_command(unsigned char);
static void dsp73_send_data(unsigned char);
static void dsp73_init(void);
static int set_format(int);
static int set_speed(int);
static int set_stereo(int);
static void set_volume(int);
static void fifo_start(int);
static void fifo_stop(void);
static void fifo_reset(void);
static void fifo_output_block(void);
static int fifo_send(pcm_data *, int);
static void fifo_sendtrailer(int);
static void fifo_send_stereo(pcm_data *, int);
static void fifo_send_monoral(pcm_data *, int);
static void fifo_send_stereo_ulaw(pcm_data *, int);
static void fifo_send_stereo_8(pcm_data *, int, int);
static void fifo_send_stereo_16le(pcm_data *, int, int);
static void fifo_send_stereo_16be(pcm_data *, int, int);
static void fifo_send_mono_ulaw(pcm_data *, int);
static void fifo_send_mono_8(pcm_data *, int, int);
static void fifo_send_mono_16le(pcm_data *, int, int);
static void fifo_send_mono_16be(pcm_data *, int, int);
static void fifo_input_block(void);
static void fifo_recv(pcm_data *, int);
static void fifo_recv_stereo(pcm_data *, int);
static void fifo_recv_monoral(pcm_data *, int);
static void fifo_recv_stereo_ulaw(pcm_data *, int);
static void fifo_recv_stereo_8(pcm_data *, int, int);
static void fifo_recv_stereo_16le(pcm_data *, int, int);
static void fifo_recv_stereo_16be(pcm_data *, int, int);
static void fifo_recv_mono_ulaw(pcm_data *, int);
static void fifo_recv_mono_8(pcm_data *, int, int);
static void fifo_recv_mono_16le(pcm_data *, int, int);
static void fifo_recv_mono_16be(pcm_data *, int, int);
static void pcm_stop(void);
static void pcm_init(void);


/*
 * Identity
 */

static struct audio_operations pcm86_operations =
{
    "PC-9801-86 SoundBoard", /* filled in properly by auto configuration */
    NOTHING_SPECIAL,
    ( AFMT_MU_LAW |
      AFMT_U8 | AFMT_S16_LE | AFMT_S16_BE |
      AFMT_S8 | AFMT_U16_LE | AFMT_U16_BE ),
    NULL,
    pcm86_open,
    pcm86_close,
    pcm86_output_block,
    pcm86_start_input,
    pcm86_ioctl,
    pcm86_prepare_for_input,
    pcm86_prepare_for_output,
    pcm86_reset,
    pcm86_halt_xfer,
    NULL,
    NULL
};


/*
 * Codes for internal use
 */

static void
dsp73_send_command(unsigned char command)
{
    /* wait for RDY */
    while ((inb(pcm_s.iobase + 2) & 0x48) != 8);

    /* command mode */
    outb(pcm_s.iobase + 2, (inb(pcm_s.iobase + 2) & 0x20) | 3);

    /* wait for RDY */
    while ((inb(pcm_s.iobase + 2) & 0x48) != 8);

    /* send command */
    outb(pcm_s.iobase + 4, command);
}


static void
dsp73_send_data(unsigned char data)
{
    /* wait for RDY */
    while ((inb(pcm_s.iobase + 2) & 0x48) != 8);

    /* data mode */
    outb(pcm_s.iobase + 2, (inb(pcm_s.iobase + 2) & 0x20) | 0x83);

    /* wait for RDY */
    while ((inb(pcm_s.iobase + 2) & 0x48) != 8);

    /* send command */
    outb(pcm_s.iobase + 4, data);
}


static void
dsp73_init(void)
{
    const unsigned char dspinst[15] = {
	0x00, 0x00, 0x27,
	0x3f, 0xe0, 0x01,
	0x00, 0x00, 0x27,
	0x36, 0x5a, 0x0d,
	0x3e, 0x60, 0x04
    };
    unsigned char t;
    int i;

    /* reset DSP */
    t = inb(pcm_s.iobase + 2);
    outb(pcm_s.iobase + 2, (t & 0x80) | 0x23);

    /* mute on */
    dsp73_send_command(0x04);
    dsp73_send_data(0x6f);
    dsp73_send_data(0x3c);

    /* write DSP instructions */
    dsp73_send_command(0x01);
    dsp73_send_data(0x00);
    for (i = 0; i < 16; i++)
	dsp73_send_data(dspinst[i]);

    /* mute off */
    dsp73_send_command(0x04);
    dsp73_send_data(0x6f);
    dsp73_send_data(0x30);

    /* wait for RDY */
    while ((inb(pcm_s.iobase + 2) & 0x48) != 8);

    outb(pcm_s.iobase + 2, 3);
}


static int
set_format(int format)
{
    switch (format) {
    case AFMT_MU_LAW:
    case AFMT_S8:
    case AFMT_U8:
	pcm_s.format = format;
	pcm_s.bytes = 1;	/* 8bit */
	break;
    case AFMT_S16_LE:
    case AFMT_U16_LE:
    case AFMT_S16_BE:
    case AFMT_U16_BE:
	pcm_s.format = format;
	pcm_s.bytes = 2;	/* 16bit */
	break;
    case AFMT_QUERY:
	break;
    default:
	return -1;
    }

    return pcm_s.format;
}


static int
set_speed(int speed)
{
    int i;

    if (speed < 4000)	/* Minimum 4000Hz */
	speed = 4000;
    if (speed > 44100)	/* Maximum 44100Hz */
	speed = 44100;
    for (i = 7; i >= 0; i--) {
	if (speed <= rates_tbl[i]) {
	    pcm_s.chipspeedno = i;
	    pcm_s.chipspeed = rates_tbl[i];
	    break;
	}
    }
    pcm_s.speed = speed;

    return speed;
}


static int
set_stereo(int stereo)
{
    pcm_s.stereo = stereo ? YES : NO;

    return pcm_s.stereo;
}


static void
set_volume(int volume)
{
    if (volume < 0)
	volume = 0;
    if (volume > 15)
	volume = 15;
    pcm_s.volume = volume;

    outb(pcm_s.iobase + 6, 0xaf - volume);	/* D/A -> LINE OUT */
    outb(0x5f,0);
    outb(0x5f,0);
    outb(0x5f,0);
    outb(0x5f,0);
    outb(pcm_s.iobase + 6, 0x20);		/* FM -> A/D */
    outb(0x5f,0);
    outb(0x5f,0);
    outb(0x5f,0);
    outb(0x5f,0);
    outb(pcm_s.iobase + 6, 0x60);		/* LINE IN -> A/D */
    outb(0x5f,0);
    outb(0x5f,0);
    outb(0x5f,0);
    outb(0x5f,0);
}


static void
fifo_start(int mode)
{
    unsigned char tmp;

    /* Set frame length & panpot(LR). */
    tmp = inb(pcm_s.iobase + 10) & 0x88;
    outb(pcm_s.iobase + 10, tmp | ((pcm_s.bytes == 1) ? 0x72 : 0x32));

    tmp = pcm_s.chipspeedno;
    if (mode == IMODE_INPUT)
	tmp |= 0x40;

    /* Reset intr. flag. */
    outb(pcm_s.iobase + 8, tmp);
    outb(pcm_s.iobase + 8, tmp | 0x10);

    /* Enable FIFO intr. */
    outb(pcm_s.iobase + 8, tmp | 0x30);

    /* Set intr. interval. */
    outb(pcm_s.iobase + 10, pcm_s.intr_size / 128 - 1);

    /* Start intr. */
    outb(pcm_s.iobase + 8, tmp | 0xb0);
}


static void
fifo_stop(void)
{
    unsigned char tmp;

    /* Reset intr. flag, and disable FIFO intr. */
    tmp = inb(pcm_s.iobase + 8) & 0x0f;
    outb(pcm_s.iobase + 8, tmp);
}


static void
fifo_reset(void)
{
    unsigned char tmp;

    /* Reset FIFO. */
    tmp = inb(pcm_s.iobase + 8) & 0x77;
    outb(pcm_s.iobase + 8, tmp | 0x8);
    outb(pcm_s.iobase + 8, tmp);
}


static void
fifo_output_block(void)
{
    int chunksize, count;

    if (pcm_s.pdma_chunkcount) {
	/* Update chunksize and then send the next chunk to FIFO. */
	chunksize = pcm_s.pdma_count / pcm_s.pdma_chunkcount--;
	count = fifo_send(pcm_s.pdma_buf, chunksize);
    } else {
	/* ??? something wrong... */
	printk("pcm0: chunkcount overrun\n");
	chunksize = count = 0;
    }

    if (((audio_devs[my_dev]->dmap->qlen < 2) && (pcm_s.pdma_chunkcount == 0))
	|| (count < pcm_s.intr_size)) {
	/* The sent chunk seems to be the last one. */
	fifo_sendtrailer(pcm_s.intr_size);
	pcm_s.intr_last = YES;
    }

    pcm_s.pdma_buf += chunksize;
    pcm_s.pdma_count -= chunksize;
}


static int
fifo_send(pcm_data *buf, int count)
{
    int i, length, r, cnt, rslt;
    pcm_data *p;

    /* Calculate the length of PCM frames. */
    cnt = count + tmpbuf.size;
    length = pcm_s.bytes << pcm_s.stereo;
    r = cnt % length;
    cnt -= r;

    if (cnt > 0) {
	if (pcm_s.stereo)
	    fifo_send_stereo(buf, cnt);
	else
	    fifo_send_monoral(buf, cnt);
	/* Carry over extra data which doesn't seem to be a full PCM frame. */
	p = (pcm_data *)buf + count - r;
	for (i = 0; i < r; i++)
	    tmpbuf.buff[i] = *p++;
    } else {
	/* Carry over extra data which doesn't seem to be a full PCM frame. */
	p = (pcm_data *)buf;
	for (i = tmpbuf.size; i < r; i++)
	    tmpbuf.buff[i] = *p++;
    }
    tmpbuf.size = r;

    rslt = ((cnt / length) * pcm_s.chipspeed / pcm_s.speed) * pcm_s.bytes * 2;
#ifdef PCM86_DEBUG
    printk("fifo_send(): %d bytes sent\n", rslt);
#endif
    return rslt;
}


static void
fifo_sendtrailer(int count)
{
    /* Send trailing zeros to the FIFO buffer. */
    int i;

    for (i = 0; i < count; i++)
	outb(pcm_s.iobase + 12, 0);
    pcm_s.intr_trailer = YES;

#ifdef PCM86_DEBUG
    printk("fifo_sendtrailer(): %d bytes sent\n", count);
#endif
}


static void
fifo_send_stereo(pcm_data *buf, int count)
{
    /* Convert format and sampling speed. */
    switch (pcm_s.format) {
    case AFMT_MU_LAW:
	fifo_send_stereo_ulaw(buf, count);
	break;
    case AFMT_S8:
	fifo_send_stereo_8(buf, count, NO);
	break;
    case AFMT_U8:
	fifo_send_stereo_8(buf, count, YES);
	break;
    case AFMT_S16_LE:
	fifo_send_stereo_16le(buf, count, NO);
	break;
    case AFMT_U16_LE:
	fifo_send_stereo_16le(buf, count, YES);
	break;
    case AFMT_S16_BE:
	fifo_send_stereo_16be(buf, count, NO);
	break;
    case AFMT_U16_BE:
	fifo_send_stereo_16be(buf, count, YES);
	break;
    }
}


static void
fifo_send_monoral(pcm_data *buf, int count)
{
    /* Convert format and sampling speed. */
    switch (pcm_s.format) {
    case AFMT_MU_LAW:
	fifo_send_mono_ulaw(buf, count);
	break;
    case AFMT_S8:
	fifo_send_mono_8(buf, count, NO);
	break;
    case AFMT_U8:
	fifo_send_mono_8(buf, count, YES);
	break;
    case AFMT_S16_LE:
	fifo_send_mono_16le(buf, count, NO);
	break;
    case AFMT_U16_LE:
	fifo_send_mono_16le(buf, count, YES);
	break;
    case AFMT_S16_BE:
	fifo_send_mono_16be(buf, count, NO);
	break;
    case AFMT_U16_BE:
	fifo_send_mono_16be(buf, count, YES);
	break;
    }
}


static void
fifo_send_stereo_ulaw(pcm_data *buf, int count)
{
    int i;
    signed char dl, dl0, dl1, dr, dr0, dr1;
    pcm_data t[2];

    if (tmpbuf.size > 0)
	t[0] = ulaw2linear[tmpbuf.buff[0]];
    else
	t[0] = ulaw2linear[*buf++];
    t[1] = ulaw2linear[*buf++];

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	outb(pcm_s.iobase + 12, t[0]);
	outb(pcm_s.iobase + 12, t[1]);
	count -= 2;
	for (i = 0; i < count; i++)
	    outb(pcm_s.iobase + 12, ulaw2linear[*buf++]);
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = t[0];
	dr1 = t[1];
	i = 0;
	count /= 2;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		dl0 = dl1;
		dr0 = dr1;
		if (i < count) {
		    dl1 = ulaw2linear[*buf++];
		    dr1 = ulaw2linear[*buf++];
		} else
		    dl1 = dr1 = 0;
	    }
	    dl = ((dl0 * (pcm_s.chipspeed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    dr = ((dr0 * (pcm_s.chipspeed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, dl);
	    outb(pcm_s.iobase + 12, dr);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_send_stereo_8(pcm_data *buf, int count, int uflag)
{
    int i;
    signed char dl, dl0, dl1, dr, dr0, dr1, zlev;
    pcm_data t[2];

    zlev = uflag ? -128 : 0;

    if (tmpbuf.size > 0)
	t[0] = tmpbuf.buff[0] + zlev;
    else
	t[0] = *buf++ + zlev;
    t[1] = *buf++ + zlev;

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	outb(pcm_s.iobase + 12, t[0]);
	outb(pcm_s.iobase + 12, t[1]);
	count -= 2;
	for (i = 0; i < count; i++)
	    outb(pcm_s.iobase + 12, *buf++ + zlev);
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = t[0];
	dr1 = t[1];
	i = 0;
	count /= 2;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		dl0 = dl1;
		dr0 = dr1;
		if (i < count) {
		    dl1 = *buf++ + zlev;
		    dr1 = *buf++ + zlev;
		} else
		    dl1 = dr1 = 0;
	    }
	    dl = ((dl0 * (pcm_s.chipspeed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    dr = ((dr0 * (pcm_s.chipspeed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, dl);
	    outb(pcm_s.iobase + 12, dr);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_send_stereo_16le(pcm_data *buf, int count, int uflag)
{
    int i;
    short dl, dl0, dl1, dr, dr0, dr1, zlev;
    pcm_data t[4];

    zlev = uflag ? -128 : 0;

    for (i = 0; i < 4; i++)
	t[i] = (tmpbuf.size > i) ? tmpbuf.buff[i] : *buf++;

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	outb(pcm_s.iobase + 12, t[1] + zlev);
	outb(pcm_s.iobase + 12, t[0]);
	outb(pcm_s.iobase + 12, t[3] + zlev);
	outb(pcm_s.iobase + 12, t[2]);
	count = count / 2 - 2;
	for (i = 0; i < count; i++) {
	    outb(pcm_s.iobase + 12, *(buf + 1) + zlev);
	    outb(pcm_s.iobase + 12, *buf);
	    buf += 2;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = t[0] + ((t[1] + zlev) << 8);
	dr1 = t[2] + ((t[3] + zlev) << 8);
	i = 0;
	count /= 4;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		dl0 = dl1;
		dr0 = dr1;
		if (i < count) {
		    dl1 = *buf + ((*(buf + 1) + zlev) << 8);
		    buf += 2;
		    dr1 = *buf + ((*(buf + 1) + zlev) << 8);
		    buf += 2;
		} else
		    dl1 = dr1 = 0;
	    }
	    dl = ((dl0 * (pcm_s.chipspeed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    dr = ((dr0 * (pcm_s.chipspeed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, (dl >> 8) & 0xff);
	    outb(pcm_s.iobase + 12, dl & 0xff);
	    outb(pcm_s.iobase + 12, (dr >> 8) & 0xff);
	    outb(pcm_s.iobase + 12, dr & 0xff);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_send_stereo_16be(pcm_data *buf, int count, int uflag)
{
    int i;
    short dl, dl0, dl1, dr, dr0, dr1, zlev;
    pcm_data t[4];

    zlev = uflag ? -128 : 0;

    for (i = 0; i < 4; i++)
	t[i] = (tmpbuf.size > i) ? tmpbuf.buff[i] : *buf++;

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	outb(pcm_s.iobase + 12, t[0] + zlev);
	outb(pcm_s.iobase + 12, t[1]);
	outb(pcm_s.iobase + 12, t[2] + zlev);
	outb(pcm_s.iobase + 12, t[3]);
	count = count / 2 - 2;
	for (i = 0; i < count; i++) {
	    outb(pcm_s.iobase + 12, *buf + zlev);
	    outb(pcm_s.iobase + 12, *(buf + 1));
	    buf += 2;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = ((t[0] + zlev) << 8) + t[1];
	dr1 = ((t[2] + zlev) << 8) + t[3];
	i = 0;
	count /= 4;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		dl0 = dl1;
		dr0 = dr1;
		if (i < count) {
		    dl1 = ((*buf + zlev) << 8) + *(buf + 1);
		    buf += 2;
		    dr1 = ((*buf + zlev) << 8) + *(buf + 1);
		    buf += 2;
		} else
		    dl1 = dr1 = 0;
	    }
	    dl = ((dl0 * (pcm_s.chipspeed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    dr = ((dr0 * (pcm_s.chipspeed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, (dl >> 8) & 0xff);
	    outb(pcm_s.iobase + 12, dl & 0xff);
	    outb(pcm_s.iobase + 12, (dr >> 8) & 0xff);
	    outb(pcm_s.iobase + 12, dr & 0xff);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_send_mono_ulaw(pcm_data *buf, int count)
{
    int i;
    signed char d, d0, d1;

    if (pcm_s.speed == pcm_s.chipspeed)
	/* No reason to convert the pcm speed. */
	for (i = 0; i < count; i++) {
	    d = ulaw2linear[*buf++];
	    outb(pcm_s.iobase + 12, d);
	    outb(pcm_s.iobase + 12, d);
	}
    else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	d1 = ulaw2linear[*buf++];
	i = 0;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		d0 = d1;
		d1 = (i < count) ? ulaw2linear[*buf++] : 0;
	    }
	    d = ((d0 * (pcm_s.chipspeed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, d);
	    outb(pcm_s.iobase + 12, d);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = d0;
    }
}


static void
fifo_send_mono_8(pcm_data *buf, int count, int uflag)
{
    int i;
    signed char d, d0, d1, zlev;

    zlev = uflag ? -128 : 0;

    if (pcm_s.speed == pcm_s.chipspeed)
	/* No reason to convert the pcm speed. */
	for (i = 0; i < count; i++) {
	    d = *buf++ + zlev;
	    outb(pcm_s.iobase + 12, d);
	    outb(pcm_s.iobase + 12, d);
	}
    else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	d1 = *buf++ + zlev;
	i = 0;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		d0 = d1;
		d1 = (i < count) ? *buf++ + zlev : 0;
	    }
	    d = ((d0 * (pcm_s.chipspeed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, d);
	    outb(pcm_s.iobase + 12, d);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = d0;
    }
}


static void
fifo_send_mono_16le(pcm_data *buf, int count, int uflag)
{
    int i;
    short d, d0, d1, zlev;
    pcm_data t[2];

    zlev = uflag ? -128 : 0;

    for (i = 0; i < 2; i++)
	t[i] = (tmpbuf.size > i) ? tmpbuf.buff[i] : *buf++;

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	outb(pcm_s.iobase + 12, t[1] + zlev);
	outb(pcm_s.iobase + 12, t[0]);
	outb(pcm_s.iobase + 12, t[1] + zlev);
	outb(pcm_s.iobase + 12, t[0]);
	count = count / 2 - 1;
	for (i = 0; i < count; i++) {
	    outb(pcm_s.iobase + 12, *(buf + 1) + zlev);
	    outb(pcm_s.iobase + 12, *buf);
	    outb(pcm_s.iobase + 12, *(buf + 1) + zlev);
	    outb(pcm_s.iobase + 12, *buf);
	    buf += 2;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	d1 = t[0] + ((t[1] + zlev) << 8);
	i = 0;
	count /= 2;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		d0 = d1;
		if (i < count) {
		    d1 = *buf + ((*(buf + 1) + zlev) << 8);
		    buf += 2;
		} else
		    d1 = 0;
	    }
	    d = ((d0 * (pcm_s.chipspeed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, (d >> 8) & 0xff);
	    outb(pcm_s.iobase + 12, d & 0xff);
	    outb(pcm_s.iobase + 12, (d >> 8) & 0xff);
	    outb(pcm_s.iobase + 12, d & 0xff);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = d0;
    }
}


static void
fifo_send_mono_16be(pcm_data *buf, int count, int uflag)
{
    int i;
    short d, d0, d1, zlev;
    pcm_data t[2];

    zlev = uflag ? -128 : 0;

    for (i = 0; i < 2; i++)
	t[i] = (tmpbuf.size > i) ? tmpbuf.buff[i] : *buf++;

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	outb(pcm_s.iobase + 12, t[0] + zlev);
	outb(pcm_s.iobase + 12, t[1]);
	outb(pcm_s.iobase + 12, t[0] + zlev);
	outb(pcm_s.iobase + 12, t[1]);
	count = count / 2 - 1;
	for (i = 0; i < count; i++) {
	    outb(pcm_s.iobase + 12, *buf + zlev);
	    outb(pcm_s.iobase + 12, *(buf + 1));
	    outb(pcm_s.iobase + 12, *buf + zlev);
	    outb(pcm_s.iobase + 12, *(buf + 1));
	    buf += 2;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	d1 = ((t[0] + zlev) << 8) + t[1];
	i = 0;
	count /= 2;
	while (i < count) {
	    while (pcm_s.acc >= pcm_s.chipspeed) {
		pcm_s.acc -= pcm_s.chipspeed;
		i++;
		d0 = d1;
		if (i < count) {
		    d1 = ((*buf + zlev) << 8) + *(buf + 1);
		    buf += 2;
		} else
		    d1 = 0;
	    }
	    d = ((d0 * (pcm_s.chipspeed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.chipspeed;
	    outb(pcm_s.iobase + 12, d & 0xff);
	    outb(pcm_s.iobase + 12, (d >> 8) & 0xff);
	    outb(pcm_s.iobase + 12, d & 0xff);
	    outb(pcm_s.iobase + 12, (d >> 8) & 0xff);
	    pcm_s.acc += pcm_s.speed;
	}

	pcm_s.last_l = d0;
    }
}


static void
fifo_input_block(void)
{
    int chunksize;

    if (pcm_s.pdma_chunkcount) {
	/* Update chunksize and then receive the next chunk from FIFO. */
	chunksize = pcm_s.pdma_count / pcm_s.pdma_chunkcount--;
	fifo_recv(pcm_s.pdma_buf, chunksize);
	pcm_s.pdma_buf += chunksize;
	pcm_s.pdma_count -= chunksize;
    } else
	/* ??? something wrong... */
	printk("pcm0: chunkcount overrun\n");
}


static void
fifo_recv(pcm_data *buf, int count)
{
    int i;

    if (count > tmpbuf.size) {
	for (i = 0; i < tmpbuf.size; i++)
	    *buf++ = tmpbuf.buff[i];
	count -= tmpbuf.size;
	tmpbuf.size = 0;
	if (pcm_s.stereo)
	    fifo_recv_stereo(buf, count);
	else
	    fifo_recv_monoral(buf, count);
    } else {
	for (i = 0; i < count; i++)
	    *buf++ = tmpbuf.buff[i];
	for (i = 0; i < tmpbuf.size - count; i++)
	    tmpbuf.buff[i] = tmpbuf.buff[i + count];
	tmpbuf.size -= count;
    }

#ifdef PCM86_DEBUG
    printk("fifo_recv(): %d bytes received\n",
	   ((count / (pcm_s.bytes << pcm_s.stereo)) * pcm_s.chipspeed
	    / pcm_s.speed) * pcm_s.bytes * 2);
#endif
}


static void
fifo_recv_stereo(pcm_data *buf, int count)
{
    /* Convert format and sampling speed. */
    switch (pcm_s.format) {
    case AFMT_MU_LAW:
	fifo_recv_stereo_ulaw(buf, count);
	break;
    case AFMT_S8:
	fifo_recv_stereo_8(buf, count, NO);
	break;
    case AFMT_U8:
	fifo_recv_stereo_8(buf, count, YES);
	break;
    case AFMT_S16_LE:
	fifo_recv_stereo_16le(buf, count, NO);
	break;
    case AFMT_U16_LE:
	fifo_recv_stereo_16le(buf, count, YES);
	break;
    case AFMT_S16_BE:
	fifo_recv_stereo_16be(buf, count, NO);
	break;
    case AFMT_U16_BE:
	fifo_recv_stereo_16be(buf, count, YES);
	break;
    }
}


static void
fifo_recv_monoral(pcm_data *buf, int count)
{
    /* Convert format and sampling speed. */
    switch (pcm_s.format) {
    case AFMT_MU_LAW:
	fifo_recv_mono_ulaw(buf, count);
	break;
    case AFMT_S8:
	fifo_recv_mono_8(buf, count, NO);
	break;
    case AFMT_U8:
	fifo_recv_mono_8(buf, count, YES);
	break;
    case AFMT_S16_LE:
	fifo_recv_mono_16le(buf, count, NO);
	break;
    case AFMT_U16_LE:
	fifo_recv_mono_16le(buf, count, YES);
	break;
    case AFMT_S16_BE:
	fifo_recv_mono_16be(buf, count, NO);
	break;
    case AFMT_U16_BE:
	fifo_recv_mono_16be(buf, count, YES);
	break;
    }
}


static void
fifo_recv_stereo_ulaw(pcm_data *buf, int count)
{
    int i, cnt;
    signed char dl, dl0, dl1, dr, dr0, dr1;

    cnt = count / 2;
    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < cnt; i++) {
	    *buf++ = linear2ulaw[inb(pcm_s.iobase + 12)];
	    *buf++ = linear2ulaw[inb(pcm_s.iobase + 12)];
	}
	if (count % 2) {
	    *buf++ = linear2ulaw[inb(pcm_s.iobase + 12)];
	    tmpbuf.buff[0] = linear2ulaw[inb(pcm_s.iobase + 12)];
	    tmpbuf.size = 1;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = inb(pcm_s.iobase + 12);
	dr1 = inb(pcm_s.iobase + 12);
	for (i = 0; i < cnt; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = linear2ulaw[dl & 0xff];
	    *buf++ = linear2ulaw[dr & 0xff];
	    pcm_s.acc += pcm_s.chipspeed;
	}
	if (count % 2) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = linear2ulaw[dl & 0xff];
	    tmpbuf.buff[0] = linear2ulaw[dr & 0xff];
	    tmpbuf.size = 1;
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_recv_stereo_8(pcm_data *buf, int count, int uflag)
{
    int i, cnt;
    signed char dl, dl0, dl1, dr, dr0, dr1, zlev;

    zlev = uflag ? -128 : 0;

    cnt = count / 2;
    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < cnt; i++) {
	    *buf++ = inb(pcm_s.iobase + 12) + zlev;
	    *buf++ = inb(pcm_s.iobase + 12) + zlev;
	}
	if (count % 2) {
	    *buf++ = inb(pcm_s.iobase + 12) + zlev;
	    tmpbuf.buff[0] = inb(pcm_s.iobase + 12) + zlev;
	    tmpbuf.size = 1;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = inb(pcm_s.iobase + 12);
	dr1 = inb(pcm_s.iobase + 12);
	for (i = 0; i < cnt; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = dl + zlev;
	    *buf++ = dr + zlev;
	    pcm_s.acc += pcm_s.chipspeed;
	}
	if (count % 2) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = dl + zlev;
	    tmpbuf.buff[0] = dr + zlev;
	    tmpbuf.size = 1;
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_recv_stereo_16le(pcm_data *buf, int count, int uflag)
{
    int i, cnt;
    short dl, dl0, dl1, dr, dr0, dr1, zlev;
    pcm_data t[4];

    zlev = uflag ? -128 : 0;

    cnt = count / 4;
    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < cnt; i++) {
	    *(buf + 1) = inb(pcm_s.iobase + 12) + zlev;
	    *buf = inb(pcm_s.iobase + 12);
	    *(buf + 3) = inb(pcm_s.iobase + 12) + zlev;
	    *(buf + 2) = inb(pcm_s.iobase + 12);
	    buf += 4;
	}
	if (count % 4) {
	    t[1] = inb(pcm_s.iobase + 12) + zlev;
	    t[0] = inb(pcm_s.iobase + 12);
	    t[3] = inb(pcm_s.iobase + 12) + zlev;
	    t[2] = inb(pcm_s.iobase + 12);
	    tmpbuf.size = 0;
	    for (i = 0; i < count % 4; i++)
		*buf++ = t[i];
	    for (i = count % 4; i < 4; i++)
		tmpbuf.buff[tmpbuf.size++] = t[i];
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = inb(pcm_s.iobase + 12) << 8;
	dl1 |= inb(pcm_s.iobase + 12);
	dr1 = inb(pcm_s.iobase + 12) << 8;
	dr1 |= inb(pcm_s.iobase + 12);
	for (i = 0; i < cnt; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12) << 8;
		dl1 |= inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12) << 8;
		dr1 |= inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = dl & 0xff;
	    *buf++ = ((dl >> 8) & 0xff) + zlev;
	    *buf++ = dr & 0xff;
	    *buf++ = ((dr >> 8) & 0xff) + zlev;
	    pcm_s.acc += pcm_s.chipspeed;
	}
	if (count % 4) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12) << 8;
		dl1 |= inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12) << 8;
		dr1 |= inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    t[0] = dl & 0xff;
	    t[1] = ((dl >> 8) & 0xff) + zlev;
	    t[2] = dr & 0xff;
	    t[3] = ((dr >> 8) & 0xff) + zlev;
	    tmpbuf.size = 0;
	    for (i = 0; i < count % 4; i++)
		*buf++ = t[i];
	    for (i = count % 4; i < 4; i++)
		tmpbuf.buff[tmpbuf.size++] = t[i];
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_recv_stereo_16be(pcm_data *buf, int count, int uflag)
{
    int i, cnt;
    short dl, dl0, dl1, dr, dr0, dr1, zlev;
    pcm_data t[4];

    zlev = uflag ? -128 : 0;

    cnt = count / 4;
    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < cnt; i++) {
	    *buf++ = inb(pcm_s.iobase + 12) + zlev;
	    *buf++ = inb(pcm_s.iobase + 12);
	    *buf++ = inb(pcm_s.iobase + 12) + zlev;
	    *buf++ = inb(pcm_s.iobase + 12);
	}
	if (count % 4) {
	    t[0] = inb(pcm_s.iobase + 12) + zlev;
	    t[1] = inb(pcm_s.iobase + 12);
	    t[2] = inb(pcm_s.iobase + 12) + zlev;
	    t[3] = inb(pcm_s.iobase + 12);
	    tmpbuf.size = 0;
	    for (i = 0; i < count % 4; i++)
		*buf++ = t[i];
	    for (i = count % 4; i < 4; i++)
		tmpbuf.buff[tmpbuf.size++] = t[i];
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	dl0 = pcm_s.last_l;
	dr0 = pcm_s.last_r;
	dl1 = inb(pcm_s.iobase + 12) << 8;
	dl1 |= inb(pcm_s.iobase + 12);
	dr1 = inb(pcm_s.iobase + 12) << 8;
	dr1 |= inb(pcm_s.iobase + 12);
	for (i = 0; i < cnt; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12) << 8;
		dl1 |= inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12) << 8;
		dr1 |= inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = ((dl >> 8) & 0xff) + zlev;
	    *buf++ = dl & 0xff;
	    *buf++ = ((dr >> 8) & 0xff) + zlev;
	    *buf++ = dr & 0xff;
	    pcm_s.acc += pcm_s.chipspeed;
	}
	if (count % 4) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		dl0 = dl1;
		dr0 = dr1;
		dl1 = inb(pcm_s.iobase + 12) << 8;
		dl1 |= inb(pcm_s.iobase + 12);
		dr1 = inb(pcm_s.iobase + 12) << 8;
		dr1 |= inb(pcm_s.iobase + 12);
	    }
	    dl = ((dl0 * (pcm_s.speed - pcm_s.acc)) + (dl1 * pcm_s.acc))
		/ pcm_s.speed;
	    dr = ((dr0 * (pcm_s.speed - pcm_s.acc)) + (dr1 * pcm_s.acc))
		/ pcm_s.speed;
	    t[0] = ((dl >> 8) & 0xff) + zlev;
	    t[1] = dl & 0xff;
	    t[2] = ((dr >> 8) & 0xff) + zlev;
	    t[3] = dr & 0xff;
	    tmpbuf.size = 0;
	    for (i = 0; i < count % 4; i++)
		*buf++ = t[i];
	    for (i = count % 4; i < 4; i++)
		tmpbuf.buff[tmpbuf.size++] = t[i];
	}

	pcm_s.last_l = dl0;
	pcm_s.last_r = dr0;
    }
}


static void
fifo_recv_mono_ulaw(pcm_data *buf, int count)
{
    int i;
    signed char d, d0, d1;

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < count; i++) {
	    d = ((signed char)inb(pcm_s.iobase + 12)
		 + (signed char)inb(pcm_s.iobase + 12)) >> 1;
	    *buf++ = linear2ulaw[d & 0xff];
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	d1 = ((signed char)inb(pcm_s.iobase + 12)
	      + (signed char)inb(pcm_s.iobase + 12)) >> 1;
	for (i = 0; i < count; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		d0 = d1;
		d1 = ((signed char)inb(pcm_s.iobase + 12)
		      + (signed char)inb(pcm_s.iobase + 12)) >> 1;
	    }
	    d = ((d0 * (pcm_s.speed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = linear2ulaw[d & 0xff];
	    pcm_s.acc += pcm_s.chipspeed;
	}

	pcm_s.last_l = d0;
    }
}


static void
fifo_recv_mono_8(pcm_data *buf, int count, int uflag)
{
    int i;
    signed char d, d0, d1, zlev;

    zlev = uflag ? -128 : 0;

    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < count; i++) {
	    d = ((signed char)inb(pcm_s.iobase + 12)
		 + (signed char)inb(pcm_s.iobase + 12)) >> 1;
	    *buf++ = d + zlev;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	d1 = ((signed char)inb(pcm_s.iobase + 12)
	      + (signed char)inb(pcm_s.iobase + 12)) >> 1;
	for (i = 0; i < count; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		d0 = d1;
		d1 = ((signed char)inb(pcm_s.iobase + 12)
		      + (signed char)inb(pcm_s.iobase + 12)) >> 1;
	    }
	    d = ((d0 * (pcm_s.speed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = d + zlev;
	    pcm_s.acc += pcm_s.chipspeed;
	}

	pcm_s.last_l = d0;
    }
}


static void
fifo_recv_mono_16le(pcm_data *buf, int count, int uflag)
{
    int i, cnt;
    short d, d0, d1, el, er, zlev;

    zlev = uflag ? -128 : 0;

    cnt = count / 2;
    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < cnt; i++) {
	    el = inb(pcm_s.iobase + 12) << 8;
	    el |= inb(pcm_s.iobase + 12);
	    er = inb(pcm_s.iobase + 12) << 8;
	    er |= inb(pcm_s.iobase + 12);
	    d = (el + er) >> 1;
	    *buf++ = d & 0xff;
	    *buf++ = ((d >> 8) & 0xff) + zlev;
	}
	if (count % 2) {
	    el = inb(pcm_s.iobase + 12) << 8;
	    el |= inb(pcm_s.iobase + 12);
	    er = inb(pcm_s.iobase + 12) << 8;
	    er |= inb(pcm_s.iobase + 12);
	    d = (el + er) >> 1;
	    *buf++ = d & 0xff;
	    tmpbuf.buff[0] = ((d >> 8) & 0xff) + zlev;
	    tmpbuf.size = 1;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	el = inb(pcm_s.iobase + 12) << 8;
	el |= inb(pcm_s.iobase + 12);
	er = inb(pcm_s.iobase + 12) << 8;
	er |= inb(pcm_s.iobase + 12);
	d1 = (el + er) >> 1;
	for (i = 0; i < cnt; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		d0 = d1;
		el = inb(pcm_s.iobase + 12) << 8;
		el |= inb(pcm_s.iobase + 12);
		er = inb(pcm_s.iobase + 12) << 8;
		er |= inb(pcm_s.iobase + 12);
		d1 = (el + er) >> 1;
	    }
	    d = ((d0 * (pcm_s.speed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = d & 0xff;
	    *buf++ = ((d >> 8) & 0xff) + zlev;
	    pcm_s.acc += pcm_s.chipspeed;
	}
	if (count % 2) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		d0 = d1;
		el = inb(pcm_s.iobase + 12) << 8;
		el |= inb(pcm_s.iobase + 12);
		er = inb(pcm_s.iobase + 12) << 8;
		er |= inb(pcm_s.iobase + 12);
		d1 = (el + er) >> 1;
	    }
	    d = ((d0 * (pcm_s.speed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = d & 0xff;
	    tmpbuf.buff[0] = ((d >> 8) & 0xff) + zlev;
	    tmpbuf.size = 1;
	}

	pcm_s.last_l = d0;
    }
}


static void
fifo_recv_mono_16be(pcm_data *buf, int count, int uflag)
{
    int i, cnt;
    short d, d0, d1, el, er, zlev;

    zlev = uflag ? -128 : 0;

    cnt = count / 2;
    if (pcm_s.speed == pcm_s.chipspeed) {
	/* No reason to convert the pcm speed. */
	for (i = 0; i < cnt; i++) {
	    el = inb(pcm_s.iobase + 12) << 8;
	    el |= inb(pcm_s.iobase + 12);
	    er = inb(pcm_s.iobase + 12) << 8;
	    er |= inb(pcm_s.iobase + 12);
	    d = (el + er) >> 1;
	    *buf++ = ((d >> 8) & 0xff) + zlev;
	    *buf++ = d & 0xff;
	}
	if (count % 2) {
	    el = inb(pcm_s.iobase + 12) << 8;
	    el |= inb(pcm_s.iobase + 12);
	    er = inb(pcm_s.iobase + 12) << 8;
	    er |= inb(pcm_s.iobase + 12);
	    d = (el + er) >> 1;
	    *buf++ = ((d >> 8) & 0xff) + zlev;
	    tmpbuf.buff[0] = d & 0xff;
	    tmpbuf.size = 1;
	}
    } else {
	/* Speed conversion with linear interpolation method. */
	d0 = pcm_s.last_l;
	el = inb(pcm_s.iobase + 12) << 8;
	el |= inb(pcm_s.iobase + 12);
	er = inb(pcm_s.iobase + 12) << 8;
	er |= inb(pcm_s.iobase + 12);
	d1 = (el + er) >> 1;
	for (i = 0; i < cnt; i++) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		d0 = d1;
		el = inb(pcm_s.iobase + 12) << 8;
		el |= inb(pcm_s.iobase + 12);
		er = inb(pcm_s.iobase + 12) << 8;
		er |= inb(pcm_s.iobase + 12);
		d1 = (el + er) >> 1;
	    }
	    d = ((d0 * (pcm_s.speed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = ((d >> 8) & 0xff) + zlev;
	    *buf++ = d & 0xff;
	    pcm_s.acc += pcm_s.chipspeed;
	}
	if (count % 2) {
	    while (pcm_s.acc >= pcm_s.speed) {
		pcm_s.acc -= pcm_s.speed;
		d0 = d1;
		el = inb(pcm_s.iobase + 12) << 8;
		el |= inb(pcm_s.iobase + 12);
		er = inb(pcm_s.iobase + 12) << 8;
		er |= inb(pcm_s.iobase + 12);
		d1 = (el + er) >> 1;
	    }
	    d = ((d0 * (pcm_s.speed - pcm_s.acc)) + (d1 * pcm_s.acc))
		/ pcm_s.speed;
	    *buf++ = ((d >> 8) & 0xff) + zlev;
	    tmpbuf.buff[0] = d & 0xff;
	    tmpbuf.size = 1;
	}

	pcm_s.last_l = d0;
    }
}


static void
pcm_stop(void)
{
    fifo_stop();		/* stop FIFO */
    fifo_reset();		/* reset FIFO buffer */

    /* Reset driver's status. */
    pcm_s.intr_busy = NO;
    pcm_s.intr_last = NO;
    pcm_s.intr_trailer = NO;
    pcm_s.acc = 0;
    pcm_s.last_l = 0;
    pcm_s.last_r = 0;

    DEB(printk("pcm_stop\n"));
}


static void
pcm_init(void)
{
    /* Initialize registers on the board. */
    pcm_stop();
    if (pcm_s.board_type == PC980173_FAMILY)
	dsp73_init();

    /* Set default volume. */
    set_volume(DEFAULT_VOLUME);

    /* Initialize driver's status. */
    pcm_s.opened = NO;
    pcm_initialized = YES;
}


/*
 * Codes for global use
 */

int
probe_pcm86(struct address_info *hw_config)
{
    return pcm86_detect(hw_config);
}


long
attach_pcm86(long mem_start, struct address_info *hw_config)
{
    if (pcm_s.board_type == NO_SUPPORTED_BOARD)
	return mem_start;

    /* Initialize the board. */
    pcm_init();

    printk("pcm0: <%s>", pcm86_operations.name);

    if (num_audiodevs < MAX_AUDIO_DEV) {
	my_dev = num_audiodevs++;
	audio_devs[my_dev] = &pcm86_operations;
	audio_devs[my_dev]->buffcount = DSP_BUFFCOUNT;
	audio_devs[my_dev]->buffsize = DSP_BUFFSIZE;
#ifdef PCM86_DEBUG
	printk("\nbuffsize = %d", DSP_BUFFSIZE);
#endif
    } else
	printk("pcm0: Too many PCM devices available");

    return mem_start;
}


static int
pcm86_detect(struct address_info *hw_config)
{
    int opna_iobase = 0x188, irq = 12, i;
    unsigned char tmp;

    if (hw_config->io_base == -1) {
	printf("pcm0: iobase not specified. Assume default port(0x%x)\n",
	       PCM86_IOBASE);
	hw_config->io_base = PCM86_IOBASE;
    }
    pcm_s.iobase = hw_config->io_base;

    /* auto configuration */
    tmp = inb(pcm_s.iobase) & 0xfc;
    switch ((tmp & 0xf0) >> 4) {
    case 2:
	opna_iobase = 0x188;
	pcm_s.board_type = PC980173_FAMILY;
	break;
    case 3:
	opna_iobase = 0x288;
	pcm_s.board_type = PC980173_FAMILY;
	break;
    case 4:
	opna_iobase = 0x188;
	pcm_s.board_type = PC980186_FAMILY;
	break;
    case 5:
	opna_iobase = 0x288;
	pcm_s.board_type = PC980186_FAMILY;
	break;
    default:
	pcm_s.board_type = NO_SUPPORTED_BOARD;
	return NO;
    }

    /* Enable OPNA(YM2608) facilities. */
    outb(pcm_s.iobase, tmp | 0x01);

    /* Wait for OPNA to be ready. */
    i = 100000;		/* Some large value */
    while((inb(opna_iobase) & 0x80) && (i-- > 0));

    /* Make IOA/IOB port ready (IOA:input, IOB:output) */
    outb(opna_iobase, 0x07);
    outb(0x5f, 0);	/* Because OPNA ports are comparatively slow(?), */
    outb(0x5f, 0);	/* we'd better wait a moment. */
    outb(0x5f, 0);
    outb(0x5f, 0);
    tmp = inb(opna_iobase + 2) & 0x3f;
    outb(opna_iobase + 2, tmp | 0x80);

    /* Wait for OPNA to be ready. */
    i = 100000;		/* Some large value */
    while((inb(opna_iobase) & 0x80) && (i-- > 0));

    /* Get irq number from IOA port. */
    outb(opna_iobase, 0x0e);
    outb(0x5f, 0);
    outb(0x5f, 0);
    outb(0x5f, 0);
    outb(0x5f, 0);
    tmp = inb(opna_iobase + 2) & 0xc0;
    switch (tmp >> 6) {
    case 0:	/* INT0 (IRQ3)*/
	irq = 3;
	break;
    case 1:	/* INT6 (IRQ13)*/
	irq = 13;
	break;
    case 2:	/* INT4 (IRQ10)*/
	irq = 10;
	break;
    case 3:	/* INT5 (IRQ12)*/
	irq = 12;
	break;
    default:	/* error */
	return NO;
    }

    /* Wait for OPNA to be ready. */
    i = 100000;		/* Some large value */
    while((inb(opna_iobase) & 0x80) && (i-- > 0));

    /* Reset OPNA timer register. */
    outb(opna_iobase, 0x27);
    outb(0x5f, 0);
    outb(0x5f, 0);
    outb(0x5f, 0);
    outb(0x5f, 0);
    outb(opna_iobase + 2, 0x30);

    /* Ok.  Detection finished. */
    sprintf(pcm86_operations.name, board_name[pcm_s.board_type]);
    pcm_initialized = NO;
    pcm_s.irq = irq;

    if ((hw_config->irq > 0) && (hw_config->irq != irq))
	printf("pcm0: change irq %d -> %d\n", hw_config->irq, irq);
    hw_config->irq = irq;

    return YES;
}


static int
pcm86_open(int dev, int mode)
{
    int err;

    if (!pcm_initialized)
	return RET_ERROR(ENXIO);

    if (pcm_s.intr_busy || pcm_s.opened)
	return RET_ERROR(EBUSY);

    if ((err = snd_set_irq_handler(pcm_s.irq, pcmintr, "PC-9801-73/86")) < 0)
	return err;

    pcm_stop();

    tmpbuf.size = 0;
    pcm_s.intr_mode = IMODE_NONE;
    pcm_s.opened = YES;

    return 0;
}


static void
pcm86_close(int dev)
{
    snd_release_irq(pcm_s.irq);

    pcm_s.opened = NO;
}


static void
pcm86_output_block(int dev, unsigned long buf, int count, int intrflag,
		   int dma_restart)
{
    unsigned long flags, cnt;
    int maxchunksize;

#ifdef PCM86_DEBUG
    printk("pcm86_output_block():");
    if (audio_devs[dev]->dmap->flags & DMA_BUSY)
	printk(" DMA_BUSY");
    if (audio_devs[dev]->dmap->flags & DMA_RESTART)
	printk(" DMA_RESTART");
    if (audio_devs[dev]->dmap->flags & DMA_ACTIVE)
	printk(" DMA_ACTIVE");
    if (audio_devs[dev]->dmap->flags & DMA_STARTED)
	printk(" DMA_STARTED");
    if (audio_devs[dev]->dmap->flags & DMA_ALLOC_DONE)
	printk(" DMA_ALLOC_DONE");
    printk("\n");
#endif

#if 0
    DISABLE_INTR(flags);
#endif

#ifdef PCM86_DEBUG
    printk("pcm86_output_block(): count = %d, intrsize= %d\n",
	   count, pcm_s.intr_size);
#endif

    pcm_s.pdma_buf = (pcm_data *)buf;
    pcm_s.pdma_count = count;
    pcm_s.pdma_chunkcount = 1;
    maxchunksize = (((PCM86_FIFOSIZE - pcm_s.intr_size * 2)
		     / (pcm_s.bytes * 2)) * pcm_s.speed
		    / pcm_s.chipspeed) * (pcm_s.bytes << pcm_s.stereo);
    if (count > maxchunksize)
	pcm_s.pdma_chunkcount = 2 * count / maxchunksize;
    /*
     * Let chunksize = (float)count / (float)pcm_s.pdma_chunkcount.
     * Data of size chunksize is sent to the FIFO buffer on the 86-board
     * on every occuring of interrupt.
     * By assuming that pcm_s.intr_size < PCM86_FIFOSIZE / 2, we can conclude
     * that the FIFO buffer never overflows from the following lemma.
     *
     * Lemma:
     *	     maxchunksize / 2 <= chunksize <= maxchunksize.
     *   (Though pcm_s.pdma_chunkcount is obtained through the flooring
     *    function, this inequality holds.)
     * Proof) Omitted.
     */

    fifo_output_block();

    pcm_s.intr_last = NO;
    pcm_s.intr_mode = IMODE_OUTPUT;
    if (!pcm_s.intr_busy)
	fifo_start(IMODE_OUTPUT);
    pcm_s.intr_busy = YES;

#if 0
    RESTORE_INTR(flags);
#endif
}


static void
pcm86_start_input(int dev, unsigned long buf, int count, int intrflag,
		  int dma_restart)
{
    unsigned long flags, cnt;
    int maxchunksize;

#ifdef PCM86_DEBUG
    printk("pcm86_start_input():");
    if (audio_devs[dev]->dmap->flags & DMA_BUSY)
	printk(" DMA_BUSY");
    if (audio_devs[dev]->dmap->flags & DMA_RESTART)
	printk(" DMA_RESTART");
    if (audio_devs[dev]->dmap->flags & DMA_ACTIVE)
	printk(" DMA_ACTIVE");
    if (audio_devs[dev]->dmap->flags & DMA_STARTED)
	printk(" DMA_STARTED");
    if (audio_devs[dev]->dmap->flags & DMA_ALLOC_DONE)
	printk(" DMA_ALLOC_DONE");
    printk("\n");
#endif

#if 0
    DISABLE_INTR(flags);
#endif

    pcm_s.intr_size = PCM86_INTRSIZE_IN;

#ifdef PCM86_DEBUG
    printk("pcm86_start_input(): count = %d, intrsize= %d\n",
	   count, pcm_s.intr_size);
#endif

    pcm_s.pdma_buf = (pcm_data *)buf;
    pcm_s.pdma_count = count;
    pcm_s.pdma_chunkcount = 1;
    maxchunksize = ((pcm_s.intr_size / (pcm_s.bytes * 2)) * pcm_s.speed
		    / pcm_s.chipspeed) * (pcm_s.bytes << pcm_s.stereo);
    if (count > maxchunksize)
	pcm_s.pdma_chunkcount = 2 * count / maxchunksize;

    pcm_s.intr_mode = IMODE_INPUT;
    if (!pcm_s.intr_busy)
	fifo_start(IMODE_INPUT);
    pcm_s.intr_busy = YES;

#if 0
    RESTORE_INTR(flags);
#endif
}


static int
pcm86_ioctl(int dev, unsigned int cmd, unsigned int arg, int local)
{
    switch (cmd) {
    case SOUND_PCM_WRITE_RATE:
	if (local)
	    return set_speed(arg);
	return IOCTL_OUT(arg, set_speed(IOCTL_IN(arg)));

    case SOUND_PCM_READ_RATE:
	if (local)
	    return pcm_s.speed;
	return IOCTL_OUT(arg, pcm_s.speed);

    case SNDCTL_DSP_STEREO:
	if (local)
	    return set_stereo(arg);
	return IOCTL_OUT(arg, set_stereo(IOCTL_IN(arg)));

    case SOUND_PCM_WRITE_CHANNELS:
	if (local)
	    return set_stereo(arg - 1) + 1;
	return IOCTL_OUT(arg, set_stereo(IOCTL_IN(arg) - 1) + 1);

    case SOUND_PCM_READ_CHANNELS:
	if (local)
	    return pcm_s.stereo + 1;
	return IOCTL_OUT(arg, pcm_s.stereo + 1);

    case SNDCTL_DSP_SETFMT:
	if (local)
	    return set_format(arg);
	return IOCTL_OUT(arg, set_format(IOCTL_IN(arg)));

    case SOUND_PCM_READ_BITS:
	if (local)
	    return pcm_s.bytes * 8;
	return IOCTL_OUT(arg, pcm_s.bytes * 8);
    }

    /* Invalid ioctl request */
    return RET_ERROR(EINVAL);
}


static int
pcm86_prepare_for_input(int dev, int bufsize, int nbufs)
{
    pcm_s.intr_size = PCM86_INTRSIZE_IN;
    pcm_s.intr_mode = IMODE_NONE;
    pcm_s.acc = 0;
    pcm_s.last_l = 0;
    pcm_s.last_r = 0;

    DEB(printk("pcm86_prepare_for_input\n"));

    return 0;
}


static int
pcm86_prepare_for_output(int dev, int bufsize, int nbufs)
{
    pcm_s.intr_size = PCM86_INTRSIZE_OUT;
    pcm_s.intr_mode = IMODE_NONE;
    pcm_s.acc = 0;
    pcm_s.last_l = 0;
    pcm_s.last_r = 0;

    DEB(printk("pcm86_prepare_for_output\n"));

    return 0;
}


static void
pcm86_reset(int dev)
{
    pcm_stop();
}


static void
pcm86_halt_xfer(int dev)
{
    pcm_stop();

    DEB(printk("pcm86_halt_xfer\n"));
}


void
pcmintr(int unit)
{
    unsigned char tmp;

    if ((inb(pcm_s.iobase + 8) & 0x10) == 0)
	return;		/* not FIFO intr. */

    switch(pcm_s.intr_mode) {
    case IMODE_OUTPUT:
	if (pcm_s.intr_trailer) {
	    DEB(printk("pcmintr(): fifo_reset\n"));
	    fifo_reset();
	    pcm_s.intr_trailer = NO;
	    pcm_s.intr_busy = NO;
	}
	if (pcm_s.pdma_count > 0)
	    fifo_output_block();
	else
	    DMAbuf_outputintr(my_dev, 1);
	/* Reset intr. flag. */
	tmp = inb(pcm_s.iobase + 8);
	outb(pcm_s.iobase + 8, tmp & ~0x10);
	outb(pcm_s.iobase + 8, tmp | 0x10);
	break;

    case IMODE_INPUT:
	fifo_input_block();
	if (pcm_s.pdma_count == 0)
	    DMAbuf_inputintr(my_dev);
	/* Reset intr. flag. */
	tmp = inb(pcm_s.iobase + 8);
	outb(pcm_s.iobase + 8, tmp & ~0x10);
	outb(pcm_s.iobase + 8, tmp | 0x10);
	break;

    default:
	pcm_stop();
	printk("pcm0: unexpected interrupt\n");
    }
}


#endif	/* EXCLUDE_PCM86, EXCLUDE_AUDIO */

#endif	/* CONFIGURE_SOUNDCARD */
