# ./aclocal.m4 generated automatically by aclocal 1.5

# Copyright 1996, 1997, 1998, 1999, 2000, 2001
# Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

dnl $Id: acinclude.m4,v 1.15 1998/05/23 14:54:53 joda Exp $
dnl
dnl Only put things that for some reason can't live in the `cf'
dnl directory in this file.
dnl

dnl $xId: misc.m4,v 1.1 1997/12/14 15:59:04 joda Exp $
dnl
define(upcase,`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`)dnl

dnl $Id: misc.m4,v 1.2 2000/07/19 15:04:00 joda Exp $
dnl
AC_DEFUN([upcase],[`echo $1 | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ`])dnl
AC_DEFUN([rk_CONFIG_HEADER],[AH_TOP([#ifndef RCSID
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (const char *)rcsid, "\100(#)" msg }
#endif

#undef BINDIR 
#undef LIBDIR
#undef LIBEXECDIR
#undef SBINDIR

#undef HAVE_INT8_T
#undef HAVE_INT16_T
#undef HAVE_INT32_T
#undef HAVE_INT64_T
#undef HAVE_U_INT8_T
#undef HAVE_U_INT16_T
#undef HAVE_U_INT32_T
#undef HAVE_U_INT64_T

/* Maximum values on all known systems */
#define MaxHostNameLen (64+4)
#define MaxPathLen (1024+4)

])])
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


# AM_PROG_LEX
# Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN([AM_PROG_LEX],
[AC_REQUIRE([AM_MISSING_HAS_RUN])
AC_CHECK_PROGS(LEX, flex lex, [${am_missing_run}flex])
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


dnl $Id: mips-abi.m4,v 1.5 2000/07/18 15:01:42 joda Exp $
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
         *) AC_MSG_ERROR("Invalid ABI specified") ;;
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
		AC_MSG_ERROR([$CC does not support the $with_mips_abi ABI])
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
			AC_MSG_ERROR([$CC does not support the $with_mips_abi ABI])
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
         *) AC_MSG_ERROR("Invalid ABI specified") ;;
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



# serial 40 AC_PROG_LIBTOOL
AC_DEFUN(AC_PROG_LIBTOOL,
[AC_REQUIRE([AC_LIBTOOL_SETUP])dnl

# Save cache, so that ltconfig can load it
AC_CACHE_SAVE

# Actually configure libtool.  ac_aux_dir is where install-sh is found.
CC="$CC" CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" \
LD="$LD" LDFLAGS="$LDFLAGS" LIBS="$LIBS" \
LN_S="$LN_S" NM="$NM" RANLIB="$RANLIB" \
DLLTOOL="$DLLTOOL" AS="$AS" OBJDUMP="$OBJDUMP" \
${CONFIG_SHELL-/bin/sh} $ac_aux_dir/ltconfig --no-reexec \
$libtool_flags --no-verify $ac_aux_dir/ltmain.sh $lt_target \
|| AC_MSG_ERROR([libtool configure failed])

# Reload cache, that may have been modified by ltconfig
AC_CACHE_LOAD

# This can be used to rebuild libtool when needed
LIBTOOL_DEPS="$ac_aux_dir/ltconfig $ac_aux_dir/ltmain.sh"

# Always use our own libtool.
LIBTOOL='$(SHELL) $(top_builddir)/libtool'
AC_SUBST(LIBTOOL)dnl

# Redirect the config.log output again, so that the ltconfig log is not
# clobbered by the next message.
exec 5>>./config.log
])

AC_DEFUN(AC_LIBTOOL_SETUP,
[AC_PREREQ(2.13)dnl
AC_REQUIRE([AC_ENABLE_SHARED])dnl
AC_REQUIRE([AC_ENABLE_STATIC])dnl
AC_REQUIRE([AC_ENABLE_FAST_INSTALL])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
AC_REQUIRE([AC_PROG_RANLIB])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_LD])dnl
AC_REQUIRE([AC_PROG_NM])dnl
AC_REQUIRE([AC_PROG_LN_S])dnl
dnl

case "$target" in
NONE) lt_target="$host" ;;
*) lt_target="$target" ;;
esac

# Check for any special flags to pass to ltconfig.
libtool_flags="--cache-file=$cache_file"
test "$enable_shared" = no && libtool_flags="$libtool_flags --disable-shared"
test "$enable_static" = no && libtool_flags="$libtool_flags --disable-static"
test "$enable_fast_install" = no && libtool_flags="$libtool_flags --disable-fast-install"
test "$ac_cv_prog_gcc" = yes && libtool_flags="$libtool_flags --with-gcc"
test "$ac_cv_prog_gnu_ld" = yes && libtool_flags="$libtool_flags --with-gnu-ld"
ifdef([AC_PROVIDE_AC_LIBTOOL_DLOPEN],
[libtool_flags="$libtool_flags --enable-dlopen"])
ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
[libtool_flags="$libtool_flags --enable-win32-dll"])
AC_ARG_ENABLE(libtool-lock,
  [  --disable-libtool-lock  avoid locking (might break parallel builds)])
test "x$enable_libtool_lock" = xno && libtool_flags="$libtool_flags --disable-lock"
test x"$silent" = xyes && libtool_flags="$libtool_flags --silent"

# Some flags need to be propagated to the compiler or linker for good
# libtool support.
case "$lt_target" in
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
  SAVE_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -belf"
  AC_CACHE_CHECK([whether the C compiler needs -belf], lt_cv_cc_needs_belf,
    [AC_TRY_LINK([],[],[lt_cv_cc_needs_belf=yes],[lt_cv_cc_needs_belf=no])])
  if test x"$lt_cv_cc_needs_belf" != x"yes"; then
    # this is probably gcc 2.8.0, egcs 1.0 or newer; no need for -belf
    CFLAGS="$SAVE_CFLAGS"
  fi
  ;;

ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
[*-*-cygwin* | *-*-mingw*)
  AC_CHECK_TOOL(DLLTOOL, dlltool, false)
  AC_CHECK_TOOL(AS, as, false)
  AC_CHECK_TOOL(OBJDUMP, objdump, false)
  ;;
])
esac
])

# AC_LIBTOOL_DLOPEN - enable checks for dlopen support
AC_DEFUN(AC_LIBTOOL_DLOPEN, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])])

# AC_LIBTOOL_WIN32_DLL - declare package support for building win32 dll's
AC_DEFUN(AC_LIBTOOL_WIN32_DLL, [AC_BEFORE([$0], [AC_LIBTOOL_SETUP])])

# AC_ENABLE_SHARED - implement the --enable-shared flag
# Usage: AC_ENABLE_SHARED[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_SHARED, [dnl
define([AC_ENABLE_SHARED_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(shared,
changequote(<<, >>)dnl
<<  --enable-shared[=PKGS]  build shared libraries [default=>>AC_ENABLE_SHARED_DEFAULT],
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
enable_shared=AC_ENABLE_SHARED_DEFAULT)dnl
])

