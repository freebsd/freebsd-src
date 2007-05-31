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
 */

#include <dev/sound/pcm/sound.h>

#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

MALLOC_DEFINE(M_FEEDER, "feeder", "pcm feeder");

#define MAXFEEDERS 	256
#undef FEEDER_DEBUG

int feeder_buffersize = FEEDBUFSZ;
TUNABLE_INT("hw.snd.feeder_buffersize", &feeder_buffersize);

#ifdef SND_DEBUG
static int
sysctl_hw_snd_feeder_buffersize(SYSCTL_HANDLER_ARGS)
{
	int i, err, val;

	val = feeder_buffersize;
	err = sysctl_handle_int(oidp, &val, sizeof(val), req);

	if (err != 0 || req->newptr == NULL)
		return err;

	if (val < FEEDBUFSZ_MIN || val > FEEDBUFSZ_MAX)
		return EINVAL;

	i = 0;
	while (val >> i)
		i++;
	i = 1 << i;
	if (i > val && (i >> 1) > 0 && (i >> 1) >= ((val * 3) >> 2))
		i >>= 1;

	feeder_buffersize = i;

	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, feeder_buffersize, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_feeder_buffersize, "I",
	"feeder buffer size");
#else
SYSCTL_INT(_hw_snd, OID_AUTO, feeder_buffersize, CTLFLAG_RD,
	&feeder_buffersize, FEEDBUFSZ, "feeder buffer size");
#endif

struct feedertab_entry {
	SLIST_ENTRY(feedertab_entry) link;
	struct feeder_class *feederclass;
	struct pcm_feederdesc *desc;

	int idx;
};
static SLIST_HEAD(, feedertab_entry) feedertab;

/*****************************************************************************/

void
feeder_register(void *p)
{
	static int feedercnt = 0;

	struct feeder_class *fc = p;
	struct feedertab_entry *fte;
	int i;

	if (feedercnt == 0) {
		KASSERT(fc->desc == NULL, ("first feeder not root: %s", fc->name));

		SLIST_INIT(&feedertab);
		fte = malloc(sizeof(*fte), M_FEEDER, M_NOWAIT | M_ZERO);
		if (fte == NULL) {
			printf("can't allocate memory for root feeder: %s\n",
			    fc->name);

			return;
		}
		fte->feederclass = fc;
		fte->desc = NULL;
		fte->idx = feedercnt;
		SLIST_INSERT_HEAD(&feedertab, fte, link);
		feedercnt++;

		/* initialize global variables */

		if (snd_verbose < 0 || snd_verbose > 4)
			snd_verbose = 1;

		/* initialize unit numbering */
		snd_unit_init();
		if (snd_unit < 0 || snd_unit > PCMMAXUNIT)
			snd_unit = 0;
		
		if (snd_maxautovchans < 0 ||
		    snd_maxautovchans > SND_MAXVCHANS)
			snd_maxautovchans = 0;

		if (chn_latency < CHN_LATENCY_MIN ||
		    chn_latency > CHN_LATENCY_MAX)
			chn_latency = CHN_LATENCY_DEFAULT;

		if (chn_latency_profile < CHN_LATENCY_PROFILE_MIN ||
		    chn_latency_profile > CHN_LATENCY_PROFILE_MAX)
			chn_latency_profile = CHN_LATENCY_PROFILE_DEFAULT;

		if (feeder_buffersize < FEEDBUFSZ_MIN ||
		    	    feeder_buffersize > FEEDBUFSZ_MAX)
			feeder_buffersize = FEEDBUFSZ;

		if (feeder_rate_min < FEEDRATE_MIN ||
			    feeder_rate_max < FEEDRATE_MIN ||
			    feeder_rate_min > FEEDRATE_MAX ||
			    feeder_rate_max > FEEDRATE_MAX ||
			    !(feeder_rate_min < feeder_rate_max)) {
			feeder_rate_min = FEEDRATE_RATEMIN;
			feeder_rate_max = FEEDRATE_RATEMAX;
		}

		if (feeder_rate_round < FEEDRATE_ROUNDHZ_MIN ||
		    	    feeder_rate_round > FEEDRATE_ROUNDHZ_MAX)
			feeder_rate_round = FEEDRATE_ROUNDHZ;

		if (bootverbose)
			printf("%s: snd_unit=%d snd_maxautovchans=%d "
			    "latency=%d feeder_buffersize=%d "
			    "feeder_rate_min=%d feeder_rate_max=%d "
			    "feeder_rate_round=%d\n",
			    __func__, snd_unit, snd_maxautovchans,
			    chn_latency, feeder_buffersize,
			    feeder_rate_min, feeder_rate_max,
			    feeder_rate_round);

		/* we've got our root feeder so don't veto pcm loading anymore */
		pcm_veto_load = 0;

		return;
	}

	KASSERT(fc->desc != NULL, ("feeder '%s' has no descriptor", fc->name));

	/* beyond this point failure is non-fatal but may result in some translations being unavailable */
	i = 0;
	while ((feedercnt < MAXFEEDERS) && (fc->desc[i].type > 0)) {
		/* printf("adding feeder %s, %x -> %x\n", fc->name, fc->desc[i].in, fc->desc[i].out); */
		fte = malloc(sizeof(*fte), M_FEEDER, M_NOWAIT | M_ZERO);
		if (fte == NULL) {
			printf("can't allocate memory for feeder '%s', %x -> %x\n", fc->name, fc->desc[i].in, fc->desc[i].out);

			return;
		}
		fte->feederclass = fc;
		fte->desc = &fc->desc[i];
		fte->idx = feedercnt;
		fte->desc->idx = feedercnt;
		SLIST_INSERT_HEAD(&feedertab, fte, link);
		i++;
	}
	feedercnt++;
	if (feedercnt >= MAXFEEDERS)
		printf("MAXFEEDERS (%d >= %d) exceeded\n", feedercnt, MAXFEEDERS);
}

