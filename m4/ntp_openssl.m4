dnl ######################################################################
dnl OpenSSL support shared by top-level and sntp/configure.ac
AC_DEFUN([NTP_OPENSSL], [
	
AC_SUBST([OPENSSL])
AC_SUBST([OPENSSL_INC])
AC_SUBST([OPENSSL_LIB])

AC_MSG_CHECKING([for openssl library directory])
AC_ARG_WITH(
    [openssl-libdir],
    [AS_HELP_STRING(
	[--with-openssl-libdir], 
	[+ =/something/reasonable]
    )],
    [ans=$withval],
    [
	case "$build" in
	 $host) ans=yes ;;
	 *)     ans=no ;;
	esac
    ]
)
case "$ans" in
 no) 
    ;;
 yes) # Look in:
    ans="/usr/lib /usr/lib/openssl /usr/sfw/lib /usr/local/lib /usr/local/ssl/lib /lib"
    ;;
 *) # Look where they said
    ;;
esac
case "$ans" in
 no)
    ;;
 *) # Look for libcrypto.a and libssl.a:
    for i in $ans no
    do
	case "$host" in
	 *-*-darwin*)
	    test -f $i/libcrypto.dylib -a -f $i/libssl.dylib && break
	    ;;
	 *)
	    test -f $i/libcrypto.so -a -f $i/libssl.so && break
	    test -f $i/libcrypto.a -a -f $i/libssl.a && break
	    ;;
	esac
    done
    case "$i" in
     no)
	ans=no
	OPENSSL_LIB=
	;;
     *) ans=$i
	OPENSSL_LIB=$ans
	;;
    esac
    ;;
esac
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([for openssl include directory])
AC_ARG_WITH(
    [openssl-incdir],
    [AS_HELP_STRING(
	[--with-openssl-incdir],
	[+ =/something/reasonable]
    )],
    [ans=$withval],
    [
	case "$build" in
	 $host) ans=yes ;;
	 *)     ans=no ;;
	esac
    ]
)
case "$ans" in
 no)
    ;;
 yes) # look in:
    ans="/usr/include /usr/sfw/include /usr/local/include /usr/local/ssl/include"
    ;;
 *) # Look where they said
    ;;
esac
case "$ans" in
 no)
    ;;
 *) # look for openssl/opensslconf.h:
    for i in $ans no
    do
	test -f $i/openssl/opensslconf.h && break
    done
    case "$i" in
     no)
	ans=no
	OPENSSL_INC=
	;;
     *) ans=$i
	OPENSSL_INC=$ans
	;;
    esac
    ;;
esac
AC_MSG_RESULT([$ans])

AC_MSG_CHECKING([if we will use crypto])
AC_ARG_WITH(
    [crypto],
    [AS_HELP_STRING(
	[--with-crypto],
	[+ =openssl]
    )],
    [ans=$withval],
    [ans=yes]
)
case "$ans" in
 no)
    ;;
 yes|openssl)
    if test -z "$OPENSSL_LIB" -o -z "$OPENSSL_INC"
    then
	ans=no
    else
	ans=yes
    fi
esac
ntp_openssl=$ans
AC_MSG_RESULT([$ans])

case "$ntp_openssl" in
 yes)
    # We have OpenSSL inc/lib - use them.
    case "$OPENSSL_INC" in
     /usr/include)
	;;
     *)	
	CPPFLAGS="$CPPFLAGS -I$OPENSSL_INC"
	;;
    esac
    case "$OPENSSL_LIB" in
     /usr/lib)
	;;
     *)	
	LDFLAGS="$LDFLAGS -L$OPENSSL_LIB"
	case "$need_dash_r" in
	 1)
	    LDFLAGS="$LDFLAGS -R$OPENSSL_LIB"
	esac
	;;
    esac
    AC_SUBST([LCRYPTO], [-lcrypto])
    AC_DEFINE([OPENSSL], [1], [Use OpenSSL?])
