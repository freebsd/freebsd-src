/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include  <dev/sound/isa/sb.h>
#include  <dev/sound/chip.h>

#define ESS_BUFFSIZE (16384)
#define ABS(x) (((x) < 0)? -(x) : (x))

/* if defined, playback always uses the 2nd channel and full duplex works */
#undef ESS18XX_DUPLEX

/* more accurate clocks and split audio1/audio2 rates */
#define ESS18XX_NEWSPEED

/* channel interface for ESS */
static void *esschan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int esschan_setdir(void *data, int dir);
static int esschan_setformat(void *data, u_int32_t format);
static int esschan_setspeed(void *data, u_int32_t speed);
static int esschan_setblocksize(void *data, u_int32_t blocksize);
static int esschan_trigger(void *data, int go);
static int esschan_getptr(void *data);
static pcmchan_caps *esschan_getcaps(void *data);

static u_int32_t ess_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_U16_LE,
	AFMT_STEREO | AFMT_U16_LE,
	0
};
static pcmchan_caps ess_playcaps = {5000, 49000, ess_playfmt, 0};

/*
 * Recording output is byte-swapped
 */
static u_int32_t ess_recfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_BE,
	AFMT_STEREO | AFMT_S16_BE,
	AFMT_U16_BE,
	AFMT_STEREO | AFMT_U16_BE,
	0
};
static pcmchan_caps ess_reccaps = {5000, 49000, ess_recfmt, 0};

static pcm_channel ess_chantemplate = {
	esschan_init,
	esschan_setdir,
	esschan_setformat,
	esschan_setspeed,
	esschan_setblocksize,
	esschan_trigger,
	esschan_getptr,
	esschan_getcaps,
	NULL, 			/* free */
	NULL, 			/* nop1 */
	NULL, 			/* nop2 */
	NULL, 			/* nop3 */
	NULL, 			/* nop4 */
	NULL, 			/* nop5 */
	NULL, 			/* nop6 */
	NULL, 			/* nop7 */
};

struct ess_info;

struct ess_chinfo {
	struct ess_info *parent;
	pcm_channel *channel;
	snd_dbuf *buffer;
	int dir, hwch, stopping;
	u_int32_t fmt, spd;
};

struct ess_info {
    	struct resource *io, *sb, *vc, *mpu, *gp;	/* I/O address for the board */
    	struct resource *irq;
    	bus_dma_tag_t parent_dmat;

    	int simplex_dir, type, duplex:1, newspeed:1, dmasz[2];
    	struct ess_chinfo pch, rch;
};

static int ess_rd(struct ess_info *sc, int reg);
static void ess_wr(struct ess_info *sc, int reg, u_int8_t val);
static int ess_dspready(struct ess_info *sc);
static int ess_cmd(struct ess_info *sc, u_char val);
static int ess_cmd1(struct ess_info *sc, u_char cmd, int val);
static int ess_get_byte(struct ess_info *sc);
static void ess_setmixer(struct ess_info *sc, u_int port, u_int value);
static int ess_getmixer(struct ess_info *sc, u_int port);
static int ess_reset_dsp(struct ess_info *sc);

static int ess_write(struct ess_info *sc, u_char reg, int val);
static int ess_read(struct ess_info *sc, u_char reg);

static void ess_intr(void *arg);
static int ess_setupch(struct ess_info *sc, int ch, int dir, int spd, u_int32_t fmt, int len);
static int ess_start(struct ess_chinfo *ch);
static int ess_stop(struct ess_chinfo *ch);

static int ess_dmasetup(struct ess_info *sc, int ch, u_int32_t base, u_int16_t cnt, int dir);
static int ess_dmapos(struct ess_info *sc, int ch);
static int ess_dmatrigger(struct ess_info *sc, int ch, int go);

static int essmix_init(snd_mixer *m);
static int essmix_set(snd_mixer *m, unsigned dev, unsigned left, unsigned right);
static int essmix_setrecsrc(snd_mixer *m, u_int32_t src);

static snd_mixer ess_mixer = {
    	"ESS mixer",
    	essmix_init,
	NULL,
    	essmix_set,
    	essmix_setrecsrc,
};

static devclass_t pcm_devclass;

