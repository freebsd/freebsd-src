/*
 *   Copyright (c) 1998,1999 Martin Husemann. All rights reserved.
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
 *	$Id: main.c,v 1.34 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD: src/usr.sbin/i4b/isdnmonitor/main.c,v 1.7 1999/12/14 21:07:41 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:52:11 1999]
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#ifndef WIN32
#include <unistd.h>
#include <netdb.h>
#endif
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <stdarg.h>
#include <windows.h>
extern char	*optarg;
int getopt(int nargc, char * const nargv[], const char *ostr);
#define close(f)	closesocket(f)
#define sleep(s)	Sleep(s*1000)
#define vsnprintf	_vsnprintf
#define ssize_t long
#endif
#ifdef ERROR
#undef ERROR
#endif

#define MAIN
#include "monprivate.h"
#undef MAIN

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

#ifdef DEBUG
#include <ctype.h>
#endif

#include "monitor.h"

/*
 * Local function prototypes
 */
static int connect_local(char *sockpath);
static int connect_remote(char *host, int portno);
static void usage();
static void mloop();
static void handle_input();
static void print_menu();
static void print_logevent(time_t tstamp, int prio, char * what, char * msg);
static void print_charge(time_t tstamp, int controller, int channel, int units, int estimated);
static void print_connect(time_t tstamp, int dir, int controller, int channel, char * cfgname, char * devname, char * remphone, char * locphone);
static void print_disconnect(time_t tstamp, int controller, int channel);
static void print_updown(time_t tstamp, int contoller, int channel, int isup);
static void handle_event(u_int8_t *msg, int len);
#ifdef DEBUG
static void dump_event(u_int8_t *msg, int len, int readflag);
#endif

static ssize_t sock_read(int fd, void *buf, size_t nbytes);
static ssize_t sock_write(int fd, void *buf, size_t nbytes);

static void mprintf(char *fmt, ...);

/*
 * Global variables
 */
static int debug = 0;
#define DBG_DUMPALL	0x01
#define DBG_PSEND	0x02

static int monsock = -1;
static int state = ST_INIT;
static int sub_state = 0;
static int sub_state_count = 0;

static int isdn_major = 0;
static int isdn_minor = 0;
static u_int32_t rights = 0;

static char *logfilename = NULL;
static FILE *lfp = NULL;

/*---------------------------------------------------------------------------
 *	Display usage and exit
 *---------------------------------------------------------------------------*/
static void
usage()
{
        fprintf(stderr, "\n");
        fprintf(stderr, "isdnmonitor - version %02d.%02d.%d, %s %s (protocol %02d.%02d)\n", VERSION, REL, STEP, __DATE__, __TIME__, MPROT_VERSION, MPROT_REL);
#ifdef FOREIGN
        fprintf(stderr, "  usage: isdnmonitor [-c] [-d val] [-f name] [-h host] [-p port]\n");
#else
        fprintf(stderr, "  usage: isdnmonitor [-c] [-d val] [-f name] [-h host] [-l path] [-p port]\n");
#endif
        fprintf(stderr, "    -c        switch to curses fullscreen output\n");        
        fprintf(stderr, "    -d <val>  debug flags (see source ...)\n");
	fprintf(stderr, "    -dn       no debug output on fullscreen display\n");
        fprintf(stderr, "    -f <name> filename to log output to\n");
        fprintf(stderr, "    -h <host> hostname/address to connect to\n");        
#ifndef FOREIGN
        fprintf(stderr, "    -l <path> pathname to local domain socket to connect to\n");
#endif
        fprintf(stderr, "    -p <port> portnumber to use to connect to remote host\n");
	exit(1);
}

