/*
 * $Id: log.h,v 1.14 1997/10/26 01:03:06 brian Exp $
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
#define LogIPCP		(9)
#define LogLCP		(10)
#define LogLINK		(11)
#define LogLQM		(12)
#define LogPHASE	(13)
#define LogTCPIP	(14)
#define LogTUN		(15)	/* If set, tun%d is output with each message */
#define LogMAXCONF	(15)
#define LogWARN		(16)	/* Sent to VarTerm else syslog(LOG_WARNING, ) */
#define LogERROR	(17)	/* syslog(LOG_ERR, ....), + sent to VarTerm */
#define LogALERT	(18)	/* syslog(LOG_ALERT, ....)	 */
#define LogMAX		(18)

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
extern void LogPrintf(int, char *,...);
extern void LogDumpBp(int, char *, struct mbuf *);
extern void LogDumpBuff(int, char *, u_char *, int);
