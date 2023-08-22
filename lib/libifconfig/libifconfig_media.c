/*
 * Copyright (c) 1983, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libifconfig.h"
#include "libifconfig_internal.h"

static const struct ifmedia_description *lookup_media_desc(
    const struct ifmedia_description *, const char *);
static const struct ifmedia_type_to_subtype *get_toptype_ttos(ifmedia_t);

#define IFM_OPMODE(x)							 \
	((x) & (IFM_IEEE80211_ADHOC | IFM_IEEE80211_HOSTAP |		 \
	IFM_IEEE80211_IBSS | IFM_IEEE80211_WDS | IFM_IEEE80211_MONITOR | \
	IFM_IEEE80211_MBSS))
#define IFM_IEEE80211_STA    0

static const struct ifmedia_description
    ifm_type_descriptions[] =
    IFM_TYPE_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_ethernet_descriptions[] =
    IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_ethernet_aliases[] =
    IFM_SUBTYPE_ETHERNET_ALIASES;

static const struct ifmedia_description
    ifm_subtype_ethernet_option_descriptions[] =
    IFM_SUBTYPE_ETHERNET_OPTION_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_ieee80211_descriptions[] =
    IFM_SUBTYPE_IEEE80211_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_ieee80211_aliases[] =
    IFM_SUBTYPE_IEEE80211_ALIASES;

static const struct ifmedia_description
    ifm_subtype_ieee80211_option_descriptions[] =
    IFM_SUBTYPE_IEEE80211_OPTION_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_ieee80211_mode_descriptions[] =
    IFM_SUBTYPE_IEEE80211_MODE_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_ieee80211_mode_aliases[] =
    IFM_SUBTYPE_IEEE80211_MODE_ALIASES;

static const struct ifmedia_description
    ifm_subtype_atm_descriptions[] =
    IFM_SUBTYPE_ATM_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_atm_aliases[] =
    IFM_SUBTYPE_ATM_ALIASES;

static const struct ifmedia_description
    ifm_subtype_atm_option_descriptions[] =
    IFM_SUBTYPE_ATM_OPTION_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_shared_descriptions[] =
    IFM_SUBTYPE_SHARED_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_subtype_shared_aliases[] =
    IFM_SUBTYPE_SHARED_ALIASES;

static const struct ifmedia_description
    ifm_shared_option_descriptions[] =
    IFM_SHARED_OPTION_DESCRIPTIONS;

static const struct ifmedia_description
    ifm_shared_option_aliases[] =
    IFM_SHARED_OPTION_ALIASES;

static const struct ifmedia_description *
lookup_media_desc(const struct ifmedia_description *desc, const char *name)
{

	for (; desc->ifmt_string != NULL; ++desc)
		if (strcasecmp(desc->ifmt_string, name) == 0)
			return (desc);
	return (NULL);
}

struct ifmedia_type_to_subtype {
	struct {
		const struct ifmedia_description *desc;
		bool alias;
	}
	subtypes[5];
	struct {
		const struct ifmedia_description *desc;
		bool alias;
	}
	options[4];
	struct {
		const struct ifmedia_description *desc;
		bool alias;
	}
	modes[3];
};

/* must be in the same order as IFM_TYPE_DESCRIPTIONS */
static const struct ifmedia_type_to_subtype ifmedia_types_to_subtypes[] =
{
	{
		{
			{ &ifm_subtype_shared_descriptions[0],		 0 },
			{ &ifm_subtype_shared_aliases[0],		 1 },
			{ &ifm_subtype_ethernet_descriptions[0],	 0 },
			{ &ifm_subtype_ethernet_aliases[0],		 1 },
			{ NULL,						 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0],		 0 },
			{ &ifm_shared_option_aliases[0],		 1 },
			{ &ifm_subtype_ethernet_option_descriptions[0],	 0 },
			{ NULL,						 0 },
		},
		{
			{ NULL,						 0 },
		},
	},
	{
		{
			{ &ifm_subtype_shared_descriptions[0],		 0 },
			{ &ifm_subtype_shared_aliases[0],		 1 },
			{ &ifm_subtype_ieee80211_descriptions[0],	 0 },
			{ &ifm_subtype_ieee80211_aliases[0],		 1 },
			{ NULL,						 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0],		 0 },
			{ &ifm_shared_option_aliases[0],		 1 },
			{ &ifm_subtype_ieee80211_option_descriptions[0], 0 },
			{ NULL,						 0 },
		},
		{
			{ &ifm_subtype_ieee80211_mode_descriptions[0],	 0 },
			{ &ifm_subtype_ieee80211_mode_aliases[0],	 0 },
			{ NULL,						 0 },
		},
	},
	{
		{
			{ &ifm_subtype_shared_descriptions[0],		 0 },
			{ &ifm_subtype_shared_aliases[0],		 1 },
			{ &ifm_subtype_atm_descriptions[0],		 0 },
			{ &ifm_subtype_atm_aliases[0],			 1 },
			{ NULL,						 0 },
		},
		{
			{ &ifm_shared_option_descriptions[0],		 0 },
			{ &ifm_shared_option_aliases[0],		 1 },
			{ &ifm_subtype_atm_option_descriptions[0],	 0 },
			{ NULL,						 0 },
		},
		{
			{ NULL,						 0 },
		},
	},
};

