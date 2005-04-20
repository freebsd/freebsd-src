/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	main.c - i4b selftest utility
 *	-----------------------------
 *
 *	$Id: main.c,v 1.16 2000/03/13 16:18:38 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Mar 13 17:19:26 2000]
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>

#include <machine/i4b_ioctl.h>
#include <machine/i4b_cause.h>

static void kbdrdhdl ( void );
static void isdnrdhdl (int isdnfd );

void handle_connect_ind(unsigned char *ptr);
void handle_disconnect(unsigned char *ptr);
void handle_connect_active_ind(unsigned char *ptr);

int connect_response(int isdnfd, unsigned int cdid, int response);
int disconnect_request(int isdnfd, unsigned int cdid);
unsigned int get_cdid(int isdnfd);
int connect_request(int isdnfd, unsigned int cdid);
int do_test(void);
static void cleanup(void);
static void usage(void);
void setup_wrfix(int len, unsigned char *buf);
int check_rd(int len, unsigned char *wbuf, unsigned char *rdbuf);

static int isdnfd;
char outgoingnumber[32];
char incomingnumber[32];
int debug_level = 0;

#define I4BDEVICE	"/dev/i4b"
#define DATADEV0	"/dev/i4brbch0"
#define DATAUNIT0	0
#define DATADEV1	"/dev/i4brbch1"
#define DATAUNIT1	1

unsigned int out_cdid = CDID_UNUSED;
unsigned int in_cdid = CDID_UNUSED;

int waitchar = 0;
int usehdlc = 0;
int controller = 0;
int dotest = 0;

/*---------------------------------------------------------------------------*
 *	usage display and exit
 *---------------------------------------------------------------------------*/
static void
usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "isdntest - i4b selftest, version %d.%d.%d, compiled %s %s\n",VERSION, REL, STEP, __DATE__, __TIME__);
	fprintf(stderr, "usage: isdntest [-c ctrl] [-d level] [-h] [-i telno] [-o telno] [-t num] [-w]\n");
	fprintf(stderr, "       -c <ctrl>     specify controller to use\n");
	fprintf(stderr, "       -d <level>    set debug level\n");	
	fprintf(stderr, "       -h            use HDLC as Bchannel protocol\n");
	fprintf(stderr, "       -i <telno>    incoming telephone number\n");
	fprintf(stderr, "       -o <telno>    outgoing telephone number\n");
	fprintf(stderr, "       -t <num>      send test pattern num times\n");
	fprintf(stderr, "       -w            wait for keyboard entry to disconnect\n");
	fprintf(stderr, "\n");
	exit(1);
}

