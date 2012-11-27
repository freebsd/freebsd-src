/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#endif /* _KERNEL */
#include <sys/socket.h>

#include <net/if.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_feature.h>
#include <netinet/ipfw/diffuse_feature_iat_common.h>
#include <netinet/ipfw/diffuse_feature_iatbd.h>
#ifdef _KERNEL
#include <netinet/ipfw/diffuse_private.h>
#include <netinet/ipfw/diffuse_feature_module.h>
#include <netinet/ipfw/ip_fw_private.h>
#endif /* _KERNEL */

#ifndef _KERNEL
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define	KPI_USER_COMPAT
#include <netinet/ipfw/diffuse_user_compat.h> /* Must come after stdlib.h */
#endif

/*
 * Feature inter-arrival time bidirectional computes minimum, mean, maximum and
 * std deviation of inter-arrival time (time between two consecutive packets)
 * for both directions of traffic flow.
 */

/* If we are linked from userspace only put the declaration here. */
#ifdef _KERNEL
DI_IATBD_STAT_NAMES;
#else
DI_IATBD_STAT_NAMES_DECL;
#endif

/* State for jump windows. */
struct di_iatbd_jump_win_state {
	uint32_t	fmin;
	uint32_t	fmax;
	uint64_t	fsum;
	uint64_t	fsqsum;
	uint32_t	bmin;
	uint32_t	bmax;
	uint64_t	bsum;
	uint64_t	bsqsum;
	uint16_t	fcnt;
	uint16_t	bcnt;
	int		jump;
	int		first;
};

/* Per flow data (ring buffer). */
struct di_iatbd_fdata {
	int				full;
	int				changed;
	int				index;
	struct timeval			flast_abs_time;
	struct timeval			blast_abs_time;
	/*
	 * We can only store intervals <= 4294 seconds and stats are signed so
	 * can only be <= 2147 seconds, but default flow timeouts are much
	 * smaller.
	 */
	uint32_t			*iats;
	uint8_t				*dirs;
	struct di_iatbd_jump_win_state	*jstate;
};

static int iatbd_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata);

