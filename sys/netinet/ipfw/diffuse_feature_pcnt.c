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
 * DIFFUSE packet count feature.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#endif
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_diffuse.h>
#include <netinet/ip_var.h>

#include <netinet/ipfw/diffuse_common.h>
#include <netinet/ipfw/diffuse_feature.h>
#include <netinet/ipfw/diffuse_feature_pcnt.h>
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
 * Feature packet count computes number of forward packets, backward packets and
 * ratio.
 */

/* If we are linked from userspace only put the declaration here. */
#ifdef _KERNEL
DI_PCNT_STAT_NAMES;
#else
DI_PCNT_STAT_NAMES_DECL;
#endif

/* State for jump windows. */
struct di_pcnt_jump_win_state {
	uint16_t	fpkts;
	uint16_t	bpkts;
	int		jump;
	int		first;
};

/* Per flow data (ring buffer). */
struct di_pcnt_fdata {
	int				full;
	int				changed;
	int				index;
	uint8_t				*pdirs;
	struct di_pcnt_jump_win_state	*jstate;
};

static int pcnt_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata);

static int
pcnt_init_instance(struct di_cdata *cdata, struct di_oid *params)
{
	struct di_feature_pcnt_config *conf, *p;

	cdata->conf = malloc(sizeof(struct di_feature_pcnt_config), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (cdata->conf == NULL)
		return (ENOMEM);

	conf = (struct di_feature_pcnt_config *)cdata->conf;

	/* Set default configuration. */
	conf->pcnt_window = DI_DEFAULT_PCNT_WINDOW;
	conf->pcnt_partial_window = 0;
	conf->pcnt_jump_window = 0;

	/* Set configuration */
	if (params != NULL) {
		p = (struct di_feature_pcnt_config *)params;

		if (p->pcnt_window != -1)
			conf->pcnt_window = p->pcnt_window;

		if (p->pcnt_partial_window != -1)
			conf->pcnt_partial_window = p->pcnt_partial_window;

		if (p->pcnt_jump_window != -1)
			conf->pcnt_jump_window = p->pcnt_jump_window;
	}

	return (0);
}

static int
pcnt_destroy_instance(struct di_cdata *cdata)
{

	free(cdata->conf, M_DIFFUSE);

	return (0);
}

static int
pcnt_get_conf(struct di_cdata *cdata, struct di_oid *cbuf, int size_only)
{

	if (!size_only)
		memcpy(cbuf, cdata->conf,
		    sizeof(struct di_feature_pcnt_config));

	return (sizeof(struct di_feature_pcnt_config));
}

static int
pcnt_init_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_pcnt_config *conf;
	struct di_pcnt_fdata *data;

	conf = (struct di_feature_pcnt_config *)cdata->conf;

	fdata->data = malloc(sizeof(struct di_pcnt_fdata), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);
	if (fdata->data == NULL)
		return (ENOMEM);

	data = (struct di_pcnt_fdata *)fdata->data;

	if (!conf->pcnt_jump_window) {
		data->pdirs = malloc(conf->pcnt_window * sizeof(uint8_t),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	} else {
		data->jstate = malloc(sizeof(struct di_pcnt_jump_win_state),
		    M_DIFFUSE, M_NOWAIT | M_ZERO);
	}
	fdata->stats = malloc(DI_PCNT_NO_STATS * sizeof(int32_t), M_DIFFUSE,
	    M_NOWAIT | M_ZERO);

	if (data->pdirs == NULL || data->jstate == NULL ||
	    fdata->stats == NULL) {
		free(fdata->stats, M_DIFFUSE);
		free(data->jstate, M_DIFFUSE);
		free(data->pdirs, M_DIFFUSE);
		free(fdata->data, M_DIFFUSE);

		return (ENOMEM);
	}

	pcnt_reset_stats(cdata, fdata);

	return (0);
}

static int
pcnt_destroy_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_pcnt_config *conf;
	struct di_pcnt_fdata *data;

	conf  = (struct di_feature_pcnt_config *)cdata->conf;
	data = (struct di_pcnt_fdata *)fdata->data;

	if (!conf->pcnt_jump_window)
		free(data->pdirs, M_DIFFUSE);
	else
		free(data->jstate, M_DIFFUSE);

	free(fdata->data, M_DIFFUSE);
	free(fdata->stats, M_DIFFUSE);

	return (0);
}

static void
reset_jump_win_state(struct di_pcnt_jump_win_state *state)
{

	state->fpkts = 0;
	state->bpkts = 0;
	state->jump = 0;
}

static int
pcnt_update_stats(struct di_cdata *cdata, struct di_fdata *fdata,
    struct mbuf *mbuf, int proto, void *ulp, int dir)
{
	struct di_feature_pcnt_config *conf;
	struct di_pcnt_fdata *data;

