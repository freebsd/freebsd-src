/*
 * Copyright 2001 The Aerospace Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of The Aerospace Corporation may not be used to endorse or
 *    promote products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AEROSPACE CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AEROSPACE CORPORATION BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*-
 * Copyright (c) 1997, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "ifconfig.h"

static void set80211(int s, int type, int val, int len, void *data);
static const char *get_string(const char *val, const char *sep,
    u_int8_t *buf, int *lenp);
static void print_string(const u_int8_t *buf, int len);

static struct ieee80211req_chaninfo chaninfo;
static struct ifmediareq *ifmr;

/*
 * Collect channel info from the kernel.  We use this (mostly)
 * to handle mapping between frequency and IEEE channel number.
 */
static void
getchaninfo(int s)
{
	struct ieee80211req ireq;

	if (chaninfo.ic_nchans != 0)
		return;
	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_CHANINFO;
	ireq.i_data = &chaninfo;
	ireq.i_len = sizeof(chaninfo);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		errx(1, "unable to get channel information");

	ifmr = ifmedia_getstate(s);
}

/*
 * Given the channel at index i with attributes from,
 * check if there is a channel with attributes to in
 * the channel table.  With suitable attributes this
 * allows the caller to look for promotion; e.g. from
 * 11b > 11g.
 */
static int
canpromote(int i, int from, int to)
{
	const struct ieee80211_channel *fc = &chaninfo.ic_chans[i];
	int j;

	if ((fc->ic_flags & from) != from)
		return i;
	/* NB: quick check exploiting ordering of chans w/ same frequency */
	if (i+1 < chaninfo.ic_nchans &&
	    chaninfo.ic_chans[i+1].ic_freq == fc->ic_freq &&
	    (chaninfo.ic_chans[i+1].ic_flags & to) == to)
		return i+1;
	/* brute force search in case channel list is not ordered */
	for (j = 0; j < chaninfo.ic_nchans; j++) {
		const struct ieee80211_channel *tc = &chaninfo.ic_chans[j];
		if (j != i &&
		    tc->ic_freq == fc->ic_freq && (tc->ic_flags & to) == to)
		return j;
	}
	return i;
}

/*
 * Handle channel promotion.  When a channel is specified with
 * only a frequency we want to promote it to the ``best'' channel
 * available.  The channel list has separate entries for 11b, 11g,
 * 11a, and 11n[ga] channels so specifying a frequency w/o any
 * attributes requires we upgrade, e.g. from 11b -> 11g.  This
 * gets complicated when the channel is specified on the same
 * command line with a media request that constrains the available
 * channe list (e.g. mode 11a); we want to honor that to avoid
 * confusing behaviour.
 */
static int
promote(int i)
{
	/*
	 * Query the current mode of the interface in case it's
	 * constrained (e.g. to 11a).  We must do this carefully
	 * as there may be a pending ifmedia request in which case
	 * asking the kernel will give us the wrong answer.  This
	 * is an unfortunate side-effect of the way ifconfig is
	 * structure for modularity (yech).
	 *
	 * NB: ifmr is actually setup in getchaninfo (above); we
	 *     assume it's called coincident with to this call so
	 *     we have a ``current setting''; otherwise we must pass
	 *     the socket descriptor down to here so we can make
	 *     the ifmedia_getstate call ourselves.
	 */
	int chanmode = ifmr != NULL ? IFM_MODE(ifmr->ifm_current) : IFM_AUTO;

	/* when ambiguous promote to ``best'' */
	/* NB: we abitrarily pick HT40+ over HT40- */
	if (chanmode != IFM_IEEE80211_11B)
		i = canpromote(i, IEEE80211_CHAN_B, IEEE80211_CHAN_G);
	if (chanmode != IFM_IEEE80211_11G) {
		i = canpromote(i, IEEE80211_CHAN_G,
			IEEE80211_CHAN_G | IEEE80211_CHAN_HT20);
		i = canpromote(i, IEEE80211_CHAN_G,
			IEEE80211_CHAN_G | IEEE80211_CHAN_HT40D);
		i = canpromote(i, IEEE80211_CHAN_G,
			IEEE80211_CHAN_G | IEEE80211_CHAN_HT40U);
	}
	if (chanmode != IFM_IEEE80211_11A) {
		i = canpromote(i, IEEE80211_CHAN_A,
			IEEE80211_CHAN_A | IEEE80211_CHAN_HT20);
		i = canpromote(i, IEEE80211_CHAN_A,
			IEEE80211_CHAN_A | IEEE80211_CHAN_HT40D);
		i = canpromote(i, IEEE80211_CHAN_A,
			IEEE80211_CHAN_A | IEEE80211_CHAN_HT40U);
	}
	return i;
}

static void
mapfreq(struct ieee80211_channel *chan, int freq, int flags)
{
	int i;

	for (i = 0; i < chaninfo.ic_nchans; i++) {
		const struct ieee80211_channel *c = &chaninfo.ic_chans[i];

		if (c->ic_freq == freq && (c->ic_flags & flags) == flags) {
			if (flags == 0) {
				/* when ambiguous promote to ``best'' */
				c = &chaninfo.ic_chans[promote(i)];
			}
			*chan = *c;
			return;
		}
	}
	errx(1, "unknown/undefined frequency %u/0x%x", freq, flags);
}

static void
mapchan(struct ieee80211_channel *chan, int ieee, int flags)
{
	int i;

	for (i = 0; i < chaninfo.ic_nchans; i++) {
		const struct ieee80211_channel *c = &chaninfo.ic_chans[i];

		if (c->ic_ieee == ieee && (c->ic_flags & flags) == flags) {
			if (flags == 0) {
				/* when ambiguous promote to ``best'' */
				c = &chaninfo.ic_chans[promote(i)];
			}
			*chan = *c;
			return;
		}
	}
	errx(1, "unknown/undefined channel number %d", ieee);
}

static int
ieee80211_mhz2ieee(int freq, int flags)
{
	struct ieee80211_channel chan;
	mapfreq(&chan, freq, flags);
	return chan.ic_ieee;
}

static int
isanyarg(const char *arg)
{
	return (strcmp(arg, "-") == 0 ||
	    strcasecmp(arg, "any") == 0 || strcasecmp(arg, "off") == 0);
}

static void
set80211ssid(const char *val, int d, int s, const struct afswtch *rafp)
{
	int		ssid;
	int		len;
	u_int8_t	data[IEEE80211_NWID_LEN];

	ssid = 0;
	len = strlen(val);
	if (len > 2 && isdigit(val[0]) && val[1] == ':') {
		ssid = atoi(val)-1;
		val += 2;
	}

	bzero(data, sizeof(data));
	len = sizeof(data);
	if (get_string(val, NULL, data, &len) == NULL)
		exit(1);

	set80211(s, IEEE80211_IOC_SSID, ssid, len, data);
}

static void
set80211stationname(const char *val, int d, int s, const struct afswtch *rafp)
{
	int			len;
	u_int8_t		data[33];

	bzero(data, sizeof(data));
	len = sizeof(data);
	get_string(val, NULL, data, &len);

	set80211(s, IEEE80211_IOC_STATIONNAME, 0, len, data);
}

/*
 * Parse a channel specification for attributes/flags.
 * The syntax is:
 *	freq/xx		channel width (5,10,20,40,40+,40-)
 *	freq:mode	channel mode (a,b,g,h,n,t,s,d)
 *
 * These can be combined in either order; e.g. 2437:ng/40.
 * Modes are case insensitive.
 *
 * The result is not validated here; it's assumed to be
 * checked against the channel table fetched from the kernel.
 */ 
static int
getchannelflags(const char *val)
{
#define	CHAN_HT_DEFAULT	IEEE80211_CHAN_HT40U
#define	_CHAN_HT	0x80000000
	const char *cp;
	int flags;

	flags = 0;

	cp = strchr(val, ':');
	if (cp != NULL) {
		for (cp++; isalpha((int) *cp); cp++) {
			/* accept mixed case */
			int c = *cp;
			if (isupper(c))
				c = tolower(c);
			switch (c) {
			case 'a':		/* 802.11a */
				flags |= IEEE80211_CHAN_A;
				break;
			case 'b':		/* 802.11b */
				flags |= IEEE80211_CHAN_B;
				break;
			case 'g':		/* 802.11g */
				flags |= IEEE80211_CHAN_G;
				break;
			case 'h':		/* ht = 802.11n */
			case 'n':		/* 802.11n */
				flags |= _CHAN_HT;	/* NB: private */
				break;
			case 'd':		/* dt = Atheros Dynamic Turbo */
				flags |= IEEE80211_CHAN_TURBO;
				break;
			case 't':		/* ht, dt, st, t */
				/* dt and unadorned t specify Dynamic Turbo */
				if ((flags & (IEEE80211_CHAN_STURBO|_CHAN_HT)) == 0)
					flags |= IEEE80211_CHAN_TURBO;
				break;
			case 's':		/* st = Atheros Static Turbo */
				flags |= IEEE80211_CHAN_STURBO;
				break;
			default:
				errx(-1, "%s: Invalid channel attribute %c",
				    val, *cp);
			}
		}
	}
	cp = strchr(val, '/');
	if (cp != NULL) {
		char *ep;
		u_long cw = strtoul(cp+1, &ep, 10);

		switch (cw) {
		case 5:
			flags |= IEEE80211_CHAN_QUARTER;
			break;
		case 10:
			flags |= IEEE80211_CHAN_HALF;
			break;
		case 20:
			/* NB: this may be removed below */
			flags |= IEEE80211_CHAN_HT20;
			break;
		case 40:
			if (ep != NULL && *ep == '+')
				flags |= IEEE80211_CHAN_HT40U;
			else if (ep != NULL && *ep == '-')
				flags |= IEEE80211_CHAN_HT40D;
			else		/* NB: pick something */
				flags |= CHAN_HT_DEFAULT;
			break;
		default:
			errx(-1, "%s: Invalid channel width", val);
		}
	}
	/*
	 * Cleanup specifications.
	 */ 
	if ((flags & _CHAN_HT) == 0) {
		/*
		 * If user specified freq/20 or freq/40 quietly remove
		 * HT cw attributes depending on channel use.  To give
		 * an explicit 20/40 width for an HT channel you must
		 * indicate it is an HT channel since all HT channels
		 * are also usable for legacy operation; e.g. freq:n/40.
		 */
		flags &= ~IEEE80211_CHAN_HT;
	} else {
		/*
		 * Remove private indicator that this is an HT channel
		 * and if no explicit channel width has been given
		 * provide the default settings.
		 */
		flags &= ~_CHAN_HT;
		if ((flags & IEEE80211_CHAN_HT) == 0)
			flags |= CHAN_HT_DEFAULT;
	}
	return flags;
#undef CHAN_HT_DEFAULT
#undef _CHAN_HT
}

