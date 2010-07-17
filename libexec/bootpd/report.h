/* report.h */
/* $FreeBSD: src/libexec/bootpd/report.h,v 1.5.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

extern void report_init(int nolog);
extern void report(int, const char *, ...) __printflike(2, 3);
extern const char *get_errmsg(void);
