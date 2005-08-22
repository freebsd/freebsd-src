/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if 0
#ifndef lint
static const char copyright[] = "@(#) Copyright (c) 1997, 1998, 1999\
	Bill Paul. All rights reserved.";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/if_wireg.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>

static int wi_getval(const char *, struct wi_req *);
static int wi_getvalmaybe(const char *, struct wi_req *);
static void wi_setval(const char *, struct wi_req *);
static void wi_printstr(struct wi_req *);
static void wi_setstr(const char *, int, char *);
static void wi_setbytes(const char *, int, char *, int);
static void wi_setword(const char *, int, int);
static void wi_sethex(const char *, int, char *);
static void wi_printwords(struct wi_req *);
static void wi_printbool(struct wi_req *);
static void wi_printhex(struct wi_req *);
static void wi_printaps(struct wi_req *);
static void wi_dumpinfo(const char *);
static void wi_dumpstats(const char *);
static void wi_setkeys(const char *, char *, int);
static void wi_printkeys(struct wi_req *);
static void wi_printaplist(const char *);
static int wi_hex2int(char);
static void wi_str2key(char *, struct wi_key *);
#ifdef WICACHE
static void wi_zerocache(const char *);
static void wi_readcache(const char *);
#endif
static void usage(const char *);
static int listaps;
static int quiet;

/*
 * Print a value a la the %b format of the kernel's printf
 * (ripped screaming from ifconfig/ifconfig.c)
 */
void
printb(char *s, uint32_t v, char *bits)
{
	int i, any = 0;
	char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++)) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}

static int
_wi_getval(const char *iface, struct wi_req *wreq)
{
	struct ifreq		ifr;
	int			s;
	int			retval;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		err(1, "socket");
	retval = ioctl(s, SIOCGWAVELAN, &ifr);
	close(s);

	return (retval);
}

static int
wi_getval(const char *iface, struct wi_req *wreq)
{
	if (_wi_getval(iface, wreq) == -1) {
		if (errno != EINPROGRESS)
			err(1, "SIOCGWAVELAN");
		return (-1);
	}
	return (0);
}

static int
wi_getvalmaybe(const char *iface, struct wi_req *wreq)
{
	if (_wi_getval(iface, wreq) == -1) {
		if (errno != EINPROGRESS && errno != EINVAL)
			err(1, "SIOCGWAVELAN");
		return (-1);
	}
	return (0);
}

static void
wi_setval(const char *iface, struct wi_req *wreq)
{
	struct ifreq		ifr;
	int			s;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s == -1)
		err(1, "socket");

	if (ioctl(s, SIOCSWAVELAN, &ifr) == -1)
		err(1, "SIOCSWAVELAN");

	close(s);

	return;
}

void
wi_printstr(struct wi_req *wreq)
{
	char			*ptr;
	int			i;

	if (wreq->wi_type == WI_RID_SERIALNO) {
		ptr = (char *)&wreq->wi_val;
		for (i = 0; i < (wreq->wi_len - 1) * 2; i++) {
			if (ptr[i] == '\0')
				ptr[i] = ' ';
		}
	} else {
		ptr = (char *)&wreq->wi_val[1];
		for (i = 0; i < wreq->wi_val[0]; i++) {
			if (ptr[i] == '\0')
				ptr[i] = ' ';
		}
	}

	ptr[i] = '\0';
	printf("[ %s ]", ptr);

	return;
}

void
wi_setstr(const char *iface, int code, char *str)
{
	struct wi_req		wreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

	if (str == NULL)
		errx(1, "must specify string");

	bzero((char *)&wreq, sizeof(wreq));

	if (strlen(str) > 30)
		errx(1, "string too long");

	wreq.wi_type = code;
	wreq.wi_len = 18;
	wreq.wi_val[0] = strlen(str);
	bcopy(str, (char *)&wreq.wi_val[1], strlen(str));

	wi_setval(iface, &wreq);

	return;
}

void
wi_setbytes(const char *iface, int code, char *bytes, int len)
{
	struct wi_req		wreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_type = code;
	wreq.wi_len = (len / 2) + 1;
	bcopy(bytes, (char *)&wreq.wi_val[0], len);

	wi_setval(iface, &wreq);

	return;
}

void
wi_setword(const char *iface, int code, int word)
{
	struct wi_req		wreq;

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_type = code;
	wreq.wi_len = 2;
	wreq.wi_val[0] = word;

	wi_setval(iface, &wreq);

	return;
}

