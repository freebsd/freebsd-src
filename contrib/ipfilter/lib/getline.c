/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: getline.c,v 1.3 2001/06/09 17:09:24 darrenr Exp
 */

#include <stdio.h>
#if !defined(__SVR4) && !defined(__GNUC__)
#include <strings.h>
#endif
#include <string.h>
#include "ipf.h"


/*
 * Similar to fgets(3) but can handle '\\' and NL is converted to NUL.
 * Returns NULL if error occured, EOF encounterd or input line is too long.
 */
char *getline(str, size, file, linenum)
register char	*str;
size_t	size;
FILE	*file;
int	*linenum;
{
	char *p;
	int s, len;

	do {
		for (p = str, s = size;; p += (len - 1), s -= (len - 1)) {
			/*
			 * if an error occured, EOF was encounterd, or there
			 * was no room to put NUL, return NULL.
			 */
			if (fgets(p, s, file) == NULL)
				return (NULL);
			len = strlen(p);
			if (p[len - 1] != '\n') {
				p[len] = '\0';
				break;
			}
			(*linenum)++;
			p[len - 1] = '\0';
			if (len < 2 || p[len - 2] != '\\')
				break;
			else
				/*
				 * Convert '\\' to a space so words don't
				 * run together
				 */
				p[len - 2] = ' ';
		}
	} while (*str == '\0');
	return (str);
}
