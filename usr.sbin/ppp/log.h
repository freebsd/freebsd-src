/*
 * $Id: log.h,v 1.15 1997/11/04 01:17:01 brian Exp $
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
extern void LogPrintf(int, char *,...);
extern void LogDumpBp(int, char *, struct mbuf *);
extern void LogDumpBuff(int, char *, u_char *, int);
