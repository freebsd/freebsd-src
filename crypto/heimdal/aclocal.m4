dnl aclocal.m4 generated automatically by aclocal 1.4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl $Id: acinclude.m4,v 1.15 1998/05/23 14:54:53 joda Exp $
dnl
dnl Only put things that for some reason can't live in the `cf'
dnl directory in this file.
dnl

dnl $xId: misc.m4,v 1.1 1997/12/14 15:59:04 joda Exp $
dnl
define(upcase,`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`)dnl

# Like AC_CONFIG_HEADER, but automatically create stamp file.

AC_DEFUN(AM_CONFIG_HEADER,
[AC_PREREQ([2.12])
AC_CONFIG_HEADER([$1])
dnl When config.status generates a header, we must update the stamp-h file.
dnl This file resides in the same directory as the config header
dnl that is generated.  We must strip everything past the first ":",
dnl and everything past the last "/".
AC_OUTPUT_COMMANDS(changequote(<<,>>)dnl
ifelse(patsubst(<<$1>>, <<[^ ]>>, <<>>), <<>>,
<<test -z "<<$>>CONFIG_HEADERS" || echo timestamp > patsubst(<<$1>>, <<^\([^:]*/\)?.*>>, <<\1>>)stamp-h<<>>dnl>>,
<<am_indx=1
for am_file in <<$1>>; do
  case " <<$>>CONFIG_HEADERS " in
  *" <<$>>am_file "*<<)>>
    echo timestamp > `echo <<$>>am_file | sed -e 's%:.*%%' -e 's%[^/]*$%%'`stamp-h$am_indx
    ;;
  esac
  am_indx=`expr "<<$>>am_indx" + 1`
done<<>>dnl>>)
changequote([,]))])

# Do all the work for Automake.  This macro actually does too much --
# some checks are only needed if your package does certain things.
# But this isn't really a big deal.

# serial 1

dnl Usage:
dnl AM_INIT_AUTOMAKE(package,version, [no-define])

