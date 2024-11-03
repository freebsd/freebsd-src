/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012-2021 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023-2024 Florian Walpen <dev@submerge.ch>
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
 * RME HDSP driver for FreeBSD (pcm-part).
 * Supported cards: HDSP 9632, HDSP 9652.
 */

#include <sys/libkern.h>

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pci/hdsp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <mixer_if.h>

#define HDSP_MATRIX_MAX	8

struct hdsp_latency {
	uint32_t n;
	uint32_t period;
	float ms;
};

static struct hdsp_latency latency_map[] = {
	{ 7,   32, 0.7 },
	{ 0,   64, 1.5 },
	{ 1,  128,   3 },
	{ 2,  256,   6 },
	{ 3,  512,  12 },
	{ 4, 1024,  23 },
	{ 5, 2048,  46 },
	{ 6, 4096,  93 },

	{ 0,    0,   0 },
};

struct hdsp_rate {
	uint32_t speed;
	uint32_t reg;
};

static struct hdsp_rate rate_map[] = {
	{  32000, (HDSP_FREQ_32000) },
	{  44100, (HDSP_FREQ_44100) },
	{  48000, (HDSP_FREQ_48000) },
	{  64000, (HDSP_FREQ_32000 | HDSP_FREQ_DOUBLE) },
	{  88200, (HDSP_FREQ_44100 | HDSP_FREQ_DOUBLE) },
	{  96000, (HDSP_FREQ_48000 | HDSP_FREQ_DOUBLE) },
	{ 128000, (HDSP_FREQ_32000 | HDSP_FREQ_QUAD)   },
	{ 176400, (HDSP_FREQ_44100 | HDSP_FREQ_QUAD)   },
	{ 192000, (HDSP_FREQ_48000 | HDSP_FREQ_QUAD)   },

	{ 0, 0 },
};

static uint32_t
hdsp_adat_slot_map(uint32_t speed)
{
	/* ADAT slot bitmap depends on sample rate. */
	if (speed <= 48000)
		return (0x000000ff); /* 8 channels single speed. */
	else if (speed <= 96000)
		return (0x000000aa); /* 4 channels (1,3,5,7) double speed. */
	else
		return (0x00000000); /* ADAT disabled at quad speed. */
}

static uint32_t
hdsp_port_slot_map(uint32_t ports, uint32_t speed)
{
	uint32_t slot_map = 0;

	if (ports & HDSP_CHAN_9632_ALL) {
		/* Map HDSP 9632 ports to slot bitmap. */
		if (ports & HDSP_CHAN_9632_ADAT)
			slot_map |= (hdsp_adat_slot_map(speed) << 0);
		if (ports & HDSP_CHAN_9632_SPDIF)
			slot_map |= (0x03 << 8);  /* 2 channels SPDIF. */
		if (ports & HDSP_CHAN_9632_LINE)
			slot_map |= (0x03 << 10); /* 2 channels line. */
		if (ports & HDSP_CHAN_9632_EXT)
			slot_map |= (0x0f << 12); /* 4 channels extension. */
	} else if ((ports & HDSP_CHAN_9652_ALL) && (speed <= 96000)) {
		/* Map HDSP 9652 ports to slot bitmap, no quad speed. */
		if (ports & HDSP_CHAN_9652_ADAT1)
			slot_map |= (hdsp_adat_slot_map(speed) << 0);
		if (ports & HDSP_CHAN_9652_ADAT2)
			slot_map |= (hdsp_adat_slot_map(speed) << 8);
		if (ports & HDSP_CHAN_9652_ADAT3)
			slot_map |= (hdsp_adat_slot_map(speed) << 16);
		if (ports & HDSP_CHAN_9652_SPDIF)
			slot_map |= (0x03 << 24); /* 2 channels SPDIF. */
	}

	return (slot_map);
}

static uint32_t
hdsp_slot_first(uint32_t slots)
{
	return (slots & (~(slots - 1)));	/* Extract first bit set. */
}

static uint32_t
hdsp_slot_first_row(uint32_t slots)
{
	uint32_t ends;

	/* Ends of slot rows are followed by a slot which is not in the set. */
	ends = slots & (~(slots >> 1));
	/* First row of contiguous slots ends in the first row end. */
	return (slots & (ends ^ (ends - 1)));
}

