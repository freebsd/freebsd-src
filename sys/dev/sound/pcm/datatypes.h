/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
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
 *
 * $FreeBSD$
 */

typedef struct _snd_mixer snd_mixer;
typedef struct _snd_dbuf snd_dbuf;
typedef struct _snddev_info snddev_info;
typedef struct _pcmchan_caps pcmchan_caps;
typedef struct _pcm_feeder pcm_feeder;
typedef struct _pcm_channel pcm_channel;

/*****************************************************************************/

struct _snd_mixer {
	KOBJ_FIELDS;
	const char *name;
	void *devinfo;
	int busy;
	int hwvol_muted;
	int hwvol_mixer;
	int hwvol_step;
	u_int32_t hwvol_mute_level;
	u_int32_t devs;
	u_int32_t recdevs;
	u_int32_t recsrc;
	u_int16_t level[32];
};

/*****************************************************************************/

/*
 * descriptor of a dma buffer. See dmabuf.c for documentation.
 * (rp,rl) and (fp,fl) identify the READY and FREE regions of the
 * buffer. dl contains the length used for dma transfer, dl>0 also
 * means that the channel is busy and there is a DMA transfer in progress.
 */

struct _snd_dbuf {
        u_int8_t *buf;
        int bufsize, maxsize;
        volatile int dl; /* transfer size */
        volatile int rp, fp; /* pointers to the ready and free area */
	volatile int rl, fl; /* lenght of ready and free areas. */
	volatile int hp;
	volatile u_int32_t int_count, prev_int_count;
	volatile u_int32_t total, prev_total;
	int chan, dir;       /* dma channel */
	int fmt, spd, bps;
	int blksz, blkcnt;
	int underflow, overrun;
	u_int32_t flags;
	bus_dmamap_t dmamap;
	bus_dma_tag_t dmatag;
	struct selinfo sel;
};
#define	SNDBUF_F_ISADMA		0x00000001

/*****************************************************************************/

struct pcm_feederdesc {
	u_int32_t type;
	u_int32_t in, out;
	u_int32_t flags;
	int idx;
};

struct _pcm_feeder {
    	KOBJ_FIELDS;
	int align;
	struct pcm_feederdesc *desc;
	void *data;
	pcm_feeder *source;
};

/*****************************************************************************/

struct _pcmchan_caps {
	u_int32_t minspeed, maxspeed;
	u_int32_t *fmtlist;
	u_int32_t caps;
};

struct _pcm_channel {
	kobj_t methods;

	pcm_feeder *feeder;
	u_int32_t align;

	int volume;
	u_int32_t speed;
	u_int32_t format;
	u_int32_t flags;
	u_int32_t feederflags;
	u_int32_t blocks;

	int direction;
	snd_dbuf buffer, bufsoft;
	snddev_info *parent;
	void *devinfo;
};

/*****************************************************************************/

#define SND_STATUSLEN	64
/* descriptor of audio device */
struct _snddev_info {
	pcm_channel *play, *rec, **aplay, **arec, fakechan;
	int *ref, *atype;
	unsigned playcount, reccount, chancount, maxchans;
	snd_mixer *mixer;
	u_long magic;
	unsigned flags;
	void *devinfo;
	device_t dev;
	char status[SND_STATUSLEN];
#ifdef SND_DYNSYSCTL
	struct sysctl_ctx_list sysctl_tree;
	struct sysctl_oid *sysctl_tree_top;
#endif
};

/*****************************************************************************/

/* mixer description structure and macros - these should go away,
 * only mss.[ch] use them
 */
struct mixer_def {
    	u_int regno:7;
    	u_int polarity:1;	/* 1 means reversed */
    	u_int bitoffs:4;
    	u_int nbits:4;
};
typedef struct mixer_def mixer_ent;
typedef struct mixer_def mixer_tab[32][2];

#define MIX_ENT(name, reg_l, pol_l, pos_l, len_l, reg_r, pol_r, pos_r, len_r) \
    	{{reg_l, pol_l, pos_l, len_l}, {reg_r, pol_r, pos_r, len_r}}

#define PMIX_ENT(name, reg_l, pos_l, len_l, reg_r, pos_r, len_r) \
    	{{reg_l, 0, pos_l, len_l}, {reg_r, 0, pos_r, len_r}}

#define MIX_NONE(name) MIX_ENT(name, 0,0,0,0, 0,0,0,0)

