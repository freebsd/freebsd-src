/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
static char sccsid[] = "@(#)trace.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#ident "$Revision: 1.1.3.1 $"

#define	RIPCMDS
#include "defs.h"
#include "pathnames.h"
#include <sys/stat.h>
#include <sys/signal.h>
#include <fcntl.h>


#ifdef sgi
/* use *stat64 for files on large filesystems */
#define stat	stat64
#endif

#define	NRECORDS	50		/* size of circular trace buffer */

u_int	tracelevel, new_tracelevel;
FILE	*ftrace = stdout;		/* output trace file */
char	*tracelevel_msg = "";

char savetracename[MAXPATHLEN+1];


char *
naddr_ntoa(naddr a)
{
#define NUM_BUFS 4
	static int i;
	static struct {
	    char    str[16];		/* xxx.xxx.xxx.xxx\0 */
	} bufs[NUM_BUFS];
	struct in_addr addr;
	char *s;

	addr.s_addr = a;
	s = strcpy(bufs[i].str, inet_ntoa(addr));
	i = (i+1) % NUM_BUFS;
	return s;
}


char *
saddr_ntoa(struct sockaddr *sa)
{
	return (sa == 0) ? "?" : naddr_ntoa(S_ADDR(sa));
}


static char *
ts(time_t secs) {
	static char s[20];

	secs += epoch.tv_sec;
#ifdef sgi
	(void)cftime(s, "%T", &secs);
#else
	bcopy(ctime(&secs)+11, s, 8);
	s[8] = '\0';
#endif
	return s;
}


/* On each event, display a time stamp.
 * This assumes that 'now' is update once for each event, and
 * that at least now.tv_usec changes.
 */
void
lastlog(void)
{
	static struct timeval last;

	if (last.tv_sec != now.tv_sec
	    || last.tv_usec != now.tv_usec) {
		(void)fprintf(ftrace, "--- %s ---\n", ts(now.tv_sec));
		last = now;
	}
}


static void
tmsg(char *msg1, char* msg2)
{
	if (ftrace != 0) {
		lastlog();
		(void)fprintf(ftrace, "%s%s\n", msg1,msg2);
	}
}


static void
trace_close(char *msg1, char *msg2)
{
	int fd;

	fflush(stdout);
	fflush(stderr);

	if (ftrace != 0) {
		tmsg(msg1,msg2);
		fflush(ftrace);

		if (savetracename[0] != '\0') {
			fd = open(_PATH_DEVNULL, O_RDWR);
			(void)dup2(fd, STDOUT_FILENO);
			(void)dup2(fd, STDERR_FILENO);
			(void)close(fd);
			fclose(ftrace);
			ftrace = 0;
		}
	}
}


void
trace_flush(void)
{
	if (ftrace != 0) {
		fflush(ftrace);
		if (ferror(ftrace))
			trace_off("tracing off: ", strerror(ferror(ftrace)));
	}
}


void
trace_off(char *msg1, char *msg2)
{
	trace_close(msg1, msg2);

	new_tracelevel = tracelevel = 0;
}


void
trace_on(char *filename,
	 int trusted)
{
	struct stat stbuf;
	FILE *n_ftrace;


	if (stat(filename, &stbuf) >= 0 &&
	    (stbuf.st_mode & S_IFMT) != S_IFREG) {
		msglog("wrong type (%#x) of trace file \"%s\"",
		       stbuf.st_mode, filename);
		return;
	}
	if (!trusted
	    && strcmp(filename, savetracename)
	    && strncmp(filename, _PATH_TRACE, sizeof(_PATH_TRACE)-1)) {
		msglog("wrong directory for trace file %s", filename);
		return;
	}
	n_ftrace = fopen(filename, "a");
	if (n_ftrace == 0) {
		msglog("failed to open trace file \"%s\": %s",
		       filename, strerror(errno));
		return;
	}

	trace_close("switch to trace file ", filename);
	if (filename != savetracename)
		strncpy(savetracename, filename, sizeof(savetracename)-1);
	ftrace = n_ftrace;

	fflush(stdout);
	fflush(stderr);
	dup2(fileno(ftrace), STDOUT_FILENO);
	dup2(fileno(ftrace), STDERR_FILENO);

	if (new_tracelevel == 0) {
		tracelevel_msg = "trace command: ";
		new_tracelevel = 1;
	} else {
		tmsg("trace command","");
	}
}


