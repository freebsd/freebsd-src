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
 * RME HDSPe driver for FreeBSD (pcm-part).
 * Supported cards: AIO, RayDAT.
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pci/hdspe.h>
#include <dev/sound/chip.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <mixer_if.h>

struct hdspe_latency {
	uint32_t n;
	uint32_t period;
	float ms;
};

static struct hdspe_latency latency_map[] = {
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

struct hdspe_rate {
	uint32_t speed;
	uint32_t reg;
};

static struct hdspe_rate rate_map[] = {
	{  32000, (HDSPE_FREQ_32000) },
	{  44100, (HDSPE_FREQ_44100) },
	{  48000, (HDSPE_FREQ_48000) },
	{  64000, (HDSPE_FREQ_32000 | HDSPE_FREQ_DOUBLE) },
	{  88200, (HDSPE_FREQ_44100 | HDSPE_FREQ_DOUBLE) },
	{  96000, (HDSPE_FREQ_48000 | HDSPE_FREQ_DOUBLE) },
	{ 128000, (HDSPE_FREQ_32000 | HDSPE_FREQ_QUAD)   },
	{ 176400, (HDSPE_FREQ_44100 | HDSPE_FREQ_QUAD)   },
	{ 192000, (HDSPE_FREQ_48000 | HDSPE_FREQ_QUAD)   },

	{ 0, 0 },
};

static uint32_t
hdspe_channel_play_ports(struct hdspe_channel *hc)
{
	return (hc->ports & (HDSPE_CHAN_AIO_ALL | HDSPE_CHAN_RAY_ALL));
}

static uint32_t
hdspe_channel_rec_ports(struct hdspe_channel *hc)
{
	return (hc->ports & (HDSPE_CHAN_AIO_ALL_REC | HDSPE_CHAN_RAY_ALL));
}

static unsigned int
hdspe_adat_width(uint32_t speed)
{
	if (speed > 96000)
		return (2);
	if (speed > 48000)
		return (4);
	return (8);
}

static uint32_t
hdspe_port_first(uint32_t ports)
{
	return (ports & (~(ports - 1)));	/* Extract first bit set. */
}

static uint32_t
hdspe_port_first_row(uint32_t ports)
{
	uint32_t ends;

	/* Restrict ports to one set with contiguous slots. */
	if (ports & HDSPE_CHAN_AIO_LINE)
		ports = HDSPE_CHAN_AIO_LINE;	/* Gap in the AIO slots here. */
	else if (ports & HDSPE_CHAN_AIO_ALL)
		ports &= HDSPE_CHAN_AIO_ALL;	/* Rest of the AIO slots. */
	else if (ports & HDSPE_CHAN_RAY_ALL)
		ports &= HDSPE_CHAN_RAY_ALL;	/* All RayDAT slots. */

	/* Ends of port rows are followed by a port which is not in the set. */
	ends = ports & (~(ports >> 1));
	/* First row of contiguous ports ends in the first row end. */
	return (ports & (ends ^ (ends - 1)));
}

static unsigned int
hdspe_channel_count(uint32_t ports, uint32_t adat_width)
{
	unsigned int count = 0;

	if (ports & HDSPE_CHAN_AIO_ALL) {
		/* AIO ports. */
		if (ports & HDSPE_CHAN_AIO_LINE)
			count += 2;
		if (ports & HDSPE_CHAN_AIO_PHONE)
			count += 2;
		if (ports & HDSPE_CHAN_AIO_AES)
			count += 2;
		if (ports & HDSPE_CHAN_AIO_SPDIF)
			count += 2;
		if (ports & HDSPE_CHAN_AIO_ADAT)
			count += adat_width;
	} else if (ports & HDSPE_CHAN_RAY_ALL) {
		/* RayDAT ports. */
		if (ports & HDSPE_CHAN_RAY_AES)
			count += 2;
		if (ports & HDSPE_CHAN_RAY_SPDIF)
			count += 2;
		if (ports & HDSPE_CHAN_RAY_ADAT1)
			count += adat_width;
		if (ports & HDSPE_CHAN_RAY_ADAT2)
			count += adat_width;
		if (ports & HDSPE_CHAN_RAY_ADAT3)
			count += adat_width;
		if (ports & HDSPE_CHAN_RAY_ADAT4)
			count += adat_width;
	}

	return (count);
}

static unsigned int
hdspe_channel_offset(uint32_t subset, uint32_t ports, unsigned int adat_width)
{
	uint32_t preceding;

	/* Make sure we have a subset of ports. */
	subset &= ports;
	/* Include all ports preceding the first one of the subset. */
	preceding = ports & (~subset & (subset - 1));

	if (preceding & HDSPE_CHAN_AIO_ALL)
		preceding &= HDSPE_CHAN_AIO_ALL;	/* Contiguous AIO slots. */
	else if (preceding & HDSPE_CHAN_RAY_ALL)
		preceding &= HDSPE_CHAN_RAY_ALL;	/* Contiguous RayDAT slots. */

	return (hdspe_channel_count(preceding, adat_width));
}

static unsigned int
hdspe_port_slot_offset(uint32_t port, unsigned int adat_width)
{
	/* Exctract the first port (lowest bit) if set of ports. */
	switch (hdspe_port_first(port)) {
	/* AIO ports */
	case HDSPE_CHAN_AIO_LINE:
		return (0);
	case HDSPE_CHAN_AIO_PHONE:
		return (6);
	case HDSPE_CHAN_AIO_AES:
		return (8);
	case HDSPE_CHAN_AIO_SPDIF:
		return (10);
	case HDSPE_CHAN_AIO_ADAT:
		return (12);

	/* RayDAT ports */
	case HDSPE_CHAN_RAY_AES:
		return (0);
	case HDSPE_CHAN_RAY_SPDIF:
		return (2);
	case HDSPE_CHAN_RAY_ADAT1:
		return (4);
	case HDSPE_CHAN_RAY_ADAT2:
		return (4 + adat_width);
	case HDSPE_CHAN_RAY_ADAT3:
		return (4 + 2 * adat_width);
	case HDSPE_CHAN_RAY_ADAT4:
		return (4 + 3 * adat_width);
	default:
		return (0);
	}
}

static unsigned int
hdspe_port_slot_width(uint32_t ports, unsigned int adat_width)
{
	uint32_t row;

	/* Count number of contiguous slots from the first physical port. */
	row = hdspe_port_first_row(ports);
	return (hdspe_channel_count(row, adat_width));
}

static int
hdspe_hw_mixer(struct sc_chinfo *ch, unsigned int dst,
    unsigned int src, unsigned short data)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	int offs;

