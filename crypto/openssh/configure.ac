# $Id: configure.ac,v 1.292 2005/08/31 16:59:49 tim Exp $
#
# Copyright (c) 1999-2004 Damien Miller
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

AC_INIT(OpenSSH, Portable, openssh-unix-dev@mindrot.org)
AC_CONFIG_SRCDIR([ssh.c])

AC_CONFIG_HEADER(config.h)
AC_PROG_CC
AC_CANONICAL_HOST
AC_C_BIGENDIAN

# Checks for programs.
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PATH_PROG(AR, ar)
AC_PATH_PROG(CAT, cat)
AC_PATH_PROG(KILL, kill)
AC_PATH_PROGS(PERL, perl5 perl)
AC_PATH_PROG(SED, sed)
AC_SUBST(PERL)
AC_PATH_PROG(ENT, ent)
AC_SUBST(ENT)
AC_PATH_PROG(TEST_MINUS_S_SH, bash)
AC_PATH_PROG(TEST_MINUS_S_SH, ksh)
AC_PATH_PROG(TEST_MINUS_S_SH, sh)
AC_PATH_PROG(SH, sh)
AC_SUBST(TEST_SHELL,sh)

dnl for buildpkg.sh
AC_PATH_PROG(PATH_GROUPADD_PROG, groupadd, groupadd,
	[/usr/sbin${PATH_SEPARATOR}/etc])
AC_PATH_PROG(PATH_USERADD_PROG, useradd, useradd,
	[/usr/sbin${PATH_SEPARATOR}/etc])
AC_CHECK_PROG(MAKE_PACKAGE_SUPPORTED, pkgmk, yes, no)

# System features
AC_SYS_LARGEFILE

if test -z "$AR" ; then
	AC_MSG_ERROR([*** 'ar' missing, please install or fix your \$PATH ***])
fi

# Use LOGIN_PROGRAM from environment if possible
if test ! -z "$LOGIN_PROGRAM" ; then
	AC_DEFINE_UNQUOTED(LOGIN_PROGRAM_FALLBACK, "$LOGIN_PROGRAM")
else
	# Search for login
	AC_PATH_PROG(LOGIN_PROGRAM_FALLBACK, login)
	if test ! -z "$LOGIN_PROGRAM_FALLBACK" ; then
		AC_DEFINE_UNQUOTED(LOGIN_PROGRAM_FALLBACK, "$LOGIN_PROGRAM_FALLBACK")
	fi
fi

AC_PATH_PROG(PATH_PASSWD_PROG, passwd)
if test ! -z "$PATH_PASSWD_PROG" ; then
	AC_DEFINE_UNQUOTED(_PATH_PASSWD_PROG, "$PATH_PASSWD_PROG")
fi

if test -z "$LD" ; then
	LD=$CC
fi
AC_SUBST(LD)

AC_C_INLINE

AC_CHECK_DECL(LLONG_MAX, have_llong_max=1, , [#include <limits.h>])

if test "$GCC" = "yes" || test "$GCC" = "egcs"; then
	CFLAGS="$CFLAGS -Wall -Wpointer-arith -Wuninitialized"
	GCC_VER=`$CC --version`
	case $GCC_VER in
		1.*) ;;
		2.8* | 2.9*) CFLAGS="$CFLAGS -Wsign-compare" ;;
		2.*) ;;
		*) CFLAGS="$CFLAGS -Wsign-compare" ;;
	esac

	if test -z "$have_llong_max"; then
		# retry LLONG_MAX with -std=gnu99, needed on some Linuxes
		unset ac_cv_have_decl_LLONG_MAX
		saved_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS -std=gnu99"
		AC_CHECK_DECL(LLONG_MAX,
		    [have_llong_max=1],
		    [CFLAGS="$saved_CFLAGS"],
		    [#include <limits.h>]
		)
	fi
fi

if test -z "$have_llong_max"; then
	AC_MSG_CHECKING([for max value of long long])
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdio.h>
/* Why is this so damn hard? */
#ifdef __GNUC__
# undef __GNUC__
#endif
#define __USE_ISOC99
#include <limits.h>
#define DATA "conftest.llminmax"
int main(void) {
	FILE *f;
	long long i, llmin, llmax = 0;

	if((f = fopen(DATA,"w")) == NULL)
		exit(1);

#if defined(LLONG_MIN) && defined(LLONG_MAX)
	fprintf(stderr, "Using system header for LLONG_MIN and LLONG_MAX\n");
	llmin = LLONG_MIN;
	llmax = LLONG_MAX;
#else
	fprintf(stderr, "Calculating  LLONG_MIN and LLONG_MAX\n");
	/* This will work on one's complement and two's complement */
	for (i = 1; i > llmax; i <<= 1, i++)
		llmax = i;
	llmin = llmax + 1LL;	/* wrap */
#endif

	/* Sanity check */
	if (llmin + 1 < llmin || llmin - 1 < llmin || llmax + 1 > llmax
	    || llmax - 1 > llmax) {
		fprintf(f, "unknown unknown\n");
		exit(2);
	}

	if (fprintf(f ,"%lld %lld", llmin, llmax) < 0)
		exit(3);

	exit(0);
}
		]])],
		[
			llong_min=`$AWK '{print $1}' conftest.llminmax`
			llong_max=`$AWK '{print $2}' conftest.llminmax`
			AC_MSG_RESULT($llong_max)
			AC_DEFINE_UNQUOTED(LLONG_MAX, [${llong_max}LL],
			    [max value of long long calculated by configure])
			AC_MSG_CHECKING([for min value of long long])
			AC_MSG_RESULT($llong_min)
			AC_DEFINE_UNQUOTED(LLONG_MIN, [${llong_min}LL],
			    [min value of long long calculated by configure])
		],
		[
			AC_MSG_RESULT(not found)
		],
		[
			AC_MSG_WARN([cross compiling: not checking])
		]
	)
fi

AC_ARG_WITH(rpath,
	[  --without-rpath         Disable auto-added -R linker paths],
	[
		if test "x$withval" = "xno" ; then
			need_dash_r=""
		fi
		if test "x$withval" = "xyes" ; then
			need_dash_r=1
		fi
	]
)

# Check for some target-specific stuff
case "$host" in
*-*-aix*)
	AC_MSG_CHECKING([how to specify blibpath for linker ($LD)])
	if (test -z "$blibpath"); then
		blibpath="/usr/lib:/lib"
	fi
	saved_LDFLAGS="$LDFLAGS"
	for tryflags in -blibpath: -Wl,-blibpath: -Wl,-rpath, ;do
		if (test -z "$blibflags"); then
			LDFLAGS="$saved_LDFLAGS $tryflags$blibpath"
			AC_TRY_LINK([], [], [blibflags=$tryflags])
		fi
	done
	if (test -z "$blibflags"); then
		AC_MSG_RESULT(not found)
		AC_MSG_ERROR([*** must be able to specify blibpath on AIX - check config.log])
	else
		AC_MSG_RESULT($blibflags)
	fi
	LDFLAGS="$saved_LDFLAGS"
	dnl Check for authenticate.  Might be in libs.a on older AIXes
	AC_CHECK_FUNC(authenticate, [AC_DEFINE(WITH_AIXAUTHENTICATE)],
		[AC_CHECK_LIB(s,authenticate,
			[ AC_DEFINE(WITH_AIXAUTHENTICATE)
				LIBS="$LIBS -ls"
			])
		])
	dnl Check for various auth function declarations in headers.
	AC_CHECK_DECLS([authenticate, loginrestrictions, loginsuccess,
	    passwdexpired, setauthdb], , , [#include <usersec.h>])
	dnl Check if loginfailed is declared and takes 4 arguments (AIX >= 5.2)
	AC_CHECK_DECLS(loginfailed,
		 [AC_MSG_CHECKING(if loginfailed takes 4 arguments)
		  AC_TRY_COMPILE(
			[#include <usersec.h>],
			[(void)loginfailed("user","host","tty",0);],
			[AC_MSG_RESULT(yes)
			 AC_DEFINE(AIX_LOGINFAILED_4ARG)],
			[AC_MSG_RESULT(no)]
		)],
		[],
		[#include <usersec.h>]
	)
	AC_CHECK_FUNCS(setauthdb)
	check_for_aix_broken_getaddrinfo=1
	AC_DEFINE(BROKEN_REALPATH)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	dnl AIX handles lastlog as part of its login message
	AC_DEFINE(DISABLE_LASTLOG)
	AC_DEFINE(LOGIN_NEEDS_UTMPX)
	AC_DEFINE(SPT_TYPE,SPT_REUSEARGV)
	;;
*-*-cygwin*)
	check_for_libcrypt_later=1
	LIBS="$LIBS /usr/lib/textmode.o"
	AC_DEFINE(HAVE_CYGWIN)
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(DISABLE_SHADOW)
	AC_DEFINE(IP_TOS_IS_BROKEN)
	AC_DEFINE(NO_X11_UNIX_SOCKETS)
	AC_DEFINE(NO_IPPORT_RESERVED_CONCEPT)
	AC_DEFINE(DISABLE_FD_PASSING)
	;;
*-*-dgux*)
	AC_DEFINE(IP_TOS_IS_BROKEN)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	;;
*-*-darwin*)
	AC_MSG_CHECKING(if we have working getaddrinfo)
	AC_TRY_RUN([#include <mach-o/dyld.h>
main() { if (NSVersionOfRunTimeLibrary("System") >= (60 << 16))
		exit(0);
	else
		exit(1);
}], [AC_MSG_RESULT(working)],
	[AC_MSG_RESULT(buggy)
	AC_DEFINE(BROKEN_GETADDRINFO)],
	[AC_MSG_RESULT(assume it is working)])
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE_UNQUOTED(BIND_8_COMPAT, 1)
	;;
*-*-hpux*)
	# first we define all of the options common to all HP-UX releases
	CPPFLAGS="$CPPFLAGS -D_HPUX_SOURCE -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1"
	IPADDR_IN_DISPLAY=yes
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(LOGIN_NO_ENDOPT)
	AC_DEFINE(LOGIN_NEEDS_UTMPX)
	AC_DEFINE(LOCKED_PASSWD_STRING, "*")
	AC_DEFINE(SPT_TYPE,SPT_PSTAT)
	LIBS="$LIBS -lsec"
	AC_CHECK_LIB(xnet, t_error, ,
	    AC_MSG_ERROR([*** -lxnet needed on HP-UX - check config.log ***]))

	# next, we define all of the options specific to major releases
	case "$host" in
	*-*-hpux10*)
		if test -z "$GCC"; then
			CFLAGS="$CFLAGS -Ae"
		fi
		;;
	*-*-hpux11*)
		AC_DEFINE(PAM_SUN_CODEBASE)
		AC_DEFINE(DISABLE_UTMP)
		AC_DEFINE(USE_BTMP, 1, [Use btmp to log bad logins])
		check_for_hpux_broken_getaddrinfo=1
		check_for_conflicting_getspnam=1
		;;
	esac

	# lastly, we define options specific to minor releases
	case "$host" in
	*-*-hpux10.26)
		AC_DEFINE(HAVE_SECUREWARE)
		disable_ptmx_check=yes
		LIBS="$LIBS -lsecpw"
		;;
	esac
	;;
*-*-irix5*)
	PATH="$PATH:/usr/etc"
	AC_DEFINE(BROKEN_INET_NTOA)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(WITH_ABBREV_NO_TTY)
	AC_DEFINE(LOCKED_PASSWD_STRING, "*LK*")
	;;
*-*-irix6*)
	PATH="$PATH:/usr/etc"
	AC_DEFINE(WITH_IRIX_ARRAY)
	AC_DEFINE(WITH_IRIX_PROJECT)
	AC_DEFINE(WITH_IRIX_AUDIT)
	AC_CHECK_FUNC(jlimit_startjob, [AC_DEFINE(WITH_IRIX_JOBS)])
	AC_DEFINE(BROKEN_INET_NTOA)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(BROKEN_UPDWTMPX)
	AC_DEFINE(WITH_ABBREV_NO_TTY)
	AC_DEFINE(LOCKED_PASSWD_STRING, "*LK*")
	;;
*-*-linux*)
	no_dev_ptmx=1
	check_for_libcrypt_later=1
	check_for_openpty_ctty_bug=1
	AC_DEFINE(DONT_TRY_OTHER_AF)
	AC_DEFINE(PAM_TTY_KLUDGE)
	AC_DEFINE(LOCKED_PASSWD_PREFIX, "!")
	AC_DEFINE(SPT_TYPE,SPT_REUSEARGV)
	AC_DEFINE(LINK_OPNOTSUPP_ERRNO, EPERM)
	AC_DEFINE(_PATH_BTMP, "/var/log/btmp", [log for bad login attempts])
	AC_DEFINE(USE_BTMP, 1, [Use btmp to log bad logins])
	inet6_default_4in6=yes
	case `uname -r` in
	1.*|2.0.*)
		AC_DEFINE(BROKEN_CMSG_TYPE)
		;;
	esac
	;;
mips-sony-bsd|mips-sony-newsos4)
	AC_DEFINE(NEED_SETPRGP, [], [Need setpgrp to acquire controlling tty])
	SONY=1
	;;
*-*-netbsd*)
	check_for_libcrypt_before=1
	if test "x$withval" != "xno" ; then
		need_dash_r=1
	fi
	;;
*-*-freebsd*)
	check_for_libcrypt_later=1
	;;
*-*-bsdi*)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	;;
*-next-*)
	conf_lastlog_location="/usr/adm/lastlog"
	conf_utmp_location=/etc/utmp
	conf_wtmp_location=/usr/adm/wtmp
	MAIL=/usr/spool/mail
	AC_DEFINE(HAVE_NEXT)
	AC_DEFINE(BROKEN_REALPATH)
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(BROKEN_SAVED_UIDS)
	;;
