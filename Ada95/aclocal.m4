dnl***************************************************************************
dnl Copyright (c) 2010,2011 Free Software Foundation, Inc.                   *
dnl                                                                          *
dnl Permission is hereby granted, free of charge, to any person obtaining a  *
dnl copy of this software and associated documentation files (the            *
dnl "Software"), to deal in the Software without restriction, including      *
dnl without limitation the rights to use, copy, modify, merge, publish,      *
dnl distribute, distribute with modifications, sublicense, and/or sell       *
dnl copies of the Software, and to permit persons to whom the Software is    *
dnl furnished to do so, subject to the following conditions:                 *
dnl                                                                          *
dnl The above copyright notice and this permission notice shall be included  *
dnl in all copies or substantial portions of the Software.                   *
dnl                                                                          *
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
dnl MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
dnl IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
dnl DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
dnl OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
dnl THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
dnl                                                                          *
dnl Except as contained in this notice, the name(s) of the above copyright   *
dnl holders shall not be used in advertising or otherwise to promote the     *
dnl sale, use or other dealings in this Software without prior written       *
dnl authorization.                                                           *
dnl***************************************************************************
dnl
dnl Author: Thomas E. Dickey
dnl
dnl $Id: aclocal.m4,v 1.20 2011/01/22 19:46:50 tom Exp $
dnl Macros used in NCURSES Ada95 auto-configuration script.
dnl
dnl These macros are maintained separately from NCURSES.  The copyright on
dnl this file applies to the aggregation of macros and does not affect use of
dnl these macros in other applications.
dnl
dnl See http://invisible-island.net/autoconf/ for additional information.
dnl
dnl ---------------------------------------------------------------------------
dnl ---------------------------------------------------------------------------
dnl CF_ADA_INCLUDE_DIRS version: 6 updated: 2010/02/26 19:52:07
dnl -------------------
dnl Construct the list of include-options for the C programs in the Ada95
dnl binding.
AC_DEFUN([CF_ADA_INCLUDE_DIRS],
[
ACPPFLAGS="-I. -I../include -I../../include $ACPPFLAGS"
if test "$srcdir" != "."; then
	ACPPFLAGS="-I\${srcdir}/../../include $ACPPFLAGS"
fi
if test "$GCC" != yes; then
	ACPPFLAGS="$ACPPFLAGS -I\${includedir}"
elif test "$includedir" != "/usr/include"; then
	if test "$includedir" = '${prefix}/include' ; then
		if test $prefix != /usr ; then
			ACPPFLAGS="$ACPPFLAGS -I\${includedir}"
		fi
	else
		ACPPFLAGS="$ACPPFLAGS -I\${includedir}"
	fi
fi
AC_SUBST(ACPPFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_ADAFLAGS version: 1 updated: 2010/06/19 15:22:18
dnl ---------------
dnl Add to $ADAFLAGS, which is substituted into makefile and scripts.
AC_DEFUN([CF_ADD_ADAFLAGS],[
 	ADAFLAGS="$ADAFLAGS $1"
	AC_SUBST(ADAFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_CFLAGS version: 10 updated: 2010/05/26 05:38:42
dnl -------------
dnl Copy non-preprocessor flags to $CFLAGS, preprocessor flags to $CPPFLAGS
dnl The second parameter if given makes this macro verbose.
dnl
dnl Put any preprocessor definitions that use quoted strings in $EXTRA_CPPFLAGS,
dnl to simplify use of $CPPFLAGS in compiler checks, etc., that are easily
dnl confused by the quotes (which require backslashes to keep them usable).
AC_DEFUN([CF_ADD_CFLAGS],
[
cf_fix_cppflags=no
cf_new_cflags=
cf_new_cppflags=
cf_new_extra_cppflags=

for cf_add_cflags in $1
do
case $cf_fix_cppflags in
no)
	case $cf_add_cflags in #(vi
	-undef|-nostdinc*|-I*|-D*|-U*|-E|-P|-C) #(vi
		case $cf_add_cflags in
		-D*)
			cf_tst_cflags=`echo ${cf_add_cflags} |sed -e 's/^-D[[^=]]*='\''\"[[^"]]*//'`

			test "${cf_add_cflags}" != "${cf_tst_cflags}" \
				&& test -z "${cf_tst_cflags}" \
				&& cf_fix_cppflags=yes

			if test $cf_fix_cppflags = yes ; then
				cf_new_extra_cppflags="$cf_new_extra_cppflags $cf_add_cflags"
				continue
			elif test "${cf_tst_cflags}" = "\"'" ; then
				cf_new_extra_cppflags="$cf_new_extra_cppflags $cf_add_cflags"
				continue
			fi
			;;
		esac
		case "$CPPFLAGS" in
		*$cf_add_cflags) #(vi
			;;
		*) #(vi
			case $cf_add_cflags in #(vi
			-D*)
				cf_tst_cppflags=`echo "x$cf_add_cflags" | sed -e 's/^...//' -e 's/=.*//'`
				CF_REMOVE_DEFINE(CPPFLAGS,$CPPFLAGS,$cf_tst_cppflags)
				;;
			esac
			cf_new_cppflags="$cf_new_cppflags $cf_add_cflags"
			;;
		esac
		;;
	*)
		cf_new_cflags="$cf_new_cflags $cf_add_cflags"
		;;
	esac
	;;
yes)
	cf_new_extra_cppflags="$cf_new_extra_cppflags $cf_add_cflags"

	cf_tst_cflags=`echo ${cf_add_cflags} |sed -e 's/^[[^"]]*"'\''//'`

	test "${cf_add_cflags}" != "${cf_tst_cflags}" \
		&& test -z "${cf_tst_cflags}" \
		&& cf_fix_cppflags=no
	;;
esac
done

if test -n "$cf_new_cflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$CFLAGS $cf_new_cflags)])
	CFLAGS="$CFLAGS $cf_new_cflags"
fi

if test -n "$cf_new_cppflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$CPPFLAGS $cf_new_cppflags)])
	CPPFLAGS="$CPPFLAGS $cf_new_cppflags"
fi

if test -n "$cf_new_extra_cppflags" ; then
	ifelse([$2],,,[CF_VERBOSE(add to \$EXTRA_CPPFLAGS $cf_new_extra_cppflags)])
	EXTRA_CPPFLAGS="$cf_new_extra_cppflags $EXTRA_CPPFLAGS"
fi

