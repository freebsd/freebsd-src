/*-
 * Copyright (c) 2014-2018 Netflix, Inc.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Author: Lawrence Stewart <lstewart@netflix.com>
 */

#include <sys/param.h>
#include <sys/arb.h>
#include <sys/ctype.h>
#include <sys/errno.h>
#include <sys/hash.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/qmath.h>
#include <sys/sbuf.h>
#if defined(DIAGNOSTIC)
#include <sys/tree.h>
#endif
#include <sys/stats.h> /* Must come after qmath.h and arb.h */
#include <sys/stddef.h>
#include <sys/stdint.h>
#include <sys/time.h>

#ifdef _KERNEL
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#else /* ! _KERNEL */
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif /* _KERNEL */

struct voistatdata_voistate {
	/* Previous VOI value for diff calculation. */
	struct voistatdata_numeric prev;
};

#define	VS_VSDVALID	0x0001	/* Stat's voistatdata updated at least once. */
struct voistat {
	int8_t		stype;		/* Type of stat e.g. VS_STYPE_SUM. */
	enum vsd_dtype	dtype : 8;	/* Data type of this stat's data. */
	uint16_t	data_off;	/* Blob offset for this stat's data. */
	uint16_t	dsz;		/* Size of stat's data. */
#define	VS_EBITS 8
	uint16_t	errs : VS_EBITS;/* Non-wrapping error count. */
	uint16_t	flags : 16 - VS_EBITS;
};
/* The voistat error count is capped to avoid wrapping. */
#define	VS_INCERRS(vs) do {						\
	if ((vs)->errs < (1U << VS_EBITS) - 1)				\
		(vs)->errs++;						\
} while (0)

/*
 * Ideas for flags:
 *   - Global or entity specific (global would imply use of counter(9)?)
 *   - Whether to reset stats on read or not
 *   - Signal an overflow?
 *   - Compressed voistat array
 */
#define	VOI_REQSTATE	0x0001	/* VOI requires VS_STYPE_VOISTATE. */
struct voi {
	int16_t		id;		/* VOI id. */
	enum vsd_dtype	dtype : 8;	/* Data type of the VOI itself. */
	int8_t		voistatmaxid;	/* Largest allocated voistat index. */
	uint16_t	stats_off;	/* Blob offset for this VOIs stats. */
	uint16_t	flags;
};

/*
 * Memory for the entire blob is allocated as a slab and then offsets are
 * maintained to carve up the slab into sections holding different data types.
 *
 * Ideas for flags:
 * - Compressed voi array (trade off memory usage vs search time)
 * - Units of offsets (default bytes, flag for e.g. vm_page/KiB/Mib)
 */
struct statsblobv1 {
	uint8_t		abi;
	uint8_t		endian;
	uint16_t	flags;
	uint16_t	maxsz;
	uint16_t	cursz;
	/* Fields from here down are opaque to consumers. */
	uint32_t	tplhash;	/* Base template hash ID. */
	uint16_t	stats_off;	/* voistat array blob offset. */
	uint16_t	statsdata_off;	/* voistatdata array blob offset. */
	sbintime_t	created;	/* Blob creation time. */
	sbintime_t	lastrst;	/* Time of last reset. */
	struct voi	vois[];		/* Array indexed by [voi_id]. */
} __aligned(sizeof(void *));
_Static_assert(offsetof(struct statsblobv1, cursz) +
    SIZEOF_MEMBER(struct statsblobv1, cursz) ==
    offsetof(struct statsblob, opaque),
    "statsblobv1 ABI mismatch");

struct statsblobv1_tpl {
	struct metablob		*mb;
	struct statsblobv1	*sb;
};

/* Context passed to iterator callbacks. */
struct sb_iter_ctx {
	void		*usrctx;	/* Caller supplied context. */
	uint32_t	flags;		/* Flags for current iteration. */
	int16_t		vslot;		/* struct voi slot index. */
	int8_t		vsslot;		/* struct voistat slot index. */
};

struct sb_tostrcb_ctx {
	struct sbuf		*buf;
	struct statsblob_tpl	*tpl;
	enum sb_str_fmt	fmt;
	uint32_t		flags;
};

struct sb_visitcb_ctx {
	stats_blob_visitcb_t	cb;
	void			*usrctx;
};

/* Stats blob iterator callback. */
typedef int (*stats_v1_blob_itercb_t)(struct statsblobv1 *sb, struct voi *v,
    struct voistat *vs, struct sb_iter_ctx *ctx);

#ifdef _KERNEL
static struct rwlock tpllistlock;
RW_SYSINIT(stats_tpl_list, &tpllistlock, "Stat template list lock");
#define	TPL_LIST_RLOCK() rw_rlock(&tpllistlock)
#define	TPL_LIST_RUNLOCK() rw_runlock(&tpllistlock)
#define	TPL_LIST_WLOCK() rw_wlock(&tpllistlock)
#define	TPL_LIST_WUNLOCK() rw_wunlock(&tpllistlock)
#define	TPL_LIST_LOCK_ASSERT() rw_assert(&tpllistlock, RA_LOCKED)
#define	TPL_LIST_RLOCK_ASSERT() rw_assert(&tpllistlock, RA_RLOCKED)
#define	TPL_LIST_WLOCK_ASSERT() rw_assert(&tpllistlock, RA_WLOCKED)
MALLOC_DEFINE(M_STATS, "stats(9) related memory", "stats(9) related memory");
#define	stats_free(ptr) free((ptr), M_STATS)
#else /* ! _KERNEL */
static void stats_constructor(void);
static void stats_destructor(void);
static pthread_rwlock_t tpllistlock;
#define	TPL_LIST_UNLOCK() pthread_rwlock_unlock(&tpllistlock)
#define	TPL_LIST_RLOCK() pthread_rwlock_rdlock(&tpllistlock)
#define	TPL_LIST_RUNLOCK() TPL_LIST_UNLOCK()
#define	TPL_LIST_WLOCK() pthread_rwlock_wrlock(&tpllistlock)
#define	TPL_LIST_WUNLOCK() TPL_LIST_UNLOCK()
#define	TPL_LIST_LOCK_ASSERT() do { } while (0)
#define	TPL_LIST_RLOCK_ASSERT() do { } while (0)
#define	TPL_LIST_WLOCK_ASSERT() do { } while (0)
#ifdef NDEBUG
#define	KASSERT(cond, msg) do {} while (0)
#define	stats_abort() do {} while (0)
#else /* ! NDEBUG */
#define	KASSERT(cond, msg) do { \
	if (!(cond)) { \
		panic msg; \
	} \
} while (0)
#define	stats_abort() abort()
#endif /* NDEBUG */
#define	stats_free(ptr) free(ptr)
#define	panic(fmt, ...) do { \
	fprintf(stderr, (fmt), ##__VA_ARGS__); \
	stats_abort(); \
} while (0)
#endif /* _KERNEL */

#define	SB_V1_MAXSZ 65535

/* Obtain a blob offset pointer. */
#define	BLOB_OFFSET(sb, off) ((void *)(((uint8_t *)(sb)) + (off)))

/*
 * Number of VOIs in the blob's vois[] array. By virtue of struct voi being a
 * power of 2 size, we can shift instead of divide. The shift amount must be
 * updated if sizeof(struct voi) ever changes, which the assert should catch.
 */
#define	NVOIS(sb) ((int32_t)((((struct statsblobv1 *)(sb))->stats_off - \
    sizeof(struct statsblobv1)) >> 3))
_Static_assert(sizeof(struct voi) == 8, "statsblobv1 voi ABI mismatch");

/* Try restrict names to alphanumeric and underscore to simplify JSON compat. */
const char *vs_stype2name[VS_NUM_STYPES] = {
	[VS_STYPE_VOISTATE] = "VOISTATE",
	[VS_STYPE_SUM] = "SUM",
	[VS_STYPE_MAX] = "MAX",
	[VS_STYPE_MIN] = "MIN",
	[VS_STYPE_HIST] = "HIST",
	[VS_STYPE_TDGST] = "TDGST",
};

const char *vs_stype2desc[VS_NUM_STYPES] = {
	[VS_STYPE_VOISTATE] = "VOI related state data (not a real stat)",
	[VS_STYPE_SUM] = "Simple arithmetic accumulator",
	[VS_STYPE_MAX] = "Maximum observed VOI value",
	[VS_STYPE_MIN] = "Minimum observed VOI value",
	[VS_STYPE_HIST] = "Histogram of observed VOI values",
	[VS_STYPE_TDGST] = "t-digest of observed VOI values",
};

const char *vsd_dtype2name[VSD_NUM_DTYPES] = {
	[VSD_DTYPE_VOISTATE] = "VOISTATE",
	[VSD_DTYPE_INT_S32] = "INT_S32",
	[VSD_DTYPE_INT_U32] = "INT_U32",
	[VSD_DTYPE_INT_S64] = "INT_S64",
	[VSD_DTYPE_INT_U64] = "INT_U64",
	[VSD_DTYPE_INT_SLONG] = "INT_SLONG",
	[VSD_DTYPE_INT_ULONG] = "INT_ULONG",
	[VSD_DTYPE_Q_S32] = "Q_S32",
	[VSD_DTYPE_Q_U32] = "Q_U32",
	[VSD_DTYPE_Q_S64] = "Q_S64",
	[VSD_DTYPE_Q_U64] = "Q_U64",
	[VSD_DTYPE_CRHIST32] = "CRHIST32",
	[VSD_DTYPE_DRHIST32] = "DRHIST32",
	[VSD_DTYPE_DVHIST32] = "DVHIST32",
	[VSD_DTYPE_CRHIST64] = "CRHIST64",
	[VSD_DTYPE_DRHIST64] = "DRHIST64",
	[VSD_DTYPE_DVHIST64] = "DVHIST64",
	[VSD_DTYPE_TDGSTCLUST32] = "TDGSTCLUST32",
	[VSD_DTYPE_TDGSTCLUST64] = "TDGSTCLUST64",
};

const size_t vsd_dtype2size[VSD_NUM_DTYPES] = {
	[VSD_DTYPE_VOISTATE] = sizeof(struct voistatdata_voistate),
	[VSD_DTYPE_INT_S32] = sizeof(struct voistatdata_int32),
	[VSD_DTYPE_INT_U32] = sizeof(struct voistatdata_int32),
	[VSD_DTYPE_INT_S64] = sizeof(struct voistatdata_int64),
	[VSD_DTYPE_INT_U64] = sizeof(struct voistatdata_int64),
	[VSD_DTYPE_INT_SLONG] = sizeof(struct voistatdata_intlong),
	[VSD_DTYPE_INT_ULONG] = sizeof(struct voistatdata_intlong),
	[VSD_DTYPE_Q_S32] = sizeof(struct voistatdata_q32),
	[VSD_DTYPE_Q_U32] = sizeof(struct voistatdata_q32),
	[VSD_DTYPE_Q_S64] = sizeof(struct voistatdata_q64),
	[VSD_DTYPE_Q_U64] = sizeof(struct voistatdata_q64),
	[VSD_DTYPE_CRHIST32] = sizeof(struct voistatdata_crhist32),
	[VSD_DTYPE_DRHIST32] = sizeof(struct voistatdata_drhist32),
	[VSD_DTYPE_DVHIST32] = sizeof(struct voistatdata_dvhist32),
	[VSD_DTYPE_CRHIST64] = sizeof(struct voistatdata_crhist64),
	[VSD_DTYPE_DRHIST64] = sizeof(struct voistatdata_drhist64),
	[VSD_DTYPE_DVHIST64] = sizeof(struct voistatdata_dvhist64),
	[VSD_DTYPE_TDGSTCLUST32] = sizeof(struct voistatdata_tdgstclust32),
	[VSD_DTYPE_TDGSTCLUST64] = sizeof(struct voistatdata_tdgstclust64),
};

static const bool vsd_compoundtype[VSD_NUM_DTYPES] = {
	[VSD_DTYPE_VOISTATE] = true,
	[VSD_DTYPE_INT_S32] = false,
	[VSD_DTYPE_INT_U32] = false,
	[VSD_DTYPE_INT_S64] = false,
	[VSD_DTYPE_INT_U64] = false,
	[VSD_DTYPE_INT_SLONG] = false,
	[VSD_DTYPE_INT_ULONG] = false,
	[VSD_DTYPE_Q_S32] = false,
	[VSD_DTYPE_Q_U32] = false,
	[VSD_DTYPE_Q_S64] = false,
	[VSD_DTYPE_Q_U64] = false,
	[VSD_DTYPE_CRHIST32] = true,
	[VSD_DTYPE_DRHIST32] = true,
	[VSD_DTYPE_DVHIST32] = true,
	[VSD_DTYPE_CRHIST64] = true,
	[VSD_DTYPE_DRHIST64] = true,
	[VSD_DTYPE_DVHIST64] = true,
	[VSD_DTYPE_TDGSTCLUST32] = true,
	[VSD_DTYPE_TDGSTCLUST64] = true,
};

const struct voistatdata_numeric numeric_limits[2][VSD_DTYPE_Q_U64 + 1] = {
	[LIM_MIN] = {
		[VSD_DTYPE_VOISTATE] = {0},
		[VSD_DTYPE_INT_S32] = {.int32 = {.s32 = INT32_MIN}},
		[VSD_DTYPE_INT_U32] = {.int32 = {.u32 = 0}},
		[VSD_DTYPE_INT_S64] = {.int64 = {.s64 = INT64_MIN}},
		[VSD_DTYPE_INT_U64] = {.int64 = {.u64 = 0}},
		[VSD_DTYPE_INT_SLONG] = {.intlong = {.slong = LONG_MIN}},
		[VSD_DTYPE_INT_ULONG] = {.intlong = {.ulong = 0}},
		[VSD_DTYPE_Q_S32] = {.q32 = {.sq32 = Q_IFMINVAL(INT32_MIN)}},
		[VSD_DTYPE_Q_U32] = {.q32 = {.uq32 = 0}},
		[VSD_DTYPE_Q_S64] = {.q64 = {.sq64 = Q_IFMINVAL(INT64_MIN)}},
		[VSD_DTYPE_Q_U64] = {.q64 = {.uq64 = 0}},
	},
	[LIM_MAX] = {
		[VSD_DTYPE_VOISTATE] = {0},
		[VSD_DTYPE_INT_S32] = {.int32 = {.s32 = INT32_MAX}},
		[VSD_DTYPE_INT_U32] = {.int32 = {.u32 = UINT32_MAX}},
		[VSD_DTYPE_INT_S64] = {.int64 = {.s64 = INT64_MAX}},
		[VSD_DTYPE_INT_U64] = {.int64 = {.u64 = UINT64_MAX}},
		[VSD_DTYPE_INT_SLONG] = {.intlong = {.slong = LONG_MAX}},
		[VSD_DTYPE_INT_ULONG] = {.intlong = {.ulong = ULONG_MAX}},
		[VSD_DTYPE_Q_S32] = {.q32 = {.sq32 = Q_IFMAXVAL(INT32_MAX)}},
		[VSD_DTYPE_Q_U32] = {.q32 = {.uq32 = Q_IFMAXVAL(UINT32_MAX)}},
		[VSD_DTYPE_Q_S64] = {.q64 = {.sq64 = Q_IFMAXVAL(INT64_MAX)}},
		[VSD_DTYPE_Q_U64] = {.q64 = {.uq64 = Q_IFMAXVAL(UINT64_MAX)}},
	}
};

/* tpllistlock protects tpllist and ntpl */
static uint32_t ntpl;
static struct statsblob_tpl **tpllist;

static inline void * stats_realloc(void *ptr, size_t oldsz, size_t newsz,
    int flags);
//static void stats_v1_blob_finalise(struct statsblobv1 *sb);
static int stats_v1_blob_init_locked(struct statsblobv1 *sb, uint32_t tpl_id,
    uint32_t flags);
static int stats_v1_blob_expand(struct statsblobv1 **sbpp, int newvoibytes,
    int newvoistatbytes, int newvoistatdatabytes);
static void stats_v1_blob_iter(struct statsblobv1 *sb,
    stats_v1_blob_itercb_t icb, void *usrctx, uint32_t flags);
static inline int stats_v1_vsd_tdgst_add(enum vsd_dtype vs_dtype,
    struct voistatdata_tdgst *tdgst, s64q_t x, uint64_t weight, int attempt);

static inline int
ctd32cmp(const struct voistatdata_tdgstctd32 *c1, const struct voistatdata_tdgstctd32 *c2)
{

	KASSERT(Q_PRECEQ(c1->mu, c2->mu),
	    ("%s: Q_RELPREC(c1->mu,c2->mu)=%d", __func__,
	    Q_RELPREC(c1->mu, c2->mu)));

       return (Q_QLTQ(c1->mu, c2->mu) ? -1 : 1);
}
ARB_GENERATE_STATIC(ctdth32, voistatdata_tdgstctd32, ctdlnk, ctd32cmp);

static inline int
ctd64cmp(const struct voistatdata_tdgstctd64 *c1, const struct voistatdata_tdgstctd64 *c2)
{

	KASSERT(Q_PRECEQ(c1->mu, c2->mu),
	    ("%s: Q_RELPREC(c1->mu,c2->mu)=%d", __func__,
	    Q_RELPREC(c1->mu, c2->mu)));

       return (Q_QLTQ(c1->mu, c2->mu) ? -1 : 1);
}
ARB_GENERATE_STATIC(ctdth64, voistatdata_tdgstctd64, ctdlnk, ctd64cmp);

#ifdef DIAGNOSTIC
RB_GENERATE_STATIC(rbctdth32, voistatdata_tdgstctd32, rblnk, ctd32cmp);
RB_GENERATE_STATIC(rbctdth64, voistatdata_tdgstctd64, rblnk, ctd64cmp);
#endif

static inline sbintime_t
stats_sbinuptime(void)
{
	sbintime_t sbt;
#ifdef _KERNEL

	sbt = sbinuptime();
#else /* ! _KERNEL */
	struct timespec tp;

	clock_gettime(CLOCK_MONOTONIC_FAST, &tp);
	sbt = tstosbt(tp);
#endif /* _KERNEL */

	return (sbt);
}

static inline void *
stats_realloc(void *ptr, size_t oldsz, size_t newsz, int flags)
{

#ifdef _KERNEL
	/* Default to M_NOWAIT if neither M_NOWAIT or M_WAITOK are set. */
	if (!(flags & (M_WAITOK | M_NOWAIT)))
		flags |= M_NOWAIT;
	ptr = realloc(ptr, newsz, M_STATS, flags);
#else /* ! _KERNEL */
	ptr = realloc(ptr, newsz);
	if ((flags & M_ZERO) && ptr != NULL) {
		if (oldsz == 0)
			memset(ptr, '\0', newsz);
		else if (newsz > oldsz)
			memset(BLOB_OFFSET(ptr, oldsz), '\0', newsz - oldsz);
	}
#endif /* _KERNEL */

	return (ptr);
}

