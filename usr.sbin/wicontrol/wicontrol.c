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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>

#include <machine/if_wavelan_ieee.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#if !defined(lint)
static const char copyright[] = "@(#) Copyright (c) 1997, 1998, 1999\
	Bill Paul. All rights reserved.";
static const char rcsid[] =
	"@(#) $FreeBSD$";
#endif

static void wi_getval		__P((char *, struct wi_req *));
static void wi_setval		__P((char *, struct wi_req *));
static void wi_printstr		__P((struct wi_req *));
static void wi_setstr		__P((char *, int, char *));
static void wi_setbytes		__P((char *, int, char *, int));
static void wi_setword		__P((char *, int, int));
static void wi_sethex		__P((char *, int, char *));
static void wi_printwords	__P((struct wi_req *));
static void wi_printbool	__P((struct wi_req *));
static void wi_printhex		__P((struct wi_req *));
static void wi_dumpinfo		__P((char *, char));
static void wi_setkeys		__P((char *, char *, int));
static void wi_printkeys	__P((struct wi_req *, char));
static void usage		__P((char *));

static void wi_getval(iface, wreq)
	char			*iface;
	struct wi_req		*wreq;
{
	struct ifreq		ifr;
	int			s;

	bzero((char *)&ifr, sizeof(ifr));

	strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)wreq;

	s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s == -1)
		err(1, "socket");

	if (ioctl(s, SIOCGWAVELAN, &ifr) == -1)
		err(1, "SIOCGWAVELAN");

	close(s);

	return;
}

static void wi_setval(iface, wreq)
	char			*iface;
	struct wi_req		*wreq;
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

void wi_printstr(wreq)
	struct wi_req		*wreq;
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

void wi_setstr(iface, code, str)
	char			*iface;
	int			code;
	char			*str;
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

void wi_setbytes(iface, code, bytes, len)
	char			*iface;
	int			code;
	char			*bytes;
	int			len;
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

void wi_setword(iface, code, word)
	char			*iface;
	int			code;
	int			word;
{
	struct wi_req		wreq;

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_type = code;
	wreq.wi_len = 2;
	wreq.wi_val[0] = word;

	wi_setval(iface, &wreq);

	return;
}

void wi_sethex(iface, code, str)
	char			*iface;
	int			code;
	char			*str;
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

static int wi_hex2int(c)
	char			c;
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);

	return (0); 
}

static void wi_str2key(s, k)
	char			*s;
	struct wi_key		*k;
{
	int			n, i;
	char			*p;

	/* Is this a hex string? */
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		/* Yes, convert to int. */
		n = 0;
		p = (char *)&k->wi_keydat[0];
		for (i = 2; i < strlen(s); i+= 2) {
			*p++ = (wi_hex2int(s[i]) << 4) + wi_hex2int(s[i + 1]);
			n++;
		}
		k->wi_keylen = n;
	} else {
		/* No, just copy it in. */
		bcopy(s, k->wi_keydat, strlen(s));
		k->wi_keylen = strlen(s);
	}

	return;
}

static void wi_setkeys(iface, key, idx)
	char			*iface;
	char			*key;
	int			idx;
{
	struct wi_req		wreq;
	struct wi_ltv_keys	*keys;
	struct wi_key		*k;

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_WEP_AVAIL;

	wi_getval(iface, &wreq);
	if (wreq.wi_val[0] == 0)
		err(1, "no WEP option available on this card");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;

	wi_getval(iface, &wreq);
	keys = (struct wi_ltv_keys *)&wreq;

	if (strlen(key) > 14) {
		err(1, "encryption key must be no "
		    "more than 14 characters long");
	}

	if (idx > 3)
		err(1, "only 4 encryption keys available");

	k = &keys->wi_keys[idx];
	wi_str2key(key, k);

	wreq.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
	wreq.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
	wi_setval(iface, &wreq);

	return;
}

static void wi_printkeys(wreq, asciikeys)
	struct wi_req		*wreq;
	char asciikeys;
{
	int			i, j;
	struct wi_key		*k;
	struct wi_ltv_keys	*keys;
	unsigned char		*ptr;

	keys = (struct wi_ltv_keys *)wreq;

	for (i = 0; i < 4; i++) {
		k = &keys->wi_keys[i];
		ptr = (char *)k->wi_keydat;
		if (asciikeys) {
			for (j = 0; j < k->wi_keylen; j++) {
				if (ptr[j] == '\0')
					ptr[j] = ' ';
			}
			ptr[j] = '\0';
			printf("[ %s ]", ptr);
		} else {
			printf("[ ");
			if (k->wi_keylen)
				printf("0x");
			for (j = 0; j < k->wi_keylen; j++)
				printf("%02x",ptr[j]);
			printf(" ]");
		}
	}

	return;
};

