/* messages.c - error reporter -
   Copyright (C) 1987, 1991, 1992 Free Software Foundation, Inc.
   
   This file is part of GAS, the GNU Assembler.
   
   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef lint
static char rcsid[] = "$Id: messages.c,v 1.2 1993/11/03 00:52:01 paul Exp $";
#endif

#include <stdio.h> /* define stderr */
#include <errno.h>

#include "as.h"

#ifndef NO_STDARG
#include <stdarg.h>
#else
#ifndef NO_VARARGS
#include <varargs.h>
#endif /* NO_VARARGS */
#endif /* NO_STDARG */

/*
 * Despite the rest of the comments in this file, (FIXME-SOON),
 * here is the current scheme for error messages etc:
 *
 * as_fatal() is used when gas is quite confused and
 * continuing the assembly is pointless.  In this case we
 * exit immediately with error status.
 *
 * as_bad() is used to mark errors that result in what we
 * presume to be a useless object file.  Say, we ignored
 * something that might have been vital.  If we see any of
 * these, assembly will continue to the end of the source,
 * no object file will be produced, and we will terminate
 * with error status.  The new option, -Z, tells us to
 * produce an object file anyway but we still exit with
 * error status.  The assumption here is that you don't want
 * this object file but we could be wrong.
 *
 * as_warn() is used when we have an error from which we
 * have a plausible error recovery.  eg, masking the top
 * bits of a constant that is longer than will fit in the
 * destination.  In this case we will continue to assemble
 * the source, although we may have made a bad assumption,
 * and we will produce an object file and return normal exit
 * status (ie, no error).  The new option -X tells us to
 * treat all as_warn() errors as as_bad() errors.  That is,
 * no object file will be produced and we will exit with
 * error status.  The idea here is that we don't kill an
 * entire make because of an error that we knew how to
 * correct.  On the other hand, sometimes you might want to
 * stop the make at these points.
 *
 * as_tsktsk() is used when we see a minor error for which
 * our error recovery action is almost certainly correct.
 * In this case, we print a message and then assembly
 * continues as though no error occurred.
 */

/*
  ERRORS
  
  JF: this is now bogus.  We now print more standard error messages
  that try to look like everyone else's.
  
  We print the error message 1st, beginning in column 1.
  All ancillary info starts in column 2 on lines after the
  key error text.
  We try to print a location in logical and physical file
  just after the main error text.
  Caller then prints any appendices after that, begining all
  lines with at least 1 space.
  
  Optionally, we may die.
  There is no need for a trailing '\n' in your error text format
  because we supply one.
  
  as_warn(fmt,args)  Like fprintf(stderr,fmt,args) but also call errwhere().
  
  as_fatal(fmt,args) Like as_warn() but exit with a fatal status.
  
  */

static int warning_count = 0; /* Count of number of warnings issued */

int had_warnings() {
	return(warning_count);
} /* had_err() */

/* Nonzero if we've hit a 'bad error', and should not write an obj file,
   and exit with a nonzero error code */

static int error_count = 0;

int had_errors() {
	return(error_count);
} /* had_errors() */


/*
 *			a s _ p e r r o r
 *
 * Like perror(3), but with more info.
 */
void as_perror(gripe, filename)
char *gripe;		/* Unpunctuated error theme. */
char *filename;
{
#ifndef HAVE_STRERROR
	extern char *strerror();
#endif /* HAVE_STRERROR */

	as_where();
	fprintf(stderr, gripe, filename);
	fprintf(stderr, "%s.\n", strerror(errno));
	errno = 0; /* After reporting, clear it. */
} /* as_perror() */

/*
 *			a s _ t s k t s k ()
 *
 * Send to stderr a string (with bell) (JF: Bell is obnoxious!) as a warning, and locate warning
 * in input file(s).
 * Please only use this for when we have some recovery action.
 * Please explain in string (which may have '\n's) what recovery was done.
 */

#ifndef NO_STDARG
void as_tsktsk(Format)
const char *Format;
{
	va_list args;
	
	as_where();
	va_start(args, Format);
	vfprintf(stderr, Format, args);
	va_end(args);
	(void) putc('\n', stderr);
} /* as_tsktsk() */
#else
#ifndef NO_VARARGS
void as_tsktsk(Format,va_alist)
char *Format;
va_dcl
{
	va_list args;
	
	as_where();
	va_start(args);
	vfprintf(stderr, Format, args);
	va_end(args);
	(void) putc('\n', stderr);
} /* as_tsktsk() */
#else
/*VARARGS1 */
as_tsktsk(Format,args)
char *Format;
{
	as_where();
	_doprnt (Format, &args, stderr);
	(void)putc ('\n', stderr);
	/* as_where(); */
} /* as_tsktsk */
#endif /* not NO_VARARGS */
#endif /* not NO_STDARG */

#ifdef DONTDEF
void as_tsktsk(Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an)
char *format;
{
	as_where();
	fprintf(stderr,Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an);
	(void)putc('\n',stderr);
} /* as_tsktsk() */
#endif
/*
 *			a s _ w a r n ()
 *
 * Send to stderr a string (with bell) (JF: Bell is obnoxious!) as a warning, and locate warning
 * in input file(s).
 * Please only use this for when we have some recovery action.
 * Please explain in string (which may have '\n's) what recovery was done.
 */