static uint32_t
hdsp_slot_first_n(uint32_t slots, unsigned int n)
{
	/* Clear all but the first n slots. */
	for (uint32_t slot = 1; slot != 0; slot <<= 1) {
		if ((slots & slot) && n > 0)
			--n;
		else
			slots &= ~slot;
	}
	return (slots);
}

static unsigned int
hdsp_slot_count(uint32_t slots)
{
	return (bitcount32(slots));
}

static unsigned int
hdsp_slot_offset(uint32_t slots)
{
	return (hdsp_slot_count(hdsp_slot_first(slots) - 1));
}

static unsigned int
hdsp_slot_channel_offset(uint32_t subset, uint32_t slots)
{
	uint32_t preceding;

	/* Make sure we have a subset of slots. */
	subset &= slots;
	/* Include all slots preceding the first one of the subset. */
	preceding = slots & (hdsp_slot_first(subset) - 1);

	return (hdsp_slot_count(preceding));
}

static uint32_t
hdsp_port_first(uint32_t ports)
{
	return (ports & (~(ports - 1)));	/* Extract first bit set. */
}

static unsigned int
hdsp_port_slot_count(uint32_t ports, uint32_t speed)
{
	return (hdsp_slot_count(hdsp_port_slot_map(ports, speed)));
}

static unsigned int
hdsp_port_slot_count_max(uint32_t ports)
{
	return (hdsp_slot_count(hdsp_port_slot_map(ports, 48000)));
}

static uint32_t
hdsp_channel_play_ports(struct hdsp_channel *hc)
{
	return (hc->ports & (HDSP_CHAN_9632_ALL | HDSP_CHAN_9652_ALL));
}

static uint32_t
hdsp_channel_rec_ports(struct hdsp_channel *hc)
{
	return (hc->ports & (HDSP_CHAN_9632_ALL | HDSP_CHAN_9652_ALL));
}

static int
hdsp_hw_mixer(struct sc_chinfo *ch, unsigned int dst,
    unsigned int src, unsigned short data)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t value;
	int offset;

	scp = ch->parent;
	sc = scp->sc;

	offset = 0;
	value = (HDSP_MIN_GAIN << 16) | (uint16_t) data;

	if (ch->dir != PCMDIR_PLAY)
		return (0);

	switch (sc->type) {
	case HDSP_9632:
		/* Mixer is 2 rows of sources (inputs, playback) per output. */
		offset = dst * (2 * HDSP_MIX_SLOTS_9632);
		/* Source index in the second row (playback). */
		offset += HDSP_MIX_SLOTS_9632 + src;
		break;
	case HDSP_9652:
		/* Mixer is 2 rows of sources (inputs, playback) per output. */
		offset = dst * (2 * HDSP_MIX_SLOTS_9652);
		/* Source index in the second row (playback). */
		offset += HDSP_MIX_SLOTS_9652 + src;
		break;
	default:
		return (0);
	}

	/*
	 * We have to write mixer matrix values in pairs, with the second
	 * (odd) value in the upper 16 bits of the 32 bit value.
	 * Make value offset even and shift value accordingly.
	 * Assume the paired value to be silenced, since we only set gain
	 * on the diagonal where src and dst are the same.
	 */
	if (offset % 2) {
		offset -= 1;
		value = (value << 16) | HDSP_MIN_GAIN;
	}

	hdsp_write_4(sc, HDSP_MIXER_BASE + offset * sizeof(uint16_t), value);

	return (0);
};

static int
hdspchan_setgain(struct sc_chinfo *ch)
{
	uint32_t port, ports;
	uint32_t slot, slots;
	unsigned int offset;
	unsigned short volume;

	/* Iterate through all physical ports of the channel. */
	ports = ch->ports;
	port = hdsp_port_first(ports);
	while (port != 0) {
		/*
		 * Get slot map from physical port.
		 * Unlike DMA buffers, the hardware mixer's channel mapping
		 * does not change with double or quad speed sample rates.
		 */
		slots = hdsp_port_slot_map(port, 48000);
		slot = hdsp_slot_first(slots);

		/* Treat first slot as left channel. */
		volume = ch->lvol * HDSP_MAX_GAIN / 100;
		while (slot != 0) {
			offset = hdsp_slot_offset(slot);
			hdsp_hw_mixer(ch, offset, offset, volume);

			slots &= ~slot;
			slot = hdsp_slot_first(slots);

			/* Subsequent slots all get the right channel volume. */
			volume = ch->rvol * HDSP_MAX_GAIN / 100;
		}

		ports &= ~port;
		port = hdsp_port_first(ports);
	}

	return (0);
}