void wi_printwords(wreq)
	struct wi_req		*wreq;
{
	int			i;

	printf("[ ");
	for (i = 0; i < wreq->wi_len - 1; i++)
		printf("%d ", wreq->wi_val[i]);
	printf("]");

	return;
}

void wi_printbool(wreq)
	struct wi_req		*wreq;
{
	if (wreq->wi_val[0])
		printf("[ On ]");
	else
		printf("[ Off ]");

	return;
}

void wi_printhex(wreq)
	struct wi_req		*wreq;
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

#define WI_STRING		0x01
#define WI_BOOL			0x02
#define WI_WORDS		0x03
#define WI_HEXBYTES		0x04
#define WI_KEYSTRUCT		0x05

struct wi_table {
	int			wi_code;
	int			wi_type;
	char			*wi_str;
};

static struct wi_table wi_table[] = {
	{ WI_RID_SERIALNO, WI_STRING, "NIC serial number:\t\t\t" },
	{ WI_RID_NODENAME, WI_STRING, "Station name:\t\t\t\t" },
	{ WI_RID_OWN_SSID, WI_STRING, "SSID for IBSS creation:\t\t\t" },
	{ WI_RID_CURRENT_SSID, WI_STRING, "Current netname (SSID):\t\t\t" },
	{ WI_RID_DESIRED_SSID, WI_STRING, "Desired netname (SSID):\t\t\t" },
	{ WI_RID_CURRENT_BSSID, WI_HEXBYTES, "Current BSSID:\t\t\t\t" },
	{ WI_RID_CHANNEL_LIST, WI_WORDS, "Channel list:\t\t\t\t" },
	{ WI_RID_OWN_CHNL, WI_WORDS, "IBSS channel:\t\t\t\t" },
	{ WI_RID_CURRENT_CHAN, WI_WORDS, "Current channel:\t\t\t" },
	{ WI_RID_COMMS_QUALITY, WI_WORDS, "Comms quality/signal/noise:\t\t" },
	{ WI_RID_PROMISC, WI_BOOL, "Promiscuous mode:\t\t\t" },
	{ WI_RID_PORTTYPE, WI_WORDS, "Port type (1=BSS, 3=ad-hoc):\t\t"},
	{ WI_RID_MAC_NODE, WI_HEXBYTES, "MAC address:\t\t\t\t"},
	{ WI_RID_TX_RATE, WI_WORDS, "TX rate (selection):\t\t\t"},
	{ WI_RID_CUR_TX_RATE, WI_WORDS, "TX rate (actual speed):\t\t\t"},
	{ WI_RID_RTS_THRESH, WI_WORDS, "RTS/CTS handshake threshold:\t\t"},
	{ WI_RID_CREATE_IBSS, WI_BOOL, "Create IBSS:\t\t\t\t" },
	{ WI_RID_SYSTEM_SCALE, WI_WORDS, "Access point density:\t\t\t" },
	{ WI_RID_PM_ENABLED, WI_WORDS, "Power Mgmt (1=on, 0=off):\t\t" },
	{ WI_RID_MAX_SLEEP, WI_WORDS, "Max sleep time:\t\t\t\t" },
	{ 0, NULL }
};

static struct wi_table wi_crypt_table[] = {
	{ WI_RID_ENCRYPTION, WI_BOOL, "WEP encryption:\t\t\t\t" },
	{ WI_RID_TX_CRYPT_KEY, WI_WORDS, "TX encryption key:\t\t\t" },
	{ WI_RID_DEFLT_CRYPT_KEYS, WI_KEYSTRUCT, "Encryption keys:\t\t\t" },
	{ 0, NULL }
};

static void wi_dumpinfo(iface, asciikeys)
	char			*iface,asciikeys;
{
	struct wi_req		wreq;
	int			i, has_wep;
	struct wi_table		*w;

	bzero((char *)&wreq, sizeof(wreq));

	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_WEP_AVAIL;

	wi_getval(iface, &wreq);
	has_wep = wreq.wi_val[0];

	w = wi_table;

	for (i = 0; w[i].wi_type; i++) {
		bzero((char *)&wreq, sizeof(wreq));

		wreq.wi_len = WI_MAX_DATALEN;
		wreq.wi_type = w[i].wi_code;

		wi_getval(iface, &wreq);
		printf("%s", w[i].wi_str);
		switch(w[i].wi_type) {
		case WI_STRING:
			wi_printstr(&wreq);
			break;
		case WI_WORDS:
			wi_printwords(&wreq);
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

			wi_getval(iface, &wreq);
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
				wi_printkeys(&wreq, asciikeys);
				break;
			default:
				break;
			}	
			printf("\n");
		}
	}

	return;
}

