dnl Process this file with autoconf to produce a configure script.
AC_INIT(file, 5.03, christos@astron.com)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
#AC_CONFIG_MACRO_DIR([m4])

AC_MSG_CHECKING(for builtin ELF support)
AC_ARG_ENABLE(elf,
[  --disable-elf            disable builtin ELF support],
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([BUILTIN_ELF], 1, [Define if built-in ELF support is used])
else
  AC_MSG_RESULT(no)
fi], [
  # enable by default
  AC_MSG_RESULT(yes)
  AC_DEFINE([BUILTIN_ELF], 1, [Define in built-in ELF support is used])
])

AC_MSG_CHECKING(for ELF core file support)
AC_ARG_ENABLE(elf-core,
[  --disable-elf-core       disable ELF core file support],
[if test "${enableval}" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE([ELFCORE], 1, [Define for ELF core file support])
else
  AC_MSG_RESULT(no)
fi], [
  # enable by default
  AC_MSG_RESULT(yes)
  AC_DEFINE([ELFCORE], 1, [Define for ELF core file support])
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

AC_SUBST([pkgdatadir], ['$(datadir)/misc'])
AC_SUBST(fsect)
AM_CONDITIONAL(FSECT5, test x$fsect = x5)

AC_SUBST(WARNINGS)
AC_GNU_SOURCE

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

dnl Checks for headers
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(stdint.h fcntl.h locale.h stdint.h inttypes.h unistd.h)
AC_CHECK_HEADERS(utime.h wchar.h wctype.h limits.h)
AC_CHECK_HEADERS(getopt.h err.h)
AC_CHECK_HEADERS(sys/mman.h sys/stat.h sys/types.h sys/utime.h sys/time.h)
AC_CHECK_HEADERS(zlib.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_rdev])

AC_STRUCT_TM
AC_CHECK_MEMBERS([struct tm.tm_gmtoff, struct tm.tm_zone])
AC_STRUCT_TIMEZONE_DAYLIGHT
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_TYPE_MBSTATE_T

AC_STRUCT_OPTION_GETOPT_H

AC_CHECK_TYPES([uint8_t, uint16_t, uint32_t, int32_t, uint64_t, int64_t])
AC_CHECK_SIZEOF(long long)
AH_BOTTOM([
#ifndef HAVE_UINT8_T
typedef unsigned char uint8_t;
#endif
#ifndef HAVE_UINT16_T
typedef unsigned short uint16_t;
#endif
#ifndef HAVE_UINT32_T
typedef unsigned int uint32_t;
#endif
#ifndef HAVE_INT32_T
typedef int int32_t;
#endif
#ifndef HAVE_UINT64_T
#if SIZEOF_LONG_LONG == 8
typedef unsigned long long uint64_t;
#else
typedef unsigned long uint64_t;
#endif
#endif
#ifndef HAVE_INT64_T
#if SIZEOF_LONG_LONG == 8
typedef long long int64_t;
#else
typedef long int64_t;
#endif
#endif
])

AC_MSG_CHECKING(for gcc compiler warnings)
AC_ARG_ENABLE(warnings,
[  --disable-warnings	disable compiler warnings],
[if test "${enableval}" = no -o "$GCC" = no; then
   AC_MSG_RESULT(no)
   WARNINGS=
else
   AC_MSG_RESULT(yes)
   WARNINGS="-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
       -Wmissing-declarations -Wredundant-decls -Wnested-externs \
       -Wsign-compare -Wreturn-type -Wswitch -Wshadow \
       -Wcast-qual -Wwrite-strings -Wextra -Wunused-parameter"
fi], [
if test "$GCC" = no; then
   WARNINGS=
   AC_MSG_RESULT(no)
else
   AC_MSG_RESULT(yes)
   WARNINGS="-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
       -Wmissing-declarations -Wredundant-decls -Wnested-externs \
       -Wsign-compare -Wreturn-type -Wswitch -Wshadow \
       -Wcast-qual -Wwrite-strings -Wextra -Wunused-parameter"
fi])

dnl Checks for functions
AC_CHECK_FUNCS(mmap strerror strndup strtoul mbrtowc mkstemp utimes utime wcwidth strtof)

dnl Provide implementation of some required functions if necessary
AC_REPLACE_FUNCS(getopt_long asprintf vasprintf strlcpy strlcat)

dnl Checks for libraries
AC_CHECK_LIB(z,gzopen)

dnl See if we are cross-compiling
AM_CONDITIONAL(IS_CROSS_COMPILE, test "$cross_compiling" = yes)

AC_CONFIG_FILES([Makefile src/Makefile magic/Makefile tests/Makefile doc/Makefile python/Makefile])
AC_OUTPUT