	conf  = (struct di_feature_pcnt_config *)cdata->conf;
	data = (struct di_pcnt_fdata *)fdata->data;

	if (!conf->pcnt_jump_window) {
		data->pdirs[data->index++] = dir;
	} else {
		if (dir == MATCH_FORWARD)
			data->jstate->fpkts++;
		else
			data->jstate->bpkts++;

		data->index++;

		if (data->index == conf->pcnt_window) {
			data->jstate->jump = 1;
			if (data->jstate->first)
				data->jstate->first = 0;
		}
	}

	if (!data->full && data->index == conf->pcnt_window)
		data->full = 1;

	data->changed = 1;
	if (data->index == conf->pcnt_window)
		data->index = 0;

	return (0);
}

static int
pcnt_reset_stats(struct di_cdata *cdata, struct di_fdata *fdata)
{
	struct di_feature_pcnt_config *conf;
	struct di_pcnt_fdata *data;
	int i;

	conf = (struct di_feature_pcnt_config *)cdata->conf;
	data = (struct di_pcnt_fdata *)fdata->data;

	for (i = 0; i < DI_PCNT_NO_STATS; i++)
		fdata->stats[i] = 0;

	if (!conf->pcnt_jump_window) {
		for (i = 0; i < conf->pcnt_window; i++)
			data->pdirs[i] = 0;
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
pcnt_get_stats(struct di_cdata *cdata, struct di_fdata *fdata, int32_t **stats)
{
#define	DI_PCNT_FPKTS	fdata->stats[0]
#define	DI_PCNT_BPKTS	fdata->stats[1]
#define	DI_PCNT_RATIO	fdata->stats[2]
	struct di_feature_pcnt_config *conf;
	struct di_pcnt_fdata *data;
	int i, wsize;

	conf  = (struct di_feature_pcnt_config *)cdata->conf;
	data = (struct di_pcnt_fdata *)fdata->data;

	if (!data->full && !(conf->pcnt_partial_window && data->index > 0))
		return (0); /* Window is not full yet. */

	/* Compute stats only if we need update. */
	if ((!conf->pcnt_jump_window && data->changed) ||
	    (conf->pcnt_jump_window && (data->jstate->jump ||
	    (conf->pcnt_partial_window && data->jstate->first)))) {
		wsize = conf->pcnt_window;
		if (!data->full)
			wsize = data->index;

		if (!conf->pcnt_jump_window) {
			DI_PCNT_FPKTS = 0;
			DI_PCNT_BPKTS = 0;
			for(i = 0; i < wsize; i++) {
				if (data->pdirs[i] == MATCH_FORWARD)
					DI_PCNT_FPKTS++;
				else
					DI_PCNT_BPKTS++;
			}
		} else {
			DI_PCNT_FPKTS = data->jstate->fpkts;
			DI_PCNT_BPKTS = data->jstate->bpkts;
			reset_jump_win_state(data->jstate);
		}
		DI_PCNT_RATIO = 0;
		if (DI_PCNT_BPKTS > 0)
			DI_PCNT_RATIO = (DI_PCNT_FPKTS * 1000) / DI_PCNT_BPKTS;

		data->changed = 0;
	}
	*stats = fdata->stats;

	return (DI_PCNT_NO_STATS);
}

static int
pcnt_get_stat(struct di_cdata *cdata, struct di_fdata *fdata, int which,
    int32_t *stat)
{
	int32_t *stats;

	if (which < 0 || which > DI_PCNT_NO_STATS)
		return (-1);

	if (!pcnt_get_stats(cdata, fdata, &stats))
		return (0);

	*stat = stats[which];

	return (1);
}

static int
pcnt_get_stat_names(char **names[])
{

	*names = di_pcnt_stat_names;

	return (DI_PCNT_NO_STATS);
}

static struct di_feature_alg di_pcnt_desc = {
	_FI( .name = )			DI_PCNT_NAME,
	_FI( .type = )			DI_PCNT_TYPE,
	_FI( .ref_count = )		0,

	_FI( .init_instance = )		pcnt_init_instance,
	_FI( .destroy_instance = )	pcnt_destroy_instance,
	_FI( .init_stats = )		pcnt_init_stats,
	_FI( .destroy_stats = )		pcnt_destroy_stats,
	_FI( .update_stats = )		pcnt_update_stats,
	_FI( .reset_stats = )		pcnt_reset_stats,
	_FI( .get_stats = )		pcnt_get_stats,
	_FI( .get_stat = )		pcnt_get_stat,
	_FI( .get_stat_names = )	pcnt_get_stat_names,
	_FI( .get_conf = )		pcnt_get_conf,
};

DECLARE_DIFFUSE_FEATURE_MODULE(pcnt, &di_pcnt_desc);
