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
 * $Id: log.h,v 1.9 1997/03/13 14:53:54 brian Exp $
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
#define LOG_PHASE_BIT	(1 << LOG_PHASE)
#		define	LM_PHASE	"Phase"
#define	LOG_CHAT	1
#define LOG_CHAT_BIT	(1 << LOG_CHAT)
#		define	LM_CHAT		"Chat"
#define	LOG_LQM		2
#define LOG_LQM_BIT	(1 << LOG_LQM)
#		define	LM_LQM		"LQM"
#define	LOG_LCP		3
#define LOG_LCP_BIT	(1 << LOG_LCP)
#		define	LM_LCP		"LCP"
#define	LOG_TCPIP	4
#define LOG_TCPIP_BIT	(1 << LOG_TCPIP)
#		define	LM_TCPIP	"TCP/IP"
#define	LOG_HDLC	5
#define LOG_HDLC_BIT	(1 << LOG_HDLC)
#		define	LM_HDLC		"HDLC"
#define	LOG_ASYNC	6
#define LOG_ASYNC_BIT	(1 << LOG_ASYNC)
#		define	LM_ASYNC	"Async"
#define LOG_LINK	7
#define LOG_LINK_BIT	(1 << LOG_LINK)
#		define 	LM_LINK		"Link"
#define LOG_CONNECT	8
#define LOG_CONNECT_BIT	(1 << LOG_CONNECT)
#		define 	LM_CONNECT	"Connect"
#define LOG_CARRIER	9
#define LOG_CARRIER_BIT	(1 << LOG_CARRIER)
#		define 	LM_CARRIER	"Carrier"
#define	MAXLOGLEVEL	10

extern int loglevel, tunno;
extern char *logptr;

extern void LogTimeStamp __P((void));
extern int LogOpen __P((int));
extern void LogReOpen __P((int));
extern void DupLog __P((void));
extern void LogClose __P((void));
extern void logprintf __P((char *, ...)), LogPrintf __P((int, char *, ...));
extern void LogDumpBp __P((int level, char *header, struct mbuf *bp));
extern void LogDumpBuff __P((int level, char *header, u_char *ptr, int cnt));
extern void ListLog __P((void));
#endif
