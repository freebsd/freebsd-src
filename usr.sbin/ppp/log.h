#define LogMIN		(1)
#define LogASYNC	(1)	/* syslog(LOG_INFO, ....)	*/
#define LogCARRIER	(2)
#define LogCHAT		(3)
#define LogCOMMAND	(4)
#define LogCONNECT	(5)
#define LogDEBUG	(6)	/* syslog(LOG_DEBUG, ....)	*/
#define LogHDLC		(7)
#define LogLCP		(8)
#define LogLINK		(9)
#define LogLQM		(10)
#define LogPHASE	(11)
#define LogTCPIP	(12)
#define LogTUN		(13)	/* If set, tun%d is output with each message */
#define LogMAXCONF	(13)
#define LogWARN		(14)	/* Sent to VarTerm else syslog(LOG_WARNING, ) */
#define LogERROR	(15)	/* syslog(LOG_ERR, ....), + sent to VarTerm */
#define LogALERT	(16)	/* syslog(LOG_ALERT, ....)	*/
#define LogMAX		(16)

/* The first int arg for all of the following is one of the above values */
extern const char *LogName(int);
extern void LogKeep(int);
extern void LogDiscard(int);
extern void LogDiscardAll();
extern int LogIsKept(int);
extern void LogOpen(const char *);
extern void LogSetTun(int);
extern void LogClose();
extern void LogPrintf(int, char *, ...);
extern void LogDumpBp(int, char *hdr, struct mbuf *bp);
extern void LogDumpBuff(int, char *hdr, u_char *ptr, int n);
