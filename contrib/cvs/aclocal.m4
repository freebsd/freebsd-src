# aclocal.m4 generated automatically by aclocal 1.5

# Copyright 1996, 1997, 1998, 1999, 2000, 2001
# Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

/* This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

AC_DEFUN(ACX_WITH_GSSAPI,[
#
# Use --with-gssapi[=DIR] to enable GSSAPI support.
#
# defaults to enabled with DIR in default list below
#
# Search for /SUNHEA/ and read the comments about this default below.
#
AC_ARG_WITH([gssapi],
	    [  --with-gssapi=value     GSSAPI directory],
	    [acx_gssapi_withgssapi=$withval], [acx_gssapi_withgssapi=yes])dnl

dnl
dnl FIXME - cache withval and obliterate later cache values when options change
dnl
#
# Try to locate a GSSAPI installation if no location was specified, assuming
# GSSAPI was enabled (the default).
# 
if test -n "$acx_gssapi_cv_gssapi"; then
  # Granted, this is a slightly ugly way to print this info, but the
  # AC_CHECK_HEADER used in the search for a GSSAPI installation makes using
  # AC_CACHE_CHECK worse
  AC_MSG_CHECKING([for GSSAPI])
else :; fi
AC_CACHE_VAL([acx_gssapi_cv_gssapi], [
if test x$acx_gssapi_withgssapi = xyes; then
  # --with but no location specified
  # assume a gssapi.h or gssapi/gssapi.h locates our install.
  #
  # This isn't always strictly true.  For instance Solaris 7's SUNHEA (header)
  # package installs gssapi.h whether or not the necessary libraries are
  # installed.  I'm still not sure whether to consider this a bug.  The long
  # way around is to not consider GSSPAI installed unless gss_import_name is
  # found, but that brings up a lot of other hassles, like continuing to let
  # gcc & ld generate the error messages when the user uses --with-gssapi=dir
  # as a debugging aid.  The short way around is to disable GSSAPI by default,
  # but I think Sun users have been faced with this for awhile and I haven't
  # heard many complaints.
  acx_gssapi_save_CPPFLAGS=$CPPFLAGS
  for acx_gssapi_cv_gssapi in yes /usr/kerberos /usr/cygnus/kerbnet no; do
    if test x$acx_gssapi_cv_gssapi = xno; then
      break
    fi
    if test x$acx_gssapi_cv_gssapi = xyes; then
      AC_CHECKING([for GSSAPI])
    else
      CPPFLAGS="$acx_gssapi_save_CPPFLAGS -I$acx_gssapi_cv_gssapi/include"
      AC_CHECKING([for GSSAPI in $acx_gssapi_cv_gssapi])
    fi
    unset ac_cv_header_gssapi_h
    unset ac_cv_header_gssapi_gssapi_h
    unset ac_cv_header_krb5_h
    AC_CHECK_HEADERS([gssapi.h gssapi/gssapi.h krb5.h])
    if (test "$ac_cv_header_gssapi_h" = yes ||
	  test "$ac_cv_header_gssapi_gssapi_h" = yes) &&
	test "$ac_cv_header_krb5_h" = yes; then
      break
    fi
  done
  CPPFLAGS=$acx_gssapi_save_CPPFLAGS
else
  acx_gssapi_cv_gssapi=$acx_gssapi_withgssapi
fi
AC_MSG_CHECKING([for GSSAPI])
])dnl
AC_MSG_RESULT([$acx_gssapi_cv_gssapi])

#
# Set up GSSAPI includes for later use.  We don't bother to check for
# $acx_gssapi_cv_gssapi=no here since that will be caught later.
#
if test x$acx_gssapi_cv_gssapi = yes; then
  # no special includes necessary
  GSSAPI_INCLUDES=""
else
  # GSSAPI at $acx_gssapi_cv_gssapi (could be 'no')
  GSSAPI_INCLUDES=" -I$acx_gssapi_cv_gssapi/include"
fi

#
# Get the rest of the information CVS needs to compile with GSSAPI support
#
if test x$acx_gssapi_cv_gssapi != xno; then
  # define HAVE_GSSAPI and set up the includes
  AC_DEFINE([HAVE_GSSAPI],, [Define if you have GSSAPI with Kerberos version 5 available.])
  includeopt=$includeopt$GSSAPI_INCLUDES

  # locate any other headers
  acx_gssapi_save_CPPFLAGS=$CPPFLAGS
  CPPFLAGS=$CPPFLAGS$GSSAPI_INCLUDES
  dnl We don't use HAVE_KRB5_H anywhere, but including it here might make it
  dnl easier to spot errors by reading configure output
  AC_CHECK_HEADERS([gssapi.h gssapi/gssapi.h gssapi/gssapi_generic.h krb5.h])
  # And look through them for GSS_C_NT_HOSTBASED_SERVICE or its alternatives
  AC_CACHE_CHECK([for GSS_C_NT_HOSTBASED_SERVICE], [acx_gssapi_cv_gss_c_nt_hostbased_service],
   [acx_gssapi_cv_gss_c_nt_hostbased_service=no
    if test "$ac_cv_header_gssapi_h" = "yes"; then
      AC_EGREP_HEADER([GSS_C_NT_HOSTBASED_SERVICE], [gssapi.h],
		      [acx_gssapi_cv_gss_c_nt_hostbased_service=yes],
		      AC_EGREP_HEADER([gss_nt_service_name], [gssapi.h],
				      [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name]))
    fi
    if test $acx_gssapi_cv_gss_c_nt_hostbased_service = no &&
        test "$ac_cv_header_gssapi_gssapi_h" = "yes"; then
      AC_EGREP_HEADER([GSS_C_NT_HOSTBASED_SERVICE], [gssapi/gssapi.h],
		      [acx_gssapi_cv_gss_c_nt_hostbased_service],
		      AC_EGREP_HEADER([gss_nt_service_name], [gssapi/gssapi.h],
				      [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name]))
    fi
    if test $acx_gssapi_cv_gss_c_nt_hostbased_service = no &&
        test "$ac_cv_header_gssapi_gssapi_generic_h" = "yes"; then
      AC_EGREP_HEADER([GSS_C_NT_HOSTBASED_SERVICE], [gssapi/gssapi_generic.h],
		      [acx_gssapi_cv_gss_c_nt_hostbased_service],
		      AC_EGREP_HEADER([gss_nt_service_name], [gssapi/gssapi_generic.h],
				      [acx_gssapi_cv_gss_c_nt_hostbased_service=gss_nt_service_name]))
    fi])
  if test $acx_gssapi_cv_gss_c_nt_hostbased_service != yes &&
      test $acx_gssapi_cv_gss_c_nt_hostbased_service != no; then
    # don't define for yes since that means it already means something and
    # don't define for no since we'd rather the compiler catch the error
    AC_DEFINE_UNQUOTED([GSS_C_NT_HOSTBASED_SERVICE], [$acx_gssapi_cv_gss_c_nt_hostbased_service],
[Define to an alternative value if GSS_C_NT_HOSTBASED_SERVICE isn't defined
in the gssapi.h header file.  MIT Kerberos 1.2.1 requires this.  Only relevant
when using GSSAPI.])
  fi
  CPPFLAGS=$acx_gssapi_save_CPPFLAGS

  # Expect the libs to be installed parallel to the headers
  #
  # We could try once with and once without, but I'm not sure it's worth the
  # trouble.
  if test x$acx_gssapi_cv_gssapi != xyes; then
    if test -z "$LIBS"; then
      LIBS="-L$acx_gssapi_cv_gssapi/lib"
    else
      LIBS="-L$acx_gssapi_cv_gssapi/lib $LIBS"
    fi
  else :; fi

  dnl What happens if we want to enable, say, krb5 and some other GSSAPI
  dnl authentication method at the same time?
  #
  # Some of the order below is particular due to library dependencies
  #

  #
  # des			Heimdal K 0.3d, but Heimdal seems to be set up such
  #			that it could have been installed from elsewhere.
  #
  AC_SEARCH_LIBS([des_set_odd_parity], [des])

  #
  # com_err		Heimdal K 0.3d
  #
  # com_err		MIT K5 v1.2.2-beta1
  #
  AC_SEARCH_LIBS([com_err], [com_err])

  #
  # asn1		Heimdal K 0.3d		-lcom_err
  #
  AC_SEARCH_LIBS([initialize_asn1_error_table_r], [asn1])

  #
  # resolv		required, but not installed by Heimdal K 0.3d
  #
  # resolv		MIT K5 1.2.2-beta1
  # 			Linux 2.2.17
  #
  AC_SEARCH_LIBS([__dn_expand], [resolv])

  #
  # roken		Heimdal K 0.3d		-lresolv
  #
  AC_SEARCH_LIBS([roken_gethostbyaddr], [roken])

  #
  # k5crypto		MIT K5 v1.2.2-beta1
  #
  AC_SEARCH_LIBS([valid_enctype], [k5crypto])

  #
  # gen			? ? ?			Needed on Irix 5.3 with some
  #			Irix 5.3		version of Kerberos.  I'm not
  #						sure which since Irix didn't
  #						get any testing this time
  #						around.  Original comment:
  #
  # This is necessary on Irix 5.3, in order to link against libkrb5 --
  # there, an_to_ln.o refers to things defined only in -lgen.
  #
  AC_SEARCH_LIBS([compile], [gen])

  #
  # krb5		? ? ?			-lgen -l???
  #			Irix 5.3
  #
  # krb5		MIT K5 v1.1.1
  #
  # krb5		MIT K5 v1.2.2-beta1	-lcrypto -lcom_err
  # 			Linux 2.2.17
  #
  # krb5		MIT K5 v1.2.2-beta1	-lcrypto -lcom_err -lresolv
  #
  # krb5		Heimdal K 0.3d		-lasn1 -lroken -ldes
  #
  AC_SEARCH_LIBS([krb5_free_context], [krb5])

  #
  # gssapi_krb5		Only lib needed with MIT K5 v1.2.1, so find it first in
  #			order to prefer MIT Kerberos.  If both MIT & Heimdal
  #			Kerberos are installed and in the path, this will leave
  #			some of the libraries above in LIBS unnecessarily, but
  #			noone would ever do that, right?
  #
  # gssapi_krb5		MIT K5 v1.2.2-beta1	-lkrb5
  #
  # gssapi		Heimdal K 0.3d		-lkrb5
  #
  AC_SEARCH_LIBS([gss_import_name], [gssapi_krb5 gssapi])
fi
])dnl

# Do all the work for Automake.  This macro actually does too much --
# some checks are only needed if your package does certain things.
# But this isn't really a big deal.

# serial 5

# There are a few dirty hacks below to avoid letting `AC_PROG_CC' be
# written in clear, in which case automake, when reading aclocal.m4,
# will think it sees a *use*, and therefore will trigger all it's
# C support machinery.  Also note that it means that autoscan, seeing
# CC etc. in the Makefile, will ask for an AC_PROG_CC use...


# We require 2.13 because we rely on SHELL being computed by configure.
AC_PREREQ([2.13])

# AC_PROVIDE_IFELSE(MACRO-NAME, IF-PROVIDED, IF-NOT-PROVIDED)
# -----------------------------------------------------------
# If MACRO-NAME is provided do IF-PROVIDED, else IF-NOT-PROVIDED.
# The purpose of this macro is to provide the user with a means to
# check macros which are provided without letting her know how the
# information is coded.
# If this macro is not defined by Autoconf, define it here.
ifdef([AC_PROVIDE_IFELSE],
      [],
      [define([AC_PROVIDE_IFELSE],
              [ifdef([AC_PROVIDE_$1],
                     [$2], [$3])])])


# AM_INIT_AUTOMAKE(PACKAGE,VERSION, [NO-DEFINE])
# ----------------------------------------------
AC_DEFUN([AM_INIT_AUTOMAKE],
[AC_REQUIRE([AC_PROG_INSTALL])dnl
# test to see if srcdir already configured
if test "`CDPATH=:; cd $srcdir && pwd`" != "`pwd`" &&
   test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run \"make distclean\" there first])
fi

# Define the identity of the package.
PACKAGE=$1
AC_SUBST(PACKAGE)dnl
VERSION=$2
AC_SUBST(VERSION)dnl
ifelse([$3],,
[AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package])])

# Autoconf 2.50 wants to disallow AM_ names.  We explicitly allow
# the ones we care about.
ifdef([m4_pattern_allow],
      [m4_pattern_allow([^AM_[A-Z]+FLAGS])])dnl

# Autoconf 2.50 always computes EXEEXT.  However we need to be
# compatible with 2.13, for now.  So we always define EXEEXT, but we
# don't compute it.
AC_SUBST(EXEEXT)
# Similar for OBJEXT -- only we only use OBJEXT if the user actually
# requests that it be used.  This is a bit dumb.
: ${OBJEXT=o}
AC_SUBST(OBJEXT)

# Some tools Automake needs.
AC_REQUIRE([AM_SANITY_CHECK])dnl
AC_REQUIRE([AC_ARG_PROGRAM])dnl
AM_MISSING_PROG(ACLOCAL, aclocal)
AM_MISSING_PROG(AUTOCONF, autoconf)
AM_MISSING_PROG(AUTOMAKE, automake)
AM_MISSING_PROG(AUTOHEADER, autoheader)
AM_MISSING_PROG(MAKEINFO, makeinfo)
AM_MISSING_PROG(AMTAR, tar)
AM_PROG_INSTALL_SH
AM_PROG_INSTALL_STRIP
# We need awk for the "check" target.  The system "awk" is bad on
# some platforms.
AC_REQUIRE([AC_PROG_AWK])dnl
AC_REQUIRE([AC_PROG_MAKE_SET])dnl
AC_REQUIRE([AM_DEP_TRACK])dnl
AC_REQUIRE([AM_SET_DEPDIR])dnl
AC_PROVIDE_IFELSE([AC_PROG_][CC],
                  [_AM_DEPENDENCIES(CC)],
                  [define([AC_PROG_][CC],
                          defn([AC_PROG_][CC])[_AM_DEPENDENCIES(CC)])])dnl
AC_PROVIDE_IFELSE([AC_PROG_][CXX],
                  [_AM_DEPENDENCIES(CXX)],
                  [define([AC_PROG_][CXX],
                          defn([AC_PROG_][CXX])[_AM_DEPENDENCIES(CXX)])])dnl
])

#
# Check to make sure that the build environment is sane.
#

# serial 3

# AM_SANITY_CHECK
# ---------------
AC_DEFUN([AM_SANITY_CHECK],
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftest.file
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftest.file 2> /dev/null`
   if test "$[*]" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftest.file`
   fi
   rm -f conftest.file
   if test "$[*]" != "X $srcdir/configure conftest.file" \
      && test "$[*]" != "X conftest.file $srcdir/configure"; then

      # If neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a
      # broken ls alias from the environment.  This has actually
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
alias in your environment])
   fi

   test "$[2]" = conftest.file
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
AC_MSG_RESULT(yes)])


# serial 2

# AM_MISSING_PROG(NAME, PROGRAM)
# ------------------------------
AC_DEFUN([AM_MISSING_PROG],
[AC_REQUIRE([AM_MISSING_HAS_RUN])
$1=${$1-"${am_missing_run}$2"}
AC_SUBST($1)])


# AM_MISSING_HAS_RUN
# ------------------
# Define MISSING if not defined so far and test if it supports --run.
# If it does, set am_missing_run to use it, otherwise, to nothing.
AC_DEFUN([AM_MISSING_HAS_RUN],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
test x"${MISSING+set}" = xset || MISSING="\${SHELL} $am_aux_dir/missing"
# Use eval to expand $SHELL
if eval "$MISSING --run true"; then
  am_missing_run="$MISSING --run "
else
  am_missing_run=
  am_backtick='`'
  AC_MSG_WARN([${am_backtick}missing' script is too old or missing])
fi
])

# AM_AUX_DIR_EXPAND

# For projects using AC_CONFIG_AUX_DIR([foo]), Autoconf sets
# $ac_aux_dir to `$srcdir/foo'.  In other projects, it is set to
# `$srcdir', `$srcdir/..', or `$srcdir/../..'.
#
# Of course, Automake must honor this variable whenever it calls a
# tool from the auxiliary directory.  The problem is that $srcdir (and
# therefore $ac_aux_dir as well) can be either absolute or relative,
# depending on how configure is run.  This is pretty annoying, since
# it makes $ac_aux_dir quite unusable in subdirectories: in the top
# source directory, any form will work fine, but in subdirectories a
# relative path needs to be adjusted first.
#
# $ac_aux_dir/missing
#    fails when called from a subdirectory if $ac_aux_dir is relative
# $top_srcdir/$ac_aux_dir/missing
#    fails if $ac_aux_dir is absolute,
#    fails when called from a subdirectory in a VPATH build with
#          a relative $ac_aux_dir
#
# The reason of the latter failure is that $top_srcdir and $ac_aux_dir
# are both prefixed by $srcdir.  In an in-source build this is usually
# harmless because $srcdir is `.', but things will broke when you
# start a VPATH build or use an absolute $srcdir.
#
# So we could use something similar to $top_srcdir/$ac_aux_dir/missing,
# iff we strip the leading $srcdir from $ac_aux_dir.  That would be:
#   am_aux_dir='\$(top_srcdir)/'`expr "$ac_aux_dir" : "$srcdir//*\(.*\)"`
# and then we would define $MISSING as
#   MISSING="\${SHELL} $am_aux_dir/missing"
# This will work as long as MISSING is not called from configure, because
# unfortunately $(top_srcdir) has no meaning in configure.
# However there are other variables, like CC, which are often used in
# configure, and could therefore not use this "fixed" $ac_aux_dir.
#
# Another solution, used here, is to always expand $ac_aux_dir to an
# absolute PATH.  The drawback is that using absolute paths prevent a
# configured tree to be moved without reconfiguration.

AC_DEFUN([AM_AUX_DIR_EXPAND], [
# expand $ac_aux_dir to an absolute path
am_aux_dir=`CDPATH=:; cd $ac_aux_dir && pwd`
])

# AM_PROG_INSTALL_SH
# ------------------
# Define $install_sh.
AC_DEFUN([AM_PROG_INSTALL_SH],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
install_sh=${install_sh-"$am_aux_dir/install-sh"}
AC_SUBST(install_sh)])

# One issue with vendor `install' (even GNU) is that you can't
# specify the program used to strip binaries.  This is especially
# annoying in cross-compiling environments, where the build's strip
# is unlikely to handle the host's binaries.
# Fortunately install-sh will honor a STRIPPROG variable, so we
# always use install-sh in `make install-strip', and initialize
# STRIPPROG with the value of the STRIP variable (set by the user).
AC_DEFUN([AM_PROG_INSTALL_STRIP],
[AC_REQUIRE([AM_PROG_INSTALL_SH])dnl
INSTALL_STRIP_PROGRAM="\${SHELL} \$(install_sh) -c -s"
AC_SUBST([INSTALL_STRIP_PROGRAM])])

# serial 4						-*- Autoconf -*-



# There are a few dirty hacks below to avoid letting `AC_PROG_CC' be
# written in clear, in which case automake, when reading aclocal.m4,
# will think it sees a *use*, and therefore will trigger all it's
# C support machinery.  Also note that it means that autoscan, seeing
# CC etc. in the Makefile, will ask for an AC_PROG_CC use...



# _AM_DEPENDENCIES(NAME)
# ---------------------
# See how the compiler implements dependency checking.
# NAME is "CC", "CXX" or "OBJC".
# We try a few techniques and use that to set a single cache variable.
#
# We don't AC_REQUIRE the corresponding AC_PROG_CC since the latter was
# modified to invoke _AM_DEPENDENCIES(CC); we would have a circular
# dependency, and given that the user is not expected to run this macro,
# just rely on AC_PROG_CC.
AC_DEFUN([_AM_DEPENDENCIES],
[AC_REQUIRE([AM_SET_DEPDIR])dnl
AC_REQUIRE([AM_OUTPUT_DEPENDENCY_COMMANDS])dnl
AC_REQUIRE([AM_MAKE_INCLUDE])dnl
AC_REQUIRE([AM_DEP_TRACK])dnl

ifelse([$1], CC,   [depcc="$CC"   am_compiler_list=],
       [$1], CXX,  [depcc="$CXX"  am_compiler_list=],
       [$1], OBJC, [depcc="$OBJC" am_compiler_list='gcc3 gcc']
       [$1], GCJ,  [depcc="$GCJ"  am_compiler_list='gcc3 gcc'],
                   [depcc="$$1"   am_compiler_list=])

AC_CACHE_CHECK([dependency style of $depcc],
               [am_cv_$1_dependencies_compiler_type],
[if test -z "$AMDEP_TRUE" && test -f "$am_depcomp"; then
  # We make a subdir and do the tests there.  Otherwise we can end up
  # making bogus files that we don't know about and never remove.  For
  # instance it was reported that on HP-UX the gcc test will end up
  # making a dummy file named `D' -- because `-MD' means `put the output
  # in D'.
  mkdir conftest.dir
  # Copy depcomp to subdir because otherwise we won't find it if we're
  # using a relative directory.
  cp "$am_depcomp" conftest.dir
  cd conftest.dir

  am_cv_$1_dependencies_compiler_type=none
  if test "$am_compiler_list" = ""; then
     am_compiler_list=`sed -n ['s/^#*\([a-zA-Z0-9]*\))$/\1/p'] < ./depcomp`
  fi
  for depmode in $am_compiler_list; do
    # We need to recreate these files for each test, as the compiler may
    # overwrite some of them when testing with obscure command lines.
    # This happens at least with the AIX C compiler.
    echo '#include "conftest.h"' > conftest.c
    echo 'int i;' > conftest.h
    echo "${am__include} ${am__quote}conftest.Po${am__quote}" > confmf

    case $depmode in
    nosideeffect)
      # after this tag, mechanisms are not by side-effect, so they'll
      # only be used when explicitly requested
      if test "x$enable_dependency_tracking" = xyes; then
	continue
      else
	break
      fi
      ;;
    none) break ;;
    esac
    # We check with `-c' and `-o' for the sake of the "dashmstdout"
    # mode.  It turns out that the SunPro C++ compiler does not properly
    # handle `-M -o', and we need to detect this.
    if depmode=$depmode \
       source=conftest.c object=conftest.o \
       depfile=conftest.Po tmpdepfile=conftest.TPo \
       $SHELL ./depcomp $depcc -c conftest.c -o conftest.o >/dev/null 2>&1 &&
       grep conftest.h conftest.Po > /dev/null 2>&1 &&
       ${MAKE-make} -s -f confmf > /dev/null 2>&1; then
      am_cv_$1_dependencies_compiler_type=$depmode
      break
    fi
  done

  cd ..
  rm -rf conftest.dir
else
  am_cv_$1_dependencies_compiler_type=none
fi
])
$1DEPMODE="depmode=$am_cv_$1_dependencies_compiler_type"
AC_SUBST([$1DEPMODE])
])


# AM_SET_DEPDIR
# -------------
# Choose a directory name for dependency files.
# This macro is AC_REQUIREd in _AM_DEPENDENCIES
AC_DEFUN([AM_SET_DEPDIR],
[rm -f .deps 2>/dev/null
mkdir .deps 2>/dev/null
if test -d .deps; then
  DEPDIR=.deps
else
  # MS-DOS does not allow filenames that begin with a dot.
  DEPDIR=_deps
fi
rmdir .deps 2>/dev/null
AC_SUBST(DEPDIR)
])


# AM_DEP_TRACK
# ------------
AC_DEFUN([AM_DEP_TRACK],
[AC_ARG_ENABLE(dependency-tracking,
[  --disable-dependency-tracking Speeds up one-time builds
  --enable-dependency-tracking  Do not reject slow dependency extractors])
if test "x$enable_dependency_tracking" != xno; then
  am_depcomp="$ac_aux_dir/depcomp"
  AMDEPBACKSLASH='\'
fi
AM_CONDITIONAL([AMDEP], [test "x$enable_dependency_tracking" != xno])
pushdef([subst], defn([AC_SUBST]))
subst(AMDEPBACKSLASH)
popdef([subst])
])

# Generate code to set up dependency tracking.
# This macro should only be invoked once -- use via AC_REQUIRE.
# Usage:
# AM_OUTPUT_DEPENDENCY_COMMANDS

#
# This code is only required when automatic dependency tracking
# is enabled.  FIXME.  This creates each `.P' file that we will
# need in order to bootstrap the dependency handling code.
AC_DEFUN([AM_OUTPUT_DEPENDENCY_COMMANDS],[
AC_OUTPUT_COMMANDS([
test x"$AMDEP_TRUE" != x"" ||
for mf in $CONFIG_FILES; do
  case "$mf" in
  Makefile) dirpart=.;;
  */Makefile) dirpart=`echo "$mf" | sed -e 's|/[^/]*$||'`;;
  *) continue;;
  esac
  grep '^DEP_FILES *= *[^ #]' < "$mf" > /dev/null || continue
  # Extract the definition of DEP_FILES from the Makefile without
  # running `make'.
  DEPDIR=`sed -n -e '/^DEPDIR = / s///p' < "$mf"`
  test -z "$DEPDIR" && continue
  # When using ansi2knr, U may be empty or an underscore; expand it
  U=`sed -n -e '/^U = / s///p' < "$mf"`
  test -d "$dirpart/$DEPDIR" || mkdir "$dirpart/$DEPDIR"
  # We invoke sed twice because it is the simplest approach to
  # changing $(DEPDIR) to its actual value in the expansion.
  for file in `sed -n -e '
    /^DEP_FILES = .*\\\\$/ {
      s/^DEP_FILES = //
      :loop
	s/\\\\$//
	p
	n
	/\\\\$/ b loop
      p
    }
    /^DEP_FILES = / s/^DEP_FILES = //p' < "$mf" | \
       sed -e 's/\$(DEPDIR)/'"$DEPDIR"'/g' -e 's/\$U/'"$U"'/g'`; do
    # Make sure the directory exists.
    test -f "$dirpart/$file" && continue
    fdir=`echo "$file" | sed -e 's|/[^/]*$||'`
    $ac_aux_dir/mkinstalldirs "$dirpart/$fdir" > /dev/null 2>&1
    # echo "creating $dirpart/$file"
    echo '# dummy' > "$dirpart/$file"
  done