static inline char *
stats_strdup(const char *s,
#ifdef _KERNEL
    int flags)
{
	char *copy;
	size_t len;

	if (!(flags & (M_WAITOK | M_NOWAIT)))
		flags |= M_NOWAIT;

	len = strlen(s) + 1;
	if ((copy = malloc(len, M_STATS, flags)) != NULL)
		bcopy(s, copy, len);

	return (copy);
#else
    int flags __unused)
{
	return (strdup(s));
#endif
}

static inline void
stats_tpl_update_hash(struct statsblob_tpl *tpl)
{

	TPL_LIST_WLOCK_ASSERT();
	tpl->mb->tplhash = hash32_str(tpl->mb->tplname, 0);
	for (int voi_id = 0; voi_id < NVOIS(tpl->sb); voi_id++) {
		if (tpl->mb->voi_meta[voi_id].name != NULL)
			tpl->mb->tplhash = hash32_str(
			    tpl->mb->voi_meta[voi_id].name, tpl->mb->tplhash);
	}
	tpl->mb->tplhash = hash32_buf(tpl->sb, tpl->sb->cursz,
	    tpl->mb->tplhash);
}

static inline uint64_t
stats_pow_u64(uint64_t base, uint64_t exp)
{
	uint64_t result = 1;

	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}

	return (result);
}

static inline int
stats_vss_hist_bkt_hlpr(struct vss_hist_hlpr_info *info, uint32_t curbkt,
    struct voistatdata_numeric *bkt_lb, struct voistatdata_numeric *bkt_ub)
{
	uint64_t step = 0;
	int error = 0;

