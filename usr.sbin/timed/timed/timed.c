/*-
 * Copyright (c) 1985, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1985, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)timed.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#ifdef sgi
#ident "$Revision: 1.3 $"
#endif /* sgi */

#define TSPTYPES
#include "globals.h"
#include <net/if.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <setjmp.h>
#include "pathnames.h"
#include <math.h>
#include <sys/types.h>
#include <sys/times.h>
#ifdef sgi
#include <unistd.h>
#include <sys/syssgi.h>
#include <sys/schedctl.h>
#endif /* sgi */

int trace = 0;
int sock, sock_raw = -1;
int status = 0;
u_short sequence;			/* sequence number */
long delay1;
long delay2;

int nslavenets;				/* nets were I could be a slave */
int nmasternets;			/* nets were I could be a master */
int nignorednets;			/* ignored nets */
int nnets;				/* nets I am connected to */

FILE *fd;				/* trace file FD */

jmp_buf jmpenv;

struct netinfo *nettab = 0;
struct netinfo *slavenet;
int Mflag;
int justquit = 0;
int debug;

static struct nets {
	char	*name;
	long	net;
	struct nets *next;
} *nets = 0;

struct hosttbl hosttbl[NHOSTS+1];	/* known hosts */

static struct goodhost {		/* hosts that we trust */
	char	name[MAXHOSTNAMELEN+1];
	struct goodhost *next;
	char	perm;
} *goodhosts;

static char *goodgroup;			/* net group of trusted hosts */
static void checkignorednets __P((void));
static void pickslavenet __P((struct netinfo *));
static void add_good_host __P((char *, int));

#ifdef sgi
char *timetrim_fn;
char *timetrim_wpat = "long timetrim = %ld;\ndouble tot_adj = %.0f;\ndouble tot_ticks = %.0f;\n/* timed version 2 */\n";
char *timetrim_rpat = "long timetrim = %ld;\ndouble tot_adj = %lf;\ndouble tot_ticks = %lf;";
long timetrim;
double tot_adj, hr_adj;			/* totals in nsec */
double tot_ticks, hr_ticks;

int bufspace = 60*1024;
#endif


/*
 * The timedaemons synchronize the clocks of hosts in a local area network.
 * One daemon runs as master, all the others as slaves. The master
 * performs the task of computing clock differences and sends correction
 * values to the slaves.
 * Slaves start an election to choose a new master when the latter disappears
 * because of a machine crash, network partition, or when killed.
 * A resolution protocol is used to kill all but one of the masters
 * that happen to exist in segments of a partitioned network when the
 * network partition is fixed.
 *
 * Authors: Riccardo Gusella & Stefano Zatti
 *
 * overhauled at Silicon Graphics
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int on;
	int ret;
	int nflag, iflag;
	struct timeval ntime;
	struct servent *srvp;
	char buf[BUFSIZ], *cp, *cplim;
	struct ifconf ifc;
	struct ifreq ifreq, ifreqf, *ifr;
	register struct netinfo *ntp;
	struct netinfo *ntip;
	struct netinfo *savefromnet;
	struct netent *nentp;
	struct nets *nt;
	struct sockaddr_in server;
	u_short port;
	char c;
	extern char *optarg;
	extern int optind, opterr;
#ifdef sgi
	FILE *timetrim_st;
#endif

#define	IN_MSG "timed: -i and -n make no sense together\n"
#ifdef sgi
	struct tms tms;
#define USAGE "timed: [-dtM] [-i net|-n net] [-F host1 host2 ...] [-G netgp] [-P trimfile]\n"
#else
#ifdef HAVENIS
#define USAGE "timed: [-dtM] [-i net|-n net] [-F host1 host2 ...] [-G netgp]\n"
#else
#define USAGE "timed: [-dtM] [-i net|-n net] [-F host1 host2 ...]\n"
#endif /* HAVENIS */
#endif /* sgi */

#ifdef lint
	ntip = NULL;
#endif

	on = 1;
	nflag = OFF;
	iflag = OFF;

#ifdef sgi
	if (0 > syssgi(SGI_GETTIMETRIM, &timetrim)) {
		perror("timed: syssgi(GETTIMETRIM)");
		timetrim = 0;
	}
	tot_ticks = hr_ticks = times(&tms);
