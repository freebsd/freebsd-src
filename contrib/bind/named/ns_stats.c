#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_stats.c	4.10 (Berkeley) 6/27/90";
static char rcsid[] = "$Id: ns_stats.c,v 8.8 1996/09/22 00:13:10 vixie Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986,1994
 * -
 * Copyright (c) 1986,1994
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

/**************************************************************************/
/*                simple monitoring of named behavior                     */
/*            dumps a bunch of values into a well-known file              */
/**************************************************************************/

#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>

#include "named.h"
#include "tree.h"

static u_long typestats[T_ANY+1];
static const char *typenames[T_ANY+1] = {
	/* 5 types per line */
	"Unknown", "A", "NS", "invalid(MD)", "invalid(MF)",
	"CNAME", "SOA", "MB", "MG", "MR",
	"NULL", "WKS", "PTR", "HINFO", "MINFO",
	"MX", "TXT", "RP", "AFSDB", "X25",
	"ISDN", "RT", "NSAP", "NSAP_PTR", "SIG",
	"KEY", "PX", "invalid(GPOS)", "AAAA", "LOC",
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0,
	/* 20 per line */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 100 */
	"UINFO", "UID", "GID", "UNSPEC", 0, 0, 0, 0, 0, 0,
	/* 110 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 120 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 200 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 240 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 250 */
	0, 0, "AXFR", "MAILB", "MAILA", "ANY" 
};

static void		nameserStats __P((FILE *));

void
ns_stats()
{
	time_t timenow = time(NULL);
	register FILE *f;
	register int i;

	syslog(LOG_NOTICE, "dumping nameserver stats\n");

	if (!(f = fopen(statsfile, "a"))) {
		syslog(LOG_NOTICE, "cannot open stat file, \"%s\"\n",
		       statsfile);
		return;
	}

	fprintf(f, "+++ Statistics Dump +++ (%ld) %s",
		(long)timenow, ctime(&timenow));
	fprintf(f, "%ld\ttime since boot (secs)\n",
		(long)(timenow - boottime));
	fprintf(f, "%ld\ttime since reset (secs)\n",
		(long)(timenow - resettime));

	/* query type statistics */
	fprintf(f, "%lu\tUnknown query types\n", (u_long)typestats[0]);
	for(i=1; i < T_ANY+1; i++)
		if (typestats[i])
			if (typenames[i])
				fprintf(f, "%lu\t%s queries\n",
					(u_long)typestats[i], typenames[i]);
			else
				fprintf(f, "%lu\ttype %d queries\n",
					(u_long)typestats[i], i);

	/* name server statistics */
	nameserStats(f);

	fprintf(f, "--- Statistics Dump --- (%ld) %s",
		(long)timenow, ctime(&timenow));
	(void) my_fclose(f);
	syslog(LOG_NOTICE, "done dumping nameserver stats\n");
}

void
qtypeIncr(qtype)
	int qtype;
{
	if (qtype < T_A || qtype > T_ANY)
		qtype = 0;	/* bad type */
	typestats[qtype]++;
}

static tree		*nameserTree;
static int		nameserInit;

#ifdef STATS
static FILE		*nameserStatsFile;
static u_long		globalStats[nssLast];
static const char	*statNames[nssLast] = {
			"RR",		/* sent us an answer */
			"RNXD",		/* sent us a negative response */
			"RFwdR",	/* sent us a response we had to fwd */
			"RDupR",	/* sent us an extra answer */
			"RFail",	/* sent us a SERVFAIL */
			"RFErr",	/* sent us a FORMERR */
			"RErr",		/* sent us some other error */
			"RAXFR",	/* sent us an AXFR */
			"RLame",	/* sent us a lame delegation */
			"ROpts",	/* sent us some IP options */
			"SSysQ",	/* sent them a sysquery */
			"SAns",		/* sent them an answer */
			"SFwdQ",	/* fwdd a query to them */
			"SDupQ",	/* sent them a retry */
			"SErr",	        /* sent failed (in sendto) */
#ifdef XSTATS
			"RQ",		/* sent us a query */
			"RIQ",		/* sent us an inverse query */
			"RFwdQ",	/* sent us a query we had to fwd */
			"RDupQ",	/* sent us a retry */
			"RTCP",		/* sent us a query using TCP */
			"SFwdR",	/* fwdd a response to them */
			"SFail",	/* sent them a SERVFAIL */
			"SFErr",	/* sent them a FORMERR */
			"SNaAns",       /* sent them a non autoritative answer */
			"SNXD",         /* sent them a negative response */
#endif
			};
#endif /*STATS*/

static int
nameserCompar(t1, t2)
	const tree_t t1, t2;
{
	u_int32_t	a1 = ntohl(((struct nameser *)t1)->addr.s_addr),
			a2 = ntohl(((struct nameser *)t2)->addr.s_addr);

	if (a1 < a2)
		return (-1);
	else if (a1 > a2)
		return (1);
	else
		return (0);
}
	
