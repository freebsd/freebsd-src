/* $FreeBSD$ */
/*
 * ipsend.c (C) 1995-1998 Darren Reed
 *
 * This was written to test what size TCP fragments would get through
 * various TCP/IP packet filters, as used in IP firewalls.  In certain
 * conditions, enough of the TCP header is missing for unpredictable
 * results unless the filter is aware that this can happen.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(__sgi) && (IRIX > 602)
# include <sys/ptimers.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#ifndef	linux
#include <netinet/ip_var.h>
#endif
#include "ipsend.h"

#if !defined(lint)
static const char sccsid[] = "@(#)ipsend.c	1.5 12/10/95 (C)1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: ipsend.c,v 2.2.2.6 2002/12/06 11:40:35 darrenr Exp $";
#endif


extern	char	*optarg;
extern	int	optind;
extern	void	iplang __P((FILE *));

char	options[68];
int	opts;
#ifdef	linux
char	default_device[] = "eth0";
#else
# ifdef	sun
char	default_device[] = "le0";
# else
#  ifdef	ultrix
char	default_device[] = "ln0";
#  else
#   ifdef	__bsdi__
char	default_device[] = "ef0";
#   else
#    ifdef	__sgi
char	default_device[] = "ec0";
#    else
char	default_device[] = "lan0";
#    endif
#   endif
#  endif
# endif
#endif


static	void	usage __P((char *));
static	void	do_icmp __P((ip_t *, char *));
int	main __P((int, char **));


static	void	usage(prog)
char	*prog;
{
	fprintf(stderr, "Usage: %s [options] dest [flags]\n\
\toptions:\n\
\t\t-d\tdebug mode\n\
\t\t-i device\tSend out on this device\n\
\t\t-f fragflags\tcan set IP_MF or IP_DF\n\
\t\t-g gateway\tIP gateway to use if non-local dest.\n\
\t\t-I code,type[,gw[,dst[,src]]]\tSet ICMP protocol\n\
\t\t-m mtu\t\tfake MTU to use when sending out\n\
\t\t-P protocol\tSet protocol by name\n\
\t\t-s src\t\tsource address for IP packet\n\
\t\t-T\t\tSet TCP protocol\n\
\t\t-t port\t\tdestination port\n\
\t\t-U\t\tSet UDP protocol\n\
\t\t-v\tverbose mode\n\
\t\t-w <window>\tSet the TCP window size\n\
", prog);
	fprintf(stderr, "Usage: %s [-dv] -L <filename>\n\
\toptions:\n\
\t\t-d\tdebug mode\n\
\t\t-L filename\tUse IP language for sending packets\n\
\t\t-v\tverbose mode\n\
", prog);
	exit(1);
}


static void do_icmp(ip, args)
ip_t *ip;
char *args;
{
	struct	icmp	*ic;
	char	*s;

	ip->ip_p = IPPROTO_ICMP;
	ip->ip_len += sizeof(*ic);
	ic = (struct icmp *)(ip + 1);
	bzero((char *)ic, sizeof(*ic));
	if (!(s = strchr(args, ',')))
	    {
		fprintf(stderr, "ICMP args missing: ,\n");
		return;
	    }
	*s++ = '\0';
	ic->icmp_type = atoi(args);
	ic->icmp_code = atoi(s);
	if (ic->icmp_type == ICMP_REDIRECT && strchr(s, ','))
	    {
		char	*t;

		t = strtok(s, ",");
		t = strtok(NULL, ",");
		if (resolve(t, (char *)&ic->icmp_gwaddr) == -1)
		    {
			fprintf(stderr,"Cant resolve %s\n", t);
			exit(2);
		    }
		if ((t = strtok(NULL, ",")))
		    {
			if (resolve(t, (char *)&ic->icmp_ip.ip_dst) == -1)
			    {
				fprintf(stderr,"Cant resolve %s\n", t);
				exit(2);
			    }
			if ((t = strtok(NULL, ",")))
			    {
				if (resolve(t,
					    (char *)&ic->icmp_ip.ip_src) == -1)
				    {
					fprintf(stderr,"Cant resolve %s\n", t);
					exit(2);
				    }
			    }
		    }
	    }
}


int send_packets(dev, mtu, ip, gwip)
char *dev;
int mtu;
ip_t *ip;
struct in_addr gwip;
{
	u_short	sport = 0;
	int	wfd;

	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		sport = ((struct tcpiphdr *)ip)->ti_sport;
	wfd = initdevice(dev, sport, 5);

	return send_packet(wfd, mtu, ip, gwip);
}


int main(argc, argv)
int	argc;
char	**argv;
{
	FILE	*langfile = NULL;
	struct	tcpiphdr *ti;
	struct	in_addr	gwip;
	tcphdr_t	*tcp;
	ip_t	*ip;
	char	*name =  argv[0], host[MAXHOSTNAMELEN + 1];
	char	*gateway = NULL, *dev = NULL;
	char	*src = NULL, *dst, *s;
	int	mtu = 1500, olen = 0, c, nonl = 0;

	/*
	 * 65535 is maximum packet size...you never know...
	 */
	ip = (ip_t *)calloc(1, 65536);
	ti = (struct tcpiphdr *)ip;
	tcp = (tcphdr_t *)&ti->ti_sport;
	ip->ip_len = sizeof(*ip);
	ip->ip_hl = sizeof(*ip) >> 2;

	while ((c = getopt(argc, argv, "I:L:P:TUdf:i:g:m:o:s:t:vw:")) != -1)
		switch (c)
		{
		case 'I' :
			nonl++;
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			do_icmp(ip, optarg);
			break;
		case 'L' :
			if (nonl) {
				fprintf(stderr,
					"Incorrect usage of -L option.\n");
				usage(name);
			}
			if (!strcmp(optarg, "-"))
				langfile = stdin;
			else if (!(langfile = fopen(optarg, "r"))) {
				fprintf(stderr, "can't open file %s\n",
					optarg);
				exit(1);
			}
			iplang(langfile);
			return 0;
		case 'P' :
		    {
			struct	protoent	*p;

			nonl++;
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			if ((p = getprotobyname(optarg)))
				ip->ip_p = p->p_proto;
			else
				fprintf(stderr, "Unknown protocol: %s\n",
					optarg);
			break;
		    }
		case 'T' :
			nonl++;
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			ip->ip_p = IPPROTO_TCP;
			ip->ip_len += sizeof(tcphdr_t);
			break;
		case 'U' :
			nonl++;
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			ip->ip_p = IPPROTO_UDP;
			ip->ip_len += sizeof(udphdr_t);
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'f' :
			nonl++;
			ip->ip_off = strtol(optarg, NULL, 0);
			break;
		case 'g' :
			nonl++;
			gateway = optarg;
			break;
		case 'i' :
			nonl++;
			dev = optarg;
			break;
		case 'm' :
			nonl++;
			mtu = atoi(optarg);
			if (mtu < 28)
			    {
				fprintf(stderr, "mtu must be > 28\n");
				exit(1);
			    }
			break;
		case 'o' :
			nonl++;
			olen = buildopts(optarg, options, (ip->ip_hl - 5) << 2);
			break;
		case 's' :
			nonl++;
			src = optarg;
			break;
		case 't' :
			nonl++;
			if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
				tcp->th_dport = htons(atoi(optarg));
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		case 'w' :
			nonl++;
			if (ip->ip_p == IPPROTO_TCP)
				tcp->th_win = atoi(optarg);
			else
				fprintf(stderr, "set protocol to TCP first\n");
			break;
		default :
			fprintf(stderr, "Unknown option \"%c\"\n", c);
			usage(name);
		}

	if (argc - optind < 1)
		usage(name);
	dst = argv[optind++];

	if (!src)
	    {
		gethostname(host, sizeof(host));
		src = host;
	    }

	if (resolve(src, (char *)&ip->ip_src) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", src);
		exit(2);
	    }

	if (resolve(dst, (char *)&ip->ip_dst) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", dst);
		exit(2);
	    }

	if (!gateway)
		gwip = ip->ip_dst;
	else if (resolve(gateway, (char *)&gwip) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", gateway);
		exit(2);
	    }

	if (olen)
	    {
		caddr_t	ipo = (caddr_t)ip;

		printf("Options: %d\n", olen);
		ti = (struct tcpiphdr *)malloc(olen + ip->ip_len);
		if(!ti)
		    {
			fprintf(stderr,"malloc failed\n");
			exit(2);
		    } 

		bcopy((char *)ip, (char *)ti, sizeof(*ip));
		ip = (ip_t *)ti;
		ip->ip_hl = (olen >> 2);
		bcopy(options, (char *)(ip + 1), olen);
		bcopy((char *)tcp, (char *)(ip + 1) + olen, sizeof(*tcp));
		ip->ip_len += olen;
		bcopy((char *)ip, (char *)ipo, ip->ip_len);
		ip = (ip_t *)ipo;
		tcp = (tcphdr_t *)((char *)(ip + 1) + olen);
	    }

	if (ip->ip_p == IPPROTO_TCP)
		for (s = argv[optind]; s && (c = *s); s++)
			switch(c)
			{
			case 'S' : case 's' :
				tcp->th_flags |= TH_SYN;
				break;
			case 'A' : case 'a' :
				tcp->th_flags |= TH_ACK;
				break;
			case 'F' : case 'f' :
				tcp->th_flags |= TH_FIN;
				break;
			case 'R' : case 'r' :
				tcp->th_flags |= TH_RST;
				break;
			case 'P' : case 'p' :
				tcp->th_flags |= TH_PUSH;
				break;
			case 'U' : case 'u' :
				tcp->th_flags |= TH_URG;
				break;
			}

	if (!dev)
		dev = default_device;
	printf("Device:  %s\n", dev);
	printf("Source:  %s\n", inet_ntoa(ip->ip_src));
	printf("Dest:    %s\n", inet_ntoa(ip->ip_dst));
	printf("Gateway: %s\n", inet_ntoa(gwip));
	if (ip->ip_p == IPPROTO_TCP && tcp->th_flags)
		printf("Flags:   %#x\n", tcp->th_flags);
	printf("mtu:     %d\n", mtu);

#ifdef	DOSOCKET
	if (tcp->th_dport)
		return do_socket(dev, mtu, ti, gwip);
#endif
	return send_packets(dev, mtu, (ip_t *)ti, gwip);
}