/* ARGSUSED */
void
sigtrace_on(int s)
{
	new_tracelevel++;
	tracelevel_msg = "SIGUSR1: ";
}


/* ARGSUSED */
void
sigtrace_off(int s)
{
	new_tracelevel--;
	tracelevel_msg = "SIGUSR2: ";
}


/* Move to next higher level of tracing when -t option processed or
 * SIGUSR1 is received.  Successive levels are:
 *	actions
 *	actions + packets
 *	actions + packets + contents
 */
void
set_tracelevel(void)
{
	static char *off_msgs[MAX_TRACELEVEL] = {
		"Tracing actions stopped",
		"Tracing packets stopped",
		"Tracing packet contents stopped",
	};
	static char *on_msgs[MAX_TRACELEVEL] = {
		"Tracing actions started",
		"Tracing packets started",
		"Tracing packet contents started",
	};


	if (new_tracelevel > MAX_TRACELEVEL)
		new_tracelevel = MAX_TRACELEVEL;
	while (new_tracelevel != tracelevel) {
		if (new_tracelevel < tracelevel) {
			if (--tracelevel == 0)
				trace_off(tracelevel_msg, off_msgs[0]);
			else
				tmsg(tracelevel_msg, off_msgs[tracelevel]);
		} else {
			if (ftrace == 0) {
				if (savetracename[0] != '\0')
					trace_on(savetracename, 1);
				else
					ftrace = stdout;
			}
			tmsg(tracelevel_msg, on_msgs[tracelevel++]);
		}
	}
}


/* display an address
 */
char *
addrname(naddr	addr,			/* in network byte order */
	 naddr	mask,
	 int	force)			/* 0=show mask if nonstandard, */
{					/*	1=always show mask, 2=never */
	static char s[15+20];
	char *sp;
	naddr dmask;
	int i;

	(void)strcpy(s, naddr_ntoa(addr));

	if (force == 1 || (force == 0 && mask != std_mask(addr))) {
		sp = &s[strlen(s)];

		dmask = mask & -mask;
		if (mask + dmask == 0) {
			for (i = 0; i != 32 && ((1<<i) & mask) == 0; i++)
				continue;
			(void)sprintf(sp, "/%d", 32-i);

		} else {
			(void)sprintf(sp, " (mask %#x)", mask);
		}
	}

	return s;
}


/* display a bit-field
 */
struct bits {
	int	bits_mask;
	char	*bits_name;
};

static struct bits if_bits[] = {
	{ IFF_UP,		"" },
	{ IFF_BROADCAST,	"" },
	{ IFF_LOOPBACK,		"LOOPBACK" },
	{ IFF_POINTOPOINT,	"PT-TO-PT" },
	{ IFF_RUNNING,		"" },
	{ IFF_MULTICAST,	"" },
	{ -1,			""},
	{ 0 }
};

static struct bits is_bits[] = {
	{ IS_SUBNET,		"" },
	{ IS_REMOTE,		"REMOTE" },
	{ IS_PASSIVE,		"PASSIVE" },
	{ IS_EXTERNAL,		"EXTERNAL" },
	{ IS_CHECKED,		"" },
	{ IS_ALL_HOSTS,		"" },
	{ IS_ALL_ROUTERS,	"" },
	{ IS_RIP_QUERIED,	"" },
	{ IS_BROKE,		"BROKE" },
	{ IS_ACTIVE,		"ACTIVE" },
	{ IS_QUIET,		"QUIET" },
	{ IS_NEED_NET_SUB,	"" },
	{ IS_NO_AG,		"NO_AG" },
	{ IS_NO_SUPER_AG,	"NO_SUPER_AG" },
	{ (IS_NO_RIPV1_IN
	   | IS_NO_RIPV2_IN
	   | IS_NO_RIPV1_OUT
	   | IS_NO_RIPV2_OUT),	"NO_RIP" },
	{ IS_NO_RIPV1_IN,	"NO_RIPV1_IN" },
	{ IS_NO_RIPV2_IN,	"NO_RIPV2_IN" },
	{ IS_NO_RIPV1_OUT,	"NO_RIPV1_OUT" },
	{ IS_NO_RIPV2_OUT,	"NO_RIPV2_OUT" },
	{ (IS_NO_ADV_IN
	   | IS_NO_SOL_OUT
	   | IS_NO_ADV_OUT),	"NO_RDISC" },
	{ IS_NO_SOL_OUT,	"NO_SOLICIT" },
	{ IS_SOL_OUT,		"SEND_SOLICIT" },
	{ IS_NO_ADV_OUT,	"NO_RDISC_ADV" },
	{ IS_ADV_OUT,		"RDISC_ADV" },
	{ IS_BCAST_RDISC,	"BCAST_RDISC" },
	{ 0 }
};