static int
hdspmixer_init(struct snd_mixer *m)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	int mask;

	scp = mix_getdevinfo(m);
	sc = scp->sc;
	if (sc == NULL)
		return (-1);

	mask = SOUND_MASK_PCM;

	if (hdsp_channel_play_ports(scp->hc))
		mask |= SOUND_MASK_VOLUME;

	if (hdsp_channel_rec_ports(scp->hc))
		mask |= SOUND_MASK_RECLEV;

	snd_mtxlock(sc->lock);
	pcm_setflags(scp->dev, pcm_getflags(scp->dev) | SD_F_SOFTPCMVOL);
	mix_setdevs(m, mask);
	snd_mtxunlock(sc->lock);

	return (0);
}

static int
hdspmixer_set(struct snd_mixer *m, unsigned dev,
    unsigned left, unsigned right)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	int i;

	scp = mix_getdevinfo(m);

#if 0
	device_printf(scp->dev, "hdspmixer_set() %d %d\n",
	    left, right);
#endif

	for (i = 0; i < scp->chnum; i++) {
		ch = &scp->chan[i];
		if ((dev == SOUND_MIXER_VOLUME && ch->dir == PCMDIR_PLAY) ||
		    (dev == SOUND_MIXER_RECLEV && ch->dir == PCMDIR_REC)) {
			ch->lvol = left;
			ch->rvol = right;
			if (ch->run)
				hdspchan_setgain(ch);
		}
	}

	return (0);
}

static kobj_method_t hdspmixer_methods[] = {
	KOBJMETHOD(mixer_init,      hdspmixer_init),
	KOBJMETHOD(mixer_set,       hdspmixer_set),
	KOBJMETHOD_END
};
MIXER_DECLARE(hdspmixer);

static void
hdspchan_enable(struct sc_chinfo *ch, int value)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t slot, slots;
	unsigned int offset;
	int reg;

	scp = ch->parent;
	sc = scp->sc;

	if (ch->dir == PCMDIR_PLAY)
		reg = HDSP_OUT_ENABLE_BASE;
	else
		reg = HDSP_IN_ENABLE_BASE;

	ch->run = value;

	/* Iterate through all slots of the channel's physical ports. */
	slots = hdsp_port_slot_map(ch->ports, sc->speed);
	slot = hdsp_slot_first(slots);
	while (slot != 0) {
		/* Set register to enable or disable slot. */
		offset = hdsp_slot_offset(slot);
		hdsp_write_1(sc, reg + (4 * offset), value);

		slots &= ~slot;
		slot = hdsp_slot_first(slots);
	}
}

static int
hdsp_running(struct sc_info *sc)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	device_t *devlist;
	int devcount;
	int i, j;
	int running;

	running = 0;

	devlist = NULL;
	devcount = 0;

	if (device_get_children(sc->dev, &devlist, &devcount) != 0)
		running = 1;	/* On error, avoid channel config changes. */

	for (i = 0; running == 0 && i < devcount; i++) {
		scp = device_get_ivars(devlist[i]);
		for (j = 0; j < scp->chnum; j++) {
			ch = &scp->chan[j];
			if (ch->run) {
				running = 1;
				break;
			}
		}
	}

#if 0
	if (running == 1)
		device_printf(sc->dev, "hdsp is running\n");
#endif

	free(devlist, M_TEMP);

	return (running);
}

static void
hdsp_start_audio(struct sc_info *sc)
{

	sc->ctrl_register |= (HDSP_AUDIO_INT_ENABLE | HDSP_ENABLE);
	hdsp_write_4(sc, HDSP_CONTROL_REG, sc->ctrl_register);
}

