/*
 *   Copyright (c) 1998 Martin Husemann. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - network monitor client
 *	-----------------------------------
 *
 *	$Id: main.c,v 1.12 1999/05/11 08:15:59 hm Exp $
 *
 *      last edit-date: [Tue Apr 20 14:14:26 1999]
 *
 *	-mh	created
 *	-hm	checking in
 *	-hm	porting to HPUX
 *	-mh	all events the fullscreen mode displays now as monitor event
 *
 *---------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <machine/i4b_ioctl.h>

#ifdef __hpux
#define AF_LOCAL AF_UNIX
#endif

#ifdef DEBUG
#include <ctype.h>
#endif

#include "monitor.h"

/*
 * Local function prototypes
 */
static int connect_local(const char *sockpath);
static int connect_remote(const char *host, int portno);
static void usage();
static void mloop();
static void handle_input();
static void print_menu();
static void print_logevent(time_t tstamp, int prio, const char * what, const char * msg);
static void print_charge(time_t tstamp, int channel, int units, int estimated);
static void print_connect(time_t tstamp, int dir, int channel, const char * cfgname, const char * devname, const char * remphone, const char * locphone);
static void print_disconnect(time_t tstamp, int channel);
static void print_updown(time_t tstamp, int channel, int isup);
static void handle_event(BYTE *msg, int len);
#ifdef DEBUG
static void dump_event(BYTE *msg, int len);
#endif

/*
 * Global variables
 */
static int dumpall = 0;
static int monsock = -1;
static int state = 0;
static int sub_state = 0;
static int sub_state_count = 0;

static int isdn_major = 0;
static int isdn_minor = 0;
static int nctrl = 0;
static u_int32_t rights = 0;

/*
 * Parse command line, startup monitor client
 */
int main(int argc, char **argv)
{
	char * sockpath = NULL;
	char * hostname = NULL;
	int portno = DEF_MONPORT;
	int i;

	while ((i = getopt(argc, argv, "dh:p:l:")) != EOF)
	{
		switch (i)
		{
			case 'd':
				dumpall = 1;
				break;
			case 'h':
				hostname = optarg;
				break;
			case 'l':
				sockpath = optarg;
				break;
			case 'p':
				if ((sscanf(optarg, "%i", &portno)) != 1)
					usage();
				break;
			default:
				usage();
				break;
		}
	}

	if (hostname && sockpath)
	{
		fprintf(stderr, "Error: can not use local socket path on remote machine\n"
				"conflicting options -h and -l!\n");
		return 1;
	}

	if (sockpath)
	{
		monsock = connect_local(sockpath);
	}
	else if (hostname)
	{
		monsock = connect_remote(hostname, portno);
	}
	else
	{
		usage();
	}

	if (monsock == -1)
	{
		fprintf(stderr, "Could not connect to i4b isdn daemon.\n");
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);
	mloop();

	close(monsock);

	return 0;
}

/*
 * Display usage and exit
 */
static void usage()
{
	fprintf(stderr, "usage:\n"
			"    isdnmonitor [-d] -h (host) -p (port)\n"
			"or\n"
			"    isdnmonitor [-d] -l (path)\n"
			"where (host) is the hostname and (port) the port number of\n"
			"the isdnd to be monitored and (path) is the pathname of the\n"
			"local domain socket used to communicate with a daemon on the\n"
			"local machine.\n"
			"Options are:\n"
			" -d  dump all incoming packets as hexdump\n"
			);
	exit(0);
}

/*
 * Connect via tcp/ip.
 * Return socket if successfull, -1 on error.
 */
static int connect_remote(const char *host, int portno)
{
	struct sockaddr_in sa;
	struct hostent *h;
	int remotesockfd;

	h = gethostbyname(host);

	if (!h)
	{
		fprintf(stderr, "could not resolve hostname '%s'\n", host);
		exit(1);
	}

	remotesockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (remotesockfd == -1)
	{
		fprintf(stderr, "could not create remote monitor socket: %s\n", strerror(errno));
		exit(1);
	}

	memset(&sa, 0, sizeof sa);

#ifdef BSD4_4
	sa.sin_len = sizeof sa;
#endif
	sa.sin_family = AF_INET;
	sa.sin_port = htons(portno);

	memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], sizeof sa.sin_addr.s_addr);

	if (connect(remotesockfd, (struct sockaddr *)&sa, sizeof sa) == -1)
	{
		fprintf(stderr, "could not connect remote monitor: %s\n", strerror(errno));
		exit(1);
	}

	return remotesockfd;
}

