dnl $Id: krb-prog-ln-s.m4,v 1.1 1997/12/14 15:59:01 joda Exp $
dnl
dnl
dnl Better test for ln -s, ln or cp
dnl

AC_DEFUN(AC_KRB_PROG_LN_S,
[AC_MSG_CHECKING(for ln -s or something else)
AC_CACHE_VAL(ac_cv_prog_LN_S,
[rm -f conftestdata
if ln -s X conftestdata 2>/dev/null
then
  rm -f conftestdata
  ac_cv_prog_LN_S="ln -s"
else
  touch conftestdata1
  if ln conftestdata1 conftestdata2; then
    rm -f conftestdata*
    ac_cv_prog_LN_S=ln
  else
    ac_cv_prog_LN_S=cp
  fi
fi])dnl
LN_S="$ac_cv_prog_LN_S"
AC_MSG_RESULT($ac_cv_prog_LN_S)
AC_SUBST(LN_S)dnl
])