#endif /* sgi */

	opterr = 0;
	while ((c = getopt(argc, argv, "Mtdn:i:F:G:P:")) != -1) {
		switch (c) {
		case 'M':
			Mflag = 1;
			break;

		case 't':
			trace = 1;
			break;

		case 'n':
			if (iflag) {
				fprintf(stderr, IN_MSG);
				exit(1);
			} else {
				nflag = ON;
				addnetname(optarg);
			}
			break;

		case 'i':
			if (nflag) {
				fprintf(stderr, IN_MSG);
				exit(1);
			} else {
				iflag = ON;
				addnetname(optarg);
			}
			break;

		case 'F':
			add_good_host(optarg,1);
			while (optind < argc && argv[optind][0] != '-')
				add_good_host(argv[optind++], 1);
			break;

		case 'd':
			debug = 1;
			break;
		case 'G':
			if (goodgroup != 0) {
				fprintf(stderr,"timed: only one net group\n");
				exit(1);
			}
			goodgroup = optarg;
			break;
#ifdef sgi
		case 'P':
			timetrim_fn = optarg;
			timetrim_st = fopen(timetrim_fn, "r+");
			if (0 == timetrim_st) {
				if (errno != ENOENT) {
					(void)fprintf(stderr,"timed: ");
					perror(timetrim_fn);
					timetrim_fn = 0;
				}
			} else {
				int i;
				long trim;
				double adj, ticks;

				i = fscanf(timetrim_st, timetrim_rpat,
					   &trim, &adj, &ticks);
				if (i < 1
				    || trim > MAX_TRIM
				    || trim < -MAX_TRIM
				    || i == 2
				    || (i == 3
					&& trim != rint(adj*CLK_TCK/ticks))) {
					if (trace && i != EOF)
						(void)fprintf(stderr,
				    "timed: unrecognized contents in %s\n",
							      timetrim_fn);
				} else {
					if (0 > syssgi(SGI_SETTIMETRIM,
						       trim)) {
					 perror("timed: syssgi(SETTIMETRIM)");
					} else {
						timetrim = trim;
					}
					if (i == 3) {
						tot_adj = adj;
						tot_ticks -= ticks;
					}
				}
				(void)fclose(timetrim_st);
			}
			break;
#endif /* sgi */

		default:
			fprintf(stderr, USAGE);
			exit(1);
			break;
		}
	}
	if (optind < argc) {
		fprintf(stderr, USAGE);
		exit(1);
	}

	/* If we care about which machine is the master, then we must
	 *	be willing to be a master
	 */
	if (0 != goodgroup || 0 != goodhosts)
		Mflag = 1;

	if (gethostname(hostname, sizeof(hostname) - 1) < 0) {
		perror("gethostname");
		exit(1);
	}
	self.l_bak = &self;
	self.l_fwd = &self;
	self.h_bak = &self;
	self.h_fwd = &self;
	self.head = 1;
	self.good = 1;

	if (goodhosts != 0)		/* trust ourself */
		add_good_host(hostname,1);

	srvp = getservbyname("timed", "udp");
	if (srvp == 0) {
		fprintf(stderr, "unknown service 'timed/udp'\n");
		exit(1);
	}
	port = srvp->s_port;
	bzero(&server, sizeof(struct sockaddr_in));
	server.sin_port = srvp->s_port;
	server.sin_family = AF_INET;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&on,
							sizeof(on)) < 0) {
		perror("setsockopt");
		exit(1);
	}
	if (bind(sock, (struct sockaddr*)&server, sizeof(server))) {
		if (errno == EADDRINUSE)
			fprintf(stderr,"timed: time daemon already running\n");
		else
			perror("bind");
		exit(1);
	}
#ifdef sgi
	/*
	 * handle many slaves with our buffer
	 */
	if (0 > setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufspace,
			 sizeof(bufspace))) {
		perror("setsockopt");
		exit(1);
	}
#endif /* sgi */

	/* choose a unique seed for random number generation */
	(void)gettimeofday(&ntime, 0);
	srandom(ntime.tv_sec + ntime.tv_usec);

	sequence = random();     /* initial seq number */

#ifndef sgi
	/* rounds kernel variable time to multiple of 5 ms. */
	ntime.tv_sec = 0;
	ntime.tv_usec = -((ntime.tv_usec/1000) % 5) * 1000;
	(void)adjtime(&ntime, (struct timeval *)0);