static void
set80211channel(const char *val, int d, int s, const struct afswtch *rafp)
{
	struct ieee80211_channel chan;

	memset(&chan, 0, sizeof(chan));
	if (!isanyarg(val)) {
		int v = atoi(val);
		int flags = getchannelflags(val);

		getchaninfo(s);
		if (v > 255) {		/* treat as frequency */
			mapfreq(&chan, v, flags);
		} else {
			mapchan(&chan, v, flags);
		}
	} else {
		chan.ic_freq = IEEE80211_CHAN_ANY;
	}
	set80211(s, IEEE80211_IOC_CURCHAN, 0, sizeof(chan), &chan);
}

static void
set80211authmode(const char *val, int d, int s, const struct afswtch *rafp)
{
	int	mode;

	if (strcasecmp(val, "none") == 0) {
		mode = IEEE80211_AUTH_NONE;
	} else if (strcasecmp(val, "open") == 0) {
		mode = IEEE80211_AUTH_OPEN;
	} else if (strcasecmp(val, "shared") == 0) {
		mode = IEEE80211_AUTH_SHARED;
	} else if (strcasecmp(val, "8021x") == 0) {
		mode = IEEE80211_AUTH_8021X;
	} else if (strcasecmp(val, "wpa") == 0) {
		mode = IEEE80211_AUTH_WPA;
	} else {
		errx(1, "unknown authmode");
	}

	set80211(s, IEEE80211_IOC_AUTHMODE, mode, 0, NULL);
}

static void
set80211powersavemode(const char *val, int d, int s, const struct afswtch *rafp)
{
	int	mode;

	if (strcasecmp(val, "off") == 0) {
		mode = IEEE80211_POWERSAVE_OFF;
	} else if (strcasecmp(val, "on") == 0) {
		mode = IEEE80211_POWERSAVE_ON;
	} else if (strcasecmp(val, "cam") == 0) {
		mode = IEEE80211_POWERSAVE_CAM;
	} else if (strcasecmp(val, "psp") == 0) {
		mode = IEEE80211_POWERSAVE_PSP;
	} else if (strcasecmp(val, "psp-cam") == 0) {
		mode = IEEE80211_POWERSAVE_PSP_CAM;
	} else {
		errx(1, "unknown powersavemode");
	}

	set80211(s, IEEE80211_IOC_POWERSAVE, mode, 0, NULL);
}

static void
set80211powersave(const char *val, int d, int s, const struct afswtch *rafp)
{
	if (d == 0)
		set80211(s, IEEE80211_IOC_POWERSAVE, IEEE80211_POWERSAVE_OFF,
		    0, NULL);
	else
		set80211(s, IEEE80211_IOC_POWERSAVE, IEEE80211_POWERSAVE_ON,
		    0, NULL);
}

static void
set80211powersavesleep(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_POWERSAVESLEEP, atoi(val), 0, NULL);
}

static void
set80211wepmode(const char *val, int d, int s, const struct afswtch *rafp)
{
	int	mode;

	if (strcasecmp(val, "off") == 0) {
		mode = IEEE80211_WEP_OFF;
	} else if (strcasecmp(val, "on") == 0) {
		mode = IEEE80211_WEP_ON;
	} else if (strcasecmp(val, "mixed") == 0) {
		mode = IEEE80211_WEP_MIXED;
	} else {
		errx(1, "unknown wep mode");
	}

	set80211(s, IEEE80211_IOC_WEP, mode, 0, NULL);
}

static void
set80211wep(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_WEP, d, 0, NULL);
}

static int
isundefarg(const char *arg)
{
	return (strcmp(arg, "-") == 0 || strncasecmp(arg, "undef", 5) == 0);
}

static void
set80211weptxkey(const char *val, int d, int s, const struct afswtch *rafp)
{
	if (isundefarg(val))
		set80211(s, IEEE80211_IOC_WEPTXKEY, IEEE80211_KEYIX_NONE, 0, NULL);
	else
		set80211(s, IEEE80211_IOC_WEPTXKEY, atoi(val)-1, 0, NULL);
}

static void
set80211wepkey(const char *val, int d, int s, const struct afswtch *rafp)
{
	int		key = 0;
	int		len;
	u_int8_t	data[IEEE80211_KEYBUF_SIZE];

	if (isdigit(val[0]) && val[1] == ':') {
		key = atoi(val)-1;
		val += 2;
	}

	bzero(data, sizeof(data));
	len = sizeof(data);
	get_string(val, NULL, data, &len);

	set80211(s, IEEE80211_IOC_WEPKEY, key, len, data);
}

/*
 * This function is purely a NetBSD compatability interface.  The NetBSD
 * interface is too inflexible, but it's there so we'll support it since
 * it's not all that hard.
 */
static void
set80211nwkey(const char *val, int d, int s, const struct afswtch *rafp)
{
	int		txkey;
	int		i, len;
	u_int8_t	data[IEEE80211_KEYBUF_SIZE];

	set80211(s, IEEE80211_IOC_WEP, IEEE80211_WEP_ON, 0, NULL);

	if (isdigit(val[0]) && val[1] == ':') {
		txkey = val[0]-'0'-1;
		val += 2;

		for (i = 0; i < 4; i++) {
			bzero(data, sizeof(data));
			len = sizeof(data);
			val = get_string(val, ",", data, &len);
			if (val == NULL)
				exit(1);

			set80211(s, IEEE80211_IOC_WEPKEY, i, len, data);
		}
	} else {
		bzero(data, sizeof(data));
		len = sizeof(data);
		get_string(val, NULL, data, &len);
		txkey = 0;

		set80211(s, IEEE80211_IOC_WEPKEY, 0, len, data);

		bzero(data, sizeof(data));
		for (i = 1; i < 4; i++)
			set80211(s, IEEE80211_IOC_WEPKEY, i, 0, data);
	}

	set80211(s, IEEE80211_IOC_WEPTXKEY, txkey, 0, NULL);
}

static void
set80211rtsthreshold(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_RTSTHRESHOLD,
		isundefarg(val) ? IEEE80211_RTS_MAX : atoi(val), 0, NULL);
}

static void
set80211protmode(const char *val, int d, int s, const struct afswtch *rafp)
{
	int	mode;

	if (strcasecmp(val, "off") == 0) {
		mode = IEEE80211_PROTMODE_OFF;
	} else if (strcasecmp(val, "cts") == 0) {
		mode = IEEE80211_PROTMODE_CTS;
	} else if (strcasecmp(val, "rtscts") == 0) {
		mode = IEEE80211_PROTMODE_RTSCTS;
	} else {
		errx(1, "unknown protection mode");
	}

	set80211(s, IEEE80211_IOC_PROTMODE, mode, 0, NULL);
}

static void
set80211txpower(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_TXPOWER, atoi(val), 0, NULL);
}

#define	IEEE80211_ROAMING_DEVICE	0
#define	IEEE80211_ROAMING_AUTO		1
#define	IEEE80211_ROAMING_MANUAL	2

static void
set80211roaming(const char *val, int d, int s, const struct afswtch *rafp)
{
	int mode;

	if (strcasecmp(val, "device") == 0) {
		mode = IEEE80211_ROAMING_DEVICE;
	} else if (strcasecmp(val, "auto") == 0) {
		mode = IEEE80211_ROAMING_AUTO;
	} else if (strcasecmp(val, "manual") == 0) {
		mode = IEEE80211_ROAMING_MANUAL;
	} else {
		errx(1, "unknown roaming mode");
	}
	set80211(s, IEEE80211_IOC_ROAMING, mode, 0, NULL);
}

static void
set80211wme(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_WME, d, 0, NULL);
}

static void
set80211hidessid(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_HIDESSID, d, 0, NULL);
}

static void
set80211apbridge(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_APBRIDGE, d, 0, NULL);
}

static void
set80211fastframes(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_FF, d, 0, NULL);
}

static void
set80211dturbo(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_TURBOP, d, 0, NULL);
}

static void
set80211chanlist(const char *val, int d, int s, const struct afswtch *rafp)
{
	struct ieee80211req_chanlist chanlist;
#define	MAXCHAN	(sizeof(chanlist.ic_channels)*NBBY)
	char *temp, *cp, *tp;

	temp = malloc(strlen(val) + 1);
	if (temp == NULL)
		errx(1, "malloc failed");
	strcpy(temp, val);
	memset(&chanlist, 0, sizeof(chanlist));
	cp = temp;
	for (;;) {
		int first, last, f;

		tp = strchr(cp, ',');
		if (tp != NULL)
			*tp++ = '\0';
		switch (sscanf(cp, "%u-%u", &first, &last)) {
		case 1:
			if (first > MAXCHAN)
				errx(-1, "channel %u out of range, max %zu",
					first, MAXCHAN);
			setbit(chanlist.ic_channels, first);
			break;
		case 2:
			if (first > MAXCHAN)
				errx(-1, "channel %u out of range, max %zu",
					first, MAXCHAN);
			if (last > MAXCHAN)
				errx(-1, "channel %u out of range, max %zu",
					last, MAXCHAN);
			if (first > last)
				errx(-1, "void channel range, %u > %u",
					first, last);
			for (f = first; f <= last; f++)
				setbit(chanlist.ic_channels, f);
			break;
		}
		if (tp == NULL)
			break;
		while (isspace(*tp))
			tp++;
		if (!isdigit(*tp))
			break;
		cp = tp;
	}
	set80211(s, IEEE80211_IOC_CHANLIST, 0, sizeof(chanlist), &chanlist);
#undef MAXCHAN
}

static void
set80211bssid(const char *val, int d, int s, const struct afswtch *rafp)
{

	if (!isanyarg(val)) {
		char *temp;
		struct sockaddr_dl sdl;

		temp = malloc(strlen(val) + 2); /* ':' and '\0' */
		if (temp == NULL)
			errx(1, "malloc failed");
		temp[0] = ':';
		strcpy(temp + 1, val);
		sdl.sdl_len = sizeof(sdl);
		link_addr(temp, &sdl);
		free(temp);
		if (sdl.sdl_alen != IEEE80211_ADDR_LEN)
			errx(1, "malformed link-level address");
		set80211(s, IEEE80211_IOC_BSSID, 0,
			IEEE80211_ADDR_LEN, LLADDR(&sdl));
	} else {
		uint8_t zerobssid[IEEE80211_ADDR_LEN];
		memset(zerobssid, 0, sizeof(zerobssid));
		set80211(s, IEEE80211_IOC_BSSID, 0,
			IEEE80211_ADDR_LEN, zerobssid);
	}
}

static int
getac(const char *ac)
{
	if (strcasecmp(ac, "ac_be") == 0 || strcasecmp(ac, "be") == 0)
		return WME_AC_BE;
	if (strcasecmp(ac, "ac_bk") == 0 || strcasecmp(ac, "bk") == 0)
		return WME_AC_BK;
	if (strcasecmp(ac, "ac_vi") == 0 || strcasecmp(ac, "vi") == 0)
		return WME_AC_VI;
	if (strcasecmp(ac, "ac_vo") == 0 || strcasecmp(ac, "vo") == 0)
		return WME_AC_VO;
	errx(1, "unknown wme access class %s", ac);
}

