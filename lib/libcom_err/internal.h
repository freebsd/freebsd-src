/*
 * internal include file for com_err package
 */
#include "mit-sipb-copyright.h"
#ifndef __STDC__
#undef const
#define const
#endif

extern int errno;
extern char const * const sys_errlist[];
extern /* const */ int sys_nerr;

#ifdef __STDC__
void perror (const char *);
#else
int perror ();
#endif