	scp = ch->parent;
	sc = scp->sc;

	offs = 0;
	if (ch->dir == PCMDIR_PLAY)
		offs = 64;

	hdspe_write_4(sc, HDSPE_MIXER_BASE +
	    ((offs + src + 128 * dst) * sizeof(uint32_t)),
	    data & 0xFFFF);

	return (0);
};

static int
hdspechan_setgain(struct sc_chinfo *ch)
{
	struct sc_info *sc;
	uint32_t port, ports;
	unsigned int slot, end_slot;
	unsigned short volume;

	sc = ch->parent->sc;

	/* Iterate through all physical ports of the channel. */
	ports = ch->ports;
	port = hdspe_port_first(ports);
	while (port != 0) {
		/* Get slot range of the physical port. */
		slot =
		    hdspe_port_slot_offset(port, hdspe_adat_width(sc->speed));
		end_slot = slot +
		    hdspe_port_slot_width(port, hdspe_adat_width(sc->speed));

		/* Treat first slot as left channel. */
		volume = ch->lvol * HDSPE_MAX_GAIN / 100;
		for (; slot < end_slot; slot++) {
			hdspe_hw_mixer(ch, slot, slot, volume);
			/* Subsequent slots all get the right channel volume. */
			volume = ch->rvol * HDSPE_MAX_GAIN / 100;
		}

		ports &= ~port;
		port = hdspe_port_first(ports);
	}

	return (0);
}