/*
 * Common code for the midi and pcm functions
 *
 * ess_cmd write a single byte to the CMD port.
 * ess_cmd1 write a CMD + 1 byte arg
 * ess_cmd2 write a CMD + 2 byte arg
 * ess_get_byte returns a single byte from the DSP data port
 *
 * ess_write is actually ess_cmd1
 * ess_read access ext. regs via ess_cmd(0xc0, reg) followed by ess_get_byte
 */

static int
port_rd(struct resource *port, int regno, int size)
{
	bus_space_tag_t st = rman_get_bustag(port);
	bus_space_handle_t sh = rman_get_bushandle(port);

	switch (size) {
	case 1:
		return bus_space_read_1(st, sh, regno);
	case 2:
		return bus_space_read_2(st, sh, regno);
	case 4:
		return bus_space_read_4(st, sh, regno);
	default:
		return 0xffffffff;
	}
}

static void
port_wr(struct resource *port, int regno, u_int32_t data, int size)
{
	bus_space_tag_t st = rman_get_bustag(port);
	bus_space_handle_t sh = rman_get_bushandle(port);

	switch (size) {
	case 1:
		bus_space_write_1(st, sh, regno, data);
		break;
	case 2:
		bus_space_write_2(st, sh, regno, data);
		break;
	case 4:
		bus_space_write_4(st, sh, regno, data);
		break;
	}
}

static int
ess_rd(struct ess_info *sc, int reg)
{
	return port_rd(sc->sb, reg, 1);
}

static void
ess_wr(struct ess_info *sc, int reg, u_int8_t val)
{
	port_wr(sc->sb, reg, val, 1);
}

static int
ess_dspready(struct ess_info *sc)
{
	return ((ess_rd(sc, SBDSP_STATUS) & 0x80) == 0);
}

static int
ess_dspwr(struct ess_info *sc, u_char val)
{
    	int  i;

    	for (i = 0; i < 1000; i++) {
		if (ess_dspready(sc)) {
	    		ess_wr(sc, SBDSP_CMD, val);
	    		return 1;
		}
		if (i > 10) DELAY((i > 100)? 1000 : 10);
    	}
    	printf("ess_dspwr(0x%02x) timed out.\n", val);
    	return 0;
}

static int
ess_cmd(struct ess_info *sc, u_char val)
{
	DEB(printf("ess_cmd: %x\n", val));
    	return ess_dspwr(sc, val);
}

static int
ess_cmd1(struct ess_info *sc, u_char cmd, int val)
{
    	DEB(printf("ess_cmd1: %x, %x\n", cmd, val));
    	if (ess_dspwr(sc, cmd)) {
		return ess_dspwr(sc, val & 0xff);
    	} else return 0;
}

static void
ess_setmixer(struct ess_info *sc, u_int port, u_int value)
{
    	u_long   flags;

	DEB(printf("ess_setmixer: reg=%x, val=%x\n", port, value);)
    	flags = spltty();
    	ess_wr(sc, SB_MIX_ADDR, (u_char) (port & 0xff)); /* Select register */
    	DELAY(10);
    	ess_wr(sc, SB_MIX_DATA, (u_char) (value & 0xff));
    	DELAY(10);
    	splx(flags);
}

static int
ess_getmixer(struct ess_info *sc, u_int port)
{
    	int val;
    	u_long flags;

    	flags = spltty();
    	ess_wr(sc, SB_MIX_ADDR, (u_char) (port & 0xff)); /* Select register */
    	DELAY(10);
    	val = ess_rd(sc, SB_MIX_DATA);
    	DELAY(10);
    	splx(flags);

    	return val;
}

static int
ess_get_byte(struct ess_info *sc)
{
    	int i;

    	for (i = 1000; i > 0; i--) {
		if (ess_rd(sc, 0xc) & 0x40)
			return ess_rd(sc, DSP_READ);
		else
			DELAY(20);
    	}
    	return -1;
}

static int
ess_write(struct ess_info *sc, u_char reg, int val)
{
    	return ess_cmd1(sc, reg, val);
}

static int
ess_read(struct ess_info *sc, u_char reg)
{
    	return (ess_cmd(sc, 0xc0) && ess_cmd(sc, reg))? ess_get_byte(sc) : -1;
}

