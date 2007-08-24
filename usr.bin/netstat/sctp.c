/*-
 * Copyright (c) 2001-2007, by Weongyo Jeong. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#if 0
#ifndef lint
static char sccsid[] = "@(#)sctp.c	0.1 (Berkeley) 4/18/2007";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/protosw.h>

#include <netinet/in.h>
#include <netinet/sctp.h>
#include <netinet/sctp_constants.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

#ifdef SCTP

void	inetprint (struct in_addr *, int, const char *, int);
static void sctp_statesprint(uint32_t state);

#define NETSTAT_SCTP_STATES_CLOSED		0x0
#define NETSTAT_SCTP_STATES_BOUND		0x1
#define NETSTAT_SCTP_STATES_LISTEN		0x2
#define NETSTAT_SCTP_STATES_COOKIE_WAIT		0x3
#define NETSTAT_SCTP_STATES_COOKIE_ECHOED	0x4
#define NETSTAT_SCTP_STATES_ESTABLISHED		0x5
#define NETSTAT_SCTP_STATES_SHUTDOWN_SENT	0x6
#define NETSTAT_SCTP_STATES_SHUTDOWN_RECEIVED	0x7
#define NETSTAT_SCTP_STATES_SHUTDOWN_ACK_SENT	0x8
#define NETSTAT_SCTP_STATES_SHUTDOWN_PENDING	0x9

char *sctpstates[] = {
	"CLOSED",
	"BOUND",
	"LISTEN", 
	"COOKIE_WAIT", 
	"COOKIE_ECHOED", 
	"ESTABLISHED", 
	"SHUTDOWN_SENT",
	"SHUTDOWN_RECEIVED",
	"SHUTDOWN_ACK_SENT",
	"SHUTDOWN_PENDING"
};

LIST_HEAD(xladdr_list, xladdr_entry) xladdr_head;
struct xladdr_entry {
	struct xsctp_laddr *xladdr;
	LIST_ENTRY(xladdr_entry) xladdr_entries;
};

LIST_HEAD(xraddr_list, xraddr_entry) xraddr_head;
struct xraddr_entry {
        struct xsctp_raddr *xraddr;
        LIST_ENTRY(xraddr_entry) xraddr_entries;
};

static int
sctp_skip_xinpcb_ifneed(char *buf, const size_t buflen, size_t *offset)
{
	int exist_tcb = 0;
	struct xsctp_tcb *xstcb;
	struct xsctp_raddr *xraddr;
	struct xsctp_laddr *xladdr;

	while (*offset < buflen) {
		xladdr = (struct xsctp_laddr *)(buf + *offset);
		*offset += sizeof(struct xsctp_laddr);
		if (xladdr->last == 1)
			break;
	}
	
	while (*offset < buflen) {
		xstcb = (struct xsctp_tcb *)(buf + *offset);
		*offset += sizeof(struct xsctp_tcb);
		if (xstcb->last == 1)
			break;

		exist_tcb = 1;

		while (*offset < buflen) {
			xladdr = (struct xsctp_laddr *)(buf + *offset);
			*offset += sizeof(struct xsctp_laddr);
			if (xladdr->last == 1)
				break;
		}

		while (*offset < buflen) {
			xraddr = (struct xsctp_raddr *)(buf + *offset);
			*offset += sizeof(struct xsctp_raddr);
			if (xraddr->last == 1)
				break;
		}
	}

	/*
	 * If Lflag is set, we don't care about the return value.
	 */
	if (Lflag)
		return 0;

	return exist_tcb;
}