static int
hdspemixer_init(struct snd_mixer *m)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	int mask;

	scp = mix_getdevinfo(m);
	sc = scp->sc;
	if (sc == NULL)
		return (-1);

	mask = SOUND_MASK_PCM;

	if (hdspe_channel_play_ports(scp->hc))
		mask |= SOUND_MASK_VOLUME;

	if (hdspe_channel_rec_ports(scp->hc))
		mask |= SOUND_MASK_RECLEV;

	snd_mtxlock(sc->lock);
	pcm_setflags(scp->dev, pcm_getflags(scp->dev) | SD_F_SOFTPCMVOL);
	mix_setdevs(m, mask);
	snd_mtxunlock(sc->lock);

	return (0);
}

static int
hdspemixer_set(struct snd_mixer *m, unsigned dev,
    unsigned left, unsigned right)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	int i;

	scp = mix_getdevinfo(m);

#if 0
	device_printf(scp->dev, "hdspemixer_set() %d %d\n",
	    left, right);
#endif

	for (i = 0; i < scp->chnum; i++) {
		ch = &scp->chan[i];
		if ((dev == SOUND_MIXER_VOLUME && ch->dir == PCMDIR_PLAY) ||
		    (dev == SOUND_MIXER_RECLEV && ch->dir == PCMDIR_REC)) {
			ch->lvol = left;
			ch->rvol = right;
			if (ch->run)
				hdspechan_setgain(ch);
		}
	}

	return (0);
}

static kobj_method_t hdspemixer_methods[] = {
	KOBJMETHOD(mixer_init,      hdspemixer_init),
	KOBJMETHOD(mixer_set,       hdspemixer_set),
	KOBJMETHOD_END
};
MIXER_DECLARE(hdspemixer);

static void
hdspechan_enable(struct sc_chinfo *ch, int value)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t row, ports;
	int reg;
	unsigned int slot, end_slot;

	scp = ch->parent;
	sc = scp->sc;

	if (ch->dir == PCMDIR_PLAY)
		reg = HDSPE_OUT_ENABLE_BASE;
	else
		reg = HDSPE_IN_ENABLE_BASE;

	ch->run = value;

	/* Iterate through rows of ports with contiguous slots. */
	ports = ch->ports;
	row = hdspe_port_first_row(ports);
	while (row != 0) {
		slot =
		    hdspe_port_slot_offset(row, hdspe_adat_width(sc->speed));
		end_slot = slot +
		    hdspe_port_slot_width(row, hdspe_adat_width(sc->speed));

		for (; slot < end_slot; slot++) {
			hdspe_write_1(sc, reg + (4 * slot), value);
		}

		ports &= ~row;
		row = hdspe_port_first_row(ports);
	}
}

static int
hdspe_running(struct sc_info *sc)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	device_t *devlist;
	int devcount;
	int i, j;
	int err;

	if ((err = device_get_children(sc->dev, &devlist, &devcount)) != 0)
		goto bad;

	for (i = 0; i < devcount; i++) {
		scp = device_get_ivars(devlist[i]);
		for (j = 0; j < scp->chnum; j++) {
			ch = &scp->chan[j];
			if (ch->run)
				goto bad;
		}
	}

	free(devlist, M_TEMP);

	return (0);
bad:

#if 0
	device_printf(sc->dev, "hdspe is running\n");
#endif

	free(devlist, M_TEMP);

	return (1);
}

static void
hdspe_start_audio(struct sc_info *sc)
{

	sc->ctrl_register |= (HDSPE_AUDIO_INT_ENABLE | HDSPE_ENABLE);
	hdspe_write_4(sc, HDSPE_CONTROL_REG, sc->ctrl_register);
}

static void
hdspe_stop_audio(struct sc_info *sc)
{

	if (hdspe_running(sc) == 1)
		return;

	sc->ctrl_register &= ~(HDSPE_AUDIO_INT_ENABLE | HDSPE_ENABLE);
	hdspe_write_4(sc, HDSPE_CONTROL_REG, sc->ctrl_register);
}

static void
buffer_mux_write(uint32_t *dma, uint32_t *pcm, unsigned int pos,
    unsigned int samples, unsigned int slots, unsigned int channels)
{
	int slot;

	for (; samples > 0; samples--) {
		for (slot = 0; slot < slots; slot++) {
			dma[slot * HDSPE_CHANBUF_SAMPLES + pos] =
			    pcm[pos * channels + slot];
		}
		pos = (pos + 1) % HDSPE_CHANBUF_SAMPLES;
	}
}