static void
feeder_unregisterall(void *p)
{
	struct feedertab_entry *fte, *next;

	next = SLIST_FIRST(&feedertab);
	while (next != NULL) {
		fte = next;
		next = SLIST_NEXT(fte, link);
		free(fte, M_FEEDER);
	}
}

static int
cmpdesc(struct pcm_feederdesc *n, struct pcm_feederdesc *m)
{
	return ((n->type == m->type) &&
		((n->in == 0) || (n->in == m->in)) &&
		((n->out == 0) || (n->out == m->out)) &&
		(n->flags == m->flags));
}

static void
feeder_destroy(struct pcm_feeder *f)
{
	FEEDER_FREE(f);
	kobj_delete((kobj_t)f, M_FEEDER);
}

static struct pcm_feeder *
feeder_create(struct feeder_class *fc, struct pcm_feederdesc *desc)
{
	struct pcm_feeder *f;
	int err;

	f = (struct pcm_feeder *)kobj_create((kobj_class_t)fc, M_FEEDER, M_NOWAIT | M_ZERO);
	if (f == NULL)
		return NULL;

	f->align = fc->align;
	f->data = fc->data;
	f->source = NULL;
	f->parent = NULL;
	f->class = fc;
	f->desc = &(f->desc_static);

	if (desc) {
		*(f->desc) = *desc;
	} else {
		f->desc->type = FEEDER_ROOT;
		f->desc->in = 0;
		f->desc->out = 0;
		f->desc->flags = 0;
		f->desc->idx = 0;
	}

	err = FEEDER_INIT(f);
	if (err) {
		printf("feeder_init(%p) on %s returned %d\n", f, fc->name, err);
		feeder_destroy(f);

		return NULL;
	}

	return f;
}

struct feeder_class *
feeder_getclass(struct pcm_feederdesc *desc)
{
	struct feedertab_entry *fte;

	SLIST_FOREACH(fte, &feedertab, link) {
		if ((desc == NULL) && (fte->desc == NULL))
			return fte->feederclass;
		if ((fte->desc != NULL) && (desc != NULL) && cmpdesc(desc, fte->desc))
			return fte->feederclass;
	}
	return NULL;
}

