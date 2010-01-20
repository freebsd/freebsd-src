/* $OpenBSD: match.h,v 1.14 2008/06/10 03:57:27 djm Exp $ */

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
#ifndef MATCH_H
#define MATCH_H

int	 match_pattern(const char *, const char *);
int	 match_pattern_list(const char *, const char *, u_int, int);
int	 match_hostname(const char *, const char *, u_int);
int	 match_host_and_ip(const char *, const char *, const char *);
int	 match_user(const char *, const char *, const char *, const char *);
char	*match_list(const char *, const char *, u_int *);

/* addrmatch.c */
int	 addr_match_list(const char *, const char *);

#endif