static int
ess_reset_dsp(struct ess_info *sc)
{
	DEB(printf("ess_reset_dsp\n"));
    	ess_wr(sc, SBDSP_RST, 3);
    	DELAY(100);
    	ess_wr(sc, SBDSP_RST, 0);
    	if (ess_get_byte(sc) != 0xAA) {
        	DEB(printf("ess_reset_dsp failed\n"));
/*
			   rman_get_start(d->io_base)));
*/
		return ENXIO;	/* Sorry */
    	}
    	ess_cmd(sc, 0xc6);
    	return 0;
}

static void
ess_intr(void *arg)
{
    	struct ess_info *sc = (struct ess_info *)arg;
	int src, pirq = 0, rirq = 0;

	src = 0;
	if (ess_getmixer(sc, 0x7a) & 0x80)
		src |= 2;
	if (ess_rd(sc, 0x0c) & 0x01)
		src |= 1;

	if (src == 0)
		return;

	if (sc->duplex) {
		pirq = (src & sc->pch.hwch)? 1 : 0;
		rirq = (src & sc->rch.hwch)? 1 : 0;
	} else {
		if (sc->simplex_dir == PCMDIR_PLAY)
			pirq = 1;
		if (sc->simplex_dir == PCMDIR_REC)
			rirq = 1;
		if (!pirq && !rirq)
			printf("solo: IRQ neither playback nor rec!\n");
	}

	DEB(printf("ess_intr: pirq:%d rirq:%d\n",pirq,rirq));

	if (pirq) {
		if (sc->pch.stopping) {
			ess_dmatrigger(sc, sc->pch.hwch, 0);
			sc->pch.stopping = 0;
			if (sc->pch.hwch == 1)
				ess_write(sc, 0xb8, ess_read(sc, 0xb8) & ~0x01);
			else
				ess_setmixer(sc, 0x78, ess_getmixer(sc, 0x78) & ~0x03);
		}
		chn_intr(sc->pch.channel);
	}

	if (rirq) {
		if (sc->rch.stopping) {
			ess_dmatrigger(sc, sc->rch.hwch, 0);
			sc->rch.stopping = 0;
			/* XXX: will this stop audio2? */
			ess_write(sc, 0xb8, ess_read(sc, 0xb8) & ~0x01);
		}
		chn_intr(sc->rch.channel);
	}

	if (src & 2)
		ess_setmixer(sc, 0x7a, ess_getmixer(sc, 0x7a) & ~0x80);
	if (src & 1)
    		ess_rd(sc, DSP_DATA_AVAIL);
}

/* utility functions for ESS */
static u_int8_t
ess_calcspeed8(int *spd)
{
	int speed = *spd;
	u_int32_t t;

	if (speed > 22000) {
		t = (795500 + speed / 2) / speed;
		speed = (795500 + t / 2) / t;
		t = (256 - t) | 0x80;
	} else {
		t = (397700 + speed / 2) / speed;
		speed = (397700 + t / 2) / t;
		t = 128 - t;
	}
	*spd = speed;
	return t & 0x000000ff;
}

static u_int8_t
ess_calcspeed9(int *spd)
{
	int speed, s0, s1, use0;
	u_int8_t t0, t1;

	/* rate = source / (256 - divisor) */
	/* divisor = 256 - (source / rate) */
	speed = *spd;
	t0 = 128 - (793800 / speed);
	s0 = 793800 / (128 - t0);

	t1 = 128 - (768000 / speed);
	s1 = 768000 / (128 - t1);
	t1 |= 0x80;

	use0 = (ABS(speed - s0) < ABS(speed - s1))? 1 : 0;

	*spd = use0? s0 : s1;
	return use0? t0 : t1;
}

static u_int8_t
ess_calcfilter(int spd)
{
	int cutoff;

	/* cutoff = 7160000 / (256 - divisor) */
	/* divisor = 256 - (7160000 / cutoff) */
	cutoff = (spd * 9 * 82) / 20;
	return (256 - (7160000 / cutoff));
}

