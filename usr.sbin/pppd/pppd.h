/*
 * pppd.h - PPP daemon global declarations.
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: pppd.h,v 1.8 1995/04/26 06:46:31 paulus Exp $
 */

/*
 * TODO:
 */

#ifndef __PPPD_H__
#define __PPPD_H__

#include <sys/param.h>		/* for MAXPATHLEN and BSD4_4, if defined */
#include <sys/types.h>		/* for u_int32_t, if defined */
#include <net/ppp_defs.h>

#define NUM_PPP	1		/* One PPP interface supported (per process) */

/*
 * Limits.
 */

#define MAXWORDLEN	1024	/* max length of word in file (incl null) */
#define MAXARGS		1	/* max # args to a command */
#define MAXNAMELEN	256	/* max length of hostname or name for auth */
#define MAXSECRETLEN	256	/* max length of password or secret */

/*
 * Global variables.
 */

extern int	hungup;		/* Physical layer has disconnected */
extern int	ifunit;		/* Interface unit number */
extern char	ifname[];	/* Interface name */
extern int	fd;		/* Serial device file descriptor */
extern char	hostname[];	/* Our hostname */
extern u_char	outpacket_buf[]; /* Buffer for outgoing packets */
extern int	phase;		/* Current state of link - see values below */
extern int	baud_rate;	/* Current link speed in bits/sec */
extern char	*progname;	/* Name of this program */

/*
 * Variables set by command-line options.
 */

extern int	debug;		/* Debug flag */
extern int	kdebugflag;	/* Tell kernel to print debug messages */
extern int	default_device;	/* Using /dev/tty or equivalent */
extern char	devnam[];	/* Device name */
extern int	crtscts;	/* Use hardware flow control */
extern int	modem;		/* Use modem control lines */
extern int	inspeed;	/* Input/Output speed requested */
extern u_int32_t netmask;	/* IP netmask to set on interface */
extern int	lockflag;	/* Create lock file to lock the serial dev */
extern int	nodetach;	/* Don't detach from controlling tty */
extern char	*connector;	/* Script to establish physical link */
extern char	*disconnector;	/* Script to disestablish physical link */
extern char	user[];		/* Username for PAP */
extern char	passwd[];	/* Password for PAP */
extern int	auth_required;	/* Peer is required to authenticate */
extern int	proxyarp;	/* Set up proxy ARP entry for peer */
extern int	persist;	/* Reopen link after it goes down */
extern int	uselogin;	/* Use /etc/passwd for checking PAP */
extern int	lcp_echo_interval; /* Interval between LCP echo-requests */
extern int	lcp_echo_fails;	/* Tolerance to unanswered echo-requests */
extern char	our_name[];	/* Our name for authentication purposes */
extern char	remote_name[];	/* Peer's name for authentication */
extern int	usehostname;	/* Use hostname for our_name */
extern int	disable_defaultip; /* Don't use hostname for default IP adrs */
extern char	*ipparam;	/* Extra parameter for ip up/down scripts */
extern int	cryptpap;	/* Others' PAP passwords are encrypted */

/*
 * Values for phase.
 */
#define PHASE_DEAD		0
#define PHASE_ESTABLISH		1
#define PHASE_AUTHENTICATE	2
#define PHASE_NETWORK		3
#define PHASE_TERMINATE		4

/*
 * Prototypes.
 */
void quit __P((void));	/* Cleanup and exit */
void timeout __P((void (*)(), caddr_t, int));
				/* Look-alike of kernel's timeout() */
void untimeout __P((void (*)(), caddr_t));
				/* Look-alike of kernel's untimeout() */
void output __P((int, u_char *, int));
				/* Output a PPP packet */
void demuxprotrej __P((int, int));
				/* Demultiplex a Protocol-Reject */
int  check_passwd __P((int, char *, int, char *, int, char **, int *));
				/* Check peer-supplied username/password */
int  get_secret __P((int, char *, char *, char *, int *, int));
				/* get "secret" for chap */
u_int32_t GetMask __P((u_int32_t)); /* get netmask for address */
void die __P((int));

