/*
 * Copyright (c) 1994 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Copyright (c) 1994 Henrik Vestergaard Drabøl (hvd@terry.pping.dk) */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)rtprio.c	5.4 (Berkeley) 6/1/90";*/
static char rcsid[] = "$Id: rtprio.c,v 1.2 1993/11/23 00:02:23 jtc Exp $";
#endif /* not lint */

#include <sys/time.h>
#include <stdio.h>
#include <sys/rtprio.h>
#include <locale.h>
#include <errno.h>
#include <err.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

static void usage();

int
main(argc, argv)
	int argc;
	char **argv;
{
	int nrtprio = RTPRIO_RTOFF;
	int proc=0;

	setlocale(LC_ALL, "");
	errno = 0;

	switch (argc) {
        case 2:
	  proc = abs(atoi(argv[1]));
		/* FALLTHROUGH */

	case 1:
          nrtprio = rtprio(proc,RTPRIO_NOCHG);
          fprintf(stderr,"rtprio: %d %s\n",
            nrtprio,
            nrtprio==RTPRIO_RTOFF?"(RTOFF)":"");
          exit(nrtprio); 
		/* NOTREACHED */
          
        default: {
		switch (argv[1][0]) {
		case '-':
		  if (strcmp(argv[1],"-t")==0)
		       nrtprio = RTPRIO_RTOFF;
		     else 
		       usage();
  		  break;

		case '0':case '1':case '2':case '3':case '4':
		case '5':case '6':case '7':case '8':case '9':
		  nrtprio = atoi (argv[1]);

		  if (errno== ERANGE) usage();
		  break;

		default:
		  usage(); 
		  break;
		}
		switch (argv[2][0]) {
		case '-':
		  proc = -atoi(argv[2]);

  		break;
		}
      
	errno = 0;

        nrtprio = rtprio(proc, nrtprio);

	if (errno) {
		err (1, "rtprio");
		/* NOTREACHED */
	}

	if (proc == 0) {
          execvp(argv[2], &argv[2]);
   	  err ((errno == ENOENT) ? 127 : 126, "%s", argv[2]);}
	  /* NOTREACHED */
	      }
	}
return(nrtprio);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: rtprio\n");
	(void)fprintf(stderr, "usage: rtprio [-] pid\n");
	(void)fprintf(stderr, "usage: rtprio priority command [ args ] \n");
	(void)fprintf(stderr, "usage: rtprio priority -pid \n");
	(void)fprintf(stderr, "usage: rtprio -t command [ args ] \n");
	(void)fprintf(stderr, "usage: rtprio -t -pid \n");
	
	exit(-1);
}
