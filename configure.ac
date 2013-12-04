dnl -*-fundamental-*-
dnl Process this file with autoconf to produce a configure script.
m4_include([version.m4])
AC_PREREQ([2.61])
AC_INIT([ntp], [VERSION_NUMBER])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([.])

# Increment ntp_configure_cache_version by one for each change to
# configure.ac or .m4 files which invalidates cached values from
# previous versions.
#
# If the change affects cache variables used only by the main NTP
# configure.ac, then only its version number should be bumped, while
# the subdir configure.ac version numbers should be unchanged.  The
# same is true for a test/variable that is used only by one subdir
# being changed incompatibly; only that subdir's cache version needs
# bumping.
#
# If a change affects variables shared by all NTP configure scripts,
# please bump the version numbers of all three.  If you are not sure,
# the safe choice is to bump all three on any cache-invalidating change.
#
# In order to avoid the risk of version stamp collision between -stable
# and -dev branches, do not simply increment the version, instead use
# the date YYYYMMDD optionally with -HHMM if there is more than one
# bump in a day.

ntp_configure_cache_version=20091117

# When the cache version of config.cache and configure do not
# match, NTP_CACHEVERSION will flush the cache.

NTP_CACHEVERSION([main], [$ntp_configure_cache_version])

AM_INIT_AUTOMAKE
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
dnl the 'build' machine is where we run configure and compile
dnl the 'host' machine is where the resulting stuff runs.
AC_DEFINE_UNQUOTED([STR_SYSTEM], ["$host"],
    [canonical system (cpu-vendor-os) of where we should run])
AM_CONFIG_HEADER([config.h])
dnl AC_ARG_PROGRAM

ntp_atom_ok=${ntp_atom_ok=no}
ntp_oncore_ok=${ntp_oncore_ok=no}
ntp_parse_ok=${ntp_parse_ok=no}
ntp_ripe_ncc_ok=${ntp_parse_ok=no}
ntp_jupiter_ok=${ntp_jupiter_ok=no}

dnl check these early to avoid autoconf warnings
AC_AIX
AC_MINIX

# So far, the only shared library we might use is libopts.
# It's a small library - we might as well use a static version of it.
AC_DISABLE_SHARED

dnl  we need to check for cross compile tools for vxWorks here
AC_PROG_CC
# Ralf Wildenhues: With per-target flags we need CC_C_O
# AM_PROG_CC_C_O supersets AC_PROG_CC_C_O
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_YACC

# AC_PROG_CC_STDC has two functions.  It attempts to find a compiler
# capable of C99, or failing that, for C89.  CC is set afterward with
# the selected invocation, such as "gcc --std=gnu99".  Also, the
# ac_cv_prog_cc_stdc variable is no if the compiler selected for CC
# does not accept C89.

AC_PROG_CC_STDC

case "$ac_cv_prog_cc_stdc" in
 no)
    AC_MSG_WARN([ANSI C89/ISO C90 is the minimum to compile NTP ]
		[version 4.2.5 and higher.])
esac

# HMS: These need to be moved to AM_CPPFLAGS and/or AM_CFLAGS
case "$host" in
 *-pc-cygwin*)
    CFLAGS="$CFLAGS -DSYS_CYGWIN32"
    ;;
 i386-sequent-sysv4)
    case "$CC" in
     cc)
	CFLAGS="$CFLAGS -Wc,+abi-socket"
	;;
    esac
    ;;
 *-*-mpeix*)
    CPPFLAGS="$CPPFLAGS -DMPE -D_POSIX_SOURCE -D_SOCKET_SOURCE -I/SYSLOG/PUB"
    LDFLAGS="$LDFLAGS -L/SYSLOG/PUB"
    LIBS="$LIBS -lcurses"
    ;;
 *-*-solaris*)
    # see "man standards".
    # -D_XOPEN_SOURCE=500 is probably OK for c89 and before
    # -D_XOPEN_SOURCE=600 seems OK for c99
    #CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=500 -D__EXTENSIONS__"
    CPPFLAGS="$CPPFLAGS -D__EXTENSIONS__"
    libxnet=-lxnet
    ;;
esac

NTP_OS_CFLAGS
NTP_DIR_SEP
NTP_VPATH_HACK

# NTP has (so far) been relying on leading-edge autogen.
# Therefore, by default:
# - use the version we ship with
# - do not install it
# - build a static copy (AC_DISABLE_SHARED - done earlier)
case "${enable_local_libopts+set}" in
 set) ;;
 *) enable_local_libopts=yes ;;
esac
case "${enable_libopts_install+set}" in
 set) ;;
 *) enable_libopts_install=no ;;
esac
LIBOPTS_CHECK_NOBUILD([sntp/libopts])

AC_FUNC_FORK

AC_CACHE_CHECK(
    [if $CC can handle @%:@warning],
    [ntp_cv_cpp_warning],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([[]], [[#warning foo]])],
	[ntp_cv_cpp_warning=yes],
	[ntp_cv_cpp_warning=no]
    )]
)
case "$ntp_cv_cpp_warning" in
 no)
    AC_DEFINE([NO_OPTION_NAME_WARNINGS], [1],
	[Should we avoid @%:@warning on option name collisions?])
esac

case "$GCC" in
 yes)
    SAVED_CFLAGS_NTP="$CFLAGS"
    CFLAGS="$CFLAGS -Wstrict-overflow"
    AC_CACHE_CHECK(
	[if $CC can handle -Wstrict-overflow], 
	[ntp_cv_gcc_Wstrict_overflow], 
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM([[]], [[]])],
	    [ntp_cv_gcc_Wstrict_overflow=yes],
	    [ntp_cv_gcc_Wstrict_overflow=no]
	)	]
    )
    #
    # $ntp_cv_gcc_Wstrict_overflow is tested later to add the 
    # flag to CFLAGS.
    #
    CFLAGS="$SAVED_CFLAGS_NTP -Winit-self"
    AC_CACHE_CHECK(
	[if $CC can handle -Winit-self], 
	[ntp_cv_gcc_Winit_self],
	[
	    AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([[]], [[]])],
		[ntp_cv_gcc_Winit_self=yes],
		[ntp_cv_gcc_Winit_self=no]
	    )
	]
    )
    CFLAGS="$SAVED_CFLAGS_NTP"
    AS_UNSET([SAVED_CFLAGS_NTP])
    #
    # $ntp_cv_gcc_Winit_self is tested later to add the 
    # flag to CFLAGS.
    #
esac

# Expose a cross-compilation indicator to makefiles
AM_CONDITIONAL([NTP_CROSSCOMPILE], [test $build != $host])

AC_MSG_CHECKING([for bin subdirectory])
AC_ARG_WITH(
    [binsubdir],
    [AS_HELP_STRING(
	[--with-binsubdir],
	[bin ={bin,sbin}]
    )],
    [use_binsubdir="$withval"],
    [use_binsubdir="bin"]
)
case "$use_binsubdir" in
 bin)
    ;;
 sbin)
    ;;
 *)
    AC_MSG_ERROR([<$use_binsubdir> is illegal - must be "bin" or "sbin"])
    ;;
esac
AC_MSG_RESULT([$use_binsubdir])

BINSUBDIR=$use_binsubdir
AC_SUBST([BINSUBDIR])
AM_CONDITIONAL([NTP_BINSUBDIR_IS_BIN], [test "bin" = "$BINSUBDIR"])

AC_MSG_CHECKING([for deprecated --with-arlib])
AC_ARG_WITH([arlib],
	AS_HELP_STRING([--with-arlib], [- deprecated, arlib not distributed]),
	[ans=$withval], [ans=no])
AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    AC_MSG_WARN([Please do not use --with-arlib, arlib is no longer included.  In the future, --with-arlib will not be recognized.])
esac

AC_ARG_WITH(rpath,
	AS_HELP_STRING([--without-rpath], [s Disable auto-added -R linker paths]),
[ans=$withval], [ans=x])
case "$ans" in
 no)
    need_dash_r=
    ;;
 yes)
    need_dash_r=1
    ;;
esac
# HMS: Why isn't this $build?
# Well, that depends on if we need this for the build toolchain or
# for info in the host executable...
# I still have no idea which way this should go, but nobody has complained.
case "$host" in
 *-*-netbsd*)
    case "$need_dash_r" in
     no) ;;
     *)  need_dash_r=1
	 ;;
    esac
    ;;
 *-*-solaris*)
    case "$need_dash_r" in
     no) ;;
     *)  need_dash_r=1
	 ;;
    esac
    ;;
esac

dnl  we need to check for cross compile tools for vxWorks here
AC_PROG_AWK
AC_PROG_MAKE_SET

rm -f conftest*

case "$GCC" in
 yes)
    CFLAGS="$CFLAGS -Wall"
    # CFLAGS="$CFLAGS -Wcast-align"
    CFLAGS="$CFLAGS -Wcast-qual"
    # CFLAGS="$CFLAGS -Wconversion"
    # CFLAGS="$CFLAGS -Werror"
    # CFLAGS="$CFLAGS -Wextra"
    # CFLAGS="$CFLAGS -Wfloat-equal"
    CFLAGS="$CFLAGS -Wmissing-prototypes"
    CFLAGS="$CFLAGS -Wpointer-arith"
    CFLAGS="$CFLAGS -Wshadow"
    # CFLAGS="$CFLAGS -Wtraditional"
    # CFLAGS="$CFLAGS -Wwrite-strings"
    case "$ntp_cv_gcc_Winit_self" in
     yes)
	CFLAGS="$CFLAGS -Winit-self"
    esac
    case "$ntp_cv_gcc_Wstrict_overflow" in
     yes)
	CFLAGS="$CFLAGS -Wstrict-overflow"
    esac
    # -W[no-]strict-prototypes is added later depending on OpenSSL
esac

ac_busted_vpath_in_make=no

case "$build" in
 *-*-irix6.1*)	# 64 bit only
    # busted vpath?
    ;;
 *-*-irix6*)	# 6.2 (and later?)
    ac_busted_vpath_in_make=yes
    ;;
 *-*-solaris2.5.1)
    ac_busted_vpath_in_make=yes
    ;;
 *-*-unicosmp*)
    ac_busted_vpath_in_make=yes
    ;;
esac

case "$ac_busted_vpath_in_make$srcdir" in
 no*) ;;
 yes.) ;;
 *) case "`${MAKE-make} -v -f /dev/null 2>/dev/null | sed -e 's/GNU Make version \(1-9.]*\).*/\1/' -e q`" in
     '')
	AC_MSG_ERROR([building outside of the main directory requires GNU make])
	;;
     *) ;;
    esac
    ;;
esac

AC_SUBST([CFLAGS])
AC_SUBST([LDFLAGS])

AC_PROG_LIBTOOL

AC_PROG_LN_S
AC_PROG_GCC_TRADITIONAL
AC_C_VOLATILE
AC_ISC_POSIX

AC_PATH_PROG([PATH_PERL], [perl])
AC_PATH_PROG([PATH_SH], [sh])
AC_PATH_PROG([PATH_TEST], [test])

AC_ARG_WITH(
    [net-snmp-config],
    [AS_HELP_STRING(
	[--with-net-snmp-config],
	[+ =net-snmp-config]
    )],
    [ans=$withval],
    [ans=yes]
)
case "$ans" in
 no)
    ;;
 yes)
    ans=net-snmp-config
    ;;
 /*)
    ;;
 */*)
    AC_MSG_ERROR([--with-net-snmp-config takes either a name or an absolute path])
    ;;
 *)
    ;;
esac
PROG_NET_SNMP_CONFIG=$ans
AC_MSG_CHECKING([for net-snmp-config path])
case "$PROG_NET_SNMP_CONFIG" in
 no) ;;
 /*)
    PATH_NET_SNMP_CONFIG=$PROG_NET_SNMP_CONFIG
    ;;
 *)
    AC_PATH_PROG([PATH_NET_SNMP_CONFIG], [$PROG_NET_SNMP_CONFIG])
    ;;
esac
AC_MSG_RESULT([$PATH_NET_SNMP_CONFIG])

case "$host" in
 *-*-vxworks*)
    ac_link="$ac_link $VX_KERNEL"
    ;;
esac

AC_PROG_INSTALL

AC_CHECK_FUNC([gethostent], ,
  AC_SEARCH_LIBS([gethostent], [nsl], , , [$libxnet -lsocket]))
AC_CHECK_FUNC([openlog], , 
  AC_SEARCH_LIBS([openlog], [gen], , 
  AC_SEARCH_LIBS([openlog], [syslog], , , [$libxnet -lsocket])))
AC_SEARCH_LIBS([MD5Init], [md5 md])
AC_CHECK_FUNCS(MD5Init)

NTP_LINEEDITLIBS

dnl Digital UNIX V4.0 and Solaris 7 have POSIX.1c functions in -lrt
dnl Solaris 2.6 only has -lposix4; in Solaris 7, this is a symlink to -lrt,
dnl so only use one of them.  Linux (glibc-2.1.2 and -2.2.2, at least)
dnl does Strange Things with extra processes using the Posix-compatibility
dnl real-time library, so we don't want to use it.
dnl
dnl 081118 Harlan got tired of looking for a way to get the sched*()
dnl functions to link OK with either cc or gcc.

case "$host" in
 *-*-*linux*) ;;
 *-*-osf4*) ;;
 *-*-osf5*) ;;
 *)
    AC_CHECK_LIB([rt], [sched_setscheduler], [],
	[AC_CHECK_LIB([posix4], [sched_setscheduler])])
    ;;
esac

AC_SEARCH_LIBS([setsockopt], [socket xnet])
AC_SEARCH_LIBS([res_init], [resolv], [], [], [-lsocket -lnsl])
case "$host" in
 *-*-darwin*)
    AC_CHECK_LIB([resolv],[res_9_init])
    ;;
esac
AC_HEADER_RESOLV
AC_CHECK_FUNCS([res_init __res_init])

AC_HEADER_STDC
AC_CHECK_HEADERS([bstring.h])
AC_CHECK_HEADER(
    [dns_sd.h],
    [AC_CHECK_LIB(
	[dns_sd],
	[DNSServiceRegister],
	[AC_DEFINE([HAVE_DNSREGISTRATION], [1],
	    [Use Rendezvous/DNS-SD registration])]
    )]
)
case "$ac_cv_lib_dns_sd_DNSServiceRegister" in
 yes)
    LIBS="-ldns_sd $LIBS"
esac
AC_CHECK_HEADERS([errno.h fcntl.h ieeefp.h kvm.h math.h])

AC_CHECK_HEADERS(
    [md5.h],
    [],
    [],
    [
	#ifdef HAVE_SYS_TYPES_H
	# include <sys/types.h>
	#endif
    ]
)
AC_CHECK_HEADERS([memory.h netdb.h poll.h])
AC_CHECK_HEADERS([sgtty.h stdlib.h string.h termio.h])
AC_CHECK_HEADERS([termios.h timepps.h timex.h unistd.h])

case "$host" in
 *-*-aix*)
    AC_CHECK_HEADERS([utmpx.h])
    case "$ac_cv_header_utmpx_h" in
     yes)
	;;
     *)
	AC_CHECK_HEADERS([utmp.h])
	;;
    esac
    ;;
 *) 
    AC_CHECK_HEADERS([utmp.h utmpx.h])
    ;;
esac

#
# On Suns only (so far) getpass() truncates the typed password to 8
# characters, but getpassphrase() allows up to 257.  Most systems'
# getpass() does not truncate, at least not so as to affect ntpq and
# ntpdc password prompts.
#
# So check for getpassphrase(), but only on Sun operating systems.
#
case "$host" in
 *-*-sunos*|*-*-solaris*)
    AC_CHECK_FUNCS([getpassphrase])
esac

AC_CHECK_HEADERS([arpa/nameser.h])
AC_CHECK_HEADERS([sys/socket.h])

dnl  HP-UX 11.31 on HPPA has a net/if.h that can't be compiled with gcc4
dnl  due to an incomplete type (a union) mpinfou used in an array.  gcc3
dnl  compiles it without complaint.  The mpinfou union is defined later
dnl  in the resulting preprocessed source than the spu_info array in
dnl  /usr/include/machine/sys/getppdp.h:
dnl	extern union mpinfou spu_info[];
dnl  triggering the error.  Our strategy is on HP-UX only, test for
dnl  net/netmp.h, which is the file included by net/if.h that leads to
dnl  getppdp.h.  If it is present but can't be compiled, try adding
dnl  a duplicate definition of mpinfou, which should then allow the
dnl  following net/if.h and net/if6.h tests to proceed normally.
dnl  Using net/netmp.h allows us to avoid polluting test results for
dnl  net/if.h.
#
case "$host" in
 *-hp-hpux*)
    AC_CHECK_HEADERS(
	[net/netmp.h],
	[netmp_h_works=yes],
	[netmp_h_works=no]
    )
    case "$netmp_h_works" in
     no)
	cat >>confdefs.h <<_ACEOF
#ifndef MPINFOU_PREDECLARED
# define MPINFOU_PREDECLARED
typedef union mpinfou {		/* For lint */
	struct pdk_mpinfo *pdkptr;
	struct mpinfo *pikptr;
} mpinfou_t;
#endif
_ACEOF
	AH_BOTTOM([
#ifndef MPINFOU_PREDECLARED
# define MPINFOU_PREDECLARED
typedef union mpinfou {		/* For lint */
	struct pdk_mpinfo *pdkptr;
	struct mpinfo *pikptr;
} mpinfou_t;
#endif
])
	;;
    esac
    ;;
esac

case "$host" in
 *-linux*)
    AC_CHECK_HEADERS([linux/if_addr.h], [], [], [
	#ifdef HAVE_SYS_SOCKET_H
	# include <sys/socket.h>
	#endif
    ])
esac

AC_CHECK_HEADERS([net/if.h], [], [],
[#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])
AC_CHECK_HEADERS([net/if6.h])
AC_CHECK_HEADERS([net/route.h], [], [], [
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
])
AC_CHECK_HEADERS([netinet/in_system.h netinet/in_systm.h netinet/in.h])
AC_CHECK_HEADERS([net/if_var.h], [], [],
[#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
])
AC_CHECK_HEADERS([netinet/ip.h netinet/in_var.h], [], [],
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
])

# Check for IPTOS_PREC
AC_CACHE_CHECK(
    [IPPROTO_IP IP_TOS IPTOS_LOWDELAY],
    [ntp_cv_ip_tos],
    [AC_EGREP_CPP(
	[yes],
	[
	    #ifdef HAVE_SYS_TYPES_H
	    # include <sys/types.h>
	    #endif
	    #ifdef HAVE_NETINET_IP_H
	    # include <netinet/in.h>
	    # include <netinet/ip.h>
	    #endif
	    #if defined(IPPROTO_IP) && defined(IP_TOS) && defined(IPTOS_LOWDELAY)
		yes
	    #endif
	],
	[ntp_cv_ip_tos=yes],
	[ntp_cv_ip_tos=no]
    )]
)
case "$ntp_cv_ip_tos" in
 yes)
    AC_DEFINE([HAVE_IPTOS_SUPPORT], [1], [Do we have IPTOS support?])
esac

AC_CHECK_HEADERS([netinfo/ni.h])
case "$ac_cv_header_netinfo_ni_h" in
 yes)
    AC_DEFINE([HAVE_NETINFO], [1], [NetInfo support?])