/*---------------------------------------------------------------------------
 *	Parse command line, startup monitor client
 *---------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
	int i;

#ifdef WIN32
	WSADATA wsCaps;
	WSAStartup(MAKEWORD(2, 0), &wsCaps);
#endif

	portno = DEF_MONPORT;
	devbuf[0] = '\0';

#ifndef FOREIGN	
	while((i = getopt(argc, argv, "cd:f:h:p:l:")) != -1)
#else
	while((i = getopt(argc, argv, "cd:f:h:p:")) != -1)
#endif
	{
		switch(i)
		{
			case 'c':
				fullscreen = 1;
				break;
			case 'd':
                                if(*optarg == 'n')
                                {
	                                debug_noscreen = 1;
	                        }
				else
				{
					if((sscanf(optarg, "%i", &debug)) != 1)
						usage();
				}
				break;
			case 'f':
				logfilename = optarg;
				break;
			case 'h':
				hostname = optarg;
				break;
#ifndef FOREIGN
			case 'l':
				sockpath = optarg;
				break;
#endif
			case 'p':
				if((sscanf(optarg, "%i", &portno)) != 1)
					usage();
				break;
			default:
				usage();
				break;
		}
	}

#ifndef FOREIGN
	if(hostname && sockpath)
	{
		fprintf(stderr, "Error: can not use local socket path on remote machine\n"
				"conflicting options -h and -l!\n");
		return 1;
	}

	if(sockpath)
	{
		monsock = connect_local(sockpath);
	}
	else if(hostname)
#else
	if(hostname)
#endif

	{
		monsock = connect_remote(hostname, portno);
	}
	else
	{
		usage();
	}

	if(monsock == -1)
	{
		fprintf(stderr, "Could not connect to i4b isdn daemon.\n");
		return 1;
	}

	if(logfilename != NULL)
	{
		if((lfp = fopen(logfilename, "w")) == NULL)
		{
			fprintf(stderr, "could not open logfile [%s], %s\n", logfilename, strerror(errno));
			exit(1);
		}
	}

#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
		
	mloop();

	close(monsock);

	return 0;
}

/*---------------------------------------------------------------------------
 *	Connect via tcp/ip.
 *	Return socket if successfull, -1 on error.
 ---------------------------------------------------------------------------*/