#endif /* sgi */

	for (nt = nets; nt; nt = nt->next) {
		nentp = getnetbyname(nt->name);
		if (nentp == 0) {
			nt->net = inet_network(nt->name);
			if (nt->net != INADDR_NONE)
				nentp = getnetbyaddr(nt->net, AF_INET);
		}
		if (nentp != 0) {
			nt->net = nentp->n_net;
		} else if (nt->net == INADDR_NONE) {
			fprintf(stderr, "timed: unknown net %s\n", nt->name);
			exit(1);
		} else if (nt->net == INADDR_ANY) {
			fprintf(stderr, "timed: bad net %s\n", nt->name);
			exit(1);
		} else {
			fprintf(stderr,
				"timed: warning: %s unknown in /etc/networks\n",
				nt->name);
		}

		if (0 == (nt->net & 0xff000000))
		    nt->net <<= 8;
		if (0 == (nt->net & 0xff000000))
		    nt->net <<= 8;
		if (0 == (nt->net & 0xff000000))
		    nt->net <<= 8;
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0) {
		perror("timed: get interface configuration");
		exit(1);
	}
	ntp = NULL;
#ifdef sgi
#define size(p)	(sizeof(*ifr) - sizeof(ifr->ifr_name))  /* XXX hack. kludge */
#else
#define size(p)	max((p).sa_len, sizeof(p))
#endif
	cplim = buf + ifc.ifc_len; /*skip over if's with big ifr_addr's */
	for (cp = buf; cp < cplim;
			cp += sizeof (ifr->ifr_name) + size(ifr->ifr_addr)) {
		ifr = (struct ifreq *)cp;
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		if (!ntp)
			ntp = (struct netinfo*)malloc(sizeof(struct netinfo));
		bzero(ntp,sizeof(*ntp));
		ntp->my_addr=((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;
		ntp->status = NOMASTER;
		ifreq = *ifr;
		ifreqf = *ifr;

		if (ioctl(sock, SIOCGIFFLAGS, (char *)&ifreqf) < 0) {
			perror("get interface flags");
			continue;
		}
		if ((ifreqf.ifr_flags & IFF_UP) == 0)
			continue;
		if ((ifreqf.ifr_flags & IFF_BROADCAST) == 0 &&
		    (ifreqf.ifr_flags & IFF_POINTOPOINT) == 0) {
			continue;
		}


		if (ioctl(sock, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
			perror("get netmask");
			continue;
		}
		ntp->mask = ((struct sockaddr_in *)
			&ifreq.ifr_addr)->sin_addr.s_addr;

		if (ifreqf.ifr_flags & IFF_BROADCAST) {
			if (ioctl(sock, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
				perror("get broadaddr");
				continue;
			}
			ntp->dest_addr = *(struct sockaddr_in *)&ifreq.ifr_broadaddr;
			/* What if the broadcast address is all ones?
			 * So we cannot just mask ntp->dest_addr.  */
			ntp->net = ntp->my_addr;
			ntp->net.s_addr &= ntp->mask;
		} else {
			if (ioctl(sock, SIOCGIFDSTADDR,
						(char *)&ifreq) < 0) {
				perror("get destaddr");
				continue;
			}
			ntp->dest_addr = *(struct sockaddr_in *)&ifreq.ifr_dstaddr;
			ntp->net = ntp->dest_addr.sin_addr;
		}

		ntp->dest_addr.sin_port = port;

		for (nt = nets; nt; nt = nt->next) {
			if (ntp->net.s_addr == htonl(nt->net))
				break;
		}
		if (nflag && !nt || iflag && nt)
			continue;

		ntp->next = NULL;
		if (nettab == NULL) {
			nettab = ntp;
		} else {
			ntip->next = ntp;
		}
		ntip = ntp;
		ntp = NULL;
	}
	if (ntp)
		(void) free((char *)ntp);
	if (nettab == NULL) {
		fprintf(stderr, "timed: no network usable\n");
		exit(1);
	}


#ifdef sgi
	(void)schedctl(RENICE,0,10);	   /* run fast to get good time */

	/* ticks to delay before responding to a broadcast */
	delay1 = casual(0, CLK_TCK/10);
#else

	/* microseconds to delay before responding to a broadcast */
	delay1 = casual(1, 100*1000);
#endif /* sgi */

	/* election timer delay in secs. */
	delay2 = casual(MINTOUT, MAXTOUT);


#ifdef sgi
	(void)_daemonize(debug ? _DF_NOFORK|_DF_NOCHDIR : 0, sock, -1, -1);
#else
	if (!debug)
		daemon(debug, 0);
#endif /* sgi */

	if (trace)
		traceon();
	openlog("timed", LOG_CONS|LOG_PID, LOG_DAEMON);

	/*
	 * keep returning here
	 */
	ret = setjmp(jmpenv);
	savefromnet = fromnet;
	setstatus();

	if (Mflag) {
		switch (ret) {

		case 0:
			checkignorednets();
			pickslavenet(0);
			break;
		case 1:
			/* Just lost our master */
			if (slavenet != 0)
				slavenet->status = election(slavenet);
			if (!slavenet || slavenet->status == MASTER) {
				checkignorednets();
				pickslavenet(0);
			} else {
				makeslave(slavenet);	/* prune extras */
			}
			break;

		case 2:
			/* Just been told to quit */
			justquit = 1;
			pickslavenet(savefromnet);
			break;
		}

		setstatus();
		if (!(status & MASTER) && sock_raw != -1) {
			/* sock_raw is not being used now */
			(void)close(sock_raw);
			sock_raw = -1;
		}

		if (status == MASTER)
			master();
		else
			slave();

	} else {
		if (sock_raw != -1) {
			(void)close(sock_raw);
			sock_raw = -1;
		}

		if (ret) {
			/* we just lost our master or were told to quit */
			justquit = 1;
		}
		for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
			if (ntp->status == MASTER)
				rmnetmachs(ntp);
				ntp->status = NOMASTER;
		}
		checkignorednets();
		pickslavenet(0);
		setstatus();

		slave();
	}
	/* NOTREACHED */
#ifdef lint
	return(0);
#endif
}

/*
 * suppress an upstart, untrustworthy, self-appointed master
 */
void
suppress(addr, name,net)
	struct sockaddr_in *addr;
	char *name;
	struct netinfo *net;
{
	struct sockaddr_in tgt;
	char tname[MAXHOSTNAMELEN];
	struct tsp msg;
	static struct timeval wait;

	if (trace)
		fprintf(fd, "suppress: %s\n", name);
	tgt = *addr;
	(void)strcpy(tname, name);

	while (0 != readmsg(TSP_ANY, ANYADDR, &wait, net)) {
		if (trace)
			fprintf(fd, "suppress:\tdiscarded packet from %s\n",
				    name);
	}

	syslog(LOG_NOTICE, "suppressing false master %s", tname);
	msg.tsp_type = TSP_QUIT;
	(void)strcpy(msg.tsp_name, hostname);
	(void)acksend(&msg, &tgt, tname, TSP_ACK, 0, 1);
}

void
lookformaster(ntp)
	struct netinfo *ntp;
{
	struct tsp resp, conflict, *answer;
	struct timeval ntime;
	char mastername[MAXHOSTNAMELEN];
	struct sockaddr_in masteraddr;

	get_goodgroup(0);
	ntp->status = SLAVE;

	/* look for master */
	resp.tsp_type = TSP_MASTERREQ;
	(void)strcpy(resp.tsp_name, hostname);
	answer = acksend(&resp, &ntp->dest_addr, ANYADDR,
			 TSP_MASTERACK, ntp, 0);
	if (answer != 0 && !good_host_name(answer->tsp_name)) {
		suppress(&from, answer->tsp_name, ntp);
		ntp->status = NOMASTER;
		answer = 0;
	}
	if (answer == 0) {
		/*
		 * Various conditions can cause conflict: races between
		 * two just started timedaemons when no master is
		 * present, or timedaemons started during an election.
		 * A conservative approach is taken.  Give up and became a
		 * slave, postponing election of a master until first
		 * timer expires.
		 */
		ntime.tv_sec = ntime.tv_usec = 0;
		answer = readmsg(TSP_MASTERREQ, ANYADDR, &ntime, ntp);
		if (answer != 0) {
			if (!good_host_name(answer->tsp_name)) {
				suppress(&from, answer->tsp_name, ntp);
				ntp->status = NOMASTER;
			}
			return;
		}

		ntime.tv_sec = ntime.tv_usec = 0;
		answer = readmsg(TSP_MASTERUP, ANYADDR, &ntime, ntp);
		if (answer != 0) {
			if (!good_host_name(answer->tsp_name)) {
				suppress(&from, answer->tsp_name, ntp);
				ntp->status = NOMASTER;
			}
			return;
		}

		ntime.tv_sec = ntime.tv_usec = 0;
		answer = readmsg(TSP_ELECTION, ANYADDR, &ntime, ntp);
		if (answer != 0) {
			if (!good_host_name(answer->tsp_name)) {
				suppress(&from, answer->tsp_name, ntp);
				ntp->status = NOMASTER;
			}
			return;
		}

		if (Mflag)
			ntp->status = MASTER;
		else
			ntp->status = NOMASTER;
		return;
	}

	ntp->status = SLAVE;
	(void)strcpy(mastername, answer->tsp_name);
	masteraddr = from;

	/*
	 * If network has been partitioned, there might be other
	 * masters; tell the one we have just acknowledged that
	 * it has to gain control over the others.
	 */
	ntime.tv_sec = 0;
	ntime.tv_usec = 300000;
	answer = readmsg(TSP_MASTERACK, ANYADDR, &ntime, ntp);
	/*
	 * checking also not to send CONFLICT to ack'ed master
	 * due to duplicated MASTERACKs
	 */
	if (answer != NULL &&
	    strcmp(answer->tsp_name, mastername) != 0) {
		conflict.tsp_type = TSP_CONFLICT;
		(void)strcpy(conflict.tsp_name, hostname);
		if (!acksend(&conflict, &masteraddr, mastername,
			     TSP_ACK, 0, 0)) {
			syslog(LOG_ERR,
			       "error on sending TSP_CONFLICT");
		}
	}
}

/*
 * based on the current network configuration, set the status, and count
 * networks;
 */
void
setstatus()
{
	struct netinfo *ntp;

	status = 0;
	nmasternets = nslavenets = nnets = nignorednets = 0;
	if (trace)
		fprintf(fd, "Net status:\n");
	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		switch ((int)ntp->status) {
		case MASTER:
			nmasternets++;
			break;
		case SLAVE:
			nslavenets++;
			break;
		case NOMASTER:
		case IGNORE:
			nignorednets++;
			break;
		}
		if (trace) {
			fprintf(fd, "\t%-16s", inet_ntoa(ntp->net));
			switch ((int)ntp->status) {
			case NOMASTER:
				fprintf(fd, "NOMASTER\n");
				break;
			case MASTER:
				fprintf(fd, "MASTER\n");
				break;
			case SLAVE:
				fprintf(fd, "SLAVE\n");
				break;
			case IGNORE:
				fprintf(fd, "IGNORE\n");
				break;
			default:
				fprintf(fd, "invalid state %d\n",
					(int)ntp->status);
				break;
			}
		}
		nnets++;
		status |= ntp->status;
	}
	status &= ~IGNORE;
	if (trace)
		fprintf(fd,
			"\tnets=%d masters=%d slaves=%d ignored=%d delay2=%d\n",
			nnets, nmasternets, nslavenets, nignorednets, delay2);
}

void
makeslave(net)
	struct netinfo *net;
{
	register struct netinfo *ntp;

	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		if (ntp->status == SLAVE && ntp != net)
			ntp->status = IGNORE;
	}
	slavenet = net;
}

