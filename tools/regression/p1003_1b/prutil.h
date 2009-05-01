#ifndef _PRUTIL_H_
#define _PRUTIL_H_

/*
 * $FreeBSD: src/tools/regression/p1003_1b/prutil.h,v 1.1.54.1 2009/04/15 03:14:26 kensmith Exp $
 */

struct sched_param;

void quit(const char *);
char *sched_text(int);
int sched_is(int line, struct sched_param *, int);

#endif /* _PRUTIL_H_ */
