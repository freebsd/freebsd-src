/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6sd/debug.c,v 1.1.2.1 2000/07/15 07:36:35 kris Exp $
 */

#include <stdio.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip_mroute.h>
#include <netinet/icmp6.h>
#include <netinet6/pim6.h>
#include "pathnames.h"
#include "defs.h"
#include "pimd.h"
#include "debug.h"
#include "mrt.h"
#include "vif.h"
#include "rp.h"
#include "inet6.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern char    *progname;

int             log_nmsgs = 0;
unsigned long   debug = 0x00000000;	/* If (long) is smaller than 4 bytes,
					 * then we are in trouble. */
static char     dumpfilename[] = _PATH_PIM6D_DUMP;
static char     cachefilename[] = _PATH_PIM6D_CACHE;	/* TODO: notused */
static char	statfilename[] = _PATH_PIM6D_STAT;

static char *sec2str __P((time_t));

static char *
sec2str(total)
	time_t total;
{
	static char result[256];
	int days, hours, mins, secs;
	int first = 1;
	char *p = result;

	days = total / 3600 / 24;
	hours = (total / 3600) % 24;
	mins = (total / 60) % 60;
	secs = total % 60;

	if (days) {
		first = 0;
		p += sprintf(p, "%dd", days);
	}
	if (!first || hours) {
		first = 0;
		p += sprintf(p, "%dh", hours);
	}
	if (!first || mins) {
		first = 0;
		p += sprintf(p, "%dm", mins);
	}
	sprintf(p, "%ds", secs);

	return(result);
}

char           *
packet_kind(proto, type, code)
    u_int           proto,
                    type,
                    code;
{
    static char     unknown[60];

    switch (proto)
    {
    case IPPROTO_ICMPV6:
	switch (type)
	{
	case MLD6_LISTENER_QUERY:
	    return "Multicast Listener Query    ";
	case MLD6_LISTENER_REPORT:
	    return "Multicast Listener Report   ";
	case MLD6_LISTENER_DONE:
	    return "Multicast Listener Done     ";
	default:
	    sprintf(unknown,
		    "UNKNOWN ICMPv6 message: type = 0x%02x, code = 0x%02x ",
		    type, code);
	    return unknown;
	}

    case IPPROTO_PIM:		/* PIM v2 */
	switch (type)
	{
	case PIM_V2_HELLO:
	    return "PIM v2 Hello             ";
	case PIM_V2_REGISTER:
	    return "PIM v2 Register          ";
	case PIM_V2_REGISTER_STOP:
	    return "PIM v2 Register_Stop     ";
	case PIM_V2_JOIN_PRUNE:
	    return "PIM v2 Join/Prune        ";
	case PIM_V2_BOOTSTRAP:
	    return "PIM v2 Bootstrap         ";
	case PIM_V2_ASSERT:
	    return "PIM v2 Assert            ";
	case PIM_V2_GRAFT:
	    return "PIM-DM v2 Graft          ";
	case PIM_V2_GRAFT_ACK:
	    return "PIM-DM v2 Graft_Ack      ";
	case PIM_V2_CAND_RP_ADV:
	    return "PIM v2 Cand. RP Adv.     ";
	default:
	    sprintf(unknown, "UNKNOWN PIM v2 message type =%3d ", type);
	    return unknown;
	}
    default:
	sprintf(unknown, "UNKNOWN proto =%3d               ", proto);
	return unknown;
    }
}


/*
 * Used for debugging particular type of messages.
 */
int
debug_kind(proto, type, code)
    u_int           proto,
                    type,
                    code;
{
    switch (proto)
    {
    case IPPROTO_ICMPV6:
	switch (type)
	{
	case MLD6_LISTENER_QUERY:
	    return DEBUG_MLD;
	case MLD6_LISTENER_REPORT:
	    return DEBUG_MLD;
	case MLD6_LISTENER_DONE:
	    return DEBUG_MLD;
	default:
	    return DEBUG_MLD;
	}
    case IPPROTO_PIM:		/* PIM v2 */
	/* TODO: modify? */
	switch (type)
	{
	case PIM_V2_HELLO:
	    return DEBUG_PIM;
	case PIM_V2_REGISTER:
	    return DEBUG_PIM_REGISTER;
	case PIM_V2_REGISTER_STOP:
	    return DEBUG_PIM_REGISTER;
	case PIM_V2_JOIN_PRUNE:
	    return DEBUG_PIM;
	case PIM_V2_BOOTSTRAP:
	    return DEBUG_PIM_BOOTSTRAP;
	case PIM_V2_ASSERT:
	    return DEBUG_PIM;
	case PIM_V2_GRAFT:
	    return DEBUG_PIM;
	case PIM_V2_GRAFT_ACK:
	    return DEBUG_PIM;
	case PIM_V2_CAND_RP_ADV:
	    return DEBUG_PIM_CAND_RP;
	default:
	    return DEBUG_PIM;
	}
    default:
	return 0;
    }
    return 0;
}