static void
sctp_process_tcb(struct xsctp_tcb *xstcb, const char *name,
    char *buf, const size_t buflen, size_t *offset, int *indent)
{
	int i, xl_total = 0, xr_total = 0, x_max;
	struct sockaddr *sa;
	struct xsctp_raddr *xraddr;
	struct xsctp_laddr *xladdr;
	struct xladdr_entry *prev_xl = NULL, *xl = NULL, *xl_tmp;
	struct xraddr_entry *prev_xr = NULL, *xr = NULL, *xr_tmp;
#ifdef INET6
	struct sockaddr_in6 *in6;
#endif

	LIST_INIT(&xladdr_head);
	LIST_INIT(&xraddr_head);

	/*
	 * Make `struct xladdr_list' list and `struct xraddr_list' list
	 * to handle the address flexibly.
	 */
	while (*offset < buflen) {
		xladdr = (struct xsctp_laddr *)(buf + *offset);
		*offset += sizeof(struct xsctp_laddr);
		if (xladdr->last == 1)
			break;
		
		prev_xl = xl;
		xl = malloc(sizeof(struct xladdr_entry));
		if (xl == NULL) {
			warnx("malloc %lu bytes", 
			    (u_long)sizeof(struct xladdr_entry));
			goto out;
		}
		xl->xladdr = xladdr;
		if (prev_xl == NULL)
			LIST_INSERT_HEAD(&xladdr_head, xl, xladdr_entries);
		else
			LIST_INSERT_AFTER(prev_xl, xl, xladdr_entries);
		xl_total++;
	}
	
	while (*offset < buflen) {
		xraddr = (struct xsctp_raddr *)(buf + *offset);
		*offset += sizeof(struct xsctp_raddr);
		if (xraddr->last == 1)
			break;
		
		prev_xr = xr;
		xr = malloc(sizeof(struct xraddr_entry));
		if (xr == NULL) {
			warnx("malloc %lu bytes", 
			    (u_long)sizeof(struct xraddr_entry));
			goto out;
		}
		xr->xraddr = xraddr;
		if (prev_xr == NULL)
			LIST_INSERT_HEAD(&xraddr_head, xr, xraddr_entries);
		else
			LIST_INSERT_AFTER(prev_xr, xr, xraddr_entries);
		xr_total++;
	}
	
	/*
	 * Let's print the address infos.
	 */
	xl = LIST_FIRST(&xladdr_head);
	xr = LIST_FIRST(&xraddr_head);
	x_max = (xl_total > xr_total) ? xl_total : xr_total;
	for (i = 0; i < x_max; i++) {
		if (((*indent == 0) && i > 0) || *indent > 0)
			printf("%-11s ", " ");
		
		if (xl != NULL) {
			sa = &(xl->xladdr->address.sa);
			if ((sa->sa_family) == AF_INET)
				inetprint(&((struct sockaddr_in *)sa)->sin_addr, 
				    htons(xstcb->local_port), 
				    name, numeric_port);
#ifdef INET6
			else {
				in6 = (struct sockaddr_in6 *)sa;
				inet6print(&in6->sin6_addr,
				    htons(xstcb->local_port),
				    name, numeric_port);
			}
#endif
		}
		
		if (xr != NULL && !Lflag) {
			sa = &(xr->xraddr->address.sa);
			if ((sa->sa_family) == AF_INET)
				inetprint(&((struct sockaddr_in *)sa)->sin_addr,
				    htons(xstcb->remote_port),
				    name, numeric_port);
#ifdef INET6
			else {
				in6 = (struct sockaddr_in6 *)sa;
				inet6print(&in6->sin6_addr,
				    htons(xstcb->remote_port),
				    name, numeric_port);
			}
#endif
		}
		
		if (xl != NULL)
			xl = LIST_NEXT(xl, xladdr_entries);
		if (xr != NULL)
			xr = LIST_NEXT(xr, xraddr_entries);
		
		if (i == 0 && !Lflag)
			sctp_statesprint(xstcb->state);
		
		if (i < x_max)
			putchar('\n');
	}
	
out:
	/*
	 * Free the list which be used to handle the address.
	 */
	xl = LIST_FIRST(&xladdr_head);
	while (xl != NULL) {
		xl_tmp = LIST_NEXT(xl, xladdr_entries);
		free(xl);
		xl = xl_tmp;
	}
	
	xr = LIST_FIRST(&xraddr_head);
	while (xr != NULL) {
		xr_tmp = LIST_NEXT(xr, xraddr_entries);
		free(xr);
		xr = xr_tmp;
	}
}

