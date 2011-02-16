#ifndef _PRUTIL_H_
#define _PRUTIL_H_

/*
 * $FreeBSD: src/tools/regression/p1003_1b/prutil.h,v 1.1.60.1 2010/12/21 17:10:29 kensmith Exp $
 */

struct sched_param;

void quit(const char *);
char *sched_text(int);
int sched_is(int line, struct sched_param *, int);

#endif /* _PRUTIL_H_ */
