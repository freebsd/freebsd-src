/*	$OpenBSD: sshlogin.h,v 1.3 2001/06/26 17:27:25 markus Exp $	*/

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */
#ifndef SSHLOGIN_H
#define SSHLOGIN_H

void
record_login(pid_t, const char *, const char *, uid_t,
    const char *, struct sockaddr *);
void   record_logout(pid_t, const char *, const char *);
u_long         get_last_login_time(uid_t, const char *, char *, u_int);

#ifdef LOGIN_NEEDS_UTMPX
void	record_utmp_only(pid_t, const char *, const char *, const char *,
		struct sockaddr *);
#endif

#endif
