K5_AC_INIT([aclocal.m4])

# If $runstatedir isn't set by autoconf (<2.70), set it manually.
if test x"$runstatedir" = x; then
  runstatedir='${localstatedir}/run'
fi
AC_SUBST(runstatedir)

# Don't make duplicate profile path entries for /etc/krb5.conf if
# $sysconfdir is /etc
if test "$sysconfdir" = /etc; then
  SYSCONFCONF=""
else
  SYSCONFCONF=":${sysconfdir}/krb5.conf"
fi
AC_SUBST(SYSCONFCONF)

CONFIG_RULES
KRB5_VERSION=K5_VERSION
AC_SUBST(KRB5_VERSION)


AC_REQUIRE_CPP

PKG_PROG_PKG_CONFIG

AC_CHECK_HEADER([stdint.h], [],
  [AC_MSG_ERROR([stdint.h is required])])

AC_CACHE_CHECK([whether integers are two's complement],
  [krb5_cv_ints_twos_compl],
  [AC_COMPILE_IFELSE(
    [AC_LANG_BOOL_COMPILE_TRY(
[#include <limits.h>
],
	[/* Basic two's complement check */
	  ~(-1) == 0 && ~(-1L) == 0L &&
	  /* Check that values with sign bit 1 and value bits 0 are valid */
	  -(INT_MIN + 1) == INT_MAX && -(LONG_MIN + 1) == LONG_MAX &&
	  /* Check that unsigned-to-signed conversions preserve bit patterns */
	  (int)((unsigned int)INT_MAX + 1) == INT_MIN &&
	  (long)((unsigned long)LONG_MAX + 1) == LONG_MIN])],
    [krb5_cv_ints_twos_compl=yes],
    [krb5_cv_ints_twos_compl=no])])

if test "$krb5_cv_ints_twos_compl" = "no"; then
  AC_MSG_ERROR([integers are not two's complement])
fi

AC_CACHE_CHECK([whether CHAR_BIT is 8],
  [krb5_cv_char_bit_8],
  [AC_PREPROC_IFELSE([AC_LANG_SOURCE(
[[#include <limits.h>
#if CHAR_BIT != 8
  #error CHAR_BIT != 8
#endif
]])],
    [krb5_cv_char_bit_8=yes], [krb5_cv_char_bit_8=no])])

if test "$krb5_cv_char_bit_8" = "no"; then
  AC_MSG_ERROR([CHAR_BIT is not 8])
fi

AC_CACHE_CHECK(if va_copy is available, krb5_cv_va_copy,
[AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <stdarg.h>
void f(va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);
  va_end(ap2);
}
va_list x;
int main()
{
  f(x);
  return 0;
}])], krb5_cv_va_copy=yes, krb5_cv_va_copy=no)])
if test "$krb5_cv_va_copy" = yes; then
  AC_DEFINE(HAS_VA_COPY,1,[Define if va_copy macro or function is available.])
fi

# Note that this isn't checking if the copied value *works*, just
# whether the C language constraints permit the copying.  If
# va_list is defined as an array type, it can't be assigned.
AC_CACHE_CHECK(if va_list objects can be copied by assignment,
	       krb5_cv_va_simple_copy,
[AC_COMPILE_IFELSE([
AC_LANG_SOURCE([#include <stdarg.h>
void f(va_list va2) {
  va_list va1;
  va1 = va2;
}])], krb5_cv_va_simple_copy=yes, krb5_cv_va_simple_copy=no)])
if test "$krb5_cv_va_simple_copy" = yes; then
  AC_DEFINE(CAN_COPY_VA_LIST,1,[Define if va_list objects can be simply copied by assignment.])
fi

# The following lines are so that configure --help gives some global
# configuration options.

KRB5_LIB_AUX
AC_ARG_ENABLE([athena],
[  --enable-athena         build with MIT Project Athena configuration],,)

# Begin autoconf tests for the Makefiles generated out of the top-level
# configure.in...

KRB5_BUILD_LIBOBJS
KRB5_BUILD_LIBRARY
KRB5_BUILD_PROGRAM
# for kprop
AC_TYPE_MODE_T
AC_PROG_INSTALL
KRB5_AC_NEED_DAEMON
KRB5_GETSOCKNAME_ARGS
KRB5_GETPEERNAME_ARGS
LIBUTIL=
AC_CHECK_LIB(util,main,[AC_DEFINE(HAVE_LIBUTIL,1,[Define if the util library is available])
LIBUTIL=-lutil
])
AC_SUBST(LIBUTIL)

# Determine if NLS is desired and supported.
po=
AC_ARG_ENABLE([nls],
  [AS_HELP_STRING([--disable-nls], [disable native language support])],
  [], [enable_nls=check])
if test "$enable_nls" != no; then
  AC_CHECK_HEADER(libintl.h, [
    AC_SEARCH_LIBS(dgettext, intl, [
      AC_DEFINE(ENABLE_NLS, 1,
                [Define if translation functions should be used.])
      nls_enabled=yes])])

  AC_CHECK_PROG(MSGFMT,msgfmt,msgfmt)
  if test x"$MSGFMT" != x; then
    K5_GEN_MAKEFILE(po)
    po=po
  fi

  # Error out if --enable-nls was explicitly requested but can't be enabled.
  if test "$enable_nls" = yes; then
    if test "$nls_enabled" != yes -o "x$po" = x; then
      AC_MSG_ERROR([NLS support requested but cannot be built])
    fi
  fi
fi
AC_SUBST(po)

# for kdc
AC_CHECK_HEADERS(sys/sockio.h ifaddrs.h unistd.h fnmatch.h)
AC_CHECK_FUNCS(vsprintf vasprintf vsnprintf strlcpy fnmatch secure_getenv)