esac
AC_CHECK_HEADERS([sun/audioio.h sys/audioio.h])
dnl AC_CHECK_HEADERS([sys/chudefs.h])
AC_CHECK_HEADERS([sys/clkdefs.h sys/file.h])
case "$host" in
 *-*-sunos4*)
    ;;
 *) 
    AC_CHECK_HEADERS([sys/ioctl.h])
    ;;
esac
AC_CHECK_HEADERS([sys/ipc.h sys/lock.h sys/mman.h])
AC_CHECK_HEADERS([sys/modem.h sys/param.h sys/ppsclock.h])
# HMS: Check sys/proc.h and sys/resource.h after some others
AC_CHECK_HEADERS([sys/ppstime.h sched.h])
case "$ac_cv_header_sched_h" in
 yes)
    ;;
 *) 
    AC_CHECK_HEADERS([sys/sched.h])
    ;;
esac
case "$host" in
 *-*-sco*)
    AC_CHECK_HEADERS([sys/sio.h])
    ;;
esac
# HMS: Check sys/shm.h after some others
AC_CHECK_HEADERS([sys/select.h sys/signal.h sys/sockio.h])
# HMS: Checked sys/socket.h earlier
case "$host" in
 *-*-netbsd*)
    ;;
 *) 
    AC_CHECK_HEADERS([machine/soundcard.h sys/soundcard.h])
    ;;
esac
AC_CHECK_HEADERS([sys/stat.h sys/stream.h sys/stropts.h])
# sys/sysctl.h depends on sys/param.h on OpenBSD - Bug 1576
AC_CHECK_HEADERS([sys/sysctl.h], [], [],
[#if defined HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif])
AC_CHECK_HEADERS([sys/syssgi.h sys/systune.h])
AC_CHECK_HEADERS([sys/termios.h sys/time.h sys/signal.h])
AC_EGREP_CPP(
    [yes],
    [
	#ifdef HAVE_SYS_TIME_H
	# include <sys/time.h>
	#endif
	#ifdef HAVE_ERRNO_H
	# include <errno.h>
	#endif
	#include <sys/timepps.h>
	#ifdef PPS_API_VERS_1
	    yes
	#endif
    ],
    [AC_CHECK_HEADERS(
	[sys/timepps.h],
	[],
	[],
	[
	    #ifdef HAVE_SYS_TIME_H
	    # include <sys/time.h>
	    #endif
	    #ifdef HAVE_ERRNO_H
	    # include <errno.h>
	    #endif
	]
    )]
)
AC_CHECK_HEADERS([sys/timers.h sys/tpro.h sys/types.h sys/wait.h])
AC_HEADER_TIME
case "$host" in
*-convex-*)
    AC_CHECK_HEADERS([/sys/sync/queue.h /sys/sync/sema.h])
    ;;
*-*-bsdi*)
    AC_CHECK_HEADERS([machine/inline.h sys/pcl720.h sys/i8253.h])
    ;;
esac

case "$host" in
 *-*-*linux*)
    AC_CHECK_FUNCS([__adjtimex __ntp_gettime])
    ;;
esac
case "$ac_cv_func___adjtimex" in
 yes)
    ;;
 *)
    AC_CHECK_LIB([elf], [nlist])	dnl Only needed for tickadj...
    dnl AC_CHECK_LIB(kvm, main, , , -lelf)
    AC_CHECK_LIB([kvm], [main])		dnl We already know about -lelf here...
    AC_CHECK_LIB([ld], [nlist])
    AC_CHECK_LIB([mld], [nlist])
    AC_CHECK_HEADERS([nlist.h sys/var.h])
    case "$ac_cv_header_nlist_h" in
     yes)
	AC_DEFINE([NLIST_STRUCT], [1], [nlist stuff])
    esac
    AC_CACHE_CHECK(
	[for n_un in struct nlist],
	[ntp_cv_struct_nlist_n_un],
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #include <nlist.h>
		]],
		[[
		    struct nlist n;
		    n.n_un.n_name = 0;
		]]
	    )]
	    [ntp_cv_struct_nlist_n_un=yes],
	    [ntp_cv_struct_nlist_n_un=no]
	)]
    )
    case "$ntp_cv_struct_nlist_n_un" in
     yes)
	AC_DEFINE([NLIST_NAME_UNION], [1],
	    [does struct nlist use a name union?])
    esac
    ;;
esac

AC_CHECK_HEADERS([sys/proc.h], [], [],
[#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
])

AC_CHECK_HEADERS([sys/resource.h], [], [],
[#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
])

AC_CHECK_HEADERS([sys/shm.h], [], [],
[#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_IPC_H
# include <sys/ipc.h>
#endif
])

AC_CHECK_HEADERS([sys/timex.h], [], [],
[#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
])

AC_CHECK_HEADERS([resolv.h], [], [],
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
])

AC_CACHE_CHECK(
    [for basic volatile support],
    [ntp_cv_c_volatile],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
	    ]],
	    [[
		volatile int x;
	    ]]
	)]
	[ntp_cv_c_volatile=yes],
	[ntp_cv_c_volatile=no]
    )]
)
case "$ntp_cv_c_volatile" in
 yes)
    ;;
 *)
    AC_DEFINE([volatile], [], [define away volatile?])
    ;;
esac

AC_C_CONST
AC_C_BIGENDIAN
AC_TYPE_SIGNAL
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([time_t, long, uintptr_t])
AC_CHECK_SIZEOF([time_t])
 
AH_VERBATIM([TYPEDEF_UINTPTR_T],
[/* Provide a typedef for uintptr_t? */
#ifndef HAVE_UINTPTR_T
typedef unsigned int	uintptr_t;
#define HAVE_UINTPTR_T	1
#endif])

AC_STRUCT_TM

AC_CACHE_CHECK(
    [for u_int8_t],
    [ntp_cv_type_u_int8_t],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
	    ]],
	    [[
		u_int8_t len = 42;
	    ]]
	)],
	[ntp_cv_type_u_int8_t=yes],
	[ntp_cv_type_u_int8_t=no]
    )]
)
case "$ntp_cv_type_u_int8_t" in
 yes)
    AC_DEFINE([HAVE_TYPE_U_INT8_T], [1], [Does u_int8_t exist?])
esac

AC_CACHE_CHECK(
    [for u_int64_t],
    [ntp_cv_type_u_int64_t],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
	    ]],
	    [[
		u_int64_t len = 42;
	    ]]
	)],
	[ntp_cv_type_u_int64_t=yes],
	[ntp_cv_type_u_int64_t=no]
    )]
)
case "$ntp_cv_type_u_int64_t" in
 yes)
    AC_DEFINE([HAVE_TYPE_U_INT64_T], [1], [Does u_int64_t exist?])
esac

AC_CACHE_CHECK(
    [for a fallback value for HZ],
    [ntp_cv_default_hz],
    [
	ntp_cv_default_hz=100
	case "$host" in
	 alpha*-dec-osf4*|alpha*-dec-osf5*)
	    ntp_cv_default_hz=1024
	    ;;
	 mips-dec-ultrix4*)
	    ntp_cv_default_hz=256
	    ;;
	esac
    ]
)
AC_DEFINE_UNQUOTED([DEFAULT_HZ], [$ntp_cv_default_hz],
    [What is the fallback value for HZ?])

AC_CACHE_CHECK(
    [if we need to override the system's value for HZ],
    [ntp_cv_override_hz],
    [
	ntp_cv_override_hz=no
	case "$host" in
	 alpha*-dec-osf4*|alpha*-dec-osf5*)
	    ntp_cv_override_hz=yes
	    ;;
	 mips-dec-ultrix4*)
	    ntp_cv_override_hz=yes
	    ;;
	 *-*-freebsd*)
	    ntp_cv_override_hz=yes
	    ;;
	 *-*-sunos4*)
	    ntp_cv_override_hz=yes
	    ;;
	 *-*-kfreebsd*)
	    ntp_cv_override_hz=yes
	    ;;
	esac
    ]
)
case "$ntp_cv_override_hz" in
 yes)
    AC_DEFINE([OVERRIDE_HZ], [1],
	[Do we need to override the system's idea of HZ?])
esac

dnl AC_CACHE_CHECK(ut_host in struct utmp, ac_cv_func_ut_host_in_utmp,
dnl [AC_TRY_LINK([#include <sys/types.h>
dnl #include <utmp.h>], [struct utmp ut; ut.ut_host;],
dnl ac_cv_func_ut_host_in_utmp=yes, ac_cv_func_ut_host_in_utmp=no)])
dnl if test $su_cv_func_ut_host_in_utmp = yes; then
dnl   AC_DEFINE(HAVE_UT_HOST)
dnl fi

dnl AC_MSG_CHECKING(if we can get the system boot time)
dnl AC_CACHE_VAL(su_cv_have_boot_time,
dnl [AC_EGREP_CPP(yes,
dnl [#ifdef HAVE_UTMPX_H
dnl #include <utmpx.h>
dnl #else
dnl #include <utmp.h>
dnl #endif
dnl #ifdef BOOT_TIME
dnl yes
dnl #endif
dnl ], su_cv_have_boot_time=yes, su_cv_have_boot_time=no)])
dnl AC_MSG_RESULT($su_cv_have_boot_time)

AC_CACHE_CHECK(
    [for struct rt_msghdr],
    [ntp_cv_struct_rt_msghdr],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <net/if.h>
		#include <net/route.h>
	    ]],
	    [[
		struct rt_msghdr p;
	    ]]
	)],
	[ntp_cv_struct_rt_msghdr=yes],
	[ntp_cv_struct_rt_msghdr=no]
    )]
)

AC_CACHE_CHECK(
    [for struct rtattr],
    [ntp_cv_rtattr],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <stddef.h>
		#include <sys/socket.h>
		#include <linux/rtnetlink.h>
	    ]],
	    [[
		struct rtattr p;
	    ]]
	)],
	[ntp_cv_rtattr=yes],
	[ntp_cv_rtattr=no]
    )]
)

case "$ntp_cv_struct_rt_msghdr$ntp_cv_rtattr" in
 *yes*)
    AC_DEFINE([HAS_ROUTING_SOCKET], [1],
	[Do we have a routing socket (rt_msghdr or rtattr)?])
    case "$ntp_cv_rtattr" in
     yes)
	AC_DEFINE([HAVE_RTNETLINK], [1],
	    [Do we have Linux routing socket?])
    esac
esac

AC_CACHE_CHECK(
    [struct sigaction for sa_sigaction],
    [ntp_cv_struct_sigaction_has_sa_sigaction],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <signal.h>
	    ]],
	    [[
		struct sigaction act;
		act.sa_sigaction = 0;
	    ]]
	)],
	[ntp_cv_struct_sigaction_has_sa_sigaction=yes],
	[ntp_cv_struct_sigaction_has_sa_sigaction=no]
    )]
)
case "$ntp_cv_struct_sigaction_has_sa_sigaction" in
 yes)
    AC_DEFINE([HAVE_SA_SIGACTION_IN_STRUCT_SIGACTION], [1], [Obvious])
esac

AC_CACHE_CHECK(
    [for struct ppsclockev],
    [ntp_cv_struct_ppsclockev],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_SYS_TERMIOS_H
		# include <sys/termios.h>
		#endif
		#ifdef HAVE_SYS_TIME_H
		# include <sys/time.h>
		#endif
		#ifdef HAVE_SYS_PPSCLOCK_H
		# include <sys/ppsclock.h>
		#endif
	    ]],
	    [[
		extern struct ppsclockev *pce;
		return pce->serial;
	    ]]
	)],
	[ntp_cv_struct_ppsclockev=yes],
	[ntp_cv_struct_ppsclockev=no]
    )]
)
case "$ntp_cv_struct_ppsclockev" in
 yes)
    AC_DEFINE([HAVE_STRUCT_PPSCLOCKEV], [1],
	[Does a system header define struct ppsclockev?])
esac

AC_CACHE_CHECK(
    [for struct sockaddr_storage],
    [ntp_cv_sockaddr_storage],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_SYS_SOCKET_H
		# include <sys/socket.h>
		#endif
		#ifdef HAVE_NETINET_IN_H
		# include <netinet/in.h>
		#endif
	    ]],
	    [[
		struct sockaddr_storage n;
	    ]]
	)],
	[ntp_cv_sockaddr_storage=yes],
	[ntp_cv_sockaddr_storage=no]
    )]
)
case "$ntp_cv_sockaddr_storage" in
 yes)
    AC_DEFINE([HAVE_STRUCT_SOCKADDR_STORAGE], [1],
	[Does a system header define struct sockaddr_storage?])
esac

AC_CACHE_CHECK(
    [for sockaddr_storage.ss_family],
    [ntp_cv_have_ss_family],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_SYS_SOCKET_H
		# include <sys/socket.h>
		#endif
		#ifdef HAVE_NETINET_IN_H
		# include <netinet/in.h>
		#endif
	    ]],
	    [[
		struct sockaddr_storage s;
		s.ss_family = 1;
	    ]]
	)],
	[ntp_cv_have_ss_family=yes],
	[ntp_cv_have_ss_family=no]
    )]
)

case "$ntp_cv_have_ss_family" in
 no)
    AC_CACHE_CHECK(
	[for sockaddr_storage.__ss_family],
	[ntp_cv_have___ss_family],
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #ifdef HAVE_SYS_TYPES_H
		    # include <sys/types.h>
		    #endif
		    #ifdef HAVE_SYS_SOCKET_H
		    # include <sys/socket.h>
		    #endif
		    #ifdef HAVE_NETINET_IN_H
		    # include <netinet/in.h>
		    #endif
		]],
		[[
		    struct sockaddr_storage s;
		    s.__ss_family = 1;
		]]
	    )],
	    [ntp_cv_have___ss_family=yes],
	    [ntp_cv_have___ss_family=no]
	)]
    )
    case "$ntp_cv_have___ss_family" in
     yes)
	AC_DEFINE([HAVE___SS_FAMILY_IN_SS], [1],
	    [Does struct sockaddr_storage have __ss_family?])
    esac
esac

AH_VERBATIM(
    [HAVE___SS_FAMILY_IN_SS_VERBATIM],
    [
	/* Handle sockaddr_storage.__ss_family */
	#ifdef HAVE___SS_FAMILY_IN_SS
	# define ss_family __ss_family
	#endif /* HAVE___SS_FAMILY_IN_SS */
    ]
)

AC_CACHE_CHECK(
    [for sockaddr_storage.ss_len],
    [ntp_cv_have_ss_len],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_SYS_SOCKET_H
		# include <sys/socket.h>
		#endif
		#ifdef HAVE_NETINET_IN_H
		# include <netinet/in.h>
		#endif
	    ]],
	    [[
		struct sockaddr_storage s;
		s.ss_len = 1;
	    ]]
	)],
	[ntp_cv_have_ss_len=yes],
	[ntp_cv_have_ss_len=no]
    )]
)

case "$ntp_cv_have_ss_len" in
 no)
    AC_CACHE_CHECK(
	[for sockaddr_storage.__ss_len],
	[ntp_cv_have___ss_len],
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #ifdef HAVE_SYS_TYPES_H
		    # include <sys/types.h>
		    #endif
		    #ifdef HAVE_SYS_SOCKET_H
		    # include <sys/socket.h>
		    #endif
		    #ifdef HAVE_NETINET_IN_H
		    # include <netinet/in.h>
		    #endif
		]],
		[[
		    struct sockaddr_storage s;
		    s.__ss_len = 1;
		]]
	    )],
	    [ntp_cv_have___ss_len=yes],
	    [ntp_cv_have___ss_len=no]
	)]
    )
    case "$ntp_cv_have___ss_len" in
     yes)
	AC_DEFINE([HAVE___SS_LEN_IN_SS], [1],
	    [Does struct sockaddr_storage have __ss_len?])
    esac
esac

AH_VERBATIM(
    [HAVE___SS_LEN_IN_SS_VERBATIM],
    [
	/* Handle sockaddr_storage.__ss_len */
	#ifdef HAVE___SS_LEN_IN_SS
	# define ss_len __ss_len
	#endif /* HAVE___SS_LEN_IN_SS */
    ]
)

#
# Look for in_port_t.
#
AC_CACHE_CHECK(
    [for in_port_t],
    [isc_cv_have_in_port_t],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#include <netinet/in.h>
	    ]],
	    [[
		in_port_t port = 25; 
		return (0);
	    ]]
	)],
	[isc_cv_have_in_port_t=yes],
	[isc_cv_have_in_port_t=no]
    )]
)
case "$isc_cv_have_in_port_t" in
 no)
	AC_DEFINE([ISC_PLATFORM_NEEDPORTT], [1],
	    [Declare in_port_t?])
esac

case "$ac_cv_header_machine_soundcard_h$ac_cv_header_sys_soundcard_h" in
  *yes*)
    AC_CACHE_CHECK(
	[for struct snd_size],
	[ntp_cv_struct_snd_size],
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #ifdef HAVE_MACHINE_SOUNDCARD_H
		    # include <machine/soundcard.h>
		    #endif
		    #ifdef HAVE_SYS_SOUNDCARD_H
		    # include <sys/soundcard.h>
		    #endif
		]],
		[[
		    extern struct snd_size *ss;
		    return ss->rec_size;
		]]
	    )],
	    [ntp_cv_struct_snd_size=yes],
	    [ntp_cv_struct_snd_size=no]
        )]
    )
    case "$ntp_cv_struct_snd_size" in
     yes)
	AC_DEFINE([HAVE_STRUCT_SND_SIZE], [1],
	    [Do we have struct snd_size?])
    esac
esac

AC_CACHE_CHECK(
    [struct clockinfo for hz],
    [ntp_cv_struct_clockinfo_has_hz],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/time.h>
	    ]],
	    [[
		extern struct clockinfo *pc;
		return pc->hz;
	    ]]
	)],
	[ntp_cv_struct_clockinfo_has_hz=yes],
	[ntp_cv_struct_clockinfo_has_hz=no]
    )]
)
case "$ntp_cv_struct_clockinfo_has_hz" in
 yes)
    AC_DEFINE([HAVE_HZ_IN_STRUCT_CLOCKINFO], [1], [Obvious])
esac

AC_CACHE_CHECK(
    [struct clockinfo for tickadj],
    [ntp_cv_struct_clockinfo_has_hz],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/time.h>
	    ]],
	    [[
		extern struct clockinfo *pc;
		return pc->tickadj;
	    ]]
	)],
	[ntp_cv_struct_clockinfo_has_hz=yes],
	[ntp_cv_struct_clockinfo_has_hz=no]
    )]
)
case "$ntp_cv_struct_clockinfo_has_hz" in
 yes)
    AC_DEFINE([HAVE_TICKADJ_IN_STRUCT_CLOCKINFO], [1], [Obvious])
esac

AC_CACHE_CHECK(
    [for struct timespec],
    [ntp_cv_struct_timespec],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/time.h>
		/* Under SunOS, timespec is in sys/timepps.h,
		   which needs errno.h and FRAC */
		#ifdef HAVE_ERRNO_H
		# include <errno.h>
		#endif
		#ifdef HAVE_SYS_TIMEPPS_H
		# define FRAC 4294967296
		# include <sys/timepps.h>
		#endif
	    ]],
	    [[
		struct timespec n;
	    ]]
	)],
	[ntp_cv_struct_timespec=yes],
	[ntp_cv_struct_timespec=no]
    )]
)
case "$ntp_cv_struct_timespec" in
 yes)
    AC_DEFINE([HAVE_STRUCT_TIMESPEC], [1], [struct timespec declared?])