#ifdef SCTP_DEBUG
uint32_t sctp_pdup[64];
int sctp_pcnt = 0;
#endif

static void
sctp_process_inpcb(struct xsctp_inpcb *xinpcb, const char *name,
    char *buf, const size_t buflen, size_t *offset)
{
	int offset_backup, indent = 0, xladdr_total = 0, is_listening = 0;
	static int first = 1;
	char *tname;
	struct xsctp_tcb *xstcb;
	struct xsctp_laddr *xladdr;
	struct sockaddr *sa;
#ifdef INET6
	struct sockaddr_in6 *in6;
#endif

	if ((xinpcb->flags & SCTP_PCB_FLAGS_TCPTYPE) ==
	    SCTP_PCB_FLAGS_TCPTYPE && xinpcb->maxqlen > 0)
		is_listening = 1;

	if (!Lflag && !is_listening &&
	    !(xinpcb->flags & SCTP_PCB_FLAGS_CONNECTED)) {
#ifdef SCTP_DEBUG
		int i, found = 0;

		for (i = 0; i < sctp_pcnt; i++) {
			if (sctp_pdup[i] == xinpcb->flags) {
				found = 1;
				break;
			}
		}
		if (!found) {
			sctp_pdup[sctp_pcnt++] = xinpcb->flags;
			if (sctp_pcnt >= 64)
				sctp_pcnt = 0;
			printf("[0x%08x]", xinpcb->flags);
		}
#endif
		offset_backup = *offset;
		if (!sctp_skip_xinpcb_ifneed(buf, buflen, offset))
			return;
		*offset = offset_backup;
	}

	if (first) {
		if (!Lflag) {
			printf("Active SCTP associations");
			if (aflag)
				printf(" (including servers)");
		} else
			printf("Current listen queue sizes (qlen/maxqlen)");
		putchar('\n');
		if (Aflag)
			printf("%-8.8s ", "Socket");
		if (Lflag)
			printf("%-5.5s %-5.5s %-8.8s %-22.22s\n",
			    "Proto", "Type", "Listen", "Local Address");
		else
			printf((Aflag && !Wflag) ?
			    "%-5.5s %-5.5s %-18.18s %-18.18s %s\n" :
			    "%-5.5s %-5.5s %-22.22s %-22.22s %s\n",
			    "Proto", "Type",
			    "Local Address", "Foreign Address",
			    "(state)");
		first = 0;
	}
	if (Lflag && xinpcb->maxqlen == 0) {
		(int)sctp_skip_xinpcb_ifneed(buf, buflen, offset);
		return;
	}
	if (Aflag)
		printf("%8lx ", (u_long)xinpcb);

	printf("%-5.5s ", name);
	
	if (xinpcb->flags & SCTP_PCB_FLAGS_TCPTYPE)
		tname = "1to1";
	else if (xinpcb->flags & SCTP_PCB_FLAGS_UDPTYPE)
		tname = "1toN";
	else
		return;
	
	printf("%-5.5s ", tname);

	if (Lflag) {
		char buf1[9];
		
		snprintf(buf1, 9, "%hu/%hu", xinpcb->qlen, xinpcb->maxqlen);
		printf("%-8.8s ", buf1);
	}
	/*
	 * process the local address.  This routine are used for Lflag.
	 */
	while (*offset < buflen) {
		xladdr = (struct xsctp_laddr *)(buf + *offset);
		*offset += sizeof(struct xsctp_laddr);
		if (xladdr->last == 1)
			break;

		if (!Lflag && !is_listening)
			continue;

		if (xladdr_total != 0)
			putchar('\n');
		if (xladdr_total > 0)
			printf((Lflag) ?
			    "%-20.20s " : "%-11.11s ", " ");

		sa = &(xladdr->address.sa);
		if ((sa->sa_family) == AF_INET)
			inetprint(&((struct sockaddr_in *)sa)->sin_addr, 
			    htons(xinpcb->local_port), name, numeric_port);
#ifdef INET6
		else {
			in6 = (struct sockaddr_in6 *)sa;
			inet6print(&in6->sin6_addr,
			    htons(xinpcb->local_port), name, numeric_port);
		}
#endif

		if (!Lflag && xladdr_total == 0 && is_listening == 1)
			printf("%-22.22s LISTEN", " ");

		xladdr_total++;
	}

	xstcb = (struct xsctp_tcb *)(buf + *offset);
	*offset += sizeof(struct xsctp_tcb);
	while (xstcb->last == 0 && *offset < buflen) {
		sctp_process_tcb(xstcb, name, buf, buflen, offset, &indent);
		indent++;
		xstcb = (struct xsctp_tcb *)(buf + *offset);
		*offset += sizeof(struct xsctp_tcb);
	}

	putchar('\n');
}