	switch (info->scheme) {
	case BKT_LIN:
		step = info->lin.stepinc;
		break;
	case BKT_EXP:
		step = stats_pow_u64(info->exp.stepbase,
		    info->exp.stepexp + curbkt);
		break;
	case BKT_LINEXP:
		{
		uint64_t curstepexp = 1;

		switch (info->voi_dtype) {
		case VSD_DTYPE_INT_S32:
			while ((int32_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= bkt_lb->int32.s32)
				curstepexp++;
			break;
		case VSD_DTYPE_INT_U32:
			while ((uint32_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= bkt_lb->int32.u32)
				curstepexp++;
			break;
		case VSD_DTYPE_INT_S64:
			while ((int64_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= bkt_lb->int64.s64)
				curstepexp++;
			break;
		case VSD_DTYPE_INT_U64:
			while ((uint64_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= bkt_lb->int64.u64)
				curstepexp++;
			break;
		case VSD_DTYPE_INT_SLONG:
			while ((long)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= bkt_lb->intlong.slong)
				curstepexp++;
			break;
		case VSD_DTYPE_INT_ULONG:
			while ((unsigned long)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= bkt_lb->intlong.ulong)
				curstepexp++;
			break;
		case VSD_DTYPE_Q_S32:
			while ((s32q_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= Q_GIVAL(bkt_lb->q32.sq32))
			break;
		case VSD_DTYPE_Q_U32:
			while ((u32q_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= Q_GIVAL(bkt_lb->q32.uq32))
			break;
		case VSD_DTYPE_Q_S64:
			while ((s64q_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= Q_GIVAL(bkt_lb->q64.sq64))
				curstepexp++;
			break;
		case VSD_DTYPE_Q_U64:
			while ((u64q_t)stats_pow_u64(info->linexp.stepbase,
			    curstepexp) <= Q_GIVAL(bkt_lb->q64.uq64))
				curstepexp++;
			break;
		default:
			break;
		}

		step = stats_pow_u64(info->linexp.stepbase, curstepexp) /
		    info->linexp.linstepdiv;
		if (step == 0)
			step = 1;
		break;
		}
	default:
		break;
	}

	if (info->scheme == BKT_USR) {
		*bkt_lb = info->usr.bkts[curbkt].lb;
		*bkt_ub = info->usr.bkts[curbkt].ub;
	} else if (step != 0) {
		switch (info->voi_dtype) {
		case VSD_DTYPE_INT_S32:
			bkt_ub->int32.s32 += (int32_t)step;
			break;
		case VSD_DTYPE_INT_U32:
			bkt_ub->int32.u32 += (uint32_t)step;
			break;
		case VSD_DTYPE_INT_S64:
			bkt_ub->int64.s64 += (int64_t)step;
			break;
		case VSD_DTYPE_INT_U64:
			bkt_ub->int64.u64 += (uint64_t)step;
			break;
		case VSD_DTYPE_INT_SLONG:
			bkt_ub->intlong.slong += (long)step;
			break;
		case VSD_DTYPE_INT_ULONG:
			bkt_ub->intlong.ulong += (unsigned long)step;
			break;
		case VSD_DTYPE_Q_S32:
			error = Q_QADDI(&bkt_ub->q32.sq32, step);
			break;
		case VSD_DTYPE_Q_U32:
			error = Q_QADDI(&bkt_ub->q32.uq32, step);
			break;
		case VSD_DTYPE_Q_S64:
			error = Q_QADDI(&bkt_ub->q64.sq64, step);
			break;
		case VSD_DTYPE_Q_U64:
			error = Q_QADDI(&bkt_ub->q64.uq64, step);
			break;
		default:
			break;
		}
	} else { /* info->scheme != BKT_USR && step == 0 */
		return (EINVAL);
	}

	return (error);
}

static uint32_t
stats_vss_hist_nbkts_hlpr(struct vss_hist_hlpr_info *info)
{
	struct voistatdata_numeric bkt_lb, bkt_ub;
	uint32_t nbkts;
	int done;

	if (info->scheme == BKT_USR) {
		/* XXXLAS: Setting info->{lb,ub} from macro is tricky. */
		info->lb = info->usr.bkts[0].lb;
		info->ub = info->usr.bkts[info->usr.nbkts - 1].lb;
	}

	nbkts = 0;
	done = 0;
	bkt_ub = info->lb;

	do {
		bkt_lb = bkt_ub;
		if (stats_vss_hist_bkt_hlpr(info, nbkts++, &bkt_lb, &bkt_ub))
			return (0);

		if (info->scheme == BKT_USR)
			done = (nbkts == info->usr.nbkts);
		else {
			switch (info->voi_dtype) {
			case VSD_DTYPE_INT_S32:
				done = (bkt_ub.int32.s32 > info->ub.int32.s32);
				break;
			case VSD_DTYPE_INT_U32:
				done = (bkt_ub.int32.u32 > info->ub.int32.u32);
				break;
			case VSD_DTYPE_INT_S64:
				done = (bkt_ub.int64.s64 > info->ub.int64.s64);
				break;
			case VSD_DTYPE_INT_U64:
				done = (bkt_ub.int64.u64 > info->ub.int64.u64);
				break;
			case VSD_DTYPE_INT_SLONG:
				done = (bkt_ub.intlong.slong >
				    info->ub.intlong.slong);
				break;
			case VSD_DTYPE_INT_ULONG:
				done = (bkt_ub.intlong.ulong >
				    info->ub.intlong.ulong);
				break;
			case VSD_DTYPE_Q_S32:
				done = Q_QGTQ(bkt_ub.q32.sq32,
				    info->ub.q32.sq32);
				break;
			case VSD_DTYPE_Q_U32:
				done = Q_QGTQ(bkt_ub.q32.uq32,
				    info->ub.q32.uq32);
				break;
			case VSD_DTYPE_Q_S64:
				done = Q_QGTQ(bkt_ub.q64.sq64,
				    info->ub.q64.sq64);
				break;
			case VSD_DTYPE_Q_U64:
				done = Q_QGTQ(bkt_ub.q64.uq64,
				    info->ub.q64.uq64);
				break;
			default:
				return (0);
			}
		}
	} while (!done);

	if (info->flags & VSD_HIST_LBOUND_INF)
		nbkts++;
	if (info->flags & VSD_HIST_UBOUND_INF)
		nbkts++;

	return (nbkts);
}

int
stats_vss_hist_hlpr(enum vsd_dtype voi_dtype, struct voistatspec *vss,
    struct vss_hist_hlpr_info *info)
{
	struct voistatdata_hist *hist;
	struct voistatdata_numeric bkt_lb, bkt_ub, *lbinfbktlb, *lbinfbktub,
	    *ubinfbktlb, *ubinfbktub;
	uint32_t bkt, nbkts, nloop;

	if (vss == NULL || info == NULL || (info->flags &
	(VSD_HIST_LBOUND_INF|VSD_HIST_UBOUND_INF) && (info->hist_dtype ==
	VSD_DTYPE_DVHIST32 || info->hist_dtype == VSD_DTYPE_DVHIST64)))
		return (EINVAL);

	info->voi_dtype = voi_dtype;

	if ((nbkts = stats_vss_hist_nbkts_hlpr(info)) == 0)
		return (EINVAL);

	switch (info->hist_dtype) {
	case VSD_DTYPE_CRHIST32:
		vss->vsdsz = HIST_NBKTS2VSDSZ(crhist32, nbkts);
		break;
	case VSD_DTYPE_DRHIST32:
		vss->vsdsz = HIST_NBKTS2VSDSZ(drhist32, nbkts);
		break;
	case VSD_DTYPE_DVHIST32:
		vss->vsdsz = HIST_NBKTS2VSDSZ(dvhist32, nbkts);
		break;
	case VSD_DTYPE_CRHIST64:
		vss->vsdsz = HIST_NBKTS2VSDSZ(crhist64, nbkts);
		break;
	case VSD_DTYPE_DRHIST64:
		vss->vsdsz = HIST_NBKTS2VSDSZ(drhist64, nbkts);
		break;
	case VSD_DTYPE_DVHIST64:
		vss->vsdsz = HIST_NBKTS2VSDSZ(dvhist64, nbkts);
		break;
	default:
		return (EINVAL);
	}

	vss->iv = stats_realloc(NULL, 0, vss->vsdsz, M_ZERO);
	if (vss->iv == NULL)
		return (ENOMEM);

	hist = (struct voistatdata_hist *)vss->iv;
	bkt_ub = info->lb;

	for (bkt = (info->flags & VSD_HIST_LBOUND_INF), nloop = 0;
	    bkt < nbkts;
	    bkt++, nloop++) {
		bkt_lb = bkt_ub;
		if (stats_vss_hist_bkt_hlpr(info, nloop, &bkt_lb, &bkt_ub))
			return (EINVAL);

		switch (info->hist_dtype) {
		case VSD_DTYPE_CRHIST32:
			VSD(crhist32, hist)->bkts[bkt].lb = bkt_lb;
			break;
		case VSD_DTYPE_DRHIST32:
			VSD(drhist32, hist)->bkts[bkt].lb = bkt_lb;
			VSD(drhist32, hist)->bkts[bkt].ub = bkt_ub;
			break;
		case VSD_DTYPE_DVHIST32:
			VSD(dvhist32, hist)->bkts[bkt].val = bkt_lb;
			break;
		case VSD_DTYPE_CRHIST64:
			VSD(crhist64, hist)->bkts[bkt].lb = bkt_lb;
			break;
		case VSD_DTYPE_DRHIST64:
			VSD(drhist64, hist)->bkts[bkt].lb = bkt_lb;
			VSD(drhist64, hist)->bkts[bkt].ub = bkt_ub;
			break;
		case VSD_DTYPE_DVHIST64:
			VSD(dvhist64, hist)->bkts[bkt].val = bkt_lb;
			break;
		default:
			return (EINVAL);
		}
	}

	lbinfbktlb = lbinfbktub = ubinfbktlb = ubinfbktub = NULL;

	switch (info->hist_dtype) {
	case VSD_DTYPE_CRHIST32:
		lbinfbktlb = &VSD(crhist32, hist)->bkts[0].lb;
		ubinfbktlb = &VSD(crhist32, hist)->bkts[nbkts - 1].lb;
		break;
	case VSD_DTYPE_DRHIST32:
		lbinfbktlb = &VSD(drhist32, hist)->bkts[0].lb;
		lbinfbktub = &VSD(drhist32, hist)->bkts[0].ub;
		ubinfbktlb = &VSD(drhist32, hist)->bkts[nbkts - 1].lb;
		ubinfbktub = &VSD(drhist32, hist)->bkts[nbkts - 1].ub;
		break;
	case VSD_DTYPE_CRHIST64:
		lbinfbktlb = &VSD(crhist64, hist)->bkts[0].lb;
		ubinfbktlb = &VSD(crhist64, hist)->bkts[nbkts - 1].lb;
		break;
	case VSD_DTYPE_DRHIST64:
		lbinfbktlb = &VSD(drhist64, hist)->bkts[0].lb;
		lbinfbktub = &VSD(drhist64, hist)->bkts[0].ub;
		ubinfbktlb = &VSD(drhist64, hist)->bkts[nbkts - 1].lb;
		ubinfbktub = &VSD(drhist64, hist)->bkts[nbkts - 1].ub;
		break;
	case VSD_DTYPE_DVHIST32:
	case VSD_DTYPE_DVHIST64:
		break;
	default:
		return (EINVAL);
	}

	if ((info->flags & VSD_HIST_LBOUND_INF) && lbinfbktlb) {
		*lbinfbktlb = numeric_limits[LIM_MIN][info->voi_dtype];
		/*
		 * Assignment from numeric_limit array for Q types assigns max
		 * possible integral/fractional value for underlying data type,
		 * but we must set control bits for this specific histogram per
		 * the user's choice of fractional bits, which we extract from
		 * info->lb.
		 */
		if (info->voi_dtype == VSD_DTYPE_Q_S32 ||
		    info->voi_dtype == VSD_DTYPE_Q_U32) {
			/* Signedness doesn't matter for setting control bits. */
			Q_SCVAL(lbinfbktlb->q32.sq32,
			    Q_GCVAL(info->lb.q32.sq32));
		} else if (info->voi_dtype == VSD_DTYPE_Q_S64 ||
		    info->voi_dtype == VSD_DTYPE_Q_U64) {
			/* Signedness doesn't matter for setting control bits. */
			Q_SCVAL(lbinfbktlb->q64.sq64,
			    Q_GCVAL(info->lb.q64.sq64));
		}
		if (lbinfbktub)
			*lbinfbktub = info->lb;
	}
	if ((info->flags & VSD_HIST_UBOUND_INF) && ubinfbktlb) {
		*ubinfbktlb = bkt_lb;
		if (ubinfbktub) {
			*ubinfbktub = numeric_limits[LIM_MAX][info->voi_dtype];
			if (info->voi_dtype == VSD_DTYPE_Q_S32 ||
			    info->voi_dtype == VSD_DTYPE_Q_U32) {
				Q_SCVAL(ubinfbktub->q32.sq32,
				    Q_GCVAL(info->lb.q32.sq32));
			} else if (info->voi_dtype == VSD_DTYPE_Q_S64 ||
			    info->voi_dtype == VSD_DTYPE_Q_U64) {
				Q_SCVAL(ubinfbktub->q64.sq64,
				    Q_GCVAL(info->lb.q64.sq64));
			}
		}
	}

	return (0);
}

int
stats_vss_tdgst_hlpr(enum vsd_dtype voi_dtype, struct voistatspec *vss,
    struct vss_tdgst_hlpr_info *info)
{
	struct voistatdata_tdgst *tdgst;
	struct ctdth32 *ctd32tree;
	struct ctdth64 *ctd64tree;
	struct voistatdata_tdgstctd32 *ctd32;
	struct voistatdata_tdgstctd64 *ctd64;

	info->voi_dtype = voi_dtype;

	switch (info->tdgst_dtype) {
	case VSD_DTYPE_TDGSTCLUST32:
		vss->vsdsz = TDGST_NCTRS2VSDSZ(tdgstclust32, info->nctds);
		break;
	case VSD_DTYPE_TDGSTCLUST64:
		vss->vsdsz = TDGST_NCTRS2VSDSZ(tdgstclust64, info->nctds);
		break;
	default:
		return (EINVAL);
	}

	vss->iv = stats_realloc(NULL, 0, vss->vsdsz, M_ZERO);
	if (vss->iv == NULL)
		return (ENOMEM);

	tdgst = (struct voistatdata_tdgst *)vss->iv;

	switch (info->tdgst_dtype) {
	case VSD_DTYPE_TDGSTCLUST32:
		ctd32tree = &VSD(tdgstclust32, tdgst)->ctdtree;
		ARB_INIT(ctd32, ctdlnk, ctd32tree, info->nctds) {
			Q_INI(&ctd32->mu, 0, 0, info->prec);
		}
		break;
	case VSD_DTYPE_TDGSTCLUST64:
		ctd64tree = &VSD(tdgstclust64, tdgst)->ctdtree;
		ARB_INIT(ctd64, ctdlnk, ctd64tree, info->nctds) {
			Q_INI(&ctd64->mu, 0, 0, info->prec);
		}
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

int
stats_vss_numeric_hlpr(enum vsd_dtype voi_dtype, struct voistatspec *vss,
    struct vss_numeric_hlpr_info *info)
{
	struct voistatdata_numeric iv;

	switch (vss->stype) {
	case VS_STYPE_SUM:
		iv = stats_ctor_vsd_numeric(0);
		break;
	case VS_STYPE_MIN:
		iv = numeric_limits[LIM_MAX][voi_dtype];
		break;
	case VS_STYPE_MAX:
		iv = numeric_limits[LIM_MIN][voi_dtype];
		break;
	default:
		return (EINVAL);
	}

	vss->iv = stats_realloc(NULL, 0, vsd_dtype2size[voi_dtype], 0);
	if (vss->iv == NULL)
		return (ENOMEM);

	vss->vs_dtype = voi_dtype;
	vss->vsdsz = vsd_dtype2size[voi_dtype];
	switch (voi_dtype) {
	case VSD_DTYPE_INT_S32:
		*((int32_t *)vss->iv) = iv.int32.s32;
		break;
	case VSD_DTYPE_INT_U32:
		*((uint32_t *)vss->iv) = iv.int32.u32;
		break;
	case VSD_DTYPE_INT_S64:
		*((int64_t *)vss->iv) = iv.int64.s64;
		break;
	case VSD_DTYPE_INT_U64:
		*((uint64_t *)vss->iv) = iv.int64.u64;
		break;
	case VSD_DTYPE_INT_SLONG:
		*((long *)vss->iv) = iv.intlong.slong;
		break;
	case VSD_DTYPE_INT_ULONG:
		*((unsigned long *)vss->iv) = iv.intlong.ulong;
		break;
	case VSD_DTYPE_Q_S32:
		*((s32q_t *)vss->iv) = Q_SCVAL(iv.q32.sq32,
		    Q_CTRLINI(info->prec));
		break;
	case VSD_DTYPE_Q_U32:
		*((u32q_t *)vss->iv) = Q_SCVAL(iv.q32.uq32,
		    Q_CTRLINI(info->prec));
		break;
	case VSD_DTYPE_Q_S64:
		*((s64q_t *)vss->iv) = Q_SCVAL(iv.q64.sq64,
		    Q_CTRLINI(info->prec));
		break;
	case VSD_DTYPE_Q_U64:
		*((u64q_t *)vss->iv) = Q_SCVAL(iv.q64.uq64,
		    Q_CTRLINI(info->prec));
		break;
	default:
		break;
	}

	return (0);
}

int
stats_vss_hlpr_init(enum vsd_dtype voi_dtype, uint32_t nvss,
    struct voistatspec *vss)
{
	int i, ret;

	for (i = nvss - 1; i >= 0; i--) {
		if (vss[i].hlpr && (ret = vss[i].hlpr(voi_dtype, &vss[i],
		    vss[i].hlprinfo)) != 0)
			return (ret);
	}

	return (0);
}

void
stats_vss_hlpr_cleanup(uint32_t nvss, struct voistatspec *vss)
{
	int i;

	for (i = nvss - 1; i >= 0; i--) {
		if (vss[i].hlpr) {
			stats_free((void *)vss[i].iv);
			vss[i].iv = NULL;
		}
	}
}

int
stats_tpl_fetch(int tpl_id, struct statsblob_tpl **tpl)
{
	int error;

	error = 0;

	TPL_LIST_WLOCK();
	if (tpl_id < 0 || tpl_id >= (int)ntpl) {
		error = ENOENT;
	} else {
		*tpl = tpllist[tpl_id];
		/* XXXLAS: Acquire refcount on tpl. */
	}
	TPL_LIST_WUNLOCK();

	return (error);
}

int
stats_tpl_fetch_allocid(const char *name, uint32_t hash)
{
	int i, tpl_id;

	tpl_id = -ESRCH;

	TPL_LIST_RLOCK();
	for (i = ntpl - 1; i >= 0; i--) {
		if (name != NULL) {
			if (strlen(name) == strlen(tpllist[i]->mb->tplname) &&
			    strncmp(name, tpllist[i]->mb->tplname,
			    TPL_MAX_NAME_LEN) == 0 && (!hash || hash ==
			    tpllist[i]->mb->tplhash)) {
				tpl_id = i;
				break;
			}
		} else if (hash == tpllist[i]->mb->tplhash) {
			tpl_id = i;
			break;
		}
	}
	TPL_LIST_RUNLOCK();

	return (tpl_id);
}

int
stats_tpl_id2name(uint32_t tpl_id, char *buf, size_t len)
{
	int error;

	error = 0;

	TPL_LIST_RLOCK();
	if (tpl_id < ntpl) {
		if (buf != NULL && len > strlen(tpllist[tpl_id]->mb->tplname))
			strlcpy(buf, tpllist[tpl_id]->mb->tplname, len);
		else
			error = EOVERFLOW;
	} else
		error = ENOENT;
	TPL_LIST_RUNLOCK();

	return (error);
}

int
stats_tpl_sample_rollthedice(struct stats_tpl_sample_rate *rates, int nrates,
    void *seed_bytes, size_t seed_len)
{
	uint32_t cum_pct, rnd_pct;
	int i;

	cum_pct = 0;

	/*
	 * Choose a pseudorandom or seeded number in range [0,100] and use
	 * it to make a sampling decision and template selection where required.
	 * If no seed is supplied, a PRNG is used to generate a pseudorandom
	 * number so that every selection is independent. If a seed is supplied,
	 * the caller desires random selection across different seeds, but
	 * deterministic selection given the same seed. This is achieved by
	 * hashing the seed and using the hash as the random number source.
	 *
	 * XXXLAS: Characterise hash function output distribution.
	 */
	if (seed_bytes == NULL)
		rnd_pct = random() / (INT32_MAX / 100);
	else
		rnd_pct = hash32_buf(seed_bytes, seed_len, 0) /
		    (UINT32_MAX / 100U);

	/*
	 * We map the randomly selected percentage on to the interval [0,100]
	 * consisting of the cumulatively summed template sampling percentages.
	 * The difference between the cumulative sum of all template sampling
	 * percentages and 100 is treated as a NULL assignment i.e. no stats
	 * template will be assigned, and -1 returned instead.
	 */
	for (i = 0; i < nrates; i++) {
		cum_pct += rates[i].tpl_sample_pct;

		KASSERT(cum_pct <= 100, ("%s cum_pct %u > 100", __func__,
		    cum_pct));
		if (rnd_pct > cum_pct || rates[i].tpl_sample_pct == 0)
			continue;

		return (rates[i].tpl_slot_id);
	}

	return (-1);
}

int
stats_v1_blob_clone(struct statsblobv1 **dst, size_t dstmaxsz,
    struct statsblobv1 *src, uint32_t flags)
{
	int error;

	error = 0;

	if (src == NULL || dst == NULL ||
	    src->cursz < sizeof(struct statsblob) ||
	    ((flags & SB_CLONE_ALLOCDST) &&
	    (flags & (SB_CLONE_USRDSTNOFAULT | SB_CLONE_USRDST)))) {
		error = EINVAL;
	} else if (flags & SB_CLONE_ALLOCDST) {
		*dst = stats_realloc(NULL, 0, src->cursz, 0);
		if (*dst)
			(*dst)->maxsz = dstmaxsz = src->cursz;
		else
			error = ENOMEM;
	} else if (*dst == NULL || dstmaxsz < sizeof(struct statsblob)) {
		error = EINVAL;
	}

	if (!error) {
		size_t postcurszlen;

		/*
		 * Clone src into dst except for the maxsz field. If dst is too
		 * small to hold all of src, only copy src's header and return
		 * EOVERFLOW.
		 */
#ifdef _KERNEL
		if (flags & SB_CLONE_USRDSTNOFAULT)
			copyout_nofault(src, *dst,
			    offsetof(struct statsblob, maxsz));
		else if (flags & SB_CLONE_USRDST)
			copyout(src, *dst, offsetof(struct statsblob, maxsz));
		else
#endif
			memcpy(*dst, src, offsetof(struct statsblob, maxsz));

		if (dstmaxsz >= src->cursz) {
			postcurszlen = src->cursz -
			    offsetof(struct statsblob, cursz);
		} else {
			error = EOVERFLOW;
			postcurszlen = sizeof(struct statsblob) -
			    offsetof(struct statsblob, cursz);
		}
#ifdef _KERNEL
		if (flags & SB_CLONE_USRDSTNOFAULT)
			copyout_nofault(&(src->cursz), &((*dst)->cursz),
			    postcurszlen);
		else if (flags & SB_CLONE_USRDST)
			copyout(&(src->cursz), &((*dst)->cursz), postcurszlen);
		else
#endif
			memcpy(&((*dst)->cursz), &(src->cursz), postcurszlen);
	}

	return (error);
}

int
stats_v1_tpl_alloc(const char *name, uint32_t flags __unused)
{
	struct statsblobv1_tpl *tpl, **newtpllist;
	struct statsblobv1 *tpl_sb;
	struct metablob *tpl_mb;
	int tpl_id;

	if (name != NULL && strlen(name) > TPL_MAX_NAME_LEN)
		return (-EINVAL);

	if (name != NULL && stats_tpl_fetch_allocid(name, 0) >= 0)
		return (-EEXIST);

	tpl = stats_realloc(NULL, 0, sizeof(struct statsblobv1_tpl), M_ZERO);
	tpl_mb = stats_realloc(NULL, 0, sizeof(struct metablob), M_ZERO);
	tpl_sb = stats_realloc(NULL, 0, sizeof(struct statsblobv1), M_ZERO);

	if (tpl_mb != NULL && name != NULL)
		tpl_mb->tplname = stats_strdup(name, 0);

	if (tpl == NULL || tpl_sb == NULL || tpl_mb == NULL ||
	    tpl_mb->tplname == NULL) {
		stats_free(tpl);
		stats_free(tpl_sb);
		if (tpl_mb != NULL) {
			stats_free(tpl_mb->tplname);
			stats_free(tpl_mb);
		}
		return (-ENOMEM);
	}

	tpl->mb = tpl_mb;
	tpl->sb = tpl_sb;

	tpl_sb->abi = STATS_ABI_V1;
	tpl_sb->endian =
#if BYTE_ORDER == LITTLE_ENDIAN
	    SB_LE;
#elif BYTE_ORDER == BIG_ENDIAN
	    SB_BE;
#else
	    SB_UE;
#endif
	tpl_sb->cursz = tpl_sb->maxsz = sizeof(struct statsblobv1);
	tpl_sb->stats_off = tpl_sb->statsdata_off = sizeof(struct statsblobv1);

	TPL_LIST_WLOCK();
	newtpllist = stats_realloc(tpllist, ntpl * sizeof(void *),
	    (ntpl + 1) * sizeof(void *), 0);
	if (newtpllist != NULL) {
		tpl_id = ntpl++;
		tpllist = (struct statsblob_tpl **)newtpllist;
		tpllist[tpl_id] = (struct statsblob_tpl *)tpl;
		stats_tpl_update_hash(tpllist[tpl_id]);
	} else {
		stats_free(tpl);
		stats_free(tpl_sb);
		if (tpl_mb != NULL) {
			stats_free(tpl_mb->tplname);
			stats_free(tpl_mb);
		}
		tpl_id = -ENOMEM;
	}
	TPL_LIST_WUNLOCK();

	return (tpl_id);
}

int
stats_v1_tpl_add_voistats(uint32_t tpl_id, int32_t voi_id, const char *voi_name,
    enum vsd_dtype voi_dtype, uint32_t nvss, struct voistatspec *vss,
    uint32_t flags)
{
	struct voi *voi;
	struct voistat *tmpstat;
	struct statsblobv1 *tpl_sb;
	struct metablob *tpl_mb;
	int error, i, newstatdataidx, newvoibytes, newvoistatbytes,
	    newvoistatdatabytes, newvoistatmaxid;
	uint32_t nbytes;

	if (voi_id < 0 || voi_dtype == 0 || voi_dtype >= VSD_NUM_DTYPES ||
	    nvss == 0 || vss == NULL)
		return (EINVAL);

	error = nbytes = newvoibytes = newvoistatbytes =
	    newvoistatdatabytes = 0;
	newvoistatmaxid = -1;

	/* Calculate the number of bytes required for the new voistats. */
	for (i = nvss - 1; i >= 0; i--) {
		if (vss[i].stype == 0 || vss[i].stype >= VS_NUM_STYPES ||
		    vss[i].vs_dtype == 0 || vss[i].vs_dtype >= VSD_NUM_DTYPES ||
		    vss[i].iv == NULL || vss[i].vsdsz == 0)
			return (EINVAL);
		if ((int)vss[i].stype > newvoistatmaxid)
			newvoistatmaxid = vss[i].stype;
		newvoistatdatabytes += vss[i].vsdsz;
	}

	if (flags & SB_VOI_RELUPDATE) {
		/* XXXLAS: VOI state bytes may need to vary based on stat types. */
		newvoistatdatabytes += sizeof(struct voistatdata_voistate);
	}
	nbytes += newvoistatdatabytes;

	TPL_LIST_WLOCK();
	if (tpl_id < ntpl) {
		tpl_sb = (struct statsblobv1 *)tpllist[tpl_id]->sb;
		tpl_mb = tpllist[tpl_id]->mb;

		if (voi_id >= NVOIS(tpl_sb) || tpl_sb->vois[voi_id].id == -1) {
			/* Adding a new VOI and associated stats. */
			if (voi_id >= NVOIS(tpl_sb)) {
				/* We need to grow the tpl_sb->vois array. */
				newvoibytes = (voi_id - (NVOIS(tpl_sb) - 1)) *
				    sizeof(struct voi);
				nbytes += newvoibytes;
			}
			newvoistatbytes =
			    (newvoistatmaxid + 1) * sizeof(struct voistat);
		} else {
			/* Adding stats to an existing VOI. */
			if (newvoistatmaxid >
			    tpl_sb->vois[voi_id].voistatmaxid) {
				newvoistatbytes = (newvoistatmaxid -
				    tpl_sb->vois[voi_id].voistatmaxid) *
				    sizeof(struct voistat);
			}
			/* XXXLAS: KPI does not yet support expanding VOIs. */
			error = EOPNOTSUPP;
		}
		nbytes += newvoistatbytes;

		if (!error && newvoibytes > 0) {
			struct voi_meta *voi_meta = tpl_mb->voi_meta;

			voi_meta = stats_realloc(voi_meta, voi_meta == NULL ?
			    0 : NVOIS(tpl_sb) * sizeof(struct voi_meta),
			    (1 + voi_id) * sizeof(struct voi_meta),
			    M_ZERO);

			if (voi_meta == NULL)
				error = ENOMEM;
			else
				tpl_mb->voi_meta = voi_meta;
		}

		if (!error) {
			/* NB: Resizing can change where tpl_sb points. */
			error = stats_v1_blob_expand(&tpl_sb, newvoibytes,
			    newvoistatbytes, newvoistatdatabytes);
		}

		if (!error) {
			tpl_mb->voi_meta[voi_id].name = stats_strdup(voi_name,
			    0);
			if (tpl_mb->voi_meta[voi_id].name == NULL)
				error = ENOMEM;
		}

		if (!error) {
			/* Update the template list with the resized pointer. */
			tpllist[tpl_id]->sb = (struct statsblob *)tpl_sb;

			/* Update the template. */
			voi = &tpl_sb->vois[voi_id];

			if (voi->id < 0) {
				/* VOI is new and needs to be initialised. */
				voi->id = voi_id;
				voi->dtype = voi_dtype;
				voi->stats_off = tpl_sb->stats_off;
				if (flags & SB_VOI_RELUPDATE)
					voi->flags |= VOI_REQSTATE;
			} else {
				/*
				 * XXXLAS: When this else block is written, the
				 * "KPI does not yet support expanding VOIs"
				 * error earlier in this function can be
				 * removed. What is required here is to shuffle
				 * the voistat array such that the new stats for
				 * the voi are contiguous, which will displace
				 * stats for other vois that reside after the
				 * voi being updated. The other vois then need
				 * to have their stats_off adjusted post
				 * shuffle.
				 */
			}

			voi->voistatmaxid = newvoistatmaxid;
			newstatdataidx = 0;

			if (voi->flags & VOI_REQSTATE) {
				/* Initialise the voistate stat in slot 0. */
				tmpstat = BLOB_OFFSET(tpl_sb, voi->stats_off);
				tmpstat->stype = VS_STYPE_VOISTATE;
				tmpstat->flags = 0;
				tmpstat->dtype = VSD_DTYPE_VOISTATE;
				newstatdataidx = tmpstat->dsz =
				    sizeof(struct voistatdata_numeric);
				tmpstat->data_off = tpl_sb->statsdata_off;
			}

			for (i = 0; (uint32_t)i < nvss; i++) {
				tmpstat = BLOB_OFFSET(tpl_sb, voi->stats_off +
				    (vss[i].stype * sizeof(struct voistat)));
				KASSERT(tmpstat->stype < 0, ("voistat %p "
				    "already initialised", tmpstat));
				tmpstat->stype = vss[i].stype;
				tmpstat->flags = vss[i].flags;
				tmpstat->dtype = vss[i].vs_dtype;
				tmpstat->dsz = vss[i].vsdsz;
				tmpstat->data_off = tpl_sb->statsdata_off +
				    newstatdataidx;
				memcpy(BLOB_OFFSET(tpl_sb, tmpstat->data_off),
				    vss[i].iv, vss[i].vsdsz);
				newstatdataidx += vss[i].vsdsz;
			}

			/* Update the template version hash. */
			stats_tpl_update_hash(tpllist[tpl_id]);
			/* XXXLAS: Confirm tpl name/hash pair remains unique. */
		}
	} else
		error = EINVAL;
	TPL_LIST_WUNLOCK();

	return (error);
}

struct statsblobv1 *
stats_v1_blob_alloc(uint32_t tpl_id, uint32_t flags __unused)
{
	struct statsblobv1 *sb;
	int error;

	sb = NULL;

	TPL_LIST_RLOCK();
	if (tpl_id < ntpl) {
		sb = stats_realloc(NULL, 0, tpllist[tpl_id]->sb->maxsz, 0);
		if (sb != NULL) {
			sb->maxsz = tpllist[tpl_id]->sb->maxsz;
			error = stats_v1_blob_init_locked(sb, tpl_id, 0);
		} else
			error = ENOMEM;

		if (error) {
			stats_free(sb);
			sb = NULL;
		}
	}
	TPL_LIST_RUNLOCK();

	return (sb);
}

void
stats_v1_blob_destroy(struct statsblobv1 *sb)
{

	stats_free(sb);
}

int
stats_v1_voistat_fetch_dptr(struct statsblobv1 *sb, int32_t voi_id,
    enum voi_stype stype, enum vsd_dtype *retdtype, struct voistatdata **retvsd,
    size_t *retvsdsz)
{
	struct voi *v;
	struct voistat *vs;

	if (retvsd == NULL || sb == NULL || sb->abi != STATS_ABI_V1 ||
	    voi_id >= NVOIS(sb))
		return (EINVAL);

	v = &sb->vois[voi_id];
	if ((__typeof(v->voistatmaxid))stype > v->voistatmaxid)
		return (EINVAL);

	vs = BLOB_OFFSET(sb, v->stats_off + (stype * sizeof(struct voistat)));
	*retvsd = BLOB_OFFSET(sb, vs->data_off);
	if (retdtype != NULL)
		*retdtype = vs->dtype;
	if (retvsdsz != NULL)
		*retvsdsz = vs->dsz;

	return (0);
}

int
stats_v1_blob_init(struct statsblobv1 *sb, uint32_t tpl_id, uint32_t flags)
{
	int error;

	error = 0;

	TPL_LIST_RLOCK();
	if (sb == NULL || tpl_id >= ntpl) {
		error = EINVAL;
	} else {
		error = stats_v1_blob_init_locked(sb, tpl_id, flags);
	}
	TPL_LIST_RUNLOCK();

	return (error);
}

static inline int
stats_v1_blob_init_locked(struct statsblobv1 *sb, uint32_t tpl_id,
    uint32_t flags __unused)
{
	int error;

	TPL_LIST_RLOCK_ASSERT();
	error = (sb->maxsz >= tpllist[tpl_id]->sb->cursz) ? 0 : EOVERFLOW;
	KASSERT(!error,
	    ("sb %d instead of %d bytes", sb->maxsz, tpllist[tpl_id]->sb->cursz));

	if (!error) {
		memcpy(sb, tpllist[tpl_id]->sb, tpllist[tpl_id]->sb->cursz);
		sb->created = sb->lastrst = stats_sbinuptime();
		sb->tplhash = tpllist[tpl_id]->mb->tplhash;
	}

	return (error);
}

static int
stats_v1_blob_expand(struct statsblobv1 **sbpp, int newvoibytes,
    int newvoistatbytes, int newvoistatdatabytes)
{
	struct statsblobv1 *sb;
	struct voi *tmpvoi;
	struct voistat *tmpvoistat, *voistat_array;
	int error, i, idxnewvois, idxnewvoistats, nbytes, nvoistats;

	KASSERT(newvoibytes % sizeof(struct voi) == 0,
	    ("Bad newvoibytes %d", newvoibytes));
	KASSERT(newvoistatbytes % sizeof(struct voistat) == 0,
	    ("Bad newvoistatbytes %d", newvoistatbytes));

	error = ((newvoibytes % sizeof(struct voi) == 0) &&
	    (newvoistatbytes % sizeof(struct voistat) == 0)) ? 0 : EINVAL;
	sb = *sbpp;
	nbytes = newvoibytes + newvoistatbytes + newvoistatdatabytes;

	/*
	 * XXXLAS: Required until we gain support for flags which alter the
	 * units of size/offset fields in key structs.
	 */
	if (!error && ((((int)sb->cursz) + nbytes) > SB_V1_MAXSZ))
		error = EFBIG;

	if (!error && (sb->cursz + nbytes > sb->maxsz)) {
		/* Need to expand our blob. */
		sb = stats_realloc(sb, sb->maxsz, sb->cursz + nbytes, M_ZERO);
		if (sb != NULL) {
			sb->maxsz = sb->cursz + nbytes;
			*sbpp = sb;
		} else
		    error = ENOMEM;
	}

	if (!error) {
		/*
		 * Shuffle memory within the expanded blob working from the end
		 * backwards, leaving gaps for the new voistat and voistatdata
		 * structs at the beginning of their respective blob regions,
		 * and for the new voi structs at the end of their blob region.
		 */
		memmove(BLOB_OFFSET(sb, sb->statsdata_off + nbytes),
		    BLOB_OFFSET(sb, sb->statsdata_off),
		    sb->cursz - sb->statsdata_off);
		memmove(BLOB_OFFSET(sb, sb->stats_off + newvoibytes +
		    newvoistatbytes), BLOB_OFFSET(sb, sb->stats_off),
		    sb->statsdata_off - sb->stats_off);

		/* First index of new voi/voistat structs to be initialised. */
		idxnewvois = NVOIS(sb);
		idxnewvoistats = (newvoistatbytes / sizeof(struct voistat)) - 1;

		/* Update housekeeping variables and offsets. */
		sb->cursz += nbytes;
		sb->stats_off += newvoibytes;
		sb->statsdata_off += newvoibytes + newvoistatbytes;

		/* XXXLAS: Zeroing not strictly needed but aids debugging. */
		memset(&sb->vois[idxnewvois], '\0', newvoibytes);
		memset(BLOB_OFFSET(sb, sb->stats_off), '\0',
		    newvoistatbytes);
		memset(BLOB_OFFSET(sb, sb->statsdata_off), '\0',
		    newvoistatdatabytes);

		/* Initialise new voi array members and update offsets. */
		for (i = 0; i < NVOIS(sb); i++) {
			tmpvoi = &sb->vois[i];
			if (i >= idxnewvois) {
				tmpvoi->id = tmpvoi->voistatmaxid = -1;
			} else if (tmpvoi->id > -1) {
				tmpvoi->stats_off += newvoibytes +
				    newvoistatbytes;
			}
		}

		/* Initialise new voistat array members and update offsets. */
		nvoistats = (sb->statsdata_off - sb->stats_off) /
		    sizeof(struct voistat);
		voistat_array = BLOB_OFFSET(sb, sb->stats_off);
		for (i = 0; i < nvoistats; i++) {
			tmpvoistat = &voistat_array[i];
			if (i <= idxnewvoistats) {
				tmpvoistat->stype = -1;
			} else if (tmpvoistat->stype > -1) {
				tmpvoistat->data_off += nbytes;
			}
		}
	}

	return (error);
}

static void
stats_v1_blob_finalise(struct statsblobv1 *sb __unused)
{

	/* XXXLAS: Fill this in. */
}

static void
stats_v1_blob_iter(struct statsblobv1 *sb, stats_v1_blob_itercb_t icb,
    void *usrctx, uint32_t flags)
{
	struct voi *v;
	struct voistat *vs;
	struct sb_iter_ctx ctx;
	int i, j, firstvoi;

	ctx.usrctx = usrctx;
	ctx.flags = SB_IT_FIRST_CB;
	firstvoi = 1;

	for (i = 0; i < NVOIS(sb); i++) {
		v = &sb->vois[i];
		ctx.vslot = i;
		ctx.vsslot = -1;
		ctx.flags |= SB_IT_FIRST_VOISTAT;

		if (firstvoi)
			ctx.flags |= SB_IT_FIRST_VOI;
		else if (i == (NVOIS(sb) - 1))
			ctx.flags |= SB_IT_LAST_VOI | SB_IT_LAST_CB;

		if (v->id < 0 && (flags & SB_IT_NULLVOI)) {
			if (icb(sb, v, NULL, &ctx))
				return;
			firstvoi = 0;
			ctx.flags &= ~SB_IT_FIRST_CB;
		}

		/* If NULL voi, v->voistatmaxid == -1 */
		for (j = 0; j <= v->voistatmaxid; j++) {
			vs = &((struct voistat *)BLOB_OFFSET(sb,
			    v->stats_off))[j];
			if (vs->stype < 0 &&
			    !(flags & SB_IT_NULLVOISTAT))
				continue;

			if (j == v->voistatmaxid) {
				ctx.flags |= SB_IT_LAST_VOISTAT;
				if (i == (NVOIS(sb) - 1))
					ctx.flags |=
					    SB_IT_LAST_CB;
			} else
				ctx.flags &= ~SB_IT_LAST_CB;

			ctx.vsslot = j;
			if (icb(sb, v, vs, &ctx))
				return;

			ctx.flags &= ~(SB_IT_FIRST_CB | SB_IT_FIRST_VOISTAT |
			    SB_IT_LAST_VOISTAT);
		}
		ctx.flags &= ~(SB_IT_FIRST_VOI | SB_IT_LAST_VOI);
	}
}

static inline void
stats_voistatdata_tdgst_tostr(enum vsd_dtype voi_dtype __unused,
    const struct voistatdata_tdgst *tdgst, enum vsd_dtype tdgst_dtype,
    size_t tdgst_dsz __unused, enum sb_str_fmt fmt, struct sbuf *buf, int objdump)
{
	const struct ctdth32 *ctd32tree;
	const struct ctdth64 *ctd64tree;
	const struct voistatdata_tdgstctd32 *ctd32;
	const struct voistatdata_tdgstctd64 *ctd64;
	const char *fmtstr;
	uint64_t smplcnt, compcnt;
	int is32bit, qmaxstrlen;
	uint16_t maxctds, curctds;

	switch (tdgst_dtype) {
	case VSD_DTYPE_TDGSTCLUST32:
		smplcnt = CONSTVSD(tdgstclust32, tdgst)->smplcnt;
		compcnt = CONSTVSD(tdgstclust32, tdgst)->compcnt;
		maxctds = ARB_MAXNODES(&CONSTVSD(tdgstclust32, tdgst)->ctdtree);
		curctds = ARB_CURNODES(&CONSTVSD(tdgstclust32, tdgst)->ctdtree);
		ctd32tree = &CONSTVSD(tdgstclust32, tdgst)->ctdtree;
		ctd32 = (objdump ? ARB_CNODE(ctd32tree, 0) :
		    ARB_CMIN(ctdth32, ctd32tree));
		qmaxstrlen = (ctd32 == NULL) ? 1 : Q_MAXSTRLEN(ctd32->mu, 10);
		is32bit = 1;
		ctd64tree = NULL;
		ctd64 = NULL;
		break;
	case VSD_DTYPE_TDGSTCLUST64:
		smplcnt = CONSTVSD(tdgstclust64, tdgst)->smplcnt;
		compcnt = CONSTVSD(tdgstclust64, tdgst)->compcnt;
		maxctds = ARB_MAXNODES(&CONSTVSD(tdgstclust64, tdgst)->ctdtree);
		curctds = ARB_CURNODES(&CONSTVSD(tdgstclust64, tdgst)->ctdtree);
		ctd64tree = &CONSTVSD(tdgstclust64, tdgst)->ctdtree;
		ctd64 = (objdump ? ARB_CNODE(ctd64tree, 0) :
		    ARB_CMIN(ctdth64, ctd64tree));
		qmaxstrlen = (ctd64 == NULL) ? 1 : Q_MAXSTRLEN(ctd64->mu, 10);
		is32bit = 0;
		ctd32tree = NULL;
		ctd32 = NULL;
		break;
	default:
		return;
	}

	switch (fmt) {
	case SB_STRFMT_FREEFORM:
		fmtstr = "smplcnt=%ju, compcnt=%ju, maxctds=%hu, nctds=%hu";
		break;
	case SB_STRFMT_JSON:
	default:
		fmtstr =
		    "\"smplcnt\":%ju,\"compcnt\":%ju,\"maxctds\":%hu,"
		    "\"nctds\":%hu,\"ctds\":[";
		break;
	}
	sbuf_printf(buf, fmtstr, (uintmax_t)smplcnt, (uintmax_t)compcnt,
	    maxctds, curctds);

	while ((is32bit ? NULL != ctd32 : NULL != ctd64)) {
		char qstr[qmaxstrlen];

		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = "\n\t\t\t\t";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = "{";
			break;
		}
		sbuf_cat(buf, fmtstr);

		if (objdump) {
			switch (fmt) {
			case SB_STRFMT_FREEFORM:
				fmtstr = "ctd[%hu].";
				break;
			case SB_STRFMT_JSON:
			default:
				fmtstr = "\"ctd\":%hu,";
				break;
			}
			sbuf_printf(buf, fmtstr, is32bit ?
			    ARB_SELFIDX(ctd32tree, ctd32) :
			    ARB_SELFIDX(ctd64tree, ctd64));
		}

		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = "{mu=";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = "\"mu\":";
			break;
		}
		sbuf_cat(buf, fmtstr);
		Q_TOSTR((is32bit ? ctd32->mu : ctd64->mu), -1, 10, qstr,
		    sizeof(qstr));
		sbuf_cat(buf, qstr);

		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = is32bit ? ",cnt=%u}" : ",cnt=%ju}";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = is32bit ? ",\"cnt\":%u}" : ",\"cnt\":%ju}";
			break;
		}
		sbuf_printf(buf, fmtstr,
		    is32bit ? ctd32->cnt : (uintmax_t)ctd64->cnt);

		if (is32bit)
			ctd32 = (objdump ? ARB_CNODE(ctd32tree,
			    ARB_SELFIDX(ctd32tree, ctd32) + 1) :
			    ARB_CNEXT(ctdth32, ctd32tree, ctd32));
		else
			ctd64 = (objdump ? ARB_CNODE(ctd64tree,
			    ARB_SELFIDX(ctd64tree, ctd64) + 1) :
			    ARB_CNEXT(ctdth64, ctd64tree, ctd64));

		if (fmt == SB_STRFMT_JSON &&
		    (is32bit ? NULL != ctd32 : NULL != ctd64))
			sbuf_putc(buf, ',');
	}
	if (fmt == SB_STRFMT_JSON)
		sbuf_cat(buf, "]");
}

static inline void
stats_voistatdata_hist_tostr(enum vsd_dtype voi_dtype,
    const struct voistatdata_hist *hist, enum vsd_dtype hist_dtype,
    size_t hist_dsz, enum sb_str_fmt fmt, struct sbuf *buf, int objdump)
{
	const struct voistatdata_numeric *bkt_lb, *bkt_ub;
	const char *fmtstr;
	int is32bit;
	uint16_t i, nbkts;

	switch (hist_dtype) {
	case VSD_DTYPE_CRHIST32:
		nbkts = HIST_VSDSZ2NBKTS(crhist32, hist_dsz);
		is32bit = 1;
		break;
	case VSD_DTYPE_DRHIST32:
		nbkts = HIST_VSDSZ2NBKTS(drhist32, hist_dsz);
		is32bit = 1;
		break;
	case VSD_DTYPE_DVHIST32:
		nbkts = HIST_VSDSZ2NBKTS(dvhist32, hist_dsz);
		is32bit = 1;
		break;
	case VSD_DTYPE_CRHIST64:
		nbkts = HIST_VSDSZ2NBKTS(crhist64, hist_dsz);
		is32bit = 0;
		break;
	case VSD_DTYPE_DRHIST64:
		nbkts = HIST_VSDSZ2NBKTS(drhist64, hist_dsz);
		is32bit = 0;
		break;
	case VSD_DTYPE_DVHIST64:
		nbkts = HIST_VSDSZ2NBKTS(dvhist64, hist_dsz);
		is32bit = 0;
		break;
	default:
		return;
	}

	switch (fmt) {
	case SB_STRFMT_FREEFORM:
		fmtstr = "nbkts=%hu, ";
		break;
	case SB_STRFMT_JSON:
	default:
		fmtstr = "\"nbkts\":%hu,";
		break;
	}
	sbuf_printf(buf, fmtstr, nbkts);

	switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = (is32bit ? "oob=%u" : "oob=%ju");
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = (is32bit ? "\"oob\":%u,\"bkts\":[" :
			    "\"oob\":%ju,\"bkts\":[");
			break;
	}
	sbuf_printf(buf, fmtstr, is32bit ? VSD_CONSTHIST_FIELDVAL(hist,
	    hist_dtype, oob) : (uintmax_t)VSD_CONSTHIST_FIELDVAL(hist,
	    hist_dtype, oob));

	for (i = 0; i < nbkts; i++) {
		switch (hist_dtype) {
		case VSD_DTYPE_CRHIST32:
		case VSD_DTYPE_CRHIST64:
			bkt_lb = VSD_CONSTCRHIST_FIELDPTR(hist, hist_dtype,
			    bkts[i].lb);
			if (i < nbkts - 1)
				bkt_ub = VSD_CONSTCRHIST_FIELDPTR(hist,
				    hist_dtype, bkts[i + 1].lb);
			else
				bkt_ub = &numeric_limits[LIM_MAX][voi_dtype];
			break;
		case VSD_DTYPE_DRHIST32:
		case VSD_DTYPE_DRHIST64:
			bkt_lb = VSD_CONSTDRHIST_FIELDPTR(hist, hist_dtype,
			    bkts[i].lb);
			bkt_ub = VSD_CONSTDRHIST_FIELDPTR(hist, hist_dtype,
			    bkts[i].ub);
			break;
		case VSD_DTYPE_DVHIST32:
		case VSD_DTYPE_DVHIST64:
			bkt_lb = bkt_ub = VSD_CONSTDVHIST_FIELDPTR(hist,
			    hist_dtype, bkts[i].val);
			break;
		default:
			break;
		}

		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = "\n\t\t\t\t";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = "{";
			break;
		}
		sbuf_cat(buf, fmtstr);

		if (objdump) {
			switch (fmt) {
			case SB_STRFMT_FREEFORM:
				fmtstr = "bkt[%hu].";
				break;
			case SB_STRFMT_JSON:
			default:
				fmtstr = "\"bkt\":%hu,";
				break;
			}
			sbuf_printf(buf, fmtstr, i);
		}

		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = "{lb=";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = "\"lb\":";
			break;
		}
		sbuf_cat(buf, fmtstr);
		stats_voistatdata_tostr((const struct voistatdata *)bkt_lb,
		    voi_dtype, voi_dtype, sizeof(struct voistatdata_numeric),
		    fmt, buf, objdump);

		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = ",ub=";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = ",\"ub\":";
			break;
		}
		sbuf_cat(buf, fmtstr);
		stats_voistatdata_tostr((const struct voistatdata *)bkt_ub,
		    voi_dtype, voi_dtype, sizeof(struct voistatdata_numeric),
		    fmt, buf, objdump);

		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = is32bit ? ",cnt=%u}" : ",cnt=%ju}";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = is32bit ? ",\"cnt\":%u}" : ",\"cnt\":%ju}";
			break;
		}
		sbuf_printf(buf, fmtstr, is32bit ?
		    VSD_CONSTHIST_FIELDVAL(hist, hist_dtype, bkts[i].cnt) :
		    (uintmax_t)VSD_CONSTHIST_FIELDVAL(hist, hist_dtype,
		    bkts[i].cnt));

		if (fmt == SB_STRFMT_JSON && i < nbkts - 1)
			sbuf_putc(buf, ',');
	}
	if (fmt == SB_STRFMT_JSON)
		sbuf_cat(buf, "]");
}

int
stats_voistatdata_tostr(const struct voistatdata *vsd, enum vsd_dtype voi_dtype,
    enum vsd_dtype vsd_dtype, size_t vsd_sz, enum sb_str_fmt fmt,
    struct sbuf *buf, int objdump)
{
	const char *fmtstr;

