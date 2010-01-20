dnl $Id: krb-struct-winsize.m4 13338 2004-02-12 14:21:14Z lha $
dnl
dnl
dnl Search for struct winsize
dnl

AC_DEFUN([AC_KRB_STRUCT_WINSIZE], [
AC_MSG_CHECKING(for struct winsize)
AC_CACHE_VAL(ac_cv_struct_winsize, [
ac_cv_struct_winsize=no
for i in sys/termios.h sys/ioctl.h; do
AC_EGREP_HEADER(
struct[[ 	]]*winsize,dnl
$i, ac_cv_struct_winsize=yes; break)dnl
done
])
if test "$ac_cv_struct_winsize" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_WINSIZE, 1, [define if struct winsize is declared in sys/termios.h])
fi
AC_MSG_RESULT($ac_cv_struct_winsize)
AC_EGREP_HEADER(ws_xpixel, termios.h, 
	AC_DEFINE(HAVE_WS_XPIXEL, 1, [define if struct winsize has ws_xpixel]))
AC_EGREP_HEADER(ws_ypixel, termios.h, 
	AC_DEFINE(HAVE_WS_YPIXEL, 1, [define if struct winsize has ws_ypixel]))
])