EXTRA_SUPPORT_SYMS=
AC_CHECK_FUNC(strlcpy,
[STRLCPY_ST_OBJ=
STRLCPY_OBJ=],
[STRLCPY_ST_OBJ=strlcpy.o
STRLCPY_OBJ='$(OUTPRE)strlcpy.$(OBJEXT)'
EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS krb5int_strlcpy krb5int_strlcat"])
AC_SUBST(STRLCPY_OBJ)
AC_SUBST(STRLCPY_ST_OBJ)

AC_CHECK_FUNC(getopt,
[GETOPT_ST_OBJ=
GETOPT_OBJ=
AC_DEFINE(HAVE_GETOPT, 1, [Define if system getopt should be used.])],
[GETOPT_ST_OBJ='getopt.o'
GETOPT_OBJ='$(OUTPRE)getopt.$(OBJEXT)'
EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS k5_optind k5_optarg k5_opterr k5_optopt k5_getopt"])
AC_SUBST(GETOPT_OBJ)
AC_SUBST(GETOPT_ST_OBJ)

AC_CHECK_FUNC(getopt_long,
[GETOPT_LONG_ST_OBJ=
GETOPT_LONG_OBJ=
AC_DEFINE(HAVE_GETOPT_LONG, 1, [Define if system getopt_long should be used.])],
[GETOPT_LONG_ST_OBJ='getopt_long.o'
GETOPT_LONG_OBJ='$(OUTPRE)getopt_long.$(OBJEXT)'
EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS k5_getopt_long"])
AC_SUBST(GETOPT_LONG_OBJ)
AC_SUBST(GETOPT_LONG_ST_OBJ)

AC_CHECK_FUNC(fnmatch,
[FNMATCH_ST_OBJ=
FNMATCH_OBJ=],
[FNMATCH_ST_OBJ=fnmatch.o
FNMATCH_OBJ='$(OUTPRE)fnmatch.$(OBJEXT)'
EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS k5_fnmatch"])
AC_SUBST(FNMATCH_OBJ)
AC_SUBST(FNMATCH_ST_OBJ)

AC_CHECK_FUNC(vasprintf,
[PRINTF_ST_OBJ=
PRINTF_OBJ=],
[PRINTF_ST_OBJ=printf.o
PRINTF_OBJ='$(OUTPRE)printf.$(OBJEXT)'
EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS krb5int_asprintf krb5int_vasprintf"])
AC_SUBST(PRINTF_OBJ)
AC_SUBST(PRINTF_ST_OBJ)
KRB5_NEED_PROTO([#include <stdarg.h>
#include <stdio.h>
],vasprintf)
KRB5_NEED_PROTO([#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
/* Solaris 8 declares swab in stdlib.h.  */
#include <stdlib.h>
],swab,1)

AC_CHECK_FUNC(secure_getenv,
[SECURE_GETENV_ST_OBJ=
SECURE_GETENV_OBJ=
SECURE_GETENV_INIT=],
[SECURE_GETENV_ST_OBJ=secure_getenv.o
SECURE_GETENV_OBJ='$(OUTPRE)secure_getenv.$(OBJEXT)'
SECURE_GETENV_INIT=k5_secure_getenv_init
EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS k5_secure_getenv"])
AC_SUBST(SECURE_GETENV_OBJ)
AC_SUBST(SECURE_GETENV_ST_OBJ)
AC_SUBST(SECURE_GETENV_INIT)

AC_PROG_AWK
KRB5_AC_INET6
KRB5_SOCKADDR_SA_LEN
CHECK_SIGNALS

# --with-vague-errors disables useful error messages.

AC_ARG_WITH([vague-errors],
  [AS_HELP_STRING([--with-vague-errors],
    [Do not @<:@do@:>@ send helpful errors to client])],
  [], [withval=no])
if test "$withval" = yes; then
	AC_MSG_NOTICE(Supplying vague error messages to KDC clients)
	AC_DEFINE(KRBCONF_VAGUE_ERRORS,1,[Define if the KDC should return only vague error codes to clients])
fi

# Check which (if any) audit plugin to build
audit_plugin=""
AC_ARG_ENABLE([audit-plugin],
  [AS_HELP_STRING([--enable-audit-plugin=IMPL],
    [use audit plugin @<:@ do not use audit @:>@])],
  [], enableval=no)
if test "$enableval" != no; then
    case "$enableval" in
    simple)
        # if audit_log_user_message is found, we assume
        # that audit_open and audit_close are also defined.
        AC_CHECK_LIB(audit, audit_log_user_message,
                     [AUDIT_IMPL_LIBS=-laudit
                     K5_GEN_MAKEFILE(plugins/audit/simple)
                     audit_plugin=plugins/audit/simple ],
                     AC_MSG_ERROR([libaudit not found or undefined symbol audit_log_user_message]))
        ;;
    *)
        AC_MSG_ERROR([Unknown audit plugin implementation $enableval.])
        ;;
    esac
fi
AC_SUBST(AUDIT_IMPL_LIBS)
AC_SUBST(audit_plugin)

# WITH_CRYPTO_IMPL

CRYPTO_IMPL=builtin
AC_ARG_WITH([crypto-impl],
  [AS_HELP_STRING([--with-crypto-impl=IMPL],
    [use specified crypto implementation @<:@builtin@:>@])],
  [CRYPTO_IMPL=$withval
   AC_MSG_NOTICE(k5crypto will use '$withval')])

CRYPTO_BUILTIN_TESTS=no
case $CRYPTO_IMPL in
builtin)
  CRYPTO_BUILTIN_TESTS=yes
  ;;
openssl)
  AC_CHECK_LIB(crypto, PKCS7_get_signer_info)
  AC_DEFINE([CRYPTO_OPENSSL], 1, [Define to use OpenSSL crypto library])
  ;;
*)
  AC_MSG_ERROR([Unknown crypto implementation $withval])
  ;;
esac
AC_SUBST([CRYPTO_IMPL_CFLAGS])
AC_SUBST([CRYPTO_IMPL_LIBS])
AC_SUBST([CRYPTO_BUILTIN_TESTS])

# WITH_TLS_IMPL

AC_ARG_WITH([tls-impl],
  [AS_HELP_STRING([--with-tls-impl=IMPL],
    [use specified TLS implementation @<:@auto@:>@])],
  [TLS_IMPL=$withval], [TLS_IMPL=auto])
case "$TLS_IMPL" in
openssl|auto)
  AC_CHECK_LIB(ssl,SSL_CTX_new,[have_lib_ssl=true],[have_lib_ssl=false],
               -lcrypto)
  AC_MSG_CHECKING([for OpenSSL])
  if test x$have_lib_ssl = xtrue ; then
    AC_DEFINE(TLS_IMPL_OPENSSL,1,[Define if TLS implementation is OpenSSL])
    AC_MSG_RESULT([yes])
    TLS_IMPL_LIBS="-lssl -lcrypto"
    TLS_IMPL=openssl
    AC_MSG_NOTICE([TLS module will use OpenSSL])
  else
    if test "$TLS_IMPL" = openssl ; then
      AC_MSG_ERROR([OpenSSL not found!])
    else
      AC_MSG_WARN([OpenSSL not found!])
    fi
    TLS_IMPL=no
    AC_MSG_NOTICE(building without TLS support)
  fi
  ;;
no)
  AC_MSG_NOTICE(building without TLS support)
  ;;
*)
  AC_MSG_ERROR([Unsupported TLS implementation $withval])
  ;;
esac

if test "$TLS_IMPL" = no; then
   AC_DEFINE(TLS_IMPL_NONE,1,[Define if no TLS implementation is selected])
fi

AC_SUBST(TLS_IMPL)
AC_SUBST(TLS_IMPL_CFLAGS)
AC_SUBST(TLS_IMPL_LIBS)

AC_ARG_WITH([keyutils],
  [AS_HELP_STRING([--without-keyutils], [do not link with libkeyutils])],
  [], [with_keyutils=check])
if test "$with_keyutils" != no; then
  have_keyutils=false
  AC_CHECK_HEADERS([keyutils.h],
    AC_CHECK_LIB(keyutils, add_key, [have_keyutils=true]))
  if test "$have_keyutils" = true; then
    AC_DEFINE(USE_KEYRING_CCACHE, 1,
              [Define if the keyring ccache should be enabled])
    LIBS="-lkeyutils $LIBS"
    # If libkeyutils supports persistent keyrings, use them.
    AC_CHECK_LIB(keyutils, keyctl_get_persistent,
      [AC_DEFINE(HAVE_PERSISTENT_KEYRING, 1,
                 [Define if persistent keyrings are supported])
      ])
  elif test "$with_keyutils" = yes; then
    AC_MSG_ERROR([libkeyutils not found])
  fi
fi

# The SPAKE preauth plugin currently supports edwards25519 natively,
# and can support three NIST groups using OpenSSL.
HAVE_SPAKE_OPENSSL=no
AC_ARG_WITH([spake-openssl],
  [AS_HELP_STRING([--with-spake-openssl],
    [use OpenSSL for SPAKE preauth @<:@auto@:>@])],
  [], [withval=auto])
if test "$withval" = auto -o "$withval" = yes; then
  AC_CHECK_LIB([crypto],[EC_POINT_new],[have_crypto=true],[have_crypto=false])
  if test "$have_crypto" = true; then
    AC_DEFINE(SPAKE_OPENSSL,1,[Define to use OpenSSL for SPAKE preauth])
    SPAKE_OPENSSL_LIBS=-lcrypto
    HAVE_SPAKE_OPENSSL=yes
  elif test "$withval" = yes; then
    AC_MSG_ERROR([OpenSSL libcrypto not found])
  fi
fi
AC_SUBST(HAVE_SPAKE_OPENSSL)
AC_SUBST(SPAKE_OPENSSL_LIBS)

AC_ARG_ENABLE([aesni],
  [AS_HELP_STRING([--disable-aesni], [Do not build with AES-NI support])],
  [], [enable_aesni=check])
if test "$CRYPTO_IMPL" = builtin -a "x$enable_aesni" != xno; then
    case "$host" in
    i686-*)
	aesni_obj=iaesx86.o
	aesni_machine=x86
	;;
    x86_64-*)
	aesni_obj=iaesx64.o
	aesni_machine=amd64
	;;
    esac
    case "$host" in
    *-*-linux* | *-*-gnu* | *-*-*bsd* | *-*-solaris*)
	# All Unix-like platforms need -D__linux__ for iaesx64.s to
	# use the System V x86-64 calling convention.
	aesni_flags="-D__linux__ -f elf -m $aesni_machine"
	;;
    esac
    if test "x$aesni_obj" != x && test "x$aesni_flags" != x; then
	AC_CHECK_PROG(YASM,yasm,yasm)
	AC_CHECK_HEADERS(cpuid.h)
	if test x"$YASM" != x -a "x$ac_cv_header_cpuid_h" = xyes; then
	    AESNI_OBJ=$aesni_obj
	    AESNI_FLAGS=$aesni_flags
	    AC_DEFINE(AESNI,1,[Define if AES-NI support is enabled])
	    AC_MSG_NOTICE([Building with AES-NI support])
	fi
    fi
    if test "x$enable_aesni" = xyes -a "x$AESNI_OBJ" = x; then
	AC_MSG_ERROR([AES-NI support requested but cannot be built])
    fi
fi
AC_SUBST(AESNI_OBJ)
AC_SUBST(AESNI_FLAGS)

AC_ARG_ENABLE([kdc-lookaside-cache],
  AS_HELP_STRING([--disable-kdc-lookaside-cache],
    [Disable the cache which detects client retransmits]),
  [], [enableval=yes])
if test "$enableval" = no ; then
	AC_DEFINE(NOCACHE,1,[Define if the KDC should use no lookaside cache])
fi
KRB5_RUN_FLAGS

# asan is a gcc and clang facility to instrument the code with memory
# error checking.  To use it, we compile C and C++ source files with
# -fsanitize=address, and set ASAN=yes to suppress the undefined
# symbols check when building shared libraries.
AC_ARG_ENABLE([asan],
  [AS_HELP_STRING([--enable-asan], [Build with asan memory checking])],
  [], [enable_asan=no])
if test "$enable_asan" != no; then
    if test "$enable_asan" = yes; then
        enable_asan=address
    fi
    ASAN_FLAGS="$DEFS -fsanitize=$enable_asan"
    ASAN=yes
    UNDEF_CHECK=
else
    ASAN_FLAGS=
    ASAN=no
fi
AC_SUBST(ASAN_FLAGS)
AC_SUBST(ASAN)

# from old include/configure.in
AH_TEMPLATE([HAVE_STRUCT_SOCKADDR_STORAGE],
[Define if "struct sockaddr_storage" is available.])

AC_CONFIG_HEADERS(include/autoconf.h, [echo timestamp > include/autoconf.stamp])
AC_C_CONST
AC_HEADER_DIRENT
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS(strdup setvbuf seteuid setresuid setreuid setegid setresgid setregid setsid flock fchmod chmod strptime geteuid setenv unsetenv getenv gmtime_r localtime_r bswap16 bswap64 mkstemp getusershell access getcwd srand48 srand srandom stat strchr strerror timegm explicit_bzero explicit_memset getresuid getresgid)

AC_CHECK_FUNC(mkstemp,
[MKSTEMP_ST_OBJ=
MKSTEMP_OBJ=],
[MKSTEMP_ST_OBJ='mkstemp.o'
MKSTEMP_OBJ='$(OUTPRE)mkstemp.$(OBJEXT)'
EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS krb5int_mkstemp"])
AC_SUBST(MKSTEMP_OBJ)
AC_SUBST(MKSTEMP_ST_OBJ)

AC_CHECK_FUNC(gettimeofday,
	[GETTIMEOFDAY_ST_OBJ=
	GETTIMEOFDAY_OBJ=
	AC_DEFINE(HAVE_GETTIMEOFDAY, 1, [Have the gettimeofday function])
],
	[GETTIMEOFDAY_ST_OBJ='gettimeofday.o'
	GETTIMEOFDAY_OBJ='$(OUTPRE)gettimeofday.$(OBJEXT)'
	EXTRA_SUPPORT_SYMS="$EXTRA_SUPPORT_SYMS krb5int_gettimeofday"])
AC_SUBST(GETTIMEOFDAY_OBJ)
AC_SUBST(GETTIMEOFDAY_ST_OBJ)
AC_SUBST(EXTRA_SUPPORT_SYMS)

DECLARE_SYS_ERRLIST
AC_CHECK_HEADERS(unistd.h paths.h regex.h regexpr.h fcntl.h memory.h ifaddrs.h sys/filio.h byteswap.h machine/endian.h machine/byte_order.h sys/bswap.h endian.h pwd.h arpa/inet.h alloca.h dlfcn.h limits.h)
AC_CHECK_HEADER(regexp.h, [], [],
[#define INIT char *sp = instring;
#define GETC() (*sp++)
#define PEEKC() (*sp)
#define UNGETC(c) (--sp)
#define RETURN(c) return(c)
#define ERROR(c)
])
AC_CHECK_MEMBERS([struct stat.st_mtimensec,struct stat.st_mtimespec.tv_nsec,struct stat.st_mtim.tv_nsec],,,[#include <sys/types.h>
#include <sys/stat.h>])
KRB5_AC_REGEX_FUNCS
AC_TYPE_OFF_T

# Fancy caching of perror result...
AC_MSG_CHECKING(for perror declaration)
AC_CACHE_VAL(krb5_cv_decl_perror,
[AC_EGREP_HEADER(perror, errno.h,
  krb5_cv_decl_perror=yes, krb5_cv_decl_perror=no)])
AC_MSG_RESULT($krb5_cv_decl_perror)
if test $krb5_cv_decl_perror = yes; then
	AC_DEFINE(HDR_HAS_PERROR,1,[Define if errno.h declares perror])
fi

KRB5_NEED_PROTO([#include <time.h>],strptime)
CHECK_WAIT_TYPE
CHECK_SIGPROCMASK
AC_TYPE_GETGROUPS
CHECK_SETJMP

# *rpcent return types needed for lib/rpc

AC_MSG_CHECKING([return type of setrpcent])
AC_CACHE_VAL(k5_cv_type_setrpcent,
[AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE([[#include <netdb.h>
                    extern void setrpcent();]])],
  [k5_cv_type_setrpcent=void],
  [k5_cv_type_setrpcent=int])])
AC_MSG_RESULT($k5_cv_type_setrpcent)
AC_DEFINE_UNQUOTED(SETRPCENT_TYPE, $k5_cv_type_setrpcent, [Define as return type of setrpcent])

AC_MSG_CHECKING([return type of endrpcent])
AC_CACHE_VAL(k5_cv_type_endrpcent,
[AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE(
    [[#include <netdb.h>
      extern void endrpcent();]])],
  [k5_cv_type_endrpcent=void], [k5_cv_type_endrpcent=int])])
AC_MSG_RESULT($k5_cv_type_endrpcent)
AC_DEFINE_UNQUOTED(ENDRPCENT_TYPE, $k5_cv_type_endrpcent, [Define as return type of endrpcent])


# bswap_16 is a macro in byteswap.h under GNU libc
AC_MSG_CHECKING(for bswap_16)
AC_CACHE_VAL(krb5_cv_bswap_16,
[AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
    [[#if HAVE_BYTESWAP_H
      #include <byteswap.h>
      #endif
    ]],
    [[bswap_16(37);]])],
  [krb5_cv_bswap_16=yes], [krb5_cv_bswap_16=no])])