int
chn_addfeeder(struct pcm_channel *c, struct feeder_class *fc, struct pcm_feederdesc *desc)
{
	struct pcm_feeder *nf;

	nf = feeder_create(fc, desc);
	if (nf == NULL)
		return ENOSPC;

	nf->source = c->feeder;

	/* XXX we should use the lowest common denominator for align */
	if (nf->align > 0)
		c->align += nf->align;
	else if (nf->align < 0 && c->align < -nf->align)
		c->align = -nf->align;
	if (c->feeder != NULL)
		c->feeder->parent = nf;
	c->feeder = nf;

	return 0;
}

int
chn_removefeeder(struct pcm_channel *c)
{
	struct pcm_feeder *f;

	if (c->feeder == NULL)
		return -1;
	f = c->feeder;
	c->feeder = c->feeder->source;
	feeder_destroy(f);

	return 0;
}

struct pcm_feeder *
chn_findfeeder(struct pcm_channel *c, u_int32_t type)
{
	struct pcm_feeder *f;

	f = c->feeder;
	while (f != NULL) {
		if (f->desc->type == type)
			return f;
		f = f->source;
	}

	return NULL;
}

static int
chainok(struct pcm_feeder *test, struct pcm_feeder *stop)
{
	u_int32_t visited[MAXFEEDERS / 32];
	u_int32_t idx, mask;

	bzero(visited, sizeof(visited));
	while (test && (test != stop)) {
		idx = test->desc->idx;
		if (idx < 0)
			panic("bad idx %d", idx);
		if (idx >= MAXFEEDERS)
			panic("bad idx %d", idx);
		mask = 1 << (idx & 31);
		idx >>= 5;
		if (visited[idx] & mask)
			return 0;
		visited[idx] |= mask;
		test = test->source;
	}

	return 1;
}

/*
 * See feeder_fmtchain() for the mumbo-jumbo ridiculous explanation
 * of what the heck is this FMT_Q_*
 */
#define FMT_Q_UP	1
#define FMT_Q_DOWN	2
#define FMT_Q_EQ	3
#define FMT_Q_MULTI	4

/*
 * 14bit format scoring
 * --------------------
 *
 *  13  12  11  10   9   8        2        1   0    offset
 * +---+---+---+---+---+---+-------------+---+---+
 * | X | X | X | X | X | X | X X X X X X | X | X |
 * +---+---+---+---+---+---+-------------+---+---+
 *   |   |   |   |   |   |        |        |   |
 *   |   |   |   |   |   |        |        |   +--> signed?
 *   |   |   |   |   |   |        |        |
 *   |   |   |   |   |   |        |        +------> bigendian?
 *   |   |   |   |   |   |        |
 *   |   |   |   |   |   |        +---------------> total channels
 *   |   |   |   |   |   |
 *   |   |   |   |   |   +------------------------> AFMT_A_LAW
 *   |   |   |   |   |
 *   |   |   |   |   +----------------------------> AFMT_MU_LAW
 *   |   |   |   |
 *   |   |   |   +--------------------------------> AFMT_8BIT
 *   |   |   |
 *   |   |   +------------------------------------> AFMT_16BIT
 *   |   |
 *   |   +----------------------------------------> AFMT_24BIT
 *   |
 *   +--------------------------------------------> AFMT_32BIT
 */
#define score_signeq(s1, s2)	(((s1) & 0x1) == ((s2) & 0x1))
#define score_endianeq(s1, s2)	(((s1) & 0x2) == ((s2) & 0x2))
#define score_cheq(s1, s2)	(((s1) & 0xfc) == ((s2) & 0xfc))
#define score_val(s1)		((s1) & 0x3f00)
#define score_cse(s1)		((s1) & 0x7f)