/*
 * Some messages are more important than others.  This routine determines the
 * logging level at which to log a send error (often "No route to host").
 * This is important when there is asymmetric reachability and someone is
 * trying to, i.e., mrinfo me periodically.
 */
int
log_level(proto, type, code)
    u_int           proto,
                    type,
                    code;
{
    switch (proto)
    {
    case IPPROTO_ICMPV6:
	switch (type)
	{
    	default:
	    return LOG_WARNING;
    	}

    case IPPROTO_PIM:
    /* PIM v2 */
    	switch (type)
    	{
    	default:
	    return LOG_INFO;
    	}
    default:
    	return LOG_WARNING;
    }

    return LOG_WARNING;
}


/*
 * Dump internal data structures to stderr.
 */
/*
 * TODO: currently not used void dump(int i) { dump_vifs(stderr);
 * dump_pim_mrt(stderr); }
 */

/*
 * Dump internal data structures to a file.
 */
void
fdump(i)
    int             i;
{
    FILE           *fp;
    fp = fopen(dumpfilename, "w");
    if (fp != NULL)
    {
	dump_vifs(fp);
	dump_nbrs(fp);
	dump_mldqueriers(fp);
	dump_pim_mrt(fp);
	dump_rp_set(fp);
	(void) fclose(fp);
    }
}

/* TODO: dummy, to be used in the future. */
/*
 * Dump local cache contents to a file.
 */
void
cdump(i)
    int             i;
{
    FILE           *fp;

    fp = fopen(cachefilename, "w");
    if (fp != NULL)
    {
	/*
	 * TODO: implement it: dump_cache(fp);
	 */
	(void) fclose(fp);
    }
}