AC_MSG_RESULT($krb5_cv_bswap_16)
if test "$krb5_cv_bswap_16" = yes; then
  AC_DEFINE(HAVE_BSWAP_16,1,[Define to 1 if bswap_16 is available via byteswap.h])
fi
AC_MSG_CHECKING(for bswap_64)
AC_CACHE_VAL(krb5_cv_bswap_64,
[AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
    [[#if HAVE_BYTESWAP_H
      #include <byteswap.h>
      #endif
    ]],
    [[bswap_64(37);]])],
  [krb5_cv_bswap_64=yes], [krb5_cv_bswap_64=no])])
AC_MSG_RESULT($krb5_cv_bswap_64)
if test "$krb5_cv_bswap_64" = yes; then
  AC_DEFINE(HAVE_BSWAP_64,1,[Define to 1 if bswap_64 is available via byteswap.h])
fi

# Needed for ksu and some appl stuff.

case $krb5_cv_host in
alpha*-dec-osf*)
	AC_CHECK_LIB(security,setluid,
		AC_DEFINE(HAVE_SETLUID,1,[Define if setluid provided in OSF/1 security library])
		KSU_LIBS="-lsecurity"
	)
	;;
esac
AC_SUBST(KSU_LIBS)

if test $ac_cv_func_setenv = no || test $ac_cv_func_unsetenv = no \
  || test $ac_cv_func_getenv = no; then
  SETENVOBJ=setenv.o
else
  SETENVOBJ=
fi
AC_SUBST(SETENVOBJ)

# Check what the return types for gethostbyname_r and getservbyname_r are.

AC_CHECK_FUNC(gethostbyname_r,[
ac_cv_func_gethostbyname_r=yes
if test "$ac_cv_func_gethostbyname_r" = yes; then
  AC_MSG_CHECKING([if gethostbyname_r returns an int])
  AC_CACHE_VAL(krb5_cv_gethostbyname_r_returns_int,
  [AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE(
      [[#include <netdb.h>
        extern int gethostbyname_r();]])],
    [krb5_cv_gethostbyname_r_returns_int=yes],
    [krb5_cv_gethostbyname_r_returns_int=no])])
  AC_MSG_RESULT($krb5_cv_gethostbyname_r_returns_int)

  AC_MSG_CHECKING([if gethostbyname_r returns a pointer])
  AC_CACHE_VAL(krb5_cv_gethostbyname_r_returns_ptr,
  [AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE(
      [[#include <netdb.h>
        extern struct hostent *gethostbyname_r();]])],
    [krb5_cv_gethostbyname_r_returns_ptr=yes],
    [krb5_cv_gethostbyname_r_returns_ptr=no])])
  AC_MSG_RESULT($krb5_cv_gethostbyname_r_returns_ptr)

  if test "$krb5_cv_gethostbyname_r_returns_int" = "$krb5_cv_gethostbyname_r_returns_ptr"; then
    AC_MSG_WARN(cannot determine return type of gethostbyname_r -- disabling)
    ac_cv_func_gethostbyname_r=no
  fi
  if test "$krb5_cv_gethostbyname_r_returns_int" = yes; then
    AC_DEFINE(GETHOSTBYNAME_R_RETURNS_INT, 1, [Define if gethostbyname_r returns int rather than struct hostent * ])
  fi
