dnl $Id: largefile.m4 13768 2004-04-24 21:51:32Z joda $
dnl
dnl Figure out what flags we need for 64-bit file access, and also set
dnl them on the command line.
dnl
AC_DEFUN([rk_SYS_LARGEFILE],[
AC_REQUIRE([AC_SYS_LARGEFILE])dnl
dnl need to set this on the command line, since it might otherwise break
dnl with generated code, such as lex
if test "$enable_largefile" != no -a "$ac_cv_sys_large_files" != no; then
	CPPFLAGS="$CPPFLAGS -D_LARGE_FILES=$ac_cv_sys_large_files"
fi
if test "$enable_largefile" != no -a "$ac_cv_sys_file_offset_bits" != no; then
	CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits"
fi
])