u_int32_t
chn_fmtscore(u_int32_t fmt)
{
	u_int32_t ret;

	ret = 0;
	if (fmt & AFMT_SIGNED)
		ret |= 1 << 0;
	if (fmt & AFMT_BIGENDIAN)
		ret |= 1 << 1;
	if (fmt & AFMT_STEREO)
		ret |= (2 & 0x3f) << 2;
	else
		ret |= (1 & 0x3f) << 2;
	if (fmt & AFMT_A_LAW)
		ret |= 1 << 8;
	else if (fmt & AFMT_MU_LAW)
		ret |= 1 << 9;
	else if (fmt & AFMT_8BIT)
		ret |= 1 << 10;
	else if (fmt & AFMT_16BIT)
		ret |= 1 << 11;
	else if (fmt & AFMT_24BIT)
		ret |= 1 << 12;
	else if (fmt & AFMT_32BIT)
		ret |= 1 << 13;

	return ret;
}

static u_int32_t
chn_fmtbestfunc(u_int32_t fmt, u_int32_t *fmts, int cheq)
{
	u_int32_t best, score, score2, oldscore;
	int i;

	if (fmt == 0 || fmts == NULL || fmts[0] == 0)
		return 0;

	if (fmtvalid(fmt, fmts))
		return fmt;

	best = 0;
	score = chn_fmtscore(fmt);
	oldscore = 0;
	for (i = 0; fmts[i] != 0; i++) {
		score2 = chn_fmtscore(fmts[i]);
		if (cheq && !score_cheq(score, score2))
			continue;
		if (oldscore == 0 ||
			    (score_val(score2) == score_val(score)) ||
			    (score_val(score2) == score_val(oldscore)) ||
			    (score_val(score2) > score_val(oldscore) &&
			    score_val(score2) < score_val(score)) ||
			    (score_val(score2) < score_val(oldscore) &&
			    score_val(score2) > score_val(score)) ||
			    (score_val(oldscore) < score_val(score) &&
			    score_val(score2) > score_val(oldscore))) {
			if (score_val(oldscore) != score_val(score2) ||
				    score_cse(score) == score_cse(score2) ||
				    ((score_cse(oldscore) != score_cse(score) &&
				    !score_endianeq(score, oldscore) &&
				    (score_endianeq(score, score2) ||
				    (!score_signeq(score, oldscore) &&
				    score_signeq(score, score2)))))) {
				best = fmts[i];
				oldscore = score2;
			}
		}
	}
	return best;
}

u_int32_t
chn_fmtbestbit(u_int32_t fmt, u_int32_t *fmts)
{
	return chn_fmtbestfunc(fmt, fmts, 0);
}

u_int32_t
chn_fmtbeststereo(u_int32_t fmt, u_int32_t *fmts)
{
	return chn_fmtbestfunc(fmt, fmts, 1);
}

u_int32_t
chn_fmtbest(u_int32_t fmt, u_int32_t *fmts)
{
	u_int32_t best1, best2;
	u_int32_t score, score1, score2;

	if (fmtvalid(fmt, fmts))
		return fmt;

	best1 = chn_fmtbeststereo(fmt, fmts);
	best2 = chn_fmtbestbit(fmt, fmts);

	if (best1 != 0 && best2 != 0 && best1 != best2) {
		if (fmt & AFMT_STEREO)
			return best1;
		else {
			score = score_val(chn_fmtscore(fmt));
			score1 = score_val(chn_fmtscore(best1));
			score2 = score_val(chn_fmtscore(best2));
			if (score1 == score2 || score1 == score)
				return best1;
			else if (score2 == score)
				return best2;
			else if (score1 > score2)
				return best1;
			return best2;
		}
	} else if (best2 == 0)
		return best1;
	else
		return best2;
}

