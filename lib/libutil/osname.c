/*
 * This file is in the public domain.
 * Written by Garrett A. Wollman <wollman@freefall.cdrom.com>.
 *
 * $Id: osname.c,v 1.1 1994/02/04 02:55:24 wollman Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>

/*
 * _osname - return the name of the current operating system.
 */
char *
_osname(void) {
	static struct utsname uts;
	if(uname(&uts))
	  return "unknown";
	else
	  return uts.sysname;
}

/*
 * _osnamever - return the name and version of the current operating system.
 */
char *
_osnamever(void) {
	static struct utsname uts;
	static char name[2*SYS_NMLN + 1];
	if(uname(&uts)) {
		return "unknown";
	} else {
		strcpy(name, uts.sysname);
		strcat(name, " ");
		strcat(name, uts.release);
		return name;
	}
}

	  

