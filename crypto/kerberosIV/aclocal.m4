dnl aclocal.m4 generated automatically by aclocal 1.4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl $Id: acinclude.m4,v 1.2 1999/03/01 13:06:21 joda Exp $
dnl
dnl Only put things that for some reason can't live in the `cf'
dnl directory in this file.
dnl

dnl $xId: misc.m4,v 1.1 1997/12/14 15:59:04 joda Exp $
dnl
define(upcase,`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`)dnl

dnl $Id: krb-prog-ln-s.m4,v 1.1 1997/12/14 15:59:01 joda Exp $
dnl
dnl
dnl Better test for ln -s, ln or cp
dnl

AC_DEFUN(AC_KRB_PROG_LN_S,
[AC_MSG_CHECKING(for ln -s or something else)
AC_CACHE_VAL(ac_cv_prog_LN_S,
[rm -f conftestdata
if ln -s X conftestdata 2>/dev/null
then
  rm -f conftestdata
  ac_cv_prog_LN_S="ln -s"
else
  touch conftestdata1
  if ln conftestdata1 conftestdata2; then
    rm -f conftestdata*
    ac_cv_prog_LN_S=ln
  else
    ac_cv_prog_LN_S=cp
  fi
fi])dnl
LN_S="$ac_cv_prog_LN_S"
AC_MSG_RESULT($ac_cv_prog_LN_S)
AC_SUBST(LN_S)dnl
])


