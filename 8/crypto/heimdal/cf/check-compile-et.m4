dnl $Id: check-compile-et.m4 19252 2006-12-06 13:32:55Z lha $
dnl
dnl CHECK_COMPILE_ET
AC_DEFUN([CHECK_COMPILE_ET], [

AC_CHECK_PROG(COMPILE_ET, compile_et, [compile_et])

krb_cv_compile_et="no"
krb_cv_com_err_need_r=""
krb_cv_compile_et_cross=no
if test "${COMPILE_ET}" = "compile_et"; then

dnl We have compile_et.  Now let's see if it supports `prefix' and `index'.
AC_MSG_CHECKING(whether compile_et has the features we need)
cat > conftest_et.et <<'EOF'
error_table test conf
prefix CONFTEST
index 1
error_code CODE1, "CODE1"
index 128
error_code CODE2, "CODE2"
end
EOF
if ${COMPILE_ET} conftest_et.et >/dev/null 2>&1; then
  dnl XXX Some systems have <et/com_err.h>.
  save_CPPFLAGS="${CPPFLAGS}"
  if test -d "/usr/include/et"; then
    CPPFLAGS="-I/usr/include/et ${CPPFLAGS}"
  fi
  dnl Check that the `prefix' and `index' directives were honored.
  AC_RUN_IFELSE([
#include <com_err.h>
#include <string.h>
#include "conftest_et.h"
int main(int argc, char **argv){
#ifndef ERROR_TABLE_BASE_conf
#error compile_et does not handle error_table N M
#endif
return (CONFTEST_CODE2 - CONFTEST_CODE1) != 127;}
  ], [krb_cv_compile_et="yes"],[CPPFLAGS="${save_CPPFLAGS}"],
  [krb_cv_compile_et="yes" krb_cv_compile_et_cross=yes] )
fi
AC_MSG_RESULT(${krb_cv_compile_et})
if test "${krb_cv_compile_et}" = "yes" -a "${krb_cv_compile_et_cross}" = no; then
  AC_MSG_CHECKING([for if com_err generates a initialize_conf_error_table_r])
  AC_EGREP_CPP([initialize_conf_error_table_r.*struct et_list],
     [#include "conftest_et.h"],
     [krb_cv_com_err_need_r="ok"])
  if test X"$krb_cv_com_err_need_r" = X ; then
    AC_MSG_RESULT(no)
    krb_cv_compile_et=no
  else
    AC_MSG_RESULT(yes)
  fi
fi
rm -fr conftest*
fi

if test "${krb_cv_compile_et_cross}" = yes ; then
  krb_cv_com_err="cross"
elif test "${krb_cv_compile_et}" = "yes"; then
  dnl Since compile_et seems to work, let's check libcom_err
  krb_cv_save_LIBS="${LIBS}"
  LIBS="${LIBS} -lcom_err"
  AC_MSG_CHECKING(for com_err)
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <com_err.h>]],[[
    const char *p;
    p = error_message(0);
    initialize_error_table_r(0,0,0,0);
  ]])],[krb_cv_com_err="yes"],[krb_cv_com_err="no"; CPPFLAGS="${save_CPPFLAGS}"])
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
    localcomerr=no
elif test "${krb_cv_com_err}" = "cross"; then
    DIR_com_err="com_err"
    LIB_com_err="\$(top_builddir)/lib/com_err/libcom_err.la"
    LIB_com_err_a="\$(top_builddir)/lib/com_err/.libs/libcom_err.a"
    LIB_com_err_so="\$(top_builddir)/lib/com_err/.libs/libcom_err.so"
    AC_MSG_NOTICE(Using our own com_err with toolchain compile_et)
    localcomerr=yes
else
    COMPILE_ET="\$(top_builddir)/lib/com_err/compile_et"
    DIR_com_err="com_err"
    LIB_com_err="\$(top_builddir)/lib/com_err/libcom_err.la"
    LIB_com_err_a="\$(top_builddir)/lib/com_err/.libs/libcom_err.a"
    LIB_com_err_so="\$(top_builddir)/lib/com_err/.libs/libcom_err.so"
    AC_MSG_NOTICE(Using our own com_err)
    localcomerr=yes
fi
AM_CONDITIONAL(COM_ERR, test "$localcomerr" = yes)dnl
AC_SUBST(DIR_com_err)
AC_SUBST(LIB_com_err)
AC_SUBST(LIB_com_err_a)
AC_SUBST(LIB_com_err_so)

])
