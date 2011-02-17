dnl
dnl Bash specific tests
dnl
dnl Some derived from PDKSH 5.1.3 autoconf tests
dnl

AC_DEFUN(BASH_C_LONG_LONG,
[AC_CACHE_CHECK(for long long, ac_cv_c_long_long,
[if test "$GCC" = yes; then
  ac_cv_c_long_long=yes
else
AC_TRY_RUN([
int
main()
{
long long foo = 0;
exit(sizeof(long long) < sizeof(long));
}
], ac_cv_c_long_long=yes, ac_cv_c_long_long=no)
fi])
if test $ac_cv_c_long_long = yes; then
  AC_DEFINE(HAVE_LONG_LONG, 1, [Define if the `long long' type works.])
fi
])

dnl
dnl This is very similar to AC_C_LONG_DOUBLE, with the fix for IRIX
dnl (< changed to <=) added.
dnl
AC_DEFUN(BASH_C_LONG_DOUBLE,
[AC_CACHE_CHECK(for long double, ac_cv_c_long_double,
[if test "$GCC" = yes; then
  ac_cv_c_long_double=yes
else
AC_TRY_RUN([
int
main()
{
  /* The Stardent Vistra knows sizeof(long double), but does not
     support it. */
  long double foo = 0.0;
  /* On Ultrix 4.3 cc, long double is 4 and double is 8.  */
  /* On IRIX 5.3, the compiler converts long double to double with a warning,
     but compiles this successfully. */
  exit(sizeof(long double) <= sizeof(double));
}
], ac_cv_c_long_double=yes, ac_cv_c_long_double=no)
fi])
if test $ac_cv_c_long_double = yes; then
  AC_DEFINE(HAVE_LONG_DOUBLE, 1, [Define if the `long double' type works.])
fi
])

dnl
dnl Check for <inttypes.h>.  This is separated out so that it can be
dnl AC_REQUIREd.
dnl
dnl BASH_HEADER_INTTYPES
AC_DEFUN(BASH_HEADER_INTTYPES,
[
 AC_CHECK_HEADERS(inttypes.h)
])

dnl
dnl check for typedef'd symbols in header files, but allow the caller to
dnl specify the include files to be checked in addition to the default
dnl 
dnl BASH_CHECK_TYPE(TYPE, HEADERS, DEFAULT[, VALUE-IF-FOUND])
AC_DEFUN(BASH_CHECK_TYPE,
[
AC_REQUIRE([AC_HEADER_STDC])dnl
AC_REQUIRE([BASH_HEADER_INTTYPES])
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(bash_cv_type_$1,
[AC_EGREP_CPP($1, [#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
$2
], bash_cv_type_$1=yes, bash_cv_type_$1=no)])
AC_MSG_RESULT($bash_cv_type_$1)
ifelse($#, 4, [if test $bash_cv_type_$1 = yes; then
	AC_DEFINE($4)
	fi])
if test $bash_cv_type_$1 = no; then
  AC_DEFINE_UNQUOTED($1, $3)
fi
])

dnl
dnl BASH_CHECK_DECL(FUNC)
dnl
dnl Check for a declaration of FUNC in stdlib.h and inttypes.h like
dnl AC_CHECK_DECL
dnl
AC_DEFUN(BASH_CHECK_DECL,
[
AC_REQUIRE([AC_HEADER_STDC])
AC_REQUIRE([BASH_HEADER_INTTYPES])
AC_CACHE_CHECK([for declaration of $1], bash_cv_decl_$1,
[AC_TRY_LINK(
[
#if STDC_HEADERS
#  include <stdlib.h>
#endif
#if HAVE_INTTYPES_H
#  include <inttypes.h>
#endif
],
[return !$1;],
bash_cv_decl_$1=yes, bash_cv_decl_$1=no)])
bash_tr_func=HAVE_DECL_`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
if test $bash_cv_decl_$1 = yes; then
  AC_DEFINE_UNQUOTED($bash_tr_func, 1)
else
  AC_DEFINE_UNQUOTED($bash_tr_func, 0)
fi
])

AC_DEFUN(BASH_DECL_PRINTF,
[AC_MSG_CHECKING(for declaration of printf in <stdio.h>)
AC_CACHE_VAL(bash_cv_printf_declared,
[AC_TRY_RUN([
#include <stdio.h>
#ifdef __STDC__
typedef int (*_bashfunc)(const char *, ...);
#else
typedef int (*_bashfunc)();
#endif
main()
{
_bashfunc pf;
pf = (_bashfunc) printf;
exit(pf == 0);
}
], bash_cv_printf_declared=yes, bash_cv_printf_declared=no,
   [AC_MSG_WARN(cannot check printf declaration if cross compiling -- defaulting to yes)
    bash_cv_printf_declared=yes]
)])
AC_MSG_RESULT($bash_cv_printf_declared)
if test $bash_cv_printf_declared = yes; then
AC_DEFINE(PRINTF_DECLARED)
fi
])

AC_DEFUN(BASH_DECL_SBRK,
[AC_MSG_CHECKING(for declaration of sbrk in <unistd.h>)
AC_CACHE_VAL(bash_cv_sbrk_declared,
[AC_EGREP_HEADER(sbrk, unistd.h,
 bash_cv_sbrk_declared=yes, bash_cv_sbrk_declared=no)])
AC_MSG_RESULT($bash_cv_sbrk_declared)
if test $bash_cv_sbrk_declared = yes; then
AC_DEFINE(SBRK_DECLARED)
fi
])

dnl
dnl Check for sys_siglist[] or _sys_siglist[]
dnl
AC_DEFUN(BASH_DECL_UNDER_SYS_SIGLIST,
[AC_MSG_CHECKING([for _sys_siglist in signal.h or unistd.h])
AC_CACHE_VAL(bash_cv_decl_under_sys_siglist,
[AC_TRY_COMPILE([
#include <sys/types.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], [ char *msg = _sys_siglist[2]; ],
  bash_cv_decl_under_sys_siglist=yes, bash_cv_decl_under_sys_siglist=no,
  [AC_MSG_WARN(cannot check for _sys_siglist[] if cross compiling -- defaulting to no)])])dnl
AC_MSG_RESULT($bash_cv_decl_under_sys_siglist)
if test $bash_cv_decl_under_sys_siglist = yes; then
AC_DEFINE(UNDER_SYS_SIGLIST_DECLARED)
fi
])

AC_DEFUN(BASH_UNDER_SYS_SIGLIST,
[AC_REQUIRE([BASH_DECL_UNDER_SYS_SIGLIST])
AC_MSG_CHECKING([for _sys_siglist in system C library])
AC_CACHE_VAL(bash_cv_under_sys_siglist,
[AC_TRY_RUN([
#include <sys/types.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef UNDER_SYS_SIGLIST_DECLARED
extern char *_sys_siglist[];
#endif
main()
{
char *msg = (char *)_sys_siglist[2];
exit(msg == 0);
}],
	bash_cv_under_sys_siglist=yes, bash_cv_under_sys_siglist=no,
	[AC_MSG_WARN(cannot check for _sys_siglist[] if cross compiling -- defaulting to no)
	 bash_cv_under_sys_siglist=no])])
AC_MSG_RESULT($bash_cv_under_sys_siglist)
if test $bash_cv_under_sys_siglist = yes; then
AC_DEFINE(HAVE_UNDER_SYS_SIGLIST)
fi
])

AC_DEFUN(BASH_SYS_SIGLIST,
[AC_REQUIRE([AC_DECL_SYS_SIGLIST])
AC_MSG_CHECKING([for sys_siglist in system C library])
AC_CACHE_VAL(bash_cv_sys_siglist,
[AC_TRY_RUN([
#include <sys/types.h>
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef SYS_SIGLIST_DECLARED
extern char *sys_siglist[];
#endif
main()
{
char *msg = sys_siglist[2];
exit(msg == 0);
}],
	bash_cv_sys_siglist=yes, bash_cv_sys_siglist=no,
	[AC_MSG_WARN(cannot check for sys_siglist if cross compiling -- defaulting to no)
	 bash_cv_sys_siglist=no])])
AC_MSG_RESULT($bash_cv_sys_siglist)
if test $bash_cv_sys_siglist = yes; then
AC_DEFINE(HAVE_SYS_SIGLIST)
fi
])

dnl Check for the various permutations of sys_siglist and make sure we
dnl compile in siglist.o if they're not defined
AC_DEFUN(BASH_CHECK_SYS_SIGLIST, [
AC_REQUIRE([BASH_SYS_SIGLIST])
AC_REQUIRE([BASH_DECL_UNDER_SYS_SIGLIST])
AC_REQUIRE([BASH_FUNC_STRSIGNAL])
if test "$bash_cv_sys_siglist" = no && test "$bash_cv_under_sys_siglist" = no && test "$bash_cv_have_strsignal" = no; then
  SIGLIST_O=siglist.o
else
  SIGLIST_O=
fi
AC_SUBST([SIGLIST_O])
])

dnl Check for sys_errlist[] and sys_nerr, check for declaration
AC_DEFUN(BASH_SYS_ERRLIST,
[AC_MSG_CHECKING([for sys_errlist and sys_nerr])
AC_CACHE_VAL(bash_cv_sys_errlist,
[AC_TRY_LINK([#include <errno.h>],
[extern char *sys_errlist[];
 extern int sys_nerr;
 char *msg = sys_errlist[sys_nerr - 1];],
    bash_cv_sys_errlist=yes, bash_cv_sys_errlist=no)])dnl
AC_MSG_RESULT($bash_cv_sys_errlist)
if test $bash_cv_sys_errlist = yes; then
AC_DEFINE(HAVE_SYS_ERRLIST)
fi
])

dnl
dnl Check if dup2() does not clear the close on exec flag
dnl
AC_DEFUN(BASH_FUNC_DUP2_CLOEXEC_CHECK,
[AC_MSG_CHECKING(if dup2 fails to clear the close-on-exec flag)
AC_CACHE_VAL(bash_cv_dup2_broken,
[AC_TRY_RUN([
#include <sys/types.h>
#include <fcntl.h>
main()
{
  int fd1, fd2, fl;
  fd1 = open("/dev/null", 2);
  if (fcntl(fd1, 2, 1) < 0)
    exit(1);
  fd2 = dup2(fd1, 1);
  if (fd2 < 0)
    exit(2);
  fl = fcntl(fd2, 1, 0);
  /* fl will be 1 if dup2 did not reset the close-on-exec flag. */
  exit(fl != 1);
}
], bash_cv_dup2_broken=yes, bash_cv_dup2_broken=no,
    [AC_MSG_WARN(cannot check dup2 if cross compiling -- defaulting to no)
     bash_cv_dup2_broken=no])
])
AC_MSG_RESULT($bash_cv_dup2_broken)
if test $bash_cv_dup2_broken = yes; then
AC_DEFINE(DUP2_BROKEN)
fi
])

AC_DEFUN(BASH_FUNC_STRSIGNAL,
[AC_MSG_CHECKING([for the existence of strsignal])
AC_CACHE_VAL(bash_cv_have_strsignal,
[AC_TRY_LINK([#include <sys/types.h>
#include <signal.h>],
[char *s = (char *)strsignal(2);],
 bash_cv_have_strsignal=yes, bash_cv_have_strsignal=no)])
AC_MSG_RESULT($bash_cv_have_strsignal)
if test $bash_cv_have_strsignal = yes; then
AC_DEFINE(HAVE_STRSIGNAL)
fi
])

dnl Check to see if opendir will open non-directories (not a nice thing)
AC_DEFUN(BASH_FUNC_OPENDIR_CHECK,
[AC_REQUIRE([AC_HEADER_DIRENT])dnl
AC_MSG_CHECKING(if opendir() opens non-directories)
AC_CACHE_VAL(bash_cv_opendir_not_robust,
[AC_TRY_RUN([
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* SYSNDIR */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* SYSDIR */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif /* HAVE_DIRENT_H */
main()
{
DIR *dir;
int fd, err;
err = mkdir("/tmp/bash-aclocal", 0700);
if (err < 0) {
  perror("mkdir");
  exit(1);
}
unlink("/tmp/bash-aclocal/not_a_directory");
fd = open("/tmp/bash-aclocal/not_a_directory", O_WRONLY|O_CREAT|O_EXCL, 0666);
write(fd, "\n", 1);
close(fd);
dir = opendir("/tmp/bash-aclocal/not_a_directory");
unlink("/tmp/bash-aclocal/not_a_directory");
rmdir("/tmp/bash-aclocal");
exit (dir == 0);
}], bash_cv_opendir_not_robust=yes,bash_cv_opendir_not_robust=no,
    [AC_MSG_WARN(cannot check opendir if cross compiling -- defaulting to no)
     bash_cv_opendir_not_robust=no]
)])
AC_MSG_RESULT($bash_cv_opendir_not_robust)
if test $bash_cv_opendir_not_robust = yes; then
AC_DEFINE(OPENDIR_NOT_ROBUST)
fi
])

dnl
AC_DEFUN(BASH_TYPE_SIGHANDLER,
[AC_MSG_CHECKING([whether signal handlers are of type void])
AC_CACHE_VAL(bash_cv_void_sighandler,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <signal.h>
#ifdef signal
#undef signal
#endif
#ifdef __cplusplus
extern "C"
#endif
void (*signal ()) ();],
[int i;], bash_cv_void_sighandler=yes, bash_cv_void_sighandler=no)])dnl
AC_MSG_RESULT($bash_cv_void_sighandler)
if test $bash_cv_void_sighandler = yes; then
AC_DEFINE(VOID_SIGHANDLER)
fi
])

dnl
dnl A signed 16-bit integer quantity
dnl
AC_DEFUN(BASH_TYPE_BITS16_T,
[
if test "$ac_cv_sizeof_short" = 2; then
  AC_CHECK_TYPE(bits16_t, short)
elif test "$ac_cv_sizeof_char" = 2; then
  AC_CHECK_TYPE(bits16_t, char)
else
  AC_CHECK_TYPE(bits16_t, short)
fi
])

dnl
dnl An unsigned 16-bit integer quantity
dnl
AC_DEFUN(BASH_TYPE_U_BITS16_T,
[
if test "$ac_cv_sizeof_short" = 2; then
  AC_CHECK_TYPE(u_bits16_t, unsigned short)
elif test "$ac_cv_sizeof_char" = 2; then
  AC_CHECK_TYPE(u_bits16_t, unsigned char)
else
  AC_CHECK_TYPE(u_bits16_t, unsigned short)
fi
])

dnl
dnl A signed 32-bit integer quantity
dnl
AC_DEFUN(BASH_TYPE_BITS32_T,
[
if test "$ac_cv_sizeof_int" = 4; then
  AC_CHECK_TYPE(bits32_t, int)
elif test "$ac_cv_sizeof_long" = 4; then
  AC_CHECK_TYPE(bits32_t, long)
else
  AC_CHECK_TYPE(bits32_t, int)
fi
])

dnl
dnl An unsigned 32-bit integer quantity
dnl
AC_DEFUN(BASH_TYPE_U_BITS32_T,
[
if test "$ac_cv_sizeof_int" = 4; then
  AC_CHECK_TYPE(u_bits32_t, unsigned int)
elif test "$ac_cv_sizeof_long" = 4; then
  AC_CHECK_TYPE(u_bits32_t, unsigned long)
else
  AC_CHECK_TYPE(u_bits32_t, unsigned int)
fi
])

AC_DEFUN(BASH_TYPE_PTRDIFF_T,
[
if test "$ac_cv_sizeof_int" = "$ac_cv_sizeof_char_p"; then
  AC_CHECK_TYPE(ptrdiff_t, int)
elif test "$ac_cv_sizeof_long" = "$ac_cv_sizeof_char_p"; then
  AC_CHECK_TYPE(ptrdiff_t, long)
elif test "$ac_cv_type_long_long" = yes && test "$ac_cv_sizeof_long_long" = "$ac_cv_sizeof_char_p"; then
  AC_CHECK_TYPE(ptrdiff_t, [long long])
else
  AC_CHECK_TYPE(ptrdiff_t, int)
fi
])

dnl
dnl A signed 64-bit quantity
dnl
AC_DEFUN(BASH_TYPE_BITS64_T,
[
if test "$ac_cv_sizeof_char_p" = 8; then
  AC_CHECK_TYPE(bits64_t, char *)
elif test "$ac_cv_sizeof_double" = 8; then
  AC_CHECK_TYPE(bits64_t, double)
elif test -n "$ac_cv_type_long_long" && test "$ac_cv_sizeof_long_long" = 8; then
  AC_CHECK_TYPE(bits64_t, [long long])
elif test "$ac_cv_sizeof_long" = 8; then
  AC_CHECK_TYPE(bits64_t, long)
else
  AC_CHECK_TYPE(bits64_t, double)
fi
])

AC_DEFUN(BASH_TYPE_LONG_LONG,
[
AC_CACHE_CHECK([for long long], bash_cv_type_long_long,
[AC_TRY_LINK([
long long ll = 1; int i = 63;],
[
long long llm = (long long) -1;
return ll << i | ll >> i | llm / ll | llm % ll;
], bash_cv_type_long_long='long long', bash_cv_type_long_long='long')])
if test "$bash_cv_type_long_long" = 'long long'; then
  AC_DEFINE(HAVE_LONG_LONG, 1)
fi
])

AC_DEFUN(BASH_TYPE_UNSIGNED_LONG_LONG,
[
AC_CACHE_CHECK([for unsigned long long], bash_cv_type_unsigned_long_long,
[AC_TRY_LINK([
unsigned long long ull = 1; int i = 63;],
[
unsigned long long ullmax = (unsigned long long) -1;
return ull << i | ull >> i | ullmax / ull | ullmax % ull;
], bash_cv_type_unsigned_long_long='unsigned long long',
   bash_cv_type_unsigned_long_long='unsigned long')])
if test "$bash_cv_type_unsigned_long_long" = 'unsigned long long'; then
  AC_DEFINE(HAVE_UNSIGNED_LONG_LONG, 1)
fi
])

dnl
dnl Type of struct rlimit fields: some systems (OSF/1, NetBSD, RISC/os 5.0)
dnl have a rlim_t, others (4.4BSD based systems) use quad_t, others use
dnl long and still others use int (HP-UX 9.01, SunOS 4.1.3).  To simplify
dnl matters, this just checks for rlim_t, quad_t, or long.
dnl
AC_DEFUN(BASH_TYPE_RLIMIT,
[AC_MSG_CHECKING(for size and type of struct rlimit fields)
AC_CACHE_VAL(bash_cv_type_rlimit,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/resource.h>],
[rlim_t xxx;], bash_cv_type_rlimit=rlim_t,[
AC_TRY_RUN([
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
main()
{
#ifdef HAVE_QUAD_T
  struct rlimit rl;
  if (sizeof(rl.rlim_cur) == sizeof(quad_t))
    exit(0);
#endif
  exit(1);
}], bash_cv_type_rlimit=quad_t, bash_cv_type_rlimit=long,
        [AC_MSG_WARN(cannot check quad_t if cross compiling -- defaulting to long)
         bash_cv_type_rlimit=long])])
])
AC_MSG_RESULT($bash_cv_type_rlimit)
if test $bash_cv_type_rlimit = quad_t; then
AC_DEFINE(RLIMTYPE, quad_t)
elif test $bash_cv_type_rlimit = rlim_t; then
AC_DEFINE(RLIMTYPE, rlim_t)
fi
])

AC_DEFUN(BASH_FUNC_LSTAT,
[dnl Cannot use AC_CHECK_FUNCS(lstat) because Linux defines lstat() as an
dnl inline function in <sys/stat.h>.
AC_CACHE_CHECK([for lstat], bash_cv_func_lstat,
[AC_TRY_LINK([
#include <sys/types.h>
#include <sys/stat.h>
],[ lstat(".",(struct stat *)0); ],
bash_cv_func_lstat=yes, bash_cv_func_lstat=no)])
if test $bash_cv_func_lstat = yes; then
  AC_DEFINE(HAVE_LSTAT)
fi
])

AC_DEFUN(BASH_FUNC_INET_ATON,
[
AC_CACHE_CHECK([for inet_aton], bash_cv_func_inet_aton,
[AC_TRY_LINK([
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
struct in_addr ap;], [ inet_aton("127.0.0.1", &ap); ],
bash_cv_func_inet_aton=yes, bash_cv_func_inet_aton=no)])
if test $bash_cv_func_inet_aton = yes; then
  AC_DEFINE(HAVE_INET_ATON)
else
  AC_LIBOBJ(inet_aton)
fi
])

AC_DEFUN(BASH_FUNC_GETENV,
[AC_MSG_CHECKING(to see if getenv can be redefined)
AC_CACHE_VAL(bash_cv_getenv_redef,
[AC_TRY_RUN([
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifndef __STDC__
#  ifndef const
#    define const
#  endif
#endif
char *
getenv (name)
#if defined (__linux__) || defined (__bsdi__) || defined (convex)
     const char *name;
#else
     char const *name;
#endif /* !__linux__ && !__bsdi__ && !convex */
{
return "42";
}
main()
{
char *s;
/* The next allows this program to run, but does not allow bash to link
   when it redefines getenv.  I'm not really interested in figuring out
   why not. */
#if defined (NeXT)
exit(1);
#endif
s = getenv("ABCDE");
exit(s == 0);	/* force optimizer to leave getenv in */
}
], bash_cv_getenv_redef=yes, bash_cv_getenv_redef=no,
   [AC_MSG_WARN(cannot check getenv redefinition if cross compiling -- defaulting to yes)
    bash_cv_getenv_redef=yes]
)])
AC_MSG_RESULT($bash_cv_getenv_redef)
if test $bash_cv_getenv_redef = yes; then
AC_DEFINE(CAN_REDEFINE_GETENV)
fi
])

# We should check for putenv before calling this
AC_DEFUN(BASH_FUNC_STD_PUTENV,
[
AC_REQUIRE([AC_HEADER_STDC])
AC_REQUIRE([AC_C_PROTOTYPES])
AC_CACHE_CHECK([for standard-conformant putenv declaration], bash_cv_std_putenv,
[AC_TRY_LINK([
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#ifndef __STDC__
#  ifndef const
#    define const
#  endif
#endif
#ifdef PROTOTYPES
extern int putenv (char *);
#else
extern int putenv ();
#endif
],
[return (putenv == 0);],
bash_cv_std_putenv=yes, bash_cv_std_putenv=no
)])
if test $bash_cv_std_putenv = yes; then
AC_DEFINE(HAVE_STD_PUTENV)
fi
])

# We should check for unsetenv before calling this
AC_DEFUN(BASH_FUNC_STD_UNSETENV,
[
AC_REQUIRE([AC_HEADER_STDC])
AC_REQUIRE([AC_C_PROTOTYPES])
AC_CACHE_CHECK([for standard-conformant unsetenv declaration], bash_cv_std_unsetenv,
[AC_TRY_LINK([
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#ifndef __STDC__
#  ifndef const
#    define const
#  endif
#endif
#ifdef PROTOTYPES
extern int unsetenv (const char *);
#else
extern int unsetenv ();
#endif
],
[return (unsetenv == 0);],
bash_cv_std_unsetenv=yes, bash_cv_std_unsetenv=no
)])
if test $bash_cv_std_unsetenv = yes; then
AC_DEFINE(HAVE_STD_UNSETENV)
fi
])

AC_DEFUN(BASH_FUNC_ULIMIT_MAXFDS,
[AC_MSG_CHECKING(whether ulimit can substitute for getdtablesize)
AC_CACHE_VAL(bash_cv_ulimit_maxfds,
[AC_TRY_RUN([
main()
{
long maxfds = ulimit(4, 0L);
exit (maxfds == -1L);
}
], bash_cv_ulimit_maxfds=yes, bash_cv_ulimit_maxfds=no,
   [AC_MSG_WARN(cannot check ulimit if cross compiling -- defaulting to no)
    bash_cv_ulimit_maxfds=no]
)])
AC_MSG_RESULT($bash_cv_ulimit_maxfds)
if test $bash_cv_ulimit_maxfds = yes; then
AC_DEFINE(ULIMIT_MAXFDS)
fi
])

AC_DEFUN(BASH_FUNC_GETCWD,
[AC_MSG_CHECKING([if getcwd() will dynamically allocate memory])
AC_CACHE_VAL(bash_cv_getcwd_malloc,
[AC_TRY_RUN([
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

main()
{
	char	*xpwd;
	xpwd = getcwd(0, 0);
	exit (xpwd == 0);
}
], bash_cv_getcwd_malloc=yes, bash_cv_getcwd_malloc=no,
   [AC_MSG_WARN(cannot check whether getcwd allocates memory when cross-compiling -- defaulting to no)
    bash_cv_getcwd_malloc=no]
)])
AC_MSG_RESULT($bash_cv_getcwd_malloc)
if test $bash_cv_getcwd_malloc = no; then
AC_DEFINE(GETCWD_BROKEN)
AC_LIBOBJ(getcwd)
fi
])

dnl
dnl This needs BASH_CHECK_SOCKLIB, but since that's not called on every
dnl system, we can't use AC_PREREQ
dnl
AC_DEFUN(BASH_FUNC_GETHOSTBYNAME,
[if test "X$bash_cv_have_gethostbyname" = "X"; then
_bash_needmsg=yes
else
AC_MSG_CHECKING(for gethostbyname in socket library)
_bash_needmsg=
fi
AC_CACHE_VAL(bash_cv_have_gethostbyname,
[AC_TRY_LINK([#include <netdb.h>],
[ struct hostent *hp;
  hp = gethostbyname("localhost");
], bash_cv_have_gethostbyname=yes, bash_cv_have_gethostbyname=no)]
)
if test "X$_bash_needmsg" = Xyes; then
    AC_MSG_CHECKING(for gethostbyname in socket library)
fi
AC_MSG_RESULT($bash_cv_have_gethostbyname)
if test "$bash_cv_have_gethostbyname" = yes; then
AC_DEFINE(HAVE_GETHOSTBYNAME)
fi
])

AC_DEFUN(BASH_FUNC_FNMATCH_EXTMATCH,
[AC_MSG_CHECKING(if fnmatch does extended pattern matching with FNM_EXTMATCH)
AC_CACHE_VAL(bash_cv_fnm_extmatch,
[AC_TRY_RUN([
#include <fnmatch.h>

main()
{
#ifdef FNM_EXTMATCH
  exit (0);
#else
  exit (1);
#endif
}
], bash_cv_fnm_extmatch=yes, bash_cv_fnm_extmatch=no,
    [AC_MSG_WARN(cannot check FNM_EXTMATCH if cross compiling -- defaulting to no)
     bash_cv_fnm_extmatch=no])
])
AC_MSG_RESULT($bash_cv_fnm_extmatch)
if test $bash_cv_fnm_extmatch = yes; then
AC_DEFINE(HAVE_LIBC_FNM_EXTMATCH)
fi
])

AC_DEFUN(BASH_FUNC_POSIX_SETJMP,
[AC_REQUIRE([BASH_SYS_SIGNAL_VINTAGE])
AC_MSG_CHECKING(for presence of POSIX-style sigsetjmp/siglongjmp)
AC_CACHE_VAL(bash_cv_func_sigsetjmp,
[AC_TRY_RUN([
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>

main()
{
#if !defined (_POSIX_VERSION) || !defined (HAVE_POSIX_SIGNALS)
exit (1);
#else

int code;
sigset_t set, oset;
sigjmp_buf xx;

/* get the mask */
sigemptyset(&set);
sigemptyset(&oset);
sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &set);
sigprocmask(SIG_BLOCK, (sigset_t *)NULL, &oset);

/* save it */
code = sigsetjmp(xx, 1);
if (code)
  exit(0);	/* could get sigmask and compare to oset here. */

/* change it */
sigaddset(&set, SIGINT);
sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

/* and siglongjmp */
siglongjmp(xx, 10);
exit(1);
#endif
}], bash_cv_func_sigsetjmp=present, bash_cv_func_sigsetjmp=missing,
    [AC_MSG_WARN(cannot check for sigsetjmp/siglongjmp if cross-compiling -- defaulting to missing)
     bash_cv_func_sigsetjmp=missing]
)])
AC_MSG_RESULT($bash_cv_func_sigsetjmp)
if test $bash_cv_func_sigsetjmp = present; then
AC_DEFINE(HAVE_POSIX_SIGSETJMP)
fi
])

AC_DEFUN(BASH_FUNC_STRCOLL,
[
AC_MSG_CHECKING(whether or not strcoll and strcmp differ)
AC_CACHE_VAL(bash_cv_func_strcoll_broken,
[AC_TRY_RUN([
#include <stdio.h>
#if defined (HAVE_LOCALE_H)
#include <locale.h>
#endif

main(c, v)
int     c;
char    *v[];
{
        int     r1, r2;
        char    *deflocale, *defcoll;

#ifdef HAVE_SETLOCALE
        deflocale = setlocale(LC_ALL, "");
	defcoll = setlocale(LC_COLLATE, "");
#endif

#ifdef HAVE_STRCOLL
	/* These two values are taken from tests/glob-test. */
        r1 = strcoll("abd", "aXd");
#else
	r1 = 0;
#endif
        r2 = strcmp("abd", "aXd");

	/* These two should both be greater than 0.  It is permissible for
	   a system to return different values, as long as the sign is the
	   same. */

        /* Exit with 1 (failure) if these two values are both > 0, since
	   this tests whether strcoll(3) is broken with respect to strcmp(3)
	   in the default locale. */
	exit (r1 > 0 && r2 > 0);
}
], bash_cv_func_strcoll_broken=yes, bash_cv_func_strcoll_broken=no,
   [AC_MSG_WARN(cannot check strcoll if cross compiling -- defaulting to no)
    bash_cv_func_strcoll_broken=no]
)])
AC_MSG_RESULT($bash_cv_func_strcoll_broken)
if test $bash_cv_func_strcoll_broken = yes; then
AC_DEFINE(STRCOLL_BROKEN)
fi
])

AC_DEFUN(BASH_FUNC_PRINTF_A_FORMAT,
[AC_MSG_CHECKING([for printf floating point output in hex notation])
AC_CACHE_VAL(bash_cv_printf_a_format,
[AC_TRY_RUN([
#include <stdio.h>
#include <string.h>

int
main()
{
	double y = 0.0;
	char abuf[1024];

	sprintf(abuf, "%A", y);
	exit(strchr(abuf, 'P') == (char *)0);
}
], bash_cv_printf_a_format=yes, bash_cv_printf_a_format=no,
   [AC_MSG_WARN(cannot check printf if cross compiling -- defaulting to no)
    bash_cv_printf_a_format=no]
)])
AC_MSG_RESULT($bash_cv_printf_a_format)
if test $bash_cv_printf_a_format = yes; then
AC_DEFINE(HAVE_PRINTF_A_FORMAT)
fi
])

AC_DEFUN(BASH_STRUCT_TERMIOS_LDISC,
[
AC_CHECK_MEMBER(struct termios.c_line, AC_DEFINE(TERMIOS_LDISC), ,[
#include <sys/types.h>
#include <termios.h>
])
])

AC_DEFUN(BASH_STRUCT_TERMIO_LDISC,
[
AC_CHECK_MEMBER(struct termio.c_line, AC_DEFINE(TERMIO_LDISC), ,[
#include <sys/types.h>
#include <termio.h>
])
])

dnl
dnl Like AC_STRUCT_ST_BLOCKS, but doesn't muck with LIBOBJS
dnl
dnl sets bash_cv_struct_stat_st_blocks
dnl
dnl unused for now; we'll see how AC_CHECK_MEMBERS works
dnl
AC_DEFUN(BASH_STRUCT_ST_BLOCKS,
[
AC_MSG_CHECKING([for struct stat.st_blocks])
AC_CACHE_VAL(bash_cv_struct_stat_st_blocks,
[AC_TRY_COMPILE(
[
#include <sys/types.h>
#include <sys/stat.h>
],
[
main()
{
static struct stat a;
if (a.st_blocks) return 0;
return 0;
}
], bash_cv_struct_stat_st_blocks=yes, bash_cv_struct_stat_st_blocks=no)
])
AC_MSG_RESULT($bash_cv_struct_stat_st_blocks)
if test "$bash_cv_struct_stat_st_blocks" = "yes"; then
AC_DEFINE(HAVE_STRUCT_STAT_ST_BLOCKS)
fi
])

AC_DEFUN([BASH_CHECK_LIB_TERMCAP],
[
if test "X$bash_cv_termcap_lib" = "X"; then
_bash_needmsg=yes
else
AC_MSG_CHECKING(which library has the termcap functions)
_bash_needmsg=
fi
AC_CACHE_VAL(bash_cv_termcap_lib,
[AC_CHECK_FUNC(tgetent, bash_cv_termcap_lib=libc,
  [AC_CHECK_LIB(termcap, tgetent, bash_cv_termcap_lib=libtermcap,
    [AC_CHECK_LIB(tinfo, tgetent, bash_cv_termcap_lib=libtinfo,
        [AC_CHECK_LIB(curses, tgetent, bash_cv_termcap_lib=libcurses,
	    [AC_CHECK_LIB(ncurses, tgetent, bash_cv_termcap_lib=libncurses,
	        bash_cv_termcap_lib=gnutermcap)])])])])])
if test "X$_bash_needmsg" = "Xyes"; then
AC_MSG_CHECKING(which library has the termcap functions)
fi
AC_MSG_RESULT(using $bash_cv_termcap_lib)
if test $bash_cv_termcap_lib = gnutermcap && test -z "$prefer_curses"; then
LDFLAGS="$LDFLAGS -L./lib/termcap"
TERMCAP_LIB="./lib/termcap/libtermcap.a"
TERMCAP_DEP="./lib/termcap/libtermcap.a"
elif test $bash_cv_termcap_lib = libtermcap && test -z "$prefer_curses"; then
TERMCAP_LIB=-ltermcap
TERMCAP_DEP=
elif test $bash_cv_termcap_lib = libtinfo; then
TERMCAP_LIB=-ltinfo
TERMCAP_DEP=
elif test $bash_cv_termcap_lib = libncurses; then
TERMCAP_LIB=-lncurses
TERMCAP_DEP=
elif test $bash_cv_termcap_lib = libc; then
TERMCAP_LIB=
TERMCAP_DEP=
else
TERMCAP_LIB=-lcurses
TERMCAP_DEP=
fi
])

dnl
dnl Check for the presence of getpeername in libsocket.
dnl If libsocket is present, check for libnsl and add it to LIBS if
dnl it's there, since most systems with libsocket require linking
dnl with libnsl as well.  This should only be called if getpeername
dnl was not found in libc.
dnl
dnl NOTE: IF WE FIND GETPEERNAME, WE ASSUME THAT WE HAVE BIND/CONNECT
dnl	  AS WELL
dnl
AC_DEFUN(BASH_CHECK_LIB_SOCKET,
[
if test "X$bash_cv_have_socklib" = "X"; then
_bash_needmsg=
else
AC_MSG_CHECKING(for socket library)
_bash_needmsg=yes
fi
AC_CACHE_VAL(bash_cv_have_socklib,
[AC_CHECK_LIB(socket, getpeername,
        bash_cv_have_socklib=yes, bash_cv_have_socklib=no, -lnsl)])
if test "X$_bash_needmsg" = Xyes; then
  AC_MSG_RESULT($bash_cv_have_socklib)
  _bash_needmsg=
fi
if test $bash_cv_have_socklib = yes; then
  # check for libnsl, add it to LIBS if present
  if test "X$bash_cv_have_libnsl" = "X"; then
    _bash_needmsg=
  else
    AC_MSG_CHECKING(for libnsl)
    _bash_needmsg=yes
  fi
  AC_CACHE_VAL(bash_cv_have_libnsl,
	   [AC_CHECK_LIB(nsl, t_open,
		 bash_cv_have_libnsl=yes, bash_cv_have_libnsl=no)])
  if test "X$_bash_needmsg" = Xyes; then
    AC_MSG_RESULT($bash_cv_have_libnsl)
    _bash_needmsg=
  fi
  if test $bash_cv_have_libnsl = yes; then
    LIBS="-lsocket -lnsl $LIBS"
  else
    LIBS="-lsocket $LIBS"
  fi
  AC_DEFINE(HAVE_LIBSOCKET)
  AC_DEFINE(HAVE_GETPEERNAME)
fi
])

AC_DEFUN(BASH_STRUCT_DIRENT_D_INO,
[AC_REQUIRE([AC_HEADER_DIRENT])
AC_MSG_CHECKING(for struct dirent.d_ino)
AC_CACHE_VAL(bash_cv_dirent_has_dino,
[AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* SYSNDIR */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* SYSDIR */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif /* HAVE_DIRENT_H */
],[
struct dirent d; int z; z = d.d_ino;
], bash_cv_dirent_has_dino=yes, bash_cv_dirent_has_dino=no)])
AC_MSG_RESULT($bash_cv_dirent_has_dino)
if test $bash_cv_dirent_has_dino = yes; then
AC_DEFINE(HAVE_STRUCT_DIRENT_D_INO)
fi
])

AC_DEFUN(BASH_STRUCT_DIRENT_D_FILENO,
[AC_REQUIRE([AC_HEADER_DIRENT])
AC_MSG_CHECKING(for struct dirent.d_fileno)
AC_CACHE_VAL(bash_cv_dirent_has_d_fileno,
[AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* SYSNDIR */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* SYSDIR */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif /* HAVE_DIRENT_H */
],[
struct dirent d; int z; z = d.d_fileno;
], bash_cv_dirent_has_d_fileno=yes, bash_cv_dirent_has_d_fileno=no)])
AC_MSG_RESULT($bash_cv_dirent_has_d_fileno)
if test $bash_cv_dirent_has_d_fileno = yes; then
AC_DEFINE(HAVE_STRUCT_DIRENT_D_FILENO)
fi
])

AC_DEFUN(BASH_STRUCT_DIRENT_D_NAMLEN,
[AC_REQUIRE([AC_HEADER_DIRENT])
AC_MSG_CHECKING(for struct dirent.d_namlen)
AC_CACHE_VAL(bash_cv_dirent_has_d_namlen,
[AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* SYSNDIR */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* SYSDIR */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif /* HAVE_DIRENT_H */
],[
struct dirent d; int z; z = d.d_namlen;
], bash_cv_dirent_has_d_namlen=yes, bash_cv_dirent_has_d_namlen=no)])
AC_MSG_RESULT($bash_cv_dirent_has_d_namlen)
if test $bash_cv_dirent_has_d_namlen = yes; then
AC_DEFINE(HAVE_STRUCT_DIRENT_D_NAMLEN)
fi
])

AC_DEFUN(BASH_STRUCT_TIMEVAL,
[AC_MSG_CHECKING(for struct timeval in sys/time.h and time.h)
AC_CACHE_VAL(bash_cv_struct_timeval,
[
AC_EGREP_HEADER(struct timeval, sys/time.h,
		bash_cv_struct_timeval=yes,
		AC_EGREP_HEADER(struct timeval, time.h,
			bash_cv_struct_timeval=yes,
			bash_cv_struct_timeval=no))
])
AC_MSG_RESULT($bash_cv_struct_timeval)
if test $bash_cv_struct_timeval = yes; then
  AC_DEFINE(HAVE_TIMEVAL)
fi
])

AC_DEFUN(BASH_STRUCT_TIMEZONE,
[AC_MSG_CHECKING(for struct timezone in sys/time.h and time.h)
AC_CACHE_VAL(bash_cv_struct_timezone,
[
AC_EGREP_HEADER(struct timezone, sys/time.h,
		bash_cv_struct_timezone=yes,
		AC_EGREP_HEADER(struct timezone, time.h,
			bash_cv_struct_timezone=yes,
			bash_cv_struct_timezone=no))
])
AC_MSG_RESULT($bash_cv_struct_timezone)
if test $bash_cv_struct_timezone = yes; then
  AC_DEFINE(HAVE_STRUCT_TIMEZONE)
fi
])

AC_DEFUN(BASH_STRUCT_WINSIZE,
[AC_MSG_CHECKING(for struct winsize in sys/ioctl.h and termios.h)
AC_CACHE_VAL(bash_cv_struct_winsize_header,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [struct winsize x;],
  bash_cv_struct_winsize_header=ioctl_h,
  [AC_TRY_COMPILE([#include <sys/types.h>
#include <termios.h>], [struct winsize x;],
  bash_cv_struct_winsize_header=termios_h, bash_cv_struct_winsize_header=other)
])])
if test $bash_cv_struct_winsize_header = ioctl_h; then
  AC_MSG_RESULT(sys/ioctl.h)
  AC_DEFINE(STRUCT_WINSIZE_IN_SYS_IOCTL)
elif test $bash_cv_struct_winsize_header = termios_h; then
  AC_MSG_RESULT(termios.h)
  AC_DEFINE(STRUCT_WINSIZE_IN_TERMIOS)
else
  AC_MSG_RESULT(not found)
fi
])

dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)
AC_DEFUN(BASH_SYS_SIGNAL_VINTAGE,
[AC_REQUIRE([AC_TYPE_SIGNAL])
AC_MSG_CHECKING(for type of signal functions)
AC_CACHE_VAL(bash_cv_signal_vintage,
[
  AC_TRY_LINK([#include <signal.h>],[
    sigset_t ss;
    struct sigaction sa;
    sigemptyset(&ss); sigsuspend(&ss);
    sigaction(SIGINT, &sa, (struct sigaction *) 0);
    sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
  ], bash_cv_signal_vintage=posix,
  [
    AC_TRY_LINK([#include <signal.h>], [
	int mask = sigmask(SIGINT);
	sigsetmask(mask); sigblock(mask); sigpause(mask);
    ], bash_cv_signal_vintage=4.2bsd,
    [
      AC_TRY_LINK([
	#include <signal.h>
	RETSIGTYPE foo() { }], [
		int mask = sigmask(SIGINT);
		sigset(SIGINT, foo); sigrelse(SIGINT);
		sighold(SIGINT); sigpause(SIGINT);
        ], bash_cv_signal_vintage=svr3, bash_cv_signal_vintage=v7
    )]
  )]
)
])
AC_MSG_RESULT($bash_cv_signal_vintage)
if test "$bash_cv_signal_vintage" = posix; then
AC_DEFINE(HAVE_POSIX_SIGNALS)
elif test "$bash_cv_signal_vintage" = "4.2bsd"; then
AC_DEFINE(HAVE_BSD_SIGNALS)
elif test "$bash_cv_signal_vintage" = svr3; then
AC_DEFINE(HAVE_USG_SIGHOLD)
fi
])

dnl Check if the pgrp of setpgrp() can't be the pid of a zombie process.
AC_DEFUN(BASH_SYS_PGRP_SYNC,
[AC_REQUIRE([AC_FUNC_GETPGRP])
AC_MSG_CHECKING(whether pgrps need synchronization)
AC_CACHE_VAL(bash_cv_pgrp_pipe,
[AC_TRY_RUN([
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
main()
{
# ifdef GETPGRP_VOID
#  define getpgID()	getpgrp()
# else
#  define getpgID()	getpgrp(0)
#  define setpgid(x,y)	setpgrp(x,y)
# endif
	int pid1, pid2, fds[2];
	int status;
	char ok;

	switch (pid1 = fork()) {
	  case -1:
	    exit(1);
	  case 0:
	    setpgid(0, getpid());
	    exit(0);
	}
	setpgid(pid1, pid1);

	sleep(2);	/* let first child die */

	if (pipe(fds) < 0)
	  exit(2);

	switch (pid2 = fork()) {
	  case -1:
	    exit(3);
	  case 0:
	    setpgid(0, pid1);
	    ok = getpgID() == pid1;
	    write(fds[1], &ok, 1);
	    exit(0);
	}
	setpgid(pid2, pid1);

	close(fds[1]);
	if (read(fds[0], &ok, 1) != 1)
	  exit(4);
	wait(&status);
	wait(&status);
	exit(ok ? 0 : 5);
}
], bash_cv_pgrp_pipe=no,bash_cv_pgrp_pipe=yes,
   [AC_MSG_WARN(cannot check pgrp synchronization if cross compiling -- defaulting to no)
    bash_cv_pgrp_pipe=no])
])
AC_MSG_RESULT($bash_cv_pgrp_pipe)
if test $bash_cv_pgrp_pipe = yes; then
AC_DEFINE(PGRP_PIPE)
fi
])

AC_DEFUN(BASH_SYS_REINSTALL_SIGHANDLERS,
[AC_REQUIRE([AC_TYPE_SIGNAL])
AC_REQUIRE([BASH_SYS_SIGNAL_VINTAGE])
AC_MSG_CHECKING([if signal handlers must be reinstalled when invoked])
AC_CACHE_VAL(bash_cv_must_reinstall_sighandlers,
[AC_TRY_RUN([
#include <signal.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

typedef RETSIGTYPE sigfunc();

int nsigint;

#ifdef HAVE_POSIX_SIGNALS
sigfunc *
set_signal_handler(sig, handler)
     int sig;
     sigfunc *handler;
{
  struct sigaction act, oact;
  act.sa_handler = handler;
  act.sa_flags = 0;
  sigemptyset (&act.sa_mask);
  sigemptyset (&oact.sa_mask);
  sigaction (sig, &act, &oact);
  return (oact.sa_handler);
}
#else
#define set_signal_handler(s, h) signal(s, h)
#endif

RETSIGTYPE
sigint(s)
int s;
{
  nsigint++;
}

main()
{
	nsigint = 0;
	set_signal_handler(SIGINT, sigint);
	kill((int)getpid(), SIGINT);
	kill((int)getpid(), SIGINT);
	exit(nsigint != 2);
}
], bash_cv_must_reinstall_sighandlers=no, bash_cv_must_reinstall_sighandlers=yes,
   [AC_MSG_WARN(cannot check signal handling if cross compiling -- defaulting to no)
    bash_cv_must_reinstall_sighandlers=no]
)])
AC_MSG_RESULT($bash_cv_must_reinstall_sighandlers)
if test $bash_cv_must_reinstall_sighandlers = yes; then
AC_DEFINE(MUST_REINSTALL_SIGHANDLERS)
fi
])

dnl check that some necessary job control definitions are present
AC_DEFUN(BASH_SYS_JOB_CONTROL_MISSING,
[AC_REQUIRE([BASH_SYS_SIGNAL_VINTAGE])
AC_MSG_CHECKING(for presence of necessary job control definitions)
AC_CACHE_VAL(bash_cv_job_control_missing,
[AC_TRY_RUN([
#include <sys/types.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>

/* Add more tests in here as appropriate. */
main()
{
/* signal type */
#if !defined (HAVE_POSIX_SIGNALS) && !defined (HAVE_BSD_SIGNALS)
exit(1);
#endif

/* signals and tty control. */
#if !defined (SIGTSTP) || !defined (SIGSTOP) || !defined (SIGCONT)
exit (1);
#endif

/* process control */
#if !defined (WNOHANG) || !defined (WUNTRACED) 
exit(1);
#endif

/* Posix systems have tcgetpgrp and waitpid. */
#if defined (_POSIX_VERSION) && !defined (HAVE_TCGETPGRP)
exit(1);
#endif

#if defined (_POSIX_VERSION) && !defined (HAVE_WAITPID)
exit(1);
#endif

/* Other systems have TIOCSPGRP/TIOCGPRGP and wait3. */
#if !defined (_POSIX_VERSION) && !defined (HAVE_WAIT3)
exit(1);
#endif

exit(0);
}], bash_cv_job_control_missing=present, bash_cv_job_control_missing=missing,
    [AC_MSG_WARN(cannot check job control if cross-compiling -- defaulting to missing)
     bash_cv_job_control_missing=missing]
)])
AC_MSG_RESULT($bash_cv_job_control_missing)
if test $bash_cv_job_control_missing = missing; then
AC_DEFINE(JOB_CONTROL_MISSING)
fi
])

dnl check whether named pipes are present
dnl this requires a previous check for mkfifo, but that is awkward to specify
AC_DEFUN(BASH_SYS_NAMED_PIPES,
[AC_MSG_CHECKING(for presence of named pipes)
AC_CACHE_VAL(bash_cv_sys_named_pipes,
[AC_TRY_RUN([
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Add more tests in here as appropriate. */
main()
{
int fd, err;

#if defined (HAVE_MKFIFO)
exit (0);
#endif

#if !defined (S_IFIFO) && (defined (_POSIX_VERSION) && !defined (S_ISFIFO))
exit (1);
#endif

#if defined (NeXT)
exit (1);
#endif
err = mkdir("/tmp/bash-aclocal", 0700);
if (err < 0) {
  perror ("mkdir");
  exit(1);
}
fd = mknod ("/tmp/bash-aclocal/sh-np-autoconf", 0666 | S_IFIFO, 0);
if (fd == -1) {
  rmdir ("/tmp/bash-aclocal");
  exit (1);
}
close(fd);
unlink ("/tmp/bash-aclocal/sh-np-autoconf");
rmdir ("/tmp/bash-aclocal");
exit(0);
}], bash_cv_sys_named_pipes=present, bash_cv_sys_named_pipes=missing,
    [AC_MSG_WARN(cannot check for named pipes if cross-compiling -- defaulting to missing)
     bash_cv_sys_named_pipes=missing]
)])
AC_MSG_RESULT($bash_cv_sys_named_pipes)
if test $bash_cv_sys_named_pipes = missing; then
AC_DEFINE(NAMED_PIPES_MISSING)
fi
])

AC_DEFUN(BASH_SYS_DEFAULT_MAIL_DIR,
[AC_MSG_CHECKING(for default mail directory)
AC_CACHE_VAL(bash_cv_mail_dir,
[if test -d /var/mail; then
   bash_cv_mail_dir=/var/mail
 elif test -d /var/spool/mail; then
   bash_cv_mail_dir=/var/spool/mail
 elif test -d /usr/mail; then
   bash_cv_mail_dir=/usr/mail
 elif test -d /usr/spool/mail; then
   bash_cv_mail_dir=/usr/spool/mail
 else
   bash_cv_mail_dir=unknown
 fi
])
AC_MSG_RESULT($bash_cv_mail_dir)
AC_DEFINE_UNQUOTED(DEFAULT_MAIL_DIRECTORY, "$bash_cv_mail_dir")
])

AC_DEFUN(BASH_HAVE_TIOCGWINSZ,
[AC_MSG_CHECKING(for TIOCGWINSZ in sys/ioctl.h)
AC_CACHE_VAL(bash_cv_tiocgwinsz_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCGWINSZ;],
  bash_cv_tiocgwinsz_in_ioctl=yes,bash_cv_tiocgwinsz_in_ioctl=no)])
AC_MSG_RESULT($bash_cv_tiocgwinsz_in_ioctl)
if test $bash_cv_tiocgwinsz_in_ioctl = yes; then   
AC_DEFINE(GWINSZ_IN_SYS_IOCTL)
fi
])

AC_DEFUN(BASH_HAVE_TIOCSTAT,
[AC_MSG_CHECKING(for TIOCSTAT in sys/ioctl.h)
AC_CACHE_VAL(bash_cv_tiocstat_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCSTAT;],
  bash_cv_tiocstat_in_ioctl=yes,bash_cv_tiocstat_in_ioctl=no)])
AC_MSG_RESULT($bash_cv_tiocstat_in_ioctl)
if test $bash_cv_tiocstat_in_ioctl = yes; then   
AC_DEFINE(TIOCSTAT_IN_SYS_IOCTL)
fi
])

AC_DEFUN(BASH_HAVE_FIONREAD,
[AC_MSG_CHECKING(for FIONREAD in sys/ioctl.h)
AC_CACHE_VAL(bash_cv_fionread_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = FIONREAD;],
  bash_cv_fionread_in_ioctl=yes,bash_cv_fionread_in_ioctl=no)])
AC_MSG_RESULT($bash_cv_fionread_in_ioctl)
if test $bash_cv_fionread_in_ioctl = yes; then   
AC_DEFINE(FIONREAD_IN_SYS_IOCTL)
fi
])

dnl
dnl See if speed_t is declared in <sys/types.h>.  Some versions of linux
dnl require a definition of speed_t each time <termcap.h> is included,
dnl but you can only get speed_t if you include <termios.h> (on some
dnl versions) or <sys/types.h> (on others).
dnl
AC_DEFUN(BASH_CHECK_SPEED_T,
[AC_MSG_CHECKING(for speed_t in sys/types.h)
AC_CACHE_VAL(bash_cv_speed_t_in_sys_types,
[AC_TRY_COMPILE([#include <sys/types.h>], [speed_t x;],
  bash_cv_speed_t_in_sys_types=yes,bash_cv_speed_t_in_sys_types=no)])
AC_MSG_RESULT($bash_cv_speed_t_in_sys_types)
if test $bash_cv_speed_t_in_sys_types = yes; then   
AC_DEFINE(SPEED_T_IN_SYS_TYPES)
fi
])

AC_DEFUN(BASH_CHECK_GETPW_FUNCS,
[AC_MSG_CHECKING(whether getpw functions are declared in pwd.h)
AC_CACHE_VAL(bash_cv_getpw_declared,
[AC_EGREP_CPP(getpwuid,
[
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <pwd.h>
],
bash_cv_getpw_declared=yes,bash_cv_getpw_declared=no)])
AC_MSG_RESULT($bash_cv_getpw_declared)
if test $bash_cv_getpw_declared = yes; then
AC_DEFINE(HAVE_GETPW_DECLS)
fi
])

AC_DEFUN(BASH_CHECK_DEV_FD,
[AC_MSG_CHECKING(whether /dev/fd is available)
AC_CACHE_VAL(bash_cv_dev_fd,
[bash_cv_dev_fd=""
if test -d /dev/fd  && test -r /dev/fd/0 < /dev/null; then
# check for systems like FreeBSD 5 that only provide /dev/fd/[012]
   exec 3</dev/null
   if test -r /dev/fd/3; then
     bash_cv_dev_fd=standard
   else
     bash_cv_dev_fd=absent
   fi
   exec 3<&-
fi
if test -z "$bash_cv_dev_fd" ; then 
  if test -d /proc/self/fd && test -r /proc/self/fd/0 < /dev/null; then
    bash_cv_dev_fd=whacky
  else
    bash_cv_dev_fd=absent
  fi
fi
])
AC_MSG_RESULT($bash_cv_dev_fd)
if test $bash_cv_dev_fd = "standard"; then
  AC_DEFINE(HAVE_DEV_FD)
  AC_DEFINE(DEV_FD_PREFIX, "/dev/fd/")
elif test $bash_cv_dev_fd = "whacky"; then
  AC_DEFINE(HAVE_DEV_FD)
  AC_DEFINE(DEV_FD_PREFIX, "/proc/self/fd/")
fi
])

AC_DEFUN(BASH_CHECK_DEV_STDIN,
[AC_MSG_CHECKING(whether /dev/stdin stdout stderr are available)
AC_CACHE_VAL(bash_cv_dev_stdin,
[if test -d /dev/fd && test -r /dev/stdin < /dev/null; then
   bash_cv_dev_stdin=present
 elif test -d /proc/self/fd && test -r /dev/stdin < /dev/null; then
   bash_cv_dev_stdin=present
 else
   bash_cv_dev_stdin=absent
 fi
])
AC_MSG_RESULT($bash_cv_dev_stdin)
if test $bash_cv_dev_stdin = "present"; then
  AC_DEFINE(HAVE_DEV_STDIN)
fi
])

dnl
dnl Check if HPUX needs _KERNEL defined for RLIMIT_* definitions
dnl
AC_DEFUN(BASH_CHECK_KERNEL_RLIMIT,
[AC_MSG_CHECKING([whether $host_os needs _KERNEL for RLIMIT defines])
AC_CACHE_VAL(bash_cv_kernel_rlimit,
[AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/resource.h>
],
[
  int f;
  f = RLIMIT_DATA;
], bash_cv_kernel_rlimit=no,
[AC_TRY_COMPILE([
#include <sys/types.h>
#define _KERNEL
#include <sys/resource.h>
#undef _KERNEL
],
[
	int f;
        f = RLIMIT_DATA;
], bash_cv_kernel_rlimit=yes, bash_cv_kernel_rlimit=no)]
)])
AC_MSG_RESULT($bash_cv_kernel_rlimit)
if test $bash_cv_kernel_rlimit = yes; then
AC_DEFINE(RLIMIT_NEEDS_KERNEL)
fi
])

dnl
dnl Check for 64-bit off_t -- used for malloc alignment
dnl
dnl C does not allow duplicate case labels, so the compile will fail if
dnl sizeof(off_t) is > 4.
dnl
AC_DEFUN(BASH_CHECK_OFF_T_64,
[AC_CACHE_CHECK(for 64-bit off_t, bash_cv_off_t_64,
AC_TRY_COMPILE([
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
],[
switch (0) case 0: case (sizeof (off_t) <= 4):;
], bash_cv_off_t_64=no, bash_cv_off_t_64=yes))
if test $bash_cv_off_t_64 = yes; then
        AC_DEFINE(HAVE_OFF_T_64)
fi])

AC_DEFUN(BASH_CHECK_RTSIGS,
[AC_MSG_CHECKING(for unusable real-time signals due to large values)
AC_CACHE_VAL(bash_cv_unusable_rtsigs,
[AC_TRY_RUN([
#include <sys/types.h>
#include <signal.h>

#ifndef NSIG
#  define NSIG 64
#endif

main ()
{
  int n_sigs = 2 * NSIG;
#ifdef SIGRTMIN
  int rtmin = SIGRTMIN;
#else
  int rtmin = 0;
#endif

  exit(rtmin < n_sigs);
}], bash_cv_unusable_rtsigs=yes, bash_cv_unusable_rtsigs=no,
    [AC_MSG_WARN(cannot check real-time signals if cross compiling -- defaulting to yes)
     bash_cv_unusable_rtsigs=yes]
)])
AC_MSG_RESULT($bash_cv_unusable_rtsigs)
if test $bash_cv_unusable_rtsigs = yes; then
AC_DEFINE(UNUSABLE_RT_SIGNALS)
fi
])

dnl
dnl check for availability of multibyte characters and functions
dnl
dnl geez, I wish I didn't have to check for all of this stuff separately
dnl
AC_DEFUN(BASH_CHECK_MULTIBYTE,
[
AC_CHECK_HEADERS(wctype.h)
AC_CHECK_HEADERS(wchar.h)
AC_CHECK_HEADERS(langinfo.h)

AC_CHECK_FUNC(mbsrtowcs, AC_DEFINE(HAVE_MBSRTOWCS))
AC_CHECK_FUNC(mbrlen, AC_DEFINE(HAVE_MBRLEN))

AC_CHECK_FUNC(wcrtomb, AC_DEFINE(HAVE_WCRTOMB))
AC_CHECK_FUNC(wcscoll, AC_DEFINE(HAVE_WCSCOLL))
AC_CHECK_FUNC(wcsdup, AC_DEFINE(HAVE_WCSDUP))
AC_CHECK_FUNC(wcwidth, AC_DEFINE(HAVE_WCWIDTH))
AC_CHECK_FUNC(wctype, AC_DEFINE(HAVE_WCTYPE))

dnl checks for both mbrtowc and mbstate_t
AC_FUNC_MBRTOWC
if test $ac_cv_func_mbrtowc = yes; then
	AC_DEFINE(HAVE_MBSTATE_T)
fi

AC_CHECK_FUNCS(iswlower iswupper towlower towupper iswctype)

AC_CACHE_CHECK([for nl_langinfo and CODESET], bash_cv_langinfo_codeset,
[AC_TRY_LINK(
[#include <langinfo.h>],
[char* cs = nl_langinfo(CODESET);],
bash_cv_langinfo_codeset=yes, bash_cv_langinfo_codeset=no)])
if test $bash_cv_langinfo_codeset = yes; then
  AC_DEFINE(HAVE_LANGINFO_CODESET)
fi

dnl check for wchar_t in <wchar.h>
AC_CACHE_CHECK([for wchar_t in wchar.h], bash_cv_type_wchar_t,
[AC_TRY_COMPILE(
[#include <wchar.h>
],
[
        wchar_t foo;
        foo = 0;
], bash_cv_type_wchar_t=yes, bash_cv_type_wchar_t=no)])
if test $bash_cv_type_wchar_t = yes; then
        AC_DEFINE(HAVE_WCHAR_T, 1, [systems should define this type here])
fi

dnl check for wctype_t in <wctype.h>
AC_CACHE_CHECK([for wctype_t in wctype.h], bash_cv_type_wctype_t,
[AC_TRY_COMPILE(
[#include <wctype.h>],
[
        wctype_t foo;
        foo = 0;
], bash_cv_type_wctype_t=yes, bash_cv_type_wctype_t=no)])
if test $bash_cv_type_wctype_t = yes; then
        AC_DEFINE(HAVE_WCTYPE_T, 1, [systems should define this type here])
fi

dnl check for wint_t in <wctype.h>
AC_CACHE_CHECK([for wint_t in wctype.h], bash_cv_type_wint_t,
[AC_TRY_COMPILE(
[#include <wctype.h>],
[
        wint_t foo;
        foo = 0;
], bash_cv_type_wint_t=yes, bash_cv_type_wint_t=no)])
if test $bash_cv_type_wint_t = yes; then
        AC_DEFINE(HAVE_WINT_T, 1, [systems should define this type here])
fi

])

dnl need: prefix exec_prefix libdir includedir CC TERMCAP_LIB
dnl require:
dnl	AC_PROG_CC
dnl	BASH_CHECK_LIB_TERMCAP

AC_DEFUN([RL_LIB_READLINE_VERSION],
[
AC_REQUIRE([BASH_CHECK_LIB_TERMCAP])

AC_MSG_CHECKING([version of installed readline library])

# What a pain in the ass this is.

# save cpp and ld options
_save_CFLAGS="$CFLAGS"
_save_LDFLAGS="$LDFLAGS"
_save_LIBS="$LIBS"

# Don't set ac_cv_rl_prefix if the caller has already assigned a value.  This
# allows the caller to do something like $_rl_prefix=$withval if the user
# specifies --with-installed-readline=PREFIX as an argument to configure

if test -z "$ac_cv_rl_prefix"; then
test "x$prefix" = xNONE && ac_cv_rl_prefix=$ac_default_prefix || ac_cv_rl_prefix=${prefix}
fi

eval ac_cv_rl_includedir=${ac_cv_rl_prefix}/include
eval ac_cv_rl_libdir=${ac_cv_rl_prefix}/lib

LIBS="$LIBS -lreadline ${TERMCAP_LIB}"
CFLAGS="$CFLAGS -I${ac_cv_rl_includedir}"
LDFLAGS="$LDFLAGS -L${ac_cv_rl_libdir}"

AC_CACHE_VAL(ac_cv_rl_version,
[AC_TRY_RUN([
#include <stdio.h>
#include <readline/readline.h>

extern int rl_gnu_readline_p;

main()
{
	FILE *fp;
	fp = fopen("conftest.rlv", "w");
	if (fp == 0)
		exit(1);
	if (rl_gnu_readline_p != 1)
		fprintf(fp, "0.0\n");
	else
		fprintf(fp, "%s\n", rl_library_version ? rl_library_version : "0.0");
	fclose(fp);
	exit(0);
}
],
ac_cv_rl_version=`cat conftest.rlv`,
ac_cv_rl_version='0.0',
ac_cv_rl_version='4.2')])

CFLAGS="$_save_CFLAGS"
LDFLAGS="$_save_LDFLAGS"
LIBS="$_save_LIBS"

RL_MAJOR=0
RL_MINOR=0

# (
case "$ac_cv_rl_version" in
2*|3*|4*|5*|6*|7*|8*|9*)
	RL_MAJOR=`echo $ac_cv_rl_version | sed 's:\..*$::'`
	RL_MINOR=`echo $ac_cv_rl_version | sed -e 's:^.*\.::' -e 's:[[a-zA-Z]]*$::'`
	;;
esac

# (((
case $RL_MAJOR in
[[0-9][0-9]])	_RL_MAJOR=$RL_MAJOR ;;
[[0-9]])	_RL_MAJOR=0$RL_MAJOR ;;
*)		_RL_MAJOR=00 ;;
esac

# (((
case $RL_MINOR in
[[0-9][0-9]])	_RL_MINOR=$RL_MINOR ;;
[[0-9]])	_RL_MINOR=0$RL_MINOR ;;
*)		_RL_MINOR=00 ;;
esac

RL_VERSION="0x${_RL_MAJOR}${_RL_MINOR}"

# Readline versions greater than 4.2 have these defines in readline.h

if test $ac_cv_rl_version = '0.0' ; then
	AC_MSG_WARN([Could not test version of installed readline library.])
elif test $RL_MAJOR -gt 4 || { test $RL_MAJOR = 4 && test $RL_MINOR -gt 2 ; } ; then
	# set these for use by the caller
	RL_PREFIX=$ac_cv_rl_prefix
	RL_LIBDIR=$ac_cv_rl_libdir
	RL_INCLUDEDIR=$ac_cv_rl_includedir
	AC_MSG_RESULT($ac_cv_rl_version)
else

AC_DEFINE_UNQUOTED(RL_READLINE_VERSION, $RL_VERSION, [encoded version of the installed readline library])
AC_DEFINE_UNQUOTED(RL_VERSION_MAJOR, $RL_MAJOR, [major version of installed readline library])
AC_DEFINE_UNQUOTED(RL_VERSION_MINOR, $RL_MINOR, [minor version of installed readline library])

AC_SUBST(RL_VERSION)
AC_SUBST(RL_MAJOR)
AC_SUBST(RL_MINOR)

# set these for use by the caller
RL_PREFIX=$ac_cv_rl_prefix
RL_LIBDIR=$ac_cv_rl_libdir
RL_INCLUDEDIR=$ac_cv_rl_includedir

AC_MSG_RESULT($ac_cv_rl_version)

fi
])

AC_DEFUN(BASH_FUNC_CTYPE_NONASCII,
[
AC_MSG_CHECKING(whether the ctype macros accept non-ascii characters)
AC_CACHE_VAL(bash_cv_func_ctype_nonascii,
[AC_TRY_RUN([
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <stdio.h>
#include <ctype.h>

main(c, v)
int	c;
char	*v[];
{
	char	*deflocale;
	unsigned char x;
	int	r1, r2;

#ifdef HAVE_SETLOCALE
	/* We take a shot here.  If that locale is not known, try the
	   system default.  We try this one because '\342' (226) is
	   known to be a printable character in that locale. */
	deflocale = setlocale(LC_ALL, "en_US.ISO8859-1");
	if (deflocale == 0)
		deflocale = setlocale(LC_ALL, "");
#endif

	x = '\342';
	r1 = isprint(x);
	x -= 128;
	r2 = isprint(x);
	exit (r1 == 0 || r2 == 0);
}
], bash_cv_func_ctype_nonascii=yes, bash_cv_func_ctype_nonascii=no,
   [AC_MSG_WARN(cannot check ctype macros if cross compiling -- defaulting to no)
    bash_cv_func_ctype_nonascii=no]
)])
AC_MSG_RESULT($bash_cv_func_ctype_nonascii)
if test $bash_cv_func_ctype_nonascii = yes; then
AC_DEFINE(CTYPE_NON_ASCII)
fi
])

AC_DEFUN(BASH_CHECK_WCONTINUED,
[
AC_MSG_CHECKING(whether WCONTINUED flag to waitpid is unavailable or available but broken)
AC_CACHE_VAL(bash_cv_wcontinued_broken,
[AC_TRY_RUN([
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#ifndef errno
extern int errno;
#endif
main()
{
	int	x;

	x = waitpid(-1, (int *)0, WNOHANG|WCONTINUED);
	if (x == -1 && errno == EINVAL)
		exit (1);
	else
		exit (0);
}
], bash_cv_wcontinued_broken=no,bash_cv_wcontinued_broken=yes,
   [AC_MSG_WARN(cannot check WCONTINUED if cross compiling -- defaulting to no)
    bash_cv_wcontinued_broken=no]
)])
AC_MSG_RESULT($bash_cv_wcontinued_broken)
if test $bash_cv_wcontinued_broken = yes; then
AC_DEFINE(WCONTINUED_BROKEN)
fi
])

dnl
dnl tests added for bashdb
dnl


AC_DEFUN([AM_PATH_LISPDIR],
 [AC_ARG_WITH(lispdir, AC_HELP_STRING([--with-lispdir], [override the default lisp directory]),
  [ lispdir="$withval" 
    AC_MSG_CHECKING([where .elc files should go])
    AC_MSG_RESULT([$lispdir])],
  [
  # If set to t, that means we are running in a shell under Emacs.
  # If you have an Emacs named "t", then use the full path.
  test x"$EMACS" = xt && EMACS=
  AC_CHECK_PROGS(EMACS, emacs xemacs, no)
  if test $EMACS != "no"; then
    if test x${lispdir+set} != xset; then
      AC_CACHE_CHECK([where .elc files should go], [am_cv_lispdir], [dnl
	am_cv_lispdir=`$EMACS -batch -q -eval '(while load-path (princ (concat (car load-path) "\n")) (setq load-path (cdr load-path)))' | sed -n -e 's,/$,,' -e '/.*\/lib\/\(x\?emacs\/site-lisp\)$/{s,,${libdir}/\1,;p;q;}' -e '/.*\/share\/\(x\?emacs\/site-lisp\)$/{s,,${datadir}/\1,;p;q;}'`
	if test -z "$am_cv_lispdir"; then
	  am_cv_lispdir='${datadir}/emacs/site-lisp'
	fi
      ])
      lispdir="$am_cv_lispdir"
    fi
  fi
 ])
 AC_SUBST(lispdir)
])

dnl
dnl tests added for gettext
dnl
# codeset.m4 serial AM1 (gettext-0.10.40)
dnl Copyright (C) 2000-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.

AC_DEFUN([AM_LANGINFO_CODESET],
[
  AC_CACHE_CHECK([for nl_langinfo and CODESET], am_cv_langinfo_codeset,
    [AC_TRY_LINK([#include <langinfo.h>],
      [char* cs = nl_langinfo(CODESET);],
      am_cv_langinfo_codeset=yes,
      am_cv_langinfo_codeset=no)
    ])
  if test $am_cv_langinfo_codeset = yes; then
    AC_DEFINE(HAVE_LANGINFO_CODESET, 1,
      [Define if you have <langinfo.h> and nl_langinfo(CODESET).])
  fi
])
# gettext.m4 serial 20 (gettext-0.12)
dnl Copyright (C) 1995-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.
dnl
dnl This file can can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2003.

dnl Macro to add for using GNU gettext.

dnl Usage: AM_GNU_GETTEXT([INTLSYMBOL], [NEEDSYMBOL], [INTLDIR]).
dnl INTLSYMBOL can be one of 'external', 'no-libtool', 'use-libtool'. The
dnl    default (if it is not specified or empty) is 'no-libtool'.
dnl    INTLSYMBOL should be 'external' for packages with no intl directory,
dnl    and 'no-libtool' or 'use-libtool' for packages with an intl directory.
dnl    If INTLSYMBOL is 'use-libtool', then a libtool library
dnl    $(top_builddir)/intl/libintl.la will be created (shared and/or static,
dnl    depending on --{enable,disable}-{shared,static} and on the presence of
dnl    AM-DISABLE-SHARED). If INTLSYMBOL is 'no-libtool', a static library
dnl    $(top_builddir)/intl/libintl.a will be created.
dnl If NEEDSYMBOL is specified and is 'need-ngettext', then GNU gettext
dnl    implementations (in libc or libintl) without the ngettext() function
dnl    will be ignored.  If NEEDSYMBOL is specified and is
dnl    'need-formatstring-macros', then GNU gettext implementations that don't
dnl    support the ISO C 99 <inttypes.h> formatstring macros will be ignored.
dnl INTLDIR is used to find the intl libraries.  If empty,
dnl    the value `$(top_builddir)/intl/' is used.
dnl
dnl The result of the configuration is one of three cases:
dnl 1) GNU gettext, as included in the intl subdirectory, will be compiled
dnl    and used.
dnl    Catalog format: GNU --> install in $(datadir)
dnl    Catalog extension: .mo after installation, .gmo in source tree
dnl 2) GNU gettext has been found in the system's C library.
dnl    Catalog format: GNU --> install in $(datadir)
dnl    Catalog extension: .mo after installation, .gmo in source tree
dnl 3) No internationalization, always use English msgid.
dnl    Catalog format: none
dnl    Catalog extension: none
dnl If INTLSYMBOL is 'external', only cases 2 and 3 can occur.
dnl The use of .gmo is historical (it was needed to avoid overwriting the
dnl GNU format catalogs when building on a platform with an X/Open gettext),
dnl but we keep it in order not to force irrelevant filename changes on the
dnl maintainers.
dnl
AC_DEFUN([AM_GNU_GETTEXT],
[
  dnl Argument checking.
  ifelse([$1], [], , [ifelse([$1], [external], , [ifelse([$1], [no-libtool], , [ifelse([$1], [use-libtool], ,
    [errprint([ERROR: invalid first argument to AM_GNU_GETTEXT
])])])])])
  ifelse([$2], [], , [ifelse([$2], [need-ngettext], , [ifelse([$2], [need-formatstring-macros], ,
    [errprint([ERROR: invalid second argument to AM_GNU_GETTEXT
])])])])
  define(gt_included_intl, ifelse([$1], [external], [no], [yes]))
  define(gt_libtool_suffix_prefix, ifelse([$1], [use-libtool], [l], []))

  AC_REQUIRE([AM_PO_SUBDIRS])dnl
  ifelse(gt_included_intl, yes, [
    AC_REQUIRE([AM_INTL_SUBDIR])dnl
  ])

  dnl Prerequisites of AC_LIB_LINKFLAGS_BODY.
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])

  dnl Sometimes libintl requires libiconv, so first search for libiconv.
  dnl Ideally we would do this search only after the
  dnl      if test "$USE_NLS" = "yes"; then
  dnl        if test "$gt_cv_func_gnugettext_libc" != "yes"; then
  dnl tests. But if configure.in invokes AM_ICONV after AM_GNU_GETTEXT
  dnl the configure script would need to contain the same shell code
  dnl again, outside any 'if'. There are two solutions:
  dnl - Invoke AM_ICONV_LINKFLAGS_BODY here, outside any 'if'.
  dnl - Control the expansions in more detail using AC_PROVIDE_IFELSE.
  dnl Since AC_PROVIDE_IFELSE is only in autoconf >= 2.52 and not
  dnl documented, we avoid it.
  ifelse(gt_included_intl, yes, , [
    AC_REQUIRE([AM_ICONV_LINKFLAGS_BODY])
  ])

  dnl Set USE_NLS.
  AM_NLS

  ifelse(gt_included_intl, yes, [
    BUILD_INCLUDED_LIBINTL=no
    USE_INCLUDED_LIBINTL=no
  ])
  LIBINTL=
  LTLIBINTL=
  POSUB=

  dnl If we use NLS figure out what method
  if test "$USE_NLS" = "yes"; then
    gt_use_preinstalled_gnugettext=no
    ifelse(gt_included_intl, yes, [
      AC_MSG_CHECKING([whether included gettext is requested])
      AC_ARG_WITH(included-gettext,
        [  --with-included-gettext use the GNU gettext library included here],
        nls_cv_force_use_gnu_gettext=$withval,
        nls_cv_force_use_gnu_gettext=no)
      AC_MSG_RESULT($nls_cv_force_use_gnu_gettext)

      nls_cv_use_gnu_gettext="$nls_cv_force_use_gnu_gettext"
      if test "$nls_cv_force_use_gnu_gettext" != "yes"; then
    ])
        dnl User does not insist on using GNU NLS library.  Figure out what
        dnl to use.  If GNU gettext is available we use this.  Else we have
        dnl to fall back to GNU NLS library.

        dnl Add a version number to the cache macros.
        define([gt_api_version], ifelse([$2], [need-formatstring-macros], 3, ifelse([$2], [need-ngettext], 2, 1)))
        define([gt_cv_func_gnugettext_libc], [gt_cv_func_gnugettext]gt_api_version[_libc])
        define([gt_cv_func_gnugettext_libintl], [gt_cv_func_gnugettext]gt_api_version[_libintl])

        AC_CACHE_CHECK([for GNU gettext in libc], gt_cv_func_gnugettext_libc,
         [AC_TRY_LINK([#include <libintl.h>
]ifelse([$2], [need-formatstring-macros],
[#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
#define __GNU_GETTEXT_SUPPORTED_REVISION(major) ((major) == 0 ? 0 : -1)
#endif
changequote(,)dnl
typedef int array [2 * (__GNU_GETTEXT_SUPPORTED_REVISION(0) >= 1) - 1];
changequote([,])dnl
], [])[extern int _nl_msg_cat_cntr;
extern int *_nl_domain_bindings;],
            [bindtextdomain ("", "");
return (int) gettext ("")]ifelse([$2], [need-ngettext], [ + (int) ngettext ("", "", 0)], [])[ + _nl_msg_cat_cntr + *_nl_domain_bindings],
            gt_cv_func_gnugettext_libc=yes,
            gt_cv_func_gnugettext_libc=no)])

        if test "$gt_cv_func_gnugettext_libc" != "yes"; then
          dnl Sometimes libintl requires libiconv, so first search for libiconv.
          ifelse(gt_included_intl, yes, , [
            AM_ICONV_LINK
          ])
          dnl Search for libintl and define LIBINTL, LTLIBINTL and INCINTL
          dnl accordingly. Don't use AC_LIB_LINKFLAGS_BODY([intl],[iconv])
          dnl because that would add "-liconv" to LIBINTL and LTLIBINTL
          dnl even if libiconv doesn't exist.
          AC_LIB_LINKFLAGS_BODY([intl])
          AC_CACHE_CHECK([for GNU gettext in libintl],
            gt_cv_func_gnugettext_libintl,
           [gt_save_CPPFLAGS="$CPPFLAGS"
            CPPFLAGS="$CPPFLAGS $INCINTL"
            gt_save_LIBS="$LIBS"
            LIBS="$LIBS $LIBINTL"
            dnl Now see whether libintl exists and does not depend on libiconv.
            AC_TRY_LINK([#include <libintl.h>
]ifelse([$2], [need-formatstring-macros],
[#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
#define __GNU_GETTEXT_SUPPORTED_REVISION(major) ((major) == 0 ? 0 : -1)
#endif
changequote(,)dnl
typedef int array [2 * (__GNU_GETTEXT_SUPPORTED_REVISION(0) >= 1) - 1];
changequote([,])dnl
], [])[extern int _nl_msg_cat_cntr;
extern
#ifdef __cplusplus
"C"
#endif
const char *_nl_expand_alias ();],
              [bindtextdomain ("", "");
return (int) gettext ("")]ifelse([$2], [need-ngettext], [ + (int) ngettext ("", "", 0)], [])[ + _nl_msg_cat_cntr + *_nl_expand_alias (0)],
              gt_cv_func_gnugettext_libintl=yes,
              gt_cv_func_gnugettext_libintl=no)
            dnl Now see whether libintl exists and depends on libiconv.
            if test "$gt_cv_func_gnugettext_libintl" != yes && test -n "$LIBICONV"; then
              LIBS="$LIBS $LIBICONV"
              AC_TRY_LINK([#include <libintl.h>
]ifelse([$2], [need-formatstring-macros],
[#ifndef __GNU_GETTEXT_SUPPORTED_REVISION
#define __GNU_GETTEXT_SUPPORTED_REVISION(major) ((major) == 0 ? 0 : -1)
#endif
changequote(,)dnl
typedef int array [2 * (__GNU_GETTEXT_SUPPORTED_REVISION(0) >= 1) - 1];
changequote([,])dnl
], [])[extern int _nl_msg_cat_cntr;
extern
#ifdef __cplusplus
"C"
#endif
const char *_nl_expand_alias ();],
                [bindtextdomain ("", "");
return (int) gettext ("")]ifelse([$2], [need-ngettext], [ + (int) ngettext ("", "", 0)], [])[ + _nl_msg_cat_cntr + *_nl_expand_alias (0)],
               [LIBINTL="$LIBINTL $LIBICONV"
                LTLIBINTL="$LTLIBINTL $LTLIBICONV"
                gt_cv_func_gnugettext_libintl=yes
               ])
            fi
            CPPFLAGS="$gt_save_CPPFLAGS"
            LIBS="$gt_save_LIBS"])
        fi

        dnl If an already present or preinstalled GNU gettext() is found,
        dnl use it.  But if this macro is used in GNU gettext, and GNU
        dnl gettext is already preinstalled in libintl, we update this
        dnl libintl.  (Cf. the install rule in intl/Makefile.in.)
        if test "$gt_cv_func_gnugettext_libc" = "yes" \
           || { test "$gt_cv_func_gnugettext_libintl" = "yes" \
                && test "$PACKAGE" != gettext-runtime \
                && test "$PACKAGE" != gettext-tools; }; then
          gt_use_preinstalled_gnugettext=yes
        else
          dnl Reset the values set by searching for libintl.
          LIBINTL=
          LTLIBINTL=
          INCINTL=
        fi

    ifelse(gt_included_intl, yes, [
        if test "$gt_use_preinstalled_gnugettext" != "yes"; then
          dnl GNU gettext is not found in the C library.
          dnl Fall back on included GNU gettext library.
          nls_cv_use_gnu_gettext=yes
        fi
      fi

      if test "$nls_cv_use_gnu_gettext" = "yes"; then
        dnl Mark actions used to generate GNU NLS library.
        BUILD_INCLUDED_LIBINTL=yes
        USE_INCLUDED_LIBINTL=yes
        LIBINTL="ifelse([$3],[],\${top_builddir}/intl,[$3])/libintl.[]gt_libtool_suffix_prefix[]a $LIBICONV"
        LTLIBINTL="ifelse([$3],[],\${top_builddir}/intl,[$3])/libintl.[]gt_libtool_suffix_prefix[]a $LTLIBICONV"
        LIBS=`echo " $LIBS " | sed -e 's/ -lintl / /' -e 's/^ //' -e 's/ $//'`
      fi

      if test "$gt_use_preinstalled_gnugettext" = "yes" \
         || test "$nls_cv_use_gnu_gettext" = "yes"; then
        dnl Mark actions to use GNU gettext tools.
        CATOBJEXT=.gmo
      fi
    ])

    if test "$gt_use_preinstalled_gnugettext" = "yes" \
       || test "$nls_cv_use_gnu_gettext" = "yes"; then
      AC_DEFINE(ENABLE_NLS, 1,
        [Define to 1 if translation of program messages to the user's native language
   is requested.])
    else
      USE_NLS=no
    fi
  fi

  AC_MSG_CHECKING([whether to use NLS])
  AC_MSG_RESULT([$USE_NLS])
  if test "$USE_NLS" = "yes"; then
    AC_MSG_CHECKING([where the gettext function comes from])
    if test "$gt_use_preinstalled_gnugettext" = "yes"; then
      if test "$gt_cv_func_gnugettext_libintl" = "yes"; then
        gt_source="external libintl"
      else
        gt_source="libc"
      fi
    else
      gt_source="included intl directory"
    fi
    AC_MSG_RESULT([$gt_source])
  fi

  if test "$USE_NLS" = "yes"; then

    if test "$gt_use_preinstalled_gnugettext" = "yes"; then
      if test "$gt_cv_func_gnugettext_libintl" = "yes"; then
        AC_MSG_CHECKING([how to link with libintl])
        AC_MSG_RESULT([$LIBINTL])
        AC_LIB_APPENDTOVAR([CPPFLAGS], [$INCINTL])
      fi

      dnl For backward compatibility. Some packages may be using this.
      AC_DEFINE(HAVE_GETTEXT, 1,
       [Define if the GNU gettext() function is already present or preinstalled.])
      AC_DEFINE(HAVE_DCGETTEXT, 1,
       [Define if the GNU dcgettext() function is already present or preinstalled.])
    fi

    dnl We need to process the po/ directory.
    POSUB=po
  fi

  ifelse(gt_included_intl, yes, [
    dnl If this is used in GNU gettext we have to set BUILD_INCLUDED_LIBINTL
    dnl to 'yes' because some of the testsuite requires it.
    if test "$PACKAGE" = gettext-runtime || test "$PACKAGE" = gettext-tools; then
      BUILD_INCLUDED_LIBINTL=yes
    fi

    dnl Make all variables we use known to autoconf.
    AC_SUBST(BUILD_INCLUDED_LIBINTL)
    AC_SUBST(USE_INCLUDED_LIBINTL)
    AC_SUBST(CATOBJEXT)

    dnl For backward compatibility. Some configure.ins may be using this.
    nls_cv_header_intl=
    nls_cv_header_libgt=

    dnl For backward compatibility. Some Makefiles may be using this.
    DATADIRNAME=share
    AC_SUBST(DATADIRNAME)

    dnl For backward compatibility. Some Makefiles may be using this.
    INSTOBJEXT=.mo
    AC_SUBST(INSTOBJEXT)

    dnl For backward compatibility. Some Makefiles may be using this.
    GENCAT=gencat
    AC_SUBST(GENCAT)

    dnl For backward compatibility. Some Makefiles may be using this.
    if test "$USE_INCLUDED_LIBINTL" = yes; then
      INTLOBJS="\$(GETTOBJS)"
    fi
    AC_SUBST(INTLOBJS)

    dnl Enable libtool support if the surrounding package wishes it.
    INTL_LIBTOOL_SUFFIX_PREFIX=gt_libtool_suffix_prefix
    AC_SUBST(INTL_LIBTOOL_SUFFIX_PREFIX)
  ])

  dnl For backward compatibility. Some Makefiles may be using this.
  INTLLIBS="$LIBINTL"
  AC_SUBST(INTLLIBS)

  dnl Make all documented variables known to autoconf.
  AC_SUBST(LIBINTL)
  AC_SUBST(LTLIBINTL)
  AC_SUBST(POSUB)
])


