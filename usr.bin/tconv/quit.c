/*
 * quit.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:14
 *
 * quit with a diagnostic message printed on stderr
 *
 */

#define NOTLIB
#include "defs.h"

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo quit.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

char *prg_name;

#if defined(USE_PROTOTYPES) && !defined(lint)
void (*cleanup)(int);
#else
void (*cleanup)();
#endif

/* PRINTFLIKE2 */
noreturn
#ifdef USE_STDARG
#ifdef USE_PROTOTYPES
void
quit(int e, char *fmt, ...)
#else
void quit(e, fmt)
int e;
char *fmt;
#endif
#else
void quit(va_alist)
va_dcl
#endif
{
#ifndef USE_STDARG
	int e;
	char *fmt;
#endif
	va_list ap;

#ifdef USE_STDARG
	va_start(ap, fmt);
#else
	va_start(ap);
	e = va_arg(ap, int);
	fmt = va_arg(ap, char *);
#endif

	(*cleanup)(e);

	if (e != 0)
		fprintf(stderr, "%s: ", prg_name);
#ifdef USE_DOPRNT
	_doprnt(fmt, ap, stderr);
#else
	vfprintf(stderr, fmt, ap);
#endif
	putc('\n', stderr);
	if (e > 0 && e < sys_nerr) {
		fprintf(stderr, "%d - %s\n", e, sys_errlist[e]);
	}
	fflush(stderr);
	exit(e);
}
