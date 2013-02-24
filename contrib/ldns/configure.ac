#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.56)
sinclude(acx_nlnetlabs.m4)

# must be numbers. ac_defun because of later processing.
m4_define([VERSION_MAJOR],[1])
m4_define([VERSION_MINOR],[6])
m4_define([VERSION_MICRO],[16])
AC_INIT(ldns, m4_defn([VERSION_MAJOR]).m4_defn([VERSION_MINOR]).m4_defn([VERSION_MICRO]), libdns@nlnetlabs.nl, libdns)
AC_CONFIG_SRCDIR([packet.c])
# needed to build correct soname
AC_SUBST(LIBTOOL_VERSION_INFO, VERSION_MAJOR:VERSION_MINOR:VERSION_MICRO)
AC_SUBST(LDNS_VERSION_MAJOR, [VERSION_MAJOR])
AC_SUBST(LDNS_VERSION_MINOR, [VERSION_MINOR])
AC_SUBST(LDNS_VERSION_MICRO, [VERSION_MICRO])

OURCPPFLAGS=''
CPPFLAGS=${CPPFLAGS:-${OURCPPFLAGS}}
CFLAGS="$CFLAGS"

AC_AIX
# Checks for programs.
AC_PROG_CC
ACX_DEPFLAG
AC_PROG_MAKE_SET

# Extra (sp)lint flags for NetBSD
AC_CANONICAL_HOST
case "$host_os" in
	netbsd*) LINTFLAGS="'-D__RENAME(x)=' -D_NETINET_IN_H_ $LINTFLAGS" 
		 ;;
	*)       LINTFLAGS="$LINTFLAGS"
		 ;;
esac
AC_SUBST(LINTFLAGS)

AC_DEFINE(WINVER, 0x0502, [the version of the windows API enabled])

ACX_CHECK_COMPILER_FLAG(std=c99, [C99FLAG="-std=c99"])
ACX_CHECK_COMPILER_FLAG(xc99, [C99FLAG="-xc99"])

# routine to copy files
# argument 1 is a list of files (relative to the source dir)
# argument 2 is a destination directory (relative to the current
# working directory
AC_DEFUN([COPY_FILES],
[
for file in $1; do
	sh $srcdir/install-sh -m 644 $file $2
done
])

