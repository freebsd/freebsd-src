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
 *	i4b daemon - logging routines
 *	-----------------------------
 *
 *	$Id: log.c,v 1.25 2000/10/09 12:53:29 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:47:28 1999]
 *
 *---------------------------------------------------------------------------*/

#include "isdnd.h"

#define LOGBUFLEN 256

extern int do_monitor;
extern int accepted;
extern FILE *logfp;

static void check_reg(char *logstring);

struct logtab {
	char *text;
	int pri;
};

/*---------------------------------------------------------------------------*
 * 	table for converting internal log levels into syslog levels
 *---------------------------------------------------------------------------*/
static struct logtab logtab[] = {
	{"ERR", LOG_ERR},	/* error conditions			*/
	{"WRN", LOG_WARNING},   /* warning conditions, nonfatal		*/
	{"DMN", LOG_NOTICE},	/* significant conditions of the daemon	*/
	{"CHD", LOG_INFO},	/* informational, call handling		*/
	{"DBG", LOG_DEBUG},	/* debug messages 			*/
	{"MER", LOG_ERR},	/* monitor error conditions		*/	
	{"PKT", LOG_INFO}	/* packet logging 			*/
};

/*---------------------------------------------------------------------------*
 *	initialize logging
 *---------------------------------------------------------------------------*/
void
init_log(void)
{
	int i;

	if(uselogfile)
	{
		if((logfp = fopen(logfile, "a")) == NULL)
		{
			fprintf(stderr, "ERROR, cannot open logfile %s: %s\n",
				logfile, strerror(errno));
			exit(1);
		}
	
		/* set unbuffered operation */
	
		setvbuf(logfp, (char *)NULL, _IONBF, 0);
	}
	else
	{
#if DEBUG
		if(do_debug && do_fork == 0 && do_fullscreen == 0)
			(void)openlog("isdnd",
				LOG_PID|LOG_CONS|LOG_NDELAY|LOG_PERROR,
				logfacility);
		else
#endif
		(void)openlog("isdnd", LOG_PID|LOG_CONS|LOG_NDELAY,
				logfacility);
	}

	/* initialize the regexp array */

	for(i = 0; i < MAX_RE; i++)
	{
		char *p;
		char buf[64];

		snprintf(buf, sizeof(buf), "%s%d", REGPROG_DEF, i);

		rarr[i].re_flg = 0;

		if((p = malloc(strlen(buf) + 1)) == NULL)
		{
			log(LL_DBG, "init_log: malloc failed: %s", strerror(errno));
			do_exit(1);
		}

		strcpy(p, buf);

		rarr[i].re_prog = p;
	}
}

/*---------------------------------------------------------------------------*
 *	finish logging
 *---------------------------------------------------------------------------*/
void
finish_log(void)
{
	if(uselogfile)
	{
		fflush(logfp);
		fclose(logfp);
	}
	else
	{
		(void)closelog();
	}
}

/*---------------------------------------------------------------------------*
 *	place entry into logfile
 *---------------------------------------------------------------------------*/
void
log(int what, const char *fmt, ...)
{
	char buffer[LOGBUFLEN];
	register char *dp;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buffer, LOGBUFLEN-1, fmt, ap);
	va_end(ap);
	
	dp = getlogdatetime();	/* get time string ptr */
	
#ifdef USE_CURSES

	/* put log on screen ? */

	if((do_fullscreen && curses_ready) &&
	   ((!debug_noscreen) || (debug_noscreen && (what != LL_DBG))))
	{
		wprintw(lower_w, "%s %s %-.*s\n", dp, logtab[what].text,

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
		     COLS-((strlen(dp))+(strlen(logtab[what].text))+3), buffer);
#else
		     (int)(COLS-((strlen(dp))+(strlen(logtab[what].text))+2)), buffer);
#endif
		wrefresh(lower_w);
	}
#endif

#ifdef I4B_EXTERNAL_MONITOR
	if(what != LL_MER) /* don't send monitor errs, endless loop !!! */
		monitor_evnt_log(logtab[what].pri, logtab[what].text, buffer);
#endif

	if(uselogfile)
	{
		fprintf(logfp, "%s %s %s\n", dp, logtab[what].text, buffer);
	}
	else
	{
		register char *s = buffer;
		
		/* strip leading spaces from syslog output */
		
		while(*s && (*s == ' '))
			s++;
			
		syslog(logtab[what].pri, "%s %s", logtab[what].text, s);
	}


#if DEBUG
	if(what != LL_DBG) /* don't check debug logs, endless loop !!! */
#endif
		check_reg(buffer);
}

/*---------------------------------------------------------------------------*
 *	return ptr to static area containing date/time
 *---------------------------------------------------------------------------*/
char *
getlogdatetime()
{
	static char logdatetime[41];
	time_t tim;
	register struct tm *tp;
	
	tim = time(NULL);
	tp = localtime(&tim);
	strftime(logdatetime,40,I4B_TIME_FORMAT,tp);
	return(logdatetime);
}

/*---------------------------------------------------------------------------*
 *	check for a match in the regexp array
 *---------------------------------------------------------------------------*/
static void
check_reg(char *logstring)
{
	register int i;

	for(i = 0; i < MAX_RE; i++)
	{
		if(rarr[i].re_flg && (!regexec(&(rarr[i].re), logstring, (size_t) 0, NULL, 0)))
		{
			char* argv[3];
			argv[0] = rarr[i].re_prog;
			argv[1] = logstring;
			argv[2] = NULL;

			exec_prog(rarr[i].re_prog, argv);
			break;
		}
	}
}

/* EOF */
