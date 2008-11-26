/* report.h */
/* $FreeBSD: src/libexec/bootpd/report.h,v 1.5.30.1 2008/10/02 02:57:24 kensmith Exp $ */

extern void report_init(int nolog);
extern void report(int, const char *, ...) __printflike(2, 3);
extern const char *get_errmsg(void);