# copy all .h files in the dir at argument 1
# (relative to source) to the dir at argument 2
# (relative to current dir)
AC_DEFUN([COPY_HEADER_FILES],
[
echo "copying header files"
COPY_FILES($srcdir/$1/*.h, $2)
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_LANG_C
if test "x$CFLAGS" = "x" ; then
ACX_CHECK_COMPILER_FLAG(g, [CFLAGS="-g"])
ACX_CHECK_COMPILER_FLAG(O2, [CFLAGS="-O2 $CFLAGS"])
fi
ACX_CHECK_COMPILER_FLAG(Wall, [CFLAGS="-Wall $CFLAGS"])
ACX_CHECK_COMPILER_FLAG(W, [CFLAGS="-W $CFLAGS"])
ACX_CHECK_COMPILER_FLAG(Wwrite-strings, [CFLAGS="-Wwrite-strings $CFLAGS"])


AC_CHECK_HEADERS([getopt.h time.h],,, [AC_INCLUDES_DEFAULT])

# MinGW32 tests
AC_CHECK_HEADERS([winsock2.h ws2tcpip.h],,, [AC_INCLUDES_DEFAULT])
# end mingw32 tests

ACX_DETERMINE_EXT_FLAGS_UNBOUND

AC_C_INLINE
AC_CHECK_TYPE(int8_t, char)
AC_CHECK_TYPE(int16_t, short)
AC_CHECK_TYPE(int32_t, int)
AC_CHECK_TYPE(int64_t, long long)
AC_CHECK_TYPE(uint8_t, unsigned char)
AC_CHECK_TYPE(uint16_t, unsigned short)
AC_CHECK_TYPE(uint32_t, unsigned int)
AC_CHECK_TYPE(uint64_t, unsigned long long)

# my own checks
AC_CHECK_PROG(doxygen, doxygen, doxygen)

# check to see if libraries are needed for these functions.
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([inet_pton], [nsl])


AC_ARG_WITH(drill, AC_HELP_STRING([--with-drill], 
 [Also build drill.]), 
 [],[with_drill="no"])
if test x_$with_drill != x_no ; then
	AC_SUBST(DRILL,[drill])
	AC_SUBST(INSTALL_DRILL,[install-drill])
	AC_SUBST(UNINSTALL_DRILL,[uninstall-drill])
	AC_SUBST(CLEAN_DRILL,[clean-drill])
	AC_SUBST(LINT_DRILL,[lint-drill])
	if test -e $srcdir/drill/config.h -o -e drill/config.h ; then
		AC_MSG_ERROR([
A config.h was detected in the drill subdirectory.
This does not work with the --with-drill option.
Please remove the config.h from the drill subdirectory 
or do not use the --with-drill option.])
	fi
	DRILL_CONFIG=" drill/drill.1"
else
	AC_SUBST(DRILL,[""])
	AC_SUBST(INSTALL_DRILL,[""])
	AC_SUBST(UNINSTALL_DRILL,[""])
	AC_SUBST(CLEAN_DRILL,[""])
	AC_SUBST(LINT_DRILL,[""])
	DRILL_CONFIG=""
fi


AC_ARG_WITH(examples, AC_HELP_STRING([--with-examples], 
 [Also build examples.]), 
 [],[with_examples="no"])
if test x_$with_examples != x_no ; then
	AC_SUBST(EXAMPLES,[examples])
	AC_SUBST(INSTALL_EXAMPLES,[install-examples])
	AC_SUBST(UNINSTALL_EXAMPLES,[uninstall-examples])
	AC_SUBST(CLEAN_EXAMPLES,[clean-examples])
	AC_SUBST(LINT_EXAMPLES,[lint-examples])
	if test -e $srcdir/examples/config.h -o -e examples/config.h ; then
		AC_MSG_ERROR([
A config.h was detected in the examples subdirectory.
This does not work with the --with-examples option.
Please remove the config.h from the examples subdirectory 
or do not use the --with-examples option.])
	fi
	EXAMPLES_CONFIG=" examples/ldns-dane.1 examples/ldns-verify-zone.1"
else
	AC_SUBST(EXAMPLES,[""])
	AC_SUBST(INSTALL_EXAMPLES,[""])
	AC_SUBST(UNINSTALL_EXAMPLES,[""])
	AC_SUBST(CLEAN_EXAMPLES,[""])
	AC_SUBST(LINT_EXAMPLES,[""])
	EXAMPLES_CONFIG=""
fi

# add option to disable installation of ldns-config script
AC_ARG_ENABLE(ldns-config, [ --disable-ldns-config	disable installation of ldns-config (default=enabled)],
	enable_ldns_config=$enableval, enable_ldns_config=yes)
if test "x$enable_ldns_config" = xyes; then
	AC_SUBST(INSTALL_CONFIG, [install-config])
	AC_SUBST(INSTALL_CONFIG_MANPAGE, [install-config-manpage])
	AC_SUBST(UNINSTALL_CONFIG, [uninstall-config])
	AC_SUBST(UNINSTALL_CONFIG_MANPAGE, [uninstall-config-manpage])
else
	AC_SUBST(INSTALL_CONFIG, [""])
	AC_SUBST(INSTALL_CONFIG_MANPAGE, [""])
	AC_SUBST(UNINSTALL_CONFIG, [""])
	AC_SUBST(UNINSTALL_CONFIG_MANPAGE, [""])
fi

# check for python
PYTHON_X_CFLAGS=""
ldns_with_pyldns=no
ldns_with_pyldnsx=no
AC_ARG_WITH(pyldns, AC_HELP_STRING([--with-pyldns], 
 [generate python library, or --without-pyldns to disable Python support.]), 
 [],[ withval="no" ])
ldns_have_python=no
if test x_$withval != x_no; then
   sinclude(ax_python_devel.m4)
   ac_save_LIBS="$LIBS" dnl otherwise AC_PYTHON_DEVEL thrashes $LIBS

   AX_PYTHON_DEVEL([>= '2.4.0'])
   if test ! -z "$ac_python_version"; then
	ldns_have_python=yes
   fi

   # pass additional Python 3 option to SWIG
   if test `$PYTHON -c "import sys; \
       ver = sys.version.split()[[0]]; \
       print(ver >= '3')"` = "True"; then
       AC_SUBST(SWIGPY3, ["-py3 -DPY3"])
   fi

   # check for SWIG
   if test x_$ldns_have_python != x_no; then
        sinclude(ax_pkg_swig.m4)

        # check for >=SWIG-2.0.4 if Python 3.2 used
        if test `$PYTHON -c "import sys; \
            ver = sys.version.split()[[0]]; \
            print(ver >= '3.2')"` = "True"; then
            AX_PKG_SWIG(2.0.4, [], [AC_MSG_ERROR([SWIG-2.0.4 is required to build pyldns for Python 3.2 and greater.])])
        else
            AX_PKG_SWIG
        fi

	if test ! -x "$SWIG"; then
		AC_MSG_ERROR([failed to find SWIG tool, install it, or do not build pyldns])
	else
		AC_DEFINE(HAVE_SWIG,1,[Define if you have SWIG libraries and header files.])
		AC_SUBST(PYLDNS, "pyldns")
		AC_SUBST(swig, "$SWIG")
		ldns_with_pyldns=yes
	fi
   else
	AC_MSG_RESULT([*** don't have Python, skipping SWIG, no pyldns ***]) # '
   fi

   # xtra cflags for pyldns
   if test x_$ldns_have_python != x_no; then
	ACX_CHECK_COMPILER_FLAG(fno-strict-aliasing, [PYTHON_X_CFLAGS="-fno-strict-aliasing"])
	ACX_CHECK_COMPILER_FLAG(Wno-missing-field-initializers, [PYTHON_X_CFLAGS="-Wno-missing-field-initializers $PYTHON_X_CFLAGS"])
	ACX_CHECK_COMPILER_FLAG(Wno-unused-parameter, [PYTHON_X_CFLAGS="-Wno-unused-parameter $PYTHON_X_CFLAGS"])
	ACX_CHECK_COMPILER_FLAG(Wno-unused-variable, [PYTHON_X_CFLAGS="-Wno-unused-variable $PYTHON_X_CFLAGS"])
   fi
fi
AC_SUBST(PYTHON_X_CFLAGS)

# Check for pyldnsx
AC_ARG_WITH(pyldnsx, AC_HELP_STRING([--without-pyldnsx], 
  [Do not install the ldnsx python module, or --with-pyldnsx to install it.]), 
  [],[ withval="with_pyldns" ])
if test x_$withval != x_no; then
  if test x_$ldns_with_pyldns != x_no; then
    AC_SUBST(PYLDNSX, "pyldnsx")
    ldns_with_pyldnsx=yes
  else
    if test x_$withval != x_with_pyldns; then
      AC_MSG_ERROR([--with-pyldns is needed for the ldnsx python module])
    fi
  fi
fi

if test x_$ldns_with_pyldns != x_no; then
  AC_SUBST(PYLDNSINST, "install-pyldns")dnl
  AC_SUBST(PYLDNSUNINST, "uninstall-pyldns")
else
  AC_SUBST(PYLDNSINST, "")dnl
  AC_SUBST(PYLDNSUNINST, "")
fi
if test x_$ldns_with_pyldnsx != x_no; then
  AC_SUBST(PYLDNSXINST, "install-pyldnsx")dnl
  AC_SUBST(PYLDNSXUNINST, "uninstall-pyldnsx")
else
  AC_SUBST(PYLDNSXINST, "")dnl
  AC_SUBST(PYLDNSXUNINST, "")
fi

# Use libtool
ACX_LIBTOOL_C_ONLY

tmp_CPPFLAGS=$CPPFLAGS
tmp_LDFLAGS=$LDFLAGS
tmp_LIBS=$LIBS

ACX_WITH_SSL_OPTIONAL

AC_CHECK_FUNCS([EVP_sha256])

# for macosx, see if glibtool exists and use that
# BSD's need to know the version...
#AC_CHECK_PROG(glibtool, glibtool, [glibtool], )
#AC_CHECK_PROGS(libtool, [libtool15 libtool], [./libtool])

AC_ARG_ENABLE(sha2, AC_HELP_STRING([--disable-sha2], [Disable SHA256 and SHA512 RRSIG support]))
case "$enable_sha2" in
    no)
        ;;
    yes|*)
        if test "x$HAVE_SSL" != "xyes"; then
            AC_MSG_ERROR([SHA2 enabled, but no SSL support])
        fi
	AC_MSG_CHECKING(for SHA256 and SHA512)
	AC_CHECK_FUNC(SHA256_Init, [], [
		AC_MSG_ERROR([No SHA2 functions found in OpenSSL: please upgrade OpenSSL or rerun with --disable-sha2])
		])
	AC_DEFINE_UNQUOTED([USE_SHA2], [1], [Define this to enable SHA256 and SHA512 support.])
        ;;
esac

AC_ARG_ENABLE(gost, AC_HELP_STRING([--disable-gost], [Disable GOST support]))
case "$enable_gost" in
    no)
        ;;
    *)  dnl default
        if test "x$HAVE_SSL" != "xyes"; then
            AC_MSG_ERROR([GOST enabled, but no SSL support])
        fi
        AC_MSG_CHECKING(for GOST)
        AC_CHECK_FUNC(EVP_PKEY_set_type_str, [],[AC_MSG_ERROR([OpenSSL >= 1.0.0 is needed for GOST support or rerun with --disable-gost])])
        AC_CHECK_FUNC(EC_KEY_new, [], [AC_MSG_ERROR([No ECC functions found in OpenSSL: please upgrade OpenSSL or rerun with --disable-gost])])
        AC_DEFINE_UNQUOTED([USE_GOST], [1], [Define this to enable GOST support.])
        ;;
esac

AC_ARG_ENABLE(ecdsa, AC_HELP_STRING([--disable-ecdsa], [Disable ECDSA support]))
case "$enable_ecdsa" in
    no)
      ;;
    *) dnl default
      if test "x$HAVE_SSL" != "xyes"; then
        AC_MSG_ERROR([ECDSA enabled, but no SSL support])
      fi
      AC_CHECK_FUNC(ECDSA_sign, [], [AC_MSG_ERROR([OpenSSL does not support ECDSA: please upgrade OpenSSL or rerun with --disable-ecdsa])])
      AC_CHECK_FUNC(SHA384_Init, [], [AC_MSG_ERROR([OpenSSL does not support SHA384: please upgrade OpenSSL or rerun with --disable-ecdsa])])
      AC_CHECK_DECLS([NID_X9_62_prime256v1, NID_secp384r1], [], [AC_MSG_ERROR([OpenSSL does not support the ECDSA curves: please upgrade OpenSSL or rerun with --disable-ecdsa])], [AC_INCLUDES_DEFAULT
#include <openssl/evp.h>
      ])
      # we now know we have ECDSA and the required curves.
      AC_DEFINE_UNQUOTED([USE_ECDSA], [1], [Define this to enable ECDSA support.])
      ;;
esac

AC_SUBST(LIBSSL_CPPFLAGS)
AC_SUBST(LIBSSL_LDFLAGS)
AC_SUBST(LIBSSL_LIBS)
if test "x$HAVE_SSL" = "xyes"; then
AC_SUBST(LIBSSL_SSL_LIBS, ["$LIBSSL_LIBS -lssl"])
fi
CPPFLAGS=$tmp_CPPFLAGS
LDFLAGS=$tmp_LDFLAGS
LIBS=$tmp_LIBS


# add option to disable the evil rpath
ACX_ARG_RPATH

#AC_RUN_IFELSE([AC_LANG_SOURCE(
#[
#int main()
#{
#short one = 1;
#char *cp = (char*)&one;
#if ( *cp == 0 )
#return(0);
#else
#return(1);
#}
#])], [],[
#AC_DEFINE(CONFCHECK_LITTLE_ENDIAN, 1, [system appears to be little-endian])
#],[])

# should define WORDS_BIGENDIAN if the system is big-endian
AC_C_BIGENDIAN

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
#AC_HEADER_SYS_WAIT
#AC_CHECK_HEADERS([getopt.h fcntl.h stdlib.h string.h strings.h unistd.h])
# do the very minimum - we can always extend this
AC_CHECK_HEADERS([getopt.h stdarg.h openssl/ssl.h netinet/in.h time.h arpa/inet.h netdb.h],,, [AC_INCLUDES_DEFAULT])
AC_CHECK_HEADERS(sys/param.h sys/mount.h,,,
[AC_INCLUDES_DEFAULT
  [
   #if HAVE_SYS_PARAM_H
   # include <sys/param.h>
   #endif
  ]
])
AC_CHECK_HEADER(sys/socket.h,
[
include_sys_socket_h='#include <sys/socket.h>'
AC_DEFINE(HAVE_SYS_SOCKET_H, 1, [define if you have sys/socket.h])
],[
include_sys_socket_h=''
],[AC_INCLUDES_DEFAULT
  [
   #if HAVE_SYS_PARAM_H
   # include <sys/param.h>
   #endif
  ]
])
AC_SUBST(include_sys_socket_h)
AC_CHECK_HEADER(inttypes.h,
[
include_inttypes_h='#include <inttypes.h>'
AC_DEFINE(HAVE_INTTYPES_H, 1, [define if you have inttypes.h])
AC_SUBST(ldns_build_config_have_inttypes_h, 1)
],[
include_inttypes_h=''
AC_SUBST(ldns_build_config_have_inttypes_h, 0)
],[AC_INCLUDES_DEFAULT
])
AC_SUBST(include_inttypes_h)
AC_CHECK_HEADER(sys/types.h,
[
include_systypes_h='#include <sys/types.h>'
AC_DEFINE(HAVE_SYS_TYPES_H, 1, [define if you have sys/types.h])
],[
include_systypes_h=''
],[AC_INCLUDES_DEFAULT
])
AC_SUBST(include_systypes_h)
AC_CHECK_HEADER(unistd.h,
[
include_unistd_h='#include <unistd.h>'
AC_DEFINE(HAVE_UNISTD_H, 1, [define if you have unistd.h])
],[
include_unistd_h=''
],[AC_INCLUDES_DEFAULT
])
AC_SUBST(include_unistd_h)

AC_CHECK_SIZEOF(time_t,,[
AC_INCLUDES_DEFAULT
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
])

if test x_$with_examples != x_no; then
AC_CHECK_HEADERS([pcap.h],, [AC_INCLUDES_DEFAULT])
AC_CHECK_LIB(pcap, pcap_open_offline, [
	AC_DEFINE([HAVE_LIBPCAP], [1], [Define to 1 if you have the `pcap' library (-lpcap).])dnl`
	AC_SUBST([LIBPCAP_LIBS], [-lpcap])
    ], [
	AC_MSG_WARN([Can't find pcap library (needed for ldns-dpa, will not build dpa now.)])dnl'
	AC_SUBST([LIBPCAP_LIBS], [])
    ]
)
AC_CHECK_HEADERS([netinet/in_systm.h net/if.h netinet/ip.h netinet/udp.h netinet/igmp.h netinet/if_ether.h netinet/ip6.h net/ethernet.h netinet/ip_compat.h],,, [
AC_INCLUDES_DEFAULT 
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif])
fi

ACX_TYPE_SOCKLEN_T
if test "x$ac_cv_type_socklen_t" = xyes; then
  AC_SUBST(ldns_build_config_have_socklen_t, 1)
else
  AC_SUBST(ldns_build_config_have_socklen_t, 0)
fi
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_INTPTR_T
AC_CHECK_TYPE(in_addr_t, [], [AC_DEFINE([in_addr_t], [uint32_t], [in_addr_t])], [
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif])
AC_CHECK_TYPE(in_port_t, [], [AC_DEFINE([in_port_t], [uint16_t], [in_port_t])], [
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif])
ACX_CHECK_SS_FAMILY

AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_REPLACE_FUNCS(b64_pton)
AC_REPLACE_FUNCS(b64_ntop)
AC_REPLACE_FUNCS(b32_pton)
AC_REPLACE_FUNCS(b32_ntop)
AC_REPLACE_FUNCS(calloc)
AC_REPLACE_FUNCS(timegm)
AC_REPLACE_FUNCS(gmtime_r)
AC_REPLACE_FUNCS(ctime_r)
AC_REPLACE_FUNCS(localtime_r)
AC_REPLACE_FUNCS(isblank)
AC_REPLACE_FUNCS(isascii)
AC_REPLACE_FUNCS(inet_aton)
AC_REPLACE_FUNCS(inet_pton)
AC_REPLACE_FUNCS(inet_ntop)
AC_REPLACE_FUNCS(snprintf)
AC_REPLACE_FUNCS(strlcpy)
AC_REPLACE_FUNCS(memmove)
AC_FUNC_FORK
AC_CHECK_FUNCS([endprotoent endservent sleep random fcntl strtoul bzero memset])

ACX_CHECK_GETADDRINFO_WITH_INCLUDES
if test $ac_cv_func_getaddrinfo = no; then
	AC_LIBOBJ([fake-rfc2553])
fi
if test "$USE_WINSOCK" = 1; then
	AC_CHECK_TOOL(WINDRES, windres)
fi
ACX_FUNC_IOCTLSOCKET

#AC_SEARCH_LIBS(RSA_new, [crypto])

ACX_CHECK_FORMAT_ATTRIBUTE
ACX_CHECK_UNUSED_ATTRIBUTE

# check OSX deployment target which is needed
if echo $build_os | grep darwin > /dev/null; then
 	export MACOSX_DEPLOYMENT_TARGET="10.4"
fi

AC_DEFINE([SYSCONFDIR], [sysconfdir], [System configuration dir])

AC_ARG_WITH(trust-anchor, AC_HELP_STRING([--with-trust-anchor=KEYFILE], [Default location of the trust anchor file for drill and ldns-dane. [default=SYSCONFDIR/unbound/root.key]]), [
 LDNS_TRUST_ANCHOR_FILE="$withval"
],[
 if test "x$LDNS_TRUST_ANCHOR_FILE" = "x"; then
  if test "x$sysconfdir" = 'x${prefix}/etc' ; then
   if test "x$prefix" = 'xNONE' ; then
    LDNS_TRUST_ANCHOR_FILE="/etc/unbound/root.key"
   else
    LDNS_TRUST_ANCHOR_FILE="${prefix}/etc/unbound/root.key"
   fi
  else
    LDNS_TRUST_ANCHOR_FILE="${sysconfdir}/unbound/root.key"
  fi
 fi
])
AC_DEFINE_UNQUOTED([LDNS_TRUST_ANCHOR_FILE], ["$LDNS_TRUST_ANCHOR_FILE"], [Default trust anchor file])
AC_SUBST(LDNS_TRUST_ANCHOR_FILE)
AC_MSG_NOTICE([Default trust anchor: $LDNS_TRUST_ANCHOR_FILE])

AC_ARG_WITH(ca-file, AC_HELP_STRING([--with-ca-file=CAFILE], [File containing CA certificates for ldns-dane]), [
 AC_DEFINE([HAVE_DANE_CA_FILE], [1], [Is a CAFILE given at configure time])
 AC_DEFINE_UNQUOTED([LDNS_DANE_CA_FILE], ["$withval"], [Is a CAFILE given at configure time])
 AC_MSG_NOTICE([Using CAfile: $withval])
 AC_SUBST(DEFAULT_CAFILE, ["Default is $withval"])
],[
 AC_DEFINE([HAVE_DANE_CA_FILE], [0], [Is a CAFILE given at configure time])
 AC_SUBST(DEFAULT_CAFILE, [])
])

AC_ARG_WITH(ca-path, AC_HELP_STRING([--with-ca-path=CAPATH], [Directory containing CA certificate files for ldns-dane]), [
 AC_DEFINE([HAVE_DANE_CA_PATH], [1], [Is a CAPATH given at configure time])
 AC_DEFINE_UNQUOTED([LDNS_DANE_CA_PATH], ["$withval"], [Is a CAPATH given at configure time])
 AC_MSG_NOTICE([Using CApath: $withval])
 AC_SUBST(DEFAULT_CAPATH, ["Default is $withval"])
],[
 AC_DEFINE([HAVE_DANE_CA_PATH], [0], [Is a CAPATH given at configure time])
 AC_SUBST(DEFAULT_CAPATH, [])
])

AH_BOTTOM([
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

#ifndef BYTE_ORDER
#ifdef WORDS_BIGENDIAN
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif /* WORDS_BIGENDIAN */
#endif /* BYTE_ORDER */

#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
]
AHX_CONFIG_W32_FD_SET_T
)

AH_BOTTOM([
#ifdef __cplusplus
extern "C" {
#endif

#ifndef B64_PTON
int ldns_b64_ntop(uint8_t const *src, size_t srclength,
	 	  char *target, size_t targsize);
/**
 * calculates the size needed to store the result of b64_ntop
 */
/*@unused@*/
static inline size_t ldns_b64_ntop_calculate_size(size_t srcsize)
{
	return ((((srcsize + 2) / 3) * 4) + 1);
}
#endif /* !B64_PTON */
#ifndef B64_NTOP
int ldns_b64_pton(char const *src, uint8_t *target, size_t targsize);
/**
 * calculates the size needed to store the result of ldns_b64_pton
 */
/*@unused@*/
static inline size_t ldns_b64_pton_calculate_size(size_t srcsize)
{
	return (((((srcsize + 3) / 4) * 3)) + 1);
}
#endif /* !B64_NTOP */

#ifndef HAVE_SLEEP
/* use windows sleep, in millisecs, instead */
#define sleep(x) Sleep((x)*1000)
#endif

#ifndef HAVE_RANDOM
#define srandom(x) srand(x)
#define random(x) rand(x)
#endif

#ifndef HAVE_TIMEGM
#include <time.h>
time_t timegm (struct tm *tm);
#endif /* !TIMEGM */
#ifndef HAVE_GMTIME_R
struct tm *gmtime_r(const time_t *timep, struct tm *result);
#endif
#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *timep, struct tm *result);
#endif
#ifndef HAVE_ISBLANK
int isblank(int c);
#endif /* !HAVE_ISBLANK */
#ifndef HAVE_ISASCII
int isascii(int c);
#endif /* !HAVE_ISASCII */
#ifndef HAVE_SNPRINTF
#include <stdarg.h>
int snprintf (char *str, size_t count, const char *fmt, ...);
int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif /* HAVE_SNPRINTF */
#ifndef HAVE_INET_PTON
int inet_pton(int af, const char* src, void* dst);
#endif /* HAVE_INET_PTON */
#ifndef HAVE_INET_NTOP
const char *inet_ntop(int af, const void *src, char *dst, size_t size);
#endif
#ifndef HAVE_INET_ATON
int inet_aton(const char *cp, struct in_addr *addr);
#endif
#ifndef HAVE_MEMMOVE
void *memmove(void *dest, const void *src, size_t n);
#endif
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif
#ifdef __cplusplus
}
#endif
#ifndef HAVE_GETADDRINFO
#include "compat/fake-rfc2553.h"
#endif
#ifndef HAVE_STRTOUL
#define strtoul (unsigned long)strtol
#endif
])

if test "x$HAVE_SSL" = "xyes"; then
	AC_SUBST(ldns_build_config_have_ssl, 1)
else
	AC_SUBST(ldns_build_config_have_ssl, 0)
fi
if test "x$ac_cv_c_format_attribute" = "xyes"; then
  AC_SUBST(ldns_build_config_have_attr_format, 1)
else
  AC_SUBST(ldns_build_config_have_attr_format, 0)
fi
if test "x$ac_cv_c_unused_attribute" = "xyes"; then
  AC_SUBST(ldns_build_config_have_attr_unused, 1)
else
  AC_SUBST(ldns_build_config_have_attr_unused, 0)
fi

CONFIG_FILES="Makefile ldns/common.h ldns/net.h ldns/util.h packaging/libldns.pc packaging/ldns-config $DRILL_CONFIG $EXAMPLES_CONFIG"
AC_SUBST(CONFIG_FILES)
AC_CONFIG_FILES([$CONFIG_FILES])

AC_CONFIG_HEADER([ldns/config.h])
AC_OUTPUT
COPY_HEADER_FILES(ldns/, ldns/)

dnl AC_CONFIG_SUBDIRS([drill])


