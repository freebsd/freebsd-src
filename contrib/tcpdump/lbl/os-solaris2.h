/*
 * Copyright (c) 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header: os-solaris2.h,v 1.16 96/07/05 22:11:23 leres Exp $ (LBL)
 */

/* Signal routines are this type */
#define SIGRET void
/* Signal routines use "return SIGRETVAL;" */
#define SIGRETVAL
/* The wait() status variable is this type */
#define WAITSTATUS int

#define major(x)	((int)(((unsigned)(x)>>8)&0377))
#define minor(x)	((int)((x)&0377))

/* Prototypes missing in SunOS 5 */
int	daemon(int, int);
int	dn_expand(u_char *, u_char *, u_char *, u_char *, int);
int	dn_skipname(u_char *, u_char *);
int	getdtablesize(void);
int	gethostname(char *, int);
char	*getusershell(void);
char	*getwd(char *);
int	iruserok(u_int, int, char *, char *);
#ifdef __STDC__
struct	utmp;
void	login(struct utmp *);
#endif
int	logout(const char *);
int	res_query(char *, int, int, u_char *, int);
int	setenv(const char *, const char *, int);
#if defined(_STDIO_H) && defined(HAVE_SETLINEBUF)
int	setlinebuf(FILE *);
#endif
int	sigblock(int);
int	sigsetmask(int);
char    *strerror(int);
int	snprintf(char *, size_t, const char *, ...);
int	strcasecmp(const char *, const char *);
void	unsetenv(const char *);
#ifdef __STDC__
struct	timeval;
#endif
int	utimes(const char *, struct timeval *);

/* Solaris signal compat */
#ifndef sigmask
#define sigmask(m)	(1 << ((m)-1))
#endif
#ifndef signal
#define signal(s, f)	sigset(s, f)
#endif

/* Solaris random compat */
#ifndef srandom
#define srandom(seed) srand48((long)seed)
#endif
#ifndef random
#define random() lrand48()
#endif

#ifndef CBREAK
#define CBREAK	O_CBREAK
#define CRMOD	O_CRMOD
#define RAW	O_RAW
#define TBDELAY	O_TBDELAY
#endif

#ifndef TIOCPKT_DATA
#define		TIOCPKT_DATA		0x00	/* data packet */
#define		TIOCPKT_FLUSHREAD	0x01	/* flush packet */
#define		TIOCPKT_FLUSHWRITE	0x02	/* flush packet */
#define		TIOCPKT_STOP		0x04	/* stop output */
#define		TIOCPKT_START		0x08	/* start output */
#define		TIOCPKT_NOSTOP		0x10	/* no more ^S, ^Q */
#define		TIOCPKT_DOSTOP		0x20	/* now do ^S ^Q */
#define		TIOCPKT_IOCTL		0x40	/* state change of pty driver */
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#define STDOUT_FILENO 1
#define STDIN_FILENO 0
#endif

#ifndef FD_SET
#define FD_SET(n, p)	((p)->fds_bits[0] |= (1<<(n)))
#define FD_CLR(n, p)	((p)->fds_bits[0] &= ~(1<<(n)))
#define FD_ISSET(n, p)	((p)->fds_bits[0] & (1<<(n)))
#define FD_ZERO(p)	((p)->fds_bits[0] = 0)
#endif

#ifndef S_ISTXT
#define S_ISTXT S_ISVTX
#endif