static struct pcm_feeder *
feeder_fmtchain(u_int32_t *to, struct pcm_feeder *source, struct pcm_feeder *stop, int maxdepth)
{
	struct feedertab_entry *fte, *ftebest;
	struct pcm_feeder *try, *ret;
	uint32_t fl, qout, qsrc, qdst;
	int qtype;

	if (to == NULL || to[0] == 0)
		return NULL;

	DEB(printf("trying %s (0x%08x -> 0x%08x)...\n", source->class->name, source->desc->in, source->desc->out));
	if (fmtvalid(source->desc->out, to)) {
		DEB(printf("got it\n"));
		return source;
	}

	if (maxdepth < 0)
		return NULL;

	/*
	 * WARNING: THIS IS _NOT_ FOR THE FAINT HEART
	 * Disclaimer: I don't expect anybody could understand this
	 *             without deep logical and mathematical analysis
	 *             involving various unnamed probability theorem.
	 *
	 * This "Best Fit Random Chain Selection" (BLEHBLEHWHATEVER) algorithm
	 * is **extremely** difficult to digest especially when applied to
	 * large sets / numbers of random chains (feeders), each with
	 * unique characteristic providing different sets of in/out format.
	 *
	 * Basically, our FEEDER_FMT (see feeder_fmt.c) chains characteristic:
	 * 1) Format chains
	 *    1.1 "8bit to any, not to 8bit"
	 *      1.1.1 sign can remain consistent, e.g: u8 -> u16[le|be]
	 *      1.1.2 sign can be changed, e.g: u8 -> s16[le|be]
	 *      1.1.3 endian can be changed, e.g: u8 -> u16[le|be]
	 *      1.1.4 both can be changed, e.g: u8 -> [u|s]16[le|be]
	 *    1.2 "Any to 8bit, not from 8bit"
	 *      1.2.1 sign can remain consistent, e.g: s16le -> s8
	 *      1.2.2 sign can be changed, e.g: s16le -> u8
	 *      1.2.3 source endian can be anything e.g: s16[le|be] -> s8
	 *      1.2.4 source endian / sign can be anything e.g: [u|s]16[le|be] -> u8
	 *    1.3 "Any to any where BOTH input and output either 8bit or non-8bit"
	 *      1.3.1 endian MUST remain consistent
	 *      1.3.2 sign CAN be changed
	 *    1.4 "Long jump" is allowed, e.g: from 16bit to 32bit, excluding
	 *        16bit to 24bit .
	 * 2) Channel chains (mono <-> stereo)
	 *    2.1 Both endian and sign MUST remain consistent
	 * 3) Endian chains (big endian <-> little endian)
	 *    3.1 Channels and sign MUST remain consistent
	 * 4) Sign chains (signed <-> unsigned)
	 *    4.1 Channels and endian MUST remain consistent
	 *
	 * .. and the mother of all chaining rules:
	 *
	 * Rules 0: Source and destination MUST not contain multiple selections.
	 *          (qtype != FMT_Q_MULTI)
	 *
	 * First of all, our caller ( chn_fmtchain() ) will reduce the possible
	 * multiple from/to formats to a single best format using chn_fmtbest().
	 * Then, using chn_fmtscore(), we determine the chaining characteristic.
	 * Our main goal is to narrow it down until it reach FMT_Q_EQ chaining
	 * type while still adhering above chaining rules.
	 *
	 * The need for this complicated chaining procedures is inevitable,
	 * since currently we have more than 200 different types of FEEDER_FMT
	 * doing various unique format conversion. Without this (the old way),
	 * it is possible to generate broken chain since it doesn't do any
	 * sanity checking to ensure that the output format is "properly aligned"
	 * with the direction of conversion (quality up/down/equal).
	 *
	 *   Conversion: s24le to s32le
	 *   Possible chain: 1) s24le -> s32le (correct, optimized)
	 *                   2) s24le -> s16le -> s32le
	 *                      (since we have feeder_24to16 and feeder_16to32)
	 *                      +-- obviously broken!
	 *
	 * Using scoring mechanisme, this will ensure that the chaining
	 * process do the right thing, or at least, give the best chain
	 * possible without causing quality (the 'Q') degradation.
	 */

	qdst = chn_fmtscore(to[0]);
	qsrc = chn_fmtscore(source->desc->out);

#define score_q(s1)			score_val(s1)
#define score_8bit(s1)			((s1) & 0x700)
#define score_non8bit(s1)		(!score_8bit(s1))
#define score_across8bit(s1, s2)	((score_8bit(s1) && score_non8bit(s2)) || \
					(score_8bit(s2) && score_non8bit(s1)))