static struct bits rs_bits[] = {
	{ RS_IF,	"IF" },
	{ RS_NET_SUB,	"NET_SUB" },
	{ RS_NET_HOST,	"NET_HOST" },
	{ RS_NET_INT,	"NET_INT" },
	{ RS_SUBNET,	"" },
	{ RS_LOCAL,	"LOCAL" },
	{ RS_MHOME,	"MHOME" },
	{ RS_GW,	"GW" },
	{ RS_STATIC,	"STATIC" },
	{ RS_RDISC,	"RDISC" },
	{ 0 }
};


static void
trace_bits(struct bits *tbl,
	   u_int field)
{
	int first = 1;
	int b;


	while (field != 0) {
		b = tbl->bits_mask;
		if (b == 0)
			break;
		if ((b & field) == b
		    && tbl->bits_name[0] != '\0') {
			(void)fprintf(ftrace, first ? "<%s" : "|%s",
				      tbl->bits_name);
			first = 0;
		}
		field &= ~b;
		tbl++;
	}
	if (field != 0) {
		(void)fputc(first ? '<' : '|', ftrace);
		(void)fprintf(ftrace, "%#x", field);
		first = 0;
	}

	if (!first)
		(void)fputs("> ", ftrace);
}


void
trace_if(char *act,
	  struct interface *ifp)
{
	if (ftrace == 0)
		return;

	lastlog();
	(void)fprintf(ftrace, "%s interface %-4s ", act, ifp->int_name);
	(void)fprintf(ftrace, "%-15s --> %s ",
		      naddr_ntoa(ifp->int_addr),
		      ((ifp->int_if_flags & IFF_POINTOPOINT)
		       ? naddr_ntoa(ifp->int_dstaddr)
		       : addrname(htonl(ifp->int_net), ifp->int_mask, 0)));
	(void)fprintf(ftrace, "metric=%d ", ifp->int_metric);
	trace_bits(if_bits, ifp->int_if_flags);
	trace_bits(is_bits, ifp->int_state);
	(void)fputc('\n',ftrace);
}


void
trace_upslot(struct rt_entry *rt,
	     struct rt_spare *rts,
	     naddr	gate,
	     naddr	router,
	     struct interface *ifp,
	     int	metric,
	     u_short	tag,
	     time_t	new_time)
{
	if (ftrace == 0)
		return;
	if (rts->rts_gate == gate
	    && rts->rts_router == router
	    && rts->rts_metric == metric
	    && rts->rts_tag == tag)
		return;

	lastlog();
	if (rts->rts_gate != RIP_DEFAULT) {
		(void)fprintf(ftrace, "Chg #%d %-16s--> ",
			      rts - rt->rt_spares,
			      addrname(rt->rt_dst, rt->rt_mask, 0));
		(void)fprintf(ftrace, "%-15s ",
			      naddr_ntoa(rts->rts_gate));
		if (rts->rts_gate != rts->rts_gate)
			(void)fprintf(ftrace, "router=%s ",
				      naddr_ntoa(rts->rts_gate));
		if (rts->rts_tag != 0)
			(void)fprintf(ftrace, "tag=%#x ", rts->rts_tag);
		(void)fprintf(ftrace, "metric=%-2d ", rts->rts_metric);
		if (rts->rts_ifp != 0)
			(void)fprintf(ftrace, "%s ",
				      rts->rts_ifp->int_name);
		(void)fprintf(ftrace, "%s\n", ts(rts->rts_time));

		(void)fprintf(ftrace, "       %-16s--> ",
			      addrname(rt->rt_dst, rt->rt_mask, 0));
		(void)fprintf(ftrace, "%-15s ",
			      gate != rts->rts_gate ? naddr_ntoa(gate) : "");
		if (gate != router)
			(void)fprintf(ftrace,"router=%s ",naddr_ntoa(router));
		if (tag != rts->rts_tag)
			(void)fprintf(ftrace, "tag=%#x ", tag);
		if (metric != rts->rts_metric)
			(void)fprintf(ftrace, "metric=%-2d ", metric);
		if (ifp != rts->rts_ifp && ifp != 0 )
			(void)fprintf(ftrace, "%s ", ifp->int_name);
		(void)fprintf(ftrace, "%s\n",
			      new_time != rts->rts_time ? ts(new_time) : "");

	} else {
		(void)fprintf(ftrace, "Add #%d %-16s--> ",
			      rts - rt->rt_spares,
			      addrname(rt->rt_dst, rt->rt_mask, 0));
		(void)fprintf(ftrace, "%-15s ", naddr_ntoa(gate));
		if (gate != router)
			(void)fprintf(ftrace, "router=%s ", naddr_ntoa(gate));
		if (tag != 0)
			(void)fprintf(ftrace, "tag=%#x ", tag);
		(void)fprintf(ftrace, "metric=%-2d ", metric);
		if (ifp != 0)
			(void)fprintf(ftrace, "%s ", ifp->int_name);
		(void)fprintf(ftrace, "%s\n", ts(new_time));
	}
}