fi
if test "$ac_cv_func_gethostbyname_r" = yes; then
  AC_DEFINE(HAVE_GETHOSTBYNAME_R, 1, [Define if gethostbyname_r exists and its return type is known])
  AC_CHECK_FUNC(gethostbyaddr_r)
fi
])


# PTHREAD_CFLAGS changes which variant of these functions is declared
# on Solaris 11, so use it for these tests.
old_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
AC_CHECK_FUNC(getpwnam_r,ac_cv_func_getpwnam_r=yes,ac_cv_func_getpwnam_r=no)
AC_CHECK_FUNC(getpwuid_r,ac_cv_func_getpwuid_r=yes,ac_cv_func_getpwuid_r=no)
if test "$ac_cv_func_getpwnam_r" = yes; then
  AC_MSG_CHECKING([return type of getpwnam_r])
  AC_CACHE_VAL(krb5_cv_getpwnam_r_return_type,
  [AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE(
      [[#include <pwd.h>
        extern int getpwnam_r();]])],
    [getpwnam_r_returns_int=yes], [getpwnam_r_returns_int=no])
   AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE(
      [[#include <pwd.h>
        extern struct passwd *getpwnam_r();]])],
    [getpwnam_r_returns_ptr=yes], [getpwnam_r_returns_ptr=no])
   case "$getpwnam_r_returns_int/$getpwnam_r_returns_ptr" in
     yes/no) krb5_cv_getpwnam_r_return_type=int ;;
     no/yes) krb5_cv_getpwnam_r_return_type=ptr ;;
     *) krb5_cv_getpwnam_r_return_type=unknown ;;
   esac])
  AC_MSG_RESULT($krb5_cv_getpwnam_r_return_type)
  if test $krb5_cv_getpwnam_r_return_type = int; then
    AC_DEFINE(GETPWNAM_R_RETURNS_INT, 1, [Define if getpwnam_r returns an int])
  elif test $krb5_cv_getpwnam_r_return_type = unknown; then
    AC_MSG_WARN([Cannot determine getpwnam_r return type, disabling getpwnam_r])
    ac_cv_func_getpwnam_r=no
  fi
fi
if test "$ac_cv_func_getpwnam_r" = yes; then
  AC_MSG_CHECKING([number of arguments to getpwnam_r])
  AC_CACHE_VAL(krb5_cv_getpwnam_r_args,
  [AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <pwd.h>
        struct passwd pwx; char buf[1024];]],
      [[getpwnam_r("", &pwx, buf, sizeof(buf));]])],
    [args4=yes], [args4=no])
   AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
       [[#include <pwd.h>
         struct passwd pwx, *p;
         char buf[1024];]],
       [[getpwnam_r("", &pwx, buf, sizeof(buf), &p);]])],
     [args5=yes], [args5=no])
   case $args4/$args5 in
     yes/no) krb5_cv_getpwnam_r_args=4 ;;
     no/yes) krb5_cv_getpwnam_r_args=5 ;;
     *) krb5_cv_getpwnam_r_args=unknown ;;
   esac])
  AC_MSG_RESULT($krb5_cv_getpwnam_r_args)
  if test "$krb5_cv_getpwnam_r_args" = unknown; then
    AC_MSG_WARN([Cannot determine number of arguments to getpwnam_r, disabling its use.])
    ac_cv_func_getpwnam_r=no
  else
    AC_DEFINE(HAVE_GETPWNAM_R,1,[Define if getpwnam_r is available and useful.])
    if test "$krb5_cv_getpwnam_r_args" = 4; then
      AC_DEFINE(GETPWNAM_R_4_ARGS,1,[Define if getpwnam_r exists but takes only 4 arguments (e.g., POSIX draft 6 implementations like some Solaris releases).])
    fi
  fi
fi
CFLAGS=$old_CFLAGS

if test "$ac_cv_func_getpwnam_r" = no && test "$ac_cv_func_getpwuid_r" = yes; then
  # Actually, we could do this check, and the corresponding checks
  # for return type and number of arguments, but I doubt we'll run
  # into a system where we'd get to use getpwuid_r but not getpwnam_r.
  AC_MSG_NOTICE([getpwnam_r not useful, so disabling getpwuid_r too])
  ac_cv_func_getpwuid_r=no
fi
if test "$ac_cv_func_getpwuid_r" = yes; then
  AC_DEFINE(HAVE_GETPWUID_R,1,[Define if getpwuid_r is available and useful.])
  # Hack: Assume getpwuid_r is the shorter form if getpwnam_r is.
  if test "$krb5_cv_getpwnam_r_args" = 4; then
    AC_DEFINE(GETPWUID_R_4_ARGS,1,[Define if getpwuid_r exists but takes only 4 arguments (e.g., POSIX draft 6 implementations like some Solaris releases).])
  fi
fi

if test "$ac_cv_func_gmtime_r" = yes; then
  AC_MSG_CHECKING([whether gmtime_r returns int])
  AC_CACHE_VAL(krb5_cv_gmtime_r_returns_int,
  [AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE(
      [[#include <time.h>
        extern int gmtime_r();]])],
    [return_int=yes], [return_int=no])
   AC_COMPILE_IFELSE([
     AC_LANG_SOURCE(
       [[#include <time.h>
         extern struct tm *gmtime_r();]])],
     [return_ptr=yes], [return_ptr=no])
   case $return_int/$return_ptr in
     yes/no) krb5_cv_gmtime_r_returns_int=yes ;;
     no/yes) krb5_cv_gmtime_r_returns_int=no ;;
     *)      # Can't figure it out, punt the function.
             ac_cv_func_gmtime_r=no ;;
   esac])
  if test "$ac_cv_func_gmtime_r" = no; then
    AC_MSG_RESULT(unknown -- ignoring gmtime_r)
  else
    AC_MSG_RESULT($krb5_cv_gmtime_r_returns_int)
    if test "$krb5_cv_gmtime_r_returns_int" = yes; then
      AC_DEFINE(GMTIME_R_RETURNS_INT,1,[Define if gmtime_r returns int instead of struct tm pointer, as on old HP-UX systems.])
    fi
  fi
fi

AC_CHECK_FUNC(getservbyname_r,[
ac_cv_func_getservbyname_r=yes
if test "$ac_cv_func_getservbyname_r" = yes; then
  AC_MSG_CHECKING([if getservbyname_r returns an int])
  AC_CACHE_VAL(krb5_cv_getservbyname_r_returns_int,
  [AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE(
      [[#include <netdb.h>
        extern int getservbyname_r();]])],
    [krb5_cv_getservbyname_r_returns_int=yes],
    [krb5_cv_getservbyname_r_returns_int=no])])
  AC_MSG_RESULT($krb5_cv_getservbyname_r_returns_int)

  AC_MSG_CHECKING([if getservbyname_r returns a pointer])
  AC_CACHE_VAL(krb5_cv_getservbyname_r_returns_ptr,
  [AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE(
      [[#include <netdb.h>
        extern struct servent *getservbyname_r();]])],
    [krb5_cv_getservbyname_r_returns_ptr=yes],
    [krb5_cv_getservbyname_r_returns_ptr=no])])
  AC_MSG_RESULT($krb5_cv_getservbyname_r_returns_ptr)

  if test "$krb5_cv_getservbyname_r_returns_int" = "$krb5_cv_getservbyname_r_returns_ptr"; then
    AC_MSG_WARN(cannot determine return type of getservbyname_r -- disabling)
    ac_cv_func_getservbyname_r=no
  fi
  if test "$krb5_cv_getservbyname_r_returns_int" = yes; then
    AC_DEFINE(GETSERVBYNAME_R_RETURNS_INT, 1, [Define if getservbyname_r returns int rather than struct servent * ])
  fi
fi
if test "$ac_cv_func_getservbyname_r" = yes; then
  AC_DEFINE(HAVE_GETSERVBYNAME_R, 1, [Define if getservbyname_r exists and its return type is known])
  AC_CHECK_FUNC(getservbyport_r)
fi
])

