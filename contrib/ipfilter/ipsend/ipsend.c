/*
 * ipsend.c (C) 1995 Darren Reed
 *
 * This was written to test what size TCP fragments would get through
 * various TCP/IP packet filters, as used in IP firewalls.  In certain
 * conditions, enough of the TCP header is missing for unpredictable
 * results unless the filter is aware that this can happen.
 *
 * The author provides this program as-is, with no gaurantee for its
 * suitability for any specific purpose.  The author takes no responsibility
 * for the misuse/abuse of this program and provides it for the sole purpose
 * of testing packet filter policies.  This file maybe distributed freely
 * providing it is not modified and that this notice remains in tact.
 *
 * This was written and tested (successfully) on SunOS 4.1.x.
 */
#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ipsend.c	1.5 12/10/95 (C)1995 Darren Reed";
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
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


extern	char	*optarg;
extern	int	optind;

char	options[68];
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
char	default_device[] = "lan0";
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
\t\t-d device\tSend out on this device\n\
\t\t-f fragflags\tcan set IP_MF or IP_DF\n\
\t\t-g gateway\tIP gateway to use if non-local dest.\n\
\t\t-I code,type[,gw[,dst[,src]]]\tSet ICMP protocol\n\
\t\t-m mtu\t\tfake MTU to use when sending out\n\
\t\t-P protocol\tSet protocol by name\n\
\t\t-s src\t\tsource address for IP packet\n\
\t\t-T\t\tSet TCP protocol\n\
\t\t-t port\t\tdestination port\n\
\t\t-U\t\tSet UDP protocol\n\
", prog);
	exit(1);
}


void do_icmp(ip, args)
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
	struct	tcpiphdr *ti;
	struct	in_addr	gwip;
	tcphdr_t	*tcp;
	ip_t	*ip;
	char	*name =  argv[0], host[64], *gateway = NULL, *dev = NULL;
	char	*src = NULL, *dst, c, *s;
	int	mtu = 1500, olen = 0;

	/*
	 * 65535 is maximum packet size...you never know...
	 */
	ip = (ip_t *)calloc(1, 65536);
	ti = (struct tcpiphdr *)ip;
	tcp = (tcphdr_t *)&ti->ti_sport;
	ip->ip_len = sizeof(*ip);
	ip->ip_hl = sizeof(*ip) >> 2;

	while ((c = (char)getopt(argc, argv, "IP:TUd:f:g:m:o:s:t:")) != -1)
		switch (c)
		{
		case 'I' :
			if (ip->ip_p)
			    {
				fprintf(stderr, "Protocol already set: %d\n",
					ip->ip_p);
				break;
			    }
			do_icmp(ip, optarg);
			break;
		case 'P' :
		    {
			struct	protoent	*p;

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
			dev = optarg;
			break;
		case 'f' :
			ip->ip_off = strtol(optarg, NULL, 0);
			break;
		case 'g' :
			gateway = optarg;
			break;
		case 'm' :
			mtu = atoi(optarg);
			if (mtu < 28)
			    {
				fprintf(stderr, "mtu must be > 28\n");
				exit(1);
			    }
			break;
		case 'o' :
			olen = optname(optarg, options);
			break;
		case 's' :
			src = optarg;
			break;
		case 't' :
			if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
				tcp->th_dport = htons(atoi(optarg));
			break;
		case 'w' :
			if (ip->ip_p == IPPROTO_TCP)
				tcp->th_win = atoi(optarg);
			else
				fprintf(stderr, "set protocol to TCP first\n");
			break;
		default :
			fprintf(stderr, "Unknown option \"%c\"\n", c);
			usage(name);
		}

	if (argc - optind < 2)
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

	if (ip->ip_p == IPPROTO_TCP)
		for (s = argv[optind]; (c = *s); s++)
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

	if (olen)
	    {
		printf("Options: %d\n", olen);
		ti = (struct tcpiphdr *)malloc(olen + ip->ip_len);
		bcopy((char *)ip, (char *)ti, sizeof(*ip));
		ip = (ip_t *)ti;
		ip->ip_hl += (olen >> 2);
		bcopy(options, (char *)(ip + 1), olen);
		bcopy((char *)tcp, (char *)(ip + 1) + olen, sizeof(*tcp));
		tcp = (tcphdr_t *)((char *)(ip + 1) + olen);
		ip->ip_len += olen;
	    }

#ifdef	DOSOCKET
	if (tcp->th_dport)
		return do_socket(dev, mtu, ti, gwip);
#endif
	return send_packets(dev, mtu, (ip_t *)ti, gwip);
}