void
wi_sethex(const char *iface, int code, char *str)
{
	struct ether_addr	*addr;

	if (str == NULL)
		errx(1, "must specify address");

	addr = ether_aton(str);

	if (addr == NULL)
		errx(1, "badly formatted address");

	wi_setbytes(iface, code, (char *)addr, ETHER_ADDR_LEN);

	return;
}

static int
wi_hex2int(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);

	return (0); 
}

static void
wi_str2key(char *s, struct wi_key *k)
{
	int			n, i;
	char			*p;

	/* Is this a hex string? */
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		/* Yes, convert to int. */
		n = 0;
		p = (char *)&k->wi_keydat[0];
		for (i = 2; s[i] != '\0' && s[i + 1] != '\0'; i+= 2) {
			*p++ = (wi_hex2int(s[i]) << 4) + wi_hex2int(s[i + 1]);
			n++;
		}
		if (s[i] != '\0')
			errx(1, "hex strings must be of even length");
		k->wi_keylen = n;
	} else {
		/* No, just copy it in. */
		bcopy(s, k->wi_keydat, strlen(s));
		k->wi_keylen = strlen(s);
	}

	return;
}

static void
wi_setkeys(const char *iface, char *key, int idx)
{
	int			keylen;
	struct wi_req		wreq;
	struct wi_ltv_keys	*keys;
	struct wi_key		*k;
	int			has_wep;

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_WEP_AVAIL;

	if (wi_getval(iface, &wreq) == 0)
		has_wep = wreq.wi_val[0];
	else
		has_wep = 0;
	if (!has_wep)
		errx(1, "no WEP option available on this card");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;

	if (wi_getval(iface, &wreq) == -1)
		errx(1, "Cannot get default key index");
	keys = (struct wi_ltv_keys *)&wreq;

	keylen = strlen(key);
	if (key[0] == '0' && (key[1] == 'x' || key[1] == 'X')) {
		if (keylen != 2 && keylen != 12 && keylen != 28) {
			errx(1, "encryption key must be 0, 10, or 26 "
			    "hex digits long");
		}
	} else {
		if (keylen != 0 && keylen != 5 && keylen != 13) {
			errx(1, "encryption key must be 0, 5, or 13 "
			    "bytes long");
		}
	}

	if (idx > 3)
		errx(1, "only 4 encryption keys available");

	k = &keys->wi_keys[idx];
	wi_str2key(key, k);

	wreq.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
	wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
	wi_setval(iface, &wreq);

	return;
}

static void
wi_printkeys(struct wi_req *wreq)
{
	int			i, j;
	int			isprintable;
	struct wi_key		*k;
	struct wi_ltv_keys	*keys;
	char			*ptr;

	keys = (struct wi_ltv_keys *)wreq;

	for (i = 0; i < 4; i++) {
		k = &keys->wi_keys[i];
		ptr = (char *)k->wi_keydat;
		isprintable = 1;
		for (j = 0; j < k->wi_keylen; j++) {
			if (!isprint(ptr[j])) {
				isprintable = 0;
				break;
			}
		}
		if (isprintable) {
			ptr[j] = '\0';
			printf("[ %s ]", ptr);
		} else {
			printf("[ 0x");
			for (j = 0; j < k->wi_keylen; j++) {
				printf("%02x", ptr[j] & 0xFF);
			}
			printf(" ]");
					
		}
	}

	return;
}

void
wi_printwords(struct wi_req *wreq)
{
	int			i;

	printf("[ ");
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%d ", wreq->wi_val[i]);
	printf("]");

	return;
}

void
wi_printswords(struct wi_req *wreq)
{
	int			i;

	printf("[ ");
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%d ", ((int16_t *) wreq->wi_val)[i]);
	printf("]");

	return;
}

void
wi_printhexwords(struct wi_req *wreq)
{
	int			i;

	printf("[ ");
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%x ", wreq->wi_val[i]);
	printf("]");

	return;
}

void
wi_printregdoms(struct wi_req *wreq)
{
	int			i;
	struct wi_ltv_domains	*regdom = (struct wi_ltv_domains *)wreq;

	printf("[ ");
	for (i = 0; i < regdom->wi_num_dom; i++) {
		switch (regdom->wi_domains[i]) {
		case 0x10: printf("usa"); break;
		case 0x20: printf("canada"); break;
		case 0x30: printf("eu/au"); break;
		case 0x31: printf("es"); break;
		case 0x32: printf("fr"); break;
		case 0x40: printf("jp"); break;
		case 0x41: printf("jp new"); break;
		default: printf("0x%x", regdom->wi_domains[i]); break;
		}
		if (i < regdom->wi_num_dom - 1)
			printf(", ");
	}
	printf(" ]");

	return;
}