esac

AC_CACHE_CHECK(
    [for struct ntptimeval],
    [ntp_cv_struct_ntptimeval],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/time.h>
		#include <sys/timex.h>
	    ]],
	    [[
		struct ntptimeval n;
	    ]]
	)],
	[ntp_cv_struct_ntptimeval=yes],
	[ntp_cv_struct_ntptimeval=no]
    )]
)
case "$ntp_cv_struct_ntptimeval" in
 yes)
    AC_DEFINE([HAVE_STRUCT_NTPTIMEVAL], [1],
	[Do we have struct ntptimeval?])
esac

AC_CHECK_MEMBERS(
    [struct ntptimeval.time.tv_nsec],
    [],
    [],
    [
	#ifdef HAVE_SYS_TIME_H
	#include <sys/time.h>
	#else
	# ifdef HAVE_TIME_H
	#  include <time.h>
	# endif
	#endif
	#ifdef HAVE_SYS_TIMEX_H
	# include <sys/timex.h>
	#else
	# ifdef HAVE_TIMEX_H
	#  include <timex.h>
	# endif
	#endif
    ]
)

AC_C_INLINE

case "$ac_cv_c_inline" in
 '')
    ;;
 *)
    AC_DEFINE([HAVE_INLINE], [1], [inline keyword or macro available])
    AC_SUBST([HAVE_INLINE])
esac

AC_C_CHAR_UNSIGNED		dnl CROSS_COMPILE?
AC_CHECK_SIZEOF([signed char])
AC_CHECK_TYPES([s_char])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])

case "$ac_cv_c_char_unsigned$ac_cv_sizeof_signed_char$ac_cv_type_s_char" in
 *yes)
    # We have a typedef for s_char.  Might as well believe it...
    ;;
 no0no)
    # We have signed chars, can't say 'signed char', no s_char typedef.
    AC_DEFINE([NEED_S_CHAR_TYPEDEF], [1],
	[Do we need an s_char typedef?])
    ;;
 no1no)
    # We have signed chars, can say 'signed char', no s_char typedef.
    AC_DEFINE([NEED_S_CHAR_TYPEDEF], [1],
	[Do we need an s_char typedef?])
    ;;
 yes0no)
    # We have unsigned chars, can't say 'signed char', no s_char typedef.
    AC_MSG_ERROR([No way to specify a signed character!])
    ;;
 yes1no)
    # We have unsigned chars, can say 'signed char', no s_char typedef.
    AC_DEFINE([NEED_S_CHAR_TYPEDEF], [1],
	[Do we need an s_char typedef?])
    ;;
esac
AC_TYPE_UID_T

case "$host" in
 *-*-aix[[456]]*)
	# (prr) aix 4.1 doesn't have clock_settime, but in aix 4.3 it's a stub
	# (returning ENOSYS).  I didn't check 4.2.  If, in the future,
	# IBM pulls its thumbs out long enough to implement clock_settime,
	# this conditional will need to change.  Maybe use AC_TRY_RUN
	# instead to try to set the time to itself and check errno.
    ;;
 *)
    AC_SEARCH_LIBS([clock_gettime], [rt])
    AC_CHECK_FUNCS([clock_gettime clock_settime])
    ;;
esac
AC_CHECK_FUNCS([daemon])
AC_CHECK_FUNCS(
    [finite],
    [],
    [AC_CHECK_FUNCS(
	[isfinite],
	[],
	[
	    AC_MSG_CHECKING([for isfinite with <math.h>])
	    _libs=$LIBS
	    LIBS="$LIBS -lm"
	    AC_LINK_IFELSE(
		[AC_LANG_PROGRAM(
		    [[
			#include <math.h>
		    ]],
		    [[
			float f = 0.0;
			isfinite(f);
		    ]]
		)],
		[ans=yes],
		[ans=no]
	    )
	    LIBS=$_libs
	    AC_MSG_RESULT([$ans])
	    case "$ans" in
	     yes)
		AC_DEFINE([HAVE_ISFINITE], [1])
	    esac
	]
    )]
)

AC_CHECK_FUNCS([getbootfile getclock getdtablesize])

AC_ARG_ENABLE(
    [getifaddrs],
    [AS_HELP_STRING(
	[--enable-getifaddrs],
	[s Enable the use of getifaddrs() [[yes|no|glibc]].
glibc: Use getifaddrs() in glibc if you know it supports IPv6.]
    )],
    [want_getifaddrs="$enableval"],
    [want_getifaddrs="yes"]
)

case $want_getifaddrs in
 yes|glibc)
    #
    # Do we have getifaddrs() ?
    #
    case $host in
     *-*linux*)
	# Some recent versions of glibc support getifaddrs() which does not
	# provide AF_INET6 addresses while the function provided by the USAGI
	# project handles the AF_INET6 case correctly.  We need to avoid
	# using the former but prefer the latter unless overridden by
	# --enable-getifaddrs=glibc.
	case "$want_getifaddrs" in
	 glibc)
	    AC_CHECK_FUNCS([getifaddrs])
	    ;;
	 *)
	    save_LIBS="$LIBS"
	    LIBS="-L/usr/local/v6/lib $LIBS"
	    AC_CHECK_LIB(
		[inet6],
		[getifaddrs],
		[ans=yes],
		[ans=no]
	    )
	    case "$ans" in
	     yes)
		LIBS="$LIBS -linet6"
		AC_DEFINE([HAVE_GETIFADDRS], [1])
		;;
	     *)
		LIBS=${save_LIBS}
		;;
	    esac
	    ;;
	esac
	;;
    esac
    ;;
 *)
    AC_CHECK_FUNCS([getifaddrs])
    ;;
esac

AC_CACHE_CHECK(
    [type of socklen arg for getsockname()],
    [ntp_cv_getsockname_socklen_type],
    [
    getsockname_socklen_type_found=no
    for getsockname_arg2 in 'struct sockaddr *' 'void *'; do
	for ntp_cv_getsockname_socklen_type in 'socklen_t' 'size_t' 'unsigned int' 'int'; do
	    AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
		    [[
			#ifdef HAVE_SYS_TYPES_H
			# include <sys/types.h>
			#endif
			#ifdef HAVE_SYS_SOCKET_H
			# include <sys/socket.h>
			#endif
		    ]], [[
			extern
			getsockname(int, $getsockname_arg2, 
				$ntp_cv_getsockname_socklen_type *);
		    ]]
		)],
		[getsockname_socklen_type_found=yes ; break 2],
		[]
	    )
	done
    done
    case "$getsockname_socklen_type_found" in
     no)
	ntp_cv_getsockname_socklen_type='socklen_t'
    esac
    AS_UNSET([getsockname_arg2])
    AS_UNSET([getsockname_socklen_type_found])
    ]
)
AC_DEFINE_UNQUOTED([GETSOCKNAME_SOCKLEN_TYPE],
		   [$ntp_cv_getsockname_socklen_type],
		   [What is getsockname()'s socklen type?])

AC_CHECK_FUNCS([getuid getrusage hstrerror])
AC_CHECK_FUNC([gettimeofday], [], [
case "$host" in
  *-*-mpeix*) ac_cv_func_gettimeofday=yes
    ;;
esac])

#
# Check for if_nametoindex() for IPv6 scoped addresses support
#
case "$host" in
 *-hp-hpux*)
    AC_SEARCH_LIBS([if_nametoindex], [ipv6])
esac
AC_CHECK_FUNCS([if_nametoindex])
case "$ac_cv_func_if_nametoindex" in
 yes)
	AC_DEFINE([ISC_PLATFORM_HAVEIFNAMETOINDEX], [1],
	    [ISC: do we have if_nametoindex()?])
esac

AC_SEARCH_LIBS([inet_ntop], [resolv], , , [-lsocket -lnsl])
AC_CHECK_FUNC([inet_ntop], [], 
    [AC_DEFINE([ISC_PLATFORM_NEEDNTOP], [1], [ISC: provide inet_ntop()])])
AC_CHECK_FUNC([inet_pton], [],
    [AC_DEFINE([ISC_PLATFORM_NEEDPTON], [1], [ISC: provide inet_pton()])])

case "$ac_cv_header_kvm_h" in
 yes)
    AC_CHECK_FUNCS([kvm_open])
    ;;
esac

AC_CHECK_FUNCS([memcpy memmove memset])
case "$host" in
 *-*-sco3.2v5.0.*)
    # Just stubs.  Idiots.
    ;;
 *) AC_CHECK_FUNCS([mkstemp])
    ;;
esac
AC_CHECK_FUNCS([mktime])
case "$host" in
 *-*-aix[[456]]*)
    # Just a stub.  Idiots.
    ;;
 *-*-irix[[45]]*)
    # Just a stub in "old" Irix.  Idiots.
    ;;
 *-*-*linux*)
    # there, but more trouble than it is worth for now (resolver problems)
    ;;
 *-*-qnx*)
    # Apparently there but not working in QNX.  Idiots?
    ;;
 *-*-sco3.2v5.0.*)
    # Just a stub.  Idiots.
    ;;
 alpha*-dec-osf4*|alpha*-dec-osf5*)
    # mlockall is there, as a #define calling memlk via <sys/mman.h>
    # Not easy to test for - cheat.
    AC_CHECK_FUNCS([memlk], [ac_cv_func_mlockall=yes])
    AC_CHECK_FUNCS([mlockall])
    ;;
 *) AC_CHECK_FUNCS([mlockall])
    ;;
esac
AC_CHECK_FUNCS([nice nlist])
case "$host" in
 *-*-solaris2.6)
    # Broken...
    ;;
 *) AC_CHECK_FUNCS([ntp_adjtime ntp_gettime])
    ;;
esac
AC_CHECK_FUNCS([plock pututline pututxline readlink recvmsg rtprio])
case "$host" in
 *-*-aix[[456]]*)
    # Just a stub in AIX 4.  Idiots.
    ;;
 *-*-solaris2.5*)
    # Just stubs in solaris2.5.  Idiots.
    ;;
 *) AC_CHECK_FUNCS([sched_setscheduler])
    ;;
esac
AC_CHECK_FUNCS([setlinebuf setpgid setpriority setsid])
AC_CHECK_FUNCS([setrlimit])
AC_CHECK_FUNCS([settimeofday], ,[
case "$host" in
  *-*-mpeix*) ac_cv_func_settimeofday=yes
    ;;
esac])
AC_CHECK_FUNCS([setvbuf sigaction sigvec sigset sigsuspend stime strchr])
AC_CHECK_FUNCS([sysconf sysctl snprintf strdup strerror strstr timegm])
case "$host" in
 *-*-aix[[456]]*)
    # Just stubs.  Idiots.
    ;;
 *-*-netbsd1*)
    # Just stubs.  Idiots.
    ;;
 *-*-netbsdelf1*)
    # Just stubs.  Idiots.
    ;;
 *-*-openbsd*)
    # Just stubs.  Idiots.
    ;;
 *) AC_CHECK_FUNCS([timer_create timer_settime])
    ;;
esac
AC_CHECK_FUNCS([umask uname updwtmp updwtmpx vsnprintf vsprintf])

###

# http://bugs.ntp.org/737
case "$ac_cv_func_recvmsg" in
 yes)
    AC_CACHE_CHECK(
	[if we need extra help to define struct iovec],
	[ntp_cv_struct_iovec_help],
	[
	    compiled=no
	    for ntp_cv_struct_iovec_help in '0' '1'; do
		AC_COMPILE_IFELSE(
		    [AC_LANG_PROGRAM(
			[[
			    #ifdef HAVE_SYS_TYPES_H
			    # include <sys/types.h>
			    #endif
			    #ifdef HAVE_SYS_SOCKET_H
			    # include <sys/socket.h>
			    #endif
			    #if $ntp_cv_struct_iovec_help
			    # include <sys/uio.h>
			    #endif
			]],
			[[
			    void foo(void) {
				ssize_t x;
				int s = 0;
				struct iovec iov;
				struct msghdr mh;
				int flags = 0;

				mh.msg_iov = &iov;
				x = recvmsg(s, &mh, flags);
			    }
			]]
		    )],
		    [compiled=yes ; break 1],
		    []
		)
	    done
	    case "$compiled" in
	     no)
		ntp_cv_struct_iovec_help=0
	    esac
	    AS_UNSET([compiled])
	]
    )
    case "$ntp_cv_struct_iovec_help" in
     1)
	AC_DEFINE([HAVE_SYS_UIO_H], [1],
	    [Use sys/uio.h for struct iovec help])
    esac
esac

case "$host" in
 *-*-sunos4*)
    AC_DEFINE([SPRINTF_CHAR], [1], [*s*printf() functions are char*])
    ;;
esac

AC_CACHE_CHECK(
    [number of arguments to gettimeofday()],
    [ntp_cv_func_Xettimeofday_nargs],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/time.h>
	    ]],
	    [[
		gettimeofday(0, 0);
		settimeofday(0, 0);
	    ]]
	)],
	[ntp_cv_func_Xettimeofday_nargs=2],
	[ntp_cv_func_Xettimeofday_nargs=1]
    )]
)
case "$ntp_cv_func_Xettimeofday_nargs" in
 1)
    AC_DEFINE([SYSV_TIMEOFDAY], [1], [Does Xettimeofday take 1 arg?])
esac

AC_CACHE_CHECK(
    [number of arguments taken by setpgrp()],
    [ntp_cv_func_setpgrp_nargs],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_UNISTD_H
		# include <unistd.h>
		#endif
	    ]],
	    [[
		setpgrp(0, 0);
	    ]]
	)],
	[ntp_cv_func_setpgrp_nargs=2],
	[ntp_cv_func_setpgrp_nargs=0]
    )]
)
case "$ntp_cv_func_setpgrp_nargs" in
 0)
    AC_DEFINE([HAVE_SETPGRP_0], [1],
	      [define if setpgrp takes 0 arguments])
esac

dnl we require ANSI C which mandates void * here
dnl we should clean up all uses of QSORT_USES_VOID_P so
dnl this can be removed.
AC_DEFINE([QSORT_USES_VOID_P], [1],
    [Does qsort expect to work on "void *" stuff?])

AC_CACHE_CHECK(
    [if we need to declare 'errno'],
    [ntp_cv_decl_errno],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_ERRNO_H
		# include <errno.h>
		#endif
	    ]],
	    [[
		errno = 0;
	    ]]
	)],
	[ntp_cv_decl_errno=no],
	[ntp_cv_decl_errno=yes]
    )]
)
case "$ntp_cv_decl_errno" in
 yes)
    AC_DEFINE([DECL_ERRNO], [1], [Declare errno?])
esac

dnl (prr) aix 4.3 defines h_errno as (*(int *)h_errno_which()) for
dnl MT purposes.  This makes the line "extern int h_errno" choke
dnl the compiler.  Hopefully adding !defined(h_errno) fixes this
dnl without breaking any other platforms.
dnl
AC_CACHE_CHECK(
    [if we may declare 'h_errno'],
    [ntp_cv_decl_h_errno],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#ifdef HAVE_NETINET_IN_H
		# include <netinet/in.h>
		#endif
		#ifdef HAVE_ARPA_NAMESER_H
		# include <arpa/nameser.h>
		#endif
		#ifdef HAVE_NETDB_H
		# include <netdb.h>
		#endif
		#ifdef HAVE_RESOLV_H
		# include <resolv.h>
		#endif
	    ]],
	    [[
		extern int h_errno;
	    ]]
	)],
	[ntp_cv_decl_h_errno=yes],
	[ntp_cv_decl_h_errno=no]
    )]
)
case "$ntp_cv_decl_h_errno" in
 yes) 
    AC_DEFINE([DECL_H_ERRNO], [1], [Declare h_errno?])
esac

AC_CACHE_CHECK(
    [if declaring 'syscall()' is ok],
    [ntp_cv_decl_syscall],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_SYS_TYPES_H
		# include <sys/types.h>
		#endif
		#ifdef HAVE_UNISTD_H
		# include <unistd.h>
		#endif
	    ]],
	    [[
		extern int syscall (int, ...);
	    ]]
	)]
	[ntp_cv_decl_syscall=yes],
	[ntp_cv_decl_syscall=no]
    )]
)
case "$ntp_cv_decl_syscall" in
 yes)
    AC_DEFINE([DECL_SYSCALL], [1], [Declare syscall()?])
esac

case "$host" in
 *-*-aix4.3.*)
    AC_DEFINE([DECL_HSTRERROR_0], [1], [Declaration style])		# Needed for XLC under AIX 4.3.2
    ;;
 *-*-mpeix*)
    AC_DEFINE([DECL_ADJTIME_0], [1], [Declaration style])
    AC_DEFINE([DECL_INET_NTOA_0], [1], [Declaration style])
    AC_DEFINE([DECL_MKTEMP_0], [1], [Declaration style])
    AC_DEFINE([DECL_SELECT_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETITIMER_0], [1], [Declaration style])
    AC_DEFINE([DECL_SYSLOG_0], [1], [Declaration style])
    AC_DEFINE([DECL_TIMEOFDAY_0], [1], [Declaration style])
    ;;
 *-*-osf[[45]]*)
    AC_DEFINE([DECL_PLOCK_0], [1], [Declaration style])
    AC_DEFINE([DECL_STIME_1], [1], [Declaration style])
    ;;
 *-*-qnx*)
    AC_DEFINE([DECL_ADJTIME_0], [1], [Declaration style])
    ;;
 *-*-riscos4*)
    AC_DEFINE([DECL_ADJTIME_0], [1], [Declaration style])
    AC_DEFINE([DECL_BZERO_0], [1], [Declaration style])
    AC_DEFINE([DECL_IOCTL_0], [1], [Declaration style])
    AC_DEFINE([DECL_IPC_0], [1], [Declaration style])
    AC_DEFINE([DECL_MEMMOVE_0], [1], [Declaration style])
    AC_DEFINE([DECL_MKTEMP_0], [1], [Declaration style])
    AC_DEFINE([DECL_RENAME_0], [1], [Declaration style])
    AC_DEFINE([DECL_SELECT_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETITIMER_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETPRIORITY_0], [1], [Declaration style])
    AC_DEFINE([DECL_STDIO_0], [1], [Declaration style])
    AC_DEFINE([DECL_STRTOL_0], [1], [Declaration style])
    AC_DEFINE([DECL_SYSLOG_0], [1], [Declaration style])
    AC_DEFINE([DECL_TIME_0], [1], [Declaration style])
    AC_DEFINE([DECL_TIMEOFDAY_0], [1], [Declaration style])
    AC_DEFINE([DECL_TOLOWER_0], [1], [Declaration style])
    ;;
 *-*-solaris2*)
    AC_DEFINE([DECL_MKSTEMP_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETPRIORITY_1], [1], [Declaration style])
    case "$host" in
     *-*-solaris2.4)
        AC_DEFINE([DECL_TIMEOFDAY_0], [1], [Declaration style])
	;;
    esac
    ;;
 *-*-sunos4*)
    AC_DEFINE([DECL_ADJTIME_0], [1], [Declaration style])
    AC_DEFINE([DECL_BCOPY_0], [1], [Declaration style])
    AC_DEFINE([DECL_BZERO_0], [1], [Declaration style])
    AC_DEFINE([DECL_IOCTL_0], [1], [Declaration style])
    AC_DEFINE([DECL_IPC_0], [1], [Declaration style])
    AC_DEFINE([DECL_MEMMOVE_0], [1], [Declaration style])
    AC_DEFINE([DECL_MKTEMP_0], [1], [Declaration style])
    AC_DEFINE([DECL_MKSTEMP_0], [1], [Declaration style])
    AC_DEFINE([DECL_RENAME_0], [1], [Declaration style])
    AC_DEFINE([DECL_SELECT_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETITIMER_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETPRIORITY_0], [1], [Declaration style])
    AC_DEFINE([DECL_SIGVEC_0], [1], [Declaration style])
    case "`basename $ac_cv_prog_CC`" in
     acc*) ;;
     *) AC_DEFINE([DECL_STDIO_0], [1], [Declaration style])
	;;
    esac
    AC_DEFINE([DECL_STRTOL_0], [1], [Declaration style])
    AC_DEFINE([DECL_SYSLOG_0], [1], [Declaration style])
    AC_DEFINE([DECL_TIME_0], [1], [Declaration style])
    AC_DEFINE([DECL_TIMEOFDAY_0], [1], [Declaration style])
    AC_DEFINE([DECL_TOLOWER_0], [1], [Declaration style])
    AC_DEFINE([DECL_TOUPPER_0], [1], [Declaration style])
    AC_DEFINE([DECL_STRERROR_0], [1], [Declaration style])
    ;;
 *-*-ultrix4*)
    AC_DEFINE([DECL_ADJTIME_0], [1], [Declaration style])
    AC_DEFINE([DECL_BZERO_0], [1], [Declaration style])
    AC_DEFINE([DECL_CFSETISPEED_0], [1], [Declaration style])
    AC_DEFINE([DECL_IOCTL_0], [1], [Declaration style])
    AC_DEFINE([DECL_IPC_0], [1], [Declaration style])
    AC_DEFINE([DECL_MKTEMP_0], [1], [Declaration style])
    AC_DEFINE([DECL_NLIST_0], [1], [Declaration style])
    AC_DEFINE([DECL_PLOCK_0], [1], [Declaration style])
    AC_DEFINE([DECL_SELECT_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETITIMER_0], [1], [Declaration style])
    AC_DEFINE([DECL_SETPRIORITY_0], [1], [Declaration style])
    AC_DEFINE([DECL_STIME_0], [1], [Declaration style])
    AC_DEFINE([DECL_SYSLOG_0], [1], [Declaration style])
    AC_DEFINE([DECL_TIMEOFDAY_0], [1], [Declaration style])
    ;;