dnl $Id: krb-prog-yacc.m4,v 1.1 1997/12/14 15:59:02 joda Exp $
dnl
dnl
dnl We prefer byacc or yacc because they do not use `alloca'
dnl

AC_DEFUN(AC_KRB_PROG_YACC,
[AC_CHECK_PROGS(YACC, byacc yacc 'bison -y')])

dnl $Id: test-package.m4,v 1.7 1999/04/19 13:33:05 assar Exp $
dnl
dnl AC_TEST_PACKAGE_NEW(package,headers,libraries,extra libs,default locations)

AC_DEFUN(AC_TEST_PACKAGE,[AC_TEST_PACKAGE_NEW($1,[#include <$2>],$4,,$5)])

AC_DEFUN(AC_TEST_PACKAGE_NEW,[
AC_ARG_WITH($1,
[  --with-$1=dir                use $1 in dir])
AC_ARG_WITH($1-lib,
[  --with-$1-lib=dir            use $1 libraries in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])
AC_ARG_WITH($1-include,
[  --with-$1-include=dir        use $1 headers in dir],
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

AC_MSG_CHECKING(for $1)

case "$with_$1" in
yes)	;;
no)	;;
"")	;;
*)	if test "$with_$1_include" = ""; then
		with_$1_include="$with_$1/include"
	fi
	if test "$with_$1_lib" = ""; then
		with_$1_lib="$with_$1/lib$abilibdirext"
	fi
	;;
esac
header_dirs=
lib_dirs=
d='$5'
for i in $d; do
	header_dirs="$header_dirs $i/include"
	lib_dirs="$lib_dirs $i/lib$abilibdirext"
done

case "$with_$1_include" in
yes) ;;
no)  ;;
*)   header_dirs="$with_$1_include $header_dirs";;
esac
case "$with_$1_lib" in
yes) ;;
no)  ;;
*)   lib_dirs="$with_$1_lib $lib_dirs";;
esac

save_CFLAGS="$CFLAGS"
save_LIBS="$LIBS"
ires= lres=
for i in $header_dirs; do
	CFLAGS="-I$i $save_CFLAGS"
	AC_TRY_COMPILE([$2],,ires=$i;break)
done
for i in $lib_dirs; do
	LIBS="-L$i $3 $4 $save_LIBS"
	AC_TRY_LINK([$2],,lres=$i;break)
done
CFLAGS="$save_CFLAGS"
LIBS="$save_LIBS"

if test "$ires" -a "$lres" -a "$with_$1" != "no"; then
	$1_includedir="$ires"
	$1_libdir="$lres"
	INCLUDE_$1="-I$$1_includedir"
	LIB_$1="-L$$1_libdir $3"
	AC_DEFINE_UNQUOTED(upcase($1),1,[Define if you have the $1 package.])
	with_$1=yes
	AC_MSG_RESULT([headers $ires, libraries $lres])
else
	INCLUDE_$1=
	LIB_$1=
	with_$1=no
	AC_MSG_RESULT($with_$1)
fi
AC_SUBST(INCLUDE_$1)
AC_SUBST(LIB_$1)
])

dnl $Id: osfc2.m4,v 1.2 1999/03/27 17:28:16 joda Exp $
dnl
dnl enable OSF C2 stuff

AC_DEFUN(AC_CHECK_OSFC2,[
AC_ARG_ENABLE(osfc2,
[  --enable-osfc2          enable some OSF C2 support])
LIB_security=
if test "$enable_osfc2" = yes; then
	AC_DEFINE(HAVE_OSFC2, 1, [Define to enable basic OSF C2 support.])
	LIB_security=-lsecurity
fi
AC_SUBST(LIB_security)
])

dnl $Id: mips-abi.m4,v 1.4 1998/05/16 20:44:15 joda Exp $
dnl
dnl
dnl Check for MIPS/IRIX ABI flags. Sets $abi and $abilibdirext to some
dnl value.

AC_DEFUN(AC_MIPS_ABI, [
AC_ARG_WITH(mips_abi,
[  --with-mips-abi=abi     ABI to use for IRIX (32, n32, or 64)])

case "$host_os" in
irix*)
with_mips_abi="${with_mips_abi:-yes}"
if test -n "$GCC"; then

# GCC < 2.8 only supports the O32 ABI. GCC >= 2.8 has a flag to select
# which ABI to use, but only supports (as of 2.8.1) the N32 and 64 ABIs.
#
# Default to N32, but if GCC doesn't grok -mabi=n32, we assume an old
# GCC and revert back to O32. The same goes if O32 is asked for - old
# GCCs doesn't like the -mabi option, and new GCCs can't output O32.
#
# Don't you just love *all* the different SGI ABIs?

case "${with_mips_abi}" in 
        32|o32) abi='-mabi=32';  abilibdirext=''     ;;
       n32|yes) abi='-mabi=n32'; abilibdirext='32'  ;;
        64) abi='-mabi=64';  abilibdirext='64'   ;;
	no) abi=''; abilibdirext='';;
         *) AC_ERROR("Invalid ABI specified") ;;
esac
if test -n "$abi" ; then
ac_foo=krb_cv_gcc_`echo $abi | tr =- __`
dnl
dnl can't use AC_CACHE_CHECK here, since it doesn't quote CACHE-ID to
dnl AC_MSG_RESULT
dnl
AC_MSG_CHECKING([if $CC supports the $abi option])
AC_CACHE_VAL($ac_foo, [
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $abi"
AC_TRY_COMPILE(,int x;, eval $ac_foo=yes, eval $ac_foo=no)
CFLAGS="$save_CFLAGS"
])
ac_res=`eval echo \\\$$ac_foo`
AC_MSG_RESULT($ac_res)
if test $ac_res = no; then
# Try to figure out why that failed...
case $abi in
	-mabi=32) 
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS -mabi=n32"
	AC_TRY_COMPILE(,int x;, ac_res=yes, ac_res=no)
	CLAGS="$save_CFLAGS"
	if test $ac_res = yes; then
		# New GCC
		AC_ERROR([$CC does not support the $with_mips_abi ABI])
	fi
	# Old GCC
	abi=''
	abilibdirext=''
	;;
	-mabi=n32|-mabi=64)
		if test $with_mips_abi = yes; then
			# Old GCC, default to O32
			abi=''
			abilibdirext=''
		else
			# Some broken GCC
			AC_ERROR([$CC does not support the $with_mips_abi ABI])
		fi
	;;
esac
fi #if test $ac_res = no; then
fi #if test -n "$abi" ; then
else
case "${with_mips_abi}" in
        32|o32) abi='-32'; abilibdirext=''     ;;
       n32|yes) abi='-n32'; abilibdirext='32'  ;;
        64) abi='-64'; abilibdirext='64'   ;;
	no) abi=''; abilibdirext='';;
         *) AC_ERROR("Invalid ABI specified") ;;
esac
fi #if test -n "$GCC"; then
;;
esac
])

dnl
dnl $Id: shared-libs.m4,v 1.4.14.3 2000/12/07 18:03:00 bg Exp $
dnl
dnl Shared library stuff has to be different everywhere
dnl

AC_DEFUN(AC_SHARED_LIBS, [

dnl Check if we want to use shared libraries
AC_ARG_ENABLE(shared,
[  --enable-shared         create shared libraries for Kerberos])

AC_SUBST(CFLAGS)dnl
AC_SUBST(LDFLAGS)dnl

case ${enable_shared} in
  yes ) enable_shared=yes;;
  no  ) enable_shared=no;;
  *   ) enable_shared=no;;
esac

# NOTE: Building shared libraries may not work if you do not use gcc!
#
# OS		$SHLIBEXT
# HP-UX		sl
# Linux		so
# NetBSD	so
# FreeBSD	so
# OSF		so
# SunOS5	so
# SunOS4	so.0.5
# Irix		so
#
# LIBEXT is the extension we should build (.a or $SHLIBEXT)
LINK='$(CC)'
AC_SUBST(LINK)
lib_deps=yes
REAL_PICFLAGS="-fpic"
LDSHARED='$(CC) $(PICFLAGS) -shared'
LIBPREFIX=lib
build_symlink_command=@true
install_symlink_command=@true
install_symlink_command2=@true
REAL_SHLIBEXT=so
changequote({,})dnl
SHLIB_VERSION=`echo $VERSION | sed 's/\([0-9.]*\).*/\1/'`
SHLIB_SONAME=`echo $VERSION | sed 's/\([0-9]*\).*/\1/'`
changequote([,])dnl
case "${host}" in
*-*-hpux*)
	REAL_SHLIBEXT=sl
	REAL_LD_FLAGS='-Wl,+b$(libdir)'
	if test -z "$GCC"; then
		LDSHARED="ld -b"
		REAL_PICFLAGS="+z"
	fi
	lib_deps=no
	;;
*-*-linux*)
	LDSHARED='$(CC) -shared -Wl,-soname,$(LIBNAME).so.'"${SHLIB_SONAME}"
	REAL_LD_FLAGS='-Wl,-rpath,$(libdir)'
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	build_symlink_command='$(LN_S) -f [$][@] $(LIBNAME).so'
	install_symlink_command='$(LN_S) -f $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so.'"${SHLIB_SONAME}"';$(LN_S) -f $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so'
	install_symlink_command2='$(LN_S) -f $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so.'"${SHLIB_SONAME}"';$(LN_S) -f $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so'
	;;
changequote(,)dnl
*-*-freebsd[345]* | *-*-freebsdelf[345]*)
changequote([,])dnl
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	REAL_LD_FLAGS='-Wl,-R$(libdir)'
	build_symlink_command='$(LN_S) -f [$][@] $(LIBNAME).so'
	install_symlink_command='$(LN_S) -f $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so'
	install_symlink_command2='$(LN_S) -f $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so'
	;;
*-*-*bsd*)
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	LDSHARED='ld -Bshareable'
	REAL_LD_FLAGS='-Wl,-R$(libdir)'
	;;
*-*-osf*)
	REAL_LD_FLAGS='-Wl,-rpath,$(libdir)'
	REAL_PICFLAGS=
	LDSHARED='ld -shared -expect_unresolved \*'
	;;
*-*-solaris2*)
	LDSHARED='$(CC) -shared -Wl,-h$(LIBNAME).so.'"${SHLIB_SONAME}"
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	build_symlink_command='$(LN_S) [$][@] $(LIBNAME).so'
	install_symlink_command='$(LN_S) $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so.'"${SHLIB_SONAME}"';$(LN_S) $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so'
	install_symlink_command2='$(LN_S) $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so.'"${SHLIB_SONAME}"';$(LN_S) $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so'
	REAL_LD_FLAGS='-Wl,-R$(libdir)'
	if test -z "$GCC"; then
		LDSHARED='$(CC) -G -h$(LIBNAME).so.'"${SHLIB_SONAME}"
		REAL_PICFLAGS="-Kpic"
	fi
	;;
*-fujitsu-uxpv*)
	REAL_LD_FLAGS='' # really: LD_RUN_PATH=$(libdir) cc -o ...
	REAL_LINK='LD_RUN_PATH=$(libdir) $(CC)'
	LDSHARED='$(CC) -G'
	REAL_PICFLAGS="-Kpic"
	lib_deps=no # fails in mysterious ways
	;;
*-*-sunos*)
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	REAL_LD_FLAGS='-Wl,-L$(libdir)'
	lib_deps=no
	;;
*-*-irix*)
        libdir="${libdir}${abilibdirext}"
        REAL_LD_FLAGS="${abi} -Wl,-rpath,\$(libdir)"
        LD_FLAGS="${abi} -Wl,-rpath,\$(libdir)"
	LDSHARED="\$(CC) -shared ${abi}"
        REAL_PICFLAGS=
        CFLAGS="${abi} ${CFLAGS}"
	;;
*-*-os2*)
	LIBPREFIX=
	EXECSUFFIX='.exe'
	RANLIB=EMXOMF
	LD_FLAGS=-Zcrtdll
	REAL_SHLIBEXT=nobuild
	;;
*-*-cygwin32*)
	EXECSUFFIX='.exe'
	REAL_SHLIBEXT=nobuild
	;;
*)	REAL_SHLIBEXT=nobuild
	REAL_PICFLAGS= 
	;;
esac

if test "${enable_shared}" != "yes" ; then 
 PICFLAGS=""
 SHLIBEXT="nobuild"
 LIBEXT="a"
 build_symlink_command=@true
 install_symlink_command=@true
 install_symlink_command2=@true
else
 PICFLAGS="$REAL_PICFLAGS"
 SHLIBEXT="$REAL_SHLIBEXT"
 LIBEXT="$SHLIBEXT"
 AC_MSG_CHECKING(whether to use -rpath)
 case "$libdir" in
   /lib | /usr/lib | /usr/local/lib)
     AC_MSG_RESULT(no)
     REAL_LD_FLAGS=
     LD_FLAGS=
     ;;
   *)
     LD_FLAGS="$REAL_LD_FLAGS"
     test "$REAL_LINK" && LINK="$REAL_LINK"
     AC_MSG_RESULT($LD_FLAGS)
     ;;
   esac
fi

if test "$lib_deps" = yes; then
	lib_deps_yes=""
	lib_deps_no="# "
else
	lib_deps_yes="# "
	lib_deps_no=""
fi
AC_SUBST(lib_deps_yes)
AC_SUBST(lib_deps_no)

# use supplied ld-flags, or none if `no'
if test "$with_ld_flags" = no; then
	LD_FLAGS=