CHECK_DIRENT
AC_TYPE_UID_T

AC_CHECK_HEADER(termios.h,
[AC_CHECK_FUNC([tcsetattr],
  AC_DEFINE(POSIX_TERMIOS,1,[Define if termios.h exists and tcsetattr exists]))])

AC_CHECK_HEADERS(poll.h stdlib.h string.h stddef.h sys/types.h sys/file.h sys/param.h sys/stat.h sys/time.h netinet/in.h sys/uio.h sys/filio.h sys/select.h time.h paths.h errno.h)

# If compiling with IPv6 support, test if in6addr_any functions.
# Irix 6.5.16 defines it, but lacks support in the C library.
if test $krb5_cv_inet6 = yes || test "$krb5_cv_inet6_with_dinet6" = yes ; then
  AC_CACHE_CHECK([for in6addr_any definition in library],
  [krb5_cv_var_in6addr_any],
  [AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
      [[#ifdef HAVE_SYS_TYPES_H
        #include <sys/types.h>
        #endif
        #include <sys/socket.h>
        #include <netinet/in.h>
        #include <netdb.h>
        #include <stdio.h>
      ]],
      [[struct sockaddr_in6 in;
        in.sin6_addr = in6addr_any;
        printf("%x", &in);]])],
    [krb5_cv_var_in6addr_any=yes], [krb5_cv_var_in6addr_any=no])])
  if test $krb5_cv_var_in6addr_any = no; then
    AC_DEFINE(NEED_INSIXADDR_ANY,1,[Define if in6addr_any is not defined in libc])
  fi
fi

# then from osconf.h, we have

AC_CHECK_TYPE(time_t, long)
AC_CHECK_SIZEOF(time_t)
SIZEOF_TIME_T=$ac_cv_sizeof_time_t
AC_SUBST(SIZEOF_TIME_T)

# Determine where to put the replay cache.

AC_MSG_CHECKING([for replay cache directory])
AC_CACHE_VAL(krb5_cv_sys_rcdir,
[
if test $cross_compiling = yes; then
	krb5_cv_sys_rcdir=/var/tmp
else
	for t_dir in /var/tmp /usr/tmp /var/usr/tmp /tmp ; do
		test -d $t_dir || continue
		krb5_cv_sys_rcdir=$t_dir
		break
	done
fi])
AC_MSG_RESULT($krb5_cv_sys_rcdir)
KRB5_RCTMPDIR=$krb5_cv_sys_rcdir
AC_SUBST(KRB5_RCTMPDIR)


AC_MSG_CHECKING(for socklen_t)
AC_CACHE_VAL(krb5_cv_has_type_socklen_t,
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <sys/types.h>
      #include <sys/socket.h>
    ]],
    [[sizeof(socklen_t);]])],
  [krb5_cv_has_type_socklen_t=yes], [krb5_cv_has_type_socklen_t=no])])
AC_MSG_RESULT($krb5_cv_has_type_socklen_t)
if test $krb5_cv_has_type_socklen_t = yes; then
    AC_DEFINE(HAVE_SOCKLEN_T,1,[Define if there is a socklen_t type. If not, probably use size_t])
fi

AC_MSG_CHECKING(for struct lifconf)
AC_CACHE_VAL(krb5_cv_has_struct_lifconf,
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <sys/socket.h>
      #include <net/if.h>
    ]],
    [[sizeof (struct lifconf);]])],
  [krb5_cv_has_struct_lifconf=yes], [krb5_cv_has_struct_lifconf=no])])
AC_MSG_RESULT($krb5_cv_has_struct_lifconf)
if test $krb5_cv_has_struct_lifconf = yes; then
    AC_DEFINE(HAVE_STRUCT_LIFCONF,1,[Define if there is a struct lifconf.])
fi
# HP-UX 11 uses stuct if_laddrconf
AC_MSG_CHECKING(for struct if_laddrconf)
AC_CACHE_VAL(krb5_cv_has_struct_if_laddrconf,
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <sys/socket.h>
      #include <net/if.h>
      #include <net/if6.h>
    ]],
    [[sizeof(struct if_laddrconf);]])],
  [krb5_cv_has_struct_if_laddrconf=yes],
  [krb5_cv_has_struct_if_laddrconf=no])])
AC_MSG_RESULT($krb5_cv_has_struct_if_laddrconf)
if test $krb5_cv_has_struct_if_laddrconf = yes; then
    AC_DEFINE(HAVE_STRUCT_IF_LADDRCONF,1,[Define if there is a struct if_laddrconf.])
fi


AC_MSG_CHECKING([for h_errno in netdb.h])
AC_CACHE_VAL(krb5_cv_header_netdb_h_h_errno,
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <netdb.h>]],
    [[int x = h_errno;]])],
    [krb5_cv_header_netdb_h_h_errno=yes],
    [krb5_cv_header_netdb_h_h_errno=no])])
AC_MSG_RESULT($krb5_cv_header_netdb_h_h_errno)
if test $krb5_cv_header_netdb_h_h_errno = yes; then
    AC_DEFINE([HAVE_NETDB_H_H_ERRNO], 1,
	[Define if netdb.h declares h_errno])
fi


AC_ARG_ENABLE([athena],
[  --enable-athena         build with MIT Project Athena configuration],
AC_DEFINE(KRB5_ATHENA_COMPAT,1,[Define if MIT Project Athena default configuration should be used]),)


AC_C_INLINE
AH_TOP([
#ifndef KRB5_AUTOCONF_H
#define KRB5_AUTOCONF_H
])
AH_BOTTOM([
#if defined(__GNUC__) && !defined(inline)
/* Silence gcc pedantic warnings about ANSI C.  */
# define inline __inline__
#endif
#endif /* KRB5_AUTOCONF_H */
])

AC_CHECK_TYPES([struct cmsghdr, struct in_pktinfo, struct in6_pktinfo, struct sockaddr_storage], , , [
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
])
AC_CHECK_TYPES([struct rt_msghdr], , , [
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
])

# Tests for 64-bit edwards25519 code.
AC_CHECK_SIZEOF([size_t])
AC_CHECK_TYPES([__int128_t, __uint128_t])

# types libdb2 wants

AC_CHECK_TYPES([ssize_t, u_char, u_int, u_long, u_int8_t, u_int16_t, u_int32_t, int8_t, int16_t, int32_t])

# Some libdb2 test programs want a shell that supports functions.
FCTSH=false
AC_PATH_PROG(SH,sh,false)
AC_PATH_PROG(SH5,sh5,false)
AC_PATH_PROG(BASH,bash,false)
for prog in $SH $SH5 $BASH; do
  AC_MSG_CHECKING(if $prog supports functions)
  if $prog -c 'foo() { true; }; foo' >/dev/null 2>&1; then
    AC_MSG_RESULT(yes)
    FCTSH=$prog
    break
  else
    AC_MSG_RESULT(no)
  fi
done
AC_SUBST(FCTSH)

# Test for POSIX 2001 *printf support (X/Open System Interfaces extension
# to ANSI/ISO C 1999 specification).  Specifically, positional
# specifications; not checking for other features like %zx at present.
AC_MSG_CHECKING(for POSIX printf positional specification support)
AC_CACHE_VAL(ac_cv_printf_positional,
[AC_RUN_IFELSE(
  [AC_LANG_SOURCE(
    [[#include <stdio.h>
      #include <string.h>
      const char expected[] = "200 100";
      int main()
      {
          char buf[30];
          sprintf(buf, "%2\$x %1\$d", 100, 512);
          if (strcmp(expected, buf)) {
              fprintf(stderr, "bad result: <%s> wanted: <%s>\n",
                      buf, expected);
              return 1;
          }
          return 0;
      }]])],
  [ac_cv_printf_positional=yes], [ac_cv_printf_positional=no],
  [AC_MSG_ERROR(Cannot test for printf positional argument support when cross compiling)])])
# Nothing for autoconf.h for now.
AC_MSG_RESULT($ac_cv_printf_positional)


# for t_locate_kdc test

AC_PATH_PROG(DIG, dig, false)
AC_PATH_PROG(NSLOOKUP, nslookup, false)

# for kadmin

AC_PROG_YACC
ath_compat=
AC_ARG_ENABLE([athena],
[  --enable-athena         build with MIT Project Athena configuration],
ath_compat=compat,)

KRB5_AC_PRIOCNTL_HACK

AC_CHECK_PROG(PERL,perl,perl)

# lib/gssapi
AC_CHECK_HEADER(xom.h,[
	include_xom='awk '\''END{printf("%cinclude <xom.h>\n", 35);}'\'' < /dev/null'], [
	include_xom='echo "/* no xom.h */"'])
AC_SUBST(include_xom)