static int
connect_remote(char *host, int portno)
{
	struct sockaddr_in sa;
	struct hostent *h;
	int remotesockfd;

	h = gethostbyname(host);

	if(!h)
	{
		fprintf(stderr, "could not resolve hostname '%s'\n", host);
		exit(1);
	}

	remotesockfd = socket(AF_INET, SOCK_STREAM, 0);

	if(remotesockfd == -1)
	{
		fprintf(stderr, "could not create remote monitor socket: %s\n", strerror(errno));
		exit(1);
	}

	memset(&sa, 0, sizeof(sa));

#ifdef BSD4_4
	sa.sin_len = sizeof(sa);
#endif
	sa.sin_family = AF_INET;
	sa.sin_port = htons(portno);

	memcpy(&sa.sin_addr.s_addr, h->h_addr_list[0], sizeof(sa.sin_addr.s_addr));

	if(connect(remotesockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
	{
		fprintf(stderr, "could not connect remote monitor: %s\n", strerror(errno));
		exit(1);
	}

	return remotesockfd;
}

#ifndef FOREIGN
/*---------------------------------------------------------------------------
 *	Connect local.
 *	Return socket on success, -1 on failure.
 *---------------------------------------------------------------------------*/
static int
connect_local(char *sockpath)
{
	int s;
	struct sockaddr_un sa;

	/* check path length */
	if(strlen(sockpath) >= sizeof(sa.sun_path))
	{
		fprintf(stderr, "pathname to long for local socket: %s\n",
			sockpath);
		exit(1);
	}

	/* create and setup socket */
	s = socket(AF_LOCAL, SOCK_STREAM, 0);

	if(s == -1)
	{
		fprintf(stderr, "could not create local monitor socket:%s\n", strerror(errno));
		exit(1);
	}

	memset(&sa, 0, sizeof(sa));

	sa.sun_len = sizeof(sa);
	sa.sun_family = AF_LOCAL;
	strcpy(sa.sun_path, sockpath);

	if(connect(s, (struct sockaddr *)&sa, sizeof(sa)))
	{
		fprintf(stderr, "could not connect local monitor socket [%s]: %s\n", sockpath, strerror(errno));
	}

	return s;
}
#endif

/*---------------------------------------------------------------------------*
 *	data from keyboard available, read and process it 
 *---------------------------------------------------------------------------*/
#ifndef WIN32
static void
kbdrdhdl(void)
{
	int ch = getch();
		
	switch(ch)
	{
		case 0x0c:	/* control L */
			wrefresh(curscr);
			break;
		
		case '\n':
		case '\r':
			do_menu();
			break;
	}
}
#endif

/*---------------------------------------------------------------------------
 *	main event loop
 *---------------------------------------------------------------------------*/
static void
mloop()
{
	for(;;)
	{
		fd_set rd, wr, ex;

		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_ZERO(&ex);
		FD_SET(fileno(stdin), &rd);
		FD_SET(monsock, &rd);

		select(monsock+1, &rd, &wr, &ex, NULL);

		if(FD_ISSET(fileno(stdin), &rd))
		{
#ifndef WIN32
			if(fullscreen && curses_ready)
				kbdrdhdl();
			else
#endif
			     if(!fullscreen)
				handle_input();
			else
				getchar();
		}

		if(FD_ISSET(monsock, &rd))
		{
			u_int8_t buf[8192];
			int bytes, ret;

			/* Network transfer may deliver two or more packets concatenated.
			 * Peek at the header and read only one event at a time... */

			bytes = recv(monsock, buf, I4B_MON_EVNT_HDR, MSG_PEEK);

			if(bytes == 0)
			{
				close(monsock);

#ifndef WIN32
				if(curses_ready)
				{
					endwin();
					curses_ready = 0;
				}
#endif
				
				mprintf("remote isdnd has closed our connection\n");
				exit(0);
			}
			else if(bytes < 0)
			{
				fprintf(stderr, "recv error: %s\n", strerror(errno));
				close(monsock);
				exit(1);
			}

			if (bytes < I4B_MON_EVNT_HDR)
				continue;	/* errh? something must be wrong... */

			bytes = I4B_GET_2B(buf, I4B_MON_EVNT_LEN);

			if(bytes >= sizeof(buf))
			{
				fprintf(stderr, "mloop: socket recv buffer overflow %d!\n", bytes);
				break;
			}

			/* now we know the size, it fits, so lets read it! */
			
			ret = sock_read(monsock, buf, bytes);

			if(ret == 0)
			{
				close(monsock);
#ifndef WIN32
				if(curses_ready)
					endwin();
#endif
				mprintf("remote isdnd has closed our connection\n");
				exit(0);
			}
			else if(ret < 0)
			{
				mprintf("error reading from isdnd: %s", strerror(errno));
				break;
			}
#ifdef DEBUG
			if(debug & DBG_DUMPALL)
				dump_event(buf, ret, 1);
#endif
			handle_event(buf, ret);
		}
	}
}

#ifdef DEBUG
/*
 * Dump a complete event packet.
 */
static void dump_event(u_int8_t *msg, int len, int read)
{
	int i;

	if(read)
		mprintf("read from socket:");
	else
		mprintf("write to socket:");

	for(i = 0; i < len; i++)
	{
		if(i % 8 == 0)
			mprintf("\n%02d: ", i);
		mprintf("0x%02x %c  ", msg[i], isprint(msg[i]) ? msg[i] : '.');
	}
	mprintf("\n");
}
#endif

static void
print_logevent(time_t tstamp, int prio, char * what, char * msg)
{
	char buf[256];
	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));
	mprintf("log: %s prio %d what=%s msg=%s\n", buf, prio, what, msg);

#ifndef WIN32
	if(fullscreen)
	{
		if((!debug_noscreen) || (debug_noscreen && (((strcmp(what, "DBG"))) != 0)))
		{
/*
 * FreeBSD-current integrated ncurses. Since then it is no longer possible
 * to write to the last column in the logfilewindow without causing an
 * automatic newline to occur resulting in a blank line in that window.
 */
#ifdef __FreeBSD__
#include <osreldate.h>
#endif
#if defined(__FreeBSD_version) && __FreeBSD_version >= 400009
#warning "FreeBSD ncurses is buggy: write to last column = auto newline!"
	                wprintw(lower_w, "%s %s %-.*s\n", buf, what,
				COLS-((strlen(buf))+(strlen(what))+3), msg);
#else
	                wprintw(lower_w, "%s %s %-.*s\n", buf, what,
				COLS-((strlen(buf))+(strlen(what))+2), msg);
#endif
			wrefresh(lower_w);
                }
        }
#endif
}

