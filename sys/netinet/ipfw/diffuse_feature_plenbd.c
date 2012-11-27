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
#include <netinet/ipfw/diffuse_feature_plenbd.h>
#include <netinet/ipfw/diffuse_feature_plen_common.h>
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
 * Feature packet length bidirectional computes minimum, mean, maximum and std
 * deviation of packet length (length = length of IP data or length of UDP/TCP
 * data) for both directions of traffic flow.
 */

/* If we are linked from userspace only put the declaration here. */
#ifdef _KERNEL
DI_PLENBD_STAT_NAMES;
#else
DI_PLENBD_STAT_NAMES_DECL;
#endif

/* State for jump windows. */
struct di_plenbd_jump_win_state {
	uint16_t	fmin;
	uint16_t	fmax;
	uint32_t	fsum;
	uint64_t	fsqsum;
	uint16_t	bmin;
	uint16_t	bmax;
	uint32_t	bsum;
	uint64_t	bsqsum;
	uint16_t	fcnt;
	uint16_t	bcnt;
	int		jump;
	int		first;
};

/* Per flow data (ring buffer). */
struct di_plenbd_fdata {
	int				full;
	int				changed;
	int				index;
	uint16_t			*plens;
	uint8_t				*dirs;
	struct di_plenbd_jump_win_state	*jstate;
};

static int plenbd_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata);

