/*
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_mib.h>
#include <net/if_atm.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "atmconfig.h"
#include "private.h"
#include "diag.h"

static void natm_add(int, char *[]);
static void natm_delete(int, char *[]);
static void natm_show(int, char *[]);

const struct cmdtab natm_tab[] = {
	{ "add",	NULL,		natm_add },
	{ "delete",	NULL,		natm_delete },
	{ "show",	NULL,		natm_show },
	{ NULL, 	NULL, 		NULL }
};

/*
 * Structure to hold a route
 */
struct natm_route {
	TAILQ_ENTRY(natm_route) link;
	struct in_addr	host;
	struct diagif	*aif;
	u_int		flags;
	int		llcsnap;
	u_int		vpi, vci;
	u_int		traffic;
	u_int		pcr, scr, mbs, icr, mcr;
	u_int		tbe, nrm, trm, adtf, rif, rdf, cdf;
};
static TAILQ_HEAD(, natm_route) natm_route_list =
    TAILQ_HEAD_INITIALIZER(natm_route_list);

static void
store_route(struct rt_msghdr *rtm)
{
	u_int i;
	struct natm_route *r;
	char *cp;
	struct sockaddr *sa;
	struct sockaddr_in *sain;
	struct sockaddr_dl *sdl;
	struct diagif *aif;
	u_int n;

	r = malloc(sizeof(*r));
	if (r == NULL)
		err(1, "allocate route");

	r->flags = rtm->rtm_flags;
	cp = (char *)(rtm + 1);
	for (i = 1; i != 0; i <<= 1) {
		if (rtm->rtm_addrs & i) {
			sa = (struct sockaddr *)cp;
			cp += roundup(sa->sa_len, sizeof(long));
			switch (i) {

			  case RTA_DST:
				if (sa->sa_family != AF_INET) {
					warnx("RTA_DST not AF_INET %u", sa->sa_family);
					goto fail;
				}
				sain = (struct sockaddr_in *)(void *)sa;
				if (sain->sin_len < 4)
					r->host.s_addr = INADDR_ANY;
				else
					r->host = sain->sin_addr;
				break;

			  case RTA_GATEWAY:
				if (sa->sa_family != AF_LINK) {
					warnx("RTA_GATEWAY not AF_LINK");
					goto fail;
				}
				sdl = (struct sockaddr_dl *)(void *)sa;
				TAILQ_FOREACH(aif, &diagif_list, link)
					if (strlen(aif->ifname) ==
					    sdl->sdl_nlen &&
					    strncmp(aif->ifname, sdl->sdl_data,
					    sdl->sdl_nlen) == 0)
						break;
				if (aif == NULL) {
					warnx("interface '%.*s' not found",
					    sdl->sdl_nlen, sdl->sdl_data);
					goto fail;
				}
				r->aif = aif;

				/* parse ATM stuff */

#define	GET3()	(((sdl->sdl_data[n] & 0xff) << 16) |	\
		 ((sdl->sdl_data[n + 1] & 0xff) << 8) |	\
		 ((sdl->sdl_data[n + 2] & 0xff) << 0))
#define	GET2()	(((sdl->sdl_data[n] & 0xff) << 8) |	\
		 ((sdl->sdl_data[n + 1] & 0xff) << 0))
#define	GET1()	(((sdl->sdl_data[n] & 0xff) << 0))

				n = sdl->sdl_nlen;
				if (sdl->sdl_alen < 4) {
					warnx("RTA_GATEWAY alen too short");
					goto fail;
				}
				r->llcsnap = GET1() & ATM_PH_LLCSNAP;
				n++;
				r->vpi = GET1();
				n++;
				r->vci = GET2();
				n += 2;
				if (sdl->sdl_alen == 4) {
					/* old address */
					r->traffic = ATMIO_TRAFFIC_UBR;
					r->pcr = 0;
					break;
				}
				/* new address */
				r->traffic = GET1();
				n++;
				switch (r->traffic) {

				  case ATMIO_TRAFFIC_UBR:
					if (sdl->sdl_alen >= 5 + 3) {
						r->pcr = GET3();
						n += 3;
					} else
						r->pcr = 0;
					break;

				  case ATMIO_TRAFFIC_CBR:
					if (sdl->sdl_alen < 5 + 3) {
						warnx("CBR address too short");
						goto fail;
					}
					r->pcr = GET3();
					n += 3;
					break;

				  case ATMIO_TRAFFIC_VBR:
					if (sdl->sdl_alen < 5 + 3 * 3) {
						warnx("VBR address too short");
						goto fail;
					}
					r->pcr = GET3();
					n += 3;
					r->scr = GET3();
					n += 3;
					r->mbs = GET3();
					n += 3;
					break;

				  case ATMIO_TRAFFIC_ABR:
					if (sdl->sdl_alen < 5 + 4 * 3 + 2 +
					    1 * 2 + 3) {
						warnx("ABR address too short");
						goto fail;
					}
					r->pcr = GET3();
					n += 3;
					r->mcr = GET3();
					n += 3;
					r->icr = GET3();
					n += 3;
					r->tbe = GET3();
					n += 3;
					r->nrm = GET1();
					n++;
					r->trm = GET1();
					n++;
					r->adtf = GET2();
					n += 2;
					r->rif = GET1();
					n++;
					r->rdf = GET1();
					n++;
					r->cdf = GET1();
					n++;
					break;

				  default:
					goto fail;
				}
				break;
			}
		}
	}

	TAILQ_INSERT_TAIL(&natm_route_list, r, link);

	return;
  fail:
	free(r);
}

