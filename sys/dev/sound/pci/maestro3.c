/*-
 * Copyright (c) 2001 Scott Long <scottl@freebsd.org>
 * Copyright (c) 2001 Darrell Anderson <anderson@cs.duke.edu>
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
 * Maestro-3/Allegro FreeBSD pcm sound driver
 *
 * executive status summary:
 * (+) /dev/dsp multiple concurrent play channels.
 * (+) /dev/dsp config (speed, mono/stereo, 8/16 bit).
 * (+) /dev/mixer sets left/right volumes.
 * (+) /dev/dsp recording works.  Tested successfully with the cdrom channel
 * (+) apm suspend/resume works, and works properly!.
 * (-) hardware volme controls don't work =-(
 * (-) setblocksize() does nothing.
 *
 * The real credit goes to:
 *
 * Zach Brown for his Linux driver core and helpful technical comments.
 * <zab@zabbo.net>, http://www.zabbo.net/maestro3
 *
 * Cameron Grant created the pcm framework used here nearly verbatim.
 * <cg@freebsd.org>, http://people.freebsd.org/~cg/template.c
 *
 * Taku YAMAMOTO for his Maestro-1/2 FreeBSD driver and sanity reference.
 * <taku@cent.saitama-u.ac.jp>
 *
 * ESS docs explained a few magic registers and numbers.
 * http://virgo.caltech.edu/~dmoore/maestro3.pdf.gz
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <gnu/dev/sound/pci/maestro3_reg.h>
#include <gnu/dev/sound/pci/maestro3_dsp.h>

SND_DECLARE_FILE("$FreeBSD$");

/* -------------------------------------------------------------------- */

enum {CHANGE=0, CALL=1, INTR=2, BORING=3, NONE=-1};
#ifndef M3_DEBUG_LEVEL
#define M3_DEBUG_LEVEL NONE
#endif
#define M3_DEBUG(level, _msg) {if ((level) <= M3_DEBUG_LEVEL) {printf _msg;}}

/* -------------------------------------------------------------------- */
enum {
	ESS_ALLEGRO_1,
	ESS_MAESTRO3
};

static struct m3_card_type {
	u_int32_t pci_id; int which; int delay1; int delay2; char *name;
} m3_card_types[] = {
	{ 0x1988125d, ESS_ALLEGRO_1, 50, 800, "ESS Technology Allegro-1" },
	{ 0x1998125d, ESS_MAESTRO3, 20, 500, "ESS Technology Maestro3" },
	{ 0x199a125d, ESS_MAESTRO3, 20, 500, "ESS Technology Maestro3" },
	{ 0, 0, 0, 0, NULL }
};

#define M3_BUFSIZE_DEFAULT 4096
#define M3_PCHANS 4 /* create /dev/dsp0.[0-N] to use more than one */
#define M3_RCHANS 1
#define M3_MAXADDR ((1 << 27) - 1)

struct sc_info;

struct sc_pchinfo {
	u_int32_t	spd;
	u_int32_t	fmt;
	struct snd_dbuf	*buffer;
	struct pcm_channel	*channel;
	struct sc_info	*parent;
	u_int32_t	bufsize;
	u_int32_t	dac_data;
	u_int32_t	dac_idx;
	u_int32_t	active;
};

struct sc_rchinfo {
	u_int32_t	spd;
	u_int32_t	fmt;
	struct snd_dbuf	*buffer;
	struct pcm_channel	*channel;
	struct sc_info	*parent;
	u_int32_t	bufsize;
	u_int32_t	adc_data;
	u_int32_t	adc_idx;
	u_int32_t	active;
};

struct sc_info {
	device_t		dev;
	u_int32_t		type;
	int			which;
	int			delay1;
	int			delay2;

	bus_space_tag_t		st;
	bus_space_handle_t	 sh;
	bus_dma_tag_t		parent_dmat;

	struct resource		*reg;
	struct resource		*irq;
	int			regtype;
	int			regid;
	int			irqid;
	void			*ih;

	struct sc_pchinfo	pch[M3_PCHANS];
	struct sc_rchinfo	rch[M3_RCHANS];
	int			pch_cnt;
	int			rch_cnt;
	int			pch_active_cnt;
	unsigned int		bufsz;
	u_int16_t		*savemem;
};

/* -------------------------------------------------------------------- */

/* play channel interface */
static void *m3_pchan_init(kobj_t, void *, struct snd_dbuf *, struct pcm_channel *, int);
static int m3_pchan_free(kobj_t, void *);
static int m3_pchan_setformat(kobj_t, void *, u_int32_t);
static int m3_pchan_setspeed(kobj_t, void *, u_int32_t);
static int m3_pchan_setblocksize(kobj_t, void *, u_int32_t);
static int m3_pchan_trigger(kobj_t, void *, int);
static int m3_pchan_getptr(kobj_t, void *);
static struct pcmchan_caps *m3_pchan_getcaps(kobj_t, void *);

/* record channel interface */
static void *m3_rchan_init(kobj_t, void *, struct snd_dbuf *, struct pcm_channel *, int);
static int m3_rchan_free(kobj_t, void *);
static int m3_rchan_setformat(kobj_t, void *, u_int32_t);
static int m3_rchan_setspeed(kobj_t, void *, u_int32_t);
static int m3_rchan_setblocksize(kobj_t, void *, u_int32_t);
static int m3_rchan_trigger(kobj_t, void *, int);
static int m3_rchan_getptr(kobj_t, void *);
static struct pcmchan_caps *m3_rchan_getcaps(kobj_t, void *);

/* talk to the codec - called from ac97.c */
static int	 m3_initcd(kobj_t, void *);
static int	 m3_rdcd(kobj_t, void *, int);
static int  	 m3_wrcd(kobj_t, void *, int, u_int32_t);

/* stuff */
static void      m3_intr(void *);
static int       m3_power(struct sc_info *, int);
static int       m3_init(struct sc_info *);
static int       m3_uninit(struct sc_info *);
static u_int8_t	 m3_assp_halt(struct sc_info *);
static void	 m3_config(struct sc_info *);
static void	 m3_amp_enable(struct sc_info *);
static void	 m3_enable_ints(struct sc_info *);
static void	 m3_codec_reset(struct sc_info *);

