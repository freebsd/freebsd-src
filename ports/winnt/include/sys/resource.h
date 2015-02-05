/*
 * ports/winnt/include/sys/resource.h
 *
 * routines declared in Unix systems' sys/resource.h
 */

#define	PRIO_PROCESS	0
#define	NTP_PRIO	(-12)

int setpriority(int, int, int);		/* winnt\libntp\setpriority.c */