/*
 * Connect local.
 * Return socket on success, -1 on failure.
 */
static int connect_local(const char *sockpath)
{
	int s;
	struct sockaddr_un sa;

	/* check path length */
	if (strlen(sockpath) >= sizeof sa.sun_path)
	{
		fprintf(stderr, "pathname to long for local socket: %s\n",
			sockpath);
		exit(1);
	}

	/* create and setup socket */
	s = socket(AF_LOCAL, SOCK_STREAM, 0);

	if (s == -1)
	{
		fprintf(stderr, "could not create local monitor socket:%s\n", strerror(errno));
		exit(1);
	}

	memset(&sa, 0, sizeof sa);

#ifndef __hpux
	sa.sun_len = sizeof sa;
#endif

	sa.sun_family = AF_LOCAL;
	strcpy(sa.sun_path, sockpath);

	if (connect(s, (struct sockaddr *)&sa, sizeof sa))
	{
		fprintf(stderr, "could not connect local monitor socket [%s]: %s\n", sockpath, strerror(errno));
	}

	return s;
}

/*
 * main event loop
 */
static void mloop()
{
	for (;;)
	{
		fd_set rd, wr, ex;

		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_ZERO(&ex);
		FD_SET(fileno(stdin), &rd);
		FD_SET(monsock, &rd);

		select(monsock+1, &rd, &wr, &ex, NULL);

		if (FD_ISSET(fileno(stdin), &rd))
		{
			handle_input();
		}

		if (FD_ISSET(monsock, &rd))
		{
			BYTE buf[4096];
			u_long u;
			int bytes, ret;

			/* Network transfer may deliver two or more packets concatenated.
			 * Peek at the header and read only one event at a time... */

			ioctl(monsock, FIONREAD, &u);

			if (u < I4B_MON_EVNT_HDR)
				continue;	/* not enough data there yet */

			bytes = recv(monsock, buf, I4B_MON_EVNT_HDR, MSG_PEEK);

			if (bytes < I4B_MON_EVNT_HDR)
				continue;	/* errh? something must be wrong... */

			bytes = I4B_GET_2B(buf, I4B_MON_EVNT_LEN);

			if (bytes >= sizeof buf)
				break;

			/* now we know the size, it fits, so lets read it! */
			
			ret = read(monsock, buf, bytes);

			if (ret == 0)
			{
				printf("remote isdnd has closed our connection\n");
				break;
			}
			else if (ret < 0)
			{
				printf("error reading from isdnd: %s", strerror(errno));
				break;
			}
#ifdef DEBUG
			if (dumpall)
				dump_event(buf, ret);
#endif
			handle_event(buf, ret);
		}
	}
}

#ifdef DEBUG
/*
 * Dump a complete event packet.
 */
static void dump_event(BYTE *msg, int len)
{
	int i;

	printf("event dump:");

	for (i = 0; i < len; i++)
	{
		if (i % 8 == 0)
			printf("\n%02x: ", i);
		printf("%02x %c  ", msg[i], isprint(msg[i]) ? msg[i] : '.');
	}
	printf("\n");
}
#endif

static void print_logevent(time_t tstamp, int prio, const char * what, const char * msg)
{
	char buf[256];
	strftime(buf, sizeof buf, I4B_TIME_FORMAT, localtime(&tstamp));
	printf("log: %s prio %d what=%s msg=%s\n",
		buf, prio, what, msg);
}

static void print_charge(time_t tstamp, int channel, int units, int estimated)
{
	char buf[256];
	strftime(buf, sizeof buf, I4B_TIME_FORMAT, localtime(&tstamp));
	printf("%s: channel %d, charge = %d%s\n",
		buf, channel, units, estimated ? " (estimated)" : "");
}

/*
 * Print a connect event.
 * A real monitor would allocate state info for "channel" on this
 * event.
 */
static void print_connect(
	time_t tstamp, 	/* server time of event */
	int outgoing,	/* 0 = incoming, 1 = outgoing */
	int channel,	/* channel no, used to identify this connection until disconnect */
	const char * cfgname, 	/* name of config entry/connection */
	const char * devname, 	/* device used (e.g. isp0) */
	const char * remphone, 	/* phone no of remote side */
	const char * locphone)	/* local phone no */
{
	char buf[256];
	strftime(buf, sizeof buf, I4B_TIME_FORMAT, localtime(&tstamp));

	if (outgoing)
		printf("%s: calling out to '%s' [from msn: '%s']",
			buf, remphone, locphone);
	else
		printf("%s: incoming call from '%s' [to msn: '%s']",
			buf, remphone, locphone);
	printf(", channel %d, config '%s' on device '%s'\n",
		channel, cfgname, devname);
}