static int
ess_setupch(struct ess_info *sc, int ch, int dir, int spd, u_int32_t fmt, int len)
{
	int play = (dir == PCMDIR_PLAY)? 1 : 0;
	int b16 = (fmt & AFMT_16BIT)? 1 : 0;
	int stereo = (fmt & AFMT_STEREO)? 1 : 0;
	int unsign = (fmt == AFMT_U8 || fmt == AFMT_U16_LE || fmt == AFMT_U16_BE)? 1 : 0;
	u_int8_t spdval, fmtval;

	DEB(printf("ess_setupch\n"));
	spdval = (sc->newspeed)? ess_calcspeed9(&spd) : ess_calcspeed8(&spd);

	sc->simplex_dir = play ? PCMDIR_PLAY : PCMDIR_REC ;

	if (ch == 1) {
		KASSERT((dir == PCMDIR_PLAY) || (dir == PCMDIR_REC), ("ess_setupch: dir1 bad"));
		len = -len;
		/* transfer length low */
		ess_write(sc, 0xa4, len & 0x00ff);
		/* transfer length high */
		ess_write(sc, 0xa5, (len & 0xff00) >> 8);
		/* autoinit, dma dir */
		ess_write(sc, 0xb8, 0x04 | (play? 0x00 : 0x0a));
		/* mono/stereo */
		ess_write(sc, 0xa8, (ess_read(sc, 0xa8) & ~0x03) | (stereo? 0x01 : 0x02));
		/* demand mode, 4 bytes/xfer */
		ess_write(sc, 0xb9, 0x02);
		/* sample rate */
        	ess_write(sc, 0xa1, spdval);
		/* filter cutoff */
		ess_write(sc, 0xa2, ess_calcfilter(spd));
		/* setup dac/adc */
		/*
		if (play)
			ess_write(sc, 0xb6, unsign? 0x80 : 0x00);
		*/
		/* mono, b16: signed, load signal */
		/*
		ess_write(sc, 0xb7, 0x51 | (unsign? 0x00 : 0x20));
		*/
		/* setup fifo */
		ess_write(sc, 0xb7, 0x91 | (unsign? 0x00 : 0x20) |
					   (b16? 0x04 : 0x00) |
					   (stereo? 0x08 : 0x40));
		/* irq control */
		ess_write(sc, 0xb1, (ess_read(sc, 0xb1) & 0x0f) | 0x50);
		/* drq control */
		ess_write(sc, 0xb2, (ess_read(sc, 0xb2) & 0x0f) | 0x50);
	} else if (ch == 2) {
		KASSERT(dir == PCMDIR_PLAY, ("ess_setupch: dir2 bad"));
		len >>= 1;
		len = -len;
		/* transfer length low */
		ess_setmixer(sc, 0x74, len & 0x00ff);
		/* transfer length high */
		ess_setmixer(sc, 0x76, (len & 0xff00) >> 8);
		/* autoinit, 4 bytes/req */
		ess_setmixer(sc, 0x78, 0x10);
		fmtval = b16 | (stereo << 1) | ((!unsign) << 2);
		/* enable irq, set format */
		ess_setmixer(sc, 0x7a, 0x40 | fmtval);
		if (sc->newspeed) {
			/* sample rate */
			ess_setmixer(sc, 0x70, spdval);
			/* filter cutoff */
			ess_setmixer(sc, 0x72, ess_calcfilter(spd));
		}

	}
	return 0;
}
static int
ess_start(struct ess_chinfo *ch)
{
	struct ess_info *sc = ch->parent;

	DEB(printf("ess_start\n"););
	ess_setupch(sc, ch->hwch, ch->dir, ch->spd, ch->fmt, ch->buffer->dl);
	ch->stopping = 0;
	if (ch->hwch == 1) {
		ess_write(sc, 0xb8, ess_read(sc, 0xb8) | 0x01);
		if (ch->dir == PCMDIR_PLAY) {
#if 0
			DELAY(100000); /* 100 ms */
#endif
			ess_cmd(sc, 0xd1);
		}
	} else
		ess_setmixer(sc, 0x78, ess_getmixer(sc, 0x78) | 0x03);
	return 0;
}

static int
ess_stop(struct ess_chinfo *ch)
{
	struct ess_info *sc = ch->parent;

	DEB(printf("ess_stop\n"));
	ch->stopping = 1;
	if (ch->hwch == 1)
		ess_write(sc, 0xb8, ess_read(sc, 0xb8) & ~0x04);
	else
		ess_setmixer(sc, 0x78, ess_getmixer(sc, 0x78) & ~0x10);
	DEB(printf("done with stop\n"));
	return 0;
}