static void
hdsp_stop_audio(struct sc_info *sc)
{

	if (hdsp_running(sc) == 1)
		return;

	sc->ctrl_register &= ~(HDSP_AUDIO_INT_ENABLE | HDSP_ENABLE);
	hdsp_write_4(sc, HDSP_CONTROL_REG, sc->ctrl_register);
}

static void
buffer_mux_write(uint32_t *dma, uint32_t *pcm, unsigned int pos,
    unsigned int pos_end, unsigned int width, unsigned int channels)
{
	unsigned int slot;

	for (; pos < pos_end; ++pos) {
		for (slot = 0; slot < width; slot++) {
			dma[slot * HDSP_CHANBUF_SAMPLES + pos] =
			    pcm[pos * channels + slot];
		}
	}
}

static void
buffer_mux_port(uint32_t *dma, uint32_t *pcm, uint32_t subset, uint32_t slots,
    unsigned int pos, unsigned int samples, unsigned int channels)
{
	unsigned int slot_offset, width;
	unsigned int chan_pos;

	/* Translate DMA slot offset to DMA buffer offset. */
	slot_offset = hdsp_slot_offset(subset);
	dma += slot_offset * HDSP_CHANBUF_SAMPLES;

	/* Channel position of the slot subset. */
	chan_pos = hdsp_slot_channel_offset(subset, slots);
	pcm += chan_pos;

	/* Only copy channels supported by both hardware and pcm format. */
	width = hdsp_slot_count(subset);

	/* Let the compiler inline and loop unroll common cases. */
	if (width == 1)
		buffer_mux_write(dma, pcm, pos, pos + samples, 1, channels);
	else if (width == 2)
		buffer_mux_write(dma, pcm, pos, pos + samples, 2, channels);
	else if (width == 4)
		buffer_mux_write(dma, pcm, pos, pos + samples, 4, channels);
	else if (width == 8)
		buffer_mux_write(dma, pcm, pos, pos + samples, 8, channels);
	else
		buffer_mux_write(dma, pcm, pos, pos + samples, width, channels);
}

static void
buffer_demux_read(uint32_t *dma, uint32_t *pcm, unsigned int pos,
    unsigned int pos_end, unsigned int width, unsigned int channels)
{
	unsigned int slot;

	for (; pos < pos_end; ++pos) {
		for (slot = 0; slot < width; slot++) {
			pcm[pos * channels + slot] =
			    dma[slot * HDSP_CHANBUF_SAMPLES + pos];
		}
	}
}

static void
buffer_demux_port(uint32_t *dma, uint32_t *pcm, uint32_t subset, uint32_t slots,
    unsigned int pos, unsigned int samples, unsigned int channels)
{
	unsigned int slot_offset, width;
	unsigned int chan_pos;

	/* Translate DMA slot offset to DMA buffer offset. */
	slot_offset = hdsp_slot_offset(subset);
	dma += slot_offset * HDSP_CHANBUF_SAMPLES;

	/* Channel position of the slot subset. */
	chan_pos = hdsp_slot_channel_offset(subset, slots);
	pcm += chan_pos;

	/* Only copy channels supported by both hardware and pcm format. */
	width = hdsp_slot_count(subset);

	/* Let the compiler inline and loop unroll common cases. */
	if (width == 1)
		buffer_demux_read(dma, pcm, pos, pos + samples, 1, channels);
	else if (width == 2)
		buffer_demux_read(dma, pcm, pos, pos + samples, 2, channels);
	else if (width == 4)
		buffer_demux_read(dma, pcm, pos, pos + samples, 4, channels);
	else if (width == 8)
		buffer_demux_read(dma, pcm, pos, pos + samples, 8, channels);
	else
		buffer_demux_read(dma, pcm, pos, pos + samples, width, channels);
}