elif test -n "$with_ld_flags"; then
	LD_FLAGS="$with_ld_flags"
fi

AC_SUBST(REAL_PICFLAGS) dnl
AC_SUBST(REAL_SHLIBEXT) dnl
AC_SUBST(REAL_LD_FLAGS) dnl

AC_SUBST(PICFLAGS) dnl
AC_SUBST(SHLIBEXT) dnl
AC_SUBST(LDSHARED) dnl
AC_SUBST(LD_FLAGS) dnl
AC_SUBST(LIBEXT) dnl
AC_SUBST(LIBPREFIX) dnl
AC_SUBST(EXECSUFFIX) dnl

AC_SUBST(build_symlink_command)dnl
AC_SUBST(install_symlink_command)dnl
AC_SUBST(install_symlink_command2)dnl
])

dnl
dnl $Id: c-attribute.m4,v 1.2 1999/03/01 09:52:23 joda Exp $
dnl

dnl
dnl Test for __attribute__
dnl

AC_DEFUN(AC_C___ATTRIBUTE__, [
AC_MSG_CHECKING(for __attribute__)
AC_CACHE_VAL(ac_cv___attribute__, [
AC_TRY_COMPILE([
#include <stdlib.h>
],
[
static void foo(void) __attribute__ ((noreturn));

static void
foo(void)
{
  exit(1);
}
],
ac_cv___attribute__=yes,
ac_cv___attribute__=no)])
if test "$ac_cv___attribute__" = "yes"; then
  AC_DEFINE(HAVE___ATTRIBUTE__, 1, [define if your compiler has __attribute__])
fi
AC_MSG_RESULT($ac_cv___attribute__)
])


dnl $Id: krb-sys-nextstep.m4,v 1.2 1998/06/03 23:48:40 joda Exp $
dnl
dnl
dnl NEXTSTEP is not posix compliant by default,
dnl you need a switch -posix to the compiler
dnl

AC_DEFUN(AC_KRB_SYS_NEXTSTEP, [
AC_MSG_CHECKING(for NEXTSTEP)
AC_CACHE_VAL(krb_cv_sys_nextstep,
AC_EGREP_CPP(yes, 
[#if defined(NeXT) && !defined(__APPLE__)
	yes
#endif 
], krb_cv_sys_nextstep=yes, krb_cv_sys_nextstep=no) )
if test "$krb_cv_sys_nextstep" = "yes"; then
  CFLAGS="$CFLAGS -posix"
  LIBS="$LIBS -posix"
fi
AC_MSG_RESULT($krb_cv_sys_nextstep)
])

dnl $Id: krb-sys-aix.m4,v 1.1 1997/12/14 15:59:02 joda Exp $
dnl
dnl
dnl AIX have a very different syscall convention
dnl
AC_DEFUN(AC_KRB_SYS_AIX, [
AC_MSG_CHECKING(for AIX)
AC_CACHE_VAL(krb_cv_sys_aix,
AC_EGREP_CPP(yes, 
[#ifdef _AIX
	yes
#endif 
], krb_cv_sys_aix=yes, krb_cv_sys_aix=no) )
AC_MSG_RESULT($krb_cv_sys_aix)
])