AC_DEFUN(AM_INIT_AUTOMAKE,
[AC_REQUIRE([AC_PROG_INSTALL])
PACKAGE=[$1]
AC_SUBST(PACKAGE)
VERSION=[$2]
AC_SUBST(VERSION)
dnl test to see if srcdir already configured
if test "`cd $srcdir && pwd`" != "`pwd`" && test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
fi
ifelse([$3],,
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package]))
AC_REQUIRE([AM_SANITY_CHECK])
AC_REQUIRE([AC_ARG_PROGRAM])
dnl FIXME This is truly gross.
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AM_MISSING_PROG(MAKEINFO, makeinfo, $missing_dir)
AC_REQUIRE([AC_PROG_MAKE_SET])])

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "[$]*" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftestfile`
   fi
   if test "[$]*" != "X $srcdir/configure conftestfile" \
      && test "[$]*" != "X conftestfile $srcdir/configure"; then

      # If neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a
      # broken ls alias from the environment.  This has actually
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
alias in your environment])
   fi

   test "[$]2" = conftestfile
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

dnl AM_MISSING_PROG(NAME, PROGRAM, DIRECTORY)
dnl The program must properly implement --version.
AC_DEFUN(AM_MISSING_PROG,
[AC_MSG_CHECKING(for working $2)
# Run test in a subshell; some versions of sh will print an error if
# an executable is not found, even if stderr is redirected.
# Redirect stdin to placate older versions of autoconf.  Sigh.
if ($2 --version) < /dev/null > /dev/null 2>&1; then
   $1=$2
   AC_MSG_RESULT(found)
else
   $1="$3/missing $2"
   AC_MSG_RESULT(missing)
fi
AC_SUBST($1)])


dnl AM_PROG_LEX
dnl Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN(AM_PROG_LEX,
[missing_dir=ifelse([$1],,`cd $ac_aux_dir && pwd`,$1)
AC_CHECK_PROGS(LEX, flex lex, "$missing_dir/missing flex")
AC_PROG_LEX
AC_DECL_YYTEXT])

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



# serial 25 AM_PROG_LIBTOOL
AC_DEFUN(AM_PROG_LIBTOOL,
[AC_REQUIRE([AM_ENABLE_SHARED])dnl
AC_REQUIRE([AM_ENABLE_STATIC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_PROG_RANLIB])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AM_PROG_LD])dnl
AC_REQUIRE([AM_PROG_NM])dnl
AC_REQUIRE([AC_PROG_LN_S])dnl
dnl
# Always use our own libtool.
LIBTOOL='$(top_builddir)/libtool'
AC_SUBST(LIBTOOL)dnl

# Check for any special flags to pass to ltconfig.
libtool_flags=
test "$enable_shared" = no && libtool_flags="$libtool_flags --disable-shared"
test "$enable_static" = no && libtool_flags="$libtool_flags --disable-static"
test "$silent" = yes && libtool_flags="$libtool_flags --silent"
test "$ac_cv_prog_gcc" = yes && libtool_flags="$libtool_flags --with-gcc"
test "$ac_cv_prog_gnu_ld" = yes && libtool_flags="$libtool_flags --with-gnu-ld"

# Some flags need to be propagated to the compiler or linker for good
# libtool support.
case "$host" in
*-*-irix6*)
  # Find out which ABI we are using.
  echo '[#]line __oline__ "configure"' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case "`/usr/bin/file conftest.o`" in
    *32-bit*)
      LD="${LD-ld} -32"
      ;;
    *N32*)
      LD="${LD-ld} -n32"
      ;;
    *64-bit*)
      LD="${LD-ld} -64"
      ;;
    esac
  fi
  rm -rf conftest*
  ;;

*-*-sco3.2v5*)
  # On SCO OpenServer 5, we need -belf to get full-featured binaries.
  CFLAGS="$CFLAGS -belf"
  ;;
esac

# Actually configure libtool.  ac_aux_dir is where install-sh is found.
CC="$CC" CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" \
LD="$LD" NM="$NM" RANLIB="$RANLIB" LN_S="$LN_S" \
${CONFIG_SHELL-/bin/sh} $ac_aux_dir/ltconfig --no-reexec \
$libtool_flags --no-verify $ac_aux_dir/ltmain.sh $host \
|| AC_MSG_ERROR([libtool configure failed])

# Redirect the config.log output again, so that the ltconfig log is not
# clobbered by the next message.
exec 5>>./config.log
])

# AM_ENABLE_SHARED - implement the --enable-shared flag
# Usage: AM_ENABLE_SHARED[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AM_ENABLE_SHARED,
[define([AM_ENABLE_SHARED_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(shared,
changequote(<<, >>)dnl
<<  --enable-shared[=PKGS]  build shared libraries [default=>>AM_ENABLE_SHARED_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_shared=yes ;;
no) enable_shared=no ;;
*)
  enable_shared=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_shared=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_shared=AM_ENABLE_SHARED_DEFAULT)dnl
])

# AM_DISABLE_SHARED - set the default shared flag to --disable-shared
AC_DEFUN(AM_DISABLE_SHARED,
[AM_ENABLE_SHARED(no)])

# AM_DISABLE_STATIC - set the default static flag to --disable-static
AC_DEFUN(AM_DISABLE_STATIC,
[AM_ENABLE_STATIC(no)])

# AM_ENABLE_STATIC - implement the --enable-static flag
# Usage: AM_ENABLE_STATIC[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AM_ENABLE_STATIC,
[define([AM_ENABLE_STATIC_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(static,
changequote(<<, >>)dnl
<<  --enable-static[=PKGS]  build static libraries [default=>>AM_ENABLE_STATIC_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_static=yes ;;
no) enable_static=no ;;
*)
  enable_static=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_static=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_static=AM_ENABLE_STATIC_DEFAULT)dnl
])


# AM_PROG_LD - find the path to the GNU or non-GNU linker
AC_DEFUN(AM_PROG_LD,
[AC_ARG_WITH(gnu-ld,
[  --with-gnu-ld           assume the C compiler uses GNU ld [default=no]],
test "$withval" = no || with_gnu_ld=yes, with_gnu_ld=no)
AC_REQUIRE([AC_PROG_CC])
ac_prog=ld
if test "$ac_cv_prog_gcc" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by GCC])
  ac_prog=`($CC -print-prog-name=ld) 2>&5`
  case "$ac_prog" in
  # Accept absolute paths.
changequote(,)dnl
  /* | [A-Za-z]:\\*)
changequote([,])dnl
    test -z "$LD" && LD="$ac_prog"
    ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test "$with_gnu_ld" = yes; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL(ac_cv_path_LD,
[if test -z "$LD"; then
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  for ac_dir in $PATH; do
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog"; then
      ac_cv_path_LD="$ac_dir/$ac_prog"
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some GNU ld's only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      if "$ac_cv_path_LD" -v 2>&1 < /dev/null | egrep '(GNU|with BFD)' > /dev/null; then
	test "$with_gnu_ld" != no && break
      else
        test "$with_gnu_ld" != yes && break
      fi
    fi
  done
  IFS="$ac_save_ifs"
else
  ac_cv_path_LD="$LD" # Let the user override the test with a path.
fi])
LD="$ac_cv_path_LD"
if test -n "$LD"; then
  AC_MSG_RESULT($LD)
else
  AC_MSG_RESULT(no)
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
AC_SUBST(LD)
AM_PROG_LD_GNU
])

AC_DEFUN(AM_PROG_LD_GNU,
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], ac_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  ac_cv_prog_gnu_ld=yes
else
  ac_cv_prog_gnu_ld=no
fi])
])

# AM_PROG_NM - find the path to a BSD-compatible name lister
AC_DEFUN(AM_PROG_NM,
[AC_MSG_CHECKING([for BSD-compatible nm])
AC_CACHE_VAL(ac_cv_path_NM,
[if test -n "$NM"; then
  # Let the user override the test.
  ac_cv_path_NM="$NM"
else
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  for ac_dir in /usr/ucb /usr/ccs/bin $PATH /bin; do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/nm; then
      # Check to see if the nm accepts a BSD-compat flag.
      # Adding the `sed 1q' prevents false positives on HP-UX, which says:
      #   nm: unknown option "B" ignored
      if ($ac_dir/nm -B /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
        ac_cv_path_NM="$ac_dir/nm -B"
      elif ($ac_dir/nm -p /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
        ac_cv_path_NM="$ac_dir/nm -p"
      else
        ac_cv_path_NM="$ac_dir/nm"
      fi
      break
    fi
  done
  IFS="$ac_save_ifs"
  test -z "$ac_cv_path_NM" && ac_cv_path_NM=nm
fi])
NM="$ac_cv_path_NM"
AC_MSG_RESULT([$NM])
AC_SUBST(NM)
])