struct nameser *
nameserFind(addr, flags)
	struct in_addr addr;
	int flags;
{
	struct nameser dummy;
	struct nameser *ns;

	if (!nameserInit) {
		tree_init(&nameserTree);
		nameserInit++;
	}

	dummy.addr = addr;
	ns = (struct nameser *)tree_srch(&nameserTree, nameserCompar,
					 (tree_t)&dummy);
	if (!ns && (flags & NS_F_INSERT)) {
		ns = (struct nameser *)malloc(sizeof(struct nameser));
		if (!ns) {
 nomem:			if (!haveComplained("nameserFind complaint", ""))
				syslog(LOG_NOTICE,
				       "nameserFind: malloc failed; %m");
			return (NULL);
		}
		memset(ns, 0, sizeof(struct nameser));
		ns->addr = addr;
		if (!tree_add(&nameserTree, nameserCompar, (tree_t)ns, NULL)) {
			int save = errno;
			free(ns);
			errno = save;
			goto nomem;
		}
	}
	return (ns);
}


void
nameserIncr(addr, which)
	struct in_addr addr;
	enum nameserStats which;
{
#ifdef STATS
	struct nameser *ns = nameserFind(addr, NS_F_INSERT);

	if ((int)which < (int)nssLast) {
		if (ns)
			ns->stats[(int)which]++;
		globalStats[(int)which]++;
	} else {
		syslog(LOG_DEBUG, "nameserIncr([%d], %d): bad 'which'",
		       inet_ntoa(addr), (int)which);
	}
#endif /*STATS*/
}

#ifdef STATS
static void
nameserStatsOut(f, stats)
	FILE *f;
	u_long stats[];
{
	int i;
	const char *pre = "\t";

	for (i = 0;  i < (int)nssLast;  i++) {
		fprintf(f, "%s%lu", pre, (u_long)stats[i]);
		pre = ((i+1) % 5) ? " " : "  ";
	}
	fputc('\n', f);
}

static void
nameserStatsHdr(f)
	FILE *f;
{
	int i;
	const char *pre = "\t";

	fprintf(f, "(Legend)\n");
	for (i = 0;  i < (int)nssLast;  i++) {
		fprintf(f, "%s%s", pre,
			statNames[i] ? statNames[i] : "");
		pre = ((i+1) % 5) ? "\t" : "\n\t";
	}
	fputc('\n', f);
}

static int
nameserStatsTravUAR(t)
	tree_t t;
{
	struct nameser *ns = (struct nameser *)t;

	fprintf(nameserStatsFile, "[%s]\n",	/* : rtt %u */
		inet_ntoa(ns->addr)  /*, ns->rtt*/ );
	nameserStatsOut(nameserStatsFile, ns->stats);
	return (1);
}
#endif /*STATS*/

static void
nameserStats(f)
	FILE *f;
{
#ifndef STATS
	fprintf(f, "<<No nameserver statistics in this server>>\n");
#else
	nameserStatsFile = f;
	fprintf(f, "++ Name Server Statistics ++\n");
	nameserStatsHdr(f);
	fprintf(f, "(Global)\n");
	nameserStatsOut(f, globalStats);
	tree_trav(&nameserTree, nameserStatsTravUAR);
	fprintf(f, "-- Name Server Statistics --\n");
	nameserStatsFile = NULL;
#endif /*STATS*/
}

#ifdef XSTATS
/* Benoit Grange, log minimal statistics, called from ns_maint */
void
ns_logstats()
{
	char buffer[1024];
	char buffer2[32], header[64];
	time_t timenow = time(NULL);
	int i;

#ifdef HAVE_GETRUSAGE
# define tv_float(tv) ((tv).tv_sec + ((tv).tv_usec / 1000000.0))
	struct rusage usage, childu;

	getrusage(RUSAGE_SELF, &usage);
	getrusage(RUSAGE_CHILDREN, &childu);

	sprintf(buffer, "CPU=%gu/%gs CHILDCPU=%gu/%gs",
		tv_float(usage.ru_utime), tv_float(usage.ru_stime),
		tv_float(childu.ru_utime), tv_float(childu.ru_stime));
	syslog(LOG_INFO, "USAGE %lu %lu %s", (u_long)timenow, (u_long)boottime,
		buffer);
# undef tv_float
#endif

	sprintf(header, "NSTATS %lu %lu", (u_long)timenow, (u_long)boottime);
	strcpy(buffer, header);

	for (i = 0; i < T_ANY+1; i++) {
		if (typestats[i]) {
			if (typenames[i])
				sprintf(buffer2, " %s=%lu",
					typenames[i], typestats[i]);
			else
				sprintf(buffer2, " %d=%lu", i, typestats[i]);
			if (strlen(buffer) + strlen(buffer2) >
			    sizeof(buffer) - 1) {
				syslog(LOG_INFO, buffer);
				strcpy(buffer, header);
			}
			strcat(buffer, buffer2);
		}
	}
	syslog(LOG_INFO, buffer);

	sprintf(header, "XSTATS %lu %lu", (u_long)timenow, (u_long)boottime);
	strcpy(buffer, header);
	for (i = 0;  i < (int)nssLast;  i++) {
		sprintf(buffer2, " %s=%lu",
			statNames[i]?statNames[i]:"?", (u_long)globalStats[i]);
		if (strlen(buffer) + strlen(buffer2) > sizeof(buffer) - 1) {
			syslog(LOG_INFO, buffer);
			strcpy(buffer, header);
		}
		strcat(buffer, buffer2);
	}
	syslog(LOG_INFO, buffer);
}

#endif /*XSTATS*/
