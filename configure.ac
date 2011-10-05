dnl Process this file with autoconf to produce a configure script.
AC_REVISION($Revision$)
AC_PREREQ(2.62)
test -z "$CFLAGS" && CFLAGS="-g"
AC_INIT([Heimdal],[1.5],[heimdal-bugs@h5l.org])
AC_CONFIG_SRCDIR([kuser/kinit.c])
AC_CONFIG_HEADERS(include/config.h)
AC_CONFIG_MACRO_DIR([cf])

AM_INIT_AUTOMAKE([foreign 1.10.3])
AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_LIBTOOL

AC_PREFIX_DEFAULT(/usr/heimdal)

test "$sysconfdir" = '${prefix}/etc' && sysconfdir='/etc'
test "$localstatedir" = '${prefix}/var' && localstatedir='/var/heimdal'

AC_CANONICAL_HOST
CANONICAL_HOST=$host
AC_SUBST(CANONICAL_HOST)

rk_SYS_LARGEFILE

rk_AIX
rk_IRIX
rk_SUNOS

dnl
dnl this is needed to run the configure tests against glibc
dnl
AC_DEFINE([_GNU_SOURCE], 1,
	[Define to enable extensions on glibc-based systems such as Linux.])

AC_OBJEXT
AC_EXEEXT

dnl AC_KRB_PROG_YACC
AC_PROG_YACC
AM_PROG_LEX
dnl AC_PROG_RANLIB
AC_PROG_AWK
AC_KRB_PROG_LN_S

AC_MIPS_ABI
CC="$CC $abi"
libdir="$libdir$abilibdirext"

AC_C___ATTRIBUTE__

LT_PREREQ([2.2])
LT_INIT([shared static win32-dll])

AM_CONDITIONAL(ENABLE_SHARED, test "$enable_shared" = "yes")
rk_VERSIONSCRIPT

dnl
dnl Helper bits for cross compiling
dnl



AM_CONDITIONAL(CROSS_COMPILE, test "${cross_compiling}" = yes)

AC_ARG_WITH(cross-tools,
	AS_HELP_STRING([--with-cross-tools=dir], [use cross tools in dir]),
	[if test "$withval" = "yes"; then
		AC_MSG_ERROR([Need path to cross tools])
	fi
	with_cross_tools="${with_cross_tools}/"
	])

if test "${cross_compiling}" != yes ; then

   ASN1_COMPILE="\$(top_builddir)/lib/asn1/asn1_compile\$(EXEEXT)"
   SLC="\$(top_builddir)/lib/sl/slc"

   ASN1_COMPILE_DEP="\$(ASN1_COMPILE)"
   SLC_DEP="\$(SLC)"
else
   ASN1_COMPILE="${with_cross_tools}asn1_compile"
   SLC="${with_cross_tools}slc"

   ASN1_COMPILE_DEP=
   SLC_DEP=
fi

AC_SUBST([ASN1_COMPILE])
AC_SUBST([ASN1_COMPILE_DEP])
AC_SUBST([SLC])
AC_SUBST([SLC_DEP])


dnl ---

AC_DEFINE(HEIM_WEAK_CRYPTO, 1, [Define if you want support for weak crypto])

