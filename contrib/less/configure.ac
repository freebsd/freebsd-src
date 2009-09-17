# Process this file with autoconf to produce a configure script.

# Copyright (C) 1984-2009  Mark Nudelman
#
# You may distribute under the terms of either the GNU General Public
# License or the Less License, as specified in the README file.
#
# For more information about less, or for information on how to 
# contact the author, see the README file.

# Autoconf initialization.
AC_INIT(less, 1)
AC_CONFIG_SRCDIR([forwback.c])
AC_CONFIG_HEADER([defines.h])

# Checks for programs.
AC_PROG_CC
AC_ISC_POSIX
AC_PROG_GCC_TRADITIONAL
AC_PROG_INSTALL

# Checks for compilation model.
AC_SYS_LARGEFILE

# Checks for general libraries.
AC_CHECK_LIB(tinfo, tgoto, [have_tinfo=yes], [have_tinfo=no])
AC_CHECK_LIB(xcurses, initscr, [have_xcurses=yes], [have_xcurses=no])
AC_CHECK_LIB(ncursesw, initscr, [have_ncursesw=yes], [have_ncursesw=no])
AC_CHECK_LIB(ncurses, initscr, [have_ncurses=yes], [have_ncurses=no])
AC_CHECK_LIB(curses, initscr, [have_curses=yes], [have_curses=no])
AC_CHECK_LIB(termcap, tgetent, [have_termcap=yes], [have_termcap=no])
AC_CHECK_LIB(termlib, tgetent, [have_termlib=yes], [have_termlib=no])
# Regular expressions (regcmp) are in -lgen on Solaris 2,
# and in -lintl on SCO Unix.
AC_CHECK_LIB(gen, regcmp)
AC_CHECK_LIB(intl, regcmp)
AC_CHECK_LIB(PW, regcmp)

# Checks for terminal libraries
AC_MSG_CHECKING([for working terminal libraries])
TERMLIBS=

# Check for systems where curses is broken.
curses_broken=0
if test x`uname -s` = "xHP-UX" >/dev/null 2>&1; then
if test x`uname -r` = "xB.11.00" >/dev/null 2>&1; then
   curses_broken=1
fi
if test x`uname -r` = "xB.11.11" >/dev/null 2>&1; then
   curses_broken=1
fi
fi

if test $curses_broken = 0; then

# -- Try tinfo.
if test "x$TERMLIBS" = x; then
  if test $have_tinfo = yes; then
    TERMLIBS="-ltinfo"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
fi

# -- Try xcurses.
if test "x$TERMLIBS" = x; then
  if test $have_xcurses = yes; then
    TERMLIBS="-lxcurses"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
fi

# -- Try ncursesw.
if test "x$TERMLIBS" = x; then
  if test $have_ncursesw = yes; then
    TERMLIBS="-lncursesw"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
fi

# -- Try ncurses.
if test "x$TERMLIBS" = x; then
  if test $have_ncurses = yes; then
    TERMLIBS="-lncurses"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
fi

# -- Try curses.
if test "x$TERMLIBS" = x; then
  if test $have_curses = yes; then
    TERMLIBS="-lcurses"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
fi

# -- Try curses & termcap.
if test "x$TERMLIBS" = x; then
  if test $have_curses = yes; then
  if test $have_termcap = yes; then
    TERMLIBS="-lcurses -ltermcap"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
  fi
fi
fi

# -- Try termcap.
if test "x$TERMLIBS" = x; then
  if test $have_termcap = yes; then
    TERMLIBS="-ltermcap"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
fi

# -- Try termlib.
if test "x$TERMLIBS" = x; then
  if test $have_termlib = yes; then
    TERMLIBS="-lcurses -ltermlib"
    SAVE_LIBS=$LIBS
    LIBS="$LIBS $TERMLIBS"
    AC_TRY_LINK(, [tgetent(0,0); tgetflag(0); tgetnum(0); tgetstr(0,0);],
      [termok=yes], [termok=no])
    LIBS=$SAVE_LIBS
    if test $termok = no; then TERMLIBS=""; fi
  fi
fi

if test "x$TERMLIBS" = x; then
  AC_MSG_RESULT(Cannot find terminal libraries - configure failed)
  exit 1
fi
AC_MSG_RESULT(using $TERMLIBS)
LIBS="$LIBS $TERMLIBS"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h limits.h stdio.h stdlib.h string.h termcap.h termio.h termios.h time.h unistd.h values.h sys/ioctl.h sys/stream.h wctype.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STAT
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Autoheader templates for symbols defined later by AC_DEFINE.
AH_TEMPLATE([HAVE_POSIX_REGCOMP],
	[POSIX regcomp() and regex.h])
