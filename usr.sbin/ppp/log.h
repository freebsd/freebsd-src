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
 *	$Id: log.h,v 1.18.2.5 1998/04/24 19:15:44 brian Exp $
 */

#define LogMIN		(1)
#define LogASYNC	(1)	/* syslog(LOG_INFO, ....)	 */
#define LogCCP		(2)
#define LogCHAT		(3)
#define LogCOMMAND	(4)
#define LogCONNECT	(5)
#define LogDEBUG	(6)	/* syslog(LOG_DEBUG, ....)	 */
#define LogTIMER	(7)	/* syslog(LOG_DEBUG, ....)	 */
#define LogHDLC		(8)
#define LogID0		(9)
#define LogIPCP		(10)
#define LogLCP		(11)
#define LogLQM		(12)
#define LogPHASE	(13)
#define LogTCPIP	(14)
#define LogTUN		(15)	/* If set, tun%d is output with each message */
#define LogMAXCONF	(15)
#define LogWARN		(16)	/* Sent to VarTerm else syslog(LOG_WARNING, ) */
#define LogERROR	(17)	/* syslog(LOG_ERR, ....), + sent to VarTerm */
#define LogALERT	(18)	/* syslog(LOG_ALERT, ....)	 */
#define LogMAX		(18)

struct mbuf;
struct cmdargs;
struct prompt;

/* The first int arg for all of the following is one of the above values */
extern const char *LogName(int);
extern void LogKeep(int);
extern void LogKeepLocal(int, u_long *);
extern void LogDiscard(int);
extern void LogDiscardLocal(int, u_long *);
extern void LogDiscardAll(void);
extern void LogDiscardAllLocal(u_long *);
#define LOG_KEPT_SYSLOG (1)	/* Results of LogIsKept() */
#define LOG_KEPT_LOCAL  (2)	/* Results of LogIsKept() */
extern int LogIsKept(int);
extern int LogIsKeptLocal(int, u_long);
extern void LogOpen(const char *);
extern void LogSetTun(int);
extern void LogClose(void);
#ifdef __GNUC__
extern void LogPrintf(int, const char *,...)
            __attribute__ ((format (printf, 2, 3)));
#else
extern void LogPrintf(int, const char *,...);
#endif
extern void LogDumpBp(int, const char *, const struct mbuf *);
extern void LogDumpBuff(int, const char *, const u_char *, int);
extern void log_RegisterPrompt(struct prompt *);
extern void log_UnRegisterPrompt(struct prompt *);
extern int log_ShowLevel(struct cmdargs const *);
extern int log_SetLevel(struct cmdargs const *);
extern int log_ShowWho(struct cmdargs const *);