/*
 * Print a summary of SCTP connections related to an Internet
 * protocol.
 */
void
sctp_protopr(u_long off __unused,
    const char *name, int af1, int proto)
{
	char *buf;
	const char *mibvar = "net.inet.sctp.assoclist";
	size_t offset = 0;
	size_t len = 0;
	struct xsctp_inpcb *xinpcb;
	
	if (proto != IPPROTO_SCTP)
		return;

	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		if (errno != ENOENT)
			warn("sysctl: %s", mibvar);
		return;
	}
	if ((buf = malloc(len)) == 0) {
		warnx("malloc %lu bytes", (u_long)len);
		return;
	}
	if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
		warn("sysctl: %s", mibvar);
		free(buf);
		return;
	}

	xinpcb = (struct xsctp_inpcb *)(buf + offset);
	offset += sizeof(struct xsctp_inpcb);
	while (xinpcb->last == 0 && offset < len) {
		sctp_process_inpcb(xinpcb, name, buf, (const size_t)len,
		    &offset);

		xinpcb = (struct xsctp_inpcb *)(buf + offset);
		offset += sizeof(struct xsctp_inpcb);
	}

	free(buf);
}

static void
sctp_statesprint(uint32_t state)
{
	int idx;

	switch (state) {
	case SCTP_STATE_COOKIE_WAIT:
		idx = NETSTAT_SCTP_STATES_COOKIE_WAIT;
		break;
	case SCTP_STATE_COOKIE_ECHOED:
		idx = NETSTAT_SCTP_STATES_COOKIE_ECHOED;
		break;
	case SCTP_STATE_OPEN:
		idx = NETSTAT_SCTP_STATES_ESTABLISHED;
		break;
	case SCTP_STATE_SHUTDOWN_SENT:
		idx = NETSTAT_SCTP_STATES_SHUTDOWN_SENT;
		break;
	case SCTP_STATE_SHUTDOWN_RECEIVED:
		idx = NETSTAT_SCTP_STATES_SHUTDOWN_RECEIVED;
		break;
	case SCTP_STATE_SHUTDOWN_ACK_SENT:
		idx = NETSTAT_SCTP_STATES_SHUTDOWN_ACK_SENT;
		break;
	case SCTP_STATE_SHUTDOWN_PENDING:
		idx = NETSTAT_SCTP_STATES_SHUTDOWN_PENDING;
		break;
	default:
		printf("UNKNOWN 0x%08x", state);
		return;
	}

	printf("%s", sctpstates[idx]);
}

/*
 * Dump SCTP statistics structure.
 */