static void
print_charge(time_t tstamp, int controller, int channel, int units, int estimated)
{
	char buf[256];
	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));
	mprintf("%s: controller %d, channel %d, charge = %d%s\n",
		buf, controller, channel, units, estimated ? " (estimated)" : "");
#ifndef WIN32
	if(fullscreen)
	{
		if(estimated)
			display_ccharge(CHPOS(controller, channel), units);
		else
			display_charge(CHPOS(controller, channel), units);
	}
#endif
}

/*
 * Print a connect event.
 * A real monitor would allocate state info for "channel" on this
 * event.
 */
static void print_connect(
	time_t tstamp, 	/* server time of event */
	int outgoing,	/* 0 = incoming, 1 = outgoing */
	int controller, /* controller number */
	int channel,	/* channel no, used to identify this connection until disconnect */
	char * cfgname, 	/* name of config entry/connection */
	char * devname, 	/* device used (e.g. isp0) */
	char * remphone, 	/* phone no of remote side */
	char * locphone)	/* local phone no */
{
	char buf[256];

	if(channel == 0)
		remstate[controller].ch1state = 1;
	else
		remstate[controller].ch2state = 1;

	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));

	if(outgoing)
		mprintf("%s: calling out to '%s' [from msn: '%s']",
			buf, remphone, locphone);
	else
		mprintf("%s: incoming call from '%s' [to msn: '%s']",
			buf, remphone, locphone);
	mprintf(", controller %d, channel %d, config '%s' on device '%s'\n",
		controller, channel, cfgname, devname);

#ifndef WIN32
	if(fullscreen)
		display_connect(CHPOS(controller, channel), outgoing, cfgname, remphone, devname);
#endif
}

/*
 * Print a disconnect event.
 * A real monitor could free the "per connection" state
 * for this channel now
 */
static void
print_disconnect(time_t tstamp, int controller, int channel)
{
	char buf[256];

	if(channel == 0)
		remstate[controller].ch1state = 0;
	else
		remstate[controller].ch2state = 0;

	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));

	mprintf("%s: controller %d, channel %d disconnected\n",
		buf, controller, channel);

#ifndef WIN32
	if(fullscreen)
		display_disconnect(CHPOS(controller, channel));
#endif
}

/*
 * Print an up- or down event
 */
static void
print_updown(time_t tstamp, int controller, int channel, int isup)
{
	char buf[256];
	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));
	mprintf("%s: channel %d is %s\n",
		buf, channel, isup ? "up" : "down");
}

/*
 * Print l1 / l2 status
 */
static void
print_l12stat(time_t tstamp, int controller, int layer, int state)
{
	char buf[256];
	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));

	mprintf("%s: layer %d change on controller %d: %s\n",
		buf, layer, controller, state ? "up" : "down");
#ifndef WIN32
	if(fullscreen)
		display_l12stat(controller, layer, state);
#endif
}

/*
 * Print TEI
 */
static void
print_tei(time_t tstamp, int controller, int tei)
{
	char buf[256];
	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));

	mprintf("%s: controller %d, TEI is %d\n",
		buf, controller, tei);

#ifndef WIN32
	if(fullscreen)
		display_tei(controller, tei);
#endif
}

/*
 * Print accounting information
 */
static void
print_acct(time_t tstamp, int controller, int channel, int obytes, int obps,
		int ibytes, int ibps)
{
	char buf[256];
	strftime(buf, sizeof(buf), I4B_TIME_FORMAT, localtime(&tstamp));

	mprintf("%s: controller %d, channel %d: %d obytes, %d obps, %d ibytes, %d ibps\n",
		buf, controller, channel, obytes, obps, ibytes, ibps);
#ifndef WIN32
	if(fullscreen)
		display_acct(CHPOS(controller, channel), obytes, obps, ibytes, ibps);
#endif
}