static int
iatbd_init_instance(struct di_cdata *cdata, struct di_oid *params)
{
	struct di_feature_iatbd_config *conf, *p;

	cdata->conf = malloc(sizeof(struct di_feature_iatbd_config), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (cdata->conf == NULL)
		return (ENOMEM);

	conf = (struct di_feature_iatbd_config *)cdata->conf;

	/* Set default configuration. */
	conf->iat_window = DI_DEFAULT_IATBD_WINDOW;
	conf->iat_partial_window = 0;
	conf->iat_ts_acc = DI_IAT_TS_ACC_NORMAL;
	conf->iat_jump_window = 0;
	conf->iat_prec = 1;

	/* Set configuration. */
	if (params != NULL) {
		p = (struct di_feature_iatbd_config *)params;

		if (p->iat_window != -1)
			conf->iat_window = p->iat_window;

		if (p->iat_partial_window != -1)
			conf->iat_partial_window = p->iat_partial_window;

		if (p->iat_ts_acc != -1)
			conf->iat_ts_acc = p->iat_ts_acc;

		if (p->iat_jump_window != -1)
			conf->iat_jump_window = p->iat_jump_window;

		if (p->iat_prec != -1)
			conf->iat_prec = p->iat_prec;
	}

	return (0);
}

static int
iatbd_destroy_instance(struct di_cdata *cdata)
{

	free(cdata->conf, M_DIFFUSE);

	return (0);
}

static int
iatbd_get_conf(struct di_cdata *cdata, struct di_oid *cbuf, int size_only)
{

	if (!size_only)
		memcpy(cbuf, cdata->conf, sizeof(struct di_feature_iatbd_config));

	return (sizeof(struct di_feature_iatbd_config));
}

static int
iatbd_init_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_iatbd_config *conf;
	struct di_iatbd_fdata *data;

	conf = (struct di_feature_iatbd_config *)cdata->conf;

	fdata->data = malloc(sizeof(struct di_iatbd_fdata), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (fdata->data == NULL)
		return (ENOMEM);

	data = (struct di_iatbd_fdata *)fdata->data;

	if (!conf->iat_jump_window) {
		data->iats = malloc((conf->iat_window - 1) * sizeof(uint32_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
		data->dirs = malloc((conf->iat_window - 1) * sizeof(uint8_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	} else {
		data->jstate = malloc(sizeof(struct di_iatbd_jump_win_state),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	}
	fdata->stats = malloc(DI_IATBD_NO_STATS * sizeof(int32_t), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);

	if (data->iats == NULL || data->dirs == NULL || data->jstate == NULL ||
	    fdata->stats == NULL) {
		free(fdata->stats, M_DIFFUSE);
		free(data->jstate, M_DIFFUSE);
		free(data->dirs, M_DIFFUSE);
		free(data->iats, M_DIFFUSE);
		free(fdata->data, M_DIFFUSE);

		return (ENOMEM);
	}

	iatbd_reset_stats(cdata, fdata);

	return (0);
}

static int
iatbd_destroy_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_iatbd_config *conf;
	struct di_iatbd_fdata *data;

	conf = (struct di_feature_iatbd_config *)cdata->conf;
	data = (struct di_iatbd_fdata *)fdata->data;

	if (!conf->iat_jump_window) {
		free(data->iats, M_DIFFUSE);
		free(data->dirs, M_DIFFUSE);
	} else {
		free(data->jstate, M_DIFFUSE);
	}
	free(fdata->data, M_DIFFUSE);
	free(fdata->stats, M_DIFFUSE);

	return (0);
}

static void
reset_jump_win_state(struct di_iatbd_jump_win_state *state)
{

	state->fmin = 0x7FFFFFFF;
	state->fmax = 0;
	state->fsum = 0;
	state->fsqsum = 0;
	state->bmin = 0x7FFFFFFF;
	state->bmax = 0;
	state->bsum = 0;
	state->bsqsum = 0;
	state->fcnt = 0;
	state->bcnt = 0;
	state->jump = 0;
}

static int
iatbd_update_stats(struct di_cdata *cdata, struct di_fdata *fdata,
    struct mbuf *mbuf, int proto, void *ulp, int dir)
{
	struct di_iatbd_fdata *data;
	struct di_feature_iatbd_config *conf;
	struct timeval t, tv_diff;
	uint32_t diff;

	conf = (struct di_feature_iatbd_config *)cdata->conf;
	data = (struct di_iatbd_fdata *)fdata->data;
	t.tv_sec = t.tv_usec = 0;

#if !defined(__linux__) && defined(_KERNEL)
	/* Get from external source if possible (hardware), see bpf.c. */
	if (mbuf != NULL) {
		struct m_tag *tag = m_tag_locate(mbuf, MTAG_BPF,
		    MTAG_BPF_TIMESTAMP, NULL);
		if (tag != NULL) {
			struct bintime bt = *(struct bintime *)(tag + 1);
			bintime2timeval(&bt, &t);
		}
	}
#endif

	if (t.tv_sec == 0 && t.tv_usec == 0) {
		if (conf->iat_ts_acc == DI_IAT_TS_ACC_ACCURATE)
			getmicrouptime(&t);
		else
			microuptime(&t);
	}

	if ((dir == MATCH_FORWARD &&
	    (data->flast_abs_time.tv_sec > 0 ||
	    data->flast_abs_time.tv_usec > 0)) ||
	    (dir == MATCH_REVERSE &&
	    (data->blast_abs_time.tv_sec > 0 ||
	    data->blast_abs_time.tv_usec > 0))) {
		/* Get inter-arrival time. */
		if (dir == MATCH_FORWARD)
			tv_diff = tv_sub0(&t, &data->flast_abs_time);
		else
			tv_diff = tv_sub0(&t, &data->blast_abs_time);

		diff = (tv_diff.tv_sec * 1000000 + tv_diff.tv_usec) /
		    conf->iat_prec; /* XXX: round() */

		if (!conf->iat_jump_window) {
			data->iats[data->index] = diff;
			data->dirs[data->index++] = dir;
		} else {
			if (dir == MATCH_FORWARD) {
				if (diff < data->jstate->fmin)
					data->jstate->fmin = diff;

				if (diff > data->jstate->fmax)
					data->jstate->fmax = diff;

				data->jstate->fsum += diff;
				data->jstate->fsqsum += (uint64_t)diff * diff;
				data->jstate->fcnt++;
			} else {
				if (diff < data->jstate->bmin)
					data->jstate->bmin = diff;

				if (diff > data->jstate->bmax)
					data->jstate->bmax = diff;

				data->jstate->bsum += diff;
				data->jstate->bsqsum += (uint64_t)diff * diff;
				data->jstate->bcnt++;
			}
			data->index++;

			if (data->index == (conf->iat_window - 1)) {
				data->jstate->jump = 1;
				if (data->jstate->first)
					data->jstate->first = 0;
			}
		}

		if (!data->full && data->index == (conf->iat_window - 1))
			data->full = 1;

		data->changed = 1;
		if (data->index == conf->iat_window - 1)
			data->index = 0;
	}
	if (dir == MATCH_FORWARD)
		data->flast_abs_time = t;
	else
		data->blast_abs_time = t;

	return (0);
}

static int
iatbd_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_iatbd_config *conf;
	struct di_iatbd_fdata *data;
	int i;

	conf = (struct di_feature_iatbd_config *)cdata->conf;
	data = (struct di_iatbd_fdata *)fdata->data;

	fdata->stats[0] = 0x7FFFFFFF;
	fdata->stats[1] = 0;
	fdata->stats[2] = 0;
	fdata->stats[3] = 0;
	fdata->stats[4] = 0x7FFFFFFF;
	fdata->stats[5] = 0;
	fdata->stats[6] = 0;
	fdata->stats[7] = 0;
	if (!conf->iat_jump_window) {
		for (i = 0; i < (conf->iat_window - 1); i++)
			data->iats[i] = 0;
	} else {
		reset_jump_win_state(data->jstate);
		data->jstate->first = 1;
	}
	data->full = 0;
	data->changed = 0;
	data->index = 0;

	return (0);
}

/*
 * XXX: potential overflow of square sum for jumping windows otherwise overflow
 * only possible if flow timeouts would be set much higher than default.
 */
static int
iatbd_get_stats(struct di_cdata *cdata, struct di_fdata *fdata, int32_t **stats)
{
#define	DI_IAT_FMIN	fdata->stats[0]
#define	DI_IAT_FMEAN	fdata->stats[1]
#define	DI_IAT_FMAX	fdata->stats[2]
#define	DI_IAT_FSTDEV	fdata->stats[3]
#define	DI_IAT_BMIN	fdata->stats[4]
#define	DI_IAT_BMEAN	fdata->stats[5]
#define	DI_IAT_BMAX	fdata->stats[6]
#define	DI_IAT_BSTDEV	fdata->stats[7]
	struct di_feature_iatbd_config *conf;
	struct di_iatbd_fdata *data;
	uint64_t bsum, fsum, sqsum;
	uint16_t bcnt, fcnt;
	int i, wsize;

	conf = (struct di_feature_iatbd_config *)cdata->conf;
	data = (struct di_iatbd_fdata *)fdata->data;

	if (!data->full && !(conf->iat_partial_window && data->index > 1))
		return (0); /* Window is not full yet. */

	/* Compute stats only if we need update. */
	if ((!conf->iat_jump_window && data->changed) ||
	    (conf->iat_jump_window && (data->jstate->jump ||
	    (conf->iat_partial_window && data->jstate->first)))) {
		wsize = conf->iat_window - 1;
		if (!data->full)
			wsize = data->index;

		if (!conf->iat_jump_window) {
			bsum = fsum = 0;
			bcnt = fcnt = 0;

			DI_IAT_FMIN = 0x7FFFFFFF;
			DI_IAT_FMAX = 0;
			DI_IAT_BMIN = 0x7FFFFFFF;
			DI_IAT_BMAX = 0;

			for (i = 0; i < wsize; i++) {
				if (data->dirs[i] == MATCH_FORWARD) {
					if (data->iats[i] < DI_IAT_FMIN)
						DI_IAT_FMIN = data->iats[i];

					if (data->iats[i] > DI_IAT_FMAX)
						DI_IAT_FMAX = data->iats[i];

					fsum += data->iats[i];
					fcnt++;
				} else {
					if (data->iats[i] < DI_IAT_BMIN)
						DI_IAT_BMIN = data->iats[i];

					if (data->iats[i] > DI_IAT_BMAX)
						DI_IAT_BMAX = data->iats[i];

					bsum += data->iats[i];
					bcnt++;
				}
			}

			if (fcnt > 0) {
				DI_IAT_FMEAN = fsum / fcnt;
			} else {
				DI_IAT_FMIN = 0;
				DI_IAT_FMEAN = 0;
			}
			if (bcnt > 0) {
				DI_IAT_BMEAN = bsum / bcnt;
			} else {
				DI_IAT_BMIN = 0;
				DI_IAT_BMEAN = 0;
			}

			if (fcnt > 1) {
				sqsum = 0;
				/* Need a second loop or sqsum may overflow. */
				for (i = 0; i < fcnt; i++) {
					if (data->dirs[i] == MATCH_FORWARD) {
						sqsum +=
						    ((int64_t)data->iats[i] -
						    DI_IAT_FMEAN) *
						    ((int64_t)data->iats[i] -
						    DI_IAT_FMEAN);
					}
				}
				DI_IAT_FSTDEV = fixp_sqrt(sqsum / fcnt);
			} else {
				DI_IAT_FSTDEV = 0;
			}
			if (bcnt > 1) {
				sqsum = 0;
				/* Need a second loop or sqsum may overflow. */
				for (i = 0; i < bcnt; i++) {
					if (data->dirs[i] == MATCH_REVERSE) {
						sqsum +=
						    ((int64_t)data->iats[i] -
						    DI_IAT_BMEAN) *
						    ((int64_t)data->iats[i] -
						    DI_IAT_BMEAN);
					}
				}
				DI_IAT_BSTDEV = fixp_sqrt(sqsum / bcnt);
			} else {
				DI_IAT_BSTDEV = 0;
			}

		} else {
			DI_IAT_FMIN = data->jstate->fmin;
			DI_IAT_FMAX = data->jstate->fmax;
			DI_IAT_BMIN = data->jstate->bmin;
			DI_IAT_BMAX = data->jstate->bmax;

			if (data->jstate->fcnt > 0)
				DI_IAT_FMEAN = data->jstate->fsum /
				    data->jstate->fcnt;
			else
				DI_IAT_FMIN = 0;

			if (data->jstate->fcnt > 1) {
				DI_IAT_FSTDEV = fixp_sqrt(
				    (data->jstate->fsqsum -
				    (data->jstate->fsum *
				    data->jstate->fsum /
				    data->jstate->fcnt)) /
				    (data->jstate->fcnt - 1));
			}
			if (data->jstate->bcnt > 0)
				DI_IAT_BMEAN = data->jstate->bsum /
				    data->jstate->bcnt;
			else
				DI_IAT_BMIN = 0;

			if (data->jstate->bcnt > 1) {
				DI_IAT_BSTDEV = fixp_sqrt(
				    (data->jstate->bsqsum -
				    (data->jstate->bsum *
				    data->jstate->bsum /
				    data->jstate->bcnt)) /
				    (data->jstate->bcnt - 1));
			}

			if (data->jstate->jump)
				reset_jump_win_state(data->jstate);
		}
		data->changed = 0;
	}
	*stats = fdata->stats;

	return (DI_IATBD_NO_STATS);
}

static int
iatbd_get_stat(struct di_cdata *cdata, struct di_fdata *fdata, int which,
    int32_t *stat)
{
	int32_t *stats;

	if (which < 0 || which > DI_IATBD_NO_STATS)
		return (-1);

	if (!iatbd_get_stats(cdata, fdata, &stats))
		return (0);

	*stat = stats[which];

	return (1);
}

static int
iatbd_get_stat_names(char **names[])
{

	*names = di_iatbd_stat_names;

	return (DI_IATBD_NO_STATS);
}

static struct di_feature_alg di_iatbd_desc = {
	_FI( .name = )			DI_IATBD_NAME,
	_FI( .type = )			DI_IATBD_TYPE,
	_FI( .ref_count = )		0,

	_FI( .init_instance = )		iatbd_init_instance,
	_FI( .destroy_instance = )	iatbd_destroy_instance,
	_FI( .init_stats = )		iatbd_init_stats,
	_FI( .destroy_stats = )		iatbd_destroy_stats,
	_FI( .update_stats = )		iatbd_update_stats,
	_FI( .reset_stats = )		iatbd_reset_stats,
	_FI( .get_stats = )		iatbd_get_stats,
	_FI( .get_stat = )		iatbd_get_stat,
	_FI( .get_stat_names = )	iatbd_get_stat_names,
	_FI( .get_conf = )		iatbd_get_conf,
};

DECLARE_DIFFUSE_FEATURE_MODULE(iatbd, &di_iatbd_desc);
