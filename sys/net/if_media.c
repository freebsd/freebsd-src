/*-
 * Copyright (c) 1997
 *	Jonathan Stone and Jason R. Thorpe.  All rights reserved.
 *
 * This software is derived from information provided by Matt Thomas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jonathan Stone
 *	and Jason R. Thorpe for the NetBSD Project.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $NetBSD: if_media.c,v 1.1 1997/03/17 02:55:15 thorpej Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ifmedia.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>

static MALLOC_DEFINE(M_IFMEDIA, "if_media", "interface media info");

struct ifmedia {
	if_media_t      ifm_media;	/* current user-set media word */
	if_media_t      ifm_mask;	/* mask of changes we don't care */
	if_media_t      *ifm_array;	/* array of all supported mediae */
	if_media_t	*ifm_cur;	/* currently selected media */
};

void 	ifmedia_alloc(struct ifnet *, struct if_attach_args *);
void 	ifmedia_free(struct ifnet *);
int	ifmedia_ioctl(struct ifnet *, struct ifreq *, u_long);

static if_media_t *	ifmedia_match(struct ifmedia *, if_media_t);
static if_media_t	ifmedia_compat(if_media_t media);
static uint64_t		ifmedia_baudrate(if_media_t);
static int		ifmedia_link_state(u_int);
#ifdef IFMEDIA_DEBUG
static void		ifmedia_printword(int);
static int ifmedia_debug;
#endif

/*
 * Called by if_attach(), if interface reports media.
 */
void
ifmedia_alloc(struct ifnet *ifp, struct if_attach_args *ifat)
{
	struct ifmedia *ifm;

	ifm = malloc(sizeof(struct ifmedia), M_IFMEDIA, M_WAITOK);
	ifm->ifm_array = ifat->ifat_mediae;
	ifm->ifm_mask = ifat->ifat_mediamask;
	ifm->ifm_cur = ifmedia_match(ifm, ifat->ifat_media);
	ifm->ifm_media = *ifm->ifm_cur;

	if_setsoftc(ifp, IF_MEDIA, ifm);

	ifp->if_baudrate = ifmedia_baudrate(ifm->ifm_media);
}

/*
 * Called by if_free().
 */
void
ifmedia_free(struct ifnet *ifp)
{
	struct ifmedia *ifm;

	ifm = if_getsoftc(ifp, IF_MEDIA);
	if_setsoftc(ifp, IF_MEDIA, NULL);
	free(ifm, M_IFMEDIA);
}

/*
 * Device-independent media ioctl support function.
 */
int
ifmedia_ioctl(struct ifnet *ifp, struct ifreq *ifr, u_long cmd)
{
	struct ifmediareq *ifmr = (struct ifmediareq *) ifr;
	struct ifmedia *ifm;
	if_media_t newmedia, *match;
	int i, error;

	ifm = if_getsoftc(ifp, IF_MEDIA);
	if (ifm == NULL)
		return (ENODEV);

	error = 0;
	switch (cmd) {
	case  SIOCSIFMEDIA:
		newmedia = ifr->ifr_media;
		match = ifmedia_match(ifm, newmedia);
		if (match == NULL) {
#ifdef IFMEDIA_DEBUG
			if (ifmedia_debug) {
				printf("%s: no media found for 0x%x\n", 
				    __func__, newmedia);
			}
#endif
			return (ENXIO);
		}

		/*
		 * If no change, we're done.
		 */
		if ((IFM_SUBTYPE(newmedia) != IFM_AUTO) &&
		    (newmedia == ifm->ifm_media) &&
		    (match == ifm->ifm_cur))
			return (0);

		/*
		 * We found a match, now make the driver switch to it.
		 * Make sure to preserve our old media type in case the
		 * driver can't switch.
		 */
#ifdef IFMEDIA_DEBUG
		if (ifmedia_debug) {
			printf("%s: switching %s to ", __func__, ifp->if_xname);
			ifmedia_printword(*match);
		}
#endif
		error = ifp->if_ops->ifop_media_change(ifp, newmedia);
		if (error)
			break;
		ifm->ifm_cur = match;
		ifm->ifm_media = newmedia;
		/*
		 * Some drivers, e.g. miibus(4) enabled, already set the
		 * baudrate in ifop_media_change, but some may not.
		 */
		ifp->if_baudrate = ifmedia_baudrate(newmedia);
		
		break;

	/*
	 * Get list of available media and current media on interface.
	 */
	case  SIOCGIFMEDIA: 
	case  SIOCGIFXMEDIA: 
		if (ifmr->ifm_count < 0)
			return (EINVAL);

		if (cmd == SIOCGIFMEDIA) {
			ifmr->ifm_active = ifmr->ifm_current = ifm->ifm_cur ?
			    ifmedia_compat(*ifm->ifm_cur) : IFM_NONE;
		} else {
			ifmr->ifm_active = ifmr->ifm_current = ifm->ifm_cur ?
			    *ifm->ifm_cur : IFM_NONE;
		}
		ifmr->ifm_mask = ifm->ifm_mask;
		ifmr->ifm_status = 0;
		ifp->if_ops->ifop_media_status(ifp, ifmr);

		/*
		 * If there are more supported mediae on the list, count
		 * them.  This allows the caller to set ifmr->ifm_count
		 * to 0 on the first call to know how much space to
		 * allocate.
		 */
		for (i = 0; ifm->ifm_array[i] != 0; i++)
			if (i < ifmr->ifm_count) {
				error = copyout(&ifm->ifm_array[i],
				    ifmr->ifm_ulist + i, sizeof(if_media_t));
				if (error)
					break;
			}
		if (error == 0 && i > ifmr->ifm_count)
			error = ifmr->ifm_count ? E2BIG : 0;
		ifmr->ifm_count = i;
		break;

	default:
		return (EINVAL);
	}

	return (error);
}

