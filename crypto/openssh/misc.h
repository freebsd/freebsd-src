/*	$OpenBSD: misc.h,v 1.11 2002/01/24 21:09:25 stevesk Exp $	*/

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

char	*chop(char *);
char	*strdelim(char **);
void	 set_nonblock(int);
void	 unset_nonblock(int);
void	 set_nodelay(int);
int	 a2port(const char *);
char	*cleanhostname(char *);
char	*colon(char *);
long	 convtime(const char *);

struct passwd *pwcopy(struct passwd *);

typedef struct arglist arglist;
struct arglist {
        char    **list;
        int     num;
        int     nalloc;
};
void	 addargs(arglist *, char *, ...) __attribute__((format(printf, 2, 3)));