	if (vsd == NULL || buf == NULL || voi_dtype >= VSD_NUM_DTYPES ||
	    vsd_dtype >= VSD_NUM_DTYPES || fmt >= SB_STRFMT_NUM_FMTS)
		return (EINVAL);

	switch (vsd_dtype) {
	case VSD_DTYPE_VOISTATE:
		switch (fmt) {
		case SB_STRFMT_FREEFORM:
			fmtstr = "prev=";
			break;
		case SB_STRFMT_JSON:
		default:
			fmtstr = "\"prev\":";
			break;
		}
		sbuf_cat(buf, fmtstr);
		/*
		 * Render prev by passing it as *vsd and voi_dtype as vsd_dtype.
		 */
		stats_voistatdata_tostr(
		    (const struct voistatdata *)&CONSTVSD(voistate, vsd)->prev,
		    voi_dtype, voi_dtype, vsd_sz, fmt, buf, objdump);
		break;
	case VSD_DTYPE_INT_S32:
		sbuf_printf(buf, "%d", vsd->int32.s32);
		break;
	case VSD_DTYPE_INT_U32:
		sbuf_printf(buf, "%u", vsd->int32.u32);
		break;
	case VSD_DTYPE_INT_S64:
		sbuf_printf(buf, "%jd", (intmax_t)vsd->int64.s64);
		break;
	case VSD_DTYPE_INT_U64:
		sbuf_printf(buf, "%ju", (uintmax_t)vsd->int64.u64);
		break;
	case VSD_DTYPE_INT_SLONG:
		sbuf_printf(buf, "%ld", vsd->intlong.slong);
		break;
	case VSD_DTYPE_INT_ULONG:
		sbuf_printf(buf, "%lu", vsd->intlong.ulong);
		break;
	case VSD_DTYPE_Q_S32:
		{
		char qstr[Q_MAXSTRLEN(vsd->q32.sq32, 10)];
		Q_TOSTR((s32q_t)vsd->q32.sq32, -1, 10, qstr, sizeof(qstr));
		sbuf_cat(buf, qstr);
		}
		break;
	case VSD_DTYPE_Q_U32:
		{
		char qstr[Q_MAXSTRLEN(vsd->q32.uq32, 10)];
		Q_TOSTR((u32q_t)vsd->q32.uq32, -1, 10, qstr, sizeof(qstr));
		sbuf_cat(buf, qstr);
		}
		break;
	case VSD_DTYPE_Q_S64:
		{
		char qstr[Q_MAXSTRLEN(vsd->q64.sq64, 10)];
		Q_TOSTR((s64q_t)vsd->q64.sq64, -1, 10, qstr, sizeof(qstr));
		sbuf_cat(buf, qstr);
		}
		break;
	case VSD_DTYPE_Q_U64:
		{
		char qstr[Q_MAXSTRLEN(vsd->q64.uq64, 10)];
		Q_TOSTR((u64q_t)vsd->q64.uq64, -1, 10, qstr, sizeof(qstr));
		sbuf_cat(buf, qstr);
		}
		break;
	case VSD_DTYPE_CRHIST32:
	case VSD_DTYPE_DRHIST32:
	case VSD_DTYPE_DVHIST32:
	case VSD_DTYPE_CRHIST64:
	case VSD_DTYPE_DRHIST64:
	case VSD_DTYPE_DVHIST64:
		stats_voistatdata_hist_tostr(voi_dtype, CONSTVSD(hist, vsd),
		    vsd_dtype, vsd_sz, fmt, buf, objdump);
		break;
	case VSD_DTYPE_TDGSTCLUST32:
	case VSD_DTYPE_TDGSTCLUST64:
		stats_voistatdata_tdgst_tostr(voi_dtype,
		    CONSTVSD(tdgst, vsd), vsd_dtype, vsd_sz, fmt, buf,
		    objdump);
		break;
	default:
		break;
	}