esac

case "$host" in
 *-*-sco3.2*)
    AC_DEFINE([TERMIOS_NEEDS__SVID3], [1],
	[Do we need to #define _SVID3 when we #include <termios.h>?])
    ;;
esac

case "$host" in
 *-*-hpux[[567]]*)
    AC_DEFINE([NEED_RCVBUF_SLOP], [1],
	[Do we need extra room for SO_RCVBUF? (HPUX < 8)])
esac

dnl Using AC_CACHE_CHECK to honor preset ntp_cv_var_open_bcast_socket
AC_CACHE_CHECK(
    [if we will open the broadcast socket],
    [ntp_cv_var_open_bcast_socket],
    [
	ans=yes
	case "$host" in
	 *-*-domainos)
	    ans=no
	esac
	ntp_cv_var_open_bcast_socket=$ans
    ]
)
case "$ntp_cv_var_open_bcast_socket" in
 yes)
    AC_DEFINE([OPEN_BCAST_SOCKET], [1],
	[Should we open the broadcast socket?])
esac

case "$host" in
 *-*-hpux*)
    AC_DEFINE([NEED_HPUX_FINDCONFIG], [1],
	[Do we want the HPUX FindConfig()?])
esac

dnl using AC_CACHE_CHECK to honor preset $ntp_cv_arg_setpgrp_negpid
AC_CACHE_CHECK(
    [if process groups are set with -pid],
    [ntp_cv_arg_setpgrp_negpid],
    [
	case "$host" in
	 *-*-hpux[[567]]*)
	    ans=no
	    ;;
	 *-*-hpux*)
	    ans=yes
	    ;;
	 *-*-*linux*)
	    ans=yes
	    ;;
	 *-*-sunos3*)
	    ans=yes
	    ;;
	 *-*-ultrix2*)
	    ans=yes
	    ;;
	 *)
	    ans=no
	    ;;
	esac
	ntp_cv_arg_setpgrp_negpid=$ans
    ]
)
case "$ntp_cv_arg_setpgrp_negpid" in
 yes)
    AC_DEFINE([UDP_BACKWARDS_SETOWN], [1],
	[Do we set process groups with -pid?])
esac

AC_CACHE_CHECK(
    [if we need a ctty for F_SETOWN],
    [ntp_cv_func_ctty_for_f_setown],
    [
	case "$host" in
	 *-*-bsdi[23]*)
	    ans=yes
	    ;;
	 *-*-freebsd*)
	    ans=yes
	    ;;
	# NetBSD versions prior to 3.99.8 require a CTTY for F_SETOWN,
	# while later versions will fail a ioctl(TIOCSCTTY, 0) call in
	# some cases and so should not have USE_FSETOWNCTTY.  "netbsd"
	# in $host may be followed by "aout", "ecoff", or "elf".
	 *-*-netbsd*[a-z]3.[0-8]*|*-*-netbsd*[a-z][0-2].*|*-*-netbsd*[a-z]3.99.[0-7])
	    ans=yes
	    ;;
	 *-*-netbsd3.[0-8]*|*-*-netbsd[0-2].*|*-*-netbsd3.99.[0-7])
	    ans=yes
	    ;;
	 *-*-openbsd*)
	    ans=yes
	    ;;
	 *-*-osf*)
	    ans=yes
	    ;;
	 *-*-darwin*)
	    ans=yes
	    ;;
	 *) 
	    ans=no
	    ;;
	esac
	ntp_cv_func_ctty_for_f_setown=$ans
    ]
)
case "$ntp_cv_func_ctty_for_f_setown" in
 yes)
    AC_DEFINE([USE_FSETOWNCTTY], [1], [Must we have a CTTY for fsetown?])
esac

AC_CACHE_CHECK(
    [if the OS clears cached routes when more specifics become available],
    [ntp_cv_os_routeupdates],
    [
	case "$host" in
	 *-*-netbsd*)
	    ans=yes
	    ;;
	 *) 
	    ans=no
	    ;;
	esac
	ntp_cv_os_routeupdates=$ans
    ]
)
case "$ntp_cv_os_routeupdates" in
 yes)
    AC_DEFINE([OS_MISSES_SPECIFIC_ROUTE_UPDATES], [1],
	[need to recreate sockets on changed routing?])
esac

AC_CACHE_CHECK(
    [if the wildcard socket needs REUSEADDR to bind other addresses],
    [ntp_cv_os_wildcardreuse],
    [
	case "$host" in
	 *-*-*linux*)
	    ans=yes
	    ;;
	 *) ans=no
	    ;;
	esac
	ntp_cv_os_wildcardreuse=$ans
    ]
)
case "$ntp_cv_os_wildcardreuse" in
 yes)
    AC_DEFINE([OS_NEEDS_REUSEADDR_FOR_IFADDRBIND], [1],
	[wildcard socket needs REUSEADDR to bind interface addresses])
esac

AC_MSG_CHECKING([if we'll use clock_settime or settimeofday or stime])
ntp_warning='GRONK'
ans=none
case "$ac_cv_func_clock_settime$ac_cv_func_settimeofday$ac_cv_func_stime" in
 yes*)
    ntp_warning=''
    ans='clock_settime()'
    ;;
 noyes*)
    ntp_warning='But clock_settime() would be better (if we had it)'
    ans='settimeofday()'
    ;;
 nonoyes)
    ntp_warning='Which is the worst of the three'
    ans='stime()'
    ;;
 *) 
    case "$build" in
     $host) 
	ntp_warning='Which leaves us with nothing to use!'
    esac
esac
AC_MSG_RESULT([$ans])
case "$ntp_warning" in
 '')
    ;;
 *)
    AC_MSG_WARN([*** $ntp_warning ***])
    ;;
esac

AC_CACHE_CHECK(
    [for SIGIO],
    [ntp_cv_hdr_def_sigio],
    [AC_EGREP_CPP(
	[yes],
	[
	    #include <signal.h>

	    #ifdef SIGIO
		yes
	    #endif
	],
	[ntp_cv_hdr_def_sigio=yes],
	[ntp_cv_hdr_def_sigio=no]
    )]
 )

dnl Override those system that have a losing SIGIO
AC_CACHE_CHECK(
    [if we want to use signalled IO],
    [ntp_cv_var_signalled_io],
    [
	ans=no
	case "$ntp_cv_hdr_def_sigio" in
	 yes)
	    ans=yes
	    case "$host" in
	     alpha*-dec-osf4*|alpha*-dec-osf5*)
		ans=no
		;;
	     *-convex-*)
		ans=no
		;;
	     *-dec-*)
		ans=no
		;;
	     *-pc-cygwin*)
		ans=no
		;;
	     *-sni-sysv*)
		ans=no
		;;
	     *-univel-sysv*)
		ans=no
		;;
	     *-*-irix6*)
		ans=no
		;;
	     *-*-freebsd*)
		ans=no
		;;
	     *-*-*linux*)
		ans=no
		;;
	     *-*-unicosmp*)
		ans=no
		;;
	     *-*-kfreebsd*)
		ans=no
		;;
	     m68k-*-mint*)
		ans=no
		;;
	    esac
	    ;;
	esac
	ntp_cv_var_signalled_io=$ans
    ]
)
case "$ntp_cv_var_signalled_io" in
 yes)
    AC_DEFINE([HAVE_SIGNALED_IO], [1],
	[Can we use SIGIO for tcp and udp IO?])
esac

AC_CACHE_CHECK(
    [for SIGPOLL],
    [ntp_cv_hdr_def_sigpoll],
    [AC_EGREP_CPP(
	[yes],
	[
	    #include <signal.h>
	    
	    #ifdef SIGPOLL
		yes
	    #endif
	],
	[ntp_cv_hdr_def_sigpoll=yes],
	[ntp_cv_hdr_def_sigpoll=no]
    )]
)

AC_CACHE_CHECK(
    [for SIGSYS],
    [ntp_cv_hdr_def_sigsys],
    [AC_EGREP_CPP(
	[yes],
	[
	    #include <signal.h>

	    #ifdef SIGSYS
		yes
	    #endif
	],
	[ntp_cv_hdr_def_sigsys=yes],
	[ntp_cv_hdr_def_sigsys=no]
    )]
)

AC_CACHE_CHECK(
    [if we can use SIGPOLL for UDP I/O],
    [ntp_cv_var_use_udp_sigpoll],
    [
	ans=no
	case "$ntp_cv_hdr_def_sigpoll" in
	 yes)
	    case "$host" in
	     mips-sgi-irix*)
		ans=no
		;;
	     vax-dec-bsd)
		ans=no
		;;
	     *-pc-cygwin*)
		ans=no
		;;
	     *-sni-sysv*)
		ans=no
		;;
	     *-*-aix[[456]]*)
		ans=no
		;;
	     *-*-hpux*)
		ans=no
		;;
	     *-*-*linux*)
		ans=no
		;;
	     *-*-osf*)
		ans=no
		;;
	     *-*-qnx*)
		ans=no
		;;
	     *-*-sunos*)
		ans=no
		;;
	     *-*-solaris*)
		ans=no
		;;
	     *-*-ultrix*)
		ans=no
		;;
	     *-*-unicosmp*)
		ans=no
		;;
	     *-*-kfreebsd*)
		ans=no
		;;
	     *) ans=yes
		;;
	    esac
	    ;;
	esac
	ntp_cv_var_use_udp_sigpoll=$ans
    ]
)
case "$ntp_cv_var_use_udp_sigpoll" in
 yes)
    AC_DEFINE([USE_UDP_SIGPOLL], [1], [Can we use SIGPOLL for UDP?])
esac

AC_CACHE_CHECK(
    [if we can use SIGPOLL for TTY I/O],
    [ntp_cv_var_use_tty_sigpoll],
    [
	ans=no
	case "$ntp_cv_hdr_def_sigpoll" in
	 yes)
	    case "$host" in
	     mips-sgi-irix*)
		ans=no
		;;
	     vax-dec-bsd)
		ans=no
		;;
	     *-pc-cygwin*)
		ans=no
		;;
	     *-sni-sysv*)
		ans=no
		;;
	     *-*-aix[[456]]*)
		ans=no
		;;
	     *-*-hpux*)
		ans=no
		;;
	     *-*-*linux*)
		ans=no
		;;
	     *-*-osf*)
		ans=no
		;;
	     *-*-sunos*)
		ans=no
		;;
	     *-*-ultrix*)
		ans=no
		;;
	     *-*-qnx*)
		ans=no
		;;
	     *-*-unicosmp*)
		ans=no
		;;
	     *-*-kfreebsd*)
		ans=no
		;;
	     *) ans=yes
		;;
	    esac
	    ;;
	esac
	ntp_cv_var_use_tty_sigpoll=$ans
    ]
)
case "$ntp_cv_var_use_tty_sigpoll" in
 yes)
    AC_DEFINE([USE_TTY_SIGPOLL], [1], [Can we use SIGPOLL for tty IO?])
esac

case "$host" in
 *-*-aix*)
    AC_DEFINE([NLIST_EXTRA_INDIRECTION], [1],
	[Might nlist() values require an extra level of indirection (AIX)?])
esac

AC_CACHE_CHECK(
    [for a minimum recommended value of tickadj],
    [ntp_cv_var_min_rec_tickadj],
    [
	ans=no
	case "$host" in
	 *-*-aix*)
	    ans=40
	    ;;
	esac
	ntp_cv_var_min_rec_tickadj=$ans
    ]
)
case "$ntp_cv_var_min_rec_tickadj" in
 ''|no)
    ;;
 *)
    AC_DEFINE_UNQUOTED([MIN_REC_TICKADJ], [$ntp_cv_var_min_rec_tickadj],
	[Should we recommend a minimum value for tickadj?])
esac

AC_CACHE_CHECK(
    [if the TTY code permits PARENB and IGNPAR],
    [ntp_cv_no_parenb_ignpar],
    [
	ans=no
	case "$host" in
	 i?86-*-*linux*)
	    ans=yes
	    ;;
	 mips-sgi-irix*)
	    ans=yes
	    ;;
	 i?86-*-freebsd[[123]].*)
	    ;;
	 i?86-*-freebsd*)
	    ans=yes
	    ;;
	 *-*-unicosmp*)
	    ans=yes
	    ;;
	esac
	ntp_cv_no_parenb_ignpar=$ans
    ]
)
case "$ntp_cv_no_parenb_ignpar" in
 yes)
    AC_DEFINE([NO_PARENB_IGNPAR], [1],
	[Is there a problem using PARENB and IGNPAR?])
esac

AC_MSG_CHECKING([if we're including ntpd debugging code])
AC_ARG_ENABLE(
    [debugging],
    [AS_HELP_STRING(
        [--enable-debugging],
        [+ include ntpd debugging code]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=yes]
)
case "$ntp_ok" in
 yes)
    AC_DEFINE([DEBUG], [1], [Enable ntpd debugging code?])
esac
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([if we're including processing time debugging code])
AC_ARG_ENABLE(
    [debug-timing],
    [AS_HELP_STRING(
	[--enable-debug-timing],
	[- include processing time debugging code (costs performance)]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=no]
)
case "$ntp_ok" in
 yes)
    AC_DEFINE([DEBUG_TIMING], [1], [Enable processing time debugging?])
esac
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([for a the number of minutes in a DST adjustment])
AC_ARG_ENABLE(
    [dst-minutes],
    [AS_HELP_STRING(
	[--enable-dst-minutes],
	[n minutes per DST adjustment @<:@60@:>@])   dnl @<:@ is [, @:>@ is ]
    ],
    [ans=$enableval],
    [ans=60]
)
AC_DEFINE_UNQUOTED([DSTMINUTES], [$ans],
    [The number of minutes in a DST adjustment])
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([if ntpd will retry permanent DNS failures])
AC_ARG_ENABLE(
    [ignore-dns-errors],
    [AS_HELP_STRING(
	[--enable-ignore-dns-errors], 
	[- retry DNS queries on any error]
    )],
    [ans=$enableval],
    [ans=no]
)
case "$ans" in
 yes)
    AC_DEFINE([IGNORE_DNS_ERRORS], [1],
	[Retry queries on _any_ DNS error?])
esac
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([if ntpd will use the deferred DNS lookup path])
AC_ARG_ENABLE(
    [force-defer-DNS],
    [AS_HELP_STRING(
	[--enable-force-defer-DNS],
	[- force all DNS lookups to take the deferred path]
    )],
    [ans=$enableval],
    [ans=no]
)
case "$ans" in
 yes)
    AC_DEFINE([FORCE_DEFER_DNS], [1], [Force deferred DNS lookups?])
esac
AC_MSG_RESULT([$ans])

case "$ac_cv_header_sys_sio_h" in
 yes)
    AC_CACHE_CHECK(
	[sys/sio.h for TIOCDCDTIMESTAMP],
	[ntp_cv_hdr_def_tiocdcdtimestamp],
	[AC_EGREP_CPP(
	    [yes],
	    [
		#include <sys/sio.h>

		#ifdef TIOCDCDTIMESTAMP
		    yes
		#endif
	    ],
	    [ntp_cv_hdr_def_tiocdcdtimestamp=yes],
	    [ntp_cv_hdr_def_tiocdcdtimestamp=no]
	)]
    )
esac

AC_CACHE_CHECK(
    [if we have the tty_clk line discipline/streams module],
    [ntp_cv_tty_clk],
    [
	case "$ac_cv_header_sys_clkdefs_h$ntp_cv_hdr_def_tiocdcdtimestamp" in
	 *yes*)
	    ntp_cv_tty_clk=yes
	    ;;
	 *)
	    ntp_cv_tty_clk=no
	    ;;
	esac
    ]
)
case "$ntp_cv_tty_clk" in
 yes)
    AC_DEFINE([TTYCLK], [1],
	[Do we have the tty_clk line discipline/streams module?])
esac

AC_MSG_CHECKING([for the ppsclock streams module])
case "$ntp_cv_struct_ppsclockev" in
 yes)
    ans=yes
    AC_DEFINE([PPS], [1],
	[Do we have the ppsclock streams module?])
    ;;
 *)
    ans=no
esac
AC_MSG_RESULT([$ans])

AC_CACHE_CHECK(
    [for multicast IP support],
    [ntp_cv_multicast],
    [
	ntp_cv_multicast=no
	case "$host" in
	 i386-sequent-sysv4)
	    ;;
	 *)
	    AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
		    [[
			#ifdef HAVE_NETINET_IN_H
			#include <netinet/in.h>
			#endif
		    ]],
		    [[
			struct ip_mreq ipmr;
			ipmr.imr_interface.s_addr = 0;
		    ]]
		)],
		[ntp_cv_multicast=yes],
		[]
	    )
	esac
    ]
)
case "$ntp_cv_multicast" in
 yes)
    AC_DEFINE([MCAST], [1], [Does the target support multicast IP?])
    AC_CACHE_CHECK(
	[arg type needed for setsockopt() IP*_MULTICAST_LOOP],
	[ntp_cv_typeof_ip_multicast_loop],
	[
	    case "$host" in
	     *-*-netbsd*|*-*-*linux*)
		ntp_cv_typeof_ip_multicast_loop=u_int
		;;
	     *)
		ntp_cv_typeof_ip_multicast_loop=u_char
	    esac
	]
    )
    AC_DEFINE_UNQUOTED([TYPEOF_IP_MULTICAST_LOOP],
	[$ntp_cv_typeof_ip_multicast_loop],
	[What type to use for setsockopt])