/* -------------------------------------------------------------------- */
/* Codec descriptor */
static kobj_method_t m3_codec_methods[] = {
	KOBJMETHOD(ac97_init,	m3_initcd),
	KOBJMETHOD(ac97_read,	m3_rdcd),
	KOBJMETHOD(ac97_write,	m3_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(m3_codec);

/* -------------------------------------------------------------------- */
/* channel descriptors */

static u_int32_t m3_playfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps m3_playcaps = {8000, 48000, m3_playfmt, 0};

static kobj_method_t m3_pch_methods[] = {
	KOBJMETHOD(channel_init,		m3_pchan_init),
	KOBJMETHOD(channel_setformat,		m3_pchan_setformat),
	KOBJMETHOD(channel_setspeed,		m3_pchan_setspeed),
	KOBJMETHOD(channel_setblocksize,	m3_pchan_setblocksize),
	KOBJMETHOD(channel_trigger,		m3_pchan_trigger),
	KOBJMETHOD(channel_getptr,		m3_pchan_getptr),
	KOBJMETHOD(channel_getcaps,		m3_pchan_getcaps),
	KOBJMETHOD(channel_free,		m3_pchan_free),
	{ 0, 0 }
};
CHANNEL_DECLARE(m3_pch);

static u_int32_t m3_recfmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps m3_reccaps = {8000, 48000, m3_recfmt, 0};

static kobj_method_t m3_rch_methods[] = {
	KOBJMETHOD(channel_init,		m3_rchan_init),
	KOBJMETHOD(channel_setformat,		m3_rchan_setformat),
	KOBJMETHOD(channel_setspeed,		m3_rchan_setspeed),
	KOBJMETHOD(channel_setblocksize,	m3_rchan_setblocksize),
	KOBJMETHOD(channel_trigger,		m3_rchan_trigger),
	KOBJMETHOD(channel_getptr,		m3_rchan_getptr),
	KOBJMETHOD(channel_getcaps,		m3_rchan_getcaps),
	KOBJMETHOD(channel_free,		m3_rchan_free),
	{ 0, 0 }
};
CHANNEL_DECLARE(m3_rch);

/* -------------------------------------------------------------------- */
/* some i/o convenience functions */

#define m3_rd_1(sc, regno) bus_space_read_1(sc->st, sc->sh, regno)
#define m3_rd_2(sc, regno) bus_space_read_2(sc->st, sc->sh, regno)
#define m3_rd_4(sc, regno) bus_space_read_4(sc->st, sc->sh, regno)
#define m3_wr_1(sc, regno, data) bus_space_write_1(sc->st, sc->sh, regno, data)
#define m3_wr_2(sc, regno, data) bus_space_write_2(sc->st, sc->sh, regno, data)
#define m3_wr_4(sc, regno, data) bus_space_write_4(sc->st, sc->sh, regno, data)
#define m3_rd_assp_code(sc, index) \
        m3_rd_assp(sc, MEMTYPE_INTERNAL_CODE, index)
#define m3_wr_assp_code(sc, index, data) \
        m3_wr_assp(sc, MEMTYPE_INTERNAL_CODE, index, data)
#define m3_rd_assp_data(sc, index) \
        m3_rd_assp(sc, MEMTYPE_INTERNAL_DATA, index)
#define m3_wr_assp_data(sc, index, data) \
        m3_wr_assp(sc, MEMTYPE_INTERNAL_DATA, index, data)

static __inline u_int16_t
m3_rd_assp(struct sc_info *sc, u_int16_t region, u_int16_t index)
{
        m3_wr_2(sc, DSP_PORT_MEMORY_TYPE, region & MEMTYPE_MASK);
        m3_wr_2(sc, DSP_PORT_MEMORY_INDEX, index);
        return m3_rd_2(sc, DSP_PORT_MEMORY_DATA);
}

static __inline void
m3_wr_assp(struct sc_info *sc, u_int16_t region, u_int16_t index,
	   u_int16_t data)
{
        m3_wr_2(sc, DSP_PORT_MEMORY_TYPE, region & MEMTYPE_MASK);
        m3_wr_2(sc, DSP_PORT_MEMORY_INDEX, index);
        m3_wr_2(sc, DSP_PORT_MEMORY_DATA, data);
}

static __inline int
m3_wait(struct sc_info *sc)
{
	int i;

	for (i=0 ; i<20 ; i++) {
		if ((m3_rd_1(sc, CODEC_STATUS) & 1) == 0) {
			return 0;
		}
		DELAY(2);
	}
	return -1;
}

/* -------------------------------------------------------------------- */
/* ac97 codec */

static int
m3_initcd(kobj_t kobj, void *devinfo)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	u_int32_t data;

	M3_DEBUG(CALL, ("m3_initcd\n"));

	/* init ac-link */

	data = m3_rd_1(sc, CODEC_COMMAND);
	return ((data & 0x1) ? 0 : 1);
}

static int
m3_rdcd(kobj_t kobj, void *devinfo, int regno)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	u_int32_t data;

	if (m3_wait(sc)) {
		device_printf(sc->dev, "m3_rdcd timed out.\n");
		return -1;
	}
	m3_wr_1(sc, CODEC_COMMAND, (regno & 0x7f) | 0x80);
	DELAY(50); /* ac97 cycle = 20.8 usec */
	if (m3_wait(sc)) {
		device_printf(sc->dev, "m3_rdcd timed out.\n");
		return -1;
	}
	data = m3_rd_2(sc, CODEC_DATA);
	return data;
}

static int
m3_wrcd(kobj_t kobj, void *devinfo, int regno, u_int32_t data)
{
	struct sc_info *sc = (struct sc_info *)devinfo;
	if (m3_wait(sc)) {
		device_printf(sc->dev, "m3_wrcd timed out.\n");
		return -1;;
	}
	m3_wr_2(sc, CODEC_DATA, data);
	m3_wr_1(sc, CODEC_COMMAND, regno & 0x7f);
	DELAY(50); /* ac97 cycle = 20.8 usec */
	return 0;
}

/* -------------------------------------------------------------------- */
/* play channel interface */

#define LO(x) (((x) & 0x0000ffff)      )
#define HI(x) (((x) & 0xffff0000) >> 16)