void
sctp_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct sctpstat sctpstat, zerostat;
	size_t len = sizeof(sctpstat);

	if (live) {
		if (zflag)
			memset(&zerostat, 0, len);
		if (sysctlbyname("net.inet.sctp.stats", &sctpstat, &len,
		    zflag ? &zerostat : NULL, zflag ? len : 0) < 0) {
			warn("sysctl: net.inet.sctp.stats");
			return;
		}
	} else
		kread(off, &sctpstat, len);

	printf ("%s:\n", name);

#define	p(f, m) if (sctpstat.f || sflag <= 1) \
    printf(m, sctpstat.f, plural(sctpstat.f))
#define	p1a(f, m) if (sctpstat.f || sflag <= 1) \
    printf(m, sctpstat.f)
#define	p2(f1, f2, m) if (sctpstat.f1 || sctpstat.f2 || sflag <= 1) \
    printf(m, sctpstat.f1, plural(sctpstat.f1), sctpstat.f2, plural(sctpstat.f2))
#define	p2a(f1, f2, m) if (sctpstat.f1 || sctpstat.f2 || sflag <= 1) \
    printf(m, sctpstat.f1, plural(sctpstat.f1), sctpstat.f2)
#define	p3(f, m) if (sctpstat.f || sflag <= 1) \
    printf(m, sctpstat.f, plurales(sctpstat.f))

	/*
	 * input statistics
	 */
	p(sctps_recvpackets, "\t%lu input packet%s\n");
	p(sctps_recvdatagrams, "\t\t%lu datagram%s\n");
	p(sctps_recvpktwithdata, "\t\t%lu packet%s that had data\n");
	p(sctps_recvsacks, "\t\t%lu input SACK chunk%s\n");
	p(sctps_recvdata, "\t\t%lu input DATA chunk%s\n");
	p(sctps_recvdupdata, "\t\t%lu duplicate DATA chunk%s\n");
	p(sctps_recvheartbeat, "\t\t%lu input HB chunk%s\n");
	p(sctps_recvheartbeatack, "\t\t%lu HB-ACK chunk%s\n");
	p(sctps_recvecne, "\t\t%lu input ECNE chunk%s\n");
	p(sctps_recvauth, "\t\t%lu input AUTH chunk%s\n");
	p(sctps_recvauthmissing, "\t\t%lu chunk%s missing AUTH\n");
	p(sctps_recvivalhmacid, "\t\t%lu invalid HMAC id%s received\n");
	p(sctps_recvivalkeyid, "\t\t%lu invalid %secret ids received\n");
	p1a(sctps_recvauthfailed, "\t\t%lu auth failed\n");
	p(sctps_recvexpress, "\t\t%lu fa%st path receives all one chunk\n");
	p(sctps_recvexpressm, "\t\t%lu fa%st path multi-part data\n");

	/*
	 * output statistics
	 */
	p(sctps_sendpackets, "\t%lu output packet%s\n");
	p(sctps_sendsacks, "\t\t%lu output SACK%s\n");
	p(sctps_senddata, "\t\t%lu output DATA chunk%s\n");
	p(sctps_sendretransdata, "\t\t%lu retran%smitted DATA chunks\n");
	p(sctps_sendfastretrans, "\t\t%lu fa%st retransmitted DATA chunks\n");
	p(sctps_sendmultfastretrans, "\t\t%lu FR'%s that happened more "
	    "than once to same chunk.\n");
	p(sctps_sendheartbeat, "\t\t%lu intput HB chunk%s\n");
	p(sctps_sendecne, "\t\t%lu output ECNE chunk%s\n");
	p(sctps_sendauth, "\t\t%lu output AUTH chunk%s\n");
	p1a(sctps_senderrors, "\t\t%lu ip_output error counter\n");

	/*
	 * PCKDROPREP statistics
	 */
	printf("\tPacket drop statistics:\n");
	p1a(sctps_pdrpfmbox, "\t\t%lu from middle box\n");
	p(sctps_pdrpfehos, "\t\t%lu from end ho%st\n");
	p1a(sctps_pdrpmbda, "\t\t%lu with data\n");
	p1a(sctps_pdrpmbct, "\t\t%lu non-data, non-endhost\n");
	p(sctps_pdrpbwrpt, "\t\t%lu non-endho%st, bandwidth rep only\n");
	p1a(sctps_pdrpcrupt, "\t\t%lu not enough for chunk header\n");
	p1a(sctps_pdrpnedat, "\t\t%lu not enough data to confirm\n");
	p(sctps_pdrppdbrk, "\t\t%lu where proce%ss_chunk_drop said break\n");
	p1a(sctps_pdrptsnnf, "\t\t%lu failed to find TSN\n");
	p(sctps_pdrpdnfnd, "\t\t%lu attempt rever%se TSN lookup\n");
	p(sctps_pdrpdiwnp, "\t\t%lu e-ho%st confirms zero-rwnd\n");
	p(sctps_pdrpdizrw, "\t\t%lu midbox confirm%s no space\n");
	p1a(sctps_pdrpbadd, "\t\t%lu data did not match TSN\n");
	p(sctps_pdrpmark, "\t\t%lu TSN'%s marked for Fast Retran\n");

	/*
	 * Timeouts
	 */
	printf("\tTimeouts:\n");
	p(sctps_timoiterator, "\t\t%lu iterator timer%s fired\n");
	p(sctps_timodata, "\t\t%lu T3 data time out%s\n");
	p(sctps_timowindowprobe, "\t\t%lu window probe (T3) timer%s fired\n");
	p(sctps_timoinit, "\t\t%lu INIT timer%s fired\n");
	p(sctps_timosack, "\t\t%lu %sack timers fired\n");
	p(sctps_timoshutdown, "\t\t%lu %shutdown timers fired\n");
	p(sctps_timoheartbeat, "\t\t%lu heartbeat timer%s fired\n");
	p1a(sctps_timocookie, "\t\t%lu a cookie timeout fired\n");
	p1a(sctps_timosecret, "\t\t%lu an endpoint changed its cookie"
	    "secret\n");
	p(sctps_timopathmtu, "\t\t%lu PMTU timer%s fired\n");
	p(sctps_timoshutdownack, "\t\t%lu %shutdown ack timers fired\n");
	p(sctps_timoshutdownguard, "\t\t%lu %shutdown guard timers fired\n");
	p(sctps_timostrmrst, "\t\t%lu %stream reset timers fired\n");
	p(sctps_timoearlyfr, "\t\t%lu early FR timer%s fired\n");
	p1a(sctps_timoasconf, "\t\t%lu an asconf timer fired\n");
	p1a(sctps_timoautoclose, "\t\t%lu auto close timer fired\n");
	p(sctps_timoassockill, "\t\t%lu a%soc free timers expired\n");
	p(sctps_timoinpkill, "\t\t%lu inp free timer%s expired\n");