void
trace_msg(char *p, ...)
{
	va_list args;

	if (!TRACEACTIONS || ftrace == 0)
		return;

	lastlog();
	va_start(args, p);
	vfprintf(ftrace, p, args);
}


void
trace_change(struct rt_entry *rt,
	     u_int	state,
	     naddr	gate,		/* forward packets here */
	     naddr	router,		/* on the authority of this router */
	     int	metric,
	     u_short	tag,
	     struct interface *ifp,
	     time_t	new_time,
	     char	*label)
{
	if (ftrace == 0)
		return;

	if (rt->rt_metric == metric
	    && rt->rt_gate == gate
	    && rt->rt_router == router
	    && rt->rt_state == state
	    && rt->rt_tag == tag)
		return;

	lastlog();
	(void)fprintf(ftrace, "%s %-16s--> %-15s metric=%-2d ",
		      label,
		      addrname(rt->rt_dst, rt->rt_mask, 0),
		      naddr_ntoa(rt->rt_gate), rt->rt_metric);
	if (rt->rt_router != rt->rt_gate)
		(void)fprintf(ftrace, "router=%s ",
			      naddr_ntoa(rt->rt_router));
	if (rt->rt_tag != 0)
		(void)fprintf(ftrace, "tag=%#x ", rt->rt_tag);
	trace_bits(rs_bits, rt->rt_state);
	(void)fprintf(ftrace, "%s ",
		      rt->rt_ifp == 0 ? "-" : rt->rt_ifp->int_name);
	(void)fprintf(ftrace, "%s\n",
		      AGE_RT(rt, rt->rt_ifp) ? ts(rt->rt_time) : "");

	(void)fprintf(ftrace, "%*s %-16s--> %-15s ",
		      strlen(label), "",
		      addrname(rt->rt_dst, rt->rt_mask, 0),
		      (rt->rt_gate != gate) ? naddr_ntoa(gate) : "");
	if (rt->rt_metric != metric)
		(void)fprintf(ftrace, "metric=%-2d ", metric);
	if (router != gate)
		(void)fprintf(ftrace, "router=%s ", naddr_ntoa(router));
	if (rt->rt_tag != tag)
		(void)fprintf(ftrace, "tag=%#x ", tag);
	if (rt->rt_state != state)
		trace_bits(rs_bits, state);
	if (rt->rt_ifp != ifp)
		(void)fprintf(ftrace, "%s ",
			      ifp != 0 ? ifp->int_name : "-");
	if (rt->rt_hold_down > now.tv_sec)
		(void)fprintf(ftrace, "hold-down=%d ",
			      rt->rt_hold_down - now.tv_sec);
	(void)fprintf(ftrace, "%s\n",
		      ((rt->rt_time == new_time || !AGE_RT(rt, ifp))
		       ? "" : ts(new_time)));
}


void
trace_add_del(char * action, struct rt_entry *rt)
{
	u_int state = rt->rt_state;

	if (ftrace == 0)
		return;

	lastlog();
	(void)fprintf(ftrace, "%s    %-16s--> %-15s metric=%-2d ",
		      action,
		      addrname(rt->rt_dst, rt->rt_mask, 0),
		      naddr_ntoa(rt->rt_gate), rt->rt_metric);
	if (rt->rt_router != rt->rt_gate)
		(void)fprintf(ftrace, "router=%s ",
			      naddr_ntoa(rt->rt_router));
	if (rt->rt_tag != 0)
		(void)fprintf(ftrace, "tag=%#x ", rt->rt_tag);
	trace_bits(rs_bits, state);
	if (rt->rt_ifp != 0)
		(void)fprintf(ftrace, "%s ", rt->rt_ifp->int_name);
	(void)fprintf(ftrace, "%s\n", ts(rt->rt_time));
}


