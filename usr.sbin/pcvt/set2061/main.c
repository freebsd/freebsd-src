/*
 * Copyright (c) 1994 Hellmuth Michaelis
 *
 * All rights reserved.
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
 *	This product includes software developed by
 *	Hellmuth Michaelis
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

static char *id =
	"@(#)set2061.c, 1.00, Last Edit-Date: [Sun Jan 15 19:52:05 1995]";

/*---------------------------------------------------------------------------*
 *
 *	history:
 *
 *	-hm	start using 132 columns on my Elsa Winner
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <fcntl.h>
#include <machine/pcvt_ioctl.h>

#define DEFAULTFD 0

void AltICD2061SetClock(long frequency, int select);

main(argc,argv)
int argc;
char *argv[];
{
	extern int optind;
	extern int opterr;
	extern char *optarg;

	int fd;
	int c;
	long freq = -1;
	int no = -1;

	while( (c = getopt(argc, argv, "f:n:")) !=  -1)
	{
		switch(c)
		{
			case 'f':
				freq = atoi(optarg);
				break;

			case 'n':
				no = atoi(optarg);
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if(freq == -1 || no == -1)
		usage();

	if((fd = open("/dev/console", O_RDONLY)) < 0)
		fd = DEFAULTFD;

	if(ioctl(fd, KDENABIO, 0) < 0)
	{
		perror("ioctl(KDENABIO)");
		return 1;
	}

	AltICD2061SetClock(freq, no);

	(void)ioctl(fd, KDDISABIO, 0);

	exit(0);
}

usage()
{
	fprintf(stderr,"\nset2061 - program the ICD2061 video clock chip\n");
	fprintf(stderr,"usage: set2061 -f <freq> -n <no>\n");
	fprintf(stderr,"       -f <freq>     frequency in Hz\n");
	fprintf(stderr,"       -n <no>       clock generator number\n");
	exit(1);
}