/* channel interface for ESS18xx */
static void *
esschan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	struct ess_info *sc = devinfo;
	struct ess_chinfo *ch = (dir == PCMDIR_PLAY)? &sc->pch : &sc->rch;

	DEB(printf("esschan_init\n"));
	ch->parent = sc;
	ch->channel = c;
	ch->buffer = b;
	ch->buffer->bufsize = ESS_BUFFSIZE;
	if (chn_allocbuf(ch->buffer, sc->parent_dmat) == -1)
		return NULL;
	ch->hwch = 1;
	if ((dir == PCMDIR_PLAY) && (sc->duplex))
		ch->hwch = 2;
	return ch;
}

static int
esschan_setdir(void *data, int dir)
{
	struct ess_chinfo *ch = data;

	ch->dir = dir;
	return 0;
}

static int
esschan_setformat(void *data, u_int32_t format)
{
	struct ess_chinfo *ch = data;

	ch->fmt = format;
	return 0;
}

static int
esschan_setspeed(void *data, u_int32_t speed)
{
	struct ess_chinfo *ch = data;
	struct ess_info *sc = ch->parent;

	ch->spd = speed;
	if (sc->newspeed)
		ess_calcspeed9(&ch->spd);
	else
		ess_calcspeed8(&ch->spd);
	return ch->spd;
}

static int
esschan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
esschan_trigger(void *data, int go)
{
	struct ess_chinfo *ch = data;
	struct ess_info *sc = ch->parent;

	DEB(printf("esschan_trigger: %d\n",go));
	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	switch (go) {
	case PCMTRIG_START:
		ess_dmasetup(sc, ch->hwch, vtophys(ch->buffer->buf), ch->buffer->bufsize, ch->dir);
		ess_dmatrigger(sc, ch->hwch, 1);
		ess_start(ch);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
	default:
		ess_stop(ch);
		break;
	}
	return 0;
}

static int
esschan_getptr(void *data)
{
	struct ess_chinfo *ch = data;
	struct ess_info *sc = ch->parent;

	return ess_dmapos(sc, ch->hwch);
}

static pcmchan_caps *
esschan_getcaps(void *data)
{
	struct ess_chinfo *ch = data;

	return (ch->dir == PCMDIR_PLAY)? &ess_playcaps : &ess_reccaps;
}

/************************************************************/

static int
essmix_init(snd_mixer *m)
{
    	struct ess_info *sc = mix_getdevinfo(m);

	mix_setrecdevs(m, SOUND_MASK_CD | SOUND_MASK_MIC | SOUND_MASK_LINE |
			  SOUND_MASK_IMIX);

	mix_setdevs(m, SOUND_MASK_SYNTH | SOUND_MASK_PCM | SOUND_MASK_LINE |
		       SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_VOLUME |
		       SOUND_MASK_LINE1);

	ess_setmixer(sc, 0, 0); /* reset */

	return 0;
}

static int
essmix_set(snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
    	struct ess_info *sc = mix_getdevinfo(m);
    	int preg = 0, rreg = 0, l, r;

	l = (left * 15) / 100;
	r = (right * 15) / 100;
	switch (dev) {
	case SOUND_MIXER_SYNTH:
		preg = 0x36;
		rreg = 0x6b;
		break;

	case SOUND_MIXER_PCM:
		preg = 0x14;
		rreg = 0x7c;
		break;

	case SOUND_MIXER_LINE:
		preg = 0x3e;
		rreg = 0x6e;
		break;

	case SOUND_MIXER_MIC:
		preg = 0x1a;
		rreg = 0x68;
		break;

	case SOUND_MIXER_LINE1:
		preg = 0x3a;
		rreg = 0x6c;
		break;

	case SOUND_MIXER_CD:
		preg = 0x38;
		rreg = 0x6a;
		break;

	case SOUND_MIXER_VOLUME:
		l = left? (left * 63) / 100 : 64;
		r = right? (right * 63) / 100 : 64;
		ess_setmixer(sc, 0x60, l);
		ess_setmixer(sc, 0x62, r);
		left = (l == 64)? 0 : (l * 100) / 63;
		right = (r == 64)? 0 : (r * 100) / 63;
    		return left | (right << 8);
	}

	if (preg)
		ess_setmixer(sc, preg, (l << 4) | r);
	if (rreg)
		ess_setmixer(sc, rreg, (l << 4) | r);

	left = (l * 100) / 15;
	right = (r * 100) / 15;

    	return left | (right << 8);
}

