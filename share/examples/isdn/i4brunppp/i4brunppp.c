/*
 * Copyright (c) 1999, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	i4brunppp - run userland ppp for incoming call from rbch i/f
 *	------------------------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sat Jul 21 13:38:10 2001]
 *
 *---------------------------------------------------------------------------
 *
 * BEWARE: HIGHLY EXPERIMENTAL!
 * ---------------------------
 * 
 * This program is used in conjunction with a isdnd.rc entry similar to
 *
 *  regexpr = "ULPPP.*call active"   # look for matches in log messages
 *  regprog = i4brunppp              # execute program when match is found
 *
 * this one. It _must_ be put into /etc/isdn!
 * When an active call is detected, isdnd fires off i4brunppp, which attaches
 * the rbch device used to stdin/stdout and then runs ppp which is given the
 * "-direct" command and the string "inc_rbchX" (where X is the i4brbch unit
 * number) as arguments.
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <machine/i4b_ioctl.h>
#include <machine/i4b_rbch_ioctl.h>

#define I4BDEVICE	"/dev/i4b"	

#define PPPPROG		"/usr/sbin/ppp"
#define PPPNAME		"ppp"
#define PPPARG1		"-direct"
#define PPPLABEL	"inc_"

#define VERIFYSTRING	"call active"
#define DEVSTRING	"rbch"

#define PPPDEBUG

/*---------------------------------------------------------------------------*
 *	program entry
 *---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	char buffer[256];
	int rbch_fd;
	char *p = "DeadPointer";
	int found;
	int i;
	
#ifdef PPPDEBUG
	FILE *dfp;
	time_t tim;
        register struct tm *tp;
#endif
	
	/* open syslog */
	
	(void)openlog("i4brunppp", LOG_PID|LOG_CONS|LOG_NDELAY, LOG_USER);

#ifdef PPPDEBUG

	/* open debug log */
	
	if((dfp = fopen("/tmp/i4brunppp-debug.log", "a")) == NULL)
	{
		syslog(LOG_INFO, "cannot open logfile: %s", strerror(errno));
		exit(1);
	}

	tim = time(NULL);
	tp = localtime(&tim);
        strftime(buffer, 40, I4B_TIME_FORMAT, tp);
	fprintf(dfp, "\n=================== %s ===================\n", buffer);

	for(i=0; i < argc; i++)
		fprintf(dfp, "\t%s\n", argv[i]);
#endif

	/* check if this is the right message */
	
	found = 0;
	
	for(i=0; i < argc; i++)
	{
		if((strstr(argv[i], VERIFYSTRING)) != NULL)
		{
			found = 1;
			break;
		}
	}

	if(found == 0)
	{
#ifdef PPPDEBUG
		fprintf(dfp, "did not found [%s], exit\n", VERIFYSTRING);
#endif
		exit(0);
	}
		
	found = 0;

	/* check if we got a good device name */
	
	for(; i < argc; i++)
	{
		if((p = strstr(argv[i], DEVSTRING)) != NULL)
		{
			found = 1;
			break;
		}
	}

	if(found == 0)
	{
#ifdef PPPDEBUG
		fprintf(dfp, "did not found [%s], exit\n", DEVSTRING);
#endif
		exit(0);
	}

	/* everything ok, now prepare for running ppp */	

	/* close all file descriptors */
	
	i = getdtablesize();

	for(;i >= 0; i--)
           if (i != 2)
		close(i);

	/* fiddle a terminating zero after the rbch unit number */
	
	p += strlen(DEVSTRING);

	if(isdigit(*p) && isdigit(*(p+1)))
		*(p+2) = '\0';
	else
		*(p+1) = '\0';

	/* construct /dev/i4brbchX device name */
	
	sprintf(buffer, "%s%s%s", I4BDEVICE, DEVSTRING, p);

	/* open the rbch device as fd 0 = stdin */
	
	rbch_fd = open(buffer, O_RDWR);

	if(rbch_fd != 0)
	{
		if(rbch_fd < 0)		
			syslog(LOG_INFO, "cannot open %s: %s", buffer, strerror(errno));
		else
			syslog(LOG_INFO, "cannot open %s as fd 0 (is %d): %s", buffer, rbch_fd, strerror(errno));
		exit(1);
	}

	/* dup rbch device fd as fd 1 = stdout */
	
	if((i = dup(rbch_fd)) != 1)
	{
		if(i < 0)		
			syslog(LOG_INFO, "cannot dup rbch_fd: %s", strerror(errno));
		else
			syslog(LOG_INFO, "cannot dup rbch as fd 1 (is %d): %s", i, strerror(errno));
		exit(1);
	}

	/* construct the label for ppp's ppp.conf file */
	
	sprintf(buffer, "%s%s%s", PPPLABEL, DEVSTRING, p);

	syslog(LOG_INFO, "executing: %s %s %s %s", PPPPROG, PPPNAME, PPPARG1, buffer);

	/* execute ppp */
	
	if((execl(PPPPROG, PPPNAME, PPPARG1, buffer, NULL)) == -1)
	{
		syslog(LOG_INFO, "cannot exec: %s", strerror(errno));
		exit(1);
	}
	syslog(LOG_INFO, "finished: %s %s %s %s", PPPPROG, PPPNAME, PPPARG1, buffer);
	return(0);
}

/* EOF */