static void wi_dumpstats(iface)
	char			*iface;
{
	struct wi_req		wreq;
	struct wi_counters	*c;

	if (iface == NULL)
		errx(1, "must specify interface name");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = WI_MAX_DATALEN;
	wreq.wi_type = WI_RID_IFACE_STATS;

	wi_getval(iface, &wreq);

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

static void usage(p)
	char			*p;
{
	fprintf(stderr, "usage:  %s -i iface\n", p);
	fprintf(stderr, "\t%s -i iface -o\n", p);
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
	fprintf(stderr, "\t%s -i iface -P 0|1t\n", p);
	fprintf(stderr, "\t%s -i iface -S max sleep duration\n", p);
	fprintf(stderr, "\t%s -i iface -T 1|2|3|4\n", p);
#ifdef WICACHE
	fprintf(stderr, "\t%s -i iface -Z zero out signal cache\n", p);
	fprintf(stderr, "\t%s -i iface -C print signal cache\n", p);
#endif

	exit(1);
}

#ifdef WICACHE
static void wi_zerocache(iface)
	char			*iface;
{
	struct wi_req		wreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

	bzero((char *)&wreq, sizeof(wreq));
	wreq.wi_len = 0;
	wreq.wi_type = WI_RID_ZERO_CACHE;

	wi_getval(iface, &wreq);
}

static void wi_readcache(iface)
	char			*iface;
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

	wi_getval(iface, &wreq);

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

int main(argc, argv)
	int			argc;
	char			*argv[];
{
	int			ch;
	char			*iface = NULL;
	char			*p = argv[0];
	char			*key = NULL;
	int			modifier = 0, show_ascii_keys = 0;

	while((ch = getopt(argc, argv,
	    "ahoc:d:e:f:i:k:p:r:q:t:n:s:m:v:P:S:T:ZC")) != -1) {
		switch(ch) {
		case 'Z':
#ifdef WICACHE
			wi_zerocache(iface);
			exit(0);
#else
			printf("WICACHE not available\n");
#endif
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
		case 'i':
			iface = optarg;
			break;
		case 'a':
			show_ascii_keys++;
			break;
		case 'c':
			wi_setword(iface, WI_RID_CREATE_IBSS, atoi(optarg));
			exit(0);
			break;
		case 'd':
			wi_setword(iface, WI_RID_MAX_DATALEN, atoi(optarg));
			exit(0);
			break;
		case 'e':
			wi_setword(iface, WI_RID_ENCRYPTION, atoi(optarg));
			exit(0);
			break;
		case 'f':
			wi_setword(iface, WI_RID_OWN_CHNL, atoi(optarg));
			exit(0);
			break;
		case 'k':
			key = optarg;
			break;
		case 'p':
			wi_setword(iface, WI_RID_PORTTYPE, atoi(optarg));
			exit(0);
			break;
		case 'r':
			wi_setword(iface, WI_RID_RTS_THRESH, atoi(optarg));
			exit(0);
			break;
		case 't':
			wi_setword(iface, WI_RID_TX_RATE, atoi(optarg));
			exit(0);
			break;
		case 'n':
			wi_setstr(iface, WI_RID_DESIRED_SSID, optarg);
			exit(0);
			break;
		case 's':
			wi_setstr(iface, WI_RID_NODENAME, optarg);
			exit(0);
			break;
		case 'm':
			wi_sethex(iface, WI_RID_MAC_NODE, optarg);
			exit(0);
			break;
		case 'q':
			wi_setstr(iface, WI_RID_OWN_SSID, optarg);
			exit(0);
			break;
		case 'S':
			wi_setword(iface, WI_RID_MAX_SLEEP, atoi(optarg));
			exit(0);
			break;
		case 'T':
			wi_setword(iface,
			    WI_RID_TX_CRYPT_KEY, atoi(optarg) - 1);
			exit(0);
			break;
		case 'P':
			wi_setword(iface, WI_RID_PM_ENABLED, atoi(optarg));
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

	wi_dumpinfo(iface,show_ascii_keys);

	exit(0);
}