void
dump_stat()
{
	FILE *fp;
	vifi_t vifi;
	register struct uvif *v;

	fp = fopen(statfilename, "w");
	if (fp == NULL) {
		log(LOG_WARNING, errno, "dump_stat: can't open file(%s)",
		    statfilename);
		return;
	}

	fprintf(fp, "pim6sd per-interface statistics\n");
	for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++) {
#if 0				/* is it better to skip them? */
		if ((v->uv_flags & (VIFF_DISABLED|VIFF_DOWN)) != 0)
			continue;
#endif
		fprintf(fp, " Mif=%d, PhyIF=%s\n", vifi, v->uv_name);
		fprintf(fp, "\t%qu pim6 hello received\n",
			(unsigned long long)v->uv_in_pim6_hello);
		fprintf(fp, "\t%qu pim6 join-prune received\n",
			(unsigned long long)v->uv_in_pim6_join_prune);
		fprintf(fp, "\t%qu pim6 bootstrap received\n",
			(unsigned long long)v->uv_in_pim6_bootsrap);
		fprintf(fp, "\t%qu pim6 assert received\n",
			(unsigned long long)v->uv_in_pim6_assert);

		fprintf(fp, "\t%qu pim6 hello sent\n",
			(unsigned long long)v->uv_out_pim6_hello);
		fprintf(fp, "\t%qu pim6 join-prune sent\n",
			(unsigned long long)v->uv_out_pim6_join_prune);
		fprintf(fp, "\t%qu pim6 bootstrap sent\n",
			(unsigned long long)v->uv_out_pim6_bootsrap);
		fprintf(fp, "\t%qu pim6 assert sent\n",
			(unsigned long long)v->uv_out_pim6_assert);

		fprintf(fp, "\t%qu MLD query received\n",
			(unsigned long long)v->uv_in_mld_query);
		fprintf(fp, "\t%qu MLD report received\n",
			(unsigned long long)v->uv_in_mld_report);
		fprintf(fp, "\t%qu MLD done received\n",
			(unsigned long long)v->uv_in_mld_done);

		fprintf(fp, "\t%qu MLD query sent\n",
			(unsigned long long)v->uv_out_mld_query);
		fprintf(fp, "\t%qu MLD report sent\n",
			(unsigned long long)v->uv_out_mld_report);
		fprintf(fp, "\t%qu MLD done sent\n",
			(unsigned long long)v->uv_out_mld_done);

		fprintf(fp, "\t%qu forwarding cache miss\n",
			(unsigned long long)v->uv_cache_miss);
		fprintf(fp, "\t%qu forwarding cache miss and not created\n",
			(unsigned long long)v->uv_cache_notcreated);

		fprintf(fp, "\t%qu PIM neighbor timeouts\n",
			(unsigned long long)v->uv_pim6_nbr_timo);
		fprintf(fp, "\t%qu MLD listener timeouts\n",
			(unsigned long long)v->uv_listener_timo);
		fprintf(fp, "\t%qu MLD querier timeouts\n",
			(unsigned long long)v->uv_querier_timo);
		fprintf(fp, "\t%qu out-I/F timeouts\n",
			(unsigned long long)v->uv_outif_timo);
	}

	fprintf(fp, "\npim6sd interface independent statistics\n");

	fprintf(fp, "\t%qu pim6 register received\n",
		(unsigned long long)pim6dstat.in_pim6_register);
	fprintf(fp, "\t%qu pim6 register-stop received\n",
		(unsigned long long)pim6dstat.in_pim6_register_stop);
	fprintf(fp, "\t%qu pim6 cand-RP received\n",
		(unsigned long long)pim6dstat.in_pim6_cand_rp);
	fprintf(fp, "\t%qu pim6 graft received\n",
		(unsigned long long)pim6dstat.in_pim6_graft);
	fprintf(fp, "\t%qu pim6 graft ack received\n",
		(unsigned long long)pim6dstat.in_pim6_graft_ack);

	fprintf(fp, "\t%qu pim6 register sent\n",
		(unsigned long long)pim6dstat.out_pim6_register);
	fprintf(fp, "\t%qu pim6 register-stop sent\n",
		(unsigned long long)pim6dstat.out_pim6_register_stop);
	fprintf(fp, "\t%qu pim6 cand-RP sent\n",
		(unsigned long long)pim6dstat.out_pim6_cand_rp);

	fprintf(fp, "\t%qu transitions of forwarder initiated SPT\n",
		(unsigned long long)pim6dstat.pim6_trans_spt_forward);
	fprintf(fp, "\t%qu transitions of RP initiated SPT\n",
		(unsigned long long)pim6dstat.pim6_trans_spt_rp);

	fprintf(fp, "\t%qu pim6 bootstrap timeouts\n",
		(unsigned long long)pim6dstat.pim6_bootstrap_timo);
	fprintf(fp, "\t%qu pim6 RP group entry timeouts\n",
		(unsigned long long)pim6dstat.pim6_rpgrp_timo);
	fprintf(fp, "\t%qu pim6 routing entry timeouts\n",
		(unsigned long long)pim6dstat.pim6_rtentry_timo);

	fprintf(fp, "\t%qu kernel cache additions\n",
		(unsigned long long)pim6dstat.kern_add_cache);
	fprintf(fp, "\t%qu kernel cache addition failures\n",
		(unsigned long long)pim6dstat.kern_add_cache_fail);
	fprintf(fp, "\t%qu kernel cache deletions\n",
		(unsigned long long)pim6dstat.kern_del_cache);
	fprintf(fp, "\t%qu kernel cache deletion failures\n",
		(unsigned long long)pim6dstat.kern_del_cache_fail);
	fprintf(fp, "\t%qu failures of getting kernel cache\n",
		(unsigned long long)pim6dstat.kern_sgcnt_fail);

	fclose(fp);
}

