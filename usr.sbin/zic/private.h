#ifndef PRIVATE_H

#define PRIVATE_H

/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
*/

/*
 * FreeBSD modifications: separate libc's privates from zic's.
 * This makes it easier when we need to update one but not the other.
 * I have removed all of the ifdef spaghetti which is not relevant to
 * zic from this file.
 *
 * $FreeBSD$
 */

/*
** This header is for use ONLY with the time conversion code.
** There is no guarantee that it will remain unchanged,
** or that it will remain at all.
** Do NOT copy it to any system include directory.
** Thank you!
*/

/*
** ID
*/

#ifndef lint
#ifndef NOID
static const char	privatehid[] = "@(#)private.h	7.53";
#endif /* !defined NOID */
#endif /* !defined lint */

/*
** Defaults for preprocessor symbols.
** You can override these in your C compiler options, e.g. `-DHAVE_ADJTIME=0'.
*/

#ifndef HAVE_GETTEXT
#define HAVE_GETTEXT		0
#endif /* !defined HAVE_GETTEXT */

#ifndef HAVE_STRERROR
#define HAVE_STRERROR		1
#endif /* !defined HAVE_STRERROR */

#ifndef HAVE_SYMLINK
#define HAVE_SYMLINK		1
#endif /* !defined HAVE_SYMLINK */

#ifndef HAVE_SYS_STAT_H
#define HAVE_SYS_STAT_H		1
#endif /* !defined HAVE_SYS_STAT_H */

#ifndef HAVE_SYS_WAIT_H
#define HAVE_SYS_WAIT_H		1
#endif /* !defined HAVE_SYS_WAIT_H */

#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H		1
#endif /* !defined HAVE_UNISTD_H */

/*
** Nested includes
*/

#include "sys/types.h"	/* for time_t */
#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "limits.h"	/* for CHAR_BIT */
#include "time.h"
#include "stdlib.h"

#if HAVE_GETTEXT - 0
#include "libintl.h"
#endif /* HAVE_GETTEXT - 0 */

#if HAVE_SYS_WAIT_H - 0
#include <sys/wait.h>	/* for WIFEXITED and WEXITSTATUS */
#endif /* HAVE_SYS_WAIT_H - 0 */

#if HAVE_UNISTD_H - 0
#include "unistd.h"	/* for F_OK and R_OK */
#endif /* HAVE_UNISTD_H - 0 */

#if !(HAVE_UNISTD_H - 0)
#ifndef F_OK
#define F_OK	0
#endif /* !defined F_OK */
#ifndef R_OK
#define R_OK	4
#endif /* !defined R_OK */
#endif /* !(HAVE_UNISTD_H - 0) */

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX.  */
#define is_digit(c) ((unsigned)(c) - '0' <= 9)

#define P(x) x

/*
** Private function declarations.
*/
char *	icalloc P((int nelem, int elsize));
char *	icatalloc P((char * old, const char * new));
char *	icpyalloc P((const char * string));
char *	imalloc P((int n));
void *	irealloc P((void * pointer, int size));
void	icfree P((char * pointer));
void	ifree P((char * pointer));
char *	scheck P((const char *string, const char *format));

/*
** Finally, some convenience items.
*/

#ifndef TRUE
#define TRUE	1
#endif /* !defined TRUE */

#ifndef FALSE
#define FALSE	0
#endif /* !defined FALSE */

#ifndef TYPE_BIT
#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#endif /* !defined TYPE_BIT */

#ifndef TYPE_SIGNED
#define TYPE_SIGNED(type) (((type) -1) < 0)
#endif /* !defined TYPE_SIGNED */

#ifndef INT_STRLEN_MAXIMUM
/*
** 302 / 1000 is log10(2.0) rounded up.
** Subtract one for the sign bit if the type is signed;
** add one for integer division truncation;
** add one more for a minus sign if the type is signed.
*/
#define INT_STRLEN_MAXIMUM(type) \
    ((TYPE_BIT(type) - TYPE_SIGNED(type)) * 302 / 1000 + 1 + TYPE_SIGNED(type))
#endif /* !defined INT_STRLEN_MAXIMUM */

/*
** INITIALIZE(x)
*/

#ifndef GNUC_or_lint
#ifdef lint
#define GNUC_or_lint
#endif /* defined lint */
#ifndef lint
#ifdef __GNUC__
#define GNUC_or_lint
#endif /* defined __GNUC__ */
#endif /* !defined lint */
#endif /* !defined GNUC_or_lint */

#ifndef INITIALIZE
#ifdef GNUC_or_lint
#define INITIALIZE(x)	((x) = 0)
#endif /* defined GNUC_or_lint */
#ifndef GNUC_or_lint
#define INITIALIZE(x)
#endif /* !defined GNUC_or_lint */
#endif /* !defined INITIALIZE */

/*
** For the benefit of GNU folk...
** `_(MSGID)' uses the current locale's message library string for MSGID.
** The default is to use gettext if available, and use MSGID otherwise.
*/

#ifndef _
#if HAVE_GETTEXT - 0
#define _(msgid) gettext(msgid)
#else /* !(HAVE_GETTEXT - 0) */
#define _(msgid) msgid
#endif /* !(HAVE_GETTEXT - 0) */
#endif /* !defined _ */

#ifndef TZ_DOMAIN
#define TZ_DOMAIN "tz"
#endif /* !defined TZ_DOMAIN */

/*
** UNIX was a registered trademark of The Open Group in 2003.
*/

#endif /* !defined PRIVATE_H */
