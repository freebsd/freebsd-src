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
 *	isdntelctl - i4b set telephone interface options
 *	------------------------------------------------
 *
 *	$Id: main.c,v 1.12 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:54:50 1999]
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <paths.h>
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
#include <machine/i4b_tel_ioctl.h>

static void usage ( void );

#define I4BTELDEVICE	"/dev/i4btel"

int opt_get = 0;
int opt_unit = 0;
int opt_U = 0;
int opt_A = 0;
int opt_C = 0;
int opt_N = 0;

/*---------------------------------------------------------------------------*
 *	program entry
 *---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	int c;
	int ret;
	int telfd;
	char namebuffer[128];
	
	while ((c = getopt(argc, argv, "cgu:AUN")) != -1)
	{
		switch(c)
		{
			case 'c':
				opt_C = 1;
				break;

			case 'g':
				opt_get = 1;
				break;

			case 'u':
				opt_unit = atoi(optarg);
				if(opt_unit < 0 || opt_unit > 9)
					usage();
				break;

			case 'A':
				opt_A = 1;
				break;

			case 'U':
				opt_U = 1;
				break;

			case 'N':
				opt_N = 1;
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if(opt_get == 0 && opt_N == 0 && opt_U == 0 && opt_A == 0 && opt_C == 0)
	{
		opt_get = 1;
	}

	if((opt_get + opt_N + opt_U + opt_A + opt_C) > 1)
	{
		usage();
	}

	sprintf(namebuffer,"%s%d", I4BTELDEVICE, opt_unit);
	
	if((telfd = open(namebuffer, O_RDWR)) < 0)
	{
		fprintf(stderr, "isdntelctl: cannot open %s: %s\n", namebuffer, strerror(errno));
		exit(1);
	}

	if(opt_get)
	{
		int format;
		
		if((ret = ioctl(telfd, I4B_TEL_GETAUDIOFMT, &format)) < 0)
		{
			fprintf(stderr, "ioctl I4B_TEL_GETAUDIOFMT failed: %s", strerror(errno));
			exit(1);
		}

		if(format == CVT_NONE)
		{
			printf("device %s does not do A-law/u-law format conversion\n", namebuffer);
		}
		else if(format == CVT_ALAW2ULAW)
		{
			printf("device %s does ISDN: A-law -> user: u-law format conversion\n", namebuffer);
		}
		else if(format == CVT_ULAW2ALAW)
		{
			printf("device %s does ISDN: u-law -> user: A-law format conversion\n", namebuffer);
		}
		else
		{
			printf("ERROR, device %s uses unknown format %d!\n", namebuffer, format);
		}
		exit(0);
	}

	if(opt_A)
	{
		int format = CVT_ALAW2ULAW;
		
		if((ret = ioctl(telfd, I4B_TEL_SETAUDIOFMT, &format)) < 0)
		{
			fprintf(stderr, "ioctl I4B_TEL_SETAUDIOFMT failed: %s", strerror(errno));
			exit(1);
		}
		exit(0);
	}

	if(opt_U)
	{
		int format = CVT_ULAW2ALAW;
		
		if((ret = ioctl(telfd, I4B_TEL_SETAUDIOFMT, &format)) < 0)
		{
			fprintf(stderr, "ioctl I4B_TEL_SETAUDIOFMT failed: %s", strerror(errno));
			exit(1);
		}
		exit(0);
	}
	if(opt_N)
	{
		int format = CVT_NONE;
		
		if((ret = ioctl(telfd, I4B_TEL_SETAUDIOFMT, &format)) < 0)
		{
			fprintf(stderr, "ioctl I4B_TEL_SETAUDIOFMT failed: %s", strerror(errno));
			exit(1);
		}
		exit(0);
	}
	if(opt_C)
	{
		int dummy;
		if((ret = ioctl(telfd, I4B_TEL_EMPTYINPUTQUEUE, &dummy)) < 0)
		{
			fprintf(stderr, "ioctl I4B_TEL_EMPTYINPUTQUEUE failed: %s", strerror(errno));
			exit(1);
		}
		exit(0);
	}
	return(0);
}
	
/*---------------------------------------------------------------------------*
 *	usage display and exit
 *---------------------------------------------------------------------------*/
static void
usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "isdntelctl - %si4btel control, version %d.%d.%d (%s %s)\n", _PATH_DEV, VERSION, REL, STEP, __DATE__, __TIME__);
	fprintf(stderr, "usage: isdntelctl -c -g -u <unit> -A -N -U\n");
	fprintf(stderr, "       -c            clear input queue\n");
	fprintf(stderr, "       -g            get current settings\n");
	fprintf(stderr, "       -u unit       specify unit number\n");	
	fprintf(stderr, "       -A            set conversion ISDN: A-law -> user: u-law\n");
	fprintf(stderr, "       -U            set conversion ISDN: u-law -> user: A-law\n");
	fprintf(stderr, "       -N            set conversion to no A-law/u-law conversion\n");
	fprintf(stderr, "\n");
	exit(1);
}

/* EOF */
