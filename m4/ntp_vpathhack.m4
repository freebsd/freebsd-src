dnl ######################################################################
dnl NTP_VPATH_HACK
dnl
dnl Are we using FreeBSD's make?
dnl if we are building outside the srcdir and either
dnl   force_ntp_vpath_hack is set
dnl     or
dnl   we're on freebsd and not using GNU make
dnl then we want VPATH_HACK to be true in automake tests
dnl
AC_DEFUN([NTP_VPATH_HACK], [
AC_MSG_CHECKING([to see if we need a VPATH hack])
ntp_vpath_hack="no"
case "$srcdir::$build_os::${force_ntp_vpath_hack+set}" in
 .::*::*)
    ;;
 *::*::set)
    ntp_vpath_hack="yes"
    ;;
 *::freebsd*::)
    case "`${MAKE-make} -v -f /dev/null 2>/dev/null | grep 'GNU Make'`" in
     '')
	ntp_vpath_hack="yes"
    esac
esac
AC_MSG_RESULT([$ntp_vpath_hack])
AM_CONDITIONAL([VPATH_HACK], [test x$ntp_vpath_hack = xyes])
])
dnl ======================================================================
