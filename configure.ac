dnl Copyright (c) 1994, 1995, 1996, 1997
dnl	The Regents of the University of California.  All rights reserved.
dnl
dnl Process this file with autoconf to produce a configure script.
dnl

#
# See
#
#	https://ftp.gnu.org/gnu/config/README
#
# for the URLs to use to fetch new versions of config.guess and
# config.sub.
#

AC_PREREQ(2.69)
AC_INIT(tcpdump, m4_esyscmd_s([cat VERSION]))
AC_CONFIG_SRCDIR(tcpdump.c)

AC_CANONICAL_HOST

AC_LBL_C_INIT_BEFORE_CC(V_INCLS)
#
# Try to enable as many C99 features as we can.
# At minimum, we want C++/C99-style // comments.
#
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" = "no"; then
	AC_MSG_WARN([The C compiler does not support C99; there may be compiler errors])
fi
AC_LBL_C_INIT(V_CCOPT, V_INCLS)
AC_LBL_C_INLINE

AC_CHECK_HEADERS(fcntl.h rpc/rpc.h rpc/rpcent.h net/if.h)

case "$host_os" in

darwin*)
	AC_ARG_ENABLE(universal,
	AS_HELP_STRING([--disable-universal],[don't build universal on macOS]))
	if test "$enable_universal" != "no"; then
		case "$host_os" in

		darwin9.*)
			#
			# Leopard.  Build for x86 and 32-bit PowerPC, with
			# x86 first.  (That's what Apple does.)
			#
			V_CCOPT="$V_CCOPT -arch i386 -arch ppc"
			LDFLAGS="$LDFLAGS -arch i386 -arch ppc"
			;;

		darwin10.*)
			#
			# Snow Leopard.  Build for x86-64 and x86, with
			# x86-64 first.  (That's what Apple does.)
			#
			V_CCOPT="$V_CCOPT -arch x86_64 -arch i386"
			LDFLAGS="$LDFLAGS -arch x86_64 -arch i386"
			;;
		esac
	fi
	;;
esac


AC_ARG_WITH([smi],
   [AS_HELP_STRING([--with-smi],
     [link with libsmi (allows to load MIBs on the fly to decode SNMP packets) [default=yes, if available]])],
   [],
   [with_smi=yes])

if test "x$with_smi" != "xno" ; then
	AC_CHECK_HEADER(smi.h,
	[
		#
		# OK, we found smi.h.  Do we have libsmi with smiInit?
		#
		AC_CHECK_LIB(smi, smiInit,
		[
			#
			# OK, we have libsmi with smiInit.  Can we use it?
			#
			AC_MSG_CHECKING([whether to enable libsmi])
			savedlibs="$LIBS"
			LIBS="-lsmi $LIBS"
			AC_TRY_RUN(
				[
/* libsmi available check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <smi.h>
int main()
{
  int current, revision, age, n;
  const int required = 2;
  if (smiInit(""))
    exit(1);
  if (strcmp(SMI_LIBRARY_VERSION, smi_library_version))
    exit(2);
  n = sscanf(smi_library_version, "%d:%d:%d", &current, &revision, &age);
  if (n != 3)
    exit(3);
  if (required < current - age || required > current)
    exit(4);
  exit(0);
}
				],
				[
					AC_MSG_RESULT(yes)
					AC_DEFINE(USE_LIBSMI, 1,
					    [Define if you enable support for libsmi])
				],
				[
					dnl autoconf documentation says that
					dnl $? contains the exit value.
					dnl reality is that it does not.
					dnl We leave this in just in case
					dnl autoconf ever comes back to
					dnl match the documentation.
					case $? in
					  1) AC_MSG_RESULT(no - smiInit failed) ;;
					  2) AC_MSG_RESULT(no - header/library version mismatch) ;;
					  3) AC_MSG_RESULT(no - can't determine library version) ;;
					  4) AC_MSG_RESULT(no - too old) ;;
					  *) AC_MSG_RESULT(no) ;;
					esac
					LIBS="$savedlibs"
				],
				[
					AC_MSG_RESULT(not when cross-compiling)
					LIBS="$savedlibs"
				]
			)
		])
	])
fi

AC_MSG_CHECKING([whether to enable the possibly-buggy SMB printer])
AC_ARG_ENABLE([smb],
   [AS_HELP_STRING([--enable-smb],
     [enable possibly-buggy SMB printer [default=no]])],
   [],
   [enableval=no])
case "$enableval" in
yes)	AC_MSG_RESULT(yes)
	AC_DEFINE(ENABLE_SMB, 1,
	    [define if you want to build the possibly-buggy SMB printer])
	LOCALSRC="print-smb.c smbutil.c $LOCALSRC"
	;;
*)	AC_MSG_RESULT(no)
	;;
esac

AC_ARG_WITH(user, [  --with-user=USERNAME    drop privileges by default to USERNAME])
AC_MSG_CHECKING([whether to drop root privileges by default])
if test ! -z "$with_user" ; then
       AC_DEFINE_UNQUOTED(WITH_USER, "$withval",
           [define if should drop privileges by default])
       AC_MSG_RESULT(to \"$withval\")
else
       AC_MSG_RESULT(no)
fi

AC_ARG_WITH(chroot, [  --with-chroot=DIRECTORY when dropping privileges, chroot to DIRECTORY])
AC_MSG_CHECKING([whether to chroot])
if test ! -z "$with_chroot" && test "$with_chroot" != "no" ; then
       AC_DEFINE_UNQUOTED(WITH_CHROOT, "$withval",
           [define if should chroot when dropping privileges])
       AC_MSG_RESULT(to \"$withval\")
else
       AC_MSG_RESULT(no)
fi

AC_ARG_WITH(sandbox-capsicum,
	AS_HELP_STRING([--with-sandbox-capsicum],
		       [use Capsicum security functions @<:@default=yes, if available@:>@]))
#
# Check whether various functions are available.  If any are, set
# ac_lbl_capsicum_function_seen to yes; if any are not, set
# ac_lbl_capsicum_function_not_seen to yes.
#
# We don't check cap_rights_init(), as it's a macro, wrapping another
# function, in at least some versions of FreeBSD, and AC_CHECK_FUNCS()
# doesn't handle that.
#
# All of the ones we check for must be available in order to enable
# capsicum sandboxing.
#
# XXX - do we need to check for all of them, or are there some that, if
# present, imply others are present?
#
if test -z "$with_sandbox_capsicum" || test "$with_sandbox_capsicum" != "no" ; then
	#
	# First, make sure we have the required header.
	#
	AC_CHECK_HEADER(sys/capsicum.h,
	[
		#
		# We do; now make sure we have the required functions.
		#
		AC_CHECK_FUNCS(cap_enter cap_rights_limit cap_ioctls_limit openat,
		    ac_lbl_capsicum_function_seen=yes,
		    ac_lbl_capsicum_function_not_seen=yes)
	])
	AC_CHECK_LIB(casper, cap_init, LIBS="$LIBS -lcasper")
	AC_CHECK_LIB(cap_dns, cap_gethostbyaddr, LIBS="$LIBS -lcap_dns")
fi
AC_MSG_CHECKING([whether to sandbox using capsicum])
if test "x$ac_lbl_capsicum_function_seen" = "xyes" -a "x$ac_lbl_capsicum_function_not_seen" != "xyes"; then
	AC_DEFINE(HAVE_CAPSICUM, 1, [capsicum support available])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AC_MSG_CHECKING([whether to sandbox using Casper library])
if test "x$ac_cv_lib_casper_cap_init" = "xyes" -a "x$ac_cv_lib_cap_dns_cap_gethostbyaddr" = "xyes"; then
	AC_DEFINE(HAVE_CASPER, 1, [Casper support available])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

#
# We must check this before checking whether to check the OS's IPv6,
# support because, on some platforms (such as SunOS 5.x), the test
# program requires the extra networking libraries.
#
AC_LBL_LIBRARY_NET

#
# Check whether AF_INET6 and struct in6_addr are defined.
# If they aren't both defined, we don't have sufficient OS
# support for IPv6, so we don't look for IPv6 support libraries,
# and we define AF_INET6 and struct in6_addr ourselves.
#
AC_MSG_CHECKING([whether the operating system supports IPv6])
AC_COMPILE_IFELSE(
    [
      AC_LANG_SOURCE(
	[[
#include <string.h>
/* AF_INET6 available check */
#include <sys/types.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#ifdef AF_INET6
void
foo(struct in6_addr *addr)
{
 memset(addr, 0, sizeof (struct in6_addr));
}
#else
#error "AF_INET6 not defined"
#endif
	]])
    ],
    [
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_OS_IPV6_SUPPORT, 1,
	    [define if the OS provides AF_INET6 and struct in6_addr])
	ipv6=yes
    ],
    [
	AC_MSG_RESULT(no)
	ipv6=no
    ]
)

ipv6type=unknown
ipv6lib=none
ipv6trylibc=no

if test "$ipv6" = "yes"; then
	AC_MSG_CHECKING([ipv6 stack type])
	for i in inria kame linux-glibc linux-libinet6 toshiba v6d zeta; do
		case $i in
		inria)
			dnl http://www.kame.net/
			AC_EGREP_CPP(yes,
[#include <netinet/in.h>
#ifdef IPV6_INRIA_VERSION
yes
#endif],
				[ipv6type=$i])
			;;
		kame)
			dnl http://www.kame.net/
			AC_EGREP_CPP(yes,
[#include <netinet/in.h>
#ifdef __KAME__
yes
#endif],
				[ipv6type=$i;
				ipv6lib=inet6;
				ipv6libdir=/usr/local/v6/lib;
				ipv6trylibc=yes])
			;;
		linux-glibc)
			dnl http://www.v6.linux.or.jp/
			AC_EGREP_CPP(yes,
[#include <features.h>
#if defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
yes
#endif],
				[ipv6type=$i])
			;;
		linux-libinet6)
			dnl http://www.v6.linux.or.jp/
			dnl
			dnl This also matches Solaris 8 and Tru64 UNIX 5.1,
			dnl and possibly other versions of those OSes
			dnl
			if test -d /usr/inet6 -o -f /usr/include/netinet/ip6.h; then
				ipv6type=$i
				ipv6lib=inet6
				ipv6libdir=/usr/inet6/lib
				ipv6trylibc=yes;
				CFLAGS="-I/usr/inet6/include $CFLAGS"
			fi
			;;
		toshiba)
			AC_EGREP_CPP(yes,
[#include <sys/param.h>
#ifdef _TOSHIBA_INET6
yes
#endif],
				[ipv6type=$i;
				ipv6lib=inet6;
				ipv6libdir=/usr/local/v6/lib])
			;;
		v6d)
			AC_EGREP_CPP(yes,
[#include </usr/local/v6/include/sys/v6config.h>
#ifdef __V6D__
yes
#endif],
				[ipv6type=$i;
				ipv6lib=v6;
				ipv6libdir=/usr/local/v6/lib;
				CFLAGS="-I/usr/local/v6/include $CFLAGS"])
			;;
		zeta)
			AC_EGREP_CPP(yes,
[#include <sys/param.h>
#ifdef _ZETA_MINAMI_INET6
yes
#endif],
				[ipv6type=$i;
				ipv6lib=inet6;
				ipv6libdir=/usr/local/v6/lib])
			;;
		esac
		if test "$ipv6type" != "unknown"; then
			break
		fi
	done
	AC_MSG_RESULT($ipv6type)
fi

if test "$ipv6" = "yes" -a "$ipv6lib" != "none"; then
	if test -d $ipv6libdir -a -f $ipv6libdir/lib$ipv6lib.a; then
		LIBS="-L$ipv6libdir -l$ipv6lib $LIBS"
		echo "You have $ipv6lib library, using it"
	else
		if test "$ipv6trylibc" = "yes"; then
			echo "You do not have $ipv6lib library, using libc"
		else
			echo 'Fatal: no $ipv6lib library found.  cannot continue.'
			echo "You need to fetch lib$ipv6lib.a from appropriate"
			echo 'ipv6 kit and compile beforehand.'
			exit 1
		fi
	fi
fi

AC_REPLACE_FUNCS(strlcat strlcpy strdup strsep getservent getopt_long)
AC_CHECK_FUNCS(fork vfork)
AC_CHECK_FUNCS(setlinebuf)

#
# Make sure we have vsnprintf() and snprintf(); we require them.
#
AC_CHECK_FUNC(vsnprintf,,
    AC_MSG_ERROR([vsnprintf() is required but wasn't found]))
AC_CHECK_FUNC(snprintf,,
    AC_MSG_ERROR([snprintf() is required but wasn't found]))

#
# Define HAVE_NO_PRINTF_Z to make it possible to disable test cases that
# depend on %zu.
#
AC_MSG_CHECKING([whether printf(3) supports the z length modifier])
AC_RUN_IFELSE(
    [
        AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>

int main()
{
  char buf[100];
  snprintf(buf, sizeof(buf), "%zu", sizeof(buf));
  return strncmp(buf, "100", sizeof(buf)) ? 1 : 0;
}
        ]])
    ],
    [
        AC_MSG_RESULT(yes)
    ],
    [
        AC_MSG_RESULT(no)
        AC_DEFINE(HAVE_NO_PRINTF_Z, 1,
            [Define to 1 if printf(3) does not support the z length modifier.])
    ],
    [
        AC_MSG_RESULT(not while cross-compiling)
    ]
)

AC_CHECK_LIB(rpc, main)		dnl It's unclear why we might need -lrpc

dnl Some platforms may need -lnsl for getrpcbynumber.
AC_SEARCH_LIBS(getrpcbynumber, nsl,
    AC_DEFINE(HAVE_GETRPCBYNUMBER, 1, [define if you have getrpcbynumber()]))

AC_LBL_LIBPCAP(V_PCAPDEP, V_INCLS)

#
# Check for these after AC_LBL_LIBPCAP, so we link with the appropriate
# libraries (e.g., "-lsocket -lnsl" on Solaris).
#
# You are in a twisty little maze of UN*Xes, all different.
# Some might not have ether_ntohost().
# Some might have it and declare it in <net/ethernet.h>.
# Some might have it and declare it in <netinet/ether.h>
# Some might have it and declare it in <sys/ethernet.h>.
# Some might have it and declare it in <arpa/inet.h>.
# Some might have it and declare it in <netinet/if_ether.h>.
# Some might have it and not declare it in any header file.
#
# Before you is a C compiler.
#
AC_CHECK_FUNCS(ether_ntohost, [
    AC_CACHE_CHECK(for buggy ether_ntohost, ac_cv_buggy_ether_ntohost, [
	AC_TRY_RUN([
		#include <netdb.h>
		#include <netinet/ether.h>
		#include <stdlib.h>
		#include <sys/types.h>
		#include <sys/param.h>
		#include <sys/socket.h>

		int
		main(int argc, char **argv)
		{
			u_char ea[6] = { 0xff, 0xff, 0xff, 0xff, 0xff };
			char name[MAXHOSTNAMELEN];

			ether_ntohost(name, (struct ether_addr *)ea);
			exit(0);
		}
	], [ac_cv_buggy_ether_ntohost=no],
	   [ac_cv_buggy_ether_ntohost=yes],
	   [ac_cv_buggy_ether_ntohost="not while cross-compiling"])])
    if test "$ac_cv_buggy_ether_ntohost" = "no"; then
	AC_DEFINE(USE_ETHER_NTOHOST, 1,
	    [define if you have ether_ntohost() and it works])
    fi
])
if test "$ac_cv_func_ether_ntohost" = yes -a \
    "$ac_cv_buggy_ether_ntohost" = "no"; then
	#
	# OK, we have ether_ntohost().  Is it declared in <net/ethernet.h>?
	#
	# This test fails if we don't have <net/ethernet.h> or if we do
	# but it doesn't declare ether_ntohost().
	#
	AC_CHECK_DECL(ether_ntohost,
	    [
		AC_DEFINE(NET_ETHERNET_H_DECLARES_ETHER_NTOHOST,,
		    [Define to 1 if net/ethernet.h declares `ether_ntohost'])
	    ],,
	    [
#include <net/ethernet.h>
	    ])
	#
	# Did that succeed?
	#
	if test "$ac_cv_have_decl_ether_ntohost" != yes; then
		#
		# No, how about <netinet/ether.h>, as on Linux?
		#
		# This test fails if we don't have <netinet/ether.h>
		# or if we do but it doesn't declare ether_ntohost().
		#
		# Unset ac_cv_have_decl_ether_ntohost so we don't
		# treat the previous failure as a cached value and
		# suppress the next test.
		#
		unset ac_cv_have_decl_ether_ntohost
		AC_CHECK_DECL(ether_ntohost,
		    [
			AC_DEFINE(NETINET_ETHER_H_DECLARES_ETHER_NTOHOST,,
			    [Define to 1 if netinet/ether.h declares `ether_ntohost'])
		    ],,
		    [
#include <netinet/ether.h>
		    ])
	fi
	#
	# Did that succeed?
	#
	if test "$ac_cv_have_decl_ether_ntohost" != yes; then
		#
		# No, how about <sys/ethernet.h>, as on Solaris 10
		# and later?
		#
		# This test fails if we don't have <sys/ethernet.h>
		# or if we do but it doesn't declare ether_ntohost().
		#
		# Unset ac_cv_have_decl_ether_ntohost so we don't
		# treat the previous failure as a cached value and
		# suppress the next test.
		#
		unset ac_cv_have_decl_ether_ntohost
		AC_CHECK_DECL(ether_ntohost,
		    [
			AC_DEFINE(SYS_ETHERNET_H_DECLARES_ETHER_NTOHOST,,
			    [Define to 1 if sys/ethernet.h declares `ether_ntohost'])
		    ],,
		    [
#include <sys/ethernet.h>
		    ])
	fi
	#
	# Did that succeed?
	#
	if test "$ac_cv_have_decl_ether_ntohost" != yes; then
		#
		# No, how about <arpa/inet.h>, as in AIX?
		#
		# This test fails if we don't have <arpa/inet.h>
		# (if we have ether_ntohost(), we should have
		# networking, and if we have networking, we should
		# have <arpa/inet.h>) or if we do but it doesn't
		# declare ether_ntohost().
		#
		# Unset ac_cv_have_decl_ether_ntohost so we don't
		# treat the previous failure as a cached value and
		# suppress the next test.
		#
		unset ac_cv_have_decl_ether_ntohost
		AC_CHECK_DECL(ether_ntohost,
		    [
			AC_DEFINE(ARPA_INET_H_DECLARES_ETHER_NTOHOST,,
			    [Define to 1 if arpa/inet.h declares `ether_ntohost'])
		    ],,
		    [
#include <arpa/inet.h>
		    ])
	fi
	#
	# Did that succeed?
	#
	if test "$ac_cv_have_decl_ether_ntohost" != yes; then
		#
		# No, how about <netinet/if_ether.h>?
		# On some platforms, it requires <net/if.h> and
		# <netinet/in.h>, and we always include it with
		# both of them, so test it with both of them.
		#
		# This test fails if we don't have <netinet/if_ether.h>
		# and the headers we include before it, or if we do but
		# <netinet/if_ether.h> doesn't declare ether_hostton().
		#
		# Unset ac_cv_have_decl_ether_ntohost so we don't
		# treat the previous failure as a cached value and
		# suppress the next test.
		#
		unset ac_cv_have_decl_ether_ntohost
		AC_CHECK_DECL(ether_ntohost,
		    [
			AC_DEFINE(NETINET_IF_ETHER_H_DECLARES_ETHER_NTOHOST,,
			    [Define to 1 if netinet/if_ether.h declares `ether_ntohost'])
		    ],,
		    [
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
		    ])
	fi
	#
	# After all that, is ether_ntohost() declared?
	#
	if test "$ac_cv_have_decl_ether_ntohost" = yes; then
		#
		# Yes.
		#
		AC_DEFINE(HAVE_DECL_ETHER_NTOHOST, 1,
		    [Define to 1 if you have the declaration of `ether_ntohost'])
	else
		#
		# No, we'll have to declare it ourselves.
		# Do we have "struct ether_addr" if we include
		# <netinet/if_ether.h>?
		#
		AC_CHECK_TYPES(struct ether_addr,,,
		    [
			#include <sys/types.h>
			#include <sys/socket.h>
			#include <net/if.h>
			#include <netinet/in.h>
			#include <netinet/if_ether.h>
		    ])
	fi
fi

dnl
dnl Check for "pcap_list_datalinks()" and use a substitute version if
dnl it's not present.  If it is present, check for "pcap_free_datalinks()";
dnl if it's not present, we don't replace it for now.  (We could do so
dnl on UN*X, but not on Windows, where hilarity ensues if a program
dnl built with one version of the MSVC support library tries to free
dnl something allocated by a library built with another version of
dnl the MSVC support library.)
dnl
AC_CHECK_FUNC(pcap_list_datalinks,
	[
	    AC_DEFINE(HAVE_PCAP_LIST_DATALINKS, 1,
		[define if libpcap has pcap_list_datalinks()])
	    AC_CHECK_FUNCS(pcap_free_datalinks)
	],
	[
	    AC_LIBOBJ(datalinks)
	])

dnl
dnl Check for "pcap_datalink_name_to_val()", and use a substitute
dnl version if it's not present.  If it is present, check for
dnl "pcap_datalink_val_to_description()", and if we don't have it,
dnl use a substitute version.
dnl
AC_CHECK_FUNC(pcap_datalink_name_to_val,
	[
	    AC_DEFINE(HAVE_PCAP_DATALINK_NAME_TO_VAL, 1,
		[define if libpcap has pcap_datalink_name_to_val()])
	    AC_CHECK_FUNC(pcap_datalink_val_to_description,
		AC_DEFINE(HAVE_PCAP_DATALINK_VAL_TO_DESCRIPTION, 1,
		    [define if libpcap has pcap_datalink_val_to_description()]),
		[
		    AC_LIBOBJ(dlnames)
		])
	],
	[
	    AC_LIBOBJ(dlnames)
	])

dnl
dnl Check for "pcap_set_datalink()"; you can't substitute for it if
dnl it's absent (it has hooks into libpcap), so just define the
dnl HAVE_ value if it's there.
dnl
AC_CHECK_FUNCS(pcap_set_datalink)

dnl
dnl Check for "pcap_breakloop()"; you can't substitute for it if
dnl it's absent (it has hooks into the live capture routines),
dnl so just define the HAVE_ value if it's there.
dnl
AC_CHECK_FUNCS(pcap_breakloop)

#
# Do we have the new open API?  Check for pcap_create, and assume that,
# if we do, we also have pcap_activate() and the other new routines
# introduced in libpcap 1.0.0.
#
AC_CHECK_FUNCS(pcap_create)
if test $ac_cv_func_pcap_create = "yes" ; then
	#
	# OK, do we have pcap_set_tstamp_type?  If so, assume we have
	# pcap_list_tstamp_types and pcap_free_tstamp_types as well.
	#
	AC_CHECK_FUNCS(pcap_set_tstamp_type)
	#
	# And do we have pcap_set_tstamp_precision?  If so, we assume
	# we also have pcap_open_offline_with_tstamp_precision.
	#
	AC_CHECK_FUNCS(pcap_set_tstamp_precision)
fi

#
# Check for a miscellaneous collection of functions which we use
# if we have them.
#
AC_CHECK_FUNCS(pcap_findalldevs)
if test $ac_cv_func_pcap_findalldevs = "yes" ; then
dnl Check for libpcap having pcap_findalldevs() but the pcap.h header
dnl not having pcap_if_t; some versions of Mac OS X shipped with pcap.h
dnl from 0.6 and libpcap 0.8, so that libpcap had pcap_findalldevs but
dnl pcap.h didn't have pcap_if_t.
    savedcppflags="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $V_INCLS"
    AC_CHECK_TYPES(pcap_if_t, , , [#include <pcap.h>])
    CPPFLAGS="$savedcppflags"
fi
AC_CHECK_FUNCS(pcap_dump_flush pcap_lib_version)
if test $ac_cv_func_pcap_lib_version = "no" ; then
    AC_MSG_CHECKING(whether pcap_version is defined by libpcap)
    AC_TRY_LINK([],
       [
	extern char pcap_version[];

	return (int)pcap_version;
       ],
       ac_lbl_cv_pcap_version_defined=yes,
       ac_lbl_cv_pcap_version_defined=no)
    if test "$ac_lbl_cv_pcap_version_defined" = yes ; then
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_PCAP_VERSION, 1, [define if libpcap has pcap_version])
    else
	AC_MSG_RESULT(no)
    fi
fi
AC_CHECK_FUNCS(pcap_setdirection pcap_set_immediate_mode pcap_dump_ftell64)
AC_CHECK_FUNCS(pcap_open pcap_findalldevs_ex)
AC_REPLACE_FUNCS(pcap_dump_ftell)

#
# Check for special debugging functions
#
AC_CHECK_FUNCS(pcap_set_parser_debug)
if test "$ac_cv_func_pcap_set_parser_debug" = "no" ; then
	#
	# OK, we don't have pcap_set_parser_debug() to set the libpcap
	# filter expression parser debug flag; can we directly set the
	# flag?
	AC_MSG_CHECKING(whether pcap_debug is defined by libpcap)
	AC_TRY_LINK([],
	   [
		extern int pcap_debug;

		return pcap_debug;
	   ],
	   ac_lbl_cv_pcap_debug_defined=yes,
	   ac_lbl_cv_pcap_debug_defined=no)
	if test "$ac_lbl_cv_pcap_debug_defined" = yes ; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_PCAP_DEBUG, 1, [define if libpcap has pcap_debug])
	else
		AC_MSG_RESULT(no)
		#
		# OK, what about "yydebug"?
		#
		AC_MSG_CHECKING(whether yydebug is defined by libpcap)
		AC_TRY_LINK([],
		   [
			extern int yydebug;

			return yydebug;
		   ],
		   ac_lbl_cv_yydebug_defined=yes,
		   ac_lbl_cv_yydebug_defined=no)
		if test "$ac_lbl_cv_yydebug_defined" = yes ; then
			AC_MSG_RESULT(yes)
			AC_DEFINE(HAVE_YYDEBUG, 1, [define if libpcap has yydebug])
		else
			AC_MSG_RESULT(no)
		fi
	fi
fi
AC_CHECK_FUNCS(pcap_set_optimizer_debug)
AC_REPLACE_FUNCS(bpf_dump)	dnl moved to libpcap in 0.6

V_GROUP=0
if test -f /etc/group -a ! -z "`grep '^wheel:' /etc/group`" ; then
	V_GROUP=wheel
fi
#
# Assume V7/BSD convention for man pages (file formats in section 5,
# miscellaneous info in section 7).
#
MAN_FILE_FORMATS=5
MAN_MISC_INFO=7
case "$host_os" in

aix*)
	dnl Workaround to enable certain features
	AC_DEFINE(_SUN,1,[define on AIX to get certain functions])
	;;

hpux*)
	#
	# Use System V conventions for man pages.
	#
	MAN_FILE_FORMATS=4
	MAN_MISC_INFO=5
	;;

irix*)
	V_GROUP=sys

	#
	# Use System V conventions for man pages.
	#
	MAN_FILE_FORMATS=4
	MAN_MISC_INFO=5
	;;

osf*)
	V_GROUP=system

	#
	# Use System V conventions for man pages.
	#
	MAN_FILE_FORMATS=4
	MAN_MISC_INFO=5
	;;

solaris*)
	V_GROUP=sys

	#
	# Use System V conventions for man pages.
	#
	MAN_FILE_FORMATS=4
	MAN_MISC_INFO=5
	;;
esac

if test -f /dev/bpf0 ; then
	V_GROUP=bpf
fi

#
# Make sure we have a definition for C99's uintptr_t (regardless of
# whether the environment is a C99 environment or not).
#
AC_TYPE_UINTPTR_T

#
# Check whether we have pcap/pcap-inttypes.h.
# If we do, we use that to get the C99 types defined.
#
savedcppflags="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $V_INCLS"
AC_CHECK_HEADERS(pcap/pcap-inttypes.h)
CPPFLAGS="$savedcppflags"

#
# Define the old BSD specified-width types in terms of the C99 types;
# we may need them with libpcap include files.
#
AC_CHECK_TYPE([u_int8_t], ,
	[AC_DEFINE([u_int8_t], [uint8_t],
	[Define to `uint8_t' if u_int8_t not defined.])],
	[AC_INCLUDES_DEFAULT
#include <sys/types.h>
])
AC_CHECK_TYPE([u_int16_t], ,
	[AC_DEFINE([u_int16_t], [uint16_t],
	[Define to `uint16_t' if u_int16_t not defined.])],
	[AC_INCLUDES_DEFAULT
#include <sys/types.h>
])
AC_CHECK_TYPE([u_int32_t], ,
	[AC_DEFINE([u_int32_t], [uint32_t],
	[Define to `uint32_t' if u_int32_t not defined.])],
	[AC_INCLUDES_DEFAULT
#include <sys/types.h>
])
AC_CHECK_TYPE([u_int64_t], ,
	[AC_DEFINE([u_int64_t], [uint64_t],
	[Define to `uint64_t' if u_int64_t not defined.])],
	[AC_INCLUDES_DEFAULT
#include <sys/types.h>
])

AC_PROG_RANLIB
AC_CHECK_TOOL([AR], [ar])

AC_LBL_DEVEL(V_CCOPT)

# Check for OpenSSL/libressl libcrypto
AC_MSG_CHECKING(whether to use OpenSSL/libressl libcrypto)
# Specify location for both includes and libraries.
want_libcrypto=ifavailable
AC_ARG_WITH(crypto,
    AS_HELP_STRING([--with-crypto]@<:@=DIR@:>@,
		   [use OpenSSL/libressl libcrypto (located in directory DIR, if specified) @<:@default=yes, if available@:>@]),
[
	if test $withval = no
	then
		# User doesn't want to link with libcrypto.
		want_libcrypto=no
		AC_MSG_RESULT(no)
	elif test $withval = yes
	then
		# User wants to link with libcrypto but hasn't specified
		# a directory.
		want_libcrypto=yes
		AC_MSG_RESULT(yes)
	else
		# User wants to link with libcrypto and has specified
		# a directory, so use the provided value.
		want_libcrypto=yes
		libcrypto_root=$withval
		AC_MSG_RESULT([yes, using the version installed in $withval])

		#
		# Put the subdirectories of the libcrypto root directory
		# at the front of the header and library search path.
		#
		CFLAGS="-I$withval/include $CFLAGS"
		LIBS="-L$withval/lib $LIBS"
	fi
],[
	#
	# Use libcrypto if it's present, otherwise don't; no directory
	# was specified.
	#
	want_libcrypto=ifavailable
	AC_MSG_RESULT([yes, if available])
])
if test "$want_libcrypto" != "no"; then
	#
	# Don't check for libcrypto unless we have its headers;
	# Apple, bless their pointy little heads, apparently ship
	# libcrypto as a library, but not the header files, in
	# El Capitan, probably because they don't want you writing
	# nasty portable code that could run on other UN*Xes, they
	# want you writing code that uses their Shiny New Crypto
	# Library and that only runs on macOS.
	#
	AC_CHECK_HEADER(openssl/crypto.h,
	[
		AC_CHECK_LIB(crypto, DES_cbc_encrypt)
		if test "$ac_cv_lib_crypto_DES_cbc_encrypt" = "yes"; then
			AC_CHECK_HEADERS(openssl/evp.h)
			#
			# OK, then:
			#
			# 1) do we have EVP_CIPHER_CTX_new?
			# If so, we use it to allocate an
			# EVP_CIPHER_CTX, as EVP_CIPHER_CTX may be
			# opaque; otherwise, we allocate it ourselves.
			#
			# 2) do we have EVP_DecryptInit_ex()?
			# If so, we use it, because we need to be
			# able to make two "initialize the cipher"
			# calls, one with the cipher and key, and
			# one with the IV, and, as of OpenSSL 1.1,
			# You Can't Do That with EVP_DecryptInit(),
			# because a call to EVP_DecryptInit() will
			# unconditionally clear the context, and
			# if you don't supply a cipher, it'll
			# clear the cipher, rendering the context
			# unusable and causing a crash.
			#
			AC_CHECK_FUNCS(EVP_CIPHER_CTX_new EVP_DecryptInit_ex)
		fi
	])
fi

# Check for libcap-ng
AC_MSG_CHECKING(whether to use libcap-ng)
# Specify location for both includes and libraries.
want_libcap_ng=ifavailable
AC_ARG_WITH(cap_ng,
    AS_HELP_STRING([--with-cap-ng],
		   [use libcap-ng @<:@default=yes, if available@:>@]),
[
	if test $withval = no
	then
		want_libcap_ng=no
		AC_MSG_RESULT(no)
	elif test $withval = yes
	then
		want_libcap_ng=yes
		AC_MSG_RESULT(yes)
	fi
],[
	#
	# Use libcap-ng if it's present, otherwise don't.
	#
	want_libcap_ng=ifavailable
	AC_MSG_RESULT([yes, if available])
])
if test "$want_libcap_ng" != "no"; then
	AC_CHECK_LIB(cap-ng, capng_change_id)
	AC_CHECK_HEADERS(cap-ng.h)
fi

dnl
dnl set additional include path if necessary
if test "$missing_includes" = "yes"; then
	CPPFLAGS="$CPPFLAGS -I$srcdir/missing"
	V_INCLS="$V_INCLS -I$srcdir/missing"
fi

AC_SUBST(V_CCOPT)
AC_SUBST(V_DEFS)
AC_SUBST(V_GROUP)
AC_SUBST(V_INCLS)
AC_SUBST(V_PCAPDEP)
AC_SUBST(LOCALSRC)
AC_SUBST(MAN_FILE_FORMATS)
AC_SUBST(MAN_MISC_INFO)

AC_PROG_INSTALL

AC_CONFIG_HEADER(config.h)

AC_OUTPUT_COMMANDS([if test -f .devel; then
	echo timestamp > stamp-h
	cat $srcdir/Makefile-devel-adds >> Makefile
	make depend || exit 1
fi])
AC_OUTPUT(Makefile tcpdump.1)
exit 0