/* Copy data between DMA and PCM buffers. */
static void
buffer_copy(struct sc_chinfo *ch)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t row, slots;
	uint32_t dma_pos;
	unsigned int pos, length, remainder, offset, buffer_size;
	unsigned int channels;

	scp = ch->parent;
	sc = scp->sc;

	channels = AFMT_CHANNEL(ch->format); /* Number of PCM channels. */

	/* HDSP cards read / write a double buffer, twice the latency period. */
	buffer_size = 2 * sc->period * sizeof(uint32_t);

	/* Derive buffer position and length to be copied. */
	if (ch->dir == PCMDIR_PLAY) {
		/* Buffer position scaled down to a single channel. */
		pos = sndbuf_getreadyptr(ch->buffer) / channels;
		length = sndbuf_getready(ch->buffer) / channels;
		/* Copy no more than 2 periods in advance. */
		if (length > buffer_size)
			length = buffer_size;
		/* Skip what was already copied last time. */
		offset = (ch->position + buffer_size) - pos;
		offset %= buffer_size;
		if (offset <= length) {
			pos = (pos + offset) % buffer_size;
			length -= offset;
		}
	} else {
		/* Buffer position scaled down to a single channel. */
		pos = sndbuf_getfreeptr(ch->buffer) / channels;
		/* Get DMA buffer write position. */
		dma_pos = hdsp_read_2(sc, HDSP_STATUS_REG);
		dma_pos &= HDSP_BUF_POSITION_MASK;
		dma_pos %= buffer_size;
		/* Copy what is newly available. */
		length = (dma_pos + buffer_size) - pos;
		length %= buffer_size;
	}

	/* Position and length in samples (4 bytes). */
	pos /= 4;
	length /= 4;
	buffer_size /= sizeof(uint32_t);

	/* Split copy length to wrap around at buffer end. */
	remainder = 0;
	if (pos + length > buffer_size)
		remainder = (pos + length) - buffer_size;

	/* Iterate through rows of contiguous slots. */
	slots = hdsp_port_slot_map(ch->ports, sc->speed);
	slots = hdsp_slot_first_n(slots, channels);
	row = hdsp_slot_first_row(slots);

	while (row != 0) {
		if (ch->dir == PCMDIR_PLAY) {
			buffer_mux_port(sc->pbuf, ch->data, row, slots, pos,
			    length - remainder, channels);
			buffer_mux_port(sc->pbuf, ch->data, row, slots, 0,
			    remainder, channels);
		} else {
			buffer_demux_port(sc->rbuf, ch->data, row, slots, pos,
			    length - remainder, channels);
			buffer_demux_port(sc->rbuf, ch->data, row, slots, 0,
			    remainder, channels);
		}

		slots &= ~row;
		row = hdsp_slot_first_row(slots);
	}

	ch->position = ((pos + length) * 4) % buffer_size;
}

static int
clean(struct sc_chinfo *ch)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t *buf;
	uint32_t slot, slots;
	unsigned int offset;

	scp = ch->parent;
	sc = scp->sc;
	buf = sc->rbuf;

	if (ch->dir == PCMDIR_PLAY)
		buf = sc->pbuf;

	/* Iterate through all of the channel's slots. */
	slots = hdsp_port_slot_map(ch->ports, sc->speed);
	slot = hdsp_slot_first(slots);
	while (slot != 0) {
		/* Clear the slot's buffer. */
		offset = hdsp_slot_offset(slot);
		bzero(buf + offset * HDSP_CHANBUF_SAMPLES, HDSP_CHANBUF_SIZE);

		slots &= ~slot;
		slot = hdsp_slot_first(slots);
	}

	ch->position = 0;

	return (0);
}

/* Channel interface. */
static int
hdspchan_free(kobj_t obj, void *data)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	struct sc_info *sc;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;

#if 0
	device_printf(scp->dev, "hdspchan_free()\n");
#endif

	snd_mtxlock(sc->lock);
	if (ch->data != NULL) {
		free(ch->data, M_HDSP);
		ch->data = NULL;
	}
	if (ch->caps != NULL) {
		free(ch->caps, M_HDSP);
		ch->caps = NULL;
	}
	snd_mtxunlock(sc->lock);

	return (0);
}

