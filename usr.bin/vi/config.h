/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
/* $FreeBSD: src/usr.bin/vi/config.h,v 1.2.6.1 2000/05/06 02:31:49 jlemon Exp $ */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you have a working `mmap' system call.  */
#define HAVE_MMAP 1

/* Define if your struct stat has st_blksize.  */
#define HAVE_ST_BLKSIZE 1

/* Define if you have <vfork.h>.  */
/* #undef HAVE_VFORK_H */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef mode_t */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef pid_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define vfork as fork if vfork does not work.  */
/* #undef vfork */

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define to `int' if <sys/types.h> doesn't define.  */
/* #undef ssize_t */

/* Define if you want a debugging version. */
/* #undef DEBUG */

/* Define if you have a System V-style (broken) gettimeofday. */
/* #undef HAVE_BROKEN_GETTIMEOFDAY */

/* Define if you have a Ultrix-style (broken) vdisable. */
/* #undef HAVE_BROKEN_VDISABLE */

/* Define if you have a BSD version of curses. */
#ifndef SYSV_CURSES
#define HAVE_BSD_CURSES 1
#endif

/* Define if you have the curses(3) addnstr function. */
#define HAVE_CURSES_ADDNSTR 1

/* Define if you have the curses(3) beep function. */
#ifdef SYSV_CURSES
#define HAVE_CURSES_BEEP 1
#endif

/* Define if you have the curses(3) flash function. */
#ifdef SYSV_CURSES
#define HAVE_CURSES_FLASH 1
#endif

/* Define if you have the curses(3) idlok function. */
#define HAVE_CURSES_IDLOK 1

/* Define if you have the curses(3) keypad function. */
#ifdef SYSV_CURSES
#define HAVE_CURSES_KEYPAD 1
#endif

/* Define if you have the curses(3) newterm function. */
#ifdef SYSV_CURSES
#define HAVE_CURSES_NEWTERM 1
#endif

/* Define if you have the curses(3) setupterm function. */
#ifdef SYSV_CURSES
#define HAVE_CURSES_SETUPTERM 1
#endif

/* Define if you have the curses(3) tigetstr/tigetnum functions. */
#ifdef SYSV_CURSES
#define HAVE_CURSES_TIGETSTR 1
#endif

/* Define if you have the chsize(2) system call. */
/* #undef HAVE_FTRUNCATE_CHSIZE */

/* Define if you have the ftruncate(2) system call. */
#define HAVE_FTRUNCATE_FTRUNCATE 1

/* Define if you have fcntl(2) style locking. */
/* #undef HAVE_LOCK_FCNTL */

/* Define if you have flock(2) style locking. */
#define HAVE_LOCK_FLOCK 1

/* Define if you want to compile in the Perl interpreter. */
/* #undef HAVE_PERL_INTERP */	/* XXX: SET IN Makefile CFLAGS */

/* Define if your Perl is at least 5.003_01. */
/* #undef HAVE_PERL_5_003_01 */	/* XXX: SET IN Makefile CFLAGS */

/* Define if you have the Berkeley style revoke(2) system call. */
#define HAVE_REVOKE 1

/* Define if you have <sys/mman.h> */
#define HAVE_SYS_MMAN_H 1

/* Define if you have <sys/select.h> */
/* #undef HAVE_SYS_SELECT_H 1 */

/* Define if you have the System V style pty calls. */
/* #undef HAVE_SYS5_PTY */

/* Define if you want to compile in the Tcl interpreter. */
/* #define HAVE_TCL_INTERP */	/* XXX: SET IN Makefile CFLAGS */

/* Define if your sprintf returns a pointer, not a length. */
/* #undef SPRINTF_RET_CHARPNT */

/* Define if you have the bsearch function.  */
#define HAVE_BSEARCH 1

/* Define if you have the gethostname function.  */
#define HAVE_GETHOSTNAME 1

/* Define if you have the getopt function.  */
#define HAVE_GETOPT 1

/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1

/* Define if you have the memchr function.  */
#define HAVE_MEMCHR 1

/* Define if you have the memcpy function.  */
#define HAVE_MEMCPY 1

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

/* Define if you have the memset function.  */
#define HAVE_MEMSET 1

/* Define if you have the mkstemp function.  */
#define HAVE_MKSTEMP 1

/* Define if you have the mmap function.  */
#define HAVE_MMAP 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if you have the strpbrk function.  */
#define HAVE_STRPBRK 1

/* Define if you have the strsep function.  */
#define HAVE_STRSEP 1

/* Define if you have the strtol function.  */
#define HAVE_STRTOL 1

/* Define if you have the strtoul function.  */
#define HAVE_STRTOUL 1

/* Define if you have the unsetenv function.  */
#define HAVE_UNSETENV 1

/* Define if you have the valloc function.  */
#define HAVE_VALLOC 1

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1
