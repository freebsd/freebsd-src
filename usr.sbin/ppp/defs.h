/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $FreeBSD$
 *
 *	TODO:
 */

/* Check the following definitions for your machine environment */
#ifdef __FreeBSD__
# define  MODEM_LIST	"/dev/cuaa1\0/dev/cuaa0"	/* name of tty device */
#else
# ifdef __OpenBSD__
#  define MODEM_LIST	"/dev/cua01\0/dev/cua00"	/* name of tty device */
# else
#  define MODEM_LIST	"/dev/tty01\0/dev/tty00"	/* name of tty device */
# endif
#endif
#define NMODEMS		2

#define _PATH_PPP	"/etc/ppp"

#define TUN_NAME	"tun"
#define TUN_PREFIX	(_PATH_DEV TUN_NAME)	/* /dev/tun */

#define MODEM_SPEED	B38400	/* tty speed */
#define	SERVER_PORT	3000	/* Base server port no. */
#define	MODEM_CTSRTS	1	/* Default (true): use CTS/RTS signals */
#define	RECONNECT_TIMEOUT 3	/* Default timer for carrier loss */
#define	DIAL_TIMEOUT	30	/* Default and Max random time to redial */
#define	DIAL_NEXT_TIMEOUT 3	/* Default Hold time to next number redial */
#define SCRIPT_LEN 512		/* Size of login/dial/hangup scripts */
#define LINE_LEN SCRIPT_LEN 	/* Size of lines */
#define DEVICE_LEN SCRIPT_LEN	/* Size of individual devices */
#define AUTHLEN 100 		/* Size of authname/authkey */
#define CHAPDIGESTLEN 100	/* Maximum chap digest */
#define CHAPCHALLENGELEN 48	/* Maximum chap challenge */
#define MAXARGS 40		/* How many args per config line */
#define NCP_IDLE_TIMEOUT 180	/* Drop all links */
#define CHOKED_TIMEOUT 120	/* Delete queued packets w/ blocked tun */

#define MIN_LQRPERIOD 2		/* Minimum LQR frequency */
#define DEF_LQRPERIOD 30	/* Default LQR frequency */
#define MIN_FSMRETRY 3		/* Minimum FSM retry frequency */
#define DEF_FSMRETRY 3		/* FSM retry frequency */
#define DEF_FSMTRIES 5		/* Default max retries */
#define DEF_FSMAUTHTRIES 3	/* Default max auth retries */
#define DEF_IFQUEUE 30		/* Default interface queue size */

#define	CONFFILE 	"ppp.conf"
#define	LINKUPFILE 	"ppp.linkup"
#define	LINKDOWNFILE 	"ppp.linkdown"
#define	SECRETFILE	"ppp.secret"

#define	EX_SIG		-1
#define	EX_NORMAL	0
#define	EX_START	1
#define	EX_SOCK		2
#define	EX_MODEM	3
#define	EX_DIAL		4
#define	EX_DEAD		5
#define	EX_DONE		6
#define	EX_REBOOT	7
#define	EX_ERRDEAD	8
#define	EX_HANGUP	9
#define	EX_TERM		10
#define EX_NODIAL	11
#define EX_NOLOGIN	12
/* return values for -background mode, not really exits */
#define EX_REDIAL	13
#define EX_RECONNECT	14

/* physical::type values (OR'd in bundle::phys_type) */
#define PHYS_NONE		0
#define PHYS_INTERACTIVE	1  /* Manual link */
#define PHYS_AUTO		2  /* Dial-on-demand link */
#define	PHYS_DIRECT		4  /* Incoming link, deleted when closed */
#define	PHYS_DEDICATED		8  /* Dedicated link */
#define	PHYS_DDIAL		16 /* Dial immediately, stay connected */
#define PHYS_BACKGROUND		32 /* Dial immediately, deleted when closed */
#define PHYS_FOREGROUND		64 /* Pseudo mode, same as background */
#define PHYS_ALL		127

/* flags passed to findblank() and MakeArgs() */
#define PARSE_NORMAL	0
#define PARSE_REDUCE	1
#define PARSE_NOHASH	2

#define ROUNDUP(x) ((x) ? (1 + (((x) - 1) | (sizeof(long) - 1))) : sizeof(long))

extern void randinit(void);
extern ssize_t fullread(int, void *, size_t);
extern const char *mode2Nam(int);
extern int Nam2mode(const char *);
extern struct in_addr GetIpAddr(const char *);
extern int SpeedToInt(speed_t);
extern speed_t IntToSpeed(int);
extern char *findblank(char *, int);
extern int MakeArgs(char *, char **, int, int);
extern const char *NumStr(long, char *, size_t);
extern const char *HexStr(long, char *, size_t);
extern const char *ex_desc(int);
extern void SetTitle(const char *);