static void
buffer_mux_port(uint32_t *dma, uint32_t *pcm, uint32_t subset, uint32_t ports,
    unsigned int pos, unsigned int samples, unsigned int adat_width,
    unsigned int pcm_width)
{
	unsigned int slot_offset, slots;
	unsigned int channels, chan_pos;

	/* Translate DMA slot offset to DMA buffer offset. */
	slot_offset = hdspe_port_slot_offset(subset, adat_width);
	dma += slot_offset * HDSPE_CHANBUF_SAMPLES;

	/* Channel position of the port subset and total number of channels. */
	chan_pos = hdspe_channel_offset(subset, ports, pcm_width);
	pcm += chan_pos;
	channels = hdspe_channel_count(ports, pcm_width);

	/* Only copy as much as supported by both hardware and pcm channel. */
	slots = hdspe_port_slot_width(subset, MIN(adat_width, pcm_width));

	/* Let the compiler inline and loop unroll common cases. */
	if (slots == 2)
		buffer_mux_write(dma, pcm, pos, samples, 2, channels);
	else if (slots == 4)
		buffer_mux_write(dma, pcm, pos, samples, 4, channels);
	else if (slots == 8)
		buffer_mux_write(dma, pcm, pos, samples, 8, channels);
	else
		buffer_mux_write(dma, pcm, pos, samples, slots, channels);
}

static void
buffer_demux_read(uint32_t *dma, uint32_t *pcm, unsigned int pos,
    unsigned int samples, unsigned int slots, unsigned int channels)
{
	int slot;

	for (; samples > 0; samples--) {
		for (slot = 0; slot < slots; slot++) {
			pcm[pos * channels + slot] =
			    dma[slot * HDSPE_CHANBUF_SAMPLES + pos];
		}
		pos = (pos + 1) % HDSPE_CHANBUF_SAMPLES;
	}
}

static void
buffer_demux_port(uint32_t *dma, uint32_t *pcm, uint32_t subset, uint32_t ports,
    unsigned int pos, unsigned int samples, unsigned int adat_width,
    unsigned int pcm_width)
{
	unsigned int slot_offset, slots;
	unsigned int channels, chan_pos;

	/* Translate port slot offset to DMA buffer offset. */
	slot_offset = hdspe_port_slot_offset(subset, adat_width);
	dma += slot_offset * HDSPE_CHANBUF_SAMPLES;

	/* Channel position of the port subset and total number of channels. */
	chan_pos = hdspe_channel_offset(subset, ports, pcm_width);
	pcm += chan_pos;
	channels = hdspe_channel_count(ports, pcm_width);

	/* Only copy as much as supported by both hardware and pcm channel. */
	slots = hdspe_port_slot_width(subset, MIN(adat_width, pcm_width));

	/* Let the compiler inline and loop unroll common cases. */
	if (slots == 2)
		buffer_demux_read(dma, pcm, pos, samples, 2, channels);
	else if (slots == 4)
		buffer_demux_read(dma, pcm, pos, samples, 4, channels);
	else if (slots == 8)
		buffer_demux_read(dma, pcm, pos, samples, 8, channels);
	else
		buffer_demux_read(dma, pcm, pos, samples, slots, channels);
}


