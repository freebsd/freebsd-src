/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: logwtmp.c,v 1.14 1999/12/02 16:58:31 joda Exp $");
#endif

#include <stdio.h>
#include <string.h>
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#else
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UTMP_H
#include <utmp.h>
#endif
#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif
#include "extern.h"

#ifndef WTMP_FILE
#ifdef _PATH_WTMP
#define WTMP_FILE _PATH_WTMP
#else
#define WTMP_FILE "/var/adm/wtmp"
#endif
#endif

void
ftpd_logwtmp(char *line, char *name, char *host)
{
    static int init = 0;
    static int fd;
#ifdef WTMPX_FILE
    static int fdx;
#endif
    struct utmp ut;
#ifdef WTMPX_FILE
    struct utmpx utx;
#endif

    memset(&ut, 0, sizeof(struct utmp));
#ifdef HAVE_STRUCT_UTMP_UT_TYPE
    if(name[0])
	ut.ut_type = USER_PROCESS;
    else
	ut.ut_type = DEAD_PROCESS;
#endif
    strncpy(ut.ut_line, line, sizeof(ut.ut_line));
    strncpy(ut.ut_name, name, sizeof(ut.ut_name));
#ifdef HAVE_STRUCT_UTMP_UT_PID
    ut.ut_pid = getpid();
#endif
#ifdef HAVE_STRUCT_UTMP_UT_HOST
    strncpy(ut.ut_host, host, sizeof(ut.ut_host));
#endif
    ut.ut_time = time(NULL);

#ifdef WTMPX_FILE
    strncpy(utx.ut_line, line, sizeof(utx.ut_line));
    strncpy(utx.ut_user, name, sizeof(utx.ut_user));
    strncpy(utx.ut_host, host, sizeof(utx.ut_host));
#ifdef HAVE_STRUCT_UTMPX_UT_SYSLEN
    utx.ut_syslen = strlen(host) + 1;
    if (utx.ut_syslen > sizeof(utx.ut_host))
        utx.ut_syslen = sizeof(utx.ut_host);
#endif
    {
	struct timeval tv;

	gettimeofday (&tv, 0);
	utx.ut_tv.tv_sec = tv.tv_sec;
	utx.ut_tv.tv_usec = tv.tv_usec;
    }

    if(name[0])
	utx.ut_type = USER_PROCESS;
    else
	utx.ut_type = DEAD_PROCESS;
#endif

    if(!init){
	fd = open(WTMP_FILE, O_WRONLY|O_APPEND, 0);
#ifdef WTMPX_FILE
	fdx = open(WTMPX_FILE, O_WRONLY|O_APPEND, 0);
#endif
	init = 1;
    }
    if(fd >= 0) {
	write(fd, &ut, sizeof(struct utmp)); /* XXX */
#ifdef WTMPX_FILE
	write(fdx, &utx, sizeof(struct utmpx));
#endif	
    }
}