*-*-openbsd*)
	AC_DEFINE(HAVE_ATTRIBUTE__SENTINEL__, 1, [OpenBSD's gcc has sentinel])
	;;
*-*-solaris*)
	if test "x$withval" != "xno" ; then
		need_dash_r=1
	fi
	AC_DEFINE(PAM_SUN_CODEBASE)
	AC_DEFINE(LOGIN_NEEDS_UTMPX)
	AC_DEFINE(LOGIN_NEEDS_TERM)
	AC_DEFINE(PAM_TTY_KLUDGE)
	AC_DEFINE(SSHPAM_CHAUTHTOK_NEEDS_RUID)
	AC_DEFINE(LOCKED_PASSWD_STRING, "*LK*")
	# Pushing STREAMS modules will cause sshd to acquire a controlling tty.
	AC_DEFINE(SSHD_ACQUIRES_CTTY)
	external_path_file=/etc/default/login
	# hardwire lastlog location (can't detect it on some versions)
	conf_lastlog_location="/var/adm/lastlog"
	AC_MSG_CHECKING(for obsolete utmp and wtmp in solaris2.x)
	sol2ver=`echo "$host"| sed -e 's/.*[[0-9]]\.//'`
	if test "$sol2ver" -ge 8; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(DISABLE_UTMP)
		AC_DEFINE(DISABLE_WTMP)
	else
		AC_MSG_RESULT(no)
	fi
	;;
*-*-sunos4*)
	CPPFLAGS="$CPPFLAGS -DSUNOS4"
	AC_CHECK_FUNCS(getpwanam)
	AC_DEFINE(PAM_SUN_CODEBASE)
	conf_utmp_location=/etc/utmp
	conf_wtmp_location=/var/adm/wtmp
	conf_lastlog_location=/var/adm/lastlog
	AC_DEFINE(USE_PIPES)
	;;
*-ncr-sysv*)
	LIBS="$LIBS -lc89"
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(SSHD_ACQUIRES_CTTY)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	;;
*-sni-sysv*)
	# /usr/ucblib MUST NOT be searched on ReliantUNIX
	AC_CHECK_LIB(dl, dlsym, ,)
	# -lresolv needs to be at then end of LIBS or DNS lookups break
	AC_CHECK_LIB(res_query, resolv, [ LIBS="$LIBS -lresolv" ])
	IPADDR_IN_DISPLAY=yes
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(IP_TOS_IS_BROKEN)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(SSHD_ACQUIRES_CTTY)
	external_path_file=/etc/default/login
	# /usr/ucblib/libucb.a no longer needed on ReliantUNIX
	# Attention: always take care to bind libsocket and libnsl before libc,
	# otherwise you will find lots of "SIOCGPGRP errno 22" on syslog
	;;
# UnixWare 1.x, UnixWare 2.x, and others based on code from Univel.
*-*-sysv4.2*)
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(PASSWD_NEEDS_USERNAME, 1, [must supply username to passwd])
	;;
# UnixWare 7.x, OpenUNIX 8
*-*-sysv5*)
	check_for_libcrypt_later=1
	AC_DEFINE(UNIXWARE_LONG_PASSWORDS, 1, [Support passwords > 8 chars])
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(PASSWD_NEEDS_USERNAME, 1, [must supply username to passwd])
	case "$host" in
	*-*-sysv5SCO_SV*)	# SCO OpenServer 6.x
		TEST_SHELL=/u95/bin/sh
		AC_DEFINE(BROKEN_LIBIAF, 1, [ia_uinfo routines not supported by OS yet])
		;;
	esac
	;;
*-*-sysv*)
	;;
# SCO UNIX and OEM versions of SCO UNIX
*-*-sco3.2v4*)
	AC_MSG_ERROR("This Platform is no longer supported.")
	;;
# SCO OpenServer 5.x
*-*-sco3.2v5*)
	if test -z "$GCC"; then
		CFLAGS="$CFLAGS -belf"
	fi
	LIBS="$LIBS -lprot -lx -ltinfo -lm"
	no_dev_ptmx=1
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(HAVE_SECUREWARE)
	AC_DEFINE(DISABLE_SHADOW)
	AC_DEFINE(DISABLE_FD_PASSING)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(WITH_ABBREV_NO_TTY)
	AC_DEFINE(BROKEN_UPDWTMPX)
	AC_DEFINE(PASSWD_NEEDS_USERNAME, 1, [must supply username to passwd])
	AC_CHECK_FUNCS(getluid setluid)
	MANTYPE=man
	TEST_SHELL=ksh
	;;
*-*-unicosmk*)
	AC_DEFINE(NO_SSH_LASTLOG)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(DISABLE_FD_PASSING)
	LDFLAGS="$LDFLAGS"
	LIBS="$LIBS -lgen -lrsc -lshare -luex -lacm"
	MANTYPE=cat
	;;
*-*-unicosmp*)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(WITH_ABBREV_NO_TTY)
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(DISABLE_FD_PASSING)
	LDFLAGS="$LDFLAGS"
	LIBS="$LIBS -lgen -lacid -ldb"
	MANTYPE=cat
	;;
*-*-unicos*)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(DISABLE_FD_PASSING)
	AC_DEFINE(NO_SSH_LASTLOG)
	LDFLAGS="$LDFLAGS -Wl,-Dmsglevel=334:fatal"
	LIBS="$LIBS -lgen -lrsc -lshare -luex -lacm"
	MANTYPE=cat
	;;
*-dec-osf*)
	AC_MSG_CHECKING(for Digital Unix SIA)
	no_osfsia=""
	AC_ARG_WITH(osfsia,
		[  --with-osfsia           Enable Digital Unix SIA],
		[
			if test "x$withval" = "xno" ; then
				AC_MSG_RESULT(disabled)
				no_osfsia=1
			fi
		],
	)
	if test -z "$no_osfsia" ; then
		if test -f /etc/sia/matrix.conf; then
			AC_MSG_RESULT(yes)
			AC_DEFINE(HAVE_OSF_SIA)
			AC_DEFINE(DISABLE_LOGIN)
			AC_DEFINE(DISABLE_FD_PASSING)
			LIBS="$LIBS -lsecurity -ldb -lm -laud"
		else
			AC_MSG_RESULT(no)
			AC_DEFINE(LOCKED_PASSWD_SUBSTR, "Nologin")
		fi
	fi
	AC_DEFINE(BROKEN_GETADDRINFO)
	AC_DEFINE(SETEUID_BREAKS_SETUID)
	AC_DEFINE(BROKEN_SETREUID)
	AC_DEFINE(BROKEN_SETREGID)
	;;

*-*-nto-qnx)
	AC_DEFINE(USE_PIPES)
	AC_DEFINE(NO_X11_UNIX_SOCKETS)
	AC_DEFINE(MISSING_NFDBITS)
	AC_DEFINE(MISSING_HOWMANY)
	AC_DEFINE(MISSING_FD_MASK)
	;;

*-*-ultrix*)
	AC_DEFINE(BROKEN_GETGROUPS, [], [getgroups(0,NULL) will return -1])
	AC_DEFINE(BROKEN_MMAP, [], [Ultrix mmap can't map files])
	AC_DEFINE(NEED_SETPRGP, [], [Need setpgrp to acquire controlling tty])
	AC_DEFINE(HAVE_SYS_SYSLOG_H, 1, [Force use of sys/syslog.h on Ultrix])
	;;

*-*-lynxos)
        CFLAGS="$CFLAGS -D__NO_INCLUDE_WARN__"
        AC_DEFINE(MISSING_HOWMANY)
        AC_DEFINE(BROKEN_SETVBUF, 1, [LynxOS has broken setvbuf() implementation])
        ;;
esac

# Allow user to specify flags
AC_ARG_WITH(cflags,
	[  --with-cflags           Specify additional flags to pass to compiler],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			CFLAGS="$CFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(cppflags,
	[  --with-cppflags         Specify additional flags to pass to preprocessor] ,
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			CPPFLAGS="$CPPFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(ldflags,
	[  --with-ldflags          Specify additional flags to pass to linker],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			LDFLAGS="$LDFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(libs,
	[  --with-libs             Specify additional libraries to link with],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			LIBS="$LIBS $withval"
		fi
	]
)
AC_ARG_WITH(Werror,
	[  --with-Werror           Build main code with -Werror],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"; then
			werror_flags="-Werror"
			if "x${withval}" != "xyes"; then
				werror_flags="$withval"
			fi
		fi
	]
)

AC_MSG_CHECKING(compiler and flags for sanity)
AC_RUN_IFELSE(
	[AC_LANG_SOURCE([
#include <stdio.h>
int main(){exit(0);}
	])],
	[	AC_MSG_RESULT(yes) ],
	[
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([*** compiler cannot create working executables, check config.log ***])
	],
	[	AC_MSG_WARN([cross compiling: not checking compiler sanity]) ]
)

dnl Checks for header files.
AC_CHECK_HEADERS( \
	bstring.h \
	crypt.h \
	dirent.h \
	endian.h \
	features.h \
	floatingpoint.h \
	getopt.h \
	glob.h \
	ia.h \
	iaf.h \
	lastlog.h \
	limits.h \
	login.h \
	login_cap.h \
	maillock.h \
	ndir.h \
	netdb.h \
	netgroup.h \
	netinet/in_systm.h \
	pam/pam_appl.h \
	paths.h \
	pty.h \
	readpassphrase.h \
	rpc/types.h \
	security/pam_appl.h \
	shadow.h \
	stddef.h \
	stdint.h \
	string.h \
	strings.h \
	sys/audit.h \
	sys/bitypes.h \
	sys/bsdtty.h \
	sys/cdefs.h \
	sys/dir.h \
	sys/mman.h \
	sys/ndir.h \
	sys/prctl.h \
	sys/pstat.h \
	sys/select.h \
	sys/stat.h \
	sys/stream.h \
	sys/stropts.h \
	sys/strtio.h \
	sys/sysmacros.h \
	sys/time.h \
	sys/timers.h \
	sys/un.h \
	time.h \
	tmpdir.h \
	ttyent.h \
	unistd.h \
	usersec.h \
	util.h \
	utime.h \
	utmp.h \
	utmpx.h \
	vis.h \
)

# sys/ptms.h requires sys/stream.h to be included first on Solaris
AC_CHECK_HEADERS(sys/ptms.h, [], [], [
#ifdef HAVE_SYS_STREAM_H
# include <sys/stream.h>
#endif
])

# Checks for libraries.
AC_CHECK_FUNC(yp_match, , AC_CHECK_LIB(nsl, yp_match))
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))

dnl IRIX and Solaris 2.5.1 have dirname() in libgen
AC_CHECK_FUNCS(dirname, [AC_CHECK_HEADERS(libgen.h)] ,[
	AC_CHECK_LIB(gen, dirname,[
		AC_CACHE_CHECK([for broken dirname],
			ac_cv_have_broken_dirname, [
			save_LIBS="$LIBS"
			LIBS="$LIBS -lgen"
			AC_TRY_RUN(
				[
#include <libgen.h>
#include <string.h>

int main(int argc, char **argv) {
    char *s, buf[32];

    strncpy(buf,"/etc", 32);
    s = dirname(buf);
    if (!s || strncmp(s, "/", 32) != 0) {
	exit(1);
    } else {
	exit(0);
    }
}
				],
				[ ac_cv_have_broken_dirname="no" ],
				[ ac_cv_have_broken_dirname="yes" ]
			)
			LIBS="$save_LIBS"
		])
		if test "x$ac_cv_have_broken_dirname" = "xno" ; then
			LIBS="$LIBS -lgen"
			AC_DEFINE(HAVE_DIRNAME)
			AC_CHECK_HEADERS(libgen.h)
		fi
	])
])

AC_CHECK_FUNC(getspnam, ,
	AC_CHECK_LIB(gen, getspnam, LIBS="$LIBS -lgen"))
AC_SEARCH_LIBS(basename, gen, AC_DEFINE(HAVE_BASENAME))

dnl zlib is required
AC_ARG_WITH(zlib,
	[  --with-zlib=PATH        Use zlib in PATH],
	[ if test "x$withval" = "xno" ; then
		AC_MSG_ERROR([*** zlib is required ***])
	  elif test "x$withval" != "xyes"; then
		if test -d "$withval/lib"; then
			if test -n "${need_dash_r}"; then
				LDFLAGS="-L${withval}/lib -R${withval}/lib ${LDFLAGS}"
			else
				LDFLAGS="-L${withval}/lib ${LDFLAGS}"
			fi
		else
			if test -n "${need_dash_r}"; then
				LDFLAGS="-L${withval} -R${withval} ${LDFLAGS}"
			else
				LDFLAGS="-L${withval} ${LDFLAGS}"
			fi
		fi
		if test -d "$withval/include"; then
			CPPFLAGS="-I${withval}/include ${CPPFLAGS}"
		else
			CPPFLAGS="-I${withval} ${CPPFLAGS}"
		fi
	fi ]
)

AC_CHECK_LIB(z, deflate, ,
	[
		saved_CPPFLAGS="$CPPFLAGS"
		saved_LDFLAGS="$LDFLAGS"
		save_LIBS="$LIBS"
		dnl Check default zlib install dir
		if test -n "${need_dash_r}"; then
			LDFLAGS="-L/usr/local/lib -R/usr/local/lib ${saved_LDFLAGS}"
		else
			LDFLAGS="-L/usr/local/lib ${saved_LDFLAGS}"
		fi
		CPPFLAGS="-I/usr/local/include ${saved_CPPFLAGS}"
		LIBS="$LIBS -lz"
		AC_TRY_LINK_FUNC(deflate, AC_DEFINE(HAVE_LIBZ),
			[
				AC_MSG_ERROR([*** zlib missing - please install first or check config.log ***])
			]
		)
	]
)
AC_CHECK_HEADER([zlib.h], ,AC_MSG_ERROR([*** zlib.h missing - please install first or check config.log ***]))

AC_ARG_WITH(zlib-version-check,
	[  --without-zlib-version-check Disable zlib version check],
	[  if test "x$withval" = "xno" ; then
		zlib_check_nonfatal=1
	   fi
	]
)

