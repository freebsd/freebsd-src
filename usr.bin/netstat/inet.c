/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
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
static char sccsid[] = "@(#)inet.c	5.15 (Berkeley) 6/18/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netdb.h>

#include <stdio.h>
#include <string.h>

struct	inpcb inpcb;
struct	tcpcb tcpcb;
struct	socket sockb;
extern	int Aflag;
extern	int aflag;
extern	int nflag;
extern	char *plural();

char	*inetname();

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
protopr(off, name)
	off_t off;
	char *name;
{
	struct inpcb cb;
	register struct inpcb *prev, *next;
	int istcp;
	static int first = 1;

	if (off == 0)
		return;
	istcp = strcmp(name, "tcp") == 0;
	kvm_read(off, (char *)&cb, sizeof (struct inpcb));
	inpcb = cb;
	prev = (struct inpcb *)off;
	if (inpcb.inp_next == (struct inpcb *)off)
		return;
	while (inpcb.inp_next != (struct inpcb *)off) {

		next = inpcb.inp_next;
		kvm_read((off_t)next, (char *)&inpcb, sizeof (inpcb));
		if (inpcb.inp_prev != prev) {
			printf("???\n");
			break;
		}
		if (!aflag &&
		  inet_lnaof(inpcb.inp_laddr) == INADDR_ANY) {
			prev = next;
			continue;
		}
		kvm_read((off_t)inpcb.inp_socket,
				(char *)&sockb, sizeof (sockb));
		if (istcp) {
			kvm_read((off_t)inpcb.inp_ppcb,
				(char *)&tcpcb, sizeof (tcpcb));
		}
		if (first) {
			printf("Active Internet connections");
			if (aflag)
				printf(" (including servers)");
			putchar('\n');
			if (Aflag)
				printf("%-8.8s ", "PCB");
			printf(Aflag ?
				"%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s\n" :
				"%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s\n",
				"Proto", "Recv-Q", "Send-Q",
				"Local Address", "Foreign Address", "(state)");
			first = 0;
		}
		if (Aflag)
			if (istcp)
				printf("%8x ", inpcb.inp_ppcb);
			else
				printf("%8x ", next);
		printf("%-5.5s %6d %6d ", name, sockb.so_rcv.sb_cc,
			sockb.so_snd.sb_cc);
		inetprint(&inpcb.inp_laddr, inpcb.inp_lport, name);
		inetprint(&inpcb.inp_faddr, inpcb.inp_fport, name);
		if (istcp) {
			if (tcpcb.t_state < 0 || tcpcb.t_state >= TCP_NSTATES)
				printf(" %d", tcpcb.t_state);
			else
				printf(" %s", tcpstates[tcpcb.t_state]);
		}
		putchar('\n');
		prev = next;
	}
}

/*
 * Dump TCP statistics structure.
 */