esac

#
# check for linking with -lcrypto failure, and try -lz -lcrypto.
# Helps m68k-atari-mint
#
case "$ntp_openssl" in
 yes)
    NTPO_SAVED_LIBS="$LIBS"
    LIBS="$NTPO_SAVED_LIBS $LCRYPTO"
    AC_CACHE_CHECK(
	[if linking with $LCRYPTO alone works],
	[ntp_cv_bare_lcrypto],
	[AC_LINK_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #include "openssl/err.h"
		    #include "openssl/evp.h"
		]],
		[[
		    ERR_load_crypto_strings();
		    OpenSSL_add_all_algorithms();
		]]
	    )],
	    [ntp_cv_bare_lcrypto=yes],
	    [ntp_cv_bare_lcrypto=no]
	)]
    )
    case "$ntp_cv_bare_lcrypto" in
     no)
	LIBS="$NTPO_SAVED_LIBS $LCRYPTO -lz"
	AC_CACHE_CHECK(
	    [if linking with $LCRYPTO -lz works],
	    [ntp_cv_lcrypto_lz],
	    [AC_LINK_IFELSE(
		[AC_LANG_PROGRAM(
		    [[
			#include "openssl/err.h"
			#include "openssl/evp.h"
		    ]],
		    [[
			ERR_load_crypto_strings();
			OpenSSL_add_all_algorithms();
		    ]]
		)],
		[ntp_cv_lcrypto_lz=yes],
		[ntp_cv_lcrypto_lz=no]
	    )]
	)
	case "$ntp_cv_lcrypto_lz" in
	 yes)
	     LCRYPTO="$LCRYPTO -lz"
	esac
    esac
    LIBS="$NTPO_SAVED_LIBS"
    AS_UNSET([NTPO_SAVED_LIBS])
esac

#
# Older OpenSSL headers have a number of callback prototypes inside
# other function prototypes which trigger copious warnings with gcc's
# -Wstrict-prototypes, which is included in -Wall.
#
# An example:
#
# int i2d_RSA_NET(const RSA *a, unsigned char **pp, 
#		  int (*cb)(), int sgckey);
#		  ^^^^^^^^^^^
#
# 
#
openssl_triggers_warnings=unknown
SAVED_CFLAGS="$CFLAGS"

case "$GCC$ntp_openssl" in
 yesyes)
    CFLAGS="$CFLAGS -Werror"
    AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	    [[
	    ]],
	    [[
		/* see if -Werror breaks gcc */
	    ]]
	)],
	[gcc_handles_Werror=yes],
	[gcc_handles_Werror=no]
    )
    case "$gcc_handles_Werror" in
     no)
	# if this gcc doesn't do -Werror go ahead and use
	# -Wstrict-prototypes.
	openssl_triggers_warnings=yes
	;;
     yes)
	CFLAGS="$CFLAGS -Wstrict-prototypes"
	AC_COMPILE_IFELSE(
	    [AC_LANG_PROGRAM(
		[[
		    #include "openssl/asn1_mac.h"
		    #include "openssl/bn.h"
		    #include "openssl/err.h"
		    #include "openssl/evp.h"
		    #include "openssl/pem.h"
		    #include "openssl/rand.h"
		    #include "openssl/x509v3.h"
		]],
		[[
		    /* empty body */
		]]
	    )],
	    [openssl_triggers_warnings=no],
	    [openssl_triggers_warnings=yes]
	)
    esac
    case "$openssl_triggers_warnings" in
     yes)
	CFLAGS="$SAVED_CFLAGS -Wno-strict-prototypes"
	;;
     *)
	CFLAGS="$SAVED_CFLAGS -Wstrict-prototypes"
    esac
    ;;
 yesno)
    # gcc without OpenSSL
    CFLAGS="$SAVED_CFLAGS -Wstrict-prototypes"
esac

])
dnl ======================================================================
