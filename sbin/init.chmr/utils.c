/*
 * Copyright (c) 1993 Christoph M. Robitschko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christoph M. Robitschko
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * utils.c
 * Misc. utility functions
 */

#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/signal.h>
#include "init.h"
#include "prototypes.h"


/* global variable */
char		**ienviron;


/*
 * IPUTENV
 * put an environment variable in a separate space
 */
void
iputenv(var, val)
const char	*var, *val;
{
static int	ienvalloc = 0;
char		*plaza;
char		**ienv;
int		ienvcount;
int		varlen;


	varlen = strlen(var) + 1;			/* VAR= */
	plaza = malloc(varlen + strlen(val) + 1);
	if (!plaza)
		return;			/* fail miserably */
/* 	sprintf(plaza, "%s=%s", var, val); */
	strcpy(plaza, var);
	strcat(plaza, "=");
	strcat(plaza, val);


	if (!ienvalloc) {
		ienviron = malloc(4*sizeof(char *));
		if (!ienviron) return;
		ienvalloc = 4;
		*ienviron = (char *)0;
	}

	/* Search ienviron if variable is already defined */
	for (ienvcount = 0, ienv = ienviron; *ienv; ienv++, ienvcount++)
		if (!strncmp(*ienv, plaza, varlen)) {
			free(*ienv);
			*ienv = plaza;
			return;
		}

	/* Not found, create new environ entry */
	if (ienvcount >= ienvalloc)
		if (( ienviron = realloc (ienviron, (ienvalloc + 4)* sizeof (char *))))
			ienvalloc += 4;
		else
			return;			/* kaaplotz */

	for (ienv = ienviron; *ienv; ienv++);
	*ienv = plaza;
	*(++ienv) = (char *)0;
	return;
}



/*
 * dEBUG
 * print a message if current debug level > log level
 */
void
Debug(int level, const char *format, ...)
{
#ifdef DEBUG
	va_list		args;


	if (level > debug)
		return;
	va_start(args, format);
	vsyslog(LOG_DEBUG, format, args);
	va_end(args);
#endif
}


#undef toupper
/*
 * STRcCMP
 * see if s2 is the beginning of s1, ignoring case
 */
int
strCcmp(s1, s2)			/* strncasecmp(s1, s2, strlen(s1)) */
char		*s1, *s2;
{


	for (;*s1; s1++, s2++)
		if (toupper(*s1) != toupper(*s2))
			return(1);
	return(0);
}



/*
 * NEWSTRING
 * allocate memory for a string and copy it.
 */
char *
newstring(string)
const char	*string;
{
char		*s;

	s = (char *)malloc(strlen(string)+1);
	if (s)
		strcpy(s, string);
	return(s);
}



/*
 * STR2U
 * atoi with error handling. errors are signalled with a negative return value
 */
long
str2u(str)
const char	*str;
{
char	*s;
long	res;

	res = strtol(str, &s, 0);
	if ((s != str) && !*s)
		return(res);
	return(-1);
}