/*---------------------------------------------------------------------------*
 *	program entry
 *---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	int i;
	int c;
	fd_set set;
	int ret;
	char *ptr;
	
	incomingnumber[0] = '\0';
	outgoingnumber[0] = '\0';	
	
	while ((c = getopt(argc, argv, "c:d:hi:o:t:w")) != -1)
	{
		switch(c)
		{
			case 'c':
				if(isdigit(*optarg))
				{
					controller = strtoul(optarg, NULL, 10);
				}
				else
				{
					fprintf(stderr, "Error: option -c requires a numeric argument!\n");
					usage();
				}
				break;

			case 'd':
				if(isdigit(*optarg))
				{
					debug_level = strtoul(optarg, NULL, 10);
				}
				else
				{
					fprintf(stderr, "Error: option -d requires a numeric argument!\n");
					usage();
				}
				break;

			case 'o':
				i = 0;
				ptr = optarg;

				while(*ptr)
				{
					if(isdigit(*ptr))
					{
						outgoingnumber[i++] = *ptr++;
					}
					else
					{
						fprintf(stderr, "Error: option -o requires a numeric argument!\n");
						usage();
					}
				}					
				outgoingnumber[i] = '\0';
				break;

			case 'i':
				i = 0;
				ptr = optarg;

				while(*ptr)
				{
					if(isdigit(*ptr))
					{
						incomingnumber[i++] = *ptr++;
					}
					else
					{
						fprintf(stderr, "Error: option -i requires a numeric argument!\n");
						usage();
					}
				}					
				incomingnumber[i] = '\0';
				break;

			case 'w':
				waitchar = 1;
				break;
				
			case 'h':
				usehdlc = 1;
				break;
				
			case 't':
				if(isdigit(*optarg))
				{
					dotest = strtoul(optarg, NULL, 10);
				}
				else
				{
					fprintf(stderr, "Error: option -t requires a numeric argument!\n");
					usage();
				}
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if((strlen(incomingnumber) == 0) || (strlen(outgoingnumber) == 0))
		usage();

	fprintf(stderr, "isdntest: accepting calls from telephone number [%s] \n", incomingnumber);
	fprintf(stderr, "isdntest:          calling out telephone number [%s] \n", outgoingnumber);

	if((atexit(cleanup)) != 0)
	{
		fprintf(stderr, "isdntest: atexit error: %s\n", strerror(errno));
		exit(1);
	}
	
	/* open isdn device */
	
	if((isdnfd = open(I4BDEVICE, O_RDWR)) < 0)
	{
		fprintf(stderr, "\nisdntest: cannot open %s: %s\n", I4BDEVICE, strerror(errno));
		fprintf(stderr, "          isdnd is probably running, to use isdntest,\n");
		fprintf(stderr, "          terminate isdnd and then run isdntest again!\n");
		exit(1);
	}

	if((out_cdid = get_cdid(isdnfd)) == 0)
	{
		fprintf(stderr, "isdntest: error getting cdid: %s\n", strerror(errno));
		exit(1);
	}

	if((connect_request(isdnfd, out_cdid)) == -1)
	{
		fprintf(stderr, "isdntest: error, outgoing call failed!\n");
		exit(1);
	}
	
	for(;;)
	{
		FD_ZERO(&set);

		FD_SET(0, &set);

		FD_SET(isdnfd, &set);

		ret = select(isdnfd + 1, &set, NULL, NULL, NULL);

		if(ret > 0)
		{
			if(FD_ISSET(isdnfd, &set))
				isdnrdhdl(isdnfd);

			if(FD_ISSET(0, &set))
				kbdrdhdl();
		}
		else
		{
			fprintf(stderr, "isdntest: select error: %s\n", strerror(errno));
		}			
	}
}

/*---------------------------------------------------------------------------*
 *	data from keyboard available
 *---------------------------------------------------------------------------*/
static void
kbdrdhdl(void)
{
	cleanup();
	exit(2);
}

/*---------------------------------------------------------------------------*
 *	data from /dev/isdn available, read and process them
 *---------------------------------------------------------------------------*/
static void
isdnrdhdl(int isdnfd)
{
	static unsigned char buf[1024];
	int len;

	if((len = read(isdnfd, buf, 1024 - 1)) > 0)
	{
		switch (buf[0])
		{
			case MSG_CONNECT_IND:
				handle_connect_ind(&buf[0]); 
				break;
				
			case MSG_CONNECT_ACTIVE_IND:
				handle_connect_active_ind(&buf[0]); 
				break;
				
			case MSG_DISCONNECT_IND:
				handle_disconnect(&buf[0]); 
				break;
				
			default:
				if(debug_level)
					fprintf(stderr, "isdntest: unknown message 0x%x = %c\n", buf[0], buf[0]);
				break;
		}
	}
	else
	{
		fprintf(stderr, "isdntest: read error, errno = %d, length = %d", errno, len);
	}
}

/*---------------------------------------------------------------------------*
 *	initiate an outgoing connection
 *---------------------------------------------------------------------------*/
