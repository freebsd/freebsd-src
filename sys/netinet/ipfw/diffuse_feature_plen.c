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

/*
 * DIFFUSE packet length feature.
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
#include <netinet/ipfw/diffuse_feature_plen.h>
#include <netinet/ipfw/diffuse_feature_plen_common.h>
#ifdef _KERNEL
#include <netinet/ipfw/diffuse_private.h>
#include <netinet/ipfw/diffuse_feature_module.h>
#endif /* _KERNEL */

#ifndef _KERNEL
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#define	KPI_USER_COMPAT
#include <netinet/ipfw/diffuse_user_compat.h> /* Must come after stdlib.h */
#endif

/*
 * Feature packet length computes minimum, mean, maximum and std deviation of
 * packet length (length = length of IP data or length of UDP/TCP data).
 */

/* If we are linked from userspace only put the declaration here. */
#ifdef _KERNEL
DI_PLEN_STAT_NAMES;
#else
DI_PLEN_STAT_NAMES_DECL;
#endif

/* State for jump windows. */
struct di_plen_jump_win_state {
	uint16_t	min;
	uint16_t	max;
	uint32_t	sum;
	uint64_t	sqsum;
	int		jump;
	int		first;
};

/* Per flow data (ring buffer). */
struct di_plen_fdata {
	int				full;
	int				changed;
	int				index;
	uint16_t			*plens;
	struct di_plen_jump_win_state	*jstate;
};

static int plen_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata);