/*
 * Fetch the INET routes that a ours
 */
static void
natm_route_fetch(void)
{
	int name[6];
	size_t needed;
	u_char *buf, *next;
	struct rt_msghdr *rtm;

	name[0] = CTL_NET;
	name[1] = PF_ROUTE;
	name[2] = 0;
	name[3] = AF_INET;
	name[4] = NET_RT_DUMP;
	name[5] = 0;

	if (sysctl(name, 6, NULL, &needed, NULL, 0) == -1)
		err(1, "rtable estimate");
	needed *= 2;
	if ((buf = malloc(needed)) == NULL)
		err(1, "rtable buffer (%zu)", needed);
	if (sysctl(name, 6, buf, &needed, NULL, 0) == -1)
		err(1, "rtable get");

	next = buf;
	while (next < buf + needed) {
		rtm = (struct rt_msghdr *)(void *)next;
		next += rtm->rtm_msglen;

		if (rtm->rtm_type == RTM_GET) {
			if ((rtm->rtm_flags & (RTF_UP | RTF_HOST |
			    RTF_STATIC)) == (RTF_UP | RTF_HOST | RTF_STATIC) &&
			    (rtm->rtm_addrs & (RTA_DST | RTA_GATEWAY |
			    RTA_IFP)) == (RTA_DST | RTA_GATEWAY | RTA_IFP))
				store_route(rtm);
		}
	}
}

static u_long
parse_num(const char *arg, const char *name, u_long limit)
{
	u_long res;
	char *end;

	errno = 0;
	res = strtoul(arg, &end, 10);
	if (*end != '\0' || end == arg || errno != 0)
		errx(1, "cannot parse %s '%s'", name, arg);
	if (res > limit)
		errx(1, "%s out of range (0...%lu)", name, limit);
	return (res);
}

static void
do_route(u_int type, u_int flags, const struct sockaddr_in *sain,
    const struct sockaddr_dl *sdl)
{
	struct {
		struct rt_msghdr h;
		char	space[512];
	} msg;
	char *ptr;
	int s;
	ssize_t rlen;

	/* create routing message */
	bzero(&msg, sizeof(msg));
	msg.h.rtm_msglen = sizeof(msg.h);
	msg.h.rtm_version = RTM_VERSION;
	msg.h.rtm_type = type;
	msg.h.rtm_index = 0;
	msg.h.rtm_flags = flags;
	msg.h.rtm_addrs = RTA_DST | (sdl != NULL ? RTA_GATEWAY : 0);
	msg.h.rtm_pid = getpid();

	ptr = (char *)&msg + sizeof(msg.h);
	memcpy(ptr, sain, sain->sin_len);
	ptr += roundup(sain->sin_len, sizeof(long));
	msg.h.rtm_msglen += roundup(sain->sin_len, sizeof(long));

	if (sdl != NULL) {
		memcpy(ptr, sdl, sdl->sdl_len);
		ptr += roundup(sdl->sdl_len, sizeof(long));
		msg.h.rtm_msglen += roundup(sdl->sdl_len, sizeof(long));
	}

	/* open socket */
	s = socket(PF_ROUTE, SOCK_RAW, AF_INET);
	if (s == -1)
		err(1, "cannot open routing socket");

	rlen = write(s, &msg, msg.h.rtm_msglen);
	if (rlen == -1)
		err(1, "writing to routing socket");
	if ((size_t)rlen != msg.h.rtm_msglen)
		errx(1, "short write to routing socket: %zu %u",
		    (size_t)rlen, msg.h.rtm_msglen);
	close(s);
}

