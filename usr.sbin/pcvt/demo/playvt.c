/*
 * Copyright (c) 1995 Hellmuth Michaelis
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
 *	This product includes software developed by Hellmuth Michaelis
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

static char *id =
	"@(#)playvt.c, 1.00, Last Edit-Date: [Sun Jan  1 18:32:22 1995]";

/*---------------------------------------------------------------------------*
 *
 *	history:
 *
 *	-hm	want to see my xmas greeting ... :-)
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>

main(argc,argv)
int argc;
char *argv[];
{
	extern int optind;
	extern int opterr;
	extern char *optarg;

	int c;
	FILE *fp = stdin;
	volatile int i;
	int delay = 0;
	int fflag = -1;
	char *filename;

	while( (c = getopt(argc, argv, "d:f:")) != -1)
	{
		switch(c)
		{
			case 'd':
				delay = atoi(optarg);
				break;

			case 'f':
				filename = optarg;
				fflag = 1;
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if(fflag == 1)
	{
		if((fp = fopen(filename, "r")) == NULL)
		{
			char buffer[80];
			strcpy(buffer,"ERROR opening file ");
			strcat(buffer,filename);
			perror(buffer);
			exit(1);
		}
	}

	while((c = getc(fp)) != EOF)
	{
		putchar(c);
		for(i = delay; i > 0; i--)
			;
	}
}


usage()
{
	fprintf(stderr,"\nplayvt - play a VT animation with programmable delay\n");
	fprintf(stderr,"usage: playvt -f [filename] -d [delay]\n");
	fprintf(stderr,"       -f <filename>   file containing the animation\n");
	fprintf(stderr,"       -d <delay>      delay between characters\n");
	exit(1);
}