int
connect_request(int isdnfd, unsigned int cdid)
{
	msg_connect_req_t mcr;
	int ret;

	bzero(&mcr, sizeof(msg_connect_req_t));
	
	mcr.controller = controller;
	mcr.channel = CHAN_ANY;	/* any channel */
	mcr.cdid = cdid;	/* cdid from get_cdid() */	

	if(usehdlc)
		mcr.bprot = BPROT_RHDLC;/* b channel protocol */
	else
		mcr.bprot = BPROT_NONE;	/* b channel protocol */	

	mcr.driver = BDRV_RBCH;		/* raw b channel driver */
	mcr.driver_unit = DATAUNIT0;	/* raw b channel driver unit */	

	strcpy(mcr.dst_telno, outgoingnumber);
	strcpy(mcr.src_telno, incomingnumber);
	
	if((ret = ioctl(isdnfd, I4B_CONNECT_REQ, &mcr)) < 0)
	{
		fprintf(stderr, "ioctl I4B_CONNECT_REQ failed: %s", strerror(errno));
		return(-1);
	}
	fprintf(stderr, "isdntest: calling out to telephone number [%s] \n", outgoingnumber);
	return(0);
}

/*---------------------------------------------------------------------------*
 *	handle setup indicator
 *---------------------------------------------------------------------------*/
void
handle_connect_ind(unsigned char *ptr)
{
	msg_connect_ind_t *msi = (msg_connect_ind_t *)ptr;

	fprintf(stderr, "isdntest: incoming SETUP: from %s to %s\n",
				msi->src_telno,
				msi->dst_telno);

	fprintf(stderr, "          channel %d, controller %d, bprot %d, cdid %d\n",
				msi->channel,
				msi->controller,
				msi->bprot,
				msi->header.cdid);

	in_cdid = msi->header.cdid;
	
	if(strcmp(msi->dst_telno, outgoingnumber))
	{
		msg_connect_resp_t msr;
		int ret;

		fprintf(stderr, "isdntest: ignoring incoming SETUP: my number [%s] != outgoing [%s]\n",
			msi->dst_telno, outgoingnumber);

		msr.cdid = in_cdid;
		msr.response = SETUP_RESP_DNTCRE;

		if((ret = ioctl(isdnfd, I4B_CONNECT_RESP, &msr)) < 0)
		{
			fprintf(stderr, "ioctl I4B_CONNECT_RESP ignore failed: %s", strerror(errno));
			exit(1);
		}

	}
	else
	{
		msg_connect_resp_t msr;
		int ret;

		fprintf(stderr, "isdntest: accepting call, sending CONNECT_RESPONSE .....\n");

		msr.cdid = in_cdid;
		msr.response = SETUP_RESP_ACCEPT;

		if(usehdlc)
			msr.bprot = BPROT_RHDLC;
		else
			msr.bprot = BPROT_NONE;
	
		msr.driver = BDRV_RBCH;
		msr.driver_unit = DATAUNIT1;
		
		if((ret = ioctl(isdnfd, I4B_CONNECT_RESP, &msr)) < 0)
		{
			fprintf(stderr, "ioctl I4B_CONNECT_RESP accept failed: %s", strerror(errno));
			exit(1);
		}
	}
}

#define SLEEPTIME 5

/*---------------------------------------------------------------------------*
 *	handle connection active
 *---------------------------------------------------------------------------*/
void
handle_connect_active_ind(unsigned char *ptr)
{
	msg_connect_active_ind_t *msi = (msg_connect_active_ind_t *)ptr;
	int i;
	
	fprintf(stderr, "isdntest: connection active, cdid %d\n", msi->header.cdid);

	if(out_cdid == msi->header.cdid)
	{
		if(waitchar)
		{
			fprintf(stderr, "isdntest: press any key to disconnect ...%c%c%c\n", 0x07, 0x07, 0x07);
			getchar();
		}
		else
		{
			if(dotest)
			{
				do_test();
			}
			else
			{
				fprintf(stderr, "isdntest: %d secs delay until disconnect:", SLEEPTIME);
	
				for(i=0; i < SLEEPTIME;i++)
				{
					fprintf(stderr, " .");
					sleep(1);
				}
				fprintf(stderr, "\n");
			}
			cleanup();
			exit(0);
		}
	}
}

