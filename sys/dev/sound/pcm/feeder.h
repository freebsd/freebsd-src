/*
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
 * $FreeBSD$
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
SYSINIT(feeder, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, feeder_register, &feeder ## _class);

#define FEEDER_ROOT	1
#define FEEDER_FMT 	2
#define	FEEDER_MIXER	3
#define FEEDER_RATE 	4
#define FEEDER_FILTER 	5
#define FEEDER_VOLUME 	6
#define FEEDER_LAST	FEEDER_VOLUME

#define FEEDRATE_SRC	1
#define FEEDRATE_DST	2