done
], [AMDEP_TRUE="$AMDEP_TRUE"
ac_aux_dir="$ac_aux_dir"])])

# AM_MAKE_INCLUDE()
# -----------------
# Check to see how make treats includes.
AC_DEFUN([AM_MAKE_INCLUDE],
[am_make=${MAKE-make}
cat > confinc << 'END'
doit:
	@echo done
END
# If we don't find an include directive, just comment out the code.
AC_MSG_CHECKING([for style of include used by $am_make])
am__include='#'
am__quote=
_am_result=none
# First try GNU make style include.
echo "include confinc" > confmf
# We grep out `Entering directory' and `Leaving directory'
# messages which can occur if `w' ends up in MAKEFLAGS.
# In particular we don't look at `^make:' because GNU make might
# be invoked under some other name (usually "gmake"), in which
# case it prints its new name instead of `make'.
if test "`$am_make -s -f confmf 2> /dev/null | fgrep -v 'ing directory'`" = "done"; then
   am__include=include
   am__quote=
   _am_result=GNU
fi
# Now try BSD make style include.
if test "$am__include" = "#"; then
   echo '.include "confinc"' > confmf
   if test "`$am_make -s -f confmf 2> /dev/null`" = "done"; then
      am__include=.include
      am__quote='"'
      _am_result=BSD
   fi
fi
AC_SUBST(am__include)
AC_SUBST(am__quote)
AC_MSG_RESULT($_am_result)
rm -f confinc confmf
])

