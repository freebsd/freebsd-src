#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# This file is part of flex.

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:

# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.

# Neither the name of the University nor the names of its contributors
# may be used to endorse or promote products derived from this software
# without specific prior written permission.

# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE.

# autoconf requirements and initialization

AC_INIT([the fast lexical analyser generator], [2.5.37],
[flex-help@lists.sourceforge.net], [flex])
AC_CONFIG_SRCDIR([scan.l])
AM_INIT_AUTOMAKE([gnits dist-bzip2 1.10])
AC_CONFIG_HEADER([config.h:conf.in])
AC_CONFIG_LIBOBJ_DIR([lib])

# checks for programs

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.12)
AC_PROG_YACC
AM_PROG_LEX
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_LN_S
AC_PROG_RANLIB
AC_PROG_AWK
AC_PROG_INSTALL

AC_PATH_PROG(BISON, bison,bison)
AC_PATH_PROG(HELP2MAN, help2man, help2man)

# Check for a m4 that supports -P

AC_CACHE_CHECK([for m4 that supports -P], [ac_cv_path_M4],
[AC_PATH_PROGS_FEATURE_CHECK([M4], [gm4 gnum4 m4],
              [[m4out=`echo 'm''4_divnum' | $ac_path_M4 -P`]
                [test "x$m4out" = x0 \
                && ac_cv_path_M4=$ac_path_M4 ac_path_M4_found=:]],
              [AC_MSG_ERROR([could not find m4 that supports -P])])])
AC_SUBST([M4], [$ac_cv_path_M4])
AC_DEFINE_UNQUOTED([M4], ["$M4"], [Define to the m4 executable name.])

AC_PATH_PROG(INDENT, indent, indent)
# if INDENT is set to 'indent' then we didn't find indent
if test "$INDENT" != indent ; then
   AC_MSG_CHECKING(if $INDENT is GNU indent)
   if $INDENT --version 2>/dev/null | head -n 1|grep "GNU indent" > /dev/null ; then
      AC_MSG_RESULT(yes)
   else
      AC_MSG_RESULT(no)
      AC_MSG_WARN($INDENT does not appear to be GNU indent.)
   fi
else
   AC_MSG_WARN(no indent program found: make indent target will not function)
fi

# checks for headers

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([inttypes.h libintl.h limits.h locale.h malloc.h netinet/in.h regex.h stddef.h stdlib.h string.h strings.h unistd.h])

# checks for libraries

# The test test-pthread uses libpthread, so we check for it here, but
# all we need is the preprocessor symbol defined since we don't need
# LIBS to include libpthread for building flex.

AC_CHECK_LIB(pthread, pthread_mutex_lock,
AC_DEFINE([HAVE_LIBPTHREAD], 1, [pthread library] ),
AC_DEFINE([HAVE_LIBPTHREAD], 0, [pthread library] )
)
AC_CHECK_HEADERS([pthread.h])

AC_CHECK_LIB(m, log10)

# Checks for typedefs, structures, and compiler characteristics.

AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.

AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 isascii memset pow regcomp setlocale strchr strtol])

AC_CONFIG_FILES(
Makefile
doc/Makefile
examples/Makefile
examples/fastwc/Makefile
examples/manual/Makefile
lib/Makefile
po/Makefile.in
tools/Makefile
tests/Makefile
tests/TEMPLATE/Makefile
tests/test-array-nr/Makefile
tests/test-array-r/Makefile
tests/test-basic-nr/Makefile
tests/test-basic-r/Makefile
tests/test-bison-yylloc/Makefile
tests/test-bison-yylval/Makefile
tests/test-c-cpp-nr/Makefile
tests/test-c-cpp-r/Makefile
tests/test-header-nr/Makefile
tests/test-header-r/Makefile
tests/test-include-by-buffer/Makefile
tests/test-include-by-push/Makefile
tests/test-include-by-reentrant/Makefile
tests/test-multiple-scanners-nr/Makefile
tests/test-multiple-scanners-r/Makefile
tests/test-noansi-nr/Makefile
tests/test-noansi-r/Makefile
tests/test-prefix-nr/Makefile
tests/test-prefix-r/Makefile
tests/test-pthread/Makefile
tests/test-string-nr/Makefile
tests/test-string-r/Makefile
tests/test-yyextra/Makefile
tests/test-alloc-extra/Makefile
tests/test-lineno-nr/Makefile
tests/test-lineno-r/Makefile
tests/test-linedir-r/Makefile
tests/test-debug-r/Makefile
tests/test-debug-nr/Makefile
tests/test-mem-nr/Makefile
tests/test-mem-r/Makefile
tests/test-posix/Makefile
tests/test-posixly-correct/Makefile
tests/test-table-opts/Makefile
tests/test-c++-basic/Makefile
tests/test-bison-nr/Makefile
tests/test-reject/Makefile
tests/test-c++-multiple-scanners/Makefile
tests/test-top/Makefile
tests/test-rescan-nr/Makefile
tests/test-rescan-r/Makefile
tests/test-quotes/Makefile
tests/test-ccl/Makefile
tests/test-extended/Makefile
tests/test-c++-yywrap/Makefile
tests/test-concatenated-options/Makefile
dnl --new-test-here-- This line is processed by tests/create-test.
)

AC_OUTPUT