static void *
hdspchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
    struct pcm_channel *c, int dir)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	struct sc_info *sc;
	int num;

	scp = devinfo;
	sc = scp->sc;

	snd_mtxlock(sc->lock);
	num = scp->chnum;

	ch = &scp->chan[num];

	if (dir == PCMDIR_PLAY)
		ch->ports = hdsp_channel_play_ports(scp->hc);
	else
		ch->ports = hdsp_channel_rec_ports(scp->hc);

	ch->run = 0;
	ch->lvol = 0;
	ch->rvol = 0;

	/* Support all possible ADAT widths as channel formats. */
	ch->cap_fmts[0] =
	    SND_FORMAT(AFMT_S32_LE, hdsp_port_slot_count(ch->ports, 48000), 0);
	ch->cap_fmts[1] =
	    SND_FORMAT(AFMT_S32_LE, hdsp_port_slot_count(ch->ports, 96000), 0);
	ch->cap_fmts[2] =
	    SND_FORMAT(AFMT_S32_LE, hdsp_port_slot_count(ch->ports, 192000), 0);
	ch->cap_fmts[3] = 0;

	ch->caps = malloc(sizeof(struct pcmchan_caps), M_HDSP, M_NOWAIT);
	*(ch->caps) = (struct pcmchan_caps) {32000, 192000, ch->cap_fmts, 0};

	/* HDSP 9652 does not support quad speed sample rates. */
	if (sc->type == HDSP_9652) {
		ch->cap_fmts[2] = SND_FORMAT(AFMT_S32_LE, 2, 0);
		ch->caps->maxspeed = 96000;
	}

	/* Allocate maximum buffer size. */
	ch->size = HDSP_CHANBUF_SIZE * hdsp_port_slot_count_max(ch->ports);
	ch->data = malloc(ch->size, M_HDSP, M_NOWAIT);
	ch->position = 0;

	ch->buffer = b;
	ch->channel = c;
	ch->parent = scp;

	ch->dir = dir;

	snd_mtxunlock(sc->lock);

	if (sndbuf_setup(ch->buffer, ch->data, ch->size) != 0) {
		device_printf(scp->dev, "Can't setup sndbuf.\n");
		hdspchan_free(obj, ch);
		return (NULL);
	}

	return (ch);
}

static int
hdspchan_trigger(kobj_t obj, void *data, int go)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	struct sc_info *sc;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;

	snd_mtxlock(sc->lock);
	switch (go) {
	case PCMTRIG_START:
#if 0
		device_printf(scp->dev, "hdspchan_trigger(): start\n");
#endif
		hdspchan_enable(ch, 1);
		hdspchan_setgain(ch);
		hdsp_start_audio(sc);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
#if 0
		device_printf(scp->dev, "hdspchan_trigger(): stop or abort\n");
#endif
		clean(ch);
		hdspchan_enable(ch, 0);
		hdsp_stop_audio(sc);
		break;

	case PCMTRIG_EMLDMAWR:
	case PCMTRIG_EMLDMARD:
		if(ch->run)
			buffer_copy(ch);
		break;
	}

	snd_mtxunlock(sc->lock);

	return (0);
}

static uint32_t
hdspchan_getptr(kobj_t obj, void *data)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	struct sc_info *sc;
	uint32_t ret, pos;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;

	snd_mtxlock(sc->lock);
	ret = hdsp_read_2(sc, HDSP_STATUS_REG);
	snd_mtxunlock(sc->lock);

	pos = ret & HDSP_BUF_POSITION_MASK;
	pos %= (2 * sc->period * sizeof(uint32_t)); /* Double buffer. */
	pos *= AFMT_CHANNEL(ch->format); /* Hardbuf with multiple channels. */

	return (pos);
}

static int
hdspchan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct sc_chinfo *ch;

	ch = data;

#if 0
	struct sc_pcminfo *scp = ch->parent;
	device_printf(scp->dev, "hdspchan_setformat(%d)\n", format);
#endif

	ch->format = format;

	return (0);
}

static uint32_t
hdspchan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct sc_pcminfo *scp;
	struct hdsp_rate *hr;
	struct sc_chinfo *ch;
	struct sc_info *sc;
	int threshold;
	int i;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;
	hr = NULL;

#if 0
	device_printf(scp->dev, "hdspchan_setspeed(%d)\n", speed);