/* Copy data between DMA and PCM buffers. */
static void
buffer_copy(struct sc_chinfo *ch)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t row, ports;
	unsigned int pos;
	unsigned int n;
	unsigned int adat_width, pcm_width;

	scp = ch->parent;
	sc = scp->sc;

	n = AFMT_CHANNEL(ch->format); /* n channels */

	/* Let pcm formats differ from current hardware ADAT width. */
	adat_width = hdspe_adat_width(sc->speed);
	if (n == hdspe_channel_count(ch->ports, 2))
		pcm_width = 2;
	else if (n == hdspe_channel_count(ch->ports, 4))
		pcm_width = 4;
	else
		pcm_width = 8;

	if (ch->dir == PCMDIR_PLAY)
		pos = sndbuf_getreadyptr(ch->buffer);
	else
		pos = sndbuf_getfreeptr(ch->buffer);

	pos /= 4; /* Bytes per sample. */
	pos /= n; /* Destination buffer n-times smaller. */

	/* Iterate through rows of ports with contiguous slots. */
	ports = ch->ports;
	if (pcm_width == adat_width)
		row = hdspe_port_first_row(ports);
	else
		row = hdspe_port_first(ports);

	while (row != 0) {
		if (ch->dir == PCMDIR_PLAY)
			buffer_mux_port(sc->pbuf, ch->data, row, ch->ports, pos,
			    sc->period * 2, adat_width, pcm_width);
		else
			buffer_demux_port(sc->rbuf, ch->data, row, ch->ports,
			    pos, sc->period * 2, adat_width, pcm_width);

		ports &= ~row;
		if (pcm_width == adat_width)
			row = hdspe_port_first_row(ports);
		else
			row = hdspe_port_first(ports);
	}
}

static int
clean(struct sc_chinfo *ch)
{
	struct sc_pcminfo *scp;
	struct sc_info *sc;
	uint32_t *buf;
	uint32_t row, ports;
	unsigned int offset, slots;

	scp = ch->parent;
	sc = scp->sc;
	buf = sc->rbuf;

	if (ch->dir == PCMDIR_PLAY)
		buf = sc->pbuf;

	/* Iterate through rows of ports with contiguous slots. */
	ports = ch->ports;
	row = hdspe_port_first_row(ports);
	while (row != 0) {
		offset = hdspe_port_slot_offset(row,
		    hdspe_adat_width(sc->speed));
		slots = hdspe_port_slot_width(row, hdspe_adat_width(sc->speed));

		bzero(buf + offset * HDSPE_CHANBUF_SAMPLES,
		    slots * HDSPE_CHANBUF_SIZE);

		ports &= ~row;
		row = hdspe_port_first_row(ports);
	}

	return (0);
}

/* Channel interface. */
static void *
hdspechan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b,
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
		ch->ports = hdspe_channel_play_ports(scp->hc);
	else
		ch->ports = hdspe_channel_rec_ports(scp->hc);

	ch->run = 0;
	ch->lvol = 0;
	ch->rvol = 0;

	/* Support all possible ADAT widths as channel formats. */
	ch->cap_fmts[0] =
	    SND_FORMAT(AFMT_S32_LE, hdspe_channel_count(ch->ports, 2), 0);
	ch->cap_fmts[1] =
	    SND_FORMAT(AFMT_S32_LE, hdspe_channel_count(ch->ports, 4), 0);
	ch->cap_fmts[2] =
	    SND_FORMAT(AFMT_S32_LE, hdspe_channel_count(ch->ports, 8), 0);
	ch->cap_fmts[3] = 0;
	ch->caps = malloc(sizeof(struct pcmchan_caps), M_HDSPE, M_NOWAIT);
	*(ch->caps) = (struct pcmchan_caps) {32000, 192000, ch->cap_fmts, 0};

	/* Allocate maximum buffer size. */
	ch->size = HDSPE_CHANBUF_SIZE * hdspe_channel_count(ch->ports, 8);
	ch->data = malloc(ch->size, M_HDSPE, M_NOWAIT);

	ch->buffer = b;
	ch->channel = c;
	ch->parent = scp;

	ch->dir = dir;

	snd_mtxunlock(sc->lock);

	if (sndbuf_setup(ch->buffer, ch->data, ch->size) != 0) {
		device_printf(scp->dev, "Can't setup sndbuf.\n");
		return (NULL);
	}

	return (ch);
}

static int
hdspechan_trigger(kobj_t obj, void *data, int go)
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
		device_printf(scp->dev, "hdspechan_trigger(): start\n");
#endif
		hdspechan_enable(ch, 1);
		hdspechan_setgain(ch);
		hdspe_start_audio(sc);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
#if 0
		device_printf(scp->dev, "hdspechan_trigger(): stop or abort\n");
