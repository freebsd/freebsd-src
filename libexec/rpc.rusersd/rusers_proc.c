/*-
 * Copyright (c) 1993, John Brezak
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

#ifndef lint
static const char rcsid[] =
	"$Id: rusers_proc.c,v 1.7 1997/11/26 07:36:50 charnier Exp $";
#endif /* not lint */

#ifdef DEBUG
#include <errno.h>
#endif
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <syslog.h>
#include <utmp.h>
#ifdef XIDLE
#include <setjmp.h>
#include <X11/Xlib.h>
#include <X11/extensions/xidle.h>
#endif
#define utmp rutmp
#include <rpcsvc/rnusers.h>
#undef utmp

#define	IGNOREUSER	"sleeper"

#ifdef OSF
#define _PATH_UTMP UTMP_FILE
#endif

#ifndef _PATH_UTMP
#define _PATH_UTMP "/etc/utmp"
#endif

#ifndef _PATH_DEV
#define _PATH_DEV "/dev"
#endif

#ifndef UT_LINESIZE
#define UT_LINESIZE sizeof(((struct utmp *)0)->ut_line)
#endif
#ifndef UT_NAMESIZE
#define UT_NAMESIZE sizeof(((struct utmp *)0)->ut_name)
#endif
#ifndef UT_HOSTSIZE
#define UT_HOSTSIZE sizeof(((struct utmp *)0)->ut_host)
#endif

typedef char ut_line_t[UT_LINESIZE+1];
typedef char ut_name_t[UT_NAMESIZE+1];
typedef char ut_host_t[UT_HOSTSIZE+1];

utmpidle utmp_idle[MAXUSERS];
rutmp old_utmp[MAXUSERS];
ut_line_t line[MAXUSERS];
ut_name_t name[MAXUSERS];
ut_host_t host[MAXUSERS];

extern int from_inetd;

FILE *ufp;

#ifdef XIDLE
Display *dpy;

static jmp_buf openAbort;

static void
abortOpen ()
{
    longjmp (openAbort, 1);
}

XqueryIdle(char *display)
{
        int first_event, first_error;
        Time IdleTime;

        (void) signal (SIGALRM, abortOpen);
        (void) alarm ((unsigned) 10);
        if (!setjmp (openAbort)) {
                if (!(dpy= XOpenDisplay(display))) {
                        syslog(LOG_ERR, "Cannot open display %s", display);
                        return(-1);
                }
                if (XidleQueryExtension(dpy, &first_event, &first_error)) {
                        if (!XGetIdleTime(dpy, &IdleTime)) {
                                syslog(LOG_ERR, "%s: unable to get idle time", display);
                                return(-1);
                        }
                }
                else {
                        syslog(LOG_ERR, "%s: Xidle extension not loaded", display);
                        return(-1);
                }
                XCloseDisplay(dpy);
        }
        else {
                syslog(LOG_ERR, "%s: server grabbed for over 10 seconds", display);
                return(-1);
        }
        (void) signal (SIGALRM, SIG_DFL);
        (void) alarm ((unsigned) 0);

        IdleTime /= 1000;
        return((IdleTime + 30) / 60);
}
#endif

static u_int
getidle(char *tty, char *display)
{
        struct stat st;
        char devname[PATH_MAX];
        time_t now;
        u_long idle;

        /*
         * If this is an X terminal or console, then try the
         * XIdle extension
         */
#ifdef XIDLE
        if (display && *display && (idle = XqueryIdle(display)) >= 0)
                return(idle);
#endif
        idle = 0;
        if (*tty == 'X') {
                u_long kbd_idle, mouse_idle;
#if	!defined(__FreeBSD__)
                kbd_idle = getidle("kbd", NULL);
#else
                kbd_idle = getidle("vga", NULL);
#endif
                mouse_idle = getidle("mouse", NULL);
                idle = (kbd_idle < mouse_idle)?kbd_idle:mouse_idle;
        }
        else {
                sprintf(devname, "%s/%s", _PATH_DEV, tty);
                if (stat(devname, &st) < 0) {
#ifdef DEBUG
                        printf("%s: %s\n", devname, strerror(errno));
#endif
                        return(-1);
                }
                time(&now);
#ifdef DEBUG
                printf("%s: now=%d atime=%d\n", devname, now,
                       st.st_atime);
#endif
                idle = now - st.st_atime;
                idle = (idle + 30) / 60; /* secs->mins */
        }
        if (idle < 0) idle = 0;

        return(idle);
}