# AC_DISABLE_SHARED - set the default shared flag to --disable-shared
AC_DEFUN(AC_DISABLE_SHARED, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_SHARED(no)])

# AC_ENABLE_STATIC - implement the --enable-static flag
# Usage: AC_ENABLE_STATIC[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_STATIC, [dnl
define([AC_ENABLE_STATIC_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(static,
changequote(<<, >>)dnl
<<  --enable-static[=PKGS]  build static libraries [default=>>AC_ENABLE_STATIC_DEFAULT],
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
enable_static=AC_ENABLE_STATIC_DEFAULT)dnl
])

# AC_DISABLE_STATIC - set the default static flag to --disable-static
AC_DEFUN(AC_DISABLE_STATIC, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_STATIC(no)])


# AC_ENABLE_FAST_INSTALL - implement the --enable-fast-install flag
# Usage: AC_ENABLE_FAST_INSTALL[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_FAST_INSTALL, [dnl
define([AC_ENABLE_FAST_INSTALL_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(fast-install,
changequote(<<, >>)dnl
<<  --enable-fast-install[=PKGS]  optimize for fast installation [default=>>AC_ENABLE_FAST_INSTALL_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_fast_install=yes ;;
no) enable_fast_install=no ;;
*)
  enable_fast_install=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_fast_install=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_fast_install=AC_ENABLE_FAST_INSTALL_DEFAULT)dnl
])

# AC_ENABLE_FAST_INSTALL - set the default to --disable-fast-install
AC_DEFUN(AC_DISABLE_FAST_INSTALL, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_FAST_INSTALL(no)])

# AC_PROG_LD - find the path to the GNU or non-GNU linker
AC_DEFUN(AC_PROG_LD,
[AC_ARG_WITH(gnu-ld,
[  --with-gnu-ld           assume the C compiler uses GNU ld [default=no]],
test "$withval" = no || with_gnu_ld=yes, with_gnu_ld=no)
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
ac_prog=ld
if test "$ac_cv_prog_gcc" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by GCC])
  ac_prog=`($CC -print-prog-name=ld) 2>&5`
  case "$ac_prog" in
    # Accept absolute paths.
changequote(,)dnl
    [\\/]* | [A-Za-z]:[\\/]*)
      re_direlt='/[^/][^/]*/\.\./'
changequote([,])dnl
      # Canonicalize the path of ld
      ac_prog=`echo $ac_prog| sed 's%\\\\%/%g'`
      while echo $ac_prog | grep "$re_direlt" > /dev/null 2>&1; do
	ac_prog=`echo $ac_prog| sed "s%$re_direlt%/%"`
      done
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
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH; do
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
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
AC_PROG_LD_GNU
])

AC_DEFUN(AC_PROG_LD_GNU,
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], ac_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  ac_cv_prog_gnu_ld=yes
else
  ac_cv_prog_gnu_ld=no
fi])
])

# AC_PROG_NM - find the path to a BSD-compatible name lister
AC_DEFUN(AC_PROG_NM,
[AC_MSG_CHECKING([for BSD-compatible nm])
AC_CACHE_VAL(ac_cv_path_NM,
[if test -n "$NM"; then
  # Let the user override the test.
  ac_cv_path_NM="$NM"
else
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH /usr/ccs/bin /usr/ucb /bin; do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/nm || test -f $ac_dir/nm$ac_exeext ; then
      # Check to see if the nm accepts a BSD-compat flag.
      # Adding the `sed 1q' prevents false positives on HP-UX, which says:
      #   nm: unknown option "B" ignored
      if ($ac_dir/nm -B /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
	ac_cv_path_NM="$ac_dir/nm -B"
	break
      elif ($ac_dir/nm -p /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
	ac_cv_path_NM="$ac_dir/nm -p"
	break
      else
	ac_cv_path_NM=${ac_cv_path_NM="$ac_dir/nm"} # keep the first match, but
	continue # so that we can try to find one that supports BSD flags
      fi
    fi
  done
  IFS="$ac_save_ifs"
  test -z "$ac_cv_path_NM" && ac_cv_path_NM=nm
fi])
NM="$ac_cv_path_NM"
AC_MSG_RESULT([$NM])
])

# AC_CHECK_LIBM - check for math library
AC_DEFUN(AC_CHECK_LIBM,
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
LIBM=
case "$lt_target" in
*-*-beos* | *-*-cygwin*)
  # These system don't have libm
  ;;
*-ncr-sysv4.3*)
  AC_CHECK_LIB(mw, _mwvalidcheckl, LIBM="-lmw")
  AC_CHECK_LIB(m, main, LIBM="$LIBM -lm")
  ;;
*)
  AC_CHECK_LIB(m, main, LIBM="-lm")
  ;;
esac
])

# AC_LIBLTDL_CONVENIENCE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl convenience library, adds --enable-ltdl-convenience to
# the configure arguments.  Note that LIBLTDL is not AC_SUBSTed, nor
# is AC_CONFIG_SUBDIRS called.  If DIR is not provided, it is assumed
# to be `${top_builddir}/libltdl'.  Make sure you start DIR with
# '${top_builddir}/' (note the single quotes!) if your package is not
# flat, and, if you're not using automake, define top_builddir as
# appropriate in the Makefiles.
AC_DEFUN(AC_LIBLTDL_CONVENIENCE, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  case "$enable_ltdl_convenience" in
  no) AC_MSG_ERROR([this package needs a convenience libltdl]) ;;
  "") enable_ltdl_convenience=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-convenience" ;;
  esac
  LIBLTDL=ifelse($#,1,$1,['${top_builddir}/libltdl'])/libltdlc.la
  INCLTDL=ifelse($#,1,-I$1,['-I${top_builddir}/libltdl'])
])