#ifndef NO_STDARG
void as_warn(Format)
const char *Format;
{
	va_list args;
	char buffer[200];
	
	if (!flagseen['W']) {
		++warning_count;
		as_where();
		va_start(args, Format);
		fprintf(stderr,"Warning: ");
		vsprintf(buffer, Format, args);
		fprintf(stderr, buffer);
#ifndef NO_LISTING
		listing_warning(buffer);
#endif
		va_end(args);
		(void) putc('\n', stderr);
	}
} /* as_warn() */
#else
#ifndef NO_VARARGS
void as_warn(Format,va_alist)
char *Format;
va_dcl
{
	va_list args;
	char buffer[200];
	
	if (!flagseen['W']) {
		++warning_count;
		as_where();
		va_start(args);
		fprintf(stderr,"Warning: ");
		vsprintf(buffer, Format, args);
		fprintf(stderr,buffer);
#ifndef NO_LISTING
		listing_warning(buffer);
#endif		
		va_end(args);
		(void) putc('\n', stderr);
	}
} /* as_warn() */
#else
/*VARARGS1 */
as_warn(Format,args)
char *Format;
{
	/* -W supresses warning messages. */
	if (! flagseen['W']) {
		++warning_count;
		as_where();
		_doprnt (Format, &args, stderr);
		(void)putc ('\n', stderr);
		/* as_where(); */
	}
} /* as_warn() */
#endif /* not NO_VARARGS */
#endif /* not NO_STDARG */

#ifdef DONTDEF
void as_warn(Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an)
char *format;
{
	if (!flagseen['W']) {
		++warning_count;
		as_where();
		fprintf(stderr,Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an);
		(void)putc('\n',stderr);
	}
} /* as_warn() */
#endif
/*
 *			a s _ b a d ()
 *
 * Send to stderr a string (with bell) (JF: Bell is obnoxious!) as a warning,
 * and locate warning in input file(s).
 * Please us when there is no recovery, but we want to continue processing
 * but not produce an object file.
 * Please explain in string (which may have '\n's) what recovery was done.
 */

#ifndef NO_STDARG
void as_bad(Format)
const char *Format;
{
	va_list args;
	char buffer[200];
	
	++error_count;
	as_where();
	va_start(args, Format);
	fprintf(stderr,"Error: ");
	
	vsprintf(buffer, Format, args);
	fprintf(stderr,buffer);
#ifndef NO_LISTING
	listing_error(buffer);
#endif
	va_end(args);
	(void) putc('\n', stderr);
} /* as_bad() */
#else
#ifndef NO_VARARGS
void as_bad(Format,va_alist)
char *Format;
va_dcl
{
	va_list args;
	char buffer[200];
	
	++error_count;
	as_where();
	va_start(args);
	vsprintf(buffer, Format, args);
	fprintf(stderr,buffer);
#ifndef NO_LISTING
	listing_error(buffer);
#endif
	
	va_end(args);
	(void) putc('\n', stderr);
}				/* as_bad() */
#else
/*VARARGS1 */
as_bad(Format,args)
char *Format;
{
	++error_count;
	
	as_where();
	fprintf(stderr,"Error: ");
	_doprnt (Format, &args, stderr);
	(void)putc ('\n', stderr);
	/* as_where(); */
} /* as_bad() */
#endif /* not NO_VARARGS */
#endif /* not NO_STDARG */

#ifdef DONTDEF
void as_bad(Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an)
char *format;
{
	++error_count;
	as_where();
	fprintf(stderr,Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an);
	(void)putc('\n',stderr);
} /* as_bad() */
#endif

/*
 *			a s _ f a t a l ()
 *
 * Send to stderr a string (with bell) (JF: Bell is obnoxious!) as a fatal
 * message, and locate stdsource in input file(s).
 * Please only use this for when we DON'T have some recovery action.
 * It exit()s with a warning status.
 */

#ifndef NO_STDARG
void as_fatal(Format)
const char *Format;
{
	va_list args;
	
	as_where();
	va_start(args, Format);
	fprintf (stderr, "FATAL:");
	vfprintf(stderr, Format, args);
	(void) putc('\n', stderr);
	va_end(args);
	exit(33);
} /* as_fatal() */
#else
#ifndef NO_VARARGS
void as_fatal(Format,va_alist)
char *Format;
va_dcl
{
	va_list args;
	
	as_where();
	va_start(args);
	fprintf (stderr, "FATAL:");
	vfprintf(stderr, Format, args);
	(void) putc('\n', stderr);
	va_end(args);
	exit(33);
} /* as_fatal() */
#else
/*VARARGS1 */
as_fatal(Format, args)
char *Format;
{
	as_where();
	fprintf(stderr,"FATAL:");
	_doprnt (Format, &args, stderr);
	(void)putc ('\n', stderr);
	/* as_where(); */
	exit(33);		/* What is a good exit status? */
} /* as_fatal() */
#endif /* not NO_VARARGS */
#endif /* not NO_STDARG */

#ifdef DONTDEF
void as_fatal(Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an)
char *Format;
{
	as_where();
	fprintf (stderr, "FATAL:");
	fprintf(stderr, Format,aa,ab,ac,ad,ae,af,ag,ah,ai,aj,ak,al,am,an);
	(void) putc('\n', stderr);
	exit(33);
} /* as_fatal() */
#endif

/* end of messages.c */