/*---------------------------------------------------------------------------*
 *	handle disconnect indication
 *---------------------------------------------------------------------------*/
void
handle_disconnect(unsigned char *ptr)
{
	msg_disconnect_ind_t *mdi = (msg_disconnect_ind_t *)ptr;
	
	if(mdi->header.cdid == out_cdid)
	{
		fprintf(stderr, "isdntest: incoming disconnect indication, cdid %d (out_cdid), cause %d\n",
			mdi->header.cdid, mdi->cause);

		out_cdid = CDID_UNUSED;
	}
	else if(mdi->header.cdid == in_cdid)
	{
		fprintf(stderr, "isdntest: incoming disconnect indication, cdid %d (in_cdid), cause %d\n",
			mdi->header.cdid, mdi->cause);
		in_cdid = CDID_UNUSED;
	}
	else
	{
		fprintf(stderr, "isdntest: incoming disconnect indication, cdid %d (???), cause %d\n",
			mdi->header.cdid, mdi->cause);
	}		
}
	
/*---------------------------------------------------------------------------*
 *	hang up
 *---------------------------------------------------------------------------*/
int
disconnect_request(int isdnfd, unsigned int cdid)
{
	msg_discon_req_t mdr;
	int ret;

	mdr.cdid = cdid;
	mdr.cause = (CAUSET_I4B << 8) | CAUSE_I4B_NORMAL;
	
	if((ret = ioctl(isdnfd, I4B_DISCONNECT_REQ, &mdr)) < 0)
	{
		fprintf(stderr, "ioctl I4B_DISCONNECT_REQ failed: %s", strerror(errno));
		return(-1);
	}
	fprintf(stderr, "isdntest: sending disconnect request\n");
	return(0);
}

/*---------------------------------------------------------------------------*
 *	get cdid from kernel
 *---------------------------------------------------------------------------*/
unsigned int
get_cdid(int isdnfd)
{
	msg_cdid_req_t mcr;
	int ret;
	
	mcr.cdid = 0;
	
	if((ret = ioctl(isdnfd, I4B_CDID_REQ, &mcr)) < 0)
	{
		fprintf(stderr, "ioctl I4B_CDID_REQ failed: %s", strerror(errno));
		return(0);
	}
	fprintf(stderr, "isdntest: got cdid %d from kernel\n", mcr.cdid);
	return(mcr.cdid);
}

/*---------------------------------------------------------------------------*
 *	make shure all cdid's are inactive before leaving
 *---------------------------------------------------------------------------*/
void cleanup(void)
{
	int len;
	char buf[1024];
	
	if(out_cdid != CDID_UNUSED)
	{
		fprintf(stderr, "isdntest: cleanup, send disconnect req for out_cdid %d, in_cdid %d\n", out_cdid, in_cdid);
		disconnect_request(isdnfd, out_cdid);
	}

	while((out_cdid != CDID_UNUSED) || (in_cdid != CDID_UNUSED))
	{
		if(debug_level)
			fprintf(stderr, "isdntest: cleanup, out_cdid %d, in_cdid %d\n", out_cdid, in_cdid);

		if((len = read(isdnfd, buf, 1024 - 1)) > 0)
		{
			switch (buf[0])
			{
				case MSG_CONNECT_IND:
					handle_connect_ind(&buf[0]); 
					break;
					
				case MSG_CONNECT_ACTIVE_IND:
					handle_connect_active_ind(&buf[0]); 
					break;
					
				case MSG_DISCONNECT_IND:
					handle_disconnect(&buf[0]); 
					break;
					
				default:
					if(debug_level)				
						fprintf(stderr, "isdntest: unknown message 0x%x = %c\n", buf[0], buf[0]);
					break;
			}
		}
		else
		{
			fprintf(stderr, "isdntest: read error, errno = %d, length = %d", errno, len);
		}
	}
	if(debug_level)	
		fprintf(stderr, "isdntest: exit cleanup, out_cdid %d, in_cdid %d\n", out_cdid, in_cdid);
}

