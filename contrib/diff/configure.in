dnl Process this file with autoconf to produce a configure script.
AC_INIT(diff.h)
AC_CONFIG_HEADER(config.h:config.hin)
AC_ARG_PROGRAM
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_ISC_POSIX
AC_MINIX
AC_C_CONST
AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h fcntl.h limits.h stdlib.h string.h sys/file.h time.h)
AC_HEADER_DIRENT
AC_HEADER_STAT
AC_HEADER_SYS_WAIT
AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(dup2 memchr sigaction strchr strerror tmpnam)
AC_REPLACE_FUNCS(memchr waitpid)
AC_FUNC_ALLOCA
AC_FUNC_CLOSEDIR_VOID
dnl No need for AC_FUNC_MEMCMP, since memcmp is used only to test for equality.
AC_FUNC_VFORK
AC_FUNC_VPRINTF
AC_STRUCT_ST_BLKSIZE
AC_OUTPUT(Makefile, [date > stamp-h])