dnl $Id: wflags.m4,v 1.3 1999/03/11 12:11:41 joda Exp $
dnl
dnl set WFLAGS

AC_DEFUN(AC_WFLAGS,[
WFLAGS_NOUNUSED=""
WFLAGS_NOIMPLICITINT=""
if test -z "$WFLAGS" -a "$GCC" = "yes"; then
  # -Wno-implicit-int for broken X11 headers
  # leave these out for now:
  #   -Wcast-align doesn't work well on alpha osf/1
  #   -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast
  #   -Wmissing-declarations -Wnested-externs
  WFLAGS="ifelse($#, 0,-Wall, $1)"
  WFLAGS_NOUNUSED="-Wno-unused"
  WFLAGS_NOIMPLICITINT="-Wno-implicit-int"
fi
AC_SUBST(WFLAGS)dnl
AC_SUBST(WFLAGS_NOUNUSED)dnl
AC_SUBST(WFLAGS_NOIMPLICITINT)dnl
])

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

dnl $Id: find-func.m4,v 1.1 1997/12/14 15:58:58 joda Exp $
dnl
dnl AC_FIND_FUNC(func, libraries, includes, arguments)
AC_DEFUN(AC_FIND_FUNC, [
AC_FIND_FUNC_NO_LIBS([$1], [$2], [$3], [$4])
if test -n "$LIB_$1"; then
	LIBS="$LIB_$1 $LIBS"
fi
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

dnl $Id: check-man.m4,v 1.2 1999/03/21 14:30:50 joda Exp $
dnl check how to format manual pages
dnl

AC_DEFUN(AC_CHECK_MAN,
[AC_PATH_PROG(NROFF, nroff)
AC_PATH_PROG(GROFF, groff)
AC_CACHE_CHECK(how to format man pages,ac_cv_sys_man_format,
[cat > conftest.1 << END
.Dd January 1, 1970
.Dt CONFTEST 1
.Sh NAME
.Nm conftest
.Nd
foobar
END

if test "$NROFF" ; then
	for i in "-mdoc" "-mandoc"; do
		if "$NROFF" $i conftest.1 2> /dev/null | \
			grep Jan > /dev/null 2>&1 ; then
			ac_cv_sys_man_format="$NROFF $i"
			break
		fi
	done
fi
if test "$ac_cv_sys_man_format" = "" -a "$GROFF" ; then
	for i in "-mdoc" "-mandoc"; do
		if "$GROFF" -Tascii $i conftest.1 2> /dev/null | \
			grep Jan > /dev/null 2>&1 ; then
			ac_cv_sys_man_format="$GROFF -Tascii $i"
			break
		fi
	done
fi
if test "$ac_cv_sys_man_format"; then
	ac_cv_sys_man_format="$ac_cv_sys_man_format \[$]< > \[$]@"
fi
])
if test "$ac_cv_sys_man_format"; then
	CATMAN="$ac_cv_sys_man_format"
	AC_SUBST(CATMAN)
fi
AM_CONDITIONAL(CATMAN, test "$CATMAN")
AC_CACHE_CHECK(extension of pre-formatted manual pages,ac_cv_sys_catman_ext,
[if grep _suffix /etc/man.conf > /dev/null 2>&1; then
	ac_cv_sys_catman_ext=0
else
	ac_cv_sys_catman_ext=number
fi
])
if test "$ac_cv_sys_catman_ext" = number; then
	CATMANEXT='$$ext'
else
	CATMANEXT=0
fi
AC_SUBST(CATMANEXT)

])
dnl
dnl $Id: krb-bigendian.m4,v 1.5 2000/01/08 10:34:44 assar Exp $
dnl

dnl check if this computer is little or big-endian
dnl if we can figure it out at compile-time then don't define the cpp symbol
dnl otherwise test for it and define it.  also allow options for overriding
dnl it when cross-compiling

AC_DEFUN(KRB_C_BIGENDIAN, [
AC_ARG_ENABLE(bigendian,
[  --enable-bigendian	the target is big endian],
krb_cv_c_bigendian=yes)
AC_ARG_ENABLE(littleendian,
[  --enable-littleendian	the target is little endian],
krb_cv_c_bigendian=no)
AC_CACHE_CHECK(whether byte order is known at compile time,
krb_cv_c_bigendian_compile,
[AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/param.h>],[
#if !BYTE_ORDER || !BIG_ENDIAN || !LITTLE_ENDIAN
 bogus endian macros
#endif], krb_cv_c_bigendian_compile=yes, krb_cv_c_bigendian_compile=no)])
if test "$krb_cv_c_bigendian_compile" = "no"; then
  AC_CACHE_CHECK(whether byte ordering is bigendian, krb_cv_c_bigendian,[
  if test "$krb_cv_c_bigendian" = ""; then
    krb_cv_c_bigendian=unknown
  fi
  AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/param.h>],[
#if BYTE_ORDER != BIG_ENDIAN
  not big endian
#endif], krb_cv_c_bigendian=yes, krb_cv_c_bigendian=no)
  if test "$krb_cv_c_bigendian" = "unknown"; then
    AC_TRY_RUN([main () {
      /* Are we little or big endian?  From Harbison&Steele.  */
      union
      {
	long l;
	char c[sizeof (long)];
      } u;
      u.l = 1;
      exit (u.c[sizeof (long) - 1] == 1);
    }], krb_cv_c_bigendian=no, krb_cv_c_bigendian=yes,
    AC_MSG_ERROR([specify either --enable-bigendian or --enable-littleendian]))
  fi
  ])
  if test "$krb_cv_c_bigendian" = "yes"; then
    AC_DEFINE(WORDS_BIGENDIAN, 1, [define if target is big endian])dnl
  fi
fi
if test "$krb_cv_c_bigendian_compile" = "yes"; then
  AC_DEFINE(ENDIANESS_IN_SYS_PARAM_H, 1, [define if sys/param.h defines the endiness])dnl
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

dnl $Id: have-type.m4,v 1.5 1999/12/31 03:10:22 assar Exp $
dnl
dnl check for existance of a type

dnl AC_HAVE_TYPE(TYPE,INCLUDES)
AC_DEFUN(AC_HAVE_TYPE, [
AC_REQUIRE([AC_HEADER_STDC])
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

dnl $Id: krb-ipv6.m4,v 1.8 2000/01/01 11:44:45 assar Exp $
dnl
dnl test for IPv6
dnl
AC_DEFUN(AC_KRB_IPV6, [
AC_ARG_WITH(ipv6,
[  --without-ipv6	do not enable IPv6 support],[
if test "$withval" = "no"; then
	ac_cv_lib_ipv6=no
fi])
AC_CACHE_VAL(ac_cv_lib_ipv6,
[dnl check for different v6 implementations (by itojun)
v6type=unknown
v6lib=none

AC_MSG_CHECKING([ipv6 stack type])
for i in v6d toshiba kame inria zeta linux; do
	case $i in
	v6d)
		AC_EGREP_CPP(yes, [dnl
#include </usr/local/v6/include/sys/types.h>
#ifdef __V6D__
yes
#endif],
			[v6type=$i; v6lib=v6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-I/usr/local/v6/include $CFLAGS"])
		;;
	toshiba)
		AC_EGREP_CPP(yes, [dnl
#include <sys/param.h>
#ifdef _TOSHIBA_INET6
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	kame)
		AC_EGREP_CPP(yes, [dnl
#include <netinet/in.h>
#ifdef __KAME__
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	inria)
		AC_EGREP_CPP(yes, [dnl
#include <netinet/in.h>
#ifdef IPV6_INRIA_VERSION
yes
#endif],
			[v6type=$i; CFLAGS="-DINET6 $CFLAGS"])
		;;
	zeta)
		AC_EGREP_CPP(yes, [dnl
#include <sys/param.h>
#ifdef _ZETA_MINAMI_INET6
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	linux)
		if test -d /usr/inet6; then
			v6type=$i
			v6lib=inet6
			v6libdir=/usr/inet6
			CFLAGS="-DINET6 $CFLAGS"
		fi
		;;
	esac
	if test "$v6type" != "unknown"; then
		break
	fi
done
AC_MSG_RESULT($v6type)

if test "$v6lib" != "none"; then
	for dir in $v6libdir /usr/local/v6/lib /usr/local/lib; do
		if test -d $dir -a -f $dir/lib$v6lib.a; then
			LIBS="-L$dir -l$v6lib $LIBS"
			break
		fi
	done
fi
AC_TRY_LINK([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
],
[
 struct sockaddr_in6 sin6;
 int s;

 s = socket(AF_INET6, SOCK_DGRAM, 0);

 sin6.sin6_family = AF_INET6;
 sin6.sin6_port = htons(17);
 sin6.sin6_addr = in6addr_any;
 bind(s, (struct sockaddr *)&sin6, sizeof(sin6));
],
ac_cv_lib_ipv6=yes,
ac_cv_lib_ipv6=no)])
AC_MSG_CHECKING(for IPv6)
AC_MSG_RESULT($ac_cv_lib_ipv6)
if test "$ac_cv_lib_ipv6" = yes; then
  AC_DEFINE(HAVE_IPV6, 1, [Define if you have IPv6.])
fi
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

dnl $Id: auth-modules.m4,v 1.1 1999/03/21 13:48:00 joda Exp $
dnl
dnl Figure what authentication modules should be built

AC_DEFUN(AC_AUTH_MODULES,[
AC_MSG_CHECKING(which authentication modules should be built)

LIB_AUTH_SUBDIRS=

if test "$ac_cv_header_siad_h" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS sia"
fi

if test "$ac_cv_header_security_pam_modules_h" = yes -a "$enable_shared" = yes; then
	LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS pam"
fi

case "${host}" in
changequote(,)dnl
*-*-irix[56]*) LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS afskauthlib" ;;
changequote([,])dnl
esac

AC_MSG_RESULT($LIB_AUTH_SUBDIRS)

AC_SUBST(LIB_AUTH_SUBDIRS)dnl
])