static
DECL_CMD_FUNC2(set80211cwmin, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_CWMIN, atoi(val), getac(ac), NULL);
}

static
DECL_CMD_FUNC2(set80211cwmax, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_CWMAX, atoi(val), getac(ac), NULL);
}

static
DECL_CMD_FUNC2(set80211aifs, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_AIFS, atoi(val), getac(ac), NULL);
}

static
DECL_CMD_FUNC2(set80211txoplimit, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_TXOPLIMIT, atoi(val), getac(ac), NULL);
}

static
DECL_CMD_FUNC(set80211acm, ac, d)
{
	set80211(s, IEEE80211_IOC_WME_ACM, 1, getac(ac), NULL);
}
static
DECL_CMD_FUNC(set80211noacm, ac, d)
{
	set80211(s, IEEE80211_IOC_WME_ACM, 0, getac(ac), NULL);
}

static
DECL_CMD_FUNC(set80211ackpolicy, ac, d)
{
	set80211(s, IEEE80211_IOC_WME_ACKPOLICY, 1, getac(ac), NULL);
}
static
DECL_CMD_FUNC(set80211noackpolicy, ac, d)
{
	set80211(s, IEEE80211_IOC_WME_ACKPOLICY, 0, getac(ac), NULL);
}

static
DECL_CMD_FUNC2(set80211bsscwmin, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_CWMIN, atoi(val),
		getac(ac)|IEEE80211_WMEPARAM_BSS, NULL);
}

static
DECL_CMD_FUNC2(set80211bsscwmax, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_CWMAX, atoi(val),
		getac(ac)|IEEE80211_WMEPARAM_BSS, NULL);
}

static
DECL_CMD_FUNC2(set80211bssaifs, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_AIFS, atoi(val),
		getac(ac)|IEEE80211_WMEPARAM_BSS, NULL);
}

static
DECL_CMD_FUNC2(set80211bsstxoplimit, ac, val)
{
	set80211(s, IEEE80211_IOC_WME_TXOPLIMIT, atoi(val),
		getac(ac)|IEEE80211_WMEPARAM_BSS, NULL);
}

static
DECL_CMD_FUNC(set80211dtimperiod, val, d)
{
	set80211(s, IEEE80211_IOC_DTIM_PERIOD, atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211bintval, val, d)
{
	set80211(s, IEEE80211_IOC_BEACON_INTERVAL, atoi(val), 0, NULL);
}

static void
set80211macmac(int s, int op, const char *val)
{
	char *temp;
	struct sockaddr_dl sdl;

	temp = malloc(strlen(val) + 2); /* ':' and '\0' */
	if (temp == NULL)
		errx(1, "malloc failed");
	temp[0] = ':';
	strcpy(temp + 1, val);
	sdl.sdl_len = sizeof(sdl);
	link_addr(temp, &sdl);
	free(temp);
	if (sdl.sdl_alen != IEEE80211_ADDR_LEN)
		errx(1, "malformed link-level address");
	set80211(s, op, 0, IEEE80211_ADDR_LEN, LLADDR(&sdl));
}

static
DECL_CMD_FUNC(set80211addmac, val, d)
{
	set80211macmac(s, IEEE80211_IOC_ADDMAC, val);
}

static
DECL_CMD_FUNC(set80211delmac, val, d)
{
	set80211macmac(s, IEEE80211_IOC_DELMAC, val);
}

static
DECL_CMD_FUNC(set80211kickmac, val, d)
{
	char *temp;
	struct sockaddr_dl sdl;
	struct ieee80211req_mlme mlme;

	temp = malloc(strlen(val) + 2); /* ':' and '\0' */
	if (temp == NULL)
		errx(1, "malloc failed");
	temp[0] = ':';
	strcpy(temp + 1, val);
	sdl.sdl_len = sizeof(sdl);
	link_addr(temp, &sdl);
	free(temp);
	if (sdl.sdl_alen != IEEE80211_ADDR_LEN)
		errx(1, "malformed link-level address");
	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = IEEE80211_REASON_AUTH_EXPIRE;
	memcpy(mlme.im_macaddr, LLADDR(&sdl), IEEE80211_ADDR_LEN);
	set80211(s, IEEE80211_IOC_MLME, 0, sizeof(mlme), &mlme);
}

static
DECL_CMD_FUNC(set80211maccmd, val, d)
{
	set80211(s, IEEE80211_IOC_MACCMD, d, 0, NULL);
}

static void
set80211pureg(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_PUREG, d, 0, NULL);
}

static void
set80211bgscan(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_BGSCAN, d, 0, NULL);
}

