/* report.h */
/* $FreeBSD: src/libexec/bootpd/report.h,v 1.1.1.1.14.1 2001/03/05 10:58:59 kris Exp $ */

#ifdef	__STDC__
#define P(args) args
#else
#define P(args) ()
#endif

extern void report_init P((int nolog));
extern void report P((int, const char *, ...));
extern const char *get_errmsg P((void));

#undef P
