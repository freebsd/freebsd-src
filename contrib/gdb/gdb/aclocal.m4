dnl aclocal.m4 generated automatically by aclocal 1.4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl written by Rob Savoye <rob@cygnus.com> for Cygnus Support
dnl major rewriting for Tcl 7.5 by Don Libes <libes@nist.gov>

dnl gdb/configure.in uses BFD_NEED_DECLARATION, so get its definition.
sinclude(../bfd/acinclude.m4)

dnl This gets the standard macros, like the TCL, TK, etc ones.
sinclude(../config/acinclude.m4)

dnl CYGNUS LOCAL: This gets the right posix flag for gcc
AC_DEFUN(CY_AC_TCL_LYNX_POSIX,
[AC_REQUIRE([AC_PROG_CC])AC_REQUIRE([AC_PROG_CPP])
AC_MSG_CHECKING([if running LynxOS])
AC_CACHE_VAL(ac_cv_os_lynx,
[AC_EGREP_CPP(yes,
[/*
 * The old Lynx "cc" only defines "Lynx", but the newer one uses "__Lynx__"
 */
#if defined(__Lynx__) || defined(Lynx)
yes
#endif
], ac_cv_os_lynx=yes, ac_cv_os_lynx=no)])
#
if test "$ac_cv_os_lynx" = "yes" ; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(LYNX)
  AC_MSG_CHECKING([whether -mposix or -X is available])
  AC_CACHE_VAL(ac_cv_c_posix_flag,
  [AC_TRY_COMPILE(,[
  /*
   * This flag varies depending on how old the compiler is.
   * -X is for the old "cc" and "gcc" (based on 1.42).
   * -mposix is for the new gcc (at least 2.5.8).
   */
  #if defined(__GNUC__) && __GNUC__ >= 2
  choke me
  #endif
  ], ac_cv_c_posix_flag=" -mposix", ac_cv_c_posix_flag=" -X")])
  CC="$CC $ac_cv_c_posix_flag"
  AC_MSG_RESULT($ac_cv_c_posix_flag)
  else
  AC_MSG_RESULT(no)
fi
])