dnl $Id: find-func-no-libs.m4,v 1.5 1999/10/30 21:08:18 assar Exp $
dnl
dnl
dnl Look for function in any of the specified libraries
dnl

dnl AC_FIND_FUNC_NO_LIBS(func, libraries, includes, arguments, extra libs, extra args)
AC_DEFUN(AC_FIND_FUNC_NO_LIBS, [
AC_FIND_FUNC_NO_LIBS2([$1], ["" $2], [$3], [$4], [$5], [$6])])

dnl $Id: find-func-no-libs2.m4,v 1.3 1999/10/30 21:09:53 assar Exp $
dnl
dnl
dnl Look for function in any of the specified libraries
dnl

dnl AC_FIND_FUNC_NO_LIBS2(func, libraries, includes, arguments, extra libs, extra args)
AC_DEFUN(AC_FIND_FUNC_NO_LIBS2, [

AC_MSG_CHECKING([for $1])
AC_CACHE_VAL(ac_cv_funclib_$1,
[
if eval "test \"\$ac_cv_func_$1\" != yes" ; then
	ac_save_LIBS="$LIBS"
	for ac_lib in $2; do
		if test -n "$ac_lib"; then 
			ac_lib="-l$ac_lib"
		else
			ac_lib=""
		fi
		LIBS="$6 $ac_lib $5 $ac_save_LIBS"
		AC_TRY_LINK([$3],[$1($4)],eval "if test -n \"$ac_lib\";then ac_cv_funclib_$1=$ac_lib; else ac_cv_funclib_$1=yes; fi";break)
	done
	eval "ac_cv_funclib_$1=\${ac_cv_funclib_$1-no}"
	LIBS="$ac_save_LIBS"
fi
])

eval "ac_res=\$ac_cv_funclib_$1"

dnl autoheader tricks *sigh*
: << END
@@@funcs="$funcs $1"@@@
@@@libs="$libs $2"@@@
END

# $1
eval "ac_tr_func=HAVE_[]upcase($1)"
eval "ac_tr_lib=HAVE_LIB[]upcase($ac_res | sed -e 's/-l//')"
eval "LIB_$1=$ac_res"

case "$ac_res" in
	yes)
	eval "ac_cv_func_$1=yes"
	eval "LIB_$1="
	AC_DEFINE_UNQUOTED($ac_tr_func)
	AC_MSG_RESULT([yes])
	;;
	no)
	eval "ac_cv_func_$1=no"
	eval "LIB_$1="
	AC_MSG_RESULT([no])
	;;
	*)
	eval "ac_cv_func_$1=yes"
	eval "ac_cv_lib_`echo "$ac_res" | sed 's/-l//'`=yes"
	AC_DEFINE_UNQUOTED($ac_tr_func)
	AC_DEFINE_UNQUOTED($ac_tr_lib)
	AC_MSG_RESULT([yes, in $ac_res])
	;;
esac
AC_SUBST(LIB_$1)
])

dnl
dnl $Id: check-netinet-ip-and-tcp.m4,v 1.2 1999/05/14 13:15:40 assar Exp $
dnl

dnl extra magic check for netinet/{ip.h,tcp.h} because on irix 6.5.3
dnl you have to include standards.h before including these files

AC_DEFUN(CHECK_NETINET_IP_AND_TCP,
[
AC_CHECK_HEADERS(standards.h)
for i in netinet/ip.h netinet/tcp.h; do

cv=`echo "$i" | sed 'y%./+-%__p_%'`

AC_MSG_CHECKING([for $i])
AC_CACHE_VAL([ac_cv_header_$cv],
[AC_TRY_CPP([\
#ifdef HAVE_STANDARDS_H
#include <standards.h>
#endif
#include <$i>
],
eval "ac_cv_header_$cv=yes",
eval "ac_cv_header_$cv=no")])
AC_MSG_RESULT(`eval echo \\$ac_cv_header_$cv`)
changequote(, )dnl
if test `eval echo \\$ac_cv_header_$cv` = yes; then
  ac_tr_hdr=HAVE_`echo $i | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
changequote([, ])dnl
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
done
dnl autoheader tricks *sigh*
: << END
@@@headers="$headers netinet/ip.h netinet/tcp.h"@@@
END

])

dnl $Id: grok-type.m4,v 1.4 1999/11/29 11:16:48 joda Exp $
dnl
AC_DEFUN(AC_GROK_TYPE, [
AC_CACHE_VAL(ac_cv_type_$1, 
AC_TRY_COMPILE([
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
],
$i x;
,
eval ac_cv_type_$1=yes,
eval ac_cv_type_$1=no))])

AC_DEFUN(AC_GROK_TYPES, [
for i in $1; do
	AC_MSG_CHECKING(for $i)
	AC_GROK_TYPE($i)
	eval ac_res=\$ac_cv_type_$i
	if test "$ac_res" = yes; then
		type=HAVE_[]upcase($i)
		AC_DEFINE_UNQUOTED($type)
	fi
	AC_MSG_RESULT($ac_res)
done
])

dnl $Id: find-func.m4,v 1.1 1997/12/14 15:58:58 joda Exp $
dnl
dnl AC_FIND_FUNC(func, libraries, includes, arguments)
AC_DEFUN(AC_FIND_FUNC, [
AC_FIND_FUNC_NO_LIBS([$1], [$2], [$3], [$4])
if test -n "$LIB_$1"; then
	LIBS="$LIB_$1 $LIBS"
fi
])

dnl 
dnl See if there is any X11 present
dnl
dnl $Id: check-x.m4,v 1.2 1999/11/05 04:25:23 assar Exp $

AC_DEFUN(KRB_CHECK_X,[
AC_PATH_XTRA

# try to figure out if we need any additional ld flags, like -R
# and yes, the autoconf X test is utterly broken
if test "$no_x" != yes; then
	AC_CACHE_CHECK(for special X linker flags,krb_cv_sys_x_libs_rpath,[
	ac_save_libs="$LIBS"
	ac_save_cflags="$CFLAGS"
	CFLAGS="$CFLAGS $X_CFLAGS"
	krb_cv_sys_x_libs_rpath=""
	krb_cv_sys_x_libs=""
	for rflag in "" "-R" "-R " "-rpath "; do
		if test "$rflag" = ""; then
			foo="$X_LIBS"
		else
			foo=""
			for flag in $X_LIBS; do
			case $flag in
			-L*)
				foo="$foo $flag `echo $flag | sed \"s/-L/$rflag/\"`"
				;;
			*)
				foo="$foo $flag"
				;;
			esac
			done
		fi
		LIBS="$ac_save_libs $foo $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
		AC_TRY_RUN([
		#include <X11/Xlib.h>
		foo()
		{
		XOpenDisplay(NULL);
		}
		main()
		{
		return 0;
		}
		], krb_cv_sys_x_libs_rpath="$rflag"; krb_cv_sys_x_libs="$foo"; break,:)
	done
	LIBS="$ac_save_libs"
	CFLAGS="$ac_save_cflags"
	])
	X_LIBS="$krb_cv_sys_x_libs"