AC_SUBST(EXTRA_CPPFLAGS)

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_INCDIR version: 13 updated: 2010/05/26 16:44:57
dnl -------------
dnl Add an include-directory to $CPPFLAGS.  Don't add /usr/include, since it's
dnl redundant.  We don't normally need to add -I/usr/local/include for gcc,
dnl but old versions (and some misinstalled ones) need that.  To make things
dnl worse, gcc 3.x may give error messages if -I/usr/local/include is added to
dnl the include-path).
AC_DEFUN([CF_ADD_INCDIR],
[
if test -n "$1" ; then
  for cf_add_incdir in $1
  do
	while test $cf_add_incdir != /usr/include
	do
	  if test -d $cf_add_incdir
	  then
		cf_have_incdir=no
		if test -n "$CFLAGS$CPPFLAGS" ; then
		  # a loop is needed to ensure we can add subdirs of existing dirs
		  for cf_test_incdir in $CFLAGS $CPPFLAGS ; do
			if test ".$cf_test_incdir" = ".-I$cf_add_incdir" ; then
			  cf_have_incdir=yes; break
			fi
		  done
		fi

		if test "$cf_have_incdir" = no ; then
		  if test "$cf_add_incdir" = /usr/local/include ; then
			if test "$GCC" = yes
			then
			  cf_save_CPPFLAGS=$CPPFLAGS
			  CPPFLAGS="$CPPFLAGS -I$cf_add_incdir"
			  AC_TRY_COMPILE([#include <stdio.h>],
				  [printf("Hello")],
				  [],
				  [cf_have_incdir=yes])
			  CPPFLAGS=$cf_save_CPPFLAGS
			fi
		  fi
		fi

		if test "$cf_have_incdir" = no ; then
		  CF_VERBOSE(adding $cf_add_incdir to include-path)
		  ifelse([$2],,CPPFLAGS,[$2])="$ifelse([$2],,CPPFLAGS,[$2]) -I$cf_add_incdir"

		  cf_top_incdir=`echo $cf_add_incdir | sed -e 's%/include/.*$%/include%'`
		  test "$cf_top_incdir" = "$cf_add_incdir" && break
		  cf_add_incdir="$cf_top_incdir"
		else
		  break
		fi
	  fi
	done
  done
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LIB version: 2 updated: 2010/06/02 05:03:05
dnl ----------
dnl Add a library, used to enforce consistency.
dnl
dnl $1 = library to add, without the "-l"
dnl $2 = variable to update (default $LIBS)
AC_DEFUN([CF_ADD_LIB],[CF_ADD_LIBS(-l$1,ifelse($2,,LIBS,[$2]))])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LIBDIR version: 9 updated: 2010/05/26 16:44:57
dnl -------------
dnl	Adds to the library-path
dnl
dnl	Some machines have trouble with multiple -L options.
dnl
dnl $1 is the (list of) directory(s) to add
dnl $2 is the optional name of the variable to update (default LDFLAGS)
dnl
AC_DEFUN([CF_ADD_LIBDIR],
[
if test -n "$1" ; then
  for cf_add_libdir in $1
  do
    if test $cf_add_libdir = /usr/lib ; then
      :
    elif test -d $cf_add_libdir
    then
      cf_have_libdir=no
      if test -n "$LDFLAGS$LIBS" ; then
        # a loop is needed to ensure we can add subdirs of existing dirs
        for cf_test_libdir in $LDFLAGS $LIBS ; do
          if test ".$cf_test_libdir" = ".-L$cf_add_libdir" ; then
            cf_have_libdir=yes; break
          fi
        done
      fi
      if test "$cf_have_libdir" = no ; then
        CF_VERBOSE(adding $cf_add_libdir to library-path)
        ifelse([$2],,LDFLAGS,[$2])="-L$cf_add_libdir $ifelse([$2],,LDFLAGS,[$2])"
      fi
    fi
  done
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_LIBS version: 1 updated: 2010/06/02 05:03:05
dnl -----------
dnl Add one or more libraries, used to enforce consistency.
dnl
dnl $1 = libraries to add, with the "-l", etc.
dnl $2 = variable to update (default $LIBS)
AC_DEFUN([CF_ADD_LIBS],[ifelse($2,,LIBS,[$2])="$1 [$]ifelse($2,,LIBS,[$2])"])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ADD_SUBDIR_PATH version: 3 updated: 2010/07/03 20:58:12
dnl ------------------
dnl Append to a search-list for a nonstandard header/lib-file
dnl	$1 = the variable to return as result
dnl	$2 = the package name
dnl	$3 = the subdirectory, e.g., bin, include or lib
dnl $4 = the directory under which we will test for subdirectories
dnl $5 = a directory that we do not want $4 to match
AC_DEFUN([CF_ADD_SUBDIR_PATH],
[
test "$4" != "$5" && \
test -d "$4" && \
ifelse([$5],NONE,,[(test $5 = NONE || test "$4" != "$5") &&]) {
	test -n "$verbose" && echo "	... testing for $3-directories under $4"
	test -d $4/$3 &&          $1="[$]$1 $4/$3"
	test -d $4/$3/$2 &&       $1="[$]$1 $4/$3/$2"
	test -d $4/$3/$2/$3 &&    $1="[$]$1 $4/$3/$2/$3"
	test -d $4/$2/$3 &&       $1="[$]$1 $4/$2/$3"
	test -d $4/$2/$3/$2 &&    $1="[$]$1 $4/$2/$3/$2"
}
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ANSI_CC_CHECK version: 10 updated: 2010/10/23 15:52:32
dnl ----------------
dnl This is adapted from the macros 'fp_PROG_CC_STDC' and 'fp_C_PROTOTYPES'
dnl in the sharutils 4.2 distribution.
AC_DEFUN([CF_ANSI_CC_CHECK],
[
AC_CACHE_CHECK(for ${CC:-cc} option to accept ANSI C, cf_cv_ansi_cc,[
cf_cv_ansi_cc=no
cf_save_CFLAGS="$CFLAGS"
cf_save_CPPFLAGS="$CPPFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
# UnixWare 1.2		(cannot use -Xc, since ANSI/POSIX clashes)
for cf_arg in "-DCC_HAS_PROTOS" \
	"" \
	-qlanglvl=ansi \
	-std1 \
	-Ae \
	"-Aa -D_HPUX_SOURCE" \
	-Xc
do
	CF_ADD_CFLAGS($cf_arg)
	AC_TRY_COMPILE(
[
#ifndef CC_HAS_PROTOS
#if !defined(__STDC__) || (__STDC__ != 1)
choke me
#endif
#endif
],[
	int test (int i, double x);
	struct s1 {int (*f) (int a);};
	struct s2 {int (*f) (double a);};],
	[cf_cv_ansi_cc="$cf_arg"; break])
done
CFLAGS="$cf_save_CFLAGS"
CPPFLAGS="$cf_save_CPPFLAGS"
])

if test "$cf_cv_ansi_cc" != "no"; then
if test ".$cf_cv_ansi_cc" != ".-DCC_HAS_PROTOS"; then
	CF_ADD_CFLAGS($cf_cv_ansi_cc)
else
	AC_DEFINE(CC_HAS_PROTOS)
fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_ANSI_CC_REQD version: 4 updated: 2008/03/23 14:48:54
dnl ---------------
dnl For programs that must use an ANSI compiler, obtain compiler options that
dnl will make it recognize prototypes.  We'll do preprocessor checks in other
dnl macros, since tools such as unproto can fake prototypes, but only part of
dnl the preprocessor.
AC_DEFUN([CF_ANSI_CC_REQD],
[AC_REQUIRE([CF_ANSI_CC_CHECK])
if test "$cf_cv_ansi_cc" = "no"; then
	AC_MSG_ERROR(
[Your compiler does not appear to recognize prototypes.
You have the following choices:
	a. adjust your compiler options
	b. get an up-to-date compiler
	c. use a wrapper such as unproto])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_AR_FLAGS version: 5 updated: 2010/05/20 20:24:29
dnl -----------
dnl Check for suitable "ar" (archiver) options for updating an archive.
AC_DEFUN([CF_AR_FLAGS],[
AC_REQUIRE([CF_PROG_AR])

AC_CACHE_CHECK(for options to update archives, cf_cv_ar_flags,[
	cf_cv_ar_flags=unknown
	for cf_ar_flags in -curv curv -crv crv -cqv cqv -rv rv
	do

		# check if $ARFLAGS already contains this choice
		if test "x$ARFLAGS" != "x" ; then
			cf_check_ar_flags=`echo "x$ARFLAGS" | sed -e "s/$cf_ar_flags\$//" -e "s/$cf_ar_flags / /"`
			if test "x$ARFLAGS" != "$cf_check_ar_flags" ; then
				cf_cv_ar_flags=
				break
			fi
		fi

		rm -f conftest.$ac_cv_objext
		rm -f conftest.a

		cat >conftest.$ac_ext <<EOF
#line __oline__ "configure"
int	testdata[[3]] = { 123, 456, 789 };
EOF
		if AC_TRY_EVAL(ac_compile) ; then
			echo "$AR $ARFLAGS $cf_ar_flags conftest.a conftest.$ac_cv_objext" >&AC_FD_CC
			$AR $ARFLAGS $cf_ar_flags conftest.a conftest.$ac_cv_objext 2>&AC_FD_CC 1>/dev/null
			if test -f conftest.a ; then
				cf_cv_ar_flags=$cf_ar_flags
				break
			fi
		else
			CF_VERBOSE(cannot compile test-program)
			break
		fi
	done
	rm -f conftest.a conftest.$ac_ext conftest.$ac_cv_objext
])

if test -n "$ARFLAGS" ; then
	if test -n "$cf_cv_ar_flags" ; then
		ARFLAGS="$ARFLAGS $cf_cv_ar_flags"
	fi
else
	ARFLAGS=$cf_cv_ar_flags
fi

AC_SUBST(ARFLAGS)
])
dnl ---------------------------------------------------------------------------
dnl CF_BUILD_CC version: 6 updated: 2006/10/14 15:23:15
dnl -----------
dnl If we're cross-compiling, allow the user to override the tools and their
dnl options.  The configure script is oriented toward identifying the host
dnl compiler, etc., but we need a build compiler to generate parts of the
dnl source.
dnl
dnl $1 = default for $CPPFLAGS
dnl $2 = default for $LIBS
AC_DEFUN([CF_BUILD_CC],[
AC_REQUIRE([CF_PROG_EXT])
if test "$cross_compiling" = yes ; then

	# defaults that we might want to override
	: ${BUILD_CFLAGS:=''}
	: ${BUILD_CPPFLAGS:='ifelse([$1],,,[$1])'}
	: ${BUILD_LDFLAGS:=''}
	: ${BUILD_LIBS:='ifelse([$2],,,[$2])'}
	: ${BUILD_EXEEXT:='$x'}
	: ${BUILD_OBJEXT:='o'}

	AC_ARG_WITH(build-cc,
		[  --with-build-cc=XXX     the build C compiler ($BUILD_CC)],
		[BUILD_CC="$withval"],
		[AC_CHECK_PROGS(BUILD_CC, gcc cc cl)])
	AC_MSG_CHECKING(for native build C compiler)
	AC_MSG_RESULT($BUILD_CC)

	AC_MSG_CHECKING(for native build C preprocessor)
	AC_ARG_WITH(build-cpp,
		[  --with-build-cpp=XXX    the build C preprocessor ($BUILD_CPP)],
		[BUILD_CPP="$withval"],
		[BUILD_CPP='${BUILD_CC} -E'])
	AC_MSG_RESULT($BUILD_CPP)

	AC_MSG_CHECKING(for native build C flags)
	AC_ARG_WITH(build-cflags,
		[  --with-build-cflags=XXX the build C compiler-flags ($BUILD_CFLAGS)],
		[BUILD_CFLAGS="$withval"])
	AC_MSG_RESULT($BUILD_CFLAGS)

	AC_MSG_CHECKING(for native build C preprocessor-flags)
	AC_ARG_WITH(build-cppflags,
		[  --with-build-cppflags=XXX the build C preprocessor-flags ($BUILD_CPPFLAGS)],
		[BUILD_CPPFLAGS="$withval"])
	AC_MSG_RESULT($BUILD_CPPFLAGS)

	AC_MSG_CHECKING(for native build linker-flags)
	AC_ARG_WITH(build-ldflags,
		[  --with-build-ldflags=XXX the build linker-flags ($BUILD_LDFLAGS)],
		[BUILD_LDFLAGS="$withval"])
	AC_MSG_RESULT($BUILD_LDFLAGS)

	AC_MSG_CHECKING(for native build linker-libraries)
	AC_ARG_WITH(build-libs,
		[  --with-build-libs=XXX   the build libraries (${BUILD_LIBS})],
		[BUILD_LIBS="$withval"])
	AC_MSG_RESULT($BUILD_LIBS)

	# this assumes we're on Unix.
	BUILD_EXEEXT=
	BUILD_OBJEXT=o

	: ${BUILD_CC:='${CC}'}

	if ( test "$BUILD_CC" = "$CC" || test "$BUILD_CC" = '${CC}' ) ; then
		AC_MSG_ERROR([Cross-build requires two compilers.
Use --with-build-cc to specify the native compiler.])
	fi

else
	: ${BUILD_CC:='${CC}'}
	: ${BUILD_CPP:='${CPP}'}
	: ${BUILD_CFLAGS:='${CFLAGS}'}
	: ${BUILD_CPPFLAGS:='${CPPFLAGS}'}
	: ${BUILD_LDFLAGS:='${LDFLAGS}'}
	: ${BUILD_LIBS:='${LIBS}'}
	: ${BUILD_EXEEXT:='$x'}
	: ${BUILD_OBJEXT:='o'}
fi

AC_SUBST(BUILD_CC)
AC_SUBST(BUILD_CPP)
AC_SUBST(BUILD_CFLAGS)
AC_SUBST(BUILD_CPPFLAGS)
AC_SUBST(BUILD_LDFLAGS)
AC_SUBST(BUILD_LIBS)
AC_SUBST(BUILD_EXEEXT)
AC_SUBST(BUILD_OBJEXT)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CFG_DEFAULTS version: 7 updated: 2005/09/24 16:15:00
dnl ---------------
dnl Determine the default configuration into which we'll install ncurses.  This
dnl can be overridden by the user's command-line options.  There's two items to
dnl look for:
dnl	1. the prefix (e.g., /usr)
dnl	2. the header files (e.g., /usr/include/ncurses)
dnl We'll look for a previous installation of ncurses and use the same defaults.
dnl
dnl We don't use AC_PREFIX_DEFAULT, because it gets evaluated too soon, and
dnl we don't use AC_PREFIX_PROGRAM, because we cannot distinguish ncurses's
dnl programs from a vendor's.
AC_DEFUN([CF_CFG_DEFAULTS],
[
AC_MSG_CHECKING(for prefix)
if test "x$prefix" = "xNONE" ; then
	case "$cf_cv_system_name" in
		# non-vendor systems don't have a conflict
	openbsd*|freebsd*|linux*|cygwin*|k*bsd*-gnu)
		prefix=/usr
		;;
	*)	prefix=$ac_default_prefix
		;;
	esac
fi
AC_MSG_RESULT($prefix)

if test "x$prefix" = "xNONE" ; then
AC_MSG_CHECKING(for default include-directory)
test -n "$verbose" && echo 1>&AC_FD_MSG
for cf_symbol in \
	$includedir \
	$includedir/ncurses \
	$prefix/include \
	$prefix/include/ncurses \
	/usr/local/include \
	/usr/local/include/ncurses \
	/usr/include \
	/usr/include/ncurses
do
	cf_dir=`eval echo $cf_symbol`
	if test -f $cf_dir/curses.h ; then
	if ( fgrep NCURSES_VERSION $cf_dir/curses.h 2>&1 >/dev/null ) ; then
		includedir="$cf_symbol"
		test -n "$verbose"  && echo $ac_n "	found " 1>&AC_FD_MSG
		break
	fi
	fi
	test -n "$verbose"  && echo "	tested $cf_dir" 1>&AC_FD_MSG
done
AC_MSG_RESULT($includedir)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CHECK_CACHE version: 11 updated: 2008/03/23 14:45:59
dnl --------------
dnl Check if we're accidentally using a cache from a different machine.
dnl Derive the system name, as a check for reusing the autoconf cache.
dnl
dnl If we've packaged config.guess and config.sub, run that (since it does a
dnl better job than uname).  Normally we'll use AC_CANONICAL_HOST, but allow
dnl an extra parameter that we may override, e.g., for AC_CANONICAL_SYSTEM
dnl which is useful in cross-compiles.
dnl
dnl Note: we would use $ac_config_sub, but that is one of the places where
dnl autoconf 2.5x broke compatibility with autoconf 2.13
AC_DEFUN([CF_CHECK_CACHE],
[
if test -f $srcdir/config.guess || test -f $ac_aux_dir/config.guess ; then
	ifelse([$1],,[AC_CANONICAL_HOST],[$1])
	system_name="$host_os"
else
	system_name="`(uname -s -r) 2>/dev/null`"
	if test -z "$system_name" ; then
		system_name="`(hostname) 2>/dev/null`"
	fi
fi
test -n "$system_name" && AC_DEFINE_UNQUOTED(SYSTEM_NAME,"$system_name")
AC_CACHE_VAL(cf_cv_system_name,[cf_cv_system_name="$system_name"])

test -z "$system_name" && system_name="$cf_cv_system_name"
test -n "$cf_cv_system_name" && AC_MSG_RESULT(Configuring for $cf_cv_system_name)

if test ".$system_name" != ".$cf_cv_system_name" ; then
	AC_MSG_RESULT(Cached system name ($system_name) does not agree with actual ($cf_cv_system_name))
	AC_MSG_ERROR("Please remove config.cache and try again.")
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_CURSES_HEADER version: 2 updated: 2010/04/28 06:02:16
dnl ----------------
dnl Find a "curses" header file, e.g,. "curses.h", or one of the more common
dnl variations of ncurses' installs.
dnl
dnl $1 = ncurses when looking for ncurses, or is empty
AC_DEFUN([CF_CURSES_HEADER],[
AC_CACHE_CHECK(if we have identified curses headers,cf_cv_ncurses_header,[
cf_cv_ncurses_header=none
for cf_header in ifelse($1,,,[ \
    $1/curses.h \
	$1/ncurses.h]) \
	curses.h \
	ncurses.h ifelse($1,,[ncurses/curses.h ncurses/ncurses.h])
do
AC_TRY_COMPILE([#include <${cf_header}>],
	[initscr(); tgoto("?", 0,0)],
	[cf_cv_ncurses_header=$cf_header; break],[])
done
])

if test "$cf_cv_ncurses_header" = none ; then
	AC_MSG_ERROR(No curses header-files found)
fi

# cheat, to get the right #define's for HAVE_NCURSES_H, etc.
AC_CHECK_HEADERS($cf_cv_ncurses_header)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_DIRNAME version: 4 updated: 2002/12/21 19:25:52
dnl ----------
dnl "dirname" is not portable, so we fake it with a shell script.
AC_DEFUN([CF_DIRNAME],[$1=`echo $2 | sed -e 's%/[[^/]]*$%%'`])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FIND_LIBRARY version: 9 updated: 2008/03/23 14:48:54
dnl ---------------
dnl Look for a non-standard library, given parameters for AC_TRY_LINK.  We
dnl prefer a standard location, and use -L options only if we do not find the
dnl library in the standard library location(s).
dnl	$1 = library name
dnl	$2 = library class, usually the same as library name
dnl	$3 = includes
dnl	$4 = code fragment to compile/link
dnl	$5 = corresponding function-name
dnl	$6 = flag, nonnull if failure should not cause an error-exit
dnl
dnl Sets the variable "$cf_libdir" as a side-effect, so we can see if we had
dnl to use a -L option.
AC_DEFUN([CF_FIND_LIBRARY],
[
	eval 'cf_cv_have_lib_'$1'=no'
	cf_libdir=""
	AC_CHECK_FUNC($5,
		eval 'cf_cv_have_lib_'$1'=yes',[
		cf_save_LIBS="$LIBS"
		AC_MSG_CHECKING(for $5 in -l$1)
		LIBS="-l$1 $LIBS"
		AC_TRY_LINK([$3],[$4],
			[AC_MSG_RESULT(yes)
			 eval 'cf_cv_have_lib_'$1'=yes'
			],
			[AC_MSG_RESULT(no)
			CF_LIBRARY_PATH(cf_search,$2)
			for cf_libdir in $cf_search
			do
				AC_MSG_CHECKING(for -l$1 in $cf_libdir)
				LIBS="-L$cf_libdir -l$1 $cf_save_LIBS"
				AC_TRY_LINK([$3],[$4],
					[AC_MSG_RESULT(yes)
			 		 eval 'cf_cv_have_lib_'$1'=yes'
					 break],
					[AC_MSG_RESULT(no)
					 LIBS="$cf_save_LIBS"])
			done
			])
		])
eval 'cf_found_library=[$]cf_cv_have_lib_'$1
ifelse($6,,[
if test $cf_found_library = no ; then
	AC_MSG_ERROR(Cannot link $1 library)
fi
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_FIND_LINKAGE version: 19 updated: 2010/05/29 16:31:02
dnl ---------------
dnl Find a library (specifically the linkage used in the code fragment),
dnl searching for it if it is not already in the library path.
dnl See also CF_ADD_SEARCHPATH.
dnl
dnl Parameters (4-on are optional):
dnl     $1 = headers for library entrypoint
dnl     $2 = code fragment for library entrypoint
dnl     $3 = the library name without the "-l" option or ".so" suffix.
dnl     $4 = action to perform if successful (default: update CPPFLAGS, etc)
dnl     $5 = action to perform if not successful
dnl     $6 = module name, if not the same as the library name
dnl     $7 = extra libraries
dnl
dnl Sets these variables:
dnl     $cf_cv_find_linkage_$3 - yes/no according to whether linkage is found
dnl     $cf_cv_header_path_$3 - include-directory if needed
dnl     $cf_cv_library_path_$3 - library-directory if needed
dnl     $cf_cv_library_file_$3 - library-file if needed, e.g., -l$3
AC_DEFUN([CF_FIND_LINKAGE],[

# If the linkage is not already in the $CPPFLAGS/$LDFLAGS configuration, these
# will be set on completion of the AC_TRY_LINK below.
cf_cv_header_path_$3=
cf_cv_library_path_$3=

CF_MSG_LOG([Starting [FIND_LINKAGE]($3,$6)])

cf_save_LIBS="$LIBS"

AC_TRY_LINK([$1],[$2],[
	cf_cv_find_linkage_$3=yes
	cf_cv_header_path_$3=/usr/include
	cf_cv_library_path_$3=/usr/lib
],[

LIBS="-l$3 $7 $cf_save_LIBS"

AC_TRY_LINK([$1],[$2],[
	cf_cv_find_linkage_$3=yes
	cf_cv_header_path_$3=/usr/include
	cf_cv_library_path_$3=/usr/lib
	cf_cv_library_file_$3="-l$3"
],[
	cf_cv_find_linkage_$3=no
	LIBS="$cf_save_LIBS"

    CF_VERBOSE(find linkage for $3 library)
    CF_MSG_LOG([Searching for headers in [FIND_LINKAGE]($3,$6)])

    cf_save_CPPFLAGS="$CPPFLAGS"
    cf_test_CPPFLAGS="$CPPFLAGS"

    CF_HEADER_PATH(cf_search,ifelse([$6],,[$3],[$6]))
    for cf_cv_header_path_$3 in $cf_search
    do
      if test -d $cf_cv_header_path_$3 ; then
        CF_VERBOSE(... testing $cf_cv_header_path_$3)
        CPPFLAGS="$cf_save_CPPFLAGS -I$cf_cv_header_path_$3"
        AC_TRY_COMPILE([$1],[$2],[
            CF_VERBOSE(... found $3 headers in $cf_cv_header_path_$3)
            cf_cv_find_linkage_$3=maybe
            cf_test_CPPFLAGS="$CPPFLAGS"
            break],[
            CPPFLAGS="$cf_save_CPPFLAGS"
            ])
      fi
    done

    if test "$cf_cv_find_linkage_$3" = maybe ; then

      CF_MSG_LOG([Searching for $3 library in [FIND_LINKAGE]($3,$6)])

      cf_save_LIBS="$LIBS"
      cf_save_LDFLAGS="$LDFLAGS"

      ifelse([$6],,,[
        CPPFLAGS="$cf_test_CPPFLAGS"
        LIBS="-l$3 $7 $cf_save_LIBS"
        AC_TRY_LINK([$1],[$2],[
            CF_VERBOSE(... found $3 library in system)
            cf_cv_find_linkage_$3=yes])
            CPPFLAGS="$cf_save_CPPFLAGS"
            LIBS="$cf_save_LIBS"
            ])

      if test "$cf_cv_find_linkage_$3" != yes ; then
        CF_LIBRARY_PATH(cf_search,$3)
        for cf_cv_library_path_$3 in $cf_search
        do
          if test -d $cf_cv_library_path_$3 ; then
            CF_VERBOSE(... testing $cf_cv_library_path_$3)
            CPPFLAGS="$cf_test_CPPFLAGS"
            LIBS="-l$3 $7 $cf_save_LIBS"
            LDFLAGS="$cf_save_LDFLAGS -L$cf_cv_library_path_$3"
            AC_TRY_LINK([$1],[$2],[
                CF_VERBOSE(... found $3 library in $cf_cv_library_path_$3)
                cf_cv_find_linkage_$3=yes
                cf_cv_library_file_$3="-l$3"
                break],[
                CPPFLAGS="$cf_save_CPPFLAGS"
                LIBS="$cf_save_LIBS"
                LDFLAGS="$cf_save_LDFLAGS"
                ])
          fi
        done
        CPPFLAGS="$cf_save_CPPFLAGS"
        LDFLAGS="$cf_save_LDFLAGS"
      fi

    else
      cf_cv_find_linkage_$3=no
    fi
    ],$7)
])

LIBS="$cf_save_LIBS"

if test "$cf_cv_find_linkage_$3" = yes ; then
ifelse([$4],,[
	CF_ADD_INCDIR($cf_cv_header_path_$3)
	CF_ADD_LIBDIR($cf_cv_library_path_$3)
	CF_ADD_LIB($3)
],[$4])
else
ifelse([$5],,AC_MSG_WARN(Cannot find $3 library),[$5])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_ATTRIBUTES version: 14 updated: 2010/10/23 15:52:32
dnl -----------------
dnl Test for availability of useful gcc __attribute__ directives to quiet
dnl compiler warnings.  Though useful, not all are supported -- and contrary
dnl to documentation, unrecognized directives cause older compilers to barf.
AC_DEFUN([CF_GCC_ATTRIBUTES],
[
if test "$GCC" = yes
then
cat > conftest.i <<EOF
#ifndef GCC_PRINTF
#define GCC_PRINTF 0
#endif
#ifndef GCC_SCANF
#define GCC_SCANF 0
#endif
#ifndef GCC_NORETURN
#define GCC_NORETURN /* nothing */
#endif
#ifndef GCC_UNUSED
#define GCC_UNUSED /* nothing */
#endif
EOF
if test "$GCC" = yes
then
	AC_CHECKING([for $CC __attribute__ directives])
cat > conftest.$ac_ext <<EOF
#line __oline__ "${as_me:-configure}"
#include "confdefs.h"
#include "conftest.h"
#include "conftest.i"
#if	GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var) /*nothing*/
#endif
#if	GCC_SCANF
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
#else
#define GCC_SCANFLIKE(fmt,var)  /*nothing*/
#endif
extern void wow(char *,...) GCC_SCANFLIKE(1,2);
extern void oops(char *,...) GCC_PRINTFLIKE(1,2) GCC_NORETURN;
extern void foo(void) GCC_NORETURN;
int main(int argc GCC_UNUSED, char *argv[[]] GCC_UNUSED) { return 0; }
EOF
	cf_printf_attribute=no
	cf_scanf_attribute=no
	for cf_attribute in scanf printf unused noreturn
	do
		CF_UPPER(cf_ATTRIBUTE,$cf_attribute)
		cf_directive="__attribute__(($cf_attribute))"
		echo "checking for $CC $cf_directive" 1>&AC_FD_CC

		case $cf_attribute in #(vi
		printf) #(vi
			cf_printf_attribute=yes
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE 1
EOF
			;;
		scanf) #(vi
			cf_scanf_attribute=yes
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE 1
EOF
			;;
		*) #(vi
			cat >conftest.h <<EOF
#define GCC_$cf_ATTRIBUTE $cf_directive
EOF
			;;
		esac

		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... $cf_attribute)
			cat conftest.h >>confdefs.h
			case $cf_attribute in #(vi
			printf) #(vi
				if test "$cf_printf_attribute" = no ; then
					cat >>confdefs.h <<EOF
#define GCC_PRINTFLIKE(fmt,var) /* nothing */
EOF
				else
					cat >>confdefs.h <<EOF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
EOF
				fi
				;;
			scanf) #(vi
				if test "$cf_scanf_attribute" = no ; then
					cat >>confdefs.h <<EOF
#define GCC_SCANFLIKE(fmt,var) /* nothing */
EOF
				else
					cat >>confdefs.h <<EOF
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
EOF
				fi
				;;
			esac
		fi
	done
else
	fgrep define conftest.i >>confdefs.h
fi
rm -rf conftest*
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_VERSION version: 5 updated: 2010/04/24 11:02:31
dnl --------------
dnl Find version of gcc
AC_DEFUN([CF_GCC_VERSION],[
AC_REQUIRE([AC_PROG_CC])
GCC_VERSION=none
if test "$GCC" = yes ; then
	AC_MSG_CHECKING(version of $CC)
	GCC_VERSION="`${CC} --version 2>/dev/null | sed -e '2,$d' -e 's/^.*(GCC) //' -e 's/^[[^0-9.]]*//' -e 's/[[^0-9.]].*//'`"
	test -z "$GCC_VERSION" && GCC_VERSION=unknown
	AC_MSG_RESULT($GCC_VERSION)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GCC_WARNINGS version: 27 updated: 2010/10/23 15:52:32
dnl ---------------
dnl Check if the compiler supports useful warning options.  There's a few that
dnl we don't use, simply because they're too noisy:
dnl
dnl	-Wconversion (useful in older versions of gcc, but not in gcc 2.7.x)
dnl	-Wredundant-decls (system headers make this too noisy)
dnl	-Wtraditional (combines too many unrelated messages, only a few useful)
dnl	-Wwrite-strings (too noisy, but should review occasionally).  This
dnl		is enabled for ncurses using "--enable-const".
dnl	-pedantic
dnl
dnl Parameter:
dnl	$1 is an optional list of gcc warning flags that a particular
dnl		application might want to use, e.g., "no-unused" for
dnl		-Wno-unused
dnl Special:
dnl	If $with_ext_const is "yes", add a check for -Wwrite-strings
dnl
AC_DEFUN([CF_GCC_WARNINGS],
[
AC_REQUIRE([CF_GCC_VERSION])
CF_INTEL_COMPILER(GCC,INTEL_COMPILER,CFLAGS)

cat > conftest.$ac_ext <<EOF
#line __oline__ "${as_me:-configure}"
int main(int argc, char *argv[[]]) { return (argv[[argc-1]] == 0) ; }
EOF

if test "$INTEL_COMPILER" = yes
then
# The "-wdXXX" options suppress warnings:
# remark #1419: external declaration in primary source file
# remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type (potential portability problem)
# remark #1684: conversion from pointer to same-sized integral type (potential portability problem)
# remark #193: zero used for undefined preprocessing identifier
# remark #593: variable "curs_sb_left_arrow" was set but never used
# remark #810: conversion from "int" to "Dimension={unsigned short}" may lose significant bits
# remark #869: parameter "tw" was never referenced
# remark #981: operands are evaluated in unspecified order
# warning #279: controlling expression is constant

	AC_CHECKING([for $CC warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS="-Wall"
	for cf_opt in \
		wd1419 \
		wd1683 \
		wd1684 \
		wd193 \
		wd593 \
		wd279 \
		wd810 \
		wd869 \
		wd981
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
		fi
	done
	CFLAGS="$cf_save_CFLAGS"

elif test "$GCC" = yes
then
	AC_CHECKING([for $CC warning options])
	cf_save_CFLAGS="$CFLAGS"
	EXTRA_CFLAGS=
	cf_warn_CONST=""
	test "$with_ext_const" = yes && cf_warn_CONST="Wwrite-strings"
	for cf_opt in W Wall \
		Wbad-function-cast \
		Wcast-align \
		Wcast-qual \
		Winline \
		Wmissing-declarations \
		Wmissing-prototypes \
		Wnested-externs \
		Wpointer-arith \
		Wshadow \
		Wstrict-prototypes \
		Wundef $cf_warn_CONST $1
	do
		CFLAGS="$cf_save_CFLAGS $EXTRA_CFLAGS -$cf_opt"
		if AC_TRY_EVAL(ac_compile); then
			test -n "$verbose" && AC_MSG_RESULT(... -$cf_opt)
			case $cf_opt in #(vi
			Wcast-qual) #(vi
				CPPFLAGS="$CPPFLAGS -DXTSTRINGDEFINES"
				;;
			Winline) #(vi
				case $GCC_VERSION in
				[[34]].*)
					CF_VERBOSE(feature is broken in gcc $GCC_VERSION)
					continue;;
				esac
				;;
			esac
			EXTRA_CFLAGS="$EXTRA_CFLAGS -$cf_opt"
		fi
	done
	CFLAGS="$cf_save_CFLAGS"
fi
rm -rf conftest*

AC_SUBST(EXTRA_CFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNAT_GENERICS version: 1 updated: 2010/11/13 14:15:18
dnl ----------------
AC_DEFUN([CF_GNAT_GENERICS],
[
AC_MSG_CHECKING(if GNAT supports generics)
case $cf_gnat_version in #(vi
3.[[1-9]]*|[[4-9]].*) #(vi
	cf_gnat_generics=yes
	;;
*)
	cf_gnat_generics=no
	;;
esac
AC_MSG_RESULT($cf_gnat_generics)

if test "$cf_gnat_generics" = yes
then
	cf_compile_generics=generics
	cf_generic_objects="\${GENOBJS}"
else
	cf_compile_generics=
	cf_generic_objects=
fi

AC_SUBST(cf_compile_generics)
AC_SUBST(cf_generic_objects)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNAT_PRAGMA_UNREF version: 1 updated: 2010/06/19 15:22:18
dnl --------------------
dnl Check if the gnat pragma "Unreferenced" works.
AC_DEFUN([CF_GNAT_PRAGMA_UNREF],[
AC_CACHE_CHECK(if GNAT pragma Unreferenced works,cf_cv_pragma_unreferenced,[
CF_GNAT_TRY_LINK([procedure conftest;],
[with Text_IO;
with GNAT.OS_Lib;
procedure conftest is
   test : Integer;
   pragma Unreferenced (test);
begin
   test := 1;
   Text_IO.Put ("Hello World");
   Text_IO.New_Line;
   GNAT.OS_Lib.OS_Exit (0);
end conftest;],
	[cf_cv_pragma_unreferenced=yes],
	[cf_cv_pragma_unreferenced=no])])

# if the pragma is supported, use it (needed in the Trace code).
if test $cf_cv_pragma_unreferenced = yes ; then
	PRAGMA_UNREF=TRUE
else
	PRAGMA_UNREF=FALSE
fi
AC_SUBST(PRAGMA_UNREF)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNAT_PROJECTS version: 1 updated: 2010/11/13 14:15:18
dnl ----------------
AC_DEFUN([CF_GNAT_PROJECTS],
[
AC_MSG_CHECKING(if GNAT supports project files)
case $cf_gnat_version in #(vi
3.[[0-9]]*) #(vi
	cf_gnat_projects=no
	;;
*)
	case $cf_cv_system_name in #(vi
	cygwin*) #(vi
		cf_gnat_projects=no
		;;
	*)
		cf_gnat_projects=yes
		;;
	esac
	;;
esac
AC_MSG_RESULT($cf_gnat_projects)

if test "$cf_gnat_projects" = yes
then
	USE_OLD_MAKERULES="#"
	USE_GNAT_PROJECTS=""
else
	USE_OLD_MAKERULES=""
	USE_GNAT_PROJECTS="#"
fi

AC_SUBST(USE_OLD_MAKERULES)
AC_SUBST(USE_GNAT_PROJECTS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNAT_TRY_LINK version: 2 updated: 2010/08/14 18:25:37
dnl ----------------
dnl Verify that a test program compiles/links with GNAT.
dnl $cf_ada_make is set to the program that compiles/links
dnl $ADAFLAGS may be set to the GNAT flags.
dnl
dnl $1 is the text of the spec
dnl $2 is the text of the body
dnl $3 is the shell command to execute if successful
dnl $4 is the shell command to execute if not successful
AC_DEFUN([CF_GNAT_TRY_LINK],
[
rm -rf conftest*
cat >>conftest.ads <<CF_EOF
$1
CF_EOF
cat >>conftest.adb <<CF_EOF
$2
CF_EOF
if ( $cf_ada_make $ADAFLAGS conftest 1>&AC_FD_CC 2>&1 ) ; then
ifelse($3,,      :,[      $3])
ifelse($4,,,[else
   $4])
fi
rm -rf conftest*
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNAT_TRY_RUN version: 4 updated: 2010/08/14 18:25:37
dnl ---------------
dnl Verify that a test program compiles and runs with GNAT
dnl $cf_ada_make is set to the program that compiles/links
dnl $ADAFLAGS may be set to the GNAT flags.
dnl
dnl $1 is the text of the spec
dnl $2 is the text of the body
dnl $3 is the shell command to execute if successful
dnl $4 is the shell command to execute if not successful
AC_DEFUN([CF_GNAT_TRY_RUN],
[
rm -rf conftest*
cat >>conftest.ads <<CF_EOF
$1
CF_EOF
cat >>conftest.adb <<CF_EOF
$2
CF_EOF
if ( $cf_ada_make $ADAFLAGS conftest 1>&AC_FD_CC 2>&1 ) ; then
   if ( ./conftest 1>&AC_FD_CC 2>&1 ) ; then
ifelse($3,,      :,[      $3])
ifelse($4,,,[   else
      $4])
   fi
ifelse($4,,,[else
   $4])
fi
rm -rf conftest*
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_GNAT_VERSION version: 16 updated: 2010/11/13 14:15:18
dnl ---------------
dnl Verify version of GNAT.
AC_DEFUN([CF_GNAT_VERSION],
[
AC_MSG_CHECKING(for gnat version)
cf_gnat_version=`${cf_ada_make:-gnatmake} -v 2>&1 | \
	grep '[[0-9]].[[0-9]][[0-9]]*' |\
    sed -e '2,$d' -e 's/[[^0-9 \.]]//g' -e 's/^[[ ]]*//' -e 's/ .*//'`
AC_MSG_RESULT($cf_gnat_version)

case $cf_gnat_version in #(vi
3.1[[1-9]]*|3.[[2-9]]*|[[4-9]].*) #(vi
	cf_cv_prog_gnat_correct=yes
	;;
*)
	AC_MSG_WARN(Unsupported GNAT version $cf_gnat_version. We require 3.11 or better. Disabling Ada95 binding.)
	cf_cv_prog_gnat_correct=no
	;;
esac

CF_GNAT_GENERICS
CF_GNAT_PROJECTS
])
dnl ---------------------------------------------------------------------------
dnl CF_GNU_SOURCE version: 6 updated: 2005/07/09 13:23:07
dnl -------------
dnl Check if we must define _GNU_SOURCE to get a reasonable value for
dnl _XOPEN_SOURCE, upon which many POSIX definitions depend.  This is a defect
dnl (or misfeature) of glibc2, which breaks portability of many applications,
dnl since it is interwoven with GNU extensions.
dnl
dnl Well, yes we could work around it...
AC_DEFUN([CF_GNU_SOURCE],
[
AC_CACHE_CHECK(if we must define _GNU_SOURCE,cf_cv_gnu_source,[
AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_gnu_source=no],
	[cf_save="$CPPFLAGS"
	 CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_gnu_source=no],
	[cf_cv_gnu_source=yes])
	CPPFLAGS="$cf_save"
	])
])
test "$cf_cv_gnu_source" = yes && CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HEADER_PATH version: 12 updated: 2010/05/05 05:22:40
dnl --------------
dnl Construct a search-list of directories for a nonstandard header-file
dnl
dnl Parameters
dnl	$1 = the variable to return as result
dnl	$2 = the package name
AC_DEFUN([CF_HEADER_PATH],
[
$1=

# collect the current set of include-directories from compiler flags
cf_header_path_list=""
if test -n "${CFLAGS}${CPPFLAGS}" ; then
	for cf_header_path in $CPPFLAGS $CFLAGS
	do
		case $cf_header_path in #(vi
		-I*)
			cf_header_path=`echo ".$cf_header_path" |sed -e 's/^...//' -e 's,/include$,,'`
			CF_ADD_SUBDIR_PATH($1,$2,include,$cf_header_path,NONE)
			cf_header_path_list="$cf_header_path_list [$]$1"
			;;
		esac
	done
fi

# add the variations for the package we are looking for
CF_SUBDIR_PATH($1,$2,include)

test "$includedir" != NONE && \
test "$includedir" != "/usr/include" && \
test -d "$includedir" && {
	test -d $includedir &&    $1="[$]$1 $includedir"
	test -d $includedir/$2 && $1="[$]$1 $includedir/$2"
}

test "$oldincludedir" != NONE && \
test "$oldincludedir" != "/usr/include" && \
test -d "$oldincludedir" && {
	test -d $oldincludedir    && $1="[$]$1 $oldincludedir"
	test -d $oldincludedir/$2 && $1="[$]$1 $oldincludedir/$2"
}

$1="[$]$1 $cf_header_path_list"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_HELP_MESSAGE version: 3 updated: 1998/01/14 10:56:23
dnl ---------------
dnl Insert text into the help-message, for readability, from AC_ARG_WITH.
AC_DEFUN([CF_HELP_MESSAGE],
[AC_DIVERT_HELP([$1])dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INCLUDE_DIRS version: 6 updated: 2009/01/06 19:37:40
dnl ---------------
dnl Construct the list of include-options according to whether we're building
dnl in the source directory or using '--srcdir=DIR' option.  If we're building
dnl with gcc, don't append the includedir if it happens to be /usr/include,
dnl since that usually breaks gcc's shadow-includes.
AC_DEFUN([CF_INCLUDE_DIRS],
[
CPPFLAGS="$CPPFLAGS -I. -I../include"
if test "$srcdir" != "."; then
	CPPFLAGS="$CPPFLAGS -I\${srcdir}/../include"
fi
if test "$GCC" != yes; then
	CPPFLAGS="$CPPFLAGS -I\${includedir}"
elif test "$includedir" != "/usr/include"; then
	if test "$includedir" = '${prefix}/include' ; then
		if test $prefix != /usr ; then
			CPPFLAGS="$CPPFLAGS -I\${includedir}"
		fi
	else
		CPPFLAGS="$CPPFLAGS -I\${includedir}"
	fi
fi
AC_SUBST(CPPFLAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_INTEL_COMPILER version: 4 updated: 2010/05/26 05:38:42
dnl -----------------
dnl Check if the given compiler is really the Intel compiler for Linux.  It
dnl tries to imitate gcc, but does not return an error when it finds a mismatch
dnl between prototypes, e.g., as exercised by CF_MISSING_CHECK.
dnl
dnl This macro should be run "soon" after AC_PROG_CC or AC_PROG_CPLUSPLUS, to
dnl ensure that it is not mistaken for gcc/g++.  It is normally invoked from
dnl the wrappers for gcc and g++ warnings.
dnl
dnl $1 = GCC (default) or GXX
dnl $2 = INTEL_COMPILER (default) or INTEL_CPLUSPLUS
dnl $3 = CFLAGS (default) or CXXFLAGS
AC_DEFUN([CF_INTEL_COMPILER],[
ifelse([$2],,INTEL_COMPILER,[$2])=no

if test "$ifelse([$1],,[$1],GCC)" = yes ; then
	case $host_os in
	linux*|gnu*)
		AC_MSG_CHECKING(if this is really Intel ifelse([$1],GXX,C++,C) compiler)
		cf_save_CFLAGS="$ifelse([$3],,CFLAGS,[$3])"
		ifelse([$3],,CFLAGS,[$3])="$ifelse([$3],,CFLAGS,[$3]) -no-gcc"
		AC_TRY_COMPILE([],[
#ifdef __INTEL_COMPILER
#else
make an error
#endif
],[ifelse([$2],,INTEL_COMPILER,[$2])=yes
cf_save_CFLAGS="$cf_save_CFLAGS -we147 -no-gcc"
],[])
		ifelse([$3],,CFLAGS,[$3])="$cf_save_CFLAGS"
		AC_MSG_RESULT($ifelse([$2],,INTEL_COMPILER,[$2]))
		;;
	esac
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LARGEFILE version: 7 updated: 2007/06/02 11:58:50
dnl ------------
dnl Add checks for large file support.
AC_DEFUN([CF_LARGEFILE],[
ifdef([AC_FUNC_FSEEKO],[
    AC_SYS_LARGEFILE
    if test "$enable_largefile" != no ; then
	AC_FUNC_FSEEKO

	# Normally we would collect these definitions in the config.h,
	# but (like _XOPEN_SOURCE), some environments rely on having these
	# defined before any of the system headers are included.  Another
	# case comes up with C++, e.g., on AIX the compiler compiles the
	# header files by themselves before looking at the body files it is
	# told to compile.  For ncurses, those header files do not include
	# the config.h
	test "$ac_cv_sys_large_files"      != no && CPPFLAGS="$CPPFLAGS -D_LARGE_FILES "
	test "$ac_cv_sys_largefile_source" != no && CPPFLAGS="$CPPFLAGS -D_LARGEFILE_SOURCE "
	test "$ac_cv_sys_file_offset_bits" != no && CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits "

	AC_CACHE_CHECK(whether to use struct dirent64, cf_cv_struct_dirent64,[
		AC_TRY_COMPILE([
#include <sys/types.h>
#include <dirent.h>
		],[
		/* if transitional largefile support is setup, this is true */
		extern struct dirent64 * readdir(DIR *);
		struct dirent64 *x = readdir((DIR *)0);
		struct dirent *y = readdir((DIR *)0);
		int z = x - y;
		],
		[cf_cv_struct_dirent64=yes],
		[cf_cv_struct_dirent64=no])
	])
	test "$cf_cv_struct_dirent64" = yes && AC_DEFINE(HAVE_STRUCT_DIRENT64)
    fi
])
])
dnl ---------------------------------------------------------------------------
dnl CF_LD_RPATH_OPT version: 3 updated: 2010/06/02 05:03:05
dnl ---------------
dnl For the given system and compiler, find the compiler flags to pass to the
dnl loader to use the "rpath" feature.
AC_DEFUN([CF_LD_RPATH_OPT],
[
AC_REQUIRE([CF_CHECK_CACHE])

LD_RPATH_OPT=
AC_MSG_CHECKING(for an rpath option)
case $cf_cv_system_name in #(vi
irix*) #(vi
	if test "$GCC" = yes; then
		LD_RPATH_OPT="-Wl,-rpath,"
	else
		LD_RPATH_OPT="-rpath "
	fi
	;;
linux*|gnu*|k*bsd*-gnu) #(vi
	LD_RPATH_OPT="-Wl,-rpath,"
	;;
openbsd[[2-9]].*) #(vi
	LD_RPATH_OPT="-Wl,-rpath,"
	;;
freebsd*) #(vi
	LD_RPATH_OPT="-rpath "
	;;
netbsd*) #(vi
	LD_RPATH_OPT="-Wl,-rpath,"
	;;
osf*|mls+*) #(vi
	LD_RPATH_OPT="-rpath "
	;;
solaris2*) #(vi
	LD_RPATH_OPT="-R"
	;;
*)
	;;
esac
AC_MSG_RESULT($LD_RPATH_OPT)

case "x$LD_RPATH_OPT" in #(vi
x-R*)
	AC_MSG_CHECKING(if we need a space after rpath option)
	cf_save_LIBS="$LIBS"
	CF_ADD_LIBS(${LD_RPATH_OPT}$libdir)
	AC_TRY_LINK(, , cf_rpath_space=no, cf_rpath_space=yes)
	LIBS="$cf_save_LIBS"
	AC_MSG_RESULT($cf_rpath_space)
	test "$cf_rpath_space" = yes && LD_RPATH_OPT="$LD_RPATH_OPT "
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LIBRARY_PATH version: 9 updated: 2010/03/28 12:52:50
dnl ---------------
dnl Construct a search-list of directories for a nonstandard library-file
dnl
dnl Parameters
dnl	$1 = the variable to return as result
dnl	$2 = the package name
AC_DEFUN([CF_LIBRARY_PATH],
[
$1=
cf_library_path_list=""
if test -n "${LDFLAGS}${LIBS}" ; then
	for cf_library_path in $LDFLAGS $LIBS
	do
		case $cf_library_path in #(vi
		-L*)
			cf_library_path=`echo ".$cf_library_path" |sed -e 's/^...//' -e 's,/lib$,,'`
			CF_ADD_SUBDIR_PATH($1,$2,lib,$cf_library_path,NONE)
			cf_library_path_list="$cf_library_path_list [$]$1"
			;;
		esac
	done
fi

CF_SUBDIR_PATH($1,$2,lib)

$1="$cf_library_path_list [$]$1"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LIB_PREFIX version: 8 updated: 2008/09/13 11:34:16
dnl -------------
dnl Compute the library-prefix for the given host system
dnl $1 = variable to set
AC_DEFUN([CF_LIB_PREFIX],
[
	case $cf_cv_system_name in #(vi
	OS/2*|os2*) #(vi
        LIB_PREFIX=''
        ;;
	*)	LIB_PREFIX='lib'
        ;;
	esac
ifelse($1,,,[$1=$LIB_PREFIX])
	AC_SUBST(LIB_PREFIX)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LIB_SONAME version: 5 updated: 2010/08/14 18:25:37
dnl -------------
dnl Find the and soname for the given shared library.  Set the cache variable
dnl cf_cv_$3_soname to this, unless it is not found.  Then set the cache
dnl variable to "unknown".
dnl
dnl $1 = headers
dnl $2 = code
dnl $3 = library name
AC_DEFUN([CF_LIB_SONAME],
[
AC_CACHE_CHECK(for soname of $3 library,cf_cv_$3_soname,[

cf_cv_$3_soname=unknown
if test "$cross_compiling" != yes ; then
cat >conftest.$ac_ext <<CF_EOF
$1
int main()
{
$2
	${cf_cv_main_return:-return}(0);
}
CF_EOF
cf_save_LIBS="$LIBS"
	CF_ADD_LIB($3)
	if AC_TRY_EVAL(ac_compile) ; then
		if AC_TRY_EVAL(ac_link) ; then
			cf_cv_$3_soname=`ldd conftest$ac_exeext 2>/dev/null | sed -e 's,^.*/,,' -e 's, .*$,,' | fgrep lib$3.`
			test -z "$cf_cv_$3_soname" && cf_cv_$3_soname=unknown
		fi
	fi
rm -rf conftest*
LIBS="$cf_save_LIBS"
fi
])
])
dnl ---------------------------------------------------------------------------
dnl CF_LIB_SUFFIX version: 16 updated: 2008/12/27 12:30:03
dnl -------------
dnl Compute the library file-suffix from the given model name
dnl $1 = model name
dnl $2 = variable to set (the nominal library suffix)
dnl $3 = dependency variable to set (actual filename)
dnl The variable $LIB_SUFFIX, if set, prepends the variable to set.
AC_DEFUN([CF_LIB_SUFFIX],
[
	AC_REQUIRE([CF_SUBST_NCURSES_VERSION])
	case $1 in #(vi
	libtool) #(vi
		$2='.la'
		$3=[$]$2
		;;
	normal) #(vi
		$2='.a'
		$3=[$]$2
		;;
	debug) #(vi
		$2='_g.a'
		$3=[$]$2
		;;
	profile) #(vi
		$2='_p.a'
		$3=[$]$2
		;;
	shared) #(vi
		case $cf_cv_system_name in
		aix[[56]]*) #(vi
			$2='.a'
			$3=[$]$2
			;;
		cygwin*) #(vi
			$2='.dll'
			$3='.dll.a'
			;;
		darwin*) #(vi
			$2='.dylib'
			$3=[$]$2
			;;
		hpux*) #(vi
			case $target in
			ia64*) #(vi
				$2='.so'
				$3=[$]$2
				;;
			*) #(vi
				$2='.sl'
				$3=[$]$2
				;;
			esac
			;;
		*)	$2='.so'
			$3=[$]$2
			;;
		esac
	esac
	test -n "$LIB_SUFFIX" && $2="${LIB_SUFFIX}[$]{$2}"
	test -n "$LIB_SUFFIX" && $3="${LIB_SUFFIX}[$]{$3}"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LIB_TYPE version: 4 updated: 2000/10/20 22:57:49
dnl -----------
dnl Compute the string to append to -library from the given model name
dnl $1 = model name
dnl $2 = variable to set
dnl The variable $LIB_SUFFIX, if set, prepends the variable to set.
AC_DEFUN([CF_LIB_TYPE],
[
	case $1 in
	libtool) $2=''   ;;
	normal)  $2=''   ;;
	debug)   $2='_g' ;;
	profile) $2='_p' ;;
	shared)  $2=''   ;;
	esac
	test -n "$LIB_SUFFIX" && $2="${LIB_SUFFIX}[$]{$2}"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LINK_DATAONLY version: 9 updated: 2009/01/01 20:21:38
dnl ----------------
dnl Some systems have a non-ANSI linker that doesn't pull in modules that have
dnl only data (i.e., no functions), for example NeXT.  On those systems we'll
dnl have to provide wrappers for global tables to ensure they're linked
dnl properly.
AC_DEFUN([CF_LINK_DATAONLY],
[
AC_MSG_CHECKING([if data-only library module links])
AC_CACHE_VAL(cf_cv_link_dataonly,[
	rm -f conftest.a
	cat >conftest.$ac_ext <<EOF
#line __oline__ "configure"
int	testdata[[3]] = { 123, 456, 789 };
EOF
	if AC_TRY_EVAL(ac_compile) ; then
		mv conftest.o data.o && \
		( $AR $ARFLAGS conftest.a data.o ) 2>&AC_FD_CC 1>/dev/null
	fi
	rm -f conftest.$ac_ext data.o
	cat >conftest.$ac_ext <<EOF
#line __oline__ "configure"
int	testfunc()
{
#if defined(NeXT)
	${cf_cv_main_return:-return}(1);	/* I'm told this linker is broken */
#else
	extern int testdata[[3]];
	return testdata[[0]] == 123
	   &&  testdata[[1]] == 456
	   &&  testdata[[2]] == 789;
#endif
}
EOF
	if AC_TRY_EVAL(ac_compile); then
		mv conftest.o func.o && \
		( $AR $ARFLAGS conftest.a func.o ) 2>&AC_FD_CC 1>/dev/null
	fi
	rm -f conftest.$ac_ext func.o
	( eval $RANLIB conftest.a ) 2>&AC_FD_CC >/dev/null
	cf_saveLIBS="$LIBS"
	LIBS="conftest.a $LIBS"
	AC_TRY_RUN([
	int main()
	{
		extern int testfunc();
		${cf_cv_main_return:-return} (!testfunc());
	}
	],
	[cf_cv_link_dataonly=yes],
	[cf_cv_link_dataonly=no],
	[cf_cv_link_dataonly=unknown])
	LIBS="$cf_saveLIBS"
	])
AC_MSG_RESULT($cf_cv_link_dataonly)

if test "$cf_cv_link_dataonly" = no ; then
	AC_DEFINE(BROKEN_LINKER)
	BROKEN_LINKER=1
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_LINK_FUNCS version: 7 updated: 2006/12/16 12:33:30
dnl -------------
dnl Most Unix systems have both link and symlink, a few don't have symlink.
dnl A few non-Unix systems implement symlink, but not link.
dnl A few non-systems implement neither (or have nonfunctional versions).
AC_DEFUN([CF_LINK_FUNCS],
[
AC_CHECK_FUNCS( \
	remove \
	unlink )

if test "$cross_compiling" = yes ; then
	AC_CHECK_FUNCS( \
		link \
		symlink )
else
	AC_CACHE_CHECK(if link/symlink functions work,cf_cv_link_funcs,[
		cf_cv_link_funcs=
		for cf_func in link symlink ; do
			AC_TRY_RUN([
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
int main()
{
	int fail = 0;
	char *src = "config.log";
	char *dst = "conftest.chk";
	struct stat src_sb;
	struct stat dst_sb;

	stat(src, &src_sb);
	fail = ($cf_func("config.log", "conftest.chk") < 0)
	    || (stat(dst, &dst_sb) < 0)
	    || (dst_sb.st_mtime != src_sb.st_mtime);
#ifdef HAVE_UNLINK
	unlink(dst);
#else
	remove(dst);
#endif
	${cf_cv_main_return:-return} (fail);
}
			],[
			cf_cv_link_funcs="$cf_cv_link_funcs $cf_func"
			eval 'ac_cv_func_'$cf_func'=yes'],[
			eval 'ac_cv_func_'$cf_func'=no'],[
			eval 'ac_cv_func_'$cf_func'=error'])
		done
		test -z "$cf_cv_link_funcs" && cf_cv_link_funcs=no
	])
	test "$ac_cv_func_link"    = yes && AC_DEFINE(HAVE_LINK)
	test "$ac_cv_func_symlink" = yes && AC_DEFINE(HAVE_SYMLINK)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAIN_RETURN version: 1 updated: 2006/12/10 09:51:54
dnl --------------
dnl Check if a return from main to the shell actually returns the same exit
dnl code.  This is true for almost any POSIX environment.
dnl
dnl Some very old environments did not flush stdout, etc., on an exit.  That
dnl would be a useful case to test for also.
AC_DEFUN([CF_MAIN_RETURN],
[
cf_cv_main_return=return
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAKEFLAGS version: 13 updated: 2010/10/23 15:52:32
dnl ------------
dnl Some 'make' programs support ${MAKEFLAGS}, some ${MFLAGS}, to pass 'make'
dnl options to lower-levels.  It's very useful for "make -n" -- if we have it.
dnl (GNU 'make' does both, something POSIX 'make', which happens to make the
dnl ${MAKEFLAGS} variable incompatible because it adds the assignments :-)
AC_DEFUN([CF_MAKEFLAGS],
[
AC_CACHE_CHECK(for makeflags variable, cf_cv_makeflags,[
	cf_cv_makeflags=''
	for cf_option in '-${MAKEFLAGS}' '${MFLAGS}'
	do
		cat >cf_makeflags.tmp <<CF_EOF
SHELL = /bin/sh
all :
	@ echo '.$cf_option'
CF_EOF
		cf_result=`${MAKE:-make} -k -f cf_makeflags.tmp 2>/dev/null | sed -e 's,[[ 	]]*$,,'`
		case "$cf_result" in
		.*k)
			cf_result=`${MAKE:-make} -k -f cf_makeflags.tmp CC=cc 2>/dev/null`
			case "$cf_result" in
			.*CC=*)	cf_cv_makeflags=
				;;
			*)	cf_cv_makeflags=$cf_option
				;;
			esac
			break
			;;
		.-)	;;
		*)	echo "given option \"$cf_option\", no match \"$cf_result\""
			;;
		esac
	done
	rm -f cf_makeflags.tmp
])

AC_SUBST(cf_cv_makeflags)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MAKE_TAGS version: 6 updated: 2010/10/23 15:52:32
dnl ------------
dnl Generate tags/TAGS targets for makefiles.  Do not generate TAGS if we have
dnl a monocase filesystem.
AC_DEFUN([CF_MAKE_TAGS],[
AC_REQUIRE([CF_MIXEDCASE_FILENAMES])

AC_CHECK_PROGS(CTAGS, exctags ctags)
AC_CHECK_PROGS(ETAGS, exetags etags)

AC_CHECK_PROG(MAKE_LOWER_TAGS, ${CTAGS:-ctags}, yes, no)

if test "$cf_cv_mixedcase" = yes ; then
	AC_CHECK_PROG(MAKE_UPPER_TAGS, ${ETAGS:-etags}, yes, no)
else
	MAKE_UPPER_TAGS=no
fi

if test "$MAKE_UPPER_TAGS" = yes ; then
	MAKE_UPPER_TAGS=
else
	MAKE_UPPER_TAGS="#"
fi

if test "$MAKE_LOWER_TAGS" = yes ; then
	MAKE_LOWER_TAGS=
else
	MAKE_LOWER_TAGS="#"
fi

AC_SUBST(CTAGS)
AC_SUBST(ETAGS)

AC_SUBST(MAKE_UPPER_TAGS)
AC_SUBST(MAKE_LOWER_TAGS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MIXEDCASE_FILENAMES version: 3 updated: 2003/09/20 17:07:55
dnl ----------------------
dnl Check if the file-system supports mixed-case filenames.  If we're able to
dnl create a lowercase name and see it as uppercase, it doesn't support that.
AC_DEFUN([CF_MIXEDCASE_FILENAMES],
[
AC_CACHE_CHECK(if filesystem supports mixed-case filenames,cf_cv_mixedcase,[
if test "$cross_compiling" = yes ; then
	case $target_alias in #(vi
	*-os2-emx*|*-msdosdjgpp*|*-cygwin*|*-mingw32*|*-uwin*) #(vi
		cf_cv_mixedcase=no
		;;
	*)
		cf_cv_mixedcase=yes
		;;
	esac
else
	rm -f conftest CONFTEST
	echo test >conftest
	if test -f CONFTEST ; then
		cf_cv_mixedcase=no
	else
		cf_cv_mixedcase=yes
	fi
	rm -f conftest CONFTEST
fi
])
test "$cf_cv_mixedcase" = yes && AC_DEFINE(MIXEDCASE_FILENAMES)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MKSTEMP version: 7 updated: 2010/08/14 18:25:37
dnl ----------
dnl Check for a working mkstemp.  This creates two files, checks that they are
dnl successfully created and distinct (AmigaOS apparently fails on the last).
AC_DEFUN([CF_MKSTEMP],[
AC_CACHE_CHECK(for working mkstemp, cf_cv_func_mkstemp,[
rm -rf conftest*
AC_TRY_RUN([
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
int main()
{
	char *tmpl = "conftestXXXXXX";
	char name[2][80];
	int n;
	int result = 0;
	int fd;
	struct stat sb;

	umask(077);
	for (n = 0; n < 2; ++n) {
		strcpy(name[n], tmpl);
		if ((fd = mkstemp(name[n])) >= 0) {
			if (!strcmp(name[n], tmpl)
			 || stat(name[n], &sb) != 0
			 || (sb.st_mode & S_IFMT) != S_IFREG
			 || (sb.st_mode & 077) != 0) {
				result = 1;
			}
			close(fd);
		}
	}
	if (result == 0
	 && !strcmp(name[0], name[1]))
		result = 1;
	${cf_cv_main_return:-return}(result);
}
],[cf_cv_func_mkstemp=yes
],[cf_cv_func_mkstemp=no
],[AC_CHECK_FUNC(mkstemp)
])
])
if test "x$cf_cv_func_mkstemp" = xyes || test "x$ac_cv_func_mkstemp" = xyes ; then
	AC_DEFINE(HAVE_MKSTEMP)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_MSG_LOG version: 5 updated: 2010/10/23 15:52:32
dnl ----------
dnl Write a debug message to config.log, along with the line number in the
dnl configure script.
AC_DEFUN([CF_MSG_LOG],[
echo "${as_me:-configure}:__oline__: testing $* ..." 1>&AC_FD_CC
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_ADDON version: 3 updated: 2010/10/23 15:54:49
dnl ----------------
dnl Configure an ncurses add-on, built outside the ncurses tree.
AC_DEFUN([CF_NCURSES_ADDON],[

AC_PROVIDE([CF_SUBST_NCURSES_VERSION])

AC_MSG_CHECKING(if you want wide-character code)
AC_ARG_ENABLE(widec,
	[  --enable-widec          compile with wide-char/UTF-8 code],
	[with_widec=$enableval],
	[with_widec=no])
AC_MSG_RESULT($with_widec)
if test "$with_widec" = yes ; then
	CF_UTF8_LIB 
	CF_NCURSES_CONFIG(ncursesw)
else
	CF_NCURSES_CONFIG(ncurses)
fi

if test "$NCURSES_CONFIG" != none ; then

cf_version=`$NCURSES_CONFIG --version`

NCURSES_MAJOR=`echo "$cf_version" | sed -e 's/\..*//'`
NCURSES_MINOR=`echo "$cf_version" | sed -e 's/^[[0-9]]\+\.//' -e 's/\..*//'`
NCURSES_PATCH=`echo "$cf_version" | sed -e 's/^[[0-9]]\+\.[[0-9]]\+\.//'`

# ABI version is not available from headers
cf_cv_abi_version=`$NCURSES_CONFIG --abi-version`

else

for cf_name in MAJOR MINOR PATCH
do
cat >conftest.$ac_ext <<CF_EOF
#include <${cf_cv_ncurses_header:-curses.h}>
AUTOCONF_$cf_name NCURSES_VERSION_$cf_name
CF_EOF
	cf_try="$ac_cpp conftest.$ac_ext 2>&5 | fgrep AUTOCONF_$cf_name >conftest.out"
	AC_TRY_EVAL(cf_try)
	if test -f conftest.out ; then
		cf_result=`cat conftest.out | sed -e "s/^.*AUTOCONF_$cf_name[[ 	]]\+//"`
		eval NCURSES_$cf_name=$cf_result
		cat conftest.$ac_ext
		cat conftest.out
	fi
done

cf_cv_abi_version=${NCURSES_MAJOR}

fi

cf_cv_rel_version=${NCURSES_MAJOR}.${NCURSES_MINOR}

dnl Show the computed version, for logging
cf_cv_timestamp=`date`

AC_MSG_RESULT(Configuring NCURSES $cf_cv_rel_version ABI $cf_cv_abi_version ($cf_cv_timestamp))

dnl We need these values in the generated headers
AC_SUBST(NCURSES_MAJOR)
AC_SUBST(NCURSES_MINOR)
AC_SUBST(NCURSES_PATCH)

dnl We need these values in the generated makefiles
AC_SUBST(cf_cv_rel_version)
AC_SUBST(cf_cv_abi_version)

dnl FIXME - not needed for Ada95
AC_SUBST(cf_cv_builtin_bool)
AC_SUBST(cf_cv_header_stdbool_h)
AC_SUBST(cf_cv_type_of_bool)dnl

])
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CC_CHECK version: 4 updated: 2007/07/29 10:39:05
dnl -------------------
dnl Check if we can compile with ncurses' header file
dnl $1 is the cache variable to set
dnl $2 is the header-file to include
dnl $3 is the root name (ncurses or ncursesw)
AC_DEFUN([CF_NCURSES_CC_CHECK],[
	AC_TRY_COMPILE([
]ifelse($3,ncursesw,[
#define _XOPEN_SOURCE_EXTENDED
#undef  HAVE_LIBUTF8_H	/* in case we used CF_UTF8_LIB */
#define HAVE_LIBUTF8_H	/* to force ncurses' header file to use cchar_t */
])[
#include <$2>],[
#ifdef NCURSES_VERSION
]ifelse($3,ncursesw,[
#ifndef WACS_BSSB
	make an error
#endif
])[
printf("%s\n", NCURSES_VERSION);
#else
#ifdef __NCURSES_H
printf("old\n");
#else
	make an error
#endif
#endif
	]
	,[$1=$2]
	,[$1=no])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CONFIG version: 8 updated: 2010/07/08 05:17:30
dnl -----------------
dnl Tie together the configure-script macros for ncurses.
dnl Prefer the "-config" script from ncurses 6.x, to simplify analysis.
dnl Allow that to be overridden using the $NCURSES_CONFIG environment variable.
dnl
dnl $1 is the root library name (default: "ncurses")
AC_DEFUN([CF_NCURSES_CONFIG],
[
cf_ncuconfig_root=ifelse($1,,ncurses,$1)

echo "Looking for ${cf_ncuconfig_root}-config"
AC_PATH_PROGS(NCURSES_CONFIG,${cf_ncuconfig_root}6-config ${cf_ncuconfig_root}5-config,none)

if test "$NCURSES_CONFIG" != none ; then

CPPFLAGS="$CPPFLAGS `$NCURSES_CONFIG --cflags`"
CF_ADD_LIBS(`$NCURSES_CONFIG --libs`)

# even with config script, some packages use no-override for curses.h
CF_CURSES_HEADER(ifelse($1,,ncurses,$1))

dnl like CF_NCURSES_CPPFLAGS
AC_DEFINE(NCURSES)

dnl like CF_NCURSES_LIBS
CF_UPPER(cf_nculib_ROOT,HAVE_LIB$cf_ncuconfig_root)
AC_DEFINE_UNQUOTED($cf_nculib_ROOT)

dnl like CF_NCURSES_VERSION
cf_cv_ncurses_version=`$NCURSES_CONFIG --version`

else

CF_NCURSES_CPPFLAGS(ifelse($1,,ncurses,$1))
CF_NCURSES_LIBS(ifelse($1,,ncurses,$1))

fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_CPPFLAGS version: 20 updated: 2010/11/20 17:02:38
dnl -------------------
dnl Look for the SVr4 curses clone 'ncurses' in the standard places, adjusting
dnl the CPPFLAGS variable so we can include its header.
dnl
dnl The header files may be installed as either curses.h, or ncurses.h (would
dnl be obsolete, except that some packagers prefer this name to distinguish it
dnl from a "native" curses implementation).  If not installed for overwrite,
dnl the curses.h file would be in an ncurses subdirectory (e.g.,
dnl /usr/include/ncurses), but someone may have installed overwriting the
dnl vendor's curses.  Only very old versions (pre-1.9.2d, the first autoconf'd
dnl version) of ncurses don't define either __NCURSES_H or NCURSES_VERSION in
dnl the header.
dnl
dnl If the installer has set $CFLAGS or $CPPFLAGS so that the ncurses header
dnl is already in the include-path, don't even bother with this, since we cannot
dnl easily determine which file it is.  In this case, it has to be <curses.h>.
dnl
dnl The optional parameter gives the root name of the library, in case it is
dnl not installed as the default curses library.  That is how the
dnl wide-character version of ncurses is installed.
AC_DEFUN([CF_NCURSES_CPPFLAGS],
[AC_REQUIRE([CF_WITH_CURSES_DIR])

AC_PROVIDE([CF_CURSES_CPPFLAGS])dnl
cf_ncuhdr_root=ifelse($1,,ncurses,$1)

test -n "$cf_cv_curses_dir" && \
test "$cf_cv_curses_dir" != "no" && { \
  CF_ADD_INCDIR($cf_cv_curses_dir/include/$cf_ncuhdr_root)
}

AC_CACHE_CHECK(for $cf_ncuhdr_root header in include-path, cf_cv_ncurses_h,[
	cf_header_list="$cf_ncuhdr_root/curses.h $cf_ncuhdr_root/ncurses.h"
	( test "$cf_ncuhdr_root" = ncurses || test "$cf_ncuhdr_root" = ncursesw ) && cf_header_list="$cf_header_list curses.h ncurses.h"
	for cf_header in $cf_header_list
	do
		CF_NCURSES_CC_CHECK(cf_cv_ncurses_h,$cf_header,$1)
		test "$cf_cv_ncurses_h" != no && break
	done
])

CF_NCURSES_HEADER
CF_TERM_HEADER

# some applications need this, but should check for NCURSES_VERSION
AC_DEFINE(NCURSES)

CF_NCURSES_VERSION
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_HEADER version: 2 updated: 2008/03/23 14:48:54
dnl -----------------
dnl Find a "curses" header file, e.g,. "curses.h", or one of the more common
dnl variations of ncurses' installs.
dnl
dnl See also CF_CURSES_HEADER, which sets the same cache variable.
AC_DEFUN([CF_NCURSES_HEADER],[

if test "$cf_cv_ncurses_h" != no ; then
	cf_cv_ncurses_header=$cf_cv_ncurses_h
else

AC_CACHE_CHECK(for $cf_ncuhdr_root include-path, cf_cv_ncurses_h2,[
	test -n "$verbose" && echo
	CF_HEADER_PATH(cf_search,$cf_ncuhdr_root)
	test -n "$verbose" && echo search path $cf_search
	cf_save2_CPPFLAGS="$CPPFLAGS"
	for cf_incdir in $cf_search
	do
		CF_ADD_INCDIR($cf_incdir)
		for cf_header in \
			ncurses.h \
			curses.h
		do
			CF_NCURSES_CC_CHECK(cf_cv_ncurses_h2,$cf_header,$1)
			if test "$cf_cv_ncurses_h2" != no ; then
				cf_cv_ncurses_h2=$cf_incdir/$cf_header
				test -n "$verbose" && echo $ac_n "	... found $ac_c" 1>&AC_FD_MSG
				break
			fi
			test -n "$verbose" && echo "	... tested $cf_incdir/$cf_header" 1>&AC_FD_MSG
		done
		CPPFLAGS="$cf_save2_CPPFLAGS"
		test "$cf_cv_ncurses_h2" != no && break
	done
	test "$cf_cv_ncurses_h2" = no && AC_MSG_ERROR(not found)
	])

	CF_DIRNAME(cf_1st_incdir,$cf_cv_ncurses_h2)
	cf_cv_ncurses_header=`basename $cf_cv_ncurses_h2`
	if test `basename $cf_1st_incdir` = $cf_ncuhdr_root ; then
		cf_cv_ncurses_header=$cf_ncuhdr_root/$cf_cv_ncurses_header
	fi
	CF_ADD_INCDIR($cf_1st_incdir)

fi

# Set definitions to allow ifdef'ing for ncurses.h

case $cf_cv_ncurses_header in # (vi
*ncurses.h)
	AC_DEFINE(HAVE_NCURSES_H)
	;;
esac

case $cf_cv_ncurses_header in # (vi
ncurses/curses.h|ncurses/ncurses.h)
	AC_DEFINE(HAVE_NCURSES_NCURSES_H)
	;;
ncursesw/curses.h|ncursesw/ncurses.h)
	AC_DEFINE(HAVE_NCURSESW_NCURSES_H)
	;;
esac

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_LIBS version: 16 updated: 2010/11/20 17:02:38
dnl ---------------
dnl Look for the ncurses library.  This is a little complicated on Linux,
dnl because it may be linked with the gpm (general purpose mouse) library.
dnl Some distributions have gpm linked with (bsd) curses, which makes it
dnl unusable with ncurses.  However, we don't want to link with gpm unless
dnl ncurses has a dependency, since gpm is normally set up as a shared library,
dnl and the linker will record a dependency.
dnl
dnl The optional parameter gives the root name of the library, in case it is
dnl not installed as the default curses library.  That is how the
dnl wide-character version of ncurses is installed.
AC_DEFUN([CF_NCURSES_LIBS],
[AC_REQUIRE([CF_NCURSES_CPPFLAGS])

cf_nculib_root=ifelse($1,,ncurses,$1)
	# This works, except for the special case where we find gpm, but
	# ncurses is in a nonstandard location via $LIBS, and we really want
	# to link gpm.
cf_ncurses_LIBS=""
cf_ncurses_SAVE="$LIBS"
AC_CHECK_LIB(gpm,Gpm_Open,
	[AC_CHECK_LIB(gpm,initscr,
		[LIBS="$cf_ncurses_SAVE"],
		[cf_ncurses_LIBS="-lgpm"])])

case $host_os in #(vi
freebsd*)
	# This is only necessary if you are linking against an obsolete
	# version of ncurses (but it should do no harm, since it's static).
	if test "$cf_nculib_root" = ncurses ; then
		AC_CHECK_LIB(mytinfo,tgoto,[cf_ncurses_LIBS="-lmytinfo $cf_ncurses_LIBS"])
	fi
	;;
esac

CF_ADD_LIBS($cf_ncurses_LIBS)

if ( test -n "$cf_cv_curses_dir" && test "$cf_cv_curses_dir" != "no" )
then
	CF_ADD_LIBS(-l$cf_nculib_root)
else
	CF_FIND_LIBRARY($cf_nculib_root,$cf_nculib_root,
		[#include <${cf_cv_ncurses_header:-curses.h}>],
		[initscr()],
		initscr)
fi

if test -n "$cf_ncurses_LIBS" ; then
	AC_MSG_CHECKING(if we can link $cf_nculib_root without $cf_ncurses_LIBS)
	cf_ncurses_SAVE="$LIBS"
	for p in $cf_ncurses_LIBS ; do
		q=`echo $LIBS | sed -e "s%$p %%" -e "s%$p$%%"`
		if test "$q" != "$LIBS" ; then
			LIBS="$q"
		fi
	done
	AC_TRY_LINK([#include <${cf_cv_ncurses_header:-curses.h}>],
		[initscr(); mousemask(0,0); tgoto((char *)0, 0, 0);],
		[AC_MSG_RESULT(yes)],
		[AC_MSG_RESULT(no)
		 LIBS="$cf_ncurses_SAVE"])
fi

CF_UPPER(cf_nculib_ROOT,HAVE_LIB$cf_nculib_root)
AC_DEFINE_UNQUOTED($cf_nculib_ROOT)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NCURSES_VERSION version: 13 updated: 2010/10/23 15:54:49
dnl ------------------
dnl Check for the version of ncurses, to aid in reporting bugs, etc.
dnl Call CF_CURSES_CPPFLAGS first, or CF_NCURSES_CPPFLAGS.  We don't use
dnl AC_REQUIRE since that does not work with the shell's if/then/else/fi.
AC_DEFUN([CF_NCURSES_VERSION],
[
AC_REQUIRE([CF_CURSES_CPPFLAGS])dnl
AC_CACHE_CHECK(for ncurses version, cf_cv_ncurses_version,[
	cf_cv_ncurses_version=no
	cf_tempfile=out$$
	rm -f $cf_tempfile
	AC_TRY_RUN([
#include <${cf_cv_ncurses_header:-curses.h}>
#include <stdio.h>
int main()
{
	FILE *fp = fopen("$cf_tempfile", "w");
#ifdef NCURSES_VERSION
# ifdef NCURSES_VERSION_PATCH
	fprintf(fp, "%s.%d\n", NCURSES_VERSION, NCURSES_VERSION_PATCH);
# else
	fprintf(fp, "%s\n", NCURSES_VERSION);
# endif
#else
# ifdef __NCURSES_H
	fprintf(fp, "old\n");
# else
	make an error
# endif
#endif
	${cf_cv_main_return:-return}(0);
}],[
	cf_cv_ncurses_version=`cat $cf_tempfile`],,[

	# This will not work if the preprocessor splits the line after the
	# Autoconf token.  The 'unproto' program does that.
	cat > conftest.$ac_ext <<EOF
#include <${cf_cv_ncurses_header:-curses.h}>
#undef Autoconf
#ifdef NCURSES_VERSION
Autoconf NCURSES_VERSION
#else
#ifdef __NCURSES_H
Autoconf "old"
#endif
;
#endif
EOF
	cf_try="$ac_cpp conftest.$ac_ext 2>&AC_FD_CC | grep '^Autoconf ' >conftest.out"
	AC_TRY_EVAL(cf_try)
	if test -f conftest.out ; then
		cf_out=`cat conftest.out | sed -e 's%^Autoconf %%' -e 's%^[[^"]]*"%%' -e 's%".*%%'`
		test -n "$cf_out" && cf_cv_ncurses_version="$cf_out"
		rm -f conftest.out
	fi
])
	rm -f $cf_tempfile
])
test "$cf_cv_ncurses_version" = no || AC_DEFINE(NCURSES)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NO_LEAKS_OPTION version: 4 updated: 2006/12/16 14:24:05
dnl ------------------
dnl see CF_WITH_NO_LEAKS
AC_DEFUN([CF_NO_LEAKS_OPTION],[
AC_MSG_CHECKING(if you want to use $1 for testing)
AC_ARG_WITH($1,
	[$2],
	[AC_DEFINE($3)ifelse([$4],,[
	 $4
])
	: ${with_cflags:=-g}
	: ${with_no_leaks:=yes}
	 with_$1=yes],
	[with_$1=])
AC_MSG_RESULT(${with_$1:-no})

case .$with_cflags in #(vi
.*-g*)
	case .$CFLAGS in #(vi
	.*-g*) #(vi
		;;
	*)
		CF_ADD_CFLAGS([-g])
		;;
	esac
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_NUMBER_SYNTAX version: 1 updated: 2003/09/20 18:12:49
dnl ----------------
dnl Check if the given variable is a number.  If not, report an error.
dnl $1 is the variable
dnl $2 is the message
AC_DEFUN([CF_NUMBER_SYNTAX],[
if test -n "$1" ; then
  case $1 in #(vi
  [[0-9]]*) #(vi
 	;;
  *)
	AC_MSG_ERROR($2 is not a number: $1)
 	;;
  esac
else
  AC_MSG_ERROR($2 value is empty)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_OBJ_SUBDIR version: 4 updated: 2002/02/23 20:38:31
dnl -------------
dnl Compute the object-directory name from the given model name
AC_DEFUN([CF_OBJ_SUBDIR],
[
	case $1 in
	libtool) $2='obj_lo'  ;;
	normal)  $2='objects' ;;
	debug)   $2='obj_g' ;;
	profile) $2='obj_p' ;;
	shared)
		case $cf_cv_system_name in #(vi
		cygwin) #(vi
			$2='objects' ;;
		*)
			$2='obj_s' ;;
		esac
	esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PATHSEP version: 5 updated: 2010/05/26 05:38:42
dnl ----------
dnl Provide a value for the $PATH and similar separator
AC_DEFUN([CF_PATHSEP],
[
	case $cf_cv_system_name in
	os2*)	PATH_SEPARATOR=';'  ;;
	*)	PATH_SEPARATOR=':'  ;;
	esac
ifelse([$1],,,[$1=$PATH_SEPARATOR])
	AC_SUBST(PATH_SEPARATOR)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PATH_SYNTAX version: 13 updated: 2010/05/26 05:38:42
dnl --------------
dnl Check the argument to see that it looks like a pathname.  Rewrite it if it
dnl begins with one of the prefix/exec_prefix variables, and then again if the
dnl result begins with 'NONE'.  This is necessary to work around autoconf's
dnl delayed evaluation of those symbols.
AC_DEFUN([CF_PATH_SYNTAX],[
if test "x$prefix" != xNONE; then
  cf_path_syntax="$prefix"
else
  cf_path_syntax="$ac_default_prefix"
fi

case ".[$]$1" in #(vi
.\[$]\(*\)*|.\'*\'*) #(vi
  ;;
..|./*|.\\*) #(vi
  ;;
.[[a-zA-Z]]:[[\\/]]*) #(vi OS/2 EMX
  ;;
.\[$]{*prefix}*) #(vi
  eval $1="[$]$1"
  case ".[$]$1" in #(vi
  .NONE/*)
    $1=`echo [$]$1 | sed -e s%NONE%$cf_path_syntax%`
    ;;
  esac
  ;; #(vi
.no|.NONE/*)
  $1=`echo [$]$1 | sed -e s%NONE%$cf_path_syntax%`
  ;;
*)
  ifelse([$2],,[AC_MSG_ERROR([expected a pathname, not \"[$]$1\"])],$2)
  ;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PKG_CONFIG version: 3 updated: 2009/01/25 10:55:09
dnl -------------
dnl Check for the package-config program, unless disabled by command-line.
AC_DEFUN([CF_PKG_CONFIG],
[
AC_MSG_CHECKING(if you want to use pkg-config)
AC_ARG_WITH(pkg-config,
	[  --with-pkg-config{=path} enable/disable use of pkg-config],
	[cf_pkg_config=$withval],
	[cf_pkg_config=yes])
AC_MSG_RESULT($cf_pkg_config)

case $cf_pkg_config in #(vi
no) #(vi
	PKG_CONFIG=none
	;;
yes) #(vi
	AC_PATH_PROG(PKG_CONFIG, pkg-config, none)
	;;
*)
	PKG_CONFIG=$withval
	;;
esac

test -z "$PKG_CONFIG" && PKG_CONFIG=none
if test "$PKG_CONFIG" != none ; then
	CF_PATH_SYNTAX(PKG_CONFIG)
fi

AC_SUBST(PKG_CONFIG)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_POSIX_C_SOURCE version: 8 updated: 2010/05/26 05:38:42
dnl -----------------
dnl Define _POSIX_C_SOURCE to the given level, and _POSIX_SOURCE if needed.
dnl
dnl	POSIX.1-1990				_POSIX_SOURCE
dnl	POSIX.1-1990 and			_POSIX_SOURCE and
dnl		POSIX.2-1992 C-Language			_POSIX_C_SOURCE=2
dnl		Bindings Option
dnl	POSIX.1b-1993				_POSIX_C_SOURCE=199309L
dnl	POSIX.1c-1996				_POSIX_C_SOURCE=199506L
dnl	X/Open 2000				_POSIX_C_SOURCE=200112L
dnl
dnl Parameters:
dnl	$1 is the nominal value for _POSIX_C_SOURCE
AC_DEFUN([CF_POSIX_C_SOURCE],
[
cf_POSIX_C_SOURCE=ifelse([$1],,199506L,[$1])

cf_save_CFLAGS="$CFLAGS"
cf_save_CPPFLAGS="$CPPFLAGS"

CF_REMOVE_DEFINE(cf_trim_CFLAGS,$cf_save_CFLAGS,_POSIX_C_SOURCE)
CF_REMOVE_DEFINE(cf_trim_CPPFLAGS,$cf_save_CPPFLAGS,_POSIX_C_SOURCE)

AC_CACHE_CHECK(if we should define _POSIX_C_SOURCE,cf_cv_posix_c_source,[
	CF_MSG_LOG(if the symbol is already defined go no further)
	AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _POSIX_C_SOURCE
make an error
#endif],
	[cf_cv_posix_c_source=no],
	[cf_want_posix_source=no
	 case .$cf_POSIX_C_SOURCE in #(vi
	 .[[12]]??*) #(vi
		cf_cv_posix_c_source="-D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE"
		;;
	 .2) #(vi
		cf_cv_posix_c_source="-D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE"
		cf_want_posix_source=yes
		;;
	 .*)
		cf_want_posix_source=yes
		;;
	 esac
	 if test "$cf_want_posix_source" = yes ; then
		AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _POSIX_SOURCE
make an error
#endif],[],
		cf_cv_posix_c_source="$cf_cv_posix_c_source -D_POSIX_SOURCE")
	 fi
	 CF_MSG_LOG(ifdef from value $cf_POSIX_C_SOURCE)
	 CFLAGS="$cf_trim_CFLAGS"
	 CPPFLAGS="$cf_trim_CPPFLAGS $cf_cv_posix_c_source"
	 CF_MSG_LOG(if the second compile does not leave our definition intact error)
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _POSIX_C_SOURCE
make an error
#endif],,
	 [cf_cv_posix_c_source=no])
	 CFLAGS="$cf_save_CFLAGS"
	 CPPFLAGS="$cf_save_CPPFLAGS"
	])
])

if test "$cf_cv_posix_c_source" != no ; then
	CFLAGS="$cf_trim_CFLAGS"
	CPPFLAGS="$cf_trim_CPPFLAGS"
	CF_ADD_CFLAGS($cf_cv_posix_c_source)
fi

])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_AR version: 1 updated: 2009/01/01 20:15:22
dnl ----------
dnl Check for archiver "ar".
AC_DEFUN([CF_PROG_AR],[
AC_CHECK_TOOL(AR, ar, ar)
])
dnl ---------------------------------------------------------------------------
dnl CF_PROG_AWK version: 1 updated: 2006/09/16 11:40:59
dnl -----------
dnl Check for awk, ensure that the check found something.
AC_DEFUN([CF_PROG_AWK],
[
AC_PROG_AWK
test -z "$AWK" && AC_MSG_ERROR(No awk program found)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_CC_C_O version: 3 updated: 2010/08/14 18:25:37
dnl --------------
dnl Analogous to AC_PROG_CC_C_O, but more useful: tests only $CC, ensures that
dnl the output file can be renamed, and allows for a shell variable that can
dnl be used later.  The parameter is either CC or CXX.  The result is the
dnl cache variable:
dnl	$cf_cv_prog_CC_c_o
dnl	$cf_cv_prog_CXX_c_o
AC_DEFUN([CF_PROG_CC_C_O],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_MSG_CHECKING([whether [$]$1 understands -c and -o together])
AC_CACHE_VAL(cf_cv_prog_$1_c_o,
[
cat > conftest.$ac_ext <<CF_EOF
#include <stdio.h>
int main()
{
	${cf_cv_main_return:-return}(0);
}
CF_EOF
# We do the test twice because some compilers refuse to overwrite an
# existing .o file with -o, though they will create one.
ac_try='[$]$1 -c conftest.$ac_ext -o conftest2.$ac_objext >&AC_FD_CC'
if AC_TRY_EVAL(ac_try) &&
  test -f conftest2.$ac_objext && AC_TRY_EVAL(ac_try);
then
  eval cf_cv_prog_$1_c_o=yes
else
  eval cf_cv_prog_$1_c_o=no
fi
rm -rf conftest*
])dnl
if test $cf_cv_prog_$1_c_o = yes; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_EGREP version: 1 updated: 2006/09/16 11:40:59
dnl -------------
dnl AC_PROG_EGREP was introduced in autoconf 2.53.
dnl This macro adds a check to ensure the script found something.
AC_DEFUN([CF_PROG_EGREP],
[AC_CACHE_CHECK([for egrep], [ac_cv_prog_egrep],
   [if echo a | (grep -E '(a|b)') >/dev/null 2>&1
    then ac_cv_prog_egrep='grep -E'
    else ac_cv_prog_egrep='egrep'
    fi])
 EGREP=$ac_cv_prog_egrep
 AC_SUBST([EGREP])
test -z "$EGREP" && AC_MSG_ERROR(No egrep program found)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_EXT version: 10 updated: 2004/01/03 19:28:18
dnl -----------
dnl Compute $PROG_EXT, used for non-Unix ports, such as OS/2 EMX.
AC_DEFUN([CF_PROG_EXT],
[
AC_REQUIRE([CF_CHECK_CACHE])
case $cf_cv_system_name in
os2*)
    CFLAGS="$CFLAGS -Zmt"
    CPPFLAGS="$CPPFLAGS -D__ST_MT_ERRNO__"
    CXXFLAGS="$CXXFLAGS -Zmt"
    # autoconf's macro sets -Zexe and suffix both, which conflict:w
    LDFLAGS="$LDFLAGS -Zmt -Zcrtdll"
    ac_cv_exeext=.exe
    ;;
esac

AC_EXEEXT
AC_OBJEXT

PROG_EXT="$EXEEXT"
AC_SUBST(PROG_EXT)
test -n "$PROG_EXT" && AC_DEFINE_UNQUOTED(PROG_EXT,"$PROG_EXT")
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_GNAT version: 1 updated: 2010/06/19 15:22:18
dnl ------------
dnl Check for gnatmake, ensure that it is complete.
AC_DEFUN([CF_PROG_GNAT],[
cf_ada_make=gnatmake
AC_CHECK_PROG(gnat_exists, $cf_ada_make, yes, no)
if test "$ac_cv_prog_gnat_exists" = no; then
   cf_ada_make=
else
   CF_GNAT_VERSION
   AC_CHECK_PROG(M4_exists, m4, yes, no)
   if test "$ac_cv_prog_M4_exists" = no; then
      cf_cv_prog_gnat_correct=no
      echo Ada95 binding required program m4 not found. Ada95 binding disabled.
   fi
   if test "$cf_cv_prog_gnat_correct" = yes; then
      AC_MSG_CHECKING(if GNAT works)
      CF_GNAT_TRY_RUN([procedure conftest;],
[with Text_IO;
with GNAT.OS_Lib;
procedure conftest is
begin
   Text_IO.Put ("Hello World");
   Text_IO.New_Line;
   GNAT.OS_Lib.OS_Exit (0);
end conftest;],[cf_cv_prog_gnat_correct=yes],[cf_cv_prog_gnat_correct=no])
      AC_MSG_RESULT($cf_cv_prog_gnat_correct)
   fi
fi

AC_SUBST(cf_ada_make)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_PROG_LN_S version: 2 updated: 2010/08/14 18:25:37
dnl ------------
dnl Combine checks for "ln -s" and "ln -sf", updating $LN_S to include "-f"
dnl option if it is supported.
AC_DEFUN([CF_PROG_LN_S],[
AC_PROG_LN_S
AC_MSG_CHECKING(if $LN_S -f options work)

rm -f conf$$.src conf$$dst
echo >conf$$.dst
echo first >conf$$.src
if $LN_S -f conf$$.src conf$$.dst 2>/dev/null; then
	cf_prog_ln_sf=yes
else
	cf_prog_ln_sf=no
fi
rm -f conf$$.dst conf$$src
AC_MSG_RESULT($cf_prog_ln_sf)

test "$cf_prog_ln_sf" = yes && LN_S="$LN_S -f"
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_REMOVE_DEFINE version: 3 updated: 2010/01/09 11:05:50
dnl ----------------
dnl Remove all -U and -D options that refer to the given symbol from a list
dnl of C compiler options.  This works around the problem that not all
dnl compilers process -U and -D options from left-to-right, so a -U option
dnl cannot be used to cancel the effect of a preceding -D option.
dnl
dnl $1 = target (which could be the same as the source variable)
dnl $2 = source (including '$')
dnl $3 = symbol to remove
define([CF_REMOVE_DEFINE],
[
$1=`echo "$2" | \
	sed	-e 's/-[[UD]]'"$3"'\(=[[^ 	]]*\)\?[[ 	]]/ /g' \
		-e 's/-[[UD]]'"$3"'\(=[[^ 	]]*\)\?[$]//g'`
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_REMOVE_LIB version: 1 updated: 2007/02/17 14:11:52
dnl -------------
dnl Remove the given library from the symbol
dnl
dnl $1 = target (which could be the same as the source variable)
dnl $2 = source (including '$')
dnl $3 = library to remove
define([CF_REMOVE_LIB],
[
# remove $3 library from $2
$1=`echo "$2" | sed -e 's/-l$3[[ 	]]//g' -e 's/-l$3[$]//'`
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SHARED_OPTS version: 64 updated: 2010/06/05 16:51:16
dnl --------------
dnl --------------
dnl Attempt to determine the appropriate CC/LD options for creating a shared
dnl library.
dnl
dnl Note: ${LOCAL_LDFLAGS} is used to link executables that will run within the
dnl build-tree, i.e., by making use of the libraries that are compiled in ../lib
dnl We avoid compiling-in a ../lib path for the shared library since that can
dnl lead to unexpected results at runtime.
dnl ${LOCAL_LDFLAGS2} has the same intention but assumes that the shared libraries
dnl are compiled in ../../lib
dnl
dnl The variable 'cf_cv_do_symlinks' is used to control whether we configure
dnl to install symbolic links to the rel/abi versions of shared libraries.
dnl
dnl The variable 'cf_cv_shlib_version' controls whether we use the rel or abi
dnl version when making symbolic links.
dnl
dnl The variable 'cf_cv_shlib_version_infix' controls whether shared library
dnl version numbers are infix (ex: libncurses.<ver>.dylib) or postfix
dnl (ex: libncurses.so.<ver>).
dnl
dnl Some loaders leave 'so_locations' lying around.  It's nice to clean up.
AC_DEFUN([CF_SHARED_OPTS],
[
	AC_REQUIRE([CF_SUBST_NCURSES_VERSION])
	AC_REQUIRE([CF_LD_RPATH_OPT])
	LOCAL_LDFLAGS=
	LOCAL_LDFLAGS2=
	LD_SHARED_OPTS=
	INSTALL_LIB="-m 644"

	cf_cv_do_symlinks=no
	cf_ld_rpath_opt=
	test "$cf_cv_enable_rpath" = yes && cf_ld_rpath_opt="$LD_RPATH_OPT"

	AC_MSG_CHECKING(if release/abi version should be used for shared libs)
	AC_ARG_WITH(shlib-version,
	[  --with-shlib-version=X  Specify rel or abi version for shared libs],
	[test -z "$withval" && withval=auto
	case $withval in #(vi
	yes) #(vi
		cf_cv_shlib_version=auto
		;;
	rel|abi|auto|no) #(vi
		cf_cv_shlib_version=$withval
		;;
	*)
		AC_MSG_ERROR([option value must be one of: rel, abi, auto or no])
		;;
	esac
	],[cf_cv_shlib_version=auto])
	AC_MSG_RESULT($cf_cv_shlib_version)

	cf_cv_rm_so_locs=no

	# Some less-capable ports of gcc support only -fpic
	CC_SHARED_OPTS=
	if test "$GCC" = yes
	then
		AC_MSG_CHECKING(which $CC option to use)
		cf_save_CFLAGS="$CFLAGS"
		for CC_SHARED_OPTS in -fPIC -fpic ''
		do
			CFLAGS="$cf_save_CFLAGS $CC_SHARED_OPTS"
			AC_TRY_COMPILE([#include <stdio.h>],[int x = 1],[break],[])
		done
		AC_MSG_RESULT($CC_SHARED_OPTS)
		CFLAGS="$cf_save_CFLAGS"
	fi

	cf_cv_shlib_version_infix=no

	case $cf_cv_system_name in #(vi
	aix[[56]]*) #(vi
		if test "$GCC" = yes; then
			CC_SHARED_OPTS=
			MK_SHARED_LIB='$(CC) -shared'
		fi
		;;
	beos*) #(vi
		MK_SHARED_LIB='${CC} ${CFLAGS} -o $[@] -Xlinker -soname=`basename $[@]` -nostart -e 0'
		;;
	cygwin*) #(vi
		CC_SHARED_OPTS=
		MK_SHARED_LIB='sh ../mk_shared_lib.sh [$]@ [$]{CC} [$]{CFLAGS}'
		cf_cv_shlib_version=cygdll
		cf_cv_shlib_version_infix=cygdll
		cat >mk_shared_lib.sh <<-CF_EOF
		#!/bin/sh
		SHARED_LIB=\[$]1
		IMPORT_LIB=\`echo "\[$]1" | sed -e 's/cyg/lib/' -e 's/[[0-9]]*\.dll[$]/.dll.a/'\`
		shift
		cat <<-EOF
		Linking shared library
		** SHARED_LIB \[$]SHARED_LIB
		** IMPORT_LIB \[$]IMPORT_LIB
EOF
		exec \[$]* -shared -Wl,--out-implib=../lib/\[$]{IMPORT_LIB} -Wl,--export-all-symbols -o ../lib/\[$]{SHARED_LIB}
CF_EOF
		chmod +x mk_shared_lib.sh
		;;
	darwin*) #(vi
		EXTRA_CFLAGS="-no-cpp-precomp"
		CC_SHARED_OPTS="-dynamic"
		MK_SHARED_LIB='${CC} ${CFLAGS} -dynamiclib -install_name ${libdir}/`basename $[@]` -compatibility_version ${ABI_VERSION} -current_version ${ABI_VERSION} -o $[@]'
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=abi
		cf_cv_shlib_version_infix=yes
		AC_CACHE_CHECK([if ld -search_paths_first works], cf_cv_ldflags_search_paths_first, [
			cf_save_LDFLAGS=$LDFLAGS
			LDFLAGS="$LDFLAGS -Wl,-search_paths_first"
			AC_TRY_LINK(, [int i;], cf_cv_ldflags_search_paths_first=yes, cf_cv_ldflags_search_paths_first=no)
				LDFLAGS=$cf_save_LDFLAGS])
		if test $cf_cv_ldflags_search_paths_first = yes; then
			LDFLAGS="$LDFLAGS -Wl,-search_paths_first"
		fi
		;;
	hpux*) #(vi
		# (tested with gcc 2.7.2 -- I don't have c89)
		if test "$GCC" = yes; then
			LD_SHARED_OPTS='-Xlinker +b -Xlinker ${libdir}'
		else
			CC_SHARED_OPTS='+Z'
			LD_SHARED_OPTS='-Wl,+b,${libdir}'
		fi
		MK_SHARED_LIB='${LD} +b ${libdir} -b -o $[@]'
		# HP-UX shared libraries must be executable, and should be
		# readonly to exploit a quirk in the memory manager.
		INSTALL_LIB="-m 555"
		;;
	irix*) #(vi
		if test "$cf_cv_enable_rpath" = yes ; then
			EXTRA_LDFLAGS="${cf_ld_rpath_opt}\${libdir} $EXTRA_LDFLAGS"
		fi
		# tested with IRIX 5.2 and 'cc'.
		if test "$GCC" != yes; then
			CC_SHARED_OPTS='-KPIC'
			MK_SHARED_LIB='${CC} -shared -rdata_shared -soname `basename $[@]` -o $[@]'
		else
			MK_SHARED_LIB='${CC} -shared -Wl,-soname,`basename $[@]` -o $[@]'
		fi
		cf_cv_rm_so_locs=yes
		;;
	linux*|gnu*|k*bsd*-gnu) #(vi
		if test "$DFT_LWR_MODEL" = "shared" ; then
			LOCAL_LDFLAGS="${LD_RPATH_OPT}\$(LOCAL_LIBDIR)"
			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
		fi
		if test "$cf_cv_enable_rpath" = yes ; then
			EXTRA_LDFLAGS="${cf_ld_rpath_opt}\${libdir} $EXTRA_LDFLAGS"
		fi
		CF_SHARED_SONAME
		MK_SHARED_LIB='${CC} ${CFLAGS} -shared -Wl,-soname,'$cf_cv_shared_soname',-stats,-lc -o $[@]'
		;;
	openbsd[[2-9]].*) #(vi
		if test "$DFT_LWR_MODEL" = "shared" ; then
			LOCAL_LDFLAGS="${LD_RPATH_OPT}\$(LOCAL_LIBDIR)"
			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
		fi
		if test "$cf_cv_enable_rpath" = yes ; then
			EXTRA_LDFLAGS="${cf_ld_rpath_opt}\${libdir} $EXTRA_LDFLAGS"
		fi
		CC_SHARED_OPTS="$CC_SHARED_OPTS -DPIC"
		CF_SHARED_SONAME
		MK_SHARED_LIB='${CC} ${CFLAGS} -shared -Wl,-Bshareable,-soname,'$cf_cv_shared_soname',-stats,-lc -o $[@]'
		;;
	nto-qnx*|openbsd*|freebsd[[12]].*) #(vi
		CC_SHARED_OPTS="$CC_SHARED_OPTS -DPIC"
		MK_SHARED_LIB='${LD} -Bshareable -o $[@]'
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		;;
	freebsd*) #(vi
		CC_SHARED_OPTS="$CC_SHARED_OPTS -DPIC"
		if test "$DFT_LWR_MODEL" = "shared" && test "$cf_cv_enable_rpath" = yes ; then
			LOCAL_LDFLAGS="${cf_ld_rpath_opt}\$(LOCAL_LIBDIR)"
			LOCAL_LDFLAGS2="${cf_ld_rpath_opt}\${libdir} $LOCAL_LDFLAGS"
			EXTRA_LDFLAGS="${cf_ld_rpath_opt}\${libdir} $EXTRA_LDFLAGS"
		fi
		CF_SHARED_SONAME
		MK_SHARED_LIB='${LD} -shared -Bshareable -soname=`basename $[@]` -o $[@]'
		;;
	netbsd*) #(vi
		CC_SHARED_OPTS="$CC_SHARED_OPTS -DPIC"
		if test "$DFT_LWR_MODEL" = "shared" && test "$cf_cv_enable_rpath" = yes ; then
			LOCAL_LDFLAGS="${cf_ld_rpath_opt}\$(LOCAL_LIBDIR)"
			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
			EXTRA_LDFLAGS="${cf_ld_rpath_opt}\${libdir} $EXTRA_LDFLAGS"
			if test "$cf_cv_shlib_version" = auto; then
			if test -f /usr/libexec/ld.elf_so; then
				cf_cv_shlib_version=abi
			else
				cf_cv_shlib_version=rel
			fi
			fi
			CF_SHARED_SONAME
			MK_SHARED_LIB='${CC} ${CFLAGS} -shared -Wl,-soname,'$cf_cv_shared_soname' -o $[@]'
		else
			MK_SHARED_LIB='${LD} -shared -Bshareable -o $[@]'
		fi
		;;
	osf*|mls+*) #(vi
		# tested with OSF/1 V3.2 and 'cc'
		# tested with OSF/1 V3.2 and gcc 2.6.3 (but the c++ demo didn't
		# link with shared libs).
		MK_SHARED_LIB='${LD} -set_version ${REL_VERSION}:${ABI_VERSION} -expect_unresolved "*" -shared -soname `basename $[@]`'
		case $host_os in #(vi
		osf4*)
			MK_SHARED_LIB="${MK_SHARED_LIB} -msym"
			;;
		esac
		MK_SHARED_LIB="${MK_SHARED_LIB}"' -o $[@]'
		if test "$DFT_LWR_MODEL" = "shared" ; then
			LOCAL_LDFLAGS="${LD_RPATH_OPT}\$(LOCAL_LIBDIR)"
			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
		fi
		cf_cv_rm_so_locs=yes
		;;
	sco3.2v5*)  # (also uw2* and UW7: hops 13-Apr-98
		# tested with osr5.0.5
		if test "$GCC" != yes; then
			CC_SHARED_OPTS='-belf -KPIC'
		fi
		MK_SHARED_LIB='${LD} -dy -G -h `basename $[@] .${REL_VERSION}`.${ABI_VERSION} -o [$]@'
		if test "$cf_cv_enable_rpath" = yes ; then
			# only way is to set LD_RUN_PATH but no switch for it
			RUN_PATH=$libdir
		fi
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		LINK_PROGS='LD_RUN_PATH=${libdir}'
		LINK_TESTS='Pwd=`pwd`;LD_RUN_PATH=`dirname $${Pwd}`/lib'
		;;
	sunos4*) #(vi
		# tested with SunOS 4.1.1 and gcc 2.7.0
		if test "$GCC" != yes; then
			CC_SHARED_OPTS='-KPIC'
		fi
		MK_SHARED_LIB='${LD} -assert pure-text -o $[@]'
		test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=rel
		;;
	solaris2*) #(vi
		# tested with SunOS 5.5.1 (solaris 2.5.1) and gcc 2.7.2
		# tested with SunOS 5.10 (solaris 10) and gcc 3.4.3
		if test "$DFT_LWR_MODEL" = "shared" ; then
			LOCAL_LDFLAGS="-R \$(LOCAL_LIBDIR):\${libdir}"
			LOCAL_LDFLAGS2="$LOCAL_LDFLAGS"
		fi
		if test "$cf_cv_enable_rpath" = yes ; then
			EXTRA_LDFLAGS="-R \${libdir} $EXTRA_LDFLAGS"
		fi
		CF_SHARED_SONAME
		if test "$GCC" != yes; then
			cf_save_CFLAGS="$CFLAGS"
			for cf_shared_opts in -xcode=pic32 -xcode=pic13 -KPIC -Kpic -O
			do
				CFLAGS="$cf_shared_opts $cf_save_CFLAGS"
				AC_TRY_COMPILE([#include <stdio.h>],[printf("Hello\n");],[break])
			done
			CFLAGS="$cf_save_CFLAGS"
			CC_SHARED_OPTS=$cf_shared_opts
			MK_SHARED_LIB='${CC} -dy -G -h '$cf_cv_shared_soname' -o $[@]'
		else
			MK_SHARED_LIB='${CC} -shared -dy -G -h '$cf_cv_shared_soname' -o $[@]'
		fi
		;;
	sysv5uw7*|unix_sv*) #(vi
		# tested with UnixWare 7.1.0 (gcc 2.95.2 and cc)
		if test "$GCC" != yes; then
			CC_SHARED_OPTS='-KPIC'
		fi
		MK_SHARED_LIB='${LD} -d y -G -o [$]@'
		;;
	*)
		CC_SHARED_OPTS='unknown'
		MK_SHARED_LIB='echo unknown'
		;;
	esac

	# This works if the last tokens in $MK_SHARED_LIB are the -o target.
	case "$cf_cv_shlib_version" in #(vi
	rel|abi)
		case "$MK_SHARED_LIB" in #(vi
		*'-o $[@]') #(vi
			test "$cf_cv_do_symlinks" = no && cf_cv_do_symlinks=yes
			;;
		*)
			AC_MSG_WARN(ignored --with-shlib-version)
			;;
		esac
		;;
	esac

	if test -n "$cf_ld_rpath_opt" ; then
		MK_SHARED_LIB="$MK_SHARED_LIB $cf_ld_rpath_opt\${libdir}"
	fi

	AC_SUBST(CC_SHARED_OPTS)
	AC_SUBST(LD_RPATH_OPT)
	AC_SUBST(LD_SHARED_OPTS)
	AC_SUBST(MK_SHARED_LIB)
	AC_SUBST(LINK_PROGS)
	AC_SUBST(LINK_TESTS)
	AC_SUBST(EXTRA_LDFLAGS)
	AC_SUBST(LOCAL_LDFLAGS)
	AC_SUBST(LOCAL_LDFLAGS2)
	AC_SUBST(INSTALL_LIB)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SHARED_SONAME version: 3 updated: 2008/09/08 18:34:43
dnl ----------------
dnl utility macro for CF_SHARED_OPTS, constructs "$cf_cv_shared_soname" for
dnl substitution into MK_SHARED_LIB string for the "-soname" (or similar)
dnl option.
dnl
dnl $1 is the default that should be used for "$cf_cv_shlib_version".
dnl If missing, use "rel".
define([CF_SHARED_SONAME],
[
	test "$cf_cv_shlib_version" = auto && cf_cv_shlib_version=ifelse($1,,rel,$1)
	if test "$cf_cv_shlib_version" = rel; then
		cf_cv_shared_soname='`basename $[@] .${REL_VERSION}`.${ABI_VERSION}'
	else
		cf_cv_shared_soname='`basename $[@]`'
	fi
])
dnl ---------------------------------------------------------------------------
dnl CF_STRIP_G_OPT version: 3 updated: 2002/12/21 19:25:52
dnl --------------
dnl	Remove "-g" option from the compiler options
AC_DEFUN([CF_STRIP_G_OPT],
[$1=`echo ${$1} | sed -e 's%-g %%' -e 's%-g$%%'`])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SUBDIR_PATH version: 6 updated: 2010/04/21 06:20:50
dnl --------------
dnl Construct a search-list for a nonstandard header/lib-file
dnl	$1 = the variable to return as result
dnl	$2 = the package name
dnl	$3 = the subdirectory, e.g., bin, include or lib
AC_DEFUN([CF_SUBDIR_PATH],
[
$1=

CF_ADD_SUBDIR_PATH($1,$2,$3,/usr,$prefix)
CF_ADD_SUBDIR_PATH($1,$2,$3,$prefix,NONE)
CF_ADD_SUBDIR_PATH($1,$2,$3,/usr/local,$prefix)
CF_ADD_SUBDIR_PATH($1,$2,$3,/opt,$prefix)
CF_ADD_SUBDIR_PATH($1,$2,$3,[$]HOME,$prefix)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_SUBST version: 4 updated: 2006/06/17 12:33:03
dnl --------
dnl	Shorthand macro for substituting things that the user may override
dnl	with an environment variable.
dnl
dnl	$1 = long/descriptive name
dnl	$2 = environment variable
dnl	$3 = default value
AC_DEFUN([CF_SUBST],
[AC_CACHE_VAL(cf_cv_subst_$2,[
AC_MSG_CHECKING(for $1 (symbol $2))
CF_SUBST_IF([-z "[$]$2"], [$2], [$3])
cf_cv_subst_$2=[$]$2
AC_MSG_RESULT([$]$2)
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TERM_HEADER version: 2 updated: 2010/10/23 15:54:49
dnl --------------
dnl Look for term.h, which is part of X/Open curses.  It defines the interface
dnl to terminfo database.  Usually it is in the same include-path as curses.h,
dnl but some packagers change this, breaking various applications.
AC_DEFUN([CF_TERM_HEADER],[
AC_CACHE_CHECK(for terminfo header, cf_cv_term_header,[
case ${cf_cv_ncurses_header} in #(vi
*/ncurses.h|*/ncursesw.h) #(vi
	cf_term_header=`echo "$cf_cv_ncurses_header" | sed -e 's%ncurses[[^.]]*\.h$%term.h%'`
	;;
*)
	cf_term_header=term.h
	;;
esac

for cf_test in $cf_term_header "ncurses/term.h" "ncursesw/term.h"
do
AC_TRY_COMPILE([#include <stdio.h>
#include <${cf_cv_ncurses_header:-curses.h}>
#include <$cf_test>
],[int x = auto_left_margin],[
	cf_cv_term_header="$cf_test"],[
	cf_cv_term_header=unknown
	])
	test "$cf_cv_term_header" != unknown && break
done
])

# Set definitions to allow ifdef'ing to accommodate subdirectories

case $cf_cv_term_header in # (vi
*term.h)
	AC_DEFINE(HAVE_TERM_H)
	;;
esac

case $cf_cv_term_header in # (vi
ncurses/term.h) #(vi
	AC_DEFINE(HAVE_NCURSES_TERM_H)
	;;
ncursesw/term.h)
	AC_DEFINE(HAVE_NCURSESW_TERM_H)
	;;
esac
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_TOP_BUILDDIR version: 1 updated: 2006/10/15 16:33:23
dnl ---------------
dnl Define a top_builddir symbol, for applications that need an absolute path.
AC_DEFUN([CF_TOP_BUILDDIR],
[
top_builddir=`pwd`
AC_SUBST(top_builddir)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_UPPER version: 5 updated: 2001/01/29 23:40:59
dnl --------
dnl Make an uppercase version of a variable
dnl $1=uppercase($2)
AC_DEFUN([CF_UPPER],
[
$1=`echo "$2" | sed y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%`
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_UTF8_LIB version: 7 updated: 2010/06/20 09:24:28
dnl -----------
dnl Check for multibyte support, and if not found, utf8 compatibility library
AC_DEFUN([CF_UTF8_LIB],
[
AC_CACHE_CHECK(for multibyte character support,cf_cv_utf8_lib,[
	cf_save_LIBS="$LIBS"
	AC_TRY_LINK([
#include <stdlib.h>],[putwc(0,0);],
	[cf_cv_utf8_lib=yes],
	[CF_FIND_LINKAGE([
#include <libutf8.h>],[putwc(0,0);],utf8,
		[cf_cv_utf8_lib=add-on],
		[cf_cv_utf8_lib=no])
])])

# HAVE_LIBUTF8_H is used by ncurses if curses.h is shared between
# ncurses/ncursesw:
if test "$cf_cv_utf8_lib" = "add-on" ; then
	AC_DEFINE(HAVE_LIBUTF8_H)
	CF_ADD_INCDIR($cf_cv_header_path_utf8)
	CF_ADD_LIBDIR($cf_cv_library_path_utf8)
	CF_ADD_LIBS($cf_cv_library_file_utf8)
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_VERBOSE version: 3 updated: 2007/07/29 09:55:12
dnl ----------
dnl Use AC_VERBOSE w/o the warnings
AC_DEFUN([CF_VERBOSE],
[test -n "$verbose" && echo "	$1" 1>&AC_FD_MSG
CF_MSG_LOG([$1])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WEAK_SYMBOLS version: 1 updated: 2008/08/16 19:18:06
dnl ---------------
dnl Check for compiler-support for weak symbols.
dnl This works with "recent" gcc.
AC_DEFUN([CF_WEAK_SYMBOLS],[
AC_CACHE_CHECK(if $CC supports weak symbols,cf_cv_weak_symbols,[

AC_TRY_COMPILE([
#include <stdio.h>],
[
#if defined(__GNUC__)
#  if defined __USE_ISOC99
#    define _cat_pragma(exp)	_Pragma(#exp)
#    define _weak_pragma(exp)	_cat_pragma(weak name)
#  else
#    define _weak_pragma(exp)
#  endif
#  define _declare(name)	__extension__ extern __typeof__(name) name
#  define weak_symbol(name)	_weak_pragma(name) _declare(name) __attribute__((weak))
#endif

weak_symbol(fopen);
],[cf_cv_weak_symbols=yes],[cf_cv_weak_symbols=no])
])
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_ADA_COMPILER version: 2 updated: 2010/06/26 17:35:58
dnl --------------------
dnl Command-line option to specify the Ada95 compiler.
AC_DEFUN([CF_WITH_ADA_COMPILER],[
AC_MSG_CHECKING(for ada-compiler)
AC_ARG_WITH(ada-compiler,
	[  --with-ada-compiler=CMD specify Ada95 compiler command (default gnatmake)],
	[cf_ada_compiler=$withval],
	[cf_ada_compiler=gnatmake])
AC_SUBST(cf_ada_compiler)
AC_MSG_RESULT($cf_ada_compiler)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_ADA_INCLUDE version: 2 updated: 2010/06/26 17:35:58
dnl -------------------
dnl Command-line option to specify where Ada includes will install.
AC_DEFUN([CF_WITH_ADA_INCLUDE],[
AC_MSG_CHECKING(for ada-include)
CF_WITH_PATH(ada-include,
   [  --with-ada-include=DIR  Ada includes are in DIR],
   ADA_INCLUDE,
   PREFIX/share/ada/adainclude,
   [$]prefix/share/ada/adainclude)
AC_SUBST(ADA_INCLUDE)
AC_MSG_RESULT($ADA_INCLUDE)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_ADA_OBJECTS version: 2 updated: 2010/06/26 17:35:58
dnl -------------------
dnl Command-line option to specify where Ada objects will install.
AC_DEFUN([CF_WITH_ADA_OBJECTS],[
AC_MSG_CHECKING(for ada-objects)
CF_WITH_PATH(ada-objects,
   [  --with-ada-objects=DIR  Ada objects are in DIR],
   ADA_OBJECTS,
   PREFIX/lib/ada/adalib,
   [$]prefix/lib/ada/adalib)
AC_SUBST(ADA_OBJECTS)
AC_MSG_RESULT($ADA_OBJECTS)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_ADA_SHAREDLIB version: 2 updated: 2010/06/26 17:35:58
dnl ---------------------
dnl Command-line option to specify if an Ada95 shared-library should be built,
dnl and optionally what its soname should be.
AC_DEFUN([CF_WITH_ADA_SHAREDLIB],[
AC_MSG_CHECKING(if an Ada95 shared-library should be built)
AC_ARG_WITH(ada-sharedlib,
	[  --with-ada-sharedlib=XX build Ada95 shared-library],
	[with_ada_sharedlib=$withval],
	[with_ada_sharedlib=no])
AC_MSG_RESULT($with_ada_sharedlib)

ADA_SHAREDLIB='lib$(LIB_NAME).so.1'
MAKE_ADA_SHAREDLIB="#"

if test "x$with_ada_sharedlib" != xno
then
	MAKE_ADA_SHAREDLIB=
	if test "x$with_ada_sharedlib" != xyes
	then
		ADA_SHAREDLIB="$with_ada_sharedlib"
	fi
fi

AC_SUBST(ADA_SHAREDLIB)
AC_SUBST(MAKE_ADA_SHAREDLIB)
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_CURSES_DIR version: 3 updated: 2010/11/20 17:02:38
dnl ------------------
dnl Wrapper for AC_ARG_WITH to specify directory under which to look for curses
dnl libraries.
AC_DEFUN([CF_WITH_CURSES_DIR],[

AC_MSG_CHECKING(for specific curses-directory)
AC_ARG_WITH(curses-dir,
	[  --with-curses-dir=DIR   directory in which (n)curses is installed],
	[cf_cv_curses_dir=$withval],
	[cf_cv_curses_dir=no])
AC_MSG_RESULT($cf_cv_curses_dir)

if ( test -n "$cf_cv_curses_dir" && test "$cf_cv_curses_dir" != "no" )
then
	CF_PATH_SYNTAX(withval)
	if test -d "$cf_cv_curses_dir"
	then
		CF_ADD_INCDIR($cf_cv_curses_dir/include)
		CF_ADD_LIBDIR($cf_cv_curses_dir/lib)
	fi
fi
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PATH version: 10 updated: 2010/10/23 15:44:18
dnl ------------
dnl Wrapper for AC_ARG_WITH to ensure that user supplies a pathname, not just
dnl defaulting to yes/no.
dnl
dnl $1 = option name
dnl $2 = help-text
dnl $3 = environment variable to set
dnl $4 = default value, shown in the help-message, must be a constant
dnl $5 = default value, if it's an expression & cannot be in the help-message
dnl
AC_DEFUN([CF_WITH_PATH],
[AC_ARG_WITH($1,[$2 ](default: ifelse([$4],,empty,[$4])),,
ifelse([$4],,[withval="${$3}"],[withval="${$3:-ifelse([$5],,[$4],[$5])}"]))dnl
if ifelse([$5],,true,[test -n "$5"]) ; then
CF_PATH_SYNTAX(withval)
fi
$3="$withval"
AC_SUBST($3)dnl
])dnl
dnl ---------------------------------------------------------------------------
dnl CF_WITH_PTHREAD version: 3 updated: 2010/05/29 16:31:02
dnl ---------------
dnl Check for POSIX thread library.
AC_DEFUN([CF_WITH_PTHREAD],
[
AC_MSG_CHECKING(if you want to link with the pthread library)
AC_ARG_WITH(pthread,
    [  --with-pthread          use POSIX thread library],
    [with_pthread=$withval],
    [with_pthread=no])
AC_MSG_RESULT($with_pthread)

if test "$with_pthread" != no ; then
    AC_CHECK_HEADER(pthread.h,[
        AC_DEFINE(HAVE_PTHREADS_H)

        AC_MSG_CHECKING(if we can link with the pthread library)
        cf_save_LIBS="$LIBS"
        CF_ADD_LIB(pthread)
        AC_TRY_LINK([
#include <pthread.h>
],[
        int rc = pthread_create(0,0,0,0);
],[with_pthread=yes],[with_pthread=no])
        LIBS="$cf_save_LIBS"
        AC_MSG_RESULT($with_pthread)

        if test "$with_pthread" = yes ; then
            CF_ADD_LIB(pthread)
            AC_DEFINE(HAVE_LIBPTHREADS)
        else
            AC_MSG_ERROR(Cannot link with pthread library)
        fi
    ])
fi
])
dnl ---------------------------------------------------------------------------
dnl CF_XOPEN_SOURCE version: 34 updated: 2010/05/26 05:38:42
dnl ---------------
dnl Try to get _XOPEN_SOURCE defined properly that we can use POSIX functions,
dnl or adapt to the vendor's definitions to get equivalent functionality,
dnl without losing the common non-POSIX features.
dnl
dnl Parameters:
dnl	$1 is the nominal value for _XOPEN_SOURCE
dnl	$2 is the nominal value for _POSIX_C_SOURCE
AC_DEFUN([CF_XOPEN_SOURCE],[

cf_XOPEN_SOURCE=ifelse([$1],,500,[$1])
cf_POSIX_C_SOURCE=ifelse([$2],,199506L,[$2])
cf_xopen_source=

case $host_os in #(vi
aix[[456]]*) #(vi
	cf_xopen_source="-D_ALL_SOURCE"
	;;
darwin[[0-8]].*) #(vi
	cf_xopen_source="-D_APPLE_C_SOURCE"
	;;
darwin*) #(vi
	cf_xopen_source="-D_DARWIN_C_SOURCE"
	;;
freebsd*|dragonfly*) #(vi
	# 5.x headers associate
	#	_XOPEN_SOURCE=600 with _POSIX_C_SOURCE=200112L
	#	_XOPEN_SOURCE=500 with _POSIX_C_SOURCE=199506L
	cf_POSIX_C_SOURCE=200112L
	cf_XOPEN_SOURCE=600
	cf_xopen_source="-D_BSD_TYPES -D__BSD_VISIBLE -D_POSIX_C_SOURCE=$cf_POSIX_C_SOURCE -D_XOPEN_SOURCE=$cf_XOPEN_SOURCE"
	;;
hpux11*) #(vi
	cf_xopen_source="-D_HPUX_SOURCE -D_XOPEN_SOURCE=500"
	;;
hpux*) #(vi
	cf_xopen_source="-D_HPUX_SOURCE"
	;;
irix[[56]].*) #(vi
	cf_xopen_source="-D_SGI_SOURCE"
	;;
linux*|gnu*|mint*|k*bsd*-gnu) #(vi
	CF_GNU_SOURCE
	;;
mirbsd*) #(vi
	# setting _XOPEN_SOURCE or _POSIX_SOURCE breaks <arpa/inet.h>
	;;
netbsd*) #(vi
	# setting _XOPEN_SOURCE breaks IPv6 for lynx on NetBSD 1.6, breaks xterm, is not needed for ncursesw
	;;
openbsd*) #(vi
	# setting _XOPEN_SOURCE breaks xterm on OpenBSD 2.8, is not needed for ncursesw
	;;
osf[[45]]*) #(vi
	cf_xopen_source="-D_OSF_SOURCE"
	;;
nto-qnx*) #(vi
	cf_xopen_source="-D_QNX_SOURCE"
	;;
sco*) #(vi
	# setting _XOPEN_SOURCE breaks Lynx on SCO Unix / OpenServer
	;;
solaris2.1[[0-9]]) #(vi
	cf_xopen_source="-D__EXTENSIONS__ -D_XOPEN_SOURCE=$cf_XOPEN_SOURCE"
	;;
solaris2.[[1-9]]) #(vi
	cf_xopen_source="-D__EXTENSIONS__"
	;;
*)
	AC_CACHE_CHECK(if we should define _XOPEN_SOURCE,cf_cv_xopen_source,[
	AC_TRY_COMPILE([#include <sys/types.h>],[
#ifndef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_xopen_source=no],
	[cf_save="$CPPFLAGS"
	 CPPFLAGS="$CPPFLAGS -D_XOPEN_SOURCE=$cf_XOPEN_SOURCE"
	 AC_TRY_COMPILE([#include <sys/types.h>],[
#ifdef _XOPEN_SOURCE
make an error
#endif],
	[cf_cv_xopen_source=no],
	[cf_cv_xopen_source=$cf_XOPEN_SOURCE])
	CPPFLAGS="$cf_save"
	])
])
	if test "$cf_cv_xopen_source" != no ; then
		CF_REMOVE_DEFINE(CFLAGS,$CFLAGS,_XOPEN_SOURCE)
		CF_REMOVE_DEFINE(CPPFLAGS,$CPPFLAGS,_XOPEN_SOURCE)
		cf_temp_xopen_source="-D_XOPEN_SOURCE=$cf_cv_xopen_source"
		CF_ADD_CFLAGS($cf_temp_xopen_source)
	fi
	CF_POSIX_C_SOURCE($cf_POSIX_C_SOURCE)
	;;
esac

if test -n "$cf_xopen_source" ; then
	CF_ADD_CFLAGS($cf_xopen_source)
fi
])