static void
print_initialization(void)
{
#ifndef WIN32
	if(fullscreen)
	{
		if(curses_ready == 0)
			init_screen();
	}
	else
#endif
	{
		print_menu();
	}
}

/*
 * Dispatch one message received from the daemon.
 */
static void
handle_event(u_int8_t *msg, int len)
{
	u_int8_t cmd[I4B_MON_ICLIENT_SIZE];
	int local;	
	u_int32_t net;
	u_int32_t mask;
	u_int32_t who;
	static int first = 1;
	
	switch(state)
	{
		case ST_INIT:	/* initial data */

			isdn_major = I4B_GET_2B(msg, I4B_MON_IDATA_VERSMAJOR);
			isdn_minor = I4B_GET_2B(msg, I4B_MON_IDATA_VERSMINOR);
			nctrl = I4B_GET_2B(msg, I4B_MON_IDATA_NUMCTRL);
			nentries = I4B_GET_2B(msg, I4B_MON_IDATA_NUMENTR);
			rights = I4B_GET_4B(msg, I4B_MON_IDATA_CLACCESS);

			mprintf("remote protocol version is %02d.%02d\n", isdn_major, isdn_minor);

			if(isdn_major != MPROT_VERSION || isdn_minor != MPROT_REL)
			{
				fprintf(stderr, "ERROR, remote protocol version mismatch:\n");
				fprintf(stderr, "\tremote major version is %02d, local major version is %02d\n", isdn_major, MPROT_VERSION);
				fprintf(stderr, "\tremote minor version is %02d, local minor version is %02d\n", isdn_minor, MPROT_REL);
				exit(1);
			}

			mprintf("our rights = 0x%x\n", rights);

			sub_state = 0;				
			first = 1;
			
			if(nctrl > 0)
			{
				state = ST_ICTRL;
			}
			else if(nentries > 0)
			{
				state = ST_IDEV;
			}
			else
			{
				state = ST_ANYEV;
				sleep(2);
				print_initialization();
			}
			
			/* set maximum event mask */
			I4B_PREP_CMD(cmd, I4B_MON_CCMD_SETMASK);
			I4B_PUT_2B(cmd, I4B_MON_ICLIENT_VERMAJOR, MPROT_VERSION);
			I4B_PUT_2B(cmd, I4B_MON_ICLIENT_VERMINOR, MPROT_REL);
			I4B_PUT_4B(cmd, I4B_MON_ICLIENT_EVENTS, ~0U);

#ifdef DEBUG
			if(debug & DBG_DUMPALL)
				dump_event(cmd, sizeof(cmd), 0);
#endif
			
			if((sock_write(monsock, cmd, sizeof(cmd))) == -1)
			{
				fprintf(stderr, "sock_write failed: %s\n", strerror(errno));
				exit(1);
			}
			break;

		case ST_ICTRL:	/* initial controller list */
			if(first)
			{
				first = 0;
				mprintf("%d controller(s) found:\n", nctrl);
			}
			mprintf("\tcontroller %d: %s\n", sub_state++, msg+I4B_MON_ICTRL_NAME);

			if(sub_state >= nctrl)
			{
				sub_state = 0;
				first = 1;
				if(nentries > 0)
				{
					state = ST_IDEV; /* end of list reached */
				}
				else
				{
					state = ST_ANYEV;
					sleep(2);
					print_initialization();
				}
			}
			break;

		case ST_IDEV:	/* initial entry devicename list */
			if(first)
			{
				first = 0;
				mprintf("%d entries found:\n", nentries);
			}
			
			mprintf("\tentry %d: device %s\n", sub_state++, msg+I4B_MON_IDEV_NAME);

			strcat(devbuf, msg+I4B_MON_IDEV_NAME);
			/* strcat(devbuf, " "); */
			
			if(sub_state >= nentries)
			{
				first = 1;
				state = ST_ANYEV; /* end of list reached */
				sub_state = 0;
				sleep(2);
				print_initialization();
			}
			break;

		case ST_ANYEV: /* any event */
			switch(I4B_GET_2B(msg, I4B_MON_EVNT))
			{
				case I4B_MON_DRINI_CODE:
					state = ST_RIGHT;	/* list of rights entries will follow */
					sub_state = 0;
					sub_state_count = I4B_GET_2B(msg, I4B_MON_DRINI_COUNT);
					mprintf("monitor rights:\n");
					break;

				case I4B_MON_DCINI_CODE:
					state = ST_CONNS;
					sub_state = 0;
					sub_state_count = I4B_GET_2B(msg, I4B_MON_DCINI_COUNT);
					mprintf("monitor connections:\n");
					break;

				case I4B_MON_LOGEVNT_CODE:
					print_logevent(I4B_GET_4B(msg, I4B_MON_LOGEVNT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_LOGEVNT_PRIO),
						msg+I4B_MON_LOGEVNT_WHAT,
						msg+I4B_MON_LOGEVNT_MSG);
					break;

				case I4B_MON_CHRG_CODE:
					print_charge(I4B_GET_4B(msg, I4B_MON_CHRG_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_CHRG_CTRL),
						I4B_GET_4B(msg, I4B_MON_CHRG_CHANNEL),
						I4B_GET_4B(msg, I4B_MON_CHRG_UNITS),
						I4B_GET_4B(msg, I4B_MON_CHRG_ESTIMATED));
					break;
					
				case I4B_MON_CONNECT_CODE:
					print_connect(
						I4B_GET_4B(msg, I4B_MON_CONNECT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_CONNECT_DIR),
						I4B_GET_4B(msg, I4B_MON_CONNECT_CTRL),
						I4B_GET_4B(msg, I4B_MON_CONNECT_CHANNEL),
						msg+I4B_MON_CONNECT_CFGNAME,
						msg+I4B_MON_CONNECT_DEVNAME,
						msg+I4B_MON_CONNECT_REMPHONE,
						msg+I4B_MON_CONNECT_LOCPHONE);
					break;
					
				case I4B_MON_DISCONNECT_CODE:
					print_disconnect(
						I4B_GET_4B(msg, I4B_MON_DISCONNECT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_DISCONNECT_CTRL),
						I4B_GET_4B(msg, I4B_MON_DISCONNECT_CHANNEL));
					break;
					
				case I4B_MON_UPDOWN_CODE:
					print_updown(
						I4B_GET_4B(msg, I4B_MON_UPDOWN_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_UPDOWN_CTRL),
						I4B_GET_4B(msg, I4B_MON_UPDOWN_CHANNEL),
						I4B_GET_4B(msg, I4B_MON_UPDOWN_ISUP));
					break;
				case I4B_MON_L12STAT_CODE:
					print_l12stat(
						I4B_GET_4B(msg, I4B_MON_L12STAT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_L12STAT_CTRL),
						I4B_GET_4B(msg, I4B_MON_L12STAT_LAYER),
						I4B_GET_4B(msg, I4B_MON_L12STAT_STATE));
					break;
				case I4B_MON_TEI_CODE:
					print_tei(
						I4B_GET_4B(msg, I4B_MON_TEI_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_TEI_CTRL),
						I4B_GET_4B(msg, I4B_MON_TEI_TEI));
					break;
				case I4B_MON_ACCT_CODE:
					print_acct(
						I4B_GET_4B(msg, I4B_MON_ACCT_TSTAMP),
						I4B_GET_4B(msg, I4B_MON_ACCT_CTRL),
						I4B_GET_4B(msg, I4B_MON_ACCT_CHAN),
						I4B_GET_4B(msg, I4B_MON_ACCT_OBYTES),
						I4B_GET_4B(msg, I4B_MON_ACCT_OBPS),
						I4B_GET_4B(msg, I4B_MON_ACCT_IBYTES),
						I4B_GET_4B(msg, I4B_MON_ACCT_IBPS));
					break;
				default:
					mprintf("unknown event code: %d\n", I4B_GET_2B(msg, I4B_MON_EVNT));
			}
			break;

		case ST_RIGHT:	/* one record in a list of monitor rights */
			rights = I4B_GET_4B(msg, I4B_MON_DR_RIGHTS);
			net = I4B_GET_4B(msg, I4B_MON_DR_NET);
			mask = I4B_GET_4B(msg, I4B_MON_DR_MASK);
			local = I4B_GET_1B(msg, I4B_MON_DR_LOCAL);

			if(local)
			{
				mprintf("\tlocal: rights = %x\n", rights);
			}
			else
			{
				mprintf("\tfrom: %d.%d.%d.%d, mask %d.%d.%d.%d, rights = %x\n",
					(net >> 24) & 0x00ff, (net >> 16) & 0x00ff, (net >> 8) & 0x00ff, net & 0x00ff,
					(mask >> 24) & 0x00ff, (mask >> 16) & 0x00ff, (mask >> 8) & 0x00ff, mask & 0x00ff,
					rights);
			}

			sub_state++;

			if(sub_state >= sub_state_count)
			{
				state = ST_ANYEV;
				print_initialization();
			}
			break;

		case ST_CONNS:
			who = I4B_GET_4B(msg, I4B_MON_DC_WHO);
			rights = I4B_GET_4B(msg, I4B_MON_DC_RIGHTS);

			mprintf("\tfrom: %d.%d.%d.%d, rights = %x\n",
				(who >> 24) & 0x00ff, (who >> 16) & 0x00ff, (who >> 8) & 0x00ff, who & 0x00ff,
				rights);

			sub_state++;

			if(sub_state >= sub_state_count)
			{
				state = ST_ANYEV;
				print_initialization();
			}
			break;

		default:
			mprintf("unknown event from remote: local state = %d, evnt = %x, len = %d\n",
				state, I4B_GET_2B(msg, I4B_MON_EVNT), len);
	}
}

