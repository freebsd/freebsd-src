/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file performs some of the things login(1) normally does.  We cannot
 * easily use something like login -p -h host -f user, because there are
 * several different logins around, and it is hard to determined what kind of
 * login the current system has.  Also, we want to be able to execute commands
 * on a tty.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * Copyright (c) 1999 Theo de Raadt.  All rights reserved.
 * Copyright (c) 1999 Markus Friedl.  All rights reserved.
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
 */

#include "includes.h"
RCSID("$OpenBSD: sshlogin.c,v 1.4 2002/06/23 03:30:17 deraadt Exp $");
RCSID("$FreeBSD$");

#include "loginrec.h"

/*
 * Returns the time when the user last logged in.  Returns 0 if the
 * information is not available.  This must be called before record_login.
 * The host the user logged in from will be returned in buf.
 */
u_long
get_last_login_time(uid_t uid, const char *logname,
    char *buf, u_int bufsize)
{
  struct logininfo li;

  login_get_lastlog(&li, uid);
  strlcpy(buf, li.hostname, bufsize);
  return li.tv_sec;
}

/*
 * Records that the user has logged in.  I these parts of operating systems
 * were more standardized.
 */
void
record_login(pid_t pid, const char *ttyname, const char *user, uid_t uid,
    const char *host, struct sockaddr * addr)
{
  struct logininfo *li;

  li = login_alloc_entry(pid, user, host, ttyname);
  login_set_addr(li, addr, sizeof(struct sockaddr));
  login_login(li);
  login_free_entry(li);
}

#ifdef LOGIN_NEEDS_UTMPX
void
record_utmp_only(pid_t pid, const char *ttyname, const char *user,
		 const char *host, struct sockaddr * addr)
{
  struct logininfo *li;

  li = login_alloc_entry(pid, user, host, ttyname);
  login_set_addr(li, addr, sizeof(struct sockaddr));
  login_utmp_only(li);
  login_free_entry(li);
}
#endif

/* Records that the user has logged out. */
void
record_logout(pid_t pid, const char *ttyname, const char *user)
{
  struct logininfo *li;

  li = login_alloc_entry(pid, user, NULL, ttyname);
  login_logout(li);
  login_free_entry(li);
}