dnl Checks for all prerequisites of the intl subdirectory,
dnl except for INTL_LIBTOOL_SUFFIX_PREFIX (and possibly LIBTOOL), INTLOBJS,
dnl            USE_INCLUDED_LIBINTL, BUILD_INCLUDED_LIBINTL.
AC_DEFUN([AM_INTL_SUBDIR],
[
  AC_REQUIRE([AC_PROG_INSTALL])dnl
  AC_REQUIRE([AM_MKINSTALLDIRS])dnl
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AC_CANONICAL_HOST])dnl
  AC_REQUIRE([AC_PROG_RANLIB])dnl
  AC_REQUIRE([AC_ISC_POSIX])dnl
  AC_REQUIRE([AC_HEADER_STDC])dnl
  AC_REQUIRE([AC_C_CONST])dnl
  AC_REQUIRE([AC_C_INLINE])dnl
  AC_REQUIRE([AC_TYPE_OFF_T])dnl
  AC_REQUIRE([AC_TYPE_SIZE_T])dnl
  AC_REQUIRE([AC_FUNC_ALLOCA])dnl
  AC_REQUIRE([AC_FUNC_MMAP])dnl
  AC_REQUIRE([jm_GLIBC21])dnl
  AC_REQUIRE([gt_INTDIV0])dnl
  AC_REQUIRE([jm_AC_TYPE_UINTMAX_T])dnl
  AC_REQUIRE([gt_HEADER_INTTYPES_H])dnl
  AC_REQUIRE([gt_INTTYPES_PRI])dnl

  AC_CHECK_HEADERS([argz.h limits.h locale.h nl_types.h malloc.h stddef.h \
stdlib.h string.h unistd.h sys/param.h])
  AC_CHECK_FUNCS([feof_unlocked fgets_unlocked getc_unlocked getcwd getegid \
geteuid getgid getuid mempcpy munmap putenv setenv setlocale stpcpy \
strcasecmp strdup strtoul tsearch __argz_count __argz_stringify __argz_next \
__fsetlocking])

  AM_ICONV
  AM_LANGINFO_CODESET
  if test $ac_cv_header_locale_h = yes; then
    AM_LC_MESSAGES
  fi

  dnl intl/plural.c is generated from intl/plural.y. It requires bison,
  dnl because plural.y uses bison specific features. It requires at least
  dnl bison-1.26 because earlier versions generate a plural.c that doesn't
  dnl compile.
  dnl bison is only needed for the maintainer (who touches plural.y). But in
  dnl order to avoid separate Makefiles or --enable-maintainer-mode, we put
  dnl the rule in general Makefile. Now, some people carelessly touch the
  dnl files or have a broken "make" program, hence the plural.c rule will
  dnl sometimes fire. To avoid an error, defines BISON to ":" if it is not
  dnl present or too old.
  AC_CHECK_PROGS([INTLBISON], [bison])
  if test -z "$INTLBISON"; then
    ac_verc_fail=yes
  else
    dnl Found it, now check the version.
    AC_MSG_CHECKING([version of bison])