#if 0
	/*
	 * Early fast retransmission counters
	 */
	p(sctps_earlyfrstart, "\t%lu TODO:%sctps_earlyfrstart\n");
	p(sctps_earlyfrstop, "\t%lu TODO:sctp%s_earlyfrstop\n");
	p(sctps_earlyfrmrkretrans, "\t%lu TODO:%sctps_earlyfrmrkretrans\n");
	p(sctps_earlyfrstpout, "\t%lu TODO:%sctps_earlyfrstpout\n");
	p(sctps_earlyfrstpidsck1, "\t%lu TODO:%sctps_earlyfrstpidsck1\n");
	p(sctps_earlyfrstpidsck2, "\t%lu TODO:%sctps_earlyfrstpidsck2\n");
	p(sctps_earlyfrstpidsck3, "\t%lu TODO:%sctps_earlyfrstpidsck3\n");
	p(sctps_earlyfrstpidsck4, "\t%lu TODO:%sctps_earlyfrstpidsck4\n");
	p(sctps_earlyfrstrid, "\t%lu TODO:%sctps_earlyfrstrid\n");
	p(sctps_earlyfrstrout, "\t%lu TODO:%sctps_earlyfrstrout\n");
	p(sctps_earlyfrstrtmr, "\t%lu TODO:%sctps_earlyfrstrtmr\n");