AC_MSG_CHECKING(for possibly buggy zlib)
AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <zlib.h>
int main()
{
	int a=0, b=0, c=0, d=0, n, v;
	n = sscanf(ZLIB_VERSION, "%d.%d.%d.%d", &a, &b, &c, &d);
	if (n != 3 && n != 4)
		exit(1);
	v = a*1000000 + b*10000 + c*100 + d;
	fprintf(stderr, "found zlib version %s (%d)\n", ZLIB_VERSION, v);

	/* 1.1.4 is OK */
	if (a == 1 && b == 1 && c >= 4)
		exit(0);

	/* 1.2.3 and up are OK */
	if (v >= 1020300)
		exit(0);

	exit(2);
}
	]])],
	AC_MSG_RESULT(no),
	[ AC_MSG_RESULT(yes)
	  if test -z "$zlib_check_nonfatal" ; then
		AC_MSG_ERROR([*** zlib too old - check config.log ***
Your reported zlib version has known security problems.  It's possible your
vendor has fixed these problems without changing the version number.  If you
are sure this is the case, you can disable the check by running
"./configure --without-zlib-version-check".
If you are in doubt, upgrade zlib to version 1.2.3 or greater.
See http://www.gzip.org/zlib/ for details.])
	  else
		AC_MSG_WARN([zlib version may have security problems])
	  fi
	],
	[	AC_MSG_WARN([cross compiling: not checking zlib version]) ]
)

dnl UnixWare 2.x
AC_CHECK_FUNC(strcasecmp,
	[], [ AC_CHECK_LIB(resolv, strcasecmp, LIBS="$LIBS -lresolv") ]
)
AC_CHECK_FUNC(utimes,
	[], [ AC_CHECK_LIB(c89, utimes, [AC_DEFINE(HAVE_UTIMES)
					LIBS="$LIBS -lc89"]) ]
)

dnl    Checks for libutil functions
AC_CHECK_HEADERS(libutil.h)
AC_SEARCH_LIBS(login, util bsd, [AC_DEFINE(HAVE_LOGIN)])
AC_CHECK_FUNCS(logout updwtmp logwtmp)

AC_FUNC_STRFTIME

# Check for ALTDIRFUNC glob() extension
AC_MSG_CHECKING(for GLOB_ALTDIRFUNC support)
AC_EGREP_CPP(FOUNDIT,
	[
		#include <glob.h>
		#ifdef GLOB_ALTDIRFUNC
		FOUNDIT
		#endif
	],
	[
		AC_DEFINE(GLOB_HAS_ALTDIRFUNC)
		AC_MSG_RESULT(yes)
	],
	[
		AC_MSG_RESULT(no)
	]
)

# Check for g.gl_matchc glob() extension
AC_MSG_CHECKING(for gl_matchc field in glob_t)
AC_EGREP_CPP(FOUNDIT,
	[
		#include <glob.h>
		int main(void){glob_t g; g.gl_matchc = 1;}
	],
	[
		AC_DEFINE(GLOB_HAS_GL_MATCHC)
		AC_MSG_RESULT(yes)
	],
	[
		AC_MSG_RESULT(no)
	]
)

AC_MSG_CHECKING([whether struct dirent allocates space for d_name])
AC_RUN_IFELSE(
	[AC_LANG_SOURCE([[
#include <sys/types.h>
#include <dirent.h>
int main(void){struct dirent d;exit(sizeof(d.d_name)<=sizeof(char));}
	]])],
	[AC_MSG_RESULT(yes)],
	[
		AC_MSG_RESULT(no)
		AC_DEFINE(BROKEN_ONE_BYTE_DIRENT_D_NAME)
	],
	[
		AC_MSG_WARN([cross compiling: assuming BROKEN_ONE_BYTE_DIRENT_D_NAME])
		AC_DEFINE(BROKEN_ONE_BYTE_DIRENT_D_NAME)
	]
)

AC_MSG_CHECKING([for /proc/pid/fd directory])
if test -d "/proc/$$/fd" ; then
	AC_DEFINE(HAVE_PROC_PID)
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

# Check whether user wants S/Key support
SKEY_MSG="no"
AC_ARG_WITH(skey,
	[  --with-skey[[=PATH]]      Enable S/Key support (optionally in PATH)],
	[
		if test "x$withval" != "xno" ; then

			if test "x$withval" != "xyes" ; then
				CPPFLAGS="$CPPFLAGS -I${withval}/include"
				LDFLAGS="$LDFLAGS -L${withval}/lib"
			fi

			AC_DEFINE(SKEY)
			LIBS="-lskey $LIBS"
			SKEY_MSG="yes"

			AC_MSG_CHECKING([for s/key support])
			AC_TRY_RUN(
				[
#include <stdio.h>
#include <skey.h>
int main() { char *ff = skey_keyinfo(""); ff=""; exit(0); }
				],
				[AC_MSG_RESULT(yes)],
				[
					AC_MSG_RESULT(no)
					AC_MSG_ERROR([** Incomplete or missing s/key libraries.])
				])
                 	AC_MSG_CHECKING(if skeychallenge takes 4 arguments)
			AC_TRY_COMPILE(
				[#include <stdio.h>
				 #include <skey.h>],
				[(void)skeychallenge(NULL,"name","",0);],
				[AC_MSG_RESULT(yes)
				 AC_DEFINE(SKEYCHALLENGE_4ARG)],
				[AC_MSG_RESULT(no)]
        		)
		fi
	]
)

# Check whether user wants TCP wrappers support
TCPW_MSG="no"
AC_ARG_WITH(tcp-wrappers,
	[  --with-tcp-wrappers[[=PATH]] Enable tcpwrappers support (optionally in PATH)],
	[
		if test "x$withval" != "xno" ; then
			saved_LIBS="$LIBS"
			saved_LDFLAGS="$LDFLAGS"
			saved_CPPFLAGS="$CPPFLAGS"
			if test -n "${withval}" && \
			    test "x${withval}" != "xyes"; then
				if test -d "${withval}/lib"; then
					if test -n "${need_dash_r}"; then
						LDFLAGS="-L${withval}/lib -R${withval}/lib ${LDFLAGS}"
					else
						LDFLAGS="-L${withval}/lib ${LDFLAGS}"
					fi
				else
					if test -n "${need_dash_r}"; then
						LDFLAGS="-L${withval} -R${withval} ${LDFLAGS}"
					else
						LDFLAGS="-L${withval} ${LDFLAGS}"
					fi
				fi
				if test -d "${withval}/include"; then
					CPPFLAGS="-I${withval}/include ${CPPFLAGS}"
				else
					CPPFLAGS="-I${withval} ${CPPFLAGS}"
				fi
			fi
			LIBWRAP="-lwrap"
			LIBS="$LIBWRAP $LIBS"
			AC_MSG_CHECKING(for libwrap)
			AC_TRY_LINK(
				[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <tcpd.h>
					int deny_severity = 0, allow_severity = 0;
				],
				[hosts_access(0);],
				[
					AC_MSG_RESULT(yes)
					AC_DEFINE(LIBWRAP)
					AC_SUBST(LIBWRAP)
					TCPW_MSG="yes"
				],
				[
					AC_MSG_ERROR([*** libwrap missing])
				]
			)
			LIBS="$saved_LIBS"
		fi
	]
)

# Check whether user wants libedit support
LIBEDIT_MSG="no"
AC_ARG_WITH(libedit,
	[  --with-libedit[[=PATH]]   Enable libedit support for sftp],
	[ if test "x$withval" != "xno" ; then
		if test "x$withval" != "xyes"; then
			CPPFLAGS="$CPPFLAGS -I$withval/include"
			LDFLAGS="$LDFLAGS -L$withval/lib"
		fi
		AC_CHECK_LIB(edit, el_init,
			[ AC_DEFINE(USE_LIBEDIT, [], [Use libedit for sftp])
			  LIBEDIT="-ledit -lcurses"
			  LIBEDIT_MSG="yes"
			  AC_SUBST(LIBEDIT)
			],
			[ AC_MSG_ERROR(libedit not found) ],
			[ -lcurses ]
		)
		AC_MSG_CHECKING(if libedit version is compatible)
		AC_COMPILE_IFELSE(
		    [AC_LANG_SOURCE([[
#include <histedit.h>
int main(void)
{
	int i = H_SETSIZE;
	el_init("", NULL, NULL, NULL);
	exit(0);
}
		    ]])],
		    [ AC_MSG_RESULT(yes) ],
		    [ AC_MSG_RESULT(no)
		      AC_MSG_ERROR(libedit version is not compatible) ]
		)
	fi ]
)

AUDIT_MODULE=none
AC_ARG_WITH(audit,
	[  --with-audit=module     Enable EXPERIMENTAL audit support (modules=debug,bsm)],
	[
	  AC_MSG_CHECKING(for supported audit module)
	  case "$withval" in
	  bsm)
		AC_MSG_RESULT(bsm)
		AUDIT_MODULE=bsm
		dnl    Checks for headers, libs and functions
		AC_CHECK_HEADERS(bsm/audit.h, [],
		    [AC_MSG_ERROR(BSM enabled and bsm/audit.h not found)])
		AC_CHECK_LIB(bsm, getaudit, [],
		    [AC_MSG_ERROR(BSM enabled and required library not found)])
		AC_CHECK_FUNCS(getaudit, [],
		    [AC_MSG_ERROR(BSM enabled and required function not found)])
		# These are optional
		AC_CHECK_FUNCS(getaudit_addr)
		AC_DEFINE(USE_BSM_AUDIT, [], [Use BSM audit module])
		;;
	  debug)
		AUDIT_MODULE=debug
		AC_MSG_RESULT(debug)
		AC_DEFINE(SSH_AUDIT_EVENTS, [], Use audit debugging module)
		;;
	  no)
		AC_MSG_RESULT(no)
		;;
	  *)
		AC_MSG_ERROR([Unknown audit module $withval])
		;;
	esac ]
)

dnl    Checks for library functions. Please keep in alphabetical order
AC_CHECK_FUNCS( \
	arc4random \
	b64_ntop \
	__b64_ntop \
	b64_pton \
	__b64_pton \
	bcopy \
	bindresvport_sa \
	clock \
	closefrom \
	dirfd \
	fchmod \
	fchown \
	freeaddrinfo \
	futimes \
	getaddrinfo \
	getcwd \
	getgrouplist \
	getnameinfo \
	getopt \
	getpeereid \
	_getpty \
	getrlimit \
	getttyent \
	glob \
	inet_aton \
	inet_ntoa \
	inet_ntop \
	innetgr \
	login_getcapbool \
	md5_crypt \
	memmove \
	mkdtemp \
	mmap \
	ngetaddrinfo \
	nsleep \
	ogetaddrinfo \
	openlog_r \
	openpty \
	prctl \
	pstat \
	readpassphrase \
	realpath \
	recvmsg \
	rresvport_af \
	sendmsg \
	setdtablesize \
	setegid \
	setenv \
	seteuid \
	setgroups \
	setlogin \
	setpcred \
	setproctitle \
	setregid \
	setreuid \
	setrlimit \
	setsid \
	setvbuf \
	sigaction \
	sigvec \
	snprintf \
	socketpair \
	strdup \
	strerror \
	strlcat \
	strlcpy \
	strmode \
	strnvis \
	strtonum \
	strtoll \
	strtoul \
	sysconf \
	tcgetpgrp \
	truncate \
	unsetenv \
	updwtmpx \
	utimes \
	vhangup \
	vsnprintf \
	waitpid \
)

# IRIX has a const char return value for gai_strerror()
AC_CHECK_FUNCS(gai_strerror,[
	AC_DEFINE(HAVE_GAI_STRERROR)
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

const char *gai_strerror(int);],[
char *str;

str = gai_strerror(0);],[
		AC_DEFINE(HAVE_CONST_GAI_STRERROR_PROTO, 1,
		[Define if gai_strerror() returns const char *])])])

AC_SEARCH_LIBS(nanosleep, rt posix4, AC_DEFINE(HAVE_NANOSLEEP))

dnl Make sure prototypes are defined for these before using them.
AC_CHECK_DECL(getrusage, [AC_CHECK_FUNCS(getrusage)])
AC_CHECK_DECL(strsep,
	[AC_CHECK_FUNCS(strsep)],
	[],
	[
#ifdef HAVE_STRING_H
# include <string.h>
#endif
	])

dnl tcsendbreak might be a macro
AC_CHECK_DECL(tcsendbreak,
	[AC_DEFINE(HAVE_TCSENDBREAK)],
	[AC_CHECK_FUNCS(tcsendbreak)],
	[#include <termios.h>]
)

AC_CHECK_DECLS(h_errno, , ,[#include <netdb.h>])

AC_CHECK_FUNCS(setresuid, [
	dnl Some platorms have setresuid that isn't implemented, test for this
	AC_MSG_CHECKING(if setresuid seems to work)
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdlib.h>
#include <errno.h>
int main(){errno=0; setresuid(0,0,0); if (errno==ENOSYS) exit(1); else exit(0);}
		]])],
		[AC_MSG_RESULT(yes)],
		[AC_DEFINE(BROKEN_SETRESUID)
		 AC_MSG_RESULT(not implemented)],
		[AC_MSG_WARN([cross compiling: not checking setresuid])]
	)
])

AC_CHECK_FUNCS(setresgid, [
	dnl Some platorms have setresgid that isn't implemented, test for this
	AC_MSG_CHECKING(if setresgid seems to work)
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdlib.h>
#include <errno.h>
int main(){errno=0; setresgid(0,0,0); if (errno==ENOSYS) exit(1); else exit(0);}
		]])],
		[AC_MSG_RESULT(yes)],
		[AC_DEFINE(BROKEN_SETRESGID)
		 AC_MSG_RESULT(not implemented)],
		[AC_MSG_WARN([cross compiling: not checking setresuid])]
	)
])

dnl    Checks for time functions
AC_CHECK_FUNCS(gettimeofday time)
dnl    Checks for utmp functions
AC_CHECK_FUNCS(endutent getutent getutid getutline pututline setutent)
AC_CHECK_FUNCS(utmpname)
dnl    Checks for utmpx functions
AC_CHECK_FUNCS(endutxent getutxent getutxid getutxline pututxline )
AC_CHECK_FUNCS(setutxent utmpxname)

AC_CHECK_FUNC(daemon,
	[AC_DEFINE(HAVE_DAEMON)],
	[AC_CHECK_LIB(bsd, daemon, [LIBS="$LIBS -lbsd"; AC_DEFINE(HAVE_DAEMON)])]
)

AC_CHECK_FUNC(getpagesize,
	[AC_DEFINE(HAVE_GETPAGESIZE)],
	[AC_CHECK_LIB(ucb, getpagesize, [LIBS="$LIBS -lucb"; AC_DEFINE(HAVE_GETPAGESIZE)])]
)

# Check for broken snprintf
if test "x$ac_cv_func_snprintf" = "xyes" ; then
	AC_MSG_CHECKING([whether snprintf correctly terminates long strings])
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdio.h>
int main(void){char b[5];snprintf(b,5,"123456789");exit(b[4]!='\0');}
		]])],
		[AC_MSG_RESULT(yes)],
		[
			AC_MSG_RESULT(no)
			AC_DEFINE(BROKEN_SNPRINTF)
			AC_MSG_WARN([****** Your snprintf() function is broken, complain to your vendor])
		],
		[ AC_MSG_WARN([cross compiling: Assuming working snprintf()]) ]
	)
fi