fi
])

dnl $Id: check-xau.m4,v 1.3 1999/05/14 01:17:06 assar Exp $
dnl
dnl check for Xau{Read,Write}Auth and XauFileName
dnl
AC_DEFUN(AC_CHECK_XAU,[
save_CFLAGS="$CFLAGS"
CFLAGS="$X_CFLAGS $CFLAGS"
save_LIBS="$LIBS"
dnl LIBS="$X_LIBS $X_PRE_LIBS $X_EXTRA_LIBS $LIBS"
LIBS="$X_PRE_LIBS $X_EXTRA_LIBS $LIBS"
save_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $X_LIBS"


AC_FIND_FUNC_NO_LIBS(XauWriteAuth, X11 Xau)
ac_xxx="$LIBS"
LIBS="$LIB_XauWriteAuth $LIBS"
AC_FIND_FUNC_NO_LIBS(XauReadAuth, X11 Xau)
LIBS="$LIB_XauReadAauth $LIBS"
AC_FIND_FUNC_NO_LIBS(XauFileName, X11 Xau)
LIBS="$ac_xxx"

case "$ac_cv_funclib_XauWriteAuth" in
yes)	;;
no)	;;
*)	if test "$ac_cv_funclib_XauReadAuth" = yes; then
		if test "$ac_cv_funclib_XauFileName" = yes; then
			LIB_XauReadAuth="$LIB_XauWriteAuth"
		else
			LIB_XauReadAuth="$LIB_XauWriteAuth $LIB_XauFileName"
		fi
	else
		if test "$ac_cv_funclib_XauFileName" = yes; then
			LIB_XauReadAuth="$LIB_XauReadAuth $LIB_XauWriteAuth"
		else
			LIB_XauReadAuth="$LIB_XauReadAuth $LIB_XauWriteAuth $LIB_XauFileName"
		fi
	fi
	;;
esac

if test "$AUTOMAKE" != ""; then
	AM_CONDITIONAL(NEED_WRITEAUTH, test "$ac_cv_func_XauWriteAuth" != "yes")
else
	AC_SUBST(NEED_WRITEAUTH_TRUE)
	AC_SUBST(NEED_WRITEAUTH_FALSE)
	if test "$ac_cv_func_XauWriteAuth" != "yes"; then
		NEED_WRITEAUTH_TRUE=
		NEED_WRITEAUTH_FALSE='#'
	else
		NEED_WRITEAUTH_TRUE='#'
		NEED_WRITEAUTH_FALSE=
	fi
fi
CFLAGS=$save_CFLAGS
LIBS=$save_LIBS
LDFLAGS=$save_LDFLAGS
])

# Define a conditional.

AC_DEFUN(AM_CONDITIONAL,
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])

dnl $Id: krb-find-db.m4,v 1.5.16.1 2000/08/16 04:11:57 assar Exp $
dnl
dnl find a suitable database library
dnl
dnl AC_FIND_DB(libraries)
AC_DEFUN(KRB_FIND_DB, [

lib_dbm=no
lib_db=no

for i in $1; do

	if test "$i"; then
		m="lib$i"
		l="-l$i"
	else
		m="libc"
		l=""
	fi	

	AC_MSG_CHECKING(for dbm_open in $m)
	AC_CACHE_VAL(ac_cv_krb_dbm_open_$m, [

	save_LIBS="$LIBS"
	LIBS="$l $LIBS"
	AC_TRY_RUN([
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_NDBM_H)
#include <ndbm.h>
#elif defined(HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined(HAVE_DBM_H)
#include <dbm.h>
#elif defined(HAVE_RPCSVC_DBM_H)
#include <rpcsvc/dbm.h>
#elif defined(HAVE_DB_H)
#define DB_DBM_HSEARCH 1
#include <db.h>
#endif
int main()
{
  DBM *d;

  d = dbm_open("conftest", O_RDWR | O_CREAT, 0666);
  if(d == NULL)
    return 1;
  dbm_close(d);
  return 0;
}], [
	if test -f conftest.db; then
		ac_res=db
	else
		ac_res=dbm
	fi], ac_res=no, ac_res=no)

	LIBS="$save_LIBS"

	eval ac_cv_krb_dbm_open_$m=$ac_res])
	eval ac_res=\$ac_cv_krb_dbm_open_$m
	AC_MSG_RESULT($ac_res)

	if test "$lib_dbm" = no -a $ac_res = dbm; then
		lib_dbm="$l"
	elif test "$lib_db" = no -a $ac_res = db; then
		lib_db="$l"
		break
	fi
done

AC_MSG_CHECKING(for NDBM library)
ac_ndbm=no
if test "$lib_db" != no; then
	LIB_DBM="$lib_db"
	ac_ndbm=yes
	AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files ending in .db).])
	if test "$LIB_DBM"; then
		ac_res="yes, $LIB_DBM"
	else
		ac_res=yes
	fi