void
wi_printbool(struct wi_req *wreq)
{
	if (wreq->wi_val[0])
		printf("[ On ]");
	else
		printf("[ Off ]");

	return;
}

void
wi_printhex(struct wi_req *wreq)
{
	int			i;
	unsigned char		*c;

	c = (unsigned char *)&wreq->wi_val;

	printf("[ ");
	for (i = 0; i < (wreq->wi_len - 1) * 2; i++) {
		printf("%02x", c[i]);
		if (i < ((wreq->wi_len - 1) * 2) - 1)
			printf(":");
	}

	printf(" ]");
	return;
}

static float
get_wiaprate(int inrate)
{
	float rate;

	switch (inrate) {
	case WI_APRATE_1:
		rate = 1;
		break;
	case WI_APRATE_2:
		rate = 2;
		break;
	case WI_APRATE_5:
		rate = 5.5;
		break;
	case WI_APRATE_11:
		rate = 11;
		break;
#ifdef WI_APRATE_0
	case WI_APRATE_0:
#endif
	default:
		rate = 0;
		break;
	}

	return (rate);
}

void
wi_printaplist(const char *iface)
{
	int			prism2, len, i = 0, j, r;
	struct wi_req		wreq;
	struct wi_scan_p2_hdr	*wi_p2_h;
	struct wi_scan_res	*res;
	float			rate;

	if (!quiet)
		printf("Available APs:\n");

	/* first determine if this is a prism2 card or not */
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_PRISM2;

	if (wi_getval(iface, &wreq) == 0)
		prism2 = wreq.wi_val[0];
	else
		prism2 = 0;

	/* send out a scan request */
	wreq.wi_len = prism2 ? 3 : 1;
	wreq.wi_type = WI_RID_SCAN_REQ;

	if (prism2) {
		wreq.wi_val[0] = 0x3FFF;
		wreq.wi_val[1] = 0x000F;
	}

	wi_setval(iface, &wreq);

	do {
		/*
		 * sleep for 100 milliseconds so there's enough time for the card to
		 * respond... prism2's take a little longer.
		 */
		usleep(prism2 ? 500000 : 100000);

		/* get the scan results */
		wreq.wi_len = WI_MAX_DATALEN;
		wreq.wi_type = WI_RID_SCAN_RES;
	} while (wi_getval(iface, &wreq) == -1 && errno == EINPROGRESS);

	if (prism2) {
		wi_p2_h = (struct wi_scan_p2_hdr *)wreq.wi_val;

		/* if the reason is 0, this info is invalid */
		if (wi_p2_h->wi_reason == 0)
			return;

		i = 4;
	}

	len = prism2 ? WI_PRISM2_RES_SIZE : WI_WAVELAN_RES_SIZE;

	if (!quiet) {
		int nstations = ((wreq.wi_len * 2) - i) / len;
		printf("%d station%s:\n", nstations, nstations == 1 ? "" : "s");
		printf("%-16.16s            BSSID         Chan     SN  S  N  Intrvl Capinfo\n", "SSID");
	}
	for (; i < (wreq.wi_len * 2) - len; i += len) {
		res = (struct wi_scan_res *)((char *)wreq.wi_val + i);

		res->wi_ssid[res->wi_ssid_len] = '\0';

		printf("%-16.16s  [ %02x:%02x:%02x:%02x:%02x:%02x ]  [ %-2d ]  "
		    "[ %2d %2d %2d ]  %3d  ", res->wi_ssid,
		    res->wi_bssid[0], res->wi_bssid[1], res->wi_bssid[2],
		    res->wi_bssid[3], res->wi_bssid[4], res->wi_bssid[5],
		    res->wi_chan, res->wi_signal - res->wi_noise,
		    res->wi_signal, res->wi_noise, res->wi_interval);

		if (!quiet && res->wi_capinfo) {
			printf("[ ");
			if (res->wi_capinfo & WI_CAPINFO_ESS)
				printf("ess ");
			if (res->wi_capinfo & WI_CAPINFO_IBSS)
				printf("ibss ");
			if (res->wi_capinfo & IEEE80211_CAPINFO_CF_POLLABLE)
				printf("cfp  ");
			if (res->wi_capinfo & IEEE80211_CAPINFO_CF_POLLREQ)
				printf("cfpr ");
			if (res->wi_capinfo & WI_CAPINFO_PRIV)
				printf("priv ");
			if (res->wi_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE)
				printf("shpr ");
			if (res->wi_capinfo & IEEE80211_CAPINFO_PBCC)
				printf("pbcc ");
			if (res->wi_capinfo & IEEE80211_CAPINFO_CHNL_AGILITY)
				printf("chna ");
			if (res->wi_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)
				printf("shst ");
			if (res->wi_capinfo & IEEE80211_CAPINFO_DSSSOFDM)
				printf("ofdm ");
			printf("]  ");
		}

		if (prism2 && res->wi_srates[0] != 0) {
			printf("\n%16s  [ ", "");
			for (j = 0; j < 10 && res->wi_srates[j] != 0; j++) {
				r = res->wi_srates[j] & IEEE80211_RATE_VAL;
				if (r & 1)
					printf("%d.%d", r / 2, (r % 2) * 5);
				else
					printf("%d", r / 2);
				printf("%s ", res->wi_srates[j] & IEEE80211_RATE_BASIC ? "b" : "");
			}
			printf("]  ");
			rate = get_wiaprate(res->wi_rate);
			if (rate)
				printf("* %2.1f *\n", rate);
		}
		putchar('\n');
	}
}