#endif

	/*
	 * Others
	 */
	p(sctps_hdrops, "\t%lu packet %shorter than header\n");
	p(sctps_badsum, "\t%lu check%sum error\n");
	p1a(sctps_noport, "\t%lu no endpoint for port\n");
	p1a(sctps_badvtag, "\t%lu bad v-tag\n");
	p1a(sctps_badsid, "\t%lu bad SID\n");
	p1a(sctps_nomem, "\t%lu no memory\n");
	p1a(sctps_fastretransinrtt, "\t%lu number of multiple FR in a RTT "
	    "window\n");
#if 0
	p(sctps_markedretrans, "\t%lu TODO:%sctps_markedretrans\n");
#endif
	p(sctps_naglesent, "\t%lu RFC813 allowed %sending\n");
	p(sctps_naglequeued, "\t%lu RFC813 does not allow sending\n");
	p(sctps_maxburstqueued, "\t%lu max bur%st dosn't allow sending\n");
	p(sctps_ifnomemqueued, "\t%lu look ahead tell%s us no memory in "
	    "interface\n");
	p(sctps_windowprobed, "\t%lu numbers of window probe%s sent\n");
	p(sctps_lowlevelerr, "\t%lu time%s an output error to clamp "
	    "down on next user send.\n");
	p(sctps_lowlevelerrusr, "\t%lu time%s sctp_senderrors were "
	    "caused from a user\n");
	p(sctps_datadropchklmt, "\t%lu number of in data drop%s due to "
	    "chunk limit reached\n");
	p(sctps_datadroprwnd, "\t%lu number of in data drop%s due to rwnd "
	    "limit reached\n");
	p(sctps_ecnereducedcwnd, "\t%lu time%s a ECN reduced "
	    "the cwnd\n");
	p(sctps_vtagexpress, "\t%lu u%sed express lookup via vtag\n");
	p(sctps_vtagbogus, "\t%lu colli%sion in express lookup.\n");
	p(sctps_primary_randry, "\t%lu time%s the sender ran dry "
	    "of user data on primary\n");
	p1a(sctps_cmt_randry, "\t%lu same for above\n");
	p(sctps_slowpath_sack, "\t%lu sack%s the slow way\n");
	p(sctps_wu_sacks_sent, "\t%lu window update only %sacks sent\n");
	p(sctps_sends_with_flags, "\t%lu %sends with sinfo_flags !=0\n");
	p(sctps_sends_with_unord, "\t%lu unordered %sends\n");
	p(sctps_sends_with_eof, "\t%lu %sends with EOF flag set\n");
	p(sctps_sends_with_abort, "\t%lu %sends with ABORT flag set\n");
	p(sctps_protocol_drain_calls, "\t%lu time%s protocol drain called\n");
	p(sctps_protocol_drains_done, "\t%lu time%s we did a protocol "
	    "drain\n");
	p(sctps_read_peeks, "\t%lu time%s recv was called with peek\n");
	p(sctps_cached_chk, "\t%lu cached chunk%s used\n");
	p(sctps_cached_strmoq, "\t%lu cached %stream oq's used\n");
	p(sctps_left_abandon, "\t%lu unread me%ssage abandonded by close\n");
	p(sctps_send_burst_avoid, "\t%lu send bur%st avoidance, already "
	    "max burst inflight to net\n");
	p(sctps_send_cwnd_avoid, "\t%lu send cwnd full avoidance, already "
	    "max bur%st inflight to net\n");
	p(sctps_fwdtsn_map_over, "\t%lu number of map array over-run%s via "
	    "fwd-tsn's\n");

#undef p
#undef p1a
#undef p2
#undef p2a
#undef p3
}

#endif /* SCTP */