/*
 * Try to become master over ignored nets..
 */
static void
checkignorednets()
{
	register struct netinfo *ntp;

	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		if (!Mflag && ntp->status == SLAVE)
			break;

		if (ntp->status == IGNORE || ntp->status == NOMASTER) {
			lookformaster(ntp);
			if (!Mflag && ntp->status == SLAVE)
				break;
		}
	}
}

/*
 * choose a good network on which to be a slave
 *	The ignored networks must have already been checked.
 *	Take a hint about for a good network.
 */
static void
pickslavenet(ntp)
	struct netinfo *ntp;
{
	if (slavenet != 0 && slavenet->status == SLAVE) {
		makeslave(slavenet);		/* prune extras */
		return;
	}

	if (ntp == 0 || ntp->status != SLAVE) {
		for (ntp = nettab; ntp != 0; ntp = ntp->next) {
			if (ntp->status == SLAVE)
				break;
		}
	}
	makeslave(ntp);
}

/*
 * returns a random number in the range [inf, sup]
 */
long
casual(inf, sup)
	long inf, sup;
{
	double value;

	value = ((double)(random() & 0x7fffffff)) / (0x7fffffff*1.0);
	return(inf + (sup - inf)*value);
}

char *
date()
{
#ifdef sgi
	struct	timeval tv;
	static char tm[32];

	(void)gettimeofday(&tv, (struct timezone *)0);
	(void)cftime(tm, "%D %T", &tv.tv_sec);
	return (tm);
#else
	struct	timeval tv;

	(void)gettimeofday(&tv, (struct timezone *)0);
	return (ctime(&tv.tv_sec));
#endif /* sgi */
}