AH_TEMPLATE([HAVE_PCRE],
	[PCRE (Perl-compatible regular expression) library])
AH_TEMPLATE([HAVE_RE_COMP],
	[BSD re_comp()])
AH_TEMPLATE([HAVE_REGCMP],
	[System V regcmp()])
AH_TEMPLATE([HAVE_V8_REGCOMP],
	[Henry Spencer V8 regcomp() and regexp.h])
AH_TEMPLATE([NO_REGEX],
	[pattern matching is supported, but without metacharacters.])
AH_TEMPLATE([HAVE_REGEXEC2],
	[])
AH_TEMPLATE([HAVE_VOID],
	[Define HAVE_VOID if your compiler supports the "void" type.])
AH_TEMPLATE([HAVE_CONST],
	[Define HAVE_CONST if your compiler supports the "const" modifier.])
AH_TEMPLATE([HAVE_STAT_INO],
	[Define HAVE_STAT_INO if your struct stat has st_ino and st_dev.])
AH_TEMPLATE([HAVE_TIME_T],
	[Define HAVE_TIME_T if your system supports the "time_t" type.])
AH_TEMPLATE([HAVE_STRERROR],
	[Define HAVE_STRERROR if you have the strerror() function.])
AH_TEMPLATE([HAVE_FILENO],
	[Define HAVE_FILENO if you have the fileno() macro.])
AH_TEMPLATE([HAVE_ERRNO],
	[Define HAVE_ERRNO if you have the errno variable.])
AH_TEMPLATE([MUST_DEFINE_ERRNO],
	[Define MUST_DEFINE_ERRNO if you have errno but it is not define in errno.h.])
AH_TEMPLATE([HAVE_SYS_ERRLIST],
	[Define HAVE_SYS_ERRLIST if you have the sys_errlist[] variable.])
AH_TEMPLATE([HAVE_OSPEED],
	[Define HAVE_OSPEED if your termcap library has the ospeed variable.])
AH_TEMPLATE([MUST_DEFINE_OSPEED],
	[Define MUST_DEFINE_OSPEED if you have ospeed but it is not defined in termcap.h.])
AH_TEMPLATE([HAVE_LOCALE],
	[Define HAVE_LOCALE if you have locale.h and setlocale.])
AH_TEMPLATE([HAVE_TERMIOS_FUNCS],
	[Define HAVE_TERMIOS_FUNCS if you have tcgetattr/tcsetattr.])
AH_TEMPLATE([HAVE_UPPER_LOWER],
	[Define HAVE_UPPER_LOWER if you have isupper, islower, toupper, tolower.])
AH_TEMPLATE([HAVE_WCTYPE],
	[Define HAVE_WCTYPE if you have iswupper, iswlower, towupper, towlower.])
AH_TEMPLATE([HAVE_SIGSET_T],
	[Define HAVE_SIGSET_T you have the sigset_t type.])
AH_TEMPLATE([HAVE_SIGEMPTYSET],
	[Define HAVE_SIGEMPTYSET if you have the sigemptyset macro.])
AH_TEMPLATE([EDIT_PGM],
	[Define EDIT_PGM to your editor.])
AH_TEMPLATE([SECURE_COMPILE],
	[Define SECURE_COMPILE=1 to build a secure version of less.])

# Checks for identifiers.
AC_TYPE_OFF_T
AC_MSG_CHECKING(for void)
AC_TRY_COMPILE(, [void *foo = 0;], 
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_VOID)], [AC_MSG_RESULT(no)])
AC_MSG_CHECKING(for const)
AC_TRY_COMPILE(, [const int foo = 0;], 
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_CONST)], [AC_MSG_RESULT(no)])
AC_MSG_CHECKING(for time_t)
AC_TRY_COMPILE([#include <time.h>], [time_t t = 0;],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_TIME_T)], [AC_MSG_RESULT(no)])
AC_MSG_CHECKING(for st_ino in struct stat)
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/stat.h>],
  [struct stat s; dev_t dev = s.st_dev; ino_t ino = s.st_ino;],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_STAT_INO)], [AC_MSG_RESULT(no)])

# Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([fsync popen _setjmp sigprocmask sigsetmask snprintf stat system fchmod])

