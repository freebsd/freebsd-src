#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
m4_include([../version.m4])
AC_PREREQ([2.61])
AC_INIT([sntp], [VERSION_NUMBER])
AC_CONFIG_MACRO_DIR([../m4])
AC_CONFIG_AUX_DIR([.])

# Increment sntp_configure_cache_version by one for each change to
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

sntp_configure_cache_version=20091117

# When the version of config.cache and configure do not
# match, NTP_CACHEVERSION will flush the cache.

NTP_CACHEVERSION([sntp], [$sntp_configure_cache_version])

AM_INIT_AUTOMAKE
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
dnl the 'build' machine is where we run configure and compile
dnl the 'host' machine is where the resulting stuff runs.
AC_DEFINE_UNQUOTED([STR_SYSTEM], "$host", [canonical system (cpu-vendor-os) of where we should run])
AC_CONFIG_HEADER([config.h])
dnl AC_ARG_PROGRAM

# Checks for programs.
AM_PROG_CC_C_O
AC_PROG_CPP

# AC_PROG_CC_STDC has two functions.  It attempts to find a compiler
# capable of C99, or failing that, for C89.  CC is set afterward with
# the selected invocation, such as "gcc --std=gnu99".  Also, the
# ac_cv_prog_cc_stdc variable is no if the compiler selected for CC
# does not accept C89.

AC_PROG_CC_STDC

case "$ac_cv_prog_cc_stdc" in
 no)
    AC_MSG_WARN([ANSI C89/ISO C90 is the minimum to compile SNTP ]
		[version 4.2.5 and higher.])
esac

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

# HMS: These need to be moved to AM_CPPFLAGS and/or AM_CFLAGS
case "$host" in
 *-*-solaris*)
    # see "man standards".
    # -D_XOPEN_SOURCE=500 is probably OK for c89 and before
    # -D_XOPEN_SOURCE=600 seems OK for c99
    #CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=500 -D__EXTENSIONS__"
    CPPFLAGS="$CPPFLAGS -D__EXTENSIONS__"
    libxnet=-lxnet
    ;;
esac

AC_DISABLE_SHARED

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
LIBOPTS_CHECK

AC_PROG_LIBTOOL
NTP_DIR_SEP

# Checks for libraries.
NTP_LIB_M

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

AC_CHECK_FUNC([gethostent], ,
	[AC_SEARCH_LIBS([gethostent], [nsl], , , [$libxnet -lsocket])])
AC_CHECK_FUNC([openlog], , 
	[AC_SEARCH_LIBS([openlog], [gen], , 
		[AC_SEARCH_LIBS([openlog], [syslog], , , [$libxnet -lsocket])])])

AC_SEARCH_LIBS([setsockopt], [socket xnet])

# Checks for header files.
AC_HEADER_STDC

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

AC_CHECK_HEADERS([netdb.h netinet/in.h stdlib.h string.h strings.h syslog.h])
AC_CHECK_HEADERS([sys/socket.h sys/time.h])
AC_HEADER_TIME

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([time_t])

AC_C_INLINE

case "$ac_cv_c_inline" in
 '')
    ;;
 *)
    AC_DEFINE(HAVE_INLINE,1,[inline keyword or macro available])
    AC_SUBST(HAVE_INLINE)
esac

AC_C_CHAR_UNSIGNED		dnl CROSS_COMPILE?
AC_CHECK_SIZEOF([signed char])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])

AC_CHECK_TYPES([s_char])
case "$ac_cv_c_char_unsigned$ac_cv_sizeof_signed_char$ac_cv_type_s_char" in
 *yes)
    # We have a typedef for s_char.  Might as well believe it...
    ;;
 no0no)
    # We have signed chars, can't say 'signed char', no s_char typedef.
    AC_DEFINE([NEED_S_CHAR_TYPEDEF], 1, [Do we need an s_char typedef?])
    ;;
 no1no)
    # We have signed chars, can say 'signed char', no s_char typedef.
    AC_DEFINE([NEED_S_CHAR_TYPEDEF])
    ;;
 yes0no)
    # We have unsigned chars, can't say 'signed char', no s_char typedef.
    AC_MSG_ERROR([No way to specify a signed character!])
    ;;
 yes1no)
    # We have unsigned chars, can say 'signed char', no s_char typedef.
    AC_DEFINE([NEED_S_CHAR_TYPEDEF])
    ;;
esac
AC_TYPE_UID_T

NTP_OPENSSL

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
		   $ntp_cv_getsockname_socklen_type,
		   [What is getsockname()'s socklen type?])

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
	$ntp_cv_typeof_ip_multicast_loop,
	[What type to use for setsockopt])
esac

# HMS: We don't need res_init, but since we may be using cached 
# values from ntpd we need to test the same way 
AC_SEARCH_LIBS([res_init], [resolv], , , [-lsocket -lnsl]) 
AC_SEARCH_LIBS([inet_ntop], [resolv], , , [-lsocket -lnsl])
AC_CHECK_FUNC([inet_ntop], [], 
    [AC_DEFINE([ISC_PLATFORM_NEEDNTOP], [1], [ISC: provide inet_ntop()])])
AC_CHECK_FUNC([inet_pton], [],
    [AC_DEFINE([ISC_PLATFORM_NEEDPTON], [1], [ISC: provide inet_pton()])])

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


###

# Hacks
# these need work if we're to move libntp under sntp
AC_DEFINE([HAVE_NO_NICE], 1, [sntp does not care about 'nice'])
AC_DEFINE([HAVE_TERMIOS], 1, [sntp does not care about TTY stuff])

# Checks for library functions.
AC_CHECK_FUNCS([socket vsnprintf vsprintf])

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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