static const struct ifmedia_type_to_subtype *
get_toptype_ttos(ifmedia_t media)
{
	const struct ifmedia_description *desc;
	const struct ifmedia_type_to_subtype *ttos;

	for (desc = ifm_type_descriptions, ttos = ifmedia_types_to_subtypes;
	    desc->ifmt_string != NULL; desc++, ttos++) {
		if (IFM_TYPE(media) == desc->ifmt_word)
			return (ttos);
	}
	errno = ENOENT;
	return (NULL);
}

const char *
ifconfig_media_get_type(ifmedia_t media)
{
	const struct ifmedia_description *desc;

	for (desc = ifm_type_descriptions; desc->ifmt_string != NULL; ++desc) {
		if (IFM_TYPE(media) == desc->ifmt_word)
			return (desc->ifmt_string);
	}
	errno = ENOENT;
	return (NULL);
}

ifmedia_t
ifconfig_media_lookup_type(const char *name)
{
	const struct ifmedia_description *desc;

	desc = lookup_media_desc(ifm_type_descriptions, name);
	return (desc == NULL ? INVALID_IFMEDIA : desc->ifmt_word);
}

const char *
ifconfig_media_get_subtype(ifmedia_t media)
{
	const struct ifmedia_description *desc;
	const struct ifmedia_type_to_subtype *ttos;

	ttos = get_toptype_ttos(media);
	if (ttos == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	for (size_t i = 0; ttos->subtypes[i].desc != NULL; ++i) {
		if (ttos->subtypes[i].alias)
			continue;
		for (desc = ttos->subtypes[i].desc;
		    desc->ifmt_string != NULL; ++desc) {
			if (IFM_SUBTYPE(media) == desc->ifmt_word)
				return (desc->ifmt_string);
		}
	}
	errno = ENOENT;
	return (NULL);
}

ifmedia_t
ifconfig_media_lookup_subtype(ifmedia_t media, const char *name)
{
	const struct ifmedia_description *desc;
	const struct ifmedia_type_to_subtype *ttos;

	ttos = get_toptype_ttos(media);
	if (ttos == NULL) {
		errno = EINVAL;
		return (INVALID_IFMEDIA);
	}

	for (size_t i = 0; ttos->subtypes[i].desc != NULL; ++i) {
		desc = lookup_media_desc(ttos->subtypes[i].desc, name);
		if (desc != NULL)
			return (desc->ifmt_word);
	}
	errno = ENOENT;
	return (INVALID_IFMEDIA);
}

const char *
ifconfig_media_get_mode(ifmedia_t media)
{
	const struct ifmedia_description *desc;
	const struct ifmedia_type_to_subtype *ttos;

	ttos = get_toptype_ttos(media);
	if (ttos == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	for (size_t i = 0; ttos->modes[i].desc != NULL; ++i) {
		if (ttos->modes[i].alias)
			continue;
		for (desc = ttos->modes[i].desc;
		    desc->ifmt_string != NULL; ++desc) {
			if (IFM_MODE(media) == desc->ifmt_word)
				return (desc->ifmt_string);
		}
	}
	errno = ENOENT;
	return (NULL);
}

ifmedia_t
ifconfig_media_lookup_mode(ifmedia_t media, const char *name)
{
	const struct ifmedia_description *desc;
	const struct ifmedia_type_to_subtype *ttos;

	ttos = get_toptype_ttos(media);
	if (ttos == NULL) {
		errno = EINVAL;
		return (INVALID_IFMEDIA);
	}

	for (size_t i = 0; ttos->modes[i].desc != NULL; ++i) {
		desc = lookup_media_desc(ttos->modes[i].desc, name);
		if (desc != NULL)
			return (desc->ifmt_word);
	}
	errno = ENOENT;
	return (INVALID_IFMEDIA);
}

const char **
ifconfig_media_get_options(ifmedia_t media)
{
	const char **options;
	const struct ifmedia_description *desc;
	const struct ifmedia_type_to_subtype *ttos;
	size_t n;

	ttos = get_toptype_ttos(media);
	if (ttos == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	n = 0;
	for (size_t i = 0; ttos->options[i].desc != NULL; ++i) {
		if (ttos->options[i].alias)
			continue;
		for (desc = ttos->options[i].desc;
		    desc->ifmt_string != NULL; ++desc) {
			if ((media & desc->ifmt_word) != 0)
				++n;
		}
	}
	if (n == 0) {
		errno = ENOENT;
		return (NULL);
	}

	options = calloc(n + 1, sizeof(*options));
	if (options == NULL)
		return (NULL);

	options[n] = NULL;
	n = 0;
	for (size_t i = 0; ttos->options[i].desc != NULL; ++i) {
		if (ttos->options[i].alias)
			continue;
		for (desc = ttos->options[i].desc;
		    desc->ifmt_string != NULL; ++desc) {
			if ((media & desc->ifmt_word) != 0) {
				options[n] = desc->ifmt_string;
				++n;
			}
		}
	}
	return (options);
}

ifmedia_t *
ifconfig_media_lookup_options(ifmedia_t media, const char **opts, size_t nopts)
{
	ifmedia_t *options;
	const struct ifmedia_description *desc, *opt;
	const struct ifmedia_type_to_subtype *ttos;

	assert(opts != NULL);
	assert(nopts > 0);

	ttos = get_toptype_ttos(media);
	if (ttos == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	options = calloc(nopts, sizeof(*options));
	if (options == NULL)
		return (NULL);
	(void)memset(options, INVALID_IFMEDIA, nopts * sizeof(ifmedia_t));

	for (size_t i = 0; ttos->options[i].desc != NULL; ++i) {
		desc = ttos->options[i].desc;
		for (size_t j = 0; j < nopts; ++j) {
			opt = lookup_media_desc(desc, opts[j]);
			if (opt != NULL)
				options[j] = opt->ifmt_word;
		}
	}
	return (options);
}

/***************************************************************************
* Above this point, this file is mostly copied from sbin/ifconfig/ifmedia.c
***************************************************************************/

/* Internal structure used for allocations and frees */
struct _ifconfig_media_status {
	struct ifmediareq ifmr;
	int medialist[0];
};

int
ifconfig_media_get_mediareq(ifconfig_handle_t *h, const char *name,
    struct ifmediareq **ifmr)
{
	struct _ifconfig_media_status *ms, *ms2;
	unsigned long cmd = SIOCGIFXMEDIA;

	*ifmr = NULL;
	ms = calloc(1, sizeof(*ms));
	if (ms == NULL) {
		h->error.errtype = OTHER;
		h->error.errcode = ENOMEM;
		return (-1);
	}
	(void)strlcpy(ms->ifmr.ifm_name, name, sizeof(ms->ifmr.ifm_name));

	/*
	 * Check if interface supports extended media types.
	 */
	if (ifconfig_ioctlwrap(h, AF_LOCAL, cmd, &ms->ifmr) < 0) {
		cmd = SIOCGIFMEDIA;
		if (ifconfig_ioctlwrap(h, AF_LOCAL, cmd, &ms->ifmr) < 0) {
			/* Interface doesn't support SIOC{G,S}IFMEDIA.  */
			h->error.errtype = OK;
			free(ms);
			return (-1);
		}
	}
	if (ms->ifmr.ifm_count == 0) {
		*ifmr = &ms->ifmr;
		return (0);     /* Interface has no media types ?*/
	}

	ms2 = realloc(ms, sizeof(*ms) + sizeof(int) * ms->ifmr.ifm_count);
	if (ms2 == NULL) {
		h->error.errtype = OTHER;
		h->error.errcode = ENOMEM;
		free(ms);
		return (-1);
	}
	ms2->ifmr.ifm_ulist = &ms2->medialist[0];

	if (ifconfig_ioctlwrap(h, AF_LOCAL, cmd, &ms2->ifmr) < 0) {
		free(ms2);
		return (-1);
	}

	*ifmr = &ms2->ifmr;
	return (0);
}

const char *
ifconfig_media_get_status(const struct ifmediareq *ifmr)
{
	switch (IFM_TYPE(ifmr->ifm_active)) {
	case IFM_ETHER:
	case IFM_ATM:
		if (ifmr->ifm_status & IFM_ACTIVE) {
			return ("active");
		} else {
			return ("no carrier");
		}
		break;

	case IFM_IEEE80211:
		if (ifmr->ifm_status & IFM_ACTIVE) {
			/* NB: only sta mode associates */
			if (IFM_OPMODE(ifmr->ifm_active) == IFM_IEEE80211_STA) {
				return ("associated");
			} else {
				return ("running");
			}
		} else {
			return ("no carrier");
		}
		break;
	default:
		return ("");
	}
}

int
ifconfig_media_get_downreason(ifconfig_handle_t *h, const char *name,
    struct ifdownreason *ifdr)
{

	(void)memset(ifdr, 0, sizeof(*ifdr));
	(void)strlcpy(ifdr->ifdr_name, name, sizeof(ifdr->ifdr_name));
	return (ifconfig_ioctlwrap(h, AF_LOCAL, SIOCGIFDOWNREASON, ifdr));
}