static int
plenbd_init_instance(struct di_cdata *cdata, struct di_oid *params)
{
	struct di_feature_plenbd_config *conf, *p;

	cdata->conf = malloc(sizeof(struct di_feature_plenbd_config), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (cdata->conf == NULL)
		return (ENOMEM);

	conf = (struct di_feature_plenbd_config *)cdata->conf;

	/* Set default configuration. */
	conf->plen_window = DI_DEFAULT_PLENBD_WINDOW;
	conf->plen_partial_window = 0;
	conf->plen_len_type = DI_PLEN_LEN_FULL;
	conf->plen_jump_window = 0;

	/* Set configuration. */
	if (params != NULL) {
		p = (struct di_feature_plenbd_config *)params;

		if (p->plen_window != -1)
			conf->plen_window = p->plen_window;

		if (p->plen_partial_window != -1)
			conf->plen_partial_window = p->plen_partial_window;

		if (p->plen_len_type != -1)
			conf->plen_len_type = p->plen_len_type;

		if (p->plen_jump_window != -1)
			conf->plen_jump_window = p->plen_jump_window;
	}

	return (0);
}

static int
plenbd_destroy_instance(struct di_cdata *cdata)
{

	free(cdata->conf, M_DIFFUSE);

	return (0);
}

static int
plenbd_get_conf(struct di_cdata *cdata, struct di_oid *cbuf, int size_only)
{

	if (!size_only)
		memcpy(cbuf, cdata->conf,
		    sizeof(struct di_feature_plenbd_config));

	return (sizeof(struct di_feature_plenbd_config));
}

static int
plenbd_init_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_plenbd_config *conf;
	struct di_plenbd_fdata *data;

	conf = (struct di_feature_plenbd_config *)cdata->conf;

	fdata->data = malloc(sizeof(struct di_plenbd_fdata), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (fdata->data == NULL)
		return (ENOMEM);

	data = (struct di_plenbd_fdata *)fdata->data;

	if (!conf->plen_jump_window) {
		data->plens = malloc(conf->plen_window * sizeof(uint16_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
		data->dirs = malloc(conf->plen_window * sizeof(uint8_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	} else {
		data->jstate = malloc(sizeof(struct di_plenbd_jump_win_state),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	}
	fdata->stats = malloc(DI_PLENBD_NO_STATS * sizeof(int32_t), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);

	if (data->plens == NULL || data->dirs == NULL || data->jstate == NULL ||
	    fdata->stats == NULL) {
		free(fdata->stats, M_DIFFUSE);
		free(data->jstate, M_DIFFUSE);
		free(data->dirs, M_DIFFUSE);
		free(data->plens, M_DIFFUSE);
		free(fdata->data, M_DIFFUSE);

		return (ENOMEM);
	}

	plenbd_reset_stats(cdata, fdata);

	return (0);
}

static int
plenbd_destroy_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_plenbd_config *conf;
	struct di_plenbd_fdata *data;

	conf  = (struct di_feature_plenbd_config *)cdata->conf;
	data = (struct di_plenbd_fdata *)fdata->data;

	if (!conf->plen_jump_window) {
		free(data->plens, M_DIFFUSE);
		free(data->dirs, M_DIFFUSE);
	} else {
		free(data->jstate, M_DIFFUSE);
	}
	free(fdata->data, M_DIFFUSE);
	free(fdata->stats, M_DIFFUSE);

	return (0);
}

static void
reset_jump_win_state(struct di_plenbd_jump_win_state *state)
{

	state->fmin = 0xFFFF;
	state->fmax = 0;
	state->fsum = 0;
	state->fsqsum = 0;
	state->bmin = 0xFFFF;
	state->bmax = 0;
	state->bsum = 0;
	state->bsqsum = 0;
	state->fcnt = 0;
	state->bcnt = 0;
	state->jump = 0;
}

static int
plenbd_update_stats(struct di_cdata *cdata, struct di_fdata *fdata, struct mbuf *mbuf,
    int proto, void *ulp, int dir)
{
	struct di_feature_plenbd_config *conf;
	struct di_plenbd_fdata *data;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *tcp;
	uint16_t plen;
	int iplen;

	conf = (struct di_feature_plenbd_config *)cdata->conf;
	data = (struct di_plenbd_fdata *)fdata->data;
	ip = mtod(mbuf, struct ip *);
	plen = 0;

	/* Length of data in IP. */
	if (conf->plen_len_type == DI_PLEN_LEN_FULL) {
		plen = m_length(mbuf, NULL);
	} else {
		iplen = 0;
		if (ip->ip_v == 6) {
			ip6 = (struct ip6_hdr *)ip;
			iplen = ntohs(ip6->ip6_plen);
		} else {
			iplen = ntohs(ip->ip_len) - ip->ip_hl * 4;
		}
		if (conf->plen_len_type == DI_PLEN_LEN_IPDATA) {
			plen = iplen;
		} else {
			/* Length of data in UDP or TCP. */
			if (proto != 0 && ulp != NULL) {
				if (proto == IPPROTO_TCP) {
					tcp = (struct tcphdr *)ulp;
					plen = iplen - tcp->th_off * 4;
				} else if (proto == IPPROTO_UDP) {
					plen = iplen - sizeof(struct udphdr);
				}
			}
		}
	}

	if (!conf->plen_jump_window) {
		data->plens[data->index] = plen;
		data->dirs[data->index++] = dir;
	} else {
		if (dir == MATCH_FORWARD) {
			if (plen < data->jstate->fmin)
				data->jstate->fmin = plen;

			if (plen > data->jstate->fmax)
				data->jstate->fmax = plen;

			data->jstate->fsum += plen;
			data->jstate->fsqsum += (uint64_t)plen * plen;
			data->jstate->fcnt++;
		} else {
			if (plen < data->jstate->bmin)
				data->jstate->bmin = plen;

			if (plen > data->jstate->bmax)
				data->jstate->bmax = plen;

			data->jstate->bsum += plen;
			data->jstate->bsqsum += (uint64_t)plen * plen;
			data->jstate->bcnt++;
		}
		data->index++;

		if (data->index == conf->plen_window) {
			data->jstate->jump = 1;
			if (data->jstate->first)
				data->jstate->first = 0;
		}
	}

	if (!data->full && data->index == conf->plen_window)
		data->full = 1;

	data->changed = 1;
	if (data->index == conf->plen_window)
		data->index = 0;

	return (0);
}

static int
plenbd_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_plenbd_config *conf;
	struct di_plenbd_fdata *data;
	int i;

	conf = (struct di_feature_plenbd_config *)cdata->conf;
	data = (struct di_plenbd_fdata *)fdata->data;

	fdata->stats[0] = 0x7FFFFFFF;
	fdata->stats[1] = 0;
	fdata->stats[2] = 0;
	fdata->stats[3] = 0;
	fdata->stats[4] = 0;
	fdata->stats[5] = 0x7FFFFFFF;
	fdata->stats[6] = 0;
	fdata->stats[7] = 0;
	fdata->stats[8] = 0;
	fdata->stats[9] = 0;

	if (!conf->plen_jump_window) {
		for (i = 0; i < conf->plen_window; i++) {
			data->plens[i] = 0;
			data->dirs[i] = 0;
		}
	} else {
		reset_jump_win_state(data->jstate);
		data->jstate->first = 1;
	}
	data->full = 0;
	data->changed = 0;
	data->index = 0;

	return (0);
}

static int
plenbd_get_stats(struct di_cdata *cdata, struct di_fdata *fdata,
    int32_t **stats)
{
#define	DI_PLEN_FMIN	fdata->stats[0]
#define	DI_PLEN_FMEAN	fdata->stats[1]
#define	DI_PLEN_FMAX	fdata->stats[2]
#define	DI_PLEN_FSTDEV	fdata->stats[3]
#define	DI_PLEN_FSUM	fdata->stats[4]
#define	DI_PLEN_BMIN	fdata->stats[5]
#define	DI_PLEN_BMEAN	fdata->stats[6]
#define	DI_PLEN_BMAX	fdata->stats[7]
#define	DI_PLEN_BSTDEV	fdata->stats[8]
#define	DI_PLEN_BSUM	fdata->stats[9]
	struct di_feature_plenbd_config *conf;
	struct di_plenbd_fdata *data;
	uint32_t bcnt, bsum, fcnt, fsum;
	uint64_t bsqsum, fsqsum;
	int i, wsize;

	conf = (struct di_feature_plenbd_config *)cdata->conf;
	data = (struct di_plenbd_fdata *)fdata->data;
	bcnt = bsum = fcnt = fsum = 0;
	bsqsum = fsqsum = 0;

	if (!data->full && !(conf->plen_partial_window && data->index > 0))
		return (0); /* Window is not full yet. */

	/* Compute stats only if we need update. */
	if ((!conf->plen_jump_window && data->changed) ||
	    (conf->plen_jump_window && (data->jstate->jump ||
	    (conf->plen_partial_window && data->jstate->first)))) {
		wsize = conf->plen_window;
		if (!data->full)
			wsize = data->index;

		if (!conf->plen_jump_window) {
			DI_PLEN_FMIN = 0x7FFFFFFF;
			DI_PLEN_FMAX = 0;
			DI_PLEN_BMIN = 0x7FFFFFFF;
			DI_PLEN_BMAX = 0;
			for (i = 0; i < wsize; i++) {
				if (data->dirs[i] == MATCH_FORWARD) {
					if (data->plens[i] < DI_PLEN_FMIN)
						DI_PLEN_FMIN = data->plens[i];

					if (data->plens[i] > DI_PLEN_FMAX)
						DI_PLEN_FMAX = data->plens[i];

					fsum += data->plens[i];
					fsqsum += (uint64_t)data->plens[i] *
					    data->plens[i];
					fcnt++;
				} else {
					if (data->plens[i] < DI_PLEN_BMIN)
						DI_PLEN_BMIN = data->plens[i];

					if (data->plens[i] > DI_PLEN_BMAX)
						DI_PLEN_BMAX = data->plens[i];

					bsum += data->plens[i];
					bsqsum += (uint64_t)data->plens[i] *
					    data->plens[i];
					bcnt++;
				}
			}
		} else {
			DI_PLEN_FMIN = data->jstate->fmin;
			DI_PLEN_FMAX = data->jstate->fmax;
			DI_PLEN_BMIN = data->jstate->bmin;
			DI_PLEN_BMAX = data->jstate->bmax;
			fsum = data->jstate->fsum;
			fsqsum = data->jstate->fsqsum;
			bsum = data->jstate->bsum;
			bsqsum = data->jstate->bsqsum;
			fcnt = data->jstate->fcnt;
			bcnt = data->jstate->bcnt;

			if (data->jstate->jump)
				reset_jump_win_state(data->jstate);
		}

		DI_PLEN_FSUM = fsum;
		DI_PLEN_BSUM = bsum;

		if (fcnt > 0) {
			DI_PLEN_FMEAN = fsum / fcnt;
		} else {
			DI_PLEN_FMIN = 0;
			DI_PLEN_FMEAN = 0;
		}
		if (fcnt > 1) {
			DI_PLEN_FSTDEV = fixp_sqrt((fsqsum -
			    ((uint64_t)fsum * fsum / fcnt)) / (fcnt - 1));
		} else {
			DI_PLEN_FSTDEV = 0;
		}
		if (bcnt > 0) {
			DI_PLEN_BMEAN = bsum / bcnt;
		} else {
			DI_PLEN_BMIN = 0;
			DI_PLEN_BMEAN = 0;
		}
		if (bcnt > 1) {
			DI_PLEN_BSTDEV = fixp_sqrt((bsqsum -
			    ((uint64_t)bsum * bsum / bcnt)) / (bcnt - 1));
		} else {
			DI_PLEN_BSTDEV = 0;
		}

		data->changed = 0;
	}
	*stats = fdata->stats;

	return (DI_PLENBD_NO_STATS);
}

static int
plenbd_get_stat(struct di_cdata *cdata, struct di_fdata *fdata, int which, int32_t *stat)
{
	int32_t *stats;

	if (which < 0 || which > DI_PLENBD_NO_STATS)
		return (-1);

	if (!plenbd_get_stats(cdata, fdata, &stats))
		return (0);

	*stat = stats[which];

	return (1);
}

static int
plenbd_get_stat_names(char **names[])
{

	*names = di_plenbd_stat_names;

	return (DI_PLENBD_NO_STATS);
}

static struct di_feature_alg di_plenbd_desc = {
	_FI( .name = )			DI_PLENBD_NAME,
	_FI( .type = )			DI_PLENBD_TYPE,
	_FI( .ref_count = )		0,

	_FI( .init_instance = )		plenbd_init_instance,
	_FI( .destroy_instance = )	plenbd_destroy_instance,
	_FI( .init_stats = )		plenbd_init_stats,
	_FI( .destroy_stats = )		plenbd_destroy_stats,
	_FI( .update_stats = )		plenbd_update_stats,
	_FI( .reset_stats = )		plenbd_reset_stats,
	_FI( .get_stats = )		plenbd_get_stats,
	_FI( .get_stat = )		plenbd_get_stat,
	_FI( .get_stat_names = )	plenbd_get_stat_names,
	_FI( .get_conf = )		plenbd_get_conf,
};

DECLARE_DIFFUSE_FEATURE_MODULE(plenbd, &di_plenbd_desc);