elif test "$lib_dbm" != no; then
	LIB_DBM="$lib_dbm"
	ac_ndbm=yes
	if test "$LIB_DBM"; then
		ac_res="yes, $LIB_DBM"
	else
		ac_res=yes
	fi
else
	LIB_DBM=""
	ac_res=no
fi
test "$ac_ndbm" = yes && AC_DEFINE(NDBM, 1, [Define if you have NDBM (and not DBM)])dnl
AC_SUBST(LIB_DBM)
DBLIB="$LIB_DBM"
AC_SUBST(DBLIB)
AC_MSG_RESULT($ac_res)

])

dnl $Id: broken-snprintf.m4,v 1.3 1999/03/01 09:52:22 joda Exp $
dnl
AC_DEFUN(AC_BROKEN_SNPRINTF, [
AC_CACHE_CHECK(for working snprintf,ac_cv_func_snprintf_working,
ac_cv_func_snprintf_working=yes
AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
int main()
{
changequote(`,')dnl
	char foo[3];
changequote([,])dnl
	snprintf(foo, 2, "12");
	return strcmp(foo, "1");
}],:,ac_cv_func_snprintf_working=no,:))

if test "$ac_cv_func_snprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_SNPRINTF, 1, [define if you have a working snprintf])
fi
if test "$ac_cv_func_snprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],snprintf)
fi
])

AC_DEFUN(AC_BROKEN_VSNPRINTF,[
AC_CACHE_CHECK(for working vsnprintf,ac_cv_func_vsnprintf_working,
ac_cv_func_vsnprintf_working=yes
AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int foo(int num, ...)
{
changequote(`,')dnl
	char bar[3];
changequote([,])dnl
	va_list arg;
	va_start(arg, num);
	vsnprintf(bar, 2, "%s", arg);
	va_end(arg);
	return strcmp(bar, "1");
}


int main()
{
	return foo(0, "12");
}],:,ac_cv_func_vsnprintf_working=no,:))

if test "$ac_cv_func_vsnprintf_working" = yes; then
	AC_DEFINE_UNQUOTED(HAVE_VSNPRINTF, 1, [define if you have a working vsnprintf])
fi
if test "$ac_cv_func_vsnprintf_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>],vsnprintf)
fi
])

dnl $Id: need-proto.m4,v 1.2 1999/03/01 09:52:24 joda Exp $
dnl
dnl
dnl Check if we need the prototype for a function
dnl

dnl AC_NEED_PROTO(includes, function)

AC_DEFUN(AC_NEED_PROTO, [
if test "$ac_cv_func_$2+set" != set -o "$ac_cv_func_$2" = yes; then
AC_CACHE_CHECK([if $2 needs a prototype], ac_cv_func_$2_noproto,
AC_TRY_COMPILE([$1],
[struct foo { int foo; } xx;
extern int $2 (struct foo*);
$2(&xx);
],
eval "ac_cv_func_$2_noproto=yes",
eval "ac_cv_func_$2_noproto=no"))
define([foo], [NEED_]translit($2, [a-z], [A-Z])[_PROTO])
if test "$ac_cv_func_$2_noproto" = yes; then
	AC_DEFINE(foo, 1, [define if the system is missing a prototype for $2()])
fi
undefine([foo])
fi
])

dnl $Id: broken-glob.m4,v 1.2 1999/03/01 09:52:15 joda Exp $
dnl
dnl check for glob(3)
dnl
AC_DEFUN(AC_BROKEN_GLOB,[
AC_CACHE_CHECK(for working glob, ac_cv_func_glob_working,
ac_cv_func_glob_working=yes
AC_TRY_LINK([
#include <stdio.h>
#include <glob.h>],[
glob(NULL, GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE, NULL, NULL);
],:,ac_cv_func_glob_working=no,:))

if test "$ac_cv_func_glob_working" = yes; then
	AC_DEFINE(HAVE_GLOB, 1, [define if you have a glob() that groks 
	GLOB_BRACE, GLOB_NOCHECK, GLOB_QUOTE, and GLOB_TILDE])
fi
if test "$ac_cv_func_glob_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>
#include <glob.h>],glob)
fi
])

dnl
dnl $Id: capabilities.m4,v 1.2 1999/09/01 11:02:26 joda Exp $
dnl

dnl
dnl Test SGI capabilities
dnl

AC_DEFUN(KRB_CAPABILITIES,[

AC_CHECK_HEADERS(capability.h sys/capability.h)

AC_CHECK_FUNCS(sgi_getcapabilitybyname cap_set_proc)
])

dnl $Id: check-getpwnam_r-posix.m4,v 1.2 1999/03/23 16:47:31 joda Exp $
dnl
dnl check for getpwnam_r, and if it's posix or not

AC_DEFUN(AC_CHECK_GETPWNAM_R_POSIX,[
AC_FIND_FUNC_NO_LIBS(getpwnam_r,c_r)
if test "$ac_cv_func_getpwnam_r" = yes; then
	AC_CACHE_CHECK(if getpwnam_r is posix,ac_cv_func_getpwnam_r_posix,
	ac_libs="$LIBS"
	LIBS="$LIBS $LIB_getpwnam_r"
	AC_TRY_RUN([
#include <pwd.h>
int main()
{
	struct passwd pw, *pwd;
	return getpwnam_r("", &pw, NULL, 0, &pwd) < 0;
}
],ac_cv_func_getpwnam_r_posix=yes,ac_cv_func_getpwnam_r_posix=no,:)
LIBS="$ac_libs")
if test "$ac_cv_func_getpwnam_r_posix" = yes; then
	AC_DEFINE(POSIX_GETPWNAM_R, 1, [Define if getpwnam_r has POSIX flavour.])
fi
fi
])
dnl
dnl $Id: krb-func-getlogin.m4,v 1.1 1999/07/13 17:45:30 assar Exp $
dnl
dnl test for POSIX (broken) getlogin
dnl


