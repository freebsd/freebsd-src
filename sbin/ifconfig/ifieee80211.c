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
#include <net/route.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ifconfig.h"

static void set80211(int s, int type, int val, int len, u_int8_t *data);
static const char *get_string(const char *val, const char *sep,
    u_int8_t *buf, int *lenp);
static void print_string(const u_int8_t *buf, int len);

void
set80211ssid(const char *val, int d, int s, const struct afswtch *rafp)
{
	int		ssid;
	int		len;
	u_int8_t	data[33];

	ssid = 0;
	len = strlen(val);
	if (len > 2 && isdigit(val[0]) && val[1] == ':') {
		ssid = atoi(val)-1;
		val += 2;
	}

	bzero(data, sizeof(data));
	len = sizeof(data);
	get_string(val, NULL, data, &len);

	set80211(s, IEEE80211_IOC_SSID, ssid, len, data);
}

void
set80211stationname(const char *val, int d, int s, const struct afswtch *rafp)
{
	int			len;
	u_int8_t		data[33];

	bzero(data, sizeof(data));
	len = sizeof(data);
	get_string(val, NULL, data, &len);

	set80211(s, IEEE80211_IOC_STATIONNAME, 0, len, data);
}

void
set80211channel(const char *val, int d, int s, const struct afswtch *rafp)
{
	if (strcmp(val, "-") == 0)
		set80211(s, IEEE80211_IOC_CHANNEL, IEEE80211_CHAN_ANY, 0, NULL);
	else
		set80211(s, IEEE80211_IOC_CHANNEL, atoi(val), 0, NULL);
}

void
set80211authmode(const char *val, int d, int s, const struct afswtch *rafp)
{
	int	mode;

	if (strcasecmp(val, "none") == 0) {
		mode = IEEE80211_AUTH_NONE;
	} else if (strcasecmp(val, "open") == 0) {
		mode = IEEE80211_AUTH_OPEN;
	} else if (strcasecmp(val, "shared") == 0) {
		mode = IEEE80211_AUTH_SHARED;
	} else {
		err(1, "unknown authmode");
	}

	set80211(s, IEEE80211_IOC_AUTHMODE, mode, 0, NULL);
}

void
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
		err(1, "unknown powersavemode");
	}

	set80211(s, IEEE80211_IOC_POWERSAVE, mode, 0, NULL);
}

void
set80211powersave(const char *val, int d, int s, const struct afswtch *rafp)
{
	if (d == 0)
		set80211(s, IEEE80211_IOC_POWERSAVE, IEEE80211_POWERSAVE_OFF,
		    0, NULL);
	else
		set80211(s, IEEE80211_IOC_POWERSAVE, IEEE80211_POWERSAVE_ON,
		    0, NULL);
}

void
set80211powersavesleep(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_POWERSAVESLEEP, atoi(val), 0, NULL);
}

void
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
		err(1, "unknown wep mode");
	}

	set80211(s, IEEE80211_IOC_WEP, mode, 0, NULL);
}

void
set80211wep(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_WEP, d, 0, NULL);
}

void
set80211weptxkey(const char *val, int d, int s, const struct afswtch *rafp)
{
	set80211(s, IEEE80211_IOC_WEPTXKEY, atoi(val)-1, 0, NULL);
}

void
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
 * This function is purly a NetBSD compatability interface.  The NetBSD
 * iterface is too inflexable, but it's there so we'll support it since
 * it's not all that hard.
 */
void
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