# Check for missing getpeereid (or equiv) support
NO_PEERCHECK=""
if test "x$ac_cv_func_getpeereid" != "xyes" ; then
	AC_MSG_CHECKING([whether system supports SO_PEERCRED getsockopt])
	AC_TRY_COMPILE(
		[#include <sys/types.h>
		 #include <sys/socket.h>],
		[int i = SO_PEERCRED;],
		[ AC_MSG_RESULT(yes)
		  AC_DEFINE(HAVE_SO_PEERCRED, [], [Have PEERCRED socket option])
		],
		[AC_MSG_RESULT(no)
		NO_PEERCHECK=1]
        )
fi

dnl see whether mkstemp() requires XXXXXX
if test "x$ac_cv_func_mkdtemp" = "xyes" ; then
AC_MSG_CHECKING([for (overly) strict mkstemp])
AC_TRY_RUN(
	[
#include <stdlib.h>
main() { char template[]="conftest.mkstemp-test";
if (mkstemp(template) == -1)
	exit(1);
unlink(template); exit(0);
}
	],
	[
		AC_MSG_RESULT(no)
	],
	[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_STRICT_MKSTEMP)
	],
	[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_STRICT_MKSTEMP)
	]
)
fi

dnl make sure that openpty does not reacquire controlling terminal
if test ! -z "$check_for_openpty_ctty_bug"; then
	AC_MSG_CHECKING(if openpty correctly handles controlling tty)
	AC_TRY_RUN(
		[
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int
main()
{
	pid_t pid;
	int fd, ptyfd, ttyfd, status;

	pid = fork();
	if (pid < 0) {		/* failed */
		exit(1);
	} else if (pid > 0) {	/* parent */
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			exit(WEXITSTATUS(status));
		else
			exit(2);
	} else {		/* child */
		close(0); close(1); close(2);
		setsid();
		openpty(&ptyfd, &ttyfd, NULL, NULL, NULL);
		fd = open("/dev/tty", O_RDWR | O_NOCTTY);
		if (fd >= 0)
			exit(3);	/* Acquired ctty: broken */
		else
			exit(0);	/* Did not acquire ctty: OK */
	}
}
		],
		[
			AC_MSG_RESULT(yes)
		],
		[
			AC_MSG_RESULT(no)
			AC_DEFINE(SSHD_ACQUIRES_CTTY)
		]
	)
fi

if test "x$ac_cv_func_getaddrinfo" = "xyes" && \
    test "x$check_for_hpux_broken_getaddrinfo" = "x1"; then
	AC_MSG_CHECKING(if getaddrinfo seems to work)
	AC_TRY_RUN(
		[
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>

#define TEST_PORT "2222"

int
main(void)
{
	int err, sock;
	struct addrinfo *gai_ai, *ai, hints;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV], *name = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(name, TEST_PORT, &hints, &gai_ai);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo failed (%s)", gai_strerror(err));
		exit(1);
	}

	for (ai = gai_ai; ai != NULL; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET6)
			continue;

		err = getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop,
		    sizeof(ntop), strport, sizeof(strport),
		    NI_NUMERICHOST|NI_NUMERICSERV);

		if (err != 0) {
			if (err == EAI_SYSTEM)
				perror("getnameinfo EAI_SYSTEM");
			else
				fprintf(stderr, "getnameinfo failed: %s\n",
				    gai_strerror(err));
			exit(2);
		}

		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0)
			perror("socket");
		if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
			if (errno == EBADF)
				exit(3);
		}
	}
	exit(0);
}
		],
		[
			AC_MSG_RESULT(yes)
		],
		[
			AC_MSG_RESULT(no)
			AC_DEFINE(BROKEN_GETADDRINFO)
		]
	)
fi

if test "x$ac_cv_func_getaddrinfo" = "xyes" && \
    test "x$check_for_aix_broken_getaddrinfo" = "x1"; then
	AC_MSG_CHECKING(if getaddrinfo seems to work)
	AC_TRY_RUN(
		[
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/in.h>

#define TEST_PORT "2222"

int
main(void)
{
	int err, sock;
	struct addrinfo *gai_ai, *ai, hints;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV], *name = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	err = getaddrinfo(name, TEST_PORT, &hints, &gai_ai);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo failed (%s)", gai_strerror(err));
		exit(1);
	}

	for (ai = gai_ai; ai != NULL; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;

		err = getnameinfo(ai->ai_addr, ai->ai_addrlen, ntop,
		    sizeof(ntop), strport, sizeof(strport),
		    NI_NUMERICHOST|NI_NUMERICSERV);

		if (ai->ai_family == AF_INET && err != 0) {
			perror("getnameinfo");
			exit(2);
		}
	}
	exit(0);
}
		],
		[
			AC_MSG_RESULT(yes)
			AC_DEFINE(AIX_GETNAMEINFO_HACK, [],
[Define if you have a getaddrinfo that fails for the all-zeros IPv6 address])
		],
		[
			AC_MSG_RESULT(no)
			AC_DEFINE(BROKEN_GETADDRINFO)
		]
	)
fi

if test "x$check_for_conflicting_getspnam" = "x1"; then
	AC_MSG_CHECKING(for conflicting getspnam in shadow.h)
	AC_COMPILE_IFELSE(
		[
#include <shadow.h>
int main(void) {exit(0);}
		],
		[
			AC_MSG_RESULT(no)
		],
		[
			AC_MSG_RESULT(yes)
			AC_DEFINE(GETSPNAM_CONFLICTING_DEFS, 1,
			    [Conflicting defs for getspnam])
		]
	)
fi

AC_FUNC_GETPGRP

# Check for PAM libs
PAM_MSG="no"
AC_ARG_WITH(pam,
	[  --with-pam              Enable PAM support ],
	[
		if test "x$withval" != "xno" ; then
			if test "x$ac_cv_header_security_pam_appl_h" != "xyes" && \
			   test "x$ac_cv_header_pam_pam_appl_h" != "xyes" ; then
				AC_MSG_ERROR([PAM headers not found])
			fi

			AC_CHECK_LIB(dl, dlopen, , )
			AC_CHECK_LIB(pam, pam_set_item, , AC_MSG_ERROR([*** libpam missing]))
			AC_CHECK_FUNCS(pam_getenvlist)
			AC_CHECK_FUNCS(pam_putenv)

			PAM_MSG="yes"

			AC_DEFINE(USE_PAM)
			if test $ac_cv_lib_dl_dlopen = yes; then
				LIBPAM="-lpam -ldl"
			else
				LIBPAM="-lpam"
			fi
			AC_SUBST(LIBPAM)
		fi
	]
)

# Check for older PAM
if test "x$PAM_MSG" = "xyes" ; then
	# Check PAM strerror arguments (old PAM)
	AC_MSG_CHECKING([whether pam_strerror takes only one argument])
	AC_TRY_COMPILE(
		[
#include <stdlib.h>
#if defined(HAVE_SECURITY_PAM_APPL_H)
#include <security/pam_appl.h>
#elif defined (HAVE_PAM_PAM_APPL_H)
#include <pam/pam_appl.h>
#endif
		],
		[(void)pam_strerror((pam_handle_t *)NULL, -1);],
		[AC_MSG_RESULT(no)],
		[
			AC_DEFINE(HAVE_OLD_PAM)
			AC_MSG_RESULT(yes)
			PAM_MSG="yes (old library)"
		]
	)
fi

