/*
 * Copyright by Hannu Savolainen 1993
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

/*
 * This is the interface for a sequencer to interact a midi driver.
 * This interface translates the sequencer operations to the corresponding
 * midi messages, and vice versa.
 */

#include <dev/sound/midi/midi.h>

#define TYPEDRANGE(type, x, lower, upper) \
{ \
	type tl, tu; \
	tl = (lower); \
	tu = (upper); \
	if (x < tl) { \
		x = tl; \
	} else if(x > tu) { \
		x = tu; \
	} \
}

/*
 * These functions goes into midisynthdev_op_desc.
 */
static mdsy_killnote_t synth_killnote;
static mdsy_setinstr_t synth_setinstr;
static mdsy_startnote_t synth_startnote;
static mdsy_reset_t synth_reset;
static mdsy_hwcontrol_t synth_hwcontrol;
static mdsy_loadpatch_t synth_loadpatch;
static mdsy_panning_t synth_panning;
static mdsy_aftertouch_t synth_aftertouch;
static mdsy_controller_t synth_controller;
static mdsy_patchmgr_t synth_patchmgr;
static mdsy_bender_t synth_bender;
static mdsy_allocvoice_t synth_allocvoice;
static mdsy_setupvoice_t synth_setupvoice;
static mdsy_sendsysex_t synth_sendsysex;
static mdsy_prefixcmd_t synth_prefixcmd;
static mdsy_volumemethod_t synth_volumemethod;
static mdsy_readraw_t synth_readraw;
static mdsy_writeraw_t synth_writeraw;

/*
 * This is the synthdev_info for a midi interface device.
 * You may have to replace a few of functions for an internal
 * synthesizer.
 */
synthdev_info midisynth_op_desc = {
	synth_killnote,
	synth_setinstr,
	synth_startnote,
	synth_reset,
	synth_hwcontrol,
	synth_loadpatch,
	synth_panning,
	synth_aftertouch,
	synth_controller,
	synth_patchmgr,
	synth_bender,
	synth_allocvoice,
	synth_setupvoice,
	synth_sendsysex,
	synth_prefixcmd,
	synth_volumemethod,
	synth_readraw,
	synth_writeraw,
};

/* The following functions are local. */
static int synth_leavesysex(mididev_info *md);

/*
 * Here are the main functions to interact to the midi sequencer.
 * These are called from the sequencer functions in sequencer.c.
 */

static int
synth_killnote(mididev_info *md, int chn, int note, int vel)
{
	int unit, lenw;
	synthdev_info *sd;
	u_char c[3];

	unit = md->unit;
	sd = &md->synth;

	if (note < 0 || note > 127 || chn < 0 || chn > 15)
		return (EINVAL);
	TYPEDRANGE(int, vel, 0, 127);
	if (synth_leavesysex(md) == EAGAIN)
		return (EAGAIN);

	if (vel == 64) {
		c[0] = 0x90 | (chn & 0x0f); /* Note on. */
		c[1] = (u_char)note;
		c[2] = 0;
	} else {
		c[0] = 0x80 | (chn & 0x0f); /* Note off. */
		c[1] = (u_char)note;
		c[2] = (u_char)vel;
	}

	if (synth_prefixcmd(md, c[0]))
		return (0);

	return (md->synth.writeraw(md, c, 3, &lenw, 1));
}

static int
synth_setinstr(mididev_info *md, int chn, int instr)
{
	int unit, lenw;
	synthdev_info *sd;
	u_char c[2];

	unit = md->unit;
	sd = &md->synth;

	if (instr < 0 || instr > 127 || chn < 0 || chn > 15)
		return (EINVAL);

	if (synth_leavesysex(md) == EAGAIN)
		return (EAGAIN);

	c[0] = 0xc0 | (chn & 0x0f); /* Progamme change. */
	c[1] = (u_char)instr;

	return (md->synth.writeraw(md, c, 3, &lenw, 1));
}

static int
synth_startnote(mididev_info *md, int chn, int note, int vel)
{
	int unit, lenw;
	synthdev_info *sd;
	u_char c[3];

	unit = md->unit;
	sd = &md->synth;

	if (note < 0 || note > 127 || chn < 0 || chn > 15)
		return (EINVAL);
	TYPEDRANGE(int, vel, 0, 127);
	if (synth_leavesysex(md) == EAGAIN)
		return (EAGAIN);

	c[0] = 0x90 | (chn & 0x0f); /* Note on. */
	c[1] = (u_char)note;
	c[2] = (u_char)vel;
	if (synth_prefixcmd(md, c[0]))
		return (0);

	return (md->synth.writeraw(md, c, 3, &lenw, 1));
}

static int
synth_reset(mididev_info *md)
{
	synth_leavesysex(md);
	return (0);
}