/*
 * Print a disconnect event.
 * A real monitor could free the "per connection" state
 * for this channel now
 */
static void print_disconnect(time_t tstamp, int channel)
{
	char buf[256];
	strftime(buf, sizeof buf, I4B_TIME_FORMAT, localtime(&tstamp));
	printf("%s: channel %d disconnected\n",
		buf, channel);
}

/*
 * Print an up- or down event
 */
static void print_updown(time_t tstamp, int channel, int isup)
{
	char buf[256];
	strftime(buf, sizeof buf, I4B_TIME_FORMAT, localtime(&tstamp));
	printf("%s: channel %d is %s\n",
		buf, channel, isup ? "up" : "down");
}

/*
 * Dispatch one message received from the daemon.
 */
static void handle_event(BYTE *msg, int len)
{
	BYTE cmd[I4B_MON_ICLIENT_SIZE];
	int local;	
	u_int32_t net;
	u_int32_t mask;
	u_int32_t who;
	
	switch (state)
	{
		case 0:	/* initial data */

			isdn_major = I4B_GET_2B(msg, I4B_MON_IDATA_VERSMAJOR);
			isdn_minor = I4B_GET_2B(msg, I4B_MON_IDATA_VERSMINOR);
			nctrl = I4B_GET_2B(msg, I4B_MON_IDATA_NUMCTRL);
			rights = I4B_GET_4B(msg, I4B_MON_IDATA_CLACCESS);

			printf("remote protocol version is %02d.%02d, %d controller(s) found, our rights = %x\n",
				isdn_major, isdn_minor, nctrl, rights);

			if (nctrl > 0)
			{
				state = 1;
				sub_state = 0;
			}
			else
			{
				state = 2;

				/* show menu for the first time */
				print_menu();
			}
			
			/* set maximum event mask */
			I4B_PREP_CMD(cmd, I4B_MON_CCMD_SETMASK);
			I4B_PUT_2B(cmd, I4B_MON_ICLIENT_VERMAJOR, MPROT_VERSION);
			I4B_PUT_2B(cmd, I4B_MON_ICLIENT_VERMINOR, MPROT_REL);
			I4B_PUT_4B(cmd, I4B_MON_ICLIENT_EVENTS, ~0U);

			write(monsock, cmd, sizeof cmd);

			break;

		case 1:	/* initial controller list */
			printf("controller %d: %s\n", sub_state++, msg+I4B_MON_ICTRL_NAME);

			if (sub_state >= nctrl)
			{
				state = 2;	/* end of list reached */
				sub_state = 0;
	
				/* show menu for the first time */
				print_menu();
			}
			break;

		case 2: /* any event */

			switch (I4B_GET_2B(msg, I4B_MON_EVNT))
			{
				case I4B_MON_DRINI_CODE:
					state = 3;	/* list of rights entries will follow */
					sub_state = 0;
					sub_state_count = I4B_GET_2B(msg, I4B_MON_DRINI_COUNT);
					printf("monitor rights:\n");
					break;

				case I4B_MON_DCINI_CODE:
					state = 4;
					sub_state = 0;
					sub_state_count = I4B_GET_2B(msg, I4B_MON_DCINI_COUNT);
					printf("monitor connections:\n");
					break;

				case I4B_MON_LOGEVNT_CODE:
					print_logevent(I4B_GET_4B(msg, I4B_MON_LOGEVNT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_LOGEVNT_PRIO),
						msg+I4B_MON_LOGEVNT_WHAT,
						msg+I4B_MON_LOGEVNT_MSG);
					break;
					
				case I4B_MON_CHRG_CODE:
					print_charge(I4B_GET_4B(msg, I4B_MON_CHRG_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_CHRG_CHANNEL),
						I4B_GET_4B(msg, I4B_MON_CHRG_UNITS),
						I4B_GET_4B(msg, I4B_MON_CHRG_ESTIMATED));
					break;
					
				case I4B_MON_CONNECT_CODE:
					print_connect(
						I4B_GET_4B(msg, I4B_MON_CONNECT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_CONNECT_DIR),
						I4B_GET_4B(msg, I4B_MON_CONNECT_CHANNEL),
						msg+I4B_MON_CONNECT_CFGNAME,
						msg+I4B_MON_CONNECT_DEVNAME,
						msg+I4B_MON_CONNECT_REMPHONE,
						msg+I4B_MON_CONNECT_LOCPHONE);
					break;
					
				case I4B_MON_DISCONNECT_CODE:
					print_disconnect(
						I4B_GET_4B(msg, I4B_MON_DISCONNECT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_DISCONNECT_CHANNEL));
					break;
					
				case I4B_MON_UPDOWN_CODE:
					print_updown(
						I4B_GET_4B(msg, I4B_MON_UPDOWN_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_UPDOWN_CHANNEL),
						I4B_GET_4B(msg, I4B_MON_UPDOWN_ISUP));
					break;

				default:
					printf("unknown event code: %d\n", I4B_GET_2B(msg, I4B_MON_EVNT));
			}
			break;

		case 3:	/* one record in a list of monitor rights */
			rights = I4B_GET_4B(msg, I4B_MON_DR_RIGHTS);
			net = I4B_GET_4B(msg, I4B_MON_DR_NET);
			mask = I4B_GET_4B(msg, I4B_MON_DR_MASK);
			local = I4B_GET_1B(msg, I4B_MON_DR_LOCAL);

			if (local)
			{
				printf("\tlocal: rights = %x\n", rights);
			}
			else
			{
				printf("\tfrom: %d.%d.%d.%d, mask %d.%d.%d.%d, rights = %x\n",
					(net >> 24) & 0x00ff, (net >> 16) & 0x00ff, (net >> 8) & 0x00ff, net & 0x00ff,
					(mask >> 24) & 0x00ff, (mask >> 16) & 0x00ff, (mask >> 8) & 0x00ff, mask & 0x00ff,
					rights);
			}

			sub_state++;

			if (sub_state >= sub_state_count)
			{
				state = 2;
				print_menu();
			}
			break;

		case 4:
			who = I4B_GET_4B(msg, I4B_MON_DC_WHO);
			rights = I4B_GET_4B(msg, I4B_MON_DC_RIGHTS);

			printf("\tfrom: %d.%d.%d.%d, rights = %x\n",
				(who >> 24) & 0x00ff, (who >> 16) & 0x00ff, (who >> 8) & 0x00ff, who & 0x00ff,
				rights);

			sub_state++;

			if (sub_state >= sub_state_count)
			{
				state = 2;
				print_menu();
			}
			break;

		default:
			printf("unknown event from remote: local state = %d, evnt = %x, len = %d\n",
				state, I4B_GET_2B(msg, I4B_MON_EVNT), len);
	}
}