	return (sbuf_error(buf));
}

static void
stats_v1_itercb_tostr_freeform(struct statsblobv1 *sb, struct voi *v,
    struct voistat *vs, struct sb_iter_ctx *ctx)
{
	struct sb_tostrcb_ctx *sctx;
	struct metablob *tpl_mb;
	struct sbuf *buf;
	void *vsd;
	uint8_t dump;

	sctx = ctx->usrctx;
	buf = sctx->buf;
	tpl_mb = sctx->tpl ? sctx->tpl->mb : NULL;
	dump = ((sctx->flags & SB_TOSTR_OBJDUMP) != 0);

	if (ctx->flags & SB_IT_FIRST_CB) {
		sbuf_printf(buf, "struct statsblobv1@%p", sb);
		if (dump) {
			sbuf_printf(buf, ", abi=%hhu, endian=%hhu, maxsz=%hu, "
			    "cursz=%hu, created=%jd, lastrst=%jd, flags=0x%04hx, "
			    "stats_off=%hu, statsdata_off=%hu",
			    sb->abi, sb->endian, sb->maxsz, sb->cursz,
			    sb->created, sb->lastrst, sb->flags, sb->stats_off,
			    sb->statsdata_off);
		}
		sbuf_printf(buf, ", tplhash=%u", sb->tplhash);
	}

	if (ctx->flags & SB_IT_FIRST_VOISTAT) {
		sbuf_printf(buf, "\n\tvois[%hd]: id=%hd", ctx->vslot, v->id);
		if (v->id < 0)
			return;
		sbuf_printf(buf, ", name=\"%s\"", (tpl_mb == NULL) ? "" :
		    tpl_mb->voi_meta[v->id].name);
		if (dump)
		    sbuf_printf(buf, ", flags=0x%04hx, dtype=%s, "
		    "voistatmaxid=%hhd, stats_off=%hu", v->flags,
		    vsd_dtype2name[v->dtype], v->voistatmaxid, v->stats_off);
	}

	if (!dump && vs->stype <= 0)
		return;

	sbuf_printf(buf, "\n\t\tvois[%hd]stat[%hhd]: stype=", v->id, ctx->vsslot);
	if (vs->stype < 0) {
		sbuf_printf(buf, "%hhd", vs->stype);
		return;
	} else
		sbuf_printf(buf, "%s, errs=%hu", vs_stype2name[vs->stype],
		    vs->errs);
	vsd = BLOB_OFFSET(sb, vs->data_off);
	if (dump)
		sbuf_printf(buf, ", flags=0x%04x, dtype=%s, dsz=%hu, "
		    "data_off=%hu", vs->flags, vsd_dtype2name[vs->dtype],
		    vs->dsz, vs->data_off);

	sbuf_cat(buf, "\n\t\t\tvoistatdata: ");
	stats_voistatdata_tostr(vsd, v->dtype, vs->dtype, vs->dsz,
	    sctx->fmt, buf, dump);
}

static void
stats_v1_itercb_tostr_json(struct statsblobv1 *sb, struct voi *v, struct voistat *vs,
    struct sb_iter_ctx *ctx)
{
	struct sb_tostrcb_ctx *sctx;
	struct metablob *tpl_mb;
	struct sbuf *buf;
	const char *fmtstr;
	void *vsd;
	uint8_t dump;

	sctx = ctx->usrctx;
	buf = sctx->buf;
	tpl_mb = sctx->tpl ? sctx->tpl->mb : NULL;
	dump = ((sctx->flags & SB_TOSTR_OBJDUMP) != 0);

	if (ctx->flags & SB_IT_FIRST_CB) {
		sbuf_putc(buf, '{');
		if (dump) {
			sbuf_printf(buf, "\"abi\":%hhu,\"endian\":%hhu,"
			    "\"maxsz\":%hu,\"cursz\":%hu,\"created\":%jd,"
			    "\"lastrst\":%jd,\"flags\":%hu,\"stats_off\":%hu,"
			    "\"statsdata_off\":%hu,", sb->abi,
			    sb->endian, sb->maxsz, sb->cursz, sb->created,
			    sb->lastrst, sb->flags, sb->stats_off,
			    sb->statsdata_off);
		}

		if (tpl_mb == NULL)
			fmtstr = "\"tplname\":%s,\"tplhash\":%u,\"vois\":{";
		else
			fmtstr = "\"tplname\":\"%s\",\"tplhash\":%u,\"vois\":{";

		sbuf_printf(buf, fmtstr, tpl_mb ? tpl_mb->tplname : "null",
		    sb->tplhash);
	}

	if (ctx->flags & SB_IT_FIRST_VOISTAT) {
		if (dump) {
			sbuf_printf(buf, "\"[%d]\":{\"id\":%d", ctx->vslot,
			    v->id);
			if (v->id < 0) {
				sbuf_cat(buf, "},");
				return;
			}
			
			if (tpl_mb == NULL)
				fmtstr = ",\"name\":%s,\"flags\":%hu,"
				    "\"dtype\":\"%s\",\"voistatmaxid\":%hhd,"
				    "\"stats_off\":%hu,";
			else
				fmtstr = ",\"name\":\"%s\",\"flags\":%hu,"
				    "\"dtype\":\"%s\",\"voistatmaxid\":%hhd,"
				    "\"stats_off\":%hu,";

			sbuf_printf(buf, fmtstr, tpl_mb ?
			    tpl_mb->voi_meta[v->id].name : "null", v->flags,
			    vsd_dtype2name[v->dtype], v->voistatmaxid,
			    v->stats_off);
		} else {
			if (tpl_mb == NULL) {
				sbuf_printf(buf, "\"[%hd]\":{", v->id);
			} else {
				sbuf_printf(buf, "\"%s\":{",
				    tpl_mb->voi_meta[v->id].name);
			}
		}
		sbuf_cat(buf, "\"stats\":{");
	}

	vsd = BLOB_OFFSET(sb, vs->data_off);
	if (dump) {
		sbuf_printf(buf, "\"[%hhd]\":", ctx->vsslot);
		if (vs->stype < 0) {
			sbuf_cat(buf, "{\"stype\":-1},");
			return;
		}
		sbuf_printf(buf, "{\"stype\":\"%s\",\"errs\":%hu,\"flags\":%hu,"
		    "\"dtype\":\"%s\",\"data_off\":%hu,\"voistatdata\":{",
		    vs_stype2name[vs->stype], vs->errs, vs->flags,
		    vsd_dtype2name[vs->dtype], vs->data_off);
	} else if (vs->stype > 0) {
		if (tpl_mb == NULL)
			sbuf_printf(buf, "\"[%hhd]\":", vs->stype);
		else
			sbuf_printf(buf, "\"%s\":", vs_stype2name[vs->stype]);
	} else
		return;

	if ((vs->flags & VS_VSDVALID) || dump) {
		if (!dump)
			sbuf_printf(buf, "{\"errs\":%hu,", vs->errs);
		/* Simple non-compound VSD types need a key. */
		if (!vsd_compoundtype[vs->dtype])
			sbuf_cat(buf, "\"val\":");
		stats_voistatdata_tostr(vsd, v->dtype, vs->dtype, vs->dsz,
		    sctx->fmt, buf, dump);
		sbuf_cat(buf, dump ? "}}" : "}");
	} else
		sbuf_cat(buf, dump ? "null}" : "null");

	if (ctx->flags & SB_IT_LAST_VOISTAT)
		sbuf_cat(buf, "}}");

	if (ctx->flags & SB_IT_LAST_CB)
		sbuf_cat(buf, "}}");
	else
		sbuf_putc(buf, ',');
}

static int
stats_v1_itercb_tostr(struct statsblobv1 *sb, struct voi *v, struct voistat *vs,
    struct sb_iter_ctx *ctx)
{
	struct sb_tostrcb_ctx *sctx;

	sctx = ctx->usrctx;

	switch (sctx->fmt) {
	case SB_STRFMT_FREEFORM:
		stats_v1_itercb_tostr_freeform(sb, v, vs, ctx);
		break;
	case SB_STRFMT_JSON:
		stats_v1_itercb_tostr_json(sb, v, vs, ctx);
		break;
	default:
		break;
	}

	return (sbuf_error(sctx->buf));
}

int
stats_v1_blob_tostr(struct statsblobv1 *sb, struct sbuf *buf,
    enum sb_str_fmt fmt, uint32_t flags)
{
	struct sb_tostrcb_ctx sctx;
	uint32_t iflags;

	if (sb == NULL || sb->abi != STATS_ABI_V1 || buf == NULL ||
	    fmt >= SB_STRFMT_NUM_FMTS)
		return (EINVAL);

	sctx.buf = buf;
	sctx.fmt = fmt;
	sctx.flags = flags;

	if (flags & SB_TOSTR_META) {
		if (stats_tpl_fetch(stats_tpl_fetch_allocid(NULL, sb->tplhash),
		    &sctx.tpl))
			return (EINVAL);
	} else
		sctx.tpl = NULL;

	iflags = 0;
	if (flags & SB_TOSTR_OBJDUMP)
		iflags |= (SB_IT_NULLVOI | SB_IT_NULLVOISTAT);
	stats_v1_blob_iter(sb, stats_v1_itercb_tostr, &sctx, iflags);

	return (sbuf_error(buf));
}

static int
stats_v1_itercb_visit(struct statsblobv1 *sb, struct voi *v,
    struct voistat *vs, struct sb_iter_ctx *ctx)
{
	struct sb_visitcb_ctx *vctx;
	struct sb_visit sbv;

	vctx = ctx->usrctx;

	sbv.tplhash = sb->tplhash;
	sbv.voi_id = v->id;
	sbv.voi_dtype = v->dtype;
	sbv.vs_stype = vs->stype;
	sbv.vs_dtype = vs->dtype;
	sbv.vs_dsz = vs->dsz;
	sbv.vs_data = BLOB_OFFSET(sb, vs->data_off);
	sbv.vs_errs = vs->errs;
	sbv.flags = ctx->flags & (SB_IT_FIRST_CB | SB_IT_LAST_CB |
	    SB_IT_FIRST_VOI | SB_IT_LAST_VOI | SB_IT_FIRST_VOISTAT |
	    SB_IT_LAST_VOISTAT);

	return (vctx->cb(&sbv, vctx->usrctx));
}

int
stats_v1_blob_visit(struct statsblobv1 *sb, stats_blob_visitcb_t func,
    void *usrctx)
{
	struct sb_visitcb_ctx vctx;

	if (sb == NULL || sb->abi != STATS_ABI_V1 || func == NULL)
		return (EINVAL);

	vctx.cb = func;
	vctx.usrctx = usrctx;

	stats_v1_blob_iter(sb, stats_v1_itercb_visit, &vctx, 0);

	return (0);
}

static int
stats_v1_icb_reset_voistat(struct statsblobv1 *sb, struct voi *v __unused,
    struct voistat *vs, struct sb_iter_ctx *ctx __unused)
{
	void *vsd;

	if (vs->stype == VS_STYPE_VOISTATE)
		return (0);

	vsd = BLOB_OFFSET(sb, vs->data_off);

	/* Perform the stat type's default reset action. */
	switch (vs->stype) {
	case VS_STYPE_SUM:
		switch (vs->dtype) {
		case VSD_DTYPE_Q_S32:
			Q_SIFVAL(VSD(q32, vsd)->sq32, 0);
			break;
		case VSD_DTYPE_Q_U32:
			Q_SIFVAL(VSD(q32, vsd)->uq32, 0);
			break;
		case VSD_DTYPE_Q_S64:
			Q_SIFVAL(VSD(q64, vsd)->sq64, 0);
			break;
		case VSD_DTYPE_Q_U64:
			Q_SIFVAL(VSD(q64, vsd)->uq64, 0);
			break;
		default:
			bzero(vsd, vs->dsz);
			break;
		}
		break;
	case VS_STYPE_MAX:
		switch (vs->dtype) {
		case VSD_DTYPE_Q_S32:
			Q_SIFVAL(VSD(q32, vsd)->sq32,
			    Q_IFMINVAL(VSD(q32, vsd)->sq32));
			break;
		case VSD_DTYPE_Q_U32:
			Q_SIFVAL(VSD(q32, vsd)->uq32,
			    Q_IFMINVAL(VSD(q32, vsd)->uq32));
			break;
		case VSD_DTYPE_Q_S64:
			Q_SIFVAL(VSD(q64, vsd)->sq64,
			    Q_IFMINVAL(VSD(q64, vsd)->sq64));
			break;
		case VSD_DTYPE_Q_U64:
			Q_SIFVAL(VSD(q64, vsd)->uq64,
			    Q_IFMINVAL(VSD(q64, vsd)->uq64));
			break;
		default:
			memcpy(vsd, &numeric_limits[LIM_MIN][vs->dtype],
			    vs->dsz);
			break;
		}
		break;
	case VS_STYPE_MIN:
		switch (vs->dtype) {
		case VSD_DTYPE_Q_S32:
			Q_SIFVAL(VSD(q32, vsd)->sq32,
			    Q_IFMAXVAL(VSD(q32, vsd)->sq32));
			break;
		case VSD_DTYPE_Q_U32:
			Q_SIFVAL(VSD(q32, vsd)->uq32,
			    Q_IFMAXVAL(VSD(q32, vsd)->uq32));
			break;
		case VSD_DTYPE_Q_S64:
			Q_SIFVAL(VSD(q64, vsd)->sq64,
			    Q_IFMAXVAL(VSD(q64, vsd)->sq64));
			break;
		case VSD_DTYPE_Q_U64:
			Q_SIFVAL(VSD(q64, vsd)->uq64,
			    Q_IFMAXVAL(VSD(q64, vsd)->uq64));
			break;
		default:
			memcpy(vsd, &numeric_limits[LIM_MAX][vs->dtype],
			    vs->dsz);
			break;
		}
		break;
	case VS_STYPE_HIST:
		{
		/* Reset bucket counts. */
		struct voistatdata_hist *hist;
		int i, is32bit;
		uint16_t nbkts;

		hist = VSD(hist, vsd);
		switch (vs->dtype) {
		case VSD_DTYPE_CRHIST32:
			nbkts = HIST_VSDSZ2NBKTS(crhist32, vs->dsz);
			is32bit = 1;
			break;
		case VSD_DTYPE_DRHIST32:
			nbkts = HIST_VSDSZ2NBKTS(drhist32, vs->dsz);
			is32bit = 1;
			break;
		case VSD_DTYPE_DVHIST32:
			nbkts = HIST_VSDSZ2NBKTS(dvhist32, vs->dsz);
			is32bit = 1;
			break;
		case VSD_DTYPE_CRHIST64:
			nbkts = HIST_VSDSZ2NBKTS(crhist64, vs->dsz);
			is32bit = 0;
			break;
		case VSD_DTYPE_DRHIST64:
			nbkts = HIST_VSDSZ2NBKTS(drhist64, vs->dsz);
			is32bit = 0;
			break;
		case VSD_DTYPE_DVHIST64:
			nbkts = HIST_VSDSZ2NBKTS(dvhist64, vs->dsz);
			is32bit = 0;
			break;
		default:
			return (0);
		}

		bzero(VSD_HIST_FIELDPTR(hist, vs->dtype, oob),
		    is32bit ? sizeof(uint32_t) : sizeof(uint64_t));
		for (i = nbkts - 1; i >= 0; i--) {
			bzero(VSD_HIST_FIELDPTR(hist, vs->dtype,
			    bkts[i].cnt), is32bit ? sizeof(uint32_t) :
			    sizeof(uint64_t));
		}
		break;
		}
	case VS_STYPE_TDGST:
		{
		/* Reset sample count centroids array/tree. */
		struct voistatdata_tdgst *tdgst;
		struct ctdth32 *ctd32tree;
		struct ctdth64 *ctd64tree;
		struct voistatdata_tdgstctd32 *ctd32;
		struct voistatdata_tdgstctd64 *ctd64;

		tdgst = VSD(tdgst, vsd);
		switch (vs->dtype) {
		case VSD_DTYPE_TDGSTCLUST32:
			VSD(tdgstclust32, tdgst)->smplcnt = 0;
			VSD(tdgstclust32, tdgst)->compcnt = 0;
			ctd32tree = &VSD(tdgstclust32, tdgst)->ctdtree;
			ARB_INIT(ctd32, ctdlnk, ctd32tree,
			    ARB_MAXNODES(ctd32tree)) {
				ctd32->cnt = 0;
				Q_SIFVAL(ctd32->mu, 0);
			}
#ifdef DIAGNOSTIC
			RB_INIT(&VSD(tdgstclust32, tdgst)->rbctdtree);
#endif
		break;
		case VSD_DTYPE_TDGSTCLUST64:
			VSD(tdgstclust64, tdgst)->smplcnt = 0;
			VSD(tdgstclust64, tdgst)->compcnt = 0;
			ctd64tree = &VSD(tdgstclust64, tdgst)->ctdtree;
			ARB_INIT(ctd64, ctdlnk, ctd64tree,
			    ARB_MAXNODES(ctd64tree)) {
				ctd64->cnt = 0;
				Q_SIFVAL(ctd64->mu, 0);
			}
#ifdef DIAGNOSTIC
			RB_INIT(&VSD(tdgstclust64, tdgst)->rbctdtree);
#endif
		break;
		default:
			return (0);
		}
		break;
		}
	default:
		KASSERT(0, ("Unknown VOI stat type %d", vs->stype));
		break;
	}