changequote(<<,>>)dnl
    ac_prog_version=`$INTLBISON --version 2>&1 | sed -n 's/^.*GNU Bison.* \([0-9]*\.[0-9.]*\).*$/\1/p'`
    case $ac_prog_version in
      '') ac_prog_version="v. ?.??, bad"; ac_verc_fail=yes;;
      1.2[6-9]* | 1.[3-9][0-9]* | [2-9].*)
changequote([,])dnl
         ac_prog_version="$ac_prog_version, ok"; ac_verc_fail=no;;
      *) ac_prog_version="$ac_prog_version, bad"; ac_verc_fail=yes;;
    esac
    AC_MSG_RESULT([$ac_prog_version])
  fi
  if test $ac_verc_fail = yes; then
    INTLBISON=:
  fi
])


dnl Usage: AM_GNU_GETTEXT_VERSION([gettext-version])
AC_DEFUN([AM_GNU_GETTEXT_VERSION], [])
# glibc21.m4 serial 2 (fileutils-4.1.3, gettext-0.10.40)
dnl Copyright (C) 2000-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

# Test for the GNU C Library, version 2.1 or newer.
# From Bruno Haible.

AC_DEFUN([jm_GLIBC21],
  [
    AC_CACHE_CHECK(whether we are using the GNU C Library 2.1 or newer,
      ac_cv_gnu_library_2_1,
      [AC_EGREP_CPP([Lucky GNU user],
	[
#include <features.h>
#ifdef __GNU_LIBRARY__
 #if (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1) || (__GLIBC__ > 2)
  Lucky GNU user
 #endif
#endif
	],
	ac_cv_gnu_library_2_1=yes,
	ac_cv_gnu_library_2_1=no)
      ]
    )
    AC_SUBST(GLIBC21)
    GLIBC21="$ac_cv_gnu_library_2_1"
  ]
)
# iconv.m4 serial AM4 (gettext-0.11.3)
dnl Copyright (C) 2000-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.

