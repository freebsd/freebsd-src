dnl ######################################################################
dnl Debugging: "yes" means general, "mem" means general and memory debugging,
dnl and "no" means none.
AC_DEFUN(AMU_OPT_DEBUG,
[AC_MSG_CHECKING(for debugging options)
AC_ARG_ENABLE(debug,
AC_HELP_STRING([--enable-debug=ARG],[enable debugging (yes/mem/no)]),
[
if test "$enableval" = yes; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(DEBUG)
  ac_cv_opt_debug=yes
elif test "$enableval" = mem; then
  AC_MSG_RESULT(mem)
  AC_DEFINE(DEBUG)
  AC_DEFINE(DEBUG_MEM)
  AC_CHECK_LIB(mapmalloc, malloc_verify)
  AC_CHECK_LIB(malloc, mallinfo)
  ac_cv_opt_debug=mem
else
  AC_MSG_RESULT(no)
  ac_cv_opt_debug=no
fi
],
[
  # default is no debugging
  AC_MSG_RESULT(no)
])
])
dnl ======================================================================