#define FMT_CHAIN_Q_UP(s1, s2)		(score_q(s1) < score_q(s2))
#define FMT_CHAIN_Q_DOWN(s1, s2)	(score_q(s1) > score_q(s2))
#define FMT_CHAIN_Q_EQ(s1, s2)		(score_q(s1) == score_q(s2))
#define FMT_Q_DOWN_FLAGS(s1, s2)	(0x1 | (score_across8bit(s1, s2) ? \
						0x2 : 0x0))
#define FMT_Q_UP_FLAGS(s1, s2)		FMT_Q_DOWN_FLAGS(s1, s2)
#define FMT_Q_EQ_FLAGS(s1, s2)		(0x3ffc | \
					((score_cheq(s1, s2) && \
						score_endianeq(s1, s2)) ? \
						0x1 : 0x0) | \
					((score_cheq(s1, s2) && \
						score_signeq(s1, s2)) ? \
						0x2 : 0x0))

	/* Determine chaining direction and set matching flag */
	fl = 0x3fff;
	if (to[1] != 0) {
		qtype = FMT_Q_MULTI;
		printf("%s: WARNING: FMT_Q_MULTI chaining. Expect the unexpected.\n", __func__);
	} else if (FMT_CHAIN_Q_DOWN(qsrc, qdst)) {
		qtype = FMT_Q_DOWN;
		fl = FMT_Q_DOWN_FLAGS(qsrc, qdst);
	} else if (FMT_CHAIN_Q_UP(qsrc, qdst)) {
		qtype = FMT_Q_UP;
		fl = FMT_Q_UP_FLAGS(qsrc, qdst);
	} else {
		qtype = FMT_Q_EQ;
		fl = FMT_Q_EQ_FLAGS(qsrc, qdst);
	}

	ftebest = NULL;

	SLIST_FOREACH(fte, &feedertab, link) {
		if (fte->desc == NULL)
			continue;
		if (fte->desc->type != FEEDER_FMT)
			continue;
		qout = chn_fmtscore(fte->desc->out);
#define FMT_Q_MULTI_VALIDATE(qt)		((qt) == FMT_Q_MULTI)
#define FMT_Q_FL_MATCH(qfl, s1, s2)		(((s1) & (qfl)) == ((s2) & (qfl)))
#define FMT_Q_UP_VALIDATE(qt, s1, s2, s3)	((qt) == FMT_Q_UP && \
						score_q(s3) >= score_q(s1) && \
						score_q(s3) <= score_q(s2))
#define FMT_Q_DOWN_VALIDATE(qt, s1, s2, s3)	((qt) == FMT_Q_DOWN && \
						score_q(s3) <= score_q(s1) && \
						score_q(s3) >= score_q(s2))
#define FMT_Q_EQ_VALIDATE(qt, s1, s2)		((qt) == FMT_Q_EQ && \
						score_q(s1) == score_q(s2))
		if (fte->desc->in == source->desc->out &&
			    (FMT_Q_MULTI_VALIDATE(qtype) ||
			    (FMT_Q_FL_MATCH(fl, qout, qdst) &&
			    (FMT_Q_UP_VALIDATE(qtype, qsrc, qdst, qout) ||
			    FMT_Q_DOWN_VALIDATE(qtype, qsrc, qdst, qout) ||
			    FMT_Q_EQ_VALIDATE(qtype, qdst, qout))))) {
			try = feeder_create(fte->feederclass, fte->desc);
			if (try) {
				try->source = source;
				ret = chainok(try, stop) ? feeder_fmtchain(to, try, stop, maxdepth - 1) : NULL;
				if (ret != NULL)
					return ret;
				feeder_destroy(try);
			}
		} else if (fte->desc->in == source->desc->out) {
			/* XXX quality must be considered! */
			if (ftebest == NULL)
				ftebest = fte;
		}
	}

	if (ftebest != NULL) {
		try = feeder_create(ftebest->feederclass, ftebest->desc);
		if (try) {
			try->source = source;
			ret = chainok(try, stop) ? feeder_fmtchain(to, try, stop, maxdepth - 1) : NULL;
			if (ret != NULL)
				return ret;
			feeder_destroy(try);
		}
	}

	/* printf("giving up %s...\n", source->class->name); */

	return NULL;
}