AC_DEFUN([AM_ICONV_LINKFLAGS_BODY],
[
  dnl Prerequisites of AC_LIB_LINKFLAGS_BODY.
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])

  dnl Search for libiconv and define LIBICONV, LTLIBICONV and INCICONV
  dnl accordingly.
  AC_LIB_LINKFLAGS_BODY([iconv])
])

AC_DEFUN([AM_ICONV_LINK],
[
  dnl Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  dnl those with the standalone portable GNU libiconv installed).

  dnl Search for libiconv and define LIBICONV, LTLIBICONV and INCICONV
  dnl accordingly.
  AC_REQUIRE([AM_ICONV_LINKFLAGS_BODY])

  dnl Add $INCICONV to CPPFLAGS before performing the following checks,
  dnl because if the user has installed libiconv and not disabled its use
  dnl via --without-libiconv-prefix, he wants to use it. The first
  dnl AC_TRY_LINK will then fail, the second AC_TRY_LINK will succeed.
  am_save_CPPFLAGS="$CPPFLAGS"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INCICONV])

  AC_CACHE_CHECK(for iconv, am_cv_func_iconv, [
    am_cv_func_iconv="no, consider installing GNU libiconv"
    am_cv_lib_iconv=no
    AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
      [iconv_t cd = iconv_open("","");
       iconv(cd,NULL,NULL,NULL,NULL);
       iconv_close(cd);],
      am_cv_func_iconv=yes)
    if test "$am_cv_func_iconv" != yes; then
      am_save_LIBS="$LIBS"
      LIBS="$LIBS $LIBICONV"
      AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
        [iconv_t cd = iconv_open("","");
         iconv(cd,NULL,NULL,NULL,NULL);
         iconv_close(cd);],
        am_cv_lib_iconv=yes
        am_cv_func_iconv=yes)
      LIBS="$am_save_LIBS"
    fi
  ])
  if test "$am_cv_func_iconv" = yes; then
    AC_DEFINE(HAVE_ICONV, 1, [Define if you have the iconv() function.])
  fi
  if test "$am_cv_lib_iconv" = yes; then
    AC_MSG_CHECKING([how to link with libiconv])
    AC_MSG_RESULT([$LIBICONV])
  else
    dnl If $LIBICONV didn't lead to a usable library, we don't need $INCICONV
    dnl either.
    CPPFLAGS="$am_save_CPPFLAGS"
    LIBICONV=
    LTLIBICONV=
  fi
  AC_SUBST(LIBICONV)
  AC_SUBST(LTLIBICONV)
])