void
dump_vifs(fp)
    FILE           *fp;
{
    vifi_t          vifi;
    register struct uvif *v;
    struct phaddr  *pa;

    fprintf(fp, "\nMulticast Interface Table\n %-4s %-6s %-43s %5s %-14s\n",
	    "Mif", " PhyIF", "Local-Address/Prefixlen","Scope", "Flags");

    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v)
    {
	int firstaddr = 1;
	for (pa = v->uv_addrs; pa; pa = pa->pa_next)
	{
	    if (!firstaddr)
	    {
		fprintf(fp, "  %3s %6s %-43s", "", "",
			net6name(&pa->pa_addr.sin6_addr,
				 &pa->pa_subnetmask));
	    	fprintf(fp," %-5d\n", pa->pa_addr.sin6_scope_id);
		continue;
	    }

	    firstaddr = 0;
	    fprintf(fp, "  %-3u %6s %-43s", vifi,
		    (v->uv_flags & MIFF_REGISTER)?"regist":v->uv_name,
		    net6name(&pa->pa_addr.sin6_addr,
			     &pa->pa_subnetmask));
	    fprintf(fp," %-5d", pa->pa_addr.sin6_scope_id);

	    if (v->uv_flags & MIFF_REGISTER)
		fprintf(fp, " REGISTER");
	    if (v->uv_flags & VIFF_DISABLED)
		fprintf(fp, " DISABLED");
	    if (v->uv_flags & VIFF_NOLISTENER)
		fprintf(fp, " NOLISTENER");
	    if (v->uv_flags & VIFF_DOWN)
		fprintf(fp, " DOWN");
	    if (v->uv_flags & VIFF_DR)
		fprintf(fp, " DR");
	    if (v->uv_flags & VIFF_PIM_NBR)
		fprintf(fp, " PIM");
	    if (v->uv_flags & VIFF_QUERIER)
		fprintf(fp, " QRY");
#if 0				/* impossible */
	    if (v->uv_flags & VIFF_DVMRP_NBR)
	    {
		fprintf(fp, " DVMRP");
	    }
#endif
	    if (v->uv_flags & VIFF_NONBRS)
		fprintf(fp, " NO-NBR");

	    fprintf(fp, "\n");
	}

	fprintf(fp, "  %3s %6s ", "", "");
	fprintf(fp, "Timers: PIM hello = %d:%02d, MLD query = %d:%02d\n",
		v->uv_pim_hello_timer / 60, v->uv_pim_hello_timer % 60,
		v->uv_gq_timer / 60, v->uv_gq_timer % 60);
    }
    fprintf(fp, "\n");
}

void
dump_nbrs(fp)
	FILE *fp;
{
	struct uvif *v;
	vifi_t vifi;
	pim_nbr_entry_t *n;

	fprintf(fp, "PIM Neighbor List\n");
	fprintf(fp, " %-3s %6s %-40s %-5s\n",
		"Mif", "PhyIF", "Address", "Timer");

	for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
		if ((n = v->uv_pim_neighbors) != NULL) {
			int first = 1;

			fprintf(fp, " %-3u %6s", vifi,
				(v->uv_flags & MIFF_REGISTER) ? "regist":
				v->uv_name);
			for (; n != NULL; n = n->next) {
				if (first)
					first = 0;
				else
					fprintf(fp, " %3s %6s", "", "");
				fprintf(fp, " %-40s %-5u\n",
					inet6_fmt(&n->address.sin6_addr),
					n->timer);
			}
		}
	}

	fprintf(fp, "\n");
}

void
dump_mldqueriers(fp)
	FILE *fp;
{
	struct uvif *v;
	vifi_t vifi;
	time_t now;

	fprintf(fp, "MLD Querier List\n");
	fprintf(fp, " %-3s %6s %-40s %-5s %15s\n",
		"Mif", "PhyIF", "Address", "Timer", "Last");
	(void)time(&now);

	for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
		if (v->uv_querier) {
			fprintf(fp, " %-3u %6s", vifi,
				(v->uv_flags & MIFF_REGISTER) ? "regist":
				v->uv_name);

			fprintf(fp, " %-40s %5lu %15s\n",
				sa6_fmt(&v->uv_querier->al_addr),
				(u_long)v->uv_querier->al_timer,
				sec2str(now - v->uv_querier->al_ctime));
		}
	}

	fprintf(fp, "\n");
} 

/*
 * Log errors and other messages to the system log daemon and to stderr,
 * according to the severity of the message and the current debug level. For
 * errors of severity LOG_ERR or worse, terminate the program.
 */