static int
synth_hwcontrol(mididev_info *md, u_char *event)
{
	/* NOP. */
	return (0);
}

static int
synth_loadpatch(mididev_info *md, int format, struct uio *buf, int offs, int count, int pmgr_flag)
{
	struct sysex_info sysex;
	synthdev_info *sd;
	int unit, i, eox_seen, first_byte, left, src_offs, hdr_size, lenw;
	u_char c[count];

	unit = md->unit;
	sd = &md->synth;

	eox_seen = 0;
	first_byte = 1;
	hdr_size = offsetof(struct sysex_info, data);

	if (synth_leavesysex(md) == EAGAIN)
		return (EAGAIN);

	if (synth_prefixcmd(md, 0xf0))
		return (0);
	if (format != SYSEX_PATCH) {
		printf("synth_loadpatch: patch format 0x%x is invalid.\n", format);
		return (EINVAL);
	}
	if (count < hdr_size) {
		printf("synth_loadpatch: patch header is too short.\n");
		return (EINVAL);
	}
	count -= hdr_size;

	/* Copy the patch data. */
	if (uiomove((caddr_t)&((char *)&sysex)[offs], hdr_size - offs, buf))
		printf("synth_loadpatch: memory mangled?\n");

	if (count < sysex.len) {
		sysex.len = (long)count;
		printf("synth_loadpatch: sysex record of %d bytes is too long, adjusted to %d bytes.\n", (int)sysex.len, count);
	}
	left = sysex.len;
	src_offs = 0;

	for (i = 0 ; i < left ; i++) {
		uiomove((caddr_t)&c[i], 1, buf);
		eox_seen = i > 0 && (c[i] & 0x80) != 0;
		if (eox_seen && c[i] != 0xf7)
			c[i] = 0xf7;
		if (i == 0 && c[i] != 0x80) {
			printf("synth_loadpatch: sysex does not begin with the status.\n");
			return (EINVAL);
		}
		if (!first_byte && (c[i] & 0x80) != 0) {
			md->synth.writeraw(md, c, i + 1, &lenw, 0);
			return (0);
		}
		first_byte = 0;
	}

	if (!eox_seen) {
		c[0] = 0xf7;
		md->synth.writeraw(md, c, 1, &lenw, 0);
	}

	return (0);
}

static int
synth_panning(mididev_info *md, int chn, int pan)
{
	/* NOP. */
	return (0);
}

static int
synth_aftertouch(mididev_info *md, int chn, int press)
{
	int unit, lenw;
	synthdev_info *sd;
	u_char c[2];

	unit = md->unit;
	sd = &md->synth;

	if (press < 0 || press > 127 || chn < 0 || chn > 15)
		return (EINVAL);
	if (synth_leavesysex(md) == EAGAIN)
		return (EAGAIN);

	c[0] = 0xd0 | (chn & 0x0f); /* Channel Pressure. */
	c[1] = (u_char)press;
	if (synth_prefixcmd(md, c[0]))
		return (0);

	return (md->synth.writeraw(md, c, 2, &lenw, 1));
}

static int
synth_controller(mididev_info *md, int chn, int ctrlnum, int val)
{
	int unit, lenw;
	synthdev_info *sd;
	u_char c[3];

	unit = md->unit;
	sd = &md->synth;

	if (ctrlnum < 1 || ctrlnum > 127 || chn < 0 || chn > 15)
		return (EINVAL);
	if (synth_leavesysex(md) == EAGAIN)
		return (EAGAIN);

	c[0] = 0xb0 | (chn & 0x0f); /* Control Message. */
	c[1] = (u_char)ctrlnum;
	if (synth_prefixcmd(md, c[0]))
		return (0);

	return (md->synth.writeraw(md, c, 3, &lenw, 1));
}

static int
synth_patchmgr(mididev_info *md, struct patmgr_info *rec)
{
	return (EINVAL);
}

static int
synth_bender(mididev_info *md, int chn, int val)
{
	int unit, lenw;
	synthdev_info *sd;
	u_char c[3];

	unit = md->unit;
	sd = &md->synth;

	if (val < 0 || val > 16383 || chn < 0 || chn > 15)
		return (EINVAL);
	if (synth_leavesysex(md) == EAGAIN)
		return (EAGAIN);

	c[0] = 0xe0 | (chn & 0x0f); /* Pitch bend. */
	c[1] = (u_char)val & 0x7f;
	c[2] = (u_char)(val >> 7) & 0x7f;
	if (synth_prefixcmd(md, c[0]))
		return (0);

	return (md->synth.writeraw(md, c, 3, &lenw, 1));
}

static int
synth_allocvoice(mididev_info *md, int chn, int note, struct voice_alloc_info *alloc)
{
	/* NOP. */
	return (0);
}

