#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_stats.c	4.10 (Berkeley) 6/27/90";
static const char rcsid[] = "$Id: ns_stats.c,v 8.34 2001/08/09 13:17:21 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986
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
 */

/*
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
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/tree.h>

#include "port_after.h"

#ifdef HAVE_GETRUSAGE	/* XXX */
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "named.h"

static u_long		typestats[T_ANY+1];
static void		nameserStats(FILE *);
static u_int32_t	ns_stats_cnt = 0;
static int		ns_stats_disabled = 0;

void
ns_stats_dumpandclear() {
	time_t timenow = time(NULL);
	FILE *f;

	ns_stats();
	if (!(f = fopen(server_options->stats_filename, "a"))) {
		ns_notice(ns_log_statistics, "cannot open stat file, \"%s\"",
			  server_options->stats_filename);
	}
	if (f != NULL) {
		(void) fchown(fileno(f), user_id, group_id);
		fprintf(f, "+++ Host Statistics Cleared +++ (%ld) %s",
			(long)timenow, checked_ctime(&timenow));
		(void) my_fclose(f);
	}
	ns_freestats();
}

void
ns_stats() {
	time_t timenow = time(NULL);
	FILE *f;
	int i;

	ns_notice(ns_log_statistics, "dumping nameserver stats");

	if (!(f = fopen(server_options->stats_filename, "a"))) {
		ns_notice(ns_log_statistics, "cannot open stat file, \"%s\"",
			  server_options->stats_filename);
		return;
	}
	(void) fchown(fileno(f), user_id, group_id);

	fprintf(f, "+++ Statistics Dump +++ (%ld) %s",
		(long)timenow, checked_ctime(&timenow));
	fprintf(f, "%ld\ttime since boot (secs)\n",
		(long)(timenow - boottime));
	fprintf(f, "%ld\ttime since reset (secs)\n",
		(long)(timenow - resettime));

	/* query type statistics */
	fprintf(f, "%lu\tUnknown query types\n", (u_long)typestats[0]);
	for (i = 1; i < T_ANY+1; i++)
		fprintf(f, "%lu\t%s queries\n", typestats[i], p_type(i));

	/* name server statistics */
	nameserStats(f);

	fprintf(f, "--- Statistics Dump --- (%ld) %s",
		(long)timenow, checked_ctime(&timenow));
	(void) my_fclose(f);

	/* Now do the memory statistics file */
	if (!(f = fopen(server_options->memstats_filename, "a"))) {
		ns_notice(ns_log_statistics, "cannot open memstat file, \"%s\"",
			  server_options->memstats_filename);
		return;
	}
	(void) fchown(fileno(f), user_id, group_id);

	fprintf(f, "+++ Memory Statistics Dump +++ (%ld) %s",
		(long)timenow, checked_ctime(&timenow));

	fprintf(f, "%ld\ttime since boot (secs)\n",
		(long)(timenow - boottime));
	fprintf(f, "%ld\ttime since reset (secs)\n",
		(long)(timenow - resettime));

	fprintf(f, "++ Memory Statistics ++\n");
	memstats(f);
	fprintf(f, "-- Memory Statistics --\n");

	fprintf(f, "--- Memory Statistics Dump --- (%ld) %s",
		(long)timenow, checked_ctime(&timenow));
	(void) my_fclose(f);

	ns_notice(ns_log_statistics, "done dumping nameserver stats");
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

static FILE		*nameserStatsFile;
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
			"RUQ",		/* sent us an unapproved query */
			"RURQ",		/* sent us an unapproved recursive query */
			"RUXFR",	/* sent us an unapproved AXFR or IXFR */
			"RUUpd",	/* sent us an unapproved update */
			};

/*
 * Note that addresses in network byte order always have the high byte first.
 * XXX - this is horribly IPv4 dependent, but it's performance critical.
 */