static void *
m3_pchan_init(kobj_t kobj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_pchinfo *ch;
	u_int32_t bus_addr, i;

	int idx = sc->pch_cnt; /* dac instance number, no active reuse! */
	int data_bytes = (((MINISRC_TMP_BUFFER_SIZE & ~1) +
			   (MINISRC_IN_BUFFER_SIZE & ~1) +
			   (MINISRC_OUT_BUFFER_SIZE & ~1) + 4) + 255) &~ 255;
	int dac_data = 0x1100 + (data_bytes * idx);

	int dsp_in_size = MINISRC_IN_BUFFER_SIZE - (0x20 * 2);
	int dsp_out_size = MINISRC_OUT_BUFFER_SIZE - (0x20 * 2);
	int dsp_in_buf = dac_data + (MINISRC_TMP_BUFFER_SIZE/2);
	int dsp_out_buf = dsp_in_buf + (dsp_in_size/2) + 1;

        M3_DEBUG(CHANGE, ("m3_pchan_init(dac=%d)\n", idx));

	if (dir != PCMDIR_PLAY) {
		device_printf(sc->dev, "m3_pchan_init not PCMDIR_PLAY\n");
		return NULL;
	}
	ch = &sc->pch[idx];

	ch->dac_idx = idx;
	ch->dac_data = dac_data;
	if (ch->dac_data + data_bytes/2 >= 0x1c00) {
		device_printf(sc->dev, "m3_pchan_init: revb mem exhausted\n");
		return NULL;
	}

	ch->buffer = b;
	ch->parent = sc;
	ch->channel = c;
	ch->fmt = AFMT_U8;
	ch->spd = DSP_DEFAULT_SPEED;
	if (sndbuf_alloc(ch->buffer, sc->parent_dmat, sc->bufsz) == -1) {
		device_printf(sc->dev, "m3_pchan_init chn_allocbuf failed\n");
		return NULL;
	}
	ch->bufsize = sndbuf_getsize(ch->buffer);

	/* host dma buffer pointers */
	bus_addr = sndbuf_getbufaddr(ch->buffer);
	if (bus_addr & 3) {
		device_printf(sc->dev, "m3_pchan_init unaligned bus_addr\n");
		bus_addr = (bus_addr + 4) & ~3;
	}
	m3_wr_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_ADDRL, LO(bus_addr));
	m3_wr_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_ADDRH, HI(bus_addr));
	m3_wr_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_END_PLUS_1L,
			LO(bus_addr + ch->bufsize));
	m3_wr_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_END_PLUS_1H,
			HI(bus_addr + ch->bufsize));
	m3_wr_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_CURRENTL,
			LO(bus_addr));
	m3_wr_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_CURRENTH,
			HI(bus_addr));

	/* dsp buffers */
	m3_wr_assp_data(sc, ch->dac_data + CDATA_IN_BUF_BEGIN, dsp_in_buf);
	m3_wr_assp_data(sc, ch->dac_data + CDATA_IN_BUF_END_PLUS_1,
			dsp_in_buf + dsp_in_size/2);
	m3_wr_assp_data(sc, ch->dac_data + CDATA_IN_BUF_HEAD, dsp_in_buf);
	m3_wr_assp_data(sc, ch->dac_data + CDATA_IN_BUF_TAIL, dsp_in_buf);
	m3_wr_assp_data(sc, ch->dac_data + CDATA_OUT_BUF_BEGIN, dsp_out_buf);
	m3_wr_assp_data(sc, ch->dac_data + CDATA_OUT_BUF_END_PLUS_1,
			dsp_out_buf + dsp_out_size/2);
	m3_wr_assp_data(sc, ch->dac_data + CDATA_OUT_BUF_HEAD, dsp_out_buf);
	m3_wr_assp_data(sc, ch->dac_data + CDATA_OUT_BUF_TAIL, dsp_out_buf);

	/* some per client initializers */
	m3_wr_assp_data(sc, ch->dac_data + SRC3_DIRECTION_OFFSET + 12,
			ch->dac_data + 40 + 8);
	m3_wr_assp_data(sc, ch->dac_data + SRC3_DIRECTION_OFFSET + 19,
			0x400 + MINISRC_COEF_LOC);
	/* enable or disable low pass filter? (0xff if rate> 45000) */
	m3_wr_assp_data(sc, ch->dac_data + SRC3_DIRECTION_OFFSET + 22, 0);
	/* tell it which way dma is going? */
	m3_wr_assp_data(sc, ch->dac_data + CDATA_DMA_CONTROL,
			DMACONTROL_AUTOREPEAT + DMAC_PAGE3_SELECTOR +
			DMAC_BLOCKF_SELECTOR);

	/* set an armload of static initializers */
	for(i = 0 ; i < (sizeof(pv) / sizeof(pv[0])) ; i++) {
		m3_wr_assp_data(sc, ch->dac_data + pv[i].addr, pv[i].val);
	}

	/* put us in the packed task lists */
	m3_wr_assp_data(sc, KDATA_INSTANCE0_MINISRC +
			(sc->pch_cnt + sc->rch_cnt),
			ch->dac_data >> DP_SHIFT_COUNT);
	m3_wr_assp_data(sc, KDATA_DMA_XFER0 + (sc->pch_cnt + sc->rch_cnt),
			ch->dac_data >> DP_SHIFT_COUNT);
	m3_wr_assp_data(sc, KDATA_MIXER_XFER0 + sc->pch_cnt,
			ch->dac_data >> DP_SHIFT_COUNT);

	m3_pchan_trigger(NULL, ch, PCMTRIG_START); /* gotta start before stop */
	m3_pchan_trigger(NULL, ch, PCMTRIG_STOP); /* silence noise on load */

	sc->pch_cnt++;
	return ch;
}

static int
m3_pchan_free(kobj_t kobj, void *chdata)
{
	struct sc_pchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;

        M3_DEBUG(CHANGE, ("m3_pchan_free(dac=%d)\n", ch->dac_idx));

	/*
	 * should remove this exact instance from the packed lists, but all
	 * are released at once (and in a stopped state) so this is ok.
	 */
	m3_wr_assp_data(sc, KDATA_INSTANCE0_MINISRC +
			(sc->pch_cnt - 1) + sc->rch_cnt, 0);
	m3_wr_assp_data(sc, KDATA_DMA_XFER0 +
			(sc->pch_cnt - 1) + sc->rch_cnt, 0);
	m3_wr_assp_data(sc, KDATA_MIXER_XFER0 + (sc->pch_cnt-1), 0);

	sc->pch_cnt--;
	return 0;
}

static int
m3_pchan_setformat(kobj_t kobj, void *chdata, u_int32_t format)
{
	struct sc_pchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t data;

	M3_DEBUG(CHANGE,
		 ("m3_pchan_setformat(dac=%d, format=0x%x{%s-%s})\n",
		  ch->dac_idx, format,
		  format & (AFMT_U8|AFMT_S8) ? "8bit":"16bit",
		  format & AFMT_STEREO ? "STEREO":"MONO"));

	/* mono word */
        data = (format & AFMT_STEREO) ? 0 : 1;
        m3_wr_assp_data(sc, ch->dac_data + SRC3_MODE_OFFSET, data);

        /* 8bit word */
        data = ((format & AFMT_U8) || (format & AFMT_S8)) ? 1 : 0;
        m3_wr_assp_data(sc, ch->dac_data + SRC3_WORD_LENGTH_OFFSET, data);

        ch->fmt = format;
        return 0;
}

static int
m3_pchan_setspeed(kobj_t kobj, void *chdata, u_int32_t speed)
{
	struct sc_pchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t freq;

	M3_DEBUG(CHANGE, ("m3_pchan_setspeed(dac=%d, speed=%d)\n",
			  ch->dac_idx, speed));

        if ((freq = ((speed << 15) + 24000) / 48000) != 0) {
                freq--;
        }

        m3_wr_assp_data(sc, ch->dac_data + CDATA_FREQUENCY, freq);

	ch->spd = speed;
	return speed; /* return closest possible speed */
}

static int
m3_pchan_setblocksize(kobj_t kobj, void *chdata, u_int32_t blocksize)
{
	struct sc_pchinfo *ch = chdata;

	M3_DEBUG(CHANGE, ("m3_pchan_setblocksize(dac=%d, blocksize=%d)\n",
			  ch->dac_idx, blocksize));

	return blocksize;
}

