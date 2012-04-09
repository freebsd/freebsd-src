/* report.h */
/* $FreeBSD: src/libexec/bootpd/report.h,v 1.5.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $ */

extern void report_init(int nolog);
extern void report(int, const char *, ...) __printflike(2, 3);
extern const char *get_errmsg(void);