#define WI_STRING		0x01
#define WI_BOOL			0x02
#define WI_WORDS		0x03
#define WI_HEXBYTES		0x04
#define WI_KEYSTRUCT		0x05
#define WI_SWORDS		0x06
#define WI_HEXWORDS		0x07
#define WI_REGDOMS		0x08

struct wi_table {
	int			wi_code;
	int			wi_type;
	const char		*wi_str;
};

static struct wi_table wi_table[] = {
	{ WI_RID_SERIALNO, WI_STRING, "NIC serial number:\t\t\t" },
	{ WI_RID_NODENAME, WI_STRING, "Station name:\t\t\t\t" },
	{ WI_RID_OWN_SSID, WI_STRING, "SSID for IBSS creation:\t\t\t" },
	{ WI_RID_CURRENT_SSID, WI_STRING, "Current netname (SSID):\t\t\t" },
	{ WI_RID_DESIRED_SSID, WI_STRING, "Desired netname (SSID):\t\t\t" },
	{ WI_RID_CURRENT_BSSID, WI_HEXBYTES, "Current BSSID:\t\t\t\t" },
	{ WI_RID_CHANNEL_LIST, WI_HEXWORDS, "Channel list:\t\t\t\t" },
	{ WI_RID_OWN_CHNL, WI_WORDS, "IBSS channel:\t\t\t\t" },
	{ WI_RID_CURRENT_CHAN, WI_WORDS, "Current channel:\t\t\t" },
	{ WI_RID_COMMS_QUALITY, WI_WORDS, "Comms quality/signal/noise:\t\t" },
	{ WI_RID_DBM_COMMS_QUAL, WI_SWORDS, "dBm Coms Quality:\t\t\t" },
	{ WI_RID_PROMISC, WI_BOOL, "Promiscuous mode:\t\t\t" },
	{ WI_RID_PROCFRAME, WI_BOOL, "Process 802.11b Frame:\t\t\t" },
	{ WI_RID_PRISM2, WI_WORDS, "Intersil-Prism2 based card:\t\t" },
	{ WI_RID_PORTTYPE, WI_WORDS, "Port type (1=BSS, 3=ad-hoc):\t\t"},
	{ WI_RID_MAC_NODE, WI_HEXBYTES, "MAC address:\t\t\t\t"},
	{ WI_RID_TX_RATE, WI_WORDS, "TX rate (selection):\t\t\t"},
	{ WI_RID_CUR_TX_RATE, WI_WORDS, "TX rate (actual speed):\t\t\t"},
	{ WI_RID_RTS_THRESH, WI_WORDS, "RTS/CTS handshake threshold:\t\t"},
	{ WI_RID_CREATE_IBSS, WI_BOOL, "Create IBSS:\t\t\t\t" },
	{ WI_RID_SYSTEM_SCALE, WI_WORDS, "Access point density:\t\t\t" },
	{ WI_RID_PM_ENABLED, WI_WORDS, "Power Mgmt (1=on, 0=off):\t\t" },
	{ WI_RID_MAX_SLEEP, WI_WORDS, "Max sleep time:\t\t\t\t" },
	{ WI_RID_PRI_IDENTITY, WI_WORDS, "PRI Identity:\t\t\t\t" },
	{ WI_RID_STA_IDENTITY, WI_WORDS, "STA Identity:\t\t\t\t" } ,
	{ WI_RID_CARD_ID, WI_HEXWORDS, "Card ID register:\t\t\t" },
	{ WI_RID_REG_DOMAINS, WI_REGDOMS, "Regulatory Domains:\t\t\t" },
	{ WI_RID_TEMP_TYPE, WI_WORDS, "Temperature Range:\t\t\t" },
#ifdef WI_EXTRA_INFO
	{ WI_RID_PRI_SUP_RANGE, WI_WORDS, "PRI Sup Range:\t\t\t\t" },
	{ WI_RID_CIF_ACT_RANGE, WI_WORDS, "CFI Act Sup Range:\t\t\t" },
	{ WI_RID_STA_SUP_RANGE, WI_WORDS, "STA Sup Range:\t\t\t\t" } ,
	{ WI_RID_MFI_ACT_RANGE, WI_WORDS, "MFI Act Sup Range:\t\t\t" } ,
#endif
	{ 0, 0, NULL }
};

