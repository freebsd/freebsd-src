/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b daemon - location of files
 *	------------------------------
 *
 *	$Id: pathnames.h,v 1.10 1999/12/13 21:25:25 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdnd/pathnames.h,v 1.6 1999/12/14 21:07:30 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:48:01 1999]
 *
 *---------------------------------------------------------------------------*/

#ifndef _PATHNAMES_H_
#define _PATHNAMES_H_

#define I4BDEVICE	"/dev/i4b"

#define ETCPATH		"/etc/isdn"
#define CONFIG_FILE_DEF	"/etc/isdn/isdnd.rc"
#define RATES_FILE_DEF	"/etc/isdn/isdnd.rates"

#define TINA_FILE_DEF	"/etc/isdn/tinainitprog"

#define LOG_FILE_DEF	"/var/log/isdnd.log"
#ifdef __bsdi__
#define ACCT_FILE_DEF	"/var/log/isdnd.acct"
#else
#define ACCT_FILE_DEF	"/var/account/isdnd.acct"
#endif

#define PIDFILE		"/var/run/isdnd.pid"

#endif /* _PATHNAMES_H_ */

/* EOF */
