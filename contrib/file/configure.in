dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([src/file.c])
AM_INIT_AUTOMAKE(file, 4.12)
AM_CONFIG_HEADER([config.h])
dnl AM_MAINTAINER_MODE

AC_MSG_CHECKING(for builtin ELF support)
AC_ARG_ENABLE(elf,
[  --disable-elf            disable builtin ELF support],
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(BUILTIN_ELF)
else
  AC_MSG_RESULT(no)
fi], [
  # enable by default
  AC_MSG_RESULT(yes)
  AC_DEFINE(BUILTIN_ELF)
])

AC_MSG_CHECKING(for ELF core file support)
AC_ARG_ENABLE(elf-core,
[  --disable-elf-core       disable ELF core file support],
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(ELFCORE)
else
  AC_MSG_RESULT(no)
fi], [
  # enable by default
  AC_MSG_RESULT(yes)
  AC_DEFINE(ELFCORE)
])

AC_MSG_CHECKING(for file formats in man section 5)
AC_ARG_ENABLE(fsect-man5,
[  --enable-fsect-man5      enable file formats in man section 5],
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  fsect=5
else
  AC_MSG_RESULT(no)
  fsect=4
fi], [
  # disable by default
  AC_MSG_RESULT(no)
  fsect=4
])
AC_SUBST(fsect)
AM_CONDITIONAL(FSECT5, test x$fsect = x5)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

dnl Templates for autoheader
AH_TEMPLATE([BUILTIN_ELF],
	    [Use the builtin ELF recognition code])
AH_TEMPLATE([ELFCORE],
	    [Recognize ELF core files])
AH_TEMPLATE([HAVE_DAYLIGHT], [])
AH_TEMPLATE([HAVE_LONG_LONG], [])
AH_TEMPLATE([HAVE_TM_ISDST], [])
AH_TEMPLATE([SIZEOF_UINT16_T], [])
AH_TEMPLATE([SIZEOF_UINT32_T], [])
AH_TEMPLATE([SIZEOF_UINT64_T], [])
AH_TEMPLATE([SIZEOF_UINT8_T], [])
AH_TEMPLATE([int32_t], [])
AH_TEMPLATE([uint16_t], [])
AH_TEMPLATE([uint32_t], [])
AH_TEMPLATE([uint64_t], [])
AH_TEMPLATE([uint8_t], [])

dnl Checks for headers
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_SYS_WAIT
AC_HEADER_STDINT
AC_CHECK_HEADERS(fcntl.h locale.h)
AC_CHECK_HEADERS(sys/mman.h sys/stat.h sys/types.h)
AC_CHECK_HEADERS(stdint.h inttypes.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(getopt.h)
AC_CHECK_HEADERS(locale.h)
AC_CHECK_HEADERS(utime.h)
AC_CHECK_HEADERS(sys/utime.h)
AC_CHECK_HEADERS(wchar.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_DIAGNOSE([obsolete],[AC_STRUCT_ST_RDEV:
	your code should no longer depend upon `HAVE_ST_RDEV', but
	`HAVE_STRUCT_STAT_ST_RDEV'.  Remove this warning and
	the `AC_DEFINE' when you adjust the code.])
AC_CHECK_MEMBERS([struct stat.st_rdev],[AC_DEFINE(HAVE_ST_RDEV, 1,
			    [Define to 1 if your `struct stat' has `st_rdev'.
			     Deprecated, use `HAVE_STRUCT_STAT_ST_RDEV'
			     instead.])])

AC_STRUCT_TIMEZONE_DAYLIGHT
AC_SYS_LARGEFILE
AC_MBSTATE_T

AC_CHECK_TYPE_STDC(uint8_t, unsigned char)
AC_CHECK_TYPE_STDC(uint16_t, unsigned short)
AC_CHECK_TYPE_STDC(uint32_t, unsigned int)
AC_CHECK_TYPE_STDC(int32_t, int)
AC_C_LONG_LONG
if test $ac_cv_c_long_long = yes; then
  long64='unsigned long long';
else
  long64='unsigned long';
fi
dnl This needs a patch to autoconf 2.13 acgeneral.m4
AC_CHECK_TYPE2_STDC(uint64_t, $long64)

AC_CHECK_SIZEOF_STDC_HEADERS(uint8_t, 0)
AC_CHECK_SIZEOF_STDC_HEADERS(uint16_t, 0)
AC_CHECK_SIZEOF_STDC_HEADERS(uint32_t, 0)
AC_CHECK_SIZEOF_STDC_HEADERS(uint64_t, 0)

dnl Checks for functions
AC_CHECK_FUNCS(mmap strerror strtoul mbrtowc mkstemp getopt_long utimes utime wcwidth)

dnl Checks for libraries
AC_CHECK_LIB(z,gzopen)

AC_CONFIG_FILES([Makefile src/Makefile magic/Makefile doc/Makefile python/Makefile])
AC_OUTPUT
