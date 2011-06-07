/*	$FreeBSD$	*/
/*	$KAME: dump.c,v 1.32 2003/05/19 09:46:50 keiichi Exp $	*/

/*
 * Copyright (C) 2000 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>

#include <netinet/in.h>

/* XXX: the following two are non-standard include files */
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <netdb.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

#include "rtadvd.h"
#include "timer.h"
#include "if.h"
#include "dump.h"

static FILE *fp;

extern struct rainfo *ralist;

static char *ether_str(struct sockaddr_dl *);
static void if_dump(void);
static size_t dname_labeldec(char *, size_t, const char *);

static const char *rtpref_str[] = {
	"medium",		/* 00 */
	"high",			/* 01 */
	"rsv",			/* 10 */
	"low"			/* 11 */
};

static char *
ether_str(struct sockaddr_dl *sdl)
{
	static char hbuf[32];
	u_char *cp;

	if (sdl->sdl_alen && sdl->sdl_alen > 5) {
		cp = (u_char *)LLADDR(sdl);
		snprintf(hbuf, sizeof(hbuf), "%x:%x:%x:%x:%x:%x",
			cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
	} else
		snprintf(hbuf, sizeof(hbuf), "NONE");

	return (hbuf);
}

static void
if_dump(void)
{
	struct rainfo *rai;
	struct prefix *pfx;
#ifdef ROUTEINFO
	struct rtinfo *rti;
#endif
	struct rdnss *rdn;
	struct dnssl *dns;
	char prefixbuf[INET6_ADDRSTRLEN];
	struct timeval now;

	gettimeofday(&now, NULL); /* XXX: unused in most cases */
	TAILQ_FOREACH(rai, &railist, rai_next) {
		fprintf(fp, "%s:\n", rai->rai_ifname);

		fprintf(fp, "  Status: %s\n",
		    (iflist[rai->rai_ifindex]->ifm_flags & IFF_UP) ? "UP" :
		    "DOWN");

		/* control information */
		if (rai->rai_lastsent.tv_sec) {
			/* note that ctime() appends CR by itself */
			fprintf(fp, "  Last RA sent: %s",
			    ctime((time_t *)&rai->rai_lastsent.tv_sec));
		}
		if (rai->rai_timer)
			fprintf(fp, "  Next RA will be sent: %s",
			    ctime((time_t *)&rai->rai_timer->rat_tm.tv_sec));
		else
			fprintf(fp, "  RA timer is stopped");
		fprintf(fp, "  waits: %d, initcount: %d\n",
			rai->rai_waiting, rai->rai_initcounter);

		/* statistics */
		fprintf(fp, "  statistics: RA(out/in/inconsistent): "
		    "%llu/%llu/%llu, ",
		    (unsigned long long)rai->rai_raoutput,
		    (unsigned long long)rai->rai_rainput,
		    (unsigned long long)rai->rai_rainconsistent);
		fprintf(fp, "RS(input): %llu\n",
		    (unsigned long long)rai->rai_rsinput);

		/* interface information */
		if (rai->rai_advlinkopt)
			fprintf(fp, "  Link-layer address: %s\n",
			    ether_str(rai->rai_sdl));
		fprintf(fp, "  MTU: %d\n", rai->rai_phymtu);

		/* Router configuration variables */
		fprintf(fp, "  DefaultLifetime: %d, MaxAdvInterval: %d, "
		    "MinAdvInterval: %d\n", rai->rai_lifetime,
		    rai->rai_maxinterval, rai->rai_mininterval);
		fprintf(fp, "  Flags: ");
		if (rai->rai_managedflg || rai->rai_otherflg) {
			fprintf(fp, "%s", rai->rai_managedflg ? "M" : "");
			fprintf(fp, "%s", rai->rai_otherflg ? "O" : "");
		} else
			fprintf(fp, "<none>");
		fprintf(fp, ", ");
		fprintf(fp, "Preference: %s, ",
		    rtpref_str[(rai->rai_rtpref >> 3) & 0xff]);
		fprintf(fp, "MTU: %d\n", rai->rai_linkmtu);
		fprintf(fp, "  ReachableTime: %d, RetransTimer: %d, "
		    "CurHopLimit: %d\n", rai->rai_reachabletime,
		    rai->rai_retranstimer, rai->rai_hoplimit);
		if (rai->rai_clockskew)
			fprintf(fp, "  Clock skew: %ldsec\n",
			    rai->rai_clockskew);
		TAILQ_FOREACH(pfx, &rai->rai_prefix, pfx_next) {
			if (pfx == TAILQ_FIRST(&rai->rai_prefix))
				fprintf(fp, "  Prefixes:\n");
			fprintf(fp, "    %s/%d(",
			    inet_ntop(AF_INET6, &pfx->pfx_prefix, prefixbuf,
			    sizeof(prefixbuf)), pfx->pfx_prefixlen);
			switch (pfx->pfx_origin) {
			case PREFIX_FROM_KERNEL:
				fprintf(fp, "KERNEL, ");
				break;
			case PREFIX_FROM_CONFIG:
				fprintf(fp, "CONFIG, ");
				break;
			case PREFIX_FROM_DYNAMIC:
				fprintf(fp, "DYNAMIC, ");
				break;
			}
			if (pfx->pfx_validlifetime == ND6_INFINITE_LIFETIME)
				fprintf(fp, "vltime: infinity");
			else
				fprintf(fp, "vltime: %ld",
				    (long)pfx->pfx_validlifetime);
			if (pfx->pfx_vltimeexpire != 0)
				fprintf(fp, "(decr,expire %ld), ",
				    (long)pfx->pfx_vltimeexpire > now.tv_sec ?
				    (long)pfx->pfx_vltimeexpire - now.tv_sec :
				    0);
			else
				fprintf(fp, ", ");
			if (pfx->pfx_preflifetime ==  ND6_INFINITE_LIFETIME)
				fprintf(fp, "pltime: infinity");
			else
				fprintf(fp, "pltime: %ld",
				    (long)pfx->pfx_preflifetime);
			if (pfx->pfx_pltimeexpire != 0)
				fprintf(fp, "(decr,expire %ld), ",
				    (long)pfx->pfx_pltimeexpire > now.tv_sec ?
				    (long)pfx->pfx_pltimeexpire - now.tv_sec :
				    0);
			else
				fprintf(fp, ", ");
			fprintf(fp, "flags: ");
			if (pfx->pfx_onlinkflg || pfx->pfx_autoconfflg) {
				fprintf(fp, "%s",
				    pfx->pfx_onlinkflg ? "L" : "");
				fprintf(fp, "%s",
				    pfx->pfx_autoconfflg ? "A" : "");
			} else
				fprintf(fp, "<none>");
			if (pfx->pfx_timer) {
				struct timeval *rest;

				rest = rtadvd_timer_rest(pfx->pfx_timer);
				if (rest) { /* XXX: what if not? */
					fprintf(fp, ", expire in: %ld",
					    (long)rest->tv_sec);
				}
			}
			fprintf(fp, ")\n");
		}
#ifdef ROUTEINFO
		TAILQ_FOREACH(rti, &rai->rai_route, rti_next) {
			if (rti == TAILQ_FIRST(&rai->rai_route))
				fprintf(fp, "  Route Information:\n");
			fprintf(fp, "    %s/%d (",
				inet_ntop(AF_INET6, &rti->rti_prefix,
				    prefixbuf, sizeof(prefixbuf)),
				    rti->rti_prefixlen);
			fprintf(fp, "preference: %s, ",
				rtpref_str[0xff & (rti->rti_rtpref >> 3)]);
			if (rti->rti_ltime == ND6_INFINITE_LIFETIME)
				fprintf(fp, "lifetime: infinity");
			else
				fprintf(fp, "lifetime: %ld",
				    (long)rti->rti_ltime);
			fprintf(fp, ")\n");
		}
#endif
		TAILQ_FOREACH(rdn, &rai->rai_rdnss, rd_next) {
			struct rdnss_addr *rdna;

			if (rdn == TAILQ_FIRST(&rai->rai_rdnss))
				fprintf(fp, "  Recursive DNS servers:\n"
					    "    Lifetime\tServers\n");

			fprintf(fp, "    %8u\t", rdn->rd_ltime);
			TAILQ_FOREACH(rdna, &rdn->rd_list, ra_next) {
				inet_ntop(AF_INET6, &rdna->ra_dns,
				    prefixbuf, sizeof(prefixbuf));

				if (rdna != TAILQ_FIRST(&rdn->rd_list))
					fprintf(fp, "            \t");
				fprintf(fp, "%s\n", prefixbuf);
			}
			fprintf(fp, "\n");
		}

		TAILQ_FOREACH(dns, &rai->rai_dnssl, dn_next) {
			struct dnssl_addr *dnsa;
			char buf[NI_MAXHOST];

			if (dns == TAILQ_FIRST(&rai->rai_dnssl))
				fprintf(fp, "  DNS search list:\n"
					    "    Lifetime\tDomains\n");

			fprintf(fp, "    %8u\t", dns->dn_ltime);
			TAILQ_FOREACH(dnsa, &dns->dn_list, da_next) {
				dname_labeldec(buf, sizeof(buf), dnsa->da_dom);
				if (dnsa != TAILQ_FIRST(&dns->dn_list))
					fprintf(fp, "            \t");
				fprintf(fp, "%s(%d)\n", buf, dnsa->da_len);
			}
			fprintf(fp, "\n");
		}
	}
}

void
rtadvd_dump_file(const char *dumpfile)
{
	syslog(LOG_DEBUG, "<%s> dump current status to %s", __func__,
	    dumpfile);

	if ((fp = fopen(dumpfile, "w")) == NULL) {
		syslog(LOG_WARNING, "<%s> open a dump file(%s)",
		    __func__, dumpfile);
		return;
	}

	if_dump();

	fclose(fp);
}

/* Decode domain name label encoding in RFC 1035 Section 3.1 */
static size_t
dname_labeldec(char *dst, size_t dlen, const char *src)
{
	size_t len;
	const char *src_origin;
	const char *src_last;
	const char *dst_origin;

	src_origin = src;
	src_last = strchr(src, '\0');
	dst_origin = dst;
	memset(dst, '\0', dlen);
	while (src && (len = (uint8_t)(*src++) & 0x3f) &&
	    (src + len) <= src_last) {
		if (dst != dst_origin)
			*dst++ = '.';
		syslog(LOG_DEBUG, "<%s> labellen = %zd", __func__, len);
		memcpy(dst, src, len);
		src += len;
		dst += len;
	}
	*dst = '\0';

	return (src - src_origin);
}