# Search for OpenSSL
saved_CPPFLAGS="$CPPFLAGS"
saved_LDFLAGS="$LDFLAGS"
AC_ARG_WITH(ssl-dir,
	[  --with-ssl-dir=PATH     Specify path to OpenSSL installation ],
	[
		if test "x$withval" != "xno" ; then
			case "$withval" in
				# Relative paths
				./*|../*)	withval="`pwd`/$withval"
			esac
			if test -d "$withval/lib"; then
				if test -n "${need_dash_r}"; then
					LDFLAGS="-L${withval}/lib -R${withval}/lib ${LDFLAGS}"
				else
					LDFLAGS="-L${withval}/lib ${LDFLAGS}"
				fi
			else
				if test -n "${need_dash_r}"; then
					LDFLAGS="-L${withval} -R${withval} ${LDFLAGS}"
				else
					LDFLAGS="-L${withval} ${LDFLAGS}"
				fi
			fi
			if test -d "$withval/include"; then
				CPPFLAGS="-I${withval}/include ${CPPFLAGS}"
			else
				CPPFLAGS="-I${withval} ${CPPFLAGS}"
			fi
		fi
	]
)
LIBS="-lcrypto $LIBS"
AC_TRY_LINK_FUNC(RAND_add, AC_DEFINE(HAVE_OPENSSL),
	[
		dnl Check default openssl install dir
		if test -n "${need_dash_r}"; then
			LDFLAGS="-L/usr/local/ssl/lib -R/usr/local/ssl/lib ${saved_LDFLAGS}"
		else
			LDFLAGS="-L/usr/local/ssl/lib ${saved_LDFLAGS}"
		fi
		CPPFLAGS="-I/usr/local/ssl/include ${saved_CPPFLAGS}"
		AC_TRY_LINK_FUNC(RAND_add, AC_DEFINE(HAVE_OPENSSL),
			[
				AC_MSG_ERROR([*** Can't find recent OpenSSL libcrypto (see config.log for details) ***])
			]
		)
	]
)

# Determine OpenSSL header version
AC_MSG_CHECKING([OpenSSL header version])
AC_RUN_IFELSE(
	[AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
#include <openssl/opensslv.h>
#define DATA "conftest.sslincver"
int main(void) {
	FILE *fd;
	int rc;

	fd = fopen(DATA,"w");
	if(fd == NULL)
		exit(1);

	if ((rc = fprintf(fd ,"%x (%s)\n", OPENSSL_VERSION_NUMBER, OPENSSL_VERSION_TEXT)) <0)
		exit(1);

	exit(0);
}
	]])],
	[
		ssl_header_ver=`cat conftest.sslincver`
		AC_MSG_RESULT($ssl_header_ver)
	],
	[
		AC_MSG_RESULT(not found)
		AC_MSG_ERROR(OpenSSL version header not found.)
	],
	[
		AC_MSG_WARN([cross compiling: not checking])
	]
)

# Determine OpenSSL library version
AC_MSG_CHECKING([OpenSSL library version])
AC_RUN_IFELSE(
	[AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#define DATA "conftest.ssllibver"
int main(void) {
	FILE *fd;
	int rc;

	fd = fopen(DATA,"w");
	if(fd == NULL)
		exit(1);

	if ((rc = fprintf(fd ,"%x (%s)\n", SSLeay(), SSLeay_version(SSLEAY_VERSION))) <0)
		exit(1);

	exit(0);
}
	]])],
	[
		ssl_library_ver=`cat conftest.ssllibver`
		AC_MSG_RESULT($ssl_library_ver)
	],
	[
		AC_MSG_RESULT(not found)
		AC_MSG_ERROR(OpenSSL library not found.)
	],
	[
		AC_MSG_WARN([cross compiling: not checking])
	]
)

# Sanity check OpenSSL headers
AC_MSG_CHECKING([whether OpenSSL's headers match the library])
AC_RUN_IFELSE(
	[AC_LANG_SOURCE([[
#include <string.h>
#include <openssl/opensslv.h>
int main(void) { exit(SSLeay() == OPENSSL_VERSION_NUMBER ? 0 : 1); }
	]])],
	[
		AC_MSG_RESULT(yes)
	],
	[
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([Your OpenSSL headers do not match your library.
Check config.log for details.
Also see contrib/findssl.sh for help identifying header/library mismatches.])
	],
	[
		AC_MSG_WARN([cross compiling: not checking])
	]
)

# Some systems want crypt() from libcrypt, *not* the version in OpenSSL,
# because the system crypt() is more featureful.
if test "x$check_for_libcrypt_before" = "x1"; then
	AC_CHECK_LIB(crypt, crypt)
fi

# Some Linux systems (Slackware) need crypt() from libcrypt, *not* the
# version in OpenSSL.
if test "x$check_for_libcrypt_later" = "x1"; then
	AC_CHECK_LIB(crypt, crypt, LIBS="$LIBS -lcrypt")
fi

AC_CHECK_LIB(iaf, ia_openinfo)

### Configure cryptographic random number support

# Check wheter OpenSSL seeds itself
AC_MSG_CHECKING([whether OpenSSL's PRNG is internally seeded])
AC_RUN_IFELSE(
	[AC_LANG_SOURCE([[
#include <string.h>
#include <openssl/rand.h>
int main(void) { exit(RAND_status() == 1 ? 0 : 1); }
	]])],
	[
		OPENSSL_SEEDS_ITSELF=yes
		AC_MSG_RESULT(yes)
	],
	[
		AC_MSG_RESULT(no)
		# Default to use of the rand helper if OpenSSL doesn't
		# seed itself
		USE_RAND_HELPER=yes
	],
	[
		AC_MSG_WARN([cross compiling: assuming yes])
		# This is safe, since all recent OpenSSL versions will
		# complain at runtime if not seeded correctly.
		OPENSSL_SEEDS_ITSELF=yes
	]
)


# Do we want to force the use of the rand helper?
AC_ARG_WITH(rand-helper,
	[  --with-rand-helper      Use subprocess to gather strong randomness ],
	[
		if test "x$withval" = "xno" ; then
			# Force use of OpenSSL's internal RNG, even if
			# the previous test showed it to be unseeded.
			if test -z "$OPENSSL_SEEDS_ITSELF" ; then
				AC_MSG_WARN([*** Forcing use of OpenSSL's non-self-seeding PRNG])
				OPENSSL_SEEDS_ITSELF=yes
				USE_RAND_HELPER=""
			fi
		else
			USE_RAND_HELPER=yes
		fi
	],
)

# Which randomness source do we use?
if test ! -z "$OPENSSL_SEEDS_ITSELF" && test -z "$USE_RAND_HELPER" ; then
	# OpenSSL only
	AC_DEFINE(OPENSSL_PRNG_ONLY)
	RAND_MSG="OpenSSL internal ONLY"
	INSTALL_SSH_RAND_HELPER=""
elif test ! -z "$USE_RAND_HELPER" ; then
	# install rand helper
	RAND_MSG="ssh-rand-helper"
	INSTALL_SSH_RAND_HELPER="yes"
fi
AC_SUBST(INSTALL_SSH_RAND_HELPER)

### Configuration of ssh-rand-helper

# PRNGD TCP socket
AC_ARG_WITH(prngd-port,
	[  --with-prngd-port=PORT  read entropy from PRNGD/EGD TCP localhost:PORT],
	[
		case "$withval" in
		no)
			withval=""
			;;
		[[0-9]]*)
			;;
		*)
			AC_MSG_ERROR(You must specify a numeric port number for --with-prngd-port)
			;;
		esac
		if test ! -z "$withval" ; then
			PRNGD_PORT="$withval"
			AC_DEFINE_UNQUOTED(PRNGD_PORT, $PRNGD_PORT)
		fi
	]
)

# PRNGD Unix domain socket
AC_ARG_WITH(prngd-socket,
	[  --with-prngd-socket=FILE read entropy from PRNGD/EGD socket FILE (default=/var/run/egd-pool)],
	[
		case "$withval" in
		yes)
			withval="/var/run/egd-pool"
			;;
		no)
			withval=""
			;;
		/*)
			;;
		*)
			AC_MSG_ERROR(You must specify an absolute path to the entropy socket)
			;;
		esac

		if test ! -z "$withval" ; then
			if test ! -z "$PRNGD_PORT" ; then
				AC_MSG_ERROR(You may not specify both a PRNGD/EGD port and socket)
			fi
			if test ! -r "$withval" ; then
				AC_MSG_WARN(Entropy socket is not readable)
			fi
			PRNGD_SOCKET="$withval"
			AC_DEFINE_UNQUOTED(PRNGD_SOCKET, "$PRNGD_SOCKET")
		fi
	],
	[
		# Check for existing socket only if we don't have a random device already
		if test "$USE_RAND_HELPER" = yes ; then
			AC_MSG_CHECKING(for PRNGD/EGD socket)
			# Insert other locations here
			for sock in /var/run/egd-pool /dev/egd-pool /etc/entropy; do
				if test -r $sock && $TEST_MINUS_S_SH -c "test -S $sock -o -p $sock" ; then
					PRNGD_SOCKET="$sock"
					AC_DEFINE_UNQUOTED(PRNGD_SOCKET, "$PRNGD_SOCKET")
					break;
				fi
			done
			if test ! -z "$PRNGD_SOCKET" ; then
				AC_MSG_RESULT($PRNGD_SOCKET)
			else
				AC_MSG_RESULT(not found)
			fi
		fi
	]
)

# Change default command timeout for hashing entropy source
entropy_timeout=200
AC_ARG_WITH(entropy-timeout,
	[  --with-entropy-timeout  Specify entropy gathering command timeout (msec)],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			entropy_timeout=$withval
		fi
	]
)
AC_DEFINE_UNQUOTED(ENTROPY_TIMEOUT_MSEC, $entropy_timeout)

SSH_PRIVSEP_USER=sshd
AC_ARG_WITH(privsep-user,
	[  --with-privsep-user=user Specify non-privileged user for privilege separation],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			SSH_PRIVSEP_USER=$withval
		fi
	]
)
AC_DEFINE_UNQUOTED(SSH_PRIVSEP_USER, "$SSH_PRIVSEP_USER")
AC_SUBST(SSH_PRIVSEP_USER)

# We do this little dance with the search path to insure
# that programs that we select for use by installed programs
# (which may be run by the super-user) come from trusted
# locations before they come from the user's private area.
# This should help avoid accidentally configuring some
# random version of a program in someone's personal bin.

OPATH=$PATH
PATH=/bin:/usr/bin
test -h /bin 2> /dev/null && PATH=/usr/bin
test -d /sbin && PATH=$PATH:/sbin
test -d /usr/sbin && PATH=$PATH:/usr/sbin
PATH=$PATH:/etc:$OPATH

# These programs are used by the command hashing source to gather entropy
OSSH_PATH_ENTROPY_PROG(PROG_LS, ls)
OSSH_PATH_ENTROPY_PROG(PROG_NETSTAT, netstat)
OSSH_PATH_ENTROPY_PROG(PROG_ARP, arp)
OSSH_PATH_ENTROPY_PROG(PROG_IFCONFIG, ifconfig)
OSSH_PATH_ENTROPY_PROG(PROG_JSTAT, jstat)
OSSH_PATH_ENTROPY_PROG(PROG_PS, ps)
OSSH_PATH_ENTROPY_PROG(PROG_SAR, sar)
OSSH_PATH_ENTROPY_PROG(PROG_W, w)
OSSH_PATH_ENTROPY_PROG(PROG_WHO, who)
OSSH_PATH_ENTROPY_PROG(PROG_LAST, last)
OSSH_PATH_ENTROPY_PROG(PROG_LASTLOG, lastlog)
OSSH_PATH_ENTROPY_PROG(PROG_DF, df)
OSSH_PATH_ENTROPY_PROG(PROG_VMSTAT, vmstat)
OSSH_PATH_ENTROPY_PROG(PROG_UPTIME, uptime)
OSSH_PATH_ENTROPY_PROG(PROG_IPCS, ipcs)
OSSH_PATH_ENTROPY_PROG(PROG_TAIL, tail)
# restore PATH
PATH=$OPATH

# Where does ssh-rand-helper get its randomness from?
INSTALL_SSH_PRNG_CMDS=""
if test ! -z "$INSTALL_SSH_RAND_HELPER" ; then
	if test ! -z "$PRNGD_PORT" ; then
		RAND_HELPER_MSG="TCP localhost:$PRNGD_PORT"
	elif test ! -z "$PRNGD_SOCKET" ; then
		RAND_HELPER_MSG="Unix domain socket \"$PRNGD_SOCKET\""
	else
		RAND_HELPER_MSG="Command hashing (timeout $entropy_timeout)"
		RAND_HELPER_CMDHASH=yes
		INSTALL_SSH_PRNG_CMDS="yes"
	fi
fi
AC_SUBST(INSTALL_SSH_PRNG_CMDS)


# Cheap hack to ensure NEWS-OS libraries are arranged right.
if test ! -z "$SONY" ; then
  LIBS="$LIBS -liberty";
fi

# Checks for data types
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(short int, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long int, 4)
AC_CHECK_SIZEOF(long long int, 8)

# Sanity check long long for some platforms (AIX)
if test "x$ac_cv_sizeof_long_long_int" = "x4" ; then
	ac_cv_sizeof_long_long_int=0
fi

# More checks for data types
AC_CACHE_CHECK([for u_int type], ac_cv_have_u_int, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ u_int a; a = 1;],
		[ ac_cv_have_u_int="yes" ],
		[ ac_cv_have_u_int="no" ]
	)
])
if test "x$ac_cv_have_u_int" = "xyes" ; then
	AC_DEFINE(HAVE_U_INT)
	have_u_int=1
fi

AC_CACHE_CHECK([for intXX_t types], ac_cv_have_intxx_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ int8_t a; int16_t b; int32_t c; a = b = c = 1;],
		[ ac_cv_have_intxx_t="yes" ],
		[ ac_cv_have_intxx_t="no" ]
	)
])
if test "x$ac_cv_have_intxx_t" = "xyes" ; then
	AC_DEFINE(HAVE_INTXX_T)
	have_intxx_t=1
fi

if (test -z "$have_intxx_t" && \
	   test "x$ac_cv_header_stdint_h" = "xyes")
then
    AC_MSG_CHECKING([for intXX_t types in stdint.h])
	AC_TRY_COMPILE(
		[ #include <stdint.h> ],
		[ int8_t a; int16_t b; int32_t c; a = b = c = 1;],
		[
			AC_DEFINE(HAVE_INTXX_T)
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

AC_CACHE_CHECK([for int64_t type], ac_cv_have_int64_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <sys/socket.h>
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif
		],
		[ int64_t a; a = 1;],
		[ ac_cv_have_int64_t="yes" ],
		[ ac_cv_have_int64_t="no" ]
	)
])
if test "x$ac_cv_have_int64_t" = "xyes" ; then
	AC_DEFINE(HAVE_INT64_T)
fi

AC_CACHE_CHECK([for u_intXX_t types], ac_cv_have_u_intxx_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ u_int8_t a; u_int16_t b; u_int32_t c; a = b = c = 1;],
		[ ac_cv_have_u_intxx_t="yes" ],
		[ ac_cv_have_u_intxx_t="no" ]
	)
])
if test "x$ac_cv_have_u_intxx_t" = "xyes" ; then
	AC_DEFINE(HAVE_U_INTXX_T)
	have_u_intxx_t=1
fi

if test -z "$have_u_intxx_t" ; then
    AC_MSG_CHECKING([for u_intXX_t types in sys/socket.h])
	AC_TRY_COMPILE(
		[ #include <sys/socket.h> ],
		[ u_int8_t a; u_int16_t b; u_int32_t c; a = b = c = 1;],
		[
			AC_DEFINE(HAVE_U_INTXX_T)
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

AC_CACHE_CHECK([for u_int64_t types], ac_cv_have_u_int64_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ u_int64_t a; a = 1;],
		[ ac_cv_have_u_int64_t="yes" ],
		[ ac_cv_have_u_int64_t="no" ]
	)
])
if test "x$ac_cv_have_u_int64_t" = "xyes" ; then
	AC_DEFINE(HAVE_U_INT64_T)
	have_u_int64_t=1
fi

if test -z "$have_u_int64_t" ; then
    AC_MSG_CHECKING([for u_int64_t type in sys/bitypes.h])
	AC_TRY_COMPILE(
		[ #include <sys/bitypes.h> ],
		[ u_int64_t a; a = 1],
		[
			AC_DEFINE(HAVE_U_INT64_T)
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

if test -z "$have_u_intxx_t" ; then
	AC_CACHE_CHECK([for uintXX_t types], ac_cv_have_uintxx_t, [
		AC_TRY_COMPILE(
			[
#include <sys/types.h>
			],
			[ uint8_t a; uint16_t b; uint32_t c; a = b = c = 1; ],
			[ ac_cv_have_uintxx_t="yes" ],
			[ ac_cv_have_uintxx_t="no" ]
		)
	])
	if test "x$ac_cv_have_uintxx_t" = "xyes" ; then
		AC_DEFINE(HAVE_UINTXX_T)
	fi
fi

if test -z "$have_uintxx_t" ; then
    AC_MSG_CHECKING([for uintXX_t types in stdint.h])
	AC_TRY_COMPILE(
		[ #include <stdint.h> ],
		[ uint8_t a; uint16_t b; uint32_t c; a = b = c = 1;],
		[
			AC_DEFINE(HAVE_UINTXX_T)
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

if (test -z "$have_u_intxx_t" || test -z "$have_intxx_t" && \
	   test "x$ac_cv_header_sys_bitypes_h" = "xyes")
then
	AC_MSG_CHECKING([for intXX_t and u_intXX_t types in sys/bitypes.h])
	AC_TRY_COMPILE(
		[
#include <sys/bitypes.h>
		],
		[
			int8_t a; int16_t b; int32_t c;
			u_int8_t e; u_int16_t f; u_int32_t g;
			a = b = c = e = f = g = 1;
		],
		[
			AC_DEFINE(HAVE_U_INTXX_T)
			AC_DEFINE(HAVE_INTXX_T)
			AC_MSG_RESULT(yes)
		],
		[AC_MSG_RESULT(no)]
	)
fi


AC_CACHE_CHECK([for u_char], ac_cv_have_u_char, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ u_char foo; foo = 125; ],
		[ ac_cv_have_u_char="yes" ],
		[ ac_cv_have_u_char="no" ]
	)
])
if test "x$ac_cv_have_u_char" = "xyes" ; then
	AC_DEFINE(HAVE_U_CHAR)
fi

TYPE_SOCKLEN_T

AC_CHECK_TYPES(sig_atomic_t,,,[#include <signal.h>])

AC_CHECK_TYPES(in_addr_t,,,
[#include <sys/types.h>
#include <netinet/in.h>])

AC_CACHE_CHECK([for size_t], ac_cv_have_size_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ size_t foo; foo = 1235; ],
		[ ac_cv_have_size_t="yes" ],
		[ ac_cv_have_size_t="no" ]
	)
])
if test "x$ac_cv_have_size_t" = "xyes" ; then
	AC_DEFINE(HAVE_SIZE_T)
fi

AC_CACHE_CHECK([for ssize_t], ac_cv_have_ssize_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ ssize_t foo; foo = 1235; ],
		[ ac_cv_have_ssize_t="yes" ],
		[ ac_cv_have_ssize_t="no" ]
	)
])
if test "x$ac_cv_have_ssize_t" = "xyes" ; then
	AC_DEFINE(HAVE_SSIZE_T)
fi

AC_CACHE_CHECK([for clock_t], ac_cv_have_clock_t, [
	AC_TRY_COMPILE(
		[
#include <time.h>
		],
		[ clock_t foo; foo = 1235; ],
		[ ac_cv_have_clock_t="yes" ],
		[ ac_cv_have_clock_t="no" ]
	)
])
if test "x$ac_cv_have_clock_t" = "xyes" ; then
	AC_DEFINE(HAVE_CLOCK_T)
fi

AC_CACHE_CHECK([for sa_family_t], ac_cv_have_sa_family_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
		],
		[ sa_family_t foo; foo = 1235; ],
		[ ac_cv_have_sa_family_t="yes" ],
		[ AC_TRY_COMPILE(
		  [
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
		],
		[ sa_family_t foo; foo = 1235; ],
		[ ac_cv_have_sa_family_t="yes" ],

		[ ac_cv_have_sa_family_t="no" ]
	)]
	)
])
if test "x$ac_cv_have_sa_family_t" = "xyes" ; then
	AC_DEFINE(HAVE_SA_FAMILY_T)
fi

AC_CACHE_CHECK([for pid_t], ac_cv_have_pid_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ pid_t foo; foo = 1235; ],
		[ ac_cv_have_pid_t="yes" ],
		[ ac_cv_have_pid_t="no" ]
	)
])
if test "x$ac_cv_have_pid_t" = "xyes" ; then
	AC_DEFINE(HAVE_PID_T)
fi

AC_CACHE_CHECK([for mode_t], ac_cv_have_mode_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ mode_t foo; foo = 1235; ],
		[ ac_cv_have_mode_t="yes" ],
		[ ac_cv_have_mode_t="no" ]
	)
])
if test "x$ac_cv_have_mode_t" = "xyes" ; then
	AC_DEFINE(HAVE_MODE_T)
fi


AC_CACHE_CHECK([for struct sockaddr_storage], ac_cv_have_struct_sockaddr_storage, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
		],
		[ struct sockaddr_storage s; ],
		[ ac_cv_have_struct_sockaddr_storage="yes" ],
		[ ac_cv_have_struct_sockaddr_storage="no" ]
	)
])
if test "x$ac_cv_have_struct_sockaddr_storage" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_SOCKADDR_STORAGE)
fi

AC_CACHE_CHECK([for struct sockaddr_in6], ac_cv_have_struct_sockaddr_in6, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <netinet/in.h>
		],
		[ struct sockaddr_in6 s; s.sin6_family = 0; ],
		[ ac_cv_have_struct_sockaddr_in6="yes" ],
		[ ac_cv_have_struct_sockaddr_in6="no" ]
	)
])
if test "x$ac_cv_have_struct_sockaddr_in6" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_SOCKADDR_IN6)
fi

AC_CACHE_CHECK([for struct in6_addr], ac_cv_have_struct_in6_addr, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <netinet/in.h>
		],
		[ struct in6_addr s; s.s6_addr[0] = 0; ],
		[ ac_cv_have_struct_in6_addr="yes" ],
		[ ac_cv_have_struct_in6_addr="no" ]
	)
])
if test "x$ac_cv_have_struct_in6_addr" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_IN6_ADDR)
fi

AC_CACHE_CHECK([for struct addrinfo], ac_cv_have_struct_addrinfo, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
		],
		[ struct addrinfo s; s.ai_flags = AI_PASSIVE; ],
		[ ac_cv_have_struct_addrinfo="yes" ],
		[ ac_cv_have_struct_addrinfo="no" ]
	)
])
if test "x$ac_cv_have_struct_addrinfo" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_ADDRINFO)
fi

AC_CACHE_CHECK([for struct timeval], ac_cv_have_struct_timeval, [
	AC_TRY_COMPILE(
		[ #include <sys/time.h> ],
		[ struct timeval tv; tv.tv_sec = 1;],
		[ ac_cv_have_struct_timeval="yes" ],
		[ ac_cv_have_struct_timeval="no" ]
	)
])
if test "x$ac_cv_have_struct_timeval" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_TIMEVAL)
	have_struct_timeval=1
fi

AC_CHECK_TYPES(struct timespec)

# We need int64_t or else certian parts of the compile will fail.
if test "x$ac_cv_have_int64_t" = "xno" && \
	test "x$ac_cv_sizeof_long_int" != "x8" && \
	test "x$ac_cv_sizeof_long_long_int" = "x0" ; then
	echo "OpenSSH requires int64_t support.  Contact your vendor or install"
	echo "an alternative compiler (I.E., GCC) before continuing."
	echo ""
	exit 1;
else
dnl test snprintf (broken on SCO w/gcc)
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SNPRINTF
main()
{
	char buf[50];
	char expected_out[50];
	int mazsize = 50 ;
#if (SIZEOF_LONG_INT == 8)
	long int num = 0x7fffffffffffffff;
#else
	long long num = 0x7fffffffffffffffll;
#endif
	strcpy(expected_out, "9223372036854775807");
	snprintf(buf, mazsize, "%lld", num);
	if(strcmp(buf, expected_out) != 0)
		exit(1);
	exit(0);
}
#else
main() { exit(0); }
#endif
		]])], [ true ], [ AC_DEFINE(BROKEN_SNPRINTF) ],
		AC_MSG_WARN([cross compiling: Assuming working snprintf()])
	)
fi

dnl Checks for structure members
OSSH_CHECK_HEADER_FOR_FIELD(ut_host, utmp.h, HAVE_HOST_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_host, utmpx.h, HAVE_HOST_IN_UTMPX)
OSSH_CHECK_HEADER_FOR_FIELD(syslen, utmpx.h, HAVE_SYSLEN_IN_UTMPX)
OSSH_CHECK_HEADER_FOR_FIELD(ut_pid, utmp.h, HAVE_PID_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_type, utmp.h, HAVE_TYPE_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_type, utmpx.h, HAVE_TYPE_IN_UTMPX)
OSSH_CHECK_HEADER_FOR_FIELD(ut_tv, utmp.h, HAVE_TV_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_id, utmp.h, HAVE_ID_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_id, utmpx.h, HAVE_ID_IN_UTMPX)
OSSH_CHECK_HEADER_FOR_FIELD(ut_addr, utmp.h, HAVE_ADDR_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_addr, utmpx.h, HAVE_ADDR_IN_UTMPX)
OSSH_CHECK_HEADER_FOR_FIELD(ut_addr_v6, utmp.h, HAVE_ADDR_V6_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_addr_v6, utmpx.h, HAVE_ADDR_V6_IN_UTMPX)
OSSH_CHECK_HEADER_FOR_FIELD(ut_exit, utmp.h, HAVE_EXIT_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_time, utmp.h, HAVE_TIME_IN_UTMP)
OSSH_CHECK_HEADER_FOR_FIELD(ut_time, utmpx.h, HAVE_TIME_IN_UTMPX)
OSSH_CHECK_HEADER_FOR_FIELD(ut_tv, utmpx.h, HAVE_TV_IN_UTMPX)

AC_CHECK_MEMBERS([struct stat.st_blksize])

AC_CACHE_CHECK([for ss_family field in struct sockaddr_storage],
		ac_cv_have_ss_family_in_struct_ss, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
		],
		[ struct sockaddr_storage s; s.ss_family = 1; ],
		[ ac_cv_have_ss_family_in_struct_ss="yes" ],
		[ ac_cv_have_ss_family_in_struct_ss="no" ],
	)
])
if test "x$ac_cv_have_ss_family_in_struct_ss" = "xyes" ; then
	AC_DEFINE(HAVE_SS_FAMILY_IN_SS)
fi

AC_CACHE_CHECK([for __ss_family field in struct sockaddr_storage],
		ac_cv_have___ss_family_in_struct_ss, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
		],
		[ struct sockaddr_storage s; s.__ss_family = 1; ],
		[ ac_cv_have___ss_family_in_struct_ss="yes" ],
		[ ac_cv_have___ss_family_in_struct_ss="no" ]
	)
])
if test "x$ac_cv_have___ss_family_in_struct_ss" = "xyes" ; then
	AC_DEFINE(HAVE___SS_FAMILY_IN_SS)
fi

AC_CACHE_CHECK([for pw_class field in struct passwd],
		ac_cv_have_pw_class_in_struct_passwd, [
	AC_TRY_COMPILE(
		[
#include <pwd.h>
		],
		[ struct passwd p; p.pw_class = 0; ],
		[ ac_cv_have_pw_class_in_struct_passwd="yes" ],
		[ ac_cv_have_pw_class_in_struct_passwd="no" ]
	)
])
if test "x$ac_cv_have_pw_class_in_struct_passwd" = "xyes" ; then
	AC_DEFINE(HAVE_PW_CLASS_IN_PASSWD)
fi

AC_CACHE_CHECK([for pw_expire field in struct passwd],
		ac_cv_have_pw_expire_in_struct_passwd, [
	AC_TRY_COMPILE(
		[
#include <pwd.h>
		],
		[ struct passwd p; p.pw_expire = 0; ],
		[ ac_cv_have_pw_expire_in_struct_passwd="yes" ],
		[ ac_cv_have_pw_expire_in_struct_passwd="no" ]
	)
])
if test "x$ac_cv_have_pw_expire_in_struct_passwd" = "xyes" ; then
	AC_DEFINE(HAVE_PW_EXPIRE_IN_PASSWD)
fi

AC_CACHE_CHECK([for pw_change field in struct passwd],
		ac_cv_have_pw_change_in_struct_passwd, [
	AC_TRY_COMPILE(
		[
#include <pwd.h>
		],
		[ struct passwd p; p.pw_change = 0; ],
		[ ac_cv_have_pw_change_in_struct_passwd="yes" ],
		[ ac_cv_have_pw_change_in_struct_passwd="no" ]
	)
])
if test "x$ac_cv_have_pw_change_in_struct_passwd" = "xyes" ; then
	AC_DEFINE(HAVE_PW_CHANGE_IN_PASSWD)
fi

dnl make sure we're using the real structure members and not defines
AC_CACHE_CHECK([for msg_accrights field in struct msghdr],
		ac_cv_have_accrights_in_msghdr, [
	AC_COMPILE_IFELSE(
		[
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
int main() {
#ifdef msg_accrights
#error "msg_accrights is a macro"
exit(1);
#endif
struct msghdr m;
m.msg_accrights = 0;
exit(0);
}
		],
		[ ac_cv_have_accrights_in_msghdr="yes" ],
		[ ac_cv_have_accrights_in_msghdr="no" ]
	)
])
if test "x$ac_cv_have_accrights_in_msghdr" = "xyes" ; then
	AC_DEFINE(HAVE_ACCRIGHTS_IN_MSGHDR)
fi

AC_CACHE_CHECK([for msg_control field in struct msghdr],
		ac_cv_have_control_in_msghdr, [
	AC_COMPILE_IFELSE(
		[
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
int main() {
#ifdef msg_control
#error "msg_control is a macro"
exit(1);
#endif
struct msghdr m;
m.msg_control = 0;
exit(0);
}
		],
		[ ac_cv_have_control_in_msghdr="yes" ],
		[ ac_cv_have_control_in_msghdr="no" ]
	)
])
if test "x$ac_cv_have_control_in_msghdr" = "xyes" ; then
	AC_DEFINE(HAVE_CONTROL_IN_MSGHDR)
fi

AC_CACHE_CHECK([if libc defines __progname], ac_cv_libc_defines___progname, [
	AC_TRY_LINK([],
		[ extern char *__progname; printf("%s", __progname); ],
		[ ac_cv_libc_defines___progname="yes" ],
		[ ac_cv_libc_defines___progname="no" ]
	)
])
if test "x$ac_cv_libc_defines___progname" = "xyes" ; then
	AC_DEFINE(HAVE___PROGNAME)
fi

AC_CACHE_CHECK([whether $CC implements __FUNCTION__], ac_cv_cc_implements___FUNCTION__, [
	AC_TRY_LINK([
#include <stdio.h>
],
		[ printf("%s", __FUNCTION__); ],
		[ ac_cv_cc_implements___FUNCTION__="yes" ],
		[ ac_cv_cc_implements___FUNCTION__="no" ]
	)
])
if test "x$ac_cv_cc_implements___FUNCTION__" = "xyes" ; then
	AC_DEFINE(HAVE___FUNCTION__)
fi

AC_CACHE_CHECK([whether $CC implements __func__], ac_cv_cc_implements___func__, [
	AC_TRY_LINK([
#include <stdio.h>
],
		[ printf("%s", __func__); ],
		[ ac_cv_cc_implements___func__="yes" ],
		[ ac_cv_cc_implements___func__="no" ]
	)
])
if test "x$ac_cv_cc_implements___func__" = "xyes" ; then
	AC_DEFINE(HAVE___func__)
fi

AC_CACHE_CHECK([whether getopt has optreset support],
		ac_cv_have_getopt_optreset, [
	AC_TRY_LINK(
		[
#include <getopt.h>
		],
		[ extern int optreset; optreset = 0; ],
		[ ac_cv_have_getopt_optreset="yes" ],
		[ ac_cv_have_getopt_optreset="no" ]
	)
])
if test "x$ac_cv_have_getopt_optreset" = "xyes" ; then
	AC_DEFINE(HAVE_GETOPT_OPTRESET)
fi

AC_CACHE_CHECK([if libc defines sys_errlist], ac_cv_libc_defines_sys_errlist, [
	AC_TRY_LINK([],
		[ extern const char *const sys_errlist[]; printf("%s", sys_errlist[0]);],
		[ ac_cv_libc_defines_sys_errlist="yes" ],
		[ ac_cv_libc_defines_sys_errlist="no" ]
	)
])
if test "x$ac_cv_libc_defines_sys_errlist" = "xyes" ; then
	AC_DEFINE(HAVE_SYS_ERRLIST)
fi


AC_CACHE_CHECK([if libc defines sys_nerr], ac_cv_libc_defines_sys_nerr, [
	AC_TRY_LINK([],
		[ extern int sys_nerr; printf("%i", sys_nerr);],
		[ ac_cv_libc_defines_sys_nerr="yes" ],
		[ ac_cv_libc_defines_sys_nerr="no" ]
	)
])
if test "x$ac_cv_libc_defines_sys_nerr" = "xyes" ; then
	AC_DEFINE(HAVE_SYS_NERR)
fi

SCARD_MSG="no"
# Check whether user wants sectok support
AC_ARG_WITH(sectok,
	[  --with-sectok           Enable smartcard support using libsectok],
	[
		if test "x$withval" != "xno" ; then
			if test "x$withval" != "xyes" ; then
				CPPFLAGS="$CPPFLAGS -I${withval}"
				LDFLAGS="$LDFLAGS -L${withval}"
				if test ! -z "$need_dash_r" ; then
					LDFLAGS="$LDFLAGS -R${withval}"
				fi
				if test ! -z "$blibpath" ; then
					blibpath="$blibpath:${withval}"
				fi
			fi
			AC_CHECK_HEADERS(sectok.h)
			if test "$ac_cv_header_sectok_h" != yes; then
				AC_MSG_ERROR(Can't find sectok.h)
			fi
			AC_CHECK_LIB(sectok, sectok_open)
			if test "$ac_cv_lib_sectok_sectok_open" != yes; then
				AC_MSG_ERROR(Can't find libsectok)
			fi
			AC_DEFINE(SMARTCARD)
			AC_DEFINE(USE_SECTOK)
			SCARD_MSG="yes, using sectok"
		fi
	]
)

# Check whether user wants OpenSC support
OPENSC_CONFIG="no"
AC_ARG_WITH(opensc,
	[--with-opensc[[=PFX]]       Enable smartcard support using OpenSC (optionally in PATH)],
	[
	    if test "x$withval" != "xno" ; then
		if test "x$withval" != "xyes" ; then
  			OPENSC_CONFIG=$withval/bin/opensc-config
		else
  			AC_PATH_PROG(OPENSC_CONFIG, opensc-config, no)
		fi
		if test "$OPENSC_CONFIG" != "no"; then
			LIBOPENSC_CFLAGS=`$OPENSC_CONFIG --cflags`
			LIBOPENSC_LIBS=`$OPENSC_CONFIG --libs`
			CPPFLAGS="$CPPFLAGS $LIBOPENSC_CFLAGS"
			LDFLAGS="$LDFLAGS $LIBOPENSC_LIBS"
			AC_DEFINE(SMARTCARD)
			AC_DEFINE(USE_OPENSC)
			SCARD_MSG="yes, using OpenSC"
		fi
	    fi
	]
)

# Check libraries needed by DNS fingerprint support
AC_SEARCH_LIBS(getrrsetbyname, resolv,
	[AC_DEFINE(HAVE_GETRRSETBYNAME)],
	[
		# Needed by our getrrsetbyname()
		AC_SEARCH_LIBS(res_query, resolv)
		AC_SEARCH_LIBS(dn_expand, resolv)
		AC_MSG_CHECKING(if res_query will link)
		AC_TRY_LINK_FUNC(res_query, AC_MSG_RESULT(yes),
		   [AC_MSG_RESULT(no)
		    saved_LIBS="$LIBS"
		    LIBS="$LIBS -lresolv"
		    AC_MSG_CHECKING(for res_query in -lresolv)
		    AC_LINK_IFELSE([
#include <resolv.h>
int main()
{
	res_query (0, 0, 0, 0, 0);
	return 0;
}
			],
			[LIBS="$LIBS -lresolv"
			 AC_MSG_RESULT(yes)],
			[LIBS="$saved_LIBS"
			 AC_MSG_RESULT(no)])
		    ])
		AC_CHECK_FUNCS(_getshort _getlong)
		AC_CHECK_DECLS([_getshort, _getlong], , ,
		    [#include <sys/types.h>
		    #include <arpa/nameser.h>])
		AC_CHECK_MEMBER(HEADER.ad,
			[AC_DEFINE(HAVE_HEADER_AD)],,
			[#include <arpa/nameser.h>])
	])

# Check whether user wants Kerberos 5 support
KRB5_MSG="no"
AC_ARG_WITH(kerberos5,
	[  --with-kerberos5=PATH   Enable Kerberos 5 support],
	[ if test "x$withval" != "xno" ; then
		if test "x$withval" = "xyes" ; then
			KRB5ROOT="/usr/local"
		else
			KRB5ROOT=${withval}
		fi

		AC_DEFINE(KRB5)
		KRB5_MSG="yes"

		AC_MSG_CHECKING(for krb5-config)
		if test -x  $KRB5ROOT/bin/krb5-config ; then
			KRB5CONF=$KRB5ROOT/bin/krb5-config
			AC_MSG_RESULT($KRB5CONF)

			AC_MSG_CHECKING(for gssapi support)
			if $KRB5CONF | grep gssapi >/dev/null ; then
				AC_MSG_RESULT(yes)
				AC_DEFINE(GSSAPI)
				k5confopts=gssapi
			else
				AC_MSG_RESULT(no)
				k5confopts=""
			fi
			K5CFLAGS="`$KRB5CONF --cflags $k5confopts`"
			K5LIBS="`$KRB5CONF --libs $k5confopts`"
			CPPFLAGS="$CPPFLAGS $K5CFLAGS"
			AC_MSG_CHECKING(whether we are using Heimdal)
			AC_TRY_COMPILE([ #include <krb5.h> ],
				       [ char *tmp = heimdal_version; ],
				       [ AC_MSG_RESULT(yes)
					 AC_DEFINE(HEIMDAL) ],
				         AC_MSG_RESULT(no)
			)
		else
			AC_MSG_RESULT(no)
			CPPFLAGS="$CPPFLAGS -I${KRB5ROOT}/include"
			LDFLAGS="$LDFLAGS -L${KRB5ROOT}/lib"
			AC_MSG_CHECKING(whether we are using Heimdal)
			AC_TRY_COMPILE([ #include <krb5.h> ],
				       [ char *tmp = heimdal_version; ],
				       [ AC_MSG_RESULT(yes)
					 AC_DEFINE(HEIMDAL)
					 K5LIBS="-lkrb5 -ldes"
					 K5LIBS="$K5LIBS -lcom_err -lasn1"
					 AC_CHECK_LIB(roken, net_write,
					   [K5LIBS="$K5LIBS -lroken"])
				       ],
				       [ AC_MSG_RESULT(no)
					 K5LIBS="-lkrb5 -lk5crypto -lcom_err"
				       ]
			)
			AC_SEARCH_LIBS(dn_expand, resolv)

			AC_CHECK_LIB(gssapi,gss_init_sec_context,
				[ AC_DEFINE(GSSAPI)
				  K5LIBS="-lgssapi $K5LIBS" ],
				[ AC_CHECK_LIB(gssapi_krb5,gss_init_sec_context,
					[ AC_DEFINE(GSSAPI)
					  K5LIBS="-lgssapi_krb5 $K5LIBS" ],
					AC_MSG_WARN([Cannot find any suitable gss-api library - build may fail]),
					$K5LIBS)
				],
				$K5LIBS)

			AC_CHECK_HEADER(gssapi.h, ,
				[ unset ac_cv_header_gssapi_h
				  CPPFLAGS="$CPPFLAGS -I${KRB5ROOT}/include/gssapi"
				  AC_CHECK_HEADERS(gssapi.h, ,
					AC_MSG_WARN([Cannot find any suitable gss-api header - build may fail])
				  )
				]
			)

			oldCPP="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS -I${KRB5ROOT}/include/gssapi"
			AC_CHECK_HEADER(gssapi_krb5.h, ,
					[ CPPFLAGS="$oldCPP" ])

		fi
		if test ! -z "$need_dash_r" ; then
			LDFLAGS="$LDFLAGS -R${KRB5ROOT}/lib"
		fi
		if test ! -z "$blibpath" ; then
			blibpath="$blibpath:${KRB5ROOT}/lib"
		fi
	fi

	AC_CHECK_HEADERS(gssapi.h gssapi/gssapi.h)
	AC_CHECK_HEADERS(gssapi_krb5.h gssapi/gssapi_krb5.h)
	AC_CHECK_HEADERS(gssapi_generic.h gssapi/gssapi_generic.h)

	LIBS="$LIBS $K5LIBS"
	AC_SEARCH_LIBS(k_hasafs, kafs, AC_DEFINE(USE_AFS))
	]
)

# Looking for programs, paths and files

PRIVSEP_PATH=/var/empty
AC_ARG_WITH(privsep-path,
	[  --with-privsep-path=xxx Path for privilege separation chroot (default=/var/empty)],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			PRIVSEP_PATH=$withval
		fi
	]
)
AC_SUBST(PRIVSEP_PATH)

AC_ARG_WITH(xauth,
	[  --with-xauth=PATH       Specify path to xauth program ],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			xauth_path=$withval
		fi
	],
	[
		TestPath="$PATH"
		TestPath="${TestPath}${PATH_SEPARATOR}/usr/X/bin"
		TestPath="${TestPath}${PATH_SEPARATOR}/usr/bin/X11"
		TestPath="${TestPath}${PATH_SEPARATOR}/usr/X11R6/bin"
		TestPath="${TestPath}${PATH_SEPARATOR}/usr/openwin/bin"
		AC_PATH_PROG(xauth_path, xauth, , $TestPath)
		if (test ! -z "$xauth_path" && test -x "/usr/openwin/bin/xauth") ; then
			xauth_path="/usr/openwin/bin/xauth"
		fi
	]
)

STRIP_OPT=-s
AC_ARG_ENABLE(strip,
	[  --disable-strip         Disable calling strip(1) on install],
	[
		if test "x$enableval" = "xno" ; then
			STRIP_OPT=
		fi
	]
)
AC_SUBST(STRIP_OPT)

if test -z "$xauth_path" ; then
	XAUTH_PATH="undefined"
	AC_SUBST(XAUTH_PATH)
else
	AC_DEFINE_UNQUOTED(XAUTH_PATH, "$xauth_path")
	XAUTH_PATH=$xauth_path
	AC_SUBST(XAUTH_PATH)
fi

# Check for mail directory (last resort if we cannot get it from headers)
if test ! -z "$MAIL" ; then
	maildir=`dirname $MAIL`
	AC_DEFINE_UNQUOTED(MAIL_DIRECTORY, "$maildir")
fi

if test ! -z "$cross_compiling" && test "x$cross_compiling" = "xyes"; then
	AC_MSG_WARN([cross compiling: Disabling /dev/ptmx test])
	disable_ptmx_check=yes
fi
if test -z "$no_dev_ptmx" ; then
	if test "x$disable_ptmx_check" != "xyes" ; then
		AC_CHECK_FILE("/dev/ptmx",
			[
				AC_DEFINE_UNQUOTED(HAVE_DEV_PTMX)
				have_dev_ptmx=1
			]
		)
	fi
fi

if test ! -z "$cross_compiling" && test "x$cross_compiling" != "xyes"; then
	AC_CHECK_FILE("/dev/ptc",
		[
			AC_DEFINE_UNQUOTED(HAVE_DEV_PTS_AND_PTC)
			have_dev_ptc=1
		]
	)
else
	AC_MSG_WARN([cross compiling: Disabling /dev/ptc test])
fi

# Options from here on. Some of these are preset by platform above
AC_ARG_WITH(mantype,
	[  --with-mantype=man|cat|doc  Set man page type],
	[
		case "$withval" in
		man|cat|doc)
			MANTYPE=$withval
			;;
		*)
			AC_MSG_ERROR(invalid man type: $withval)
			;;
		esac
	]
)
if test -z "$MANTYPE"; then
	TestPath="/usr/bin${PATH_SEPARATOR}/usr/ucb"
	AC_PATH_PROGS(NROFF, nroff awf, /bin/false, $TestPath)
	if ${NROFF} -mdoc ${srcdir}/ssh.1 >/dev/null 2>&1; then
		MANTYPE=doc
	elif ${NROFF} -man ${srcdir}/ssh.1 >/dev/null 2>&1; then
		MANTYPE=man
	else
		MANTYPE=cat
	fi
fi
AC_SUBST(MANTYPE)
if test "$MANTYPE" = "doc"; then
	mansubdir=man;
else
	mansubdir=$MANTYPE;
fi
AC_SUBST(mansubdir)

# Check whether to enable MD5 passwords
MD5_MSG="no"
AC_ARG_WITH(md5-passwords,
	[  --with-md5-passwords    Enable use of MD5 passwords],
	[
		if test "x$withval" != "xno" ; then
			AC_DEFINE(HAVE_MD5_PASSWORDS)
			MD5_MSG="yes"
		fi
	]
)

# Whether to disable shadow password support
AC_ARG_WITH(shadow,
	[  --without-shadow        Disable shadow password support],
	[
		if test "x$withval" = "xno" ; then
			AC_DEFINE(DISABLE_SHADOW)
			disable_shadow=yes
		fi
	]
)

if test -z "$disable_shadow" ; then
	AC_MSG_CHECKING([if the systems has expire shadow information])
	AC_TRY_COMPILE(
	[
#include <sys/types.h>
#include <shadow.h>
	struct spwd sp;
	],[ sp.sp_expire = sp.sp_lstchg = sp.sp_inact = 0; ],
	[ sp_expire_available=yes ], []
	)

	if test "x$sp_expire_available" = "xyes" ; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAS_SHADOW_EXPIRE)
	else
		AC_MSG_RESULT(no)
	fi
fi

# Use ip address instead of hostname in $DISPLAY
if test ! -z "$IPADDR_IN_DISPLAY" ; then
	DISPLAY_HACK_MSG="yes"
	AC_DEFINE(IPADDR_IN_DISPLAY)
else
	DISPLAY_HACK_MSG="no"
	AC_ARG_WITH(ipaddr-display,
		[  --with-ipaddr-display   Use ip address instead of hostname in \$DISPLAY],
		[
			if test "x$withval" != "xno" ; then
				AC_DEFINE(IPADDR_IN_DISPLAY)
				DISPLAY_HACK_MSG="yes"
			fi
		]
	)
fi

# check for /etc/default/login and use it if present.
AC_ARG_ENABLE(etc-default-login,
	[  --disable-etc-default-login Disable using PATH from /etc/default/login [no]],
	[ if test "x$enableval" = "xno"; then
		AC_MSG_NOTICE([/etc/default/login handling disabled])
		etc_default_login=no
	  else
		etc_default_login=yes
	  fi ],
	[ etc_default_login=yes ]
)

if test "x$etc_default_login" != "xno"; then
	AC_CHECK_FILE("/etc/default/login",
	    [ external_path_file=/etc/default/login ])
	if test ! -z "$cross_compiling" && test "x$cross_compiling" = "xyes";
	then
		AC_MSG_WARN([cross compiling: Disabling /etc/default/login test])
	elif test "x$external_path_file" = "x/etc/default/login"; then
		AC_DEFINE(HAVE_ETC_DEFAULT_LOGIN)
	fi
fi

dnl BSD systems use /etc/login.conf so --with-default-path= has no effect
if test $ac_cv_func_login_getcapbool = "yes" && \
	test $ac_cv_header_login_cap_h = "yes" ; then
	external_path_file=/etc/login.conf
fi

# Whether to mess with the default path
SERVER_PATH_MSG="(default)"
AC_ARG_WITH(default-path,
	[  --with-default-path=    Specify default \$PATH environment for server],
	[
		if test "x$external_path_file" = "x/etc/login.conf" ; then
			AC_MSG_WARN([
--with-default-path=PATH has no effect on this system.
Edit /etc/login.conf instead.])
		elif test "x$withval" != "xno" ; then
			if test ! -z "$external_path_file" ; then
				AC_MSG_WARN([
--with-default-path=PATH will only be used if PATH is not defined in
$external_path_file .])
			fi
			user_path="$withval"
			SERVER_PATH_MSG="$withval"
		fi
	],
	[ if test "x$external_path_file" = "x/etc/login.conf" ; then
		AC_MSG_WARN([Make sure the path to scp is in /etc/login.conf])
	else
		if test ! -z "$external_path_file" ; then
			AC_MSG_WARN([
If PATH is defined in $external_path_file, ensure the path to scp is included,
otherwise scp will not work.])
		fi
		AC_TRY_RUN(
			[
/* find out what STDPATH is */
#include <stdio.h>
#ifdef HAVE_PATHS_H
# include <paths.h>
#endif
#ifndef _PATH_STDPATH
# ifdef _PATH_USERPATH	/* Irix */
#  define _PATH_STDPATH _PATH_USERPATH
# else
#  define _PATH_STDPATH "/usr/bin:/bin:/usr/sbin:/sbin"
# endif
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define DATA "conftest.stdpath"

main()
{
	FILE *fd;
	int rc;

	fd = fopen(DATA,"w");
	if(fd == NULL)
		exit(1);

	if ((rc = fprintf(fd,"%s", _PATH_STDPATH)) < 0)
		exit(1);

	exit(0);
}
		], [ user_path=`cat conftest.stdpath` ],
		[ user_path="/usr/bin:/bin:/usr/sbin:/sbin" ],
		[ user_path="/usr/bin:/bin:/usr/sbin:/sbin" ]
	)