static utmpidlearr *
do_names_2(int all)
{
        static utmpidlearr ut;
	struct utmp usr;
        int nusers = 0;

        bzero((char *)&ut, sizeof(ut));
        ut.utmpidlearr_val = &utmp_idle[0];

	ufp = fopen(_PATH_UTMP, "r");
        if (!ufp) {
                syslog(LOG_ERR, "%m");
                return(&ut);
        }

        /* only entries with both name and line fields */
        while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1 &&
               nusers < MAXUSERS)
                if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
                            sizeof(usr.ut_name))
#ifdef OSF
                    && usr.ut_type == USER_PROCESS
#endif
                    ) {
                        utmp_idle[nusers].ui_utmp.ut_time =
                                usr.ut_time;
                        utmp_idle[nusers].ui_idle =
                                getidle(usr.ut_line, usr.ut_host);
                        utmp_idle[nusers].ui_utmp.ut_line = line[nusers];
                        strncpy(line[nusers], usr.ut_line, UT_LINESIZE);
                        utmp_idle[nusers].ui_utmp.ut_name = name[nusers];
                        strncpy(name[nusers], usr.ut_name, UT_NAMESIZE);
                        utmp_idle[nusers].ui_utmp.ut_host = host[nusers];
                        strncpy(host[nusers], usr.ut_host, UT_HOSTSIZE);

			/* Make sure entries are NUL terminated */
			line[nusers][UT_LINESIZE] =
			name[nusers][UT_NAMESIZE] =
			host[nusers][UT_HOSTSIZE] = '\0';
                        nusers++;
                }

        ut.utmpidlearr_len = nusers;
        fclose(ufp);
        return(&ut);
}

int *
rusers_num()
{
        static int num_users = 0;
	struct utmp usr;

        ufp = fopen(_PATH_UTMP, "r");
        if (!ufp) {
                syslog(LOG_ERR, "%m");
                return(NULL);
        }

        /* only entries with both name and line fields */
        while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
                if (*usr.ut_name && *usr.ut_line &&
		    strncmp(usr.ut_name, IGNOREUSER,
                            sizeof(usr.ut_name))
#ifdef OSF
                    && usr.ut_type == USER_PROCESS
#endif
                    ) {
                        num_users++;
                }

        fclose(ufp);
        return(&num_users);
}

static utmparr *
do_names_1(int all)
{
        utmpidlearr *utidle;
        static utmparr ut;
        int i;

        bzero((char *)&ut, sizeof(ut));

        utidle = do_names_2(all);
        if (utidle) {
                ut.utmparr_len = utidle->utmpidlearr_len;
                ut.utmparr_val = &old_utmp[0];
                for (i = 0; i < ut.utmparr_len; i++)
                        bcopy(&utmp_idle[i].ui_utmp, &old_utmp[i],
                              sizeof(old_utmp[0]));

        }

        return(&ut);
}

utmpidlearr *
rusersproc_names_2()
{
        return(do_names_2(0));
}

utmpidlearr *
rusersproc_allnames_2()
{
        return(do_names_2(1));
}

utmparr *
rusersproc_names_1()
{
        return(do_names_1(0));
}

utmparr *
rusersproc_allnames_1()
{
        return(do_names_1(1));
}

void
rusers_service(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		int fill;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		goto leave;

	case RUSERSPROC_NUM:
		xdr_argument = xdr_void;
		xdr_result = xdr_int;
                local = (char *(*)()) rusers_num;
		break;

	case RUSERSPROC_NAMES:
		xdr_argument = xdr_void;
		xdr_result = xdr_utmpidlearr;
                switch (rqstp->rq_vers) {
                case RUSERSVERS_ORIG:
                        local = (char *(*)()) rusersproc_names_1;
                        break;
                case RUSERSVERS_IDLE:
                        local = (char *(*)()) rusersproc_names_2;
                        break;
                default:
                        svcerr_progvers(transp, RUSERSVERS_ORIG, RUSERSVERS_IDLE);
                        goto leave;
                        /*NOTREACHED*/
                }
		break;

	case RUSERSPROC_ALLNAMES:
		xdr_argument = xdr_void;
		xdr_result = xdr_utmpidlearr;
                switch (rqstp->rq_vers) {
                case RUSERSVERS_ORIG:
                        local = (char *(*)()) rusersproc_allnames_1;
                        break;
                case RUSERSVERS_IDLE:
                        local = (char *(*)()) rusersproc_allnames_2;
                        break;
                default:
                        svcerr_progvers(transp, RUSERSVERS_ORIG, RUSERSVERS_IDLE);
                        goto leave;
                        /*NOTREACHED*/
                }
		break;

	default:
		svcerr_noproc(transp);
		goto leave;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		goto leave;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
leave:
        if (from_inetd)
                exit(0);
}
