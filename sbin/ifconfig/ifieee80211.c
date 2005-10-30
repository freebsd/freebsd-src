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

#include "ifconfig.h"

static void set80211(int s, int type, int val, int len, u_int8_t *data);
static const char *get_string(const char *val, const char *sep,
    u_int8_t *buf, int *lenp);
static void print_string(const u_int8_t *buf, int len);

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
 * Convert IEEE channel number to MHz frequency.
 */
static u_int
ieee80211_ieee2mhz(u_int chan)
{
	if (chan == 14)
		return 2484;
	if (chan < 14)			/* 0-13 */
		return 2407 + chan*5;
	if (chan < 27)			/* 15-26 */
		return 2512 + ((chan-15)*20);
	return 5000 + (chan*5);
}

/*
 * Convert MHz frequency to IEEE channel number.
 */
static u_int
ieee80211_mhz2ieee(u_int freq)
{
	if (freq == 2484)
		return 14;
	if (freq < 2484)
		return (freq - 2407) / 5;
	if (freq < 5000)
		return 15 + ((freq - 2512) / 20);
	return (freq - 5000) / 5;
}

static void
set80211channel(const char *val, int d, int s, const struct afswtch *rafp)
{
	if (!isanyarg(val)) {
		int v = atoi(val);
		if (v > 255)		/* treat as frequency */
			v = ieee80211_mhz2ieee(v);
		set80211(s, IEEE80211_IOC_CHANNEL, v, 0, NULL);
	} else
		set80211(s, IEEE80211_IOC_CHANNEL, IEEE80211_CHAN_ANY, 0, NULL);
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
	set80211(s, IEEE80211_IOC_CHANLIST, 0,
		sizeof(chanlist), (uint8_t *) &chanlist);
#undef MAXCHAN
}

