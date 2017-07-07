AC_PREREQ(2.63)
AC_COPYRIGHT([Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007, 2008, 2009
Massachusetts Institute of Technology.
])
dnl
define([K5_TOPDIR],[.])dnl
dnl
AC_DEFUN(V5_SET_TOPDIR,[dnl
ac_reltopdir="K5_TOPDIR"
if test ! -r "$srcdir/K5_TOPDIR/aclocal.m4"; then
  AC_MSG_ERROR([Configure could not determine the relative topdir])
fi
ac_topdir=$srcdir/$ac_reltopdir
ac_config_fragdir=$ac_reltopdir/config
# echo "Looking for $srcdir/$ac_config_fragdir"
if test -d "$srcdir/$ac_config_fragdir"; then
  AC_CONFIG_AUX_DIR(K5_TOPDIR/config)
else
  AC_MSG_ERROR([can not find config/ directory in $ac_reltopdir])
fi
])dnl
dnl
dnl Version info.
dnl
pushdef([x],esyscmd([sed -n 's/#define \([A-Z0-9_]*\)[ \t]*\(.*\)/\1=\2/p' < ]K5_TOPDIR/patchlevel.h))
define([PL_KRB5_MAJOR_RELEASE],regexp(x,[KRB5_MAJOR_RELEASE=\(.*\)],[\1]))
ifelse(PL_KRB5_MAJOR_RELEASE,,[errprint([Can't determine KRB5_MAJOR_RELEASE value from patchlevel.h.
]) m4exit(1) dnl sometimes that does not work?
builtin(m4exit,1)])
define([PL_KRB5_MINOR_RELEASE],regexp(x,[KRB5_MINOR_RELEASE=\(.*\)],[\1]))
ifelse(PL_KRB5_MINOR_RELEASE,,[errprint([Can't determine KRB5_MINOR_RELEASE value from patchlevel.h.
]) m4exit(1) dnl sometimes that does not work?
builtin(m4exit,1)])
define([PL_KRB5_PATCHLEVEL],regexp(x,[KRB5_PATCHLEVEL=\(.*\)],[\1]))
ifelse(PL_KRB5_PATCHLEVEL,,[errprint([Can't determine KRB5_PATCHLEVEL value from patchlevel.h.
]) m4exit(1) dnl sometimes that does not work?
builtin(m4exit,1)])
define([PL_KRB5_RELTAIL],regexp(x,[KRB5_RELTAIL="\(.*\)"],[\1]))
dnl RELTAIL is allowed to not be defined.
popdef([x])
define([K5_VERSION],PL_KRB5_MAJOR_RELEASE.PL_KRB5_MINOR_RELEASE[]ifelse(PL_KRB5_PATCHLEVEL,0,,.PL_KRB5_PATCHLEVEL)ifelse(PL_KRB5_RELTAIL,,,-PL_KRB5_RELTAIL))
define([K5_BUGADDR],krb5-bugs@mit.edu)
define([K5_AC_INIT],[AC_INIT(Kerberos 5, K5_VERSION, K5_BUGADDR, krb5)
AC_CONFIG_SRCDIR($1)
build_dynobj=no])
dnl
dnl drop in standard rules for all configure files -- CONFIG_RULES
dnl
AC_DEFUN(CONFIG_RULES,[dnl
AC_REQUIRE([V5_SET_TOPDIR]) dnl
EXTRA_FILES=""
AC_SUBST(EXTRA_FILES)
dnl Consider using AC_USE_SYSTEM_EXTENSIONS when we require autoconf
dnl 2.59c or later, but be sure to test on Solaris first.
AC_DEFINE([_GNU_SOURCE], 1, [Define to enable extensions in glibc])
AC_DEFINE([__STDC_WANT_LIB_EXT1__], 1, [Define to enable C11 extensions])

WITH_CC dnl
AC_REQUIRE_CPP
if test -z "$LD" ; then LD=$CC; fi
AC_ARG_VAR(LD,[linker command [CC]])
AC_SUBST(LDFLAGS) dnl
KRB5_AC_CHOOSE_ET dnl
KRB5_AC_CHOOSE_SS dnl
KRB5_AC_CHOOSE_DB dnl
dnl allow stuff in tree to access deprecated stuff for now
dnl AC_DEFINE([KRB5_DEPRECATED], 1, [Define only if building in-tree])
AC_C_CONST dnl
WITH_NETLIB dnl
WITH_HESIOD dnl
KRB5_AC_MAINTAINER_MODE dnl
AC_ARG_PROGRAM dnl
dnl
dnl This identifies the top of the source tree relative to the directory 
dnl in which the configure file lives.
dnl
CONFIG_RELTOPDIR=$ac_reltopdir
AC_SUBST(CONFIG_RELTOPDIR)
lib_frag=$srcdir/$ac_config_fragdir/lib.in
AC_SUBST_FILE(lib_frag)
libobj_frag=$srcdir/$ac_config_fragdir/libobj.in
AC_SUBST_FILE(libobj_frag)
libnover_frag=$srcdir/$ac_config_fragdir/libnover.in
AC_SUBST_FILE(libnover_frag)
libpriv_frag=$srcdir/$ac_config_fragdir/libpriv.in
AC_SUBST_FILE(libpriv_frag)
libnodeps_frag=$srcdir/$ac_config_fragdir/libnodeps.in
AC_SUBST_FILE(libnodeps_frag)
dnl
KRB5_AC_PRAGMA_WEAK_REF
WITH_LDAP
KRB5_LIB_PARAMS
KRB5_AC_INITFINI
KRB5_AC_ENABLE_THREADS
KRB5_AC_FIND_DLOPEN
KRB5_AC_KEYRING_CCACHE
KRB5_AC_PERSISTENT_KEYRING
])dnl

dnl Maintainer mode, akin to what automake provides, 'cept we don't
dnl want to use automake right now.
AC_DEFUN([KRB5_AC_MAINTAINER_MODE],
[AC_ARG_ENABLE([maintainer-mode],
AC_HELP_STRING([--enable-maintainer-mode],[enable rebuilding of source files, Makefiles, etc]),
USE_MAINTAINER_MODE=$enableval,
USE_MAINTAINER_MODE=no)
if test "$USE_MAINTAINER_MODE" = yes; then
  MAINTAINER_MODE_TRUE=
  MAINTAINER_MODE_FALSE='#'
  AC_MSG_NOTICE(enabling maintainer mode)
else
  MAINTAINER_MODE_TRUE='#'
  MAINTAINER_MODE_FALSE=
fi
MAINT=$MAINTAINER_MODE_TRUE
AC_SUBST(MAINTAINER_MODE_TRUE)
AC_SUBST(MAINTAINER_MODE_FALSE)
AC_SUBST(MAINT)
])

dnl
AC_DEFUN([KRB5_AC_INITFINI],[
dnl Do we want initialization at load time?
AC_ARG_ENABLE([delayed-initialization],
AC_HELP_STRING([--disable-delayed-initialization],initialize library code when loaded @<:@delay until first use@:>@), , enable_delayed_initialization=yes)
case "$enable_delayed_initialization" in
  yes)
    AC_DEFINE(DELAY_INITIALIZER,1,[Define if library initialization should be delayed until first use]) ;;
  no) ;;
  *)  AC_MSG_ERROR(invalid option $enable_delayed_initialization for delayed-initialization) ;;
esac
dnl We always want finalization at unload time.
dnl
dnl Can we do things through gcc?
KRB5_AC_GCC_ATTRS
dnl How about with the linker?
if test -z "$use_linker_init_option" ; then
  AC_MSG_ERROR(ran INITFINI before checking shlib.conf?)
fi
if test "$use_linker_init_option" = yes; then
  AC_DEFINE(USE_LINKER_INIT_OPTION,1,[Define if link-time options for library initialization will be used])
fi
if test "$use_linker_fini_option" = yes; then
  AC_DEFINE(USE_LINKER_FINI_OPTION,1,[Define if link-time options for library finalization will be used])
fi
])

dnl find dlopen
AC_DEFUN([KRB5_AC_FIND_DLOPEN],[
old_LIBS="$LIBS"
DL_LIB=
AC_SEARCH_LIBS(dlopen, dl, [
if test "$ac_cv_search_dlopen" != "none required"; then
  DL_LIB=$ac_cv_search_dlopen
fi
LIBS="$old_LIBS"
AC_DEFINE(USE_DLOPEN,1,[Define if dlopen should be used])])
AC_SUBST(DL_LIB)
])


dnl Hack for now.
AC_DEFUN([KRB5_AC_ENABLE_THREADS],[
AC_ARG_ENABLE([thread-support],
AC_HELP_STRING([--disable-thread-support],don't enable thread support @<:@enabled@:>@), , enable_thread_support=yes)
if test "$enable_thread_support" = yes ; then
  AC_MSG_NOTICE(enabling thread support)
  AC_DEFINE(ENABLE_THREADS,1,[Define if thread support enabled])
fi
dnl Maybe this should be inside the conditional above?  Doesn't cache....
if test "$enable_thread_support" = yes; then
  ACX_PTHREAD(,[AC_MSG_ERROR([cannot determine options for enabling thread support; try --disable-thread-support])])
  AC_MSG_NOTICE(PTHREAD_CC = $PTHREAD_CC)
  AC_MSG_NOTICE(PTHREAD_CFLAGS = $PTHREAD_CFLAGS)
  AC_MSG_NOTICE(PTHREAD_LIBS = $PTHREAD_LIBS)
  dnl Not really needed -- if pthread.h isn't found, ACX_PTHREAD will fail.
  dnl AC_CHECK_HEADERS(pthread.h)
  # AIX and Tru64 don't support weak references, and don't have
  # stub versions of the pthread code in libc.
  case "${host_os}" in
    aix* | osf*)
      # On these platforms, we'll always pull in the thread support.
      LIBS="$LIBS $PTHREAD_LIBS"
      CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
      # We don't need to sometimes add the flags we've just folded in...
      PTHREAD_LIBS=
      PTHREAD_CFLAGS=
      ;;
    hpux*)
      # These are the flags that "gcc -pthread" adds.  But we don't
      # want "-pthread" because that has link-time effects, and we
      # don't exclude CFLAGS when linking.  *sigh*
      PTHREAD_CFLAGS="-D_REENTRANT -D_THREAD_SAFE -D_POSIX_C_SOURCE=199506L"
      ;;
    solaris2.[[1-9]])
      # On Solaris 10 with gcc 3.4.3, the autoconf archive macro doesn't
      # get the right result.   XXX What about Solaris 9 and earlier?
      if test "$GCC" = yes ; then
        PTHREAD_CFLAGS="-D_REENTRANT -pthreads"
      fi
      ;;
    solaris*)
      # On Solaris 10 with gcc 3.4.3, the autoconf archive macro doesn't
      # get the right result.
      if test "$GCC" = yes ; then
        PTHREAD_CFLAGS="-D_REENTRANT -pthreads"
      fi
      # On Solaris 10, the thread support is always available in libc.
      AC_DEFINE(NO_WEAK_PTHREADS,1,[Define if references to pthread routines should be non-weak.])
      ;;
  esac
  THREAD_SUPPORT=1