/*
 * Process input from user
 */
static void
handle_input()
{
	char buf[1024];
	int channel, controller;
	
	fgets(buf, sizeof(buf), stdin);

	switch(atoi(buf))
	{
		case 1:
		    {
		    	u_int8_t cmd[I4B_MON_DUMPRIGHTS_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_DUMPRIGHTS_CODE);
#ifdef DEBUG
			if(debug & DBG_DUMPALL)
				dump_event(cmd, I4B_MON_DUMPRIGHTS_SIZE, 0);
#endif

			if((sock_write(monsock, cmd, I4B_MON_DUMPRIGHTS_SIZE)) == -1)
			{
				fprintf(stderr, "sock_write failed: %s\n", strerror(errno));
				exit(1);
			}
		    }
		    break;

		case 2:
		    {
		    	u_int8_t cmd[I4B_MON_DUMPMCONS_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_DUMPMCONS_CODE);
#ifdef DEBUG
			if(debug & DBG_DUMPALL)
				dump_event(cmd, I4B_MON_DUMPMCONS_CODE, 0);
#endif

			if((sock_write(monsock, cmd, I4B_MON_DUMPMCONS_SIZE)) == -1)
			{
				fprintf(stderr, "sock_write failed: %s\n", strerror(errno));
				exit(1);
			}			
		    }
		    break;

		case 3:
		    {
		    	u_int8_t cmd[I4B_MON_CFGREREAD_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_CFGREREAD_CODE);
#ifdef DEBUG
			if(debug & DBG_DUMPALL)
				dump_event(cmd, I4B_MON_CFGREREAD_CODE, 0);
#endif

			if((sock_write(monsock, cmd, I4B_MON_CFGREREAD_SIZE)) == -1)
			{
				fprintf(stderr, "sock_write failed: %s\n", strerror(errno));
				exit(1);
			}
		    }
		    break;
			
		case 4:
		    {
		    	u_int8_t cmd[I4B_MON_HANGUP_SIZE];
			I4B_PREP_CMD(cmd, I4B_MON_HANGUP_CODE);
			
			printf("Which controller you wish to hangup? ");
			fgets(buf, sizeof(buf), stdin);
			controller = atoi(buf);
			I4B_PUT_4B(cmd, I4B_MON_HANGUP_CTRL, controller);

			printf("Which channel do you wish to hangup? ");
			fgets(buf, sizeof(buf), stdin);
			channel = atoi(buf);
			I4B_PUT_4B(cmd, I4B_MON_HANGUP_CHANNEL, channel);
			
#ifdef DEBUG
			if(debug & DBG_DUMPALL)
				dump_event(cmd, I4B_MON_HANGUP_CHANNEL, 0);
#endif

			if((sock_write(monsock, cmd, I4B_MON_HANGUP_SIZE)) == -1)
			{
				fprintf(stderr, "sock_write failed: %s\n", strerror(errno));
				exit(1);
			}			
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

void
reread(void)
{
    	u_int8_t cmd[I4B_MON_CFGREREAD_SIZE];
	I4B_PREP_CMD(cmd, I4B_MON_CFGREREAD_CODE);
#ifdef DEBUG
	if(debug & DBG_DUMPALL)
		dump_event(cmd, I4B_MON_CFGREREAD_CODE, 0);
#endif
	if((sock_write(monsock, cmd, I4B_MON_CFGREREAD_SIZE)) == -1)
	{
		fprintf(stderr, "sock_write failed: %s\n", strerror(errno));
		exit(1);
	}
}

void
hangup(int ctrl, int chan)
{
    	u_int8_t cmd[I4B_MON_HANGUP_SIZE];

	I4B_PREP_CMD(cmd, I4B_MON_HANGUP_CODE);
	I4B_PUT_4B(cmd, I4B_MON_HANGUP_CTRL, ctrl);
	I4B_PUT_4B(cmd, I4B_MON_HANGUP_CHANNEL, chan);
			
#ifdef DEBUG
	if(debug & DBG_DUMPALL)
		dump_event(cmd, I4B_MON_HANGUP_CHANNEL, 0);
#endif

	if((sock_write(monsock, cmd, I4B_MON_HANGUP_SIZE)) == -1)
	{
		fprintf(stderr, "sock_write failed: %s\n", strerror(errno));
		exit(1);
	}			
}

/*
 * Display menu
 */
static void
print_menu()
{
	if(!fullscreen)
	{
		printf("Menu: <1> display rights,     <2> display monitor connections,\n");
		printf("      <3> reread config file, <4> hangup \n");
		printf("      <9> quit isdnmonitor\n");
		fflush(stdout);
	}
}

static ssize_t
sock_read(int fd, void *buf, size_t nbytes)
{
	size_t nleft;
	ssize_t nread;
	unsigned char *ptr;

	ptr = buf;
	nleft = nbytes;

	while(nleft > 0)
	{
		if((nread = read(fd, ptr, nleft)) < 0)
		{
			if(errno == EINTR)
			{
				nread = 0;
			}
			else
			{
				return(-1);
			}
		}
		else if(nread == 0)
		{
			break; /* EOF */
		}

		nleft -= nread;
		ptr += nread;
	}
	return(nbytes - nleft);
}

static ssize_t
sock_write(int fd, void *buf, size_t nbytes)
{
	size_t nleft;
	ssize_t nwritten;
	unsigned char *ptr;

	ptr = buf;
	nleft = nbytes;

	while(nleft > 0)
	{
		if((nwritten = write(fd, ptr, nleft)) <= 0)
		{
			if(errno == EINTR)
			{
				nwritten = 0;
			}
			else
			{
				return(-1);
			}
		}

		nleft -= nwritten;
		ptr += nwritten;
	}
	return(nbytes);
}

static void
mprintf(char *fmt, ...)
{
#define	PRBUFLEN 1024
	char buffer[PRBUFLEN];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, PRBUFLEN-1, fmt, ap);
	va_end(ap);

	if(!fullscreen || (fullscreen && (!curses_ready)))
		printf("%s", buffer);
	
	if(logfilename != NULL)
	{
		fprintf(lfp, "%s", buffer);
		fflush(lfp);
	}
}

/* EOF */