void
ieee80211_status (int s, struct rt_addrinfo *info __unused)
{
	int			i;
	int			num;
	struct ieee80211req	ireq;
	u_int8_t		data[32];
	char			spacer;

	(void) memset(&ireq, 0, sizeof(ireq));
	(void) strncpy(ireq.i_name, name, sizeof(ireq.i_name));
	ireq.i_data = &data;

	ireq.i_type = IEEE80211_IOC_SSID;
	ireq.i_val = -1;
	if (ioctl(s, SIOCG80211, &ireq) < 0) {
		/* If we can't get the SSID, the this isn't an 802.11 device. */
		return;
	}
	printf("\tssid ");
	print_string(data, ireq.i_len);
	num = 0;
	ireq.i_type = IEEE80211_IOC_NUMSSIDS;
	if (ioctl(s, SIOCG80211, &ireq) >= 0) {
		num = ireq.i_val;
	}
	ireq.i_type = IEEE80211_IOC_SSID;
	for (ireq.i_val = 0; ireq.i_val < num; ireq.i_val++) {
		if (ioctl(s, SIOCG80211, &ireq) >= 0 && ireq.i_len > 0) {
			printf(" %d:", ireq.i_val + 1);
			print_string(data, ireq.i_len);
		}
	}
	printf("\n");

	ireq.i_type = IEEE80211_IOC_STATIONNAME;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		printf("\tstationname ");
		print_string(data, ireq.i_len);
		printf("\n");
	}

	ireq.i_type = IEEE80211_IOC_CHANNEL;
	if (ioctl(s, SIOCG80211, &ireq) < 0) {
		goto end;
	}
	printf("\tchannel %d", ireq.i_val);

	ireq.i_type = IEEE80211_IOC_AUTHMODE;
	if (ioctl(s, SIOCG80211, &ireq) != -1) {
		printf(" authmode");
		switch (ireq.i_val) {
			case IEEE80211_AUTH_NONE:
				printf(" NONE");
				break;
			case IEEE80211_AUTH_OPEN:
				printf(" OPEN");
				break;
			case IEEE80211_AUTH_SHARED:
				printf(" SHARED");
				break;
			default:
				printf(" UNKNOWN");
				break;
		}
	}

	ireq.i_type = IEEE80211_IOC_POWERSAVE;
	if (ioctl(s, SIOCG80211, &ireq) != -1 &&
	    ireq.i_val != IEEE80211_POWERSAVE_NOSUP ) {
		printf(" powersavemode");
		switch (ireq.i_val) {
			case IEEE80211_POWERSAVE_OFF:
				printf(" OFF");
				break;
			case IEEE80211_POWERSAVE_CAM:
				printf(" CAM");
				break;
			case IEEE80211_POWERSAVE_PSP:
				printf(" PSP");
				break;
			case IEEE80211_POWERSAVE_PSP_CAM:
				printf(" PSP-CAM");
				break;
		}

		ireq.i_type = IEEE80211_IOC_POWERSAVESLEEP;
		if (ioctl(s, SIOCG80211, &ireq) != -1) {
			if (ireq.i_val)
				printf(" powersavesleep %d", ireq.i_val);
		}
	}

	printf("\n");

	ireq.i_type = IEEE80211_IOC_WEP;
	if (ioctl(s, SIOCG80211, &ireq) != -1 &&
	    ireq.i_val != IEEE80211_WEP_NOSUP) {
		printf("\twepmode");
		switch (ireq.i_val) {
			case IEEE80211_WEP_OFF:
				printf(" OFF");
				break;
			case IEEE80211_WEP_ON:
				printf(" ON");
				break;
			case IEEE80211_WEP_MIXED:
				printf(" MIXED");
				break;
			default:
				printf(" UNKNOWN");
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
		printf(" weptxkey %d", ireq.i_val+1);

		ireq.i_type = IEEE80211_IOC_NUMWEPKEYS;
		if (ioctl(s, SIOCG80211, &ireq) < 0) {
			warn("WEP support, but no NUMWEPKEYS support!");
			goto end;
		}
		num = ireq.i_val;

		printf("\n");

		ireq.i_type = IEEE80211_IOC_WEPKEY;
		spacer = '\t';
		for (i = 0; i < num; i++) {
			ireq.i_val = i;
			if (ioctl(s, SIOCG80211, &ireq) < 0) {
				warn("WEP support, but can get keys!");
				goto end;
			}
			if (ireq.i_len == 0 ||
			    ireq.i_len > IEEE80211_KEYBUF_SIZE)
				continue;
			printf("%cwepkey %d:%s", spacer, i+1,
			    ireq.i_len <= 5 ? "40-bit" :
			    ireq.i_len <= 13 ? "104-bit" : "128-bit");
			if (spacer == '\t')
				spacer = ' ';
		}
		if (spacer == ' ')
			printf("\n");
	}

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
			if (!isxdigit((u_char)val[0]) ||
			    !isxdigit((u_char)val[1])) {
				warnx("bad hexadecimal digits");
				return NULL;
			}
		}
		if (p > buf + len) {
			if (hexstr)
				warnx("hexadecimal digits too long");
			else
				warnx("strings too long");
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