static int
essmix_setrecsrc(snd_mixer *m, u_int32_t src)
{
    	struct ess_info *sc = mix_getdevinfo(m);
    	u_char recdev;

    	switch (src) {
	case SOUND_MASK_CD:
		recdev = 0x02;
		break;

	case SOUND_MASK_LINE:
		recdev = 0x06;
		break;

	case SOUND_MASK_IMIX:
		recdev = 0x05;
		break;

	case SOUND_MASK_MIC:
	default:
		recdev = 0x00;
		src = SOUND_MASK_MIC;
		break;
	}

	ess_setmixer(sc, 0x1c, recdev);

	return src;
}

/************************************************************/

static int
ess_dmasetup(struct ess_info *sc, int ch, u_int32_t base, u_int16_t cnt, int dir)
{
	KASSERT(ch == 1 || ch == 2, ("bad ch"));
	sc->dmasz[ch - 1] = cnt;
	if (ch == 1) {
		port_wr(sc->vc, 0x8, 0xc4, 1); /* command */
		port_wr(sc->vc, 0xd, 0xff, 1); /* reset */
		port_wr(sc->vc, 0xf, 0x01, 1); /* mask */
		port_wr(sc->vc, 0xb, dir == PCMDIR_PLAY? 0x58 : 0x54, 1); /* mode */
		port_wr(sc->vc, 0x0, base, 4);
		port_wr(sc->vc, 0x4, cnt - 1, 2);

	} else if (ch == 2) {
		port_wr(sc->io, 0x6, 0x08, 1); /* autoinit */
		port_wr(sc->io, 0x0, base, 4);
		port_wr(sc->io, 0x4, cnt, 2);
	}
	return 0;
}

static int
ess_dmapos(struct ess_info *sc, int ch)
{
	int p = 0, i = 0, j = 0;
	u_long flags;

	KASSERT(ch == 1 || ch == 2, ("bad ch"));
	flags = spltty();
	if (ch == 1) {

/*
 * During recording, this register is known to give back
 * garbage if it's not quiescent while being read. That's
 * why we spl, stop the DMA, and try over and over until
 * adjacent reads are "close", in the right order and not
 * bigger than is otherwise possible.
 */
		ess_dmatrigger(sc, ch, 0);
		DELAY(20);
		do {
			DELAY(10);
			if (j > 1)
				printf("DMA count reg bogus: %04x & %04x\n",
					i, p);
			i = port_rd(sc->vc, 0x4, 2) + 1;
			p = port_rd(sc->vc, 0x4, 2) + 1;
		} while ((p > sc->dmasz[ch -1 ] || i < p || (p - i) > 0x8) && j++ < 1000);
		ess_dmatrigger(sc, ch, 1);
	}
	else if (ch == 2)
		p = port_rd(sc->io, 0x4, 2);
	splx(flags);
	return sc->dmasz[ch - 1] - p;
}

static int
ess_dmatrigger(struct ess_info *sc, int ch, int go)
{
	KASSERT(ch == 1 || ch == 2, ("bad ch"));
	if (ch == 1)
		port_wr(sc->vc, 0xf, go? 0x00 : 0x01, 1); /* mask */
	else if (ch == 2)
		port_wr(sc->io, 0x6, 0x08 | (go? 0x02 : 0x00), 1); /* autoinit */
	return 0;
}

static void
ess_release_resources(struct ess_info *sc, device_t dev)
{
    	/* should we bus_teardown_intr here? */
    	if (sc->irq) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
		sc->irq = 0;
    	}
    	if (sc->io) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0 * 4 + PCIR_MAPS, sc->io);
		sc->io = 0;
    	}

    	if (sc->sb) {
		bus_release_resource(dev, SYS_RES_IOPORT, 1 * 4 + PCIR_MAPS, sc->sb);
		sc->sb = 0;
    	}

    	if (sc->vc) {
		bus_release_resource(dev, SYS_RES_IOPORT, 2 * 4 + PCIR_MAPS, sc->vc);
		sc->vc = 0;
    	}

    	if (sc->mpu) {
		bus_release_resource(dev, SYS_RES_IOPORT, 3 * 4 + PCIR_MAPS, sc->mpu);
		sc->mpu = 0;
    	}

    	if (sc->gp) {
		bus_release_resource(dev, SYS_RES_IOPORT, 4 * 4 + PCIR_MAPS, sc->gp);
		sc->gp = 0;
    	}

    	free(sc, M_DEVBUF);
}

