dnl $Id: crypto.m4,v 1.7 2001/08/29 17:02:48 assar Exp $
dnl
dnl test for crypto libraries:
dnl - libcrypto (from openssl)
dnl - libdes (from krb4)
dnl - own-built libdes

AC_DEFUN([KRB_CRYPTO],[
crypto_lib=unknown
AC_WITH_ALL([openssl])

DIR_des=

AC_MSG_CHECKING([for crypto library])

if test "$crypto_lib" = "unknown" -a "$with_openssl" != "no"; then

  save_CPPFLAGS="$CPPFLAGS"
  save_LIBS="$LIBS"
  INCLUDE_des=
  LIB_des=
  if test "$with_openssl_include" != ""; then
    INCLUDE_des="-I${with_openssl}/include"
  fi
  if test "$with_openssl_lib" != ""; then
    LIB_des="-L${with_openssl}/lib"
  fi
  CPPFLAGS="${INCLUDE_des} ${CPPFLAGS}"
  LIB_des="${LIB_des} -lcrypto"
  LIB_des_a="$LIB_des"
  LIB_des_so="$LIB_des"
  LIB_des_appl="$LIB_des"
  LIBS="${LIBS} ${LIB_des}"
  AC_TRY_LINK([
  #include <openssl/md4.h>
  #include <openssl/md5.h>
  #include <openssl/sha.h>
  #include <openssl/des.h>
  #include <openssl/rc4.h>
  ],
  [
    MD4_CTX md4;
    MD5_CTX md5;
    SHA_CTX sha1;

    MD4_Init(&md4);
    MD5_Init(&md5);
    SHA1_Init(&sha1);

    des_cbc_encrypt(0, 0, 0, 0, 0, 0);
    RC4(0, 0, 0, 0);
  ], [
  crypto_lib=libcrypto
  AC_DEFINE([HAVE_OPENSSL], 1, [define to use openssl's libcrypto])
  AC_MSG_RESULT([libcrypto])])
  CPPFLAGS="$save_CPPFLAGS"
  LIBS="$save_LIBS"
fi

if test "$crypto_lib" = "unknown" -a "$with_krb4" != "no"; then

  save_CPPFLAGS="$CPPFLAGS"
  save_LIBS="$LIBS"
  INCLUDE_des="${INCLUDE_krb4}"
  LIB_des=
  if test "$krb4_libdir"; then
    LIB_des="-L${krb4_libdir}"
  fi
  LIB_des="${LIB_des} -ldes"
  CPPFLAGS="${CPPFLAGS} ${INCLUDE_des}"
  LIBS="${LIBS} ${LIB_des}"
  LIB_des_a="$LIB_des"
  LIB_des_so="$LIB_des"
  LIB_des_appl="$LIB_des"
  LIBS="${LIBS} ${LIB_des}"
  AC_TRY_LINK([
  #undef KRB5 /* makes md4.h et al unhappy */
  #define KRB4
  #include <md4.h>
  #include <md5.h>
  #include <sha.h>
  #include <des.h>
  #include <rc4.h>
  ],
  [
    MD4_CTX md4;
    MD5_CTX md5;
    SHA_CTX sha1;

    MD4_Init(&md4);
    MD5_Init(&md5);
    SHA1_Init(&sha1);

    des_cbc_encrypt(0, 0, 0, 0, 0, 0);
    RC4(0, 0, 0, 0);
  ], [crypto_lib=krb4; AC_MSG_RESULT([krb4's libdes])])
  CPPFLAGS="$save_CPPFLAGS"
  LIBS="$save_LIBS"
fi

if test "$crypto_lib" = "unknown"; then

  DIR_des='des'
  LIB_des='$(top_builddir)/lib/des/libdes.la'
  LIB_des_a='$(top_builddir)/lib/des/.libs/libdes.a'
  LIB_des_so='$(top_builddir)/lib/des/.libs/libdes.so'
  LIB_des_appl="-ldes"

  AC_MSG_RESULT([included libdes])

fi

AC_SUBST(DIR_des)
AC_SUBST(INCLUDE_des)
AC_SUBST(LIB_des)
AC_SUBST(LIB_des_a)
AC_SUBST(LIB_des_so)
AC_SUBST(LIB_des_appl)
])