u_int32_t
chn_fmtchain(struct pcm_channel *c, u_int32_t *to)
{
	struct pcm_feeder *try, *del, *stop;
	u_int32_t tmpfrom[2], tmpto[2], best, *from;
	int i, max, bestmax;

	KASSERT(c != NULL, ("c == NULL"));
	KASSERT(c->feeder != NULL, ("c->feeder == NULL"));
	KASSERT(to != NULL, ("to == NULL"));
	KASSERT(to[0] != 0, ("to[0] == 0"));

	if (c == NULL || c->feeder == NULL || to == NULL || to[0] == 0)
		return 0;

	stop = c->feeder;
	best = 0;

	if (c->direction == PCMDIR_REC && c->feeder->desc->type == FEEDER_ROOT) {
		from = chn_getcaps(c)->fmtlist;
		if (from[1] != 0) {
			best = chn_fmtbest(to[0], from);
			if (best != 0) {
				tmpfrom[0] = best;
				tmpfrom[1] = 0;
				from = tmpfrom;
			}
		}
	} else {
		tmpfrom[0] = c->feeder->desc->out;
		tmpfrom[1] = 0;
		from = tmpfrom;
		if (to[1] != 0) {
			best = chn_fmtbest(from[0], to);
			if (best != 0) {
				tmpto[0] = best;
				tmpto[1] = 0;
				to = tmpto;
			}
		}
	}

#define FEEDER_FMTCHAIN_MAXDEPTH	8

	try = NULL;

	if (to[0] != 0 && from[0] != 0 &&
		    to[1] == 0 && from[1] == 0) {
		max = 0;
		best = from[0];
		c->feeder->desc->out = best;
		do {
			try = feeder_fmtchain(to, c->feeder, stop, max);
			DEB(if (try != NULL) {
				printf("%s: 0x%08x -> 0x%08x (maxdepth: %d)\n",
					__func__, from[0], to[0], max);
			});
		} while (try == NULL && max++ < FEEDER_FMTCHAIN_MAXDEPTH);
	} else {
		printf("%s: Using the old-way format chaining!\n", __func__);
		i = 0;
		best = 0;
		bestmax = 100;
		while (from[i] != 0) {
			c->feeder->desc->out = from[i];
			try = NULL;
			max = 0;
			do {
				try = feeder_fmtchain(to, c->feeder, stop, max);
			} while (try == NULL && max++ < FEEDER_FMTCHAIN_MAXDEPTH);
			if (try != NULL && max < bestmax) {
				bestmax = max;
				best = from[i];
			}
			while (try != NULL && try != stop) {
				del = try;
				try = try->source;
				feeder_destroy(del);
			}
			i++;
		}
		if (best == 0)
			return 0;

		c->feeder->desc->out = best;
		try = feeder_fmtchain(to, c->feeder, stop, bestmax);
	}
	if (try == NULL)
		return 0;

	c->feeder = try;
	c->align = 0;
#ifdef FEEDER_DEBUG
	printf("\n\nchain: ");
#endif
	while (try && (try != stop)) {
#ifdef FEEDER_DEBUG
		printf("%s [%d]", try->class->name, try->desc->idx);
		if (try->source)
			printf(" -> ");
#endif
		if (try->source)
			try->source->parent = try;
		if (try->align > 0)
			c->align += try->align;
		else if (try->align < 0 && c->align < -try->align)
			c->align = -try->align;
		try = try->source;
	}
#ifdef FEEDER_DEBUG
	printf("%s [%d]\n", try->class->name, try->desc->idx);
#endif

	if (c->direction == PCMDIR_REC) {
		try = c->feeder;
		while (try != NULL) {
			if (try->desc->type == FEEDER_ROOT)
				return try->desc->out;
			try = try->source;
		}
		return best;
	} else
		return c->feeder->desc->out;
}