# AC_LIBLTDL_INSTALLABLE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl installable library, and adds --enable-ltdl-install to
# the configure arguments.  Note that LIBLTDL is not AC_SUBSTed, nor
# is AC_CONFIG_SUBDIRS called.  If DIR is not provided, it is assumed
# to be `${top_builddir}/libltdl'.  Make sure you start DIR with
# '${top_builddir}/' (note the single quotes!) if your package is not
# flat, and, if you're not using automake, define top_builddir as
# appropriate in the Makefiles.
# In the future, this macro may have to be called after AC_PROG_LIBTOOL.
AC_DEFUN(AC_LIBLTDL_INSTALLABLE, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  AC_CHECK_LIB(ltdl, main,
  [test x"$enable_ltdl_install" != xyes && enable_ltdl_install=no],
  [if test x"$enable_ltdl_install" = xno; then
     AC_MSG_WARN([libltdl not installed, but installation disabled])
   else
     enable_ltdl_install=yes
   fi
  ])
  if test x"$enable_ltdl_install" = x"yes"; then
    ac_configure_args="$ac_configure_args --enable-ltdl-install"
    LIBLTDL=ifelse($#,1,$1,['${top_builddir}/libltdl'])/libltdl.la
    INCLTDL=ifelse($#,1,-I$1,['-I${top_builddir}/libltdl'])
  else
    ac_configure_args="$ac_configure_args --enable-ltdl-install=no"
    LIBLTDL="-lltdl"
    INCLTDL=
  fi
])

dnl old names
AC_DEFUN(AM_PROG_LIBTOOL, [indir([AC_PROG_LIBTOOL])])dnl
AC_DEFUN(AM_ENABLE_SHARED, [indir([AC_ENABLE_SHARED], $@)])dnl
AC_DEFUN(AM_ENABLE_STATIC, [indir([AC_ENABLE_STATIC], $@)])dnl
AC_DEFUN(AM_DISABLE_SHARED, [indir([AC_DISABLE_SHARED], $@)])dnl
AC_DEFUN(AM_DISABLE_STATIC, [indir([AC_DISABLE_STATIC], $@)])dnl
AC_DEFUN(AM_PROG_LD, [indir([AC_PROG_LD])])dnl
AC_DEFUN(AM_PROG_NM, [indir([AC_PROG_NM])])dnl

dnl This is just to silence aclocal about the macro not being used
ifelse([AC_DISABLE_FAST_INSTALL])dnl

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

dnl $Id: db.m4,v 1.5 2001/09/13 00:34:07 assar Exp $
dnl
dnl tests for various db libraries
dnl
AC_DEFUN([rk_DB],[berkeley_db=db
AC_ARG_WITH(berkeley-db,
[  --without-berkeley-db   if you don't want berkeley db],[
if test "$withval" = no; then
	berkeley_db=""
fi
])

have_ndbm=no
db_type=unknown

if test "$berkeley_db"; then

  AC_CHECK_HEADERS([				\
	db.h					\
	db_185.h				\
  ])

dnl db_create is used by db3

  AC_FIND_FUNC_NO_LIBS(db_create, $berkeley_db, [
  #include <stdio.h>
  #include <db.h>
  ],[NULL, NULL, 0])

  if test "$ac_cv_func_db_create" = "yes"; then
    db_type=db3
    if test "$ac_cv_funclib_db_create" != "yes"; then
      DBLIB="$ac_cv_funclib_db_create"
    else
      DBLIB=""
    fi
    AC_DEFINE(HAVE_DB3, 1, [define if you have a berkeley db3 library])
  else

dnl dbopen is used by db1/db2

    AC_FIND_FUNC_NO_LIBS(dbopen, $berkeley_db, [
    #include <stdio.h>
    #if defined(HAVE_DB_185_H)
    #include <db_185.h>
    #elif defined(HAVE_DB_H)
    #include <db.h>
    #else
    #error no db.h
    #endif
    ],[NULL, 0, 0, 0, NULL])

    if test "$ac_cv_func_dbopen" = "yes"; then
      db_type=db1
      if test "$ac_cv_funclib_dbopen" != "yes"; then
        DBLIB="$ac_cv_funclib_dbopen"
      else
        DBLIB=""
      fi
      AC_DEFINE(HAVE_DB1, 1, [define if you have a berkeley db1/2 library])
    fi
  fi

dnl test for ndbm compatability

  AC_FIND_FUNC_NO_LIBS2(dbm_firstkey, $ac_cv_funclib_dbopen $ac_cv_funclib_db_create, [
  #include <stdio.h>
  #define DB_DBM_HSEARCH 1
  #include <db.h>
  DBM *dbm;
  ],[NULL])

  if test "$ac_cv_func_dbm_firstkey" = "yes"; then
    if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
      LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
    else
      LIB_NDBM=""
    fi
    AC_DEFINE(HAVE_DB_NDBM, 1, [define if you have ndbm compat in db])
    AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files *.db)])
  else
    $as_unset ac_cv_func_dbm_firstkey
    $as_unset ac_cv_funclib_dbm_firstkey
  fi

fi # berkeley db

if test "$db_type" = "unknown" -o "$ac_cv_func_dbm_firstkey" = ""; then

  AC_CHECK_HEADERS([				\
	dbm.h					\
	ndbm.h					\
  ])

  AC_FIND_FUNC_NO_LIBS(dbm_firstkey, ndbm, [
  #include <stdio.h>
  #if defined(HAVE_NDBM_H)
  #include <ndbm.h>
  #elif defined(HAVE_DBM_H)
  #include <dbm.h>
  #else
  #error no ndbm.h
  #endif
  DBM *dbm;
  ],[NULL])

  if test "$ac_cv_func_dbm_firstkey" = "yes"; then
    if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
      LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
    else
      LIB_NDBM=""
    fi
    AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
    have_ndbm=yes
    if test "$db_type" = "unknown"; then
      db_type=ndbm
      DBLIB="$LIB_NDBM"
    fi
  else

    $as_unset ac_cv_func_dbm_firstkey
    $as_unset ac_cv_funclib_dbm_firstkey

    AC_CHECK_HEADERS([				\
	  gdbm/ndbm.h				\
    ])

    AC_FIND_FUNC_NO_LIBS(dbm_firstkey, gdbm, [
    #include <stdio.h>
    #include <gdbm/ndbm.h>
    DBM *dbm;
    ],[NULL])

    if test "$ac_cv_func_dbm_firstkey" = "yes"; then
      if test "$ac_cv_funclib_dbm_firstkey" != "yes"; then
	LIB_NDBM="$ac_cv_funclib_dbm_firstkey"
      else
	LIB_NDBM=""
      fi
      AC_DEFINE(HAVE_NDBM, 1, [define if you have a ndbm library])dnl
      have_ndbm=yes
      if test "$db_type" = "unknown"; then
	db_type=ndbm
	DBLIB="$LIB_NDBM"
      fi
    fi
  fi

fi # unknown

if test "$have_ndbm" = "yes"; then
  AC_MSG_CHECKING([if ndbm is implemented with db])
  AC_TRY_RUN([
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined(HAVE_NDBM_H)
#include <ndbm.h>
#elif defined(HAVE_DBM_H)
#include <dbm.h>
#endif
int main()
{
  DBM *d;

  d = dbm_open("conftest", O_RDWR | O_CREAT, 0666);
  if (d == NULL)
    return 1;
  dbm_close(d);
  return 0;
}],[
    if test -f conftest.db; then
      AC_MSG_RESULT([yes])
      AC_DEFINE(HAVE_NEW_DB, 1, [Define if NDBM really is DB (creates files *.db)])
    else
      AC_MSG_RESULT([no])
    fi],[AC_MSG_RESULT([no])])
fi

AC_SUBST(DBLIB)dnl
AC_SUBST(LIB_NDBM)dnl
])

dnl $Id: find-func-no-libs.m4,v 1.5 1999/10/30 21:08:18 assar Exp $
dnl
dnl
dnl Look for function in any of the specified libraries
dnl