esac

AC_CACHE_CHECK(
    [availability of ntp_{adj,get}time()],
    [ntp_cv_var_ntp_syscalls],
    [
	ntp_cv_var_ntp_syscalls=no
	case "$ac_cv_func_ntp_adjtime$ac_cv_func_ntp_gettime$ac_cv_func___adjtimex" in
	 yesyes*)
	    ntp_cv_var_ntp_syscalls=libc
	    ;;
	 *yes)
	    ntp_cv_var_ntp_syscalls=inline
	    ;;
	 *) 
	    AC_EGREP_CPP(
		[yes],
		[
		    #include <sys/syscall.h>
		    
		    #if defined(SYS_ntp_gettime) && defined(SYS_ntp_adjtime)
			yes
		    #endif
		],
		[ntp_cv_var_ntp_syscalls=kernel]
	    )
	    ;;
	 esac
    ]
)
case "$ntp_cv_var_ntp_syscalls" in
 libc)
    AC_DEFINE([NTP_SYSCALLS_LIBC], [1],
	[Do we have ntp_{adj,get}time in libc?])
    ;;
 kernel)
    AC_DEFINE([NTP_SYSCALLS_STD], [1],
	[Do we have ntp_{adj,get}time in the kernel?])
    ;;
esac

AC_CACHE_CHECK(
    [if sys/timex.h has STA_FLL],
    [ntp_cv_var_sta_fll],
    [AC_EGREP_CPP(
	[yes],
	[
	    #include <sys/timex.h>

	    #ifdef STA_FLL
		yes
	    #endif
	],
	[ntp_cv_var_sta_fll=yes],
	[ntp_cv_var_sta_fll=no]
    )]
)

AC_CACHE_CHECK(
    [if we have kernel PLL support],
    [ntp_cv_var_kernel_pll],
    [dnl ntp_cv_var_ntp_syscalls is {no,libc,kernel}
	case "$ac_cv_header_sys_timex_h$ntp_cv_struct_ntptimeval$ntp_cv_var_sta_fll$ntp_cv_var_ntp_syscalls" in
	 *no*)
	    ntp_cv_var_kernel_pll=no
	    ;;
	 *) ntp_cv_var_kernel_pll=yes
	    ;;
	esac
    ]
)
case "$ntp_cv_var_kernel_pll" in
 yes)
    AC_DEFINE([KERNEL_PLL], [1],
	[Does the kernel support precision time discipline?])
esac

AC_CACHE_CHECK(
    [if SIOCGIFCONF returns buffer size in the buffer],
    [ntp_cv_size_returned_in_buffer],
    [
	ans=no
	case "$host" in
	 *-fujitsu-uxp*)
	    ans=yes
	    ;;
	 *-ncr-sysv4*)
	    ans=yes
	    ;;
	 *-univel-sysv*)
	    ans=yes
	    ;;
	esac
	ntp_cv_size_returned_in_buffer=$ans
    ]
)
case "$ntp_cv_size_returned_in_buffer" in
 yes)
    AC_DEFINE([SIZE_RETURNED_IN_BUFFER], [1],
	[Does SIOCGIFCONF return size in the buffer?])
esac

# Check for ioctls TIOCGPPSEV
AC_MSG_CHECKING([for TTY PPS ioctl TIOCGPPSEV])
case "$ac_cv_header_termios_h" in
 yes)
    AC_EGREP_CPP(
	[yes],
	[
	    #include <termios.h>

	    #ifdef TIOCGPPSEV
		yes
	    #endif
	],
	[ntp_ok=yes],
	[ntp_ok=no]
    )
    ;;
 *)
    ntp_ok=no
    ;;
esac
case "$ntp_ok" in
 yes)
    AC_DEFINE([HAVE_TIOCGPPSEV], [1],
	[Do we have the TIOCGPPSEV ioctl (Solaris)?])
esac
AC_MSG_RESULT([$ntp_ok])

# Check for ioctls TIOCSPPS
AC_MSG_CHECKING([for TTY PPS ioctl TIOCSPPS])
case "$ac_cv_header_termios_h" in
 yes)
    AC_EGREP_CPP(
	[yes],
	[
	    #include <termios.h>

	    #ifdef TIOCSPPS
		yes
	    #endif
	 ],
	 [ntp_ok=yes],
	 [ntp_ok=no]
    )
    ;;
 *)
    ntp_ok=no
    ;;
esac
case "$ntp_ok" in
 yes)
    AC_DEFINE([HAVE_TIOCSPPS], [1],
	[Do we have the TIOCSPPS ioctl (Solaris)?])
esac
AC_MSG_RESULT([$ntp_ok])

# Check for ioctls CIOGETEV
AC_MSG_CHECKING([for TTY PPS ioctl CIOGETEV])
case "$ac_cv_header_sys_ppsclock_h" in
 yes)
    AC_EGREP_CPP(
	[yes],
	[
	    #include <sys/ppsclock.h>

	    #ifdef CIOGETEV
		yes
	    #endif
	],
	[ntp_ok=yes],
	[ntp_ok=no]
    )
    ;;
 *)
    ntp_ok=no
    ;;
esac
case "$ntp_ok" in
 yes)
    AC_DEFINE([HAVE_CIOGETEV], [1],
	[Do we have the CIOGETEV ioctl (SunOS, Linux)?])
esac
AC_MSG_RESULT([$ntp_ok])

# ATOM/PPSAPI stuff.

ntp_atom_ok=yes

# Check for header timepps.h, if found then we have PPS API (Draft RFC) stuff.

# The PPSAPI headers need "inline" ($ac_cv_c_inline='inline')
# The PPSAPI needs struct timespec.
# The PPSAPI also needs a timepps header.

case "$ac_cv_c_inline$ntp_cv_struct_timespec" in
 inlineyes)
    case "$ac_cv_header_timepps_h$ac_cv_header_sys_timepps_h$host_os" in
     *yes* | *sunos* | *solaris* | *sco* | *netbsd* )
	AC_DEFINE(HAVE_PPSAPI, 1, [Do we have the PPS API per the Draft RFC?])
	ntp_jupiter_ok=yes
	ntp_oncore_ok=yes
	ntp_parse_ok=yes
	ntp_ripe_ncc_ok=yes
	;;
    esac
    ;;
esac

# Check for ioctls TIOCGSERIAL, TIOCSSERIAL, ASYNC_PPS_CD_POS, ASYNC_PPS_CD_NEG
AC_CHECK_HEADER([linux/serial.h])
case "$ac_cv_header_sys_ppsclock_h$ac_cv_header_linux_serial_h" in
  yesyes)
    AC_MSG_CHECKING([ioctl TIOCGSERIAL, TIOCSSERIAL, ASYNC_PPS_CD_POS, ASYNC_PPS_CD_NEG])
    AC_EGREP_CPP(
	[yes],
	[
	    #include <sys/time.h>
	    typedef int u_int;
	    #include <sys/ppsclock.h>
	    #include <linux/serial.h>

	    #ifdef TIOCGSERIAL
	    #ifdef TIOCSSERIAL
	    #ifdef ASYNC_PPS_CD_POS
	    #ifdef ASYNC_PPS_CD_NEG
	    #ifdef CIOGETEV
		 yes
	    #endif
	    #endif
	    #endif
	    #endif
	    #endif
	],
	[ntp_ok=yes],
	[ntp_ok=no]
    )
    AC_MSG_RESULT([$ntp_ok])
    ;;
  *)
    ntp_ok=no
    ;;
esac
case "$ntp_ok" in
 yes)
    AC_DEFINE([HAVE_TIO_SERIAL_STUFF], 1,
	[Do we have the TIO serial stuff?])
esac

# Check for SHMEM_STATUS support
AC_MSG_CHECKING([SHMEM_STATUS support])
case "$ac_cv_header_sys_mman_h" in
 yes)
    ntp_ok=yes
    ;;
 *)
    ntp_ok=no
    ;;
esac
case "$ntp_ok" in
 yes)
    AC_DEFINE([ONCORE_SHMEM_STATUS], [1],
	[Do we have support for SHMEM_STATUS?])
esac
AC_MSG_RESULT([$ntp_ok])

ntp_refclock=no

# HPUX only, and by explicit request
AC_MSG_CHECKING([Datum/Bancomm bc635/VME interface])
AC_ARG_ENABLE(
    [BANCOMM],
    [AS_HELP_STRING(
	[--enable-BANCOMM], 
	[- Datum/Bancomm bc635/VME interface]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=no]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_BANC], [1], [Datum/Bancomm bc635/VME interface?])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$host" in
 yes*-*-hpux*) ;;
 yes*) AC_MSG_WARN([*** But the expected answer is... no ***]) ;;
esac

#HPUX only, and only by explicit request
AC_MSG_CHECKING([TrueTime GPS receiver/VME interface])
AC_ARG_ENABLE(
    [GPSVME],
    [AS_HELP_STRING(
	[--enable-GPSVME],
	[- TrueTime GPS receiver/VME interface]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=no]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_GPSVME], 1, [TrueTime GPS receiver/VME interface?])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$host" in
 yes*-*-hpux*) ;;
 yes*) AC_MSG_WARN([*** But the expected answer is... no ***]) ;;
esac

AC_MSG_CHECKING([for PCL720 clock support])
case "$ac_cv_header_machine_inline_h$ac_cv_header_sys_pcl720_h$ac_cv_header_sys_i8253_h" in
 yesyesyes)
    AC_DEFINE([CLOCK_PPS720], 1, [PCL 720 clock support])
    ans=yes
    ;;
 *)
    ans=no
    ;;
esac
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([for default inclusion of all suitable non-PARSE clocks])
AC_ARG_ENABLE(
    [all-clocks],
    [AS_HELP_STRING(
	[--enable-all-clocks],
	[+ include all suitable non-PARSE clocks:]
    )],
    [ntp_eac=$enableval],
    [ntp_eac=yes]
)
AC_MSG_RESULT([$ntp_eac])

# HMS: Should we also require ntp_parse_ok?
AC_MSG_CHECKING([if we have support for PARSE clocks])
case "$ntp_atom_ok$ac_cv_header_termio_h$ac_cv_header_termios_h" in
 yes*yes*)
    ntp_canparse=yes
    ;;
 *) ntp_canparse=no
    ;;
esac
AC_MSG_RESULT([$ntp_canparse])

AC_MSG_CHECKING([if we have support for audio clocks])
case "$ac_cv_header_sun_audioio_h$ac_cv_header_sys_audioio_h$ac_cv_header_machine_soundcard_h$ac_cv_header_sys_soundcard_h" in
 *yes*)
    ntp_canaudio=yes
    AC_DEFINE([HAVE_AUDIO], [], [Do we have audio support?])
    ;;
 *) ntp_canaudio=no ;;
esac
AC_MSG_RESULT([$ntp_canaudio])

AC_MSG_CHECKING([if we have support for the SHM refclock interface])
case "$ac_cv_header_sys_ipc_h$ac_cv_header_sys_shm_h" in
 yesyes)
    ntp_canshm=yes
    ;;
 *) ntp_canshm=no ;;
esac
AC_MSG_RESULT([$ntp_canshm])

# Test for termios TIOCMBIS modem control (ACTS, Heath, Palisade)
AC_CACHE_CHECK(
    [for termios modem control],
    [ntp_cv_modem_control],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#ifdef HAVE_UNISTD_H
		# include <unistd.h>
		#endif
		#ifdef HAVE_TERMIOS_H
		# include <termios.h>
		#endif
		#ifdef HAVE_SYS_IOCTL_H
		# include <sys/ioctl.h>
		#endif
	    ]],
	    [[
		int	dtr = TIOCM_DTR;
		
		ioctl(1, TIOCMBIS, (char *)&dtr);
	    ]]
	)],
	[ntp_cv_modem_control=yes],
	[ntp_cv_modem_control=no]
    )]
)
case "$ntp_eac::$ntp_cv_modem_control" in
 yes::yes)
    ntp_enable_all_modem_control_clocks=yes
    ;;
 *)
    ntp_enable_all_modem_control_clocks=no
    ;;
esac

# Requires modem control
AC_MSG_CHECKING([ACTS modem service])
AC_ARG_ENABLE(
    [ACTS],
    [AS_HELP_STRING(
	[--enable-ACTS],
	[s ACTS modem service]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_enable_all_modem_control_clocks]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_ACTS], [1], [ACTS modem service])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Arbiter 1088A/B GPS receiver])
AC_ARG_ENABLE(
    [ARBITER],
    [AS_HELP_STRING(
	[--enable-ARBITER],
	[+ Arbiter 1088A/B GPS receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_ARBITER], [1], [Arbiter 1088A/B GPS receiver])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Arcron MSF receiver])