#endif

	if (hdsp_running(sc) == 1)
		goto end;

	/* HDSP 9652 only supports sample rates up to 96kHz. */
	if (sc->type == HDSP_9652 && speed > 96000)
		speed = 96000;

	if (sc->force_speed > 0)
		speed = sc->force_speed;

	/* First look for equal frequency. */
	for (i = 0; rate_map[i].speed != 0; i++) {
		if (rate_map[i].speed == speed)
			hr = &rate_map[i];
	}

	/* If no match, just find nearest. */
	if (hr == NULL) {
		for (i = 0; rate_map[i].speed != 0; i++) {
			hr = &rate_map[i];
			threshold = hr->speed + ((rate_map[i + 1].speed != 0) ?
			    ((rate_map[i + 1].speed - hr->speed) >> 1) : 0);
			if (speed < threshold)
				break;
		}
	}

	/* Write frequency on the device. */
	sc->ctrl_register &= ~HDSP_FREQ_MASK;
	sc->ctrl_register |= hr->reg;
	hdsp_write_4(sc, HDSP_CONTROL_REG, sc->ctrl_register);

	if (sc->type == HDSP_9632) {
		/* Set DDS value. */
		hdsp_write_4(sc, HDSP_FREQ_REG, hdsp_freq_reg_value(hr->speed));
	}

	sc->speed = hr->speed;
end:

	return (sc->speed);
}

static uint32_t
hdspchan_setblocksize(kobj_t obj, void *data, uint32_t blocksize)
{
	struct hdsp_latency *hl;
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	struct sc_info *sc;
	int threshold;
	int i;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;
	hl = NULL;

#if 0
	device_printf(scp->dev, "hdspchan_setblocksize(%d)\n", blocksize);
#endif

	if (hdsp_running(sc) == 1)
		goto end;

	if (blocksize > HDSP_LAT_BYTES_MAX)
		blocksize = HDSP_LAT_BYTES_MAX;
	else if (blocksize < HDSP_LAT_BYTES_MIN)
		blocksize = HDSP_LAT_BYTES_MIN;

	blocksize /= 4 /* samples */;

	if (sc->force_period > 0)
		blocksize = sc->force_period;

	/* First look for equal latency. */
	for (i = 0; latency_map[i].period != 0; i++) {
		if (latency_map[i].period == blocksize)
			hl = &latency_map[i];
	}

	/* If no match, just find nearest. */
	if (hl == NULL) {
		for (i = 0; latency_map[i].period != 0; i++) {
			hl = &latency_map[i];
			threshold = hl->period + ((latency_map[i + 1].period != 0) ?
			    ((latency_map[i + 1].period - hl->period) >> 1) : 0);
			if (blocksize < threshold)
				break;
		}
	}

	snd_mtxlock(sc->lock);
	sc->ctrl_register &= ~HDSP_LAT_MASK;
	sc->ctrl_register |= hdsp_encode_latency(hl->n);
	hdsp_write_4(sc, HDSP_CONTROL_REG, sc->ctrl_register);
	sc->period = hl->period;
	snd_mtxunlock(sc->lock);

#if 0
	device_printf(scp->dev, "New period=%d\n", sc->period);
#endif

	sndbuf_resize(ch->buffer, 2,
	    (sc->period * AFMT_CHANNEL(ch->format) * sizeof(uint32_t)));

	/* Reset pointer, rewrite frequency (same register) for 9632. */
	hdsp_write_4(sc, HDSP_RESET_POINTER, 0);
	if (sc->type == HDSP_9632)
		hdsp_write_4(sc, HDSP_FREQ_REG, hdsp_freq_reg_value(sc->speed));
end:

	return (sndbuf_getblksz(ch->buffer));
}

static uint32_t hdsp_bkp_fmt[] = {
	SND_FORMAT(AFMT_S32_LE, 2, 0),
	0
};

/* Capabilities fallback, no quad speed for HDSP 9652 compatibility. */
static struct pcmchan_caps hdsp_bkp_caps = {32000, 96000, hdsp_bkp_fmt, 0};

static struct pcmchan_caps *
hdspchan_getcaps(kobj_t obj, void *data)
{
	struct sc_chinfo *ch;

	ch = data;

#if 0
	device_printf(ch->parent->dev, "hdspchan_getcaps()\n");
#endif

	if (ch->caps != NULL)
		return (ch->caps);

	return (&hdsp_bkp_caps);
}