AC_DEFUN([AM_ICONV],
[
  AM_ICONV_LINK
  if test "$am_cv_func_iconv" = yes; then
    AC_MSG_CHECKING([for iconv declaration])
    AC_CACHE_VAL(am_cv_proto_iconv, [
      AC_TRY_COMPILE([
#include <stdlib.h>
#include <iconv.h>
extern
#ifdef __cplusplus
"C"
#endif
#if defined(__STDC__) || defined(__cplusplus)
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
#else
size_t iconv();
#endif
], [], am_cv_proto_iconv_arg1="", am_cv_proto_iconv_arg1="const")
      am_cv_proto_iconv="extern size_t iconv (iconv_t cd, $am_cv_proto_iconv_arg1 char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);"])
    am_cv_proto_iconv=`echo "[$]am_cv_proto_iconv" | tr -s ' ' | sed -e 's/( /(/'`
    AC_MSG_RESULT([$]{ac_t:-
         }[$]am_cv_proto_iconv)
    AC_DEFINE_UNQUOTED(ICONV_CONST, $am_cv_proto_iconv_arg1,
      [Define as const if the declaration of iconv() needs const.])
  fi
])
# intdiv0.m4 serial 1 (gettext-0.11.3)
dnl Copyright (C) 2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.

AC_DEFUN([gt_INTDIV0],
[
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AC_CANONICAL_HOST])dnl

  AC_CACHE_CHECK([whether integer division by zero raises SIGFPE],
    gt_cv_int_divbyzero_sigfpe,
    [
      AC_TRY_RUN([
#include <stdlib.h>
#include <signal.h>

static void
#ifdef __cplusplus
sigfpe_handler (int sig)
#else
sigfpe_handler (sig) int sig;
#endif
{
  /* Exit with code 0 if SIGFPE, with code 1 if any other signal.  */
  exit (sig != SIGFPE);
}

int x = 1;
int y = 0;
int z;
int nan;

int main ()
{
  signal (SIGFPE, sigfpe_handler);
/* IRIX and AIX (when "xlc -qcheck" is used) yield signal SIGTRAP.  */
#if (defined (__sgi) || defined (_AIX)) && defined (SIGTRAP)
  signal (SIGTRAP, sigfpe_handler);
#endif
/* Linux/SPARC yields signal SIGILL.  */
#if defined (__sparc__) && defined (__linux__)
  signal (SIGILL, sigfpe_handler);
#endif

  z = x / y;
  nan = y / y;
  exit (1);
}
], gt_cv_int_divbyzero_sigfpe=yes, gt_cv_int_divbyzero_sigfpe=no,
        [
          # Guess based on the CPU.
          case "$host_cpu" in
            alpha* | i[34567]86 | m68k | s390*)
              gt_cv_int_divbyzero_sigfpe="guessing yes";;
            *)
              gt_cv_int_divbyzero_sigfpe="guessing no";;
          esac
        ])
    ])
  case "$gt_cv_int_divbyzero_sigfpe" in
    *yes) value=1;;
    *) value=0;;
  esac
  AC_DEFINE_UNQUOTED(INTDIV0_RAISES_SIGFPE, $value,
    [Define if integer division by zero raises signal SIGFPE.])
])
# inttypes.m4 serial 1 (gettext-0.11.4)
dnl Copyright (C) 1997-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