#endif
		clean(ch);
		hdspechan_enable(ch, 0);
		hdspe_stop_audio(sc);
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
hdspechan_getptr(kobj_t obj, void *data)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	struct sc_info *sc;
	uint32_t ret, pos;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;

	snd_mtxlock(sc->lock);
	ret = hdspe_read_2(sc, HDSPE_STATUS_REG);
	snd_mtxunlock(sc->lock);

	pos = ret & HDSPE_BUF_POSITION_MASK;
	pos *= AFMT_CHANNEL(ch->format); /* Hardbuf with multiple channels. */

	return (pos);
}

static int
hdspechan_free(kobj_t obj, void *data)
{
	struct sc_pcminfo *scp;
	struct sc_chinfo *ch;
	struct sc_info *sc;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;

#if 0
	device_printf(scp->dev, "hdspechan_free()\n");
#endif

	snd_mtxlock(sc->lock);
	if (ch->data != NULL) {
		free(ch->data, M_HDSPE);
		ch->data = NULL;
	}
	if (ch->caps != NULL) {
		free(ch->caps, M_HDSPE);
		ch->caps = NULL;
	}
	snd_mtxunlock(sc->lock);

	return (0);
}

static int
hdspechan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct sc_chinfo *ch;

	ch = data;

#if 0
	struct sc_pcminfo *scp = ch->parent;
	device_printf(scp->dev, "hdspechan_setformat(%d)\n", format);
#endif

	ch->format = format;

	return (0);
}

static uint32_t
hdspechan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct sc_pcminfo *scp;
	struct hdspe_rate *hr;
	struct sc_chinfo *ch;
	struct sc_info *sc;
	long long period;
	int threshold;
	int i;

	ch = data;
	scp = ch->parent;
	sc = scp->sc;
	hr = NULL;

#if 0
	device_printf(scp->dev, "hdspechan_setspeed(%d)\n", speed);
#endif

	if (hdspe_running(sc) == 1)
		goto end;

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

	switch (sc->type) {
	case HDSPE_RAYDAT:
	case HDSPE_AIO:
		period = HDSPE_FREQ_AIO;
		break;
	default:
		/* Unsupported card. */
		goto end;
	}

	/* Write frequency on the device. */
	sc->ctrl_register &= ~HDSPE_FREQ_MASK;
	sc->ctrl_register |= hr->reg;
	hdspe_write_4(sc, HDSPE_CONTROL_REG, sc->ctrl_register);

	speed = hr->speed;
	if (speed > 96000)
		speed /= 4;
	else if (speed > 48000)
		speed /= 2;

	/* Set DDS value. */
	period /= speed;
	hdspe_write_4(sc, HDSPE_FREQ_REG, period);

	sc->speed = hr->speed;
end:

	return (sc->speed);
}

static uint32_t
hdspechan_setblocksize(kobj_t obj, void *data, uint32_t blocksize)
{
	struct hdspe_latency *hl;
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
	device_printf(scp->dev, "hdspechan_setblocksize(%d)\n", blocksize);
#endif

	if (hdspe_running(sc) == 1)
		goto end;

	if (blocksize > HDSPE_LAT_BYTES_MAX)
		blocksize = HDSPE_LAT_BYTES_MAX;
	else if (blocksize < HDSPE_LAT_BYTES_MIN)
		blocksize = HDSPE_LAT_BYTES_MIN;

	blocksize /= 4 /* samples */;

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
	sc->ctrl_register &= ~HDSPE_LAT_MASK;
	sc->ctrl_register |= hdspe_encode_latency(hl->n);
	hdspe_write_4(sc, HDSPE_CONTROL_REG, sc->ctrl_register);
	sc->period = hl->period;
	snd_mtxunlock(sc->lock);

#if 0
	device_printf(scp->dev, "New period=%d\n", sc->period);
#endif

	sndbuf_resize(ch->buffer,
	    (HDSPE_CHANBUF_SIZE * AFMT_CHANNEL(ch->format)) / (sc->period * 4),
	    (sc->period * 4));
end:

	return (sndbuf_getblksz(ch->buffer));
}

static uint32_t hdspe_bkp_fmt[] = {
	SND_FORMAT(AFMT_S32_LE, 2, 0),
	0
};

static struct pcmchan_caps hdspe_bkp_caps = {32000, 192000, hdspe_bkp_fmt, 0};