# AC_CHECK_FUNCS may not work for inline functions, so test these separately.
AC_MSG_CHECKING(for memcpy)
AC_TRY_LINK([
#if HAVE_STRING_H
#include <string.h>
#endif], [memcpy(0,0,0);],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MEMCPY)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for strchr)
AC_TRY_LINK([
#if HAVE_STRING_H
#include <string.h>
#endif], [strchr("x",'x');],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_STRCHR)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for strstr)
AC_TRY_LINK([
#if HAVE_STRING_H
#include <string.h>
#endif], [strstr("x","x");],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_STRSTR)], [AC_MSG_RESULT(no)])

# Some systems have termios.h but not the corresponding functions.
AC_CHECK_FUNC(tcgetattr, AC_DEFINE(HAVE_TERMIOS_FUNCS))

AC_MSG_CHECKING(for fileno)
AC_TRY_LINK([
#if HAVE_STDIO_H
#include <stdio.h>
#endif], [static int x; x = fileno(stdin);],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_FILENO)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for strerror)
AC_TRY_LINK([
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif], [static char *x; x = strerror(0);],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_STRERROR)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for sys_errlist)
AC_TRY_LINK(, [extern char *sys_errlist[]; static char **x; x = sys_errlist;], 
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_SYS_ERRLIST)], [AC_MSG_RESULT(no)])

AC_CHECK_TYPES([sigset_t],,,[#include <signal.h>])

AC_MSG_CHECKING(for sigemptyset)
AC_TRY_LINK([
#include <signal.h>
], [sigset_t s; sigemptyset(&s);],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_SIGEMPTYSET)], [AC_MSG_RESULT(no)])

have_errno=no
AC_MSG_CHECKING(for errno)
AC_TRY_LINK([
#if HAVE_ERRNO_H
#include <errno.h>
#endif], [static int x; x = errno;], 
  [AC_MSG_RESULT(yes - in errno.h); AC_DEFINE(HAVE_ERRNO) have_errno=yes])
if test $have_errno = no; then
AC_TRY_LINK([
#if HAVE_ERRNO_H
#include <errno.h>
#endif], [extern int errno; static int x; x = errno;], 
  [AC_MSG_RESULT(yes - must define); AC_DEFINE(HAVE_ERRNO) AC_DEFINE(MUST_DEFINE_ERRNO)],
  [AC_MSG_RESULT(no)])
fi

AC_MSG_CHECKING(for locale)
AC_TRY_LINK([#include <locale.h>
#include <ctype.h>
#include <langinfo.h>], [setlocale(LC_CTYPE,""); isprint(0); iscntrl(0);],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_LOCALE)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for ctype functions)
AC_TRY_LINK([
#if HAVE_CTYPE_H
#include <ctype.h>
#endif], [static int x; x = isupper(x); x = tolower(x); x = toupper(x);],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_UPPER_LOWER)], [AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for wctype functions)
AC_TRY_LINK([#include <wctype.h>], [iswlower(0); iswupper(0); towlower(0); towupper(0);],
  [AC_MSG_RESULT(yes); AC_DEFINE(HAVE_WCTYPE)], [AC_MSG_RESULT(no)])

# Checks for external variable ospeed in the termcap library.
have_ospeed=no
AC_MSG_CHECKING(termcap for ospeed)
AC_TRY_LINK([
#include <sys/types.h>
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#if HAVE_TERMCAP_H
#include <termcap.h>
#endif], [ospeed = 0;],
[AC_MSG_RESULT(yes - in termcap.h); AC_DEFINE(HAVE_OSPEED) have_ospeed=yes])
if test $have_ospeed = no; then
AC_TRY_LINK(, [extern short ospeed; ospeed = 0;], 
  [AC_MSG_RESULT(yes - must define); AC_DEFINE(HAVE_OSPEED) AC_DEFINE(MUST_DEFINE_OSPEED)],
  [AC_MSG_RESULT(no)])
fi

# Compile in secure mode?
AC_ARG_WITH(secure,
  [  --with-secure  Compile in secure mode],
  AC_DEFINE(SECURE_COMPILE, 1), AC_DEFINE(SECURE_COMPILE, 0))

# Checks for regular expression functions.
have_regex=no
have_posix_regex=unknown
AC_MSG_CHECKING(for regcomp)

# Select a regular expression library.
WANT_REGEX=auto
AC_ARG_WITH(regex,
  [  --with-regex={auto,pcre,posix,regcmp,re_comp,regcomp,regcomp-local}  Select a regular expression library  [auto]],
  WANT_REGEX="$withval")

if test $have_regex = no; then
if test $WANT_REGEX = auto -o $WANT_REGEX = posix; then
# Some versions of Solaris have a regcomp() function, but it doesn't work!
# So we run a test program.  If we're cross-compiling, do it the old way.
AC_TRY_RUN([
#include <sys/types.h>
#include <regex.h>
main() { regex_t r; regmatch_t rm; char *text = "xabcy";
if (regcomp(&r, "abc", 0)) exit(1);
if (regexec(&r, text, 1, &rm, 0)) exit(1);
#ifndef __WATCOMC__
if (rm.rm_so != 1) exit(1); /* check for correct offset */
#else
if (rm.rm_sp != text + 1) exit(1); /* check for correct offset */
#endif
exit(0); }],
  have_posix_regex=yes, have_posix_regex=no, have_posix_regex=unknown)
if test $have_posix_regex = yes; then
  AC_MSG_RESULT(using POSIX regcomp)
  AC_DEFINE(HAVE_POSIX_REGCOMP)
  have_regex=yes
elif test $have_posix_regex = unknown; then
  AC_TRY_LINK([
#include <sys/types.h>
#include <regex.h>],
  [regex_t *r; regfree(r);],
  AC_MSG_RESULT(using POSIX regcomp)
  AC_DEFINE(HAVE_POSIX_REGCOMP) have_regex=yes)
else
  AC_MSG_RESULT(no)
fi
fi
fi

if test $have_regex = no; then
if test $WANT_REGEX = auto -o $WANT_REGEX = pcre; then
AC_CHECK_LIB(pcre, pcre_compile, 
[AC_MSG_RESULT(using pcre); AC_DEFINE(HAVE_PCRE) LIBS="$LIBS -lpcre" have_regex=yes], [])
fi
fi

if test $have_regex = no; then
if test $WANT_REGEX = auto -o $WANT_REGEX = regcmp; then
AC_CHECK_FUNC(regcmp, 
AC_MSG_RESULT(using regcmp); AC_DEFINE(HAVE_REGCMP) have_regex=yes)
fi
fi

if test $have_regex = no; then
if test $WANT_REGEX = auto -o $WANT_REGEX = regcomp; then
AC_TRY_LINK([
#include "regexp.h"], [regcomp("");],
AC_MSG_RESULT(using V8 regcomp); AC_DEFINE(HAVE_V8_REGCOMP) have_regex=yes)
fi
fi

if test $have_regex = no && test -f ${srcdir}/regexp.c; then
if test $WANT_REGEX = auto -o $WANT_REGEX = regcomp-local; then
AC_MSG_RESULT(using V8 regcomp -- local source); AC_DEFINE(HAVE_V8_REGCOMP) AC_DEFINE(HAVE_REGEXEC2) REGEX_O='regexp.$(O)' AC_SUBST(REGEX_O) have_regex=yes
fi
fi

if test $have_regex = no; then
if test $WANT_REGEX = auto -o $WANT_REGEX = re_comp; then
AC_MSG_RESULT(using re_comp); AC_CHECK_FUNC(re_comp, AC_DEFINE(HAVE_RE_COMP) have_regex=yes)
fi
fi

if test $have_regex = no; then
AC_MSG_RESULT(cannot find regular expression library); AC_DEFINE(NO_REGEX)
fi

AC_ARG_WITH(editor,
  [  --with-editor=PROGRAM   use PROGRAM as the default editor [vi]],
  AC_DEFINE_UNQUOTED(EDIT_PGM, "$withval"), AC_DEFINE(EDIT_PGM, "vi"))

AH_TOP([
/* Unix definition file for less.  -*- C -*-
 *
 * This file has 3 sections:
 * User preferences.
 * Settings always true on Unix.
 * Settings automatically determined by configure.
 *
 * * * * * *  WARNING  * * * * * *
 * If you edit defines.h by hand, do "touch stamp-h" before you run make
 * so config.status doesn't overwrite your changes.
 */

/* User preferences.  */

/*
 * SECURE is 1 if you wish to disable a bunch of features in order to
 * be safe to run by unprivileged users.
 * SECURE_COMPILE is set by the --with-secure configure option.
 */
#define	SECURE		SECURE_COMPILE

/*
 * SHELL_ESCAPE is 1 if you wish to allow shell escapes.
 * (This is possible only if your system supplies the system() function.)
 */
#define	SHELL_ESCAPE	(!SECURE)

/*
 * EXAMINE is 1 if you wish to allow examining files by name from within less.
 */
#define	EXAMINE		(!SECURE)

/*
 * TAB_COMPLETE_FILENAME is 1 if you wish to allow the TAB key
 * to complete filenames at prompts.
 */
#define	TAB_COMPLETE_FILENAME	(!SECURE)

/*
 * CMD_HISTORY is 1 if you wish to allow keys to cycle through
 * previous commands at prompts.
 */
#define	CMD_HISTORY	1

/*
 * HILITE_SEARCH is 1 if you wish to have search targets to be 
 * displayed in standout mode.
 */
#define	HILITE_SEARCH	1

/*
 * EDITOR is 1 if you wish to allow editor invocation (the "v" command).
 * (This is possible only if your system supplies the system() function.)
 * EDIT_PGM is the name of the (default) editor to be invoked.
 */
#define	EDITOR		(!SECURE)

/*
 * TAGS is 1 if you wish to support tag files.
 */
#define	TAGS		(!SECURE)

/*
 * USERFILE is 1 if you wish to allow a .less file to specify 
 * user-defined key bindings.
 */
#define	USERFILE	(!SECURE)

/*
 * GLOB is 1 if you wish to have shell metacharacters expanded in filenames.
 * This will generally work if your system provides the "popen" function
 * and the "echo" shell command.
 */
#define	GLOB		(!SECURE)

/*
 * PIPEC is 1 if you wish to have the "|" command
 * which allows the user to pipe data into a shell command.
 */
#define	PIPEC		(!SECURE)

/*
 * LOGFILE is 1 if you wish to allow the -l option (to create log files).
 */
#define	LOGFILE		(!SECURE)

/*
 * GNU_OPTIONS is 1 if you wish to support the GNU-style command
 * line options --help and --version.
 */
#define	GNU_OPTIONS	1

/*
 * ONLY_RETURN is 1 if you want RETURN to be the only input which
 * will continue past an error message.
 * Otherwise, any key will continue past an error message.
 */
#define	ONLY_RETURN	0

/*
 * LESSKEYFILE is the filename of the default lesskey output file 
 * (in the HOME directory).
 * LESSKEYFILE_SYS is the filename of the system-wide lesskey output file.
 * DEF_LESSKEYINFILE is the filename of the default lesskey input 
 * (in the HOME directory).
 * LESSHISTFILE is the filename of the history file
 * (in the HOME directory).
 */
#define	LESSKEYFILE		".less"
#define	LESSKEYFILE_SYS		SYSDIR "/sysless"
#define	DEF_LESSKEYINFILE	".lesskey"
#define LESSHISTFILE		".lesshst"


/* Settings always true on Unix.  */

/*
 * Define MSDOS_COMPILER if compiling under Microsoft C.
 */
#define	MSDOS_COMPILER	0

/*
 * Pathname separator character.
 */
#define	PATHNAME_SEP	"/"

/*
 * The value returned from tgetent on success.
 * Some HP-UX systems return 0 on success.
 */
#define TGETENT_OK  1

/*
 * HAVE_SYS_TYPES_H is 1 if your system has <sys/types.h>.
 */
#define HAVE_SYS_TYPES_H	1

/*
 * Define if you have the <sgstat.h> header file.
 */
#undef HAVE_SGSTAT_H

/*
 * HAVE_PERROR is 1 if your system has the perror() call.
 * (Actually, if it has sys_errlist, sys_nerr and errno.)
 */
#define	HAVE_PERROR	1

/*
 * HAVE_TIME is 1 if your system has the time() call.
 */
#define	HAVE_TIME	1

/*
 * HAVE_SHELL is 1 if your system supports a SHELL command interpreter.
 */
#define	HAVE_SHELL	1

/*
 * Default shell metacharacters and meta-escape character.
 */
#define	DEF_METACHARS	"; *?\t\n'\"()<>[]|&^`#\\$%=~"
#define	DEF_METAESCAPE	"\\"

/* 
 * HAVE_DUP is 1 if your system has the dup() call.
 */
#define	HAVE_DUP	1

/* Define to 1 if you have the memcpy() function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the strchr() function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the strstr() function. */
#define HAVE_STRSTR 1

/*
 * Sizes of various buffers.
 */
#define	CMDBUF_SIZE	512	/* Buffer for multichar commands */
#define	UNGOT_SIZE	100	/* Max chars to unget() */
#define	LINEBUF_SIZE	1024	/* Max size of line in input file */
#define	OUTBUF_SIZE	1024	/* Output buffer */
#define	PROMPT_SIZE	200	/* Max size of prompt string */
#define	TERMBUF_SIZE	2048	/* Termcap buffer for tgetent */
#define	TERMSBUF_SIZE	1024	/* Buffer to hold termcap strings */
#define	TAGLINE_SIZE	512	/* Max size of line in tags file */
#define	TABSTOP_MAX	32	/* Max number of custom tab stops */

/* Settings automatically determined by configure.  */
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