dnl AC_FIND_FUNC_NO_LIBS(func, libraries, includes, arguments, extra libs, extra args)
AC_DEFUN(AC_FIND_FUNC_NO_LIBS, [
AC_FIND_FUNC_NO_LIBS2([$1], ["" $2], [$3], [$4], [$5], [$6])])

dnl $Id: find-func-no-libs2.m4,v 1.6 2001/09/01 10:57:32 assar Exp $
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
		case "$ac_lib" in
		"") ;;
		yes) ac_lib="" ;;
		no) continue ;;
		-l*) ;;
		*) ac_lib="-l$ac_lib" ;;
		esac
		LIBS="$6 $ac_lib $5 $ac_save_LIBS"
		AC_TRY_LINK([$3],[$1($4)],eval "if test -n \"$ac_lib\";then ac_cv_funclib_$1=$ac_lib; else ac_cv_funclib_$1=yes; fi";break)
	done
	eval "ac_cv_funclib_$1=\${ac_cv_funclib_$1-no}"
	LIBS="$ac_save_LIBS"
fi
])

eval "ac_res=\$ac_cv_funclib_$1"

if false; then
	AC_CHECK_FUNCS($1)
dnl	AC_CHECK_LIBS($2, foo)
fi
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

dnl $Id: roken-frag.m4,v 1.34 2001/11/30 03:29:47 assar Exp $
dnl
dnl some code to get roken working
dnl
dnl rk_ROKEN(subdir)
dnl
AC_DEFUN(rk_ROKEN, [

AC_REQUIRE([rk_CONFIG_HEADER])

DIR_roken=roken
LIB_roken='$(top_builddir)/$1/libroken.la'
INCLUDES_roken='-I$(top_builddir)/$1 -I$(top_srcdir)/$1'

dnl Checks for programs
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AC_PROG_AWK])
AC_REQUIRE([AC_OBJEXT])
AC_REQUIRE([AC_EXEEXT])
AC_REQUIRE([AC_PROG_LIBTOOL])

AC_REQUIRE([AC_MIPS_ABI])

dnl C characteristics

AC_REQUIRE([AC_C___ATTRIBUTE__])
AC_REQUIRE([AC_C_INLINE])
AC_REQUIRE([AC_C_CONST])
AC_WFLAGS(-Wall -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast -Wmissing-declarations -Wnested-externs)

AC_REQUIRE([rk_DB])

dnl C types

AC_REQUIRE([AC_TYPE_SIZE_T])
AC_CHECK_TYPE_EXTRA(ssize_t, int, [#include <unistd.h>])
AC_REQUIRE([AC_TYPE_PID_T])
AC_REQUIRE([AC_TYPE_UID_T])
AC_HAVE_TYPE([long long])

AC_REQUIRE([rk_RETSIGTYPE])

dnl Checks for header files.
AC_REQUIRE([AC_HEADER_STDC])
AC_REQUIRE([AC_HEADER_TIME])

AC_CHECK_HEADERS([\
	arpa/inet.h				\
	arpa/nameser.h				\
	config.h				\
	crypt.h					\
	dirent.h				\
	errno.h					\
	err.h					\
	fcntl.h					\
	grp.h					\
	ifaddrs.h				\
	net/if.h				\
	netdb.h					\
	netinet/in.h				\
	netinet/in6.h				\
	netinet/in_systm.h			\
	netinet6/in6.h				\
	netinet6/in6_var.h			\
	paths.h					\
	pwd.h					\
	resolv.h				\
	rpcsvc/ypclnt.h				\
	shadow.h				\
	sys/bswap.h				\
	sys/ioctl.h				\
	sys/param.h				\
	sys/proc.h				\
	sys/resource.h				\
	sys/socket.h				\
	sys/sockio.h				\
	sys/stat.h				\
	sys/sysctl.h				\
	sys/time.h				\
	sys/tty.h				\
	sys/types.h				\
	sys/uio.h				\
	sys/utsname.h				\
	sys/wait.h				\
	syslog.h				\
	termios.h				\
	unistd.h				\
	userconf.h				\
	usersec.h				\
	util.h					\
	vis.h					\
])
	
AC_REQUIRE([CHECK_NETINET_IP_AND_TCP])

AM_CONDITIONAL(have_err_h, test "$ac_cv_header_err_h" = yes)
AM_CONDITIONAL(have_fnmatch_h, test "$ac_cv_header_fnmatch_h" = yes)
AM_CONDITIONAL(have_ifaddrs_h, test "$ac_cv_header_ifaddrs_h" = yes)
AM_CONDITIONAL(have_vis_h, test "$ac_cv_header_vis_h" = yes)

dnl Check for functions and libraries

AC_FIND_FUNC(socket, socket)
AC_FIND_FUNC(gethostbyname, nsl)
AC_FIND_FUNC(syslog, syslog)

AC_KRB_IPV6

AC_FIND_FUNC(gethostbyname2, inet6 ip6)

AC_FIND_FUNC(res_search, resolv,
[
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
],
[0,0,0,0,0])

AC_FIND_FUNC(dn_expand, resolv,
[
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
],
[0,0,0,0,0])

AC_BROKEN_SNPRINTF
AC_BROKEN_VSNPRINTF

AC_BROKEN_GLOB
if test "$ac_cv_func_glob_working" != yes; then
	LIBOBJS="$LIBOBJS glob.o"
fi
AM_CONDITIONAL(have_glob_h, test "$ac_cv_func_glob_working" = yes)


AC_CHECK_FUNCS([				\
	asnprintf				\
	asprintf				\
	atexit					\
	cgetent					\
	getconfattr				\
	getprogname				\
	getrlimit				\
	getspnam				\
	initstate				\
	issetugid				\
	on_exit					\
	random					\
	setprogname				\
	setstate				\
	strsvis					\
	strunvis				\
	strvis					\
	strvisx					\
	svis					\
	sysconf					\
	sysctl					\
	uname					\
	unvis					\
	vasnprintf				\
	vasprintf				\
	vis					\
])

if test "$ac_cv_func_cgetent" = no; then
	LIBOBJS="$LIBOBJS getcap.o"
fi

AC_REQUIRE([AC_FUNC_GETLOGIN])

AC_FIND_FUNC_NO_LIBS(getsockopt,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif],
[0,0,0,0,0])
AC_FIND_FUNC_NO_LIBS(setsockopt,,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif],
[0,0,0,0,0])

AC_FIND_IF_NOT_BROKEN(hstrerror, resolv,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],
17)
if test "$ac_cv_func_hstrerror" = yes; then
AC_NEED_PROTO([
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],
hstrerror)
fi