static void
set80211bssid(const char *val, int d, int s, const struct afswtch *rafp)
{

	if (!isanyarg(val)) {
		char *temp;
		struct sockaddr_dl sdl;

		temp = malloc(strlen(val) + 1);
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

	temp = malloc(strlen(val) + 1);
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

	temp = malloc(strlen(val) + 1);
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
	set80211(s, IEEE80211_IOC_MLME, 0, sizeof(mlme), (u_int8_t *) &mlme);
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

static
DECL_CMD_FUNC(set80211fragthreshold, val, d)
{
	set80211(s, IEEE80211_IOC_FRAGTHRESHOLD,
		isundefarg(val) ? IEEE80211_FRAG_MAX : atoi(val), 0, NULL);
}

static int
getmaxrate(uint8_t rates[15], uint8_t nrates)
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
				printie(" WPA", vp, 2+vp[1], maxcols);
			else if (iswmeoui(vp))
				printie(" WME", vp, 2+vp[1], maxcols);
			else if (isatherosoui(vp))
				printie(" ATH", vp, 2+vp[1], maxcols);
			else
				printie(" VEN", vp, 2+vp[1], maxcols);
			break;
		case IEEE80211_ELEMID_RSN:
			printie(" RSN", vp, 2+vp[1], maxcols);
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
	char ssid[14];
	uint8_t *cp;
	int len;

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

	printf("%-14.14s  %-17.17s  %4s %4s  %-5s %3s %4s\n"
		, "SSID"
		, "BSSID"
		, "CHAN"
		, "RATE"
		, "S:N"
		, "INT"
		, "CAPS"
	);
	cp = buf;
	do {
		struct ieee80211req_scan_result *sr;
		uint8_t *vp;

		sr = (struct ieee80211req_scan_result *) cp;
		vp = (u_int8_t *)(sr+1);
		printf("%-14.*s  %s  %3d  %3dM %2d:%-2d  %3d %-4.4s"
			, copy_essid(ssid, sizeof(ssid), vp, sr->isr_ssid_len)
				, ssid
			, ether_ntoa((const struct ether_addr *) sr->isr_bssid)
			, ieee80211_mhz2ieee(sr->isr_freq)
			, getmaxrate(sr->isr_rates, sr->isr_nrates)
			, sr->isr_rssi, sr->isr_noise
			, sr->isr_intval
			, getcaps(sr->isr_capinfo)
		);
		printies(vp + sr->isr_ssid_len, sr->isr_ie_len, 24);;
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

static void
list_stations(int s)
{
	uint8_t buf[24*1024];
	struct ieee80211req ireq;
	uint8_t *cp;
	int len;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_STA_INFO;
	ireq.i_data = buf;
	ireq.i_len = sizeof(buf);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		errx(1, "unable to get station information");
	len = ireq.i_len;
	if (len < sizeof(struct ieee80211req_sta_info))
		return;

	printf("%-17.17s %4s %4s %4s %4s %4s %6s %6s %4s %3s\n"
		, "ADDR"
		, "AID"
		, "CHAN"
		, "RATE"
		, "RSSI"
		, "IDLE"
		, "TXSEQ"
		, "RXSEQ"
		, "CAPS"
		, "ERP"
	);
	cp = buf;
	do {
		struct ieee80211req_sta_info *si;
		uint8_t *vp;

		si = (struct ieee80211req_sta_info *) cp;
		vp = (u_int8_t *)(si+1);
		printf("%s %4u %4d %3dM %4d %4d %6d %6d %-4.4s %3x"
			, ether_ntoa((const struct ether_addr*) si->isi_macaddr)
			, IEEE80211_AID(si->isi_associd)
			, ieee80211_mhz2ieee(si->isi_freq)
			, (si->isi_rates[si->isi_txrate] & IEEE80211_RATE_VAL)/2
			, si->isi_rssi
			, si->isi_inact
			, si->isi_txseqs[0]
			, si->isi_rxseqs[0]
			, getcaps(si->isi_capinfo)
			, si->isi_erp
		);
		printies(vp, si->isi_ie_len, 24);
		printf("\n");
		cp += si->isi_len, len -= si->isi_len;
	} while (len >= sizeof(struct ieee80211req_sta_info));
}

static void
print_chaninfo(const struct ieee80211_channel *c)
{
#define	IEEE80211_IS_CHAN_PASSIVE(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_PASSIVE))
	char buf[14];

	buf[0] = '\0';
	if (IEEE80211_IS_CHAN_FHSS(c))
		strlcat(buf, " FHSS", sizeof(buf));
	if (IEEE80211_IS_CHAN_A(c))
		strlcat(buf, " 11a", sizeof(buf));
	/* XXX 11g schizophrenia */
	if (IEEE80211_IS_CHAN_G(c) ||
	    IEEE80211_IS_CHAN_PUREG(c))
		strlcat(buf, " 11g", sizeof(buf));
	else if (IEEE80211_IS_CHAN_B(c))
		strlcat(buf, " 11b", sizeof(buf));
	if (IEEE80211_IS_CHAN_T(c))
		strlcat(buf, " Turbo", sizeof(buf));
	printf("Channel %3u : %u%c Mhz%-14.14s",
		ieee80211_mhz2ieee(c->ic_freq), c->ic_freq,
		IEEE80211_IS_CHAN_PASSIVE(c) ? '*' : ' ', buf);
#undef IEEE80211_IS_CHAN_PASSIVE
}

static void
list_channels(int s, int allchans)
{
	struct ieee80211req ireq;
	struct ieee80211req_chaninfo chans;
	struct ieee80211req_chaninfo achans;
	const struct ieee80211_channel *c;
	int i, half;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_CHANINFO;
	ireq.i_data = &chans;
	ireq.i_len = sizeof(chans);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		errx(1, "unable to get channel information");
	if (!allchans) {
		struct ieee80211req_chanlist active;

		ireq.i_type = IEEE80211_IOC_CHANLIST;
		ireq.i_data = &active;
		ireq.i_len = sizeof(active);
		if (ioctl(s, SIOCG80211, &ireq) < 0)
			errx(1, "unable to get active channel list");
		memset(&achans, 0, sizeof(achans));
		for (i = 0; i < chans.ic_nchans; i++) {
			c = &chans.ic_chans[i];
			if (isset(active.ic_channels, ieee80211_mhz2ieee(c->ic_freq)) || allchans)
				achans.ic_chans[achans.ic_nchans++] = *c;
		}
	} else
		achans = chans;
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
list_keys(int s)
{
}

#define	IEEE80211_C_BITS \
"\020\1WEP\2TKIP\3AES\4AES_CCM\6CKIP\11IBSS\12PMGT\13HOSTAP\14AHDEMO" \
"\15SWRETRY\16TXPMGT\17SHSLOT\20SHPREAMBLE\21MONITOR\22TKIPMIC\30WPA1" \
"\31WPA2\32BURST\33WME"

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

static const struct ieee80211_channel *
getchaninfo(int s, int chan)
{
	struct ieee80211req ireq;
	static struct ieee80211req_chaninfo chans;
	static struct ieee80211_channel undef;
	const struct ieee80211_channel *c;
	int i, freq;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_CHANINFO;
	ireq.i_data = &chans;
	ireq.i_len = sizeof(chans);
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		errx(1, "unable to get channel information");
	freq = ieee80211_ieee2mhz(chan);
	for (i = 0; i < chans.ic_nchans; i++) {
		c = &chans.ic_chans[i];
		if (c->ic_freq == freq)
			return c;
	}
	return &undef;
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
int	col;
char	spacer;

#define	LINE_BREAK() do {			\
	if (spacer != '\t') {			\
		printf("\n");			\
		spacer = '\t';			\
	}					\
	col = 8;	/* 8-col tab */		\
} while (0)
#define	LINE_CHECK(fmt, ...) do {		\
	col += sizeof(fmt)-2;			\
	if (col > MAXCOL) {			\
		LINE_BREAK();			\
		col += sizeof(fmt)-2;		\
	}					\
	printf(fmt, __VA_ARGS__);		\
	spacer = ' ';				\
} while (0)

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
		LINE_CHECK("%cwepkey %u:%s", spacer, ik->ik_keyix+1,
		    keylen <= 5 ? "40-bit" :
		    keylen <= 13 ? "104-bit" : "128-bit");
		break;
	case IEEE80211_CIPHER_TKIP:
		if (keylen > 128/8)
			keylen -= 128/8;	/* ignore MIC for now */
		LINE_CHECK("%cTKIP %u:%u-bit",
			spacer, ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_AES_OCB:
		LINE_CHECK("%cAES-OCB %u:%u-bit",
			spacer, ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_AES_CCM:
		LINE_CHECK("%cAES-CCM %u:%u-bit",
			spacer, ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_CKIP:
		LINE_CHECK("%cCKIP %u:%u-bit",
			spacer, ik->ik_keyix+1, 8*keylen);
		break;
	case IEEE80211_CIPHER_NONE:
		LINE_CHECK("%cNULL %u:%u-bit",
			spacer, ik->ik_keyix+1, 8*keylen);
		break;
	default:
		LINE_CHECK("%cUNKNOWN (0x%x) %u:%u-bit", spacer,
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
	int i, num, wpa, wme;
	struct ieee80211req ireq;
	u_int8_t data[32];
	const struct ieee80211_channel *c;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_data = &data;

	wpa = 0;		/* unknown/not set */

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

	ireq.i_type = IEEE80211_IOC_CHANNEL;
	if (ioctl(s, SIOCG80211, &ireq) < 0)
		goto end;
	c = getchaninfo(s, ireq.i_val);
	if (ireq.i_val != -1) {
		printf(" channel %d", ireq.i_val);
		if (verbose)
			printf(" (%u)", c->ic_freq);
	} else if (verbose)
		printf(" channel UNDEF");

	ireq.i_type = IEEE80211_IOC_BSSID;
	ireq.i_len = IEEE80211_ADDR_LEN;
	if (ioctl(s, SIOCG80211, &ireq) >= 0 &&
	    memcmp(ireq.i_data, zerobssid, sizeof(zerobssid)) != 0)
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
				LINE_CHECK("%cauthmode NONE", spacer);
				break;
			case IEEE80211_AUTH_OPEN:
				LINE_CHECK("%cauthmode OPEN", spacer);
				break;
			case IEEE80211_AUTH_SHARED:
				LINE_CHECK("%cauthmode SHARED", spacer);
				break;
			case IEEE80211_AUTH_8021X:
				LINE_CHECK("%cauthmode 802.1x", spacer);
				break;
			case IEEE80211_AUTH_WPA:
				ireq.i_type = IEEE80211_IOC_WPA;
				if (ioctl(s, SIOCG80211, &ireq) != -1)
					wpa = ireq.i_val;
				if (!wpa)
					wpa = 1;	/* default to WPA1 */
				switch (wpa) {
				case 2:
					LINE_CHECK("%cauthmode WPA2/802.11i",
						spacer);
					break;
				case 3:
					LINE_CHECK("%cauthmode WPA1+WPA2/802.11i",
						spacer);
					break;
				default:
					LINE_CHECK("%cauthmode WPA", spacer);
					break;
				}
				break;
			case IEEE80211_AUTH_AUTO:
				LINE_CHECK("%cauthmode AUTO", spacer);
				break;
			default:
				LINE_CHECK("%cauthmode UNKNOWN (0x%x)",
					spacer, ireq.i_val);
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
				LINE_CHECK("%cprivacy OFF", spacer);
				break;
			case IEEE80211_WEP_ON:
				LINE_CHECK("%cprivacy ON", spacer);
				break;
			case IEEE80211_WEP_MIXED:
				LINE_CHECK("%cprivacy MIXED", spacer);
				break;
			default:
				LINE_CHECK("%cprivacy UNKNOWN (0x%x)",
					spacer, wepmode);
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
			LINE_CHECK("%cdeftxkey %d", spacer, ireq.i_val+1);
		else if (wepmode != IEEE80211_WEP_OFF || verbose)
			LINE_CHECK("%cdeftxkey UNDEF", spacer);

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
	}

	ireq.i_type = IEEE80211_IOC_POWERSAVE;
	if (ioctl(s, SIOCG80211, &ireq) != -1 &&
	    ireq.i_val != IEEE80211_POWERSAVE_NOSUP ) {
		if (ireq.i_val != IEEE80211_POWERSAVE_OFF || verbose) {
			switch (ireq.i_val) {
				case IEEE80211_POWERSAVE_OFF:
					LINE_CHECK("%cpowersavemode OFF",
						spacer);
					break;
				case IEEE80211_POWERSAVE_CAM:
					LINE_CHECK("%cpowersavemode CAM",
						spacer);
					break;
				case IEEE80211_POWERSAVE_PSP:
					LINE_CHECK("%cpowersavemode PSP",
						spacer);
					break;
				case IEEE80211_POWERSAVE_PSP_CAM:
					LINE_CHECK("%cpowersavemode PSP-CAM",
						spacer);
					break;
			}
			ireq.i_type = IEEE80211_IOC_POWERSAVESLEEP;
			if (ioctl(s, SIOCG80211, &ireq) != -1)
				LINE_CHECK("%cpowersavesleep %d",
					spacer, ireq.i_val);
		}
	}

	ireq.i_type = IEEE80211_IOC_TXPOWMAX;
	if (ioctl(s, SIOCG80211, &ireq) != -1)
		LINE_CHECK("%ctxpowmax %d", spacer, ireq.i_val);

	if (verbose) {
		ireq.i_type = IEEE80211_IOC_TXPOWER;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			LINE_CHECK("%ctxpower %d", spacer, ireq.i_val);
	}

	ireq.i_type = IEEE80211_IOC_RTSTHRESHOLD;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val != IEEE80211_RTS_MAX || verbose)
			LINE_CHECK("%crtsthreshold %d", spacer, ireq.i_val);
	}

	ireq.i_type = IEEE80211_IOC_FRAGTHRESHOLD;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val != IEEE80211_FRAG_MAX || verbose)
			LINE_CHECK("%cfragthreshold %d", spacer, ireq.i_val);
	}

	if (IEEE80211_IS_CHAN_G(c) || IEEE80211_IS_CHAN_PUREG(c) || verbose) {
		ireq.i_type = IEEE80211_IOC_PUREG;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				LINE_CHECK("%cpureg", spacer);
			else if (verbose)
				LINE_CHECK("%c-pureg", spacer);
		}
		ireq.i_type = IEEE80211_IOC_PROTMODE;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			switch (ireq.i_val) {
				case IEEE80211_PROTMODE_OFF:
					LINE_CHECK("%cprotmode OFF", spacer);
					break;
				case IEEE80211_PROTMODE_CTS:
					LINE_CHECK("%cprotmode CTS", spacer);
					break;
				case IEEE80211_PROTMODE_RTSCTS:
					LINE_CHECK("%cprotmode RTSCTS", spacer);
					break;
				default:
					LINE_CHECK("%cprotmode UNKNOWN (0x%x)",
						spacer, ireq.i_val);
					break;
			}
		}
	}

	ireq.i_type = IEEE80211_IOC_WME;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		wme = ireq.i_val;
		if (wme)
			LINE_CHECK("%cwme", spacer);
		else if (verbose)
			LINE_CHECK("%c-wme", spacer);
	} else
		wme = 0;

	if (opmode == IEEE80211_M_HOSTAP) {
		ireq.i_type = IEEE80211_IOC_HIDESSID;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				LINE_CHECK("%cssid HIDE", spacer);
			else if (verbose)
				LINE_CHECK("%cssid SHOW", spacer);
		}

		ireq.i_type = IEEE80211_IOC_APBRIDGE;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (!ireq.i_val)
				LINE_CHECK("%c-apbridge", spacer);
			else if (verbose)
				LINE_CHECK("%capbridge", spacer);
		}

		ireq.i_type = IEEE80211_IOC_DTIM_PERIOD;
		if (ioctl(s, SIOCG80211, &ireq) != -1)
			LINE_CHECK("%cdtimperiod %u", spacer, ireq.i_val);
	} else {
		ireq.i_type = IEEE80211_IOC_ROAMING;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val != IEEE80211_ROAMING_AUTO || verbose) {
				switch (ireq.i_val) {
				case IEEE80211_ROAMING_DEVICE:
					LINE_CHECK("%croaming DEVICE", spacer);
					break;
				case IEEE80211_ROAMING_AUTO:
					LINE_CHECK("%croaming AUTO", spacer);
					break;
				case IEEE80211_ROAMING_MANUAL:
					LINE_CHECK("%croaming MANUAL", spacer);
					break;
				default:
					LINE_CHECK("%croaming UNKNOWN (0x%x)",
						spacer, ireq.i_val);
					break;
				}
			}
		}
	}
	ireq.i_type = IEEE80211_IOC_BEACON_INTERVAL;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		if (ireq.i_val)
			LINE_CHECK("%cbintval %u", spacer, ireq.i_val);
		else if (verbose)
			LINE_CHECK("%cbintval %u", spacer, ireq.i_val);
	}

	if (wme && verbose) {
		LINE_BREAK();
		list_wme(s);
	}

	if (wpa) {
		ireq.i_type = IEEE80211_IOC_COUNTERMEASURES;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				LINE_CHECK("%ccountermeasures", spacer);
			else if (verbose)
				LINE_CHECK("%c-countermeasures", spacer);
		}
#if 0
		/* XXX not interesting with WPA done in user space */
		ireq.i_type = IEEE80211_IOC_KEYMGTALGS;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
		}

		ireq.i_type = IEEE80211_IOC_MCASTCIPHER;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			printf("%cmcastcipher ", spacer);
			printcipher(s, &ireq, IEEE80211_IOC_MCASTKEYLEN);
			spacer = ' ';
		}

		ireq.i_type = IEEE80211_IOC_UCASTCIPHER;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			printf("%cucastcipher ", spacer);
			printcipher(s, &ireq, IEEE80211_IOC_UCASTKEYLEN);
		}

		if (wpa & 2) {
			ireq.i_type = IEEE80211_IOC_RSNCAPS;
			if (ioctl(s, SIOCG80211, &ireq) != -1) {
				printf("%cRSN caps 0x%x", spacer, ireq.i_val);
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
set80211(int s, int type, int val, int len, u_int8_t *data)
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
	if (!hexstr && len == 1 && buf[0] == '-')
		len = 0;
	if (len < *lenp)
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
	DEF_CMD_ARG("fragthreshold",	set80211fragthreshold),
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
