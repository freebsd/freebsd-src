dnl See whether we need to use fopen-bin.h rather than fopen-same.h.
AC_DEFUN(BFD_BINARY_FOPEN,
[AC_REQUIRE([AC_CANONICAL_SYSTEM])
case "${host}" in
changequote(,)dnl
i[345]86-*-msdos* | i[345]86-*-go32* | i[345]86-*-mingw32* | *-*-cygwin32* | *-*-windows)
changequote([,])dnl
  AC_DEFINE(USE_BINARY_FOPEN) ;;
esac])dnl

dnl Get a default for CC_FOR_BUILD to put into Makefile.
AC_DEFUN(BFD_CC_FOR_BUILD,
[# Put a plausible default for CC_FOR_BUILD in Makefile.
if test -z "$CC_FOR_BUILD"; then
  if test "x$cross_compiling" = "xno"; then
    CC_FOR_BUILD='$(CC)'
  else
    CC_FOR_BUILD=gcc
  fi
fi
AC_SUBST(CC_FOR_BUILD)
# Also set EXEEXT_FOR_BUILD.
if test "x$cross_compiling" = "xno"; then
  EXEEXT_FOR_BUILD='$(EXEEXT)'
else
  AC_CACHE_CHECK([for build system executable suffix], bfd_cv_build_exeext,
    [cat > ac_c_test.c << 'EOF'
int main() {
/* Nothing needed here */
}
EOF
    ${CC_FOR_BUILD} -o ac_c_test am_c_test.c 1>&5 2>&5
    bfd_cv_build_exeext=`echo ac_c_test.* | grep -v ac_c_test.c | sed -e s/ac_c_test//`
    rm -f ac_c_test*
    test x"${bfd_cv_build_exeext}" = x && bfd_cv_build_exeext=no])
  EXEEXT_FOR_BUILD=""
  test x"${bfd_cv_build_exeext}" != xno && EXEEXT_FOR_BUILD=${bfd_cv_build_exeext}
fi
AC_SUBST(EXEEXT_FOR_BUILD)])dnl

dnl See whether we need a declaration for a function.
AC_DEFUN(BFD_NEED_DECLARATION,
[AC_MSG_CHECKING([whether $1 must be declared])
AC_CACHE_VAL(bfd_cv_decl_needed_$1,
[AC_TRY_COMPILE([
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif],
[char *(*pfn) = (char *(*)) $1],
bfd_cv_decl_needed_$1=no, bfd_cv_decl_needed_$1=yes)])
AC_MSG_RESULT($bfd_cv_decl_needed_$1)
if test $bfd_cv_decl_needed_$1 = yes; then
  bfd_tr_decl=NEED_DECLARATION_`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
  AC_DEFINE_UNQUOTED($bfd_tr_decl)
fi
])dnl