/*---------------------------------------------------------------------------*
 *	test the b-channels
 *---------------------------------------------------------------------------*/
int do_test(void)
{

#define FPH 0x3c
#define FPL 0x66

	int fd0, fd1;
	unsigned char wrbuf[2048];
	unsigned char rdbuf[2048];
	int sz;
	fd_set set;
	struct timeval timeout;
	int ret;
	int frame;
	int errcnt;
	int frm_len;
	int bytecnt = 0;
	time_t start_time;
	time_t cur_time;
	time_t run_time;
	
	if((fd0 = open(DATADEV0, O_RDWR)) == -1)
	{
		fprintf(stderr, "open of %s failed: %s", DATADEV0, strerror(errno));
		return(-1);
	}		

	if((fd1 = open(DATADEV1, O_RDWR)) == -1)
	{
		fprintf(stderr, "open of %s failed: %s", DATADEV1, strerror(errno));
		return(-1);
	}		

	printf("\n");
	frame = 0;
	errcnt = 0;	

	frm_len = 2;

	start_time = time(NULL);

	printf(" frame size errors totalbytes  bps elapsedtime\n");
	
	for(;dotest > 0; dotest--)
	{	
		setup_wrfix(frm_len, &wrbuf[0]);

		frame++;

		bytecnt += frm_len;
		
		printf("%6d %4d", frame, frm_len);
		fflush(stdout);
		
		if((sz = write(fd0, wrbuf, frm_len)) != frm_len)
		{
			fprintf(stderr, "write (%d of %d bytes) to %s failed: %s\n", sz, frm_len, DATADEV0, strerror(errno));
		}

                timeout.tv_sec =  2;
                timeout.tv_usec = 0;
                                
		FD_ZERO(&set);

		FD_SET(0, &set);
		FD_SET(fd1, &set);		

		ret = select(fd1+1, &set, NULL, NULL, &timeout);

		if(ret > 0)
		{
			if(FD_ISSET(fd1, &set))
			{
				if((sz = read(fd1, rdbuf, 2048)) != frm_len)
				{
					fprintf(stderr, "read (%d bytes) from %s failed: %s\n", sz, DATADEV1, strerror(errno));
				}

				cur_time = time(NULL);
				run_time = difftime(cur_time, start_time);

				if(run_time == 0)
					run_time = 1;
				
				printf(" %6d %10d %4d %2.2d:%2.2d:%2.2d     \r",
					errcnt, bytecnt,
					(int)((int)bytecnt/(int)run_time),
					(int)run_time/3600, (int)run_time/60, (int)run_time%60);
				fflush(stdout);

				errcnt += check_rd(frm_len, &wrbuf[0], &rdbuf[0]);
				
#ifdef NOTDEF
				for(i=0; i<sz; i++)
				{
					printf("0x%02x ", (unsigned char)rdbuf[i]);
				}
				printf("\n");
#endif				
			}

			if(FD_ISSET(0, &set))
			{
				return(0);
				printf("\n\n");
			}
		}
		else
		{
			fprintf(stderr, "isdntest, do_test: select error: %s\n", strerror(errno));
		}

		frm_len = frm_len*2;
		if(frm_len > 2048)
			frm_len = 2;

	}
	printf("\n\n");
	return(0);
}

void
setup_wrfix(int len, unsigned char *buf)
{
	register int i;
	
	for(i=0; i<len;)
	{
		*buf = FPH;
		buf++;
		*buf = FPL;
		buf++;
		i+=2;
	}
}

int		
check_rd(int len, unsigned char *wbuf, unsigned char *rbuf)
{
	register int i;
	int ret = 0;
	
	for(i=0; i<len; i++)
	{
		if(*wbuf != *rbuf)
		{
			fprintf(stderr, "\nERROR, byte %d, written 0x%02x, read 0x%02x\n", i, *wbuf, *rbuf);
			ret++;
		}
		wbuf++;
		rbuf++;
	}
	return(ret);
}


/* EOF */
