#
# $Id$
#
# See ./INSTALL for more info
#

#
# Release numbering: even numbered dot releases are official ones, and
# odd numbers are development ones.  The svn version of this file will
# only (ONLY!) ever (EVER!) contain odd numbers, so I'll always know if
# a particular user has the dist or svn release.
#

AC_PREREQ(2.2)
AC_INIT([libxo], [0.3.2], [phil@juniper.net])
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability])

# Support silent build rules.  Requires at least automake-1.11.
# Disable with "configure --disable-silent-rules" or "make V=1"
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_AR
AC_PROG_INSTALL
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_LN_S

# Must be after AC_PROG_AR
LT_INIT([dlopen shared])

AC_PATH_PROG(BASENAME, basename, /usr/bin/basename)
AC_PATH_PROG(BISON, bison, /usr/bin/bison)
AC_PATH_PROG(CAT, cat, /bin/cat)
AC_PATH_PROG(CHMOD, chmod, /bin/chmod)
AC_PATH_PROG(CP, cp, /bin/cp)
AC_PATH_PROG(DIFF, diff, /usr/bin/diff)
AC_PATH_PROG(MKDIR, mkdir, /bin/mkdir)
AC_PATH_PROG(MV, mv, /bin/mv)
AC_PATH_PROG(RM, rm, /bin/rm)
AC_PATH_PROG(SED, sed, /bin/sed)

AC_STDC_HEADERS

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero memmove strchr strcspn strerror strspn])
AC_CHECK_FUNCS([sranddev srand strlcpy])
AC_CHECK_FUNCS([fdopen getrusage])
AC_CHECK_FUNCS([gettimeofday ctime])
AC_CHECK_FUNCS([getpass])
AC_CHECK_FUNCS([sysctlbyname])
AC_CHECK_FUNCS([flock])
AC_CHECK_FUNCS([asprintf])
AC_CHECK_FUNCS([__flbf])

AC_CHECK_HEADERS([dlfcn.h])
AC_CHECK_HEADERS([stdio_ext.h])
AC_CHECK_HEADERS([tzfile.h])
AC_CHECK_HEADERS([stdtime/tzfile.h])
AC_CHECK_FUNCS([dlfunc])

AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([ctype.h errno.h stdio.h stdlib.h])
AC_CHECK_HEADERS([string.h sys/param.h unistd.h ])
AC_CHECK_HEADERS([sys/sysctl.h])

AC_CHECK_LIB([crypto], [MD5_Init])
AM_CONDITIONAL([HAVE_LIBCRYPTO], [test "$HAVE_LIBCRYPTO" != "no"])

dnl
dnl Some packages need to be checked against version numbers so we
dnl define a function here for later use
dnl
AC_DEFUN([VERSION_TO_NUMBER],
[`$1 | sed -e 's/lib.* //' | awk 'BEGIN { FS = "."; } { printf "%d", ([$]1 * 1000 + [$]2) * 1000 + [$]3;}'`])

LIBSLAX_CONFIG_PREFIX=""
LIBSLAX_SRC=""

AC_ARG_WITH(libslax-prefix,
        [  --with-libslax-prefix=[PFX]           Specify location of libslax config],
        LIBSLAX_CONFIG_PREFIX=$withval
)

AC_MSG_CHECKING(for libslax) 
if test "x$LIBSLAX_CONFIG_PREFIX" != "x"
then
        SLAX_CONFIG=${LIBSLAX_CONFIG_PREFIX}/bin/slax-config
else
        SLAX_CONFIG=slax-config
fi

dnl
dnl make sure slax-config is executable,
dnl test version and init our variables
dnl

if ${SLAX_CONFIG} --libs > /dev/null 2>&1
then
	LIBSLAX_VERSION=`$SLAX_CONFIG --version`
	SLAX_BINDIR="`$SLAX_CONFIG --bindir | head -1`"
	SLAX_OXTRADOCDIR="`$SLAX_CONFIG --oxtradoc | head -1`"
	AC_MSG_RESULT($LIBSLAX_VERSION found)
else
	LIBSLAX_VERSION=
	SLAX_BINDIR=
	SLAX_OXTRADOCDIR=
	AC_MSG_RESULT([no])
fi

AC_SUBST(SLAX_BINDIR)
AC_SUBST(SLAX_OXTRADOCDIR)

AC_MSG_CHECKING([whether to build with warnings])
AC_ARG_ENABLE([warnings],
    [  --enable-warnings    Turn on compiler warnings],
    [LIBXO_WARNINGS=$enableval],
    [LIBXO_WARNINGS=no])
AC_MSG_RESULT([$LIBXO_WARNINGS])
AM_CONDITIONAL([LIBXO_WARNINGS_HIGH], [test "$LIBXO_WARNINGS" != "no"])

AC_MSG_CHECKING([whether to build with debugging])
AC_ARG_ENABLE([debug],
    [  --enable-debug    Turn on debugging],
    [LIBXO_DEBUG=yes; AC_DEFINE([LIBXO_DEBUG], [1], [Enable debugging])],
    [LIBXO_DEBUG=no])
AC_MSG_RESULT([$LIBXO_DEBUG])
AM_CONDITIONAL([LIBXO_DEBUG], [test "$LIBXO_DEBUG" != "no"])

AC_MSG_CHECKING([whether to build with text-only rendering])
AC_ARG_ENABLE([text-only],
    [  --enable-text-only    Turn on text-only rendering],
    [LIBXO_TEXT_ONLY=yes; AC_DEFINE([LIBXO_TEXT_ONLY], [1], [Enable text-only rendering])],
    [LIBXO_TEXT_ONLY=no])