# Define HAVE_INTTYPES_H if <inttypes.h> exists and doesn't clash with
# <sys/types.h>.

AC_DEFUN([gt_HEADER_INTTYPES_H],
[
  AC_CACHE_CHECK([for inttypes.h], gt_cv_header_inttypes_h,
  [
    AC_TRY_COMPILE(
      [#include <sys/types.h>
#include <inttypes.h>],
      [], gt_cv_header_inttypes_h=yes, gt_cv_header_inttypes_h=no)
  ])
  if test $gt_cv_header_inttypes_h = yes; then
    AC_DEFINE_UNQUOTED(HAVE_INTTYPES_H, 1,
      [Define if <inttypes.h> exists and doesn't clash with <sys/types.h>.])
  fi
])
# inttypes_h.m4 serial 5 (gettext-0.12)
dnl Copyright (C) 1997-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

# Define HAVE_INTTYPES_H_WITH_UINTMAX if <inttypes.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.

AC_DEFUN([jm_AC_HEADER_INTTYPES_H],
[
  AC_CACHE_CHECK([for inttypes.h], jm_ac_cv_header_inttypes_h,
  [AC_TRY_COMPILE(
    [#include <sys/types.h>
#include <inttypes.h>],
    [uintmax_t i = (uintmax_t) -1;],
    jm_ac_cv_header_inttypes_h=yes,
    jm_ac_cv_header_inttypes_h=no)])
  if test $jm_ac_cv_header_inttypes_h = yes; then
    AC_DEFINE_UNQUOTED(HAVE_INTTYPES_H_WITH_UINTMAX, 1,
      [Define if <inttypes.h> exists, doesn't clash with <sys/types.h>,
       and declares uintmax_t. ])
  fi
])
# inttypes-pri.m4 serial 1 (gettext-0.11.4)
dnl Copyright (C) 1997-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.

# Define PRI_MACROS_BROKEN if <inttypes.h> exists and defines the PRI*
# macros to non-string values.  This is the case on AIX 4.3.3.

AC_DEFUN([gt_INTTYPES_PRI],
[
  AC_REQUIRE([gt_HEADER_INTTYPES_H])
  if test $gt_cv_header_inttypes_h = yes; then
    AC_CACHE_CHECK([whether the inttypes.h PRIxNN macros are broken],
      gt_cv_inttypes_pri_broken,
      [
        AC_TRY_COMPILE([#include <inttypes.h>
#ifdef PRId32
char *p = PRId32;
#endif
], [], gt_cv_inttypes_pri_broken=no, gt_cv_inttypes_pri_broken=yes)
      ])
  fi
  if test "$gt_cv_inttypes_pri_broken" = yes; then
    AC_DEFINE_UNQUOTED(PRI_MACROS_BROKEN, 1,
      [Define if <inttypes.h> exists and defines unusable PRI* macros.])
  fi
])
# isc-posix.m4 serial 2 (gettext-0.11.2)
dnl Copyright (C) 1995-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

# This file is not needed with autoconf-2.53 and newer.  Remove it in 2005.

# This test replaces the one in autoconf.
# Currently this macro should have the same name as the autoconf macro
# because gettext's gettext.m4 (distributed in the automake package)
# still uses it.  Otherwise, the use in gettext.m4 makes autoheader
# give these diagnostics:
#   configure.in:556: AC_TRY_COMPILE was called before AC_ISC_POSIX
#   configure.in:556: AC_TRY_RUN was called before AC_ISC_POSIX

undefine([AC_ISC_POSIX])

AC_DEFUN([AC_ISC_POSIX],
  [
    dnl This test replaces the obsolescent AC_ISC_POSIX kludge.
    AC_CHECK_LIB(cposix, strerror, [LIBS="$LIBS -lcposix"])
  ]
)
# lcmessage.m4 serial 3 (gettext-0.11.3)
dnl Copyright (C) 1995-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.
dnl
dnl This file can can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995.

# Check whether LC_MESSAGES is available in <locale.h>.

AC_DEFUN([AM_LC_MESSAGES],
[
  AC_CACHE_CHECK([for LC_MESSAGES], am_cv_val_LC_MESSAGES,
    [AC_TRY_LINK([#include <locale.h>], [return LC_MESSAGES],
       am_cv_val_LC_MESSAGES=yes, am_cv_val_LC_MESSAGES=no)])
  if test $am_cv_val_LC_MESSAGES = yes; then
    AC_DEFINE(HAVE_LC_MESSAGES, 1,
      [Define if your <locale.h> file defines LC_MESSAGES.])
  fi
])
# lib-ld.m4 serial 2 (gettext-0.12)
dnl Copyright (C) 1996-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl Subroutines of libtool.m4,
dnl with replacements s/AC_/AC_LIB/ and s/lt_cv/acl_cv/ to avoid collision
dnl with libtool.m4.

dnl From libtool-1.4. Sets the variable with_gnu_ld to yes or no.
AC_DEFUN([AC_LIB_PROG_LD_GNU],
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], acl_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  acl_cv_prog_gnu_ld=yes
else
  acl_cv_prog_gnu_ld=no
fi])
with_gnu_ld=$acl_cv_prog_gnu_ld
])

dnl From libtool-1.4. Sets the variable LD.
AC_DEFUN([AC_LIB_PROG_LD],
[AC_ARG_WITH(gnu-ld,
[  --with-gnu-ld           assume the C compiler uses GNU ld [default=no]],
test "$withval" = no || with_gnu_ld=yes, with_gnu_ld=no)
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
# Prepare PATH_SEPARATOR.
# The user is always right.
if test "${PATH_SEPARATOR+set}" != set; then
  echo "#! /bin/sh" >conf$$.sh
  echo  "exit 0"   >>conf$$.sh
  chmod +x conf$$.sh
  if (PATH="/nonexistent;."; conf$$.sh) >/dev/null 2>&1; then
    PATH_SEPARATOR=';'
  else
    PATH_SEPARATOR=:
  fi
  rm -f conf$$.sh
fi
ac_prog=ld
if test "$GCC" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by GCC])
  case $host in
  *-*-mingw*)
    # gcc leaves a trailing carriage return which upsets mingw
    ac_prog=`($CC -print-prog-name=ld) 2>&5 | tr -d '\015'` ;;
  *)
    ac_prog=`($CC -print-prog-name=ld) 2>&5` ;;
  esac
  case $ac_prog in
    # Accept absolute paths.
    [[\\/]* | [A-Za-z]:[\\/]*)]
      [re_direlt='/[^/][^/]*/\.\./']
      # Canonicalize the path of ld
      ac_prog=`echo $ac_prog| sed 's%\\\\%/%g'`
      while echo $ac_prog | grep "$re_direlt" > /dev/null 2>&1; do
	ac_prog=`echo $ac_prog| sed "s%$re_direlt%/%"`
      done
      test -z "$LD" && LD="$ac_prog"
      ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test "$with_gnu_ld" = yes; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL(acl_cv_path_LD,
[if test -z "$LD"; then
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH; do
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
      acl_cv_path_LD="$ac_dir/$ac_prog"
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some GNU ld's only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      if "$acl_cv_path_LD" -v 2>&1 < /dev/null | egrep '(GNU|with BFD)' > /dev/null; then
	test "$with_gnu_ld" != no && break
      else
	test "$with_gnu_ld" != yes && break
      fi
    fi
  done
  IFS="$ac_save_ifs"
else
  acl_cv_path_LD="$LD" # Let the user override the test with a path.
fi])
LD="$acl_cv_path_LD"
if test -n "$LD"; then
  AC_MSG_RESULT($LD)
else
  AC_MSG_RESULT(no)
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
AC_LIB_PROG_LD_GNU
])
# lib-link.m4 serial 4 (gettext-0.12)
dnl Copyright (C) 2001-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.

dnl AC_LIB_LINKFLAGS(name [, dependencies]) searches for libname and
dnl the libraries corresponding to explicit and implicit dependencies.
dnl Sets and AC_SUBSTs the LIB${NAME} and LTLIB${NAME} variables and
dnl augments the CPPFLAGS variable.
AC_DEFUN([AC_LIB_LINKFLAGS],
[
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])
  define([Name],[translit([$1],[./-], [___])])
  define([NAME],[translit([$1],[abcdefghijklmnopqrstuvwxyz./-],
                               [ABCDEFGHIJKLMNOPQRSTUVWXYZ___])])
  AC_CACHE_CHECK([how to link with lib[]$1], [ac_cv_lib[]Name[]_libs], [
    AC_LIB_LINKFLAGS_BODY([$1], [$2])
    ac_cv_lib[]Name[]_libs="$LIB[]NAME"
    ac_cv_lib[]Name[]_ltlibs="$LTLIB[]NAME"
    ac_cv_lib[]Name[]_cppflags="$INC[]NAME"
  ])
  LIB[]NAME="$ac_cv_lib[]Name[]_libs"
  LTLIB[]NAME="$ac_cv_lib[]Name[]_ltlibs"
  INC[]NAME="$ac_cv_lib[]Name[]_cppflags"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INC]NAME)
  AC_SUBST([LIB]NAME)
  AC_SUBST([LTLIB]NAME)
  dnl Also set HAVE_LIB[]NAME so that AC_LIB_HAVE_LINKFLAGS can reuse the
  dnl results of this search when this library appears as a dependency.
  HAVE_LIB[]NAME=yes
  undefine([Name])
  undefine([NAME])
])

dnl AC_LIB_HAVE_LINKFLAGS(name, dependencies, includes, testcode)
dnl searches for libname and the libraries corresponding to explicit and
dnl implicit dependencies, together with the specified include files and
dnl the ability to compile and link the specified testcode. If found, it
dnl sets and AC_SUBSTs HAVE_LIB${NAME}=yes and the LIB${NAME} and
dnl LTLIB${NAME} variables and augments the CPPFLAGS variable, and
dnl #defines HAVE_LIB${NAME} to 1. Otherwise, it sets and AC_SUBSTs
dnl HAVE_LIB${NAME}=no and LIB${NAME} and LTLIB${NAME} to empty.
AC_DEFUN([AC_LIB_HAVE_LINKFLAGS],
[
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  AC_REQUIRE([AC_LIB_RPATH])
  define([Name],[translit([$1],[./-], [___])])
  define([NAME],[translit([$1],[abcdefghijklmnopqrstuvwxyz./-],
                               [ABCDEFGHIJKLMNOPQRSTUVWXYZ___])])

  dnl Search for lib[]Name and define LIB[]NAME, LTLIB[]NAME and INC[]NAME
  dnl accordingly.
  AC_LIB_LINKFLAGS_BODY([$1], [$2])

  dnl Add $INC[]NAME to CPPFLAGS before performing the following checks,
  dnl because if the user has installed lib[]Name and not disabled its use
  dnl via --without-lib[]Name-prefix, he wants to use it.
  ac_save_CPPFLAGS="$CPPFLAGS"
  AC_LIB_APPENDTOVAR([CPPFLAGS], [$INC]NAME)

  AC_CACHE_CHECK([for lib[]$1], [ac_cv_lib[]Name], [
    ac_save_LIBS="$LIBS"
    LIBS="$LIBS $LIB[]NAME"
    AC_TRY_LINK([$3], [$4], [ac_cv_lib[]Name=yes], [ac_cv_lib[]Name=no])
    LIBS="$ac_save_LIBS"
  ])
  if test "$ac_cv_lib[]Name" = yes; then
    HAVE_LIB[]NAME=yes
    AC_DEFINE([HAVE_LIB]NAME, 1, [Define if you have the $1 library.])
    AC_MSG_CHECKING([how to link with lib[]$1])
    AC_MSG_RESULT([$LIB[]NAME])
  else
    HAVE_LIB[]NAME=no
    dnl If $LIB[]NAME didn't lead to a usable library, we don't need
    dnl $INC[]NAME either.
    CPPFLAGS="$ac_save_CPPFLAGS"
    LIB[]NAME=
    LTLIB[]NAME=
  fi
  AC_SUBST([HAVE_LIB]NAME)
  AC_SUBST([LIB]NAME)
  AC_SUBST([LTLIB]NAME)
  undefine([Name])
  undefine([NAME])
])

dnl Determine the platform dependent parameters needed to use rpath:
dnl libext, shlibext, hardcode_libdir_flag_spec, hardcode_libdir_separator,
dnl hardcode_direct, hardcode_minus_L.
AC_DEFUN([AC_LIB_RPATH],
[
  AC_REQUIRE([AC_PROG_CC])                dnl we use $CC, $GCC, $LDFLAGS
  AC_REQUIRE([AC_LIB_PROG_LD])            dnl we use $LD, $with_gnu_ld
  AC_REQUIRE([AC_CANONICAL_HOST])         dnl we use $host
  AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT]) dnl we use $ac_aux_dir
  AC_CACHE_CHECK([for shared library run path origin], acl_cv_rpath, [
    CC="$CC" GCC="$GCC" LDFLAGS="$LDFLAGS" LD="$LD" with_gnu_ld="$with_gnu_ld" \
    ${CONFIG_SHELL-/bin/sh} "$ac_aux_dir/config.rpath" "$host" > conftest.sh
    . ./conftest.sh
    rm -f ./conftest.sh
    acl_cv_rpath=done
  ])
  wl="$acl_cv_wl"
  libext="$acl_cv_libext"
  shlibext="$acl_cv_shlibext"
  hardcode_libdir_flag_spec="$acl_cv_hardcode_libdir_flag_spec"
  hardcode_libdir_separator="$acl_cv_hardcode_libdir_separator"
  hardcode_direct="$acl_cv_hardcode_direct"
  hardcode_minus_L="$acl_cv_hardcode_minus_L"
  dnl Determine whether the user wants rpath handling at all.
  AC_ARG_ENABLE(rpath,
    [  --disable-rpath         do not hardcode runtime library paths],
    :, enable_rpath=yes)
])

