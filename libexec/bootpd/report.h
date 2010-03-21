/* report.h */
/* $FreeBSD: src/libexec/bootpd/report.h,v 1.5.38.1 2010/02/10 00:26:20 kensmith Exp $ */

extern void report_init(int nolog);
extern void report(int, const char *, ...) __printflike(2, 3);
extern const char *get_errmsg(void);