AC_MSG_RESULT([$LIBXO_TEXT_ONLY])
AM_CONDITIONAL([LIBXO_TEXT_ONLY], [test "$LIBXO_TEXT_ONLY" != "no"])

AC_CHECK_LIB([m], [lrint])
AM_CONDITIONAL([HAVE_LIBM], [test "$HAVE_LIBM" != "no"])

AC_MSG_CHECKING([compiler for gcc])
HAVE_GCC=no
if test "${CC}" != ""; then
    HAVE_GCC=`${CC} --version 2>&1 | grep GCC`
    if test "${HAVE_GCC}" != ""; then
        HAVE_GCC=yes
    else
        HAVE_GCC=no
    fi
fi
AC_MSG_RESULT([$HAVE_GCC])
AM_CONDITIONAL([HAVE_GCC], [test "$HAVE_GCC" = "yes"])

AC_MSG_CHECKING([whether to build with printflike])
AC_ARG_ENABLE([printflike],
    [  --enable-printflike    Enable use of GCC __printflike attribute],
    [HAVE_PRINTFLIKE=yes;
           AC_DEFINE([HAVE_PRINTFLIKE], [1], [Support printflike])],
    [HAVE_PRINTFLIKE=no])
AC_MSG_RESULT([$HAVE_PRINTFLIKE])
AM_CONDITIONAL([HAVE_PRINTFLIKE], [test "$HAVE_PRINTFLIKE" != ""])

AC_MSG_CHECKING([whether to build with LIBXO_OPTIONS])
AC_ARG_ENABLE([libxo-options],
    [  --disable-libxo-options  Turn off support for LIBXO_OPTIONS],
    [LIBXO_OPTS=$enableval],
    [LIBXO_OPTS=yes])
AC_MSG_RESULT([$LIBXO_OPTS])
AM_CONDITIONAL([NO_LIBXO_OPTIONS], [test "$LIBXO_OPTS" != "yes"])

case $host_os in
     darwin*)
        LIBTOOL=glibtool
	;;
     Linux*|linux*)
        CFLAGS="-D_GNU_SOURCE $CFLAGS"
	LDFLAGS=-ldl
	;;
     cygwin*|CYGWIN*)
	LDFLAGS=-no-undefined
	;;
esac

case $prefix in
     NONE)
	prefix=/usr/local
	;;
esac

XO_LIBS=-lxo
XO_SRCDIR=${srcdir}
XO_LIBDIR=${libdir}
XO_BINDIR=${bindir}
XO_INCLUDEDIR=${includedir}

AC_SUBST(XO_SRCDIR)
AC_SUBST(XO_LIBDIR)
AC_SUBST(XO_BINDIR)
AC_SUBST(XO_INCLUDEDIR)

AC_ARG_WITH(share-dir,
        [  --with-share-dir=[DIR]           Specify location of shared files],
        [XO_SHAREDIR=$withval],
        [XO_SHAREDIR=$datarootdir/libxo]
)
XO_SHAREDIR=`echo $XO_SHAREDIR | sed "s;\\${prefix};$prefix;"`
AC_SUBST(XO_SHAREDIR)

dnl for the spec file
RELDATE=`date +'%Y-%m-%d%n'`
AC_SUBST(RELDATE)

AC_MSG_RESULT(Using configure dir $ac_abs_confdir)

if test -d $ac_abs_confdir/.git ; then
  extra=`git branch | awk '/\*/ { print $2 }'`
  if test "$extra" != "" -a "$extra" != "master"
  then
      LIBXO_VERSION_EXTRA="-git-$extra"
  fi
fi

LIBXO_VERSION=$PACKAGE_VERSION
LIBXO_VERSION_NUMBER=VERSION_TO_NUMBER(echo $PACKAGE_VERSION)
AC_SUBST(LIBXO_VERSION)
AC_SUBST(LIBXO_VERSION_NUMBER)
AC_SUBST(LIBXO_VERSION_EXTRA)

AC_CONFIG_HEADERS([libxo/xoconfig.h])
AC_CONFIG_FILES([
  Makefile
  libxo-config
  xohtml/xohtml.sh
  libxo/Makefile
  libxo/xoversion.h
  xo/Makefile
  xolint/Makefile
  xohtml/Makefile
  packaging/libxo.pc
  doc/Makefile
  tests/Makefile
  tests/core/Makefile
  tests/xo/Makefile
  packaging/libxo.spec
  packaging/libxo.rb.base
])
AC_OUTPUT

AC_MSG_NOTICE([summary of build options:

  libxo version:    ${VERSION} ${LIBXO_VERSION_EXTRA}
  host type:        ${host} / ${host_os}
  install prefix:   ${prefix}
  srcdir:           ${XO_SRCDIR}
  libdir:           ${XO_LIBDIR}
  bindir:           ${XO_BINDIR}
  includedir:       ${XO_INCLUDEDIR}
  share dir:        ${XO_SHAREDIR}
  oxtradoc dir:     ${SLAX_OXTRADOCDIR}

  compiler:         ${CC} (${HAVE_GCC:-no})
  compiler flags:   ${CFLAGS}
  library types:    Shared=${enable_shared}, Static=${enable_static}

  warnings:         ${LIBXO_WARNINGS:-no}
  debug:            ${LIBXO_DEBUG:-no}
  printf-like:      ${HAVE_PRINTFLIKE:-no}
  libxo-options:    ${LIBXO_OPTS:-no}
  text-only:        ${LIBXO_TEXT_ONLY:-no}
])
