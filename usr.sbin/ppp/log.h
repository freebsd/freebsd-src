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
 * $Id: log.h,v 1.4 1995/02/27 03:18:16 amurai Exp $
 *
 *	TODO:
 */

#ifndef _LOG_H_
#define	_LOG_H_
#include "cdefs.h"
/*
 *  Definition of log level
 */
#define	LOG_PHASE	0
#		define	LM_PHASE	"Phase"
#define	LOG_CHAT	1
#		define	LM_CHAT		"Chat"
#define	LOG_LQM		2
#		define	LM_LQM		"LQM"
#define	LOG_LCP		3
#		define	LM_LCP		"LCP"
#define	LOG_TCPIP	4
#		define	LM_TCPIP	"TCP/IP"
#define	LOG_HDLC	5
#		define	LM_HDLC		"HDLC"
#define	LOG_ASYNC	6
#		define	LM_ASYNC	"Async"
#define	MAXLOGLEVEL	7

extern int loglevel;

extern void LogTimeStamp __P((void));
extern int LogOpen __P((void));
extern void DupLog __P((void));
extern void LogClose __P((void));
extern void logprintf __P((char *, ...)), LogPrintf __P((int, char *, ...));
extern void LogDumpBp __P((int level, char *header, struct mbuf *bp));
extern void LogDumpBuff __P((int level, char *header, u_char *ptr, int cnt));
extern void ListLog __P((void));
#endif