static
DECL_CMD_FUNC(set80211bgscanidle, val, d)
{
	set80211(s, IEEE80211_IOC_BGSCAN_IDLE, atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211bgscanintvl, val, d)
{
	set80211(s, IEEE80211_IOC_BGSCAN_INTERVAL, atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211scanvalid, val, d)
{
	set80211(s, IEEE80211_IOC_SCANVALID, atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211roamrssi11a, val, d)
{
	set80211(s, IEEE80211_IOC_ROAM_RSSI_11A, atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211roamrssi11b, val, d)
{
	set80211(s, IEEE80211_IOC_ROAM_RSSI_11B, atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211roamrssi11g, val, d)
{
	set80211(s, IEEE80211_IOC_ROAM_RSSI_11G, atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211roamrate11a, val, d)
{
	set80211(s, IEEE80211_IOC_ROAM_RATE_11A, 2*atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211roamrate11b, val, d)
{
	set80211(s, IEEE80211_IOC_ROAM_RATE_11B, 2*atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211roamrate11g, val, d)
{
	set80211(s, IEEE80211_IOC_ROAM_RATE_11G, 2*atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211mcastrate, val, d)
{
	set80211(s, IEEE80211_IOC_MCAST_RATE, 2*atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211fragthreshold, val, d)
{
	set80211(s, IEEE80211_IOC_FRAGTHRESHOLD,
		isundefarg(val) ? IEEE80211_FRAG_MAX : atoi(val), 0, NULL);
}

static
DECL_CMD_FUNC(set80211bmissthreshold, val, d)
{
	set80211(s, IEEE80211_IOC_BMISSTHRESHOLD,
		isundefarg(val) ? IEEE80211_HWBMISS_MAX : atoi(val), 0, NULL);
}

static void
set80211burst(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_BURST, d, 0, NULL);
}

static void
set80211doth(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_DOTH, d, 0, NULL);
}

static int
getmaxrate(const uint8_t rates[15], uint8_t nrates)
{
	int i, maxrate = -1;

	for (i = 0; i < nrates; i++) {
		int rate = rates[i] & IEEE80211_RATE_VAL;
		if (rate > maxrate)
			maxrate = rate;
	}
	return maxrate / 2;
}

static const char *
getcaps(int capinfo)
{
	static char capstring[32];
	char *cp = capstring;

	if (capinfo & IEEE80211_CAPINFO_ESS)
		*cp++ = 'E';
	if (capinfo & IEEE80211_CAPINFO_IBSS)
		*cp++ = 'I';
	if (capinfo & IEEE80211_CAPINFO_CF_POLLABLE)
		*cp++ = 'c';
	if (capinfo & IEEE80211_CAPINFO_CF_POLLREQ)
		*cp++ = 'C';
	if (capinfo & IEEE80211_CAPINFO_PRIVACY)
		*cp++ = 'P';
	if (capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)
		*cp++ = 'S';
	if (capinfo & IEEE80211_CAPINFO_PBCC)
		*cp++ = 'B';
	if (capinfo & IEEE80211_CAPINFO_CHNL_AGILITY)
		*cp++ = 'A';
	if (capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)
		*cp++ = 's';
	if (capinfo & IEEE80211_CAPINFO_RSN)
		*cp++ = 'R';
	if (capinfo & IEEE80211_CAPINFO_DSSSOFDM)
		*cp++ = 'D';
	*cp = '\0';
	return capstring;
}

static const char *
getflags(int flags)
{
/* XXX need these publicly defined or similar */
#define	IEEE80211_NODE_AUTH	0x0001		/* authorized for data */
#define	IEEE80211_NODE_QOS	0x0002		/* QoS enabled */
#define	IEEE80211_NODE_ERP	0x0004		/* ERP enabled */
#define	IEEE80211_NODE_PWR_MGT	0x0010		/* power save mode enabled */
#define	IEEE80211_NODE_HT	0x0040		/* HT enabled */
	static char flagstring[32];
	char *cp = flagstring;

	if (flags & IEEE80211_NODE_AUTH)
		*cp++ = 'A';
	if (flags & IEEE80211_NODE_QOS)
		*cp++ = 'Q';
	if (flags & IEEE80211_NODE_ERP)
		*cp++ = 'E';
	if (flags & IEEE80211_NODE_PWR_MGT)
		*cp++ = 'P';
	if (flags & IEEE80211_NODE_HT)
		*cp++ = 'H';
	*cp = '\0';
	return flagstring;
#undef IEEE80211_NODE_HT
#undef IEEE80211_NODE_AUTH
#undef IEEE80211_NODE_QOS
#undef IEEE80211_NODE_ERP
#undef IEEE80211_NODE_PWR_MGT
}

static void
printie(const char* tag, const uint8_t *ie, size_t ielen, int maxlen)
{
	printf("%s", tag);
	if (verbose) {
		maxlen -= strlen(tag)+2;
		if (2*ielen > maxlen)
			maxlen--;
		printf("<");
		for (; ielen > 0; ie++, ielen--) {
			if (maxlen-- <= 0)
				break;
			printf("%02x", *ie);
		}
		if (ielen != 0)
			printf("-");
		printf(">");
	}
}

#define LE_READ_2(p)					\
	((u_int16_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

/*
 * NB: The decoding routines assume a properly formatted ie
 *     which should be safe as the kernel only retains them
 *     if they parse ok.
 */

static void
printwmeie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)
	static const char *acnames[] = { "BE", "BK", "VO", "VI" };
	int i;

	printf("%s", tag);
	if (verbose) {
		printf("<qosinfo 0x%x", ie[
			__offsetof(struct ieee80211_wme_param, param_qosInfo)]);
		ie += __offsetof(struct ieee80211_wme_param, params_acParams);
		for (i = 0; i < WME_NUM_AC; i++) {
			printf(" %s[%saifsn %u cwmin %u cwmax %u txop %u]"
				, acnames[i]
				, MS(ie[0], WME_PARAM_ACM) ? "acm " : ""
				, MS(ie[0], WME_PARAM_AIFSN)
				, MS(ie[1], WME_PARAM_LOGCWMIN)
				, MS(ie[1], WME_PARAM_LOGCWMAX)
				, LE_READ_2(ie+2)
			);
			ie += 4;
		}
		printf(">");
	}
#undef MS
}

static void
printathie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{

	printf("%s", tag);
	if (verbose) {
		const struct ieee80211_ath_ie *ath =
			(const struct ieee80211_ath_ie *)ie;

		printf("<");
		if (ath->ath_capability & ATHEROS_CAP_TURBO_PRIME)
			printf("DTURBO,");
		if (ath->ath_capability & ATHEROS_CAP_COMPRESSION)
			printf("COMP,");
		if (ath->ath_capability & ATHEROS_CAP_FAST_FRAME)
			printf("FF,");
		if (ath->ath_capability & ATHEROS_CAP_XR)
			printf("XR,");
		if (ath->ath_capability & ATHEROS_CAP_AR)
			printf("AR,");
		if (ath->ath_capability & ATHEROS_CAP_BURST)
			printf("BURST,");
		if (ath->ath_capability & ATHEROS_CAP_WME)
			printf("WME,");
		if (ath->ath_capability & ATHEROS_CAP_BOOST)
			printf("BOOST,");
		printf("0x%x>", LE_READ_2(ath->ath_defkeyix));
	}
}

static const char *
wpa_cipher(const u_int8_t *sel)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case WPA_SEL(WPA_CSE_NULL):
		return "NONE";
	case WPA_SEL(WPA_CSE_WEP40):
		return "WEP40";
	case WPA_SEL(WPA_CSE_WEP104):
		return "WEP104";
	case WPA_SEL(WPA_CSE_TKIP):
		return "TKIP";
	case WPA_SEL(WPA_CSE_CCMP):
		return "AES-CCMP";
	}
	return "?";		/* NB: so 1<< is discarded */
#undef WPA_SEL
}

static const char *
wpa_keymgmt(const u_int8_t *sel)
{
#define	WPA_SEL(x)	(((x)<<24)|WPA_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case WPA_SEL(WPA_ASE_8021X_UNSPEC):
		return "8021X-UNSPEC";
	case WPA_SEL(WPA_ASE_8021X_PSK):
		return "8021X-PSK";
	case WPA_SEL(WPA_ASE_NONE):
		return "NONE";
	}
	return "?";
#undef WPA_SEL
}

static void
printwpaie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	u_int8_t len = ie[1];

	printf("%s", tag);
	if (verbose) {
		const char *sep;
		int n;

		ie += 6, len -= 4;		/* NB: len is payload only */

		printf("<v%u", LE_READ_2(ie));
		ie += 2, len -= 2;

		printf(" mc:%s", wpa_cipher(ie));
		ie += 4, len -= 4;

		/* unicast ciphers */
		n = LE_READ_2(ie);
		ie += 2, len -= 2;
		sep = " uc:";
		for (; n > 0; n--) {
			printf("%s%s", sep, wpa_cipher(ie));
			ie += 4, len -= 4;
			sep = "+";
		}

		/* key management algorithms */
		n = LE_READ_2(ie);
		ie += 2, len -= 2;
		sep = " km:";
		for (; n > 0; n--) {
			printf("%s%s", sep, wpa_keymgmt(ie));
			ie += 4, len -= 4;
			sep = "+";
		}

		if (len > 2)		/* optional capabilities */
			printf(", caps 0x%x", LE_READ_2(ie));
		printf(">");
	}
}

static const char *
rsn_cipher(const u_int8_t *sel)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case RSN_SEL(RSN_CSE_NULL):
		return "NONE";
	case RSN_SEL(RSN_CSE_WEP40):
		return "WEP40";
	case RSN_SEL(RSN_CSE_WEP104):
		return "WEP104";
	case RSN_SEL(RSN_CSE_TKIP):
		return "TKIP";
	case RSN_SEL(RSN_CSE_CCMP):
		return "AES-CCMP";
	case RSN_SEL(RSN_CSE_WRAP):
		return "AES-OCB";
	}
	return "?";
#undef WPA_SEL
}

static const char *
rsn_keymgmt(const u_int8_t *sel)
{
#define	RSN_SEL(x)	(((x)<<24)|RSN_OUI)
	u_int32_t w = LE_READ_4(sel);

	switch (w) {
	case RSN_SEL(RSN_ASE_8021X_UNSPEC):
		return "8021X-UNSPEC";
	case RSN_SEL(RSN_ASE_8021X_PSK):
		return "8021X-PSK";
	case RSN_SEL(RSN_ASE_NONE):
		return "NONE";
	}
	return "?";
#undef RSN_SEL
}

static void
printrsnie(const char *tag, const u_int8_t *ie, size_t ielen, int maxlen)
{
	u_int8_t len = ie[1];

	printf("%s", tag);
	if (verbose) {
		const char *sep;
		int n;

		ie += 6, len -= 4;		/* NB: len is payload only */

		printf("<v%u", LE_READ_2(ie));
		ie += 2, len -= 2;

		printf(" mc:%s", rsn_cipher(ie));
		ie += 4, len -= 4;

		/* unicast ciphers */
		n = LE_READ_2(ie);
		ie += 2, len -= 2;
		sep = " uc:";
		for (; n > 0; n--) {
			printf("%s%s", sep, rsn_cipher(ie));
			ie += 4, len -= 4;
			sep = "+";
		}

		/* key management algorithms */
		n = LE_READ_2(ie);
		ie += 2, len -= 2;
		sep = " km:";
		for (; n > 0; n--) {
			printf("%s%s", sep, rsn_keymgmt(ie));
			ie += 4, len -= 4;
			sep = "+";
		}

		if (len > 2)		/* optional capabilities */
			printf(", caps 0x%x", LE_READ_2(ie));
		/* XXXPMKID */
		printf(">");
	}
}

/*
 * Copy the ssid string contents into buf, truncating to fit.  If the
 * ssid is entirely printable then just copy intact.  Otherwise convert
 * to hexadecimal.  If the result is truncated then replace the last
 * three characters with "...".
 */
static int
copy_essid(char buf[], size_t bufsize, const u_int8_t *essid, size_t essid_len)
{
	const u_int8_t *p; 
	size_t maxlen;
	int i;

	if (essid_len > bufsize)
		maxlen = bufsize;
	else
		maxlen = essid_len;
	/* determine printable or not */
	for (i = 0, p = essid; i < maxlen; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i != maxlen) {		/* not printable, print as hex */
		if (bufsize < 3)
			return 0;
		strlcpy(buf, "0x", bufsize);
		bufsize -= 2;
		p = essid;
		for (i = 0; i < maxlen && bufsize >= 2; i++) {
			sprintf(&buf[2+2*i], "%02x", p[i]);
			bufsize -= 2;
		}
		if (i != essid_len)
			memcpy(&buf[2+2*i-3], "...", 3);
	} else {			/* printable, truncate as needed */
		memcpy(buf, essid, maxlen);
		if (maxlen != essid_len)
			memcpy(&buf[maxlen-3], "...", 3);
	}
	return maxlen;
}

/* unaligned little endian access */     
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

static int __inline
iswpaoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WPA_OUI_TYPE<<24)|WPA_OUI);
}

static int __inline
iswmeoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}

static int __inline
isatherosoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((ATH_OUI_TYPE<<24)|ATH_OUI);
}