static struct wi_table wi_crypt_table[] = {
	{ WI_RID_ENCRYPTION, WI_BOOL, "WEP encryption:\t\t\t\t" },
	{ WI_RID_TX_CRYPT_KEY, WI_WORDS, "TX encryption key:\t\t\t" },
	{ WI_RID_DEFLT_CRYPT_KEYS, WI_KEYSTRUCT, "Encryption keys:\t\t\t" },
	{ 0, 0, NULL }
};

static void
wi_dumpinfo(const char *iface)
{
	struct wi_req		wreq;
	int			i, has_wep;
	struct wi_table		*w;

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_WEP_AVAIL;

	if (wi_getval(iface, &wreq) == 0)
		has_wep = wreq.wi_val[0];
	else
		has_wep = 0;

	w = wi_table;

	for (i = 0; w[i].wi_type; i++) {
		bzero((char *)&wreq, sizeof(wreq));

		wreq.wi_len = WI_MAX_DATALEN;
		wreq.wi_type = w[i].wi_code;

		if (wi_getvalmaybe(iface, &wreq) == -1)
			continue;
		printf("%s", w[i].wi_str);
		switch(w[i].wi_type) {
		case WI_STRING:
			wi_printstr(&wreq);
			break;
		case WI_WORDS:
			wi_printwords(&wreq);
			break;
		case WI_SWORDS:
			wi_printswords(&wreq);
			break;
		case WI_HEXWORDS:
			wi_printhexwords(&wreq);
			break;
		case WI_REGDOMS:
			wi_printregdoms(&wreq);
			break;
		case WI_BOOL:
			wi_printbool(&wreq);
			break;
		case WI_HEXBYTES:
			wi_printhex(&wreq);
			break;
		default:
			break;
		}	
		printf("\n");
	}

	if (has_wep) {
		w = wi_crypt_table;
		for (i = 0; w[i].wi_type; i++) {
			bzero((char *)&wreq, sizeof(wreq));

			wreq.wi_len = WI_MAX_DATALEN;
			wreq.wi_type = w[i].wi_code;

			if (wi_getval(iface, &wreq) == -1)
				continue;
			printf("%s", w[i].wi_str);
			switch(w[i].wi_type) {
			case WI_STRING:
				wi_printstr(&wreq);
				break;
			case WI_WORDS:
				if (wreq.wi_type == WI_RID_TX_CRYPT_KEY)
					wreq.wi_val[0]++;
				wi_printwords(&wreq);
				break;
			case WI_BOOL:
				wi_printbool(&wreq);
				break;
			case WI_HEXBYTES:
				wi_printhex(&wreq);
				break;
			case WI_KEYSTRUCT:
				wi_printkeys(&wreq);
				break;
			default:
				break;
			}	
			printf("\n");
		}
	}

	if (listaps)
		wi_printaplist(iface);

	return;
}