/*
 * Inline versions of get/put char/short/long.
 * Pointer is advanced; we assume that both arguments
 * are lvalues and will already be in registers.
 * cp MUST be u_char *.
 */
#define GETCHAR(c, cp) { \
	(c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
	*(cp)++ = (u_char) (c); \
}


#define GETSHORT(s, cp) { \
	(s) = *(cp)++ << 8; \
	(s) |= *(cp)++; \
}
#define PUTSHORT(s, cp) { \
	*(cp)++ = (u_char) ((s) >> 8); \
	*(cp)++ = (u_char) (s); \
}

#define GETLONG(l, cp) { \
	(l) = *(cp)++ << 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; \
}
#define PUTLONG(l, cp) { \
	*(cp)++ = (u_char) ((l) >> 24); \
	*(cp)++ = (u_char) ((l) >> 16); \
	*(cp)++ = (u_char) ((l) >> 8); \
	*(cp)++ = (u_char) (l); \
}

#define INCPTR(n, cp)	((cp) += (n))
#define DECPTR(n, cp)	((cp) -= (n))

#undef  FALSE
#define FALSE	0
#undef  TRUE
#define TRUE	1

/*
 * System dependent definitions for user-level 4.3BSD UNIX implementation.
 */

#define DEMUXPROTREJ(u, p)	demuxprotrej(u, p)

#define TIMEOUT(r, f, t)	timeout((r), (f), (t))
#define UNTIMEOUT(r, f)		untimeout((r), (f))

#define BCOPY(s, d, l)		memcpy(d, s, l)
#define BZERO(s, n)		memset(s, 0, n)
#define EXIT(u)			quit()

#define PRINTMSG(m, l)	{ m[l] = '\0'; syslog(LOG_INFO, "Remote message: %s", m); }

/*
 * MAKEHEADER - Add Header fields to a packet.
 */
#define MAKEHEADER(p, t) { \
    PUTCHAR(PPP_ALLSTATIONS, p); \
    PUTCHAR(PPP_UI, p); \
    PUTSHORT(t, p); }


#ifdef DEBUGALL
#define DEBUGMAIN	1
#define DEBUGFSM	1
#define DEBUGLCP	1
#define DEBUGIPCP	1
#define DEBUGUPAP	1
#define DEBUGCHAP	1
#endif

#ifndef LOG_PPP			/* we use LOG_LOCAL2 for syslog by default */
#if defined(DEBUGMAIN) || defined(DEBUGFSM) || defined(DEBUG) \
  || defined(DEBUGLCP) || defined(DEBUGIPCP) || defined(DEBUGUPAP) \
  || defined(DEBUGCHAP) 
#define LOG_PPP LOG_LOCAL2
#else
#define LOG_PPP LOG_DAEMON
#endif
#endif /* LOG_PPP */

#ifdef DEBUGMAIN
#define MAINDEBUG(x)	if (debug) syslog x
#else
#define MAINDEBUG(x)
#endif

#ifdef DEBUGFSM
#define FSMDEBUG(x)	if (debug) syslog x
#else
#define FSMDEBUG(x)
#endif

#ifdef DEBUGLCP
#define LCPDEBUG(x)	if (debug) syslog x
#else
#define LCPDEBUG(x)
#endif

#ifdef DEBUGIPCP
#define IPCPDEBUG(x)	if (debug) syslog x
#else
#define IPCPDEBUG(x)
#endif

#ifdef DEBUGUPAP
#define UPAPDEBUG(x)	if (debug) syslog x
#else
#define UPAPDEBUG(x)
#endif

#ifdef DEBUGCHAP
#define CHAPDEBUG(x)	if (debug) syslog x
#else
#define CHAPDEBUG(x)
#endif

#ifndef SIGTYPE
#if defined(sun) || defined(SYSV) || defined(POSIX_SOURCE)
#define SIGTYPE void
#else
#define SIGTYPE int
#endif /* defined(sun) || defined(SYSV) || defined(POSIX_SOURCE) */
#endif /* SIGTYPE */

#ifndef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#endif
#ifndef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))
#endif

#endif /* __PPP_H__ */
