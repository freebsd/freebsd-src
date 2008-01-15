/* report.h */
/* $FreeBSD: src/libexec/bootpd/report.h,v 1.5 2002/05/28 18:36:43 alfred Exp $ */

extern void report_init(int nolog);
extern void report(int, const char *, ...) __printflike(2, 3);
extern const char *get_errmsg(void);
