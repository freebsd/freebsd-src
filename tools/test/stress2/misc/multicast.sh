#!/bin/sh

# Multicast test example by Mark Claypool, claypool at cs.wpi.edu
# https://web.cs.wpi.edu/~claypool/courses/4514-B99/samples/multicast.c

# Kernel page fault seen with WiP branch:
# https://people.freebsd.org/~pho/stress/log/kip036.txt

. ../default.cfg

dir=/tmp
odir=`pwd`
cd $dir
sed '1,/^EOF/d' < $odir/$0 > $dir/multicast.c
mycc -o multicast -Wall -Wextra -O0 -g multicast.c || exit 1
rm -f multicast.c
cd $odir

cd $dir
(
	timeout -k 1s 20s ./multicast &
	sleep 1
	timeout -k 1s 25s ./multicast 1
) > /dev/null
wait

rm -f $dir/multicast
exit $s
EOF
/*
multicast.c

The following program sends or receives multicast packets. If invoked
with one argument, it sends a packet containing the current time to an
arbitrarily chosen multicast group and UDP port. If invoked with no
arguments, it receives and prints these packets. Start it as a sender on
just one host and as a receiver on all the other hosts

*/

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define EXAMPLE_PORT 6000
#define EXAMPLE_GROUP "239.0.0.1"

int
main(int argc, char *argv[] __unused)
{
	struct ip_mreq mreq;
	struct sockaddr_in addr;
	socklen_t addrlen;
	int cnt, sock;
	char message[50];

	/* set up socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	bzero((char *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(EXAMPLE_PORT);
	addrlen = sizeof(addr);

	if (argc > 1) {
		/* send */
		addr.sin_addr.s_addr = inet_addr(EXAMPLE_GROUP);
		while (1) {
			time_t t = time(0);
			sprintf(message, "time is %-24.24s", ctime(&t));
			printf("sending: %s\n", message);
			cnt = sendto(sock, message, sizeof(message), 0,
					(struct sockaddr *) &addr, addrlen);
			if (cnt < 0) {
				perror("sendto");
				exit(1);
			}
			sleep(5);
		}
	} else {

		/* receive */
		if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			perror("bind");
			exit(1);
		}
		mreq.imr_multiaddr.s_addr = inet_addr(EXAMPLE_GROUP);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
					&mreq, sizeof(mreq)) < 0) {
			perror("setsockopt mreq");
			exit(1);
		}
		while (1) {
			cnt = recvfrom(sock, message, sizeof(message), 0,
					(struct sockaddr *) &addr, &addrlen);
			if (cnt < 0) {
				perror("recvfrom");
				exit(1);
			} else if (cnt == 0) {
				break;
			}
			printf("%s: message = \"%s\"\n", inet_ntoa(addr.sin_addr), message);
		}
	}
}
