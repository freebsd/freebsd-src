/* report.h */
/* $FreeBSD$ */

#define P(args) args

extern void report_init P((int nolog));
extern void report P((int, const char *, ...)) __printflike(2, 3);
extern const char *get_errmsg P((void));

#undef P