static void
wi_dumpstats(const char *iface)
{
	struct wi_req		wreq;
	struct wi_counters	*c;

	if (iface == NULL)
		errx(1, "must specify interface name");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_IFACE_STATS;

	if (wi_getval(iface, &wreq) == -1)
		errx(1, "Cannot get interface stats");

	c = (struct wi_counters *)&wreq.wi_val;

	printf("Transmitted unicast frames:\t\t%d\n",
	    c->wi_tx_unicast_frames);
	printf("Transmitted multicast frames:\t\t%d\n",
	    c->wi_tx_multicast_frames);
	printf("Transmitted fragments:\t\t\t%d\n",
	    c->wi_tx_fragments);
	printf("Transmitted unicast octets:\t\t%d\n",
	    c->wi_tx_unicast_octets);
	printf("Transmitted multicast octets:\t\t%d\n",
	    c->wi_tx_multicast_octets);
	printf("Single transmit retries:\t\t%d\n",
	    c->wi_tx_single_retries);
	printf("Multiple transmit retries:\t\t%d\n",
	    c->wi_tx_multi_retries);
	printf("Transmit retry limit exceeded:\t\t%d\n",
	    c->wi_tx_retry_limit);
	printf("Transmit discards:\t\t\t%d\n",
	    c->wi_tx_discards);
	printf("Transmit discards due to wrong SA:\t%d\n",
	    c->wi_tx_discards_wrong_sa);
	printf("Received unicast frames:\t\t%d\n",
	    c->wi_rx_unicast_frames);
	printf("Received multicast frames:\t\t%d\n",
	    c->wi_rx_multicast_frames);
	printf("Received fragments:\t\t\t%d\n",
	    c->wi_rx_fragments);
	printf("Received unicast octets:\t\t%d\n",
	    c->wi_rx_unicast_octets);
	printf("Received multicast octets:\t\t%d\n",
	    c->wi_rx_multicast_octets);
	printf("Receive FCS errors:\t\t\t%d\n",
	    c->wi_rx_fcs_errors);
	printf("Receive discards due to no buffer:\t%d\n",
	    c->wi_rx_discards_nobuf);
	printf("Can't decrypt WEP frame:\t\t%d\n",
	    c->wi_rx_WEP_cant_decrypt);
	printf("Received message fragments:\t\t%d\n",
	    c->wi_rx_msg_in_msg_frags);
	printf("Received message bad fragments:\t\t%d\n",
	    c->wi_rx_msg_in_bad_msg_frags);

	return;
}

static void
usage(const char *p)
{
	fprintf(stderr, "usage:  %s -i iface\n", p);
	fprintf(stderr, "\t%s -i iface -o\n", p);
	fprintf(stderr, "\t%s -i iface -l\n", p);
	fprintf(stderr, "\t%s -i iface -L\n", p);
	fprintf(stderr, "\t%s -i iface -t tx rate\n", p);
	fprintf(stderr, "\t%s -i iface -n network name\n", p);
	fprintf(stderr, "\t%s -i iface -s station name\n", p);
	fprintf(stderr, "\t%s -i iface -c 0|1\n", p);
	fprintf(stderr, "\t%s -i iface -q SSID\n", p);
	fprintf(stderr, "\t%s -i iface -p port type\n", p);
	fprintf(stderr, "\t%s -i iface -a access point density\n", p);
	fprintf(stderr, "\t%s -i iface -m mac address\n", p);
	fprintf(stderr, "\t%s -i iface -d max data length\n", p);
	fprintf(stderr, "\t%s -i iface -e 0|1\n", p);
	fprintf(stderr, "\t%s -i iface -k encryption key [-v 1|2|3|4]\n", p);
	fprintf(stderr, "\t%s -i iface -r RTS threshold\n", p);
	fprintf(stderr, "\t%s -i iface -f frequency\n", p);
	fprintf(stderr, "\t%s -i iface -F 0|1\n", p);
	fprintf(stderr, "\t%s -i iface -P 0|1\n", p);
	fprintf(stderr, "\t%s -i iface -S max sleep duration\n", p);
	fprintf(stderr, "\t%s -i iface -T 1|2|3|4\n", p);
#ifdef WICACHE
	fprintf(stderr, "\t%s -i iface -Z zero out signal cache\n", p);
	fprintf(stderr, "\t%s -i iface -C print signal cache\n", p);
#endif

	exit(1);
}