else
  PTHREAD_CC="$CC"
  PTHREAD_CFLAGS=""
  PTHREAD_LIBS=""
  THREAD_SUPPORT=0
fi
AC_SUBST(THREAD_SUPPORT)
dnl We want to know where these routines live, so on systems with weak
dnl reference support we can figure out whether or not the pthread library
dnl has been linked in.
dnl If we don't add any libraries for thread support, don't bother.
AC_CHECK_FUNCS(pthread_once pthread_rwlock_init)
old_CC="$CC"
test "$PTHREAD_CC" != "" && test "$ac_cv_c_compiler_gnu" = no && CC=$PTHREAD_CC
old_CFLAGS="$CFLAGS"
# On Solaris, -pthreads is added to CFLAGS, no extra explicit libraries.
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
AC_SUBST(PTHREAD_CFLAGS)
old_LIBS="$LIBS"
LIBS="$PTHREAD_LIBS $LIBS"
AC_MSG_NOTICE(rechecking with PTHREAD_... options)
AC_CHECK_LIB(c, pthread_rwlock_init,
  [AC_DEFINE(HAVE_PTHREAD_RWLOCK_INIT_IN_THREAD_LIB,1,[Define if pthread_rwlock_init is provided in the thread library.])])
LIBS="$old_LIBS"
CC="$old_CC"
CFLAGS="$old_CFLAGS"
])

dnl This is somewhat gross and should go away when the build system
dnl is revamped. -- tlyu
dnl DECLARE_SYS_ERRLIST - check for sys_errlist in libc
dnl
AC_DEFUN([DECLARE_SYS_ERRLIST],
[AC_CACHE_CHECK([for sys_errlist declaration], krb5_cv_decl_sys_errlist,
[AC_TRY_COMPILE([#include <stdio.h>
#include <errno.h>], [1+sys_nerr;],
krb5_cv_decl_sys_errlist=yes, krb5_cv_decl_sys_errlist=no)])
# assume sys_nerr won't be declared w/o being in libc
if test $krb5_cv_decl_sys_errlist = yes; then
  AC_DEFINE(SYS_ERRLIST_DECLARED,1,[Define if sys_errlist is defined in errno.h])
  AC_DEFINE(HAVE_SYS_ERRLIST,1,[Define if sys_errlist in libc])
else
  # This means that sys_errlist is not declared in errno.h, but may still
  # be in libc.
  AC_CACHE_CHECK([for sys_errlist in libc], krb5_cv_var_sys_errlist,
  [AC_TRY_LINK([extern int sys_nerr;], [if (1+sys_nerr < 0) return 1;],
  krb5_cv_var_sys_errlist=yes, krb5_cv_var_sys_errlist=no;)])
  if test $krb5_cv_var_sys_errlist = yes; then
    AC_DEFINE(HAVE_SYS_ERRLIST,1,[Define if sys_errlist in libc])
    # Do this cruft for backwards compatibility for now.
    AC_DEFINE(NEED_SYS_ERRLIST,1,[Define if need to declare sys_errlist])
  else
    AC_MSG_WARN([sys_errlist is neither in errno.h nor in libc])
  fi
fi])

dnl
dnl check for sigmask/sigprocmask -- CHECK_SIGPROCMASK
dnl
AC_DEFUN(CHECK_SIGPROCMASK,[
AC_MSG_CHECKING([for use of sigprocmask])
AC_CACHE_VAL(krb5_cv_func_sigprocmask_use,
[AC_TRY_LINK([#include <signal.h>], [sigprocmask(SIG_SETMASK,0,0);],
 krb5_cv_func_sigprocmask_use=yes,
AC_TRY_LINK([#include <signal.h>], [sigmask(1);], 
 krb5_cv_func_sigprocmask_use=no, krb5_cv_func_sigprocmask_use=yes))])
AC_MSG_RESULT($krb5_cv_func_sigprocmask_use)
if test $krb5_cv_func_sigprocmask_use = yes; then
 AC_DEFINE(USE_SIGPROCMASK,1,[Define if sigprocmask should be used])
fi
])dnl
dnl
AC_DEFUN(AC_PROG_ARCHIVE, [AC_CHECK_PROG(ARCHIVE, ar, ar cqv, false)])dnl
AC_DEFUN(AC_PROG_ARCHIVE_ADD, [AC_CHECK_PROG(ARADD, ar, ar cruv, false)])dnl
dnl
dnl check for <dirent.h> -- CHECK_DIRENT
dnl (may need to be more complex later)
dnl
AC_DEFUN(CHECK_DIRENT,[
AC_CHECK_HEADER(dirent.h,AC_DEFINE(USE_DIRENT_H,1,[Define if you have dirent.h functionality]))])dnl
dnl
dnl check if union wait is defined, or if WAIT_USES_INT -- CHECK_WAIT_TYPE
dnl
AC_DEFUN(CHECK_WAIT_TYPE,[
AC_MSG_CHECKING([if argument to wait is int *])
AC_CACHE_VAL(krb5_cv_struct_wait,
dnl Test for prototype clash - if there is none - then assume int * works
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/wait.h>
extern pid_t wait(int *);],[], krb5_cv_struct_wait=no,dnl
dnl Else fallback on old stuff
[AC_TRY_COMPILE(
[#include <sys/wait.h>], [union wait i;
#ifdef WEXITSTATUS
  WEXITSTATUS (i);
#endif
], 
	krb5_cv_struct_wait=yes, krb5_cv_struct_wait=no)])])
AC_MSG_RESULT($krb5_cv_struct_wait)
if test $krb5_cv_struct_wait = no; then
	AC_DEFINE(WAIT_USES_INT,1,[Define if wait takes int as a argument])
fi
])dnl
dnl
dnl check for POSIX signal handling -- CHECK_SIGNALS
dnl
AC_DEFUN(CHECK_SIGNALS,[
AC_CHECK_FUNC(sigprocmask,
AC_MSG_CHECKING(for sigset_t and POSIX_SIGNALS)
AC_CACHE_VAL(krb5_cv_type_sigset_t,
[AC_TRY_COMPILE(
[#include <signal.h>],
[sigset_t x],
krb5_cv_type_sigset_t=yes, krb5_cv_type_sigset_t=no)])
AC_MSG_RESULT($krb5_cv_type_sigset_t)
if test $krb5_cv_type_sigset_t = yes; then
  AC_DEFINE(POSIX_SIGNALS,1,[Define if POSIX signal handling is used])
fi
)])dnl
dnl
dnl check for signal type
dnl
dnl AC_RETSIGTYPE isn't quite right, but almost.
AC_DEFUN(KRB5_SIGTYPE,[
AC_MSG_CHECKING([POSIX signal handlers])
AC_CACHE_VAL(krb5_cv_has_posix_signals,
[AC_TRY_COMPILE(
[#include <sys/types.h>
#include <signal.h>
#ifdef signal
#undef signal
#endif
extern void (*signal ()) ();], [],
krb5_cv_has_posix_signals=yes, krb5_cv_has_posix_signals=no)])
AC_MSG_RESULT($krb5_cv_has_posix_signals)
if test $krb5_cv_has_posix_signals = yes; then
   stype=void
   AC_DEFINE(POSIX_SIGTYPE, 1, [Define if POSIX signal handlers are used])
else
  if test $ac_cv_type_signal = void; then
     stype=void
  else
     stype=int
  fi
fi
AC_DEFINE_UNQUOTED(krb5_sigtype, $stype, [Define krb5_sigtype to type of signal handler])dnl
])dnl
dnl
dnl check for POSIX setjmp/longjmp -- CHECK_SETJMP
dnl
AC_DEFUN(CHECK_SETJMP,[
AC_CHECK_FUNC(sigsetjmp,
AC_MSG_CHECKING(for sigjmp_buf)
AC_CACHE_VAL(krb5_cv_struct_sigjmp_buf,
[AC_TRY_COMPILE(
[#include <setjmp.h>],[sigjmp_buf x],
krb5_cv_struct_sigjmp_buf=yes,krb5_cv_struct_sigjmp_buf=no)])
AC_MSG_RESULT($krb5_cv_struct_sigjmp_buf)
if test $krb5_cv_struct_sigjmp_buf = yes; then
  AC_DEFINE(POSIX_SETJMP,1,[Define if setjmp indicates POSIX interface])
fi
)])dnl
dnl
dnl Check for IPv6 compile-time support.
dnl
AC_DEFUN(KRB5_AC_INET6,[
AC_CHECK_HEADERS(sys/types.h sys/socket.h netinet/in.h netdb.h)
AC_CHECK_FUNCS(inet_ntop inet_pton getnameinfo)
dnl getaddrinfo test needs netdb.h, for proper compilation on alpha
dnl under OSF/1^H^H^H^H^HDigital^H^H^H^H^H^H^HTru64 UNIX, where it's
dnl a macro
AC_MSG_CHECKING(for getaddrinfo)
AC_CACHE_VAL(ac_cv_func_getaddrinfo,
[AC_TRY_LINK([#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[
struct addrinfo *ai;
getaddrinfo("kerberos.mit.edu", "echo", 0, &ai);
], ac_cv_func_getaddrinfo=yes, ac_cv_func_getaddrinfo=no)])
AC_MSG_RESULT($ac_cv_func_getaddrinfo)
if test $ac_cv_func_getaddrinfo = yes; then
  AC_DEFINE(HAVE_GETADDRINFO,1,[Define if you have the getaddrinfo function])
fi
dnl
AC_REQUIRE([KRB5_SOCKADDR_SA_LEN])dnl
AC_MSG_CHECKING(for IPv6 compile-time support without -DINET6)
AC_CACHE_VAL(krb5_cv_inet6,[
if test "$ac_cv_func_inet_ntop" != "yes" ; then
  krb5_cv_inet6=no
else
AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
],[
  struct sockaddr_in6 in;
  AF_INET6;
  IN6_IS_ADDR_LINKLOCAL (&in.sin6_addr);
],krb5_cv_inet6=yes,krb5_cv_inet6=no)])
fi
AC_MSG_RESULT($krb5_cv_inet6)
if test "$krb5_cv_inet6" = no && test "$ac_cv_func_inet_ntop" = yes; then
AC_MSG_CHECKING(for IPv6 compile-time support with -DINET6)
AC_CACHE_VAL(krb5_cv_inet6_with_dinet6,[
old_CC="$CC"
CC="$CC -DINET6"
AC_TRY_COMPILE([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
],[
  struct sockaddr_in6 in;
  AF_INET6;
  IN6_IS_ADDR_LINKLOCAL (&in.sin6_addr);
],krb5_cv_inet6_with_dinet6=yes,krb5_cv_inet6_with_dinet6=no)
CC="$old_CC"])
AC_MSG_RESULT($krb5_cv_inet6_with_dinet6)
fi
if test $krb5_cv_inet6 = yes || test "$krb5_cv_inet6_with_dinet6" = yes; then
  if test "$krb5_cv_inet6_with_dinet6" = yes; then
    AC_DEFINE(INET6,1,[May need to be defined to enable IPv6 support, for example on IRIX])
  fi
fi
])dnl
dnl
AC_DEFUN(KRB5_AC_CHECK_FOR_CFLAGS,[
AC_BEFORE([$0],[AC_PROG_CC])
AC_BEFORE([$0],[AC_PROG_CXX])
krb5_ac_cflags_set=${CFLAGS+set}
krb5_ac_cxxflags_set=${CXXFLAGS+set}
krb5_ac_warn_cflags_set=${WARN_CFLAGS+set}
krb5_ac_warn_cxxflags_set=${WARN_CXXFLAGS+set}
])
dnl
AC_DEFUN(TRY_WARN_CC_FLAG,[dnl
  cachevar=`echo "krb5_cv_cc_flag_$1" | sed -e s/=/_eq_/g -e s/-/_dash_/g -e s/[[^a-zA-Z0-9_]]/_/g`
  AC_CACHE_CHECK([if C compiler supports $1], [$cachevar],
  [# first try without, then with
  AC_TRY_COMPILE([], 1;,
    [old_cflags="$CFLAGS"
     CFLAGS="$CFLAGS $1"
     AC_TRY_COMPILE([], 1;, eval $cachevar=yes, eval $cachevar=no)
     CFLAGS="$old_cflags"],
    [AC_MSG_ERROR(compiling simple test program with $CFLAGS failed)])])
  if eval test '"${'$cachevar'}"' = yes; then
    WARN_CFLAGS="$WARN_CFLAGS $1"
  fi
  eval flag_supported='${'$cachevar'}'
])dnl
dnl
AC_DEFUN(WITH_CC,[dnl
AC_REQUIRE([KRB5_AC_CHECK_FOR_CFLAGS])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_CXX])dnl
if test $ac_cv_c_compiler_gnu = yes ; then
     HAVE_GCC=yes
     else HAVE_GCC=