static void
printies(const u_int8_t *vp, int ielen, int maxcols)
{
	while (ielen > 0) {
		switch (vp[0]) {
		case IEEE80211_ELEMID_VENDOR:
			if (iswpaoui(vp))
				printwpaie(" WPA", vp, 2+vp[1], maxcols);
			else if (iswmeoui(vp))
				printwmeie(" WME", vp, 2+vp[1], maxcols);
			else if (isatherosoui(vp))
				printathie(" ATH", vp, 2+vp[1], maxcols);
			else
				printie(" VEN", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_RSN:
			printrsnie(" RSN", vp, 2+vp[1], maxcols);
			break;
		default:
			printie(" ???", vp, 2+vp[1], maxcols);
			break;
		}
		ielen -= 2+vp[1];
		vp += 2+vp[1];
	}
}

static void
list_scan(int s)
{
	uint8_t buf[24*1024];
	struct ieee80211req ireq;
	char ssid[IEEE80211_NWID_LEN+1];
	uint8_t *cp;
	int len, ssidmax;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_SCAN_RESULTS;
	ireq.i_data = buf;
	ireq.i_len = sizeof(buf);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		errx(1, "unable to get scan results");
	len = ireq.i_len;
	if (len < sizeof(struct ieee80211req_scan_result))
		return;

	getchaninfo(s);

	ssidmax = verbose ? IEEE80211_NWID_LEN : 14;
	printf("%-*.*s  %-17.17s  %4s %4s  %-7s  %3s %4s\n"
		, ssidmax, ssidmax, "SSID"
		, "BSSID"
		, "CHAN"
		, "RATE"
		, " S:N"
		, "INT"
		, "CAPS"
	);
	cp = buf;
	do {
		const struct ieee80211req_scan_result *sr;
		const uint8_t *vp;

		sr = (const struct ieee80211req_scan_result *) cp;
		vp = ((const u_int8_t *)sr) + sr->isr_ie_off;
		printf("%-*.*s  %s  %3d  %3dM %3d:%-3d  %3d %-4.4s"
			, ssidmax
			  , copy_essid(ssid, ssidmax, vp, sr->isr_ssid_len)
			  , ssid
			, ether_ntoa((const struct ether_addr *) sr->isr_bssid)
			, ieee80211_mhz2ieee(sr->isr_freq, sr->isr_flags)
			, getmaxrate(sr->isr_rates, sr->isr_nrates)
			, (sr->isr_rssi/2)+sr->isr_noise, sr->isr_noise
			, sr->isr_intval
			, getcaps(sr->isr_capinfo)
		);
		printies(vp + sr->isr_ssid_len, sr->isr_ie_len, 24);
		printf("\n");
		cp += sr->isr_len, len -= sr->isr_len;
	} while (len >= sizeof(struct ieee80211req_scan_result));
}

#include <net80211/ieee80211_freebsd.h>

static void
scan_and_wait(int s)
{
	struct ieee80211req ireq;
	int sroute;

	sroute = socket(PF_ROUTE, SOCK_RAW, 0);
	if (sroute < 0) {
		perror("socket(PF_ROUTE,SOCK_RAW)");
		return;
	}
	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_SCAN_REQ;
	/* NB: only root can trigger a scan so ignore errors */
	if (ioctl(s, SIOCS80211, &ireq) >= 0) {
		char buf[2048];
		struct if_announcemsghdr *ifan;
		struct rt_msghdr *rtm;

		do {
			if (read(sroute, buf, sizeof(buf)) < 0) {
				perror("read(PF_ROUTE)");
				break;
			}
			rtm = (struct rt_msghdr *) buf;
			if (rtm->rtm_version != RTM_VERSION)
				break;
			ifan = (struct if_announcemsghdr *) rtm;
		} while (rtm->rtm_type != RTM_IEEE80211 ||
		    ifan->ifan_what != RTM_IEEE80211_SCAN);
	}
	close(sroute);
}

static
DECL_CMD_FUNC(set80211scan, val, d)
{
	scan_and_wait(s);
	list_scan(s);
}

static enum ieee80211_opmode get80211opmode(int s);

static void
list_stations(int s)
{
	union {
		struct ieee80211req_sta_req req;
		uint8_t buf[24*1024];
	} u;
	enum ieee80211_opmode opmode = get80211opmode(s);
	struct ieee80211req ireq;
	const uint8_t *cp;
	int len;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	/* broadcast address =>'s get all stations */
	(void) memset(u.req.is_u.macaddr, 0xff, IEEE80211_ADDR_LEN);
	if (opmode == IEEE80211_M_STA) {
		/*
		 * Get information about the associated AP.
		 */
		ireq.i_type = IEEE80211_IOC_BSSID;
		ireq.i_data = u.req.is_u.macaddr;
		ireq.i_len = IEEE80211_ADDR_LEN;
		(void) ioctl(s, SIOCG80211, &ireq);
	}
	ireq.i_type = IEEE80211_IOC_STA_INFO;
	ireq.i_data = &u;
	ireq.i_len = sizeof(u);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		errx(1, "unable to get station information");
	len = ireq.i_len;
	if (len < sizeof(struct ieee80211req_sta_info))
		return;

	getchaninfo(s);

	printf("%-17.17s %4s %4s %4s %4s %4s %6s %6s %4s %4s\n"
		, "ADDR"
		, "AID"
		, "CHAN"
		, "RATE"
		, "RSSI"
		, "IDLE"
		, "TXSEQ"
		, "RXSEQ"
		, "CAPS"
		, "FLAG"
	);
	cp = (const uint8_t *) u.req.info;
	do {
		const struct ieee80211req_sta_info *si;

		si = (const struct ieee80211req_sta_info *) cp;
		if (si->isi_len < sizeof(*si))
			break;
		printf("%s %4u %4d %3dM %3.1f %4d %6d %6d %-4.4s %-4.4s"
			, ether_ntoa((const struct ether_addr*) si->isi_macaddr)
			, IEEE80211_AID(si->isi_associd)
			, ieee80211_mhz2ieee(si->isi_freq, si->isi_flags)
			, (si->isi_rates[si->isi_txrate] & IEEE80211_RATE_VAL)/2
			, si->isi_rssi/2.
			, si->isi_inact
			, si->isi_txseqs[0]
			, si->isi_rxseqs[0]
			, getcaps(si->isi_capinfo)
			, getflags(si->isi_state)
		);
		printies(cp + si->isi_ie_off, si->isi_ie_len, 24);
		printf("\n");
		cp += si->isi_len, len -= si->isi_len;
	} while (len >= sizeof(struct ieee80211req_sta_info));
}

static const char *
get_chaninfo(const struct ieee80211_channel *c, int precise,
	char buf[], size_t bsize)
{
	buf[0] = '\0';
	if (IEEE80211_IS_CHAN_FHSS(c))
		strlcat(buf, " FHSS", bsize);
	if (IEEE80211_IS_CHAN_A(c)) {
		if (IEEE80211_IS_CHAN_HALF(c))
			strlcat(buf, " 11a/10Mhz", bsize);
		else if (IEEE80211_IS_CHAN_QUARTER(c))
			strlcat(buf, " 11a/5Mhz", bsize);
		else
			strlcat(buf, " 11a", bsize);
	}
	if (IEEE80211_IS_CHAN_ANYG(c)) {
		if (IEEE80211_IS_CHAN_HALF(c))
			strlcat(buf, " 11g/10Mhz", bsize);
		else if (IEEE80211_IS_CHAN_QUARTER(c))
			strlcat(buf, " 11g/5Mhz", bsize);
		else
			strlcat(buf, " 11g", bsize);
	} else if (IEEE80211_IS_CHAN_B(c))
		strlcat(buf, " 11b", bsize);
	if (IEEE80211_IS_CHAN_TURBO(c))
		strlcat(buf, " Turbo", bsize);
	if (precise) {
		if (IEEE80211_IS_CHAN_HT20(c))
			strlcat(buf, " ht/20", bsize);
		else if (IEEE80211_IS_CHAN_HT40D(c))
			strlcat(buf, " ht/40-", bsize);
		else if (IEEE80211_IS_CHAN_HT40U(c))
			strlcat(buf, " ht/40+", bsize);
	} else {
		if (IEEE80211_IS_CHAN_HT(c))
			strlcat(buf, " ht", bsize);
	}
	return buf;
}

static void
print_chaninfo(const struct ieee80211_channel *c)
{
	char buf[14];

	printf("Channel %3u : %u%c Mhz%-14.14s",
		ieee80211_mhz2ieee(c->ic_freq, c->ic_flags), c->ic_freq,
		IEEE80211_IS_CHAN_PASSIVE(c) ? '*' : ' ',
		get_chaninfo(c, verbose, buf, sizeof(buf)));
}

static void
list_channels(int s, int allchans)
{
	struct ieee80211req_chaninfo achans;
	uint8_t reported[IEEE80211_CHAN_BYTES];
	const struct ieee80211_channel *c;
	int i, half;

	getchaninfo(s);
	memset(&achans, 0, sizeof(achans));
	memset(reported, 0, sizeof(reported));
	if (!allchans) {
		struct ieee80211req_chanlist active;
		struct ieee80211req ireq;

		(void) memset(&ireq, 0, sizeof(ireq));
		(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
		ireq.i_type = IEEE80211_IOC_CHANLIST;
		ireq.i_data = &active;
		ireq.i_len = sizeof(active);
		if (ioctl(s, SIOCG80211, &ireq) < 0)
			errx(1, "unable to get active channel list");
		memset(&achans, 0, sizeof(achans));
		for (i = 0; i < chaninfo.ic_nchans; i++) {
			c = &chaninfo.ic_chans[i];
			if (!isset(active.ic_channels, c->ic_ieee))
				continue;
			/*
			 * Suppress compatible duplicates unless
			 * verbose.  The kernel gives us it's
			 * complete channel list which has separate
			 * entries for 11g/11b and 11a/turbo.
			 */
			if (isset(reported, c->ic_ieee) && !verbose) {
				/* XXX we assume duplicates are adjacent */
				achans.ic_chans[achans.ic_nchans-1] = *c;
			} else {
				achans.ic_chans[achans.ic_nchans++] = *c;
				setbit(reported, c->ic_ieee);
			}
		}
	} else {
		for (i = 0; i < chaninfo.ic_nchans; i++) {
			c = &chaninfo.ic_chans[i];
			/* suppress duplicates as above */
			if (isset(reported, c->ic_ieee) && !verbose) {
				/* XXX we assume duplicates are adjacent */
				achans.ic_chans[achans.ic_nchans-1] = *c;
			} else {
				achans.ic_chans[achans.ic_nchans++] = *c;
				setbit(reported, c->ic_ieee);
			}
		}
	}
	half = achans.ic_nchans / 2;
	if (achans.ic_nchans % 2)
		half++;

	for (i = 0; i < achans.ic_nchans / 2; i++) {
		print_chaninfo(&achans.ic_chans[i]);
		print_chaninfo(&achans.ic_chans[half+i]);
		printf("\n");
	}
	if (achans.ic_nchans % 2) {
		print_chaninfo(&achans.ic_chans[i]);
		printf("\n");
	}
}

static void
print_txpow(const struct ieee80211_channel *c)
{
	printf("Channel %3u : %u Mhz %3.1f reg %2d  ",
	    c->ic_ieee, c->ic_freq,
	    c->ic_maxpower/2., c->ic_maxregpower);
}

static void
print_txpow_verbose(const struct ieee80211_channel *c)
{
	print_chaninfo(c);
	printf("min %4.1f dBm  max %3.1f dBm  reg %2d dBm",
	    c->ic_minpower/2., c->ic_maxpower/2., c->ic_maxregpower);
	/* indicate where regulatory cap limits power use */
	if (c->ic_maxpower > 2*c->ic_maxregpower)
		printf(" <");
}

static void
list_txpow(int s)
{
	struct ieee80211req_chaninfo achans;
	uint8_t reported[IEEE80211_CHAN_BYTES];
	struct ieee80211_channel *c, *prev;
	int i, half;

	getchaninfo(s);
	memset(&achans, 0, sizeof(achans));
	memset(reported, 0, sizeof(reported));
	for (i = 0; i < chaninfo.ic_nchans; i++) {
		c = &chaninfo.ic_chans[i];
		/* suppress duplicates as above */
		if (isset(reported, c->ic_ieee) && !verbose) {
			/* XXX we assume duplicates are adjacent */
			prev = &achans.ic_chans[achans.ic_nchans-1];
			/* display highest power on channel */
			if (c->ic_maxpower > prev->ic_maxpower)
				*prev = *c;
		} else {
			achans.ic_chans[achans.ic_nchans++] = *c;
			setbit(reported, c->ic_ieee);
		}
	}
	if (!verbose) {
		half = achans.ic_nchans / 2;
		if (achans.ic_nchans % 2)
			half++;

		for (i = 0; i < achans.ic_nchans / 2; i++) {
			print_txpow(&achans.ic_chans[i]);
			print_txpow(&achans.ic_chans[half+i]);
			printf("\n");
		}
		if (achans.ic_nchans % 2) {
			print_txpow(&achans.ic_chans[i]);
			printf("\n");
		}
	} else {
		for (i = 0; i < achans.ic_nchans; i++) {
			print_txpow_verbose(&achans.ic_chans[i]);
			printf("\n");
		}
	}
}

static void
list_keys(int s)
{
}

#define	IEEE80211_C_BITS \
"\020\1WEP\2TKIP\3AES\4AES_CCM\6CKIP\7FF\10TURBOP\11IBSS\12PMGT\13HOSTAP\14AHDEMO" \
"\15SWRETRY\16TXPMGT\17SHSLOT\20SHPREAMBLE\21MONITOR\22TKIPMIC\30WPA1" \
"\31WPA2\32BURST\33WME\34WDS\36BGSCAN\37TXFRAG"

static void
list_capabilities(int s)
{
	struct ieee80211req ireq;
	u_int32_t caps;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_DRIVER_CAPS;
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		errx(1, "unable to get driver capabilities");
	caps = (((u_int16_t) ireq.i_val) << 16) | ((u_int16_t) ireq.i_len);
	printb(name, caps, IEEE80211_C_BITS);
	putchar('\n');
}

static void
list_wme(int s)
{
	static const char *acnames[] = { "AC_BE", "AC_BK", "AC_VI", "AC_VO" };
	struct ieee80211req ireq;
	int ac;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_len = 0;
	for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
again:
		if (ireq.i_len & IEEE80211_WMEPARAM_BSS)
			printf("\t%s", "     ");
		else
			printf("\t%s", acnames[ac]);

		ireq.i_len = (ireq.i_len & IEEE80211_WMEPARAM_BSS) | ac;

		/* show WME BSS parameters */
		ireq.i_type = IEEE80211_IOC_WME_CWMIN;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			printf(" cwmin %2u", ireq.i_val);
		ireq.i_type = IEEE80211_IOC_WME_CWMAX;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			printf(" cwmax %2u", ireq.i_val);
		ireq.i_type = IEEE80211_IOC_WME_AIFS;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			printf(" aifs %2u", ireq.i_val);
		ireq.i_type = IEEE80211_IOC_WME_TXOPLIMIT;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			printf(" txopLimit %3u", ireq.i_val);
		ireq.i_type = IEEE80211_IOC_WME_ACM;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				printf(" acm");
			else if (verbose)
				printf(" -acm");
		}
		/* !BSS only */
		if ((ireq.i_len & IEEE80211_WMEPARAM_BSS) == 0) {
			ireq.i_type = IEEE80211_IOC_WME_ACKPOLICY;
			if (ioctl(s, SIOCG80211, &ireq) != -1) {
				if (!ireq.i_val)
					printf(" -ack");
				else if (verbose)
					printf(" ack");
			}
		}
		printf("\n");
		if ((ireq.i_len & IEEE80211_WMEPARAM_BSS) == 0) {
			ireq.i_len |= IEEE80211_WMEPARAM_BSS;
			goto again;
		} else
			ireq.i_len &= ~IEEE80211_WMEPARAM_BSS;
	}
}

