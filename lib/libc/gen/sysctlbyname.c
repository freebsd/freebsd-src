/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */
#include <sys/types.h>
#include <sys/sysctl.h>

int
sysctlbyname(char *name, void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
	int name2oid_oid[2];
	int real_oid[CTL_MAXNAME+2];
	int error, oidlen;

	name2oid_oid[0] = 0;	/* This is magic & undocumented! */
	name2oid_oid[1] = 3;

	oidlen = sizeof(real_oid);
	error = sysctl(name2oid_oid, 2, real_oid, &oidlen, name, strlen(name));
	if (error < 0) 
		return error;
	oidlen /= sizeof (int);
	error = sysctl(real_oid, oidlen, oldp, oldlenp, newp, newlen);
	return (error);
}