# lib/rpc
### Check where struct rpcent is declared.

# This is necessary to determine:
# 1. If /usr/include/netdb.h declares struct rpcent
# 2. If /usr/include/rpc/netdb.h declares struct rpcent

# We have our own rpc/netdb.h, and if /usr/include/netdb.h includes
# rpc/netdb.h, then nastiness could happen.

# Logic: If /usr/include/netdb.h declares struct rpcent, then check
# rpc/netdb.h.  If /usr/include/rpc/netdb.h declares struct rpcent,
# then define STRUCT_RPCENT_IN_RPC_NETDB_H, otherwise do not.  If
# neither netdb.h nor rpc/netdb.h declares struct rpcent, then define
# STRUCT_RPCENT_IN_RPC_NETDB_H anyway.

AC_MSG_CHECKING([where struct rpcent is declared])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <netdb.h>
    ]],
    [[struct rpcent e;
      char c = e.r_name[0];
      int i = e.r_number;]])],
    [netdb_rpcent=yes], [netdb_rpcent=no])
if test "$netdb_rpcent" = yes; then
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
      [[#include <rpc/netdb.h>
      ]],
      [[struct rpcent e;
        char c = e.r_name[0];
        int i = e.r_number;]])],
    [rpc_netdb_rpcent=yes], [rpc_netdb_rpcent=no])
  if test "$rpc_netdb_rpcent" = yes; then
    AC_MSG_RESULT([rpc/netdb.h])
    rpcent_define='#define STRUCT_RPCENT_IN_RPC_NETDB_H'
  else
    AC_MSG_RESULT([netdb.h])
  fi
else
  AC_MSG_RESULT([nowhere])
  rpcent_define='#define STRUCT_RPCENT_IN_RPC_NETDB_H'
fi
AC_SUBST(rpcent_define)

AC_CHECK_HEADERS(sys/select.h sys/time.h unistd.h)
if test $ac_cv_header_sys_select_h = yes; then
  GSSRPC__SYS_SELECT_H='#include <sys/select.h>'
else
  GSSRPC__SYS_SELECT_H='/* #include <sys/select.h> */'
fi
AC_SUBST(GSSRPC__SYS_SELECT_H)
if test $ac_cv_header_sys_time_h = yes; then
  GSSRPC__SYS_TIME_H='#include <sys/time.h>'
else
  GSSRPC__SYS_TIME_H='/* #include <sys/time.h> */'
fi
AC_SUBST(GSSRPC__SYS_TIME_H)
if test $ac_cv_header_unistd_h = yes; then
  GSSRPC__UNISTD_H='#include <unistd.h>'
else
  GSSRPC__UNISTD_H='/* #include <unistd.h> */'
fi
AC_SUBST(GSSRPC__UNISTD_H)

AC_CACHE_CHECK([for MAXHOSTNAMELEN in sys/param.h],
[krb5_cv_header_sys_param_h_maxhostnamelen],
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <sys/param.h>
    ]],
    [[int i = MAXHOSTNAMELEN;]])],
    [krb5_cv_header_sys_param_h_maxhostnamelen=yes],
    [krb5_cv_header_sys_param_h_maxhostnamelen=no])])
AC_CACHE_CHECK([for MAXHOSTNAMELEN in netdb.h],
[krb5_cv_header_netdb_h_maxhostnamelen],
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <netdb.h>
    ]],
    [[int i = MAXHOSTNAMELEN;]])],
    [krb5_cv_header_netdb_h_maxhostnamelen=yes],
    [krb5_cv_header_netdb_h_maxhostnamelen=no])])

GSSRPC__SYS_PARAM_H='/* #include <sys/param.h> */'
GSSRPC__NETDB_H='/* #include <netdb.h> */'
if test $krb5_cv_header_sys_param_h_maxhostnamelen = yes; then
  GSSRPC__SYS_PARAM_H='#include <sys/param.h>'
else
  if test $krb5_cv_header_netdb_h_maxhostnamelen = yes; then
    GSSRPC__NETDB_H='#include <netdb.h>'
  else
    AC_MSG_WARN([can't find MAXHOSTNAMELEN definition; faking it])
  fi
fi
AC_SUBST(GSSRPC__SYS_PARAM_H)
AC_SUBST(GSSRPC__NETDB_H)

AC_CACHE_CHECK([for BSD type aliases], [krb5_cv_type_bsdaliases],
[AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#include <sys/types.h>
      #if HAVE_UNISTD_H
      #include <unistd.h>
      #endif
    ]],
    [[u_char c;
      u_int i;
      u_long l;]])],
  [krb5_cv_type_bsdaliases=yes], [krb5_cv_type_bsdaliases=no])])
if test $krb5_cv_type_bsdaliases = yes; then
  GSSRPC__BSD_TYPEALIASES='/* #undef GSSRPC__BSD_TYPEALIASES */'
else
  GSSRPC__BSD_TYPEALIASES='#define GSSRPC__BSD_TYPEALIASES 1'
fi
AC_SUBST(GSSRPC__BSD_TYPEALIASES)

AC_MSG_CHECKING([return type of setrpcent])
AC_CACHE_VAL(k5_cv_type_setrpcent,
[AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE(
    [[#include <netdb.h>
      extern void setrpcent();]])],
  [k5_cv_type_setrpcent=void], [k5_cv_type_setrpcent=int])])
AC_MSG_RESULT($k5_cv_type_setrpcent)
AC_DEFINE_UNQUOTED(SETRPCENT_TYPE, $k5_cv_type_setrpcent, [Define as return type of setrpcent])

AC_MSG_CHECKING([return type of endrpcent])
AC_CACHE_VAL(k5_cv_type_endrpcent,
[AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE(
    [[#include <netdb.h>
      extern void endrpcent();]])],
  [k5_cv_type_endrpcent=void], [k5_cv_type_endrpcent=int])])
AC_MSG_RESULT($k5_cv_type_endrpcent)
AC_DEFINE_UNQUOTED(ENDRPCENT_TYPE, $k5_cv_type_endrpcent, [Define as return type of endrpcent])
K5_GEN_FILE(include/gssrpc/types.h:include/gssrpc/types.hin)

# for pkinit
AC_ARG_ENABLE([pkinit],
[  --disable-pkinit        disable PKINIT plugin support],,
enable_pkinit=try)
if test "$enable_pkinit" = yes || test "$enable_pkinit" = try; then
  AC_CACHE_CHECK(for a recent enough OpenSSL, k5_cv_openssl_version_okay,
[AC_COMPILE_IFELSE([AC_LANG_SOURCE([#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < 0x10000000L
# error openssl is too old, need 1.0.0
#endif
int i = 1;
])], k5_cv_openssl_version_okay=yes, k5_cv_openssl_version_okay=no)])
  old_LIBS="$LIBS"
  AC_CHECK_LIB(crypto, PKCS7_get_signer_info)
  AC_CHECK_FUNCS(EVP_PKEY_get_bn_param)
  LIBS="$old_LIBS"
fi
if test "$k5_cv_openssl_version_okay" = yes && (test "$enable_pkinit" = yes || test "$enable_pkinit" = try); then
  K5_GEN_MAKEFILE(plugins/preauth/pkinit)
  K5_GEN_MAKEFILE(tests/softpkcs11)
  PKINIT=yes
elif test "$k5_cv_openssl_version_okay" = no && test "$enable_pkinit" = yes; then
  AC_MSG_ERROR([Version of OpenSSL is too old; cannot enable PKINIT.])
else
  AC_DEFINE([DISABLE_PKINIT], 1, [Define to disable PKINIT plugin support])
  AC_MSG_NOTICE([Disabling PKINIT support.])
  PKINIT=no
fi
AC_SUBST(PKINIT)

# for lib/apputils
AC_REPLACE_FUNCS(daemon)

# For Python tests.  Python version 3.2.4 is required as prior
# versions do not accept string input to subprocess.Popen.communicate
# when universal_newlines is set.
PYTHON_MINVERSION=3.2.4
AC_SUBST(PYTHON_MINVERSION)
AC_CHECK_PROG(PYTHON,python3,python3)
if test x"$PYTHON" = x; then
	AC_CHECK_PROG(PYTHON,python,python)
fi
HAVE_PYTHON=no
if test x"$PYTHON" != x; then
	wantver="(sys.hexversion >= 0x30204F0)"
	if "$PYTHON" -c "import sys; sys.exit(not $wantver and 1 or 0)"; then
		HAVE_PYTHON=yes
	fi
fi
AC_SUBST(HAVE_PYTHON)