static void
list_mac(int s)
{
	struct ieee80211req ireq;
	struct ieee80211req_maclist *acllist;
	int i, nacls, policy;
	char c;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name)); /* XXX ?? */
	ireq.i_type = IEEE80211_IOC_MACCMD;
	ireq.i_val = IEEE80211_MACCMD_POLICY;
	if (ioctl(s, SIOCG80211, &ireq) < 0) {
		if (errno == EINVAL) {
			printf("No acl policy loaded\n");
			return;
		}
		err(1, "unable to get mac policy");
	}
	policy = ireq.i_val;

	ireq.i_val = IEEE80211_MACCMD_LIST;
	ireq.i_len = 0;
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		err(1, "unable to get mac acl list size");
	if (ireq.i_len == 0)		/* NB: no acls */
		return;

	ireq.i_data = malloc(ireq.i_len);
	if (ireq.i_data == NULL)
		err(1, "out of memory for acl list");

	if (ioctl(s, SIOCG80211, &ireq) < 0)
		err(1, "unable to get mac acl list");
	if (policy == IEEE80211_MACCMD_POLICY_OPEN) {
		if (verbose)
			printf("policy: open\n");
		c = '*';
	} else if (policy == IEEE80211_MACCMD_POLICY_ALLOW) {
		if (verbose)
			printf("policy: allow\n");
		c = '+';
	} else if (policy == IEEE80211_MACCMD_POLICY_DENY) {
		if (verbose)
			printf("policy: deny\n");
		c = '-';
	} else {
		printf("policy: unknown (%u)\n", policy);
		c = '?';
	}
	nacls = ireq.i_len / sizeof(*acllist);
	acllist = (struct ieee80211req_maclist *) ireq.i_data;
	for (i = 0; i < nacls; i++)
		printf("%c%s\n", c, ether_ntoa(
			(const struct ether_addr *) acllist[i].ml_macaddr));
}

static
DECL_CMD_FUNC(set80211list, arg, d)
{
#define	iseq(a,b)	(strncasecmp(a,b,sizeof(b)-1) == 0)

	if (iseq(arg, "sta"))
		list_stations(s);
	else if (iseq(arg, "scan") || iseq(arg, "ap"))
		list_scan(s);
	else if (iseq(arg, "chan") || iseq(arg, "freq"))
		list_channels(s, 1);
	else if (iseq(arg, "active"))
		list_channels(s, 0);
	else if (iseq(arg, "keys"))
		list_keys(s);
	else if (iseq(arg, "caps"))
		list_capabilities(s);
	else if (iseq(arg, "wme"))
		list_wme(s);
	else if (iseq(arg, "mac"))
		list_mac(s);
	else if (iseq(arg, "txpow"))
		list_txpow(s);
	else
		errx(1, "Don't know how to list %s for %s", arg, name);
#undef iseq
}

static enum ieee80211_opmode
get80211opmode(int s)
{
	struct ifmediareq ifmr;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) >= 0) {
		if (ifmr.ifm_current & IFM_IEEE80211_ADHOC)
			return IEEE80211_M_IBSS;	/* XXX ahdemo */
		if (ifmr.ifm_current & IFM_IEEE80211_HOSTAP)
			return IEEE80211_M_HOSTAP;
		if (ifmr.ifm_current & IFM_IEEE80211_MONITOR)
			return IEEE80211_M_MONITOR;
	}
	return IEEE80211_M_STA;
}

#if 0
static void
printcipher(int s, struct ieee80211req *ireq, int keylenop)
{
	switch (ireq->i_val) {
	case IEEE80211_CIPHER_WEP:
		ireq->i_type = keylenop;
		if (ioctl(s, SIOCG80211, ireq) != -1)
			printf("WEP-%s", 
			    ireq->i_len <= 5 ? "40" :
			    ireq->i_len <= 13 ? "104" : "128");
		else
			printf("WEP");
		break;
	case IEEE80211_CIPHER_TKIP:
		printf("TKIP");
		break;
	case IEEE80211_CIPHER_AES_OCB:
		printf("AES-OCB");
		break;
	case IEEE80211_CIPHER_AES_CCM:
		printf("AES-CCM");
		break;
	case IEEE80211_CIPHER_CKIP:
		printf("CKIP");
		break;
	case IEEE80211_CIPHER_NONE:
		printf("NONE");
		break;
	default:
		printf("UNKNOWN (0x%x)", ireq->i_val);
		break;
	}
}
#endif

#define	MAXCOL	78
static	int col;
static	char spacer;

static void
LINE_BREAK(void)
{
	if (spacer != '\t') {
		printf("\n");
		spacer = '\t';
	}
	col = 8;	/* 8-col tab */
}

static void
LINE_CHECK(const char *fmt, ...)
{
	char buf[80];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf+1, sizeof(buf)-1, fmt, ap);
	va_end(ap);
	col += 1+n;
	if (col > MAXCOL) {
		LINE_BREAK();
		col += n;
	}
	buf[0] = spacer;
	printf("%s", buf);
	spacer = ' ';
}

