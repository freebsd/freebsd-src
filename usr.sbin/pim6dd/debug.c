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
 *  Pavlin Ivanov Radoslavov (pavlin@catarina.usc.edu)
 *
 *  $Id: debug.c,v 1.10 2000/10/05 22:27:07 itojun Exp $
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD$
 */

#include "defs.h"


#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern int haveterminal;
extern char *progname;

int log_nmsgs = 0;
unsigned long debug = 0x00000000;        /* If (long) is smaller than
					  * 4 bytes, then we are in
					  * trouble.
					  */
static char dumpfilename[] = _PATH_PIM6D_DUMP;
static char cachefilename[] = _PATH_PIM6D_CACHE; /* TODO: notused */

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

char *
packet_kind(proto, type, code)
    u_int proto, type, code;
{
    static char unknown[60];

    switch (proto) {
     case IPPROTO_ICMPV6:
	switch (type) {
	 case MLD6_LISTENER_QUERY:	return "Multicast Listener Query    ";
	 case MLD6_LISTENER_REPORT:	return "Multicast Listener Report   ";
	 case MLD6_LISTENER_DONE:	return "Multicast Listener Done     ";
	 default:
	    sprintf(unknown,
		    "UNKNOWN ICMPv6 message: type = 0x%02x, code = 0x%02x ",
		    type, code);
	    return unknown;
	}

     case IPPROTO_PIM:    /* PIM v2 */
	switch (type) {
	case PIM_V2_HELLO:	       return "PIM v2 Hello             ";
	case PIM_V2_REGISTER:          return "PIM v2 Register          ";
	case PIM_V2_REGISTER_STOP:     return "PIM v2 Register_Stop     ";
	case PIM_V2_JOIN_PRUNE:        return "PIM v2 Join/Prune        ";
	case PIM_V2_BOOTSTRAP:         return "PIM v2 Bootstrap         ";
	case PIM_V2_ASSERT:            return "PIM v2 Assert            ";
	case PIM_V2_GRAFT:             return "PIM-DM v2 Graft          ";
	case PIM_V2_GRAFT_ACK:         return "PIM-DM v2 Graft_Ack      ";
	case PIM_V2_CAND_RP_ADV:       return "PIM v2 Cand. RP Adv.     ";
	default:
	    sprintf(unknown,      "UNKNOWN PIM v2 message type =%3d ", type);
	    return unknown;
	}
    default:
	sprintf(unknown,          "UNKNOWN proto =%3d               ", proto);
	return unknown;
    }
}


/*
 * Used for debugging particular type of messages.
 */
int
debug_kind(proto, type, code)
    u_int proto, type, code;
{
    switch (proto) {
    case IPPROTO_ICMPV6:
	switch (type) {
	case MLD6_LISTENER_QUERY:	return DEBUG_MLD;
	case MLD6_LISTENER_REPORT:	return DEBUG_MLD;
	case MLD6_LISTENER_DONE:	return DEBUG_MLD;
	default:                           return DEBUG_MLD;
	}
    case IPPROTO_PIM: 	/* PIM v2 */
	/* TODO: modify? */
	switch (type) {
	case PIM_V2_HELLO:             return DEBUG_PIM;
	case PIM_V2_REGISTER:          return DEBUG_PIM_REGISTER;
	case PIM_V2_REGISTER_STOP:     return DEBUG_PIM_REGISTER;
	case PIM_V2_JOIN_PRUNE:        return DEBUG_PIM;
	case PIM_V2_BOOTSTRAP:         return DEBUG_PIM_BOOTSTRAP;
	case PIM_V2_ASSERT:            return DEBUG_PIM;
	case PIM_V2_GRAFT:             return DEBUG_PIM;
	case PIM_V2_GRAFT_ACK:         return DEBUG_PIM;
	case PIM_V2_CAND_RP_ADV:       return DEBUG_PIM_CAND_RP;
	default:                       return DEBUG_PIM;
	}
    default:                               return 0;
    }
    return 0;
}


/*
 * Some messages are more important than others.  This routine
 * determines the logging level at which to log a send error (often
 * "No route to host").  This is important when there is asymmetric
 * reachability and someone is trying to, i.e., mrinfo me periodically.
 */