static int
nameserCompar(const tree_t t1, const tree_t t2) {
	u_char *p1 = (u_char *)t1, *p2 = (u_char *)t2;
	int i;

	for (i = INADDRSZ; i > 0; i--) {
		u_char c1 = *p1++, c2 = *p2++;

		if (c1 < c2)
			return (-1);
		if (c1 > c2)
			return (1);
	}
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
		ns_stats_cnt = 0;
		ns_stats_disabled = 0;
	}

	dummy.addr = addr;
	ns = (struct nameser *)tree_srch(&nameserTree, nameserCompar,
					 (tree_t)&dummy);
	if (ns == NULL && (flags & NS_F_INSERT) != 0) {
		if (server_options->max_host_stats != 0 &&
		    ns_stats_cnt > server_options->max_host_stats) {
			if (!ns_stats_disabled)
				ns_notice(ns_log_statistics,
					  "ns_stats_disabled: %s reached",
					  "host-statistics-max");
			ns_stats_disabled = 1;
			return (NULL);
		}
		ns = (struct nameser *)memget(sizeof(struct nameser));
		if (ns == NULL) {
 nomem:			if (!haveComplained((u_long)nameserFind, 0))
				ns_notice(ns_log_statistics, 
					  "nameserFind: memget failed; %s",
					  strerror(errno));
			return (NULL);
		}
		memset(ns, 0, sizeof *ns);
		ns->addr = addr;
		if (!tree_add(&nameserTree, nameserCompar, (tree_t)ns, NULL)) {
			int save = errno;
			memput(ns, sizeof *ns);
			errno = save;
			goto nomem;
		}
		ns_stats_cnt++;
	}
	return (ns);
}

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

static void
nameserStats(f)
	FILE *f;
{
	nameserStatsFile = f;
	fprintf(f, "++ Name Server Statistics ++\n");
	nameserStatsHdr(f);
	fprintf(f, "(Global)\n");
	nameserStatsOut(f, globalStats);
	if (NS_OPTION_P(OPTION_HOSTSTATS)) {
		tree_trav(&nameserTree, nameserStatsTravUAR);
		if (ns_stats_disabled)
			fprintf(f, "++ Host Statistics Incomplete ++\n");
	}
	fprintf(f, "-- Name Server Statistics --\n");
	nameserStatsFile = NULL;
}

void
ns_logstats(evContext ctx, void *uap, struct timespec due,
	    struct timespec inter)
{
	char buffer[1024];
	char buffer2[32], header[128];
	time_t timenow = time(NULL);
	int i;
#ifdef HAVE_GETRUSAGE
	struct rusage usage, childu;
#endif /*HAVE_GETRUSAGE*/

#ifdef HAVE_GETRUSAGE
# define tv_float(tv) ((tv).tv_sec + ((tv).tv_usec / 1000000.0))

	UNUSED(ctx);
	UNUSED(uap);
	UNUSED(due);
	UNUSED(inter);

	getrusage(RUSAGE_SELF, &usage);
	getrusage(RUSAGE_CHILDREN, &childu);

	/*
	 * Get around a stupid compiler bug in gcc on solaris.
	 * There is a problem if three or more doubles are passed to
	 * sprintf.
	 * <http://gcc.gnu.org/cgi-bin/gnatsweb.pl?cmd=view&pr=337&database=gcc>
	 */
	sprintf(buffer, "CPU=%gu/%gs CHILDCPU=", tv_float(usage.ru_utime),
		tv_float(usage.ru_stime));
	sprintf(header, "%gu/%gs", tv_float(childu.ru_utime),
		tv_float(childu.ru_stime));
	ns_info(ns_log_statistics, "USAGE %lu %lu %s%s", (u_long)timenow,
		(u_long)boottime, buffer, header);
# undef tv_float
#endif /*HAVE_GETRUSAGE*/

	sprintf(header, "NSTATS %lu %lu", (u_long)timenow, (u_long)boottime);
	strcpy(buffer, header);

	for (i = 0; i < T_ANY+1; i++) {
		if (typestats[i]) {
			sprintf(buffer2, " %s=%lu", p_type(i), typestats[i]);
			if (strlen(buffer) + strlen(buffer2) >
			    sizeof(buffer) - 1) {
				ns_info(ns_log_statistics, "%s", buffer);
				strcpy(buffer, header);
			}
			strcat(buffer, buffer2);
		}
	}
	ns_info(ns_log_statistics, "%s", buffer);

	sprintf(header, "XSTATS %lu %lu", (u_long)timenow, (u_long)boottime);
	strcpy(buffer, header);
	for (i = 0;  i < (int)nssLast;  i++) {
		sprintf(buffer2, " %s=%lu",
			statNames[i]?statNames[i]:"?", (u_long)globalStats[i]);
		if (strlen(buffer) + strlen(buffer2) > sizeof(buffer) - 1) {
			ns_info(ns_log_statistics, "%s", buffer);
			strcpy(buffer, header);
		}
		strcat(buffer, buffer2);
	}
	ns_info(ns_log_statistics, "%s", buffer);
}

static void
nameserFree(void *uap) {
	struct nameser *ns = uap;

	memput(ns, sizeof *ns);
}

void
ns_freestats(void) {
	if (nameserTree == NULL)
		return;
	tree_mung(&nameserTree, nameserFree);
	nameserInit = 0;
	ns_stats_cnt = 0;
	ns_stats_disabled = 0;
}