static void
printkey(const struct ieee80211req_key *ik)
{
	static const uint8_t zerodata[IEEE80211_KEYBUF_SIZE];
	int keylen = ik->ik_keylen;
	int printcontents;

	printcontents = printkeys &&
		(memcmp(ik->ik_keydata, zerodata, keylen) != 0 || verbose);
	if (printcontents)
		LINE_BREAK();
	switch (ik->ik_type) {
	case IEEE80211_CIPHER_WEP:
		/* compatibility */
		LINE_CHECK("wepkey %u:%s", ik->ik_keyix+1,
		    keylen <= 5 ? "40-bit" :
		    keylen <= 13 ? "104-bit" : "128-bit");
		break;
	case IEEE80211_CIPHER_TKIP:
		if (keylen > 128/8)
			keylen -= 128/8;	/* ignore MIC for now */
		LINE_CHECK("TKIP %u:%u-bit", ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_AES_OCB:
		LINE_CHECK("AES-OCB %u:%u-bit", ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_AES_CCM:
		LINE_CHECK("AES-CCM %u:%u-bit", ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_CKIP:
		LINE_CHECK("CKIP %u:%u-bit", ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_NONE:
		LINE_CHECK("NULL %u:%u-bit", ik->ik_keyix+1, 8*keylen);
		break;
	default:
		LINE_CHECK("UNKNOWN (0x%x) %u:%u-bit",
			ik->ik_type, ik->ik_keyix+1, 8*keylen);
		break;
	}
	if (printcontents) {
		int i;

		printf(" <");
		for (i = 0; i < keylen; i++)
			printf("%02x", ik->ik_keydata[i]);
		printf(">");
		if (ik->ik_type != IEEE80211_CIPHER_WEP &&
		    (ik->ik_keyrsc != 0 || verbose))
			printf(" rsc %ju", (uintmax_t)ik->ik_keyrsc);
		if (ik->ik_type != IEEE80211_CIPHER_WEP &&
		    (ik->ik_keytsc != 0 || verbose))
			printf(" tsc %ju", (uintmax_t)ik->ik_keytsc);
		if (ik->ik_flags != 0 && verbose) {
			const char *sep = " ";

			if (ik->ik_flags & IEEE80211_KEY_XMIT)
				printf("%stx", sep), sep = "+";
			if (ik->ik_flags & IEEE80211_KEY_RECV)
				printf("%srx", sep), sep = "+";
			if (ik->ik_flags & IEEE80211_KEY_DEFAULT)
				printf("%sdef", sep), sep = "+";
		}
		LINE_BREAK();
	}
}

static void
ieee80211_status(int s)
{
	static const uint8_t zerobssid[IEEE80211_ADDR_LEN];
	enum ieee80211_opmode opmode = get80211opmode(s);
	int i, num, wpa, wme, bgscan, bgscaninterval;
	struct ieee80211req ireq;
	u_int8_t data[32];
	struct ieee80211_channel chan;
	const struct ieee80211_channel *c;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_data = &data;

	wpa = 0;		/* unknown/not set */
	bgscan = 0;		/* unknown/not set */

	ireq.i_type = IEEE80211_IOC_SSID;
	ireq.i_val = -1;
	if (ioctl(s, SIOCG80211, &ireq) < 0) {
		/* If we can't get the SSID, this isn't an 802.11 device. */
		return;
	}
	num = 0;
	ireq.i_type = IEEE80211_IOC_NUMSSIDS;
	if (ioctl(s, SIOCG80211, &ireq) >= 0)
		num = ireq.i_val;
	printf("\tssid ");
	if (num > 1) {
		ireq.i_type = IEEE80211_IOC_SSID;
		for (ireq.i_val = 0; ireq.i_val < num; ireq.i_val++) {
			if (ioctl(s, SIOCG80211, &ireq) >= 0 && ireq.i_len > 0) {
				printf(" %d:", ireq.i_val + 1);
				print_string(data, ireq.i_len);
			}
		}
	} else
		print_string(data, ireq.i_len);

	ireq.i_data = &chan;
	ireq.i_len = sizeof(chan);
	ireq.i_type = IEEE80211_IOC_CURCHAN;
	if (ioctl(s, SIOCG80211, &ireq) < 0) {
		/* fall back to legacy ioctl */
		ireq.i_data = NULL;
		ireq.i_len = 0;
		ireq.i_type = IEEE80211_IOC_CHANNEL;
		if (ioctl(s, SIOCG80211, &ireq) < 0)
			goto end;
		getchaninfo(s);
		mapchan(&chan, ireq.i_val, 0);
	}
	c = &chan;
	if (c->ic_freq != IEEE80211_CHAN_ANY) {
		char buf[14];
		printf(" channel %d (%u Mhz%s)", c->ic_ieee, c->ic_freq,
			get_chaninfo(c, 1, buf, sizeof(buf)));
	} else if (verbose)
		printf(" channel UNDEF");
	ireq.i_data = &data;    /* reset data buffer */

	ireq.i_type = IEEE80211_IOC_BSSID;
	ireq.i_len = IEEE80211_ADDR_LEN;
	if (ioctl(s, SIOCG80211, &ireq) >= 0 &&
	    (memcmp(ireq.i_data, zerobssid, sizeof(zerobssid)) != 0 || verbose))
		printf(" bssid %s", ether_ntoa(ireq.i_data));

	ireq.i_type = IEEE80211_IOC_STATIONNAME;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		printf("\n\tstationname ");
		print_string(data, ireq.i_len);
	}

	spacer = ' ';		/* force first break */
	LINE_BREAK();

	ireq.i_type = IEEE80211_IOC_AUTHMODE;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		switch (ireq.i_val) {
			case IEEE80211_AUTH_NONE:
				LINE_CHECK("authmode NONE");
				break;
			case IEEE80211_AUTH_OPEN:
				LINE_CHECK("authmode OPEN");
				break;
			case IEEE80211_AUTH_SHARED:
				LINE_CHECK("authmode SHARED");
				break;
			case IEEE80211_AUTH_8021X:
				LINE_CHECK("authmode 802.1x");
				break;
			case IEEE80211_AUTH_WPA:
				ireq.i_type = IEEE80211_IOC_WPA;
				if (ioctl(s, SIOCG80211, &ireq) != -1)
					wpa = ireq.i_val;
				if (!wpa)
					wpa = 1;	/* default to WPA1 */
				switch (wpa) {
				case 2:
					LINE_CHECK("authmode WPA2/802.11i");
					break;
				case 3:
					LINE_CHECK("authmode WPA1+WPA2/802.11i");
					break;
				default:
					LINE_CHECK("authmode WPA");
					break;
				}
				break;
			case IEEE80211_AUTH_AUTO:
				LINE_CHECK("authmode AUTO");
				break;
			default:
				LINE_CHECK("authmode UNKNOWN (0x%x)",
					ireq.i_val);
				break;
		}
	}

	ireq.i_type = IEEE80211_IOC_WEP;
	if (ioctl(s, SIOCG80211, &ireq) != -1 &&
	    ireq.i_val != IEEE80211_WEP_NOSUP) {
		int firstkey, wepmode;

		wepmode = ireq.i_val;
		switch (wepmode) {
			case IEEE80211_WEP_OFF:
				LINE_CHECK("privacy OFF");
				break;
			case IEEE80211_WEP_ON:
				LINE_CHECK("privacy ON");
				break;
			case IEEE80211_WEP_MIXED:
				LINE_CHECK("privacy MIXED");
				break;
			default:
				LINE_CHECK("privacy UNKNOWN (0x%x)", wepmode);
				break;
		}

		/*
		 * If we get here then we've got WEP support so we need
		 * to print WEP status.
		 */

		ireq.i_type = IEEE80211_IOC_WEPTXKEY;
		if (ioctl(s, SIOCG80211, &ireq) < 0) {
			warn("WEP support, but no tx key!");
			goto end;
		}
		if (ireq.i_val != -1)
			LINE_CHECK("deftxkey %d", ireq.i_val+1);
		else if (wepmode != IEEE80211_WEP_OFF || verbose)
			LINE_CHECK("deftxkey UNDEF");

		ireq.i_type = IEEE80211_IOC_NUMWEPKEYS;
		if (ioctl(s, SIOCG80211, &ireq) < 0) {
			warn("WEP support, but no NUMWEPKEYS support!");
			goto end;
		}
		num = ireq.i_val;

		firstkey = 1;
		for (i = 0; i < num; i++) {
			struct ieee80211req_key ik;

			memset(&ik, 0, sizeof(ik));
			ik.ik_keyix = i;
			ireq.i_type = IEEE80211_IOC_WPAKEY;
			ireq.i_data = &ik;
			ireq.i_len = sizeof(ik);
			if (ioctl(s, SIOCG80211, &ireq) < 0) {
				warn("WEP support, but can get keys!");
				goto end;
			}
			if (ik.ik_keylen != 0) {
				if (verbose)
					LINE_BREAK();
				printkey(&ik);
				firstkey = 0;
			}
		}
		ireq.i_data = &data;    /* reset data buffer */
	}

	ireq.i_type = IEEE80211_IOC_POWERSAVE;
	if (ioctl(s, SIOCG80211, &ireq) != -1 &&
	    ireq.i_val != IEEE80211_POWERSAVE_NOSUP ) {
		if (ireq.i_val != IEEE80211_POWERSAVE_OFF || verbose) {
			switch (ireq.i_val) {
				case IEEE80211_POWERSAVE_OFF:
					LINE_CHECK("powersavemode OFF");
					break;
				case IEEE80211_POWERSAVE_CAM:
					LINE_CHECK("powersavemode CAM");
					break;
				case IEEE80211_POWERSAVE_PSP:
					LINE_CHECK("powersavemode PSP");
					break;
				case IEEE80211_POWERSAVE_PSP_CAM:
					LINE_CHECK("powersavemode PSP-CAM");
					break;
			}
			ireq.i_type = IEEE80211_IOC_POWERSAVESLEEP;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("powersavesleep %d", ireq.i_val);
		}
	}

	ireq.i_type = IEEE80211_IOC_TXPOWMAX;
	if (ioctl(s, SIOCG80211, &ireq) != -1)
		LINE_CHECK("txpowmax %d", ireq.i_val);

	if (verbose) {
		ireq.i_type = IEEE80211_IOC_TXPOWER;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			LINE_CHECK("txpower %d", ireq.i_val);
	}

	ireq.i_type = IEEE80211_IOC_RTSTHRESHOLD;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val != IEEE80211_RTS_MAX || verbose)
			LINE_CHECK("rtsthreshold %d", ireq.i_val);
	}

	ireq.i_type = IEEE80211_IOC_FRAGTHRESHOLD;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val != IEEE80211_FRAG_MAX || verbose)
			LINE_CHECK("fragthreshold %d", ireq.i_val);
	}
	ireq.i_type = IEEE80211_IOC_BMISSTHRESHOLD;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val != IEEE80211_HWBMISS_MAX || verbose)
			LINE_CHECK("bmiss %d", ireq.i_val);
	}

	ireq.i_type = IEEE80211_IOC_MCAST_RATE;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val != 2*1 || verbose) {
			if (ireq.i_val == 11)
				LINE_CHECK("mcastrate 5.5");
			else
				LINE_CHECK("mcastrate %d", ireq.i_val/2);
		}
	}

	ireq.i_type = IEEE80211_IOC_BGSCAN_INTERVAL;
	if (ioctl(s, SIOCG80211, &ireq) != -1)
		bgscaninterval = ireq.i_val;
	else
		bgscaninterval = -1;

	ireq.i_type = IEEE80211_IOC_SCANVALID;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val != bgscaninterval || verbose)
			LINE_CHECK("scanvalid %u", ireq.i_val);
	}

	ireq.i_type = IEEE80211_IOC_BGSCAN;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		bgscan = ireq.i_val;
		if (ireq.i_val)
			LINE_CHECK("bgscan");
		else if (verbose)
			LINE_CHECK("-bgscan");
	}
	if (bgscan || verbose) {
		if (bgscaninterval != -1)
			LINE_CHECK("bgscanintvl %u", bgscaninterval);
		ireq.i_type = IEEE80211_IOC_BGSCAN_IDLE;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			LINE_CHECK("bgscanidle %u", ireq.i_val);
		if (IEEE80211_IS_CHAN_A(c) || verbose) {
			ireq.i_type = IEEE80211_IOC_ROAM_RSSI_11A;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("roam:rssi11a %d", ireq.i_val);
			ireq.i_type = IEEE80211_IOC_ROAM_RATE_11A;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("roam:rate11a %u", ireq.i_val/2);
		}
		if (IEEE80211_IS_CHAN_B(c) || verbose) {
			ireq.i_type = IEEE80211_IOC_ROAM_RSSI_11B;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("roam:rssi11b %d", ireq.i_val);
			ireq.i_type = IEEE80211_IOC_ROAM_RATE_11B;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("roam:rate11b %u", ireq.i_val/2);
		}
		if (IEEE80211_IS_CHAN_ANYG(c) || verbose) {
			ireq.i_type = IEEE80211_IOC_ROAM_RSSI_11G;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("roam:rssi11g %d", ireq.i_val);
			ireq.i_type = IEEE80211_IOC_ROAM_RATE_11G;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("roam:rate11g %u", ireq.i_val/2);
		}
	}

	if (IEEE80211_IS_CHAN_ANYG(c) || verbose) {
		ireq.i_type = IEEE80211_IOC_PUREG;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				LINE_CHECK("pureg");
			else if (verbose)
				LINE_CHECK("-pureg");
		}
		ireq.i_type = IEEE80211_IOC_PROTMODE;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			switch (ireq.i_val) {
				case IEEE80211_PROTMODE_OFF:
					LINE_CHECK("protmode OFF");
					break;
				case IEEE80211_PROTMODE_CTS:
					LINE_CHECK("protmode CTS");
					break;
				case IEEE80211_PROTMODE_RTSCTS:
					LINE_CHECK("protmode RTSCTS");
					break;
				default:
					LINE_CHECK("protmode UNKNOWN (0x%x)",
						ireq.i_val);
					break;
			}
		}
	}

	ireq.i_type = IEEE80211_IOC_WME;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		wme = ireq.i_val;
		if (wme)
			LINE_CHECK("wme");
		else if (verbose)
			LINE_CHECK("-wme");
	} else
		wme = 0;

	ireq.i_type = IEEE80211_IOC_BURST;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val)
			LINE_CHECK("burst");
		else if (verbose)
			LINE_CHECK("-burst");
	}

	ireq.i_type = IEEE80211_IOC_FF;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val)
			LINE_CHECK("ff");
		else if (verbose)
			LINE_CHECK("-ff");
	}
	ireq.i_type = IEEE80211_IOC_TURBOP;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val)
			LINE_CHECK("dturbo");
		else if (verbose)
			LINE_CHECK("-dturbo");
	}

	if (opmode == IEEE80211_M_HOSTAP) {
		ireq.i_type = IEEE80211_IOC_HIDESSID;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				LINE_CHECK("hidessid");
			else if (verbose)
				LINE_CHECK("-hidessid");
		}

		ireq.i_type = IEEE80211_IOC_APBRIDGE;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (!ireq.i_val)
				LINE_CHECK("-apbridge");
			else if (verbose)
				LINE_CHECK("apbridge");
		}

		ireq.i_type = IEEE80211_IOC_DTIM_PERIOD;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			LINE_CHECK("dtimperiod %u", ireq.i_val);

		ireq.i_type = IEEE80211_IOC_DOTH;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (!ireq.i_val)
				LINE_CHECK("-doth");
			else if (verbose)
				LINE_CHECK("doth");
		}
	} else {
		ireq.i_type = IEEE80211_IOC_ROAMING;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val != IEEE80211_ROAMING_AUTO || verbose) {
				switch (ireq.i_val) {
				case IEEE80211_ROAMING_DEVICE:
					LINE_CHECK("roaming DEVICE");
					break;
				case IEEE80211_ROAMING_AUTO:
					LINE_CHECK("roaming AUTO");
					break;
				case IEEE80211_ROAMING_MANUAL:
					LINE_CHECK("roaming MANUAL");
					break;
				default:
					LINE_CHECK("roaming UNKNOWN (0x%x)",
						ireq.i_val);
					break;
				}
			}
		}
	}
	ireq.i_type = IEEE80211_IOC_BEACON_INTERVAL;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val)
			LINE_CHECK("bintval %u", ireq.i_val);
		else if (verbose)
			LINE_CHECK("bintval %u", ireq.i_val);
	}

	if (wme && verbose) {
		LINE_BREAK();
		list_wme(s);
	}

	if (wpa) {
		ireq.i_type = IEEE80211_IOC_COUNTERMEASURES;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				LINE_CHECK("countermeasures");
			else if (verbose)
				LINE_CHECK("-countermeasures");
		}