	vs->errs = 0;
	vs->flags &= ~VS_VSDVALID;

	return (0);
}

int
stats_v1_blob_snapshot(struct statsblobv1 **dst, size_t dstmaxsz,
    struct statsblobv1 *src, uint32_t flags)
{
	int error;

	if (src != NULL && src->abi == STATS_ABI_V1) {
		error = stats_v1_blob_clone(dst, dstmaxsz, src, flags);
		if (!error) {
			if (flags & SB_CLONE_RSTSRC) {
				stats_v1_blob_iter(src,
				    stats_v1_icb_reset_voistat, NULL, 0);
				src->lastrst = stats_sbinuptime();
			}
			stats_v1_blob_finalise(*dst);
		}
	} else
		error = EINVAL;

	return (error);
}

static inline int
stats_v1_voi_update_max(enum vsd_dtype voi_dtype __unused,
    struct voistatdata *voival, struct voistat *vs, void *vsd)
{
	int error;

	KASSERT(vs->dtype < VSD_NUM_DTYPES,
	    ("Unknown VSD dtype %d", vs->dtype));

	error = 0;

	switch (vs->dtype) {
	case VSD_DTYPE_INT_S32:
		if (VSD(int32, vsd)->s32 < voival->int32.s32) {
			VSD(int32, vsd)->s32 = voival->int32.s32;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_U32:
		if (VSD(int32, vsd)->u32 < voival->int32.u32) {
			VSD(int32, vsd)->u32 = voival->int32.u32;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_S64:
		if (VSD(int64, vsd)->s64 < voival->int64.s64) {
			VSD(int64, vsd)->s64 = voival->int64.s64;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_U64:
		if (VSD(int64, vsd)->u64 < voival->int64.u64) {
			VSD(int64, vsd)->u64 = voival->int64.u64;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_SLONG:
		if (VSD(intlong, vsd)->slong < voival->intlong.slong) {
			VSD(intlong, vsd)->slong = voival->intlong.slong;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_ULONG:
		if (VSD(intlong, vsd)->ulong < voival->intlong.ulong) {
			VSD(intlong, vsd)->ulong = voival->intlong.ulong;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_S32:
		if (Q_QLTQ(VSD(q32, vsd)->sq32, voival->q32.sq32) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q32, vsd)->sq32,
		    voival->q32.sq32)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_U32:
		if (Q_QLTQ(VSD(q32, vsd)->uq32, voival->q32.uq32) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q32, vsd)->uq32,
		    voival->q32.uq32)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_S64:
		if (Q_QLTQ(VSD(q64, vsd)->sq64, voival->q64.sq64) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q64, vsd)->sq64,
		    voival->q64.sq64)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_U64:
		if (Q_QLTQ(VSD(q64, vsd)->uq64, voival->q64.uq64) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q64, vsd)->uq64,
		    voival->q64.uq64)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static inline int
stats_v1_voi_update_min(enum vsd_dtype voi_dtype __unused,
    struct voistatdata *voival, struct voistat *vs, void *vsd)
{
	int error;

	KASSERT(vs->dtype < VSD_NUM_DTYPES,
	    ("Unknown VSD dtype %d", vs->dtype));

	error = 0;

	switch (vs->dtype) {
	case VSD_DTYPE_INT_S32:
		if (VSD(int32, vsd)->s32 > voival->int32.s32) {
			VSD(int32, vsd)->s32 = voival->int32.s32;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_U32:
		if (VSD(int32, vsd)->u32 > voival->int32.u32) {
			VSD(int32, vsd)->u32 = voival->int32.u32;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_S64:
		if (VSD(int64, vsd)->s64 > voival->int64.s64) {
			VSD(int64, vsd)->s64 = voival->int64.s64;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_U64:
		if (VSD(int64, vsd)->u64 > voival->int64.u64) {
			VSD(int64, vsd)->u64 = voival->int64.u64;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_SLONG:
		if (VSD(intlong, vsd)->slong > voival->intlong.slong) {
			VSD(intlong, vsd)->slong = voival->intlong.slong;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_INT_ULONG:
		if (VSD(intlong, vsd)->ulong > voival->intlong.ulong) {
			VSD(intlong, vsd)->ulong = voival->intlong.ulong;
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_S32:
		if (Q_QGTQ(VSD(q32, vsd)->sq32, voival->q32.sq32) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q32, vsd)->sq32,
		    voival->q32.sq32)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_U32:
		if (Q_QGTQ(VSD(q32, vsd)->uq32, voival->q32.uq32) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q32, vsd)->uq32,
		    voival->q32.uq32)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_S64:
		if (Q_QGTQ(VSD(q64, vsd)->sq64, voival->q64.sq64) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q64, vsd)->sq64,
		    voival->q64.sq64)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	case VSD_DTYPE_Q_U64:
		if (Q_QGTQ(VSD(q64, vsd)->uq64, voival->q64.uq64) &&
		    (0 == (error = Q_QCPYVALQ(&VSD(q64, vsd)->uq64,
		    voival->q64.uq64)))) {
			vs->flags |= VS_VSDVALID;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static inline int
stats_v1_voi_update_sum(enum vsd_dtype voi_dtype __unused,
    struct voistatdata *voival, struct voistat *vs, void *vsd)
{
	int error;

	KASSERT(vs->dtype < VSD_NUM_DTYPES,
	    ("Unknown VSD dtype %d", vs->dtype));

	error = 0;

	switch (vs->dtype) {
	case VSD_DTYPE_INT_S32:
		VSD(int32, vsd)->s32 += voival->int32.s32;
		break;
	case VSD_DTYPE_INT_U32:
		VSD(int32, vsd)->u32 += voival->int32.u32;
		break;
	case VSD_DTYPE_INT_S64:
		VSD(int64, vsd)->s64 += voival->int64.s64;
		break;
	case VSD_DTYPE_INT_U64:
		VSD(int64, vsd)->u64 += voival->int64.u64;
		break;
	case VSD_DTYPE_INT_SLONG:
		VSD(intlong, vsd)->slong += voival->intlong.slong;
		break;
	case VSD_DTYPE_INT_ULONG:
		VSD(intlong, vsd)->ulong += voival->intlong.ulong;
		break;
	case VSD_DTYPE_Q_S32:
		error = Q_QADDQ(&VSD(q32, vsd)->sq32, voival->q32.sq32);
		break;
	case VSD_DTYPE_Q_U32:
		error = Q_QADDQ(&VSD(q32, vsd)->uq32, voival->q32.uq32);
		break;
	case VSD_DTYPE_Q_S64:
		error = Q_QADDQ(&VSD(q64, vsd)->sq64, voival->q64.sq64);
		break;
	case VSD_DTYPE_Q_U64:
		error = Q_QADDQ(&VSD(q64, vsd)->uq64, voival->q64.uq64);
		break;
	default:
		error = EINVAL;
		break;
	}

	if (!error)
		vs->flags |= VS_VSDVALID;

	return (error);
}

static inline int
stats_v1_voi_update_hist(enum vsd_dtype voi_dtype, struct voistatdata *voival,
    struct voistat *vs, struct voistatdata_hist *hist)
{
	struct voistatdata_numeric *bkt_lb, *bkt_ub;
	uint64_t *oob64, *cnt64;
	uint32_t *oob32, *cnt32;
	int error, i, found, is32bit, has_ub, eq_only;

	error = 0;

	switch (vs->dtype) {
	case VSD_DTYPE_CRHIST32:
		i = HIST_VSDSZ2NBKTS(crhist32, vs->dsz);
		is32bit = 1;
		has_ub = eq_only = 0;
		oob32 = &VSD(crhist32, hist)->oob;
		break;
	case VSD_DTYPE_DRHIST32:
		i = HIST_VSDSZ2NBKTS(drhist32, vs->dsz);
		is32bit = has_ub = 1;
		eq_only = 0;
		oob32 = &VSD(drhist32, hist)->oob;
		break;
	case VSD_DTYPE_DVHIST32:
		i = HIST_VSDSZ2NBKTS(dvhist32, vs->dsz);
		is32bit = eq_only = 1;
		has_ub = 0;
		oob32 = &VSD(dvhist32, hist)->oob;
		break;
	case VSD_DTYPE_CRHIST64:
		i = HIST_VSDSZ2NBKTS(crhist64, vs->dsz);
		is32bit = has_ub = eq_only = 0;
		oob64 = &VSD(crhist64, hist)->oob;
		break;
	case VSD_DTYPE_DRHIST64:
		i = HIST_VSDSZ2NBKTS(drhist64, vs->dsz);
		is32bit = eq_only = 0;
		has_ub = 1;
		oob64 = &VSD(drhist64, hist)->oob;
		break;
	case VSD_DTYPE_DVHIST64:
		i = HIST_VSDSZ2NBKTS(dvhist64, vs->dsz);
		is32bit = has_ub = 0;
		eq_only = 1;
		oob64 = &VSD(dvhist64, hist)->oob;
		break;
	default:
		return (EINVAL);
	}
	i--; /* Adjust for 0-based array index. */

	/* XXXLAS: Should probably use a better bucket search algorithm. ARB? */
	for (found = 0; i >= 0 && !found; i--) {
		switch (vs->dtype) {
		case VSD_DTYPE_CRHIST32:
			bkt_lb = &VSD(crhist32, hist)->bkts[i].lb;
			cnt32 = &VSD(crhist32, hist)->bkts[i].cnt;
			break;
		case VSD_DTYPE_DRHIST32:
			bkt_lb = &VSD(drhist32, hist)->bkts[i].lb;
			bkt_ub = &VSD(drhist32, hist)->bkts[i].ub;
			cnt32 = &VSD(drhist32, hist)->bkts[i].cnt;
			break;
		case VSD_DTYPE_DVHIST32:
			bkt_lb = &VSD(dvhist32, hist)->bkts[i].val;
			cnt32 = &VSD(dvhist32, hist)->bkts[i].cnt;
			break;
		case VSD_DTYPE_CRHIST64:
			bkt_lb = &VSD(crhist64, hist)->bkts[i].lb;
			cnt64 = &VSD(crhist64, hist)->bkts[i].cnt;
			break;
		case VSD_DTYPE_DRHIST64:
			bkt_lb = &VSD(drhist64, hist)->bkts[i].lb;
			bkt_ub = &VSD(drhist64, hist)->bkts[i].ub;
			cnt64 = &VSD(drhist64, hist)->bkts[i].cnt;
			break;
		case VSD_DTYPE_DVHIST64:
			bkt_lb = &VSD(dvhist64, hist)->bkts[i].val;
			cnt64 = &VSD(dvhist64, hist)->bkts[i].cnt;
			break;
		default:
			return (EINVAL);
		}

		switch (voi_dtype) {
		case VSD_DTYPE_INT_S32:
			if (voival->int32.s32 >= bkt_lb->int32.s32) {
				if ((eq_only && voival->int32.s32 ==
				    bkt_lb->int32.s32) ||
				    (!eq_only && (!has_ub ||
				    voival->int32.s32 < bkt_ub->int32.s32)))
					found = 1;
			}
			break;
		case VSD_DTYPE_INT_U32:
			if (voival->int32.u32 >= bkt_lb->int32.u32) {
				if ((eq_only && voival->int32.u32 ==
				    bkt_lb->int32.u32) ||
				    (!eq_only && (!has_ub ||
				    voival->int32.u32 < bkt_ub->int32.u32)))
					found = 1;
			}
			break;
		case VSD_DTYPE_INT_S64:
			if (voival->int64.s64 >= bkt_lb->int64.s64)
				if ((eq_only && voival->int64.s64 ==
				    bkt_lb->int64.s64) ||
				    (!eq_only && (!has_ub ||
				    voival->int64.s64 < bkt_ub->int64.s64)))
					found = 1;
			break;
		case VSD_DTYPE_INT_U64:
			if (voival->int64.u64 >= bkt_lb->int64.u64)
				if ((eq_only && voival->int64.u64 ==
				    bkt_lb->int64.u64) ||
				    (!eq_only && (!has_ub ||
				    voival->int64.u64 < bkt_ub->int64.u64)))
					found = 1;
			break;
		case VSD_DTYPE_INT_SLONG:
			if (voival->intlong.slong >= bkt_lb->intlong.slong)
				if ((eq_only && voival->intlong.slong ==
				    bkt_lb->intlong.slong) ||
				    (!eq_only && (!has_ub ||
				    voival->intlong.slong <
				    bkt_ub->intlong.slong)))
					found = 1;
			break;
		case VSD_DTYPE_INT_ULONG:
			if (voival->intlong.ulong >= bkt_lb->intlong.ulong)
				if ((eq_only && voival->intlong.ulong ==
				    bkt_lb->intlong.ulong) ||
				    (!eq_only && (!has_ub ||
				    voival->intlong.ulong <
				    bkt_ub->intlong.ulong)))
					found = 1;
			break;
		case VSD_DTYPE_Q_S32:
			if (Q_QGEQ(voival->q32.sq32, bkt_lb->q32.sq32))
				if ((eq_only && Q_QEQ(voival->q32.sq32,
				    bkt_lb->q32.sq32)) ||
				    (!eq_only && (!has_ub ||
				    Q_QLTQ(voival->q32.sq32,
				    bkt_ub->q32.sq32))))
					found = 1;
			break;
		case VSD_DTYPE_Q_U32:
			if (Q_QGEQ(voival->q32.uq32, bkt_lb->q32.uq32))
				if ((eq_only && Q_QEQ(voival->q32.uq32,
				    bkt_lb->q32.uq32)) ||
				    (!eq_only && (!has_ub ||
				    Q_QLTQ(voival->q32.uq32,
				    bkt_ub->q32.uq32))))
					found = 1;
			break;
		case VSD_DTYPE_Q_S64:
			if (Q_QGEQ(voival->q64.sq64, bkt_lb->q64.sq64))
				if ((eq_only && Q_QEQ(voival->q64.sq64,
				    bkt_lb->q64.sq64)) ||
				    (!eq_only && (!has_ub ||
				    Q_QLTQ(voival->q64.sq64,
				    bkt_ub->q64.sq64))))
					found = 1;
			break;
		case VSD_DTYPE_Q_U64:
			if (Q_QGEQ(voival->q64.uq64, bkt_lb->q64.uq64))
				if ((eq_only && Q_QEQ(voival->q64.uq64,
				    bkt_lb->q64.uq64)) ||
				    (!eq_only && (!has_ub ||
				    Q_QLTQ(voival->q64.uq64,
				    bkt_ub->q64.uq64))))
					found = 1;
			break;
		default:
			break;
		}
	}

	if (found) {
		if (is32bit)
			*cnt32 += 1;
		else
			*cnt64 += 1;
	} else {
		if (is32bit)
			*oob32 += 1;
		else
			*oob64 += 1;
	}

	vs->flags |= VS_VSDVALID;
	return (error);
}

static inline int
stats_v1_vsd_tdgst_compress(enum vsd_dtype vs_dtype,
    struct voistatdata_tdgst *tdgst, int attempt)
{
	struct ctdth32 *ctd32tree;
	struct ctdth64 *ctd64tree;
	struct voistatdata_tdgstctd32 *ctd32;
	struct voistatdata_tdgstctd64 *ctd64;
	uint64_t ebits, idxmask;
	uint32_t bitsperidx, nebits;
	int error, idx, is32bit, maxctds, remctds, tmperr;

	error = 0;

	switch (vs_dtype) {
	case VSD_DTYPE_TDGSTCLUST32:
		ctd32tree = &VSD(tdgstclust32, tdgst)->ctdtree;
		if (!ARB_FULL(ctd32tree))
			return (0);
		VSD(tdgstclust32, tdgst)->compcnt++;
		maxctds = remctds = ARB_MAXNODES(ctd32tree);
		ARB_RESET_TREE(ctd32tree, ctdth32, maxctds);
		VSD(tdgstclust32, tdgst)->smplcnt = 0;
		is32bit = 1;
		ctd64tree = NULL;
		ctd64 = NULL;
#ifdef DIAGNOSTIC
		RB_INIT(&VSD(tdgstclust32, tdgst)->rbctdtree);
#endif
		break;
	case VSD_DTYPE_TDGSTCLUST64:
		ctd64tree = &VSD(tdgstclust64, tdgst)->ctdtree;
		if (!ARB_FULL(ctd64tree))
			return (0);
		VSD(tdgstclust64, tdgst)->compcnt++;
		maxctds = remctds = ARB_MAXNODES(ctd64tree);
		ARB_RESET_TREE(ctd64tree, ctdth64, maxctds);
		VSD(tdgstclust64, tdgst)->smplcnt = 0;
		is32bit = 0;
		ctd32tree = NULL;
		ctd32 = NULL;
#ifdef DIAGNOSTIC
		RB_INIT(&VSD(tdgstclust64, tdgst)->rbctdtree);
#endif
		break;
	default:
		return (EINVAL);
	}

	/*
	 * Rebuild the t-digest ARB by pseudorandomly selecting centroids and
	 * re-inserting the mu/cnt of each as a value and corresponding weight.
	 */

	/*
	 * XXXCEM: random(9) is currently rand(3), not random(3).  rand(3)
	 * RAND_MAX happens to be approximately 31 bits (range [0,
	 * 0x7ffffffd]), so the math kinda works out.  When/if this portion of
	 * the code is compiled in userspace, it gets the random(3) behavior,
	 * which has expected range [0, 0x7fffffff].
	 */
#define	bitsperrand 31
	ebits = 0;
	nebits = 0;
	bitsperidx = fls(maxctds);
	KASSERT(bitsperidx <= sizeof(ebits) << 3,
	    ("%s: bitsperidx=%d, ebits=%d",
	    __func__, bitsperidx, (int)(sizeof(ebits) << 3)));
	idxmask = (UINT64_C(1) << bitsperidx) - 1;

	/* Initialise the free list with randomised centroid indices. */
	for (; remctds > 0; remctds--) {
		while (nebits < bitsperidx) {
			ebits |= ((uint64_t)random()) << nebits;
			nebits += bitsperrand;
			if (nebits > (sizeof(ebits) << 3))
				nebits = sizeof(ebits) << 3;
		}
		idx = ebits & idxmask;
		nebits -= bitsperidx;
		ebits >>= bitsperidx;

		/*
		 * Select the next centroid to put on the ARB free list. We
		 * start with the centroid at our randomly selected array index,
		 * and work our way forwards until finding one (the latter
		 * aspect reduces re-insertion randomness, but is good enough).
		 */
		do {
			if (idx >= maxctds)
				idx %= maxctds;

			if (is32bit)
				ctd32 = ARB_NODE(ctd32tree, idx);
			else
				ctd64 = ARB_NODE(ctd64tree, idx);
		} while ((is32bit ? ARB_ISFREE(ctd32, ctdlnk) :
		    ARB_ISFREE(ctd64, ctdlnk)) && ++idx);

		/* Put the centroid on the ARB free list. */
		if (is32bit)
			ARB_RETURNFREE(ctd32tree, ctd32, ctdlnk);
		else
			ARB_RETURNFREE(ctd64tree, ctd64, ctdlnk);
	}

	/*
	 * The free list now contains the randomised indices of every centroid.
	 * Walk the free list from start to end, re-inserting each centroid's
	 * mu/cnt. The tdgst_add() call may or may not consume the free centroid
	 * we re-insert values from during each loop iteration, so we must latch
	 * the index of the next free list centroid before the re-insertion
	 * call. The previous loop above should have left the centroid pointer
	 * pointing to the element at the head of the free list.
	 */
	KASSERT((is32bit ?
	    ARB_FREEIDX(ctd32tree) == ARB_SELFIDX(ctd32tree, ctd32) :
	    ARB_FREEIDX(ctd64tree) == ARB_SELFIDX(ctd64tree, ctd64)),
	    ("%s: t-digest ARB@%p free list bug", __func__,
	    (is32bit ? (void *)ctd32tree : (void *)ctd64tree)));
	remctds = maxctds;
	while ((is32bit ? ctd32 != NULL : ctd64 != NULL)) {
		tmperr = 0;
		if (is32bit) {
			s64q_t x;

			idx = ARB_NEXTFREEIDX(ctd32, ctdlnk);
			/* Cloning a s32q_t into a s64q_t should never fail. */
			tmperr = Q_QCLONEQ(&x, ctd32->mu);
			tmperr = tmperr ? tmperr : stats_v1_vsd_tdgst_add(
			    vs_dtype, tdgst, x, ctd32->cnt, attempt);
			ctd32 = ARB_NODE(ctd32tree, idx);
			KASSERT(ctd32 == NULL || ARB_ISFREE(ctd32, ctdlnk),
			    ("%s: t-digest ARB@%p free list bug", __func__,
			    ctd32tree));
		} else {
			idx = ARB_NEXTFREEIDX(ctd64, ctdlnk);
			tmperr = stats_v1_vsd_tdgst_add(vs_dtype, tdgst,
			    ctd64->mu, ctd64->cnt, attempt);
			ctd64 = ARB_NODE(ctd64tree, idx);
			KASSERT(ctd64 == NULL || ARB_ISFREE(ctd64, ctdlnk),
			    ("%s: t-digest ARB@%p free list bug", __func__,
			    ctd64tree));
		}
		/*
		 * This process should not produce errors, bugs notwithstanding.
		 * Just in case, latch any errors and attempt all re-insertions.
		 */
		error = tmperr ? tmperr : error;
		remctds--;
	}

	KASSERT(remctds == 0, ("%s: t-digest ARB@%p free list bug", __func__,
	    (is32bit ? (void *)ctd32tree : (void *)ctd64tree)));

	return (error);
}

static inline int
stats_v1_vsd_tdgst_add(enum vsd_dtype vs_dtype, struct voistatdata_tdgst *tdgst,
    s64q_t x, uint64_t weight, int attempt)
{
#ifdef DIAGNOSTIC
	char qstr[Q_MAXSTRLEN(x, 10)];
#endif
	struct ctdth32 *ctd32tree;
	struct ctdth64 *ctd64tree;
	void *closest, *cur, *lb, *ub;
	struct voistatdata_tdgstctd32 *ctd32;
	struct voistatdata_tdgstctd64 *ctd64;
	uint64_t cnt, smplcnt, sum, tmpsum;
	s64q_t k, minz, q, z;
	int error, is32bit, n;

	error = 0;
	minz = Q_INI(&z, 0, 0, Q_NFBITS(x));

	switch (vs_dtype) {
	case VSD_DTYPE_TDGSTCLUST32:
		if ((UINT32_MAX - weight) < VSD(tdgstclust32, tdgst)->smplcnt)
			error = EOVERFLOW;
		smplcnt = VSD(tdgstclust32, tdgst)->smplcnt;
		ctd32tree = &VSD(tdgstclust32, tdgst)->ctdtree;
		is32bit = 1;
		ctd64tree = NULL;
		ctd64 = NULL;
		break;
	case VSD_DTYPE_TDGSTCLUST64:
		if ((UINT64_MAX - weight) < VSD(tdgstclust64, tdgst)->smplcnt)
			error = EOVERFLOW;
		smplcnt = VSD(tdgstclust64, tdgst)->smplcnt;
		ctd64tree = &VSD(tdgstclust64, tdgst)->ctdtree;
		is32bit = 0;
		ctd32tree = NULL;
		ctd32 = NULL;
		break;
	default:
		error = EINVAL;
		break;
	}

	if (error)
		return (error);

	/*
	 * Inspired by Ted Dunning's AVLTreeDigest.java
	 */
	do {
#if defined(DIAGNOSTIC)
		KASSERT(attempt < 5,
		    ("%s: Too many attempts", __func__));
#endif
		if (attempt >= 5)
			return (EAGAIN);

		Q_SIFVAL(minz, Q_IFMAXVAL(minz));
		closest = ub = NULL;
		sum = tmpsum = 0;

		if (is32bit)
			lb = cur = (void *)(ctd32 = ARB_MIN(ctdth32, ctd32tree));
		else
			lb = cur = (void *)(ctd64 = ARB_MIN(ctdth64, ctd64tree));

		if (lb == NULL) /* Empty tree. */
			lb = (is32bit ? (void *)ARB_ROOT(ctd32tree) :
			    (void *)ARB_ROOT(ctd64tree));

		/*
		 * Find the set of centroids with minimum distance to x and
		 * compute the sum of counts for all centroids with mean less
		 * than the first centroid in the set.
		 */
		for (; cur != NULL;
		    cur = (is32bit ?
		    (void *)(ctd32 = ARB_NEXT(ctdth32, ctd32tree, ctd32)) :
		    (void *)(ctd64 = ARB_NEXT(ctdth64, ctd64tree, ctd64)))) {
			if (is32bit) {
				cnt = ctd32->cnt;
				KASSERT(Q_PRECEQ(ctd32->mu, x),
				    ("%s: Q_RELPREC(mu,x)=%d", __func__,
				    Q_RELPREC(ctd32->mu, x)));
				/* Ok to assign as both have same precision. */
				z = ctd32->mu;
			} else {
				cnt = ctd64->cnt;
				KASSERT(Q_PRECEQ(ctd64->mu, x),
				    ("%s: Q_RELPREC(mu,x)=%d", __func__,
				    Q_RELPREC(ctd64->mu, x)));
				/* Ok to assign as both have same precision. */
				z = ctd64->mu;
			}

			error = Q_QSUBQ(&z, x);
#if defined(DIAGNOSTIC)
			KASSERT(!error, ("%s: unexpected error %d", __func__,
			    error));
#endif
			if (error)
				return (error);

			z = Q_QABS(z);
			if (Q_QLTQ(z, minz)) {
				minz = z;
				lb = cur;
				sum = tmpsum;
				tmpsum += cnt;
			} else if (Q_QGTQ(z, minz)) {
				ub = cur;
				break;
			}
		}

		cur = (is32bit ?
		    (void *)(ctd32 = (struct voistatdata_tdgstctd32 *)lb) :
		    (void *)(ctd64 = (struct voistatdata_tdgstctd64 *)lb));

		for (n = 0; cur != ub; cur = (is32bit ?
		    (void *)(ctd32 = ARB_NEXT(ctdth32, ctd32tree, ctd32)) :
		    (void *)(ctd64 = ARB_NEXT(ctdth64, ctd64tree, ctd64)))) {
			if (is32bit)
				cnt = ctd32->cnt;
			else
				cnt = ctd64->cnt;

			q = Q_CTRLINI(16);
			if (smplcnt == 1)
				error = Q_QFRACI(&q, 1, 2);
			else
				/* [ sum + ((cnt - 1) / 2) ] / (smplcnt - 1) */
				error = Q_QFRACI(&q, (sum << 1) + cnt - 1,
				    (smplcnt - 1) << 1);
			k = q;
			/* k = q x 4 x samplcnt x attempt */
			error |= Q_QMULI(&k, 4 * smplcnt * attempt);
			/* k = k x (1 - q) */
			error |= Q_QSUBI(&q, 1);
			q = Q_QABS(q);
			error |= Q_QMULQ(&k, q);
#if defined(DIAGNOSTIC)
#if !defined(_KERNEL)
			double q_dbl, k_dbl, q2d, k2d;
			q2d = Q_Q2D(q);
			k2d = Q_Q2D(k);
			q_dbl = smplcnt == 1 ? 0.5 :
			    (sum + ((cnt - 1)  / 2.0)) / (double)(smplcnt - 1);
			k_dbl = 4 * smplcnt * q_dbl * (1.0 - q_dbl) * attempt;
			/*
			 * If the difference between q and q_dbl is greater than
			 * the fractional precision of q, something is off.
			 * NB: q is holding the value of 1 - q
			 */
			q_dbl = 1.0 - q_dbl;
			KASSERT((q_dbl > q2d ? q_dbl - q2d : q2d - q_dbl) <
			    (1.05 * ((double)1 / (double)(1ULL << Q_NFBITS(q)))),
			    ("Q-type q bad precision"));
			KASSERT((k_dbl > k2d ? k_dbl - k2d : k2d - k_dbl) <
			    1.0 + (0.01 * smplcnt),
			    ("Q-type k bad precision"));
#endif /* !_KERNEL */
			KASSERT(!error, ("%s: unexpected error %d", __func__,
			    error));
#endif /* DIAGNOSTIC */
			if (error)
				return (error);
			if ((is32bit && ((ctd32->cnt + weight) <=
			    (uint64_t)Q_GIVAL(k))) ||
			    (!is32bit && ((ctd64->cnt + weight) <=
			    (uint64_t)Q_GIVAL(k)))) {
				n++;
				/* random() produces 31 bits. */
				if (random() < (INT32_MAX / n))
					closest = cur;
			}
			sum += cnt;
		}
	} while (closest == NULL &&
	    (is32bit ? ARB_FULL(ctd32tree) : ARB_FULL(ctd64tree)) &&
	    (error = stats_v1_vsd_tdgst_compress(vs_dtype, tdgst,
	    attempt++)) == 0);

	if (error)
		return (error);

	if (closest != NULL) {
		/* Merge with an existing centroid. */
		if (is32bit) {
			ctd32 = (struct voistatdata_tdgstctd32 *)closest;
			error = Q_QSUBQ(&x, ctd32->mu);
			/*
			 * The following calculation "x / (cnt + weight)"
			 * computes the amount by which to adjust the centroid's
			 * mu value in order to merge in the VOI sample.
			 *
			 * It can underflow (Q_QDIVI() returns ERANGE) when the
			 * user centroids' fractional precision (which is
			 * inherited by 'x') is too low to represent the result.
			 *
			 * A sophisticated approach to dealing with this issue
			 * would minimise accumulation of error by tracking
			 * underflow per centroid and making an adjustment when
			 * a LSB's worth of underflow has accumulated.
			 *
			 * A simpler approach is to let the result underflow
			 * i.e. merge the VOI sample into the centroid without
			 * adjusting the centroid's mu, and rely on the user to
			 * specify their t-digest with sufficient centroid
			 * fractional precision such that the accumulation of
			 * error from multiple underflows is of no material
			 * consequence to the centroid's final value of mu.
			 *
			 * For the moment, the latter approach is employed by
			 * simply ignoring ERANGE here.
			 *
			 * XXXLAS: Per-centroid underflow tracking is likely too
			 * onerous, but it probably makes sense to accumulate a
			 * single underflow error variable across all centroids
			 * and report it as part of the digest to provide
			 * additional visibility into the digest's fidelity.
			 */
			error = error ? error :
			    Q_QDIVI(&x, ctd32->cnt + weight);
			if ((error && error != ERANGE)
			    || (error = Q_QADDQ(&ctd32->mu, x))) {
#ifdef DIAGNOSTIC
				KASSERT(!error, ("%s: unexpected error %d",
				    __func__, error));
#endif
				return (error);
			}
			ctd32->cnt += weight;
			error = ARB_REINSERT(ctdth32, ctd32tree, ctd32) ==
			    NULL ? 0 : EALREADY;
#ifdef DIAGNOSTIC
			RB_REINSERT(rbctdth32,
			    &VSD(tdgstclust32, tdgst)->rbctdtree, ctd32);
#endif
		} else {
			ctd64 = (struct voistatdata_tdgstctd64 *)closest;
			error = Q_QSUBQ(&x, ctd64->mu);
			error = error ? error :
			    Q_QDIVI(&x, ctd64->cnt + weight);
			/* Refer to is32bit ERANGE discussion above. */
			if ((error && error != ERANGE)
			    || (error = Q_QADDQ(&ctd64->mu, x))) {
				KASSERT(!error, ("%s: unexpected error %d",
				    __func__, error));
				return (error);
			}
			ctd64->cnt += weight;
			error = ARB_REINSERT(ctdth64, ctd64tree, ctd64) ==
			    NULL ? 0 : EALREADY;
#ifdef DIAGNOSTIC
			RB_REINSERT(rbctdth64,
			    &VSD(tdgstclust64, tdgst)->rbctdtree, ctd64);
#endif
		}
	} else {
		/*
		 * Add a new centroid. If digest compression is working
		 * correctly, there should always be at least one free.
		 */
		if (is32bit) {
			ctd32 = ARB_GETFREE(ctd32tree, ctdlnk);
#ifdef DIAGNOSTIC
			KASSERT(ctd32 != NULL,
			    ("%s: t-digest@%p has no free centroids",
			    __func__, tdgst));
#endif
			if (ctd32 == NULL)
				return (EAGAIN);
			if ((error = Q_QCPYVALQ(&ctd32->mu, x)))
				return (error);
			ctd32->cnt = weight;
			error = ARB_INSERT(ctdth32, ctd32tree, ctd32) == NULL ?
			    0 : EALREADY;
#ifdef DIAGNOSTIC
			RB_INSERT(rbctdth32,
			    &VSD(tdgstclust32, tdgst)->rbctdtree, ctd32);
#endif
		} else {
			ctd64 = ARB_GETFREE(ctd64tree, ctdlnk);
#ifdef DIAGNOSTIC
			KASSERT(ctd64 != NULL,
			    ("%s: t-digest@%p has no free centroids",
			    __func__, tdgst));
#endif
			if (ctd64 == NULL) /* Should not happen. */
				return (EAGAIN);
			/* Direct assignment ok as both have same type/prec. */
			ctd64->mu = x;
			ctd64->cnt = weight;
			error = ARB_INSERT(ctdth64, ctd64tree, ctd64) == NULL ?
			    0 : EALREADY;
#ifdef DIAGNOSTIC
			RB_INSERT(rbctdth64, &VSD(tdgstclust64,
			    tdgst)->rbctdtree, ctd64);
#endif
		}
	}

	if (is32bit)
		VSD(tdgstclust32, tdgst)->smplcnt += weight;
	else {
		VSD(tdgstclust64, tdgst)->smplcnt += weight;

#ifdef DIAGNOSTIC
		struct rbctdth64 *rbctdtree =
		    &VSD(tdgstclust64, tdgst)->rbctdtree;
		struct voistatdata_tdgstctd64 *rbctd64;
		int i = 0;
		ARB_FOREACH(ctd64, ctdth64, ctd64tree) {
			rbctd64 = (i == 0 ? RB_MIN(rbctdth64, rbctdtree) :
			    RB_NEXT(rbctdth64, rbctdtree, rbctd64));

			if (i >= ARB_CURNODES(ctd64tree)
			    || ctd64 != rbctd64
			    || ARB_MIN(ctdth64, ctd64tree) !=
			       RB_MIN(rbctdth64, rbctdtree)
			    || ARB_MAX(ctdth64, ctd64tree) !=
			       RB_MAX(rbctdth64, rbctdtree)
			    || ARB_LEFTIDX(ctd64, ctdlnk) !=
			       ARB_SELFIDX(ctd64tree, RB_LEFT(rbctd64, rblnk))
			    || ARB_RIGHTIDX(ctd64, ctdlnk) !=
			       ARB_SELFIDX(ctd64tree, RB_RIGHT(rbctd64, rblnk))
			    || ARB_PARENTIDX(ctd64, ctdlnk) !=
			       ARB_SELFIDX(ctd64tree,
			       RB_PARENT(rbctd64, rblnk))) {
				Q_TOSTR(ctd64->mu, -1, 10, qstr, sizeof(qstr));
				printf("ARB ctd=%3d p=%3d l=%3d r=%3d c=%2d "
				    "mu=%s\n",
				    (int)ARB_SELFIDX(ctd64tree, ctd64),
				    ARB_PARENTIDX(ctd64, ctdlnk),
				    ARB_LEFTIDX(ctd64, ctdlnk),
				    ARB_RIGHTIDX(ctd64, ctdlnk),
				    ARB_COLOR(ctd64, ctdlnk),
				    qstr);

				Q_TOSTR(rbctd64->mu, -1, 10, qstr,
				    sizeof(qstr));
				struct voistatdata_tdgstctd64 *parent;
				parent = RB_PARENT(rbctd64, rblnk);
				int rb_color =
					parent == NULL ? 0 :
					RB_LEFT(parent, rblnk) == rbctd64 ?
					(_RB_BITSUP(parent, rblnk) & _RB_L) != 0 :
 					(_RB_BITSUP(parent, rblnk) & _RB_R) != 0;
				printf(" RB ctd=%3d p=%3d l=%3d r=%3d c=%2d "
				    "mu=%s\n",
				    (int)ARB_SELFIDX(ctd64tree, rbctd64),
				    (int)ARB_SELFIDX(ctd64tree,
				      RB_PARENT(rbctd64, rblnk)),
				    (int)ARB_SELFIDX(ctd64tree,
				      RB_LEFT(rbctd64, rblnk)),
				    (int)ARB_SELFIDX(ctd64tree,
				      RB_RIGHT(rbctd64, rblnk)),
				    rb_color,
				    qstr);

				panic("RB@%p and ARB@%p trees differ\n",
				    rbctdtree, ctd64tree);
			}
			i++;
		}
#endif /* DIAGNOSTIC */
	}

	return (error);
}

static inline int
stats_v1_voi_update_tdgst(enum vsd_dtype voi_dtype, struct voistatdata *voival,
    struct voistat *vs, struct voistatdata_tdgst *tdgst)
{
	s64q_t x;
	int error;

	error = 0;

	switch (vs->dtype) {
	case VSD_DTYPE_TDGSTCLUST32:
		/* Use same precision as the user's centroids. */
		Q_INI(&x, 0, 0, Q_NFBITS(
		    ARB_CNODE(&VSD(tdgstclust32, tdgst)->ctdtree, 0)->mu));
		break;
	case VSD_DTYPE_TDGSTCLUST64:
		/* Use same precision as the user's centroids. */
		Q_INI(&x, 0, 0, Q_NFBITS(
		    ARB_CNODE(&VSD(tdgstclust64, tdgst)->ctdtree, 0)->mu));
		break;
	default:
		KASSERT(vs->dtype == VSD_DTYPE_TDGSTCLUST32 ||
		    vs->dtype == VSD_DTYPE_TDGSTCLUST64,
		    ("%s: vs->dtype(%d) != VSD_DTYPE_TDGSTCLUST<32|64>",
		    __func__, vs->dtype));
		return (EINVAL);
	}

	/*
	 * XXXLAS: Should have both a signed and unsigned 'x' variable to avoid
	 * returning EOVERFLOW if the voival would have fit in a u64q_t.
	 */
	switch (voi_dtype) {
	case VSD_DTYPE_INT_S32:
		error = Q_QCPYVALI(&x, voival->int32.s32);
		break;
	case VSD_DTYPE_INT_U32:
		error = Q_QCPYVALI(&x, voival->int32.u32);
		break;
	case VSD_DTYPE_INT_S64:
		error = Q_QCPYVALI(&x, voival->int64.s64);
		break;
	case VSD_DTYPE_INT_U64:
		error = Q_QCPYVALI(&x, voival->int64.u64);
		break;
	case VSD_DTYPE_INT_SLONG:
		error = Q_QCPYVALI(&x, voival->intlong.slong);
		break;
	case VSD_DTYPE_INT_ULONG:
		error = Q_QCPYVALI(&x, voival->intlong.ulong);
		break;
	case VSD_DTYPE_Q_S32:
		error = Q_QCPYVALQ(&x, voival->q32.sq32);
		break;
	case VSD_DTYPE_Q_U32:
		error = Q_QCPYVALQ(&x, voival->q32.uq32);
		break;
	case VSD_DTYPE_Q_S64:
		error = Q_QCPYVALQ(&x, voival->q64.sq64);
		break;
	case VSD_DTYPE_Q_U64:
		error = Q_QCPYVALQ(&x, voival->q64.uq64);
		break;
	default:
		error = EINVAL;
		break;
	}

	if (error ||
	    (error = stats_v1_vsd_tdgst_add(vs->dtype, tdgst, x, 1, 1)))
		return (error);

	vs->flags |= VS_VSDVALID;
	return (0);
}

int
stats_v1_voi_update(struct statsblobv1 *sb, int32_t voi_id,
    enum vsd_dtype voi_dtype, struct voistatdata *voival, uint32_t flags)
{
	struct voi *v;
	struct voistat *vs;
	void *statevsd, *vsd;
	int error, i, tmperr;

	error = 0;

	if (sb == NULL || sb->abi != STATS_ABI_V1 || voi_id >= NVOIS(sb) ||
	    voi_dtype == 0 || voi_dtype >= VSD_NUM_DTYPES || voival == NULL)
		return (EINVAL);
	v = &sb->vois[voi_id];
	if (voi_dtype != v->dtype || v->id < 0 ||
	    ((flags & SB_VOI_RELUPDATE) && !(v->flags & VOI_REQSTATE)))
		return (EINVAL);

	vs = BLOB_OFFSET(sb, v->stats_off);
	if (v->flags & VOI_REQSTATE)
		statevsd = BLOB_OFFSET(sb, vs->data_off);
	else
		statevsd = NULL;

	if (flags & SB_VOI_RELUPDATE) {
		switch (voi_dtype) {
		case VSD_DTYPE_INT_S32:
			voival->int32.s32 +=
			    VSD(voistate, statevsd)->prev.int32.s32;
			break;
		case VSD_DTYPE_INT_U32:
			voival->int32.u32 +=
			    VSD(voistate, statevsd)->prev.int32.u32;
			break;
		case VSD_DTYPE_INT_S64:
			voival->int64.s64 +=
			    VSD(voistate, statevsd)->prev.int64.s64;
			break;
		case VSD_DTYPE_INT_U64:
			voival->int64.u64 +=
			    VSD(voistate, statevsd)->prev.int64.u64;
			break;
		case VSD_DTYPE_INT_SLONG:
			voival->intlong.slong +=
			    VSD(voistate, statevsd)->prev.intlong.slong;
			break;
		case VSD_DTYPE_INT_ULONG:
			voival->intlong.ulong +=
			    VSD(voistate, statevsd)->prev.intlong.ulong;
			break;
		case VSD_DTYPE_Q_S32:
			error = Q_QADDQ(&voival->q32.sq32,
			    VSD(voistate, statevsd)->prev.q32.sq32);
			break;
		case VSD_DTYPE_Q_U32:
			error = Q_QADDQ(&voival->q32.uq32,
			    VSD(voistate, statevsd)->prev.q32.uq32);
			break;
		case VSD_DTYPE_Q_S64:
			error = Q_QADDQ(&voival->q64.sq64,
			    VSD(voistate, statevsd)->prev.q64.sq64);
			break;
		case VSD_DTYPE_Q_U64:
			error = Q_QADDQ(&voival->q64.uq64,
			    VSD(voistate, statevsd)->prev.q64.uq64);
			break;
		default:
			KASSERT(0, ("Unknown VOI data type %d", voi_dtype));
			break;
		}
	}

	if (error)
		return (error);

	for (i = v->voistatmaxid; i > 0; i--) {
		vs = &((struct voistat *)BLOB_OFFSET(sb, v->stats_off))[i];
		if (vs->stype < 0)
			continue;

		vsd = BLOB_OFFSET(sb, vs->data_off);

		switch (vs->stype) {
		case VS_STYPE_MAX:
			tmperr = stats_v1_voi_update_max(voi_dtype, voival,
			    vs, vsd);
			break;
		case VS_STYPE_MIN:
			tmperr = stats_v1_voi_update_min(voi_dtype, voival,
			    vs, vsd);
			break;
		case VS_STYPE_SUM:
			tmperr = stats_v1_voi_update_sum(voi_dtype, voival,
			    vs, vsd);
			break;
		case VS_STYPE_HIST:
			tmperr = stats_v1_voi_update_hist(voi_dtype, voival,
			    vs, vsd);
			break;
		case VS_STYPE_TDGST:
			tmperr = stats_v1_voi_update_tdgst(voi_dtype, voival,
			    vs, vsd);
			break;
		default:
			KASSERT(0, ("Unknown VOI stat type %d", vs->stype));
			break;
		}

		if (tmperr) {
			error = tmperr;
			VS_INCERRS(vs);
		}
	}

	if (statevsd) {
		switch (voi_dtype) {
		case VSD_DTYPE_INT_S32:
			VSD(voistate, statevsd)->prev.int32.s32 =
			    voival->int32.s32;
			break;
		case VSD_DTYPE_INT_U32:
			VSD(voistate, statevsd)->prev.int32.u32 =
			    voival->int32.u32;
			break;
		case VSD_DTYPE_INT_S64:
			VSD(voistate, statevsd)->prev.int64.s64 =
			    voival->int64.s64;
			break;
		case VSD_DTYPE_INT_U64:
			VSD(voistate, statevsd)->prev.int64.u64 =
			    voival->int64.u64;
			break;
		case VSD_DTYPE_INT_SLONG:
			VSD(voistate, statevsd)->prev.intlong.slong =
			    voival->intlong.slong;
			break;
		case VSD_DTYPE_INT_ULONG:
			VSD(voistate, statevsd)->prev.intlong.ulong =
			    voival->intlong.ulong;
			break;
		case VSD_DTYPE_Q_S32:
			error = Q_QCPYVALQ(
			    &VSD(voistate, statevsd)->prev.q32.sq32,
			    voival->q32.sq32);
			break;
		case VSD_DTYPE_Q_U32:
			error = Q_QCPYVALQ(
			    &VSD(voistate, statevsd)->prev.q32.uq32,
			    voival->q32.uq32);
			break;
		case VSD_DTYPE_Q_S64:
			error = Q_QCPYVALQ(
			    &VSD(voistate, statevsd)->prev.q64.sq64,
			    voival->q64.sq64);
			break;
		case VSD_DTYPE_Q_U64:
			error = Q_QCPYVALQ(
			    &VSD(voistate, statevsd)->prev.q64.uq64,
			    voival->q64.uq64);
			break;
		default:
			KASSERT(0, ("Unknown VOI data type %d", voi_dtype));
			break;
		}
	}

	return (error);
}

#ifdef _KERNEL

static void
stats_init(void *arg)
{

}
SYSINIT(stats, SI_SUB_KDTRACE, SI_ORDER_FIRST, stats_init, NULL);

/*
 * Sysctl handler to display the list of available stats templates.
 */
static int
stats_tpl_list_available(SYSCTL_HANDLER_ARGS)
{
	struct sbuf *s;
	int err, i;

	err = 0;

	/* We can tolerate ntpl being stale, so do not take the lock. */
	s = sbuf_new(NULL, NULL, /* +1 per tpl for , */
	    ntpl * (STATS_TPL_MAX_STR_SPEC_LEN + 1), SBUF_FIXEDLEN);
	if (s == NULL)
		return (ENOMEM);

	TPL_LIST_RLOCK();
	for (i = 0; i < ntpl; i++) {
		err = sbuf_printf(s, "%s\"%s\":%u", i ? "," : "",
		    tpllist[i]->mb->tplname, tpllist[i]->mb->tplhash);
		if (err) {
			/* Sbuf overflow condition. */
			err = EOVERFLOW;
			break;
		}
	}
	TPL_LIST_RUNLOCK();

	if (!err) {
		sbuf_finish(s);
		err = sysctl_handle_string(oidp, sbuf_data(s), 0, req);
	}

	sbuf_delete(s);
	return (err);
}

/*
 * Called by subsystem-specific sysctls to report and/or parse the list of
 * templates being sampled and their sampling rates. A stats_tpl_sr_cb_t
 * conformant function pointer must be passed in as arg1, which is used to
 * interact with the subsystem's stats template sample rates list. If arg2 > 0,
 * a zero-initialised allocation of arg2-sized contextual memory is
 * heap-allocated and passed in to all subsystem callbacks made during the
 * operation of stats_tpl_sample_rates().
 *
 * XXXLAS: Assumes templates are never removed, which is currently true but may
 * need to be reworked in future if dynamic template management becomes a
 * requirement e.g. to support kernel module based templates.
 */
int
stats_tpl_sample_rates(SYSCTL_HANDLER_ARGS)
{
	char kvpair_fmt[16], tplspec_fmt[16];
	char tpl_spec[STATS_TPL_MAX_STR_SPEC_LEN];
	char tpl_name[TPL_MAX_NAME_LEN + 2]; /* +2 for "" */
	stats_tpl_sr_cb_t subsys_cb;
	void *subsys_ctx;
	char *buf, *new_rates_usr_str, *tpl_name_p;
	struct stats_tpl_sample_rate *rates;
	struct sbuf *s, _s;
	uint32_t cum_pct, pct, tpl_hash;
	int err, i, off, len, newlen, nrates;

	buf = NULL;
	rates = NULL;
	err = nrates = 0;
	subsys_cb = (stats_tpl_sr_cb_t)arg1;
	KASSERT(subsys_cb != NULL, ("%s: subsys_cb == arg1 == NULL", __func__));
	if (arg2 > 0)
		subsys_ctx = malloc(arg2, M_TEMP, M_WAITOK | M_ZERO);
	else
		subsys_ctx = NULL;

	/* Grab current count of subsystem rates. */
	err = subsys_cb(TPL_SR_UNLOCKED_GET, NULL, &nrates, subsys_ctx);
	if (err)
		goto done;

	/* +1 to ensure we can append '\0' post copyin, +5 per rate for =nnn, */
	len = max(req->newlen + 1, nrates * (STATS_TPL_MAX_STR_SPEC_LEN + 5));

	if (req->oldptr != NULL || req->newptr != NULL)
		buf = malloc(len, M_TEMP, M_WAITOK);

	if (req->oldptr != NULL) {
		if (nrates == 0) {
			/* No rates, so return an empty string via oldptr. */
			err = SYSCTL_OUT(req, "", 1);
			if (err)
				goto done;
			goto process_new;
		}

		s = sbuf_new(&_s, buf, len, SBUF_FIXEDLEN | SBUF_INCLUDENUL);

		/* Grab locked count of, and ptr to, subsystem rates. */
		err = subsys_cb(TPL_SR_RLOCKED_GET, &rates, &nrates,
		    subsys_ctx);
		if (err)
			goto done;
		TPL_LIST_RLOCK();
		for (i = 0; i < nrates && !err; i++) {
			err = sbuf_printf(s, "%s\"%s\":%u=%u", i ? "," : "",
			    tpllist[rates[i].tpl_slot_id]->mb->tplname,
			    tpllist[rates[i].tpl_slot_id]->mb->tplhash,
			    rates[i].tpl_sample_pct);
		}
		TPL_LIST_RUNLOCK();
		/* Tell subsystem that we're done with its rates list. */
		err = subsys_cb(TPL_SR_RUNLOCK, &rates, &nrates, subsys_ctx);
		if (err)
			goto done;

		err = sbuf_finish(s);
		if (err)
			goto done; /* We lost a race for buf to be too small. */

		/* Return the rendered string data via oldptr. */
		err = SYSCTL_OUT(req, sbuf_data(s), sbuf_len(s));
	} else {
		/* Return the upper bound size for buffer sizing requests. */
		err = SYSCTL_OUT(req, NULL, len);
	}

process_new:
	if (err || req->newptr == NULL)
		goto done;

	newlen = req->newlen - req->newidx;
	err = SYSCTL_IN(req, buf, newlen);
	if (err)
		goto done;

	/*
	 * Initialise format strings at run time.
	 *
	 * Write the max template spec string length into the
	 * template_spec=percent key-value pair parsing format string as:
	 *     " %<width>[^=]=%u %n"
	 *
	 * Write the max template name string length into the tplname:tplhash
	 * parsing format string as:
	 *     "%<width>[^:]:%u"
	 *
	 * Subtract 1 for \0 appended by sscanf().
	 */
	sprintf(kvpair_fmt, " %%%zu[^=]=%%u %%n", sizeof(tpl_spec) - 1);
	sprintf(tplspec_fmt, "%%%zu[^:]:%%u", sizeof(tpl_name) - 1);

	/*
	 * Parse each CSV key-value pair specifying a template and its sample
	 * percentage. Whitespace either side of a key-value pair is ignored.
	 * Templates can be specified by name, hash, or name and hash per the
	 * following formats (chars in [] are optional):
	 *    ["]<tplname>["]=<percent>
	 *    :hash=pct
	 *    ["]<tplname>["]:hash=<percent>
	 */
	cum_pct = nrates = 0;
	rates = NULL;
	buf[newlen] = '\0'; /* buf is at least newlen+1 in size. */
	new_rates_usr_str = buf;
	while (isspace(*new_rates_usr_str))
		new_rates_usr_str++; /* Skip leading whitespace. */
	while (*new_rates_usr_str != '\0') {
		tpl_name_p = tpl_name;
		tpl_name[0] = '\0';
		tpl_hash = 0;
		off = 0;

		/*
		 * Parse key-value pair which must perform 2 conversions, then
		 * parse the template spec to extract either name, hash, or name
		 * and hash depending on the three possible spec formats. The
		 * tplspec_fmt format specifier parses name or name and hash
		 * template specs, while the ":%u" format specifier parses
		 * hash-only template specs. If parsing is successfull, ensure
		 * the cumulative sampling percentage does not exceed 100.
		 */
		err = EINVAL;
		if (2 != sscanf(new_rates_usr_str, kvpair_fmt, tpl_spec, &pct,
		    &off))
			break;
		if ((1 > sscanf(tpl_spec, tplspec_fmt, tpl_name, &tpl_hash)) &&
		    (1 != sscanf(tpl_spec, ":%u", &tpl_hash)))
			break;
		if ((cum_pct += pct) > 100)
			break;
		err = 0;

		/* Strip surrounding "" from template name if present. */
		len = strlen(tpl_name);
		if (len > 0) {
			if (tpl_name[len - 1] == '"')
				tpl_name[--len] = '\0';
			if (tpl_name[0] == '"') {
				tpl_name_p++;
				len--;
			}
		}

		rates = stats_realloc(rates, 0, /* oldsz is unused in kernel. */
		    (nrates + 1) * sizeof(*rates), M_WAITOK);
		rates[nrates].tpl_slot_id =
		    stats_tpl_fetch_allocid(len ? tpl_name_p : NULL, tpl_hash);
		if (rates[nrates].tpl_slot_id < 0) {
			err = -rates[nrates].tpl_slot_id;
			break;
		}
		rates[nrates].tpl_sample_pct = pct;
		nrates++;
		new_rates_usr_str += off;
		if (*new_rates_usr_str != ',')
			break; /* End-of-input or malformed. */
		new_rates_usr_str++; /* Move past comma to next pair. */
	}

	if (!err) {
		if ((new_rates_usr_str - buf) < newlen) {
			/* Entire input has not been consumed. */
			err = EINVAL;
		} else {
			/*
			 * Give subsystem the new rates. They'll return the
			 * appropriate rates pointer for us to garbage collect.
			 */
			err = subsys_cb(TPL_SR_PUT, &rates, &nrates,
			    subsys_ctx);
		}
	}
	stats_free(rates);

done:
	free(buf, M_TEMP);
	free(subsys_ctx, M_TEMP);
	return (err);
}

SYSCTL_NODE(_kern, OID_AUTO, stats, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "stats(9) MIB");

SYSCTL_PROC(_kern_stats, OID_AUTO, templates,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    stats_tpl_list_available, "A",
    "list the name/hash of all available stats(9) templates");

#else /* ! _KERNEL */

static void __attribute__ ((constructor))
stats_constructor(void)
{

	pthread_rwlock_init(&tpllistlock, NULL);
}

static void __attribute__ ((destructor))
stats_destructor(void)
{

	pthread_rwlock_destroy(&tpllistlock);
}

#endif /* _KERNEL */
