/*
 * Copyright (c) 1995, 1996, 1997, 1999 Kungliga Tekniska Högskolan
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

#include "login_locl.h"

RCSID("$Id: utmp_login.c,v 1.17 1999/12/02 17:04:56 joda Exp $");

void
prepare_utmp (struct utmp *utmp, char *tty, 
	      const char *username, const char *hostname)
{
    char *ttyx = clean_ttyname (tty);

    memset(utmp, 0, sizeof(*utmp));
    utmp->ut_time = time(NULL);
    strncpy(utmp->ut_line, ttyx, sizeof(utmp->ut_line));
    strncpy(utmp->ut_name, username, sizeof(utmp->ut_name));

# ifdef HAVE_STRUCT_UTMP_UT_USER
    strncpy(utmp->ut_user, username, sizeof(utmp->ut_user));
# endif

# ifdef HAVE_STRUCT_UTMP_UT_ADDR
    if (hostname[0]) {
        struct hostent *he;
	if ((he = gethostbyname(hostname)))
	    memcpy(&utmp->ut_addr, he->h_addr_list[0],
		   sizeof(utmp->ut_addr));
    }
# endif

# ifdef HAVE_STRUCT_UTMP_UT_HOST
    strncpy(utmp->ut_host, hostname, sizeof(utmp->ut_host));
# endif

# ifdef HAVE_STRUCT_UTMP_UT_TYPE
    utmp->ut_type = USER_PROCESS;
# endif

# ifdef HAVE_STRUCT_UTMP_UT_PID
    utmp->ut_pid = getpid();
# endif

# ifdef HAVE_STRUCT_UTMP_UT_ID
    strncpy(utmp->ut_id, make_id(ttyx), sizeof(utmp->ut_id));
# endif
}

#ifdef HAVE_UTMPX_H
void utmp_login(char *tty, const char *username, const char *hostname)
{ 
    return;
}
#else

/* update utmp and wtmp - the BSD way */

void utmp_login(char *tty, const char *username, const char *hostname)
{
    struct utmp utmp;
    int fd;

    prepare_utmp (&utmp, tty, username, hostname);

#ifdef HAVE_SETUTENT
    utmpname(_PATH_UTMP);
    setutent();
    pututline(&utmp);
    endutent();
#else

#ifdef HAVE_TTYSLOT
    {
      int ttyno;
      ttyno = ttyslot();
      if (ttyno > 0 && (fd = open(_PATH_UTMP, O_WRONLY, 0)) >= 0) {
	lseek(fd, (long)(ttyno * sizeof(struct utmp)), SEEK_SET);
	write(fd, &utmp, sizeof(struct utmp));
	close(fd);
      }
    }
#endif /* HAVE_TTYSLOT */
#endif /* HAVE_SETUTENT */

    if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) >= 0) {
	write(fd, &utmp, sizeof(struct utmp));
	close(fd);
    }
}
#endif /* !HAVE_UTMPX_H */
