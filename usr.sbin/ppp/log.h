/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
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
 *	$Id$
 */

#define LogMIN		(1)
#define LogASYNC	(1)	/* syslog(LOG_INFO, ....)	 */
#define LogCARRIER	(2)
#define LogCCP		(3)
#define LogCHAT		(4)
#define LogCOMMAND	(5)
#define LogCONNECT	(6)
#define LogDEBUG	(7)	/* syslog(LOG_DEBUG, ....)	 */
#define LogHDLC		(8)
#define LogID0		(9)
#define LogIPCP		(10)
#define LogLCP		(11)
#define LogLINK		(12)
#define LogLQM		(13)
#define LogPHASE	(14)
#define LogTCPIP	(15)
#define LogTUN		(16)	/* If set, tun%d is output with each message */
#define LogMAXCONF	(16)
#define LogWARN		(17)	/* Sent to VarTerm else syslog(LOG_WARNING, ) */
#define LogERROR	(18)	/* syslog(LOG_ERR, ....), + sent to VarTerm */
#define LogALERT	(19)	/* syslog(LOG_ALERT, ....)	 */
#define LogMAX		(19)

/* The first int arg for all of the following is one of the above values */
extern const char *LogName(int);
extern void LogKeep(int);
extern void LogKeepLocal(int);
extern void LogDiscard(int);
extern void LogDiscardLocal(int);
extern void LogDiscardAll(void);
extern void LogDiscardAllLocal(void);
#define LOG_KEPT_SYSLOG (1)	/* Results of LogIsKept() */
#define LOG_KEPT_LOCAL  (2)	/* Results of LogIsKept() */
extern int LogIsKept(int);
extern void LogOpen(const char *);
extern void LogSetTun(int);
extern void LogClose(void);
extern void LogPrintf(int, const char *,...);
extern void LogDumpBp(int, const char *, const struct mbuf *);
extern void LogDumpBuff(int, const char *, const u_char *, int);
