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
 * $Id: defs.h,v 1.18 1997/08/25 00:29:10 brian Exp $
 *
 *	TODO:
 */

#ifndef _DEFS_H_
#define	_DEFS_H_

#include <sys/types.h>
#include <machine/endian.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include "mbuf.h"
#include "log.h"

/*
 *  Check follwiing definitions for your machine envirinment
 */
#ifdef __FreeBSD__
#define	MODEM_DEV	"/dev/cuaa1"	/* name of tty device */
#define	BASE_MODEM_DEV	"cuaa1"	/* name of base tty device */
#else
#ifdef __OpenBSD__
#define	MODEM_DEV	"/dev/cua01"	/* name of tty device */
#define	BASE_MODEM_DEV	"cua01"	/* name of base tty device */
#else
#define	MODEM_DEV	"/dev/tty01"	/* name of tty device */
#define	BASE_MODEM_DEV	"tty01"	/* name of base tty device */
#endif
#endif
#define MODEM_SPEED	B38400	/* tty speed */
#define	SERVER_PORT	3000	/* Base server port no. */

#define	MODEM_CTSRTS	TRUE	/* Default (true): use CTS/RTS signals */
#define	RECONNECT_TIMER	3	/* Default timer for carrier loss */
#define	RECONNECT_TRIES	0	/* Default retries on carrier loss */
#define	REDIAL_PERIOD	30	/* Default Hold time to redial */
#define	NEXT_REDIAL_PERIOD 3	/* Default Hold time to next number redial */

#define	CONFFILE 	"ppp.conf"
#define	LINKUPFILE 	"ppp.linkup"
#define	LINKDOWNFILE 	"ppp.linkdown"
#define	SECRETFILE	"ppp.secret"

/*
 *  Definition of working mode
 */
#define MODE_INTER	1	/* Interactive mode */
#define MODE_AUTO	2	/* Auto calling mode */
#define	MODE_DIRECT	4	/* Direct connection mode */
#define	MODE_DEDICATED	8	/* Dedicated line mode */
#define	MODE_DDIAL	16	/* Dedicated dialing line mode */
#define	MODE_ALIAS	32	/* Packet aliasing (masquerading) */
#define MODE_BACKGROUND 64	/* Background mode. */


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
#define	EX_HANGUP	10
#define	EX_TERM		11
#define EX_NODIAL	12
#define EX_NOLOGIN	13

int mode;
int BGFiledes[2];

int modem;
int tun_in, tun_out;
int netfd;
char *dstsystem;

#ifndef TRUE
#define	TRUE 	(1)
#endif
#ifndef FALSE
#define	FALSE 	(0)
#endif

#endif				/* _DEFS_H_ */
