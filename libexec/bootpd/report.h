/* report.h */
/* $FreeBSD: src/libexec/bootpd/report.h,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $ */

extern void report_init(int nolog);
extern void report(int, const char *, ...) __printflike(2, 3);
extern const char *get_errmsg(void);