int
log_level(proto, type, code)
    u_int proto, type, code;
{
    switch (proto) {
    case IPPROTO_ICMPV6:
	switch (type) {
	default:
	    return LOG_WARNING;
	}
	
    case IPPROTO_PIM:
	/* PIM v2 */
	switch (type) {
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
/* TODO: currently not used
void 
dump(int i)
{
    dump_vifs(stderr);
    dump_pim_mrt(stderr);
}	
*/

/*
 * Dump internal data structures to a file.
 */
void 
fdump(i)
    int i;
{
    FILE *fp;
    fp = fopen(dumpfilename, "w");
    if (fp != NULL) {
	dump_vifs(fp);
	dump_mldqueriers(fp);
	dump_pim_mrt(fp);
	dump_lcl_grp(fp);
	(void) fclose(fp);
    }
}

/* TODO: dummy, to be used in the future. */
/*
 * Dump local cache contents to a file.
 */
void
cdump(i)
    int i;
{
    FILE *fp;
    
    fp = fopen(cachefilename, "w");
    if (fp != NULL) {
      /* TODO: implement it:
	 dump_cache(fp); 
	 */
	(void) fclose(fp);
    }
}

void
dump_vifs(fp)
    FILE *fp;
{	
    vifi_t vifi;
    register struct uvif *v;
    pim_nbr_entry_t *n;
    struct phaddr *pa;
    int width;
    int i;

    fprintf(fp, "\nMulticast Interface Table\n %-4s %-6s %-50s %-14s %s",
	    "Mif", " PhyIF", "Local-Address/Prefixlen", "Flags",
	    "Neighbors\n");
    
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	    int firstaddr = 1;
	    for (pa = v->uv_addrs; pa; pa = pa->pa_next) {
		    if (!firstaddr) {
			    fprintf(fp, "  %3s %6s %-50s\n", "", "",
				    net6name(&pa->pa_addr.sin6_addr,
					     &pa->pa_subnetmask));
			    continue;
		    }

		    firstaddr = 0;
		    fprintf(fp, "  %-3u %6s %-50s", vifi, v->uv_name,
			    net6name(&pa->pa_addr.sin6_addr,
				     &pa->pa_subnetmask));
		    firstaddr = 0;
#if 0
		    if (v->uv_flags & MIFF_REGISTER)
			    fprintf(fp, "%-16s ", v->uv_name);
		    else
			    fprintf(fp,"%-16.16s ",
				    net6name(&v->uv_prefix.sin6_addr,
					     &v->uv_subnetmask));
#endif
		    width = 0;
		    if (v->uv_flags & VIFF_DISABLED)
			    fprintf(fp, " DISABLED");
		    if (v->uv_flags & VIFF_NOLISTENER)
			    fprintf(fp, " NOLISTENER");
		    if (v->uv_flags & VIFF_DOWN)
			    fprintf(fp, " DOWN");
		    if (v->uv_flags & VIFF_DR) {
			    fprintf(fp, " DR");
			    width += 3;
		    }
		    if (v->uv_flags & VIFF_PIM_NBR) {
			    fprintf(fp, " PIM");
			    width += 4;
		    }
#if 0				/* impossible */
		    if (v->uv_flags & VIFF_DVMRP_NBR) {
			    fprintf(fp, " DVMRP");
			    width += 6;
		    }
#endif 
		    if (v->uv_flags & VIFF_NONBRS) {
			    fprintf(fp, " %-12s", "NO-NBR");
			    width += 6;
		    }
		    if (v->uv_flags & VIFF_QUERIER)
			    fprintf(fp, " QRY");

		    if ((n = v->uv_pim_neighbors) != NULL) {
			    /* Print the first neighbor on the same line */
			    for (i = width; i <= 15; i++)
				    fprintf(fp, " ");
			    fprintf(fp, "%-12s\n",
				    inet6_fmt(&n->address.sin6_addr));
			    for (n = n->next; n != NULL; n = n->next)
				    fprintf(fp, "%64s %-15s\n", "",
					    inet6_fmt(&n->address.sin6_addr));
	    
		    }
		    else
			    fprintf(fp, "\n");
	    }
    }
    fprintf(fp, "\n");
}


/*
 * Log errors and other messages to the system log daemon and to stderr,
 * according to the severity of the message and the current debug level.
 * For errors of severity LOG_ERR or worse, terminate the program.
 */
#ifdef __STDC__
void
log(int severity, int syserr, char *format, ...)
{
    va_list ap;
    static char fmt[211] = "warning - ";
    char *msg;
    struct timeval now;
    struct tm *thyme;
    
    va_start(ap, format);
#else
/*VARARGS3*/
void
log(severity, syserr, format, va_alist)
    int severity, syserr;
    char *format;
    va_dcl
{
    va_list ap;
    static char fmt[311] = "warning - ";
    char *msg;
    char tbuf[20];
    struct timeval now;
    struct tm *thyme;
    
    va_start(ap);
#endif
    vsprintf(&fmt[10], format, ap);
    va_end(ap);
    msg = (severity == LOG_WARNING) ? fmt : &fmt[10];
    
    /*
     * Log to stderr if we haven't forked yet and it's a warning or worse,
     * or if we're debugging.
     */
    if (haveterminal && (debug || severity <= LOG_WARNING)) {
	time_t sectime;
	gettimeofday(&now,NULL);
	sectime = (time_t) now.tv_sec;
	thyme = localtime(&sectime);
	if (!debug)
	    fprintf(stderr, "%s: ", progname);
	fprintf(stderr, "%02d:%02d:%02d.%03ld %s", thyme->tm_hour,
		thyme->tm_min, thyme->tm_sec, (long int)now.tv_usec / 1000,
		msg);
	if (syserr == 0)
	    fprintf(stderr, "\n");
	else if (syserr < sys_nerr)
	    fprintf(stderr, ": %s\n", sys_errlist[syserr]);
	else
	    fprintf(stderr, ": errno %d\n", syserr);
    }
    
    /*
     * Always log things that are worse than warnings, no matter what
     * the log_nmsgs rate limiter says.
     * Only count things worse than debugging in the rate limiter
     * (since if you put daemon.debug in syslog.conf you probably
     * actually want to log the debugging messages so they shouldn't
     * be rate-limited)
     */
    if ((severity < LOG_WARNING) || (log_nmsgs < LOG_MAX_MSGS)) {
	if (severity < LOG_DEBUG)
	    log_nmsgs++;
	if (syserr != 0) {
	    errno = syserr;
	    syslog(severity, "%s: %m", msg);
	} else
	    syslog(severity, "%s", msg);
    }
    
    if (severity <= LOG_ERR) exit(-1);
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
				inet6_fmt(&v->uv_querier->al_addr.sin6_addr),
				(u_long)v->uv_querier->al_timer,
				sec2str(now - v->uv_querier->al_ctime));
		}
	}

	fprintf(fp, "\n");
} 

