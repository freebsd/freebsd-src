dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT
AC_CONFIG_SRCDIR([gold.cc])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE(gold, 0.1)

AM_CONFIG_HEADER(config.h:config.in)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_YACC
AC_PROG_RANLIB
AC_PROG_INSTALL
ZW_GNU_GETTEXT_SISTER_DIR
AM_PO_SUBDIRS

AC_C_BIGENDIAN

AC_EXEEXT

AM_BINUTILS_WARNINGS

WARN_CXXFLAGS=`echo ${WARN_CFLAGS} | sed -e 's/-Wstrict-prototypes//' -e 's/-Wmissing-prototypes//'`
AC_SUBST(WARN_CXXFLAGS)

dnl Force support for large files by default.  This may need to be
dnl host dependent.  If build == host, we can check getconf LFS_CFLAGS.
LFS_CXXFLAGS="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
AC_SUBST(LFS_CXXFLAGS)

AC_LANG_PUSH(C++)

AC_CHECK_HEADERS(tr1/unordered_set tr1/unordered_map)
AC_CHECK_HEADERS(ext/hash_map ext/hash_set)

dnl Test whether the compiler can specify a member templates to call.
AC_COMPILE_IFELSE([
class c { public: template<int i> void fn(); };
template<int i> void foo(c cv) { cv.fn<i>(); }
template void foo<1>(c cv);],
[AC_DEFINE(HAVE_MEMBER_TEMPLATE_SPECIFICATIONS, [],
  [Whether the C++ compiler can call a template member with no arguments])])

AC_LANG_POP(C++)

AM_MAINTAINER_MODE

AC_OUTPUT(Makefile testsuite/Makefile po/Makefile.in:po/Make-in)
