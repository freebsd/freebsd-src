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

typedef int (mix_set_t)(snd_mixer *m, unsigned dev, unsigned left, unsigned right);
typedef int (mix_recsrc_t)(snd_mixer *m, u_int32_t src);
typedef int (mix_init_t)(snd_mixer *m);

struct _snd_mixer {
	char name[64];
	mix_init_t *init;
	mix_set_t *set;
	mix_recsrc_t *setrecsrc;

	void *devinfo;
	u_int32_t devs;
	u_int32_t recdevs;
	u_int32_t recsrc;
	u_int16_t level[32];
};

/*
 * descriptor of a dma buffer. See dmabuf.c for documentation.
 * (rp,rl) and (fp,fl) identify the READY and FREE regions of the
 * buffer. dl contains the length used for dma transfer, dl>0 also
 * means that the channel is busy and there is a DMA transfer in progress.
 */

struct _snd_dbuf {
        u_int8_t *buf;
        int bufsize;
        volatile int dl; /* transfer size */
        volatile int rp, fp; /* pointers to the ready and free area */
	volatile int rl, fl; /* lenght of ready and free areas. */
	volatile int hp;
	volatile u_int32_t int_count, prev_int_count;
	volatile u_int32_t total, prev_total;
	int chan, dir;       /* dma channel */
	int fmt, blksz, blkcnt;
	int underflow, overrun;
	bus_dmamap_t dmamap;
	struct selinfo sel;
};

typedef int (pcmfeed_init_t)(pcm_feeder *feeder);
typedef int (pcmfeed_free_t)(pcm_feeder *feeder);
typedef int (pcmfeed_feed_t)(pcm_feeder *feeder, pcm_channel *c, u_int8_t *buffer,
			     u_int32_t count, struct uio *stream);

#define FEEDER_ROOT	1
#define FEEDER_FMT 	2
#define FEEDER_RATE 	3
#define FEEDER_FILTER 	4

struct pcm_feederdesc {
	u_int32_t type;
	u_int32_t in, out;
	u_int32_t flags;
	int idx;
};

#define MAXFEEDERS 	256

struct _pcm_feeder {
	char name[16];
	int align;
	struct pcm_feederdesc *desc;
	pcmfeed_init_t *init;
	pcmfeed_free_t *free;
	pcmfeed_feed_t *feed;
	void *data;
	pcm_feeder *source;
};

struct _pcmchan_caps {
	u_int32_t minspeed, maxspeed;
	u_int32_t *fmtlist;
	u_int32_t caps;
};

typedef void *(pcmchan_init_t)(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
typedef int (pcmchan_setdir_t)(void *data, int dir);
typedef int (pcmchan_setformat_t)(void *data, u_int32_t format);
typedef int (pcmchan_setspeed_t)(void *data, u_int32_t speed);
typedef int (pcmchan_setblocksize_t)(void *data, u_int32_t blocksize);
typedef int (pcmchan_trigger_t)(void *data, int go);
typedef int (pcmchan_getptr_t)(void *data);
typedef pcmchan_caps *(pcmchan_getcaps_t)(void *data);

struct _pcm_channel {
	pcmchan_init_t *init;
	pcmchan_setdir_t *setdir;
	pcmchan_setformat_t *setformat;
	pcmchan_setspeed_t *setspeed;
	pcmchan_setblocksize_t *setblocksize;
	pcmchan_trigger_t *trigger;
	pcmchan_getptr_t *getptr;
	pcmchan_getcaps_t *getcaps;
	pcm_feeder *feeder;
	struct pcm_feederdesc *feederdesc;
	u_int32_t align;

	int volume;
	u_int32_t speed;
	u_int32_t flags;
	u_int32_t format;
	u_int32_t blocks;

	int direction;
	snd_dbuf buffer, buffer2nd;
	snddev_info *parent;
	void *devinfo;
};

typedef void (pcm_swap_t)(void *data, int dir);
#define SND_STATUSLEN	64
/* descriptor of audio device */
struct _snddev_info {
	pcm_channel *play, *rec, **aplay, **arec, fakechan;
	int *ref;
	unsigned playcount, reccount, chancount;
	snd_mixer mixer;
	u_long magic;
	unsigned flags;
	void *devinfo;
	pcm_swap_t *swap;
	device_t dev;
	char status[SND_STATUSLEN];
};

/* mixer description structure and macros - these should go away,
 * only sb.[ch] and mss.[ch] use them
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

