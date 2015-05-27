/*-
 * Copyright (c) 2014 Adrian Chadd <adrian@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

/*
 * iwn ioctl API.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <err.h>


#include "net80211/ieee80211_ioctl.h"
#include "net80211/ieee80211_radiotap.h"

#include "if_iwn_ioctl.h"

/*
 * This contains the register definitions for iwn; including
 * the statistics definitions.
 */
#include "if_iwnreg.h"

#include "iwnstats.h"

#include "iwn_ioctl.h"

void
iwn_setifname(struct iwnstats *is, const char *ifname)
{

	strncpy(is->ifr.ifr_name, ifname, sizeof (is->ifr.ifr_name));
}

void
iwn_zerostats(struct iwnstats *is)
{

	if (ioctl(is->s, SIOCZIWNSTATS, &is->ifr) < 0)
		err(-1, "ioctl: %s", is->ifr.ifr_name);
}

int
iwn_collect(struct iwnstats *is)
{
	int err;

	is->ifr.ifr_data = (caddr_t) &is->st;
	err = ioctl(is->s, SIOCGIWNSTATS, &is->ifr);
	if (err < 0)
		warn("ioctl: %s", is->ifr.ifr_name);
	return (err);
}