dnl sigh, wish this could be done in a loop
if test "$ac_cv_func_asprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
asprintf)dnl
fi
if test "$ac_cv_func_vasprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
vasprintf)dnl
fi
if test "$ac_cv_func_asnprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
asnprintf)dnl
fi
if test "$ac_cv_func_vasnprintf" = yes; then
AC_NEED_PROTO([
#include <stdio.h>
#include <string.h>],
vasnprintf)dnl
fi

AC_FIND_FUNC_NO_LIBS(bswap16,,
[#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(bswap32,,
[#ifdef HAVE_SYS_BSWAP_H
#include <sys/bswap.h>
#endif],0)

AC_FIND_FUNC_NO_LIBS(pidfile,util,
[#ifdef HAVE_UTIL_H
#include <util.h>
#endif],0)

AC_FIND_IF_NOT_BROKEN(getaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0,0,0,0])

AC_FIND_IF_NOT_BROKEN(getnameinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0,0,0,0,0,0,0])

AC_FIND_IF_NOT_BROKEN(freeaddrinfo,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0])

AC_FIND_IF_NOT_BROKEN(gai_strerror,,
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[0])

AC_BROKEN([					\
	chown					\
	copyhostent				\
	daemon					\
	ecalloc					\
	emalloc					\
	erealloc				\
	estrdup					\
	err					\
	errx					\
	fchown					\
	flock					\
	fnmatch					\
	freehostent				\
	getcwd					\
	getdtablesize				\
	getegid					\
	geteuid					\
	getgid					\
	gethostname				\
	getifaddrs				\
	getipnodebyaddr				\
	getipnodebyname				\
	getopt					\
	gettimeofday				\
	getuid					\
	getusershell				\
	initgroups				\
	innetgr					\
	iruserok				\
	localtime_r				\
	lstat					\
	memmove					\
	mkstemp					\
	putenv					\
	rcmd					\
	readv					\
	recvmsg					\
	sendmsg					\
	setegid					\
	setenv					\
	seteuid					\
	strcasecmp				\
	strdup					\
	strerror				\
	strftime				\
	strlcat					\
	strlcpy					\
	strlwr					\
	strncasecmp				\
	strndup					\
	strnlen					\
	strptime				\
	strsep					\
	strsep_copy				\
	strtok_r				\
	strupr					\
	swab					\
	unsetenv				\
	verr					\
	verrx					\
	vsyslog					\
	vwarn					\
	vwarnx					\
	warn					\
	warnx					\
	writev					\
])

AC_BROKEN2(inet_aton,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0,0])

AC_BROKEN2(inet_ntop,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0, 0, 0, 0])

AC_BROKEN2(inet_pton,
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
[0,0,0])

dnl
dnl Check for sa_len in struct sockaddr, 
dnl needs to come before the getnameinfo test
dnl
AC_HAVE_STRUCT_FIELD(struct sockaddr, sa_len, [#include <sys/types.h>
#include <sys/socket.h>])

if test "$ac_cv_func_getnameinfo" = "yes"; then
  rk_BROKEN_GETNAMEINFO
  if test "$ac_cv_func_getnameinfo_broken" = yes; then
    LIBOBJS="$LIBOBJS getnameinfo.o"
  fi
fi

if test "$ac_cv_func_getaddrinfo" = "yes"; then
  rk_BROKEN_GETADDRINFO
  if test "$ac_cv_func_getaddrinfo_numserv" = no; then
    LIBOBJS="$LIBOBJS getaddrinfo.o freeaddrinfo.o"
  fi
fi

AC_NEED_PROTO([#include <stdlib.h>], setenv)
AC_NEED_PROTO([#include <stdlib.h>], unsetenv)
AC_NEED_PROTO([#include <unistd.h>], gethostname)
AC_NEED_PROTO([#include <unistd.h>], mkstemp)
AC_NEED_PROTO([#include <unistd.h>], getusershell)

AC_NEED_PROTO([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif],
inet_aton)

AC_FIND_FUNC_NO_LIBS(crypt, crypt)dnl

AC_REQUIRE([rk_BROKEN_REALLOC])dnl

dnl AC_KRB_FUNC_GETCWD_BROKEN

dnl
dnl Checks for prototypes and declarations
dnl

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
gethostbyname, struct hostent *gethostbyname(const char *))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
gethostbyaddr, struct hostent *gethostbyaddr(const void *, size_t, int))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
],
getservbyname, struct servent *getservbyname(const char *, const char *))

AC_PROTO_COMPAT([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
],
getsockname, int getsockname(int, struct sockaddr*, socklen_t*))

AC_PROTO_COMPAT([
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
],
openlog, void openlog(const char *, int, int))

AC_NEED_PROTO([
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
],
crypt)

AC_NEED_PROTO([
#include <string.h>
],
strtok_r)

AC_NEED_PROTO([
#include <string.h>
],
strsep)

dnl variables

rk_CHECK_VAR(h_errno, 
[#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR(h_errlist, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR(h_nerr, 
[#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif])

rk_CHECK_VAR([__progname], 
[#ifdef HAVE_ERR_H
#include <err.h>
#endif])

AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optarg)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optind)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], opterr)
AC_CHECK_DECLARATION([#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif], optopt)

AC_CHECK_DECLARATION([#include <stdlib.h>], environ)

dnl
dnl Check for fields in struct tm
dnl

AC_HAVE_STRUCT_FIELD(struct tm, tm_gmtoff, [#include <time.h>])
AC_HAVE_STRUCT_FIELD(struct tm, tm_zone, [#include <time.h>])

dnl
dnl or do we have a variable `timezone' ?
dnl

rk_CHECK_VAR(timezone,[#include <time.h>])

AC_HAVE_TYPE([sa_family_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([socklen_t],[#include <sys/socket.h>])
AC_HAVE_TYPE([struct sockaddr], [#include <sys/socket.h>])
AC_HAVE_TYPE([struct sockaddr_storage], [#include <sys/socket.h>])
AC_HAVE_TYPE([struct addrinfo], [#include <netdb.h>])
AC_HAVE_TYPE([struct ifaddrs], [#include <ifaddrs.h>])

dnl
dnl Check for struct winsize
dnl

AC_KRB_STRUCT_WINSIZE

dnl
dnl Check for struct spwd
dnl

AC_KRB_STRUCT_SPWD

dnl won't work with automake
dnl moved to AC_OUTPUT in configure.in
dnl AC_CONFIG_FILES($1/Makefile)

LIB_roken="${LIB_roken} \$(LIB_crypt) \$(LIB_dbopen)"

AC_SUBST(DIR_roken)dnl
AC_SUBST(LIB_roken)dnl
AC_SUBST(INCLUDES_roken)dnl
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

dnl $Id: have-type.m4,v 1.6 2000/07/15 18:10:00 joda Exp $
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
ac_foo=`eval echo \\$ac_cv_type_$cv`
AC_MSG_RESULT($ac_foo)
if test "$ac_foo" = yes; then
  ac_tr_hdr=HAVE_`echo $1 | sed 'y%abcdefghijklmnopqrstuvwxyz./- %ABCDEFGHIJKLMNOPQRSTUVWXYZ____%'`
if false; then
	AC_CHECK_TYPES($1)
fi
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1, [Define if you have type `$1'])
fi
])

dnl
dnl $Id: retsigtype.m4,v 1.1 2000/07/15 18:05:56 joda Exp $
dnl
dnl Figure out return type of signal handlers, and define SIGRETURN macro
dnl that can be used to return from one
dnl
AC_DEFUN(rk_RETSIGTYPE,[
AC_TYPE_SIGNAL
if test "$ac_cv_type_signal" = "void" ; then
	AC_DEFINE(VOID_RETSIGTYPE, 1, [Define if signal handlers return void.])
fi
AC_SUBST(VOID_RETSIGTYPE)
AH_BOTTOM([#ifdef VOID_RETSIGTYPE
#define SIGRETURN(x) return
#else
#define SIGRETURN(x) return (RETSIGTYPE)(x)
#endif])
])
dnl
dnl $Id: check-netinet-ip-and-tcp.m4,v 1.3 2000/07/18 10:33:02 joda Exp $
dnl

dnl extra magic check for netinet/{ip.h,tcp.h} because on irix 6.5.3
dnl you have to include standards.h before including these files

AC_DEFUN(CHECK_NETINET_IP_AND_TCP,
[
AC_CHECK_HEADERS(standards.h)
for i in netinet/ip.h netinet/tcp.h; do

cv=`echo "$i" | sed 'y%./+-%__p_%'`

AC_CACHE_CHECK([for $i],ac_cv_header_$cv,
[AC_TRY_CPP([\
#ifdef HAVE_STANDARDS_H
#include <standards.h>
#endif
#include <$i>
],
eval "ac_cv_header_$cv=yes",
eval "ac_cv_header_$cv=no")])
ac_res=`eval echo \\$ac_cv_header_$cv`
if test "$ac_res" = yes; then
	ac_tr_hdr=HAVE_`echo $i | sed 'y%abcdefghijklmnopqrstuvwxyz./-%ABCDEFGHIJKLMNOPQRSTUVWXYZ___%'`
	AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
done
if false;then
	AC_CHECK_HEADERS(netinet/ip.h netinet/tcp.h)
fi
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

dnl $Id: krb-ipv6.m4,v 1.12 2001/08/19 16:27:02 joda Exp $
dnl
dnl test for IPv6
dnl
AC_DEFUN(AC_KRB_IPV6, [
AC_ARG_WITH(ipv6,
[  --without-ipv6	do not enable IPv6 support],[
if test "$withval" = "no"; then
	ac_cv_lib_ipv6=no
fi])
save_CFLAGS="${CFLAGS}"
AC_CACHE_VAL(ac_cv_lib_ipv6,
[dnl check for different v6 implementations (by itojun)
v6type=unknown
v6lib=none

AC_MSG_CHECKING([ipv6 stack type])
for i in v6d toshiba kame inria zeta linux; do
	case $i in
	v6d)
		AC_EGREP_CPP(yes, [
#include </usr/local/v6/include/sys/types.h>
#ifdef __V6D__
yes
#endif],
			[v6type=$i; v6lib=v6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-I/usr/local/v6/include $CFLAGS"])
		;;
	toshiba)
		AC_EGREP_CPP(yes, [
#include <sys/param.h>
#ifdef _TOSHIBA_INET6
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	kame)
		AC_EGREP_CPP(yes, [
#include <netinet/in.h>
#ifdef __KAME__
yes
#endif],
			[v6type=$i; v6lib=inet6;
			v6libdir=/usr/local/v6/lib;
			CFLAGS="-DINET6 $CFLAGS"])
		;;
	inria)
		AC_EGREP_CPP(yes, [
#include <netinet/in.h>
#ifdef IPV6_INRIA_VERSION
yes
#endif],
			[v6type=$i; CFLAGS="-DINET6 $CFLAGS"])
		;;
	zeta)
		AC_EGREP_CPP(yes, [
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
else
  CFLAGS="${save_CFLAGS}"
fi

if test "$ac_cv_lib_ipv6" = yes; then
	AC_CACHE_CHECK([for in6addr_loopback],[ac_cv_var_in6addr_loopback],[
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
#endif],[
struct sockaddr_in6 sin6;
sin6.sin6_addr = in6addr_loopback;
],ac_cv_var_in6addr_loopback=yes,ac_cv_var_in6addr_loopback=no)])
	if test "$ac_cv_var_in6addr_loopback" = yes; then
		AC_DEFINE(HAVE_IN6ADDR_LOOPBACK, 1, 
			[Define if you have the in6addr_loopback variable])
	fi
fi
])
dnl $Id: broken-snprintf.m4,v 1.4 2001/09/01 11:56:05 assar Exp $
dnl
AC_DEFUN(AC_BROKEN_SNPRINTF, [
AC_CACHE_CHECK(for working snprintf,ac_cv_func_snprintf_working,
ac_cv_func_snprintf_working=yes
AC_TRY_RUN([
#include <stdio.h>
#include <string.h>
int main()
{
	char foo[[3]];
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
	char bar[[3]];
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

dnl $Id: broken-glob.m4,v 1.4 2001/06/19 09:59:46 assar Exp $
dnl
dnl check for glob(3)
dnl
AC_DEFUN(AC_BROKEN_GLOB,[
AC_CACHE_CHECK(for working glob, ac_cv_func_glob_working,
ac_cv_func_glob_working=yes
AC_TRY_LINK([
#include <stdio.h>
#include <glob.h>],[
glob(NULL, GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE|
#ifdef GLOB_MAXPATH
GLOB_MAXPATH
#else
GLOB_LIMIT
#endif
,
NULL, NULL);
],:,ac_cv_func_glob_working=no,:))

if test "$ac_cv_func_glob_working" = yes; then
	AC_DEFINE(HAVE_GLOB, 1, [define if you have a glob() that groks 
	GLOB_BRACE, GLOB_NOCHECK, GLOB_QUOTE, GLOB_TILDE, and GLOB_LIMIT])
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

dnl $Id: broken.m4,v 1.4 2000/07/15 18:06:36 joda Exp $
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
if false; then
	AC_CHECK_FUNCS($1)
fi
done
AC_SUBST(LIBOBJS)dnl
])

dnl $Id: broken2.m4,v 1.1 2000/12/15 14:27:33 assar Exp $
dnl
dnl AC_BROKEN but with more arguments

dnl AC_BROKEN2(func, includes, arguments)
AC_DEFUN(AC_BROKEN2,
[for ac_func in $1
do
AC_MSG_CHECKING([for $ac_func])
AC_CACHE_VAL(ac_cv_func_$ac_func,
[AC_TRY_LINK([$2],
[
/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias.  */
#if defined (__stub_$1) || defined (__stub___$1)
choke me
#else
$ac_func($3)
#endif
], [eval "ac_cv_func_$ac_func=yes"], [eval "ac_cv_func_$ac_func=no"])])
if eval "test \"\${ac_cv_func_$ac_func}\" = yes"; then
  ac_tr_func=HAVE_[]upcase($ac_func)
  AC_DEFINE_UNQUOTED($ac_tr_func)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  LIBOBJS[]="$LIBOBJS ${ac_func}.o"
fi
done
if false; then
	AC_CHECK_FUNCS($1)
fi
AC_SUBST(LIBOBJS)dnl
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

dnl $Id: broken-getnameinfo.m4,v 1.2 2000/12/05 09:09:00 joda Exp $
dnl
dnl test for broken AIX getnameinfo

AC_DEFUN(rk_BROKEN_GETNAMEINFO,[
AC_CACHE_CHECK([if getnameinfo is broken], ac_cv_func_getnameinfo_broken,
AC_TRY_RUN([[#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int
main(int argc, char **argv)
{
  struct sockaddr_in sin;
  char host[256];
  memset(&sin, 0, sizeof(sin));
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
  sin.sin_len = sizeof(sin);
#endif
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = 0xffffffff;
  sin.sin_port = 0;
  return getnameinfo((struct sockaddr*)&sin, sizeof(sin), host, sizeof(host),
	      NULL, 0, 0);
}
]], ac_cv_func_getnameinfo_broken=no, ac_cv_func_getnameinfo_broken=yes))])

dnl $Id: broken-getaddrinfo.m4,v 1.2 2001/08/22 01:05:29 assar Exp $
dnl
dnl test if getaddrinfo can handle numeric services

AC_DEFUN(rk_BROKEN_GETADDRINFO,[
AC_CACHE_CHECK([if getaddrinfo handles numeric services], ac_cv_func_getaddrinfo_numserv,
AC_TRY_RUN([[#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int
main(int argc, char **argv)
{
	struct addrinfo hints, *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = PF_UNSPEC;
	if(getaddrinfo(NULL, "17", &hints, &ai) == EAI_SERVICE)
		return 1;
	return 0;
}
]], ac_cv_func_getaddrinfo_numserv=yes, ac_cv_func_getaddrinfo_numserv=no))])

dnl
dnl $Id: broken-realloc.m4,v 1.1 2000/07/15 18:05:36 joda Exp $
dnl
dnl Test for realloc that doesn't handle NULL as first parameter
dnl
AC_DEFUN(rk_BROKEN_REALLOC, [
AC_CACHE_CHECK(if realloc if broken, ac_cv_func_realloc_broken, [
ac_cv_func_realloc_broken=no
AC_TRY_RUN([
#include <stddef.h>
#include <stdlib.h>

int main()
{
	return realloc(NULL, 17) == NULL;
}
],:, ac_cv_func_realloc_broken=yes, :)
])
if test "$ac_cv_func_realloc_broken" = yes ; then
	AC_DEFINE(BROKEN_REALLOC, 1, [Define if realloc(NULL) doesn't work.])
fi
AH_BOTTOM([#ifdef BROKEN_REALLOC
#define realloc(X, Y) isoc_realloc((X), (Y))
#define isoc_realloc(X, Y) ((X) ? realloc((X), (Y)) : malloc(Y))
#endif])
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
dnl $Id: check-var.m4,v 1.6 2001/08/21 12:00:16 joda Exp $
dnl
dnl rk_CHECK_VAR(variable, includes)
AC_DEFUN([rk_CHECK_VAR], [
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(ac_cv_var_$1, [
AC_TRY_LINK([extern int $1;
int foo() { return $1; }],
	    [foo()],
	    ac_cv_var_$1=yes, ac_cv_var_$1=no)
])
ac_foo=`eval echo \\$ac_cv_var_$1`
AC_MSG_RESULT($ac_foo)
if test "$ac_foo" = yes; then
	AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_[]$1), 1, 
		[Define if you have the `]$1[' variable.])
	m4_ifval([$2], AC_CHECK_DECLARATION([$2],[$1]))
fi
])

AC_WARNING_ENABLE([obsolete])
AU_DEFUN([AC_CHECK_VAR], [rk_CHECK_VAR([$2], [$1])], [foo])
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

dnl $Id: krb-struct-winsize.m4,v 1.3 2001/09/01 11:56:05 assar Exp $
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
struct[[ 	]]*winsize,dnl
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

dnl $Id: test-package.m4,v 1.9 2000/12/15 04:54:24 assar Exp $
dnl
dnl AC_TEST_PACKAGE_NEW(package,headers,libraries,extra libs,default locations, conditional)

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
	m4_ifval([$6],
		AC_DEFINE_UNQUOTED($6,1,[Define if you have the $1 package.]),
		AC_DEFINE_UNQUOTED(upcase($1),1,[Define if you have the $1 package.]))
	with_$1=yes
	AC_MSG_RESULT([headers $ires, libraries $lres])
else
	INCLUDE_$1=
	LIB_$1=
	with_$1=no
	AC_MSG_RESULT($with_$1)
fi
dnl m4_ifval([$6],
dnl 	AM_CONDITIONAL($6, test "$with_$1" = yes)
dnl 	AM_CONDITIONAL(upcase($1), test "$with_$1" = yes))
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

dnl $Id: check-man.m4,v 1.3 2000/11/30 01:47:17 joda Exp $
dnl check how to format manual pages
dnl

AC_DEFUN(rk_CHECK_MAN,
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
	CATMANEXT='$$section'
else
	CATMANEXT=0
fi
AC_SUBST(CATMANEXT)
])
dnl
dnl $Id: krb-bigendian.m4,v 1.6 2000/08/19 15:37:00 assar Exp $
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
AC_CACHE_CHECK(whether byte ordering is bigendian, krb_cv_c_bigendian,[
  if test "$krb_cv_c_bigendian_compile" = "yes"; then
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/param.h>],[
#if BYTE_ORDER != BIG_ENDIAN
  not big endian
#endif], krb_cv_c_bigendian=yes, krb_cv_c_bigendian=no)
  else
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
if test "$krb_cv_c_bigendian_compile" = "yes"; then
  AC_DEFINE(ENDIANESS_IN_SYS_PARAM_H, 1, [define if sys/param.h defines the endiness])dnl
fi
])

dnl
dnl $Id: aix.m4,v 1.5 2000/11/05 17:15:46 joda Exp $
dnl

AC_DEFUN(KRB_AIX,[
aix=no
case "$host" in 
*-*-aix3*)
	aix=3
	;;
*-*-aix4*)
	aix=4
	;;
esac
AM_CONDITIONAL(AIX, test "$aix" != no)dnl
AM_CONDITIONAL(AIX4, test "$aix" = 4)
aix_dynamic_afs=yes
AM_CONDITIONAL(AIX_DYNAMIC_AFS, test "$aix_dynamic_afs" = yes)dnl

AC_FIND_FUNC_NO_LIBS(dlopen, dl)

if test "$aix" != no; then
	if test "$aix_dynamic_afs" = yes; then
		if test "$ac_cv_funclib_dlopen" = yes; then
			AIX_EXTRA_KAFS=
		elif test "$ac_cv_funclib_dlopen" != no; then
			AIX_EXTRA_KAFS="$ac_cv_funclib_dlopen"
		else
			AIX_EXTRA_KAFS=-lld
		fi
	else
		AIX_EXTRA_KAFS=
	fi
fi

AM_CONDITIONAL(HAVE_DLOPEN, test "$ac_cv_funclib_dlopen" != no)dnl
AC_SUBST(AIX_EXTRA_KAFS)dnl

])
dnl
dnl $Id: krb-irix.m4,v 1.2 2000/12/13 12:48:45 assar Exp $
dnl

dnl requires AC_CANONICAL_HOST
AC_DEFUN(KRB_IRIX,[
irix=no
case "$host_os" in
irix*) irix=yes ;;
esac
AM_CONDITIONAL(IRIX, test "$irix" != no)dnl
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

dnl
dnl $Id: with-all.m4,v 1.1 2001/08/29 17:01:23 assar Exp $
dnl

dnl AC_WITH_ALL(name)

AC_DEFUN([AC_WITH_ALL], [
AC_ARG_WITH($1,
	AC_HELP_STRING([--with-$1=dir],
		[use $1 in dir]))

AC_ARG_WITH($1-lib,
	AC_HELP_STRING([--with-$1-lib=dir],
		[use $1 libraries in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-lib])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

AC_ARG_WITH($1-include,
	AC_HELP_STRING([--with-$1-include=dir],
		[use $1 headers in dir]),
[if test "$withval" = "yes" -o "$withval" = "no"; then
  AC_MSG_ERROR([No argument for --with-$1-include])
elif test "X$with_$1" = "X"; then
  with_$1=yes
fi])

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
])
dnl $Id: krb-readline.m4,v 1.2 2000/11/15 00:47:08 assar Exp $
dnl
dnl Tests for readline functions
dnl

dnl el_init

AC_DEFUN(KRB_READLINE,[
AC_FIND_FUNC_NO_LIBS(el_init, edit, [], [], [$LIB_tgetent])
if test "$ac_cv_func_el_init" = yes ; then
	AC_CACHE_CHECK(for four argument el_init, ac_cv_func_el_init_four,[
		AC_TRY_COMPILE([#include <stdio.h>
			#include <histedit.h>],
			[el_init("", NULL, NULL, NULL);],
			ac_cv_func_el_init_four=yes,
			ac_cv_func_el_init_four=no)])
	if test "$ac_cv_func_el_init_four" = yes; then
		AC_DEFINE(HAVE_FOUR_VALUED_EL_INIT, 1, [Define if el_init takes four arguments.])
	fi
fi

dnl readline

ac_foo=no
if test "$with_readline" = yes; then
	:
elif test "$ac_cv_func_readline" = yes; then
	:
elif test "$ac_cv_func_el_init" = yes; then
	ac_foo=yes
	LIB_readline="\$(top_builddir)/lib/editline/libel_compat.la $LIB_el_init"
else
	LIB_readline='$(top_builddir)/lib/editline/libeditline.la'
fi
AM_CONDITIONAL(el_compat, test "$ac_foo" = yes)
if test "$readline_libdir"; then
	LIB_readline="-rpath $readline_libdir $LIB_readline"
fi
LIB_readline="$LIB_readline \$(LIB_tgetent)"
AC_DEFINE(HAVE_READLINE, 1, 
	[Define if you have a readline compatible library.])dnl

])
dnl $Id: check-compile-et.m4,v 1.6 2001/09/02 17:08:48 assar Exp $
dnl
dnl CHECK_COMPILE_ET
AC_DEFUN([CHECK_COMPILE_ET], [

AC_CHECK_PROG(COMPILE_ET, compile_et, [compile_et])

krb_cv_compile_et="no"
if test "${COMPILE_ET}" = "compile_et"; then

dnl We have compile_et.  Now let's see if it supports `prefix' and `index'.
AC_MSG_CHECKING(whether compile_et has the features we need)
cat > conftest_et.et <<'EOF'
error_table conf
prefix CONFTEST
index 1
error_code CODE1, "CODE1"
index 128
error_code CODE2, "CODE2"
end
EOF
if ${COMPILE_ET} conftest_et.et >/dev/null 2>&1; then
  dnl XXX Some systems have <et/com_err.h>.
  save_CPPFLAGS="${save_CPPFLAGS}"
  if test -d "/usr/include/et"; then
    CPPFLAGS="-I/usr/include/et ${CPPFLAGS}"
  fi
  dnl Check that the `prefix' and `index' directives were honored.
  AC_TRY_RUN([
#include <com_err.h>
#include <string.h>
#include "conftest_et.h"
int main(){return (CONFTEST_CODE2 - CONFTEST_CODE1) != 127;}
  ], [krb_cv_compile_et="yes"],[CPPFLAGS="${save_CPPFLAGS}"])
fi
AC_MSG_RESULT(${krb_cv_compile_et})
rm -fr conftest*
fi

if test "${krb_cv_compile_et}" = "yes"; then
  dnl Since compile_et seems to work, let's check libcom_err
  krb_cv_save_LIBS="${LIBS}"
  LIBS="${LIBS} -lcom_err"
  AC_MSG_CHECKING(for com_err)
  AC_TRY_LINK([#include <com_err.h>],[
    const char *p;
    p = error_message(0);
  ],[krb_cv_com_err="yes"],[krb_cv_com_err="no"; CPPFLAGS="${save_CPPFLAGS}"])
  AC_MSG_RESULT(${krb_cv_com_err})
  LIBS="${krb_cv_save_LIBS}"
else
  dnl Since compile_et doesn't work, forget about libcom_err
  krb_cv_com_err="no"
fi

dnl Only use the system's com_err if we found compile_et, libcom_err, and
dnl com_err.h.
if test "${krb_cv_com_err}" = "yes"; then
    DIR_com_err=""
    LIB_com_err="-lcom_err"
    LIB_com_err_a=""
    LIB_com_err_so=""
    AC_MSG_NOTICE(Using the already-installed com_err)
else
    COMPILE_ET="\$(top_builddir)/lib/com_err/compile_et"
    DIR_com_err="com_err"
    LIB_com_err="\$(top_builddir)/lib/com_err/libcom_err.la"
    LIB_com_err_a="\$(top_builddir)/lib/com_err/.libs/libcom_err.a"
    LIB_com_err_so="\$(top_builddir)/lib/com_err/.libs/libcom_err.so"
    AC_MSG_NOTICE(Using our own com_err)
fi
AC_SUBST(DIR_com_err)
AC_SUBST(LIB_com_err)
AC_SUBST(LIB_com_err_a)
AC_SUBST(LIB_com_err_so)

])

dnl $Id: auth-modules.m4,v 1.2 2001/09/01 11:56:05 assar Exp $
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
*-*-irix[[56]]*) LIB_AUTH_SUBDIRS="$LIB_AUTH_SUBDIRS afskauthlib" ;;
esac

AC_MSG_RESULT($LIB_AUTH_SUBDIRS)

AC_SUBST(LIB_AUTH_SUBDIRS)dnl
])