#if 0
		/* XXX not interesting with WPA done in user space */
		ireq.i_type = IEEE80211_IOC_KEYMGTALGS;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
		}

		ireq.i_type = IEEE80211_IOC_MCASTCIPHER;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			LINE_CHECK("mcastcipher ");
			printcipher(s, &ireq, IEEE80211_IOC_MCASTKEYLEN);
			spacer = ' ';
		}

		ireq.i_type = IEEE80211_IOC_UCASTCIPHER;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			LINE_CHECK("ucastcipher ");
			printcipher(s, &ireq, IEEE80211_IOC_UCASTKEYLEN);
		}

		if (wpa & 2) {
			ireq.i_type = IEEE80211_IOC_RSNCAPS;
			if (ioctl(s, SIOCG80211, &ireq) != -1) {
				LINE_CHECK("RSN caps 0x%x", ireq.i_val);
				spacer = ' ';
			}
		}

		ireq.i_type = IEEE80211_IOC_UCASTCIPHERS;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
		}
#endif
		LINE_BREAK();
	}
	LINE_BREAK();

end:
	return;
}

static void
set80211(int s, int type, int val, int len, void *data)
{
	struct ieee80211req	ireq;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = type;
	ireq.i_val = val;
	ireq.i_len = len;
	ireq.i_data = data;
	if (ioctl(s, SIOCS80211, &ireq) < 0)
		err(1, "SIOCS80211");
}

static const char *
get_string(const char *val, const char *sep, u_int8_t *buf, int *lenp)
{
	int len;
	int hexstr;
	u_int8_t *p;

	len = *lenp;
	p = buf;
	hexstr = (val[0] == '0' && tolower((u_char)val[1]) == 'x');
	if (hexstr)
		val += 2;
	for (;;) {
		if (*val == '\0')
			break;
		if (sep != NULL && strchr(sep, *val) != NULL) {
			val++;
			break;
		}
		if (hexstr) {
			if (!isxdigit((u_char)val[0])) {
				warnx("bad hexadecimal digits");
				return NULL;
			}
			if (!isxdigit((u_char)val[1])) {
				warnx("odd count hexadecimal digits");
				return NULL;
			}
		}
		if (p >= buf + len) {
			if (hexstr)
				warnx("hexadecimal digits too long");
			else
				warnx("string too long");
			return NULL;
		}
		if (hexstr) {
#define	tohex(x)	(isdigit(x) ? (x) - '0' : tolower(x) - 'a' + 10)
			*p++ = (tohex((u_char)val[0]) << 4) |
			    tohex((u_char)val[1]);
#undef tohex
			val += 2;
		} else
			*p++ = *val++;
	}
	len = p - buf;
	/* The string "-" is treated as the empty string. */
	if (!hexstr && len == 1 && buf[0] == '-') {
		len = 0;
		memset(buf, 0, *lenp);
	} else if (len < *lenp)
		memset(p, 0, *lenp - len);
	*lenp = len;
	return val;
}

static void
print_string(const u_int8_t *buf, int len)
{
	int i;
	int hasspc;

	i = 0;
	hasspc = 0;
	for (; i < len; i++) {
		if (!isprint(buf[i]) && buf[i] != '\0')
			break;
		if (isspace(buf[i]))
			hasspc++;
	}
	if (i == len) {
		if (hasspc || len == 0 || buf[0] == '\0')
			printf("\"%.*s\"", len, buf);
		else
			printf("%.*s", len, buf);
	} else {
		printf("0x");
		for (i = 0; i < len; i++)
			printf("%02x", buf[i]);
	}
}

static struct cmd ieee80211_cmds[] = {
	DEF_CMD_ARG("ssid",		set80211ssid),
	DEF_CMD_ARG("nwid",		set80211ssid),
	DEF_CMD_ARG("stationname",	set80211stationname),
	DEF_CMD_ARG("station",		set80211stationname),	/* BSD/OS */
	DEF_CMD_ARG("channel",		set80211channel),
	DEF_CMD_ARG("authmode",		set80211authmode),
	DEF_CMD_ARG("powersavemode",	set80211powersavemode),
	DEF_CMD("powersave",	1,	set80211powersave),
	DEF_CMD("-powersave",	0,	set80211powersave),
	DEF_CMD_ARG("powersavesleep", 	set80211powersavesleep),
	DEF_CMD_ARG("wepmode",		set80211wepmode),
	DEF_CMD("wep",		1,	set80211wep),
	DEF_CMD("-wep",		0,	set80211wep),
	DEF_CMD_ARG("deftxkey",		set80211weptxkey),
	DEF_CMD_ARG("weptxkey",		set80211weptxkey),
	DEF_CMD_ARG("wepkey",		set80211wepkey),
	DEF_CMD_ARG("nwkey",		set80211nwkey),		/* NetBSD */
	DEF_CMD("-nwkey",	0,	set80211wep),		/* NetBSD */
	DEF_CMD_ARG("rtsthreshold",	set80211rtsthreshold),
	DEF_CMD_ARG("protmode",		set80211protmode),
	DEF_CMD_ARG("txpower",		set80211txpower),
	DEF_CMD_ARG("roaming",		set80211roaming),
	DEF_CMD("wme",		1,	set80211wme),
	DEF_CMD("-wme",		0,	set80211wme),
	DEF_CMD("hidessid",	1,	set80211hidessid),
	DEF_CMD("-hidessid",	0,	set80211hidessid),
	DEF_CMD("apbridge",	1,	set80211apbridge),
	DEF_CMD("-apbridge",	0,	set80211apbridge),
	DEF_CMD_ARG("chanlist",		set80211chanlist),
	DEF_CMD_ARG("bssid",		set80211bssid),
	DEF_CMD_ARG("ap",		set80211bssid),
	DEF_CMD("scan",	0,		set80211scan),
	DEF_CMD_ARG("list",		set80211list),
	DEF_CMD_ARG2("cwmin",		set80211cwmin),
	DEF_CMD_ARG2("cwmax",		set80211cwmax),
	DEF_CMD_ARG2("aifs",		set80211aifs),
	DEF_CMD_ARG2("txoplimit",	set80211txoplimit),
	DEF_CMD_ARG("acm",		set80211acm),
	DEF_CMD_ARG("-acm",		set80211noacm),
	DEF_CMD_ARG("ack",		set80211ackpolicy),
	DEF_CMD_ARG("-ack",		set80211noackpolicy),
	DEF_CMD_ARG2("bss:cwmin",	set80211bsscwmin),
	DEF_CMD_ARG2("bss:cwmax",	set80211bsscwmax),
	DEF_CMD_ARG2("bss:aifs",	set80211bssaifs),
	DEF_CMD_ARG2("bss:txoplimit",	set80211bsstxoplimit),
	DEF_CMD_ARG("dtimperiod",	set80211dtimperiod),
	DEF_CMD_ARG("bintval",		set80211bintval),
	DEF_CMD("mac:open",	IEEE80211_MACCMD_POLICY_OPEN,	set80211maccmd),
	DEF_CMD("mac:allow",	IEEE80211_MACCMD_POLICY_ALLOW,	set80211maccmd),
	DEF_CMD("mac:deny",	IEEE80211_MACCMD_POLICY_DENY,	set80211maccmd),
	DEF_CMD("mac:flush",	IEEE80211_MACCMD_FLUSH,		set80211maccmd),
	DEF_CMD("mac:detach",	IEEE80211_MACCMD_DETACH,	set80211maccmd),
	DEF_CMD_ARG("mac:add",		set80211addmac),
	DEF_CMD_ARG("mac:del",		set80211delmac),
	DEF_CMD_ARG("mac:kick",		set80211kickmac),
	DEF_CMD("pureg",	1,	set80211pureg),
	DEF_CMD("-pureg",	0,	set80211pureg),
	DEF_CMD("ff",		1,	set80211fastframes),
	DEF_CMD("-ff",		0,	set80211fastframes),
	DEF_CMD("dturbo",	1,	set80211dturbo),
	DEF_CMD("-dturbo",	0,	set80211dturbo),
	DEF_CMD("bgscan",	1,	set80211bgscan),
	DEF_CMD("-bgscan",	0,	set80211bgscan),
	DEF_CMD_ARG("bgscanidle",	set80211bgscanidle),
	DEF_CMD_ARG("bgscanintvl",	set80211bgscanintvl),
	DEF_CMD_ARG("scanvalid",	set80211scanvalid),
	DEF_CMD_ARG("roam:rssi11a",	set80211roamrssi11a),
	DEF_CMD_ARG("roam:rssi11b",	set80211roamrssi11b),
	DEF_CMD_ARG("roam:rssi11g",	set80211roamrssi11g),
	DEF_CMD_ARG("roam:rate11a",	set80211roamrate11a),
	DEF_CMD_ARG("roam:rate11b",	set80211roamrate11b),
	DEF_CMD_ARG("roam:rate11g",	set80211roamrate11g),
	DEF_CMD_ARG("mcastrate",	set80211mcastrate),
	DEF_CMD_ARG("fragthreshold",	set80211fragthreshold),
	DEF_CMD("burst",	1,	set80211burst),
	DEF_CMD("-burst",	0,	set80211burst),
	DEF_CMD_ARG("bmiss",		set80211bmissthreshold),
	DEF_CMD_ARG("bmissthreshold",	set80211bmissthreshold),
	DEF_CMD("doth",		1,	set80211doth),
	DEF_CMD("-doth",	0,	set80211doth),
};
static struct afswtch af_ieee80211 = {
	.af_name	= "af_ieee80211",
	.af_af		= AF_UNSPEC,
	.af_other_status = ieee80211_status,
};

static __constructor void
ieee80211_ctor(void)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(ieee80211_cmds);  i++)
		cmd_register(&ieee80211_cmds[i]);
	af_register(&af_ieee80211);
#undef N
}