static int
m3_pchan_trigger(kobj_t kobj, void *chdata, int go)
{
	struct sc_pchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t data;

	M3_DEBUG(go == PCMTRIG_START ? CHANGE :
		 go == PCMTRIG_STOP ? CHANGE :
		 go == PCMTRIG_ABORT ? CHANGE :
		 CALL,
		 ("m3_pchan_trigger(dac=%d, go=0x%x{%s})\n", ch->dac_idx, go,
		  go == PCMTRIG_START ? "PCMTRIG_START" :
		  go == PCMTRIG_STOP ? "PCMTRIG_STOP" :
		  go == PCMTRIG_ABORT ? "PCMTRIG_ABORT" : "ignore"));

	switch(go) {
	case PCMTRIG_START:
		if (ch->active) {
			return 0;
		}
		ch->active = 1;
		sc->pch_active_cnt++;

		/*[[inc_timer_users]]*/
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_RELOAD, 240);
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_CURRENT, 240);
                data = m3_rd_2(sc, HOST_INT_CTRL);
                m3_wr_2(sc, HOST_INT_CTRL, data | CLKRUN_GEN_ENABLE);

                m3_wr_assp_data(sc, ch->dac_data + CDATA_INSTANCE_READY, 1);
                m3_wr_assp_data(sc, KDATA_MIXER_TASK_NUMBER,
				sc->pch_active_cnt);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		if (ch->active == 0) {
			return 0;
		}
		ch->active = 0;
		sc->pch_active_cnt--;

		/* XXX should the channel be drained? */
		/*[[dec_timer_users]]*/
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_RELOAD, 0);
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_CURRENT, 0);
                data = m3_rd_2(sc, HOST_INT_CTRL);
                m3_wr_2(sc, HOST_INT_CTRL, data & ~CLKRUN_GEN_ENABLE);

                m3_wr_assp_data(sc, ch->dac_data + CDATA_INSTANCE_READY, 0);
                m3_wr_assp_data(sc, KDATA_MIXER_TASK_NUMBER,
				sc->pch_active_cnt);
		break;

	case PCMTRIG_EMLDMAWR:
		/* got play irq, transfer next buffer - ignore if using dma */
	case PCMTRIG_EMLDMARD:
		/* got rec irq, transfer next buffer - ignore if using dma */
	default:
		break;
	}
	return 0;
}

static int
m3_pchan_getptr(kobj_t kobj, void *chdata)
{
	struct sc_pchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t hi, lo, bus_crnt;
	u_int32_t bus_base = sndbuf_getbufaddr(ch->buffer);

	hi = m3_rd_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_CURRENTH);
        lo = m3_rd_assp_data(sc, ch->dac_data + CDATA_HOST_SRC_CURRENTL);
        bus_crnt = lo | (hi << 16);

	M3_DEBUG(CALL, ("m3_pchan_getptr(dac=%d) result=%d\n",
			ch->dac_idx, bus_crnt - bus_base));

	return (bus_crnt - bus_base); /* current byte offset of channel */
}

static struct pcmchan_caps *
m3_pchan_getcaps(kobj_t kobj, void *chdata)
{
	struct sc_pchinfo *ch = chdata;

        M3_DEBUG(CALL, ("m3_pchan_getcaps(dac=%d)\n", ch->dac_idx));

	return &m3_playcaps;
}

/* -------------------------------------------------------------------- */
/* rec channel interface */

static void *
m3_rchan_init(kobj_t kobj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct sc_info *sc = devinfo;
	struct sc_rchinfo *ch;
	u_int32_t bus_addr, i;

	int idx = sc->rch_cnt; /* adc instance number, no active reuse! */
	int data_bytes = (((MINISRC_TMP_BUFFER_SIZE & ~1) +
			   (MINISRC_IN_BUFFER_SIZE & ~1) +
			   (MINISRC_OUT_BUFFER_SIZE & ~1) + 4) + 255) &~ 255;
	int adc_data = 0x1100 + (data_bytes * idx) + data_bytes/2;

	int dsp_in_size = MINISRC_IN_BUFFER_SIZE + (0x10 * 2);
	int dsp_out_size = MINISRC_OUT_BUFFER_SIZE - (0x10 * 2);
	int dsp_in_buf = adc_data + (MINISRC_TMP_BUFFER_SIZE / 2);
	int dsp_out_buf = dsp_in_buf + (dsp_in_size / 2) + 1;

        M3_DEBUG(CHANGE, ("m3_rchan_init(adc=%d)\n", idx));

	if (dir != PCMDIR_REC) {
		device_printf(sc->dev, "m3_pchan_init not PCMDIR_REC\n");
		return NULL;
	}
	ch = &sc->rch[idx];

	ch->adc_idx = idx;
	ch->adc_data = adc_data;
	if (ch->adc_data + data_bytes/2 >= 0x1c00) {
		device_printf(sc->dev, "m3_rchan_init: revb mem exhausted\n");
		return NULL;
	}

	ch->buffer = b;
	ch->parent = sc;
	ch->channel = c;
	ch->fmt = AFMT_U8;
	ch->spd = DSP_DEFAULT_SPEED;
	if (sndbuf_alloc(ch->buffer, sc->parent_dmat, sc->bufsz) == -1) {
		device_printf(sc->dev, "m3_rchan_init chn_allocbuf failed\n");
		return NULL;
	}
	ch->bufsize = sndbuf_getsize(ch->buffer);

	/* host dma buffer pointers */
	bus_addr = sndbuf_getbufaddr(ch->buffer);
	if (bus_addr & 3) {
		device_printf(sc->dev, "m3_rchan_init unaligned bus_addr\n");
		bus_addr = (bus_addr + 4) & ~3;
	}
	m3_wr_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_ADDRL, LO(bus_addr));
	m3_wr_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_ADDRH, HI(bus_addr));
	m3_wr_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_END_PLUS_1L,
			LO(bus_addr + ch->bufsize));
	m3_wr_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_END_PLUS_1H,
			HI(bus_addr + ch->bufsize));
	m3_wr_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_CURRENTL,
			LO(bus_addr));
	m3_wr_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_CURRENTH,
			HI(bus_addr));

	/* dsp buffers */
	m3_wr_assp_data(sc, ch->adc_data + CDATA_IN_BUF_BEGIN, dsp_in_buf);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_IN_BUF_END_PLUS_1,
			dsp_in_buf + dsp_in_size/2);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_IN_BUF_HEAD, dsp_in_buf);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_IN_BUF_TAIL, dsp_in_buf);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_OUT_BUF_BEGIN, dsp_out_buf);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_OUT_BUF_END_PLUS_1,
			dsp_out_buf + dsp_out_size/2);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_OUT_BUF_HEAD, dsp_out_buf);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_OUT_BUF_TAIL, dsp_out_buf);

	/* some per client initializers */
	m3_wr_assp_data(sc, ch->adc_data + SRC3_DIRECTION_OFFSET + 12,
			ch->adc_data + 40 + 8);
	m3_wr_assp_data(sc, ch->adc_data + CDATA_DMA_CONTROL,
			DMACONTROL_DIRECTION + DMACONTROL_AUTOREPEAT +
			DMAC_PAGE3_SELECTOR + DMAC_BLOCKF_SELECTOR);

	/* set an armload of static initializers */
	for(i = 0 ; i < (sizeof(rv) / sizeof(rv[0])) ; i++) {
		m3_wr_assp_data(sc, ch->adc_data + rv[i].addr, rv[i].val);
	}

	/* put us in the packed task lists */
	m3_wr_assp_data(sc, KDATA_INSTANCE0_MINISRC +
			(sc->pch_cnt + sc->rch_cnt),
			ch->adc_data >> DP_SHIFT_COUNT);
	m3_wr_assp_data(sc, KDATA_DMA_XFER0 + (sc->pch_cnt + sc->rch_cnt),
			ch->adc_data >> DP_SHIFT_COUNT);
	m3_wr_assp_data(sc, KDATA_ADC1_XFER0 + sc->rch_cnt,
			ch->adc_data >> DP_SHIFT_COUNT);

	m3_rchan_trigger(NULL, ch, PCMTRIG_START); /* gotta start before stop */
	m3_rchan_trigger(NULL, ch, PCMTRIG_STOP); /* stop on init */

	sc->rch_cnt++;
	return ch;
}

