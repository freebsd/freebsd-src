dnl $Id: crypto.m4,v 1.11 2002/08/28 23:09:05 assar Exp $
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

openssl=no
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
    void *schedule = 0;
    MD4_CTX md4;
    MD5_CTX md5;
    SHA_CTX sha1;

    MD4_Init(&md4);
    MD5_Init(&md5);
    SHA1_Init(&sha1);

    des_cbc_encrypt(0, 0, 0, schedule, 0, 0);
    RC4(0, 0, 0, 0);
  ], [
  crypto_lib=libcrypto openssl=yes
  AC_MSG_RESULT([libcrypto])])
  CPPFLAGS="$save_CPPFLAGS"
  LIBS="$save_LIBS"
fi

if test "$crypto_lib" = "unknown" -a "$with_krb4" != "no"; then
	save_CPPFLAGS="$CPPFLAGS"
	save_LIBS="$LIBS"

	cdirs= clibs=
	for i in $LIB_krb4; do
		case "$i" in
		-L*) cdirs="$cdirs $i";;
		-l*) clibs="$clibs $i";;
		esac
	done

	ires=
	for i in $INCLUDE_krb4; do
		CFLAGS="$i $save_CFLAGS"
		AC_TRY_COMPILE([
			#undef KRB5 /* makes md4.h et al unhappy */
			#define KRB4
			#include <openssl/md4.h>
			#include <openssl/md5.h>
			#include <openssl/sha.h>
			#include <openssl/des.h>
			#include <openssl/rc4.h>
			], [
			MD4_CTX md4;
			MD5_CTX md5;
			SHA_CTX sha1;

			MD4_Init(&md4);
			MD5_Init(&md5);
			SHA1_Init(&sha1);

			des_cbc_encrypt(0, 0, 0, 0, 0, 0);
			RC4(0, 0, 0, 0);],openssl=yes ires="$i"; break)
		AC_TRY_COMPILE([
			#undef KRB5 /* makes md4.h et al unhappy */
			#define KRB4
			#include <md4.h>
			#include <md5.h>
			#include <sha.h>
			#include <des.h>
			#include <rc4.h>
			], [
			MD4_CTX md4;
			MD5_CTX md5;
			SHA_CTX sha1;

			MD4_Init(&md4);
			MD5_Init(&md5);
			SHA1_Init(&sha1);

			des_cbc_encrypt(0, 0, 0, 0, 0, 0);
			RC4(0, 0, 0, 0);],ires="$i"; break)
	done
	lres=
	for i in $cdirs; do
		for j in $clibs; do
			LIBS="$i $j $save_LIBS"
			if test "$openssl" = yes; then
			AC_TRY_LINK([
				#undef KRB5 /* makes md4.h et al unhappy */
				#define KRB4
				#include <openssl/md4.h>
				#include <openssl/md5.h>
				#include <openssl/sha.h>
				#include <openssl/des.h>
				#include <openssl/rc4.h>
				], [
				MD4_CTX md4;
				MD5_CTX md5;
				SHA_CTX sha1;
	
				MD4_Init(&md4);
				MD5_Init(&md5);
				SHA1_Init(&sha1);
	
				des_cbc_encrypt(0, 0, 0, 0, 0, 0);
				RC4(0, 0, 0, 0);],lres="$i $j"; break 2)
			else
			AC_TRY_LINK([
				#undef KRB5 /* makes md4.h et al unhappy */
				#define KRB4
				#include <md4.h>
				#include <md5.h>
				#include <sha.h>
				#include <des.h>
				#include <rc4.h>
				], [
				MD4_CTX md4;
				MD5_CTX md5;
				SHA_CTX sha1;
	
				MD4_Init(&md4);
				MD5_Init(&md5);
				SHA1_Init(&sha1);
	
				des_cbc_encrypt(0, 0, 0, 0, 0, 0);
				RC4(0, 0, 0, 0);],lres="$i $j"; break 2)
			fi
		done
	done
	CFLAGS="$save_CFLAGS"
	LIBS="$save_LIBS"
	if test "$ires" -a "$lres"; then
		INCLUDE_des="$ires"
		LIB_des="$lres"
		crypto_lib=krb4
		AC_MSG_RESULT([same as krb4])
		LIB_des_a='$(LIB_des)'
		LIB_des_so='$(LIB_des)'
		LIB_des_appl='$(LIB_des)'
	fi
fi

if test "$crypto_lib" = "unknown"; then

  DIR_des='des'
  LIB_des='$(top_builddir)/lib/des/libdes.la'
  LIB_des_a='$(top_builddir)/lib/des/.libs/libdes.a'
  LIB_des_so='$(top_builddir)/lib/des/.libs/libdes.so'
  LIB_des_appl="-ldes"

  AC_MSG_RESULT([included libdes])

fi

if test "$openssl" = "yes"; then
  AC_DEFINE([HAVE_OPENSSL], 1, [define to use openssl's libcrypto])
fi
AM_CONDITIONAL(HAVE_OPENSSL, test "$openssl" = yes)dnl

AC_SUBST(DIR_des)
AC_SUBST(INCLUDE_des)
AC_SUBST(LIB_des)
AC_SUBST(LIB_des_a)
AC_SUBST(LIB_des_so)
AC_SUBST(LIB_des_appl)
])
