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
#ifndef	NO_IPF
extern	struct	ipread	snoop, pcap, etherf, iphex, tcpd, iptext;
#endif

int	opts = 0;
#ifndef	DEFAULT_DEVICE
# ifdef	linux
char	default_device[] = "eth0";
# else
#  ifdef	sun
char	default_device[] = "le0";
#  else
#   ifdef	ultrix
char	default_device[] = "ln0";
#   else
#    ifdef	__bsdi__
char	default_device[] = "ef0";
#    else
char	default_device[] = "lan0";
#    endif
#   endif
#  endif
# endif
#else
char	default_device[] = DEFAULT_DEVICE;
#endif


static	void	usage __P((char *));
int	main __P((int, char **));


static void usage(prog)
char	*prog;
{
	fprintf(stderr, "Usage: %s [options] <-r filename|-R filename>\n\
\t\t-r filename\tsnoop data file to resend\n\
\t\t-R filename\tlibpcap data file to resend\n\
\toptions:\n\
\t\t-d device\tSend out on this device\n\
\t\t-g gateway\tIP gateway to use if non-local dest.\n\
\t\t-m mtu\t\tfake MTU to use when sending out\n\
", prog);
	exit(1);
}


int main(argc, argv)
int	argc;
char	**argv;
{
	struct	in_addr	gwip;
	struct	ipread	*ipr = NULL;
	char	*name =  argv[0], *gateway = NULL, *dev = NULL;
	char	c, *resend = NULL;
	int	mtu = 1500;

	while ((c = getopt(argc, argv, "EHPSTXd:g:m:r:")) != -1)
		switch (c)
		{
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
		case 'r' :
			resend = optarg;
			break;
#ifndef	NO_IPF
		case 'E' :
			ipr = &etherf;
			break;
		case 'H' :
			ipr = &iphex;
			break;
		case 'P' :
			ipr = &pcap;
			break;
		case 'S' :
			ipr = &snoop;
			break;
		case 'T' :
			ipr = &tcpd;
			break;
		case 'X' :
			ipr = &iptext;
			break;
#endif
		default :
			fprintf(stderr, "Unknown option \"%c\"\n", c);
			usage(name);
		}

	if (!ipr || !resend)
		usage(name);

	gwip.s_addr = 0;
	if (gateway && resolve(gateway, (char *)&gwip) == -1)
	    {
		fprintf(stderr,"Cant resolve %s\n", gateway);
		exit(2);
	    }

	if (!dev)
		dev = default_device;

	printf("Device:  %s\n", dev);
	printf("Gateway: %s\n", inet_ntoa(gwip));
	printf("mtu:     %d\n", mtu);

	return ip_resend(dev, mtu, ipr, gwip, resend);
}