static int
m3_rchan_free(kobj_t kobj, void *chdata)
{
	struct sc_rchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;

        M3_DEBUG(CHANGE, ("m3_rchan_free(adc=%d)\n", ch->adc_idx));

	/*
	 * should remove this exact instance from the packed lists, but all
	 * are released at once (and in a stopped state) so this is ok.
	 */
	m3_wr_assp_data(sc, KDATA_INSTANCE0_MINISRC +
			(sc->rch_cnt - 1) + sc->pch_cnt, 0);
	m3_wr_assp_data(sc, KDATA_DMA_XFER0 +
			(sc->rch_cnt - 1) + sc->pch_cnt, 0);
	m3_wr_assp_data(sc, KDATA_ADC1_XFER0 + (sc->rch_cnt - 1), 0);

	sc->rch_cnt--;
	return 0;
}

static int
m3_rchan_setformat(kobj_t kobj, void *chdata, u_int32_t format)
{
	struct sc_rchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t data;

	M3_DEBUG(CHANGE,
		 ("m3_rchan_setformat(dac=%d, format=0x%x{%s-%s})\n",
		  ch->adc_idx, format,
		  format & (AFMT_U8|AFMT_S8) ? "8bit":"16bit",
		  format & AFMT_STEREO ? "STEREO":"MONO"));

	/* mono word */
        data = (format & AFMT_STEREO) ? 0 : 1;
        m3_wr_assp_data(sc, ch->adc_data + SRC3_MODE_OFFSET, data);

        /* 8bit word */
        data = ((format & AFMT_U8) || (format & AFMT_S8)) ? 1 : 0;
        m3_wr_assp_data(sc, ch->adc_data + SRC3_WORD_LENGTH_OFFSET, data);

        ch->fmt = format;
        return 0;
}

static int
m3_rchan_setspeed(kobj_t kobj, void *chdata, u_int32_t speed)
{
	struct sc_rchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t freq;

	M3_DEBUG(CHANGE, ("m3_rchan_setspeed(adc=%d, speed=%d)\n",
			  ch->adc_idx, speed));

        if ((freq = ((speed << 15) + 24000) / 48000) != 0) {
                freq--;
        }

        m3_wr_assp_data(sc, ch->adc_data + CDATA_FREQUENCY, freq);

	ch->spd = speed;
	return speed; /* return closest possible speed */
}

static int
m3_rchan_setblocksize(kobj_t kobj, void *chdata, u_int32_t blocksize)
{
	struct sc_rchinfo *ch = chdata;

	M3_DEBUG(CHANGE, ("m3_rchan_setblocksize(adc=%d, blocksize=%d)\n",
			  ch->adc_idx, blocksize));

	return blocksize;
}

static int
m3_rchan_trigger(kobj_t kobj, void *chdata, int go)
{
	struct sc_rchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t data;

	M3_DEBUG(go == PCMTRIG_START ? CHANGE :
		 go == PCMTRIG_STOP ? CHANGE :
		 go == PCMTRIG_ABORT ? CHANGE :
		 CALL,
		 ("m3_rchan_trigger(adc=%d, go=0x%x{%s})\n", ch->adc_idx, go,
		  go == PCMTRIG_START ? "PCMTRIG_START" :
		  go == PCMTRIG_STOP ? "PCMTRIG_STOP" :
		  go == PCMTRIG_ABORT ? "PCMTRIG_ABORT" : "ignore"));

	switch(go) {
	case PCMTRIG_START:
		if (ch->active) {
			return 0;
		}
		ch->active = 1;

		/*[[inc_timer_users]]*/
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_RELOAD, 240);
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_CURRENT, 240);
                data = m3_rd_2(sc, HOST_INT_CTRL);
                m3_wr_2(sc, HOST_INT_CTRL, data | CLKRUN_GEN_ENABLE);

                m3_wr_assp_data(sc, KDATA_ADC1_REQUEST, 1);
                m3_wr_assp_data(sc, ch->adc_data + CDATA_INSTANCE_READY, 1);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		if (ch->active == 0) {
			return 0;
		}
		ch->active = 0;

		/*[[dec_timer_users]]*/
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_RELOAD, 0);
                m3_wr_assp_data(sc, KDATA_TIMER_COUNT_CURRENT, 0);
                data = m3_rd_2(sc, HOST_INT_CTRL);
                m3_wr_2(sc, HOST_INT_CTRL, data & ~CLKRUN_GEN_ENABLE);

                m3_wr_assp_data(sc, ch->adc_data + CDATA_INSTANCE_READY, 0);
                m3_wr_assp_data(sc, KDATA_ADC1_REQUEST, 0);
		break;

	case PCMTRIG_EMLDMAWR:
		/* got play irq, transfer next buffer - ignore if using dma */
	case PCMTRIG_EMLDMARD:
		/* got rec irq, transfer next buffer - ignore if using dma */
	default:
		break;
	}
	return 0;
}

static int
m3_rchan_getptr(kobj_t kobj, void *chdata)
{
	struct sc_rchinfo *ch = chdata;
	struct sc_info *sc = ch->parent;
	u_int32_t hi, lo, bus_crnt;
	u_int32_t bus_base = sndbuf_getbufaddr(ch->buffer);

	hi = m3_rd_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_CURRENTH);
        lo = m3_rd_assp_data(sc, ch->adc_data + CDATA_HOST_SRC_CURRENTL);
        bus_crnt = lo | (hi << 16);

	M3_DEBUG(CALL, ("m3_rchan_getptr(adc=%d) result=%d\n",
			ch->adc_idx, bus_crnt - bus_base));

	return (bus_crnt - bus_base); /* current byte offset of channel */
}

static struct pcmchan_caps *
m3_rchan_getcaps(kobj_t kobj, void *chdata)
{
	struct sc_rchinfo *ch = chdata;

        M3_DEBUG(CALL, ("m3_rchan_getcaps(adc=%d)\n", ch->adc_idx));

	return &m3_reccaps;
}

/* -------------------------------------------------------------------- */
/* The interrupt handler */

static void
m3_intr(void *p)
{
	struct sc_info *sc = (struct sc_info *)p;
	u_int32_t status, ctl, i;

	M3_DEBUG(INTR, ("m3_intr\n"));

	status = m3_rd_1(sc, HOST_INT_STATUS);
	if (!status)
		return;

	m3_wr_1(sc, HOST_INT_STATUS, 0xff); /* ack the int? */

	if (status & HV_INT_PENDING) {
		u_int8_t event;

		event = m3_rd_1(sc, HW_VOL_COUNTER_MASTER);
		switch (event) {
		case 0x99:
			mixer_hwvol_mute(sc->dev);
			break;
		case 0xaa:
			mixer_hwvol_step(sc->dev, 1, 1);
			break;
		case 0x66:
			mixer_hwvol_step(sc->dev, -1, -1);
			break;
		case 0x88:
			break;
		default:
			device_printf(sc->dev, "Unknown HWVOL event\n");
		}
		m3_wr_1(sc, HW_VOL_COUNTER_MASTER, 0x88);

	}

	if (status & ASSP_INT_PENDING) {
		ctl = m3_rd_1(sc, ASSP_CONTROL_B);
		if (!(ctl & STOP_ASSP_CLOCK)) {
			ctl = m3_rd_1(sc, ASSP_HOST_INT_STATUS);
			if (ctl & DSP2HOST_REQ_TIMER) {
				m3_wr_1(sc, ASSP_HOST_INT_STATUS,
					DSP2HOST_REQ_TIMER);
				/*[[ess_update_ptr]]*/
			}
		}
	}

	for (i=0 ; i<sc->pch_cnt ; i++) {
		if (sc->pch[i].active) {
			chn_intr(sc->pch[i].channel);
		}
	}
	for (i=0 ; i<sc->rch_cnt ; i++) {
		if (sc->rch[i].active) {
			chn_intr(sc->rch[i].channel);
		}
	}
}