static void
wi_printaps(struct wi_req *wreq)
{
	struct wi_apinfo	*w;
	int i, j, nstations, rate;

	nstations = *(int *)wreq->wi_val;
	printf("%d station%s:\n", nstations, nstations == 1 ? "" : "s");
	w =  (struct wi_apinfo *)(((char *)&wreq->wi_val) + sizeof(int));
	for ( i = 0; i < nstations; i++, w++) {
		printf("ap[%d]:\n", i);
		if (w->scanreason) {
			static char *scanm[] = {
				"Host initiated",
				"Firmware initiated",
				"Inquiry request from host"
			};
			printf("\tScanReason:\t\t\t[ %s ]\n",
				scanm[w->scanreason - 1]);
		}
		printf("\tnetname (SSID):\t\t\t[ ");
			for (j = 0; j < w->namelen; j++) {
				printf("%c", w->name[j]);
			}
			printf(" ]\n");
		printf("\tBSSID:\t\t\t\t[ %02x:%02x:%02x:%02x:%02x:%02x ]\n",
			w->bssid[0]&0xff, w->bssid[1]&0xff,
			w->bssid[2]&0xff, w->bssid[3]&0xff,
			w->bssid[4]&0xff, w->bssid[5]&0xff);
		printf("\tChannel:\t\t\t[ %d ]\n", w->channel);
		printf("\tQuality/Signal/Noise [signal]:\t[ %d / %d / %d ]\n"
		       "\t                        [dBm]:\t[ %d / %d / %d ]\n", 
			w->quality, w->signal, w->noise,
			w->quality, w->signal - 149, w->noise - 149);
		printf("\tBSS Beacon Interval [msec]:\t[ %d ]\n", w->interval); 
		printf("\tCapinfo:\t\t\t[ "); 
			if (w->capinfo & IEEE80211_CAPINFO_ESS)
				printf("ESS ");
			if (w->capinfo & IEEE80211_CAPINFO_PRIVACY)
				printf("WEP ");
			printf("]\n");

		switch (w->rate) {
		case WI_APRATE_1:
			rate = 1;
			break;
		case WI_APRATE_2:
			rate = 2;
			break;
		case WI_APRATE_5:
			rate = 5.5;
			break;
		case WI_APRATE_11:
			rate = 11;
			break;
#ifdef WI_APRATE_0
		case WI_APRATE_0:
#endif
		default:
			rate = 0;
			break;
		}
		if (rate) printf("\tDataRate [Mbps]:\t\t[ %d ]\n", rate);
	}
}

static void
wi_dumpstations(const char *iface)
{
	struct wi_req		wreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_READ_APS;

	if (wi_getval(iface, &wreq) == -1)
		errx(1, "Cannot get stations");
	wi_printaps(&wreq);
}

#ifdef WICACHE
static void
wi_zerocache(const char *iface)
{
	struct wi_req		wreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = 0;
	wreq.wi_type = WI_RID_ZERO_CACHE;
	wi_getval(iface, &wreq);
}

static void
wi_readcache(const char *iface)
{
	struct wi_req		wreq;
	int 			*wi_sigitems;
	struct wi_sigcache 	*sc;
	char *			pt;
	int 			i;

	if (iface == NULL)
		errx(1, "must specify interface name");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_READ_CACHE;
	if (wi_getval(iface, &wreq) == -1)
		errx(1, "Cannot read signal cache");

	wi_sigitems = (int *) &wreq.wi_val; 
	pt = ((char *) &wreq.wi_val);
	pt += sizeof(int);
	sc = (struct wi_sigcache *) pt;

	for (i = 0; i < *wi_sigitems; i++) {
		printf("[%d/%d]:", i+1, *wi_sigitems);
		printf(" %02x:%02x:%02x:%02x:%02x:%02x,",
		  		    	sc->macsrc[0]&0xff,
		  		    	sc->macsrc[1]&0xff,
		   		    	sc->macsrc[2]&0xff,
		   			sc->macsrc[3]&0xff,
		   			sc->macsrc[4]&0xff,
		   			sc->macsrc[5]&0xff);
        	printf(" %d.%d.%d.%d,",((sc->ipsrc >> 0) & 0xff),
				        ((sc->ipsrc >> 8) & 0xff),
				        ((sc->ipsrc >> 16) & 0xff),
				        ((sc->ipsrc >> 24) & 0xff));
		printf(" sig: %d, noise: %d, qual: %d\n",
		   			sc->signal,
		   			sc->noise,
		   			sc->quality);
		sc++;
	}

	return;
}
#endif