/*
 * Upcall from driver to report new media status.
 * We intentionally don't change ifm_cur or ifm_media, since this
 * upcall should come only in case if media is set to autonegotiation.
 */
void
if_media_status(struct ifnet *ifp, if_media_t media, if_media_t status)
{

	if_setbaudrate(ifp, ifmedia_baudrate(media));
	if_link_state_change(ifp, ifmedia_link_state(status));
}

/*
 * Interface wants to change its media list.
 */
void
if_media_change(struct ifnet *ifp, if_media_t *array, if_media_t cur)
{
	struct ifmedia *ifm;

	ifm = if_getsoftc(ifp, IF_MEDIA);
	ifm->ifm_array = array;
	ifm->ifm_cur = ifmedia_match(ifm, cur);
	ifm->ifm_media = *ifm->ifm_cur;

	ifp->if_baudrate = ifmedia_baudrate(ifm->ifm_media);
}

/*
 * Find media entry index matching a given ifm word.
 */
static if_media_t *
ifmedia_match(struct ifmedia *ifm, if_media_t target)
{
	if_media_t *match, mask;

	mask = ~ifm->ifm_mask;
	match = NULL;

	for (int i = 0; ifm->ifm_array[i] != 0; i++)
		if ((ifm->ifm_array[i] & mask) == (target & mask)) {
#if defined(IFMEDIA_DEBUG) || defined(DIAGNOSTIC)
			if (match != NULL)
				printf("%s: multiple match for "
				    "0x%x/0x%x\n", __func__, target, mask);
#endif
			match = &ifm->ifm_array[i];
		}

	return (match);
}

/*
 * Given a media word, return one suitable for an application
 * using the original encoding.
 */
static if_media_t
ifmedia_compat(if_media_t media)
{

	if (IFM_TYPE(media) == IFM_ETHER && IFM_SUBTYPE(media) > IFM_OTHER) {
		media &= ~(IFM_ETH_XTYPE|IFM_TMASK);
		media |= IFM_OTHER;
	}
	return (media);
}

/*
 * Compute the interface `baudrate' from the media, for the interface
 * metrics (used by routing daemons).
 */
static const struct ifmedia_baudrate ifmedia_baudrate_descriptions[] =   
    IFM_BAUDRATE_DESCRIPTIONS;

static uint64_t
ifmedia_baudrate(if_media_t mword)
{
	int i;

	for (i = 0; ifmedia_baudrate_descriptions[i].ifmb_word != 0; i++) {
		if (IFM_TYPE_MATCH(mword, ifmedia_baudrate_descriptions[i].ifmb_word))
			return (ifmedia_baudrate_descriptions[i].ifmb_baudrate);
	}

	/* Not known. */
	return (0);
}

static int
ifmedia_link_state(u_int mstatus)
{

	if (mstatus & IFM_AVALID) {
		if (mstatus & IFM_ACTIVE)
			return (LINK_STATE_UP);
		else
			return (LINK_STATE_DOWN);
	} else
		return (LINK_STATE_UNKNOWN);
}
 
#ifdef IFMEDIA_DEBUG
SYSCTL_INT(_debug, OID_AUTO, ifmedia, CTLFLAG_RW, &ifmedia_debug,
	    0, "if_media debugging msgs");