/*
 * Add a new NATM route
 */
static void
natm_add(int argc, char *argv[])
{
	int opt;
	struct hostent *hp;
	struct sockaddr_in sain;
	struct sockaddr_dl sdl;
	struct diagif *aif;
	u_long num, num1;
	u_int idx;

	static int printonly;

	static const struct option opts[] = {
	    { "printonly", OPT_SIMPLE, &printonly },
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	if (argc < 5)
		errx(1, "missing arguments for 'natm add'");

	memset(&sdl, 0, sizeof(sdl));
	sdl.sdl_len = sizeof(sdl);
	sdl.sdl_family = AF_LINK;

	/* get the IP address for <dest> */
	memset(&sain, 0, sizeof(sain));
	hp = gethostbyname(argv[0]);
	if (hp == NULL)
		errx(1, "bad hostname %s: %s", argv[0], hstrerror(h_errno));
	if (hp->h_addrtype != AF_INET)
		errx(1, "bad address type for %s", argv[0]);
	sain.sin_len = sizeof(sain);
	sain.sin_family = AF_INET;
	memcpy(&sain.sin_addr, hp->h_addr, sizeof(sain.sin_addr));

	/* find interface */
	diagif_fetch();
	TAILQ_FOREACH(aif, &diagif_list, link)
		if (strcmp(aif->ifname, argv[1]) == 0)
			break;
	if (aif == NULL)
		errx(1, "unknown ATM interface '%s'", argv[1]);
	sdl.sdl_index = aif->index;
	strcpy(sdl.sdl_data, aif->ifname);
	idx = sdl.sdl_nlen = strlen(aif->ifname);
	idx++;

	/* verify VPI/VCI */
	num = parse_num(argv[2], "VPI", (1U << aif->mib.vpi_bits));
	sdl.sdl_data[idx++] = num & 0xff;
	num = parse_num(argv[3], "VCI", (1U << aif->mib.vci_bits));
	if (num == 0)
		errx(1, "VCI may not be 0");
	sdl.sdl_data[idx++] = (num >> 8) & 0xff;
	sdl.sdl_data[idx++] = num & 0xff;

	/* encapsulation */
	if (strcasecmp(argv[4], "llc/snap") == 0) {
		sdl.sdl_data[sdl.sdl_nlen] = ATM_PH_LLCSNAP;
	} else if (strcasecmp(argv[4], "aal5") == 0) {
		sdl.sdl_data[sdl.sdl_nlen] = 0;
	} else
		errx(1, "bad encapsulation type '%s'", argv[4]);

	/* look at the traffic */
	argc -= 5;
	argv += 5;

	if (argc != 0) {
		if (strcasecmp(argv[0], "ubr") == 0) {
			sdl.sdl_data[idx++] = ATMIO_TRAFFIC_UBR;
			if (argc == 1)
				/* ok */;
			else if (argc == 2) {
				num = parse_num(argv[1], "PCR", aif->mib.pcr);
				sdl.sdl_data[idx++] = (num >> 16) & 0xff;
				sdl.sdl_data[idx++] = (num >>  8) & 0xff;
				sdl.sdl_data[idx++] = (num >>  0) & 0xff;
			} else
				errx(1, "too many parameters for UBR");

		} else if (strcasecmp(argv[0], "cbr") == 0) {
			sdl.sdl_data[idx++] = ATMIO_TRAFFIC_CBR;
			if (argc == 1)
				errx(1, "missing PCR for CBR");
			if (argc > 2)
				errx(1, "too many parameters for CBR");
			num = parse_num(argv[1], "PCR", aif->mib.pcr);
			sdl.sdl_data[idx++] = (num >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

		} else if (strcasecmp(argv[0], "vbr") == 0) {
			sdl.sdl_data[idx++] = ATMIO_TRAFFIC_VBR;

			if (argc < 4)
				errx(1, "missing arg(s) for VBR");
			if (argc > 4)
				errx(1, "too many parameters for VBR");

			num = parse_num(argv[1], "PCR", aif->mib.pcr);
			sdl.sdl_data[idx++] = (num >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;
			num = parse_num(argv[2], "SCR", num);
			sdl.sdl_data[idx++] = (num >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;
			num = parse_num(argv[3], "MBS", 0xffffffLU);
			sdl.sdl_data[idx++] = (num >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

		} else if (strcasecmp(argv[0], "abr") == 0) {
			sdl.sdl_data[idx++] = ATMIO_TRAFFIC_ABR;
			if (argc < 11)
				errx(1, "missing arg(s) for ABR");
			if (argc > 11)
				errx(1, "too many parameters for ABR");

			num = parse_num(argv[1], "PCR", aif->mib.pcr);
			sdl.sdl_data[idx++] = (num >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			num1 = parse_num(argv[2], "MCR", num);
			sdl.sdl_data[idx++] = (num1 >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num1 >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num1 >>  0) & 0xff;

			num = parse_num(argv[3], "ICR", num);
			sdl.sdl_data[idx++] = (num >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			if (num < num1)
				errx(1, "ICR must be >= MCR");

			num = parse_num(argv[4], "TBE", 0xffffffUL);
			sdl.sdl_data[idx++] = (num >> 16) & 0xff;
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			num = parse_num(argv[5], "NRM", 0x7UL);
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			num = parse_num(argv[6], "TRM", 0x7UL);
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			num = parse_num(argv[7], "ADTF", 0x3ffUL);
			sdl.sdl_data[idx++] = (num >>  8) & 0xff;
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			num = parse_num(argv[8], "RIF", 0xfUL);
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			num = parse_num(argv[9], "RDF", 0xfUL);
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

			num = parse_num(argv[10], "CDF", 0x7UL);
			sdl.sdl_data[idx++] = (num >>  0) & 0xff;

		} else
			errx(1, "bad traffic type '%s'", argv[0]);
	} else
		sdl.sdl_data[idx++] = ATMIO_TRAFFIC_UBR;

	sdl.sdl_alen = idx - sdl.sdl_nlen;
	sdl.sdl_len += sdl.sdl_nlen + sdl.sdl_alen;

	if (printonly) {
		printf("route add -iface %s -link %.*s",
		    inet_ntoa(sain.sin_addr), sdl.sdl_nlen, sdl.sdl_data);
		for (idx = 0; idx < sdl.sdl_alen; idx++)
			printf("%c%x", ".:"[idx == 0],
			    (u_int)sdl.sdl_data[sdl.sdl_nlen + idx] & 0xffU);
		printf("\n");
		exit(0);
	}

	do_route(RTM_ADD, RTF_HOST | RTF_STATIC | RTF_UP, &sain, &sdl);
}

/*
 * Delete an NATM route
 */
static void
natm_delete(int argc, char *argv[])
{
	int opt;
	struct hostent *hp;
	struct sockaddr_in sain;
	u_int vpi, vci;
	struct diagif *aif;
	struct natm_route *r;

	static int printonly;

	static const struct option opts[] = {
	    { "printonly", OPT_SIMPLE, &printonly },
	    { NULL, 0, NULL }
	};

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	diagif_fetch();
	natm_route_fetch();

	memset(&sain, 0, sizeof(sain));
	sain.sin_len = sizeof(sain);
	sain.sin_family = AF_INET;

	if (argc == 1) {
		/* get the IP address for <dest> */
		hp = gethostbyname(argv[0]);
		if (hp == NULL)
			errx(1, "bad hostname %s: %s", argv[0],
			    hstrerror(h_errno));
		if (hp->h_addrtype != AF_INET)
			errx(1, "bad address type for %s", argv[0]);
		memcpy(&sain.sin_addr, hp->h_addr, sizeof(sain.sin_addr));

		TAILQ_FOREACH(r, &natm_route_list, link)
			if (r->host.s_addr == sain.sin_addr.s_addr)
				break;
		if (r == NULL)
			errx(1, "no NATM route to host '%s' (%s)", argv[0],
			    inet_ntoa(sain.sin_addr));

	} else if (argc == 3) {
		TAILQ_FOREACH(aif, &diagif_list, link)
			if (strcmp(aif->ifname, argv[0]) == 0)
				break;
		if (aif == 0)
			errx(1, "no such interface '%s'", argv[0]);

		vpi = parse_num(argv[1], "VPI", 0xff);
		vci = parse_num(argv[2], "VCI", 0xffff);

		TAILQ_FOREACH(r, &natm_route_list, link)
			if (r->aif == aif && r->vpi == vpi && r->vci == vci)
				break;
		if (r == NULL)
			errx(1, "no such NATM route %s %u %u", argv[0],
			    vpi, vci);
		sain.sin_addr = r->host;

	} else
		errx(1, "bad number of arguments for 'natm delete'");

	if (printonly) {
		printf("route delete %s\n", inet_ntoa(r->host));
		exit(0);
	}

	do_route(RTM_DELETE, r->flags, &sain, NULL);
}

/*
 * Show NATM routes
 */
static void
natm_show(int argc, char *argv[])
{
	int opt;
	struct natm_route *r;
	struct hostent *hp;

	static const char *const traffics[] = {
		[ATMIO_TRAFFIC_UBR] = "UBR",
		[ATMIO_TRAFFIC_CBR] = "CBR",
		[ATMIO_TRAFFIC_VBR] = "VBR",
		[ATMIO_TRAFFIC_ABR] = "ABR"
	};

	static int numeric, abr;

	static const struct option opts[] = {
	    { "abr", OPT_SIMPLE, &abr },
	    { "numeric", OPT_SIMPLE, &numeric },
	    { NULL, 0, NULL }
	};

	static const char head[] =
	    "Destination         Iface       VPI VCI   Encaps   Trf PCR     "
	    "SCR/MCR MBS/ICR\n";
	static const char head_abr[] =
	    "Destination         Iface       VPI VCI   Encaps   Trf PCR     "
	    "SCR/MCR MBS/ICR TBE     NRM TRM ADTF RIF RDF CDF\n";

	while ((opt = parse_options(&argc, &argv, opts)) != -1)
		switch (opt) {
		}

	diagif_fetch();
	natm_route_fetch();

	heading_init();
	TAILQ_FOREACH(r, &natm_route_list, link) {
		heading(abr ? head_abr : head);
		if (numeric)
			printf("%-20s", inet_ntoa(r->host));
		else if (r->host.s_addr == INADDR_ANY)
			printf("%-20s", "default");
		else {
			hp = gethostbyaddr((char *)&r->host, sizeof(r->host),
			    AF_INET);
			if (hp != NULL)
				printf("%-20s", hp->h_name);
			else
				printf("%-20s", inet_ntoa(r->host));
		}
		printf("%-12s%-4u%-6u%-9s%-4s", r->aif->ifname, r->vpi, r->vci,
		    r->llcsnap ? "LLC/SNAP" : "AAL5", traffics[r->traffic]);
		switch (r->traffic) {

		  case ATMIO_TRAFFIC_UBR:
		  case ATMIO_TRAFFIC_CBR:
			printf("%-8u", r->pcr);
			break;

		  case ATMIO_TRAFFIC_VBR:
			printf("%-8u%-8u%-8u", r->pcr, r->scr, r->mbs);
			break;

		  case ATMIO_TRAFFIC_ABR:
			printf("%-8u%-8u%-8u", r->pcr, r->mcr, r->icr);
			if (abr)
				printf("%-8u%-4u%-4u%-5u%-4u%-4u%-4u",
				    r->tbe, r->nrm, r->trm, r->adtf,
				    r->rif, r->rdf, r->cdf);
			break;
		}
		printf("\n");
	}
}