#ifdef __STDC__
void
log(int severity, int syserr, char *format,...)
{
    va_list         ap;
    static char     fmt[211] = "warning - ";
    char           *msg;
    struct timeval  now;
    struct tm      *thyme;

    va_start(ap, format);
#else
/* VARARGS3 */
void
log(severity, syserr, format, va_alist)
    int             severity,
                    syserr;
    char           *format;
va_dcl
{
    va_list         ap;
    static char     fmt[311] = "warning - ";
    char           *msg;
    char            tbuf[20];
    struct timeval  now;
    struct tm      *thyme;

    va_start(ap);
#endif
    vsprintf(&fmt[10], format, ap);
    va_end(ap);
    msg = (severity == LOG_WARNING) ? fmt : &fmt[10];

    /*
     * Log to stderr if we haven't forked yet and it's a warning or worse, or
     * if we're debugging.
     */
    if (debug || severity <= LOG_WARNING)
    {
	time_t t;
	FILE *fp = log_fp ? log_fp : stderr;

	gettimeofday(&now, NULL);
	t = (time_t)now.tv_sec;
	thyme = localtime(&t);
	if (!debug)
	    fprintf(fp, "%s: ", progname);
	fprintf(fp, "%02d:%02d:%02d.%03ld %s", thyme->tm_hour,
		thyme->tm_min, thyme->tm_sec, (long int)now.tv_usec / 1000,
		msg);
	if (syserr == 0)
	    fprintf(fp, "\n");
	else
	    if (syserr < sys_nerr)
		fprintf(fp, ": %s\n", sys_errlist[syserr]);
	    else
		fprintf(fp, ": errno %d\n", syserr);
    }

    /*
     * Always log things that are worse than warnings, no matter what the
     * log_nmsgs rate limiter says. Only count things worse than debugging in
     * the rate limiter (since if you put daemon.debug in syslog.conf you
     * probably actually want to log the debugging messages so they shouldn't
     * be rate-limited)
     */
    if ((severity < LOG_WARNING) || (log_nmsgs < LOG_MAX_MSGS))
    {
	if (severity < LOG_DEBUG)
	    log_nmsgs++;
	if (syserr != 0)
	{
	    errno = syserr;
	    syslog(severity, "%s: %m", msg);
	}
	else
	    syslog(severity, "%s", msg);
    }

    if (severity <= LOG_ERR)
	exit(-1);
}

/* TODO: format the output for better readability */
void
dump_pim_mrt(fp)
    FILE           *fp;
{
    grpentry_t     *g;
    register mrtentry_t *r;
    register vifi_t vifi;
    int i;
    u_int           number_of_cache_mirrors = 0;
    u_int           number_of_groups = 0;
    char            joined_oifs[(sizeof(if_set) << 3) + 1];
    char            asserted_oifs[(sizeof(if_set) << 3) + 1];
    cand_rp_t      *rp;
    kernel_cache_t *kernel_cache;
    char            oifs[(sizeof(if_set) << 3) + 1];
    char            pruned_oifs[(sizeof(if_set) << 3) + 1];
    char            leaves_oifs[(sizeof(if_set) << 3) + 1];
    char            incoming_iif[(sizeof(if_set) << 3) + 1];

    fprintf(fp, "Multicast Routing Table\n%s",
	    " Source          Group           RP-addr         Flags\n");

    /* TODO: remove the dummy 0:: group (first in the chain) */
    for (g = grplist->next; g != (grpentry_t *) NULL; g = g->next)
    {
	number_of_groups++;
	if ((r = g->grp_route) != (mrtentry_t *) NULL)
	{
	    if (r->flags & MRTF_KERNEL_CACHE)
	    {
		for (kernel_cache = r->kernel_cache;
		     kernel_cache != (kernel_cache_t *) NULL;
		     kernel_cache = kernel_cache->next)
		    number_of_cache_mirrors++;
	    }

	    /* Print the (*,G) routing info */
	    fprintf(fp, "---------------------------(*,G)----------------------------\n");
	    fprintf(fp, " %-15s", "IN6ADDR_ANY");
	    fprintf(fp, " %-15s", inet6_fmt(&g->group.sin6_addr));
	    fprintf(fp, " %-15s",
	    g->active_rp_grp ? inet6_fmt(&g->rpaddr.sin6_addr) : "NULL");

	    for (vifi = 0; vifi < numvifs; vifi++)
	    {
		oifs[vifi] =
		    IF_ISSET(vifi, &r->oifs) ? 'o' : '.';
		joined_oifs[vifi] =
		    IF_ISSET(vifi, &r->joined_oifs) ? 'j' : '.';
		pruned_oifs[vifi] =
		    IF_ISSET(vifi, &r->pruned_oifs) ? 'p' : '.';
		leaves_oifs[vifi] =
		    IF_ISSET(vifi, &r->leaves) ? 'l' : '.';
		asserted_oifs[vifi] =
		    IF_ISSET(vifi, &r->asserted_oifs) ? 'a' : '.';
		incoming_iif[vifi] = '.';
	    }
	    oifs[vifi] = 0x0;	/* End of string */
	    joined_oifs[vifi] = 0x0;
	    pruned_oifs[vifi] = 0x0;
	    leaves_oifs[vifi] = 0x0;
	    asserted_oifs[vifi] = 0x0;
	    incoming_iif[vifi] = 0x0;
	    incoming_iif[r->incoming] = 'I';

	    /* TODO: don't need some of the flags */
	    if (r->flags & MRTF_SPT)
		fprintf(fp, " SPT");
	    if (r->flags & MRTF_WC)
		fprintf(fp, " WC");
	    if (r->flags & MRTF_RP)
		fprintf(fp, " RP");
	    if (r->flags & MRTF_REGISTER)
		fprintf(fp, " REG");
	    if (r->flags & MRTF_IIF_REGISTER)
		fprintf(fp, " IIF_REG");
	    if (r->flags & MRTF_NULL_OIF)
		fprintf(fp, " NULL_OIF");
	    if (r->flags & MRTF_KERNEL_CACHE)
		fprintf(fp, " CACHE");
	    if (r->flags & MRTF_ASSERTED)
		fprintf(fp, " ASSERTED");
	    if (r->flags & MRTF_REG_SUPP)
		fprintf(fp, " REG_SUPP");
	    if (r->flags & MRTF_SG)
		fprintf(fp, " SG");
	    if (r->flags & MRTF_PMBR)
		fprintf(fp, " PMBR");
	    fprintf(fp, "\n");

	    fprintf(fp, "Joined   oifs: %-20s\n", joined_oifs);
	    fprintf(fp, "Pruned   oifs: %-20s\n", pruned_oifs);
	    fprintf(fp, "Leaves   oifs: %-20s\n", leaves_oifs);
	    fprintf(fp, "Asserted oifs: %-20s\n", asserted_oifs);
	    fprintf(fp, "Outgoing oifs: %-20s\n", oifs);
	    fprintf(fp, "Incoming     : %-20s\n", incoming_iif);
	
	    fprintf(fp, "Upstream nbr: %s\n",
		    r->upstream ? inet6_fmt(&r->upstream->address.sin6_addr) : "NONE");

	    fprintf(fp, "\nTIMERS: Entry=%d JP=%d RS=%d Assert=%d\n",
		    r->timer, r->jp_timer, r->rs_timer, r->assert_timer);

	    fprintf(fp, "  MIF   0   1   2   3   4   5   6   7   8   9\n");
	    for (vifi = 0, i = 0; vifi < numvifs && i <= numvifs / 10; i++) {
		    int j;

		    fprintf(fp, " %4d", i);
		    for (j = 0; j < 10 && vifi < numvifs; j++, vifi++)
			    fprintf(fp, " %3d", r->vif_timers[vifi]);
		    fprintf(fp, "\n");
	    }
	}

	/* Print all (S,G) routing info */

	for (r = g->mrtlink; r != (mrtentry_t *) NULL; r = r->grpnext)
	{
	    fprintf(fp, "---------------------------(S,G)----------------------------\n");
	    if (r->flags & MRTF_KERNEL_CACHE)
		number_of_cache_mirrors++;

	    /* Print the routing info */
	    fprintf(fp, " %-15s", inet6_fmt(&r->source->address.sin6_addr));
	    fprintf(fp, " %-15s", inet6_fmt(&g->group.sin6_addr));
	    fprintf(fp, " %-15s",
	       g->active_rp_grp ? inet6_fmt(&g->rpaddr.sin6_addr) : "NULL");

	    for (vifi = 0; vifi < numvifs; vifi++)
	    {
		oifs[vifi] =
		IF_ISSET(vifi, &r->oifs) ? 'o' : '.';
		    joined_oifs[vifi] =
		IF_ISSET(vifi, &r->joined_oifs) ? 'j' : '.';
			pruned_oifs[vifi] =
		IF_ISSET(vifi, &r->pruned_oifs) ? 'p' : '.';
		leaves_oifs[vifi] =
		IF_ISSET(vifi, &r->leaves) ? 'l' : '.';
		    asserted_oifs[vifi] =
		IF_ISSET(vifi, &r->asserted_oifs) ? 'a' : '.';
		incoming_iif[vifi] = '.';
	    }
	    oifs[vifi] = 0x0;	/* End of string */
	    joined_oifs[vifi] = 0x0;
	    pruned_oifs[vifi] = 0x0;
	    leaves_oifs[vifi] = 0x0;
	    asserted_oifs[vifi] = 0x0;
	    incoming_iif[vifi] = 0x0;
	    incoming_iif[r->incoming] = 'I';

	    /* TODO: don't need some of the flags */
	    if (r->flags & MRTF_SPT)
		fprintf(fp, " SPT");
	    if (r->flags & MRTF_WC)
		fprintf(fp, " WC");
	    if (r->flags & MRTF_RP)
		fprintf(fp, " RP");
	    if (r->flags & MRTF_REGISTER)
		fprintf(fp, " REG");
	    if (r->flags & MRTF_IIF_REGISTER)
		fprintf(fp, " IIF_REG");
	    if (r->flags & MRTF_NULL_OIF)
		fprintf(fp, " NULL_OIF");
	    if (r->flags & MRTF_KERNEL_CACHE)
		fprintf(fp, " CACHE");
	    if (r->flags & MRTF_ASSERTED)
		fprintf(fp, " ASSERTED");
	    if (r->flags & MRTF_REG_SUPP)
		fprintf(fp, " REG_SUPP");
	    if (r->flags & MRTF_SG)
		fprintf(fp, " SG");
	    if (r->flags & MRTF_PMBR)
		fprintf(fp, " PMBR");
	    fprintf(fp, "\n");

	    fprintf(fp, "Joined   oifs: %-20s\n", joined_oifs);
	    fprintf(fp, "Pruned   oifs: %-20s\n", pruned_oifs);
	    fprintf(fp, "Leaves   oifs: %-20s\n", leaves_oifs);
	    fprintf(fp, "Asserted oifs: %-20s\n", asserted_oifs);
	    fprintf(fp, "Outgoing oifs: %-20s\n", oifs);
	    fprintf(fp, "Incoming     : %-20s\n", incoming_iif);

	    fprintf(fp, "Upstream nbr: %s\n",
		    r->upstream ? inet6_fmt(&r->upstream->address.sin6_addr) : "NONE");

	    fprintf(fp, "\nTIMERS: Entry=%d JP=%d RS=%d Assert=%d\n",
		    r->timer, r->jp_timer, r->rs_timer, r->assert_timer);

	    fprintf(fp, "  MIF   0   1   2   3   4   5   6   7   8   9\n");
	    for (vifi = 0, i = 0; vifi < numvifs && i <= numvifs / 10; i++) {
		    int j;

		    fprintf(fp, " %4d", i);
		    for (j = 0; j < 10 && vifi < numvifs; j++, vifi++)
			    fprintf(fp, " %3d", r->vif_timers[vifi]);
		    fprintf(fp, "\n");
	    }
	}
    }				/* for all groups */

    /* Print the (*,*,R) routing entries */
    fprintf(fp, "--------------------------(*,*,RP)--------------------------\n");
    for (rp = cand_rp_list; rp != (cand_rp_t *) NULL; rp = rp->next)
    {
	if ((r = rp->rpentry->mrtlink) != (mrtentry_t *) NULL)
	{
	    if (r->flags & MRTF_KERNEL_CACHE)
	    {
		for (kernel_cache = r->kernel_cache;
		     kernel_cache != (kernel_cache_t *) NULL;
		     kernel_cache = kernel_cache->next)
		    number_of_cache_mirrors++;
	    }

	    /* Print the (*,*,RP) routing info */
	    fprintf(fp, " RP = %-15s", inet6_fmt(&r->source->address.sin6_addr));
	    fprintf(fp, " %-15s", "IN6ADDR_ANY");

	    for (vifi = 0; vifi < numvifs; vifi++)
	    {
		oifs[vifi] =
		    IF_ISSET(vifi, &r->oifs) ? 'o' : '.';
		joined_oifs[vifi] =
		    IF_ISSET(vifi, &r->joined_oifs) ? 'j' : '.';
		pruned_oifs[vifi] =
		    IF_ISSET(vifi, &r->pruned_oifs) ? 'p' : '.';
		leaves_oifs[vifi] =
		    IF_ISSET(vifi, &r->leaves) ? 'l' : '.';
		asserted_oifs[vifi] =
		    IF_ISSET(vifi, &r->asserted_oifs) ? 'a' : '.';
		incoming_iif[vifi] = '.';
	    }
	    oifs[vifi] = 0x0;	/* End of string */
	    joined_oifs[vifi] = 0x0;
	    pruned_oifs[vifi] = 0x0;
	    leaves_oifs[vifi] = 0x0;
	    asserted_oifs[vifi] = 0x0;
	    incoming_iif[vifi] = 0x0;
	    incoming_iif[r->incoming] = 'I';

	    /* TODO: don't need some of the flags */
	    if (r->flags & MRTF_SPT)
		fprintf(fp, " SPT");
	    if (r->flags & MRTF_WC)
		fprintf(fp, " WC");
	    if (r->flags & MRTF_RP)
		fprintf(fp, " RP");
	    if (r->flags & MRTF_REGISTER)
		fprintf(fp, " REG");
	    if (r->flags & MRTF_IIF_REGISTER)
		fprintf(fp, " IIF_REG");
	    if (r->flags & MRTF_NULL_OIF)
		fprintf(fp, " NULL_OIF");
	    if (r->flags & MRTF_KERNEL_CACHE)
		fprintf(fp, " CACHE");
	    if (r->flags & MRTF_ASSERTED)
		fprintf(fp, " ASSERTED");
	    if (r->flags & MRTF_REG_SUPP)
		fprintf(fp, " REG_SUPP");
	    if (r->flags & MRTF_SG)
		fprintf(fp, " SG");
	    if (r->flags & MRTF_PMBR)
		fprintf(fp, " PMBR");
	    fprintf(fp, "\n");

	    fprintf(fp, "Joined   oifs: %-20s\n", joined_oifs);
	    fprintf(fp, "Pruned   oifs: %-20s\n", pruned_oifs);
	    fprintf(fp, "Leaves   oifs: %-20s\n", leaves_oifs);
	    fprintf(fp, "Asserted oifs: %-20s\n", asserted_oifs);
	    fprintf(fp, "Outgoing oifs: %-20s\n", oifs);
	    fprintf(fp, "Incoming     : %-20s\n", incoming_iif);

	    fprintf(fp, "\nTIMERS: Entry=%d JP=%d RS=%d Assert=%d\n",
		    r->timer, r->jp_timer, r->rs_timer, r->assert_timer);

	    fprintf(fp, "  MIF   0   1   2   3   4   5   6   7   8   9\n");
	    for (vifi = 0, i = 0; vifi < numvifs && i <= numvifs / 10; i++) {
		    int j;

		    fprintf(fp, " %4d", i);
		    for (j = 0; j < 10 && vifi < numvifs; j++, vifi++)
			    fprintf(fp, " %3d", r->vif_timers[vifi]);
		    fprintf(fp, "\n");
	    }
	}
    }				/* For all (*,*,RP) */

    fprintf(fp, "Number of Groups: %u\n", number_of_groups);
    fprintf(fp, "Number of Cache MIRRORs: %u\n\n", number_of_cache_mirrors);
}


/* TODO: modify the output for better redability */
/*
 * Dumps the local Cand-RP-set
 */
int
dump_rp_set(fp)
    FILE           *fp;
{
    cand_rp_t      *rp;
    rp_grp_entry_t *rp_grp_entry;
    grp_mask_t     *grp_mask;

    fprintf(fp, "---------------------------RP-Set----------------------------\n");
    fprintf(fp, "Current BSR address: %s Prio: %d Timeout: %d\n",
	    inet6_fmt(&curr_bsr_address.sin6_addr), curr_bsr_priority,
	    pim_bootstrap_timer);
    fprintf(fp, "%-40s %-3s Group prefix     Prio Hold Age\n",
	    "RP-address", "IN");

    for (rp = cand_rp_list; rp != (cand_rp_t *) NULL; rp = rp->next)
    {

	fprintf(fp, "%-40s %-3d ",
		inet6_fmt(&rp->rpentry->address.sin6_addr),
		rp->rpentry->incoming);
	if ((rp_grp_entry = rp->rp_grp_next) != (rp_grp_entry_t *) NULL)
	{
	    grp_mask = rp_grp_entry->group;
	    fprintf(fp, "%-16.16s %-4u %-4u %-3u\n",
		    net6name(&grp_mask->group_addr.sin6_addr,
			     &grp_mask->group_mask),
		    rp_grp_entry->priority, rp_grp_entry->advholdtime,
		    rp_grp_entry->holdtime);

	    for (rp_grp_entry = rp_grp_entry->rp_grp_next;
		 rp_grp_entry != (rp_grp_entry_t *) NULL;
		 rp_grp_entry = rp_grp_entry->rp_grp_next)
	    {
		grp_mask = rp_grp_entry->group;
		fprintf(fp, "%59.16s %-4u %-4u %-3u\n",	/* XXX: hardcoding */
			net6name(&grp_mask->group_addr.sin6_addr,
				 &grp_mask->group_mask),
			rp_grp_entry->priority,
			rp_grp_entry->advholdtime, rp_grp_entry->holdtime);
	    }
	}
    }
    return (TRUE);
}
