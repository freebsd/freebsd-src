/*
 * include file for midi synthesizer interface.
 * 
 * Copyright by Seigo Tanimura 1999.
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
 *
 */

#define SYNTH_MAX_VOICES	32

/* This is the voice allocation state for a synthesizer. */
struct voice_alloc_info {
	int max_voice;
	int used_voices;
	int ptr;		/* For device specific use */
	u_short map[SYNTH_MAX_VOICES]; /* (ch << 8) | (note+1) */
	int timestamp;
	int alloc_times[SYNTH_MAX_VOICES];
};

/* This is the channel information for a synthesizer. */
struct channel_info {
	int pgm_num;
	int bender_value;
	u_char controllers[128];
};

/* These are the function types for a midi synthesizer interface. */
typedef int (mdsy_killnote_t)(mididev_info *md, int chn, int note, int vel);
typedef int (mdsy_setinstr_t)(mididev_info *md, int chn, int instr);
typedef int (mdsy_startnote_t)(mididev_info *md, int chn, int note, int vel);
typedef int (mdsy_reset_t)(mididev_info *md);
typedef int (mdsy_hwcontrol_t)(mididev_info *md, u_char *event);
typedef int (mdsy_loadpatch_t)(mididev_info *md, int format, struct uio *buf, int offs, int count, int pmgr_flag);
typedef int (mdsy_panning_t)(mididev_info *md, int chn, int pan);
typedef int (mdsy_aftertouch_t)(mididev_info *md, int chn, int press);
typedef int (mdsy_controller_t)(mididev_info *md, int chn, int ctrlnum, int val);
typedef int (mdsy_patchmgr_t)(mididev_info *md, struct patmgr_info *rec);
typedef int (mdsy_bender_t)(mididev_info *md, int chn, int val);
typedef int (mdsy_allocvoice_t)(mididev_info *md, int chn, int note, struct voice_alloc_info *alloc);
typedef int (mdsy_setupvoice_t)(mididev_info *md, int voice, int chn);
typedef int (mdsy_sendsysex_t)(mididev_info *md, u_char *sysex, int len);
typedef int (mdsy_prefixcmd_t)(mididev_info *md, int status);
typedef int (mdsy_volumemethod_t)(mididev_info *md, int mode);
typedef int (mdsy_readraw_t)(mididev_info *md, u_char *buf, int len, int *lenr, int nonblock);
typedef int (mdsy_writeraw_t)(mididev_info *md, u_char *buf, int len, int *lenw, int nonblock);

/*
 * The order of mutex lock (from the first to the last)
 *
 * 1. sequencer flags, queues, timer and devlice list
 * 2. midi synth voice and channel
 * 3. midi synth status
 * 4. generic midi flags and queues
 * 5. midi device
 */

/* This is a midi synthesizer interface and state. */
struct _synthdev_info {
	mdsy_killnote_t *killnote;
	mdsy_setinstr_t *setinstr;
	mdsy_startnote_t *startnote;
	mdsy_reset_t *reset;
	mdsy_hwcontrol_t *hwcontrol;
	mdsy_loadpatch_t *loadpatch;
	mdsy_panning_t *panning;
	mdsy_aftertouch_t *aftertouch;
	mdsy_controller_t *controller;
	mdsy_patchmgr_t *patchmgr;
	mdsy_bender_t *bender;
	mdsy_allocvoice_t *allocvoice;
	mdsy_setupvoice_t *setupvoice;
	mdsy_sendsysex_t *sendsysex;
	mdsy_prefixcmd_t *prefixcmd;
	mdsy_volumemethod_t *volumemethod;
	mdsy_readraw_t *readraw;
	mdsy_writeraw_t *writeraw;

	/* Voice and channel */
	struct mtx vc_mtx; /* Mutex to protect voice and channel. */
	struct voice_alloc_info alloc; /* Voice allocation. */
	struct channel_info chn_info[16]; /* Channel information. */

	/* Status */
	struct mtx status_mtx; /* Mutex to protect status. */
	int sysex_state; /* State of sysex transmission. */
};
typedef struct _synthdev_info synthdev_info;