static void
dep(const char *flag, const char *opt)
{
	warnx("warning: flag %s deprecated, migrate to ifconfig %s", flag,
	    opt);
}

int
main(int argc, char *argv[])
{
	int			ch;
	const char		*iface = NULL;
	char			*p = argv[0];
	char			*key = NULL;
	int			modifier = 0;

	/* Get the interface name */
	opterr = 0;
	ch = getopt(argc, argv, "i:");
	if (ch == 'i') {
		iface = optarg;
	} else {
		if (argc > 1 && *argv[1] != '-') {
			iface = argv[1];
			optind = 2; 
		} else {
			iface = "wi0";
			optind = 1;
		}
		optreset = 1;
	}
	opterr = 1;
		
	while((ch = getopt(argc, argv,
	    "a:c:d:e:f:hi:k:lm:n:op:q:r:s:t:v:CF:LP:QS:T:Z")) != -1) {
		switch(ch) {
		case 'Z':
#ifdef WICACHE
			wi_zerocache(iface);
#else
			printf("WICACHE not available\n");
#endif
			exit(0);
			break;
		case 'C':
#ifdef WICACHE
			wi_readcache(iface);
#else
			printf("WICACHE not available\n");
#endif
			exit(0);
			break;
		case 'o':
			wi_dumpstats(iface);
			exit(0);
			break;
		case 'c':
			dep("c", "mediaopt");
			wi_setword(iface, WI_RID_CREATE_IBSS, atoi(optarg));
			exit(0);
			break;
		case 'd':
			wi_setword(iface, WI_RID_MAX_DATALEN, atoi(optarg));
			exit(0);
			break;
		case 'e':
			dep("e", "wepmode");
			wi_setword(iface, WI_RID_ENCRYPTION, atoi(optarg));
			exit(0);
			break;
		case 'f':
			dep("f", "channel");
			wi_setword(iface, WI_RID_OWN_CHNL, atoi(optarg));
			exit(0);
			break;
		case 'F':
			wi_setword(iface, WI_RID_PROCFRAME, atoi(optarg));
			exit(0);
			break;
 		case 'k':
			dep("k", "wepkey");
 			key = optarg;
			break;
		case 'L':
			listaps++;
			break;
		case 'l':
			wi_dumpstations(iface);
			exit(0);
			break;
		case 'p':
			dep("p", "mediaopt");
			wi_setword(iface, WI_RID_PORTTYPE, atoi(optarg));
			exit(0);
			break;
		case 'r':
			wi_setword(iface, WI_RID_RTS_THRESH, atoi(optarg));
			exit(0);
			break;
		case 't':
			dep("t", "mediaopt");
			wi_setword(iface, WI_RID_TX_RATE, atoi(optarg));
			exit(0);
			break;
		case 'n':
			dep("n", "ssid");
			wi_setstr(iface, WI_RID_DESIRED_SSID, optarg);
			exit(0);
			break;
		case 's':
			dep("s", "stationname");
			wi_setstr(iface, WI_RID_NODENAME, optarg);
			exit(0);
			break;
		case 'm':
			wi_sethex(iface, WI_RID_MAC_NODE, optarg);
			exit(0);
			break;
		case 'Q':
			quiet = 1;
			break;
		case 'q':
			dep("q", "ssid");
			wi_setstr(iface, WI_RID_OWN_SSID, optarg);
			exit(0);
			break;
		case 'S':
			dep("S", "powersavesleep");
			wi_setword(iface, WI_RID_MAX_SLEEP, atoi(optarg));
			exit(0);
			break;
		case 'T':
			dep("T", "weptxkey");
			wi_setword(iface,
			    WI_RID_TX_CRYPT_KEY, atoi(optarg) - 1);
			exit(0);
			break;
		case 'P':
			dep("P", "powersave");
			wi_setword(iface, WI_RID_PM_ENABLED, atoi(optarg));
			exit(0);
			break;
		case 'a':
			wi_setword(iface, WI_RID_SYSTEM_SCALE, atoi(optarg));
			exit(0);
			break;
		case 'v':
			modifier = atoi(optarg);
			modifier--;
			break;
		case 'h':
		default:
			usage(p);
			break;
		}
	}

	if (iface == NULL)
		usage(p);

	if (key != NULL) {
		wi_setkeys(iface, key, modifier);
		exit(0);
	}

	if (listaps > 1) {
		wi_printaplist(iface);
		exit(0);
	}

	wi_dumpinfo(iface);

	exit(0);
}