# For cmocka tests.
CMOCKA_LIBS=
HAVE_CMOCKA=no
HAVE_CMOCKA_H=no
HAVE_CMOCKA_LIB=no
AC_CHECK_HEADER(cmocka.h, [HAVE_CMOCKA_H=yes], :, [
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h> ])
AC_CHECK_LIB(cmocka, _cmocka_run_group_tests, [HAVE_CMOCKA_LIB=yes])
if test "$HAVE_CMOCKA_LIB" = yes && test "$HAVE_CMOCKA_H" = yes; then
    HAVE_CMOCKA=yes
    CMOCKA_LIBS='-lcmocka'
    AC_DEFINE([HAVE_CMOCKA],1,[Define if cmocka library is available.])
fi
AC_SUBST(HAVE_CMOCKA)
AC_SUBST(CMOCKA_LIBS)

# For URI lookup tests. Requires resolv_wrapper >= 1.1.5 for URI
# support.
HAVE_RESOLV_WRAPPER=0
PKG_CHECK_EXISTS([resolv_wrapper >= 1.1.5], [HAVE_RESOLV_WRAPPER=1])
AC_SUBST(HAVE_RESOLV_WRAPPER)

# for plugins/kdb/db2

# AIX is unusual in that it wants all symbols resolved at link time
#  Fortunately, it will allow us to link the kdb library now, even if
# it is linked again later.
case $krb5_cv_host in
*-*-aix*)
	DB_EXTRA_LIBS=-ldb
	;;
*)
	DB_EXTRA_LIBS=
	;;
esac
AC_SUBST(DB_EXTRA_LIBS)



# Warn about possible thread safety issues.  These functions have all
# been checked for previously.
tsfuncs="getpwnam_r getpwuid_r gethostbyname_r getservbyname_r gmtime_r localtime_r"
if test "$enable_thread_support" = yes; then
  tsmissing=""
  for ts in $tsfuncs; do
    if eval "test \"\${ac_cv_func_$ts}\" != yes"; then
      tsmissing="$tsmissing $ts"
    fi
  done
  if test "$ac_cv_func_res_nsearch/$ac_cv_lib_resolv_res_nsearch" = "no/no"; then
    tsmissing="$tsmissing res_nsearch"
  fi
  if test "$tsmissing" != ""; then
    AC_MSG_WARN([Some functions that are needed for library thread])
    AC_MSG_WARN([safety appear to be missing.])
    for ts in $tsmissing; do
      AC_MSG_WARN([  missing thread-safe function: $ts])
    done
    AC_MSG_WARN([Without these functions, the installed libraries])
    AC_MSG_WARN([may not be thread-safe.])
  fi # tsmissing not empty
fi # enable_thread_support

# Sadly, we seem to have accidentally committed ourselves in 1.4 to
# an ABI that includes the existence of libkrb5support.0 even
# though random apps should never use anything from it.  And on
# the Mac, to which that didn't apply, we can't use major version 0.

case $krb5_cv_host in
*-*-darwin* | *-*-rhapsody*) SUPPORTLIB_MAJOR=1 ;;
*)			     SUPPORTLIB_MAJOR=0 ;;
esac
AC_SUBST(SUPPORTLIB_MAJOR)


if test "$COM_ERR_VERSION" = k5 ; then
  K5_GEN_MAKEFILE(util/et)
fi
if test "$SS_VERSION" = k5 ; then
  K5_GEN_MAKEFILE(util/ss)
fi


ldap_plugin_dir=""
ldap_lib=""
if test -n "$OPENLDAP_PLUGIN"; then
  AC_CHECK_HEADERS(ldap.h lber.h, :, [AC_MSG_ERROR($ac_header not found)])
  AC_CHECK_LIB(ldap, ldap_str2dn, :, [AC_MSG_ERROR(libldap not found or missing ldap_str2dn)])

  BER_OKAY=0
  AC_CHECK_LIB(ldap, ber_init, [BER_OKAY=1])
  if test "$BER_OKAY" = "1"; then
    LDAP_LIBS='-lldap'
  else
    AC_CHECK_LIB(lber, ber_init, [BER_OKAY=1], [AC_MSG_WARN([libber not found])])
    if test "$BER_OKAY" = "1"; then
      LDAP_LIBS='-lldap -llber'
    else
      AC_MSG_ERROR("BER library missing - cannot build LDAP database module")
    fi
  fi
  AC_DEFINE([ENABLE_LDAP], 1, [Define if LDAP KDB support within the Kerberos library (mainly ASN.1 code) should be enabled.])
  AC_SUBST(LDAP_LIBS)

  AC_CHECK_HEADERS([sasl/sasl.h], [HAVE_SASL=yes], [HAVE_SASL=no])
  AC_SUBST(HAVE_SASL)
  if test "$HAVE_SASL" = no; then
    AC_MSG_WARN([not building LDAP SASL support])
  fi

  K5_GEN_MAKEFILE(plugins/kdb/ldap)
  K5_GEN_MAKEFILE(plugins/kdb/ldap/ldap_util)
  K5_GEN_MAKEFILE(plugins/kdb/ldap/libkdb_ldap)
  ldap_plugin_dir='plugins/kdb/ldap plugins/kdb/ldap/ldap_util'
  LDAP=yes
else
  LDAP=no
fi
AC_SUBST(ldap_plugin_dir)
AC_SUBST(LDAP)
# This check is for plugins/preauth/securid_sam2
sam2_plugin=""
old_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
AC_CHECK_LIB(aceclnt, SD_Init, [
	     AC_MSG_NOTICE([Enabling RSA securID support])
	     K5_GEN_MAKEFILE(plugins/preauth/securid_sam2)
	     sam2_plugin=plugins/preauth/securid_sam2
	     ])
AC_SUBST(sam2_plugin)
CFLAGS=$old_CFLAGS

lmdb_plugin_dir=""
HAVE_LMDB=no
AC_ARG_WITH([lmdb],
  [AS_HELP_STRING([--with-lmdb],
    [compile LMDB database backend module @<:@auto@:>@])],
  [], [withval=auto])
if test "$withval" = auto -o "$withval" = yes; then
  AC_CHECK_LIB([lmdb],[mdb_env_create],[have_lmdb=true],[have_lmdb=false])
  if test "$have_lmdb" = true; then
    LMDB_LIBS=-llmdb
    HAVE_LMDB=yes
    lmdb_plugin_dir='plugins/kdb/lmdb'
    K5_GEN_MAKEFILE(plugins/kdb/lmdb)
  elif test "$withval" = yes; then
    AC_MSG_ERROR([liblmdb not found])
  fi
fi
AC_SUBST(HAVE_LMDB)
AC_SUBST(LMDB_LIBS)
AC_SUBST(lmdb_plugin_dir)

# Kludge for simple server --- FIXME is this the best way to do this?

