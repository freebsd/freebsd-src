/*
 * Copyright (c) 1999, 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_ieee80211.h>

#include <dev/ray/if_rayreg.h>
#include <dev/ray/if_raymib.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

static char *	ray_printhex	(u_int8_t *d, char *s, int len);
static void	ray_getval	(char *iface, struct ray_param_req *rreq);
static void	ray_getstats	(char *iface, struct ray_stats_req *sreq);
static int	ray_version	(char *iface);
static void	ray_dumpstats	(char *iface);
static void	ray_dumpinfo	(char *iface);
static void	ray_setstr	(char *iface, u_int8_t mib, char *s);
static void	ray_setword	(char *iface, u_int8_t mib, u_int16_t v);
static void	ray_setval	(char *iface, struct ray_param_req *rreq);
static void	usage		(char *p);

static char	*mib_strings[] = RAY_MIB_STRINGS;
static char	*mib_help_strings[] = RAY_MIB_HELP_STRINGS;
static int	mib_info[RAY_MIB_MAX+1][3] = RAY_MIB_INFO;

static char *
ray_printhex(u_int8_t *d, char *s, int len)
{
	static char buf[3*256];
	char *p;
    	int i;

	if (2 * len + strlen(s) * (len - 1) > sizeof(buf) - 1)
		err(1, "Byte string too long");

	sprintf(buf, "%02x", *d);
	for (p = buf + 2, i = 1; i < len; i++)
		p += sprintf(p, "%s%02x", s, *(d+i));

	return(buf);
}

static void
ray_getval(char *iface, struct ray_param_req *rreq)
{
	struct ifreq ifr;
	int s;

        bzero((char *)&ifr, sizeof(ifr));

        strcpy(ifr.ifr_name, iface);
        ifr.ifr_data = (caddr_t)rreq;

        s = socket(AF_INET, SOCK_DGRAM, 0);

        if (s == -1)
                err(1, "socket");

        if (ioctl(s, SIOCGRAYPARAM, &ifr) == -1)
                warn("SIOCGRAYPARAM failed with failcode 0x%02x",
		    rreq->r_failcause);

        close(s);
}

static void
ray_getsiglev(char *iface, struct ray_siglev *siglev)
{
	struct ifreq ifr;
	int s;

        bzero((char *)&ifr, sizeof(ifr));

        strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
        ifr.ifr_data = (caddr_t)siglev;

        s = socket(AF_INET, SOCK_DGRAM, 0);

        if (s == -1)
                err(1, "socket");

        if (ioctl(s, SIOCGRAYSIGLEV, &ifr) == -1)
                err(1, "SIOCGRAYSIGLEV failed");

        close(s);
}

static void
ray_getstats(char *iface, struct ray_stats_req *sreq)
{
	struct ifreq ifr;
	int s;

        bzero((char *)&ifr, sizeof(ifr));

        strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
        ifr.ifr_data = (caddr_t)sreq;

        s = socket(AF_INET, SOCK_DGRAM, 0);

        if (s == -1)
                err(1, "socket");

        if (ioctl(s, SIOCGRAYSTATS, &ifr) == -1)
                err(1, "SIOCGRAYSTATS failed");

        close(s);
}

static int
ray_version(char *iface)
{
	struct ray_param_req rreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

        bzero((char *)&rreq, sizeof(rreq));
	rreq.r_paramid = RAY_MIB_VERSION;
	ray_getval(iface, &rreq);
	return(*rreq.r_data);
}

static void
ray_dumpinfo(char *iface)
{
	struct ray_param_req rreq;
	u_int8_t mib, version;

	if (iface == NULL)
		errx(1, "must specify interface name");

        bzero((char *)&rreq, sizeof(rreq));

	version = ray_version(iface);
	printf("%-26s\t",  mib_strings[RAY_MIB_VERSION]);
	printf("%d\n", 3+version);

	for (mib = RAY_MIB_NET_TYPE; mib <= RAY_MIB_MAX; mib++) {

		if ((mib_info[mib][0] & version) == 0)
			continue;
		if (mib == RAY_MIB_VERSION)
			continue;

		rreq.r_paramid = mib;
		ray_getval(iface, &rreq);
		printf("%-26s\t",  mib_strings[mib]);
		switch (rreq.r_len) {

		case 2:
			printf("0x%02x%02x", *rreq.r_data, *(rreq.r_data+1));
			break;

		case ETHER_ADDR_LEN:
			printf("%s",
			    ray_printhex(rreq.r_data, ":", rreq.r_len));
			break;

		case IEEE80211_NWID_LEN:
			printf("%-32s", (char *)rreq.r_data);
			break;


		case 1:
		default:
			printf("0x%02x", *rreq.r_data);
			break;
		}
		printf("\t%s\n",  mib_help_strings[mib]);
	}
}

static void
ray_dumpsiglev(char *iface)
{
	struct ray_siglev siglevs[RAY_NSIGLEVRECS];
	int i;

	if (iface == NULL)
		errx(1, "must specify interface name");

        bzero((char *)siglevs, sizeof(siglevs));

	ray_getsiglev(iface, siglevs);

	for (i = 0; i < RAY_NSIGLEVRECS; i++) {
		printf("Slot %d: %s", i,
		    ray_printhex(siglevs[i].rsl_host, ":", ETHER_ADDR_LEN));
		printf("  %s",
		    ray_printhex(siglevs[i].rsl_siglevs, ",", RAY_NSIGLEV));
		printf("  %s\n",
		    ray_printhex(siglevs[i].rsl_antennas, "", RAY_NANTENNA));
	}

}

static void
ray_dumpstats(char *iface)
{
	struct ray_stats_req sreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

        bzero((char *)&sreq, sizeof(sreq));

	ray_getstats(iface, &sreq);

	printf("Receiver overflows        %lu\n",
	    (unsigned long int)sreq.rxoverflow);
	printf("Receiver checksum errors  %lu\n",
	    (unsigned long int)sreq.rxcksum);
	printf("Header checksum errors    %lu\n",
	    (unsigned long int)sreq.rxhcksum);
	printf("Clear channel noise level %u\n", sreq.rxnoise);
}

static void
ray_setval(char *iface, struct ray_param_req *rreq)
{
	struct ifreq ifr;
	int s;

        bzero((char *)&ifr, sizeof(ifr));

        strlcpy(ifr.ifr_name, iface, sizeof(ifr.ifr_name));
        ifr.ifr_data = (caddr_t)rreq;

        s = socket(AF_INET, SOCK_DGRAM, 0);

        if (s == -1)
                err(1, "socket");

        if (ioctl(s, SIOCSRAYPARAM, &ifr) == -1) {
                err(1, "SIOCSRAYPARAM failed with failcode 0x%02x",
		    rreq->r_failcause);
	}

        close(s);
}

static void
ray_setword(char *iface, u_int8_t mib, u_int16_t v)
{
	struct ray_param_req rreq;

	if (iface == NULL)
		errx(1, "must specify interface name");

        bzero((char *)&rreq, sizeof(rreq));

	rreq.r_paramid = mib;
	rreq.r_len = RAY_MIB_SIZE(mib_info, mib, ray_version(iface));
	switch (rreq.r_len) {

	case 1:
		*rreq.r_data = (u_int8_t)(v & 0xff);
		break;

	case 2:
		*rreq.r_data = (u_int8_t)((v & 0xff00) >> 8);
		*(rreq.r_data+1) = (u_int8_t)(v & 0xff);
		break;

	default:
	    	break;
	}

	ray_setval(iface, &rreq);
}

static void
ray_setstr(char *iface, u_int8_t mib, char *s)
{
	struct ray_param_req rreq;

	if (iface == NULL)
		errx(1, "must specify interface name");
	if (s == NULL)
		errx(1, "must specify string");
	if (strlen(s) > RAY_MIB_SIZE(mib_info, mib, ray_version(iface)))
		errx(1, "string too long");

        bzero((char *)&rreq, sizeof(rreq));

	rreq.r_paramid = mib;
	rreq.r_len = RAY_MIB_SIZE(mib_info, mib, ray_version(iface));
	bcopy(s, (char *)rreq.r_data, strlen(s));

	ray_setval(iface, &rreq);
}

static void
usage(char *p)
{
	fprintf(stderr, "usage:  %s -i iface\n", p);
	fprintf(stderr, "\t%s -i iface -o\n", p);
	fprintf(stderr, "\t%s -i iface -t tx rate\n", p);
	fprintf(stderr, "\t%s -i iface -n network name\n", p);
	fprintf(stderr, "\t%s -i iface -p port type\n", p);
	fprintf(stderr, "\t%s -i iface -m mac address\n", p);
	fprintf(stderr, "\t%s -i iface -d max data length\n", p);
	fprintf(stderr, "\t%s -i iface -r RTS threshold\n", p);
	fprintf(stderr, "\t%s -i iface -f hopset\n", p);
	fprintf(stderr, "\t%s -i iface -P 0|1\n", p);
	fprintf(stderr, "\t%s -i iface -S max sleep duration\n", p);
	fprintf(stderr, "\t%s -i iface -C print signal cache\n", p);

	exit(1);
}

int
main(int argc, char *argv[])
{

	char *iface, *p;
	int ch, val;

	iface = NULL;
	p = argv[0];

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
			iface = "ray0";
			optind = 1;
		}
		optreset = 1;
	}
	opterr = 1;

	while ((ch = getopt(argc, argv, "hoCi:d:f:n:p:r:t:W:")) != -1) {
		switch (ch) {

		case 'i':
			iface = optarg;
			break;

		case 'd':
		    	val = atoi(optarg);
			if (((val < 350) &&
			    (val != -1)) || (val > RAY_MIB_FRAG_THRESH_MAXIMUM))
				usage(p);
			if (val == -1)
				val = 0x7fff;
			ray_setword(iface, RAY_MIB_FRAG_THRESH, val);
			exit(0);
			break;

		case 'f':
		    	val = atoi(optarg);
			if ((val < RAY_MIB_COUNTRY_CODE_MIMIMUM) ||
			    (val > RAY_MIB_COUNTRY_CODE_MAXIMUM))
				usage(p);
		    	ray_setword(iface, RAY_MIB_COUNTRY_CODE, val);
			exit(0);
			break;

		case 'n':
		    	ray_setstr(iface, RAY_MIB_SSID, optarg);
			exit(0);
			break;

		case 'o':
			ray_dumpstats(iface);
			exit(0);
			break;

		case 'p':
		    	val = atoi(optarg);
			if ((val < 0) || (val > 1))
				usage(p);
			ray_setword(iface, RAY_MIB_NET_TYPE, val);
			exit(0);
			break;


		case 'r':
		    	val = atoi(optarg);
			if ((val < -1) || (val > RAY_MIB_RTS_THRESH_MAXIMUM))
				usage(p);
			if (val == -1)
				val = 0x7fff;
			ray_setword(iface, RAY_MIB_RTS_THRESH, val);
			exit(0);
			break;

		case 't':
		    	val = atoi(optarg);
			if ((val < RAY_MIB_BASIC_RATE_SET_MINIMUM) ||
			    (val > RAY_MIB_BASIC_RATE_SET_MAXIMUM))
				usage(p);
		    	ray_setword(iface, RAY_MIB_BASIC_RATE_SET, val);
			exit(0);
			break;

		case 'C':
		    	ray_dumpsiglev(iface);
			exit(0);
			break;

		case 'W':
			{
				char *stringp, **ap, *av[5];
				u_int8_t mib;

				stringp = optarg;
				ap = av;
				*ap = strsep(&stringp, ":");
				ap++;
				*ap = strsep(&stringp, ":");
				mib = atoi(av[0]);
				sscanf(av[1], "%x", &val);
				printf("mib %d, val 0x%02x\n", mib, val);
				ray_setword(iface, mib, val);
			}
			exit(0);
			break;

		case 'h':
		default:
			usage(p);

		}
	}

	if (iface == NULL)
		usage(p);

	ray_dumpinfo(iface);

	exit(0);
}
