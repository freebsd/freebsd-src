# AC_LANG_STDCALL_FUNC_LINK_TRY(FUNCTION, SIGNATURE)
# -------------------------------
# Produce a source which links correctly iff the FUNCTION exists.
AC_DEFUN([AC_LANG_STDCALL_FUNC_LINK_TRY],
[_AC_LANG_DISPATCH([$0], _AC_LANG, $@)])

# AC_CHECK_STDCALL_FUNC(FUNCTION, SIGNATURE, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# -----------------------------------------------------------------
AC_DEFUN([AC_CHECK_STDCALL_FUNC],
[AS_VAR_PUSHDEF([ac_var], [ac_cv_func_$1])dnl
AC_CACHE_CHECK([for $1], ac_var,
[AC_LINK_IFELSE([AC_LANG_STDCALL_FUNC_LINK_TRY([$1],[$2])],
                [AS_VAR_SET(ac_var, yes)],
                [AS_VAR_SET(ac_var, no)])])
AS_IF([test AS_VAR_GET(ac_var) = yes], [$3], [$4])dnl
AS_VAR_POPDEF([ac_var])dnl
])# AC_CHECK_FUNC

# AC_LANG_STDCALL_FUNC_LINK_TRY(C)(FUNCTION, SIGNATURE)
# ----------------------------------
# Don't include <ctype.h> because on OSF/1 3.0 it includes
# <sys/types.h> which includes <sys/select.h> which contains a
# prototype for select.  Similarly for bzero.
m4_define([AC_LANG_STDCALL_FUNC_LINK_TRY(C)],
[AC_LANG_PROGRAM(
[/* System header to define __stub macros and hopefully few prototypes,
    which can conflict with char __stdcall $1 ( $2 ) below.  */
#include <assert.h>
/* Override any gcc2 internal prototype to avoid an error.  */
#ifdef __cplusplus
extern "C"
#endif
/* We use char because int might match the return type of a gcc2
   builtin and then its argument prototype would still apply.  */
char __stdcall $1 ( $2 );
char (*f) ( $2 );
],
[/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias.  */
#if defined (__stub_$1) || defined (__stub___$1)
choke me
#else
f = $1;
#endif
])])

# AC_LANG_STDCALL_FUNC_LINK_TRY(C++)(FUNCTION)
# ------------------------------------
m4_copy([AC_LANG_STDCALL_FUNC_LINK_TRY(C)], [AC_LANG_STDCALL_FUNC_LINK_TRY(C++)])

