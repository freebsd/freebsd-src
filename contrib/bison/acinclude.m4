dnl BISON_DEFINE_FILE(VARNAME, FILE)
dnl Defines (with AC_DEFINE) VARNAME to the expansion of the FILE
dnl variable, expanding ${prefix} and such.
dnl Example: BISON_DEFINE_FILE(DATADIR, datadir)
dnl By Alexandre Oliva <oliva@dcc.unicamp.br>
AC_DEFUN(BISON_DEFINE_FILE, [
        ac_expanded=`(
            test "x$prefix" = xNONE && prefix="$ac_default_prefix"
            test "x$exec_prefix" = xNONE && exec_prefix="${prefix}"
            eval echo \""[$]$2"\"
        )`
        AC_DEFINE_UNQUOTED($1, "$ac_expanded")
])

dnl See whether we need a declaration for a function.
dnl BISON_NEED_DECLARATION(FUNCTION [, EXTRA-HEADER-FILES])
AC_DEFUN(BISON_NEED_DECLARATION,
[AC_MSG_CHECKING([whether $1 must be declared])
AC_CACHE_VAL(bison_cv_decl_needed_$1,
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
#endif
#ifndef HAVE_RINDEX
#define rindex strrchr
#endif
#ifndef HAVE_INDEX
#define index strchr
#endif
$2],
[char *(*pfn) = (char *(*)) $1],
eval "bison_cv_decl_needed_$1=no", eval "bison_cv_decl_needed_$1=yes")])
if eval "test \"`echo '$bison_cv_decl_needed_'$1`\" = yes"; then
  AC_MSG_RESULT(yes)
  bison_tr_decl=NEED_DECLARATION_`echo $1 | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
  AC_DEFINE_UNQUOTED($bison_tr_decl)
else
  AC_MSG_RESULT(no)
fi
])dnl

dnl Check multiple functions to see whether each needs a declaration.
dnl BISON_NEED_DECLARATIONS(FUNCTION... [, EXTRA-HEADER-FILES])
AC_DEFUN(BISON_NEED_DECLARATIONS,
[for ac_func in $1
do
BISON_NEED_DECLARATION($ac_func, $2)
done
])