static int
ess_alloc_resources(struct ess_info *sc, device_t dev)
{
	int rid;

	rid = 0 * 4 + PCIR_MAPS;
    	sc->io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);

	rid = 1 * 4 + PCIR_MAPS;
    	sc->sb = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);

	rid = 2 * 4 + PCIR_MAPS;
    	sc->vc = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);

	rid = 3 * 4 + PCIR_MAPS;
    	sc->mpu = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);

	rid = 4 * 4 + PCIR_MAPS;
    	sc->gp = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);

	rid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);

	return (sc->irq && sc->io && sc->sb && sc->vc && sc->mpu && sc->gp)? 0 : ENXIO;
}

static int
ess_probe(device_t dev)
{
	char *s = NULL;
	u_int32_t subdev;

	subdev = (pci_get_subdevice(dev) << 16) | pci_get_subvendor(dev);
	switch (pci_get_devid(dev)) {
	case 0x1969125d:
		if (subdev == 0x8888125d)
			s = "ESS Solo-1E";
		else if (subdev == 0x1818125d)
			s = "ESS Solo-1";
		else
			s = "ESS Solo-1 (unknown vendor)";
		break;
	}

	if (s)
		device_set_desc(dev, s);
	return s? 0 : ENXIO;
}

#define PCI_LEGACYCONTROL       0x40
#define PCI_CONFIG              0x50
#define PCI_DDMACONTROL      	0x60

static int
ess_attach(device_t dev)
{
    	struct ess_info *sc;
    	void *ih;
    	char status[SND_STATUSLEN];
	u_int16_t ddma;
	u_int32_t data;

	sc = (struct ess_info *)malloc(sizeof *sc, M_DEVBUF, M_NOWAIT);
    	if (!sc)
		return ENXIO;
    	bzero(sc, sizeof *sc);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

    	if (ess_alloc_resources(sc, dev))
		goto no;

	ddma = rman_get_start(sc->vc) | 1;
	pci_write_config(dev, PCI_LEGACYCONTROL, 0x805f, 2);
	pci_write_config(dev, PCI_DDMACONTROL, ddma, 2);
	pci_write_config(dev, PCI_CONFIG, 0, 2);

    	if (ess_reset_dsp(sc))
		goto no;
    	mixer_init(dev, &ess_mixer, sc);

	port_wr(sc->io, 0x7, 0xb0, 1); /* enable irqs */
#ifdef ESS18XX_DUPLEX
	sc->duplex = 1;
#else
	sc->duplex = 0;
#endif

#ifdef ESS18XX_NEWSPEED
	sc->newspeed = 1;
#else
	sc->newspeed = 0;
#endif
	if (sc->newspeed)
		ess_setmixer(sc, 0x71, 0x2a);

	bus_setup_intr(dev, sc->irq, INTR_TYPE_TTY, ess_intr, sc, &ih);
    	if (!sc->duplex)
		pcm_setflags(dev, pcm_getflags(dev) | SD_F_SIMPLEX);

    	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/65536, /*boundary*/0,
			/*lowaddr*/BUS_SPACE_MAXADDR_24BIT,
			/*highaddr*/BUS_SPACE_MAXADDR,
			/*filter*/NULL, /*filterarg*/NULL,
			/*maxsize*/ESS_BUFFSIZE, /*nsegments*/1,
			/*maxsegz*/0x3ffff,
			/*flags*/0, &sc->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto no;
    	}

    	snprintf(status, SND_STATUSLEN, "at io 0x%lx,0x%lx,0x%lx irq %ld",
    	     	rman_get_start(sc->io), rman_get_start(sc->sb), rman_get_start(sc->vc),
		rman_get_start(sc->irq));

    	if (pcm_register(dev, sc, 1, 1))
		goto no;
      	pcm_addchan(dev, PCMDIR_REC, &ess_chantemplate, sc);
	pcm_addchan(dev, PCMDIR_PLAY, &ess_chantemplate, sc);
	pcm_setstatus(dev, status);

    	return 0;

no:
    	ess_release_resources(sc, dev);
    	return ENXIO;
}

static device_method_t ess_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ess_probe),
	DEVMETHOD(device_attach,	ess_attach),

	{ 0, 0 }
};

static driver_t ess_driver = {
	"pcm",
	ess_methods,
	sizeof(snddev_info),
};

DRIVER_MODULE(snd_solo, pci, ess_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_solo, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_solo, 1);