void
addnetname(name)
	char *name;
{
	register struct nets **netlist = &nets;

	while (*netlist)
		netlist = &((*netlist)->next);
	*netlist = (struct nets *)malloc(sizeof **netlist);
	if (*netlist == 0) {
		fprintf(stderr,"malloc failed\n");
		exit(1);
	}
	bzero((char *)*netlist, sizeof(**netlist));
	(*netlist)->name = name;
}

/* note a host as trustworthy */
static void
add_good_host(name, perm)
	char *name;
	int perm;			/* 1=not part of the netgroup */
{
	register struct goodhost *ghp;
	register struct hostent *hentp;

	ghp = (struct goodhost*)malloc(sizeof(*ghp));
	if (!ghp) {
		syslog(LOG_ERR, "malloc failed");
		exit(1);
	}

	bzero((char*)ghp, sizeof(*ghp));
	(void)strncpy(&ghp->name[0], name, sizeof(ghp->name));
	ghp->next = goodhosts;
	ghp->perm = perm;
	goodhosts = ghp;

	hentp = gethostbyname(name);
	if (0 == hentp && perm)
		(void)fprintf(stderr, "unknown host %s\n", name);
}


/* update our image of the net-group of trustworthy hosts
 */
void
get_goodgroup(force)
	int force;
{
# define NG_DELAY (30*60*CLK_TCK)	/* 30 minutes */
	static unsigned long last_update = -NG_DELAY;
	unsigned long new_update;
	struct hosttbl *htp;
	struct goodhost *ghp, **ghpp;
	char *mach, *usr, *dom;
	struct tms tm;


	/* if no netgroup, then we are finished */
	if (goodgroup == 0 || !Mflag)
		return;

	/* Do not chatter with the netgroup master too often.
	 */
	new_update = times(&tm);
	if (new_update < last_update + NG_DELAY
	    && !force)
		return;
	last_update = new_update;

	/* forget the old temporary entries */
	ghpp = &goodhosts;
	while (0 != (ghp = *ghpp)) {
		if (!ghp->perm) {
			*ghpp = ghp->next;
			free((char*)ghp);
		} else {
			ghpp = &ghp->next;
		}
	}

#ifdef HAVENIS
	/* quit now if we are not one of the trusted masters
	 */
	if (!innetgr(goodgroup, &hostname[0], 0,0)) {
		if (trace)
			(void)fprintf(fd, "get_goodgroup: %s not in %s\n",
				      &hostname[0], goodgroup);
		return;
	}
	if (trace)
		(void)fprintf(fd, "get_goodgroup: %s in %s\n",
				  &hostname[0], goodgroup);

	/* mark the entire netgroup as trusted */
	(void)setnetgrent(goodgroup);
	while (getnetgrent(&mach,&usr,&dom)) {
		if (0 != mach)
			add_good_host(mach,0);
	}
	(void)endnetgrent();

	/* update list of slaves */
	for (htp = self.l_fwd; htp != &self; htp = htp->l_fwd) {
		htp->good = good_host_name(&htp->name[0]);
	}
#endif /* HAVENIS */
}


/* see if a machine is trustworthy
 */
int					/* 1=trust hp to change our date */
good_host_name(name)
	char *name;
{
	register struct goodhost *ghp = goodhosts;
	register char c;

	if (!ghp || !Mflag)		/* trust everyone if no one named */
		return 1;

	c = *name;
	do {
		if (c == ghp->name[0]
		    && !strcasecmp(name, ghp->name))
			return 1;	/* found him, so say so */
	} while (0 != (ghp = ghp->next));

	if (!strcasecmp(name,hostname))	/* trust ourself */
		return 1;

	return 0;			/* did not find him */
}