AC_DEFUN(AC_FUNC_GETLOGIN, [
AC_CHECK_FUNCS(getlogin setlogin)
if test "$ac_cv_func_getlogin" = yes; then
AC_CACHE_CHECK(if getlogin is posix, ac_cv_func_getlogin_posix, [
if test "$ac_cv_func_getlogin" = yes -a "$ac_cv_func_setlogin" = yes; then
	ac_cv_func_getlogin_posix=no
else
	ac_cv_func_getlogin_posix=yes
fi
])
if test "$ac_cv_func_getlogin_posix" = yes; then
	AC_DEFINE(POSIX_GETLOGIN, 1, [Define if getlogin has POSIX flavour (and not BSD).])
fi
fi
])

dnl $Id: find-if-not-broken.m4,v 1.2 1998/03/16 22:16:27 joda Exp $
dnl
dnl
dnl Mix between AC_FIND_FUNC and AC_BROKEN
dnl

AC_DEFUN(AC_FIND_IF_NOT_BROKEN,
[AC_FIND_FUNC([$1], [$2], [$3], [$4])
if eval "test \"$ac_cv_func_$1\" != yes"; then
LIBOBJS[]="$LIBOBJS $1.o"
fi
AC_SUBST(LIBOBJS)dnl
])

dnl $Id: broken.m4,v 1.3 1998/03/16 22:16:19 joda Exp $
dnl
dnl
dnl Same as AC _REPLACE_FUNCS, just define HAVE_func if found in normal
dnl libraries 

AC_DEFUN(AC_BROKEN,
[for ac_func in $1
do
AC_CHECK_FUNC($ac_func, [
ac_tr_func=HAVE_[]upcase($ac_func)
AC_DEFINE_UNQUOTED($ac_tr_func)],[LIBOBJS[]="$LIBOBJS ${ac_func}.o"])
dnl autoheader tricks *sigh*
: << END
@@@funcs="$funcs $1"@@@
END
done
AC_SUBST(LIBOBJS)dnl
])

dnl $Id: krb-func-getcwd-broken.m4,v 1.2 1999/03/01 13:03:32 joda Exp $
dnl
dnl
dnl test for broken getcwd in (SunOS braindamage)
dnl

AC_DEFUN(AC_KRB_FUNC_GETCWD_BROKEN, [
if test "$ac_cv_func_getcwd" = yes; then
AC_MSG_CHECKING(if getcwd is broken)
AC_CACHE_VAL(ac_cv_func_getcwd_broken, [
ac_cv_func_getcwd_broken=no

AC_TRY_RUN([
#include <errno.h>
char *getcwd(char*, int);

void *popen(char *cmd, char *mode)
{
	errno = ENOTTY;
	return 0;
}

int main()
{
	char *ret;
	ret = getcwd(0, 1024);
	if(ret == 0 && errno == ENOTTY)
		return 0;
	return 1;
}
], ac_cv_func_getcwd_broken=yes,:,:)
])
if test "$ac_cv_func_getcwd_broken" = yes; then
	AC_DEFINE(BROKEN_GETCWD, 1, [Define if getcwd is broken (like in SunOS 4).])dnl
	LIBOBJS="$LIBOBJS getcwd.o"
	AC_SUBST(LIBOBJS)dnl
	AC_MSG_RESULT($ac_cv_func_getcwd_broken)
else
	AC_MSG_RESULT([seems ok])
fi
fi
])

dnl $Id: proto-compat.m4,v 1.3 1999/03/01 13:03:48 joda Exp $
dnl
dnl
dnl Check if the prototype of a function is compatible with another one
dnl

dnl AC_PROTO_COMPAT(includes, function, prototype)

AC_DEFUN(AC_PROTO_COMPAT, [
AC_CACHE_CHECK([if $2 is compatible with system prototype],
ac_cv_func_$2_proto_compat,
AC_TRY_COMPILE([$1],
[$3;],
eval "ac_cv_func_$2_proto_compat=yes",
eval "ac_cv_func_$2_proto_compat=no"))
define([foo], translit($2, [a-z], [A-Z])[_PROTO_COMPATIBLE])
if test "$ac_cv_func_$2_proto_compat" = yes; then
	AC_DEFINE(foo, 1, [define if prototype of $2 is compatible with
	$3])
fi
undefine([foo])
])
dnl $Id: check-var.m4,v 1.2 1999/03/01 09:52:23 joda Exp $
dnl
dnl AC_CHECK_VAR(includes, variable)
AC_DEFUN(AC_CHECK_VAR, [
AC_MSG_CHECKING(for $2)
AC_CACHE_VAL(ac_cv_var_$2, [
AC_TRY_LINK([extern int $2;
int foo() { return $2; }],
	    [foo()],
	    ac_cv_var_$2=yes, ac_cv_var_$2=no)
])
define([foo], [HAVE_]translit($2, [a-z], [A-Z]))

AC_MSG_RESULT(`eval echo \\$ac_cv_var_$2`)
if test `eval echo \\$ac_cv_var_$2` = yes; then
	AC_DEFINE_UNQUOTED(foo, 1, [define if you have $2])
	AC_CHECK_DECLARATION([$1],[$2])
fi
undefine([foo])
])

dnl $Id: check-declaration.m4,v 1.3 1999/03/01 13:03:08 joda Exp $
dnl
dnl
dnl Check if we need the declaration of a variable
dnl

dnl AC_HAVE_DECLARATION(includes, variable)
AC_DEFUN(AC_CHECK_DECLARATION, [
AC_MSG_CHECKING([if $2 is properly declared])
AC_CACHE_VAL(ac_cv_var_$2_declaration, [
AC_TRY_COMPILE([$1
extern struct { int foo; } $2;],
[$2.foo = 1;],
eval "ac_cv_var_$2_declaration=no",
eval "ac_cv_var_$2_declaration=yes")
])

define(foo, [HAVE_]translit($2, [a-z], [A-Z])[_DECLARATION])

AC_MSG_RESULT($ac_cv_var_$2_declaration)
if eval "test \"\$ac_cv_var_$2_declaration\" = yes"; then
	AC_DEFINE(foo, 1, [define if your system declares $2])
fi
undefine([foo])
])

