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
 * $Id: defs.h,v 1.3 1995/09/02 17:20:51 amurai Exp $
 *
 *	TODO:
 */

#ifndef _DEFS_H_
#define	_DEFS_H_

#include <machine/endian.h>
#include <sys/types.h>
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
#define	LOGFILE		"/var/log/ppp.log"	/* Name of log file */
#ifdef __FreeBSD__
#define	MODEM_DEV	"/dev/cuaa1"		/* name of tty device */
#else
#define	MODEM_DEV	"/dev/tty01"		/* name of tty device */
#endif
#define MODEM_SPEED	B38400			/* tty speed */
#define	SERVER_PORT	3000			/* Base server port no. */

#define	REDIAL_PERIOD	30			/* Hold time to redial */

#define	CONFFILE 	"ppp.conf"
#define	LINKFILE 	"ppp.linkup"
#define	ETHERFILE	"ppp.etherup"
#define	SECRETFILE	"ppp.secret"

/*
 *  Definition of working mode
 */
#define MODE_INTER	1	/* Interactive mode */
#define MODE_AUTO	2	/* Auto calling mode */
#define	MODE_DIRECT	4	/* Direct connection mode */
#define	MODE_DEDICATED	8	/* Dedicated line mode */

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

int mode;

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

#endif	/* _DEFS_H_ */