static int
plen_init_instance(struct di_cdata *cdata, struct di_oid *params)
{
	struct di_feature_plen_config *conf, *p;

	cdata->conf = malloc(sizeof(struct di_feature_plen_config), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (cdata->conf == NULL)
		return (ENOMEM);

	conf = (struct di_feature_plen_config *)cdata->conf;

	/* Set default configuration. */
	conf->plen_window = DI_DEFAULT_PLEN_WINDOW;
	conf->plen_partial_window = 0;
	conf->plen_len_type = DI_PLEN_LEN_FULL;
	conf->plen_jump_window = 0;

	/* Set configuration. */
	if (params != NULL) {
		p = (struct di_feature_plen_config *)params;

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
plen_destroy_instance(struct di_cdata *cdata)
{

	free(cdata->conf, M_DIFFUSE);

	return (0);
}

static int
plen_get_conf(struct di_cdata *cdata, struct di_oid *cbuf, int size_only)
{

	if (!size_only)
		memcpy(cbuf, cdata->conf,
		    sizeof(struct di_feature_plen_config));

	return (sizeof(struct di_feature_plen_config));
}

static int
plen_init_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_plen_config *conf;
	struct di_plen_fdata *data;

	conf = (struct di_feature_plen_config *)cdata->conf;

	fdata->data = malloc(sizeof(struct di_plen_fdata), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (fdata->data == NULL)
		return (ENOMEM);

	data = (struct di_plen_fdata *)fdata->data;

	if (!conf->plen_jump_window) {
		data->plens = malloc(conf->plen_window * sizeof(uint16_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	} else {
		data->jstate = malloc(sizeof(struct di_plen_jump_win_state),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	}
	fdata->stats = malloc(DI_PLEN_NO_STATS * sizeof(int32_t), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);

	if (data->plens == NULL || data->jstate == NULL ||
	    fdata->stats == NULL) {
		free(fdata->stats, M_DIFFUSE);
		free(data->jstate, M_DIFFUSE);
		free(data->plens, M_DIFFUSE);
		free(fdata->data, M_DIFFUSE);

		return (ENOMEM);
	}

	plen_reset_stats(cdata, fdata);

	return (0);
}

static int
plen_destroy_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_plen_fdata *data;
	struct di_feature_plen_config *conf;

	conf = (struct di_feature_plen_config *)cdata->conf;
	data = (struct di_plen_fdata *)fdata->data;

	if (!conf->plen_jump_window)
		free(data->plens, M_DIFFUSE);
	else
		free(data->jstate, M_DIFFUSE);

	free(fdata->data, M_DIFFUSE);
	free(fdata->stats, M_DIFFUSE);

	return (0);
}

static void
reset_jump_win_state(struct di_plen_jump_win_state *state)
{

	state->min = 0xFFFF;
	state->max = 0;
	state->sum = 0;
	state->sqsum = 0;
	state->jump = 0;
}

static int
plen_update_stats(struct di_cdata *cdata, struct di_fdata *fdata,
    struct mbuf *mbuf, int proto, void *ulp, int dir)
{
	struct di_feature_plen_config *conf;
	struct di_plen_fdata *data;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *tcp;
	uint16_t plen;
	int iplen;

	conf = (struct di_feature_plen_config *)cdata->conf;
	data = (struct di_plen_fdata *)fdata->data;
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
		data->plens[data->index++] = plen;
	} else {
		if (plen < data->jstate->min)
			data->jstate->min = plen;

		if (plen > data->jstate->max)
			data->jstate->max = plen;

		data->jstate->sum += plen;
		data->jstate->sqsum += (uint64_t)plen * plen;
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
plen_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_plen_config *conf;
	struct di_plen_fdata *data;
	int i;

	conf = (struct di_feature_plen_config *)cdata->conf;
	data = (struct di_plen_fdata *)fdata->data;

	fdata->stats[0] = 0x7FFFFFFF;
	for (i = 1; i < DI_PLEN_NO_STATS; i++)
		fdata->stats[i] = 0;

	if (!conf->plen_jump_window) {
		for (i = 0; i < conf->plen_window; i++)
			data->plens[i] = 0;
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
plen_get_stats(struct di_cdata *cdata, struct di_fdata *fdata, int32_t **stats)
{
#define	DI_PLEN_MIN	fdata->stats[0]
#define	DI_PLEN_MEAN	fdata->stats[1]
#define	DI_PLEN_MAX	fdata->stats[2]
#define	DI_PLEN_STDEV	fdata->stats[3]
#define	DI_PLEN_SUM	fdata->stats[4]
	struct di_feature_plen_config *conf;
	struct di_plen_fdata *data;
	uint64_t sqsum = 0;
	uint32_t sum = 0;
	int i, wsize;

	conf = (struct di_feature_plen_config *)cdata->conf;
	data = (struct di_plen_fdata *)fdata->data;
	sqsum = 0;
	sum = 0;

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
			DI_PLEN_MIN = 0x7FFFFFFF;
			DI_PLEN_MAX = 0;
			for (i = 0; i < wsize; i++) {
				if (data->plens[i] < DI_PLEN_MIN)
					DI_PLEN_MIN = data->plens[i];

				if (data->plens[i] > DI_PLEN_MAX)
					DI_PLEN_MAX = data->plens[i];

				sum += data->plens[i];
				sqsum += (uint64_t)data->plens[i] *
				    data->plens[i];
			}
		} else {
			DI_PLEN_MIN = data->jstate->min;
			DI_PLEN_MAX = data->jstate->max;
			sum = data->jstate->sum;
			sqsum = data->jstate->sqsum;
			reset_jump_win_state(data->jstate);
		}
		DI_PLEN_MEAN = sum / wsize;
		if (wsize > 1) {
			DI_PLEN_STDEV = fixp_sqrt((sqsum -
			    ((uint64_t)sum * sum / wsize)) / (wsize - 1));
		}
		DI_PLEN_SUM = sum;
		data->changed = 0;
	}
	*stats = fdata->stats;

	return (DI_PLEN_NO_STATS);
}

static int
plen_get_stat(struct di_cdata *cdata, struct di_fdata *fdata, int which,
    int32_t *stat)
{
	int32_t *stats;

	if (which < 0 || which > DI_PLEN_NO_STATS)
		return (-1);

	if (!plen_get_stats(cdata, fdata, &stats))
		return (0);

	*stat = stats[which];

	return (1);
}

static int
plen_get_stat_names(char **names[])
{

	*names = di_plen_stat_names;

	return (DI_PLEN_NO_STATS);
}

static struct di_feature_alg di_plen_desc = {
	_FI( .name = )			DI_PLEN_NAME,
	_FI( .type = )			DI_PLEN_TYPE,
	_FI( .ref_count = )		0,

	_FI( .init_instance = )		plen_init_instance,
	_FI( .destroy_instance = )	plen_destroy_instance,
	_FI( .init_stats = )		plen_init_stats,
	_FI( .destroy_stats = )		plen_destroy_stats,
	_FI( .update_stats = )		plen_update_stats,
	_FI( .reset_stats = )		plen_reset_stats,
	_FI( .get_stats = )		plen_get_stats,
	_FI( .get_stat = )		plen_get_stat,
	_FI( .get_stat_names = )	plen_get_stat_names,
	_FI( .get_conf = )		plen_get_conf,
};

DECLARE_DIFFUSE_FEATURE_MODULE(plen, &di_plen_desc);