tcp_stats(off, name)
	off_t off;
	char *name;
{
	struct tcpstat tcpstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kvm_read(off, (char *)&tcpstat, sizeof (tcpstat));

#define	p(f, m)		printf(m, tcpstat.f, plural(tcpstat.f))
#define	p2(f1, f2, m)	printf(m, tcpstat.f1, plural(tcpstat.f1), tcpstat.f2, plural(tcpstat.f2))
  
	p(tcps_sndtotal, "\t%d packet%s sent\n");
	p2(tcps_sndpack,tcps_sndbyte,
		"\t\t%d data packet%s (%d byte%s)\n");
	p2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
		"\t\t%d data packet%s (%d byte%s) retransmitted\n");
	p2(tcps_sndacks, tcps_delack,
		"\t\t%d ack-only packet%s (%d delayed)\n");
	p(tcps_sndurg, "\t\t%d URG only packet%s\n");
	p(tcps_sndprobe, "\t\t%d window probe packet%s\n");
	p(tcps_sndwinup, "\t\t%d window update packet%s\n");
	p(tcps_sndctrl, "\t\t%d control packet%s\n");
	p(tcps_rcvtotal, "\t%d packet%s received\n");
	p2(tcps_rcvackpack, tcps_rcvackbyte, "\t\t%d ack%s (for %d byte%s)\n");
	p(tcps_rcvdupack, "\t\t%d duplicate ack%s\n");
	p(tcps_rcvacktoomuch, "\t\t%d ack%s for unsent data\n");
	p2(tcps_rcvpack, tcps_rcvbyte,
		"\t\t%d packet%s (%d byte%s) received in-sequence\n");
	p2(tcps_rcvduppack, tcps_rcvdupbyte,
		"\t\t%d completely duplicate packet%s (%d byte%s)\n");
	p2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
		"\t\t%d packet%s with some dup. data (%d byte%s duped)\n");
	p2(tcps_rcvoopack, tcps_rcvoobyte,
		"\t\t%d out-of-order packet%s (%d byte%s)\n");
	p2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
		"\t\t%d packet%s (%d byte%s) of data after window\n");
	p(tcps_rcvwinprobe, "\t\t%d window probe%s\n");
	p(tcps_rcvwinupd, "\t\t%d window update packet%s\n");
	p(tcps_rcvafterclose, "\t\t%d packet%s received after close\n");
	p(tcps_rcvbadsum, "\t\t%d discarded for bad checksum%s\n");
	p(tcps_rcvbadoff, "\t\t%d discarded for bad header offset field%s\n");
	p(tcps_rcvshort, "\t\t%d discarded because packet too short\n");
	p(tcps_connattempt, "\t%d connection request%s\n");
	p(tcps_accepts, "\t%d connection accept%s\n");
	p(tcps_connects, "\t%d connection%s established (including accepts)\n");
	p2(tcps_closed, tcps_drops,
		"\t%d connection%s closed (including %d drop%s)\n");
	p(tcps_conndrops, "\t%d embryonic connection%s dropped\n");
	p2(tcps_rttupdated, tcps_segstimed,
		"\t%d segment%s updated rtt (of %d attempt%s)\n");
	p(tcps_rexmttimeo, "\t%d retransmit timeout%s\n");
	p(tcps_timeoutdrop, "\t\t%d connection%s dropped by rexmit timeout\n");
	p(tcps_persisttimeo, "\t%d persist timeout%s\n");
	p(tcps_keeptimeo, "\t%d keepalive timeout%s\n");
	p(tcps_keepprobe, "\t\t%d keepalive probe%s sent\n");
	p(tcps_keepdrops, "\t\t%d connection%s dropped by keepalive\n");
#undef p
#undef p2
}

/*
 * Dump UDP statistics structure.
 */
udp_stats(off, name)
	off_t off;
	char *name;
{
	struct udpstat udpstat;

	if (off == 0)
		return;
	kvm_read(off, (char *)&udpstat, sizeof (udpstat));
	printf("%s:\n\t%u incomplete header%s\n", name,
		udpstat.udps_hdrops, plural(udpstat.udps_hdrops));
	printf("\t%u bad data length field%s\n",
		udpstat.udps_badlen, plural(udpstat.udps_badlen));
	printf("\t%u bad checksum%s\n",
		udpstat.udps_badsum, plural(udpstat.udps_badsum));
}

/*
 * Dump IP statistics structure.
 */