rk_TEST_PACKAGE(openldap,
[#include <lber.h>
#include <ldap.h>],
[-lldap -llber],,,OPENLDAP)

AC_ARG_ENABLE(hdb-openldap-module, 
	AS_HELP_STRING([--enable-hdb-openldap-module],
		[if you want support to build openldap hdb as shared object]))
if test "$enable_hdb_openldap_module" = yes -a "$with_openldap" = yes; then
    AC_DEFINE(OPENLDAP_MODULE, 1, [Define if you want support for hdb ldap module])
fi
AM_CONDITIONAL(OPENLDAP_MODULE, test "$enable_hdb_openldap_module" = yes -a "$with_openldap" = yes)

dnl
dnl Optional modules, pk-init, digest, kx509
dnl

AC_ARG_ENABLE(pk-init, 
	AS_HELP_STRING([--disable-pk-init],
		[if you want disable to PK-INIT support]))
if test "$enable_pk_init" != no ;then
	AC_DEFINE([PKINIT], 1, [Define to enable PKINIT.])
fi
AM_CONDITIONAL(PKINIT, test "$enable_pk_init" != no)

AC_ARG_ENABLE(digest, 
	AS_HELP_STRING([--disable-digest],
		[if you want disable to DIGEST support]))
if test "$enable_digest" != no ;then
	AC_DEFINE([DIGEST], 1, [Define to enable DIGEST.])
fi

AC_ARG_ENABLE(kx509, 
	AS_HELP_STRING([--disable-kx509],
		[if you want disable to kx509 support]))
if test "$enable_kx509" != no ;then
	AC_DEFINE([KX509], 1, [Define to enable kx509.])
fi

dnl Need to test if pkg-config exists
PKG_PROG_PKG_CONFIG

dnl libcap-ng
AC_ARG_WITH([capng],
  AC_HELP_STRING([--with-capng], [use libcap-ng to drop KDC privileges @<:@default=check@:>@]),
  [],
  [with_capng=check])
if test "$with_capng" != "no"; then
  PKG_CHECK_MODULES([CAPNG], [libcap-ng >= 0.4.0],
		    [with_capng=yes],[with_capng=no])
fi
if test "$with_capng" = "yes"; then
  AC_DEFINE_UNQUOTED([HAVE_CAPNG], 1, [whether capng is available for privilege reduction])
fi
AM_CONDITIONAL([HAVE_CAPNG], [test "$with_capng" != "no"])
AC_SUBST([CAPNG_CFLAGS])
AC_SUBST([CAPNG_LIBS])

dnl Check for sqlite
rk_TEST_PACKAGE(sqlite3,
[#include <sqlite3.h>
#ifndef SQLITE_OPEN_CREATE
#error "old version"
#endif],
[-lsqlite3],,,SQLITE3)

if test "X$with_sqlite3" != Xyes ; then
   INCLUDE_sqlite3="-I\$(top_srcdir)/lib/sqlite"
   LIB_sqlite3="\$(top_builddir)/lib/sqlite/libheimsqlite.la"
fi
AM_CONDITIONAL(SQLITE3,  test "X$with_sqlite3" = Xyes)

AC_DEFINE(HAVE_SQLITE3, 1, [Define if you want support for sqlite in Heimdal.])

AC_ARG_ENABLE(sqlite-cache, 
	AS_HELP_STRING([--disable-sqlite-cache],[if you want support for cache in sqlite]))
if test "$enable_sqlite_cache" != no; then
    AC_DEFINE(HAVE_SCC, 1, [Define if you want support for cache in sqlite.])
fi
AM_CONDITIONAL(have_scc, test "$enable_sqlite_cache" != no)


dnl check for libintl
rk_TEST_PACKAGE(libintl,
[#include <libintl.h>],
[-lintl],,,LIBINTL)

dnl path where the hdb directory is stored
AC_ARG_WITH([hdbdir], 
	[AS_HELP_STRING([--with-hdbdir],[Default location for KDC database @<:@default=/var/heimdal@:>@])],
	[],
	[with_hdbdir=/var/heimdal])
DIR_hdbdir="$with_hdbdir"
AC_SUBST([DIR_hdbdir])


dnl no kerberos4 any more
with_krb4=no
AC_SUBST(INCLUDE_krb4)
AC_SUBST(LIB_krb4)
AM_CONDITIONAL(KRB4, false)

AM_CONDITIONAL(KRB5, true)
AM_CONDITIONAL(do_roken_rename, true)

AC_DEFINE(SUPPORT_INETD, 1, [Enable use of inetd style startup.])dnl


AC_DEFINE(KRB5, 1, [Enable Kerberos 5 support in applications.])dnl
AC_SUBST(LIB_kdb)dnl

KRB_CRYPTO

KRB_PTHREADS

AC_ARG_ENABLE(dce, 
	AS_HELP_STRING([--enable-dce],[if you want support for DCE/DFS PAG's]))
if test "$enable_dce" = yes; then
    AC_DEFINE(DCE, 1, [Define if you want support for DCE/DFS PAG's.])
fi
AM_CONDITIONAL(DCE, test "$enable_dce" = yes)

## XXX quite horrible:
if test -f /etc/ibmcxx.cfg; then
	dpagaix_ldadd=`sed -n '/^xlc_r4/,/^$/p' /etc/ibmcxx.cfg | sed -n -e '/libraries/{;s/^[[^=]]*=\(.*\)/\1/;s/,/ /gp;}'`
	dpagaix_cflags=`sed -n '/^xlc_r4/,/^$/p' /etc/ibmcxx.cfg | sed -n -e '/options/{;s/^[[^=]]*=\(.*\)/\1/;s/-q[^,]*//;s/,/ /gp;}'`
	dpagaix_ldflags=
else
	dpagaix_cflags="-D_THREAD_SAFE -D_AIX_PTHREADS_D7 -D_AIX32_THREADS=1 -D_AES_SOURCE -D_AIX41 -I/usr/include/dce"
	dpagaix_ldadd="-L/usr/lib/threads -ldcelibc_r -ldcepthreads -lpthreads_compat lpthreads -lc_r"
	dpagaix_ldflags="-Wl,-bI:dfspag.exp"
fi
AC_SUBST(dpagaix_cflags)
AC_SUBST(dpagaix_ldadd)
AC_SUBST(dpagaix_ldflags)

AC_ARG_ENABLE([afs-support],
	AS_HELP_STRING([--disable-afs-support],[if you don't want support for AFS]))
if test "$enable_afs_support" = no; then
	AC_DEFINE(NO_AFS, 1, [Define if you don't wan't support for AFS.])
	NO_AFS="1"
fi
AC_SUBST(NO_AFS)dnl

rk_DB

dnl AC_ROKEN(10,[/usr/heimdal /usr/athena],[lib/roken],[$(top_builddir)/lib/roken/libroken.la],[-I$(top_builddir)/lib/roken -I$(top_srcdir)/lib/roken])

rk_ROKEN(lib/roken)
LIBADD_roken="$LIB_roken"
AC_SUBST(LIBADD_roken)dnl
LIB_roken="\$(top_builddir)/lib/vers/libvers.la $LIB_roken"

rk_OTP

rk_LIBDISPATCH

AC_CHECK_OSFC2

AC_ARG_ENABLE(mmap,
	AS_HELP_STRING([--disable-mmap],[disable use of mmap]))
if test "$enable_mmap" = "no"; then
	AC_DEFINE(NO_MMAP, 1, [Define if you don't want to use mmap.])
fi

AC_ARG_ENABLE(afs-string-to-key,
	AS_HELP_STRING([--disable-afs-string-to-key],
	[disable use of weak AFS string-to-key functions]),
	[], [enable_afs_string_to_key=yes])

if test "$enable_afs_string_to_key" = "yes"; then
	AC_DEFINE(ENABLE_AFS_STRING_TO_KEY, 1, [Define if want to use the weak AFS string to key functions.])
fi


rk_CHECK_MAN

rk_TEST_PACKAGE(readline,
[#include <stdio.h>
#if defined(HAVE_READLINE_READLINE_H)
#include <readline/readline.h>
#elif defined(HAVE_READLINE_H)
#include <readline.h>
#endif
],-lreadline,,, READLINE,, [readline.h readline/readline.h])

rk_TEST_PACKAGE(libedit,
[#include <stdio.h>
#if defined(HAVE_READLINE_READLINE_H)
#include <readline/readline.h>
#elif defined(HAVE_READLINE_H)
#include <readline.h>
#endif
],-ledit,,, READLINE,, [readline.h readline/readline.h])

AC_CONFIG_SUBDIRS([lib/libedit])

rk_TEST_PACKAGE(hesiod,[#include <hesiod.h>],-lhesiod,,, HESIOD)

KRB_C_BIGENDIAN
AC_C_INLINE

KRB_CHECK_X

AM_CONDITIONAL(HAVE_X, test "$no_x" != yes)

AC_CHECK_XAU

dnl AM_C_PROTOTYPES

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_CHECK_TYPE_EXTRA(mode_t, unsigned short, [])
AC_CHECK_TYPE_EXTRA(sig_atomic_t, int, [#include <signal.h>])
AC_HAVE_TYPE([long long])
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for header files.
AC_HEADER_STDC

AC_CHECK_HEADERS([\
	CommonCrypto/CommonDigest.h		\
	CommonCrypto/CommonCryptor.h		\
	arpa/ftp.h				\
	arpa/telnet.h				\
	bind/bitypes.h				\
	bsdsetjmp.h				\
	curses.h				\
	dlfcn.h					\
	execinfo.h				\
	fnmatch.h				\
	inttypes.h				\
	io.h					\
	libutil.h				\
	limits.h				\
	maillock.h				\
	netgroup.h				\
	netinet/in6_machtypes.h			\
	pthread.h				\
	pty.h					\
	sac.h					\
	sgtty.h					\
	siad.h					\
	signal.h				\
	strings.h				\
	stropts.h				\
	sys/bitypes.h				\
	sys/category.h				\
	sys/file.h				\
	sys/filio.h				\
	sys/ioccom.h				\
	sys/mman.h				\
	sys/param.h				\
	sys/pty.h				\
	sys/ptyio.h				\
	sys/select.h				\
	sys/socket.h				\
	sys/str_tty.h				\
	sys/stream.h				\
	sys/stropts.h				\
	sys/syscall.h				\
	sys/termio.h				\
	sys/timeb.h				\
	sys/times.h				\
	sys/types.h				\
	sys/un.h				\
	locale.h				\
	termcap.h				\
	termio.h				\
	termios.h				\
	time.h					\
	tmpdir.h				\
	udb.h					\
	util.h					\
	utmp.h					\
	utmpx.h					\
])

dnl On Solaris 8 there's a compilation warning for term.h because
dnl it doesn't define `bool'.
AC_CHECK_HEADERS(term.h, , , -)

dnl aix have asl.h (A/IX screen library) that we don't want
AC_CHECK_HEADERS(asl.h, , , [
#include <asl.h>
#ifndef ASL_STRING_EMERG
#error ASL_STRING_EMERG missing
#endif])

AC_CHECK_HEADERS(net/if.h, , , [AC_INCLUDES_DEFAULT
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif])

AC_CHECK_HEADERS(sys/ptyvar.h, , , [AC_INCLUDES_DEFAULT
#if HAVE_SYS_TTY_H
#include <sys/tty.h>
#endif])

AC_CHECK_HEADERS(sys/strtty.h, , , [AC_INCLUDES_DEFAULT
#if HAVE_TERMIOS_H
#include <termios.h>
#endif
#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif])

AC_CHECK_HEADERS(sys/ucred.h, , , [AC_INCLUDES_DEFAULT
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif])

AC_CHECK_HEADERS(security/pam_modules.h, , , [AC_INCLUDES_DEFAULT
#include <security/pam_appl.h>
])

dnl export symbols
rk_WIN32_EXPORT(BUILD_KRB5_LIB, KRB5_LIB)
rk_WIN32_EXPORT(BUILD_ROKEN_LIB, ROKEN_LIB)
rk_WIN32_EXPORT(BUILD_GSSAPI_LIB, GSSAPI_LIB)

dnl Checks for libraries.

AC_FIND_FUNC_NO_LIBS(logwtmp, util,[
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
],[0,0,0])
AC_FIND_FUNC_NO_LIBS(logout, util,[
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
],[0])
AC_FIND_FUNC_NO_LIBS(openpty, util,[
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
],[0,0,0,0,0])

AC_FIND_FUNC_NO_LIBS(tgetent, termcap ncurses curses,[
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
],[0,0])

dnl Checks for library functions.

AC_CHECK_FUNCS([				\
	_getpty					\
	_scrsize				\
	arc4random				\
	backtrace				\
	fcntl					\
	getpeereid				\
	getpeerucred				\
	grantpt					\
	mktime					\
	ptsname					\
	rand					\
	revoke					\
	select					\
	setitimer				\
	setpcred				\
	setpgid					\
	setproctitle				\
	setregid				\
	setresgid				\
	setresuid				\
	setreuid				\
	setsid					\
	setutent				\
	sigaction				\
	strstr					\
	ttyname					\
	ttyslot					\
	umask					\
	unlockpt				\
	vhangup					\
	yp_get_default_domain			\
])

AC_MSG_CHECKING([checking for __sync_add_and_fetch])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>]],
	[[unsigned int foo; __sync_add_and_fetch(&foo, 1);]])],
	[ac_rk_have___sync_add_and_fetch=yes], [ac_rk_have___sync_add_and_fetch=no])
if test "$ac_rk_have___sync_add_and_fetch" = "yes" ; then
	AC_DEFINE_UNQUOTED(HAVE___SYNC_ADD_AND_FETCH, 1, [have __sync_add_and_fetch])
fi
AC_MSG_RESULT($ac_rk_have___sync_add_and_fetch)

AC_FUNC_MMAP

KRB_CAPABILITIES

AC_CHECK_GETPWNAM_R_POSIX

dnl detect doors on solaris
if test "$enable_pthread_support" != no; then
   saved_LIBS="$LIBS"
   LIBS="$LIBS $PTHREADS_LIBS"
   AC_FIND_FUNC_NO_LIBS(door_create, door)
   LIBS="$saved_LIBS"
fi

AC_ARG_ENABLE(kcm,
	AS_HELP_STRING([--enable-kcm],[enable Kerberos Credentials Manager]),
,[enable_kcm=yes])

if test "$enable_kcm" = yes ; then
   if test  "$ac_cv_header_sys_un_h" != yes -a "$ac_cv_funclib_door_create" != yes ; then
  	enable_kcm=no
   fi
fi
if test "$enable_kcm" = yes; then
       AC_DEFINE(HAVE_KCM, 1,
               [Define if you want to use the Kerberos Credentials Manager.])
fi
AM_CONDITIONAL(KCM, test "$enable_kcm" = yes)



dnl Cray stuff
AC_CHECK_FUNCS(getudbnam setlim)

dnl AC_KRB_FUNC_GETCWD_BROKEN

dnl
dnl Check for fields in struct utmp
dnl

AC_HAVE_STRUCT_FIELD(struct utmp, ut_addr, [#include <utmp.h>])
AC_HAVE_STRUCT_FIELD(struct utmp, ut_host, [#include <utmp.h>])
AC_HAVE_STRUCT_FIELD(struct utmp, ut_id, [#include <utmp.h>])
AC_HAVE_STRUCT_FIELD(struct utmp, ut_pid, [#include <utmp.h>])
AC_HAVE_STRUCT_FIELD(struct utmp, ut_type, [#include <utmp.h>])
AC_HAVE_STRUCT_FIELD(struct utmp, ut_user, [#include <utmp.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_host, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_id, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_line, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_pid, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_tv, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_type, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_user, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_exit, [#include <utmpx.h>])
AC_HAVE_STRUCT_FIELD(struct utmpx, ut_syslen, [#include <utmpx.h>])

AC_CHECK_TYPES([int8_t, int16_t, int32_t, int64_t, 
	u_int8_t, u_int16_t, u_int32_t, u_int64_t,
	uint8_t, uint16_t, uint32_t, uint64_t],,,[
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#ifdef HAVE_BIND_BITYPES_H
#include <bind/bitypes.h>
#endif
#ifdef HAVE_NETINET_IN6_MACHTYPES_H
#include <netinet/in6_machtypes.h>
#endif
])

rk_FRAMEWORK_SECURITY

KRB_READLINE

rk_TELNET

dnl Some operating systems already have com_err and compile_et
CHECK_COMPILE_ET

rk_AUTH_MODULES([sia afskauthlib])

rk_DESTDIRS

rk_WFLAGS([-Wall -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast -Wmissing-declarations -Wnested-externs])


AH_BOTTOM([#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif])

AH_BOTTOM([#ifdef ROKEN_RENAME
#include "roken_rename.h"
#endif])

AC_ARG_ENABLE(heimdal-documentation,
	AS_HELP_STRING([--disable-heimdal-documentation],
		[if you want disable to heimdal documentation]))
AM_CONDITIONAL(HEIMDAL_DOCUMENTATION, test "$enable_heimdal_documentation" != no)

AC_CONFIG_FILES(Makefile 		\
	etc/Makefile			\
	include/Makefile		\
	include/gssapi/Makefile		\
	include/hcrypto/Makefile	\
	include/kadm5/Makefile		\
	lib/Makefile			\
	base/Makefile			\
	lib/asn1/Makefile		\
	lib/com_err/Makefile		\
	lib/hcrypto/Makefile		\
	lib/hx509/Makefile		\
	lib/gssapi/Makefile		\
	lib/ntlm/Makefile		\
	lib/hdb/Makefile		\
	lib/ipc/Makefile		\
	lib/kadm5/Makefile		\
	lib/kafs/Makefile		\
	lib/kdfs/Makefile		\
	lib/krb5/Makefile		\
	lib/otp/Makefile		\
	lib/roken/Makefile		\
	lib/sl/Makefile			\
	lib/sqlite/Makefile		\
	lib/vers/Makefile		\
	lib/wind/Makefile		\
	po/Makefile			\
	kuser/Makefile			\
	kpasswd/Makefile		\
	kadmin/Makefile			\
	admin/Makefile			\
	kcm/Makefile			\
	kdc/Makefile			\
	appl/Makefile			\
	appl/afsutil/Makefile		\
	appl/ftp/Makefile		\
	appl/ftp/common/Makefile	\
	appl/ftp/ftp/Makefile		\
	appl/ftp/ftpd/Makefile		\
	appl/gssmask/Makefile		\
	appl/kx/Makefile		\
	appl/login/Makefile		\
	appl/otp/Makefile		\
	appl/popper/Makefile		\
	appl/push/Makefile		\
	appl/rsh/Makefile		\
	appl/rcp/Makefile		\
	appl/su/Makefile		\
	appl/xnlock/Makefile		\
	appl/telnet/Makefile		\
	appl/telnet/libtelnet/Makefile	\
	appl/telnet/telnet/Makefile	\
	appl/telnet/telnetd/Makefile	\
	appl/test/Makefile		\
	appl/kf/Makefile		\
	appl/dceutils/Makefile		\
	tests/Makefile			\
	tests/bin/Makefile		\
	tests/can/Makefile		\
	tests/db/Makefile		\
	tests/kdc/Makefile		\
	tests/ldap/Makefile		\
	tests/gss/Makefile		\
	tests/java/Makefile		\
	tests/plugin/Makefile		\
	packages/Makefile		\
	packages/mac/Makefile		\
	doc/Makefile			\
	tools/Makefile			\
)

AC_OUTPUT

dnl
dnl This is the release version name-number[beta]
dnl

cat > include/newversion.h.in <<EOF
#ifndef VERSION_HIDDEN
#define VERSION_HIDDEN
#endif
VERSION_HIDDEN const char *heimdal_long_version = "@([#])\$Version: $PACKAGE_STRING by @USER@ on @HOST@ ($host) @DATE@ \$";
VERSION_HIDDEN const char *heimdal_version = "AC_PACKAGE_STRING";
EOF

if test -f include/version.h && cmp -s include/newversion.h.in include/version.h.in; then
	echo "include/version.h is unchanged"
	rm -f include/newversion.h.in
else
 	echo "creating include/version.h"
 	User=${USER-${LOGNAME}}
 	Host=`(hostname || uname -n || echo unknown) 2>/dev/null | sed 1q`
 	Date=`date`
	mv -f include/newversion.h.in include/version.h.in
	sed -e "s/@USER@/$User/" -e "s/@HOST@/$Host/" -e "s/@DATE@/$Date/" include/version.h.in > include/version.h
fi