dnl AC_LIB_LINKFLAGS_BODY(name [, dependencies]) searches for libname and
dnl the libraries corresponding to explicit and implicit dependencies.
dnl Sets the LIB${NAME}, LTLIB${NAME} and INC${NAME} variables.
AC_DEFUN([AC_LIB_LINKFLAGS_BODY],
[
  define([NAME],[translit([$1],[abcdefghijklmnopqrstuvwxyz./-],
                               [ABCDEFGHIJKLMNOPQRSTUVWXYZ___])])
  dnl By default, look in $includedir and $libdir.
  use_additional=yes
  AC_LIB_WITH_FINAL_PREFIX([
    eval additional_includedir=\"$includedir\"
    eval additional_libdir=\"$libdir\"
  ])
  AC_LIB_ARG_WITH([lib$1-prefix],
[  --with-lib$1-prefix[=DIR]  search for lib$1 in DIR/include and DIR/lib
  --without-lib$1-prefix     don't search for lib$1 in includedir and libdir],
[
    if test "X$withval" = "Xno"; then
      use_additional=no
    else
      if test "X$withval" = "X"; then
        AC_LIB_WITH_FINAL_PREFIX([
          eval additional_includedir=\"$includedir\"
          eval additional_libdir=\"$libdir\"
        ])
      else
        additional_includedir="$withval/include"
        additional_libdir="$withval/lib"
      fi
    fi
])
  dnl Search the library and its dependencies in $additional_libdir and
  dnl $LDFLAGS. Using breadth-first-seach.
  LIB[]NAME=
  LTLIB[]NAME=
  INC[]NAME=
  rpathdirs=
  ltrpathdirs=
  names_already_handled=
  names_next_round='$1 $2'
  while test -n "$names_next_round"; do
    names_this_round="$names_next_round"
    names_next_round=
    for name in $names_this_round; do
      already_handled=
      for n in $names_already_handled; do
        if test "$n" = "$name"; then
          already_handled=yes
          break
        fi
      done
      if test -z "$already_handled"; then
        names_already_handled="$names_already_handled $name"
        dnl See if it was already located by an earlier AC_LIB_LINKFLAGS
        dnl or AC_LIB_HAVE_LINKFLAGS call.
        uppername=`echo "$name" | sed -e 'y|abcdefghijklmnopqrstuvwxyz./-|ABCDEFGHIJKLMNOPQRSTUVWXYZ___|'`
        eval value=\"\$HAVE_LIB$uppername\"
        if test -n "$value"; then
          if test "$value" = yes; then
            eval value=\"\$LIB$uppername\"
            test -z "$value" || LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$value"
            eval value=\"\$LTLIB$uppername\"
            test -z "$value" || LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }$value"
          else
            dnl An earlier call to AC_LIB_HAVE_LINKFLAGS has determined
            dnl that this library doesn't exist. So just drop it.
            :
          fi
        else
          dnl Search the library lib$name in $additional_libdir and $LDFLAGS
          dnl and the already constructed $LIBNAME/$LTLIBNAME.
          found_dir=
          found_la=
          found_so=
          found_a=
          if test $use_additional = yes; then
            if test -n "$shlibext" && test -f "$additional_libdir/lib$name.$shlibext"; then
              found_dir="$additional_libdir"
              found_so="$additional_libdir/lib$name.$shlibext"
              if test -f "$additional_libdir/lib$name.la"; then
                found_la="$additional_libdir/lib$name.la"
              fi
            else
              if test -f "$additional_libdir/lib$name.$libext"; then
                found_dir="$additional_libdir"
                found_a="$additional_libdir/lib$name.$libext"
                if test -f "$additional_libdir/lib$name.la"; then
                  found_la="$additional_libdir/lib$name.la"
                fi
              fi
            fi
          fi
          if test "X$found_dir" = "X"; then
            for x in $LDFLAGS $LTLIB[]NAME; do
              AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
              case "$x" in
                -L*)
                  dir=`echo "X$x" | sed -e 's/^X-L//'`
                  if test -n "$shlibext" && test -f "$dir/lib$name.$shlibext"; then
                    found_dir="$dir"
                    found_so="$dir/lib$name.$shlibext"
                    if test -f "$dir/lib$name.la"; then
                      found_la="$dir/lib$name.la"
                    fi
                  else
                    if test -f "$dir/lib$name.$libext"; then
                      found_dir="$dir"
                      found_a="$dir/lib$name.$libext"
                      if test -f "$dir/lib$name.la"; then
                        found_la="$dir/lib$name.la"
                      fi
                    fi
                  fi
                  ;;
              esac
              if test "X$found_dir" != "X"; then
                break
              fi
            done
          fi
          if test "X$found_dir" != "X"; then
            dnl Found the library.
            LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-L$found_dir -l$name"
            if test "X$found_so" != "X"; then
              dnl Linking with a shared library. We attempt to hardcode its
              dnl directory into the executable's runpath, unless it's the
              dnl standard /usr/lib.
              if test "$enable_rpath" = no || test "X$found_dir" = "X/usr/lib"; then
                dnl No hardcoding is needed.
                LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
              else
                dnl Use an explicit option to hardcode DIR into the resulting
                dnl binary.
                dnl Potentially add DIR to ltrpathdirs.
                dnl The ltrpathdirs will be appended to $LTLIBNAME at the end.
                haveit=
                for x in $ltrpathdirs; do
                  if test "X$x" = "X$found_dir"; then
                    haveit=yes
                    break
                  fi
                done
                if test -z "$haveit"; then
                  ltrpathdirs="$ltrpathdirs $found_dir"
                fi
                dnl The hardcoding into $LIBNAME is system dependent.
                if test "$hardcode_direct" = yes; then
                  dnl Using DIR/libNAME.so during linking hardcodes DIR into the
                  dnl resulting binary.
                  LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
                else
                  if test -n "$hardcode_libdir_flag_spec" && test "$hardcode_minus_L" = no; then
                    dnl Use an explicit option to hardcode DIR into the resulting
                    dnl binary.
                    LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
                    dnl Potentially add DIR to rpathdirs.
                    dnl The rpathdirs will be appended to $LIBNAME at the end.
                    haveit=
                    for x in $rpathdirs; do
                      if test "X$x" = "X$found_dir"; then
                        haveit=yes
                        break
                      fi
                    done
                    if test -z "$haveit"; then
                      rpathdirs="$rpathdirs $found_dir"
                    fi
                  else
                    dnl Rely on "-L$found_dir".
                    dnl But don't add it if it's already contained in the LDFLAGS
                    dnl or the already constructed $LIBNAME
                    haveit=
                    for x in $LDFLAGS $LIB[]NAME; do
                      AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                      if test "X$x" = "X-L$found_dir"; then
                        haveit=yes
                        break
                      fi
                    done
                    if test -z "$haveit"; then
                      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-L$found_dir"
                    fi
                    if test "$hardcode_minus_L" != no; then
                      dnl FIXME: Not sure whether we should use
                      dnl "-L$found_dir -l$name" or "-L$found_dir $found_so"
                      dnl here.
                      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_so"
                    else
                      dnl We cannot use $hardcode_runpath_var and LD_RUN_PATH
                      dnl here, because this doesn't fit in flags passed to the
                      dnl compiler. So give up. No hardcoding. This affects only
                      dnl very old systems.
                      dnl FIXME: Not sure whether we should use
                      dnl "-L$found_dir -l$name" or "-L$found_dir $found_so"
                      dnl here.
                      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-l$name"
                    fi
                  fi
                fi
              fi
            else
              if test "X$found_a" != "X"; then
                dnl Linking with a static library.
                LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$found_a"
              else
                dnl We shouldn't come here, but anyway it's good to have a
                dnl fallback.
                LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-L$found_dir -l$name"
              fi
            fi
            dnl Assume the include files are nearby.
            additional_includedir=
            case "$found_dir" in
              */lib | */lib/)
                basedir=`echo "X$found_dir" | sed -e 's,^X,,' -e 's,/lib/*$,,'`
                additional_includedir="$basedir/include"
                ;;
            esac
            if test "X$additional_includedir" != "X"; then
              dnl Potentially add $additional_includedir to $INCNAME.
              dnl But don't add it
              dnl   1. if it's the standard /usr/include,
              dnl   2. if it's /usr/local/include and we are using GCC on Linux,
              dnl   3. if it's already present in $CPPFLAGS or the already
              dnl      constructed $INCNAME,
              dnl   4. if it doesn't exist as a directory.
              if test "X$additional_includedir" != "X/usr/include"; then
                haveit=
                if test "X$additional_includedir" = "X/usr/local/include"; then
                  if test -n "$GCC"; then
                    case $host_os in
                      linux*) haveit=yes;;
                    esac
                  fi
                fi
                if test -z "$haveit"; then
                  for x in $CPPFLAGS $INC[]NAME; do
                    AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                    if test "X$x" = "X-I$additional_includedir"; then
                      haveit=yes
                      break
                    fi
                  done
                  if test -z "$haveit"; then
                    if test -d "$additional_includedir"; then
                      dnl Really add $additional_includedir to $INCNAME.
                      INC[]NAME="${INC[]NAME}${INC[]NAME:+ }-I$additional_includedir"
                    fi
                  fi
                fi
              fi
            fi
            dnl Look for dependencies.
            if test -n "$found_la"; then
              dnl Read the .la file. It defines the variables
              dnl dlname, library_names, old_library, dependency_libs, current,
              dnl age, revision, installed, dlopen, dlpreopen, libdir.
              save_libdir="$libdir"
              case "$found_la" in
                */* | *\\*) . "$found_la" ;;
                *) . "./$found_la" ;;
              esac
              libdir="$save_libdir"
              dnl We use only dependency_libs.
              for dep in $dependency_libs; do
                case "$dep" in
                  -L*)
                    additional_libdir=`echo "X$dep" | sed -e 's/^X-L//'`
                    dnl Potentially add $additional_libdir to $LIBNAME and $LTLIBNAME.
                    dnl But don't add it
                    dnl   1. if it's the standard /usr/lib,
                    dnl   2. if it's /usr/local/lib and we are using GCC on Linux,
                    dnl   3. if it's already present in $LDFLAGS or the already
                    dnl      constructed $LIBNAME,
                    dnl   4. if it doesn't exist as a directory.
                    if test "X$additional_libdir" != "X/usr/lib"; then
                      haveit=
                      if test "X$additional_libdir" = "X/usr/local/lib"; then
                        if test -n "$GCC"; then
                          case $host_os in
                            linux*) haveit=yes;;
                          esac
                        fi
                      fi
                      if test -z "$haveit"; then
                        haveit=
                        for x in $LDFLAGS $LIB[]NAME; do
                          AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                          if test "X$x" = "X-L$additional_libdir"; then
                            haveit=yes
                            break
                          fi
                        done
                        if test -z "$haveit"; then
                          if test -d "$additional_libdir"; then
                            dnl Really add $additional_libdir to $LIBNAME.
                            LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-L$additional_libdir"
                          fi
                        fi
                        haveit=
                        for x in $LDFLAGS $LTLIB[]NAME; do
                          AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
                          if test "X$x" = "X-L$additional_libdir"; then
                            haveit=yes
                            break
                          fi
                        done
                        if test -z "$haveit"; then
                          if test -d "$additional_libdir"; then
                            dnl Really add $additional_libdir to $LTLIBNAME.
                            LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-L$additional_libdir"
                          fi
                        fi
                      fi
                    fi
                    ;;
                  -R*)
                    dir=`echo "X$dep" | sed -e 's/^X-R//'`
                    if test "$enable_rpath" != no; then
                      dnl Potentially add DIR to rpathdirs.
                      dnl The rpathdirs will be appended to $LIBNAME at the end.
                      haveit=
                      for x in $rpathdirs; do
                        if test "X$x" = "X$dir"; then
                          haveit=yes
                          break
                        fi
                      done
                      if test -z "$haveit"; then
                        rpathdirs="$rpathdirs $dir"
                      fi
                      dnl Potentially add DIR to ltrpathdirs.
                      dnl The ltrpathdirs will be appended to $LTLIBNAME at the end.
                      haveit=
                      for x in $ltrpathdirs; do
                        if test "X$x" = "X$dir"; then
                          haveit=yes
                          break
                        fi
                      done
                      if test -z "$haveit"; then
                        ltrpathdirs="$ltrpathdirs $dir"
                      fi
                    fi
                    ;;
                  -l*)
                    dnl Handle this in the next round.
                    names_next_round="$names_next_round "`echo "X$dep" | sed -e 's/^X-l//'`
                    ;;
                  *.la)
                    dnl Handle this in the next round. Throw away the .la's
                    dnl directory; it is already contained in a preceding -L
                    dnl option.
                    names_next_round="$names_next_round "`echo "X$dep" | sed -e 's,^X.*/,,' -e 's,^lib,,' -e 's,\.la$,,'`
                    ;;
                  *)
                    dnl Most likely an immediate library name.
                    LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$dep"
                    LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }$dep"
                    ;;
                esac
              done
            fi
          else
            dnl Didn't find the library; assume it is in the system directories
            dnl known to the linker and runtime loader. (All the system
            dnl directories known to the linker should also be known to the
            dnl runtime loader, otherwise the system is severely misconfigured.)
            LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }-l$name"
            LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-l$name"
          fi
        fi
      fi
    done
  done
  if test "X$rpathdirs" != "X"; then
    if test -n "$hardcode_libdir_separator"; then
      dnl Weird platform: only the last -rpath option counts, the user must
      dnl pass all path elements in one option. We can arrange that for a
      dnl single library, but not when more than one $LIBNAMEs are used.
      alldirs=
      for found_dir in $rpathdirs; do
        alldirs="${alldirs}${alldirs:+$hardcode_libdir_separator}$found_dir"
      done
      dnl Note: hardcode_libdir_flag_spec uses $libdir and $wl.
      acl_save_libdir="$libdir"
      libdir="$alldirs"
      eval flag=\"$hardcode_libdir_flag_spec\"
      libdir="$acl_save_libdir"
      LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$flag"
    else
      dnl The -rpath options are cumulative.
      for found_dir in $rpathdirs; do
        acl_save_libdir="$libdir"
        libdir="$found_dir"
        eval flag=\"$hardcode_libdir_flag_spec\"
        libdir="$acl_save_libdir"
        LIB[]NAME="${LIB[]NAME}${LIB[]NAME:+ }$flag"
      done
    fi
  fi
  if test "X$ltrpathdirs" != "X"; then
    dnl When using libtool, the option that works for both libraries and
    dnl executables is -R. The -R options are cumulative.
    for found_dir in $ltrpathdirs; do
      LTLIB[]NAME="${LTLIB[]NAME}${LTLIB[]NAME:+ }-R$found_dir"
    done
  fi
])

dnl AC_LIB_APPENDTOVAR(VAR, CONTENTS) appends the elements of CONTENTS to VAR,
dnl unless already present in VAR.
dnl Works only for CPPFLAGS, not for LIB* variables because that sometimes
dnl contains two or three consecutive elements that belong together.
AC_DEFUN([AC_LIB_APPENDTOVAR],
[
  for element in [$2]; do
    haveit=
    for x in $[$1]; do
      AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
      if test "X$x" = "X$element"; then
        haveit=yes
        break
      fi
    done
    if test -z "$haveit"; then
      [$1]="${[$1]}${[$1]:+ }$element"
    fi
  done
])
# lib-prefix.m4 serial 2 (gettext-0.12)
dnl Copyright (C) 2001-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Bruno Haible.

dnl AC_LIB_ARG_WITH is synonymous to AC_ARG_WITH in autoconf-2.13, and
dnl similar to AC_ARG_WITH in autoconf 2.52...2.57 except that is doesn't
dnl require excessive bracketing.
ifdef([AC_HELP_STRING],
[AC_DEFUN([AC_LIB_ARG_WITH], [AC_ARG_WITH([$1],[[$2]],[$3],[$4])])],
[AC_DEFUN([AC_LIB_ARG_WITH], [AC_ARG_WITH([$1],[$2],[$3],[$4])])])