if test "$ac_cv_lib_socket" = "yes" -a "$ac_cv_lib_nsl" = "yes"; then
	AC_DEFINE(BROKEN_STREAMS_SOCKETS,1,[Define if socket can't be bound to 0.0.0.0])
fi

# Compile with libedit support in ss by default if available.  Compile
# with readline only if asked, to avoid a default GPL dependency.
AC_ARG_WITH([libedit],
  [AS_HELP_STRING([--without-libedit], [do not compile with libedit])],
  [], [with_libedit=default])
AC_ARG_WITH([readline],
  [AS_HELP_STRING([--with-readline], [compile with GNU Readline])],
  [], [with_readline=no])
if test "x$with_readline" = xyes; then
  with_libedit=no
fi
RL_CFLAGS=
RL_LIBS=
if test "x$with_libedit" != xno; then
  PKG_CHECK_MODULES(LIBEDIT, libedit, [have_libedit=yes], [have_libedit=no])
  if test "x$have_libedit" = xyes; then
    RL_CFLAGS=$LIBEDIT_CFLAGS
    RL_LIBS=$LIBEDIT_LIBS
    AC_DEFINE([HAVE_LIBEDIT], 1, [Define if building with libedit.])
    AC_MSG_NOTICE([Using libedit for readline support])
  elif test "x$with_libedit" = xyes; then
    # We were explicitly asked for libedit and couldn't find it.
    AC_MSG_ERROR([Could not detect libedit with pkg-config])
  else
    AC_MSG_NOTICE([Not using any readline support])
  fi
elif test "x$with_readline" = xyes; then
  AC_MSG_NOTICE([Using GNU Readline])
  AC_CHECK_LIB([readline], [main], :,
	       AC_MSG_FAILURE([Cannot find readline library.]))
  AC_DEFINE([HAVE_READLINE], 1, [Define if building with GNU Readline.])
  RL_LIBS='-lreadline'
else
  AC_MSG_RESULT([Not using any readline support])
fi
AC_SUBST([RL_CFLAGS])
AC_SUBST([RL_LIBS])

AC_ARG_WITH([system-verto],
  [AS_HELP_STRING([--with-system-verto], [always use system verto library])],
  [], [with_system_verto=default])
VERTO_VERSION=k5
if test "x$with_system_verto" != xno; then
  PKG_CHECK_MODULES(VERTO, libverto, [have_sysverto=yes], [have_sysverto=no])
  if test "x$have_sysverto" = xyes; then
    VERTO_VERSION=sys
  elif test "x$with_system_verto" = xyes; then
    AC_MSG_ERROR([cannot detect system libverto])
  fi
fi
if test "x$VERTO_VERSION" = xsys; then
  AC_MSG_NOTICE([Using system libverto])
else
  VERTO_CFLAGS=
  VERTO_LIBS="-lverto"
  AC_MSG_NOTICE([Using built-in libverto])
fi
AC_SUBST([VERTO_CFLAGS])
AC_SUBST([VERTO_LIBS])
AC_SUBST([VERTO_VERSION])

AC_PATH_PROG(GROFF, groff)

# Make localedir work in autoconf 2.5x.
if test "${localedir+set}" != set; then
    localedir='$(datadir)/locale'
fi
AC_SUBST(localedir)

# Determine the default macOS ccache type and whether to build the KCM
# Mach RPC support.
MACOS_FRAMEWORK=
dnl The outer brackets around the case statement prevent m4 from
dnl eating the brackets in the glob patterns, but also prevent us from
dnl using AC_DEFINE within the body.
[case $host in
*-*-darwin[0-9].* | *-*-darwin10.*)
  # Use the normal default cache type for macOS 10.6 (Darwin 10) and
  # prior.  Build the KCM Mach RPC support.
  OSX=osx
  ;;
*-*-darwin*)
  # macOS 10.6 (Darwin 11) uses the KCM type by default.  macOS 11
  # (Darwin 20) uses an xpc-based cache type called XCACHE by default.
  # We can access either of these collections via a macos-specific
  # implementation of the API cache type.  Build the KCM Mach RPC
  # support.
  OSX=osx
  macos_defccname=API:
  MACOS_FRAMEWORK="-framework Kerberos"
  ;;
*)
  # This is not macOS; do not build the Mach RPC support and use the
  # normal default cache type.
  OSX=no
  ;;
esac]
if test "$macos_defccname" = API:; then
  AC_DEFINE(USE_CCAPI_MACOS, 1, [Define to build macOS CCAPI client])
fi
AC_SUBST(OSX)
AC_SUBST(MACOS_FRAMEWORK)

# Build-time default ccache, keytab, and client keytab names.  These
# can be given as variable arguments DEFCCNAME, DEFKTNAME, and
# DEFCKTNAME.  Otherwise, we try to get the OS defaults from
# krb5-config if we can, or fall back to hardcoded defaults.
AC_ARG_VAR(DEFCCNAME, [Default ccache name])
AC_ARG_VAR(DEFKTNAME, [Default keytab name])
AC_ARG_VAR(DEFCKTNAME, [Default client keytab name])
AC_ARG_WITH([krb5-config],
  [AS_HELP_STRING([--with-krb5-config=PATH],
    [path to existing krb5-config program for defaults])],
  [], [with_krb5_config=krb5-config])
if test "x$with_krb5_config" != xno; then
	if test "x$with_krb5_config" = xyes; then
		with_krb5_config=krb5-config
	fi
	if $with_krb5_config --help 2>&1 | grep defccname >/dev/null; then
		AC_MSG_NOTICE([Using $with_krb5_config for build defaults])
		: "${DEFCCNAME=`$with_krb5_config --defccname`}"
		: "${DEFKTNAME=`$with_krb5_config --defktname`}"
		: "${DEFCKTNAME=`$with_krb5_config --defcktname`}"
	fi
fi
if test "${DEFCCNAME+set}" != set; then
	if test "${macos_defccname+set}" = set; then
		DEFCCNAME=$macos_defccname
	else
		DEFCCNAME=FILE:/tmp/krb5cc_%{uid}
	fi
fi
if test "${DEFKTNAME+set}" != set; then
	DEFKTNAME=FILE:/etc/krb5.keytab
fi
if test "${DEFCKTNAME+set}" != set; then
	AX_RECURSIVE_EVAL($localstatedir, exp_localstatedir)
	DEFCKTNAME=FILE:$exp_localstatedir/krb5/user/%{euid}/client.keytab
fi
AC_MSG_NOTICE([Default ccache name: $DEFCCNAME])
AC_MSG_NOTICE([Default keytab name: $DEFKTNAME])
AC_MSG_NOTICE([Default client keytab name: $DEFCKTNAME])
AC_DEFINE_UNQUOTED(DEFCCNAME, ["$DEFCCNAME"], [Define to default ccache name])
AC_DEFINE_UNQUOTED(DEFKTNAME, ["$DEFKTNAME"], [Define to default keytab name])
AC_DEFINE_UNQUOTED(DEFCKTNAME, ["$DEFCKTNAME"],
                   [Define to default client keytab name])

AC_ARG_VAR(PKCS11_MODNAME, [Default PKCS11 module name])
if test "${PKCS11_MODNAME+set}" != set; then
	PKCS11_MODNAME=opensc-pkcs11.so
fi
AC_MSG_NOTICE([Default PKCS11 module name: $PKCS11_MODNAME])
AC_DEFINE_UNQUOTED(PKCS11_MODNAME, ["$PKCS11_MODNAME"],
                   [Default PKCS11 module name])

AC_CONFIG_FILES([build-tools/krb5-config], [chmod +x build-tools/krb5-config])
AC_CONFIG_FILES([build-tools/kadm-server.pc
	build-tools/kadm-client.pc
	build-tools/kdb.pc
	build-tools/krb5.pc
	build-tools/krb5-gssapi.pc
	build-tools/mit-krb5.pc
	build-tools/mit-krb5-gssapi.pc
	build-tools/gssrpc.pc
])
V5_AC_OUTPUT_MAKEFILE(.

	util util/support util/profile util/profile/testmod
	util/verto

	lib lib/kdb

	lib/crypto lib/crypto/krb lib/crypto/crypto_tests
	lib/crypto/builtin lib/crypto/builtin/des
	lib/crypto/builtin/aes lib/crypto/builtin/camellia
	lib/crypto/builtin/md4 lib/crypto/builtin/md5
	lib/crypto/builtin/sha1 lib/crypto/builtin/sha2
	lib/crypto/builtin/enc_provider lib/crypto/builtin/hash_provider
	lib/crypto/openssl lib/crypto/openssl/des
	lib/crypto/openssl/enc_provider lib/crypto/openssl/hash_provider

	lib/krb5 lib/krb5/error_tables lib/krb5/asn.1 lib/krb5/ccache
	lib/krb5/keytab lib/krb5/krb lib/krb5/rcache lib/krb5/os
	lib/krb5/unicode

	lib/gssapi lib/gssapi/generic lib/gssapi/krb5 lib/gssapi/spnego
	lib/gssapi/mechglue

	lib/rpc lib/rpc/unit-test

	lib/kadm5 lib/kadm5/clnt lib/kadm5/srv
	lib/krad
	lib/apputils

	kdc kprop config-files build-tools man doc include

	plugins/certauth/test
	plugins/gssapi/negoextest
	plugins/hostrealm/test
	plugins/localauth/test
	plugins/kadm5_hook/test
	plugins/kadm5_auth/test
	plugins/pwqual/test
	plugins/audit
	plugins/audit/test
	plugins/kdb/db2
	plugins/kdb/db2/libdb2
	plugins/kdb/db2/libdb2/hash
	plugins/kdb/db2/libdb2/btree
	plugins/kdb/db2/libdb2/db
	plugins/kdb/db2/libdb2/mpool
	plugins/kdb/db2/libdb2/recno
	plugins/kdb/db2/libdb2/test
	plugins/kdb/test
	plugins/kdcpolicy/test
	plugins/preauth/otp
	plugins/preauth/spake
	plugins/preauth/test
	plugins/authdata/greet_client
	plugins/authdata/greet_server
	plugins/tls/k5tls

	clients clients/klist clients/kinit clients/kvno
	clients/kdestroy clients/kpasswd clients/ksu clients/kswitch

	kadmin kadmin/cli kadmin/dbutil kadmin/ktutil kadmin/server

	appl
	appl/sample appl/sample/sclient appl/sample/sserver
	appl/simple appl/simple/client appl/simple/server
	appl/gss-sample appl/user_user

	tests tests/asn.1 tests/create tests/hammer tests/verify tests/gssapi
	tests/threads tests/shlib tests/gss-threads tests/misc
)
