/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Rozanna Jesudasan.
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
 * DIFFUSE Skype Feature.
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
#include <netinet/ipfw/diffuse_feature_skype.h>
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
 * Feature Skype computes mean, ratio and the absolute two packet difference of
 * packet length (length = length of only the data, length of UDP/TCP data, or
 * length of IP data).
 */

/* If we are linked from userspace only put the declaration here. */
#ifdef _KERNEL
DI_SKYPE_STAT_NAMES;
#else
DI_SKYPE_STAT_NAMES_DECL;
#endif

/* State for jump windows. */
struct di_skype_jump_win_state {
	uint32_t	fwdsum;
	uint32_t	revsum;
	uint64_t	atpdsum;
	int32_t		plen1;
	int32_t		plen2;
	int		jump;
	int		first;
};

/* Per flow data (ring buffer). */
struct di_skype_fdata {
	int				full;
	int				changed;
	int				index;
	uint16_t			*plens;
	uint8_t				*dirs;
	struct di_skype_jump_win_state	*jstate;
};

static int skype_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata);

static int
skype_init_instance(struct di_cdata *cdata, struct di_oid *params)
{
	struct di_feature_skype_config *conf, *p;

	cdata->conf = malloc(sizeof(struct di_feature_skype_config), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (cdata->conf == NULL)
		return (ENOMEM);

	conf = (struct di_feature_skype_config *)cdata->conf;

	/* Set default configuration. */
	conf->plen_window = DI_DEFAULT_SKYPE_WINDOW;
	conf->plen_partial_window = 0;
	conf->plen_len_type = DI_PLEN_LEN_PAYLOAD;
	conf->plen_jump_window = 0;

	/* Set configuration */
	if (params != NULL) {
		p = (struct di_feature_skype_config *)params;

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
skype_destroy_instance(struct di_cdata *cdata)
{

	free(cdata->conf, M_DIFFUSE);

	return (0);
}

static int
skype_get_conf(struct di_cdata *cdata, struct di_oid *cbuf, int size_only)
{

	if (!size_only)
		memcpy(cbuf, cdata->conf,
		    sizeof(struct di_feature_skype_config));

	return (sizeof(struct di_feature_skype_config));
}

static int
skype_init_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_skype_config *conf;
	struct di_skype_fdata *data;

	conf = (struct di_feature_skype_config *)cdata->conf;

	fdata->data = malloc(sizeof(struct di_skype_fdata), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (fdata->data == NULL)
		return (ENOMEM);

	data = (struct di_skype_fdata *)fdata->data;

	if (!conf->plen_jump_window) {
		data->plens = malloc(conf->plen_window * sizeof(uint16_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
		data->dirs = malloc(conf->plen_window * sizeof(uint8_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	} else {
		data->jstate = malloc(sizeof(struct di_skype_jump_win_state),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	}

	fdata->stats = malloc(DI_SKYPE_NO_STATS * sizeof(int32_t), M_DIFFUSE,
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

	skype_reset_stats(cdata, fdata);

	return (0);
}

static int
skype_destroy_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_skype_config *conf;
	struct di_skype_fdata *data;

	conf = (struct di_feature_skype_config *)cdata->conf;
	data = (struct di_skype_fdata *)fdata->data;

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

static int
skype_update_stats(struct di_cdata *cdata, struct di_fdata *fdata,
    struct mbuf *mbuf, int proto, void *ulp, int dir)
{
	struct di_feature_skype_config *conf;
	struct di_skype_fdata *data;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *tcp;
	uint16_t plen;
	int iplen;

	conf = (struct di_feature_skype_config *)cdata->conf;
	data = (struct di_skype_fdata *)fdata->data;
	ip = mtod(mbuf, struct ip *);
	plen = 0;

	/* Length of data in IP */
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
					tcp = (struct tcphdr*)ulp;
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

		if (dir == MATCH_FORWARD)
			data->jstate->fwdsum += plen;
		else
			data->jstate->revsum += plen;

		if (data->jstate->plen1 != -1) {
			data->jstate->atpdsum += (plen > data->jstate->plen1) ?
			    (plen - data->jstate->plen1) :
			    (data->jstate->plen1 - plen);

			if (data->jstate->plen2 != -1) {
				data->jstate->atpdsum +=
				    (plen > data->jstate->plen2) ?
				    (plen - data->jstate->plen2) :
				    (data->jstate->plen2 - plen);
			}
		}

		data->jstate->plen2 = data->jstate->plen1;
		data->jstate->plen1 = plen;
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

static void
reset_jump_win_state(struct di_skype_jump_win_state *state)
{

	state->fwdsum = 0;
	state->revsum = 0;
	state->atpdsum = 0;
	state->plen1 = -1;
	state->plen2 = -1;
	state->jump = 0;
}

static int
skype_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_skype_config *conf;
	struct di_skype_fdata *data;
	int i;

	conf = (struct di_feature_skype_config *)cdata->conf;
	data = (struct di_skype_fdata *)fdata->data;

	for (i = 1; i < DI_SKYPE_NO_STATS; i++)
		fdata->stats[i] = 0;

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

/*
 * Calculates the attributes mean, absolute two packet difference and ratio of
 * the packet payload lengths for a traffic flow or subflow. The statistics
 * calculated are stored in the fdata structure stats array and are used for
 * classification. The attribute two packet difference is the difference in
 * payload length between two consecutive packets. The ratio is calculated for
 * the payload length in the forward and reverse directions of the flow.
 */
static int
skype_get_stats(struct di_cdata *cdata, struct di_fdata *fdata, int32_t **stats)
{
#define	PL_MEAN		fdata->stats[0]
#define	PL_ATPD		fdata->stats[1]
#define	PL_RATIO	fdata->stats[2]
	struct di_feature_skype_config *conf;
	struct di_skype_fdata *data;
	uint32_t atpdsum, fwdsum, revsum, sum;
	int32_t plen1, plen2;
	int i, wsize;

	conf = (struct di_feature_skype_config *)cdata->conf;
	data = (struct di_skype_fdata *)fdata->data;
	atpdsum = fwdsum = revsum = sum = 0;
	plen1 = plen2 = -1;

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
			for (i = 0; i < wsize; i++) {

				if (data->dirs[i] == MATCH_FORWARD)
					fwdsum += data->plens[i];
				else
					revsum += data->plens[i];

				sum += data->plens[i];

				/*
				 * Get stats for the absolute two packet
				 * difference.
				 */
				if (plen1 != -1) {
					atpdsum += (data->plens[i] > plen1) ?
					    (data->plens[i] - plen1) :
					    (plen1 - data->plens[i]);

					if (plen2 != -1) {
						atpdsum +=
						    (data->plens[i] > plen2) ?
						    (data->plens[i] - plen2) :
						    (plen2 - data->plens[i]);
					}
				}
				plen2 = plen1;
				plen1 = data->plens[i];
			}
		} else {
			sum = data->jstate->fwdsum + data->jstate->revsum;
			revsum = data->jstate->revsum;
			fwdsum = data->jstate->fwdsum;
			atpdsum = data->jstate->atpdsum;
			reset_jump_win_state(data->jstate);
		}

		/* Calculate ratio. */
		if (fwdsum >= revsum && fwdsum > 0)
			PL_RATIO = (revsum * 1000) / fwdsum;

		if (revsum > fwdsum && revsum > 0)
			PL_RATIO = (fwdsum * 1000) / revsum;

		/* Calculate absolute two packet difference. */
		if (sum > 0)
			PL_ATPD = (atpdsum * 1000) / (sum * 2);

		/* Calculate the mean. */
		PL_MEAN = sum / wsize;
		data->changed = 0;
	}

	*stats = fdata->stats;

	return (DI_SKYPE_NO_STATS);
}

static int
skype_get_stat(struct di_cdata *cdata, struct di_fdata *fdata, int which,
    int32_t *stat)
{
	int32_t *stats;

	if (which < 0 || which > DI_SKYPE_NO_STATS)
		return (-1);

	if (!skype_get_stats(cdata, fdata, &stats))
		return (0);

	*stat = stats[which];

	return (1);
}

static int
skype_get_stat_names(char **names[])
{

	*names = di_skype_stat_names;

	return (DI_SKYPE_NO_STATS);
}

static struct di_feature_alg di_skype_desc = {
	_FI( .name = )			DI_SKYPE_NAME,
	_FI( .type = )			DI_SKYPE_TYPE,
	_FI( .ref_count = )		0,

	_FI( .init_instance = )		skype_init_instance,
	_FI( .destroy_instance = )	skype_destroy_instance,
	_FI( .init_stats = )		skype_init_stats,
	_FI( .destroy_stats = )		skype_destroy_stats,
	_FI( .update_stats = )		skype_update_stats,
	_FI( .reset_stats = )		skype_reset_stats,
	_FI( .get_stats = )		skype_get_stats,
	_FI( .get_stat = )		skype_get_stat,
	_FI( .get_stat_names = )	skype_get_stat_names,
	_FI( .get_conf = )		skype_get_conf,
};

DECLARE_DIFFUSE_FEATURE_MODULE(skype, &di_skype_desc);
