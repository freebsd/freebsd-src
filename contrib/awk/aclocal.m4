dnl
dnl aclocal.m4 --- autoconf input file for gawk
dnl 
dnl Copyright (C) 1995, 1996, 1998, 1999, 2000 the Free Software Foundation, Inc.
dnl 
dnl This file is part of GAWK, the GNU implementation of the
dnl AWK Progamming Language.
dnl 
dnl GAWK is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl 
dnl GAWK is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
dnl

dnl gawk-specific macros for autoconf. one day hopefully part of autoconf

AC_DEFUN(GAWK_AC_C_STRINGIZE, [
AC_REQUIRE([AC_PROG_CPP])
AC_MSG_CHECKING([for ANSI stringizing capability])
AC_CACHE_VAL(gawk_cv_c_stringize, 
AC_EGREP_CPP([#teststring],[
#define x(y) #y

char *s = x(teststring);
], gawk_cv_c_stringize=no, gawk_cv_c_stringize=yes))
if test "${gawk_cv_c_stringize}" = yes
then
	AC_DEFINE(HAVE_STRINGIZE)
fi
AC_MSG_RESULT([${gawk_cv_c_stringize}])
])dnl


dnl By default, many hosts won't let programs access large files;
dnl one must use special compiler options to get large-file access to work.
dnl For more details about this brain damage please see:
dnl http://www.sas.com/standards/large.file/x_open.20Mar96.html

dnl Written by Paul Eggert <eggert@twinsun.com>.

dnl Internal subroutine of GAWK_AC_SYS_LARGEFILE.
dnl GAWK_AC_SYS_LARGEFILE_TEST_INCLUDES
AC_DEFUN(GAWK_AC_SYS_LARGEFILE_TEST_INCLUDES,
  [[#include <sys/types.h>
    int a[(off_t) 9223372036854775807 == 9223372036854775807 ? 1 : -1];
  ]])

dnl Internal subroutine of GAWK_AC_SYS_LARGEFILE.
dnl GAWK_AC_SYS_LARGEFILE_MACRO_VALUE(C-MACRO, VALUE, CACHE-VAR, COMMENT, INCLUDES, FUNCTION-BODY)
AC_DEFUN(GAWK_AC_SYS_LARGEFILE_MACRO_VALUE,
  [AC_CACHE_CHECK([for $1 value needed for large files], $3,
     [$3=no
      AC_TRY_COMPILE(GAWK_AC_SYS_LARGEFILE_TEST_INCLUDES
$5
        ,
	[$6], 
	,
	[AC_TRY_COMPILE([#define $1 $2]
GAWK_AC_SYS_LARGEFILE_TEST_INCLUDES
$5
	   ,
	   [$6],
	   [$3=$2])])])
   if test "[$]$3" != no; then
     AC_DEFINE_UNQUOTED([$1], [$]$3, [$4])
   fi])

AC_DEFUN(GAWK_AC_SYS_LARGEFILE,
  [AC_ARG_ENABLE(largefile,
     [  --disable-largefile     omit support for large files])
   if test "$enable_largefile" != no; then

     AC_CACHE_CHECK([for special C compiler options needed for large files],
       gawk_cv_sys_largefile_CC,
       [gawk_cv_sys_largefile_CC=no
        if test "$GCC" != yes; then
	  # IRIX 6.2 and later do not support large files by default,
	  # so use the C compiler's -n32 option if that helps.
	  AC_TRY_COMPILE(GAWK_AC_SYS_LARGEFILE_TEST_INCLUDES, , ,
	    [ac_save_CC="$CC"
	     CC="$CC -n32"
	     AC_TRY_COMPILE(GAWK_AC_SYS_LARGEFILE_TEST_INCLUDES, ,
	       gawk_cv_sys_largefile_CC=' -n32')
	     CC="$ac_save_CC"])
        fi])
     if test "$gawk_cv_sys_largefile_CC" != no; then
       CC="$CC$gawk_cv_sys_largefile_CC"
     fi

     GAWK_AC_SYS_LARGEFILE_MACRO_VALUE(_FILE_OFFSET_BITS, 64,
       gawk_cv_sys_file_offset_bits,
       [Number of bits in a file offset, on hosts where this is settable.])
     GAWK_AC_SYS_LARGEFILE_MACRO_VALUE(_LARGEFILE_SOURCE, 1,
       gawk_cv_sys_largefile_source,
       [Define to make ftello visible on some hosts (e.g. HP-UX 10.20).],
       [#include <stdio.h>], [return !ftello;])
     GAWK_AC_SYS_LARGEFILE_MACRO_VALUE(_LARGE_FILES, 1,
       gawk_cv_sys_large_files,
       [Define for large files, on AIX-style hosts.])
     GAWK_AC_SYS_LARGEFILE_MACRO_VALUE(_XOPEN_SOURCE, 500,
       gawk_cv_sys_xopen_source,
       [Define to make ftello visible on some hosts (e.g. glibc 2.1.3).],
       [#include <stdio.h>], [return !ftello;])
   fi
  ])

dnl Check for AIX and add _XOPEN_SOURCE_EXTENDED
AC_DEFUN(GAWK_AC_AIX_TWEAK, [
AC_MSG_CHECKING([for AIX compilation hacks])
AC_CACHE_VAL(gawk_cv_aix_hack, [
if test -d /lpp/bos
then
	CFLAGS="$CFLAGS -D_XOPEN_SOURCE_EXTENDED=1"
	gawk_cv_aix_hack=yes
else
	gawk_cv_aix_hack=no
fi
])dnl
AC_MSG_RESULT([${gawk_cv_aix_hack}])
])dnl