/* -------------------------------------------------------------------- */
/* stuff */

static int
m3_power(struct sc_info *sc, int state)
{
	u_int32_t data;

	M3_DEBUG(CHANGE, ("m3_power(%d)\n", state));

	data = pci_read_config(sc->dev, 0x34, 1);
	if (pci_read_config(sc->dev, data, 1) == 1) {
		pci_write_config(sc->dev, data + 4, state, 1);
	}

	return 0;
}

static int
m3_init(struct sc_info *sc)
{
	u_int32_t data, i, size;
	u_int8_t reset_state;

        M3_DEBUG(CHANGE, ("m3_init\n"));

	/* diable legacy emulations. */
	data = pci_read_config(sc->dev, PCI_LEGACY_AUDIO_CTRL, 2);
	data |= DISABLE_LEGACY;
	pci_write_config(sc->dev, PCI_LEGACY_AUDIO_CTRL, data, 2);

	m3_config(sc);

	reset_state = m3_assp_halt(sc);

	m3_codec_reset(sc);

	/* [m3_assp_init] */
	/* zero kernel data */
	size = REV_B_DATA_MEMORY_UNIT_LENGTH * NUM_UNITS_KERNEL_DATA;
	for(i = 0 ; i < size / 2 ; i++) {
		m3_wr_assp_data(sc, KDATA_BASE_ADDR + i, 0);
	}
	/* zero mixer data? */
	size = REV_B_DATA_MEMORY_UNIT_LENGTH * NUM_UNITS_KERNEL_DATA;
	for(i = 0 ; i < size / 2 ; i++) {
		m3_wr_assp_data(sc, KDATA_BASE_ADDR2 + i, 0);
	}
	/* init dma pointer */
	m3_wr_assp_data(sc, KDATA_CURRENT_DMA,
			KDATA_DMA_XFER0);
	/* write kernel into code memory */
	size = sizeof(assp_kernel_image);
	for(i = 0 ; i < size / 2; i++) {
		m3_wr_assp_code(sc, REV_B_CODE_MEMORY_BEGIN + i,
				assp_kernel_image[i]);
	}
	/*
	 * We only have this one client and we know that 0x400 is free in
	 * our kernel's mem map, so lets just drop it there.  It seems that
	 * the minisrc doesn't need vectors, so we won't bother with them..
	 */
	size = sizeof(assp_minisrc_image);
	for(i = 0 ; i < size / 2; i++) {
		m3_wr_assp_code(sc, 0x400 + i, assp_minisrc_image[i]);
	}
	/* write the coefficients for the low pass filter? */
	size = sizeof(minisrc_lpf_image);
	for(i = 0; i < size / 2 ; i++) {
		m3_wr_assp_code(sc,0x400 + MINISRC_COEF_LOC + i,
				minisrc_lpf_image[i]);
	}
	m3_wr_assp_code(sc, 0x400 + MINISRC_COEF_LOC + size, 0x8000);
	/* the minisrc is the only thing on our task list */
	m3_wr_assp_data(sc, KDATA_TASK0, 0x400);
	/* init the mixer number */
	m3_wr_assp_data(sc, KDATA_MIXER_TASK_NUMBER, 0);
	/* extreme kernel master volume */
	m3_wr_assp_data(sc, KDATA_DAC_LEFT_VOLUME, ARB_VOLUME);
	m3_wr_assp_data(sc, KDATA_DAC_RIGHT_VOLUME, ARB_VOLUME);

	m3_amp_enable(sc);

	/* [m3_assp_client_init] (only one client at index 0) */
	for (i=0x1100 ; i<0x1c00 ; i++) {
		m3_wr_assp_data(sc, i, 0); /* zero entire dac/adc area */
	}

	/* [m3_assp_continue] */
	m3_wr_1(sc, DSP_PORT_CONTROL_REG_B, reset_state | REGB_ENABLE_RESET);

	return 0;
}

static int
m3_uninit(struct sc_info *sc)
{
        M3_DEBUG(CHANGE, ("m3_uninit\n"));
	return 0;
}

/* -------------------------------------------------------------------- */
/* Probe and attach the card */

static int
m3_pci_probe(device_t dev)
{
	struct m3_card_type *card;

	M3_DEBUG(CALL, ("m3_pci_probe(0x%x)\n", pci_get_devid(dev)));

	for (card = m3_card_types ; card->pci_id ; card++) {
		if (pci_get_devid(dev) == card->pci_id) {
			device_set_desc(dev, card->name);
			return 0;
		}
	}
	return ENXIO;
}

