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
static	char	sccsid[] = "%W% %G% (C)1995 Darren Reed";
#endif
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
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
#ifdef	linux
#include <linux/sockios.h>
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
int	main __P((int, char **));


static void usage(prog)
char *prog;
{
	fprintf(stderr, "Usage: %s [options] dest\n\
\toptions:\n\
\t\t-d device\tSend out on this device\n\
\t\t-g gateway\tIP gateway to use if non-local dest.\n\
\t\t-m mtu\t\tfake MTU to use when sending out\n\
\t\t-p pointtest\t\n\
\t\t-s src\t\tsource address for IP packet\n\
\t\t-1 \t\tPerform test 1 (IP header)\n\
\t\t-2 \t\tPerform test 2 (IP options)\n\
\t\t-3 \t\tPerform test 3 (ICMP)\n\
\t\t-4 \t\tPerform test 4 (UDP)\n\
\t\t-5 \t\tPerform test 5 (TCP)\n\
\t\t-6 \t\tPerform test 6 (overlapping fragments)\n\
\t\t-7 \t\tPerform test 7 (random packets)\n\
", prog);
	exit(1);
}


int main(argc, argv)
int argc;
char **argv;
{
	struct	tcpiphdr *ti;
	struct	in_addr	gwip;
	ip_t	*ip;
	char	*name =  argv[0], host[64], *gateway = NULL, *dev = NULL;
	char	*src = NULL, *dst, c;
	int	mtu = 1500, tests = 0, pointtest = 0;

	/*
	 * 65535 is maximum packet size...you never know...
	 */
	ip = (ip_t *)calloc(1, 65536);
	ti = (struct tcpiphdr *)ip;
	ip->ip_len = sizeof(*ip);
	ip->ip_hl = sizeof(*ip) >> 2;

	while ((c = getopt(argc, argv, "1234567IP:TUd:f:g:m:o:p:s:t:")) != -1)
		switch (c)
		{
		case '1' :
		case '2' :
		case '3' :
		case '4' :
		case '5' :
		case '6' :
		case '7' :
			tests = c - '0';
			break;
		case 'd' :
			dev = optarg;
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
		case 'p' :
			pointtest = atoi(optarg);
			break;
		case 's' :
			src = optarg;
			break;
		default :
			fprintf(stderr, "Unknown option \"%c\"\n", c);
			usage(name);
		}

	if (argc - optind < 2 && !tests)
		usage(name);
	dst = argv[optind++];

	if (!src)
	    {
		gethostname(host, sizeof(host));
		src = host;
	    }

	if (resolve(dst, (char *)&ip->ip_dst) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", dst);
		exit(2);
	    }

	if (resolve(src, (char *)&ip->ip_src) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", src);
		exit(2);
	    }

	if (!gateway)
		gwip = ip->ip_dst;
	else if (resolve(gateway, (char *)&gwip) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", gateway);
		exit(2);
	    }


	if (!dev)
		dev = default_device;
	printf("Device:  %s\n", dev);
	printf("Source:  %s\n", inet_ntoa(ip->ip_src));
	printf("Dest:    %s\n", inet_ntoa(ip->ip_dst));
	printf("Gateway: %s\n", inet_ntoa(gwip));
	printf("mtu:     %d\n", mtu);

	switch (tests)
	{
	case 1 :
		ip_test1(dev, mtu, (ip_t *)ti, gwip, pointtest);
		break;
	case 2 :
		ip_test2(dev, mtu, (ip_t *)ti, gwip, pointtest);
		break;
	case 3 :
		ip_test3(dev, mtu, (ip_t *)ti, gwip, pointtest);
		break;
	case 4 :
		ip_test4(dev, mtu, (ip_t *)ti, gwip, pointtest);
		break;
	case 5 :
		ip_test5(dev, mtu, (ip_t *)ti, gwip, pointtest);
		break;
	case 6 :
		ip_test6(dev, mtu, (ip_t *)ti, gwip, pointtest);
		break;
	case 7 :
		ip_test7(dev, mtu, (ip_t *)ti, gwip, pointtest);
		break;
	default :
		break;
	}
	return 0;
}