struct ifmedia_description ifm_type_descriptions[] =
    IFM_TYPE_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_ethernet_descriptions[] =
    IFM_SUBTYPE_ETHERNET_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_ethernet_option_descriptions[] =
    IFM_SUBTYPE_ETHERNET_OPTION_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_tokenring_descriptions[] =
    IFM_SUBTYPE_TOKENRING_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_tokenring_option_descriptions[] =
    IFM_SUBTYPE_TOKENRING_OPTION_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_fddi_descriptions[] =
    IFM_SUBTYPE_FDDI_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_fddi_option_descriptions[] =
    IFM_SUBTYPE_FDDI_OPTION_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_ieee80211_descriptions[] =
    IFM_SUBTYPE_IEEE80211_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_ieee80211_option_descriptions[] =
    IFM_SUBTYPE_IEEE80211_OPTION_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_ieee80211_mode_descriptions[] =
    IFM_SUBTYPE_IEEE80211_MODE_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_atm_descriptions[] =
    IFM_SUBTYPE_ATM_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_atm_option_descriptions[] =
    IFM_SUBTYPE_ATM_OPTION_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_shared_descriptions[] =
    IFM_SUBTYPE_SHARED_DESCRIPTIONS;

struct ifmedia_description ifm_shared_option_descriptions[] =
    IFM_SHARED_OPTION_DESCRIPTIONS;

struct ifmedia_type_to_subtype {
	struct ifmedia_description *subtypes;
	struct ifmedia_description *options;
	struct ifmedia_description *modes;
};

/* must be in the same order as IFM_TYPE_DESCRIPTIONS */
struct ifmedia_type_to_subtype ifmedia_types_to_subtypes[] = {
	{
	  &ifm_subtype_ethernet_descriptions[0],
	  &ifm_subtype_ethernet_option_descriptions[0],
	  NULL,
	},
	{
	  &ifm_subtype_tokenring_descriptions[0],
	  &ifm_subtype_tokenring_option_descriptions[0],
	  NULL,
	},
	{
	  &ifm_subtype_fddi_descriptions[0],
	  &ifm_subtype_fddi_option_descriptions[0],
	  NULL,
	},
	{
	  &ifm_subtype_ieee80211_descriptions[0],
	  &ifm_subtype_ieee80211_option_descriptions[0],
	  &ifm_subtype_ieee80211_mode_descriptions[0]
	},
	{
	  &ifm_subtype_atm_descriptions[0],
	  &ifm_subtype_atm_option_descriptions[0],
	  NULL,
	},
};

/*
 * print a media word.
 */
static void
ifmedia_printword(ifmw)
	int ifmw;
{
	struct ifmedia_description *desc;
	struct ifmedia_type_to_subtype *ttos;
	int seen_option = 0;

	/* Find the top-level interface type. */
	for (desc = ifm_type_descriptions, ttos = ifmedia_types_to_subtypes;
	    desc->ifmt_string != NULL; desc++, ttos++)
		if (IFM_TYPE(ifmw) == desc->ifmt_word)
			break;
	if (desc->ifmt_string == NULL) {
		printf("<unknown type>\n");
		return;
	}
	printf("%s", desc->ifmt_string);

	/* Any mode. */
	for (desc = ttos->modes; desc && desc->ifmt_string != NULL; desc++)
		if (IFM_MODE(ifmw) == desc->ifmt_word) {
			if (desc->ifmt_string != NULL)
				printf(" mode %s", desc->ifmt_string);
			break;
		}

	/*
	 * Check for the shared subtype descriptions first, then the
	 * type-specific ones.
	 */
	for (desc = ifm_subtype_shared_descriptions;
	    desc->ifmt_string != NULL; desc++)
		if (IFM_SUBTYPE(ifmw) == desc->ifmt_word)
			goto got_subtype;

	for (desc = ttos->subtypes; desc->ifmt_string != NULL; desc++)
		if (IFM_SUBTYPE(ifmw) == desc->ifmt_word)
			break;
	if (desc->ifmt_string == NULL) {
		printf(" <unknown subtype>\n");
		return;
	}

 got_subtype:
	printf(" %s", desc->ifmt_string);

	/*
	 * Look for shared options.
	 */
	for (desc = ifm_shared_option_descriptions;
	    desc->ifmt_string != NULL; desc++) {
		if (ifmw & desc->ifmt_word) {
			if (seen_option == 0)
				printf(" <");
			printf("%s%s", seen_option++ ? "," : "",
			    desc->ifmt_string);
		}
	}

	/*
	 * Look for subtype-specific options.
	 */
	for (desc = ttos->options; desc->ifmt_string != NULL; desc++) {
		if (ifmw & desc->ifmt_word) {
			if (seen_option == 0)
				printf(" <");
			printf("%s%s", seen_option++ ? "," : "",
			    desc->ifmt_string); 
		}
	}
	printf("%s\n", seen_option ? ">" : "");
}
#endif /* IFMEDIA_DEBUG */