static int
m3_pci_attach(device_t dev)
{
	struct sc_info *sc;
	struct ac97_info *codec = NULL;
	u_int32_t data, i;
	char status[SND_STATUSLEN];
	struct m3_card_type *card;
	int len;

	M3_DEBUG(CALL, ("m3_pci_attach\n"));

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	sc->dev = dev;
	sc->type = pci_get_devid(dev);

	for (card = m3_card_types ; card->pci_id ; card++) {
		if (sc->type == card->pci_id) {
			sc->which = card->which;
			sc->delay1 = card->delay1;
			sc->delay2 = card->delay2;
			break;
		}
	}

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);

	sc->regid = PCIR_MAPS;
	sc->regtype = SYS_RES_MEMORY;
	sc->reg = bus_alloc_resource(dev, sc->regtype, &sc->regid,
				     0, ~0, 1, RF_ACTIVE);
	if (!sc->reg) {
		sc->regtype = SYS_RES_IOPORT;
		sc->reg = bus_alloc_resource(dev, sc->regtype, &sc->regid,
					     0, ~0, 1, RF_ACTIVE);
	}
	if (!sc->reg) {
		device_printf(dev, "unable to allocate register space\n");
		goto bad;
	}
	sc->st = rman_get_bustag(sc->reg);
	sc->sh = rman_get_bushandle(sc->reg);

	sc->irqid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irqid,
				     0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->irq) {
		device_printf(dev, "unable to allocate interrupt\n");
		goto bad;
	}

	if (snd_setup_intr(dev, sc->irq, 0, m3_intr, sc, &sc->ih)) {
		device_printf(dev, "unable to setup interrupt\n");
		goto bad;
	}

	sc->bufsz = pcm_getbuffersize(dev, 1024, M3_BUFSIZE_DEFAULT, 65536);

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
			       /*lowaddr*/M3_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/sc->bufsz, /*nsegments*/1,
			       /*maxsegz*/0x3ffff,
			       /*flags*/0, /*lockfunc*/busdma_lock_mutex,
			       /*lockarg*/&Giant, &sc->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	m3_power(sc, 0); /* power up */

	/* init chip */
	if (m3_init(sc) == -1) {
		device_printf(dev, "unable to initialize the card\n");
		goto bad;
	}

	/* create/init mixer */
	codec = AC97_CREATE(dev, sc, m3_codec);
	if (codec == NULL) {
		device_printf(dev, "ac97_create error\n");
		goto bad;
	}
	if (mixer_init(dev, ac97_getmixerclass(), codec)) {
		device_printf(dev, "mixer_init error\n");
		goto bad;
	}

	m3_enable_ints(sc);

	if (pcm_register(dev, sc, M3_PCHANS, M3_RCHANS)) {
		device_printf(dev, "pcm_register error\n");
		goto bad;
	}
	for (i=0 ; i<M3_PCHANS ; i++) {
		if (pcm_addchan(dev, PCMDIR_PLAY, &m3_pch_class, sc)) {
			device_printf(dev, "pcm_addchan (play) error\n");
			goto bad;
		}
	}
	for (i=0 ; i<M3_RCHANS ; i++) {
		if (pcm_addchan(dev, PCMDIR_REC, &m3_rch_class, sc)) {
			device_printf(dev, "pcm_addchan (rec) error\n");
			goto bad;
		}
	}
 	snprintf(status, SND_STATUSLEN, "at %s 0x%lx irq %ld",
		 (sc->regtype == SYS_RES_IOPORT)? "io" : "memory",
		 rman_get_start(sc->reg), rman_get_start(sc->irq));
	if (pcm_setstatus(dev, status)) {
		device_printf(dev, "attach: pcm_setstatus error\n");
		goto bad;
	}

	mixer_hwvol_init(dev);

	/* Create the buffer for saving the card state during suspend */
	len = sizeof(u_int16_t) * (REV_B_CODE_MEMORY_LENGTH +
	    REV_B_DATA_MEMORY_LENGTH);
	sc->savemem = (u_int16_t*)malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->savemem == NULL) {
		device_printf(dev, "Failed to create suspend buffer\n");
		goto bad;
	}

	return 0;

 bad:
	if (codec) {
		ac97_destroy(codec);
	}
	if (sc->reg) {
		bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	}
	if (sc->ih) {
		bus_teardown_intr(dev, sc->irq, sc->ih);
	}
	if (sc->irq) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	}
	if (sc->parent_dmat) {
		bus_dma_tag_destroy(sc->parent_dmat);
	}
	free(sc, M_DEVBUF);
	return ENXIO;
}

static int
m3_pci_detach(device_t dev)
{
	struct sc_info *sc = pcm_getdevinfo(dev);
	int r;

	M3_DEBUG(CALL, ("m3_pci_detach\n"));

	if ((r = pcm_unregister(dev)) != 0) {
		return r;
	}
	m3_uninit(sc); /* shutdown chip */
	m3_power(sc, 3); /* power off */

	bus_release_resource(dev, sc->regtype, sc->regid, sc->reg);
	bus_teardown_intr(dev, sc->irq, sc->ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->irqid, sc->irq);
	bus_dma_tag_destroy(sc->parent_dmat);

	free(sc->savemem, M_DEVBUF);
	free(sc, M_DEVBUF);
	return 0;
}

static int
m3_pci_suspend(device_t dev)
{
	struct sc_info *sc = pcm_getdevinfo(dev);
	int i, index = 0;

        M3_DEBUG(CHANGE, ("m3_pci_suspend\n"));

	for (i=0 ; i<sc->pch_cnt ; i++) {
		if (sc->pch[i].active) {
			m3_pchan_trigger(NULL, &sc->pch[i], PCMTRIG_STOP);
		}
	}
	for (i=0 ; i<sc->rch_cnt ; i++) {
		if (sc->rch[i].active) {
			m3_rchan_trigger(NULL, &sc->rch[i], PCMTRIG_STOP);
		}
	}
	DELAY(10 * 1000); /* give things a chance to stop */

	/* Disable interrupts */
	m3_wr_2(sc, HOST_INT_CTRL, 0);
	m3_wr_1(sc, ASSP_CONTROL_C, 0);

	m3_assp_halt(sc);

	/* Save the state of the ASSP */
	for (i = REV_B_CODE_MEMORY_BEGIN; i <= REV_B_CODE_MEMORY_END; i++)
		sc->savemem[index++] = m3_rd_assp_code(sc, i);
	for (i = REV_B_DATA_MEMORY_BEGIN; i <= REV_B_DATA_MEMORY_END; i++)
		sc->savemem[index++] = m3_rd_assp_data(sc, i);

	/* Power down the card to D3 state */
	m3_power(sc, 3);

	return 0;
}

static int
m3_pci_resume(device_t dev)
{
	struct sc_info *sc = pcm_getdevinfo(dev);
	int i, index = 0;
	u_int8_t reset_state;

	M3_DEBUG(CHANGE, ("m3_pci_resume\n"));

	/* Power the card back to D0 */
	m3_power(sc, 0);

	m3_config(sc);

	reset_state = m3_assp_halt(sc);

	m3_codec_reset(sc);

	/* Restore the ASSP state */
	for (i = REV_B_CODE_MEMORY_BEGIN; i <= REV_B_CODE_MEMORY_END; i++)
		m3_wr_assp_code(sc, i, sc->savemem[index++]);
	for (i = REV_B_DATA_MEMORY_BEGIN; i <= REV_B_DATA_MEMORY_END; i++)
		m3_wr_assp_data(sc, i, sc->savemem[index++]);

	/* Restart the DMA engine */
	m3_wr_assp_data(sc, KDATA_DMA_ACTIVE, 0);

	/* [m3_assp_continue] */
	m3_wr_1(sc, DSP_PORT_CONTROL_REG_B, reset_state | REGB_ENABLE_RESET);

	m3_amp_enable(sc);

	m3_enable_ints(sc);

	if (mixer_reinit(dev) == -1) {
		device_printf(dev, "unable to reinitialize the mixer\n");
		return ENXIO;
	}

	/* Turn the channels back on */
	for (i=0 ; i<sc->pch_cnt ; i++) {
		if (sc->pch[i].active) {
			m3_pchan_trigger(NULL, &sc->pch[i], PCMTRIG_START);
		}
	}
	for (i=0 ; i<sc->rch_cnt ; i++) {
		if (sc->rch[i].active) {
			m3_rchan_trigger(NULL, &sc->rch[i], PCMTRIG_START);
		}
	}

	return 0;
}

static int
m3_pci_shutdown(device_t dev)
{
	struct sc_info *sc = pcm_getdevinfo(dev);

	M3_DEBUG(CALL, ("m3_pci_shutdown\n"));

	m3_power(sc, 3); /* power off */
	return 0;
}