#
# Sometimes the native compiler is a bogus stub for gcc or /usr/ucb/cc. This
# makes configure think it's cross compiling. If --target wasn't used, then
# we can't configure, so something is wrong. We don't use the cache
# here cause if somebody fixes their compiler install, we want this to work.
AC_DEFUN(CY_AC_C_WORKS,
[# If we cannot compile and link a trivial program, we can't expect anything to work
AC_MSG_CHECKING(whether the compiler ($CC) actually works)
AC_TRY_COMPILE(, [/* don't need anything here */],
        c_compiles=yes, c_compiles=no)

AC_TRY_LINK(, [/* don't need anything here */],
        c_links=yes, c_links=no)

if test x"${c_compiles}" = x"no" ; then
  AC_MSG_ERROR(the native compiler is broken and won't compile.)
fi

if test x"${c_links}" = x"no" ; then
  AC_MSG_ERROR(the native compiler is broken and won't link.)
fi
AC_MSG_RESULT(yes)
])

AC_DEFUN(CY_AC_PATH_TCLH, [
#
# Ok, lets find the tcl source trees so we can use the headers
# Warning: transition of version 9 to 10 will break this algorithm
# because 10 sorts before 9. We also look for just tcl. We have to
# be careful that we don't match stuff like tclX by accident.
# the alternative search directory is involked by --with-tclinclude
#

no_tcl=true
AC_MSG_CHECKING(for Tcl private headers. dir=${configdir})
AC_ARG_WITH(tclinclude, [  --with-tclinclude=DIR   Directory where tcl private headers are], with_tclinclude=${withval})
AC_CACHE_VAL(ac_cv_c_tclh,[
# first check to see if --with-tclinclude was specified
if test x"${with_tclinclude}" != x ; then
  if test -f ${with_tclinclude}/tclInt.h ; then
    ac_cv_c_tclh=`(cd ${with_tclinclude}; pwd)`
  elif test -f ${with_tclinclude}/generic/tclInt.h ; then
    ac_cv_c_tclh=`(cd ${with_tclinclude}/generic; pwd)`
  else
    AC_MSG_ERROR([${with_tclinclude} directory doesn't contain private headers])
  fi
fi

# next check if it came with Tcl configuration file
if test x"${ac_cv_c_tclconfig}" = x ; then
  if test -f $ac_cv_c_tclconfig/../generic/tclInt.h ; then
    ac_cv_c_tclh=`(cd $ac_cv_c_tclconfig/..; pwd)`
  fi
fi

# next check in private source directory
#
# since ls returns lowest version numbers first, reverse its output
if test x"${ac_cv_c_tclh}" = x ; then
  for i in \
		${srcdir}/../tcl \
		`ls -dr ${srcdir}/../tcl[[7-9]]* 2>/dev/null` \
		${srcdir}/../../tcl \
		`ls -dr ${srcdir}/../../tcl[[7-9]]* 2>/dev/null` \
		${srcdir}/../../../tcl \
		`ls -dr ${srcdir}/../../../tcl[[7-9]]* 2>/dev/null ` ; do
    if test -f $i/generic/tclInt.h ; then
      ac_cv_c_tclh=`(cd $i/generic; pwd)`
      break
    fi
  done
fi
# finally check in a few common install locations
#
# since ls returns lowest version numbers first, reverse its output
if test x"${ac_cv_c_tclh}" = x ; then
  for i in \
		`ls -dr /usr/local/src/tcl[[7-9]]* 2>/dev/null` \
		`ls -dr /usr/local/lib/tcl[[7-9]]* 2>/dev/null` \
		/usr/local/src/tcl \
		/usr/local/lib/tcl \
		${prefix}/include ; do
    if test -f $i/generic/tclInt.h ; then
      ac_cv_c_tclh=`(cd $i/generic; pwd)`
      break
    fi
  done
fi
# see if one is installed
if test x"${ac_cv_c_tclh}" = x ; then
   AC_HEADER_CHECK(tclInt.h, ac_cv_c_tclh=installed, ac_cv_c_tclh="")
fi
])
if test x"${ac_cv_c_tclh}" = x ; then
  TCLHDIR="# no Tcl private headers found"
  AC_MSG_ERROR([Can't find Tcl private headers])
fi
if test x"${ac_cv_c_tclh}" != x ; then
  no_tcl=""
  if test x"${ac_cv_c_tclh}" = x"installed" ; then
    AC_MSG_RESULT([is installed])
    TCLHDIR=""
  else
    AC_MSG_RESULT([found in ${ac_cv_c_tclh}])
    # this hack is cause the TCLHDIR won't print if there is a "-I" in it.
    TCLHDIR="-I${ac_cv_c_tclh}"
  fi
fi

AC_SUBST(TCLHDIR)
])


AC_DEFUN(CY_AC_PATH_TCLCONFIG, [
#
# Ok, lets find the tcl configuration
# First, look for one uninstalled.  
# the alternative search directory is invoked by --with-tclconfig
#

if test x"${no_tcl}" = x ; then
  # we reset no_tcl in case something fails here
  no_tcl=true
  AC_ARG_WITH(tclconfig, [  --with-tclconfig=DIR    Directory containing tcl configuration (tclConfig.sh)],
         with_tclconfig=${withval})
  AC_MSG_CHECKING([for Tcl configuration])
  AC_CACHE_VAL(ac_cv_c_tclconfig,[

  # First check to see if --with-tclconfig was specified.
  if test x"${with_tclconfig}" != x ; then
    if test -f "${with_tclconfig}/tclConfig.sh" ; then
      ac_cv_c_tclconfig=`(cd ${with_tclconfig}; pwd)`
    else
      AC_MSG_ERROR([${with_tclconfig} directory doesn't contain tclConfig.sh])
    fi
  fi

  # then check for a private Tcl installation
  if test x"${ac_cv_c_tclconfig}" = x ; then
    for i in \
		../tcl \
		`ls -dr ../tcl[[7-9]]* 2>/dev/null` \
		../../tcl \
		`ls -dr ../../tcl[[7-9]]* 2>/dev/null` \
		../../../tcl \
		`ls -dr ../../../tcl[[7-9]]* 2>/dev/null` ; do
      if test -f "$i/${configdir}/tclConfig.sh" ; then
        ac_cv_c_tclconfig=`(cd $i/${configdir}; pwd)`
	break
      fi
    done
  fi
  # check in a few common install locations
  if test x"${ac_cv_c_tclconfig}" = x ; then
    for i in `ls -d ${prefix}/lib /usr/local/lib 2>/dev/null` ; do
      if test -f "$i/tclConfig.sh" ; then
        ac_cv_c_tclconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few other private locations
  if test x"${ac_cv_c_tclconfig}" = x ; then
    for i in \
		${srcdir}/../tcl \
		`ls -dr ${srcdir}/../tcl[[7-9]]* 2>/dev/null` ; do
      if test -f "$i/${configdir}/tclConfig.sh" ; then
        ac_cv_c_tclconfig=`(cd $i/${configdir}; pwd)`
	break
      fi
    done
  fi
  ])
  if test x"${ac_cv_c_tclconfig}" = x ; then
    TCLCONFIG="# no Tcl configs found"
    AC_MSG_WARN(Can't find Tcl configuration definitions)
  else
    no_tcl=
    TCLCONFIG=${ac_cv_c_tclconfig}/tclConfig.sh
    AC_MSG_RESULT(found $TCLCONFIG)
  fi
fi
])

# Defined as a separate macro so we don't have to cache the values
# from PATH_TCLCONFIG (because this can also be cached).
AC_DEFUN(CY_AC_LOAD_TCLCONFIG, [
    . $TCLCONFIG

    AC_SUBST(TCL_VERSION)
    AC_SUBST(TCL_MAJOR_VERSION)
    AC_SUBST(TCL_MINOR_VERSION)
    AC_SUBST(TCL_CC)
    AC_SUBST(TCL_DEFS)

dnl not used, don't export to save symbols
dnl    AC_SUBST(TCL_LIB_FILE)

dnl don't export, not used outside of configure
dnl     AC_SUBST(TCL_LIBS)
dnl not used, don't export to save symbols
dnl    AC_SUBST(TCL_PREFIX)

dnl not used, don't export to save symbols
dnl    AC_SUBST(TCL_EXEC_PREFIX)

    AC_SUBST(TCL_SHLIB_CFLAGS)
    AC_SUBST(TCL_SHLIB_LD)
dnl don't export, not used outside of configure
    AC_SUBST(TCL_SHLIB_LD_LIBS)
    AC_SUBST(TCL_SHLIB_SUFFIX)
dnl not used, don't export to save symbols
    AC_SUBST(TCL_DL_LIBS)
    AC_SUBST(TCL_LD_FLAGS)
dnl don't export, not used outside of configure
    AC_SUBST(TCL_LD_SEARCH_FLAGS)
    AC_SUBST(TCL_COMPAT_OBJS)
    AC_SUBST(TCL_RANLIB)
    AC_SUBST(TCL_BUILD_LIB_SPEC)
    AC_SUBST(TCL_LIB_SPEC)
    AC_SUBST(TCL_LIB_VERSIONS_OK)

dnl not used, don't export to save symbols
dnl    AC_SUBST(TCL_SHARED_LIB_SUFFIX)

dnl not used, don't export to save symbols
dnl    AC_SUBST(TCL_UNSHARED_LIB_SUFFIX)
])

# Warning: Tk definitions are very similar to Tcl definitions but
# are not precisely the same.  There are a couple of differences,
# so don't do changes to Tcl thinking you can cut and paste it do 
# the Tk differences and later simply substitute "Tk" for "Tcl".
# Known differences:
#  - Acceptable Tcl major version #s is 7-9 while Tk is 4-9
#  - Searching for Tcl includes looking for tclInt.h, Tk looks for tk.h
#  - Computing major/minor versions is different because Tk depends on
#    headers to Tcl, Tk, and X.
#  - Symbols in tkConfig.sh are different than tclConfig.sh
#  - Acceptable for Tk to be missing but not Tcl.

AC_DEFUN(CY_AC_PATH_TKH, [
#
# Ok, lets find the tk source trees so we can use the headers
# If the directory (presumably symlink) named "tk" exists, use that one
# in preference to any others.  Same logic is used when choosing library
# and again with Tcl. The search order is the best place to look first, then in
# decreasing significance. The loop breaks if the trigger file is found.
# Note the gross little conversion here of srcdir by cd'ing to the found
# directory. This converts the path from a relative to an absolute, so
# recursive cache variables for the path will work right. We check all
# the possible paths in one loop rather than many seperate loops to speed
# things up.
# the alternative search directory is involked by --with-tkinclude
#
no_tk=true
AC_MSG_CHECKING(for Tk private headers)
AC_ARG_WITH(tkinclude, [  --with-tkinclude=DIR    Directory where tk private headers are], with_tkinclude=${withval})
AC_CACHE_VAL(ac_cv_c_tkh,[
# first check to see if --with-tkinclude was specified
if test x"${with_tkinclude}" != x ; then
  if test -f ${with_tkinclude}/tk.h ; then
    ac_cv_c_tkh=`(cd ${with_tkinclude}; pwd)`
  elif test -f ${with_tkinclude}/generic/tk.h ; then
    ac_cv_c_tkh=`(cd ${with_tkinclude}/generic; pwd)`
  else
    AC_MSG_ERROR([${with_tkinclude} directory doesn't contain private headers])
  fi
fi

# next check if it came with Tk configuration file
if test x"${ac_cv_c_tkconfig}" = x ; then
  if test -f $ac_cv_c_tkconfig/../generic/tk.h ; then
    ac_cv_c_tkh=`(cd $ac_cv_c_tkconfig/..; pwd)`
  fi
fi

# next check in private source directory
#
# since ls returns lowest version numbers first, reverse its output
if test x"${ac_cv_c_tkh}" = x ; then
  for i in \
		${srcdir}/../tk \
		`ls -dr ${srcdir}/../tk[[4-9]]* 2>/dev/null` \
		${srcdir}/../../tk \
		`ls -dr ${srcdir}/../../tk[[4-9]]* 2>/dev/null` \
		${srcdir}/../../../tk \
		`ls -dr ${srcdir}/../../../tk[[4-9]]* 2>/dev/null ` ; do
    if test -f $i/generic/tk.h ; then
      ac_cv_c_tkh=`(cd $i/generic; pwd)`
      break
    fi
  done
fi
# finally check in a few common install locations
#
# since ls returns lowest version numbers first, reverse its output
if test x"${ac_cv_c_tkh}" = x ; then
  for i in \
		`ls -dr /usr/local/src/tk[[4-9]]* 2>/dev/null` \
		`ls -dr /usr/local/lib/tk[[4-9]]* 2>/dev/null` \
		/usr/local/src/tk \
		/usr/local/lib/tk \
		${prefix}/include ; do
    if test -f $i/generic/tk.h ; then
      ac_cv_c_tkh=`(cd $i/generic; pwd)`
      break
    fi
  done
fi
# see if one is installed
if test x"${ac_cv_c_tkh}" = x ; then
   AC_HEADER_CHECK(tk.h, ac_cv_c_tkh=installed, ac_cv_c_tkh="")
fi
])
if test x"${ac_cv_c_tkh}" != x ; then
  no_tk=""
  if test x"${ac_cv_c_tkh}" = x"installed" ; then
    AC_MSG_RESULT([is installed])
    TKHDIR=""
  else
    AC_MSG_RESULT([found in ${ac_cv_c_tkh}])
    # this hack is cause the TKHDIR won't print if there is a "-I" in it.
    TKHDIR="-I${ac_cv_c_tkh}"
  fi
else
  TKHDIR="# no Tk directory found"
  AC_MSG_WARN([Can't find Tk private headers])
  no_tk=true
fi

AC_SUBST(TKHDIR)
])


AC_DEFUN(CY_AC_PATH_TKCONFIG, [
#
# Ok, lets find the tk configuration
# First, look for one uninstalled.  
# the alternative search directory is invoked by --with-tkconfig
#

if test x"${no_tk}" = x ; then
  # we reset no_tk in case something fails here
  no_tk=true
  AC_ARG_WITH(tkconfig, [  --with-tkconfig=DIR     Directory containing tk configuration (tkConfig.sh)],
         with_tkconfig=${withval})
  AC_MSG_CHECKING([for Tk configuration])
  AC_CACHE_VAL(ac_cv_c_tkconfig,[

  # First check to see if --with-tkconfig was specified.
  if test x"${with_tkconfig}" != x ; then
    if test -f "${with_tkconfig}/tkConfig.sh" ; then
      ac_cv_c_tkconfig=`(cd ${with_tkconfig}; pwd)`
    else
      AC_MSG_ERROR([${with_tkconfig} directory doesn't contain tkConfig.sh])
    fi
  fi

  # then check for a private Tk library
  if test x"${ac_cv_c_tkconfig}" = x ; then
    for i in \
		../tk \
		`ls -dr ../tk[[4-9]]* 2>/dev/null` \
		../../tk \
		`ls -dr ../../tk[[4-9]]* 2>/dev/null` \
		../../../tk \
		`ls -dr ../../../tk[[4-9]]* 2>/dev/null` ; do
      if test -f "$i/${configdir}/tkConfig.sh" ; then
        ac_cv_c_tkconfig=`(cd $i/${configdir}; pwd)`
	break
      fi
    done
  fi
  # check in a few common install locations
  if test x"${ac_cv_c_tkconfig}" = x ; then
    for i in `ls -d ${prefix}/lib /usr/local/lib 2>/dev/null` ; do
      if test -f "$i/tkConfig.sh" ; then
        ac_cv_c_tkconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few other private locations
  if test x"${ac_cv_c_tkconfig}" = x ; then
    for i in \
		${srcdir}/../tk \
		`ls -dr ${srcdir}/../tk[[4-9]]* 2>/dev/null` ; do
      if test -f "$i/${configdir}/tkConfig.sh" ; then
        ac_cv_c_tkconfig=`(cd $i/${configdir}; pwd)`
	break
      fi
    done
  fi
  ])
  if test x"${ac_cv_c_tkconfig}" = x ; then
    TKCONFIG="# no Tk configs found"
    AC_MSG_WARN(Can't find Tk configuration definitions)
  else
    no_tk=
    TKCONFIG=${ac_cv_c_tkconfig}/tkConfig.sh
    AC_MSG_RESULT(found $TKCONFIG)
  fi
fi

])

# Defined as a separate macro so we don't have to cache the values
# from PATH_TKCONFIG (because this can also be cached).
AC_DEFUN(CY_AC_LOAD_TKCONFIG, [
    if test -f "$TKCONFIG" ; then
      . $TKCONFIG
    fi

    AC_SUBST(TK_VERSION)
dnl not actually used, don't export to save symbols
dnl    AC_SUBST(TK_MAJOR_VERSION)
dnl    AC_SUBST(TK_MINOR_VERSION)
    AC_SUBST(TK_DEFS)

dnl not used, don't export to save symbols
    dnl AC_SUBST(TK_LIB_FILE)

dnl not used outside of configure
dnl    AC_SUBST(TK_LIBS)
dnl not used, don't export to save symbols
dnl    AC_SUBST(TK_PREFIX)

dnl not used, don't export to save symbols
dnl    AC_SUBST(TK_EXEC_PREFIX)

    AC_SUBST(TK_BUILD_INCLUDES)
    AC_SUBST(TK_XINCLUDES)
    AC_SUBST(TK_XLIBSW)
    AC_SUBST(TK_BUILD_LIB_SPEC)
    AC_SUBST(TK_LIB_SPEC)
])

# check for Itcl headers. 

AC_DEFUN(CY_AC_PATH_ITCLCONFIG, [
#
# Ok, lets find the itcl configuration
# First, look for one uninstalled.  
# the alternative search directory is invoked by --with-itclconfig
#

if test x"${no_itcl}" = x ; then
  # we reset no_itcl in case something fails here
  no_itcl=true
  AC_ARG_WITH(itclconfig, [  --with-itclconfig       Directory containing itcl configuration (itclConfig.sh)],
         with_itclconfig=${withval})
  AC_MSG_CHECKING([for Itcl configuration])
  AC_CACHE_VAL(ac_cv_c_itclconfig,[

  # First check to see if --with-itclconfig was specified.
  if test x"${with_itclconfig}" != x ; then
    if test -f "${with_itclconfig}/itclConfig.sh" ; then
      ac_cv_c_itclconfig=`(cd ${with_itclconfig}; pwd)`
    else
      AC_MSG_ERROR([${with_itclconfig} directory doesn't contain itclConfig.sh])
    fi
  fi

  # then check for a private Itcl library
  if test x"${ac_cv_c_itclconfig}" = x ; then
    for i in \
		../itcl/itcl \
		`ls -dr ../itcl[[4-9]]*/itcl 2>/dev/null` \
		../../itcl \
		`ls -dr ../../itcl[[4-9]]*/itcl 2>/dev/null` \
		../../../itcl \
		`ls -dr ../../../itcl[[4-9]]*/itcl 2>/dev/null` ; do
      if test -f "$i/itclConfig.sh" ; then
        ac_cv_c_itclconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few common install locations
  if test x"${ac_cv_c_itclconfig}" = x ; then
    for i in `ls -d ${prefix}/lib /usr/local/lib 2>/dev/null` ; do
      if test -f "$i/itclConfig.sh" ; then
        ac_cv_c_itclconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few other private locations
  if test x"${ac_cv_c_itclconfig}" = x ; then
    for i in \
		${srcdir}/../itcl/itcl \
		`ls -dr ${srcdir}/../itcl[[4-9]]*/itcl 2>/dev/null` ; do
      if test -f "$i/itclConfig.sh" ; then
        ac_cv_c_itclconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  ])
  if test x"${ac_cv_c_itclconfig}" = x ; then
    ITCLCONFIG="# no Itcl configs found"
    AC_MSG_WARN(Can't find Itcl configuration definitions)
  else
    no_itcl=
    ITCLCONFIG=${ac_cv_c_itclconfig}/itclConfig.sh
    AC_MSG_RESULT(found $ITCLCONFIG)
  fi
fi
])

# Defined as a separate macro so we don't have to cache the values
# from PATH_ITCLCONFIG (because this can also be cached).
AC_DEFUN(CY_AC_LOAD_ITCLCONFIG, [
    if test -f "$ITCLCONFIG" ; then
      . $ITCLCONFIG
    fi

    AC_SUBST(ITCL_VERSION)
dnl not actually used, don't export to save symbols
dnl    AC_SUBST(ITCL_MAJOR_VERSION)
dnl    AC_SUBST(ITCL_MINOR_VERSION)
    AC_SUBST(ITCL_DEFS)

dnl not used, don't export to save symbols
    dnl AC_SUBST(ITCL_LIB_FILE)

dnl not used outside of configure
dnl    AC_SUBST(ITCL_LIBS)
dnl not used, don't export to save symbols
dnl    AC_SUBST(ITCL_PREFIX)

dnl not used, don't export to save symbols
dnl    AC_SUBST(ITCL_EXEC_PREFIX)

    AC_SUBST(ITCL_BUILD_INCLUDES)
    AC_SUBST(ITCL_BUILD_LIB_SPEC)
    AC_SUBST(ITCL_LIB_SPEC)
])

# check for Itcl headers. 

AC_DEFUN(CY_AC_PATH_ITCLH, [
AC_MSG_CHECKING(for Itcl private headers. srcdir=${srcdir})
if test x"${ac_cv_c_itclh}" = x ; then
  for i in ${srcdir}/../itcl ${srcdir}/../../itcl ${srcdir}/../../../itcl ${srcdir}/../itcl/itcl; do
    if test -f $i/generic/itcl.h ; then
      ac_cv_c_itclh=`(cd $i/generic; pwd)`
      break
    fi
  done
fi
if test x"${ac_cv_c_itclh}" = x ; then
  ITCLHDIR="# no Itcl private headers found"
  AC_MSG_ERROR([Can't find Itcl private headers])
fi
if test x"${ac_cv_c_itclh}" != x ; then
     ITCLHDIR="-I${ac_cv_c_itclh}"
fi
# should always be here
#     ITCLLIB="../itcl/itcl/unix/libitcl.a"
AC_SUBST(ITCLHDIR)
#AC_SUBST(ITCLLIB)
])


AC_DEFUN(CY_AC_PATH_ITKCONFIG, [
#
# Ok, lets find the itk configuration
# First, look for one uninstalled.  
# the alternative search directory is invoked by --with-itkconfig
#

if test x"${no_itk}" = x ; then
  # we reset no_itk in case something fails here
  no_itk=true
  AC_ARG_WITH(itkconfig, [  --with-itkconfig        Directory containing itk configuration (itkConfig.sh)],
         with_itkconfig=${withval})
  AC_MSG_CHECKING([for Itk configuration])
  AC_CACHE_VAL(ac_cv_c_itkconfig,[

  # First check to see if --with-itkconfig was specified.
  if test x"${with_itkconfig}" != x ; then
    if test -f "${with_itkconfig}/itkConfig.sh" ; then
      ac_cv_c_itkconfig=`(cd ${with_itkconfig}; pwd)`
    else
      AC_MSG_ERROR([${with_itkconfig} directory doesn't contain itkConfig.sh])
    fi
  fi

  # then check for a private Itk library
  if test x"${ac_cv_c_itkconfig}" = x ; then
    for i in \
		../itcl/itk \
		`ls -dr ../itcl[[4-9]]*/itk 2>/dev/null` \
		../../itk \
		`ls -dr ../../itcl[[4-9]]*/itk 2>/dev/null` \
		../../../itk \
		`ls -dr ../../../itcl[[4-9]]*/itk 2>/dev/null` ; do
      if test -f "$i/itkConfig.sh" ; then
        ac_cv_c_itkconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few common install locations
  if test x"${ac_cv_c_itkconfig}" = x ; then
    for i in `ls -d ${prefix}/lib /usr/local/lib 2>/dev/null` ; do
      if test -f "$i/itkConfig.sh" ; then
        ac_cv_c_itkconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few other private locations
  if test x"${ac_cv_c_itkconfig}" = x ; then
    for i in \
		${srcdir}/../itcl/itk \
		`ls -dr ${srcdir}/../itcl[[4-9]]*/itk 2>/dev/null` ; do
      if test -f "$i/itkConfig.sh" ; then
        ac_cv_c_itkconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  ])
  if test x"${ac_cv_c_itkconfig}" = x ; then
    ITKCONFIG="# no Itk configs found"
    AC_MSG_WARN(Can't find Itk configuration definitions)
  else
    no_itk=
    ITKCONFIG=${ac_cv_c_itkconfig}/itkConfig.sh
    AC_MSG_RESULT(found $ITKCONFIG)
  fi
fi

])

# Defined as a separate macro so we don't have to cache the values
# from PATH_ITKCONFIG (because this can also be cached).
AC_DEFUN(CY_AC_LOAD_ITKCONFIG, [
    if test -f "$ITKCONFIG" ; then
      . $ITKCONFIG
    fi

    AC_SUBST(ITK_VERSION)
dnl not actually used, don't export to save symbols
dnl    AC_SUBST(ITK_MAJOR_VERSION)
dnl    AC_SUBST(ITK_MINOR_VERSION)
    AC_SUBST(ITK_DEFS)

dnl not used, don't export to save symbols
    dnl AC_SUBST(ITK_LIB_FILE)

dnl not used outside of configure
dnl    AC_SUBST(ITK_LIBS)
dnl not used, don't export to save symbols
dnl    AC_SUBST(ITK_PREFIX)

dnl not used, don't export to save symbols
dnl    AC_SUBST(ITK_EXEC_PREFIX)

    AC_SUBST(ITK_BUILD_INCLUDES)
    AC_SUBST(ITK_BUILD_LIB_SPEC)
    AC_SUBST(ITK_LIB_SPEC)
])

AC_DEFUN(CY_AC_PATH_ITKH, [
AC_MSG_CHECKING(for Itk private headers. srcdir=${srcdir})
if test x"${ac_cv_c_itkh}" = x ; then
  for i in ${srcdir}/../itcl ${srcdir}/../../itcl ${srcdir}/../../../itcl ${srcdir}/../itcl/itk; do
    if test -f $i/generic/itk.h ; then
      ac_cv_c_itkh=`(cd $i/generic; pwd)`
      break
    fi
  done
fi
if test x"${ac_cv_c_itkh}" = x ; then
  ITKHDIR="# no Itk private headers found"
  AC_MSG_ERROR([Can't find Itk private headers])
fi
if test x"${ac_cv_c_itkh}" != x ; then
     ITKHDIR="-I${ac_cv_c_itkh}"
fi
# should always be here
#     ITKLIB="../itcl/itk/unix/libitk.a"
AC_SUBST(ITKHDIR)
#AC_SUBST(ITKLIB)
])

# check for Tix headers. 

AC_DEFUN(CY_AC_PATH_TIXH, [
AC_MSG_CHECKING(for Tix private headers. srcdir=${srcdir})
if test x"${ac_cv_c_tixh}" = x ; then
  for i in ${srcdir}/../tix ${srcdir}/../../tix ${srcdir}/../../../tix ; do
    if test -f $i/generic/tix.h ; then
      ac_cv_c_tixh=`(cd $i/generic; pwd)`
      break
    fi
  done
fi
if test x"${ac_cv_c_tixh}" = x ; then
  TIXHDIR="# no Tix private headers found"
  AC_MSG_ERROR([Can't find Tix private headers])
fi
if test x"${ac_cv_c_tixh}" != x ; then
     TIXHDIR="-I${ac_cv_c_tixh}"
fi
AC_SUBST(TIXHDIR)
])

AC_DEFUN(CY_AC_PATH_TIXCONFIG, [
#
# Ok, lets find the tix configuration
# First, look for one uninstalled.  
# the alternative search directory is invoked by --with-itkconfig
#

if test x"${no_tix}" = x ; then
  # we reset no_tix in case something fails here
  no_tix=true
  AC_ARG_WITH(tixconfig, [  --with-tixconfig        Directory containing tix configuration (tixConfig.sh)],
         with_tixconfig=${withval})
  AC_MSG_CHECKING([for Tix configuration])
  AC_CACHE_VAL(ac_cv_c_tixconfig,[

  # First check to see if --with-tixconfig was specified.
  if test x"${with_tixconfig}" != x ; then
    if test -f "${with_tixconfig}/tixConfig.sh" ; then
      ac_cv_c_tixconfig=`(cd ${with_tixconfig}; pwd)`
    else
      AC_MSG_ERROR([${with_tixconfig} directory doesn't contain tixConfig.sh])
    fi
  fi

  # then check for a private Tix library
  if test x"${ac_cv_c_tixconfig}" = x ; then
    for i in \
		../tix \
		`ls -dr ../tix 2>/dev/null` \
		../../tix \
		`ls -dr ../../tix 2>/dev/null` \
		../../../tix \
		`ls -dr ../../../tix 2>/dev/null` ; do
      echo "**** Looking at $i - with ${configdir}"
      if test -f "$i/tixConfig.sh" ; then
        ac_cv_c_tixconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few common install locations
  if test x"${ac_cv_c_tixconfig}" = x ; then
    for i in `ls -d ${prefix}/lib /usr/local/lib 2>/dev/null` ; do
      echo "**** Looking at $i"
      if test -f "$i/tixConfig.sh" ; then
        ac_cv_c_tixconfig=`(cd $i; pwd)`
	break
      fi
    done
  fi
  # check in a few other private locations
  echo "**** Other private locations"
  if test x"${ac_cv_c_tixconfig}" = x ; then
    for i in \
		${srcdir}/../tix \
		`ls -dr ${srcdir}/../tix 2>/dev/null` ; do
      echo "**** Looking at $i - with ${configdir}"
      if test -f "$i/${configdir}/tixConfig.sh" ; then
        ac_cv_c_tixconfig=`(cd $i/${configdir}; pwd)`
	break
      fi
    done
  fi
  ])
  if test x"${ac_cv_c_tixconfig}" = x ; then
    TIXCONFIG="# no Tix configs found"
    AC_MSG_WARN(Can't find Tix configuration definitions)
  else
    no_tix=
    TIXCONFIG=${ac_cv_c_tixconfig}/tixConfig.sh
    AC_MSG_RESULT(found $TIXCONFIG)
  fi
fi

])

# Defined as a separate macro so we don't have to cache the values
# from PATH_TIXCONFIG (because this can also be cached).
AC_DEFUN(CY_AC_LOAD_TIXCONFIG, [
    if test -f "$TIXCONFIG" ; then
      . $TIXCONFIG
    fi

    AC_SUBST(TIX_VERSION)
dnl not actually used, don't export to save symbols
dnl    AC_SUBST(TIX_MAJOR_VERSION)
dnl    AC_SUBST(TIX_MINOR_VERSION)
dnl    AC_SUBST(TIX_DEFS)

dnl not used, don't export to save symbols
dnl    dnl AC_SUBST(TIX_LIB_FILE)

dnl not used outside of configure
dnl    AC_SUBST(TIX_LIBS)
dnl not used, don't export to save symbols
dnl    AC_SUBST(TIX_PREFIX)

dnl not used, don't export to save symbols
dnl    AC_SUBST(TIX_EXEC_PREFIX)

dnl    AC_SUBST(TIX_BUILD_INCLUDES)
    AC_SUBST(TIX_BUILD_LIB_SPEC)
dnl    AC_SUBST(TIX_LIB_SPEC)
])

dnl sinclude(../gettext.m4) already included by bfd/acinclude.m4
dnl The lines below arrange for aclocal not to bring gettext.m4's
dnl CY_GNU_GETTEXT into aclocal.m4.
ifelse(yes,no,[
AC_DEFUN([CY_GNU_GETTEXT],)
])


# Copyright 1996, 1997, 1999, 2000, 2001 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

# serial 1

# @defmac AC_PROG_CC_STDC
# @maindex PROG_CC_STDC
# @ovindex CC
# If the C compiler in not in ANSI C mode by default, try to add an option
# to output variable @code{CC} to make it so.  This macro tries various
# options that select ANSI C on some system or another.  It considers the
# compiler to be in ANSI C mode if it handles function prototypes correctly.
#
# If you use this macro, you should check after calling it whether the C
# compiler has been set to accept ANSI C; if not, the shell variable
# @code{am_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
# code in ANSI C, you can make an un-ANSIfied copy of it by using the
# program @code{ansi2knr}, which comes with Ghostscript.
# @end defmac

AC_DEFUN([AM_PROG_CC_STDC],
[AC_REQUIRE([AC_PROG_CC])
AC_BEFORE([$0], [AC_C_INLINE])
AC_BEFORE([$0], [AC_C_CONST])
dnl Force this before AC_PROG_CPP.  Some cpp's, eg on HPUX, require
dnl a magic option to avoid problems with ANSI preprocessor commands
dnl like #elif.
dnl FIXME: can't do this because then AC_AIX won't work due to a
dnl circular dependency.
dnl AC_BEFORE([$0], [AC_PROG_CPP])
AC_MSG_CHECKING([for ${CC-cc} option to accept ANSI C])
AC_CACHE_VAL(am_cv_prog_cc_stdc,
[am_cv_prog_cc_stdc=no
ac_save_CC="$CC"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX 10.20 and later	-Ae
# HP-UX older versions	-Aa -D_HPUX_SOURCE
# SVR4			-Xc -D__EXTENSIONS__
for ac_arg in "" -qlanglvl=ansi -std1 -Ae "-Aa -D_HPUX_SOURCE" "-Xc -D__EXTENSIONS__"
do
  CC="$ac_save_CC $ac_arg"
  AC_TRY_COMPILE(
[#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
/* Most of the following tests are stolen from RCS 5.7's src/conf.sh.  */
struct buf { int x; };
FILE * (*rcsopen) (struct buf *, struct stat *, int);
static char *e (p, i)
     char **p;
     int i;
{
  return p[i];
}
static char *f (char * (*g) (char **, int), char **p, ...)
{
  char *s;
  va_list v;
  va_start (v,p);
  s = g (p, va_arg (v,int));
  va_end (v);
  return s;
}
int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};
int pairnames (int, char **, FILE *(*)(struct buf *, struct stat *, int), int, int);
int argc;
char **argv;
], [
return f (e, argv, 0) != argv[0]  ||  f (e, argv, 1) != argv[1];
],
[am_cv_prog_cc_stdc="$ac_arg"; break])
done
CC="$ac_save_CC"
])
if test -z "$am_cv_prog_cc_stdc"; then
  AC_MSG_RESULT([none needed])
else
  AC_MSG_RESULT([$am_cv_prog_cc_stdc])
fi
case "x$am_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $am_cv_prog_cc_stdc" ;;
esac
])

# Add --enable-maintainer-mode option to configure.
# From Jim Meyering

# serial 1

AC_DEFUN(AM_MAINTAINER_MODE,
[AC_MSG_CHECKING([whether to enable maintainer-specific portions of Makefiles])
  dnl maintainer-mode is disabled by default
  AC_ARG_ENABLE(maintainer-mode,
[  --enable-maintainer-mode enable make rules and dependencies not useful
                          (and sometimes confusing) to the casual installer],
      USE_MAINTAINER_MODE=$enableval,
      USE_MAINTAINER_MODE=no)
  AC_MSG_RESULT($USE_MAINTAINER_MODE)
  AM_CONDITIONAL(MAINTAINER_MODE, test $USE_MAINTAINER_MODE = yes)
  MAINT=$MAINTAINER_MODE_TRUE
  AC_SUBST(MAINT)dnl
]
)

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