/*
 * Process input from user
 */
static void handle_input()
{
	char buf[1024];
	int channel;
	
	fgets(buf, sizeof buf, stdin);

	switch (atoi(buf))
	{
		case 1:
		    {
		    	BYTE cmd[I4B_MON_DUMPRIGHTS_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_DUMPRIGHTS_CODE);
			write(monsock, cmd, I4B_MON_DUMPRIGHTS_SIZE);
		    }
		    break;

		case 2:
		    {
		    	BYTE cmd[I4B_MON_DUMPMCONS_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_DUMPMCONS_CODE);
			write(monsock, cmd, I4B_MON_DUMPMCONS_SIZE);
		    }
		    break;

		case 3:
		    {
		    	BYTE cmd[I4B_MON_CFGREREAD_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_CFGREREAD_CODE);
			write(monsock, cmd, I4B_MON_CFGREREAD_SIZE);
		    }
		    break;
			
		case 4:
		    {
		    	BYTE cmd[I4B_MON_HANGUP_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_HANGUP_CODE);
			printf("Which channel do you wish to hangup? ");
			fgets(buf, sizeof buf, stdin);
			channel = atoi(buf);
			I4B_PUT_4B(cmd, I4B_MON_HANGUP_CHANNEL, channel);
			write(monsock, cmd, I4B_MON_HANGUP_SIZE);
		    }
		    break;

		case 9:
			close(monsock);
			exit(0);
			break;

		default:
			print_menu();
			break;
	}
}

/*
 * Display menu
 */
static void print_menu()
{
	printf("Menu: <1> display rights,     <2> display monitor connections,\n");
	printf("      <3> reread config file, <4> hangup \n");
	printf("      <9> quit isdnmonitor\n");
}