static int
synth_setupvoice(mididev_info *md, int voice, int chn)
{
	/* NOP. */
	return (0);
}

static int
synth_sendsysex(mididev_info *md, u_char *sysex, int len)
{
	int unit, i, lenw;
	synthdev_info *sd;
	u_char c[len];

	unit = md->unit;
	sd = &md->synth;

	mtx_lock(&sd->status_mtx);
	for (i = 0 ; i < len ; i++) {
		switch (sysex[i]) {
		case 0xf0:
			/* Sysex begins. */
			if (synth_prefixcmd(md, 0xf0)) {
				mtx_unlock(&sd->status_mtx);
				return (0);
			}
			sd->sysex_state = 1;
			break;
		case 0xf7:
			/* Sysex ends. */
			if (!sd->sysex_state) {
				mtx_unlock(&sd->status_mtx);
				return (0);
			}
			sd->sysex_state = 0;
			break;
		default:
			if (!sd->sysex_state) {
				mtx_unlock(&sd->status_mtx);
				return (0);
			}
			if ((sysex[i] & 0x80) != 0) {
				/* A status in a sysex? */
				sysex[i] = 0xf7;
				sd->sysex_state = 0;
			}
			break;
		}
		c[i] = sysex[i];
		if (!sd->sysex_state)
			break;
	}
	mtx_unlock(&sd->status_mtx);

	return (md->synth.writeraw(md, c, i, &lenw, 1));
}

static int
synth_prefixcmd(mididev_info *md, int status)
{
	/* NOP. */
	return (0);
}

static int
synth_volumemethod(mididev_info *md, int mode)
{
	/* NOP. */
	return (0);
}

static int
synth_readraw(mididev_info *md, u_char *buf, int len, int *lenr, int nonblock)
{
	int unit, ret;

	if (md == NULL)
		return (ENXIO);
	if (lenr == NULL)
		return (EINVAL);

	*lenr = 0;
	unit = md->unit;

	if ((md->fflags & FREAD) == 0) {
		MIDI_DEBUG(printf("synth_readraw: unit %d is not for reading.\n", unit));
		return (EIO);
	}

	mtx_lock(&md->flagqueue_mtx);

	/* Begin recording. */
	if ((md->flags & MIDI_F_READING) == 0)
		md->callback(md, MIDI_CB_START | MIDI_CB_RD);

	if (nonblock) {
		/* Have we got enough data to read? */
		if (md->midi_dbuf_in.rl < len) {
			mtx_unlock(&md->flagqueue_mtx);
			return (EAGAIN);
		}
	}

	ret = midibuf_seqread(&md->midi_dbuf_in, buf, len, lenr,
			      md->callback, md, MIDI_CB_START | MIDI_CB_RD,
			      &md->flagqueue_mtx);

	mtx_unlock(&md->flagqueue_mtx);

	return (ret);
}

static int
synth_writeraw(mididev_info *md, u_char *buf, int len, int *lenw, int nonblock)
{
	int unit, ret;

	if (md == NULL)
		return (ENXIO);
	if (lenw == NULL)
		return (EINVAL);

	*lenw = 0;
	unit = md->unit;

	if ((md->fflags & FWRITE) == 0) {
		MIDI_DEBUG(printf("synth_writeraw: unit %d is not for writing.\n", unit));
		return (EIO);
	}

	/* For nonblocking, have we got enough space to write? */
	mtx_lock(&md->flagqueue_mtx);
	if (nonblock && md->midi_dbuf_out.fl < len) {
		/* Begin playing. */
		md->callback(md, MIDI_CB_START | MIDI_CB_WR);
		mtx_unlock(&md->flagqueue_mtx);
		return (EAGAIN);
	}

	ret = midibuf_seqwrite(&md->midi_dbuf_out, buf, len, lenw,
			       md->callback, md, MIDI_CB_START | MIDI_CB_WR,
			       &md->flagqueue_mtx);

	if (ret == 0)
		/* Begin playing. */
		md->callback(md, MIDI_CB_START | MIDI_CB_WR);

	mtx_unlock(&md->flagqueue_mtx);

	return (ret);
}

/*
 * The functions below here are the libraries for the above ones.
 */

static int
synth_leavesysex(mididev_info *md)
{
	int unit, lenw;
	synthdev_info *sd;
	u_char c;

	unit = md->unit;
	sd = &md->synth;

	mtx_lock(&sd->status_mtx);
	if (!sd->sysex_state) {
		mtx_unlock(&sd->status_mtx);
		return (0);
	}

	sd->sysex_state = 0;
	mtx_unlock(&sd->status_mtx);
	c = 0xf7;

	return (md->synth.writeraw(md, &c, sizeof(c), &lenw, 1));
}
