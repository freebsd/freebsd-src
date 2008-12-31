/*-
 * Copyright (c) 1999 Cameron Grant <cg@freebsd.org>
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
 * $FreeBSD: src/sys/dev/sound/pcm/feeder.h,v 1.15.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

struct pcm_feederdesc {
	u_int32_t type;
	u_int32_t in, out;
	u_int32_t flags;
	int idx;
};

struct feeder_class {
	KOBJ_CLASS_FIELDS;
	int align;
	struct pcm_feederdesc *desc;
	void *data;
};

struct pcm_feeder {
    	KOBJ_FIELDS;
	int align;
	struct pcm_feederdesc *desc, desc_static;
	void *data;
	struct feeder_class *class;
	struct pcm_feeder *source, *parent;

};

void feeder_register(void *p);
struct feeder_class *feeder_getclass(struct pcm_feederdesc *desc);

u_int32_t chn_fmtscore(u_int32_t fmt);
u_int32_t chn_fmtbestbit(u_int32_t fmt, u_int32_t *fmts);
u_int32_t chn_fmtbeststereo(u_int32_t fmt, u_int32_t *fmts);
u_int32_t chn_fmtbest(u_int32_t fmt, u_int32_t *fmts);
u_int32_t chn_fmtchain(struct pcm_channel *c, u_int32_t *to);
int chn_addfeeder(struct pcm_channel *c, struct feeder_class *fc, struct pcm_feederdesc *desc);
int chn_removefeeder(struct pcm_channel *c);
struct pcm_feeder *chn_findfeeder(struct pcm_channel *c, u_int32_t type);
void feeder_printchain(struct pcm_feeder *head);

#define FEEDER_DECLARE(feeder, palign, pdata) \
static struct feeder_class feeder ## _class = { \
	.name =		#feeder, \
	.methods =	feeder ## _methods, \
	.size =		sizeof(struct pcm_feeder), \
	.align =	palign, \
	.desc =		feeder ## _desc, \
	.data =		pdata, \
}; \
SYSINIT(feeder, SI_SUB_DRIVERS, SI_ORDER_ANY, feeder_register, &feeder ## _class);

#define FEEDER_ROOT	0
#define FEEDER_FMT 	1
#define FEEDER_MIXER	2
#define FEEDER_RATE 	3
#define FEEDER_FILTER 	4
#define FEEDER_VOLUME 	5
#define FEEDER_SWAPLR 	6
#define FEEDER_LAST	32

#define FEEDRATE_SRC	1
#define FEEDRATE_DST	2

#define FEEDRATE_RATEMIN	1
#define FEEDRATE_RATEMAX	2016000	/* 48000 * 42 */

#define FEEDRATE_MIN		1
#define FEEDRATE_MAX		0x7fffff	/* sign 24bit ~ 8ghz ! */

#define FEEDRATE_ROUNDHZ	25
#define FEEDRATE_ROUNDHZ_MIN	0
#define FEEDRATE_ROUNDHZ_MAX	500

/*
 * Default buffer size for feeder processing.
 *
 * Big   = less sndbuf_feed(), more memory usage.
 * Small = aggresive sndbuf_feed() (perhaps too much), less memory usage.
 */
#define FEEDBUFSZ	16384
#define FEEDBUFSZ_MIN	2048
#define FEEDBUFSZ_MAX	131072

extern int feeder_rate_min;
extern int feeder_rate_max;
extern int feeder_rate_round;
extern int feeder_buffersize;