void
feeder_printchain(struct pcm_feeder *head)
{
	struct pcm_feeder *f;

	printf("feeder chain (head @%p)\n", head);
	f = head;
	while (f != NULL) {
		printf("%s/%d @ %p\n", f->class->name, f->desc->idx, f);
		f = f->source;
	}
	printf("[end]\n\n");
}

/*****************************************************************************/

static int
feed_root(struct pcm_feeder *feeder, struct pcm_channel *ch, u_int8_t *buffer, u_int32_t count, void *source)
{
	struct snd_dbuf *src = source;
	int l, offset;

	KASSERT(count > 0, ("feed_root: count == 0"));
	/* count &= ~((1 << ch->align) - 1); */
	KASSERT(count > 0, ("feed_root: aligned count == 0 (align = %d)", ch->align));

	if (++ch->feedcount == 0)
		ch->feedcount = 2;

	l = min(count, sndbuf_getready(src));

	/* When recording only return as much data as available */
	if (ch->direction == PCMDIR_REC) {
		sndbuf_dispose(src, buffer, l);
		return l;
	}


	offset = count - l;

	if (offset > 0) {
		if (snd_verbose > 3)
			printf("%s: (%s) %spending %d bytes "
			    "(count=%d l=%d feed=%d)\n",
			    __func__,
			    (ch->flags & CHN_F_VIRTUAL) ? "virtual" : "hardware",
			    (ch->feedcount == 1) ? "pre" : "ap",
			    offset, count, l, ch->feedcount);

		if (ch->feedcount == 1) {
			memset(buffer,
			    sndbuf_zerodata(sndbuf_getfmt(src)),
			    offset);
			if (l > 0)
				sndbuf_dispose(src, buffer + offset, l);
			else
				ch->feedcount--;
		} else {
			if (l > 0)
				sndbuf_dispose(src, buffer, l);
#if 1
			memset(buffer + l,
			    sndbuf_zerodata(sndbuf_getfmt(src)),
			    offset);
			if (!(ch->flags & CHN_F_CLOSING))
				ch->xruns++;
#else
			if (l < 1 || (ch->flags & CHN_F_CLOSING)) {
				memset(buffer + l,
				    sndbuf_zerodata(sndbuf_getfmt(src)),
				    offset);
				if (!(ch->flags & CHN_F_CLOSING))
					ch->xruns++;
			} else {
				int cp, tgt;

				tgt = l;
				while (offset > 0) {
					cp = min(l, offset);
					memcpy(buffer + tgt, buffer, cp);
					offset -= cp;
					tgt += cp;
				}
				ch->xruns++;
			}
#endif
		}
	} else if (l > 0)
		sndbuf_dispose(src, buffer, l);

	return count;
}

static kobj_method_t feeder_root_methods[] = {
    	KOBJMETHOD(feeder_feed,		feed_root),
	{ 0, 0 }
};
static struct feeder_class feeder_root_class = {
	.name =		"feeder_root",
	.methods =	feeder_root_methods,
	.size =		sizeof(struct pcm_feeder),
	.align =	0,
	.desc =		NULL,
	.data =		NULL,
};
SYSINIT(feeder_root, SI_SUB_DRIVERS, SI_ORDER_FIRST, feeder_register, &feeder_root_class);
SYSUNINIT(feeder_root, SI_SUB_DRIVERS, SI_ORDER_FIRST, feeder_unregisterall, NULL);