dnl $Id: have-struct-field.m4,v 1.6 1999/07/29 01:44:32 assar Exp $
dnl
dnl check for fields in a structure
dnl
dnl AC_HAVE_STRUCT_FIELD(struct, field, headers)

AC_DEFUN(AC_HAVE_STRUCT_FIELD, [
define(cache_val, translit(ac_cv_type_$1_$2, [A-Z ], [a-z_]))
AC_CACHE_CHECK([for $2 in $1], cache_val,[
AC_TRY_COMPILE([$3],[$1 x; x.$2;],
cache_val=yes,
cache_val=no)])
if test "$cache_val" = yes; then
	define(foo, translit(HAVE_$1_$2, [a-z ], [A-Z_]))
	AC_DEFINE(foo, 1, [Define if $1 has field $2.])
	undefine([foo])
fi
undefine([cache_val])
])

dnl $Id: have-type.m4,v 1.4 1999/07/24 19:23:01 assar Exp $
dnl
dnl check for existance of a type

dnl AC_HAVE_TYPE(TYPE,INCLUDES)
AC_DEFUN(AC_HAVE_TYPE, [
cv=`echo "$1" | sed 'y%./+- %__p__%'`
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL([ac_cv_type_$cv],
AC_TRY_COMPILE(
[#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
$2],
[$1 foo;],
eval "ac_cv_type_$cv=yes",
eval "ac_cv_type_$cv=no"))dnl
AC_MSG_RESULT(`eval echo \\$ac_cv_type_$cv`)
if test `eval echo \\$ac_cv_type_$cv` = yes; then
  ac_tr_hdr=HAVE_`echo $1 | sed 'y%abcdefghijklmnopqrstuvwxyz./- %ABCDEFGHIJKLMNOPQRSTUVWXYZ____%'`
dnl autoheader tricks *sigh*
define(foo,translit($1, [ ], [_]))
: << END
@@@funcs="$funcs foo"@@@
END
undefine([foo])
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
])

dnl $Id: krb-struct-spwd.m4,v 1.3 1999/07/13 21:04:11 assar Exp $
dnl
dnl Test for `struct spwd'

AC_DEFUN(AC_KRB_STRUCT_SPWD, [
AC_MSG_CHECKING(for struct spwd)
AC_CACHE_VAL(ac_cv_struct_spwd, [
AC_TRY_COMPILE(
[#include <pwd.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif],
[struct spwd foo;],
ac_cv_struct_spwd=yes,
ac_cv_struct_spwd=no)
])
AC_MSG_RESULT($ac_cv_struct_spwd)

if test "$ac_cv_struct_spwd" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_SPWD, 1, [define if you have struct spwd])
fi
])

dnl $Id: krb-struct-winsize.m4,v 1.2 1999/03/01 09:52:23 joda Exp $
dnl
dnl
dnl Search for struct winsize
dnl

AC_DEFUN(AC_KRB_STRUCT_WINSIZE, [
AC_MSG_CHECKING(for struct winsize)
AC_CACHE_VAL(ac_cv_struct_winsize, [
ac_cv_struct_winsize=no
for i in sys/termios.h sys/ioctl.h; do
AC_EGREP_HEADER(
changequote(, )dnl
struct[ 	]*winsize,dnl
changequote([,])dnl
$i, ac_cv_struct_winsize=yes; break)dnl
done
])
if test "$ac_cv_struct_winsize" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_WINSIZE, 1, [define if struct winsize is declared in sys/termios.h])
fi
AC_MSG_RESULT($ac_cv_struct_winsize)
AC_EGREP_HEADER(ws_xpixel, termios.h, 
	AC_DEFINE(HAVE_WS_XPIXEL, 1, [define if struct winsize has ws_xpixel]))
AC_EGREP_HEADER(ws_ypixel, termios.h, 
	AC_DEFINE(HAVE_WS_YPIXEL, 1, [define if struct winsize has ws_ypixel]))
])

dnl $Id: check-type-extra.m4,v 1.2 1999/03/01 09:52:23 joda Exp $
dnl
dnl ac_check_type + extra headers

dnl AC_CHECK_TYPE_EXTRA(TYPE, DEFAULT, HEADERS)
AC_DEFUN(AC_CHECK_TYPE_EXTRA,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_type_$1,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
$3], ac_cv_type_$1=yes, ac_cv_type_$1=no)])dnl
AC_MSG_RESULT($ac_cv_type_$1)
if test $ac_cv_type_$1 = no; then
  AC_DEFINE($1, $2, [Define this to what the type $1 should be.])
fi
])

dnl $Id: krb-version.m4,v 1.1 1997/12/14 15:59:03 joda Exp $
dnl
dnl
dnl output a C header-file with some version strings
dnl
AC_DEFUN(AC_KRB_VERSION,[
dnl AC_OUTPUT_COMMANDS([
cat > include/newversion.h.in <<FOOBAR
char *${PACKAGE}_long_version = "@(#)\$Version: $PACKAGE-$VERSION by @USER@ on @HOST@ ($host) @DATE@ \$";
char *${PACKAGE}_version = "$PACKAGE-$VERSION";
FOOBAR

if test -f include/version.h && cmp -s include/newversion.h.in include/version.h.in; then
	echo "include/version.h is unchanged"
	rm -f include/newversion.h.in
else
 	echo "creating include/version.h"
 	User=${USER-${LOGNAME}}
 	Host=`(hostname || uname -n) 2>/dev/null | sed 1q`
 	Date=`date`
	mv -f include/newversion.h.in include/version.h.in
	sed -e "s/@USER@/$User/" -e "s/@HOST@/$Host/" -e "s/@DATE@/$Date/" include/version.h.in > include/version.h
fi
dnl ],host=$host PACKAGE=$PACKAGE VERSION=$VERSION)
])

