/*	$NetBSD: strerror.c,v 1.3 2005/05/11 01:01:56 lukem Exp $	*/

#include "tnftp.h"

char *
strerror(int n)
{
	static char msg[] = "Unknown error (1234567890)";

	extern int sys_nerr;
	extern char *sys_errlist[];

	if (n >= sys_nerr) {
		snprintf(msg, sizeof(msg), "Unknown error (%d)", n);
		return(msg);
	} else {
		return(sys_errlist[n]);
	}
}