void
trace_rip(char *dir1, char *dir2,
	  struct sockaddr_in *who,
	  struct interface *ifp,
	  struct rip *msg,
	  int size)			/* total size of message */
{
	struct netinfo *n, *lim;
	struct netauth *a;
	int i;

	if (ftrace == 0)
		return;

	lastlog();
	if (msg->rip_cmd >= RIPCMD_MAX
	    || msg->rip_vers == 0) {
		(void)fprintf(ftrace, "%s bad RIPv%d cmd=%d %s %s.%d%s%s"
			      " size=%d msg=%#x\n",
			      dir1, msg->rip_vers, msg->rip_cmd, dir2,
			      naddr_ntoa(who->sin_addr.s_addr),
			      ntohs(who->sin_port),
			      size, msg);
		return;
	}

	(void)fprintf(ftrace, "%s RIPv%d %s %s %s.%d%s%s\n",
		      dir1, msg->rip_vers, ripcmds[msg->rip_cmd], dir2,
		      naddr_ntoa(who->sin_addr.s_addr), ntohs(who->sin_port),
		      ifp ? " via " : "", ifp ? ifp->int_name : "");
	if (!TRACECONTENTS)
		return;

	switch (msg->rip_cmd) {
	case RIPCMD_REQUEST:
	case RIPCMD_RESPONSE:
		n = msg->rip_nets;
		lim = (struct netinfo *)((char*)msg + size);
		for (; n < lim; n++) {
			if (n->n_family == RIP_AF_UNSPEC
			    && ntohl(n->n_metric) == HOPCNT_INFINITY
			    && n+1 == lim
			    && n == msg->rip_nets
			    && msg->rip_cmd == RIPCMD_REQUEST) {
				(void)fputs("\tQUERY ", ftrace);
				if (n->n_dst != 0)
					(void)fprintf(ftrace, "%s ",
						      naddr_ntoa(n->n_dst));
				if (n->n_mask != 0)
					(void)fprintf(ftrace, "mask=%#x ",
						      ntohl(n->n_mask));
				if (n->n_nhop != 0)
					(void)fprintf(ftrace, " nhop=%s ",
						      naddr_ntoa(n->n_nhop));
				if (n->n_tag != 0)
					(void)fprintf(ftrace, "tag=%#x",
						      n->n_tag);
				(void)fputc('\n',ftrace);
				continue;
			}

			if (n->n_family == RIP_AF_AUTH) {
				a = (struct netauth*)n;
				(void)fprintf(ftrace,
					      "\tAuthentication type %d: ",
					      ntohs(a->a_type));
				for (i = 0;
				     i < sizeof(a->au.au_pw);
				     i++)
					(void)fprintf(ftrace, "%02x ",
						      a->au.au_pw[i]);
				(void)fputc('\n',ftrace);
				continue;
			}

			if (n->n_family != RIP_AF_INET) {
				(void)fprintf(ftrace,
					      "\t(af %d) %-18s mask=%#x",
					      ntohs(n->n_family),
					      naddr_ntoa(n->n_dst),
					      ntohl(n->n_mask));
			} else if (msg->rip_vers == RIPv1) {
				(void)fprintf(ftrace, "\t%-18s ",
					      addrname(n->n_dst,
						       ntohl(n->n_mask),
						       n->n_mask==0 ? 2 : 1));
			} else {
				(void)fprintf(ftrace, "\t%-18s ",
					      addrname(n->n_dst,
						       ntohl(n->n_mask),
						       n->n_mask==0 ? 2 : 0));
			}
			(void)fprintf(ftrace, "metric=%-2d ",
				      ntohl(n->n_metric));
			if (n->n_nhop != 0)
				(void)fprintf(ftrace, " nhop=%s ",
					      naddr_ntoa(n->n_nhop));
			if (n->n_tag != 0)
				(void)fprintf(ftrace, "tag=%#x",
					      n->n_tag);
			(void)fputc('\n',ftrace);
		}
		if (size != (char *)n - (char *)msg)
			(void)fprintf(ftrace, "truncated record, len %d\n",
				size);
		break;

	case RIPCMD_TRACEON:
		fprintf(ftrace, "\tfile=%*s\n", size-4, msg->rip_tracefile);
		break;

	case RIPCMD_TRACEOFF:
		break;
	}
}