AC_ARG_ENABLE(
    [ARCRON_MSF],
    [AS_HELP_STRING(
	[--enable-ARCRON-MSF], 
	[+ Arcron MSF receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_ARCRON_MSF], [1], [ARCRON support?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Austron 2200A/2201A GPS receiver])
AC_ARG_ENABLE(
    [AS2201],
    [AS_HELP_STRING(
	[--enable-AS2201],
	[+ Austron 2200A/2201A GPS receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_AS2201], [1], [Austron 2200A/2201A GPS receiver?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([ATOM PPS interface])
AC_ARG_ENABLE(
    [ATOM],
    [AS_HELP_STRING(
	[--enable-ATOM],
	[s ATOM PPS interface]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
case "$ntp_atom_ok" in
 no) ntp_ok=no ;;
esac
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_ATOM], [1], [PPS interface?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Chrono-log K-series WWVB receiver])
AC_ARG_ENABLE(
    [CHRONOLOG],
    [AS_HELP_STRING(
	[--enable-CHRONOLOG],
	[+ Chrono-log K-series WWVB receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_CHRONOLOG], [1], [Chronolog K-series WWVB receiver?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([CHU modem/decoder])
AC_ARG_ENABLE(
    [CHU],
    [AS_HELP_STRING(
	[--enable-CHU],
	[+ CHU modem/decoder]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_CHU], [1], [CHU modem/decoder])
fi
AC_MSG_RESULT([$ntp_ok])
ntp_refclock_chu=$ntp_ok

AC_MSG_CHECKING([CHU audio/decoder])
AC_ARG_ENABLE(
    [AUDIO-CHU],
    [AS_HELP_STRING(
	[--enable-AUDIO-CHU],
	[s CHU audio/decoder]
    )],
    [ntp_ok=$enableval],
    [
	case "$ntp_eac$ntp_refclock_chu$ntp_canaudio" in
	 *no*)	ntp_ok=no  ;;
	 *)	ntp_ok=yes ;;
	esac
    ]
)
if test "$ntp_ok" = "yes"; then
    AC_DEFINE([AUDIO_CHU], [1], [CHU audio/decoder?])
fi
AC_MSG_RESULT([$ntp_ok])
# We used to check for sunos/solaris target...
case "$ntp_ok$ntp_refclock_chu$ntp_canaudio" in
 yes*no*) AC_MSG_WARN([*** But the expected answer is...no ***])
esac

# Not under HP-UX
AC_MSG_CHECKING([Datum Programmable Time System])
AC_ARG_ENABLE(
    [DATUM],
    [AS_HELP_STRING(
	[--enable-DATUM],
	[s Datum Programmable Time System]
    )],
    [ntp_ok=$enableval],
    [
	case "$ac_cv_header_termios_h" in
	 yes)
	    ntp_ok=$ntp_eac
	    ;;
	 *) ntp_ok=no
	    ;;
	esac
    ]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_DATUM], [1], [Datum Programmable Time System?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Dumb generic hh:mm:ss local clock])
AC_ARG_ENABLE(
    [DUMBCLOCK],
    [AS_HELP_STRING(
	[--enable-DUMBCLOCK],
	[+ Dumb generic hh:mm:ss local clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_DUMBCLOCK], [1], [Dumb generic hh:mm:ss local clock?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Forum Graphic GPS])
AC_ARG_ENABLE(
    [FG],
    [AS_HELP_STRING(
	[--enable-FG],
	[+ Forum Graphic GPS]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_FG], [1], [Forum Graphic GPS datating station driver?])
fi
AC_MSG_RESULT([$ntp_ok])

# Requires modem control
AC_MSG_CHECKING([Heath GC-1000 WWV/WWVH receiver])
AC_ARG_ENABLE(
    [HEATH],
    [AS_HELP_STRING(
	[--enable-HEATH],
	[s Heath GC-1000 WWV/WWVH receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_enable_all_modem_control_clocks]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_HEATH], [1], [Heath GC-1000 WWV/WWVH receiver?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([for hopf serial clock device])
AC_ARG_ENABLE(
    [HOPFSERIAL],
    [AS_HELP_STRING(
	[--enable-HOPFSERIAL],
	[+ hopf serial clock device]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_HOPF_SERIAL], [1], [HOPF serial clock device?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([for hopf PCI clock 6039])
AC_ARG_ENABLE(
    [HOPFPCI],
    [AS_HELP_STRING(
	[--enable-HOPFPCI],
	[+ hopf 6039 PCI board]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_HOPF_PCI], [1], [HOPF PCI clock device?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([HP 58503A GPS receiver])
AC_ARG_ENABLE(
    [HPGPS],
    [AS_HELP_STRING(
	[--enable-HPGPS],
	[+ HP 58503A GPS receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_HPGPS], 1, [HP 58503A GPS receiver?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([IRIG audio decoder])
AC_ARG_ENABLE(
    [IRIG],
    [AS_HELP_STRING(
	[--enable-IRIG],
	[s IRIG audio decoder]
    )],
    [ntp_ok=$enableval],
    [
	case "$ntp_eac$ntp_canaudio" in
	 *no*)	ntp_ok=no  ;;
	 *)	ntp_ok=yes ;;
	esac
    ]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_IRIG], [1], [IRIG audio decoder?])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canaudio" in
 yesno) AC_MSG_WARN([*** But the expected answer is... no ***])
esac

AC_MSG_CHECKING([for JJY receiver])
AC_ARG_ENABLE(
    [JJY],
    [AS_HELP_STRING(
	[--enable-JJY],
	[+ JJY receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_JJY], [1], [JJY receiver?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Rockwell Jupiter GPS receiver])
AC_ARG_ENABLE(
    [JUPITER],
    [AS_HELP_STRING(
	[--enable-JUPITER],
	[s Rockwell Jupiter GPS receiver]
    )],
    [ntp_ok=$enableval], [ntp_ok=$ntp_eac])
case "$ntp_jupiter_ok" in
 no) ntp_ok=no ;;
esac
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_JUPITER], [1], [Rockwell Jupiter GPS clock?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Leitch CSD 5300 Master Clock System Driver])
AC_ARG_ENABLE(
    [LEITCH],
    [AS_HELP_STRING(
	[--enable-LEITCH],
	[+ Leitch CSD 5300 Master Clock System Driver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_LEITCH], [1],
	[Leitch CSD 5300 Master Clock System Driver?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([local clock reference])
AC_ARG_ENABLE(
    [LOCAL-CLOCK],
    [AS_HELP_STRING(
	[--enable-LOCAL-CLOCK],
	[+ local clock reference]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_LOCAL], [1], [local clock reference?])
fi
AC_MSG_RESULT([$ntp_ok])

dnl Bug 340: longstanding unfixed bugs
dnl AC_MSG_CHECKING([EES M201 MSF receiver])
dnl AC_ARG_ENABLE([MSFEES],
dnl     [AS_HELP_STRING([--enable-MSFEES], [+ EES M201 MSF receiver])],
dnl     [ntp_ok=$enableval], [ntp_ok=$ntp_eac])
dnl if test "$ntp_ok" = "yes"; then
dnl     ntp_refclock=yes
dnl     AC_DEFINE([CLOCK_MSFEES], [1], [EES M201 MSF receiver])
dnl fi
dnl AC_MSG_RESULT([$ntp_ok])

# Not Ultrix
AC_MSG_CHECKING([Magnavox MX4200 GPS receiver])
AC_ARG_ENABLE(
    [MX4200],
    [AS_HELP_STRING(
	[--enable-MX4200 ],
	[s Magnavox MX4200 GPS receiver]
    )],
    [ntp_ok=$enableval],
    [
	case "$ac_cv_var_ppsclock" in
	 yes)
	    ntp_ok=$ntp_eac
	    ;;
	 *)
	    ntp_ok=no
	    ;;
	esac
    ]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_MX4200], [1], [Magnavox MX4200 GPS receiver])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$host" in
 yes*-*-ultrix*) AC_MSG_WARN([*** But the expected answer is... no ***])
esac

AC_MSG_CHECKING([for NeoClock4X receiver])
AC_ARG_ENABLE(
    [NEOCLOCK4X],
    [AS_HELP_STRING(
	[--enable-NEOCLOCK4X],
	[+ NeoClock4X DCF77 / TDF receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_NEOCLOCK4X], [1], [NeoClock4X])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([NMEA GPS receiver])
AC_ARG_ENABLE(
    [NMEA],
    [AS_HELP_STRING(
	[--enable-NMEA],
	[+ NMEA GPS receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_NMEA], [1], [NMEA GPS receiver])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([for ONCORE Motorola VP/UT Oncore GPS])
AC_ARG_ENABLE(
    [ONCORE],
    [AS_HELP_STRING(
	[--enable-ONCORE],
	[s Motorola VP/UT Oncore GPS receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
case "$ntp_oncore_ok" in
 no) ntp_ok=no ;;
esac
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_ONCORE], 1, [Motorola UT Oncore GPS])
fi
AC_MSG_RESULT([$ntp_ok])

# Requires modem control
AC_MSG_CHECKING([for Palisade clock])
AC_ARG_ENABLE(
    [PALISADE],
    [AS_HELP_STRING(
	[--enable-PALISADE],
	[s Palisade clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_enable_all_modem_control_clocks]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_PALISADE], [1], [Palisade clock])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Conrad parallel port radio clock])
AC_ARG_ENABLE(
    [PCF],
    [AS_HELP_STRING(
	[--enable-PCF ],
	[+ Conrad parallel port radio clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_PCF], [1], [Conrad parallel port radio clock])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([PST/Traconex 1020 WWV/WWVH receiver])
AC_ARG_ENABLE(
    [PST],
    [AS_HELP_STRING(
	[--enable-PST],
	[+ PST/Traconex 1020 WWV/WWVH receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_PST], [1], [PST/Traconex 1020 WWV/WWVH receiver])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([RIPENCC specific Trimble driver])
AC_ARG_ENABLE(
    [RIPENCC],
    [AS_HELP_STRING(
	[--enable-RIPENCC],
	[- RIPENCC specific Trimble driver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=no]
)
# 020629: HMS: s/$ntp_eac -> -/no because of ptr += sprintf(ptr, ...) usage
case "$ntp_ripe_ncc_ok" in
 no) ntp_ok=no ;;
esac
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_RIPENCC], [],[RIPE NCC Trimble clock])
fi
AC_MSG_RESULT([$ntp_ok])

# Danny Meyer says SHM compiles (with a few warnings) under Win32.
# For *IX, we need sys/ipc.h and sys/shm.h.
AC_MSG_CHECKING([for SHM clock attached thru shared memory])
AC_ARG_ENABLE(
    [SHM],
    [AS_HELP_STRING(
	[--enable-SHM],
	[s SHM clock attached thru shared memory]
    )],
    [ntp_ok=$enableval],
    [
	case "$ntp_eac$ntp_canshm" in
	 *no*)	ntp_ok=no  ;;
	 *)	ntp_ok=yes ;;
	esac
    ]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_SHM], [1], [clock thru shared memory])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Spectracom 8170/Netclock/2 WWVB receiver])
AC_ARG_ENABLE(
    [SPECTRACOM],
    [AS_HELP_STRING(
	[--enable-SPECTRACOM],
	[+ Spectracom 8170/Netclock/2 WWVB receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_SPECTRACOM], [1],
	[Spectracom 8170/Netclock/2 WWVB receiver])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([KSI/Odetics TPRO/S GPS receiver/IRIG interface])
AC_ARG_ENABLE(
    [TPRO],
    [AS_HELP_STRING(
	[--enable-TPRO],
	[s KSI/Odetics TPRO/S GPS receiver/IRIG interface]
    )],
    [ntp_ok=$enableval],
    [
	case "$ac_cv_header_sys_tpro_h" in
	 yes)
	    ntp_ok=$ntp_eac
	    ;;
	 *)
	    ntp_ok=no
	    ;;
	esac
    ]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_TPRO], [1],
	[KSI/Odetics TPRO/S GPS receiver/IRIG interface])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ac_cv_header_sys_tpro" in
 yesno) AC_MSG_WARN([*** But the expected answer is... no ***])
esac

dnl Bug 342: longstanding unfixed bugs
dnl AC_MSG_CHECKING([TRAK 8810 GPS receiver])
dnl AC_ARG_ENABLE([TRAK],
dnl     [AS_HELP_STRING([--enable-TRAK], [+ TRAK 8810 GPS receiver])],
dnl     [ntp_ok=$enableval], [ntp_ok=$ntp_eac])
dnl if test "$ntp_ok" = "yes"; then
dnl     ntp_refclock=yes
dnl     AC_DEFINE([CLOCK_TRAK], [1], [TRAK 8810 GPS receiver])
dnl fi
dnl AC_MSG_RESULT([$ntp_ok])

# Not on a vax-dec-bsd
AC_MSG_CHECKING([Kinemetrics/TrueTime receivers])
AC_ARG_ENABLE(
    [TRUETIME],
    [AS_HELP_STRING(
	[--enable-TRUETIME],
	[s Kinemetrics/TrueTime receivers]
    )],
    [ntp_ok=$enableval],
    [
	case "$host" in
	 vax-dec-bsd)
	    ntp_ok=no
	    ;;
	 *)
	    ntp_ok=$ntp_eac
	    ;;
	esac
    ]
)    
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_TRUETIME], [1], [Kinemetrics/TrueTime receivers])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$host" in
 yesvax-dec-bsd) AC_MSG_WARN([*** But the expected answer is... no ***])
esac

AC_MSG_CHECKING([TrueTime 560 IRIG-B decoder])
AC_ARG_ENABLE(
    [TT560],
    [AS_HELP_STRING(
	[--enable-TT560],
	[- TrueTime 560 IRIG-B decoder]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=no]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_TT560], [], [TrueTime 560 IRIG-B decoder?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([Ultralink M320 WWVB receiver])
AC_ARG_ENABLE(
    [ULINK],
    [AS_HELP_STRING(
	[--enable-ULINK],
	[+ Ultralink WWVB receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_ULINK], [1], [Ultralink M320 WWVB receiver?])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([WWV receiver])
AC_ARG_ENABLE(
    [WWV],
    [AS_HELP_STRING(
	[--enable-WWV],
	[s WWV Audio receiver]
    )],
    [ntp_ok=$enableval],
    [
	case "$ntp_eac$ntp_canaudio" in
	 *no*)	ntp_ok=no  ;;
	 *)	ntp_ok=yes ;;
	esac
    ]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_WWV], [1], [WWV audio driver])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canaudio" in
 yesno) AC_MSG_WARN([*** But the expected answer is... no ***])
esac

AC_MSG_CHECKING([for Zyfer receiver])
AC_ARG_ENABLE(
    [ZYFER],
    [AS_HELP_STRING(
	[--enable-ZYFER],
	[+ Zyfer GPStarplus receiver]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eac]
)
if test "$ntp_ok" = "yes"; then
    ntp_refclock=yes
    AC_DEFINE([CLOCK_ZYFER], [1], [Zyfer GPStarplus])
fi
AC_MSG_RESULT([$ntp_ok])

AC_MSG_CHECKING([for default inclusion of all suitable PARSE clocks])
AC_ARG_ENABLE(
    [parse-clocks],
    [AS_HELP_STRING(
	[--enable-parse-clocks],
	[- include all suitable PARSE clocks:]
    )],
    [ntp_eapc=$enableval],
    [
	case "$ntp_eac" in
	 yes)	ntp_eapc=$ntp_canparse ;;
	 *)	ntp_eapc=no ;;
	esac
	# Delete the next line one of these days
	ntp_eapc=no
    ]
)
AC_MSG_RESULT($ntp_eapc)

case "$ntp_eac$ntp_eapc$ntp_canparse" in
 noyes*)
    AC_MSG_ERROR(["--enable-parse-clocks" requires "--enable-all-clocks".])
    ;;
 yesyesno)
    AC_MSG_ERROR([You said "--enable-parse-clocks" but PARSE isn't supported on this platform!])
    ;;
esac

ntp_libparse=no
ntp_parseutil=no
ntp_rawdcf=no

AC_MSG_CHECKING([Diem Computime Radio Clock])
AC_ARG_ENABLE(
    [COMPUTIME],
    [AS_HELP_STRING(
	[--enable-COMPUTIME],
	[s Diem Computime Radio Clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_COMPUTIME], [1], [Diems Computime Radio Clock?])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([ELV/DCF7000 clock])
AC_ARG_ENABLE(
    [DCF7000],
    [AS_HELP_STRING(
	[--enable-DCF7000],
	[s ELV/DCF7000 clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_DCF7000], [1], [ELV/DCF7000 clock?])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([HOPF 6021 clock])
AC_ARG_ENABLE(
    [HOPF6021],
    [AS_HELP_STRING(
	[--enable-HOPF6021],
	[s HOPF 6021 clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_HOPF6021], [1], [HOPF 6021 clock?])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([Meinberg clocks])
AC_ARG_ENABLE(
    [MEINBERG],
    [AS_HELP_STRING(
	[--enable-MEINBERG],
	[s Meinberg clocks]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_MEINBERG], [1], [Meinberg clocks])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([DCF77 raw time code])
AC_ARG_ENABLE(
    [RAWDCF],
    [AS_HELP_STRING(
	[--enable-RAWDCF],
	[s DCF77 raw time code]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_parseutil=yes
    ntp_refclock=yes
    ntp_rawdcf=yes
    AC_DEFINE([CLOCK_RAWDCF], [1], [DCF77 raw time code])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

case "$ntp_rawdcf" in
 yes)
    AC_CACHE_CHECK([if we must enable parity for RAWDCF],
	[ntp_cv_rawdcf_parity],
	[
	    ans=no
	    case "$host" in
	     *-*-*linux*)
		ans=yes
		;;
	    esac
	    ntp_cv_rawdcf_parity=$ans
	]
    )
    case "$ntp_cv_rawdcf_parity" in
     yes)
	AC_DEFINE([RAWDCF_NO_IGNPAR], [1],
	    [Should we not IGNPAR (Linux)?]) ;;
    esac
esac

AC_MSG_CHECKING([RCC 8000 clock])
AC_ARG_ENABLE(
    [RCC8000],
    [AS_HELP_STRING(
	[--enable-RCC8000],
	[s RCC 8000 clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_RCC8000], [1], [RCC 8000 clock])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([Schmid DCF77 clock])
AC_ARG_ENABLE(
    [SCHMID],
    [AS_HELP_STRING(
	[--enable-SCHMID ],
	[s Schmid DCF77 clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_SCHMID], [1], [Schmid DCF77 clock])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([Trimble GPS receiver/TAIP protocol])
AC_ARG_ENABLE(
    [TRIMTAIP],
    [AS_HELP_STRING(
	[--enable-TRIMTAIP],
	[s Trimble GPS receiver/TAIP protocol]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_TRIMTAIP], [1],
	[Trimble GPS receiver/TAIP protocol])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([Trimble GPS receiver/TSIP protocol])
AC_ARG_ENABLE(
    [TRIMTSIP],
    [AS_HELP_STRING(
	[--enable-TRIMTSIP],
	[s Trimble GPS receiver/TSIP protocol]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_TRIMTSIP], [1],
	[Trimble GPS receiver/TSIP protocol])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([WHARTON 400A Series clock])
AC_ARG_ENABLE(
    [WHARTON],
    [AS_HELP_STRING(
	[--enable-WHARTON],
	[s WHARTON 400A Series clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_WHARTON_400A], [1], [WHARTON 400A Series clock])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_MSG_CHECKING([VARITEXT clock])
AC_ARG_ENABLE(
    [VARITEXT],
    [AS_HELP_STRING(
	[--enable-VARITEXT],
	[s VARITEXT clock]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=$ntp_eapc]
)
if test "$ntp_ok" = "yes"; then
    ntp_libparse=yes
    ntp_refclock=yes
    AC_DEFINE([CLOCK_VARITEXT], [1], [VARITEXT clock])
fi
AC_MSG_RESULT([$ntp_ok])
case "$ntp_ok$ntp_canparse" in
 yesno)
    AC_MSG_ERROR([That's a parse clock and this system doesn't support it!])
esac

AC_SUBST([LIBPARSE])
AC_SUBST([MAKE_LIBPARSE])
AC_SUBST([MAKE_LIBPARSE_KERNEL])
AC_SUBST([MAKE_CHECK_Y2K])

AC_MSG_CHECKING([if we need to make and use the parse libraries])
ans=no
case "$ntp_libparse" in
 yes)
    ans=yes
    AC_DEFINE([CLOCK_PARSE], [1], [PARSE driver interface])
    LIBPARSE=../libparse/libparse.a
    MAKE_LIBPARSE=libparse.a
    # HMS: check_y2k trips the 34 year problem now...
    false && MAKE_CHECK_Y2K=check_y2k
esac
AC_MSG_RESULT([$ans])

NTP_OPENSSL

AC_MSG_CHECKING([if we want to compile with ElectricFence])
AC_ARG_WITH(
    [electricfence],
    [AS_HELP_STRING(
	[--with-electricfence],
	[- compile with ElectricFence malloc debugger]
    )],
    [ans=$withval],
    [ans=no]
)
case "$ans" in
 no) ;;
 *)
    LIBS="$LIBS \${top_builddir}/ElectricFence/libefence.a"
    EF_PROGS="eftest tstheap"
    AC_SUBST([EF_PROGS])
    EF_LIBS=libefence.a
    AC_SUBST([EF_LIBS])
    ans=yes
    ;;
esac
AC_MSG_RESULT([$ans])

AC_SUBST([MAKE_CHECK_LAYOUT])
AC_MSG_CHECKING([if we want to run check-layout])
case "$cross_compiling$PATH_PERL" in
 no/*)
    MAKE_CHECK_LAYOUT=check-layout
    ans=yes
    ;;
 *)
    ans=no
    ;;
esac
AC_MSG_RESULT([$ans])

AC_SUBST([TESTDCF])
AC_SUBST([DCFD])
AC_MSG_CHECKING([if we can make dcf parse utilities])
ans=no
if test "$ntp_parseutil" = "yes"; then
    case "$host" in
     *-*-sunos4*|*-*-solaris2*|*-*-*linux*|*-*-netbsd*)
	ans="dcfd testdcf"
	DCFD=dcfd
        TESTDCF=testdcf
    esac
fi
AC_MSG_RESULT([$ans])

AC_SUBST([MAKE_PARSEKMODULE])
AC_MSG_CHECKING([if we can build kernel streams modules for parse])
ans=no
case "$ntp_parseutil$ac_cv_header_sys_stropts_h" in
 yesyes)
    case "$host" in
     sparc-*-sunos4*)
        case "$ntp_cv_var_kernel_pll" in
	yes)
	    AC_DEFINE([PPS_SYNC], [1], [PARSE kernel PLL PPS support])
	    ;;
	esac
	ans=parsestreams
	MAKE_PARSEKMODULE=parsestreams.loadable_module.o
	;;
     sparc-*-solaris2*)
	ans=parsesolaris
	MAKE_PARSEKMODULE=parse
	AC_CHECK_HEADERS([strings.h])
	;;
    esac
    ;;
esac
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([if we need basic refclock support])
if test "$ntp_refclock" = "yes"; then
    AC_DEFINE([REFCLOCK], [1], [Basic refclock support?])
fi
AC_MSG_RESULT($ntp_refclock)

dnl Things that can be made in clockstuff/
AC_SUBST([PROPDELAY])	dnl Set to "propdelay"
AC_SUBST([CHUTEST])	dnl Set to "chutest"
AC_SUBST([CLKTEST])	dnl Set to "clktest"

AC_SUBST([MAKE_ADJTIMED])
AC_MSG_CHECKING([if we want HP-UX adjtimed support])
case "$host" in
 *-*-hpux[[56789]]*)
    ans=yes
    ;;
 *) ans=no
    ;;
esac
if test "$ans" = "yes"; then
    MAKE_ADJTIMED=adjtimed
    AC_DEFINE([NEED_HPUX_ADJTIME], [1],
	[Do we need HPUX adjtime() library support?])
fi
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([if we want QNX adjtime support])
case "$host" in
 *-*-qnx*)
    ans=yes
    ;;
 *) ans=no
    ;;
esac
if test "$ans" = "yes"; then
    AC_DEFINE([NEED_QNX_ADJTIME], [1],
	[Do we need the qnx adjtime call?])
fi
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([if we can read kmem])

#  the default is to enable it if the system has the capability

case "$ac_cv_func_nlist$ac_cv_func_K_open$ac_cv_func_kvm_open" in
 *yes*)
    ans=yes
    ;;
 *) ans=no
esac

case "$host" in
 *-*-domainos)	# Won't be found...
    ans=no
    ;;
 *-*-hpux*)
    #ans=no
    ;;
 *-*-irix[[456]]*)
    ans=no
    ;;
 *-*-*linux*)
    ans=no
    ;;
 *-*-winnt3.5)
    ans=no
    ;;
 *-*-unicosmp*)
    ans=no
    ;;
esac

# --enable-kmem / --disable-kmem controls if present
AC_ARG_ENABLE(
    [kmem],
    [AS_HELP_STRING(
	[--enable-kmem],
	[s read /dev/kmem for tick and/or tickadj]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    can_kmem=yes
    ;;
 *) 
    can_kmem=no
    AC_DEFINE([NOKMEM], [1], [Should we NOT read /dev/kmem?])
esac


AC_MSG_CHECKING([if adjtime is accurate])

# target-dependent defaults

case "$host" in
 i386-sequent-ptx*)
    ans=no
    ;;
 i386-unknown-osf1*)
    ans=yes
    ;;
 mips-sgi-irix[[456]]*)
    ans=yes
    ;;
 *-fujitsu-uxp*)
    ans=yes
    ;;
 *-ibm-aix[[456]]*)
    ans=yes
    ;;
 *-*-*linux*)
    ans=yes
    ;;
 *-*-solaris2.[[01]])
    ans=no
    ;;
 *-*-solaris2*)
    ans=yes
    ;;
 *-*-unicosmp*)
    ans=yes
    ;;
 *) ans=no
esac

# --enable-accurate-adjtime / --disable-accurate-adjtime
# override the default
AC_ARG_ENABLE(
    [accurate-adjtime],
    [AS_HELP_STRING(
	[--enable-accurate-adjtime], 
	[s the adjtime() call is accurate]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes) 
    AC_DEFINE([ADJTIME_IS_ACCURATE], [1], [Is adjtime() accurate?])
    adjtime_is_accurate=yes
    ;;
 *)
    adjtime_is_accurate=no
    ;;
esac

AC_CACHE_CHECK(
    [the name of 'tick' in the kernel],
    [ntp_cv_nlist_tick],
    [
	ans=_tick
	case "$host" in
	 m68*-hp-hpux*) # HP9000/300?
	    ans=_old_tick
	    ;;
	 *-apple-aux[[23]]*)
	    ans=tick
	    ;;
	 *-hp-hpux*)
	    ans=old_tick
	    ;;
	 *-ibm-aix[[3456]]*)
	    ans=no
	    ;;
	 *-*-mpeix*)
	    ans=no
	    ;;
	 *-*-ptx*)
	    ans=tick
	    ;;
	 *-*-sco3.2v[[45]]*)
	    ans=no
	    ;;
	 *-*-solaris2*)
	    ans=nsec_per_tick
	    ;;
	 *-*-sysv4*)
	    ans=tick
	    ;;
	esac
	ntp_cv_nlist_tick=$ans
    ]
)
case "$ntp_cv_nlist_tick" in
 ''|no)
    ;;	# HMS: I think we can only get 'no' here...
 *) 
    AC_DEFINE_UNQUOTED([K_TICK_NAME], ["$ntp_cv_nlist_tick"],
	[What is the name of TICK in the kernel?])
esac

AC_CACHE_CHECK(
    [for the units of 'tick'],
    [ntp_cv_tick_nano],
    [
	ans=usec
	case "$host" in
	 *-*-solaris2*)
	    ans=nsec
	    ;;
	esac
	ntp_cv_tick_nano=$ans
    ]
)
case "$ntp_cv_tick_nano" in
 nsec)
    AC_DEFINE([TICK_NANO], [1], [Is K_TICK_NAME in nanoseconds?])
esac

AC_CACHE_CHECK(
    [the name of 'tickadj' in the kernel],
    [ntp_cv_nlist_tickadj],
    [
	ans=_tickadj
	case "$host" in
	 m68*-hp-hpux*) # HP9000/300?
	    ans=_tickadj
	    ;;
	 *-apple-aux[[23]]*)
	    ans=tickadj
	    ;;
	 *-hp-hpux10*)
	    ans=no
	    ;;
	 *-hp-hpux9*)
	    ans=no
	    ;;
	 *-hp-hpux*)
	    ans=tickadj
	    ;;
	 *-*-aix*)
	    ans=tickadj
	    ;;
	 *-*-mpeix*)
	    ans=no
	    ;;
	 *-*-ptx*)
	    ans=tickadj
	    ;;
	 *-*-sco3.2v4*)
	    ans=no
	    ;;
	 *-*-sco3.2v5.0*)
	    ans=clock_drift
	    ;;
	 *-*-solaris2*)
	    ans=no	# hrestime_adj
	    ;;
	 *-*-sysv4*)
	    ans=tickadj
	    ;;
	esac
	ntp_cv_nlist_tickadj=$ans
    ]
)
case "$ntp_cv_nlist_tickadj" in
 ''|no)
    ;;	# HMS: I think we can only get 'no' here...
 *) 
    AC_DEFINE_UNQUOTED([K_TICKADJ_NAME], ["$ntp_cv_nlist_tickadj"],
	[What is the name of TICKADJ in the kernel?])
esac

AC_CACHE_CHECK(
    [for the units of 'tickadj'],
    [ntp_cv_tickadj_nano],
    [
	ans=usec
	case "$host" in
	 *-*-solaris2*)
	    ans=nsec
	    ;;
	esac
	ntp_cv_tickadj_nano=$ans
    ]
)
case "$ntp_cv_tickadj_nano" in
 nsec)
    AC_DEFINE([TICKADJ_NANO], [1], [Is K_TICKADJ_NAME in nanoseconds?])
esac

AC_CACHE_CHECK(
    [half-heartedly for 'dosynctodr' in the kernel],
    [ntp_cv_nlist_dosynctodr],
    [
	case "$host" in
	 *-apple-aux[[23]]*)
	    ans=no
	    ;;
	 *-sni-sysv*)
	    ans=dosynctodr
	    ;;
	 *-*-aix*)
	    ans=dosynctodr
	    ;;
	 *-*-hpux*)
	    ans=no
	    ;;
	 *-*-mpeix*)
	    ans=no
	    ;;
	 *-*-nextstep*)
	    ans=_dosynctodr
	    ;;
	 *-*-ptx*)
	    ans=doresettodr
	    ;;
	 *-*-sco3.2v4*)
	    ans=no
	    ;;
	 *-*-sco3.2v5*)
	    ans=track_rtc
	    ;;
	 *-*-solaris2*)
	    ans=dosynctodr
	    ;;
	 *-*-sysv4*)
	    ans=doresettodr
	    ;;
	 *)
	    ans=_dosynctodr
	    ;;
	esac
	ntp_cv_nlist_dosynctodr=$ans
    ]
)
case "$ntp_cv_nlist_dosynctodr" in
 no)
    ;;
 *)
    AC_DEFINE_UNQUOTED([K_DOSYNCTODR_NAME], ["$ntp_cv_nlist_dosynctodr"],
	[What is (probably) the name of DOSYNCTODR in the kernel?])
    ;;
esac

AC_CACHE_CHECK(
    [half-heartedly for 'noprintf' in the kernel],
    [ntp_cv_nlist_noprintf],
    [
	case "$host" in
	 *-apple-aux[[23]]*)
	    ans=no
	    ;;
	 *-sni-sysv*)
	    ans=noprintf
	    ;;
	 *-*-aix*)
	    ans=noprintf
	    ;;
	 *-*-hpux*)
	    ans=no
	    ;;
	 *-*-mpeix*)
	    ans=no
	    ;;
	 *-*-ptx*)
	    ans=noprintf
	    ;;
	 *-*-nextstep*)
	    ans=_noprintf
	    ;;
	 *-*-solaris2*)
	    ans=noprintf
	    ;;
	 *-*-sysv4*)
	    ans=noprintf
	    ;;
	 *)
	    ans=_noprintf
	    ;;
	esac
	ntp_cv_nlist_noprintf=$ans
    ]
)
case "$ntp_cv_nlist_noprintf" in
 no)
    ;;
 *)
    AC_DEFINE_UNQUOTED([K_NOPRINTF_NAME], ["$ntp_cv_nlist_noprintf"],
	[What is (probably) the name of NOPRINTF in the kernel?])
    ;;
esac

dnl The tick/tickadj sections were written by Skippy, who never learned
dnl that it's impolite (horridly gross) to show your guts in public.

dnl	tick		tickadj	
dnl	10000		80	    Unixware
dnl	1000000L/hz	tick/16     (Solaris,UXPV,HPUX) && ADJTIME_IS_ACCURATE
dnl	10000		150	    sgi IRIX
dnl	1000000L/hz	1000	    RS6000 && NOKMEM
dnl	1000000L/hz	668	    DOMAINOS && NOKMEM
dnl	1000000L/hz	500/HZ	    other && NOKMEM
dnl	txc.tick	1	    Linux
dnl	(every / 10)	50	    WinNT - tickadj is roughly 500/hz
dnl	1000000L/hz	(nlist)     (Solaris && !ADJTIME_IS_ACCURATE),
dnl				    (RS6000 && !NOKMEM), SINIX MIPS

dnl But we'll only use these "values" if we can't find anything else.

AC_MSG_CHECKING([for a default value for 'tick'])

# target-dependent default for tick

case "$host" in
 *-*-pc-cygwin*)
    AC_MSG_ERROR([tick needs work for cygwin])
    ;;
 *-univel-sysv*)
    ans=10000
    ;;
 *-*-irix*)
    ans=10000
    ;;
 *-*-*linux*)
    ans=txc.tick
    ;;
 *-*-mpeix*)
    ans=no
    ;;
 *-*-winnt3.5)
    ans='(every / 10)'
    ;;
 *-*-unicosmp*)
    ans=10000
    ;;
 *)
    ans='1000000L/hz'
    ;;
esac

AC_ARG_ENABLE(
    [tick],
    [AS_HELP_STRING(
	[--enable-tick=VALUE],
	[s force a value for 'tick']
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 ''|no)
    ;;	# HMS: I think we can only get 'no' here...
 *)
    AC_DEFINE_UNQUOTED([PRESET_TICK], [$ans],
	[Preset a value for 'tick'?])
esac

AC_MSG_CHECKING([for a default value for 'tickadj'])

# target-specific default
ans='500/hz'
case "$host" in
 *-fujitsu-uxp*)
    case "$adjtime_is_accurate" in
     yes)
	ans='tick/16'
    esac
    ;;
 *-univel-sysv*)
    ans=80
    ;;
 *-*-aix*)
    case "$can_kmem" in
     no)
	ans=1000
    esac
    ;;
 *-*-domainos)	# Skippy: won't be found...
    case "$can_kmem" in
     no)
	ans=668
    esac
    ;;
 *-*-hpux*)
    case "$adjtime_is_accurate" in
     yes)
	ans='tick/16'
    esac
    ;;
 *-*-irix*)
    ans=150
    ;;
 *-*-mpeix*)
    ans=no
    ;;
 *-*-sco3.2v5.0*)
    ans=10000L/hz
    ;;
 *-*-winnt3.5)
    ans=50
    ;;
 *-*-unicosmp*)
    ans=150
esac

AC_ARG_ENABLE(
    [tickadj],
    [AS_HELP_STRING(
	[--enable-tickadj=VALUE],
	[s force a value for 'tickadj']
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

default_tickadj=$ans

case "$default_tickadj" in
 ''|no)
    ;;	# HMS: I think we can only get 'no' here...
 *)
    AC_DEFINE_UNQUOTED([PRESET_TICKADJ], [$default_tickadj],
	[Preset a value for 'tickadj'?]) ;;
esac

# Newer versions of ReliantUNIX round adjtime() values down to
# 1/100s (system tick). Sigh ...
# Unfortunately, there is no easy way to know if particular release
# has this "feature" or any obvious way to test for it.
case "$host" in
 mips-sni-sysv4*)
    AC_DEFINE([RELIANTUNIX_CLOCK], [1],
	[Do we want the ReliantUNIX clock hacks?])
esac

case "$host" in
 *-*-sco3.2v5*)
    AC_DEFINE([SCO5_CLOCK], [1], [Do we want the SCO clock hacks?])
esac

ntp_cv_make_tickadj=yes
case "$can_kmem$ac_cv_var_tick$default_tickadj" in
 nonono)	# Don't read KMEM, no presets.  Bogus.
    AC_MSG_WARN([Can't read kmem, no PRESET_TICK or PRESET_TICKADJ.  No tickadj.])
    ntp_cv_make_tickadj=no
    ;;
 nono*)		# Don't read KMEM, no PRESET_TICK but PRESET_TICKADJ.  Bogus.
    AC_MSG_WARN([Can't read kmem but no PRESET_TICK.  No tickadj.])
    ntp_cv_make_tickadj=no
    ;;
 no*no)		# Don't read KMEM, PRESET_TICK but no PRESET_TICKADJ.  Bogus.
    AC_MSG_WARN([Can't read kmem but no PRESET_TICKADJ.  No tickadj.])
    ntp_cv_make_tickadj=no
    ;;
 no*)		# Don't read KMEM, PRESET_TICK and PRESET_TICKADJ.  Cool.
    ;;
 yesnono)	# Read KMEM, no presets.  Cool.
    ;;
 yesno*)	# Read KMEM, no PRESET_TICK but PRESET_TICKADJ.  Bogus.
    AC_MSG_WARN([PRESET_TICKADJ is defined but not PRESET_TICK.  Please report this.])
    ;;
 yes*no)	# Read KMEM, PRESET_TICK but no PRESET_TICKADJ.  Cool.
    ;;
 yes*)		# READ KMEM, PRESET_TICK and PRESET_TICKADJ.
    ;;
 *)		# Generally bogus.
    AC_MSG_ERROR([This shouldn't happen.])
    ;;
esac

AC_SUBST(MAKE_NTPTIME)
AC_CACHE_CHECK([if we want and can make the ntptime utility], ac_cv_make_ntptime,
[case "$host" in
 *) case "$ntp_cv_struct_ntptimeval$ntp_cv_var_kernel_pll" in
     yesyes)
	ans=yes
	;;
     *)
	ans=no
	;;
    esac
    ;;
esac
ac_cv_make_ntptime=$ans])
case "$ac_cv_make_ntptime" in
 yes)
    MAKE_NTPTIME=ntptime
    ;;
esac

AC_SUBST([MAKE_TICKADJ])
case "$host" in
 mips-sni-sysv4*)
    # tickadj is pretty useless on newer versions of ReliantUNIX
    # Do not bother
    ntp_cv_make_tickadj=no
    ;;
 *-*-irix*)
    ntp_cv_make_tickadj=no
    ;;
 *-*-solaris2*)
    # DLM says tickadj is a no-no starting with solaris2.5
    case "$host" in
     *-*-solaris2.1[[0-9]]*)
	ntp_cv_make_tickadj=no
	;;
     *-*-solaris2.[[0-4]]*) ;;
     *) ntp_cv_make_tickadj=no ;;
    esac
    ;;
 *-*-unicosmp*)
    ntp_cv_make_tickadj=no
    ;;
esac

#
# Despite all the above, we always make tickadj.  Setting
# ntp_cv_make_tickadj before AC_CACHE_CHECK will cause a false
# report that the configuration variable was cached.  It may
# be better to simply remove the hunk above, I did not want
# to remove it if there is hope it will be used again.
#
AS_UNSET([ntp_cv_make_tickadj])

AC_CACHE_CHECK(
    [if we want and can make the tickadj utility],
    [ntp_cv_make_tickadj],
    [ntp_cv_make_tickadj=yes]
)
case "$ntp_cv_make_tickadj" in
 yes)
    MAKE_TICKADJ=tickadj
esac

AC_SUBST([MAKE_TIMETRIM])
AC_CACHE_CHECK(
    [if we want and can make the timetrim utility],
    [ntp_cv_make_timetrim],
    [
	case "$host" in
	 *-*-irix*)
	    ntp_cv_make_timetrim=yes
	    ;;
	 *-*-unicosmp*)
	    ntp_cv_make_timetrim=yes
	    ;;
	 *)
	    ntp_cv_make_timetrim=no
	    ;;
	esac
    ]
)
case "$ntp_cv_make_timetrim" in
 yes)
    MAKE_TIMETRIM=timetrim
esac

AC_SUBST([MAKE_LIBNTPSIM])
AC_SUBST([MAKE_NTPDSIM])

AC_MSG_CHECKING([if we want to build the NTPD simulator])
AC_ARG_ENABLE(
    [simulator],
    [AS_HELP_STRING(
	[--enable-simulator],
	[- build/install the NTPD simulator?]
    )],
    [ans=$enableval], 
    [ans=no]
)
AC_MSG_RESULT([$ans])
case "$ans" in
 yes)
    MAKE_NTPDSIM=ntpdsim
    MAKE_LIBNTPSIM=libntpsim.a
esac

case "$build" in
 $host)
    ;;
 *) case "$host" in
     *-*-vxworks*)
	LDFLAGS="$LDFLAGS -r"
	;;
    esac
    ;;
esac


AC_MSG_CHECKING([if we want to build ntpsnmpd])
AC_ARG_WITH(
    [ntpsnmpd],
    [AS_HELP_STRING(
	[--with-ntpsnmpd],
	[s Build ntpsnmpd MIB agent?]
    )],
    [ans=$withval],
    [
	case "$PATH_NET_SNMP_CONFIG" in
	 /*)	ans=yes ;;
	 *)	ans=no  ;;
	esac
    ]
)
AC_MSG_RESULT([$ans])
case "$ans" in
 yes)
    case "$PATH_NET_SNMP_CONFIG" in
     /*)
	SNMP_LIBS=`$PATH_NET_SNMP_CONFIG --agent-libs`
	AC_SUBST([SNMP_LIBS])
	# HMS: we really want to separate CPPFLAGS and CFLAGS
	foo=`$PATH_NET_SNMP_CONFIG --cflags`
	SNMP_CPPFLAGS=
	SNMP_CFLAGS=
	for i in $foo; do
	    case "$i" in
	     -D*|-U*|-I*)
		SNMP_CPPFLAGS="$SNMP_CPPFLAGS $i"
		;;
	    *)	SNMP_CFLAGS="$SNMP_CFLAGS $i"
		;;
	    esac
	done
	AC_SUBST([SNMP_CPPFLAGS])
	AC_SUBST([SNMP_CFLAGS])

	save_CFLAGS=$CFLAGS
	save_CPPFLAGS=$CPPFLAGS
	save_LIBS=$LIBS
	CFLAGS=$SNMP_CFLAGS
	CPPFLAGS=$SNMP_CPPFLAGS

	AC_CHECK_HEADER(
	    [net-snmp/net-snmp-config.h],
	    [MAKE_NTPSNMPD=ntpsnmpd],
	    [AC_MSG_WARN([net-snmp-config present but net-snmp headers are not available!])]
	)

	# Do this last, as we're messing up LIBS.
	# check -lnetsnmp for netsnmp_daemonize
	LIBS=`$PATH_NET_SNMP_CONFIG --libs`
	AC_CHECK_LIB(
	    [netsnmp],
	    [netsnmp_daemonize],
	    [ans=yes],
	    [ans=no]
	)
	case "$ans" in
	 no)
	    AC_DEFINE([NEED_NETSNMP_DAEMONIZE], [1],
		[We need to provide netsnmp_daemonize()])
	esac
	
	CFLAGS=$save_CFLAGS
	CPPFLAGS=$save_CPPFLAGS
	LIBS=$save_LIBS
	;;
     *) 
	AC_MSG_WARN([Cannot build ntpsnmpd - net-snmp-config cannot be found])
	;;
    esac
    ;;
esac
AC_SUBST([MAKE_NTPSNMPD])


AC_MSG_CHECKING([if we should always slew the time])

# target-specific defaults

case "$host" in
 *-apple-aux[[23]]*)
    ans=yes
    ;;
 *-*-bsdi[[012]]*)
    ans=no
    ;;
 *-*-bsdi*)
    ans=yes
    ;;
 *-*-openvms*)	# HMS: won't be found
    ans=yes
    ;;
 *) ans=no
    ;;
esac

# --enable-slew-always / --disable-slew-always overrides default

AC_ARG_ENABLE(
    [slew-always],
    [AS_HELP_STRING(
	[--enable-slew-always],
	[s always slew the time]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    AC_DEFINE([SLEWALWAYS], [1], [Slew always?])
esac

AC_MSG_CHECKING([if we should step and slew the time])

case "$host" in
 *-sni-sysv*)
    ans=yes
    ;;
 *-univel-sysv*)
    ans=no
    ;;
 *-*-ptx*)
    ans=yes
    ;;
 *-*-solaris2.1[[0-9]]*)
    ans=no
    ;;
 *-*-solaris2.[[012]]*)
    ans=yes
    ;;
 *-*-sysv4*)	# HMS: Does this catch Fujitsu UXP?
    ans=yes
    ;;
 *) ans=no
    ;;
esac

AC_ARG_ENABLE(
    [step-slew],
    [AS_HELP_STRING(
	[--enable-step-slew],
	[s step and slew the time]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    AC_DEFINE([STEP_SLEW], [1], [Step, then slew the clock?])
esac

AC_MSG_CHECKING([if ntpdate should step the time])

case "$host" in
 *-apple-aux[[23]]*)
    ans=yes
    ;;
 *) ans=no
esac

AC_ARG_ENABLE(
    [ntpdate-step],
    [AS_HELP_STRING(
	[--enable-ntpdate-step],
	[s if ntpdate should step the time]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    AC_DEFINE([FORCE_NTPDATE_STEP], [1],
	[force ntpdate to step the clock if !defined(STEP_SLEW) ?])
esac


AC_MSG_CHECKING([if we should sync TODR clock every hour])

case "$host" in
 *-*-nextstep*)
    ans=yes
    ;;
 *-*-openvms*)	# HMS: won't be found
    ans=yes
    ;;
 *)
    ans=no
esac

AC_ARG_ENABLE(
    [hourly-todr-sync],
    [AS_HELP_STRING(
	[--enable-hourly-todr-sync],
	[s if we should sync TODR hourly]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ac_cv_var_sync_todr" in
 yes)
    AC_DEFINE([DOSYNCTODR], [1], [synch TODR hourly?]) ;;
esac


AC_MSG_CHECKING([if we should avoid kernel FLL bug])

case "$host" in
 *-*-solaris2.6)
    unamev=`uname -v`
    case "$unamev" in
     Generic_105181-*)
	old_IFS="$IFS"
	IFS="-"
	set $unamev
	IFS="$old_IFS"
	if test "$2" -ge 17
	then
	    # Generic_105181-17 and higher
	    ans=no
	else
	    ans=yes
	fi
	;;
     *) 
	ans=yes
    esac
    ;;
 *-*-solaris2.7)
    unamev=`uname -v`
    case "$unamev" in
     Generic_106541-*)
	old_IFS="$IFS"
	IFS="-"
	set $unamev
	IFS="$old_IFS"
	if test "$2" -ge 07
	then
	    # Generic_106541-07 and higher
	    ans=no
	else
	    ans=yes
	fi
	;;
     *)
	ans=yes
    esac
    ;;
 *)
    ans=no
esac

AC_ARG_ENABLE(
    [kernel-fll-bug],
    [AS_HELP_STRING(
	[--enable-kernel-fll-bug],
	[s if we should avoid a kernel FLL bug]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    AC_DEFINE([KERNEL_FLL_BUG], [1], [Does the kernel have an FLL bug?])
esac


AC_MSG_CHECKING([if we want new session key behavior])
AC_ARG_ENABLE(
    [bug1243-fix],
    [AS_HELP_STRING(
	[--enable-bug1243-fix],
	[+ use unmodified autokey session keys]
    )],
    [ans=$enableval],
    [ans=yes]
)
AC_MSG_RESULT([$ans])
case "$ans" in
 no)
    AC_DEFINE([DISABLE_BUG1243_FIX], [1],
	[use old autokey session key behavior?])
esac


AC_MSG_CHECKING([if we should use the IRIG sawtooth filter])

case "$host" in
 *-*-solaris2.[[89]])
    ans=yes
    ;;
 *-*-solaris2.1[[0-9]]*)
    ans=yes
    ;;
 *) ans=no
esac

AC_ARG_ENABLE(
    [irig-sawtooth], 
    [AS_HELP_STRING(
	[--enable-irig-sawtooth],
	[s if we should enable the IRIG sawtooth filter]
    )],
    [ans=$enableval]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    AC_DEFINE([IRIG_SUCKS], [1],
	[Should we use the IRIG sawtooth filter?])
esac


AC_MSG_CHECKING([if we should enable NIST lockclock scheme])

AC_ARG_ENABLE(
	[nist], 
	[AS_HELP_STRING(
	    [--enable-nist],
	    [- if we should enable the NIST lockclock scheme]
	)],
	[ans=$enableval],
	[ans=no]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 yes)
    AC_DEFINE([LOCKCLOCK], [1],
	[Should we align with the NIST lockclock scheme?]) ;;
esac


AC_MSG_CHECKING([if we want support for Samba's signing daemon])

AC_ARG_ENABLE(
    [ntp-signd],
    [AS_HELP_STRING(
	[--enable-ntp-signd],
	[- Provide support for Samba's signing daemon, =/var/run/ntp_signd]
    )],
    [ans=$enableval],
    [ans=no]
)

AC_MSG_RESULT([$ans])

case "$ans" in
 no)
    ntp_signd_path=
    ;;
 yes)
    ntp_signd_path=/var/run/ntp_signd
    ;;
 *)
    ntp_signd_path="$ans"
esac

case "$ntp_signd_path" in
 '')
    ;;
 *)
    AC_DEFINE([HAVE_NTP_SIGND], [1],
	[Do we want support for Samba's signing daemon?])
    AC_DEFINE_UNQUOTED([NTP_SIGND_PATH], ["$ntp_signd_path"],
	[Path to sign daemon rendezvous socket])
esac


AC_CHECK_HEADERS([sys/clockctl.h])

case "$host" in
 *-*-netbsd*)
    ans=yes
    ;;
 *) ans=no
    ;;
esac

AC_ARG_ENABLE(
    [clockctl],
    [AS_HELP_STRING(
	[--enable-clockctl],
	[s Use /dev/clockctl for non-root clock control]
    )],
    [ntp_use_dev_clockctl=$enableval],
    [ntp_use_dev_clockctl=$ac_cv_header_sys_clockctl_h]
)

AC_MSG_CHECKING([[if we should use /dev/clockctl]])
AC_MSG_RESULT([$ntp_use_dev_clockctl])


AC_CHECK_HEADERS([sys/capability.h sys/prctl.h])

AC_MSG_CHECKING([if we have linux capabilities (libcap)])

case "$ac_cv_header_sys_capability_h$ac_cv_header_sys_prctl_h" in
 yesyes)
    case "$host" in
     mips-sgi-irix*)
	ntp_have_linuxcaps=no
	;;
     *) ntp_have_linuxcaps=yes
	;;
    esac
    ;;
 *)
    ntp_have_linuxcaps=no
    ;;
esac

AC_ARG_ENABLE(
    [linuxcaps],
    [AS_HELP_STRING(
	[--enable-linuxcaps],
	[+ Use Linux capabilities for non-root clock control]
    )],
    [ntp_have_linuxcaps=$enableval]
)

AC_MSG_RESULT([$ntp_have_linuxcaps])

case "$ntp_have_linuxcaps" in
 yes)
    AC_DEFINE([HAVE_LINUX_CAPABILITIES], [1],
	[Do we have Linux capabilities?])
    LIBS="$LIBS -lcap"
esac

case "$ntp_use_dev_clockctl$ntp_have_linuxcaps" in
 *yes*)
    AC_DEFINE([HAVE_DROPROOT], [1],
	[Can we drop root privileges?])
esac


AC_CHECK_HEADERS([libscf.h])
LSCF=
case "$ac_cv_header_libscf_h" in
 yes)
    LSCF='-lscf'
esac
AC_SUBST([LSCF])

AC_CHECK_FUNC(
    [setppriv],
    [ans=yes],
    [ans=no]
)
case "$ans" in
 yes)
    AC_DEFINE([HAVE_SOLARIS_PRIVS], [1],
	[Are Solaris privileges available?])
esac


AC_CACHE_CHECK(
    [struct sockaddr for sa_len],
    [isc_cv_platform_havesalen],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#include <sys/socket.h>
	    ]],
	    [[
		extern struct sockaddr *ps;
		return ps->sa_len;
	    ]]
	)],
	[isc_cv_platform_havesalen=yes],
	[isc_cv_platform_havesalen=no]
    )]
)
case "$isc_cv_platform_havesalen" in
 yes)
    AC_DEFINE([ISC_PLATFORM_HAVESALEN], [1],
	 [struct sockaddr has sa_len?])
esac

AC_ARG_ENABLE(
    [ipv6],
    [AS_HELP_STRING(
	[--enable-ipv6],
	[s use IPv6?]
    )]
)

case "$enable_ipv6" in
 yes|''|autodetect)
    case "$host" in
     powerpc-ibm-aix4*)
	;;
     *)
	AC_DEFINE([WANT_IPV6], [1], [configure --enable-ipv6])
	;;
    esac
    ;;
 no)
    ;;
esac


dnl [Bug 1984] ntp/libisc fails to compile on OS X 10.7 (Lion)
case "$host" in
 *-*-darwin*)
    AC_DEFINE([__APPLE_USE_RFC_3542], [1], [Are we _special_?])
esac


AC_CACHE_CHECK(
    [for IPv6 structures],
    [isc_cv_found_ipv6],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
	    ]],
	    [[
		struct sockaddr_in6 sin6;
	    ]]
	)],
	[isc_cv_found_ipv6=yes],
	[isc_cv_found_ipv6=no]
    )]
)

#
# See whether IPv6 support is provided via a Kame add-on.
# This is done before other IPv6 linking tests so LIBS is properly set.
#
AC_MSG_CHECKING([for Kame IPv6 support])
AC_ARG_WITH(
    [kame],
    [AS_HELP_STRING(
	[--with-kame],
	[- =/usr/local/v6]
    )],
    [use_kame="$withval"],
    [use_kame="no"]
)
case "$use_kame" in
 no)
    ;;
 yes)
    kame_path=/usr/local/v6
    ;;
 *)
    kame_path="$use_kame"
    ;;
esac
case "$use_kame" in
 no)
    AC_MSG_RESULT([no])
    ;;
 *)
    if test -f $kame_path/lib/libinet6.a; then
	AC_MSG_RESULT([$kame_path/lib/libinet6.a])
	LIBS="-L$kame_path/lib -linet6 $LIBS"
    else
	AC_MSG_ERROR([$kame_path/lib/libinet6.a not found.

Please choose the proper path with the following command:

    configure --with-kame=PATH
])
    fi
    ;;
esac

#
# Whether netinet6/in6.h is needed has to be defined in isc/platform.h.
# Including it on Kame-using platforms is very bad, though, because
# Kame uses #error against direct inclusion.   So include it on only
# the platform that is otherwise broken without it -- BSD/OS 4.0 through 4.1.
# This is done before the in6_pktinfo check because that's what
# netinet6/in6.h is needed for.
#
case "$host" in
 *-bsdi4.[[01]]*)
    AC_DEFINE([ISC_PLATFORM_NEEDNETINET6IN6H], [1],
	[Do we need netinet6/in6.h?])
    isc_netinet6in6_hack="#include <netinet6/in6.h>"
    ;;
 *)
    isc_netinet6in6_hack=""
    ;;
esac

#
# This is similar to the netinet6/in6.h issue.
#
case "$host" in
 *-sco-sysv*uw*|*-*-sysv*UnixWare*|*-*-sysv*OpenUNIX*)
    AC_DEFINE([ISC_PLATFORM_FIXIN6ISADDR], [1],
	[Do we need to fix in6isaddr?])
    isc_netinetin6_hack="#include <netinet/in6.h>"
    ;;
 *)
    isc_netinetin6_hack=""
    ;;
esac


case "$isc_cv_found_ipv6" in
 yes)
    AC_DEFINE([ISC_PLATFORM_HAVEIPV6], [1], [have IPv6?])
    AC_CACHE_CHECK(
	[for in6_pktinfo],
	[isc_cv_have_in6_pktinfo],
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #include <sys/types.h>
		    #include <sys/socket.h>
		    #include <netinet/in.h>
		    $isc_netinetin6_hack
		    $isc_netinet6in6_hack
		]],
		[[
		    struct in6_pktinfo xyzzy;
		]]
	    )],
	    [isc_cv_have_in6_pktinfo=yes],
	    [isc_cv_have_in6_pktinfo=no]
	)]
    )
    case "$isc_cv_have_in6_pktinfo" in
     yes)
	AC_DEFINE([ISC_PLATFORM_HAVEIN6PKTINFO], [1],
		[have struct in6_pktinfo?])
    esac


    # HMS: Use HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID instead?
    AC_CACHE_CHECK(
	[for sockaddr_in6.sin6_scope_id],
	[isc_cv_have_sin6_scope_id],
	[AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #include <sys/types.h>
		    #include <sys/socket.h>
		    #include <netinet/in.h>
		    $isc_netinetin6_hack
		    $isc_netinet6in6_hack
		]],
		[[
		    struct sockaddr_in6 xyzzy;
		    xyzzy.sin6_scope_id = 0;
		]]
	    )],
	    [isc_cv_have_sin6_scope_id=yes],
	    [isc_cv_have_sin6_scope_id=no]
	)]
    )

    case "$isc_cv_have_sin6_scope_id" in
     yes)
	AC_DEFINE([ISC_PLATFORM_HAVESCOPEID], [1], [sin6_scope_id?])
    esac
esac


# We need this check run even without isc_cv_found_ipv6=yes

AC_CACHE_CHECK(
    [for in6addr_any],
    [isc_cv_have_in6addr_any],
    [AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#include <sys/socket.h>
		#include <netinet/in.h>
		$isc_netinetin6_hack
		$isc_netinet6in6_hack
	    ]],
	    [[
		struct in6_addr in6; 
		in6 = in6addr_any;
	    ]]
	)],
	[isc_cv_have_in6addr_any=yes],
	[isc_cv_have_in6addr_any=no]
    )]
)

case "$isc_cv_have_in6addr_any" in
 no)
    AC_DEFINE([ISC_PLATFORM_NEEDIN6ADDRANY], [1], [missing in6addr_any?])
esac


AC_CACHE_CHECK(
    [for struct if_laddrconf],
    [isc_cv_struct_if_laddrconf],
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#include <net/if6.h>
	    ]],
	    [[
		struct if_laddrconf a;
	    ]]
	)],
	[isc_cv_struct_if_laddrconf=yes],
	[isc_cv_struct_if_laddrconf=no]
    )]
)

case "$isc_cv_struct_if_laddrconf" in
 yes)
    AC_DEFINE([ISC_PLATFORM_HAVEIF_LADDRCONF], [1],
	[have struct if_laddrconf?])
esac

AC_CACHE_CHECK(
    [for struct if_laddrreq],
    isc_cv_struct_if_laddrreq,
    [AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
		#include <sys/types.h>
		#include <net/if6.h>
	    ]],
	    [[
		struct if_laddrreq a;
	    ]]
	)],
	[isc_cv_struct_if_laddrreq=yes],
	[isc_cv_struct_if_laddrreq=no]
    )]
)

case "$isc_cv_struct_if_laddrreq" in
 yes)
    AC_DEFINE([ISC_PLATFORM_HAVEIF_LADDRREQ], [1],
	[have struct if_laddrreq?])
esac

#
# Look for a sysctl call to get the list of network interfaces.
#
AC_CACHE_CHECK(
    [for interface list sysctl],
    [ntp_cv_iflist_sysctl],
    [AC_EGREP_CPP(
	[found_rt_iflist], 
	[
	    #include <sys/param.h>
	    #include <sys/sysctl.h>
	    #include <sys/socket.h>
	    #ifdef NET_RT_IFLIST  
		found_rt_iflist
	    #endif
	],
	[ntp_cv_iflist_sysctl=yes],
	[ntp_cv_iflist_sysctl=no]
    )]
)
case "$ntp_cv_iflist_sysctl" in
 yes)
    AC_DEFINE([HAVE_IFLIST_SYSCTL], [1], [have iflist_sysctl?])
esac

###

AC_MSG_CHECKING([if we want the saveconfig mechanism])
AC_ARG_ENABLE(
    [saveconfig],
    [AS_HELP_STRING(
	[--enable-saveconfig],
	[+ saveconfig mechanism]
    )],
    [ntp_ok=$enableval],
    [ntp_ok=yes]
)
ntp_saveconfig_enabled=0
if test "$ntp_ok" = "yes"; then
    ntp_saveconfig_enabled=1
    AC_DEFINE([SAVECONFIG], [1], [saveconfig mechanism])
fi
AM_CONDITIONAL([SAVECONFIG_ENABLED], [test x$ntp_saveconfig_enabled = x1])
AC_MSG_RESULT([$ntp_ok])

###

AC_DEFINE_DIR([NTP_KEYSDIR], [sysconfdir],
    [Default location of crypto key info])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([ElectricFence/Makefile])
AC_CONFIG_FILES([adjtimed/Makefile])
AC_CONFIG_FILES([clockstuff/Makefile])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([include/isc/Makefile])
AC_CONFIG_FILES([kernel/Makefile])
AC_CONFIG_FILES([kernel/sys/Makefile])
AC_CONFIG_FILES([libntp/Makefile])
AC_CONFIG_FILES([libparse/Makefile])
AC_CONFIG_FILES([ntpd/Makefile])
AC_CONFIG_FILES([ntpdate/Makefile])
AC_CONFIG_FILES([ntpdc/Makefile])
AC_CONFIG_FILES([ntpdc/nl.pl],		[chmod +x ntpdc/nl.pl])
AC_CONFIG_FILES([ntpq/Makefile])
AC_CONFIG_FILES([ntpsnmpd/Makefile])
AC_CONFIG_FILES([parseutil/Makefile])
AC_CONFIG_FILES([scripts/Makefile])
AC_CONFIG_FILES([scripts/calc_tickadj],	[chmod +x scripts/calc_tickadj])
AC_CONFIG_FILES([scripts/checktime],	[chmod +x scripts/checktime])
AC_CONFIG_FILES([scripts/freq_adj],	[chmod +x scripts/freq_adj])
AC_CONFIG_FILES([scripts/html2man],	[chmod +x scripts/html2man])
AC_CONFIG_FILES([scripts/mkver],	[chmod +x scripts/mkver])
AC_CONFIG_FILES([scripts/ntp-wait],	[chmod +x scripts/ntp-wait])
AC_CONFIG_FILES([scripts/ntpsweep],	[chmod +x scripts/ntpsweep])
AC_CONFIG_FILES([scripts/ntptrace],	[chmod +x scripts/ntptrace])
AC_CONFIG_FILES([scripts/ntpver],	[chmod +x scripts/ntpver])
AC_CONFIG_FILES([scripts/plot_summary],	[chmod +x scripts/plot_summary])
AC_CONFIG_FILES([scripts/summary],	[chmod +x scripts/summary])
AC_CONFIG_FILES([util/Makefile])

AC_CONFIG_SUBDIRS([sntp])

AC_OUTPUT