/* TODO: format the output for better readability */
void 
dump_pim_mrt(fp)
    FILE *fp;
{
    grpentry_t *g;
    register mrtentry_t *r;
    register vifi_t vifi;
    u_int number_of_groups = 0;
    char oifs[(sizeof(if_set)<<3)+1];
    char pruned_oifs[(sizeof(if_set)<<3)+1];
    char asserted_oifs[(sizeof(if_set)<<3)+1];
    char leaves_oifs[(sizeof(if_set)<<3)+1];
    char filter_oifs[(sizeof(if_set)<<3)+1];
    char incoming_iif[(sizeof(if_set)<<3)+1];
    
    fprintf(fp, "Multicast Routing Table\n%s", 
	    " Source          Group           Flags\n");

    /* TODO: remove the dummy 0:: group (first in the chain) */
    for (g = grplist->next; g != (grpentry_t *)NULL; g = g->next) {
	number_of_groups++;
	/* Print all (S,G) routing info */
	for (r = g->mrtlink; r != (mrtentry_t *)NULL; r = r->grpnext) {
	    fprintf(fp, "---------------------------(S,G)----------------------------\n");
	    /* Print the routing info */
	    fprintf(fp, " %-15s", inet6_fmt(&r->source->address.sin6_addr));
	    fprintf(fp, " %-15s", inet6_fmt(&g->group.sin6_addr));

	    for (vifi = 0; vifi < numvifs; vifi++) {
		oifs[vifi] =
		    IF_ISSET(vifi, &r->oifs)          ? 'o' : '.';
		pruned_oifs[vifi] =
		    IF_ISSET(vifi, &r->pruned_oifs)   ? 'p' : '.';
		asserted_oifs[vifi] =
		    IF_ISSET(vifi, &r->pruned_oifs)   ? 'a' : '.';
		filter_oifs[vifi] =
		    IF_ISSET(vifi, &r->filter_oifs)   ? 'f' : '.';
		leaves_oifs[vifi] =
		    IF_ISSET(vifi, &r->leaves)        ? 'l' : '.';
		incoming_iif[vifi] = '.';
	    }
	    oifs[vifi]          = 0x0;  /* End of string */
	    pruned_oifs[vifi]   = 0x0;
	    leaves_oifs[vifi]   = 0x0;
	    filter_oifs[vifi]	= 0x0;
	    incoming_iif[vifi]  = 0x0;
	    incoming_iif[r->incoming] = 'I';

	    /* TODO: don't need some of the flags */
	    if (r->flags & MRTF_SPT)           fprintf(fp, " SPT");
	    if (r->flags & MRTF_WC)            fprintf(fp, " WC");
	    if (r->flags & MRTF_RP)            fprintf(fp, " RP");
	    if (r->flags & MRTF_REGISTER)      fprintf(fp, " REG");
	    if (r->flags & MRTF_IIF_REGISTER)  fprintf(fp, " IIF_REG");
	    if (r->flags & MRTF_NULL_OIF)      fprintf(fp, " NULL_OIF");
	    if (r->flags & MRTF_KERNEL_CACHE)  fprintf(fp, " CACHE");
	    if (r->flags & MRTF_ASSERTED)      fprintf(fp, " ASSERTED");
	    if (r->flags & MRTF_REG_SUPP)      fprintf(fp, " REG_SUPP");
	    if (r->flags & MRTF_SG)            fprintf(fp, " SG");
	    if (r->flags & MRTF_PMBR)          fprintf(fp, " PMBR");
	    fprintf(fp, "\n");
	    
	    fprintf(fp, "Pruned   oifs: %-20s\n", pruned_oifs);
	    fprintf(fp, "Asserted oifs: %-20s\n", pruned_oifs);
	    fprintf(fp, "Filtered oifs: %-20s\n", filter_oifs);
	    fprintf(fp, "Leaves   oifs: %-20s\n", leaves_oifs);
	    fprintf(fp, "Outgoing oifs: %-20s\n", oifs);
	    fprintf(fp, "Incoming     : %-20s\n", incoming_iif);

	    fprintf(fp, "Upstream nbr: %s\n", 
		    r->upstream ? inet6_fmt(&r->upstream->address.sin6_addr) : "NONE");
	    fprintf(fp, "\nTIMERS:  Entry   Prune VIFS:");
	    for (vifi = 0; vifi < numvifs; vifi++)
		fprintf(fp, "  %2d", vifi);
	    fprintf(fp, "\n           %3d              ",
		    r->timer);
	    for (vifi = 0; vifi < numvifs; vifi++)
		fprintf(fp, " %3d", r->prune_timers[vifi]);
	    fprintf(fp, "\n");
	}
    }/* for all groups */

    fprintf(fp, "Number of Groups: %u\n", number_of_groups);
}

void
dump_lcl_grp(fp)
	FILE *fp;
{
	vifi_t vifi;
    	struct uvif *v;
	struct listaddr *g;

	fprintf(fp, "\nList of local listener information per interface\n");
	for (vifi = 0, v = uvifs; vifi < numvifs; vifi++, v++) {
		int first = 1;

		for (g = v->uv_groups; g != NULL; g = g->al_next) {
			if (first) {
				fprintf(fp, "  Mif %d(%s)\n", vifi, v->uv_name);
				first = 0;
			}
			fprintf(fp, "    %s", inet6_fmt(&g->al_addr.sin6_addr));
			if (g->al_timerid)
				fprintf(fp, " timeout: %d",
					timer_leftTimer(g->al_timerid));
			if (g->al_query)
				fprintf(fp, " querytimer: %d",
					timer_leftTimer(g->al_timerid));
			fputc('\n', fp);
		}
	}
}
