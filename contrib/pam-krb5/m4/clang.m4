dnl Determine whether the current compiler is Clang.
dnl
dnl If the current compiler is Clang, set the shell variable CLANG to yes.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2015 Russ Allbery <eagle@eyrie.org>
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Source used by RRA_PROG_CC_CLANG.
AC_DEFUN([_RRA_PROG_CC_CLANG_SOURCE], [[
#if ! __clang__
#error
#endif
]])

AC_DEFUN([RRA_PROG_CC_CLANG],
[AC_CACHE_CHECK([if the compiler is Clang], [rra_cv_prog_cc_clang],
    [AC_COMPILE_IFELSE([AC_LANG_SOURCE([_RRA_PROG_CC_CLANG_SOURCE])],
        [rra_cv_prog_cc_clang=yes],
        [rra_cv_prog_cc_clang=no])])
 AS_IF([test x"$rra_cv_prog_cc_clang" = xyes], [CLANG=yes])])