fi
AC_SUBST(HAVE_GCC)
AC_CACHE_CHECK([for GNU linker], krb5_cv_prog_gnu_ld,
[krb5_cv_prog_gnu_ld=no
if test "$GCC" = yes; then
  if AC_TRY_COMMAND([$CC -Wl,-v 2>&1 dnl
			| grep "GNU ld" > /dev/null]); then
    krb5_cv_prog_gnu_ld=yes
  fi
fi])
AC_ARG_WITH([size-optimizations],
[  --with-size-optimizations enable a few optimizations to reduce code size
                          possibly at some run-time cost],
,
withval=no)
if test "$withval" = yes; then
  AC_DEFINE(CONFIG_SMALL,1,[Define to reduce code size even if it means more cpu usage])
fi
# -Wno-long-long, if needed, for k5-platform.h without inttypes.h etc.
extra_gcc_warn_opts="-Wall -Wcast-align -Wshadow"
# -Wmissing-prototypes
if test "$GCC" = yes ; then
  # Putting this here means we get -Os after -O2, which works.
  if test "$with_size_optimizations" = yes && test "x$krb5_ac_cflags_set" != xset; then
    AC_MSG_NOTICE(adding -Os optimization option)
    case "$CFLAGS" in
      "-g -O2") CFLAGS="-g -Os" ;;
      "-O2")    CFLAGS="-Os" ;;
      *)        CFLAGS="$CFLAGS -Os" ;;
    esac
  fi
  if test "x$krb5_ac_warn_cflags_set" = xset ; then
    AC_MSG_NOTICE(not adding extra gcc warning flags because WARN_CFLAGS was set)
  else
    AC_MSG_NOTICE(adding extra warning flags for gcc)
    WARN_CFLAGS="$WARN_CFLAGS $extra_gcc_warn_opts -Wmissing-prototypes"
    if test "`uname -s`" = Darwin ; then
      AC_MSG_NOTICE(skipping pedantic warnings on Darwin)
    elif test "`uname -s`" = Linux ; then
      AC_MSG_NOTICE(skipping pedantic warnings on Linux)
    else
      WARN_CFLAGS="$WARN_CFLAGS -pedantic"
    fi
    if test "$ac_cv_cxx_compiler_gnu" = yes; then
      if test "x$krb5_ac_warn_cxxflags_set" = xset ; then
        AC_MSG_NOTICE(not adding extra g++ warnings because WARN_CXXFLAGS was set)
      else
        AC_MSG_NOTICE(adding extra warning flags for g++)
        WARN_CXXFLAGS="$WARN_CXXFLAGS $extra_gcc_warn_opts"
      fi
    fi
    # Currently, G++ does not support -Wno-format-zero-length.
    TRY_WARN_CC_FLAG(-Wno-format-zero-length)
    # Other flags here may not be supported on some versions of
    # gcc that people want to use.
    for flag in overflow strict-overflow missing-format-attribute missing-prototypes return-type missing-braces parentheses switch unused-function unused-label unused-variable unused-value unknown-pragmas sign-compare newline-eof error=uninitialized error=pointer-arith error=int-conversion error=incompatible-pointer-types error=discarded-qualifiers ; do
      TRY_WARN_CC_FLAG(-W$flag)
    done
    #  old-style-definition? generates many, many warnings
    #
    # Warnings that we'd like to turn into errors on versions of gcc
    # that support promoting only specific warnings to errors, but
    # we'll take as warnings on older compilers.  (If such a warning
    # is added after the -Werror=foo feature, you can just put
    # error=foo in the above list, and skip the test for the
    # warning-only form.)  At least in some versions, -Werror= doesn't
    # seem to make the conditions actual errors, but still issues
    # warnings; I guess we'll take what we can get.
    #
    # We're currently targeting C89+, not C99, so disallow some
    # constructs.
    for flag in declaration-after-statement ; do
      TRY_WARN_CC_FLAG(-Werror=$flag)
      if test "$flag_supported" = no; then
        TRY_WARN_CC_FLAG(-W$flag)
      fi
    done
    # We require function declarations now.
    #
    # In some compiler versions -- e.g., "gcc version 4.2.1 (Apple
    # Inc. build 5664)" -- the -Werror- option works, but the -Werror=
    # version doesn't cause implicitly declared functions to be
    # flagged as errors.  If neither works, -Wall implies
    # -Wimplicit-function-declaration so don't bother.
    TRY_WARN_CC_FLAG(-Werror-implicit-function-declaration)
    if test "implicit-function-declaration_supported" = no; then
      TRY_WARN_CC_FLAG(-Werror=implicit-function-declaration)
    fi
    #
  fi
  if test "`uname -s`" = Darwin ; then
    # Someday this should be a feature test.
    # One current (Jaguar = OS 10.2) problem:
    # Archive library with foo.o undef sym X and bar.o common sym X,
    # if foo.o is pulled in at link time, bar.o may not be, causing
    # the linker to complain.
    # Dynamic library problems too?
    case "$CC $CFLAGS" in
    *-fcommon*) ;; # why someone would do this, I don't know
    *-fno-common*) ;; # okay, they're already doing the right thing
    *)
      AC_MSG_NOTICE(disabling the use of common storage on Darwin)
      CFLAGS="$CFLAGS -fno-common"
      ;;
    esac
    case "$LD $LDFLAGS" in
    *-Wl,-search_paths_first*) ;;
    *) LDFLAGS="${LDFLAGS} -Wl,-search_paths_first" ;;
    esac
  fi