# make sure $bindir is in USER_PATH so scp will work
		t_bindir=`eval echo ${bindir}`
		case $t_bindir in
			NONE/*) t_bindir=`echo $t_bindir | sed "s~NONE~$prefix~"` ;;
		esac
		case $t_bindir in
			NONE/*) t_bindir=`echo $t_bindir | sed "s~NONE~$ac_default_prefix~"` ;;
		esac
		echo $user_path | grep ":$t_bindir"  > /dev/null 2>&1
		if test $? -ne 0  ; then
			echo $user_path | grep "^$t_bindir"  > /dev/null 2>&1
			if test $? -ne 0  ; then
				user_path=$user_path:$t_bindir
				AC_MSG_RESULT(Adding $t_bindir to USER_PATH so scp will work)
			fi
		fi
	fi ]
)
if test "x$external_path_file" != "x/etc/login.conf" ; then
	AC_DEFINE_UNQUOTED(USER_PATH, "$user_path")
	AC_SUBST(user_path)
fi

# Set superuser path separately to user path
AC_ARG_WITH(superuser-path,
	[  --with-superuser-path=  Specify different path for super-user],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			AC_DEFINE_UNQUOTED(SUPERUSER_PATH, "$withval")
			superuser_path=$withval
		fi
	]
)


AC_MSG_CHECKING([if we need to convert IPv4 in IPv6-mapped addresses])
IPV4_IN6_HACK_MSG="no"
AC_ARG_WITH(4in6,
	[  --with-4in6             Check for and convert IPv4 in IPv6 mapped addresses],
	[
		if test "x$withval" != "xno" ; then
			AC_MSG_RESULT(yes)
			AC_DEFINE(IPV4_IN_IPV6)
			IPV4_IN6_HACK_MSG="yes"
		else
			AC_MSG_RESULT(no)
		fi
	],[
		if test "x$inet6_default_4in6" = "xyes"; then
			AC_MSG_RESULT([yes (default)])
			AC_DEFINE(IPV4_IN_IPV6)
			IPV4_IN6_HACK_MSG="yes"
		else
			AC_MSG_RESULT([no (default)])
		fi
	]
)

# Whether to enable BSD auth support
BSD_AUTH_MSG=no
AC_ARG_WITH(bsd-auth,
	[  --with-bsd-auth         Enable BSD auth support],
	[
		if test "x$withval" != "xno" ; then
			AC_DEFINE(BSD_AUTH)
			BSD_AUTH_MSG=yes
		fi
	]
)

# Where to place sshd.pid
piddir=/var/run
# make sure the directory exists
if test ! -d $piddir ; then
	piddir=`eval echo ${sysconfdir}`
	case $piddir in
		NONE/*) piddir=`echo $piddir | sed "s~NONE~$ac_default_prefix~"` ;;
	esac
fi

AC_ARG_WITH(pid-dir,
	[  --with-pid-dir=PATH     Specify location of ssh.pid file],
	[
		if test -n "$withval"  &&  test "x$withval" != "xno"  &&  \
		    test "x${withval}" != "xyes"; then
			piddir=$withval
			if test ! -d $piddir ; then
			AC_MSG_WARN([** no $piddir directory on this system **])
			fi
		fi
	]
)

AC_DEFINE_UNQUOTED(_PATH_SSH_PIDDIR, "$piddir")
AC_SUBST(piddir)

dnl allow user to disable some login recording features
AC_ARG_ENABLE(lastlog,
	[  --disable-lastlog       disable use of lastlog even if detected [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_LASTLOG)
		fi
	]
)
AC_ARG_ENABLE(utmp,
	[  --disable-utmp          disable use of utmp even if detected [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_UTMP)
		fi
	]
)
AC_ARG_ENABLE(utmpx,
	[  --disable-utmpx         disable use of utmpx even if detected [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_UTMPX)
		fi
	]
)
AC_ARG_ENABLE(wtmp,
	[  --disable-wtmp          disable use of wtmp even if detected [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_WTMP)
		fi
	]
)
AC_ARG_ENABLE(wtmpx,
	[  --disable-wtmpx         disable use of wtmpx even if detected [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_WTMPX)
		fi
	]
)
AC_ARG_ENABLE(libutil,
	[  --disable-libutil       disable use of libutil (login() etc.) [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_LOGIN)
		fi
	]
)
AC_ARG_ENABLE(pututline,
	[  --disable-pututline     disable use of pututline() etc. ([uw]tmp) [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_PUTUTLINE)
		fi
	]
)
AC_ARG_ENABLE(pututxline,
	[  --disable-pututxline    disable use of pututxline() etc. ([uw]tmpx) [no]],
	[
		if test "x$enableval" = "xno" ; then
			AC_DEFINE(DISABLE_PUTUTXLINE)
		fi
	]
)
AC_ARG_WITH(lastlog,
  [  --with-lastlog=FILE|DIR specify lastlog location [common locations]],
	[
		if test "x$withval" = "xno" ; then
			AC_DEFINE(DISABLE_LASTLOG)
		elif test -n "$withval"  &&  test "x${withval}" != "xyes"; then
			conf_lastlog_location=$withval
		fi
	]
)

dnl lastlog, [uw]tmpx? detection
dnl  NOTE: set the paths in the platform section to avoid the
dnl   need for command-line parameters
dnl lastlog and [uw]tmp are subject to a file search if all else fails

dnl lastlog detection
dnl  NOTE: the code itself will detect if lastlog is a directory
AC_MSG_CHECKING([if your system defines LASTLOG_FILE])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
#ifdef HAVE_LASTLOG_H
#  include <lastlog.h>
#endif
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif
#ifdef HAVE_LOGIN_H
# include <login.h>
#endif
	],
	[ char *lastlog = LASTLOG_FILE; ],
	[ AC_MSG_RESULT(yes) ],
	[
		AC_MSG_RESULT(no)
		AC_MSG_CHECKING([if your system defines _PATH_LASTLOG])
		AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
#ifdef HAVE_LASTLOG_H
#  include <lastlog.h>
#endif
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif
		],
		[ char *lastlog = _PATH_LASTLOG; ],
		[ AC_MSG_RESULT(yes) ],
		[
			AC_MSG_RESULT(no)
			system_lastlog_path=no
		])
	]
)

if test -z "$conf_lastlog_location"; then
	if test x"$system_lastlog_path" = x"no" ; then
		for f in /var/log/lastlog /usr/adm/lastlog /var/adm/lastlog /etc/security/lastlog ; do
				if (test -d "$f" || test -f "$f") ; then
					conf_lastlog_location=$f
				fi
		done
		if test -z "$conf_lastlog_location"; then
			AC_MSG_WARN([** Cannot find lastlog **])
			dnl Don't define DISABLE_LASTLOG - that means we don't try wtmp/wtmpx
		fi
	fi
fi

if test -n "$conf_lastlog_location"; then
	AC_DEFINE_UNQUOTED(CONF_LASTLOG_FILE, "$conf_lastlog_location")
fi

dnl utmp detection
AC_MSG_CHECKING([if your system defines UTMP_FILE])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif
	],
	[ char *utmp = UTMP_FILE; ],
	[ AC_MSG_RESULT(yes) ],
	[ AC_MSG_RESULT(no)
	  system_utmp_path=no ]
)
if test -z "$conf_utmp_location"; then
	if test x"$system_utmp_path" = x"no" ; then
		for f in /etc/utmp /usr/adm/utmp /var/run/utmp; do
			if test -f $f ; then
				conf_utmp_location=$f
			fi
		done
		if test -z "$conf_utmp_location"; then
			AC_DEFINE(DISABLE_UTMP)
		fi
	fi
fi
if test -n "$conf_utmp_location"; then
	AC_DEFINE_UNQUOTED(CONF_UTMP_FILE, "$conf_utmp_location")
fi

dnl wtmp detection
AC_MSG_CHECKING([if your system defines WTMP_FILE])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif
	],
	[ char *wtmp = WTMP_FILE; ],
	[ AC_MSG_RESULT(yes) ],
	[ AC_MSG_RESULT(no)
	  system_wtmp_path=no ]
)
if test -z "$conf_wtmp_location"; then
	if test x"$system_wtmp_path" = x"no" ; then
		for f in /usr/adm/wtmp /var/log/wtmp; do
			if test -f $f ; then
				conf_wtmp_location=$f
			fi
		done
		if test -z "$conf_wtmp_location"; then
			AC_DEFINE(DISABLE_WTMP)
		fi
	fi
fi
if test -n "$conf_wtmp_location"; then
	AC_DEFINE_UNQUOTED(CONF_WTMP_FILE, "$conf_wtmp_location")
fi


dnl utmpx detection - I don't know any system so perverse as to require
dnl  utmpx, but not define UTMPX_FILE (ditto wtmpx.) No doubt it's out
dnl  there, though.
AC_MSG_CHECKING([if your system defines UTMPX_FILE])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif
	],
	[ char *utmpx = UTMPX_FILE; ],
	[ AC_MSG_RESULT(yes) ],
	[ AC_MSG_RESULT(no)
	  system_utmpx_path=no ]
)
if test -z "$conf_utmpx_location"; then
	if test x"$system_utmpx_path" = x"no" ; then
		AC_DEFINE(DISABLE_UTMPX)
	fi
else
	AC_DEFINE_UNQUOTED(CONF_UTMPX_FILE, "$conf_utmpx_location")
fi

dnl wtmpx detection
AC_MSG_CHECKING([if your system defines WTMPX_FILE])
AC_TRY_COMPILE([
#include <sys/types.h>
#include <utmp.h>
#ifdef HAVE_UTMPX_H
#include <utmpx.h>
#endif
#ifdef HAVE_PATHS_H
#  include <paths.h>
#endif
	],
	[ char *wtmpx = WTMPX_FILE; ],
	[ AC_MSG_RESULT(yes) ],
	[ AC_MSG_RESULT(no)
	  system_wtmpx_path=no ]
)
if test -z "$conf_wtmpx_location"; then
	if test x"$system_wtmpx_path" = x"no" ; then
		AC_DEFINE(DISABLE_WTMPX)
	fi
else
	AC_DEFINE_UNQUOTED(CONF_WTMPX_FILE, "$conf_wtmpx_location")
fi


if test ! -z "$blibpath" ; then
	LDFLAGS="$LDFLAGS $blibflags$blibpath"
	AC_MSG_WARN([Please check and edit blibpath in LDFLAGS in Makefile])
fi

dnl remove pam and dl because they are in $LIBPAM
if test "$PAM_MSG" = yes ; then
	LIBS=`echo $LIBS | sed 's/-lpam //'`
fi
if test "$ac_cv_lib_pam_pam_set_item" = yes ; then
	LIBS=`echo $LIBS | sed 's/-ldl //'`
fi

dnl Adding -Werror to CFLAGS early prevents configure tests from running.
dnl Add now.
CFLAGS="$CFLAGS $werror_flags"

AC_EXEEXT
AC_CONFIG_FILES([Makefile buildpkg.sh opensshd.init openbsd-compat/Makefile \
	scard/Makefile ssh_prng_cmds survey.sh])
AC_OUTPUT

# Print summary of options

# Someone please show me a better way :)
A=`eval echo ${prefix}` ; A=`eval echo ${A}`
B=`eval echo ${bindir}` ; B=`eval echo ${B}`
C=`eval echo ${sbindir}` ; C=`eval echo ${C}`
D=`eval echo ${sysconfdir}` ; D=`eval echo ${D}`
E=`eval echo ${libexecdir}/ssh-askpass` ; E=`eval echo ${E}`
F=`eval echo ${mandir}/${mansubdir}X` ; F=`eval echo ${F}`
G=`eval echo ${piddir}` ; G=`eval echo ${G}`
H=`eval echo ${PRIVSEP_PATH}` ; H=`eval echo ${H}`
I=`eval echo ${user_path}` ; I=`eval echo ${I}`
J=`eval echo ${superuser_path}` ; J=`eval echo ${J}`

echo ""
echo "OpenSSH has been configured with the following options:"
echo "                     User binaries: $B"
echo "                   System binaries: $C"
echo "               Configuration files: $D"
echo "                   Askpass program: $E"
echo "                      Manual pages: $F"
echo "                          PID file: $G"
echo "  Privilege separation chroot path: $H"
if test "x$external_path_file" = "x/etc/login.conf" ; then
echo "   At runtime, sshd will use the path defined in $external_path_file"
echo "   Make sure the path to scp is present, otherwise scp will not work"
else
echo "            sshd default user PATH: $I"
	if test ! -z "$external_path_file"; then
echo "   (If PATH is set in $external_path_file it will be used instead. If"
echo "   used, ensure the path to scp is present, otherwise scp will not work.)"
	fi
fi
if test ! -z "$superuser_path" ; then
echo "          sshd superuser user PATH: $J"
fi
echo "                    Manpage format: $MANTYPE"
echo "                       PAM support: $PAM_MSG"
echo "                 KerberosV support: $KRB5_MSG"
echo "                 Smartcard support: $SCARD_MSG"
echo "                     S/KEY support: $SKEY_MSG"
echo "              TCP Wrappers support: $TCPW_MSG"
echo "              MD5 password support: $MD5_MSG"
echo "                   libedit support: $LIBEDIT_MSG"
echo "       IP address in \$DISPLAY hack: $DISPLAY_HACK_MSG"
echo "           Translate v4 in v6 hack: $IPV4_IN6_HACK_MSG"
echo "                  BSD Auth support: $BSD_AUTH_MSG"
echo "              Random number source: $RAND_MSG"
if test ! -z "$USE_RAND_HELPER" ; then
echo "     ssh-rand-helper collects from: $RAND_HELPER_MSG"
fi

echo ""

echo "              Host: ${host}"
echo "          Compiler: ${CC}"
echo "    Compiler flags: ${CFLAGS}"
echo "Preprocessor flags: ${CPPFLAGS}"
echo "      Linker flags: ${LDFLAGS}"
echo "         Libraries: ${LIBWRAP} ${LIBPAM} ${LIBS}"

echo ""

if test "x$MAKE_PACKAGE_SUPPORTED" = "xyes" ; then
	echo "SVR4 style packages are supported with \"make package\""
	echo ""
fi

if test "x$PAM_MSG" = "xyes" ; then
	echo "PAM is enabled. You may need to install a PAM control file "
	echo "for sshd, otherwise password authentication may fail. "
	echo "Example PAM control files can be found in the contrib/ "
	echo "subdirectory"
	echo ""
fi

if test ! -z "$RAND_HELPER_CMDHASH" ; then
	echo "WARNING: you are using the builtin random number collection "
	echo "service. Please read WARNING.RNG and request that your OS "
	echo "vendor includes kernel-based random number collection in "
	echo "future versions of your OS."
	echo ""
fi

if test ! -z "$NO_PEERCHECK" ; then
	echo "WARNING: the operating system that you are using does not "
	echo "appear to support either the getpeereid() API nor the "
	echo "SO_PEERCRED getsockopt() option. These facilities are used to "
	echo "enforce security checks to prevent unauthorised connections to "
	echo "ssh-agent. Their absence increases the risk that a malicious "
	echo "user can connect to your agent. "
	echo ""
fi

if test "$AUDIT_MODULE" = "bsm" ; then
	echo "WARNING: BSM audit support is currently considered EXPERIMENTAL."
	echo "See the Solaris section in README.platform for details."
fi