# serial 3

# AM_CONDITIONAL(NAME, SHELL-CONDITION)
# -------------------------------------
# Define a conditional.
#
# FIXME: Once using 2.50, use this:
# m4_match([$1], [^TRUE\|FALSE$], [AC_FATAL([$0: invalid condition: $1])])dnl
AC_DEFUN([AM_CONDITIONAL],
[ifelse([$1], [TRUE],
        [errprint(__file__:__line__: [$0: invalid condition: $1
])dnl
m4exit(1)])dnl
ifelse([$1], [FALSE],
       [errprint(__file__:__line__: [$0: invalid condition: $1
])dnl
m4exit(1)])dnl
AC_SUBST([$1_TRUE])
AC_SUBST([$1_FALSE])
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])

# Like AC_CONFIG_HEADER, but automatically create stamp file.

# serial 3

# When config.status generates a header, we must update the stamp-h file.
# This file resides in the same directory as the config header
# that is generated.  We must strip everything past the first ":",
# and everything past the last "/".

AC_PREREQ([2.12])

AC_DEFUN([AM_CONFIG_HEADER],
[ifdef([AC_FOREACH],dnl
	 [dnl init our file count if it isn't already
	 m4_ifndef([_AM_Config_Header_Index], m4_define([_AM_Config_Header_Index], [0]))
	 dnl prepare to store our destination file list for use in config.status
	 AC_FOREACH([_AM_File], [$1],
		    [m4_pushdef([_AM_Dest], m4_patsubst(_AM_File, [:.*]))
		    m4_define([_AM_Config_Header_Index], m4_incr(_AM_Config_Header_Index))
		    dnl and add it to the list of files AC keeps track of, along
		    dnl with our hook
		    AC_CONFIG_HEADERS(_AM_File,
dnl COMMANDS, [, INIT-CMDS]
[# update the timestamp
echo timestamp >"AS_ESCAPE(_AM_DIRNAME(]_AM_Dest[))/stamp-h]_AM_Config_Header_Index["
][$2]m4_ifval([$3], [, [$3]]))dnl AC_CONFIG_HEADERS
		    m4_popdef([_AM_Dest])])],dnl
[AC_CONFIG_HEADER([$1])
  AC_OUTPUT_COMMANDS(
   ifelse(patsubst([$1], [[^ ]], []),
	  [],
	  [test -z "$CONFIG_HEADERS" || echo timestamp >dnl
	   patsubst([$1], [^\([^:]*/\)?.*], [\1])stamp-h]),dnl
[am_indx=1
for am_file in $1; do
  case " \$CONFIG_HEADERS " in
  *" \$am_file "*)
    am_dir=\`echo \$am_file |sed 's%:.*%%;s%[^/]*\$%%'\`
    if test -n "\$am_dir"; then
      am_tmpdir=\`echo \$am_dir |sed 's%^\(/*\).*\$%\1%'\`
      for am_subdir in \`echo \$am_dir |sed 's%/% %'\`; do
        am_tmpdir=\$am_tmpdir\$am_subdir/
        if test ! -d \$am_tmpdir; then
          mkdir \$am_tmpdir
        fi
      done
    fi
    echo timestamp > "\$am_dir"stamp-h\$am_indx
    ;;
  esac
  am_indx=\`expr \$am_indx + 1\`
done])
])]) # AM_CONFIG_HEADER

# _AM_DIRNAME(PATH)
# -----------------
# Like AS_DIRNAME, only do it during macro expansion
AC_DEFUN([_AM_DIRNAME],
       [m4_if(m4_regexp([$1], [^.*[^/]//*[^/][^/]*/*$]), -1,
	      m4_if(m4_regexp([$1], [^//\([^/]\|$\)]), -1,
		    m4_if(m4_regexp([$1], [^/.*]), -1,
			  [.],
			  m4_patsubst([$1], [^\(/\).*], [\1])),
		    m4_patsubst([$1], [^\(//\)\([^/].*\|$\)], [\1])),
	      m4_patsubst([$1], [^\(.*[^/]\)//*[^/][^/]*/*$], [\1]))[]dnl
]) # _AM_DIRNAME

# serial 2

# AM_PROG_CC_C_O
# --------------
# Like AC_PROG_CC_C_O, but changed for automake.
AC_DEFUN([AM_PROG_CC_C_O],
[AC_REQUIRE([AC_PROG_CC_C_O])dnl
AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
# FIXME: we rely on the cache variable name because
# there is no other way.
set dummy $CC
ac_cc=`echo $[2] | sed ['s/[^a-zA-Z0-9_]/_/g;s/^[0-9]/_/']`
if eval "test \"`echo '$ac_cv_prog_cc_'${ac_cc}_c_o`\" != yes"; then
   # Losing compiler, so override with the script.
   # FIXME: It is wrong to rewrite CC.
   # But if we don't then we get into trouble of one sort or another.
   # A longer-term fix would be to have automake use am__CC in this case,
   # and then we could set am__CC="\$(top_srcdir)/compile \$(CC)"
   CC="$am_aux_dir/compile $CC"
fi
])

#serial 1
# This test replaces the one in autoconf.
# Currently this macro should have the same name as the autoconf macro
# because gettext's gettext.m4 (distributed in the automake package)
# still uses it.  Otherwise, the use in gettext.m4 makes autoheader
# give these diagnostics:
#   configure.in:556: AC_TRY_COMPILE was called before AC_ISC_POSIX
#   configure.in:556: AC_TRY_RUN was called before AC_ISC_POSIX

undefine([AC_ISC_POSIX])

AC_DEFUN([AC_ISC_POSIX],
  [
    dnl This test replaces the obsolescent AC_ISC_POSIX kludge.
    AC_CHECK_LIB(cposix, strerror, [LIBS="$LIBS -lcposix"])
  ]
)