dnl AC_LIB_PREFIX adds to the CPPFLAGS and LDFLAGS the flags that are needed
dnl to access previously installed libraries. The basic assumption is that
dnl a user will want packages to use other packages he previously installed
dnl with the same --prefix option.
dnl This macro is not needed if only AC_LIB_LINKFLAGS is used to locate
dnl libraries, but is otherwise very convenient.
AC_DEFUN([AC_LIB_PREFIX],
[
  AC_BEFORE([$0], [AC_LIB_LINKFLAGS])
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_REQUIRE([AC_LIB_PREPARE_PREFIX])
  dnl By default, look in $includedir and $libdir.
  use_additional=yes
  AC_LIB_WITH_FINAL_PREFIX([
    eval additional_includedir=\"$includedir\"
    eval additional_libdir=\"$libdir\"
  ])
  AC_LIB_ARG_WITH([lib-prefix],
[  --with-lib-prefix[=DIR] search for libraries in DIR/include and DIR/lib
  --without-lib-prefix    don't search for libraries in includedir and libdir],
[
    if test "X$withval" = "Xno"; then
      use_additional=no
    else
      if test "X$withval" = "X"; then
        AC_LIB_WITH_FINAL_PREFIX([
          eval additional_includedir=\"$includedir\"
          eval additional_libdir=\"$libdir\"
        ])
      else
        additional_includedir="$withval/include"
        additional_libdir="$withval/lib"
      fi
    fi
])
  if test $use_additional = yes; then
    dnl Potentially add $additional_includedir to $CPPFLAGS.
    dnl But don't add it
    dnl   1. if it's the standard /usr/include,
    dnl   2. if it's already present in $CPPFLAGS,
    dnl   3. if it's /usr/local/include and we are using GCC on Linux,
    dnl   4. if it doesn't exist as a directory.
    if test "X$additional_includedir" != "X/usr/include"; then
      haveit=
      for x in $CPPFLAGS; do
        AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
        if test "X$x" = "X-I$additional_includedir"; then
          haveit=yes
          break
        fi
      done
      if test -z "$haveit"; then
        if test "X$additional_includedir" = "X/usr/local/include"; then
          if test -n "$GCC"; then
            case $host_os in
              linux*) haveit=yes;;
            esac
          fi
        fi
        if test -z "$haveit"; then
          if test -d "$additional_includedir"; then
            dnl Really add $additional_includedir to $CPPFLAGS.
            CPPFLAGS="${CPPFLAGS}${CPPFLAGS:+ }-I$additional_includedir"
          fi
        fi
      fi
    fi
    dnl Potentially add $additional_libdir to $LDFLAGS.
    dnl But don't add it
    dnl   1. if it's the standard /usr/lib,
    dnl   2. if it's already present in $LDFLAGS,
    dnl   3. if it's /usr/local/lib and we are using GCC on Linux,
    dnl   4. if it doesn't exist as a directory.
    if test "X$additional_libdir" != "X/usr/lib"; then
      haveit=
      for x in $LDFLAGS; do
        AC_LIB_WITH_FINAL_PREFIX([eval x=\"$x\"])
        if test "X$x" = "X-L$additional_libdir"; then
          haveit=yes
          break
        fi
      done
      if test -z "$haveit"; then
        if test "X$additional_libdir" = "X/usr/local/lib"; then
          if test -n "$GCC"; then
            case $host_os in
              linux*) haveit=yes;;
            esac
          fi
        fi
        if test -z "$haveit"; then
          if test -d "$additional_libdir"; then
            dnl Really add $additional_libdir to $LDFLAGS.
            LDFLAGS="${LDFLAGS}${LDFLAGS:+ }-L$additional_libdir"
          fi
        fi
      fi
    fi
  fi
])

dnl AC_LIB_PREPARE_PREFIX creates variables acl_final_prefix,
dnl acl_final_exec_prefix, containing the values to which $prefix and
dnl $exec_prefix will expand at the end of the configure script.
AC_DEFUN([AC_LIB_PREPARE_PREFIX],
[
  dnl Unfortunately, prefix and exec_prefix get only finally determined
  dnl at the end of configure.
  if test "X$prefix" = "XNONE"; then
    acl_final_prefix="$ac_default_prefix"
  else
    acl_final_prefix="$prefix"
  fi
  if test "X$exec_prefix" = "XNONE"; then
    acl_final_exec_prefix='${prefix}'
  else
    acl_final_exec_prefix="$exec_prefix"
  fi
  acl_save_prefix="$prefix"
  prefix="$acl_final_prefix"
  eval acl_final_exec_prefix=\"$acl_final_exec_prefix\"
  prefix="$acl_save_prefix"
])

dnl AC_LIB_WITH_FINAL_PREFIX([statement]) evaluates statement, with the
dnl variables prefix and exec_prefix bound to the values they will have
dnl at the end of the configure script.
AC_DEFUN([AC_LIB_WITH_FINAL_PREFIX],
[
  acl_save_prefix="$prefix"
  prefix="$acl_final_prefix"
  acl_save_exec_prefix="$exec_prefix"
  exec_prefix="$acl_final_exec_prefix"
  $1
  exec_prefix="$acl_save_exec_prefix"
  prefix="$acl_save_prefix"
])
# nls.m4 serial 1 (gettext-0.12)
dnl Copyright (C) 1995-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.
dnl
dnl This file can can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2003.

AC_DEFUN([AM_NLS],
[
  AC_MSG_CHECKING([whether NLS is requested])
  dnl Default is enabled NLS
  AC_ARG_ENABLE(nls,
    [  --disable-nls           do not use Native Language Support],
    USE_NLS=$enableval, USE_NLS=yes)
  AC_MSG_RESULT($USE_NLS)
  AC_SUBST(USE_NLS)
])

AC_DEFUN([AM_MKINSTALLDIRS],
[
  dnl If the AC_CONFIG_AUX_DIR macro for autoconf is used we possibly
  dnl find the mkinstalldirs script in another subdir but $(top_srcdir).
  dnl Try to locate it.
  MKINSTALLDIRS=
  if test -n "$ac_aux_dir"; then
    case "$ac_aux_dir" in
      /*) MKINSTALLDIRS="$ac_aux_dir/mkinstalldirs" ;;
      *) MKINSTALLDIRS="\$(top_builddir)/$ac_aux_dir/mkinstalldirs" ;;
    esac
  fi
  if test -z "$MKINSTALLDIRS"; then
    MKINSTALLDIRS="\$(top_srcdir)/mkinstalldirs"
  fi
  AC_SUBST(MKINSTALLDIRS)
])
# po.m4 serial 1 (gettext-0.12)
dnl Copyright (C) 1995-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.
dnl
dnl This file can can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2003.

dnl Checks for all prerequisites of the po subdirectory.
AC_DEFUN([AM_PO_SUBDIRS],
[
  AC_REQUIRE([AC_PROG_MAKE_SET])dnl
  AC_REQUIRE([AC_PROG_INSTALL])dnl
  AC_REQUIRE([AM_MKINSTALLDIRS])dnl
  AC_REQUIRE([AM_NLS])dnl

  dnl Perform the following tests also if --disable-nls has been given,
  dnl because they are needed for "make dist" to work.

  dnl Search for GNU msgfmt in the PATH.
  dnl The first test excludes Solaris msgfmt and early GNU msgfmt versions.
  dnl The second test excludes FreeBSD msgfmt.
  AM_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
    [$ac_dir/$ac_word --statistics /dev/null >/dev/null 2>&1 &&
     (if $ac_dir/$ac_word --statistics /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
    :)
  AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)

  dnl Search for GNU xgettext 0.12 or newer in the PATH.
  dnl The first test excludes Solaris xgettext and early GNU xgettext versions.
  dnl The second test excludes FreeBSD xgettext.
  AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
    [$ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null >/dev/null 2>&1 &&
     (if $ac_dir/$ac_word --omit-header --copyright-holder= --msgid-bugs-address= /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi)],
    :)
  dnl Remove leftover from FreeBSD xgettext call.
  rm -f messages.po

  dnl Search for GNU msgmerge 0.11 or newer in the PATH.
  AM_PATH_PROG_WITH_TEST(MSGMERGE, msgmerge,
    [$ac_dir/$ac_word --update -q /dev/null /dev/null >/dev/null 2>&1], :)

  dnl This could go away some day; the PATH_PROG_WITH_TEST already does it.
  dnl Test whether we really found GNU msgfmt.
  if test "$GMSGFMT" != ":"; then
    dnl If it is no GNU msgfmt we define it as : so that the
    dnl Makefiles still can work.
    if $GMSGFMT --statistics /dev/null >/dev/null 2>&1 &&
       (if $GMSGFMT --statistics /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi); then
      : ;
    else
      GMSGFMT=`echo "$GMSGFMT" | sed -e 's,^.*/,,'`
      AC_MSG_RESULT(
        [found $GMSGFMT program is not GNU msgfmt; ignore it])
      GMSGFMT=":"
    fi
  fi

  dnl This could go away some day; the PATH_PROG_WITH_TEST already does it.
  dnl Test whether we really found GNU xgettext.
  if test "$XGETTEXT" != ":"; then
    dnl If it is no GNU xgettext we define it as : so that the
    dnl Makefiles still can work.
    if $XGETTEXT --omit-header --copyright-holder= --msgid-bugs-address= /dev/null >/dev/null 2>&1 &&
       (if $XGETTEXT --omit-header --copyright-holder= --msgid-bugs-address= /dev/null 2>&1 >/dev/null | grep usage >/dev/null; then exit 1; else exit 0; fi); then
      : ;
    else
      AC_MSG_RESULT(
        [found xgettext program is not GNU xgettext; ignore it])
      XGETTEXT=":"
    fi
    dnl Remove leftover from FreeBSD xgettext call.
    rm -f messages.po
  fi

  AC_OUTPUT_COMMANDS([
    for ac_file in $CONFIG_FILES; do
      # Support "outfile[:infile[:infile...]]"
      case "$ac_file" in
        *:*) ac_file=`echo "$ac_file"|sed 's%:.*%%'` ;;
      esac
      # PO directories have a Makefile.in generated from Makefile.in.in.
      case "$ac_file" in */Makefile.in)
        # Adjust a relative srcdir.
        ac_dir=`echo "$ac_file"|sed 's%/[^/][^/]*$%%'`
        ac_dir_suffix="/`echo "$ac_dir"|sed 's%^\./%%'`"
        ac_dots=`echo "$ac_dir_suffix"|sed 's%/[^/]*%../%g'`
        # In autoconf-2.13 it is called $ac_given_srcdir.
        # In autoconf-2.50 it is called $srcdir.
        test -n "$ac_given_srcdir" || ac_given_srcdir="$srcdir"
        case "$ac_given_srcdir" in
          .)  top_srcdir=`echo $ac_dots|sed 's%/$%%'` ;;
          /*) top_srcdir="$ac_given_srcdir" ;;
          *)  top_srcdir="$ac_dots$ac_given_srcdir" ;;
        esac
        if test -f "$ac_given_srcdir/$ac_dir/POTFILES.in"; then
          rm -f "$ac_dir/POTFILES"
          test -n "$as_me" && echo "$as_me: creating $ac_dir/POTFILES" || echo "creating $ac_dir/POTFILES"
          cat "$ac_given_srcdir/$ac_dir/POTFILES.in" | sed -e "/^#/d" -e "/^[ 	]*\$/d" -e "s,.*,     $top_srcdir/& \\\\," | sed -e "\$s/\(.*\) \\\\/\1/" > "$ac_dir/POTFILES"
          POMAKEFILEDEPS="POTFILES.in"
          # ALL_LINGUAS, POFILES, GMOFILES, UPDATEPOFILES, DUMMYPOFILES depend
          # on $ac_dir but don't depend on user-specified configuration
          # parameters.
          if test -f "$ac_given_srcdir/$ac_dir/LINGUAS"; then
            # The LINGUAS file contains the set of available languages.
            if test -n "$OBSOLETE_ALL_LINGUAS"; then
              test -n "$as_me" && echo "$as_me: setting ALL_LINGUAS in configure.in is obsolete" || echo "setting ALL_LINGUAS in configure.in is obsolete"
            fi
            ALL_LINGUAS_=`sed -e "/^#/d" "$ac_given_srcdir/$ac_dir/LINGUAS"`
            # Hide the ALL_LINGUAS assigment from automake.
            eval 'ALL_LINGUAS''=$ALL_LINGUAS_'
            POMAKEFILEDEPS="$POMAKEFILEDEPS LINGUAS"
          else
            # The set of available languages was given in configure.in.
            eval 'ALL_LINGUAS''=$OBSOLETE_ALL_LINGUAS'
          fi
          case "$ac_given_srcdir" in
            .) srcdirpre= ;;
            *) srcdirpre='$(srcdir)/' ;;
          esac
          POFILES=
          GMOFILES=
          UPDATEPOFILES=
          DUMMYPOFILES=
          for lang in $ALL_LINGUAS; do
            POFILES="$POFILES $srcdirpre$lang.po"
            GMOFILES="$GMOFILES $srcdirpre$lang.gmo"
            UPDATEPOFILES="$UPDATEPOFILES $lang.po-update"
            DUMMYPOFILES="$DUMMYPOFILES $lang.nop"
          done
          # CATALOGS depends on both $ac_dir and the user's LINGUAS
          # environment variable.
          INST_LINGUAS=
          if test -n "$ALL_LINGUAS"; then
            for presentlang in $ALL_LINGUAS; do
              useit=no
              if test "%UNSET%" != "$LINGUAS"; then
                desiredlanguages="$LINGUAS"
              else
                desiredlanguages="$ALL_LINGUAS"
              fi
              for desiredlang in $desiredlanguages; do
                # Use the presentlang catalog if desiredlang is
                #   a. equal to presentlang, or
                #   b. a variant of presentlang (because in this case,
                #      presentlang can be used as a fallback for messages
                #      which are not translated in the desiredlang catalog).
                case "$desiredlang" in
                  "$presentlang"*) useit=yes;;
                esac
              done
              if test $useit = yes; then
                INST_LINGUAS="$INST_LINGUAS $presentlang"
              fi
            done
          fi
          CATALOGS=
          if test -n "$INST_LINGUAS"; then
            for lang in $INST_LINGUAS; do
              CATALOGS="$CATALOGS $lang.gmo"
            done
          fi
          test -n "$as_me" && echo "$as_me: creating $ac_dir/Makefile" || echo "creating $ac_dir/Makefile"
          sed -e "/^POTFILES =/r $ac_dir/POTFILES" -e "/^# Makevars/r $ac_given_srcdir/$ac_dir/Makevars" -e "s|@POFILES@|$POFILES|g" -e "s|@GMOFILES@|$GMOFILES|g" -e "s|@UPDATEPOFILES@|$UPDATEPOFILES|g" -e "s|@DUMMYPOFILES@|$DUMMYPOFILES|g" -e "s|@CATALOGS@|$CATALOGS|g" -e "s|@POMAKEFILEDEPS@|$POMAKEFILEDEPS|g" "$ac_dir/Makefile.in" > "$ac_dir/Makefile"
          for f in "$ac_given_srcdir/$ac_dir"/Rules-*; do
            if test -f "$f"; then
              case "$f" in
                *.orig | *.bak | *~) ;;
                *) cat "$f" >> "$ac_dir/Makefile" ;;
              esac
            fi
          done
        fi
        ;;
      esac
    done],
   [# Capture the value of obsolete ALL_LINGUAS because we need it to compute
    # POFILES, GMOFILES, UPDATEPOFILES, DUMMYPOFILES, CATALOGS. But hide it
    # from automake.
    eval 'OBSOLETE_ALL_LINGUAS''="$ALL_LINGUAS"'
    # Capture the value of LINGUAS because we need it to compute CATALOGS.
    LINGUAS="${LINGUAS-%UNSET%}"
   ])
])
# progtest.m4 serial 3 (gettext-0.12)
dnl Copyright (C) 1996-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.
dnl
dnl This file can can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1996.

# Search path for a program which passes the given test.

dnl AM_PATH_PROG_WITH_TEST(VARIABLE, PROG-TO-CHECK-FOR,
dnl   TEST-PERFORMED-ON-FOUND_PROGRAM [, VALUE-IF-NOT-FOUND [, PATH]])
AC_DEFUN([AM_PATH_PROG_WITH_TEST],
[
# Prepare PATH_SEPARATOR.
# The user is always right.
if test "${PATH_SEPARATOR+set}" != set; then
  echo "#! /bin/sh" >conf$$.sh
  echo  "exit 0"   >>conf$$.sh
  chmod +x conf$$.sh
  if (PATH="/nonexistent;."; conf$$.sh) >/dev/null 2>&1; then
    PATH_SEPARATOR=';'
  else
    PATH_SEPARATOR=:
  fi
  rm -f conf$$.sh
fi

# Find out how to test for executable files. Don't use a zero-byte file,
# as systems may use methods other than mode bits to determine executability.
cat >conf$$.file <<_ASEOF
#! /bin/sh
exit 0
_ASEOF
chmod +x conf$$.file
if test -x conf$$.file >/dev/null 2>&1; then
  ac_executable_p="test -x"
else
  ac_executable_p="test -f"
fi
rm -f conf$$.file

# Extract the first word of "$2", so it can be a program name with args.
set dummy $2; ac_word=[$]2
AC_MSG_CHECKING([for $ac_word])
AC_CACHE_VAL(ac_cv_path_$1,
[case "[$]$1" in
  [[\\/]]* | ?:[[\\/]]*)
    ac_cv_path_$1="[$]$1" # Let the user override the test with a path.
    ;;
  *)
    ac_save_IFS="$IFS"; IFS=$PATH_SEPARATOR
    for ac_dir in ifelse([$5], , $PATH, [$5]); do
      IFS="$ac_save_IFS"
      test -z "$ac_dir" && ac_dir=.
      for ac_exec_ext in '' $ac_executable_extensions; do
        if $ac_executable_p "$ac_dir/$ac_word$ac_exec_ext"; then
          if [$3]; then
            ac_cv_path_$1="$ac_dir/$ac_word$ac_exec_ext"
            break 2
          fi
        fi
      done
    done
    IFS="$ac_save_IFS"
dnl If no 4th arg is given, leave the cache variable unset,
dnl so AC_PATH_PROGS will keep looking.
ifelse([$4], , , [  test -z "[$]ac_cv_path_$1" && ac_cv_path_$1="$4"
])dnl
    ;;
esac])dnl
$1="$ac_cv_path_$1"
if test ifelse([$4], , [-n "[$]$1"], ["[$]$1" != "$4"]); then
  AC_MSG_RESULT([$]$1)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST($1)dnl
])
# stdint_h.m4 serial 3 (gettext-0.12)
dnl Copyright (C) 1997-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

# Define HAVE_STDINT_H_WITH_UINTMAX if <stdint.h> exists,
# doesn't clash with <sys/types.h>, and declares uintmax_t.

AC_DEFUN([jm_AC_HEADER_STDINT_H],
[
  AC_CACHE_CHECK([for stdint.h], jm_ac_cv_header_stdint_h,
  [AC_TRY_COMPILE(
    [#include <sys/types.h>
#include <stdint.h>],
    [uintmax_t i = (uintmax_t) -1;],
    jm_ac_cv_header_stdint_h=yes,
    jm_ac_cv_header_stdint_h=no)])
  if test $jm_ac_cv_header_stdint_h = yes; then
    AC_DEFINE_UNQUOTED(HAVE_STDINT_H_WITH_UINTMAX, 1,
      [Define if <stdint.h> exists, doesn't clash with <sys/types.h>,
       and declares uintmax_t. ])
  fi
])
# uintmax_t.m4 serial 7 (gettext-0.12)
dnl Copyright (C) 1997-2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

AC_PREREQ(2.13)

# Define uintmax_t to 'unsigned long' or 'unsigned long long'
# if it is not already defined in <stdint.h> or <inttypes.h>.

AC_DEFUN([jm_AC_TYPE_UINTMAX_T],
[
  AC_REQUIRE([jm_AC_HEADER_INTTYPES_H])
  AC_REQUIRE([jm_AC_HEADER_STDINT_H])
  if test $jm_ac_cv_header_inttypes_h = no && test $jm_ac_cv_header_stdint_h = no; then
    AC_REQUIRE([jm_AC_TYPE_UNSIGNED_LONG_LONG])
    test $ac_cv_type_unsigned_long_long = yes \
      && ac_type='unsigned long long' \
      || ac_type='unsigned long'
    AC_DEFINE_UNQUOTED(uintmax_t, $ac_type,
      [Define to unsigned long or unsigned long long
       if <stdint.h> and <inttypes.h> don't define.])
  else
    AC_DEFINE(HAVE_UINTMAX_T, 1,
      [Define if you have the 'uintmax_t' type in <stdint.h> or <inttypes.h>.])
  fi
])
# ulonglong.m4 serial 2 (fileutils-4.0.32, gettext-0.10.40)
dnl Copyright (C) 1999-2002 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

dnl From Paul Eggert.

AC_DEFUN([jm_AC_TYPE_UNSIGNED_LONG_LONG],
[
  AC_CACHE_CHECK([for unsigned long long], ac_cv_type_unsigned_long_long,
  [AC_TRY_LINK([unsigned long long ull = 1; int i = 63;],
    [unsigned long long ullmax = (unsigned long long) -1;
     return ull << i | ull >> i | ullmax / ull | ullmax % ull;],
    ac_cv_type_unsigned_long_long=yes,
    ac_cv_type_unsigned_long_long=no)])
  if test $ac_cv_type_unsigned_long_long = yes; then
    AC_DEFINE(HAVE_UNSIGNED_LONG_LONG, 1,
      [Define if you have the unsigned long long type.])
  fi
])