else
  if test "`uname -s`" = AIX ; then
    # Using AIX but not GCC, assume native compiler.
    # The native compiler appears not to give a nonzero exit
    # status for certain classes of errors, like missing arguments
    # in function calls.  Let's try to fix that with -qhalt=e.
    case "$CC $CFLAGS" in
      *-qhalt=*) ;;
      *)
	CFLAGS="$CFLAGS -qhalt=e"
	AC_MSG_NOTICE(adding -qhalt=e for better error reporting)
	;;
    esac
    # Also, the optimizer isn't turned on by default, which means
    # the static inline functions get left in random object files,
    # leading to references to pthread_mutex_lock from anything that
    # includes k5-int.h whether it uses threads or not.
    case "$CC $CFLAGS" in
      *-O*) ;;
      *)
	CFLAGS="$CFLAGS -O"
	AC_MSG_NOTICE(adding -O for inline thread-support function elimination)
	;;
    esac
  fi
  if test "`uname -s`" = SunOS ; then
    # Using Solaris but not GCC, assume Sunsoft compiler.
    # We have some error-out-on-warning options available.
    # Sunsoft 12 compiler defaults to -xc99=all, it appears, so "inline"
    # works, but it also means that declaration-in-code warnings won't
    # be issued.
    # -v -fd -errwarn=E_DECLARATION_IN_CODE ...
    WARN_CFLAGS="-errtags=yes -errwarn=E_BAD_PTR_INT_COMBINATION,E_BAD_PTR_INT_COMB_ARG,E_PTR_TO_VOID_IN_ARITHMETIC,E_NO_IMPLICIT_DECL_ALLOWED,E_ATTRIBUTE_PARAM_UNDEFINED"
    WARN_CXXFLAGS="-errtags=yes +w +w2 -xport64"
  fi