static struct pcmchan_caps *
hdspechan_getcaps(kobj_t obj, void *data)
{
	struct sc_chinfo *ch;

	ch = data;

#if 0
	struct sc_pcminfo *scl = ch->parent;
	device_printf(scp->dev, "hdspechan_getcaps()\n");
#endif

	if (ch->caps != NULL)
		return (ch->caps);

	return (&hdspe_bkp_caps);
}

static kobj_method_t hdspechan_methods[] = {
	KOBJMETHOD(channel_init,         hdspechan_init),
	KOBJMETHOD(channel_free,         hdspechan_free),
	KOBJMETHOD(channel_setformat,    hdspechan_setformat),
	KOBJMETHOD(channel_setspeed,     hdspechan_setspeed),
	KOBJMETHOD(channel_setblocksize, hdspechan_setblocksize),
	KOBJMETHOD(channel_trigger,      hdspechan_trigger),
	KOBJMETHOD(channel_getptr,       hdspechan_getptr),
	KOBJMETHOD(channel_getcaps,      hdspechan_getcaps),
	KOBJMETHOD_END
};
CHANNEL_DECLARE(hdspechan);

static int
hdspe_pcm_probe(device_t dev)
{

#if 0
	device_printf(dev,"hdspe_pcm_probe()\n");
#endif

	return (0);
}

static uint32_t
hdspe_pcm_intr(struct sc_pcminfo *scp)
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
hdspe_pcm_attach(device_t dev)
{
	char status[SND_STATUSLEN];
	struct sc_pcminfo *scp;
	const char *buf;
	int err;
	int play, rec;

	scp = device_get_ivars(dev);
	scp->ih = &hdspe_pcm_intr;

	if (scp->hc->ports & HDSPE_CHAN_AIO_ALL)
		buf = "AIO";
	else if (scp->hc->ports & HDSPE_CHAN_RAY_ALL)
		buf = "RayDAT";
	else
		buf = "?";
	device_set_descf(dev, "HDSPe %s [%s]", buf, scp->hc->descr);

	/*
	 * We don't register interrupt handler with snd_setup_intr
	 * in pcm device. Mark pcm device as MPSAFE manually.
	 */
	pcm_setflags(dev, pcm_getflags(dev) | SD_F_MPSAFE);

	play = (hdspe_channel_play_ports(scp->hc)) ? 1 : 0;
	rec = (hdspe_channel_rec_ports(scp->hc)) ? 1 : 0;
	err = pcm_register(dev, scp, play, rec);
	if (err) {
		device_printf(dev, "Can't register pcm.\n");
		return (ENXIO);
	}

	scp->chnum = 0;
	if (play) {
		pcm_addchan(dev, PCMDIR_PLAY, &hdspechan_class, scp);
		scp->chnum++;
	}

	if (rec) {
		pcm_addchan(dev, PCMDIR_REC, &hdspechan_class, scp);
		scp->chnum++;
	}

	snprintf(status, SND_STATUSLEN, "port 0x%jx irq %jd on %s",
	    rman_get_start(scp->sc->cs),
	    rman_get_start(scp->sc->irq),
	    device_get_nameunit(device_get_parent(dev)));
	pcm_setstatus(dev, status);

	mixer_init(dev, &hdspemixer_class, scp);

	return (0);
}

static int
hdspe_pcm_detach(device_t dev)
{
	int err;

	err = pcm_unregister(dev);
	if (err) {
		device_printf(dev, "Can't unregister device.\n");
		return (err);
	}

	return (0);
}

static device_method_t hdspe_pcm_methods[] = {
	DEVMETHOD(device_probe,     hdspe_pcm_probe),
	DEVMETHOD(device_attach,    hdspe_pcm_attach),
	DEVMETHOD(device_detach,    hdspe_pcm_detach),
	{ 0, 0 }
};

static driver_t hdspe_pcm_driver = {
	"pcm",
	hdspe_pcm_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_hdspe_pcm, hdspe, hdspe_pcm_driver, 0, 0);
MODULE_DEPEND(snd_hdspe, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
MODULE_VERSION(snd_hdspe, 1);
