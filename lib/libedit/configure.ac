#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# Compile with debug symbols:
#    CFLAGS="-ggdb -pedandic -O0" ./configure
#    CFLAGS="-ggdb -Wall -Wextra -pedantic -O0" ./configure
#
# Verbose output can be enabled with
#    "./configure --disable-silent-rules" or "make V=1"
#

AC_PREREQ(2.61)
AC_INIT(libedit, [EL_RELEASE],, libedit-[EL_TIMESTAMP])
AC_CONFIG_SRCDIR([src/strlcat.c])
AC_CONFIG_HEADER([config.h])

# features of Posix that are extensions to C (define _GNU_SOURCE)
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE
AC_PROG_LIBTOOL

# libtool -version-info
AC_SUBST(LT_VERSION, [0:36:0])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC_C99
#AC_PROG_CC
AC_PROG_LN_S
AC_PROG_AWK
EL_MANTYPE


AC_CHECK_LIB(curses, tgetent,,
   [AC_CHECK_LIB(ncurses, tgetent,,
      [AC_MSG_ERROR([libcurses or libncurses are required!])] )] )


### use option --enable-widec to turn on use of wide-character support
EL_ENABLE_WIDEC

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h stdlib.h string.h sys/ioctl.h sys/param.h unistd.h curses.h ncurses.h sys/cdefs.h])

AC_CHECK_HEADER([termios.h], [], [AC_MSG_ERROR([termios.h is required!])],[])

## include curses.h to prevent "Present But Cannot Be Compiled"
AC_CHECK_HEADERS([term.h],,,
[[#if HAVE_CURSES_H
# include <curses.h>
#elif HAVE_NCURSES_H
# include <ncurses.h>
#endif
]])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([u_int32_t])

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
## _AIX is offended by rpl_malloc and rpl_realloc
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([endpwent isascii memchr memset re_comp regcomp strcasecmp strchr strcspn strdup strerror strrchr strstr strtol issetugid wcsdup])
EL_GETPW_R_POSIX
EL_GETPW_R_DRAFT


AH_BOTTOM([
#include "sys.h"
#define SCCSID
#undef LIBC_SCCS
#define lint
])

AC_CONFIG_FILES([Makefile
                 src/Makefile
])
AC_OUTPUT
