/*
 * config.h
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/19 18:56:28
 *
 * @(#) mytinfo config.h 3.3 92/02/19 public domain, By Ross Ridge
 *
 * Read the file INSTALL for more information on configuring mytinfo
 *
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifdef __STDC__
#define USE_ANSIC		/* undefine this if your compiler lies */
#endif

#define USE_TERMIOS             /* use POSIX termios */
#undef  USE_TERMIO              /* use termio (SysIII, SysV) */
#undef USE_SGTTY		/* use sgtty (v7, BSD) */
#define USE_WINSZ		/* get window size from the tty driver */
#undef USE_STRINGS		/* include <strings.h> instead of <string.h> */
#undef  USE_MYBSEARCH           /* your library doesn't have bsearch */
#undef  USE_MYSTRTOK            /* your library doesn't have strtok */
#undef  USE_MYQSORT             /* your library doesn't have qsort */
#undef  USE_MYMKDIR             /* your library doesn't have mkdir */
#define USE_MEMORY		/* you have an <memory.h> header */
#undef  USE_FAKE_STDIO          /* don't use real stdio */
#undef USE_DOPRNT		/* no vfprintf, use _doprnt */
#undef USE_TERMINFO             /* look in terminfo dirs for entry */
#define USE_TERMCAP             /* look in termcap dirs for entry */

#undef  USE_SHORT_BSEARCH       /* speeds up MYBSEARCH on most machines */

#undef  USE_SMALLMEM            /* save some memory */

#undef USE_UPBC_KLUDGE		/* do tgoto like real togo */
#undef USE_EXTERN_UPBC		/* get cuu1 and cub1 from externs UP and BC */
#undef USE_LITOUT_KLUDGE	/* an alternate tgoto kludge, not recommened */


#ifndef USE_ANSIC

#undef USE_PROTOTYPES		/* use ANSI C prototypes */
#undef USE_STDLIB		/* you have a <stdlib.h> */
#undef USE_STDARG		/* you have a <stdarg.h> */
#undef USE_STDDEF		/* you have a <stddef.h> */

#define const
#define volatile
#define noreturn		/* a function that doesn't return */

typedef char *anyptr;		/* a type that any pointer can be assigned to */

#define mysize_t unsigned	/* size_t, the size of an object */

#else /* USE_ANSIC */

#define USE_PROTOTYPES
#define USE_STDLIB
#define USE_STDARG
#define USE_STDDEF

typedef void *anyptr;

#define mysize_t size_t

#ifdef __GNUC__
#define noreturn volatile
#else
#define noreturn
#endif

#endif /* USE_ANSIC */

#ifdef __FreeBSD__
#define TERMCAPFILE "$TERMCAPFILE $HOME/.termcap /usr/share/misc/termcap"
#else
#define TERMCAPFILE "$TERMCAPFILE $HOME/.termcap /etc/termcap"
#endif

#define TERMINFOSRC "/usr/lib/terminfo/terminfo.src"

#define TERMINFODIR "/usr/lib/terminfo"

#endif
