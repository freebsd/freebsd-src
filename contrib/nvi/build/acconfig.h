/* @(#)acconfig.h	8.18 (Berkeley) 7/2/96 */

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef ssize_t

/* Define if you want a debugging version. */
#undef DEBUG

/* Define if you have a System V-style (broken) gettimeofday. */
#undef HAVE_BROKEN_GETTIMEOFDAY

/* Define if you have a Ultrix-style (broken) vdisable. */
#undef HAVE_BROKEN_VDISABLE

/* Define if you have a BSD version of curses. */
#undef HAVE_BSD_CURSES

/* Define if you have the curses(3) addnstr function. */
#undef HAVE_CURSES_ADDNSTR

/* Define if you have the curses(3) beep function. */
#undef HAVE_CURSES_BEEP

/* Define if you have the curses(3) flash function. */
#undef HAVE_CURSES_FLASH

/* Define if you have the curses(3) idlok function. */
#undef HAVE_CURSES_IDLOK

/* Define if you have the curses(3) keypad function. */
#undef HAVE_CURSES_KEYPAD

/* Define if you have the curses(3) newterm function. */
#undef HAVE_CURSES_NEWTERM

/* Define if you have the curses(3) setupterm function. */
#undef HAVE_CURSES_SETUPTERM

/* Define if you have the curses(3) tigetstr/tigetnum functions. */
#undef HAVE_CURSES_TIGETSTR

/* Define if you have the DB __hash_open call in the C library. */
#undef HAVE_DB_HASH_OPEN

/* Define if you have the chsize(2) system call. */
#undef HAVE_FTRUNCATE_CHSIZE

/* Define if you have the ftruncate(2) system call. */
#undef HAVE_FTRUNCATE_FTRUNCATE

/* Define if you have fcntl(2) style locking. */
#undef HAVE_LOCK_FCNTL

/* Define if you have flock(2) style locking. */
#undef HAVE_LOCK_FLOCK

/* Define if you want to compile in the Perl interpreter. */
#undef HAVE_PERL_INTERP

/* Define if your Perl is at least 5.003_01. */
#undef HAVE_PERL_5_003_01

/* Define if you have the Berkeley style revoke(2) system call. */
#undef HAVE_REVOKE

/* Define if you have the Berkeley style strsep(3) function. */
#undef HAVE_STRSEP

/* Define if you have <sys/mman.h> */
#undef HAVE_SYS_MMAN_H

/* Define if you have <sys/select.h> */
#undef HAVE_SYS_SELECT_H

/* Define if you have the System V style pty calls. */
#undef HAVE_SYS5_PTY

/* Define if you want to compile in the Tcl interpreter. */
#undef HAVE_TCL_INTERP

/* Define if your sprintf returns a pointer, not a length. */
#undef SPRINTF_RET_CHARPNT