fi
AC_SUBST(WARN_CFLAGS)
AC_SUBST(WARN_CXXFLAGS)
])dnl
dnl
dnl
dnl check for yylineno -- HAVE_YYLINENO
dnl
AC_DEFUN(HAVE_YYLINENO,[dnl
AC_REQUIRE_CPP()AC_REQUIRE([AC_PROG_LEX])dnl
AC_MSG_CHECKING([for yylineno declaration])
AC_CACHE_VAL(krb5_cv_type_yylineno,
# some systems have yylineno, others don't...
  echo '%%
%%' | ${LEX} -t > conftest.out
  if egrep yylineno conftest.out >/dev/null 2>&1; then
	krb5_cv_type_yylineno=yes
  else
	krb5_cv_type_yylineno=no
  fi
  rm -f conftest.out)
  AC_MSG_RESULT($krb5_cv_type_yylineno)
  if test $krb5_cv_type_yylineno = no; then
	AC_DEFINE(NO_YYLINENO, 1, [Define if lex produes code with yylineno])
  fi
])dnl
dnl
dnl K5_GEN_MAKEFILE([dir, [frags]])
dnl
AC_DEFUN(K5_GEN_MAKEFILE,[dnl
ifelse($1, ,[_K5_GEN_MAKEFILE(.,$2)],[_K5_GEN_MAKEFILE($1,$2)])
])
dnl
dnl _K5_GEN_MAKEFILE(dir, [frags])
dnl  dir must be present in this case
dnl  Note: Be careful in quoting. 
dnl        The ac_foreach generates the list of fragments to include
dnl        or "" if $2 is empty
AC_DEFUN(_K5_GEN_MAKEFILE,[dnl
AC_CONFIG_FILES([$1/Makefile:$srcdir/]K5_TOPDIR[/config/pre.in:$1/Makefile.in:$1/deps:$srcdir/]K5_TOPDIR[/config/post.in])
])
dnl
dnl K5_GEN_FILE( <ac_output arguments> )
dnl
AC_DEFUN(K5_GEN_FILE,[AC_CONFIG_FILES($1)])dnl
dnl
dnl K5_AC_OUTPUT
dnl    Note: Adds the variables to config.status for individual 
dnl          Makefile generation from config.status
AC_DEFUN(K5_AC_OUTPUT,[AC_OUTPUT])dnl
dnl
dnl V5_AC_OUTPUT_MAKEFILE
dnl
AC_DEFUN(V5_AC_OUTPUT_MAKEFILE,
[ifelse($1, , [_V5_AC_OUTPUT_MAKEFILE(.,$2)],[_V5_AC_OUTPUT_MAKEFILE($1,$2)])])
dnl
define(_V5_AC_OUTPUT_MAKEFILE,
[ifelse($2, , ,AC_CONFIG_FILES($2))
AC_FOREACH([DIR], [$1],dnl
 [AC_CONFIG_FILES(DIR[/Makefile:$srcdir/]K5_TOPDIR[/config/pre.in:]DIR[/Makefile.in:]DIR[/deps:$srcdir/]K5_TOPDIR[/config/post.in])])
K5_AC_OUTPUT])dnl
dnl
dnl
dnl KRB5_SOCKADDR_SA_LEN: define HAVE_SA_LEN if sockaddr contains the sa_len
dnl component
dnl
AC_DEFUN([KRB5_SOCKADDR_SA_LEN],[ dnl
AC_CHECK_MEMBER(struct sockaddr.sa_len,
  AC_DEFINE(HAVE_SA_LEN,1,[Define if struct sockaddr contains sa_len])
,,[#include <sys/types.h>
#include <sys/socket.h>])])
dnl
dnl WITH_NETLIB
dnl 
dnl
AC_DEFUN(WITH_NETLIB,[
AC_ARG_WITH([netlib],
AC_HELP_STRING([--with-netlib=LIBS], use user defined resolver library),
[  if test "$withval" = yes -o "$withval" = no ; then
	AC_MSG_RESULT("netlib will link with C library resolver only")
  else
	LIBS="$LIBS $withval"
	AC_MSG_RESULT("netlib will use \'$withval\'")
  fi
],dnl
[AC_LIBRARY_NET]
)])dnl
dnl
dnl
AC_DEFUN(KRB5_AC_NEED_DAEMON, [
KRB5_NEED_PROTO([#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif],daemon,1)])dnl

dnl
dnl KRB5_AC_NEED_LIBGEN --- check if libgen needs to be linked in for
dnl 				compile/step	
dnl
dnl
AC_DEFUN(KRB5_AC_NEED_LIBGEN,[
AC_REQUIRE([AC_PROG_CC])dnl
dnl
dnl regcomp is present but non-functional on Solaris 2.4
dnl
AC_MSG_CHECKING(for working regcomp)
AC_CACHE_VAL(ac_cv_func_regcomp,[
AC_TRY_RUN([
#include <sys/types.h>
#include <regex.h>
regex_t x; regmatch_t m;
int main() { return regcomp(&x,"pat.*",0) || regexec(&x,"pattern",1,&m,0); }
], ac_cv_func_regcomp=yes, ac_cv_func_regcomp=no, AC_MSG_ERROR([Cannot test regcomp when cross compiling]))])
AC_MSG_RESULT($ac_cv_func_regcomp)
test $ac_cv_func_regcomp = yes && AC_DEFINE(HAVE_REGCOMP,1,[Define if regcomp exists and functions])
dnl
dnl Check for the compile and step functions - only if regcomp is not available
dnl
if test $ac_cv_func_regcomp = no; then
 save_LIBS="$LIBS"
 LIBS=-lgen
dnl this will fail if there's no compile/step in -lgen, or if there's
dnl no -lgen.  This is fine.
 AC_CHECK_FUNCS(compile step)
 LIBS="$save_LIBS"
dnl
dnl Set GEN_LIB if necessary 
dnl
 AC_CHECK_LIB(gen, compile, GEN_LIB=-lgen, GEN_LIB=)
 AC_SUBST(GEN_LIB)
fi
])
dnl
dnl KRB5_AC_REGEX_FUNCS --- check for different regular expression 
dnl				support functions
dnl
AC_DEFUN(KRB5_AC_REGEX_FUNCS,[
AC_CHECK_FUNCS(re_comp re_exec regexec)
AC_REQUIRE([KRB5_AC_NEED_LIBGEN])dnl
])dnl
dnl
dnl AC_KRB5_TCL_FIND_CONFIG (uses tcl_dir)
dnl
AC_DEFUN(AC_KRB5_TCL_FIND_CONFIG,[
AC_REQUIRE([KRB5_LIB_AUX])dnl
AC_MSG_CHECKING(for tclConfig.sh)
dnl On Debian, we might be given --with-tcl=/usr, or tclsh might
dnl point us to /usr/lib/tcl8.4; either way, we need to find
dnl /usr/lib/tcl8.4/tclConfig.sh.
dnl On NetBSD, we might be given --with-tcl=/usr/pkg, or tclsh
dnl might point us to /usr/pkg/lib/tcl8.4; we need to find
dnl /usr/pkg/lib/tclConfig.sh.
if test -r "$tcl_dir/lib/tclConfig.sh" ; then
  tcl_conf="$tcl_dir/lib/tclConfig.sh"
elif test -r "$tcl_dir/tclConfig.sh" ; then
  tcl_conf="$tcl_dir/tclConfig.sh"
elif test -r "$tcl_dir/../tclConfig.sh" ; then
  tcl_conf="$tcl_dir/../tclConfig.sh"
else
  tcl_conf=
  lib="$tcl_dir/lib"
  changequote(<<,>>)dnl
  for d in "$lib" "$lib"/tcl7.[0-9] "$lib"/tcl8.[0-9] ; do
    if test -r "$d/tclConfig.sh" ; then
      tcl_conf="$tcl_conf $d/tclConfig.sh"
    fi
  done
  changequote([,])dnl
fi
if test -n "$tcl_conf" ; then
  AC_MSG_RESULT($tcl_conf)
else
  AC_MSG_RESULT(not found)
fi
tcl_ok_conf=
tcl_vers_maj=
tcl_vers_min=
old_CPPFLAGS=$CPPFLAGS
old_LIBS=$LIBS
old_LDFLAGS=$LDFLAGS
if test -n "$tcl_conf" ; then
  for file in $tcl_conf ; do
    TCL_MAJOR_VERSION=x ; TCL_MINOR_VERSION=x
    AC_MSG_CHECKING(Tcl info in $file)
    . $file
    v=$TCL_MAJOR_VERSION.$TCL_MINOR_VERSION
    if test -z "$tcl_vers_maj" \
	|| test "$tcl_vers_maj" -lt "$TCL_MAJOR_VERSION" \
	|| test "$tcl_vers_maj" = "$TCL_MAJOR_VERSION" -a "$tcl_vers_min" -lt "$TCL_MINOR_VERSION" ; then
      for incdir in "$TCL_PREFIX/include/tcl$v" "$TCL_PREFIX/include" ; do
	if test -r "$incdir/tcl.h" -o -r "$incdir/tcl/tcl.h" ; then
	  CPPFLAGS="$old_CPPFLAGS -I$incdir"
	  break
	fi
      done
      LIBS="$old_LIBS `eval echo x $TCL_LIB_SPEC $TCL_LIBS | sed 's/^x//'`"
      LDFLAGS="$old_LDFLAGS $TCL_LD_FLAGS"
      AC_TRY_LINK( , [Tcl_CreateInterp ();],
	tcl_ok_conf=$file
	tcl_vers_maj=$TCL_MAJOR_VERSION
	tcl_vers_min=$TCL_MINOR_VERSION
	AC_MSG_RESULT($v - working),
	AC_MSG_RESULT($v - compilation failed)
      )
    else
      AC_MSG_RESULT(older version $v)
    fi
  done
fi
CPPFLAGS=$old_CPPFLAGS
LIBS=$old_LIBS
LDFLAGS=$old_LDFLAGS
tcl_header=no
tcl_lib=no
if test -n "$tcl_ok_conf" ; then
  . $tcl_ok_conf
  TCL_INCLUDES=
  for incdir in "$TCL_PREFIX/include/tcl$v" "$TCL_PREFIX/include" ; do
    if test -r "$incdir/tcl.h" -o -r "$incdir/tcl/tcl.h" ; then
      if test "$incdir" != "/usr/include" ; then
        TCL_INCLUDES=-I$incdir
      fi
      break
    fi
  done
  # Need eval because the first-level expansion could reference
  # variables like ${TCL_DBGX}.
  eval TCL_LIBS='"'$TCL_LIB_SPEC $TCL_LIBS $TCL_DL_LIBS'"'
  TCL_LIBPATH="-L$TCL_EXEC_PREFIX/lib"
  TCL_RPATH=":$TCL_EXEC_PREFIX/lib"
  if test "$DEPLIBEXT" != "$SHLIBEXT" && test -n "$RPATH_FLAG"; then
    TCL_MAYBE_RPATH='$(RPATH_FLAG)'"$TCL_EXEC_PREFIX/lib$RPATH_TAIL"
  else
    TCL_MAYBE_RPATH=
  fi
  CPPFLAGS="$old_CPPFLAGS $TCL_INCLUDES"
  AC_CHECK_HEADER(tcl.h,AC_DEFINE(HAVE_TCL_H,1,[Define if tcl.h is available]) tcl_header=yes)
  if test $tcl_header=no; then
     AC_CHECK_HEADER(tcl/tcl.h,AC_DEFINE(HAVE_TCL_TCL_H,1,[Define if tcl/tcl.h is available]) tcl_header=yes)
  fi
  CPPFLAGS="$old_CPPFLAGS"
  tcl_lib=yes
else
  # If we read a tclConfig.sh file, it probably set this.
  TCL_LIBS=
fi  
AC_SUBST(TCL_INCLUDES)
AC_SUBST(TCL_LIBS)
AC_SUBST(TCL_LIBPATH)
AC_SUBST(TCL_RPATH)
AC_SUBST(TCL_MAYBE_RPATH)
])dnl
dnl
dnl AC_KRB5_TCL_TRYOLD
dnl attempt to use old search algorithm for locating tcl
dnl
AC_DEFUN(AC_KRB5_TCL_TRYOLD, [
AC_REQUIRE([KRB5_AC_FIND_DLOPEN])
AC_MSG_WARN([trying old tcl search code])
if test "$with_tcl" != yes -a "$with_tcl" != no; then
	TCL_INCLUDES=-I$with_tcl/include
	TCL_LIBPATH=-L$with_tcl/lib
	TCL_RPATH=:$with_tcl/lib
fi
if test "$with_tcl" != no ; then
	krb5_save_CPPFLAGS="$CPPFLAGS"
	krb5_save_LDFLAGS="$LDFLAGS"
	CPPFLAGS="$CPPFLAGS $TCL_INCLUDES"
	LDFLAGS="$LDFLAGS $TCL_LIBPATH"
	tcl_header=no
	AC_CHECK_HEADER(tcl.h,AC_DEFINE(HAVE_TCL_H,1,[Define if tcl.h found]) tcl_header=yes)
	if test $tcl_header=no; then
	   AC_CHECK_HEADER(tcl/tcl.h,AC_DEFINE(HAVE_TCL_TCL_H,1,[Define if tcl/tcl.h found]) tcl_header=yes)
	fi

	if test $tcl_header = yes ; then
		tcl_lib=no

		if test $tcl_lib = no; then
			AC_CHECK_LIB(tcl8.0, Tcl_CreateCommand, 
				TCL_LIBS="$TCL_LIBS -ltcl8.0 -lm $DL_LIB $LIBS"
				tcl_lib=yes,,-lm $DL_LIB)
		fi
		if test $tcl_lib = no; then
			AC_CHECK_LIB(tcl7.6, Tcl_CreateCommand, 
				TCL_LIBS="$TCL_LIBS -ltcl7.6 -lm $DL_LIB $LIBS"
				tcl_lib=yes,,-lm $DL_LIB)
		fi
		if test $tcl_lib = no; then
			AC_CHECK_LIB(tcl7.5, Tcl_CreateCommand, 
				TCL_LIBS="$TCL_LIBS -ltcl7.5 -lm $DL_LIB $LIBS"
				tcl_lib=yes,,-lm $DL_LIB)

		fi
		if test $tcl_lib = no ; then
			AC_CHECK_LIB(tcl, Tcl_CreateCommand, 
				TCL_LIBS="$TCL_LIBS -ltcl -lm $DL_LIB $LIBS"
				tcl_lib=yes,,-lm $DL_LIB)

		fi
		if test $tcl_lib = no ; then		
			AC_MSG_WARN("tcl.h found but not library")
		fi
	else
		AC_MSG_WARN(Could not find Tcl which is needed for the kadm5 tests)
		TCL_LIBS=
	fi
	CPPFLAGS="$krb5_save_CPPFLAGS"
	LDFLAGS="$krb5_save_LDFLAGS"
	AC_SUBST(TCL_INCLUDES)
	AC_SUBST(TCL_LIBS)
	AC_SUBST(TCL_LIBPATH)
	AC_SUBST(TCL_RPATH)
else
	AC_MSG_RESULT("Not looking for Tcl library")
fi
])dnl
dnl
dnl AC_KRB5_TCL - determine if the TCL library is present on system
dnl
AC_DEFUN(AC_KRB5_TCL,[
TCL_INCLUDES=
TCL_LIBPATH=
TCL_RPATH=
TCL_LIBS=
TCL_WITH=
tcl_dir=
AC_ARG_WITH(tcl,
[  --with-tcl=path         where Tcl resides], , with_tcl=try)
if test "$with_tcl" = no ; then
  true
elif test "$with_tcl" = yes -o "$with_tcl" = try ; then
  tcl_dir=/usr
  if test ! -r /usr/lib/tclConfig.sh; then
    cat >> conftest <<\EOF
puts "tcl_dir=$tcl_library"
EOF
    if tclsh conftest >conftest.out 2>/dev/null; then
      if grep tcl_dir= conftest.out >/dev/null 2>&1; then
        t=`sed s/tcl_dir=// conftest.out`
        tcl_dir=$t
      fi
    fi # tclsh ran script okay
  rm -f conftest conftest.out
  fi # no /usr/lib/tclConfig.sh
else
  tcl_dir=$with_tcl
fi
if test "$with_tcl" != no ; then
  AC_KRB5_TCL_FIND_CONFIG
  if test $tcl_lib = no ; then
    if test "$with_tcl" != try ; then
      AC_KRB5_TCL_TRYOLD
    else
      AC_MSG_WARN(Could not find Tcl which is needed for some tests)
    fi
  fi
fi
# If "yes" or pathname, error out if not found.
if test "$with_tcl" != no -a "$with_tcl" != try ; then
  if test "$tcl_header $tcl_lib" != "yes yes" ; then
    AC_MSG_ERROR(Could not find Tcl)
  fi
fi
])dnl

dnl
dnl WITH_HESIOD
dnl
AC_DEFUN(WITH_HESIOD,
[AC_ARG_WITH(hesiod, AC_HELP_STRING(--with-hesiod[=path], compile with hesiod support @<:@omitted@:>@),
	hesiod=$with_hesiod, with_hesiod=no)
if test "$with_hesiod" != "no"; then
	HESIOD_DEFS=-DHESIOD
	AC_CHECK_LIB(resolv, res_send, res_lib=-lresolv)
	if test "$hesiod" != "yes"; then
		HESIOD_LIBS="-L${hesiod}/lib -lhesiod $res_lib"
	else
		HESIOD_LIBS="-lhesiod $res_lib"
	fi
else
	HESIOD_DEFS=
	HESIOD_LIBS=
fi
AC_SUBST(HESIOD_DEFS)
AC_SUBST(HESIOD_LIBS)])


dnl
dnl KRB5_BUILD_LIBRARY
dnl
dnl Pull in the necessary stuff to create the libraries.

AC_DEFUN(KRB5_BUILD_LIBRARY,
[AC_REQUIRE([KRB5_LIB_AUX])dnl
AC_REQUIRE([AC_PROG_LN_S])dnl
AC_REQUIRE([AC_PROG_RANLIB])dnl
AC_REQUIRE([AC_PROG_ARCHIVE])dnl
AC_REQUIRE([AC_PROG_ARCHIVE_ADD])dnl
AC_REQUIRE([AC_PROG_INSTALL])dnl
AC_CHECK_PROG(AR, ar, ar, false)
if test "$AR" = "false"; then
  AC_MSG_ERROR([ar not found in PATH])
fi
AC_CHECK_PROG(PERL, perl, perl, false)
if test "$ac_cv_prog_PERL" = "false"; then
  AC_MSG_ERROR(Perl is now required for Kerberos builds.)
fi
AC_SUBST(LIBLIST)
AC_SUBST(LIBLINKS)
AC_SUBST(PLUGIN)
AC_SUBST(PLUGINLINK)
AC_SUBST(PLUGININST)
AC_SUBST(KDB5_PLUGIN_DEPLIBS)
AC_SUBST(KDB5_PLUGIN_LIBS)
AC_SUBST(MAKE_SHLIB_COMMAND)
AC_SUBST(SHLIB_RPATH_FLAGS)
AC_SUBST(SHLIB_EXPFLAGS)
AC_SUBST(SHLIB_EXPORT_FILE_DEP)
AC_SUBST(DYNOBJ_EXPDEPS)
AC_SUBST(DYNOBJ_EXPFLAGS)
AC_SUBST(INSTALL_SHLIB)
AC_SUBST(STLIBEXT)
AC_SUBST(SHLIBEXT)
AC_SUBST(SHLIBVEXT)
AC_SUBST(SHLIBSEXT)
AC_SUBST(DEPLIBEXT)
AC_SUBST(PFLIBEXT)
AC_SUBST(LIBINSTLIST)
AC_SUBST(DYNOBJEXT)
AC_SUBST(MAKE_DYNOBJ_COMMAND)
AC_SUBST(UNDEF_CHECK)
])

dnl
dnl KRB5_BUILD_LIBOBJS
dnl
dnl Pull in the necessary stuff to build library objects.

AC_DEFUN(KRB5_BUILD_LIBOBJS,
[AC_REQUIRE([KRB5_LIB_AUX])dnl
AC_SUBST(OBJLISTS)
AC_SUBST(STOBJEXT)
AC_SUBST(SHOBJEXT)
AC_SUBST(PFOBJEXT)
AC_SUBST(PICFLAGS)
AC_SUBST(PROFFLAGS)])

dnl
dnl KRB5_BUILD_PROGRAM
dnl
dnl Set variables to build a program.

AC_DEFUN(KRB5_BUILD_PROGRAM,
[AC_REQUIRE([KRB5_LIB_AUX])dnl
AC_REQUIRE([KRB5_AC_NEED_LIBGEN])dnl
AC_SUBST(CC_LINK)
AC_SUBST(CXX_LINK)
AC_SUBST(RPATH_FLAG)
AC_SUBST(PROG_RPATH_FLAGS)
AC_SUBST(DEPLIBEXT)])

dnl
dnl KRB5_RUN_FLAGS
dnl
dnl Set up environment for running dynamic executables out of build tree

AC_DEFUN(KRB5_RUN_FLAGS,
[AC_REQUIRE([KRB5_LIB_AUX])dnl
KRB5_RUN_ENV="$RUN_ENV"
KRB5_RUN_VARS="$RUN_VARS"
AC_SUBST(KRB5_RUN_ENV)
AC_SUBST(KRB5_RUN_VARS)])

dnl
dnl KRB5_LIB_AUX
dnl
dnl Parse configure options related to library building.

AC_DEFUN(KRB5_LIB_AUX,
[AC_REQUIRE([KRB5_LIB_PARAMS])dnl

AC_ARG_ENABLE([static],,, [enable_static=no])
AC_ARG_ENABLE([shared],,, [enable_shared=yes])

if test "x$enable_static" = "x$enable_shared"; then
  AC_MSG_ERROR([--enable-static must be specified with --disable-shared])
fi

AC_ARG_ENABLE([rpath],
AC_HELP_STRING([--disable-rpath],[suppress run path flags in link lines]),,
[enable_rpath=yes])

if test "x$enable_rpath" != xyes ; then
	# Unset the rpath flag values set by shlib.conf
	SHLIB_RPATH_FLAGS=
	RPATH_FLAG=
	PROG_RPATH_FLAGS=
fi

if test "$SHLIBEXT" = ".so-nobuild"; then
   AC_MSG_ERROR([Shared libraries are not yet supported on this platform.])
fi

DEPLIBEXT=$SHLIBEXT

if test "x$enable_static" = xyes; then
	AC_MSG_NOTICE([using static libraries])
	LIBLIST='lib$(LIBBASE)$(STLIBEXT)'
	LIBLINKS='$(TOPLIBD)/lib$(LIBBASE)$(STLIBEXT)'
	PLUGIN='libkrb5_$(LIBBASE)$(STLIBEXT)'
	PLUGINLINK='$(TOPLIBD)/libkrb5_$(LIBBASE)$(STLIBEXT)'
	PLUGININST=install-static
	OBJLISTS=OBJS.ST
	LIBINSTLIST=install-static
	DEPLIBEXT=$STLIBEXT
	AC_DEFINE([STATIC_PLUGINS], 1, [Define for static plugin linkage])

	KDB5_PLUGIN_DEPLIBS='$(TOPLIBD)/libkrb5_db2$(DEPLIBEXT)'
	KDB5_PLUGIN_LIBS='-lkrb5_db2'
	if test "x$OPENLDAP_PLUGIN" = xyes; then
		KDB5_PLUGIN_DEBLIBS=$KDB5_PLUGIN_DEPLIBS' $(TOPLIBD)/libkrb5_ldap$(DEPLIBEXT) $(TOPLIBD)/libkdb_ldap$(DEPLIBEXT)'
		KDB5_PLUGIN_LIBS=$KDB5_PLUGIN_LIBS' -lkrb5_kldap -lkdb_ldap $(LDAP_LIBS)'
	fi
	# kadm5srv_mit normally comes before kdb on the link line.  Add it
	# again after the KDB plugins, since they depend on it for XDR stuff.
	KDB5_PLUGIN_DEPLIBS=$KDB5_PLUGIN_DEPLIBS' $(TOPLIBD)/libkadm5srv_mit$(DEPLIBEXT)'
	KDB5_PLUGIN_LIBS=$KDB5_PLUGIN_LIBS' -lkadm5srv_mit'

	# avoid duplicate rules generation for AIX and such
	SHLIBEXT=.so-nobuild
	SHLIBVEXT=.so.v-nobuild
	SHLIBSEXT=.so.s-nobuild
else
	AC_MSG_NOTICE([using shared libraries])

	# Clear some stuff in case of AIX, etc.
	if test "$STLIBEXT" = "$SHLIBEXT" ; then
		STLIBEXT=.a-nobuild
	fi
	case "$SHLIBSEXT" in
	.so.s-nobuild)
		LIBLIST='lib$(LIBBASE)$(SHLIBEXT)'
		LIBLINKS='$(TOPLIBD)/lib$(LIBBASE)$(SHLIBEXT) $(TOPLIBD)/lib$(LIBBASE)$(SHLIBVEXT)'
		LIBINSTLIST="install-shared"
		;;
	*)
		LIBLIST='lib$(LIBBASE)$(SHLIBEXT) lib$(LIBBASE)$(SHLIBSEXT)'
		LIBLINKS='$(TOPLIBD)/lib$(LIBBASE)$(SHLIBEXT) $(TOPLIBD)/lib$(LIBBASE)$(SHLIBVEXT) $(TOPLIBD)/lib$(LIBBASE)$(SHLIBSEXT)'
		LIBINSTLIST="install-shlib-soname"
		;;
	esac
	OBJLISTS="OBJS.SH"
	PLUGIN='$(LIBBASE)$(DYNOBJEXT)'
	PLUGINLINK='../$(PLUGIN)'
	PLUGININST=install-plugin
	KDB5_PLUGIN_DEPLIBS=
	KDB5_PLUGIN_LIBS=
fi
CC_LINK="$CC_LINK_SHARED"
CXX_LINK="$CXX_LINK_SHARED"

if test -z "$LIBLIST"; then
	AC_MSG_ERROR([must enable one of shared or static libraries])
fi

# Check whether to build profiled libraries.
AC_ARG_ENABLE([profiled],
dnl [  --enable-profiled       build profiled libraries @<:@disabled@:>@]
,
[if test "$enableval" = yes; then
  AC_MSG_ERROR([Sorry, profiled libraries do not work in this release.])
fi])])

dnl
dnl KRB5_LIB_PARAMS
dnl
dnl Determine parameters related to libraries, e.g. various extensions.

AC_DEFUN(KRB5_LIB_PARAMS,
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
krb5_cv_host=$host
AC_SUBST(krb5_cv_host)
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([V5_SET_TOPDIR])dnl
. $ac_topdir/config/shlib.conf])
dnl
dnl The following was written by jhawk@mit.edu
dnl
dnl AC_LIBRARY_NET: Id: net.m4,v 1.4 1997/10/25 20:49:53 jhawk Exp 
dnl
dnl This test is for network applications that need socket() and
dnl gethostbyname() -ish functions.  Under Solaris, those applications need to
dnl link with "-lsocket -lnsl".  Under IRIX, they should *not* link with
dnl "-lsocket" because libsocket.a breaks a number of things (for instance:
dnl gethostbyname() under IRIX 5.2, and snoop sockets under most versions of
dnl IRIX).
dnl 
dnl Unfortunately, many application developers are not aware of this, and
dnl mistakenly write tests that cause -lsocket to be used under IRIX.  It is
dnl also easy to write tests that cause -lnsl to be used under operating
dnl systems where neither are necessary (or useful), such as SunOS 4.1.4, which
dnl uses -lnsl for TLI.
dnl 
dnl This test exists so that every application developer does not test this in
dnl a different, and subtly broken fashion.
dnl 
dnl It has been argued that this test should be broken up into two seperate
dnl tests, one for the resolver libraries, and one for the libraries necessary
dnl for using Sockets API. Unfortunately, the two are carefully intertwined and
dnl allowing the autoconf user to use them independantly potentially results in
dnl unfortunate ordering dependancies -- as such, such component macros would
dnl have to carefully use indirection and be aware if the other components were
dnl executed. Since other autoconf macros do not go to this trouble, and almost
dnl no applications use sockets without the resolver, this complexity has not
dnl been implemented.
dnl
dnl The check for libresolv is in case you are attempting to link statically
dnl and happen to have a libresolv.a lying around (and no libnsl.a).
dnl
AC_DEFUN(AC_LIBRARY_NET, [
   # Most operating systems have gethostbyname() in the default searched
   # libraries (i.e. libc):
   AC_CHECK_FUNC(gethostbyname, , [
     # Some OSes (eg. Solaris) place it in libnsl:
     AC_CHECK_LIB(nsl, gethostbyname, , [
       # Some strange OSes (SINIX) have it in libsocket:
       AC_CHECK_LIB(socket, gethostbyname, , [
          # Unfortunately libsocket sometimes depends on libnsl.
          # AC_CHECK_LIB's API is essentially broken so the following
          # ugliness is necessary:
          AC_CHECK_LIB(socket, gethostbyname,
             LIBS="-lsocket -lnsl $LIBS",
               [AC_CHECK_LIB(resolv, gethostbyname,
			     LIBS="-lresolv $LIBS" )],
             -lnsl)
       ])
     ])
   ])
  AC_CHECK_FUNC(socket, , AC_CHECK_LIB(socket, socket, ,
    AC_CHECK_LIB(socket, socket, LIBS="-lsocket -lnsl $LIBS", , -lnsl)))
  KRB5_AC_ENABLE_DNS
  if test "$enable_dns" = yes ; then
    # We assume that if libresolv exists we can link against it.
    # This may get us a gethostby* that doesn't respect nsswitch.
    AC_CHECK_LIB(resolv, main)

_KRB5_AC_CHECK_RES_FUNCS(res_ninit res_nclose res_ndestroy res_nsearch dnl
ns_initparse ns_name_uncompress dn_skipname res_search)
    if test $krb5_cv_func_res_nsearch = no \
      && test $krb5_cv_func_res_search = no; then
	# Attempt to link with res_search(), in case it's not prototyped.
	AC_CHECK_FUNC(res_search,
	  [AC_DEFINE(HAVE_RES_SEARCH, 1,
	    [Define to 1 if you have the `res_search' function])],
	  [AC_ERROR([cannot find res_nsearch or res_search])])
    fi
  fi
])
AC_DEFUN([_KRB5_AC_CHECK_RES_FUNCS],
[AC_FOREACH([AC_Func], [$1],
  [AH_TEMPLATE(AS_TR_CPP(HAVE_[]AC_Func),
               [Define to 1 if you have the `]AC_Func[' function.])])dnl
for krb5_func in $1; do
_KRB5_AC_CHECK_RES_FUNC($krb5_func)
done
])
AC_DEFUN([_KRB5_AC_CHECK_RES_FUNC], [
# Solaris 9 prototypes ns_name_uncompress() in arpa/nameser.h, but
# doesn't export it from libresolv.so, so we use extreme paranoia here
# and check both for the declaration and that we can link against the
# function.
AC_CACHE_CHECK([for $1], [krb5_cv_func_$1], [AC_TRY_LINK(
[#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
@%:@include <resolv.h>],
[/*
 * Use volatile, or else optimization can cause false positives.
 */
void (* volatile p)() = (void (*)())$1;],
			     [AS_VAR_SET(krb5_cv_func_$1, yes)],
			     [AS_VAR_SET(krb5_cv_func_$1, no)])])
AS_IF([test AS_VAR_GET(krb5_cv_func_$1) = yes],
      [AC_DEFINE_UNQUOTED(AS_TR_CPP([HAVE_$1]), 1,
			  [Define to 1 if you have the `$1' function])])[]dnl
])
dnl
dnl
dnl KRB5_AC_ENABLE_DNS
dnl
AC_DEFUN(KRB5_AC_ENABLE_DNS, [
enable_dns=yes
  AC_ARG_ENABLE([dns-for-realm],
[  --enable-dns-for-realm  enable DNS lookups of Kerberos realm names], ,
[enable_dns_for_realm=no])
  if test "$enable_dns_for_realm" = yes; then
    AC_DEFINE(KRB5_DNS_LOOKUP_REALM,1,[Define to enable DNS lookups of Kerberos realm names])
  fi

AC_DEFINE(KRB5_DNS_LOOKUP, 1,[Define for DNS support of locating realms and KDCs])

])
dnl
dnl
dnl Check if we need the prototype for a function - we give it a bogus 
dnl prototype and if it complains - then a valid prototype exists on the 
dnl system.
dnl
dnl KRB5_NEED_PROTO(includes, function, [bypass])
dnl if $3 set, don't see if library defined. 
dnl Useful for case where we will define in libkrb5 the function if need be
dnl but want to know if a prototype exists in either case on system.
dnl
AC_DEFUN([KRB5_NEED_PROTO], [
ifelse([$3], ,[if test "x$ac_cv_func_$2" = xyes; then])
AC_CACHE_CHECK([if $2 needs a prototype provided], krb5_cv_func_$2_noproto,
AC_TRY_COMPILE([$1],
[#undef $2
struct k5foo {int foo; } xx;
extern int $2 (struct k5foo*);
$2(&xx);
],
krb5_cv_func_$2_noproto=yes,krb5_cv_func_$2_noproto=no))
if test $krb5_cv_func_$2_noproto = yes; then
	AC_DEFINE([NEED_]translit($2, [a-z], [A-Z])[_PROTO], 1, dnl
[define if the system header files are missing prototype for $2()])
fi
ifelse([$3], ,[fi])
])
dnl
dnl =============================================================
dnl Internal function for testing for getpeername prototype
dnl
AC_DEFUN([KRB5_GETPEERNAME_ARGS],[
AC_DEFINE([GETPEERNAME_ARG2_TYPE],GETSOCKNAME_ARG2_TYPE,[Type of getpeername second argument.])
AC_DEFINE([GETPEERNAME_ARG3_TYPE],GETSOCKNAME_ARG3_TYPE,[Type of getpeername second argument.])
])
dnl
dnl =============================================================
dnl Internal function for testing for getsockname arguments
dnl
AC_DEFUN([TRY_GETSOCK_INT],[
krb5_lib_var=`echo "$1 $2" | sed 'y% ./+-*%___p_p%'`
AC_MSG_CHECKING([if getsockname() takes arguments $1 and $2])
AC_CACHE_VAL(krb5_cv_getsockname_proto_$krb5_lib_var,
[
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>
extern int getsockname(int, $1, $2);
],,eval "krb5_cv_getsockname_proto_$krb5_lib_var=yes",
    eval "krb5_cv_getsockname_proto_$krb5_lib_var=no")])
if eval "test \"`echo '$krb5_cv_getsockname_proto_'$krb5_lib_var`\" = yes"; then
	AC_MSG_RESULT(yes)
	sock_set=yes; res1="$1"; res2="$2"
else
	AC_MSG_RESULT(no)
fi
])
dnl
dnl Determines the types of the second and third arguments to getsockname().
dnl
AC_DEFUN([KRB5_GETSOCKNAME_ARGS],[
sock_set=no
for sock_arg1 in "struct sockaddr *" "void *"
do
  for sock_arg2 in "size_t *" "int *" "socklen_t *"
  do
	if test $sock_set = no; then
	  TRY_GETSOCK_INT($sock_arg1, $sock_arg2)
	fi
  done 
done
if test "$sock_set" = no; then
  AC_MSG_NOTICE(assuming struct sockaddr and socklen_t for getsockname args)
  res1="struct sockaddr *"
  res2="socklen_t *"
fi
res1=`echo "$res1" | tr -d '*' | sed -e 's/ *$//'`
res2=`echo "$res2" | tr -d '*' | sed -e 's/ *$//'`
AC_DEFINE_UNQUOTED([GETSOCKNAME_ARG2_TYPE],$res1,[Type of pointer target for argument 2 to getsockname])
AC_DEFINE_UNQUOTED([GETSOCKNAME_ARG3_TYPE],$res2,[Type of pointer target for argument 3 to getsockname])
])
dnl
dnl
AC_DEFUN([KRB5_AC_CHOOSE_ET],[
AC_ARG_WITH([system-et],
AC_HELP_STRING(--with-system-et,use system compile_et and -lcom_err @<:@default: build and install a local version@:>@))
AC_MSG_CHECKING(which version of com_err to use)
if test "x$with_system_et" = xyes ; then
  # This will be changed to "intlsys" if textdomain support is present.
  COM_ERR_VERSION=sys
  AC_MSG_RESULT(system)
else
  COM_ERR_VERSION=k5
  AC_MSG_RESULT(krb5)
fi
if test $COM_ERR_VERSION = sys; then
  # check for various functions we need
  AC_CHECK_LIB(com_err, add_error_table, :, AC_MSG_ERROR(cannot find add_error_table in com_err library))
  AC_CHECK_LIB(com_err, remove_error_table, :, AC_MSG_ERROR(cannot find remove_error_table in com_err library))
  # make sure compile_et provides "et_foo" name
  cat >> conf$$e.et <<EOF
error_table foo
error_code ERR_FOO, "foo"
end
EOF
  AC_CHECK_PROGS(compile_et,compile_et,false)
  if test "$compile_et" = false; then
    AC_MSG_ERROR(cannot find compile_et)
  fi
  AC_CACHE_CHECK(whether compile_et is useful,krb5_cv_compile_et_useful,[
  if compile_et conf$$e.et >/dev/null 2>&1 ; then true ; else
    AC_MSG_ERROR(execution failed)
  fi
  AC_TRY_COMPILE([#include "conf$$e.h"
      		 ],[ &et_foo_error_table; ],:,
		 [AC_MSG_ERROR(cannot use et_foo_error_table)])
  # Anything else we need to test for?
  rm -f conf$$e.c conf$$e.h
  krb5_cv_compile_et_useful=yes
  ])
  AC_CACHE_CHECK(whether compile_et supports --textdomain,
                 krb5_cv_compile_et_textdomain,[
  krb5_cv_compile_et_textdomain=no
  if compile_et --textdomain=xyzw conf$$e.et >/dev/null 2>&1 ; then
    if grep -q xyzw conf$$e.c; then
      krb5_cv_compile_et_textdomain=yes
    fi
  fi
  rm -f conf$$e.c conf$$e.h
  ])
  if test "$krb5_cv_compile_et_textdomain" = yes; then
    COM_ERR_VERSION=intlsys
  fi
  rm -f conf$$e.et
fi
AC_SUBST(COM_ERR_VERSION)
if test "$COM_ERR_VERSION" = k5 -o "$COM_ERR_VERSION" = intlsys; then
  AC_DEFINE(HAVE_COM_ERR_INTL,1,
            [Define if com_err has compatible gettext support])
fi
])
AC_DEFUN([KRB5_AC_CHOOSE_SS],[
AC_ARG_WITH(system-ss,
	    AC_HELP_STRING(--with-system-ss,use system -lss and mk_cmds @<:@private version@:>@))
AC_ARG_VAR(SS_LIB,[system libraries for 'ss' package [-lss]])
AC_MSG_CHECKING(which version of subsystem package to use)
if test "x$with_system_ss" = xyes ; then
  SS_VERSION=sys
  AC_MSG_RESULT(system)
  # todo: check for various libraries we might need
  # in the meantime...
  test "x${SS_LIB+set}" = xset || SS_LIB=-lss
  old_LIBS="$LIBS"
  LIBS="$LIBS $SS_LIB"
  AC_CACHE_CHECK(whether system ss package works, krb5_cv_system_ss_okay,[
  AC_TRY_RUN([
#include <ss/ss.h>
int main(int argc, char *argv[]) {
  if (argc == 42) {
    int i, err;
    i = ss_create_invocation("foo","foo","",0,&err);
    ss_listen(i);
  }
  return 0;
}], krb5_cv_system_ss_okay=yes, AC_MSG_ERROR(cannot run test program),
  krb5_cv_system_ss_okay="assumed")])
  LIBS="$old_LIBS"
  KRB5_NEED_PROTO([#include <ss/ss.h>],ss_execute_command,1)
else
  SS_VERSION=k5
  AC_MSG_RESULT(krb5)
fi
AC_SUBST(SS_LIB)
AC_SUBST(SS_VERSION)
])
dnl
AC_DEFUN([KRB5_AC_CHOOSE_DB],[
AC_ARG_WITH(system-db,
	    AC_HELP_STRING(--with-system-db,use system Berkeley db @<:@private version@:>@))
AC_ARG_VAR(DB_HEADER,[header file for system Berkeley db package [db.h]])
AC_ARG_VAR(DB_LIB,[library for system Berkeley db package [-ldb]])
if test "x$with_system_db" = xyes ; then
  DB_VERSION=sys
  # TODO: Do we have specific routines we should check for?
  # How about known, easily recognizable bugs?
  # We want to use bt_rseq in some cases, but no other version but
  # ours has it right now.
  #
  # Okay, check the variables.
  test "x${DB_HEADER+set}" = xset || DB_HEADER=db.h
  test "x${DB_LIB+set}" = xset || DB_LIB=-ldb
  #
  if test "x${DB_HEADER}" = xdb.h ; then
    DB_HEADER_VERSION=sys
  else
    DB_HEADER_VERSION=redirect
  fi
  KDB5_DB_LIB="$DB_LIB"
else
  DB_VERSION=k5
  AC_DEFINE(HAVE_BT_RSEQ,1,[Define if bt_rseq is available, for recursive btree traversal.])
  DB_HEADER=db.h
  DB_HEADER_VERSION=k5
  # libdb gets sucked into libkdb
  KDB5_DB_LIB=
  # needed for a couple of things that need libdb for its own sake
  DB_LIB=-ldb
fi
AC_SUBST(DB_VERSION)
AC_SUBST(DB_HEADER)
AC_SUBST(DB_HEADER_VERSION)
AC_SUBST(DB_LIB)
AC_SUBST(KDB5_DB_LIB)
])
dnl
dnl KRB5_AC_PRIOCNTL_HACK
dnl
dnl
AC_DEFUN([KRB5_AC_PRIOCNTL_HACK],
[AC_REQUIRE([AC_PROG_AWK])dnl
AC_REQUIRE([AC_LANG_COMPILER_REQUIRE])dnl
AC_CACHE_CHECK([whether to use priocntl hack], [krb5_cv_priocntl_hack],
[case $krb5_cv_host in
*-*-solaris2.9*)
	if test "$cross_compiling" = yes; then
		krb5_cv_priocntl_hack=yes
	else
		# Solaris patch 117171-11 (sparc) or 117172-11 (x86)
		# fixes the Solaris 9 bug where final pty output
		# gets lost on close.
		if showrev -p | $AWK 'BEGIN { e = 1 }
/Patch: 11717[[12]]/ { x = index[]([$]2, "-");
if (substr[]([$]2, x + 1, length([$]2) - x) >= 11)
{ e = 0 } else { e = 1 } }
END { exit e; }'; then
			krb5_cv_priocntl_hack=no
		else
			krb5_cv_priocntl_hack=yes
		fi
	fi
	;;
*)
	krb5_cv_priocntl_hack=no
	;;
esac])
if test "$krb5_cv_priocntl_hack" = yes; then
	PRIOCNTL_HACK=1
else
	PRIOCNTL_HACK=0
fi
AC_SUBST(PRIOCNTL_HACK)])
dnl
dnl
dnl KRB5_AC_GCC_ATTRS
AC_DEFUN([KRB5_AC_GCC_ATTRS],
[AC_CACHE_CHECK([for constructor/destructor attribute support],krb5_cv_attr_constructor_destructor,
[rm -f conftest.1 conftest.2
if test -r conftest.1 || test -r conftest.2 ; then
  AC_MSG_ERROR(write error in local file system?)
fi
true > conftest.1
true > conftest.2
if test -r conftest.1 && test -r conftest.2 ; then true ; else
  AC_MSG_ERROR(write error in local file system?)
fi
a=no
b=no
# blindly assume we have 'unlink'...
AC_TRY_RUN([void foo1() __attribute__((constructor));
void foo1() { unlink("conftest.1"); }
void foo2() __attribute__((destructor));
void foo2() { unlink("conftest.2"); }
int main () { return 0; }],
[test -r conftest.1 || a=yes
test -r conftest.2 || b=yes], , AC_MSG_ERROR(Cannot test for constructor/destructor support when cross compiling))
case $krb5_cv_host in
*-*-aix4.*)
	# Under AIX 4.3.3, at least, shared library destructor functions
	# appear to get executed in reverse link order (right to left),
	# so that a library's destructor function may run after that of
	# libraries it depends on, and may still have to access in the
	# destructor.
	#
	# That counts as "not working", for me, but it's a much more
	# complicated test case to set up.
	b=no
	;;
esac
krb5_cv_attr_constructor_destructor="$a,$b"
])
# Okay, krb5_cv_... should be set now.
case $krb5_cv_attr_constructor_destructor in
  yes,*)
    AC_DEFINE(CONSTRUCTOR_ATTR_WORKS,1,[Define if __attribute__((constructor)) works]) ;;
esac
case $krb5_cv_attr_constructor_destructor in
  *,yes)
    AC_DEFINE(DESTRUCTOR_ATTR_WORKS,1,[Define if __attribute__((destructor)) works]) ;;
esac
dnl End of attributes we care about right now.
])
dnl
dnl
dnl KRB5_AC_PRAGMA_WEAK_REF
AC_DEFUN([KRB5_AC_PRAGMA_WEAK_REF],
[AC_CACHE_CHECK([whether pragma weak references are supported],
krb5_cv_pragma_weak_ref,
[AC_TRY_LINK([#pragma weak flurbl
extern int flurbl(void);],[if (&flurbl != 0) return flurbl();],
krb5_cv_pragma_weak_ref=yes,krb5_cv_pragma_weak_ref=no)])
if test $krb5_cv_pragma_weak_ref = yes ; then
  AC_DEFINE(HAVE_PRAGMA_WEAK_REF,1,[Define if #pragma weak references work])
fi])
dnl
dnl
m4_include(config/ac-archive/acx_pthread.m4)
m4_include(config/ac-archive/relpaths.m4)
dnl
dnl
dnl
dnl --with-ldap=value
dnl
AC_DEFUN(WITH_LDAP,[
AC_ARG_WITH([ldap],
[  --with-ldap             compile OpenLDAP database backend module],
[case "$withval" in
    OPENLDAP) with_ldap=yes ;;
    yes | no) ;;
    *)  AC_MSG_ERROR(Invalid option value --with-ldap="$withval") ;;
esac], with_ldap=no)dnl

if test "$with_ldap" = yes; then
  AC_MSG_NOTICE(enabling OpenLDAP database backend module support)
  OPENLDAP_PLUGIN=yes
fi
])dnl
dnl
dnl If libkeyutils exists (on Linux) include it and use keyring ccache
AC_DEFUN(KRB5_AC_KEYRING_CCACHE,[
  AC_CHECK_HEADERS([keyutils.h],
    AC_CHECK_LIB(keyutils, add_key, 
      [dnl Pre-reqs were found
       AC_DEFINE(USE_KEYRING_CCACHE, 1, [Define if the keyring ccache should be enabled])
       LIBS="-lkeyutils $LIBS"
      ]))
])dnl
dnl
dnl If libkeyutils supports persistent keyrings, use them
AC_DEFUN(KRB5_AC_PERSISTENT_KEYRING,[
  AC_CHECK_HEADERS([keyutils.h],
    AC_CHECK_LIB(keyutils, keyctl_get_persistent,
      [AC_DEFINE(HAVE_PERSISTENT_KEYRING, 1,
                 [Define if persistent keyrings are supported])
      ]))
])dnl
dnl
