/*	$OpenBSD: misc.h,v 1.21 2005/03/01 10:09:52 djm Exp $	*/

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

/* misc.c */

char	*chop(char *);
char	*strdelim(char **);
int	 set_nonblock(int);
int	 unset_nonblock(int);
void	 set_nodelay(int);
int	 a2port(const char *);
char	*hpdelim(char **);
char	*cleanhostname(char *);
char	*colon(char *);
long	 convtime(const char *);

struct passwd *pwcopy(struct passwd *);

typedef struct arglist arglist;
struct arglist {
	char    **list;
	u_int   num;
	u_int   nalloc;
};
void	 addargs(arglist *, char *, ...) __attribute__((format(printf, 2, 3)));

/* tildexpand.c */

char	*tilde_expand_filename(const char *, uid_t);

/* readpass.c */

#define RP_ECHO			0x0001
#define RP_ALLOW_STDIN		0x0002
#define RP_ALLOW_EOF		0x0004
#define RP_USE_ASKPASS		0x0008

char	*read_passphrase(const char *, int);
int	 ask_permission(const char *, ...) __attribute__((format(printf, 1, 2)));
int	 read_keyfile_line(FILE *, const char *, char *, size_t, u_long *);