static kobj_method_t hdspchan_methods[] = {
	KOBJMETHOD(channel_init,         hdspchan_init),
	KOBJMETHOD(channel_free,         hdspchan_free),
	KOBJMETHOD(channel_setformat,    hdspchan_setformat),
	KOBJMETHOD(channel_setspeed,     hdspchan_setspeed),
	KOBJMETHOD(channel_setblocksize, hdspchan_setblocksize),
	KOBJMETHOD(channel_trigger,      hdspchan_trigger),
	KOBJMETHOD(channel_getptr,       hdspchan_getptr),
	KOBJMETHOD(channel_getcaps,      hdspchan_getcaps),
	KOBJMETHOD_END
};
CHANNEL_DECLARE(hdspchan);

static int
hdsp_pcm_probe(device_t dev)
{

#if 0
	device_printf(dev,"hdsp_pcm_probe()\n");
#endif

	return (0);
}

static uint32_t
hdsp_pcm_intr(struct sc_pcminfo *scp)
{
	struct sc_chinfo *ch;
	struct sc_info *sc;
	int i;

	sc = scp->sc;

	for (i = 0; i < scp->chnum; i++) {
		ch = &scp->chan[i];
		snd_mtxunlock(sc->lock);
		chn_intr(ch->channel);
		snd_mtxlock(sc->lock);
	}

	return (0);
}

static int
hdsp_pcm_attach(device_t dev)
{
	char status[SND_STATUSLEN];
	struct sc_pcminfo *scp;
	const char *buf;
	uint32_t pcm_flags;
	int err;
	int play, rec;

	scp = device_get_ivars(dev);
	scp->ih = &hdsp_pcm_intr;

	if (scp->hc->ports & HDSP_CHAN_9632_ALL)
		buf = "9632";
	else if (scp->hc->ports & HDSP_CHAN_9652_ALL)
		buf = "9652";
	else
		buf = "?";
	device_set_descf(dev, "HDSP %s [%s]", buf, scp->hc->descr);

	/*
	 * We don't register interrupt handler with snd_setup_intr
	 * in pcm device. Mark pcm device as MPSAFE manually.
	 */
	pcm_flags = pcm_getflags(dev) | SD_F_MPSAFE;
	if (hdsp_port_slot_count_max(scp->hc->ports) > HDSP_MATRIX_MAX)
		/* Disable vchan conversion, too many channels. */
		pcm_flags |= SD_F_BITPERFECT;
	pcm_setflags(dev, pcm_flags);

	pcm_init(dev, scp);

	play = (hdsp_channel_play_ports(scp->hc)) ? 1 : 0;
	rec = (hdsp_channel_rec_ports(scp->hc)) ? 1 : 0;

	scp->chnum = 0;
	if (play) {
		pcm_addchan(dev, PCMDIR_PLAY, &hdspchan_class, scp);
		scp->chnum++;
	}

	if (rec) {
		pcm_addchan(dev, PCMDIR_REC, &hdspchan_class, scp);
		scp->chnum++;
	}

	snprintf(status, SND_STATUSLEN, "port 0x%jx irq %jd on %s",
	    rman_get_start(scp->sc->cs),
	    rman_get_start(scp->sc->irq),
	    device_get_nameunit(device_get_parent(dev)));
	err = pcm_register(dev, status);
	if (err) {
		device_printf(dev, "Can't register pcm.\n");
		return (ENXIO);
	}

	mixer_init(dev, &hdspmixer_class, scp);

	return (0);
}

static int
hdsp_pcm_detach(device_t dev)
{
	int err;

	err = pcm_unregister(dev);
	if (err) {
		device_printf(dev, "Can't unregister device.\n");
		return (err);
	}

	return (0);
}

static device_method_t hdsp_pcm_methods[] = {
	DEVMETHOD(device_probe,     hdsp_pcm_probe),
	DEVMETHOD(device_attach,    hdsp_pcm_attach),
	DEVMETHOD(device_detach,    hdsp_pcm_detach),
	{ 0, 0 }
};

static driver_t hdsp_pcm_driver = {
	"pcm",
	hdsp_pcm_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_hdsp_pcm, hdsp, hdsp_pcm_driver, 0, 0);
MODULE_DEPEND(snd_hdsp, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(snd_hdsp, 1);
