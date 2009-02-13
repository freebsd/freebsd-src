AC_DEFUN(AM_SANITY_CHECK_CC,
[dnl Derived from macros from Bruno Haible and from Cygnus.
AC_MSG_CHECKING([whether the compiler ($CC $CFLAGS $LDFLAGS) actually works])
AC_LANG_SAVE
  AC_LANG_C
  AC_TRY_RUN([main() { exit(0); }],
             am_cv_prog_cc_works=yes, am_cv_prog_cc_works=no,
             dnl When crosscompiling, just try linking.
             AC_TRY_LINK([], [], am_cv_prog_cc_works=yes,
                         am_cv_prog_cc_works=no))
AC_LANG_RESTORE
case "$am_cv_prog_cc_works" in
  *no) AC_MSG_ERROR([Installation or configuration problem: C compiler cannot create executables.]) ;;
  *yes) ;;
esac
AC_MSG_RESULT(yes)
])dnl