ip_stats(off, name)
	off_t off;
	char *name;
{
	struct ipstat ipstat;

	if (off == 0)
		return;
	kvm_read(off, (char *)&ipstat, sizeof (ipstat));
	printf("%s:\n\t%u total packets received\n", name,
		ipstat.ips_total);
	printf("\t%u bad header checksum%s\n",
		ipstat.ips_badsum, plural(ipstat.ips_badsum));
	printf("\t%u with size smaller than minimum\n", ipstat.ips_tooshort);
	printf("\t%u with data size < data length\n", ipstat.ips_toosmall);
	printf("\t%u with header length < data size\n", ipstat.ips_badhlen);
	printf("\t%u with data length < header length\n", ipstat.ips_badlen);
	printf("\t%u fragment%s received\n",
		ipstat.ips_fragments, plural(ipstat.ips_fragments));
	printf("\t%u fragment%s dropped (dup or out of space)\n",
		ipstat.ips_fragdropped, plural(ipstat.ips_fragdropped));
	printf("\t%u fragment%s dropped after timeout\n",
		ipstat.ips_fragtimeout, plural(ipstat.ips_fragtimeout));
	printf("\t%u packet%s forwarded\n",
		ipstat.ips_forward, plural(ipstat.ips_forward));
	printf("\t%u packet%s not forwardable\n",
		ipstat.ips_cantforward, plural(ipstat.ips_cantforward));
	printf("\t%u redirect%s sent\n",
		ipstat.ips_redirectsent, plural(ipstat.ips_redirectsent));
}

static	char *icmpnames[] = {
	"echo reply",
	"#1",
	"#2",
	"destination unreachable",
	"source quench",
	"routing redirect",
	"#6",
	"#7",
	"echo",
	"#9",
	"#10",
	"time exceeded",
	"parameter problem",
	"time stamp",
	"time stamp reply",
	"information request",
	"information request reply",
	"address mask request",
	"address mask reply",
};

/*
 * Dump ICMP statistics.
 */
icmp_stats(off, name)
	off_t off;
	char *name;
{
	struct icmpstat icmpstat;
	register int i, first;

	if (off == 0)
		return;
	kvm_read(off, (char *)&icmpstat, sizeof (icmpstat));
	printf("%s:\n\t%u call%s to icmp_error\n", name,
		icmpstat.icps_error, plural(icmpstat.icps_error));
	printf("\t%u error%s not generated 'cuz old message was icmp\n",
		icmpstat.icps_oldicmp, plural(icmpstat.icps_oldicmp));
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %u\n", icmpnames[i],
				icmpstat.icps_outhist[i]);
		}
	printf("\t%u message%s with bad code fields\n",
		icmpstat.icps_badcode, plural(icmpstat.icps_badcode));
	printf("\t%u message%s < minimum length\n",
		icmpstat.icps_tooshort, plural(icmpstat.icps_tooshort));
	printf("\t%u bad checksum%s\n",
		icmpstat.icps_checksum, plural(icmpstat.icps_checksum));
	printf("\t%u message%s with bad length\n",
		icmpstat.icps_badlen, plural(icmpstat.icps_badlen));
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %u\n", icmpnames[i],
				icmpstat.icps_inhist[i]);
		}
	printf("\t%u message response%s generated\n",
		icmpstat.icps_reflect, plural(icmpstat.icps_reflect));
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
inetprint(in, port, proto)
	register struct in_addr *in;
	u_short port; 
	char *proto;
{
	struct servent *sp = 0;
	char line[80], *cp, *index();
	int width;

	sprintf(line, "%.*s.", (Aflag && !nflag) ? 12 : 16, inetname(*in));
	cp = index(line, '\0');
	if (!nflag && port)
		sp = getservbyport((int)port, proto);
	if (sp || port == 0)
		sprintf(cp, "%.8s", sp ? sp->s_name : "*");
	else
		sprintf(cp, "%d", ntohs((u_short)port));
	width = Aflag ? 18 : 22;
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give 
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(in)
	struct in_addr in;
{
	register char *cp;
	static char line[50];
	struct hostent *hp;
	struct netent *np;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag && in.s_addr != INADDR_ANY) {
		int net = inet_netof(in);
		int lna = inet_lnaof(in);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)&in, sizeof (in), AF_INET);
			if (hp) {
				if ((cp = index(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = 0;
				cp = hp->h_name;
			}
		}
	}
	if (in.s_addr == INADDR_ANY)
		strcpy(line, "*");
	else if (cp)
		strcpy(line, cp);
	else {
		in.s_addr = ntohl(in.s_addr);
#define C(x)	((x) & 0xff)
		sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
	}
	return (line);
}