static u_int8_t
m3_assp_halt(struct sc_info *sc)
{
	u_int8_t data, reset_state;

	data = m3_rd_1(sc, DSP_PORT_CONTROL_REG_B);
	reset_state = data & ~REGB_STOP_CLOCK; /* remember for continue */
        DELAY(10 * 1000);
	m3_wr_1(sc, DSP_PORT_CONTROL_REG_B, reset_state & ~REGB_ENABLE_RESET);
        DELAY(10 * 1000); /* necessary? */

	return reset_state;
}

static void
m3_config(struct sc_info *sc)
{
	u_int32_t data, hv_cfg;
	int hint;

	/*
	 * The volume buttons can be wired up via two different sets of pins.
	 * This presents a problem since we can't tell which way it's
	 * configured.  Allow the user to set a hint in order to twiddle
	 * the proper bits.
	 */
	if (resource_int_value(device_get_name(sc->dev),
	                       device_get_unit(sc->dev),
			       "hwvol_config", &hint) == 0)
		hv_cfg = (hint > 0) ? HV_BUTTON_FROM_GD : 0;
	else
		hv_cfg = HV_BUTTON_FROM_GD;

	data = pci_read_config(sc->dev, PCI_ALLEGRO_CONFIG, 4);
	data &= ~HV_BUTTON_FROM_GD;
	data |= REDUCED_DEBOUNCE | HV_CTRL_ENABLE | hv_cfg;
	data |= PM_CTRL_ENABLE | CLK_DIV_BY_49 | USE_PCI_TIMING;
	pci_write_config(sc->dev, PCI_ALLEGRO_CONFIG, data, 4);

	m3_wr_1(sc, ASSP_CONTROL_B, RESET_ASSP);
	data = pci_read_config(sc->dev, PCI_ALLEGRO_CONFIG, 4);
	data &= ~INT_CLK_SELECT;
	if (sc->which == ESS_MAESTRO3) {
		data &= ~INT_CLK_MULT_ENABLE;
		data |= INT_CLK_SRC_NOT_PCI;
	}
	data &= ~(CLK_MULT_MODE_SELECT | CLK_MULT_MODE_SELECT_2);
	pci_write_config(sc->dev, PCI_ALLEGRO_CONFIG, data, 4);

	if (sc->which == ESS_ALLEGRO_1) {
		data = pci_read_config(sc->dev, PCI_USER_CONFIG, 4);
		data |= IN_CLK_12MHZ_SELECT;
		pci_write_config(sc->dev, PCI_USER_CONFIG, data, 4);
	}

	data = m3_rd_1(sc, ASSP_CONTROL_A);
	data &= ~(DSP_CLK_36MHZ_SELECT | ASSP_CLK_49MHZ_SELECT);
	data |= ASSP_CLK_49MHZ_SELECT; /*XXX assumes 49MHZ dsp XXX*/
	data |= ASSP_0_WS_ENABLE;
	m3_wr_1(sc, ASSP_CONTROL_A, data);

	m3_wr_1(sc, ASSP_CONTROL_B, RUN_ASSP);
}

static void
m3_enable_ints(struct sc_info *sc)
{
	u_int8_t data;

	m3_wr_2(sc, HOST_INT_CTRL, ASSP_INT_ENABLE | HV_INT_ENABLE);
	data = m3_rd_1(sc, ASSP_CONTROL_C);
	m3_wr_1(sc, ASSP_CONTROL_C, data | ASSP_HOST_INT_ENABLE);
}

static void
m3_amp_enable(struct sc_info *sc)
{
	u_int32_t gpo, polarity_port, polarity;
	u_int16_t data;

	switch (sc->which) {
        case ESS_ALLEGRO_1:
                polarity_port = 0x1800;
                break;
	case ESS_MAESTRO3:
                polarity_port = 0x1100;
                break;
        default:
		panic("bad sc->which");
	}
	gpo = (polarity_port >> 8) & 0x0f;
	polarity = polarity_port >> 12;
	polarity = !polarity; /* enable */
	polarity = polarity << gpo;
	gpo = 1 << gpo;
	m3_wr_2(sc, GPIO_MASK, ~gpo);
	data = m3_rd_2(sc, GPIO_DIRECTION);
	m3_wr_2(sc, GPIO_DIRECTION, data | gpo);
	data = GPO_SECONDARY_AC97 | GPO_PRIMARY_AC97 | polarity;
	m3_wr_2(sc, GPIO_DATA, data);
	m3_wr_2(sc, GPIO_MASK, ~0);
}

static void
m3_codec_reset(struct sc_info *sc)
{
	u_int16_t data, dir;
	int retry = 0;

	do {
		data = m3_rd_2(sc, GPIO_DIRECTION);
		dir = data | 0x10; /* assuming pci bus master? */

		/* [[remote_codec_config]] */
		data = m3_rd_2(sc, RING_BUS_CTRL_B);
		m3_wr_2(sc, RING_BUS_CTRL_B, data & ~SECOND_CODEC_ID_MASK);
		data = m3_rd_2(sc, SDO_OUT_DEST_CTRL);
		m3_wr_2(sc, SDO_OUT_DEST_CTRL, data & ~COMMAND_ADDR_OUT);
		data = m3_rd_2(sc, SDO_IN_DEST_CTRL);
		m3_wr_2(sc, SDO_IN_DEST_CTRL, data & ~STATUS_ADDR_IN);

		m3_wr_2(sc, RING_BUS_CTRL_A, IO_SRAM_ENABLE);
		DELAY(20);

		m3_wr_2(sc, GPIO_DIRECTION, dir & ~GPO_PRIMARY_AC97);
		m3_wr_2(sc, GPIO_MASK, ~GPO_PRIMARY_AC97);
		m3_wr_2(sc, GPIO_DATA, 0);
		m3_wr_2(sc, GPIO_DIRECTION, dir | GPO_PRIMARY_AC97);
		DELAY(sc->delay1 * 1000); /*delay1 (ALLEGRO:50, MAESTRO3:20)*/
		m3_wr_2(sc, GPIO_DATA, GPO_PRIMARY_AC97);
		DELAY(5);
		m3_wr_2(sc, RING_BUS_CTRL_A, IO_SRAM_ENABLE |
		    SERIAL_AC_LINK_ENABLE);
		m3_wr_2(sc, GPIO_MASK, ~0);
		DELAY(sc->delay2 * 1000); /*delay2 (ALLEGRO:800, MAESTRO3:500)*/

		/* [[try read vendor]] */
		data = m3_rdcd(NULL, sc, 0x7c);
		if ((data == 0) || (data == 0xffff)) {
			retry++;
			if (retry > 3) {
				device_printf(sc->dev, "Codec reset failed\n");
				break;
			}
			device_printf(sc->dev, "Codec reset retry\n");
		} else retry = 0;
	} while (retry);
}

static device_method_t m3_methods[] = {
	DEVMETHOD(device_probe,		m3_pci_probe),
	DEVMETHOD(device_attach,	m3_pci_attach),
	DEVMETHOD(device_detach,	m3_pci_detach),
	DEVMETHOD(device_suspend,       m3_pci_suspend),
	DEVMETHOD(device_resume,        m3_pci_resume),
	DEVMETHOD(device_shutdown,      m3_pci_shutdown),
	{ 0, 0 }
};

static driver_t m3_driver = {
	"pcm",
	m3_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_maestro3, pci, m3_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_maestro3, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_maestro3, 1);
